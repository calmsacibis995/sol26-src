/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#if !defined(PPCDIS_MNEMONICS_H)
#define	PPCDIS_MNEMONICS_H

#pragma ident	"@(#)ppcdis_mnemonics.h	1.6	95/10/20 SMI"

/*
 * PowerPC Disassembler - Standard Mnemonics
 *
 * This file contains all the Power PC standard mnemonics
 * NOTE: Extended mnemonics are handled on a per-case basis.
 */

#define	PPC_INST_3	"twi"		/* Trap Word Immediate */
#define	PPC_INST_7	"mulli"		/* Multiply Low Immediate */
#define	PPC_INST_8	"subfic"	/* Subtract from Immediate Carrying */
#define	PPC_INST_9	"dozi"		/* Difference or Zero Immediate */
					/* (POWER/601 only) */
#define	PPC_INST_10	"cmpli"		/* Compare Logical Immediate */
#define	PPC_INST_11	"cmpi"		/* Compare Immediate */
#define	PPC_INST_12	"addic"		/* Add Immediate Carrying */
#define	PPC_INST_13	"addic."	/* Add Immediate Carrying and Record */
#define	PPC_INST_14	"addi"		/* Add Immediate */
#define	PPC_INST_15	"addis"		/* Add Immediate Shifted */
#define	PPC_INST_16	"bcx"		/* Branch Conditional */
#define	PPC_INST_17	"sc"		/* System Call */
#define	PPC_INST_18	"bx"		/* Branch */
#define	PPC_INST_19	"??"		/* ?? */
#define	PPC_INST_20	"rlwimi"	/* Rotate Left Word Immediate then */
					/* AND with Mask Insert */
#define	PPC_INST_21	"rlwinm"	/* Rotate Left Word Immediate then */
					/* AND with Mask */
#define	PPC_INST_22	"rlmi"		/* Rotate Left then Mask Insert */
					/* (POWER/601 only) */
#define	PPC_INST_23	"rlwnm"		/* Rotate Left Word then AND w/Mask */
#define	PPC_INST_24	"ori"		/* OR Immediate */
#define	PPC_INST_25	"oris"		/* OR Immediate Shifted */
#define	PPC_INST_26	"xori"		/* XOR Immediate */
#define	PPC_INST_27	"xoris"		/* XOR Immediate Shifted */
#define	PPC_INST_28	"andi."		/* AND Immediate */
#define	PPC_INST_29	"andis."	/* AND Immediate Shifted */
#define	PPC_INST_31	"??"		/* ?? */
#define	PPC_INST_32	"lwz"		/* Load Word and Zero */
#define	PPC_INST_33	"lwzu"		/* Load Word and Zero with Update */
#define	PPC_INST_34	"lbz"		/* Load Byte and Zero */
#define	PPC_INST_35	"lbzu"		/* Load Byte and Zero with Update */
#define	PPC_INST_36	"stw"		/* Store Word */
#define	PPC_INST_37	"stwu"		/* Store Word with Update */
#define	PPC_INST_38	"stb"		/* Store Byte */
#define	PPC_INST_39	"stbu"		/* Store Byte with Update */
#define	PPC_INST_40	"lhz"		/* Load Halfword and Zero */
#define	PPC_INST_41	"lhzu"		/* Load Halfword and Zero */
#define	PPC_INST_42	"lha"		/* Load Halfword Algebraic */
#define	PPC_INST_43	"lhau"		/* Load Halfword Algebraic w/Update */
#define	PPC_INST_44	"sth"		/* Store Halfword */
#define	PPC_INST_45	"sthu"		/* Store Halfword with Update */
#define	PPC_INST_46	"lmw"		/* Load Multiple Word */
#define	PPC_INST_47	"stmw"		/* Store Multiple Word */
#define	PPC_INST_48	"lfs"		/* Load Floating-Point Single-Prec. */
#define	PPC_INST_49	"lfsu"		/* Load Floating-Point */
					/* Single-Precision with Update */
#define	PPC_INST_50	"lfd"		/* Load Floating-Point Double-Prec. */
#define	PPC_INST_51	"lfdu"		/* Load Floating-Point */
					/* Double-Precision with Update */
#define	PPC_INST_52	"stfs"		/* Store Floating-Point Single-Prec. */
#define	PPC_INST_53	"stfsu"		/* Store Floating-Point */
					/* Single-Precision with Update */
#define	PPC_INST_54	"stfd"		/* Store Floating-Point Double-Prec. */
#define	PPC_INST_55	"stfdu"		/* Store Floating-Point */
					/* Double-Precision with Update */
#define	PPC_INST_59	"??"		/* ?? */
#define	PPC_INST_63	"??"		/* ?? */

/*
 * Opcodes 19, 31, 59, and 63 have additional extended opcodes described below
 */

/*
 * Condition Register-Related Instructions
 */
#define	PPC_INST19_0	"mcrf"		/* Move Condition Register Field */
#define	PPC_INST19_16	"bclrx"		/* Brnch Conditional to Link Register */
#define	PPC_INST19_33	"crnor"		/* Condition Register NOR */
#define	PPC_INST19_50	"rfi"		/* Return from Interrupt */
#define	PPC_INST19_129	"crandc"	/* Condition Register AND w/Complemnt */
#define	PPC_INST19_150	"isync"		/* Instruction Synchronize */
#define	PPC_INST19_193	"crxor"		/* Condition Register XOR */
#define	PPC_INST19_225	"crnand"	/* Condition Register NAND */
#define	PPC_INST19_257	"crand"		/* Condition Register AND */
#define	PPC_INST19_289	"creqv"		/* Condition Register Equivalent */
#define	PPC_INST19_417	"crorc"		/* Condition Register OR w/Complement */
#define	PPC_INST19_449	"cror"		/* Condition Register OR */
#define	PPC_INST19_528	"bcctrx"	/* Branch Conditional to Count Reg. */


#define	PPC_INST31_0	"cmp"		/* Compare */
#define	PPC_INST31_4	"tw"		/* Trap Word */
#define	PPC_INST31_8	"subfc"		/* Subtract from Carrying */
#define	PPC_INST31_10	"addc"		/* Add Carrying */
#define	PPC_INST31_11	"mulhwu"	/* Multiply High Word Unsigned */
#define	PPC_INST31_19	"mfcr"		/* Move from Condition Register */
#define	PPC_INST31_20	"lwarx"		/* Load Word and Reserve Indexed */
#define	PPC_INST31_23	"lwzx"		/* Load Word and Zero Indexed */
#define	PPC_INST31_24	"slw"		/* Shift Left Word */
#define	PPC_INST31_26	"cntlzw"	/* Count Leading Zeros Word */
#define	PPC_INST31_28	"and"		/* AND */
#define	PPC_INST31_29	"maskg"		/* Mask Generate (POWER/601 only) */
#define	PPC_INST31_32	"cmpl"		/* Compare Logical */
#define	PPC_INST31_40	"subf"		/* Subtract from */
#define	PPC_INST31_54	"dcbst"		/* Data Cache Block Store */
#define	PPC_INST31_55	"lwzux"		/* Load Word and Zero with Update */
					/* Indexed */
#define	PPC_INST31_60	"andc"		/* AND with Complement */
#define	PPC_INST31_75	"mulhw"		/* Multiply High Word */
#define	PPC_INST31_83	"mfmsr"		/* Move from Machine State Register */
#define	PPC_INST31_86	"dcbf"		/* Data Cache Block Flush */
#define	PPC_INST31_87	"lbzx"		/* Load Byte and Zero Indexed */
#define	PPC_INST31_104	"neg"		/* Negate */
#define	PPC_INST31_107	"mul"		/* Multiply (POWER/601 only) */
#define	PPC_INST31_115	"mfpmr"		/* Move from Program Mode Register */
#define	PPC_INST31_119	"lbzux"		/* Load Byte and Zero with Update */
					/* Indexed */
#define	PPC_INST31_124	"nor"		/* NOR */
#define	PPC_INST31_136	"subfe"		/* Subtract from Extended */
#define	PPC_INST31_138	"adde"		/* Add Extended */
#define	PPC_INST31_144	"mtcrf"		/* Move to Condition Register Fields */
#define	PPC_INST31_146	"mtmsr"		/* Move to Machine State Register */
#define	PPC_INST31_150	"stwcx."	/* Store Word Conditional Indexed */
#define	PPC_INST31_151	"stwx"		/* Store Word Indexed */
#define	PPC_INST31_152	"slq"		/* Shift Left w/MQ (POWER/601 only) */
#define	PPC_INST31_153	"sle"		/* Shift Left Extended */
					/* (POWER/601 only) */
#define	PPC_INST31_178	"mtpmr"		/* Move to Program Mode Register */
#define	PPC_INST31_183	"stwux"		/* Store Word with Update Indexed */
#define	PPC_INST31_184	"sliq"		/* Shift Left Immediate with MQ */
					/* (POWER/601 only) */
#define	PPC_INST31_200	"subfze"	/* Subtract from Zero Extended */
#define	PPC_INST31_202	"addze"		/* Add to Zero Extended */
#define	PPC_INST31_210	"mtsr"		/* Move to Segment Register */
#define	PPC_INST31_215	"stbx"		/* Store Byte Indexed */
#define	PPC_INST31_216	"sllq"		/* Shift Left Long with MQ */
					/* (POWER/601 only) */
#define	PPC_INST31_217	"sleq"		/* Shift Left Extended with MQ */
					/* (POWER/601 only) */
#define	PPC_INST31_232	"subfme"	/* Subtract from Minus One Extended */
#define	PPC_INST31_234	"addme"		/* Add to Minus One Extended */
#define	PPC_INST31_235	"mullw"		/* Multiply Low */
#define	PPC_INST31_242	"mtsrin"	/* Move to Segment Register Indirect */
#define	PPC_INST31_246	"dcbtst"	/* Data Cache Block Touch for Store */
#define	PPC_INST31_247	"stbux"		/* Store Byte with Update Indexed */
#define	PPC_INST31_248	"slliq"		/* Shift Left Long Immediate with MQ */
					/* (POWER/601 only) */
#define	PPC_INST31_264	"doz"		/* Difference or Zero */
					/* (POWER/601 only) */
#define	PPC_INST31_266	"add"		/* Add */
#define	PPC_INST31_277	"lscbx"		/* Load String and Compare Byte */
					/* Indexed (POWER/601 only) */
#define	PPC_INST31_278	"dcbt"		/* Data Cache Block Touch */
#define	PPC_INST31_279	"lhzx"		/* Load Halfword and Zero Indexed */
#define	PPC_INST31_284	"eqv"		/* Equivalent */
#define	PPC_INST31_306	"tlbie"		/* TLB Invalidate Entry */
#define	PPC_INST31_310	"eciwx"		/* External Control Input Word */
					/* Indexed */
#define	PPC_INST31_311	"lhzux"		/* Load Halfword and Zero with */
					/* Update Indexed */
#define	PPC_INST31_316	"xor"		/* XOR */
#define	PPC_INST31_331	"div"		/* Divide	(POWER/601 only) */
#define	PPC_INST31_339	"mfspr"		/* Move from Special Purpose Register */
#define	PPC_INST31_343	"lhax"		/* Load Halfword Algebraic Indexed */
#define	PPC_INST31_360	"abs"		/* Absolute	(POWER/601 only) */
#define	PPC_INST31_363	"divs"		/* Divide Short (POWER/601 only) */
#define	PPC_INST31_371	"mftb"		/* Move from Time Base */
#define	PPC_INST31_375	"lhaux"		/* Load Halfword Algebraic with */
					/* Update Indexed */
#define	PPC_INST31_407	"sthx"		/* Store Halfword Indexed */
#define	PPC_INST31_412	"orc"		/* OR with Complement */
#define	PPC_INST31_434	"slbia"		/* SLB Invalidate Entry */
#define	PPC_INST31_438	"ecowx"		/* External Control Output Word */
					/* Indexed */
#define	PPC_INST31_439	"sthux"		/* Store Halfword with Update Indexed */
#define	PPC_INST31_444	"or"		/* OR */
#define	PPC_INST31_459	"divwu"		/* Divide Word Unsigned */
#define	PPC_INST31_466	"slbiex"	/* SLB Invalidate Entry by Index */
#define	PPC_INST31_467	"mtspr"		/* Move to Special Purpose Register */
#define	PPC_INST31_470	"dcbi"		/* Data Cache Block Invalidate */
#define	PPC_INST31_476	"nand"		/* NAND */
#define	PPC_INST31_488	"nabs"		/* Negative Absolute (POWER/601 only) */
#define	PPC_INST31_491	"divw"		/* Divide Word */
#define	PPC_INST31_498	"slbia"		/* SLB Invalidate All */
#define	PPC_INST31_512	"mcrxr"		/* Move to Condition Register fr. XER */
#define	PPC_INST31_531	"clcs"		/* Cache Line Compute Size */
					/* (POWER/601 only) */
#define	PPC_INST31_533	"lswx"		/* Load String Word Indexed */
#define	PPC_INST31_534	"lwbrx"		/* Load Word Byte-Reverse Indexed */
#define	PPC_INST31_535	"lfsx"		/* Load Floating-Point */
					/* Single-Precision Indexed */
#define	PPC_INST31_536	"srw"		/* Shift Right Word */
#define	PPC_INST31_537	"rrib"		/* Rotate Right and Insert Bit */
					/* (POWER/601 only) */
#define	PPC_INST31_541	"maskir"	/* Mask Insert from Register */
					/* (POWER/601 only) */
#define	PPC_INST31_566	"tlbsync"	/* TLB Synchronize */
#define	PPC_INST31_567	"lfsux"		/* Load Floating-Point Single-Prec. */
					/* witn Update Indexed */
#define	PPC_INST31_595	"mfsr"		/* Move from Segment Register */
#define	PPC_INST31_597	"lswi"		/* Load String Word Immediate */
#define	PPC_INST31_598	"sync"		/* Synchronize */
#define	PPC_INST31_599	"lfdx"		/* Load Floating-Point */
					/* Double-Precion Indexed */
#define	PPC_INST31_631	"lfdux"		/* Load Floating-Point Double-Prec. */
					/* with Update Indexed */
#define	PPC_INST31_659	"mfsrin"	/* Move from Segment Register */
					/* Indirect */
#define	PPC_INST31_661	"stswx"		/* Store String Word Indexed */
#define	PPC_INST31_662	"stwbrx"	/* Store Word Byte-Reverse Indexed */
#define	PPC_INST31_663	"stfsx"		/* Store Floating-Point */
					/* Single-Precision Indexed */
#define	PPC_INST31_664	"srq"		/* Shift Right with MQ */
					/* (POWER/601 only) */
#define	PPC_INST31_665	"sre"		/* Shift Right Extended */
					/* (POWER/601 only) */
#define	PPC_INST31_695	"stfsux"	/* Store Floating-Point Single-Prec. */
					/* with Update Indexed */
#define	PPC_INST31_696	"sriq"		/* Shift Right Immediate with MQ */
					/* (POWER/601 only) */
#define	PPC_INST31_725	"stswi"		/* Store String Word Immediate */
#define	PPC_INST31_727	"stfdx"		/* Store Floating-Point */
					/* Double-Precision Indexed */
#define	PPC_INST31_728	"srlq"		/* Shift Right Long with MQ */
					/* (POWER/601 only) */
#define	PPC_INST31_729	"sreq"		/* Shift Right Extended with MQ */
					/* (POWER/601 only) */
#define	PPC_INST31_759	"stfdux"	/* Store Floating-Point Double-Prec. */
					/* with Update Indexed */
#define	PPC_INST31_760	"srliq"		/* Shift Right Long Immediate with MQ */
					/* (POWER/601 only) */
#define	PPC_INST31_790	"lhbrx"		/* Load Halfword Byte-Reverse Indexed */
#define	PPC_INST31_792	"sraw"		/* Shift Right Algebraic Word */
#define	PPC_INST31_824	"srawi"		/* Shift Right Algebraic Word */
					/* Immediate */
#define	PPC_INST31_854	"eieio"		/* Enforce In-Order Execution of I/O */
#define	PPC_INST31_918	"sthbrx"	/* Store Halfword Byte-Reverse */
					/* Indexed */
#define	PPC_INST31_920	"sraq"		/* Shift Right Algebraic with MQ */
					/* (POWER/601 only) */
#define	PPC_INST31_921	"srea"		/* Shift Right Extended Algebraic */
					/* (POWER/601 only) */
#define	PPC_INST31_922	"extsh"		/* Extend Sign Halfword */
#define	PPC_INST31_952	"sraiq"		/* Shift Right Algebraic Immediate */
					/* with MQ (POWER/601 only) */
#define	PPC_INST31_954	"extsb"		/* Extend Sign Byte */
#define	PPC_INST31_978	"tlbld"		/* Load Data TLB Entry (603 specific) */
#define	PPC_INST31_982	"icbi"		/* Instruction Cache Block Invalidate */
#define	PPC_INST31_983	"stfiwx"	/* Store Floating-Point as Integer */
					/* Word Indexed */
#define	PPC_INST31_1010	"tlbli"		/* Load Instruction TLB Entry (603 specific) */
#define	PPC_INST31_1014	"dcbz"		/* Data Cache Block set to Zero */

/*
 * Single-Precision Floating-Point Instructions
 */
#define	PPC_INST59_18	"fdivs"		/* Floating-Point Divide */
					/* Single-Precision */
#define	PPC_INST59_20	"fsubs"		/* Floating-Point Subtract */
					/* Single-Precision */
#define	PPC_INST59_21	"fadds"		/* Floating-Point Add Single-Prec. */
#define	PPC_INST59_22	"fsqrts"	/* Floating-Point Square Root */
					/* Single-Precision */
#define	PPC_INST59_24	"fres"		/* Floating-Point Reciprocal */
					/* Estimate Single-Precision */
#define	PPC_INST59_25	"fmuls"		/* Floating-Point Multiply */
					/* Single-Precision */
#define	PPC_INST59_28	"fmsubs"	/* Floating-Point Multiply-Subtract */
					/* Single-Precision */
#define	PPC_INST59_29	"fmadds"	/* Floating-Point Multiply-Add */
					/* Single-Precision */
#define	PPC_INST59_30	"fnmsubs"	/* Floating-Point Negative */
					/* Multiply-Subtract Single-Precision */
#define	PPC_INST59_31	"fnmadds"	/* Floating-Point Negative */
					/* Multiply-Add Single-Precision */

/*
 * Floating-Point Instructions
 */
#define	PPC_INST63_0	"fcmpu"		/* Floating-Point Compare Unordered */
#define	PPC_INST63_12	"frsp"		/* Floating-Point Round to */
					/* Single-Precision */
#define	PPC_INST63_14	"fctiw"		/* Floating-Point Convert to */
					/* Integer Word */
#define	PPC_INST63_15	"fctiwz"	/* Floating-Point Convert to Integer */
					/* Word with Round Toward Zero */
#define	PPC_INST63_18	"fdiv"		/* Floating-Point Divide */
#define	PPC_INST63_20	"fsub"		/* Floating-Point Subtract */
#define	PPC_INST63_21	"fadd"		/* Floating-Point Add */
#define	PPC_INST63_22	"fsqrt"		/* Floating-Point Square Root */
#define	PPC_INST63_23	"fsel"		/* Floating-Point Select */
#define	PPC_INST63_25	"fmul"		/* Floating-Point Multiply */
#define	PPC_INST63_26	"frsqrte"	/* Floating-Point Square Root */
					/* Estimate */
#define	PPC_INST63_28	"fmsub"		/* Floating-Point Multiply-Subtract */
#define	PPC_INST63_29	"fmadd"		/* Floating-Point Multiply-Add */
#define	PPC_INST63_30	"fnmsub"	/* Floating-Point Negative */
					/* Multiply-Subtract */
#define	PPC_INST63_31	"fnmadd"	/* Floating-Point Negative */
					/* Multiply-Add */
#define	PPC_INST63_32	"fcmpo"		/* Floating-Point Compare Ordered */
#define	PPC_INST63_38	"mtfsb1"	/* Move to FPSCR Bit 1 */
#define	PPC_INST63_40	"fneg"		/* Floating-Point Negate */
#define	PPC_INST63_64	"mcrfs"		/* Move to Condition Register from */
					/* FPSCR */
#define	PPC_INST63_70	"mtfsb0"	/* Move to FPSCR Bit 0 */
#define	PPC_INST63_72	"fmr"		/* Floating-Point Move Register */
#define	PPC_INST63_134	"mtfsfi"	/* Move to FPSCR Field Immediate */
#define	PPC_INST63_136	"fnabs"		/* Floating-Point Negative */
					/* Absolute Value */
#define	PPC_INST63_264	"fabs"		/* Floating-Point Absolute Value */
#define	PPC_INST63_583	"mffs"		/* Move from FPSCR */
#define	PPC_INST63_711	"mtfsf"		/* Move to FPSCR Fields */

#endif		/* PPCDIS_MNEMONICS_H */
