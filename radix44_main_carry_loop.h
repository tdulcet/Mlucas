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

// This main loop is same for un-and-multithreaded, so stick into a header file
// (can't use a macro because of the #if-enclosed stuff).

for(k=1; k <= khi; k++)	/* Do n/(radix(1)*nwt) outer loop executions...	*/
{
	for(j = jstart; j < jhi; j += stride)	// Stride = 4 reals for SSE2, 8 for AVX
	{
		j1 =  j;
		j1 = j1 + ( (j1 >> DAT_BITS) << PAD_BITS );	/* padded-array fetch index is here */
		j2 = j1 + RE_IM_STRIDE;

	/*...The radix-44 DIT pass is here:	*/

	#ifdef USE_SSE2

		// Radix-4 Outputs in SSE2 mode are temps 2*11*sz_vd bytes apart:
	  #ifdef USE_AVX
		#define OFF	0x2c0
	  #else
		#define OFF	0x160
	  #endif

	  // Only have a fully-fused macro for the DIT here, not enough timing gain to justify more work for a DIF:
	  #if GCC_ASM_FULL_INLINE

		add0 = &a[j1    ];
		SSE2_RADIX44_DIT_NOTWIDDLE(add0,p01,p02,p03,p04, r00, ua0, s1p00);

	  #else

		/*...gather the needed data (44 64-bit complex, i.e. 88 64-bit reals) and do 11 radix-4 transforms...*/
		/* Outputs in SSE2 modes are temps 2*11*16 = 22*16 = 0x160 bytes apart: */
		add0 = &a[j1    ]; 	add1 = add0+p01;	add3 = add0+p02;	add2 = add0+p03;	SSE2_RADIX4_DIT_0TWIDDLE_STRIDE(add0, add1, add2, add3, r00, OFF)
		add3 = &a[j1+p28];	add2 = add3+p01;	add1 = add3+p02;	add0 = add3+p03;	SSE2_RADIX4_DIT_0TWIDDLE_STRIDE(add0, add1, add2, add3, r01, OFF)
		add3 = &a[j1+p12];	add2 = add3+p01;	add1 = add3+p02;	add0 = add3+p03;	SSE2_RADIX4_DIT_0TWIDDLE_STRIDE(add0, add1, add2, add3, r02, OFF)
		add1 = &a[j1+p40];	add0 = add1+p01;	add2 = add1+p02;	add3 = add1+p03;	SSE2_RADIX4_DIT_0TWIDDLE_STRIDE(add0, add1, add2, add3, r03, OFF)
		add1 = &a[j1+p24];	add0 = add1+p01;	add2 = add1+p02;	add3 = add1+p03;	SSE2_RADIX4_DIT_0TWIDDLE_STRIDE(add0, add1, add2, add3, r04, OFF)
		add1 = &a[j1+p08];	add0 = add1+p01;	add2 = add1+p02;	add3 = add1+p03;	SSE2_RADIX4_DIT_0TWIDDLE_STRIDE(add0, add1, add2, add3, r05, OFF)
		add2 = &a[j1+p36];	add3 = add2+p01;	add0 = add2+p02;	add1 = add2+p03;	SSE2_RADIX4_DIT_0TWIDDLE_STRIDE(add0, add1, add2, add3, r06, OFF)
		add2 = &a[j1+p20];	add3 = add2+p01;	add0 = add2+p02;	add1 = add2+p03;	SSE2_RADIX4_DIT_0TWIDDLE_STRIDE(add0, add1, add2, add3, r07, OFF)
		add2 = &a[j1+p04];	add3 = add2+p01;	add0 = add2+p02;	add1 = add2+p03;	SSE2_RADIX4_DIT_0TWIDDLE_STRIDE(add0, add1, add2, add3, r08, OFF)
		add0 = &a[j1+p32];	add1 = add0+p01;	add3 = add0+p02;	add2 = add0+p03;	SSE2_RADIX4_DIT_0TWIDDLE_STRIDE(add0, add1, add2, add3, r09, OFF)
		add0 = &a[j1+p16];	add1 = add0+p01;	add3 = add0+p02;	add2 = add0+p03;	SSE2_RADIX4_DIT_0TWIDDLE_STRIDE(add0, add1, add2, add3, r0a, OFF)

		/*...and now do 4 radix-11 transforms...*/
		SSE2_RADIX_11_DFT(r00,r01,r02,r03,r04,r05,r06,r07,r08,r09,r0a, ua0, s1p00,s1p0c,s1p18,s1p24,s1p04,s1p10,s1p1c,s1p28,s1p08,s1p14,s1p20)
		SSE2_RADIX_11_DFT(r0b,r0c,r0d,r0e,r0f,r10,r11,r12,r13,r14,r15, ua0, s1p0b,s1p17,s1p23,s1p03,s1p0f,s1p1b,s1p27,s1p07,s1p13,s1p1f,s1p2b)
		SSE2_RADIX_11_DFT(r16,r17,r18,r19,r1a,r1b,r1c,r1d,r1e,r1f,r20, ua0, s1p16,s1p22,s1p02,s1p0e,s1p1a,s1p26,s1p06,s1p12,s1p1e,s1p2a,s1p0a)
		SSE2_RADIX_11_DFT(r21,r22,r23,r24,r25,r26,r27,r28,r29,r2a,r2b, ua0, s1p21,s1p01,s1p0d,s1p19,s1p25,s1p05,s1p11,s1p1d,s1p29,s1p09,s1p15)

	  #endif	// AVX or SSE2?

	#else	/* !USE_SSE2 */

		/*...gather the needed data (44 64-bit complex, i.e. 88 64-bit reals) and do 11 radix-4 transforms...*/
		tptr = t;
		jt = j1    ; jp = j2    ;	RADIX_04_DIT(a[jt    ],a[jp    ],a[jt+p01],a[jp+p01],a[jt+p03],a[jp+p03],a[jt+p02],a[jp+p02], tptr->re,tptr->im,(tptr+11)->re,(tptr+11)->im,(tptr+22)->re,(tptr+22)->im,(tptr+33)->re,(tptr+33)->im, rt,it);	tptr++;
		jt = j1+p28; jp = j2+p28;	RADIX_04_DIT(a[jt+p03],a[jp+p03],a[jt+p02],a[jp+p02],a[jt+p01],a[jp+p01],a[jt    ],a[jp    ], tptr->re,tptr->im,(tptr+11)->re,(tptr+11)->im,(tptr+22)->re,(tptr+22)->im,(tptr+33)->re,(tptr+33)->im, rt,it);	tptr++;
		jt = j1+p12; jp = j2+p12;	RADIX_04_DIT(a[jt+p03],a[jp+p03],a[jt+p02],a[jp+p02],a[jt+p01],a[jp+p01],a[jt    ],a[jp    ], tptr->re,tptr->im,(tptr+11)->re,(tptr+11)->im,(tptr+22)->re,(tptr+22)->im,(tptr+33)->re,(tptr+33)->im, rt,it);	tptr++;
		jt = j1+p40; jp = j2+p40;	RADIX_04_DIT(a[jt+p01],a[jp+p01],a[jt    ],a[jp    ],a[jt+p02],a[jp+p02],a[jt+p03],a[jp+p03], tptr->re,tptr->im,(tptr+11)->re,(tptr+11)->im,(tptr+22)->re,(tptr+22)->im,(tptr+33)->re,(tptr+33)->im, rt,it);	tptr++;
		jt = j1+p24; jp = j2+p24;	RADIX_04_DIT(a[jt+p01],a[jp+p01],a[jt    ],a[jp    ],a[jt+p02],a[jp+p02],a[jt+p03],a[jp+p03], tptr->re,tptr->im,(tptr+11)->re,(tptr+11)->im,(tptr+22)->re,(tptr+22)->im,(tptr+33)->re,(tptr+33)->im, rt,it);	tptr++;
		jt = j1+p08; jp = j2+p08;	RADIX_04_DIT(a[jt+p01],a[jp+p01],a[jt    ],a[jp    ],a[jt+p02],a[jp+p02],a[jt+p03],a[jp+p03], tptr->re,tptr->im,(tptr+11)->re,(tptr+11)->im,(tptr+22)->re,(tptr+22)->im,(tptr+33)->re,(tptr+33)->im, rt,it);	tptr++;
		jt = j1+p36; jp = j2+p36;	RADIX_04_DIT(a[jt+p02],a[jp+p02],a[jt+p03],a[jp+p03],a[jt    ],a[jp    ],a[jt+p01],a[jp+p01], tptr->re,tptr->im,(tptr+11)->re,(tptr+11)->im,(tptr+22)->re,(tptr+22)->im,(tptr+33)->re,(tptr+33)->im, rt,it);	tptr++;
		jt = j1+p20; jp = j2+p20;	RADIX_04_DIT(a[jt+p02],a[jp+p02],a[jt+p03],a[jp+p03],a[jt    ],a[jp    ],a[jt+p01],a[jp+p01], tptr->re,tptr->im,(tptr+11)->re,(tptr+11)->im,(tptr+22)->re,(tptr+22)->im,(tptr+33)->re,(tptr+33)->im, rt,it);	tptr++;
		jt = j1+p04; jp = j2+p04;	RADIX_04_DIT(a[jt+p02],a[jp+p02],a[jt+p03],a[jp+p03],a[jt    ],a[jp    ],a[jt+p01],a[jp+p01], tptr->re,tptr->im,(tptr+11)->re,(tptr+11)->im,(tptr+22)->re,(tptr+22)->im,(tptr+33)->re,(tptr+33)->im, rt,it);	tptr++;
		jt = j1+p32; jp = j2+p32;	RADIX_04_DIT(a[jt    ],a[jp    ],a[jt+p01],a[jp+p01],a[jt+p03],a[jp+p03],a[jt+p02],a[jp+p02], tptr->re,tptr->im,(tptr+11)->re,(tptr+11)->im,(tptr+22)->re,(tptr+22)->im,(tptr+33)->re,(tptr+33)->im, rt,it);	tptr++;
		jt = j1+p16; jp = j2+p16;	RADIX_04_DIT(a[jt    ],a[jp    ],a[jt+p01],a[jp+p01],a[jt+p03],a[jp+p03],a[jt+p02],a[jp+p02], tptr->re,tptr->im,(tptr+11)->re,(tptr+11)->im,(tptr+22)->re,(tptr+22)->im,(tptr+33)->re,(tptr+33)->im, rt,it);
		/*...and now do 4 radix-11 transforms...*/
		tptr = t;
		jt = j1    ; jp = j2    ;	RADIX_11_DFT(tptr->re,tptr->im,(tptr+0x1)->re,(tptr+0x1)->im,(tptr+0x2)->re,(tptr+0x2)->im,(tptr+0x3)->re,(tptr+0x3)->im,(tptr+0x4)->re,(tptr+0x4)->im,(tptr+0x5)->re,(tptr+0x5)->im,(tptr+0x6)->re,(tptr+0x6)->im,(tptr+0x7)->re,(tptr+0x7)->im,(tptr+0x8)->re,(tptr+0x8)->im,(tptr+0x9)->re,(tptr+0x9)->im,(tptr+0xa)->re,(tptr+0xa)->im, a[jt    ],a[jp    ],a[jt+p12],a[jp+p12],a[jt+p24],a[jp+p24],a[jt+p36],a[jp+p36],a[jt+p04],a[jp+p04],a[jt+p16],a[jp+p16],a[jt+p28],a[jp+p28],a[jt+p40],a[jp+p40],a[jt+p08],a[jp+p08],a[jt+p20],a[jp+p20],a[jt+p32],a[jp+p32], a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,b0,b1,b2,b3,b4,b5,b6,b7,b8,b9);	tptr += 11;
		jt = j1+p03; jp = j2+p03;	RADIX_11_DFT(tptr->re,tptr->im,(tptr+0x1)->re,(tptr+0x1)->im,(tptr+0x2)->re,(tptr+0x2)->im,(tptr+0x3)->re,(tptr+0x3)->im,(tptr+0x4)->re,(tptr+0x4)->im,(tptr+0x5)->re,(tptr+0x5)->im,(tptr+0x6)->re,(tptr+0x6)->im,(tptr+0x7)->re,(tptr+0x7)->im,(tptr+0x8)->re,(tptr+0x8)->im,(tptr+0x9)->re,(tptr+0x9)->im,(tptr+0xa)->re,(tptr+0xa)->im, a[jt+p08],a[jp+p08],a[jt+p20],a[jp+p20],a[jt+p32],a[jp+p32],a[jt    ],a[jp    ],a[jt+p12],a[jp+p12],a[jt+p24],a[jp+p24],a[jt+p36],a[jp+p36],a[jt+p04],a[jp+p04],a[jt+p16],a[jp+p16],a[jt+p28],a[jp+p28],a[jt+p40],a[jp+p40], a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,b0,b1,b2,b3,b4,b5,b6,b7,b8,b9);	tptr += 11;
		jt = j1+p02; jp = j2+p02;	RADIX_11_DFT(tptr->re,tptr->im,(tptr+0x1)->re,(tptr+0x1)->im,(tptr+0x2)->re,(tptr+0x2)->im,(tptr+0x3)->re,(tptr+0x3)->im,(tptr+0x4)->re,(tptr+0x4)->im,(tptr+0x5)->re,(tptr+0x5)->im,(tptr+0x6)->re,(tptr+0x6)->im,(tptr+0x7)->re,(tptr+0x7)->im,(tptr+0x8)->re,(tptr+0x8)->im,(tptr+0x9)->re,(tptr+0x9)->im,(tptr+0xa)->re,(tptr+0xa)->im, a[jt+p20],a[jp+p20],a[jt+p32],a[jp+p32],a[jt    ],a[jp    ],a[jt+p12],a[jp+p12],a[jt+p24],a[jp+p24],a[jt+p36],a[jp+p36],a[jt+p04],a[jp+p04],a[jt+p16],a[jp+p16],a[jt+p28],a[jp+p28],a[jt+p40],a[jp+p40],a[jt+p08],a[jp+p08], a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,b0,b1,b2,b3,b4,b5,b6,b7,b8,b9);	tptr += 11;
		jt = j1+p01; jp = j2+p01;	RADIX_11_DFT(tptr->re,tptr->im,(tptr+0x1)->re,(tptr+0x1)->im,(tptr+0x2)->re,(tptr+0x2)->im,(tptr+0x3)->re,(tptr+0x3)->im,(tptr+0x4)->re,(tptr+0x4)->im,(tptr+0x5)->re,(tptr+0x5)->im,(tptr+0x6)->re,(tptr+0x6)->im,(tptr+0x7)->re,(tptr+0x7)->im,(tptr+0x8)->re,(tptr+0x8)->im,(tptr+0x9)->re,(tptr+0x9)->im,(tptr+0xa)->re,(tptr+0xa)->im, a[jt+p32],a[jp+p32],a[jt    ],a[jp    ],a[jt+p12],a[jp+p12],a[jt+p24],a[jp+p24],a[jt+p36],a[jp+p36],a[jt+p04],a[jp+p04],a[jt+p16],a[jp+p16],a[jt+p28],a[jp+p28],a[jt+p40],a[jp+p40],a[jt+p08],a[jp+p08],a[jt+p20],a[jp+p20], a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,b0,b1,b2,b3,b4,b5,b6,b7,b8,b9);

	#endif

/*...Now do the carries. Since the outputs would
	normally be getting dispatched to RADIX separate blocks of the A-array, we need RADIX separate carries.	*/

	#ifdef USE_AVX

		add1 = &wt1[col  ];
		add2 = &wt1[co2-1];
		add3 = &wt1[co3-1];

		l= j & (nwt-1);						tmp = half_arr + 64;	/* ptr to local storage for the doubled wtl,wtn terms: */
		n_minus_sil  ->d0 = n-si[l  ];		tmp->d0 = wt0[    l  ];
		n_minus_silp1->d0 = n-si[l+1];		tmp->d1 = wt0[nwt-l  ]*scale;
		sinwt        ->d0 = si[nwt-l  ];	tmp->d2 = wt0[    l+1];
		sinwtm1      ->d0 = si[nwt-l-1];	tmp->d3 = wt0[nwt-l-1]*scale;

		l= (j+2) & (nwt-1);					++tmp;	/* Get ready for next 4 weights-related doubles... */
		n_minus_sil  ->d1 = n-si[l  ];		tmp->d0 = wt0[    l  ];
		n_minus_silp1->d1 = n-si[l+1];		tmp->d1 = wt0[nwt-l  ]*scale;
		sinwt        ->d1 = si[nwt-l  ];	tmp->d2 = wt0[    l+1];
		sinwtm1      ->d1 = si[nwt-l-1];	tmp->d3 = wt0[nwt-l-1]*scale;

		l= (j+4) & (nwt-1);					++tmp;	/* Get ready for next 4 weights-related doubles... */
		n_minus_sil  ->d2 = n-si[l  ];		tmp->d0 = wt0[    l  ];
		n_minus_silp1->d2 = n-si[l+1];		tmp->d1 = wt0[nwt-l  ]*scale;
		sinwt        ->d2 = si[nwt-l  ];	tmp->d2 = wt0[    l+1];
		sinwtm1      ->d2 = si[nwt-l-1];	tmp->d3 = wt0[nwt-l-1]*scale;

		l= (j+6) & (nwt-1);					++tmp;	/* Get ready for next 4 weights-related doubles... */
		n_minus_sil  ->d3 = n-si[l  ];		tmp->d0 = wt0[    l  ];
		n_minus_silp1->d3 = n-si[l+1];		tmp->d1 = wt0[nwt-l  ]*scale;
		sinwt        ->d3 = si[nwt-l  ];	tmp->d2 = wt0[    l+1];
		sinwtm1      ->d3 = si[nwt-l-1];	tmp->d3 = wt0[nwt-l-1]*scale;

	/* In AVX mode advance carry-ptrs just 1 for each vector-carry-macro call: */
		tm1 = s1p00; tmp = cy; itmp = bjmodn;
		AVX_cmplx_carry_norm_errcheck0_X4(tm1,add1,add2,add3,tmp,itmp,half_arr,i,n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_n,sse_sw);
		tm1 += 8; tmp += 1; itmp += 4;
		for(l = 1; l < RADIX>>2; l++) {
			AVX_cmplx_carry_norm_errcheck1_X4(tm1,add1,add2,add3,tmp,itmp,half_arr,  n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_n,sse_sw);
			tm1 += 8; tmp += 1; itmp += 4;
		}

		co2 = co3;	// For all data but the first set in each j-block, co2=co3. Thus, after the first block of data is done
					// (and only then: for all subsequent blocks it's superfluous), this assignment decrements co2 by radix(1).

		i =((uint32)(sw - bjmodn[0]) >> 31);	/* get ready for the next set...	*/

	#elif defined(USE_SSE2)

		l= j & (nwt-1);
		n_minus_sil   = n-si[l  ];
		n_minus_silp1 = n-si[l+1];
		sinwt   = si[nwt-l  ];
		sinwtm1 = si[nwt-l-1];

		wtl     =wt0[    l  ];
		wtn     =wt0[nwt-l  ]*scale;	/* Include 1/(n/2) scale factor of inverse transform here...	*/
		wtlp1   =wt0[    l+1];
		wtnm1   =wt0[nwt-l-1]*scale;	/* ...and here.	*/

	/************ See the radix16_ditN_cy_dif1 routine for details on how the SSE2 carry stuff works **********/

		ctmp = (struct complex *)half_arr + 16;	/* ptr to local storage for the doubled wtl,wtn terms: */
		ctmp->re = wtl;		ctmp->im = wtl;	++ctmp;
		ctmp->re = wtn;		ctmp->im = wtn;	++ctmp;
		ctmp->re = wtlp1;	ctmp->im = wtlp1;++ctmp;
		ctmp->re = wtnm1;	ctmp->im = wtnm1;

		add1 = &wt1[col  ];	/* Don't use add0 here, to avoid need to reload main-array address */
		add2 = &wt1[co2-1];
		add3 = &wt1[co3-1];

		tm1 = s1p00; tmp = cy; tm2 = cy+0x01; itmp = bjmodn;
		SSE2_cmplx_carry_norm_errcheck0_2B(tm1,add1,add2,add3,tmp,tm2,itmp,half_arr,i,n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_n,sse_sw);	tm1 += 8; tmp += 2; tm2 += 2; itmp += 4;
		for(l = 1; l < RADIX>>2; l++) {
			SSE2_cmplx_carry_norm_errcheck1_2B(tm1,add1,add2,add3,tmp,tm2,itmp,half_arr,  n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_n,sse_sw);	tm1 += 8; tmp += 2; tm2 += 2; itmp += 4;
		}

		l= (j+2) & (nwt-1);			/* We want (S*J mod N) - SI(L) for all 16 carries, so precompute	*/
		n_minus_sil   = n-si[l  ];		/* N - SI(L) and for each J, find N - (B*J mod N) - SI(L)		*/
		n_minus_silp1 = n-si[l+1];		/* For the inverse weight, want (S*(N - J) mod N) - SI(NWT - L) =	*/
		sinwt   = si[nwt-l  ];		/*	= N - (S*J mod N) - SI(NWT - L) = (B*J mod N) - SI(NWT - L).	*/
		sinwtm1 = si[nwt-l-1];

		wtl     =wt0[    l  ];
		wtn     =wt0[nwt-l  ]*scale;	/* Include 1/(n/2) scale factor of inverse transform here...	*/
		wtlp1   =wt0[    l+1];
		wtnm1   =wt0[nwt-l-1]*scale;	/* ...and here.	*/

		ctmp = (struct complex *)half_arr + 16;	/* ptr to local storage for the doubled wtl,wtn terms: */
		ctmp->re = wtl;		ctmp->im = wtl;	++ctmp;
		ctmp->re = wtn;		ctmp->im = wtn;	++ctmp;
		ctmp->re = wtlp1;	ctmp->im = wtlp1;++ctmp;
		ctmp->re = wtnm1;	ctmp->im = wtnm1;

		co2 = co3;	/* For all data but the first set in each j-block, co2=co3. Thus, after the first block of data is done
					(and only then: for all subsequent blocks it's superfluous), this assignment decrements co2 by radix(1).	*/

		add1 = &wt1[col  ];
		add2 = &wt1[co2-1];

		tm1 = s1p00; tmp = cy; tm2 = cy+0x01; itmp = bjmodn;
		for(l = 0; l < RADIX>>2; l++) {
			SSE2_cmplx_carry_norm_errcheck2_2B(tm1,add1,add2,     tmp,tm2,itmp,half_arr,  n_minus_silp1,n_minus_sil,sign_mask,sinwt,sinwtm1,sse_bw,sse_n,sse_sw);	tm1 += 8; tmp += 2; tm2 += 2; itmp += 4;
		}

		i =((uint32)(sw - bjmodn[0]) >> 31);	/* get ready for the next set...	*/

	#else	// Scalar-double mode:

		l= j & (nwt-1);
		n_minus_sil   = n-si[l  ];
		n_minus_silp1 = n-si[l+1];
		sinwt   = si[nwt-l  ];
		sinwtm1 = si[nwt-l-1];

		wtl     =wt0[    l  ];
		wtn     =wt0[nwt-l  ]*scale;	/* Include 1/(n/2) scale factor of inverse transform here...	*/
		wtlp1   =wt0[    l+1];
		wtnm1   =wt0[nwt-l-1]*scale;	/* ...and here.	*/

		/*...set0 is slightly different from others; divide work into blocks of 4 macro calls, 1st set of which gets pulled out of loop: */		
		l = 0; addr = cy; itmp = bjmodn;
	   cmplx_carry_norm_errcheck0(a[j1    ],a[j2    ],*addr,*itmp  ); ++l; ++addr; ++itmp;
		cmplx_carry_norm_errcheck(a[j1+p01],a[j2+p01],*addr,*itmp,l); ++l; ++addr; ++itmp;
		cmplx_carry_norm_errcheck(a[j1+p02],a[j2+p02],*addr,*itmp,l); ++l; ++addr; ++itmp;
		cmplx_carry_norm_errcheck(a[j1+p03],a[j2+p03],*addr,*itmp,l); ++l; ++addr; ++itmp;
		// Remaining quartets of macro calls done in loop:
		for(ntmp = 1; ntmp < RADIX>>2; ntmp++) {
			jt = j1 + poff[ntmp]; jp = j2 + poff[ntmp];	// poff[] = p04,p08,...
			cmplx_carry_norm_errcheck(a[jt    ],a[jp    ],*addr,*itmp,l); ++l; ++addr; ++itmp;
			cmplx_carry_norm_errcheck(a[jt+p01],a[jp+p01],*addr,*itmp,l); ++l; ++addr; ++itmp;
			cmplx_carry_norm_errcheck(a[jt+p02],a[jp+p02],*addr,*itmp,l); ++l; ++addr; ++itmp;
			cmplx_carry_norm_errcheck(a[jt+p03],a[jp+p03],*addr,*itmp,l); ++l; ++addr; ++itmp;
		}

		i =((uint32)(sw - bjmodn[0]) >> 31);	/* get ready for the next set...	*/
		co2 = co3;	/* For all data but the first set in each j-block, co2=co3. Thus, after the first block of data is done
					(and only then: for all subsequent blocks it's superfluous)ghis assignment decrements co2 by radix(1).	*/

	#endif	// USE_AVX?

		/*...The radix-44 DIF pass is here:	*/

	#ifdef USE_SSE2

		/*...gather the needed data (44 64-bit complex, i.e. 88 64-bit reals) and do 4 radix-11 transforms...*/
		SSE2_RADIX_11_DFT(s1p00,s1p28,s1p24,s1p20,s1p1c,s1p18,s1p14,s1p10,s1p0c,s1p08,s1p04, ua0, r00,r01,r02,r03,r04,r05,r06,r07,r08,r09,r0a)
		SSE2_RADIX_11_DFT(s1p21,s1p1d,s1p19,s1p15,s1p11,s1p0d,s1p09,s1p05,s1p01,s1p29,s1p25, ua0, r0b,r0c,r0d,r0e,r0f,r10,r11,r12,r13,r14,r15)
		SSE2_RADIX_11_DFT(s1p16,s1p12,s1p0e,s1p0a,s1p06,s1p02,s1p2a,s1p26,s1p22,s1p1e,s1p1a, ua0, r16,r17,r18,r19,r1a,r1b,r1c,r1d,r1e,r1f,r20)
		SSE2_RADIX_11_DFT(s1p0b,s1p07,s1p03,s1p2b,s1p27,s1p23,s1p1f,s1p1b,s1p17,s1p13,s1p0f, ua0, r21,r22,r23,r24,r25,r26,r27,r28,r29,r2a,r2b)

		/*...and now do 11 radix-4 transforms...*/
		// Radix-4 Inputs in SIMD mode are temps 2*11*sz_vd bytes apart. Notice how the add* indices after the first row repeat with period 4:OFF:
		add0 = &a[j1    ]; 	add1 = add0+p01;	add2 = add0+p02;	add3 = add0+p03;	SSE2_RADIX4_DIF_0TWIDDLE_STRIDE(add0, add1, add2, add3, r00, OFF)
		add1 = &a[j1+p40];	add0 = add1+p01;	add3 = add1+p02;	add2 = add1+p03;	SSE2_RADIX4_DIF_0TWIDDLE_STRIDE(add0, add1, add2, add3, r01, OFF)
		add3 = &a[j1+p36];	add2 = add3+p01;	add0 = add3+p02;	add1 = add3+p03;	SSE2_RADIX4_DIF_0TWIDDLE_STRIDE(add0, add1, add2, add3, r02, OFF)
		add0 = &a[j1+p32];	add1 = add0+p01;	add2 = add0+p02;	add3 = add0+p03;	SSE2_RADIX4_DIF_0TWIDDLE_STRIDE(add0, add1, add2, add3, r03, OFF)
		add2 = &a[j1+p28];	add3 = add2+p01;	add1 = add2+p02;	add0 = add2+p03;	SSE2_RADIX4_DIF_0TWIDDLE_STRIDE(add0, add1, add2, add3, r04, OFF)
		add1 = &a[j1+p24];	add0 = add1+p01;	add3 = add1+p02;	add2 = add1+p03;	SSE2_RADIX4_DIF_0TWIDDLE_STRIDE(add0, add1, add2, add3, r05, OFF)
		add3 = &a[j1+p20];	add2 = add3+p01;	add0 = add3+p02;	add1 = add3+p03;	SSE2_RADIX4_DIF_0TWIDDLE_STRIDE(add0, add1, add2, add3, r06, OFF)
		add0 = &a[j1+p16];	add1 = add0+p01;	add2 = add0+p02;	add3 = add0+p03;	SSE2_RADIX4_DIF_0TWIDDLE_STRIDE(add0, add1, add2, add3, r07, OFF)
		add2 = &a[j1+p12];	add3 = add2+p01;	add1 = add2+p02;	add0 = add2+p03;	SSE2_RADIX4_DIF_0TWIDDLE_STRIDE(add0, add1, add2, add3, r08, OFF)
		add1 = &a[j1+p08];	add0 = add1+p01;	add3 = add1+p02;	add2 = add1+p03;	SSE2_RADIX4_DIF_0TWIDDLE_STRIDE(add0, add1, add2, add3, r09, OFF)
		add3 = &a[j1+p04];	add2 = add3+p01;	add0 = add3+p02;	add1 = add3+p03;	SSE2_RADIX4_DIF_0TWIDDLE_STRIDE(add0, add1, add2, add3, r0a, OFF)

	  #undef OFF

	#else	/* !USE_SSE2 */

		/*...gather the needed data (44 64-bit complex, i.e. 88 64-bit reals) and do 4 radix-11 transforms...*/
		tptr = t;
		jt = j1    ; jp = j2    ;	RADIX_11_DFT(a[jt    ],a[jp    ],a[jt+p40],a[jp+p40],a[jt+p36],a[jp+p36],a[jt+p32],a[jp+p32],a[jt+p28],a[jp+p28],a[jt+p24],a[jp+p24],a[jt+p20],a[jp+p20],a[jt+p16],a[jp+p16],a[jt+p12],a[jp+p12],a[jt+p08],a[jp+p08],a[jt+p04],a[jp+p04], tptr->re,tptr->im,(tptr+0x1)->re,(tptr+0x1)->im,(tptr+0x2)->re,(tptr+0x2)->im,(tptr+0x3)->re,(tptr+0x3)->im,(tptr+0x4)->re,(tptr+0x4)->im,(tptr+0x5)->re,(tptr+0x5)->im,(tptr+0x6)->re,(tptr+0x6)->im,(tptr+0x7)->re,(tptr+0x7)->im,(tptr+0x8)->re,(tptr+0x8)->im,(tptr+0x9)->re,(tptr+0x9)->im,(tptr+0xa)->re,(tptr+0xa)->im, a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,b0,b1,b2,b3,b4,b5,b6,b7,b8,b9);	tptr += 11;
		jt = j1+p01; jp = j2+p01;	RADIX_11_DFT(a[jt+p32],a[jp+p32],a[jt+p28],a[jp+p28],a[jt+p24],a[jp+p24],a[jt+p20],a[jp+p20],a[jt+p16],a[jp+p16],a[jt+p12],a[jp+p12],a[jt+p08],a[jp+p08],a[jt+p04],a[jp+p04],a[jt    ],a[jp    ],a[jt+p40],a[jp+p40],a[jt+p36],a[jp+p36], tptr->re,tptr->im,(tptr+0x1)->re,(tptr+0x1)->im,(tptr+0x2)->re,(tptr+0x2)->im,(tptr+0x3)->re,(tptr+0x3)->im,(tptr+0x4)->re,(tptr+0x4)->im,(tptr+0x5)->re,(tptr+0x5)->im,(tptr+0x6)->re,(tptr+0x6)->im,(tptr+0x7)->re,(tptr+0x7)->im,(tptr+0x8)->re,(tptr+0x8)->im,(tptr+0x9)->re,(tptr+0x9)->im,(tptr+0xa)->re,(tptr+0xa)->im, a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,b0,b1,b2,b3,b4,b5,b6,b7,b8,b9);	tptr += 11;
		jt = j1+p02; jp = j2+p02;	RADIX_11_DFT(a[jt+p20],a[jp+p20],a[jt+p16],a[jp+p16],a[jt+p12],a[jp+p12],a[jt+p08],a[jp+p08],a[jt+p04],a[jp+p04],a[jt    ],a[jp    ],a[jt+p40],a[jp+p40],a[jt+p36],a[jp+p36],a[jt+p32],a[jp+p32],a[jt+p28],a[jp+p28],a[jt+p24],a[jp+p24], tptr->re,tptr->im,(tptr+0x1)->re,(tptr+0x1)->im,(tptr+0x2)->re,(tptr+0x2)->im,(tptr+0x3)->re,(tptr+0x3)->im,(tptr+0x4)->re,(tptr+0x4)->im,(tptr+0x5)->re,(tptr+0x5)->im,(tptr+0x6)->re,(tptr+0x6)->im,(tptr+0x7)->re,(tptr+0x7)->im,(tptr+0x8)->re,(tptr+0x8)->im,(tptr+0x9)->re,(tptr+0x9)->im,(tptr+0xa)->re,(tptr+0xa)->im, a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,b0,b1,b2,b3,b4,b5,b6,b7,b8,b9);	tptr += 11;
		jt = j1+p03; jp = j2+p03;	RADIX_11_DFT(a[jt+p08],a[jp+p08],a[jt+p04],a[jp+p04],a[jt    ],a[jp    ],a[jt+p40],a[jp+p40],a[jt+p36],a[jp+p36],a[jt+p32],a[jp+p32],a[jt+p28],a[jp+p28],a[jt+p24],a[jp+p24],a[jt+p20],a[jp+p20],a[jt+p16],a[jp+p16],a[jt+p12],a[jp+p12], tptr->re,tptr->im,(tptr+0x1)->re,(tptr+0x1)->im,(tptr+0x2)->re,(tptr+0x2)->im,(tptr+0x3)->re,(tptr+0x3)->im,(tptr+0x4)->re,(tptr+0x4)->im,(tptr+0x5)->re,(tptr+0x5)->im,(tptr+0x6)->re,(tptr+0x6)->im,(tptr+0x7)->re,(tptr+0x7)->im,(tptr+0x8)->re,(tptr+0x8)->im,(tptr+0x9)->re,(tptr+0x9)->im,(tptr+0xa)->re,(tptr+0xa)->im, a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,b0,b1,b2,b3,b4,b5,b6,b7,b8,b9);
		/*...and now do 11 radix-4 transforms...*/
		tptr = t;
		jt = j1    ; jp = j2    ;	RADIX_04_DIF(tptr->re,tptr->im,(tptr+11)->re,(tptr+11)->im,(tptr+22)->re,(tptr+22)->im,(tptr+33)->re,(tptr+33)->im, a[jt    ],a[jp    ],a[jt+p01],a[jp+p01],a[jt+p02],a[jp+p02],a[jt+p03],a[jp+p03],rt,it);	tptr++;
		jt = j1+p40; jp = j2+p40;	RADIX_04_DIF(tptr->re,tptr->im,(tptr+11)->re,(tptr+11)->im,(tptr+22)->re,(tptr+22)->im,(tptr+33)->re,(tptr+33)->im, a[jt+p01],a[jp+p01],a[jt    ],a[jp    ],a[jt+p03],a[jp+p03],a[jt+p02],a[jp+p02],rt,it);	tptr++;
		jt = j1+p36; jp = j2+p36;	RADIX_04_DIF(tptr->re,tptr->im,(tptr+11)->re,(tptr+11)->im,(tptr+22)->re,(tptr+22)->im,(tptr+33)->re,(tptr+33)->im, a[jt+p02],a[jp+p02],a[jt+p03],a[jp+p03],a[jt+p01],a[jp+p01],a[jt    ],a[jp    ],rt,it);	tptr++;
		jt = j1+p32; jp = j2+p32;	RADIX_04_DIF(tptr->re,tptr->im,(tptr+11)->re,(tptr+11)->im,(tptr+22)->re,(tptr+22)->im,(tptr+33)->re,(tptr+33)->im, a[jt    ],a[jp    ],a[jt+p01],a[jp+p01],a[jt+p02],a[jp+p02],a[jt+p03],a[jp+p03],rt,it);	tptr++;
		jt = j1+p28; jp = j2+p28;	RADIX_04_DIF(tptr->re,tptr->im,(tptr+11)->re,(tptr+11)->im,(tptr+22)->re,(tptr+22)->im,(tptr+33)->re,(tptr+33)->im, a[jt+p03],a[jp+p03],a[jt+p02],a[jp+p02],a[jt    ],a[jp    ],a[jt+p01],a[jp+p01],rt,it);	tptr++;
		jt = j1+p24; jp = j2+p24;	RADIX_04_DIF(tptr->re,tptr->im,(tptr+11)->re,(tptr+11)->im,(tptr+22)->re,(tptr+22)->im,(tptr+33)->re,(tptr+33)->im, a[jt+p01],a[jp+p01],a[jt    ],a[jp    ],a[jt+p03],a[jp+p03],a[jt+p02],a[jp+p02],rt,it);	tptr++;
		jt = j1+p20; jp = j2+p20;	RADIX_04_DIF(tptr->re,tptr->im,(tptr+11)->re,(tptr+11)->im,(tptr+22)->re,(tptr+22)->im,(tptr+33)->re,(tptr+33)->im, a[jt+p02],a[jp+p02],a[jt+p03],a[jp+p03],a[jt+p01],a[jp+p01],a[jt    ],a[jp    ],rt,it);	tptr++;
		jt = j1+p16; jp = j2+p16;	RADIX_04_DIF(tptr->re,tptr->im,(tptr+11)->re,(tptr+11)->im,(tptr+22)->re,(tptr+22)->im,(tptr+33)->re,(tptr+33)->im, a[jt    ],a[jp    ],a[jt+p01],a[jp+p01],a[jt+p02],a[jp+p02],a[jt+p03],a[jp+p03],rt,it);	tptr++;
		jt = j1+p12; jp = j2+p12;	RADIX_04_DIF(tptr->re,tptr->im,(tptr+11)->re,(tptr+11)->im,(tptr+22)->re,(tptr+22)->im,(tptr+33)->re,(tptr+33)->im, a[jt+p03],a[jp+p03],a[jt+p02],a[jp+p02],a[jt    ],a[jp    ],a[jt+p01],a[jp+p01],rt,it);	tptr++;
		jt = j1+p08; jp = j2+p08;	RADIX_04_DIF(tptr->re,tptr->im,(tptr+11)->re,(tptr+11)->im,(tptr+22)->re,(tptr+22)->im,(tptr+33)->re,(tptr+33)->im, a[jt+p01],a[jp+p01],a[jt    ],a[jp    ],a[jt+p03],a[jp+p03],a[jt+p02],a[jp+p02],rt,it);	tptr++;
		jt = j1+p04; jp = j2+p04;	RADIX_04_DIF(tptr->re,tptr->im,(tptr+11)->re,(tptr+11)->im,(tptr+22)->re,(tptr+22)->im,(tptr+33)->re,(tptr+33)->im, a[jt+p02],a[jp+p02],a[jt+p03],a[jp+p03],a[jt+p01],a[jp+p01],a[jt    ],a[jp    ],rt,it);

	#endif	/* !USE_SSE2 */

	}	/* end for(j=_jstart; j < _jhi; j += 2) */

	jstart += nwt;
	jhi    += nwt;

	col += RADIX;
	co3 -= RADIX;
}	/* end for(k=1; k <= khi; k++) */