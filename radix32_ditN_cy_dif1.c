/*******************************************************************************
*                                                                              *
*   (C) 1997-2013 by Ernst W. Mayer.                                           *
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

#include "Mlucas.h"

#define FFT_DEBUG	0
#if FFT_DEBUG
	char dbg_fname[] = "a.txt";
#endif

#ifdef CTIME	// define at compile time to enable internal timing diagnostics
	double dt_fwd, dt_inv, dt_cy, dt_tot;
	clock_t clock1, clock2, clock3;
#endif

#ifdef USE_SSE2

	const int radix32_creals_in_local_store = 128;

	#undef DEBUG_SSE2
//	#define DEBUG_SSE2

//	#define ERR_CHECK_ALL	/* #define this to do ROE checking of all convolution outputs, rather than just every 36th one */

	#ifdef COMPILER_TYPE_MSVC

		#include "sse2_macro.h"
		/* carry.h already included by Mlucas.h */

		#include "radix32_ditN_cy_dif1_win32.h"

	#else	/* GCC-style inline ASM: */

		#if OS_BITS == 32

			#include "radix32_ditN_cy_dif1_gcc32.h"

		#else

			#include "radix32_ditN_cy_dif1_gcc64.h"

		#endif

	#endif

  #ifdef USE_PTHREAD

	// Use non-pooled simple spawn/rejoin thread-team model
	#include "threadpool.h"

	struct cy_thread_data_t{
	// int data - if needed, pad to yield an even number of these:
		int tid;
		int ndivr;
		int _pad0;	// Pads to make sizeof this struct a multiple of 16 bytes
		int _pad1;
	
		int khi;
		int i;
		int jstart;
		int jhi;
		int col;
		int co2;
		int co3;
		int sw;
		int nwt;
		int _pad_;

	// double data:
		double maxerr;
		double scale;

	// pointer data:
		double *arrdat;			/* Main data array */
		double *wt0;
		double *wt1;
		int *si;
		struct complex *rn0;
		struct complex *rn1;
		struct complex *r00;

		int bjmodn00;
		int bjmodn01;
		int bjmodn02;
		int bjmodn03;
		int bjmodn04;
		int bjmodn05;
		int bjmodn06;
		int bjmodn07;
		int bjmodn08;
		int bjmodn09;
		int bjmodn0A;
		int bjmodn0B;
		int bjmodn0C;
		int bjmodn0D;
		int bjmodn0E;
		int bjmodn0F;
		int bjmodn10;
		int bjmodn11;
		int bjmodn12;
		int bjmodn13;
		int bjmodn14;
		int bjmodn15;
		int bjmodn16;
		int bjmodn17;
		int bjmodn18;
		int bjmodn19;
		int bjmodn1A;
		int bjmodn1B;
		int bjmodn1C;
		int bjmodn1D;
		int bjmodn1E;
		int bjmodn1F;
		/* carries: */
		double cy_r00;
		double cy_r01;
		double cy_r02;
		double cy_r03;
		double cy_r04;
		double cy_r05;
		double cy_r06;
		double cy_r07;
		double cy_r08;
		double cy_r09;
		double cy_r0A;
		double cy_r0B;
		double cy_r0C;
		double cy_r0D;
		double cy_r0E;
		double cy_r0F;
		double cy_r10;
		double cy_r11;
		double cy_r12;
		double cy_r13;
		double cy_r14;
		double cy_r15;
		double cy_r16;
		double cy_r17;
		double cy_r18;
		double cy_r19;
		double cy_r1A;
		double cy_r1B;
		double cy_r1C;
		double cy_r1D;
		double cy_r1E;
		double cy_r1F;

		double cy_i00;
		double cy_i01;
		double cy_i02;
		double cy_i03;
		double cy_i04;
		double cy_i05;
		double cy_i06;
		double cy_i07;
		double cy_i08;
		double cy_i09;
		double cy_i0A;
		double cy_i0B;
		double cy_i0C;
		double cy_i0D;
		double cy_i0E;
		double cy_i0F;
		double cy_i10;
		double cy_i11;
		double cy_i12;
		double cy_i13;
		double cy_i14;
		double cy_i15;
		double cy_i16;
		double cy_i17;
		double cy_i18;
		double cy_i19;
		double cy_i1A;
		double cy_i1B;
		double cy_i1C;
		double cy_i1D;
		double cy_i1E;
		double cy_i1F;
	};

  #endif

#endif	/* USE_SSE2 */

/***************/

/* If using the FFT routines for a standalone build of the GCD code,
don't need the special-number carry routines: */
#ifdef GCD_STANDALONE

int radix32_ditN_cy_dif1(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter, double *fracmax, uint64 p)
{
	ASSERT(HERE, 0,"radix32_ditN_cy_dif1 should not be called if GCD_STANDALONE is set!");
	return 0;
}

#else

/***************/

int radix32_ditN_cy_dif1(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter, double *fracmax, uint64 p)
{
/*
!...Acronym: DWT = Discrete Weighted Transform, DIT = Decimation In Time, DIF = Decimation In Frequency
!
!...Performs a final radix-32 complex DIT pass, an inverse DWT weighting, a carry propagation,
!   a forward DWT weighting, and an initial radix-32 complex DIF pass on the data in the length-N real vector A.
!
!   Data enter and are returned in the A-array.
!
!   See the documentation in mers_mod_square and radix16_dif_pass for further details on the array
!   storage scheme, and radix16_ditN_cy_dif1 for details on the reduced-length weights array scheme.
*/
	const uint32 RADIX = 32;
	const double crnd = 3.0*0x4000000*0x2000000;
	int idx_offset,idx_incr,NDIVR;
	int i,j,j1,j2,jstart,jhi,full_pass,k,khi,outer;
	int l,col,co2,co3,n_minus_sil,n_minus_silp1,sinwt,sinwtm1;
	static uint32 bjmodnini;
	static uint64 psave=0;
	static uint32 bw,sw,nm1,p01,p02,p03,p04,p05,p06,p07,p08,p0C,p10,p14,p18,p1C;
	static double    c     = 0.92387953251128675613, s     = 0.38268343236508977173	/* exp(  i*twopi/16)	*/
			,c32_1 = 0.98078528040323044912, s32_1 = 0.19509032201612826784	/* exp(  i*twopi/32)	*/
			,c32_3 = 0.83146961230254523708, s32_3 = 0.55557023301960222473 /* exp(3*i*twopi/32)	*/
			,radix_inv, n2inv;
	double scale
		,t00,t01,t02,t03,t04,t05,t06,t07,t08,t09,t0A,t0B,t0C,t0D,t0E,t0F
		,t10,t11,t12,t13,t14,t15,t16,t17,t18,t19,t1A,t1B,t1C,t1D,t1E,t1F
		,t20,t21,t22,t23,t24,t25,t26,t27,t28,t29,t2A,t2B,t2C,t2D,t2E,t2F
		,t30,t31,t32,t33,t34,t35,t36,t37,t38,t39,t3A,t3B,t3C,t3D,t3E,t3F;
	double maxerr = 0.0;
	int err;
	static int first_entry=TRUE;

	int n_div_nwt;

#ifdef USE_SSE2

	static int cslots_in_local_store;
	static struct complex *sc_arr = 0x0, *sc_ptr;
	static uint64 *sm_ptr, *sign_mask, *sse_bw, *sse_sw, *sse_nm1;
	uint64 tmp64;

  #ifdef MULTITHREAD

	#ifdef USE_PTHREAD
		static struct complex *__r0;	// Base address for discrete per-thread local stores
		static struct cy_thread_data_t *tdat = 0x0;
		// Threadpool-based dispatch stuff:
		static int main_work_units = 0, pool_work_units = 0;
		static struct threadpool *tpool = 0x0;
		static int task_is_blocking = TRUE;
		static thread_control_t thread_control = {0,0,0};
		// First 3 subfields same for all threads, 4th provides thread-specifc data, will be inited at thread dispatch:
		static task_control_t   task_control = {NULL, (void*)cy32_process_chunk, NULL, 0x0};
	#endif

  #else

//	int i0,i1,m0,m1,m3;	/* m2 already def'd for regular carry sequence */
	double *add0, *add1, *add2;
//	double *add3, *add4, *add5, *add6, *add7;	/* Addresses into array sections */
/*...stuff for the reduced-length DWT weights array is here:	*/
	double wtl,wtlp1,wtn,wtnm1;	/* Mersenne-mod weights stuff */

  #endif

	static struct complex *isrt2, *cc0, *ss0, *cc1, *ss1, *cc3, *ss3, *max_err, *sse2_rnd, *half_arr, *tmp
		,*r00,*r01,*r02,*r03,*r04,*r05,*r06,*r07,*r08,*r09,*r0A,*r0B,*r0C,*r0D,*r0E,*r0F
		,*r10,*r11,*r12,*r13,*r14,*r15,*r16,*r17,*r18,*r19,*r1A,*r1B,*r1C,*r1D,*r1E,*r1F
		,*r20,*r21,*r22,*r23,*r24,*r25,*r26,*r27,*r28,*r29,*r2A,*r2B,*r2C,*r2D,*r2E,*r2F
		,*r30,*r31,*r32,*r33,*r34,*r35,*r36,*r37,*r38,*r39,*r3A,*r3B,*r3C,*r3D,*r3E,*r3F;

	static struct complex *cy_r00,*cy_r02,*cy_r04,*cy_r06,*cy_r08,*cy_r0A,*cy_r0C,*cy_r0E,*cy_r10,*cy_r12,*cy_r14,*cy_r16,*cy_r18,*cy_r1A,*cy_r1C,*cy_r1E;
	static struct complex *cy_i00,*cy_i02,*cy_i04,*cy_i06,*cy_i08,*cy_i0A,*cy_i0C,*cy_i0E,*cy_i10,*cy_i12,*cy_i14,*cy_i16,*cy_i18,*cy_i1A,*cy_i1C,*cy_i1E;
	static int *bjmodn00,*bjmodn01,*bjmodn02,*bjmodn03,*bjmodn04,*bjmodn05,*bjmodn06,*bjmodn07,*bjmodn08,*bjmodn09,*bjmodn0A,*bjmodn0B,*bjmodn0C,*bjmodn0D,*bjmodn0E,*bjmodn0F,*bjmodn10,*bjmodn11,*bjmodn12,*bjmodn13,*bjmodn14,*bjmodn15,*bjmodn16,*bjmodn17,*bjmodn18,*bjmodn19,*bjmodn1A,*bjmodn1B,*bjmodn1C,*bjmodn1D,*bjmodn1E,*bjmodn1F;

  #ifdef DEBUG_SSE2
	int jt,jp;
  #endif

#else

	const  double one_half[3] = {1.0, 0.5, 0.25};	/* Needed for small-weights-tables scheme */
	double rt,it;
	int jt,jp,k1,k2,m,m2;
	double wt,wtinv,wtl,wtlp1,wtn,wtnm1,wtA,wtB,wtC;	/* Mersenne-mod weights stuff */
	static double wt_re,wt_im;									/* Fermat-mod weights stuff */
  #if PFETCH
	double *addr;
	int prefetch_offset;
  #endif

	double temp,frac;
	int bjmodn00,bjmodn01,bjmodn02,bjmodn03,bjmodn04,bjmodn05,bjmodn06,bjmodn07,bjmodn08,bjmodn09,bjmodn0A,bjmodn0B,bjmodn0C,bjmodn0D,bjmodn0E,bjmodn0F,bjmodn10,bjmodn11,bjmodn12,bjmodn13,bjmodn14,bjmodn15,bjmodn16,bjmodn17,bjmodn18,bjmodn19,bjmodn1A,bjmodn1B,bjmodn1C,bjmodn1D,bjmodn1E,bjmodn1F;
	double
	 a1p00r,a1p01r,a1p02r,a1p03r,a1p04r,a1p05r,a1p06r,a1p07r,a1p08r,a1p09r,a1p0Ar,a1p0Br,a1p0Cr,a1p0Dr,a1p0Er,a1p0Fr
	,a1p10r,a1p11r,a1p12r,a1p13r,a1p14r,a1p15r,a1p16r,a1p17r,a1p18r,a1p19r,a1p1Ar,a1p1Br,a1p1Cr,a1p1Dr,a1p1Er,a1p1Fr
	,a1p00i,a1p01i,a1p02i,a1p03i,a1p04i,a1p05i,a1p06i,a1p07i,a1p08i,a1p09i,a1p0Ai,a1p0Bi,a1p0Ci,a1p0Di,a1p0Ei,a1p0Fi
	,a1p10i,a1p11i,a1p12i,a1p13i,a1p14i,a1p15i,a1p16i,a1p17i,a1p18i,a1p19i,a1p1Ai,a1p1Bi,a1p1Ci,a1p1Di,a1p1Ei,a1p1Fi
	,cy_r00,cy_r01,cy_r02,cy_r03,cy_r04,cy_r05,cy_r06,cy_r07,cy_r08,cy_r09,cy_r0A,cy_r0B,cy_r0C,cy_r0D,cy_r0E,cy_r0F,cy_r10,cy_r11,cy_r12,cy_r13,cy_r14,cy_r15,cy_r16,cy_r17,cy_r18,cy_r19,cy_r1A,cy_r1B,cy_r1C,cy_r1D,cy_r1E,cy_r1F
	,cy_i00,cy_i01,cy_i02,cy_i03,cy_i04,cy_i05,cy_i06,cy_i07,cy_i08,cy_i09,cy_i0A,cy_i0B,cy_i0C,cy_i0D,cy_i0E,cy_i0F,cy_i10,cy_i11,cy_i12,cy_i13,cy_i14,cy_i15,cy_i16,cy_i17,cy_i18,cy_i19,cy_i1A,cy_i1B,cy_i1C,cy_i1D,cy_i1E,cy_i1F;

#endif

/*...stuff for the multithreaded implementation is here:	*/
	static uint32 CY_THREADS,pini;
	int ithread,j_jhi;
	uint32 ptr_prod;
	static int *_bjmodn00 = 0x0,*_bjmodn01 = 0x0,*_bjmodn02 = 0x0,*_bjmodn03 = 0x0,*_bjmodn04 = 0x0,*_bjmodn05 = 0x0,*_bjmodn06 = 0x0,*_bjmodn07 = 0x0,*_bjmodn08 = 0x0,*_bjmodn09 = 0x0,*_bjmodn0A = 0x0,*_bjmodn0B = 0x0,*_bjmodn0C = 0x0,*_bjmodn0D = 0x0,*_bjmodn0E = 0x0,*_bjmodn0F = 0x0,*_bjmodn10 = 0x0,*_bjmodn11 = 0x0,*_bjmodn12 = 0x0,*_bjmodn13 = 0x0,*_bjmodn14 = 0x0,*_bjmodn15 = 0x0,*_bjmodn16 = 0x0,*_bjmodn17 = 0x0,*_bjmodn18 = 0x0,*_bjmodn19 = 0x0,*_bjmodn1A = 0x0,*_bjmodn1B = 0x0,*_bjmodn1C = 0x0,*_bjmodn1D = 0x0,*_bjmodn1E = 0x0,*_bjmodn1F = 0x0;
	static int *_bjmodnini = 0x0;
	static int *_i, *_jstart = 0x0, *_jhi = 0x0, *_col = 0x0, *_co2 = 0x0, *_co3 = 0x0;
	static double *_maxerr = 0x0,
	*_cy_r00 = 0x0,*_cy_r01 = 0x0,*_cy_r02 = 0x0,*_cy_r03 = 0x0,*_cy_r04 = 0x0,*_cy_r05 = 0x0,*_cy_r06 = 0x0,*_cy_r07 = 0x0,*_cy_r08 = 0x0,*_cy_r09 = 0x0,*_cy_r0A = 0x0,*_cy_r0B = 0x0,*_cy_r0C = 0x0,*_cy_r0D = 0x0,*_cy_r0E = 0x0,*_cy_r0F = 0x0,*_cy_r10 = 0x0,*_cy_r11 = 0x0,*_cy_r12 = 0x0,*_cy_r13 = 0x0,*_cy_r14 = 0x0,*_cy_r15 = 0x0,*_cy_r16 = 0x0,*_cy_r17 = 0x0,*_cy_r18 = 0x0,*_cy_r19 = 0x0,*_cy_r1A = 0x0,*_cy_r1B = 0x0,*_cy_r1C = 0x0,*_cy_r1D = 0x0,*_cy_r1E = 0x0,*_cy_r1F = 0x0,
	*_cy_i00 = 0x0,*_cy_i01 = 0x0,*_cy_i02 = 0x0,*_cy_i03 = 0x0,*_cy_i04 = 0x0,*_cy_i05 = 0x0,*_cy_i06 = 0x0,*_cy_i07 = 0x0,*_cy_i08 = 0x0,*_cy_i09 = 0x0,*_cy_i0A = 0x0,*_cy_i0B = 0x0,*_cy_i0C = 0x0,*_cy_i0D = 0x0,*_cy_i0E = 0x0,*_cy_i0F = 0x0,*_cy_i10 = 0x0,*_cy_i11 = 0x0,*_cy_i12 = 0x0,*_cy_i13 = 0x0,*_cy_i14 = 0x0,*_cy_i15 = 0x0,*_cy_i16 = 0x0,*_cy_i17 = 0x0,*_cy_i18 = 0x0,*_cy_i19 = 0x0,*_cy_i1A = 0x0,*_cy_i1B = 0x0,*_cy_i1C = 0x0,*_cy_i1D = 0x0,*_cy_i1E = 0x0,*_cy_i1F = 0x0;

#ifdef CTIME
	const double ICPS = 1.0/CLOCKS_PER_SEC;
	clock1 = clock();
	dt_fwd = dt_inv = dt_cy = dt_tot = 0.0;
#endif

/*...change NDIVR and n_div_wt to non-static to work around a gcc compiler bug. */
	NDIVR   = n/RADIX;
	n_div_nwt = NDIVR >> nwt_bits;

	if((n_div_nwt << nwt_bits) != NDIVR)
	{
		sprintf(cbuf,"FATAL: iter = %10d; NWT_BITS does not divide N/RADIX in radix32_ditN_cy_dif1.\n",iter);
		if(INTERACT)fprintf(stderr,"%s",cbuf);
		fp = fopen(   OFILE,"a");
		fq = fopen(STATFILE,"a");
		fprintf(fp,"%s",cbuf);
		fprintf(fq,"%s",cbuf);
		fclose(fp);	fp = 0x0;
		fclose(fq);	fq = 0x0;
		err=ERR_CARRY;
		return(err);
	}

	if(p != psave)
	{
		first_entry=TRUE;
	}

/*...initialize things upon first entry: */

	if(first_entry)
	{
		psave = p;
		first_entry=FALSE;
		radix_inv = qfdbl(qf_rational_quotient((int64)1, (int64)32));
		n2inv     = qfdbl(qf_rational_quotient((int64)1, (int64)(n/2)));

		bw    = p%n;		/* Number of bigwords in the Crandall/Fagin mixed-radix representation = (Mersenne exponent) mod (vector length).	*/
		sw    = n - bw;	/* Number of smallwords.	*/

		nm1   = n-1;

	/* to-do: Add threading to sse2 code */
	#ifdef MULTITHREAD

		/* #Chunks ||ized in carry step is ideally a power of 2, so use the smallest
		power of 2 that is >= the value of the global NTHREADS (but still <= MAX_THREADS):
		*/
		if(isPow2(NTHREADS))
			CY_THREADS = NTHREADS;
		else
		{
			i = leadz32(NTHREADS);
			CY_THREADS = (((uint32)NTHREADS << i) & 0x80000000) >> (i-1);
		}

		if(CY_THREADS > MAX_THREADS)
		{
		//	CY_THREADS = MAX_THREADS;
			fprintf(stderr,"WARN: CY_THREADS = %d exceeds number of cores = %d\n", CY_THREADS, MAX_THREADS);
		}
		ASSERT(HERE, CY_THREADS >= NTHREADS,"CY_THREADS < NTHREADS");
		ASSERT(HERE, isPow2(CY_THREADS)    ,"CY_THREADS not a power of 2!");
		if(CY_THREADS > 1)
		{
			ASSERT(HERE, NDIVR    %CY_THREADS == 0,"NDIVR    %CY_THREADS != 0");
			ASSERT(HERE, n_div_nwt%CY_THREADS == 0,"n_div_nwt%CY_THREADS != 0");
		}

	  #ifdef USE_PTHREAD

		j = (uint32)sizeof(struct cy_thread_data_t);
		if(0 != (j & 0xf)) {
			printf("sizeof(cy_thread_data_t) = %x\n",j);
			ASSERT(HERE, 0, "struct cy_thread_data_t not 16-byte size multiple!");
		}
		tdat = (struct cy_thread_data_t *)calloc(CY_THREADS, sizeof(struct cy_thread_data_t));

		// MacOS does weird things with threading (e.g. Idle" main thread burning 100% of 1 CPU)
		// so on that platform try to be clever and interleave main-thread and threadpool-work processing
		#ifdef OS_TYPE_MACOSX

			if(CY_THREADS > 1) {
				main_work_units = CY_THREADS/2;
				pool_work_units = CY_THREADS - main_work_units;
				ASSERT(HERE, 0x0 != (tpool = threadpool_init(pool_work_units, MAX_THREADS, pool_work_units, &thread_control)), "threadpool_init failed!");
				printf("radix%d_ditN_cy_dif1: Init threadpool of %d threads\n", RADIX, pool_work_units);
			} else {
				main_work_units = 1;
				printf("radix%d_ditN_cy_dif1: CY_THREADS = 1: Using main execution thread, no threadpool needed.\n", RADIX);
			}

		#else

			main_work_units = 0;
			pool_work_units = CY_THREADS;
			ASSERT(HERE, 0x0 != (tpool = threadpool_init(CY_THREADS, MAX_THREADS, CY_THREADS, &thread_control)), "threadpool_init failed!");

		#endif

	  #endif

		fprintf(stderr,"Using %d threads in carry step\n", CY_THREADS);

	#else
		CY_THREADS = 1;
	#endif

	#ifdef USE_SSE2

		ASSERT(HERE, ((uint32)wt0    & 0x3f) == 0, "wt0[]  not 64-byte aligned!");
		ASSERT(HERE, ((uint32)wt1    & 0x3f) == 0, "wt1[]  not 64-byte aligned!");

		// Use double-complex type size (16 bytes) to alloc a block of local storage
		// consisting of 128 dcomplex and (12+RADIX/2) uint64 element slots per thread:
		cslots_in_local_store = radix32_creals_in_local_store + (12+RADIX/2)/2;
		sc_arr = ALLOC_COMPLEX(sc_arr, cslots_in_local_store*CY_THREADS);	if(!sc_arr){ sprintf(cbuf, "FATAL: unable to allocate sc_arr!.\n"); fprintf(stderr,"%s", cbuf);	ASSERT(HERE, 0,cbuf); }
		sc_ptr = ALIGN_COMPLEX(sc_arr);
		ASSERT(HERE, ((uint32)sc_ptr & 0x3f) == 0, "sc_ptr not 64-byte aligned!");
		sm_ptr = (uint64*)(sc_ptr + radix32_creals_in_local_store);
		ASSERT(HERE, ((uint32)sm_ptr & 0x3f) == 0, "sm_ptr not 64-byte aligned!");

	/* Use low 64 16-byte slots of sc_arr for temporaries, next 7 for the nontrivial complex 16th roots,
	next 32 for the doubled carry pairs, next 2 for ROE and RND_CONST, next 20 for the half_arr table lookup stuff,
	plus at least 3 more slots to allow for 64-byte alignment of the array:
	*/
	#ifdef USE_PTHREAD
		__r0 = sc_ptr;
	#endif
		r00		= sc_ptr + 0x00;	isrt2	= sc_ptr + 0x40;
		r01		= sc_ptr + 0x01;	cc0		= sc_ptr + 0x41;
		r02		= sc_ptr + 0x02;	ss0		= sc_ptr + 0x42;
		r03		= sc_ptr + 0x03;	cc1		= sc_ptr + 0x43;
		r04		= sc_ptr + 0x04;	ss1		= sc_ptr + 0x44;
		r05		= sc_ptr + 0x05;	cc3		= sc_ptr + 0x45;
		r06		= sc_ptr + 0x06;	ss3		= sc_ptr + 0x46;
		r07		= sc_ptr + 0x07;	cy_r00	= sc_ptr + 0x47;
		r08		= sc_ptr + 0x08;	cy_r02	= sc_ptr + 0x48;
		r09		= sc_ptr + 0x09;	cy_r04	= sc_ptr + 0x49;
		r0A		= sc_ptr + 0x0a;	cy_r06	= sc_ptr + 0x4a;
		r0B		= sc_ptr + 0x0b;	cy_r08	= sc_ptr + 0x4b;
		r0C		= sc_ptr + 0x0c;	cy_r0A	= sc_ptr + 0x4c;
		r0D		= sc_ptr + 0x0d;	cy_r0C	= sc_ptr + 0x4d;
		r0E		= sc_ptr + 0x0e;	cy_r0E	= sc_ptr + 0x4e;
		r0F		= sc_ptr + 0x0f;	cy_r10	= sc_ptr + 0x4f;
		r10		= sc_ptr + 0x10;	cy_r12	= sc_ptr + 0x50;
		r11		= sc_ptr + 0x11;	cy_r14	= sc_ptr + 0x51;
		r12		= sc_ptr + 0x12;	cy_r16	= sc_ptr + 0x52;
		r13		= sc_ptr + 0x13;	cy_r18	= sc_ptr + 0x53;
		r14		= sc_ptr + 0x14;	cy_r1A	= sc_ptr + 0x54;
		r15		= sc_ptr + 0x15;	cy_r1C	= sc_ptr + 0x55;
		r16		= sc_ptr + 0x16;	cy_r1E	= sc_ptr + 0x56;
		r17		= sc_ptr + 0x17;	cy_i00	= sc_ptr + 0x57;
		r18		= sc_ptr + 0x18;	cy_i02	= sc_ptr + 0x58;
		r19		= sc_ptr + 0x19;	cy_i04	= sc_ptr + 0x59;
		r1A		= sc_ptr + 0x1a;	cy_i06	= sc_ptr + 0x5a;
		r1B		= sc_ptr + 0x1b;	cy_i08	= sc_ptr + 0x5b;
		r1C		= sc_ptr + 0x1c;	cy_i0A	= sc_ptr + 0x5c;
		r1D		= sc_ptr + 0x1d;	cy_i0C	= sc_ptr + 0x5d;
		r1E		= sc_ptr + 0x1e;	cy_i0E	= sc_ptr + 0x5e;
		r1F		= sc_ptr + 0x1f;	cy_i10	= sc_ptr + 0x5f;
		r20		= sc_ptr + 0x20;	cy_i12	= sc_ptr + 0x60;
		r21		= sc_ptr + 0x21;	cy_i14	= sc_ptr + 0x61;
		r22		= sc_ptr + 0x22;	cy_i16	= sc_ptr + 0x62;
		r23		= sc_ptr + 0x23;	cy_i18	= sc_ptr + 0x63;
		r24		= sc_ptr + 0x24;	cy_i1A	= sc_ptr + 0x64;
		r25		= sc_ptr + 0x25;	cy_i1C	= sc_ptr + 0x65;
		r26		= sc_ptr + 0x26;	cy_i1E	= sc_ptr + 0x66;
		r27		= sc_ptr + 0x27;	max_err = sc_ptr + 0x67;
		r28		= sc_ptr + 0x28;	sse2_rnd= sc_ptr + 0x68;
		r29		= sc_ptr + 0x29;	half_arr= sc_ptr + 0x69;	/* This table needs 20x16 bytes */
		r2A		= sc_ptr + 0x2a;
		r2B		= sc_ptr + 0x2b;
		r2C		= sc_ptr + 0x2c;
		r2D		= sc_ptr + 0x2d;
		r2E		= sc_ptr + 0x2e;
		r2F		= sc_ptr + 0x2f;
		r30		= sc_ptr + 0x30;
		r31		= sc_ptr + 0x31;
		r32		= sc_ptr + 0x32;
		r33		= sc_ptr + 0x33;
		r34		= sc_ptr + 0x34;
		r35		= sc_ptr + 0x35;
		r36		= sc_ptr + 0x36;
		r37		= sc_ptr + 0x37;
		r38		= sc_ptr + 0x38;
		r39		= sc_ptr + 0x39;
		r3A		= sc_ptr + 0x3a;
		r3B		= sc_ptr + 0x3b;
		r3C		= sc_ptr + 0x3c;
		r3D		= sc_ptr + 0x3d;
		r3E		= sc_ptr + 0x3e;
		r3F		= sc_ptr + 0x3f;

		/* These remain fixed: */
		isrt2->re = isrt2->im = ISRT2;
		cc0  ->re = cc0  ->im = c	;		ss0  ->re = ss0  ->im = s	;
		cc1  ->re = cc1  ->im = c32_1;		ss1  ->re = ss1  ->im = s32_1;
		cc3  ->re = cc3  ->im = c32_3;		ss3  ->re = ss3  ->im = s32_3;

		/* SSE2 math = 53-mantissa-bit IEEE double-float: */
		sse2_rnd->re = sse2_rnd->im = crnd;

		/* SSE2 version of the one_half array - we have a 2-bit lookup, low bit is from the low word of the carry pair,
		high bit from the high, i.e. based on this lookup index [listed with LSB at right], we have:

			index	half_lo	half_hi
			00		1.0		1.0
			01		.50		1.0
			10		1.0		.50
			11		.50		.50

		The inverse-weights computation uses a similar table, but with all entries multiplied by .50:

			index2	half_lo	half_hi
			00		.50		.50
			01		.25		.50
			10		.50		.25
			11		.25		.25

		We do similarly for the base[] and baseinv[] table lookups - each of these get 4 further slots in half_arr.
		We also allocate a further 4 16-byte slots [uninitialized] for storage of the wtl,wtn,wtlp1,wtnm1 locals.
		*/
		tmp = half_arr;

	if(TRANSFORM_TYPE == RIGHT_ANGLE)
	{
		/* In Fermat-mod mode, use first 2 128-bit slots for base and 1/base: */
		tmp->re = base   [0];	tmp->im = base   [0];	++tmp;
		tmp->re = baseinv[0];	tmp->im = baseinv[0];	++tmp;
		/* [+2] slot is for [scale,scale] */
	}
	else
	{
		/* Forward-weight multipliers: */
		tmp->re = 1.0;	tmp->im = 1.0;	++tmp;
		tmp->re = .50;	tmp->im = 1.0;	++tmp;
		tmp->re = 1.0;	tmp->im = .50;	++tmp;
		tmp->re = .50;	tmp->im = .50;	++tmp;
		/* Inverse-weight multipliers: */
		tmp->re = .50;	tmp->im = .50;	++tmp;
		tmp->re = .25;	tmp->im = .50;	++tmp;
		tmp->re = .50;	tmp->im = .25;	++tmp;
		tmp->re = .25;	tmp->im = .25;	++tmp;
		/* Forward-base[] multipliers: */
		tmp->re = base   [0];	tmp->im = base   [0];	++tmp;
		tmp->re = base   [1];	tmp->im = base   [0];	++tmp;
		tmp->re = base   [0];	tmp->im = base   [1];	++tmp;
		tmp->re = base   [1];	tmp->im = base   [1];	++tmp;
		/* Inverse-base[] multipliers: */
		tmp->re = baseinv[0];	tmp->im = baseinv[0];	++tmp;
		tmp->re = baseinv[1];	tmp->im = baseinv[0];	++tmp;
		tmp->re = baseinv[0];	tmp->im = baseinv[1];	++tmp;
		tmp->re = baseinv[1];	tmp->im = baseinv[1];	++tmp;
	}

		/* Floating-point sign mask used for FABS on packed doubles: */
		sign_mask = sm_ptr;
		*sign_mask++ = (uint64)0x7FFFFFFFFFFFFFFFull;
		*sign_mask-- = (uint64)0x7FFFFFFFFFFFFFFFull;

		/* Test it: */
		tmp  = half_arr + 16;	/* ptr to local SSE2-floating-point storage */
		tmp->re = -ISRT2;	tmp->im = -ISRT2;

	#if 0
		__asm	mov	eax, tmp
		__asm	mov	ebx, sign_mask
		__asm	movaps	xmm0,[eax]
		__asm	andpd	xmm0,[ebx]
		__asm	movaps	[eax],xmm0

		ASSERT(HERE, tmp->re == ISRT2, "sign_mask0");
		ASSERT(HERE, tmp->im == ISRT2, "sign_mask1");

		// Set up the quadrupled-32-bit-int SSE constants used by the carry macros:
		sse_bw  = sm_ptr + 2;
		__asm	mov	eax, bw
		__asm	mov	ebx, sse_bw
		__asm	movd	xmm0,eax	/* Move actual *value* of reg eax into low 32 bits of xmm0 */
		__asm	pshufd	xmm0,xmm0,0	/* Broadcast low 32 bits of xmm0 to all 4 slots of xmm0 */
		__asm	movaps	[ebx],xmm0

		sse_sw  = sm_ptr + 4;
		__asm	lea	eax, sw
		__asm	mov	ebx, sse_sw
		__asm	movd	xmm0,[eax]	/* Variant 2: Move contents of address pointed to by reg eax into low 32 bits of xmm0 */
		__asm	pshufd	xmm0,xmm0,0	/* Broadcast low 32 bits of xmm0 to all 4 slots of xmm0 */
		__asm	movaps	[ebx],xmm0

		sse_nm1 = sm_ptr + 6;
		__asm	lea	eax, nm1
		__asm	mov	ebx, sse_nm1
		__asm	movd	xmm0,[eax]
		__asm	pshufd	xmm0,xmm0,0	/* Broadcast low 32 bits of xmm0 to all 4 slots of xmm0 */
		__asm	movaps	[ebx],xmm0
	#else
		sse_bw  = sm_ptr + 2;
		tmp64 = (uint64)bw;
		tmp64 = tmp64 + (tmp64 << 32);
		*sse_bw++ = tmp64;
		*sse_bw-- = tmp64;

		sse_sw  = sm_ptr + 4;
		tmp64 = (uint64)sw;
		tmp64 = tmp64 + (tmp64 << 32);
		*sse_sw++ = tmp64;
		*sse_sw-- = tmp64;

		sse_nm1 = sm_ptr + 6;
		tmp64 = (uint64)nm1;
		tmp64 = tmp64 + (tmp64 << 32);
		*sse_nm1++ = tmp64;
		*sse_nm1-- = tmp64;
	#endif

#ifdef USE_PTHREAD
	/* Populate the elements of the thread-specific data structs which don't change after init: */
	for(ithread = 0; ithread < CY_THREADS; ithread++)
	{
	// int data:
		tdat[ithread].tid = ithread;
		tdat[ithread].ndivr = NDIVR;
	
		tdat[ithread].sw  = sw;
		tdat[ithread].nwt = nwt;

	// pointer data:
		tdat[ithread].arrdat = a;			/* Main data array */
		tdat[ithread].wt0 = wt0;
		tdat[ithread].wt1 = wt1;
		tdat[ithread].si  = si;
		tdat[ithread].rn0 = rn0;
		tdat[ithread].rn1 = rn1;
		tdat[ithread].r00 = __r0 + ithread*cslots_in_local_store;
	}
#endif

		bjmodn00 = (int*)(sm_ptr + 8);
		bjmodn01 = bjmodn00 + 0x01;
		bjmodn02 = bjmodn00 + 0x02;
		bjmodn03 = bjmodn00 + 0x03;
		bjmodn04 = bjmodn00 + 0x04;
		bjmodn05 = bjmodn00 + 0x05;
		bjmodn06 = bjmodn00 + 0x06;
		bjmodn07 = bjmodn00 + 0x07;
		bjmodn08 = bjmodn00 + 0x08;
		bjmodn09 = bjmodn00 + 0x09;
		bjmodn0A = bjmodn00 + 0x0A;
		bjmodn0B = bjmodn00 + 0x0B;
		bjmodn0C = bjmodn00 + 0x0C;
		bjmodn0D = bjmodn00 + 0x0D;
		bjmodn0E = bjmodn00 + 0x0E;
		bjmodn0F = bjmodn00 + 0x0F;
		bjmodn10 = bjmodn00 + 0x10;
		bjmodn11 = bjmodn00 + 0x11;
		bjmodn12 = bjmodn00 + 0x12;
		bjmodn13 = bjmodn00 + 0x13;
		bjmodn14 = bjmodn00 + 0x14;
		bjmodn15 = bjmodn00 + 0x15;
		bjmodn16 = bjmodn00 + 0x16;
		bjmodn17 = bjmodn00 + 0x17;
		bjmodn18 = bjmodn00 + 0x18;
		bjmodn19 = bjmodn00 + 0x19;
		bjmodn1A = bjmodn00 + 0x1A;
		bjmodn1B = bjmodn00 + 0x1B;
		bjmodn1C = bjmodn00 + 0x1C;
		bjmodn1D = bjmodn00 + 0x1D;
		bjmodn1E = bjmodn00 + 0x1E;
		bjmodn1F = bjmodn00 + 0x1F;

	  #ifdef USE_PTHREAD
		r00 = __r0 + cslots_in_local_store;
		/* Init thread 1-CY_THREADS's local stores and pointers: */
		for(i = 1; i < CY_THREADS; ++i) {
			/* Only care about the constants for each thread here, but easier to just copy the entire thread0 local store: */
			memcpy(r00, __r0, cslots_in_local_store<<4);	// bytewise copy treats complex and uint64 subdata the same
			r00 += cslots_in_local_store;
		}
	  #endif

	#endif	// USE_SSE2

		/*   constant index offsets for load/stores are here.	*/
		pini = NDIVR/CY_THREADS;
		pini += ( (pini >> DAT_BITS) << PAD_BITS );
		p01 = NDIVR;
		p02 = p01 +p01;
		p03 = p02 +p01;
		p04 = p03 +p01;
		p05 = p04 +p01;
		p06 = p05 +p01;
		p07 = p06 +p01;
		p08 = p04 +p04;
		p0C = p08 +p04;
		p10 = p0C +p04;
		p14 = p10 +p04;
		p18 = p14 +p04;
		p1C = p18 +p04;

		p01 = p01 + ( (p01 >> DAT_BITS) << PAD_BITS );
		p02 = p02 + ( (p02 >> DAT_BITS) << PAD_BITS );
		p03 = p03 + ( (p03 >> DAT_BITS) << PAD_BITS );
		p04 = p04 + ( (p04 >> DAT_BITS) << PAD_BITS );
		p05 = p05 + ( (p05 >> DAT_BITS) << PAD_BITS );
		p06 = p06 + ( (p06 >> DAT_BITS) << PAD_BITS );
		p07 = p07 + ( (p07 >> DAT_BITS) << PAD_BITS );
		p08 = p08 + ( (p08 >> DAT_BITS) << PAD_BITS );
		p0C = p0C + ( (p0C >> DAT_BITS) << PAD_BITS );
		p10 = p10 + ( (p10 >> DAT_BITS) << PAD_BITS );
		p14 = p14 + ( (p14 >> DAT_BITS) << PAD_BITS );
		p18 = p18 + ( (p18 >> DAT_BITS) << PAD_BITS );
		p1C = p1C + ( (p1C >> DAT_BITS) << PAD_BITS );

		ASSERT(HERE, p01+p01 == p02, "p01+p01 != p02");
		ASSERT(HERE, p02+p02 == p04, "p02+p02 != p04");
		ASSERT(HERE, p04+p04 == p08, "p04+p04 != p08");
		ASSERT(HERE, p08+p04 == p0C, "p08+p04 != p0C");
		ASSERT(HERE, p0C+p04 == p10, "p0C+p04 != p10");
		ASSERT(HERE, p10+p04 == p14, "p10+p04 != p14");
		ASSERT(HERE, p14+p04 == p18, "p14+p04 != p18");
		ASSERT(HERE, p18+p04 == p1C, "p18+p04 != p1C");

		if(_cy_r00)	/* If it's a new exponent of a range test, need to deallocate these. */
		{
			free((void *)_i     ); _i      = 0x0;

			free((void *)_bjmodn00); _bjmodn00 = 0x0;
			free((void *)_bjmodn01); _bjmodn01 = 0x0;
			free((void *)_bjmodn02); _bjmodn02 = 0x0;
			free((void *)_bjmodn03); _bjmodn03 = 0x0;
			free((void *)_bjmodn04); _bjmodn04 = 0x0;
			free((void *)_bjmodn05); _bjmodn05 = 0x0;
			free((void *)_bjmodn06); _bjmodn06 = 0x0;
			free((void *)_bjmodn07); _bjmodn07 = 0x0;
			free((void *)_bjmodn08); _bjmodn08 = 0x0;
			free((void *)_bjmodn09); _bjmodn09 = 0x0;
			free((void *)_bjmodn0A); _bjmodn0A = 0x0;
			free((void *)_bjmodn0B); _bjmodn0B = 0x0;
			free((void *)_bjmodn0C); _bjmodn0C = 0x0;
			free((void *)_bjmodn0D); _bjmodn0D = 0x0;
			free((void *)_bjmodn0E); _bjmodn0E = 0x0;
			free((void *)_bjmodn0F); _bjmodn0F = 0x0;
			free((void *)_bjmodn10); _bjmodn10 = 0x0;
			free((void *)_bjmodn11); _bjmodn11 = 0x0;
			free((void *)_bjmodn12); _bjmodn12 = 0x0;
			free((void *)_bjmodn13); _bjmodn13 = 0x0;
			free((void *)_bjmodn14); _bjmodn14 = 0x0;
			free((void *)_bjmodn15); _bjmodn15 = 0x0;
			free((void *)_bjmodn16); _bjmodn16 = 0x0;
			free((void *)_bjmodn17); _bjmodn17 = 0x0;
			free((void *)_bjmodn18); _bjmodn18 = 0x0;
			free((void *)_bjmodn19); _bjmodn19 = 0x0;
			free((void *)_bjmodn1A); _bjmodn1A = 0x0;
			free((void *)_bjmodn1B); _bjmodn1B = 0x0;
			free((void *)_bjmodn1C); _bjmodn1C = 0x0;
			free((void *)_bjmodn1D); _bjmodn1D = 0x0;
			free((void *)_bjmodn1E); _bjmodn1E = 0x0;
			free((void *)_bjmodn1F); _bjmodn1F = 0x0;

			free((void *)_cy_r00); _cy_r00 = 0x0;	free((void *)_cy_i00); _cy_i00 = 0x0;
			free((void *)_cy_r01); _cy_r01 = 0x0;	free((void *)_cy_i01); _cy_i01 = 0x0;
			free((void *)_cy_r02); _cy_r02 = 0x0;	free((void *)_cy_i02); _cy_i02 = 0x0;
			free((void *)_cy_r03); _cy_r03 = 0x0;	free((void *)_cy_i03); _cy_i03 = 0x0;
			free((void *)_cy_r04); _cy_r04 = 0x0;	free((void *)_cy_i04); _cy_i04 = 0x0;
			free((void *)_cy_r05); _cy_r05 = 0x0;	free((void *)_cy_i05); _cy_i05 = 0x0;
			free((void *)_cy_r06); _cy_r06 = 0x0;	free((void *)_cy_i06); _cy_i06 = 0x0;
			free((void *)_cy_r07); _cy_r07 = 0x0;	free((void *)_cy_i07); _cy_i07 = 0x0;
			free((void *)_cy_r08); _cy_r08 = 0x0;	free((void *)_cy_i08); _cy_i08 = 0x0;
			free((void *)_cy_r09); _cy_r09 = 0x0;	free((void *)_cy_i09); _cy_i09 = 0x0;
			free((void *)_cy_r0A); _cy_r0A = 0x0;	free((void *)_cy_i0A); _cy_i0A = 0x0;
			free((void *)_cy_r0B); _cy_r0B = 0x0;	free((void *)_cy_i0B); _cy_i0B = 0x0;
			free((void *)_cy_r0C); _cy_r0C = 0x0;	free((void *)_cy_i0C); _cy_i0C = 0x0;
			free((void *)_cy_r0D); _cy_r0D = 0x0;	free((void *)_cy_i0D); _cy_i0D = 0x0;
			free((void *)_cy_r0E); _cy_r0E = 0x0;	free((void *)_cy_i0E); _cy_i0E = 0x0;
			free((void *)_cy_r0F); _cy_r0F = 0x0;	free((void *)_cy_i0F); _cy_i0F = 0x0;
			free((void *)_cy_r10); _cy_r10 = 0x0;	free((void *)_cy_i10); _cy_i10 = 0x0;
			free((void *)_cy_r11); _cy_r11 = 0x0;	free((void *)_cy_i11); _cy_i11 = 0x0;
			free((void *)_cy_r12); _cy_r12 = 0x0;	free((void *)_cy_i12); _cy_i12 = 0x0;
			free((void *)_cy_r13); _cy_r13 = 0x0;	free((void *)_cy_i13); _cy_i13 = 0x0;
			free((void *)_cy_r14); _cy_r14 = 0x0;	free((void *)_cy_i14); _cy_i14 = 0x0;
			free((void *)_cy_r15); _cy_r15 = 0x0;	free((void *)_cy_i15); _cy_i15 = 0x0;
			free((void *)_cy_r16); _cy_r16 = 0x0;	free((void *)_cy_i16); _cy_i16 = 0x0;
			free((void *)_cy_r17); _cy_r17 = 0x0;	free((void *)_cy_i17); _cy_i17 = 0x0;
			free((void *)_cy_r18); _cy_r18 = 0x0;	free((void *)_cy_i18); _cy_i18 = 0x0;
			free((void *)_cy_r19); _cy_r19 = 0x0;	free((void *)_cy_i19); _cy_i19 = 0x0;
			free((void *)_cy_r1A); _cy_r1A = 0x0;	free((void *)_cy_i1A); _cy_i1A = 0x0;
			free((void *)_cy_r1B); _cy_r1B = 0x0;	free((void *)_cy_i1B); _cy_i1B = 0x0;
			free((void *)_cy_r1C); _cy_r1C = 0x0;	free((void *)_cy_i1C); _cy_i1C = 0x0;
			free((void *)_cy_r1D); _cy_r1D = 0x0;	free((void *)_cy_i1D); _cy_i1D = 0x0;
			free((void *)_cy_r1E); _cy_r1E = 0x0;	free((void *)_cy_i1E); _cy_i1E = 0x0;
			free((void *)_cy_r1F); _cy_r1F = 0x0;	free((void *)_cy_i1F); _cy_i1F = 0x0;

			free((void *)_jstart ); _jstart  = 0x0;
			free((void *)_jhi    ); _jhi     = 0x0;
			free((void *)_maxerr); _maxerr = 0x0;
			free((void *)_col   ); _col    = 0x0;
			free((void *)_co2   ); _co2    = 0x0;
			free((void *)_co3   ); _co3    = 0x0;

			free((void *)_bjmodnini); _bjmodnini = 0x0;
		}

		ptr_prod = (uint32)0;	/* Store bitmask for allocatable-array ptrs here, check vs 0 after all alloc calls finish */
		j = CY_THREADS*sizeof(int);
		_i       	= (int *)malloc(j);	ptr_prod += (uint32)(_i== 0x0);
		_bjmodn00	= (int *)malloc(j);	ptr_prod += (uint32)(_bjmodn00== 0x0);
		_bjmodn01	= (int *)malloc(j);	ptr_prod += (uint32)(_bjmodn01== 0x0);
		_bjmodn02	= (int *)malloc(j);	ptr_prod += (uint32)(_bjmodn02== 0x0);
		_bjmodn03	= (int *)malloc(j);	ptr_prod += (uint32)(_bjmodn03== 0x0);
		_bjmodn04	= (int *)malloc(j);	ptr_prod += (uint32)(_bjmodn04== 0x0);
		_bjmodn05	= (int *)malloc(j);	ptr_prod += (uint32)(_bjmodn05== 0x0);
		_bjmodn06	= (int *)malloc(j);	ptr_prod += (uint32)(_bjmodn06== 0x0);
		_bjmodn07	= (int *)malloc(j);	ptr_prod += (uint32)(_bjmodn07== 0x0);
		_bjmodn08	= (int *)malloc(j);	ptr_prod += (uint32)(_bjmodn08== 0x0);
		_bjmodn09	= (int *)malloc(j);	ptr_prod += (uint32)(_bjmodn09== 0x0);
		_bjmodn0A	= (int *)malloc(j);	ptr_prod += (uint32)(_bjmodn0A== 0x0);
		_bjmodn0B	= (int *)malloc(j);	ptr_prod += (uint32)(_bjmodn0B== 0x0);
		_bjmodn0C	= (int *)malloc(j);	ptr_prod += (uint32)(_bjmodn0C== 0x0);
		_bjmodn0D	= (int *)malloc(j);	ptr_prod += (uint32)(_bjmodn0D== 0x0);
		_bjmodn0E	= (int *)malloc(j);	ptr_prod += (uint32)(_bjmodn0E== 0x0);
		_bjmodn0F	= (int *)malloc(j);	ptr_prod += (uint32)(_bjmodn0F== 0x0);
		_bjmodn10	= (int *)malloc(j);	ptr_prod += (uint32)(_bjmodn10== 0x0);
		_bjmodn11	= (int *)malloc(j);	ptr_prod += (uint32)(_bjmodn11== 0x0);
		_bjmodn12	= (int *)malloc(j);	ptr_prod += (uint32)(_bjmodn12== 0x0);
		_bjmodn13	= (int *)malloc(j);	ptr_prod += (uint32)(_bjmodn13== 0x0);
		_bjmodn14	= (int *)malloc(j);	ptr_prod += (uint32)(_bjmodn14== 0x0);
		_bjmodn15	= (int *)malloc(j);	ptr_prod += (uint32)(_bjmodn15== 0x0);
		_bjmodn16	= (int *)malloc(j);	ptr_prod += (uint32)(_bjmodn16== 0x0);
		_bjmodn17	= (int *)malloc(j);	ptr_prod += (uint32)(_bjmodn17== 0x0);
		_bjmodn18	= (int *)malloc(j);	ptr_prod += (uint32)(_bjmodn18== 0x0);
		_bjmodn19	= (int *)malloc(j);	ptr_prod += (uint32)(_bjmodn19== 0x0);
		_bjmodn1A	= (int *)malloc(j);	ptr_prod += (uint32)(_bjmodn1A== 0x0);
		_bjmodn1B	= (int *)malloc(j);	ptr_prod += (uint32)(_bjmodn1B== 0x0);
		_bjmodn1C	= (int *)malloc(j);	ptr_prod += (uint32)(_bjmodn1C== 0x0);
		_bjmodn1D	= (int *)malloc(j);	ptr_prod += (uint32)(_bjmodn1D== 0x0);
		_bjmodn1E	= (int *)malloc(j);	ptr_prod += (uint32)(_bjmodn1E== 0x0);
		_bjmodn1F	= (int *)malloc(j);	ptr_prod += (uint32)(_bjmodn1F== 0x0);
		_jstart  	= (int *)malloc(j);	ptr_prod += (uint32)(_jstart  == 0x0);
		_jhi     	= (int *)malloc(j);	ptr_prod += (uint32)(_jhi     == 0x0);
		_col     	= (int *)malloc(j);	ptr_prod += (uint32)(_col     == 0x0);
		_co2     	= (int *)malloc(j);	ptr_prod += (uint32)(_co2     == 0x0);
		_co3     	= (int *)malloc(j);	ptr_prod += (uint32)(_co3     == 0x0);

		j = CY_THREADS*sizeof(double);
		_cy_r00	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_r00== 0x0);
		_cy_r01	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_r01== 0x0);
		_cy_r02	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_r02== 0x0);
		_cy_r03	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_r03== 0x0);
		_cy_r04	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_r04== 0x0);
		_cy_r05	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_r05== 0x0);
		_cy_r06	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_r06== 0x0);
		_cy_r07	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_r07== 0x0);
		_cy_r08	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_r08== 0x0);
		_cy_r09	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_r09== 0x0);
		_cy_r0A	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_r0A== 0x0);
		_cy_r0B	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_r0B== 0x0);
		_cy_r0C	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_r0C== 0x0);
		_cy_r0D	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_r0D== 0x0);
		_cy_r0E	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_r0E== 0x0);
		_cy_r0F	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_r0F== 0x0);
		_cy_r10	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_r10== 0x0);
		_cy_r11	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_r11== 0x0);
		_cy_r12	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_r12== 0x0);
		_cy_r13	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_r13== 0x0);
		_cy_r14	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_r14== 0x0);
		_cy_r15	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_r15== 0x0);
		_cy_r16	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_r16== 0x0);
		_cy_r17	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_r17== 0x0);
		_cy_r18	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_r18== 0x0);
		_cy_r19	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_r19== 0x0);
		_cy_r1A	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_r1A== 0x0);
		_cy_r1B	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_r1B== 0x0);
		_cy_r1C	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_r1C== 0x0);
		_cy_r1D	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_r1D== 0x0);
		_cy_r1E	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_r1E== 0x0);
		_cy_r1F	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_r1F== 0x0);

		_cy_i00	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_i00== 0x0);
		_cy_i01	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_i01== 0x0);
		_cy_i02	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_i02== 0x0);
		_cy_i03	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_i03== 0x0);
		_cy_i04	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_i04== 0x0);
		_cy_i05	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_i05== 0x0);
		_cy_i06	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_i06== 0x0);
		_cy_i07	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_i07== 0x0);
		_cy_i08	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_i08== 0x0);
		_cy_i09	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_i09== 0x0);
		_cy_i0A	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_i0A== 0x0);
		_cy_i0B	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_i0B== 0x0);
		_cy_i0C	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_i0C== 0x0);
		_cy_i0D	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_i0D== 0x0);
		_cy_i0E	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_i0E== 0x0);
		_cy_i0F	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_i0F== 0x0);
		_cy_i10	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_i10== 0x0);
		_cy_i11	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_i11== 0x0);
		_cy_i12	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_i12== 0x0);
		_cy_i13	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_i13== 0x0);
		_cy_i14	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_i14== 0x0);
		_cy_i15	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_i15== 0x0);
		_cy_i16	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_i16== 0x0);
		_cy_i17	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_i17== 0x0);
		_cy_i18	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_i18== 0x0);
		_cy_i19	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_i19== 0x0);
		_cy_i1A	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_i1A== 0x0);
		_cy_i1B	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_i1B== 0x0);
		_cy_i1C	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_i1C== 0x0);
		_cy_i1D	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_i1D== 0x0);
		_cy_i1E	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_i1E== 0x0);
		_cy_i1F	= (double *)malloc(j);	ptr_prod += (uint32)(_cy_i1F== 0x0);

		_maxerr	= (double *)malloc(j);	ptr_prod += (uint32)(_maxerr== 0x0);

		ASSERT(HERE, ptr_prod == 0, "FATAL: unable to allocate one or more auxiliary arrays in radix32_ditN_cy_dif1.");

		if(MODULUS_TYPE == MODULUS_TYPE_MERSENNE)
		{
			/* Create (THREADS + 1) copies of _bjmodnini and use the extra (uppermost) one to store the "master" increment,
			i.e. the one that n2/32-separated FFT outputs need:
			*/
			_bjmodnini = (int *)malloc((CY_THREADS + 1)*sizeof(int));	if(!_bjmodnini){ sprintf(cbuf,"FATAL: unable to allocate array _bjmodnini in radix32_ditN_cy_dif1.\n"); fprintf(stderr,"%s", cbuf);	ASSERT(HERE, 0,cbuf); }
			_bjmodnini[0] = 0;
			_bjmodnini[1] = 0;
			for(j=0; j < NDIVR/CY_THREADS; j++)
			{
				_bjmodnini[1] -= sw; _bjmodnini[1] = _bjmodnini[1] + ( (-(int)((uint32)_bjmodnini[1] >> 31)) & n);
			}

			if(CY_THREADS > 1)
			{
				for(ithread = 2; ithread <= CY_THREADS; ithread++)
				{
					_bjmodnini[ithread] = _bjmodnini[ithread-1] + _bjmodnini[1] - n; _bjmodnini[ithread] = _bjmodnini[ithread] + ( (-(int)((uint32)_bjmodnini[ithread] >> 31)) & n);
				}
			}
			/* Check upper element against scalar value, as precomputed in single-thread mode: */
			bjmodnini=0;
			for(j=0; j < NDIVR; j++)
			{
				bjmodnini -= sw; bjmodnini = bjmodnini + ( (-(int)((uint32)bjmodnini >> 31)) & n);
			}
			ASSERT(HERE, _bjmodnini[CY_THREADS] == bjmodnini,"_bjmodnini[CY_THREADS] != bjmodnini");
		}
	}	/* endif(first_entry) */

#if FFT_DEBUG
	dbg_fname[0] += (char)(CY_THREADS - 1);	// 1-thread = "a.txt", 2-thread = "b.txt", etc.
	dbg_file = fopen(dbg_fname, "w");
	ASSERT(HERE, dbg_file != 0x0, "Unable to open dbg_file!");
	fprintf(dbg_file, "radix28_ditN_cy_dif1 DEBUG: fftlen = %d\n", n);
	fprintf(dbg_file,"CY_THREADS = %d\n", CY_THREADS);
	// Use RNG to populate data array:
	rng_isaac_init(TRUE);
	double rt = 1024.0*1024.0*1024.0*1024.0;
	for(j = 0; j < n; j++) {
		j1 = j + ( (j >> DAT_BITS) << PAD_BITS );	/* padded-array fetch index is here */
		a[j1  ] = rt*rng_isaac_rand_double_norm_pm1();
		a[j1+1] = rt*rng_isaac_rand_double_norm_pm1();
	}
#endif

/*...The radix-32 final DIT pass is here.	*/

	/* init carries	*/
	for(ithread = 0; ithread < CY_THREADS; ithread++)
	{
		_cy_r00[ithread] = 0;	_cy_i00[ithread] = 0;
		_cy_r01[ithread] = 0;	_cy_i01[ithread] = 0;
		_cy_r02[ithread] = 0;	_cy_i02[ithread] = 0;
		_cy_r03[ithread] = 0;	_cy_i03[ithread] = 0;
		_cy_r04[ithread] = 0;	_cy_i04[ithread] = 0;
		_cy_r05[ithread] = 0;	_cy_i05[ithread] = 0;
		_cy_r06[ithread] = 0;	_cy_i06[ithread] = 0;
		_cy_r07[ithread] = 0;	_cy_i07[ithread] = 0;
		_cy_r08[ithread] = 0;	_cy_i08[ithread] = 0;
		_cy_r09[ithread] = 0;	_cy_i09[ithread] = 0;
		_cy_r0A[ithread] = 0;	_cy_i0A[ithread] = 0;
		_cy_r0B[ithread] = 0;	_cy_i0B[ithread] = 0;
		_cy_r0C[ithread] = 0;	_cy_i0C[ithread] = 0;
		_cy_r0D[ithread] = 0;	_cy_i0D[ithread] = 0;
		_cy_r0E[ithread] = 0;	_cy_i0E[ithread] = 0;
		_cy_r0F[ithread] = 0;	_cy_i0F[ithread] = 0;
		_cy_r10[ithread] = 0;	_cy_i10[ithread] = 0;
		_cy_r11[ithread] = 0;	_cy_i11[ithread] = 0;
		_cy_r12[ithread] = 0;	_cy_i12[ithread] = 0;
		_cy_r13[ithread] = 0;	_cy_i13[ithread] = 0;
		_cy_r14[ithread] = 0;	_cy_i14[ithread] = 0;
		_cy_r15[ithread] = 0;	_cy_i15[ithread] = 0;
		_cy_r16[ithread] = 0;	_cy_i16[ithread] = 0;
		_cy_r17[ithread] = 0;	_cy_i17[ithread] = 0;
		_cy_r18[ithread] = 0;	_cy_i18[ithread] = 0;
		_cy_r19[ithread] = 0;	_cy_i19[ithread] = 0;
		_cy_r1A[ithread] = 0;	_cy_i1A[ithread] = 0;
		_cy_r1B[ithread] = 0;	_cy_i1B[ithread] = 0;
		_cy_r1C[ithread] = 0;	_cy_i1C[ithread] = 0;
		_cy_r1D[ithread] = 0;	_cy_i1D[ithread] = 0;
		_cy_r1E[ithread] = 0;	_cy_i1E[ithread] = 0;
		_cy_r1F[ithread] = 0;	_cy_i1F[ithread] = 0;

		_maxerr[ithread] = 0.0;
	}
	/* If an LL test, init the subtract-2: */
	if(MODULUS_TYPE == MODULUS_TYPE_MERSENNE && TEST_TYPE == TEST_TYPE_PRIMALITY)
	{
		_cy_r00[0] = -2;
	}

	*fracmax=0;	/* init max. fractional error	*/
	full_pass = 1;	/* set = 1 for normal carry pass, = 0 for wrapper pass	*/
	scale = n2inv;	/* init inverse-weight scale factor  (set = 2/n for normal carry pass, = 1 for wrapper pass)	*/

for(outer=0; outer <= 1; outer++)
{
	if(MODULUS_TYPE == MODULUS_TYPE_MERSENNE)
	{
		/*
		Moved this inside the outer-loop, so on cleanup pass can use it to reset _col,_co2,_co3 starting values,
		then simply overwrite it with 1 prior to starting the k-loop.
		*/
		_i[0] = 1;		/* Pointer to the BASE and BASEINV arrays. lowest-order digit is always a bigword (_i[0] = 1).	*/
		for(ithread = 1; ithread < CY_THREADS; ithread++)
		{
			_i[ithread] = ((uint32)(sw - _bjmodnini[ithread]) >> 31);
		}

		// Include 0-thread here ... bjmodn terms all 0 for that, but need jhi computed for all threads
		j = _bjmodnini[CY_THREADS];
		khi = n_div_nwt/CY_THREADS;
		for(ithread = 0; ithread < CY_THREADS; ithread++)
		{
			_bjmodn00[ithread] = _bjmodnini[ithread];
			MOD_ADD32(_bjmodn00[ithread], j, n, _bjmodn01[ithread]);
			MOD_ADD32(_bjmodn01[ithread], j, n, _bjmodn02[ithread]);
			MOD_ADD32(_bjmodn02[ithread], j, n, _bjmodn03[ithread]);
			MOD_ADD32(_bjmodn03[ithread], j, n, _bjmodn04[ithread]);
			MOD_ADD32(_bjmodn04[ithread], j, n, _bjmodn05[ithread]);
			MOD_ADD32(_bjmodn05[ithread], j, n, _bjmodn06[ithread]);
			MOD_ADD32(_bjmodn06[ithread], j, n, _bjmodn07[ithread]);
			MOD_ADD32(_bjmodn07[ithread], j, n, _bjmodn08[ithread]);
			MOD_ADD32(_bjmodn08[ithread], j, n, _bjmodn09[ithread]);
			MOD_ADD32(_bjmodn09[ithread], j, n, _bjmodn0A[ithread]);
			MOD_ADD32(_bjmodn0A[ithread], j, n, _bjmodn0B[ithread]);
			MOD_ADD32(_bjmodn0B[ithread], j, n, _bjmodn0C[ithread]);
			MOD_ADD32(_bjmodn0C[ithread], j, n, _bjmodn0D[ithread]);
			MOD_ADD32(_bjmodn0D[ithread], j, n, _bjmodn0E[ithread]);
			MOD_ADD32(_bjmodn0E[ithread], j, n, _bjmodn0F[ithread]);
			MOD_ADD32(_bjmodn0F[ithread], j, n, _bjmodn10[ithread]);
			MOD_ADD32(_bjmodn10[ithread], j, n, _bjmodn11[ithread]);
			MOD_ADD32(_bjmodn11[ithread], j, n, _bjmodn12[ithread]);
			MOD_ADD32(_bjmodn12[ithread], j, n, _bjmodn13[ithread]);
			MOD_ADD32(_bjmodn13[ithread], j, n, _bjmodn14[ithread]);
			MOD_ADD32(_bjmodn14[ithread], j, n, _bjmodn15[ithread]);
			MOD_ADD32(_bjmodn15[ithread], j, n, _bjmodn16[ithread]);
			MOD_ADD32(_bjmodn16[ithread], j, n, _bjmodn17[ithread]);
			MOD_ADD32(_bjmodn17[ithread], j, n, _bjmodn18[ithread]);
			MOD_ADD32(_bjmodn18[ithread], j, n, _bjmodn19[ithread]);
			MOD_ADD32(_bjmodn19[ithread], j, n, _bjmodn1A[ithread]);
			MOD_ADD32(_bjmodn1A[ithread], j, n, _bjmodn1B[ithread]);
			MOD_ADD32(_bjmodn1B[ithread], j, n, _bjmodn1C[ithread]);
			MOD_ADD32(_bjmodn1C[ithread], j, n, _bjmodn1D[ithread]);
			MOD_ADD32(_bjmodn1D[ithread], j, n, _bjmodn1E[ithread]);
			MOD_ADD32(_bjmodn1E[ithread], j, n, _bjmodn1F[ithread]);

			_jstart[ithread] = ithread*NDIVR/CY_THREADS;
			if(!full_pass)
				_jhi[ithread] = _jstart[ithread] + 7;		/* Cleanup loop assumes carryins propagate at most 4 words up. */
			else
				_jhi[ithread] = _jstart[ithread] + nwt-1;

			_col[ithread] = ithread*(khi*RADIX);			/* col gets incremented by RADIX_VEC[0] on every pass through the k-loop */
			_co2[ithread] = (n>>nwt_bits)-1+RADIX - _col[ithread];	/* co2 gets decremented by RADIX_VEC[0] on every pass through the k-loop */
			_co3[ithread] = _co2[ithread]-RADIX;			/* At the start of each new j-loop, co3=co2-RADIX_VEC[0]	*/
		}
	}
	else
	{
		khi = 1;
		for(ithread = 0; ithread < CY_THREADS; ithread++)
		{
			_jstart[ithread] = ithread*NDIVR/CY_THREADS;
			/*
			For right-angle transform need *complex* elements for wraparound, so jhi needs to be twice as large
			*/
			if(!full_pass)
				_jhi[ithread] = _jstart[ithread] + 15;		/* Cleanup loop assumes carryins propagate at most 4 words up. */
			else
				_jhi[ithread] = _jstart[ithread] + n_div_nwt/CY_THREADS;
		}
	}

	/* Move this cleanup-pass-specific khi setting here, since need regular-pass khi value for above inits: */
	if(!full_pass)
	{
		khi = 1;
	#if FFT_DEBUG
		fprintf(dbg_file, "radix32_ditN_cy_dif1: Cleanup Pass:\n", n);
	#endif
	}

/* Needed to remove the prefetch-address vars add0 & add for this to compile properly: */
#ifdef USE_OMP
	omp_set_num_threads(CY_THREADS);
//#undef PFETCH
	#pragma omp parallel for (\
		temp,frac,maxerr,i,j,j1,jstart,jhi,k,k1,k2,l,col,co2,co3,m,m2,\
		n_minus_sil,n_minus_silp1,sinwt,sinwtm1,wtl,wtlp1,wtn,wtnm1,wt,wtinv,wtA,wtB,wtC,wt_re,wt_im,\
		rt,it,t00,t01,t02,t03,t04,t05,t06,t07,t08,t09,t0A,t0B,t0C,t0D,t0E,t0F,t10,t11,t12,t13,t14,t15,t16,t17,t18,t19,t1A,t1B,t1C,t1D,t1E,t1F,t20,t21,t22,t23,t24,t25,t26,t27,t28,t29,t2A,t2B,t2C,t2D,t2E,t2F,t30,t31,t32,t33,t34,t35,t36,t37,t38,t39,t3A,t3B,t3C,t3D,t3E,t3F,\
		a1p00r,a1p01r,a1p02r,a1p03r,a1p04r,a1p05r,a1p06r,a1p07r,a1p08r,a1p09r,a1p0Ar,a1p0Br,a1p0Cr,a1p0Dr,a1p0Er,a1p0Fr,a1p10r,a1p11r,a1p12r,a1p13r,a1p14r,a1p15r,a1p16r,a1p17r,a1p18r,a1p19r,a1p1Ar,a1p1Br,a1p1Cr,a1p1Dr,a1p1Er,a1p1Fr,\
		a1p00i,a1p01i,a1p02i,a1p03i,a1p04i,a1p05i,a1p06i,a1p07i,a1p08i,a1p09i,a1p0Ai,a1p0Bi,a1p0Ci,a1p0Di,a1p0Ei,a1p0Fi,a1p10i,a1p11i,a1p12i,a1p13i,a1p14i,a1p15i,a1p16i,a1p17i,a1p18i,a1p19i,a1p1Ai,a1p1Bi,a1p1Ci,a1p1Di,a1p1Ei,a1p1Fi,\
		bjmodn00,bjmodn01,bjmodn02,bjmodn03,bjmodn04,bjmodn05,bjmodn06,bjmodn07,bjmodn08,bjmodn09,bjmodn0A,bjmodn0B,bjmodn0C,bjmodn0D,bjmodn0E,bjmodn0F,bjmodn10,bjmodn11,bjmodn12,bjmodn13,bjmodn14,bjmodn15,bjmodn16,bjmodn17,bjmodn18,bjmodn19,bjmodn1A,bjmodn1B,bjmodn1C,bjmodn1D,bjmodn1E,bjmodn1F,\
		cy_r00,cy_r01,cy_r02,cy_r03,cy_r04,cy_r05,cy_r06,cy_r07,cy_r08,cy_r09,cy_r0A,cy_r0B,cy_r0C,cy_r0D,cy_r0E,cy_r0F,cy_r10,cy_r11,cy_r12,cy_r13,cy_r14,cy_r15,cy_r16,cy_r17,cy_r18,cy_r19,cy_r1A,cy_r1B,cy_r1C,cy_r1D,cy_r1E,cy_r1F,\
		cy_i00,cy_i01,cy_i02,cy_i03,cy_i04,cy_i05,cy_i06,cy_i07,cy_i08,cy_i09,cy_i0A,cy_i0B,cy_i0C,cy_i0D,cy_i0E,cy_i0F,cy_i10,cy_i11,cy_i12,cy_i13,cy_i14,cy_i15,cy_i16,cy_i17,cy_i18,cy_i19,cy_i1A,cy_i1B,cy_i1C,cy_i1D,cy_i1E,cy_i1F\
	) default(shared) schedule(static)
#endif

#ifdef USE_PTHREAD
	/* Populate the thread-specific data structs - use the invariant terms as memchecks: */
	for(ithread = 0; ithread < CY_THREADS; ithread++)
	{
	// int data:
		ASSERT(HERE, tdat[ithread].tid == ithread, "thread-local memcheck fail!");
		ASSERT(HERE, tdat[ithread].ndivr == NDIVR, "thread-local memcheck fail!");
	
		tdat[ithread].khi    = khi;
		tdat[ithread].i      = _i[ithread];	/* Pointer to the BASE and BASEINV arrays.	*/
		tdat[ithread].jstart = _jstart[ithread];
		tdat[ithread].jhi    = _jhi[ithread];

		tdat[ithread].col = _col[ithread];
		tdat[ithread].co2 = _co2[ithread];
		tdat[ithread].co3 = _co3[ithread];
		ASSERT(HERE, tdat[ithread].sw  == sw, "thread-local memcheck fail!");
		ASSERT(HERE, tdat[ithread].nwt == nwt, "thread-local memcheck fail!");

	// double data:
		tdat[ithread].maxerr = _maxerr[ithread];
		tdat[ithread].scale = scale;

	// pointer data:
		ASSERT(HERE, tdat[ithread].arrdat == a, "thread-local memcheck fail!");			/* Main data array */
		ASSERT(HERE, tdat[ithread].wt0 == wt0, "thread-local memcheck fail!");
		ASSERT(HERE, tdat[ithread].wt1 == wt1, "thread-local memcheck fail!");
		ASSERT(HERE, tdat[ithread].si  == si, "thread-local memcheck fail!");
		ASSERT(HERE, tdat[ithread].rn0 == rn0, "thread-local memcheck fail!");
		ASSERT(HERE, tdat[ithread].rn1 == rn1, "thread-local memcheck fail!");
		ASSERT(HERE, tdat[ithread].r00 == __r0 + ithread*cslots_in_local_store, "thread-local memcheck fail!");
		tmp = tdat[ithread].r00;
		ASSERT(HERE, ((tmp + 0x40)->re == ISRT2 && (tmp + 0x40)->im == ISRT2), "thread-local memcheck failed!");
		ASSERT(HERE, ((tmp + 0x68)->re == crnd && (tmp + 0x68)->im == crnd), "thread-local memcheck failed!");
	if(MODULUS_TYPE == MODULUS_TYPE_MERSENNE)
	{
		ASSERT(HERE, (tmp + 0x69+10)->re * (tmp + 0x69+14)->re == 1.0 && (tmp + 0x69+10)->im * (tmp + 0x69+14)->im == 1.0, "thread-local memcheck failed!");
	} else {
		ASSERT(HERE, (tmp + 0x69)->re * (tmp + 0x69+1)->re == 1.0 && (tmp + 0x69)->im * (tmp + 0x69+1)->im == 1.0, "thread-local memcheck failed!");
	}

		if(MODULUS_TYPE == MODULUS_TYPE_MERSENNE)
		{
			tdat[ithread].bjmodn00 = _bjmodn00[ithread];
			tdat[ithread].bjmodn01 = _bjmodn01[ithread];
			tdat[ithread].bjmodn02 = _bjmodn02[ithread];
			tdat[ithread].bjmodn03 = _bjmodn03[ithread];
			tdat[ithread].bjmodn04 = _bjmodn04[ithread];
			tdat[ithread].bjmodn05 = _bjmodn05[ithread];
			tdat[ithread].bjmodn06 = _bjmodn06[ithread];
			tdat[ithread].bjmodn07 = _bjmodn07[ithread];
			tdat[ithread].bjmodn08 = _bjmodn08[ithread];
			tdat[ithread].bjmodn09 = _bjmodn09[ithread];
			tdat[ithread].bjmodn0A = _bjmodn0A[ithread];
			tdat[ithread].bjmodn0B = _bjmodn0B[ithread];
			tdat[ithread].bjmodn0C = _bjmodn0C[ithread];
			tdat[ithread].bjmodn0D = _bjmodn0D[ithread];
			tdat[ithread].bjmodn0E = _bjmodn0E[ithread];
			tdat[ithread].bjmodn0F = _bjmodn0F[ithread];
			tdat[ithread].bjmodn10 = _bjmodn10[ithread];
			tdat[ithread].bjmodn11 = _bjmodn11[ithread];
			tdat[ithread].bjmodn12 = _bjmodn12[ithread];
			tdat[ithread].bjmodn13 = _bjmodn13[ithread];
			tdat[ithread].bjmodn14 = _bjmodn14[ithread];
			tdat[ithread].bjmodn15 = _bjmodn15[ithread];
			tdat[ithread].bjmodn16 = _bjmodn16[ithread];
			tdat[ithread].bjmodn17 = _bjmodn17[ithread];
			tdat[ithread].bjmodn18 = _bjmodn18[ithread];
			tdat[ithread].bjmodn19 = _bjmodn19[ithread];
			tdat[ithread].bjmodn1A = _bjmodn1A[ithread];
			tdat[ithread].bjmodn1B = _bjmodn1B[ithread];
			tdat[ithread].bjmodn1C = _bjmodn1C[ithread];
			tdat[ithread].bjmodn1D = _bjmodn1D[ithread];
			tdat[ithread].bjmodn1E = _bjmodn1E[ithread];
			tdat[ithread].bjmodn1F = _bjmodn1F[ithread];
			/* init carries	*/
			tdat[ithread].cy_r00 = _cy_r00[ithread];
			tdat[ithread].cy_r01 = _cy_r01[ithread];
			tdat[ithread].cy_r02 = _cy_r02[ithread];
			tdat[ithread].cy_r03 = _cy_r03[ithread];
			tdat[ithread].cy_r04 = _cy_r04[ithread];
			tdat[ithread].cy_r05 = _cy_r05[ithread];
			tdat[ithread].cy_r06 = _cy_r06[ithread];
			tdat[ithread].cy_r07 = _cy_r07[ithread];
			tdat[ithread].cy_r08 = _cy_r08[ithread];
			tdat[ithread].cy_r09 = _cy_r09[ithread];
			tdat[ithread].cy_r0A = _cy_r0A[ithread];
			tdat[ithread].cy_r0B = _cy_r0B[ithread];
			tdat[ithread].cy_r0C = _cy_r0C[ithread];
			tdat[ithread].cy_r0D = _cy_r0D[ithread];
			tdat[ithread].cy_r0E = _cy_r0E[ithread];
			tdat[ithread].cy_r0F = _cy_r0F[ithread];
			tdat[ithread].cy_r10 = _cy_r10[ithread];
			tdat[ithread].cy_r11 = _cy_r11[ithread];
			tdat[ithread].cy_r12 = _cy_r12[ithread];
			tdat[ithread].cy_r13 = _cy_r13[ithread];
			tdat[ithread].cy_r14 = _cy_r14[ithread];
			tdat[ithread].cy_r15 = _cy_r15[ithread];
			tdat[ithread].cy_r16 = _cy_r16[ithread];
			tdat[ithread].cy_r17 = _cy_r17[ithread];
			tdat[ithread].cy_r18 = _cy_r18[ithread];
			tdat[ithread].cy_r19 = _cy_r19[ithread];
			tdat[ithread].cy_r1A = _cy_r1A[ithread];
			tdat[ithread].cy_r1B = _cy_r1B[ithread];
			tdat[ithread].cy_r1C = _cy_r1C[ithread];
			tdat[ithread].cy_r1D = _cy_r1D[ithread];
			tdat[ithread].cy_r1E = _cy_r1E[ithread];
			tdat[ithread].cy_r1F = _cy_r1F[ithread];
		}
		else	/* Fermat-mod uses "double helix" carry scheme - 2 separate sets of real/imaginary carries for right-angle transform, plus "twisted" wraparound step. */
		{
			/* init carries	*/
			tdat[ithread].cy_r00 = _cy_r00[ithread];	tdat[ithread].cy_i00 = _cy_i00[ithread];
			tdat[ithread].cy_r01 = _cy_r01[ithread];	tdat[ithread].cy_i01 = _cy_i01[ithread];
			tdat[ithread].cy_r02 = _cy_r02[ithread];	tdat[ithread].cy_i02 = _cy_i02[ithread];
			tdat[ithread].cy_r03 = _cy_r03[ithread];	tdat[ithread].cy_i03 = _cy_i03[ithread];
			tdat[ithread].cy_r04 = _cy_r04[ithread];	tdat[ithread].cy_i04 = _cy_i04[ithread];
			tdat[ithread].cy_r05 = _cy_r05[ithread];	tdat[ithread].cy_i05 = _cy_i05[ithread];
			tdat[ithread].cy_r06 = _cy_r06[ithread];	tdat[ithread].cy_i06 = _cy_i06[ithread];
			tdat[ithread].cy_r07 = _cy_r07[ithread];	tdat[ithread].cy_i07 = _cy_i07[ithread];
			tdat[ithread].cy_r08 = _cy_r08[ithread];	tdat[ithread].cy_i08 = _cy_i08[ithread];
			tdat[ithread].cy_r09 = _cy_r09[ithread];	tdat[ithread].cy_i09 = _cy_i09[ithread];
			tdat[ithread].cy_r0A = _cy_r0A[ithread];	tdat[ithread].cy_i0A = _cy_i0A[ithread];
			tdat[ithread].cy_r0B = _cy_r0B[ithread];	tdat[ithread].cy_i0B = _cy_i0B[ithread];
			tdat[ithread].cy_r0C = _cy_r0C[ithread];	tdat[ithread].cy_i0C = _cy_i0C[ithread];
			tdat[ithread].cy_r0D = _cy_r0D[ithread];	tdat[ithread].cy_i0D = _cy_i0D[ithread];
			tdat[ithread].cy_r0E = _cy_r0E[ithread];	tdat[ithread].cy_i0E = _cy_i0E[ithread];
			tdat[ithread].cy_r0F = _cy_r0F[ithread];	tdat[ithread].cy_i0F = _cy_i0F[ithread];
			tdat[ithread].cy_r10 = _cy_r10[ithread];	tdat[ithread].cy_i10 = _cy_i10[ithread];
			tdat[ithread].cy_r11 = _cy_r11[ithread];	tdat[ithread].cy_i11 = _cy_i11[ithread];
			tdat[ithread].cy_r12 = _cy_r12[ithread];	tdat[ithread].cy_i12 = _cy_i12[ithread];
			tdat[ithread].cy_r13 = _cy_r13[ithread];	tdat[ithread].cy_i13 = _cy_i13[ithread];
			tdat[ithread].cy_r14 = _cy_r14[ithread];	tdat[ithread].cy_i14 = _cy_i14[ithread];
			tdat[ithread].cy_r15 = _cy_r15[ithread];	tdat[ithread].cy_i15 = _cy_i15[ithread];
			tdat[ithread].cy_r16 = _cy_r16[ithread];	tdat[ithread].cy_i16 = _cy_i16[ithread];
			tdat[ithread].cy_r17 = _cy_r17[ithread];	tdat[ithread].cy_i17 = _cy_i17[ithread];
			tdat[ithread].cy_r18 = _cy_r18[ithread];	tdat[ithread].cy_i18 = _cy_i18[ithread];
			tdat[ithread].cy_r19 = _cy_r19[ithread];	tdat[ithread].cy_i19 = _cy_i19[ithread];
			tdat[ithread].cy_r1A = _cy_r1A[ithread];	tdat[ithread].cy_i1A = _cy_i1A[ithread];
			tdat[ithread].cy_r1B = _cy_r1B[ithread];	tdat[ithread].cy_i1B = _cy_i1B[ithread];
			tdat[ithread].cy_r1C = _cy_r1C[ithread];	tdat[ithread].cy_i1C = _cy_i1C[ithread];
			tdat[ithread].cy_r1D = _cy_r1D[ithread];	tdat[ithread].cy_i1D = _cy_i1D[ithread];
			tdat[ithread].cy_r1E = _cy_r1E[ithread];	tdat[ithread].cy_i1E = _cy_i1E[ithread];
			tdat[ithread].cy_r1F = _cy_r1F[ithread];	tdat[ithread].cy_i1F = _cy_i1F[ithread];
		}
	}
#endif

#ifdef USE_PTHREAD

	// If also using main thread to do work units, that task-dispatch occurs after all the threadpool-task launches:
	for(ithread = 0; ithread < pool_work_units; ithread++)
	{
		task_control.data = (void*)(&tdat[ithread]);
		threadpool_add_task(tpool, &task_control, task_is_blocking);

#else

	for(ithread = 0; ithread < CY_THREADS; ithread++)
	{
		/***** DEC/HP CC doesn't properly copy init value of maxerr = 0 into threads,
		so need to set once again explicitly for each: *****/
		maxerr = 0.0;
	#ifdef USE_SSE2
		max_err->re = 0.0;	max_err->im = 0.0;
	#endif

		i      = _i[ithread];	/* Pointer to the BASE and BASEINV arrays.	*/
		jstart = _jstart[ithread];
		jhi    = _jhi[ithread];

		col = _col[ithread];
		co2 = _co2[ithread];
		co3 = _co3[ithread];

		if(MODULUS_TYPE == MODULUS_TYPE_MERSENNE)
		{
		#ifdef USE_SSE2
			*bjmodn00 = _bjmodn00[ithread];
			*bjmodn01 = _bjmodn01[ithread];
			*bjmodn02 = _bjmodn02[ithread];
			*bjmodn03 = _bjmodn03[ithread];
			*bjmodn04 = _bjmodn04[ithread];
			*bjmodn05 = _bjmodn05[ithread];
			*bjmodn06 = _bjmodn06[ithread];
			*bjmodn07 = _bjmodn07[ithread];
			*bjmodn08 = _bjmodn08[ithread];
			*bjmodn09 = _bjmodn09[ithread];
			*bjmodn0A = _bjmodn0A[ithread];
			*bjmodn0B = _bjmodn0B[ithread];
			*bjmodn0C = _bjmodn0C[ithread];
			*bjmodn0D = _bjmodn0D[ithread];
			*bjmodn0E = _bjmodn0E[ithread];
			*bjmodn0F = _bjmodn0F[ithread];
			*bjmodn10 = _bjmodn10[ithread];
			*bjmodn11 = _bjmodn11[ithread];
			*bjmodn12 = _bjmodn12[ithread];
			*bjmodn13 = _bjmodn13[ithread];
			*bjmodn14 = _bjmodn14[ithread];
			*bjmodn15 = _bjmodn15[ithread];
			*bjmodn16 = _bjmodn16[ithread];
			*bjmodn17 = _bjmodn17[ithread];
			*bjmodn18 = _bjmodn18[ithread];
			*bjmodn19 = _bjmodn19[ithread];
			*bjmodn1A = _bjmodn1A[ithread];
			*bjmodn1B = _bjmodn1B[ithread];
			*bjmodn1C = _bjmodn1C[ithread];
			*bjmodn1D = _bjmodn1D[ithread];
			*bjmodn1E = _bjmodn1E[ithread];
			*bjmodn1F = _bjmodn1F[ithread];
		#else
			bjmodn00 = _bjmodn00[ithread];
			bjmodn01 = _bjmodn01[ithread];
			bjmodn02 = _bjmodn02[ithread];
			bjmodn03 = _bjmodn03[ithread];
			bjmodn04 = _bjmodn04[ithread];
			bjmodn05 = _bjmodn05[ithread];
			bjmodn06 = _bjmodn06[ithread];
			bjmodn07 = _bjmodn07[ithread];
			bjmodn08 = _bjmodn08[ithread];
			bjmodn09 = _bjmodn09[ithread];
			bjmodn0A = _bjmodn0A[ithread];
			bjmodn0B = _bjmodn0B[ithread];
			bjmodn0C = _bjmodn0C[ithread];
			bjmodn0D = _bjmodn0D[ithread];
			bjmodn0E = _bjmodn0E[ithread];
			bjmodn0F = _bjmodn0F[ithread];
			bjmodn10 = _bjmodn10[ithread];
			bjmodn11 = _bjmodn11[ithread];
			bjmodn12 = _bjmodn12[ithread];
			bjmodn13 = _bjmodn13[ithread];
			bjmodn14 = _bjmodn14[ithread];
			bjmodn15 = _bjmodn15[ithread];
			bjmodn16 = _bjmodn16[ithread];
			bjmodn17 = _bjmodn17[ithread];
			bjmodn18 = _bjmodn18[ithread];
			bjmodn19 = _bjmodn19[ithread];
			bjmodn1A = _bjmodn1A[ithread];
			bjmodn1B = _bjmodn1B[ithread];
			bjmodn1C = _bjmodn1C[ithread];
			bjmodn1D = _bjmodn1D[ithread];
			bjmodn1E = _bjmodn1E[ithread];
			bjmodn1F = _bjmodn1F[ithread];
		#endif
			/* init carries	*/
		#ifdef USE_SSE2
			cy_r00->re = _cy_r00[ithread];
			cy_r00->im = _cy_r01[ithread];
			cy_r02->re = _cy_r02[ithread];
			cy_r02->im = _cy_r03[ithread];
			cy_r04->re = _cy_r04[ithread];
			cy_r04->im = _cy_r05[ithread];
			cy_r06->re = _cy_r06[ithread];
			cy_r06->im = _cy_r07[ithread];
			cy_r08->re = _cy_r08[ithread];
			cy_r08->im = _cy_r09[ithread];
			cy_r0A->re = _cy_r0A[ithread];
			cy_r0A->im = _cy_r0B[ithread];
			cy_r0C->re = _cy_r0C[ithread];
			cy_r0C->im = _cy_r0D[ithread];
			cy_r0E->re = _cy_r0E[ithread];
			cy_r0E->im = _cy_r0F[ithread];
			cy_r10->re = _cy_r10[ithread];
			cy_r10->im = _cy_r11[ithread];
			cy_r12->re = _cy_r12[ithread];
			cy_r12->im = _cy_r13[ithread];
			cy_r14->re = _cy_r14[ithread];
			cy_r14->im = _cy_r15[ithread];
			cy_r16->re = _cy_r16[ithread];
			cy_r16->im = _cy_r17[ithread];
			cy_r18->re = _cy_r18[ithread];
			cy_r18->im = _cy_r19[ithread];
			cy_r1A->re = _cy_r1A[ithread];
			cy_r1A->im = _cy_r1B[ithread];
			cy_r1C->re = _cy_r1C[ithread];
			cy_r1C->im = _cy_r1D[ithread];
			cy_r1E->re = _cy_r1E[ithread];
			cy_r1E->im = _cy_r1F[ithread];
		#else
			cy_r00 = _cy_r00[ithread];
			cy_r01 = _cy_r01[ithread];
			cy_r02 = _cy_r02[ithread];
			cy_r03 = _cy_r03[ithread];
			cy_r04 = _cy_r04[ithread];
			cy_r05 = _cy_r05[ithread];
			cy_r06 = _cy_r06[ithread];
			cy_r07 = _cy_r07[ithread];
			cy_r08 = _cy_r08[ithread];
			cy_r09 = _cy_r09[ithread];
			cy_r0A = _cy_r0A[ithread];
			cy_r0B = _cy_r0B[ithread];
			cy_r0C = _cy_r0C[ithread];
			cy_r0D = _cy_r0D[ithread];
			cy_r0E = _cy_r0E[ithread];
			cy_r0F = _cy_r0F[ithread];
			cy_r10 = _cy_r10[ithread];
			cy_r11 = _cy_r11[ithread];
			cy_r12 = _cy_r12[ithread];
			cy_r13 = _cy_r13[ithread];
			cy_r14 = _cy_r14[ithread];
			cy_r15 = _cy_r15[ithread];
			cy_r16 = _cy_r16[ithread];
			cy_r17 = _cy_r17[ithread];
			cy_r18 = _cy_r18[ithread];
			cy_r19 = _cy_r19[ithread];
			cy_r1A = _cy_r1A[ithread];
			cy_r1B = _cy_r1B[ithread];
			cy_r1C = _cy_r1C[ithread];
			cy_r1D = _cy_r1D[ithread];
			cy_r1E = _cy_r1E[ithread];
			cy_r1F = _cy_r1F[ithread];
		#endif
		}
		else	/* Fermat-mod uses "double helix" carry scheme - 2 separate sets of real/imaginary carries for right-angle transform, plus "twisted" wraparound step. */
		{
			/* init carries	*/
		#if defined(USE_SSE2)
			cy_r00->re = _cy_r00[ithread];	cy_r00->im = _cy_i00[ithread];
			cy_r02->re = _cy_r01[ithread];	cy_r02->im = _cy_i01[ithread];
			cy_r04->re = _cy_r02[ithread];	cy_r04->im = _cy_i02[ithread];
			cy_r06->re = _cy_r03[ithread];	cy_r06->im = _cy_i03[ithread];
			cy_r08->re = _cy_r04[ithread];	cy_r08->im = _cy_i04[ithread];
			cy_r0A->re = _cy_r05[ithread];	cy_r0A->im = _cy_i05[ithread];
			cy_r0C->re = _cy_r06[ithread];	cy_r0C->im = _cy_i06[ithread];
			cy_r0E->re = _cy_r07[ithread];	cy_r0E->im = _cy_i07[ithread];
			cy_r10->re = _cy_r08[ithread];	cy_r10->im = _cy_i08[ithread];
			cy_r12->re = _cy_r09[ithread];	cy_r12->im = _cy_i09[ithread];
			cy_r14->re = _cy_r0A[ithread];	cy_r14->im = _cy_i0A[ithread];
			cy_r16->re = _cy_r0B[ithread];	cy_r16->im = _cy_i0B[ithread];
			cy_r18->re = _cy_r0C[ithread];	cy_r18->im = _cy_i0C[ithread];
			cy_r1A->re = _cy_r0D[ithread];	cy_r1A->im = _cy_i0D[ithread];
			cy_r1C->re = _cy_r0E[ithread];	cy_r1C->im = _cy_i0E[ithread];
			cy_r1E->re = _cy_r0F[ithread];	cy_r1E->im = _cy_i0F[ithread];
			cy_i00->re = _cy_r10[ithread];	cy_i00->im = _cy_i10[ithread];
			cy_i02->re = _cy_r11[ithread];	cy_i02->im = _cy_i11[ithread];
			cy_i04->re = _cy_r12[ithread];	cy_i04->im = _cy_i12[ithread];
			cy_i06->re = _cy_r13[ithread];	cy_i06->im = _cy_i13[ithread];
			cy_i08->re = _cy_r14[ithread];	cy_i08->im = _cy_i14[ithread];
			cy_i0A->re = _cy_r15[ithread];	cy_i0A->im = _cy_i15[ithread];
			cy_i0C->re = _cy_r16[ithread];	cy_i0C->im = _cy_i16[ithread];
			cy_i0E->re = _cy_r17[ithread];	cy_i0E->im = _cy_i17[ithread];
			cy_i10->re = _cy_r18[ithread];	cy_i10->im = _cy_i18[ithread];
			cy_i12->re = _cy_r19[ithread];	cy_i12->im = _cy_i19[ithread];
			cy_i14->re = _cy_r1A[ithread];	cy_i14->im = _cy_i1A[ithread];
			cy_i16->re = _cy_r1B[ithread];	cy_i16->im = _cy_i1B[ithread];
			cy_i18->re = _cy_r1C[ithread];	cy_i18->im = _cy_i1C[ithread];
			cy_i1A->re = _cy_r1D[ithread];	cy_i1A->im = _cy_i1D[ithread];
			cy_i1C->re = _cy_r1E[ithread];	cy_i1C->im = _cy_i1E[ithread];
			cy_i1E->re = _cy_r1F[ithread];	cy_i1E->im = _cy_i1F[ithread];
		#else
			cy_r00 = _cy_r00[ithread];		cy_i00 = _cy_i00[ithread];
			cy_r01 = _cy_r01[ithread];		cy_i01 = _cy_i01[ithread];
			cy_r02 = _cy_r02[ithread];		cy_i02 = _cy_i02[ithread];
			cy_r03 = _cy_r03[ithread];		cy_i03 = _cy_i03[ithread];
			cy_r04 = _cy_r04[ithread];		cy_i04 = _cy_i04[ithread];
			cy_r05 = _cy_r05[ithread];		cy_i05 = _cy_i05[ithread];
			cy_r06 = _cy_r06[ithread];		cy_i06 = _cy_i06[ithread];
			cy_r07 = _cy_r07[ithread];		cy_i07 = _cy_i07[ithread];
			cy_r08 = _cy_r08[ithread];		cy_i08 = _cy_i08[ithread];
			cy_r09 = _cy_r09[ithread];		cy_i09 = _cy_i09[ithread];
			cy_r0A = _cy_r0A[ithread];		cy_i0A = _cy_i0A[ithread];
			cy_r0B = _cy_r0B[ithread];		cy_i0B = _cy_i0B[ithread];
			cy_r0C = _cy_r0C[ithread];		cy_i0C = _cy_i0C[ithread];
			cy_r0D = _cy_r0D[ithread];		cy_i0D = _cy_i0D[ithread];
			cy_r0E = _cy_r0E[ithread];		cy_i0E = _cy_i0E[ithread];
			cy_r0F = _cy_r0F[ithread];		cy_i0F = _cy_i0F[ithread];
			cy_r10 = _cy_r10[ithread];		cy_i10 = _cy_i10[ithread];
			cy_r11 = _cy_r11[ithread];		cy_i11 = _cy_i11[ithread];
			cy_r12 = _cy_r12[ithread];		cy_i12 = _cy_i12[ithread];
			cy_r13 = _cy_r13[ithread];		cy_i13 = _cy_i13[ithread];
			cy_r14 = _cy_r14[ithread];		cy_i14 = _cy_i14[ithread];
			cy_r15 = _cy_r15[ithread];		cy_i15 = _cy_i15[ithread];
			cy_r16 = _cy_r16[ithread];		cy_i16 = _cy_i16[ithread];
			cy_r17 = _cy_r17[ithread];		cy_i17 = _cy_i17[ithread];
			cy_r18 = _cy_r18[ithread];		cy_i18 = _cy_i18[ithread];
			cy_r19 = _cy_r19[ithread];		cy_i19 = _cy_i19[ithread];
			cy_r1A = _cy_r1A[ithread];		cy_i1A = _cy_i1A[ithread];
			cy_r1B = _cy_r1B[ithread];		cy_i1B = _cy_i1B[ithread];
			cy_r1C = _cy_r1C[ithread];		cy_i1C = _cy_i1C[ithread];
			cy_r1D = _cy_r1D[ithread];		cy_i1D = _cy_i1D[ithread];
			cy_r1E = _cy_r1E[ithread];		cy_i1E = _cy_i1E[ithread];
			cy_r1F = _cy_r1F[ithread];		cy_i1F = _cy_i1F[ithread];
		#endif
		}

		for(k=1; k <= khi; k++)	/* Do n/(radix(1)*nwt) outer loop executions...	*/
		{
		#ifdef USE_SSE2
			for(j = jstart; j < jhi; j += 4)
			{
			/* In SSE2 mode, data are arranged in [re0,re1,im0,im1] quartets, not the usual [re0,im0],[re1,im1] pairs.
			Thus we can still increment the j-index as if stepping through the residue array-of-doubles in strides of 2,
			but to point to the proper real datum, we need to bit-reverse bits <0:1> of j, i.e. [0,1,2,3] ==> [0,2,1,3].
			*/
				j1 = (j & mask01) + br4[j&3];
		#elif defined(USE_SSE2)	/* This allows us to use #if 0 above and disable sse2-based *computation*, while still using sse2-style data layout */
			for(j = jstart; j < jhi; j += 2)	/* Each inner loop execution processes (radix(1)*nwt) array data.	*/
			{
				j1 = (j & mask01) + br4[j&3];
		#else
			for(j = jstart; j < jhi; j += 2)	/* Each inner loop execution processes (radix(1)*nwt) array data.	*/
			{
				j1 =  j;
		#endif
				j1 = j1 + ( (j1 >> DAT_BITS) << PAD_BITS );	/* padded-array fetch index is here */
				j2 = j1+RE_IM_STRIDE;

		#ifdef DEBUG_SSE2
			rng_isaac_init(TRUE);
			jt = j1;		jp = j2;
			a[jt    ] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();	a[jp    ] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();			fprintf(stderr, "radix32_wrapper: A_in[00] = %20.5f, %20.5f\n",a[jt    ],a[jp    ]);
			a[jt+p01] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();	a[jp+p01] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();			fprintf(stderr, "radix32_wrapper: A_in[02] = %20.5f, %20.5f\n",a[jt+p01],a[jp+p01]);
			a[jt+p02] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();	a[jp+p02] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();			fprintf(stderr, "radix32_wrapper: A_in[04] = %20.5f, %20.5f\n",a[jt+p02],a[jp+p02]);
			a[jt+p03] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();	a[jp+p03] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();			fprintf(stderr, "radix32_wrapper: A_in[06] = %20.5f, %20.5f\n",a[jt+p03],a[jp+p03]);
			a[jt+p04] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();	a[jp+p04] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();			fprintf(stderr, "radix32_wrapper: A_in[08] = %20.5f, %20.5f\n",a[jt+p04],a[jp+p04]);
			a[jt+p05] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();	a[jp+p05] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();			fprintf(stderr, "radix32_wrapper: A_in[0a] = %20.5f, %20.5f\n",a[jt+p05],a[jp+p05]);
			a[jt+p06] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();	a[jp+p06] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();			fprintf(stderr, "radix32_wrapper: A_in[0c] = %20.5f, %20.5f\n",a[jt+p06],a[jp+p06]);
			a[jt+p07] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();	a[jp+p07] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();			fprintf(stderr, "radix32_wrapper: A_in[0e] = %20.5f, %20.5f\n",a[jt+p07],a[jp+p07]);	jt += p08;	jp += p08;
			a[jt    ] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();	a[jp    ] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();			fprintf(stderr, "radix32_wrapper: A_in[10] = %20.5f, %20.5f\n",a[jt    ],a[jp    ]);
			a[jt+p01] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();	a[jp+p01] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();			fprintf(stderr, "radix32_wrapper: A_in[12] = %20.5f, %20.5f\n",a[jt+p01],a[jp+p01]);
			a[jt+p02] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();	a[jp+p02] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();			fprintf(stderr, "radix32_wrapper: A_in[14] = %20.5f, %20.5f\n",a[jt+p02],a[jp+p02]);
			a[jt+p03] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();	a[jp+p03] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();			fprintf(stderr, "radix32_wrapper: A_in[16] = %20.5f, %20.5f\n",a[jt+p03],a[jp+p03]);
			a[jt+p04] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();	a[jp+p04] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();			fprintf(stderr, "radix32_wrapper: A_in[18] = %20.5f, %20.5f\n",a[jt+p04],a[jp+p04]);
			a[jt+p05] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();	a[jp+p05] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();			fprintf(stderr, "radix32_wrapper: A_in[1a] = %20.5f, %20.5f\n",a[jt+p05],a[jp+p05]);
			a[jt+p06] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();	a[jp+p06] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();			fprintf(stderr, "radix32_wrapper: A_in[1c] = %20.5f, %20.5f\n",a[jt+p06],a[jp+p06]);
			a[jt+p07] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();	a[jp+p07] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();			fprintf(stderr, "radix32_wrapper: A_in[1e] = %20.5f, %20.5f\n",a[jt+p07],a[jp+p07]);	jt += p08;	jp += p08;
			a[jt    ] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();	a[jp    ] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();			fprintf(stderr, "radix32_wrapper: A_in[20] = %20.5f, %20.5f\n",a[jt    ],a[jp    ]);
			a[jt+p01] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();	a[jp+p01] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();			fprintf(stderr, "radix32_wrapper: A_in[22] = %20.5f, %20.5f\n",a[jt+p01],a[jp+p01]);
			a[jt+p02] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();	a[jp+p02] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();			fprintf(stderr, "radix32_wrapper: A_in[24] = %20.5f, %20.5f\n",a[jt+p02],a[jp+p02]);
			a[jt+p03] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();	a[jp+p03] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();			fprintf(stderr, "radix32_wrapper: A_in[26] = %20.5f, %20.5f\n",a[jt+p03],a[jp+p03]);
			a[jt+p04] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();	a[jp+p04] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();			fprintf(stderr, "radix32_wrapper: A_in[28] = %20.5f, %20.5f\n",a[jt+p04],a[jp+p04]);
			a[jt+p05] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();	a[jp+p05] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();			fprintf(stderr, "radix32_wrapper: A_in[2a] = %20.5f, %20.5f\n",a[jt+p05],a[jp+p05]);
			a[jt+p06] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();	a[jp+p06] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();			fprintf(stderr, "radix32_wrapper: A_in[2c] = %20.5f, %20.5f\n",a[jt+p06],a[jp+p06]);
			a[jt+p07] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();	a[jp+p07] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();			fprintf(stderr, "radix32_wrapper: A_in[2e] = %20.5f, %20.5f\n",a[jt+p07],a[jp+p07]);	jt += p08;	jp += p08;
			a[jt    ] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();	a[jp    ] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();			fprintf(stderr, "radix32_wrapper: A_in[30] = %20.5f, %20.5f\n",a[jt    ],a[jp    ]);
			a[jt+p01] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();	a[jp+p01] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();			fprintf(stderr, "radix32_wrapper: A_in[32] = %20.5f, %20.5f\n",a[jt+p01],a[jp+p01]);
			a[jt+p02] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();	a[jp+p02] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();			fprintf(stderr, "radix32_wrapper: A_in[34] = %20.5f, %20.5f\n",a[jt+p02],a[jp+p02]);
			a[jt+p03] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();	a[jp+p03] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();			fprintf(stderr, "radix32_wrapper: A_in[36] = %20.5f, %20.5f\n",a[jt+p03],a[jp+p03]);
			a[jt+p04] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();	a[jp+p04] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();			fprintf(stderr, "radix32_wrapper: A_in[38] = %20.5f, %20.5f\n",a[jt+p04],a[jp+p04]);
			a[jt+p05] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();	a[jp+p05] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();			fprintf(stderr, "radix32_wrapper: A_in[3a] = %20.5f, %20.5f\n",a[jt+p05],a[jp+p05]);
			a[jt+p06] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();	a[jp+p06] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();			fprintf(stderr, "radix32_wrapper: A_in[3c] = %20.5f, %20.5f\n",a[jt+p06],a[jp+p06]);
			a[jt+p07] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();	a[jp+p07] = 1024.0*1024.0*rng_isaac_rand_double_norm_pm1();			fprintf(stderr, "radix32_wrapper: A_in[3e] = %20.5f, %20.5f\n",a[jt+p07],a[jp+p07]);	jt += p08;	jp += p08;
		#endif

		#ifdef USE_SSE2

		/*...The radix-32 DIT pass is here:	*/
#ifdef CTIME
	clock2 = clock();
#endif
		/* Disable original MSVC inline asm in favor of macro: */
		  #if 0//def COMPILER_TYPE_MSVC

		/*...Block 1: */

			jt = j1;
			jp = j2;

			add0 = &a[jt];
			add1 = add0+p01;
			add2 = add0+p02;
			add3 = add0+p03;
			add4 = add0+p04;
			add5 = add0+p05;
			add6 = add0+p06;
			add7 = add0+p07;

			SSE2_RADIX4_DIT_0TWIDDLE         (add0, add1, add2, add3, r00)
			SSE2_RADIX4_DIT_0TWIDDLE_2NDOFTWO(add4, add5, add6, add7, r08)
			/* Combine the 2 radix-4 subtransforms: */
			SSE2_RADIX8_DIT_COMBINE_RAD4_SUBS(r00,r02,r04,r06,r08,r0A,r0C,r0E)

		/*...Block 2:;	*/

			jt = j1 + p08;
			jp = j2 + p08;

			add0 = &a[jt];
			add1 = add0+p01;
			add2 = add0+p02;
			add3 = add0+p03;
			add4 = add0+p04;
			add5 = add0+p05;
			add6 = add0+p06;
			add7 = add0+p07;

			SSE2_RADIX4_DIT_0TWIDDLE         (add0, add1, add2, add3, r10)
			SSE2_RADIX4_DIT_0TWIDDLE_2NDOFTWO(add4, add5, add6, add7, r18)
			/* Combine the 2 radix-4 subtransforms: */
			SSE2_RADIX8_DIT_COMBINE_RAD4_SUBS(r10,r12,r14,r16,r18,r1A,r1C,r1E)

		/*...Block 3:	*/

			jt = j1 + p10;
			jp = j2 + p10;

			add0 = &a[jt];
			add1 = add0+p01;
			add2 = add0+p02;
			add3 = add0+p03;
			add4 = add0+p04;
			add5 = add0+p05;
			add6 = add0+p06;
			add7 = add0+p07;

			SSE2_RADIX4_DIT_0TWIDDLE         (add0, add1, add2, add3, r20)
			SSE2_RADIX4_DIT_0TWIDDLE_2NDOFTWO(add4, add5, add6, add7, r28)
			/* Combine the 2 radix-4 subtransforms: */
			SSE2_RADIX8_DIT_COMBINE_RAD4_SUBS(r20,r22,r24,r26,r28,r2A,r2C,r2E)

		/*...Block 4:	*/

			jt = j1 + p18;
			jp = j2 + p18;

			add0 = &a[jt];
			add1 = add0+p01;
			add2 = add0+p02;
			add3 = add0+p03;
			add4 = add0+p04;
			add5 = add0+p05;
			add6 = add0+p06;
			add7 = add0+p07;

			SSE2_RADIX4_DIT_0TWIDDLE         (add0, add1, add2, add3, r30)
			SSE2_RADIX4_DIT_0TWIDDLE_2NDOFTWO(add4, add5, add6, add7, r38)
			/* Combine the 2 radix-4 subtransforms: */
			SSE2_RADIX8_DIT_COMBINE_RAD4_SUBS(r30,r32,r34,r36,r38,r3A,r3C,r3E)

		/*...and now do eight radix-4 transforms, including the internal twiddle factors:	*/

		/*...Block 1: r00,r10,r20,r30	*/

			__asm	mov	eax, r00
			__asm	mov	ebx, eax		__asm	add	ebx, 0x100
			__asm	mov	ecx, eax		__asm	add	ecx, 0x200
			__asm	mov	edx, eax		__asm	add	edx, 0x300

			__asm	movaps	xmm0,[eax     ]	/* t1  */
			__asm	movaps	xmm1,[eax+0x10]	/* t2  */
			__asm	movaps	xmm2,[ebx     ]	/* t9  */
			__asm	movaps	xmm3,[ebx+0x10]	/* t10 */

			__asm	subpd	xmm0,[ebx     ]	/*~t9 =t1 -t9 */
			__asm	subpd	xmm1,[ebx+0x10]	/*~t10=t2 -t10*/
			__asm	addpd	xmm2,[eax     ]	/*~t1 =t9 +t1 */
			__asm	addpd	xmm3,[eax+0x10]	/*~t2 =t10+t2 */

			__asm	movaps	xmm4,[ecx     ]	/* t17 */
			__asm	movaps	xmm5,[ecx+0x10]	/* t18 */
			__asm	movaps	xmm6,[edx     ]	/* t25 */
			__asm	movaps	xmm7,[edx+0x10]	/* t26 */

			__asm	subpd	xmm4,[edx     ]	/*~t25=t17-t25*/
			__asm	subpd	xmm5,[edx+0x10]	/*~t26=t18-t26*/
			__asm	addpd	xmm6,[ecx     ]	/*~t17=t25+t17*/
			__asm	addpd	xmm7,[ecx+0x10]	/*~t18=t26+t18*/

			/* Intermediates in regs 2,3,0,1,6,7,4,5: */
			SSE2_RADIX4_DIT_LASTPAIR_IN_PLACE_STRIDE(xmm2,xmm3,xmm0,xmm1,xmm6,xmm7,xmm4,xmm5)

		/*...Block 5: r08,r18,r28,r38	*/

			__asm	mov	eax, r08
			__asm	mov	ebx, eax		__asm	add	ebx, 0x100
			__asm	mov	ecx, eax		__asm	add	ecx, 0x200
			__asm	mov	edx, eax		__asm	add	edx, 0x300
			__asm	mov	edi, isrt2
			__asm	movaps	xmm2,[edi]	/* isrt2 */

			__asm	movaps	xmm4,[ecx     ]	/* t28 */
			__asm	movaps	xmm5,[ecx+0x10]	/* t29 */
			__asm	movaps	xmm0,[edx     ]	/* t38 */
			__asm	movaps	xmm1,[edx+0x10]	/* t39 */

			__asm	addpd	xmm4,[ecx+0x10]	/*~t28=t28+t29*/
			__asm	subpd	xmm5,[ecx     ]	/*~t29=t29-t28*/
			__asm	subpd	xmm0,[edx+0x10]	/* rt =t38-t39*/
			__asm	addpd	xmm1,[edx     ]	/* it =t39+t38*/
			__asm	mulpd	xmm4,xmm2
			__asm	mulpd	xmm5,xmm2
			__asm	mulpd	xmm0,xmm2
			__asm	mulpd	xmm1,xmm2
			__asm	movaps	xmm6,xmm4			/* t28 copy */
			__asm	movaps	xmm7,xmm5			/* t29 copy */

			__asm	subpd	xmm4,xmm0			/*~t28=t28-rt */
			__asm	subpd	xmm5,xmm1			/*~t29=t29-it */
			__asm	addpd	xmm6,xmm0			/*~t38=t28+rt */
			__asm	addpd	xmm7,xmm1			/*~t39=t29+it */

			__asm	movaps	xmm0,[eax     ]	/* t08 */
			__asm	movaps	xmm1,[eax+0x10]	/* t09 */
			__asm	movaps	xmm2,[ebx     ]	/* t18 */
			__asm	movaps	xmm3,[ebx+0x10]	/* t19 */

			__asm	subpd	xmm0,[ebx+0x10]	/*~t18=t08-t19*/
			__asm	subpd	xmm1,[ebx     ]	/*~t09=t09-t18*/
			__asm	addpd	xmm3,[eax     ]	/*~t08=t19+t08*/
			__asm	addpd	xmm2,[eax+0x10]	/*~t19=t18+t09*/

			/* Intermediates in regs 3,1,0,2,4,5,6,7: */
			SSE2_RADIX4_DIT_LASTPAIR_IN_PLACE_STRIDE(xmm3,xmm1,xmm0,xmm2,xmm4,xmm5,xmm6,xmm7)

		/*...Block 3: r04,r14,r24,r34	*/

			__asm	mov	eax, r04
			__asm	mov	ebx, eax		__asm	add	ebx, 0x100
			__asm	mov	ecx, eax		__asm	add	ecx, 0x200
			__asm	mov	edx, eax		__asm	add	edx, 0x300
			__asm	mov	edi, isrt2
			__asm	mov	esi, cc0

			__asm	movaps	xmm4,[ecx     ]	/* t24 */				__asm	movaps	xmm0,[edx     ]	/* t34 */
			__asm	movaps	xmm5,[ecx+0x10]	/* t25 */				__asm	movaps	xmm1,[edx+0x10]	/* t35 */
			__asm	movaps	xmm6,[ecx     ]	/* xmm2 <- cpy t24 */	__asm	movaps	xmm2,[edx     ]	/* xmm6 <- cpy t34 */
			__asm	movaps	xmm7,[ecx+0x10]	/* xmm3 <- cpy t25 */	__asm	movaps	xmm3,[edx+0x10]	/* xmm7 <- cpy t35 */

			__asm	mulpd	xmm4,[esi     ]	/* t24*c */				__asm	mulpd	xmm0,[esi+0x10]	/* t34*s */
			__asm	mulpd	xmm5,[esi     ]	/* t25*c */				__asm	mulpd	xmm1,[esi+0x10]	/* t35*s */
			__asm	mulpd	xmm6,[esi+0x10]	/* t24*s */				__asm	mulpd	xmm2,[esi     ]	/* t34*c */
			__asm	mulpd	xmm7,[esi+0x10]	/* t25*s */				__asm	mulpd	xmm3,[esi     ]	/* t35*c */
			__asm	subpd	xmm5,xmm6	/* xmm1 <-~t25*/			__asm	subpd	xmm1,xmm2	/* xmm5 <- it */
			__asm	addpd	xmm4,xmm7	/* xmm0 <-~t24*/			__asm	addpd	xmm0,xmm3	/* xmm4 <- rt */
			__asm	movaps	xmm7,xmm5	/* xmm3 <- cpy~t25*/
			__asm	movaps	xmm6,xmm4	/* xmm2 <- cpy~t24*/

			__asm	addpd	xmm4,xmm0	/* ~t24 <- t24+rt */
			__asm	addpd	xmm5,xmm1	/* ~t25 <- t25+it */
			__asm	subpd	xmm6,xmm0	/* ~t34 <- t24-rt */
			__asm	subpd	xmm7,xmm1	/* ~t35 <- t25-it */

			__asm	movaps	xmm2,[ebx     ]	/* t14 */
			__asm	movaps	xmm3,[ebx+0x10]	/* t15 */
			__asm	movaps	xmm0,[eax     ]	/* t04 */
			__asm	movaps	xmm1,[eax+0x10]	/* t05 */
			__asm	addpd	xmm2,[ebx+0x10]	/*~t14=t14+t15*/
			__asm	subpd	xmm3,[ebx     ]	/*~t15=t15-t14*/
			__asm	mulpd	xmm2,[edi]	/* rt */
			__asm	mulpd	xmm3,[edi]	/* it */

			__asm	subpd	xmm0,xmm2	/*~t14 <- t04- rt */
			__asm	subpd	xmm1,xmm3	/*~t15 <- t05- it */
			__asm	addpd	xmm2,xmm2	/*          2* rt */
			__asm	addpd	xmm3,xmm3	/*          2* it */
			__asm	addpd	xmm2,xmm0	/*~t04 <- t04+ rt */
			__asm	addpd	xmm3,xmm1	/*~t05 <- t05+ it */

			/* Intermediates in regs 2,3,0,1,4,5,6,7: */
			SSE2_RADIX4_DIT_LASTPAIR_IN_PLACE_STRIDE(xmm2,xmm3,xmm0,xmm1,xmm4,xmm5,xmm6,xmm7)

		/*...Block 7: r0C,r1C,r2C,r3C	*/

			__asm	mov	eax, r0C
			__asm	mov	ebx, eax		__asm	add	ebx, 0x100
			__asm	mov	ecx, eax		__asm	add	ecx, 0x200
			__asm	mov	edx, eax		__asm	add	edx, 0x300
			__asm	mov	edi, isrt2
			__asm	mov	esi, cc0

			__asm	movaps	xmm4,[ecx     ]	/* t2C */				__asm	movaps	xmm0,[edx     ]	/* t3C */
			__asm	movaps	xmm5,[ecx+0x10]	/* t2D */				__asm	movaps	xmm1,[edx+0x10]	/* t3D */
			__asm	movaps	xmm6,[ecx     ]	/* xmm2 <- cpy t2C */	__asm	movaps	xmm2,[edx     ]	/* xmm6 <- cpy t3C */
			__asm	movaps	xmm7,[ecx+0x10]	/* xmm3 <- cpy t2D */	__asm	movaps	xmm3,[edx+0x10]	/* xmm7 <- cpy t3D */

			__asm	mulpd	xmm4,[esi+0x10]	/* t2C*s */				__asm	mulpd	xmm0,[esi     ]	/* t3C*c */
			__asm	mulpd	xmm5,[esi+0x10]	/* t2D*s */				__asm	mulpd	xmm1,[esi     ]	/* t3D*c */
			__asm	mulpd	xmm6,[esi     ]	/* t2C*c */				__asm	mulpd	xmm2,[esi+0x10]	/* t3C*s */
			__asm	mulpd	xmm7,[esi     ]	/* t2D*c */				__asm	mulpd	xmm3,[esi+0x10]	/* t3D*s */
			__asm	subpd	xmm5,xmm6	/* xmm1 <-~t2D */			__asm	subpd	xmm1,xmm2	/* xmm5 <- it */
			__asm	addpd	xmm4,xmm7	/* xmm0 <-~t2C */			__asm	addpd	xmm0,xmm3	/* xmm4 <- rt */
			__asm	movaps	xmm7,xmm5	/* xmm3 <- cpy~t2D */
			__asm	movaps	xmm6,xmm4	/* xmm2 <- cpy~t2C */

			__asm	addpd	xmm4,xmm0	/* ~t3C <- t2C+rt */
			__asm	addpd	xmm5,xmm1	/* ~t3D <- t2D+it */
			__asm	subpd	xmm6,xmm0	/* ~t2C <- t2C-rt */
			__asm	subpd	xmm7,xmm1	/* ~t2D <- t2D-it */

			__asm	movaps	xmm2,[ebx     ]	/* t1C */
			__asm	movaps	xmm3,[ebx+0x10]	/* t1D */
			__asm	movaps	xmm0,[eax     ]	/* t0C */
			__asm	movaps	xmm1,[eax+0x10]	/* t0D */
			__asm	subpd	xmm2,[ebx+0x10]	/*~t1C=t1C-t1D */
			__asm	addpd	xmm3,[ebx     ]	/*~t1D=t1D+t1C */
			__asm	mulpd	xmm2,[edi]	/* rt */
			__asm	mulpd	xmm3,[edi]	/* it */

			__asm	subpd	xmm0,xmm2	/*~t0C <- t0C- rt */
			__asm	subpd	xmm1,xmm3	/*~t0D <- t0D- it */
			__asm	addpd	xmm2,xmm2	/*          2* rt */
			__asm	addpd	xmm3,xmm3	/*          2* it */
			__asm	addpd	xmm2,xmm0	/*~t1C <- t0C+ rt */
			__asm	addpd	xmm3,xmm1	/*~t1D <- t0D+ it */

			/* Intermediates in regs 0,1,2,3,6,7,4,5: */
			SSE2_RADIX4_DIT_LASTPAIR_IN_PLACE_STRIDE(xmm0,xmm1,xmm2,xmm3,xmm6,xmm7,xmm4,xmm5)

		/*...Block 2: r02,r12,r22,r32	*/

			__asm	mov	eax, r02
			__asm	mov	ebx, eax		__asm	add	ebx, 0x100
			__asm	mov	ecx, eax		__asm	add	ecx, 0x200
			__asm	mov	edx, eax		__asm	add	edx, 0x300
			__asm	mov	edi, cc1
			__asm	mov	esi, cc3

			__asm	movaps	xmm4,[ecx     ]	/* t22 */				__asm	movaps	xmm0,[edx     ]	/* t32 */
			__asm	movaps	xmm5,[ecx+0x10]	/* t23 */				__asm	movaps	xmm1,[edx+0x10]	/* t33 */
			__asm	movaps	xmm6,[ecx     ]	/* xmm2 <- cpy t22 */	__asm	movaps	xmm2,[edx     ]	/* xmm6 <- cpy t32 */
			__asm	movaps	xmm7,[ecx+0x10]	/* xmm3 <- cpy t23 */	__asm	movaps	xmm3,[edx+0x10]	/* xmm7 <- cpy t33 */

			__asm	mulpd	xmm4,[edi     ]	/* t22*c32_1 */			__asm	mulpd	xmm0,[esi     ]	/* t32*c32_3 */
			__asm	mulpd	xmm5,[edi     ]	/* t23*c32_1 */			__asm	mulpd	xmm1,[esi     ]	/* t33*c32_3 */
			__asm	mulpd	xmm6,[edi+0x10]	/* t22*s32_1 */			__asm	mulpd	xmm2,[esi+0x10]	/* t32*s32_3 */
			__asm	mulpd	xmm7,[edi+0x10]	/* t23*s32_1 */			__asm	mulpd	xmm3,[esi+0x10]	/* t33*s32_3 */
			__asm	subpd	xmm5,xmm6	/* xmm1 <-~t23 */			__asm	subpd	xmm1,xmm2	/* xmm5 <- it */
			__asm	addpd	xmm4,xmm7	/* xmm0 <-~t22 */			__asm	addpd	xmm0,xmm3	/* xmm4 <- rt */
			__asm	movaps	xmm7,xmm5	/* xmm3 <- cpy~t23 */
			__asm	movaps	xmm6,xmm4	/* xmm2 <- cpy~t22 */

			__asm	subpd	xmm4,xmm0	/* ~t32 <- t22-rt */
			__asm	subpd	xmm5,xmm1	/* ~t33 <- t23-it */
			__asm	addpd	xmm6,xmm0	/* ~t22 <- t22+rt */
			__asm	addpd	xmm7,xmm1	/* ~t23 <- t23+it */

			__asm	mov	esi, cc0
			__asm	movaps	xmm2,[ebx     ]	/* t12 */
			__asm	movaps	xmm3,[ebx+0x10]	/* t13 */
			__asm	movaps	xmm0,[ebx     ]	/* cpy t12 */
			__asm	movaps	xmm1,[ebx+0x10]	/* cpy t13 */

			__asm	mulpd	xmm2,[esi     ]	/* t12*c */
			__asm	mulpd	xmm1,[esi+0x10]	/* t13*s */
			__asm	mulpd	xmm3,[esi     ]	/* t13*c */
			__asm	mulpd	xmm0,[esi+0x10]	/* t12*s */
			__asm	addpd	xmm2,xmm1	/* xmm2 <- rt */
			__asm	subpd	xmm3,xmm0	/* xmm3 <- it */

			__asm	movaps	xmm0,[eax     ]	/* t02 */
			__asm	movaps	xmm1,[eax+0x10]	/* t03 */

			__asm	subpd	xmm0,xmm2	/*~t12 <- t02- rt */
			__asm	subpd	xmm1,xmm3	/*~t13 <- t03- it */
			__asm	addpd	xmm2,xmm2	/*          2* rt */
			__asm	addpd	xmm3,xmm3	/*          2* it */
			__asm	addpd	xmm2,xmm0	/*~t02 <- t02+ rt */
			__asm	addpd	xmm3,xmm1	/*~t03 <- t03+ it */

			/* Intermediates in regs 2,3,0,1,6,7,4,5: */
			SSE2_RADIX4_DIT_LASTPAIR_IN_PLACE_STRIDE(xmm2,xmm3,xmm0,xmm1,xmm6,xmm7,xmm4,xmm5)

		/*...Block 6: r0A,r1A,r2A,r3A	*/

			__asm	mov	eax, r0A
			__asm	mov	ebx, eax		__asm	add	ebx, 0x100
			__asm	mov	ecx, eax		__asm	add	ecx, 0x200
			__asm	mov	edx, eax		__asm	add	edx, 0x300
			__asm	mov	edi, cc1
			__asm	mov	esi, cc3

			__asm	movaps	xmm4,[ecx     ]	/* t2A */				__asm	movaps	xmm0,[edx     ]	/* t3A */
			__asm	movaps	xmm5,[ecx+0x10]	/* t2B */				__asm	movaps	xmm1,[edx+0x10]	/* t3B */
			__asm	movaps	xmm6,[ecx     ]	/* xmm2 <- cpy t2A */	__asm	movaps	xmm2,[edx     ]	/* xmm6 <- cpy t3A */
			__asm	movaps	xmm7,[ecx+0x10]	/* xmm3 <- cpy t2B */	__asm	movaps	xmm3,[edx+0x10]	/* xmm7 <- cpy t3B */

			__asm	mulpd	xmm4,[esi+0x10]	/* t2A*s32_3 */			__asm	mulpd	xmm0,[edi     ]	/* t3A*c32_1 */
			__asm	mulpd	xmm5,[esi+0x10]	/* t2B*s32_3 */			__asm	mulpd	xmm1,[edi     ]	/* t3B*c32_1 */
			__asm	mulpd	xmm6,[esi     ]	/* t2A*c32_3 */			__asm	mulpd	xmm2,[edi+0x10]	/* t3A*s32_1 */
			__asm	mulpd	xmm7,[esi     ]	/* t2B*c32_3 */			__asm	mulpd	xmm3,[edi+0x10]	/* t3B*s32_1 */
			__asm	subpd	xmm5,xmm6	/* xmm1 <-~t2B */			__asm	addpd	xmm1,xmm2	/* xmm5 <- it */
			__asm	addpd	xmm4,xmm7	/* xmm0 <-~t2A */			__asm	subpd	xmm0,xmm3	/* xmm4 <- rt */
			__asm	movaps	xmm7,xmm5	/* xmm3 <- cpy~t2B */
			__asm	movaps	xmm6,xmm4	/* xmm2 <- cpy~t2A */

			__asm	addpd	xmm4,xmm0	/* ~t3A <- t2A+rt */
			__asm	addpd	xmm5,xmm1	/* ~t3B <- t2B+it */
			__asm	subpd	xmm6,xmm0	/* ~t2A <- t2A-rt */
			__asm	subpd	xmm7,xmm1	/* ~t2B <- t2B-it */

			__asm	mov	esi, cc0
			__asm	movaps	xmm2,[ebx     ]	/* t1A */
			__asm	movaps	xmm3,[ebx+0x10]	/* t1B */
			__asm	movaps	xmm0,[ebx     ]	/* cpy t1A */
			__asm	movaps	xmm1,[ebx+0x10]	/* cpy t1B */

			__asm	mulpd	xmm2,[esi+0x10]	/* t1A*s */
			__asm	mulpd	xmm1,[esi     ]	/* t1B*c */
			__asm	mulpd	xmm3,[esi+0x10]	/* t1B*s */
			__asm	mulpd	xmm0,[esi     ]	/* t1A*c */
			__asm	subpd	xmm2,xmm1	/* xmmA <- rt */
			__asm	addpd	xmm3,xmm0	/* xmmB <- it */

			__asm	movaps	xmm0,[eax     ]	/* t0A */
			__asm	movaps	xmm1,[eax+0x10]	/* t0B */

			__asm	subpd	xmm0,xmm2	/*~t0A <- t0A- rt */
			__asm	subpd	xmm1,xmm3	/*~t0B <- t0B- it */
			__asm	addpd	xmm2,xmm2	/*          2* rt */
			__asm	addpd	xmm3,xmm3	/*          2* it */
			__asm	addpd	xmm2,xmm0	/*~t1A <- t0A+ rt */
			__asm	addpd	xmm3,xmm1	/*~t1B <- t0B+ it */

			/* Intermediates in regs 0,1,2,3,6,7,4,5: */
			SSE2_RADIX4_DIT_LASTPAIR_IN_PLACE_STRIDE(xmm0,xmm1,xmm2,xmm3,xmm6,xmm7,xmm4,xmm5)

		/*...Block 4: r06,r16,r26,r36	*/

			__asm	mov	eax, r06
			__asm	mov	ebx, eax		__asm	add	ebx, 0x100
			__asm	mov	ecx, eax		__asm	add	ecx, 0x200
			__asm	mov	edx, eax		__asm	add	edx, 0x300
			__asm	mov	edi, cc1
			__asm	mov	esi, cc3

			__asm	movaps	xmm4,[ecx     ]	/* t26 */				__asm	movaps	xmm0,[edx     ]	/* t36 */
			__asm	movaps	xmm5,[ecx+0x10]	/* t27 */				__asm	movaps	xmm1,[edx+0x10]	/* t37 */
			__asm	movaps	xmm6,[ecx     ]	/* xmm2 <- cpy t26 */	__asm	movaps	xmm2,[edx     ]	/* xmm6 <- cpy t36 */
			__asm	movaps	xmm7,[ecx+0x10]	/* xmm3 <- cpy t27 */	__asm	movaps	xmm3,[edx+0x10]	/* xmm7 <- cpy t37 */

			__asm	mulpd	xmm4,[esi     ]	/* t26*c32_3 */			__asm	mulpd	xmm0,[edi+0x10]	/* t36*s32_1 */
			__asm	mulpd	xmm5,[esi     ]	/* t27*c32_3 */			__asm	mulpd	xmm1,[edi+0x10]	/* t37*s32_1 */
			__asm	mulpd	xmm6,[esi+0x10]	/* t26*s32_3 */			__asm	mulpd	xmm2,[edi     ]	/* t36*c32_1 */
			__asm	mulpd	xmm7,[esi+0x10]	/* t27*s32_3 */			__asm	mulpd	xmm3,[edi     ]	/* t37*c32_1 */
			__asm	subpd	xmm5,xmm6	/* xmm1 <-~t27 */			__asm	addpd	xmm1,xmm2	/* xmm5 <- it */
			__asm	addpd	xmm4,xmm7	/* xmm0 <-~t26 */			__asm	subpd	xmm0,xmm3	/* xmm4 <- rt */
			__asm	movaps	xmm7,xmm5	/* xmm3 <- cpy~t27 */
			__asm	movaps	xmm6,xmm4	/* xmm2 <- cpy~t26 */

			__asm	addpd	xmm4,xmm0	/* ~t36 <- t26+rt */
			__asm	addpd	xmm5,xmm1	/* ~t37 <- t27+it */
			__asm	subpd	xmm6,xmm0	/* ~t26 <- t26-rt */
			__asm	subpd	xmm7,xmm1	/* ~t27 <- t27-it */

			__asm	mov	esi, cc0
			__asm	movaps	xmm2,[ebx     ]	/* t16 */
			__asm	movaps	xmm3,[ebx+0x10]	/* t17 */
			__asm	movaps	xmm0,[ebx     ]	/* cpy t16 */
			__asm	movaps	xmm1,[ebx+0x10]	/* cpy t17 */

			__asm	mulpd	xmm2,[esi+0x10]	/* t16*s */
			__asm	mulpd	xmm1,[esi     ]	/* t17*c */
			__asm	mulpd	xmm3,[esi+0x10]	/* t17*s */
			__asm	mulpd	xmm0,[esi     ]	/* t16*c */
			__asm	addpd	xmm2,xmm1	/* xmm2 <- rt */
			__asm	subpd	xmm3,xmm0	/* xmm3 <- it */

			__asm	movaps	xmm0,[eax     ]	/* t06 */
			__asm	movaps	xmm1,[eax+0x10]	/* t07 */

			__asm	subpd	xmm0,xmm2	/*~t16 <- t06- rt */
			__asm	subpd	xmm1,xmm3	/*~t17 <- t07- it */
			__asm	addpd	xmm2,xmm2	/*          2* rt */
			__asm	addpd	xmm3,xmm3	/*          2* it */
			__asm	addpd	xmm2,xmm0	/*~t06 <- t06+ rt */
			__asm	addpd	xmm3,xmm1	/*~t07 <- t07+ it */

			/* Intermediates in regs 2,3,0,1,6,7,4,5: */
			SSE2_RADIX4_DIT_LASTPAIR_IN_PLACE_STRIDE(xmm2,xmm3,xmm0,xmm1,xmm6,xmm7,xmm4,xmm5)

		/*...Block 8: r0E,r1E,r2E,r3E	*/

			__asm	mov	eax, r0E
			__asm	mov	ebx, eax		__asm	add	ebx, 0x100
			__asm	mov	ecx, eax		__asm	add	ecx, 0x200
			__asm	mov	edx, eax		__asm	add	edx, 0x300
			__asm	mov	edi, cc1
			__asm	mov	esi, cc3

			__asm	movaps	xmm4,[ecx     ]	/* t2E */				__asm	movaps	xmm0,[edx     ]	/* t3E */
			__asm	movaps	xmm5,[ecx+0x10]	/* t2F */				__asm	movaps	xmm1,[edx+0x10]	/* t3F */
			__asm	movaps	xmm6,[ecx     ]	/* xmm2 <- cpy t2E */	__asm	movaps	xmm2,[edx     ]	/* xmm6 <- cpy t3E */
			__asm	movaps	xmm7,[ecx+0x10]	/* xmm3 <- cpy t2F */	__asm	movaps	xmm3,[edx+0x10]	/* xmm7 <- cpy t3F */

			__asm	mulpd	xmm4,[edi+0x10]	/* t2E*s32_1 */			__asm	mulpd	xmm0,[esi+0x10]	/* t3E*s32_3 */
			__asm	mulpd	xmm5,[edi+0x10]	/* t2F*s32_1 */			__asm	mulpd	xmm1,[esi+0x10]	/* t3F*s32_3 */
			__asm	mulpd	xmm6,[edi     ]	/* t2E*c32_1 */			__asm	mulpd	xmm2,[esi     ]	/* t3E*c32_3 */
			__asm	mulpd	xmm7,[edi     ]	/* t2F*c32_1 */			__asm	mulpd	xmm3,[esi     ]	/* t3F*c32_3 */
			__asm	subpd	xmm5,xmm6	/* xmm1 <-~t2F */			__asm	subpd	xmm1,xmm2	/* xmm5 <- it */
			__asm	addpd	xmm4,xmm7	/* xmm0 <-~t2E */			__asm	addpd	xmm0,xmm3	/* xmm4 <- rt */
			__asm	movaps	xmm7,xmm5	/* xmm3 <- cpy~t2F */
			__asm	movaps	xmm6,xmm4	/* xmm2 <- cpy~t2E */

			__asm	addpd	xmm4,xmm0	/* ~t3E <- t2E+rt */
			__asm	addpd	xmm5,xmm1	/* ~t3F <- t2F+it */
			__asm	subpd	xmm6,xmm0	/* ~t2E <- t2E-rt */
			__asm	subpd	xmm7,xmm1	/* ~t2F <- t2F-it */

			__asm	mov	esi, cc0
			__asm	movaps	xmm2,[ebx     ]	/* t1E */
			__asm	movaps	xmm3,[ebx+0x10]	/* t1F */
			__asm	movaps	xmm0,[ebx     ]	/* cpy t1E */
			__asm	movaps	xmm1,[ebx+0x10]	/* cpy t1F */

			__asm	mulpd	xmm2,[esi     ]	/* t1E*c */
			__asm	mulpd	xmm1,[esi+0x10]	/* t1F*s */
			__asm	mulpd	xmm3,[esi     ]	/* t1F*c */
			__asm	mulpd	xmm0,[esi+0x10]	/* t1E*s */
			__asm	subpd	xmm2,xmm1	/* xmmE <- rt */
			__asm	addpd	xmm3,xmm0	/* xmmF <- it */

			__asm	movaps	xmm0,[eax     ]	/* t0E */
			__asm	movaps	xmm1,[eax+0x10]	/* t0F */

			__asm	subpd	xmm0,xmm2	/*~t0E <- t0E- rt */
			__asm	subpd	xmm1,xmm3	/*~t0F <- t0F- it */
			__asm	addpd	xmm2,xmm2	/*          2* rt */
			__asm	addpd	xmm3,xmm3	/*          2* it */
			__asm	addpd	xmm2,xmm0	/*~t1E <- t0E+ rt */
			__asm	addpd	xmm3,xmm1	/*~t1F <- t0F+ it */

			/* Intermediates in regs 0,1,2,3,6,7,4,5: */
			SSE2_RADIX4_DIT_LASTPAIR_IN_PLACE_STRIDE(xmm0,xmm1,xmm2,xmm3,xmm6,xmm7,xmm4,xmm5)

		  #else	/* GCC-style inline ASM: */

			add0 = &a[j1    ];
			SSE2_RADIX32_DIT_NOTWIDDLE(add0,p01,p02,p03,p04,p08,p10,p18,r00,isrt2,cc0);

		  #endif

		#else	/* USE_SSE2 */

			/*       gather the needed data (32 64-bit complex, i.e. 64 64-bit reals) and do the first set of four length-8 transforms...	*/
			/*...Block 1:	*/
			jt = j1;	jp = j2;

				t00=a[jt    ];	t01=a[jp    ];
				rt =a[jt+p01];	it =a[jp+p01];
				t02=t00-rt;		t03=t01-it;
				t00=t00+rt;		t01=t01+it;

				t04=a[jt+p02];	t05=a[jp+p02];
				rt =a[jt+p03];	it =a[jp+p03];
				t06=t04-rt;		t07=t05-it;
				t04=t04+rt;		t05=t05+it;

				rt =t04;		it =t05;
				t04=t00-rt;		t05=t01-it;
				t00=t00+rt;		t01=t01+it;

				rt =t06;		it =t07;
				t06=t02-it;		t07=t03+rt;
				t02=t02+it;		t03=t03-rt;

				t08=a[jt+p04];	t09=a[jp+p04];
				rt =a[jt+p05];	it =a[jp+p05];
				t0A=t08-rt;		t0B=t09-it;
				t08=t08+rt;		t09=t09+it;

				t0C=a[jt+p06];	t0D=a[jp+p06];
				rt =a[jt+p07];	it =a[jp+p07];
				t0E=t0C-rt;		t0F=t0D-it;
				t0C=t0C+rt;		t0D=t0D+it;

				rt =t0C;		it =t0D;
				t0C=t08-rt;		t0D=t09-it;
				t08=t08+rt;		t09=t09+it;

				rt =t0E;		it =t0F;
				t0E=t0A-it;		t0F=t0B+rt;
				t0A=t0A+it;		t0B=t0B-rt;

				rt =t08;		it =t09;
				t08=t00-rt;		t09=t01-it;
				t00=t00+rt;		t01=t01+it;

				rt =t0C;		it =t0D;
				t0C=t04-it;		t0D=t05+rt;
				t04=t04+it;		t05=t05-rt;

				rt =(t0A+t0B)*ISRT2;it =(t0A-t0B)*ISRT2;
				t0A=t02-rt;		t0B=t03+it;
				t02=t02+rt;		t03=t03-it;

				rt =(t0E-t0F)*ISRT2;it =(t0F+t0E)*ISRT2;
				t0E=t06+rt;		t0F=t07+it;
				t06=t06-rt;		t07=t07-it;

			/*...Block 2:;	*/
			jt = j1 + p08;	jp = j2 + p08;

				t10=a[jt    ];	t11=a[jp    ];
				rt =a[jt+p01];	it =a[jp+p01];
				t12=t10-rt;		t13=t11-it;
				t10=t10+rt;		t11=t11+it;

				t14=a[jt+p02];	t15=a[jp+p02];
				rt =a[jt+p03];	it =a[jp+p03];
				t16=t14-rt;		t17=t15-it;
				t14=t14+rt;		t15=t15+it;

				rt =t14;		it =t15;
				t14=t10-rt;		t15=t11-it;
				t10=t10+rt;		t11=t11+it;

				rt =t16;		it =t17;
				t16=t12-it;		t17=t13+rt;
				t12=t12+it;		t13=t13-rt;

				t18=a[jt+p04];	t19=a[jp+p04];
				rt =a[jt+p05];	it =a[jp+p05];
				t1A=t18-rt;		t1B=t19-it;
				t18=t18+rt;		t19=t19+it;

				t1C=a[jt+p06];	t1D=a[jp+p06];
				rt =a[jt+p07];	it =a[jp+p07];
				t1E=t1C-rt;		t1F=t1D-it;
				t1C=t1C+rt;		t1D=t1D+it;

				rt =t1C;		it =t1D;
				t1C=t18-rt;		t1D=t19-it;
				t18=t18+rt;		t19=t19+it;

				rt =t1E;		it =t1F;
				t1E=t1A-it;		t1F=t1B+rt;
				t1A=t1A+it;		t1B=t1B-rt;

				rt =t18;		it =t19;
				t18=t10-rt;		t19=t11-it;
				t10=t10+rt;		t11=t11+it;

				rt =t1C;		it =t1D;
				t1C=t14-it;		t1D=t15+rt;
				t14=t14+it;		t15=t15-rt;

				rt =(t1A+t1B)*ISRT2;it =(t1A-t1B)*ISRT2;
				t1A=t12-rt;		t1B=t13+it;
				t12=t12+rt;		t13=t13-it;

				rt =(t1E-t1F)*ISRT2;it =(t1F+t1E)*ISRT2;
				t1E=t16+rt;		t1F=t17+it;
				t16=t16-rt;		t17=t17-it;

			/*...Block 3:	*/
			jt = j1 + p10;	jp = j2 + p10;

				t20=a[jt    ];	t21=a[jp    ];
				rt =a[jt+p01];	it =a[jp+p01];
				t22=t20-rt;		t23=t21-it;
				t20=t20+rt;		t21=t21+it;

				t24=a[jt+p02];	t25=a[jp+p02];
				rt =a[jt+p03];	it =a[jp+p03];
				t26=t24-rt;		t27=t25-it;
				t24=t24+rt;		t25=t25+it;

				rt =t24;		it =t25;
				t24=t20-rt;		t25=t21-it;
				t20=t20+rt;		t21=t21+it;

				rt =t26;		it =t27;
				t26=t22-it;		t27=t23+rt;
				t22=t22+it;		t23=t23-rt;

				t28=a[jt+p04];	t29=a[jp+p04];
				rt =a[jt+p05];	it =a[jp+p05];
				t2A=t28-rt;		t2B=t29-it;
				t28=t28+rt;		t29=t29+it;

				t2C=a[jt+p06];	t2D=a[jp+p06];
				rt =a[jt+p07];	it =a[jp+p07];
				t2E=t2C-rt;		t2F=t2D-it;
				t2C=t2C+rt;		t2D=t2D+it;

				rt =t2C;		it =t2D;
				t2C=t28-rt;		t2D=t29-it;
				t28=t28+rt;		t29=t29+it;

				rt =t2E;		it =t2F;
				t2E=t2A-it;		t2F=t2B+rt;
				t2A=t2A+it;		t2B=t2B-rt;

				rt =t28;		it =t29;
				t28=t20-rt;		t29=t21-it;
				t20=t20+rt;		t21=t21+it;

				rt =t2C;		it =t2D;
				t2C=t24-it;		t2D=t25+rt;
				t24=t24+it;		t25=t25-rt;

				rt =(t2A+t2B)*ISRT2;it =(t2A-t2B)*ISRT2;
				t2A=t22-rt;		t2B=t23+it;
				t22=t22+rt;		t23=t23-it;

				rt =(t2E-t2F)*ISRT2;it =(t2F+t2E)*ISRT2;
				t2E=t26+rt;		t2F=t27+it;
				t26=t26-rt;		t27=t27-it;

			/*...Block 4:	*/
			jt = j1 + p18;	jp = j2 + p18;

				t30=a[jt    ];	t31=a[jp    ];
				rt =a[jt+p01];	it =a[jp+p01];
				t32=t30-rt;		t33=t31-it;
				t30=t30+rt;		t31=t31+it;

				t34=a[jt+p02];	t35=a[jp+p02];
				rt =a[jt+p03];	it =a[jp+p03];
				t36=t34-rt;		t37=t35-it;
				t34=t34+rt;		t35=t35+it;

				rt =t34;		it =t35;
				t34=t30-rt;		t35=t31-it;
				t30=t30+rt;		t31=t31+it;

				rt =t36;		it =t37;
				t36=t32-it;		t37=t33+rt;
				t32=t32+it;		t33=t33-rt;

				t38=a[jt+p04];	t39=a[jp+p04];
				rt =a[jt+p05];	it =a[jp+p05];
				t3A=t38-rt;		t3B=t39-it;
				t38=t38+rt;		t39=t39+it;

				t3C=a[jt+p06];	t3D=a[jp+p06];
				rt =a[jt+p07];	it =a[jp+p07];
				t3E=t3C-rt;		t3F=t3D-it;
				t3C=t3C+rt;		t3D=t3D+it;

				rt =t3C;		it =t3D;
				t3C=t38-rt;		t3D=t39-it;
				t38=t38+rt;		t39=t39+it;

				rt =t3E;		it =t3F;
				t3E=t3A-it;		t3F=t3B+rt;
				t3A=t3A+it;		t3B=t3B-rt;

				rt =t38;		it =t39;
				t38=t30-rt;		t39=t31-it;
				t30=t30+rt;		t31=t31+it;

				rt =t3C;		it =t3D;
				t3C=t34-it;		t3D=t35+rt;
				t34=t34+it;		t35=t35-rt;

				rt =(t3A+t3B)*ISRT2;it =(t3A-t3B)*ISRT2;
				t3A=t32-rt;		t3B=t33+it;
				t32=t32+rt;		t33=t33-it;

				rt =(t3E-t3F)*ISRT2;it =(t3F+t3E)*ISRT2;
				t3E=t36+rt;		t3F=t37+it;
				t36=t36-rt;		t37=t37-it;

			/*...and now do eight radix-4 transforms, including the internal twiddle factors:
				1, exp(-i* 1*twopi/32) =       ( c32_1,-s32_1), exp(-i* 2*twopi/32) =       ( c    ,-s    ), exp(-i* 3*twopi/32) =       ( c32_3,-s32_3) (for inputs to transform block 2),
				1, exp(-i* 2*twopi/32) =       ( c    ,-s    ), exp(-i* 4*twopi/32) = ISRT2*( 1    ,-1    ), exp(-i* 3*twopi/32) =       ( s    ,-c    ) (for inputs to transform block 3),
				1, exp(-i* 3*twopi/32) =       ( c32_3,-s32_3), exp(-i* 6*twopi/32) =       ( s    ,-c    ), exp(-i* 9*twopi/32) =       (-s32_1,-c32_1) (for inputs to transform block 4),
				1, exp(-i* 4*twopi/32) = ISRT2*( 1    ,-1    ), exp(-i* 8*twopi/32) =       ( 0    ,-1    ), exp(-i*12*twopi/32) = ISRT2*(-1    ,-1    ) (for inputs to transform block 5),
				1, exp(-i* 5*twopi/32) =       ( s32_3,-c32_3), exp(-i*10*twopi/32) =       (-s    ,-c    ), exp(-i*15*twopi/32) =       (-c32_1,-s32_1) (for inputs to transform block 6),
				1, exp(-i* 6*twopi/32) =       ( s    ,-c    ), exp(-i*12*twopi/32) = ISRT2*(-1    ,-1    ), exp(-i*18*twopi/32) =       (-c    , s    ) (for inputs to transform block 7),
				1, exp(-i* 7*twopi/32) =       ( s32_1,-c32_1), exp(-i*14*twopi/32) =       (-c    ,-s    ), exp(-i*21*twopi/32) =       (-s32_3, c32_3) (for inputs to transform block 8),
				 and only the last 3 inputs to each of the radix-4 transforms 2 through 8 are multiplied by non-unity twiddles.	*/

			/*...Block 1: t00,t10,t20,t30	*/

				rt =t10;	t10=t00-rt;	t00=t00+rt;
				it =t11;	t11=t01-it;	t01=t01+it;

				rt =t30;	t30=t20-rt;	t20=t20+rt;
				it =t31;	t31=t21-it;	t21=t21+it;

				a1p00r=t00+t20;		a1p00i=t01+t21;
				a1p10r=t00-t20;		a1p10i=t01-t21;
				a1p08r=t10+t31;		a1p08i=t11-t30;	/* mpy by E^-4 = -I is inlined here...	*/
				a1p18r=t10-t31;		a1p18i=t11+t30;

			/*...Block 5: t08,t18,t28,t38	*/

				rt =t18;	t18=t08-t19;	t08=t08+t19;		/* twiddle mpy by E^8 =-I	*/
					t19=t09+rt;	t09=t09-rt;

				rt =(t29+t28)*ISRT2;	t29=(t29-t28)*ISRT2;		t28=rt;	/* twiddle mpy by E^-4	*/
				rt =(t38-t39)*ISRT2;	it =(t38+t39)*ISRT2;			/* twiddle mpy by E^4 = -E^-12 is here...	*/
				t38=t28+rt;			t28=t28-rt;				/* ...and get E^-12 by flipping signs here.	*/
				t39=t29+it;			t29=t29-it;

				a1p04r=t08+t28;		a1p04i=t09+t29;
				a1p14r=t08-t28;		a1p14i=t09-t29;
				a1p0Cr=t18+t39;		a1p0Ci=t19-t38;	/* mpy by E^-4 = -I is inlined here...	*/
				a1p1Cr=t18-t39;		a1p1Ci=t19+t38;

			/*...Block 3: t04,t14,t24,t34	*/

				rt =(t15+t14)*ISRT2;	it =(t15-t14)*ISRT2;			/* twiddle mpy by E^-4	*/
				t14=t04-rt;			t04=t04+rt;
				t15=t05-it;			t05=t05+it;

				rt =t24*c + t25*s;		t25=t25*c - t24*s;		t24=rt;	/* twiddle mpy by E^-2	*/
				rt =t34*s + t35*c;		it =t35*s - t34*c;			/* twiddle mpy by E^-6	*/
				t34=t24-rt;			t24=t24+rt;
				t35=t25-it;			t25=t25+it;

				a1p02r=t04+t24;		a1p02i=t05+t25;
				a1p12r=t04-t24;		a1p12i=t05-t25;
				a1p0Ar=t14+t35;		a1p0Ai=t15-t34;	/* mpy by E^-4 = -I is inlined here...	*/
				a1p1Ar=t14-t35;		a1p1Ai=t15+t34;

			/*...Block 7: t0C,t1C,t2C,t3C	*/

				rt =(t1C-t1D)*ISRT2;	it =(t1C+t1D)*ISRT2;			/* twiddle mpy by E^4 = -E^-12 is here...	*/
				t1C=t0C+rt;			t0C=t0C-rt;				/* ...and get E^-12 by flipping signs here.	*/
				t1D=t0D+it;			t0D=t0D-it;

				rt =t2C*s + t2D*c;		t2D=t2D*s - t2C*c;		t2C=rt;	/* twiddle mpy by E^-6	*/
				rt =t3C*c + t3D*s;		it =t3D*c - t3C*s;			/* twiddle mpy by E^-18 is here...	*/
				t3C=t2C+rt;			t2C=t2C-rt;				/* ...and get E^-18 by flipping signs here.	*/
				t3D=t2D+it;			t2D=t2D-it;

				a1p06r=t0C+t2C;		a1p06i=t0D+t2D;
				a1p16r=t0C-t2C;		a1p16i=t0D-t2D;
				a1p0Er=t1C+t3D;		a1p0Ei=t1D-t3C;	/* mpy by E^-4 = -I is inlined here...	*/
				a1p1Er=t1C-t3D;		a1p1Ei=t1D+t3C;

			/*...Block 2: t02,t12,t22,t32	*/

				rt =t12*c + t13*s;		it =t13*c - t12*s;			/* twiddle mpy by E^-2	*/
				t12=t02-rt;			t02=t02+rt;
				t13=t03-it;			t03=t03+it;

				rt =t22*c32_1 + t23*s32_1;	t23=t23*c32_1 - t22*s32_1;	t22=rt;	/* twiddle mpy by E^-1	*/
				rt =t32*c32_3 + t33*s32_3;	it =t33*c32_3 - t32*s32_3;		/* twiddle mpy by E^-3	*/
				t32=t22-rt;			t22=t22+rt;
				t33=t23-it;			t23=t23+it;

				a1p01r=t02+t22;		a1p01i=t03+t23;
				a1p11r=t02-t22;		a1p11i=t03-t23;
				a1p09r=t12+t33;		a1p09i=t13-t32;	/* mpy by E^-4 = -I is inlined here...	*/
				a1p19r=t12-t33;		a1p19i=t13+t32;

			/*...Block 6: t0A,t1A,t2A,t3A	*/

				rt =t1A*s - t1B*c;		it =t1B*s + t1A*c;			/* twiddle mpy by -E^-10 is here...	*/
				t1A=t0A+rt;			t0A =t0A-rt;				/* ...and get E^-10 by flipping signs here.	*/
				t1B=t0B+it;			t0B =t0B-it;

				rt =t2A*s32_3 + t2B*c32_3;	t2B=t2B*s32_3 - t2A*c32_3;	t2A=rt;	/* twiddle mpy by E^-5	*/
				rt =t3A*c32_1 - t3B*s32_1;	it =t3B*c32_1 + t3A*s32_1;		/* twiddle mpy by -E^-15 is here...	*/
				t3A=t2A+rt;			t2A=t2A-rt;				/* ...and get E^-15 by flipping signs here.	*/
				t3B=t2B+it;			t2B=t2B-it;

				a1p05r=t0A+t2A;		a1p05i=t0B+t2B;
				a1p15r=t0A-t2A;		a1p15i=t0B-t2B;
				a1p0Dr=t1A+t3B;		a1p0Di=t1B-t3A;	/* mpy by E^-4 = -I is inlined here...	*/
				a1p1Dr=t1A-t3B;		a1p1Di=t1B+t3A;

			/*...Block 4: t06,t16,t26,t36	*/

				rt =t16*s + t17*c;		it =t17*s - t16*c;			/* twiddle mpy by E^-6	*/
				t16=t06-rt;			t06 =t06+rt;
				t17=t07-it;			t07 =t07+it;

				rt =t26*c32_3 + t27*s32_3;	t27=t27*c32_3 - t26*s32_3;	t26=rt;	/* twiddle mpy by E^-3	*/
				rt =t36*s32_1 - t37*c32_1;	it =t37*s32_1 + t36*c32_1;		/* twiddle mpy by -E^-9 is here...	*/
				t36=t26+rt;			t26=t26-rt;				/* ...and get E^-9 by flipping signs here.	*/
				t37=t27+it;			t27=t27-it;

				a1p03r=t06+t26;		a1p03i=t07+t27;
				a1p13r=t06-t26;		a1p13i=t07-t27;
				a1p0Br=t16+t37;		a1p0Bi=t17-t36;	/* mpy by E^-4 = -I is inlined here...	*/
				a1p1Br=t16-t37;		a1p1Bi=t17+t36;

			/*...Block 8: t0E,t1E,t2E,t3E	*/

				rt =t1E*c - t1F*s;		it =t1F*c + t1E*s;			/* twiddle mpy by -E^-14 is here...	*/
				t1E=t0E+rt;			t0E =t0E-rt;				/* ...and get E^-14 by flipping signs here.	*/
				t1F=t0F+it;			t0F =t0F-it;

				rt =t2E*s32_1 + t2F*c32_1;	t2F=t2F*s32_1 - t2E*c32_1;	t2E=rt;	/* twiddle mpy by E^-7	*/
				rt =t3E*s32_3 + t3F*c32_3;	it =t3F*s32_3 - t3E*c32_3;		/* twiddle mpy by -E^-21 is here...	*/
				t3E=t2E+rt;			t2E=t2E-rt;				/* ...and get E^-21 by flipping signs here.	*/
				t3F=t2F+it;			t2F=t2F-it;

				a1p07r=t0E+t2E;		a1p07i=t0F+t2F;
				a1p17r=t0E-t2E;		a1p17i=t0F-t2F;
				a1p0Fr=t1E+t3F;		a1p0Fi=t1F-t3E;	/* mpy by E^-4 = -I is inlined here...	*/
				a1p1Fr=t1E-t3F;		a1p1Fi=t1F+t3E;

		#endif	/* USE_SSE2 */

#ifdef CTIME
	clock3 = clock();
	dt_fwd += (double)(clock3 - clock2);
	clock2 = clock3;
#endif
		/*...Now do the carries. Since the outputs would
		normally be getting dispatched to 32 separate blocks of the A-array, we need 32 separate carries.	*/

		/************ See the radix16_ditN_cy_dif1 routine for details on how the SSE2 carry stuff works **********/
		#ifndef USE_SSE2

			if(MODULUS_TYPE == MODULUS_TYPE_MERSENNE)
			{
				l= j & (nwt-1);			/* We want (S*J mod N) - SI(L) for all 32 carries, so precompute	*/
				n_minus_sil   = n-si[l  ];		/* N - SI(L) and for each J, find N - (B*J mod N) - SI(L)		*/
				n_minus_silp1 = n-si[l+1];		/* For the inverse weight, want (S*(N - J) mod N) - SI(NWT - L) =	*/
				sinwt   = si[nwt-l  ];		/*	= N - (S*J mod N) - SI(NWT - L) = (B*J mod N) - SI(NWT - L).	*/
				sinwtm1 = si[nwt-l-1];

				wtl     =wt0[    l  ];
				wtn     =wt0[nwt-l  ]*scale;	/* Include 1/(n/2) scale factor of inverse transform here...	*/
				wtlp1   =wt0[    l+1];
				wtnm1   =wt0[nwt-l-1]*scale;	/* ...and here.	*/

				/*...set0 is slightly different from others:	*/
			   cmplx_carry_norm_pow2_errcheck0(a1p00r,a1p00i,cy_r00,bjmodn00);
				cmplx_carry_norm_pow2_errcheck(a1p01r,a1p01i,cy_r01,bjmodn01,0x01);
				cmplx_carry_norm_pow2_errcheck(a1p02r,a1p02i,cy_r02,bjmodn02,0x02);
				cmplx_carry_norm_pow2_errcheck(a1p03r,a1p03i,cy_r03,bjmodn03,0x03);
				cmplx_carry_norm_pow2_errcheck(a1p04r,a1p04i,cy_r04,bjmodn04,0x04);
				cmplx_carry_norm_pow2_errcheck(a1p05r,a1p05i,cy_r05,bjmodn05,0x05);
				cmplx_carry_norm_pow2_errcheck(a1p06r,a1p06i,cy_r06,bjmodn06,0x06);
				cmplx_carry_norm_pow2_errcheck(a1p07r,a1p07i,cy_r07,bjmodn07,0x07);
				cmplx_carry_norm_pow2_errcheck(a1p08r,a1p08i,cy_r08,bjmodn08,0x08);
				cmplx_carry_norm_pow2_errcheck(a1p09r,a1p09i,cy_r09,bjmodn09,0x09);
				cmplx_carry_norm_pow2_errcheck(a1p0Ar,a1p0Ai,cy_r0A,bjmodn0A,0x0A);
				cmplx_carry_norm_pow2_errcheck(a1p0Br,a1p0Bi,cy_r0B,bjmodn0B,0x0B);
				cmplx_carry_norm_pow2_errcheck(a1p0Cr,a1p0Ci,cy_r0C,bjmodn0C,0x0C);
				cmplx_carry_norm_pow2_errcheck(a1p0Dr,a1p0Di,cy_r0D,bjmodn0D,0x0D);
				cmplx_carry_norm_pow2_errcheck(a1p0Er,a1p0Ei,cy_r0E,bjmodn0E,0x0E);
				cmplx_carry_norm_pow2_errcheck(a1p0Fr,a1p0Fi,cy_r0F,bjmodn0F,0x0F);
				cmplx_carry_norm_pow2_errcheck(a1p10r,a1p10i,cy_r10,bjmodn10,0x10);
				cmplx_carry_norm_pow2_errcheck(a1p11r,a1p11i,cy_r11,bjmodn11,0x11);
				cmplx_carry_norm_pow2_errcheck(a1p12r,a1p12i,cy_r12,bjmodn12,0x12);
				cmplx_carry_norm_pow2_errcheck(a1p13r,a1p13i,cy_r13,bjmodn13,0x13);
				cmplx_carry_norm_pow2_errcheck(a1p14r,a1p14i,cy_r14,bjmodn14,0x14);
				cmplx_carry_norm_pow2_errcheck(a1p15r,a1p15i,cy_r15,bjmodn15,0x15);
				cmplx_carry_norm_pow2_errcheck(a1p16r,a1p16i,cy_r16,bjmodn16,0x16);
				cmplx_carry_norm_pow2_errcheck(a1p17r,a1p17i,cy_r17,bjmodn17,0x17);
				cmplx_carry_norm_pow2_errcheck(a1p18r,a1p18i,cy_r18,bjmodn18,0x18);
				cmplx_carry_norm_pow2_errcheck(a1p19r,a1p19i,cy_r19,bjmodn19,0x19);
				cmplx_carry_norm_pow2_errcheck(a1p1Ar,a1p1Ai,cy_r1A,bjmodn1A,0x1A);
				cmplx_carry_norm_pow2_errcheck(a1p1Br,a1p1Bi,cy_r1B,bjmodn1B,0x1B);
				cmplx_carry_norm_pow2_errcheck(a1p1Cr,a1p1Ci,cy_r1C,bjmodn1C,0x1C);
				cmplx_carry_norm_pow2_errcheck(a1p1Dr,a1p1Di,cy_r1D,bjmodn1D,0x1D);
				cmplx_carry_norm_pow2_errcheck(a1p1Er,a1p1Ei,cy_r1E,bjmodn1E,0x1E);
				cmplx_carry_norm_pow2_errcheck(a1p1Fr,a1p1Fi,cy_r1F,bjmodn1F,0x1F);

				i =((uint32)(sw - bjmodn00) >> 31);	/* get ready for the next set...	*/
				co2=co3;	/* For all data but the first set in each j-block, co2=co3. Thus, after the first block of data is done
						 and only then: for all subsequent blocks it's superfluous), this assignment decrements co2 by radix(1).	*/
			}
			else
			{
				fermat_carry_norm_pow2_errcheck(a1p00r,a1p00i,cy_r00,cy_i00,0x00*NDIVR,NRTM1,NRT_BITS);
				fermat_carry_norm_pow2_errcheck(a1p01r,a1p01i,cy_r01,cy_i01,0x01*NDIVR,NRTM1,NRT_BITS);
				fermat_carry_norm_pow2_errcheck(a1p02r,a1p02i,cy_r02,cy_i02,0x02*NDIVR,NRTM1,NRT_BITS);
				fermat_carry_norm_pow2_errcheck(a1p03r,a1p03i,cy_r03,cy_i03,0x03*NDIVR,NRTM1,NRT_BITS);
				fermat_carry_norm_pow2_errcheck(a1p04r,a1p04i,cy_r04,cy_i04,0x04*NDIVR,NRTM1,NRT_BITS);
				fermat_carry_norm_pow2_errcheck(a1p05r,a1p05i,cy_r05,cy_i05,0x05*NDIVR,NRTM1,NRT_BITS);
				fermat_carry_norm_pow2_errcheck(a1p06r,a1p06i,cy_r06,cy_i06,0x06*NDIVR,NRTM1,NRT_BITS);
				fermat_carry_norm_pow2_errcheck(a1p07r,a1p07i,cy_r07,cy_i07,0x07*NDIVR,NRTM1,NRT_BITS);
				fermat_carry_norm_pow2_errcheck(a1p08r,a1p08i,cy_r08,cy_i08,0x08*NDIVR,NRTM1,NRT_BITS);
				fermat_carry_norm_pow2_errcheck(a1p09r,a1p09i,cy_r09,cy_i09,0x09*NDIVR,NRTM1,NRT_BITS);
				fermat_carry_norm_pow2_errcheck(a1p0Ar,a1p0Ai,cy_r0A,cy_i0A,0x0A*NDIVR,NRTM1,NRT_BITS);
				fermat_carry_norm_pow2_errcheck(a1p0Br,a1p0Bi,cy_r0B,cy_i0B,0x0B*NDIVR,NRTM1,NRT_BITS);
				fermat_carry_norm_pow2_errcheck(a1p0Cr,a1p0Ci,cy_r0C,cy_i0C,0x0C*NDIVR,NRTM1,NRT_BITS);
				fermat_carry_norm_pow2_errcheck(a1p0Dr,a1p0Di,cy_r0D,cy_i0D,0x0D*NDIVR,NRTM1,NRT_BITS);
				fermat_carry_norm_pow2_errcheck(a1p0Er,a1p0Ei,cy_r0E,cy_i0E,0x0E*NDIVR,NRTM1,NRT_BITS);
				fermat_carry_norm_pow2_errcheck(a1p0Fr,a1p0Fi,cy_r0F,cy_i0F,0x0F*NDIVR,NRTM1,NRT_BITS);
				fermat_carry_norm_pow2_errcheck(a1p10r,a1p10i,cy_r10,cy_i10,0x10*NDIVR,NRTM1,NRT_BITS);
				fermat_carry_norm_pow2_errcheck(a1p11r,a1p11i,cy_r11,cy_i11,0x11*NDIVR,NRTM1,NRT_BITS);
				fermat_carry_norm_pow2_errcheck(a1p12r,a1p12i,cy_r12,cy_i12,0x12*NDIVR,NRTM1,NRT_BITS);
				fermat_carry_norm_pow2_errcheck(a1p13r,a1p13i,cy_r13,cy_i13,0x13*NDIVR,NRTM1,NRT_BITS);
				fermat_carry_norm_pow2_errcheck(a1p14r,a1p14i,cy_r14,cy_i14,0x14*NDIVR,NRTM1,NRT_BITS);
				fermat_carry_norm_pow2_errcheck(a1p15r,a1p15i,cy_r15,cy_i15,0x15*NDIVR,NRTM1,NRT_BITS);
				fermat_carry_norm_pow2_errcheck(a1p16r,a1p16i,cy_r16,cy_i16,0x16*NDIVR,NRTM1,NRT_BITS);
				fermat_carry_norm_pow2_errcheck(a1p17r,a1p17i,cy_r17,cy_i17,0x17*NDIVR,NRTM1,NRT_BITS);
				fermat_carry_norm_pow2_errcheck(a1p18r,a1p18i,cy_r18,cy_i18,0x18*NDIVR,NRTM1,NRT_BITS);
				fermat_carry_norm_pow2_errcheck(a1p19r,a1p19i,cy_r19,cy_i19,0x19*NDIVR,NRTM1,NRT_BITS);
				fermat_carry_norm_pow2_errcheck(a1p1Ar,a1p1Ai,cy_r1A,cy_i1A,0x1A*NDIVR,NRTM1,NRT_BITS);
				fermat_carry_norm_pow2_errcheck(a1p1Br,a1p1Bi,cy_r1B,cy_i1B,0x1B*NDIVR,NRTM1,NRT_BITS);
				fermat_carry_norm_pow2_errcheck(a1p1Cr,a1p1Ci,cy_r1C,cy_i1C,0x1C*NDIVR,NRTM1,NRT_BITS);
				fermat_carry_norm_pow2_errcheck(a1p1Dr,a1p1Di,cy_r1D,cy_i1D,0x1D*NDIVR,NRTM1,NRT_BITS);
				fermat_carry_norm_pow2_errcheck(a1p1Er,a1p1Ei,cy_r1E,cy_i1E,0x1E*NDIVR,NRTM1,NRT_BITS);
				fermat_carry_norm_pow2_errcheck(a1p1Fr,a1p1Fi,cy_r1F,cy_i1F,0x1F*NDIVR,NRTM1,NRT_BITS);
			}

		#else	/* USE_SSE2 = true: */

			if(MODULUS_TYPE == MODULUS_TYPE_MERSENNE)
			{
				l= j & (nwt-1);			/* We want (S*J mod N) - SI(L) for all 32 carries, so precompute	*/
				n_minus_sil   = n-si[l  ];		/* N - SI(L) and for each J, find N - (B*J mod N) - SI(L)		*/
				n_minus_silp1 = n-si[l+1];		/* For the inverse weight, want (S*(N - J) mod N) - SI(NWT - L) =	*/
				sinwt   = si[nwt-l  ];		/*	= N - (S*J mod N) - SI(NWT - L) = (B*J mod N) - SI(NWT - L).	*/
				sinwtm1 = si[nwt-l-1];

				wtl     =wt0[    l  ];
				wtn     =wt0[nwt-l  ]*scale;	/* Include 1/(n/2) scale factor of inverse transform here...	*/
				wtlp1   =wt0[    l+1];
				wtnm1   =wt0[nwt-l-1]*scale;	/* ...and here.	*/

				tmp = half_arr + 16;	/* ptr to local storage for the doubled wtl,wtn terms: */
				tmp->re = wtl;		tmp->im = wtl;	++tmp;
				tmp->re = wtn;		tmp->im = wtn;	++tmp;
				tmp->re = wtlp1;	tmp->im = wtlp1;++tmp;
				tmp->re = wtnm1;	tmp->im = wtnm1;

				add0 = &wt1[col  ];
				add1 = &wt1[co2-1];
				add2 = &wt1[co3-1];

			#if defined(COMPILER_TYPE_MSVC)

			  #ifdef ERR_CHECK_ALL
				SSE2_cmplx_carry_norm_pow2_errcheck0_2B(r00,add0,add1,add2,cy_r00,cy_r02,bjmodn00);
				SSE2_cmplx_carry_norm_pow2_errcheck1_2B(r08,add0,add1,add2,cy_r04,cy_r06,bjmodn04);
				SSE2_cmplx_carry_norm_pow2_errcheck1_2B(r10,add0,add1,add2,cy_r08,cy_r0A,bjmodn08);
				SSE2_cmplx_carry_norm_pow2_errcheck1_2B(r18,add0,add1,add2,cy_r0C,cy_r0E,bjmodn0C);
				SSE2_cmplx_carry_norm_pow2_errcheck1_2B(r20,add0,add1,add2,cy_r10,cy_r12,bjmodn10);
				SSE2_cmplx_carry_norm_pow2_errcheck1_2B(r28,add0,add1,add2,cy_r14,cy_r16,bjmodn14);
				SSE2_cmplx_carry_norm_pow2_errcheck1_2B(r30,add0,add1,add2,cy_r18,cy_r1A,bjmodn18);
				SSE2_cmplx_carry_norm_pow2_errcheck1_2B(r38,add0,add1,add2,cy_r1C,cy_r1E,bjmodn1C);
			  #else
				SSE2_cmplx_carry_norm_pow2_errcheck0_2B(r00,add0,add1,add2,cy_r00,cy_r02,bjmodn00);
				SSE2_cmplx_carry_norm_pow2_nocheck1_2B (r08,add0,add1,add2,cy_r04,cy_r06,bjmodn04);
				SSE2_cmplx_carry_norm_pow2_nocheck1_2B (r10,add0,add1,add2,cy_r08,cy_r0A,bjmodn08);
				SSE2_cmplx_carry_norm_pow2_nocheck1_2B (r18,add0,add1,add2,cy_r0C,cy_r0E,bjmodn0C);
				SSE2_cmplx_carry_norm_pow2_nocheck1_2B (r20,add0,add1,add2,cy_r10,cy_r12,bjmodn10);
				SSE2_cmplx_carry_norm_pow2_nocheck1_2B (r28,add0,add1,add2,cy_r14,cy_r16,bjmodn14);
				SSE2_cmplx_carry_norm_pow2_nocheck1_2B (r30,add0,add1,add2,cy_r18,cy_r1A,bjmodn18);
				SSE2_cmplx_carry_norm_pow2_nocheck1_2B (r38,add0,add1,add2,cy_r1C,cy_r1E,bjmodn1C);
			  #endif

			#else	/* GCC-style inline ASM: */

			  #ifdef ERR_CHECK_ALL
				SSE2_cmplx_carry_norm_pow2_errcheck0_2B(r00,add0,add1,add2,cy_r00,cy_r02,bjmodn00,half_arr,i,n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_errcheck1_2B(r08,add0,add1,add2,cy_r04,cy_r06,bjmodn04,half_arr,  n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_errcheck1_2B(r10,add0,add1,add2,cy_r08,cy_r0A,bjmodn08,half_arr,  n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_errcheck1_2B(r18,add0,add1,add2,cy_r0C,cy_r0E,bjmodn0C,half_arr,  n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_errcheck1_2B(r20,add0,add1,add2,cy_r10,cy_r12,bjmodn10,half_arr,  n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_errcheck1_2B(r28,add0,add1,add2,cy_r14,cy_r16,bjmodn14,half_arr,  n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_errcheck1_2B(r30,add0,add1,add2,cy_r18,cy_r1A,bjmodn18,half_arr,  n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_errcheck1_2B(r38,add0,add1,add2,cy_r1C,cy_r1E,bjmodn1C,half_arr,  n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
			  #else
				SSE2_cmplx_carry_norm_pow2_errcheck0_2B(r00,add0,add1,add2,cy_r00,cy_r02,bjmodn00,half_arr,i,n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_nocheck1_2B (r08,add0,add1,add2,cy_r04,cy_r06,bjmodn04,half_arr,  n_minus_silp1,n_minus_sil,          sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_nocheck1_2B (r10,add0,add1,add2,cy_r08,cy_r0A,bjmodn08,half_arr,  n_minus_silp1,n_minus_sil,          sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_nocheck1_2B (r18,add0,add1,add2,cy_r0C,cy_r0E,bjmodn0C,half_arr,  n_minus_silp1,n_minus_sil,          sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_nocheck1_2B (r20,add0,add1,add2,cy_r10,cy_r12,bjmodn10,half_arr,  n_minus_silp1,n_minus_sil,          sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_nocheck1_2B (r28,add0,add1,add2,cy_r14,cy_r16,bjmodn14,half_arr,  n_minus_silp1,n_minus_sil,          sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_nocheck1_2B (r30,add0,add1,add2,cy_r18,cy_r1A,bjmodn18,half_arr,  n_minus_silp1,n_minus_sil,          sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_nocheck1_2B (r38,add0,add1,add2,cy_r1C,cy_r1E,bjmodn1C,half_arr,  n_minus_silp1,n_minus_sil,          sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
			  #endif

				/* Bizarre - when I disabled the diagnostic prints above and below, the resulting GCC build immediately gave
					fatal roundoff errors starting on iteration #5 - so insert the bogus [never taken] if() here as a workaround.
					Equally bizarre, inserting the bogus if() *before* the 4 carry-macro calls above gave the correct result as well,
					but ran fully 10% slower. Good old GCC...
				Dec 2011: Suspect this was a side effect of my gcc asm macros not including cc/memory in the clobber list, because
				the code now runs correctly without this hack ... unlike radix-28, the radix-36 carry code runs sign. faster without it.
				if(j < 0)
				{
					fprintf(stderr, "Iter %3d\n",iter);
				}
				*/

			#endif

				l= (j+2) & (nwt-1);			/* We want (S*J mod N) - SI(L) for all 16 carries, so precompute	*/
				n_minus_sil   = n-si[l  ];		/* N - SI(L) and for each J, find N - (B*J mod N) - SI(L)		*/
				n_minus_silp1 = n-si[l+1];		/* For the inverse weight, want (S*(N - J) mod N) - SI(NWT - L) =	*/
				sinwt   = si[nwt-l  ];		/*	= N - (S*J mod N) - SI(NWT - L) = (B*J mod N) - SI(NWT - L).	*/
				sinwtm1 = si[nwt-l-1];

				wtl     =wt0[    l  ];
				wtn     =wt0[nwt-l  ]*scale;	/* Include 1/(n/2) scale factor of inverse transform here...	*/
				wtlp1   =wt0[    l+1];
				wtnm1   =wt0[nwt-l-1]*scale;	/* ...and here.	*/

				tmp = half_arr + 16;	/* ptr to localstorage for the doubled wtl,wtn terms: */
				tmp->re = wtl;		tmp->im = wtl;	++tmp;
				tmp->re = wtn;		tmp->im = wtn;	++tmp;
				tmp->re = wtlp1;	tmp->im = wtlp1;++tmp;
				tmp->re = wtnm1;	tmp->im = wtnm1;

			/*	i =((uint32)(sw - *bjmodn0) >> 31);	Don't need this here, since no special index-0 macro in the set below */

				co2 = co3;	/* For all data but the first set in each j-block, co2=co3. Thus, after the first block of data is done
							(and only then: for all subsequent blocks it's superfluous), this assignment decrements co2 by radix(1).	*/

				add0 = &wt1[col  ];
				add1 = &wt1[co2-1];

			#if defined(COMPILER_TYPE_MSVC)

			  #ifdef ERR_CHECK_ALL
				SSE2_cmplx_carry_norm_pow2_errcheck2_2B(r00,add0,add1,     cy_r00,cy_r02,bjmodn00);
				SSE2_cmplx_carry_norm_pow2_errcheck2_2B(r08,add0,add1,     cy_r04,cy_r06,bjmodn04);
				SSE2_cmplx_carry_norm_pow2_errcheck2_2B(r10,add0,add1,     cy_r08,cy_r0A,bjmodn08);
				SSE2_cmplx_carry_norm_pow2_errcheck2_2B(r18,add0,add1,     cy_r0C,cy_r0E,bjmodn0C);
				SSE2_cmplx_carry_norm_pow2_errcheck2_2B(r20,add0,add1,     cy_r10,cy_r12,bjmodn10);
				SSE2_cmplx_carry_norm_pow2_errcheck2_2B(r28,add0,add1,     cy_r14,cy_r16,bjmodn14);
				SSE2_cmplx_carry_norm_pow2_errcheck2_2B(r30,add0,add1,     cy_r18,cy_r1A,bjmodn18);
				SSE2_cmplx_carry_norm_pow2_errcheck2_2B(r38,add0,add1,     cy_r1C,cy_r1E,bjmodn1C);
			  #else
				SSE2_cmplx_carry_norm_pow2_nocheck2_2B (r00,add0,add1,     cy_r00,cy_r02,bjmodn00);
				SSE2_cmplx_carry_norm_pow2_nocheck2_2B (r08,add0,add1,     cy_r04,cy_r06,bjmodn04);
				SSE2_cmplx_carry_norm_pow2_nocheck2_2B (r10,add0,add1,     cy_r08,cy_r0A,bjmodn08);
				SSE2_cmplx_carry_norm_pow2_nocheck2_2B (r18,add0,add1,     cy_r0C,cy_r0E,bjmodn0C);
				SSE2_cmplx_carry_norm_pow2_nocheck2_2B (r20,add0,add1,     cy_r10,cy_r12,bjmodn10);
				SSE2_cmplx_carry_norm_pow2_nocheck2_2B (r28,add0,add1,     cy_r14,cy_r16,bjmodn14);
				SSE2_cmplx_carry_norm_pow2_nocheck2_2B (r30,add0,add1,     cy_r18,cy_r1A,bjmodn18);
				SSE2_cmplx_carry_norm_pow2_nocheck2_2B (r38,add0,add1,     cy_r1C,cy_r1E,bjmodn1C);
			  #endif

			#else	/* GCC-style inline ASM: */

			  #ifdef ERR_CHECK_ALL
				SSE2_cmplx_carry_norm_pow2_errcheck2_2B(r00,add0,add1,     cy_r00,cy_r02,bjmodn00,half_arr,n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_errcheck2_2B(r08,add0,add1,     cy_r04,cy_r06,bjmodn04,half_arr,n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_errcheck2_2B(r10,add0,add1,     cy_r08,cy_r0A,bjmodn08,half_arr,n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_errcheck2_2B(r18,add0,add1,     cy_r0C,cy_r0E,bjmodn0C,half_arr,n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_errcheck2_2B(r20,add0,add1,     cy_r10,cy_r12,bjmodn10,half_arr,n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_errcheck2_2B(r28,add0,add1,     cy_r14,cy_r16,bjmodn14,half_arr,n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_errcheck2_2B(r30,add0,add1,     cy_r18,cy_r1A,bjmodn18,half_arr,n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_errcheck2_2B(r38,add0,add1,     cy_r1C,cy_r1E,bjmodn1C,half_arr,n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
			  #else
				SSE2_cmplx_carry_norm_pow2_nocheck2_2B (r00,add0,add1,     cy_r00,cy_r02,bjmodn00,half_arr,n_minus_silp1,n_minus_sil,          sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_nocheck2_2B (r08,add0,add1,     cy_r04,cy_r06,bjmodn04,half_arr,n_minus_silp1,n_minus_sil,          sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_nocheck2_2B (r10,add0,add1,     cy_r08,cy_r0A,bjmodn08,half_arr,n_minus_silp1,n_minus_sil,          sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_nocheck2_2B (r18,add0,add1,     cy_r0C,cy_r0E,bjmodn0C,half_arr,n_minus_silp1,n_minus_sil,          sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_nocheck2_2B (r20,add0,add1,     cy_r10,cy_r12,bjmodn10,half_arr,n_minus_silp1,n_minus_sil,          sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_nocheck2_2B (r28,add0,add1,     cy_r14,cy_r16,bjmodn14,half_arr,n_minus_silp1,n_minus_sil,          sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_nocheck2_2B (r30,add0,add1,     cy_r18,cy_r1A,bjmodn18,half_arr,n_minus_silp1,n_minus_sil,          sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_nocheck2_2B (r38,add0,add1,     cy_r1C,cy_r1E,bjmodn1C,half_arr,n_minus_silp1,n_minus_sil,          sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
			  #endif

			#endif

				i =((uint32)(sw - *bjmodn00) >> 31);	/* get ready for the next set...	*/

			}
			else	/* Fermat-mod carry in SSE2 mode */
			{
				tmp = half_arr+2;
				tmp->re = tmp->im = scale;
				/* Get the needed Nth root of -1: */
				add1 = &rn0[0];
				add2 = &rn1[0];

				idx_offset = j;
				idx_incr = NDIVR;

			#if defined(COMPILER_TYPE_MSVC)
				/* The cy_[r|i]_idx[A|B] names here are not meaningful, each simple stores one [re,im] carry pair,
				e.g. cy_r01 stores the carries our of [a0.re,a0.im], cy_r23 stores the carries our of [a1.re,a1.im], etc.
				Here is the actual mapping between these SSE2-mode 2-vector carry pairs and the scalar carries:
													  2-vector                               Scalar
													 ----------                            ----------- */
				SSE2_fermat_carry_norm_pow2_errcheck(r00,cy_r00,idx_offset,idx_incr);	/* cy_r00,cy_i00 */
				SSE2_fermat_carry_norm_pow2_errcheck(r02,cy_r02,idx_offset,idx_incr);	/* cy_r01,cy_i01 */
				SSE2_fermat_carry_norm_pow2_errcheck(r04,cy_r04,idx_offset,idx_incr);	/* cy_r02,cy_i02 */
				SSE2_fermat_carry_norm_pow2_errcheck(r06,cy_r06,idx_offset,idx_incr);	/* cy_r03,cy_i03 */
				SSE2_fermat_carry_norm_pow2_errcheck(r08,cy_r08,idx_offset,idx_incr);	/* cy_r04,cy_i04 */
				SSE2_fermat_carry_norm_pow2_errcheck(r0A,cy_r0A,idx_offset,idx_incr);	/* cy_r05,cy_i05 */
				SSE2_fermat_carry_norm_pow2_errcheck(r0C,cy_r0C,idx_offset,idx_incr);	/* cy_r06,cy_i06 */
				SSE2_fermat_carry_norm_pow2_errcheck(r0E,cy_r0E,idx_offset,idx_incr);	/* cy_r07,cy_i07 */
				SSE2_fermat_carry_norm_pow2_errcheck(r10,cy_r10,idx_offset,idx_incr);	/* cy_r08,cy_i08 */
				SSE2_fermat_carry_norm_pow2_errcheck(r12,cy_r12,idx_offset,idx_incr);	/* cy_r09,cy_i09 */
				SSE2_fermat_carry_norm_pow2_errcheck(r14,cy_r14,idx_offset,idx_incr);	/* cy_r0A,cy_i0A */
				SSE2_fermat_carry_norm_pow2_errcheck(r16,cy_r16,idx_offset,idx_incr);	/* cy_r0B,cy_i0B */
				SSE2_fermat_carry_norm_pow2_errcheck(r18,cy_r18,idx_offset,idx_incr);	/* cy_r0C,cy_i0C */
				SSE2_fermat_carry_norm_pow2_errcheck(r1A,cy_r1A,idx_offset,idx_incr);	/* cy_r0D,cy_i0D */
				SSE2_fermat_carry_norm_pow2_errcheck(r1C,cy_r1C,idx_offset,idx_incr);	/* cy_r0E,cy_i0E */
				SSE2_fermat_carry_norm_pow2_errcheck(r1E,cy_r1E,idx_offset,idx_incr);	/* cy_r0F,cy_i0F */
				SSE2_fermat_carry_norm_pow2_errcheck(r20,cy_i00,idx_offset,idx_incr);	/* cy_r10,cy_i10 */
				SSE2_fermat_carry_norm_pow2_errcheck(r22,cy_i02,idx_offset,idx_incr);	/* cy_r11,cy_i11 */
				SSE2_fermat_carry_norm_pow2_errcheck(r24,cy_i04,idx_offset,idx_incr);	/* cy_r12,cy_i12 */
				SSE2_fermat_carry_norm_pow2_errcheck(r26,cy_i06,idx_offset,idx_incr);	/* cy_r13,cy_i13 */
				SSE2_fermat_carry_norm_pow2_errcheck(r28,cy_i08,idx_offset,idx_incr);	/* cy_r14,cy_i14 */
				SSE2_fermat_carry_norm_pow2_errcheck(r2A,cy_i0A,idx_offset,idx_incr);	/* cy_r15,cy_i15 */
				SSE2_fermat_carry_norm_pow2_errcheck(r2C,cy_i0C,idx_offset,idx_incr);	/* cy_r16,cy_i16 */
				SSE2_fermat_carry_norm_pow2_errcheck(r2E,cy_i0E,idx_offset,idx_incr);	/* cy_r17,cy_i17 */
				SSE2_fermat_carry_norm_pow2_errcheck(r30,cy_i10,idx_offset,idx_incr);	/* cy_r18,cy_i18 */
				SSE2_fermat_carry_norm_pow2_errcheck(r32,cy_i12,idx_offset,idx_incr);	/* cy_r19,cy_i19 */
				SSE2_fermat_carry_norm_pow2_errcheck(r34,cy_i14,idx_offset,idx_incr);	/* cy_r1A,cy_i1A */
				SSE2_fermat_carry_norm_pow2_errcheck(r36,cy_i16,idx_offset,idx_incr);	/* cy_r1B,cy_i1B */
				SSE2_fermat_carry_norm_pow2_errcheck(r38,cy_i18,idx_offset,idx_incr);	/* cy_r1C,cy_i1C */
				SSE2_fermat_carry_norm_pow2_errcheck(r3A,cy_i1A,idx_offset,idx_incr);	/* cy_r1D,cy_i1D */
				SSE2_fermat_carry_norm_pow2_errcheck(r3C,cy_i1C,idx_offset,idx_incr);	/* cy_r1E,cy_i1E */
				SSE2_fermat_carry_norm_pow2_errcheck(r3E,cy_i1E,idx_offset,idx_incr);	/* cy_r1F,cy_i1F */
			#else
			  #if (OS_BITS == 32) || !defined(USE_64BIT_ASM_STYLE)	// In 64-bit mode, default is to use simple 64-bit-ified version of the analogous 32-bit 
				SSE2_fermat_carry_norm_pow2_errcheck(r00,cy_r00,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r02,cy_r02,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r04,cy_r04,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r06,cy_r06,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r08,cy_r08,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r0A,cy_r0A,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r0C,cy_r0C,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r0E,cy_r0E,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r10,cy_r10,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r12,cy_r12,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r14,cy_r14,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r16,cy_r16,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r18,cy_r18,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r1A,cy_r1A,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r1C,cy_r1C,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r1E,cy_r1E,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r20,cy_i00,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r22,cy_i02,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r24,cy_i04,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r26,cy_i06,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r28,cy_i08,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r2A,cy_i0A,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r2C,cy_i0C,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r2E,cy_i0E,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r30,cy_i10,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r32,cy_i12,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r34,cy_i14,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r36,cy_i16,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r38,cy_i18,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r3A,cy_i1A,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r3C,cy_i1C,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r3E,cy_i1E,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
			  #else
				SSE2_fermat_carry_norm_pow2_errcheck_X2(r00,cy_r00,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck_X2(r04,cy_r04,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck_X2(r08,cy_r08,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck_X2(r0C,cy_r0C,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck_X2(r10,cy_r10,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck_X2(r14,cy_r14,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck_X2(r18,cy_r18,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck_X2(r1C,cy_r1C,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck_X2(r20,cy_i00,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck_X2(r24,cy_i04,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck_X2(r28,cy_i08,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck_X2(r2C,cy_i0C,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck_X2(r30,cy_i10,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck_X2(r34,cy_i14,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck_X2(r38,cy_i18,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck_X2(r3C,cy_i1C,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
			  #endif
			#endif

			}

		#endif	/* #ifdef USE_SSE2 */

#ifdef CTIME
	clock3 = clock();
	dt_cy += (double)(clock3 - clock2);
	clock2 = clock3;
#endif
		#ifdef USE_SSE2

		/* SSE2_RADIX32_DIF_NOTWIDDLE: */

		/* Disable original MSVC inline asm in favor of macro: */
		#if 0//def COMPILER_TYPE_MSVC

		/* gather the needed data (32 64-bit complex, i.e. 64 64-bit reals) and do the first set of four length-8 transforms...
		   We process the sincos data in bit-reversed order.	*/

			SSE2_RADIX4_DIF_IN_PLACE         (r00,r20,r10,r30)
			SSE2_RADIX4_DIF_IN_PLACE_2NDOFTWO(r08,r28,r18,r38)
			SSE2_RADIX8_DIF_COMBINE_RAD4_SUBS(r00,r10,r20,r30,r08,r18,r28,r38)

			SSE2_RADIX4_DIF_IN_PLACE         (r04,r24,r14,r34)
			SSE2_RADIX4_DIF_IN_PLACE_2NDOFTWO(r0C,r2C,r1C,r3C)
			SSE2_RADIX8_DIF_COMBINE_RAD4_SUBS(r04,r14,r24,r34,r0C,r1C,r2C,r3C)

			SSE2_RADIX4_DIF_IN_PLACE         (r02,r22,r12,r32)
			SSE2_RADIX4_DIF_IN_PLACE_2NDOFTWO(r0A,r2A,r1A,r3A)
			SSE2_RADIX8_DIF_COMBINE_RAD4_SUBS(r02,r12,r22,r32,r0A,r1A,r2A,r3A)

			SSE2_RADIX4_DIF_IN_PLACE         (r06,r26,r16,r36)
			SSE2_RADIX4_DIF_IN_PLACE_2NDOFTWO(r0E,r2E,r1E,r3E)
			SSE2_RADIX8_DIF_COMBINE_RAD4_SUBS(r06,r16,r26,r36,r0E,r1E,r2E,r3E)

		/*...and now do eight radix-4 transforms, including the internal twiddle factors: */

		/*...Block 1: t00,t10,t20,t30 in r00,04,02,06 - note swapped middle 2 indices! */

			jt = j1;
			jp = j2;
			add0 = &a[jt];
			add1 = add0+p01;
			add2 = add0+p02;
			add3 = add0+p03;

			__asm	mov	eax, r00

			__asm	movaps	xmm0,[eax      ]	/* t00 */				__asm	movaps	xmm4,[eax+0x020]	/* t20 */
			__asm	movaps	xmm1,[eax+0x010]	/* t01 */				__asm	movaps	xmm5,[eax+0x030]	/* t21 */
			__asm	movaps	xmm2,[eax+0x040]	/* t10 */				__asm	movaps	xmm6,[eax+0x060]	/* t30 */
			__asm	movaps	xmm3,[eax+0x050]	/* t11 */				__asm	movaps	xmm7,[eax+0x070]	/* t31 */

			__asm	subpd	xmm0,[eax+0x040]	/* t10=t00-rt */		__asm	subpd	xmm4,[eax+0x060]	/* t30=t20-rt */
			__asm	subpd	xmm1,[eax+0x050]	/* t11=t01-it */		__asm	subpd	xmm5,[eax+0x070]	/* t31=t21-it */
			__asm	addpd	xmm2,[eax      ]	/* t00=t00+rt */		__asm	addpd	xmm6,[eax+0x020]	/* t20=t20+rt */
			__asm	addpd	xmm3,[eax+0x010]	/* t01=t01+it */		__asm	addpd	xmm7,[eax+0x030]	/* t21=t21+it */

			__asm	mov	eax, add0	/* restore main-array indices */
			__asm	mov	ebx, add1
			__asm	mov	ecx, add2
			__asm	mov	edx, add3

			__asm	subpd	xmm2,xmm6		/* t00 <- t00-t20 */		__asm	subpd	xmm0,xmm5		/* t10 <- t10-t31 */
			__asm	subpd	xmm3,xmm7		/* t01 <- t01-t21 */		__asm	subpd	xmm1,xmm4		/* t11 <- t11-t30 */
			__asm	addpd	xmm6,xmm6		/*          2*t20 */		__asm	addpd	xmm5,xmm5		/*          2*t31 */
			__asm	addpd	xmm7,xmm7		/*          2*t21 */		__asm	addpd	xmm4,xmm4		/*          2*t30 */
			__asm	movaps	[ebx     ],xmm2	/* a[jt+p1 ] */				__asm	movaps	[ecx     ],xmm0	/* a[jt+p2 ] */
			__asm	movaps	[ebx+0x10],xmm3	/* a[jp+p1 ] */				__asm	movaps	[edx+0x10],xmm1	/* a[jp+p3 ] */
			__asm	addpd	xmm6,xmm2		/* t20 <- t00+t20 */		__asm	addpd	xmm5,xmm0		/* t31 <- t10+t31 */
			__asm	addpd	xmm7,xmm3		/* t21 <- t01+t21 */		__asm	addpd	xmm4,xmm1		/* t30 <- t11+t30 */
			__asm	movaps	[eax     ],xmm6	/* a[jt+p0 ] */				__asm	movaps	[edx     ],xmm5	/* a[jt+p3 ] */
			__asm	movaps	[eax+0x10],xmm7	/* a[jp+p0 ] */				__asm	movaps	[ecx+0x10],xmm4	/* a[jp+p2 ] */

		/*...Block 5: t08,t18,t28,t38	*/

			jt = j1 + p04;
			jp = j2 + p04;
			add0 = &a[jt];
			add1 = add0+p01;
			add2 = add0+p02;
			add3 = add0+p03;

			__asm	mov	eax, r08
			__asm	mov	ebx, isrt2

																		__asm	movaps	xmm4,[eax+0x020]	/* t28 */
																		__asm	movaps	xmm5,[eax+0x030]	/* t29 */
																		__asm	movaps	xmm6,[eax+0x060]	/* t38 */
																		__asm	movaps	xmm7,[eax+0x070]	/* t39 */

			__asm	movaps	xmm0,[eax      ]	/* t08 */				__asm	subpd	xmm4,[eax+0x030]	/* t28-t29 */
			__asm	movaps	xmm1,[eax+0x010]	/* t09 */				__asm	addpd	xmm5,[eax+0x020]	/* t29+t28 */
			__asm	movaps	xmm2,[eax+0x040]	/* t18 */				__asm	mulpd	xmm4,[ebx]	/* t28 = (t28-t29)*ISRT2 */
			__asm	movaps	xmm3,[eax+0x050]	/* t19 */				__asm	mulpd	xmm5,[ebx]	/* t29 = (t29+t28)*ISRT2 */

			__asm	subpd	xmm0,[eax+0x050]	/* t08=t08-t19*/		__asm	addpd	xmm6,[eax+0x070]	/* t38+t39 */
			__asm	subpd	xmm1,[eax+0x040]	/* t19=t09-t18*/		__asm	subpd	xmm7,[eax+0x060]	/* t39-t38 */
			__asm	addpd	xmm2,[eax+0x010]	/* t09=t18+t09*/		__asm	mulpd	xmm6,[ebx]	/*  rt = (t38+t39)*ISRT2 */
			__asm	addpd	xmm3,[eax      ]	/* t18=t19+t08*/		__asm	mulpd	xmm7,[ebx]	/*  it = (t39-t38)*ISRT2 */

																		__asm	subpd	xmm4,xmm6		/* t28=t28-rt */
																		__asm	subpd	xmm5,xmm7		/* t29=t29-it */
																		__asm	addpd	xmm6,xmm6		/*      2* rt */
																		__asm	addpd	xmm7,xmm7		/*      2* it */
																		__asm	addpd	xmm6,xmm4		/* t38=t28+rt */
																		__asm	addpd	xmm7,xmm5		/* t39=t29+it */

			__asm	subpd	xmm0,xmm4		/* t08-t28 */
			__asm	subpd	xmm2,xmm5		/* t09-t29 */
			__asm	addpd	xmm4,xmm4		/*   2*t28 */
			__asm	addpd	xmm5,xmm5		/*   2*t29 */

			__asm	mov	eax, add0	/* restore main-array indices */
			__asm	mov	ebx, add1
			__asm	mov	ecx, add2
			__asm	mov	edx, add3
																	__asm	subpd	xmm3,xmm7		/* t18-t39 */
																	__asm	subpd	xmm1,xmm6		/* t19-t38 */
																	__asm	addpd	xmm7,xmm7		/*   2*t39 */
			__asm	movaps	[ebx     ],xmm0	/* a[jt+p1 ] */			__asm	addpd	xmm6,xmm6		/*   2*t38 */
			__asm	movaps	[ebx+0x10],xmm2	/* a[jp+p1 ] */			__asm	movaps	[ecx     ],xmm3	/* a[jt+p2 ] */
			__asm	addpd	xmm4,xmm0		/* t08+t28 */			__asm	movaps	[edx+0x10],xmm1	/* a[jp+p3 ] */
			__asm	addpd	xmm5,xmm2		/* t09+t29 */			__asm	addpd	xmm7,xmm3		/* t18+t39 */
			__asm	movaps	[eax     ],xmm4	/* a[jt+p0 ] */			__asm	addpd	xmm6,xmm1		/* t19+t38 */
			__asm	movaps	[eax+0x10],xmm5	/* a[jp+p0 ] */			__asm	movaps	[edx     ],xmm7	/* a[jt+p3 ] */
																	__asm	movaps	[ecx+0x10],xmm6	/* a[jp+p2 ] */
		/*...Block 3: t04,t14,t24,t34	*/

			jt = j1 + p08;
			jp = j2 + p08;
			add0 = &a[jt];
			add1 = add0+p01;
			add2 = add0+p02;
			add3 = add0+p03;

			__asm	mov	eax, r20
			__asm	mov	ebx, cc0

			__asm	movaps	xmm4,[eax+0x020]	/* t24 */		__asm	movaps	xmm6,[eax+0x060]	/* t34 */
			__asm	movaps	xmm5,[eax+0x030]	/* t25 */		__asm	movaps	xmm7,[eax+0x070]	/* t35 */
			__asm	movaps	xmm0,[eax+0x020]	/* copy t24 */	__asm	movaps	xmm2,[eax+0x060]	/* copy t34 */
			__asm	movaps	xmm1,[eax+0x030]	/* copy t25 */	__asm	movaps	xmm3,[eax+0x070]	/* copy t35 */

			__asm	mulpd	xmm4,[ebx     ]	/* t24*c */			__asm	mulpd	xmm6,[ebx+0x10]	/* t34*s */
			__asm	mulpd	xmm1,[ebx+0x10]	/* t25*s */			__asm	mulpd	xmm3,[ebx     ]	/* t35*c */
			__asm	mulpd	xmm5,[ebx     ]	/* t25*c */			__asm	mulpd	xmm7,[ebx+0x10]	/* t35*s */
			__asm	mulpd	xmm0,[ebx+0x10]	/* t24*s */			__asm	mulpd	xmm2,[ebx     ]	/* t34*c */
			__asm	subpd	xmm4,xmm1	/* ~t24 */				__asm	subpd	xmm6,xmm3	/* rt */
			__asm	addpd	xmm5,xmm0	/* ~t25 */				__asm	addpd	xmm7,xmm2	/* it */

			__asm	mov	ebx, isrt2
																__asm	movaps	xmm2,[eax+0x040]	/* t14 */
			__asm	subpd	xmm4,xmm6		/*~t34=t24-rt */	__asm	movaps	xmm3,[eax+0x050]	/* t15 */
			__asm	subpd	xmm5,xmm7		/*~t35=t25-it */	__asm	subpd	xmm2,[eax+0x050]	/* t14-t15 */
			__asm	addpd	xmm6,xmm6		/*      2* rt */	__asm	addpd	xmm3,[eax+0x040]	/* t15+t14 */
			__asm	addpd	xmm7,xmm7		/*      2* it */	__asm	mulpd	xmm2,[ebx]	/* rt = (t14-t15)*ISRT2 */
			__asm	addpd	xmm6,xmm4		/*~t24=t24+rt */	__asm	mulpd	xmm3,[ebx]	/* it = (t15+t14)*ISRT2 */
			__asm	addpd	xmm7,xmm5		/*~t25=t25+it */	__asm	movaps	xmm0,[eax      ]	/* t04 */
																__asm	movaps	xmm1,[eax+0x010]	/* t05 */
																__asm	subpd	xmm0,xmm2			/*~t14=t04-rt */
																__asm	subpd	xmm1,xmm3			/*~t15=t05-it */
																__asm	addpd	xmm2,[eax      ]	/*~t04=rt +t04*/
																__asm	addpd	xmm3,[eax+0x010]	/*~t05=it +t05*/

			__asm	mov	eax, add0	/* restore main-array indices */
			__asm	mov	ebx, add1
			__asm	mov	ecx, add2
			__asm	mov	edx, add3

			__asm	subpd	xmm2,xmm6		/* t04-t24 */		__asm	subpd	xmm0,xmm5		/* t14-t35 */
			__asm	subpd	xmm3,xmm7		/* t05-t25 */		__asm	subpd	xmm1,xmm4		/* t15-t34 */
			__asm	addpd	xmm6,xmm6		/*   2*t24 */		__asm	addpd	xmm5,xmm5		/*          2*t35 */
			__asm	addpd	xmm7,xmm7		/*   2*t25 */		__asm	addpd	xmm4,xmm4		/*          2*t34 */
			__asm	movaps	[ebx     ],xmm2	/* a[jt+p1 ] */		__asm	movaps	[ecx     ],xmm0	/* a[jt+p2 ] */
			__asm	movaps	[ebx+0x10],xmm3	/* a[jp+p1 ] */		__asm	movaps	[edx+0x10],xmm1	/* a[jp+p3 ] */
			__asm	addpd	xmm6,xmm2		/* t04+t24 */		__asm	addpd	xmm5,xmm0		/* t14+t35 */
			__asm	addpd	xmm7,xmm3		/* t05+t25 */		__asm	addpd	xmm4,xmm1		/* t15+t34 */
			__asm	movaps	[eax     ],xmm6	/* a[jt+p0 ] */		__asm	movaps	[edx     ],xmm5	/* a[jt+p3 ] */
			__asm	movaps	[eax+0x10],xmm7	/* a[jp+p0 ] */		__asm	movaps	[ecx+0x10],xmm4	/* a[jp+p2 ] */

		/*...Block 7: t0C,t1C,t2C,t3C	*/

			jt = j1 + p0C;
			jp = j2 + p0C;
			add0 = &a[jt];
			add1 = add0+p01;
			add2 = add0+p02;
			add3 = add0+p03;

			__asm	mov	eax, r28
			__asm	mov	ebx, cc0

			__asm	movaps	xmm4,[eax+0x020]	/* t2C */			__asm	movaps	xmm6,[eax+0x060]	/* t3C */
			__asm	movaps	xmm5,[eax+0x030]	/* t2D */			__asm	movaps	xmm7,[eax+0x070]	/* t3D */
			__asm	movaps	xmm0,[eax+0x020]	/* copy t2C */		__asm	movaps	xmm2,[eax+0x060]	/* copy t3C */
			__asm	movaps	xmm1,[eax+0x030]	/* copy t2D */		__asm	movaps	xmm3,[eax+0x070]	/* copy t3D */

			__asm	mulpd	xmm4,[ebx+0x10]	/* t2C*s */			__asm	mulpd	xmm6,[ebx     ]	/* t3C*c */
			__asm	mulpd	xmm1,[ebx     ]	/* t2D*c */			__asm	mulpd	xmm3,[ebx+0x10]	/* t3D*s */
			__asm	mulpd	xmm5,[ebx+0x10]	/* t2D*s */			__asm	mulpd	xmm7,[ebx     ]	/* t3D*c */
			__asm	mulpd	xmm0,[ebx     ]	/* t2C*c */			__asm	mulpd	xmm2,[ebx+0x10]	/* t3C*s */
			__asm	subpd	xmm4,xmm1	/* ~t24 */				__asm	subpd	xmm6,xmm3	/* rt */
			__asm	addpd	xmm5,xmm0	/* ~t25 */				__asm	addpd	xmm7,xmm2	/* it */

			__asm	mov	ebx, isrt2
																__asm	movaps	xmm2,[eax+0x040]	/* t1C */
			__asm	subpd	xmm4,xmm6		/*~t2C=t2C-rt */	__asm	movaps	xmm3,[eax+0x050]	/* t1D */
			__asm	subpd	xmm5,xmm7		/*~t2D=t2D-it */	__asm	addpd	xmm2,[eax+0x050]	/* t1C+t1D */
			__asm	addpd	xmm6,xmm6		/*      2* rt */	__asm	subpd	xmm3,[eax+0x040]	/* t1D-t1C */
			__asm	addpd	xmm7,xmm7		/*      2* it */	__asm	mulpd	xmm2,[ebx]	/* rt = (t1C+t1D)*ISRT2 */
			__asm	addpd	xmm6,xmm4		/*~t3C=t2C+rt */	__asm	mulpd	xmm3,[ebx]	/* it = (t1D-t1C)*ISRT2 */
			__asm	addpd	xmm7,xmm5		/*~t3D=t2D+it */	__asm	movaps	xmm0,[eax      ]	/* t0C */
																__asm	movaps	xmm1,[eax+0x010]	/* t0D */
																__asm	subpd	xmm0,xmm2			/*~t0C=t0C-rt */
																__asm	subpd	xmm1,xmm3			/*~t0D=t0D-it */
																__asm	addpd	xmm2,[eax      ]	/*~t1C=rt +t0C*/
																__asm	addpd	xmm3,[eax+0x010]	/*~t1D=it +t0D*/

			__asm	mov	eax, add0	/* restore main-array indices */
			__asm	mov	ebx, add1
			__asm	mov	ecx, add2
			__asm	mov	edx, add3

			__asm	subpd	xmm0,xmm4		/* t0C-t2C */		__asm	subpd	xmm2,xmm7		/* t1C-t3D */
			__asm	subpd	xmm1,xmm5		/* t0D-t2D */		__asm	subpd	xmm3,xmm6		/* t1D-t3C */
			__asm	addpd	xmm4,xmm4		/*   2*t2C */		__asm	addpd	xmm7,xmm7		/*   2*t3D */
			__asm	addpd	xmm5,xmm5		/*   2*t2D */		__asm	addpd	xmm6,xmm6		/*   2*t3C */
			__asm	movaps	[ebx     ],xmm0	/* a[jt+p1 ] */		__asm	movaps	[ecx     ],xmm2	/* a[jt+p2 ] */
			__asm	movaps	[ebx+0x10],xmm1	/* a[jp+p1 ] */		__asm	movaps	[edx+0x10],xmm3	/* a[jp+p3 ] */
			__asm	addpd	xmm4,xmm0		/* t0C+t2C */		__asm	addpd	xmm7,xmm2		/* t1C+t3D */
			__asm	addpd	xmm5,xmm1		/* t0D+t2D */		__asm	addpd	xmm6,xmm3		/* t1D+t3C */
			__asm	movaps	[eax     ],xmm4	/* a[jt+p0 ] */		__asm	movaps	[edx     ],xmm7	/* a[jt+p3 ] */
			__asm	movaps	[eax+0x10],xmm5	/* a[jp+p0 ] */		__asm	movaps	[ecx+0x10],xmm6	/* a[jp+p2 ] */

		/*...Block 2: t02,t12,t22,t32	*/

			jt = j1 + p10;
			jp = j2 + p10;
			add0 = &a[jt];
			add1 = add0+p01;
			add2 = add0+p02;
			add3 = add0+p03;

			__asm	mov	eax, r10
			__asm	mov	ebx, cc0
			__asm	mov	ecx, cc1
			__asm	mov	edx, cc3

			__asm	movaps	xmm4,[eax+0x020]	/* t22 */		__asm	movaps	xmm6,[eax+0x060]	/* t32 */
			__asm	movaps	xmm5,[eax+0x030]	/* t23 */		__asm	movaps	xmm7,[eax+0x070]	/* t33 */
			__asm	movaps	xmm0,[eax+0x020]	/* copy t22 */	__asm	movaps	xmm2,[eax+0x060]	/* copy t32 */
			__asm	movaps	xmm1,[eax+0x030]	/* copy t23 */	__asm	movaps	xmm3,[eax+0x070]	/* copy t33 */

			__asm	mulpd	xmm4,[ecx     ]	/* t22*c32_1 */		__asm	mulpd	xmm6,[edx     ]	/* t32*c32_3 */
			__asm	mulpd	xmm1,[ecx+0x10]	/* t23*s32_1 */		__asm	mulpd	xmm3,[edx+0x10]	/* t33*s32_3 */
			__asm	mulpd	xmm5,[ecx     ]	/* t23*c32_1 */		__asm	mulpd	xmm7,[edx     ]	/* t33*c32_3 */
			__asm	mulpd	xmm0,[ecx+0x10]	/* t22*s32_1 */		__asm	mulpd	xmm2,[edx+0x10]	/* t32*s32_3 */
			__asm	subpd	xmm4,xmm1	/* ~t22 */				__asm	subpd	xmm6,xmm3	/* rt */
			__asm	addpd	xmm5,xmm0	/* ~t23 */				__asm	addpd	xmm7,xmm2	/* it */

																__asm	movaps	xmm2,[eax+0x040]	/* t12 */
																__asm	movaps	xmm0,[eax+0x050]	/* t13 */
																__asm	movaps	xmm1,[eax+0x040]	/* copy t12 */
																__asm	movaps	xmm3,[eax+0x050]	/* copy t13 */

			__asm	subpd	xmm4,xmm6		/*~t32=t22-rt */	__asm	mulpd	xmm2,[ebx     ]	/* t12*c */
			__asm	subpd	xmm5,xmm7		/*~t33=t23-it */	__asm	mulpd	xmm0,[ebx+0x10]	/* t13*s */
			__asm	addpd	xmm6,xmm6		/*      2* rt */	__asm	mulpd	xmm3,[ebx     ]	/* t13*c */
			__asm	addpd	xmm7,xmm7		/*      2* it */	__asm	mulpd	xmm1,[ebx+0x10]	/* t12*s */
			__asm	addpd	xmm6,xmm4		/*~t22=t22+rt */	__asm	subpd	xmm2,xmm0	/* rt */
			__asm	addpd	xmm7,xmm5		/*~t23=t23+it */	__asm	addpd	xmm3,xmm1	/* it */

																__asm	movaps	xmm0,[eax      ]	/* t02 */
																__asm	movaps	xmm1,[eax+0x010]	/* t03 */
																__asm	subpd	xmm0,xmm2		/*~t12=t02-rt */
																__asm	subpd	xmm1,xmm3		/*~t13=t03-it */
																__asm	addpd	xmm2,[eax      ]/*~t02=rt+t02 */
																__asm	addpd	xmm3,[eax+0x010]/*~t03=it+t03 */

			__asm	mov	eax, add0	/* restore main-array indices */
			__asm	mov	ebx, add1
			__asm	mov	ecx, add2
			__asm	mov	edx, add3

			__asm	subpd	xmm2,xmm6		/* t02-t22 */		__asm	subpd	xmm0,xmm5		/* t12-t33 */
			__asm	subpd	xmm3,xmm7		/* t03-t23 */		__asm	subpd	xmm1,xmm4		/* t13-t32 */
			__asm	addpd	xmm6,xmm6		/*   2*t22 */		__asm	addpd	xmm5,xmm5		/*   2*t33 */
			__asm	addpd	xmm7,xmm7		/*   2*t23 */		__asm	addpd	xmm4,xmm4		/*   2*t32 */
			__asm	movaps	[ebx     ],xmm2	/* a[jt+p1 ] */		__asm	movaps	[ecx     ],xmm0	/* a[jt+p2 ] */
			__asm	movaps	[ebx+0x10],xmm3	/* a[jp+p1 ] */		__asm	movaps	[edx+0x10],xmm1	/* a[jp+p3 ] */
			__asm	addpd	xmm6,xmm2		/* t02+t22 */		__asm	addpd	xmm5,xmm0		/* t12+t33 */
			__asm	addpd	xmm7,xmm3		/* t03+t23 */		__asm	addpd	xmm4,xmm1		/* t13+t32 */
			__asm	movaps	[eax     ],xmm6	/* a[jt+p0 ] */		__asm	movaps	[edx     ],xmm5	/* a[jt+p3 ] */
			__asm	movaps	[eax+0x10],xmm7	/* a[jp+p0 ] */		__asm	movaps	[ecx+0x10],xmm4	/* a[jp+p2 ] */

		/*...Block 6: t0A,t1A,t2A,t3A	*/

			jt = j1 + p14;
			jp = j2 + p14;
			add0 = &a[jt];
			add1 = add0+p01;
			add2 = add0+p02;
			add3 = add0+p03;

			__asm	mov	eax, r18
			__asm	mov	ebx, cc0
			__asm	mov	ecx, cc3
			__asm	mov	edx, cc1

			__asm	movaps	xmm4,[eax+0x020]	/* t2A */		__asm	movaps	xmm6,[eax+0x060]	/* t3A */
			__asm	movaps	xmm5,[eax+0x030]	/* t2B */		__asm	movaps	xmm7,[eax+0x070]	/* t3B */
			__asm	movaps	xmm0,[eax+0x020]	/* copy t2A */	__asm	movaps	xmm2,[eax+0x060]	/* copy t3A */
			__asm	movaps	xmm1,[eax+0x030]	/* copy t2B */	__asm	movaps	xmm3,[eax+0x070]	/* copy t3B */

			__asm	mulpd	xmm4,[ecx+0x10]	/* t2A*s32_3 */		__asm	mulpd	xmm6,[edx     ]	/* t3A*c32_1 */
			__asm	mulpd	xmm1,[ecx     ]	/* t2B*c32_3 */		__asm	mulpd	xmm3,[edx+0x10]	/* t3B*s32_1 */
			__asm	mulpd	xmm5,[ecx+0x10]	/* t2B*s32_3 */		__asm	mulpd	xmm7,[edx     ]	/* t3B*c32_1 */
			__asm	mulpd	xmm0,[ecx     ]	/* t2A*c32_3 */		__asm	mulpd	xmm2,[edx+0x10]	/* t3A*s32_1 */
			__asm	subpd	xmm4,xmm1	/* ~t2A */				__asm	addpd	xmm6,xmm3	/* rt */
			__asm	addpd	xmm5,xmm0	/* ~t2B */				__asm	subpd	xmm7,xmm2	/* it */

																__asm	movaps	xmm2,[eax+0x040]	/* t1A */
																__asm	movaps	xmm0,[eax+0x050]	/* t1B */
																__asm	movaps	xmm1,[eax+0x040]	/* copy t1A */
																__asm	movaps	xmm3,[eax+0x050]	/* copy t1B */

			__asm	subpd	xmm4,xmm6		/*~t2A=t2A-rt */	__asm	mulpd	xmm2,[ebx+0x10]	/* t1A*s */
			__asm	subpd	xmm5,xmm7		/*~t2B=t2B-it */	__asm	mulpd	xmm0,[ebx     ]	/* t1B*c */
			__asm	addpd	xmm6,xmm6		/*      2* rt */	__asm	mulpd	xmm3,[ebx+0x10]	/* t1B*s */
			__asm	addpd	xmm7,xmm7		/*      2* it */	__asm	mulpd	xmm1,[ebx     ]	/* t1A*c */
			__asm	addpd	xmm6,xmm4		/*~t3A=t2A+rt */	__asm	addpd	xmm2,xmm0	/* rt */
			__asm	addpd	xmm7,xmm5		/*~t3B=t2B+it */	__asm	subpd	xmm3,xmm1	/* it */

																__asm	movaps	xmm0,[eax      ]	/* t0A */
																__asm	movaps	xmm1,[eax+0x010]	/* t0B */
																__asm	subpd	xmm0,xmm2		/*~t0A=t0A-rt */
																__asm	subpd	xmm1,xmm3		/*~t0B=t0B-it */
																__asm	addpd	xmm2,[eax      ]/*~t1A=rt+t0A */
																__asm	addpd	xmm3,[eax+0x010]/*~t1B=it+t0B */

			__asm	mov	eax, add0	/* restore main-array indices */
			__asm	mov	ebx, add1
			__asm	mov	ecx, add2
			__asm	mov	edx, add3

			__asm	subpd	xmm0,xmm4		/* t0A-t2A */		__asm	subpd	xmm2,xmm7		/* t1A-t3B */
			__asm	subpd	xmm1,xmm5		/* t0B-t2B */		__asm	subpd	xmm3,xmm6		/* t1B-t3A */
			__asm	addpd	xmm4,xmm4		/*   2*t2A */		__asm	addpd	xmm7,xmm7		/*   2*t3B */
			__asm	addpd	xmm5,xmm5		/*   2*t2B */		__asm	addpd	xmm6,xmm6		/*   2*t3A */
			__asm	movaps	[ebx     ],xmm0	/* a[jt+p1 ] */		__asm	movaps	[ecx     ],xmm2	/* a[jt+p2 ] */
			__asm	movaps	[ebx+0x10],xmm1	/* a[jp+p1 ] */		__asm	movaps	[edx+0x10],xmm3	/* a[jp+p3 ] */
			__asm	addpd	xmm4,xmm0		/* t0A+t2A */		__asm	addpd	xmm7,xmm2		/* t1A+t3B */
			__asm	addpd	xmm5,xmm1		/* t0B+t2B */		__asm	addpd	xmm6,xmm3		/* t1B+t3A */
			__asm	movaps	[eax     ],xmm4	/* a[jt+p0 ] */		__asm	movaps	[edx     ],xmm7	/* a[jt+p3 ] */
			__asm	movaps	[eax+0x10],xmm5	/* a[jp+p0 ] */		__asm	movaps	[ecx+0x10],xmm6	/* a[jp+p2 ] */

		/*...Block 4: t06,t16,t26,t36	*/

			jt = j1 + p18;
			jp = j2 + p18;
			add0 = &a[jt];
			add1 = add0+p01;
			add2 = add0+p02;
			add3 = add0+p03;

			__asm	mov	eax, r30
			__asm	mov	ebx, cc0
			__asm	mov	ecx, cc3
			__asm	mov	edx, cc1

			__asm	movaps	xmm4,[eax+0x020]	/* t26 */		__asm	movaps	xmm6,[eax+0x060]	/* t36 */
			__asm	movaps	xmm5,[eax+0x030]	/* t27 */		__asm	movaps	xmm7,[eax+0x070]	/* t37 */
			__asm	movaps	xmm0,[eax+0x020]	/* copy t26 */	__asm	movaps	xmm2,[eax+0x060]	/* copy t36 */
			__asm	movaps	xmm1,[eax+0x030]	/* copy t27 */	__asm	movaps	xmm3,[eax+0x070]	/* copy t37 */

			__asm	mulpd	xmm4,[ecx     ]	/* t26*s32_3 */		__asm	mulpd	xmm6,[edx+0x10]	/* t36*s32_1 */
			__asm	mulpd	xmm1,[ecx+0x10]	/* t27*s32_3 */		__asm	mulpd	xmm3,[edx     ]	/* t37*c32_1 */
			__asm	mulpd	xmm5,[ecx     ]	/* t27*c32_3 */		__asm	mulpd	xmm7,[edx+0x10]	/* t37*s32_1 */
			__asm	mulpd	xmm0,[ecx+0x10]	/* t26*s32_3 */		__asm	mulpd	xmm2,[edx     ]	/* t36*c32_1 */
			__asm	subpd	xmm4,xmm1	/* ~t26 */				__asm	addpd	xmm6,xmm3	/* rt */
			__asm	addpd	xmm5,xmm0	/* ~t27 */				__asm	subpd	xmm7,xmm2	/* it */

																__asm	movaps	xmm2,[eax+0x040]	/* t16 */
																__asm	movaps	xmm0,[eax+0x050]	/* t17 */
																__asm	movaps	xmm1,[eax+0x040]	/* copy t16 */
																__asm	movaps	xmm3,[eax+0x050]	/* copy t17 */

			__asm	subpd	xmm4,xmm6		/*~t26=t26-rt */	__asm	mulpd	xmm2,[ebx+0x10]	/* t16*s */
			__asm	subpd	xmm5,xmm7		/*~t27=t27-it */	__asm	mulpd	xmm0,[ebx     ]	/* t17*c */
			__asm	addpd	xmm6,xmm6		/*      2* rt */	__asm	mulpd	xmm3,[ebx+0x10]	/* t17*s */
			__asm	addpd	xmm7,xmm7		/*      2* it */	__asm	mulpd	xmm1,[ebx     ]	/* t16*c */
			__asm	addpd	xmm6,xmm4		/*~t36=t26+rt */	__asm	subpd	xmm2,xmm0	/* rt */
			__asm	addpd	xmm7,xmm5		/*~t37=t27+it */	__asm	addpd	xmm3,xmm1	/* it */

																__asm	movaps	xmm0,[eax      ]	/* t06 */
																__asm	movaps	xmm1,[eax+0x010]	/* t07 */
																__asm	subpd	xmm0,xmm2		/*~t16=t06-rt */
																__asm	subpd	xmm1,xmm3		/*~t17=t07-it */
																__asm	addpd	xmm2,[eax      ]/*~t06=rt+t06 */
																__asm	addpd	xmm3,[eax+0x010]/*~t07=it+t07 */

			__asm	mov	eax, add0	/* restore main-array indices */
			__asm	mov	ebx, add1
			__asm	mov	ecx, add2
			__asm	mov	edx, add3

			__asm	subpd	xmm2,xmm4		/* t06-t26 */		__asm	subpd	xmm0,xmm7		/* t16-t37 */
			__asm	subpd	xmm3,xmm5		/* t07-t27 */		__asm	subpd	xmm1,xmm6		/* t17-t36 */
			__asm	addpd	xmm4,xmm4		/*   2*t26 */		__asm	addpd	xmm7,xmm7		/*   2*t37 */
			__asm	addpd	xmm5,xmm5		/*   2*t27 */		__asm	addpd	xmm6,xmm6		/*   2*t36 */
			__asm	movaps	[ebx     ],xmm2	/* a[jt+p1 ] */		__asm	movaps	[ecx     ],xmm0	/* a[jt+p2 ] */
			__asm	movaps	[ebx+0x10],xmm3	/* a[jp+p1 ] */		__asm	movaps	[edx+0x10],xmm1	/* a[jp+p3 ] */
			__asm	addpd	xmm4,xmm2		/* t06+t26 */		__asm	addpd	xmm7,xmm0		/* t16+t37 */
			__asm	addpd	xmm5,xmm3		/* t07+t27 */		__asm	addpd	xmm6,xmm1		/* t17+t36 */
			__asm	movaps	[eax     ],xmm4	/* a[jt+p0 ] */		__asm	movaps	[edx     ],xmm7	/* a[jt+p3 ] */
			__asm	movaps	[eax+0x10],xmm5	/* a[jp+p0 ] */		__asm	movaps	[ecx+0x10],xmm6	/* a[jp+p2 ] */

		/*...Block 8: t0E,t1E,t2E,t3E	*/

			jt = j1 + p1C;
			jp = j2 + p1C;
			add0 = &a[jt];
			add1 = add0+p01;
			add2 = add0+p02;
			add3 = add0+p03;

			__asm	mov	eax, r38
			__asm	mov	ebx, cc0
			__asm	mov	ecx, cc1
			__asm	mov	edx, cc3

			__asm	movaps	xmm4,[eax+0x020]	/* t2E */		__asm	movaps	xmm6,[eax+0x060]	/* t3E */
			__asm	movaps	xmm5,[eax+0x030]	/* t2F */		__asm	movaps	xmm7,[eax+0x070]	/* t3F */
			__asm	movaps	xmm0,[eax+0x020]	/* copy t2E */	__asm	movaps	xmm2,[eax+0x060]	/* copy t3E */
			__asm	movaps	xmm1,[eax+0x030]	/* copy t2F */	__asm	movaps	xmm3,[eax+0x070]	/* copy t3F */

			__asm	mulpd	xmm4,[ecx+0x10]	/* t2E*s32_1 */		__asm	mulpd	xmm6,[edx+0x10]	/* t3E*c32_3 */
			__asm	mulpd	xmm1,[ecx     ]	/* t2F*c32_1 */		__asm	mulpd	xmm3,[edx     ]	/* t3F*s32_3 */
			__asm	mulpd	xmm5,[ecx+0x10]	/* t2F*s32_1 */		__asm	mulpd	xmm7,[edx+0x10]	/* t3F*c32_3 */
			__asm	mulpd	xmm0,[ecx     ]	/* t2E*c32_1 */		__asm	mulpd	xmm2,[edx     ]	/* t3E*s32_3 */
			__asm	subpd	xmm4,xmm1	/* ~t2E */				__asm	subpd	xmm6,xmm3	/* rt */
			__asm	addpd	xmm5,xmm0	/* ~t2F */				__asm	addpd	xmm7,xmm2	/* it */

																__asm	movaps	xmm2,[eax+0x040]	/* t1E */
																__asm	movaps	xmm0,[eax+0x050]	/* t1F */
																__asm	movaps	xmm1,[eax+0x040]	/* copy t1E */
																__asm	movaps	xmm3,[eax+0x050]	/* copy t1F */

			__asm	subpd	xmm4,xmm6		/*~t2E=t2E-rt */	__asm	mulpd	xmm2,[ebx     ]	/* t1E*c */
			__asm	subpd	xmm5,xmm7		/*~t2F=t2F-it */	__asm	mulpd	xmm0,[ebx+0x10]	/* t1F*s */
			__asm	addpd	xmm6,xmm6		/*      2* rt */	__asm	mulpd	xmm3,[ebx     ]	/* t1F*c */
			__asm	addpd	xmm7,xmm7		/*      2* it */	__asm	mulpd	xmm1,[ebx+0x10]	/* t1E*s */
			__asm	addpd	xmm6,xmm4		/*~t3E=t2E+rt */	__asm	addpd	xmm2,xmm0	/* rt */
			__asm	addpd	xmm7,xmm5		/*~t3F=t2F+it */	__asm	subpd	xmm3,xmm1	/* it */

																__asm	movaps	xmm0,[eax      ]	/* t0E */
																__asm	movaps	xmm1,[eax+0x010]	/* t0F */
																__asm	subpd	xmm0,xmm2		/*~t0E=t0E-rt */
																__asm	subpd	xmm1,xmm3		/*~t0F=t0F-it */
																__asm	addpd	xmm2,[eax      ]/*~t1E=rt+t0E */
																__asm	addpd	xmm3,[eax+0x010]/*~t1F=it+t0F */

			__asm	mov	eax, add0	/* restore main-array indices */
			__asm	mov	ebx, add1
			__asm	mov	ecx, add2
			__asm	mov	edx, add3

			__asm	subpd	xmm0,xmm4		/* t0E-t2E */		__asm	subpd	xmm2,xmm7		/* t1E-t3F */
			__asm	subpd	xmm1,xmm5		/* t0F-t2F */		__asm	subpd	xmm3,xmm6		/* t1F-t3E */
			__asm	addpd	xmm4,xmm4		/*   2*t2E */		__asm	addpd	xmm7,xmm7		/*   2*t3F */
			__asm	addpd	xmm5,xmm5		/*   2*t2F */		__asm	addpd	xmm6,xmm6		/*   2*t3E */
			__asm	movaps	[ebx     ],xmm0	/* a[jt+p1 ] */		__asm	movaps	[ecx     ],xmm2	/* a[jt+p2 ] */
			__asm	movaps	[ebx+0x10],xmm1	/* a[jp+p1 ] */		__asm	movaps	[edx+0x10],xmm3	/* a[jp+p3 ] */
			__asm	addpd	xmm4,xmm0		/* t0E+t2E */		__asm	addpd	xmm7,xmm2		/* t1E+t3F */
			__asm	addpd	xmm5,xmm1		/* t0F+t2F */		__asm	addpd	xmm6,xmm3		/* t1F+t3E */
			__asm	movaps	[eax     ],xmm4	/* a[jt+p0 ] */		__asm	movaps	[edx     ],xmm7	/* a[jt+p3 ] */
			__asm	movaps	[eax+0x10],xmm5	/* a[jp+p0 ] */		__asm	movaps	[ecx+0x10],xmm6	/* a[jp+p2 ] */

		  #else	/* GCC-style inline ASM: */

			add0 = &a[j1    ];
			SSE2_RADIX32_DIF_NOTWIDDLE(add0,p01,p02,p03,p04,p08,p10,p18,r00,isrt2,cc0);

		  #endif

	#if 0//DEBUG_SSE2
		fprintf(stderr, "radix32_carry_out: R00 = %20.5f, %20.5f\n",r00->re,r01-> re);
		fprintf(stderr, "radix32_carry_out: R02 = %20.5f, %20.5f\n",r02->re,r03-> re);
		fprintf(stderr, "radix32_carry_out: R04 = %20.5f, %20.5f\n",r04->re,r05-> re);
		fprintf(stderr, "radix32_carry_out: R06 = %20.5f, %20.5f\n",r06->re,r07-> re);
		fprintf(stderr, "radix32_carry_out: R08 = %20.5f, %20.5f\n",r08->re,r09-> re);
		fprintf(stderr, "radix32_carry_out: R0a = %20.5f, %20.5f\n",r0A->re,r0B-> re);
		fprintf(stderr, "radix32_carry_out: R0c = %20.5f, %20.5f\n",r0C->re,r0D-> re);
		fprintf(stderr, "radix32_carry_out: R0e = %20.5f, %20.5f\n",r0E->re,r0F-> re);
		fprintf(stderr, "radix32_carry_out: R10 = %20.5f, %20.5f\n",r10->re,r11-> re);
		fprintf(stderr, "radix32_carry_out: R12 = %20.5f, %20.5f\n",r12->re,r13-> re);
		fprintf(stderr, "radix32_carry_out: R14 = %20.5f, %20.5f\n",r14->re,r15-> re);
		fprintf(stderr, "radix32_carry_out: R16 = %20.5f, %20.5f\n",r16->re,r17-> re);
		fprintf(stderr, "radix32_carry_out: R18 = %20.5f, %20.5f\n",r18->re,r19-> re);
		fprintf(stderr, "radix32_carry_out: R1a = %20.5f, %20.5f\n",r1A->re,r1B-> re);
		fprintf(stderr, "radix32_carry_out: R1c = %20.5f, %20.5f\n",r1C->re,r1D-> re);
		fprintf(stderr, "radix32_carry_out: R1e = %20.5f, %20.5f\n",r1E->re,r1F-> re);
		fprintf(stderr, "radix32_carry_out: R20 = %20.5f, %20.5f\n",r20->re,r21-> re);
		fprintf(stderr, "radix32_carry_out: R22 = %20.5f, %20.5f\n",r22->re,r23-> re);
		fprintf(stderr, "radix32_carry_out: R24 = %20.5f, %20.5f\n",r24->re,r25-> re);
		fprintf(stderr, "radix32_carry_out: R26 = %20.5f, %20.5f\n",r26->re,r27-> re);
		fprintf(stderr, "radix32_carry_out: R28 = %20.5f, %20.5f\n",r28->re,r29-> re);
		fprintf(stderr, "radix32_carry_out: R2a = %20.5f, %20.5f\n",r2A->re,r2B-> re);
		fprintf(stderr, "radix32_carry_out: R2c = %20.5f, %20.5f\n",r2C->re,r2D-> re);
		fprintf(stderr, "radix32_carry_out: R2e = %20.5f, %20.5f\n",r2E->re,r2F-> re);
		fprintf(stderr, "radix32_carry_out: R30 = %20.5f, %20.5f\n",r30->re,r31-> re);
		fprintf(stderr, "radix32_carry_out: R32 = %20.5f, %20.5f\n",r32->re,r33-> re);
		fprintf(stderr, "radix32_carry_out: R34 = %20.5f, %20.5f\n",r34->re,r35-> re);
		fprintf(stderr, "radix32_carry_out: R36 = %20.5f, %20.5f\n",r36->re,r37-> re);
		fprintf(stderr, "radix32_carry_out: R38 = %20.5f, %20.5f\n",r38->re,r39-> re);
		fprintf(stderr, "radix32_carry_out: R3a = %20.5f, %20.5f\n",r3A->re,r3B-> re);
		fprintf(stderr, "radix32_carry_out: R3c = %20.5f, %20.5f\n",r3C->re,r3D-> re);
		fprintf(stderr, "radix32_carry_out: R3e = %20.5f, %20.5f\n",r3E->re,r3F-> re);
	exit(0);
	#endif

	#ifdef DEBUG_SSE2
		jt = j1;		jp = j2;
		fprintf(stderr, "radix32_wrapper: A_out[00] = %20.5f, %20.5f\n",a[jt    ],a[jp    ]);
		fprintf(stderr, "radix32_wrapper: A_out[02] = %20.5f, %20.5f\n",a[jt+p01],a[jp+p01]);
		fprintf(stderr, "radix32_wrapper: A_out[04] = %20.5f, %20.5f\n",a[jt+p02],a[jp+p02]);
		fprintf(stderr, "radix32_wrapper: A_out[06] = %20.5f, %20.5f\n",a[jt+p03],a[jp+p03]);
		fprintf(stderr, "radix32_wrapper: A_out[08] = %20.5f, %20.5f\n",a[jt+p04],a[jp+p04]);
		fprintf(stderr, "radix32_wrapper: A_out[0a] = %20.5f, %20.5f\n",a[jt+p05],a[jp+p05]);
		fprintf(stderr, "radix32_wrapper: A_out[0c] = %20.5f, %20.5f\n",a[jt+p06],a[jp+p06]);
		fprintf(stderr, "radix32_wrapper: A_out[0e] = %20.5f, %20.5f\n",a[jt+p07],a[jp+p07]);	jt += p08;	jp += p08;
		fprintf(stderr, "radix32_wrapper: A_out[10] = %20.5f, %20.5f\n",a[jt    ],a[jp    ]);
		fprintf(stderr, "radix32_wrapper: A_out[12] = %20.5f, %20.5f\n",a[jt+p01],a[jp+p01]);
		fprintf(stderr, "radix32_wrapper: A_out[14] = %20.5f, %20.5f\n",a[jt+p02],a[jp+p02]);
		fprintf(stderr, "radix32_wrapper: A_out[16] = %20.5f, %20.5f\n",a[jt+p03],a[jp+p03]);
		fprintf(stderr, "radix32_wrapper: A_out[18] = %20.5f, %20.5f\n",a[jt+p04],a[jp+p04]);
		fprintf(stderr, "radix32_wrapper: A_out[1a] = %20.5f, %20.5f\n",a[jt+p05],a[jp+p05]);
		fprintf(stderr, "radix32_wrapper: A_out[1c] = %20.5f, %20.5f\n",a[jt+p06],a[jp+p06]);
		fprintf(stderr, "radix32_wrapper: A_out[1e] = %20.5f, %20.5f\n",a[jt+p07],a[jp+p07]);	jt += p08;	jp += p08;
		fprintf(stderr, "radix32_wrapper: A_out[20] = %20.5f, %20.5f\n",a[jt    ],a[jp    ]);
		fprintf(stderr, "radix32_wrapper: A_out[22] = %20.5f, %20.5f\n",a[jt+p01],a[jp+p01]);
		fprintf(stderr, "radix32_wrapper: A_out[24] = %20.5f, %20.5f\n",a[jt+p02],a[jp+p02]);
		fprintf(stderr, "radix32_wrapper: A_out[26] = %20.5f, %20.5f\n",a[jt+p03],a[jp+p03]);
		fprintf(stderr, "radix32_wrapper: A_out[28] = %20.5f, %20.5f\n",a[jt+p04],a[jp+p04]);
		fprintf(stderr, "radix32_wrapper: A_out[2a] = %20.5f, %20.5f\n",a[jt+p05],a[jp+p05]);
		fprintf(stderr, "radix32_wrapper: A_out[2c] = %20.5f, %20.5f\n",a[jt+p06],a[jp+p06]);
		fprintf(stderr, "radix32_wrapper: A_out[2e] = %20.5f, %20.5f\n",a[jt+p07],a[jp+p07]);	jt += p08;	jp += p08;
		fprintf(stderr, "radix32_wrapper: A_out[30] = %20.5f, %20.5f\n",a[jt    ],a[jp    ]);
		fprintf(stderr, "radix32_wrapper: A_out[32] = %20.5f, %20.5f\n",a[jt+p01],a[jp+p01]);
		fprintf(stderr, "radix32_wrapper: A_out[34] = %20.5f, %20.5f\n",a[jt+p02],a[jp+p02]);
		fprintf(stderr, "radix32_wrapper: A_out[36] = %20.5f, %20.5f\n",a[jt+p03],a[jp+p03]);
		fprintf(stderr, "radix32_wrapper: A_out[38] = %20.5f, %20.5f\n",a[jt+p04],a[jp+p04]);
		fprintf(stderr, "radix32_wrapper: A_out[3a] = %20.5f, %20.5f\n",a[jt+p05],a[jp+p05]);
		fprintf(stderr, "radix32_wrapper: A_out[3c] = %20.5f, %20.5f\n",a[jt+p06],a[jp+p06]);
		fprintf(stderr, "radix32_wrapper: A_out[3e] = %20.5f, %20.5f\n",a[jt+p07],a[jp+p07]);	jt += p08;	jp += p08;
	exit(0);
	#endif

#else	/* #ifdef USE_SSE2 */

			/*...The radix-32 DIF pass is here:	*/

			/*       gather the needed data (32 64-bit complex, i.e. 64 64-bit reals) and do the first set of four length-8 transforms.	*/

			/*...Block 1:	*/

				t02=a1p00r-a1p10r;	t03=a1p00i-a1p10i;
				t00=a1p00r+a1p10r;	t01=a1p00i+a1p10i;

				t06=a1p08r-a1p18r;	t07=a1p08i-a1p18i;
				t04=a1p08r+a1p18r;	t05=a1p08i+a1p18i;

				rt =t04;		it =t05;
				t04=t00-rt;		t05=t01-it;
				t00=t00+rt;		t01=t01+it;

				rt =t06;		it =t07;
				t06=t02+it;		t07=t03-rt;
				t02=t02-it;		t03=t03+rt;

				t0A=a1p04r-a1p14r;	t0B=a1p04i-a1p14i;
				t08=a1p04r+a1p14r;	t09=a1p04i+a1p14i;

				t0E=a1p0Cr-a1p1Cr;	t0F=a1p0Ci-a1p1Ci;
				t0C=a1p0Cr+a1p1Cr;	t0D=a1p0Ci+a1p1Ci;

				rt =t0C;		it =t0D;
				t0C=t08-rt;		t0D=t09-it;
				t08=t08+rt;		t09=t09+it;

				rt =t0E;		it =t0F;
				t0E=t0A+it;		t0F=t0B-rt;
				t0A=t0A-it;		t0B=t0B+rt;

				rt =t08;		it =t09;
				t08=t00-rt;		t09=t01-it;
				t00=t00+rt;		t01=t01+it;

				rt =t0C;		it =t0D;
				t0C=t04+it;		t0D=t05-rt;
				t04=t04-it;		t05=t05+rt;

				rt =(t0A-t0B)*ISRT2;it =(t0A+t0B)*ISRT2;
				t0A=t02-rt;		t0B=t03-it;
				t02=t02+rt;		t03=t03+it;

				rt =(t0E+t0F)*ISRT2;it =(t0F-t0E)*ISRT2;
				t0E=t06+rt;		t0F=t07+it;
				t06=t06-rt;		t07=t07-it;

			/*...Block 2:	*/

				t12=a1p02r-a1p12r;		t13=a1p02i-a1p12i;
				t10=a1p02r+a1p12r;		t11=a1p02i+a1p12i;

				t16=a1p0Ar-a1p1Ar;		t17=a1p0Ai-a1p1Ai;
				t14=a1p0Ar+a1p1Ar;		t15=a1p0Ai+a1p1Ai;

				rt =t14;		it =t15;
				t14=t10-rt;		t15=t11-it;
				t10=t10+rt;		t11=t11+it;

				rt =t16;		it =t17;
				t16=t12+it;		t17=t13-rt;
				t12=t12-it;		t13=t13+rt;

				t1A=a1p06r-a1p16r;		t1B=a1p06i-a1p16i;
				t18=a1p06r+a1p16r;		t19=a1p06i+a1p16i;

				t1E=a1p0Er-a1p1Er;		t1F=a1p0Ei-a1p1Ei;
				t1C=a1p0Er+a1p1Er;		t1D=a1p0Ei+a1p1Ei;

				rt =t1C;		it =t1D;
				t1C=t18-rt;		t1D=t19-it;
				t18=t18+rt;		t19=t19+it;

				rt =t1E;		it =t1F;
				t1E=t1A+it;		t1F=t1B-rt;
				t1A=t1A-it;		t1B=t1B+rt;

				rt =t18;		it =t19;
				t18=t10-rt;		t19=t11-it;
				t10=t10+rt;		t11=t11+it;

				rt =t1C;		it =t1D;
				t1C=t14+it;		t1D=t15-rt;
				t14=t14-it;		t15=t15+rt;

				rt =(t1A-t1B)*ISRT2;it =(t1A+t1B)*ISRT2;
				t1A=t12-rt;		t1B=t13-it;
				t12=t12+rt;		t13=t13+it;

				rt =(t1E+t1F)*ISRT2;it =(t1F-t1E)*ISRT2;
				t1E=t16+rt;		t1F=t17+it;
				t16=t16-rt;		t17=t17-it;

			/*...Block 3:	*/

				t22=a1p01r-a1p11r;		t23=a1p01i-a1p11i;
				t20=a1p01r+a1p11r;		t21=a1p01i+a1p11i;

				t26=a1p09r-a1p19r;		t27=a1p09i-a1p19i;
				t24=a1p09r+a1p19r;		t25=a1p09i+a1p19i;

				rt =t24;		it =t25;
				t24=t20-rt;		t25=t21-it;
				t20=t20+rt;		t21=t21+it;

				rt =t26;		it =t27;
				t26=t22+it;		t27=t23-rt;
				t22=t22-it;		t23=t23+rt;

				t2A=a1p05r-a1p15r;		t2B=a1p05i-a1p15i;
				t28=a1p05r+a1p15r;		t29=a1p05i+a1p15i;

				t2E=a1p0Dr-a1p1Dr;		t2F=a1p0Di-a1p1Di;
				t2C=a1p0Dr+a1p1Dr;		t2D=a1p0Di+a1p1Di;

				rt =t2C;		it =t2D;
				t2C=t28-rt;		t2D=t29-it;
				t28=t28+rt;		t29=t29+it;

				rt =t2E;		it =t2F;
				t2E=t2A+it;		t2F=t2B-rt;
				t2A=t2A-it;		t2B=t2B+rt;

				rt =t28;		it =t29;
				t28=t20-rt;		t29=t21-it;
				t20=t20+rt;		t21=t21+it;

				rt =t2C;		it =t2D;
				t2C=t24+it;		t2D=t25-rt;
				t24=t24-it;		t25=t25+rt;

				rt =(t2A-t2B)*ISRT2;it =(t2A+t2B)*ISRT2;
				t2A=t22-rt;		t2B=t23-it;
				t22=t22+rt;		t23=t23+it;

				rt =(t2E+t2F)*ISRT2;it =(t2F-t2E)*ISRT2;
				t2E=t26+rt;		t2F=t27+it;
				t26=t26-rt;		t27=t27-it;

			/*...Block 4:	*/

				t32=a1p03r-a1p13r;		t33=a1p03i-a1p13i;
				t30=a1p03r+a1p13r;		t31=a1p03i+a1p13i;

				t36=a1p0Br-a1p1Br;		t37=a1p0Bi-a1p1Bi;
				t34=a1p0Br+a1p1Br;		t35=a1p0Bi+a1p1Bi;

				rt =t34;		it =t35;
				t34=t30-rt;		t35=t31-it;
				t30=t30+rt;		t31=t31+it;

				rt =t36;		it =t37;
				t36=t32+it;		t37=t33-rt;
				t32=t32-it;		t33=t33+rt;

				t3A=a1p07r-a1p17r;		t3B=a1p07i-a1p17i;
				t38=a1p07r+a1p17r;		t39=a1p07i+a1p17i;

				t3E=a1p0Fr-a1p1Fr;		t3F=a1p0Fi-a1p1Fi;
				t3C=a1p0Fr+a1p1Fr;		t3D=a1p0Fi+a1p1Fi;

				rt =t3C;		it =t3D;
				t3C=t38-rt;		t3D=t39-it;
				t38=t38+rt;		t39=t39+it;

				rt =t3E;		it =t3F;
				t3E=t3A+it;		t3F=t3B-rt;
				t3A=t3A-it;		t3B=t3B+rt;

				rt =t38;		it =t39;
				t38=t30-rt;		t39=t31-it;
				t30=t30+rt;		t31=t31+it;

				rt =t3C;		it =t3D;
				t3C=t34+it;		t3D=t35-rt;
				t34=t34-it;		t35=t35+rt;

				rt =(t3A-t3B)*ISRT2;it =(t3A+t3B)*ISRT2;
				t3A=t32-rt;		t3B=t33-it;
				t32=t32+rt;		t33=t33+it;

				rt =(t3E+t3F)*ISRT2;it =(t3F-t3E)*ISRT2;
				t3E=t36+rt;		t3F=t37+it;
				t36=t36-rt;		t37=t37-it;

		/*...and now do eight radix-4 transforms, including the internal twiddle factors:
			1, exp(i* 1*twopi/32) =       ( c32_1, s32_1), exp(i* 2*twopi/32) =       ( c    , s    ), exp(i* 3*twopi/32) =       ( c32_3, s32_3) (for inputs to transform block 2),
			1, exp(i* 2*twopi/32) =       ( c    , s    ), exp(i* 4*twopi/32) = ISRT2*( 1    , 1    ), exp(i* 3*twopi/32) =       ( s    , c    ) (for inputs to transform block 3),
			1, exp(i* 3*twopi/32) =       ( c32_3, s32_3), exp(i* 6*twopi/32) =       ( s    , c    ), exp(i* 9*twopi/32) =       (-s32_1, c32_1) (for inputs to transform block 4),
			1, exp(i* 4*twopi/32) = ISRT2*( 1    , 1    ), exp(i* 8*twopi/32) =       ( 0    , 1    ), exp(i*12*twopi/32) = ISRT2*(-1    , 1    ) (for inputs to transform block 5),
			1, exp(i* 5*twopi/32) =       ( s32_3, c32_3), exp(i*10*twopi/32) =       (-s    , c    ), exp(i*15*twopi/32) =       (-c32_1, s32_1) (for inputs to transform block 6),
			1, exp(i* 6*twopi/32) =       ( s    , c    ), exp(i*12*twopi/32) = ISRT2*(-1    , 1    ), exp(i*18*twopi/32) =       (-c    ,-s    ) (for inputs to transform block 7),
			1, exp(i* 7*twopi/32) =       ( s32_1, c32_1), exp(i*14*twopi/32) =       (-c    , s    ), exp(i*21*twopi/32) =       (-s32_3,-c32_3) (for inputs to transform block 8),
		and only the last 3 inputs to each of the radix-4 transforms 2 through 8 are multiplied by non-unity twiddles.	*/

			#if PFETCH
				prefetch_offset = ((j >> 1) & 3)*p01;
			#endif
			/*...Block 1: t00,t10,t20,t30	*/
			jt = j1;	jp = j2;

				rt =t10;	t10=t00-rt;	t00=t00+rt;
				it =t11;	t11=t01-it;	t01=t01+it;
			#if PFETCH
				addr = &a[jt+prefetch_offset];
				prefetch_p_doubles(addr);
			#endif
				rt =t30;	t30=t20-rt;	t20=t20+rt;
				it =t31;	t31=t21-it;	t21=t21+it;
			#if PFETCH_X86
				addr += p01;
				prefetch_p_doubles(addr);
			#endif
				a[jt    ]=t00+t20;		a[jp    ]=t01+t21;
				a[jt+p01]=t00-t20;		a[jp+p01]=t01-t21;
			#if PFETCH_X86
				addr += p01;
				prefetch_p_doubles(addr);
			#endif
				a[jt+p02]=t10-t31;		a[jp+p02]=t11+t30;	/* mpy by E^4=i is inlined here...	*/
				a[jt+p03]=t10+t31;		a[jp+p03]=t11-t30;
			#if PFETCH_X86
				addr += p01;
				prefetch_p_doubles(addr);
			#endif
			/*...Block 5: t08,t18,t28,t38	*/
			jt = j1 + p04;	jp = j2 + p04;

				rt =t18;	t18=t08+t19;	t08=t08-t19;		/* twiddle mpy by E^4 = I	*/
							t19=t09-rt;		t09=t09+rt;
			#if PFETCH
				addr = &a[jt+prefetch_offset];
				prefetch_p_doubles(addr);
			#endif
				rt =(t28-t29)*ISRT2;	t29=(t28+t29)*ISRT2;		t28=rt;	/* twiddle mpy by E^8	*/
				rt =(t39+t38)*ISRT2;	it =(t39-t38)*ISRT2;			/* twiddle mpy by -E^12 is here...	*/
				t38=t28+rt;			t28=t28-rt;				/* ...and get E^12 by flipping signs here.	*/
				t39=t29+it;			t29=t29-it;
			#if PFETCH_X86
				addr += p01;
				prefetch_p_doubles(addr);
			#endif
				a[jt    ]=t08+t28;		a[jp    ]=t09+t29;
				a[jt+p01]=t08-t28;		a[jp+p01]=t09-t29;
			#if PFETCH_X86
				addr += p01;
				prefetch_p_doubles(addr);
			#endif
				a[jt+p02]=t18-t39;		a[jp+p02]=t19+t38;	/* mpy by E^4=i is inlined here...	*/
				a[jt+p03]=t18+t39;		a[jp+p03]=t19-t38;
			#if PFETCH_X86
				addr += p01;
				prefetch_p_doubles(addr);
			#endif
			/*...Block 3: t04,t14,t24,t34	*/
			jt = j1 + p08;	jp = j2 + p08;

				rt =(t14-t15)*ISRT2;	it =(t14+t15)*ISRT2;			/* twiddle mpy by E^4	*/
				t14=t04-rt;			t04=t04+rt;
				t15=t05-it;			t05=t05+it;
			#if PFETCH
				addr = &a[jt+prefetch_offset];
				prefetch_p_doubles(addr);
			#endif
				rt =t24*c - t25*s;		t25=t25*c + t24*s;		t24=rt;	/* twiddle mpy by E^2	*/
				rt =t34*s - t35*c;		it =t35*s + t34*c;			/* twiddle mpy by E^6	*/
				t34=t24-rt;			t24=t24+rt;
				t35=t25-it;			t25=t25+it;
			#if PFETCH_X86
				addr += p01;
				prefetch_p_doubles(addr);
			#endif
				a[jt    ]=t04+t24;		a[jp    ]=t05+t25;
				a[jt+p01]=t04-t24;		a[jp+p01]=t05-t25;
			#if PFETCH_X86
				addr += p01;
				prefetch_p_doubles(addr);
			#endif
				a[jt+p02]=t14-t35;		a[jp+p02]=t15+t34;	/* mpy by E^4=i is inlined here...	*/
				a[jt+p03]=t14+t35;		a[jp+p03]=t15-t34;
			#if PFETCH_X86
				addr += p01;
				prefetch_p_doubles(addr);
			#endif
			/*...Block 7: t0C,t1C,t2C,t3C	*/
			jt = j1 + p0C;	jp = j2 + p0C;

				rt =(t1D+t1C)*ISRT2;	it =(t1D-t1C)*ISRT2;			/* twiddle mpy by -E^12 is here...	*/
				t1C=t0C+rt;			t0C=t0C-rt;				/* ...and get E^12 by flipping signs here.	*/
				t1D=t0D+it;			t0D=t0D-it;
			#if PFETCH
				addr = &a[jt+prefetch_offset];
				prefetch_p_doubles(addr);
			#endif
				rt =t2C*s - t2D*c;		t2D=t2D*s + t2C*c;		t2C=rt;	/* twiddle mpy by E^6	*/
				rt =t3C*c - t3D*s;		it =t3D*c + t3C*s;			/* twiddle mpy by E^18 is here...	*/
				t3C=t2C+rt;			t2C=t2C-rt;				/* ...and get E^18 by flipping signs here.	*/
				t3D=t2D+it;			t2D=t2D-it;
			#if PFETCH_X86
				addr += p01;
				prefetch_p_doubles(addr);
			#endif
				a[jt    ]=t0C+t2C;		a[jp    ]=t0D+t2D;
				a[jt+p01]=t0C-t2C;		a[jp+p01]=t0D-t2D;
			#if PFETCH_X86
				addr += p01;
				prefetch_p_doubles(addr);
			#endif
				a[jt+p02]=t1C-t3D;		a[jp+p02]=t1D+t3C;	/* mpy by E^4=i is inlined here...	*/
				a[jt+p03]=t1C+t3D;		a[jp+p03]=t1D-t3C;
			#if PFETCH_X86
				addr += p01;
				prefetch_p_doubles(addr);
			#endif
			/*...Block 2: t02,t12,t22,t32	*/
			jt = j1 + p10;	jp = j2 + p10;

				rt =t12*c - t13*s;		it =t13*c + t12*s;			/* twiddle mpy by E^2	*/
				t12=t02-rt;			t02=t02+rt;
				t13=t03-it;			t03=t03+it;
			#if PFETCH
				addr = &a[jt+prefetch_offset];
				prefetch_p_doubles(addr);
			#endif
				rt =t22*c32_1 - t23*s32_1;	t23=t23*c32_1 + t22*s32_1;	t22=rt;	/* twiddle mpy by E^1	*/
				rt =t32*c32_3 - t33*s32_3;	it =t33*c32_3 + t32*s32_3;		/* twiddle mpy by E^3	*/
				t32=t22-rt;			t22=t22+rt;
				t33=t23-it;			t23=t23+it;
			#if PFETCH_X86
				addr += p01;
				prefetch_p_doubles(addr);
			#endif
				a[jt    ]=t02+t22;		a[jp    ]=t03+t23;
				a[jt+p01]=t02-t22;		a[jp+p01]=t03-t23;
			#if PFETCH_X86
				addr += p01;
				prefetch_p_doubles(addr);
			#endif
				a[jt+p02]=t12-t33;		a[jp+p02]=t13+t32;	/* mpy by E^4=i is inlined here...;	*/
				a[jt+p03]=t12+t33;		a[jp+p03]=t13-t32;
			#if PFETCH_X86
				addr += p01;
				prefetch_p_doubles(addr);
			#endif
			/*...Block 6: t0A,t1A,t2A,t3A	*/
			jt = j1 + p14;	jp = j2 + p14;

				rt =t1A*s + t1B*c;		it =t1B*s - t1A*c;			/* twiddle mpy by -E^10 is here...	*/
				t1A=t0A+rt;			t0A =t0A-rt;				/* ...and get E^10 by flipping signs here.	*/
				t1B=t0B+it;			t0B =t0B-it;
			#if PFETCH
				addr = &a[jt+prefetch_offset];
				prefetch_p_doubles(addr);
			#endif
				rt =t2A*s32_3 - t2B*c32_3;	t2B=t2B*s32_3 + t2A*c32_3;	t2A=rt;	/* twiddle mpy by E^5	*/
				rt =t3A*c32_1 + t3B*s32_1;	it =t3B*c32_1 - t3A*s32_1;	 	/* twiddle mpy by -E^15 is here...	*/
				t3A=t2A+rt;			t2A=t2A-rt;				/* ...and get E^15 by flipping signs here.	*/
				t3B=t2B+it;			t2B=t2B-it;
			#if PFETCH_X86
				addr += p01;
				prefetch_p_doubles(addr);
			#endif
				a[jt    ]=t0A+t2A;		a[jp    ]=t0B+t2B;
				a[jt+p01]=t0A-t2A;		a[jp+p01]=t0B-t2B;
			#if PFETCH_X86
				addr += p01;
				prefetch_p_doubles(addr);
			#endif
				a[jt+p02]=t1A-t3B;		a[jp+p02]=t1B+t3A;	/* mpy by E^4=i is inlined here...	*/
				a[jt+p03]=t1A+t3B;		a[jp+p03]=t1B-t3A;
			#if PFETCH_X86
				addr += p01;
				prefetch_p_doubles(addr);
			#endif
			/*...Block 4: t06,t16,t26,t36	*/
			jt = j1 + p18;	jp = j2 + p18;

				rt =t16*s - t17*c;		it =t17*s + t16*c;			/* twiddle mpy by E^6	*/
				t16=t06-rt;			t06 =t06+rt;
				t17=t07-it;			t07 =t07+it;
			#if PFETCH
				addr = &a[jt+prefetch_offset];
				prefetch_p_doubles(addr);
			#endif
				rt =t26*c32_3 - t27*s32_3;	t27=t27*c32_3 + t26*s32_3;	t26=rt;	/* twiddle mpy by E^3	*/
				rt =t36*s32_1 + t37*c32_1;	it =t37*s32_1 - t36*c32_1;		/* twiddle mpy by -E^9 is here...	*/
				t36=t26+rt;			t26=t26-rt;				/* ...and get E^9 by flipping signs here.	*/
				t37=t27+it;			t27=t27-it;
			#if PFETCH_X86
				addr += p01;
				prefetch_p_doubles(addr);
			#endif
				a[jt    ]=t06+t26;		a[jp    ]=t07+t27;
				a[jt+p01]=t06-t26;		a[jp+p01]=t07-t27;
			#if PFETCH_X86
				addr += p01;
				prefetch_p_doubles(addr);
			#endif
				a[jt+p02]=t16-t37;		a[jp+p02]=t17+t36;	/* mpy by E^4=i is inlined here...	*/
				a[jt+p03]=t16+t37;		a[jp+p03]=t17-t36;
			#if PFETCH_X86
				addr += p01;
				prefetch_p_doubles(addr);
			#endif
			/*...Block 8: t0E,t1E,t2E,t3E	*/
			jt = j1 + p1C;	jp = j2 + p1C;

				rt =t1E*c + t1F*s;		it =t1F*c - t1E*s;			/* twiddle mpy by -E^14 is here...	*/
				t1E=t0E+rt;			t0E =t0E-rt;				/* ...and get E^14 by flipping signs here.	*/
				t1F=t0F+it;			t0F =t0F-it;
			#if PFETCH
				addr = &a[jt+prefetch_offset];
				prefetch_p_doubles(addr);
			#endif
				rt =t2E*s32_1 - t2F*c32_1;	t2F=t2F*s32_1 + t2E*c32_1;	t2E=rt;	/* twiddle mpy by E^7	*/
				rt =t3E*s32_3 - t3F*c32_3;	it =t3F*s32_3 + t3E*c32_3;		/* twiddle mpy by -E^21 is here...	*/
				t3E=t2E+rt;			t2E=t2E-rt;				/* ...and get E^21 by flipping signs here.	*/
				t3F=t2F+it;			t2F=t2F-it;
			#if PFETCH_X86
				addr += p01;
				prefetch_p_doubles(addr);
			#endif
				a[jt    ]=t0E+t2E;		a[jp    ]=t0F+t2F;
				a[jt+p01]=t0E-t2E;		a[jp+p01]=t0F-t2F;
			#if PFETCH_X86
				addr += p01;
				prefetch_p_doubles(addr);
			#endif
				a[jt+p02]=t1E-t3F;		a[jp+p02]=t1F+t3E;	/* mpy by E^4=i is inlined here...	*/
				a[jt+p03]=t1E+t3F;		a[jp+p03]=t1F-t3E;
			#if PFETCH_X86
				addr += p01;
				prefetch_p_doubles(addr);
			#endif

		#endif	/* #ifdef USE_SSE2 */

#ifdef CTIME
	clock3 = clock();
	dt_inv += (double)(clock3 - clock2);
	clock2 = clock3;
#endif
			}	/* end for(j=_jstart[ithread]; j < _jhi[ithread]; j += 2) */

			if(MODULUS_TYPE == MODULUS_TYPE_MERSENNE)
			{
				jstart += nwt;
				jhi    += nwt;

				col += RADIX;
				co3 -= RADIX;
			}
		}	/* end for(k=1; k <= khi; k++) */

		/* At end of each thread-processed work chunk, dump the
		carryouts into their non-thread-private array slots:
		*/
	#ifdef USE_SSE2
		if(MODULUS_TYPE == MODULUS_TYPE_MERSENNE)
		{
			_cy_r00[ithread] = cy_r00->re;
			_cy_r01[ithread] = cy_r00->im;
			_cy_r02[ithread] = cy_r02->re;
			_cy_r03[ithread] = cy_r02->im;
			_cy_r04[ithread] = cy_r04->re;
			_cy_r05[ithread] = cy_r04->im;
			_cy_r06[ithread] = cy_r06->re;
			_cy_r07[ithread] = cy_r06->im;
			_cy_r08[ithread] = cy_r08->re;
			_cy_r09[ithread] = cy_r08->im;
			_cy_r0A[ithread] = cy_r0A->re;
			_cy_r0B[ithread] = cy_r0A->im;
			_cy_r0C[ithread] = cy_r0C->re;
			_cy_r0D[ithread] = cy_r0C->im;
			_cy_r0E[ithread] = cy_r0E->re;
			_cy_r0F[ithread] = cy_r0E->im;
			_cy_r10[ithread] = cy_r10->re;
			_cy_r11[ithread] = cy_r10->im;
			_cy_r12[ithread] = cy_r12->re;
			_cy_r13[ithread] = cy_r12->im;
			_cy_r14[ithread] = cy_r14->re;
			_cy_r15[ithread] = cy_r14->im;
			_cy_r16[ithread] = cy_r16->re;
			_cy_r17[ithread] = cy_r16->im;
			_cy_r18[ithread] = cy_r18->re;
			_cy_r19[ithread] = cy_r18->im;
			_cy_r1A[ithread] = cy_r1A->re;
			_cy_r1B[ithread] = cy_r1A->im;
			_cy_r1C[ithread] = cy_r1C->re;
			_cy_r1D[ithread] = cy_r1C->im;
			_cy_r1E[ithread] = cy_r1E->re;
			_cy_r1F[ithread] = cy_r1E->im;
		}
		else
		{
			_cy_r00[ithread] = cy_r00->re;	_cy_i00[ithread] = cy_r00->im;
			_cy_r01[ithread] = cy_r02->re;	_cy_i01[ithread] = cy_r02->im;
			_cy_r02[ithread] = cy_r04->re;	_cy_i02[ithread] = cy_r04->im;
			_cy_r03[ithread] = cy_r06->re;	_cy_i03[ithread] = cy_r06->im;
			_cy_r04[ithread] = cy_r08->re;	_cy_i04[ithread] = cy_r08->im;
			_cy_r05[ithread] = cy_r0A->re;	_cy_i05[ithread] = cy_r0A->im;
			_cy_r06[ithread] = cy_r0C->re;	_cy_i06[ithread] = cy_r0C->im;
			_cy_r07[ithread] = cy_r0E->re;	_cy_i07[ithread] = cy_r0E->im;
			_cy_r08[ithread] = cy_r10->re;	_cy_i08[ithread] = cy_r10->im;
			_cy_r09[ithread] = cy_r12->re;	_cy_i09[ithread] = cy_r12->im;
			_cy_r0A[ithread] = cy_r14->re;	_cy_i0A[ithread] = cy_r14->im;
			_cy_r0B[ithread] = cy_r16->re;	_cy_i0B[ithread] = cy_r16->im;
			_cy_r0C[ithread] = cy_r18->re;	_cy_i0C[ithread] = cy_r18->im;
			_cy_r0D[ithread] = cy_r1A->re;	_cy_i0D[ithread] = cy_r1A->im;
			_cy_r0E[ithread] = cy_r1C->re;	_cy_i0E[ithread] = cy_r1C->im;
			_cy_r0F[ithread] = cy_r1E->re;	_cy_i0F[ithread] = cy_r1E->im;
			_cy_r10[ithread] = cy_i00->re;	_cy_i10[ithread] = cy_i00->im;
			_cy_r11[ithread] = cy_i02->re;	_cy_i11[ithread] = cy_i02->im;
			_cy_r12[ithread] = cy_i04->re;	_cy_i12[ithread] = cy_i04->im;
			_cy_r13[ithread] = cy_i06->re;	_cy_i13[ithread] = cy_i06->im;
			_cy_r14[ithread] = cy_i08->re;	_cy_i14[ithread] = cy_i08->im;
			_cy_r15[ithread] = cy_i0A->re;	_cy_i15[ithread] = cy_i0A->im;
			_cy_r16[ithread] = cy_i0C->re;	_cy_i16[ithread] = cy_i0C->im;
			_cy_r17[ithread] = cy_i0E->re;	_cy_i17[ithread] = cy_i0E->im;
			_cy_r18[ithread] = cy_i10->re;	_cy_i18[ithread] = cy_i10->im;
			_cy_r19[ithread] = cy_i12->re;	_cy_i19[ithread] = cy_i12->im;
			_cy_r1A[ithread] = cy_i14->re;	_cy_i1A[ithread] = cy_i14->im;
			_cy_r1B[ithread] = cy_i16->re;	_cy_i1B[ithread] = cy_i16->im;
			_cy_r1C[ithread] = cy_i18->re;	_cy_i1C[ithread] = cy_i18->im;
			_cy_r1D[ithread] = cy_i1A->re;	_cy_i1D[ithread] = cy_i1A->im;
			_cy_r1E[ithread] = cy_i1C->re;	_cy_i1E[ithread] = cy_i1C->im;
			_cy_r1F[ithread] = cy_i1E->re;	_cy_i1F[ithread] = cy_i1E->im;
		}
		if(max_err->re > max_err->im)
			maxerr = max_err->re;
		else
			maxerr = max_err->im;
	#else
		_cy_r00[ithread] = cy_r00;	_cy_i00[ithread] = cy_i00;
		_cy_r01[ithread] = cy_r01;	_cy_i01[ithread] = cy_i01;
		_cy_r02[ithread] = cy_r02;	_cy_i02[ithread] = cy_i02;
		_cy_r03[ithread] = cy_r03;	_cy_i03[ithread] = cy_i03;
		_cy_r04[ithread] = cy_r04;	_cy_i04[ithread] = cy_i04;
		_cy_r05[ithread] = cy_r05;	_cy_i05[ithread] = cy_i05;
		_cy_r06[ithread] = cy_r06;	_cy_i06[ithread] = cy_i06;
		_cy_r07[ithread] = cy_r07;	_cy_i07[ithread] = cy_i07;
		_cy_r08[ithread] = cy_r08;	_cy_i08[ithread] = cy_i08;
		_cy_r09[ithread] = cy_r09;	_cy_i09[ithread] = cy_i09;
		_cy_r0A[ithread] = cy_r0A;	_cy_i0A[ithread] = cy_i0A;
		_cy_r0B[ithread] = cy_r0B;	_cy_i0B[ithread] = cy_i0B;
		_cy_r0C[ithread] = cy_r0C;	_cy_i0C[ithread] = cy_i0C;
		_cy_r0D[ithread] = cy_r0D;	_cy_i0D[ithread] = cy_i0D;
		_cy_r0E[ithread] = cy_r0E;	_cy_i0E[ithread] = cy_i0E;
		_cy_r0F[ithread] = cy_r0F;	_cy_i0F[ithread] = cy_i0F;
		_cy_r10[ithread] = cy_r10;	_cy_i10[ithread] = cy_i10;
		_cy_r11[ithread] = cy_r11;	_cy_i11[ithread] = cy_i11;
		_cy_r12[ithread] = cy_r12;	_cy_i12[ithread] = cy_i12;
		_cy_r13[ithread] = cy_r13;	_cy_i13[ithread] = cy_i13;
		_cy_r14[ithread] = cy_r14;	_cy_i14[ithread] = cy_i14;
		_cy_r15[ithread] = cy_r15;	_cy_i15[ithread] = cy_i15;
		_cy_r16[ithread] = cy_r16;	_cy_i16[ithread] = cy_i16;
		_cy_r17[ithread] = cy_r17;	_cy_i17[ithread] = cy_i17;
		_cy_r18[ithread] = cy_r18;	_cy_i18[ithread] = cy_i18;
		_cy_r19[ithread] = cy_r19;	_cy_i19[ithread] = cy_i19;
		_cy_r1A[ithread] = cy_r1A;	_cy_i1A[ithread] = cy_i1A;
		_cy_r1B[ithread] = cy_r1B;	_cy_i1B[ithread] = cy_i1B;
		_cy_r1C[ithread] = cy_r1C;	_cy_i1C[ithread] = cy_i1C;
		_cy_r1D[ithread] = cy_r1D;	_cy_i1D[ithread] = cy_i1D;
		_cy_r1E[ithread] = cy_r1E;	_cy_i1E[ithread] = cy_i1E;
		_cy_r1F[ithread] = cy_r1F;	_cy_i1F[ithread] = cy_i1F;
	#endif

		/* Since will lose separate maxerr values when threads are merged, save them after each pass. */
		if(_maxerr[ithread] < maxerr)
		{
			_maxerr[ithread] = maxerr;
		}

  #endif	// #ifdef USE_PTHREAD

	}	/******* END OF PARALLEL FOR-LOOP ********/

#ifdef USE_PTHREAD	// End of threadpool-based dispatch: Add a small wait-loop to ensure all threads complete

  #ifdef OS_TYPE_MACOSX

	/*** Main execution thread executes remaining chunks in serial fashion (but in || with the pool threads): ***/
	for(j = 0; j < main_work_units; ++j)
	{
	//	printf("adding main task %d\n",j + pool_work_units);
		ASSERT(HERE, 0x0 == cy32_process_chunk( (void*)(&tdat[j + pool_work_units]) ), "Main-thread task failure!");
	}

  #endif

	struct timespec ns_time;
	ns_time.tv_sec  = 0.0001;// (time_t)seconds
	ns_time.tv_nsec = 0;	// (long)nanoseconds - At least allegedly, but under OS X it seems to be finer-grained than that
	
	while(tpool && tpool->free_tasks_queue.num_tasks != pool_work_units) {
		ASSERT(HERE, 0 == nanosleep(&ns_time, 0x0), "nanosleep fail!");
	}
//	printf("radix32_ditN_cy_dif1 end  ; #tasks = %d, #free_tasks = %d\n", tpool->tasks_queue.num_tasks, tpool->free_tasks_queue.num_tasks);

	/* Copy the thread-specific output carry data back to shared memory: */
	for(ithread = 0; ithread < CY_THREADS; ithread++)
	{
		_maxerr[ithread] = tdat[ithread].maxerr;
		if(maxerr < _maxerr[ithread]) {
			maxerr = _maxerr[ithread];
		}

		if(MODULUS_TYPE == MODULUS_TYPE_MERSENNE)
		{
			_cy_r00[ithread] = tdat[ithread].cy_r00;
			_cy_r01[ithread] = tdat[ithread].cy_r01;
			_cy_r02[ithread] = tdat[ithread].cy_r02;
			_cy_r03[ithread] = tdat[ithread].cy_r03;
			_cy_r04[ithread] = tdat[ithread].cy_r04;
			_cy_r05[ithread] = tdat[ithread].cy_r05;
			_cy_r06[ithread] = tdat[ithread].cy_r06;
			_cy_r07[ithread] = tdat[ithread].cy_r07;
			_cy_r08[ithread] = tdat[ithread].cy_r08;
			_cy_r09[ithread] = tdat[ithread].cy_r09;
			_cy_r0A[ithread] = tdat[ithread].cy_r0A;
			_cy_r0B[ithread] = tdat[ithread].cy_r0B;
			_cy_r0C[ithread] = tdat[ithread].cy_r0C;
			_cy_r0D[ithread] = tdat[ithread].cy_r0D;
			_cy_r0E[ithread] = tdat[ithread].cy_r0E;
			_cy_r0F[ithread] = tdat[ithread].cy_r0F;
			_cy_r10[ithread] = tdat[ithread].cy_r10;
			_cy_r11[ithread] = tdat[ithread].cy_r11;
			_cy_r12[ithread] = tdat[ithread].cy_r12;
			_cy_r13[ithread] = tdat[ithread].cy_r13;
			_cy_r14[ithread] = tdat[ithread].cy_r14;
			_cy_r15[ithread] = tdat[ithread].cy_r15;
			_cy_r16[ithread] = tdat[ithread].cy_r16;
			_cy_r17[ithread] = tdat[ithread].cy_r17;
			_cy_r18[ithread] = tdat[ithread].cy_r18;
			_cy_r19[ithread] = tdat[ithread].cy_r19;
			_cy_r1A[ithread] = tdat[ithread].cy_r1A;
			_cy_r1B[ithread] = tdat[ithread].cy_r1B;
			_cy_r1C[ithread] = tdat[ithread].cy_r1C;
			_cy_r1D[ithread] = tdat[ithread].cy_r1D;
			_cy_r1E[ithread] = tdat[ithread].cy_r1E;
			_cy_r1F[ithread] = tdat[ithread].cy_r1F;
		}
		else
		{
			_cy_r00[ithread] = tdat[ithread].cy_r00;	_cy_i00[ithread] = tdat[ithread].cy_i00;
			_cy_r01[ithread] = tdat[ithread].cy_r01;	_cy_i01[ithread] = tdat[ithread].cy_i01;
			_cy_r02[ithread] = tdat[ithread].cy_r02;	_cy_i02[ithread] = tdat[ithread].cy_i02;
			_cy_r03[ithread] = tdat[ithread].cy_r03;	_cy_i03[ithread] = tdat[ithread].cy_i03;
			_cy_r04[ithread] = tdat[ithread].cy_r04;	_cy_i04[ithread] = tdat[ithread].cy_i04;
			_cy_r05[ithread] = tdat[ithread].cy_r05;	_cy_i05[ithread] = tdat[ithread].cy_i05;
			_cy_r06[ithread] = tdat[ithread].cy_r06;	_cy_i06[ithread] = tdat[ithread].cy_i06;
			_cy_r07[ithread] = tdat[ithread].cy_r07;	_cy_i07[ithread] = tdat[ithread].cy_i07;
			_cy_r08[ithread] = tdat[ithread].cy_r08;	_cy_i08[ithread] = tdat[ithread].cy_i08;
			_cy_r09[ithread] = tdat[ithread].cy_r09;	_cy_i09[ithread] = tdat[ithread].cy_i09;
			_cy_r0A[ithread] = tdat[ithread].cy_r0A;	_cy_i0A[ithread] = tdat[ithread].cy_i0A;
			_cy_r0B[ithread] = tdat[ithread].cy_r0B;	_cy_i0B[ithread] = tdat[ithread].cy_i0B;
			_cy_r0C[ithread] = tdat[ithread].cy_r0C;	_cy_i0C[ithread] = tdat[ithread].cy_i0C;
			_cy_r0D[ithread] = tdat[ithread].cy_r0D;	_cy_i0D[ithread] = tdat[ithread].cy_i0D;
			_cy_r0E[ithread] = tdat[ithread].cy_r0E;	_cy_i0E[ithread] = tdat[ithread].cy_i0E;
			_cy_r0F[ithread] = tdat[ithread].cy_r0F;	_cy_i0F[ithread] = tdat[ithread].cy_i0F;
			_cy_r10[ithread] = tdat[ithread].cy_r10;	_cy_i10[ithread] = tdat[ithread].cy_i10;
			_cy_r11[ithread] = tdat[ithread].cy_r11;	_cy_i11[ithread] = tdat[ithread].cy_i11;
			_cy_r12[ithread] = tdat[ithread].cy_r12;	_cy_i12[ithread] = tdat[ithread].cy_i12;
			_cy_r13[ithread] = tdat[ithread].cy_r13;	_cy_i13[ithread] = tdat[ithread].cy_i13;
			_cy_r14[ithread] = tdat[ithread].cy_r14;	_cy_i14[ithread] = tdat[ithread].cy_i14;
			_cy_r15[ithread] = tdat[ithread].cy_r15;	_cy_i15[ithread] = tdat[ithread].cy_i15;
			_cy_r16[ithread] = tdat[ithread].cy_r16;	_cy_i16[ithread] = tdat[ithread].cy_i16;
			_cy_r17[ithread] = tdat[ithread].cy_r17;	_cy_i17[ithread] = tdat[ithread].cy_i17;
			_cy_r18[ithread] = tdat[ithread].cy_r18;	_cy_i18[ithread] = tdat[ithread].cy_i18;
			_cy_r19[ithread] = tdat[ithread].cy_r19;	_cy_i19[ithread] = tdat[ithread].cy_i19;
			_cy_r1A[ithread] = tdat[ithread].cy_r1A;	_cy_i1A[ithread] = tdat[ithread].cy_i1A;
			_cy_r1B[ithread] = tdat[ithread].cy_r1B;	_cy_i1B[ithread] = tdat[ithread].cy_i1B;
			_cy_r1C[ithread] = tdat[ithread].cy_r1C;	_cy_i1C[ithread] = tdat[ithread].cy_i1C;
			_cy_r1D[ithread] = tdat[ithread].cy_r1D;	_cy_i1D[ithread] = tdat[ithread].cy_i1D;
			_cy_r1E[ithread] = tdat[ithread].cy_r1E;	_cy_i1E[ithread] = tdat[ithread].cy_i1E;
			_cy_r1F[ithread] = tdat[ithread].cy_r1F;	_cy_i1F[ithread] = tdat[ithread].cy_i1F;
		}
	}

#endif

#if FFT_DEBUG
	fprintf(dbg_file,"Iter = %d:\n\n",iter);
	for(j = 0; j < n; j++) {
		j1 = j + ( (j >> DAT_BITS) << PAD_BITS );	/* padded-array fetch index is here */
		fprintf(dbg_file,"a[%d] = %20.10e %20.10e\n", j,a[j1],a[j1+1]);
	}
	if(iter > 0 && !full_pass) {
		fclose(dbg_file);
		dbg_file = 0x0;
		sprintf(cbuf, "Wrote debug file %s", dbg_fname);
		fprintf(stderr, "%s\n", cbuf);
		exit(0);
	}
#endif

	if(full_pass) {
	//	printf("Iter = %d, maxerr = %20.15f\n",iter,maxerr);
	} else {
		break;
	}

	/*   Wraparound carry cleanup loop is here:

	The cleanup carries from the end of each length-N/32 block into the begining of the next
	can all be neatly processed as follows:

	(1) Invert the radix-32 forward DIF FFT of the first block of 32 complex elements in A and unweight;
	(2) Propagate cleanup carries among the real and imaginary parts of the 32 outputs of (1);
	(3) Reweight and perform a radix-32 forward DIF FFT on the result of (2);
	(4) If any of the exit carries from (2) are nonzero, advance to the next 32 elements and repeat (1-4).
	*/
	if(MODULUS_TYPE == MODULUS_TYPE_MERSENNE)
	{
		t00= _cy_r00[CY_THREADS - 1];
		t02= _cy_r01[CY_THREADS - 1];
		t04= _cy_r02[CY_THREADS - 1];
		t06= _cy_r03[CY_THREADS - 1];
		t08= _cy_r04[CY_THREADS - 1];
		t0A= _cy_r05[CY_THREADS - 1];
		t0C= _cy_r06[CY_THREADS - 1];
		t0E= _cy_r07[CY_THREADS - 1];
		t10= _cy_r08[CY_THREADS - 1];
		t12= _cy_r09[CY_THREADS - 1];
		t14= _cy_r0A[CY_THREADS - 1];
		t16= _cy_r0B[CY_THREADS - 1];
		t18= _cy_r0C[CY_THREADS - 1];
		t1A= _cy_r0D[CY_THREADS - 1];
		t1C= _cy_r0E[CY_THREADS - 1];
		t1E= _cy_r0F[CY_THREADS - 1];
		t20= _cy_r10[CY_THREADS - 1];
		t22= _cy_r11[CY_THREADS - 1];
		t24= _cy_r12[CY_THREADS - 1];
		t26= _cy_r13[CY_THREADS - 1];
		t28= _cy_r14[CY_THREADS - 1];
		t2A= _cy_r15[CY_THREADS - 1];
		t2C= _cy_r16[CY_THREADS - 1];
		t2E= _cy_r17[CY_THREADS - 1];
		t30= _cy_r18[CY_THREADS - 1];
		t32= _cy_r19[CY_THREADS - 1];
		t34= _cy_r1A[CY_THREADS - 1];
		t36= _cy_r1B[CY_THREADS - 1];
		t38= _cy_r1C[CY_THREADS - 1];
		t3A= _cy_r1D[CY_THREADS - 1];
		t3C= _cy_r1E[CY_THREADS - 1];
		t3E= _cy_r1F[CY_THREADS - 1];

		for(ithread = CY_THREADS - 1; ithread > 0; ithread--)
		{
			ASSERT(HERE, CY_THREADS > 1,"radix32_ditN_cy_dif1.c: ");	/* Make sure loop only gets executed if multiple threads */
			_cy_r00[ithread] = _cy_r00[ithread-1];
			_cy_r01[ithread] = _cy_r01[ithread-1];
			_cy_r02[ithread] = _cy_r02[ithread-1];
			_cy_r03[ithread] = _cy_r03[ithread-1];
			_cy_r04[ithread] = _cy_r04[ithread-1];
			_cy_r05[ithread] = _cy_r05[ithread-1];
			_cy_r06[ithread] = _cy_r06[ithread-1];
			_cy_r07[ithread] = _cy_r07[ithread-1];
			_cy_r08[ithread] = _cy_r08[ithread-1];
			_cy_r09[ithread] = _cy_r09[ithread-1];
			_cy_r0A[ithread] = _cy_r0A[ithread-1];
			_cy_r0B[ithread] = _cy_r0B[ithread-1];
			_cy_r0C[ithread] = _cy_r0C[ithread-1];
			_cy_r0D[ithread] = _cy_r0D[ithread-1];
			_cy_r0E[ithread] = _cy_r0E[ithread-1];
			_cy_r0F[ithread] = _cy_r0F[ithread-1];
			_cy_r10[ithread] = _cy_r10[ithread-1];
			_cy_r11[ithread] = _cy_r11[ithread-1];
			_cy_r12[ithread] = _cy_r12[ithread-1];
			_cy_r13[ithread] = _cy_r13[ithread-1];
			_cy_r14[ithread] = _cy_r14[ithread-1];
			_cy_r15[ithread] = _cy_r15[ithread-1];
			_cy_r16[ithread] = _cy_r16[ithread-1];
			_cy_r17[ithread] = _cy_r17[ithread-1];
			_cy_r18[ithread] = _cy_r18[ithread-1];
			_cy_r19[ithread] = _cy_r19[ithread-1];
			_cy_r1A[ithread] = _cy_r1A[ithread-1];
			_cy_r1B[ithread] = _cy_r1B[ithread-1];
			_cy_r1C[ithread] = _cy_r1C[ithread-1];
			_cy_r1D[ithread] = _cy_r1D[ithread-1];
			_cy_r1E[ithread] = _cy_r1E[ithread-1];
			_cy_r1F[ithread] = _cy_r1F[ithread-1];
		}

		_cy_r00[0] =+t3E;	/* ...The wraparound carry is here: */
		_cy_r01[0] = t00;
		_cy_r02[0] = t02;
		_cy_r03[0] = t04;
		_cy_r04[0] = t06;
		_cy_r05[0] = t08;
		_cy_r06[0] = t0A;
		_cy_r07[0] = t0C;
		_cy_r08[0] = t0E;
		_cy_r09[0] = t10;
		_cy_r0A[0] = t12;
		_cy_r0B[0] = t14;
		_cy_r0C[0] = t16;
		_cy_r0D[0] = t18;
		_cy_r0E[0] = t1A;
		_cy_r0F[0] = t1C;
		_cy_r10[0] = t1E;
		_cy_r11[0] = t20;
		_cy_r12[0] = t22;
		_cy_r13[0] = t24;
		_cy_r14[0] = t26;
		_cy_r15[0] = t28;
		_cy_r16[0] = t2A;
		_cy_r17[0] = t2C;
		_cy_r18[0] = t2E;
		_cy_r19[0] = t30;
		_cy_r1A[0] = t32;
		_cy_r1B[0] = t34;
		_cy_r1C[0] = t36;
		_cy_r1D[0] = t38;
		_cy_r1E[0] = t3A;
		_cy_r1F[0] = t3C;
	}
	else
	{
		t00= _cy_r00[CY_THREADS - 1];	t01= _cy_i00[CY_THREADS - 1];
		t02= _cy_r01[CY_THREADS - 1];	t03= _cy_i01[CY_THREADS - 1];
		t04= _cy_r02[CY_THREADS - 1];	t05= _cy_i02[CY_THREADS - 1];
		t06= _cy_r03[CY_THREADS - 1];	t07= _cy_i03[CY_THREADS - 1];
		t08= _cy_r04[CY_THREADS - 1];	t09= _cy_i04[CY_THREADS - 1];
		t0A= _cy_r05[CY_THREADS - 1];	t0B= _cy_i05[CY_THREADS - 1];
		t0C= _cy_r06[CY_THREADS - 1];	t0D= _cy_i06[CY_THREADS - 1];
		t0E= _cy_r07[CY_THREADS - 1];	t0F= _cy_i07[CY_THREADS - 1];
		t10= _cy_r08[CY_THREADS - 1];	t11= _cy_i08[CY_THREADS - 1];
		t12= _cy_r09[CY_THREADS - 1];	t13= _cy_i09[CY_THREADS - 1];
		t14= _cy_r0A[CY_THREADS - 1];	t15= _cy_i0A[CY_THREADS - 1];
		t16= _cy_r0B[CY_THREADS - 1];	t17= _cy_i0B[CY_THREADS - 1];
		t18= _cy_r0C[CY_THREADS - 1];	t19= _cy_i0C[CY_THREADS - 1];
		t1A= _cy_r0D[CY_THREADS - 1];	t1B= _cy_i0D[CY_THREADS - 1];
		t1C= _cy_r0E[CY_THREADS - 1];	t1D= _cy_i0E[CY_THREADS - 1];
		t1E= _cy_r0F[CY_THREADS - 1];	t1F= _cy_i0F[CY_THREADS - 1];
		t20= _cy_r10[CY_THREADS - 1];	t21= _cy_i10[CY_THREADS - 1];
		t22= _cy_r11[CY_THREADS - 1];	t23= _cy_i11[CY_THREADS - 1];
		t24= _cy_r12[CY_THREADS - 1];	t25= _cy_i12[CY_THREADS - 1];
		t26= _cy_r13[CY_THREADS - 1];	t27= _cy_i13[CY_THREADS - 1];
		t28= _cy_r14[CY_THREADS - 1];	t29= _cy_i14[CY_THREADS - 1];
		t2A= _cy_r15[CY_THREADS - 1];	t2B= _cy_i15[CY_THREADS - 1];
		t2C= _cy_r16[CY_THREADS - 1];	t2D= _cy_i16[CY_THREADS - 1];
		t2E= _cy_r17[CY_THREADS - 1];	t2F= _cy_i17[CY_THREADS - 1];
		t30= _cy_r18[CY_THREADS - 1];	t31= _cy_i18[CY_THREADS - 1];
		t32= _cy_r19[CY_THREADS - 1];	t33= _cy_i19[CY_THREADS - 1];
		t34= _cy_r1A[CY_THREADS - 1];	t35= _cy_i1A[CY_THREADS - 1];
		t36= _cy_r1B[CY_THREADS - 1];	t37= _cy_i1B[CY_THREADS - 1];
		t38= _cy_r1C[CY_THREADS - 1];	t39= _cy_i1C[CY_THREADS - 1];
		t3A= _cy_r1D[CY_THREADS - 1];	t3B= _cy_i1D[CY_THREADS - 1];
		t3C= _cy_r1E[CY_THREADS - 1];	t3D= _cy_i1E[CY_THREADS - 1];
		t3E= _cy_r1F[CY_THREADS - 1];	t3F= _cy_i1F[CY_THREADS - 1];

		for(ithread = CY_THREADS - 1; ithread > 0; ithread--)
		{
			ASSERT(HERE, CY_THREADS > 1,"radix32_ditN_cy_dif1.c: ");	/* Make sure loop only gets executed if multiple threads */
			_cy_r00[ithread] = _cy_r00[ithread-1];		_cy_i00[ithread] = _cy_i00[ithread-1];
			_cy_r01[ithread] = _cy_r01[ithread-1];		_cy_i01[ithread] = _cy_i01[ithread-1];
			_cy_r02[ithread] = _cy_r02[ithread-1];		_cy_i02[ithread] = _cy_i02[ithread-1];
			_cy_r03[ithread] = _cy_r03[ithread-1];		_cy_i03[ithread] = _cy_i03[ithread-1];
			_cy_r04[ithread] = _cy_r04[ithread-1];		_cy_i04[ithread] = _cy_i04[ithread-1];
			_cy_r05[ithread] = _cy_r05[ithread-1];		_cy_i05[ithread] = _cy_i05[ithread-1];
			_cy_r06[ithread] = _cy_r06[ithread-1];		_cy_i06[ithread] = _cy_i06[ithread-1];
			_cy_r07[ithread] = _cy_r07[ithread-1];		_cy_i07[ithread] = _cy_i07[ithread-1];
			_cy_r08[ithread] = _cy_r08[ithread-1];		_cy_i08[ithread] = _cy_i08[ithread-1];
			_cy_r09[ithread] = _cy_r09[ithread-1];		_cy_i09[ithread] = _cy_i09[ithread-1];
			_cy_r0A[ithread] = _cy_r0A[ithread-1];		_cy_i0A[ithread] = _cy_i0A[ithread-1];
			_cy_r0B[ithread] = _cy_r0B[ithread-1];		_cy_i0B[ithread] = _cy_i0B[ithread-1];
			_cy_r0C[ithread] = _cy_r0C[ithread-1];		_cy_i0C[ithread] = _cy_i0C[ithread-1];
			_cy_r0D[ithread] = _cy_r0D[ithread-1];		_cy_i0D[ithread] = _cy_i0D[ithread-1];
			_cy_r0E[ithread] = _cy_r0E[ithread-1];		_cy_i0E[ithread] = _cy_i0E[ithread-1];
			_cy_r0F[ithread] = _cy_r0F[ithread-1];		_cy_i0F[ithread] = _cy_i0F[ithread-1];
			_cy_r10[ithread] = _cy_r10[ithread-1];		_cy_i10[ithread] = _cy_i10[ithread-1];
			_cy_r11[ithread] = _cy_r11[ithread-1];		_cy_i11[ithread] = _cy_i11[ithread-1];
			_cy_r12[ithread] = _cy_r12[ithread-1];		_cy_i12[ithread] = _cy_i12[ithread-1];
			_cy_r13[ithread] = _cy_r13[ithread-1];		_cy_i13[ithread] = _cy_i13[ithread-1];
			_cy_r14[ithread] = _cy_r14[ithread-1];		_cy_i14[ithread] = _cy_i14[ithread-1];
			_cy_r15[ithread] = _cy_r15[ithread-1];		_cy_i15[ithread] = _cy_i15[ithread-1];
			_cy_r16[ithread] = _cy_r16[ithread-1];		_cy_i16[ithread] = _cy_i16[ithread-1];
			_cy_r17[ithread] = _cy_r17[ithread-1];		_cy_i17[ithread] = _cy_i17[ithread-1];
			_cy_r18[ithread] = _cy_r18[ithread-1];		_cy_i18[ithread] = _cy_i18[ithread-1];
			_cy_r19[ithread] = _cy_r19[ithread-1];		_cy_i19[ithread] = _cy_i19[ithread-1];
			_cy_r1A[ithread] = _cy_r1A[ithread-1];		_cy_i1A[ithread] = _cy_i1A[ithread-1];
			_cy_r1B[ithread] = _cy_r1B[ithread-1];		_cy_i1B[ithread] = _cy_i1B[ithread-1];
			_cy_r1C[ithread] = _cy_r1C[ithread-1];		_cy_i1C[ithread] = _cy_i1C[ithread-1];
			_cy_r1D[ithread] = _cy_r1D[ithread-1];		_cy_i1D[ithread] = _cy_i1D[ithread-1];
			_cy_r1E[ithread] = _cy_r1E[ithread-1];		_cy_i1E[ithread] = _cy_i1E[ithread-1];
			_cy_r1F[ithread] = _cy_r1F[ithread-1];		_cy_i1F[ithread] = _cy_i1F[ithread-1];
		}

		_cy_r00[0] =-t3F;	_cy_i00[0] =+t3E;	/* ...The 2 Mo"bius carries are here: */
		_cy_r01[0] = t00;	_cy_i01[0] = t01;
		_cy_r02[0] = t02;	_cy_i02[0] = t03;
		_cy_r03[0] = t04;	_cy_i03[0] = t05;
		_cy_r04[0] = t06;	_cy_i04[0] = t07;
		_cy_r05[0] = t08;	_cy_i05[0] = t09;
		_cy_r06[0] = t0A;	_cy_i06[0] = t0B;
		_cy_r07[0] = t0C;	_cy_i07[0] = t0D;
		_cy_r08[0] = t0E;	_cy_i08[0] = t0F;
		_cy_r09[0] = t10;	_cy_i09[0] = t11;
		_cy_r0A[0] = t12;	_cy_i0A[0] = t13;
		_cy_r0B[0] = t14;	_cy_i0B[0] = t15;
		_cy_r0C[0] = t16;	_cy_i0C[0] = t17;
		_cy_r0D[0] = t18;	_cy_i0D[0] = t19;
		_cy_r0E[0] = t1A;	_cy_i0E[0] = t1B;
		_cy_r0F[0] = t1C;	_cy_i0F[0] = t1D;
		_cy_r10[0] = t1E;	_cy_i10[0] = t1F;
		_cy_r11[0] = t20;	_cy_i11[0] = t21;
		_cy_r12[0] = t22;	_cy_i12[0] = t23;
		_cy_r13[0] = t24;	_cy_i13[0] = t25;
		_cy_r14[0] = t26;	_cy_i14[0] = t27;
		_cy_r15[0] = t28;	_cy_i15[0] = t29;
		_cy_r16[0] = t2A;	_cy_i16[0] = t2B;
		_cy_r17[0] = t2C;	_cy_i17[0] = t2D;
		_cy_r18[0] = t2E;	_cy_i18[0] = t2F;
		_cy_r19[0] = t30;	_cy_i19[0] = t31;
		_cy_r1A[0] = t32;	_cy_i1A[0] = t33;
		_cy_r1B[0] = t34;	_cy_i1B[0] = t35;
		_cy_r1C[0] = t36;	_cy_i1C[0] = t37;
		_cy_r1D[0] = t38;	_cy_i1D[0] = t39;
		_cy_r1E[0] = t3A;	_cy_i1E[0] = t3B;
		_cy_r1F[0] = t3C;	_cy_i1F[0] = t3D;
	}

	full_pass = 0;
	scale = 1;

	/*
	For right-angle transform need *complex* elements for wraparound, so jhi needs to be twice as large
	*/
	if(TRANSFORM_TYPE == RIGHT_ANGLE)
	{
		j_jhi =15;
	}
	else
	{
		j_jhi = 7;
	}

    for(ithread = 0; ithread < CY_THREADS; ithread++)
    {
		for(j = ithread*pini; j <= ithread*pini + j_jhi; j++)
		{
			k = j;
			a[k    ] *= radix_inv;
			a[k+p01] *= radix_inv;
			a[k+p02] *= radix_inv;
			a[k+p03] *= radix_inv;
			a[k+p04] *= radix_inv;
			a[k+p05] *= radix_inv;
			a[k+p06] *= radix_inv;
			a[k+p07] *= radix_inv;
			k += p08;
			a[k    ] *= radix_inv;
			a[k+p01] *= radix_inv;
			a[k+p02] *= radix_inv;
			a[k+p03] *= radix_inv;
			a[k+p04] *= radix_inv;
			a[k+p05] *= radix_inv;
			a[k+p06] *= radix_inv;
			a[k+p07] *= radix_inv;
			k += p08;
			a[k    ] *= radix_inv;
			a[k+p01] *= radix_inv;
			a[k+p02] *= radix_inv;
			a[k+p03] *= radix_inv;
			a[k+p04] *= radix_inv;
			a[k+p05] *= radix_inv;
			a[k+p06] *= radix_inv;
			a[k+p07] *= radix_inv;
			k += p08;
			a[k    ] *= radix_inv;
			a[k+p01] *= radix_inv;
			a[k+p02] *= radix_inv;
			a[k+p03] *= radix_inv;
			a[k+p04] *= radix_inv;
			a[k+p05] *= radix_inv;
			a[k+p06] *= radix_inv;
			a[k+p07] *= radix_inv;
		}
    }
}	/* endfor(outer) */

#ifdef CTIME
	clock2 = clock();
	dt_tot = (double)(clock2 - clock1);
	printf("radix32_carry cycle times: total = %10.5f, fwd = %10.5f, inv = %10.5f, cy = %10.5f\n", dt_tot*ICPS, dt_fwd*ICPS, dt_inv*ICPS, dt_cy*ICPS);
#endif

	t00 = 0;
    for(ithread = 0; ithread < CY_THREADS; ithread++)
    {
		t00 += fabs(_cy_r00[0])+fabs(_cy_r01[0])+fabs(_cy_r02[0])+fabs(_cy_r03[0])+fabs(_cy_r04[0])+fabs(_cy_r05[0])+fabs(_cy_r06[0])+fabs(_cy_r07[0])+fabs(_cy_r08[0])+fabs(_cy_r09[0])+fabs(_cy_r0A[0])+fabs(_cy_r0B[0])+fabs(_cy_r0C[0])+fabs(_cy_r0D[0])+fabs(_cy_r0E[0])+fabs(_cy_r0F[0])+fabs(_cy_r10[0])+fabs(_cy_r11[0])+fabs(_cy_r12[0])+fabs(_cy_r13[0])+fabs(_cy_r14[0])+fabs(_cy_r15[0])+fabs(_cy_r16[0])+fabs(_cy_r17[0])+fabs(_cy_r18[0])+fabs(_cy_r19[0])+fabs(_cy_r1A[0])+fabs(_cy_r1B[0])+fabs(_cy_r1C[0])+fabs(_cy_r1D[0])+fabs(_cy_r1E[0])+fabs(_cy_r1F[0]);
		t00 += fabs(_cy_i00[0])+fabs(_cy_i01[0])+fabs(_cy_i02[0])+fabs(_cy_i03[0])+fabs(_cy_i04[0])+fabs(_cy_i05[0])+fabs(_cy_i06[0])+fabs(_cy_i07[0])+fabs(_cy_i08[0])+fabs(_cy_i09[0])+fabs(_cy_i0A[0])+fabs(_cy_i0B[0])+fabs(_cy_i0C[0])+fabs(_cy_i0D[0])+fabs(_cy_i0E[0])+fabs(_cy_i0F[0])+fabs(_cy_i10[0])+fabs(_cy_i11[0])+fabs(_cy_i12[0])+fabs(_cy_i13[0])+fabs(_cy_i14[0])+fabs(_cy_i15[0])+fabs(_cy_i16[0])+fabs(_cy_i17[0])+fabs(_cy_i18[0])+fabs(_cy_i19[0])+fabs(_cy_i1A[0])+fabs(_cy_i1B[0])+fabs(_cy_i1C[0])+fabs(_cy_i1D[0])+fabs(_cy_i1E[0])+fabs(_cy_i1F[0]);

		if(*fracmax < _maxerr[ithread])
			*fracmax = _maxerr[ithread];
    }

	if(t00 != 0.0)
	{
		sprintf(cbuf,"FATAL: iter = %10d; nonzero exit carry in radix32_ditN_cy_dif1 - input wordsize may be too small.\n",iter);
		if(INTERACT)fprintf(stderr,"%s",cbuf);
		fp = fopen(   OFILE,"a");
		fq = fopen(STATFILE,"a");
		fprintf(fp,"%s",cbuf);
		fprintf(fq,"%s",cbuf);
		fclose(fp);	fp = 0x0;
		fclose(fq);	fq = 0x0;
		err=ERR_CARRY;
		return(err);
	}

	return(0);
}

#endif	/* GCD_STANDALONE */

/***************/

#define USE_SCALAR_DFT_MACRO	1

void radix32_dif_pass1(double a[], int n)
{
/*
!...Acronym: DIF = Decimation In Frequency
!
!...Subroutine to perform an initial radix-32 complex DIF FFT pass on the data in the length-N real vector A.
!
!   The data are stored in a 1-D zero-offset array, with 2^PAD_BITS 8-byte padding elements inserted
!   between every block of 2^DAT_BITS contiguous data. The array padding is to prevent data being accessed
!   in strides that are large powers of two and thus to minimize cache thrashing
!   (in cache-based microprocessor architectures) or bank conflicts (in supercomputers.)
!
!   See the documentation in radix16_dif_pass for further details on storage and indexing.
*/
	int j,j1,j2,arr_offsets[32];
	static int n32,p01,p02,p03,p04,p08,p0C,p10,p14,p18,p1C, first_entry=TRUE;
	static double    c     = 0.92387953251128675613, s     = 0.38268343236508977173	/* exp(  i*twopi/16)	*/
			,c32_1 = 0.98078528040323044912, s32_1 = 0.19509032201612826784	/* exp(  i*twopi/32)	*/
			,c32_3 = 0.83146961230254523708, s32_3 = 0.55557023301960222473;/* exp(3*i*twopi/32)	*/
#ifdef USE_SSE2
	double *add0;	/* Addresses into array sections */
	static struct complex *sc_arr = 0x0, *sc_ptr;
	static struct complex *isrt2, *cc0, *ss0, *cc1, *ss1, *cc3, *ss3
		,*r00,*r01,*r02,*r03,*r04,*r05,*r06,*r07,*r08,*r09,*r0A,*r0B,*r0C,*r0D,*r0E,*r0F
		,*r10,*r11,*r12,*r13,*r14,*r15,*r16,*r17,*r18,*r19,*r1A,*r1B,*r1C,*r1D,*r1E,*r1F
		,*r20,*r21,*r22,*r23,*r24,*r25,*r26,*r27,*r28,*r29,*r2A,*r2B,*r2C,*r2D,*r2E,*r2F
		,*r30,*r31,*r32,*r33,*r34,*r35,*r36,*r37,*r38,*r39,*r3A,*r3B,*r3C,*r3D,*r3E,*r3F;
#elif !USE_SCALAR_DFT_MACRO
	int jt,jp;
	double rt,it
		,t00,t01,t02,t03,t04,t05,t06,t07,t08,t09,t0A,t0B,t0C,t0D,t0E,t0F
		,t10,t11,t12,t13,t14,t15,t16,t17,t18,t19,t1A,t1B,t1C,t1D,t1E,t1F
		,t20,t21,t22,t23,t24,t25,t26,t27,t28,t29,t2A,t2B,t2C,t2D,t2E,t2F
		,t30,t31,t32,t33,t34,t35,t36,t37,t38,t39,t3A,t3B,t3C,t3D,t3E,t3F;
#endif

	if(!first_entry && (n >> 5) != n32)	/* New runlength?	*/
	{
		first_entry=TRUE;
	}

/*...initialize things upon first entry	*/

	if(first_entry)
	{
		first_entry=FALSE;
		n32=n/32;

		p01 = n32;
		p02 = p01 +p01;
		p03 = p02 +p01;
		p04 = p03 +p01;
		p08 = p04 +p04;
		p0C = p08 +p04;
		p10 = p0C +p04;
		p14 = p10 +p04;
		p18 = p14 +p04;
		p1C = p18 +p04;

		p01 = p01 + ( (p01 >> DAT_BITS) << PAD_BITS );
		p02 = p02 + ( (p02 >> DAT_BITS) << PAD_BITS );
		p03 = p03 + ( (p03 >> DAT_BITS) << PAD_BITS );
		p04 = p04 + ( (p04 >> DAT_BITS) << PAD_BITS );
		p08 = p08 + ( (p08 >> DAT_BITS) << PAD_BITS );
		p0C = p0C + ( (p0C >> DAT_BITS) << PAD_BITS );
		p10 = p10 + ( (p10 >> DAT_BITS) << PAD_BITS );
		p14 = p14 + ( (p14 >> DAT_BITS) << PAD_BITS );
		p18 = p18 + ( (p18 >> DAT_BITS) << PAD_BITS );
		p1C = p1C + ( (p1C >> DAT_BITS) << PAD_BITS );

		arr_offsets[0x00] = 0;
		arr_offsets[0x01] = p01;
		arr_offsets[0x02] = p02;
		arr_offsets[0x03] = p03;
		arr_offsets[0x04] = p04;
		arr_offsets[0x05] = p04+p01;
		arr_offsets[0x06] = p04+p02;
		arr_offsets[0x07] = p04+p03;
		arr_offsets[0x08] = p08;
		arr_offsets[0x09] = p08+p01;
		arr_offsets[0x0A] = p08+p02;
		arr_offsets[0x0B] = p08+p03;
		arr_offsets[0x0C] = p0C;
		arr_offsets[0x0D] = p0C+p01;
		arr_offsets[0x0E] = p0C+p02;
		arr_offsets[0x0F] = p0C+p03;
		arr_offsets[0x10] = p10;
		arr_offsets[0x11] = p10+p01;
		arr_offsets[0x12] = p10+p02;
		arr_offsets[0x13] = p10+p03;
		arr_offsets[0x14] = p14;
		arr_offsets[0x15] = p14+p01;
		arr_offsets[0x16] = p14+p02;
		arr_offsets[0x17] = p14+p03;
		arr_offsets[0x18] = p18;
		arr_offsets[0x19] = p18+p01;
		arr_offsets[0x1A] = p18+p02;
		arr_offsets[0x1B] = p18+p03;
		arr_offsets[0x1C] = p1C;
		arr_offsets[0x1D] = p1C+p01;
		arr_offsets[0x1E] = p1C+p02;
		arr_offsets[0x1F] = p1C+p03;

	#ifdef USE_SSE2

		sc_arr = ALLOC_COMPLEX(sc_arr, radix32_creals_in_local_store);	if(!sc_arr){ sprintf(cbuf, "FATAL: unable to allocate sc_arr!.\n"); fprintf(stderr,"%s", cbuf);	ASSERT(HERE, 0,cbuf); }
		sc_ptr = ALIGN_COMPLEX(sc_arr);
		ASSERT(HERE, ((uint32)sc_ptr & 0x3f) == 0, "sc_ptr not 64-byte aligned!");

	/* Use low 64 16-byte slots of sc_arr for temporaries, next 7 for the nontrivial complex 16th roots,
	next 32 for the doubled carry pairs, next 2 for ROE and RND_CONST, next 20 for the half_arr table lookup stuff,
	plus at least 3 more slots to allow for 64-byte alignment of the array:
	*/
		r00		= sc_ptr + 0x00;	r01		= sc_ptr + 0x01;
		r02		= sc_ptr + 0x02;	r03		= sc_ptr + 0x03;
		r04		= sc_ptr + 0x04;	r05		= sc_ptr + 0x05;
		r06		= sc_ptr + 0x06;	r07		= sc_ptr + 0x07;
		r08		= sc_ptr + 0x08;	r09		= sc_ptr + 0x09;
		r0A		= sc_ptr + 0x0a;	r0B		= sc_ptr + 0x0b;
		r0C		= sc_ptr + 0x0c;	r0D		= sc_ptr + 0x0d;
		r0E		= sc_ptr + 0x0e;	r0F		= sc_ptr + 0x0f;
		r10		= sc_ptr + 0x10;	r11		= sc_ptr + 0x11;
		r12		= sc_ptr + 0x12;	r13		= sc_ptr + 0x13;
		r14		= sc_ptr + 0x14;	r15		= sc_ptr + 0x15;
		r16		= sc_ptr + 0x16;	r17		= sc_ptr + 0x17;
		r18		= sc_ptr + 0x18;	r19		= sc_ptr + 0x19;
		r1A		= sc_ptr + 0x1a;	r1B		= sc_ptr + 0x1b;
		r1C		= sc_ptr + 0x1c;	r1D		= sc_ptr + 0x1d;
		r1E		= sc_ptr + 0x1e;	r1F		= sc_ptr + 0x1f;
		r20		= sc_ptr + 0x20;	r21		= sc_ptr + 0x21;
		r22		= sc_ptr + 0x22;	r23		= sc_ptr + 0x23;
		r24		= sc_ptr + 0x24;	r25		= sc_ptr + 0x25;
		r26		= sc_ptr + 0x26;	r27		= sc_ptr + 0x27;
		r28		= sc_ptr + 0x28;	r29		= sc_ptr + 0x29;
		r2A		= sc_ptr + 0x2a;	r2B		= sc_ptr + 0x2b;
		r2C		= sc_ptr + 0x2c;	r2D		= sc_ptr + 0x2d;
		r2E		= sc_ptr + 0x2e;	r2F		= sc_ptr + 0x2f;
		r30		= sc_ptr + 0x30;	r31		= sc_ptr + 0x31;
		r32		= sc_ptr + 0x32;	r33		= sc_ptr + 0x33;
		r34		= sc_ptr + 0x34;	r35		= sc_ptr + 0x35;
		r36		= sc_ptr + 0x36;	r37		= sc_ptr + 0x37;
		r38		= sc_ptr + 0x38;	r39		= sc_ptr + 0x39;
		r3A		= sc_ptr + 0x3a;	r3B		= sc_ptr + 0x3b;
		r3C		= sc_ptr + 0x3c;	r3D		= sc_ptr + 0x3d;
		r3E		= sc_ptr + 0x3e;	r3F		= sc_ptr + 0x3f;
		isrt2	= sc_ptr + 0x40;
		cc0		= sc_ptr + 0x41;	ss0		= sc_ptr + 0x42;
		cc1		= sc_ptr + 0x43;	ss1		= sc_ptr + 0x44;
		cc3		= sc_ptr + 0x45;	ss3		= sc_ptr + 0x46;

		/* These remain fixed: */
		isrt2->re = ISRT2;	isrt2->im = ISRT2;
		cc0  ->re = c	;	cc0  ->im = c	;		ss0  ->re = s	;	ss0  ->im = s	;
		cc1  ->re = c32_1;	cc1  ->im = c32_1;		ss1  ->re = s32_1;	ss1  ->im = s32_1;
		cc3  ->re = c32_3;	cc3  ->im = c32_3;		ss3  ->re = s32_3;	ss3  ->im = s32_3;

	#endif
	}

/*...The radix-32 pass is here.	*/

#ifdef USE_SSE2
	for(j = 0; j < n32; j += 4)
	{
	/* In SSE2 mode, data are arranged in [re0,re1,im0,im1] quartets, not the usual [re0,im0],[re1,im1] pairs.
	Thus we can still increment the j-index as if stepping through the residue array-of-doubles in strides of 2,
	but to point to the proper real datum, we need to bit-reverse bits <0:1> of j, i.e. [0,1,2,3] ==> [0,2,1,3].
	*/
		j1 = (j & mask01) + br4[j&3];
#else
	for(j = 0; j < n32; j += 2)	/* Each inner loop execution processes (radix(1)*nwt) array data.	*/
	{
		j1 =  j;
#endif
		j1 = j1 + ( (j1 >> DAT_BITS) << PAD_BITS );	/* padded-array fetch index is here */
		j2 = j1+RE_IM_STRIDE;

	#ifdef USE_SSE2

		/* Copy inputs from main array into local-store to be compatible with the data layout assumed by the SSE2_RADIX32_DIF macro: */
		r00->re = a[j1        ];	r00->im = a[j1        +1];	r01->re = a[j1        +RE_IM_STRIDE];	r01->im = a[j1        +RE_IM_STRIDE+1];
		r02->re = a[j1    +p01];	r02->im = a[j1    +p01+1];	r03->re = a[j1    +p01+RE_IM_STRIDE];	r03->im = a[j1    +p01+RE_IM_STRIDE+1];
		r04->re = a[j1    +p02];	r04->im = a[j1    +p02+1];	r05->re = a[j1    +p02+RE_IM_STRIDE];	r05->im = a[j1    +p02+RE_IM_STRIDE+1];
		r06->re = a[j1    +p03];	r06->im = a[j1    +p03+1];	r07->re = a[j1    +p03+RE_IM_STRIDE];	r07->im = a[j1    +p03+RE_IM_STRIDE+1];
		r08->re = a[j1+p04    ];	r08->im = a[j1+p04    +1];	r09->re = a[j1+p04    +RE_IM_STRIDE];	r09->im = a[j1+p04    +RE_IM_STRIDE+1];
		r0A->re = a[j1+p04+p01];	r0A->im = a[j1+p04+p01+1];	r0B->re = a[j1+p04+p01+RE_IM_STRIDE];	r0B->im = a[j1+p04+p01+RE_IM_STRIDE+1];
		r0C->re = a[j1+p04+p02];	r0C->im = a[j1+p04+p02+1];	r0D->re = a[j1+p04+p02+RE_IM_STRIDE];	r0D->im = a[j1+p04+p02+RE_IM_STRIDE+1];
		r0E->re = a[j1+p04+p03];	r0E->im = a[j1+p04+p03+1];	r0F->re = a[j1+p04+p03+RE_IM_STRIDE];	r0F->im = a[j1+p04+p03+RE_IM_STRIDE+1];
		r10->re = a[j1+p08    ];	r10->im = a[j1+p08    +1];	r11->re = a[j1+p08    +RE_IM_STRIDE];	r11->im = a[j1+p08    +RE_IM_STRIDE+1];
		r12->re = a[j1+p08+p01];	r12->im = a[j1+p08+p01+1];	r13->re = a[j1+p08+p01+RE_IM_STRIDE];	r13->im = a[j1+p08+p01+RE_IM_STRIDE+1];
		r14->re = a[j1+p08+p02];	r14->im = a[j1+p08+p02+1];	r15->re = a[j1+p08+p02+RE_IM_STRIDE];	r15->im = a[j1+p08+p02+RE_IM_STRIDE+1];
		r16->re = a[j1+p08+p03];	r16->im = a[j1+p08+p03+1];	r17->re = a[j1+p08+p03+RE_IM_STRIDE];	r17->im = a[j1+p08+p03+RE_IM_STRIDE+1];
		r18->re = a[j1+p0C    ];	r18->im = a[j1+p0C    +1];	r19->re = a[j1+p0C    +RE_IM_STRIDE];	r19->im = a[j1+p0C    +RE_IM_STRIDE+1];
		r1A->re = a[j1+p0C+p01];	r1A->im = a[j1+p0C+p01+1];	r1B->re = a[j1+p0C+p01+RE_IM_STRIDE];	r1B->im = a[j1+p0C+p01+RE_IM_STRIDE+1];
		r1C->re = a[j1+p0C+p02];	r1C->im = a[j1+p0C+p02+1];	r1D->re = a[j1+p0C+p02+RE_IM_STRIDE];	r1D->im = a[j1+p0C+p02+RE_IM_STRIDE+1];
		r1E->re = a[j1+p0C+p03];	r1E->im = a[j1+p0C+p03+1];	r1F->re = a[j1+p0C+p03+RE_IM_STRIDE];	r1F->im = a[j1+p0C+p03+RE_IM_STRIDE+1];
		r20->re = a[j1+p10    ];	r20->im = a[j1+p10    +1];	r21->re = a[j1+p10    +RE_IM_STRIDE];	r21->im = a[j1+p10    +RE_IM_STRIDE+1];
		r22->re = a[j1+p10+p01];	r22->im = a[j1+p10+p01+1];	r23->re = a[j1+p10+p01+RE_IM_STRIDE];	r23->im = a[j1+p10+p01+RE_IM_STRIDE+1];
		r24->re = a[j1+p10+p02];	r24->im = a[j1+p10+p02+1];	r25->re = a[j1+p10+p02+RE_IM_STRIDE];	r25->im = a[j1+p10+p02+RE_IM_STRIDE+1];
		r26->re = a[j1+p10+p03];	r26->im = a[j1+p10+p03+1];	r27->re = a[j1+p10+p03+RE_IM_STRIDE];	r27->im = a[j1+p10+p03+RE_IM_STRIDE+1];
		r28->re = a[j1+p14    ];	r28->im = a[j1+p14    +1];	r29->re = a[j1+p14    +RE_IM_STRIDE];	r29->im = a[j1+p14    +RE_IM_STRIDE+1];
		r2A->re = a[j1+p14+p01];	r2A->im = a[j1+p14+p01+1];	r2B->re = a[j1+p14+p01+RE_IM_STRIDE];	r2B->im = a[j1+p14+p01+RE_IM_STRIDE+1];
		r2C->re = a[j1+p14+p02];	r2C->im = a[j1+p14+p02+1];	r2D->re = a[j1+p14+p02+RE_IM_STRIDE];	r2D->im = a[j1+p14+p02+RE_IM_STRIDE+1];
		r2E->re = a[j1+p14+p03];	r2E->im = a[j1+p14+p03+1];	r2F->re = a[j1+p14+p03+RE_IM_STRIDE];	r2F->im = a[j1+p14+p03+RE_IM_STRIDE+1];
		r30->re = a[j1+p18    ];	r30->im = a[j1+p18    +1];	r31->re = a[j1+p18    +RE_IM_STRIDE];	r31->im = a[j1+p18    +RE_IM_STRIDE+1];
		r32->re = a[j1+p18+p01];	r32->im = a[j1+p18+p01+1];	r33->re = a[j1+p18+p01+RE_IM_STRIDE];	r33->im = a[j1+p18+p01+RE_IM_STRIDE+1];
		r34->re = a[j1+p18+p02];	r34->im = a[j1+p18+p02+1];	r35->re = a[j1+p18+p02+RE_IM_STRIDE];	r35->im = a[j1+p18+p02+RE_IM_STRIDE+1];
		r36->re = a[j1+p18+p03];	r36->im = a[j1+p18+p03+1];	r37->re = a[j1+p18+p03+RE_IM_STRIDE];	r37->im = a[j1+p18+p03+RE_IM_STRIDE+1];
		r38->re = a[j1+p1C    ];	r38->im = a[j1+p1C    +1];	r39->re = a[j1+p1C    +RE_IM_STRIDE];	r39->im = a[j1+p1C    +RE_IM_STRIDE+1];
		r3A->re = a[j1+p1C+p01];	r3A->im = a[j1+p1C+p01+1];	r3B->re = a[j1+p1C+p01+RE_IM_STRIDE];	r3B->im = a[j1+p1C+p01+RE_IM_STRIDE+1];
		r3C->re = a[j1+p1C+p02];	r3C->im = a[j1+p1C+p02+1];	r3D->re = a[j1+p1C+p02+RE_IM_STRIDE];	r3D->im = a[j1+p1C+p02+RE_IM_STRIDE+1];
		r3E->re = a[j1+p1C+p03];	r3E->im = a[j1+p1C+p03+1];	r3F->re = a[j1+p1C+p03+RE_IM_STRIDE];	r3F->im = a[j1+p1C+p03+RE_IM_STRIDE+1];

		add0 = &a[j1    ];
		SSE2_RADIX32_DIF_NOTWIDDLE(add0,p01,p02,p03,p04,p08,p10,p18,r00,isrt2,cc0);

	#else	/* !USE_SSE2 */

	  #if USE_SCALAR_DFT_MACRO
	  /* Macro-ized version of code below: */
		RADIX_32_DIF(\
			a+j1,arr_offsets,\
			a+j1,arr_offsets	/* In-place DFT here, so no need for separate input/output index offsets */\
		)
	  #else
		/*       gather the needed data (32 64-bit complex, i.e. 64 64-bit reals) and do the first set of four length-8 transforms.	*/
		/*...Block 1:	*/
		jt = j1;	jp = j2;

		t00=a[jt    ];	t01=a[jp    ];
		rt =a[jt+p10];	it =a[jp+p10];
		t02=t00-rt;		t03=t01-it;
		t00=t00+rt;		t01=t01+it;

		t04=a[jt+p08];	t05=a[jp+p08];
		rt =a[jt+p18];	it =a[jp+p18];
		t06=t04-rt;		t07=t05-it;
		t04=t04+rt;		t05=t05+it;

		rt =t04;		it =t05;
		t04=t00-rt;		t05=t01-it;
		t00=t00+rt;		t01=t01+it;

		rt =t06;		it =t07;
		t06=t02+it;		t07=t03-rt;
		t02=t02-it;		t03=t03+rt;

		t08=a[jt+p04];	t09=a[jp+p04];
		rt =a[jt+p14];	it =a[jp+p14];
		t0A=t08-rt;		t0B=t09-it;
		t08=t08+rt;		t09=t09+it;

		t0C=a[jt+p0C];	t0D=a[jp+p0C];
		rt =a[jt+p1C];	it =a[jp+p1C];
		t0E=t0C-rt;		t0F=t0D-it;
		t0C=t0C+rt;		t0D=t0D+it;

		rt =t0C;		it =t0D;
		t0C=t08-rt;		t0D=t09-it;
		t08=t08+rt;		t09=t09+it;

		rt =t0E;		it =t0F;
		t0E=t0A+it;		t0F=t0B-rt;
		t0A=t0A-it;		t0B=t0B+rt;

		rt =t08;		it =t09;
		t08=t00-rt;		t09=t01-it;
		t00=t00+rt;		t01=t01+it;

		rt =t0C;		it =t0D;
		t0C=t04+it;		t0D=t05-rt;
		t04=t04-it;		t05=t05+rt;

		rt =(t0A-t0B)*ISRT2;it =(t0A+t0B)*ISRT2;
		t0A=t02-rt;		t0B=t03-it;
		t02=t02+rt;		t03=t03+it;

		rt =(t0E+t0F)*ISRT2;it =(t0F-t0E)*ISRT2;
		t0E=t06+rt;		t0F=t07+it;
		t06=t06-rt;		t07=t07-it;

		/*...Block 2:	*/
		jt =j1 + p02;	jp = j2 + p02;

		t10=a[jt    ];	t11=a[jp    ];
		rt =a[jt+p10];	it =a[jp+p10];
		t12=t10-rt;		t13=t11-it;
		t10=t10+rt;		t11=t11+it;

		t14=a[jt+p08];	t15=a[jp+p08];
		rt =a[jt+p18];	it =a[jp+p18];
		t16=t14-rt;		t17=t15-it;
		t14=t14+rt;		t15=t15+it;

		rt =t14;		it =t15;
		t14=t10-rt;		t15=t11-it;
		t10=t10+rt;		t11=t11+it;

		rt =t16;		it =t17;
		t16=t12+it;		t17=t13-rt;
		t12=t12-it;		t13=t13+rt;

		t18=a[jt+p04];	t19=a[jp+p04];
		rt =a[jt+p14];	it =a[jp+p14];
		t1A=t18-rt;		t1B=t19-it;
		t18=t18+rt;		t19=t19+it;

		t1C=a[jt+p0C];	t1D=a[jp+p0C];
		rt =a[jt+p1C];	it =a[jp+p1C];
		t1E=t1C-rt;		t1F=t1D-it;
		t1C=t1C+rt;		t1D=t1D+it;

		rt =t1C;		it =t1D;
		t1C=t18-rt;		t1D=t19-it;
		t18=t18+rt;		t19=t19+it;

		rt =t1E;		it =t1F;
		t1E=t1A+it;		t1F=t1B-rt;
		t1A=t1A-it;		t1B=t1B+rt;

		rt =t18;		it =t19;
		t18=t10-rt;		t19=t11-it;
		t10=t10+rt;		t11=t11+it;

		rt =t1C;		it =t1D;
		t1C=t14+it;		t1D=t15-rt;
		t14=t14-it;		t15=t15+rt;

		rt =(t1A-t1B)*ISRT2;it =(t1A+t1B)*ISRT2;
		t1A=t12-rt;		t1B=t13-it;
		t12=t12+rt;		t13=t13+it;

		rt =(t1E+t1F)*ISRT2;it =(t1F-t1E)*ISRT2;
		t1E=t16+rt;		t1F=t17+it;
		t16=t16-rt;		t17=t17-it;

		/*...Block 3:	*/
		jt =j1 + p01;	jp = j2 + p01;

		t20=a[jt    ];	t21=a[jp    ];
		rt =a[jt+p10];	it =a[jp+p10];
		t22=t20-rt;		t23=t21-it;
		t20=t20+rt;		t21=t21+it;

		t24=a[jt+p08];	t25=a[jp+p08];
		rt =a[jt+p18];	it =a[jp+p18];
		t26=t24-rt;		t27=t25-it;
		t24=t24+rt;		t25=t25+it;

		rt =t24;		it =t25;
		t24=t20-rt;		t25=t21-it;
		t20=t20+rt;		t21=t21+it;

		rt =t26;		it =t27;
		t26=t22+it;		t27=t23-rt;
		t22=t22-it;		t23=t23+rt;

		t28=a[jt+p04];	t29=a[jp+p04];
		rt =a[jt+p14];	it =a[jp+p14];
		t2A=t28-rt;		t2B=t29-it;
		t28=t28+rt;		t29=t29+it;

		t2C=a[jt+p0C];	t2D=a[jp+p0C];
		rt =a[jt+p1C];	it =a[jp+p1C];
		t2E=t2C-rt;		t2F=t2D-it;
		t2C=t2C+rt;		t2D=t2D+it;

		rt =t2C;		it =t2D;
		t2C=t28-rt;		t2D=t29-it;
		t28=t28+rt;		t29=t29+it;

		rt =t2E;		it =t2F;
		t2E=t2A+it;		t2F=t2B-rt;
		t2A=t2A-it;		t2B=t2B+rt;

		rt =t28;		it =t29;
		t28=t20-rt;		t29=t21-it;
		t20=t20+rt;		t21=t21+it;

		rt =t2C;		it =t2D;
		t2C=t24+it;		t2D=t25-rt;
		t24=t24-it;		t25=t25+rt;

		rt =(t2A-t2B)*ISRT2;it =(t2A+t2B)*ISRT2;
		t2A=t22-rt;		t2B=t23-it;
		t22=t22+rt;		t23=t23+it;

		rt =(t2E+t2F)*ISRT2;it =(t2F-t2E)*ISRT2;
		t2E=t26+rt;		t2F=t27+it;
		t26=t26-rt;		t27=t27-it;

		/*...Block 4:	*/
		jt =j1 + p03;	jp = j2 + p03;

		t30=a[jt    ];	t31=a[jp    ];
		rt =a[jt+p10];	it =a[jp+p10];
		t32=t30-rt;		t33=t31-it;
		t30=t30+rt;		t31=t31+it;

		t34=a[jt+p08];	t35=a[jp+p08];
		rt =a[jt+p18];	it =a[jp+p18];
		t36=t34-rt;		t37=t35-it;
		t34=t34+rt;		t35=t35+it;

		rt =t34;		it =t35;
		t34=t30-rt;		t35=t31-it;
		t30=t30+rt;		t31=t31+it;

		rt =t36;		it =t37;
		t36=t32+it;		t37=t33-rt;
		t32=t32-it;		t33=t33+rt;

		t38=a[jt+p04];	t39=a[jp+p04];
		rt =a[jt+p14];	it =a[jp+p14];
		t3A=t38-rt;		t3B=t39-it;
		t38=t38+rt;		t39=t39+it;

		t3C=a[jt+p0C];	t3D=a[jp+p0C];
		rt =a[jt+p1C];	it =a[jp+p1C];
		t3E=t3C-rt;		t3F=t3D-it;
		t3C=t3C+rt;		t3D=t3D+it;

		rt =t3C;		it =t3D;
		t3C=t38-rt;		t3D=t39-it;
		t38=t38+rt;		t39=t39+it;

		rt =t3E;		it =t3F;
		t3E=t3A+it;		t3F=t3B-rt;
		t3A=t3A-it;		t3B=t3B+rt;

		rt =t38;		it =t39;
		t38=t30-rt;		t39=t31-it;
		t30=t30+rt;		t31=t31+it;

		rt =t3C;		it =t3D;
		t3C=t34+it;		t3D=t35-rt;
		t34=t34-it;		t35=t35+rt;

		rt =(t3A-t3B)*ISRT2;it =(t3A+t3B)*ISRT2;
		t3A=t32-rt;		t3B=t33-it;
		t32=t32+rt;		t33=t33+it;

		rt =(t3E+t3F)*ISRT2;it =(t3F-t3E)*ISRT2;
		t3E=t36+rt;		t3F=t37+it;
		t36=t36-rt;		t37=t37-it;

	/*...and now do eight radix-4 transforms, including the internal twiddle factors:
		1, exp(i* 1*twopi/32) =       ( c32_1, s32_1), exp(i* 2*twopi/32) =       ( c    , s    ), exp(i* 3*twopi/32) =       ( c32_3, s32_3) (for inputs to transform block 2),
		1, exp(i* 2*twopi/32) =       ( c    , s    ), exp(i* 4*twopi/32) = ISRT2*( 1    , 1    ), exp(i* 3*twopi/32) =       ( s    , c    ) (for inputs to transform block 3),
		1, exp(i* 3*twopi/32) =       ( c32_3, s32_3), exp(i* 6*twopi/32) =       ( s    , c    ), exp(i* 9*twopi/32) =       (-s32_1, c32_1) (for inputs to transform block 4),
		1, exp(i* 4*twopi/32) = ISRT2*( 1    , 1    ), exp(i* 8*twopi/32) =       ( 0    , 1    ), exp(i*12*twopi/32) = ISRT2*(-1    , 1    ) (for inputs to transform block 5),
		1, exp(i* 5*twopi/32) =       ( s32_3, c32_3), exp(i*10*twopi/32) =       (-s    , c    ), exp(i*15*twopi/32) =       (-c32_1, s32_1) (for inputs to transform block 6),
		1, exp(i* 6*twopi/32) =       ( s    , c    ), exp(i*12*twopi/32) = ISRT2*(-1    , 1    ), exp(i*18*twopi/32) =       (-c    ,-s    ) (for inputs to transform block 7),
		1, exp(i* 7*twopi/32) =       ( s32_1, c32_1), exp(i*14*twopi/32) =       (-c    , s    ), exp(i*21*twopi/32) =       (-s32_3,-c32_3) (for inputs to transform block 8),
		 and only the last 3 inputs to each of the radix-4 transforms 2 through 8 are multiplied by non-unity twiddles.
	*/

		/*...Block 1: t00,t10,t20,t30	*/
		jt = j1;	jp = j2;

		rt =t10;	t10=t00-rt;	t00=t00+rt;
		it =t11;	t11=t01-it;	t01=t01+it;

		rt =t30;	t30=t20-rt;	t20=t20+rt;
		it =t31;	t31=t21-it;	t21=t21+it;

		a[jt    ]=t00+t20;		a[jp    ]=t01+t21;
		a[jt+p01]=t00-t20;		a[jp+p01]=t01-t21;

		a[jt+p02]=t10-t31;		a[jp+p02]=t11+t30;	/* mpy by E^4=i is inlined here...	*/
		a[jt+p03]=t10+t31;		a[jp+p03]=t11-t30;

		/*...Block 5: t08,t18,t28,t38	*/
		jt =j1 + p04;	jp = j2 + p04;

		rt =t18;	t18=t08+t19;	t08=t08-t19;		/* twiddle mpy by E^4 = I	*/
		t19=t09-rt;	t09=t09+rt;

		rt =(t28-t29)*ISRT2;	t29=(t28+t29)*ISRT2;		t28=rt;	/* twiddle mpy by E^8	*/
		rt =(t39+t38)*ISRT2;	it =(t39-t38)*ISRT2;			/* twiddle mpy by -E^12 is here...	*/
		t38=t28+rt;			t28=t28-rt;				/* ...and get E^12 by flipping signs here.	*/
		t39=t29+it;			t29=t29-it;

		a[jt    ]=t08+t28;		a[jp    ]=t09+t29;
		a[jt+p01]=t08-t28;		a[jp+p01]=t09-t29;

		a[jt+p02]=t18-t39;		a[jp+p02]=t19+t38;	/* mpy by E^4=i is inlined here...	*/
		a[jt+p03]=t18+t39;		a[jp+p03]=t19-t38;

		/*...Block 3: t04,t14,t24,t34	*/
		jt =j1 + p08;	jp = j2 + p08;

		rt =(t14-t15)*ISRT2;	it =(t14+t15)*ISRT2;			/* twiddle mpy by E^4	*/
		t14=t04-rt;			t04=t04+rt;
		t15=t05-it;			t05=t05+it;

		rt =t24*c - t25*s;		t25=t25*c + t24*s;		t24=rt;	/* twiddle mpy by E^2	*/
		rt =t34*s - t35*c;		it =t35*s + t34*c;			/* twiddle mpy by E^6	*/
		t34=t24-rt;			t24=t24+rt;
		t35=t25-it;			t25=t25+it;

		a[jt    ]=t04+t24;		a[jp    ]=t05+t25;
		a[jt+p01]=t04-t24;		a[jp+p01]=t05-t25;

		a[jt+p02]=t14-t35;		a[jp+p02]=t15+t34;	/* mpy by E^4=i is inlined here...	*/
		a[jt+p03]=t14+t35;		a[jp+p03]=t15-t34;

		/*...Block 7: t0C,t1C,t2C,t3C	*/
		jt =j1 + p0C;	jp = j2 + p0C;

		rt =(t1D+t1C)*ISRT2;	it =(t1D-t1C)*ISRT2;			/* twiddle mpy by -E^12 is here...	*/
		t1C=t0C+rt;			t0C=t0C-rt;				/* ...and get E^12 by flipping signs here.	*/
		t1D=t0D+it;			t0D=t0D-it;

		rt =t2C*s - t2D*c;		t2D=t2D*s + t2C*c;		t2C=rt;	/* twiddle mpy by E^6	*/
		rt =t3C*c - t3D*s;		it =t3D*c + t3C*s;			/* twiddle mpy by E^18 is here...	*/
		t3C=t2C+rt;			t2C=t2C-rt;				/* ...and get E^18 by flipping signs here.	*/
		t3D=t2D+it;			t2D=t2D-it;

		a[jt    ]=t0C+t2C;		a[jp    ]=t0D+t2D;
		a[jt+p01]=t0C-t2C;		a[jp+p01]=t0D-t2D;

		a[jt+p02]=t1C-t3D;		a[jp+p02]=t1D+t3C;	/* mpy by E^4=i is inlined here...	*/
		a[jt+p03]=t1C+t3D;		a[jp+p03]=t1D-t3C;

		/*...Block 2: t02,t12,t22,t32	*/
		jt =j1 + p10;	jp = j2 + p10;

		rt =t12*c - t13*s;		it =t13*c + t12*s;			/* twiddle mpy by E^2	*/
		t12=t02-rt;			t02=t02+rt;
		t13=t03-it;			t03=t03+it;

		rt =t22*c32_1 - t23*s32_1;	t23=t23*c32_1 + t22*s32_1;	t22=rt;	/* twiddle mpy by E^1	*/
		rt =t32*c32_3 - t33*s32_3;	it =t33*c32_3 + t32*s32_3;		/* twiddle mpy by E^3	*/
		t32=t22-rt;			t22=t22+rt;
		t33=t23-it;			t23=t23+it;

		a[jt    ]=t02+t22;		a[jp    ]=t03+t23;
		a[jt+p01]=t02-t22;		a[jp+p01]=t03-t23;

		a[jt+p02]=t12-t33;		a[jp+p02]=t13+t32;	/* mpy by E^4=i is inlined here...;	*/
		a[jt+p03]=t12+t33;		a[jp+p03]=t13-t32;

		/*...Block 6: t0A,t1A,t2A,t3A	*/
		jt =j1 + p14;	jp = j2 + p14;

		rt =t1A*s + t1B*c;		it =t1B*s - t1A*c;			/* twiddle mpy by -E^10 is here...	*/
		t1A=t0A+rt;			t0A =t0A-rt;				/* ...and get E^10 by flipping signs here.	*/
		t1B=t0B+it;			t0B =t0B-it;

		rt =t2A*s32_3 - t2B*c32_3;	t2B=t2B*s32_3 + t2A*c32_3;	t2A=rt;	/* twiddle mpy by E^5	*/
		rt =t3A*c32_1 + t3B*s32_1;	it =t3B*c32_1 - t3A*s32_1;	 	/* twiddle mpy by -E^15 is here...	*/
		t3A=t2A+rt;			t2A=t2A-rt;				/* ...and get E^15 by flipping signs here.	*/
		t3B=t2B+it;			t2B=t2B-it;

		a[jt    ]=t0A+t2A;		a[jp    ]=t0B+t2B;
		a[jt+p01]=t0A-t2A;		a[jp+p01]=t0B-t2B;

		a[jt+p02]=t1A-t3B;		a[jp+p02]=t1B+t3A;	/* mpy by E^4=i is inlined here...	*/
		a[jt+p03]=t1A+t3B;		a[jp+p03]=t1B-t3A;

		/*...Block 4: t06,t16,t26,t36	*/
		jt =j1 + p18;	jp = j2 + p18;

		rt =t16*s - t17*c;		it =t17*s + t16*c;			/* twiddle mpy by E^6	*/
		t16=t06-rt;			t06 =t06+rt;
		t17=t07-it;			t07 =t07+it;

		rt =t26*c32_3 - t27*s32_3;	t27=t27*c32_3 + t26*s32_3;	t26=rt;	/* twiddle mpy by E^3	*/
		rt =t36*s32_1 + t37*c32_1;	it =t37*s32_1 - t36*c32_1;		/* twiddle mpy by -E^9 is here...	*/
		t36=t26+rt;			t26=t26-rt;				/* ...and get E^9 by flipping signs here.	*/
		t37=t27+it;			t27=t27-it;

		a[jt    ]=t06+t26;		a[jp    ]=t07+t27;
		a[jt+p01]=t06-t26;		a[jp+p01]=t07-t27;

		a[jt+p02]=t16-t37;		a[jp+p02]=t17+t36;	/* mpy by E^4=i is inlined here...	*/
		a[jt+p03]=t16+t37;		a[jp+p03]=t17-t36;

		/*...Block 8: t0E,t1E,t2E,t3E	*/
		jt =j1 + p1C;	jp = j2 + p1C;

		rt =t1E*c + t1F*s;		it =t1F*c - t1E*s;			/* twiddle mpy by -E^14 is here...	*/
		t1E=t0E+rt;			t0E =t0E-rt;				/* ...and get E^14 by flipping signs here.	*/
		t1F=t0F+it;			t0F =t0F-it;

		rt =t2E*s32_1 - t2F*c32_1;	t2F=t2F*s32_1 + t2E*c32_1;	t2E=rt;	/* twiddle mpy by E^7	*/
		rt =t3E*s32_3 - t3F*c32_3;	it =t3F*s32_3 + t3E*c32_3;		/* twiddle mpy by -E^21 is here...	*/
		t3E=t2E+rt;			t2E=t2E-rt;				/* ...and get E^21 by flipping signs here.	*/
		t3F=t2F+it;			t2F=t2F-it;

		a[jt    ]=t0E+t2E;		a[jp    ]=t0F+t2F;
		a[jt+p01]=t0E-t2E;		a[jp+p01]=t0F-t2F;

		a[jt+p02]=t1E-t3F;		a[jp+p02]=t1F+t3E;	/* mpy by E^4=i is inlined here...	*/
		a[jt+p03]=t1E+t3F;		a[jp+p03]=t1F-t3E;
	  #endif

	#endif	/* USE_SSE2 */
	}
}

/**************/

void radix32_dit_pass1(double a[], int n)
{
/*
!...Acronym: DIT = Decimation In Time
!
!...Subroutine to perform an initial radix-32 complex inverse DIT FFT pass on the data in the length-N real vector A.
!
!   See the documentation in radix16_dif_pass for further details.
*/
	int j,j1,j2,arr_offsets[32];
	static int n32,p01,p02,p03,p04,p05,p06,p07,p08,p10,p18, first_entry=TRUE;
	static double    c     = 0.92387953251128675613, s     = 0.38268343236508977173	/* exp(  i*twopi/16)	*/
			,c32_1 = 0.98078528040323044912, s32_1 = 0.19509032201612826784	/* exp(  i*twopi/32)	*/
			,c32_3 = 0.83146961230254523708, s32_3 = 0.55557023301960222473;/* exp(3*i*twopi/32)	*/
#ifdef USE_SSE2
	double *add0;	/* Addresses into array sections */
	static struct complex *sc_arr = 0x0, *sc_ptr;
	static struct complex *isrt2, *cc0, *ss0, *cc1, *ss1, *cc3, *ss3
		,*r00,*r01,*r02,*r03,*r04,*r05,*r06,*r07,*r08,*r09,*r0A,*r0B,*r0C,*r0D,*r0E,*r0F
		,*r10,*r11,*r12,*r13,*r14,*r15,*r16,*r17,*r18,*r19,*r1A,*r1B,*r1C,*r1D,*r1E,*r1F
		,*r20,*r21,*r22,*r23,*r24,*r25,*r26,*r27,*r28,*r29,*r2A,*r2B,*r2C,*r2D,*r2E,*r2F
		,*r30,*r31,*r32,*r33,*r34,*r35,*r36,*r37,*r38,*r39,*r3A,*r3B,*r3C,*r3D,*r3E,*r3F;
#elif !USE_SCALAR_DFT_MACRO
	int jt,jp;
	double rt,it
		,t00,t01,t02,t03,t04,t05,t06,t07,t08,t09,t0A,t0B,t0C,t0D,t0E,t0F
		,t10,t11,t12,t13,t14,t15,t16,t17,t18,t19,t1A,t1B,t1C,t1D,t1E,t1F
		,t20,t21,t22,t23,t24,t25,t26,t27,t28,t29,t2A,t2B,t2C,t2D,t2E,t2F
		,t30,t31,t32,t33,t34,t35,t36,t37,t38,t39,t3A,t3B,t3C,t3D,t3E,t3F;
#endif

	if(!first_entry && (n >> 5) != n32)	/* New runlength?	*/
	{
		first_entry=TRUE;
	}

/*...initialize things upon first entry	*/

	if(first_entry)
	{
		first_entry=FALSE;
		n32=n/32;

		p01 = n32;
		p02 = p01 +p01;
		p03 = p02 +p01;
		p04 = p03 +p01;
		p05 = p04 +p01;
		p06 = p05 +p01;
		p07 = p06 +p01;
		p08 = p07 +p01;
		p10 = p08 +p08;
		p18 = p10 +p08;

		p01 = p01 + ( (p01 >> DAT_BITS) << PAD_BITS );
		p02 = p02 + ( (p02 >> DAT_BITS) << PAD_BITS );
		p03 = p03 + ( (p03 >> DAT_BITS) << PAD_BITS );
		p04 = p04 + ( (p04 >> DAT_BITS) << PAD_BITS );
		p05 = p05 + ( (p05 >> DAT_BITS) << PAD_BITS );
		p06 = p06 + ( (p06 >> DAT_BITS) << PAD_BITS );
		p07 = p07 + ( (p07 >> DAT_BITS) << PAD_BITS );
		p08 = p08 + ( (p08 >> DAT_BITS) << PAD_BITS );
		p10 = p10 + ( (p10 >> DAT_BITS) << PAD_BITS );
		p18 = p18 + ( (p18 >> DAT_BITS) << PAD_BITS );

		arr_offsets[0x00] = 0;
		arr_offsets[0x01] = p01;
		arr_offsets[0x02] = p02;
		arr_offsets[0x03] = p03;
		arr_offsets[0x04] = p04;
		arr_offsets[0x05] = p05;
		arr_offsets[0x06] = p06;
		arr_offsets[0x07] = p07;
		arr_offsets[0x08] = p08;
		arr_offsets[0x09] = p08+p01;
		arr_offsets[0x0A] = p08+p02;
		arr_offsets[0x0B] = p08+p03;
		arr_offsets[0x0C] = p08+p04;
		arr_offsets[0x0D] = p08+p05;
		arr_offsets[0x0E] = p08+p06;
		arr_offsets[0x0F] = p08+p07;
		arr_offsets[0x10] = p10;
		arr_offsets[0x11] = p10+p01;
		arr_offsets[0x12] = p10+p02;
		arr_offsets[0x13] = p10+p03;
		arr_offsets[0x14] = p10+p04;
		arr_offsets[0x15] = p10+p05;
		arr_offsets[0x16] = p10+p06;
		arr_offsets[0x17] = p10+p07;
		arr_offsets[0x18] = p18;
		arr_offsets[0x19] = p18+p01;
		arr_offsets[0x1A] = p18+p02;
		arr_offsets[0x1B] = p18+p03;
		arr_offsets[0x1C] = p18+p04;
		arr_offsets[0x1D] = p18+p05;
		arr_offsets[0x1E] = p18+p06;
		arr_offsets[0x1F] = p18+p07;

	#ifdef USE_SSE2
		sc_arr = ALLOC_COMPLEX(sc_arr, radix32_creals_in_local_store);	if(!sc_arr){ sprintf(cbuf, "FATAL: unable to allocate sc_arr!.\n"); fprintf(stderr,"%s", cbuf);	ASSERT(HERE, 0,cbuf); }
		sc_ptr = ALIGN_COMPLEX(sc_arr);
		ASSERT(HERE, ((uint32)sc_ptr & 0x3f) == 0, "sc_ptr not 64-byte aligned!");

	/* Use low 64 16-byte slots of sc_arr for temporaries, next 7 for the nontrivial complex 16th roots,
	next 32 for the doubled carry pairs, next 2 for ROE and RND_CONST, next 20 for the half_arr table lookup stuff,
	plus at least 3 more slots to allow for 64-byte alignment of the array:
	*/
		r00		= sc_ptr + 0x00;	r01		= sc_ptr + 0x01;
		r02		= sc_ptr + 0x02;	r03		= sc_ptr + 0x03;
		r04		= sc_ptr + 0x04;	r05		= sc_ptr + 0x05;
		r06		= sc_ptr + 0x06;	r07		= sc_ptr + 0x07;
		r08		= sc_ptr + 0x08;	r09		= sc_ptr + 0x09;
		r0A		= sc_ptr + 0x0a;	r0B		= sc_ptr + 0x0b;
		r0C		= sc_ptr + 0x0c;	r0D		= sc_ptr + 0x0d;
		r0E		= sc_ptr + 0x0e;	r0F		= sc_ptr + 0x0f;
		r10		= sc_ptr + 0x10;	r11		= sc_ptr + 0x11;
		r12		= sc_ptr + 0x12;	r13		= sc_ptr + 0x13;
		r14		= sc_ptr + 0x14;	r15		= sc_ptr + 0x15;
		r16		= sc_ptr + 0x16;	r17		= sc_ptr + 0x17;
		r18		= sc_ptr + 0x18;	r19		= sc_ptr + 0x19;
		r1A		= sc_ptr + 0x1a;	r1B		= sc_ptr + 0x1b;
		r1C		= sc_ptr + 0x1c;	r1D		= sc_ptr + 0x1d;
		r1E		= sc_ptr + 0x1e;	r1F		= sc_ptr + 0x1f;
		r20		= sc_ptr + 0x20;	r21		= sc_ptr + 0x21;
		r22		= sc_ptr + 0x22;	r23		= sc_ptr + 0x23;
		r24		= sc_ptr + 0x24;	r25		= sc_ptr + 0x25;
		r26		= sc_ptr + 0x26;	r27		= sc_ptr + 0x27;
		r28		= sc_ptr + 0x28;	r29		= sc_ptr + 0x29;
		r2A		= sc_ptr + 0x2a;	r2B		= sc_ptr + 0x2b;
		r2C		= sc_ptr + 0x2c;	r2D		= sc_ptr + 0x2d;
		r2E		= sc_ptr + 0x2e;	r2F		= sc_ptr + 0x2f;
		r30		= sc_ptr + 0x30;	r31		= sc_ptr + 0x31;
		r32		= sc_ptr + 0x32;	r33		= sc_ptr + 0x33;
		r34		= sc_ptr + 0x34;	r35		= sc_ptr + 0x35;
		r36		= sc_ptr + 0x36;	r37		= sc_ptr + 0x37;
		r38		= sc_ptr + 0x38;	r39		= sc_ptr + 0x39;
		r3A		= sc_ptr + 0x3a;	r3B		= sc_ptr + 0x3b;
		r3C		= sc_ptr + 0x3c;	r3D		= sc_ptr + 0x3d;
		r3E		= sc_ptr + 0x3e;	r3F		= sc_ptr + 0x3f;
		isrt2	= sc_ptr + 0x40;
		cc0		= sc_ptr + 0x41;	ss0		= sc_ptr + 0x42;
		cc1		= sc_ptr + 0x43;	ss1		= sc_ptr + 0x44;
		cc3		= sc_ptr + 0x45;	ss3		= sc_ptr + 0x46;

		/* These remain fixed: */
		isrt2->re = ISRT2;	isrt2->im = ISRT2;
		cc0  ->re = c	;	cc0  ->im = c	;		ss0  ->re = s	;	ss0  ->im = s	;
		cc1  ->re = c32_1;	cc1  ->im = c32_1;		ss1  ->re = s32_1;	ss1  ->im = s32_1;
		cc3  ->re = c32_3;	cc3  ->im = c32_3;		ss3  ->re = s32_3;	ss3  ->im = s32_3;

	#endif
	}

/*...The radix-32 pass is here.	*/

#ifdef USE_SSE2
	for(j = 0; j < n32; j += 4)
	{
	/* In SSE2 mode, data are arranged in [re0,re1,im0,im1] quartets, not the usual [re0,im0],[re1,im1] pairs.
	Thus we can still increment the j-index as if stepping through the residue array-of-doubles in strides of 2,
	but to point to the proper real datum, we need to bit-reverse bits <0:1> of j, i.e. [0,1,2,3] ==> [0,2,1,3].
	*/
		j1 = (j & mask01) + br4[j&3];
#else
	for(j = 0; j < n32; j += 2)	/* Each inner loop execution processes (radix(1)*nwt) array data.	*/
	{
		j1 =  j;
#endif
		j1 = j1 + ( (j1 >> DAT_BITS) << PAD_BITS );	/* padded-array fetch index is here */
		j2 = j1+RE_IM_STRIDE;

	#ifdef USE_SSE2

		add0 = &a[j1    ];
		SSE2_RADIX32_DIT_NOTWIDDLE(add0,p01,p02,p03,p04,p08,p10,p18,r00,isrt2,cc0);

		/* Now copy outputs back into main array: */
		a[j1        ] = r00->re;	a[j1        +1] = r00->im;	a[j1        +RE_IM_STRIDE] = r01->re;	a[j1        +RE_IM_STRIDE+1] = r01->im;
		a[j1    +p01] = r02->re;	a[j1    +p01+1] = r02->im;	a[j1    +p01+RE_IM_STRIDE] = r03->re;	a[j1    +p01+RE_IM_STRIDE+1] = r03->im;
		a[j1    +p02] = r04->re;	a[j1    +p02+1] = r04->im;	a[j1    +p02+RE_IM_STRIDE] = r05->re;	a[j1    +p02+RE_IM_STRIDE+1] = r05->im;
		a[j1    +p03] = r06->re;	a[j1    +p03+1] = r06->im;	a[j1    +p03+RE_IM_STRIDE] = r07->re;	a[j1    +p03+RE_IM_STRIDE+1] = r07->im;
		a[j1    +p04] = r08->re;	a[j1    +p04+1] = r08->im;	a[j1    +p04+RE_IM_STRIDE] = r09->re;	a[j1    +p04+RE_IM_STRIDE+1] = r09->im;
		a[j1    +p05] = r0A->re;	a[j1    +p05+1] = r0A->im;	a[j1    +p05+RE_IM_STRIDE] = r0B->re;	a[j1    +p05+RE_IM_STRIDE+1] = r0B->im;
		a[j1    +p06] = r0C->re;	a[j1    +p06+1] = r0C->im;	a[j1    +p06+RE_IM_STRIDE] = r0D->re;	a[j1    +p06+RE_IM_STRIDE+1] = r0D->im;
		a[j1    +p07] = r0E->re;	a[j1    +p07+1] = r0E->im;	a[j1    +p07+RE_IM_STRIDE] = r0F->re;	a[j1    +p07+RE_IM_STRIDE+1] = r0F->im;
		a[j1+p08    ] = r10->re;	a[j1+p08    +1] = r10->im;	a[j1+p08    +RE_IM_STRIDE] = r11->re;	a[j1+p08    +RE_IM_STRIDE+1] = r11->im;
		a[j1+p08+p01] = r12->re;	a[j1+p08+p01+1] = r12->im;	a[j1+p08+p01+RE_IM_STRIDE] = r13->re;	a[j1+p08+p01+RE_IM_STRIDE+1] = r13->im;
		a[j1+p08+p02] = r14->re;	a[j1+p08+p02+1] = r14->im;	a[j1+p08+p02+RE_IM_STRIDE] = r15->re;	a[j1+p08+p02+RE_IM_STRIDE+1] = r15->im;
		a[j1+p08+p03] = r16->re;	a[j1+p08+p03+1] = r16->im;	a[j1+p08+p03+RE_IM_STRIDE] = r17->re;	a[j1+p08+p03+RE_IM_STRIDE+1] = r17->im;
		a[j1+p08+p04] = r18->re;	a[j1+p08+p04+1] = r18->im;	a[j1+p08+p04+RE_IM_STRIDE] = r19->re;	a[j1+p08+p04+RE_IM_STRIDE+1] = r19->im;
		a[j1+p08+p05] = r1A->re;	a[j1+p08+p05+1] = r1A->im;	a[j1+p08+p05+RE_IM_STRIDE] = r1B->re;	a[j1+p08+p05+RE_IM_STRIDE+1] = r1B->im;
		a[j1+p08+p06] = r1C->re;	a[j1+p08+p06+1] = r1C->im;	a[j1+p08+p06+RE_IM_STRIDE] = r1D->re;	a[j1+p08+p06+RE_IM_STRIDE+1] = r1D->im;
		a[j1+p08+p07] = r1E->re;	a[j1+p08+p07+1] = r1E->im;	a[j1+p08+p07+RE_IM_STRIDE] = r1F->re;	a[j1+p08+p07+RE_IM_STRIDE+1] = r1F->im;
		a[j1+p10    ] = r20->re;	a[j1+p10    +1] = r20->im;	a[j1+p10    +RE_IM_STRIDE] = r21->re;	a[j1+p10    +RE_IM_STRIDE+1] = r21->im;
		a[j1+p10+p01] = r22->re;	a[j1+p10+p01+1] = r22->im;	a[j1+p10+p01+RE_IM_STRIDE] = r23->re;	a[j1+p10+p01+RE_IM_STRIDE+1] = r23->im;
		a[j1+p10+p02] = r24->re;	a[j1+p10+p02+1] = r24->im;	a[j1+p10+p02+RE_IM_STRIDE] = r25->re;	a[j1+p10+p02+RE_IM_STRIDE+1] = r25->im;
		a[j1+p10+p03] = r26->re;	a[j1+p10+p03+1] = r26->im;	a[j1+p10+p03+RE_IM_STRIDE] = r27->re;	a[j1+p10+p03+RE_IM_STRIDE+1] = r27->im;
		a[j1+p10+p04] = r28->re;	a[j1+p10+p04+1] = r28->im;	a[j1+p10+p04+RE_IM_STRIDE] = r29->re;	a[j1+p10+p04+RE_IM_STRIDE+1] = r29->im;
		a[j1+p10+p05] = r2A->re;	a[j1+p10+p05+1] = r2A->im;	a[j1+p10+p05+RE_IM_STRIDE] = r2B->re;	a[j1+p10+p05+RE_IM_STRIDE+1] = r2B->im;
		a[j1+p10+p06] = r2C->re;	a[j1+p10+p06+1] = r2C->im;	a[j1+p10+p06+RE_IM_STRIDE] = r2D->re;	a[j1+p10+p06+RE_IM_STRIDE+1] = r2D->im;
		a[j1+p10+p07] = r2E->re;	a[j1+p10+p07+1] = r2E->im;	a[j1+p10+p07+RE_IM_STRIDE] = r2F->re;	a[j1+p10+p07+RE_IM_STRIDE+1] = r2F->im;
		a[j1+p18    ] = r30->re;	a[j1+p18    +1] = r30->im;	a[j1+p18    +RE_IM_STRIDE] = r31->re;	a[j1+p18    +RE_IM_STRIDE+1] = r31->im;
		a[j1+p18+p01] = r32->re;	a[j1+p18+p01+1] = r32->im;	a[j1+p18+p01+RE_IM_STRIDE] = r33->re;	a[j1+p18+p01+RE_IM_STRIDE+1] = r33->im;
		a[j1+p18+p02] = r34->re;	a[j1+p18+p02+1] = r34->im;	a[j1+p18+p02+RE_IM_STRIDE] = r35->re;	a[j1+p18+p02+RE_IM_STRIDE+1] = r35->im;
		a[j1+p18+p03] = r36->re;	a[j1+p18+p03+1] = r36->im;	a[j1+p18+p03+RE_IM_STRIDE] = r37->re;	a[j1+p18+p03+RE_IM_STRIDE+1] = r37->im;
		a[j1+p18+p04] = r38->re;	a[j1+p18+p04+1] = r38->im;	a[j1+p18+p04+RE_IM_STRIDE] = r39->re;	a[j1+p18+p04+RE_IM_STRIDE+1] = r39->im;
		a[j1+p18+p05] = r3A->re;	a[j1+p18+p05+1] = r3A->im;	a[j1+p18+p05+RE_IM_STRIDE] = r3B->re;	a[j1+p18+p05+RE_IM_STRIDE+1] = r3B->im;
		a[j1+p18+p06] = r3C->re;	a[j1+p18+p06+1] = r3C->im;	a[j1+p18+p06+RE_IM_STRIDE] = r3D->re;	a[j1+p18+p06+RE_IM_STRIDE+1] = r3D->im;
		a[j1+p18+p07] = r3E->re;	a[j1+p18+p07+1] = r3E->im;	a[j1+p18+p07+RE_IM_STRIDE] = r3F->re;	a[j1+p18+p07+RE_IM_STRIDE+1] = r3F->im;

	#else	/* !USE_SSE2 */

	  #if USE_SCALAR_DFT_MACRO
		/* Macro-ized version of code below: */
		RADIX_32_DIT(\
			a+j1,arr_offsets,\
			a+j1,arr_offsets	/* In-place DFT here, so no need for separate input/output index offsets */\
		)
	  #else
		/* gather the needed data (32 64-bit complex, i.e. 64 64-bit reals) and do the first set of four length-8 transforms.	*/
		/*...Block 1:	*/
		jt = j1;	jp = j2;

		t00=a[jt    ];	t01=a[jp    ];
		rt =a[jt+p01];	it =a[jp+p01];
		t02=t00-rt;		t03=t01-it;
		t00=t00+rt;		t01=t01+it;

		t04=a[jt+p02];	t05=a[jp+p02];
		rt =a[jt+p03];	it =a[jp+p03];
		t06=t04-rt;		t07=t05-it;
		t04=t04+rt;		t05=t05+it;

		rt =t04;		it =t05;
		t04=t00-rt;		t05=t01-it;
		t00=t00+rt;		t01=t01+it;

		rt =t06;		it =t07;
		t06=t02-it;		t07=t03+rt;
		t02=t02+it;		t03=t03-rt;

		t08=a[jt+p04];	t09=a[jp+p04];
		rt =a[jt+p05];	it =a[jp+p05];
		t0A=t08-rt;		t0B=t09-it;
		t08=t08+rt;		t09=t09+it;

		t0C=a[jt+p06];	t0D=a[jp+p06];
		rt =a[jt+p07];	it =a[jp+p07];
		t0E=t0C-rt;		t0F=t0D-it;
		t0C=t0C+rt;		t0D=t0D+it;

		rt =t0C;		it =t0D;
		t0C=t08-rt;		t0D=t09-it;
		t08=t08+rt;		t09=t09+it;

		rt =t0E;		it =t0F;
		t0E=t0A-it;		t0F=t0B+rt;
		t0A=t0A+it;		t0B=t0B-rt;

		rt =t08;		it =t09;
		t08=t00-rt;		t09=t01-it;
		t00=t00+rt;		t01=t01+it;

		rt =t0C;		it =t0D;
		t0C=t04-it;		t0D=t05+rt;
		t04=t04+it;		t05=t05-rt;

		rt =(t0A+t0B)*ISRT2;it =(t0A-t0B)*ISRT2;
		t0A=t02-rt;		t0B=t03+it;
		t02=t02+rt;		t03=t03-it;

		rt =(t0E-t0F)*ISRT2;it =(t0F+t0E)*ISRT2;
		t0E=t06+rt;		t0F=t07+it;
		t06=t06-rt;		t07=t07-it;

		/*...Block 2:;	*/
		jt =j1 + p08;	jp = j2 + p08;

		t10=a[jt    ];	t11=a[jp    ];
		rt =a[jt+p01];	it =a[jp+p01];
		t12=t10-rt;		t13=t11-it;
		t10=t10+rt;		t11=t11+it;

		t14=a[jt+p02];	t15=a[jp+p02];
		rt =a[jt+p03];	it =a[jp+p03];
		t16=t14-rt;		t17=t15-it;
		t14=t14+rt;		t15=t15+it;

		rt =t14;		it =t15;
		t14=t10-rt;		t15=t11-it;
		t10=t10+rt;		t11=t11+it;

		rt =t16;		it =t17;
		t16=t12-it;		t17=t13+rt;
		t12=t12+it;		t13=t13-rt;

		t18=a[jt+p04];	t19=a[jp+p04];
		rt =a[jt+p05];	it =a[jp+p05];
		t1A=t18-rt;		t1B=t19-it;
		t18=t18+rt;		t19=t19+it;

		t1C=a[jt+p06];	t1D=a[jp+p06];
		rt =a[jt+p07];	it =a[jp+p07];
		t1E=t1C-rt;		t1F=t1D-it;
		t1C=t1C+rt;		t1D=t1D+it;

		rt =t1C;		it =t1D;
		t1C=t18-rt;		t1D=t19-it;
		t18=t18+rt;		t19=t19+it;

		rt =t1E;		it =t1F;
		t1E=t1A-it;		t1F=t1B+rt;
		t1A=t1A+it;		t1B=t1B-rt;

		rt =t18;		it =t19;
		t18=t10-rt;		t19=t11-it;
		t10=t10+rt;		t11=t11+it;

		rt =t1C;		it =t1D;
		t1C=t14-it;		t1D=t15+rt;
		t14=t14+it;		t15=t15-rt;

		rt =(t1A+t1B)*ISRT2;it =(t1A-t1B)*ISRT2;
		t1A=t12-rt;		t1B=t13+it;
		t12=t12+rt;		t13=t13-it;

		rt =(t1E-t1F)*ISRT2;it =(t1F+t1E)*ISRT2;
		t1E=t16+rt;		t1F=t17+it;
		t16=t16-rt;		t17=t17-it;

		/*...Block 3:	*/
		jt =j1 + p10;	jp = j2 + p10;

		t20=a[jt    ];	t21=a[jp    ];
		rt =a[jt+p01];	it =a[jp+p01];
		t22=t20-rt;		t23=t21-it;
		t20=t20+rt;		t21=t21+it;

		t24=a[jt+p02];	t25=a[jp+p02];
		rt =a[jt+p03];	it =a[jp+p03];
		t26=t24-rt;		t27=t25-it;
		t24=t24+rt;		t25=t25+it;

		rt =t24;		it =t25;
		t24=t20-rt;		t25=t21-it;
		t20=t20+rt;		t21=t21+it;

		rt =t26;		it =t27;
		t26=t22-it;		t27=t23+rt;
		t22=t22+it;		t23=t23-rt;

		t28=a[jt+p04];	t29=a[jp+p04];
		rt =a[jt+p05];	it =a[jp+p05];
		t2A=t28-rt;		t2B=t29-it;
		t28=t28+rt;		t29=t29+it;

		t2C=a[jt+p06];	t2D=a[jp+p06];
		rt =a[jt+p07];	it =a[jp+p07];
		t2E=t2C-rt;		t2F=t2D-it;
		t2C=t2C+rt;		t2D=t2D+it;

		rt =t2C;		it =t2D;
		t2C=t28-rt;		t2D=t29-it;
		t28=t28+rt;		t29=t29+it;

		rt =t2E;		it =t2F;
		t2E=t2A-it;		t2F=t2B+rt;
		t2A=t2A+it;		t2B=t2B-rt;

		rt =t28;		it =t29;
		t28=t20-rt;		t29=t21-it;
		t20=t20+rt;		t21=t21+it;

		rt =t2C;		it =t2D;
		t2C=t24-it;		t2D=t25+rt;
		t24=t24+it;		t25=t25-rt;

		rt =(t2A+t2B)*ISRT2;it =(t2A-t2B)*ISRT2;
		t2A=t22-rt;		t2B=t23+it;
		t22=t22+rt;		t23=t23-it;

		rt =(t2E-t2F)*ISRT2;it =(t2F+t2E)*ISRT2;
		t2E=t26+rt;		t2F=t27+it;
		t26=t26-rt;		t27=t27-it;

		/*...Block 4:	*/
		jt =j1 + p18;	jp = j2 + p18;

		t30=a[jt    ];	t31=a[jp    ];
		rt =a[jt+p01];	it =a[jp+p01];
		t32=t30-rt;		t33=t31-it;
		t30=t30+rt;		t31=t31+it;

		t34=a[jt+p02];	t35=a[jp+p02];
		rt =a[jt+p03];	it =a[jp+p03];
		t36=t34-rt;		t37=t35-it;
		t34=t34+rt;		t35=t35+it;

		rt =t34;		it =t35;
		t34=t30-rt;		t35=t31-it;
		t30=t30+rt;		t31=t31+it;

		rt =t36;		it =t37;
		t36=t32-it;		t37=t33+rt;
		t32=t32+it;		t33=t33-rt;

		t38=a[jt+p04];	t39=a[jp+p04];
		rt =a[jt+p05];	it =a[jp+p05];
		t3A=t38-rt;		t3B=t39-it;
		t38=t38+rt;		t39=t39+it;

		t3C=a[jt+p06];	t3D=a[jp+p06];
		rt =a[jt+p07];	it =a[jp+p07];
		t3E=t3C-rt;		t3F=t3D-it;
		t3C=t3C+rt;		t3D=t3D+it;

		rt =t3C;		it =t3D;
		t3C=t38-rt;		t3D=t39-it;
		t38=t38+rt;		t39=t39+it;

		rt =t3E;		it =t3F;
		t3E=t3A-it;		t3F=t3B+rt;
		t3A=t3A+it;		t3B=t3B-rt;

		rt =t38;		it =t39;
		t38=t30-rt;		t39=t31-it;
		t30=t30+rt;		t31=t31+it;

		rt =t3C;		it =t3D;
		t3C=t34-it;		t3D=t35+rt;
		t34=t34+it;		t35=t35-rt;

		rt =(t3A+t3B)*ISRT2;it =(t3A-t3B)*ISRT2;
		t3A=t32-rt;		t3B=t33+it;
		t32=t32+rt;		t33=t33-it;

		rt =(t3E-t3F)*ISRT2;it =(t3F+t3E)*ISRT2;
		t3E=t36+rt;		t3F=t37+it;
		t36=t36-rt;		t37=t37-it;

		/*...and now do eight radix-4 transforms, including the internal twiddle factors:
			1, exp(-i* 1*twopi/32) =       ( c32_1,-s32_1), exp(-i* 2*twopi/32) =       ( c    ,-s    ), exp(-i* 3*twopi/32) =       ( c32_3,-s32_3) (for inputs to transform block 2),
			1, exp(-i* 2*twopi/32) =       ( c    ,-s    ), exp(-i* 4*twopi/32) = ISRT2*( 1    ,-1    ), exp(-i* 3*twopi/32) =       ( s    ,-c    ) (for inputs to transform block 3),
			1, exp(-i* 3*twopi/32) =       ( c32_3,-s32_3), exp(-i* 6*twopi/32) =       ( s    ,-c    ), exp(-i* 9*twopi/32) =       (-s32_1,-c32_1) (for inputs to transform block 4),
			1, exp(-i* 4*twopi/32) = ISRT2*( 1    ,-1    ), exp(-i* 8*twopi/32) =       ( 0    ,-1    ), exp(-i*12*twopi/32) = ISRT2*(-1    ,-1    ) (for inputs to transform block 5),
			1, exp(-i* 5*twopi/32) =       ( s32_3,-c32_3), exp(-i*10*twopi/32) =       (-s    ,-c    ), exp(-i*15*twopi/32) =       (-c32_1,-s32_1) (for inputs to transform block 6),
			1, exp(-i* 6*twopi/32) =       ( s    ,-c    ), exp(-i*12*twopi/32) = ISRT2*(-1    ,-1    ), exp(-i*18*twopi/32) =       (-c    , s    ) (for inputs to transform block 7),
			1, exp(-i* 7*twopi/32) =       ( s32_1,-c32_1), exp(-i*14*twopi/32) =       (-c    ,-s    ), exp(-i*21*twopi/32) =       (-s32_3, c32_3) (for inputs to transform block 8),
			 and only the last 3 inputs to each of the radix-4 transforms 2 through 8 are multiplied by non-unity twiddles.
		*/

		/*...Block 1: t00,t10,t20,t30	*/
		jt = j1;	jp = j2;

		rt =t10;	t10=t00-rt;	t00=t00+rt;
		it =t11;	t11=t01-it;	t01=t01+it;

		rt =t30;	t30=t20-rt;	t20=t20+rt;
		it =t31;	t31=t21-it;	t21=t21+it;

		a[jt    ]=t00+t20;		a[jp    ]=t01+t21;
		a[jt+p10]=t00-t20;		a[jp+p10]=t01-t21;

		a[jt+p08]=t10+t31;		a[jp+p08]=t11-t30;	/* mpy by E^-4 = -I is inlined here...	*/
		a[jt+p18]=t10-t31;		a[jp+p18]=t11+t30;

		/*...Block 5: t08,t18,t28,t38	*/
		jt =j1 + p04;	jp = j2 + p04;

		rt =t18;	t18=t08-t19;	t08=t08+t19;		/* twiddle mpy by E^8 =-I	*/
		t19=t09+rt;	t09=t09-rt;

		rt =(t29+t28)*ISRT2;	t29=(t29-t28)*ISRT2;		t28=rt;	/* twiddle mpy by E^-4	*/
		rt =(t38-t39)*ISRT2;	it =(t38+t39)*ISRT2;			/* twiddle mpy by E^4 = -E^-12 is here...	*/
		t38=t28+rt;			t28=t28-rt;				/* ...and get E^-12 by flipping signs here.	*/
		t39=t29+it;			t29=t29-it;

		a[jt    ]=t08+t28;		a[jp    ]=t09+t29;
		a[jt+p10]=t08-t28;		a[jp+p10]=t09-t29;

		a[jt+p08]=t18+t39;		a[jp+p08]=t19-t38;	/* mpy by E^-4 = -I is inlined here...	*/
		a[jt+p18]=t18-t39;		a[jp+p18]=t19+t38;

		/*...Block 3: t04,t14,t24,t34	*/
		jt =j1 + p02;	jp = j2 + p02;

		rt =(t15+t14)*ISRT2;	it =(t15-t14)*ISRT2;			/* twiddle mpy by E^-4	*/
		t14=t04-rt;			t04=t04+rt;
		t15=t05-it;			t05=t05+it;

		rt =t24*c + t25*s;		t25=t25*c - t24*s;		t24=rt;	/* twiddle mpy by E^-2	*/
		rt =t34*s + t35*c;		it =t35*s - t34*c;			/* twiddle mpy by E^-6	*/
		t34=t24-rt;			t24=t24+rt;
		t35=t25-it;			t25=t25+it;

		a[jt    ]=t04+t24;		a[jp    ]=t05+t25;
		a[jt+p10]=t04-t24;		a[jp+p10]=t05-t25;

		a[jt+p08]=t14+t35;		a[jp+p08]=t15-t34;	/* mpy by E^-4 = -I is inlined here...	*/
		a[jt+p18]=t14-t35;		a[jp+p18]=t15+t34;

		/*...Block 7: t0C,t1C,t2C,t3C	*/
		jt =j1 + p06;	jp = j2 + p06;

		rt =(t1C-t1D)*ISRT2;	it =(t1C+t1D)*ISRT2;			/* twiddle mpy by E^4 = -E^-12 is here...	*/
		t1C=t0C+rt;			t0C=t0C-rt;				/* ...and get E^-12 by flipping signs here.	*/
		t1D=t0D+it;			t0D=t0D-it;

		rt =t2C*s + t2D*c;		t2D=t2D*s - t2C*c;		t2C=rt;	/* twiddle mpy by E^-6	*/
		rt =t3C*c + t3D*s;		it =t3D*c - t3C*s;			/* twiddle mpy by E^-18 is here...	*/
		t3C=t2C+rt;			t2C=t2C-rt;				/* ...and get E^-18 by flipping signs here.	*/
		t3D=t2D+it;			t2D=t2D-it;

		a[jt    ]=t0C+t2C;		a[jp    ]=t0D+t2D;
		a[jt+p10]=t0C-t2C;		a[jp+p10]=t0D-t2D;

		a[jt+p08]=t1C+t3D;		a[jp+p08]=t1D-t3C;	/* mpy by E^-4 = -I is inlined here...	*/
		a[jt+p18]=t1C-t3D;		a[jp+p18]=t1D+t3C;

		/*...Block 2: t02,t12,t22,t32	*/
		jt =j1 + p01;	jp = j2 + p01;

		rt =t12*c + t13*s;		it =t13*c - t12*s;			/* twiddle mpy by E^-2	*/
		t12=t02-rt;			t02=t02+rt;
		t13=t03-it;			t03=t03+it;

		rt =t22*c32_1 + t23*s32_1;	t23=t23*c32_1 - t22*s32_1;	t22=rt;	/* twiddle mpy by E^-1	*/
		rt =t32*c32_3 + t33*s32_3;	it =t33*c32_3 - t32*s32_3;		/* twiddle mpy by E^-3	*/
		t32=t22-rt;			t22=t22+rt;
		t33=t23-it;			t23=t23+it;

		a[jt    ]=t02+t22;		a[jp    ]=t03+t23;
		a[jt+p10]=t02-t22;		a[jp+p10]=t03-t23;

		a[jt+p08]=t12+t33;		a[jp+p08]=t13-t32;	/* mpy by E^-4 = -I is inlined here...	*/
		a[jt+p18]=t12-t33;		a[jp+p18]=t13+t32;

		/*...Block 6: t0A,t1A,t2A,t3A	*/
		jt =j1 + p05;	jp = j2 + p05;

		rt =t1A*s - t1B*c;		it =t1B*s + t1A*c;			/* twiddle mpy by -E^-10 is here...	*/
		t1A=t0A+rt;			t0A =t0A-rt;				/* ...and get E^-10 by flipping signs here.	*/
		t1B=t0B+it;			t0B =t0B-it;

		rt =t2A*s32_3 + t2B*c32_3;	t2B=t2B*s32_3 - t2A*c32_3;	t2A=rt;	/* twiddle mpy by E^-5	*/
		rt =t3A*c32_1 - t3B*s32_1;	it =t3B*c32_1 + t3A*s32_1;		/* twiddle mpy by -E^-15 is here...	*/
		t3A=t2A+rt;			t2A=t2A-rt;				/* ...and get E^-15 by flipping signs here.	*/
		t3B=t2B+it;			t2B=t2B-it;

		a[jt    ]=t0A+t2A;		a[jp    ]=t0B+t2B;
		a[jt+p10]=t0A-t2A;		a[jp+p10]=t0B-t2B;

		a[jt+p08]=t1A+t3B;		a[jp+p08]=t1B-t3A;	/* mpy by E^-4 = -I is inlined here...	*/
		a[jt+p18]=t1A-t3B;		a[jp+p18]=t1B+t3A;

		/*...Block 4: t06,t16,t26,t36	*/
		jt =j1 + p03;	jp = j2 + p03;

		rt =t16*s + t17*c;		it =t17*s - t16*c;			/* twiddle mpy by E^-6	*/
		t16=t06-rt;			t06 =t06+rt;
		t17=t07-it;			t07 =t07+it;

		rt =t26*c32_3 + t27*s32_3;	t27=t27*c32_3 - t26*s32_3;	t26=rt;	/* twiddle mpy by E^-3	*/
		rt =t36*s32_1 - t37*c32_1;	it =t37*s32_1 + t36*c32_1;		/* twiddle mpy by -E^-9 is here...	*/
		t36=t26+rt;			t26=t26-rt;				/* ...and get E^-9 by flipping signs here.	*/
		t37=t27+it;			t27=t27-it;

		a[jt    ]=t06+t26;		a[jp    ]=t07+t27;
		a[jt+p10]=t06-t26;		a[jp+p10]=t07-t27;

		a[jt+p08]=t16+t37;		a[jp+p08]=t17-t36;	/* mpy by E^-4 = -I is inlined here...	*/
		a[jt+p18]=t16-t37;		a[jp+p18]=t17+t36;

		/*...Block 8: t0E,t1E,t2E,t3E	*/
		jt =j1 + p07;	jp = j2 + p07;

		rt =t1E*c - t1F*s;		it =t1F*c + t1E*s;			/* twiddle mpy by -E^-14 is here...	*/
		t1E=t0E+rt;			t0E =t0E-rt;				/* ...and get E^-14 by flipping signs here.	*/
		t1F=t0F+it;			t0F =t0F-it;

		rt =t2E*s32_1 + t2F*c32_1;	t2F=t2F*s32_1 - t2E*c32_1;	t2E=rt;	/* twiddle mpy by E^-7	*/
		rt =t3E*s32_3 + t3F*c32_3;	it =t3F*s32_3 - t3E*c32_3;		/* twiddle mpy by -E^-21 is here...	*/
		t3E=t2E+rt;			t2E=t2E-rt;				/* ...and get E^-21 by flipping signs here.	*/
		t3F=t2F+it;			t2F=t2F-it;

		a[jt    ]=t0E+t2E;		a[jp    ]=t0F+t2F;
		a[jt+p10]=t0E-t2E;		a[jp+p10]=t0F-t2F;

		a[jt+p08]=t1E+t3F;		a[jp+p08]=t1F-t3E;	/* mpy by E^-4 = -I is inlined here...	*/
		a[jt+p18]=t1E-t3F;		a[jp+p18]=t1F+t3E;
	  #endif

	#endif	/* USE_SSE2 */
	}
}

/******************** Multithreaded function body: ***************************/

#ifdef USE_PTHREAD

	#ifndef USE_SSE2
		#error pthreaded carry code requires SSE2-enabled build!
	#endif
	#ifndef COMPILER_TYPE_GCC
		#error pthreaded carry code requires GCC build!
	#endif

	void* 
	cy32_process_chunk(void*targ)	// Thread-arg pointer *must* be cast to void and specialized inside the function
	{
		const uint32 RADIX = 32;
		const double crnd = 3.0*0x4000000*0x2000000;
		int idx_offset,idx_incr;
		int j,j1,j2,k;
		int l,n_minus_sil,n_minus_silp1,sinwt,sinwtm1;
		double wtl,wtlp1,wtn,wtnm1;	/* Mersenne-mod weights stuff */
		uint32 p01,p02,p03,p04,p08,p10,p18;
		double *add0, *add1, *add2;
		struct complex *isrt2, *cc0, *ss0, *cc1, *ss1, *cc3, *ss3, *max_err, *sse2_rnd, *half_arr, *tmp
			     ,*r02,*r04,*r06,*r08,*r0A,*r0C,*r0E
			,*r10,*r12,*r14,*r16,*r18,*r1A,*r1C,*r1E
			,*r20,*r22,*r24,*r26,*r28,*r2A,*r2C,*r2E
			,*r30,*r32,*r34,*r36,*r38,*r3A,*r3C,*r3E;
	
		struct complex *cy_r00,*cy_r02,*cy_r04,*cy_r06,*cy_r08,*cy_r0A,*cy_r0C,*cy_r0E,*cy_r10,*cy_r12,*cy_r14,*cy_r16,*cy_r18,*cy_r1A,*cy_r1C,*cy_r1E;
		struct complex *cy_i00,*cy_i02,*cy_i04,*cy_i06,*cy_i08,*cy_i0A,*cy_i0C,*cy_i0E,*cy_i10,*cy_i12,*cy_i14,*cy_i16,*cy_i18,*cy_i1A,*cy_i1C,*cy_i1E;
		int *bjmodn00,*bjmodn01,*bjmodn02,*bjmodn03,*bjmodn04,*bjmodn05,*bjmodn06,*bjmodn07,*bjmodn08,*bjmodn09,*bjmodn0A,*bjmodn0B,*bjmodn0C,*bjmodn0D,*bjmodn0E,*bjmodn0F,*bjmodn10,*bjmodn11,*bjmodn12,*bjmodn13,*bjmodn14,*bjmodn15,*bjmodn16,*bjmodn17,*bjmodn18,*bjmodn19,*bjmodn1A,*bjmodn1B,*bjmodn1C,*bjmodn1D,*bjmodn1E,*bjmodn1F;
		uint64 *sign_mask, *sse_bw, *sse_sw, *sse_nm1;

		struct cy_thread_data_t* thread_arg = targ;

	// int data:
		int NDIVR = thread_arg->ndivr;
		int n = NDIVR*RADIX;
		int khi    = thread_arg->khi;
		int i      = thread_arg->i;	/* Pointer to the BASE and BASEINV arrays.	*/
		int jstart = thread_arg->jstart;
		int jhi    = thread_arg->jhi;
		int col = thread_arg->col;
		int co2 = thread_arg->co2;
		int co3 = thread_arg->co3;
		int sw  = thread_arg->sw;
		int nwt = thread_arg->nwt;

	// double data:
		double maxerr = thread_arg->maxerr;
		double scale = thread_arg->scale;

	// pointer data:
		double *a = thread_arg->arrdat;	
		double *wt0 = thread_arg->wt0;
		double *wt1 = thread_arg->wt1;
		int *si = thread_arg->si;
		struct complex *rn0 = thread_arg->rn0;
		struct complex *rn1 = thread_arg->rn1;	
		struct complex *r00 = thread_arg->r00;

		p01 = NDIVR;
		p02 = p01 +p01;
		p03 = p02 +p01;
		p04 = p03 +p01;
		p08 = p04 +p04;
		p10 = p08 +p08;
		p18 = p10 +p08;

		p01 = p01 + ( (p01 >> DAT_BITS) << PAD_BITS );
		p02 = p02 + ( (p02 >> DAT_BITS) << PAD_BITS );
		p03 = p03 + ( (p03 >> DAT_BITS) << PAD_BITS );
		p04 = p04 + ( (p04 >> DAT_BITS) << PAD_BITS );
		p08 = p08 + ( (p08 >> DAT_BITS) << PAD_BITS );
		p10 = p10 + ( (p10 >> DAT_BITS) << PAD_BITS );
		p18 = p18 + ( (p18 >> DAT_BITS) << PAD_BITS );

		r02		= r00 + 0x02;	isrt2	= r00 + 0x40;
		r04		= r00 + 0x04;	cc0		= r00 + 0x41;
		r06		= r00 + 0x06;	ss0		= r00 + 0x42;
		r08		= r00 + 0x08;	cc1		= r00 + 0x43;
		r0A		= r00 + 0x0a;	ss1		= r00 + 0x44;
		r0C		= r00 + 0x0c;	cc3		= r00 + 0x45;
		r0E		= r00 + 0x0e;	ss3		= r00 + 0x46;
		r10		= r00 + 0x10;	cy_r00	= r00 + 0x47;
		r12		= r00 + 0x12;	cy_r02	= r00 + 0x48;
		r14		= r00 + 0x14;	cy_r04	= r00 + 0x49;
		r16		= r00 + 0x16;	cy_r06	= r00 + 0x4a;
		r18		= r00 + 0x18;	cy_r08	= r00 + 0x4b;
		r1A		= r00 + 0x1a;	cy_r0A	= r00 + 0x4c;
		r1C		= r00 + 0x1c;	cy_r0C	= r00 + 0x4d;
		r1E		= r00 + 0x1e;	cy_r0E	= r00 + 0x4e;
		r20		= r00 + 0x20;	cy_r10	= r00 + 0x4f;
		r22		= r00 + 0x22;	cy_r12	= r00 + 0x50;
		r24		= r00 + 0x24;	cy_r14	= r00 + 0x51;
		r26		= r00 + 0x26;	cy_r16	= r00 + 0x52;
		r28		= r00 + 0x28;	cy_r18	= r00 + 0x53;
		r2A		= r00 + 0x2a;	cy_r1A	= r00 + 0x54;
		r2C		= r00 + 0x2c;	cy_r1C	= r00 + 0x55;
		r2E		= r00 + 0x2e;	cy_r1E	= r00 + 0x56;
		r30		= r00 + 0x30;	cy_i00	= r00 + 0x57;
		r32		= r00 + 0x32;	cy_i02	= r00 + 0x58;
		r34		= r00 + 0x34;	cy_i04	= r00 + 0x59;
		r36		= r00 + 0x36;	cy_i06	= r00 + 0x5a;
		r38		= r00 + 0x38;	cy_i08	= r00 + 0x5b;
		r3A		= r00 + 0x3a;	cy_i0A	= r00 + 0x5c;
		r3C		= r00 + 0x3c;	cy_i0C	= r00 + 0x5d;
		r3E		= r00 + 0x3e;	cy_i0E	= r00 + 0x5e;
								cy_i10	= r00 + 0x5f;
								cy_i12	= r00 + 0x60;
								cy_i14	= r00 + 0x61;
								cy_i16	= r00 + 0x62;
								cy_i18	= r00 + 0x63;
								cy_i1A	= r00 + 0x64;
								cy_i1C	= r00 + 0x65;
								cy_i1E	= r00 + 0x66;
								max_err = r00 + 0x67;
								sse2_rnd= r00 + 0x68;
								half_arr= r00 + 0x69;	/* This table needs 20x16 bytes */
		ASSERT(HERE, (isrt2->re == ISRT2 && isrt2->im == ISRT2), "thread-local memcheck failed!");
		ASSERT(HERE, (sse2_rnd->re == crnd && sse2_rnd->im == crnd), "thread-local memcheck failed!");
	if(MODULUS_TYPE == MODULUS_TYPE_MERSENNE)
	{
		ASSERT(HERE, (half_arr+10)->re * (half_arr+14)->re == 1.0 && (half_arr+10)->im * (half_arr+14)->im == 1.0, "thread-local memcheck failed!");
	} else {
		ASSERT(HERE, half_arr->re * (half_arr+1)->re == 1.0 && half_arr->im * (half_arr+1)->im == 1.0, "thread-local memcheck failed!");
	}

		max_err->re = 0.0;	max_err->im = 0.0;

		sign_mask = (uint64*)(r00 + radix32_creals_in_local_store);
		sse_bw  = sign_mask + 2;
		sse_sw  = sign_mask + 4;
		sse_nm1 = sign_mask + 6;
		bjmodn00 = (int*)(sign_mask + 8);
		bjmodn01 = bjmodn00 + 0x01;
		bjmodn02 = bjmodn00 + 0x02;
		bjmodn03 = bjmodn00 + 0x03;
		bjmodn04 = bjmodn00 + 0x04;
		bjmodn05 = bjmodn00 + 0x05;
		bjmodn06 = bjmodn00 + 0x06;
		bjmodn07 = bjmodn00 + 0x07;
		bjmodn08 = bjmodn00 + 0x08;
		bjmodn09 = bjmodn00 + 0x09;
		bjmodn0A = bjmodn00 + 0x0A;
		bjmodn0B = bjmodn00 + 0x0B;
		bjmodn0C = bjmodn00 + 0x0C;
		bjmodn0D = bjmodn00 + 0x0D;
		bjmodn0E = bjmodn00 + 0x0E;
		bjmodn0F = bjmodn00 + 0x0F;
		bjmodn10 = bjmodn00 + 0x10;
		bjmodn11 = bjmodn00 + 0x11;
		bjmodn12 = bjmodn00 + 0x12;
		bjmodn13 = bjmodn00 + 0x13;
		bjmodn14 = bjmodn00 + 0x14;
		bjmodn15 = bjmodn00 + 0x15;
		bjmodn16 = bjmodn00 + 0x16;
		bjmodn17 = bjmodn00 + 0x17;
		bjmodn18 = bjmodn00 + 0x18;
		bjmodn19 = bjmodn00 + 0x19;
		bjmodn1A = bjmodn00 + 0x1A;
		bjmodn1B = bjmodn00 + 0x1B;
		bjmodn1C = bjmodn00 + 0x1C;
		bjmodn1D = bjmodn00 + 0x1D;
		bjmodn1E = bjmodn00 + 0x1E;
		bjmodn1F = bjmodn00 + 0x1F;

		if(MODULUS_TYPE == MODULUS_TYPE_MERSENNE)
		{
			/* Init DWT-indices: */	/* init carries	*/
			*bjmodn00 = thread_arg->bjmodn00;	cy_r00->re = thread_arg->cy_r00;
			*bjmodn01 = thread_arg->bjmodn01;	cy_r00->im = thread_arg->cy_r01;
			*bjmodn02 = thread_arg->bjmodn02;	cy_r02->re = thread_arg->cy_r02;
			*bjmodn03 = thread_arg->bjmodn03;	cy_r02->im = thread_arg->cy_r03;
			*bjmodn04 = thread_arg->bjmodn04;	cy_r04->re = thread_arg->cy_r04;
			*bjmodn05 = thread_arg->bjmodn05;	cy_r04->im = thread_arg->cy_r05;
			*bjmodn06 = thread_arg->bjmodn06;	cy_r06->re = thread_arg->cy_r06;
			*bjmodn07 = thread_arg->bjmodn07;	cy_r06->im = thread_arg->cy_r07;
			*bjmodn08 = thread_arg->bjmodn08;	cy_r08->re = thread_arg->cy_r08;
			*bjmodn09 = thread_arg->bjmodn09;	cy_r08->im = thread_arg->cy_r09;
			*bjmodn0A = thread_arg->bjmodn0A;	cy_r0A->re = thread_arg->cy_r0A;
			*bjmodn0B = thread_arg->bjmodn0B;	cy_r0A->im = thread_arg->cy_r0B;
			*bjmodn0C = thread_arg->bjmodn0C;	cy_r0C->re = thread_arg->cy_r0C;
			*bjmodn0D = thread_arg->bjmodn0D;	cy_r0C->im = thread_arg->cy_r0D;
			*bjmodn0E = thread_arg->bjmodn0E;	cy_r0E->re = thread_arg->cy_r0E;
			*bjmodn0F = thread_arg->bjmodn0F;	cy_r0E->im = thread_arg->cy_r0F;
			*bjmodn10 = thread_arg->bjmodn10;	cy_r10->re = thread_arg->cy_r10;
			*bjmodn11 = thread_arg->bjmodn11;	cy_r10->im = thread_arg->cy_r11;
			*bjmodn12 = thread_arg->bjmodn12;	cy_r12->re = thread_arg->cy_r12;
			*bjmodn13 = thread_arg->bjmodn13;	cy_r12->im = thread_arg->cy_r13;
			*bjmodn14 = thread_arg->bjmodn14;	cy_r14->re = thread_arg->cy_r14;
			*bjmodn15 = thread_arg->bjmodn15;	cy_r14->im = thread_arg->cy_r15;
			*bjmodn16 = thread_arg->bjmodn16;	cy_r16->re = thread_arg->cy_r16;
			*bjmodn17 = thread_arg->bjmodn17;	cy_r16->im = thread_arg->cy_r17;
			*bjmodn18 = thread_arg->bjmodn18;	cy_r18->re = thread_arg->cy_r18;
			*bjmodn19 = thread_arg->bjmodn19;	cy_r18->im = thread_arg->cy_r19;
			*bjmodn1A = thread_arg->bjmodn1A;	cy_r1A->re = thread_arg->cy_r1A;
			*bjmodn1B = thread_arg->bjmodn1B;	cy_r1A->im = thread_arg->cy_r1B;
			*bjmodn1C = thread_arg->bjmodn1C;	cy_r1C->re = thread_arg->cy_r1C;
			*bjmodn1D = thread_arg->bjmodn1D;	cy_r1C->im = thread_arg->cy_r1D;
			*bjmodn1E = thread_arg->bjmodn1E;	cy_r1E->re = thread_arg->cy_r1E;
			*bjmodn1F = thread_arg->bjmodn1F;	cy_r1E->im = thread_arg->cy_r1F;
		}
		else	/* Fermat-mod uses "double helix" carry scheme - 2 separate sets of real/imaginary carries for right-angle transform, plus "twisted" wraparound step. */
		{
			/* init carries	*/
			cy_r00->re = thread_arg->cy_r00;	cy_r00->im = thread_arg->cy_i00;
			cy_r02->re = thread_arg->cy_r01;	cy_r02->im = thread_arg->cy_i01;
			cy_r04->re = thread_arg->cy_r02;	cy_r04->im = thread_arg->cy_i02;
			cy_r06->re = thread_arg->cy_r03;	cy_r06->im = thread_arg->cy_i03;
			cy_r08->re = thread_arg->cy_r04;	cy_r08->im = thread_arg->cy_i04;
			cy_r0A->re = thread_arg->cy_r05;	cy_r0A->im = thread_arg->cy_i05;
			cy_r0C->re = thread_arg->cy_r06;	cy_r0C->im = thread_arg->cy_i06;
			cy_r0E->re = thread_arg->cy_r07;	cy_r0E->im = thread_arg->cy_i07;
			cy_r10->re = thread_arg->cy_r08;	cy_r10->im = thread_arg->cy_i08;
			cy_r12->re = thread_arg->cy_r09;	cy_r12->im = thread_arg->cy_i09;
			cy_r14->re = thread_arg->cy_r0A;	cy_r14->im = thread_arg->cy_i0A;
			cy_r16->re = thread_arg->cy_r0B;	cy_r16->im = thread_arg->cy_i0B;
			cy_r18->re = thread_arg->cy_r0C;	cy_r18->im = thread_arg->cy_i0C;
			cy_r1A->re = thread_arg->cy_r0D;	cy_r1A->im = thread_arg->cy_i0D;
			cy_r1C->re = thread_arg->cy_r0E;	cy_r1C->im = thread_arg->cy_i0E;
			cy_r1E->re = thread_arg->cy_r0F;	cy_r1E->im = thread_arg->cy_i0F;
			cy_i00->re = thread_arg->cy_r10;	cy_i00->im = thread_arg->cy_i10;
			cy_i02->re = thread_arg->cy_r11;	cy_i02->im = thread_arg->cy_i11;
			cy_i04->re = thread_arg->cy_r12;	cy_i04->im = thread_arg->cy_i12;
			cy_i06->re = thread_arg->cy_r13;	cy_i06->im = thread_arg->cy_i13;
			cy_i08->re = thread_arg->cy_r14;	cy_i08->im = thread_arg->cy_i14;
			cy_i0A->re = thread_arg->cy_r15;	cy_i0A->im = thread_arg->cy_i15;
			cy_i0C->re = thread_arg->cy_r16;	cy_i0C->im = thread_arg->cy_i16;
			cy_i0E->re = thread_arg->cy_r17;	cy_i0E->im = thread_arg->cy_i17;
			cy_i10->re = thread_arg->cy_r18;	cy_i10->im = thread_arg->cy_i18;
			cy_i12->re = thread_arg->cy_r19;	cy_i12->im = thread_arg->cy_i19;
			cy_i14->re = thread_arg->cy_r1A;	cy_i14->im = thread_arg->cy_i1A;
			cy_i16->re = thread_arg->cy_r1B;	cy_i16->im = thread_arg->cy_i1B;
			cy_i18->re = thread_arg->cy_r1C;	cy_i18->im = thread_arg->cy_i1C;
			cy_i1A->re = thread_arg->cy_r1D;	cy_i1A->im = thread_arg->cy_i1D;
			cy_i1C->re = thread_arg->cy_r1E;	cy_i1C->im = thread_arg->cy_i1E;
			cy_i1E->re = thread_arg->cy_r1F;	cy_i1E->im = thread_arg->cy_i1F;
		}

#if FFT_DEBUG
	int ithread = thread_arg->tid;	/* unique thread index (use for debug) */
	fprintf(dbg_file,"cy32_process_chunk: thread %d, NDIVR = %d, NWT = %d, &wt0,1 = %llx %llx, khi = %d, jlo,jhi = %d %d, scale = %15.5e\n"\
		, ithread, NDIVR, nwt, (uint64)wt0, (uint64)wt1, khi, jstart,jhi, scale);
//	printf("cy32_process_chunk: thread %d, scale = %15.5e\n", ithread,thread_arg->scale);
#endif

		for(k=1; k <= khi; k++)	/* Do n/(radix(1)*nwt) outer loop executions...	*/
		{
			for(j = jstart; j < jhi; j += 4)
			{
			/* In SSE2 mode, data are arranged in [re0,re1,im0,im1] quartets, not the usual [re0,im0],[re1,im1] pairs.
			Thus we can still increment the j-index as if stepping through the residue array-of-doubles in strides of 2,
			but to point to the proper real datum, we need to bit-reverse bits <0:1> of j, i.e. [0,1,2,3] ==> [0,2,1,3].
			*/
				j1 = (j & mask01) + br4[j&3];
				j1 = j1 + ( (j1 >> DAT_BITS) << PAD_BITS );	/* padded-array fetch index is here */
				j2 = j1+RE_IM_STRIDE;

			add0 = &a[j1    ];
			SSE2_RADIX32_DIT_NOTWIDDLE(add0,p01,p02,p03,p04,p08,p10,p18,r00,isrt2,cc0);

		/*...Now do the carries. Since the outputs would
		normally be getting dispatched to 32 separate blocks of the A-array, we need 32 separate carries.	*/

		/************ See the radix16_ditN_cy_dif1 routine for details on how the SSE2 carry stuff works **********/
			if(MODULUS_TYPE == MODULUS_TYPE_MERSENNE)
			{
				l= j & (nwt-1);			/* We want (S*J mod N) - SI(L) for all 32 carries, so precompute	*/
				n_minus_sil   = n-si[l  ];		/* N - SI(L) and for each J, find N - (B*J mod N) - SI(L)		*/
				n_minus_silp1 = n-si[l+1];		/* For the inverse weight, want (S*(N - J) mod N) - SI(NWT - L) =	*/
				sinwt   = si[nwt-l  ];		/*	= N - (S*J mod N) - SI(NWT - L) = (B*J mod N) - SI(NWT - L).	*/
				sinwtm1 = si[nwt-l-1];

				wtl     =wt0[    l  ];
				wtn     =wt0[nwt-l  ]*scale;	/* Include 1/(n/2) scale factor of inverse transform here...	*/
				wtlp1   =wt0[    l+1];
				wtnm1   =wt0[nwt-l-1]*scale;	/* ...and here.	*/

				tmp = half_arr + 16;	/* ptr to local storage for the doubled wtl,wtn terms: */
				tmp->re = wtl;		tmp->im = wtl;	++tmp;
				tmp->re = wtn;		tmp->im = wtn;	++tmp;
				tmp->re = wtlp1;	tmp->im = wtlp1;++tmp;
				tmp->re = wtnm1;	tmp->im = wtnm1;

				add0 = &wt1[col  ];
				add1 = &wt1[co2-1];
				add2 = &wt1[co3-1];

			  #ifdef ERR_CHECK_ALL
				SSE2_cmplx_carry_norm_pow2_errcheck0_2B(r00,add0,add1,add2,cy_r00,cy_r02,bjmodn00,half_arr,i,n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_errcheck1_2B(r08,add0,add1,add2,cy_r04,cy_r06,bjmodn04,half_arr,  n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_errcheck1_2B(r10,add0,add1,add2,cy_r08,cy_r0A,bjmodn08,half_arr,  n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_errcheck1_2B(r18,add0,add1,add2,cy_r0C,cy_r0E,bjmodn0C,half_arr,  n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_errcheck1_2B(r20,add0,add1,add2,cy_r10,cy_r12,bjmodn10,half_arr,  n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_errcheck1_2B(r28,add0,add1,add2,cy_r14,cy_r16,bjmodn14,half_arr,  n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_errcheck1_2B(r30,add0,add1,add2,cy_r18,cy_r1A,bjmodn18,half_arr,  n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_errcheck1_2B(r38,add0,add1,add2,cy_r1C,cy_r1E,bjmodn1C,half_arr,  n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
			  #else
				SSE2_cmplx_carry_norm_pow2_errcheck0_2B(r00,add0,add1,add2,cy_r00,cy_r02,bjmodn00,half_arr,i,n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_nocheck1_2B (r08,add0,add1,add2,cy_r04,cy_r06,bjmodn04,half_arr,  n_minus_silp1,n_minus_sil,          sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_nocheck1_2B (r10,add0,add1,add2,cy_r08,cy_r0A,bjmodn08,half_arr,  n_minus_silp1,n_minus_sil,          sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_nocheck1_2B (r18,add0,add1,add2,cy_r0C,cy_r0E,bjmodn0C,half_arr,  n_minus_silp1,n_minus_sil,          sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_nocheck1_2B (r20,add0,add1,add2,cy_r10,cy_r12,bjmodn10,half_arr,  n_minus_silp1,n_minus_sil,          sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_nocheck1_2B (r28,add0,add1,add2,cy_r14,cy_r16,bjmodn14,half_arr,  n_minus_silp1,n_minus_sil,          sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_nocheck1_2B (r30,add0,add1,add2,cy_r18,cy_r1A,bjmodn18,half_arr,  n_minus_silp1,n_minus_sil,          sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_nocheck1_2B (r38,add0,add1,add2,cy_r1C,cy_r1E,bjmodn1C,half_arr,  n_minus_silp1,n_minus_sil,          sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
			  #endif

				l= (j+2) & (nwt-1);			/* We want (S*J mod N) - SI(L) for all 16 carries, so precompute	*/
				n_minus_sil   = n-si[l  ];		/* N - SI(L) and for each J, find N - (B*J mod N) - SI(L)		*/
				n_minus_silp1 = n-si[l+1];		/* For the inverse weight, want (S*(N - J) mod N) - SI(NWT - L) =	*/
				sinwt   = si[nwt-l  ];		/*	= N - (S*J mod N) - SI(NWT - L) = (B*J mod N) - SI(NWT - L).	*/
				sinwtm1 = si[nwt-l-1];

				wtl     =wt0[    l  ];
				wtn     =wt0[nwt-l  ]*scale;	/* Include 1/(n/2) scale factor of inverse transform here...	*/
				wtlp1   =wt0[    l+1];
				wtnm1   =wt0[nwt-l-1]*scale;	/* ...and here.	*/

				tmp = half_arr + 16;	/* ptr to localstorage for the doubled wtl,wtn terms: */
				tmp->re = wtl;		tmp->im = wtl;	++tmp;
				tmp->re = wtn;		tmp->im = wtn;	++tmp;
				tmp->re = wtlp1;	tmp->im = wtlp1;++tmp;
				tmp->re = wtnm1;	tmp->im = wtnm1;

			/*	i =((uint32)(sw - *bjmodn0) >> 31);	Don't need this here, since no special index-0 macro in the set below */

				co2 = co3;	/* For all data but the first set in each j-block, co2=co3. Thus, after the first block of data is done
							(and only then: for all subsequent blocks it's superfluous), this assignment decrements co2 by radix(1).	*/

				add0 = &wt1[col  ];
				add1 = &wt1[co2-1];

			  #ifdef ERR_CHECK_ALL
				SSE2_cmplx_carry_norm_pow2_errcheck2_2B(r00,add0,add1,     cy_r00,cy_r02,bjmodn00,half_arr,n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_errcheck2_2B(r08,add0,add1,     cy_r04,cy_r06,bjmodn04,half_arr,n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_errcheck2_2B(r10,add0,add1,     cy_r08,cy_r0A,bjmodn08,half_arr,n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_errcheck2_2B(r18,add0,add1,     cy_r0C,cy_r0E,bjmodn0C,half_arr,n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_errcheck2_2B(r20,add0,add1,     cy_r10,cy_r12,bjmodn10,half_arr,n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_errcheck2_2B(r28,add0,add1,     cy_r14,cy_r16,bjmodn14,half_arr,n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_errcheck2_2B(r30,add0,add1,     cy_r18,cy_r1A,bjmodn18,half_arr,n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_errcheck2_2B(r38,add0,add1,     cy_r1C,cy_r1E,bjmodn1C,half_arr,n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
			  #else
				SSE2_cmplx_carry_norm_pow2_nocheck2_2B (r00,add0,add1,     cy_r00,cy_r02,bjmodn00,half_arr,n_minus_silp1,n_minus_sil,          sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_nocheck2_2B (r08,add0,add1,     cy_r04,cy_r06,bjmodn04,half_arr,n_minus_silp1,n_minus_sil,          sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_nocheck2_2B (r10,add0,add1,     cy_r08,cy_r0A,bjmodn08,half_arr,n_minus_silp1,n_minus_sil,          sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_nocheck2_2B (r18,add0,add1,     cy_r0C,cy_r0E,bjmodn0C,half_arr,n_minus_silp1,n_minus_sil,          sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_nocheck2_2B (r20,add0,add1,     cy_r10,cy_r12,bjmodn10,half_arr,n_minus_silp1,n_minus_sil,          sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_nocheck2_2B (r28,add0,add1,     cy_r14,cy_r16,bjmodn14,half_arr,n_minus_silp1,n_minus_sil,          sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_nocheck2_2B (r30,add0,add1,     cy_r18,cy_r1A,bjmodn18,half_arr,n_minus_silp1,n_minus_sil,          sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
				SSE2_cmplx_carry_norm_pow2_nocheck2_2B (r38,add0,add1,     cy_r1C,cy_r1E,bjmodn1C,half_arr,n_minus_silp1,n_minus_sil,          sinwt,sinwtm1,sse_bw,sse_nm1,sse_sw);
			  #endif

				i =((uint32)(sw - *bjmodn00) >> 31);	/* get ready for the next set...	*/
			}
			else	/* Fermat-mod carry in SSE2 mode */
			{
				tmp = half_arr+2;
				tmp->re = tmp->im = scale;
				/* Get the needed Nth root of -1: */
				add1 = &rn0[0];
				add2 = &rn1[0];

				idx_offset = j;
				idx_incr = NDIVR;

			  #if (OS_BITS == 32) || !defined(USE_64BIT_ASM_STYLE)	// In 64-bit mode, default is to use simple 64-bit-ified version of the analogous 32-bit 
				SSE2_fermat_carry_norm_pow2_errcheck(r00,cy_r00,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r02,cy_r02,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r04,cy_r04,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r06,cy_r06,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r08,cy_r08,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r0A,cy_r0A,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r0C,cy_r0C,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r0E,cy_r0E,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r10,cy_r10,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r12,cy_r12,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r14,cy_r14,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r16,cy_r16,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r18,cy_r18,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r1A,cy_r1A,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r1C,cy_r1C,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r1E,cy_r1E,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r20,cy_i00,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r22,cy_i02,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r24,cy_i04,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r26,cy_i06,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r28,cy_i08,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r2A,cy_i0A,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r2C,cy_i0C,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r2E,cy_i0E,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r30,cy_i10,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r32,cy_i12,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r34,cy_i14,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r36,cy_i16,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r38,cy_i18,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r3A,cy_i1A,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r3C,cy_i1C,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck(r3E,cy_i1E,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
			  #else
				SSE2_fermat_carry_norm_pow2_errcheck_X2(r00,cy_r00,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck_X2(r04,cy_r04,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck_X2(r08,cy_r08,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck_X2(r0C,cy_r0C,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck_X2(r10,cy_r10,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck_X2(r14,cy_r14,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck_X2(r18,cy_r18,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck_X2(r1C,cy_r1C,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck_X2(r20,cy_i00,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck_X2(r24,cy_i04,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck_X2(r28,cy_i08,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck_X2(r2C,cy_i0C,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck_X2(r30,cy_i10,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck_X2(r34,cy_i14,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck_X2(r38,cy_i18,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
				SSE2_fermat_carry_norm_pow2_errcheck_X2(r3C,cy_i1C,NRT_BITS,NRTM1,idx_offset,idx_incr,half_arr,sign_mask,add1,add2);
			  #endif
			}

			add0 = &a[j1    ];
			SSE2_RADIX32_DIF_NOTWIDDLE(add0,p01,p02,p03,p04,p08,p10,p18,r00,isrt2,cc0);

			}	/* end for(j=_jstart; j < _jhi; j += 2) */

			if(MODULUS_TYPE == MODULUS_TYPE_MERSENNE)
			{
				jstart += nwt;
				jhi    += nwt;

				col += RADIX;
				co3 -= RADIX;
			}
		}	/* end for(k=1; k <= khi; k++) */

		/* At end of each thread-processed work chunk, dump the
		carryouts into their non-thread-private array slots:
		*/
		if(MODULUS_TYPE == MODULUS_TYPE_MERSENNE)
		{
			thread_arg->cy_r00 = cy_r00->re;
			thread_arg->cy_r01 = cy_r00->im;
			thread_arg->cy_r02 = cy_r02->re;
			thread_arg->cy_r03 = cy_r02->im;
			thread_arg->cy_r04 = cy_r04->re;
			thread_arg->cy_r05 = cy_r04->im;
			thread_arg->cy_r06 = cy_r06->re;
			thread_arg->cy_r07 = cy_r06->im;
			thread_arg->cy_r08 = cy_r08->re;
			thread_arg->cy_r09 = cy_r08->im;
			thread_arg->cy_r0A = cy_r0A->re;
			thread_arg->cy_r0B = cy_r0A->im;
			thread_arg->cy_r0C = cy_r0C->re;
			thread_arg->cy_r0D = cy_r0C->im;
			thread_arg->cy_r0E = cy_r0E->re;
			thread_arg->cy_r0F = cy_r0E->im;
			thread_arg->cy_r10 = cy_r10->re;
			thread_arg->cy_r11 = cy_r10->im;
			thread_arg->cy_r12 = cy_r12->re;
			thread_arg->cy_r13 = cy_r12->im;
			thread_arg->cy_r14 = cy_r14->re;
			thread_arg->cy_r15 = cy_r14->im;
			thread_arg->cy_r16 = cy_r16->re;
			thread_arg->cy_r17 = cy_r16->im;
			thread_arg->cy_r18 = cy_r18->re;
			thread_arg->cy_r19 = cy_r18->im;
			thread_arg->cy_r1A = cy_r1A->re;
			thread_arg->cy_r1B = cy_r1A->im;
			thread_arg->cy_r1C = cy_r1C->re;
			thread_arg->cy_r1D = cy_r1C->im;
			thread_arg->cy_r1E = cy_r1E->re;
			thread_arg->cy_r1F = cy_r1E->im;
		}
		else
		{
			thread_arg->cy_r00 = cy_r00->re;	thread_arg->cy_i00 = cy_r00->im;
			thread_arg->cy_r01 = cy_r02->re;	thread_arg->cy_i01 = cy_r02->im;
			thread_arg->cy_r02 = cy_r04->re;	thread_arg->cy_i02 = cy_r04->im;
			thread_arg->cy_r03 = cy_r06->re;	thread_arg->cy_i03 = cy_r06->im;
			thread_arg->cy_r04 = cy_r08->re;	thread_arg->cy_i04 = cy_r08->im;
			thread_arg->cy_r05 = cy_r0A->re;	thread_arg->cy_i05 = cy_r0A->im;
			thread_arg->cy_r06 = cy_r0C->re;	thread_arg->cy_i06 = cy_r0C->im;
			thread_arg->cy_r07 = cy_r0E->re;	thread_arg->cy_i07 = cy_r0E->im;
			thread_arg->cy_r08 = cy_r10->re;	thread_arg->cy_i08 = cy_r10->im;
			thread_arg->cy_r09 = cy_r12->re;	thread_arg->cy_i09 = cy_r12->im;
			thread_arg->cy_r0A = cy_r14->re;	thread_arg->cy_i0A = cy_r14->im;
			thread_arg->cy_r0B = cy_r16->re;	thread_arg->cy_i0B = cy_r16->im;
			thread_arg->cy_r0C = cy_r18->re;	thread_arg->cy_i0C = cy_r18->im;
			thread_arg->cy_r0D = cy_r1A->re;	thread_arg->cy_i0D = cy_r1A->im;
			thread_arg->cy_r0E = cy_r1C->re;	thread_arg->cy_i0E = cy_r1C->im;
			thread_arg->cy_r0F = cy_r1E->re;	thread_arg->cy_i0F = cy_r1E->im;
			thread_arg->cy_r10 = cy_i00->re;	thread_arg->cy_i10 = cy_i00->im;
			thread_arg->cy_r11 = cy_i02->re;	thread_arg->cy_i11 = cy_i02->im;
			thread_arg->cy_r12 = cy_i04->re;	thread_arg->cy_i12 = cy_i04->im;
			thread_arg->cy_r13 = cy_i06->re;	thread_arg->cy_i13 = cy_i06->im;
			thread_arg->cy_r14 = cy_i08->re;	thread_arg->cy_i14 = cy_i08->im;
			thread_arg->cy_r15 = cy_i0A->re;	thread_arg->cy_i15 = cy_i0A->im;
			thread_arg->cy_r16 = cy_i0C->re;	thread_arg->cy_i16 = cy_i0C->im;
			thread_arg->cy_r17 = cy_i0E->re;	thread_arg->cy_i17 = cy_i0E->im;
			thread_arg->cy_r18 = cy_i10->re;	thread_arg->cy_i18 = cy_i10->im;
			thread_arg->cy_r19 = cy_i12->re;	thread_arg->cy_i19 = cy_i12->im;
			thread_arg->cy_r1A = cy_i14->re;	thread_arg->cy_i1A = cy_i14->im;
			thread_arg->cy_r1B = cy_i16->re;	thread_arg->cy_i1B = cy_i16->im;
			thread_arg->cy_r1C = cy_i18->re;	thread_arg->cy_i1C = cy_i18->im;
			thread_arg->cy_r1D = cy_i1A->re;	thread_arg->cy_i1D = cy_i1A->im;
			thread_arg->cy_r1E = cy_i1C->re;	thread_arg->cy_i1E = cy_i1C->im;
			thread_arg->cy_r1F = cy_i1E->re;	thread_arg->cy_i1F = cy_i1E->im;
		}
		if(max_err->re > max_err->im)
			maxerr = max_err->re;
		else
			maxerr = max_err->im;

		/* Since will lose separate maxerr values when threads are merged, save them after each pass. */
		if(thread_arg->maxerr < maxerr)
		{
			thread_arg->maxerr = maxerr;
		}
/*
if(ithread == 0) {
	if(scale < 1.0) {
		printf("cy32_process_chunk: thread %d, full_pass, maxerr = %20.15f, cy0 = %20.5f\n", ithread, maxerr, cy_r00->re);
	} else {
		printf("cy32_process_chunk: thread %d, cleanup_pass\n", ithread);
	}
}
*/
#if FFT_DEBUG
	double cy_sum = 0;
	cy_sum += fabs(thread_arg->cy_r00)+fabs(thread_arg->cy_r01)+fabs(thread_arg->cy_r02)+fabs(thread_arg->cy_r03)+fabs(thread_arg->cy_r04)+fabs(thread_arg->cy_r05)+fabs(thread_arg->cy_r06)+fabs(thread_arg->cy_r07)+fabs(thread_arg->cy_r08)+fabs(thread_arg->cy_r09)+fabs(thread_arg->cy_r0A)+fabs(thread_arg->cy_r0B)+fabs(thread_arg->cy_r0C)+fabs(thread_arg->cy_r0D)+fabs(thread_arg->cy_r0E)+fabs(thread_arg->cy_r0F)+fabs(thread_arg->cy_r10)+fabs(thread_arg->cy_r11)+fabs(thread_arg->cy_r12)+fabs(thread_arg->cy_r13)+fabs(thread_arg->cy_r14)+fabs(thread_arg->cy_r15)+fabs(thread_arg->cy_r16)+fabs(thread_arg->cy_r17)+fabs(thread_arg->cy_r18)+fabs(thread_arg->cy_r19)+fabs(thread_arg->cy_r1A)+fabs(thread_arg->cy_r1B)+fabs(thread_arg->cy_r1C)+fabs(thread_arg->cy_r1D)+fabs(thread_arg->cy_r1E)+fabs(thread_arg->cy_r1F);
	cy_sum += fabs(thread_arg->cy_i00)+fabs(thread_arg->cy_i01)+fabs(thread_arg->cy_i02)+fabs(thread_arg->cy_i03)+fabs(thread_arg->cy_i04)+fabs(thread_arg->cy_i05)+fabs(thread_arg->cy_i06)+fabs(thread_arg->cy_i07)+fabs(thread_arg->cy_i08)+fabs(thread_arg->cy_i09)+fabs(thread_arg->cy_i0A)+fabs(thread_arg->cy_i0B)+fabs(thread_arg->cy_i0C)+fabs(thread_arg->cy_i0D)+fabs(thread_arg->cy_i0E)+fabs(thread_arg->cy_i0F)+fabs(thread_arg->cy_i10)+fabs(thread_arg->cy_i11)+fabs(thread_arg->cy_i12)+fabs(thread_arg->cy_i13)+fabs(thread_arg->cy_i14)+fabs(thread_arg->cy_i15)+fabs(thread_arg->cy_i16)+fabs(thread_arg->cy_i17)+fabs(thread_arg->cy_i18)+fabs(thread_arg->cy_i19)+fabs(thread_arg->cy_i1A)+fabs(thread_arg->cy_i1B)+fabs(thread_arg->cy_i1C)+fabs(thread_arg->cy_i1D)+fabs(thread_arg->cy_i1E)+fabs(thread_arg->cy_i1F);
	if(cy_sum != 0.0)
	{
		printf("cy_r00 = %20.10f %20.10f\n", cy_r00->re,cy_r00->im);
		printf("cy_r02 = %20.10f %20.10f\n", cy_r02->re,cy_r02->im);
		printf("cy_r04 = %20.10f %20.10f\n", cy_r04->re,cy_r04->im);
		printf("cy_r06 = %20.10f %20.10f\n", cy_r06->re,cy_r06->im);
		printf("cy_r08 = %20.10f %20.10f\n", cy_r08->re,cy_r08->im);
		printf("cy_r0A = %20.10f %20.10f\n", cy_r0A->re,cy_r0A->im);
		printf("cy_r0C = %20.10f %20.10f\n", cy_r0C->re,cy_r0C->im);
		printf("cy_r0E = %20.10f %20.10f\n", cy_r0E->re,cy_r0E->im);
		printf("cy_r10 = %20.10f %20.10f\n", cy_r10->re,cy_r10->im);
		printf("cy_r12 = %20.10f %20.10f\n", cy_r12->re,cy_r12->im);
		printf("cy_r14 = %20.10f %20.10f\n", cy_r14->re,cy_r14->im);
		printf("cy_r16 = %20.10f %20.10f\n", cy_r16->re,cy_r16->im);
		printf("cy_r18 = %20.10f %20.10f\n", cy_r18->re,cy_r18->im);
		printf("cy_r1A = %20.10f %20.10f\n", cy_r1A->re,cy_r1A->im);
		printf("cy_r1C = %20.10f %20.10f\n", cy_r1C->re,cy_r1C->im);
		printf("cy_r1E = %20.10f %20.10f\n", cy_r1E->re,cy_r1E->im);
		printf("cy_i00 = %20.10f %20.10f\n", cy_i00->re,cy_i00->im);
		printf("cy_i02 = %20.10f %20.10f\n", cy_i02->re,cy_i02->im);
		printf("cy_i04 = %20.10f %20.10f\n", cy_i04->re,cy_i04->im);
		printf("cy_i06 = %20.10f %20.10f\n", cy_i06->re,cy_i06->im);
		printf("cy_i08 = %20.10f %20.10f\n", cy_i08->re,cy_i08->im);
		printf("cy_i0A = %20.10f %20.10f\n", cy_i0A->re,cy_i0A->im);
		printf("cy_i0C = %20.10f %20.10f\n", cy_i0C->re,cy_i0C->im);
		printf("cy_i0E = %20.10f %20.10f\n", cy_i0E->re,cy_i0E->im);
		printf("cy_i10 = %20.10f %20.10f\n", cy_i10->re,cy_i10->im);
		printf("cy_i12 = %20.10f %20.10f\n", cy_i12->re,cy_i12->im);
		printf("cy_i14 = %20.10f %20.10f\n", cy_i14->re,cy_i14->im);
		printf("cy_i16 = %20.10f %20.10f\n", cy_i16->re,cy_i16->im);
		printf("cy_i18 = %20.10f %20.10f\n", cy_i18->re,cy_i18->im);
		printf("cy_i1A = %20.10f %20.10f\n", cy_i1A->re,cy_i1A->im);
		printf("cy_i1C = %20.10f %20.10f\n", cy_i1C->re,cy_i1C->im);
		printf("cy_i1E = %20.10f %20.10f\n", cy_i1E->re,cy_i1E->im);
		ASSERT(HERE, 0, "Nonzero exit carry in thread-function!");
	}
#endif
		return 0x0;
	}
#endif

