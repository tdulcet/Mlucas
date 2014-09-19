/*******************************************************************************
*                                                                              *
*   (C) 1997-2014 by Ernst W. Mayer.                                           *
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
#ifndef radix13_sse_macro_h_included
#define radix13_sse_macro_h_included

	#include "sse2_macro.h"

	#if defined(COMPILER_TYPE_MSVC)
		/*
		__cc pointer offsets:
		-0x010 = 2.0
		0x000 =  DC1
		0x010 =  DC3
		0x020 =  DC4
		0x030 =  DS1
		0x040 =  DS2
		0x050 =  DS3
		0x060 =  DS4
		0x070 =  DS5
		0x080 = DC23
		0x090 = DC54
		0x0a0 = DC65
		0x0b0 = DS63
		0x0c0 = DS74
		0x0d0 = DS85
		0x0e0 = DS93
		0x0f0 = DSa4
		0x100 = DSb5
		*/
		/*...Radix-13 DFT: Inputs in memory locations __I0 + [__i1,__i2,__i3,__i4,__i5,__i6,__i7,__i8,__i9,__iA,__iB,__iC],\
		where r0 is a memory address and the i's are literal [byte] offsets. Outputs similarly go into memory locations\
		__O0 + [__o1,__o2,__o3,__o4,__o5,__o6,__o7,__o8,__o9,__oA,__oB,__oC], assumed disjoint with inputs:\

		Total SSE opcounts:
				32-bit:		148 MOVAPS, 164 ADD/SUBPD, 76 MULPD
				64-bit:		124 MOVAPS, 164 ADD/SUBPD, 76 MULPD
		*/\
		#define SSE2_RADIX_13_DFT(__cc,\
			__I0,__i1,__i2,__i3,__i4,__i5,__i6,__i7,__i8,__i9,__iA,__iB,__iC,\
			__O0,__o1,__o2,__o3,__o4,__o5,__o6,__o7,__o8,__o9,__oA,__oB,__oC)\
		{\
		/***************/\
		/* REAL PARTS: */\
		/***************/\
			__asm	mov	eax, __I0	\
			__asm	mov	ebx, __cc	\
			__asm	mov	ecx, __O0	\
			__asm	movaps	xmm7,[eax+__i6]		/* xr7 = __A6r */\
			__asm	movaps	xmm5,[eax+__i5]		/* xr5 = __A5r */\
			__asm	movaps	xmm4,[eax+__i4]		/* xr4 = __A4r */\
			__asm	movaps	xmm6,[eax+__i3]		/* xr6 = __A3r */\
			__asm	movaps	xmm3,[eax+__i2]		/* xr3 = __A2r */\
			__asm	movaps	xmm1,[eax+__i1]		/* xr1 = __A1r */\
		/* xr-terms need 8 registers for each side: */\
			__asm	movaps	xmm0,[ebx-0x10]		/* 2.0 */\
			__asm	addpd	xmm7,[eax+__i7]		/* xr7 += __A7r */\
			__asm	addpd	xmm5,[eax+__i8]		/* xr5 += __A8r */\
			__asm	addpd	xmm4,[eax+__i9]		/* xr4 += __A9r */\
			__asm	addpd	xmm6,[eax+__iA]		/* xr6 += __Aar */\
			__asm	addpd	xmm3,[eax+__iB]		/* xr3 += __Abr */\
			__asm	addpd	xmm1,[eax+__iC]		/* xr1 += __Acr */\
			__asm	subpd	xmm1,xmm5			__asm	mulpd	xmm5,xmm0	/* xr1 -= xr5;	xr5 *= 2.0 */\
			__asm	subpd	xmm3,xmm6			__asm	mulpd	xmm6,xmm0	/* xr3 -= xr6;	xr6 *= 2.0 */\
			__asm	subpd	xmm4,xmm7			__asm	mulpd	xmm7,xmm0	/* xr4 -= xr7;	xr7 *= 2.0 */\
			__asm	addpd	xmm5,xmm1			/* xr5 += xr1 */\
			__asm	addpd	xmm6,xmm3			/* xr6 += xr3 */\
			__asm	addpd	xmm7,xmm4			/* xr7 += xr4 */\
			__asm	movaps	xmm2,xmm5			/* xr2  = xr5 */\
			__asm	addpd	xmm2,xmm6			/* xr2 += xr6 */\
			__asm	addpd	xmm2,xmm7			/* xr2 += xr7 */\
			__asm	movaps	xmm0,[eax]			/* xr0 = __A0r */\
			__asm	addpd	xmm0,xmm2			/* xr0 += xr2 */\
			__asm	mulpd	xmm2,[ebx]			/* xr2 *= DC1 */\
			__asm	movaps	[ecx],xmm0			/* __B0r = xr0 */\
			__asm	addpd	xmm0,xmm2			/* xr0 += xr2 */\
			__asm	movaps	xmm2,[ebx+0x010]	/* xr2 = DC3 */\
			__asm	mulpd	xmm6,xmm2			/* xr6 *= xr2 */\
			__asm	mulpd	xmm5,xmm2			/* xr5 *= xr2 */\
			__asm	mulpd	xmm7,xmm2			/* xr7 *= xr2 */\
			__asm	movaps	[ecx+__o1],xmm6		/* __B1r = xr6 */\
			__asm	movaps	[ecx+__o2],xmm5		/* __B2r = xr5 */\
			__asm	movaps	[ecx+__o3],xmm7		/* __B3r = xr7 */\
			__asm	movaps	xmm2,[ebx+0x080]	/* xr2 = DC23 */\
			__asm	mulpd	xmm5,xmm2			/* xr5 *= xr2 */\
			__asm	mulpd	xmm7,xmm2			/* xr7 *= xr2 */\
			__asm	mulpd	xmm6,xmm2			/* xr6 *= xr2 */\
			__asm	addpd	xmm5,xmm0			/* xr5 += xr0 */\
			__asm	addpd	xmm7,xmm0			/* xr7 += xr0 */\
			__asm	addpd	xmm6,xmm0			/* xr6 += xr0 */\
			__asm	addpd	xmm5,[ecx+__o1]		/* xr5 += __B1r */\
			__asm	addpd	xmm7,[ecx+__o2]		/* xr7 += __B2r */\
			__asm	addpd	xmm6,[ecx+__o3]		/* xr6 += __B3r */\
		/* Rearrange the 3x3 multiply block to ease in-place computation: */\
			__asm	movaps	xmm2,[ebx+0x0a0]	/* xr2 = DC65 */\
			__asm	movaps	xmm0,[ebx+0x090]	/* xr0 = DC54 */\
			__asm	movaps	[ecx+__o1],xmm4		/* __B1r = xr4 */\
			__asm	movaps	[ecx+__o2],xmm1		/* __B2r = xr1 */\
			__asm	movaps	[ecx+__o3],xmm3		/* __B3r = xr3 */\
			__asm	mulpd	xmm4,xmm2			/* xr4 *= DC65 */\
			__asm	mulpd	xmm1,xmm2			/* xr1 *= DC65 */\
			__asm	mulpd	xmm3,xmm2			/* xr3 *= DC65 */\
			__asm	addpd	xmm4,[ecx+__o3]		/* xr4 += __B3r */\
			__asm	subpd	xmm1,[ecx+__o1]		/* xr1 -= __B1r */\
			__asm	addpd	xmm3,[ecx+__o2]		/* xr3 += __B2r */\
			__asm	mulpd	xmm4,xmm0			/* xr4 *= DC54 */\
			__asm	mulpd	xmm1,xmm0			/* xr1 *= DC54 */\
			__asm	mulpd	xmm3,xmm0			/* xr3 *= DC54 */\
			__asm	movaps	xmm2,[ebx+0x020]	/* xr2 = DC4 */\
			__asm	addpd	xmm4,[ecx+__o2]		/* xr4 += __B2r */\
			__asm	subpd	xmm1,[ecx+__o3]		/* xr1 -= __B3r */\
			__asm	subpd	xmm3,[ecx+__o1]		/* xr3 -= __B1r */\
			__asm	mulpd	xmm4,xmm2			/* xr4 *= DC4 */\
			__asm	mulpd	xmm1,xmm2			/* xr1 *= DC4 */\
			__asm	mulpd	xmm3,xmm2			/* xr3 *= DC4 */\
			__asm	movaps	xmm0,[ebx-0x010]	/* 2.0 */\
		/* Spill into destination outputs: */\
			__asm	subpd	xmm6,xmm4			__asm	mulpd	xmm4,xmm0	/* xr6 -= xr1;	xr5 *= 2.0 */\
			__asm	subpd	xmm7,xmm1			__asm	mulpd	xmm1,xmm0	/* xr7 -= xr3;	xr6 *= 2.0 */\
			__asm	subpd	xmm5,xmm3			__asm	mulpd	xmm3,xmm0	/* xr5 -= xr4;	xr7 *= 2.0 */\
			__asm	movaps	[ecx+__o3],xmm7		/* __B3r = xr7 */\
			__asm	movaps	[ecx+__o8],xmm5		/* __B8r = xr5 */\
			__asm	addpd	xmm4,xmm6			/* xr4 += xr6 */\
			__asm	addpd	xmm1,xmm7			/* xr7 += xr1 */\
			__asm	addpd	xmm3,xmm5			/* xr5 += xr3 */\
			__asm	movaps	[ecx+__o4],xmm6		/* __B4r = xr6 */\
			__asm	movaps	[ecx+__o6],xmm4		/* __B6r = xr4 */\
			__asm	movaps	[ecx+__o2],xmm1		/* __B2r = xr7 */\
			__asm	movaps	[ecx+__o1],xmm3		/* __B1r = xr5 */\
		/* yi-terms: */\
			__asm	add	eax, 0x10	\
			__asm	movaps	xmm1,[eax+__i1]		/* xr1 = __A1i */\
			__asm	movaps	xmm2,[eax+__i2]		/* xr2 = __A2i */\
			__asm	movaps	xmm5,[eax+__i3]		/* xr5 = __A3i */\
			__asm	movaps	xmm3,[eax+__i4]		/* xr3 = __A4i */\
			__asm	movaps	xmm4,[eax+__i5]		/* xr4 = __A5i */\
			__asm	movaps	xmm6,[eax+__i6]		/* xr6 = __A6i */\
			__asm	subpd	xmm1,[eax+__iC]		/* xr1 -= __Aci */\
			__asm	subpd	xmm2,[eax+__iB]		/* xr2 -= __Abi */\
			__asm	subpd	xmm5,[eax+__iA]		/* xr5 -= __Aai */\
			__asm	subpd	xmm3,[eax+__i9]		/* xr3 -= __A9i */\
			__asm	subpd	xmm4,[eax+__i8]		/* xr4 -= __A8i */\
			__asm	subpd	xmm6,[eax+__i7]		/* xr6 -= __A7i */\
			__asm	movaps	xmm7,xmm1			/* xr7 = xr1 */\
			__asm	movaps	xmm0,xmm2			/* xr0 = xr2 */\
			__asm	subpd	xmm7,xmm3			/* xr7 -= xr3 */\
			__asm	addpd	xmm0,xmm4			/* xr0 += xr4 */\
			__asm	addpd	xmm7,xmm5			/* xr7 += xr5 */\
			__asm	addpd	xmm0,xmm6			/* xr0 += xr6 */\
			__asm	addpd	xmm1,xmm3			/* xr1 += xr3 */\
			__asm	addpd	xmm5,xmm3			/* xr5 += xr3 */\
			__asm	subpd	xmm2,xmm4			/* xr2 -= xr4 */\
			__asm	subpd	xmm6,xmm4			/* xr6 -= xr4 */\
			__asm	movaps	xmm4,xmm0			/* xr4 = xr0 */\
			__asm	movaps	xmm3,xmm7			/* xr3 = xr7 */\
			__asm	mulpd	xmm0,[ebx+0x040]	/* xr0 *= DS2 */\
			__asm	mulpd	xmm7,[ebx+0x040]	/* xr7 *= DS2 */\
			__asm	mulpd	xmm4,[ebx+0x030]	/* xr4 *= DS1 */\
			__asm	mulpd	xmm3,[ebx+0x030]	/* xr3 *= DS1 */\
			__asm	subpd	xmm4,xmm7			/* xr4 -= xr7 */\
			__asm	addpd	xmm3,xmm0			/* xr3 += xr0 */\
			__asm	movaps  [ecx+__oC],xmm4		/* __Bcr = xr4; tmp-store in Bcr */\
			__asm	movaps	xmm0,xmm1			/* xr0 = xr1 */\
			__asm	movaps	xmm4,xmm5			/* xr4 = xr5 */\
			__asm	addpd	xmm0,xmm2			/* xr0 += xr2 */\
			__asm	addpd	xmm4,xmm6			/* xr4 += xr6 */\
		/*\
		xr7 = DS3*xr0-DS6*xr4;\
		xr0 = DS3*xr4-DS9*xr0;\
		*/\
			__asm	movaps  [ecx+__oB],xmm0		/* __Bbr = xr0; tmp-store in Bbr */\
			__asm	movaps	xmm7,xmm4			/* xr7 = xr4 */\
			__asm	mulpd	xmm4,[ebx+0x0b0]	/* xr4 *= DS63 */\
			__asm	mulpd	xmm0,[ebx+0x0e0]	/* xr0 *= DS93 */\
			__asm	addpd	xmm4,[ecx+__oB]		/* xr4 += __Bbr */\
			__asm	addpd	xmm0,xmm7			/* xr0 += xr7 */\
			__asm	mulpd	xmm4,[ebx+0x050]	/* xr4 *= DS3 */\
			__asm	mulpd	xmm0,[ebx+0x050]	/* xr0 *= DS3 */\
		/*\
		xmm7,xmm4+DS4*xmm2-DS7*xr6;\
		xmm2,xmm0+DS4*xmm6-DSa*xr2;\
		*/\
			__asm	movaps  [ecx+__oB],xmm2		/* __Bbr = xr2 */\
			__asm	movaps	xmm7,xmm6			/* xr7 = xr6 */\
			__asm	mulpd	xmm6,[ebx+0x0c0]	/* xr6 *= DS74 */\
			__asm	mulpd	xmm2,[ebx+0x0f0]	/* xr2 *= DSa4 */\
			__asm	addpd	xmm6,[ecx+__oB]		/* xr6 += __Bbr */\
			__asm	addpd	xmm2,xmm7			/* xr2 += xr7 */\
			__asm	mulpd	xmm6,[ebx+0x060]	/* xr6 *= DS4 */\
			__asm	mulpd	xmm2,[ebx+0x060]	/* xr2 *= DS4 */\
			__asm	addpd	xmm6,xmm4			/* xr6 += xr4 */\
			__asm	addpd	xmm2,xmm0			/* xr2 += xr0 */\
		/*\
		xmm7,xmm4+DS5*xmm1-DS8*xr5;\
		xmm0,xmm0+DS5*xmm5-DSb*xr1;\
		*/\
			__asm	movaps  [ecx+__oB],xmm1		/* __Bbr = xr1 */\
			__asm	movaps	xmm7,xmm5			/* xr7 = xr5 */\
			__asm	mulpd	xmm5,[ebx+0x0d0]	/* xr5 *= DS85 */\
			__asm	mulpd	xmm1,[ebx+0x100]	/* xr1 *= DSb5 */\
			__asm	addpd	xmm5,[ecx+__oB]		/* xr5 += __Bbr */\
			__asm	addpd	xmm1,xmm7			/* xr1 += xr7 */\
			__asm	mulpd	xmm5,[ebx+0x070]	/* xr5 *= DS5 */\
			__asm	mulpd	xmm1,[ebx+0x070]	/* xr1 *= DS5 */\
			__asm	addpd	xmm5,xmm4			/* xr5 += xr4 */\
			__asm	addpd	xmm1,xmm0			/* xr1 += xr0 */\
			__asm	movaps  xmm4,[ecx+__oC]		/* xr4 = __Bcr */\
			__asm	movaps	xmm7,xmm3			/* xr7  = xr3 */\
			__asm	movaps	xmm0,xmm4			/* xr0  = xr4 */\
			__asm	addpd	xmm7,xmm6			/* xr7 += xr6 */\
			__asm	addpd	xmm0,xmm5			/* xr0 += xr5 */\
			__asm	subpd	xmm6,xmm3			/* xr6 -= xr3 */\
			__asm	subpd	xmm5,xmm4			/* xr5 -= xr4 */\
			__asm	addpd	xmm6,xmm2			/* xr6 += xr2 */\
			__asm	addpd	xmm5,xmm1			/* xr5 += xr1 */\
			__asm	addpd	xmm2,xmm3			/* xr2 += xr3 */\
			__asm	addpd	xmm4,xmm1			/* xr4 += xr1 */\
		/* Combine xmm and yi-terms to get real parts of outputs: */\
			/* __B6r -= xr7; xr7 *= xr1; xr7 += __B6r; __B7r = xr7 */\
			__asm movaps xmm3,[ecx+__o6]	\
			__asm movaps xmm1,xmm3			\
			__asm subpd xmm3,xmm7			\
			__asm addpd xmm1,xmm7			\
			__asm movaps [ecx+__o6],xmm3	\
			__asm movaps [ecx+__o7],xmm1	\
			/* __B8r -= xr6;	xr6 *= xr1; xr6 += __B8r;   __B5r = xr6 */\
			__asm movaps xmm3,[ecx+__o8]	\
			__asm movaps xmm1,xmm3			\
			__asm subpd xmm3,xmm6			\
			__asm addpd xmm1,xmm6			\
			__asm movaps [ecx+__o8],xmm3	\
			__asm movaps [ecx+__o5],xmm1	\
			/* __B2r -= xr2;	xr2 *= xr1; xr2 += __B2r;   __Bbr = xr2 */\
			__asm movaps xmm3,[ecx+__o2]	\
			__asm movaps xmm1,xmm3			\
			__asm subpd xmm3,xmm2			\
			__asm addpd xmm1,xmm2			\
			__asm movaps [ecx+__o2],xmm3	\
			__asm movaps [ecx+__oB],xmm1	\
			/* __B3r -= xr0;	xr0 *= xr1; xr0 += __B3r;   __Bar = xr0 */\
			__asm movaps xmm3,[ecx+__o3]	\
			__asm movaps xmm1,xmm3			\
			__asm subpd xmm3,xmm0			\
			__asm addpd xmm1,xmm0			\
			__asm movaps [ecx+__o3],xmm3	\
			__asm movaps [ecx+__oA],xmm1	\
			/* __B4r -= xr5;	xr5 *= xr1; xr5 += __B4r;   __B9r = xr5 */\
			__asm movaps xmm3,[ecx+__o4]	\
			__asm movaps xmm1,xmm3			\
			__asm subpd xmm3,xmm5			\
			__asm addpd xmm1,xmm5			\
			__asm movaps [ecx+__o4],xmm3	\
			__asm movaps [ecx+__o9],xmm1	\
			/* __B1r -= xr4;	xr4 *= xr1; xr4 += __B1r;   __Bcr = xr4 */\
			__asm movaps xmm3,[ecx+__o1]	\
			__asm movaps xmm1,xmm3			\
			__asm subpd xmm3,xmm4			\
			__asm addpd xmm1,xmm4			\
			__asm movaps [ecx+__o1],xmm3	\
			__asm movaps [ecx+__oC],xmm1	\
		/***************/\
		/* IMAG PARTS: Swap __**r <--> __**i, Replace __B[j] with __B[13-j] for j > 0: */\
		/***************/\
			__asm	add	ecx, 0x10	\
			__asm	movaps	xmm7,[eax+__i6]		/* xr7 = __A6i */\
			__asm	movaps	xmm5,[eax+__i5]		/* xr5 = __A5i */\
			__asm	movaps	xmm4,[eax+__i4]		/* xr4 = __A4i */\
			__asm	movaps	xmm6,[eax+__i3]		/* xr6 = __A3i */\
			__asm	movaps	xmm3,[eax+__i2]		/* xr3 = __A2i */\
			__asm	movaps	xmm1,[eax+__i1]		/* xr1 = __A1i */\
		/* xi-terms need 8 registers for each side: */\
			__asm	movaps	xmm0,[ebx-0x10]		/* 2.0 */\
			__asm	addpd	xmm7,[eax+__i7]		/* xr7 += __A7i */\
			__asm	addpd	xmm5,[eax+__i8]		/* xr5 += __A8i */\
			__asm	addpd	xmm4,[eax+__i9]		/* xr4 += __A9i */\
			__asm	addpd	xmm6,[eax+__iA]		/* xr6 += __Aai */\
			__asm	addpd	xmm3,[eax+__iB]		/* xr3 += __Abi */\
			__asm	addpd	xmm1,[eax+__iC]		/* xr1 += __Aci */\
			__asm	subpd	xmm1,xmm5			__asm	mulpd	xmm5,xmm0	/* xr1 -= xr5;	xr5 *= 2.0 */\
			__asm	subpd	xmm3,xmm6			__asm	mulpd	xmm6,xmm0	/* xr3 -= xr6;	xr6 *= 2.0 */\
			__asm	subpd	xmm4,xmm7			__asm	mulpd	xmm7,xmm0	/* xr4 -= xr7;	xr7 *= 2.0 */\
			__asm	addpd	xmm5,xmm1			/* xr5 += xr1 */\
			__asm	addpd	xmm6,xmm3			/* xr6 += xr3 */\
			__asm	addpd	xmm7,xmm4			/* xr7 += xr4 */\
			__asm	movaps	xmm2,xmm5			/* xr2  = xr5 */\
			__asm	addpd	xmm2,xmm6			/* xr2 += xr6 */\
			__asm	addpd	xmm2,xmm7			/* xr2 += xr7 */\
			__asm	movaps	xmm0,[eax]			/* xr0 = __A0i */\
			__asm	addpd	xmm0,xmm2			/* xr0 += xr2 */\
			__asm	mulpd	xmm2,[ebx]			/* xr2 *= DC1 */\
			__asm	movaps	[ecx],xmm0			/* __B0i = xr0 */\
			__asm	addpd	xmm0,xmm2			/* xr0 += xr2 */\
			__asm	movaps	xmm2,[ebx+0x010]	/* xr2 = DC3 */\
			__asm	mulpd	xmm6,xmm2			/* xr6 *= xr2 */\
			__asm	mulpd	xmm5,xmm2			/* xr5 *= xr2 */\
			__asm	mulpd	xmm7,xmm2			/* xr7 *= xr2 */\
			__asm	movaps  [ecx+__oC],xmm6		/* __Bci = xr6 */\
			__asm	movaps  [ecx+__oB],xmm5		/* __Bbi = xr5 */\
			__asm	movaps  [ecx+__oA],xmm7		/* __Bai = xr7 */\
			__asm	movaps	xmm2,[ebx+0x080]	/* xr2 = DC23 */\
			__asm	mulpd	xmm5,xmm2			/* xr5 *= xr2 */\
			__asm	mulpd	xmm7,xmm2			/* xr7 *= xr2 */\
			__asm	mulpd	xmm6,xmm2			/* xr6 *= xr2 */\
			__asm	addpd	xmm5,xmm0			/* xr5 += xr0 */\
			__asm	addpd	xmm7,xmm0			/* xr7 += xr0 */\
			__asm	addpd	xmm6,xmm0			/* xr6 += xr0 */\
			__asm	addpd	xmm5,[ecx+__oC]		/* xr5 += __Bci */\
			__asm	addpd	xmm7,[ecx+__oB]		/* xr7 += __Bbi */\
			__asm	addpd	xmm6,[ecx+__oA]		/* xr6 += __Bai */\
		/* Rearrange the 3x3 multiply block to ease in-place computation: */\
			__asm	movaps	xmm2,[ebx+0x0a0]	/* xr2 = DC65 */\
			__asm	movaps	xmm0,[ebx+0x090]	/* xr0 = DC54 */\
			__asm	movaps  [ecx+__oC],xmm4		/* __Bci = xr4 */\
			__asm	movaps  [ecx+__oB],xmm1		/* __Bbi = xr1 */\
			__asm	movaps  [ecx+__oA],xmm3		/* __Bai = xr3 */\
			__asm	mulpd	xmm4,xmm2			/* xr4 *= DC65 */\
			__asm	mulpd	xmm1,xmm2			/* xr1 *= DC65 */\
			__asm	mulpd	xmm3,xmm2			/* xr3 *= DC65 */\
			__asm	addpd	xmm4,[ecx+__oA]		/* xr4 += __Bai */\
			__asm	subpd	xmm1,[ecx+__oC]		/* xr1 -= __Bci */\
			__asm	addpd	xmm3,[ecx+__oB]		/* xr3 += __Bbi */\
			__asm	mulpd	xmm4,xmm0			/* xr4 *= DC54 */\
			__asm	mulpd	xmm1,xmm0			/* xr1 *= DC54 */\
			__asm	mulpd	xmm3,xmm0			/* xr3 *= DC54 */\
			__asm	movaps	xmm2,[ebx+0x020]	/* xr2 = DC4 */\
			__asm	addpd	xmm4,[ecx+__oB]		/* xr4 += __Bbi */\
			__asm	subpd	xmm1,[ecx+__oA]		/* xr1 -= __Bai */\
			__asm	subpd	xmm3,[ecx+__oC]		/* xr3 -= __Bci */\
			__asm	mulpd	xmm4,xmm2			/* xr4 *= DC4 */\
			__asm	mulpd	xmm1,xmm2			/* xr1 *= DC4 */\
			__asm	mulpd	xmm3,xmm2			/* xr3 *= DC4 */\
			__asm	movaps	xmm0,[ebx-0x010]	/* 2.0 */\
		/* Spill into destination outputs: */\
			__asm	subpd	xmm6,xmm4			__asm	mulpd	xmm4,xmm0	/* xr6 -= xr1;	xr5 *= 2.0 */\
			__asm	subpd	xmm7,xmm1			__asm	mulpd	xmm1,xmm0	/* xr7 -= xr3;	xr6 *= 2.0 */\
			__asm	subpd	xmm5,xmm3			__asm	mulpd	xmm3,xmm0	/* xr5 -= xr4;	xr7 *= 2.0 */\
			__asm	movaps  [ecx+__oA],xmm7		/* __Bai = xr7 */\
			__asm	movaps	[ecx+__o5],xmm5		/* __B5i = xr5 */\
			__asm	addpd	xmm4,xmm6			/* xr4 += xr6 */\
			__asm	addpd	xmm1,xmm7			/* xr7 += xr1 */\
			__asm	addpd	xmm3,xmm5			/* xr5 += xr3 */\
			__asm	movaps	[ecx+__o9],xmm6		/* __B9i = xr6 */\
			__asm	movaps	[ecx+__o7],xmm4		/* __B7i = xr4 */\
			__asm	movaps  [ecx+__oB],xmm1		/* __Bbi = xr7 */\
			__asm	movaps  [ecx+__oC],xmm3		/* __Bci = xr5 */\
		/* yr-terms: */\
			__asm	sub	eax, 0x10	\
			__asm	movaps	xmm1,[eax+__i1]		/* xr1 = __A1r */\
			__asm	movaps	xmm2,[eax+__i2]		/* xr2 = __A2r */\
			__asm	movaps	xmm5,[eax+__i3]		/* xr5 = __A3r */\
			__asm	movaps	xmm3,[eax+__i4]		/* xr3 = __A4r */\
			__asm	movaps	xmm4,[eax+__i5]		/* xr4 = __A5r */\
			__asm	movaps	xmm6,[eax+__i6]		/* xr6 = __A6r */\
			__asm	subpd	xmm1,[eax+__iC]		/* xr1 -= __Acr */\
			__asm	subpd	xmm2,[eax+__iB]		/* xr2 -= __Abr */\
			__asm	subpd	xmm5,[eax+__iA]		/* xr5 -= __Aar */\
			__asm	subpd	xmm3,[eax+__i9]		/* xr3 -= __A9r */\
			__asm	subpd	xmm4,[eax+__i8]		/* xr4 -= __A8r */\
			__asm	subpd	xmm6,[eax+__i7]		/* xr6 -= __A7r */\
			__asm	movaps	xmm7,xmm1			/* xr7 = xr1 */\
			__asm	movaps	xmm0,xmm2			/* xr0 = xr2 */\
			__asm	subpd	xmm7,xmm3			/* xr7 -= xr3 */\
			__asm	addpd	xmm0,xmm4			/* xr0 += xr4 */\
			__asm	addpd	xmm7,xmm5			/* xr7 += xr5 */\
			__asm	addpd	xmm0,xmm6			/* xr0 += xr6 */\
			__asm	addpd	xmm1,xmm3			/* xr1 += xr3 */\
			__asm	addpd	xmm5,xmm3			/* xr5 += xr3 */\
			__asm	subpd	xmm2,xmm4			/* xr2 -= xr4 */\
			__asm	subpd	xmm6,xmm4			/* xr6 -= xr4 */\
			__asm	movaps	xmm4,xmm0			/* xr4 = xr0 */\
			__asm	movaps	xmm3,xmm7			/* xr3	= xr7 */\
			__asm	mulpd	xmm0,[ebx+0x040]	/* xr0 *= DS2 */\
			__asm	mulpd	xmm7,[ebx+0x040]	/* xr7 *= DS2 */\
			__asm	mulpd	xmm4,[ebx+0x030]	/* xr4 *= DS1 */\
			__asm	mulpd	xmm3,[ebx+0x030]	/* xr3	*= DS1 */\
			__asm	subpd	xmm4,xmm7			/* xr4 -= xr7 */\
			__asm	addpd	xmm3,xmm0			/* xr3	+= xr0 */\
			__asm	movaps	[ecx+__o1],xmm4		/* __B1i = xr4; tmp-store in B1i */\
			__asm	movaps	xmm0,xmm1			/* xr0 = xr1 */\
			__asm	movaps	xmm4,xmm5			/* xr4 = xr5 */\
			__asm	addpd	xmm0,xmm2			/* xr0 += xr2 */\
			__asm	addpd	xmm4,xmm6			/* xr4 += xr6 */\
		/*\
		xr7 = DS3*xr0-DS6*xr4;\
		xr0 = DS3*xr4-DS9*xr0;\
		*/\
			__asm	movaps	[ecx+__o2],xmm0		/*	__B2i = xr0; tmp-store in B2i */\
			__asm	movaps	xmm7,xmm4			/*	  xr7 = xr4 */\
			__asm	mulpd	xmm4,[ebx+0x0b0]	/*	xr4 *= DS63 */\
			__asm	mulpd	xmm0,[ebx+0x0e0]	/*	xr0 *= DS93 */\
			__asm	addpd	xmm4,[ecx+__o2]		/*	xr4 += __B2i */\
			__asm	addpd	xmm0,xmm7			/*	xr0 +=	xr7 */\
			__asm	mulpd	xmm4,[ebx+0x050]	/*	xr4 *= DS3 */\
			__asm	mulpd	xmm0,[ebx+0x050]	/*	xr0 *= DS3 */\
		/*\
		xmm7,xmm4+DS4*xmm2-DS7*xr6;\
		xmm2,xmm0+DS4*xmm6-DSa*xr2;\
		*/\
			__asm	movaps	[ecx+__o2],xmm2		/*	__B2i = xr2 */\
			__asm	movaps	xmm7,xmm6			/*	  xr7 = xr6 */\
			__asm	mulpd	xmm6,[ebx+0x0c0]	/*	xr6 *= DS74 */\
			__asm	mulpd	xmm2,[ebx+0x0f0]	/*	xr2 *= DSa4 */\
			__asm	addpd	xmm6,[ecx+__o2]		/*	xr6 += __B2i */\
			__asm	addpd	xmm2,xmm7			/*	xr2 +=	xr7 */\
			__asm	mulpd	xmm6,[ebx+0x060]	/*	xr6 *= DS4 */\
			__asm	mulpd	xmm2,[ebx+0x060]	/*	xr2 *= DS4 */\
			__asm	addpd	xmm6,xmm4			/*	xr6 += xr4 */\
			__asm	addpd	xmm2,xmm0			/*	xr2 += xr0 */\
		/*\
		xmm7,xmm4+DS5*xmm1-DS8*xr5;\
		xmm0,xmm0+DS5*xmm5-DSb*xr1;\
		*/\
			__asm	movaps	[ecx+__o2],xmm1		/*	__B2i = xr1 */\
			__asm	movaps	xmm7,xmm5			/*	  xr7 = xr5 */\
			__asm	mulpd	xmm5,[ebx+0x0d0]	/*	xr5 *= DS85 */\
			__asm	mulpd	xmm1,[ebx+0x100]	/*	xr1 *= DSb5 */\
			__asm	addpd	xmm5,[ecx+__o2]		/*	xr5 += __B2i */\
			__asm	addpd	xmm1,xmm7			/*	xr1 +=	xr7 */\
			__asm	mulpd	xmm5,[ebx+0x070]	/*	xr5 *= DS5 */\
			__asm	mulpd	xmm1,[ebx+0x070]	/*	xr1 *= DS5 */\
			__asm	addpd	xmm5,xmm4			/*	xr5 += xr4 */\
			__asm	addpd	xmm1,xmm0			/*	xr1 += xr0 */\
			__asm	movaps	xmm4,[ecx+__o1]		/*	xr4 = __B1i */\
			__asm	movaps	xmm7,xmm3			/*	xr7  = xr3 */\
			__asm	movaps	xmm0,xmm4			/*	xr0  = xr4 */\
			__asm	addpd	xmm7,xmm6			/*	xr7 += xr6 */\
			__asm	addpd	xmm0,xmm5			/*	xr0 += xr5 */\
			__asm	subpd	xmm6,xmm3			/*	xr6 -= xr3 */\
			__asm	subpd	xmm5,xmm4			/*	xr5 -= xr4 */\
			__asm	addpd	xmm6,xmm2			/*	xr6 += xr2 */\
			__asm	addpd	xmm5,xmm1			/*	xr5 += xr1 */\
			__asm	addpd	xmm2,xmm3			/*	xr2 += xr3 */\
			__asm	addpd	xmm4,xmm1			/*	xr4 += xr1 */\
		/* Combine xmm and yi-terms to get real parts of outputs: */\
			/* __B7i -= xr7; xr7 *= xr1; xr7 += __B7i; __B6i = xr7 */\
			__asm movaps xmm3,[ecx+__o7]	\
			__asm movaps xmm1,xmm3			\
			__asm subpd xmm3,xmm7			\
			__asm addpd xmm1,xmm7			\
			__asm movaps [ecx+__o7],xmm3	\
			__asm movaps [ecx+__o6],xmm1	\
			/* __B5i -= xr6; xr6 *= xr1; xr6+= __B5i; __B8i = xr6 */\
			__asm movaps xmm3,[ecx+__o5]	\
			__asm movaps xmm1,xmm3			\
			__asm subpd xmm3,xmm6			\
			__asm addpd xmm1,xmm6			\
			__asm movaps [ecx+__o5],xmm3	\
			__asm movaps [ecx+__o8],xmm1	\
			/* __Bbi -= xr2; xr2 *= xr1; xr2+= __Bbi; __B2i = xr2 */\
			__asm movaps xmm3,[ecx+__oB]	\
			__asm movaps xmm1,xmm3			\
			__asm subpd xmm3,xmm2			\
			__asm addpd xmm1,xmm2			\
			__asm movaps [ecx+__oB],xmm3	\
			__asm movaps [ecx+__o2],xmm1	\
			/* __Bai -= xr0; xr0 *= xr1; xr0+= __Bai; __B3i = xr0 */\
			__asm movaps xmm3,[ecx+__oA]	\
			__asm movaps xmm1,xmm3			\
			__asm subpd xmm3,xmm0			\
			__asm addpd xmm1,xmm0			\
			__asm movaps [ecx+__oA],xmm3	\
			__asm movaps [ecx+__o3],xmm1	\
			/* __B9i -= xr5; xr5 *= xr1; xr5+= __B9i; __B4i = xr5 */\
			__asm movaps xmm3,[ecx+__o9]	\
			__asm movaps xmm1,xmm3			\
			__asm subpd xmm3,xmm5			\
			__asm addpd xmm1,xmm5			\
			__asm movaps [ecx+__o9],xmm3	\
			__asm movaps [ecx+__o4],xmm1	\
			/* __Bci -= xr4; xr4 *= xr1; xr4+= __Bci; __B1i = xr4 */\
			__asm movaps xmm3,[ecx+__oC]	\
			__asm movaps xmm1,xmm3			\
			__asm subpd xmm3,xmm4			\
			__asm addpd xmm1,xmm4			\
			__asm movaps [ecx+__oC],xmm3	\
			__asm movaps [ecx+__o1],xmm1	\
		}

	#else	/* GCC-style inline ASM: */

	  #if OS_BITS == 32

		#define SSE2_RADIX_13_DFT(Xcc, XI0,Xi1,Xi2,Xi3,Xi4,Xi5,Xi6,Xi7,Xi8,Xi9,XiA,XiB,XiC, XO0,Xo1,Xo2,Xo3,Xo4,Xo5,Xo6,Xo7,Xo8,Xo9,XoA,XoB,XoC)\
		{\
		__asm__ volatile (\
		"pushl %%ebx	\n\t"/* Explicit save/restore of PIC register */\
			"movl	%[__cc],%%ebx		\n\t"\
			"movaps -0x10(%%ebx),%%xmm0	\n\t"\
		"/* xr-terms need 8 registers for each side: */\n\t"\
		"movl	%[__i6],%%esi	\n\t	movaps (%%esi),%%xmm7	\n\t"\
		"movl	%[__i5],%%edi	\n\t	movaps (%%edi),%%xmm5	\n\t"\
		"movl	%[__i4],%%eax	\n\t	movaps (%%eax),%%xmm4	\n\t"\
		"movl	%[__i3],%%ebx	\n\t	movaps (%%ebx),%%xmm6	\n\t"\
		"movl	%[__i2],%%ecx	\n\t	movaps (%%ecx),%%xmm3	\n\t"\
		"movl	%[__i1],%%edx	\n\t	movaps (%%edx),%%xmm1	\n\t"\
		"movl	%[__i7],%%esi	\n\t	addpd (%%esi),%%xmm7	\n\t"\
		"movl	%[__i8],%%edi	\n\t	addpd (%%edi),%%xmm5	\n\t"\
		"movl	%[__i9],%%eax	\n\t	addpd (%%eax),%%xmm4	\n\t"\
		"movl	%[__iA],%%ebx	\n\t	addpd (%%ebx),%%xmm6	\n\t"\
		"movl	%[__iB],%%ecx	\n\t	addpd (%%ecx),%%xmm3	\n\t"\
		"movl	%[__iC],%%edx	\n\t	addpd (%%edx),%%xmm1	\n\t"\
			"subpd	%%xmm5,%%xmm1		\n\t"\
			"mulpd	%%xmm0,%%xmm5		\n\t"\
			"subpd	%%xmm6,%%xmm3		\n\t"\
			"mulpd	%%xmm0,%%xmm6		\n\t"\
			"subpd	%%xmm7,%%xmm4		\n\t"\
			"mulpd	%%xmm0,%%xmm7		\n\t"\
			"movl	%[__I0],%%eax		\n\t"\
			"movl	%[__cc],%%ebx		\n\t"\
			"movl	%[__O0],%%ecx		\n\t"\
			"addpd	%%xmm1,%%xmm5		\n\t"\
			"addpd	%%xmm3,%%xmm6		\n\t"\
			"addpd	%%xmm4,%%xmm7		\n\t"\
			"movaps %%xmm5,%%xmm2		\n\t"\
			"addpd	%%xmm6,%%xmm2		\n\t"\
			"addpd	%%xmm7,%%xmm2		\n\t"\
			"movaps (%%eax),%%xmm0		\n\t"\
			"addpd	%%xmm2,%%xmm0		\n\t"\
			"mulpd	(%%ebx),%%xmm2		\n\t"\
			"movaps %%xmm0,(%%ecx)	\n\t"\
			"addpd	%%xmm2,%%xmm0		\n\t"\
			"movaps 0x010(%%ebx),%%xmm2	\n\t"\
			"mulpd	%%xmm2,%%xmm6		\n\t"\
			"mulpd	%%xmm2,%%xmm5		\n\t"\
			"mulpd	%%xmm2,%%xmm7		\n\t"\
		/* This section uses o-offsets 1,2,3,4,6,8 - prestore those into r10-15: */\
			"movaps 0x080(%%ebx),%%xmm2	\n\t"\
		"movl	%[__o1],%%esi	\n\t"\
		"movl	%[__o2],%%edi	\n\t"\
		"movl	%[__o3],%%eax	\n\t"\
			"movaps %%xmm6,(%%esi)	\n\t"\
			"movaps %%xmm5,(%%edi)	\n\t"\
			"movaps %%xmm7,(%%eax)	\n\t"\
			"mulpd	%%xmm2,%%xmm5		\n\t"\
			"mulpd	%%xmm2,%%xmm7		\n\t"\
			"mulpd	%%xmm2,%%xmm6		\n\t"\
			"addpd	%%xmm0,%%xmm5		\n\t"\
			"addpd	%%xmm0,%%xmm7		\n\t"\
			"addpd	%%xmm0,%%xmm6		\n\t"\
			"addpd	(%%esi),%%xmm5	\n\t"\
			"addpd	(%%edi),%%xmm7	\n\t"\
			"addpd	(%%eax),%%xmm6	\n\t"\
			"movaps 0x0a0(%%ebx),%%xmm2	\n\t"\
			"movaps 0x090(%%ebx),%%xmm0	\n\t"\
			"movaps %%xmm4,(%%esi)	\n\t"\
			"movaps %%xmm1,(%%edi)	\n\t"\
			"movaps %%xmm3,(%%eax)	\n\t"\
			"mulpd	%%xmm2,%%xmm4		\n\t"\
			"mulpd	%%xmm2,%%xmm1		\n\t"\
			"mulpd	%%xmm2,%%xmm3		\n\t"\
			"addpd	(%%eax),%%xmm4	\n\t"\
			"subpd	(%%esi),%%xmm1	\n\t"\
			"addpd	(%%edi),%%xmm3	\n\t"\
			"mulpd	%%xmm0,%%xmm4		\n\t"\
			"mulpd	%%xmm0,%%xmm1		\n\t"\
			"mulpd	%%xmm0,%%xmm3		\n\t"\
			"movaps 0x020(%%ebx),%%xmm2	\n\t"\
			"addpd	(%%edi),%%xmm4	\n\t"\
			"subpd	(%%eax),%%xmm1	\n\t"\
			"subpd	(%%esi),%%xmm3	\n\t"\
			"mulpd	%%xmm2,%%xmm4		\n\t"\
			"mulpd	%%xmm2,%%xmm1		\n\t"\
			"mulpd	%%xmm2,%%xmm3		\n\t"\
			"movaps -0x010(%%ebx),%%xmm0	\n\t"\
		"movl	%[__o4],%%ebx	\n\t"\
		"movl	%[__o6],%%ecx	\n\t"\
		"movl	%[__o8],%%edx	\n\t"\
			"subpd	%%xmm4,%%xmm6		\n\t"\
			"mulpd	%%xmm0,%%xmm4		\n\t"\
			"subpd	%%xmm1,%%xmm7		\n\t"\
			"mulpd	%%xmm0,%%xmm1		\n\t"\
			"subpd	%%xmm3,%%xmm5		\n\t"\
			"mulpd	%%xmm0,%%xmm3		\n\t"\
			"movaps %%xmm7,(%%eax)	\n\t"\
			"movaps %%xmm5,(%%edx)	\n\t"\
			"addpd	%%xmm6,%%xmm4		\n\t"\
			"addpd	%%xmm1,%%xmm7		\n\t"\
			"addpd	%%xmm3,%%xmm5		\n\t"\
			"movaps %%xmm5,(%%esi)	\n\t"\
			"movaps %%xmm7,(%%edi)	\n\t"\
			"movaps %%xmm6,(%%ebx)	\n\t"\
			"movaps %%xmm4,(%%ecx)	\n\t"\
		"/* yi-terms: */			\n\t"\
		"movl	%[__i1],%%esi	\n\t	movaps 0x10(%%esi),%%xmm1	\n\t"\
		"movl	%[__i2],%%edi	\n\t	movaps 0x10(%%edi),%%xmm2	\n\t"\
		"movl	%[__i3],%%eax	\n\t	movaps 0x10(%%eax),%%xmm5	\n\t"\
		"movl	%[__i4],%%ebx	\n\t	movaps 0x10(%%ebx),%%xmm3	\n\t"\
		"movl	%[__i5],%%ecx	\n\t	movaps 0x10(%%ecx),%%xmm4	\n\t"\
		"movl	%[__i6],%%edx	\n\t	movaps 0x10(%%edx),%%xmm6	\n\t"\
		"movl	%[__iC],%%esi	\n\t	subpd 0x10(%%esi),%%xmm1	\n\t"\
		"movl	%[__iB],%%edi	\n\t	subpd 0x10(%%edi),%%xmm2	\n\t"\
		"movl	%[__iA],%%eax	\n\t	subpd 0x10(%%eax),%%xmm5	\n\t"\
		"movl	%[__i9],%%ebx	\n\t	subpd 0x10(%%ebx),%%xmm3	\n\t"\
		"movl	%[__i8],%%ecx	\n\t	subpd 0x10(%%ecx),%%xmm4	\n\t"\
		"movl	%[__i7],%%edx	\n\t	subpd 0x10(%%edx),%%xmm6	\n\t"\
			"movaps %%xmm1,%%xmm7		\n\t"\
			"movaps %%xmm2,%%xmm0		\n\t"\
			"subpd	%%xmm3,%%xmm7		\n\t"\
			"addpd	%%xmm4,%%xmm0		\n\t"\
			"addpd	%%xmm5,%%xmm7		\n\t"\
			"addpd	%%xmm6,%%xmm0		\n\t"\
			"movl	%[__cc],%%ebx		\n\t"\
			"addpd	%%xmm3,%%xmm1		\n\t"\
			"addpd	%%xmm3,%%xmm5		\n\t"\
			"subpd	%%xmm4,%%xmm2		\n\t"\
			"subpd	%%xmm4,%%xmm6		\n\t"\
			"movaps %%xmm0,%%xmm4		\n\t"\
			"movaps %%xmm7,%%xmm3		\n\t"\
			"mulpd	0x040(%%ebx),%%xmm0	\n\t"\
			"mulpd	0x040(%%ebx),%%xmm7	\n\t"\
			"mulpd	0x030(%%ebx),%%xmm4	\n\t"\
			"mulpd	0x030(%%ebx),%%xmm3	\n\t"\
			"subpd	%%xmm7,%%xmm4		\n\t"\
			"addpd	%%xmm0,%%xmm3		\n\t"\
		/* This section uses o-offsets __o1-C with resp. frequencies [2,2,2,2,1,2,1,2,1,1,7,3] - prestore __1-2,B,C into r10,11,14,15, use r12,13 as floaters: */\
		"movl	%[__o1],%%esi	\n\t"\
		"movl	%[__o2],%%edi	\n\t"\
		"movl	%[__oB],%%ecx	\n\t"\
		"movl	%[__oC],%%edx	\n\t"\
			"movaps %%xmm4,(%%edx)	\n\t"\
			"movaps %%xmm1,%%xmm0		\n\t"\
			"movaps %%xmm5,%%xmm4		\n\t"\
			"addpd	%%xmm2,%%xmm0		\n\t"\
			"addpd	%%xmm6,%%xmm4		\n\t"\
			"movaps %%xmm0,(%%ecx)	\n\t"\
			"movaps %%xmm4,%%xmm7		\n\t"\
			"mulpd	0x0b0(%%ebx),%%xmm4	\n\t"\
			"mulpd	0x0e0(%%ebx),%%xmm0	\n\t"\
			"addpd	(%%ecx),%%xmm4	\n\t"\
			"addpd	%%xmm7,%%xmm0		\n\t"\
			"mulpd	0x050(%%ebx),%%xmm4	\n\t"\
			"mulpd	0x050(%%ebx),%%xmm0	\n\t"\
			"movaps %%xmm2,(%%ecx)	\n\t"\
			"movaps %%xmm6,%%xmm7		\n\t"\
			"mulpd	0x0c0(%%ebx),%%xmm6	\n\t"\
			"mulpd	0x0f0(%%ebx),%%xmm2	\n\t"\
			"addpd	(%%ecx),%%xmm6	\n\t"\
			"addpd	%%xmm7,%%xmm2		\n\t"\
			"mulpd	0x060(%%ebx),%%xmm6	\n\t"\
			"mulpd	0x060(%%ebx),%%xmm2	\n\t"\
			"addpd	%%xmm4,%%xmm6		\n\t"\
			"addpd	%%xmm0,%%xmm2		\n\t"\
			"movaps %%xmm1,(%%ecx)	\n\t"\
			"movaps %%xmm5,%%xmm7		\n\t"\
			"mulpd	0x0d0(%%ebx),%%xmm5	\n\t"\
			"mulpd	0x100(%%ebx),%%xmm1	\n\t"\
			"addpd	(%%ecx),%%xmm5	\n\t"\
			"addpd	%%xmm7,%%xmm1		\n\t"\
			"mulpd	0x070(%%ebx),%%xmm5	\n\t"\
			"mulpd	0x070(%%ebx),%%xmm1	\n\t"\
			"addpd	%%xmm4,%%xmm5		\n\t"\
			"addpd	%%xmm0,%%xmm1		\n\t"\
			"movaps (%%edx),%%xmm4	\n\t"\
			"movaps %%xmm3,%%xmm7		\n\t"\
			"movaps %%xmm4,%%xmm0		\n\t"\
			"addpd	%%xmm6,%%xmm7		\n\t"\
			"addpd	%%xmm5,%%xmm0		\n\t"\
			"subpd	%%xmm3,%%xmm6		\n\t"\
			"subpd	%%xmm4,%%xmm5		\n\t"\
			"addpd	%%xmm2,%%xmm6		\n\t"\
			"addpd	%%xmm1,%%xmm5		\n\t"\
			"addpd	%%xmm3,%%xmm2		\n\t"\
			"addpd	%%xmm1,%%xmm4		\n\t"\
		"movl	%[__o6],%%eax	\n\t"\
		"movl	%[__o7],%%ebx	\n\t"\
			"movaps (%%eax),%%xmm3	\n\t"\
			"movaps %%xmm3,%%xmm1		\n\t"\
			"subpd	%%xmm7,%%xmm3		\n\t"\
			"addpd	%%xmm7,%%xmm1		\n\t"\
			"movaps %%xmm3,(%%eax)	\n\t"\
			"movaps %%xmm1,(%%ebx)	\n\t"\
		"movl	%[__o8],%%eax	\n\t"\
		"movl	%[__o5],%%ebx	\n\t"\
			"movaps (%%eax),%%xmm3	\n\t"\
			"movaps %%xmm3,%%xmm1		\n\t"\
			"subpd	%%xmm6,%%xmm3		\n\t"\
			"addpd	%%xmm6,%%xmm1		\n\t"\
			"movaps %%xmm3,(%%eax)	\n\t"\
			"movaps %%xmm1,(%%ebx)	\n\t"\
			"movaps (%%edi),%%xmm3	\n\t"\
			"movaps %%xmm3,%%xmm1		\n\t"\
			"subpd	%%xmm2,%%xmm3		\n\t"\
			"addpd	%%xmm2,%%xmm1		\n\t"\
			"movaps %%xmm3,(%%edi)	\n\t"\
			"movaps %%xmm1,(%%ecx)	\n\t"\
		"movl	%[__o3],%%eax	\n\t"\
		"movl	%[__oA],%%ebx	\n\t"\
			"movaps (%%eax),%%xmm3	\n\t"\
			"movaps %%xmm3,%%xmm1		\n\t"\
			"subpd	%%xmm0,%%xmm3		\n\t"\
			"addpd	%%xmm0,%%xmm1		\n\t"\
			"movaps %%xmm3,(%%eax)	\n\t"\
			"movaps %%xmm1,(%%ebx)	\n\t"\
		"movl	%[__o4],%%eax	\n\t"\
		"movl	%[__o9],%%ebx	\n\t"\
			"movaps (%%eax),%%xmm3	\n\t"\
			"movaps %%xmm3,%%xmm1		\n\t"\
			"subpd	%%xmm5,%%xmm3		\n\t"\
			"addpd	%%xmm5,%%xmm1		\n\t"\
			"movaps %%xmm3,(%%eax)	\n\t"\
			"movaps %%xmm1,(%%ebx)	\n\t"\
			"movaps (%%esi),%%xmm3	\n\t"\
			"movaps %%xmm3,%%xmm1		\n\t"\
			"subpd	%%xmm4,%%xmm3		\n\t"\
			"addpd	%%xmm4,%%xmm1		\n\t"\
			"movaps %%xmm3,(%%esi)	\n\t"\
			"movaps %%xmm1,(%%edx)	\n\t"\
			"/***************/			\n\t"\
			"/* IMAG PARTS: */			\n\t"\
			"/***************/			\n\t"\
		"/* xi-terms need 8 registers for each	side: */\n\t"\
			"movl	%[__cc],%%ebx		\n\t"\
			"movaps -0x010(%%ebx),%%xmm0	\n\t"\
			"movl	%[__i6],%%esi	\n\t	movaps 0x10(%%esi),%%xmm7	\n\t"\
			"movl	%[__i5],%%edi	\n\t	movaps 0x10(%%edi),%%xmm5	\n\t"\
			"movl	%[__i4],%%eax	\n\t	movaps 0x10(%%eax),%%xmm4	\n\t"\
			"movl	%[__i3],%%ebx	\n\t	movaps 0x10(%%ebx),%%xmm6	\n\t"\
			"movl	%[__i2],%%ecx	\n\t	movaps 0x10(%%ecx),%%xmm3	\n\t"\
			"movl	%[__i1],%%edx	\n\t	movaps 0x10(%%edx),%%xmm1	\n\t"\
			"movl	%[__i7],%%esi	\n\t	addpd 0x10(%%esi),%%xmm7	\n\t"\
			"movl	%[__i8],%%edi	\n\t	addpd 0x10(%%edi),%%xmm5	\n\t"\
			"movl	%[__i9],%%eax	\n\t	addpd 0x10(%%eax),%%xmm4	\n\t"\
			"movl	%[__iA],%%ebx	\n\t	addpd 0x10(%%ebx),%%xmm6	\n\t"\
			"movl	%[__iB],%%ecx	\n\t	addpd 0x10(%%ecx),%%xmm3	\n\t"\
			"movl	%[__iC],%%edx	\n\t	addpd 0x10(%%edx),%%xmm1	\n\t"\
			"subpd	%%xmm5,%%xmm1		\n\t"\
			"mulpd	%%xmm0,%%xmm5		\n\t"\
			"subpd	%%xmm6,%%xmm3		\n\t"\
			"mulpd	%%xmm0,%%xmm6		\n\t"\
			"movl	%[__I0],%%eax		\n\t"\
			"movl	%[__cc],%%ebx		\n\t"\
			"movl	%[__O0],%%ecx		\n\t"\
			"subpd	%%xmm7,%%xmm4		\n\t"\
			"mulpd	%%xmm0,%%xmm7		\n\t"\
			"addpd	%%xmm1,%%xmm5		\n\t"\
			"addpd	%%xmm3,%%xmm6		\n\t"\
			"addpd	%%xmm4,%%xmm7		\n\t"\
			"movaps %%xmm5,%%xmm2		\n\t"\
			"addpd	%%xmm6,%%xmm2		\n\t"\
			"addpd	%%xmm7,%%xmm2		\n\t"\
			"movaps 0x10(%%eax),%%xmm0		\n\t"\
			"addpd	%%xmm2,%%xmm0		\n\t"\
			"mulpd	(%%ebx),%%xmm2		\n\t"\
			"movaps %%xmm0,0x10(%%ecx)	\n\t"\
			"addpd	%%xmm2,%%xmm0		\n\t"\
			"movaps 0x010(%%ebx),%%xmm2	\n\t"\
			"mulpd	%%xmm2,%%xmm6		\n\t"\
			"mulpd	%%xmm2,%%xmm5		\n\t"\
			"mulpd	%%xmm2,%%xmm7		\n\t"\
		/* This section uses o-offsets 5,7,9,A,B,C - prestore those into r10-15: */\
		"movl	%[__oA],%%eax	\n\t"\
		"movl	%[__oB],%%edi	\n\t"\
		"movl	%[__oC],%%esi	\n\t"\
			"movaps %%xmm6,0x10(%%esi)	\n\t"\
			"movaps %%xmm5,0x10(%%edi)	\n\t"\
			"movaps %%xmm7,0x10(%%eax)	\n\t"\
			"movl	%[__cc],%%ebx		\n\t"\
			"movaps 0x080(%%ebx),%%xmm2	\n\t"\
			"mulpd	%%xmm2,%%xmm5		\n\t"\
			"mulpd	%%xmm2,%%xmm7		\n\t"\
			"mulpd	%%xmm2,%%xmm6		\n\t"\
			"addpd	%%xmm0,%%xmm5		\n\t"\
			"addpd	%%xmm0,%%xmm7		\n\t"\
			"addpd	%%xmm0,%%xmm6		\n\t"\
			"addpd	0x10(%%esi),%%xmm5	\n\t"\
			"addpd	0x10(%%edi),%%xmm7	\n\t"\
			"addpd	0x10(%%eax),%%xmm6	\n\t"\
			"movaps 0x0a0(%%ebx),%%xmm2	\n\t"\
			"movaps 0x090(%%ebx),%%xmm0	\n\t"\
			"movaps %%xmm4,0x10(%%esi)	\n\t"\
			"movaps %%xmm1,0x10(%%edi)	\n\t"\
			"movaps %%xmm3,0x10(%%eax)	\n\t"\
			"mulpd	%%xmm2,%%xmm4		\n\t"\
			"mulpd	%%xmm2,%%xmm1		\n\t"\
			"mulpd	%%xmm2,%%xmm3		\n\t"\
			"addpd	0x10(%%eax),%%xmm4	\n\t"\
			"subpd	0x10(%%esi),%%xmm1	\n\t"\
			"addpd	0x10(%%edi),%%xmm3	\n\t"\
			"mulpd	%%xmm0,%%xmm4		\n\t"\
			"mulpd	%%xmm0,%%xmm1		\n\t"\
			"mulpd	%%xmm0,%%xmm3		\n\t"\
			"movaps 0x020(%%ebx),%%xmm2	\n\t"\
			"addpd	0x10(%%edi),%%xmm4	\n\t"\
			"subpd	0x10(%%eax),%%xmm1	\n\t"\
			"subpd	0x10(%%esi),%%xmm3	\n\t"\
			"mulpd	%%xmm2,%%xmm4		\n\t"\
			"mulpd	%%xmm2,%%xmm1		\n\t"\
			"mulpd	%%xmm2,%%xmm3		\n\t"\
			"movaps -0x010(%%ebx),%%xmm0	\n\t"\
		"movl	%[__o5],%%edx	\n\t"\
		"movl	%[__o7],%%ecx	\n\t"\
		"movl	%[__o9],%%ebx	\n\t"\
			"subpd	%%xmm4,%%xmm6		\n\t"\
			"mulpd	%%xmm0,%%xmm4		\n\t"\
			"subpd	%%xmm1,%%xmm7		\n\t"\
			"mulpd	%%xmm0,%%xmm1		\n\t"\
			"subpd	%%xmm3,%%xmm5		\n\t"\
			"mulpd	%%xmm0,%%xmm3		\n\t"\
			"movaps %%xmm7,0x10(%%eax)	\n\t"\
			"movaps %%xmm5,0x10(%%edx)	\n\t"\
			"addpd	%%xmm6,%%xmm4		\n\t"\
			"addpd	%%xmm1,%%xmm7		\n\t"\
			"addpd	%%xmm3,%%xmm5		\n\t"\
			"movaps %%xmm5,0x10(%%esi)	\n\t"\
			"movaps %%xmm7,0x10(%%edi)	\n\t"\
			"movaps %%xmm6,0x10(%%ebx)	\n\t"\
			"movaps %%xmm4,0x10(%%ecx)	\n\t"\
		"/* yr-terms: */			\n\t"\
			"movl	%[__i1],%%edx	\n\t	movaps (%%edx),%%xmm1	\n\t"\
			"movl	%[__i2],%%ecx	\n\t	movaps (%%ecx),%%xmm2	\n\t"\
			"movl	%[__i3],%%ebx	\n\t	movaps (%%ebx),%%xmm5	\n\t"\
			"movl	%[__i4],%%eax	\n\t	movaps (%%eax),%%xmm3	\n\t"\
			"movl	%[__i5],%%edi	\n\t	movaps (%%edi),%%xmm4	\n\t"\
			"movl	%[__i6],%%esi	\n\t	movaps (%%esi),%%xmm6	\n\t"\
			"movl	%[__iC],%%edx	\n\t	subpd (%%edx),%%xmm1	\n\t"\
			"movl	%[__iB],%%ecx	\n\t	subpd (%%ecx),%%xmm2	\n\t"\
			"movl	%[__iA],%%ebx	\n\t	subpd (%%ebx),%%xmm5	\n\t"\
			"movl	%[__i9],%%eax	\n\t	subpd (%%eax),%%xmm3	\n\t"\
			"movl	%[__i8],%%edi	\n\t	subpd (%%edi),%%xmm4	\n\t"\
			"movl	%[__i7],%%esi	\n\t	subpd (%%esi),%%xmm6	\n\t"\
			"movaps %%xmm1,%%xmm7		\n\t"\
			"movaps %%xmm2,%%xmm0		\n\t"\
			"subpd	%%xmm3,%%xmm7		\n\t"\
			"addpd	%%xmm4,%%xmm0		\n\t"\
			"addpd	%%xmm5,%%xmm7		\n\t"\
			"addpd	%%xmm6,%%xmm0		\n\t"\
			"movl	%[__cc],%%ebx		\n\t"\
			"addpd	%%xmm3,%%xmm1		\n\t"\
			"addpd	%%xmm3,%%xmm5		\n\t"\
			"subpd	%%xmm4,%%xmm2		\n\t"\
			"subpd	%%xmm4,%%xmm6		\n\t"\
			"movaps %%xmm0,%%xmm4		\n\t"\
			"movaps %%xmm7,%%xmm3		\n\t"\
			"mulpd	0x040(%%ebx),%%xmm0	\n\t"\
			"mulpd	0x040(%%ebx),%%xmm7	\n\t"\
			"mulpd	0x030(%%ebx),%%xmm4	\n\t"\
			"mulpd	0x030(%%ebx),%%xmm3	\n\t"\
			"subpd	%%xmm7,%%xmm4		\n\t"\
			"addpd	%%xmm0,%%xmm3		\n\t"\
		/* This section uses o-offsets __o1-C with resp. frequencies [3,7,1,1,2,1,2,1,2,2,2,2] - prestore __1-2,C into r10,11,15, use r12-14 as floaters: */\
		"movl	%[__o1],%%edx	\n\t"\
		"movl	%[__o2],%%ecx	\n\t"\
		"movl	%[__oC],%%esi	\n\t"\
			"movaps %%xmm4,0x10(%%edx)	\n\t"\
			"movaps %%xmm1,%%xmm0		\n\t"\
			"movaps %%xmm5,%%xmm4		\n\t"\
			"addpd	%%xmm2,%%xmm0		\n\t"\
			"addpd	%%xmm6,%%xmm4		\n\t"\
			"movaps %%xmm0,0x10(%%ecx)	\n\t"\
			"movaps %%xmm4,%%xmm7		\n\t"\
			"mulpd	0x0b0(%%ebx),%%xmm4	\n\t"\
			"mulpd	0x0e0(%%ebx),%%xmm0	\n\t"\
			"addpd	0x10(%%ecx),%%xmm4	\n\t"\
			"addpd	%%xmm7,%%xmm0		\n\t"\
			"mulpd	0x050(%%ebx),%%xmm4	\n\t"\
			"mulpd	0x050(%%ebx),%%xmm0	\n\t"\
			"movaps %%xmm2,0x10(%%ecx)	\n\t"\
			"movaps %%xmm6,%%xmm7		\n\t"\
			"mulpd	0x0c0(%%ebx),%%xmm6	\n\t"\
			"mulpd	0x0f0(%%ebx),%%xmm2	\n\t"\
			"addpd	0x10(%%ecx),%%xmm6	\n\t"\
			"addpd	%%xmm7,%%xmm2		\n\t"\
			"mulpd	0x060(%%ebx),%%xmm6	\n\t"\
			"mulpd	0x060(%%ebx),%%xmm2	\n\t"\
			"addpd	%%xmm4,%%xmm6		\n\t"\
			"addpd	%%xmm0,%%xmm2		\n\t"\
			"movaps %%xmm1,0x10(%%ecx)	\n\t"\
			"movaps %%xmm5,%%xmm7		\n\t"\
			"mulpd	0x0d0(%%ebx),%%xmm5	\n\t"\
			"mulpd	0x100(%%ebx),%%xmm1	\n\t"\
			"addpd	0x10(%%ecx),%%xmm5	\n\t"\
			"addpd	%%xmm7,%%xmm1		\n\t"\
			"mulpd	0x070(%%ebx),%%xmm5	\n\t"\
			"mulpd	0x070(%%ebx),%%xmm1	\n\t"\
			"addpd	%%xmm4,%%xmm5		\n\t"\
			"addpd	%%xmm0,%%xmm1		\n\t"\
			"movaps 0x10(%%edx),%%xmm4	\n\t"\
			"movaps %%xmm3,%%xmm7		\n\t"\
			"movaps %%xmm4,%%xmm0		\n\t"\
			"addpd	%%xmm6,%%xmm7		\n\t"\
			"addpd	%%xmm5,%%xmm0		\n\t"\
			"subpd	%%xmm3,%%xmm6		\n\t"\
			"subpd	%%xmm4,%%xmm5		\n\t"\
			"addpd	%%xmm2,%%xmm6		\n\t"\
			"addpd	%%xmm1,%%xmm5		\n\t"\
			"addpd	%%xmm3,%%xmm2		\n\t"\
			"addpd	%%xmm1,%%xmm4		\n\t"\
		"movl	%[__o7],%%ebx	\n\t"\
		"movl	%[__o6],%%eax	\n\t"\
			"movaps 0x10(%%ebx),%%xmm3	\n\t"\
			"movaps %%xmm3,%%xmm1		\n\t"\
			"subpd	%%xmm7,%%xmm3		\n\t"\
			"addpd	%%xmm7,%%xmm1		\n\t"\
			"movaps %%xmm3,0x10(%%ebx)	\n\t"\
			"movaps %%xmm1,0x10(%%eax)	\n\t"\
		"movl	%[__o5],%%ebx	\n\t"\
		"movl	%[__o8],%%eax	\n\t"\
			"movaps 0x10(%%ebx),%%xmm3	\n\t"\
			"movaps %%xmm3,%%xmm1		\n\t"\
			"subpd	%%xmm6,%%xmm3		\n\t"\
			"addpd	%%xmm6,%%xmm1		\n\t"\
			"movaps %%xmm3,0x10(%%ebx)	\n\t"\
			"movaps %%xmm1,0x10(%%eax)	\n\t"\
		"movl	%[__oB],%%ebx	\n\t"\
			"movaps 0x10(%%ebx),%%xmm3	\n\t"\
			"movaps %%xmm3,%%xmm1		\n\t"\
			"subpd	%%xmm2,%%xmm3		\n\t"\
			"addpd	%%xmm2,%%xmm1		\n\t"\
			"movaps %%xmm3,0x10(%%ebx)	\n\t"\
			"movaps %%xmm1,0x10(%%ecx)	\n\t"\
		"movl	%[__oA],%%ebx	\n\t"\
		"movl	%[__o3],%%eax	\n\t"\
			"movaps 0x10(%%ebx),%%xmm3	\n\t"\
			"movaps %%xmm3,%%xmm1		\n\t"\
			"subpd	%%xmm0,%%xmm3		\n\t"\
			"addpd	%%xmm0,%%xmm1		\n\t"\
			"movaps %%xmm3,0x10(%%ebx)	\n\t"\
			"movaps %%xmm1,0x10(%%eax)	\n\t"\
		"movl	%[__o9],%%ebx	\n\t"\
		"movl	%[__o4],%%eax	\n\t"\
			"movaps 0x10(%%ebx),%%xmm3	\n\t"\
			"movaps %%xmm3,%%xmm1		\n\t"\
			"subpd	%%xmm5,%%xmm3		\n\t"\
			"addpd	%%xmm5,%%xmm1		\n\t"\
			"movaps %%xmm3,0x10(%%ebx)	\n\t"\
			"movaps %%xmm1,0x10(%%eax)	\n\t"\
			"movaps 0x10(%%esi),%%xmm3	\n\t"\
			"movaps %%xmm3,%%xmm1		\n\t"\
			"subpd	%%xmm4,%%xmm3		\n\t"\
			"addpd	%%xmm4,%%xmm1		\n\t"\
			"movaps %%xmm3,0x10(%%esi)	\n\t"\
			"movaps %%xmm1,0x10(%%edx)	\n\t"\
		"popl %%ebx	\n\t"\
			:					/* outputs: none */\
			: [__cc] "m" (Xcc)	/* All inputs from memory addresses here */\
			 ,[__I0] "m" (XI0)\
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
			 ,[__iB] "m" (XiB)\
			 ,[__iC] "m" (XiC)\
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
			 ,[__oB] "m" (XoB)\
			 ,[__oC] "m" (XoC)\
			: "cc","memory","eax",/*"ebx",*/"ecx","edx","esi","edi","xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7"		/* Clobbered registers */\
			);\
		}

	  #else

		#ifdef USE_AVX

			#define SSE2_RADIX_13_DFT(Xcc, XI0,Xi1,Xi2,Xi3,Xi4,Xi5,Xi6,Xi7,Xi8,Xi9,XiA,XiB,XiC, XO0,Xo1,Xo2,Xo3,Xo4,Xo5,Xo6,Xo7,Xo8,Xo9,XoA,XoB,XoC)\
			{\
			__asm__ volatile (\
				"movq	%[__I0],%%rax							\n\t"\
				"movq	%[__cc],%%rbx							\n\t"\
				"movq	%[__O0],%%rcx							\n\t"\
			/* xr-terms:                                                      yi-terms: */\
				"movq	%[__i1],%%r15	\n\t	vmovaps	(%%r15),%%ymm1			\n\t	vmovaps	0x20(%%r15),%%ymm9 	\n\t"\
				"movq	%[__i2],%%r14	\n\t	vmovaps	(%%r14),%%ymm3			\n\t	vmovaps	0x20(%%r14),%%ymm10	\n\t"\
				"movq	%[__i3],%%r13	\n\t	vmovaps	(%%r13),%%ymm6			\n\t	vmovaps	0x20(%%r13),%%ymm13	\n\t"\
				"movq	%[__i4],%%r12	\n\t	vmovaps	(%%r12),%%ymm4			\n\t	vmovaps	0x20(%%r12),%%ymm11	\n\t"\
				"movq	%[__i5],%%r11	\n\t	vmovaps	(%%r11),%%ymm5			\n\t	vmovaps	0x20(%%r11),%%ymm12	\n\t"\
				"movq	%[__i6],%%r10	\n\t	vmovaps	(%%r10),%%ymm7			\n\t	vmovaps	0x20(%%r10),%%ymm14	\n\t"\
				"vmovaps	-0x20(%%rbx),%%ymm0		\n\t"\
				"movq	%[__iC],%%r15	\n\t	vaddpd	(%%r15),%%ymm1,%%ymm1	\n\t	vsubpd	0x20(%%r15),%%ymm9 ,%%ymm9 	\n\t"\
				"movq	%[__iB],%%r14	\n\t	vaddpd	(%%r14),%%ymm3,%%ymm3	\n\t	vsubpd	0x20(%%r14),%%ymm10,%%ymm10	\n\t"\
				"movq	%[__iA],%%r13	\n\t	vaddpd	(%%r13),%%ymm6,%%ymm6	\n\t	vsubpd	0x20(%%r13),%%ymm13,%%ymm13	\n\t"\
				"movq	%[__i9],%%r12	\n\t	vaddpd	(%%r12),%%ymm4,%%ymm4	\n\t	vsubpd	0x20(%%r12),%%ymm11,%%ymm11	\n\t"\
				"movq	%[__i8],%%r11	\n\t	vaddpd	(%%r11),%%ymm5,%%ymm5	\n\t	vsubpd	0x20(%%r11),%%ymm12,%%ymm12	\n\t"\
				"movq	%[__i7],%%r10	\n\t	vaddpd	(%%r10),%%ymm7,%%ymm7	\n\t	vsubpd	0x20(%%r10),%%ymm14,%%ymm14	\n\t"\
				"vsubpd	%%ymm5,%%ymm1,%%ymm1					\n\t			vmovaps	%%ymm9 ,%%ymm15					\n\t"\
				"vmulpd	%%ymm0,%%ymm5,%%ymm5					\n\t			vmovaps	%%ymm10,%%ymm8 		 			\n\t"\
				"vsubpd	%%ymm6,%%ymm3,%%ymm3					\n\t			vsubpd	%%ymm11,%%ymm15,%%ymm15			\n\t"\
				"vmulpd	%%ymm0,%%ymm6,%%ymm6					\n\t			vaddpd	%%ymm12,%%ymm8 ,%%ymm8 			\n\t"\
				"vsubpd	%%ymm7,%%ymm4,%%ymm4					\n\t			vaddpd	%%ymm13,%%ymm15,%%ymm15			\n\t"\
				"vmulpd	%%ymm0,%%ymm7,%%ymm7					\n\t			vaddpd	%%ymm14,%%ymm8 ,%%ymm8 			\n\t"\
				"vaddpd	%%ymm1,%%ymm5,%%ymm5					\n\t			vaddpd	%%ymm11,%%ymm9 ,%%ymm9 			\n\t"\
				"vaddpd	%%ymm3,%%ymm6,%%ymm6					\n\t			vaddpd	%%ymm11,%%ymm13,%%ymm13			\n\t"\
				"vaddpd	%%ymm4,%%ymm7,%%ymm7					\n\t			vsubpd	%%ymm12,%%ymm10,%%ymm10			\n\t"\
				"vmovaps	%%ymm5,%%ymm2						\n\t			vsubpd	%%ymm12,%%ymm14,%%ymm14			\n\t"\
				"vaddpd	%%ymm6,%%ymm2,%%ymm2					\n\t			vmovaps	%%ymm8 ,%%ymm12					\n\t"\
				"vaddpd	%%ymm7,%%ymm2,%%ymm2					\n\t			vmovaps	%%ymm15,%%ymm11					\n\t"\
				"vmovaps	(%%rax),%%ymm0						\n\t			vmulpd	0x80(%%rbx),%%ymm8 ,%%ymm8 	\n\t"\
				"vaddpd	%%ymm2,%%ymm0,%%ymm0					\n\t			vmulpd	0x80(%%rbx),%%ymm15,%%ymm15	\n\t"\
				"vmulpd (%%rbx),%%ymm2,%%ymm2					\n\t			vmulpd	0x60(%%rbx),%%ymm12,%%ymm12	\n\t"\
				"vmovaps	%%ymm0,(%%rcx)						\n\t			vmulpd	0x60(%%rbx),%%ymm11,%%ymm11	\n\t"\
				"vaddpd	%%ymm2,%%ymm0,%%ymm0					\n\t			vsubpd	%%ymm15,%%ymm12,%%ymm12			\n\t"\
				"vmovaps	0x20(%%rbx),%%ymm2					\n\t			vaddpd	%%ymm8 ,%%ymm11,%%ymm11			\n\t"\
			/* lcol section uses o-offsets 1,2,3 - prestore those into r10-12: */\
			"movq	%[__o1],%%r10		\n\t"		/* rcol section uses o-offsets __oB,C - prestore those into r14,15: */\
			"movq	%[__o2],%%r11								\n\t		movq	%[__oB],%%r14	\n\t"\
			"movq	%[__o3],%%r12								\n\t		movq	%[__oC],%%r15	\n\t"\
				"vmulpd	%%ymm2,%%ymm6,%%ymm6					\n\t			vmovaps	%%ymm12,(%%r15)			\n\t"\
				"vmulpd	%%ymm2,%%ymm5,%%ymm5					\n\t			vmovaps	%%ymm9 ,%%ymm8 			\n\t"\
				"vmulpd	%%ymm2,%%ymm7,%%ymm7					\n\t			vmovaps	%%ymm13,%%ymm12			\n\t"\
				"vmovaps %%ymm6,(%%r10)							\n\t			vaddpd	%%ymm10,%%ymm8 ,%%ymm8 			\n\t"\
				"vmovaps %%ymm5,(%%r11)							\n\t			vaddpd	%%ymm14,%%ymm12,%%ymm12			\n\t"\
				"vmovaps %%ymm7,(%%r12)							\n\t			vmovaps	%%ymm8 ,(%%r14)			\n\t"\
				"vmovaps 0x100(%%rbx),%%ymm2					\n\t			vmovaps	%%ymm12,%%ymm15			\n\t"\
				"vmulpd	%%ymm2,%%ymm5,%%ymm5					\n\t			vmulpd 0x160(%%rbx),%%ymm12,%%ymm12	\n\t"\
				"vmulpd	%%ymm2,%%ymm7,%%ymm7					\n\t			vmulpd 0x1c0(%%rbx),%%ymm8 ,%%ymm8 		\n\t"\
				"vmulpd	%%ymm2,%%ymm6,%%ymm6					\n\t			vaddpd	(%%r14),%%ymm12,%%ymm12			\n\t"\
				"vaddpd	%%ymm0,%%ymm5,%%ymm5					\n\t			vaddpd	%%ymm15,%%ymm8 ,%%ymm8 			\n\t"\
				"vaddpd	%%ymm0,%%ymm7,%%ymm7					\n\t			vmulpd	0xa0(%%rbx),%%ymm12,%%ymm12	\n\t"\
				"vaddpd	%%ymm0,%%ymm6,%%ymm6					\n\t			vmulpd	0xa0(%%rbx),%%ymm8 ,%%ymm8 		\n\t"\
				"vaddpd (%%r10),%%ymm5,%%ymm5					\n\t			vmovaps	%%ymm10,(%%r14)			\n\t"\
				"vaddpd (%%r11),%%ymm7,%%ymm7					\n\t			vmovaps	%%ymm14,%%ymm15			\n\t"\
				"vaddpd (%%r12),%%ymm6,%%ymm6					\n\t			vmulpd 0x180(%%rbx),%%ymm14,%%ymm14	\n\t"\
				"vmovaps 0x140(%%rbx),%%ymm2					\n\t			vmulpd 0x1e0(%%rbx),%%ymm10,%%ymm10	\n\t"\
				"vmovaps 0x120(%%rbx),%%ymm0					\n\t			vaddpd	(%%r14),%%ymm14,%%ymm14			\n\t"\
				"vmovaps %%ymm4,(%%r10)							\n\t			vaddpd	%%ymm15,%%ymm10,%%ymm10			\n\t"\
				"vmovaps %%ymm1,(%%r11)							\n\t			vmulpd	0xc0(%%rbx),%%ymm14,%%ymm14	\n\t"\
				"vmovaps %%ymm3,(%%r12)							\n\t			vmulpd	0xc0(%%rbx),%%ymm10,%%ymm10	\n\t"\
				"vmulpd	%%ymm2,%%ymm4,%%ymm4					\n\t			vaddpd	%%ymm12,%%ymm14,%%ymm14			\n\t"\
				"vmulpd	%%ymm2,%%ymm1,%%ymm1					\n\t			vaddpd	%%ymm8 ,%%ymm10,%%ymm10			\n\t"\
				"vmulpd	%%ymm2,%%ymm3,%%ymm3					\n\t			vmovaps	%%ymm9 ,(%%r14)			\n\t"\
				"vaddpd (%%r12),%%ymm4,%%ymm4					\n\t			vmovaps	%%ymm13,%%ymm15			\n\t"\
				"vsubpd (%%r10),%%ymm1,%%ymm1					\n\t			vmulpd 0x1a0(%%rbx),%%ymm13,%%ymm13	\n\t"\
				"vaddpd (%%r11),%%ymm3,%%ymm3					\n\t			vmulpd 0x200(%%rbx),%%ymm9 ,%%ymm9 		\n\t"\
				"vmulpd	%%ymm0,%%ymm4,%%ymm4					\n\t			vaddpd	(%%r14),%%ymm13,%%ymm13			\n\t"\
				"vmulpd	%%ymm0,%%ymm1,%%ymm1					\n\t			vaddpd	%%ymm15,%%ymm9 ,%%ymm9 			\n\t"\
				"vmulpd	%%ymm0,%%ymm3,%%ymm3					\n\t			vmulpd	0xe0(%%rbx),%%ymm13,%%ymm13	\n\t"\
				"vmovaps 0x40(%%rbx),%%ymm2						\n\t			vmulpd	0xe0(%%rbx),%%ymm9 ,%%ymm9 		\n\t"\
				"vaddpd (%%r11),%%ymm4,%%ymm4					\n\t			vaddpd	%%ymm12,%%ymm13,%%ymm13			\n\t"\
				"vsubpd (%%r12),%%ymm1,%%ymm1					\n\t			vaddpd	%%ymm8 ,%%ymm9 ,%%ymm9 			\n\t"\
				"vsubpd (%%r10),%%ymm3,%%ymm3					\n\t			vmovaps	(%%r15),%%ymm12			\n\t"\
				"vmulpd	%%ymm2,%%ymm4,%%ymm4					\n\t			vmovaps	%%ymm11,%%ymm15			\n\t"\
				"vmulpd	%%ymm2,%%ymm1,%%ymm1					\n\t			vmovaps	%%ymm12,%%ymm8 			\n\t"\
				"vmulpd	%%ymm2,%%ymm3,%%ymm3					\n\t			vaddpd	%%ymm14,%%ymm15,%%ymm15			\n\t"\
				"vmovaps -0x20(%%rbx),%%ymm0					\n\t			vaddpd	%%ymm13,%%ymm8 ,%%ymm8 			\n\t"\
				"vsubpd	%%ymm4,%%ymm6,%%ymm6					\n\t			vsubpd	%%ymm11,%%ymm14,%%ymm14			\n\t"\
				"vmulpd	%%ymm0,%%ymm4,%%ymm4					\n\t			vsubpd	%%ymm12,%%ymm13,%%ymm13			\n\t"\
				"vsubpd	%%ymm1,%%ymm7,%%ymm7					\n\t			vaddpd	%%ymm10,%%ymm14,%%ymm14			\n\t"\
				"vmulpd	%%ymm0,%%ymm1,%%ymm1					\n\t			vaddpd	%%ymm9 ,%%ymm13,%%ymm13			\n\t"\
				"vsubpd	%%ymm3,%%ymm5,%%ymm5					\n\t			vaddpd	%%ymm11,%%ymm10,%%ymm10			\n\t"\
				"vmulpd	%%ymm0,%%ymm3,%%ymm3					\n\t			vaddpd	%%ymm9 ,%%ymm12,%%ymm12			\n\t"\
				"vaddpd	%%ymm6,%%ymm4,%%ymm4					\n\t"\
				"vaddpd	%%ymm7,%%ymm1,%%ymm1					\n\t"\
				"vaddpd	%%ymm5,%%ymm3,%%ymm3					\n\t"\
			/* yi-data in ymm8,10,12,13,14,15; ymm0,2,9,11 free: */\
			"movq	%[__o6],%%r10	\n\t				movq	%[__o3],%%r12	\n\t"\
			"movq	%[__o7],%%r11	\n\t				movq	%[__oA],%%r13	\n\t"\
				"vmovaps	%%ymm4 ,%%ymm0						\n\t			vmovaps	%%ymm7 ,%%ymm9 			\n\t"\
				"vsubpd	%%ymm15,%%ymm4,%%ymm4					\n\t			vsubpd	%%ymm8 ,%%ymm7,%%ymm7	\n\t"\
				"vaddpd	%%ymm15,%%ymm0,%%ymm0					\n\t			vaddpd	%%ymm8 ,%%ymm9,%%ymm9	\n\t"\
				"vmovaps	%%ymm4 ,(%%r10)						\n\t			vmovaps	%%ymm7 ,(%%r12)	\n\t"\
				"vmovaps	%%ymm0 ,(%%r11)						\n\t			vmovaps	%%ymm9 ,(%%r13)	\n\t"\
			"movq	%[__o8],%%r10	\n\t				movq	%[__o4],%%r12	\n\t"\
			"movq	%[__o5],%%r11	\n\t				movq	%[__o9],%%r13	\n\t"\
				"vmovaps	%%ymm5 ,%%ymm0						\n\t			vmovaps	%%ymm6 ,%%ymm9 			\n\t"\
				"vsubpd	%%ymm14,%%ymm5,%%ymm5					\n\t			vsubpd	%%ymm13,%%ymm6,%%ymm6	\n\t"\
				"vaddpd	%%ymm14,%%ymm0,%%ymm0					\n\t			vaddpd	%%ymm13,%%ymm9,%%ymm9	\n\t"\
				"vmovaps	%%ymm5 ,(%%r10)						\n\t			vmovaps	%%ymm6 ,(%%r12)	\n\t"\
				"vmovaps	%%ymm0 ,(%%r11)						\n\t			vmovaps	%%ymm9 ,(%%r13)	\n\t"\
			"movq	%[__o2],%%r10	\n\t				movq	%[__o1],%%r12	\n\t"\
			"movq	%[__oB],%%r11	\n\t				movq	%[__oC],%%r13	\n\t"\
				"vmovaps	%%ymm1 ,%%ymm0						\n\t			vmovaps	%%ymm3 ,%%ymm9 			\n\t"\
				"vsubpd	%%ymm10,%%ymm1,%%ymm1					\n\t			vsubpd	%%ymm12,%%ymm3,%%ymm3	\n\t"\
				"vaddpd	%%ymm10,%%ymm0,%%ymm0					\n\t			vaddpd	%%ymm12,%%ymm9,%%ymm9	\n\t"\
				"vmovaps	%%ymm1 ,(%%r10)						\n\t			vmovaps	%%ymm3 ,(%%r12)	\n\t"\
				"vmovaps	%%ymm0 ,(%%r11)						\n\t			vmovaps	%%ymm9 ,(%%r13)	\n\t"\
				/****************************************************************/\
				/*                      IMAG PARTS:                             */\
				/****************************************************************/\
				"addq	$0x20,%%rax		 						\n\t"\
				"addq	$0x20,%%rcx								\n\t"\
			/* xi-terms:                                                      yr-terms: */\
				"movq	%[__i1],%%r15	\n\t	vmovaps 0x20(%%r15),%%ymm1			\n\t	vmovaps	(%%r15),%%ymm9 	\n\t"\
				"movq	%[__i2],%%r14	\n\t	vmovaps 0x20(%%r14),%%ymm3			\n\t	vmovaps	(%%r14),%%ymm10	\n\t"\
				"movq	%[__i3],%%r13	\n\t	vmovaps 0x20(%%r13),%%ymm6			\n\t	vmovaps	(%%r13),%%ymm13	\n\t"\
				"movq	%[__i4],%%r12	\n\t	vmovaps 0x20(%%r12),%%ymm4			\n\t	vmovaps	(%%r12),%%ymm11	\n\t"\
				"movq	%[__i5],%%r11	\n\t	vmovaps 0x20(%%r11),%%ymm5			\n\t	vmovaps	(%%r11),%%ymm12	\n\t"\
				"movq	%[__i6],%%r10	\n\t	vmovaps 0x20(%%r10),%%ymm7			\n\t	vmovaps	(%%r10),%%ymm14	\n\t"\
				"vmovaps -0x20(%%rbx),%%ymm0					\n\t"\
				"movq	%[__iC],%%r15	\n\t	vaddpd	0x20(%%r15),%%ymm1,%%ymm1	\n\t	vsubpd	(%%r15),%%ymm9 ,%%ymm9 	\n\t"\
				"movq	%[__iB],%%r14	\n\t	vaddpd	0x20(%%r14),%%ymm3,%%ymm3	\n\t	vsubpd	(%%r14),%%ymm10,%%ymm10	\n\t"\
				"movq	%[__iA],%%r13	\n\t	vaddpd	0x20(%%r13),%%ymm6,%%ymm6	\n\t	vsubpd	(%%r13),%%ymm13,%%ymm13	\n\t"\
				"movq	%[__i9],%%r12	\n\t	vaddpd	0x20(%%r12),%%ymm4,%%ymm4	\n\t	vsubpd	(%%r12),%%ymm11,%%ymm11	\n\t"\
				"movq	%[__i8],%%r11	\n\t	vaddpd	0x20(%%r11),%%ymm5,%%ymm5	\n\t	vsubpd	(%%r11),%%ymm12,%%ymm12	\n\t"\
				"movq	%[__i7],%%r10	\n\t	vaddpd	0x20(%%r10),%%ymm7,%%ymm7	\n\t	vsubpd	(%%r10),%%ymm14,%%ymm14	\n\t"\
				"vsubpd	%%ymm5,%%ymm1,%%ymm1					\n\t			vmovaps	%%ymm9 ,%%ymm15			\n\t"\
				"vmulpd	%%ymm0,%%ymm5,%%ymm5					\n\t			vmovaps	%%ymm10,%%ymm8 			\n\t"\
				"vsubpd	%%ymm6,%%ymm3,%%ymm3					\n\t			vsubpd	%%ymm11,%%ymm15,%%ymm15			\n\t"\
				"vmulpd	%%ymm0,%%ymm6,%%ymm6					\n\t			vaddpd	%%ymm12,%%ymm8 ,%%ymm8 			\n\t"\
				"vsubpd	%%ymm7,%%ymm4,%%ymm4					\n\t			vaddpd	%%ymm13,%%ymm15,%%ymm15			\n\t"\
				"vmulpd	%%ymm0,%%ymm7,%%ymm7					\n\t			vaddpd	%%ymm14,%%ymm8 ,%%ymm8 			\n\t"\
				"vaddpd	%%ymm1,%%ymm5,%%ymm5					\n\t			vaddpd	%%ymm11,%%ymm9 ,%%ymm9 			\n\t"\
				"vaddpd	%%ymm3,%%ymm6,%%ymm6					\n\t			vaddpd	%%ymm11,%%ymm13,%%ymm13			\n\t"\
				"vaddpd	%%ymm4,%%ymm7,%%ymm7					\n\t			vsubpd	%%ymm12,%%ymm10,%%ymm10			\n\t"\
				"vmovaps	%%ymm5,%%ymm2						\n\t			vsubpd	%%ymm12,%%ymm14,%%ymm14			\n\t"\
				"vaddpd	%%ymm6,%%ymm2,%%ymm2					\n\t			vmovaps	%%ymm8 ,%%ymm12			\n\t"\
				"vaddpd	%%ymm7,%%ymm2,%%ymm2					\n\t			vmovaps	%%ymm15,%%ymm11			\n\t"\
				"vmovaps	(%%rax),%%ymm0						\n\t			vmulpd	0x80(%%rbx),%%ymm8 ,%%ymm8 	\n\t"\
				"vaddpd	%%ymm2,%%ymm0,%%ymm0					\n\t			vmulpd	0x80(%%rbx),%%ymm15,%%ymm15	\n\t"\
				"vmulpd	(%%rbx),%%ymm2,%%ymm2					\n\t			vmulpd	0x60(%%rbx),%%ymm12,%%ymm12	\n\t"\
				"vmovaps	%%ymm0,(%%rcx)						\n\t			vmulpd	0x60(%%rbx),%%ymm11,%%ymm11	\n\t"\
				"vaddpd	%%ymm2,%%ymm0,%%ymm0					\n\t			vsubpd	%%ymm15,%%ymm12,%%ymm12			\n\t"\
				"vmovaps	0x20(%%rbx),%%ymm2					\n\t			vaddpd	%%ymm8 ,%%ymm11,%%ymm11			\n\t"\
			/* lcol section uses o-offsets A,B,C - prestore those into r13-15: */\
			"movq	%[__oA],%%r13	\n\t"						/* rcol section uses o-offsets __o1,2 - prestore those into r10,11: */\
			"movq	%[__oB],%%r14	\n\t									movq	%[__o1],%%r10	\n\t"\
			"movq	%[__oC],%%r15	\n\t									movq	%[__o2],%%r11	\n\t"\
				"vmulpd	%%ymm2,%%ymm6,%%ymm6					\n\t			vmovaps	%%ymm12,0x20(%%r10)	\n\t"\
				"vmulpd	%%ymm2,%%ymm5,%%ymm5					\n\t			vmovaps	%%ymm9 ,%%ymm8 			\n\t"\
				"vmulpd	%%ymm2,%%ymm7,%%ymm7					\n\t			vmovaps	%%ymm13,%%ymm12			\n\t"\
				"vmovaps %%ymm6,0x20(%%r15)						\n\t			vaddpd	%%ymm10,%%ymm8 ,%%ymm8 			\n\t"\
				"vmovaps %%ymm5,0x20(%%r14)						\n\t			vaddpd	%%ymm14,%%ymm12,%%ymm12			\n\t"\
				"vmovaps %%ymm7,0x20(%%r13)						\n\t			vmovaps	%%ymm8 ,0x20(%%r11)	\n\t"\
				"vmovaps 0x100(%%rbx),%%ymm2					\n\t			vmovaps	%%ymm12,%%ymm15			\n\t"\
				"vmulpd	%%ymm2,%%ymm5,%%ymm5					\n\t			vmulpd 0x160(%%rbx),%%ymm12,%%ymm12	\n\t"\
				"vmulpd	%%ymm2,%%ymm7,%%ymm7					\n\t			vmulpd 0x1c0(%%rbx),%%ymm8 ,%%ymm8 		\n\t"\
				"vmulpd	%%ymm2,%%ymm6,%%ymm6					\n\t			vaddpd	0x20(%%r11),%%ymm12,%%ymm12	\n\t"\
				"vaddpd	%%ymm0,%%ymm5,%%ymm5					\n\t			vaddpd	%%ymm15,%%ymm8 ,%%ymm8 			\n\t"\
				"vaddpd	%%ymm0,%%ymm7,%%ymm7					\n\t			vmulpd	0xa0(%%rbx),%%ymm12,%%ymm12	\n\t"\
				"vaddpd	%%ymm0,%%ymm6,%%ymm6					\n\t			vmulpd	0xa0(%%rbx),%%ymm8 ,%%ymm8 		\n\t"\
				"vaddpd	0x20(%%r15),%%ymm5,%%ymm5				\n\t			vmovaps	%%ymm10,0x20(%%r11)	\n\t"\
				"vaddpd	0x20(%%r14),%%ymm7,%%ymm7				\n\t			vmovaps	%%ymm14,%%ymm15			\n\t"\
				"vaddpd	0x20(%%r13),%%ymm6,%%ymm6				\n\t			vmulpd 0x180(%%rbx),%%ymm14,%%ymm14	\n\t"\
				"vmovaps 0x140(%%rbx),%%ymm2					\n\t			vmulpd 0x1e0(%%rbx),%%ymm10,%%ymm10	\n\t"\
				"vmovaps 0x120(%%rbx),%%ymm0					\n\t			vaddpd	0x20(%%r11),%%ymm14,%%ymm14	\n\t"\
				"vmovaps %%ymm4,0x20(%%r15)						\n\t			vaddpd	%%ymm15,%%ymm10,%%ymm10			\n\t"\
				"vmovaps %%ymm1,0x20(%%r14)						\n\t			vmulpd	0xc0(%%rbx),%%ymm14,%%ymm14	\n\t"\
				"vmovaps %%ymm3,0x20(%%r13)						\n\t			vmulpd	0xc0(%%rbx),%%ymm10,%%ymm10	\n\t"\
				"vmulpd	%%ymm2,%%ymm4,%%ymm4					\n\t			vaddpd	%%ymm12,%%ymm14,%%ymm14			\n\t"\
				"vmulpd	%%ymm2,%%ymm1,%%ymm1					\n\t			vaddpd	%%ymm8 ,%%ymm10,%%ymm10			\n\t"\
				"vmulpd	%%ymm2,%%ymm3,%%ymm3					\n\t			vmovaps	%%ymm9 ,0x20(%%r11)	\n\t"\
				"vaddpd	0x20(%%r13),%%ymm4,%%ymm4				\n\t			vmovaps	%%ymm13,%%ymm15			\n\t"\
				"vsubpd	0x20(%%r15),%%ymm1,%%ymm1				\n\t			vmulpd 0x1a0(%%rbx),%%ymm13,%%ymm13	\n\t"\
				"vaddpd	0x20(%%r14),%%ymm3,%%ymm3				\n\t			vmulpd 0x200(%%rbx),%%ymm9 ,%%ymm9 		\n\t"\
				"vmulpd	%%ymm0,%%ymm4,%%ymm4					\n\t			vaddpd	0x20(%%r11),%%ymm13,%%ymm13	\n\t"\
				"vmulpd	%%ymm0,%%ymm1,%%ymm1					\n\t			vaddpd	%%ymm15,%%ymm9 ,%%ymm9 			\n\t"\
				"vmulpd	%%ymm0,%%ymm3,%%ymm3					\n\t			vmulpd	0xe0(%%rbx),%%ymm13,%%ymm13	\n\t"\
				"vmovaps 0x40(%%rbx),%%ymm2						\n\t			vmulpd	0xe0(%%rbx),%%ymm9 ,%%ymm9 		\n\t"\
				"vaddpd	0x20(%%r14),%%ymm4,%%ymm4				\n\t			vaddpd	%%ymm12,%%ymm13,%%ymm13			\n\t"\
				"vsubpd	0x20(%%r13),%%ymm1,%%ymm1				\n\t			vaddpd	%%ymm8 ,%%ymm9 ,%%ymm9 			\n\t"\
				"vsubpd	0x20(%%r15),%%ymm3,%%ymm3				\n\t			vmovaps	0x20(%%r10),%%ymm12	\n\t"\
				"vmulpd	%%ymm2,%%ymm4,%%ymm4					\n\t			vmovaps	%%ymm11,%%ymm15			\n\t"\
				"vmulpd	%%ymm2,%%ymm1,%%ymm1					\n\t			vmovaps	%%ymm12,%%ymm8 			\n\t"\
				"vmulpd	%%ymm2,%%ymm3,%%ymm3					\n\t			vaddpd	%%ymm14,%%ymm15,%%ymm15			\n\t"\
				"vmovaps -0x20(%%rbx),%%ymm0					\n\t			vaddpd	%%ymm13,%%ymm8 ,%%ymm8 			\n\t"\
				"vsubpd	%%ymm4,%%ymm6,%%ymm6					\n\t			vsubpd	%%ymm11,%%ymm14,%%ymm14			\n\t"\
				"vmulpd	%%ymm0,%%ymm4,%%ymm4					\n\t			vsubpd	%%ymm12,%%ymm13,%%ymm13			\n\t"\
				"vsubpd	%%ymm1,%%ymm7,%%ymm7					\n\t			vaddpd	%%ymm10,%%ymm14,%%ymm14			\n\t"\
				"vmulpd	%%ymm0,%%ymm1,%%ymm1					\n\t			vaddpd	%%ymm9 ,%%ymm13,%%ymm13			\n\t"\
				"vsubpd	%%ymm3,%%ymm5,%%ymm5					\n\t			vaddpd	%%ymm11,%%ymm10,%%ymm10			\n\t"\
				"vmulpd	%%ymm0,%%ymm3,%%ymm3					\n\t			vaddpd	%%ymm9 ,%%ymm12,%%ymm12			\n\t"\
				"vaddpd	%%ymm6,%%ymm4,%%ymm4					\n\t"\
				"vaddpd	%%ymm7,%%ymm1,%%ymm1					\n\t"\
				"vaddpd	%%ymm5,%%ymm3,%%ymm3					\n\t"\
			/* yi-data in ymm8,10,12,13,14,15; ymm0,2,9,11 free: */\
			"movq	%[__o7],%%r11	\n\t			movq	%[__oA],%%r13	\n\t"\
			"movq	%[__o6],%%r10	\n\t			movq	%[__o3],%%r12	\n\t"\
				"vmovaps	%%ymm4 ,%%ymm0						\n\t			vmovaps	%%ymm7 ,%%ymm9		\n\t"\
				"vsubpd	%%ymm15,%%ymm4,%%ymm4					\n\t			vsubpd	%%ymm8 ,%%ymm7,%%ymm7		\n\t"\
				"vaddpd	%%ymm15,%%ymm0,%%ymm0					\n\t			vaddpd	%%ymm8 ,%%ymm9,%%ymm9		\n\t"\
				"vmovaps	%%ymm4 ,0x20(%%r11)					\n\t			vmovaps	%%ymm7 ,0x20(%%r13)	\n\t"\
				"vmovaps	%%ymm0 ,0x20(%%r10)					\n\t			vmovaps	%%ymm9 ,0x20(%%r12)	\n\t"\
			"movq	%[__o5],%%r11	\n\t			movq	%[__o9],%%r13	\n\t"\
			"movq	%[__o8],%%r10	\n\t			movq	%[__o4],%%r12	\n\t"\
				"vmovaps	%%ymm5 ,%%ymm0						\n\t			vmovaps	%%ymm6 ,%%ymm9		\n\t"\
				"vsubpd	%%ymm14,%%ymm5,%%ymm5					\n\t			vsubpd	%%ymm13,%%ymm6,%%ymm6		\n\t"\
				"vaddpd	%%ymm14,%%ymm0,%%ymm0					\n\t			vaddpd	%%ymm13,%%ymm9,%%ymm9		\n\t"\
				"vmovaps	%%ymm5 ,0x20(%%r11)					\n\t			vmovaps	%%ymm6 ,0x20(%%r13)	\n\t"\
				"vmovaps	%%ymm0 ,0x20(%%r10)					\n\t			vmovaps	%%ymm9 ,0x20(%%r12)	\n\t"\
			"movq	%[__oB],%%r11	\n\t			movq	%[__oC],%%r13	\n\t"\
			"movq	%[__o2],%%r10	\n\t			movq	%[__o1],%%r12	\n\t"\
				"vmovaps	%%ymm1 ,%%ymm0						\n\t			vmovaps	%%ymm3 ,%%ymm9		\n\t"\
				"vsubpd	%%ymm10,%%ymm1,%%ymm1					\n\t			vsubpd	%%ymm12,%%ymm3,%%ymm3		\n\t"\
				"vaddpd	%%ymm10,%%ymm0,%%ymm0					\n\t			vaddpd	%%ymm12,%%ymm9,%%ymm9		\n\t"\
				"vmovaps	%%ymm1 ,0x20(%%r11)					\n\t			vmovaps	%%ymm3 ,0x20(%%r13)	\n\t"\
				"vmovaps	%%ymm0 ,0x20(%%r10)					\n\t			vmovaps	%%ymm9 ,0x20(%%r12)	\n\t"\
				:					/* outputs: none */\
				: [__cc] "m" (Xcc)	/* All inputs from memory addresses here */\
				 ,[__I0] "m" (XI0)\
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
				 ,[__iB] "m" (XiB)\
				 ,[__iC] "m" (XiC)\
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
				 ,[__oB] "m" (XoB)\
				 ,[__oC] "m" (XoC)\
				: "cc","memory","rax","rbx","rcx","r10","r11","r12","r13","r14","r15","xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7","xmm8","xmm9","xmm10","xmm11","xmm12","xmm13","xmm14","xmm15"		/* Clobbered registers */\
				);\
			}

		#else	// 64-bit SSE2:

		  #define USE_64BIT_ASM_STYLE	1	// My x86 timings indicate xmm0-15-using version of radix-13 DFT is about the same speed as 8-register, but keep both along with simple toggle

		  #if !USE_64BIT_ASM_STYLE

			// Simple 64-bit-ified version of the above 32-bit ASM macro, using just xmm0-7
			#define SSE2_RADIX_13_DFT(Xcc, XI0,Xi1,Xi2,Xi3,Xi4,Xi5,Xi6,Xi7,Xi8,Xi9,XiA,XiB,XiC, XO0,Xo1,Xo2,Xo3,Xo4,Xo5,Xo6,Xo7,Xo8,Xo9,XoA,XoB,XoC)\
			{\
			__asm__ volatile (\
				"movq	%[__I0],%%rax		\n\t"\
				"movq	%[__cc],%%rbx		\n\t"\
				"movq	%[__O0],%%rcx		\n\t"\
			"/* xr-terms need 8 registers for each side: */\n\t"\
			"movq	%[__i6],%%r10	\n\t	movaps (%%r10),%%xmm7	\n\t"\
			"movq	%[__i5],%%r11	\n\t	movaps (%%r11),%%xmm5	\n\t"\
			"movq	%[__i4],%%r12	\n\t	movaps (%%r12),%%xmm4	\n\t"\
			"movq	%[__i3],%%r13	\n\t	movaps (%%r13),%%xmm6	\n\t"\
			"movq	%[__i2],%%r14	\n\t	movaps (%%r14),%%xmm3	\n\t"\
			"movq	%[__i1],%%r15	\n\t	movaps (%%r15),%%xmm1	\n\t"\
				"movaps -0x10(%%rbx),%%xmm0	\n\t"\
			"movq	%[__i7],%%r10	\n\t	addpd (%%r10),%%xmm7	\n\t"\
			"movq	%[__i8],%%r11	\n\t	addpd (%%r11),%%xmm5	\n\t"\
			"movq	%[__i9],%%r12	\n\t	addpd (%%r12),%%xmm4	\n\t"\
			"movq	%[__iA],%%r13	\n\t	addpd (%%r13),%%xmm6	\n\t"\
			"movq	%[__iB],%%r14	\n\t	addpd (%%r14),%%xmm3	\n\t"\
			"movq	%[__iC],%%r15	\n\t	addpd (%%r15),%%xmm1	\n\t"\
				"subpd	%%xmm5,%%xmm1		\n\t"\
				"mulpd	%%xmm0,%%xmm5		\n\t"\
				"subpd	%%xmm6,%%xmm3		\n\t"\
				"mulpd	%%xmm0,%%xmm6		\n\t"\
				"subpd	%%xmm7,%%xmm4		\n\t"\
				"mulpd	%%xmm0,%%xmm7		\n\t"\
				"addpd	%%xmm1,%%xmm5		\n\t"\
				"addpd	%%xmm3,%%xmm6		\n\t"\
				"addpd	%%xmm4,%%xmm7		\n\t"\
				"movaps %%xmm5,%%xmm2		\n\t"\
				"addpd	%%xmm6,%%xmm2		\n\t"\
				"addpd	%%xmm7,%%xmm2		\n\t"\
				"movaps (%%rax),%%xmm0		\n\t"\
				"addpd	%%xmm2,%%xmm0		\n\t"\
				"mulpd	(%%rbx),%%xmm2		\n\t"\
				"movaps %%xmm0,(%%rcx)	\n\t"\
				"addpd	%%xmm2,%%xmm0		\n\t"\
				"movaps 0x010(%%rbx),%%xmm2	\n\t"\
				"mulpd	%%xmm2,%%xmm6		\n\t"\
				"mulpd	%%xmm2,%%xmm5		\n\t"\
				"mulpd	%%xmm2,%%xmm7		\n\t"\
		/* This section uses o-offsets 1,2,3,4,6,8 - prestore those into r10-15: */\
			"movq	%[__o1],%%r10	\n\t"\
			"movq	%[__o2],%%r11	\n\t"\
			"movq	%[__o3],%%r12	\n\t"\
			"movq	%[__o4],%%r13	\n\t"\
			"movq	%[__o6],%%r14	\n\t"\
			"movq	%[__o8],%%r15	\n\t"\
				"movaps %%xmm6,(%%r10)	\n\t"\
				"movaps %%xmm5,(%%r11)	\n\t"\
				"movaps %%xmm7,(%%r12)	\n\t"\
				"movaps 0x080(%%rbx),%%xmm2	\n\t"\
				"mulpd	%%xmm2,%%xmm5		\n\t"\
				"mulpd	%%xmm2,%%xmm7		\n\t"\
				"mulpd	%%xmm2,%%xmm6		\n\t"\
				"addpd	%%xmm0,%%xmm5		\n\t"\
				"addpd	%%xmm0,%%xmm7		\n\t"\
				"addpd	%%xmm0,%%xmm6		\n\t"\
				"addpd	(%%r10),%%xmm5	\n\t"\
				"addpd	(%%r11),%%xmm7	\n\t"\
				"addpd	(%%r12),%%xmm6	\n\t"\
				"movaps 0x0a0(%%rbx),%%xmm2	\n\t"\
				"movaps 0x090(%%rbx),%%xmm0	\n\t"\
				"movaps %%xmm4,(%%r10)	\n\t"\
				"movaps %%xmm1,(%%r11)	\n\t"\
				"movaps %%xmm3,(%%r12)	\n\t"\
				"mulpd	%%xmm2,%%xmm4		\n\t"\
				"mulpd	%%xmm2,%%xmm1		\n\t"\
				"mulpd	%%xmm2,%%xmm3		\n\t"\
				"addpd	(%%r12),%%xmm4	\n\t"\
				"subpd	(%%r10),%%xmm1	\n\t"\
				"addpd	(%%r11),%%xmm3	\n\t"\
				"mulpd	%%xmm0,%%xmm4		\n\t"\
				"mulpd	%%xmm0,%%xmm1		\n\t"\
				"mulpd	%%xmm0,%%xmm3		\n\t"\
				"movaps 0x020(%%rbx),%%xmm2	\n\t"\
				"addpd	(%%r11),%%xmm4	\n\t"\
				"subpd	(%%r12),%%xmm1	\n\t"\
				"subpd	(%%r10),%%xmm3	\n\t"\
				"mulpd	%%xmm2,%%xmm4		\n\t"\
				"mulpd	%%xmm2,%%xmm1		\n\t"\
				"mulpd	%%xmm2,%%xmm3		\n\t"\
				"movaps -0x010(%%rbx),%%xmm0	\n\t"\
				"subpd	%%xmm4,%%xmm6		\n\t"\
				"mulpd	%%xmm0,%%xmm4		\n\t"\
				"subpd	%%xmm1,%%xmm7		\n\t"\
				"mulpd	%%xmm0,%%xmm1		\n\t"\
				"subpd	%%xmm3,%%xmm5		\n\t"\
				"mulpd	%%xmm0,%%xmm3		\n\t"\
				"movaps %%xmm7,(%%r12)	\n\t"\
				"movaps %%xmm5,(%%r15)	\n\t"\
				"addpd	%%xmm6,%%xmm4		\n\t"\
				"addpd	%%xmm1,%%xmm7		\n\t"\
				"addpd	%%xmm3,%%xmm5		\n\t"\
				"movaps %%xmm5,(%%r10)	\n\t"\
				"movaps %%xmm7,(%%r11)	\n\t"\
				"movaps %%xmm6,(%%r13)	\n\t"\
				"movaps %%xmm4,(%%r14)	\n\t"\
			"/* yi-terms: */			\n\t"\
				"addq	$0x10,%%rax		\n\t"\
			"movq	%[__i1],%%r10	\n\t	movaps 0x10(%%r10),%%xmm1	\n\t"\
			"movq	%[__i2],%%r11	\n\t	movaps 0x10(%%r11),%%xmm2	\n\t"\
			"movq	%[__i3],%%r12	\n\t	movaps 0x10(%%r12),%%xmm5	\n\t"\
			"movq	%[__i4],%%r13	\n\t	movaps 0x10(%%r13),%%xmm3	\n\t"\
			"movq	%[__i5],%%r14	\n\t	movaps 0x10(%%r14),%%xmm4	\n\t"\
			"movq	%[__i6],%%r15	\n\t	movaps 0x10(%%r15),%%xmm6	\n\t"\
			"movq	%[__iC],%%r10	\n\t	subpd 0x10(%%r10),%%xmm1	\n\t"\
			"movq	%[__iB],%%r11	\n\t	subpd 0x10(%%r11),%%xmm2	\n\t"\
			"movq	%[__iA],%%r12	\n\t	subpd 0x10(%%r12),%%xmm5	\n\t"\
			"movq	%[__i9],%%r13	\n\t	subpd 0x10(%%r13),%%xmm3	\n\t"\
			"movq	%[__i8],%%r14	\n\t	subpd 0x10(%%r14),%%xmm4	\n\t"\
			"movq	%[__i7],%%r15	\n\t	subpd 0x10(%%r15),%%xmm6	\n\t"\
				"movaps %%xmm1,%%xmm7		\n\t"\
				"movaps %%xmm2,%%xmm0		\n\t"\
				"subpd	%%xmm3,%%xmm7		\n\t"\
				"addpd	%%xmm4,%%xmm0		\n\t"\
				"addpd	%%xmm5,%%xmm7		\n\t"\
				"addpd	%%xmm6,%%xmm0		\n\t"\
				"addpd	%%xmm3,%%xmm1		\n\t"\
				"addpd	%%xmm3,%%xmm5		\n\t"\
				"subpd	%%xmm4,%%xmm2		\n\t"\
				"subpd	%%xmm4,%%xmm6		\n\t"\
				"movaps %%xmm0,%%xmm4		\n\t"\
				"movaps %%xmm7,%%xmm3		\n\t"\
				"mulpd	0x040(%%rbx),%%xmm0	\n\t"\
				"mulpd	0x040(%%rbx),%%xmm7	\n\t"\
				"mulpd	0x030(%%rbx),%%xmm4	\n\t"\
				"mulpd	0x030(%%rbx),%%xmm3	\n\t"\
				"subpd	%%xmm7,%%xmm4		\n\t"\
				"addpd	%%xmm0,%%xmm3		\n\t"\
		/* This section uses o-offsets __o1-C with resp. frequencies [2,2,2,2,1,2,1,2,1,1,7,3] - prestore __1-2,B,C into r10,11,14,15, use r12,13 as floaters: */\
			"movq	%[__o1],%%r10	\n\t"\
			"movq	%[__o2],%%r11	\n\t"\
			"movq	%[__oB],%%r14	\n\t"\
			"movq	%[__oC],%%r15	\n\t"\
				"movaps %%xmm4,(%%r15)	\n\t"\
				"movaps %%xmm1,%%xmm0		\n\t"\
				"movaps %%xmm5,%%xmm4		\n\t"\
				"addpd	%%xmm2,%%xmm0		\n\t"\
				"addpd	%%xmm6,%%xmm4		\n\t"\
				"movaps %%xmm0,(%%r14)	\n\t"\
				"movaps %%xmm4,%%xmm7		\n\t"\
				"mulpd	0x0b0(%%rbx),%%xmm4	\n\t"\
				"mulpd	0x0e0(%%rbx),%%xmm0	\n\t"\
				"addpd	(%%r14),%%xmm4	\n\t"\
				"addpd	%%xmm7,%%xmm0		\n\t"\
				"mulpd	0x050(%%rbx),%%xmm4	\n\t"\
				"mulpd	0x050(%%rbx),%%xmm0	\n\t"\
				"movaps %%xmm2,(%%r14)	\n\t"\
				"movaps %%xmm6,%%xmm7		\n\t"\
				"mulpd	0x0c0(%%rbx),%%xmm6	\n\t"\
				"mulpd	0x0f0(%%rbx),%%xmm2	\n\t"\
				"addpd	(%%r14),%%xmm6	\n\t"\
				"addpd	%%xmm7,%%xmm2		\n\t"\
				"mulpd	0x060(%%rbx),%%xmm6	\n\t"\
				"mulpd	0x060(%%rbx),%%xmm2	\n\t"\
				"addpd	%%xmm4,%%xmm6		\n\t"\
				"addpd	%%xmm0,%%xmm2		\n\t"\
				"movaps %%xmm1,(%%r14)	\n\t"\
				"movaps %%xmm5,%%xmm7		\n\t"\
				"mulpd	0x0d0(%%rbx),%%xmm5	\n\t"\
				"mulpd	0x100(%%rbx),%%xmm1	\n\t"\
				"addpd	(%%r14),%%xmm5	\n\t"\
				"addpd	%%xmm7,%%xmm1		\n\t"\
				"mulpd	0x070(%%rbx),%%xmm5	\n\t"\
				"mulpd	0x070(%%rbx),%%xmm1	\n\t"\
				"addpd	%%xmm4,%%xmm5		\n\t"\
				"addpd	%%xmm0,%%xmm1		\n\t"\
				"movaps (%%r15),%%xmm4	\n\t"\
				"movaps %%xmm3,%%xmm7		\n\t"\
				"movaps %%xmm4,%%xmm0		\n\t"\
				"addpd	%%xmm6,%%xmm7		\n\t"\
				"addpd	%%xmm5,%%xmm0		\n\t"\
				"subpd	%%xmm3,%%xmm6		\n\t"\
				"subpd	%%xmm4,%%xmm5		\n\t"\
				"addpd	%%xmm2,%%xmm6		\n\t"\
				"addpd	%%xmm1,%%xmm5		\n\t"\
				"addpd	%%xmm3,%%xmm2		\n\t"\
				"addpd	%%xmm1,%%xmm4		\n\t"\
			"movq	%[__o6],%%r12	\n\t"\
			"movq	%[__o7],%%r13	\n\t"\
				"movaps (%%r12),%%xmm3	\n\t"\
				"movaps %%xmm3,%%xmm1		\n\t"\
				"subpd	%%xmm7,%%xmm3		\n\t"\
				"addpd	%%xmm7,%%xmm1		\n\t"\
				"movaps %%xmm3,(%%r12)	\n\t"\
				"movaps %%xmm1,(%%r13)	\n\t"\
			"movq	%[__o8],%%r12	\n\t"\
			"movq	%[__o5],%%r13	\n\t"\
				"movaps (%%r12),%%xmm3	\n\t"\
				"movaps %%xmm3,%%xmm1		\n\t"\
				"subpd	%%xmm6,%%xmm3		\n\t"\
				"addpd	%%xmm6,%%xmm1		\n\t"\
				"movaps %%xmm3,(%%r12)	\n\t"\
				"movaps %%xmm1,(%%r13)	\n\t"\
				"movaps (%%r11),%%xmm3	\n\t"\
				"movaps %%xmm3,%%xmm1		\n\t"\
				"subpd	%%xmm2,%%xmm3		\n\t"\
				"addpd	%%xmm2,%%xmm1		\n\t"\
				"movaps %%xmm3,(%%r11)	\n\t"\
				"movaps %%xmm1,(%%r14)	\n\t"\
			"movq	%[__o3],%%r12	\n\t"\
			"movq	%[__oA],%%r13	\n\t"\
				"movaps (%%r12),%%xmm3	\n\t"\
				"movaps %%xmm3,%%xmm1		\n\t"\
				"subpd	%%xmm0,%%xmm3		\n\t"\
				"addpd	%%xmm0,%%xmm1		\n\t"\
				"movaps %%xmm3,(%%r12)	\n\t"\
				"movaps %%xmm1,(%%r13)	\n\t"\
			"movq	%[__o4],%%r12	\n\t"\
			"movq	%[__o9],%%r13	\n\t"\
				"movaps (%%r12),%%xmm3	\n\t"\
				"movaps %%xmm3,%%xmm1		\n\t"\
				"subpd	%%xmm5,%%xmm3		\n\t"\
				"addpd	%%xmm5,%%xmm1		\n\t"\
				"movaps %%xmm3,(%%r12)	\n\t"\
				"movaps %%xmm1,(%%r13)	\n\t"\
				"movaps (%%r10),%%xmm3	\n\t"\
				"movaps %%xmm3,%%xmm1		\n\t"\
				"subpd	%%xmm4,%%xmm3		\n\t"\
				"addpd	%%xmm4,%%xmm1		\n\t"\
				"movaps %%xmm3,(%%r10)	\n\t"\
				"movaps %%xmm1,(%%r15)	\n\t"\
				"/***************/			\n\t"\
				"/* IMAG PARTS: */			\n\t"\
				"/***************/			\n\t"\
			"/* xi-terms need 8 registers for each	side: */\n\t"\
				"addq	$0x10,%%rcx		\n\t"\
				"movq	%[__i6],%%r10	\n\t	movaps 0x10(%%r10),%%xmm7	\n\t"\
				"movq	%[__i5],%%r11	\n\t	movaps 0x10(%%r11),%%xmm5	\n\t"\
				"movq	%[__i4],%%r12	\n\t	movaps 0x10(%%r12),%%xmm4	\n\t"\
				"movq	%[__i3],%%r13	\n\t	movaps 0x10(%%r13),%%xmm6	\n\t"\
				"movq	%[__i2],%%r14	\n\t	movaps 0x10(%%r14),%%xmm3	\n\t"\
				"movq	%[__i1],%%r15	\n\t	movaps 0x10(%%r15),%%xmm1	\n\t"\
				"movaps -0x010(%%rbx),%%xmm0	\n\t"\
				"movq	%[__i7],%%r10	\n\t	addpd 0x10(%%r10),%%xmm7	\n\t"\
				"movq	%[__i8],%%r11	\n\t	addpd 0x10(%%r11),%%xmm5	\n\t"\
				"movq	%[__i9],%%r12	\n\t	addpd 0x10(%%r12),%%xmm4	\n\t"\
				"movq	%[__iA],%%r13	\n\t	addpd 0x10(%%r13),%%xmm6	\n\t"\
				"movq	%[__iB],%%r14	\n\t	addpd 0x10(%%r14),%%xmm3	\n\t"\
				"movq	%[__iC],%%r15	\n\t	addpd 0x10(%%r15),%%xmm1	\n\t"\
				"subpd	%%xmm5,%%xmm1		\n\t"\
				"mulpd	%%xmm0,%%xmm5		\n\t"\
				"subpd	%%xmm6,%%xmm3		\n\t"\
				"mulpd	%%xmm0,%%xmm6		\n\t"\
				"subpd	%%xmm7,%%xmm4		\n\t"\
				"mulpd	%%xmm0,%%xmm7		\n\t"\
				"addpd	%%xmm1,%%xmm5		\n\t"\
				"addpd	%%xmm3,%%xmm6		\n\t"\
				"addpd	%%xmm4,%%xmm7		\n\t"\
				"movaps %%xmm5,%%xmm2		\n\t"\
				"addpd	%%xmm6,%%xmm2		\n\t"\
				"addpd	%%xmm7,%%xmm2		\n\t"\
				"movaps (%%rax),%%xmm0		\n\t"\
				"addpd	%%xmm2,%%xmm0		\n\t"\
				"mulpd	(%%rbx),%%xmm2		\n\t"\
				"movaps %%xmm0,(%%rcx)	\n\t"\
				"addpd	%%xmm2,%%xmm0		\n\t"\
				"movaps 0x010(%%rbx),%%xmm2	\n\t"\
				"mulpd	%%xmm2,%%xmm6		\n\t"\
				"mulpd	%%xmm2,%%xmm5		\n\t"\
				"mulpd	%%xmm2,%%xmm7		\n\t"\
		/* This section uses o-offsets 5,7,9,A,B,C - prestore those into r10-15: */\
			"movq	%[__o5],%%r10	\n\t"\
			"movq	%[__o7],%%r11	\n\t"\
			"movq	%[__o9],%%r12	\n\t"\
			"movq	%[__oA],%%r13	\n\t"\
			"movq	%[__oB],%%r14	\n\t"\
			"movq	%[__oC],%%r15	\n\t"\
				"movaps %%xmm6,0x10(%%r15)	\n\t"\
				"movaps %%xmm5,0x10(%%r14)	\n\t"\
				"movaps %%xmm7,0x10(%%r13)	\n\t"\
				"movaps 0x080(%%rbx),%%xmm2	\n\t"\
				"mulpd	%%xmm2,%%xmm5		\n\t"\
				"mulpd	%%xmm2,%%xmm7		\n\t"\
				"mulpd	%%xmm2,%%xmm6		\n\t"\
				"addpd	%%xmm0,%%xmm5		\n\t"\
				"addpd	%%xmm0,%%xmm7		\n\t"\
				"addpd	%%xmm0,%%xmm6		\n\t"\
				"addpd	0x10(%%r15),%%xmm5	\n\t"\
				"addpd	0x10(%%r14),%%xmm7	\n\t"\
				"addpd	0x10(%%r13),%%xmm6	\n\t"\
				"movaps 0x0a0(%%rbx),%%xmm2	\n\t"\
				"movaps 0x090(%%rbx),%%xmm0	\n\t"\
				"movaps %%xmm4,0x10(%%r15)	\n\t"\
				"movaps %%xmm1,0x10(%%r14)	\n\t"\
				"movaps %%xmm3,0x10(%%r13)	\n\t"\
				"mulpd	%%xmm2,%%xmm4		\n\t"\
				"mulpd	%%xmm2,%%xmm1		\n\t"\
				"mulpd	%%xmm2,%%xmm3		\n\t"\
				"addpd	0x10(%%r13),%%xmm4	\n\t"\
				"subpd	0x10(%%r15),%%xmm1	\n\t"\
				"addpd	0x10(%%r14),%%xmm3	\n\t"\
				"mulpd	%%xmm0,%%xmm4		\n\t"\
				"mulpd	%%xmm0,%%xmm1		\n\t"\
				"mulpd	%%xmm0,%%xmm3		\n\t"\
				"movaps 0x020(%%rbx),%%xmm2	\n\t"\
				"addpd	0x10(%%r14),%%xmm4	\n\t"\
				"subpd	0x10(%%r13),%%xmm1	\n\t"\
				"subpd	0x10(%%r15),%%xmm3	\n\t"\
				"mulpd	%%xmm2,%%xmm4		\n\t"\
				"mulpd	%%xmm2,%%xmm1		\n\t"\
				"mulpd	%%xmm2,%%xmm3		\n\t"\
				"movaps -0x010(%%rbx),%%xmm0	\n\t"\
				"subpd	%%xmm4,%%xmm6		\n\t"\
				"mulpd	%%xmm0,%%xmm4		\n\t"\
				"subpd	%%xmm1,%%xmm7		\n\t"\
				"mulpd	%%xmm0,%%xmm1		\n\t"\
				"subpd	%%xmm3,%%xmm5		\n\t"\
				"mulpd	%%xmm0,%%xmm3		\n\t"\
				"movaps %%xmm7,0x10(%%r13)	\n\t"\
				"movaps %%xmm5,0x10(%%r10)	\n\t"\
				"addpd	%%xmm6,%%xmm4		\n\t"\
				"addpd	%%xmm1,%%xmm7		\n\t"\
				"addpd	%%xmm3,%%xmm5		\n\t"\
				"movaps %%xmm5,0x10(%%r15)	\n\t"\
				"movaps %%xmm7,0x10(%%r14)	\n\t"\
				"movaps %%xmm6,0x10(%%r12)	\n\t"\
				"movaps %%xmm4,0x10(%%r11)	\n\t"\
			"/* yr-terms: */			\n\t"\
				"subq	$0x10,%%rax		\n\t"\
				"movq	%[__i1],%%r10	\n\t	movaps (%%r10),%%xmm1	\n\t"\
				"movq	%[__i2],%%r11	\n\t	movaps (%%r11),%%xmm2	\n\t"\
				"movq	%[__i3],%%r12	\n\t	movaps (%%r12),%%xmm5	\n\t"\
				"movq	%[__i4],%%r13	\n\t	movaps (%%r13),%%xmm3	\n\t"\
				"movq	%[__i5],%%r14	\n\t	movaps (%%r14),%%xmm4	\n\t"\
				"movq	%[__i6],%%r15	\n\t	movaps (%%r15),%%xmm6	\n\t"\
				"movq	%[__iC],%%r10	\n\t	subpd (%%r10),%%xmm1	\n\t"\
				"movq	%[__iB],%%r11	\n\t	subpd (%%r11),%%xmm2	\n\t"\
				"movq	%[__iA],%%r12	\n\t	subpd (%%r12),%%xmm5	\n\t"\
				"movq	%[__i9],%%r13	\n\t	subpd (%%r13),%%xmm3	\n\t"\
				"movq	%[__i8],%%r14	\n\t	subpd (%%r14),%%xmm4	\n\t"\
				"movq	%[__i7],%%r15	\n\t	subpd (%%r15),%%xmm6	\n\t"\
				"movaps %%xmm1,%%xmm7		\n\t"\
				"movaps %%xmm2,%%xmm0		\n\t"\
				"subpd	%%xmm3,%%xmm7		\n\t"\
				"addpd	%%xmm4,%%xmm0		\n\t"\
				"addpd	%%xmm5,%%xmm7		\n\t"\
				"addpd	%%xmm6,%%xmm0		\n\t"\
				"addpd	%%xmm3,%%xmm1		\n\t"\
				"addpd	%%xmm3,%%xmm5		\n\t"\
				"subpd	%%xmm4,%%xmm2		\n\t"\
				"subpd	%%xmm4,%%xmm6		\n\t"\
				"movaps %%xmm0,%%xmm4		\n\t"\
				"movaps %%xmm7,%%xmm3		\n\t"\
				"mulpd	0x040(%%rbx),%%xmm0	\n\t"\
				"mulpd	0x040(%%rbx),%%xmm7	\n\t"\
				"mulpd	0x030(%%rbx),%%xmm4	\n\t"\
				"mulpd	0x030(%%rbx),%%xmm3	\n\t"\
				"subpd	%%xmm7,%%xmm4		\n\t"\
				"addpd	%%xmm0,%%xmm3		\n\t"\
		/* This section uses o-offsets __o1-C with resp. frequencies [3,7,1,1,2,1,2,1,2,2,2,2] - prestore __1-2,C into r10,11,15, use r12-14 as floaters: */\
			"movq	%[__o1],%%r10	\n\t"\
			"movq	%[__o2],%%r11	\n\t"\
			"movq	%[__oC],%%r15	\n\t"\
				"movaps %%xmm4,0x10(%%r10)	\n\t"\
				"movaps %%xmm1,%%xmm0		\n\t"\
				"movaps %%xmm5,%%xmm4		\n\t"\
				"addpd	%%xmm2,%%xmm0		\n\t"\
				"addpd	%%xmm6,%%xmm4		\n\t"\
				"movaps %%xmm0,0x10(%%r11)	\n\t"\
				"movaps %%xmm4,%%xmm7		\n\t"\
				"mulpd	0x0b0(%%rbx),%%xmm4	\n\t"\
				"mulpd	0x0e0(%%rbx),%%xmm0	\n\t"\
				"addpd	0x10(%%r11),%%xmm4	\n\t"\
				"addpd	%%xmm7,%%xmm0		\n\t"\
				"mulpd	0x050(%%rbx),%%xmm4	\n\t"\
				"mulpd	0x050(%%rbx),%%xmm0	\n\t"\
				"movaps %%xmm2,0x10(%%r11)	\n\t"\
				"movaps %%xmm6,%%xmm7		\n\t"\
				"mulpd	0x0c0(%%rbx),%%xmm6	\n\t"\
				"mulpd	0x0f0(%%rbx),%%xmm2	\n\t"\
				"addpd	0x10(%%r11),%%xmm6	\n\t"\
				"addpd	%%xmm7,%%xmm2		\n\t"\
				"mulpd	0x060(%%rbx),%%xmm6	\n\t"\
				"mulpd	0x060(%%rbx),%%xmm2	\n\t"\
				"addpd	%%xmm4,%%xmm6		\n\t"\
				"addpd	%%xmm0,%%xmm2		\n\t"\
				"movaps %%xmm1,0x10(%%r11)	\n\t"\
				"movaps %%xmm5,%%xmm7		\n\t"\
				"mulpd	0x0d0(%%rbx),%%xmm5	\n\t"\
				"mulpd	0x100(%%rbx),%%xmm1	\n\t"\
				"addpd	0x10(%%r11),%%xmm5	\n\t"\
				"addpd	%%xmm7,%%xmm1		\n\t"\
				"mulpd	0x070(%%rbx),%%xmm5	\n\t"\
				"mulpd	0x070(%%rbx),%%xmm1	\n\t"\
				"addpd	%%xmm4,%%xmm5		\n\t"\
				"addpd	%%xmm0,%%xmm1		\n\t"\
				"movaps 0x10(%%r10),%%xmm4	\n\t"\
				"movaps %%xmm3,%%xmm7		\n\t"\
				"movaps %%xmm4,%%xmm0		\n\t"\
				"addpd	%%xmm6,%%xmm7		\n\t"\
				"addpd	%%xmm5,%%xmm0		\n\t"\
				"subpd	%%xmm3,%%xmm6		\n\t"\
				"subpd	%%xmm4,%%xmm5		\n\t"\
				"addpd	%%xmm2,%%xmm6		\n\t"\
				"addpd	%%xmm1,%%xmm5		\n\t"\
				"addpd	%%xmm3,%%xmm2		\n\t"\
				"addpd	%%xmm1,%%xmm4		\n\t"\
			"movq	%[__o7],%%r12	\n\t"\
			"movq	%[__o6],%%r13	\n\t"\
				"movaps 0x10(%%r12),%%xmm3	\n\t"\
				"movaps %%xmm3,%%xmm1		\n\t"\
				"subpd	%%xmm7,%%xmm3		\n\t"\
				"addpd	%%xmm7,%%xmm1		\n\t"\
				"movaps %%xmm3,0x10(%%r12)	\n\t"\
				"movaps %%xmm1,0x10(%%r13)	\n\t"\
			"movq	%[__o5],%%r12	\n\t"\
			"movq	%[__o8],%%r13	\n\t"\
				"movaps 0x10(%%r12),%%xmm3	\n\t"\
				"movaps %%xmm3,%%xmm1		\n\t"\
				"subpd	%%xmm6,%%xmm3		\n\t"\
				"addpd	%%xmm6,%%xmm1		\n\t"\
				"movaps %%xmm3,0x10(%%r12)	\n\t"\
				"movaps %%xmm1,0x10(%%r13)	\n\t"\
			"movq	%[__oB],%%r12	\n\t"\
				"movaps 0x10(%%r12),%%xmm3	\n\t"\
				"movaps %%xmm3,%%xmm1		\n\t"\
				"subpd	%%xmm2,%%xmm3		\n\t"\
				"addpd	%%xmm2,%%xmm1		\n\t"\
				"movaps %%xmm3,0x10(%%r12)	\n\t"\
				"movaps %%xmm1,0x10(%%r11)	\n\t"\
			"movq	%[__oA],%%r12	\n\t"\
			"movq	%[__o3],%%r13	\n\t"\
				"movaps 0x10(%%r12),%%xmm3	\n\t"\
				"movaps %%xmm3,%%xmm1		\n\t"\
				"subpd	%%xmm0,%%xmm3		\n\t"\
				"addpd	%%xmm0,%%xmm1		\n\t"\
				"movaps %%xmm3,0x10(%%r12)	\n\t"\
				"movaps %%xmm1,0x10(%%r13)	\n\t"\
			"movq	%[__o9],%%r12	\n\t"\
			"movq	%[__o4],%%r13	\n\t"\
				"movaps 0x10(%%r12),%%xmm3	\n\t"\
				"movaps %%xmm3,%%xmm1		\n\t"\
				"subpd	%%xmm5,%%xmm3		\n\t"\
				"addpd	%%xmm5,%%xmm1		\n\t"\
				"movaps %%xmm3,0x10(%%r12)	\n\t"\
				"movaps %%xmm1,0x10(%%r13)	\n\t"\
				"movaps 0x10(%%r15),%%xmm3	\n\t"\
				"movaps %%xmm3,%%xmm1		\n\t"\
				"subpd	%%xmm4,%%xmm3		\n\t"\
				"addpd	%%xmm4,%%xmm1		\n\t"\
				"movaps %%xmm3,0x10(%%r15)	\n\t"\
				"movaps %%xmm1,0x10(%%r10)	\n\t"\
				:					/* outputs: none */\
				: [__cc] "m" (Xcc)	/* All inputs from memory addresses here */\
				 ,[__I0] "m" (XI0)\
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
				 ,[__iB] "m" (XiB)\
				 ,[__iC] "m" (XiC)\
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
				 ,[__oB] "m" (XoB)\
				 ,[__oC] "m" (XoC)\
				: "cc","memory","rax","rbx","rcx","r10","r11","r12","r13","r14","r15","xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7"	 /* Clobbered registers */\
				);\
			}

		  #else // USE_64BIT_ASM_STYLE - 2-Instruction-stream-overlapped 64-bit-ified version of the above 32-bit ASM macro, using all of xmm0-15

			#define SSE2_RADIX_13_DFT(Xcc, XI0,Xi1,Xi2,Xi3,Xi4,Xi5,Xi6,Xi7,Xi8,Xi9,XiA,XiB,XiC, XO0,Xo1,Xo2,Xo3,Xo4,Xo5,Xo6,Xo7,Xo8,Xo9,XoA,XoB,XoC)\
			{\
			__asm__ volatile (\
				"movq	%[__I0],%%rax							\n\t"\
				"movq	%[__cc],%%rbx							\n\t"\
				"movq	%[__O0],%%rcx							\n\t"\
			/* xr-terms:                                                      yi-terms: */\
				"movq	%[__i1],%%r15	\n\t	movaps (%%r15),%%xmm1	\n\t	movaps	0x10(%%r15),%%xmm9	\n\t"\
				"movq	%[__i2],%%r14	\n\t	movaps (%%r14),%%xmm3	\n\t	movaps	0x10(%%r14),%%xmm10	\n\t"\
				"movq	%[__i3],%%r13	\n\t	movaps (%%r13),%%xmm6	\n\t	movaps	0x10(%%r13),%%xmm13	\n\t"\
				"movq	%[__i4],%%r12	\n\t	movaps (%%r12),%%xmm4	\n\t	movaps	0x10(%%r12),%%xmm11	\n\t"\
				"movq	%[__i5],%%r11	\n\t	movaps (%%r11),%%xmm5	\n\t	movaps	0x10(%%r11),%%xmm12	\n\t"\
				"movq	%[__i6],%%r10	\n\t	movaps (%%r10),%%xmm7	\n\t	movaps	0x10(%%r10),%%xmm14	\n\t"\
				"movaps	-0x010(%%rbx),%%xmm0					\n\t"\
				"movq	%[__iC],%%r15	\n\t	addpd (%%r15),%%xmm1	\n\t	subpd	0x10(%%r15),%%xmm9	\n\t"\
				"movq	%[__iB],%%r14	\n\t	addpd (%%r14),%%xmm3	\n\t	subpd	0x10(%%r14),%%xmm10	\n\t"\
				"movq	%[__iA],%%r13	\n\t	addpd (%%r13),%%xmm6	\n\t	subpd	0x10(%%r13),%%xmm13	\n\t"\
				"movq	%[__i9],%%r12	\n\t	addpd (%%r12),%%xmm4	\n\t	subpd	0x10(%%r12),%%xmm11	\n\t"\
				"movq	%[__i8],%%r11	\n\t	addpd (%%r11),%%xmm5	\n\t	subpd	0x10(%%r11),%%xmm12	\n\t"\
				"movq	%[__i7],%%r10	\n\t	addpd (%%r10),%%xmm7	\n\t	subpd	0x10(%%r10),%%xmm14	\n\t"\
				"subpd	%%xmm5,%%xmm1							\n\t			movaps	%%xmm9 ,%%xmm15			\n\t"\
				"mulpd	%%xmm0,%%xmm5							\n\t			movaps	%%xmm10,%%xmm8			\n\t"\
				"subpd	%%xmm6,%%xmm3							\n\t			subpd	%%xmm11,%%xmm15			\n\t"\
				"mulpd	%%xmm0,%%xmm6							\n\t			addpd	%%xmm12,%%xmm8			\n\t"\
				"subpd	%%xmm7,%%xmm4							\n\t			addpd	%%xmm13,%%xmm15			\n\t"\
				"mulpd	%%xmm0,%%xmm7							\n\t			addpd	%%xmm14,%%xmm8			\n\t"\
				"addpd	%%xmm1,%%xmm5							\n\t			addpd	%%xmm11,%%xmm9			\n\t"\
				"addpd	%%xmm3,%%xmm6							\n\t			addpd	%%xmm11,%%xmm13			\n\t"\
				"addpd	%%xmm4,%%xmm7							\n\t			subpd	%%xmm12,%%xmm10			\n\t"\
				"movaps	%%xmm5,%%xmm2							\n\t			subpd	%%xmm12,%%xmm14			\n\t"\
				"addpd	%%xmm6,%%xmm2							\n\t			movaps	%%xmm8 ,%%xmm12			\n\t"\
				"addpd	%%xmm7,%%xmm2							\n\t			movaps	%%xmm15,%%xmm11			\n\t"\
				"movaps	(%%rax),%%xmm0							\n\t			mulpd	0x040(%%rbx),%%xmm8		\n\t"\
				"addpd	%%xmm2,%%xmm0							\n\t			mulpd	0x040(%%rbx),%%xmm15	\n\t"\
				"mulpd	(%%rbx),%%xmm2							\n\t			mulpd	0x030(%%rbx),%%xmm12	\n\t"\
				"movaps	%%xmm0,(%%rcx)							\n\t			mulpd	0x030(%%rbx),%%xmm11	\n\t"\
				"addpd	%%xmm2,%%xmm0							\n\t			subpd	%%xmm15,%%xmm12			\n\t"\
				"movaps	0x010(%%rbx),%%xmm2						\n\t			addpd	%%xmm8 ,%%xmm11			\n\t"\
		/* lcol section uses o-offsets 1,2,3 - prestore those into r10-12: */\
			"movq	%[__o1],%%r10		\n\t"		/* rcol section uses o-offsets __oB,C - prestore those into r14,15: */\
			"movq	%[__o2],%%r11								\n\t		movq	%[__oB],%%r14	\n\t"\
			"movq	%[__o3],%%r12								\n\t		movq	%[__oC],%%r15	\n\t"\
				"mulpd	%%xmm2,%%xmm6							\n\t			movaps	%%xmm12,(%%r15)			\n\t"\
				"mulpd	%%xmm2,%%xmm5							\n\t			movaps	%%xmm9 ,%%xmm8			\n\t"\
				"mulpd	%%xmm2,%%xmm7							\n\t			movaps	%%xmm13,%%xmm12			\n\t"\
				"movaps %%xmm6,(%%r10)							\n\t			addpd	%%xmm10,%%xmm8			\n\t"\
				"movaps %%xmm5,(%%r11)							\n\t			addpd	%%xmm14,%%xmm12			\n\t"\
				"movaps %%xmm7,(%%r12)							\n\t			movaps	%%xmm8 ,(%%r14)			\n\t"\
				"movaps 0x080(%%rbx),%%xmm2						\n\t			movaps	%%xmm12,%%xmm15			\n\t"\
				"mulpd	%%xmm2,%%xmm5							\n\t			mulpd	0x0b0(%%rbx),%%xmm12	\n\t"\
				"mulpd	%%xmm2,%%xmm7							\n\t			mulpd	0x0e0(%%rbx),%%xmm8		\n\t"\
				"mulpd	%%xmm2,%%xmm6							\n\t			addpd	(%%r14),%%xmm12			\n\t"\
				"addpd	%%xmm0,%%xmm5							\n\t			addpd	%%xmm15,%%xmm8			\n\t"\
				"addpd	%%xmm0,%%xmm7							\n\t			mulpd	0x050(%%rbx),%%xmm12	\n\t"\
				"addpd	%%xmm0,%%xmm6							\n\t			mulpd	0x050(%%rbx),%%xmm8		\n\t"\
				"addpd	(%%r10),%%xmm5							\n\t			movaps	%%xmm10,(%%r14)			\n\t"\
				"addpd	(%%r11),%%xmm7							\n\t			movaps	%%xmm14,%%xmm15			\n\t"\
				"addpd	(%%r12),%%xmm6							\n\t			mulpd	0x0c0(%%rbx),%%xmm14	\n\t"\
				"movaps 0x0a0(%%rbx),%%xmm2						\n\t			mulpd	0x0f0(%%rbx),%%xmm10	\n\t"\
				"movaps 0x090(%%rbx),%%xmm0						\n\t			addpd	(%%r14),%%xmm14			\n\t"\
				"movaps %%xmm4,(%%r10)							\n\t			addpd	%%xmm15,%%xmm10			\n\t"\
				"movaps %%xmm1,(%%r11)							\n\t			mulpd	0x060(%%rbx),%%xmm14	\n\t"\
				"movaps %%xmm3,(%%r12)							\n\t			mulpd	0x060(%%rbx),%%xmm10	\n\t"\
				"mulpd	%%xmm2,%%xmm4							\n\t			addpd	%%xmm12,%%xmm14			\n\t"\
				"mulpd	%%xmm2,%%xmm1							\n\t			addpd	%%xmm8 ,%%xmm10			\n\t"\
				"mulpd	%%xmm2,%%xmm3							\n\t			movaps	%%xmm9 ,(%%r14)			\n\t"\
				"addpd	(%%r12),%%xmm4							\n\t			movaps	%%xmm13,%%xmm15			\n\t"\
				"subpd	(%%r10),%%xmm1							\n\t			mulpd	0x0d0(%%rbx),%%xmm13	\n\t"\
				"addpd	(%%r11),%%xmm3							\n\t			mulpd	0x100(%%rbx),%%xmm9		\n\t"\
				"mulpd	%%xmm0,%%xmm4							\n\t			addpd	(%%r14),%%xmm13			\n\t"\
				"mulpd	%%xmm0,%%xmm1							\n\t			addpd	%%xmm15,%%xmm9			\n\t"\
				"mulpd	%%xmm0,%%xmm3							\n\t			mulpd	0x070(%%rbx),%%xmm13	\n\t"\
				"movaps 0x020(%%rbx),%%xmm2						\n\t			mulpd	0x070(%%rbx),%%xmm9		\n\t"\
				"addpd	(%%r11),%%xmm4							\n\t			addpd	%%xmm12,%%xmm13			\n\t"\
				"subpd	(%%r12),%%xmm1							\n\t			addpd	%%xmm8 ,%%xmm9			\n\t"\
				"subpd	(%%r10),%%xmm3							\n\t			movaps	(%%r15),%%xmm12			\n\t"\
				"mulpd	%%xmm2,%%xmm4							\n\t			movaps	%%xmm11,%%xmm15			\n\t"\
				"mulpd	%%xmm2,%%xmm1							\n\t			movaps	%%xmm12,%%xmm8			\n\t"\
				"mulpd	%%xmm2,%%xmm3							\n\t			addpd	%%xmm14,%%xmm15			\n\t"\
				"movaps -0x010(%%rbx),%%xmm0					\n\t			addpd	%%xmm13,%%xmm8			\n\t"\
				"subpd	%%xmm4,%%xmm6							\n\t			subpd	%%xmm11,%%xmm14			\n\t"\
				"mulpd	%%xmm0,%%xmm4							\n\t			subpd	%%xmm12,%%xmm13			\n\t"\
				"subpd	%%xmm1,%%xmm7							\n\t			addpd	%%xmm10,%%xmm14			\n\t"\
				"mulpd	%%xmm0,%%xmm1							\n\t			addpd	%%xmm9 ,%%xmm13			\n\t"\
				"subpd	%%xmm3,%%xmm5							\n\t			addpd	%%xmm11,%%xmm10			\n\t"\
				"mulpd	%%xmm0,%%xmm3							\n\t			addpd	%%xmm9 ,%%xmm12			\n\t"\
				"addpd	%%xmm6,%%xmm4							\n\t"\
				"addpd	%%xmm7,%%xmm1							\n\t"\
				"addpd	%%xmm5,%%xmm3							\n\t"\
		  /* yi-data in xmm8,10,12,13,14,15; xmm0,2,9,11 free: */\
			"movq	%[__o6],%%r10	\n\t		movq	%[__o3],%%r12	\n\t"\
			"movq	%[__o7],%%r11	\n\t		movq	%[__oA],%%r13	\n\t"\
				"movaps	%%xmm4 ,%%xmm0					\n\t			movaps	%%xmm7 ,%%xmm9			\n\t"\
				"subpd	%%xmm15,%%xmm4					\n\t			subpd	%%xmm8 ,%%xmm7			\n\t"\
				"addpd	%%xmm15,%%xmm0					\n\t			addpd	%%xmm8 ,%%xmm9			\n\t"\
				"movaps	%%xmm4 ,(%%r10)					\n\t			movaps	%%xmm7 ,(%%r12)	\n\t"\
				"movaps	%%xmm0 ,(%%r11)					\n\t			movaps	%%xmm9 ,(%%r13)	\n\t"\
			"movq	%[__o8],%%r10	\n\t		movq	%[__o4],%%r12	\n\t"\
			"movq	%[__o5],%%r11	\n\t		movq	%[__o9],%%r13	\n\t"\
				"movaps	%%xmm5 ,%%xmm0					\n\t			movaps	%%xmm6 ,%%xmm9			\n\t"\
				"subpd	%%xmm14,%%xmm5					\n\t			subpd	%%xmm13,%%xmm6			\n\t"\
				"addpd	%%xmm14,%%xmm0					\n\t			addpd	%%xmm13,%%xmm9			\n\t"\
				"movaps	%%xmm5 ,(%%r10)					\n\t			movaps	%%xmm6 ,(%%r12)	\n\t"\
				"movaps	%%xmm0 ,(%%r11)					\n\t			movaps	%%xmm9 ,(%%r13)	\n\t"\
			"movq	%[__o2],%%r10	\n\t		movq	%[__o1],%%r12	\n\t"\
			"movq	%[__oB],%%r11	\n\t		movq	%[__oC],%%r13	\n\t"\
				"movaps	%%xmm1 ,%%xmm0					\n\t			movaps	%%xmm3 ,%%xmm9			\n\t"\
				"subpd	%%xmm10,%%xmm1					\n\t			subpd	%%xmm12,%%xmm3			\n\t"\
				"addpd	%%xmm10,%%xmm0					\n\t			addpd	%%xmm12,%%xmm9			\n\t"\
				"movaps	%%xmm1 ,(%%r10)					\n\t			movaps	%%xmm3 ,(%%r12)	\n\t"\
				"movaps	%%xmm0 ,(%%r11)					\n\t			movaps	%%xmm9 ,(%%r13)	\n\t"\
				/****************************************************************/\
				/*                      IMAG PARTS:                             */\
				/****************************************************************/\
				"addq	$0x10,%%rax		 						\n\t"\
				"addq	$0x10,%%rcx								\n\t"\
			/* xi-terms:                                                      yr-terms: */\
				"movq	%[__i1],%%r15	\n\t	movaps 0x10(%%r15),%%xmm1	\n\t	movaps	(%%r15),%%xmm9	\n\t"\
				"movq	%[__i2],%%r14	\n\t	movaps 0x10(%%r14),%%xmm3	\n\t	movaps	(%%r14),%%xmm10	\n\t"\
				"movq	%[__i3],%%r13	\n\t	movaps 0x10(%%r13),%%xmm6	\n\t	movaps	(%%r13),%%xmm13	\n\t"\
				"movq	%[__i4],%%r12	\n\t	movaps 0x10(%%r12),%%xmm4	\n\t	movaps	(%%r12),%%xmm11	\n\t"\
				"movq	%[__i5],%%r11	\n\t	movaps 0x10(%%r11),%%xmm5	\n\t	movaps	(%%r11),%%xmm12	\n\t"\
				"movq	%[__i6],%%r10	\n\t	movaps 0x10(%%r10),%%xmm7	\n\t	movaps	(%%r10),%%xmm14	\n\t"\
				"movaps -0x010(%%rbx),%%xmm0					\n\t"\
				"movq	%[__iC],%%r15	\n\t	addpd 0x10(%%r15),%%xmm1	\n\t	subpd	(%%r15),%%xmm9	\n\t"\
				"movq	%[__iB],%%r14	\n\t	addpd 0x10(%%r14),%%xmm3	\n\t	subpd	(%%r14),%%xmm10	\n\t"\
				"movq	%[__iA],%%r13	\n\t	addpd 0x10(%%r13),%%xmm6	\n\t	subpd	(%%r13),%%xmm13	\n\t"\
				"movq	%[__i9],%%r12	\n\t	addpd 0x10(%%r12),%%xmm4	\n\t	subpd	(%%r12),%%xmm11	\n\t"\
				"movq	%[__i8],%%r11	\n\t	addpd 0x10(%%r11),%%xmm5	\n\t	subpd	(%%r11),%%xmm12	\n\t"\
				"movq	%[__i7],%%r10	\n\t	addpd 0x10(%%r10),%%xmm7	\n\t	subpd	(%%r10),%%xmm14	\n\t"\
				"subpd	%%xmm5,%%xmm1							\n\t			movaps	%%xmm9 ,%%xmm15			\n\t"\
				"mulpd	%%xmm0,%%xmm5							\n\t			movaps	%%xmm10,%%xmm8			\n\t"\
				"subpd	%%xmm6,%%xmm3							\n\t			subpd	%%xmm11,%%xmm15			\n\t"\
				"mulpd	%%xmm0,%%xmm6							\n\t			addpd	%%xmm12,%%xmm8			\n\t"\
				"subpd	%%xmm7,%%xmm4							\n\t			addpd	%%xmm13,%%xmm15			\n\t"\
				"mulpd	%%xmm0,%%xmm7							\n\t			addpd	%%xmm14,%%xmm8			\n\t"\
				"addpd	%%xmm1,%%xmm5							\n\t			addpd	%%xmm11,%%xmm9			\n\t"\
				"addpd	%%xmm3,%%xmm6							\n\t			addpd	%%xmm11,%%xmm13			\n\t"\
				"addpd	%%xmm4,%%xmm7							\n\t			subpd	%%xmm12,%%xmm10			\n\t"\
				"movaps	%%xmm5,%%xmm2							\n\t			subpd	%%xmm12,%%xmm14			\n\t"\
				"addpd	%%xmm6,%%xmm2							\n\t			movaps	%%xmm8 ,%%xmm12			\n\t"\
				"addpd	%%xmm7,%%xmm2							\n\t			movaps	%%xmm15,%%xmm11			\n\t"\
				"movaps	(%%rax),%%xmm0							\n\t			mulpd	0x040(%%rbx),%%xmm8		\n\t"\
				"addpd	%%xmm2,%%xmm0							\n\t			mulpd	0x040(%%rbx),%%xmm15	\n\t"\
				"mulpd	(%%rbx),%%xmm2							\n\t			mulpd	0x030(%%rbx),%%xmm12	\n\t"\
				"movaps	%%xmm0,(%%rcx)							\n\t			mulpd	0x030(%%rbx),%%xmm11	\n\t"\
				"addpd	%%xmm2,%%xmm0							\n\t			subpd	%%xmm15,%%xmm12			\n\t"\
				"movaps	0x010(%%rbx),%%xmm2						\n\t			addpd	%%xmm8 ,%%xmm11			\n\t"\
		/* lcol section uses o-offsets A,B,C - prestore those into r13-15: */\
			"movq	%[__oA],%%r13	\n\t"						/* rcol section uses o-offsets __o1,2 - prestore those into r10,11: */\
			"movq	%[__oB],%%r14	\n\t										movq	%[__o1],%%r10	\n\t"\
			"movq	%[__oC],%%r15	\n\t										movq	%[__o2],%%r11	\n\t"\
				"mulpd	%%xmm2,%%xmm6							\n\t			movaps	%%xmm12,0x10(%%r10)	\n\t"\
				"mulpd	%%xmm2,%%xmm5							\n\t			movaps	%%xmm9 ,%%xmm8			\n\t"\
				"mulpd	%%xmm2,%%xmm7							\n\t			movaps	%%xmm13,%%xmm12			\n\t"\
				"movaps %%xmm6,0x10(%%r15)						\n\t			addpd	%%xmm10,%%xmm8			\n\t"\
				"movaps %%xmm5,0x10(%%r14)						\n\t			addpd	%%xmm14,%%xmm12			\n\t"\
				"movaps %%xmm7,0x10(%%r13)						\n\t			movaps	%%xmm8 ,0x10(%%r11)	\n\t"\
				"movaps 0x080(%%rbx),%%xmm2						\n\t			movaps	%%xmm12,%%xmm15			\n\t"\
				"mulpd	%%xmm2,%%xmm5							\n\t			mulpd	0x0b0(%%rbx),%%xmm12	\n\t"\
				"mulpd	%%xmm2,%%xmm7							\n\t			mulpd	0x0e0(%%rbx),%%xmm8		\n\t"\
				"mulpd	%%xmm2,%%xmm6							\n\t			addpd	0x10(%%r11),%%xmm12	\n\t"\
				"addpd	%%xmm0,%%xmm5							\n\t			addpd	%%xmm15,%%xmm8			\n\t"\
				"addpd	%%xmm0,%%xmm7							\n\t			mulpd	0x050(%%rbx),%%xmm12	\n\t"\
				"addpd	%%xmm0,%%xmm6							\n\t			mulpd	0x050(%%rbx),%%xmm8		\n\t"\
				"addpd	0x10(%%r15),%%xmm5						\n\t			movaps	%%xmm10,0x10(%%r11)	\n\t"\
				"addpd	0x10(%%r14),%%xmm7						\n\t			movaps	%%xmm14,%%xmm15			\n\t"\
				"addpd	0x10(%%r13),%%xmm6						\n\t			mulpd	0x0c0(%%rbx),%%xmm14	\n\t"\
				"movaps 0x0a0(%%rbx),%%xmm2						\n\t			mulpd	0x0f0(%%rbx),%%xmm10	\n\t"\
				"movaps 0x090(%%rbx),%%xmm0						\n\t			addpd	0x10(%%r11),%%xmm14	\n\t"\
				"movaps %%xmm4,0x10(%%r15)						\n\t			addpd	%%xmm15,%%xmm10			\n\t"\
				"movaps %%xmm1,0x10(%%r14)						\n\t			mulpd	0x060(%%rbx),%%xmm14	\n\t"\
				"movaps %%xmm3,0x10(%%r13)						\n\t			mulpd	0x060(%%rbx),%%xmm10	\n\t"\
				"mulpd	%%xmm2,%%xmm4							\n\t			addpd	%%xmm12,%%xmm14			\n\t"\
				"mulpd	%%xmm2,%%xmm1							\n\t			addpd	%%xmm8 ,%%xmm10			\n\t"\
				"mulpd	%%xmm2,%%xmm3							\n\t			movaps	%%xmm9 ,0x10(%%r11)	\n\t"\
				"addpd	0x10(%%r13),%%xmm4						\n\t			movaps	%%xmm13,%%xmm15			\n\t"\
				"subpd	0x10(%%r15),%%xmm1						\n\t			mulpd	0x0d0(%%rbx),%%xmm13	\n\t"\
				"addpd	0x10(%%r14),%%xmm3						\n\t			mulpd	0x100(%%rbx),%%xmm9		\n\t"\
				"mulpd	%%xmm0,%%xmm4							\n\t			addpd	0x10(%%r11),%%xmm13	\n\t"\
				"mulpd	%%xmm0,%%xmm1							\n\t			addpd	%%xmm15,%%xmm9			\n\t"\
				"mulpd	%%xmm0,%%xmm3							\n\t			mulpd	0x070(%%rbx),%%xmm13	\n\t"\
				"movaps 0x020(%%rbx),%%xmm2						\n\t			mulpd	0x070(%%rbx),%%xmm9		\n\t"\
				"addpd	0x10(%%r14),%%xmm4						\n\t			addpd	%%xmm12,%%xmm13			\n\t"\
				"subpd	0x10(%%r13),%%xmm1						\n\t			addpd	%%xmm8 ,%%xmm9			\n\t"\
				"subpd	0x10(%%r15),%%xmm3						\n\t			movaps	0x10(%%r10),%%xmm12	\n\t"\
				"mulpd	%%xmm2,%%xmm4							\n\t			movaps	%%xmm11,%%xmm15			\n\t"\
				"mulpd	%%xmm2,%%xmm1							\n\t			movaps	%%xmm12,%%xmm8			\n\t"\
				"mulpd	%%xmm2,%%xmm3							\n\t			addpd	%%xmm14,%%xmm15			\n\t"\
				"movaps -0x010(%%rbx),%%xmm0					\n\t			addpd	%%xmm13,%%xmm8			\n\t"\
				"subpd	%%xmm4,%%xmm6							\n\t			subpd	%%xmm11,%%xmm14			\n\t"\
				"mulpd	%%xmm0,%%xmm4							\n\t			subpd	%%xmm12,%%xmm13			\n\t"\
				"subpd	%%xmm1,%%xmm7							\n\t			addpd	%%xmm10,%%xmm14			\n\t"\
				"mulpd	%%xmm0,%%xmm1							\n\t			addpd	%%xmm9 ,%%xmm13			\n\t"\
				"subpd	%%xmm3,%%xmm5							\n\t			addpd	%%xmm11,%%xmm10			\n\t"\
				"mulpd	%%xmm0,%%xmm3							\n\t			addpd	%%xmm9 ,%%xmm12			\n\t"\
				"addpd	%%xmm6,%%xmm4							\n\t"\
				"addpd	%%xmm7,%%xmm1							\n\t"\
				"addpd	%%xmm5,%%xmm3							\n\t"\
		  /* yi-data in xmm8,10,12,13,14,15; xmm0,2,9,11 free: */\
			"movq	%[__o7],%%r11	\n\t		movq	%[__oA],%%r13	\n\t"\
			"movq	%[__o6],%%r10	\n\t		movq	%[__o3],%%r12	\n\t"\
				"movaps	%%xmm4 ,%%xmm0					\n\t			movaps	%%xmm7 ,%%xmm9		\n\t"\
				"subpd	%%xmm15,%%xmm4					\n\t			subpd	%%xmm8 ,%%xmm7		\n\t"\
				"addpd	%%xmm15,%%xmm0					\n\t			addpd	%%xmm8 ,%%xmm9		\n\t"\
				"movaps	%%xmm4 ,0x10(%%r11)				\n\t			movaps	%%xmm7 ,0x10(%%r13)	\n\t"\
				"movaps	%%xmm0 ,0x10(%%r10)				\n\t			movaps	%%xmm9 ,0x10(%%r12)	\n\t"\
			"movq	%[__o5],%%r11	\n\t		movq	%[__o9],%%r13	\n\t"\
			"movq	%[__o8],%%r10	\n\t		movq	%[__o4],%%r12	\n\t"\
				"movaps	%%xmm5 ,%%xmm0					\n\t			movaps	%%xmm6 ,%%xmm9		\n\t"\
				"subpd	%%xmm14,%%xmm5					\n\t			subpd	%%xmm13,%%xmm6		\n\t"\
				"addpd	%%xmm14,%%xmm0					\n\t			addpd	%%xmm13,%%xmm9		\n\t"\
				"movaps	%%xmm5 ,0x10(%%r11)				\n\t			movaps	%%xmm6 ,0x10(%%r13)	\n\t"\
				"movaps	%%xmm0 ,0x10(%%r10)				\n\t			movaps	%%xmm9 ,0x10(%%r12)	\n\t"\
			"movq	%[__oB],%%r11	\n\t		movq	%[__oC],%%r13	\n\t"\
			"movq	%[__o2],%%r10	\n\t		movq	%[__o1],%%r12	\n\t"\
				"movaps	%%xmm1 ,%%xmm0					\n\t			movaps	%%xmm3 ,%%xmm9		\n\t"\
				"subpd	%%xmm10,%%xmm1					\n\t			subpd	%%xmm12,%%xmm3		\n\t"\
				"addpd	%%xmm10,%%xmm0					\n\t			addpd	%%xmm12,%%xmm9		\n\t"\
				"movaps	%%xmm1 ,0x10(%%r11)				\n\t			movaps	%%xmm3 ,0x10(%%r13)	\n\t"\
				"movaps	%%xmm0 ,0x10(%%r10)				\n\t			movaps	%%xmm9 ,0x10(%%r12)	\n\t"\
				:					/* outputs: none */\
				: [__cc] "m" (Xcc)	/* All inputs from memory addresses here */\
				 ,[__I0] "m" (XI0)\
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
				 ,[__iB] "m" (XiB)\
				 ,[__iC] "m" (XiC)\
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
				 ,[__oB] "m" (XoB)\
				 ,[__oC] "m" (XoC)\
				: "cc","memory","rax","rbx","rcx","r10","r11","r12","r13","r14","r15","xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7","xmm8","xmm9","xmm10","xmm11","xmm12","xmm13","xmm14","xmm15"		/* Clobbered registers */\
				);\
			}

		  #endif // USE_64BIT_ASM_STYLE

		#endif	// AVX and 64-bit SSE2

	  #endif	// IF(GCC), USE 32/64-BIT ASM STYLE

	#endif	// MSVC / GCC

#endif	/* radix13_sse_macro_h_included */

