/*******************************************************************************
*                                                                              *
*   (C) 1997-2017 by Ernst W. Mayer.                                           *
*                                                                              *
*  This program is free software; you can redistribute it and/or modify it     *
*  under the terms of the GNU General Public License as published by the       *
*  Free Software Foundation; either version 2 of the License, or (at your      *
*  option) any later version.                                                  *
*                                                                              *
*  This program is distributed in the hope that it will be useful, but WITHOUT *
*  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or       *
*  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for   *
*  more details.                                                               *
*                                                                              *
*  You should have received a copy of the GNU General Public License along     *
*  with this program; see the file GPL.txt.  If not, you may view one at       *
*  http://www.fsf.org/licenses/licenses.html, or obtain one by writing to the  *
*  Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA     *
*  02111-1307, USA.                                                            *
*                                                                              *
*******************************************************************************/

#define MI64_DEBUG	0

#include "mi64.h"
#include "align.h"
#include "qfloat.h"

#if MI64_DEBUG
	#include "Mdata.h"
	char s0[STR_MAX_LEN], s1[STR_MAX_LEN];
	char str_10k[10*1024];

//	#define YOU_WANT_IT_TO_BE_SLOW
#endif

/********* Put GPU-TF-specific versions of various mi64 functions in this separate first block: **********/

#ifdef __CUDACC__

// To compute 2^p (mod q), need a static-var-and-local-storage-free GPU analog of mi64_twopmodq together with
// a simple caller-interface function, GPU_TF_mi64:
__global__ void GPU_TF_mi64(
	const uint64*p, const uint64*pshift, uint64*gpu_thread_local, const uint32 lenP, const uint32 lenQ, const uint32 FERMAT,
	const uint32 pow2, const uint32 zshift, const uint32 start_index, const uint64 k0, uint32*kvec, const uint32 N)
{
	int i = blockDim.x * blockIdx.x + threadIdx.x;	// Hardware thread ID:
	if(i < N) {	// Allow for partially-filled thread blocks
		uint64 k = k0 + kvec[i];
		// If this k yields a factor, save the k in the 'one-beyond' slot of the kvec array.
		if(mi64_twopmodq_gpu(p, pshift, gpu_thread_local + 8*i, lenP, lenQ, FERMAT, pow2, k, start_index, zshift, i)) {
			kvec[N  ] = (uint32)(k      );	// Use 'one-beyond' elt of kvec, assume at most one factor k per batch (of ~50000), thus no need for atomic update here.
			kvec[N+1] = (uint32)(k >> 32);	// Must break 64-bit k into 2 32-bit pieces since kvec (32-bit offsets) array is 32-bit
		}
	}
}

// Simple GPU-ized version of mi64_twopmodq.
// Return 1 if q = 2.k.p+1 divides 2^p-1, 0 otherwise.
__device__ uint32 mi64_twopmodq_gpu(
	const uint64*p, const uint64*pshift, uint64*gpu_thread_local, const uint32 lenP, const uint32 lenQ,
	const uint32 FERMAT, const uint32 pow2, const uint64 k, const uint32 start_index, const uint32 zshift,
	const int i	// thread id (handy for debug)
)
{
#ifdef __CUDA_ARCH__	// Only allow device-code compilation of function body, otherwise get
						// no end of inline-asm-related errors, since nvcc doesn't speak that language
	// Each thread needs 5*(len_p+1) 64-bit words of local array storage:
	uint64 *q, *qhalf, *qinv, *x, *lo, *hi, *scratch, *scratch2;
	 int32 j, log2_numbits = lenQ << 6;	// Current-Bit index j needs to be signed because of the LR binary exponentiation.
	uint64 lo64, cyout;
	// Init pointers into this thread's chunk of the local-mem array:
	q     = gpu_thread_local + lenQ;
	qhalf = gpu_thread_local + lenQ*2;
	qinv  = gpu_thread_local + lenQ*3;
	x     = gpu_thread_local + lenQ*4;
	lo    = gpu_thread_local + lenQ*5;
	hi    = gpu_thread_local + lenQ*6;
	scratch  = hi + lenQ;
	scratch2 = hi + lenQ*2;
	cyout = mi64_mul_scalar(p,k<<1,q,lenQ);	ASSERT(HERE, 0 == cyout, "unexpected carryout of 2*p*k!");
	q[0] += 1;	// q = 2.k.p + 1; No need to check for carry since 2.k.p even
	mi64_shrl(q, qhalf, 1, lenQ);	/* (q >> 1) = (q-1)/2, since q odd. */

	// Find modular inverse (mod 2^qbits) of q in preparation for modular multiply:
	mi64_clear(qinv, lenQ);
	qinv[0] = (q[0] + q[0] + q[0]) ^ 2ull;
	for(j = 2; j < 6; j++)	/* At each step, have 2^j correct low-order bits in qinv */
	{
		lo64 = q[0]*qinv[0];
		qinv[0] = qinv[0]*(2ull - lo64);
	}

	// Now that have bottom 64 = 2^6 bits of qinv, do more Newton iterations as needed to get full [qbits] of qinv:
	for(j = 6; j < log2_numbits; j++)
	{
		mi64_mul_vector_lo_half(q, qinv, x, lenQ, scratch);	// Use *scratch for scratch storage here and elsewhere
		mi64_nega              (x, x, lenQ);
		cyout = mi64_add_scalar(x, 2ull, x, lenQ);
		mi64_mul_vector_lo_half(qinv, x, qinv, lenQ, scratch);
	}
	// Check the computed inverse:
	mi64_mul_vector_lo_half(q, qinv, x, lenQ, scratch);

	// Since zstart is a power of two < 2^192, use a streamlined code sequence for the first iteration:
	j = start_index-1;

	// MULL(zstart,qinv,lo) simply amounts to a left-shift of the bits of qinv:
	mi64_shl(qinv, x, zshift, lenQ);
  if(FERMAT) {
	mi64_mul_vector_hi_qferm(x, pow2, k, lo, (lenQ<<6), scratch);
  } else {
	mi64_mul_vector_hi_half(q, x, lo, lenQ, scratch, scratch2);
  }
	// hi = 0 in this instance, which simplifies things:
	cyout = mi64_sub(q, lo, x, lenQ);
	if(mi64_test_bit(pshift, j))
	{
		/* Combines overflow-on-add and need-to-subtract-q-from-sum checks */
		if(mi64_cmpugt(x, qhalf, lenQ)) {
			cyout = mi64_add(x, x, x, lenQ);
			cyout = mi64_sub(x, q, x, lenQ);
		} else {
			cyout = mi64_add(x, x, x, lenQ);
		}
	}

	for(j = start_index-2; j >= 0; j--)
	{
		/*...x^2 mod q is returned in x. */
		mi64_sqr_vector(x, lo, lenQ, scratch);
		mi64_mul_vector_lo_half(lo, qinv, x, lenQ, scratch);
	  if(FERMAT) {
		mi64_mul_vector_hi_qferm(x, pow2, k, lo, (lenQ<<6), scratch);
	  } else {
		mi64_mul_vector_hi_half(q, x, lo, lenQ, scratch, scratch2);
	  }
		/* If h < l, then calculate q-l+h < q; otherwise calculate h-l. */
		if(mi64_cmpult(hi, lo, lenQ)) {
			cyout = mi64_sub(q, lo, lo, lenQ);
			cyout = mi64_add(lo, hi, x, lenQ);
		} else {
			cyout = mi64_sub(hi, lo, x, lenQ);
		}

		if(mi64_test_bit(pshift, j)) {
			/* Combines overflow-on-add and need-to-subtract-q-from-sum checks */
			if(mi64_cmpugt(x, qhalf, lenQ)) {
				cyout = mi64_add(x, x, x, lenQ);
				cyout = mi64_sub(x, q, x, lenQ);
			} else {
				cyout = mi64_add(x, x, x, lenQ);
			}
		}
	}

	/*...Double and return.	These are specialized for the case
	where 2^p == 1 mod q implies divisibility, in which case x = (q+1)/2.
	*/
	cyout = mi64_add_cyin(x, x, x, lenQ, FERMAT);	// In the case of interest, x = (q+1)/2, so x + x cannot overflow.
	cyout = mi64_sub     (x, q, x, lenQ);

	return mi64_cmp_eq_scalar(x, 1ull, lenQ);

#else	// ifndef __CUDA_ARCH__
	ASSERT(HERE, 0, "Device code being called in host mode!");
	return 0;
#endif
}
/*
Need GPU-specific versions of:

mi64_add
mi64_add_cyin
mi64_add_scalar
mi64_clear
mi64_cmp_eq_scalar
mi64_cmpugt
mi64_cmpult
mi64_mul_vector_hi_half
mi64_mul_vector_hi_qferm
mi64_mul_vector_lo_half
mi64_nega
mi64_shl
mi64_shrl
mi64_sqr_vector
mi64_sub
mi64_test_bit
*/
#endif	// __CUDACC__

/*******************/

/* Fill the [len] words of the input vector with random bits. Assumes RNG
has been inited at program start via call to util.c:host_init() : */
#ifdef __CUDA_ARCH__
__device__
#endif
void	mi64_rand(uint64 x[], uint32 len)
{
	uint32 i;
	for(i = 0; i < len; ++i) {
		x[i] = rng_isaac_rand();
	}
}

/* Set X = Y, both length-(len) vectors.
Note 1: Argument order same as C memcpy, i.e. matches mnemonic "x = y".
Note 2: By default, no array-overlap checking is done. To check for such overlap, uncomment the relevant asssertion
*/
#ifdef __CUDA_ARCH__
__device__
#endif
void	mi64_set_eq(uint64 x[], const uint64 y[], uint32 len)
{
	uint32 i;
	ASSERT(HERE, len != 0, "zero-length array!");
	if(x == y) return;
//	ASSERT(HERE, !ARRAYS_OVERLAP(x,len, y,len), "Input arrays overlap!");	* Fairly expensive to check, so disable by default *
	for(i = 0; i < len; ++i) {
		x[i] = y[i];
	}
}

/* Set-X-equal-to-scalar-A: */
#ifdef __CUDA_ARCH__
__device__
#endif
void	mi64_set_eq_scalar(uint64 x[], const uint64 a, uint32 len)
{
	uint32 i;
	ASSERT(HERE, len != 0, "zero-length array!");
	x[0] = a;
	for(i = 1; i < len; ++i) {
		x[i] = 0ull;
	}
}

/* Set-X-equal-to-0: */
#ifdef __CUDA_ARCH__
__device__
#endif
void	mi64_clear(uint64 x[], uint32 len)
{
	uint32 i;
	for(i = 0; i < len; ++i) {
		x[i] = 0ull;
	}
}

/* Set selected bit: */
#ifdef __CUDA_ARCH__
__device__
#endif
void	mi64_set_bit(uint64 x[], uint32 bit)
{
	uint32 bit_in_word = bit&63, word = (bit >> 6);
	x[word] |= (0x0000000000000001ull << bit_in_word);
}

/* Return 1 if selected bit is set, 0 otherwise: */
#ifdef __CUDA_ARCH__
__device__
#endif
int		mi64_test_bit(const uint64 x[], uint32 bit)
{
	uint32 bit_in_word = bit&63, word = (bit >> 6);
	return (int)(x[word] >> bit_in_word) & 0x1;
}

/*******************/

/*
Left-shift a base-2^64 int x[] by (nbits) bits, returning the result in y[].
Any off-shifted bits aside from the least-significant 64 are lost. No array-bounds checking is done.

Returns low 64 bits of off-shifted portion.
Allows in-place operation, i.e. x == y.
*/
#ifdef __CUDA_ARCH__
__device__
#endif
uint64	mi64_shl(const uint64 x[], uint64 y[], uint32 nbits, uint32 len)
{
	int i;
	uint32 nwshift = (nbits >> 6), rembits = (nbits & 63), m64bits;
	uint64 lo64 = 0ull;
	ASSERT(HERE, len != 0, "mi64_shl: zero-length array!");
	// Special-casing for 0 shift count:
	if(!nbits) {
		if(x != y) for(i = 0; i < len; i++){ y[i] = x[i]; }
		return 0;
	}
	/* Special-casing for shift-into-Bolivian (that's Mike-Tyson-ese for "oblivion"): */
	if(nwshift >= len) {
		// If nwshift == len, save low word in lo64...
		if(nwshift == len) {
			lo64 = x[0];
		}
		// ... because after this next line, we can only distinguish between (nwhsift < len) and (nwshift >= len):
		nwshift = len;
		rembits = 0;
	}

	/* Take care of the whole-word part of the shift: */
	for(i = len-1; i >= (int)nwshift; i--) {
		y[i] = x[i-nwshift];
	}
	// lo64 plays the part of "y[len]"; see above comment for why we deal with (nwshift >= len) above rather than here:
	if(nwshift && (nwshift < len)) {
		lo64 = x[len-nwshift];
	}
	for(i = nwshift-1; i >= 0; i--) {
		y[i] = 0ull;
	}

	/* If nbits not an exact multiple of the wordlength, take care of remainder: */
	if(rembits) {
		lo64 = (lo64 << rembits) + mi64_shl_short(y,y,rembits,len);
	}
	return lo64;
}

/*******************/

/*
Logical-right-shift of a base-2^64 int x[] by (nbits) bits, returning the result in y[].
Any off-shifted bits aside from the most-significant 64 are lost. No array-bounds checking is done.

Returns high 64 bits of off-shifted portion.
Allows in-place operation, i.e. x == y.
*To-do* Consider adding support for nonzero bits-shifted-in-from-left.
*/
#ifdef __CUDA_ARCH__
__device__
#endif
uint64	mi64_shrl(const uint64 x[], uint64 y[], uint32 nbits, uint32 len)
{
	int i;
	uint32 nwshift = (nbits >> 6), rembits = (nbits & 63), m64bits;
	uint64 hi64 = 0ull;
	ASSERT(HERE, len != 0, "mi64_shrl: zero-length array!");
	// Special-casing for 0 shift count:
	if(!nbits) {
		mi64_set_eq(y, x, len);	// Set y = x
		return 0ull;
	}
	/* Special-casing for shift-into-Bolivian (that's Mike-Tyson-ese for "oblivion"): */
	if(nwshift >= len) {
		// If nwshift == len, save high word in hi64...
		if(nwshift == len) {
			hi64 = x[len-1];
		}
		// ... because after this next line, we can only distinguish between (nwhsift < len) and (nwshift >= len):
		nwshift = len;
		rembits = 0;
	}

	// hi64 plays the part of "y[-1]"; see above comment for why we deal with (nwshift >= len) above rather than here:
	// Must do this *before* the loop because of in-place possibility:
	if(nwshift && (nwshift < len)) {
		hi64 = x[nwshift-1];
	}
	/* Take care of the whole-word part of the shift: */
	for(i = 0; i < len-nwshift; i++) {
		y[i] = x[i+nwshift];
	}
	for(i = len-nwshift; i < len; i++) {
		y[i] = 0ull;
	}

	/* If nbits not an exact multiple of the wordlength, take care of remaining shift bits: */
	if(rembits) {
		m64bits = (64-rembits);
		hi64 = (hi64 >> rembits) + (y[0] << m64bits);
		/* Process all but the most-significant element, in reverse order: */
		for(i = 0; i < len-1; i++) {
			y[i] = (y[i] >> rembits) + (y[i+1] << m64bits);
		}
		/* Most-significant element gets zeros shifted in from the left: */
		y[len-1] >>= rembits;
	}
	return hi64;
}

/*** Short (< 1-word) shift routines ... on Core2 using ASM + SHLD,SHRD instructions 2x faster than compiled code. ***/

/*
"Short" Left-shift a base-2^64 int x[] by (nbits < 64) bits, returning the result in y[].
Returns the off-shifted bits.
Allows in-place operation, i.e. x == y.
*/
#ifdef __CUDA_ARCH__
__device__
#endif
// Pure-C reference code, useful for debugging the much-more-complex ASM-optimized production routine:
uint64	mi64_shl_short_ref(const uint64 x[], uint64 y[], uint32 nbits, uint32 len)
{
	int i;
	uint32 m64bits = (64-nbits);
	uint64 lo64 = 0ull;
	ASSERT(HERE, len != 0 && nbits < 64, "mi64_shl: zero-length array or shift count >= 64!");
	// Special-casing for 0 shift count:
	if(!nbits) {
		if(x != y) for(i = 0; i < len; i++){ y[i] = x[i]; }
		return 0;
	}
	// lo64 plays the part of "y[len]"; must again work our way from high word to low here,
	// since y[len-1] (used to compute lo64) gets modified on first iteration of ensuing loop:
	lo64 = (x[len-1] >> m64bits);
	// Process all but the least-significant element x[0], in downward (high-to-low-word) order.
	// Full-vector (except for x[0]) processing loop if no ASM; high-words cleanup-loop if ASM:
	for(i = len-1; i > 0; i--) {
		y[i] = (x[i] << nbits) + (x[i-1] >> m64bits);
	}
	// Least-significant element gets zeros shifted in from the right:
	y[0] = (x[0] << nbits);
	return lo64;
}

#if MI64_DEBUG
	#define MI64_SHL1_DBG	0	// Set nonzero to enable debug-print in the mi64 modpow functions below
#endif
uint64	mi64_shl_short(const uint64 x[], uint64 y[], uint32 nbits, uint32 len)
{
#if MI64_SHL1_DBG
	int dbg = 0;
	uint64 ref[1000];	if(len < 1000) ref[len] = mi64_shl_short_ref(x,ref,nbits,len);	// ref = x << nbits
#endif
  #ifdef USE_AVX2	// SSE2,AVX,AVX2 same in terms of processing 8 qwords per ASM-loop pass, but AVX2 must skip x[0:3]
	const uint32 BLOCKLENM1 = 7, BASEADDRMASK = 0x1F;	uint32 minlen = 9;
  #elif defined(USE_AVX)	// SSE2/AVX - use SSE2 ASM for both cases - do 8 words per ASM-lop pass, must skip x[0:1]
	const uint32 BLOCKLENM1 = 7, BASEADDRMASK = 0x0F;	uint32 minlen = 9;
  #elif defined(YES_ASM)
	const uint32 BLOCKLENM1 = 3, BASEADDRMASK = 0x07;	uint32 minlen = 5;
  #endif
	int i, i0 = 0, i1 = 1, use_asm = FALSE, x_misalign = 0, y_misalign = 0;
	uint32 m64bits = (64-nbits), leftover = 0;
	uint64 lo64 = 0ull;
	ASSERT(HERE, len != 0 && nbits < 64, "mi64_shl: zero-length array or shift count >= 64!");
	// Special-casing for 0 shift count:
	if(!nbits) {
		if(x != y) for(i = 0; i < len; i++){ y[i] = x[i]; }
		return 0;
	}
	// lo64 plays the part of "y[len]"; must again work our way from high word to low here,
	// since y[len-1] (used to compute lo64) gets modified on first iteration of ensuing loop:
	lo64 = (x[len-1] >> m64bits);

	/*
	Process all but the least-significant element x[0], in downward (high-to-low-word) order.
	[Note the asm-loop using the SHRD instruction was actually slower than the C-loop until being 4-way unrolled]

	Since x[0] is handled separately using C code, x[1] is where we start our SIMD-register alignment check.
	Set the index bounds for ASM-loop processing in a way which makes the ASM-loop data chunks 32-byte[AVX2]
	or 16-byte[SSE2/AVX] aligned, allowing us to use aligned SIMD loads, which is highly timing-friendly.
	
	Consider the case of a 13-element input vector - since the [0]-elt must be handled separately using C code,
	we have elts [1-12] as possible candidats for SIMD vector-processing. If - using AVX2 reg-width for this example -
	x[1] is 32-byte-aligned, we start vector processing with that. Visually, [] enclose the hex-indices of the
	quadword octets we want the AVX2 ASM to handle:
	
		c	b	a	9	[8	7	6	5	4	3	2	1]	0
	
	That is, we want to define leftover = (len-1)%8 = 4, and use C cleanup code to process (leftover) elements at the
	high end [9-c] via C-loop. However, we cannot assume such array alignment, so more generally at runtime we find
	the minimum-indexed element x[i0] above x[0] which is 32-byte aligned and start vector processing with that.
	This means i0 may range from 1-4; in the worst-case misalignment scenario we have i0 = 4 and things look like so:
	
		c	[b	a	9	8	7	6	5	4]	3	2	1	0
	
	For AVX2 our ASM shift macro does 8 elements at a time, so min-length which will trigger any ASM is len = 8 + i0,
	and in our above example, leftover = (len - i0)%8 = 1, i.e. 1 element at the top need cleaning-up via the C code.
	
	For SSE2 and AVX - both use the same SSE2-based code since AVX only supports 128-bit vector integer operands -
	we require just 16-byte alignment, thus i0 may = 1 or = 2; in the worst-case, i0 = 2. Since our SSE2-mode ASM-loop
	also processes 8 elements like the AVX2-loop, we define leftover = (len - i0)%8 for the 13-element example, thus
	we use C cleanup code to process (leftover) elements at the high end and i0 elements at the low end. Visually,
	for the worst case, i0 = 2:
	
		c	b	a	[9	8	7	6	5	4	3	2]	1	0
	
	Since the x[0] element is off-limits for ASM-processing, for SSE2/AVX, the min-length which will trigger
	any ASM is thus again len = 9 + i0.
	*/
  #ifdef YES_ASM	// Covers AVX2/AVX/SSE2 as well as non-SIMD x86_64

	if(len >= minlen) {
		/* Check alignment of x and y:
			1. always require 8-byte alignment;
			2. if x,y have different 16-byte[SSE2/AVX] or 32-byte[AVX2] alignment, skip the ASM-loop;
			3. if x,y have same 16-byte[SSE2/AVX] or 32-byte[AVX2] alignment, find i0 >= 1 such that x[i0] is SIMD-aligned.
		*/
		if( ((uint32)x & 0x7) != 0 || ((uint32)y & 0x7) != 0 )
			ASSERT(HERE, 0, "require 8-byte alignment of x,y!");
		// In SIMD-ASM case, x_misalign = (0,1,2, or 3) how many words x[0] is above next-lower alignment boundary:
		x_misalign = ((uint32)x & BASEADDRMASK)>>3;	y_misalign = ((uint32)y & BASEADDRMASK)>>3;

		if(len >= minlen) {	// Low-end clean-up loop runs from i = i0 downward thru i = 1 ... x[0] handled separately:
		  #ifdef USE_AVX2
			i0 = 4-x_misalign;	// AVX2: i0 must be 32-byte aligned, i.e. must point to lowest-order word
					// x[i] with i > 0 which is 32-byte aligned. If x[0] is so aligned (x_misalign = 0) take
					// i0=3, else if x[0] only 8/16/24-byte aligned (x_misalign=1-3), take i=3,2,1, resp.
		  #elif defined(USE_AVX)
			i0 = 2-x_misalign;	// SSE2/AVX: i0 must be 16-byte aligned, i.e. must point to lowest-order
					// word x[i] with i > 0 which is 16-byte aligned. If x[0] is so aligned (x_misalign = 0)
					// that means i0=1, otherwise if x[0] only 8-byte aligned (x_misalign=1), take i0 = 0.
		  #else
			i0 = 1;	// Generic (no-SIMD), no alignment needed other than 8-byte one asserted above, but must skip 0-word
		  #endif
		}
		minlen += i0;		// Do *not* commit i0,i1 until have determined whether use_asm == TRUE
		use_asm = (len >= minlen);
		if(use_asm) {	// Low-end clean-up loop runs from i = i0 downward thru i = 1 ... x[0] handled separately:
			leftover = (len-(i0+1)) & BLOCKLENM1;	// #words needing handling by pre-ASM (high-words) cleanup loop
			i1 = len-leftover-1;	// i1 = one-below-min-elt for high-words cleanup loo
		} else {
			i0 = 0;	// Revert i0 to default
		}
	}
  #endif

	/* Example, using non-SIMD x86_64 build, i.e. YES_ASM = true, asm-loop data only required to be 8-byte aligned:
	len,i0,i1,leftover = 6,1,5,0, misalign = 0, use_asm = 1; x,y = 0x400510,0x400510, base-addr for SHL macro = 0x400528
	
		* high-words cleanup-loop does words [len-1:i1] = [5:5], i.e. top leftover+1 = 1 words
		* ASM-loop does words [i1:i0+1] = [5:2], should be doing 4:1! (that is why we get error in 1-word)
		* low-words cleanup-loop does words [i0-1:1] = [0:1], not executed
		* y[0] = (x[0] << nbits) done separately at end.
	*/
  #if MI64_SHL1_DBG
	if(dbg)
		printf("nshift = %u: len,i0,i1,leftover = %u,%u,%u,%u, x,y_misalign = %u,%u, use_asm = %u; x,y = 0x%X,0x%X, base-addr for SHL macro = 0x%X\n",nbits,len,i0,i1,leftover,x_misalign,y_misalign,use_asm,(uint32)x,(uint32)y,(uint32)(x+i1-2));
  #endif
	// Full-vector (except for x[0]) processing loop if no ASM; high-words cleanup-loop if ASM:
	for(i = len-1; i >= i1; i--) {
		y[i] = (x[i] << nbits) + (x[i-1] >> m64bits);
	}

	/*
	Process x[i1-1:i0] in downward (high-to-low-word) order.
	[Note the asm-loop using the SHRD instruction was actually slower than the C-loop until I 4-way unrolled it].
	Since Intel crapified the double-prec shifts post Core2 (latency/recip-thruput on Haswell both 2x those on Core2),
 	under AVX2 use 256-bit vector ops - alas, no in-vector-register shift which allows bits to cross word boundaries -
	to do the shifting:
	*/
	if(use_asm) {	// Collect various-build-mode asm-macros indie this if()

  #ifdef FOO_AVX2	// code sequence here is slower on Haswell/Broadwell than the MOVDQA|MOVDQU-based one below

	__asm__ volatile (\
		"movq	%[__x],%%rax		\n\t"/*  Input array */\
		"movq	%[__y],%%rbx		\n\t"/* Output array */\
		"movslq	%[__i1],%%rcx		\n\t"/* ASM loop structured as for(j = i1; j != i0; j -= 8){...} */\
		"leaq	-0x20(%%rax,%%rcx,8),%%rax	\n\t"/* x+i1-4; note in SHL case this points to the *low* word of each ymm-register quartet */\
		"leaq	-0x20(%%rbx,%%rcx,8),%%rbx	\n\t"/* y+i1-4 */\
		"subl	%[__i0],%%ecx		\n\t"/* Skip the bottom (i0) elements */\
		"vmovd	%[__n] ,%%xmm14		\n\t"/* shift count - since imm-operands only take compile-time consts, this costs a vector register */\
		"vmovd	%[__nc],%%xmm15		\n\t"/* complement-shift count, 64-n */\
		"vmovdqa	     (%%rax),%%ymm0	\n\t"/* preload x[i0-(0:3)] */\
	"loop_shl_short:		\n\t"\
		"vmovdqa	-0x20(%%rax),%%ymm2	\n\t"/* load x[i0-(4:7)] */\
		/* Starting with ymm0 = x[i0-(0:3)] and ymm2 = x[i0-(4:7)], need ymm1 = x[i0-(1:4)]: */\
		"vpblendd $0xC0,%%ymm2,%%ymm0,%%ymm1	\n\t"/* ymm1 = x[i0-(4,1,2,3)] [no penalty for applying this dword-instruction to qword data.]
									For SHL take ymm2[src2] = -4,5,6,7 and ymm0[src1] = -0,1,2,3 ==> ymm1[dest] = -4,1,2,3,
									need fetch pattern src[22111111] ==> imm8 = 11000000 = 3*2^6 = 192 = 0xC0. */\
		"vpermq		$0x93,%%ymm1,%%ymm1	\n\t"/* ymm1 = -4,1,2,3 ==> -1,2,3,4 [0x93 = 0b10 01 00 11, i.e. put qwords 2,1,0,3 into slots 3-0] */\
		"vpsllq		%%xmm14,%%ymm0,%%ymm0	\n\t"/* x[i0-(0:3)]<<    n  */\
		"vpsrlq		%%xmm15,%%ymm1,%%ymm1	\n\t"/* x[i0-(1:4)]>>(64-n) */\
		"vpaddq		%%ymm1 ,%%ymm0,%%ymm0	\n\t"/* add carryin-bits */\
		"vmovdqa	%%ymm0,    (%%rbx)	\n\t"/* output dbl-qword */\
		/* i0-(4:7): ymm0,2 swap roles relative to above code section: */\
		"vmovdqa	-0x40(%%rax),%%ymm0	\n\t"/* load x[i0-(8:b)] */\
		"vpblendd $0xC0,%%ymm0,%%ymm2,%%ymm1	\n\t"/* ymm1 = x[i0-(8,5,6,7)] */\
		"vpermq		$0x93,%%ymm1,%%ymm1	\n\t"/* ymm1 = -8,5,6,7 ==> -5,6,7,8 */\
		"vpsllq		%%xmm14,%%ymm2,%%ymm2	\n\t"/* x[i0-(4:7)]<<    n  */\
		"vpsrlq		%%xmm15,%%ymm1,%%ymm1	\n\t"/* x[i0-(5:8)]>>(64-n) */\
		"vpaddq		%%ymm1 ,%%ymm2,%%ymm2	\n\t"/* add carryin-bits */\
		"vmovdqa	%%ymm2,-0x20(%%rbx)	\n\t"/* output dbl-qword */\
		/* Decrement array pointers by 8 words */\
		"subq	$0x40,%%rax			\n\t"\
		"subq	$0x40,%%rbx			\n\t"\
	"subq	$8,%%rcx		\n\t"\
	"jnz loop_shl_short		\n\t"/* loop end; continue is via jump-back if rcx != 0 */\
		:	/* outputs: none */\
		: [__x] "m" (x)	/* All inputs from memory addresses here */\
		 ,[__y] "m" (y)	\
		 ,[__i0] "m" (i0)	\
		 ,[__i1] "m" (i1)	\
		 ,[__n] "m" (nbits)	\
		 ,[__nc] "m" (m64bits)	\
		: "cc","memory","rax","rbx","rcx","xmm0","xmm1","xmm2","xmm14","xmm15"	/* Clobbered registers */\
		);

  #elif defined(USE_AVX2)

	// No Separate versions for same-16-byte-aligned x,y and 8-byte-staggered,
	// since under AVX2, MOVDQU applied to aligned data runs as fast as MOVDQA:
	__asm__ volatile (\
		"movq	%[__x],%%rax		\n\t"/*  Input array */\
		"movq	%[__y],%%rbx		\n\t"/* Output array */\
		"movslq	%[__i1],%%rcx		\n\t"/* ASM loop structured as for(j = i1; j != i0; j -= 8){...} */\
		"leaq	-0x20(%%rax,%%rcx,8),%%rax	\n\t"/* x+i1-4; note in SHL case this points to the *low* word of each ymm-register quartet */\
		"leaq	-0x20(%%rbx,%%rcx,8),%%rbx	\n\t"/* y+i1-4 */\
		"subl	%[__i0],%%ecx		\n\t"/* Skip the bottom (i0) elements */\
		"vmovd	%[__n] ,%%xmm14		\n\t"/* shift count - since imm-operands only take compile-time consts, this costs a vector register */\
		"vmovd	%[__nc],%%xmm15		\n\t"/* complement-shift count, 64-n */\
	"loop_shl_short2:		\n\t"\
	/* Replacing this sequence (and similarly in SHRL) with a preload-(0:3)/aligned-load-(4:7|8:b)/permute-to-get-(1:4|5:8) was slower (0.7 cycles/limb vs 0.95): */\
		/* i0-(0:3): */\
		"vmovdqu	-0x08(%%rax),%%ymm1	\n\t"/* load x[i0-(1:4)] */\
		"vmovdqa	     (%%rax),%%ymm0	\n\t"/* load x[i0-(0:3)] */\
		"vpsllq		%%xmm14,%%ymm0,%%ymm0	\n\t"/* x[i0-(0:3)]<<    n  */\
		"vpsrlq		%%xmm15,%%ymm1,%%ymm1	\n\t"/* x[i0-(1:4)]>>(64-n) */\
		"vpaddq		%%ymm1 ,%%ymm0,%%ymm2	\n\t"/* add carryin-bits ... delay output-write here. */\
		/* i0-(4:7): */\
		"vmovdqu	-0x28(%%rax),%%ymm1	\n\t"/* load x[i0-(5:8)] */\
		"vmovdqa	-0x20(%%rax),%%ymm0	\n\t"/* load x[i0-(4:7)] */\
		"vpsllq		%%xmm14,%%ymm0,%%ymm0	\n\t"/* x[i0-(4:7)]<<    n  */\
		"vpsrlq		%%xmm15,%%ymm1,%%ymm1	\n\t"/* x[i0-(5:8)]>>(64-n) */\
		"vpaddq		%%ymm1 ,%%ymm0,%%ymm0	\n\t"/* add carryin-bits */\
		"vmovdqu	%%ymm2,     (%%rbx)	\n\t"/* sign. faster to delay & combine this write with the 4:7 one here. (But not in aligned case) */\
		"vmovdqu	%%ymm0,-0x20(%%rbx)	\n\t"/* output dbl-qword */\
		/* Decrement array pointers by 8 words */\
		"subq	$0x40,%%rax			\n\t"\
		"subq	$0x40,%%rbx			\n\t"\
	"subq	$8,%%rcx		\n\t"\
	"jnz loop_shl_short2		\n\t"/* loop end; continue is via jump-back if rcx != 0 */\
		:	/* outputs: none */\
		: [__x] "m" (x)	/* All inputs from memory addresses here */\
		 ,[__y] "m" (y)	\
		 ,[__i0] "m" (i0)	\
		 ,[__i1] "m" (i1)	\
		 ,[__n] "m" (nbits)	\
		 ,[__nc] "m" (m64bits)	\
		: "cc","memory","rax","rbx","rcx","xmm0","xmm1","xmm2","xmm14","xmm15"	/* Clobbered registers */\
		);

  #elif defined(USE_AVX)	//*** See commentary in mi64_shrl_short for why wrap this SSE2 code in an AVX-only flag

	if(x_misalign == y_misalign) {	// Separate versions for same-16-byte-aligned x,y and 8-byte-staggered:

	// Ex: len,i0,i1,leftover = 10,2,9,0. ASM-loop should process index octet [9:2] via pairs [9,8],[7,6],[5,4],[3,2],
	// leaving [1:0] for bottom-cleanup loop. Starting ASM-loop index rcx = i1 = 9 gives rcx-8 = 1, as desired since
	// in SHL function we handle 0-element separately at the very end:
	__asm__ volatile (\
		"movq	%[__x],%%rax		\n\t"/*  Input array */\
		"movq	%[__y],%%rbx		\n\t"/* Output array */\
		"movslq	%[__i1],%%rcx		\n\t"/* ASM loop structured as for(j = i1; j != i0; j -= 8){...} */\
		"leaq	-0x10(%%rax,%%rcx,8),%%rax	\n\t"/* x+i1-2; note in SHL case this points to the *low* word of each xmm-register pair */\
		"leaq	-0x10(%%rbx,%%rcx,8),%%rbx	\n\t"/* y+i1-2 */\
		"subl	%[__i0],%%ecx		\n\t"/* Skip the bottom (i0) elements */\
		"movd	%[__n] ,%%xmm14		\n\t"/* shift count - since imm-operands only take compile-time consts, this costs a vector register */\
		"movd	%[__nc],%%xmm15		\n\t"/* complement-shift count, 64-n */\
		"movdqa	     (%%rax),%%xmm0	\n\t"/* preload x[i1-(0,1)] */\
	"loop_shl_short:		\n\t"\
	/* 1st version did 2 MOVDQU-load per double-qword output; current version does just 1 MOVDQU, instead uses
	shuffles to generate the 1-qword-staggered shift-in-data xmm-register operand, cuts cycles by 15% on Core2. */\
		/* i1-(0,1): x[i1-(0,1)] in xmm0 */\
		"movdqa	-0x10(%%rax),%%xmm2	\n\t"/* load x[i1-(2,3)] */\
		"movdqa %%xmm2,%%xmm1		\n\t"/* copy x[i1-(2,3)] */\
		"shufpd $1,%%xmm0,%%xmm1	\n\t"/* xmm1:x[i1-(1,2)] */\
		"psllq		%%xmm14,%%xmm0	\n\t"/* x[i1-(0,1)]<<    n  */\
		"psrlq		%%xmm15,%%xmm1	\n\t"/* x[i1-(1,2)]>>(64-n) */\
		"paddq		%%xmm1 ,%%xmm0	\n\t"/* add carryin-bits */\
		"movdqa	%%xmm0,     (%%rbx)	\n\t"/* output dbl-qword */\
		/* i1-(2,3): x[i1-(2,3)] in xmm2 */\
		"movdqa	-0x20(%%rax),%%xmm0	\n\t"/* load x[i1-(4,5)] */\
		"movdqa %%xmm0,%%xmm1		\n\t"/* copy x[i1-(4,5)] */\
		"shufpd $1,%%xmm2,%%xmm1	\n\t"/* xmm1:x[i1-(3,4)] */\
		"psllq		%%xmm14,%%xmm2	\n\t"/* x[i1-(2,3)]<<    n  */\
		"psrlq		%%xmm15,%%xmm1	\n\t"/* x[i1-(3,4)]>>(64-n) */\
		"paddq		%%xmm1 ,%%xmm2	\n\t"/* add carryin-bits */\
		"movdqa	%%xmm2,-0x10(%%rbx)	\n\t"/* output dbl-qword */\
		/* i1-(4,5): x[i1-(4,5)] in xmm0 */\
		"movdqa	-0x30(%%rax),%%xmm2	\n\t"/* load x[i1-(6,7)] */\
		"movdqa %%xmm2,%%xmm1		\n\t"/* copy x[i1-(6,7)] */\
		"shufpd $1,%%xmm0,%%xmm1	\n\t"/* xmm1:x[i1-(5,6)] */\
		"psllq		%%xmm14,%%xmm0	\n\t"/* x[i1-(4,5)]<<    n  */\
		"psrlq		%%xmm15,%%xmm1	\n\t"/* x[i1-(5,6)]>>(64-n) */\
		"paddq		%%xmm1 ,%%xmm0	\n\t"/* add carryin-bits */\
		"movdqa	%%xmm0,-0x20(%%rbx)	\n\t"/* output dbl-qword */\
		/* i1-(6,7): x[i1-(6,7)] in xmm2 */\
		"movdqa	-0x40(%%rax),%%xmm0	\n\t"/* load x[i1-(8,9)] */\
		"movdqa %%xmm0,%%xmm1		\n\t"/* copy x[i1-(8,9)] */\
		"shufpd $1,%%xmm2,%%xmm1	\n\t"/* xmm1:x[i1-(7,8)] */\
		"psllq		%%xmm14,%%xmm2	\n\t"/* x[i1-(6,7)]<<    n  */\
		"psrlq		%%xmm15,%%xmm1	\n\t"/* x[i1-(7,8)]>>(64-n) */\
		"paddq		%%xmm1 ,%%xmm2	\n\t"/* add carryin-bits */\
		"movdqa	%%xmm2,-0x30(%%rbx)	\n\t"/* output dbl-qword */\
		/* Decrement array pointers by 8 words */\
		"subq	$0x40,%%rax			\n\t"\
		"subq	$0x40,%%rbx			\n\t"\
	"subq	$8,%%rcx		\n\t"\
	"jnz loop_shl_short	\n\t"/* loop end; continue is via jump-back if rcx != 0 */\
		:	/* outputs: none */\
		: [__x] "m" (x)	/* All inputs from memory addresses here */\
		 ,[__y] "m" (y)	\
		 ,[__i0] "m" (i0) \
		 ,[__i1] "m" (i1) \
		 ,[__n] "m" (nbits)	\
		 ,[__nc] "m" (m64bits)	\
		: "cc","memory","rax","rbx","rcx","xmm0","xmm1","xmm2","xmm14","xmm15"	/* Clobbered registers */\
		);

	} else {	// x_misalign != y_misalign

	// Ex: len,i0,i1,leftover = 10,2,9,0. ASM-loop should process index octet [9:2] via pairs [9,8],[7,6],[5,4],[3,2],
	// leaving [1:0] for bottom-cleanup loop. Starting ASM-loop index rcx = i1 = 9 gives rcx-8 = 1, as desired since
	// in SHL function we handle 0-element separately at the very end:
	__asm__ volatile (\
		"movq	%[__x],%%rax		\n\t"/*  Input array */\
		"movq	%[__y],%%rbx		\n\t"/* Output array */\
		"movslq	%[__i1],%%rcx		\n\t"/* ASM loop structured as for(j = i1; j != i0; j -= 8){...} */\
		"leaq	-0x10(%%rax,%%rcx,8),%%rax	\n\t"/* x+i1-2; note in SHL case this points to the *low* word of each xmm-register pair */\
		"leaq	-0x10(%%rbx,%%rcx,8),%%rbx	\n\t"/* y+i1-2 */\
		"subl	%[__i0],%%ecx		\n\t"/* Skip the bottom (i0) elements */\
		"movd	%[__n] ,%%xmm14		\n\t"/* shift count - since imm-operands only take compile-time consts, this costs a vector register */\
		"movd	%[__nc],%%xmm15		\n\t"/* complement-shift count, 64-n */\
		"movdqa	     (%%rax),%%xmm0	\n\t"/* preload x[i1-(0,1)] */\
	"loop_shl_short2:		\n\t"\
	/* 1st version did 2 MOVDQU-load per double-qword output; current version does just 1 MOVDQU, instead uses
	shuffles to generate the 1-qword-staggered shift-in-data xmm-register operand, cuts cycles by 15% on Core2. */\
		/* i1-(0,1): x[i1-(0,1)] in xmm0 */\
		"movdqa	-0x10(%%rax),%%xmm2	\n\t"/* load x[i1-(2,3)] */\
		"movdqa %%xmm2,%%xmm1		\n\t"/* copy x[i1-(2,3)] */\
		"shufpd $1,%%xmm0,%%xmm1	\n\t"/* xmm1:x[i1-(1,2)] */\
		"psllq		%%xmm14,%%xmm0	\n\t"/* x[i1-(0,1)]<<    n  */\
		"psrlq		%%xmm15,%%xmm1	\n\t"/* x[i1-(1,2)]>>(64-n) */\
		"paddq		%%xmm0 ,%%xmm1	\n\t"/* add carryin-bits ... delay output-write here. */\
		/* i1-(2,3): x[i1-(2,3)] in xmm2 */\
		"movdqa	-0x20(%%rax),%%xmm0	\n\t"/* load x[i1-(4,5)] */\
		"movdqa %%xmm0,%%xmm3		\n\t"/* copy x[i1-(4,5)] */\
		"shufpd $1,%%xmm2,%%xmm3	\n\t"/* xmm1:x[i1-(3,4)] */\
		"psllq		%%xmm14,%%xmm2	\n\t"/* x[i1-(2,3)]<<    n  */\
		"psrlq		%%xmm15,%%xmm3	\n\t"/* x[i1-(3,4)]>>(64-n) */\
		"paddq		%%xmm3 ,%%xmm2	\n\t"/* add carryin-bits */\
		"movdqu	%%xmm1,     (%%rbx)	\n\t"/* output dbl-qword */\
		"movdqu	%%xmm2,-0x10(%%rbx)	\n\t"/* output dbl-qword */\
		/* i1-(4,5): x[i1-(4,5)] in xmm0 */\
		"movdqa	-0x30(%%rax),%%xmm2	\n\t"/* load x[i1-(6,7)] */\
		"movdqa %%xmm2,%%xmm1		\n\t"/* copy x[i1-(6,7)] */\
		"shufpd $1,%%xmm0,%%xmm1	\n\t"/* xmm1:x[i1-(5,6)] */\
		"psllq		%%xmm14,%%xmm0	\n\t"/* x[i1-(4,5)]<<    n  */\
		"psrlq		%%xmm15,%%xmm1	\n\t"/* x[i1-(5,6)]>>(64-n) */\
		"paddq		%%xmm0 ,%%xmm1	\n\t"/* add carryin-bits  ... delay output-write here.*/\
		/* i1-(6,7): x[i1-(6,7)] in xmm2 */\
		"movdqa	-0x40(%%rax),%%xmm0	\n\t"/* load x[i1-(8,9)] */\
		"movdqa %%xmm0,%%xmm3		\n\t"/* copy x[i1-(8,9)] */\
		"shufpd $1,%%xmm2,%%xmm3	\n\t"/* xmm1:x[i1-(7,8)] */\
		"psllq		%%xmm14,%%xmm2	\n\t"/* x[i1-(6,7)]<<    n  */\
		"psrlq		%%xmm15,%%xmm3	\n\t"/* x[i1-(7,8)]>>(64-n) */\
		"paddq		%%xmm3 ,%%xmm2	\n\t"/* add carryin-bits */\
		"movdqu	%%xmm1,-0x20(%%rbx)	\n\t"/* output dbl-qword */\
		"movdqu	%%xmm2,-0x30(%%rbx)	\n\t"/* output dbl-qword */\
		/* Decrement array pointers by 8 words */\
		"subq	$0x40,%%rax			\n\t"\
		"subq	$0x40,%%rbx			\n\t"\
	"subq	$8,%%rcx		\n\t"\
	"jnz loop_shl_short2	\n\t"/* loop end; continue is via jump-back if rcx != 0 */\
		:	/* outputs: none */\
		: [__x] "m" (x)	/* All inputs from memory addresses here */\
		 ,[__y] "m" (y)	\
		 ,[__i0] "m" (i0) \
		 ,[__i1] "m" (i1) \
		 ,[__n] "m" (nbits)	\
		 ,[__nc] "m" (m64bits)	\
		: "cc","memory","rax","rbx","rcx","xmm0","xmm1","xmm2","xmm3","xmm14","xmm15"	/* Clobbered registers */\
		);

	}	// x,y same 16-byte alignment?

  #else	// YES_ASM

	// (len >= 5+1[i0]) = non-SIMD-apt value:
	__asm__ volatile (\
		"movq	%[__x],%%r10		\n\t"/*  Input array */\
		"movq	%[__y],%%r11		\n\t"/* Output array */\
		"movslq	%[__i1],%%rbx		\n\t"/* ASM loop structured as for(j = i1; j != i0; j -= 4){...} */\
		"leaq	-0x08(%%r10,%%rbx,8),%%r10	\n\t"/* &x[i1-1] */\
		"leaq	-0x08(%%r11,%%rbx,8),%%r11	\n\t"/* &y[i1-1] */\
		"subl	%[__i0],%%ebx		\n\t"/* Skip the bottom (i0+1) elements */\
		"movslq	%[__n],%%rcx		\n\t"/* shift count */\
		"movq	(%%r10),%%rax		\n\t"/* SHRD allows mem-ref only in DEST, so preload x[i0] */\
	"loop_shl_short:	\n\t"/* Since this non-SIMD asm-code may be active along with the SIMD, append '2' to the label */\
		/* i-0: */\
		"movq	-0x08(%%r10),%%rsi	\n\t"/* load x[i-1] ... the si in rsi stands for 'shift-in' :) */\
		"shldq	%%cl,%%rsi,%%rax	\n\t"/* (x[i],x[i-1])<<n */\
		"movq	%%rax,    (%%r11)	\n\t"/* Write output word */\
		/* i-1: */\
		"movq	-0x10(%%r10),%%rax	\n\t"/* x[i-1] already in rsi, so swap roles of rax/rsi */\
		"shldq	%%cl,%%rax,%%rsi	\n\t"\
		"movq	%%rsi,-0x08(%%r11)	\n\t"\
		/* i-2: */\
		"movq	-0x18(%%r10),%%rsi	\n\t"/* x[i-2] already in rax, so swap back to [i-0] form */\
		"shldq	%%cl,%%rsi,%%rax	\n\t"\
		"movq	%%rax,-0x10(%%r11)	\n\t"\
		/* i-3: */\
		"movq	-0x20(%%r10),%%rax	\n\t"/* x[i-1] already in rsi, so swap back to [i-1] form */\
		"shldq	%%cl,%%rax,%%rsi	\n\t"\
		"movq	%%rsi,-0x18(%%r11)	\n\t"\
		/* Decrement array pointers by 4 words */\
		"subq	$0x20,%%r10			\n\t"\
		"subq	$0x20,%%r11			\n\t"\
	"subq	$4,%%rbx		\n\t"\
	"jnz loop_shl_short	\n\t"/* loop end; continue is via jump-back if rcx != 0 */\
		:	/* outputs: none */\
		: [__x] "m" (x)	/* All inputs from memory addresses here */\
		 ,[__y] "m" (y)	\
		 ,[__i0] "m" (i0)	\
		 ,[__i1] "m" (i1)	\
		 ,[__n] "m" (nbits)	\
		: "cc","memory","rax","rbx","rcx","rsi","r10","r11"	/* Clobbered registers */\
		);

  #endif

	}	// use_asm?

	// Low-end clean-up loop (only used in ASM-loop case):
	for(i = i0-1; i > 0; i--) {
	#if MI64_SHL1_DBG
		if(dbg) printf("Low-end clean-up loop: x[%u,%u] = 0x%16llX,0x%16llX; <<%u,>>%u = 0x%16llX,0x%16llX\n",i,i-1,x[i],x[i-1],nbits,m64bits,(x[i] << nbits),(x[i-1] >> m64bits));
	#endif
		y[i] = (x[i] << nbits) + (x[i-1] >> m64bits);
	#if MI64_SHL1_DBG
		if(dbg) printf("    ==> y[%u] = 0x%16llX\n",i,y[i]);
	#endif
	}
	// Least-significant element gets zeros shifted in from the right:
	y[0] = (x[0] << nbits);
  #if MI64_SHL1_DBG
	if(len < 1000) {
		if(lo64 != ref[len]) { printf("SHL1 Carryout mismatch: (y[%u] = %16llX) != (ref[%u] = %16llX)\n",len,lo64,len,ref[len]); ASSERT(HERE, 0, "Exiting!"); }
		if(!mi64_cmp_eq(y,ref,len)) { for(i = len-1; i >= 0; i--) { if(y[i] != ref[i]) { printf("(y[%u] = %16llX) != (ref[%u] = %16llX)\n",i,y[i],i,ref[i]); printf("nshift = %u: len,i0,i1,leftover = %u,%u,%u,%u, misalign = %u, use_asm = %u; x,y = 0x%X,0x%X, base-addr for SHL macro = 0x%X\n",nbits,len,i0,i1,leftover,x_misalign,use_asm,(uint32)x,(uint32)y,(uint32)(x+i1-2)); ASSERT(HERE, 0, "Exiting!"); } } }
	}
  #endif
	return lo64;
}

/*
"Short" Logical-right-shift a base-2^64 int x[] by (nbits < 64) bits, returning the result in y[].
Returns the off-shifted bits.
Allows in-place operation, i.e. x == y.
*/
#ifdef __CUDA_ARCH__
__device__
#endif
// Pure-C reference code, useful for debugging the much-more-complex ASM-optimized production routine:
uint64	mi64_shrl_short_ref(const uint64 x[], uint64 y[], uint32 nbits, uint32 len)
{
	int i;
	uint32 m64bits = (64-nbits), leftover = 0;
	uint64 hi64 = 0ull;
	ASSERT(HERE, len != 0 && nbits < 64, "mi64_shl: zero-length array or shift count >= 64!");
	// Special-casing for 0 shift count:
	if(!nbits) {
		if(x != y) for(i = 0; i < len; i++){ y[i] = x[i]; }
		return 0;
	}
	hi64 = (x[0] << m64bits);
	// Process all but the most-significant element, in upward (low-to-high-word) order:
	for(i = 0; i < len-1; i++) {
		y[i] = (x[i] >> nbits) + (x[i+1] << m64bits);
	}
	// Most-significant element gets zeros shifted in from the left:
	y[len-1] = (x[len-1] >> nbits);
	return hi64;
}

#if MI64_DEBUG
	#define MI64_SHR1_DBG	0	// Set nonzero to enable debug-print in the mi64 modpow functions below
#endif
uint64	mi64_shrl_short(const uint64 x[], uint64 y[], uint32 nbits, uint32 len)
{
#if MI64_SHR1_DBG
	int dbg = 0;
	uint64 ref[1000];	if(len < 1000) ref[len] = mi64_shrl_short_ref(x,ref,nbits,len);	// ref = x << nbits
#endif
  #ifdef USE_AVX2	// SSE2,AVX,AVX2 same in terms of processing 8 qwords per ASM-loop pass, but AVX2 must skip x[0:3]
	const uint32 BLOCKLENM1 = 7, BASEADDRMASK = 0x1F;	uint32 minlen = 9;
  #elif defined(USE_AVX)	// SSE2,AVX macros both do 8 words per ASM-lop pass, must skip x[0:1]
	const uint32 BLOCKLENM1 = 7, BASEADDRMASK = 0x0F;	uint32 minlen = 9;
  #elif defined(YES_ASM)
	const uint32 BLOCKLENM1 = 3, BASEADDRMASK = 0x07;	uint32 minlen = 5;
  #endif
	int i, i0 = 0, i1 = 0, use_asm = FALSE, x_misalign, y_misalign;
	uint32 m64bits = (64-nbits), leftover = 0;
	uint64 hi64 = 0ull;
	ASSERT(HERE, len != 0 && nbits < 64, "mi64_shl: zero-length array or shift count >= 64!");
	// Special-casing for 0 shift count:
	if(!nbits) {
		if(x != y) for(i = 0; i < len; i++){ y[i] = x[i]; }
		return 0;
	}
	hi64 = (x[0] << m64bits);
	/*
	Process all but the most-significant element, in upward (low-to-high-word) order:
	[Note the asm-loop using the SHRD instruction was actually slower than the C-loop until I 4-way unrolled it].
	Since Intel crapified the double-prec shifts post Core2 (latency/recip-thruput on Haswell both 2x those on Core2),
	under AVX2 use 256-bit vector ops - alas, no in-vector-register shift which allows bits to cross word boundaries -
	to do the shifting.

	Set the index bounds for ASM-loop processing in a way which allows us to assume the same alignment in the ASM-loop
	as on the 0-element of x[], i.e. by forcing x to be 32-byte[AVX2] or 16-byte[SSE2/AVX] aligned we can use aligned
	loads in both the SSE2 and AVX processing macros, which is highly timing-friendly.
	
	Consider the case of a 13-element input vector - since the [12]-elt must be handled separately (i.e. not using ASM),
	we have elts [0-11] as possible candidats for SIMD vector-processing. If - using AVX2 reg-width for this example -
	x[0] is 32-byte-aligned, we start vector processing with that. Visually, [] enclose the hex-indices of the
	quadword octets we want the AVX2 ASM to handle:
	
		c	b	a	9	8	[7	6	5	4	3	2	1	0]
	
	That is, we want to define leftover = len%8 = 5, and use C cleanup code to process (leftover) elements at the
	high end: [8-b] via loop, [c] separately. However, we cannot assume such array alignment, so more generally
	at runtime we find the minimum-indexed element x[i0] which is 32-byte aligned and start vector processing with it.
	This means i0 may range from 0-3; in the worst-case misalignment scenario we have i0 = 3 and things look like so:
	
		c	b	[a	9	8	7	6	5	4	3]	2	1	0
	
	For AVX2 our ASM shift macro does 8 elements at a time, so min-length which will trigger any ASM is len = 9 + i0,
	and in our above example, leftover = (len - i0)%8 = 2, i.e. 2 elements at the top need cleaning-up via the C code.
	
	For SSE2 and AVX - both use the same SSE2-based code since AVX only supports 128-bit vector integer operands -
	we require just 16-byte alignment, thus i0 may = 0 or = 1; in the worst-case, i0 = 1. Since our SSE2-mode ASM-loop
	also processes 8 elements like the AVX2-loop, we define leftover = (len - i0)%8 for the 13-element example, thus
	we use C cleanup code to process (leftover) elements at the high end and i0 elements at the low end. Visually,
	for the worst case, i0 = 1:
	
		c	b	a	9	[8	7	6	5	4	3	2	1]	0
	
	Since the topmost element is off-limits for ASM-processing, for SSE2/AVX, the min-length which will trigger
	any ASM is thus again len = 9 + i0.
	*/
  #ifdef YES_ASM	// Covers AVX2/AVX/SSE2 as well as non-SIMD x86_64
	if(len >= minlen) {
		/* Check alignment of x and y:
			1. always require 8-byte alignment;
			2. if x,y have different 16-byte[SSE2/AVX] or 32-byte[AVX2] alignment, skip the ASM-loop;
			3. if x,y have same 16-byte[SSE2/AVX] or 32-byte[AVX2] alignment, find i0 >= 0 such that x[i0] is SIMD-aligned.
		*/
		if( ((uint32)x & 0x7) != 0 || ((uint32)y & 0x7) != 0 )
			ASSERT(HERE, 0, "require 8-byte alignment of x,y!");
		x_misalign = ((uint32)x & BASEADDRMASK)>>3;	y_misalign = ((uint32)y & BASEADDRMASK)>>3;

		// minlen may have been incr. for alignment purposes, so use_asm not an unconditional TRUE here
		if(len >= minlen && x_misalign != 0) {	// Low-end clean-up loop runs from i = 0 upward thru i = i0-1
		  #ifdef USE_AVX2
			i0 = 4-x_misalign;	// AVX2: i0 must be 32-byte aligned, i.e. must point to lowest-order word
					// x[i] which is 32-byte aligned. If x[0] is so aligned (x_misalign=0) take
					// i0=0, else if x[0] only 8/16/24-byte aligned (x_misalign=1-3), take i=3,2,1, resp.
		  #elif defined(USE_AVX)
			i0 = 2-x_misalign;	// SSE2/AVX: i0 must be 16-byte aligned, i.e. must point to lowest-order
					// word x[i] with i > 0 which is 16-byte aligned. If x[0] is so aligned (x_misalign=0)
					// that means i0=1, otherwise if x[0] only 8-byte aligned (x_misalign=1), take i0=0.
		  #else
			// Generic (no-SIMD), no alignment needed other than 8-byte one asserted above
		  #endif
		}
		minlen += i0;		// Do *not* commit i0,i1 until have determined whether use_asm == TRUE
		use_asm = (len >= minlen);
		if(use_asm) {	// Low-end clean-up loop runs from i = i0 downward thru i = 1 ... x[0] handled separately:
			leftover = (len-(i0+1)) & BLOCKLENM1;	// #words needing handling by pre-ASM (high-words) cleanup loop
			i1 = len-leftover-1;	// i1 = one-below-min-elt for high-words cleanup loo
		} else {
			i0 = 0;	// Revert i0 to default
		}
	}
  #endif

  #if MI64_SHR1_DBG
	if(dbg)
		printf("nshift = %u: len,i0,i1,leftover = %u,%u,%u,%u, x,y_misalign = %u,%u, use_asm = %u; x,y = 0x%X,0x%X, base-addr for SHRL macro = 0x%X\n",nbits,len,i0,i1,leftover,x_misalign,y_misalign,use_asm,(uint32)x,(uint32)y,(uint32)(x+i1-2));
  #endif
	// Low-end cleanup-loop if ASM:
	for(i = 0; i < i0; i++) {
		y[i] = (x[i] >> nbits) + (x[i+1] << m64bits);
	}

	if(use_asm) {	// Collect various-build-mode asm-macros indie this if()

	#ifdef FOO_AVX2	// code sequence here is slower on Haswell/Broadwell than the MOVDQA|MOVDQU-based one below

	__asm__ volatile (\
		"movq	%[__x],%%rax		\n\t"/*  Input array */\
		"movq	%[__y],%%rbx		\n\t"/* Output array */\
		"movslq	%[__i0],%%rcx		\n\t"/* Skip the bottom (i0) elements */\
		"leaq	(%%rax,%%rcx,8),%%rax	\n\t"/* x+i0 */\
		"leaq	(%%rbx,%%rcx,8),%%rbx	\n\t"/* y+i0 */\
		"negq	%%rcx				\n\t"/* -i0 */\
		"addl	%[__i1],%%ecx		\n\t"/* ASM loop structured as for(j = i1; j != i0; j -= 8){...} */\
		"vmovd	%[__n] ,%%xmm14		\n\t"/* shift count - since imm-operands only take compile-time consts, this costs a vector register */\
		"vmovd	%[__nc],%%xmm15		\n\t"/* complement-shift count, 64-n */\
		"vmovdqa		(%%rax),%%ymm0	\n\t"/* preload x[3-0]  */\
	"loop_shrl_short:		\n\t"\
		/* i0-i3: */\
		"vmovdqa	0x20(%%rax),%%ymm2	\n\t"/* load x[7-4]  */\
		"vpblendd $3,%%ymm2,%%ymm0,%%ymm1	\n\t"/* ymm1 = 3,2,1,4 [no penalty for applying this dword-instruction to qword data.] */\
		"vpermq		$0x39,%%ymm1,%%ymm1	\n\t"/* --> ymm1 = 4,3,2,1 [0x39 = 0b00 11 10 01, i.e. put qwords 0,3,2,1 into slots 3-0] */\
		"vpsrlq		%%xmm14,%%ymm0,%%ymm0	\n\t"/* x[3-0]>>    n    */\
		"vpsllq		%%xmm15,%%ymm1,%%ymm1	\n\t"/* x[4-1]<<(64-n)   */\
		"vpaddq		%%ymm1 ,%%ymm0,%%ymm0	\n\t"/* add carryin-bits */\
		"vmovdqa	%%ymm0,    (%%rbx)	\n\t"/* output dbl-qword */\
		/* i4-i7: */\
		"vmovdqa	0x40(%%rax),%%ymm0	\n\t"/* load x[b-8]  */\
		"vpblendd $3,%%ymm0,%%ymm2,%%ymm1	\n\t"/* ymm1 = 3,2,1,4 [no penalty for applying this dword-instruction to qword data.] */\
		"vpermq		$0x39,%%ymm1,%%ymm1	\n\t"/* --> ymm1 = 4,3,2,1 [0x39 = 0b00 11 10 01, i.e. put qwords 0,3,2,1 into slots 3-0] */\
		"vpsrlq		%%xmm14,%%ymm2,%%ymm2	\n\t"/* x[7-4]>>    n    */\
		"vpsllq		%%xmm15,%%ymm1,%%ymm1	\n\t"/* x[8-5]<<(64-n)   */\
		"vpaddq		%%ymm1 ,%%ymm2,%%ymm2	\n\t"/* add carryin-bits */\
		"vmovdqa	%%ymm2,0x20(%%rbx)	\n\t"/* output dbl-qword */\
		/* Increment array pointers by 8 words */\
		"addq	$0x40,%%rax			\n\t"\
		"addq	$0x40,%%rbx			\n\t"\
	"subq	$8,%%rcx		\n\t"\
	"jnz loop_shrl_short	\n\t"/* loop end; continue is via jump-back if rcx != 0 */\
		:	/* outputs: none */\
		: [__x] "m" (x)	/* All inputs from memory addresses here */\
		 ,[__y] "m" (y)	\
		 ,[__i0] "m" (i0)	\
		 ,[__i1] "m" (i1)	\
		 ,[__n] "m" (nbits)	\
		 ,[__nc] "m" (m64bits)	\
		: "cc","memory","rax","rbx","rcx","xmm0","xmm1","xmm2","xmm14","xmm15"	/* Clobbered registers */\
		);

	#elif defined(USE_AVX2)

	// Need at least one elt above those done by the asm-loop, thus min-len = 9, not 8

	// No Separate versions for same-16-byte-aligned x,y and 8-byte-staggered,
	// since under AVX2, MOVDQU applied to aligned data runs as fast as MOVDQA:
	__asm__ volatile (\
		"movq	%[__x],%%rax		\n\t"/*  Input array */\
		"movq	%[__y],%%rbx		\n\t"/* Output array */\
		"movslq	%[__i0],%%rcx		\n\t"/* Skip the bottom (i0) elements */\
		"leaq	(%%rax,%%rcx,8),%%rax	\n\t"/* x+i0 */\
		"leaq	(%%rbx,%%rcx,8),%%rbx	\n\t"/* y+i0 */\
		"negq	%%rcx				\n\t"/* -i0 */\
		"addl	%[__i1],%%ecx		\n\t"/* ASM loop structured as for(j = i1; j != i0; j -= 8){...} */\
		"vmovd	%[__n] ,%%xmm14		\n\t"/* shift count - since imm-operands only take compile-time consts, this costs a vector register */\
		"vmovd	%[__nc],%%xmm15		\n\t"/* complement-shift count, 64-n */\
	"loop_shrl_short:		\n\t"\
	/* Replacing this sequence (and similarly in SHL) with the sequence
		preload-(3:0);
		aligned-load-(7:4|b:8);
		permute-to-get-(4:1|8:5)
	was slower (0.95 cycles/limb vs 0.7, on aligned data): */\
		/* i0-i3: */\
		"vmovdqu	0x08(%%rax),%%ymm1	\n\t"/* load x[4-1]  */\
		"vmovdqa		(%%rax),%%ymm0	\n\t"/* load x[3-0]  */\
		"vpsrlq		%%xmm14,%%ymm0,%%ymm0	\n\t"/* x[3-0]>>    n    */\
		"vpsllq		%%xmm15,%%ymm1,%%ymm1	\n\t"/* x[4-1]<<(64-n)   */\
		"vpaddq		%%ymm1 ,%%ymm0,%%ymm2	\n\t"/* add carryin-bits ... delay output-write here. */\
		/* i4-i7: */\
		"vmovdqu	0x28(%%rax),%%ymm1	\n\t"/* load x[8-5]  */\
		"vmovdqa	0x20(%%rax),%%ymm0	\n\t"/* load x[7-4]  */\
		"vpsrlq		%%xmm14,%%ymm0,%%ymm0	\n\t"/* x[7-4]>>    n    */\
		"vpsllq		%%xmm15,%%ymm1,%%ymm1	\n\t"/* x[8-5]<<(64-n)   */\
		"vpaddq		%%ymm1 ,%%ymm0,%%ymm0	\n\t"/* add carryin-bits */\
		"vmovdqu	%%ymm2,    (%%rbx)	\n\t"/* sign. faster to delay & combine this write with the 4:7 one here. (But not in aligned case) */\
		"vmovdqu	%%ymm0,0x20(%%rbx)	\n\t"/* output dbl-qword */\
		/* Increment array pointers by 8 words */\
		"addq	$0x40,%%rax			\n\t"\
		"addq	$0x40,%%rbx			\n\t"\
	"subq	$8,%%rcx		\n\t"\
	"jnz loop_shrl_short	\n\t"/* loop end; continue is via jump-back if rcx != 0 */\
		:	/* outputs: none */\
		: [__x] "m" (x)	/* All inputs from memory addresses here */\
		 ,[__y] "m" (y)	\
		 ,[__i0] "m" (i0)	\
		 ,[__i1] "m" (i1)	\
		 ,[__n] "m" (nbits)	\
		 ,[__nc] "m" (m64bits)	\
		: "cc","memory","rax","rbx","rcx","xmm0","xmm1","xmm2","xmm14","xmm15"	/* Clobbered registers */\
		);

	#elif defined(USE_AVX)

	/*
	Note: Also tried a version which, rather than the 2 MOVDQU-load per double-qword output approach used here
	does just 1 MOVDQU and instead uses shuffles to generate the 1-qword-staggered shift-in-data xmm-register operand.
	That reduced-load approach cuts cycles by 15% on Core2, but is 30% slower on AVX-but-not-AVX2-capable hardware,
	that is, *y Bridge. Since on Core2 it's 2-3x faster to use SH*D rather than SSE2 vector-shifts, retain just the
	SSE2 variant which is faster on *y Bridge and trigger it using the USE_AVX preprocessor flag, to keep it from
	being used on Core2 (where we may wish the USE_SSE2 flag to be invoked for other pieces of code).
	*/
	if(x_misalign == y_misalign) {	// Separate versions for same-16-byte-aligned x,y and 8-byte-staggered:

	__asm__ volatile (\
		"movq	%[__x],%%rax		\n\t"/*  Input array */\
		"movq	%[__y],%%rbx		\n\t"/* Output array */\
		"movslq	%[__i0],%%rcx		\n\t"/* Skip the bottom (i0) elements */\
		"leaq	(%%rax,%%rcx,8),%%rax	\n\t"/* x+i0 */\
		"leaq	(%%rbx,%%rcx,8),%%rbx	\n\t"/* y+i0 */\
		"negq	%%rcx				\n\t"/* -i0 */\
		"addl	%[__i1],%%ecx		\n\t"/* ASM loop structured as for(j = i1; j != i0; j -= 8){...} */\
		"movd	%[__n] ,%%xmm14		\n\t"/* shift count - since imm-operands only take compile-time consts, this costs a vector register */\
		"movd	%[__nc],%%xmm15		\n\t"/* complement-shift count, 64-n */\
		"movdqa		(%%rax),%%xmm0	\n\t"/* preload x[1,0] */\
	"loop_shrl_short:		\n\t"\
	/* 1st version did 2 MOVDQU-load per double-qword output; current version does just 1 MOVDQU, instead uses
	shuffles to generate the 1-qword-staggered shift-in-data xmm-register operand, cuts cycles by 15% on Core2. */\
		/* i+0,1: x[1,0] in xmm0 */\
		"movdqa	0x10(%%rax),%%xmm2	\n\t"/* load x[3,2] */\
		"movdqa %%xmm0,%%xmm1		\n\t"/* copy x[1,0] */\
		"shufpd $1,%%xmm2,%%xmm1	\n\t"/* xmm1: x[2,1] */\
		"psrlq		%%xmm14,%%xmm0	\n\t"/* x[1,0]>>    n    */\
		"psllq		%%xmm15,%%xmm1	\n\t"/* x[2,1]<<(64-n)   */\
		"paddq		%%xmm1 ,%%xmm0	\n\t"/* add carryin-bits */\
		"movdqa	%%xmm0,    (%%rbx)	\n\t"/* output dbl-qword */\
		/* i+2,3: x[3,2] in xmm2 */\
		"movdqa	0x20(%%rax),%%xmm0	\n\t"/* load x[5,4] */\
		"movdqa %%xmm2,%%xmm1		\n\t"/* copy x[3,2] */\
		"shufpd $1,%%xmm0,%%xmm1	\n\t"/* xmm1: x[4,3] */\
		"psrlq		%%xmm14,%%xmm2	\n\t"/* x[3,2]>>    n    */\
		"psllq		%%xmm15,%%xmm1	\n\t"/* x[4,3]<<(64-n)   */\
		"paddq		%%xmm1 ,%%xmm2	\n\t"/* add carryin-bits */\
		"movdqa	%%xmm2,0x10(%%rbx)	\n\t"/* output dbl-qword */\
		/* i+4,5: x[5,4] in xmm0 */\
		"movdqa	0x30(%%rax),%%xmm2	\n\t"/* load x[7,6] */\
		"movdqa %%xmm0,%%xmm1		\n\t"/* copy x[5,4] */\
		"shufpd $1,%%xmm2,%%xmm1	\n\t"/* xmm1: x[6,5] */\
		"psrlq		%%xmm14,%%xmm0	\n\t"\
		"psllq		%%xmm15,%%xmm1	\n\t"\
		"paddq		%%xmm1 ,%%xmm0	\n\t"\
		"movdqa	%%xmm0,0x20(%%rbx)	\n\t"\
		/* i+6,7: x[3,2] in xmm2 */\
		"movdqa	0x40(%%rax),%%xmm0	\n\t"/* load x[9,8] */\
		"movdqa %%xmm2,%%xmm1		\n\t"/* copy x[7,6] */\
		"shufpd $1,%%xmm0,%%xmm1	\n\t"/* xmm1: x[8,7] */\
		"psrlq		%%xmm14,%%xmm2	\n\t"\
		"psllq		%%xmm15,%%xmm1	\n\t"\
		"paddq		%%xmm1 ,%%xmm2	\n\t"\
		"movdqa	%%xmm2,0x30(%%rbx)	\n\t"\
		/* Increment array pointers by 8 words */\
		"addq	$0x40,%%rax			\n\t"\
		"addq	$0x40,%%rbx			\n\t"\
	"subq	$8,%%rcx		\n\t"\
	"jnz loop_shrl_short	\n\t"/* loop end; continue is via jump-back if rcx != 0 */\
		:	/* outputs: none */\
		: [__x] "m" (x)	/* All inputs from memory addresses here */\
		 ,[__y] "m" (y)	\
		 ,[__i0] "m" (i0)	\
		 ,[__i1] "m" (i1)	\
		 ,[__n] "m" (nbits)	\
		 ,[__nc] "m" (m64bits)	\
		: "cc","memory","rax","rbx","rcx","xmm0","xmm1","xmm2","xmm14","xmm15"	/* Clobbered registers */\
		);

	} else {	// x_misalign != y_misalign

	__asm__ volatile (\
		"movq	%[__x],%%rax		\n\t"/*  Input array */\
		"movq	%[__y],%%rbx		\n\t"/* Output array */\
		"movslq	%[__i0],%%rcx		\n\t"/* Skip the bottom (i0) elements */\
		"leaq	(%%rax,%%rcx,8),%%rax	\n\t"/* x+i0 */\
		"leaq	(%%rbx,%%rcx,8),%%rbx	\n\t"/* y+i0 */\
		"negq	%%rcx				\n\t"/* -i0 */\
		"addl	%[__i1],%%ecx		\n\t"/* ASM loop structured as for(j = i1; j != i0; j -= 8){...} */\
		"movd	%[__n] ,%%xmm14		\n\t"/* shift count - since imm-operands only take compile-time consts, this costs a vector register */\
		"movd	%[__nc],%%xmm15		\n\t"/* complement-shift count, 64-n */\
		"movdqa		(%%rax),%%xmm0	\n\t"/* preload x[1,0] */\
	"loop_shrl_short2:		\n\t"\
	/* 1st version did 2 MOVDQU-load per double-qword output; current version does just 1 MOVDQU, instead uses
	shuffles to generate the 1-qword-staggered shift-in-data xmm-register operand, cuts cycles by 15% on Core2. */\
		/* i+0,1: x[1,0] in xmm0 */\
		"movdqa	0x10(%%rax),%%xmm2	\n\t"/* load x[3,2] */\
		"movdqa %%xmm0,%%xmm1		\n\t"/* copy x[1,0] */\
		"shufpd $1,%%xmm2,%%xmm1	\n\t"/* xmm1: x[2,1] */\
		"psrlq		%%xmm14,%%xmm0	\n\t"/* x[1,0]>>    n    */\
		"psllq		%%xmm15,%%xmm1	\n\t"/* x[2,1]<<(64-n)   */\
		"paddq		%%xmm0 ,%%xmm1	\n\t"/* add carryin-bits ... delay output-write here. */\
		/* i+2,3: x[3,2] in xmm2 */\
		"movdqa	0x20(%%rax),%%xmm0	\n\t"/* load x[5,4] */\
		"movdqa %%xmm2,%%xmm3		\n\t"/* copy x[3,2] */\
		"shufpd $1,%%xmm0,%%xmm3	\n\t"/* xmm1: x[4,3] */\
		"psrlq		%%xmm14,%%xmm2	\n\t"/* x[3,2]>>    n    */\
		"psllq		%%xmm15,%%xmm3	\n\t"/* x[4,3]<<(64-n)   */\
		"paddq		%%xmm3 ,%%xmm2	\n\t"/* add carryin-bits */\
		"movdqu	%%xmm1,    (%%rbx)	\n\t"/* output dbl-qword */\
		"movdqu	%%xmm2,0x10(%%rbx)	\n\t"/* output dbl-qword */\
		/* i+4,5: x[5,4] in xmm0 */\
		"movdqa	0x30(%%rax),%%xmm2	\n\t"/* load x[7,6] */\
		"movdqa %%xmm0,%%xmm1		\n\t"/* copy x[5,4] */\
		"shufpd $1,%%xmm2,%%xmm1	\n\t"/* xmm1: x[6,5] */\
		"psrlq		%%xmm14,%%xmm0	\n\t"\
		"psllq		%%xmm15,%%xmm1	\n\t"\
		"paddq		%%xmm0 ,%%xmm1	\n\t"/* ... delay output-write here. */\
		/* i+6,7: x[3,2] in xmm2 */\
		"movdqa	0x40(%%rax),%%xmm0	\n\t"/* load x[9,8] */\
		"movdqa %%xmm2,%%xmm3		\n\t"/* copy x[7,6] */\
		"shufpd $1,%%xmm0,%%xmm3	\n\t"/* xmm1: x[8,7] */\
		"psrlq		%%xmm14,%%xmm2	\n\t"\
		"psllq		%%xmm15,%%xmm3	\n\t"\
		"paddq		%%xmm3 ,%%xmm2	\n\t"\
		"movdqu	%%xmm1,0x20(%%rbx)	\n\t"\
		"movdqu	%%xmm2,0x30(%%rbx)	\n\t"\
		/* Increment array pointers by 8 words */\
		"addq	$0x40,%%rax			\n\t"\
		"addq	$0x40,%%rbx			\n\t"\
	"subq	$8,%%rcx		\n\t"\
	"jnz loop_shrl_short2	\n\t"/* loop end; continue is via jump-back if rcx != 0 */\
		:	/* outputs: none */\
		: [__x] "m" (x)	/* All inputs from memory addresses here */\
		 ,[__y] "m" (y)	\
		 ,[__i0] "m" (i0)	\
		 ,[__i1] "m" (i1)	\
		 ,[__n] "m" (nbits)	\
		 ,[__nc] "m" (m64bits)	\
		: "cc","memory","rax","rbx","rcx","xmm0","xmm1","xmm2","xmm3","xmm14","xmm15"	/* Clobbered registers */\
		);

	}	// x,y same 16-byte alignment?

	#elif defined(FOO_SSE2)	// On pre-AVX x86_64 (e.g. Core2), SHRD and SSE2-based shifts run similarly fast - 1.7
  							// and 2.4 cycles/limb, resp.), try using a hybrid which splits the work across both.
  							// Here use SHRD to handle x[0:3], SSE2-shifts for x[4:7].
	// NOTE: In my timing-tests on Core2 this is faster than above SSE2-based code but slower than the pure-int
	// code below. On Haswell it's faster than the pure-int but both are slower than the AVX2 SIMD-based macro,
	// so disable (USE_SSE2 ==> FOO_SSE2) and no attempt to also implement similar hybrid in the SHL routine.
	__asm__ volatile (\
		"movq	%[__x],%%r10		\n\t"/*  Input array */\
		"movq	%[__y],%%r11		\n\t"/* Output array */\
		"movslq	%[__n],%%rcx		\n\t"/* shift count */\
		"movslq	%[__i0],%%rbx		\n\t"/* Skip the bottom (i0) elements */\
		"leaq	(%%r10,%%rbx,8),%%r10	\n\t"/* x+i0 */\
		"leaq	(%%r11,%%rbx,8),%%r11	\n\t"/* y+i0 */\
		"negq	%%rbx				\n\t"/* -i0 */\
		"addl	%[__i1],%%ebx		\n\t"/* ASM loop structured as for(j = i1; j != i0; j -= 8){...} */\
		"movd	%[__n] ,%%xmm14		\n\t"/* shift count - since imm-operands only take compile-time consts, this costs a vector register */\
		"movd	%[__nc],%%xmm15		\n\t"/* complement-shift count, 64-n */\
		"movq	    (%%r10),%%rax	\n\t"/* SHRD allows mem-ref only in DEST, so preload x[i+0] */\
	"loop_shrl_short:		\n\t"\
		"movdqa	0x20(%%r10),%%xmm0	\n\t"/* preload x[5,4] */\
		"movq	0x20(%%r10),%%rdx	\n\t"/* See "SSE2 write..." comment below for why need this */\
		/* i+0: */							/* i+4,5: x[5,4] in xmm0       */\
		"movq	0x08(%%r10),%%rsi	\n\t"	"movdqa	0x30(%%r10),%%xmm2	\n\t"\
		"shrdq	%%cl,%%rsi,%%rax	\n\t"	"movdqa %%xmm0,%%xmm1		\n\t"\
		"movq	%%rax,    (%%r11)	\n\t"	"shufpd $1,%%xmm2,%%xmm1	\n\t"/* SSE2-write of y[4] is why we */\
		/* i+1: */							"psrlq		%%xmm14,%%xmm0	\n\t"/* must preload x[4] into rdx for */\
		"movq	0x10(%%r10),%%rax	\n\t"	"psllq		%%xmm15,%%xmm1	\n\t"/* lcol use - otherwise rcol would */\
		"shrdq	%%cl,%%rax,%%rsi	\n\t"	"paddq		%%xmm1 ,%%xmm0	\n\t"/* clobber x[4] when doing an */\
		"movq	%%rsi,0x08(%%r11)	\n\t"	"movdqu	%%xmm0,0x20(%%r11)	\n\t"/* in-place shift (x == y). */\
		/* i+2: */							/* i+6,7: x[7,6] in xmm2       */\
		"movq	0x18(%%r10),%%rsi	\n\t"	"movdqa	0x40(%%r10),%%xmm0	\n\t"/* load x[9,8] */\
		"shrdq	%%cl,%%rsi,%%rax	\n\t"	"movdqa %%xmm2,%%xmm1		\n\t"\
		"movq	%%rax,0x10(%%r11)	\n\t"	"shufpd $1,%%xmm0,%%xmm1	\n\t"\
		/* i+3: */							"psrlq		%%xmm14,%%xmm2	\n\t"\
		"movq	     %%rdx ,%%rax	\n\t"	"psllq		%%xmm15,%%xmm1	\n\t"\
		"shrdq	%%cl,%%rax,%%rsi	\n\t"	"paddq		%%xmm1 ,%%xmm2	\n\t"\
		"movq	%%rsi,0x18(%%r11)	\n\t"	"movdqu	%%xmm2,0x30(%%r11)	\n\t"\
		/* Increment array pointers by 8 words */\
		"addq	$0x40,%%r10			\n\t"\
		"addq	$0x40,%%r11			\n\t"\
		"movq	    (%%r10),%%rax	\n\t"/* preload x[i+8] (already in xmm0[0:63], but there is no qword analog of MOVD),
							at least none supported by my clang/gcc installs, due to 'movq' being assumed a 64-bit 'mov'. */\
	"subq	$8,%%rbx		\n\t"\
	"jnz loop_shrl_short	\n\t"/* loop end; continue is via jump-back if rbx != 0 */\
		:	/* outputs: none */\
		: [__x] "m" (x)	/* All inputs from memory addresses here */\
		 ,[__y] "m" (y)	\
		 ,[__i0] "m" (i0)	\
		 ,[__i1] "m" (i1)	\
		 ,[__n] "m" (nbits)	\
		 ,[__nc] "m" (m64bits)	\
		: "cc","memory","rax","rbx","rcx","rdx","rsi","r10","r11","xmm0","xmm1","xmm2","xmm14","xmm15"	/* Clobbered registers */\
		);

	#else	// YES_ASM

		// No need for i0 in this macro, since i0 = 0 guaranteed by 8-byte alignment assertion in non-SIMD case:
	__asm__ volatile (\
		"movq	%[__x],%%r10		\n\t"/*  Input array */\
		"movq	%[__y],%%r11		\n\t"/* Output array */\
		"movslq	%[__i1],%%rbx		\n\t"/* ASM loop structured as for(j = i1; j != 0; j -= 4){...} */\
		"movslq	%[__n],%%rcx		\n\t"/* shift count */\
		"movq	(%%r10),%%rax		\n\t"/* SHRD allows mem-ref only in DEST, so preload x[0] */\
	"loop_shrl_short:	\n\t"/* Since this non-SIMD asm-code may be active along with the SIMD, append '2' to the label */\
		/* i+0: */\
		"movq	0x08(%%r10),%%rsi	\n\t"/* load x[i+1] ... the si in rsi stands for 'shift-in' :) */\
		"shrdq	%%cl,%%rsi,%%rax	\n\t"/* (x[i+1],x[i])>>n */\
		"movq	%%rax,    (%%r11)	\n\t"/* Write output word */\
		/* i+1: */\
		"movq	0x10(%%r10),%%rax	\n\t"/* x[i+1] already in rsi, so swap roles of rax/rsi */\
		"shrdq	%%cl,%%rax,%%rsi	\n\t"\
		"movq	%%rsi,0x08(%%r11)	\n\t"\
		/* i+2: */\
		"movq	0x18(%%r10),%%rsi	\n\t"/* x[i+2] already in rax, so swap back to [i+0] form */\
		"shrdq	%%cl,%%rsi,%%rax	\n\t"\
		"movq	%%rax,0x10(%%r11)	\n\t"\
		/* i+3: */\
		"movq	0x20(%%r10),%%rax	\n\t"/* x[i+1] already in rsi, so swap back to [i+1] form */\
		"shrdq	%%cl,%%rax,%%rsi	\n\t"\
		"movq	%%rsi,0x18(%%r11)	\n\t"\
		/* Increment array pointers by 4 words */\
		"addq	$0x20,%%r10			\n\t"\
		"addq	$0x20,%%r11			\n\t"\
	"subq	$4,%%rbx		\n\t"\
	"jnz loop_shrl_short	\n\t"/* loop end; continue is via jump-back if rbx != 0 */\
		:	/* outputs: none */\
		: [__x] "m" (x)	/* All inputs from memory addresses here */\
		 ,[__y] "m" (y)	\
		 ,[__i1] "m" (i1)	\
		 ,[__n] "m" (nbits)	\
		: "cc","memory","rax","rbx","rcx","rsi","r10","r11"	/* Clobbered registers */\
		);

	#endif

	}	// if(use_asm)...

	// Full-vector processing loop if no ASM; high-end cleanup-loop if ASM:
	for(i = i1; i < len-1; i++) {
		y[i] = (x[i] >> nbits) + (x[i+1] << m64bits);
	}

	/* Most-significant element gets zeros shifted in from the left: */
	y[len-1] = (x[len-1] >> nbits);

  #if MI64_SHR1_DBG
	if(len < 1000) {
		if(hi64 != ref[len]) { printf("SHR1 Carryout mismatch: (y[%u] = %16llX) != (ref[%u] = %16llX)\n",len,hi64,len,ref[len]); ASSERT(HERE, 0, "Exiting!"); }
		if(!mi64_cmp_eq(y,ref,len)) { for(i = len-1; i >= 0; i--) { if(y[i] != ref[i]) { printf("(y[%u] = %16llX) != (ref[%u] = %16llX)\n",i,y[i],i,ref[i]); ASSERT(HERE, 0, "Exiting!"); } } }
	}
  #endif
	return hi64;
}

/*******************/
/* unsigned compare: these are all built from just two elemental functions: < and == */
#ifdef __CUDA_ARCH__
__device__
#endif
uint32	mi64_cmpult(const uint64 x[], const uint64 y[], uint32 len)
{
	uint32 i;
	// Need hard-assert here due to zero-element default compare:
	ASSERT(HERE, len != 0, "mi64_cmpult: zero-length array!");
	for(i = len-1; i !=0 ; i--)	/* Loop over all but the 0 elements while equality holds.... */
	{
		if(x[i] < y[i]) {
			return TRUE;
		} else if(x[i] > y[i]) {
			return FALSE;
		}
	}
	return x[0] < y[0];
}

#ifdef __CUDA_ARCH__
__device__
#endif
uint32	mi64_cmp_eq(const uint64 x[], const uint64 y[], uint32 len)
{
	uint32 i;
	// Allow for zero-length here with default return TRUE,
	// according to the convention that a zero-length mi64 object = 0:
	ASSERT(HERE, len != 0, "mi64_cmp_eq: zero-length array!");	//  allows us to catch zero-length cases in debug build & test
	for(i = 0; i < len; i++) {
		if(x[i] != y[i])
			return FALSE;
	}
	return TRUE;
}

#ifdef __CUDA_ARCH__
__device__
#endif
uint32	mi64_cmplt_scalar(const uint64 x[], uint64 a, uint32 len)
{
	ASSERT(HERE, len != 0, "zero-length array!");
	return ( (mi64_getlen(x, len) <= 1) && (x[0] < a) );
}

#ifdef __CUDA_ARCH__
__device__
#endif
uint32	mi64_cmpgt_scalar(const uint64 x[], uint64 a, uint32 len)
{
	ASSERT(HERE, len != 0, "zero-length array!");
	return ( (x[0] > a) || (mi64_getlen(x, len) > 1) );
}

#ifdef __CUDA_ARCH__
__device__
#endif
uint32	mi64_cmp_eq_scalar(const uint64 x[], uint64 a, uint32 len)
{
	ASSERT(HERE, len != 0, "mi64_cmp_eq_scalar: zero-length array!");
	return ( (x[0] == a) && (mi64_getlen(x+1, len-1) == 0) );
}

/*******************/

#ifdef __CUDA_ARCH__
__device__
#endif
uint32	mi64_popcount(const uint64 x[], uint32 len)
{
	uint32 i, retval = 0;
	for(i = 0; i < len; i++) {
		retval += popcount64(x[i]);
	}
	return retval;
}

/*******************/

// Remember, [i]th-bit index in arglist is *unit* offset, i.e. must be in [1,MAX_CORES]
#ifdef __CUDA_ARCH__
__device__
#endif
int	mi64_ith_set_bit(const uint64 x[], uint32 bit, uint32 len)
{
	int curr_pop,i,j,retval = 0;
	if(!len || !bit) return -1;
	ASSERT(HERE, bit <= (len<<6), "[bit]th-bit specifier out of range!");
	// Find the word in which the [bit]th set-bit occurs:
	for(i = 0; i < len; i++) {
		curr_pop = popcount64(x[i]);
		retval += curr_pop;	// At this point retval stores the popcount-to-date...
		// ... If that >= [bit], replace that with the location of the [bit]th set bit:
		if(retval >= bit) {
			retval -= curr_pop;	// Subtract pop of curr_word back off
			j = ith_set_bit64(x[i],bit-retval);
			return (i<<6) + j;
		}
	}
	return -1;
}

/*******************/

/* Returns number of trailing zeros of a base-2^64 multiword int x if x != 0, 0 otherwise. */
#ifdef __CUDA_ARCH__
__device__
#endif
uint32	mi64_trailz(const uint64 x[], uint32 len)
{
	uint32 i, tz = 0;
	ASSERT(HERE, len != 0, "mi64_trailz: zero-length array!");
	for(i = 0; i < len; i++, tz += 64) {
		if(x[i]) {
			return tz + trailz64(x[i]);
		}
	}
	return 0;
}

/*******************/

/* Returns number of leading zeros of a base-2^64 multiword int x, defaulting to len*64 if x == 0. */
#ifdef __CUDA_ARCH__
__device__
#endif
uint32	mi64_leadz(const uint64 x[], uint32 len)
{
	int i;	// Loop index signed here
	uint32 lz = 0;
	ASSERT(HERE, len != 0, "mi64_leadz: zero-length array!");
	for(i = len-1; i >= 0; i--, lz += 64) {
		if(x[i]) {
			return lz + leadz64(x[i]);
		}
	}
	return lz;
}

/*******************/

/* Pair of functions to extract leading 64 significant bits, and 128-most-significant-bits-starting-at-a-specified-bit-position.
Note that despite the similarity of their names these functions do slightly different things - mi64_extract_lead64()
starts at a given word of an mi64, *finds* the leading nonzero bit in that word or [if none there] in the remaining [len - 1] words.
mi64_extract_lead128() extracts the leading 128 bits from the length-len vector X, starting at the *specified* MSB position of
X[len-1] passed in leading-zeros form in (nshift), filling in any missing low-order bits in lead_x[] with zero.
*/

/* Extract leading 64 significant bits, filling in any missing low-order bits in the result with zero.
Returns:
	- Leading 64 bits in *result;
	- Bitlength of x[] ( = 64*len - mi64_leadz(x) ) in retval. If < 64 (i.e. only x[0] word, if any, is nonzero),
	  absval of difference between it and 64 equals number of bits "borrowed" to fill in missing low-order bits.
*/
#ifdef __CUDA_ARCH__
__device__
#endif
uint32 mi64_extract_lead64(const uint64 x[], uint32 len, uint64*result)
{
	uint32 i,nshift,nwshift,rembits;

	ASSERT(HERE, len != 0, "mi64_extract_lead64: zero-length array!");

	nshift = mi64_leadz(x, len);
	nwshift = (nshift >> 6);
	rembits = (nshift & 63);
	/* shift-word count may == len, but only if x[] = 0: */
	if(nwshift >= len) {
		ASSERT(HERE, nwshift == len, "mi64_extract_lead64: nwshift out of range!");
		ASSERT(HERE, mi64_iszero(x, len), "mi64_extract_lead64: expected zero-valued array!");
		*result = 0ull;
	} else {
		i = len-1-nwshift;
		if(rembits) {
			*result  = (x[i] << rembits);
			if(i) {
				*result += (x[i-1] >> (64-rembits));
			}
		} else {
			*result = x[i];
		}
	}
	return (len << 6) - nshift;
}

/* Convert mi64 to double, rounding lowest-order mantissa bit: */
double	mi64_cvt_double(const uint64 x[], uint32 len)
{
	uint64 itmp64, lead64, lead64_rnd;
	double retval;
	int pow2 = mi64_extract_lead64(x, len, &lead64);
	if(lead64 == 0ull) {
		return 0.0;
	}
	ASSERT(HERE,(lead64 >> 63) == 1ull, "mi64_cvt_double: lead64 lacks leftmost ones bit!");
	/*  round based on 1st neglected bit: */
	lead64_rnd = (lead64 >> 11) + ((lead64 >> 10) & 0x0000000000000001ull);
	/* exponent: */
	itmp64 = (((uint64)0x3FD + (uint64)pow2) << 52);
	/* Add in mantissa, with hidden bit made explicit, hence the 0x3FD (rather than 0x3FE) initializer */
	itmp64 += lead64_rnd;
	ASSERT(HERE, itmp64 > lead64_rnd , "mi64_cvt_double: Exponent overflows IEEE64 field");
	/* GCC bug: needed to add the explicit sign-check below, otherwise GCC 'optimizes' away the (*(double *)&itmp64): */
	retval = *(double *)&itmp64;
	if(retval < 0.0) {
		sprintf(cbuf, "rng_isaac_rand_double_norm_pos: lead64 = %16llx, itmp64 = %16llx, retval = %lf not in [0,1]!\n", lead64, itmp64, retval);
		ASSERT(HERE, 0, cbuf);
	}
	return retval;
}

/*	Extract leading 128 bits from the length-len vector X, starting at the
	designated MSB position of X[len-1] passed in leading-zeros form in (nshift), which is required to be < 64.

	RETURNS: the desired 128 bits, left-justified, in the 2-array lead_x.
	If X has fewer than 128 bits below nshift, i.e. if (len*64 - nshift) < 128,
	missing low-order bits in lead_x[] are filled in with zero.

	Visually we have this:

    [                          x[len-1]                             ] [                          x[len-2]                             ] [                          x[len-3] (if nshift > 0)             ]
     321098765432109876543210987654321098765432109876543210987654321   321098765432109876543210987654321098765432109876543210987654321   321098765432109876543210987654321098765432109876543210987654321
     |-------- nshift --------|                                                                                                                                    |--------- (64-nshift) -------------|
                               |-------------------- 128 bits to be copied into lead_x[], right-padded with zeros if (len*64 - nshift) < 128 ---------------------|
*/
#ifdef __CUDA_ARCH__
__device__
#endif
void mi64_extract_lead128(const uint64 x[], uint32 len, uint32 nshift, uint64 lead_x[])
{
	lead_x[0] = lead_x[1] = 0;

	ASSERT(HERE, len != 0, "mi64_extract_lead128: zero-length array!");
	ASSERT(HERE, nshift < 64, "mi64_extract_lead128: illegal nshift value!");

	/* Syntax reminder:
		MVBITS(from_integer,low_bit_of_from_integer,num_bits,to_integer,insert_bits_in_to_integer_starting_at_this_low_bit)
	*/
		mvbits64(x[len-1],0        ,64-nshift,&lead_x[1], nshift);	/* move nonzero bits of leading digit into high word (2) of 128-bit slot, left-justifying...*/

	if(len > 1) {
	  if(nshift)
	  {
		mvbits64(x[len-2],64-nshift,nshift   ,&lead_x[1], 0     );	/* if leading digit had any leftmost zero bits, fill low-order bits of high word with high bits of next digit...*/
	  }
		mvbits64(x[len-2],0        ,64-nshift,&lead_x[0], nshift);	/* move leading bits of (lenU-1)st digit into low word (1) of 128-bit slot, left-justifying...*/
	}

	if(len > 2) {
	  if(nshift)
	  {
		mvbits64(x[len-3],64-nshift,nshift   ,&lead_x[0], 0     );	/* if necessary, fill low-order bits of low word with high bits of (lenU-2)nd digit. */
	  }
	}
}

/*******************/

#ifdef __CUDA_ARCH__
__device__
#endif
uint32	mi64_iszero(const uint64 x[], uint32 len)
{
	return mi64_getlen(x, len) == 0;
}

/*******************/
/* Given a vector int x[] and a dimension (len), returns the actual length of x[] based on the position of the most-significant word.
If all zero words, returns 0.
*/
#ifdef __CUDA_ARCH__
__device__
#endif
uint32	mi64_getlen(const uint64 x[], uint32 len)
{
	int i;
	for(i = len-1; i >= 0; i--) {
		if(x[i]) {
			return i+1;
		}
	}
	return 0;
}

/*******************/
/* Clear high (nclear) words of a vector int x[start_word]. */
#ifdef __CUDA_ARCH__
__device__
#endif
void	mi64_setlen(uint64 x[], uint32 oldlen, uint32 newlen)
{
	if(newlen <= oldlen) return;
	mi64_clear(&x[oldlen], (newlen - oldlen));
}

/*******************/

// If pow2 pointer non-null, number of trailing zeros of input array returned in that:
#ifdef __CUDA_ARCH__
__device__
#endif
uint32	mi64_isPow2(const uint64 x[], uint32 len, uint32*pow2)
{
	int i,ntz;
	for(i = len-1; i >= 0; i--) {
		if(x[i]) {
			ntz = mi64_trailz(x,len);	if(pow2) *pow2 = ntz;
			return isPow2_64(x[i]) && (ntz >= (i << 6));
		}
	}
	// If input = 0, leadz = trailz = len*64:
	if(pow2) *pow2 = (len << 6);
	return 0;
}

/*******************/

/*
	Unsigned add of two base-2^64 vector ints X + Y.
	Any or all of X, Y and Z can point to the same array object.
	Return any exit carry - up to user to determine what to do if this is nonzero.
*/
// Non-unrolled slow reference version, useful for checking results of attempted speedups:
#ifdef __CUDA_ARCH__
__device__
#endif
uint64	mi64_add_ref(const uint64 x[], const uint64 y[], uint64 z[], uint32 len)
{
	uint32 i;
	uint64 tmp, cy = 0;
	ASSERT(HERE, len != 0, "mi64_add: zero-length array!");

	for(i = 0; i < len; i++) {
		tmp = x[i] + cy;
		cy  = (tmp < x[i]);
		tmp = tmp + y[i];
		cy += (tmp < y[i]);
		z[i] = tmp;
	}
	return cy;
}

// Non-unrolled slow version allowing nonzero carryin, useful for cleanup stage of SIMD mi64_add():
#ifdef __CUDA_ARCH__
__device__
#endif
uint64	mi64_add_cyin(const uint64 x[], const uint64 y[], uint64 z[], uint32 len, uint64 cyin)
{
	uint32 i;
	uint64 tmp, cy = cyin;
	for(i = 0; i < len; i++) {
		tmp = x[i] + cy;
		cy  = (tmp < x[i]);
		tmp = tmp + y[i];
		cy += (tmp < y[i]);
		z[i] = tmp;
	}
	return cy;
}

#ifndef YES_ASM

	// Non-unrolled slow reference version, useful for checking results of attempted speedups:
  #ifdef __CUDA_ARCH__
	__device__
  #endif
	uint64	mi64_add(const uint64 x[], const uint64 y[], uint64 z[], uint32 len)
	{
	#if 0

		// cycles per limb:
		// Core2: 7.60 cpl
		// SdyBr: 6.60
		uint32 i;
		uint64 tmp, cy = 0;
		ASSERT(HERE, len != 0, "mi64_add: zero-length array!");

		for(i = 0; i < len; i++) {
			tmp = x[i] + cy;
			cy  = (tmp < x[i]);
			tmp = tmp + y[i];
			cy += (tmp < y[i]);
			z[i] = tmp;
		}

	#elif 0	// 2-unrolled C loop gives a nice boost. (4-unrolled no better in generic C).

		// Core2: 6.56 cpl
		// SdyBr: 6.14
		uint64 tmp, cy, c2 = 0;
		uint32 i, odd = (len&1), len2 = len - odd;

		for(i = 0; i < len2; i++){
			tmp = x[i] + y[i];
			cy = (tmp < x[i]);
			z[i] = tmp + c2;
			cy += (tmp > z[i]);
			i++;

			tmp = x[i] + y[i];
			c2 = (tmp < x[i]);
			z[i] = tmp + cy;
			c2 += (tmp > z[i]);
		}

		if(odd) {
			tmp = x[i] + y[i];
			cy = (tmp < x[i]);
			z[i] = tmp + c2;
			cy += (tmp > z[i]);
		} else {
			cy = c2;
		}

	#elif 1	// Try 2-folded C loop:

		// Core2: 5.07 cpl
		// SdyBr: 4.13
		uint64 tmp,tm2, cy = 0, c2 = 0;
		uint32 i,j, odd = (len&1), len2 = len >> 1;
		j = len2;
		for(i = 0; i < len2; i++, j++){
			tmp = x[i] + cy;			tm2 = x[j] + c2;
			cy  = (tmp < x[i]);			c2  = (tm2 < x[j]);
			tmp = tmp + y[i];			tm2 = tm2 + y[j];
			cy += (tmp < y[i]);			c2 += (tm2 < y[j]);
			z[i] = tmp;					z[j] = tm2;
		}
	/*
	Sep 2015 Bugfix: Hit case with len = 3 and
		x[0] = 6216518070457578443		y[0] = 12230226003251973173
		x[1] = 16881888488052985758		y[1] = 1564855585656565857
		x[2] = 65307107850795			y[2] = 2051081684
	This gives [lcol] z[0] = 0 (cy = 1), [rcol] z[1] = 18446744073709551615 = 2^64-1 (c2 = 0).
	Adding z[1]+cy gives 0 and causes a ripple-carry into z[2] ... but z[2] has not yet been inited!
	Thus when we hit the ensuing odd-length high-word cleanup step the ripple carry stored in z[2] is lost.
	FIX: change the while(cy) below to while(cy && (i < (len-odd))) :
	*/
		// Propagate low-half carry into high half; note i = len2 at this point:
		while(cy && (i < (len-odd))) {
			tmp = z[i] + cy;
			cy  = (tmp < z[i]);
			z[i] = tmp;
			++i;
		}
		c2 += cy;	// In case cy rippled all the way through the high-half result
		// Take care of any leftover high words:
		if(odd) {
			i = len - 1;
			tmp = x[i] + y[i];
			cy = (tmp < x[i]);
			z[i] = tmp + c2;
			cy += (tmp > z[i]);
		} else {
			cy = c2;
		}

	#else

		#error No reachable mi64_add code found!

	#endif

		return cy;
	}

#else	// #ifdef YES_ASM:

	uint64	mi64_add(const uint64 x[], const uint64 y[], uint64 z[], uint32 len)
	{
	#if 0//def USE_AVX
		#error experimental-olny code ... still needs debugging! [Jul 2016 - carry into topmost word missing]
		// Hybrid int64 / sse2 -based impl of ?-folded adc loop
		// Unsigned quadword compare requires sse4.2 or greater, so for now wrap it in the USE_AVX flag
		//
		// *** NOTE: *** This code is much slower than the GPR/ADC-based version - use for advance
		// prototyping of AVX512, which will have an 8-quadword unsigned-compare instruction, VPCMPUQ.
		//
		uint32 i, odd = (len&1), len2 = len >> 1;
		uint64 tmp, cy = 0, c2 = 0;
	ASSERT(HERE, has_sse42() != 0, "This ASM requires SSE4.2, which is unavailable on this CPU!");
		if(len2) {
		/* x86_64 ASM implementation of the add/carry loop: */
		__asm__ volatile (\
			"movq	%[__x0],%%rax	\n\t"/* &x[0] */\
			"movq	%[__y0],%%rbx	\n\t"/* &y[0] */\
			"movq	%[__z0],%%rdx	\n\t"/* &z[0] */\
			"movslq	%[__len2], %%rcx	\n\t"/* ASM loop structured as for(j = len2; j != 0; --j){...} */\
			"xorq   %%rsi,%%rsi    \n\t"/* Index i into the 3 arrays (really a uint64-array pointer offset) */\
			"movq	%%rcx,%%rdi	\n\t"/* Copy of half-array-length in rdi */\
			"shlq	$3,%%rdi	\n\t"/* Index j = i+len2 in bytewise ptr-offset form */\
			"xorpd	%%xmm0,%%xmm0	\n\t"/* init cy|c2 = 0 ... In sse2 impl, carries stored in negated (bitmask) form, so subtract them. */\
			"pcmpeqq	%%xmm7,%%xmm7	\n\t"/* 0x11...11 */\
			"psllq		$63,%%xmm7		\n\t"/* 0x10...00, needed to XOR against for 64-bit unsigned compare emulation */\
		"0:	\n\t"/* loop: */\
			"movlpd	(%%rax,%%rsi),%%xmm1	\n\t"/* tmp.lo = x[i] */\
			"movhpd	(%%rax,%%rdi),%%xmm1	\n\t"/* tmp.hi = x[j] */\
			"movlpd	(%%rbx,%%rsi),%%xmm2	\n\t"/* xmm2.lo = y[i] */\
			"movhpd	(%%rbx,%%rdi),%%xmm2	\n\t"/* xmm2.hi = y[j] */\
			"paddq	%%xmm0,%%xmm1		\n\t"/* tmp += cy */\
			"movaps	%%xmm1,%%xmm3		\n\t"/* tmp copy */\
			"xorpd	%%xmm7,%%xmm0	\n\t"/* toggle high bit of cy */\
			"xorpd	%%xmm7,%%xmm3	\n\t"/* toggle high bit of tmp-copy */\
			"pcmpgtq	%%xmm3,%%xmm0	\n\t"/* bitmask: cy = (tmp < cy)? Compare impl as (cy > tmp)? */\
			"paddq	%%xmm2,%%xmm1		\n\t"/* tmp += y[i|j] */\
			"movaps	%%xmm1,%%xmm3		\n\t"/* tmp copy */\
			"xorpd	%%xmm7,%%xmm2	\n\t"/* toggle high bit of cy */\
			"xorpd	%%xmm7,%%xmm3	\n\t"/* toggle high bit of tmp-copy */\
			"pcmpgtq	%%xmm3,%%xmm2	\n\t"/* bitmask: c2 = (tmp < y[i|j])? Compare impl as (y[i|j] > tmp)? */\
			"movlpd	%%xmm1,(%%rdx,%%rsi)	\n\t"/* store z[i] */\
			"movhpd	%%xmm1,(%%rdx,%%rdi)	\n\t"/* store z[j] */\
			"paddq	%%xmm0,%%xmm2		\n\t"/* accumulate 2-step carry in xmm2*/\
			"xorpd	%%xmm0,%%xmm0	\n\t"/* zero the bits of xmm0 */\
			"psubq	%%xmm2,%%xmm0	\n\t"/* negate the bitmask-form carryout to yield true carry in xmm0 */\
			"leaq	0x8(%%rsi),%%rsi	\n\t"\
			"leaq	0x8(%%rdi),%%rdi	\n\t"\
		"decq	%%rcx \n\t"\
		"jnz 0b 	\n\t"/* loop1 end; continue is via jump-back if rcx != 0 */\
			"movq	%[__cy],%%rax	\n\t"\
			"movq	%[__c2],%%rbx	\n\t"\
			"movlpd	%%xmm0,(%%rax)	\n\t"/* store cy */\
			"movhpd	%%xmm0,(%%rbx)	\n\t"/* store c2 */\
			: 				 /* outputs: none */\
			: [__x0] "g" (x)	/* All inputs from memory/register here */\
			 ,[__y0] "g" (y)	\
			 ,[__z0] "g" (z)	\
			 ,[__len2] "g" (len2)	\
			 ,[__cy] "g" (&cy)	\
			 ,[__c2] "g" (&c2)	\
			: "cc","memory","rax","rbx","rcx","rdx","rsi","rdi","xmm0","xmm1","xmm2","xmm3","xmm7"	/* Clobbered registers */\
		);
		}	// endif(len2)
		// Propagate low-half carry into high half:
		i = len2;
		while(cy) {
			tmp = z[i] + cy;
			cy  = (tmp < z[i]);
			z[i] = tmp;
			++i;
		}
		c2 += cy;	// In case cy rippled all the way through the high-half result
		// Take care of any leftover high words:
		if(odd) {
			i = len - 1;
			tmp = x[i] + y[i];
			cy = (tmp < x[i]);
			z[i] = tmp + c2;
			cy += (tmp > z[i]);
		} else {
			cy = c2;
		}

	#elif 1
		// Core2: 5.04 cpl
		// SdyBr: 2.20; Haswell: 1.84
		// Jun 2016: bizarre ... GCC builds with opt > 0 on Haswell/Broadwell init this != 0 ...
		//  making static not a reliable workaround, so try put cy = 0 init on separate line from declaration:
		uint64 cy;
		cy = 0ull;	ASSERT(HERE, cy == 0, "Init (cy = 0) fails!");
		/* x86_64 ASM implementation of the add/carry loop: */
		__asm__ volatile (\
			"movq	%[__x0],%%rax	\n\t"/* &x[0] */\
			"movq	%[__y0],%%rbx	\n\t"/* &y[0] */\
			"movq	%[__z0],%%rdx	\n\t"/* &z[0] */\
			"movslq	%[__len], %%rcx	\n\t"/* ASM loop structured as for(j = len; j != 0; --j){...} */\
			"xorq   %%rsi, %%rsi    \n\t"/* Index into the 3 arrays (really a uint64-array pointer offset) */\
			"movq	%%rcx,%%rdi	\n\t"/* Copy of array-length in rdi */\
			"shrq	$2,%%rcx	\n\t"/* How many iterations thru the 4-way loop */\
			"andq	$3,%%rdi	\n\t"/* Clear CF. Prepare for middle of loop jump */\
			"jz     0f		\n\t"/* Woohoo! Perfect multiple of 4 */\
			"incq   %%rcx		\n\t"/* Compensate for jumping into the middle of the loop */\
			"decq   %%rdi		\n\t"/* residue 1? */\
			"jz	3f		\n\t"/* Just do 1 addition in the first partial */\
			"decq   %%rdi		\n\t"/* residue 2? */\
			"jz	2f		\n\t"\
			"jmp	1f		\n\t"/* residue 3 */\
		"0:					\n\t"\
			"movq	(%%rax,%%rsi),%%r8 	\n\t"\
			"adcq	(%%rbx,%%rsi),%%r8	\n\t"\
			"movq	%%r8,(%%rdx,%%rsi)	\n\t"\
			"leaq   0x8(%%rsi), %%rsi	\n\t"\
		"1:					\n\t"\
			"movq	(%%rax,%%rsi),%%r8 	\n\t"\
			"adcq	(%%rbx,%%rsi),%%r8	\n\t"\
			"movq	%%r8,(%%rdx,%%rsi)	\n\t"\
			"leaq   0x8(%%rsi), %%rsi	\n\t"\
		"2:					\n\t"\
			"movq	(%%rax,%%rsi),%%r8 	\n\t"\
			"adcq	(%%rbx,%%rsi),%%r8	\n\t"\
			"movq	%%r8,(%%rdx,%%rsi)	\n\t"\
			"leaq   0x8(%%rsi), %%rsi	\n\t"\
		"3:					\n\t"\
			"movq	(%%rax,%%rsi),%%r8 	\n\t"\
			"adcq	(%%rbx,%%rsi),%%r8	\n\t"\
			"movq	%%r8,(%%rdx,%%rsi)	\n\t"\
			"leaq   0x8(%%rsi), %%rsi	\n\t"\

		"decq	%%rcx \n\t"\
		"jnz 0b 	\n\t"/* loop1 end; continue is via jump-back if rcx != 0 */\

			"adcq	%%rcx,%[__cy]	\n\t"/* Carryout. RCX is guaranteed to be zero at this point */\
			: [__cy] "=m" (cy) /* outputs: cy */\
			: [__x0] "m" (x)	/* All inputs from memory/register here */\
			 ,[__y0] "m" (y)	\
			 ,[__z0] "m" (z)	\
			 ,[__len] "m" (len)	\
			: "cc","memory","rax","rbx","rcx","rdx","rsi","rdi","r8"	/* Clobbered registers */\
		);

	#elif defined(USE_AVX512)

		// AVX512 prototype code for 8-folded adc loop
		uint32 i, lrem = (len&7), len8 = len >> 3;
		uint64 tmp, cy = 0, c2 = 0;
	#error mi64_add: no AVX512 support yet!
	ASSERT(HERE, has_avx512() != 0, "This ASM requires AVX512, which is unavailable on this CPU!");
vpcmpuq
*** how to encode the base.offset data? ***
vpgatherqq	%%zmmM,%%zmmD[255]	// zmmM has base_addr and
		if(len8) {
		/* x86_64 ASM implementation of the add/carry loop: */
		__asm__ volatile (\
			"movq	%[__x0],%%rax	\n\t"/* &x[0] */\
			"movq	%[__y0],%%rbx	\n\t"/* &y[0] */\
			"movq	%[__z0],%%rdx	\n\t"/* &z[0] */\
			"movslq	%[__len8], %%rcx	\n\t"/* ASM loop structured as for(j = len8; j != 0; --j){...} */\
			"xorq   %%rsi,%%rsi    \n\t"/* Index i into the 3 arrays (really a uint64-array pointer offset) */\
			"movq	%%rcx,%%rdi	\n\t"/* Copy of len/8 in rdi */\
			"shlq	$3,%%rdi	\n\t"/* Index j = i+len8 in bytewise ptr-offset form */\
			"xorpd	%%xmm0,%%xmm0	\n\t"/* init cy|c2 = 0 ... In sse2 impl, carries stored in negated (bitmask) form, so subtract them. */\
		"0:	\n\t"/* loop: */\
			@movlpd	(%%rax,%%rsi),%%xmm1	\n\t@/* tmp.lo = x[i] */\
			@movhpd	(%%rax,%%rdi),%%xmm1	\n\t@/* tmp.hi = x[j] */\
			@movlpd	(%%rbx,%%rsi),%%xmm2	\n\t@/* xmm2.lo = y[i] */\
			@movhpd	(%%rbx,%%rdi),%%xmm2	\n\t@/* xmm2.hi = y[j] */\
			@paddq	%%xmm0,%%xmm1		\n\t@/* tmp += cy */\
			@movaps	%%xmm1,%%xmm3		\n\t@/* tmp copy */\
			@xorpd	%%xmm7,%%xmm0	\n\t@/* toggle high bit of cy */\
			@xorpd	%%xmm7,%%xmm3	\n\t@/* toggle high bit of tmp-copy */\
			@pcmpgtq	%%xmm3,%%xmm0	\n\t@/* bitmask: cy = (tmp < cy)? Compare impl as (cy > tmp)? */\
			@paddq	%%xmm2,%%xmm1		\n\t@/* tmp += y[i|j] */\
			@movaps	%%xmm1,%%xmm3		\n\t@/* tmp copy */\
			@xorpd	%%xmm7,%%xmm2	\n\t@/* toggle high bit of cy */\
			@xorpd	%%xmm7,%%xmm3	\n\t@/* toggle high bit of tmp-copy */\
			@pcmpgtq	%%xmm3,%%xmm2	\n\t@/* bitmask: c2 = (tmp < y[i|j])? Compare impl as (y[i|j] > tmp)? */\
			@movlpd	%%xmm1,(%%rdx,%%rsi)	\n\t@/* store z[i] */\
			@movhpd	%%xmm1,(%%rdx,%%rdi)	\n\t@/* store z[j] */\
			@paddq	%%xmm0,%%xmm2		\n\t@/* accumulate 2-step carry in xmm2*/\
			@xorpd	%%xmm0,%%xmm0	\n\t@/* zero the bits of xmm0 */\
			@psubq	%%xmm2,%%xmm0	\n\t@/* negate the bitmask-form carryout to yield true carry in xmm0 */\
			@leaq	0x8(%%rsi),%%rsi	\n\t@\
			@leaq	0x8(%%rdi),%%rdi	\n\t@\
		"decq	%%rcx \n\t"\
		"jnz 0b 	\n\t"/* loop1 end; continue is via jump-back if rcx != 0 */\
			"movq	%[__cy],%%rax	\n\t"\
			"movq	%[__c2],%%rbx	\n\t"\
			"movlpd	%%xmm0,(%%rax)	\n\t"/* store cy */\
			"movhpd	%%xmm0,(%%rbx)	\n\t"/* store c2 */\
			: 				 /* outputs: none */\
			: [__x0] "g" (x)	/* All inputs from memory/register here */\
			 ,[__y0] "g" (y)	\
			 ,[__z0] "g" (z)	\
			 ,[__len8] "g" (len8)	\
			 ,[__cy] "g" (&cy)	\
			 ,[__c2] "g" (&c2)	\
			: "cc","memory","rax","rbx","rcx","rdx","rsi","rdi","xmm0","xmm1","xmm2","xmm3","xmm7"	/* Clobbered registers */\
		);
		}	// endif(len8)
*** need up-to-7-element cleanup ***
		// Propagate low-half carry into high half:
		i = len8;
		while(cy) {
			tmp = z[i] + cy;
			cy  = (tmp < z[i]);
			z[i] = tmp;
			++i;
		}
		c2 += cy;	// In case cy rippled all the way through the high-half result
		// Take care of any leftover high words:
		if(odd) {
			i = len - 1;
			tmp = x[i] + y[i];
			cy = (tmp < x[i]);
			z[i] = tmp + c2;
			cy += (tmp > z[i]);
		} else {
			cy = c2;
		}

	#else

		#error No reachable mi64_add code found!

	#endif

		return cy;
	}

#endif


/*
	Unsigned sub of two base-2^64 vector ints X - Y.
	Any or all of X, Y and Z can point to the same array object.
	Return any exit borrow - up to user to determine what to do if this is nonzero.
*/
#ifdef __CUDA_ARCH__
__device__
#endif
uint64	mi64_sub(const uint64 x[], const uint64 y[], uint64 z[], uint32 len)
{
	uint32 i;
	uint64 tmp, tmp2, bw = 0;

	ASSERT(HERE, len != 0, "mi64_sub: zero-length array!");
	for(i = 0; i < len; i++) {
		tmp = x[i] - bw;
		bw  = (tmp > x[i]);
//bw  = ((uint64)tmp > (uint64)x[i]);
		ASSERT(HERE, bw == ((uint64)tmp > (uint64)x[i]), "mi64_sub: compiler using signed compare (tmp > x[i])!");
		/* Need an extra temp here due to asymmetry of subtract: */
		tmp2= tmp - y[i];
		bw += (tmp2 > tmp);
//bw += ((uint64)tmp2 > (uint64)tmp);
		ASSERT(HERE, (tmp2 > tmp) == ((uint64)tmp2 > (uint64)tmp), "mi64_sub: compiler using signed compare (tmp2 > tmp)!");
		z[i] = tmp2;
	}
	return bw;
}

// Same as mi64_sub but with an input borrow:
#ifdef __CUDA_ARCH__
__device__
#endif
uint64	mi64_sub_bwin(const uint64 x[], const uint64 y[], uint64 z[], uint32 len, uint64 bwin)
{
	uint32 i;
	uint64 tmp, tmp2, bw = bwin;

	ASSERT(HERE, len != 0, "mi64_sub: zero-length array!");
	for(i = 0; i < len; i++) {
		tmp = x[i] - bw;
		bw  = (tmp > x[i]);
//bw  = ((uint64)tmp > (uint64)x[i]);
		ASSERT(HERE, bw == ((uint64)tmp > (uint64)x[i]), "mi64_sub: compiler using signed compare (tmp > x[i])!");
		/* Need an extra temp here due to asymmetry of subtract: */
		tmp2= tmp - y[i];
		bw += (tmp2 > tmp);
//bw += ((uint64)tmp2 > (uint64)tmp);
		ASSERT(HERE, (tmp2 > tmp) == ((uint64)tmp2 > (uint64)tmp), "mi64_sub: compiler using signed compare (tmp2 > tmp)!");
		z[i] = tmp2;
	}
	return bw;
}

/*******************/

/* Arithmetic negation uses that -x = ~x + 1: */
#ifdef __CUDA_ARCH__
__device__
#endif
void	mi64_nega(const uint64 x[], uint64 y[], uint32 len)
{
	if(len) {
		mi64_negl(x,y,len);
		mi64_add_scalar(y,1,y,len);
	}
}

/* Logical negation: */
#ifdef __CUDA_ARCH__
__device__
#endif
void	mi64_negl(const uint64 x[], uint64 y[], uint32 len)
{
	uint32 i;
	for(i = 0; i < len; i++) {
		y[i] = ~x[i];
	}
}


/*******************/
/*
	Add of 64-bit scalar A to base-2^64 vector int X. Allows In-place addition.
	Return any exit carry - up to user to determine what to do if this is nonzero.
	(Typically one would store the return value in x[len] and then increment len.)
*/
#ifdef __CUDA_ARCH__
__device__
#endif
uint64	mi64_add_scalar(const uint64 x[], uint64 a, uint64 y[], uint32 len)
{
	uint32 i;
	uint64 cy = a;

	ASSERT(HERE, len != 0, "mi64_add_scalar: zero-length array!");
	if(x == y) {
		/* In-place: Only need to proceed until carry peters out: */
		for(i = 0; i < len; i++) {
			y[i] = x[i] + cy;
			cy = (y[i] < cy);
			if(!cy) {
				break;
			}
		}
	} else {
		for(i = 0; i < len; i++) {
			y[i] = x[i] + cy;
			cy = (y[i] < cy);
		}
	}
	return cy;
}

/*
	Sub of 64-bit scalar A from base-2^64 vector int X. Allows In-place subtraction.
	Return any exit borrow - up to user to determine what to do if this is nonzero.
*/
#ifdef __CUDA_ARCH__
__device__
#endif
uint64	mi64_sub_scalar(const uint64 x[], uint64 a, uint64 y[], uint32 len)
{
	uint32 i;
	uint64 bw = a, tmp;

	ASSERT(HERE, len != 0, "mi64_sub_scalar: zero-length array!");
	if(x == y) {
		/* In-place: Only need to proceed until borrow peters out: */
		for(i = 0; i < len; i++) {
			tmp = x[i] - bw;
			/*  Since x[i] and y[i] point to the same memloc, need an extra temp here due to asymmetry of subtract: */
			bw = (tmp > x[i]);
			y[i] = tmp;	// This is really assigning to x[], but saying so much gives "error: assignment of read-only location"
			if(!bw) {
				break;
			}
		}
	} else {
		for(i = 0; i < len; i++) {
			tmp = x[i] - bw;
			bw = (tmp > x[i]);
			y[i] = tmp;
		}
	}
	return bw;
}

/*******************/

/*
	Unsigned multiply base-2^64 vector int x[] by scalar A, returning result in y[].
	Return any exit carry, rather than automatically storing it in a hypothetical
	[len+1]st array slot - up to user to determine what to do if this is nonzero.
	(Typically one would store the return value in y[len] and then increment len.)
	This has the advantage that we need assume nothing about the allocated length
	of the x[] array within the function.

	There is no code to check for special values of a, e.g. 0 or 1.

	Allows in-place, i.e. x == y.
*/
#ifdef __CUDA_ARCH__
__device__
#endif
uint64	mi64_mul_scalar(const uint64 x[], uint64 a, uint64 y[], uint32 len)
{
	uint32 i = 0;
	uint64 lo, hi, cy = 0;

	uint32 lmod4 = (len&0x3), ihi = len - lmod4;
	for(; i < ihi; ++i)
	{
	#ifdef MUL_LOHI64_SUBROUTINE
		MUL_LOHI64(a, x[i],&lo,&hi);
	#else
		MUL_LOHI64(a, x[i], lo, hi);
	#endif
		y[i] = lo + cy;
		cy = hi + (y[i] < lo);
		i++;
	#ifdef MUL_LOHI64_SUBROUTINE
		MUL_LOHI64(a, x[i],&lo,&hi);
	#else
		MUL_LOHI64(a, x[i], lo, hi);
	#endif
		y[i] = lo + cy;
		cy = hi + (y[i] < lo);
		i++;
	#ifdef MUL_LOHI64_SUBROUTINE
		MUL_LOHI64(a, x[i],&lo,&hi);
	#else
		MUL_LOHI64(a, x[i], lo, hi);
	#endif
		y[i] = lo + cy;
		cy = hi + (y[i] < lo);
		i++;
	#ifdef MUL_LOHI64_SUBROUTINE
		MUL_LOHI64(a, x[i],&lo,&hi);
	#else
		MUL_LOHI64(a, x[i], lo, hi);
	#endif
		y[i] = lo + cy;
		cy = hi + (y[i] < lo);
	}
	// Cleanup loop for remaining terms:
	ASSERT(HERE, len != 0, "zero-length array!");
	for(; i < len; i++)
	{
	#ifdef MUL_LOHI64_SUBROUTINE
		MUL_LOHI64(a, x[i],&lo,&hi);
	#else
		MUL_LOHI64(a, x[i], lo, hi);
	#endif
		y[i] = lo + cy;
		/*
		A*x[i] is at most (2^64 - 1)^2, i.e. hi <= 2^64 - 2. Since the
		carry out of the low 64 bits of the 128-bit add is at most 1,
		cy + hi <= 2^64 - 1, hence we don't need to check for a carry there,
		only when adding the result to x[i+1] on the next pass through the loop:
		*/
		cy = hi + (y[i] < lo);
	}
	return cy;
}

/*******************/

/*
	Unsigned multiply (vector * scalar) and add result to vector, a*X + Y = Z .
	Return any exit carry, rather than automatically storing it in a hypothetical
	[len+1]st array slot - up to user to determine what to do if this is nonzero.

	There is no code to check for special values of a, e.g. 0 or 1.

	Allows in-place, i.e. x == y.
*/
#if MI64_DEBUG
	#define MI64_MSAV2	0
#endif
#ifdef __CUDA_ARCH__
__device__
#endif
uint64	mi64_mul_scalar_add_vec2(const uint64 x[], uint64 a, const uint64 y[], uint64 z[], uint32 len)
{
	uint64 cy;	// Jul 2016: Same GCC bug as detailed in mi64_add
	cy = 0ull;	ASSERT(HERE, cy == 0, "Init (cy = 0) fails!");
#if MI64_MSAV2
	uint64 *u = 0x0, *v = 0x0;
	uint64 c2;
	u = (uint64 *)calloc(len, sizeof(uint64));
	v = (uint64 *)calloc(len, sizeof(uint64));		memcpy(v,y,(len<<3));	// Save copy of x[]
	c2  = mi64_mul_scalar(x, a, u, len);
	c2 += mi64_add(u, y, u, len);
#endif

#ifndef YES_ASM	// Toggle for x86_64 inline ASM

	uint32 i = 0;
	uint64 lo, hi, tmp;	// Oddly, the tmp-involving sequence below runs faster than the tmp-less one used in the ASM
	uint32 lmod2 = (len&0x1), ihi = len - lmod2;	// Unlike mi64_mul_scalar, 2x-unrolled works best here
	for(; i < ihi; ++i)			// "Oddly" enough, using a for-loop for cleanup rather than if(odd) is best
	{
	#ifdef MUL_LOHI64_SUBROUTINE
		MUL_LOHI64(a, x[i],&lo,&hi);
	#else
		MUL_LOHI64(a, x[i], lo, hi);
	#endif
		tmp = lo + cy;
		cy	= hi + (tmp < lo);
		z[i] = tmp + y[i];
		cy += (z[i] < tmp);
		i++;
	#ifdef MUL_LOHI64_SUBROUTINE
		MUL_LOHI64(a, x[i],&lo,&hi);
	#else
		MUL_LOHI64(a, x[i], lo, hi);
	#endif
		tmp = lo + cy;
		cy	= hi + (tmp < lo);
		z[i] = tmp + y[i];
		cy += (z[i] < tmp);
	}
	// Cleanup loop for remaining terms:
	ASSERT(HERE, len != 0, "zero-length array!");
	for(; i < len; i++)
	{
	#ifdef MUL_LOHI64_SUBROUTINE
		MUL_LOHI64(a, x[i],&lo,&hi);
	#else
		MUL_LOHI64(a, x[i], lo, hi);
	#endif
		tmp = lo + cy;
		cy	= hi + (tmp < lo);
		z[i] = tmp + y[i];
		cy += (z[i] < tmp);
		/*
		A*x[i] is at most (2^64 - 1)^2, i.e. hi <= 2^64 - 2. Since the
		carry out of the low 64 bits of the 128-bit add is at most 1,
		cy + hi <= 2^64 - 1, hence we don't need to check for a carry there,
		only when adding the result to x[i+1] on the next pass through the loop:
		*/
	}

#else	// defined(YES_ASM)

	/* x86_64 ASM implementation of the a*X + Y = Z loop: */
	__asm__ volatile (\
		"movq	%[__a],%%rbx	\n\t"/* Constant scalar multiplier*/\
		"movq	%[__x0],%%r11	\n\t"/* &x[0] */\
		"movq	%[__y0],%%r12	\n\t"/* &y[0] */\
		"movq	%[__z0],%%r13	\n\t"/* &z[0] */\
		"movslq	%[__len], %%rcx	\n\t"/* ASM loop structured as for(j = len; j != 0; --j){...} */\
		"xorq	%%r9 , %%r9 	\n\t"/* Clear the '64-bit carry' */\
		"xorq	%%rsi, %%rsi	\n\t"/* Index into the 3 arrays (really a uint64-array pointer offset) */\
		"movq	%%rcx,%%rdi	\n\t"/* Copy of array-length in rdi */\
		"shrq	$2,%%rcx	\n\t"/* How many iterations thru the 4-way loop */\
		"andq	$3,%%rdi	\n\t"/* Clear CF. Prepare for middle of loop jump */\
		"jz	 0f		\n\t"/* Woohoo! Perfect multiple of 4 */\
		"incq	%%rcx		\n\t"/* Compensate for jumping into the middle of the loop */\
		"decq	%%rdi		\n\t"/* residue 1? */\
		"jz	3f		\n\t"/* Just do 1 addition in the first partial */\
		"decq	%%rdi		\n\t"/* residue 2? */\
		"jz	2f		\n\t"\
		"jmp	1f		\n\t"/* residue 3 */\
	"0:					\n\t"\
		"movq	(%%r11,%%rsi),%%rax \n\t"/* x[i] */\
		"mulq	%%rbx				\n\t"/* MUL_LOHI64(a, x[i]); lo:hi in rax:rdx; CF set according to (hi == 0) */\
		"addq	(%%r12,%%rsi),%%rax	\n\t"/* z[i] = lo + y[i], and set CF if overflow */\
		"adcq	$0   ,%%rdx 		\n\t"\
		"adcq	%%r9 ,%%rax			\n\t"/* lo += 64-bit carryin from pvs word, and set CF if overflow */\
		"adcq	$0   ,%%rdx 		\n\t"/* Carryout into next-higher word = hi + CF */\
		"movq	%%rdx,%%r9 			\n\t"/* Save carryout in r9 for next-higher word computation */\
		"movq	%%rax,(%%r13,%%rsi)	\n\t"/* Store z[i] */\
		"leaq	0x8(%%rsi), %%rsi	\n\t"\
	"1:					\n\t"\
		"movq	(%%r11,%%rsi),%%rax \n\t"\
		"mulq	%%rbx				\n\t"\
		"addq	(%%r12,%%rsi),%%rax	\n\t"\
		"adcq	$0   ,%%rdx 		\n\t"\
		"adcq	%%r9 ,%%rax			\n\t"\
		"adcq	$0   ,%%rdx 		\n\t"\
		"movq	%%rdx,%%r9 			\n\t"\
		"movq	%%rax,(%%r13,%%rsi)	\n\t"\
		"leaq	0x8(%%rsi), %%rsi	\n\t"\
	"2:					\n\t"\
		"movq	(%%r11,%%rsi),%%rax \n\t"\
		"mulq	%%rbx				\n\t"\
		"addq	(%%r12,%%rsi),%%rax	\n\t"\
		"adcq	$0   ,%%rdx 		\n\t"\
		"adcq	%%r9 ,%%rax			\n\t"\
		"adcq	$0   ,%%rdx 		\n\t"\
		"movq	%%rdx,%%r9 			\n\t"\
		"movq	%%rax,(%%r13,%%rsi)	\n\t"\
		"leaq	0x8(%%rsi), %%rsi	\n\t"\
	"3:					\n\t"\
		"movq	(%%r11,%%rsi),%%rax \n\t"\
		"mulq	%%rbx				\n\t"\
		"addq	(%%r12,%%rsi),%%rax	\n\t"\
		"adcq	$0   ,%%rdx 		\n\t"\
		"adcq	%%r9 ,%%rax			\n\t"\
		"adcq	$0   ,%%rdx 		\n\t"\
		"movq	%%rdx,%%r9 			\n\t"\
		"movq	%%rax,(%%r13,%%rsi)	\n\t"\
		"leaq	0x8(%%rsi), %%rsi	\n\t"\
	"decq	%%rcx \n\t"\
	"jnz 0b 	\n\t"/* loop1 end; continue is via jump-back if rcx != 0 */\
		"adcq	%%r9 ,%[__cy]	\n\t"/* Carryout. */\
		: [__cy] "=m" (cy) /* outputs: cy */\
		: [__x0] "g" (x)	/* All inputs from memory/register here */\
		 ,[__y0] "g" (y)	\
		 ,[__z0] "g" (z)	\
		 ,[__a] "g" (a)	\
		 ,[__len] "g" (len)	\
		: "cc","memory","rax","rbx","rcx","rdx","rsi","rdi","r9","r11","r12","r13"	/* Clobbered registers */\
	);

#endif	// YES_ASM

#if MI64_MSAV2
	if(!mi64_cmp_eq(u,z,len) || (cy != c2)) {
		for(i = 0; i < len; i++) {
		//	if(u[i] != z[i])
			printf("i = %u Error: U = %20llu, Z = %20llu, Diff = %20lld\n",i,u[i],z[i],(int64)(u[i]-z[i]) );
		}
		if(cy != c2) printf("Carry Error: c2 = %20llu, cy = %20llu, Diff = %20lld\n",c2,cy,(int64)(c2-cy) );
		ASSERT(HERE, 0, "mi64_add ASM result incorrect!");
	}
	free((void *)u); u = 0x0;
	free((void *)v); v = 0x0;
#endif
	return cy;
}

/*******************/

/*
	Unsigned multiply of base-2^64 vector ints X * Y, having respective lengths lenX, lenY.
	Result is returned in vector int Z. For simplicity, NEITHER X NOR Y MAY OVERLAP Z,
	though X and Y may point to the same (or to overlapping) memory addresses.
	An added restriction is that Z must be large enough to store the result of the multiply,
	i.e. has at least (lenX + lenY) allocated 64-bit integer elements.

	The routine first find the larger of the 2 input vectors (in terms of number of elements,
	i.e. lenA = max(lenX, lenY), lenB = min(lenX, lenY) and pointers A and B set to point to
	the corresponding input vector. It then performs a grammar-school multiply in (lenB) passes,
	during each of which the entire vector A is multiplied by the (pass)th 64-bit scalar element
	of vector B and the resulting partial product shifted and added into vector Z.

	There is no code to check for special values of the inputs, e.g. 0 or 1; However, if one of
	the inputs is specified to have zero length, lenZ is set to zero and an immediate return effected.

	RETURNS:
		- product in vector Z;
		- actual product length via pointer *lenZ;
*/
#ifdef __CUDA_ARCH__
__device__	/* GPU version needs to supply its own scratch array: */
void	mi64_mul_vector(const uint64 x[], uint32 lenX, const uint64 y[], uint32 lenY, uint64 z[], uint32 *lenZ, uint64 u[])
#else
void	mi64_mul_vector(const uint64 x[], uint32 lenX, const uint64 y[], uint32 lenY, uint64 z[], uint32 *lenZ)
#endif
{
	uint32 i, j, lenA, lenB;
	const uint64 *A, *B;
  #ifndef __CUDA_ARCH__
	/* Scratch array for storing intermediate scalar*vector products: */
	static uint64 *u = 0x0;
	static uint32 dimU = 0;
  #endif
	ASSERT(HERE, x && y && z, "Null array x/y/z!");
	ASSERT(HERE, lenX != 0, "zero-length X-array!");
	ASSERT(HERE, lenY != 0, "zero-length Y-array!");
	ASSERT(HERE, x != z, "X and Z point to same array object!");
	ASSERT(HERE, y != z, "Y and Z point to same array object!");
	ASSERT(HERE, lenZ != 0x0, "Null lenZ pointer!");

	/* Init z[] = 0: */
	for(i = 0; i < lenX + lenY; i++) {
		z[i] = 0;
	}

	/* Find larger of the 2 inputs (in terms of actual length, not nominal length: */
	lenX = mi64_getlen(x, lenX);
	lenY = mi64_getlen(y, lenY);

	if(lenX >= lenY) {
		lenA = lenX;	lenB = lenY;
		A = x;			B = y;
	} else {
		lenA = lenY;	lenB = lenX;
		A = y;			B = x;
	}
	/* If the actual length of the smaller argument = 0, nothing to do, return 0: */
	if(lenB == 0) {
		*lenZ = 0;
		return;
	} else
		*lenZ = lenA;

	// Specialized MUL macros for equal-small-length inputs:
	if(lenA == lenB && lenA == 4) {
		*lenZ = lenA + lenB;
		mi64_mul_4word(x,y,z);
	} else {
	#ifndef __CUDA_ARCH__
		// Does scratch array need allocating or reallocating? (Use realloc for both cases):
		if(dimU < (lenA+1)) {
			dimU = 2*(lenA+1);
			// Alloc 2x the immediately-needed to avoid excessive reallocs if neededsize increases incrementally
			u = (uint64 *)realloc(u, 2*(lenA+1)*sizeof(uint64));
		}
	#endif
		/* Loop over remaining (lenB-1) elements of B[], multiplying A by each, and
		using u[] as a scratch array to store B[i]*A[] prior to adding to z[]: */
		for(j = 0; j < lenB; j++) {
			u[lenA] = mi64_mul_scalar(A, B[j], u, lenA);
			/* Add j-word-left-shifted u[] to z[]: */
			z[lenA+j] = u[lenA] + mi64_add(&z[j], u, &z[j], lenA);
		}
		*lenZ += lenB;
	}
	/* Return actual length of result vector in the function argument, so that if one or
	more leading terms of the result is zero, caller can adjust vector length accordingly:
	*/
	*lenZ = mi64_getlen(z, *lenZ);
	ASSERT(HERE, *lenZ <= lenA + lenB, "*lenZ > (lenA + lenB)!");
}

/* Squaring-specialized version of above. By way of example, consider a length-10 input vector and
examine full-double-width square in term-by-term fashion, powers of the base b = 2^64 in left col:

b^n:	Coefficient (unnormalized)
----	--------------------------
n = 0	x0^2
1		x0.x1.2
2		x0.x2.2 + x1^2
3		x0.x3.2 + x1.x2.2
4		x0.x4.2 + x1.x3.2 + x2^2
5		x0.x5.2 + x1.x4.2 + x2.x3.2
6		x0.x6.2 + x1.x5.2 + x2.x4.2 + x3^2
7		x0.x7.2 + x1.x6.2 + x2.x5.2 + x3.x4.2
8		x0.x8.2 + x1.x7.2 + x2.x6.2 + x3.x5.2 + x4^2
9		x0.x9.2 + x1.x8.2 + x2.x7.2 + x3.x6.2 + x4.x5.2
10		          x1.x9.2 + x2.x8.2 + x3.x7.2 + x4.x6.2 + x5^2
11		                    x2.x9.2 + x3.x8.2 + x4.x7.2 + x5.x6.2
12		                              x3.x9.2 + x4.x8.2 + x5.x7.2 + x6^2
13		                                        x4.x9.2 + x5.x8.2 + x6.x7.2
14		                                                  x5.x9.2 + x6.x8.2 + x7^2
15		                                                            x6.x9.2 + x7.x8.2
16		                                                                      x7.x9.2 + x8^2
17		                                                                                x8.x9.2
18		                                                                                   x9^2
19		                                                                                      0

We can thus first compute the "off-diagonal" terms:

b^n:	Coefficient (unnormalized)
----	--------------------------
n = 0
1		x0.x1
2		x0.x2
3		x0.x3 + x1.x2
4		x0.x4 + x1.x3
5		x0.x5 + x1.x4 + x2.x3
6		x0.x6 + x1.x5 + x2.x4
7		x0.x7 + x1.x6 + x2.x5 + x3.x4
8		x0.x8 + x1.x7 + x2.x6 + x3.x5
9		x0.x9 + x1.x8 + x2.x7 + x3.x6 + x4.x5
10		        x1.x9 + x2.x8 + x3.x7 + x4.x6
11		                x2.x9 + x3.x8 + x4.x7 + x5.x6
12		                        x3.x9 + x4.x8 + x5.x7
13		                                x4.x9 + x5.x8 + x6.x7
14		                                        x5.x9 + x6.x8
15		                                                x6.x9 + x7.x8
16		                                                        x7.x9
17		                                                                x8.x9
18		                                                                    0
19		                                                                    0

Which we can do as set of ever-shorter scalar-vector muls, with intermediate vector add-with-carry:

	memset(z+len, 0ull, (len<<3));	// Clear upper half of output vector
	// Initial scalar-vector mul can be used to populate low [len+1] slots of output vector:
	z[len] = mi64_mul_scalar(x+1, x[0], z+1, len-1);
	// Use loop to take care of the rest:
	z[len+1] = mi64_mul_scalar(x+2, x[1], u, len-2);	ASSERT(0 == mi64_add(z+ 3, u, z+ 3, len-2));
	z[len+2] = mi64_mul_scalar(x+3, x[2], u, len-3);	ASSERT(0 == mi64_add(z+ 5, u, z+ 5, len-3));
	z[len+3] = mi64_mul_scalar(x+4, x[3], u, len-4);	ASSERT(0 == mi64_add(z+ 7, u, z+ 7, len-4));
	z[len+4] = mi64_mul_scalar(x+5, x[4], u, len-5);	ASSERT(0 == mi64_add(z+11, u, z+11, len-5));
	z[len+5] = mi64_mul_scalar(x+6, x[5], u, len-6);	ASSERT(0 == mi64_add(z+13, u, z+13, len-6));
	z[len+6] = mi64_mul_scalar(x+7, x[6], u, len-7);	ASSERT(0 == mi64_add(z+15, u, z+15, len-7));
	z[len+7] = mi64_mul_scalar(x+8, x[7], u, len-8);	ASSERT(0 == mi64_add(z+17, u, z+17, len-8));
	z[len+8] = mi64_mul_scalar(x+9, x[8], u, len-9);	ASSERT(0 == mi64_add(z+19, u, z+19, len-9));

We then double the result, and compute the square terms and add them in. This can be done efficiently
by making a copy of the z-vector resulting from the above (the one needing double-and-add-to-square-terms),
initing a vector containing the (nonoverlapping) square terms, and then doing a full-length vector-add.
*/
#if MI64_DEBUG
	#define MI64_SQR_DBG	0	// Set nonzero to enable debug-print in the function below
#endif

#ifdef __CUDA_ARCH__
__device__
void	(const uint64 x[], uint64 z[], uint32 len, uint64 u[])
#else
void	mi64_sqr_vector(const uint64 x[], uint64 z[], uint32 len)
#endif
{
	const char func[] = "mi64_sqr_vector";
  #if MI64_SQR_DBG
	uint32 dbg = len == 10 && !x[0] && !x[1] && !x[2] && x[3] == 1073741824ull;// && STREQ(&s0[convert_mi64_base10_char(s0, x, len, 0)], "0");
	if(dbg)
		printf("%s: len = %u\n",func,len);
  #endif
	uint32 i, j, len8 = (len<<3);
	uint64 sgn, cy;
  #ifndef __CUDA_ARCH__
	/* Scratch array for storing intermediate scalar*vector products: */
	static uint64 *u = 0x0;
	static uint32 dimU = 0;
	// Does scratch array need allocating or reallocating? (Use realloc for both cases):
	if(dimU < (len+1)) {
		dimU = 2*(len+1);
	  #if MI64_SQR_DBG
		if(dbg) printf("realloc to dimU = %u\n",dimU);
	  #endif
		// Alloc 2x the immediately-needed to avoid excessive reallocs if neededsize increases incrementally
		u = (uint64 *)realloc(u, 4* len   *sizeof(uint64));
	}
  #endif
	ASSERT(HERE, z != x, "Input and output arrays must be distinct!");
	ASSERT(HERE, len != 0, "zero-length X-array!");

	memset(z, 0ull,(len8<<1));	// Clear z[0,...,2*len-1]

	// Initial scalar-vector mul can be used to populate low [len+1] slots of output vector:
	if(len > 1) {	// This ensures nonzero-length vec-arg to mi64_mul_scalar
		z[len] = mi64_mul_scalar(x+1, x[0], z+1, len-1);
	  #if MI64_SQR_DBG
		if(dbg) {
			printf("x0*x[1...n-1] = %llu * %s...\n",x[0],&cbuf[convert_mi64_base10_char(cbuf,x+1,len-1,0)]);
			printf("            ... -> z = %s...\n",&cbuf[convert_mi64_base10_char(cbuf,z,2*len,0)]);
		}
	  #endif
		// Use loop to take care of the rest, i starting at 2 ensures no 0-length vecs:
		for(i = 2, j = 1; i < len; j = i++)	// j stores i-1 throughout loop
		{
			z[len+j] = mi64_mul_scalar_add_vec2(x+i, x[j], z+i+j, z+i+j, len-i);
		  #if MI64_SQR_DBG
			if(dbg) {
				printf("x%u*x[%u...n-1] = %llu * %s...\n",j,i,x[j],&cbuf[convert_mi64_base10_char(cbuf,x+i,len-i,0)]);
				printf("          ... += z = %s...\n",&cbuf[convert_mi64_base10_char(cbuf,z,2*len,0)]);
			}
		  #endif
		}
	}
	// Init vector containing the (nonoverlapping) square terms at same time we do doubling of the cross-terms vector.
	// We avoid the need for add-with-carry by doing the doubling via left-shift of the current word 1 bit
	// and copying the (saved) high bit of the next-lower word into the thus-vacated low bit.
	cy = 0;
	for(i = j = 0; i < len; ++i, j +=2)
	{
		sgn = (int64)z[j  ] < 0;
		z[j  ] = (z[j  ] << 1) + cy;	cy = sgn;
	#ifdef MUL_LOHI64_SUBROUTINE
		SQR_LOHI64(x[i],u+j ,u+j+1 );
	#else
		SQR_LOHI64(x[i],u[j],u[j+1]);
	#endif
		sgn = (int64)z[j+1] < 0;
		z[j+1] = (z[j+1] << 1) + cy;	cy = sgn;
	}
  #if MI64_SQR_DBG
	if(dbg) {
		printf("Squared diagonal = %s...\n",&cbuf[convert_mi64_base10_char(cbuf,u,2*len-1,0)]);
		printf("         ... 2*z = %s...\n",&cbuf[convert_mi64_base10_char(cbuf,z,2*len-1,0)]);
	}
  #endif
	// then do a full-length vector-add to obtain the result:
	mi64_add(z,u,z,2*len);
  #if MI64_SQR_DBG
	if(dbg)
		printf("Output = %s\n",&cbuf[convert_mi64_base10_char(cbuf,z,2*len-1,0)]);
  #endif
}
/* Initial Debug:
																			error z[j]-v[j]
																			---------------
(gdb) p v[0]	=               515524		z[0]	=               514544	-980
(gdb) p v[1]	=                    0		z[1]	=                    1	+1
(gdb) p v[2]	=                    0		z[2]	=                    0
(gdb) p v[3]	=   695282096289087488		z[3]	=   695282096289087487	-1
(gdb) p v[4]	=  6561739352485612000		z[4]	=  6561739352485612000
(gdb) p v[5]	=  4280339908740614450		z[5]	=  4280339908740614450
(gdb) p v[6]	=  4655141245927292619		z[6]	=  4655141245927292618	-1
(gdb) p v[7]	= 16391347641773540250		z[7]	= 16391347641773540249	-1
(gdb) p v[8]	=  9660163612446427001		z[8]	=  9660163612446427001
(gdb) p v[9]	=  6118265205031034628		z[9]	=  6118265205031034627	-1
(gdb) p v[10]	=   949770555345385036		z[10]	=   949770555345385035	-1
(gdb) p v[11]	=  6888472502951621128		z[11]	=  6888472502951621128
(gdb) p v[12]	= 10263162476022843315		z[12]	= 10263162476022843315
(gdb) p v[13]	=  4557757262155789608		z[13]	=  4557757262155789608
(gdb) p v[14]	=  3367728947229070195		z[14]	=  3367728947229070195
(gdb) p v[15]	= 17118160634115880599		z[15]	= 17118160634115880599
(gdb) p v[16]	=  1896777173959327369		z[16]	=  1896777173959327369
(gdb) p v[17]	= 12756822023939931388		z[17]	= 12756822023939931387	-1
(gdb) p v[18]	= 14488044506087195387		z[18]	= 14488044506087195387
(gdb) p v[19]	=               129184		z[19]	=               129184
(gdb)
*/
/* Low and high-half mul allow in-place: */
#ifdef __CUDA_ARCH__
__device__	/* GPU version needs to supply its own scratch array: */
void	mi64_mul_vector_lo_half	(const uint64 x[], const uint64 y[], uint64 z[], uint32 len, uint64 u[])
#else
void	mi64_mul_vector_lo_half	(const uint64 x[], const uint64 y[], uint64 z[], uint32 len)
#endif
{
	uint32 j;
  #ifndef __CUDA_ARCH__
	/* Scratch array for storing intermediate scalar*vector products: */
	static uint64 *u = 0x0;
	static uint32 dimU = 0;
	ASSERT(HERE, x && y && z, "Null array pointer!");
	ASSERT(HERE, len != 0, "zero-length X-array!");
	// Does scratch array need allocating or reallocating? (Use realloc for both cases):
	if(dimU < (len+1)) {
		dimU = 2*(len+1);
		// Alloc 2x the immediately-needed to avoid excessive reallocs if neededsize increases incrementally
		u = (uint64 *)realloc(u, 2*(len+1)*sizeof(uint64));	// NB: realloc leaves newly-alloc'ed size fraction uninited
	}
	memset(u, 0ull, (len<<4));	// Accumulator u[] needs to be cleared each time
  #endif
	// Specialized MUL macros for equal-small-length inputs:
	if(len == 4) {
		mi64_mul_lo_half_4word(x,y,z);
	} else {
		/* Loop over the elements of y[], multiplying x[] by each, and
		using u[] as a scratch array to store x[]*y[j] prior to adding to z[].

		For the high-half version, only want terms x[i]*y[j] with (i + j) >= len,
		plus the high halves of the (i + j) = len-1 terms for the requisite carryins.
		*/
		for(j = 0; j < len; j++) {
			if(y[j] == 0)
				continue;
			mi64_mul_scalar_add_vec2(x, y[j], u+j, u+j, len-j);
		}
		/* Copy u[0:len-1] into z[0:len-1]: */
		memcpy(z,u,(len<<3));
	}
}

#define MI64_MULHI_DBG	0	// Set nonzero to enable debug-print in the mi64 mi64_mul_vector_hi_half function below

#ifdef __CUDA_ARCH__
__device__	/* GPU version needs to supply its own pair of scratch arrays: */
void	mi64_mul_vector_hi_half	(const uint64 x[], const uint64 y[], uint64 z[], uint32 len, uint64 u[], uint64 v[])
#else
void	mi64_mul_vector_hi_half	(const uint64 x[], const uint64 y[], uint64 z[], uint32 len)
#endif
{
#if MI64_MULHI_DBG
	uint32 i,dbg = 1;
#endif
	uint32 j;
	/* Scratch array for storing intermediate scalar*vector products: */
  #ifndef __CUDA_ARCH__
	static uint64 *u = 0x0, *v = 0x0;
	static uint32 dimU = 0;
	// Does scratch array need allocating or reallocating? (Use realloc for both cases):
	if(dimU < (len+1)) {
	#if MI64_MULHI_DBG
		if(dbg) { printf("mi64_mul_vector_hi_half: allocs with dimU = %d, len+1 = %d\n",dimU,len+1); }
	#endif
		dimU = 2*(len+1);
		// Alloc 2x the immediately-needed to avoid excessive reallocs if neededsize increases incrementally
		u = (uint64 *)realloc(u, 2*(len+1)*sizeof(uint64));
		v = (uint64 *)realloc(v, 4* len   *sizeof(uint64));
	}
	memset(v, 0ull, (len<<4));	// Accumulator v[] needs to be cleared each time
  #endif
	ASSERT(HERE, len != 0, "zero-length X-array!");

	/* Loop over the elements of y[], multiplying x[] by each, and
	using u[] as a scratch array to store x[]*y[j] prior to adding to z[].

	For the high-half version, only want terms x[i]*y[j] with (i + j) >= len,
	plus the high halves of the (i + j) = len-1 terms for the requisite carryins.
	*/
#if MI64_MULHI_DBG
	if(dbg) { printf("mi64_mul_vector_hi_half: X = %s\n", &cbuf[convert_mi64_base10_char(cbuf, x, len, 0)]); }
	if(dbg) { printf("mi64_mul_vector_hi_half: Y = %s\n", &cbuf[convert_mi64_base10_char(cbuf, y, len, 0)]); }
#endif
	for(j = 0; j < len; j++)
	{
		if(y[j] == 0)
			continue;
		u[len] = mi64_mul_scalar(x, y[j], u, len);
	#if MI64_MULHI_DBG
		if(dbg) { printf("mi64_mul_vector_hi_half: j = %d, cy = %20llu, U = %s\n",j,u[len], &cbuf[convert_mi64_base10_char(cbuf, u, len+1, 0)]); }
	#endif
		/* Add j-word-left-shifted u[] to v[]: */
		/*** 11/2013: Simply could not get this to work using any opt-level > 0 under debian/gcc4.6 ***/
		// [it works fine at all opts under OSX/gcc4.2] using the x86_64 asm-loop in mi64_add, so use the pure-C mi64_add_ref here:
		v[len+j] = u[len] + mi64_add(&v[j], u, &v[j], len);
	#if MI64_MULHI_DBG
		if(dbg) { printf("mi64_mul_vector_hi_half: j = %d, V = %s\n",j, &cbuf[convert_mi64_base10_char(cbuf, v, len+j+1, 0)]); }
		if(dbg) {
			for(i=0;i<=len;++i) {
				printf("v[%2d] = %20llu\n",i+j,v[i+j]);
			}
		}
	#endif
	}
	/* Copy v[len:2*len-1] into z[0:len-1]: */
#if MI64_MULHI_DBG
	if(dbg) { printf("mi64_mul_vector_hi_half: Zin  = %s\n", &cbuf[convert_mi64_base10_char(cbuf, z, len, 0)]); }
#endif
	memcpy(z,v+len,(len<<3));
#if MI64_MULHI_DBG
	if(dbg) { printf("mi64_mul_vector_hi_half: Zout = %s\n", &cbuf[convert_mi64_base10_char(cbuf, z, len, 0)]); exit(0);}
#endif
}


// Fast version of above, which only computes hi-half product terms and the additional ones
// needed to approximate the exact carryin to the hi half by its upper 64 bits.
// This fails for certain well-known input types, e.g. when x and y have a significant fraction
// of their 64-bit 'digts' very close in value to 2^64-1:

#if MI64_DEBUG
	#define DEBUG_HI_MUL	0	// Set nonzero to enable debug of mi64_mul_vector_hi_fast
#endif

/* 4/22/2012: This rhombus-truncated fast version of the above routine needs further work to
implement an error-correction for the low-half carryout. Here are notes related to an example
illustrating this.

Inputs are in the conetxt of emulated 640-bit twos-complement integer arithmetic:

	x = 2^607-1 ==> x[0,9] = [2^64-1,...,2^64-1,2^31-1]
	y[0,9] = [0,0,0,0,0,0,2^64-2^60,2^64-1,2^64-1,2^64-1] ==> y =

y is output of mi64_mul_vector_lo_half(lo, qinv, lo) with lo = [0,0,0,2^30,0...,0] = 2^222, q = x = 2^607-1,
qinv[0,9] = [2^64-1,...,2^64-1,2^64-1-2^31] ==> qinv = 2^640-2^607-1, check Montgomery-inverse property:

q*qinv = [2^607-1]*[2^640-2^607-1]
		= 2^1247 - 2^640 - 2^1214 + 2^607 - 2^607 + 1
		= 2^1247 - 2^1214 - 2^640 + 1 == 1 (mod 2^640) .

Now examine full-double-width multiply in term-by-term fashion, powers of the base b = 2^64 in left col:

b^n:	Coefficient (unnormalized)
----	--------------------------
n = 0	x0.y0
1		x0.y1 + x1.y0
2		x0.y2 + x1.y1 + x2.y0
3		x0.y3 + x1.y2 + x2.y1 + x3.y0
4		x0.y4 + x1.y3 + x2.y2 + x3.y1 + x4.y0
5		x0.y5 + x1.y4 + x2.y3 + x3.y2 + x4.y1 + x5.y0
6		x0.y6 + x1.y5 + x2.y4 + x3.y3 + x4.y2 + x5.y1 + x6.y0
7		x0.y7 + x1.y6 + x2.y5 + x3.y4 + x4.y3 + x5.y2 + x6.y1 + x7.y0
8		x0.y8 + x1.y7 + x2.y6 + x3.y5 + x4.y4 + x5.y3 + x6.y2 + x7.y1 + x8.y0
9		x0.y9 + x1.y8 + x2.y7 + x3.y6 + x4.y5 + x5.y4 + x6.y3 + x7.y2 + x8.y1 + x9.y0
10		        x1.y9 + x2.y8 + x3.y7 + x4.y6 + x5.y5 + x6.y4 + x7.y3 + x8.y2 + x9.y1
11		                x2.y9 + x3.y8 + x4.y7 + x5.y6 + x6.y5 + x7.y4 + x8.y3 + x9.y2
12		                        x3.y9 + x4.y8 + x5.y7 + x6.y6 + x7.y5 + x8.y4 + x9.y3
13		                                x4.y9 + x5.y8 + x6.y7 + x7.y6 + x8.y5 + x9.y4
14		                                        x5.y9 + x6.y8 + x7.y7 + x8.y6 + x9.y5
15		                                                x6.y9 + x7.y8 + x8.y7 + x9.y6
16		                                                        x7.y9 + x8.y8 + x9.y7
17		                                                                x8.y9 + x9.y8
18		                                                                        x9.y9
19		                                                                          0

Does use of only the b^9 = x0.y9 + x1.y8 + x2.y7 + x3.y6 + x4.y5 + x5.y4 + x6.y3 + x7.y2 + x8.y1 + x9.y0 term
yield the correct carry into the b^10 term here?

bc
b=2^64;
x0=x1=x2=x3=x4=x5=x6=x7=x8=b-1; x9=2^31-1
y0=y1=y2=y3=y4=y5=0; y6=b-2^60; y7=y8=y9=b-1;

																								bn (normalized)			cy
b0-5 = 0																						0						0
b6 = cy+x0*y6+x1*y5+x2*y4+x3*y3+x4*y2+x5*y1+x6*y0                   ;m=b6%b ;m;cy=(b6 -m)/b;cy	1152921504606846976		17293822569102704639
b7 = cy+x0*y7+x1*y6+x2*y5+x3*y4+x4*y3+x5*y2+x6*y1+x7*y0             ;m=b7%b ;m;cy=(b7 -m)/b;cy	0						35740566642812256254
b8 = cy+x0*y8+x1*y7+x2*y6+x3*y5+x4*y4+x5*y3+x6*y2+x7*y1+x8*y0       ;m=b8%b ;m;cy=(b8 -m)/b;cy	0						54187310716521807869
b9 = cy+x0*y9+x1*y8+x2*y7+x3*y6+x4*y5+x5*y4+x6*y3+x7*y2+x8*y1+x9*y0 ;m=b9%b ;m;cy=(b9 -m)/b;cy	0						72634054790231359484
b10= cy+      x1*y9+x2*y8+x3*y7+x4*y6+x5*y5+x6*y4+x7*y3+x8*y2+x9*y1 ;m=b10%b;m;cy=(b10-m)/b;cy	18446744073709551615	72634054790231359484
b11= cy+            x2*y9+x3*y8+x4*y7+x5*y6+x6*y5+x7*y4+x8*y3+x9*y2 ;m=b11%b;m;cy=(b11-m)/b;cy
b12= cy+                  x3*y9+x4*y8+x5*y7+x6*y6+x7*y5+x8*y4+x9*y3 ;m=b12%b;m;cy=(b12-m)/b;cy
b13= cy+                        x4*y9+x5*y8+x6*y7+x7*y6+x8*y5+x9*y4 ;m=b13%b;m;cy=(b13-m)/b;cy
b14= cy+                              x5*y9+x6*y8+x7*y7+x8*y6+x9*y5 ;m=b14%b;m;cy=(b14-m)/b;cy
b15= cy+                                    x6*y9+x7*y8+x8*y7+x9*y6 ;m=b15%b;m;cy=(b15-m)/b;cy
b16= cy+                                          x7*y9+x8*y8+x9*y7 ;m=b16%b;m;cy=(b16-m)/b;cy
b17= cy+                                                x8*y9+x9*y8 ;m=b17%b;m;cy=(b17-m)/b;cy
b18= cy+                                                      x9*y9 ;m=b18%b;m;cy=(b18-m)/b;cy
b19= cy+

Using low-order-truncated-rhombus, get

b9  = x0*y9 + x1*y8 + x2*y7 + x3*y6 + x4*y5 + x5*y4 + x6*y3 + x7*y2 + x8*y1 + x9*y0 = 1152921504606846979 + b*72634054790231359481	<*** cy is 3 too low! ***
thus carry into b10 = 72634054790231359481 .

With the 3-too-low approx-carry, get

b10 = cy + x1*y9 + x2*y8 + x3*y7 + x4*y6 + x5*y5 + x6*y4 + x7*y3 + x8*y2 + x9*y1
	= 18446744073709551612 + b*72634054790231359484 ,
which has remainder also 3-too-low (18446744073709551612 vs the correct 18446744073709551615 = 2^64-1) but correct carryout.

UPSHOT: Could spend days coming up with error-coorection scheme, but no point, since for mm(p)-TF-related modmuls we already
have an exact fast O(n) alternative to compute the mi64_mul_vector_hi_half(q, lo) result.
*/
#if 0
#error mi64_mul_vector_hi_trunc not yet debugged!
void	mi64_mul_vector_hi_trunc(const uint64 x[], const uint64 y[], uint64 z[], uint32 len)
{
	uint32 j, lm1 = len-1;
	static uint64 *u = 0x0, *v = 0x0;	// Scratch arrays for storing intermediate scalar*vector products
	static uint32 dimU = 0;
	ASSERT(HERE, len != 0, "zero-length X-array!");

	// Does scratch array need allocating or reallocating? (Use realloc for both cases):
	if(dimU < (len+1)) {
		dimU = 2*(len+1);
		// Alloc 2x the immediately-needed to avoid excessive reallocs if neededsize increases incrementally
		u = (uint64 *)realloc(u, 2*(len+1)*sizeof(uint64));
		v = (uint64 *)realloc(v, 4* len  )*sizeof(uint64));
	}

	/* Loop over the elements of y[], multiplying x[] by each, and
	using u[] as a scratch array to store x[]*y[j] prior to adding to z[].

	For the high-half version, only want terms x[i]*y[j] with (i + j) >= len,
	plus the high halves of the (i + j) = len-1 terms for the requisite carryins.
	*/
	for(j = 0; j < len; j++)
	{
		if(y[j] == 0)
			continue;
		// Only need y[j]*u[len-1-j:len-1], i.e. low term of order [len-1]
		u[lm1] = mi64_mul_scalar(&x[lm1-j], y[j], &u[lm1-j], j);
		// Add j-word-left-shifted u[] to v[], retaining only terms with index < len in the sum:
		v[lm1+j] = u[lm1] + mi64_add(&v[lm1], &u[lm1-j], &v[lm1], j);
		// Full-length: v[j, j+1, ..., j+len-1] gets added to u[0,1,...,len-1]
		// Half-length: v[ len-1, ..., j+len-1] gets added to u[len-1-j,...,len-1]
	}
#if DEBUG_HI_MUL
	// Test code for fast version of this function - re-use low half of v[] for output::
	mi64_mul_vector_hi_half(x,y,v,len);
	if(!mi64_cmp_eq(v,v+len,len)) {
		printf("mi64_mul_vector_hi_trunc result incorrect!");
	}
#endif

	/* Copy v[len:2*len-1] into z[0:len-1]: */
	memcpy(z,v+len,(len<<3));
}
#endif


/* Fast O(n) variant of the more-general O(n^2) mi64_mul_vector_hi_half() to compute UMULH(q, Y)
for the special case of MM(p)-TF-related modmul, i.e. with modulus q = 2.k.M(p) + 1, where M(p)
is a Mersenne prime. For such special-form factor candidates q we define

	Z = 2.k.Y, needing just O(N) work, where N = number of computer words needed to store Y,

And then use that

	UMULH(q,Y) = ((Z << p) - (2k-1).Y) >> B . [*]

where B is the emulated bits per vector-integer 'word' as defined by the associated MULL implementation;
that is, 2^B is the emulated base of the 2s-comp arithmetic being done.

Note that the result can NOT be computed like so:

			   = (Z >> (B-p)) - (2k-1).(Y >> B) ,

because this neglects the possible borrow from the bits of the difference which get shifted off.

The (Z << p) term in [*] needs O(n) hardware shifts and adds to compute and (2k-1).Y needs no
explicit shifts but rather O(n) hardware multiplies to evaluate the scalar-vector product,
followed by simply retaining the single-word carry out of that, discarding the low B bits.

We replace the literal vector-q-input in the function arglist with the single-word integers k and p.
*/
// emulated bits per 'word' as implemented by MULL = (len<<6):
#ifdef __CUDA_ARCH__
__device__
void	mi64_mul_vector_hi_qmmp(const uint64 y[], const uint64 p, const uint64 k, uint64 z[], uint32 bits, uint64 u[], uint64 v[])
#else
void	mi64_mul_vector_hi_qmmp(const uint64 y[], const uint64 p, const uint64 k, uint64 z[], uint32 bits)
#endif
{
	// If bits not a multiple of 64, len2 may = 2*len-1 rather than 2*len:
	uint32 i = (bits+63), len = (i >> 6), len2 = ((i+bits) >> 6), len8 = (len << 3), ldim;
	uint64 k2 = k+k, bw;
  #ifndef __CUDA_ARCH__
	/* Scratch array for storing intermediate scalar*vector products: */
	static uint64 *u = 0x0, *v = 0x0;
	static uint32 dimU = 0;
	/* Does scratch array need allocating or reallocating? */
	if(dimU < (len+2)) {
		// U needs same dim as v (i.e. (len*2) instead of (len+2)) to ensure mi64_shl can never grab an uninited high word
		dimU = (len+2);	ldim = MAX((len*2), (len+2));
		if(u) {
			free((void *)u); u = 0x0;
			free((void *)v); v = 0x0;
		}
		u = (uint64 *)calloc(ldim, sizeof(uint64));
		v = (uint64 *)calloc(ldim, sizeof(uint64));
	}
  #endif
//====need to finish 200-bit support! =======================
	ASSERT(HERE, z != y, "Input and output arrays must be distinct!");
	ASSERT(HERE, p < bits, "shift parameters out of range!");
	ASSERT(HERE, len != 0, "zero-length X-array!");
	for(i = len+1; i < len2; i++) {
		u[i] = 0ull;	// With proper padding of U don't need any zeroing of V prior to V = (U << p) step below
	}
	// memset(v, 0ull, (len<<4));	// No need to clear Accumulator v[] here due to dim = len2 in mi64_shl below
	ASSERT(HERE, (k != 0) && ((k2>>1) == k), "2*k overflows!");	// Make sure 2*k did not overflow
	u[len] = mi64_mul_scalar(y,k2,u,len);	// u[] stores Z = 2.k.Y
	mi64_shl(u,v,p,len2);			// v[] stores (Z << p), store result in V
	u[len] -= mi64_sub(u,y,u,len);	// (2k-1).Y = Z-Y, store result in U
	bw = mi64_sub(v,u,v,len+1);
	ASSERT(HERE, !bw, "Unexpected borrow!");

	/* Right-shift by B bits to get UMULH(q,Y) = ((Z << p) - (2k-1).Y) >> B: */
	mi64_shrl(v,v,bits,len2);
	memcpy(z,v,len8);
//	memcpy(z,v+len,len8);

#if MI64_DEBUG
	#define DEBUG_HI_MUL	0	// Set nonzero to compare versus mi64_mul_vector_hi_fast
#endif
#if DEBUG_HI_MUL
//	#error Check that you want DEBUG_HI_MUL set in mi64_mul_vector_hi_qmmp!
	// Compute q, store in u[]:
	memset(u, 0ull, (dimU<<3));
	u[0] = 1;
	mi64_shl(u, u, p, len);			// 2^p
	mi64_sub_scalar(u, 1, u, len);	// M(p) = 2^p-1
	ASSERT(HERE, 0 == mi64_mul_scalar(u, k2, u, len), "2.k.M(p) overflows!");	// 2.k.M(p)
	mi64_add_scalar(u, 1ull, u, len);	// q = 2.k.M(p) + 1
	// Test code for fast version of this function - re-use v[] for output::
//	mi64_mul_vector_hi_half(u,y,v,len);
	mi64_mul_vector_hi_fast(y,p,k,v,len);
	if(!mi64_cmp_eq(v,z,len)) {
		ASSERT(HERE, 0, "mi64_mul_vector_hi_qmmp/fast results differ!");
	}
#endif
}

/* Version #2 of the above, which further streamlines things. again start with the following 2 multiplicands:

	1. q = 2.k.M(p) + 1, where M(p) is a Mersenne prime;
	2. multiword multiplicand Y < q,

and take B as the emulated bits per vector-integer 'word' as defined by the associated MULL implementation; that
is, 2^B is the emulated base of the 2s-comp arithmetic being done; in terms of the hardware-integer wordsize W = 2^b,
B = n*b, i.e. the multiplicands are n-word vectors in terms of the hardware-integer wordsize.
In terms of relative sizes, y < q < B and k < W, i.e. k is a 1-word scalar multiplier. We further assume k < W/2,
thus 2*k fits into a single word, as well.

Define

	Z = 2.k.Y, which needs just O(n) work to compute via mi64_mul_scalar().

Again we use that

	UMULH(q,Y) = ((Z << p) - (2k-1).Y) >> B . [*]

The above version of this function computes the full-length scalar-vector product Z = 2.k.Y, then does a full-length
vector-vector subtract to obtain Z - Y = (2k-1).Y, followed by a double-wide vector-left-shift to get (Z << p), another
full-length vector-vector subtract to get (Z << p) - (2k-1).Y, followed by a vector-subtract-scalar to propagate the
resulting borrow into the high (n) words of the length-2n (Z << p), followed by a length-n vector-copy (in lieu of
a double-wide vector-right-shift of the result by B bits) in order to obtain the result [*].

This is O(n) work but still involves much wasted computation, specifically related to the fact that half the
words of the penultimate double-wide vector are discarded: These are only needed for the possible
subtraction-borrow they provide to the retained bits.

	Example: q = 2.k.M(127) + 1 with k = 7143819210136784550, i.e.
	q = 2430915709680614116949754105299803650411408301848040235701 ;
	y =  915005412744957807408012591600653057424688130286064771258 = y0 + 2^64*y1 + 2^128*y2,
	with y0 = 2294959606785646778; y1 = 10167084567166165345; y2 = 2688959234133783535 .

	[Note that 2^192 = 6277101735386680763835789423207666416102355444464034512896 .]

	Exact result:

		UMULH_192(q,y) = 354351598245602020483095922210514413558224553895064094733 = u0 + 2^64*u1 + 2^128*u2,
		with u0 = 141525868296128525, u1 = 4269430960237156763, u2 = 1041345754856384950 .

	b = 192 bits
	p = 127 bits
	(b-p) = 65
	Compute (2k-1).y, store in z:
	z' = (2k-1).y = 2^b*2082691509712769900 + [low 192 bits], compare that retained coefficient to direct high-64 bits of 128-bit product (2k-1).y2:
	umulh64(2k-1, y2) = 2082691509712769899, one too low, because this neglects the possibility of a carryin resulting from
	 mull64(2k-1, y2) + umulh64(2k-1, y1).

But we need the full-length product (2k-1).y *anyway* to get Z = 2.k.y (by adding another copy of y), so do as follows:

1. compute z' = (2k-1).y via vector-scalar mul, the carryout word cw = ((2k-1).Y >> B);

2. compute low n words of z = z' + y via vector-vector add, make sure there is no carryout of that (or if there is, add it to a 2nd copy of cw, say, cz);

3. compute low n words of z >> (b-p), then separately shift in cw from the left, via (2^b*cz) >> (b-p) = (cz << p).
[*** idea: define special version of mi64_shift for this ***]
4. subtract scalar cw from resulting vector to effect ... - (2k-1).(Y >> B) step in [*].
*/
#ifdef __CUDA_ARCH__
__device__
#endif
void	mi64_mul_vector_hi_fast(const uint64 y[], const uint64 p, const uint64 k, uint64 z[], uint32 len)
{
	uint32 i, bits;
	uint64 k2m1 = k-1+k, tmp,bw0,bw1,bw,cw,cy,cz;
	uint64 *zptr;
	ASSERT(HERE, z != y, "Input and output arrays must be distinct!");
	ASSERT(HERE, (k != 0) && ((k2m1>>1) == k-1), "2*k-1 overflows!");
	ASSERT(HERE, len != 0, "zero-length X-array!");

// 1. compute z' = (2k-1).y via vector-scalar mul, the carryout word cw = ((2k-1).Y >> B);
	cw = mi64_mul_scalar(y,k2m1,z,len);	// z' = (2k-1).y
	bw0 = z[len-1];
//if(k==900) printf("Mi64: bw0 = %20llu, cw = %20llu, z` = %s\n", bw0,cw,&s0[convert_mi64_base10_char(s0, z, len, 0)]);
// 2. compute low n words of z = z' + y via vector-vector add, any carryout of that gets added to a 2nd copy of cw, cz;
	cz = cw + mi64_add(y,z,z, len);	// z = z' + y
//if(k==900) printf("Mi64: cz = %20llu, z = %s\n", cz,&s0[convert_mi64_base10_char(s0, z, len, 0)]);

// 3. compute low n words of z >> (b-p), then separately shift in cz from the left, via (2^b*cz) >> (b-p) = (cz << p).
	ASSERT(HERE, (len<<6) > p, "shift parameters out of range!");
	bw1 = mi64_shrl(z,z,(len<<6)-p,len);	// low n words of z >> (b-p); high 64 bits of off-shifted portion saved in bw1
//if(k==900) printf("Mi64: bw1 = %20llu, z>> = %s\n", bw1,&s0[convert_mi64_base10_char(s0, z, len, 0)]);

/* Check for borrow-on-subtract of to-be-off-shifted sections: have a borrow if
	z' (result from above mul_scalar, not including the carryout word cw) >	((z << p) % 2^b) (off-shifted portion of z = z' + y above, left-justified to fill a b-bit field)
In order to compute this borrow with high probability without having to store the entire b-bit vectors in this comparison,
we save just the high word of z', bw0 := z'[len-1], and the high word of the off-shifted portion of z (call it bw1), and compare.
The only tricky case is is these word-length proxies for the full-length vectors compare equal, in which case we should abort
and tell the user to call the slow exact version of this function, currently inactivated inside the above #if 0.
*/
	bw = (bw0 > bw1);
	// Add (cw << (b-p)) to result.
	bits = (p&63);
	i = (p >> 6);	// Low word into which cw will get added
	zptr = z+i;
	// If (b-p) == 0 (mod 64) all of cz goes into z[i], with i = (b-p)/64;
	if(bits == 0) {
		ASSERT(HERE, 0 == mi64_add_scalar(zptr,cz,zptr,len-i), "unexpected carryout of ( + cw)!");
	// Otherwise cz gets split between z[i] and z[i+1]:
	} else {
		// low 64-(p%64) bits of cz = (cz << bits) go into z[i]:
		tmp = (cz << bits);
		*zptr += tmp; cy = (*zptr++ < tmp);
		// high (p%64) bits of cw = (cw >> bits) go into z[i+1]
		ASSERT(HERE, 0 == mi64_add_scalar(zptr,(cz >> (64-bits)) + cy,zptr,len-i-1), "unexpected carryout of ( + cw).hi!");
	}

// 4. subtract scalar (bw + cw) from resulting vector to effect ... - (2k-1).Y step in [*].
	ASSERT(HERE, 0 == mi64_sub_scalar(z,(bw + cw),z,len), "unexpected carryout of (... - cw) !");
}


/* Fast O(n) variant of the more-general O(n^2) mi64_mul_vector_hi_half() to compute UMULH(q, Y)
for the special case of Fermat-number F(p) TF-related modmul, i.e. with modulus q = 2.k.2^p + 1, where it is assumed
that the caller has fiddled with the factor-candidate q's k-value so as to put q into this Mersenne-factor-like form.
(Note: p need not be prime; just by way of hacking this from the pre-existing analogous double-Mersenne routine).
For such special-form factor candidates q we define

	Z = 2.k.Y, needing just O(N) work, where N = number of computer words needed to store Y,

And then use that

	UMULH(q,Y) = ((2.k.2^p + 1).Y) >> B
	           = ((Z << (p+1)) + Y) >> B . [*]

where B is the emulated bits per vector-integer 'word' as defined by the associated MULL implementation;
that is, 2^B is the emulated base of the 2s-comp arithmetic being done.

The (Z << (p+1)) term in [*] needs O(n) hardware muls and shifts to compute.

We replace the literal vector-q-input in the function arglist with the single-word integers k and p.
*/
// emulated bits per 'word' as implemented by MULL = (len<<6):
#ifdef __CUDA_ARCH__
__device__
void	mi64_mul_vector_hi_qferm(const uint64 y[], const uint64 p, const uint64 k, uint64 z[], uint32 bits, uint64 u[])
#else
void	mi64_mul_vector_hi_qferm(const uint64 y[], const uint64 p, const uint64 k, uint64 z[], uint32 bits)
#endif
{
	// If bits not a multiple of 64, len2 may = 2*len-1 rather than 2*len:
	uint32 i = (bits+63), len = (i >> 6), len2 = ((i+bits) >> 6), len8 = (len << 3), ldim;
	uint64 cy;
  #ifndef __CUDA_ARCH__
	/* Scratch array for storing intermediate scalar*vector products: */
	static uint64 *u = 0x0;
	static uint32 dimU = 0;
	/* Does scratch array need allocating or reallocating? */
	if(dimU < (len+2)) {
		// U needs dim (len*2) instead of (len+2)) to ensure mi64_shl can never grab an uninited high word:
		dimU = (len+2);	ldim = MAX((len*2), (len+2));
		if(u) {
			free((void *)u); u = 0x0;
		}
		u = (uint64 *)calloc(ldim, sizeof(uint64));
	}
  #endif
	ASSERT(HERE, z != y, "Input and output arrays must be distinct!");
	ASSERT(HERE, p < bits, "shift parameters out of range!");
	ASSERT(HERE, k != 0ull, "k must be nonzero!");
	for(i = len+1; i < len2; i++) {
		u[i] = 0ull;	// With proper padding of U don't need any zeroing of V prior to V = (U << p) step below
	}
	// memset(u, 0ull, (len<<4));	// No need to clear Accumulator u[] here due to dim = len2 in mi64_shl below
	u[len] = mi64_mul_scalar(y,k,u,len);	// u[] stores Z = k.Y
	mi64_shl(u,u,(p+1),len2);				// u[] stores (Z << p)
	cy = mi64_add(u,y,u,len);
	cy = mi64_add_scalar(u+len,cy,u+len, len2-len);
	ASSERT(HERE, (cy == 0ull), "Unexpected carry!");

	/* Right-shift by B bits to get UMULH(q,Y) = ((Z << p) - (2k-1).Y) >> B: */
	mi64_shrl(u,u,bits,len2);
	memcpy(z,u,len8);
}

/*************************************************************/
/****** STUFF RELATED TO FFT-BASED MULTIPLY BEGINS HERE ******/
/*************************************************************/

/*
!	Functions to convert a generic multiword int (i.e. not w.r.to a particular
!	modulus) from/to balanced-digit fixed-base floating-point form and uint64 form.
*/

/*
	EFFECTS: Converts pair of uint64 multiword ints x[],y[] plus an optional bit-concatenated carryin pair
												 ==> double (balanced-digit base-FFT_MUL_BASE representation)
			and stores the resulting floating values in packed-complex form in
			a[] (x[] terms in real/even-index terms of a[], y[] terms in imaginary/odd-index.)

	RETURNS: 2-bit concatenated carryout pair. Since inputs are by definition nonnegative, cyouts are, as well.

	ASSUMES: User has allocated enough room in a[] to store the complete result in
			balanced-digit form, and that there is no remaining output carry.
*/
uint32	mi64_cvt_uint64_double(const uint64 x[], const uint64 y[], uint32 cy, uint32 len, double a[])
{
	uint32 i, j = 0, jpad, k;
	 int64 cyi, cyj, itmp, jtmp;
	uint64 curr_re64, curr_im64, bitsm1 = FFT_MUL_BITS-1, basem1 = FFT_MUL_BASE-1;

	ASSERT(HERE, len != 0, "mi64_cvt_uint64_double: zero-length array!");

	/* Only constant base 2^16 is supported for this conversion at present: */
	ASSERT(HERE, FFT_MUL_BITS == 16, "mi64_cvt_uint64_double: FFT_MUL_BITS != 16");

	/* Redo the quicker checks of those done in util.c::check_nbits_in_types() */
	ASSERT(HERE, DNINT(FFT_MUL_BASE) == FFT_MUL_BASE, "mi64_cvt_uint64_double: FFT_MUL_BASE not pure-integer!");
	ASSERT(HERE, FFT_MUL_BASE < TWO54FLOAT, "mi64_cvt_uint64_double: FFT_MUL_BASE >= maximum allowed value of 2^54!");

	/* As we extract each floating-point word, balance it and set
	resulting carry into next FP word: */
	cyi = cy&1; cyj = cy>>1;
	for(i = 0; i < len; i++)
	{
		curr_re64 = x[i];
		curr_im64 = y[i];
		for(k = 0; k < 4; k++) {
			j = (i<<2) + k;
			j *= 2;	/* Since have two input arrays */
			jpad = j + ( (j >> DAT_BITS) << PAD_BITS );	/* Padded-array index */
			/* Extract the current 16-bit subfield: */
			itmp = cyi + ((curr_re64>>(k<<4)) & basem1);
			jtmp = cyj + ((curr_im64>>(k<<4)) & basem1);

			/* Is the current digit >= (base/2)? Note the extra != 0 check here which forces the carries to be at most 1 -
			that's needed for the extreme case tmp == base, in which the simple right-shift by bitsm1 gives cy = 2, which would cause
			(cy << FFT_MUL_BITS) = base*2, giving a "notmalized" result = -base, rather than the desired 0: */
			cyi = (int64)(((uint64)itmp>>bitsm1) != 0);	/* Cast to unsigned to ensure logical right-shift */
			cyj = (int64)(((uint64)jtmp>>bitsm1) != 0);	/* Cast to unsigned to ensure logical right-shift */
			/* If yes, balance it by subtracting the base: */
			/* RHS terms must be signed to prevent integer underflow-on-subtract: */
			a[jpad  ] = (double)(itmp - (cyi<<FFT_MUL_BITS));
			a[jpad+1] = (double)(jtmp - (cyj<<FFT_MUL_BITS));
		}
	}
	ASSERT(HERE, cyi <= 1 && cyj <= 1,"mi64_cvt_uint64_double: Output carry out of range!");
#if 0
	// It is desirable to not have the FP vector length exceed 4*len,
	// so suppress any output carry by folding back into MS array element:
	if(cyi) {
		ASSERT(HERE, a[jpad  ] <= 0,"mi64_cvt_uint64_double: MS array element >= 0!");
		a[jpad  ] += FFT_MUL_BASE;
	}
	if(cyj) {
		ASSERT(HERE, a[jpad+1] <= 0,"mi64_cvt_uint64_double: MS array element >= 0!");
		a[jpad+1] += FFT_MUL_BASE;
	}
printf("mi64_cvt_uint64_double: Final a[%u,%u] = %15.3f,%15.3f\n",jpad,jpad+1,a[jpad],a[jpad+1]);
	return 0;
#else
	return cyi + (cyj<<1);
#endif
}

/*
	Takes a pair of multiprecision n-element real vectors stored in balanced-digit
	packed-complex form in the double[] A-array, and converts them to uint64[] form,
	storing Re-part (even-index input-vec terms) of A[] in x[], Im-part in y[]. Any
	unfilled bits in the high words of the x/y output vectors are filled in with 0s.

	ASSUMES:

		- Input array has been properly rounded (i.e. all elements have zero fractional
		part) and normalized, i.e. examination of MSW suffices to determine sign.

		- Floating-point base may be > 2^32 (e.g. for mixed FFT/FGT),
		but is < 2^54, i.e. properly-normalized balanced digits are
		in [-2^53, +2^53] and thus exactly representable via an IEEE double.

	RETURNS: 4-bit concatenated carryout pair. Since inputs may be of either sign,
	the high bit of each 2-bit cyout indicates sign and the low bit the magnitude.
	The sign is only set if the cyout = -1, i.e. has unity magnitude, thus each 2-bit
	cyout takes one of just 3 possible binary values:

		00: cy =  0
		01: cy = +1
		11: cy = -1 (i.e. it's a borrow).
*/
uint32	mi64_cvt_double_uint64(const double a[], uint32 n, uint64 x[], uint64 y[])
{
	uint32 len;
	int curr_bits,i,j,nbits;
	int64 cy_re, cy_im, itmp, jtmp;
	uint64 curr_re64, curr_im64;

	ASSERT(HERE, n != 0, "zero-length array!");

	/* Redo the quicker checks of those done in util.c::check_nbits_in_types() */
	ASSERT(HERE, DNINT(FFT_MUL_BASE) == FFT_MUL_BASE, "FFT_MUL_BASE not pure-integer!");
	ASSERT(HERE, FFT_MUL_BASE < TWO54FLOAT, "FFT_MUL_BASE >= maximum allowed value of 2^54!");
/* Obsolete, for historical reference only:
	// Make sure MSW of Re(A[]) and Im(A[]) in the balanced-representation form are both >= 0:
	// Re(A[]) stored in even terms:
	for(i = 2*n-2; i >= 0; i-=2) {
		j = i + ( (i >> DAT_BITS) << PAD_BITS );
		if(a[j] != 0.0) {
			ASSERT(HERE, a[j] > 0.0, "MSW(Re(A[])) < 0!");
			break;
		}
	}
	// Im(A[]) stored in odd terms:
	for(i = 2*n-1; i >= 1; i-=2) {
		j = i + ( (i >> DAT_BITS) << PAD_BITS );
		if(a[j] != 0.0) {
			ASSERT(HERE, a[j] > 0.0, "MSW(Im(A[])) < 0!");
			break;
		}
	}
*/
	/*...Now form the terms of the uint64 representation:	*/
	len   = 0;		/* Full 64-bit words accumulated so far in the residue	*/
	nbits = 0;		/* Total bits accumulated so far in the x-array	*/
	curr_bits = 0;	/* Total bits currently stored in the 64-bit accumulator word */
	curr_re64 = 0;	/* Current value of the 64-bit accumulator word */
	curr_im64 = 0;

	cy_re = cy_im = 0;	/* Init carry */
	for(i = 0; i < 2*n; i+=2)
	{
		j = i + ( (i >> DAT_BITS) << PAD_BITS );


		itmp = (uint64)1<<curr_bits;		ASSERT(HERE, curr_bits < 64,"curr_bits < 64");
		ASSERT(HERE, curr_re64 < itmp && curr_im64 < itmp,"curr_wd64 !< (1<<curr_bits)");
		ASSERT(HERE, DNINT(a[j]) == a[j] && ABS(a[j]) < TWO54FLOAT, "a[j] not pure-integer or out of range!");

		itmp = (int64)a[j  ] + cy_re;	/* current digit in int64 form, subtracting any borrow from previous digit.	*/
		if(itmp < 0) {	/* If current digit < 0, add the base and set carry = -1	*/
			itmp += FFT_MUL_BASE;
			cy_re = -1;
		} else {
			cy_re = 0;
		}

		jtmp = (int64)a[j+1] + cy_im;
		if(jtmp < 0) {
			jtmp += FFT_MUL_BASE;
			cy_im = -1;
		} else {
			cy_im = 0;
		}
		ASSERT(HERE, itmp >= 0 && jtmp >= 0,"itmp,jtmp must be nonnegative 0!");
		ASSERT(HERE, (curr_re64>>curr_bits) == 0 && (curr_im64>>curr_bits) == 0,"(curr_wd64>>curr_bits) != 0!");

		/* Copy bits of the current residue word into the accumulator, starting
		at the (curr_bits)th bit. The resulting total number of accumulated bits
		will be curr_bits += FFT_MUL_BITS. If this is > 64, move the low 64 bits of
		the accumulator (which are in curr_wd64) into the x-array, increment (len),
		and move the overflow bits (which are in itmp >> (curr_bits - 64) ) into curr_wd64.
		*/
	#if 0
		EXAMPLE: curr_bits=40, FFT_MUL_BITS = 53: lower (64-40)=24 bits of itmp get copied to
		curr_wd64 starting at bit 40, accum now has (40+53) = 93 > 64 bits, so dump curr_wd64
		into x[len++], move upper (93-64) = 29 bits of itmp into curr_wd64, reset curr_bits=29.
	#endif

		curr_re64 += (itmp<<curr_bits);
		curr_im64 += (jtmp<<curr_bits);

		curr_bits += FFT_MUL_BITS;		/* Ex: 40+=53 => 93 */
		if(curr_bits >= 64) {
			x[len  ] = curr_re64;
			y[len++] = curr_im64;

			nbits += 64;
			curr_bits -= 64;	/* # of bits now left in curr_wd64 */	/* Ex: 93-=64 => 29 */

			/* Ex: Upper 29 bits of itmp via (unsigned) 24-bit right-shift: */
			curr_re64 = (uint64)itmp >> (FFT_MUL_BITS - curr_bits);
			curr_im64 = (uint64)jtmp >> (FFT_MUL_BITS - curr_bits);
		}
	}

	/* Dump any remaining partially-filled word: */
	if(curr_bits) {
		x[len  ] = curr_re64;
		y[len++] = curr_im64;
		nbits += curr_bits;
	}
//	printf("mi64_cvt_double_uint64: Final a[%u,%u] = %15.3f,%15.3f; x,y[%u] = %llu,%llu\n",j,j+1,a[j],a[j+1],len-1,x[len-1],y[len-1]);
	ASSERT(HERE, nbits == n*FFT_MUL_BITS,"nbits == n*FFT_MUL_BASE!");
	ASSERT(HERE, len == (n>>2)          ,"len should == n/4!");
	ASSERT(HERE, ABS(cy_re) <= 1 && ABS(cy_im) <= 1,"Output carry out of range!");
	// Carries declared signed, but throw in casts of the 0 in the < compares to ensure signedness of these:
	return ( (cy_im < (int64)0)*8 + (cy_im != 0ull)*4 + (cy_re < (int64)0)*2 + (cy_re != 0ull) );
}

/***************/

/* returns 1 if the multiword unsigned integer p is a base-z Fermat pseudoprime, 0 otherwise. The base is assumed to fit in a 64-bit unsigned scalar. */
#if MI64_DEBUG
	#define MI64_PRP_DBG 0
#endif
#ifndef __CUDA_ARCH__
uint32 mi64_pprimeF(const uint64 p[], uint64 z, uint32 len)
{
//int dbg = (p[0] == 25409026523137ull);
/* The allocatable-array version of this gave bizarre stack faults under MSVC, so use fixed size,
especially as this routine is too slow to be useful for inputs larger than a couple hundred words anyway:
*/
/*	static uint64 *y, *n, *zvec, *ppad, *prod, *tmp;	*/
	uint64 y[1024], n[1024], zvec[1024], ppad[2048], prod[2048];
	uint64 flag;
	uint32 len2 = 2*len, curr_len, retval, plen;
  #if MI64_PRP_DBG
	uint128 q128, r128;
	uint192 q192, r192;
  #endif
	ASSERT(HERE, p != 0x0, "Null input-array pointer!");
	ASSERT(HERE, len <= 1024, "mi64_pprimeF: Max 1024 words allowed at present!");
	plen = mi64_getlen(p, len);
	// Special handling for even moduli:
	if(IS_EVEN(p[0])) return (len == 1 && p[0] == 2ull);
/*
	y    = (uint64 *)calloc((  len), sizeof(uint64));
	n    = (uint64 *)calloc((  len), sizeof(uint64));
	zvec = (uint64 *)calloc((  len), sizeof(uint64));
	ppad = (uint64 *)calloc((len2), sizeof(uint64));
	prod = (uint64 *)calloc((len2), sizeof(uint64));
*/
	mi64_clear(y, len);
	mi64_clear(n, len);
	mi64_clear(zvec, len);
	mi64_clear(ppad, len2);
	mi64_clear(prod, len2);

	y[0] = 1ull;						/* y = 1 */
	zvec[0] = z;						/* vector version of z for powering */
	mi64_set_eq(ppad, p, len);			/* set ppad = p; ppad is zero-padded */
	mi64_set_eq(n   , p, len);
	mi64_sub_scalar(n, 1ull, n, len);	/* n = p-1 */
	curr_len = mi64_getlen(n, len);		ASSERT(HERE, curr_len > 0, "0-length array!");

  #if 0	// Slower RL modpow:

	while(!mi64_iszero(n, curr_len))
	{
		flag = n[0] & 1;
		mi64_shrl(n, n, 1, curr_len);	/* n >>= 1, also update curr_len */
		curr_len = mi64_getlen(n, curr_len);

	#if MI64_PRP_DBG
		if(len == 2) {
			q128[0] = y[0]; q128.d1 = y[1];
			r128[0] = zvec[0]; r128.d1 = zvec[1];
			printf("mi64_pprimeF: flag = %1d, y = %s, z = %s\n", (uint32)flag, &s0[convert_uint128_base10_char(s0, q128, 0)], &s1[convert_uint128_base10_char(s1, r128, 0)]);
		} else if(len == 3) {
			q192[0] = y[0]; q192.d1 = y[1]; q192.d2 = y[2];
			r192[0] = zvec[0]; r192.d1 = zvec[1]; r192.d2 = zvec[2];
			printf("mi64_pprimeF: flag = %1d, y = %s, z = %s\n", (uint32)flag, &s0[convert_uint192_base10_char(s0, q192, 0)], &s1[convert_uint192_base10_char(s1, r192, 0)]);
		}
	#endif

		if(flag) {
			mi64_mul_vector(y, len, zvec, len, prod, &plen);	/* y*z */
			mi64_div(prod, ppad, len2, len2, 0x0, prod);		/* Discard quotient here, overwrite prod with remainder */
			ASSERT(HERE, mi64_getlen(prod, len2) <= len, "mi64_pprimeF: (y*z)%p illegal length");
			mi64_set_eq(y, prod, len);	/* y = (y*z)%p */
		}
		mi64_mul_vector(zvec, len, zvec, len, prod, &plen);		/* z^2 */
		mi64_div(prod, ppad, len2, len2, 0x0, prod);
		ASSERT(HERE, mi64_getlen(prod, len2) <= len, "mi64_pprimeF: (z^2)%p illegal length");
		mi64_set_eq(zvec, prod, len);	/* z = (z^2)%p */
		if(mi64_iszero(zvec, len)) {
			retval=0;
		}
	}

  #else	// LR modpow:

	int j = leadz64(n[curr_len-1]), start_index = (curr_len<<6) - j;
	y[0] = z;
//if(dbg)printf("x0 = %llu, len = %u\n",y[0],plen);
  #if MI64_PRP_DBG
	printf("p  = %s, start_bit = %d\n", &cbuf[convert_mi64_base10_char(cbuf, p, plen, 0)],start_index-2);
	printf("x0 = %s\n", &cbuf[convert_mi64_base10_char(cbuf, y, plen, 0)]);
  #endif
	for(j = start_index-2; j >= 0; j--)
	{
		mi64_sqr_vector(y, prod, plen);		/* y^2 */
		mi64_div(prod, ppad, len2, len2, 0x0, prod);
		ASSERT(HERE, mi64_getlen(prod, len2) <= len, "mi64_pprimeF: (y^2)%p illegal length");
		mi64_set_eq(y, prod, len);	/* y = (y^2)%p */
	  #if MI64_PRP_DBG
		printf("j = %d: x^2 %% p = %s\n", j, &cbuf[convert_mi64_base10_char(cbuf, y, plen, 0)]);
	  #endif
//if(dbg)printf("j = %d: x^2 %% p = %llu\n",j,y[0]);
		if(mi64_test_bit(n,j)) {
			prod[plen] = mi64_mul_scalar(y, z, prod, plen);
	  #if MI64_PRP_DBG
		printf("*= %llu = %s\n", z, &cbuf[convert_mi64_base10_char(cbuf, prod, plen+1, 0)]);
	  #endif
//if(dbg)printf("*= %llu = %llu\n",z,prod[0]);
			mi64_div_binary(prod, p, plen+1, plen, 0x0, y); 	// y = prod % p
	  #if MI64_PRP_DBG
		printf("(mod p) = %s\n", &cbuf[convert_mi64_base10_char(cbuf, y, plen, 0)]);
	  #endif
//if(dbg)printf("(mod p) = %llu\n",y[0]);
		}
	}

  #endif
	retval = mi64_cmp_eq_scalar(y, 1ull, len);
/*
	free((void *)y);
	free((void *)n);
	free((void *)zvec);
	free((void *)ppad);
	free((void *)prod);
*/
//if(dbg)printf("retval = %u\n",retval);
	return retval;
}
#endif	// __CUDA_ARCH__ ?

/****************/

/* Fast division using Montgomery-modmul. First implemented: May 2012 */
// Oct 2015:
// Changed return type of all 3 multiword-div routines (mi64_div, mi64_div_mont, mi64_div_binary) from void to int -
// Now retval = 1|0 if the remainder = 0 or not, allowing user to check divisibility w/o passing a remainder array.
#ifndef __CUDA_ARCH__
int mi64_div(const uint64 x[], const uint64 y[], uint32 lenX, uint32 lenY, uint64 q[], uint64 r[])
{
	int retval = -1;	// -1 implies uninited
	uint32 xlen, ylen, max_len;
	uint64 itmp64;
	// Only the quotient array is optional:
	ASSERT(HERE, lenX && lenY, "illegal 0 dimension!");
	ASSERT(HERE, x && y, "At least one of X, Y is null!");
	ASSERT(HERE, x != y, "X and Y arrays overlap!");
	ASSERT(HERE, r != y, "Y and Rem arrays overlap!");
	ASSERT(HERE, q != x && q != y && (q == 0x0 || q != r), "Quotient array overlaps one of X, Y ,Rem!");

	/* Init Q = 0; don't do similarly for R since we allow X and R to point to same array: */
	if(q && (q != x)) {
		mi64_clear(q, lenX);
	}
	/* And now find the actual lengths of the divide operands and use those for the computation: */
	xlen = mi64_getlen(x, lenX);
	ylen = mi64_getlen(y, lenY);
	ASSERT(HERE, ylen != 0, "divide by 0!");

	// If x < y, no modding needed - copy x into remainder and set quotient = 0:
	max_len = MAX(xlen, ylen);
	if((xlen < ylen) || ((xlen == ylen) && mi64_cmpult(x, y, max_len)) ) {
		/* If no modular reduction needed and X and R not the same array, set R = X: */
		if(r != 0x0 && r != x) {
			mi64_set_eq(r, x, lenX);
		}
		if(q) mi64_clear(q,lenX);
		return mi64_iszero(x, lenX);	// For x < y, y divides x iff x = 0
	}
	// If single-word divisor, use specialized single-word-divisor version:
	if(ylen == 1) {
		itmp64 = mi64_div_by_scalar64_u4(x, y[0], xlen, q);	// Use 4-folded divrem for speed
		retval = (itmp64 == 0ull);
		if(r != 0x0)
			r[0] = itmp64;
		if(r == x) mi64_clear(r+1, lenY-1);			// Only clear high words of remainder array if rem-in-place (x == r)
	} else {
		retval = mi64_div_mont(x, y, xlen, ylen, q, r);
		if(r == x) mi64_clear(r+ylen, lenX-ylen);	// Only clear high words of remainder array if rem-in-place (x == r)
	}
	return retval;
}
#endif	// __CUDA_ARCH__ ?

/* Fast div-with-remainder with arbitrary-length divisor using Montgomery modmul, my right-to-left mod
algorithm, and my true-remainder postprocessing step of May 2012.

Returns x % y in r (required), and x / y in q (optional).
*/
#if MI64_DEBUG
	#define MI64_DIV_MONT	0
#endif
#ifndef __CUDA_ARCH__
int mi64_div_mont(const uint64 x[], const uint64 y[], uint32 lenX, uint32 lenY, uint64 q[], uint64 r[])
{
  #if MI64_DIV_MONT
	// Must limit lenX so dec-print does not overflow string-alloc:
	uint32 dbg = (lenX <= 50) && STREQ(&s0[convert_mi64_base10_char(s0, x, lenX, 0)], "364131549958466711308970009901738230041");
	uint64 *qref = 0x0, *rref = 0x0, *lo_dbg = 0x0, *hi_dbg = 0x0;
  #endif
	int retval = -1;	// -1 implies uninited
	int i,j;			// i and j must be signed
	uint32 lenW,lenD,lenS,lenP, ybits, log2_numbits, n, p, nshift, nws = 0, nbs = 0, nc = 0;
	const uint32 ncmax = 16;
	uint64 bw,mask,lo64,itmp64;
	double fquo;
	// pointers to local-storage:
	static int mod_repeat = 0, modD = 0, modDdim = 0, lens = 0, lenv = 0;	// # of 64-bit ints allocated for current scratch space
	static uint64 *vsave = 0x0, *yinv = 0x0,*cy = 0x0, *tmp = 0x0, *itmp = 0x0, *lo = 0x0, *rem_save = 0x0;
	// Sep 2015: These are for repeated div calls with same modulus - doing so sped up M4423 base-3 PRP test by 10x (!):
	static uint64 *modulus_save = 0x0, *mod_inv_save = 0x0, *basepow_save = 0x0;
	static uint64 *scratch = 0x0;	// "base pointer" for local storage shared by all of the above subarrays
	static uint64 *hi = 0x0, *v = 0x0, *w = 0x0;	// These are treated as vars (cost-offsets of the above ptrs),
													// hence non-static. *** MUST RE-INIT ON EACH ENTRY ***
	ASSERT(HERE, lenX && lenY, "illegal 0 dimension!");
	ASSERT(HERE, (lenY > 1) || (y[0] > 0), "Divide by zero!");
	ASSERT(HERE, (x && y) && (x != y), "Bad x or y array!");
	ASSERT(HERE, (q == 0x0 || q != r), "Quotient and remainder arrays must not overlap!");	// q may be 0x0, but must not overlap r
									// To-do: Change from a simple pointers-coincide to an actual arrays-overlap check.
	lenD = mi64_getlen(y, lenY);	ASSERT(HERE, lenD != 0, "0-length divisor!");

	// Alloc of the repeated-div-associated statics handled separately from other local storage:
	if(modDdim < lenD) {
		modD = 0; modDdim = lenD;
		if(modulus_save) {
			free((void *)modulus_save);	modulus_save = 0x0;
			free((void *)mod_inv_save);	mod_inv_save = 0x0;
			free((void *)basepow_save);	basepow_save = 0x0;
		}
		modulus_save = (uint64 *)calloc((modDdim), sizeof(uint64));	ASSERT(HERE, modulus_save != 0x0, "alloc fail!");
		mod_inv_save = (uint64 *)calloc((modDdim), sizeof(uint64));	ASSERT(HERE, mod_inv_save != 0x0, "alloc fail!");
		basepow_save = (uint64 *)calloc((modDdim), sizeof(uint64));	ASSERT(HERE, basepow_save != 0x0, "alloc fail!");
		mod_repeat = FALSE;
	}

	// Check for repeated divisor:
	mod_repeat = (lenD == modD && mi64_cmp_eq(y, modulus_save, lenD));	// modD != 0 ensures modulus_save pointer inited

  #if MI64_DIV_MONT
	if(dbg) {
		if(lenX <= 50)printf("x = %s\n", &s0[convert_mi64_base10_char(s0, x, lenX, 0)]);
	//	printf("x = %s\n", &str_10k[__convert_mi64_base10_char(str_10k, 10<<10, x, lenX, 0)]);
		printf("y = %s\n", &s0[convert_mi64_base10_char(s0, y, lenD, 0)]);	// Leave length-check off this so if y too large for print we assert right here
		// Compute result using slow binary-div algo, use that as reference:
		qref   = (uint64 *)calloc((lenX), sizeof(uint64));	ASSERT(HERE, qref   != 0x0, "alloc fail!");
		rref   = (uint64 *)calloc((lenX), sizeof(uint64));	ASSERT(HERE, rref   != 0x0, "alloc fail!");
		lo_dbg = (uint64 *)calloc((lenX), sizeof(uint64));	ASSERT(HERE, lo_dbg != 0x0, "alloc fail!");
		hi_dbg = (uint64 *)calloc((lenX), sizeof(uint64));	ASSERT(HERE, hi_dbg != 0x0, "alloc fail!");
		mi64_set_eq(lo_dbg,x,lenX);
		mi64_set_eq(hi_dbg,y,lenY);
		mi64_div_binary(lo_dbg,hi_dbg,lenX,lenY,qref,rref);
	printf("mi64_div_mont: mi64_div_binary gives quotient  = %s\n",&s0[convert_mi64_base10_char(s0, qref, lenX, 0)]);
	printf("mi64_div_mont: mi64_div_binary gives remainder = %s\n",&s0[convert_mi64_base10_char(s0, rref, lenD, 0)]);
	}
  #endif

	if(lenX > lenv) {
		lenv = lenX;
		if(vsave) {
			free((void *)vsave);	vsave = 0x0;
		}
		vsave = (uint64 *)calloc((lenX), sizeof(uint64));	ASSERT(HERE, vsave != 0x0, "alloc fail!");
	}
	if(lenD > lens) {
		lens = lenD;
		if(scratch) {
			free((void *)scratch);	scratch = yinv = cy = tmp = itmp = lo = hi = w = rem_save = 0x0;
		}
		/* (re)Allocate the needed auxiliary storage: */
		scratch = (uint64 *)calloc((lenD*8), sizeof(uint64));	ASSERT(HERE, scratch != 0x0, "alloc fail!");
	}
	// These ptrs just point to various disjoint length-lenD sections of the shared local-storage chunk;
	// since some of them are treated as vars, reset 'em all on each entry, as well as re-zeroing the whole memblock:
	memset(scratch, 0ull, 8*(lenD<<3));	// (8*lenD) words of 8 bytes each
	// The 10-multiplier in the preceding calloc & memset corr. to number of pointers inited here:
	yinv         = scratch;
	cy           = scratch +   lenD;
	tmp          = scratch + 2*lenD;
	itmp         = scratch + 3*lenD;
	lo           = scratch + 4*lenD;
	hi           = scratch + 5*lenD;
	w            = scratch + 6*lenD;
	rem_save     = scratch + 7*lenD;

	// If x < y, no modding needed - copy x into remainder and set quotient = 0:
	if( (lenD > lenX) || (lenD == lenX && mi64_cmpult(x, y, lenX)) ) {
		/* If no modular reduction needed and X and R not the same array, set R = X: */
		if(r != 0x0 && r != x) {
			mi64_set_eq(r, x, lenX);
		}
		if(q) mi64_clear(q,lenX);
		return mi64_iszero(x, lenX);	// For x < y, y divides x iff x = 0
	// x and y have same number of machine words, x >= y (i.e. x does indeed need modding).
	// In such cases call binary-div to do modular reduction:
	} else if(lenD == lenX && !mi64_cmpult(x, y, lenX)) {
		// Here first attempt fast-mod based on FP approximation of the
		// quotient x/y, which only defaults to bin-div if x/y > 2^53:
		i = mi64_extract_lead64(x, lenX, &lo64);
		j = mi64_extract_lead64(y, lenX, &itmp64);
		bw = (uint64)1 << (i-j);
		fquo  = (double)bw;	// Power-of-2 scaling needed on ratio of left-justified lead64 fields
		fquo *= (double)lo64/(double)itmp64;
		if(r != 0x0 && fquo < TWO54FLOAT) {
			itmp64 = (uint64)fquo;
			// Since x,v,r may all point to same memory, need local-storage to hold y*fquo - use yinv to point to that:
			mi64_mul_scalar(y,itmp64,yinv,lenX);
			bw = mi64_sub(x,yinv,r,lenX);
		#if MI64_DIV_MONT
			if(dbg)printf("fquo*x = %s\n", &s0[convert_mi64_base10_char(s0, yinv, lenD, 0)]);
			if(dbg)printf("     r = %s\n", &s0[convert_mi64_base10_char(s0, r   , lenD, 0)]);
		#endif
			// If bw = 1 need an additional correction consisting of restoring += y.
			// (Example: x = 83647611300072717776350853439630961; y = 18705066715558216151)
			// The counter nc keeps track of number of correction steps needed:
			if(bw) {
				while( (nc < ncmax) &&  mi64_cmpult(y, r, lenX)) {	// Compare here must account for unsigned-int aliasing of r < 0
					nc++;	mi64_add(r,y,r,lenX);	--itmp64;	// Need to decr quotient by 1 to account for extra add-y
				}
			} else {	// Analogous correction in the opposite direction:
				while( (nc < ncmax) && !mi64_cmpult(r, y, lenX)) {
					nc++; mi64_sub(r,y,r,lenX);	++itmp64;	// Need to incr quotient by 1 to account for extra sub-y
				}
			}
			ASSERT(HERE, nc < ncmax, "Unexpectedly large number of corrections needed for floating-double quotient!");
			ASSERT(HERE, mi64_cmpult(r, y, lenX), "Remainder should be < modulus!");
			// At this point are done with x, so set low word of quotient array and clear rest:
			if(q) {
				mi64_clear(q, lenX);	q[0] = itmp64;
			}
			retval = mi64_iszero(r, lenX);
		#if MI64_DIV_MONT
			if(dbg) goto MI64_DIV_MONT_EXIT;
		#endif
		//	free((void *)yinv); yinv = 0x0;	** Apr 2015: No clue why I was allowing this to be freed **
		} else {	// (r == 0x0 || fquo >= TWO54FLOAT):
			retval = mi64_div_binary(x,y,lenX,lenY,q,r);
		}
		return retval;
	}

	/* Modulus y must be odd for Montgomery-style modmul to work, so first shift off any low 0s.
	Unlike the binary-result is-divisible-by routines, we must restore the shift to the
	quotient and remainder at the end. */
	nshift = mi64_trailz(y,lenD);
	if(nshift) {
		w = hi + lenD;
		// If need to right-justify x and y, make a copy of x and work with that.
		if(q) {		// If q-array supplied, use that for (x >> nshift):
			v = q;
		} else {	// Otherwise point to statically allocated storage attached to vsave pointer:
			v = vsave;
		}
		nws = (nshift+63)>>6;	// Any partial word counts as 1 here
		nbs = nshift&63;
		// Save the bottom nshift bits of x (we work with copy of x saved in v) prior to right-shifting:
		mi64_set_eq(rem_save, x, nws);
		mask = ((uint64)-1 >> (64 - nbs));
		rem_save[nws-1] &= mask;
		mi64_shrl(x,v,nshift,lenX);
		mi64_shrl(y,w,nshift,lenD);
	#if MI64_DIV_MONT
		if(dbg && (lenX <= 50))printf("x >> %u = %s\n",nshift, &s0[convert_mi64_base10_char(s0, v, lenX, 0)]);
		if(dbg)printf("y >> %u = %s\n",nshift, &s0[convert_mi64_base10_char(s0, w, lenD, 0)]);
	#endif
		// We don't bother trimming leading zeros in x, only the divisor y:
		lenS = mi64_getlen(w, lenD);	// Mnemonic: lenS = "Stripped length"
	} else {
		v = (uint64*)x;	// Otherwise just point v at x, since from here on v is treated as read-only (even if not declared so).
		w = (uint64*)y;	// ...and do similarly for w and y. Add explicit cast-to-non-const to silence GCC warns (and NVCC errors).
		// GCC: "warning: assignment discards qualifiers from pointer target type"; we wish C had a C++ - style <const_cast>.
		lenS = lenD;
	}
	hi = lo + lenS;	// *** lo:hi pointer pairs must be offset by amount reflecting #words in right-justified modulus! ***
  #if MI64_DIV_MONT
	if(dbg)printf("mi64_div_mont: setting hi = lo + lenS = %llX\n",(uint64)hi);
  #endif

	// If single-word odd-component divisor, use specialized single-word-divisor version:
	if(lenS == 1)
	{
		// F-way folded version of 64-bit DIV requires dividend length to be padded as needed to make it a multiple
		// of F, but take care of that via a bit of hackery within the mi64_div_by_scalar64_uF routines themselves:
		itmp64 = mi64_div_by_scalar64_u4(v, w[0], lenP, q);
		if(r != 0x0)
			r[0] = itmp64;
		// If we applied an initial right-justify shift to the modulus, restore the shift to the
		// current (partial) remainder and re-add the off-shifted part of the true remainder.
		if(nshift) {
			// rem = (rem << nshift) + rem_save:
			if(r != 0x0) {
				mi64_shl(r,r,nshift,lenD);	/*** Need to use non-right-justified length (rather than lenS) here! ***/
				// No carryout here since we are filling in the just-vacated low bits of rem with rem_save:
				mi64_add(r,rem_save,r,nws);
				mi64_set_eq(rem_save,r,lenD);	// Place copy of remainder into rem_save
			} else {
				/* If no rem-array supplied, do in-place add of shifted-scalar remainder to rem_save.
				Current partial odd-component divisor remainder in itmp64; this needs to be added
				into the proper location of rem_save, typically in two pieces.

				Ex1: nshift = 70. Then nws = (nshift+63)>>6 = 2, and nbs = nshift&63 = 8.
					The bottom (64 - nbs) = 56 bits of itmp64 get <<=  8 and ORed into rem_save[nws-1];
					the    top       nbs  =  8 bits of itmp64 get >>= 56 and assigned to rem_save[nws  ].

				Ex2: nshift = 64. Then nws = 1, nbs = 0, and we simply assign itmp64 to rem_save[nws].
				*/
				if(nbs != 0) {	// cf. Ex1 above
					rem_save[nws-1] |= itmp64<<    nbs ;
					rem_save[nws  ]  = itmp64>>(64-nbs);
				} else {		// cf. Ex2 above
					rem_save[nws  ]  = itmp64;
				}
			}
			// In either case (r == 0x0 or not) copy of remainder ends up in rem_save,
			// so quotient algo does not require caller to supply a remainder array.
		}
	#if 0
		printf("lenX, lenD = %u, %u\n", lenX, lenD);
		printf("q = %s\n", &cbuf[convert_mi64_base10_char(cbuf, q, lenX, 0)]);
		printf("r = %s\n", &cbuf[convert_mi64_base10_char(cbuf, rem_save, lenD, 0)]);
	#endif
	/***************************************************************************/
	} else {	// Multiword-divisor (specifically, odd component thereof) case:
	/***************************************************************************/
		if(mod_repeat) {
			// Reload the previously-computed mod-inverse:
			mi64_set_eq(yinv, mod_inv_save, lenS);
		} else {
			// Only save the modulus in multiword, lenX > lenD case:
			modD = lenD; mi64_set_eq(modulus_save, y, lenD);
			/*
			Find modular inverse (mod 2^nbits) of w in preparation for modular multiply.
			w must be odd for Montgomery-style modmul to work.

			Init yinv = 3*w ^ 2. This formula returns the correct bottom 4 bits of yinv,
			and we double the number of correct bits on each of the subsequent iterations.
			*/
			ASSERT(HERE, (w[0] & (uint64)1) == 1, "modulus must be odd!");
			ybits = lenS << 6;
			log2_numbits = ceil(log(1.0*ybits)/log(2.0));
			ASSERT(HERE, (w[0] & (uint64)1) == 1, "w must be odd!");
			mi64_clear(yinv, lenS);
			yinv[0] = (w[0] + w[0] + w[0]) ^ (uint64)2;

			/* Newton iteration involves repeated steps of form

				yinv = yinv*(2 - w*yinv);

			Number of significant bits at the bottom doubles on each iteration, starting from 4 for the initial seed
			defined as yinv_0 = 3*w ^ 2. The doubling continues until we reach the bitwidth set by the MULL operation.
			*/
			for(j = 2; j < 6; j++)	/* At each step, have 2^j correct low-order bits in yinv */
			{
				lo64 = w[0]*yinv[0];
				yinv[0] = yinv[0]*((uint64)2 - lo64);
			}

			/* Now that have bottom 64 = 2^6 bits of yinv, do as many more Newton iterations as needed to get the full [ybits] of yinv.
			Number of good bits doubes each iteration, and we only need to compute the new good bits, via

				yinv.hi = MULL(-yinv.lo, MULL(w.hi, yinv.lo) + UMULH(w.lo, yinv.lo))	//========== do this optimization later===========

			where lo = converged bits, and hi = bits converged on current iteration
			*/
			i = 1;	// I stores number of converged 64-bit words
			for(j = 6; j < log2_numbits; j++, i <<= 1) {
				mi64_mul_vector_lo_half(w, yinv,tmp, lenS);
				mi64_nega              (tmp,tmp, lenS);
				bw = mi64_add_scalar(tmp, 2ull,tmp, lenS);	ASSERT(HERE, !bw, "");
				mi64_mul_vector_lo_half(yinv,tmp, yinv, lenS);
			}
			// Save inverse in case next call uses same modulus:
			mi64_set_eq(mod_inv_save, yinv, lenS);
		}

		// Check the computed inverse:
		mi64_mul_vector_lo_half(w, yinv, tmp, lenS);
		ASSERT(HERE, mi64_cmp_eq_scalar(tmp, 1ull, lenS), "Bad Montmul inverse!");
	#if MI64_DIV_MONT
		if(dbg)printf("yinv = %s\n", &s0[convert_mi64_base10_char(s0, yinv, lenS, 0)]);
	#endif

		// Process the x-array data in lenS-sized chunks. If last chunk < lenS-sized it gets special handling.
		// Number of lenS-sized chunks to do (incl. 0-padding of x if its number of machine words not an exact multiple of lenS):
	#if MI64_DIV_MONT
		if(dbg)printf("Starting remainder loop: cy = %s\n",&s0[convert_mi64_base10_char(s0, hi, lenS, 0)]);
	#endif
		lenW = (lenX+lenS-1)/lenS;
		for(i = 0; i < lenX-lenS+1; i += lenS)
		{
		#if MI64_DIV_MONT
			if(dbg)printf("i = %u; v = %s\n", i,&s0[convert_mi64_base10_char(s0, v+i, lenS, 0)]);
		#endif
			/* Add w if had a borrow - Since we low-half multiply tmp by yinv below and w*yinv == 1 (mod 2^64),
			   can simply add 1 to tmp*yinv result instead of adding w to the difference here: */
			bw = mi64_sub(v+i,hi,tmp,lenS);
//******* where is hi inited here ?? *********
			// Compute expected value of low-half of MUL_LOHI for sanity-checking
			if(bw) {
				mi64_add(tmp,w,itmp,lenS);	// itmp = tmp + w
			} else {
				mi64_set_eq(itmp,tmp,lenS);	// itmp = tmp
			}
		#if MI64_DIV_MONT
			if(dbg)printf("v-cy = %s, bw = %llu\n", &s0[convert_mi64_base10_char(s0, tmp, lenS, 0)], bw);
		#endif

			// Now do the Montgomery mod: cy = umulh( w, mull(tmp, yinv) );
			mi64_mul_vector_lo_half(tmp,yinv,tmp, lenS);	// tmp = tmp*yinv + bw;
		#if MI64_DIV_MONT
			if(dbg)printf("MULL = %s\n", &s0[convert_mi64_base10_char(s0, tmp, lenS, 0)]);
		#endif
			// bw = 0 or 1, but may propagate all the way into high word:
			ASSERT(HERE, 0ull == mi64_add_scalar(tmp,bw, tmp, lenS), "tmp += bw has carryout!");
			// Do double-wide product. Fast-divisibility test needs just high half (stored in hi); low half (lo) useful to extract true-mod
			mi64_mul_vector(tmp,lenS,w,lenS,lo, (uint32*)&j);	// lo:hi = MUL_LOHI(q, tmp)

		#if MI64_DIV_MONT
			if(dbg)printf("  lo = %s\n", &s0[convert_mi64_base10_char(s0,   lo, lenS, 0)]);
			if(dbg)printf("  hi = %s\n", &s0[convert_mi64_base10_char(s0,   hi, lenS, 0)]);
		#endif

			if(!mi64_cmp_eq(lo,itmp,lenS)) {
			#if MI64_DIV_MONT
				if(dbg)printf("itmp = %s\n", &s0[convert_mi64_base10_char(s0, itmp, lenS, 0)]);
			#endif
				ASSERT(HERE, 0, "Low-half product check mismatch!");
			}
		}

		// Last term gets special handling:
		// Zero-pad the final x-array section (by copying into cy and zeroing remaining higher-order terms of cy) if lenX != 0 (mod lenS):
		j = lenX-i;
		if(j) {
			mi64_set_eq(cy,v+i,j);	// Set cy = {x[i],x[i+1],...,x[lenX-1],0,...,0}
		#if MI64_DIV_MONT
		//	if(dbg)printf("i+ = %u; v = %s\n", i,&s0[convert_mi64_base10_char(s0, cy, j, 0)]);	// use 'i+' here to indicate this is the post-loop code
		#endif
			for(i = j; i < lenS; i++) {
				cy[i] = 0ull;
			}
			// only need the low half of the final product, which we can obtain sans explicit MUL:
			bw = mi64_sub(cy,hi,cy,lenS);
			if(bw) {
				mi64_add(cy,w,cy,lenS);	// cy += w
			}
		} else {
			mi64_set_eq(cy,lo,lenS);	// cy = lo
		}
	#if MI64_DIV_MONT
		if(dbg)printf("MR = %s\n", &s0[convert_mi64_base10_char(s0, cy, lenS, 0)]);	// MR = "Montgomery remainder"
	#endif

	//----------------------------------

		if(mod_repeat) {
			mi64_set_eq(tmp, basepow_save, lenS);
		} else {
			// Prepare to transform back out of "Montgomery space" ... first compute B^2 mod q.
			// B^2 overflows our double-wide scratch [lo]-array field, so compute B^2/2 mod q...
			mi64_clear(lo,2*lenS);	lo[2*lenS-1] = 0x8000000000000000ull;
			mi64_div_binary(lo,w,2*lenS,lenS,0,tmp);	// B^2/2 mod q returned in tmp
			// ...and mod-double the result:
			itmp64 = mi64_shl_short(tmp,tmp,1,lenS);
			if(itmp64 || mi64_cmpugt(tmp,w,lenS)) {
				mi64_sub(tmp,w,tmp,lenS);
			}
		#if MI64_DIV_MONT
		if(dbg) {
			printf("B^2 mod q = %s\n", &s0[convert_mi64_base10_char(s0, tmp, lenS, 0)]);
		}
		#endif
			/* tmp holds B^2 mod q - Now compute sequence of powers needed to obtain B^len mod q via Montgomery-muls.
			See the 64-bit-modulus-specialized version of this routine in mi64_div_by_scalar64() for details.
			*/
			p = lenW;

			// If p == 2, tmp already contains the needed power
			if(p == 3) {
				MONT_SQR_N(tmp,lo,w,yinv, tmp,lenS);
			}
			else if(p > 3) {
				n = 0;		// Init the bitstring
				for(j = 0; p > 5; j++)		// j counts number of bits processed
				{
					BIT_SETC(n,j,IS_EVEN(p));
					// Each M-mul includes another inverse power B^(-1) with the product, so we add 1 to the
					// current power p after each halving step here to account for that:
					p = (p >> 1) + 1;
				}
				ASSERT(HERE, j <= 32, "Need 64-bit bitstring!");
				/*
				Now do the needed powering. We always start with p = 2 and M-square that to get p = 3:
				*/
				MONT_SQR_N(tmp,lo,w,yinv,itmp,lenS);	// tmp has p = 2, itmp has p = 3
		//	printf("B^3 mod q = %s\n", &s0[convert_mi64_base10_char(s0, itmp, lenS, 0)]);

				/* Starting off we have the following 2 choices:
					A. Mp(2,3) -> p=4;
					B. Mp(3,3) -> p=5,
				*/
				if(p == 4) {
					MONT_MUL_N(itmp,tmp,lo,w,yinv,tmp,lenS);
		//	printf("B^4 mod q = %s\n", &s0[convert_mi64_base10_char(s0, tmp, lenS, 0)]);
				} else if(p == 5) {
					MONT_SQR_N(itmp,lo,w,yinv,tmp,lenS);
		//	printf("B^5 mod q = %s\n", &s0[convert_mi64_base10_char(s0, tmp, lenS, 0)]);
				} else {
					ASSERT(HERE, 0,"Bad starting value for power p!");
				}
				for(i = j-1; i >= 0; i--) {
					if(BIT_TEST(n,i)) {
						mi64_set_eq(itmp,tmp,lenS);	// itmp = tmp
		//	printf("B^** Copy = %s\n", &s0[convert_mi64_base10_char(s0, itmp, lenS, 0)]);
						MONT_UNITY_MUL_N(itmp,w,yinv,itmp,lenS);	// Reduce power of B by 1 in one of the 2 multiplicands...
		//	printf(".../B mod q = %s\n", &s0[convert_mi64_base10_char(s0, itmp, lenS, 0)]);
						MONT_MUL_N(itmp,tmp,lo,w,yinv,tmp,lenS);	// ...and multiply `em.
					} else {
						MONT_SQR_N(tmp,lo,w,yinv,tmp,lenS);
					}
				}
			}
			// Save base power in case next call uses same modulus:
			mi64_set_eq(basepow_save, tmp, lenS);
		}

		/*
		Now multiply the Montgomery residue (in cy) from the mod-loop by the mod-power of the base (tmp).
		Properly scaled, right-justified [RJ] remainder output in cy ... need the RJ remainder
		if doing a further quotient computation anyway, and since function allows x/r-pointers
		to refer to same memloc, keep remainder in local-array cy for now, defer copy-to-r until last:
		*/
		MONT_MUL_N(cy,tmp,lo,w,yinv,cy,lenS);
	#if MI64_DIV_MONT
		if(dbg && lenW > 2)printf("B^%u mod q = %s\n", lenW,&s0[convert_mi64_base10_char(s0, tmp, lenS, 0)]);
		if(dbg)printf("Remainder = %s\n", &s0[convert_mi64_base10_char(s0,cy, lenS, 0)]);
	#endif

	//----------------------------------

		// If q-array supplied, compute quotient and return it in that:
		if(q) {
		#if MI64_DIV_MONT
			if(dbg)printf("Computing quotient...\n");
		#endif
			// If even modulus, right-justified copy of input array already in v.
			// Now can use a simple loop and a sequence of word-size MULLs to obtain quotient.
			// Fusing the functionality of mi64_sub_scalar and the quotient extraction is fastest here:
			bw = 0;
			mi64_set_eq(hi,cy,lenS);	// Copy remainder (stored in cy) into hi-array
			for(i = 0; i < lenX-lenS+1; i += lenS)
			{
			#if MI64_DIV_MONT
				if(dbg)printf("i = %u; v = %s\n", i,&s0[convert_mi64_base10_char(s0, v+i, lenS, 0)]);
			#endif
				bw = mi64_sub(v+i,hi,tmp,lenS);	// tmp = x[i] - (bw+cy);

				// Compute expected value of low-half of MUL_LOHI for sanity-checking
				mi64_set_eq(itmp,tmp,lenS);	// itmp = tmp

				// Now do the Montgomery mod: cy = umulh( y, mull(tmp, yinv) );
				mi64_mul_vector_lo_half(tmp,yinv,tmp, lenS);	// tmp = tmp*yinv + bw;
			#if MI64_DIV_MONT
				if(dbg)printf("tmp*yinv = %s, bw = %llu\n", &s0[convert_mi64_base10_char(s0, tmp, lenS, 0)], bw);
			#endif
				// Do double-wide product. Fast-divisibility test needs just high half (stored in hi); low half (lo) useful to extract true-mod
				mi64_mul_vector(tmp,lenS,w,lenS,lo, (uint32*)&j);	// lo:hi = MUL_LOHI(q, tmp); cy is in hi half
				hi[0] += bw;	ASSERT(HERE, hi[0] >= bw, "bw!");// (cy + bw); Since bw = 0 or 1, check that bw=1 does not propagate is simply check (sum >= bw)

			#if MI64_DIV_MONT
				if(dbg)printf("  lo = %s\n", &s0[convert_mi64_base10_char(s0,   lo, lenS, 0)]);
				if(dbg)printf("  hi = %s\n", &s0[convert_mi64_base10_char(s0,   hi, lenS, 0)]);
			#endif

				if(!mi64_cmp_eq(lo,itmp,lenS)) {
				#if MI64_DIV_MONT
					printf("itmp = %s\n", &s0[convert_mi64_base10_char(s0, itmp, lenS, 0)]);
				#endif
					ASSERT(HERE, 0, "Low-half product check mismatch!");
				}
				mi64_set_eq(q+i,tmp,lenS);	// Equivalent to the y[i] = tmp step of the scalar routine
			}
			// Any words remaining due to lenS not exactly dividing lenX are guaranteed to end up = 0, use that as a sanity check:
			j = lenX-i;
			if(j) {
				// Check cy = {v[i],v[i+1],...,v[lenX-1],0,...,0}
				if(!mi64_cmp_eq(hi,v+i,j)) {
					ASSERT(HERE, mi64_cmp_eq(hi,v+i,j), "cy check!");
				}
				mi64_clear(q+i,j);	// Do after above check since v may == q
				for(i = j; i < lenS; i++) {
					ASSERT(HERE, hi[i] == 0ull, "cy check!");
				}
			}
		#if MI64_DIV_MONT
			if(dbg) {
			//	if(lenX <= 50)printf("q = %s\n", &s0[convert_mi64_base10_char(s0, q, lenX, 0)]);
				printf("q = %s\n", &str_10k[__convert_mi64_base10_char(str_10k, 10<<10, q, lenX, 0)]);
			}
		#endif
		}

		// Copy remainder from local cy-array to output r-array (which may == x).
		// If we applied an initial right-justify shift to the modulus, restore the shift to the
		// current (partial) remainder and re-add the off-shifted part of the true remainder.
		//
		// Oct 2015: remainder ends up in rem_save, since no longer require user to supply a rem-array:
		if(nshift) {
			// rem = (rem << nshift) + rem_save:
			mi64_shl(cy,cy,nshift,lenD);	/*** Need to use non-right-justified length (rather than lenS) here! ***/
			// No carryout here since we are filling in the just-vacated low bits of rem with rem_save:
			mi64_add(cy,rem_save,rem_save,nws);
		} else {
			mi64_set_eq(rem_save,cy,lenD);	// Save copy of cy in rem_save
		}
		if(r != 0x0) mi64_set_eq(r,rem_save,lenD);	// r = rem_save
	}	// (lenS == 1)?

	retval = mi64_iszero(rem_save, lenD);

  #if MI64_DIV_MONT
	MI64_DIV_MONT_EXIT:
	if(dbg)	// Compute result using slow binary-div algo, use that as reference
	{
		if(!mi64_cmp_eq(rref,r,lenY)) {
			printf("rref = %s\n", &s0[convert_mi64_base10_char(s0, rref, lenD, 0)]);
			printf("rewm = %s\n", &s0[convert_mi64_base10_char(s0, r   , lenD, 0)]);
			ASSERT(HERE, 0, "bzzt!\n");
		}
		if(!mi64_cmp_eq(qref,q,lenX)) {
			printf("qref = %s\n", &s0[convert_mi64_base10_char(s0, qref, lenX, 0)]);
			printf("qewm = %s\n", &s0[convert_mi64_base10_char(s0, q   , lenX, 0)]);
			ASSERT(HERE, 0, "bzzt!\n");
		}
	
		free((void *)qref); qref = 0x0;
		free((void *)rref);	rref = 0x0;
		free((void *)lo_dbg); lo_dbg = 0x0;
		free((void *)hi_dbg); hi_dbg = 0x0;
	}
  #endif
	return retval;
}
#endif	// __CUDA_ARCH__ ?

/*
Slow bit-at-a-time method to get quotient q = x/y and remainder r = x%y.
[optional] Output Q-array:
			if supplied, must have dimension at least as large as that of X-array, even if actual  quotient is smaller.
[optional] Output R-array:
			if supplied, must have dimension at least as large as that of Y-array, even if actual remainder is smaller.

Returns: 1 if x divisible by y (remainder = 0), 0 otherwise.
*/
#if MI64_DEBUG
	#define MI64_DIV_DBG	0
#endif
#ifndef __CUDA_ARCH__
int mi64_div_binary(const uint64 x[], const uint64 y[], uint32 lenX, uint32 lenY, uint64 q[], uint64 r[])
{
  #if MI64_DIV_DBG
	uint32 dbg = 0;//(lenX== 6 && lenY==3) && STREQ(&s0[convert_mi64_base10_char(s0, y, lenY, 0)], "53625112691923843508117942311516428173021903300344567");
  #endif
	int i, nshift;
	uint32 lz_x, lz_y, xlen, ylen, max_len;
	// pointers to local storage:
	static uint64 *xloc = 0x0, *yloc = 0x0;
	static uint64 *scratch = 0x0;	// "base pointer" for local storage shared by all of the above subarrays
	static int lens = 0;	// # of 64-bit ints allocated for current scratch space

  #if MI64_DIV_DBG
	if(dbg) {
		printf("mi64_div_binary: x = %s, y = %s\n",&s0[convert_mi64_base10_char(s0, x, lenX, 0)],&s1[convert_mi64_base10_char(s1, y, lenY, 0)]);
	}
  #endif
	ASSERT(HERE, lenX && lenY, "illegal 0 dimension!");
	ASSERT(HERE, x && y, "At least one of X, Y is null!");
	ASSERT(HERE, x != y, "X and Y arrays overlap!");
	ASSERT(HERE, r != y, "Y and Rem arrays overlap!");
	ASSERT(HERE, q != x && q != y && (q == 0x0 || q != r), "Quotient array overlaps one of X, Y ,Rem!");

	/* Init Q = 0; don't do similarly for R since we allow X and R to point to same array: */
	if(q && (q != x)) {
		mi64_clear(q, lenX);
	}
	/* And now find the actual lengths of the divide operands and use those for the computation: */
	xlen = mi64_getlen(x, lenX);
	ylen = mi64_getlen(y, lenY);
	ASSERT(HERE, ylen != 0, "divide by 0!");

	/* Allocate the needed auxiliary storage: */
	if(lens < (xlen << 1)) {
		lens = (xlen << 1);	// Alloc yloc same as x to allow for left-justification of y-copy
		scratch = (uint64 *)realloc(scratch, lens*sizeof(uint64));	ASSERT(HERE, scratch != 0x0, "alloc fail!");
	}

	// If x < y, no modding needed - copy x into remainder and set quotient = 0:
	max_len = MAX(xlen, ylen);
	if((xlen < ylen) || ((xlen == ylen) && mi64_cmpult(x, y, max_len)) ) {
		/* If no modular reduction needed and X and R not the same array, set R = X: */
		if(r != 0x0 && r != x) {
			mi64_set_eq(r, x, lenX);
		}
		if(q) mi64_clear(q,lenX);
		return mi64_iszero(x, lenX);	// For x < y, y divides x iff x = 0
	}

	// Since xlen,ylen subject to data-dependent dynamic adjustment during this routine,
	// must set ptr-offsets based on user-specified lengths:
	xloc = scratch       ;	mi64_set_eq(xloc, x, lenX);
	yloc = scratch + lenX;	mi64_set_eq(yloc, y, lenY);	mi64_clear(yloc+lenY,lenX-lenY);

	lz_x = mi64_leadz(xloc, max_len);
	lz_y = mi64_leadz(yloc, max_len);
	nshift = lz_y - lz_x;
	ASSERT(HERE, nshift >= 0, "nshift < 0");

	/* Left-justify the modulus (copy) y to match x's leading bit: */
	mi64_shl(yloc, yloc, nshift, max_len);	ylen = max_len;
	for(i = nshift; i >= 0; --i) {
	#if MI64_DIV_DBG
		if(dbg)printf("I = %3d: r = %s, yshift = %s\n", i,&s0[convert_mi64_base10_char(s0, xloc, max_len, 0)],&s1[convert_mi64_base10_char(s1, yloc, max_len, 0)]);
	#endif
		if(mi64_cmpuge(xloc, yloc, max_len)) {
			ASSERT(HERE, xlen == max_len,"xlen != max_len");
			mi64_sub(xloc, yloc, xloc, max_len);	/* r -= yshift */
			ASSERT(HERE, mi64_cmpult(xloc, yloc, max_len),"r >= yshift");
			xlen = mi64_getlen(xloc, max_len);
			if(q) {
				mi64_set_bit(q, i);
			}
		}
		if(i > 0) {
			mi64_shrl(yloc, yloc, 1, ylen);
			ylen = mi64_getlen(yloc, ylen);
		}
		max_len = MAX(xlen, ylen);
	}
	// Remainder in xloc - do some sanity checks prior to copying into r[]:
	xlen = mi64_getlen(xloc, lenX);
	ASSERT(HERE, xlen <= ylen && mi64_cmpugt(y,xloc,ylen), "Remainder should be < modulus!");
	if(r != 0x0) {
		mi64_set_eq(r, xloc, ylen);
		mi64_clear(r+ylen,lenY-ylen);
	}
	/* Final value of yloc is unchanged from its (unshifted) starting value == y */
	ASSERT(HERE, mi64_cmp_eq(yloc,y,ylen), "Final value of y-copy differs from original!");
  #if MI64_DIV_DBG
	if(dbg) {
		if(q)printf("mi64_div_binary: quotient  = %s\n",&s0[convert_mi64_base10_char(s0, q, lenX, 0)]);
		printf("mi64_div_binary: remainder = %s\n",&s0[convert_mi64_base10_char(s0, r, lenX, 0)]);
		printf("\n");
	}
  #endif
	return mi64_iszero(xloc, lenX);	// Return 1 if y divides x, 0 if not.
}
#endif	// __CUDA_ARCH__ ?

/// Fast is-divisible-by-32-bit-scalar using Montgomery modmul and right-to-left modding:
/*** NOTE *** Routine assumes x[] is a uint64 array cast to uint32[], hence the doubling-of-len
is done HERE, i.e. user must supply uint64-len just as for the 'true 64-bit' mi64 functions!
In other words, these kinds of compiler warnings are expected:

	mi64.c: In function ‘mi64_div_y32’:
	warning: passing argument 1 of ‘mi64_is_div_by_scalar32’ from incompatible pointer type
	note: expected ‘const uint32 *’ but argument is of type ‘uint64 *’
*/
#ifdef __CUDA_ARCH__
__device__
#endif
int mi64_is_div_by_scalar32(const uint32 x[], uint32 q, uint32 len)
{
	uint32 i,j,nshift,dlen,qinv,tmp,cy;

	ASSERT(HERE, q > 0, "mi64_is_div_by_scalar32: 0 modulus!");
	if(q == 1) return TRUE;
	if(len == 0) return TRUE;

	/* q must be odd for Montgomery-style modmul to work, so first shift off any low 0s: */
	nshift = trailz32(q);
	if(nshift) {
		if(trailz32(x[0]) < nshift) return FALSE;
		q >>= nshift;
	}

	qinv = (q+q+q) ^ (uint32)2;
	for(j = 0; j < 3; j++) {
		qinv = qinv*((uint32)2 - q*qinv);
	}
	cy = (uint32)0;
	dlen = len+len;	/* Since are processing a uint64 array cast to uint32[], double the #words parameter */
	for(i = 0; i < dlen; ++i) {
		tmp  = x[i] - cy;
		/* Add q if had a borrow - Since we low=half multiply tmp by qinv below and q*qinv == 1 (mod 2^32),
		   can simply add 1 to tmp*qinv result instead of adding q to the difference here:
		*/
		cy = (cy > x[i]); /* Comparing this rather than (tmp > x[i]) frees up tmp for the multiply */
		/* Now do the Montgomery mod: cy = umulh( q, mulq(tmp, qinv) ); */
		tmp *= qinv;
		tmp += cy;
		MULH32(q,tmp, cy);
	}
	return (cy == 0);
}

/* Same as above, but assumes q and its modular inverse have been precomputed: */
int		mi64_is_div_by_scalar32p(const uint32 x[], uint32 q, uint32 qinv, uint32 len)
{
	uint32 i,dlen,tmp,cy;

	ASSERT(HERE, qinv == qinv*((uint32)2 - q*qinv), "mi64_is_div_by_scalar32p: bad qinv!");
	cy = (uint32)0;
	dlen = len+len;	/* Since are processing a uint64 array cast to uint32[], double the #words parameter */
	for(i = 0; i < dlen; ++i) {
		tmp  = x[i] - cy;
		/* Add q if had a borrow - Since we low=half multiply tmp by qinv below and q*qinv == 1 (mod 2^32),
		   can simply add 1 to tmp*qinv result instead of adding q to the difference here:
		*/
		cy = (cy > x[i]); /* Comparing this rather than (tmp > x[i]) frees up tmp for the multiply */
		/* Now do the Montgomery mod: cy = umulh( q, mulq(tmp, qinv) ); */
		tmp *= qinv;
		tmp += cy;
		MULH32(q,tmp, cy);
	}
	return (cy == 0);
}

// Same as above, but 8-fold on the dividend side, same divisor:
int		mi64_is_div_by_scalar32p_x8(
	const uint32 a[],
	const uint32 b[],
	const uint32 c[],
	const uint32 d[],
	const uint32 e[],
	const uint32 f[],
	const uint32 g[],
	const uint32 h[],
	uint32 q, uint32 qinv, uint32 len, uint32*sum)
{
	int retval=0;
	uint32 tmp0,tmp1,tmp2,tmp3,tmp4,tmp5,tmp6,tmp7,cy0,cy1,cy2,cy3,cy4,cy5,cy6,cy7;
	cy0 = cy1 = cy2 = cy3 = cy4 = cy5 = cy6 = cy7 = (uint32)0;

	ASSERT(HERE, qinv == qinv*((uint32)2 - q*qinv), "mi64_is_div_by_scalar32p: bad qinv!");

	tmp0 = a[0] * qinv;
	tmp1 = b[0] * qinv;
	tmp2 = c[0] * qinv;
	tmp3 = d[0] * qinv;
	tmp4 = e[0] * qinv;
	tmp5 = f[0] * qinv;
	tmp6 = g[0] * qinv;
	tmp7 = h[0] * qinv;

	MULH32(q,tmp0, cy0);
	MULH32(q,tmp1, cy1);
	MULH32(q,tmp2, cy2);
	MULH32(q,tmp3, cy3);
	MULH32(q,tmp4, cy4);
	MULH32(q,tmp5, cy5);
	MULH32(q,tmp6, cy6);
	MULH32(q,tmp7, cy7);

	tmp0 = a[1] - cy0;
	tmp1 = b[1] - cy1;
	tmp2 = c[1] - cy2;
	tmp3 = d[1] - cy3;
	tmp4 = e[1] - cy4;
	tmp5 = f[1] - cy5;
	tmp6 = g[1] - cy6;
	tmp7 = h[1] - cy7;
	/* Add q if had a borrow - Since we low=half multiply tmp by qinv below and q*qinv == 1 (mod 2^32),
	   can simply add 1 to tmp*qinv result instead of adding q to the difference here:
	*/
	cy0 = (cy0 > a[1]);
	cy1 = (cy1 > b[1]);
	cy2 = (cy2 > c[1]);
	cy3 = (cy3 > d[1]);
	cy4 = (cy4 > e[1]);
	cy5 = (cy5 > f[1]);
	cy6 = (cy6 > g[1]);
	cy7 = (cy7 > h[1]);
	/* Now do the Montgomery mod: cy = umulh( q, mulq(tmp, qinv) ); */
	tmp0 *= qinv;
	tmp1 *= qinv;
	tmp2 *= qinv;
	tmp3 *= qinv;
	tmp4 *= qinv;
	tmp5 *= qinv;
	tmp6 *= qinv;
	tmp7 *= qinv;

	tmp0 += cy0;
	tmp1 += cy1;
	tmp2 += cy2;
	tmp3 += cy3;
	tmp4 += cy4;
	tmp5 += cy5;
	tmp6 += cy6;
	tmp7 += cy7;

	MULH32(q,tmp0, cy0);
	MULH32(q,tmp1, cy1);
	MULH32(q,tmp2, cy2);
	MULH32(q,tmp3, cy3);
	MULH32(q,tmp4, cy4);
	MULH32(q,tmp5, cy5);
	MULH32(q,tmp6, cy6);
	MULH32(q,tmp7, cy7);

	retval += (cy0 == 0);
	retval += (cy1 == 0) << 1;
	retval += (cy2 == 0) << 2;
	retval += (cy3 == 0) << 3;
	retval += (cy4 == 0) << 4;
	retval += (cy5 == 0) << 5;
	retval += (cy6 == 0) << 6;
	retval += (cy7 == 0) << 7;
	*sum = cy0+cy1+cy2+cy3+cy4+cy5+cy6+cy7;
	return retval;
}

#ifndef __CUDA_ARCH__
uint32	mi64_is_div_by_scalar32_x4(const uint32 x[], uint32 q0, uint32 q1, uint32 q2, uint32 q3, uint32 len)
{
	uint32 i,j,nshift0,nshift1,nshift2,nshift3;
	uint32 retval=0,dlen = len+len, qinv0,qinv1,qinv2,qinv3,tmp0,tmp1,tmp2,tmp3,cy0,cy1,cy2,cy3;
	uint32 xcur,trailx;

	ASSERT(HERE, q0 && q1 && q2 && q3, "mi64_is_div_by_scalar32_x4: 0 modulus!");
	if(q0 + q1 + q2 + q3 == 4) return TRUE;
	if(len == 0) return TRUE;

	trailx = trailz32(x[0]);

	/* q must be odd for Montgomery-style modmul to work, so first shift off any low 0s: */
	nshift0 = trailz32(q0);
	nshift1 = trailz32(q1);
	nshift2 = trailz32(q2);
	nshift3 = trailz32(q3);

	q0 >>= nshift0;
	q1 >>= nshift1;
	q2 >>= nshift2;
	q3 >>= nshift3;

	qinv0 = (q0+q0+q0) ^ (uint32)2;
	qinv1 = (q1+q1+q1) ^ (uint32)2;
	qinv2 = (q2+q2+q2) ^ (uint32)2;
	qinv3 = (q3+q3+q3) ^ (uint32)2;
	for(j = 0; j < 3; j++) {
		qinv0 = qinv0*((uint32)2 - q0*qinv0);
		qinv1 = qinv1*((uint32)2 - q1*qinv1);
		qinv2 = qinv2*((uint32)2 - q2*qinv2);
		qinv3 = qinv3*((uint32)2 - q3*qinv3);
	}
	cy0 = (uint32)0;
	cy1 = (uint32)0;
	cy2 = (uint32)0;
	cy3 = (uint32)0;
	for(i = 0; i < dlen; ++i) {
		xcur = x[i];

		tmp0 = xcur - cy0;
		tmp1 = xcur - cy1;
		tmp2 = xcur - cy2;
		tmp3 = xcur - cy3;
		/* Add q if had a borrow - Since we low=half multiply tmp by qinv below and q*qinv == 1 (mod 2^32),
		   can simply add 1 to tmp*qinv result instead of adding q to the difference here:
		*/
		cy0 = (cy0 > xcur);
		cy1 = (cy1 > xcur);
		cy2 = (cy2 > xcur);
		cy3 = (cy3 > xcur);
		/* Now do the Montgomery mod: cy = umulh( q, mulq(tmp, qinv) ); */
		tmp0 *= qinv0;
		tmp1 *= qinv1;
		tmp2 *= qinv2;
		tmp3 *= qinv3;

		tmp0 += cy0;
		tmp1 += cy1;
		tmp2 += cy2;
		tmp3 += cy3;
		MULH32(q0,tmp0, cy0);
		MULH32(q1,tmp1, cy1);
		MULH32(q2,tmp2, cy2);
		MULH32(q3,tmp3, cy3);
	}
	retval += ((cy0 == 0) && (nshift0 <= trailx));
	retval += ((cy1 == 0) && (nshift1 <= trailx)) << 1;
	retval += ((cy2 == 0) && (nshift2 <= trailx)) << 2;
	retval += ((cy3 == 0) && (nshift3 <= trailx)) << 3;
	return retval;
}

uint32	mi64_is_div_by_scalar32_x8(const uint32 x[], uint32 q0, uint32 q1, uint32 q2, uint32 q3, uint32 q4, uint32 q5, uint32 q6, uint32 q7, uint32 len)
{
	uint32 i,j,nshift0,nshift1,nshift2,nshift3,nshift4,nshift5,nshift6,nshift7;
	uint32 retval=0,dlen = len+len, qinv0,qinv1,qinv2,qinv3,qinv4,qinv5,qinv6,qinv7,tmp0,tmp1,tmp2,tmp3,tmp4,tmp5,tmp6,tmp7,cy0,cy1,cy2,cy3,cy4,cy5,cy6,cy7;
	uint32 xcur,trailx;

	ASSERT(HERE, q0 && q1 && q2 && q3 && q4 && q5 && q6 && q7, "mi64_is_div_by_scalar32_x8: 0 modulus!");
	if(q0 + q1 + q2 + q3 + q4 + q5 + q6 + q7 == 8) return TRUE;
	if(len == 0) return TRUE;

	trailx = trailz32(x[0]);

	/* q must be odd for Montgomery-style modmul to work, so first shift off any low 0s: */
	nshift0 = trailz32(q0);						nshift4 = trailz32(q4);
	nshift1 = trailz32(q1);						nshift5 = trailz32(q5);
	nshift2 = trailz32(q2);						nshift6 = trailz32(q6);
	nshift3 = trailz32(q3);						nshift7 = trailz32(q7);

	q0 >>= nshift0;								q4 >>= nshift4;
	q1 >>= nshift1;								q5 >>= nshift5;
	q2 >>= nshift2;								q6 >>= nshift6;
	q3 >>= nshift3;								q7 >>= nshift7;

	qinv0 = (q0+q0+q0) ^ (uint32)2;				qinv4 = (q4+q4+q4) ^ (uint32)2;
	qinv1 = (q1+q1+q1) ^ (uint32)2;				qinv5 = (q5+q5+q5) ^ (uint32)2;
	qinv2 = (q2+q2+q2) ^ (uint32)2;				qinv6 = (q6+q6+q6) ^ (uint32)2;
	qinv3 = (q3+q3+q3) ^ (uint32)2;				qinv7 = (q7+q7+q7) ^ (uint32)2;
	for(j = 0; j < 3; j++) {
		qinv0 = qinv0*((uint32)2 - q0*qinv0);	qinv4 = qinv4*((uint32)2 - q4*qinv4);
		qinv1 = qinv1*((uint32)2 - q1*qinv1);	qinv5 = qinv5*((uint32)2 - q5*qinv5);
		qinv2 = qinv2*((uint32)2 - q2*qinv2);	qinv6 = qinv6*((uint32)2 - q6*qinv6);
		qinv3 = qinv3*((uint32)2 - q3*qinv3);	qinv7 = qinv7*((uint32)2 - q7*qinv7);
	}
	cy0 = (uint32)0;							cy4 = (uint32)0;
	cy1 = (uint32)0;							cy5 = (uint32)0;
	cy2 = (uint32)0;							cy6 = (uint32)0;
	cy3 = (uint32)0;							cy7 = (uint32)0;
	for(i = 0; i < dlen; ++i) {
		xcur = x[i];

		tmp0 = xcur - cy0;						tmp4 = xcur - cy4;
		tmp1 = xcur - cy1;						tmp5 = xcur - cy5;
		tmp2 = xcur - cy2;						tmp6 = xcur - cy6;
		tmp3 = xcur - cy3;						tmp7 = xcur - cy7;

		cy0 = (cy0 > xcur);						cy4 = (cy4 > xcur);
		cy1 = (cy1 > xcur);						cy5 = (cy5 > xcur);
		cy2 = (cy2 > xcur);						cy6 = (cy6 > xcur);
		cy3 = (cy3 > xcur);						cy7 = (cy7 > xcur);

		tmp0 *= qinv0;							tmp4 *= qinv4;
		tmp1 *= qinv1;							tmp5 *= qinv5;
		tmp2 *= qinv2;							tmp6 *= qinv6;
		tmp3 *= qinv3;							tmp7 *= qinv7;

		tmp0 += cy0;							tmp4 += cy4;
		tmp1 += cy1;							tmp5 += cy5;
		tmp2 += cy2;							tmp6 += cy6;
		tmp3 += cy3;							tmp7 += cy7;

		MULH32(q0,tmp0, cy0);					MULH32(q4,tmp4, cy4);
		MULH32(q1,tmp1, cy1);					MULH32(q5,tmp5, cy5);
		MULH32(q2,tmp2, cy2);					MULH32(q6,tmp6, cy6);
		MULH32(q3,tmp3, cy3);					MULH32(q7,tmp7, cy7);
	}
	retval += ((cy0==0)&&(nshift0<=trailx));	retval += ((cy4==0)&&(nshift4<=trailx))<<4;
	retval += ((cy1==0)&&(nshift1<=trailx))<<1;	retval += ((cy5==0)&&(nshift5<=trailx))<<5;
	retval += ((cy2==0)&&(nshift2<=trailx))<<2;	retval += ((cy6==0)&&(nshift6<=trailx))<<6;
	retval += ((cy3==0)&&(nshift3<=trailx))<<3;	retval += ((cy7==0)&&(nshift7<=trailx))<<7;
	return retval;
}
#endif	// __CUDA_ARCH__ ?

#ifndef __CUDA_ARCH__
/*
Data: Integer n >= 0, unsigned 64-bit integers q; qinv, q odd and q * qinv == 1 (mod R), with R = 2^64 here.
Returns: The (n)th modular power of the twos-complement radix, R^n (mod q).
*/
#if MI64_DEBUG
	#define MI64_RAD_POW64_DBG	0
#endif
uint64 radix_power64(const uint64 q, const uint64 qinv, uint32 n)
{
	const char func[] = "radix_power64";
#if MI64_RAD_POW64_DBG
	int dbg = 0;	//q == 16357897499336320049ull;
#endif
	int i,j,bmap,p;	// i and j must be signed
	uint64 rem64,itmp64;
	double fquo;
	const double TWO96FLOAT = (double)0x0001000000000000ull * (double)0x0001000000000000ull;

	if(n < 2) return 1ull-n;	// n = 0|1: return 1|0, resp.

	// The minimum nontrivial power of B we need is B^2, so first obtain that. While the pure-int64 method
	// for obtaining 2^96 % q (= B^(3/2) % q) works for all nonzero q < 2^64, we use specialized code to
	// speed things for distinctive subranges of modulus size.

	if(q >> 48)	{	// q in [2^48,2^64)

		// Default method uses fast floating-point mod:
		fquo = TWO96FLOAT / (double)q;
	
	  #ifndef YES_ASM
	
		// Decompose 2^96 - (2^96 % q) = k.q is exactly divisible by q; k = floor(2^96/q) ;
		// thus the desired mod, (2^96 % q) = 2^96 - k.q = 2^96 - q.floor(2^96/q) .
		// But since q < 2^64 we know the mod result also fits into 64 bits, thus we can
		// work modulo 2^64, which is automatic when using 64-bit unsigned integer math:
	
		rem64  = (uint64)fquo;	// Truncation gives rem64 = floor(2^96/q)
		itmp64 = -(rem64 * q);	// Bottom 64 bits of 2^96 - q*(2^96/q)
	
		// Jul 2016: With q < 2^48 split out and handled separately, see no failures of the kind described below
		// anymore, but keep the code around just in case we run into a cavalier-with-roundoff-error compiler.
			//
		// Floating-point method here fails in a tiny fraction of cases (which on x86 is highly dependent
		// on build mode, likely due to in-register precision effects: in my test, ~35000 of 10^9 fail for
		// debug-build, only 19 for opt-build), but most of these failures easily cured via this check.
		// If quotient has (exact) fractional part = 0.99...., may end up with fquo = ceiling(2^96/q) rather
		// than floor; thus want q - (2^64 - itmp64):
		if(itmp64 > q) {
			// This check allows us to differentiate between incorrect upward-rounded and (rarer) downward-rounded cases:
			if(DNINT(fquo) == (double)rem64) {	// Incorrect   upward-rounded, e.g. fquo = 1084809392143.0001, exact = 1084809392142.999...
			//	printf("%sA: q = %llu < itmp64 = (int64)%lld, fquo = %20.4f, (double)rem64 = %20.4f\n",func,q,(int64)itmp64, fquo, (double)rem64);
				itmp64 += q;
			} else {							// Incorrect downward-rounded, e.g. fquo = 7344640876302.9990, exact = 7344640876303.0000002...
			//	printf("%sB: q = %llu < itmp64 = (int64)%lld, fquo = %20.4f *** Bad Downward ***\n",func,q,(int64)itmp64, fquo);
				itmp64 -= q;
			}
		}
	
	  #else	// Use SSE2 code for manipulating 1 double in lower 64 bits of a SIMD register:
	
		const double rnd = 3.0*0x4000000*0x2000000;	// Const for DNINT(x) = (x + rnd) - rnd emulation with 53-bit
													// SSE2-double mantissa. In hex-bitfield form: 0x4338000000000000
		__asm__ volatile (\
			"movq	%[__q],%%rcx		\n\t"\
			"movsd	%[__rnd] ,%%xmm0	\n\t"\
			"movsd	%[__fquo],%%xmm1	\n\t"\
			"cvttsd2si	%%xmm1,%%rax	\n\t"/* rem64 = (uint64)fquo */\
			"movq	%%rax,%%rbx		\n\t"/* save copy of rem64 in rbx */\
			"imulq	%%rcx,%%rax		\n\t"/* q * rem64 */\
			"negq	%%rax			\n\t"/* itmp64 = -(rem64 * q) */\
			"movq	%%rcx,%%rdx		\n\t"/* put copy-to-be-destroyed of q in rdx */\
			"subq	%%rax,%%rdx		\n\t"/* compare itmp64 vs q (nondestructive, since sub itmp64 from copy of q) */\
		"jnc	rad_pow64_end		\n\t"/* if CF = 0 (i.e. itmp64 <= q), exit */\
			"addsd	%%xmm0,%%xmm1	\n\t"\
			"subsd	%%xmm0,%%xmm1	\n\t"/* DNINT(fquo) in xmm1 */\
			"cvtsi2sd	%%rbx,%%xmm0	\n\t"/* overwrite rnd-const with (double)rem64 */\
			"cmpsd	$0,%%xmm0,%%xmm1	\n\t"/* DNINT(fquo) == (double)rem64 ? */\
			"leaq	(%%rax,%%rcx),%%rbx	\n\t"/* rbx <- itmp64 + q */\
			"subq	%%rcx,%%rax			\n\t"/* rax <- itmp64 -= q */\
			"movd	%%xmm1,%%ecx	\n\t"/* ecx = (bottom 32 bits of) CMPSD result; all-ones if true (note there is no analogous MOVQ instruction which moves 64 bits into an r-reg, only move-bottom-64-bits-of-xmm-reg-to-memory) */\
	/*		"andl	$0x1,%%ecx		\n\t"// Set ZF according to true/false-ity of CMPSD result *** EWM: why does this fail? ***/\
	/*		"cmovzq %%rbx,%%rax	\n\t" // if ZF = 1 (CMPSD = true), overwrite dest (rax = itmp64-q) with source (rbx = itmp64+q), else leave dest = itmp64-q. */\
			"xorq	%%rdx,%%rdx		\n\t"/* rdx = 0 */\
			"subq	%%rcx,%%rdx		\n\t"/* save CMPSD result in CF (fur use by CMOV) */\
			"cmovcq %%rbx,%%rax	\n\t"/* if CF = 1 (CMPSD = true), overwrite dest (rax = itmp64-q) with source (rbx = itmp64+q), else leave dest = itmp64-q. */\
		"rad_pow64_end: 	\n\t"\
			"movq	%%rax,%[__itmp64]	\n\t"\
		:	/* outputs: none */\
		: [__fquo] "m" (fquo)	/* All inputs from memory addresses here */\
		 ,[__rnd] "m" (rnd)	\
		 ,[__q] "m" (q)	\
		 ,[__itmp64] "m" (itmp64)	\
		: "cc","memory","rax","rbx","rcx","rdx","xmm0","xmm1"	/* Clobbered registers */\
		);
	
	  #endif
	
		// Floating-point computation of 2^96 % q not 100% reliable - this pure-int code is our safety net:
		if(itmp64 > q) {
			printf("Error correction failed: itmp64 = (int64)%lld, q = %llu [lq(q) = %6.4f]\n",(int64)itmp64,q,log(q)/log(2));
			// In such cases re-do using the slower but bulletproof pure-integer method.
			// Use mod-doublings to get 2^68 (mod q), followed by 3 MONT_SQR64:
			itmp64 = 0x8000000000000000ull % q;	// 2^63 (mod q)
			// 5 mod-doublings yield 2^68 (mod q):
			MOD_ADD64(itmp64,itmp64,q, itmp64);	// 2^64 (mod q)
			MOD_ADD64(itmp64,itmp64,q, itmp64);	// 2^65 (mod q)
			MOD_ADD64(itmp64,itmp64,q, itmp64);	// 2^66 (mod q)
			MOD_ADD64(itmp64,itmp64,q, itmp64);	// 2^67 (mod q)
			MOD_ADD64(itmp64,itmp64,q, itmp64);	// 2^68 (mod q)
			MONT_SQR64(itmp64,q,qinv,itmp64);	// 2^(2*68-64) == 2^72 (mod q)
			MONT_SQR64(itmp64,q,qinv,itmp64);	// 2^(2*72-64) == 2^80 (mod q)
			MONT_SQR64(itmp64,q,qinv,itmp64);	// 2^(2*80-64) == 2^96 (mod q)
			ASSERT(HERE, itmp64 < q, "Pure-integer computation of 2^96 mod q fails!");
		}

	} else if(q >> 32)	{	// q in [2^32,2^48)

		// Starting with 2^60 (mod q), do pair of MONT_SQR48 to power up to 2^96 (mod q):
		itmp64 = 0x1000000000000000ull % q;	// 2^60 (mod q)
		MONT_MUL48(itmp64,itmp64,q,qinv,itmp64);	// 2^(2*60-48) == 2^72 (mod q)
		MONT_MUL48(itmp64,itmp64,q,qinv,itmp64);	// 2^(2*72-48) == 2^96 (mod q)
		ASSERT(HERE, itmp64 < q, "Pure-integer computation of 2^96 mod q fails!");

	} else {	// q < 2^32

		// Use mod-doublings to get 2^64 (mod q), followed by a single MONT_SQR32:
		uint32 q32 = q, qinv32 = qinv, itmp32 = 0x8000000000000000ull % q;	// 2^63 (mod q)
		// Use simpler code sequence here than MOD_ADD64 since no chance of integer overflow-on-add:
		itmp32 = itmp32 + itmp32 - q32;	itmp32 += (-((int32)itmp32 < 0) & q32);	// 2^64 (mod q)
		MONT_MUL32(itmp32,itmp32,q32,qinv32,itmp32);	// 2^(2*64-32) == 2^96 (mod q)
		ASSERT(HERE, itmp32 < q32, "Pure-integer computation of 2^96 mod q fails!");
		itmp64 = itmp32;	// promote to 64-bit

	}

	// Now that have B^(3/2), do a Mont-square to get B^2 % q:
	MONT_SQR64(itmp64,q,qinv,rem64);

#if MI64_RAD_POW64_DBG
	if(dbg)printf("B^2 mod q = %20llu\n",rem64);
#endif

	/* rem64 holds B^2 mod q - Now compute sequence of powers needed to obtain B^len mod q via Montgomery-muls: */
	p = n;	// Add 1 since used high-MUL version of the scaled-remainder algo ( = Algorithm A in the paper)
	// If p == 2, rem64 already contains the needed power
	if(p == 3) {
		MONT_SQR64(rem64,q,qinv, rem64);
	} else if(p > 3) {
		// We always start with p = 2 and M-square that to get p = 3:
		bmap = 0;		// Init bitstring
		for(j = 0; p > 5; j++)		// j counts #bits processed
		{
			BIT_SETC(bmap,j,IS_EVEN(p));
			p = (p >> 1) + 1;
		}
		// Now do the needed powering. We always start with p = 2 and M-square that to get p = 3:
		MONT_SQR64(rem64,q,qinv,itmp64);	// rem64 has p = 2, itmp64 has p = 3
		if(p == 4) {
			MONT_MUL64(itmp64,rem64,q,qinv,rem64);
		} else if(p == 5) {
			MONT_SQR64(itmp64,q,qinv,rem64);
		} else {
			ASSERT(HERE, 0,"Bad starting value for power p!");
		}
		for(i = j-1; i >= 0; i--) {
			if(BIT_TEST(bmap,i)) {
				itmp64 = rem64;
				MONT_UNITY_MUL64(itmp64,q,qinv,itmp64);	// Reduce power of B by 1 in one of the 2 multiplicands...
				MONT_MUL64(itmp64,rem64,q,qinv,rem64);	// ...and multiply `em.
			} else {
				MONT_SQR64(rem64,q,qinv,rem64);
			}
		}
	}
#if MI64_RAD_POW64_DBG
	if(dbg && p > 2)printf("B^%u mod q = %20llu\n",n,rem64);
#endif
	return rem64;
}
#endif	// __CUDA_ARCH__ ?

#ifndef __CUDA_ARCH__
/* Fast is-divisible-by-64-bit scalar using Montgomery modmul and right-to-left modding: */
int mi64_is_div_by_scalar64(const uint64 x[], uint64 q, uint32 len)
{
#ifndef YES_ASM
	uint64 tmp;
#endif
	uint32 i,nshift;
	uint64 qinv,cy;

	ASSERT(HERE, q > 0, "mi64_is_div_by_scalar64: 0 modulus!");
	if(q == 1) return TRUE;
	if(len == 0) return TRUE;

	/* q must be odd for Montgomery-style modmul to work, so first shift off any low 0s: */
	nshift = trailz64(q);
	if(nshift) {
		if(trailz64(x[0]) < nshift) return FALSE;
		q >>= nshift;
	}

	uint32 q32,qi32;
	q32  = q; qi32 = minv8[(q&0xff)>>1];
	qi32 = qi32*((uint32)2 - q32*qi32);
	qi32 = qi32*((uint32)2 - q32*qi32);	qinv = qi32;
	qinv = qinv*((uint64)2 - q*qinv);

#ifndef YES_ASM

	cy = (uint64)0;
	for(i = 0; i < len; ++i) {
		tmp  = x[i] - cy;
		/* Add q if had a borrow - Since we low=half multiply tmp by qinv below and q*qinv == 1 (mod 2^64),
		   can simply add 1 to tmp*qinv result instead of adding q to the difference here:
		*/
		cy = (cy > x[i]); /* Comparing this rather than (tmp > x[i]) frees up tmp for the multiply */
		/* Now do the Montgomery mod: cy = umulh( q, mulq(tmp, qinv) ); */
		tmp = tmp*qinv + cy;
	#ifdef MUL_LOHI64_SUBROUTINE
		cy = __MULH64(q,tmp);
	#else
		MULH64(q,tmp, cy);
	#endif
	}

#elif 1

	__asm__ volatile (\
		"movq	%[__x],%%r10	\n\t"\
		"xorq	%%rdi,%%rdi		\n\t"/* Will use RDI/SIL to save CF */\
		"xorq	%%rdx,%%rdx		\n\t"/* Init cy = 0 */\
		"movq	%[__q],%%rsi	\n\t"\
		"movq	%[__qinv],%%rbx	\n\t"\

		"movslq	%[__len], %%rcx	\n\t"/* ASM loop structured as for(j = len; j != 0; --j){...} */\
	"loop_start:					\n\t"\
		"movq	(%%r10),%%rax	\n\t"/* load x[i] */\
		"addq   $0x8, %%r10	\n\t"/* Increment array pointer */\
		"subq	%%rdx,%%rax		\n\t"/* tmp  = x[i] - cy */\
		"setc	%%dil			\n\t"/* save CF */\
		"imulq	%%rbx,%%rax 	\n\t"/* tmp *= qinv */\
		"addq	%%rdi,%%rax		\n\t"/* tmp = tmp*qinv + CF */\
		"mulq	%%rsi		\n\t"/* cy = __MULH64(q,tmp) */\

	"subq	$1,%%rcx \n\t"\
	"jnz loop_start 	\n\t"/* loop1 end; continue is via jump-back if rcx != 0 */\

		"movq	%%rdx,%[__cy]	\n\t"\
		:	/* outputs: none */\
		: [__q] "m" (q)	/* All inputs from memory addresses here */\
		 ,[__qinv] "m" (qinv)	\
		 ,[__x] "m" (x)	\
		 ,[__cy] "m" (cy)	\
		 ,[__len] "m" (len)	\
		: "cc","memory","rax","rbx","rcx","rdx","rsi","rdi","r10"	/* Clobbered registers */\
		);
#else	// This variant of my original ASM due to Robert Holmes:

	__asm__ volatile (\
		"movq	%[__x],%%r10	\n\t"\
		"xorq	%%rdx,%%rdx		\n\t"/* Init cy = 0 */\
		"movq	%[__q],%%rsi	\n\t"\
		"movq	%[__qinv],%%rbx	\n\t"\

		"movslq	%[__len], %%rcx	\n\t"/* ASM loop structured as for(j = len; j != 0; --j){...} */\
	"loop_start:					\n\t"\
		"movq	(%%r10),%%rax	\n\t"/* load x[i] */\
		"addq   $0x8, %%r10	\n\t"/* Increment array pointer */\
		"subq	%%rdx,%%rax		\n\t"/* tmp  = x[i] - cy */\
		"sbbq	%%rdi,%%rdi		\n\t"/* save -CF */\
		"imulq	%%rbx,%%rax 	\n\t"/* tmp *= qinv */\
		"subq	%%rdi,%%rax		\n\t"/* tmp = tmp*qinv - (-CF) */\
		"mulq	%%rsi		\n\t"/* cy = __MULH64(q,tmp) */\

	"subq	$1,%%rcx \n\t"\
	"jnz loop_start 	\n\t"/* loop1 end; continue is via jump-back if rcx != 0 */\

		"movq	%%rdx,%[__cy]	\n\t"\
		:	/* outputs: none */\
		: [__q] "m" (q)	/* All inputs from memory addresses here */\
		 ,[__qinv] "m" (qinv)	\
		 ,[__x] "m" (x)	\
		 ,[__cy] "m" (cy)	\
		 ,[__len] "m" (len)	\
		: "cc","memory","rax","rbx","rcx","rdx","rsi","rdi","r10"	/* Clobbered registers */\
		);
#endif
	return (cy == 0);
}

// 4 trial divisors at a time:
#if MI64_DEBUG
	#define MI64_ISDIV_X4_DBG	0
#endif
int mi64_is_div_by_scalar64_x4(const uint64 x[], uint64 q0, uint64 q1, uint64 q2, uint64 q3, uint32 len)
{
	int retval = 0;
#ifndef YES_ASM
	uint64 tmp0,tmp1,tmp2,tmp3;
#endif
#if MI64_ISDIV_X4_DBG
	int dbg = 0;
#endif
	uint32 i,trailx;
	uint32 nshift0,nshift1,nshift2,nshift3;
	uint64 qinv0,qinv1,qinv2,qinv3,cy0,cy1,cy2,cy3;

	ASSERT(HERE, (len == 0), "0 length!");
	trailx = trailz64(x[0]);
	ASSERT(HERE, trailx < 64, "0 low word!");

	/* q must be odd for Montgomery-style modmul to work, so first shift off any low 0s: */
	nshift0 = trailz64(q0);
	nshift1 = trailz64(q1);
	nshift2 = trailz64(q2);
	nshift3 = trailz64(q3);

	q0 >>= nshift0;
	q1 >>= nshift1;
	q2 >>= nshift2;
	q3 >>= nshift3;
	ASSERT(HERE, q1 > 1 && q1 > 1 && q2 > 1 && q3 > 1 , "modulus must be > 1!");
	ASSERT(HERE, q0 & 1 && q1 & 1 && q2 & 1 && q3 & 1 , "even modulus!");

	qinv0 = (q0+q0+q0) ^ (uint64)2;
	qinv1 = (q1+q1+q1) ^ (uint64)2;
	qinv2 = (q2+q2+q2) ^ (uint64)2;
	qinv3 = (q3+q3+q3) ^ (uint64)2;
	for(i = 0; i < 4; i++) {
		qinv0 = qinv0*((uint64)2 - q0*qinv0);
		qinv1 = qinv1*((uint64)2 - q1*qinv1);
		qinv2 = qinv2*((uint64)2 - q2*qinv2);
		qinv3 = qinv3*((uint64)2 - q3*qinv3);
	}

#ifndef YES_ASM
	cy0 = cy1 = cy2 = cy3 = (uint64)0;
	for(i = 0; i < len; ++i) {
		tmp0 = x[i] - cy0;			tmp1 = x[i] - cy1;			tmp2 = x[i] - cy2;			tmp3 = x[i] - cy3;
		cy0 = (cy0 > x[i]);			cy1 = (cy1 > x[i]);			cy2 = (cy2 > x[i]);			cy3 = (cy3 > x[i]);
		tmp0 = tmp0*qinv0 + cy0;	tmp1 = tmp1*qinv1 + cy1;	tmp2 = tmp2*qinv2 + cy2;	tmp3 = tmp3*qinv3 + cy3;
	#ifdef MUL_LOHI64_SUBROUTINE
		cy0 = __MULH64(q0,tmp0);	cy1 = __MULH64(q1,tmp1);	cy2 = __MULH64(q2,tmp2);	cy3 = __MULH64(q3,tmp3);
	#else
		MULH64(q0,tmp0, cy0);		MULH64(q1,tmp1, cy1);		MULH64(q2,tmp2, cy2);		MULH64(q3,tmp3, cy3);
	#endif
	}

#else

	// 4-way version leaves registers RSI and RBX unused:
	__asm__ volatile (\
		"movq	%[__x],%%r10	\n\t"\
		"xorq	%%rdi,%%rdi		\n\t"/* Init cy0 = 0 */\
		"xorq	%%r8 ,%%r8 		\n\t"/* Init cy1 = 0 */\
		"xorq	%%r9 ,%%r9 		\n\t"/* Init cy2 = 0 */\
		"xorq	%%rdx,%%rdx		\n\t"/* Init cy3 = 0 */\

		"movslq	%[__len],%%r14	\n\t"/* ASM loop structured as for(j = len; j != 0; --j){...} */\
		"shrq	$1      ,%%r14	\n\t"/* len/2 */\
		"leaq	(%%r10,%%r14,8),%%r15	\n\t"/* x+len/2 */\
		"shrq	$1      ,%%r14	\n\t"/* len/4 */\
		"movq	%%r14,%%rcx	\n\t"/* Copy len/4 into loop counter*/\
	"loop4x:		\n\t"\
		"movq	(%%r10),%%rax	\n\t	leaq	(%%r10,%%r14,8),%%r11	\n\t"/* load x0,&x1 */\
		"addq	$0x8 ,%%r10		\n\t	movq	(%%r11),%%r11	\n\t"/* Increment x0-ptr, load x1 */\
		"subq	%%rdi,%%rax		\n\t	sbbq	%%rdi,%%rdi		\n\t"/* tmp0 = x0 - cy0, save -CF */\
		"subq	%%r8 ,%%r11		\n\t	sbbq	%%r8 ,%%r8 		\n\t"/* tmp1 = x1 - cy1, save -CF */\
		"movq	(%%r15),%%r12	\n\t	leaq	(%%r15,%%r14,8),%%r13	\n\t"/* load x2,&x3 */
		"addq	$0x8 ,%%r15		\n\t	movq	(%%r13),%%r13	\n\t"/* Increment x2-ptr, load x3 */\
		"subq	%%r9 ,%%r12		\n\t	sbbq	%%r9 ,%%r9 		\n\t"/* tmp2 = x2 - cy2, save -CF */\
		"subq	%%rdx,%%r13		\n\t	sbbq	%%rdx,%%rdx		\n\t"/* tmp3 = x3 - cy3, save -CF */\
		"imulq	%[__qinv0],%%rax \n\t	imulq	%[__qinv2],%%r11 	\n\t"/* tmp0,1 *= qinv */\
		"imulq	%[__qinv1],%%r12 \n\t	imulq	%[__qinv3],%%r13 	\n\t"/* tmp2,3 *= qinv */\
		"subq	%%rdi,%%rax		\n\t	subq	%%r8 ,%%r11		\n\t"/* tmp0,1 = tmp0,1 * qinv + CF */\
		"subq	%%r9 ,%%r12		\n\t	subq	%%rdx,%%r13		\n\t"/* tmp2,3 = tmp2,3 * qinv + CF */\
		"								mulq	%[__q0]	\n\t	movq	%%rdx,%%rdi	\n\t"/* cy0 = MULH64(q,tmp0), then move cy0 out of rdx in prep for cy1 computation */\
		"movq	%%r11,%%rax		\n\t	mulq	%[__q1]	\n\t	movq	%%rdx,%%r8 	\n\t"/* cy1 = MULH64(q,tmp1), then move cy1 out of rdx in prep for cy2 computation */\
		"movq	%%r12,%%rax		\n\t	mulq	%[__q2]	\n\t	movq	%%rdx,%%r9 	\n\t"/* cy2 = MULH64(q,tmp2), then move cy2 out of rdx in prep for cy3 computation */\
		"movq	%%r13,%%rax		\n\t	mulq	%[__q3]	\n\t"/* load x3 into rax; cy3 = MULH64(q,tmp3), leave result in rdx */\

	"subq	$1,%%rcx \n\t"\
	"jnz loop4x 	\n\t"/* loop1 end; continue is via jump-back if rcx != 0 */\

		"movq	%%rdi,%[__cy0]	\n\t	movq	%%r8 ,%[__cy1]	\n\t	movq	%%r9 ,%[__cy2]	\n\t	movq	%%rdx,%[__cy3]	\n\t"\
	:	/* outputs: none */\
	: [__q0] "m" (q0)	/* All inputs from memory addresses here */\
	 ,[__q1] "m" (q1)	\
	 ,[__q2] "m" (q2)	\
	 ,[__q3] "m" (q3)	\
	 ,[__qinv0] "m" (qinv0)	\
	 ,[__qinv1] "m" (qinv1)	\
	 ,[__qinv2] "m" (qinv2)	\
	 ,[__qinv3] "m" (qinv3)	\
	 ,[__x] "m" (x)	\
	 ,[__cy0] "m" (cy0)	\
	 ,[__cy1] "m" (cy1)	\
	 ,[__cy2] "m" (cy2)	\
	 ,[__cy3] "m" (cy3)	\
	 ,[__len] "m" (len)	\
	: "cc","memory","rax","rcx","rdx","rdi","r8","r9","r10","r11","r12","r13","r14","r15"	/* Clobbered registers */\
	);
#endif

#if MI64_ISDIV_X4_DBG
	if(dbg)printf("4-way carryouts: cy0-3 = %20llu, %20llu, %20llu, %20llu\n",cy0,cy1,cy2,cy3);
#endif
	retval += ((cy0 == 0) && (nshift0 <= trailx));
	retval += ((cy1 == 0) && (nshift1 <= trailx)) << 1;
	retval += ((cy2 == 0) && (nshift2 <= trailx)) << 2;
	retval += ((cy3 == 0) && (nshift3 <= trailx)) << 3;
	return retval;
}


// 2-way loop splitting:
#if MI64_DEBUG
	#define MI64_ISDIV_U2_DBG	0
#endif
int mi64_is_div_by_scalar64_u2(const uint64 x[], uint64 q, uint32 len)
{
#if MI64_ISDIV_U2_DBG
	int dbg = q == 16357897499336320049ull;//87722769297534671ull;
#endif
#ifndef YES_ASM
	uint64 tmp0,tmp1,bw0,bw1;
#endif
	uint32 i,len2 = (len>>1),nshift;
	uint64 qinv,cy0,cy1,rpow;

	ASSERT(HERE, q > 0, "mi64_is_div_by_scalar64: 0 modulus!");
	if(q == 1) return TRUE;
	if(len == 0) return TRUE;
	ASSERT(HERE, (len&1) == 0, "odd length!");
	/* q must be odd for Montgomery-style modmul to work, so first shift off any low 0s: */
	nshift = trailz64(q);
ASSERT(HERE, !nshift, "2-way folded ISDIV requires odd q!");
	if(nshift) {
		if(trailz64(x[0]) < nshift) return FALSE;
		q >>= nshift;
	}

	uint32 q32,qi32;
	q32  = q; qi32 = minv8[(q&0xff)>>1];
	qi32 = qi32*((uint32)2 - q32*qi32);
	qi32 = qi32*((uint32)2 - q32*qi32);	qinv = qi32;
	qinv = qinv*((uint64)2 - q*qinv);

#ifndef YES_ASM
	cy0 = cy1 = (uint64)0;
	for(i = 0; i < len2; ++i) {
		tmp0  = x[i] - cy0;				tmp1 = x[i+len2] - cy1;
		cy0 = (cy0 > x[i]);				cy1 = (cy1 > x[i+len2]);
		tmp0 = tmp0*qinv + cy0;			tmp1 = tmp1*qinv + cy1;
	#ifdef MUL_LOHI64_SUBROUTINE
		cy0 = __MULH64(q,tmp0);			cy1 = __MULH64(q,tmp1);
	#else
		MULH64(q,tmp0, cy0);				MULH64(q,tmp1, cy1);
	#endif
	}

#else

	__asm__ volatile (\
		"movq	%[__x],%%r10	\n\t"\
		"xorq	%%rdi,%%rdi		\n\t"/* Init cy0 = 0 */\
		"xorq	%%rdx,%%rdx		\n\t"/* Init cy1 = 0 */\
		"movq	%[__q],%%rsi	\n\t"\
		"movq	%[__qinv],%%rbx	\n\t"\

		"movslq	%[__len], %%rcx	\n\t"/* ASM loop structured as for(j = len; j != 0; --j){...} */\
		"shrq	$1, %%rcx	\n\t"/* len/2 */\
		"leaq	(%%r10,%%rcx,8),%%r11	\n\t"/* x+len/2 */\
	"loop2a:		\n\t"\
		"movq	(%%r10),%%rax	\n\t	movq	(%%r11),%%r12	\n\t"/* load x0,x1 */\
		"addq	$0x8 ,%%r10		\n\t	addq	$0x8 ,%%r11		\n\t"/* Increment array pointers */\
		"subq	%%rdi,%%rax		\n\t	sbbq	%%rdi,%%rdi		\n\t"/* tmp0 = x0 - cy0, save -CF */\
		"subq	%%rdx,%%r12		\n\t	sbbq	%%rdx,%%rdx		\n\t"/* tmp1 = x1 - cy1, save -CF */\
		"imulq	%%rbx,%%rax 	\n\t	imulq	%%rbx,%%r12 	\n\t"/* tmp *= qinv */\
		"subq	%%rdi,%%rax		\n\t	subq	%%rdx,%%r12		\n\t"/* tmp = tmp*qinv + CF */\
		"mulq	%%rsi			\n\t	movq	%%rdx,%%rdi		\n\t"/* cy0 = MULH64(q,tmp0), then move cy0 out of rdx in prep for cy1 computation */\
		"movq	%%r12,%%rax		\n\t"/* load x1 into rax */\
		"mulq	%%rsi			\n\t"/* cy1 = MULH64(q,tmp1), leave result in rdx */\

	"subq	$1,%%rcx \n\t"\
	"jnz loop2a 	\n\t"/* loop1 end; continue is via jump-back if rcx != 0 */\

		"movq	%%rdi,%[__cy0]	\n\t	movq	%%rdx,%[__cy1]	"\
		:	/* outputs: none */\
		: [__q] "m" (q)	/* All inputs from memory addresses here */\
		 ,[__qinv] "m" (qinv)	\
		 ,[__x] "m" (x)	\
		 ,[__cy0] "m" (cy0)	\
		 ,[__cy1] "m" (cy1)	\
		 ,[__len] "m" (len)	\
		: "cc","memory","rax","rbx","rcx","rdx","rsi","rdi","r10","r11","r12"		/* Clobbered registers */\
		);
#endif

#if MI64_ISDIV_U2_DBG
	if(dbg)printf("Half-length carryouts: cy0 = %20llu, cy1 = %20llu\n",cy0,cy1);
#endif
	// Compute radix-power; add 1 since used high-MUL version of the scaled-remainder algo ( = Algorithm A in the paper)
	rpow = radix_power64(q,qinv,len2+1);

	// Multiply the Montgomery residue from the mod-loop by the mod-power of the base:
	MONT_MUL64(cy1,rpow,q,qinv,cy1);	// cy1*B^p (mod q)
#if MI64_ISDIV_U2_DBG
	if(dbg) {
		printf("s1     mod q) = %20llu\n",cy0);
		printf("s2*B^p mod q) = %20llu\n",cy1);
	}
#endif
	// Sum the scaled partial remainders:
	cy0 += cy1;
	if(cy0 < cy1 || cy0 >= q) cy0 -= q;
	// Negation (mod q) needed for Algo A scaled remainder
	if(cy0) cy0 = q-cy0 ;
#if MI64_ISDIV_U2_DBG
	if(dbg)printf("(s1 + s2*B^p) mod q = %20llu, q = %20llu\n",cy0,q);
#endif
	// One more modmul of sum by same power of the base gives true remainder - may as well, since we already have B^p handy:
	MONT_MUL64(cy0,rpow,q,qinv,cy0);
#if MI64_ISDIV_U2_DBG
	if(dbg) {
		printf("True mod x mod q = %20llu\n",cy0);
		exit(0);
	}
#endif

	return (cy0 == 0);
}

// 4-way loop splitting:
#if MI64_DEBUG
	#define MI64_ISDIV_U4_DBG	0
#endif
int mi64_is_div_by_scalar64_u4(const uint64 x[], uint64 q, uint32 len)
{
#ifndef YES_ASM
	uint32 i0,i1,i2,i3;
	uint64 tmp0,tmp1,tmp2,tmp3;
#endif
#if MI64_ISDIV_U4_DBG
	int dbg = 0;
#endif
	uint32 i,len4 = (len>>2),nshift;
	uint64 qinv,cy0,cy1,cy2,cy3,rpow;

	ASSERT(HERE, q > 0, "mi64_is_div_by_scalar64: 0 modulus!");
	if(q == 1) return TRUE;
	if(len == 0) return TRUE;
	ASSERT(HERE, (len&3) == 0, "Length must be a multiple of 4!");
	/* q must be odd for Montgomery-style modmul to work, so first shift off any low 0s: */
	nshift = trailz64(q);
ASSERT(HERE, !nshift, "4-way folded ISDIV requires odd q!");
	if(nshift) {
		if(trailz64(x[0]) < nshift) return FALSE;
		q >>= nshift;
	}

	uint32 q32,qi32;
	q32  = q; qi32 = minv8[(q&0xff)>>1];
	qi32 = qi32*((uint32)2 - q32*qi32);
	qi32 = qi32*((uint32)2 - q32*qi32);	qinv = qi32;
	qinv = qinv*((uint64)2 - q*qinv);

#ifndef YES_ASM
	cy0 = cy1 = cy2 = cy3 = (uint64)0;
	for(i0 = 0, i1 = len4, i2 = i1+i1, i3 = i1+i2; i0 < len4; ++i0, ++i1, ++i2, ++i3) {
		tmp0 = x[i0] - cy0;			tmp1 = x[i1] - cy1;			tmp2 = x[i2] - cy2;			tmp3 = x[i3] - cy3;
		cy0 = (cy0 > x[i0]);		cy1 = (cy1 > x[i1]);		cy2 = (cy2 > x[i2]);		cy3 = (cy3 > x[i3]);
		tmp0 = tmp0*qinv + cy0;		tmp1 = tmp1*qinv + cy1;		tmp2 = tmp2*qinv + cy2;		tmp3 = tmp3*qinv + cy3;
	#ifdef MUL_LOHI64_SUBROUTINE
		cy0 = __MULH64(q,tmp0);		cy1 = __MULH64(q,tmp1);		cy2 = __MULH64(q,tmp2);		cy3 = __MULH64(q,tmp3);
	#else
		MULH64(q,tmp0, cy0);		MULH64(q,tmp1, cy1);		MULH64(q,tmp2, cy2);		MULH64(q,tmp3, cy3);
	#endif
	}

#else

  #ifdef USE_AVX2	// Haswell-and-beyond version (Have a MULX instruction)
			// Note: MULX reg1,reg2,reg3 [AT&T/GCC syntax] assumes src1 in RDX [MULQ assumed RAX],
			//                            src2 in named reg1, lo"hi output halves into reg2:reg3:

	__asm__ volatile (\
		"movq	%[__x],%%r10	\n\t"\
		"xorq	%%rdi,%%rdi		\n\t"/* Init cy0 = 0 */\
		"xorq	%%r8 ,%%r8 		\n\t"/* Init cy1 = 0 */\
		"xorq	%%r9 ,%%r9 		\n\t"/* Init cy2 = 0 */\
		"xorq	%%rax,%%rax		\n\t"/* Init cy3 = 0 */\
		"movq	%[__q],%%rdx	\n\t"/* In context of MULX want this fixed word in RDX, the implied-MULX-input register */\
		"movq	%[__qinv],%%rbx	\n\t"\
		"movslq	%[__len],%%r14	\n\t"/* ASM loop structured as for(j = len; j != 0; --j){...} */\
		"shrq	$1      ,%%r14	\n\t"/* len/2 */\
		"leaq	(%%r10,%%r14,8),%%r15	\n\t"/* x+len/2 */\
		"shrq	$1      ,%%r14	\n\t"/* len/4 */\
		"movq	%%r14,%%rcx	\n\t"/* Copy len/4 into loop counter*/\
	"loop4u:		\n\t"\
		"movq	(%%r10),%%rsi	\n\t	leaq	(%%r10,%%r14,8),%%r11	\n\t"/* load x0,&x1 */\
		"addq	$0x8 ,%%r10		\n\t	movq	(%%r11),%%r11	\n\t"/* Increment x0-ptr, load x1 */\
		"subq	%%rdi,%%rsi		\n\t	sbbq	%%rdi,%%rdi		\n\t"/* tmp0 = x0 - cy0, save -CF */\
		"subq	%%r8 ,%%r11		\n\t	sbbq	%%r8 ,%%r8 		\n\t"/* tmp1 = x1 - cy1, save -CF */\
		"movq	(%%r15),%%r12	\n\t	leaq	(%%r15,%%r14,8),%%r13	\n\t"/* load x2,&x3 */
		"addq	$0x8 ,%%r15		\n\t	movq	(%%r13),%%r13	\n\t"/* Increment x2-ptr, load x3 */\
		"subq	%%r9 ,%%r12		\n\t	sbbq	%%r9 ,%%r9 		\n\t"/* tmp2 = x2 - cy2, save -CF */\
		"subq	%%rax,%%r13		\n\t	sbbq	%%rax,%%rax		\n\t"/* tmp3 = x3 - cy3, save -CF */\
		"imulq	%%rbx,%%rsi 	\n\t	imulq	%%rbx,%%r11 	\n\t"/* tmp0,1 *= qinv */\
		"imulq	%%rbx,%%r12 	\n\t	imulq	%%rbx,%%r13 	\n\t"/* tmp2,3 *= qinv */\
		"subq	%%rdi,%%rsi		\n\t	subq	%%r8 ,%%r11		\n\t"/* tmp0,1 = tmp0,1 * qinv + CF */\
		"subq	%%r9 ,%%r12		\n\t	subq	%%rax,%%r13		\n\t"/* tmp2,3 = tmp2,3 * qinv + CF */\
		/* In each MULX, overwrite the explicit input register with the low (discard) product half: */\
		"mulxq	%%rsi,%%rsi,%%rdi	\n\t"/* x0 enters in rsi; cy0 = MULH64(q,tmp0), result in rdi */\
		"mulxq	%%r11,%%r11,%%r8 	\n\t"/* load x1 into r11; cy1 = MULH64(q,tmp1), result in r8  */\
		"mulxq	%%r12,%%r12,%%r9 	\n\t"/* load x2 into r12; cy2 = MULH64(q,tmp2), result in r9  */\
		"mulxq	%%r13,%%r13,%%rax	\n\t"/* load x3 into r13; cy3 = MULH64(q,tmp3), result in rax */\
	"subq	$1,%%rcx \n\t"\
	"jnz loop4u 	\n\t"/* loop1 end; continue is via jump-back if rcx != 0 */\
		"movq	%%rdi,%[__cy0]	\n\t	movq	%%r8 ,%[__cy1]	\n\t	movq	%%r9 ,%[__cy2]	\n\t	movq	%%rax,%[__cy3]	\n\t"\
	:	/* outputs: none */\
	: [__q] "m" (q)	/* All inputs from memory addresses here */\
	 ,[__qinv] "m" (qinv)	\
	 ,[__x] "m" (x)	\
	 ,[__cy0] "m" (cy0)	\
	 ,[__cy1] "m" (cy1)	\
	 ,[__cy2] "m" (cy2)	\
	 ,[__cy3] "m" (cy3)	\
	 ,[__len] "m" (len)	\
	: "cc","memory","rax","rbx","rcx","rdx","rsi","rdi","r8","r9","r10","r11","r12","r13","r14","r15"	/* Clobbered registers */\
	);

  #else		// Pre-Haswell version (no MULX instruction)
			// Note: MULQ assumes src1 in RAX, src2 in named register, lo"hi output halves into rax:rdx:

	__asm__ volatile (\
		"movq	%[__x],%%r10	\n\t"\
		"xorq	%%rdi,%%rdi		\n\t"/* Init cy0 = 0 */\
		"xorq	%%r8 ,%%r8 		\n\t"/* Init cy1 = 0 */\
		"xorq	%%r9 ,%%r9 		\n\t"/* Init cy2 = 0 */\
		"xorq	%%rdx,%%rdx		\n\t"/* Init cy3 = 0 */\
		"movq	%[__q],%%rsi	\n\t"\
		"movq	%[__qinv],%%rbx	\n\t"\
		"movslq	%[__len],%%r14	\n\t"/* ASM loop structured as for(j = len; j != 0; --j){...} */\
		"shrq	$1      ,%%r14	\n\t"/* len/2 */\
		"leaq	(%%r10,%%r14,8),%%r15	\n\t"/* x+len/2 */\
		"shrq	$1      ,%%r14	\n\t"/* len/4 */\
		"movq	%%r14,%%rcx	\n\t"/* Copy len/4 into loop counter*/\
	"loop4u:		\n\t"\
		"movq	(%%r10),%%rax	\n\t	leaq	(%%r10,%%r14,8),%%r11	\n\t"/* load x0,&x1 */\
		"addq	$0x8 ,%%r10		\n\t	movq	(%%r11),%%r11	\n\t"/* Increment x0-ptr, load x1 */\
		"subq	%%rdi,%%rax		\n\t	sbbq	%%rdi,%%rdi		\n\t"/* tmp0 = x0 - cy0, save -CF */\
		"subq	%%r8 ,%%r11		\n\t	sbbq	%%r8 ,%%r8 		\n\t"/* tmp1 = x1 - cy1, save -CF */\
		"movq	(%%r15),%%r12	\n\t	leaq	(%%r15,%%r14,8),%%r13	\n\t"/* load x2,&x3 */
		"addq	$0x8 ,%%r15		\n\t	movq	(%%r13),%%r13	\n\t"/* Increment x2-ptr, load x3 */\
		"subq	%%r9 ,%%r12		\n\t	sbbq	%%r9 ,%%r9 		\n\t"/* tmp2 = x2 - cy2, save -CF */\
		"subq	%%rdx,%%r13		\n\t	sbbq	%%rdx,%%rdx		\n\t"/* tmp3 = x3 - cy3, save -CF */\
		"imulq	%%rbx,%%rax 	\n\t	imulq	%%rbx,%%r11 	\n\t"/* tmp0,1 *= qinv */\
		"imulq	%%rbx,%%r12 	\n\t	imulq	%%rbx,%%r13 	\n\t"/* tmp2,3 *= qinv */\
		"subq	%%rdi,%%rax		\n\t	subq	%%r8 ,%%r11		\n\t"/* tmp0,1 = tmp0,1 * qinv + CF */\
		"subq	%%r9 ,%%r12		\n\t	subq	%%rdx,%%r13		\n\t"/* tmp2,3 = tmp2,3 * qinv + CF */\
		"								mulq	%%rsi	\n\t	movq	%%rdx,%%rdi	\n\t"/* cy0 = MULH64(q,tmp0), then move cy0 out of rdx in prep for cy1 computation */\
		"movq	%%r11,%%rax		\n\t	mulq	%%rsi	\n\t	movq	%%rdx,%%r8 	\n\t"/* cy1 = MULH64(q,tmp1), then move cy1 out of rdx in prep for cy2 computation */\
		"movq	%%r12,%%rax		\n\t	mulq	%%rsi	\n\t	movq	%%rdx,%%r9 	\n\t"/* cy2 = MULH64(q,tmp2), then move cy2 out of rdx in prep for cy3 computation */\
		"movq	%%r13,%%rax		\n\t	mulq	%%rsi	\n\t"/* load x3 into rax; cy3 = MULH64(q,tmp3), leave result in rdx */\
	"subq	$1,%%rcx \n\t"\
	"jnz loop4u 	\n\t"/* loop1 end; continue is via jump-back if rcx != 0 */\
		"movq	%%rdi,%[__cy0]	\n\t	movq	%%r8 ,%[__cy1]	\n\t	movq	%%r9 ,%[__cy2]	\n\t	movq	%%rdx,%[__cy3]	\n\t"\
	:	/* outputs: none */\
	: [__q] "m" (q)	/* All inputs from memory addresses here */\
	 ,[__qinv] "m" (qinv)	\
	 ,[__x] "m" (x)	\
	 ,[__cy0] "m" (cy0)	\
	 ,[__cy1] "m" (cy1)	\
	 ,[__cy2] "m" (cy2)	\
	 ,[__cy3] "m" (cy3)	\
	 ,[__len] "m" (len)	\
	: "cc","memory","rax","rbx","rcx","rdx","rsi","rdi","r8","r9","r10","r11","r12","r13","r14","r15"	/* Clobbered registers */\
	);

  #endif	// AVX2/MULX or not?

#endif

#if MI64_ISDIV_U4_DBG
	if(dbg)printf("Half-length carryouts: cy0-3 = %20llu, %20llu, %20llu, %20llu\n",cy0,cy1,cy2,cy3);
#endif
	// Compute radix-power; add 1 since used high-MUL version of the scaled-remainder algo ( = Algorithm A in the paper)
	rpow = radix_power64(q,qinv,len4+1);

	// Build up the sum of the scaled partial remainders, two terms at a time:
	MONT_MUL64(cy3,rpow,q,qinv,cy3);	cy2 += cy3;	if(cy2 < cy3 || cy2 >= q) cy2 -= q;	//               cy2 + cy3*B^p           (mod q)
	MONT_MUL64(cy2,rpow,q,qinv,cy2);	cy1 += cy2;	if(cy1 < cy2 || cy1 >= q) cy1 -= q;	//        cy1 + (cy2 + cy3*B^p)*B^p      (mod q)
	MONT_MUL64(cy1,rpow,q,qinv,cy1);	cy0 += cy1;	if(cy0 < cy1 || cy0 >= q) cy0 -= q;	// cy0 + (cy1 + (cy2 + cy3*B^p)*B^p)*B^p (mod q)
	// Negation (mod q) needed for Algo A scaled remainder
	if(cy0) cy0 = q-cy0 ;
#if MI64_ISDIV_U4_DBG
	if(dbg) printf("(sum0-3) mod q = %20llu, q = %20llu\n",cy0,q);
#endif
	// One more modmul of sum by same power of the base gives true remainder:
	MONT_MUL64(cy0,rpow,q,qinv,cy0);
#if MI64_ISDIV_U4_DBG
	if(dbg) {
		printf("True mod x mod q = %20llu\n",cy0);
		exit(0);
	}
#endif
	return (cy0 == 0);
}

/* Fast div-with-remainder with divisor 64-bit scalar using Montgomery modmul, my right-to-left mod
algorithm (as used in the strictly binary is-divisble-by test above), and my true-remainder postprocessing
step of May 2012.

Returns x % q in function result, and quotient x / q in y-vector, if one is provided. (Permits in-place, i.e. y == x).
*/
#if MI64_DEBUG
	#define MI64_DIV_MONT64		0
#endif
uint64 mi64_div_by_scalar64(const uint64 x[], uint64 q, uint32 len, uint64 y[])
{
	const char func[] = "mi64_div_by_scalar64";
#if MI64_DIV_MONT64
	int dbg = IS_EVEN(q);//(q == 8040689323464953445ull);
#endif
	uint32 i,nshift,lshift = -1,ptr_incr;
	uint64 qinv,tmp = 0,bw,cy,lo,rem64,rem_save = 0,itmp64,mask,*iptr;
	double fquo,fqinv;

	ASSERT(HERE, (x != 0) && (len != 0), "Null input array or length parameter!");
	ASSERT(HERE, q > 0, "0 modulus!");
	// Unit modulus needs special handling to return proper 0 remainder rather than 1:
	if(q == 1ull) {
		if(y) mi64_set_eq(y,x,len);
		return 0ull;
	}
	/* q must be odd for Montgomery-style modmul to work, so first shift off any low 0s.
	Unlike the binary-result is-divisible-by routines, we must restore the shift to the
	quotient and remainder at the end.

	Ex 1: 195 % 44 = 19, 195/44 = 4.
	After right-justifying divisor to get q = (44 >> 2) = 11, we must also right-shift 195 the same amount and save any off-shifted bits for later:
	(195 >> 2) = (192 + 3)/4 = 48, offshift = 3.
	We then compute 48/11 = 4 which is the true divisor, and re-add 3 to the resulting shift-restored remainder to get rem = (4 << 2) + 3 = 19.

	Ex 2: x=53:
	q =  5 -> x%q = 3
	q = 10 -> x%q = 3, above algo gives shifted rem = [53/2]%5 = 26%5 = 1, then restore as 2*1 + 53%2 = 2+1 = 3, ok
	q = 20 -> x%q =13, above algo gives shifted rem = [53/4]%5 = 13%5 = 3, then restore as 4*3 + 53%4 =12+1 =13, ok
	q = 40 -> x%q =13, above algo gives shifted rem = [53/8]%5 =  6%5 = 1, then restore as 8*1 + 53%8 = 8+5 =13, ok
	*/
	nshift = trailz64(q);
	if(nshift) {
		lshift = 64 - nshift;
		mask = ((uint64)-1 >> lshift);	// Save the bits which would be off-shifted if we actually right-shifted x[]
		rem_save = x[0] & mask;		// (Which we don`t do since x is read-only; thus we are forced into accounting tricks :)
		q >>= nshift;
	}
	ASSERT(HERE, (q & (uint64)1) == 1, "q must be odd!");

	uint32 q32,qi32;
	q32  = q; qi32 = minv8[(q&0xff)>>1];
	qi32 = qi32*((uint32)2 - q32*qi32);
	qi32 = qi32*((uint32)2 - q32*qi32);	qinv = qi32;
	qinv = qinv*((uint64)2 - q*qinv);

#if MI64_DIV_MONT64
	if(dbg) {
		printf("%s: nshift = %u, Input vector: x = 0;\n",func,nshift,q);
		for(i = 0; i < len; i++)
			printf("i = %u; x+=%20llu<<(i<<6);\n",i,x[i]);	// Pari-debug inputs; For every i++, shift count += 64
		printf("\n");
		printf("q = %20llu; qinv = %20llu\n",q,qinv);
	}
#endif

	cy = (uint64)0;
	if(!nshift) {	// Odd modulus, with or without quotient computation, uses Algo A
		for(i = 0; i < len; ++i) {
			tmp  = x[i] - cy;	// Need to add q if had a borrow - Since we do MULL(tmp,qinv) below and MULL(q*qinv) = 1,
								// can simply add 1 to tmp*qinv result instead of adding q to the difference here.
			cy = (cy > x[i]);	// Comparing this rather than (tmp > x[i]) frees up tmp for the MULL-by-qinv
			/* Now do the Montgomery mod: cy = umulh( q, mulq(tmp, qinv) ); */
		#if MI64_DIV_MONT64
			bw = cy;	// Save a copy of the borrow flag for debug-printing
			itmp64 = tmp + ((-cy)&q);	// Expected value of low-half of MUL_LOHI
	//		if(dbg)printf("i = %4u, tmp*qinv = %20llu\n",i,tmp*qinv);
		#endif
			tmp = tmp*qinv + cy;
			// Do double-wide product. Fast-divisibility test needs just high half (stored in cy); low half (tmp) needed to extract true-mod
		#ifdef MUL_LOHI64_SUBROUTINE
			MUL_LOHI64(q, tmp, &tmp,&cy);
		#else
			MUL_LOHI64(q, tmp,  tmp, cy);
		#endif
		#if MI64_DIV_MONT64
			if(dbg)printf("i = %4u, lo = %20llu, hi = %20llu, bw = %1u\n",i,tmp,cy,(uint32)bw);
			ASSERT(HERE, itmp64 == tmp, "Low-half product check mismatch!");
		#endif
		}
	} else {	// Even modulus, with or without quotient computation, uses Algo B
		if(!y) {	// If no y (quotient) array, use itmp64 to hold each shifted-x-array word:
			iptr = &itmp64;
			ptr_incr = 0;
		} else {	// Otherwise copy shifted words into y-array, which will hold the quotient on exit:
			iptr = y;
			ptr_incr = 1;
		}
		// Inline right-shift of x-vector with modding:
		for(i = 0; i < len-1; ++i, iptr += ptr_incr) {
			*iptr = (x[i] >> nshift) + (x[i+1] << lshift);
			tmp  = *iptr - cy;
			cy = (cy > *iptr);
		#if MI64_DIV_MONT64
			bw = cy;	// Save a copy of the borrow flag for debug-printing
			*iptr = tmp + ((-cy)&q);	// Expected value of low-half of MUL_LOHI
		#endif
			tmp = tmp*qinv + cy;
		#ifdef MUL_LOHI64_SUBROUTINE
			MUL_LOHI64(q, tmp, &tmp,&cy);
		#else
			MUL_LOHI64(q, tmp,  tmp, cy);
		#endif
		#if MI64_DIV_MONT64
			if(dbg)printf("i = %4u, lo = %20llu, hi = %20llu, bw = %1u\n",i,tmp,cy,(uint32)bw);
			ASSERT(HERE, *iptr == tmp, "Low-half product check mismatch!");
		#endif
		}
		// Last element has no shift-in from next-higher term, so can compute just the low-half output term, sans explicit MULs:
		*iptr = (x[i] >> nshift);
		tmp  = *iptr - cy;
		cy = (cy > *iptr);
		tmp = tmp + ((-cy)&q);
	#if MI64_DIV_MONT64
		if(dbg)printf("i = %4u, lo_out = %20llu\n",i,tmp);
	#endif
	}

	// Prepare to transform back out of "Montgomery space" ... first compute B^2 mod q using successive FP approximation:
	// Compute how many 2^48s there are in the quotient 2^96/q:
	fqinv = 1.0/q;

	// If len = 1, simply return x[0] % q = tmp % q:
	if(len == 1) {
		fquo = tmp*fqinv;
		itmp64 = (uint64)fquo;
		rem64 = tmp - q*itmp64;
		// May need a 2nd pass to clean up any ROE in 1st iteration, and
		// must account for ROE which leads to a borrow in the above subtraction to get rem64:
		if(rem64 > tmp) {	// Had a borrow
			fquo = -rem64*fqinv + 1;	// Add one to FP quotient to effectround-toward-zero
			itmp64 -= (uint64)fquo;
			rem64 = rem64 + q*(uint64)fquo;
		} else {
			fquo = rem64*fqinv;
			itmp64 += (uint64)fquo;
			rem64 = rem64 - q*(uint64)fquo;
		}
		ASSERT(HERE, rem64 == tmp%q, "Bad floating-mod mod!");
		if(y) {
			y[0] = itmp64;
		}
		if(nshift) {	// Mar 2015: BUG: Had left this restore-off-shifted-portion-to-remainder snip out of (len == 1) special-casing
			rem64 = (rem64 << nshift) + rem_save;
		}
		return rem64;
	}

	if(!nshift) {	// Odd modulus uses Algo A
		// Compute radix-power; no add-1 here since use scaled-remainder Algorithm B:
		rem64 = radix_power64(q,qinv,len+1);
		// And multiply the Montgomery residue from the mod-loop by the mod-power of the base:
		MONT_MUL64(cy,rem64,q,qinv,rem64);
		if(rem64) rem64 = q-rem64;
	} else {		// Even modulus uses Algo B
		// Compute radix-power; no add-1 here since use scaled-remainder Algorithm B:
		rem64 = radix_power64(q,qinv,len);
		// And multiply the Montgomery residue from the mod-loop by the mod-power of the base:
		MONT_MUL64(tmp,rem64,q,qinv,rem64);
	}

	// If we applied an initial right-justify shift to the modulus, restore the shift to the
	// current (partial) remainder and re-add the off-shifted part of the true remainder.
	rem64 = (rem64 << nshift) + rem_save;
#if MI64_DIV_MONT64
	if(dbg)printf("True mod: x mod q = %20llu\n",rem64);
#endif

	if(!y)	// Only remainder needed
		return rem64;

	// If y-array supplied, compute quotient and return it in that:
#if MI64_DIV_MONT64
	if(dbg)printf("Computing quotient...\n");
#endif
	// Now can use a simple loop and a sequence of word-size MULLs to obtain quotient.
	// Fusing the functionality of mi64_sub_scalar and the quotient extraction is fastest here:
	if(!nshift) {
		// If odd modulus, have not yet copied input array to y...
		bw = 0;	cy = rem64;
		for(i = 0; i < len; ++i) {
		#if MI64_DIV_MONT64
			if(dbg && i%(len>>2) == 0)printf("bw = %1llu, cy%1u = %20llu\n",bw,i/(len>>2),cy);	// Use to debug loop-folded implemntation
		#endif
			tmp = x[i] - bw - cy;
			/*  Since may be working in-place, need an extra temp here due to asymmetry of subtract: */
			bw = (tmp > x[i]);
		#if MI64_DIV_MONT64
			itmp64 = tmp;	// Expected value of low-half of MUL_LOHI; here the borrow gets subtracted from the next-higher word so no mod-q
		#endif
			tmp *= qinv;
		#ifdef MUL_LOHI64_SUBROUTINE
			MUL_LOHI64(q, tmp, &lo,&cy);
		#else
			MUL_LOHI64(q, tmp,  lo, cy);
		#endif
		#if MI64_DIV_MONT64
			if(dbg)printf("i = %4u, quot[i] = %20llu, lo1 = %20llu, lo2 = %20llu, hi = %20llu, bw = %1u\n",i,tmp,itmp64,lo,cy,(uint32)bw);
			ASSERT(HERE, itmp64 == lo, "Low-half product check mismatch!");
		#endif
			y[i] = tmp;
		}
	} else {
		// If even modulus, right-justified copy of input array already in y.
		bw = 0;	cy = rem64>>nshift;
		for(i = 0; i < len; ++i) {
			tmp = y[i] - bw - cy;
			/*  Since may be working in-place, need an extra temp here due to asymmetry of subtract: */
			bw = (tmp > y[i]);
			tmp *= qinv;
		#ifdef MUL_LOHI64_SUBROUTINE
			MUL_LOHI64(q, tmp, &lo,&cy);
		#else
			MUL_LOHI64(q, tmp,  lo, cy);
		#endif
		#if MI64_DIV_MONT64
			if(dbg)printf("i = %4u, quot[i] = %20llu\n",i,tmp);
		#endif
			y[i] = tmp;
		}
	}
	ASSERT(HERE, bw == 0 && cy == 0, "bw/cy check!");
#if MI64_DIV_MONT64
if(dbg) {
	printf("len = %u, q = %llu, nshift = %u, rem = %llu\n",len,q,nshift,rem64);
	printf("Quotient y = 0;\n",func);
	for(i = 0; i < len; i++)
		printf("i = %u; y+=%20llu<<(i<<6);\n",i,y[i]);	// Pari-debug inputs; For every i++, shift count += 64
	printf("\n");
}
#endif
	return rem64;
}

// 2-way loop splitting:
#if MI64_DEBUG
	#define MI64_DIV_MONT64_U2	0
#endif
uint64 mi64_div_by_scalar64_u2(uint64 x[], uint64 q, uint32 lenu, uint64 y[])	// x declared non-const in folded versions to permit 0-padding
{														// lenu = unpadded length
	if(lenu < 2) return mi64_div_by_scalar64(x,q,lenu,y);
#ifndef YES_ASM
	uint64 tmp0,tmp1,bw0,bw1;
#endif
#if MI64_DIV_MONT64_U2
	int dbg = 0;
#endif
	int i,j,npad = (lenu&1),len = lenu + npad,len2 = (len>>1),nshift,lshift = -1;	// Pad to even length
	uint64 qinv,cy0,cy1,rpow,rem_save = 0,xsave,itmp64,mask,*iptr0,*iptr1,ptr_incr;
	ASSERT(HERE, (x != 0) && (len != 0), "Null input array or length parameter!");
	ASSERT(HERE, q > 0, "0 modulus!");
	// Unit modulus needs special handling to return proper 0 remainder rather than 1:
	if(q == 1ull) {
		if(y) mi64_set_eq(y,x,len);
		return 0ull;
	}
	xsave = x[lenu];	x[lenu] = 0ull;	// Zero-pad one-beyond element to remove even-length restriction
										// Will restore input value of x[lenu] just prior to returning.
	/* q must be odd for Montgomery-style modmul to work, so first shift off any low 0s: */
	nshift = trailz64(q);
	if(nshift) {
		lshift = 64 - nshift;
		mask = ((uint64)-1 >> lshift);	// Save the bits which would be off-shifted if we actually right-shifted x[]
		rem_save = x[0] & mask;	// (Which we don`t do since x is read-only; thus we are forced into accounting tricks :)
		q >>= nshift;
	}

	uint32 q32,qi32;
	q32  = q; qi32 = minv8[(q&0xff)>>1];
	qi32 = qi32*((uint32)2 - q32*qi32);
	qi32 = qi32*((uint32)2 - q32*qi32);	qinv = qi32;
	qinv = qinv*((uint64)2 - q*qinv);

/* 07/30/2012: Compare full-DIV timings for the standard test, 10000 length-34420 DIVs:
Pure C:  3.585 sec --> 21 cycles
GCC ASM: 2.953 sec --> 17 cycles.
Bizarrely, switch just ONE of the 2 steps (either one) from C to ASM gives a negligible speedup; only doing both helps a lot.
[Possible code/loop alignment issue?]
See similar behavior for 4-way-split version of the algorithm.
*/
#ifndef YES_ASM

	cy0 = cy1 = (uint64)0;
	if(!nshift) {	// Odd modulus, with or without quotient computation, uses Algo A
		for(i = 0; i < len2; ++i) {
			tmp0  = x[i] - cy0;				tmp1 = x[i+len2] - cy1;
			cy0 = (cy0 > x[i]);				cy1 = (cy1 > x[i+len2]);
			tmp0 = tmp0*qinv + cy0;			tmp1 = tmp1*qinv + cy1;
		#ifdef MUL_LOHI64_SUBROUTINE
			cy0 = __MULH64(q,tmp0);			cy1 = __MULH64(q,tmp1);
		#else
			MULH64(q,tmp0, cy0);			MULH64(q,tmp1, cy1);
		#endif
		}
	} else {	// Even modulus, with or without quotient computation, uses Algo B
		if(!y) {	// If no y (quotient) array, use itmp64 to hold each shifted-x-array word:
			iptr0 = iptr1 = &itmp64;
			ptr_incr = 0;
		} else {	// Otherwise copy shifted words into y-array, which will hold the quotient on exit:
			iptr0 = y;	iptr1 = iptr0 + len2;
			ptr_incr = 1;
		}
		// Inline right-shift of x-vector with modding - 1 less loop-exec here due to shift-in from next-higher terms:
		for(i = 0, j = len2; i < len2-1; ++i, ++j, iptr0 += ptr_incr, iptr1 += ptr_incr) {
			*iptr0 = (x[i] >> nshift) + (x[i+1] << lshift);
			*iptr1 = (x[j] >> nshift) + (x[j+1] << lshift);
			tmp0 = *iptr0 - cy0;			tmp1 = *iptr1 - cy1;
			cy0 = (cy0 > *iptr0);			cy1 = (cy1 > *iptr1);
			tmp0 = tmp0*qinv + cy0;			tmp1 = tmp1*qinv + cy1;
		#ifdef MUL_LOHI64_SUBROUTINE
			cy0 = __MULH64(q,tmp0);			cy1 = __MULH64(q,tmp1);
		#else
			MULH64(q,tmp0, cy0);			MULH64(q,tmp1, cy1);
		#endif
		}
		// Last element has no shift-in from next-higher term, so can compute just low-half output term, sans explicit MULs:
		*iptr0 = (x[i] >> nshift) + (x[i+1] << lshift);
										*iptr1 = (x[j] >> nshift);
		tmp0 = *iptr0 - cy0;			tmp1 = *iptr1 - cy1;
		cy0 = (cy0 > *iptr0);			cy1 = (cy1 > *iptr1);
		cy0 = tmp0 + ((-cy0)&q);		cy1 = tmp1 + ((-cy1)&q);
	}

#else

	if(!nshift) {	// Odd modulus, with or without quotient computation, uses Algo A
	__asm__ volatile (\
		"movq	%[__x],%%r10	\n\t"\
		"xorq	%%rdi,%%rdi		\n\t"/* Init cy0 = 0 */\
		"xorq	%%rdx,%%rdx		\n\t"/* Init cy1 = 0 */\
		"movq	%[__q],%%rsi	\n\t"\
		"movq	%[__qinv],%%rbx	\n\t"\
		"movslq	%[__len2], %%rcx	\n\t"/* ASM loop structured as for(j = len2; j != 0; --j){...} */\
		"leaq	(%%r10,%%rcx,8),%%r11	\n\t"/* x + len2 */\
	"loop2b:		\n\t"\
		"movq	(%%r10),%%rax	\n\t	movq	(%%r11),%%r12	\n\t"/* load x0,x1 */\
		"addq	$0x8 ,%%r10		\n\t	addq	$0x8 ,%%r11		\n\t"/* Increment array pointers */\
		"subq	%%rdi,%%rax		\n\t	sbbq	%%rdi,%%rdi		\n\t"/* tmp0 = x0 - cy0, save -CF */\
		"subq	%%rdx,%%r12		\n\t	sbbq	%%rdx,%%rdx		\n\t"/* tmp1 = x1 - cy1, save -CF */\
		"imulq	%%rbx,%%rax 	\n\t	imulq	%%rbx,%%r12 	\n\t"/* tmp *= qinv */\
		"subq	%%rdi,%%rax		\n\t	subq	%%rdx,%%r12		\n\t"/* tmp = tmp*qinv + CF */\
		"mulq	%%rsi			\n\t	movq	%%rdx,%%rdi		\n\t"/* cy0 = MULH64(q,tmp0), then move cy0 out of rdx in prep for cy1 computation */\
		"movq	%%r12,%%rax		\n\t"/* load x1 into rax */\
		"mulq	%%rsi			\n\t"/* cy1 = MULH64(q,tmp1), leave result in rdx */\
	"subq	$1,%%rcx \n\t"\
	"jnz loop2b 	\n\t"/* loop1 end; continue is via jump-back if rcx != 0 */\
		"movq	%%rdi,%[__cy0]	\n\t	movq	%%rdx,%[__cy1]	\n\t"\
		:	/* outputs: none */\
		: [__q] "m" (q)	/* All inputs from memory addresses here */\
		 ,[__qinv] "m" (qinv)	\
		 ,[__x] "m" (x)	\
		 ,[__cy0] "m" (cy0)	\
		 ,[__cy1] "m" (cy1)	\
		 ,[__len2] "m" (len2)	\
		: "cc","memory","rax","rbx","rcx","rdx","rsi","rdi","r10","r11","r12"		/* Clobbered registers */\
		);
	} else {	// Even modulus, with or without quotient computation, uses Algo B
		if(!y) {	// If no y (quotient) array, use itmp64 to hold each shifted-x-array word:
			iptr0 = iptr1 = &itmp64;
			ptr_incr = 0;
		} else {	// Otherwise copy shifted words into y-array, which will hold the quotient on exit:
			iptr0 = y;	iptr1 = iptr0 + len2;
			ptr_incr = 8;	// Use bytewise incr in inline-ASM build mode
		}
		// Jun 2016: ASM-macro for Algo B loop is much instruction-heavier, but runs only ~5% slower than above Algo A macro for odd q:
	__asm__ volatile (\
		"movq	%[__x],%%r10		\n\t"/* Input array */\
		"movq	%[__iptr0],%%r8		\n\t"/* Output pointers (will both point to itmp64 if no quotient desired.) */\
		"movq	%[__iptr1],%%r9		\n\t"\
		"xorq	%%rdi,%%rdi			\n\t"/* Init cy0 = 0 */\
		"xorq	%%rdx,%%rdx			\n\t"/* Init cy1 = 0 */\
		"movq	%[__q],%%rsi		\n\t"\
		"movq	%[__qinv],%%rbx		\n\t"\
		"movslq	%[__len2],%%rcx		\n\t"/* ASM loop structured as for(j = len2-1; j != 0; --j){...} */\
		"leaq	(%%r10,%%rcx,8),%%r11	\n\t"/* x + len2 */\
		"subq	$1,%%rcx	\n\t"\
	"jz loop2c_b0 	\n\t"/* check rsi == 0 ? here and if so, don't exec the loop. */
		"movq	(%%r10),%%rax		\n\t	movq	(%%r11),%%r12	\n\t"/* SHRD allows mem-ref only in DEST, so preload x[i],x[j] */\
	"loop2c:		\n\t"\
		"movq	%%rcx,%%r15			\n\t"/* Move loop counter out of CL... */\
		"movslq	%[__n],%%rcx		\n\t"/* ...and move shift count in. */\
		"movq	0x8(%%r10),%%r13	\n\t	movq	0x8(%%r11),%%r14	\n\t"/* load x[i+1],x[j+1] */\
		"shrdq	%%cl,%%r13,%%rax	\n\t	shrdq	%%cl,%%r14,%%r12	\n\t"/* (x[i+1],x[i])>>n, (x[j+1],x[j])>>n */\
		"addq	$0x8 ,%%r10			\n\t	addq	$0x8 ,%%r11			\n\t"/* Increment input-array pointers */\
		"movq	%%rax,(%%r8)		\n\t	movq	%%r12,(%%r9)		\n\t"/* Write words of right-shifted input array */\
		"addq	%[__ptr_incr],%%r8	\n\t	addq	%[__ptr_incr],%%r9	\n\t"/* Increment output pointers */\
		"subq	%%rdi,%%rax			\n\t	sbbq	%%rdi,%%rdi			\n\t"/* tmp0 = x0 - cy0, save -CF */\
		"subq	%%rdx,%%r12			\n\t	sbbq	%%rdx,%%rdx			\n\t"/* tmp1 = x1 - cy1, save -CF */\
		"imulq	%%rbx,%%rax			\n\t	imulq	%%rbx,%%r12			\n\t"/* tmp *= qinv */\
		"subq	%%rdi,%%rax			\n\t	subq	%%rdx,%%r12			\n\t"/* tmp = tmp*qinv + CF */\
		"mulq	%%rsi				\n\t	movq	%%rdx,%%rdi			\n\t"/* cy0 = MULH64(q,tmp0), then move cy0 out of rdx in prep for cy1 computation */\
		"movq	%%r12,%%rax			\n\t"/* load x1 into rax */\
		"mulq	%%rsi				\n\t"/* cy1 = MULH64(q,tmp1), leave result in rdx */\
		"movq	%%r13,%%rax			\n\t	movq	%%r14,%%r12		\n\t"/* copy x[i+1],x[j+1] into current-element regs */\
		"movq	%%r15,%%rcx			\n\t"/* Restore loop counter to CL... */\
	"subq	$1,%%rcx	\n\t"\
	"jnz loop2c		\n\t"/* loop1 end; continue is via jump-back if rcx != 0 */\
		/* Last element (i = len2-1, j = len-1) has no shift-in from next-higher term: */\
	"loop2c_b0:		\n\t"\
		"movq	0x8(%%r10),%%r13	\n\t"/* load x[i+1] */\
		"movslq	%[__n],%%rcx		\n\t"/* move shift count into rcx */\
		"shrdq	%%cl,%%r13,%%rax	\n\t	shrq	%%cl,%%r12		\n\t"/* (x[i+1],x[i])>>n, x[j] */\
		"movq	%%rax,(%%r8)		\n\t	movq	%%r12,(%%r9)	\n\t"/* Write words of right-shifted input array */\
		"subq	%%rdi,%%rax			\n\t	sbbq	%%rdi,%%rdi		\n\t"/* tmp0 = x0 - cy0, overwrite cy0 with -CF */\
		"subq	%%rdx,%%r12			\n\t	sbbq	%%rdx,%%rdx		\n\t"/* tmp1 = x1 - cy1, overwrite cy1 with -CF */\
		"andq	%%rsi,%%rdi			\n\t	andq	%%rsi,%%rdx		\n\t"/* q & (-cy0|1) */\
		"addq	%%rdi,%%rax			\n\t	addq	%%rdx,%%r12		\n\t"/* cy0|1 = tmp0|1 + ((-cy0|1)&q) */\
		"movq	%%rax,%[__cy0]		\n\t	movq	%%r12,%[__cy1]	\n\t"\
		:	/* outputs: none */\
		: [__q] "m" (q)	/* All inputs from memory addresses here */\
		 ,[__qinv] "m" (qinv)	\
		 ,[__x] "m" (x)	\
		 ,[__cy0] "m" (cy0)	\
		 ,[__cy1] "m" (cy1)	\
		 ,[__len2] "m" (len2)	\
		 ,[__n] "m" (nshift)	\
		 ,[__iptr0] "m" (iptr0)	/* Output pointers (will both point to itmp64 if no quotient desired.) */\
		 ,[__iptr1] "m" (iptr1)	\
		 ,[__ptr_incr] "m" (ptr_incr)	\
		: "cc","memory","rax","rbx","rcx","rdx","rsi","rdi","r8","r9","r10","r11","r12","r13","r14","r14","r15"		/* Clobbered registers */\
		);
	}

#endif

#if MI64_DIV_MONT64_U2
	if(dbg)printf("Half-length carryouts: cy0 = %20llu, cy1 = %20llu\n",cy0,cy1);
#endif

	if(!nshift) {	// Odd modulus uses Algo A
		if(cy0) cy0 = q-cy0;
		if(cy1) cy1 = q-cy1;
	} else {		// Even modulus uses Algo B
		MONT_UNITY_MUL64(cy0,q,qinv,cy0);
		MONT_UNITY_MUL64(cy1,q,qinv,cy1);
	}
	// Compute radix-power; add-1 here since use scaled-remainder Algorithm A:
	rpow = radix_power64(q,qinv,len2+1);
	// Sum the scaled partial remainders:
	MONT_MUL64(cy1,rpow,q,qinv,cy1);	// cy1*B^p (mod q)
	cy0 += cy1;
	if(cy0 < cy1 || cy0 >= q) cy0 -= q;
	// One more modmul of sum by same power of the base gives full remainder in cy0, where we need it:
	MONT_MUL64(cy0,rpow,q,qinv,cy0);

#if MI64_DIV_MONT64_U2
	if(dbg) printf("True mod %c = %20llu\n",'A'+(nshift != 0),cy0);
#endif

	// If we applied an initial right-justify shift to the modulus, restore the shift to the
	// current (partial) remainder and re-add the off-shifted part of the true remainder.
	rpow = (cy0 << nshift) + rem_save;
	if(!y)	// Only remainder needed
		return rpow;

	/*** Here is loop-split quotient computation: ***/
	if(!nshift) {	// If odd modulus, have not yet copied input array to y...
		iptr0 = (uint64*)x;
	} else {	// If even modulus, right-justified copy of input array already in y.
		iptr0 = y;
	}
	iptr1 = iptr0 + len2;

#ifndef YES_ASM

	bw0 = bw1 = (uint64)0;
	for(i = 0; i < len2; ++i, ++iptr0, ++iptr1) {
		tmp0  = *iptr0 - bw0 - cy0;		tmp1 = *iptr1 - bw1 - cy1;
		/*  Since may be working in-place, need an extra temp here due to asymmetry of subtract: */
		bw0 = (tmp0 > *iptr0);			bw1 = (tmp1 > *iptr1);
		tmp0 *= qinv;					tmp1 *= qinv;
	#ifdef MUL_LOHI64_SUBROUTINE
		cy0 = __MULH64(q,tmp0);			cy1 = __MULH64(q,tmp1);
	#else
		MULH64(q,tmp0, cy0);			MULH64(q,tmp1, cy1);
	#endif
	#if MI64_DIV_MONT64_U2
		if(dbg)printf("quot[%2u] = %20llu, quot[%2u] = %20llu, bw0,1 = %1u,%1u, cy0,1 = %20llu,%20llu\n",i,tmp0,i+len2,tmp1,(uint32)bw0,(uint32)bw1,cy0,cy1);
	#endif
		// Write quotient word(s):
		y[i] = tmp0;					y[i+len2] = tmp1;
	}

#elif 0

	__asm__ volatile (\
		"movq	%[__iptr0],%%r10	\n\t"/* Input pointers (point to x,x+len2 if q odd, y,y+len2 if q even) */\
		"movq	%[__iptr1],%%r11	\n\t"\
		"movq	%[__y],%%r13		\n\t"\
		"movq	%[__q],%%rsi		\n\t"\
		"movq	%[__qinv],%%rbx		\n\t"\
		"xorq	%%r8,%%r8			\n\t	xorq	%%r9,%%r9		\n\t"/* Init bw0,1 = 0 */\
		"movq	%[__cy0],%%rdi		\n\t	movq	%[__cy1],%%rdx	\n\t"/* Load cy0, cy1 */\
		"movslq	%[__len2],%%rcx		\n\t"/* ASM loop structured as for(j = len2; j != 0; --j){...} */\
		"leaq (%%r13,%%rcx,8),%%r14	\n\t"/* y+len/2 */\
	"loop2d:		\n\t"
		"subq	%%r8,%%rdi			\n\t	subq	%%r9,%%rdx		\n\t"/* [rdi,r12] = (cy + bw)[0,1] via cy -(-bw), no carryout */\
		"movq	(%%r10),%%rax		\n\t	movq	(%%r11),%%r12	\n\t"/* load x0,x1 */\
		"subq	%%rdi,%%rax			\n\t	sbbq	%%r8,%%r8		\n\t"/* tmp0 = x0 - (bw0 + cy0), save -CF */\
		"subq	%%rdx,%%r12			\n\t	sbbq	%%r9,%%r9		\n\t"/* tmp1 = x1 - (bw1 + cy1), save -CF */\
		"addq	$0x8,%%r10			\n\t	addq	$0x8,%%r11		\n\t"/* Increment x-array pointers */\
		"imulq	%%rbx,%%rax			\n\t	imulq	%%rbx,%%r12		\n\t"/* tmp *= qinv */\
		"movq	%%rax,(%%r13)		\n\t	movq	%%r12,(%%r14)	\n\t"/* store tmp0,1 */\
		"addq	$0x8,%%r13			\n\t	addq	$0x8,%%r14		\n\t"/* Increment y-array pointers */\
		"mulq	%%rsi				\n\t	movq	%%rdx,%%rdi		\n\t"/* cy0 = MULH64(q,tmp0), then move cy0 out of rdx in prep for cy1 computation */\
		"movq	%%r12,%%rax			\n\t	mulq	%%rsi			\n\t"/* load tmp1 into rax, then cy1 = MULH64(q,tmp1) */\
	"subq	$1,%%rcx	\n\t"\
	"jnz loop2d			\n\t"/* loop1 end; continue is via jump-back if rcx != 0 */\
		"movq	%%rdi,%[__cy0]	\n\t	movq	%%rdx,%[__cy1]	"\
		:	/* outputs: none */\
		: [__q] "m" (q)	/* All inputs from memory addresses here */\
		 ,[__qinv] "m" (qinv)	\
		 ,[__iptr0] "m" (iptr0)	/* Input pointers (point to x,x+len2 if q odd, y,y+len2 if q even) */\
		 ,[__iptr1] "m" (iptr1)	\
		 ,[__y] "m" (y)	\
		 ,[__cy0] "m" (cy0)	\
		 ,[__cy1] "m" (cy1)	\
		 ,[__len2] "m" (len2)	\
		: "cc","memory","rax","rbx","rcx","rdx","rsi","rdi","r8","r9","r10","r11","r12","r13","r14"		/* Clobbered registers */\
		);

#else	// Low-register version; use similar for 4-folded quotient loop:

	__asm__ volatile (\
		"movq	%[__iptr0],%%r10	\n\t"/* Input pointers (point to x,x+len2 if q odd, y,y+len2 if q even) */\
		"movq	%[__iptr1],%%r11	\n\t"\
		"movq	%[__y],%%r13		\n\t"\
		"movq	%[__q],%%rsi		\n\t"\
		"movq	%[__qinv],%%rbx		\n\t"\
		"xorq	%%r8,%%r8			\n\t	xorq	%%r9,%%r9		\n\t"/* Init bw0,1 = 0 */\
		"subq	%[__cy0],%%r8		\n\t	movq	%[__cy1],%%rdx	\n\t"/* Load -(bw0 + cy0), +(bw1 + cy1) */\
		"movslq	%[__len2],%%rcx		\n\t"/* ASM loop structured as for(j = len2; j != 0; --j){...} */\
		"leaq (%%r13,%%rcx,8),%%r14	\n\t"/* y+len/2 */\
	"loop2d:		\n\t"
		"negq	%%r8				\n\t	subq	%%r9,%%rdx		\n\t"/* r8 = +(bw0 + cy0), r12 = +(bw1 + cy1) via cy1 -(-bw1), no carryout possible */\
		"movq	(%%r10),%%rax		\n\t	movq	(%%r11),%%r12	\n\t"/* load x0,x1 */\
		"subq	%%r8,%%rax			\n\t	sbbq	%%r8,%%r8		\n\t"/* tmp0 = x0 - (bw0 + cy0), save -CF */\
		"subq	%%rdx,%%r12			\n\t	sbbq	%%r9,%%r9		\n\t"/* tmp1 = x1 - (bw1 + cy1), save -CF */\
		"addq	$0x8,%%r10			\n\t	addq	$0x8,%%r11		\n\t"/* Increment x-array pointers */\
		"imulq	%%rbx,%%rax			\n\t	imulq	%%rbx,%%r12		\n\t"/* tmp *= qinv */\
		"movq	%%rax,(%%r13)		\n\t	movq	%%r12,(%%r14)	\n\t"/* store tmp0,1 */\
		"addq	$0x8,%%r13			\n\t	addq	$0x8,%%r14		\n\t"/* Increment y-array pointers */\
		"mulq	%%rsi				\n\t	subq	%%rdx,%%r8		\n\t"/* cy0 = MULH64(q,tmp0), move cy0 out of rdx into -(bw0 + cy0) in prep for cy1 computation */\
		"movq	%%r12,%%rax			\n\t	mulq	%%rsi			\n\t"/* load tmp1 into rax, then cy1 = MULH64(q,tmp1) */\
	"subq	$1,%%rcx	\n\t"\
	"jnz loop2d			\n\t"/* loop1 end; continue is via jump-back if rcx != 0 */\
		"							\n\t	movq	%%rdx,%[__cy1]	"/* Only useful carryout is cy1, check-equal-to-zero */\
		:	/* outputs: none */\
		: [__q] "m" (q)	/* All inputs from memory addresses here */\
		 ,[__qinv] "m" (qinv)	\
		 ,[__iptr0] "m" (iptr0)	/* Input pointers (point to x,x+len2 if q odd, y,y+len2 if q even) */\
		 ,[__iptr1] "m" (iptr1)	\
		 ,[__y] "m" (y)	\
		 ,[__cy0] "m" (cy0)	\
		 ,[__cy1] "m" (cy1)	\
		 ,[__len2] "m" (len2)	\
		: "cc","memory","rax","rbx","rcx","rdx","rsi","r8","r9","r10","r11","r12","r13","r14"		/* Clobbered registers */\
		);

#endif

	ASSERT(HERE, cy1 == 0, "cy check!");	// all but the uppermost carryout are generally nonzero
	x[lenu] = xsave;	// Restore input value of zero-padding one-beyond element x[lenu] prior to return
	return rpow;
}

// 4-way loop splitting:
#if MI64_DEBUG
	#define MI64_DIV_MONT64_U4	0
#endif
/*
GCC with -O3 consistently gives "Assembler messages: Error: symbol `loop4r_a' is already defined"
for the inline-asm loop body in this - and only this - mi64 function. Switching the opt-flag
to -Os allows the same file to compile successfully which indicates it's an unroll-related problem,
but we want to be able to build the whole codebase using -O3. Workaround is to switch from the above
kinds of unique-in-this-sourcefile named labels to local labels inside the ASM:
*/
// x declared non-const in folded versions to permit 0-padding:
uint64 mi64_div_by_scalar64_u4(uint64 x[], uint64 q, uint32 lenu, uint64 y[])
{														// lenu = unpadded length
	if(lenu < 4) return mi64_div_by_scalar64(x,q,lenu,y);
#ifndef YES_ASM
	uint32 i0,i1,i2,i3;
	uint64 tmp0,tmp1,tmp2,tmp3,bw0,bw1,bw2,bw3;
#endif
#if MI64_DIV_MONT64_U4
	int dbg = 0;
#endif
	int i,j,len = (lenu+3) & ~0x3,len2 = (len>>1),len4 = (len>>2),npad = len-lenu,nshift,lshift = -1;	// Pad to multiple-of-4 length
	uint64 qinv,cy0,cy1,cy2,cy3,rpow,rem_save = 0,itmp64,mask,*iptr0,*iptr1,*iptr2,*iptr3, ptr_incr,ptr_inc2;
	uint64 *xy_ptr_diff, pads[3];
	// Local-alloc-related statics - these should only ever be updated in single-thread mode:
	static int first_entry = TRUE;
	static uint32 len_save = 10;	// Initial-alloc values
	static uint64 *svec = 0x0;	// svec = "scratch vector"
	if(first_entry) {
		first_entry = FALSE;
		svec = (uint64 *)calloc(len_save, sizeof(uint64));
	}
	if(len > len_save) {
		len_save = len<<1;
		svec = (uint64 *)realloc(svec, len_save*sizeof(uint64));
	}

	ASSERT(HERE, (x != 0) && (len != 0), "Null input array or length parameter!");
	ASSERT(HERE, q > 0, "0 modulus!");
	// Unit modulus needs special handling to return proper 0 remainder rather than 1:
	if(q == 1ull) {
		if(y) mi64_set_eq(y,x,len);
		return 0ull;
	}

	// Zero-pad to next-higher multiple of 4 to remove length == 0 (mod 4) restriction.
	// Will restore input values of the 0-pad elements just prior to returning:
	for(i = 0; i < npad; i++) {
		pads[i] = x[lenu+i];	x[lenu+i] = 0ull;
	}

	/* q must be odd for Montgomery-style modmul to work, so first shift off any low 0s: */
	nshift = trailz64(q);
	if(nshift) {
		lshift = 64 - nshift;
		mask = ((uint64)-1 >> lshift);	// Save the bits which would be off-shifted if we actually right-shifted x[]
		rem_save = x[0] & mask;	// (Which we don`t do since x is read-only; thus we are forced into accounting tricks :)
		q >>= nshift;
		xy_ptr_diff = (uint64*)0;	// In even-modulus case, we use remaindering loop to store a right-justified copy
				// of the input (x) vector in any output (y) vector, i.e. quotient loop uses y-vector as in-and-output.
	} else {
		xy_ptr_diff = (uint64*)((uint64)y - (uint64)x);	// 2-step cast to avoid GCC "initialization makes pointer from integer without a cast" warning
						// The inner (uint64) casts are needed for the compiler to emit correctly functioning code.
	}

	uint32 q32,qi32;
	q32  = q; qi32 = minv8[(q&0xff)>>1];
	qi32 = qi32*((uint32)2 - q32*qi32);
	qi32 = qi32*((uint32)2 - q32*qi32);	qinv = qi32;
	qinv = qinv*((uint64)2 - q*qinv);

	if(!nshift) {		// If odd modulus, have not yet copied input array to y...
		iptr0 = (uint64*)x;
	} else if (y) {		// If even modulus, right-justified copy of input array writen to quotient vec, if available...
		iptr0 = y;
	} else {			// ...or into local-alloc scratch vec if no quotient vec [AVX2 mode only;
		iptr0 = svec;	// otherwise each word of x>>n discarded after it is used in REM accumulation]
	}

#ifndef YES_ASM

	cy0 = cy1 = cy2 = cy3 = (uint64)0;
	if(!nshift) {	// Odd modulus, with or without quotient computation, uses Algo A

		for(i0 = 0, i1 = len4, i2 = i1+i1, i3 = i1+i2; i0 < len4; ++i0, ++i1, ++i2, ++i3) {
			tmp0 = x[i0] - cy0;			tmp1 = x[i1] - cy1;			tmp2 = x[i2] - cy2;			tmp3 = x[i3] - cy3;
			cy0 = (cy0 > x[i0]);		cy1 = (cy1 > x[i1]);		cy2 = (cy2 > x[i2]);		cy3 = (cy3 > x[i3]);
			tmp0 = tmp0*qinv + cy0;		tmp1 = tmp1*qinv + cy1;		tmp2 = tmp2*qinv + cy2;		tmp3 = tmp3*qinv + cy3;
		#ifdef MUL_LOHI64_SUBROUTINE
			cy0 = __MULH64(q,tmp0);		cy1 = __MULH64(q,tmp1);		cy2 = __MULH64(q,tmp2);		cy3 = __MULH64(q,tmp3);
		#else
			MULH64(q,tmp0, cy0);		MULH64(q,tmp1, cy1);		MULH64(q,tmp2, cy2);		MULH64(q,tmp3, cy3);
		#endif
		}

	} else if(!y) {	// Even modulus, no quotient computation, uses Algo B

		// Inline right-shift of x-vector with modding - 1 less loop-exec here due to shift-in from next-higher terms:
		for(i0 = 0, i1 = len4, i2 = i1+i1, i3 = i1+i2; i0 < len4-1; ++i0, ++i1, ++i2, ++i3, iptr0 += ptr_incr, iptr1 += ptr_incr, iptr2 += ptr_incr, iptr3 += ptr_incr) {
			bw0 = (x[i0] >> nshift) + (x[i0+1] << lshift);
			bw1 = (x[i1] >> nshift) + (x[i1+1] << lshift);
			bw2 = (x[i2] >> nshift) + (x[i2+1] << lshift);
			bw3 = (x[i3] >> nshift) + (x[i3+1] << lshift);
			tmp0 = bw0 - cy0;		tmp1 = bw1 - cy1;		tmp2 = bw2 - cy2;		tmp3 = bw3 - cy3;
			cy0 = (cy0 > bw0);		cy1 = (cy1 > bw1);		cy2 = (cy2 > bw2);		cy3 = (cy3 > bw3);
			tmp0 = tmp0*qinv + cy0;		tmp1 = tmp1*qinv + cy1;		tmp2 = tmp2*qinv + cy2;		tmp3 = tmp3*qinv + cy3;
		#ifdef MUL_LOHI64_SUBROUTINE
			cy0 = __MULH64(q,tmp0);		cy1 = __MULH64(q,tmp1);		cy2 = __MULH64(q,tmp2);		cy3 = __MULH64(q,tmp3);
		#else
			MULH64(q,tmp0, cy0);		MULH64(q,tmp1, cy1);		MULH64(q,tmp2, cy2);		MULH64(q,tmp3, cy3);
		#endif
		}
		// Last element has no shift-in from next-higher term, so can compute just low-half output term, sans explicit MULs:
		bw0 = (x[i0] >> nshift) + (x[i0+1] << lshift);
		bw1 = (x[i1] >> nshift) + (x[i1+1] << lshift);
		bw2 = (x[i2] >> nshift) + (x[i2+1] << lshift);
		bw3 = (x[i3] >> nshift);
		tmp0 = bw0 - cy0;		tmp1 = bw1 - cy1;		tmp2 = bw2 - cy2;		tmp3 = bw3 - cy3;
		cy0 = (cy0 > bw0);		cy1 = (cy1 > bw1);		cy2 = (cy2 > bw2);		cy3 = (cy3 > bw3);
		cy0 = tmp0 + ((-cy0)&q);	cy1 = tmp1 + ((-cy1)&q);	cy2 = tmp2 + ((-cy2)&q);	cy3 = tmp3 + ((-cy3)&q);

	} else {	// Even modulus, with quotient computation, uses Algo B

		iptr0 = y; iptr1 = iptr0 + len4; iptr2 = iptr1 + len4; iptr3 = iptr2 + len4;
		// Inline right-shift of x-vector with modding - 1 less loop-exec here due to shift-in from next-higher terms:
		for(i0 = 0, i1 = len4, i2 = i1+i1, i3 = i1+i2; i0 < len4-1; ++i0, ++i1, ++i2, ++i3, iptr0++, iptr1++, iptr2++, iptr3++) {
			*iptr0 = (x[i0] >> nshift) + (x[i0+1] << lshift);
			*iptr1 = (x[i1] >> nshift) + (x[i1+1] << lshift);
			*iptr2 = (x[i2] >> nshift) + (x[i2+1] << lshift);
			*iptr3 = (x[i3] >> nshift) + (x[i3+1] << lshift);
			tmp0 = *iptr0 - cy0;		tmp1 = *iptr1 - cy1;		tmp2 = *iptr2 - cy2;		tmp3 = *iptr3 - cy3;
			cy0 = (cy0 > *iptr0);		cy1 = (cy1 > *iptr1);		cy2 = (cy2 > *iptr2);		cy3 = (cy3 > *iptr3);
			tmp0 = tmp0*qinv + cy0;		tmp1 = tmp1*qinv + cy1;		tmp2 = tmp2*qinv + cy2;		tmp3 = tmp3*qinv + cy3;
		#ifdef MUL_LOHI64_SUBROUTINE
			cy0 = __MULH64(q,tmp0);		cy1 = __MULH64(q,tmp1);		cy2 = __MULH64(q,tmp2);		cy3 = __MULH64(q,tmp3);
		#else
			MULH64(q,tmp0, cy0);		MULH64(q,tmp1, cy1);		MULH64(q,tmp2, cy2);		MULH64(q,tmp3, cy3);
		#endif
		}
		// Last element has no shift-in from next-higher term, so can compute just low-half output term, sans explicit MULs:
		*iptr0 = (x[i0] >> nshift) + (x[i0+1] << lshift);
		*iptr1 = (x[i1] >> nshift) + (x[i1+1] << lshift);
		*iptr2 = (x[i2] >> nshift) + (x[i2+1] << lshift);
		*iptr3 = (x[i3] >> nshift);
		tmp0 = *iptr0 - cy0;		tmp1 = *iptr1 - cy1;		tmp2 = *iptr2 - cy2;		tmp3 = *iptr3 - cy3;
		cy0 = (cy0 > *iptr0);		cy1 = (cy1 > *iptr1);		cy2 = (cy2 > *iptr2);		cy3 = (cy3 > *iptr3);
		cy0 = tmp0 + ((-cy0)&q);	cy1 = tmp1 + ((-cy1)&q);	cy2 = tmp2 + ((-cy2)&q);	cy3 = tmp3 + ((-cy3)&q);
	}

#else

	iptr1 = iptr0 + len4; iptr2 = iptr1 + len4; iptr3 = iptr2 + len4;
	if(!y) {
		ptr_incr = 0;
		ptr_inc2 = 0;	// bytewise distance (iptr2-iptr0) and (iptr3-iptr1)
	} else {
		ptr_incr = 8;	// Use bytewise incr in inline-ASM build mode
		ptr_inc2 = len2<<3;	// bytewise distance (iptr2-iptr0) and (iptr3-iptr1)
	}

  #ifdef USE_AVX2	// AVX2 version: Have a MULX instruction, full 256-bit (YMM-register-width) vector-int support.
					// Note: MULX reg1,reg2,reg3 [AT&T/GCC syntax] assumes src1 in RDX
					// [MULQ assumed RAX], src2 in named reg1, lo"hi output halves into reg2:reg3.
					// If only MULH result desired, set output-regs same: reg2 = reg3.

	/*
	For even modulus, compute x>>n and store in quotient-vec (if there is one) or scratch storage -
	the pointer iptr0 encodes the relevant such output-address.
	Intel crapified the performance of SH[L,R]D post-Core2, and use of same for inlined input-vector
	right-shift leads to major register pressure to boot (cf. the pre-AVX2 inlined-shift macros below)
	where I did such inlining in a specialized even-modulus version of the remaindering loop), so
	for our flagship 4-folded DIVREM and with a view toward good performance on AVX2 and beyond,
	do input-vector-right-shift separately via a separate preprocessing macro. AVX2 uses 256-bit vector-int
	instructions for this step, since speed of same more than compensates for need to do special handling
	of the shifted-off bits of the individual quadwords of each 256-bit chunk:
	*/
	if(nshift) {
		mi64_shrl_short(x,iptr0,nshift,len);	// Aug 2016: AVX2 build ~0.7 cycles/limb on Haswell
	}
	// Remaindering loop now looks the same - only the __x := iptr0 value differs - irrespective of odd or even modulus:
	__asm__ volatile (\
		"movq	%[__x],%%r10	\n\t"\
		"xorq	%%rdi,%%rdi		\n\t"/* Init cy0 = 0 */\
		"xorq	%%r8 ,%%r8 		\n\t"/* Init cy1 = 0 */\
		"xorq	%%r9 ,%%r9 		\n\t"/* Init cy2 = 0 */\
		"xorq	%%rax,%%rax		\n\t"/* Init cy3 = 0 */\
		"movq	%[__q],%%rdx	\n\t"/* In context of MULX want this fixed word in RDX, the implied-MULX-input register */\
		"movq	%[__qinv],%%rbx	\n\t"\
		"movslq	%[__len],%%r14	\n\t"/* ASM loop structured as for(j = len; j != 0; --j){...} */\
		"shrq	$1      ,%%r14	\n\t"/* len/2 */\
		"leaq	(%%r10,%%r14,8),%%r15	\n\t"/* x+len/2 */\
		"shrq	$1      ,%%r14	\n\t"/* len/4 */\
		"movq	%%r14,%%rcx	\n\t"/* Copy len/4 into loop counter*/\
	"0:	\n\t"/*** Needed to switch -O3 ==> -Os to work around "symbol `loop4r_a' is already defined" error, so instead use local label ***/\
		"movq	(%%r10),%%rsi	\n\t	leaq	(%%r10,%%r14,8),%%r11	\n\t"/* load x0,&x1 */\
		"addq	$0x8 ,%%r10		\n\t	movq	(%%r11),%%r11	\n\t"/* Increment x0-ptr, load x1 */\
		"subq	%%rdi,%%rsi		\n\t	sbbq	%%rdi,%%rdi		\n\t"/* tmp0 = x0 - cy0, save -CF */\
		"subq	%%r8 ,%%r11		\n\t	sbbq	%%r8 ,%%r8 		\n\t"/* tmp1 = x1 - cy1, save -CF */\
		"movq	(%%r15),%%r12	\n\t	leaq	(%%r15,%%r14,8),%%r13	\n\t"/* load x2,&x3 */
		"addq	$0x8 ,%%r15		\n\t	movq	(%%r13),%%r13	\n\t"/* Increment x2-ptr, load x3 */\
		"subq	%%r9 ,%%r12		\n\t	sbbq	%%r9 ,%%r9 		\n\t"/* tmp2 = x2 - cy2, save -CF */\
		"subq	%%rax,%%r13		\n\t	sbbq	%%rax,%%rax		\n\t"/* tmp3 = x3 - cy3, save -CF */\
		"imulq	%%rbx,%%rsi 	\n\t	imulq	%%rbx,%%r11 	\n\t"/* tmp0,1 *= qinv */\
		"imulq	%%rbx,%%r12 	\n\t	imulq	%%rbx,%%r13 	\n\t"/* tmp2,3 *= qinv */\
		"subq	%%rdi,%%rsi		\n\t	subq	%%r8 ,%%r11		\n\t"/* tmp0,1 = tmp0,1 * qinv + CF */\
		"subq	%%r9 ,%%r12		\n\t	subq	%%rax,%%r13		\n\t"/* tmp2,3 = tmp2,3 * qinv + CF */\
		/* In each MULX, overwrite the explicit input register with the low (discard) product half: */\
		"mulxq	%%rsi,%%rsi,%%rdi	\n\t"/* x0 enters in rsi; cy0 = MULH64(q,tmp0), result in rdi */\
		"mulxq	%%r11,%%r11,%%r8 	\n\t"/* load x1 into r11; cy1 = MULH64(q,tmp1), result in r8  */\
		"mulxq	%%r12,%%r12,%%r9 	\n\t"/* load x2 into r12; cy2 = MULH64(q,tmp2), result in r9  */\
		"mulxq	%%r13,%%r13,%%rax	\n\t"/* load x3 into r13; cy3 = MULH64(q,tmp3), result in rax */\
	"subq	$1,%%rcx \n\t"\
	"jnz 0b \n\t"/* loop1 end; continue is via jump-back if rcx != 0 */\
		"movq	%%rdi,%[__cy0]	\n\t	movq	%%r8 ,%[__cy1]	\n\t	movq	%%r9 ,%[__cy2]	\n\t	movq	%%rax,%[__cy3]	\n\t"\
	:	/* outputs: none */\
	: [__q] "m" (q)	/* All inputs from memory addresses here */\
	 ,[__qinv] "m" (qinv)	\
	 ,[__x] "m" (iptr0)	\
	 ,[__cy0] "m" (cy0)	\
	 ,[__cy1] "m" (cy1)	\
	 ,[__cy2] "m" (cy2)	\
	 ,[__cy3] "m" (cy3)	\
	 ,[__len] "m" (len)	\
	: "cc","memory","rax","rbx","rcx","rdx","rsi","rdi","r8","r9","r10","r11","r12","r13","r14","r15"	/* Clobbered registers */\
	);

  #else		// Pre-AVX2 version: no MULX instruction, only 128-bit (half-YMM-register-width) vector-int support.
			// For even-modulus case, use inlined-double-precision (SHRD) dividend-vector shift because SSE2-based
			// unlikely to be any faster.
			// Note: MULQ assumes src1 in RAX, src2 in named register, lo"hi output halves into rax:rdx:

	if(!nshift) {	// Odd modulus, with or without quotient computation, uses Algo A

	__asm__ volatile (\
		"movq	%[__x],%%r10	\n\t"\
		"xorq	%%rdi,%%rdi		\n\t"/* Init cy0 = 0 */\
		"xorq	%%r8 ,%%r8 		\n\t"/* Init cy1 = 0 */\
		"xorq	%%r9 ,%%r9 		\n\t"/* Init cy2 = 0 */\
		"xorq	%%rdx,%%rdx		\n\t"/* Init cy3 = 0 */\
		"movq	%[__q],%%rsi	\n\t"\
		"movq	%[__qinv],%%rbx	\n\t"\
		"movslq	%[__len],%%r14	\n\t"/* ASM loop structured as for(j = len; j != 0; --j){...} */\
		"shrq	$1      ,%%r14	\n\t"/* len/2 */\
		"leaq	(%%r10,%%r14,8),%%r15	\n\t"/* x+len/2 */\
		"shrq	$1      ,%%r14	\n\t"/* len/4 */\
		"movq	%%r14,%%rcx	\n\t"/* Copy len/4 into loop counter*/\
	"0:	\n\t"/*** Needed to switch -O3 ==> -Os to work around "symbol `loop4r_a' is already defined" error, so instead use local label ***/\
		"movq	(%%r10),%%rax	\n\t	leaq	(%%r10,%%r14,8),%%r11	\n\t"/* load x0,&x1 */\
		"addq	$0x8 ,%%r10		\n\t	movq	(%%r11),%%r11	\n\t"/* Increment x0-ptr, load x1 */\
		"subq	%%rdi,%%rax		\n\t	sbbq	%%rdi,%%rdi		\n\t"/* tmp0 = x0 - cy0, save -CF */\
		"subq	%%r8 ,%%r11		\n\t	sbbq	%%r8 ,%%r8 		\n\t"/* tmp1 = x1 - cy1, save -CF */\
		"movq	(%%r15),%%r12	\n\t	leaq	(%%r15,%%r14,8),%%r13	\n\t"/* load x2,&x3 */
		"addq	$0x8 ,%%r15		\n\t	movq	(%%r13),%%r13	\n\t"/* Increment x2-ptr, load x3 */\
		"subq	%%r9 ,%%r12		\n\t	sbbq	%%r9 ,%%r9 		\n\t"/* tmp2 = x2 - cy2, save -CF */\
		"subq	%%rdx,%%r13		\n\t	sbbq	%%rdx,%%rdx		\n\t"/* tmp3 = x3 - cy3, save -CF */\
		"imulq	%%rbx,%%rax 	\n\t	imulq	%%rbx,%%r11 	\n\t"/* tmp0,1 *= qinv */\
		"imulq	%%rbx,%%r12 	\n\t	imulq	%%rbx,%%r13 	\n\t"/* tmp2,3 *= qinv */\
		"subq	%%rdi,%%rax		\n\t	subq	%%r8 ,%%r11		\n\t"/* tmp0,1 = tmp0,1 * qinv + CF */\
		"subq	%%r9 ,%%r12		\n\t	subq	%%rdx,%%r13		\n\t"/* tmp2,3 = tmp2,3 * qinv + CF */\
		"								mulq	%%rsi	\n\t	movq	%%rdx,%%rdi	\n\t"/* cy0 = MULH64(q,tmp0), then move cy0 out of rdx in prep for cy1 computation */\
		"movq	%%r11,%%rax		\n\t	mulq	%%rsi	\n\t	movq	%%rdx,%%r8 	\n\t"/* cy1 = MULH64(q,tmp1), then move cy1 out of rdx in prep for cy2 computation */\
		"movq	%%r12,%%rax		\n\t	mulq	%%rsi	\n\t	movq	%%rdx,%%r9 	\n\t"/* cy2 = MULH64(q,tmp2), then move cy2 out of rdx in prep for cy3 computation */\
		"movq	%%r13,%%rax		\n\t	mulq	%%rsi	\n\t"/* load x3 into rax; cy3 = MULH64(q,tmp3), leave result in rdx */\
	"subq	$1,%%rcx \n\t"\
	"jnz 0b	\n\t"/* loop1 end; continue is via jump-back if rcx != 0 */\
		"movq	%%rdi,%[__cy0]	\n\t	movq	%%r8 ,%[__cy1]	\n\t	movq	%%r9 ,%[__cy2]	\n\t	movq	%%rdx,%[__cy3]	\n\t"\
		:	/* outputs: none */\
		: [__q] "m" (q)	/* All inputs from memory addresses here */\
		 ,[__qinv] "m" (qinv)	\
		 ,[__x] "m" (x)	\
		 ,[__cy0] "m" (cy0)	\
		 ,[__cy1] "m" (cy1)	\
		 ,[__cy2] "m" (cy2)	\
		 ,[__cy3] "m" (cy3)	\
		 ,[__len] "m" (len)	\
		: "cc","memory","rax","rbx","rcx","rdx","rsi","rdi","r8","r9","r10","r11","r12","r13","r14","r15"	/* Clobbered registers */\
		);

	} else if(!y) {	// Even modulus, no quotient computation, uses Algo B

		/* Not enough GPRs in odd-q remaindering macro to hold high qword of pairwise-qword shifts
		needed in even-q case, so free up the 4 regs [rdi,r8,r9,rdx] used to store cy0-3 by loading-
		from-mem on first use each loop-exec and storing-to-mem at end of each loop-exec:
		*/
		cy0 = cy1 = cy2 = cy3 = (uint64)0;

		// Inlined input-vector-right-shift leads to major register pressure (cf. mi64_div_by_scalar64_u2,
		// where I did such inlining in a specialized even-modulus version of the remaindering loop), so
		// for our flagship 4-folded DIVREM do input-vector-right-shift separately via a separate preprocessing
		// step. Since SSE2 and AVX both restricted to 128-bit vector integers, still use SH[L,R]D for this step
		// despite the post-Core2 crapification of said shift instructions:
	__asm__ volatile (\
		"movq	%[__x],%%r10		\n\t"\
		"movq	%[__qinv],%%rbx	\n\t"/* No-quotient leaves rbx free, so use for qinv (proves faster than using for q) */\
		"movslq	%[__len4],%%r14		\n\t"\
		"movq	%%r14,%%rsi			\n\t"/* Copy len/4 into loop counter (rsi) */\
		"shlq	$3,%%r14			\n\t"/* len/4 in bytewise 64-bit-pointer-offset form */\
		"leaq (%%r10,%%r14,2),%%r15	\n\t"/* x+len/2 */\
		"movslq	%[__n],%%rcx		\n\t"/* shift count needs to be in CL */\
		/* SHRD allows mem-ref only in DEST, so preload x[i0-3]: */\
		"movq	(%%r10),%%rax		\n\t	movq	(%%r10,%%r14),%%r11	\n\t"/* Load x[i0],x[i1] */\
		"movq	(%%r15),%%r12		\n\t	movq	(%%r15,%%r14),%%r13	\n\t"/* Load x[i2],x[i3] */\
		"subq	$1,%%rsi			\n\t"/* ASM loop structured as for(j = len4-1; j != 0; --j){...} */\
	"jz loop4r_b0 	\n\t"/* check rsi == 0 ? here and if so, don't exec the loop. */
	"loop4r_b:		\n\t"\
		"movq	0x8(%%r10),%%rdi	\n\t	movq	0x8(%%r10,%%r14),%%r8	\n\t"/* load x[i0+1],x[i1+1] */\
		"movq	0x8(%%r15),%%rdx	\n\t	movq	0x8(%%r15,%%r14),%%r9	\n\t"/* load x[i2+1],x[i3+1] */\
		"shrdq	%%cl,%%rdi,%%rax	\n\t	shrdq	%%cl,%%r8 ,%%r11	\n\t"/* y0,1 = (x[i0,1+1],x[i0,1])>>n */\
		"shrdq	%%cl,%%rdx,%%r12	\n\t	shrdq	%%cl,%%r9 ,%%r13	\n\t"/* y2,3 = (x[i2,3+1],x[i2,3])>>n */\
		"addq	$0x8 ,%%r10			\n\t	addq	$0x8 ,%%r15			\n\t"/* Increment input-array pointers */\
		"subq	%[__cy0],%%rax		\n\t	sbbq	%%rdi,%%rdi		\n\t"/* tmp0 = x0 - cy0, save -CF */\
		"subq	%[__cy1],%%r11		\n\t	sbbq	%%r8 ,%%r8 		\n\t"/* tmp1 = x1 - cy1, save -CF */\
		"subq	%[__cy2],%%r12		\n\t	sbbq	%%r9 ,%%r9 		\n\t"/* tmp2 = x2 - cy2, save -CF */\
		"subq	%[__cy3],%%r13		\n\t	sbbq	%%rdx,%%rdx		\n\t"/* tmp3 = x3 - cy3, save -CF */\
		"imulq	%%rbx	,%%rax		\n\t	imulq	%%rbx	,%%r11	\n\t"/* tmp0,1 *= qinv */\
		"imulq	%%rbx	,%%r12		\n\t	imulq	%%rbx	,%%r13	\n\t"/* tmp2,3 *= qinv */\
		"subq	%%rdi,%%rax			\n\t	subq	%%r8 ,%%r11		\n\t"/* tmp0,1 = tmp0,1 * qinv + CF */\
		"subq	%%r9 ,%%r12			\n\t	subq	%%rdx,%%r13		\n\t"/* tmp2,3 = tmp2,3 * qinv + CF */\
		"									mulq	%[__q]	\n\t	movq	%%rdx,%[__cy0]	\n\t"/* cy0 = MULH64(q,tmp0), then move cy0 out of rdx in prep for cy1 computation */\
		"movq	%%r11,%%rax			\n\t	mulq	%[__q]	\n\t	movq	%%rdx,%[__cy1]	\n\t"/* cy1 = MULH64(q,tmp1), then move cy1 out of rdx in prep for cy2 computation */\
		"movq	%%r12,%%rax			\n\t	mulq	%[__q]	\n\t	movq	%%rdx,%[__cy2]	\n\t"/* cy2 = MULH64(q,tmp2), then move cy2 out of rdx in prep for cy3 computation */\
		"movq	%%r13,%%rax			\n\t	mulq	%[__q]	\n\t	movq	%%rdx,%[__cy3]	\n\t"/* cy3 = MULH64(q,tmp3), then move cy3 out of rdx in prep for next loop-execution */\
		/* relood x[0-3]+1, this time into [rax,r11,r12,r13] : */\
		"movq	(%%r10),%%rax		\n\t	movq	(%%r10,%%r14),%%r11	\n\t"/* Load x[i0]+1,x[i1]+! */\
		"movq	(%%r15),%%r12		\n\t	movq	(%%r15,%%r14),%%r13	\n\t"/* Load x[i2]+1,x[i3]+1 */\
	"subq	$1,%%rsi \n\t"\
	"jnz loop4r_b 	\n\t"/* loop1 end; continue is via jump-back if rsi != 0 */\
		/* Last element (i3 = len-1) has no shift-in from next-higher term: */\
	"loop4r_b0:		\n\t"\
		"movq	0x8(%%r10),%%rdi	\n\t	movq	0x8(%%r10,%%r14),%%r8	\n\t"/* load x[i0+1],x[i1+1] */\
		"movq	0x8(%%r15),%%rdx											\n\t"/* load x[i2+1]         */\
		"shrdq	%%cl,%%rdi,%%rax	\n\t	shrdq	%%cl,%%r8 ,%%r11	\n\t"/* y0,1 = (x[i0,1+1],x[i0,1])>>n */\
		"shrdq	%%cl,%%rdx,%%r12	\n\t	shrq	%%cl      ,%%r13	\n\t"/* y2 = (x[i2+1],x[i2])>>n, y3 = x[i3]>>n */\
		"subq	%[__cy0],%%rax		\n\t	sbbq	%%rdi,%%rdi		\n\t"/* tmp0 = x0 - cy0, save -CF */\
		"subq	%[__cy1],%%r11		\n\t	sbbq	%%r8 ,%%r8 		\n\t"/* tmp1 = x1 - cy1, save -CF */\
		"subq	%[__cy2],%%r12		\n\t	sbbq	%%r9 ,%%r9 		\n\t"/* tmp2 = x2 - cy2, save -CF */\
		"subq	%[__cy3],%%r13		\n\t	sbbq	%%rdx,%%rdx		\n\t"/* tmp3 = x3 - cy3, save -CF */\
		"andq	%[__q],%%rdi		\n\t	andq	%[__q],%%r8 	\n\t"/* q & (-cy0|1) */\
		"andq	%[__q],%%r9 		\n\t	andq	%[__q],%%rdx	\n\t"/* q & (-cy2|3) */\
		"addq	%%rdi,%%rax			\n\t	addq	%%r8 ,%%r11		\n\t"/* cy0|1 = tmp0|1 + ((-cy0|1)&q) */\
		"addq	%%r9 ,%%r12			\n\t	addq	%%rdx,%%r13		\n\t"/* cy2|3 = tmp2|3 + ((-cy2|3)&q) */\
		"movq	%%rax,%[__cy0]		\n\t	movq	%%r11,%[__cy1]	\n\t"\
		"movq	%%r12,%[__cy2]		\n\t	movq	%%r13,%[__cy3]	\n\t"\
		:	/* outputs: none */\
		: [__q] "m" (q)	/* All inputs from memory addresses here */\
		 ,[__qinv] "m" (qinv)	\
		 ,[__x] "m" (x)	\
		 ,[__cy0] "m" (cy0)	\
		 ,[__cy1] "m" (cy1)	\
		 ,[__cy2] "m" (cy2)	\
		 ,[__cy3] "m" (cy3)	\
		 ,[__len4] "m" (len4)	\
		 ,[__n] "m" (nshift)	\
		: "cc","memory","rax","rbx","rcx","rdx","rsi","rdi","r8","r9","r10","r11","r12","r13","r14","r15"	/* Clobbered registers */\
		);

	} else {	// Even modulus, with quotient computation, uses Algo B

		/* Not enough GPRs in odd-q remaindering macro to hold high qword of pairwise-qword shifts
		needed in even-q case, so free up the 4 regs [rdi,r8,r9,rdx] used to store cy0-3 by loading-
		from-mem on first use each loop-exec and storing-to-mem at end of each loop-exec:
		*/
		cy0 = cy1 = cy2 = cy3 = (uint64)0;

		// Inlined input-vector-right-shift leads to major register pressure (cf. mi64_div_by_scalar64_u2,
		// where I did such inlining in a specialized even-modulus version of the remaindering loop), so
		// for our flagship 4-folded DIVREM do input-vector-right-shift separately via a separate preprocessing
		// step. Since SSE2 and AVX both restricted to 128-bit vector integers, still use SH[L,R]D for this step
		// despite the post-Core2 crapification of said shift instructions:
	// *** try inlined-shift version first ***
	__asm__ volatile (\
		"movq	%[__x],%%r10		\n\t"\
		"movq	%[__iptr0],%%rbx	\n\t"/* Output base-pointer */\
		"movslq	%[__len4],%%r14		\n\t"\
		"movq	%%r14,%%rsi			\n\t"/* Copy len/4 into loop counter (rsi) */\
		"shlq	$3,%%r14			\n\t"/* len/4 in bytewise 64-bit-pointer-offset form */\
		"leaq (%%r10,%%r14,2),%%r15	\n\t"/* x+len/2 */\
		"movslq	%[__n],%%rcx		\n\t"/* shift count needs to be in CL */\
		/* SHRD allows mem-ref only in DEST, so preload x[i0-3]: */\
		"movq	(%%r10),%%rax		\n\t	movq	(%%r10,%%r14),%%r11	\n\t"/* Load x[i0],x[i1] */\
		"movq	(%%r15),%%r12		\n\t	movq	(%%r15,%%r14),%%r13	\n\t"/* Load x[i2],x[i3] */\
		"subq	$1,%%rsi			\n\t"/* ASM loop structured as for(j = len4-1; j != 0; --j){...} */\
	"jz loop4r_c0 	\n\t"/* check rsi == 0 ? here and if so, don't exec the loop. */
	"loop4r_c:		\n\t"\
		"movq	0x8(%%r10),%%rdi	\n\t	movq	0x8(%%r10,%%r14),%%r8	\n\t"/* load x[i0+1],x[i1+1] */\
		"movq	0x8(%%r15),%%rdx	\n\t	movq	0x8(%%r15,%%r14),%%r9	\n\t"/* load x[i2+1],x[i3+1] */\
		"shrdq	%%cl,%%rdi,%%rax	\n\t	shrdq	%%cl,%%r8 ,%%r11	\n\t"/* y0,1 = (x[i0,1+1],x[i0,1])>>n */\
		"shrdq	%%cl,%%rdx,%%r12	\n\t	shrdq	%%cl,%%r9 ,%%r13	\n\t"/* y2,3 = (x[i2,3+1],x[i2,3])>>n */\
		"addq	$0x8 ,%%r10			\n\t	addq	$0x8 ,%%r15			\n\t"/* Increment input-array pointers */\
		"movq	%%rax,(%%rbx)		\n\t	movq	%%r11,(%%rbx,%%r14)	\n\t"/* Write y0,1 */\
		"addq	%[__ptr_inc2],%%rbx	\n\t"/* Large-Increment output pointer */\
		"movq	%%r12,(%%rbx)		\n\t	movq	%%r13,(%%rbx,%%r14)		\n\t"/* Write y2,3 */\
		"subq	%[__ptr_inc2],%%rbx	\n\t"/* Large-Decrement output pointer */\
		"subq	%[__cy0],%%rax		\n\t	sbbq	%%rdi,%%rdi		\n\t"/* tmp0 = x0 - cy0, save -CF */\
		"subq	%[__cy1],%%r11		\n\t	sbbq	%%r8 ,%%r8 		\n\t"/* tmp1 = x1 - cy1, save -CF */\
		"subq	%[__cy2],%%r12		\n\t	sbbq	%%r9 ,%%r9 		\n\t"/* tmp2 = x2 - cy2, save -CF */\
		"subq	%[__cy3],%%r13		\n\t	sbbq	%%rdx,%%rdx		\n\t"/* tmp3 = x3 - cy3, save -CF */\
		"imulq	%[__qinv],%%rax		\n\t	imulq	%[__qinv],%%r11	\n\t"/* tmp0,1 *= qinv */\
		"imulq	%[__qinv],%%r12		\n\t	imulq	%[__qinv],%%r13	\n\t"/* tmp2,3 *= qinv */\
		"subq	%%rdi,%%rax			\n\t	subq	%%r8 ,%%r11		\n\t"/* tmp0,1 = tmp0,1 * qinv + CF */\
		"subq	%%r9 ,%%r12			\n\t	subq	%%rdx,%%r13		\n\t"/* tmp2,3 = tmp2,3 * qinv + CF */\
		"									mulq	%[__q]	\n\t	movq	%%rdx,%[__cy0]	\n\t"/* cy0 = MULH64(q,tmp0), then move cy0 out of rdx in prep for cy1 computation */\
		"movq	%%r11,%%rax			\n\t	mulq	%[__q]	\n\t	movq	%%rdx,%[__cy1]	\n\t"/* cy1 = MULH64(q,tmp1), then move cy1 out of rdx in prep for cy2 computation */\
		"movq	%%r12,%%rax			\n\t	mulq	%[__q]	\n\t	movq	%%rdx,%[__cy2]	\n\t"/* cy2 = MULH64(q,tmp2), then move cy2 out of rdx in prep for cy3 computation */\
		"movq	%%r13,%%rax			\n\t	mulq	%[__q]	\n\t	movq	%%rdx,%[__cy3]	\n\t"/* cy3 = MULH64(q,tmp3), then move cy3 out of rdx in prep for next loop-execution */\
		"addq	%[__ptr_incr],%%rbx	\n\t"/* Small-Increment output pointer */\
		/* relood x[0-3]+1, this time into [rax,r11,r12,r13] : */\
		"movq	(%%r10),%%rax		\n\t	movq	(%%r10,%%r14),%%r11	\n\t"/* Load x[i0]+1,x[i1]+! */\
		"movq	(%%r15),%%r12		\n\t	movq	(%%r15,%%r14),%%r13	\n\t"/* Load x[i2]+1,x[i3]+1 */\
	"subq	$1,%%rsi \n\t"\
	"jnz loop4r_c 	\n\t"/* loop1 end; continue is via jump-back if rsi != 0 */\
		/* Last element (i3 = len-1) has no shift-in from next-higher term: */\
	"loop4r_c0:		\n\t"\
		"movq	0x8(%%r10),%%rdi	\n\t	movq	0x8(%%r10,%%r14),%%r8	\n\t"/* load x[i0+1],x[i1+1] */\
		"movq	0x8(%%r15),%%rdx											\n\t"/* load x[i2+1]         */\
		"shrdq	%%cl,%%rdi,%%rax	\n\t	shrdq	%%cl,%%r8 ,%%r11	\n\t"/* y0,1 = (x[i0,1+1],x[i0,1])>>n */\
		"shrdq	%%cl,%%rdx,%%r12	\n\t	shrq	%%cl      ,%%r13	\n\t"/* y2 = (x[i2+1],x[i2])>>n, y3 = x[i3]>>n */\
		"movq	%%rax,(%%rbx)		\n\t	movq	%%r11,(%%rbx,%%r14)		\n\t"/* Write y0,1 */\
		"addq	%[__ptr_inc2],%%rbx	\n\t"/* Large-Increment output pointer */\
		"movq	%%r12,(%%rbx)		\n\t	movq	%%r13,(%%rbx,%%r14)		\n\t"/* Write y2,3 */\
		"subq	%[__cy0],%%rax		\n\t	sbbq	%%rdi,%%rdi		\n\t"/* tmp0 = x0 - cy0, save -CF */\
		"subq	%[__cy1],%%r11		\n\t	sbbq	%%r8 ,%%r8 		\n\t"/* tmp1 = x1 - cy1, save -CF */\
		"subq	%[__cy2],%%r12		\n\t	sbbq	%%r9 ,%%r9 		\n\t"/* tmp2 = x2 - cy2, save -CF */\
		"subq	%[__cy3],%%r13		\n\t	sbbq	%%rdx,%%rdx		\n\t"/* tmp3 = x3 - cy3, save -CF */\
		"andq	%[__q],%%rdi		\n\t	andq	%[__q],%%r8 	\n\t"/* q & (-cy0|1) */\
		"andq	%[__q],%%r9 		\n\t	andq	%[__q],%%rdx	\n\t"/* q & (-cy2|3) */\
		"addq	%%rdi,%%rax			\n\t	addq	%%r8 ,%%r11		\n\t"/* cy0|1 = tmp0|1 + ((-cy0|1)&q) */\
		"addq	%%r9 ,%%r12			\n\t	addq	%%rdx,%%r13		\n\t"/* cy2|3 = tmp2|3 + ((-cy2|3)&q) */\
		"movq	%%rax,%[__cy0]		\n\t	movq	%%r11,%[__cy1]	\n\t"\
		"movq	%%r12,%[__cy2]		\n\t	movq	%%r13,%[__cy3]	\n\t"\
		:	/* outputs: none */\
		: [__q] "m" (q)	/* All inputs from memory addresses here */\
		 ,[__qinv] "m" (qinv)	\
		 ,[__x] "m" (x)	\
		 ,[__cy0] "m" (cy0)	\
		 ,[__cy1] "m" (cy1)	\
		 ,[__cy2] "m" (cy2)	\
		 ,[__cy3] "m" (cy3)	\
		 ,[__len4] "m" (len4)	\
		 ,[__n] "m" (nshift)	\
		 ,[__iptr0] "m" (iptr0)	/* Output base-pointer */\
		 ,[__ptr_incr] "m" (ptr_incr)	\
		 ,[__ptr_inc2] "m" (ptr_inc2)	\
		: "cc","memory","rax","rbx","rcx","rdx","rsi","rdi","r8","r9","r10","r11","r12","r13","r14","r15"	/* Clobbered registers */\
		);

	}	// q odd or even ?

  #endif	// AVX2/MULX or not?

#endif

#if MI64_DIV_MONT64_U4
	if(dbg)printf("Half-length carryouts: cy0-3 = %20llu, %20llu, %20llu, %20llu\n",cy0,cy1,cy2,cy3);
#endif

#ifdef USE_AVX2
	// AVX2 with its input-vec-preshift-if-even-modulus uses only Algo A, irresp. of parity of modulus:
	if(cy0) cy0 = q-cy0;
	if(cy1) cy1 = q-cy1;
	if(cy2) cy2 = q-cy2;
	if(cy3) cy3 = q-cy3;
#else
	if(!nshift) {	// Odd modulus uses Algo A
		if(cy0) cy0 = q-cy0;
		if(cy1) cy1 = q-cy1;
		if(cy2) cy2 = q-cy2;
		if(cy3) cy3 = q-cy3;
	} else {		// Even modulus uses Algo B
		MONT_UNITY_MUL64(cy0,q,qinv,cy0);
		MONT_UNITY_MUL64(cy1,q,qinv,cy1);
		MONT_UNITY_MUL64(cy2,q,qinv,cy2);
		MONT_UNITY_MUL64(cy3,q,qinv,cy3);
	}
#endif
	// Compute radix-power; add-1 here since use scaled-remainder Algorithm A:
	rpow = radix_power64(q,qinv,len4+1);
	// Build up the sum of the scaled partial remainders, two terms at a time:
	MONT_MUL64(cy3,rpow,q,qinv,cy3);	cy2 += cy3;	if(cy2 < cy3 || cy2 >= q) cy2 -= q;	//               cy2 + cy3*B^p           (mod q)
	MONT_MUL64(cy2,rpow,q,qinv,cy2);	cy1 += cy2;	if(cy1 < cy2 || cy1 >= q) cy1 -= q;	//        cy1 + (cy2 + cy3*B^p)*B^p      (mod q)
	MONT_MUL64(cy1,rpow,q,qinv,cy1);	cy0 += cy1;	if(cy0 < cy1 || cy0 >= q) cy0 -= q;	// cy0 + (cy1 + (cy2 + cy3*B^p)*B^p)*B^p (mod q)
	// One more modmul of sum by same power of the base gives full remainder in cy0, where we need it:
	MONT_MUL64(cy0,rpow,q,qinv,cy0);

#if MI64_DIV_MONT64_U4
	if(dbg) printf("True mod %c = %20llu\n",'A'+(nshift != 0),cy0);
#endif

	// If we applied an initial right-justify shift to the modulus, restore the shift to the
	// current (partial) remainder and re-add the off-shifted part of the true remainder.
	rpow = (cy0 << nshift) + rem_save;
	if(!y)	// Only remainder needed
		return rpow;

	/*** Here is loop-split quotient computation: ***/
	if(!nshift) {	// If odd modulus, have not yet copied input array to y...
		iptr0 = (uint64*)x;
	} else if (y) {	// If even modulus, right-justified copy of input array will be put into quotient vec, if avaialble:
		iptr0 = y;
	}

#ifndef YES_ASM

	bw0 = bw1 = bw2 = bw3 = (uint64)0;
	iptr1 = iptr0 + len4; iptr2 = iptr1 + len4; iptr3 = iptr2 + len4;
	// If odd modulus, have not yet copied input array to y...
	for(i = 0; i < len4; ++i, ++iptr0, ++iptr1, ++iptr2, ++iptr3) {
		tmp0 = *iptr0 - bw0 - cy0;	tmp1 = *iptr1 - bw1 - cy1;	tmp2 = *iptr2 - bw2 - cy2;	tmp3 = *iptr3 - bw3 - cy3;
		/*  Since may be working in-place, need an extra temp here due to asymmetry of subtract: */
		bw0 = (tmp0 > *iptr0);		bw1 = (tmp1 > *iptr1);		bw2 = (tmp2 > *iptr2);		bw3 = (tmp3 > *iptr3);
		tmp0 = tmp0*qinv;			tmp1 = tmp1*qinv;			tmp2 = tmp2*qinv;			tmp3 = tmp3*qinv;
	#ifdef MUL_LOHI64_SUBROUTINE
		cy0 = __MULH64(q,tmp0);		cy1 = __MULH64(q,tmp1);		cy2 = __MULH64(q,tmp2);		cy3 = __MULH64(q,tmp3);
	#else
		MULH64(q,tmp0, cy0);		MULH64(q,tmp1, cy1);		MULH64(q,tmp2, cy2);		MULH64(q,tmp3, cy3);
	#endif
	#if MI64_DIV_MONT64_U4
		if(dbg)printf("quot[%2u,%2u,%2u,%2u] = %20llu,%20llu,%20llu,%20llu, bw0-3 = %1u,%1u,%1u,%1u, cy0-3 = %20llu,%20llu,%20llu,%20llu\n",i0,i1,i2,i3,tmp0,tmp1,tmp2,tmp3,(uint32)bw0,(uint32)bw1,(uint32)bw2,(uint32)bw3,cy0,cy1,cy2,cy3);
	#endif
		// Write quotient words:
		y[i] = tmp0;				y[i+len4] = tmp1;			y[i+len2] = tmp2;			y[i+len2+len4] = tmp3;
	}

#else

	iptr2 = iptr0 + len2;

	/*** Bizarrely, the MULX-based version of the quotient loop runs slower than the original on Haswell ***/
  #ifdef USE_AVX2	// Haswell-and-beyond version (Have a MULX instruction)
					// Note: MULX reg1,reg2,reg3 [AT&T/GCC syntax] assumes src1 in RDX
					// [MULQ assumed RAX], src2 in named reg1, lo"hi output halves into reg2:reg3.
					// If only MULH result desired, set output-regs same: reg2 = reg3.

	__asm__ volatile (\
		"movq	%[__x],%%r10	\n\t"\
		"xorq	%%rdi,%%rdi		\n\t	subq	%[__cy0],%%rdi	\n\t"/* Init bw0 = 0, -(bw0 + cy0) */\
		"xorq	%%r8 ,%%r8 		\n\t	subq	%[__cy1],%%r8 	\n\t"/* Init bw1 = 0, -(bw1 + cy1) */\
		"xorq	%%r9 ,%%r9 		\n\t	subq	%[__cy2],%%r9 	\n\t"/* Init bw2 = 0, -(bw2 + cy2) */\
		"xorq	%%rbx,%%rbx		\n\t	movq	%[__cy3],%%rax	\n\t"/* Init bw3 = 0, +(bw3 + cy3) */\
		"movq	%[__xy_ptr_diff],%%rsi	\n\t"/* Load (y-x) pointer-diff */\
		"movslq	%[__len],%%r14	\n\t"/* ASM loop structured as for(j = len; j != 0; --j){...} */\
		"shrq	$1      ,%%r14	\n\t"/* len/2 */\
		"leaq	(%%r10,%%r14,8),%%r15	\n\t"/* x+len/2 */\
		"shrq	$1      ,%%r14	\n\t"/* len/4 */\
		"movq	%%r14,%%rcx	\n\t"/* Copy len/4 into loop counter*/\
		"shlq	$3,%%r14	\n\t"/* r14 holds pointer offset corr. to len/4 array elements */\
	"loop4c:		\n\t"
		"leaq	(%%r10,%%r14),%%r11	\n\t	movq	(%%r10),%%rdx	\n\t	movq	(%%r11),%%r11	\n\t"/* Load &x1,*x0,*x1 */\
		"leaq	(%%r15,%%r14),%%r13	\n\t	movq	(%%r15),%%r12	\n\t	movq	(%%r13),%%r13	\n\t"/* Load &x3,*x2,*x3 */
		"negq	%%rdi			\n\t	negq	%%r8			\n\t"/* rdi = +(bw0 + cy0), r8  = +(bw1 + cy1) */\
		"negq	%%r9			\n\t	subq	%%rbx,%%rax		\n\t"/* r9  = +(bw2 + cy2), rbx = +(bw3 + cy3) via cy3 -(-bw3), no carryout possible */
		"subq	%%rdi,%%rdx		\n\t	sbbq	%%rdi,%%rdi		\n\t"/* tmp0 = x0 - (bw0 + cy0), save -CF */\
		"subq	%%r8 ,%%r11		\n\t	sbbq	%%r8 ,%%r8 		\n\t"/* tmp1 = x1 - (bw1 + cy1), save -CF */\
		"subq	%%r9 ,%%r12		\n\t	sbbq	%%r9 ,%%r9 		\n\t"/* tmp2 = x2 - (bw2 + cy2), save -CF */\
		"subq	%%rax,%%r13		\n\t	sbbq	%%rbx,%%rbx		\n\t"/* tmp3 = x3 - (bw3 + cy3), save -CF */\
		"movq	%[__qinv],%%rax	\n\t"/* I get no speedup from moving qinv into a reg in practice, but my intentions wre good. :) */\
		"imulq	%%rax,%%rdx		\n\t	imulq	%%rax,%%r11		\n\t"/* tmp0,1 *= qinv */\
		"imulq	%%rax,%%r12		\n\t	imulq	%%rax,%%r13		\n\t"/* tmp2,3 *= qinv */\
		"addq	%%rsi,%%r10		\n\t	addq	%%rsi,%%r15		\n\t"/* Add (y-x) pointer-diff to x0,2 ptrs to get y0,2 */\
		"movq	%%rdx,(%%r10)	\n\t	movq	%%r12,(%%r15)	\n\t"/* store tmp0,2 */\
		"addq	%%r14,%%r10		\n\t	addq	%%r14,%%r15		\n\t"/* Add len/4 pointer-diff to y0,2 ptrs to get y1,3 */\
		"movq	%%r11,(%%r10)	\n\t	movq	%%r13,(%%r15)	\n\t"/* store tmp1,3 */\
		"subq	%%r14,%%r10		\n\t	subq	%%r14,%%r15		\n\t"/* Sub len/4 pointer-diff from y1,3 ptrs to get y0,2 */\
		"subq	%%rsi,%%r10		\n\t	subq	%%rsi,%%r15		\n\t"/* Sub (y-x) pointer-diff from y0,2 ptrs to get x0,2 */\
		/* Set up for the MULX calls by moving q into implied-reg rdx */\
		"movq	%%rdx,%%rax		\n\t	movq	%[__q],%%rdx	\n\t"\
		/* For MULX, if lo:hi output registers are the same, it holds the high half (only part we care about here) on output: */\
		"mulxq	%%rax,%%rax,%%rax	\n\t	subq	%%rax,%%rdi	\n\t"/* cy0 = MULH64(q,tmp0), move cy0 from rax -> -bw0-cy0 */\
		"mulxq	%%r11,%%rax,%%rax	\n\t	subq	%%rax,%%r8 	\n\t"/* cy1 = MULH64(q,tmp1), move cy1 from rax -> -bw1-cy1 */\
		"mulxq	%%r12,%%rax,%%rax	\n\t	subq	%%rax,%%r9 	\n\t"/* cy2 = MULH64(q,tmp2), move cy2 from rax -> -bw2-cy2 */\
		"mulxq	%%r13,%%rax,%%rax	\n\t"/* load x3 into rdx; cy3 = MULH64(q,tmp3), leave hi-half-product result in rax */\
		"addq	$0x8 ,%%r10		\n\t	addq	$0x8 ,%%r15		\n\t"/* Increment x0,2-pointers */\
	"subq	$1,%%rcx \n\t"\
	"jnz loop4c 	\n\t"/* loop1 end; continue is via jump-back if rcx != 0 */\
		"movq	%%rax,%[__cy3]	\n\t"\
	:	/* outputs: none */\
	: [__q] "m" (q)	/* All inputs from memory addresses here */\
	 ,[__qinv] "m" (qinv)	\
	 ,[__x] "m" (iptr0)	\
	 ,[__xy_ptr_diff] "m" (xy_ptr_diff)	\
	 ,[__cy0] "m" (cy0)	\
	 ,[__cy1] "m" (cy1)	\
	 ,[__cy2] "m" (cy2)	\
	 ,[__cy3] "m" (cy3)	\
	 ,[__len] "m" (len)	\
	: "cc","memory","rax","rbx","rcx","rdx","rsi","rdi","r8","r9","r10","r11","r12","r13","r14","r15"	/* Clobbered registers */\
	);

  #else		// Pre-Haswell version (no MULX instruction)
			// Note: MULQ assumes src1 in RAX, src2 in named register, lo"hi output halves into rax:rdx:

	__asm__ volatile (\
		"movq	%[__iptr0],%%r10	\n\t"/* Input pointers (point to [x|y]+{0,len2} if q [odd|even] */\
		"movq	%[__iptr2],%%r15	\n\t"\
		"xorq	%%rdi,%%rdi		\n\t	subq	%[__cy0],%%rdi	\n\t"/* Init bw0 = 0, -(bw0 + cy0) */\
		"xorq	%%r8 ,%%r8 		\n\t	subq	%[__cy1],%%r8 	\n\t"/* Init bw1 = 0, -(bw1 + cy1) */\
		"xorq	%%r9 ,%%r9 		\n\t	subq	%[__cy2],%%r9 	\n\t"/* Init bw2 = 0, -(bw2 + cy2) */\
		"xorq	%%rbx,%%rbx		\n\t	movq	%[__cy3],%%rdx	\n\t"/* Init bw3 = 0, +(bw3 + cy3) */\
	/* Quotient loop needs a register for output-array ptr; timings of rem-loop show replacing %%rsi with %[__q] in MULQs is timing-neutral: */\
	/*	"movq	%[__q],%%rsi	\n\t"\	*/\
	/*	"movq	%[__qinv],%%rbx	\n\t"\	... similar timing-neutrality holds for [__q] */\
		"movq	%[__xy_ptr_diff],%%rsi	\n\t"/* Load (y-x) pointer-diff */\
		"movslq	%[__len4],%%r14	\n\t"/* ASM loop structured as for(j = len4; j != 0; --j){...} */\
		"movq	%%r14,%%rcx	\n\t"/* Copy len/4 into loop counter*/\
		"shlq	$3,%%r14	\n\t"/* r14 holds pointer offset corr. to len/4 array elements */\
	"loop4c:		\n\t"
		"movq	(%%r10),%%rax	\n\t	movq	(%%r10,%%r14),%%r11	\n\t"/* Load *iptr0,*iptr1 */\
		"movq	(%%r15),%%r12	\n\t	movq	(%%r15,%%r14),%%r13	\n\t"/* Load *iptr2,*iptr3 */
		"negq	%%rdi			\n\t	negq	%%r8			\n\t"/* rdi = +(bw0 + cy0), r8  = +(bw1 + cy1) */\
		"negq	%%r9			\n\t	subq	%%rbx,%%rdx		\n\t"/* r9  = +(bw2 + cy2), rbx = +(bw3 + cy3) via cy3 -(-bw3), no carryout possible */
		"subq	%%rdi,%%rax		\n\t	sbbq	%%rdi,%%rdi		\n\t"/* tmp0 = x0 - (bw0 + cy0), save -CF */\
		"subq	%%r8 ,%%r11		\n\t	sbbq	%%r8 ,%%r8 		\n\t"/* tmp1 = x1 - (bw1 + cy1), save -CF */\
		"subq	%%r9 ,%%r12		\n\t	sbbq	%%r9 ,%%r9 		\n\t"/* tmp2 = x2 - (bw2 + cy2), save -CF */\
		"subq	%%rdx,%%r13		\n\t	sbbq	%%rbx,%%rbx		\n\t"/* tmp3 = x3 - (bw3 + cy3), save -CF */\
		"imulq	%[__qinv],%%rax \n\t	imulq	%[__qinv],%%r11 \n\t"/* tmp0,1 *= qinv */\
		"imulq	%[__qinv],%%r12 \n\t	imulq	%[__qinv],%%r13 \n\t"/* tmp2,3 *= qinv */\
		"addq	%%rsi,%%r10		\n\t	addq	%%rsi,%%r15		\n\t"/* Add (y-x) pointer-diff to x0,2 ptrs to get y0,2 */\
		"movq	%%rax,(%%r10)	\n\t	movq	%%r12,(%%r15)	\n\t"/* store tmp0,2 */\
		"addq	%%r14,%%r10		\n\t	addq	%%r14,%%r15		\n\t"/* Add len/4 pointer-diff to y0,2 ptrs to get y1,3 */\
		"movq	%%r11,(%%r10)	\n\t	movq	%%r13,(%%r15)	\n\t"/* store tmp1,3 */\
		"subq	%%r14,%%r10		\n\t	subq	%%r14,%%r15		\n\t"/* Sub len/4 pointer-diff from y1,3 ptrs to get y0,2 */\
		"subq	%%rsi,%%r10		\n\t	subq	%%rsi,%%r15		\n\t"/* Sub (y-x) pointer-diff from y0,2 ptrs to get x0,2 */\
		"								mulq	%[__q]	\n\t	subq	%%rdx,%%rdi	\n\t"/* cy0 = MULH64(q,tmp0), move cy0 from rdx -> -bw0-cy0 */\
		"movq	%%r11,%%rax		\n\t	mulq	%[__q]	\n\t	subq	%%rdx,%%r8 	\n\t"/* cy1 = MULH64(q,tmp1), move cy1 from rdx -> -bw1-cy1 */\
		"movq	%%r12,%%rax		\n\t	mulq	%[__q]	\n\t	subq	%%rdx,%%r9 	\n\t"/* cy2 = MULH64(q,tmp2), move cy2 from rdx -> -bw2-cy2 */\
		"movq	%%r13,%%rax		\n\t	mulq	%[__q]	\n\t"/* load x3 into rax; cy3 = MULH64(q,tmp3), leave result in rdx */\
		"addq	$0x8 ,%%r10		\n\t	addq	$0x8 ,%%r15		\n\t"/* Increment x0,2-pointers */\
	"subq	$1,%%rcx \n\t"\
	"jnz loop4c 	\n\t"/* loop1 end; continue is via jump-back if rcx != 0 */\
		"movq	%%rdx,%[__cy3]	\n\t"\
	:	/* outputs: none */\
	: [__q] "m" (q)	/* All inputs from memory addresses here */\
	 ,[__qinv] "m" (qinv)	\
	 ,[__iptr0] "m" (iptr0)	/* Input pointers (point to x,x+len2 if q odd, y,y+len2 if q even) */\
	 ,[__iptr2] "m" (iptr2)	\
	 ,[__xy_ptr_diff] "m" (xy_ptr_diff)	\
	 ,[__cy0] "m" (cy0)	\
	 ,[__cy1] "m" (cy1)	\
	 ,[__cy2] "m" (cy2)	\
	 ,[__cy3] "m" (cy3)	\
	 ,[__len4] "m" (len4)	\
	: "cc","memory","rax","rbx","rcx","rdx","rsi","rdi","r8","r9","r10","r11","r12","r13","r14","r15"	/* Clobbered registers */\
	);

  #endif	// AVX2/MULX or not?

#endif
	ASSERT(HERE, cy3 == 0, "cy check!");	// all but the uppermost carryout are generally nonzero
	// Restore input values of 0-pad elements prior to return:
	for(i = 0; i < npad; i++) {
		x[lenu+i] = pads[i];
	}
	return rpow;
}

#endif	// __CUDA_ARCH__ ?

// Divide-with-Remainder of x by y, where x is a multiword base 2^64 integer and y a 32-bit unsigned scalar.
// Returns 32-bit remainder in the function result and (optionally, if q-array pointer != 0) quotient x/y in q[].
// Allows division-in-place, i.e. x == q.
/*** NOTE *** Routine assumes x[] is a uint64 array cast to uint32[], hence the doubling-of-len
is done HERE, i.e. user must supply uint64-len just as for the 'true 64-bit' mi64 functions!
In other words, these kinds of compiler warnings are expected:

	mi64.c: In function ‘mi64_div_y32’:
	warning: passing argument 1 of ‘mi64_is_div_by_scalar32’ from incompatible pointer type
	note: expected ‘const uint32 *’ but argument is of type ‘uint64 *’
*/
#ifdef __CUDA_ARCH__
__device__
#endif
uint32 mi64_div_y32(uint64 x[], uint32 y, uint64 q[], uint32 len)
{
	int i;
	uint64 cy, rem, xlomody, tsum;
	// Apr 2015: Removed 'static' flag on these to allow || execution (and any speed gains due to
	// precomputing/saving them are likely trivial, since the most common use mode is fixed dividend x[],
	// multiple divisors y, e.g. trial-dividing a multiword int by small primes up to some threshold):
//	uint32 ysave = 0;
	uint64 two64divy, two64mody;

	two64divy = 0x8000000000000000ull/y;
	two64divy = (two64divy + two64divy);
	two64mody = 0x8000000000000000ull%y;
	/* To save a second (expensive) standard library mod call,
	double and subtract y, then re-add y if the result underflows: */
	two64mody = (two64mody + two64mody) - y;
	cy = (two64mody >> 63);
	two64mody += (-cy) & y;
	two64divy += (cy == 0);

	rem = 0;
	for(i = len-1; i >= 0; --i) {
		/* Current-term remainder (must calculate this before modifying (q[i], in case q and x point to same array) */
		xlomody = (x[i])%y;
		tsum = rem*two64mody + xlomody;

		/* Low digit of result: we must separately divide (x[i]) by y
		(making sure to add (x[i])%y to  cy*two64mody first, so as not to drop a digit)
		because x[i] may be as large as 2^64-1, and adding rem*two64mody
		prior to dividing risks unsigned integer overflow:
		*/
		if(q) {
			q[i] = rem*two64divy + tsum/y + (x[i])/y;
		}
		rem = tsum%y;
	}
	if(rem == 0 && x != q) {	// If overwrote input with quotient in above loop, skip this
		ASSERT(HERE, mi64_is_div_by_scalar32(x, y, len), "Results of mi64_div_y32 and mi64_is_div_by_scalar32 differ!");
		return 0;
	}
	return (uint32)rem;
}

/*
Returns decimal character representation of a base-2^64 multiword unsigned int in char_buf,
and the position of the leftmost nonzero digit (e.g. if the caller wants to print in
left-justified form) in the function result.

If wrap_every != 0, the routine inserts a linefeed every [wrap_every] digits.
*/
#ifndef __CUDA_ARCH__
int	convert_mi64_base10_char(char char_buf[], const uint64 x[], uint32 len, uint32 wrap_every)
{
	uint32 n_alloc_chars = STR_MAX_LEN;
	return __convert_mi64_base10_char(char_buf, n_alloc_chars, x, len, wrap_every);
}

/* This is an arbitrary-string-length core routine which can be called directly, requires caller to supply allocated-length of input string: */
int	__convert_mi64_base10_char(char char_buf[], uint32 n_alloc_chars, const uint64 x[], uint32 len, uint32 wrap_every)
{
	uint32 MAX_DIGITS;
	uint32 i, curr_len, n_dec_digits = 0;
	char c, *cptr;
	const double ln10 = log(10.0), log10_base = 19.26591972249479649367928926;	/* log10(2^64) */
	double dtmp = 0.0;
	static uint64 *temp = 0x0;
	static uint32 tlen = 0;	// #64-bit slots in current memalloc for *temp
	ASSERT(HERE, fabs(1.0 - TWO64FLOAT*TWO64FLINV) < 1e-14, "ERROR: TWO64FLOAT not inited!");	// Make sure these scaling powers have been inited

	/* Estimate # of decimal digits: */
	curr_len = mi64_getlen(x, len);	/* this checks that len > 0; need at least one digit, even if it = 0. curr_len guaranteed > 0. */
	curr_len = MAX(curr_len, 1);
	if(curr_len > tlen) {
		if(temp) {
			free((void *)temp);	temp = 0x0;
		}
		temp = (uint64 *)calloc(curr_len, sizeof(uint64));
		tlen = curr_len;
	}
	mi64_set_eq(temp, x, curr_len);
	// Lump effects of any neglected lower digits into next-lower digit/2^64
	if(curr_len > 1) dtmp = x[curr_len-2]*TWO64FLINV;
	MAX_DIGITS = ceil( (curr_len-1)*log10_base + log((double)x[curr_len-1] + dtmp)/ln10 );
	MAX_DIGITS = MAX(MAX_DIGITS, 1);
	ASSERT(HERE, MAX_DIGITS < n_alloc_chars, "Output string overflows buffer");
	if(wrap_every) {
		MAX_DIGITS += MAX_DIGITS/wrap_every;
	}
	char_buf[MAX_DIGITS-1]= '0';	// Init least-significant digit = 0, in case input = 0
	if(wrap_every) {
		char_buf[MAX_DIGITS  ]='\n';
	} else {
		char_buf[MAX_DIGITS  ]='\0';
	}

	/* Write the decimal digits into the string from right to left.
	This avoids the need to reverse the digits after calculating them.
	*/
	cptr = char_buf + MAX_DIGITS - 1;
	for(i = 0; i < MAX_DIGITS; i++) {
		/* Only print leading zero if q = 0, in which case we print a right-justifed single zero: */
		/* Since the x***_div_y32 routines return the mod *and* the divided input,
		   don't call the function until *after* performing the if() test:
		*/
		if(!mi64_iszero(temp, curr_len) || n_dec_digits == 0) {
			c = mi64_div_y32(temp, (uint32)10, temp, curr_len) + CHAROFFSET;
			curr_len = mi64_getlen(temp, curr_len);
			n_dec_digits++;
		} else {
			c = ' ';
		}
		*cptr = c; cptr--;
		if(wrap_every && ((i+1)%wrap_every == 0)) {
			*cptr-- = '\n';
		}
	}
	return (int)MAX_DIGITS-n_dec_digits;
}

// Leading-zero-printing analog of the above 2 functions:
int	convert_mi64_base10_char_print_lead0(char char_buf[], const uint64 x[], uint32 len, uint32 ndigit, uint32 wrap_every)
{
	uint32 n_alloc_chars = STR_MAX_LEN;
	return __convert_mi64_base10_char_print_lead0(char_buf, n_alloc_chars, x, len, ndigit, wrap_every);
}

int	__convert_mi64_base10_char_print_lead0(char char_buf[], uint32 n_alloc_chars, const uint64 x[], uint32 len, uint32 ndigit, uint32 wrap_every)
{
	uint32 MAX_DIGITS;
	uint32 i, curr_len;
	char c, *cptr;
	const double ln10 = log(10.0), log10_base = 19.26591972249479649367928926;	/* log10(2^64) */
	double dtmp = 0.0;
	static uint64 *temp = 0x0;
	static uint32 tlen = 0;	// #64-bit slots in current memalloc for *temp
	ASSERT(HERE, fabs(1.0 - TWO64FLOAT*TWO64FLINV) < 1e-14, "ERROR: TWO64FLOAT not inited!");	// Make sure these scaling powers have been inited

	/* Estimate # of decimal digits: */
	curr_len = mi64_getlen(x, len);	/* this checks that len > 0; need at least one digit, even if it = 0. curr_len guaranteed > 0. */
	curr_len = MAX(curr_len, 1);
	if(curr_len > tlen) {
		if(temp) {
			free((void *)temp);	temp = 0x0;
		}
		temp = (uint64 *)calloc(curr_len, sizeof(uint64));
		tlen = curr_len;
	}
	mi64_set_eq(temp, x, curr_len);
	// Lump effects of any neglected lower digits into next-lower digit/2^64
	if(curr_len > 1) dtmp = x[curr_len-2]*TWO64FLINV;
	MAX_DIGITS = ceil( (curr_len-1)*log10_base + log((double)x[curr_len-1] + dtmp)/ln10 );
	if(MAX_DIGITS > ndigit) {
		ASSERT(HERE, 0, "ERROR: MAX_DIGITS > ndigit!");
	} else {
		MAX_DIGITS = ndigit;
	}
	ASSERT(HERE, MAX_DIGITS < n_alloc_chars, "Output string overflows buffer");
	if(wrap_every) {
		MAX_DIGITS += MAX_DIGITS/wrap_every;
	}
	char_buf[MAX_DIGITS-1]= '0';	// Init least-significant digit = 0, in case input = 0
	char_buf[MAX_DIGITS  ]='\n';

	/* Write the decimal digits into the string from right to left.
	This avoids the need to reverse the digits after calculating them.
	*/
	cptr = char_buf + MAX_DIGITS - 1;
	for(i = 0; i < MAX_DIGITS; i++) {
		c = mi64_div_y32(temp, (uint32)10, temp, curr_len) + CHAROFFSET;
		curr_len = mi64_getlen(temp, curr_len);
		*cptr = c; cptr--;
		if(wrap_every && ((i+1)%wrap_every == 0)) {
			*cptr-- = '\n';
		}
	}
	return 0;
}
#endif	// __CUDA_ARCH__ ?

/****************/

/* This is essentially an mi64 version of the <stdlib.h> strtoul function: Takes an input string,
converts it - if numeric - to mi64 form, returning a pointer to the mi64 array allocated to
store the result in the function value, and the nominal length of the result in terms of 64-bit
words via argument #2.

If the input string is not a valid numeric or some other conversion problem is encountered, both
the return-value pointer and the word length argument (via the arglist len pointer) are set = 0.

If zero-padding of the resulting mi64 array is desired, user should prefix an appropriate number
of leading 0s to the input charstring.
*/
#ifndef __CUDA_ARCH__
uint64 *convert_base10_char_mi64(const char*char_buf, uint32 *len)
{
	uint64 *mi64_vec;
	uint32 LEN_MAX;
	uint64 tmp = 0;
	uint32 i, imin, imax;
	char c;
	uint64 curr_digit;
	const double log10_base = 19.26591972249479649367928926;	/* log10(2^64) */

	/* Read the decimal digits from the string from left to right,
	skipping any leading whitespace, and stopping if either non-leading
	whitespace or '\0' is encountered:
	*/
	imax = strlen(char_buf);
	for(i = 0; i < imax; i++) {
		c = char_buf[i];
		if(!isspace(c)) {
			break;
		}
	}
	/* Estimate # of 64-bit words based on length of non-whitespace portion of the string: */
	*len = 1;
	LEN_MAX = (uint32)ceil( (imax-i)/log10_base );
	mi64_vec = (uint64 *)calloc(LEN_MAX+1, sizeof(uint64));	/* 01/09/2009: Add an extra zero-pad element here as workaround for bug in mi64_div called with differing-length operands */
	imin = i;
	for(i = imin; i < imax; i++) {
		c = char_buf[i];
		if(!isdigit(c)) {
			free((void *)mi64_vec);	*len = 0;	return 0x0;
		}
		curr_digit = (uint64)(c - CHAROFFSET);
		ASSERT(HERE, curr_digit < 10,"util.c: curr_digit < 10");
		/* currsum *= 10, and check for overflow: */
		tmp = mi64_mul_scalar(mi64_vec, (uint64)10, mi64_vec, *len);
		if(tmp != 0) {
			if(*len == LEN_MAX) {
				printf("ERROR: Mul-by-10 overflows in convert_base10_char_mi64: Offending input string = %s\n", char_buf);
				ASSERT(HERE, 0,"0");
			}
			mi64_vec[(*len)++] = tmp;
		}

		*len += mi64_add_scalar(mi64_vec, curr_digit, mi64_vec, *len);
		ASSERT(HERE, *len <= LEN_MAX,"len <= LEN_MAX");
	}
	*len = LEN_MAX;	/* Nominal length, so user knows how much memory was allocated */
	return mi64_vec;
}
#endif	// __CUDA_ARCH__ ?

/****************/

#ifndef __CUDA_ARCH__	// None of stuff below to be built for GPU TF code

/*
Function to find 2^(-p) mod q, where p and q are both base-2^64 multiword unsigned integers.
Uses a Montgomery-style modmul with a power-of-2 modulus = 2^len. Result (optionally) returned in res[],
assumed to have allocated length at least as large as q[]:

The key 3-operation sequence here is as follows:

	SQR_LOHI(x,lo,hi);	// Input x has len words, lo/hi have len words each
	MULL(lo,qinv,lo);	// Inputs lo & qinv, and output (overwrites lo) have len words
	MULH(q,lo,lo);	// Inputs q & lo, and output (overwrites lo) have len words.

Returns 1 if 2^(-p) == 1 (mod q) (which also means 2^p == 1), 0 otherwise.

Sep 2015: Add code specific for Fermat-number factors: If p a power of 2 (i.e. FERMAT = true
in the on-entry inits), uses the factor k (q = 2.k.p+1) for fast UMULH(q,x) in the powering loop,
and returns 1 if 2^(-p) == -1 (mod q) (which also means 2^p == -1), 0 otherwise.
*/
#if MI64_DEBUG
	#define MI64_POW_DBG	0	// Set nonzero to enable debug-print in the mi64 modpow functions below
#endif
uint32 mi64_twopmodq(const uint64 p[], uint32 len_p, const uint64 k, uint64 q[], uint32 len, uint64*res)
{
	ASSERT(HERE, p != 0x0, "Null p-array pointer!");
	ASSERT(HERE, q != 0x0, "Null q-array pointer!");
	uint32 pow2, FERMAT = mi64_isPow2(p,len,&pow2)<<1;	// *2 is b/c need to add 2 to the usual Mers-mod residue in the Fermat case
  #if MI64_POW_DBG
	uint32 dbg = FERMAT && pow2 == 256;//STREQ(&s0[convert_mi64_base10_char(s0, q, len, 0)], "531137992816767098689588206552468627329593117727031923199444138200403559860852242739162502265229285668889329486246501015346579337652707239409519978766587351943831270835393219031728127");
  #endif
	// Local-alloc-related statics - these should only ever be updated in single-thread mode:
	static int first_entry = TRUE;
	static uint32 lenp_save = 10, lenq_save = 10;	// Initial-alloc values
	static uint64 *pshift = 0x0, *qhalf = 0x0, *qinv = 0x0, *x = 0x0, *lo = 0x0, *hi = 0x0;
	 int32 j;	// Current-Bit index j needs to be signed because of the LR binary exponentiation.
	uint32 retval, idum, pbits;
	uint64 lead_chunk, lo64, cyout;
	uint32 lenP, lenQ, qbits, log2_numbits, start_index, zshift;
  #if MI64_POW_DBG
	if(dbg) printf("mi64_twopmodq: F%u with k = %llu\n",pow2,k);
  #endif
	if(first_entry) {
		first_entry = FALSE;
		pshift = (uint64 *)calloc((lenp_save+1), sizeof(uint64));
		qhalf  = (uint64 *)calloc((lenq_save  ), sizeof(uint64));
		qinv   = (uint64 *)calloc((lenq_save  ), sizeof(uint64));
		x      = (uint64 *)calloc((lenq_save  ), sizeof(uint64));
		lo     = (uint64 *)calloc((2*lenq_save), sizeof(uint64));
	}
	lenP = mi64_getlen(p, len_p);	ASSERT(HERE, lenP > 0, "0 exponent");
	lenQ = mi64_getlen(q, len);		ASSERT(HERE, lenQ > 0, "0 modulus!");
	if(len_p > lenp_save) {
		lenp_save = len_p;
		pshift = (uint64 *)realloc(pshift, (len_p+1)*sizeof(uint64));
	}
	if(len > lenq_save) {
		lenq_save = len;
		pshift = (uint64 *)realloc(pshift, (len_p+1)*sizeof(uint64));
		qhalf  = (uint64 *)realloc(qhalf , (lenQ   )*sizeof(uint64));
		qinv   = (uint64 *)realloc(qinv  , (lenQ   )*sizeof(uint64));
		x      = (uint64 *)realloc(x     , (lenQ   )*sizeof(uint64));
		lo     = (uint64 *)realloc(lo    , (2*lenQ )*sizeof(uint64));
	}
	hi = lo + lenQ;	// Pointer to high half of double-wide product

  #if MI64_POW_DBG
	if(dbg) printf("mi64_twopmodq: k = %llu, len = %u, lenQ = %u\n",k,len,lenQ);
  #endif
	qbits = lenQ << 6;
	mi64_shrl(q, qhalf, 1, lenQ);	/* (q >> 1) = (q-1)/2, since q odd. */

	/* pshift = p + len*64 */
	pshift[lenP] = mi64_add_scalar(p, lenQ*64, pshift, lenP);	// April 2015: lenP ==> lenQ here!
	ASSERT(HERE, !pshift[lenP], "pshift overflows!");

  #if MI64_POW_DBG
	if(dbg) printf("Init: k = %llu, lenP = %u, lenQ = %u\n",k,lenP,lenQ);
  #endif
	log2_numbits = ceil(log(1.0*qbits)/log(2.0));
	/*
	Find position of the leftmost ones bit in pshift, and subtract log2_numbits-1 or log2_numbits
	to account for the fact that we can do the powering for the leftmost log2_numbits-1 or log2_numbits
	bits (depending on whether the leftmost log2_numbits >= qbits or not) via a simple shift.
	*/
	/* Leftward bit at which to start the l-r binary powering, assuming
	the leftmost 7/8 bits have already been processed via a shift.

	Since 7 bits with leftmost bit = 1 is guaranteed
	to be in [64,127], the shift count here is in [0, 63].
	That means that zstart < 2^64. Together with the fact that
	squaring a power of two gives another power of two, we can
	simplify the modmul code sequence for the first iteration.
	Every little bit counts (literally in this case :), right?
	*/
	/* Extract leftmost log2_numbits bits of pshift (if >= qbits, use the leftmost log2_numbits-1) and subtract from qbits: */
	pbits = mi64_extract_lead64(pshift,len_p,&lo64);
	ASSERT(HERE, pbits >= log2_numbits, "leadz64!");
//	if(pbits >= 64)
		lead_chunk = lo64>>(64-log2_numbits);
//	else
//		lead_chunk = lo64>>(pbits-log2_numbits);	**** lead_chunk now normalized to have >= 64 bits even if arg < 2^64 ****

	if(lead_chunk >= qbits) {
		lead_chunk >>= 1;
	#if MI64_POW_DBG
		if(dbg) printf("lead%u = %llu\n", log2_numbits-1,lead_chunk);
	#endif
		start_index = pbits-(log2_numbits-1);	/* Use only the leftmost log2_numbits-1 bits */
	} else {
	#if MI64_POW_DBG
		if(dbg) printf("lead%u = %llu\n", log2_numbits  ,lead_chunk);
	#endif
		start_index = pbits-log2_numbits;
	}

	zshift = (qbits-1) - lead_chunk;
	zshift <<= 1;				/* Doubling the shift count here takes cares of the first SQR_LOHI */
  #if MI64_POW_DBG
	if(dbg) {
		printf("zshift  = %u\n", zshift);
		printf("pshift = p + %u = %s\n",lenQ*64, &cbuf[convert_mi64_base10_char(cbuf, pshift, lenP, 0)]);
	}
  #endif
	mi64_negl(pshift, pshift, lenP);	/* ~pshift[] */

	/*
	Find modular inverse (mod 2^qbits) of q in preparation for modular multiply.
	q must be odd for Montgomery-style modmul to work.

	Init qinv = q. This formula returns the correct bottom 4 bits of qinv,
	and we double the number of correct bits on each of the subsequent iterations.
	*/
	ASSERT(HERE, (q[0] & (uint64)1) == 1, "q must be odd!");
	mi64_clear(qinv, lenQ);

	/* Newton iteration involves repeated steps of form
		qinv = qinv*(2 - q*qinv);
	Number of significant (low) bits doubles on each iteration, starting from 8 for the initial seed read
	from the minv8[] lookup table. The doubling continues until we reach the bitwidth set by the MULL operation.
	*/
	uint32 q32,qi32;
	q32 = q[0]; qi32 = minv8[(q32&0xff)>>1];
	qi32 = qi32*((uint32)2 - q32*qi32);	// 16 good low bits
	qi32 = qi32*((uint32)2 - q32*qi32);	// 32 good low bits
	qinv[0] = qi32;
	qinv[0] = qinv[0]*((uint64)2 - q[0]*qinv[0]);	// 64 good low bits

	/* Now that have bottom 64 = 2^6 bits of qinv, do as many more Newton iterations as needed to get the full [qbits] of qinv: */
	for(j = 6; j < log2_numbits; j++) {
		mi64_mul_vector_lo_half(q, qinv, x, lenQ);
		mi64_nega              (x, x, lenQ);
		mi64_add_scalar(x, 2ull, x, lenQ);	// Discard any carryout here, since only care about low lenQ words
		mi64_mul_vector_lo_half(qinv, x, qinv, lenQ);
	}
	// Check the computed inverse:
	mi64_mul_vector_lo_half(q, qinv, x, lenQ);
	ASSERT(HERE, mi64_cmp_eq_scalar(x, 1ull, lenQ), "Bad Montmul inverse!");
  #if MI64_POW_DBG
	if(dbg) {
		printf("q    = %s\n", &cbuf[convert_mi64_base10_char(cbuf, q   , lenQ, 0)]);
		printf("qinv = %s\n", &cbuf[convert_mi64_base10_char(cbuf, qinv, lenQ, 0)]);
		printf("start_index = %3d\n", start_index);
	}
  #endif

	/* Since zstart is a power of two < 2^192, use a streamlined code sequence for the first iteration: */
	j = start_index-1;

	/* MULL(zstart,qinv,lo) simply amounts to a left-shift of the bits of qinv: */
	mi64_shl(qinv, x, zshift, lenQ);
  #if MI64_POW_DBG
	if(dbg) printf("zshift = %d, (qinv << zshift) = %s\n", zshift, &cbuf[convert_mi64_base10_char(cbuf, x, lenQ, 0)]);
  #endif
  if(FERMAT) {
	mi64_mul_vector_hi_qferm(x, pow2, k, lo, (lenQ<<6));
  } else {
	mi64_mul_vector_hi_half(q, x, lo, lenQ);
  }
  #if MI64_POW_DBG
	if(dbg) printf("q*lo/2^%u = %s\n", (lenQ<<6), &cbuf[convert_mi64_base10_char(cbuf, lo, lenQ, 0)]);
  #endif
	/* hi = 0 in this instance, which simplifies things. */
	cyout = mi64_sub(q, lo, x, lenQ);	ASSERT(HERE, cyout == 0ull, "");
	if(mi64_test_bit(pshift, j)) {
		/* Combines overflow-on-add and need-to-subtract-q-from-sum checks */
		if(mi64_cmpugt(x, qhalf, lenQ)) {
			cyout = mi64_add(x, x, x, lenQ);
			cyout = mi64_sub(x, q, x, lenQ);
		} else {
			cyout = mi64_add(x, x, x, lenQ);	ASSERT(HERE, cyout == 0ull, "");
		}
	}
  #if MI64_POW_DBG
	if(dbg) printf("x0 = %s\n", &cbuf[convert_mi64_base10_char(cbuf, x, lenQ, 0)] );
  #endif

	for(j = start_index-2; j >= 0; j--)
	{
	#if MI64_POW_DBG
		if(dbg) printf("J = %d, lenQ = %u:\n",j,lenQ);
	#endif
		/*...x^2 mod q is returned in x. */
		mi64_sqr_vector(x, lo, lenQ);
	#if MI64_POW_DBG
		if(dbg) {
			printf("lo = %s\n",&cbuf[convert_mi64_base10_char(cbuf, lo, lenQ, 0)]);
			printf("hi = %s\n",&cbuf[convert_mi64_base10_char(cbuf, hi, lenQ, 0)]);
		}
	#endif
		mi64_mul_vector_lo_half(lo, qinv, x, lenQ);
	#if MI64_POW_DBG
		if(dbg) printf("x = %s\n", &cbuf[convert_mi64_base10_char(cbuf, x, lenQ, 0)] );
	#endif
	  if(FERMAT) {
		mi64_mul_vector_hi_qferm(x, pow2, k, lo, (lenQ<<6));
	  #if MI64_POW_DBG
		if(dbg) {
			mi64_mul_vector_hi_half(q, x, x, lenQ);
			if(!mi64_cmp_eq(lo,x,lenQ)) {
				printf("lo = MULH_QFERM = %s\n", &cbuf[convert_mi64_base10_char(cbuf,lo, lenQ, 0)] );
				printf("lo = MULH       = %s\n", &cbuf[convert_mi64_base10_char(cbuf, x, lenQ, 0)] );
				printf("Mismatch! pow2 = %u, k = %llu\n",pow2,k);
				exit(0);
			}
		}
	  #endif
	  } else {
		mi64_mul_vector_hi_half(q, x, lo, lenQ);
	  }
	#if MI64_POW_DBG
		if(dbg) printf("lo = %s\n", &cbuf[convert_mi64_base10_char(cbuf, lo, lenQ, 0)] );
	#endif

		/* If h < l, then calculate q-l+h < q; otherwise calculate h-l. */
		if(mi64_cmpult(hi, lo, lenQ)) {
		#if MI64_POW_DBG
			if(dbg) printf("h < l: computing q-l+h...\n");
		#endif
			cyout = mi64_sub(q, lo, lo, lenQ);
			cyout = mi64_add(lo, hi, x, lenQ);
		} else {
			cyout = mi64_sub(hi, lo, x, lenQ);	ASSERT(HERE, cyout == 0ull, "");
		}

		if(mi64_test_bit(pshift, j)) {
		#if MI64_POW_DBG
			if(dbg) printf("2x...\n");
		#endif
			ASSERT(HERE, mi64_cmpult(x, q, lenQ), "x >= q");
			/* Combines overflow-on-add and need-to-subtract-q-from-sum checks */
			if(mi64_cmpugt(x, qhalf, lenQ)) {
			#if MI64_POW_DBG
				if(dbg) printf("x > q/2: computing 2x-q...\n");
			#endif
				cyout = mi64_add(x, x, x, lenQ);
				cyout = mi64_sub(x, q, x, lenQ);
			} else {
				cyout = mi64_add(x, x, x, lenQ);	ASSERT(HERE, cyout == 0ull, "");
			}
		}
		#if MI64_POW_DBG
			if(dbg)
			printf("x = %s\n",&cbuf[convert_mi64_base10_char(cbuf, x, lenQ, 0)]);
		#endif
	}

	/*...Double and return.	These are specialized for the case
	where 2^p == 1 mod q implies divisibility, in which case x = (q+1)/2.
	*/
	cyout = mi64_add_cyin(x, x, x, lenQ, FERMAT);	/* In the case of interest, x = (q+1)/2, so x + x cannot overflow. */
	cyout = mi64_sub     (x, q, x, lenQ);
	if(res != 0x0) {
		mi64_set_eq(res, x, lenQ);
	}
  #if MI64_POW_DBG
	if(dbg) {
		printf("xout = %s\n",&cbuf[convert_mi64_base10_char(cbuf, x, lenQ, 0)]);
		exit(0);
	}
  #endif
	return mi64_cmp_eq_scalar(x, 1ull, lenQ);
}

#endif	// __CUDA_ARCH__ ?

/****************************************************************************************************/
/* Specialized version of mi64_twopmodq for moduli q = 2.k.M(p) + 1, where M(p) is a Mersenne prime */
/****************************************************************************************************/
#ifndef __CUDA_ARCH__
uint32 mi64_twopmodq_qmmp(const uint64 p, const uint64 k, uint64*res)//, uint32 half_mul_words)	<*** 05/02/2012: Compute required array length needed for modulus on the fly, adjust as needed
{
  #if MI64_POW_DBG
	uint32 dbg = (k==4296452645ull);
  #endif
	 int32 j;
	uint32 lenP, lenQ, pbits;
	uint64 k2 = k+k, lead_chunk, lo64, cyout;
	static uint64 *q = 0x0, *qhalf = 0x0, *qinv = 0x0, *x = 0x0, *lo = 0x0, *hi = 0x0;
	static uint64 psave = 0, *pshift = 0x0;
	static uint32 lenQ_save = 0, qbits, log2_numbits, start_index, zshift;
	static uint32  first_entry = TRUE;

	// Quick computation of number of uint64 needed to hold current q:
	ASSERT(HERE, (k != 0) && ((k2>>1) == k), "2*k overflows!");	// Make sure 2*k does not overflow
	j = (p+1)&63;	// p+1 mod 64, needed since q = 2*k*MMp+1 ~= k*MM(p+1)
	lenP = ((p+1) + 63)>>6;	// #64-bit words needed
	lo64 = k;		// Copy of k
	if( (j == 0) || ((lo64 << j) >> j) != k ) {	// 2*k*MMp crosses a word boundary, need one more word than required by MMp to store it
		lenQ = lenP + 1;
	} else {
		lenQ = lenP;
	}
  #if MI64_POW_DBG
	if(dbg) { printf("mi64_twopmodq_qmmp: k = %llu, lenP = %u, lenQ = %u\n",k,lenP,lenQ); }
  #endif

	if(first_entry || (p != psave) || (lenQ != lenQ_save))
	{
		first_entry = FALSE;
		psave = p;
		free((void *)pshift);
		pshift = (uint64 *)calloc((lenP+1), sizeof(uint64));
		pshift[0] = 1;
		mi64_shl(pshift, pshift, p, lenP);	// 2^p
		mi64_sub_scalar(pshift, 1, pshift, lenP);	// M(p) = 2^p-1
		/* pshift = p + len*64: */
		pshift[lenP] = mi64_add_scalar(pshift, lenP*64, pshift, lenP);
		ASSERT(HERE, !pshift[lenP], "pshift overflows!");
	#if MI64_POW_DBG
		if(dbg) { printf("mi64_twopmodq_qmmpInit: k = %llu, lenP = %u, lenQ = %u\n",k,lenP,lenQ); }
	#endif
		lenQ_save = lenQ;
		free((void *)q    );
		free((void *)qhalf);
		free((void *)qinv );
		free((void *)x    );
		free((void *)lo   );
		q      = (uint64 *)calloc((lenQ), sizeof(uint64));
		qhalf  = (uint64 *)calloc((lenQ), sizeof(uint64));
		qinv   = (uint64 *)calloc((lenQ), sizeof(uint64));
		x      = (uint64 *)calloc((lenQ), sizeof(uint64));
		lo   = (uint64 *)calloc((2*lenQ), sizeof(uint64));
		hi   = lo + lenQ;	/* Pointer to high half of double-wide product */

		qbits = lenQ << 6;
		log2_numbits = ceil(log(1.0*qbits)/log(2.0));

	/*
	Find position of the leftmost ones bit in pshift, and subtract log2_numbits-1 or log2_numbits
	to account for the fact that we can do the powering for the leftmost log2_numbits-1 or log2_numbits
	bits (depending on whether the leftmost log2_numbits >= qbits or not) via a simple shift.
	*/
		/* Leftward bit at which to start the l-r binary powering, assuming
		the leftmost 7/8 bits have already been processed via a shift.

		Since 7 bits with leftmost bit = 1 is guaranteed
		to be in [64,127], the shift count here is in [0, 63].
		That means that zstart < 2^64. Together with the fact that
		squaring a power of two gives another power of two, we can
		simplify the modmul code sequence for the first iteration.
		Every little bit counts (literally in this case :), right?
		*/
	/* Extract leftmost log2_numbits bits of pshift (if >= qbits, use the leftmost log2_numbits-1) and subtract from qbits: */
		pbits = mi64_extract_lead64(pshift,lenP,&lo64);
		ASSERT(HERE, pbits >= log2_numbits, "leadz64!");
	//	if(pbits >= 64)
			lead_chunk = lo64>>(64-log2_numbits);
	//	else
	//		lead_chunk = lo64>>(pbits-log2_numbits);	**** lead_chunk now normalized to have >= 64 bits even if arg < 2^64 ****

		if(lead_chunk >= qbits) {
			lead_chunk >>= 1;
		#if MI64_POW_DBG
			if(dbg) { printf("lead%u = %llu\n", log2_numbits-1,lead_chunk); }
		#endif
			start_index = pbits-(log2_numbits-1);	/* Use only the leftmost log2_numbits-1 bits */
		} else {
		#if MI64_POW_DBG
			if(dbg) { printf("lead%u = %llu\n", log2_numbits  ,lead_chunk); }
		#endif
			start_index = pbits-log2_numbits;
		}

		zshift = (qbits-1) - lead_chunk;	// For MMp this = 2^(log2_numbits-1) = 100...000.
		zshift <<= 1;				/* Doubling the shift count here takes cares of the first SQR_LOHI */
		mi64_negl(pshift, pshift, lenP);	/* ~pshift[] */
	#if MI64_POW_DBG
		if(dbg) { printf("pshift = %s\n", &cbuf[convert_mi64_base10_char(cbuf, pshift, lenP, 0)]); }
	#endif
	}

	/* compute q */
	memset(q, 0ull, (lenQ<<3));
  #if 1	// Oddly, the "slower" way here gives a significantly faster binary:
	q[0] = 1; mi64_shl(q, q, p, lenQ);
	mi64_sub_scalar(q, 1, q, lenQ);	// M(p) = 2^p-1
	cyout = mi64_mul_scalar(q, k2, q, lenQ);
	ASSERT(HERE, !cyout, "2.k.M(p) overflows!");	// 2.k.M(p)
	ASSERT(HERE, 0 != q[lenQ-1], "Excessive word size allocated for q!");
	mi64_add_scalar(q, 1ull, q, lenQ);	// q = 2.k.M(p) + 1
	mi64_shrl(q, qhalf, 1, lenQ);	/* (q >> 1) = (q-1)/2, since q odd. */
  #else
	// 2^p; cheaper than the "long way": q[0] = 1; mi64_shl(q, q, p, lenQ);
	j = p>>6;	// p/64; the set-bit in 2^p goes into the (j)th word of q[]
	q[j] = ( 1ull << (p-(j<<6)) );
	mi64_sub_scalar(q, 1, q, lenQ);	// M(p) = 2^p-1
	cyout = mi64_mul_scalar(q, k2, q, lenQ);	ASSERT(HERE, !cyout, "2.k.M(p) overflows!");	// 2.k.M(p)
	ASSERT(HERE, 0 != q[lenQ-1], "Excessive word size allocated for q!");
	mi64_add_scalar(q, 1ull, q, lenQ);	// q = 2.k.M(p) + 1
	mi64_shrl_short(q,qhalf, 1, lenQ);	// qhalf = (q >> 1) = (q-1)/2, since q odd.
  #endif
	/*
	Find modular inverse (mod 2^qbits) of q in preparation for modular multiply.
	q must be odd for Montgomery-style modmul to work.
	*/
	ASSERT(HERE, (q[0] & (uint64)1) == 1, "q must be odd!");
	mi64_clear(qinv, lenQ);

	/* Newton iteration involves repeated steps of form
		qinv = qinv*(2 - q*qinv);
	Number of significant (low) bits doubles on each iteration, starting from 8 for the initial seed read
	from the minv8[] lookup table. The doubling continues until we reach the bitwidth set by the MULL operation.
	*/
	uint32 q32,qi32;
	q32 = q[0]; qi32 = minv8[(q32&0xff)>>1];
	qi32 = qi32*((uint32)2 - q32*qi32);	// 16 good low bits
	qi32 = qi32*((uint32)2 - q32*qi32);	// 32 good low bits
	qinv[0] = qi32;
	qinv[0] = qinv[0]*((uint64)2 - q[0]*qinv[0]);	// 64 good low bits

	/* Now that have bottom 64 = 2^6 bits of qinv, do as many more Newton iterations as needed to get the full [qbits] of qinv: */
	for(j = 6; j < log2_numbits; j++) {
		mi64_mul_vector_lo_half(q, qinv, x, lenQ);
		mi64_nega              (x, x, lenQ);
		mi64_add_scalar(x, 2ull, x, lenQ);
		mi64_mul_vector_lo_half(qinv, x, qinv, lenQ);
	}
	// Check the computed inverse:
	mi64_mul_vector_lo_half(q, qinv, x, lenQ);
	ASSERT(HERE, mi64_cmp_eq_scalar(x, 1ull, lenQ), "Bad Montmul inverse!");
  #if MI64_POW_DBG
	if(dbg) {
		printf("q    = %s\n", &cbuf[convert_mi64_base10_char(cbuf, q   , lenQ, 0)]);
		printf("qinv = %s\n", &cbuf[convert_mi64_base10_char(cbuf, qinv, lenQ, 0)]);
		printf("start_index = %3d\n", start_index);
	}
  #endif

	/* Since zstart is a power of two < 2^192, use a streamlined code sequence for the first iteration: */
	j = start_index-1;

	/* MULL(zstart,qinv,lo) simply amounts to a left-shift of the bits of qinv: */
  #if MI64_POW_DBG
	if(dbg) { printf("zshift = %d, lo = (qinv << zshift)...\n", zshift); }
  #endif
	mi64_shl(qinv, lo, zshift, lenQ);
  #if MI64_POW_DBG
	if(dbg) { printf("lo = %s\n", &cbuf[convert_mi64_base10_char(cbuf, lo, lenQ, 0)]); }
  #endif
	mi64_mul_vector_hi_half(q, lo, lo, lenQ);
  #if MI64_POW_DBG
	if(dbg) { printf("q*lo/2^%u = %s\n", (lenQ<<6), &cbuf[convert_mi64_base10_char(cbuf, lo, lenQ, 0)]); }
  #endif

	/* hi = 0 in this instance, which simplifies things. */
	cyout = mi64_sub(q, lo, x, lenQ);	ASSERT(HERE, cyout == 0ull, "");

	// mi64_test_bit(pshift, j) always true for this portion of MMp powering
	ASSERT(HERE, mi64_test_bit(pshift, j), "pshift bit = 0 for pre-loop step!");
	ASSERT(HERE, mi64_cmpult(x, q, lenQ), "x >= q");
	/* Combines overflow-on-add and need-to-subtract-q-from-sum checks */
	if(mi64_cmpugt(x, qhalf, lenQ)) {
		cyout = mi64_add(x, x, x, lenQ);
		cyout = mi64_sub(x, q, x, lenQ);
	} else {
		cyout = mi64_add(x, x, x, lenQ);	ASSERT(HERE, cyout == 0ull, "");
	}

  #if MI64_POW_DBG
	if(dbg) { printf("x0 = %s\n", &cbuf[convert_mi64_base10_char(cbuf, x, lenQ, 0)] ); }
  #endif

	for(j = start_index-2; j >= log2_numbits; j--)
	{
	#if MI64_POW_DBG
		if(dbg) { printf("J = %d:\n",j); }
	#endif
		/*...x^2 mod q is returned in x. */
		mi64_sqr_vector(x, lo, lenQ);
	#if MI64_POW_DBG
		if(dbg) {
			printf("lo = %s\n",&cbuf[convert_mi64_base10_char(cbuf, lo, lenQ, 0)]);
			printf("hi = %s\n",&cbuf[convert_mi64_base10_char(cbuf, hi, lenQ, 0)]);
		}
	#endif
		mi64_mul_vector_lo_half(lo, qinv, x, lenQ);
	#if MI64_POW_DBG
		if(dbg) { printf(" x = %s\n",&cbuf[convert_mi64_base10_char(cbuf,  x, lenQ, 0)]); }
	#endif
		mi64_mul_vector_hi_qmmp(x, p, k, lo, (lenQ<<6));
	#if MI64_POW_DBG
		if(dbg) { printf("lo = %s\n",&cbuf[convert_mi64_base10_char(cbuf, lo, lenQ, 0)]); }
	#endif

		/* If h < l, then calculate q-l+h < q; otherwise calculate h-l. */
		if(mi64_cmpult(hi, lo, lenQ)) {
			cyout = mi64_sub(q, lo, lo, lenQ);
			cyout = mi64_add(lo, hi, x, lenQ);
		} else {
			cyout = mi64_sub(hi, lo, x, lenQ);	ASSERT(HERE, cyout == 0ull, "");
		}

	#if MI64_POW_DBG
		if(dbg) { printf("x = %s\n",&cbuf[convert_mi64_base10_char(cbuf, x, lenQ, 0)]); }
	#endif
		// mi64_test_bit(pshift, j) always true for this portion of MMp powering
		ASSERT(HERE, mi64_test_bit(pshift, j), "pshift bit = 0!");
	#if MI64_POW_DBG
		if(!mi64_cmpult(x, q, lenQ)) {
			printf("x < q test failed for k = %llu, j = %u!\n",k,j);
		}
		if(dbg) { printf("2x...\n"); }
	#else
		ASSERT(HERE, mi64_cmpult(x, q, lenQ), "x >= q");
	#endif

		/* Combines overflow-on-add and need-to-subtract-q-from-sum checks */
		if(mi64_cmpugt(x, qhalf, lenQ)) {
			cyout = mi64_add(x, x, x, lenQ);
			cyout = mi64_sub(x, q, x, lenQ);
		} else {
			cyout = mi64_add(x, x, x, lenQ);	ASSERT(HERE, cyout == 0ull, "");
		}
	}
	for(; j >= 0; j--)
	{
		/*...x^2 mod q is returned in x. */
		mi64_sqr_vector(x, lo, lenQ);
		mi64_mul_vector_lo_half(lo, qinv, x, lenQ);
		mi64_mul_vector_hi_qmmp(x, p, k, lo,(lenQ<<6));

		if(mi64_cmpult(hi, lo, lenQ)) {
			cyout = mi64_sub(q, lo, lo, lenQ);
			cyout = mi64_add(lo, hi, x, lenQ);
		} else {
			cyout = mi64_sub(hi, lo, x, lenQ);	ASSERT(HERE, cyout == 0ull, "");
		}

		if((pshift[0] >> j) & (uint64)1) {
			ASSERT(HERE, mi64_cmpult(x, q, lenQ), "x >= q");
			/* Combines overflow-on-add and need-to-subtract-q-from-sum checks */
			if(mi64_cmpugt(x, qhalf, lenQ)) {
				cyout = mi64_add(x, x, x, lenQ);
				cyout = mi64_sub(x, q, x, lenQ);
			} else {
				cyout = mi64_add(x, x, x, lenQ);	ASSERT(HERE, cyout == 0ull, "");
			}
		}
	}

	/*...Double and return.	These are specialized for the case
	where 2^p == 1 mod q implies divisibility, in which case x = (q+1)/2.
	*/
	mi64_add(x, x, x, lenQ);	/* In the case of interest, x = (q+1)/2, so x + x cannot overflow. */
	mi64_sub(x, q, x, lenQ);
	if(res != 0x0) {
		mi64_set_eq(res, x, lenQ);
	}
  #if MI64_POW_DBG
	if(dbg) {
		printf("mi64_twopmodq_qmmp: xout = %s\n", &cbuf[convert_mi64_base10_char(cbuf, x, lenQ, 0)] );
		exit(0);
	}
  #endif
	return mi64_cmp_eq_scalar(x, 1ull, lenQ);
}
#endif	// __CUDA_ARCH__ ?
