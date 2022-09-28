/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#if !defined(PPCDIS_OPCODES_H)
#define	PPCDIS_OPCODES_H

#pragma ident	"@(#)ppcdis_opcodes.h	1.5	95/10/20 SMI"
#pragma ident "@(#)ppcdis_opcodes.h 1.3	93/10/18 vla"

/*
 * PowerPC Disassembler - Primary/Secondary Opcodes
 *
 * This file contains all of the primary and secondary opcodes used by the
 * Power PC architecture instruction set.
 */

#define	PPC_OP_3		3		/* twi		*/
#define	PPC_OP_7		7		/* mulli	*/
#define	PPC_OP_8		8		/* subfic	*/
#define	PPC_OP_9		9		/* dozi (POWER/601 only) */
#define	PPC_OP_10		10		/* cmpli	*/
#define	PPC_OP_11		11		/* cmpi		*/
#define	PPC_OP_12		12		/* addic	*/
#define	PPC_OP_13		13		/* addic.	*/
#define	PPC_OP_14		14		/* addi.	*/
#define	PPC_OP_15		15		/* addis	*/
#define	PPC_OP_16		16		/* bcx		*/
#define	PPC_OP_17		17		/* sc		*/
#define	PPC_OP_18		18		/* bx		*/
#define	PPC_OP_19		19		/* op 19	*/
#define	PPC_OP_20		20		/* rlwimix	*/
#define	PPC_OP_21		21		/* rlwinmx	*/
#define	PPC_OP_22		22		/* rlmix (POWER/601 only) */
#define	PPC_OP_23		23		/* rlwnmx	*/
#define	PPC_OP_24		24		/* ori		*/
#define	PPC_OP_25		25		/* oris		*/
#define	PPC_OP_26		26		/* xori		*/
#define	PPC_OP_27		27		/* xoris	*/
#define	PPC_OP_28		28		/* andi.	*/
#define	PPC_OP_29		29		/* andis.	*/
#define	PPC_OP_31		31		/* op 31	*/
#define	PPC_OP_32		32		/* lwz		*/
#define	PPC_OP_33		33		/* lwzu		*/
#define	PPC_OP_34		34		/* lbz		*/
#define	PPC_OP_35		35		/* lbzu		*/
#define	PPC_OP_36		36		/* stw		*/
#define	PPC_OP_37		37		/* stwu		*/
#define	PPC_OP_38		38		/* stb		*/
#define	PPC_OP_39		39		/* stbu		*/
#define	PPC_OP_40		40		/* lhz		*/
#define	PPC_OP_41		41		/* lhzu		*/
#define	PPC_OP_42		42		/* lha		*/
#define	PPC_OP_43		43		/* lhau		*/
#define	PPC_OP_44		44		/* sth		*/
#define	PPC_OP_45		45		/* sthu		*/
#define	PPC_OP_46		46		/* lmw		*/
#define	PPC_OP_47		47		/* stmw		*/
#define	PPC_OP_48		48		/* lfs		*/
#define	PPC_OP_49		49		/* lfsu		*/
#define	PPC_OP_50		50		/* lfd		*/
#define	PPC_OP_51		51		/* lfdu		*/
#define	PPC_OP_52		52		/* stfs		*/
#define	PPC_OP_53		53		/* stfsu	*/
#define	PPC_OP_54		54		/* stfd		*/
#define	PPC_OP_55		55		/* stfdu	*/
#define	PPC_OP_59		59		/* op 59	*/
#define	PPC_OP_63		63		/* op 63	*/

/*
 * Opcodes 19, 31, 59, and 63 have additional extended opcodes described below
 */

#define	PPC_OP19_0		0		/* mcrf		*/
#define	PPC_OP19_16		16		/* bclrx	*/
#define	PPC_OP19_33		33		/* crnor	*/
#define	PPC_OP19_50		50		/* rfi		*/
#define	PPC_OP19_129		129		/* crandc	*/
#define	PPC_OP19_150		150		/* isync	*/
#define	PPC_OP19_193		193		/* crxor	*/
#define	PPC_OP19_225		225		/* crnand	*/
#define	PPC_OP19_257		257		/* crand	*/
#define	PPC_OP19_289		289		/* creqv	*/
#define	PPC_OP19_417		417		/* crorc	*/
#define	PPC_OP19_449		449		/* cror		*/
#define	PPC_OP19_528		528		/* bcctrx	*/


#define	PPC_OP31_0		0		/* cmp		*/
#define	PPC_OP31_4		4		/* tw		*/
#define	PPC_OP31_8		8		/* subfcx	*/
#define	PPC_OP31_10		10		/* addcx	*/
#define	PPC_OP31_11		11		/* mulhwux	*/
#define	PPC_OP31_19		19		/* mfcr		*/
#define	PPC_OP31_20		20		/* lwarx	*/
#define	PPC_OP31_23		23		/* lwzx		*/
#define	PPC_OP31_24		24		/* slwx		*/
#define	PPC_OP31_26		26		/* cntlzwx	*/
#define	PPC_OP31_28		28		/* andx		*/
#define	PPC_OP31_29		29		/* maskgx (POWER/601 only) */
#define	PPC_OP31_32		32		/* cmpl		*/
#define	PPC_OP31_40		40		/* subfx	*/
#define	PPC_OP31_54		54		/* dcbst	*/
#define	PPC_OP31_55		55		/* lwzux	*/
#define	PPC_OP31_60		60		/* andcx	*/
#define	PPC_OP31_75		75		/* mulhw	*/
#define	PPC_OP31_83		83		/* mfmsr	*/
#define	PPC_OP31_86		86		/* dcbf		*/
#define	PPC_OP31_87		87		/* lbzx		*/
#define	PPC_OP31_104		104		/* negx		*/
#define	PPC_OP31_107		107		/* mulx	(POWER/601 only) */
#define	PPC_OP31_115		115		/* mfpmr	*/
#define	PPC_OP31_119		119		/* lbzux	*/
#define	PPC_OP31_124		124		/* norx		*/
#define	PPC_OP31_136		136		/* subfex	*/
#define	PPC_OP31_138		138		/* addex	*/
#define	PPC_OP31_144		144		/* mtcrf	*/
#define	PPC_OP31_146		146		/* mtmsr	*/
#define	PPC_OP31_150		150		/* stwcx.	*/
#define	PPC_OP31_151		151		/* stwx		*/
#define	PPC_OP31_152		152		/* slqx	(POWER/601 only) */
#define	PPC_OP31_153		153		/* slex	(POWER/601 only) */
#define	PPC_OP31_178		178		/* mtpmr	*/
#define	PPC_OP31_183		183		/* stwux	*/
#define	PPC_OP31_184		184		/* sliqx (POWER/601 only) */
#define	PPC_OP31_200		200		/* subfzex	*/
#define	PPC_OP31_202		202		/* addzex	*/
#define	PPC_OP31_210		210		/* mtsr		*/
#define	PPC_OP31_215		215		/* stbx		*/
#define	PPC_OP31_216		216		/* sllqx (POWER/601 only) */
#define	PPC_OP31_217		217		/* sleqx (POWER/601 only) */
#define	PPC_OP31_232		232		/* subfmex	*/
#define	PPC_OP31_234		234		/* addmex	*/
#define	PPC_OP31_235		235		/* mullwx	*/
#define	PPC_OP31_242		242		/* mtsrin	*/
#define	PPC_OP31_246		246		/* dcbtst	*/
#define	PPC_OP31_247		247		/* stbux	*/
#define	PPC_OP31_248		248		/* slliqx (POWER/601 only) */
#define	PPC_OP31_264		264		/* dozx	(POWER/601 only) */
#define	PPC_OP31_266		266		/* addx		*/
#define	PPC_OP31_277		277		/* lscbxx (POWER/601 only) */
#define	PPC_OP31_278		278		/* dcbt		*/
#define	PPC_OP31_279		279		/* lhzx		*/
#define	PPC_OP31_284		284		/* eqvx		*/
#define	PPC_OP31_306		306		/* tlbie	*/
#define	PPC_OP31_310		310		/* eciwx	*/
#define	PPC_OP31_311		311		/* lhzux	*/
#define	PPC_OP31_316		316		/* xorx		*/
#define	PPC_OP31_331		331		/* divx	(POWER/601 only) */
#define	PPC_OP31_339		339		/* mfspr	*/
#define	PPC_OP31_343		343		/* lhax		*/
#define	PPC_OP31_360		360		/* absx	(POWER/601 only) */
#define	PPC_OP31_363		363		/* divsx (POWER/601 only) */
#define	PPC_OP31_371		371		/* mftb		*/
#define	PPC_OP31_375		375		/* lhaux	*/
#define	PPC_OP31_407		407		/* sthx		*/
#define	PPC_OP31_412		412		/* orcx		*/
#define	PPC_OP31_434		434		/* slbiz	*/
#define	PPC_OP31_438		438		/* ecowx	*/
#define	PPC_OP31_439		439		/* sthux	*/
#define	PPC_OP31_444		444		/* orx		*/
#define	PPC_OP31_459		459		/* divwux	*/
#define	PPC_OP31_466		466		/* slbiex	*/
#define	PPC_OP31_467		467		/* mtspr	*/
#define	PPC_OP31_470		470		/* dcbi		*/
#define	PPC_OP31_476		476		/* nandx	*/
#define	PPC_OP31_488		488		/* nabs	(POWER/601 only) */
#define	PPC_OP31_491		491		/* divwx	*/
#define	PPC_OP31_498		498		/* slbia	*/
#define	PPC_OP31_512		512		/* mcrxr	*/
#define	PPC_OP31_531		531		/* clcs	(POWER/601 only) */
#define	PPC_OP31_533		533		/* lswx		*/
#define	PPC_OP31_534		534		/* lwbrx	*/
#define	PPC_OP31_535		535		/* lfsx		*/
#define	PPC_OP31_536		536		/* srwx		*/
#define	PPC_OP31_537		537		/* rribx (POWER/601 only) */
#define	PPC_OP31_541		541		/* maskirx (POWER/601 only) */
#define	PPC_OP31_566		566		/* tlbsync	*/
#define	PPC_OP31_567		567		/* lfsux	*/
#define	PPC_OP31_595		595		/* mfsr		*/
#define	PPC_OP31_597		597		/* lswi		*/
#define	PPC_OP31_598		598		/* sync		*/
#define	PPC_OP31_599		599		/* lfdx		*/
#define	PPC_OP31_631		631		/* lfdux	*/
#define	PPC_OP31_659		659		/* mfsrin	*/
#define	PPC_OP31_661		661		/* stswx	*/
#define	PPC_OP31_662		662		/* stwbrx	*/
#define	PPC_OP31_663		663		/* stfsx	*/
#define	PPC_OP31_664		664		/* srqx	(POWER/601 only) */
#define	PPC_OP31_665		665		/* srex	(POWER/601 only) */
#define	PPC_OP31_695		695		/* stfsux	*/
#define	PPC_OP31_696		696		/* sriqx (POWER/601 only) */
#define	PPC_OP31_725		725		/* stswi	*/
#define	PPC_OP31_727		727		/* stfdx	*/
#define	PPC_OP31_728		728		/* srlqx (POWER/601 only) */
#define	PPC_OP31_729		729		/* sreqx (POWER/601 only) */
#define	PPC_OP31_759		759		/* stfdux	*/
#define	PPC_OP31_760		760		/* srliqx (POWER/601 only) */
#define	PPC_OP31_790		790		/* lhbrx	*/
#define	PPC_OP31_792		792		/* srawx	*/
#define	PPC_OP31_824		824		/* srawix	*/
#define	PPC_OP31_854		854		/* eieio	*/
#define	PPC_OP31_918		918		/* sthbrx	*/
#define	PPC_OP31_920		920		/* sraqx (POWER/601 only) */
#define	PPC_OP31_921		921		/* sreax (POWER/601 only) */
#define	PPC_OP31_922		922		/* extshx	*/
#define	PPC_OP31_952		952		/* sraiqx (POWER/601 only) */
#define	PPC_OP31_954		954		/* extsbx	*/
#define	PPC_OP31_978		978		/* tlbld (603 only) */
#define	PPC_OP31_982		982		/* icbi		*/
#define	PPC_OP31_983		983		/* stfiwx	*/
#define	PPC_OP31_1010		1010		/* tlbli (603 only) */
#define	PPC_OP31_1014		1014		/* dcbz		*/


#define	PPC_OP59_18		18		/* fdivsx	*/
#define	PPC_OP59_20		20		/* fsubsx	*/
#define	PPC_OP59_21		21		/* faddsx	*/
#define	PPC_OP59_22		22		/* fsqrtsx	*/
#define	PPC_OP59_24		24		/* fresx	*/
#define	PPC_OP59_25		25		/* fmulsx	*/
#define	PPC_OP59_28		28		/* fmsubsx	*/
#define	PPC_OP59_29		29		/* fmaddsx	*/
#define	PPC_OP59_30		30		/* fnmsubsx	*/
#define	PPC_OP59_31		31		/* fnmaddsx	*/


#define	PPC_OP63_0		0		/* fcmpu	*/
#define	PPC_OP63_12		12		/* frspx	*/
#define	PPC_OP63_14		14		/* fctiwx	*/
#define	PPC_OP63_15		15		/* fctiwzx	*/
#define	PPC_OP63_18		18		/* fdivx	*/
#define	PPC_OP63_20		20		/* fsubx	*/
#define	PPC_OP63_21		21		/* faddx	*/
#define	PPC_OP63_22		22		/* fsqrtx	*/
#define	PPC_OP63_23		23		/* fselx	*/
#define	PPC_OP63_25		25		/* fmulx	*/
#define	PPC_OP63_26		26		/* frsqrtex	*/
#define	PPC_OP63_28		28		/* fmsubx	*/
#define	PPC_OP63_29		29		/* fmaddx	*/
#define	PPC_OP63_30		30		/* fnmsubx	*/
#define	PPC_OP63_31		31		/* fnmaddx	*/
#define	PPC_OP63_32		32		/* fcmpo	*/
#define	PPC_OP63_38		38		/* mtfsb1x	*/
#define	PPC_OP63_40		40		/* fnegx	*/
#define	PPC_OP63_64		64		/* mcrfs	*/
#define	PPC_OP63_70		70		/* mtfsb0x	*/
#define	PPC_OP63_72		72		/* fmrx		*/
#define	PPC_OP63_134		134		/* mtfsfix 	*/
#define	PPC_OP63_136		136		/* fnabsx	*/
#define	PPC_OP63_264		264		/* fabsx	*/
#define	PPC_OP63_583		583		/* mffsx	*/
#define	PPC_OP63_711		711		/* mtfsfx	*/

#endif		/* PPCDIS_OPCODES_H */
