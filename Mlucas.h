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

/*******************************************************************************
   We now include this header file if it was not included before.
*******************************************************************************/
#ifndef Mlucas_h_included
#define Mlucas_h_included

#include "align.h"
#include "carry.h"
#include "dft_macro.h"
#include "factor.h"
#include "prefetch.h"
#include "util.h"

#ifdef INCLUDE_PM1
	#include "gcd_lehmer.h"
#endif

/*******************************************************************************
   Function prototypes. The corresponding function definitions will either
   be in a {function name}.c file or (for cases where a .c file contains
   multiple function definitions) in the given .c file:
*******************************************************************************/

/* Mlucas.c: */
void	Mlucas_init(void);
uint32	ernstMain
(
	int		mod_type,
	int		test_type,
	uint64	exponent,
	uint32	fft_length,
	int		radix_set,
	uint32	maxFFT,
	uint32	iterations,	/* Use to store log2[max factor depth] in TF mode */
	int		error_checking,
	uint64	*sh0,	/* Reference/Computed mod-(2^64) residue */
	uint64	*sh1,	/* Reference/Computed Selfridge/Hurwitz Residue mod 2^35-1 */
	uint64	*sh2,	/* Reference/Computed Selfridge/Hurwitz Residue mod 2^36-1 */
	int		scrnFlag,
	double	*runtime
);

int		cfgNeedsUpdating(char*in_line);
void	printMlucasErrCode(int retVal);
uint64	res64	(double a[], int n, const uint64 p, int *nbits, char *hex_res);
void	resSH	(double a[], int n, const uint64 p, uint64*Res35m1, uint64*Res36m1);
void	hex_res_printtofile(double a[], int n, const uint64 p, int timing_test_iters, FILE *fp);
int		convert_LL_savefiles(uint64 psave, FILE*fp, uint32*ilo, uint32 ndim, int32 arr_scratch[], double a[]);
int		read_ppm1_savefiles	(uint64 p, FILE*fp, uint32*ilo, uint8 arr_tmp[], uint64*Res64, uint64*Res35m1, uint64*Res36m1);
/* Moved this prot to Mlucas.c:
void	write_ppm1_savefiles(uint64 p, FILE*fp, uint32 ihi, uint8 arr_tmp[], uint64 Res64, uint64 Res35m1, uint64 Res36m1);
*/
int		convert_res_bytewise_FP(const uint8 arr_tmp[], double a[], int n, const uint64 p, const uint64 Res64, const uint64 Res35m1, const uint64 Res36m1);
void	convert_res_FP_bytewise(const double a[], uint8 arr_tmp[], int n, const uint64 p,       uint64*Res64,       uint64*Res35m1,       uint64*Res36m1);
uint32	get_default_factoring_depth(uint64 p);
void	write_fft_debug_data(double a[], int jlo, int jhi);

/* br.c: */
void	print_pow2_twiddles(const uint32 n, const uint32 p, const uint32 q);
void	bit_reverse_int(int vec[], int n, int nradices, int radix[], int incr, int*scratch);

/* get_fft_radices.c: */
int		get_fft_radices			(uint32 kblocks, int radix_set, int *nradices, int radix_vec[], int radix_vec_dim);
void	test_fft_radixtables	(void);
uint32	get_default_fft_length	(uint32 p);
uint32	get_nextlarger_fft_length	(uint32 n);
uint32	given_N_get_maxP		(uint32 N);

/* get_preferred_fft_radix.c: */
uint32	get_preferred_fft_radix(uint32 kblocks);
uint32	extractFFTlengthFrom32Bit (uint32 n);
void	extractFFTradicesFrom32Bit(uint32 n);

/* radix*_ditN_cy_dif1.c: */
void	radix5_dif_pass1	(double a[], int n);
void	radix6_dif_pass1	(double a[], int n);
void	radix6_dif_pass1A	(double a[], int n);
void	radix6_dif_pass1B	(double a[], int n);
void	radix7_dif_pass1	(double a[], int n);
void	radix8_dif_pass1	(double a[], int n);
void	radix9_dif_pass1	(double a[], int n);
void	radix10_dif_pass1	(double a[], int n);
void	radix11_dif_pass1	(double a[], int n);
void	radix12_dif_pass1	(double a[], int n);
void	radix13_dif_pass1	(double a[], int n);
void	radix14_dif_pass1	(double a[], int n);
void	radix15_dif_pass1	(double a[], int n);
void	radix16_dif_pass1	(double a[], int n);
void	radix18_dif_pass1	(double a[], int n);
void	radix20_dif_pass1	(double a[], int n);
void	radix22_dif_pass1	(double a[], int n);
void	radix24_dif_pass1	(double a[], int n);
void	radix26_dif_pass1	(double a[], int n);
void	radix28_dif_pass1	(double a[], int n);
void	radix30_dif_pass1	(double a[], int n);
void	radix31_dif_pass1	(double a[], int n);
void	radix32_dif_pass1	(double a[], int n);
void	radix36_dif_pass1	(double a[], int n);
void	radix40_dif_pass1	(double a[], int n);
void	radix44_dif_pass1	(double a[], int n);
void	radix48_dif_pass1	(double a[], int n);
void	radix52_dif_pass1	(double a[], int n);
void	radix56_dif_pass1	(double a[], int n);
void	radix60_dif_pass1	(double a[], int n);
void	radix63_dif_pass1	(double a[], int n);
void	radix64_dif_pass1	(double a[], int n);
void	radix72_dif_pass1	(double a[], int n);
void	radix80_dif_pass1	(double a[], int n);
void	radix88_dif_pass1 	(double a[], int n);
void	radix96_dif_pass1 	(double a[], int n);
void	radix104_dif_pass1	(double a[], int n);
void	radix112_dif_pass1	(double a[], int n);
void	radix120_dif_pass1	(double a[], int n);
void	radix128_dif_pass1	(double a[], int n);
void	radix144_dif_pass1	(double a[], int n);
void	radix160_dif_pass1	(double a[], int n);
void	radix176_dif_pass1	(double a[], int n);
void	radix192_dif_pass1	(double a[], int n);
void	radix208_dif_pass1	(double a[], int n);
void	radix224_dif_pass1	(double a[], int n);
void	radix240_dif_pass1	(double a[], int n);
void	radix256_dif_pass1	(double a[], int n);
void	radix288_dif_pass1	(double a[], int n);
void	radix320_dif_pass1	(double a[], int n);
void	radix352_dif_pass1	(double a[], int n);
void	radix384_dif_pass1	(double a[], int n);
void	radix416_dif_pass1	(double a[], int n);
void	radix448_dif_pass1	(double a[], int n);
void	radix480_dif_pass1	(double a[], int n);
void	radix512_dif_pass1	(double a[], int n);
void	radix576_dif_pass1	(double a[], int n);
void	radix640_dif_pass1	(double a[], int n);
void	radix704_dif_pass1	(double a[], int n);
void	radix768_dif_pass1	(double a[], int n);
void	radix832_dif_pass1	(double a[], int n);
void	radix896_dif_pass1	(double a[], int n);
void	radix960_dif_pass1	(double a[], int n);
void	radix992_dif_pass1	(double a[], int n);
void	radix1008_dif_pass1 (double a[], int n);
void	radix1024_dif_pass1	(double a[], int n);
void	radix4032_dif_pass1 (double a[], int n);
void	radix4096_dif_pass1	(double a[], int n);

void	radix5_dit_pass1	(double a[], int n);
void	radix6_dit_pass1	(double a[], int n);
void	radix7_dit_pass1	(double a[], int n);
void	radix8_dit_pass1	(double a[], int n);
void	radix9_dit_pass1	(double a[], int n);
void	radix10_dit_pass1	(double a[], int n);
void	radix11_dit_pass1	(double a[], int n);
void	radix12_dit_pass1	(double a[], int n);
void	radix13_dit_pass1	(double a[], int n);
void	radix13_dit_pass1A	(double a[], int n);
void	radix13_dit_pass1B	(double a[], int n);
void	radix14_dit_pass1	(double a[], int n);
void	radix15_dit_pass1	(double a[], int n);
void	radix16_dit_pass1	(double a[], int n);
void	radix18_dit_pass1	(double a[], int n);
void	radix20_dit_pass1	(double a[], int n);
void	radix22_dit_pass1	(double a[], int n);
void	radix24_dit_pass1	(double a[], int n);
void	radix26_dit_pass1	(double a[], int n);
void	radix28_dit_pass1	(double a[], int n);
void	radix30_dit_pass1	(double a[], int n);
void	radix31_dit_pass1	(double a[], int n);
void	radix32_dit_pass1	(double a[], int n);
void	radix36_dit_pass1	(double a[], int n);
void	radix40_dit_pass1	(double a[], int n);
void	radix44_dit_pass1	(double a[], int n);
void	radix48_dit_pass1	(double a[], int n);
void	radix52_dit_pass1	(double a[], int n);
void	radix56_dit_pass1	(double a[], int n);
void	radix60_dit_pass1	(double a[], int n);
void	radix63_dit_pass1	(double a[], int n);
void	radix64_dit_pass1	(double a[], int n);
void	radix72_dit_pass1	(double a[], int n);
void	radix80_dit_pass1	(double a[], int n);
void	radix88_dit_pass1 	(double a[], int n);
void	radix96_dit_pass1 	(double a[], int n);
void	radix104_dit_pass1	(double a[], int n);
void	radix112_dit_pass1	(double a[], int n);
void	radix120_dit_pass1	(double a[], int n);
void	radix128_dit_pass1	(double a[], int n);
void	radix144_dit_pass1	(double a[], int n);
void	radix160_dit_pass1	(double a[], int n);
void	radix176_dit_pass1	(double a[], int n);
void	radix192_dit_pass1	(double a[], int n);
void	radix208_dit_pass1	(double a[], int n);
void	radix224_dit_pass1	(double a[], int n);
void	radix240_dit_pass1	(double a[], int n);
void	radix256_dit_pass1	(double a[], int n);
void	radix288_dit_pass1	(double a[], int n);
void	radix320_dit_pass1	(double a[], int n);
void	radix352_dit_pass1	(double a[], int n);
void	radix384_dit_pass1	(double a[], int n);
void	radix416_dit_pass1	(double a[], int n);
void	radix448_dit_pass1	(double a[], int n);
void	radix480_dit_pass1	(double a[], int n);
void	radix512_dit_pass1	(double a[], int n);
void	radix576_dit_pass1	(double a[], int n);
void	radix640_dit_pass1	(double a[], int n);
void	radix704_dit_pass1	(double a[], int n);
void	radix768_dit_pass1	(double a[], int n);
void	radix832_dit_pass1	(double a[], int n);
void	radix896_dit_pass1	(double a[], int n);
void	radix960_dit_pass1	(double a[], int n);
void	radix992_dit_pass1	(double a[], int n);
void	radix1008_dit_pass1 (double a[], int n);
void	radix1024_dit_pass1	(double a[], int n);
void	radix4032_dit_pass1 (double a[], int n);
void	radix4096_dit_pass1	(double a[], int n);

void	radix8_dif_pass		(double a[], int n, struct complex rt0[], struct complex rt1[], int index[], int nloops, int incr, int init_sse2, int thr_id);
void	radix16_dif_pass	(double a[], int n, struct complex rt0[], struct complex rt1[], int index[], int nloops, int incr, int init_sse2, int thr_id);
void	radix32_dif_pass	(double a[], int n, struct complex rt0[], struct complex rt1[], int index[], int nloops, int incr, int init_sse2, int thr_id);
void	radix64_dif_pass	(double a[], int n, struct complex rt0[], struct complex rt1[], int index[], int nloops, int incr, int init_sse2, int thr_id);

void	radix8_dit_pass		(double a[], int n, struct complex rt0[], struct complex rt1[], int index[], int nloops, int incr, int init_sse2, int thr_id);
void	radix16_dit_pass	(double a[], int n, struct complex rt0[], struct complex rt1[], int index[], int nloops, int incr, int init_sse2, int thr_id);
void	radix32_dit_pass	(double a[], int n, struct complex rt0[], struct complex rt1[], int index[], int nloops, int incr, int init_sse2, int thr_id);
void	radix64_dit_pass	(double a[], int n, struct complex rt0[], struct complex rt1[], int index[], int nloops, int incr, int init_sse2, int thr_id);

/* mers_mod_square.c: */
int		mers_mod_square		(double a[], int arr_scratch[], int n, int ilo, int ihi, uint64 p, uint32 *err_iter, int scrnFlag, double *tdiff);

	/* radix{16|32|64}_wrapper_square.c: */
	void	pair_square  (double *x1, double *y1, double *x2, double *y2, double c, double s);
	void	pair_square2A(double *x1, double *y1, double *x2, double *y2, double *x3, double *y3, double *x4, double *y4, double c, double s);
	void	pair_square2B(double *x1, double *y1, double *x2, double *y2, double *x3, double *y3, double *x4, double *y4, double c, double s);

	void	radix16_wrapper_ini		(int n, int radix0, int iblock, int nradices_prim, int radix_prim[], int i[], int j1[], int j2[], int j2_start[], int k[], int m[], int blocklen[], int blocklen_sum[]);
	void	radix16_wrapper_square	(double a[], int arr_scratch[], int n, int radix0, struct complex rt0[], struct complex rt1[], int nradices_prim, int radix_prim[], int ws_i, int ws_j1, int ws_j2, int ws_j2_start, int ws_k, int ws_m, int ws_blocklen, int ws_blocklen_sum, int init_sse2, int thr_id);

	void	radix32_wrapper_ini		(int n, int radix0, int iblock, int nradices_prim, int radix_prim[], int i[], int j1[], int j2[], int j2_start[], int k[], int m[], int blocklen[], int blocklen_sum[]);
	void	radix32_wrapper_square	(double a[], int arr_scratch[], int n, int radix0, struct complex rt0[], struct complex rt1[], int nradices_prim, int radix_prim[], int ws_i, int ws_j1, int ws_j2, int ws_j2_start, int ws_k, int ws_m, int ws_blocklen, int ws_blocklen_sum, int init_sse2, int thr_id);

	void	radix64_wrapper_ini		(int n, int radix0, int iblock, int nradices_prim, int radix_prim[], int i[], int j1[], int j2[], int j2_start[], int k[], int m[], int blocklen[], int blocklen_sum[]);
	void	radix64_wrapper_square	(double a[], int arr_scratch[], int n, int radix0, struct complex rt0[], struct complex rt1[], int nradices_prim, int radix_prim[], int ws_i, int ws_j1, int ws_j2, int ws_j2_start, int ws_k, int ws_m, int ws_blocklen, int ws_blocklen_sum, int init_sse2, int thr_id);

/* fermat_mod_square.c: */
int 	fermat_mod_square	(double a[], int arr_scratch[], int n, int ilo, int ihi, uint64 p, uint32 *err_iter, int scrnFlag, double *tdiff);

#ifdef MULTITHREAD
	/* Multithreaded version must be in form of 1-arg functor */
	void *mers_process_chunk  (void*targ);
	void *fermat_process_chunk(void*targ);
	// These are shared by both mers and fermat-mod, although the code contains switches to invoke the corr. carry macros:
	void *cy16_process_chunk(void*targ);
	void *cy20_process_chunk(void*targ);
	void *cy24_process_chunk(void*targ);
	void *cy28_process_chunk(void*targ);
	void *cy32_process_chunk(void*targ);
	void *cy36_process_chunk(void*targ);
	void *cy40_process_chunk(void*targ);
	void *cy44_process_chunk(void*targ);
	void *cy48_process_chunk(void*targ);
	void *cy52_process_chunk(void*targ);
	void *cy56_process_chunk(void*targ);
	void *cy60_process_chunk(void*targ);
	void *cy63_process_chunk(void*targ);
	void *cy64_process_chunk(void*targ);
	void *cy72_process_chunk(void*targ);
	void *cy80_process_chunk(void*targ);
	void *cy88_process_chunk(void*targ);
	void *cy96_process_chunk(void*targ);
	void *cy104_process_chunk(void*targ);
	void *cy112_process_chunk(void*targ);
	void *cy120_process_chunk(void*targ);
	void *cy128_process_chunk(void*targ);
	void *cy144_process_chunk(void*targ);
	void *cy160_process_chunk(void*targ);
	void *cy176_process_chunk(void*targ);
	void *cy192_process_chunk(void*targ);
	void *cy208_process_chunk(void*targ);
	void *cy224_process_chunk(void*targ);
	void *cy240_process_chunk(void*targ);
	void *cy256_process_chunk(void*targ);
	void *cy288_process_chunk(void*targ);
	void *cy320_process_chunk(void*targ);
	void *cy352_process_chunk(void*targ);
	void *cy384_process_chunk(void*targ);
	void *cy416_process_chunk(void*targ);
	void *cy448_process_chunk(void*targ);
	void *cy480_process_chunk(void*targ);
	void *cy512_process_chunk(void*targ);
	void *cy576_process_chunk(void*targ);
	void *cy640_process_chunk(void*targ);
	void *cy704_process_chunk(void*targ);
	void *cy768_process_chunk(void*targ);
	void *cy832_process_chunk(void*targ);
	void *cy896_process_chunk(void*targ);
	void *cy960_process_chunk(void*targ);
	void *cy992_process_chunk(void*targ);
	void *cy1008_process_chunk(void*targ);
	void *cy1024_process_chunk(void*targ);
	void *cy4032_process_chunk(void*targ);
	void *cy4096_process_chunk(void*targ);
#else
	void mers_process_chunk  (double a[], int arr_scratch[], int n, struct complex rt0[], struct complex rt1[], int index[], int block_index[], int ii, int nradices_prim, int radix_prim[], int ws_i[], int ws_j1[], int ws_j2[], int ws_j2_start[], int ws_k[], int ws_m[], int ws_blocklen[], int ws_blocklen_sum[]);
	void fermat_process_chunk(double a[], int arr_scratch[], int n, struct complex rt0[], struct complex rt1[], int index[],                    int ii, int nradices_prim, int radix_prim[]);
#endif

	/* radix{16|32|64}_dyadic_square.c: */
	void	radix16_dyadic_square	(double a[], int arr_scratch[], int n, int radix0, struct complex rt0[], struct complex rt1[], int ii, int nradices_prim, int radix_prim[], int incr, int init_sse2, int thr_id);
	void	radix32_dyadic_square	(double a[], int arr_scratch[], int n, int radix0, struct complex rt0[], struct complex rt1[], int ii, int nradices_prim, int radix_prim[], int incr, int init_sse2, int thr_id);
	void	radix64_dyadic_square	(double a[], int arr_scratch[], int n, int radix0, struct complex rt0[], struct complex rt1[], int ii, int nradices_prim, int radix_prim[], int incr, int init_sse2, int thr_id);

/* Each of the final-DIT-pass/propagate-carries/initial-DIF-pass routines comes in both an error-checking and non-error-checking
version, unless it's one of the later SSE2-only radices, in which at least partial error checking is mandatory:
*/
	int	radix5_ditN_cy_dif1 		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix6_ditN_cy_dif1 		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int radix7_ditN_cy_dif1 		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int radix8_ditN_cy_dif1 		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix9_ditN_cy_dif1 		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix10_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix11_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix12_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix13_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix14_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix15_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int radix16_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix18_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix20_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix22_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix24_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix26_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix28_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix30_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix31_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix32_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix36_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix40_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix44_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix48_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix52_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix56_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix60_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix63_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix64_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix72_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix80_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix88_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix96_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix104_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix112_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix120_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix128_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix144_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix160_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix176_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix192_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix208_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix224_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix240_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix256_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix288_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix320_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix352_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix384_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix416_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix448_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix480_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix512_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix576_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix640_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix704_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix768_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix832_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix896_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix960_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix992_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix1008_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix1024_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix4032_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter, double *fracmax, uint64 p);
	int	radix4096_ditN_cy_dif1		(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter, double *fracmax, uint64 p);

	/* Only define 2nd version of carry routine[s] with ROE checking disabled in non-SSE2 mode, as SSE2 ROE checking is cheap.
	For large radices (> 36) also define just a single (ROE-enabled) version of the carry routine and use it irrespective of the command-line ROE-check flag value: */
  #ifndef USE_SSE2
	int	radix5_ditN_cy_dif1_nochk 	(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter,                  uint64 p);
	int	radix6_ditN_cy_dif1_nochk 	(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter,                  uint64 p);
	int	radix7_ditN_cy_dif1_nochk 	(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter,                  uint64 p);
	int radix8_ditN_cy_dif1_nochk 	(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter,                  uint64 p);
	int	radix9_ditN_cy_dif1_nochk 	(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter,                  uint64 p);
	int	radix10_ditN_cy_dif1_nochk	(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter,                  uint64 p);
	int	radix11_ditN_cy_dif1_nochk	(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter,                  uint64 p);
	int	radix12_ditN_cy_dif1_nochk	(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter,                  uint64 p);
	int	radix13_ditN_cy_dif1_nochk	(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter,                  uint64 p);
	int	radix14_ditN_cy_dif1_nochk	(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter,                  uint64 p);
	int	radix15_ditN_cy_dif1_nochk	(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter,                  uint64 p);
	int radix16_ditN_cy_dif1_nochk	(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter,                  uint64 p);
	int	radix18_ditN_cy_dif1_nochk	(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter,                  uint64 p);
	int	radix20_ditN_cy_dif1_nochk	(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter,                  uint64 p);
	int	radix22_ditN_cy_dif1_nochk	(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter,                  uint64 p);
	int	radix24_ditN_cy_dif1_nochk	(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter,                  uint64 p);
	int	radix26_ditN_cy_dif1_nochk	(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[],                                             double base[], double baseinv[], int iter,                  uint64 p);
	int	radix28_ditN_cy_dif1_nochk	(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter,                  uint64 p);
	int	radix30_ditN_cy_dif1_nochk	(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter,                  uint64 p);
	int radix32_ditN_cy_dif1_nochk	(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter,                  uint64 p);
	int	radix36_ditN_cy_dif1_nochk	(double a[], int n, int nwt, int nwt_bits, double wt0[], double wt1[], int si[], struct complex rn0[], struct complex rn1[], double base[], double baseinv[], int iter,                  uint64 p);
  #endif

#endif	/* Mlucas_h_included */
