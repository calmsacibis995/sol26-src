/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1987, 1988 Microsoft Corporation	*/
/*	  All Rights Reserved	*/

/*	This Module contains Proprietary Information of Microsoft  */
/*	Corporation and should be treated as Confidential.	   */

/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.  All Rights Reserved.
 */

#pragma ident	"@(#)tables2.c	1.23	96/05/22 SMI"

#include <sys/types.h>
#include <sys/tss.h>
#include <sys/segment.h>
#ifdef _VPIX
#include <sys/v86.h>
#endif
#include <sys/cpuvar.h>

extern char df_stack;	/* top of stack for double fault handler */

extern struct cpu	*cpu[], cpus[];			/* per-CPU data */

/*
 *	386 Interrupt Descriptor Table
 */
extern div0trap(), dbgtrap(), nmiint(), brktrap(), ovflotrap(), boundstrap();
extern invoptrap(), ndptrap(), syserrtrap(), invaltrap(), invtsstrap();
extern segnptrap(), stktrap(), gptrap(), pftrap(), ndperr();
extern overrun(), resvtrap();
extern void _start(), sys_call(), sig_clean(), cmnint();
#ifdef _VPIX
extern v86gptrap();
#endif
extern achktrap();
extern fasttrap();

/* And about 10 million interrupt handlers.....  */
extern ivct32(), ivct33(), ivct34();
extern ivct35(), ivct36(), ivct37(), ivct38(), ivct39();
extern ivct40(), ivct41(), ivct42(), ivct43(), ivct44();
extern ivct45(), ivct46(), ivct47(), ivct48(), ivct49();
extern ivct50(), ivct51(), ivct52(), ivct53(), ivct54();
extern ivct55(), ivct56(), ivct57(), ivct58(), ivct59();
extern ivct60(), ivct61(), ivct62(), ivct63(), ivct64();
extern ivct65(), ivct66(), ivct67(), ivct68(), ivct69();
extern ivct70(), ivct71(), ivct72(), ivct73(), ivct74();
extern ivct75(), ivct76(), ivct77(), ivct78(), ivct79();
extern ivct80(), ivct81(), ivct82(), ivct83(), ivct84();
extern ivct85(), ivct86(), ivct87(), ivct88(), ivct89();
extern ivct90(), ivct91(), ivct92(), ivct93(), ivct94();
extern ivct95(), ivct96(), ivct97(), ivct98(), ivct99();
extern ivct100(), ivct101(), ivct102(), ivct103(), ivct104();
extern ivct105(), ivct106(), ivct107(), ivct108(), ivct109();
extern ivct110(), ivct111(), ivct112(), ivct113(), ivct114();
extern ivct115(), ivct116(), ivct117(), ivct118(), ivct119();
extern ivct120(), ivct121(), ivct122(), ivct123(), ivct124();
extern ivct125(), ivct126(), ivct127(), ivct128(), ivct129();
extern ivct130(), ivct131(), ivct132(), ivct133(), ivct134();
extern ivct135(), ivct136(), ivct137(), ivct138(), ivct139();
extern ivct140(), ivct141(), ivct142(), ivct143(), ivct144();
extern ivct145(), ivct146(), ivct147(), ivct148(), ivct149();
extern ivct150(), ivct151(), ivct152(), ivct153(), ivct154();
extern ivct155(), ivct156(), ivct157(), ivct158(), ivct159();
extern ivct160(), ivct161(), ivct162(), ivct163(), ivct164();
extern ivct165(), ivct166(), ivct167(), ivct168(), ivct169();
extern ivct170(), ivct171(), ivct172(), ivct173(), ivct174();
extern ivct175(), ivct176(), ivct177(), ivct178(), ivct179();
extern ivct180(), ivct181(), ivct182(), ivct183(), ivct184();
extern ivct185(), ivct186(), ivct187(), ivct188(), ivct189();
extern ivct190(), ivct191(), ivct192(), ivct193(), ivct194();
extern ivct195(), ivct196(), ivct197(), ivct198(), ivct199();
extern ivct200(), ivct201(), ivct202(), ivct203(), ivct204();
extern ivct205(), ivct206(), ivct207(), ivct208(), ivct209();
extern ivct210(), ivct211(), ivct212(), ivct213(), ivct214();
extern ivct215(), ivct216(), ivct217(), ivct218(), ivct219();
extern ivct220(), ivct221(), ivct222(), ivct223(), ivct224();
extern ivct225(), ivct226(), ivct227(), ivct228(), ivct229();
extern ivct230(), ivct231(), ivct232(), ivct233(), ivct234();
extern ivct235(), ivct236(), ivct237(), ivct238(), ivct239();
extern ivct240(), ivct241(), ivct242(), ivct243(), ivct244();
extern ivct245(), ivct246(), ivct247(), ivct248(), ivct249();
extern ivct250(), ivct251(), ivct252(), ivct253(), ivct254();
extern ivct255();

/* Fast system call routines */
extern void fast_null();
extern hrtime_t get_hrtime(void);
extern hrtime_t gethrvtime(void);
extern hrtime_t get_hrestime(void);

/* extern debugtrap(); */

extern struct tss386 ktss, ltss, dftss;

struct gate_desc idt[IDTSZ] = {
			MKKTRPG(div0trap),	/* 000 */
			MKKTRPG(dbgtrap), 	/* 001 */
			MKINTG(nmiint),		/* 002 */
			MKUTRPG(brktrap), 	/* 003 */
			MKUTRPG(ovflotrap),	/* 004 */
			MKKTRPG(boundstrap),	/* 005 */
			MKKTRPG(invoptrap),	/* 006 */
			MKINTG(ndptrap),	/* 007 */
			MKGATE(0, DFTSSSEL, GATE_KACC|GATE_TSS), /* 008 */
			MKINTG(overrun),	/* 009 */
			MKKTRPG(invtsstrap),	/* 010 */
			MKKTRPG(segnptrap),	/* 011 */
			MKKTRPG(stktrap),	/* 012 */
			MKKTRPG(gptrap),	/* 013 */
			MKINTG(pftrap),		/* 014 */
			MKKTRPG(resvtrap),	/* 015 */
			MKINTG(ndperr),		/* 016 */
			MKKTRPG(achktrap),	/* 017 */
			MKKTRPG(invaltrap),	/* 018 */
			MKKTRPG(invaltrap),	/* 019 */
			MKKTRPG(invaltrap),	/* 020 */
			MKKTRPG(invaltrap),	/* 021 */
			MKKTRPG(invaltrap),	/* 022 */
			MKKTRPG(invaltrap),	/* 023 */
			MKKTRPG(invaltrap),	/* 024 */
			MKKTRPG(invaltrap),	/* 025 */
			MKKTRPG(invaltrap),	/* 026 */
			MKKTRPG(invaltrap),	/* 027 */
			MKKTRPG(invaltrap),	/* 028 */
			MKKTRPG(invaltrap),	/* 029 */
			MKKTRPG(invaltrap),	/* 030 */
			MKKTRPG(invaltrap),	/* 031 */
			MKINTG(ivct32),		/* 032 */
			MKINTG(ivct33),		/* 033 */
			MKINTG(ivct34),		/* 034 */
			MKINTG(ivct35),		/* 035 */
			MKINTG(ivct36),		/* 036 */
			MKINTG(ivct37),		/* 037 */
			MKINTG(ivct38),		/* 038 */
			MKINTG(ivct39),		/* 039 */
			MKINTG(ivct40),		/* 040 */
			MKINTG(ivct41),		/* 041 */
			MKINTG(ivct42),		/* 042 */
			MKINTG(ivct43),		/* 043 */
			MKINTG(ivct44),		/* 044 */
			MKINTG(ivct45),		/* 045 */
			MKINTG(ivct46),		/* 046 */
			MKINTG(ivct47),		/* 047 */
			MKINTG(ivct48),		/* 048 */
			MKINTG(ivct49),		/* 049 */
			MKINTG(ivct50),		/* 050 */
			MKINTG(ivct51),		/* 051 */
			MKINTG(ivct52),		/* 052 */
			MKINTG(ivct53),		/* 053 */
			MKINTG(ivct54),		/* 054 */
			MKINTG(ivct55),		/* 055 */
			MKINTG(ivct56),		/* 056 */
			MKINTG(ivct57),		/* 057 */
			MKINTG(ivct58),		/* 058 */
			MKINTG(ivct59),		/* 059 */
			MKINTG(ivct60),		/* 060 */
			MKINTG(ivct61),		/* 061 */
			MKINTG(ivct62),		/* 062 */
			MKINTG(ivct63),		/* 063 */
			MKINTG(ivct64),		/* 064 */
			MKINTG(ivct65),		/* 065 */
			MKINTG(ivct66),		/* 066 */
			MKINTG(ivct67),		/* 067 */
			MKINTG(ivct68),		/* 068 */
			MKINTG(ivct69),		/* 069 */
			MKINTG(ivct70),		/* 070 */
			MKINTG(ivct71),		/* 071 */
			MKINTG(ivct72),		/* 072 */
			MKINTG(ivct73),		/* 073 */
			MKINTG(ivct74),		/* 074 */
			MKINTG(ivct75),		/* 075 */
			MKINTG(ivct76),		/* 076 */
			MKINTG(ivct77),		/* 077 */
			MKINTG(ivct78),		/* 078 */
			MKINTG(ivct79),		/* 079 */
			MKINTG(ivct80),		/* 080 */
			MKINTG(ivct81),		/* 081 */
			MKINTG(ivct82),		/* 082 */
			MKINTG(ivct83),		/* 083 */
			MKINTG(ivct84),		/* 084 */
			MKINTG(ivct85),		/* 085 */
			MKINTG(ivct86),		/* 086 */
			MKINTG(ivct87),		/* 087 */
			MKINTG(ivct88),		/* 088 */
			MKINTG(ivct89),		/* 089 */
			MKINTG(ivct90),		/* 090 */
			MKINTG(ivct91),		/* 091 */
			MKINTG(ivct92),		/* 092 */
			MKINTG(ivct93),		/* 093 */
			MKINTG(ivct94),		/* 094 */
			MKINTG(ivct95),		/* 095 */
			MKINTG(ivct96),		/* 096 */
			MKINTG(ivct97),		/* 097 */
			MKINTG(ivct98),		/* 098 */
			MKINTG(ivct99),		/* 099 */
			MKINTG(ivct100),	/* 100 */
			MKINTG(ivct101),	/* 101 */
			MKINTG(ivct102),	/* 102 */
			MKINTG(ivct103),	/* 103 */
			MKINTG(ivct104),	/* 104 */
			MKINTG(ivct105),	/* 105 */
			MKINTG(ivct106),	/* 106 */
			MKINTG(ivct107),	/* 107 */
			MKINTG(ivct108),	/* 108 */
			MKINTG(ivct109),	/* 109 */
			MKINTG(ivct110),	/* 110 */
			MKINTG(ivct111),	/* 111 */
			MKINTG(ivct112),	/* 112 */
			MKINTG(ivct113),	/* 113 */
			MKINTG(ivct114),	/* 114 */
			MKINTG(ivct115),	/* 115 */
			MKINTG(ivct116),	/* 116 */
			MKINTG(ivct117),	/* 117 */
			MKINTG(ivct118),	/* 118 */
			MKINTG(ivct119),	/* 119 */
			MKINTG(ivct120),	/* 120 */
			MKINTG(ivct121),	/* 121 */
			MKINTG(ivct122),	/* 122 */
			MKINTG(ivct123),	/* 123 */
			MKINTG(ivct124),	/* 124 */
			MKINTG(ivct125),	/* 125 */
			MKINTG(ivct126),	/* 126 */
			MKINTG(ivct127),	/* 127 */
			MKINTG(ivct128),	/* 128 */
			MKINTG(ivct129),	/* 129 */
			MKINTG(ivct130),	/* 130 */
			MKINTG(ivct131),	/* 131 */
			MKINTG(ivct132),	/* 132 */
			MKINTG(ivct133),	/* 133 */
			MKINTG(ivct134),	/* 134 */
			MKINTG(ivct135),	/* 135 */
			MKINTG(ivct136),	/* 136 */
			MKINTG(ivct137),	/* 137 */
			MKINTG(ivct138),	/* 138 */
			MKINTG(ivct139),	/* 139 */
			MKINTG(ivct140),	/* 140 */
			MKINTG(ivct141),	/* 141 */
			MKINTG(ivct142),	/* 142 */
			MKINTG(ivct143),	/* 143 */
			MKINTG(ivct144),	/* 144 */
			MKINTG(ivct145),	/* 145 */
			MKINTG(ivct146),	/* 146 */
			MKINTG(ivct147),	/* 147 */
			MKINTG(ivct148),	/* 148 */
			MKINTG(ivct149),	/* 149 */
			MKINTG(ivct150),	/* 150 */
			MKINTG(ivct151),	/* 151 */
			MKINTG(ivct152),	/* 152 */
			MKINTG(ivct153),	/* 153 */
			MKINTG(ivct154),	/* 154 */
			MKINTG(ivct155),	/* 155 */
			MKINTG(ivct156),	/* 156 */
			MKINTG(ivct157),	/* 157 */
			MKINTG(ivct158),	/* 158 */
			MKINTG(ivct159),	/* 159 */
			MKINTG(ivct160),	/* 160 */
			MKINTG(ivct161),	/* 161 */
			MKINTG(ivct162),	/* 162 */
			MKINTG(ivct163),	/* 163 */
			MKINTG(ivct164),	/* 164 */
			MKINTG(ivct165),	/* 165 */
			MKINTG(ivct166),	/* 166 */
			MKINTG(ivct167),	/* 167 */
			MKINTG(ivct168),	/* 168 */
			MKINTG(ivct169),	/* 169 */
			MKINTG(ivct170),	/* 170 */
			MKINTG(ivct171),	/* 171 */
			MKINTG(ivct172),	/* 172 */
			MKINTG(ivct173),	/* 173 */
			MKINTG(ivct174),	/* 174 */
			MKINTG(ivct175),	/* 175 */
			MKINTG(ivct176),	/* 176 */
			MKINTG(ivct177),	/* 177 */
			MKINTG(ivct178),	/* 178 */
			MKINTG(ivct179),	/* 179 */
			MKINTG(ivct180),	/* 180 */
			MKINTG(ivct181),	/* 181 */
			MKINTG(ivct182),	/* 182 */
			MKINTG(ivct183),	/* 183 */
			MKINTG(ivct184),	/* 184 */
			MKINTG(ivct185),	/* 185 */
			MKINTG(ivct186),	/* 186 */
			MKINTG(ivct187),	/* 187 */
			MKINTG(ivct188),	/* 188 */
			MKINTG(ivct189),	/* 189 */
			MKINTG(ivct190),	/* 190 */
			MKINTG(ivct191),	/* 191 */
			MKINTG(ivct192),	/* 192 */
			MKINTG(ivct193),	/* 193 */
			MKINTG(ivct194),	/* 194 */
			MKINTG(ivct195),	/* 195 */
			MKINTG(ivct196),	/* 196 */
			MKINTG(ivct197),	/* 197 */
			MKINTG(ivct198),	/* 198 */
			MKINTG(ivct199),	/* 199 */
			MKINTG(ivct200),	/* 200 */
			MKINTG(ivct201),	/* 201 */
			MKINTG(ivct202),	/* 202 */
			MKINTG(ivct203),	/* 203 */
			MKINTG(ivct204),	/* 204 */
			MKINTG(ivct205),	/* 205 */
			MKINTG(ivct206),	/* 206 */
			MKINTG(ivct207),	/* 207 */
			MKINTG(ivct208),	/* 208 */
			MKINTG(ivct209),	/* 209 */
			MKUINTG(fasttrap),	/* 210 */
			MKINTG(ivct211),	/* 211 */
			MKINTG(ivct212),	/* 212 */
			MKINTG(ivct213),	/* 213 */
			MKINTG(ivct214),	/* 214 */
			MKINTG(ivct215),	/* 215 */
			MKINTG(ivct216),	/* 216 */
			MKINTG(ivct217),	/* 217 */
			MKINTG(ivct218),	/* 218 */
			MKINTG(ivct219),	/* 219 */
			MKINTG(ivct220),	/* 220 */
			MKINTG(ivct221),	/* 221 */
			MKINTG(ivct222),	/* 222 */
			MKINTG(ivct223),	/* 223 */
			MKINTG(ivct224),	/* 224 */
			MKINTG(ivct225),	/* 225 */
			MKINTG(ivct226),	/* 226 */
			MKINTG(ivct227),	/* 227 */
			MKINTG(ivct228),	/* 228 */
			MKINTG(ivct229),	/* 229 */
			MKINTG(ivct230),	/* 230 */
			MKINTG(ivct231),	/* 231 */
			MKINTG(ivct232),	/* 232 */
			MKINTG(ivct233),	/* 233 */
			MKINTG(ivct234),	/* 234 */
			MKINTG(ivct235),	/* 235 */
			MKINTG(ivct236),	/* 236 */
			MKINTG(ivct237),	/* 237 */
			MKINTG(ivct238),	/* 238 */
			MKINTG(ivct239),	/* 239 */
			MKINTG(ivct240),	/* 240 */
			MKINTG(ivct241),	/* 241 */
			MKINTG(ivct242),	/* 242 */
			MKINTG(ivct243),	/* 243 */
			MKINTG(ivct244),	/* 244 */
			MKINTG(ivct245),	/* 245 */
			MKINTG(ivct246),	/* 246 */
			MKINTG(ivct247),	/* 247 */
			MKINTG(ivct248),	/* 248 */
			MKINTG(ivct249),	/* 249 */
			MKINTG(ivct250),	/* 250 */
			MKINTG(ivct251),	/* 251 */
			MKINTG(ivct252),	/* 252 */
			MKINTG(ivct253),	/* 253 */
			MKINTG(ivct254),	/* 254 */
#ifdef SYSTEMPRO
/*
 * The SystemPro has a problem that causes spurious interrupts on the
 * second processor. Until something can be done about it, treat the
 * spurious interrupt as a cross processor interrupt.
 */
			MKINTG(ivctM0S5),	/* 077 */
#else
/*
 * We will extend this interface so that anyone can install intpt
 * at any vector
 */
			MKINTG(ivct255),	/* 255 */
#endif
};

/*
 * Permanent kludge. This is a second copy of the IDT with the some
 * vectors changed to be task gates to the user TSS. It is loaded
 * into the IDT register when we go into user mode for dual mode
 * processes only. The vector that has a task gate to the user
 * TSS is: the invalid opcode exception (vector 6).
 */
struct gate_desc idt2[IDTSZ] = {
			MKKTRPG(div0trap),	/* 000 */
			MKKTRPG(dbgtrap),	/* 001 */
			MKINTG(nmiint),		/* 002 */
			MKUTRPG(brktrap),	/* 003 */
			MKUTRPG(ovflotrap),	/* 004 */
			MKKTRPG(boundstrap),	/* 005 */
			/* MKKTRPG(invoptrap), */    /* 006 */
			MKGATE(0, UTSSSEL, GATE_KACC|GATE_TSS), /* 006 */
			MKINTG(ndptrap),	/* 007 */
			MKGATE(0, DFTSSSEL, GATE_KACC|GATE_TSS), /* 008 */
			MKINTG(overrun),	/* 009 */
			MKKTRPG(invtsstrap),	/* 010 */
			MKKTRPG(segnptrap),	/* 011 */
			MKKTRPG(stktrap),	/* 012 */
#ifdef _VPIX
			MKINTG(v86gptrap),	/* 013 */
#else
			MKKTRPG(gptrap),	/* 013 */
#endif
			MKKTRPG(pftrap),	/* 014 */
			MKKTRPG(resvtrap),	/* 015 */
			MKINTG(ndperr),		/* 016 */
			MKKTRPG(achktrap),	/* 017 */
			MKKTRPG(invaltrap),	/* 018 */
			MKKTRPG(invaltrap),	/* 019 */
			MKKTRPG(invaltrap),	/* 020 */
			MKKTRPG(invaltrap),	/* 021 */
			MKKTRPG(invaltrap),	/* 022 */
			MKKTRPG(invaltrap),	/* 023 */
			MKKTRPG(invaltrap),	/* 024 */
			MKKTRPG(invaltrap),	/* 025 */
			MKKTRPG(invaltrap),	/* 026 */
			MKKTRPG(invaltrap),	/* 027 */
			MKKTRPG(invaltrap),	/* 028 */
			MKKTRPG(invaltrap),	/* 029 */
			MKKTRPG(invaltrap),	/* 030 */
			MKKTRPG(invaltrap),	/* 031 */
			MKINTG(ivct32),		/* 032 */
			MKINTG(ivct33),		/* 033 */
			MKINTG(ivct34),		/* 034 */
			MKINTG(ivct35),		/* 035 */
			MKINTG(ivct36),		/* 036 */
			MKINTG(ivct37),		/* 037 */
			MKINTG(ivct38),		/* 038 */
			MKINTG(ivct39),		/* 039 */
			MKINTG(ivct40),		/* 040 */
			MKINTG(ivct41),		/* 041 */
			MKINTG(ivct42),		/* 042 */
			MKINTG(ivct43),		/* 043 */
			MKINTG(ivct44),		/* 044 */
			MKINTG(ivct45),		/* 045 */
			MKINTG(ivct46),		/* 046 */
			MKINTG(ivct47),		/* 047 */
			MKINTG(ivct48),		/* 048 */
			MKINTG(ivct49),		/* 049 */
			MKINTG(ivct50),		/* 050 */
			MKINTG(ivct51),		/* 051 */
			MKINTG(ivct52),		/* 052 */
			MKINTG(ivct53),		/* 053 */
			MKINTG(ivct54),		/* 054 */
			MKINTG(ivct55),		/* 055 */
			MKINTG(ivct56),		/* 056 */
			MKINTG(ivct57),		/* 057 */
			MKINTG(ivct58),		/* 058 */
			MKINTG(ivct59),		/* 059 */
			MKINTG(ivct60),		/* 060 */
			MKINTG(ivct61),		/* 061 */
			MKINTG(ivct62),		/* 062 */
			MKINTG(ivct63),		/* 063 */
			MKINTG(ivct64),		/* 064 */
			MKINTG(ivct65),		/* 065 */
			MKINTG(ivct66),		/* 066 */
			MKINTG(ivct67),		/* 067 */
			MKINTG(ivct68),		/* 068 */
			MKINTG(ivct69),		/* 069 */
			MKINTG(ivct70),		/* 070 */
			MKINTG(ivct71),		/* 071 */
			MKINTG(ivct72),		/* 072 */
			MKINTG(ivct73),		/* 073 */
			MKINTG(ivct74),		/* 074 */
			MKINTG(ivct75),		/* 075 */
			MKINTG(ivct76),		/* 076 */
			MKINTG(ivct77),		/* 077 */
			MKINTG(ivct78),		/* 078 */
			MKINTG(ivct79),		/* 079 */
			MKINTG(ivct80),		/* 080 */
			MKINTG(ivct81),		/* 081 */
			MKINTG(ivct82),		/* 082 */
			MKINTG(ivct83),		/* 083 */
			MKINTG(ivct84),		/* 084 */
			MKINTG(ivct85),		/* 085 */
			MKINTG(ivct86),		/* 086 */
			MKINTG(ivct87),		/* 087 */
			MKINTG(ivct88),		/* 088 */
			MKINTG(ivct89),		/* 089 */
			MKINTG(ivct90),		/* 090 */
			MKINTG(ivct91),		/* 091 */
			MKINTG(ivct92),		/* 092 */
			MKINTG(ivct93),		/* 093 */
			MKINTG(ivct94),		/* 094 */
			MKINTG(ivct95),		/* 095 */
			MKINTG(ivct96),		/* 096 */
			MKINTG(ivct97),		/* 097 */
			MKINTG(ivct98),		/* 098 */
			MKINTG(ivct99),		/* 099 */
			MKINTG(ivct100),	/* 100 */
			MKINTG(ivct101),	/* 101 */
			MKINTG(ivct102),	/* 102 */
			MKINTG(ivct103),	/* 103 */
			MKINTG(ivct104),	/* 104 */
			MKINTG(ivct105),	/* 105 */
			MKINTG(ivct106),	/* 106 */
			MKINTG(ivct107),	/* 107 */
			MKINTG(ivct108),	/* 108 */
			MKINTG(ivct109),	/* 109 */
			MKINTG(ivct110),	/* 110 */
			MKINTG(ivct111),	/* 111 */
			MKINTG(ivct112),	/* 112 */
			MKINTG(ivct113),	/* 113 */
			MKINTG(ivct114),	/* 114 */
			MKINTG(ivct115),	/* 115 */
			MKINTG(ivct116),	/* 116 */
			MKINTG(ivct117),	/* 117 */
			MKINTG(ivct118),	/* 118 */
			MKINTG(ivct119),	/* 119 */
			MKINTG(ivct120),	/* 120 */
			MKINTG(ivct121),	/* 121 */
			MKINTG(ivct122),	/* 122 */
			MKINTG(ivct123),	/* 123 */
			MKINTG(ivct124),	/* 124 */
			MKINTG(ivct125),	/* 125 */
			MKINTG(ivct126),	/* 126 */
			MKINTG(ivct127),	/* 127 */
			MKINTG(ivct128),	/* 128 */
			MKINTG(ivct129),	/* 129 */
			MKINTG(ivct130),	/* 130 */
			MKINTG(ivct131),	/* 131 */
			MKINTG(ivct132),	/* 132 */
			MKINTG(ivct133),	/* 133 */
			MKINTG(ivct134),	/* 134 */
			MKINTG(ivct135),	/* 135 */
			MKINTG(ivct136),	/* 136 */
			MKINTG(ivct137),	/* 137 */
			MKINTG(ivct138),	/* 138 */
			MKINTG(ivct139),	/* 139 */
			MKINTG(ivct140),	/* 140 */
			MKINTG(ivct141),	/* 141 */
			MKINTG(ivct142),	/* 142 */
			MKINTG(ivct143),	/* 143 */
			MKINTG(ivct144),	/* 144 */
			MKINTG(ivct145),	/* 145 */
			MKINTG(ivct146),	/* 146 */
			MKINTG(ivct147),	/* 147 */
			MKINTG(ivct148),	/* 148 */
			MKINTG(ivct149),	/* 149 */
			MKINTG(ivct150),	/* 150 */
			MKINTG(ivct151),	/* 151 */
			MKINTG(ivct152),	/* 152 */
			MKINTG(ivct153),	/* 153 */
			MKINTG(ivct154),	/* 154 */
			MKINTG(ivct155),	/* 155 */
			MKINTG(ivct156),	/* 156 */
			MKINTG(ivct157),	/* 157 */
			MKINTG(ivct158),	/* 158 */
			MKINTG(ivct159),	/* 159 */
			MKINTG(ivct160),	/* 160 */
			MKINTG(ivct161),	/* 161 */
			MKINTG(ivct162),	/* 162 */
			MKINTG(ivct163),	/* 163 */
			MKINTG(ivct164),	/* 164 */
			MKINTG(ivct165),	/* 165 */
			MKINTG(ivct166),	/* 166 */
			MKINTG(ivct167),	/* 167 */
			MKINTG(ivct168),	/* 168 */
			MKINTG(ivct169),	/* 169 */
			MKINTG(ivct170),	/* 170 */
			MKINTG(ivct171),	/* 171 */
			MKINTG(ivct172),	/* 172 */
			MKINTG(ivct173),	/* 173 */
			MKINTG(ivct174),	/* 174 */
			MKINTG(ivct175),	/* 175 */
			MKINTG(ivct176),	/* 176 */
			MKINTG(ivct177),	/* 177 */
			MKINTG(ivct178),	/* 178 */
			MKINTG(ivct179),	/* 179 */
			MKINTG(ivct180),	/* 180 */
			MKINTG(ivct181),	/* 181 */
			MKINTG(ivct182),	/* 182 */
			MKINTG(ivct183),	/* 183 */
			MKINTG(ivct184),	/* 184 */
			MKINTG(ivct185),	/* 185 */
			MKINTG(ivct186),	/* 186 */
			MKINTG(ivct187),	/* 187 */
			MKINTG(ivct188),	/* 188 */
			MKINTG(ivct189),	/* 189 */
			MKINTG(ivct190),	/* 190 */
			MKINTG(ivct191),	/* 191 */
			MKINTG(ivct192),	/* 192 */
			MKINTG(ivct193),	/* 193 */
			MKINTG(ivct194),	/* 194 */
			MKINTG(ivct195),	/* 195 */
			MKINTG(ivct196),	/* 196 */
			MKINTG(ivct197),	/* 197 */
			MKINTG(ivct198),	/* 198 */
			MKINTG(ivct199),	/* 199 */
			MKINTG(ivct200),	/* 200 */
			MKINTG(ivct201),	/* 201 */
			MKINTG(ivct202),	/* 202 */
			MKINTG(ivct203),	/* 203 */
			MKINTG(ivct204),	/* 204 */
			MKINTG(ivct205),	/* 205 */
			MKINTG(ivct206),	/* 206 */
			MKINTG(ivct207),	/* 207 */
			MKINTG(ivct208),	/* 208 */
			MKINTG(ivct209),	/* 209 */
			MKUINTG(fasttrap),	/* 210 */
			MKINTG(ivct211),	/* 211 */
			MKINTG(ivct212),	/* 212 */
			MKINTG(ivct213),	/* 213 */
			MKINTG(ivct214),	/* 214 */
			MKINTG(ivct215),	/* 215 */
			MKINTG(ivct216),	/* 216 */
			MKINTG(ivct217),	/* 217 */
			MKINTG(ivct218),	/* 218 */
			MKINTG(ivct219),	/* 219 */
			MKINTG(ivct210),	/* 220 */
			MKINTG(ivct221),	/* 221 */
			MKINTG(ivct222),	/* 222 */
			MKINTG(ivct223),	/* 223 */
			MKINTG(ivct224),	/* 224 */
			MKINTG(ivct225),	/* 225 */
			MKINTG(ivct226),	/* 226 */
			MKINTG(ivct227),	/* 227 */
			MKINTG(ivct228),	/* 228 */
			MKINTG(ivct229),	/* 229 */
			MKINTG(ivct230),	/* 230 */
			MKINTG(ivct231),	/* 231 */
			MKINTG(ivct232),	/* 232 */
			MKINTG(ivct233),	/* 233 */
			MKINTG(ivct234),	/* 234 */
			MKINTG(ivct235),	/* 235 */
			MKINTG(ivct236),	/* 236 */
			MKINTG(ivct237),	/* 237 */
			MKINTG(ivct238),	/* 238 */
			MKINTG(ivct239),	/* 239 */
			MKINTG(ivct240),	/* 240 */
			MKINTG(ivct241),	/* 241 */
			MKINTG(ivct242),	/* 242 */
			MKINTG(ivct243),	/* 243 */
			MKINTG(ivct244),	/* 244 */
			MKINTG(ivct245),	/* 245 */
			MKINTG(ivct246),	/* 246 */
			MKINTG(ivct247),	/* 247 */
			MKINTG(ivct248),	/* 248 */
			MKINTG(ivct249),	/* 249 */
			MKINTG(ivct250),	/* 250 */
			MKINTG(ivct251),	/* 251 */
			MKINTG(ivct252),	/* 252 */
			MKINTG(ivct253),	/* 253 */
			MKINTG(ivct254),	/* 254 */
#ifdef SYSTEMPRO
/*
 * The SystemPro has a problem that causes spurious interrupts on the
 * second processor. Until something can be done about it, treat the
 * spurious interrupt as a cross processor interrupt.
 */
			MKINTG(ivctM0S5),	/* 077 */
#else
/*
 * We will extend this interface so that anyone can install intpt
 * at any vector
 */
			MKINTG(ivct191),	/* 255 */
#endif
};

/*
 *	Local Descriptor Table used by default
 *
 * Initialization of this table depends on these definitions not changing
 *	USER_SCALL	0x07	call gate for system calls
 *	USER_SIGCALL	0x0F	call gate for sigreturn
 *	USER_CS		0x17	user's code segment
 *	USER_DS		0x1F	user's data segment
 *	USER_ALTSCALL	0x27	alternate call gate for system calls
 *	USER_ALTSIGCALL	0x2F	alternate call gate for sigreturn
 */

struct seg_desc ldt_default[MINLDTSZ] = {
	MKDSCR(0L, 0L, 0, 0),	/* filled in by _start with scall_dscr */
	MKDSCR(0L, 0L, 0, 0),	/* filled in by _start with sigret_dscr */
	MKDSCR(0L, (u_long)0xffffffff>>MMU_STD_PAGESHIFT, UTEXT_ACC1,
	    TEXT_ACC2),
	MKDSCR(0L, (u_long)0xffffffff>>MMU_STD_PAGESHIFT, UDATA_ACC1,
	    DATA_ACC2),
	MKDSCR(0L, 0L, 0, 0),	/* filled in by _start with scall_dscr */
	MKDSCR(0L, 0L, 0, 0),	/* filled in by _start with sigret_dscr */
};

struct seg_desc default_ldt_desc; /* segment descriptor for default LDT */

/*
 *	Global Descriptor Table
 */
struct seg_desc gdt[GDTSZ] = {
	MKDSCR(0L, 0L, 0, 0),		/* 00 */
	MKDSCR(0L, 0L, 0, 0),		/* 01 - copied in from boot */
	MKDSCR(0L, 0L, 0, 0),		/* 02 - copied in from boot */
	MKDSCR(0L, 0L, 0, 0),		/* 03 - copied in from boot */
	MKDSCR(0L, 0L, 0, 0),		/* 04 - copied in from boot */
	MKDSCR((char *)&gdt[0], GDTSZ * sizeof (struct seg_desc) - 1,
	    0x93, 0),			/* 05 */
	MKDSCR((char *)&idt[0], IDTSZ * sizeof (struct gate_desc) - 1,
	    0x92, 0),			/* 06 */
	MKDSCR(0L, 0L, 0, 0),		/* 07 */
	MKDSCR(0L, 0L, 0, 0),		/* 08 */
	MKDSCR(0L, 0L, 0, 0),		/* 09 */
	MKDSCR(0L, 0L, 0, 0),		/* 10 */
	MKDSCR(0L, 0L, 0, 0),		/* 11 */
	MKDSCR(0L, 0L, 0, 0),		/* 12 */
	MKDSCR(0L, 0L, 0, 0),		/* 13 */
	MKDSCR(0L, 0L, 0, 0),		/* 14 */
	MKDSCR(0L, 0L, 0, 0),		/* 15 */
	MKDSCR(0L, 0L, 0, 0),		/* 16 */
	MKDSCR(0L, 0L, 0, 0),		/* 17 */
	MKDSCR(0L, 0L, 0, 0),		/* 18 */
	MKDSCR(0L, 0L, 0, 0),		/* 19 */
	MKDSCR(0L, 0L, 0, 0),		/* 20 */
	MKDSCR(0L, 0L, 0, 0),		/* 21 */
	MKDSCR(0L, 0L, 0, 0),		/* 22 */
	MKDSCR(0L, 0L, 0, 0),		/* 23 */
	MKDSCR(0L, 0L, 0, 0),		/* 24 */
	MKDSCR(0L, 0L, 0, 0),		/* 25 */
	MKDSCR(0L, 0L, 0, 0),		/* 26 */
	MKDSCR(0L, 0L, 0, 0),		/* 27 */
	MKDSCR(0L, 0L, 0, 0),		/* 28 */
	MKDSCR(0L, 0L, 0, 0),		/* 29 */
	MKDSCR(0L, 0L, 0, 0),		/* 30 */
	MKDSCR(0L, 0L, 0, 0),		/* 31 */
	MKDSCR(0L, 0L, 0, 0),		/* 32 */
	MKDSCR(0L, 0L, 0, 0),		/* 33 */
	MKDSCR(0L, 0L, 0, 0),		/* 34 */
	MKDSCR(0L, 0L, 0, 0),		/* 35 */
	MKDSCR(0L, 0L, 0, 0),		/* 36 */
	MKDSCR(0L, 0L, 0, 0),		/* 37 */
	MKDSCR(0L, 0L, 0, 0),		/* 38 */
	MKDSCR(0L, 0L, 0, 0),		/* 39 */
	MKDSCR((char *)&ldt_default, MINLDTSZ * sizeof (struct seg_desc) - 1,
	    LDT_KACC1, LDT_ACC2),	/* 40 */
	MKDSCR((char *)&ktss, sizeof (struct tss386) - 1, TSS3_KACC1,
	    TSS_ACC2),			/* 41 */
	MKDSCR((char *)&ktss, sizeof (struct tss386) - 1, TSS3_KACC1,
	    TSS_ACC2),			/* 42 */
	MKDSCR(0L, 0xFFFFF, KTEXT_ACC1, TEXT_ACC2),		/* 43 */
	MKDSCR(0L, 0xFFFFF, KDATA_ACC1, DATA_ACC2),		/* 44 */
	MKDSCR((char *)&dftss, sizeof (struct tss386) - 1, TSS3_KACC1,
	    TSS_ACC2),			/* 45 */
	MKDSCR((char *)&ltss, sizeof (struct tss386) - 1, TSS3_KACC1,
	    TSS_ACC2),			/* 46 */
	MKDSCR(0L, 0L, 0, 0),		/* 47 */
	MKDSCR(0L, 0L, 0, 0),		/* 48 */
#ifdef _VPIX
	MKDSCR(XTSSADDR, 0L, TSS3_KACC1, TSS_ACC2), /* 49 */
#else
	MKDSCR(0L, 0L, 0, 0),		/* 49 */
#endif
	MKDSCR(0L, 0xFFFFF, KTEXT_ACC1, TEXT_ACC2_S),	/* 50 */ /* fp emul */
	MKDSCR(0L, 0L, 0, 0),		/* 51 */
	MKDSCR(0L, 0L, 0, 0),		/* 52 */
	/* 53 - entry to access the address of the per-cpu data using %fs */
	MKDSCR((char *)&cpu[0], sizeof (struct cpu *) - 1, KDATA_ACC1,
	    DATA_ACC2_S),
	/* 54 - entry to access the members of the per-cpu data structure */
	/* using %gs */
	MKDSCR((char *)&cpus[0], sizeof (struct cpu) - 1, KDATA_ACC1,
	    DATA_ACC2_S),
						/* other entries all zero */
};

struct tss386 ltss = {0};	/* uprt.s dumps stuff here. It's never read */

struct tss386 ktss = {
			0L,
	/* the next field (tss_esp0) is filled in dynamically in resume() */
			(unsigned long)(&df_stack+0xFFC),
			(unsigned long)KDSSEL,
			(unsigned long)(&df_stack+0xFFC),
			(unsigned long)KDSSEL,
			(unsigned long)(&df_stack+0xFFC),
			(unsigned long)KDSSEL,
	/* the next field (tss_cr3) is filled if and when needed */
			(unsigned long)0,		/* cr3 */
			(unsigned long) _start,
			0L,				/* flags */
			0L,
			0L,
			0L,
			0L,
			(unsigned long)(&df_stack+0xFFC),
			0L,
			0L,
			0L,
			(unsigned long)KDSSEL,
			(unsigned long)KCSSEL,
			(unsigned long)KDSSEL,
			(unsigned long)KDSSEL,
			(unsigned long)KFSSEL,
			(unsigned long)KGSSEL,
			(unsigned long)LDTSEL,
			(unsigned long)0xDFFF0000,
};

struct tss386 dftss = {
			0L,
			(unsigned long)(&df_stack+0xFFC),
			(unsigned long)KDSSEL,
			0L,
			0L,
			0L,
			0L,
	/* the next field (tss_cr3) is filled during initialization */
			(unsigned long)0,		/* cr3 */
			(unsigned long) syserrtrap,
			0L,				/* flags */
			0L,
			0L,
			0L,
			0L,
			(unsigned long)(&df_stack+0xFFC),
			0L,
			0L,
			0L,
			(unsigned long)KDSSEL,
			(unsigned long)KCSSEL,
			(unsigned long)KDSSEL,
			(unsigned long)KDSSEL,
			(unsigned long)KFSSEL,
			(unsigned long)KGSSEL,
			0L,				/* LDT selector */
			(unsigned long)0xDFFF0000,
};

struct gate_desc scall_dscr = {
		(ulong)sys_call, KCSSEL, 1, GATE_UACC|GATE_386CALL
};

struct gate_desc sigret_dscr = {
		(ulong)sig_clean, KCSSEL, 1, GATE_UACC|GATE_386CALL
};

void (*(fasttable[]))() = {
	fast_null,			/* T_FNULL routine */
	fast_null,			/* T_FGETFP routine (initially null) */
	fast_null,			/* T_FSETFP routine (initially null) */
	(void (*)())get_hrtime,		/* T_GETHRTIME */
	(void (*)())gethrvtime,		/* T_GETHRVTIME */
	(void (*)())get_hrestime	/* T_GETHRESTIME */
};
