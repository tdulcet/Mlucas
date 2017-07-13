/*******************************************************************************
*                                                                              *
*   (C) 1997-2016 by Ernst W. Mayer.                                           *
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
#ifndef radix11_sse_macro_h_included
#define radix11_sse_macro_h_included

#include "sse2_macro.h"

#ifdef USE_AVX512	// AVX512 version, based on RADIX_11_DFT_FMA in dft_macro.h

	// FMAs used for all arithmetic, including 'trivial' ones (one mult = 1.0) to replace ADD/SUB:
	//
	// Arithmetic opcount: [10 ADD, 140 FMA (20 trivial, incl 10 MUL), 166 memref], close to general target of 1 memref per vec_dbl arithmetic op.
	// potentially much lower cycle count (at a max theoretical rate of 2 FMA/cycle) than non-FMA version.
	//
	// Compare to non-FMA: [182 ADD, 44 MUL), 154 memref], less accurate and bottlenecked by 1 ADD/cycle max issue rate.
	//
	#define SSE2_RADIX_11_DFT(XI0,Xi1,Xi2,Xi3,Xi4,Xi5,Xi6,Xi7,Xi8,Xi9,XiA, Xcc, XO0,Xo1,Xo2,Xo3,Xo4,Xo5,Xo6,Xo7,Xo8,Xo9,XoA)\
	{\
	__asm__ volatile (\
		"movq	%[__cc],%%r15	\n\t	leaq	-0x80(%%r15),%%r8	\n\t	leaq	-0x40(%%r15),%%r9	\n\t"/* cc1,two,one */\
		"movq	%[__i1],%%rax			\n\t"/* &A1 */"		movq	%[__i6],%%rdi		\n\t"/* &A6 */\
		"movq	%[__i2],%%rbx			\n\t"/* &A2 */"		movq	%[__i7],%%r10		\n\t"/* &A7 */\
		"movq	%[__i3],%%rcx			\n\t"/* &A3 */"		movq	%[__i8],%%r11		\n\t"/* &A8 */\
		"movq	%[__i4],%%rdx			\n\t"/* &A4 */"		movq	%[__i9],%%r12		\n\t"/* &A9 */\
		"movq	%[__i5],%%rsi			\n\t"/* &A5 */"		movq	%[__iA],%%r13		\n\t"/* &AA */\
	\
		"vmovaps	(%%rax),%%zmm1		\n\t"/* A1r */"		vmovaps	0x40(%%rax),%%zmm6 	\n\t"/* A1i */\
		"vmovaps	(%%rbx),%%zmm2		\n\t"/* A2r */"		vmovaps	0x40(%%rbx),%%zmm7 	\n\t"/* A2i */\
		"vmovaps	(%%rcx),%%zmm3		\n\t"/* A3r */"		vmovaps	0x40(%%rcx),%%zmm8 	\n\t"/* A3i */\
		"vmovaps	(%%rdx),%%zmm4		\n\t"/* A4r */"		vmovaps	0x40(%%rdx),%%zmm9 	\n\t"/* A4i */\
		"vmovaps	(%%rsi),%%zmm5		\n\t"/* A5r */"		vmovaps	0x40(%%rsi),%%zmm10	\n\t"/* A5i */\
		/* Have enough registers to also hold the real-upper-half inputs: */\
		"vmovaps	(%%rdi),%%zmm11		\n\t"/* A6r */\
		"vmovaps	(%%r10),%%zmm12		\n\t"/* A7r */\
		"vmovaps	(%%r11),%%zmm13		\n\t"/* A8r */\
		"vmovaps	(%%r12),%%zmm14		\n\t"/* A9r */\
		"vmovaps	(%%r13),%%zmm15		\n\t"/* AAr */\
	\
	/* ...Do the + parts of the opening wave of radix-2 butterflies: */\
		"vmovaps	(%%r9),%%zmm0		\n\t"/* one */\
		" vfmadd231pd %%zmm15,%%zmm0,%%zmm1		\n\t	 vfmadd231pd 0x40(%%r13),%%zmm0,%%zmm6 	\n\t"/* t1 = A1 + Aa */\
		" vfmadd231pd %%zmm14,%%zmm0,%%zmm2		\n\t	 vfmadd231pd 0x40(%%r12),%%zmm0,%%zmm7 	\n\t"/* t2 = A2 + A9 */\
		" vfmadd231pd %%zmm13,%%zmm0,%%zmm3		\n\t	 vfmadd231pd 0x40(%%r11),%%zmm0,%%zmm8 	\n\t"/* t3 = A3 + A8 */\
		" vfmadd231pd %%zmm12,%%zmm0,%%zmm4		\n\t	 vfmadd231pd 0x40(%%r10),%%zmm0,%%zmm9 	\n\t"/* t4 = A4 + A7 */\
		" vfmadd231pd %%zmm11,%%zmm0,%%zmm5		\n\t	 vfmadd231pd 0x40(%%rdi),%%zmm0,%%zmm10	\n\t"/* t5 = A5 + A6 */\
	/* Write lower-half outputs back to memory... */\
		"vmovaps	%%zmm1,(%%rax)		\n\t"/* t1r */"		vmovaps	%%zmm6 ,0x40(%%rax)	\n\t"/* t1i */\
		"vmovaps	%%zmm2,(%%rbx)		\n\t"/* t2r */"		vmovaps	%%zmm7 ,0x40(%%rbx)	\n\t"/* t2i */\
		"vmovaps	%%zmm3,(%%rcx)		\n\t"/* t3r */"		vmovaps	%%zmm8 ,0x40(%%rcx)	\n\t"/* t3i */\
		"vmovaps	%%zmm4,(%%rdx)		\n\t"/* t4r */"		vmovaps	%%zmm9 ,0x40(%%rdx)	\n\t"/* t4i */\
		"vmovaps	%%zmm5,(%%rsi)		\n\t"/* t5r */"		vmovaps	%%zmm10,0x40(%%rsi)	\n\t"/* t5i */\
	/* ...Do the - parts of the radix-2 butterflies: */\
		"vmovaps	(%%r8),%%zmm0		\n\t"/* two */\
		"vfnmadd231pd %%zmm15,%%zmm0,%%zmm1		\n\t	vfnmadd231pd 0x40(%%r13),%%zmm0,%%zmm6 	\n\t"/* ta = A1 - Aa */\
		"vfnmadd231pd %%zmm14,%%zmm0,%%zmm2		\n\t	vfnmadd231pd 0x40(%%r12),%%zmm0,%%zmm7 	\n\t"/* t9 = A2 - A9 */\
		"vfnmadd231pd %%zmm13,%%zmm0,%%zmm3		\n\t	vfnmadd231pd 0x40(%%r11),%%zmm0,%%zmm8 	\n\t"/* t8 = A3 - A8 */\
		"vfnmadd231pd %%zmm12,%%zmm0,%%zmm4		\n\t	vfnmadd231pd 0x40(%%r10),%%zmm0,%%zmm9 	\n\t"/* t7 = A4 - A7 */\
		"vfnmadd231pd %%zmm11,%%zmm0,%%zmm5		\n\t	vfnmadd231pd 0x40(%%rdi),%%zmm0,%%zmm10	\n\t"/* t6 = A5 - A6 */\
	/* ...And write the upper-half outputs back to memory to free up registers for the 5x5 sine-term-subconvo computation: */\
		"vmovaps	%%zmm1,(%%r13)		\n\t"/* tar */"		vmovaps	%%zmm6 ,0x40(%%r13)	\n\t"/* tai */\
		"vmovaps	%%zmm2,(%%r12)		\n\t"/* t9r */"		vmovaps	%%zmm7 ,0x40(%%r12)	\n\t"/* t9i */\
		"vmovaps	%%zmm3,(%%r11)		\n\t"/* t8r */"		vmovaps	%%zmm8 ,0x40(%%r11)	\n\t"/* t8i */\
		"vmovaps	%%zmm4,(%%r10)		\n\t"/* t7r */"		vmovaps	%%zmm9 ,0x40(%%r10)	\n\t"/* t7i */\
		"vmovaps	%%zmm5,(%%rdi)		\n\t"/* t6r */"		vmovaps	%%zmm10,0x40(%%rdi)	\n\t"/* t6i */\
/*
	Sr1 =      ss1*tar+ss2*t9r+ss3*t8r+ss4*t7r+ss5*t6r;		Si1 =      ss1*tai+ss2*t9i+ss3*t8i+ss4*t7i+ss5*t6i;	// S1
	Sr2 =      ss2*tar+ss4*t9r-ss5*t8r-ss3*t7r-ss1*t6r;		Si2 =      ss2*tai+ss4*t9i-ss5*t8i-ss3*t7i-ss1*t6i;	// S2
	Sr3 =      ss3*tar-ss5*t9r-ss2*t8r+ss1*t7r+ss4*t6r;		Si3 =      ss3*tai-ss5*t9i-ss2*t8i+ss1*t7i+ss4*t6i;	// S3
	Sr4 =      ss4*tar-ss3*t9r+ss1*t8r+ss5*t7r-ss2*t6r;		Si4 =      ss4*tai-ss3*t9i+ss1*t8i+ss5*t7i-ss2*t6i;	// S4
	Sr5 =      ss5*tar-ss1*t9r+ss4*t8r-ss2*t7r+ss3*t6r;		Si5 =      ss5*tai-ss1*t9i+ss4*t8i-ss2*t7i+ss3*t6i;	// S5

	In a 16-reg FMA model, can keep the 10 S-terms and all 5 shared sincos data in registers, but that
	requires us to reload one of the 2 repeated t*-mults (e.g. tai in the 1st block) 5 times per block.
	OTOH if we keep both t*r,i-mults and 4 of the 5 ss-mults in-reg, we just need 2 loads-from-mem per block:
*/\
		"vmovaps	(%%r13),%%zmm1		\n\t	vmovaps	0x40(%%r13),%%zmm6	\n\t"/* S1 = ta */\
		"vmovaps	%%zmm1,%%zmm2		\n\t	vmovaps	%%zmm6 ,%%zmm7 		\n\t"/* S2 = ta */\
		"vmovaps	%%zmm1,%%zmm3		\n\t	vmovaps	%%zmm6 ,%%zmm8 		\n\t"/* S3 = ta */\
		"vmovaps	%%zmm1,%%zmm4		\n\t	vmovaps	%%zmm6 ,%%zmm9 		\n\t"/* S4 = ta */\
		"vmovaps	%%zmm1,%%zmm5		\n\t	vmovaps	%%zmm6 ,%%zmm10		\n\t"/* S5 = ta */\
	"addq	$0x140,%%r15	\n\t"/* Incr trig ptr to point to ss1 */\
		"vmovaps	0x40(%%r15),%%zmm11	\n\t	vmovaps	0x80(%%r15),%%zmm12		\n\t"/* ss2,ss3 */\
		"vmovaps	0xc0(%%r15),%%zmm13	\n\t	vmovaps	0x100(%%r15),%%zmm14		\n\t"/* ss4,ss5 */\
\
	"	vmulpd	(%%r15),%%zmm1,%%zmm1	\n\t	vmulpd	(%%r15),%%zmm6 ,%%zmm6 	\n\t"/* S1  = ss1*ta; */\
	"	vmulpd	%%zmm11,%%zmm2,%%zmm2	\n\t	vmulpd	%%zmm11,%%zmm7 ,%%zmm7 	\n\t"/* S2  = ss2*ta; */\
	"	vmulpd	%%zmm12,%%zmm3,%%zmm3	\n\t	vmulpd	%%zmm12,%%zmm8 ,%%zmm8 	\n\t"/* S3  = ss3*ta; */\
	"	vmulpd	%%zmm13,%%zmm4,%%zmm4	\n\t	vmulpd	%%zmm13,%%zmm9 ,%%zmm9 	\n\t"/* S4  = ss4*ta; */\
	"	vmulpd	%%zmm14,%%zmm5,%%zmm5	\n\t	vmulpd	%%zmm14,%%zmm10,%%zmm10	\n\t"/* S5  = ss5*ta; */\
\
		"vmovaps	    (%%r12),%%zmm0	\n\t	vmovaps	0x40(%%r12),%%zmm15	\n\t"/* t9 */\
	" vfmadd231pd %%zmm11,%%zmm0,%%zmm1	\n\t  vfmadd231pd %%zmm11,%%zmm15,%%zmm6 	\n\t"/* S1 += ss2*t9; */\
	" vfmadd231pd %%zmm13,%%zmm0,%%zmm2	\n\t  vfmadd231pd %%zmm13,%%zmm15,%%zmm7 	\n\t"/* S2 += ss4*t9; */\
	"vfnmadd231pd %%zmm14,%%zmm0,%%zmm3	\n\t vfnmadd231pd %%zmm14,%%zmm15,%%zmm8 	\n\t"/* S3 -= ss5*t9; */\
	"vfnmadd231pd %%zmm12,%%zmm0,%%zmm4	\n\t vfnmadd231pd %%zmm12,%%zmm15,%%zmm9 	\n\t"/* S4 -= ss3*t9; */\
	"vfnmadd231pd (%%r15),%%zmm0,%%zmm5	\n\t vfnmadd231pd (%%r15),%%zmm15,%%zmm10	\n\t"/* S5 -= ss1*t9; */\
\
		"vmovaps	    (%%r11),%%zmm0	\n\t	vmovaps	0x40(%%r11),%%zmm15	\n\t"/* t8 */\
	" vfmadd231pd %%zmm12,%%zmm0,%%zmm1	\n\t  vfmadd231pd %%zmm12,%%zmm15,%%zmm6 	\n\t"/* S1 += ss3*t8; */\
	"vfnmadd231pd %%zmm14,%%zmm0,%%zmm2	\n\t vfnmadd231pd %%zmm14,%%zmm15,%%zmm7 	\n\t"/* S2 -= ss5*t8; */\
	"vfnmadd231pd %%zmm11,%%zmm0,%%zmm3	\n\t vfnmadd231pd %%zmm11,%%zmm15,%%zmm8 	\n\t"/* S3 -= ss2*t8; */\
	" vfmadd231pd (%%r15),%%zmm0,%%zmm4	\n\t  vfmadd231pd (%%r15),%%zmm15,%%zmm9 	\n\t"/* S4 += ss1*t8; */\
	" vfmadd231pd %%zmm13,%%zmm0,%%zmm5	\n\t  vfmadd231pd %%zmm13,%%zmm15,%%zmm10	\n\t"/* S5 += ss4*t8; */\
\
		"vmovaps	    (%%r10),%%zmm0	\n\t	vmovaps	0x40(%%r10),%%zmm15	\n\t"/* t7 */\
	" vfmadd231pd %%zmm13,%%zmm0,%%zmm1	\n\t  vfmadd231pd %%zmm13,%%zmm15,%%zmm6 	\n\t"/* S1 += ss4*t7; */\
	"vfnmadd231pd %%zmm12,%%zmm0,%%zmm2	\n\t vfnmadd231pd %%zmm12,%%zmm15,%%zmm7 	\n\t"/* S2 -= ss3*t7; */\
	" vfmadd231pd (%%r15),%%zmm0,%%zmm3	\n\t  vfmadd231pd (%%r15),%%zmm15,%%zmm8 	\n\t"/* S3 += ss1*t7; */\
	" vfmadd231pd %%zmm14,%%zmm0,%%zmm4	\n\t  vfmadd231pd %%zmm14,%%zmm15,%%zmm9 	\n\t"/* S4 += ss5*t7; */\
	"vfnmadd231pd %%zmm11,%%zmm0,%%zmm5	\n\t vfnmadd231pd %%zmm11,%%zmm15,%%zmm10	\n\t"/* S5 -= ss2*t7; */\
\
		"vmovaps	    (%%rdi),%%zmm0	\n\t	vmovaps	0x40(%%rdi),%%zmm15	\n\t"/* t6 */\
	" vfmadd231pd %%zmm14,%%zmm0,%%zmm1	\n\t  vfmadd231pd %%zmm14,%%zmm15,%%zmm6 	\n\t"/* S1 += ss5*t6; */\
	"vfnmadd231pd (%%r15),%%zmm0,%%zmm2	\n\t vfnmadd231pd (%%r15),%%zmm15,%%zmm7 	\n\t"/* S2 -= ss1*t6; */\
	" vfmadd231pd %%zmm13,%%zmm0,%%zmm3	\n\t  vfmadd231pd %%zmm13,%%zmm15,%%zmm8 	\n\t"/* S3 += ss4*t6; */\
	"vfnmadd231pd %%zmm11,%%zmm0,%%zmm4	\n\t vfnmadd231pd %%zmm11,%%zmm15,%%zmm9 	\n\t"/* S4 -= ss2*t6; */\
	" vfmadd231pd %%zmm12,%%zmm0,%%zmm5	\n\t  vfmadd231pd %%zmm12,%%zmm15,%%zmm10	\n\t"/* S5 += ss3*t6; */\
\
	/* Write sine-term-subconvo outputs back to memory to free up registers for the 5x5 cosine-term-subconvo computation: */\
	/* These i-ptrs no longer needed, so load o-ptrs in their place: */\
		"movq	%[__o6],%%rdi		\n\t"/* &B6 */\
		"movq	%[__o7],%%r10		\n\t"/* &B7 */\
		"movq	%[__o8],%%r11		\n\t"/* &B8 */\
		"movq	%[__o9],%%r12		\n\t"/* &B9 */\
		"movq	%[__oA],%%r13		\n\t"/* &BA */\
\
		"vmovaps	%%zmm1,(%%r13)		\n\t"/* s1r */"		vmovaps	%%zmm6 ,0x40(%%r13)	\n\t"/* s1i */\
		"vmovaps	%%zmm2,(%%r12)		\n\t"/* s2r */"		vmovaps	%%zmm7 ,0x40(%%r12)	\n\t"/* s2i */\
		"vmovaps	%%zmm3,(%%r11)		\n\t"/* s3r */"		vmovaps	%%zmm8 ,0x40(%%r11)	\n\t"/* s3i */\
		"vmovaps	%%zmm4,(%%r10)		\n\t"/* s4r */"		vmovaps	%%zmm9 ,0x40(%%r10)	\n\t"/* s4i */\
		"vmovaps	%%zmm5,(%%rdi)		\n\t"/* s5r */"		vmovaps	%%zmm10,0x40(%%rdi)	\n\t"/* s5i */\
/*
	Cr1 = t0r+cc1*t1r+cc2*t2r+cc3*t3r+cc4*t4r+cc5*t5r;		Ci1 = t0i+cc1*t1i+cc2*t2i+cc3*t3i+cc4*t4i+cc5*t5i;	// C1
	Cr2 = t0r+cc2*t1r+cc4*t2r+cc5*t3r+cc3*t4r+cc1*t5r;		Ci2 = t0i+cc2*t1i+cc4*t2i+cc5*t3i+cc3*t4i+cc1*t5i;	// C2
	Cr3 = t0r+cc3*t1r+cc5*t2r+cc2*t3r+cc1*t4r+cc4*t5r;		Ci3 = t0i+cc3*t1i+cc5*t2i+cc2*t3i+cc1*t4i+cc4*t5i;	// C3
	Cr4 = t0r+cc4*t1r+cc3*t2r+cc1*t3r+cc5*t4r+cc2*t5r;		Ci4 = t0i+cc4*t1i+cc3*t2i+cc1*t3i+cc5*t4i+cc2*t5i;	// C4
	Cr5 = t0r+cc5*t1r+cc1*t2r+cc4*t3r+cc2*t4r+cc3*t5r;		Ci5 = t0i+cc5*t1i+cc1*t2i+cc4*t3i+cc2*t4i+cc3*t5i;	// C5
	B0r = t0r+    t1r+    t2r+    t3r+    t4r+    t5r;		B0i = t0i+    t1i+    t2i+    t3i+    t4i+    t5i;	// X0

	In a 16-reg FMA model, makes sense to init the c-terms to the t0-summand,
	and keep the 10 c-terms and 4 of the 5 shared sincos data in registers. That uses 14 regs,
	leaving 2 for the 2 t*[r,i] data shared by each such block. Those need to be in-reg (at least
	in the cosine-mults section) because of the need to accumulate the DC terms: E.g. at end of the
	first block below (after doing the 10 c-term-update FMAs) we add the current values of B0r,i
	(which we init to t0r,i in the ASM version) to t1r,i, then write the result to memory and
	load t2r,i into the same 2 regs in preparation for the next such block.
*/\
	"subq	$0x140,%%r15	\n\t"/* Decr trig ptr to point to cc1 */\
		"movq	%[__I0],%%rdi		\n\t"\
		"vmovaps	(%%rdi),%%zmm1		\n\t	vmovaps	0x40(%%rdi),%%zmm6	\n\t"/* c1 = A0 */\
		"vmovaps	%%zmm1,%%zmm2		\n\t	vmovaps	%%zmm6 ,%%zmm7 		\n\t"/* c2 = A0 */\
		"vmovaps	%%zmm1,%%zmm3		\n\t	vmovaps	%%zmm6 ,%%zmm8 		\n\t"/* c3 = A0 */\
		"vmovaps	%%zmm1,%%zmm4		\n\t	vmovaps	%%zmm6 ,%%zmm9 		\n\t"/* c4 = A0 */\
		"vmovaps	%%zmm1,%%zmm5		\n\t	vmovaps	%%zmm6 ,%%zmm10		\n\t"/* c5 = A0 */\
\
		"vmovaps	0x40(%%r15),%%zmm11	\n\t	vmovaps	0x80(%%r15),%%zmm12		\n\t"/* cc2,cc3 */\
		"vmovaps	0xc0(%%r15),%%zmm13	\n\t	vmovaps	0x100(%%r15),%%zmm14		\n\t"/* cc4,cc5 */\
\
		"vmovaps	    (%%rax),%%zmm0	\n\t	vmovaps	0x40(%%rax),%%zmm15	\n\t"/* t1 */\
	"vfmadd231pd (%%r15),%%zmm0,%%zmm1	\n\t vfmadd231pd (%%r15),%%zmm15,%%zmm6 	\n\t"/* c1 += cc1*t1; */\
	"vfmadd231pd %%zmm11,%%zmm0,%%zmm2	\n\t vfmadd231pd %%zmm11,%%zmm15,%%zmm7 	\n\t"/* c2 += cc2*t1; */\
	"vfmadd231pd %%zmm12,%%zmm0,%%zmm3	\n\t vfmadd231pd %%zmm12,%%zmm15,%%zmm8 	\n\t"/* c3 += cc3*t1; */\
	"vfmadd231pd %%zmm13,%%zmm0,%%zmm4	\n\t vfmadd231pd %%zmm13,%%zmm15,%%zmm9 	\n\t"/* c4 += cc4*t1; */\
	"vfmadd231pd %%zmm14,%%zmm0,%%zmm5	\n\t vfmadd231pd %%zmm14,%%zmm15,%%zmm10	\n\t"/* c5 += cc5*t1; */\
		"vaddpd (%%rdi),%%zmm0,%%zmm0	\n\t	vaddpd 0x40(%%rdi),%%zmm15,%%zmm15	\n\t"/* B0 += t1; */\
		"vmovaps	%%zmm0,(%%rdi)		\n\t	vmovaps	%%zmm15,0x40(%%rdi)			\n\t"/* Store B0 */\
\
		"vmovaps	    (%%rbx),%%zmm0	\n\t	vmovaps	0x40(%%rbx),%%zmm15	\n\t"/* t2 */\
	"vfmadd231pd %%zmm11,%%zmm0,%%zmm1	\n\t vfmadd231pd %%zmm11,%%zmm15,%%zmm6 	\n\t"/* c1 += cc2*t2; */\
	"vfmadd231pd %%zmm13,%%zmm0,%%zmm2	\n\t vfmadd231pd %%zmm13,%%zmm15,%%zmm7 	\n\t"/* c2 += cc4*t2; */\
	"vfmadd231pd %%zmm14,%%zmm0,%%zmm3	\n\t vfmadd231pd %%zmm14,%%zmm15,%%zmm8 	\n\t"/* c3 += cc5*t2; */\
	"vfmadd231pd %%zmm12,%%zmm0,%%zmm4	\n\t vfmadd231pd %%zmm12,%%zmm15,%%zmm9 	\n\t"/* c4 += cc3*t2; */\
	"vfmadd231pd (%%r15),%%zmm0,%%zmm5	\n\t vfmadd231pd (%%r15),%%zmm15,%%zmm10	\n\t"/* c5 += cc1*t2; */\
		"vaddpd (%%rdi),%%zmm0,%%zmm0	\n\t	vaddpd 0x40(%%rdi),%%zmm15,%%zmm15	\n\t"/* B0 += t2; */\
		"vmovaps	%%zmm0,(%%rdi)		\n\t	vmovaps	%%zmm15,0x40(%%rdi)			\n\t"/* Store B0 */\
\
		"vmovaps	    (%%rcx),%%zmm0	\n\t	vmovaps	0x40(%%rcx),%%zmm15	\n\t"/* t3 */\
	"vfmadd231pd %%zmm12,%%zmm0,%%zmm1	\n\t vfmadd231pd %%zmm12,%%zmm15,%%zmm6 	\n\t"/* c1 += cc3*t3; */\
	"vfmadd231pd %%zmm14,%%zmm0,%%zmm2	\n\t vfmadd231pd %%zmm14,%%zmm15,%%zmm7 	\n\t"/* c2 += cc5*t3; */\
	"vfmadd231pd %%zmm11,%%zmm0,%%zmm3	\n\t vfmadd231pd %%zmm11,%%zmm15,%%zmm8 	\n\t"/* c3 += cc2*t3; */\
	"vfmadd231pd (%%r15),%%zmm0,%%zmm4	\n\t vfmadd231pd (%%r15),%%zmm15,%%zmm9 	\n\t"/* c4 += cc1*t3; */\
	"vfmadd231pd %%zmm13,%%zmm0,%%zmm5	\n\t vfmadd231pd %%zmm13,%%zmm15,%%zmm10	\n\t"/* c5 += cc4*t3; */\
		"vaddpd (%%rdi),%%zmm0,%%zmm0	\n\t	vaddpd 0x40(%%rdi),%%zmm15,%%zmm15	\n\t"/* B0 += t3; */\
		"vmovaps	%%zmm0,(%%rdi)		\n\t	vmovaps	%%zmm15,0x40(%%rdi)			\n\t"/* Store B0 */\
\
		"vmovaps	    (%%rdx),%%zmm0	\n\t	vmovaps	0x40(%%rdx),%%zmm15	\n\t"/* t4 */\
	"vfmadd231pd %%zmm13,%%zmm0,%%zmm1	\n\t vfmadd231pd %%zmm13,%%zmm15,%%zmm6 	\n\t"/* c1 += cc4*t4; */\
	"vfmadd231pd %%zmm12,%%zmm0,%%zmm2	\n\t vfmadd231pd %%zmm12,%%zmm15,%%zmm7 	\n\t"/* c2 += cc3*t4; */\
	"vfmadd231pd (%%r15),%%zmm0,%%zmm3	\n\t vfmadd231pd (%%r15),%%zmm15,%%zmm8 	\n\t"/* c3 += cc1*t4; */\
	"vfmadd231pd %%zmm14,%%zmm0,%%zmm4	\n\t vfmadd231pd %%zmm14,%%zmm15,%%zmm9 	\n\t"/* c4 += cc5*t4; */\
	"vfmadd231pd %%zmm11,%%zmm0,%%zmm5	\n\t vfmadd231pd %%zmm11,%%zmm15,%%zmm10	\n\t"/* c5 += cc2*t4; */\
		"vaddpd (%%rdi),%%zmm0,%%zmm0	\n\t	vaddpd 0x40(%%rdi),%%zmm15,%%zmm15	\n\t"/* B0 += t4; */\
		"vmovaps	%%zmm0,(%%rdi)		\n\t	vmovaps	%%zmm15,0x40(%%rdi)			\n\t"/* Store B0 */\
\
		"vmovaps	    (%%rsi),%%zmm0	\n\t	vmovaps	0x40(%%rsi),%%zmm15	\n\t"/* t5 */\
	"vfmadd231pd %%zmm14,%%zmm0,%%zmm1	\n\t vfmadd231pd %%zmm14,%%zmm15,%%zmm6 	\n\t"/* c1 += cc5*t5; */\
	"vfmadd231pd (%%r15),%%zmm0,%%zmm2	\n\t vfmadd231pd (%%r15),%%zmm15,%%zmm7 	\n\t"/* c2 += cc1*t5; */\
	"vfmadd231pd %%zmm13,%%zmm0,%%zmm3	\n\t vfmadd231pd %%zmm13,%%zmm15,%%zmm8 	\n\t"/* c3 += cc4*t5; */\
	"vfmadd231pd %%zmm11,%%zmm0,%%zmm4	\n\t vfmadd231pd %%zmm11,%%zmm15,%%zmm9 	\n\t"/* c4 += cc2*t5; */\
	"vfmadd231pd %%zmm12,%%zmm0,%%zmm5	\n\t vfmadd231pd %%zmm12,%%zmm15,%%zmm10	\n\t"/* c5 += cc3*t5; */\
		"vaddpd (%%rdi),%%zmm0,%%zmm0	\n\t	vaddpd 0x40(%%rdi),%%zmm15,%%zmm15	\n\t"/* B0 += t5; */\
		"movq	%[__O0],%%rdi		\n\t"\
		"vmovaps	%%zmm0,(%%rdi)		\n\t	vmovaps	%%zmm15,0x40(%%rdi)			\n\t"/* Store B0, now into output slot */\
\
		"movq	%[__o6],%%rdi		\n\t"/* restore o-ptr rdi-value (r10-13 still hold &B7-A)) */\
		/* Have enough registers to also hold the real parts of S-terms: */\
		"vmovaps	(%%r13),%%zmm11		\n\t"/* s1r */"	movq	%[__o1],%%rax	\n\t"/* &B1 */\
		"vmovaps	(%%r12),%%zmm12		\n\t"/* s2r */"	movq	%[__o2],%%rbx	\n\t"/* &B2 */\
		"vmovaps	(%%r11),%%zmm13		\n\t"/* s3r */"	movq	%[__o3],%%rcx	\n\t"/* &B3 */\
		"vmovaps	(%%r10),%%zmm14		\n\t"/* s4r */"	movq	%[__o4],%%rdx	\n\t"/* &B4 */\
		"vmovaps	(%%rdi),%%zmm15		\n\t"/* s5r */"	movq	%[__o5],%%rsi	\n\t"/* &B5 */\
\
	/* ...Do the lower-half-index portion of the closing wave of radix-2 butterflies: */\
		"vmovaps	(%%r9),%%zmm0		\n\t"/* one */\
		"vfnmadd231pd 0x40(%%r13),%%zmm0,%%zmm1		\n\t	 vfmadd231pd %%zmm11,%%zmm0,%%zmm6 	\n\t"/* B1 = C1 + I*S1 */\
		"vfnmadd231pd 0x40(%%r12),%%zmm0,%%zmm2		\n\t	 vfmadd231pd %%zmm12,%%zmm0,%%zmm7 	\n\t"/* B2 = C2 + I*S2 */\
		"vfnmadd231pd 0x40(%%r11),%%zmm0,%%zmm3		\n\t	 vfmadd231pd %%zmm13,%%zmm0,%%zmm8 	\n\t"/* B3 = C3 + I*S3 */\
		"vfnmadd231pd 0x40(%%r10),%%zmm0,%%zmm4		\n\t	 vfmadd231pd %%zmm14,%%zmm0,%%zmm9 	\n\t"/* B4 = C4 + I*S4 */\
		"vfnmadd231pd 0x40(%%rdi),%%zmm0,%%zmm5		\n\t	 vfmadd231pd %%zmm15,%%zmm0,%%zmm10	\n\t"/* B5 = C5 + I*S5 */\
	/* Write lower-half outputs back to memory... */\
		"vmovaps	%%zmm1,(%%rax)		\n\t"/* B1r */"		vmovaps	%%zmm6 ,0x40(%%rax)	\n\t"/* B1i */\
		"vmovaps	%%zmm2,(%%rbx)		\n\t"/* B2r */"		vmovaps	%%zmm7 ,0x40(%%rbx)	\n\t"/* B2i */\
		"vmovaps	%%zmm3,(%%rcx)		\n\t"/* B3r */"		vmovaps	%%zmm8 ,0x40(%%rcx)	\n\t"/* B3i */\
		"vmovaps	%%zmm4,(%%rdx)		\n\t"/* B4r */"		vmovaps	%%zmm9 ,0x40(%%rdx)	\n\t"/* B4i */\
		"vmovaps	%%zmm5,(%%rsi)		\n\t"/* B5r */"		vmovaps	%%zmm10,0x40(%%rsi)	\n\t"/* B5i */\
	/* ...Do the upper-half-index portion of the radix-2 butterflies: */\
		"vmovaps	(%%r8),%%zmm0		\n\t"/* two */\
		" vfmadd231pd 0x40(%%r13),%%zmm0,%%zmm1		\n\t	vfnmadd231pd %%zmm11,%%zmm0,%%zmm6 	\n\t"/* B10= C1 - I*S1 */\
		" vfmadd231pd 0x40(%%r12),%%zmm0,%%zmm2		\n\t	vfnmadd231pd %%zmm12,%%zmm0,%%zmm7 	\n\t"/* B9 = C2 - I*S2 */\
		" vfmadd231pd 0x40(%%r11),%%zmm0,%%zmm3		\n\t	vfnmadd231pd %%zmm13,%%zmm0,%%zmm8 	\n\t"/* B8 = C3 - I*S3 */\
		" vfmadd231pd 0x40(%%r10),%%zmm0,%%zmm4		\n\t	vfnmadd231pd %%zmm14,%%zmm0,%%zmm9 	\n\t"/* B7 = C4 - I*S4 */\
		" vfmadd231pd 0x40(%%rdi),%%zmm0,%%zmm5		\n\t	vfnmadd231pd %%zmm15,%%zmm0,%%zmm10	\n\t"/* B6 = C5 - I*S5 */\
	/* ...And write the upper-half outputs back to memory. */\
		"vmovaps	%%zmm1,(%%r13)		\n\t"/* Bar */"		vmovaps	%%zmm6 ,0x40(%%r13)	\n\t"/* Bai */\
		"vmovaps	%%zmm2,(%%r12)		\n\t"/* B9r */"		vmovaps	%%zmm7 ,0x40(%%r12)	\n\t"/* B9i */\
		"vmovaps	%%zmm3,(%%r11)		\n\t"/* B8r */"		vmovaps	%%zmm8 ,0x40(%%r11)	\n\t"/* B8i */\
		"vmovaps	%%zmm4,(%%r10)		\n\t"/* B7r */"		vmovaps	%%zmm9 ,0x40(%%r10)	\n\t"/* B7i */\
		"vmovaps	%%zmm5,(%%rdi)		\n\t"/* B6r */"		vmovaps	%%zmm10,0x40(%%rdi)	\n\t"/* B6i */\
		:					/* outputs: none */\
		: [__I0] "m" (XI0)	/* All inputs from memory addresses here */\
		 ,[__i1] "m" (Xi1)\
		 ,[__i2] "m" (Xi2)\
		 ,[__i3] "m" (Xi3)\
		 ,[__i4] "m" (Xi4)\
		 ,[__i5] "m" (Xi5)\
		 ,[__i6] "m" (Xi6)\
		 ,[__i7] "m" (Xi7)\
		 ,[__i8] "m" (Xi8)\
		 ,[__i9] "m" (Xi9)\
		 ,[__iA] "m" (XiA)\
		 ,[__cc] "m" (Xcc)\
		 ,[__O0] "m" (XO0)\
		 ,[__o1] "m" (Xo1)\
		 ,[__o2] "m" (Xo2)\
		 ,[__o3] "m" (Xo3)\
		 ,[__o4] "m" (Xo4)\
		 ,[__o5] "m" (Xo5)\
		 ,[__o6] "m" (Xo6)\
		 ,[__o7] "m" (Xo7)\
		 ,[__o8] "m" (Xo8)\
		 ,[__o9] "m" (Xo9)\
		 ,[__oA] "m" (XoA)\
		: "cc","memory","rax","rbx","rcx","rdx","rsi","rdi","r8","r9","r10","r11","r12","r13","r15","xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7","xmm8","xmm9","xmm10","xmm11","xmm12","xmm13","xmm14","xmm15"		/* Clobbered registers */\
		);\
	}

#elif defined(USE_AVX2)	// AVX+FMA version, based on RADIX_11_DFT_FMA in dft_macro.h

	// FMAs used for all arithmetic, including 'trivial' ones (one mult = 1.0) to replace ADD/SUB:
	//
	// Arithmetic opcount: [10 ADD, 140 FMA (20 trivial, incl 10 MUL), 166 memref], close to general target of 1 memref per vec_dbl arithmetic op.
	// potentially much lower cycle count (at a max theoretical rate of 2 FMA/cycle) than non-FMA version.
	//
	// Compare to non-FMA: [182 ADD, 44 MUL), 154 memref], less accurate and bottlenecked by 1 ADD/cycle max issue rate.
	//
	#define SSE2_RADIX_11_DFT(XI0,Xi1,Xi2,Xi3,Xi4,Xi5,Xi6,Xi7,Xi8,Xi9,XiA, Xcc, XO0,Xo1,Xo2,Xo3,Xo4,Xo5,Xo6,Xo7,Xo8,Xo9,XoA)\
	{\
	__asm__ volatile (\
		"movq	%[__cc],%%r15	\n\t	leaq	-0x40(%%r15),%%r8	\n\t	leaq	-0x20(%%r15),%%r9	\n\t"/* cc1,two,one */\
		"movq	%[__i1],%%rax			\n\t"/* &A1 */"		movq	%[__i6],%%rdi		\n\t"/* &A6 */\
		"movq	%[__i2],%%rbx			\n\t"/* &A2 */"		movq	%[__i7],%%r10		\n\t"/* &A7 */\
		"movq	%[__i3],%%rcx			\n\t"/* &A3 */"		movq	%[__i8],%%r11		\n\t"/* &A8 */\
		"movq	%[__i4],%%rdx			\n\t"/* &A4 */"		movq	%[__i9],%%r12		\n\t"/* &A9 */\
		"movq	%[__i5],%%rsi			\n\t"/* &A5 */"		movq	%[__iA],%%r13		\n\t"/* &AA */\
	\
		"vmovaps	(%%rax),%%ymm1		\n\t"/* A1r */"		vmovaps	0x20(%%rax),%%ymm6 	\n\t"/* A1i */\
		"vmovaps	(%%rbx),%%ymm2		\n\t"/* A2r */"		vmovaps	0x20(%%rbx),%%ymm7 	\n\t"/* A2i */\
		"vmovaps	(%%rcx),%%ymm3		\n\t"/* A3r */"		vmovaps	0x20(%%rcx),%%ymm8 	\n\t"/* A3i */\
		"vmovaps	(%%rdx),%%ymm4		\n\t"/* A4r */"		vmovaps	0x20(%%rdx),%%ymm9 	\n\t"/* A4i */\
		"vmovaps	(%%rsi),%%ymm5		\n\t"/* A5r */"		vmovaps	0x20(%%rsi),%%ymm10	\n\t"/* A5i */\
		/* Have enough registers to also hold the real-upper-half inputs: */\
		"vmovaps	(%%rdi),%%ymm11		\n\t"/* A6r */\
		"vmovaps	(%%r10),%%ymm12		\n\t"/* A7r */\
		"vmovaps	(%%r11),%%ymm13		\n\t"/* A8r */\
		"vmovaps	(%%r12),%%ymm14		\n\t"/* A9r */\
		"vmovaps	(%%r13),%%ymm15		\n\t"/* AAr */\
	\
	/* ...Do the + parts of the opening wave of radix-2 butterflies: */\
		"vmovaps	(%%r9),%%ymm0		\n\t"/* one */\
		" vfmadd231pd %%ymm15,%%ymm0,%%ymm1		\n\t	 vfmadd231pd 0x20(%%r13),%%ymm0,%%ymm6 	\n\t"/* t1 = A1 + Aa */\
		" vfmadd231pd %%ymm14,%%ymm0,%%ymm2		\n\t	 vfmadd231pd 0x20(%%r12),%%ymm0,%%ymm7 	\n\t"/* t2 = A2 + A9 */\
		" vfmadd231pd %%ymm13,%%ymm0,%%ymm3		\n\t	 vfmadd231pd 0x20(%%r11),%%ymm0,%%ymm8 	\n\t"/* t3 = A3 + A8 */\
		" vfmadd231pd %%ymm12,%%ymm0,%%ymm4		\n\t	 vfmadd231pd 0x20(%%r10),%%ymm0,%%ymm9 	\n\t"/* t4 = A4 + A7 */\
		" vfmadd231pd %%ymm11,%%ymm0,%%ymm5		\n\t	 vfmadd231pd 0x20(%%rdi),%%ymm0,%%ymm10	\n\t"/* t5 = A5 + A6 */\
	/* Write lower-half outputs back to memory... */\
		"vmovaps	%%ymm1,(%%rax)		\n\t"/* t1r */"		vmovaps	%%ymm6 ,0x20(%%rax)	\n\t"/* t1i */\
		"vmovaps	%%ymm2,(%%rbx)		\n\t"/* t2r */"		vmovaps	%%ymm7 ,0x20(%%rbx)	\n\t"/* t2i */\
		"vmovaps	%%ymm3,(%%rcx)		\n\t"/* t3r */"		vmovaps	%%ymm8 ,0x20(%%rcx)	\n\t"/* t3i */\
		"vmovaps	%%ymm4,(%%rdx)		\n\t"/* t4r */"		vmovaps	%%ymm9 ,0x20(%%rdx)	\n\t"/* t4i */\
		"vmovaps	%%ymm5,(%%rsi)		\n\t"/* t5r */"		vmovaps	%%ymm10,0x20(%%rsi)	\n\t"/* t5i */\
	/* ...Do the - parts of the radix-2 butterflies: */\
		"vmovaps	(%%r8),%%ymm0		\n\t"/* two */\
		"vfnmadd231pd %%ymm15,%%ymm0,%%ymm1		\n\t	vfnmadd231pd 0x20(%%r13),%%ymm0,%%ymm6 	\n\t"/* ta = A1 - Aa */\
		"vfnmadd231pd %%ymm14,%%ymm0,%%ymm2		\n\t	vfnmadd231pd 0x20(%%r12),%%ymm0,%%ymm7 	\n\t"/* t9 = A2 - A9 */\
		"vfnmadd231pd %%ymm13,%%ymm0,%%ymm3		\n\t	vfnmadd231pd 0x20(%%r11),%%ymm0,%%ymm8 	\n\t"/* t8 = A3 - A8 */\
		"vfnmadd231pd %%ymm12,%%ymm0,%%ymm4		\n\t	vfnmadd231pd 0x20(%%r10),%%ymm0,%%ymm9 	\n\t"/* t7 = A4 - A7 */\
		"vfnmadd231pd %%ymm11,%%ymm0,%%ymm5		\n\t	vfnmadd231pd 0x20(%%rdi),%%ymm0,%%ymm10	\n\t"/* t6 = A5 - A6 */\
	/* ...And write the upper-half outputs back to memory to free up registers for the 5x5 sine-term-subconvo computation: */\
		"vmovaps	%%ymm1,(%%r13)		\n\t"/* tar */"		vmovaps	%%ymm6 ,0x20(%%r13)	\n\t"/* tai */\
		"vmovaps	%%ymm2,(%%r12)		\n\t"/* t9r */"		vmovaps	%%ymm7 ,0x20(%%r12)	\n\t"/* t9i */\
		"vmovaps	%%ymm3,(%%r11)		\n\t"/* t8r */"		vmovaps	%%ymm8 ,0x20(%%r11)	\n\t"/* t8i */\
		"vmovaps	%%ymm4,(%%r10)		\n\t"/* t7r */"		vmovaps	%%ymm9 ,0x20(%%r10)	\n\t"/* t7i */\
		"vmovaps	%%ymm5,(%%rdi)		\n\t"/* t6r */"		vmovaps	%%ymm10,0x20(%%rdi)	\n\t"/* t6i */\
/*
	Sr1 =      ss1*tar+ss2*t9r+ss3*t8r+ss4*t7r+ss5*t6r;		Si1 =      ss1*tai+ss2*t9i+ss3*t8i+ss4*t7i+ss5*t6i;	// S1
	Sr2 =      ss2*tar+ss4*t9r-ss5*t8r-ss3*t7r-ss1*t6r;		Si2 =      ss2*tai+ss4*t9i-ss5*t8i-ss3*t7i-ss1*t6i;	// S2
	Sr3 =      ss3*tar-ss5*t9r-ss2*t8r+ss1*t7r+ss4*t6r;		Si3 =      ss3*tai-ss5*t9i-ss2*t8i+ss1*t7i+ss4*t6i;	// S3
	Sr4 =      ss4*tar-ss3*t9r+ss1*t8r+ss5*t7r-ss2*t6r;		Si4 =      ss4*tai-ss3*t9i+ss1*t8i+ss5*t7i-ss2*t6i;	// S4
	Sr5 =      ss5*tar-ss1*t9r+ss4*t8r-ss2*t7r+ss3*t6r;		Si5 =      ss5*tai-ss1*t9i+ss4*t8i-ss2*t7i+ss3*t6i;	// S5

	In a 16-reg FMA model, can keep the 10 S-terms and all 5 shared sincos data in registers, but that
	requires us to reload one of the 2 repeated t*-mults (e.g. tai in the 1st block) 5 times per block.
	OTOH if we keep both t*r,i-mults and 4 of the 5 ss-mults in-reg, we just need 2 loads-from-mem per block:
*/\
		"vmovaps	(%%r13),%%ymm1		\n\t	vmovaps	0x20(%%r13),%%ymm6	\n\t"/* S1 = ta */\
		"vmovaps	%%ymm1,%%ymm2		\n\t	vmovaps	%%ymm6 ,%%ymm7 		\n\t"/* S2 = ta */\
		"vmovaps	%%ymm1,%%ymm3		\n\t	vmovaps	%%ymm6 ,%%ymm8 		\n\t"/* S3 = ta */\
		"vmovaps	%%ymm1,%%ymm4		\n\t	vmovaps	%%ymm6 ,%%ymm9 		\n\t"/* S4 = ta */\
		"vmovaps	%%ymm1,%%ymm5		\n\t	vmovaps	%%ymm6 ,%%ymm10		\n\t"/* S5 = ta */\
	"addq	$0xa0,%%r15	\n\t"/* Incr trig ptr to point to ss1 */\
		"vmovaps	0x20(%%r15),%%ymm11	\n\t	vmovaps	0x40(%%r15),%%ymm12		\n\t"/* ss2,ss3 */\
		"vmovaps	0x60(%%r15),%%ymm13	\n\t	vmovaps	0x80(%%r15),%%ymm14		\n\t"/* ss4,ss5 */\
\
	"	vmulpd	(%%r15),%%ymm1,%%ymm1	\n\t	vmulpd	(%%r15),%%ymm6 ,%%ymm6 	\n\t"/* S1  = ss1*ta; */\
	"	vmulpd	%%ymm11,%%ymm2,%%ymm2	\n\t	vmulpd	%%ymm11,%%ymm7 ,%%ymm7 	\n\t"/* S2  = ss2*ta; */\
	"	vmulpd	%%ymm12,%%ymm3,%%ymm3	\n\t	vmulpd	%%ymm12,%%ymm8 ,%%ymm8 	\n\t"/* S3  = ss3*ta; */\
	"	vmulpd	%%ymm13,%%ymm4,%%ymm4	\n\t	vmulpd	%%ymm13,%%ymm9 ,%%ymm9 	\n\t"/* S4  = ss4*ta; */\
	"	vmulpd	%%ymm14,%%ymm5,%%ymm5	\n\t	vmulpd	%%ymm14,%%ymm10,%%ymm10	\n\t"/* S5  = ss5*ta; */\
\
		"vmovaps	    (%%r12),%%ymm0	\n\t	vmovaps	0x20(%%r12),%%ymm15	\n\t"/* t9 */\
	" vfmadd231pd %%ymm11,%%ymm0,%%ymm1	\n\t  vfmadd231pd %%ymm11,%%ymm15,%%ymm6 	\n\t"/* S1 += ss2*t9; */\
	" vfmadd231pd %%ymm13,%%ymm0,%%ymm2	\n\t  vfmadd231pd %%ymm13,%%ymm15,%%ymm7 	\n\t"/* S2 += ss4*t9; */\
	"vfnmadd231pd %%ymm14,%%ymm0,%%ymm3	\n\t vfnmadd231pd %%ymm14,%%ymm15,%%ymm8 	\n\t"/* S3 -= ss5*t9; */\
	"vfnmadd231pd %%ymm12,%%ymm0,%%ymm4	\n\t vfnmadd231pd %%ymm12,%%ymm15,%%ymm9 	\n\t"/* S4 -= ss3*t9; */\
	"vfnmadd231pd (%%r15),%%ymm0,%%ymm5	\n\t vfnmadd231pd (%%r15),%%ymm15,%%ymm10	\n\t"/* S5 -= ss1*t9; */\
\
		"vmovaps	    (%%r11),%%ymm0	\n\t	vmovaps	0x20(%%r11),%%ymm15	\n\t"/* t8 */\
	" vfmadd231pd %%ymm12,%%ymm0,%%ymm1	\n\t  vfmadd231pd %%ymm12,%%ymm15,%%ymm6 	\n\t"/* S1 += ss3*t8; */\
	"vfnmadd231pd %%ymm14,%%ymm0,%%ymm2	\n\t vfnmadd231pd %%ymm14,%%ymm15,%%ymm7 	\n\t"/* S2 -= ss5*t8; */\
	"vfnmadd231pd %%ymm11,%%ymm0,%%ymm3	\n\t vfnmadd231pd %%ymm11,%%ymm15,%%ymm8 	\n\t"/* S3 -= ss2*t8; */\
	" vfmadd231pd (%%r15),%%ymm0,%%ymm4	\n\t  vfmadd231pd (%%r15),%%ymm15,%%ymm9 	\n\t"/* S4 += ss1*t8; */\
	" vfmadd231pd %%ymm13,%%ymm0,%%ymm5	\n\t  vfmadd231pd %%ymm13,%%ymm15,%%ymm10	\n\t"/* S5 += ss4*t8; */\
\
		"vmovaps	    (%%r10),%%ymm0	\n\t	vmovaps	0x20(%%r10),%%ymm15	\n\t"/* t7 */\
	" vfmadd231pd %%ymm13,%%ymm0,%%ymm1	\n\t  vfmadd231pd %%ymm13,%%ymm15,%%ymm6 	\n\t"/* S1 += ss4*t7; */\
	"vfnmadd231pd %%ymm12,%%ymm0,%%ymm2	\n\t vfnmadd231pd %%ymm12,%%ymm15,%%ymm7 	\n\t"/* S2 -= ss3*t7; */\
	" vfmadd231pd (%%r15),%%ymm0,%%ymm3	\n\t  vfmadd231pd (%%r15),%%ymm15,%%ymm8 	\n\t"/* S3 += ss1*t7; */\
	" vfmadd231pd %%ymm14,%%ymm0,%%ymm4	\n\t  vfmadd231pd %%ymm14,%%ymm15,%%ymm9 	\n\t"/* S4 += ss5*t7; */\
	"vfnmadd231pd %%ymm11,%%ymm0,%%ymm5	\n\t vfnmadd231pd %%ymm11,%%ymm15,%%ymm10	\n\t"/* S5 -= ss2*t7; */\
\
		"vmovaps	    (%%rdi),%%ymm0	\n\t	vmovaps	0x20(%%rdi),%%ymm15	\n\t"/* t6 */\
	" vfmadd231pd %%ymm14,%%ymm0,%%ymm1	\n\t  vfmadd231pd %%ymm14,%%ymm15,%%ymm6 	\n\t"/* S1 += ss5*t6; */\
	"vfnmadd231pd (%%r15),%%ymm0,%%ymm2	\n\t vfnmadd231pd (%%r15),%%ymm15,%%ymm7 	\n\t"/* S2 -= ss1*t6; */\
	" vfmadd231pd %%ymm13,%%ymm0,%%ymm3	\n\t  vfmadd231pd %%ymm13,%%ymm15,%%ymm8 	\n\t"/* S3 += ss4*t6; */\
	"vfnmadd231pd %%ymm11,%%ymm0,%%ymm4	\n\t vfnmadd231pd %%ymm11,%%ymm15,%%ymm9 	\n\t"/* S4 -= ss2*t6; */\
	" vfmadd231pd %%ymm12,%%ymm0,%%ymm5	\n\t  vfmadd231pd %%ymm12,%%ymm15,%%ymm10	\n\t"/* S5 += ss3*t6; */\
\
	/* Write sine-term-subconvo outputs back to memory to free up registers for the 5x5 cosine-term-subconvo computation: */\
	/* These i-ptrs no longer needed, so load o-ptrs in their place: */\
		"movq	%[__o6],%%rdi		\n\t"/* &B6 */\
		"movq	%[__o7],%%r10		\n\t"/* &B7 */\
		"movq	%[__o8],%%r11		\n\t"/* &B8 */\
		"movq	%[__o9],%%r12		\n\t"/* &B9 */\
		"movq	%[__oA],%%r13		\n\t"/* &BA */\
\
		"vmovaps	%%ymm1,(%%r13)		\n\t"/* s1r */"		vmovaps	%%ymm6 ,0x20(%%r13)	\n\t"/* s1i */\
		"vmovaps	%%ymm2,(%%r12)		\n\t"/* s2r */"		vmovaps	%%ymm7 ,0x20(%%r12)	\n\t"/* s2i */\
		"vmovaps	%%ymm3,(%%r11)		\n\t"/* s3r */"		vmovaps	%%ymm8 ,0x20(%%r11)	\n\t"/* s3i */\
		"vmovaps	%%ymm4,(%%r10)		\n\t"/* s4r */"		vmovaps	%%ymm9 ,0x20(%%r10)	\n\t"/* s4i */\
		"vmovaps	%%ymm5,(%%rdi)		\n\t"/* s5r */"		vmovaps	%%ymm10,0x20(%%rdi)	\n\t"/* s5i */\
/*
	Cr1 = t0r+cc1*t1r+cc2*t2r+cc3*t3r+cc4*t4r+cc5*t5r;		Ci1 = t0i+cc1*t1i+cc2*t2i+cc3*t3i+cc4*t4i+cc5*t5i;	// C1
	Cr2 = t0r+cc2*t1r+cc4*t2r+cc5*t3r+cc3*t4r+cc1*t5r;		Ci2 = t0i+cc2*t1i+cc4*t2i+cc5*t3i+cc3*t4i+cc1*t5i;	// C2
	Cr3 = t0r+cc3*t1r+cc5*t2r+cc2*t3r+cc1*t4r+cc4*t5r;		Ci3 = t0i+cc3*t1i+cc5*t2i+cc2*t3i+cc1*t4i+cc4*t5i;	// C3
	Cr4 = t0r+cc4*t1r+cc3*t2r+cc1*t3r+cc5*t4r+cc2*t5r;		Ci4 = t0i+cc4*t1i+cc3*t2i+cc1*t3i+cc5*t4i+cc2*t5i;	// C4
	Cr5 = t0r+cc5*t1r+cc1*t2r+cc4*t3r+cc2*t4r+cc3*t5r;		Ci5 = t0i+cc5*t1i+cc1*t2i+cc4*t3i+cc2*t4i+cc3*t5i;	// C5
	B0r = t0r+    t1r+    t2r+    t3r+    t4r+    t5r;		B0i = t0i+    t1i+    t2i+    t3i+    t4i+    t5i;	// X0

	In a 16-reg FMA model, makes sense to init the c-terms to the t0-summand,
	and keep the 10 c-terms and 4 of the 5 shared sincos data in registers. That uses 14 regs,
	leaving 2 for the 2 t*[r,i] data shared by each such block. Those need to be in-reg (at least
	in the cosine-mults section) because of the need to accumulate the DC terms: E.g. at end of the
	first block below (after doing the 10 c-term-update FMAs) we add the current values of B0r,i
	(which we init to t0r,i in the ASM version) to t1r,i, then write the result to memory and
	load t2r,i into the same 2 regs in preparation for the next such block.
*/\
	"subq	$0xa0,%%r15	\n\t"/* Decr trig ptr to point to cc1 */\
		"movq	%[__I0],%%rdi		\n\t"\
		"vmovaps	(%%rdi),%%ymm1		\n\t	vmovaps	0x20(%%rdi),%%ymm6	\n\t"/* c1 = A0 */\
		"vmovaps	%%ymm1,%%ymm2		\n\t	vmovaps	%%ymm6 ,%%ymm7 		\n\t"/* c2 = A0 */\
		"vmovaps	%%ymm1,%%ymm3		\n\t	vmovaps	%%ymm6 ,%%ymm8 		\n\t"/* c3 = A0 */\
		"vmovaps	%%ymm1,%%ymm4		\n\t	vmovaps	%%ymm6 ,%%ymm9 		\n\t"/* c4 = A0 */\
		"vmovaps	%%ymm1,%%ymm5		\n\t	vmovaps	%%ymm6 ,%%ymm10		\n\t"/* c5 = A0 */\
\
		"vmovaps	0x20(%%r15),%%ymm11	\n\t	vmovaps	0x40(%%r15),%%ymm12		\n\t"/* cc2,cc3 */\
		"vmovaps	0x60(%%r15),%%ymm13	\n\t	vmovaps	0x80(%%r15),%%ymm14		\n\t"/* cc4,cc5 */\
\
		"vmovaps	    (%%rax),%%ymm0	\n\t	vmovaps	0x20(%%rax),%%ymm15	\n\t"/* t1 */\
	"vfmadd231pd (%%r15),%%ymm0,%%ymm1	\n\t vfmadd231pd (%%r15),%%ymm15,%%ymm6 	\n\t"/* c1 += cc1*t1; */\
	"vfmadd231pd %%ymm11,%%ymm0,%%ymm2	\n\t vfmadd231pd %%ymm11,%%ymm15,%%ymm7 	\n\t"/* c2 += cc2*t1; */\
	"vfmadd231pd %%ymm12,%%ymm0,%%ymm3	\n\t vfmadd231pd %%ymm12,%%ymm15,%%ymm8 	\n\t"/* c3 += cc3*t1; */\
	"vfmadd231pd %%ymm13,%%ymm0,%%ymm4	\n\t vfmadd231pd %%ymm13,%%ymm15,%%ymm9 	\n\t"/* c4 += cc4*t1; */\
	"vfmadd231pd %%ymm14,%%ymm0,%%ymm5	\n\t vfmadd231pd %%ymm14,%%ymm15,%%ymm10	\n\t"/* c5 += cc5*t1; */\
		"vaddpd (%%rdi),%%ymm0,%%ymm0	\n\t	vaddpd 0x20(%%rdi),%%ymm15,%%ymm15	\n\t"/* B0 += t1; */\
		"vmovaps	%%ymm0,(%%rdi)		\n\t	vmovaps	%%ymm15,0x20(%%rdi)			\n\t"/* Store B0 */\
\
		"vmovaps	    (%%rbx),%%ymm0	\n\t	vmovaps	0x20(%%rbx),%%ymm15	\n\t"/* t2 */\
	"vfmadd231pd %%ymm11,%%ymm0,%%ymm1	\n\t vfmadd231pd %%ymm11,%%ymm15,%%ymm6 	\n\t"/* c1 += cc2*t2; */\
	"vfmadd231pd %%ymm13,%%ymm0,%%ymm2	\n\t vfmadd231pd %%ymm13,%%ymm15,%%ymm7 	\n\t"/* c2 += cc4*t2; */\
	"vfmadd231pd %%ymm14,%%ymm0,%%ymm3	\n\t vfmadd231pd %%ymm14,%%ymm15,%%ymm8 	\n\t"/* c3 += cc5*t2; */\
	"vfmadd231pd %%ymm12,%%ymm0,%%ymm4	\n\t vfmadd231pd %%ymm12,%%ymm15,%%ymm9 	\n\t"/* c4 += cc3*t2; */\
	"vfmadd231pd (%%r15),%%ymm0,%%ymm5	\n\t vfmadd231pd (%%r15),%%ymm15,%%ymm10	\n\t"/* c5 += cc1*t2; */\
		"vaddpd (%%rdi),%%ymm0,%%ymm0	\n\t	vaddpd 0x20(%%rdi),%%ymm15,%%ymm15	\n\t"/* B0 += t2; */\
		"vmovaps	%%ymm0,(%%rdi)		\n\t	vmovaps	%%ymm15,0x20(%%rdi)			\n\t"/* Store B0 */\
\
		"vmovaps	    (%%rcx),%%ymm0	\n\t	vmovaps	0x20(%%rcx),%%ymm15	\n\t"/* t3 */\
	"vfmadd231pd %%ymm12,%%ymm0,%%ymm1	\n\t vfmadd231pd %%ymm12,%%ymm15,%%ymm6 	\n\t"/* c1 += cc3*t3; */\
	"vfmadd231pd %%ymm14,%%ymm0,%%ymm2	\n\t vfmadd231pd %%ymm14,%%ymm15,%%ymm7 	\n\t"/* c2 += cc5*t3; */\
	"vfmadd231pd %%ymm11,%%ymm0,%%ymm3	\n\t vfmadd231pd %%ymm11,%%ymm15,%%ymm8 	\n\t"/* c3 += cc2*t3; */\
	"vfmadd231pd (%%r15),%%ymm0,%%ymm4	\n\t vfmadd231pd (%%r15),%%ymm15,%%ymm9 	\n\t"/* c4 += cc1*t3; */\
	"vfmadd231pd %%ymm13,%%ymm0,%%ymm5	\n\t vfmadd231pd %%ymm13,%%ymm15,%%ymm10	\n\t"/* c5 += cc4*t3; */\
		"vaddpd (%%rdi),%%ymm0,%%ymm0	\n\t	vaddpd 0x20(%%rdi),%%ymm15,%%ymm15	\n\t"/* B0 += t3; */\
		"vmovaps	%%ymm0,(%%rdi)		\n\t	vmovaps	%%ymm15,0x20(%%rdi)			\n\t"/* Store B0 */\
\
		"vmovaps	    (%%rdx),%%ymm0	\n\t	vmovaps	0x20(%%rdx),%%ymm15	\n\t"/* t4 */\
	"vfmadd231pd %%ymm13,%%ymm0,%%ymm1	\n\t vfmadd231pd %%ymm13,%%ymm15,%%ymm6 	\n\t"/* c1 += cc4*t4; */\
	"vfmadd231pd %%ymm12,%%ymm0,%%ymm2	\n\t vfmadd231pd %%ymm12,%%ymm15,%%ymm7 	\n\t"/* c2 += cc3*t4; */\
	"vfmadd231pd (%%r15),%%ymm0,%%ymm3	\n\t vfmadd231pd (%%r15),%%ymm15,%%ymm8 	\n\t"/* c3 += cc1*t4; */\
	"vfmadd231pd %%ymm14,%%ymm0,%%ymm4	\n\t vfmadd231pd %%ymm14,%%ymm15,%%ymm9 	\n\t"/* c4 += cc5*t4; */\
	"vfmadd231pd %%ymm11,%%ymm0,%%ymm5	\n\t vfmadd231pd %%ymm11,%%ymm15,%%ymm10	\n\t"/* c5 += cc2*t4; */\
		"vaddpd (%%rdi),%%ymm0,%%ymm0	\n\t	vaddpd 0x20(%%rdi),%%ymm15,%%ymm15	\n\t"/* B0 += t4; */\
		"vmovaps	%%ymm0,(%%rdi)		\n\t	vmovaps	%%ymm15,0x20(%%rdi)			\n\t"/* Store B0 */\
\
		"vmovaps	    (%%rsi),%%ymm0	\n\t	vmovaps	0x20(%%rsi),%%ymm15	\n\t"/* t5 */\
	"vfmadd231pd %%ymm14,%%ymm0,%%ymm1	\n\t vfmadd231pd %%ymm14,%%ymm15,%%ymm6 	\n\t"/* c1 += cc5*t5; */\
	"vfmadd231pd (%%r15),%%ymm0,%%ymm2	\n\t vfmadd231pd (%%r15),%%ymm15,%%ymm7 	\n\t"/* c2 += cc1*t5; */\
	"vfmadd231pd %%ymm13,%%ymm0,%%ymm3	\n\t vfmadd231pd %%ymm13,%%ymm15,%%ymm8 	\n\t"/* c3 += cc4*t5; */\
	"vfmadd231pd %%ymm11,%%ymm0,%%ymm4	\n\t vfmadd231pd %%ymm11,%%ymm15,%%ymm9 	\n\t"/* c4 += cc2*t5; */\
	"vfmadd231pd %%ymm12,%%ymm0,%%ymm5	\n\t vfmadd231pd %%ymm12,%%ymm15,%%ymm10	\n\t"/* c5 += cc3*t5; */\
		"vaddpd (%%rdi),%%ymm0,%%ymm0	\n\t	vaddpd 0x20(%%rdi),%%ymm15,%%ymm15	\n\t"/* B0 += t5; */\
		"movq	%[__O0],%%rdi		\n\t"\
		"vmovaps	%%ymm0,(%%rdi)		\n\t	vmovaps	%%ymm15,0x20(%%rdi)			\n\t"/* Store B0, now into output slot */\
\
		"movq	%[__o6],%%rdi		\n\t"/* restore o-ptr rdi-value (r10-13 still hold &B7-A)) */\
		/* Have enough registers to also hold the real parts of S-terms: */\
		"vmovaps	(%%r13),%%ymm11		\n\t"/* s1r */"	movq	%[__o1],%%rax	\n\t"/* &B1 */\
		"vmovaps	(%%r12),%%ymm12		\n\t"/* s2r */"	movq	%[__o2],%%rbx	\n\t"/* &B2 */\
		"vmovaps	(%%r11),%%ymm13		\n\t"/* s3r */"	movq	%[__o3],%%rcx	\n\t"/* &B3 */\
		"vmovaps	(%%r10),%%ymm14		\n\t"/* s4r */"	movq	%[__o4],%%rdx	\n\t"/* &B4 */\
		"vmovaps	(%%rdi),%%ymm15		\n\t"/* s5r */"	movq	%[__o5],%%rsi	\n\t"/* &B5 */\
\
	/* ...Do the lower-half-index portion of the closing wave of radix-2 butterflies: */\
		"vmovaps	(%%r9),%%ymm0		\n\t"/* one */\
		"vfnmadd231pd 0x20(%%r13),%%ymm0,%%ymm1		\n\t	 vfmadd231pd %%ymm11,%%ymm0,%%ymm6 	\n\t"/* B1 = C1 + I*S1 */\
		"vfnmadd231pd 0x20(%%r12),%%ymm0,%%ymm2		\n\t	 vfmadd231pd %%ymm12,%%ymm0,%%ymm7 	\n\t"/* B2 = C2 + I*S2 */\
		"vfnmadd231pd 0x20(%%r11),%%ymm0,%%ymm3		\n\t	 vfmadd231pd %%ymm13,%%ymm0,%%ymm8 	\n\t"/* B3 = C3 + I*S3 */\
		"vfnmadd231pd 0x20(%%r10),%%ymm0,%%ymm4		\n\t	 vfmadd231pd %%ymm14,%%ymm0,%%ymm9 	\n\t"/* B4 = C4 + I*S4 */\
		"vfnmadd231pd 0x20(%%rdi),%%ymm0,%%ymm5		\n\t	 vfmadd231pd %%ymm15,%%ymm0,%%ymm10	\n\t"/* B5 = C5 + I*S5 */\
	/* Write lower-half outputs back to memory... */\
		"vmovaps	%%ymm1,(%%rax)		\n\t"/* B1r */"		vmovaps	%%ymm6 ,0x20(%%rax)	\n\t"/* B1i */\
		"vmovaps	%%ymm2,(%%rbx)		\n\t"/* B2r */"		vmovaps	%%ymm7 ,0x20(%%rbx)	\n\t"/* B2i */\
		"vmovaps	%%ymm3,(%%rcx)		\n\t"/* B3r */"		vmovaps	%%ymm8 ,0x20(%%rcx)	\n\t"/* B3i */\
		"vmovaps	%%ymm4,(%%rdx)		\n\t"/* B4r */"		vmovaps	%%ymm9 ,0x20(%%rdx)	\n\t"/* B4i */\
		"vmovaps	%%ymm5,(%%rsi)		\n\t"/* B5r */"		vmovaps	%%ymm10,0x20(%%rsi)	\n\t"/* B5i */\
	/* ...Do the upper-half-index portion of the radix-2 butterflies: */\
		"vmovaps	(%%r8),%%ymm0		\n\t"/* two */\
		" vfmadd231pd 0x20(%%r13),%%ymm0,%%ymm1		\n\t	vfnmadd231pd %%ymm11,%%ymm0,%%ymm6 	\n\t"/* B10= C1 - I*S1 */\
		" vfmadd231pd 0x20(%%r12),%%ymm0,%%ymm2		\n\t	vfnmadd231pd %%ymm12,%%ymm0,%%ymm7 	\n\t"/* B9 = C2 - I*S2 */\
		" vfmadd231pd 0x20(%%r11),%%ymm0,%%ymm3		\n\t	vfnmadd231pd %%ymm13,%%ymm0,%%ymm8 	\n\t"/* B8 = C3 - I*S3 */\
		" vfmadd231pd 0x20(%%r10),%%ymm0,%%ymm4		\n\t	vfnmadd231pd %%ymm14,%%ymm0,%%ymm9 	\n\t"/* B7 = C4 - I*S4 */\
		" vfmadd231pd 0x20(%%rdi),%%ymm0,%%ymm5		\n\t	vfnmadd231pd %%ymm15,%%ymm0,%%ymm10	\n\t"/* B6 = C5 - I*S5 */\
	/* ...And write the upper-half outputs back to memory. */\
		"vmovaps	%%ymm1,(%%r13)		\n\t"/* Bar */"		vmovaps	%%ymm6 ,0x20(%%r13)	\n\t"/* Bai */\
		"vmovaps	%%ymm2,(%%r12)		\n\t"/* B9r */"		vmovaps	%%ymm7 ,0x20(%%r12)	\n\t"/* B9i */\
		"vmovaps	%%ymm3,(%%r11)		\n\t"/* B8r */"		vmovaps	%%ymm8 ,0x20(%%r11)	\n\t"/* B8i */\
		"vmovaps	%%ymm4,(%%r10)		\n\t"/* B7r */"		vmovaps	%%ymm9 ,0x20(%%r10)	\n\t"/* B7i */\
		"vmovaps	%%ymm5,(%%rdi)		\n\t"/* B6r */"		vmovaps	%%ymm10,0x20(%%rdi)	\n\t"/* B6i */\
		:					/* outputs: none */\
		: [__I0] "m" (XI0)	/* All inputs from memory addresses here */\
		 ,[__i1] "m" (Xi1)\
		 ,[__i2] "m" (Xi2)\
		 ,[__i3] "m" (Xi3)\
		 ,[__i4] "m" (Xi4)\
		 ,[__i5] "m" (Xi5)\
		 ,[__i6] "m" (Xi6)\
		 ,[__i7] "m" (Xi7)\
		 ,[__i8] "m" (Xi8)\
		 ,[__i9] "m" (Xi9)\
		 ,[__iA] "m" (XiA)\
		 ,[__cc] "m" (Xcc)\
		 ,[__O0] "m" (XO0)\
		 ,[__o1] "m" (Xo1)\
		 ,[__o2] "m" (Xo2)\
		 ,[__o3] "m" (Xo3)\
		 ,[__o4] "m" (Xo4)\
		 ,[__o5] "m" (Xo5)\
		 ,[__o6] "m" (Xo6)\
		 ,[__o7] "m" (Xo7)\
		 ,[__o8] "m" (Xo8)\
		 ,[__o9] "m" (Xo9)\
		 ,[__oA] "m" (XoA)\
		: "cc","memory","rax","rbx","rcx","rdx","rsi","rdi","r8","r9","r10","r11","r12","r13","r15","xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7","xmm8","xmm9","xmm10","xmm11","xmm12","xmm13","xmm14","xmm15"		/* Clobbered registers */\
		);\
	}

#elif defined(USE_AVX)
	//
	// NB: GCC builds gave "Error: invalid character '(' in mnemonic" for this macro ... I'd forgotten
	// to restore whites to several ...pd (... pairings after having removed it for purposes of col-
	// alignment during AVX-ification of what started as the SSE2 version of the macro:
	//
	// ******> A good way to track down such non-obvious syntax errors output by the assembler (typically w/o
	// helpful localization information such as line numbers) is to insert a single asm line with a different
	// kind of asm error - e.g. a vxorpd with just 2 register args in this case - and move 1 or 2 such around
	// in order to ever-more-closely bracket the first error of the type whose origin is being sought.
	//
	// Arithmetic opcount: [182 ADD, 44 MUL), 154 memref], under our general target of 1 memref per vec_dbl arithmetic op.
	//
	#define SSE2_RADIX_11_DFT(XI0,Xi1,Xi2,Xi3,Xi4,Xi5,Xi6,Xi7,Xi8,Xi9,XiA, Xcc, XO0,Xo1,Xo2,Xo3,Xo4,Xo5,Xo6,Xo7,Xo8,Xo9,XoA)\
	{\
	__asm__ volatile (\
		"/********************************************/\n\t"\
		"/*       Here are the 5 cosine terms:       */\n\t"\
		"/********************************************/\n\t"\
		"movq	%[__I0],%%rax			\n\t			movq	%[__O0],%%rcx		/* rax/rcx point to Re parts of I/Os */	\n\t"\
		"leaq	0x20(%%rax),%%rbx		\n\t			leaq	0x20(%%rcx),%%rdx	/* rbx/rdx point to Im parts of I/Os */	\n\t"\
		"movq	%[__cc],%%rsi			\n\t"\
		"/*****************************/\n\t			/******************************/\n\t"\
		"/*        Real Parts:         /\n\t			/*     Imaginary Parts:        /\n\t"\
		"/*****************************/\n\t			/******************************/\n\t"\
	"movq	%[__i1],%%r8 	\n\t	vmovaps (%%r8 ),%%ymm1	\n\t	vmovaps	0x20(%%r8 ),%%ymm9 	\n\t"\
	"movq	%[__iA],%%r9 	\n\t	vmovaps (%%r9 ),%%ymm5	\n\t	vmovaps	0x20(%%r9 ),%%ymm13	\n\t"\
	"movq	%[__i2],%%r10	\n\t	vmovaps (%%r10),%%ymm2	\n\t	vmovaps	0x20(%%r10),%%ymm10	\n\t"\
	"movq	%[__i9],%%r11	\n\t	vmovaps (%%r11),%%ymm6	\n\t	vmovaps	0x20(%%r11),%%ymm14	\n\t"\
	"movq	%[__i3],%%r12	\n\t	vmovaps (%%r12),%%ymm3	\n\t	vmovaps	0x20(%%r12),%%ymm11	\n\t"\
	"movq	%[__i8],%%r13	\n\t	vmovaps (%%r13),%%ymm7	\n\t	vmovaps	0x20(%%r13),%%ymm15	\n\t"\
	"movq	%[__i4],%%r14	\n\t	vmovaps (%%r14),%%ymm4	\n\t	vmovaps	0x20(%%r14),%%ymm12	\n\t"\
	"movq	%[__i7],%%r15	\n\t	vmovaps (%%r15),%%ymm0	\n\t	vmovaps	0x20(%%r15),%%ymm8 	\n\t"\
		"vsubpd	%%ymm5,%%ymm1,%%ymm1	\n\t			vsubpd	%%ymm13,%%ymm9 ,%%ymm9 			\n\t"\
		"vsubpd	%%ymm6,%%ymm2,%%ymm2	\n\t			vsubpd	%%ymm14,%%ymm10,%%ymm10			\n\t"\
		"vsubpd	%%ymm7,%%ymm3,%%ymm3	\n\t			vsubpd	%%ymm15,%%ymm11,%%ymm11			\n\t"\
		"vsubpd	%%ymm0,%%ymm4,%%ymm4	\n\t			vsubpd	%%ymm8 ,%%ymm12,%%ymm12			\n\t"\
	"movq	%[__oA],%%r8 	\n\t	vmovaps %%ymm1,0x20(%%r8 )	\n\t	vmovaps %%ymm9 ,(%%r8 )	\n\t"\
	"movq	%[__o9],%%r9 	\n\t	vmovaps %%ymm2,0x20(%%r9 )	\n\t	vmovaps %%ymm10,(%%r9 )	\n\t"\
	"movq	%[__o8],%%r10	\n\t	vmovaps %%ymm3,0x20(%%r10)	\n\t	vmovaps %%ymm11,(%%r10)	\n\t"\
	"movq	%[__o7],%%r11	\n\t	vmovaps %%ymm4,0x20(%%r11)	\n\t	vmovaps %%ymm12,(%%r11)	\n\t"\
		"vaddpd	%%ymm5,%%ymm5,%%ymm5	\n\t			vaddpd	%%ymm13,%%ymm13,%%ymm13			\n\t"\
		"vaddpd	%%ymm6,%%ymm6,%%ymm6	\n\t			vaddpd	%%ymm14,%%ymm14,%%ymm14			\n\t"\
		"vaddpd	%%ymm7,%%ymm7,%%ymm7	\n\t			vaddpd	%%ymm15,%%ymm15,%%ymm15			\n\t"\
		"vaddpd	%%ymm0,%%ymm0,%%ymm0	\n\t			vaddpd	%%ymm8 ,%%ymm8 ,%%ymm8 			\n\t"\
		"vaddpd	%%ymm5,%%ymm1,%%ymm1	\n\t			vaddpd	%%ymm13,%%ymm9 ,%%ymm9 			\n\t"\
		"vaddpd	%%ymm6,%%ymm2,%%ymm2	\n\t			vaddpd	%%ymm14,%%ymm10,%%ymm10			\n\t"\
	"movq	%[__i5],%%r9 	\n\t	vmovaps (%%r9 ),%%ymm5	\n\t	vmovaps 0x20(%%r9 ),%%ymm13	\n\t"\
	"movq	%[__i6],%%r11	\n\t	vmovaps (%%r11),%%ymm6	\n\t	vmovaps 0x20(%%r11),%%ymm14	\n\t"\
		"vaddpd	%%ymm7,%%ymm3,%%ymm3	\n\t			vaddpd	%%ymm15,%%ymm11,%%ymm11			\n\t"\
		"vaddpd	%%ymm0,%%ymm4,%%ymm4	\n\t			vaddpd	%%ymm8 ,%%ymm12,%%ymm12			\n\t"\
		"vsubpd	%%ymm6,%%ymm5,%%ymm5	\n\t			vsubpd	%%ymm14,%%ymm13,%%ymm13			\n\t"\
	"movq	%[__o6],%%r8 	\n\t	vmovaps %%ymm5,0x20(%%r8 )	\n\t	vmovaps %%ymm13,(%%r8 )	\n\t"\
		"vaddpd	%%ymm6,%%ymm6,%%ymm6	\n\t			vaddpd	%%ymm14,%%ymm14,%%ymm14			\n\t"\
		"vmovaps	(%%rax),%%ymm0		\n\t			vmovaps	(%%rbx),%%ymm8 					\n\t"\
		"vaddpd	%%ymm6,%%ymm5,%%ymm5	\n\t			vaddpd	%%ymm14,%%ymm13,%%ymm13			\n\t"\
	"movq	%[__o1],%%r11	\n\t"\
	"movq	%[__o2],%%r12	\n\t"\
	"movq	%[__o3],%%r13	\n\t"\
	"movq	%[__o4],%%r14	\n\t"\
	"movq	%[__o5],%%r15	\n\t"\
		"vsubpd	%%ymm2,%%ymm1,%%ymm1			\n\t	vsubpd	%%ymm10,%%ymm9 ,%%ymm9 			\n\t"\
		"vsubpd	%%ymm2,%%ymm5,%%ymm5			\n\t	vsubpd	%%ymm10,%%ymm13,%%ymm13			\n\t"\
		"vmovaps	%%ymm1,%%ymm6				\n\t	vmovaps	%%ymm9 ,%%ymm14					\n\t"\
		"vsubpd	%%ymm2,%%ymm3,%%ymm3			\n\t	vsubpd	%%ymm10,%%ymm11,%%ymm11			\n\t"\
		"vmovaps	%%ymm1,%%ymm7				\n\t	vmovaps	%%ymm9 ,%%ymm15					\n\t"\
		"vmulpd	      (%%rsi),%%ymm1,%%ymm1		\n\t	vmulpd	      (%%rsi),%%ymm9 ,%%ymm9 	\n\t"\
		"vmovaps	%%ymm1,(%%r11)				\n\t	vmovaps	%%ymm9 ,0x20(%%r11)				\n\t"\
		"vsubpd	%%ymm2,%%ymm4,%%ymm4			\n\t	vsubpd	%%ymm10,%%ymm12,%%ymm12			\n\t"\
		"vmulpd	-0x020(%%rsi),%%ymm2,%%ymm2		\n\t	vmulpd	-0x020(%%rsi),%%ymm10,%%ymm10	\n\t"\
		"vaddpd	%%ymm5,%%ymm6,%%ymm6			\n\t	vaddpd	%%ymm13,%%ymm14,%%ymm14			\n\t"\
		"vsubpd	%%ymm3,%%ymm7,%%ymm7			\n\t	vsubpd	%%ymm11,%%ymm15,%%ymm15			\n\t"\
		"vmulpd	 0x0c0(%%rsi),%%ymm7,%%ymm7		\n\t	vmulpd	 0x0c0(%%rsi),%%ymm15,%%ymm15	\n\t"\
		"vmovaps	%%ymm7,(%%r13)				\n\t	vmovaps	%%ymm15,0x20(%%r13)				\n\t"\
		"vmovaps	%%ymm3,%%ymm1				\n\t	vmovaps	%%ymm11,%%ymm9 					\n\t"\
		"vaddpd	%%ymm4,%%ymm3,%%ymm3			\n\t	vaddpd	%%ymm12,%%ymm11,%%ymm11			\n\t"\
		"vmulpd	 0x060(%%rsi),%%ymm1,%%ymm1		\n\t	vmulpd	 0x060(%%rsi),%%ymm9 ,%%ymm9 	\n\t"\
		"vmovaps	%%ymm1,(%%r12)				\n\t	vmovaps	%%ymm9 ,0x20(%%r12)				\n\t"\
		"vmovaps	%%ymm5,%%ymm7				\n\t	vmovaps	%%ymm13,%%ymm15					\n\t"\
		"vsubpd	%%ymm4,%%ymm5,%%ymm5			\n\t	vsubpd	%%ymm12,%%ymm13,%%ymm13			\n\t"\
		"vmulpd	 0x080(%%rsi),%%ymm4,%%ymm4		\n\t	vmulpd	 0x080(%%rsi),%%ymm12,%%ymm12	\n\t"\
		"vmulpd	 0x0e0(%%rsi),%%ymm5,%%ymm5		\n\t	vmulpd	 0x0e0(%%rsi),%%ymm13,%%ymm13	\n\t"\
		"vmulpd	 0x020(%%rsi),%%ymm7,%%ymm7		\n\t	vmulpd	 0x020(%%rsi),%%ymm15,%%ymm15	\n\t"\
		"vaddpd	%%ymm3,%%ymm2,%%ymm2			\n\t	vaddpd	%%ymm11,%%ymm10,%%ymm10			\n\t"\
		"vmovaps	%%ymm6,%%ymm1				\n\t	vmovaps	%%ymm14,%%ymm9 					\n\t"\
		"vsubpd	%%ymm3,%%ymm6,%%ymm6			\n\t	vsubpd	%%ymm11,%%ymm14,%%ymm14			\n\t"\
		"vmulpd	 0x100(%%rsi),%%ymm6,%%ymm6		\n\t	vmulpd	 0x100(%%rsi),%%ymm14,%%ymm14	\n\t"\
		"vmulpd	 0x0a0(%%rsi),%%ymm3,%%ymm3		\n\t	vmulpd	 0x0a0(%%rsi),%%ymm11,%%ymm11	\n\t"\
		"vaddpd	%%ymm1,%%ymm2,%%ymm2			\n\t	vaddpd	%%ymm9 ,%%ymm10,%%ymm10			\n\t"\
		"vmulpd	 0x040(%%rsi),%%ymm1,%%ymm1		\n\t	vmulpd	 0x040(%%rsi),%%ymm9 ,%%ymm9 	\n\t"\
		"vaddpd	%%ymm2,%%ymm0,%%ymm0			\n\t	vaddpd	%%ymm10,%%ymm8 ,%%ymm8 			\n\t"\
		"vmulpd	 0x120(%%rsi),%%ymm2,%%ymm2		\n\t	vmulpd	 0x120(%%rsi),%%ymm10,%%ymm10	\n\t"\
		"vmovaps	%%ymm0,(%%rcx)				\n\t	vmovaps	%%ymm8 ,(%%rdx)					\n\t"\
		"vaddpd	%%ymm0,%%ymm2,%%ymm2			\n\t	vaddpd	%%ymm8 ,%%ymm10,%%ymm10			\n\t"\
		"vaddpd	%%ymm1,%%ymm7,%%ymm7			\n\t	vaddpd	%%ymm9 ,%%ymm15,%%ymm15			\n\t"\
		"vaddpd	(%%r11),%%ymm1,%%ymm1			\n\t	vaddpd	0x20(%%r11),%%ymm9 ,%%ymm9 		\n\t"\
		"vaddpd	%%ymm3,%%ymm4,%%ymm4			\n\t	vaddpd	%%ymm11,%%ymm12,%%ymm12			\n\t"\
		"vaddpd	(%%r12),%%ymm3,%%ymm3			\n\t	vaddpd	0x20(%%r12),%%ymm11,%%ymm11		\n\t"\
		"vaddpd	%%ymm6,%%ymm5,%%ymm5			\n\t	vaddpd	%%ymm14,%%ymm13,%%ymm13			\n\t"\
		"vaddpd	(%%r13),%%ymm6,%%ymm6			\n\t	vaddpd	0x20(%%r13),%%ymm14,%%ymm14		\n\t"\
		"vmovaps	%%ymm2,%%ymm0				\n\t	vmovaps	%%ymm10,%%ymm8 					\n\t"\
		"vsubpd	%%ymm1,%%ymm2,%%ymm2			\n\t	vsubpd	%%ymm9 ,%%ymm10,%%ymm10			\n\t"\
		"vaddpd	%%ymm0,%%ymm1,%%ymm1			\n\t	vaddpd	%%ymm8 ,%%ymm9 ,%%ymm9 			\n\t"\
		"vsubpd	%%ymm7,%%ymm2,%%ymm2			\n\t	vsubpd	%%ymm15,%%ymm10,%%ymm10			\n\t"\
		"vaddpd	%%ymm0,%%ymm7,%%ymm7			\n\t	vaddpd	%%ymm8 ,%%ymm15,%%ymm15			\n\t"\
		"vsubpd	%%ymm6,%%ymm1,%%ymm1			\n\t	vsubpd	%%ymm14,%%ymm9 ,%%ymm9 			\n\t"\
		"vsubpd	%%ymm3,%%ymm2,%%ymm2			\n\t	vsubpd	%%ymm11,%%ymm10,%%ymm10			\n\t"\
		"vmovaps	%%ymm1,(%%r11)				\n\t	vmovaps	%%ymm9 ,0x20(%%r11)				\n\t"\
		"vaddpd	%%ymm0,%%ymm3,%%ymm3			\n\t	vaddpd	%%ymm8 ,%%ymm11,%%ymm11			\n\t"\
		"vsubpd	%%ymm5,%%ymm7,%%ymm7			\n\t	vsubpd	%%ymm13,%%ymm15,%%ymm15			\n\t"\
		"vsubpd	%%ymm4,%%ymm2,%%ymm2			\n\t	vsubpd	%%ymm12,%%ymm10,%%ymm10			\n\t"\
		"vmovaps	%%ymm7,(%%r15)				\n\t	vmovaps	%%ymm15,0x20(%%r15)				\n\t"\
		"vmovaps	%%ymm2,(%%r12)				\n\t	vmovaps	%%ymm10,0x20(%%r12)				\n\t"\
		"vaddpd	%%ymm0,%%ymm4,%%ymm4			\n\t	vaddpd	%%ymm8 ,%%ymm12,%%ymm12			\n\t"\
		"vaddpd	%%ymm6,%%ymm3,%%ymm3			\n\t	vaddpd	%%ymm14,%%ymm11,%%ymm11			\n\t"\
		"vaddpd	%%ymm5,%%ymm4,%%ymm4			\n\t	vaddpd	%%ymm13,%%ymm12,%%ymm12			\n\t"\
		"vmovaps	%%ymm3,(%%r13)				\n\t	vmovaps	%%ymm11,0x20(%%r13)				\n\t"\
		"vmovaps	%%ymm4,(%%r14)				\n\t	vmovaps	%%ymm12,0x20(%%r14)				\n\t"\
	"/***********************************************************************************************************/\n\t"\
	"/* Here are 5 sine terms: Similar to cosines, but t21,19,17,15,13 replace t3,5,7,9,11 and some sign flips: */\n\t"\
	"/***********************************************************************************************************/\n\t"\
		"addq	$0x140,%%rsi				\n\t"\
		"/*****************************/\n\t			/******************************/\n\t"\
		"/*        Real Parts:         /\n\t			/*     Imaginary Parts:        /\n\t"\
		"/*****************************/\n\t			/******************************/\n\t"\
	"movq	%[__oA],%%r11	\n\t	vmovaps (%%r11),%%ymm1	\n\t	vmovaps 0x20(%%r11),%%ymm9 	\n\t"\
	"movq	%[__o9],%%r12	\n\t	vmovaps (%%r12),%%ymm2	\n\t	vmovaps 0x20(%%r12),%%ymm10	\n\t"\
	"movq	%[__o8],%%r13	\n\t	vmovaps (%%r13),%%ymm3	\n\t	vmovaps 0x20(%%r13),%%ymm11	\n\t"\
	"movq	%[__o7],%%r14	\n\t	vmovaps (%%r14),%%ymm4	\n\t	vmovaps 0x20(%%r14),%%ymm12	\n\t"\
	"movq	%[__o6],%%r15	\n\t	vmovaps (%%r15),%%ymm5	\n\t	vmovaps 0x20(%%r15),%%ymm13	\n\t"\
		"vaddpd	%%ymm2,%%ymm1,%%ymm1			\n\t	vaddpd	%%ymm10,%%ymm9 ,%%ymm9 			\n\t"\
		"vaddpd	%%ymm2,%%ymm5,%%ymm5			\n\t	vaddpd	%%ymm10,%%ymm13,%%ymm13			\n\t"\
		"vmovaps	%%ymm1,%%ymm6				\n\t	vmovaps	%%ymm9 ,%%ymm14					\n\t"\
		"vaddpd	%%ymm2,%%ymm3,%%ymm3			\n\t	vaddpd	%%ymm10,%%ymm11,%%ymm11			\n\t"\
		"vmovaps	%%ymm1,%%ymm7				\n\t	vmovaps	%%ymm9 ,%%ymm15					\n\t"\
		"vmulpd	      (%%rsi),%%ymm1,%%ymm1		\n\t	vmulpd	      (%%rsi),%%ymm9 ,%%ymm9 	\n\t"\
		"vmovaps	%%ymm1,(%%r15)				\n\t	vmovaps	%%ymm9 ,0x20(%%r15)				\n\t"\
		"vaddpd	%%ymm2,%%ymm4,%%ymm4			\n\t	vaddpd	%%ymm10,%%ymm12,%%ymm12			\n\t"\
		"vmulpd	-0x160(%%rsi),%%ymm2,%%ymm2		\n\t	vmulpd	-0x160(%%rsi),%%ymm10,%%ymm10	\n\t"\
		"vaddpd	%%ymm5,%%ymm6,%%ymm6			\n\t	vaddpd	%%ymm13,%%ymm14,%%ymm14			\n\t"\
		"vsubpd	%%ymm3,%%ymm7,%%ymm7			\n\t	vsubpd	%%ymm11,%%ymm15,%%ymm15			\n\t"\
		"vmulpd	 0x0c0(%%rsi),%%ymm7,%%ymm7		\n\t	vmulpd	 0x0c0(%%rsi),%%ymm15,%%ymm15	\n\t"\
		"vmovaps	%%ymm7,(%%r13)				\n\t	vmovaps	%%ymm15,0x20(%%r13)				\n\t"\
		"vmovaps	%%ymm3,%%ymm1				\n\t	vmovaps	%%ymm11,%%ymm9 					\n\t"\
		"vaddpd	%%ymm4,%%ymm3,%%ymm3			\n\t	vaddpd	%%ymm12,%%ymm11,%%ymm11			\n\t"\
		"vmulpd	 0x060(%%rsi),%%ymm1,%%ymm1		\n\t	vmulpd	 0x060(%%rsi),%%ymm9 ,%%ymm9 	\n\t"\
		"vmovaps	%%ymm1,(%%r14)				\n\t	vmovaps	%%ymm9 ,0x20(%%r14)				\n\t"\
		"vmovaps	%%ymm5,%%ymm7				\n\t	vmovaps	%%ymm13,%%ymm15					\n\t"\
		"vsubpd	%%ymm4,%%ymm5,%%ymm5			\n\t	vsubpd	%%ymm12,%%ymm13,%%ymm13			\n\t"\
		"vmulpd	 0x080(%%rsi),%%ymm4,%%ymm4		\n\t	vmulpd	 0x080(%%rsi),%%ymm12,%%ymm12	\n\t"\
		"vmulpd	 0x0e0(%%rsi),%%ymm5,%%ymm5		\n\t	vmulpd	 0x0e0(%%rsi),%%ymm13,%%ymm13	\n\t"\
		"vmulpd	 0x020(%%rsi),%%ymm7,%%ymm7		\n\t	vmulpd	 0x020(%%rsi),%%ymm15,%%ymm15	\n\t"\
		"vsubpd	%%ymm3,%%ymm2,%%ymm2			\n\t	vsubpd	%%ymm11,%%ymm10,%%ymm10			\n\t"\
		"vmovaps	%%ymm6,%%ymm1				\n\t	vmovaps	%%ymm14,%%ymm9 					\n\t"\
		"vsubpd	%%ymm3,%%ymm6,%%ymm6			\n\t	vsubpd	%%ymm11,%%ymm14,%%ymm14			\n\t"\
		"vmulpd	 0x100(%%rsi),%%ymm6,%%ymm6		\n\t	vmulpd	 0x100(%%rsi),%%ymm14,%%ymm14	\n\t"\
		"vmulpd	 0x0a0(%%rsi),%%ymm3,%%ymm3		\n\t	vmulpd	 0x0a0(%%rsi),%%ymm11,%%ymm11	\n\t"\
		"vsubpd	%%ymm1,%%ymm2,%%ymm2			\n\t	vsubpd	%%ymm9 ,%%ymm10,%%ymm10			\n\t"\
		"vmulpd	 0x040(%%rsi),%%ymm1,%%ymm1		\n\t	vmulpd	 0x040(%%rsi),%%ymm9 ,%%ymm9 	\n\t"\
		"vmulpd	 0x120(%%rsi),%%ymm2,%%ymm2		\n\t	vmulpd	 0x120(%%rsi),%%ymm10,%%ymm10	\n\t"\
		"vaddpd	%%ymm1,%%ymm7,%%ymm7			\n\t	vaddpd	%%ymm9 ,%%ymm15,%%ymm15			\n\t"\
		"vaddpd	(%%r15),%%ymm1,%%ymm1			\n\t	vaddpd	0x20(%%r15),%%ymm9 ,%%ymm9 		\n\t"\
		"vaddpd	%%ymm3,%%ymm4,%%ymm4			\n\t	vaddpd	%%ymm11,%%ymm12,%%ymm12			\n\t"\
		"vaddpd	(%%r14),%%ymm3,%%ymm3			\n\t	vaddpd	0x20(%%r14),%%ymm11,%%ymm11		\n\t"\
		"vaddpd	%%ymm6,%%ymm5,%%ymm5			\n\t	vaddpd	%%ymm14,%%ymm13,%%ymm13			\n\t"\
		"vaddpd	(%%r13),%%ymm6,%%ymm6			\n\t	vaddpd	0x20(%%r13),%%ymm14,%%ymm14		\n\t"\
		"vxorpd	%%ymm0,%%ymm0,%%ymm0			\n\t	vxorpd	%%ymm8 ,%%ymm8 ,%%ymm8			\n\t"\
		"vsubpd	%%ymm2,%%ymm0,%%ymm0			\n\t	vsubpd	%%ymm10,%%ymm8 ,%%ymm8 			\n\t"\
		"vaddpd	%%ymm1,%%ymm0,%%ymm0			\n\t	vaddpd	%%ymm9 ,%%ymm8 ,%%ymm8 			\n\t"\
		"vaddpd	%%ymm2,%%ymm1,%%ymm1			\n\t	vaddpd	%%ymm10,%%ymm9 ,%%ymm9 			\n\t"\
		"vaddpd	%%ymm7,%%ymm0,%%ymm0			\n\t	vaddpd	%%ymm15,%%ymm8 ,%%ymm8 			\n\t"\
		"vaddpd	%%ymm2,%%ymm7,%%ymm7			\n\t	vaddpd	%%ymm10,%%ymm15,%%ymm15			\n\t"\
		"vsubpd	%%ymm6,%%ymm1,%%ymm1			\n\t	vsubpd	%%ymm14,%%ymm9 ,%%ymm9 			\n\t"\
		"vaddpd	%%ymm3,%%ymm0,%%ymm0			\n\t	vaddpd	%%ymm11,%%ymm8 ,%%ymm8 			\n\t"\
		"vaddpd	%%ymm2,%%ymm3,%%ymm3			\n\t	vaddpd	%%ymm10,%%ymm11,%%ymm11			\n\t"\
		"vsubpd	%%ymm5,%%ymm7,%%ymm7			\n\t	vsubpd	%%ymm13,%%ymm15,%%ymm15			\n\t"\
		"vaddpd	%%ymm4,%%ymm0,%%ymm0			\n\t	vaddpd	%%ymm12,%%ymm8 ,%%ymm8 			\n\t"\
		"vaddpd	%%ymm2,%%ymm4,%%ymm4			\n\t	vaddpd	%%ymm10,%%ymm12,%%ymm12			\n\t"\
		"vaddpd	%%ymm6,%%ymm3,%%ymm3			\n\t	vaddpd	%%ymm14,%%ymm11,%%ymm11			\n\t"\
		"vaddpd	%%ymm5,%%ymm4,%%ymm4			\n\t	vaddpd	%%ymm13,%%ymm12,%%ymm12			\n\t"\
		"vmovaps	%%ymm1,%%ymm2				\n\t	vmovaps	%%ymm9 ,%%ymm10					\n\t"\
	"movq	%[__o1],%%r8 					\n\t"\
		"vaddpd	(%%r8 ),%%ymm1,%%ymm1			\n\t	vaddpd	0x20(%%r8 ),%%ymm9 ,%%ymm9 		\n\t"\
		"vaddpd	%%ymm2,%%ymm2,%%ymm2			\n\t	vaddpd	%%ymm10,%%ymm10,%%ymm10			\n\t"\
		"vmovaps	%%ymm1,(%%r11)				\n\t	vmovaps	%%ymm9 ,0x20(%%r8 )				\n\t"\
		"vsubpd	%%ymm2,%%ymm1,%%ymm1			\n\t	vsubpd	%%ymm10,%%ymm9 ,%%ymm9 			\n\t"\
		"vmovaps	%%ymm1,(%%r8 )				\n\t	vmovaps	%%ymm9 ,0x20(%%r11)				\n\t"\
		"vmovaps	%%ymm0,%%ymm5				\n\t	vmovaps	%%ymm8 ,%%ymm13					\n\t"\
	"movq	%[__o2],%%r8 					\n\t"\
		"vaddpd	(%%r8 ),%%ymm0,%%ymm0			\n\t	vaddpd	0x20(%%r8 ),%%ymm8 ,%%ymm8 		\n\t"\
		"vaddpd	%%ymm5,%%ymm5,%%ymm5			\n\t	vaddpd	%%ymm13,%%ymm13,%%ymm13			\n\t"\
		"vmovaps	%%ymm0,(%%r12)				\n\t	vmovaps	%%ymm8 ,0x20(%%r8 )				\n\t"\
		"vsubpd	%%ymm5,%%ymm0,%%ymm0			\n\t	vsubpd	%%ymm13,%%ymm8 ,%%ymm8 			\n\t"\
		"vmovaps	%%ymm0,(%%r8 )				\n\t	vmovaps	%%ymm8 ,0x20(%%r12)				\n\t"\
		"vmovaps	%%ymm3,%%ymm6				\n\t	vmovaps	%%ymm11,%%ymm14					\n\t"\
	"movq	%[__o3],%%r8 					\n\t"\
		"vaddpd	(%%r8 ),%%ymm3,%%ymm3			\n\t	vaddpd	0x20(%%r8 ),%%ymm11,%%ymm11		\n\t"\
		"vaddpd	%%ymm6,%%ymm6,%%ymm6			\n\t	vaddpd	%%ymm14,%%ymm14,%%ymm14			\n\t"\
		"vmovaps	%%ymm3,(%%r13)				\n\t	vmovaps	%%ymm11,0x20(%%r8 )				\n\t"\
		"vsubpd	%%ymm6,%%ymm3,%%ymm3			\n\t	vsubpd	%%ymm14,%%ymm11,%%ymm11			\n\t"\
		"vmovaps	%%ymm3,(%%r8 )				\n\t	vmovaps	%%ymm11,0x20(%%r13)				\n\t"\
		"vmovaps	%%ymm4,%%ymm2				\n\t	vmovaps	%%ymm12,%%ymm10					\n\t"\
	"movq	%[__o4],%%r8 					\n\t"\
		"vaddpd	(%%r8 ),%%ymm4,%%ymm4			\n\t	vaddpd	0x20(%%r8 ),%%ymm12,%%ymm12		\n\t"\
		"vaddpd	%%ymm2,%%ymm2,%%ymm2			\n\t	vaddpd	%%ymm10,%%ymm10,%%ymm10			\n\t"\
		"vmovaps	%%ymm4,(%%r14)				\n\t	vmovaps	%%ymm12,0x20(%%r8 )				\n\t"\
		"vsubpd	%%ymm2,%%ymm4,%%ymm4			\n\t	vsubpd	%%ymm10,%%ymm12,%%ymm12			\n\t"\
		"vmovaps	%%ymm4,(%%r8 )				\n\t	vmovaps	%%ymm12,0x20(%%r14)				\n\t"\
		"vmovaps	%%ymm7,%%ymm5				\n\t	vmovaps	%%ymm15,%%ymm13					\n\t"\
	"movq	%[__o5],%%r8 					\n\t"\
		"vaddpd	(%%r8 ),%%ymm7,%%ymm7			\n\t	vaddpd	0x20(%%r8 ),%%ymm15,%%ymm15		\n\t"\
		"vaddpd	%%ymm5,%%ymm5,%%ymm5			\n\t	vaddpd	%%ymm13,%%ymm13,%%ymm13			\n\t"\
		"vmovaps	%%ymm7,(%%r15)				\n\t	vmovaps	%%ymm15,0x20(%%r8 )				\n\t"\
		"vsubpd	%%ymm5,%%ymm7,%%ymm7			\n\t	vsubpd	%%ymm13,%%ymm15,%%ymm15			\n\t"\
		"vmovaps	%%ymm7,(%%r8 )				\n\t	vmovaps	%%ymm15,0x20(%%r15)				\n\t"\
		:					/* outputs: none */\
		: [__I0] "m" (XI0)	/* All inputs from memory addresses here */\
		 ,[__i1] "m" (Xi1)\
		 ,[__i2] "m" (Xi2)\
		 ,[__i3] "m" (Xi3)\
		 ,[__i4] "m" (Xi4)\
		 ,[__i5] "m" (Xi5)\
		 ,[__i6] "m" (Xi6)\
		 ,[__i7] "m" (Xi7)\
		 ,[__i8] "m" (Xi8)\
		 ,[__i9] "m" (Xi9)\
		 ,[__iA] "m" (XiA)\
		 ,[__cc] "m" (Xcc)\
		 ,[__O0] "m" (XO0)\
		 ,[__o1] "m" (Xo1)\
		 ,[__o2] "m" (Xo2)\
		 ,[__o3] "m" (Xo3)\
		 ,[__o4] "m" (Xo4)\
		 ,[__o5] "m" (Xo5)\
		 ,[__o6] "m" (Xo6)\
		 ,[__o7] "m" (Xo7)\
		 ,[__o8] "m" (Xo8)\
		 ,[__o9] "m" (Xo9)\
		 ,[__oA] "m" (XoA)\
		: "cc","memory","rax","rbx","rcx","rdx","rsi","r8","r9","r10","r11","r12","r13","r14","r15","xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7","xmm8","xmm9","xmm10","xmm11","xmm12","xmm13","xmm14","xmm15"		/* Clobbered registers */\
		);\
	}

#elif defined(USE_SSE2) && (OS_BITS == 64)

  #define USE_64BIT_ASM_STYLE	1	// My x86 timings indicate 16-reg version of radix-11 DFT is faster

  #if USE_64BIT_ASM_STYLE

	// 2-Instruction-stream-overlapped 64-bit-ified version of the 32-bit-style ASM macro, using all of xmm0-15
	#define SSE2_RADIX_11_DFT(XI0,Xi1,Xi2,Xi3,Xi4,Xi5,Xi6,Xi7,Xi8,Xi9,XiA, Xcc, XO0,Xo1,Xo2,Xo3,Xo4,Xo5,Xo6,Xo7,Xo8,Xo9,XoA)\
	{\
	__asm__ volatile (\
		"/********************************************/\n\t"\
		"/*       Here are the 5 cosine terms:       */\n\t"\
		"/********************************************/\n\t"\
		"movq	%[__I0],%%rax			\n\t			movq	%[__O0],%%rcx		/* rax/rcx point to Re parts of I/Os */	\n\t"\
		"leaq	0x10(%%rax),%%rbx		\n\t			leaq	0x10(%%rcx),%%rdx	/* rbx/rdx point to Im parts of I/Os */	\n\t"\
		"movq	%[__cc],%%rsi			\n\t"\
		"/*****************************/\n\t			/******************************/\n\t"\
		"/*        Real Parts:         /\n\t			/*     Imaginary Parts:        /\n\t"\
		"/*****************************/\n\t			/******************************/\n\t"\
	"movq	%[__i1],%%r8 	\n\t	movaps (%%r8 ),%%xmm1	\n\t	movaps	0x10(%%r8 ),%%xmm9 	\n\t"\
	"movq	%[__iA],%%r9 	\n\t	movaps (%%r9 ),%%xmm5	\n\t	movaps	0x10(%%r9 ),%%xmm13	\n\t"\
	"movq	%[__i2],%%r10	\n\t	movaps (%%r10),%%xmm2	\n\t	movaps	0x10(%%r10),%%xmm10	\n\t"\
	"movq	%[__i9],%%r11	\n\t	movaps (%%r11),%%xmm6	\n\t	movaps	0x10(%%r11),%%xmm14	\n\t"\
	"movq	%[__i3],%%r12	\n\t	movaps (%%r12),%%xmm3	\n\t	movaps	0x10(%%r12),%%xmm11	\n\t"\
	"movq	%[__i8],%%r13	\n\t	movaps (%%r13),%%xmm7	\n\t	movaps	0x10(%%r13),%%xmm15	\n\t"\
	"movq	%[__i4],%%r14	\n\t	movaps (%%r14),%%xmm4	\n\t	movaps	0x10(%%r14),%%xmm12	\n\t"\
	"movq	%[__i7],%%r15	\n\t	movaps (%%r15),%%xmm0	\n\t	movaps	0x10(%%r15),%%xmm8 	\n\t"\
		"subpd	%%xmm5,%%xmm1			\n\t			subpd	%%xmm13,%%xmm9 			\n\t"\
		"subpd	%%xmm6,%%xmm2			\n\t			subpd	%%xmm14,%%xmm10			\n\t"\
		"subpd	%%xmm7,%%xmm3			\n\t			subpd	%%xmm15,%%xmm11			\n\t"\
		"subpd	%%xmm0,%%xmm4			\n\t			subpd	%%xmm8 ,%%xmm12			\n\t"\
	"movq	%[__oA],%%r8 	\n\t	movaps %%xmm1,0x10(%%r8 )	\n\t	movaps %%xmm9 ,(%%r8 )	\n\t"\
	"movq	%[__o9],%%r9 	\n\t	movaps %%xmm2,0x10(%%r9 )	\n\t	movaps %%xmm10,(%%r9 )	\n\t"\
	"movq	%[__o8],%%r10	\n\t	movaps %%xmm3,0x10(%%r10)	\n\t	movaps %%xmm11,(%%r10)	\n\t"\
	"movq	%[__o7],%%r11	\n\t	movaps %%xmm4,0x10(%%r11)	\n\t	movaps %%xmm12,(%%r11)	\n\t"\
		"addpd	%%xmm5,%%xmm5			\n\t			addpd	%%xmm13,%%xmm13			\n\t"\
		"addpd	%%xmm6,%%xmm6			\n\t			addpd	%%xmm14,%%xmm14			\n\t"\
		"addpd	%%xmm7,%%xmm7			\n\t			addpd	%%xmm15,%%xmm15			\n\t"\
		"addpd	%%xmm0,%%xmm0			\n\t			addpd	%%xmm8 ,%%xmm8 			\n\t"\
		"addpd	%%xmm5,%%xmm1			\n\t			addpd	%%xmm13,%%xmm9 			\n\t"\
		"addpd	%%xmm6,%%xmm2			\n\t			addpd	%%xmm14,%%xmm10			\n\t"\
	"movq	%[__i5],%%r9 	\n\t	movaps (%%r9 ),%%xmm5	\n\t	movaps 0x10(%%r9 ),%%xmm13	\n\t"\
	"movq	%[__i6],%%r11	\n\t	movaps (%%r11),%%xmm6	\n\t	movaps 0x10(%%r11),%%xmm14	\n\t"\
		"addpd	%%xmm7,%%xmm3			\n\t			addpd	%%xmm15,%%xmm11			\n\t"\
		"addpd	%%xmm0,%%xmm4			\n\t			addpd	%%xmm8 ,%%xmm12			\n\t"\
		"subpd	%%xmm6,%%xmm5			\n\t			subpd	%%xmm14,%%xmm13			\n\t"\
	"movq	%[__o6],%%r8 	\n\t	movaps %%xmm5,0x10(%%r8 )	\n\t	movaps %%xmm13,(%%r8 )	\n\t"\
		"addpd	%%xmm6,%%xmm6			\n\t			addpd	%%xmm14,%%xmm14			\n\t"\
		"movaps	(%%rax),%%xmm0			\n\t			movaps	(%%rbx),%%xmm8 			\n\t"\
		"addpd	%%xmm6,%%xmm5			\n\t			addpd	%%xmm14,%%xmm13			\n\t"\
	"movq	%[__o1],%%r11	\n\t"\
	"movq	%[__o2],%%r12	\n\t"\
	"movq	%[__o3],%%r13	\n\t"\
	"movq	%[__o4],%%r14	\n\t"\
	"movq	%[__o5],%%r15	\n\t"\
		"subpd	%%xmm2,%%xmm1			\n\t			subpd	%%xmm10,%%xmm9 			\n\t"\
		"subpd	%%xmm2,%%xmm5			\n\t			subpd	%%xmm10,%%xmm13			\n\t"\
		"movaps	%%xmm1,%%xmm6			\n\t			movaps	%%xmm9 ,%%xmm14			\n\t"\
		"subpd	%%xmm2,%%xmm3			\n\t			subpd	%%xmm10,%%xmm11			\n\t"\
		"movaps	%%xmm1,%%xmm7			\n\t			movaps	%%xmm9 ,%%xmm15			\n\t"\
		"mulpd	     (%%rsi),%%xmm1		\n\t			mulpd	     (%%rsi),%%xmm9 	\n\t"\
		"movaps	%%xmm1,(%%r11)			\n\t			movaps	%%xmm9 ,0x10(%%r11)		\n\t"\
		"subpd	%%xmm2,%%xmm4			\n\t			subpd	%%xmm10,%%xmm12			\n\t"\
		"mulpd	-0x10(%%rsi),%%xmm2		\n\t			mulpd	-0x10(%%rsi),%%xmm10	\n\t"\
		"addpd	%%xmm5,%%xmm6			\n\t			addpd	%%xmm13,%%xmm14			\n\t"\
		"subpd	%%xmm3,%%xmm7			\n\t			subpd	%%xmm11,%%xmm15			\n\t"\
		"mulpd	 0x60(%%rsi),%%xmm7		\n\t			mulpd	 0x60(%%rsi),%%xmm15	\n\t"\
		"movaps	%%xmm7,(%%r13)			\n\t			movaps	%%xmm15,0x10(%%r13)		\n\t"\
		"movaps	%%xmm3,%%xmm1			\n\t			movaps	%%xmm11,%%xmm9 			\n\t"\
		"addpd	%%xmm4,%%xmm3			\n\t			addpd	%%xmm12,%%xmm11			\n\t"\
		"mulpd	 0x30(%%rsi),%%xmm1		\n\t			mulpd	 0x30(%%rsi),%%xmm9 	\n\t"\
		"movaps	%%xmm1,(%%r12)			\n\t			movaps	%%xmm9 ,0x10(%%r12)		\n\t"\
		"movaps	%%xmm5,%%xmm7			\n\t			movaps	%%xmm13,%%xmm15			\n\t"\
		"subpd	%%xmm4,%%xmm5			\n\t			subpd	%%xmm12,%%xmm13			\n\t"\
		"mulpd	 0x40(%%rsi),%%xmm4		\n\t			mulpd	 0x40(%%rsi),%%xmm12	\n\t"\
		"mulpd	 0x70(%%rsi),%%xmm5		\n\t			mulpd	 0x70(%%rsi),%%xmm13	\n\t"\
		"mulpd	 0x10(%%rsi),%%xmm7		\n\t			mulpd	 0x10(%%rsi),%%xmm15	\n\t"\
		"addpd	%%xmm3,%%xmm2			\n\t			addpd	%%xmm11,%%xmm10			\n\t"\
		"movaps	%%xmm6,%%xmm1			\n\t			movaps	%%xmm14,%%xmm9 			\n\t"\
		"subpd	%%xmm3,%%xmm6			\n\t			subpd	%%xmm11,%%xmm14			\n\t"\
		"mulpd	 0x80(%%rsi),%%xmm6		\n\t			mulpd	 0x80(%%rsi),%%xmm14	\n\t"\
		"mulpd	 0x50(%%rsi),%%xmm3		\n\t			mulpd	 0x50(%%rsi),%%xmm11	\n\t"\
		"addpd	%%xmm1,%%xmm2			\n\t			addpd	%%xmm9 ,%%xmm10			\n\t"\
		"mulpd	 0x20(%%rsi),%%xmm1		\n\t			mulpd	 0x20(%%rsi),%%xmm9 	\n\t"\
		"addpd	%%xmm2,%%xmm0			\n\t			addpd	%%xmm10,%%xmm8 			\n\t"\
		"mulpd	 0x90(%%rsi),%%xmm2		\n\t			mulpd	 0x90(%%rsi),%%xmm10	\n\t"\
		"movaps	%%xmm0,(%%rcx)			\n\t			movaps	%%xmm8 ,(%%rdx)			\n\t"\
		"addpd	%%xmm0,%%xmm2			\n\t			addpd	%%xmm8 ,%%xmm10			\n\t"\
		"addpd	%%xmm1,%%xmm7			\n\t			addpd	%%xmm9 ,%%xmm15			\n\t"\
		"addpd	(%%r11),%%xmm1			\n\t			addpd	0x10(%%r11),%%xmm9 		\n\t"\
		"addpd	%%xmm3,%%xmm4			\n\t			addpd	%%xmm11,%%xmm12			\n\t"\
		"addpd	(%%r12),%%xmm3			\n\t			addpd	0x10(%%r12),%%xmm11		\n\t"\
		"addpd	%%xmm6,%%xmm5			\n\t			addpd	%%xmm14,%%xmm13			\n\t"\
		"addpd	(%%r13),%%xmm6			\n\t			addpd	0x10(%%r13),%%xmm14		\n\t"\
		"movaps	%%xmm2,%%xmm0			\n\t			movaps	%%xmm10,%%xmm8 			\n\t"\
		"subpd	%%xmm1,%%xmm2			\n\t			subpd	%%xmm9 ,%%xmm10			\n\t"\
		"addpd	%%xmm0,%%xmm1			\n\t			addpd	%%xmm8 ,%%xmm9 			\n\t"\
		"subpd	%%xmm7,%%xmm2			\n\t			subpd	%%xmm15,%%xmm10			\n\t"\
		"addpd	%%xmm0,%%xmm7			\n\t			addpd	%%xmm8 ,%%xmm15			\n\t"\
		"subpd	%%xmm6,%%xmm1			\n\t			subpd	%%xmm14,%%xmm9 			\n\t"\
		"subpd	%%xmm3,%%xmm2			\n\t			subpd	%%xmm11,%%xmm10			\n\t"\
		"movaps	%%xmm1,(%%r11)			\n\t			movaps	%%xmm9 ,0x10(%%r11)		\n\t"\
		"addpd	%%xmm0,%%xmm3			\n\t			addpd	%%xmm8 ,%%xmm11			\n\t"\
		"subpd	%%xmm5,%%xmm7			\n\t			subpd	%%xmm13,%%xmm15			\n\t"\
		"subpd	%%xmm4,%%xmm2			\n\t			subpd	%%xmm12,%%xmm10			\n\t"\
		"movaps	%%xmm7,(%%r15)			\n\t			movaps	%%xmm15,0x10(%%r15)		\n\t"\
		"movaps	%%xmm2,(%%r12)			\n\t			movaps	%%xmm10,0x10(%%r12)		\n\t"\
		"addpd	%%xmm0,%%xmm4			\n\t			addpd	%%xmm8 ,%%xmm12			\n\t"\
		"addpd	%%xmm6,%%xmm3			\n\t			addpd	%%xmm14,%%xmm11			\n\t"\
		"addpd	%%xmm5,%%xmm4			\n\t			addpd	%%xmm13,%%xmm12			\n\t"\
		"movaps	%%xmm3,(%%r13)			\n\t			movaps	%%xmm11,0x10(%%r13)		\n\t"\
		"movaps	%%xmm4,(%%r14)			\n\t			movaps	%%xmm12,0x10(%%r14)		\n\t"\
	"/***********************************************************************************************************/\n\t"\
	"/* Here are 5 sine terms: Similar to cosines, but t21,19,17,15,13 replace t3,5,7,9,11 and some sign flips: */\n\t"\
	"/***********************************************************************************************************/\n\t"\
		"addq	$0xa0,%%rsi				\n\t"\
		"/*****************************/\n\t			/******************************/\n\t"\
		"/*        Real Parts:         /\n\t			/*     Imaginary Parts:        /\n\t"\
		"/*****************************/\n\t			/******************************/\n\t"\
	"movq	%[__oA],%%r11	\n\t	movaps (%%r11),%%xmm1	\n\t	movaps 0x10(%%r11),%%xmm9 	\n\t"\
	"movq	%[__o9],%%r12	\n\t	movaps (%%r12),%%xmm2	\n\t	movaps 0x10(%%r12),%%xmm10	\n\t"\
	"movq	%[__o8],%%r13	\n\t	movaps (%%r13),%%xmm3	\n\t	movaps 0x10(%%r13),%%xmm11	\n\t"\
	"movq	%[__o7],%%r14	\n\t	movaps (%%r14),%%xmm4	\n\t	movaps 0x10(%%r14),%%xmm12	\n\t"\
	"movq	%[__o6],%%r15	\n\t	movaps (%%r15),%%xmm5	\n\t	movaps 0x10(%%r15),%%xmm13	\n\t"\
		"addpd	%%xmm2,%%xmm1			\n\t			addpd	%%xmm10,%%xmm9 			\n\t"\
		"addpd	%%xmm2,%%xmm5			\n\t			addpd	%%xmm10,%%xmm13			\n\t"\
		"movaps	%%xmm1,%%xmm6			\n\t			movaps	%%xmm9 ,%%xmm14			\n\t"\
		"addpd	%%xmm2,%%xmm3			\n\t			addpd	%%xmm10,%%xmm11			\n\t"\
		"movaps	%%xmm1,%%xmm7			\n\t			movaps	%%xmm9 ,%%xmm15			\n\t"\
		"mulpd	     (%%rsi),%%xmm1		\n\t			mulpd	     (%%rsi),%%xmm9 	\n\t"\
		"movaps	%%xmm1,(%%r15)			\n\t			movaps	%%xmm9 ,0x10(%%r15)		\n\t"\
		"addpd	%%xmm2,%%xmm4			\n\t			addpd	%%xmm10,%%xmm12			\n\t"\
		"mulpd	-0xb0(%%rsi),%%xmm2		\n\t			mulpd	-0xb0(%%rsi),%%xmm10	\n\t"\
		"addpd	%%xmm5,%%xmm6			\n\t			addpd	%%xmm13,%%xmm14			\n\t"\
		"subpd	%%xmm3,%%xmm7			\n\t			subpd	%%xmm11,%%xmm15			\n\t"\
		"mulpd	 0x60(%%rsi),%%xmm7		\n\t			mulpd	 0x60(%%rsi),%%xmm15	\n\t"\
		"movaps	%%xmm7,(%%r13)			\n\t			movaps	%%xmm15,0x10(%%r13)		\n\t"\
		"movaps	%%xmm3,%%xmm1			\n\t			movaps	%%xmm11,%%xmm9 			\n\t"\
		"addpd	%%xmm4,%%xmm3			\n\t			addpd	%%xmm12,%%xmm11			\n\t"\
		"mulpd	 0x30(%%rsi),%%xmm1		\n\t			mulpd	 0x30(%%rsi),%%xmm9 	\n\t"\
		"movaps	%%xmm1,(%%r14)			\n\t			movaps	%%xmm9 ,0x10(%%r14)		\n\t"\
		"movaps	%%xmm5,%%xmm7			\n\t			movaps	%%xmm13,%%xmm15			\n\t"\
		"subpd	%%xmm4,%%xmm5			\n\t			subpd	%%xmm12,%%xmm13			\n\t"\
		"mulpd	 0x40(%%rsi),%%xmm4		\n\t			mulpd	 0x40(%%rsi),%%xmm12	\n\t"\
		"mulpd	 0x70(%%rsi),%%xmm5		\n\t			mulpd	 0x70(%%rsi),%%xmm13	\n\t"\
		"mulpd	 0x10(%%rsi),%%xmm7		\n\t			mulpd	 0x10(%%rsi),%%xmm15	\n\t"\
		"subpd	%%xmm3,%%xmm2			\n\t			subpd	%%xmm11,%%xmm10			\n\t"\
		"movaps	%%xmm6,%%xmm1			\n\t			movaps	%%xmm14,%%xmm9 			\n\t"\
		"subpd	%%xmm3,%%xmm6			\n\t			subpd	%%xmm11,%%xmm14			\n\t"\
		"mulpd	 0x80(%%rsi),%%xmm6		\n\t			mulpd	 0x80(%%rsi),%%xmm14	\n\t"\
		"mulpd	 0x50(%%rsi),%%xmm3		\n\t			mulpd	 0x50(%%rsi),%%xmm11	\n\t"\
		"subpd	%%xmm1,%%xmm2			\n\t			subpd	%%xmm9 ,%%xmm10			\n\t"\
		"mulpd	 0x20(%%rsi),%%xmm1		\n\t			mulpd	 0x20(%%rsi),%%xmm9 	\n\t"\
		"mulpd	 0x90(%%rsi),%%xmm2		\n\t			mulpd	 0x90(%%rsi),%%xmm10	\n\t"\
		"addpd	%%xmm1,%%xmm7			\n\t			addpd	%%xmm9 ,%%xmm15			\n\t"\
		"addpd	(%%r15),%%xmm1			\n\t			addpd	0x10(%%r15),%%xmm9 		\n\t"\
		"addpd	%%xmm3,%%xmm4			\n\t			addpd	%%xmm11,%%xmm12			\n\t"\
		"addpd	(%%r14),%%xmm3			\n\t			addpd	0x10(%%r14),%%xmm11		\n\t"\
		"addpd	%%xmm6,%%xmm5			\n\t			addpd	%%xmm14,%%xmm13			\n\t"\
		"addpd	(%%r13),%%xmm6			\n\t			addpd	0x10(%%r13),%%xmm14		\n\t"\
		"xorpd	%%xmm0,%%xmm0			\n\t			xorpd	%%xmm8 ,%%xmm8 			\n\t"\
		"subpd	%%xmm2,%%xmm0			\n\t			subpd	%%xmm10,%%xmm8 			\n\t"\
		"addpd	%%xmm1,%%xmm0			\n\t			addpd	%%xmm9 ,%%xmm8 			\n\t"\
		"addpd	%%xmm2,%%xmm1			\n\t			addpd	%%xmm10,%%xmm9 			\n\t"\
		"addpd	%%xmm7,%%xmm0			\n\t			addpd	%%xmm15,%%xmm8 			\n\t"\
		"addpd	%%xmm2,%%xmm7			\n\t			addpd	%%xmm10,%%xmm15			\n\t"\
		"subpd	%%xmm6,%%xmm1			\n\t			subpd	%%xmm14,%%xmm9 			\n\t"\
		"addpd	%%xmm3,%%xmm0			\n\t			addpd	%%xmm11,%%xmm8 			\n\t"\
		"addpd	%%xmm2,%%xmm3			\n\t			addpd	%%xmm10,%%xmm11			\n\t"\
		"subpd	%%xmm5,%%xmm7			\n\t			subpd	%%xmm13,%%xmm15			\n\t"\
		"addpd	%%xmm4,%%xmm0			\n\t			addpd	%%xmm12,%%xmm8 			\n\t"\
		"addpd	%%xmm2,%%xmm4			\n\t			addpd	%%xmm10,%%xmm12			\n\t"\
		"addpd	%%xmm6,%%xmm3			\n\t			addpd	%%xmm14,%%xmm11			\n\t"\
		"addpd	%%xmm5,%%xmm4			\n\t			addpd	%%xmm13,%%xmm12			\n\t"\
		"movaps	%%xmm1,%%xmm2			\n\t			movaps	%%xmm9 ,%%xmm10			\n\t"\
	"movq	%[__o1],%%r8 	\n\t"\
		"addpd	(%%r8 ),%%xmm1			\n\t			addpd	0x10(%%r8 ),%%xmm9 		\n\t"\
		"addpd	%%xmm2,%%xmm2			\n\t			addpd	%%xmm10,%%xmm10			\n\t"\
		"movaps	%%xmm1,(%%r11)			\n\t			movaps	%%xmm9 ,0x10(%%r8 )		\n\t"\
		"subpd	%%xmm2,%%xmm1			\n\t			subpd	%%xmm10,%%xmm9 			\n\t"\
		"movaps	%%xmm1,(%%r8 )			\n\t			movaps	%%xmm9 ,0x10(%%r11)		\n\t"\
		"movaps	%%xmm0,%%xmm5			\n\t			movaps	%%xmm8 ,%%xmm13			\n\t"\
	"movq	%[__o2],%%r8 	\n\t"\
		"addpd	(%%r8 ),%%xmm0			\n\t			addpd	0x10(%%r8 ),%%xmm8 		\n\t"\
		"addpd	%%xmm5,%%xmm5			\n\t			addpd	%%xmm13,%%xmm13			\n\t"\
		"movaps	%%xmm0,(%%r12)			\n\t			movaps	%%xmm8 ,0x10(%%r8 )		\n\t"\
		"subpd	%%xmm5,%%xmm0			\n\t			subpd	%%xmm13,%%xmm8 			\n\t"\
		"movaps	%%xmm0,(%%r8 )			\n\t			movaps	%%xmm8 ,0x10(%%r12)		\n\t"\
		"movaps	%%xmm3,%%xmm6			\n\t			movaps	%%xmm11,%%xmm14			\n\t"\
	"movq	%[__o3],%%r8 	\n\t"\
		"addpd	(%%r8 ),%%xmm3			\n\t			addpd	0x10(%%r8 ),%%xmm11		\n\t"\
		"addpd	%%xmm6,%%xmm6			\n\t			addpd	%%xmm14,%%xmm14			\n\t"\
		"movaps	%%xmm3,(%%r13)			\n\t			movaps	%%xmm11,0x10(%%r8 )		\n\t"\
		"subpd	%%xmm6,%%xmm3			\n\t			subpd	%%xmm14,%%xmm11			\n\t"\
		"movaps	%%xmm3,(%%r8 )			\n\t			movaps	%%xmm11,0x10(%%r13)		\n\t"\
		"movaps	%%xmm4,%%xmm2			\n\t			movaps	%%xmm12,%%xmm10			\n\t"\
	"movq	%[__o4],%%r8 	\n\t"\
		"addpd	(%%r8 ),%%xmm4			\n\t			addpd	0x10(%%r8 ),%%xmm12		\n\t"\
		"addpd	%%xmm2,%%xmm2			\n\t			addpd	%%xmm10,%%xmm10			\n\t"\
		"movaps	%%xmm4,(%%r14)			\n\t			movaps	%%xmm12,0x10(%%r8 )		\n\t"\
		"subpd	%%xmm2,%%xmm4			\n\t			subpd	%%xmm10,%%xmm12			\n\t"\
		"movaps	%%xmm4,(%%r8 )			\n\t			movaps	%%xmm12,0x10(%%r14)		\n\t"\
		"movaps	%%xmm7,%%xmm5			\n\t			movaps	%%xmm15,%%xmm13			\n\t"\
	"movq	%[__o5],%%r8 	\n\t"\
		"addpd	(%%r8 ),%%xmm7			\n\t			addpd	0x10(%%r8 ),%%xmm15		\n\t"\
		"addpd	%%xmm5,%%xmm5			\n\t			addpd	%%xmm13,%%xmm13			\n\t"\
		"movaps	%%xmm7,(%%r15)			\n\t			movaps	%%xmm15,0x10(%%r8 )		\n\t"\
		"subpd	%%xmm5,%%xmm7			\n\t			subpd	%%xmm13,%%xmm15			\n\t"\
		"movaps	%%xmm7,(%%r8 )			\n\t			movaps	%%xmm15,0x10(%%r15)		\n\t"\
		:					/* outputs: none */\
		: [__I0] "m" (XI0)	/* All inputs from memory addresses here */\
		 ,[__i1] "m" (Xi1)\
		 ,[__i2] "m" (Xi2)\
		 ,[__i3] "m" (Xi3)\
		 ,[__i4] "m" (Xi4)\
		 ,[__i5] "m" (Xi5)\
		 ,[__i6] "m" (Xi6)\
		 ,[__i7] "m" (Xi7)\
		 ,[__i8] "m" (Xi8)\
		 ,[__i9] "m" (Xi9)\
		 ,[__iA] "m" (XiA)\
		 ,[__cc] "m" (Xcc)\
		 ,[__O0] "m" (XO0)\
		 ,[__o1] "m" (Xo1)\
		 ,[__o2] "m" (Xo2)\
		 ,[__o3] "m" (Xo3)\
		 ,[__o4] "m" (Xo4)\
		 ,[__o5] "m" (Xo5)\
		 ,[__o6] "m" (Xo6)\
		 ,[__o7] "m" (Xo7)\
		 ,[__o8] "m" (Xo8)\
		 ,[__o9] "m" (Xo9)\
		 ,[__oA] "m" (XoA)\
		: "cc","memory","rax","rbx","rcx","rdx","rsi","r8","r9","r10","r11","r12","r13","r14","r15","xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7","xmm8","xmm9","xmm10","xmm11","xmm12","xmm13","xmm14","xmm15"		/* Clobbered registers */\
		);\
	}

  #else // USE_64BIT_ASM_STYLE = false:

	// Simple 64-bit-ified version of the 32-bit ASM macro, using just xmm0-7
	#define SSE2_RADIX_11_DFT(XI0,Xi1,Xi2,Xi3,Xi4,Xi5,Xi6,Xi7,Xi8,Xi9,XiA, Xcc, XO0,Xo1,Xo2,Xo3,Xo4,Xo5,Xo6,Xo7,Xo8,Xo9,XoA)\
	{\
	__asm__ volatile (\
		"/********************************************/\n\t"\
		"/*       Here are the 5 cosine terms:       */\n\t"\
		"/********************************************/\n\t"\
		"movq	%[__I0],%%rax			\n\t"\
		"movq	%[__cc],%%rbx			\n\t"\
		"movq	%[__O0],%%rcx			\n\t"\
	"movq	%[__i1],%%r8 	\n\t	movaps (%%r8 ),%%xmm1	\n\t"\
	"movq	%[__iA],%%r9 	\n\t	movaps (%%r9 ),%%xmm5	\n\t"\
	"movq	%[__i2],%%r10	\n\t	movaps (%%r10),%%xmm2	\n\t"\
	"movq	%[__i9],%%r11	\n\t	movaps (%%r11),%%xmm6	\n\t"\
	"movq	%[__i3],%%r12	\n\t	movaps (%%r12),%%xmm3	\n\t"\
	"movq	%[__i8],%%r13	\n\t	movaps (%%r13),%%xmm7	\n\t"\
	"movq	%[__i4],%%r14	\n\t	movaps (%%r14),%%xmm4	\n\t"\
	"movq	%[__i7],%%r15	\n\t	movaps (%%r15),%%xmm0	\n\t"\
		/*addq	$0x10,%%rcx	<*** This is so following temp-outs go into Im-slots of o-array; now add 0x10 to __o* addresses to make that happen */\
		"subpd	%%xmm5,%%xmm1			\n\t"\
		"subpd	%%xmm6,%%xmm2			\n\t"\
		"subpd	%%xmm7,%%xmm3			\n\t"\
		"subpd	%%xmm0,%%xmm4			\n\t"\
	"movq	%[__oA],%%r8 	\n\t	movaps %%xmm1,0x10(%%r8 )	\n\t"\
	"movq	%[__o9],%%r9 	\n\t	movaps %%xmm2,0x10(%%r9 )	\n\t"\
	"movq	%[__o8],%%r10	\n\t	movaps %%xmm3,0x10(%%r10)	\n\t"\
	"movq	%[__o7],%%r11	\n\t	movaps %%xmm4,0x10(%%r11)	\n\t"\
		"addpd	%%xmm5,%%xmm5			\n\t"\
		"addpd	%%xmm6,%%xmm6			\n\t"\
		"addpd	%%xmm7,%%xmm7			\n\t"\
		"addpd	%%xmm0,%%xmm0			\n\t"\
		"addpd	%%xmm5,%%xmm1			\n\t"\
		"addpd	%%xmm6,%%xmm2			\n\t"\
	"movq	%[__i5],%%r9 	\n\t	movaps (%%r9 ),%%xmm5	\n\t"\
	"movq	%[__i6],%%r11	\n\t	movaps (%%r11),%%xmm6	\n\t"\
		"addpd	%%xmm7,%%xmm3			\n\t"\
		"addpd	%%xmm0,%%xmm4			\n\t"\
		"subpd	%%xmm6,%%xmm5			\n\t"\
	"movq	%[__o6],%%r8 	\n\t	movaps %%xmm5,0x10(%%r8 )	\n\t"\
		"addpd	%%xmm6,%%xmm6			\n\t"\
		"movaps	(%%rax),%%xmm0			\n\t"\
		"addpd	%%xmm6,%%xmm5			\n\t"\
		"/********************************************/\n\t"\
		"/*               Real Parts:                */\n\t"\
		"/********************************************/\n\t"\
	"movq	%[__o1],%%r11	\n\t"\
	"movq	%[__o2],%%r12	\n\t"\
	"movq	%[__o3],%%r13	\n\t"\
	"movq	%[__o4],%%r14	\n\t"\
	"movq	%[__o5],%%r15	\n\t"\
		/*subq	$0x10,%%rcx <*** Back to working with Re o-array slots */\
		"subpd	%%xmm2,%%xmm1			\n\t"\
		"subpd	%%xmm2,%%xmm5			\n\t"\
		"movaps	%%xmm1,%%xmm6			\n\t"\
		"subpd	%%xmm2,%%xmm3			\n\t"\
		"movaps	%%xmm1,%%xmm7			\n\t"\
		"mulpd	     (%%rbx),%%xmm1		\n\t"\
		"movaps	%%xmm1,(%%r11)	\n\t"\
		"subpd	%%xmm2,%%xmm4			\n\t"\
		"mulpd	-0x10(%%rbx),%%xmm2		\n\t"\
		"addpd	%%xmm5,%%xmm6			\n\t"\
		"subpd	%%xmm3,%%xmm7			\n\t"\
		"mulpd	 0x60(%%rbx),%%xmm7		\n\t"\
		"movaps	%%xmm7,(%%r13)	\n\t"\
		"movaps	%%xmm3,%%xmm1			\n\t"\
		"addpd	%%xmm4,%%xmm3			\n\t"\
		"mulpd	 0x30(%%rbx),%%xmm1		\n\t"\
		"movaps	%%xmm1,(%%r12)	\n\t"\
		"movaps	%%xmm5,%%xmm7			\n\t"\
		"subpd	%%xmm4,%%xmm5			\n\t"\
		"mulpd	 0x40(%%rbx),%%xmm4		\n\t"\
		"mulpd	 0x70(%%rbx),%%xmm5		\n\t"\
		"mulpd	 0x10(%%rbx),%%xmm7		\n\t"\
		"addpd	%%xmm3,%%xmm2			\n\t"\
		"movaps	%%xmm6,%%xmm1			\n\t"\
		"subpd	%%xmm3,%%xmm6			\n\t"\
		"mulpd	 0x80(%%rbx),%%xmm6		\n\t"\
		"mulpd	 0x50(%%rbx),%%xmm3		\n\t"\
		"addpd	%%xmm1,%%xmm2			\n\t"\
		"mulpd	 0x20(%%rbx),%%xmm1		\n\t"\
		"addpd	%%xmm2,%%xmm0			\n\t"\
		"mulpd	 0x90(%%rbx),%%xmm2		\n\t"\
		"movaps	%%xmm0,(%%rcx)			\n\t"\
		"addpd	%%xmm0,%%xmm2			\n\t"\
		"addpd	%%xmm1,%%xmm7			\n\t"\
		"addpd	(%%r11),%%xmm1	\n\t"\
		"addpd	%%xmm3,%%xmm4			\n\t"\
		"addpd	(%%r12),%%xmm3	\n\t"\
		"addpd	%%xmm6,%%xmm5			\n\t"\
		"addpd	(%%r13),%%xmm6	\n\t"\
		"movaps	%%xmm2,%%xmm0			\n\t"\
		"subpd	%%xmm1,%%xmm2			\n\t"\
		"addpd	%%xmm0,%%xmm1			\n\t"\
		"subpd	%%xmm7,%%xmm2			\n\t"\
		"addpd	%%xmm0,%%xmm7			\n\t"\
		"subpd	%%xmm6,%%xmm1			\n\t"\
		"subpd	%%xmm3,%%xmm2			\n\t"\
		"movaps	%%xmm1,(%%r11)	\n\t"\
		"addpd	%%xmm0,%%xmm3			\n\t"\
		"subpd	%%xmm5,%%xmm7			\n\t"\
		"subpd	%%xmm4,%%xmm2			\n\t"\
		"movaps	%%xmm7,(%%r15)	\n\t"\
		"movaps	%%xmm2,(%%r12)	\n\t"\
		"addpd	%%xmm0,%%xmm4			\n\t"\
		"addpd	%%xmm6,%%xmm3			\n\t"\
		"addpd	%%xmm5,%%xmm4			\n\t"\
		"movaps	%%xmm3,(%%r13)	\n\t"\
		"movaps	%%xmm4,(%%r14)	\n\t"\
		"/********************************************/\n\t"\
		"/*          Imaginary Parts:                */\n\t"\
		"/********************************************/\n\t"\
		/*addq	$0x10,%%rax	<*** ensuing block works with Im i-data */\
	"movq	%[__i1],%%r8 	\n\t	movaps 0x10(%%r8 ),%%xmm1	\n\t"\
	"movq	%[__iA],%%r9 	\n\t	movaps 0x10(%%r9 ),%%xmm5	\n\t"\
	"movq	%[__i2],%%r10	\n\t	movaps 0x10(%%r10),%%xmm2	\n\t"\
	"movq	%[__i9],%%r11	\n\t	movaps 0x10(%%r11),%%xmm6	\n\t"\
	"movq	%[__i3],%%r12	\n\t	movaps 0x10(%%r12),%%xmm3	\n\t"\
	"movq	%[__i8],%%r13	\n\t	movaps 0x10(%%r13),%%xmm7	\n\t"\
	"movq	%[__i4],%%r14	\n\t	movaps 0x10(%%r14),%%xmm4	\n\t"\
	"movq	%[__i7],%%r15	\n\t	movaps 0x10(%%r15),%%xmm0	\n\t"\
		"subpd	%%xmm5,%%xmm1			\n\t"\
		"subpd	%%xmm6,%%xmm2			\n\t"\
		"subpd	%%xmm7,%%xmm3			\n\t"\
		"subpd	%%xmm0,%%xmm4			\n\t"\
		/* Now this spill quartet is into Re slots of o-array: */\
	"movq	%[__oA],%%r8 	\n\t	movaps %%xmm1,(%%r8 )	\n\t"\
	"movq	%[__o9],%%r9 	\n\t	movaps %%xmm2,(%%r9 )	\n\t"\
	"movq	%[__o8],%%r10	\n\t	movaps %%xmm3,(%%r10)	\n\t"\
	"movq	%[__o7],%%r11	\n\t	movaps %%xmm4,(%%r11)	\n\t"\
		"addpd	%%xmm5,%%xmm5			\n\t"\
		"addpd	%%xmm6,%%xmm6			\n\t"\
		"addpd	%%xmm7,%%xmm7			\n\t"\
		"addpd	%%xmm0,%%xmm0			\n\t"\
		"addpd	%%xmm5,%%xmm1			\n\t"\
		"addpd	%%xmm6,%%xmm2			\n\t"\
	"movq	%[__i5],%%r9 	\n\t	movaps 0x10(%%r9 ),%%xmm5	\n\t"\
	"movq	%[__i6],%%r11	\n\t	movaps 0x10(%%r11),%%xmm6	\n\t"\
		"addpd	%%xmm7,%%xmm3			\n\t"\
		"addpd	%%xmm0,%%xmm4			\n\t"\
		"subpd	%%xmm6,%%xmm5			\n\t"\
	"movq	%[__o6],%%r8 	\n\t	movaps %%xmm5,(%%r8 )	\n\t"\
		"addpd	%%xmm6,%%xmm6			\n\t"\
		"movaps	0x10(%%rax),%%xmm0			\n\t"\
		"addpd	%%xmm6,%%xmm5			\n\t"\
		/*addq	$0x10,%%rcx	 <*** switch to Im slots of o-array: */\
	"movq	%[__o1],%%r11	\n\t"\
	"movq	%[__o2],%%r12	\n\t"\
	"movq	%[__o3],%%r13	\n\t"\
	"movq	%[__o4],%%r14	\n\t"\
	"movq	%[__o5],%%r15	\n\t"\
		"subpd	%%xmm2,%%xmm1			\n\t"\
		"subpd	%%xmm2,%%xmm5			\n\t"\
		"movaps	%%xmm1,%%xmm6			\n\t"\
		"subpd	%%xmm2,%%xmm3			\n\t"\
		"movaps	%%xmm1,%%xmm7			\n\t"\
		"mulpd	     (%%rbx),%%xmm1		\n\t"\
		"movaps	%%xmm1,0x10(%%r11)	\n\t"\
		"subpd	%%xmm2,%%xmm4			\n\t"\
		"mulpd	-0x10(%%rbx),%%xmm2		\n\t"\
		"addpd	%%xmm5,%%xmm6			\n\t"\
		"subpd	%%xmm3,%%xmm7			\n\t"\
		"mulpd	 0x60(%%rbx),%%xmm7		\n\t"\
		"movaps	%%xmm7,0x10(%%r13)	\n\t"\
		"movaps	%%xmm3,%%xmm1			\n\t"\
		"addpd	%%xmm4,%%xmm3			\n\t"\
		"mulpd	 0x30(%%rbx),%%xmm1		\n\t"\
		"movaps	%%xmm1,0x10(%%r12)	\n\t"\
		"movaps	%%xmm5,%%xmm7			\n\t"\
		"subpd	%%xmm4,%%xmm5			\n\t"\
		"mulpd	 0x40(%%rbx),%%xmm4		\n\t"\
		"mulpd	 0x70(%%rbx),%%xmm5		\n\t"\
		"mulpd	 0x10(%%rbx),%%xmm7		\n\t"\
		"addpd	%%xmm3,%%xmm2			\n\t"\
		"movaps	%%xmm6,%%xmm1			\n\t"\
		"subpd	%%xmm3,%%xmm6			\n\t"\
		"mulpd	 0x80(%%rbx),%%xmm6		\n\t"\
		"mulpd	 0x50(%%rbx),%%xmm3		\n\t"\
		"addpd	%%xmm1,%%xmm2			\n\t"\
		"mulpd	 0x20(%%rbx),%%xmm1		\n\t"\
		"addpd	%%xmm2,%%xmm0			\n\t"\
		"mulpd	 0x90(%%rbx),%%xmm2		\n\t"\
		"movaps	%%xmm0,0x10(%%rcx)			\n\t"\
		"addpd	%%xmm0,%%xmm2			\n\t"\
		"addpd	%%xmm1,%%xmm7			\n\t"\
		"addpd	0x10(%%r11),%%xmm1	\n\t"\
		"addpd	%%xmm3,%%xmm4			\n\t"\
		"addpd	0x10(%%r12),%%xmm3	\n\t"\
		"addpd	%%xmm6,%%xmm5			\n\t"\
		"addpd	0x10(%%r13),%%xmm6	\n\t"\
		"movaps	%%xmm2,%%xmm0			\n\t"\
		"subpd	%%xmm1,%%xmm2			\n\t"\
		"addpd	%%xmm0,%%xmm1			\n\t"\
		"subpd	%%xmm7,%%xmm2			\n\t"\
		"addpd	%%xmm0,%%xmm7			\n\t"\
		"subpd	%%xmm6,%%xmm1			\n\t"\
		"subpd	%%xmm3,%%xmm2			\n\t"\
		"movaps	%%xmm1,0x10(%%r11)	\n\t"\
		"addpd	%%xmm0,%%xmm3			\n\t"\
		"subpd	%%xmm5,%%xmm7			\n\t"\
		"subpd	%%xmm4,%%xmm2			\n\t"\
		"movaps	%%xmm7,0x10(%%r15)	\n\t"\
		"movaps	%%xmm2,0x10(%%r12)	\n\t"\
		"addpd	%%xmm0,%%xmm4			\n\t"\
		"addpd	%%xmm6,%%xmm3			\n\t"\
		"addpd	%%xmm5,%%xmm4			\n\t"\
		"movaps	%%xmm3,0x10(%%r13)	\n\t"\
		"movaps	%%xmm4,0x10(%%r14)	\n\t"\
		"/************************************************************************************************************************************/\n\t"\
		"/* Here are the 5 sine terms: Similar sequence to cosine terms, but with t21,19,17,15,13 replacing t3,5,7,9,11 and some sign flips: */\n\t"\
		"/************************************************************************************************************************************/\n\t"\
		/*subq	$0x10,%%rax	<*** Re parts of i-array; on o-side still working with Im-data */\
		"addq	$0xa0,%%rbx				\n\t"\
		"/********************************************/\n\t"\
		"/*               Real Parts:                */\n\t"\
		"/********************************************/\n\t"\
	"movq	%[__oA],%%r11	\n\t	movaps 0x10(%%r11),%%xmm1	\n\t"\
	"movq	%[__o9],%%r12	\n\t	movaps 0x10(%%r12),%%xmm2	\n\t"\
	"movq	%[__o8],%%r13	\n\t	movaps 0x10(%%r13),%%xmm3	\n\t"\
	"movq	%[__o7],%%r14	\n\t	movaps 0x10(%%r14),%%xmm4	\n\t"\
	"movq	%[__o6],%%r15	\n\t	movaps 0x10(%%r15),%%xmm5	\n\t"\
		/* r8-10 still free for o-addressing */\
		"addpd	%%xmm2,%%xmm1			\n\t"\
		"addpd	%%xmm2,%%xmm5			\n\t"\
		"movaps	%%xmm1,%%xmm6			\n\t"\
		"addpd	%%xmm2,%%xmm3			\n\t"\
		"movaps	%%xmm1,%%xmm7			\n\t"\
		"mulpd	     (%%rbx),%%xmm1		\n\t"\
		"movaps	%%xmm1,0x10(%%r15)	\n\t"\
		"addpd	%%xmm2,%%xmm4			\n\t"\
		"mulpd	-0xb0(%%rbx),%%xmm2		\n\t"\
		"addpd	%%xmm5,%%xmm6			\n\t"\
		"subpd	%%xmm3,%%xmm7			\n\t"\
		"mulpd	 0x60(%%rbx),%%xmm7		\n\t"\
		"movaps	%%xmm7,0x10(%%r13)	\n\t"\
		"movaps	%%xmm3,%%xmm1			\n\t"\
		"addpd	%%xmm4,%%xmm3			\n\t"\
		"mulpd	 0x30(%%rbx),%%xmm1		\n\t"\
		"movaps	%%xmm1,0x10(%%r14)	\n\t"\
		"movaps	%%xmm5,%%xmm7			\n\t"\
		"subpd	%%xmm4,%%xmm5			\n\t"\
		"mulpd	 0x40(%%rbx),%%xmm4		\n\t"\
		"mulpd	 0x70(%%rbx),%%xmm5		\n\t"\
		"mulpd	 0x10(%%rbx),%%xmm7		\n\t"\
		"subpd	%%xmm3,%%xmm2			\n\t"\
		"movaps	%%xmm6,%%xmm1			\n\t"\
		"subpd	%%xmm3,%%xmm6			\n\t"\
		"mulpd	 0x80(%%rbx),%%xmm6		\n\t"\
		"mulpd	 0x50(%%rbx),%%xmm3		\n\t"\
		"subpd	%%xmm1,%%xmm2			\n\t"\
		"mulpd	 0x20(%%rbx),%%xmm1		\n\t"\
		"mulpd	 0x90(%%rbx),%%xmm2		\n\t"\
		"addpd	%%xmm1,%%xmm7			\n\t"\
		"addpd	0x10(%%r15),%%xmm1	\n\t"\
		"addpd	%%xmm3,%%xmm4			\n\t"\
		"addpd	0x10(%%r14),%%xmm3	\n\t"\
		"addpd	%%xmm6,%%xmm5			\n\t"\
		"addpd	0x10(%%r13),%%xmm6	\n\t"\
		"xorpd	%%xmm0,%%xmm0			\n\t"\
		"subpd	%%xmm2,%%xmm0			\n\t"\
		"addpd	%%xmm1,%%xmm0			\n\t"\
		"addpd	%%xmm2,%%xmm1			\n\t"\
		"addpd	%%xmm7,%%xmm0			\n\t"\
		"addpd	%%xmm2,%%xmm7			\n\t"\
		"subpd	%%xmm6,%%xmm1			\n\t"\
		"addpd	%%xmm3,%%xmm0			\n\t"\
		"addpd	%%xmm2,%%xmm3			\n\t"\
		"subpd	%%xmm5,%%xmm7			\n\t"\
		"addpd	%%xmm4,%%xmm0			\n\t"\
		"addpd	%%xmm2,%%xmm4			\n\t"\
		"addpd	%%xmm6,%%xmm3			\n\t"\
		"addpd	%%xmm5,%%xmm4			\n\t"\
		"movaps	%%xmm1,%%xmm2			\n\t"\
	"movq	%[__o1],%%r8 	\n\t"\
		"addpd	0x10(%%r8 ),%%xmm1	\n\t"\
		"addpd	%%xmm2,%%xmm2			\n\t"\
		"movaps	%%xmm1,0x10(%%r8 )	\n\t"\
		"subpd	%%xmm2,%%xmm1			\n\t"\
		"movaps	%%xmm1,0x10(%%r11)	\n\t"\
		"movaps	%%xmm0,%%xmm5			\n\t"\
	"movq	%[__o2],%%r8 	\n\t"\
		"addpd	0x10(%%r8 ),%%xmm0	\n\t"\
		"addpd	%%xmm5,%%xmm5			\n\t"\
		"movaps	%%xmm0,0x10(%%r8 )	\n\t"\
		"subpd	%%xmm5,%%xmm0			\n\t"\
		"movaps	%%xmm0,0x10(%%r12)	\n\t"\
		"movaps	%%xmm3,%%xmm6			\n\t"\
	"movq	%[__o3],%%r8 	\n\t"\
		"addpd	0x10(%%r8 ),%%xmm3	\n\t"\
		"addpd	%%xmm6,%%xmm6			\n\t"\
		"movaps	%%xmm3,0x10(%%r8 )	\n\t"\
		"subpd	%%xmm6,%%xmm3			\n\t"\
		"movaps	%%xmm3,0x10(%%r13)	\n\t"\
		"movaps	%%xmm4,%%xmm2			\n\t"\
	"movq	%[__o4],%%r8 	\n\t"\
		"addpd	0x10(%%r8 ),%%xmm4	\n\t"\
		"addpd	%%xmm2,%%xmm2			\n\t"\
		"movaps	%%xmm4,0x10(%%r8 )	\n\t"\
		"subpd	%%xmm2,%%xmm4			\n\t"\
		"movaps	%%xmm4,0x10(%%r14)	\n\t"\
		"movaps	%%xmm7,%%xmm5			\n\t"\
	"movq	%[__o5],%%r8 	\n\t"\
		"addpd	0x10(%%r8 ),%%xmm7	\n\t"\
		"addpd	%%xmm5,%%xmm5			\n\t"\
		"movaps	%%xmm7,0x10(%%r8 )	\n\t"\
		"subpd	%%xmm5,%%xmm7			\n\t"\
		"movaps	%%xmm7,0x10(%%r15)	\n\t"\
		"/********************************************/\n\t"\
		"/*          Imaginary Parts:                */\n\t"\
		"/********************************************/\n\t"\
		/* subq	$0x10,%%rcx	<*** Temps needed for Im outputs stored in Re slots of o-array (I know it seems counterintuitive) */\
	"movq	%[__oA],%%r11	\n\t	movaps (%%r11),%%xmm1	\n\t"\
	"movq	%[__o9],%%r12	\n\t	movaps (%%r12),%%xmm2	\n\t"\
	"movq	%[__o8],%%r13	\n\t	movaps (%%r13),%%xmm3	\n\t"\
	"movq	%[__o7],%%r14	\n\t	movaps (%%r14),%%xmm4	\n\t"\
	"movq	%[__o6],%%r15	\n\t	movaps (%%r15),%%xmm5	\n\t"\
		"addpd	%%xmm2,%%xmm1			\n\t"\
		"addpd	%%xmm2,%%xmm5			\n\t"\
		"movaps	%%xmm1,%%xmm6			\n\t"\
		"addpd	%%xmm2,%%xmm3			\n\t"\
		"movaps	%%xmm1,%%xmm7			\n\t"\
		"mulpd	     (%%rbx),%%xmm1		\n\t"\
		"movaps	%%xmm1,(%%r15)	\n\t"\
		"addpd	%%xmm2,%%xmm4			\n\t"\
		"mulpd	-0xb0(%%rbx),%%xmm2		\n\t"\
		"addpd	%%xmm5,%%xmm6			\n\t"\
		"subpd	%%xmm3,%%xmm7			\n\t"\
		"mulpd	 0x60(%%rbx),%%xmm7		\n\t"\
		"movaps	%%xmm7,(%%r13)	\n\t"\
		"movaps	%%xmm3,%%xmm1			\n\t"\
		"addpd	%%xmm4,%%xmm3			\n\t"\
		"mulpd	 0x30(%%rbx),%%xmm1		\n\t"\
		"movaps	%%xmm1,(%%r14)	\n\t"\
		"movaps	%%xmm5,%%xmm7			\n\t"\
		"subpd	%%xmm4,%%xmm5			\n\t"\
		"mulpd	 0x40(%%rbx),%%xmm4		\n\t"\
		"mulpd	 0x70(%%rbx),%%xmm5		\n\t"\
		"mulpd	 0x10(%%rbx),%%xmm7		\n\t"\
		"subpd	%%xmm3,%%xmm2			\n\t"\
		"movaps	%%xmm6,%%xmm1			\n\t"\
		"subpd	%%xmm3,%%xmm6			\n\t"\
		"mulpd	 0x80(%%rbx),%%xmm6		\n\t"\
		"mulpd	 0x50(%%rbx),%%xmm3		\n\t"\
		"subpd	%%xmm1,%%xmm2			\n\t"\
		"mulpd	 0x20(%%rbx),%%xmm1		\n\t"\
		"mulpd	 0x90(%%rbx),%%xmm2		\n\t"\
		"addpd	%%xmm1,%%xmm7			\n\t"\
		"addpd	(%%r15),%%xmm1	\n\t"\
		"addpd	%%xmm3,%%xmm4			\n\t"\
		"addpd	(%%r14),%%xmm3	\n\t"\
		"addpd	%%xmm6,%%xmm5			\n\t"\
		"addpd	(%%r13),%%xmm6	\n\t"\
		"xorpd	%%xmm0,%%xmm0			\n\t"\
		"subpd	%%xmm2,%%xmm0			\n\t"\
		"addpd	%%xmm1,%%xmm0			\n\t"\
		"addpd	%%xmm2,%%xmm1			\n\t"\
		"addpd	%%xmm7,%%xmm0			\n\t"\
		"addpd	%%xmm2,%%xmm7			\n\t"\
		"subpd	%%xmm6,%%xmm1			\n\t"\
		"addpd	%%xmm3,%%xmm0			\n\t"\
		"addpd	%%xmm2,%%xmm3			\n\t"\
		"subpd	%%xmm5,%%xmm7			\n\t"\
		"addpd	%%xmm4,%%xmm0			\n\t"\
		"addpd	%%xmm2,%%xmm4			\n\t"\
		"addpd	%%xmm6,%%xmm3			\n\t"\
		"addpd	%%xmm5,%%xmm4			\n\t"\
		"movaps	%%xmm1,%%xmm2			\n\t"\
	"movq	%[__o1],%%r8 	\n\t"\
		"addpd	(%%r8 ),%%xmm1	\n\t"\
		"addpd	%%xmm2,%%xmm2			\n\t"\
		"movaps	%%xmm1,(%%r11)	\n\t"\
		"subpd	%%xmm2,%%xmm1			\n\t"\
		"movaps	%%xmm1,(%%r8 )	\n\t"\
		"movaps	%%xmm0,%%xmm5			\n\t"\
	"movq	%[__o2],%%r8 	\n\t"\
		"addpd	(%%r8 ),%%xmm0	\n\t"\
		"addpd	%%xmm5,%%xmm5			\n\t"\
		"movaps	%%xmm0,(%%r12)	\n\t"\
		"subpd	%%xmm5,%%xmm0			\n\t"\
		"movaps	%%xmm0,(%%r8 )	\n\t"\
		"movaps	%%xmm3,%%xmm6			\n\t"\
	"movq	%[__o3],%%r8 	\n\t"\
		"addpd	(%%r8 ),%%xmm3	\n\t"\
		"addpd	%%xmm6,%%xmm6			\n\t"\
		"movaps	%%xmm3,(%%r13)	\n\t"\
		"subpd	%%xmm6,%%xmm3			\n\t"\
		"movaps	%%xmm3,(%%r8 )	\n\t"\
		"movaps	%%xmm4,%%xmm2			\n\t"\
	"movq	%[__o4],%%r8 	\n\t"\
		"addpd	(%%r8 ),%%xmm4	\n\t"\
		"addpd	%%xmm2,%%xmm2			\n\t"\
		"movaps	%%xmm4,(%%r14)	\n\t"\
		"subpd	%%xmm2,%%xmm4			\n\t"\
		"movaps	%%xmm4,(%%r8 )	\n\t"\
		"movaps	%%xmm7,%%xmm5			\n\t"\
	"movq	%[__o5],%%r8 	\n\t"\
		"addpd	(%%r8 ),%%xmm7	\n\t"\
		"addpd	%%xmm5,%%xmm5			\n\t"\
		"movaps	%%xmm7,(%%r15)	\n\t"\
		"subpd	%%xmm5,%%xmm7			\n\t"\
		"movaps	%%xmm7,(%%r8 )	\n\t"\
		:					/* outputs: none */\
		: [__I0] "m" (XI0)	/* All inputs from memory addresses here */\
		 ,[__i1] "m" (Xi1)\
		 ,[__i2] "m" (Xi2)\
		 ,[__i3] "m" (Xi3)\
		 ,[__i4] "m" (Xi4)\
		 ,[__i5] "m" (Xi5)\
		 ,[__i6] "m" (Xi6)\
		 ,[__i7] "m" (Xi7)\
		 ,[__i8] "m" (Xi8)\
		 ,[__i9] "m" (Xi9)\
		 ,[__iA] "m" (XiA)\
		 ,[__cc] "m" (Xcc)\
		 ,[__O0] "m" (XO0)\
		 ,[__o1] "m" (Xo1)\
		 ,[__o2] "m" (Xo2)\
		 ,[__o3] "m" (Xo3)\
		 ,[__o4] "m" (Xo4)\
		 ,[__o5] "m" (Xo5)\
		 ,[__o6] "m" (Xo6)\
		 ,[__o7] "m" (Xo7)\
		 ,[__o8] "m" (Xo8)\
		 ,[__o9] "m" (Xo9)\
		 ,[__oA] "m" (XoA)\
		: "cc","memory","rax","rbx","rcx","r8","r9","r10","r11","r12","r13","r14","r15","xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7"		/* Clobbered registers */\
		);\
	}

  #endif // USE_64BIT_ASM_STYLE

#elif defined(USE_SSE2) && (OS_BITS == 32)

	#define SSE2_RADIX_11_DFT(XI0,Xi1,Xi2,Xi3,Xi4,Xi5,Xi6,Xi7,Xi8,Xi9,XiA, Xcc, XO0,Xo1,Xo2,Xo3,Xo4,Xo5,Xo6,Xo7,Xo8,Xo9,XoA)\
	{\
	__asm__ volatile (\
	"pushl %%ebx	\n\t"/* Explicit save/restore of PIC register */\
	/********************************************/\
	/*       Here are the 5 cosine terms:       */\
	/********************************************/\
		"movl	%[__cc],%%esi			\n\t"\
	"movl	%[__i1],%%eax	\n\t	movaps (%%eax),%%xmm1	\n\t"\
	"movl	%[__iA],%%ebx	\n\t	movaps (%%ebx),%%xmm5	\n\t"\
	"movl	%[__i2],%%ecx	\n\t	movaps (%%ecx),%%xmm2	\n\t"\
	"movl	%[__i9],%%edx	\n\t	movaps (%%edx),%%xmm6	\n\t"\
	"movl	%[__i3],%%eax	\n\t	movaps (%%eax),%%xmm3	\n\t"\
	"movl	%[__i8],%%ebx	\n\t	movaps (%%ebx),%%xmm7	\n\t"\
	"movl	%[__i4],%%ecx	\n\t	movaps (%%ecx),%%xmm4	\n\t"\
	"movl	%[__i7],%%edx	\n\t	movaps (%%edx),%%xmm0	\n\t"\
		"subpd	%%xmm5,%%xmm1			\n\t"\
		"subpd	%%xmm6,%%xmm2			\n\t"\
		"subpd	%%xmm7,%%xmm3			\n\t"\
		"subpd	%%xmm0,%%xmm4			\n\t"\
	"movl	%[__oA],%%eax	\n\t	movaps %%xmm1,0x10(%%eax)	\n\t"\
	"movl	%[__o9],%%ebx	\n\t	movaps %%xmm2,0x10(%%ebx)	\n\t"\
	"movl	%[__o8],%%ecx	\n\t	movaps %%xmm3,0x10(%%ecx)	\n\t"\
	"movl	%[__o7],%%edx	\n\t	movaps %%xmm4,0x10(%%edx)	\n\t"\
		"addpd	%%xmm5,%%xmm5			\n\t"\
		"addpd	%%xmm6,%%xmm6			\n\t"\
		"addpd	%%xmm7,%%xmm7			\n\t"\
		"addpd	%%xmm0,%%xmm0			\n\t"\
		"addpd	%%xmm5,%%xmm1			\n\t"\
		"addpd	%%xmm6,%%xmm2			\n\t"\
	"movl	%[__i5],%%eax	\n\t	movaps (%%eax),%%xmm5	\n\t"\
	"movl	%[__i6],%%ebx	\n\t	movaps (%%ebx),%%xmm6	\n\t"\
		"addpd	%%xmm7,%%xmm3			\n\t"\
		"addpd	%%xmm0,%%xmm4			\n\t"\
		"subpd	%%xmm6,%%xmm5			\n\t"\
	"movl	%[__o6],%%edi	\n\t	movaps %%xmm5,0x10(%%edi)	\n\t"\
		"addpd	%%xmm6,%%xmm6			\n\t"\
		"movl	%[__I0],%%eax			\n\t"\
		"movaps	(%%eax),%%xmm0			\n\t"\
		"addpd	%%xmm6,%%xmm5			\n\t"\
		"/********************************************/\n\t"\
		"/*               Real Parts:                */\n\t"\
		"/********************************************/\n\t"\
	"movl	%[__o1],%%eax	\n\t"\
	"movl	%[__o2],%%ebx	\n\t"\
	"movl	%[__o3],%%ecx	\n\t"\
	"movl	%[__o4],%%edx	\n\t"\
	"movl	%[__o5],%%edi	\n\t"\
		"subpd	%%xmm2,%%xmm1			\n\t"\
		"subpd	%%xmm2,%%xmm5			\n\t"\
		"movaps	%%xmm1,%%xmm6			\n\t"\
		"subpd	%%xmm2,%%xmm3			\n\t"\
		"movaps	%%xmm1,%%xmm7			\n\t"\
		"mulpd	     (%%esi),%%xmm1		\n\t"\
		"movaps	%%xmm1,(%%eax)	\n\t"\
		"subpd	%%xmm2,%%xmm4			\n\t"\
		"mulpd	-0x10(%%esi),%%xmm2		\n\t"\
		"addpd	%%xmm5,%%xmm6			\n\t"\
		"subpd	%%xmm3,%%xmm7			\n\t"\
		"mulpd	 0x60(%%esi),%%xmm7		\n\t"\
		"movaps	%%xmm7,(%%ecx)	\n\t"\
		"movaps	%%xmm3,%%xmm1			\n\t"\
		"addpd	%%xmm4,%%xmm3			\n\t"\
		"mulpd	 0x30(%%esi),%%xmm1		\n\t"\
		"movaps	%%xmm1,(%%ebx)	\n\t"\
		"movaps	%%xmm5,%%xmm7			\n\t"\
		"subpd	%%xmm4,%%xmm5			\n\t"\
		"mulpd	 0x40(%%esi),%%xmm4		\n\t"\
		"mulpd	 0x70(%%esi),%%xmm5		\n\t"\
		"mulpd	 0x10(%%esi),%%xmm7		\n\t"\
		"addpd	%%xmm3,%%xmm2			\n\t"\
		"movaps	%%xmm6,%%xmm1			\n\t"\
		"subpd	%%xmm3,%%xmm6			\n\t"\
		"mulpd	 0x80(%%esi),%%xmm6		\n\t"\
		"mulpd	 0x50(%%esi),%%xmm3		\n\t"\
		"addpd	%%xmm1,%%xmm2			\n\t"\
		"mulpd	 0x20(%%esi),%%xmm1		\n\t"\
		"addpd	%%xmm2,%%xmm0			\n\t"\
		"mulpd	 0x90(%%esi),%%xmm2		\n\t"\
		"movl	%[__O0],%%esi			\n\t"\
		"movaps	%%xmm0,(%%esi)			\n\t"\
		"movl	%[__cc],%%esi			\n\t"\
		"addpd	%%xmm0,%%xmm2			\n\t"\
		"addpd	%%xmm1,%%xmm7			\n\t"\
		"addpd	(%%eax),%%xmm1	\n\t"\
		"addpd	%%xmm3,%%xmm4			\n\t"\
		"addpd	(%%ebx),%%xmm3	\n\t"\
		"addpd	%%xmm6,%%xmm5			\n\t"\
		"addpd	(%%ecx),%%xmm6	\n\t"\
		"movaps	%%xmm2,%%xmm0			\n\t"\
		"subpd	%%xmm1,%%xmm2			\n\t"\
		"addpd	%%xmm0,%%xmm1			\n\t"\
		"subpd	%%xmm7,%%xmm2			\n\t"\
		"addpd	%%xmm0,%%xmm7			\n\t"\
		"subpd	%%xmm6,%%xmm1			\n\t"\
		"subpd	%%xmm3,%%xmm2			\n\t"\
		"movaps	%%xmm1,(%%eax)	\n\t"\
		"addpd	%%xmm0,%%xmm3			\n\t"\
		"subpd	%%xmm5,%%xmm7			\n\t"\
		"subpd	%%xmm4,%%xmm2			\n\t"\
		"movaps	%%xmm7,(%%edi)	\n\t"\
		"movaps	%%xmm2,(%%ebx)	\n\t"\
		"addpd	%%xmm0,%%xmm4			\n\t"\
		"addpd	%%xmm6,%%xmm3			\n\t"\
		"addpd	%%xmm5,%%xmm4			\n\t"\
		"movaps	%%xmm3,(%%ecx)	\n\t"\
		"movaps	%%xmm4,(%%edx)	\n\t"\
		"/********************************************/\n\t"\
		"/*          Imaginary Parts:                */\n\t"\
		"/********************************************/\n\t"\
	"movl	%[__i1],%%eax	\n\t	movaps 0x10(%%eax),%%xmm1	\n\t"\
	"movl	%[__iA],%%ebx	\n\t	movaps 0x10(%%ebx),%%xmm5	\n\t"\
	"movl	%[__i2],%%ecx	\n\t	movaps 0x10(%%ecx),%%xmm2	\n\t"\
	"movl	%[__i9],%%edx	\n\t	movaps 0x10(%%edx),%%xmm6	\n\t"\
	"movl	%[__i3],%%eax	\n\t	movaps 0x10(%%eax),%%xmm3	\n\t"\
	"movl	%[__i8],%%ebx	\n\t	movaps 0x10(%%ebx),%%xmm7	\n\t"\
	"movl	%[__i4],%%ecx	\n\t	movaps 0x10(%%ecx),%%xmm4	\n\t"\
	"movl	%[__i7],%%edx	\n\t	movaps 0x10(%%edx),%%xmm0	\n\t"\
		"subpd	%%xmm5,%%xmm1			\n\t"\
		"subpd	%%xmm6,%%xmm2			\n\t"\
		"subpd	%%xmm7,%%xmm3			\n\t"\
		"subpd	%%xmm0,%%xmm4			\n\t"\
		/* Now this spill quartet is into Re slots of o-array: */\
	"movl	%[__oA],%%eax	\n\t	movaps %%xmm1,(%%eax)	\n\t"\
	"movl	%[__o9],%%ebx	\n\t	movaps %%xmm2,(%%ebx)	\n\t"\
	"movl	%[__o8],%%ecx	\n\t	movaps %%xmm3,(%%ecx)	\n\t"\
	"movl	%[__o7],%%edx	\n\t	movaps %%xmm4,(%%edx)	\n\t"\
		"addpd	%%xmm5,%%xmm5			\n\t"\
		"addpd	%%xmm6,%%xmm6			\n\t"\
		"addpd	%%xmm7,%%xmm7			\n\t"\
		"addpd	%%xmm0,%%xmm0			\n\t"\
		"addpd	%%xmm5,%%xmm1			\n\t"\
		"addpd	%%xmm6,%%xmm2			\n\t"\
	"movl	%[__i5],%%eax	\n\t	movaps 0x10(%%eax),%%xmm5	\n\t"\
	"movl	%[__i6],%%ebx	\n\t	movaps 0x10(%%ebx),%%xmm6	\n\t"\
		"addpd	%%xmm7,%%xmm3			\n\t"\
		"addpd	%%xmm0,%%xmm4			\n\t"\
		"subpd	%%xmm6,%%xmm5			\n\t"\
	"movl	%[__o6],%%edi	\n\t	movaps %%xmm5,(%%edi)	\n\t"\
		"addpd	%%xmm6,%%xmm6			\n\t"\
		"movl	%[__I0],%%eax			\n\t"\
		"movaps	0x10(%%eax),%%xmm0			\n\t"\
		"addpd	%%xmm6,%%xmm5			\n\t"\
	"movl	%[__o1],%%eax	\n\t"\
	"movl	%[__o2],%%ebx	\n\t"\
	"movl	%[__o3],%%ecx	\n\t"\
	"movl	%[__o4],%%edx	\n\t"\
	"movl	%[__o5],%%edi	\n\t"\
		"subpd	%%xmm2,%%xmm1			\n\t"\
		"subpd	%%xmm2,%%xmm5			\n\t"\
		"movaps	%%xmm1,%%xmm6			\n\t"\
		"subpd	%%xmm2,%%xmm3			\n\t"\
		"movaps	%%xmm1,%%xmm7			\n\t"\
		"mulpd	     (%%esi),%%xmm1		\n\t"\
		"movaps	%%xmm1,0x10(%%eax)	\n\t"\
		"subpd	%%xmm2,%%xmm4			\n\t"\
		"mulpd	-0x10(%%esi),%%xmm2		\n\t"\
		"addpd	%%xmm5,%%xmm6			\n\t"\
		"subpd	%%xmm3,%%xmm7			\n\t"\
		"mulpd	 0x60(%%esi),%%xmm7		\n\t"\
		"movaps	%%xmm7,0x10(%%ecx)	\n\t"\
		"movaps	%%xmm3,%%xmm1			\n\t"\
		"addpd	%%xmm4,%%xmm3			\n\t"\
		"mulpd	 0x30(%%esi),%%xmm1		\n\t"\
		"movaps	%%xmm1,0x10(%%ebx)	\n\t"\
		"movaps	%%xmm5,%%xmm7			\n\t"\
		"subpd	%%xmm4,%%xmm5			\n\t"\
		"mulpd	 0x40(%%esi),%%xmm4		\n\t"\
		"mulpd	 0x70(%%esi),%%xmm5		\n\t"\
		"mulpd	 0x10(%%esi),%%xmm7		\n\t"\
		"addpd	%%xmm3,%%xmm2			\n\t"\
		"movaps	%%xmm6,%%xmm1			\n\t"\
		"subpd	%%xmm3,%%xmm6			\n\t"\
		"mulpd	 0x80(%%esi),%%xmm6		\n\t"\
		"mulpd	 0x50(%%esi),%%xmm3		\n\t"\
		"addpd	%%xmm1,%%xmm2			\n\t"\
		"mulpd	 0x20(%%esi),%%xmm1		\n\t"\
		"addpd	%%xmm2,%%xmm0			\n\t"\
		"mulpd	 0x90(%%esi),%%xmm2		\n\t"\
		"movl	%[__O0],%%esi			\n\t"\
		"movaps	%%xmm0,0x10(%%esi)		\n\t"\
		"movl	%[__cc],%%esi			\n\t"\
		"addpd	%%xmm0,%%xmm2			\n\t"\
		"addpd	%%xmm1,%%xmm7			\n\t"\
		"addpd	0x10(%%eax),%%xmm1	\n\t"\
		"addpd	%%xmm3,%%xmm4			\n\t"\
		"addpd	0x10(%%ebx),%%xmm3	\n\t"\
		"addpd	%%xmm6,%%xmm5			\n\t"\
		"addpd	0x10(%%ecx),%%xmm6	\n\t"\
		"movaps	%%xmm2,%%xmm0			\n\t"\
		"subpd	%%xmm1,%%xmm2			\n\t"\
		"addpd	%%xmm0,%%xmm1			\n\t"\
		"subpd	%%xmm7,%%xmm2			\n\t"\
		"addpd	%%xmm0,%%xmm7			\n\t"\
		"subpd	%%xmm6,%%xmm1			\n\t"\
		"subpd	%%xmm3,%%xmm2			\n\t"\
		"movaps	%%xmm1,0x10(%%eax)	\n\t"\
		"addpd	%%xmm0,%%xmm3			\n\t"\
		"subpd	%%xmm5,%%xmm7			\n\t"\
		"subpd	%%xmm4,%%xmm2			\n\t"\
		"movaps	%%xmm7,0x10(%%edi)	\n\t"\
		"movaps	%%xmm2,0x10(%%ebx)	\n\t"\
		"addpd	%%xmm0,%%xmm4			\n\t"\
		"addpd	%%xmm6,%%xmm3			\n\t"\
		"addpd	%%xmm5,%%xmm4			\n\t"\
		"movaps	%%xmm3,0x10(%%ecx)	\n\t"\
		"movaps	%%xmm4,0x10(%%edx)	\n\t"\
	/************************************************************************************************************************************/\
	/* Here are the 5 sine terms: Similar sequence to cosine terms, but with t21,19,17,15,13 replacing t3,5,7,9,11 and some sign flips: */\
	/************************************************************************************************************************************/\
		"addl	$0xa0,%%esi				\n\t"\
		"/********************************************/\n\t"\
		"/*               Real Parts:                */\n\t"\
		"/********************************************/\n\t"\
	"movl	%[__oA],%%eax	\n\t	movaps 0x10(%%eax),%%xmm1	\n\t"\
	"movl	%[__o9],%%ebx	\n\t	movaps 0x10(%%ebx),%%xmm2	\n\t"\
	"movl	%[__o8],%%ecx	\n\t	movaps 0x10(%%ecx),%%xmm3	\n\t"\
	"movl	%[__o7],%%edx	\n\t	movaps 0x10(%%edx),%%xmm4	\n\t"\
	"movl	%[__o6],%%edi	\n\t	movaps 0x10(%%edi),%%xmm5	\n\t"\
		/* r8-10 still free for o-addressing */\
		"addpd	%%xmm2,%%xmm1			\n\t"\
		"addpd	%%xmm2,%%xmm5			\n\t"\
		"movaps	%%xmm1,%%xmm6			\n\t"\
		"addpd	%%xmm2,%%xmm3			\n\t"\
		"movaps	%%xmm1,%%xmm7			\n\t"\
		"mulpd	     (%%esi),%%xmm1		\n\t"\
		"movaps	%%xmm1,0x10(%%edi)	\n\t"\
		"addpd	%%xmm2,%%xmm4			\n\t"\
		"mulpd	-0xb0(%%esi),%%xmm2		\n\t"\
		"addpd	%%xmm5,%%xmm6			\n\t"\
		"subpd	%%xmm3,%%xmm7			\n\t"\
		"mulpd	 0x60(%%esi),%%xmm7		\n\t"\
		"movaps	%%xmm7,0x10(%%ecx)	\n\t"\
		"movaps	%%xmm3,%%xmm1			\n\t"\
		"addpd	%%xmm4,%%xmm3			\n\t"\
		"mulpd	 0x30(%%esi),%%xmm1		\n\t"\
		"movaps	%%xmm1,0x10(%%edx)	\n\t"\
		"movaps	%%xmm5,%%xmm7			\n\t"\
		"subpd	%%xmm4,%%xmm5			\n\t"\
		"mulpd	 0x40(%%esi),%%xmm4		\n\t"\
		"mulpd	 0x70(%%esi),%%xmm5		\n\t"\
		"mulpd	 0x10(%%esi),%%xmm7		\n\t"\
		"subpd	%%xmm3,%%xmm2			\n\t"\
		"movaps	%%xmm6,%%xmm1			\n\t"\
		"subpd	%%xmm3,%%xmm6			\n\t"\
		"mulpd	 0x80(%%esi),%%xmm6		\n\t"\
		"mulpd	 0x50(%%esi),%%xmm3		\n\t"\
		"subpd	%%xmm1,%%xmm2			\n\t"\
		"mulpd	 0x20(%%esi),%%xmm1		\n\t"\
		"mulpd	 0x90(%%esi),%%xmm2		\n\t"\
		"addpd	%%xmm1,%%xmm7			\n\t"\
		"addpd	0x10(%%edi),%%xmm1	\n\t"\
		"addpd	%%xmm3,%%xmm4			\n\t"\
		"addpd	0x10(%%edx),%%xmm3	\n\t"\
		"addpd	%%xmm6,%%xmm5			\n\t"\
		"addpd	0x10(%%ecx),%%xmm6	\n\t"\
		"xorpd	%%xmm0,%%xmm0			\n\t"\
		"subpd	%%xmm2,%%xmm0			\n\t"\
		"addpd	%%xmm1,%%xmm0			\n\t"\
		"addpd	%%xmm2,%%xmm1			\n\t"\
		"addpd	%%xmm7,%%xmm0			\n\t"\
		"addpd	%%xmm2,%%xmm7			\n\t"\
		"subpd	%%xmm6,%%xmm1			\n\t"\
		"addpd	%%xmm3,%%xmm0			\n\t"\
		"addpd	%%xmm2,%%xmm3			\n\t"\
		"subpd	%%xmm5,%%xmm7			\n\t"\
		"addpd	%%xmm4,%%xmm0			\n\t"\
		"addpd	%%xmm2,%%xmm4			\n\t"\
		"addpd	%%xmm6,%%xmm3			\n\t"\
		"addpd	%%xmm5,%%xmm4			\n\t"\
		"movaps	%%xmm1,%%xmm2			\n\t"\
	"movl	%[__o1],%%esi	\n\t"\
		"addpd	0x10(%%esi),%%xmm1	\n\t"\
		"addpd	%%xmm2,%%xmm2			\n\t"\
		"movaps	%%xmm1,0x10(%%esi)	\n\t"\
		"subpd	%%xmm2,%%xmm1			\n\t"\
		"movaps	%%xmm1,0x10(%%eax)	\n\t"\
		"movaps	%%xmm0,%%xmm5			\n\t"\
	"movl	%[__o2],%%esi	\n\t"\
		"addpd	0x10(%%esi),%%xmm0	\n\t"\
		"addpd	%%xmm5,%%xmm5			\n\t"\
		"movaps	%%xmm0,0x10(%%esi)	\n\t"\
		"subpd	%%xmm5,%%xmm0			\n\t"\
		"movaps	%%xmm0,0x10(%%ebx)	\n\t"\
		"movaps	%%xmm3,%%xmm6			\n\t"\
	"movl	%[__o3],%%esi	\n\t"\
		"addpd	0x10(%%esi),%%xmm3	\n\t"\
		"addpd	%%xmm6,%%xmm6			\n\t"\
		"movaps	%%xmm3,0x10(%%esi)	\n\t"\
		"subpd	%%xmm6,%%xmm3			\n\t"\
		"movaps	%%xmm3,0x10(%%ecx)	\n\t"\
		"movaps	%%xmm4,%%xmm2			\n\t"\
	"movl	%[__o4],%%esi	\n\t"\
		"addpd	0x10(%%esi),%%xmm4	\n\t"\
		"addpd	%%xmm2,%%xmm2			\n\t"\
		"movaps	%%xmm4,0x10(%%esi)	\n\t"\
		"subpd	%%xmm2,%%xmm4			\n\t"\
		"movaps	%%xmm4,0x10(%%edx)	\n\t"\
		"movaps	%%xmm7,%%xmm5			\n\t"\
	"movl	%[__o5],%%esi	\n\t"\
		"addpd	0x10(%%esi),%%xmm7	\n\t"\
		"addpd	%%xmm5,%%xmm5			\n\t"\
		"movaps	%%xmm7,0x10(%%esi)	\n\t"\
		"subpd	%%xmm5,%%xmm7			\n\t"\
		"movaps	%%xmm7,0x10(%%edi)	\n\t"\
		"/********************************************/\n\t"\
		"/*          Imaginary Parts:                */\n\t"\
		"/********************************************/\n\t"\
		"movl	%[__cc],%%esi			\n\t"\
		"addl	$0xa0,%%esi				\n\t"\
	"movl	%[__oA],%%eax	\n\t	movaps (%%eax),%%xmm1	\n\t"\
	"movl	%[__o9],%%ebx	\n\t	movaps (%%ebx),%%xmm2	\n\t"\
	"movl	%[__o8],%%ecx	\n\t	movaps (%%ecx),%%xmm3	\n\t"\
	"movl	%[__o7],%%edx	\n\t	movaps (%%edx),%%xmm4	\n\t"\
	"movl	%[__o6],%%edi	\n\t	movaps (%%edi),%%xmm5	\n\t"\
		"addpd	%%xmm2,%%xmm1			\n\t"\
		"addpd	%%xmm2,%%xmm5			\n\t"\
		"movaps	%%xmm1,%%xmm6			\n\t"\
		"addpd	%%xmm2,%%xmm3			\n\t"\
		"movaps	%%xmm1,%%xmm7			\n\t"\
		"mulpd	     (%%esi),%%xmm1		\n\t"\
		"movaps	%%xmm1,(%%edi)	\n\t"\
		"addpd	%%xmm2,%%xmm4			\n\t"\
		"mulpd	-0xb0(%%esi),%%xmm2		\n\t"\
		"addpd	%%xmm5,%%xmm6			\n\t"\
		"subpd	%%xmm3,%%xmm7			\n\t"\
		"mulpd	 0x60(%%esi),%%xmm7		\n\t"\
		"movaps	%%xmm7,(%%ecx)	\n\t"\
		"movaps	%%xmm3,%%xmm1			\n\t"\
		"addpd	%%xmm4,%%xmm3			\n\t"\
		"mulpd	 0x30(%%esi),%%xmm1		\n\t"\
		"movaps	%%xmm1,(%%edx)	\n\t"\
		"movaps	%%xmm5,%%xmm7			\n\t"\
		"subpd	%%xmm4,%%xmm5			\n\t"\
		"mulpd	 0x40(%%esi),%%xmm4		\n\t"\
		"mulpd	 0x70(%%esi),%%xmm5		\n\t"\
		"mulpd	 0x10(%%esi),%%xmm7		\n\t"\
		"subpd	%%xmm3,%%xmm2			\n\t"\
		"movaps	%%xmm6,%%xmm1			\n\t"\
		"subpd	%%xmm3,%%xmm6			\n\t"\
		"mulpd	 0x80(%%esi),%%xmm6		\n\t"\
		"mulpd	 0x50(%%esi),%%xmm3		\n\t"\
		"subpd	%%xmm1,%%xmm2			\n\t"\
		"mulpd	 0x20(%%esi),%%xmm1		\n\t"\
		"mulpd	 0x90(%%esi),%%xmm2		\n\t"\
		"addpd	%%xmm1,%%xmm7			\n\t"\
		"addpd	(%%edi),%%xmm1	\n\t"\
		"addpd	%%xmm3,%%xmm4			\n\t"\
		"addpd	(%%edx),%%xmm3	\n\t"\
		"addpd	%%xmm6,%%xmm5			\n\t"\
		"addpd	(%%ecx),%%xmm6	\n\t"\
		"xorpd	%%xmm0,%%xmm0			\n\t"\
		"subpd	%%xmm2,%%xmm0			\n\t"\
		"addpd	%%xmm1,%%xmm0			\n\t"\
		"addpd	%%xmm2,%%xmm1			\n\t"\
		"addpd	%%xmm7,%%xmm0			\n\t"\
		"addpd	%%xmm2,%%xmm7			\n\t"\
		"subpd	%%xmm6,%%xmm1			\n\t"\
		"addpd	%%xmm3,%%xmm0			\n\t"\
		"addpd	%%xmm2,%%xmm3			\n\t"\
		"subpd	%%xmm5,%%xmm7			\n\t"\
		"addpd	%%xmm4,%%xmm0			\n\t"\
		"addpd	%%xmm2,%%xmm4			\n\t"\
		"addpd	%%xmm6,%%xmm3			\n\t"\
		"addpd	%%xmm5,%%xmm4			\n\t"\
		"movaps	%%xmm1,%%xmm2			\n\t"\
	"movl	%[__o1],%%esi	\n\t"\
		"addpd	(%%esi),%%xmm1	\n\t"\
		"addpd	%%xmm2,%%xmm2			\n\t"\
		"movaps	%%xmm1,(%%eax)	\n\t"\
		"subpd	%%xmm2,%%xmm1			\n\t"\
		"movaps	%%xmm1,(%%esi)	\n\t"\
		"movaps	%%xmm0,%%xmm5			\n\t"\
	"movl	%[__o2],%%esi	\n\t"\
		"addpd	(%%esi),%%xmm0	\n\t"\
		"addpd	%%xmm5,%%xmm5			\n\t"\
		"movaps	%%xmm0,(%%ebx)	\n\t"\
		"subpd	%%xmm5,%%xmm0			\n\t"\
		"movaps	%%xmm0,(%%esi)	\n\t"\
		"movaps	%%xmm3,%%xmm6			\n\t"\
	"movl	%[__o3],%%esi	\n\t"\
		"addpd	(%%esi),%%xmm3	\n\t"\
		"addpd	%%xmm6,%%xmm6			\n\t"\
		"movaps	%%xmm3,(%%ecx)	\n\t"\
		"subpd	%%xmm6,%%xmm3			\n\t"\
		"movaps	%%xmm3,(%%esi)	\n\t"\
		"movaps	%%xmm4,%%xmm2			\n\t"\
	"movl	%[__o4],%%esi	\n\t"\
		"addpd	(%%esi),%%xmm4	\n\t"\
		"addpd	%%xmm2,%%xmm2			\n\t"\
		"movaps	%%xmm4,(%%edx)	\n\t"\
		"subpd	%%xmm2,%%xmm4			\n\t"\
		"movaps	%%xmm4,(%%esi)	\n\t"\
		"movaps	%%xmm7,%%xmm5			\n\t"\
	"movl	%[__o5],%%esi	\n\t"\
		"addpd	(%%esi),%%xmm7	\n\t"\
		"addpd	%%xmm5,%%xmm5			\n\t"\
		"movaps	%%xmm7,(%%edi)	\n\t"\
		"subpd	%%xmm5,%%xmm7			\n\t"\
		"movaps	%%xmm7,(%%esi)	\n\t"\
	"popl %%ebx	\n\t"/*Explicit restore of PIC register */\
		:					/* outputs: none */\
		: [__I0] "m" (XI0)	/* All inputs from memory addresses here */\
		 ,[__i1] "m" (Xi1)\
		 ,[__i2] "m" (Xi2)\
		 ,[__i3] "m" (Xi3)\
		 ,[__i4] "m" (Xi4)\
		 ,[__i5] "m" (Xi5)\
		 ,[__i6] "m" (Xi6)\
		 ,[__i7] "m" (Xi7)\
		 ,[__i8] "m" (Xi8)\
		 ,[__i9] "m" (Xi9)\
		 ,[__iA] "m" (XiA)\
		 ,[__cc] "m" (Xcc)\
		 ,[__O0] "m" (XO0)\
		 ,[__o1] "m" (Xo1)\
		 ,[__o2] "m" (Xo2)\
		 ,[__o3] "m" (Xo3)\
		 ,[__o4] "m" (Xo4)\
		 ,[__o5] "m" (Xo5)\
		 ,[__o6] "m" (Xo6)\
		 ,[__o7] "m" (Xo7)\
		 ,[__o8] "m" (Xo8)\
		 ,[__o9] "m" (Xo9)\
		 ,[__oA] "m" (XoA)\
		: "cc","memory","eax",/*"ebx",*/"ecx","edx","esi","edi","xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7"		/* Clobbered registers */\
		);\
	}

#else

	#error Unhandled combination of preprocessor flags!

#endif	// x86 simd version ?

#endif	/* radix11_sse_macro_h_included */
