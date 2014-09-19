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

/****************************************************************************
 * We now include this header file if it was not included before.
 ****************************************************************************/
#ifndef fac_test_dat128_included
#define fac_test_dat128_included

	#include "types.h"

	struct testFac128{
		uint32 p;
		uint64 d1;
		uint64 d0;
	};

	struct testFac128x2{
		uint64 phi;
		uint64 plo;
		uint64 d1;
		uint64 d0;
	};

	/* Factors > 96 but <= 128 bits. If desired, we can construct more test factors
	by multiplying together a 63/64-bit factor q1 of M(p1) and a 65/64-bit factor q2 of M(p2)
	and checking whether q1*q2 divides M(p1*p2).*/
	static const struct testFac128 fac128[] =
	{
		{     695,   12240518780192025ull, 1654746039858251761ull},
		{     845, 2923447923687422893ull,  353773459776294223ull},
		{    1113,     128099917305337ull, 7733695761441692271ull},
		{    1145,   10811609908058563ull, 5349936413307099433ull},
		{    1149,        700245770430ull,  701890237964104231ull},
		{     733,  756146046438660814ull, 7804835620876695225ull},
		{     737,  106450884062962221ull,17050154159176967743ull},
		{     871, 7448657723978021346ull,15223106393317212577ull},
		{     947,     644719741813452ull,16055621295463638505ull},
		{     953,      44696312570505ull, 4961431981940124743ull},
		{     989,   99970972632587991ull, 1738540175825943815ull},
		{    1081,         67677680549ull,13887741953162944095ull},
		{    1091,    5287390011750720ull, 2894679571106043497ull},
		{    1097,      11129117045170ull,10375766809019373543ull},
		{    1099,       1551337752834ull, 8321741389535251703ull},
		{    1133,  133834206206032981ull, 6586095673132787791ull},
		{    1141,       5747037125100ull, 2460710484528304153ull},
		{    1181,      10824073357153ull, 7361144750966677159ull},
		{    1189,   32559650929209964ull, 8212830436061989903ull},
		{   27691,   94004235929829273ull, 4235226679561594903ull},
		{  319057,        103337218078ull, 8676403300852410079ull},
		{17363977,      62897895526806ull,14211535226588354713ull},
		{10624093,          5492917609ull,14854696485656401105ull},
		{10698673,          5799457823ull,10285356664749312993ull},
		{20799431,          4303087381ull,16578386512849109713ull},
		{33652757,          5202063708ull,18263664019678288919ull},
		{21823211,          7785579841ull, 7607475409566672241ull},
		{22330859,          7593776864ull, 5630449305759171207ull},
		{11808917,         20308449831ull, 9058738039473012457ull},
		{20090969,         15531431134ull, 5034609389988515233ull},
		{20313967,         18216394609ull, 8291172543411688687ull},
		{20544481,         16259503442ull,15859685870849762975ull},
		{22217387,         20551559047ull,11995354231649723881ull},
		{10207999,         28364424832ull,15122069645900159367ull},
		{19964723,         34441477586ull, 9636073161914837921ull},
		{21145199,         30977655046ull, 1304857345634219175ull},
		{22030163,         43144178324ull, 4788416424359163737ull},
		{33562153,         45963786472ull, 2258783450670948535ull},
		{33693587,         66325700032ull,15262466751214122975ull},
		{11865241,         57210216387ull, 3082735332820781609ull},
		{21801929,         80355238912ull,15689518004012743009ull},
		{19951201,        109346652057ull,10819675441336938065ull},
		{20616781,      17534809723250ull,10329047311584913071ull},
		{20648443,       1221873279710ull, 2595613477835803991ull},
		{21250771,      12549422209078ull, 8612165677489771129ull},
		{21547787,        112416184026ull, 9015544550402598895ull},
		{21675733,        142220976614ull,11385509628023387489ull},
		{15714269,      14320762091913ull, 2773697912020767049ull},
		{19687561,       1996508583829ull, 7515490546312285159ull},
		{20152333,        365842230851ull, 2388855518206098663ull},
		{20510053,        261078947686ull,  465403687781705377ull},
		{20759821,     199835753775288ull,17079803649869853575ull},
		{20989043,        202355339943ull,15105677628487752455ull},
		{33713123,      18738454648009ull,16692905930976531153ull},
		{20542751,        412571049040ull,18170931828058363183ull},
		{20812849,     534505286298455ull, 2216600112648316881ull},
		{0,0,0ull}
	};

	/* Factors > 96 but <= 128 bits, with p > 64 bits - most of these are from my Jan 2003 runs of primes near 2^89: */
	static const struct testFac128x2 fac128x2[] =
	{
		{33554431ull,18446744073709551175ull,      30899672449023ull,18446744073303442655ull},
		{33554431ull,18446744073709551295ull,      85098334519295ull,18446744072895454529ull},
		{33554431ull,18446744073709551513ull,  430360347665235967ull,18446742752658555745ull},
		{33554431ull,18446744073709551513ull,  259661119604391935ull,18446743276643598623ull},
		{33554431ull,18446744073709551513ull,  293843670505881599ull,18446743171715500217ull},
		{33554431ull,18446744073709551567ull,   12593691025735679ull,18446744055318810857ull},
		{33554431ull,18446744073709551595ull,            67108863ull,18446744073709551575ull},
		{33554431ull,18446744073709551595ull,        631158865919ull,18446744073709156607ull},
		{33554432ull,                  89ull,      16384159383552ull,            43457455ull},
		{33554432ull,                 705ull, 7006880245689090048ull,     147219019329871ull},
		{33554432ull,                 741ull,        882615779328ull,            19491265ull},
		{33554432ull,                 741ull,     220386851553280ull,          4866917641ull},
		{33554432ull,                 767ull,     460834488713216ull,         10533930447ull},
		{33554432ull,                 837ull,    3613005073874944ull,         90124763455ull},
		{33554432ull,                 837ull,    1504315717976064ull,         37524469375ull},
		{33554432ull,                1059ull,            67108864ull,                2119ull},
		{33554432ull,                1275ull,          4898947072ull,              186151ull},
		/*{33554432ull,                1275ull,    1096248325046272ull,         41655201151ull},*/
		{33554432ull,                1337ull,      16333760626688ull,           650830209ull},
		{33554432ull,                1337ull,       1481763717120ull,            59041921ull},
		{33554432ull,                1521ull,   19343955296518144ull,        876848578633ull},
		{33554432ull,                1547ull,          2147483648ull,               99009ull},
		{33554432ull,                1917ull,    1124806032359424ull,         64261351945ull},
		/*{33554432ull,                1917ull,    2192593957945344ull,        125265199465ull},*/
		{33554432ull,                1917ull,         22749904896ull,             1299727ull},
		{33554432ull,                2097ull,      34829164871680ull,          2176665031ull},
		{33554432ull,                2585ull,      37028188127232ull,          2852614711ull},
		/*{33554432ull,                2585ull,    1096267786616832ull,         84455377711ull},*/
		{33554432ull,                2661ull,          2080374784ull,              164983ull},
		{33554432ull,                2675ull,           805306368ull,               64201ull},
		{33554432ull,                2729ull,      42057326395392ull,          3420544975ull},
		{33554432ull,                2729ull,       4876868255744ull,           396638319ull},
		{33554432ull,                2907ull,         22615687168ull,             1959319ull},
		{33554432ull,                3045ull,    1301649700159488ull,        118122200281ull},
		{33554432ull,                3045ull,         99321118720ull,             9013201ull},
		{33554432ull,                3155ull,      26011932557312ull,          2445806481ull},
		{33554432ull,                3159ull,        413994582016ull,            38975743ull},
		{33554432ull,                3507ull,        250450280448ull,            26176249ull},
		{33554432ull,                4155ull,   62084915730055168ull,       7687891270471ull},
		{33554432ull,                4451ull,          7851737088ull,             1041535ull},
		{33554432ull,                4485ull,        434328567808ull,            58053841ull},
		{33554432ull,                4745ull,         82678120448ull,            11691681ull},
		{33554432ull,                4745ull,         71672266752ull,            10135321ull},
		{33554432ull,                5121ull,        143814295552ull,            21948607ull},
		{33554432ull,                5247ull,            67108864ull,               10495ull},
		{33554432ull,                5411ull,     544322411823104ull,         87777631593ull},
		{33554432ull,                5499ull,    1739631741632512ull,        285096017935ull},
		{33554432ull,                5735ull,      10563740499968ull,          1805515641ull},
		{33554432ull,                5837ull,       7317214986240ull,          1272874591ull},
		{0ull,0ull,0ull,0ull}
	};

#endif	/* #ifndef fac_test_dat128_included */
