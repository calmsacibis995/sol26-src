/*
 * XXXPPC - mfsri instruction is implemented on the 601,
 * but instruction page is missing
 */
/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#if !defined(PPCDIS_INST_FMT_H)
#define	PPCDIS_INST_FMT_H

#pragma ident	"@(#)ppcdis_inst_fmt.h	1.6	95/10/20 SMI"
#pragma ident "@(#)ppcdis_inst_fmt.h 1.3	93/10/19 vla"

/*
 * PowerPC Disassembler - Instruction Formats
 *
 * This file contains all the structures that describe the various Power PC
 * instruction formats.  Cf. the union "op" at the end of this file that
 * includes all structures below.
 */

#if defined(MC98601)
#define	WORD_SIZE	32
#define	HWORD_SIZE	16
#endif	/* defined(MC98601) */

/*
 * Big-Endian version of opcode structures.
 */

/*
 * XO Instruction Format:
 *
 *      0      6    11     16     21 22         31
 *      --------------------------------------------
 *      | OPCD| rT  | rA   | rB   |OE|    XO    |Rc|
 *      --------------------------------------------
 *
 * used by:
 *	absx	31/360	Absolute			I rB=0	(POWER/601 only)
 *	addx	31/266	Add				I
 *	addcx	31/10	Add Carrying			I
 *	addex	31/138	Add Extended			I
 *	addmex	31/234	Add to Minus One Extended	I rB=0
 *	addzex	31/202	Add to Zero Extended		I rB=0
 *	divx	31/331	Divide				I	(POWER/601 only)
 *	divsx	31/363	Divide Short			I	(POWER/601 only)
 *	divwx	31/491	Divide Word			I
 *	divwux	31/459	Divide Word Unsigned		I
 *	dozx	31/264	Difference or Zero		I	(POWER/601 only)
 *	mulx	31/107	Multiply			I	(POWER/601 only)
 *	mulhwx	31/75	Multiply High Word		I OE=0
 *	mulhwux	31/11	Multiply High Word Unsigned	I OE=0
 *	mullwx	31/235	Multiply Low			I
 *	nabsx	31/488	Negative Absolute		I rb=0	(POWER/601 only)
 *	negx	31/104	Negate				I rB=0
 *	subfx	31/40	Subtract from			I
 *	subfcx	31/8	Subtract from Carrying		I
 *	subfex	31/136	Subtract from Extended		I
 *	subfmex	31/232	Subtract fr Minus One Extended	I rB=0
 *	subfzex	31/200	Subtract from Zero Extended	I rB=0
 */
struct op_imath {
	unsigned Rc : 1;	/* record bit */
	unsigned XO : 9;	/* extended opcode */
	unsigned OE : 1;	/* overflow enable bit */
	unsigned rB : 5;	/* second register operand */
	unsigned rA : 5;	/* first register operand */
	unsigned rD : 5;	/* target register */
	unsigned op : 6;	/* primary opcode */
};


/*
 * D Instruction Format:
 *
 *      0      6    11     16
 *      --------------------------------------------
 *      | OPCD| rT  | rA   |       D/SI/UI         |
 *      --------------------------------------------
 *
 * used by:
 *	addi	14	Add Immediate			I D=SIMM
 *	addic	12	Add Immediate Carrying		I D=SIMM
 *	addic.	13	Add Immediate Carrying and Record I D=SIMM
 *	addis	15	Add Immediate Shifted		I D=SIMM
 *	andi.	28	AND Immediate			I D=UIMM,rD=rS
 *	andis.	29	AND Immediate Shifted		I D=UIMM,rD=rS
 *	dozi	09	Difference or Zero Immediate I D=SIMM (POWER/601 only)
 *	lbz	34	Load Byte and Zero		I
 *	lbzu	35	Load Byte and Zero with Update	I
 *	lfd	50	Load Floating-point Double-Precision	F,I rD=frD
 *	lfdu	51	Load Floating-point D.P. with Update	F,I rD=frD
 *	lfs	48	Load Floating-point Single-Precision	F,I rD=frD
 *	lfsu	49	Load Floating-point S.P. with Update	F,I rD=frD
 *	lha	42	Load Halfword Algebraic		I
 *	lhau	43	Load Halfword Algebraic with Update	I
 *	lhz	40	Load Halfword and Zero		I
 *	lhzu	41	Load Halfword and Zero with Update	I
 *	lmw	46	Load Multiple Word		I
 *	lwz	32	Load Word and Zero		I
 *	lwzu	33	Load Word and Zero with Update	I
 *	mulli	07	Multiply Low Immediate		I D=SIMM
 *	ori	24	OR Immediate			I D=UIMM,rD=rS
 *	oris	25	OR Immediate Shifted		I D=UIMM,rD=rS
 *	stb	38	Store Byte			I rD=rS
 *	stbu	39	Store Byte with Update		I rD=rS
 *	stfd	54	Store Floating-point Double	F rD=frS
 *	stfdu	55	Store Floating-point Double w/Update	F rD=frS
 *	stfs	52	Store Floating-point Single-Precision	I,FP rD=frS
 *	stfsu	53	Store Floating-point S.P. w/Update	I,FP rD=frS
 *	sth	44	Store Halfword			I rD=rS
 *	sthu	45	Store Halfword with Update	I rD=rS
 *	stmw	47	Store Multiple Word		I rD=rS
 *	stw	36	Store Word			I rD=rS
 *	stwu	37	Store Word with Update		I rD=rS
 *	subfic	08	Subtract from Immediate Carrying I D=SIMM
 *	twi	03	Trap Word Immediate		I rD=TO,D=SIMM
 *	xori	26	XOR Immediate			I D=UIMM,rD=rS
 *	xoris	27	XOR Immediate Shifted		I D=UIMM,rD=rS
 *  NOTE:	SIMM = 16-bit signed integer
 *		UIMM = 16-bit unsigned integer
 */
struct op_imm {
	unsigned D   : 16;	/* 16-bit signed 2's complement integer */
				/* sign-extend to 32 bits - op_immed operand */
	unsigned rA : 5;	/* first register operand */
	unsigned rD : 5;	/* target register */
	unsigned op : 6;	/* primary opcode */
};

/*
 * X Instruction Format:
 *
 *      0      6    11     16     21            31
 *      --------------------------------------------
 *      | OPCD| rT  | rA   | rB   |      XO     |Rc|
 *      --------------------------------------------
 *
 * used by:
 *	andx	31/28	AND				I rD=rS
 *	andcx	31/60	AND with Complement		I rD=rS
 *	clcs	31/531	Cache Line Compute Size		I rB=0	(POWER/601 only)
 *	cntlzwx	31/26	Count Leading Zeros Word	I rD=rS,rB=0
 *	dcbf	31/86	Data Cache Block Flush		I Rc,rS=0
 *	dcbi	31/470	Data Cache Block Invalidate	I Rc,rS=0
 *	dcbst	31/54	Data Cache Block Store		I Rc,rS=0
 *	dcbt	31/278	Data Cache Block Touch		I Rc,rS=0
 *	dcbtst	31/246	Data Cache Block Touch for Store I Rc,rS=0
 *	dcbz	31/1014	Data Cache Block Set to Zero	I Rc,rS=0
 *	eqvx	31/284	Equivalent			I
 *	extsbx	31/954	Extend Sign Byte		I rB=0
 *	extshx	31/922	Extend Sign Halfword		I rB=0
 *	fabsx	63/264	Floating-point Absolute Value	F frA=0
 *	eciwx	31/310	External Control In Word Indexed	? Rc=0
 *	ecowx	31/438	External Control Out Word Indexed	? Rc=0
 *	eieio	31/854	Enforce In-OrderExecution of I/O	? Rc,rD,rA,rB=0
 *	fmrx	63/72	Floating-point Move Register		F frA=0
 *	fnabsx	63/136	Floating-point Negative Absolute Value 	F frA=0
 *	fnegx	63/40	Floating-point Negate			F frA=0
 *	frspx	63/12	Floating-point Round to Single-Prec	F frA=0,rA=frA,
 *								rB=frB
 *	icbi	31/982	Instruction Cache Block Invalidate	C rD,Rc=0
 *	isync	19/150	Instruction Synchronize			C rA,rB,rD,Rc=0
 *	lbzux	31/119	Load Byte and Zero with Update Indexed	I Rc=0
 *	lbzx	31/87	Load Byte and Zero Indexed		I Rc=0
 *	lfdux	31/631	Load F.P. Double-Prec w/Update Indexed	F,I Rc=0,rD=frD
 *	lfdx	31/599	Load F.P. Double-Precision Indexed	F,I Rc=0,rD=frD
 *	lfsux	31/567	Load F.P. Single-Prec w/Update Indexed	F,I Rc=0,rD=frD
 *	lfsx	31/535	Load F.P. Single-Precision Indexed	F,I Rc=0,rD=frD
 *	lhaux	31/375	Load Halfword Algebraic w/Update Indxd	I Rc=0
 *	lhax	31/343	Load Halfword Algebraic Indexed		I Rc=0
 *	lhbrx	31/790	Load Halfword Byte-Reverse Indexed	I Rc=0
 *	lhzux	31/311	Load Halfword and Zero w/Update Indexed	I Rc=0
 *	lhzx	31/279	Load Halfword and Zero Indexed		I Rc=0
 *	lscbxx	31/277	Load String and Compare Byte Indexed I (POWER/601 only)
 *	lswi	31/597	Load String Word Immediate		I Rc=0,rB=NB
 *	lswx	31/533	Load String Word Indexed		I Rc=0
 *	lwarx	31/20	Load Word and Reserve Indexed		I Rc=0
 *	lwbrx	31/534	Load Word Byte-Reverse Indexed		I Rc=0
 *	lwzux	31/55	Load Word and Zero w/Update Indexed	I Rc=0
 *	lwzx	31/23	Load Word and Zero Indexed		I Rc=0
 *	maskgx	31/29	Mask Generate			I (POWER/601 only)
 *	maskirx	31/541	Mask Insert from Register	I (POWER/601 only)
 *	mfcr	31/19	Move from Condition Register		I rA,rB,Rc=0
 *	mffsx	63/583	Move from FPSCR			I rA,rB=0,rD=frD
 *	mfmsr	31/83	Move from Machine State Register	I rA,rB,Rc=0
 *	mfsrin	31/659	Move from Segment Register Indirect	I rA,Rc=0
 *	mtmsr	31/146	Move to Machine State Register	I rA,rB,Rc=0,rD=rS
 *	mtsrin	31/242	Move to Segment Register Indirect	I rA,Rc=0,rD=rS
 *	nandx	31/476	NAND					I rD=rS
 *	norx	31/124	NOR					I rD=rS
 *	orx		31/444	OR				I rD=rS
 *	orcx	31/412	OR with Complement			I rD=rS
 *	rfi		19/50	Return from Interrupt		I rA,rB,rD,Rc=0
 *	rribx	31/537	Rotate Right and Insert Bit	I rA,rB,rD=rS
 *								(POWER/601 only)
 *	slex	31/153	Shift Left Extended		I rA,rB,rD=rS
 *								(POWER/601 only)
 *	sleqx	31/217	Shift Left Extended with MQ	I rA,rB,rD=rS
 *								(POWER/601 only)
 *	sliqx	31/184	Shift Left Immediate with MQ	I rA,rB=SH,rD=rS
 *								(POWER/601 only)
 *	slliqx	31/248	Shift Left Long Immediate with MQ	I rA,rB=SH,rD=rS
 *								(POWER/601 only)
 *	sllqx	31/216	Shift Left Long with MQ		I rA,rB,rD=rS
 *								(POWER/601 only)
 *	slqx	31/152	Shift Left with MQ		I rA,rB,rD=rS
 *								(POWER/601 only)
 *	slwx	31/24	Shift Left Word			I rD=rS
 *	sraiqx	31/952	Shift Right Algebraic Immediate with MQ	I rA,rB=SH,rD=rS
 *								(POWER/601 only)
 *	sraqx	31/920	Shift Right Algebraic with MQ	I rA,rB,rD=rS
 *								(POWER/601 only)
 *	srawx	31/792	Shift Right Algebraic Word	I rD=rS
 *	srawix	31/824	Shift Right Algebraic Word Immediate	I rD=rS,rB=SH
 *	srex	31/665	Shift Right Extended		I rA,rB,rD=rS
 *								(POWER/601 only)
 *	sreax	31/921	Shift Right Extended Algebraic	I rA,rB,rD=rS
 *								(POWER/601 only)
 *	sreqx	31/729	Shift Right Extended with MQ	I rA,rB,rD=rS
 *								(POWER/601 only)
 *	sriqx	31/696	Shift Right Immediate with MQ	I rA,rB=SH,rD=rS
 *								(POWER/601 only)
 *	srliqx	31/760	Shift Right Long Immediate with MQ	I rA,rB=SH,rD=rS
 *								(POWER/601 only)
 *	srlqx	31/728	Shift Right Long with MQ I rA,rB,rD=rS	(POWER/601 only)
 *	srqx	31/664	Shift Right with MQ	I rA,rB,rD=rS	(POWER/601 only)
 *	srwx	31/792	Shift Right Word		I rD=rS
 *	stbux	31/247	Store Byte with Update Indexed	I Rc=0,rD=rS
 *	stbx	31/215	Store Byte Indexed		I Rc=0,rD=rS
 *	stfdux	31/759	Store F.P. Double w/Update Indexed	F Rc=0,rD=frS
 *	stfdx	31/727	Store Floating-point Double Indexed	F Rc=0,rD=frS
 *	stfiwx	31/983	Store F.P. as Integer Word Indexed	F Rc=0,rD=frS
 *	stfsux	31/695	Store F.P. Single-Prec. w/Update Indxed	I,FP Rc=0,rD=frS
 *	stfsx	31/663	Store F.P. Single-Precision Indexed	I,FP Rc=0,rD=frS
 *	sthbrx	31/918	Store Halfword Byte-Reverse Indexed	I Rc=0,rD=rS
 *	sthux	31/439	Store Halfword with Update Indexed	I Rc=0,rD=rS
 *	sthx	31/407	Store Halfword Indexed			I Rc=0,rD=rS
 *	stswi	31/725	Store String Word Immediate	I Rc=0,rD=rS,rB=NB
 *	stswx	31/661	Store String Word Indexed		I Rc=0,rD=rS
 *	stwbrx	31/662	Store Word Byte-Reverse Indexed		I Rc=0,rD=rS
 *	stwcx.	31/150	Store Word Conditional Indexed		I Rc=1,rD=rS
 *	stwux	31/183	Store Word with Update Indexed		I Rc=0,rD=rS
 *	stwx	31/151	Store Word Indexed			I Rc=0,rD=rS
 *	sync	31/598	Synchronize				I Rc,rA,rB,rD=0
 *	tlbie	31/306	Translation Lookaside Buffer Inv. Entry	I rA,rD,Rc=0
 *	tw		31/4	Trap Word			I rD=TO,Rc=0
 *	xorx	31/316	XOR					I rD=rS
 */
struct op_typ {
	unsigned Rc : 1;	/* record bit */
	unsigned XO : 10;	/* extended opcode */
	unsigned rB : 5;	/* second register operand */
	unsigned rA : 5;	/* first register operand */
	unsigned rD : 5;	/* destination register */
	unsigned op : 6;	/* primary opcode */
};

/*
 * A Instruction Format:
 *
 *      0      6    11     16     21    26      31
 *      --------------------------------------------
 *      | OPCD| rT  | frA  | frB  | frC |  XO   |Rc|
 *      --------------------------------------------
 *
 * used by:
 *	faddx	63/21	Floating-point Add			F frC=0
 *	faddsx	59/21	Floating-point Add Single-Precision	F frC=0
 *	fdivx	63/18	Floating-point Divide			F frC=0
 *	fdivsx	59/18	Floating-point Divide Single-Precision	F frC=0
 *	fmaddx	63/29	Floating-point Multiply-Add		F
 *	fmaddsx	59/29	Floating-point Multiply-Add Single-Prec	F
 *	fmsubx	63/28	Floating-point Multiply-Subtract	F
 *	fmsubsx	59/28	Floating-point Multiply-Subtract S-P	F
 *	fmulx	63/18	Floating-point Multiply			F frB=0
 *	fmulsx	59/18	Floating-point Multiply Single-Prec	F frB=0
 *	fnmaddx	63/31	Floating-point Negative Multiply-Add	F
 *	fnmaddsx 59/31	Floating-point Negative Multiply-Add SP	F
 *	fnmsubx	63/30	Floating-point Negative Multiply-Subtract	F
 *	fnmsubsx 59/30	Floating-point Negative Multiply-Subtract SP	F
 *	fsubx	63/20	Floating-point Subtract			F frC=0
 *	fsubsx	59/20	Floating-point Subtract Single-Prec	F frC=0
 */
struct op_fmath {
	unsigned Rc : 1;	/* record bit */
	unsigned XO : 5;	/* extended opcode */
	unsigned frC : 5;	/* third register operand */
	unsigned frB : 5;	/* second register operand */
	unsigned frA : 5;	/* first register operand */
	unsigned frD : 5;	/* destination register */
	unsigned op : 6;	/* primary opcode */
};

/*
 * M Instruction Format:
 *
 *      0      6    11     16     21     26     31
 *      --------------------------------------------
 *      | OPCD| rS  |  rA  |rB/SH |  MB  |  ME  |Rc|
 *      --------------------------------------------
 *
 * used by:
 *	rlwimix	20	Rotate Left Word Immediate, Mask Insert	I
 *	rlwinmx	21	Rotate Left Word Immediate, AND w/Mask	I
 *	rlmix	22	Rotate Left then Mask Insert	I (POWER/601 only)
 *	rlwnmx	23	Rotate Left Word then AND with Mask	I
 */
struct op_rot {
	unsigned Rc : 1;	/* record bit */
	unsigned ME : 5;	/* mask */
	unsigned MB : 5;	/* mask */
	unsigned SH : 5;	/* shift amount */
	unsigned rA : 5;	/* first register operand */
	unsigned rS : 5;	/* sourcw register */
	unsigned op : 6;	/* primary opcode */
};

/*
 * I Instruction Format:
 *
 *      0      6                             30 31
 *      --------------------------------------------
 *      | OPCD|            LI                |AA|LK|
 *      --------------------------------------------
 *
 * used by:
 *	bx	18	Branch				B
 */
struct op_br {
	unsigned LK : 1;	/* link bit */
	unsigned AA : 1;	/* absolute address bit */
	unsigned LI : 24;	/* op_immediate 2's complement integer */
	unsigned op : 6;	/* primary opcode */
};

/*
 * B Instruction Format:
 *
 *      0      6    11     16                30 31
 *      --------------------------------------------
 *      | OPCD| BO  |  BI  |        BD       |AA|LK|
 *      --------------------------------------------
 *
 * used by:
 *	bcx	16	Branch Conditional		B
 */
struct op_brc {
	unsigned int LK : 1;	/* link bit */
	unsigned int AA : 1;	/* absolute address bit */
	signed int BD   : 14;	/* branch displacement */
	unsigned int BI : 5;	/* branch condition */
	unsigned int BO : 5;	/* branch instruction options */
	unsigned int op : 6;	/* primary opcode */
};

/*
 * X Instruction Format (variant for fixed/float-point comparisons):
 *
 *      0      6      11    16    21             31
 *      --------------------------------------------
 *      | OPCD|BF |0|L| rA  | rB  |      XO      |0|
 *      --------------------------------------------
 *
 * used by:
 *	cmp	31/0	Compare				I LK,XO,cr0=0
 *	cmpl	31/32	Compare Logical			I LK,cr0=0
 *	fcmpo	63/32	Floating-point Compare Ordered	F cr0,crI,LK=0
 *							  rA=frA,rB=frB
 *	fcmpu	63/0	Floating-point Compare Unordered F cr0,crI,XO,LK=0
 *							  rA=frA,rB=frB
 *	mcrxr	31/512	Move to Condition Reg from XER	I LK,rB,rA,L,cr0=0
 */
struct op_cmp {
	unsigned LK : 1;	/* link bit */
	unsigned XO : 10;	/* extended operand */
	unsigned rB : 5;	/* second register operand */
	unsigned rA : 5;	/* first register operand */
	unsigned L : 1;		/* 0=(32-bit operand);1=(64-big operand) */
	unsigned cr0 : 1;	/* CR bit - must be 0 */
	unsigned crD : 3;	/* CR bits */
	unsigned op : 6;	/* primary opcode */
};

/*
 * D Instruction Format (variant for fixed-point comparisons):
 *
 *      0      6     11    16
 *      --------------------------------------------
 *      | OPCD|BF|0|L| rA  |         SI/UI         |
 *      --------------------------------------------
 *
 * used by:
 *	cmpi	11	Compare Immediate		I cr0=0
 *	cmpli	10	Compare Logical Immediate	I cr0=0,SIMM=UIMM
 *	NOTE:	UIMM = 16-bit unsigned integer
 */
struct op_cmpi {
	unsigned SIMM : 16;	/* 16-bit signed integer */
	unsigned rA : 5;	/* first register operand */
	unsigned L : 1;		/* 0=(32-bit operand);1=(64-big operand) */
	unsigned cr0 : 1;	/* CR bit - must be 0 */
	unsigned crD : 3;	/* CR bits */
	unsigned op : 6;	/* primary opcode */
};

/*
 * XL Instruction Format:
 *
 *      0      6    11     16     21            31
 *      --------------------------------------------
 *      | OPCD|  BT |  BA  |  BB  |      XO     |Rc|
 *      --------------------------------------------
 *
 * used by:
 *	crand	19/257	Condition Register AND			I Rc=0
 *	crandc	19/129	Condition Register AND w/Complement	I Rc=0
 *	crnand	19/225	Condition Register NAND			I Rc=0
 *	crnor	19/33	Condition Register NOR			I Rc=0
 *	cror	19/449	Condition Register OR			I Rc=0
 *	crorc	19/417	Condition Register OR w/Complement	I Rc=0
 *	crxor	19/193	Condition Register XOR			I Rc=0
 *	creqv	19/289	Condition Register Equivalent		I Rc=0
 *	mtfsb0x	63/70	Move to FPSCR Bit 0			I crbA,crbB=0
 *	mtfsb1x	63/38	Move to FPSCR Bit 1			I crbA,crbB=0
 *	bcctrx	19/528	Branch Conditional to Count Reg	B BT=BO,BA=BI,BB=0,Rc=LK
 *	bclrx	19/16	Branch Conditional to Link Reg	B BT=BO,BA=BI,BB=0,Rc=LK
 *	NOTE:	BO = branch instruction options
 *		BI = branch condition (condition register flag)
 *		LK = link bit
 */
struct op_bool {
	unsigned Rc : 1;	/* Record Bit/Link Bit */
	unsigned XO : 10;	/* extended opcode */
	unsigned crbB : 5;	/* condition register bit B */
	unsigned crbA : 5;	/* condition register bit A */
	unsigned crbD : 5;	/* destination CR bit D */
	unsigned op : 6;	/* primary opcode */
};

/*
 * X Instruction Format (variant for floating convert to integer):
 *
 *      0      6    11    16     21              31
 *      --------------------------------------------
 *      | OPCD| FRT |00000| FRB  |       XO      |0|
 *      --------------------------------------------
 *
 * used by:
 *	fctiwx	63/14	Floating-point Convert to Integer Word	F ZR1=0
 *	fctiwzx	63/15	Floating-point Convert to Integer Word	F frA=0
 *			with Round Toward Zero
 */
struct op_fpconv {
	unsigned Rc : 1;	/* must be zero! */
	unsigned XO : 10;	/* extended opcode */
	unsigned frB : 5;	/* floating point register B */
	unsigned ZR1 : 5;	/* this field must be 0! */
	unsigned frT : 5;	/* target floating point register */
	unsigned op : 6;	/* primary opcode */
};

/*
 * XFX Instruction Format:
 *
 *      0      6    11            21             31
 *      --------------------------------------------
 *      | OPCD| rT  |     spr     |      XO      |0|
 *      --------------------------------------------
 *
 * used by:
 *	mfspr	31/339	Move from Special Purpose Register	I Rc=0
 *	mtspr	31/467	Move to Special Purpose Register I Rc=0,rS=rD,rD=rS
 *	mftb	31/371	Move from Time Base			I Rc=0
 */
struct op_mspr {
	unsigned Rc : 1;	/* must be zero! */
	unsigned XO : 10;	/* extended opcode */
	unsigned rS : 10;	/* special purpose register */
	unsigned rD : 5;	/* destination register */
	unsigned op : 6;	/* primary opcode */
};

/*
 * X Instruction Format (variant for move from segment register):
 *
 *      0      6    11     16     21             31
 *      --------------------------------------------
 *      | OPCD| rT  |0| SR |00000|       XO      |0|
 *      --------------------------------------------
 *
 * used by:
 *	mfsr	31/595	Move from Segment Register	I ZR1,ZR2,Rc=0
 *	mtsr	31/210	Move to Segment Register	I ZR1,ZR2,Rc=0
 */
struct op_msr {
	unsigned Rc : 1;	/* must be zero! */
	unsigned XO : 10;	/* extended opcode */
	unsigned ZR1 : 5;	/* must be zero! */
	unsigned SR : 4;	/* segment register */
	unsigned ZR2 : 1;	/* must be zero! */
	unsigned rD : 5;	/* destination register */
	unsigned op : 6;	/* primary opcode */
};

/*
 * XFX Instruction Format (variant for move to condition register):
 *
 *      0      6    11            21             31
 *      --------------------------------------------
 *      | OPCD| rS  |0|   FXM   |0|      XO      |0|
 *      --------------------------------------------
 *
 * used by:
 *	mtcrf	31/144	Move to Condition Register Fields	I ZR1,ZR2,Rc=0
 */
struct op_mtcrf {
	unsigned ZR1 : 1;	/* must be zero! */
	unsigned XO  : 10;	/* extended opcode */
	unsigned ZR2 : 1;	/* must be zero! */
	unsigned FXM : 8;	/* condition register mask */
	unsigned ZR3 : 1;	/* must be zero! */
	unsigned rS : 5;	/* source register */
	unsigned op : 6;	/* primary opcode */
};

/*
 * XL Instruction Format (variant for move condition register):
 *
 *      0      6     11     16    21             31
 *      --------------------------------------------
 *      | OPCD| BF|00|BFA|00|00000|      XO      |0|
 *      --------------------------------------------
 *
 * used by:
 *	mcrf	19/0	Move Condition Register Fields	I ZR1,ZR2,Rc=0
 *  mcrfs	63/64	Move to Cond Registr from FPSCR	F Rc,ZR1,ZR2,ZR3=0
 */
struct op_mcrf {
	unsigned Rc : 1;	/* must be zero! */
	unsigned XO : 10;	/* extended opcode */
	unsigned ZR1 : 5;	/* must be zero! */
	unsigned ZR2 : 2;	/* must be zero! */
	unsigned BFA : 3;	/* source condition register */
	unsigned ZR3 : 2;	/* must be zero! */
	unsigned BF : 3;	/* target condition register */
	unsigned op : 6;	/* primary opcode */
};

/*
 * XFL Instruction Format:
 *
 *      0      6            16      21          31
 *      --------------------------------------------
 *      | OPCD|0|   FLM   |0|  frB  |     XO    |Rc|
 *      --------------------------------------------
 *
 * used by:
 *	mtfsfx	63/711	Move to FPSCR Fields		I ZR1,ZR2=0
 */
struct op_mfpscr {
	unsigned Rc : 1;	/* record bit */
	unsigned XO : 10;	/* extended opcode */
	unsigned frB : 5;	/* operand register */
	unsigned ZR1 : 1;	/* must be zero! */
	unsigned FM : 8;	/* field mask */
	unsigned ZR2 : 1;	/* must be zero! */
	unsigned op : 6;	/* primary opcode */
};

/*
 * X Instruction Format (variant for move to FPSCR):
 *
 *      0      6      11    16      21          31
 *      --------------------------------------------
 *      | OPCD| BF |00|00000|  U  |0|     XO    |Rc|
 *      --------------------------------------------
 *
 * used by:
 *	mtfsfix	63/134	Move to FPSCR Field Immediate	I ZR1,ZR2,ZR3-0
 */
struct op_mfpscri {
	unsigned Rc : 1;	/* record bit */
	unsigned XO : 10;	/* extended opcode */
	unsigned ZR1 : 1;	/* must be zero! */
	unsigned IMM : 4;	/* op_immediate value */
	unsigned ZR2 : 5;	/* must be zero! */
	unsigned ZR3 : 2;	/* must be zero! */
	unsigned crfD : 3;	/* FPSCR field */
	unsigned op : 6;	/* primary opcode */
};

/*
 * SC Instruction Format:
 *
 *      0      6    11    16               30 31
 *      -----------------------------------------
 *      | OPCD|00000|00000|0000000000000000|XO|0|
 *      -----------------------------------------
 *
 * used by:
 *	sc	17	System Call			I LK=0;ZR1,ZR2,ZR3=0
 *	subsequent instruction contains information used
 *	during execution of this system call.
 */
struct op_sc {
	unsigned LK : 1;	/* 1 is invalid for PPC, but ok for MC98601 */
	unsigned NZR : 1;	/* must be nonzero! */
				/* combine the next three fields? */
	unsigned ZR1 : 14;	/* must be zero! */
	unsigned ZR2 : 5;	/* must be zero! */
	unsigned ZR3 : 5;	/* must be zero! */
	unsigned op : 6;	/* primary opcode */
};

/*
 * put it all together -
 * now we can define anything!
 */
union op {
		struct op_imath		op_imath;
		struct op_imm		op_imm;
		struct op_typ		op_typ;
		struct op_fmath		op_fmath;
		struct op_rot		op_rot;
		struct op_br		op_br;
		struct op_brc		op_brc;
		struct op_cmp		op_cmp;
		struct op_cmpi		op_cmpi;
		struct op_bool		op_bool;
		struct op_fpconv	op_fpconv;
		struct op_mspr		op_mspr;
		struct op_msr		op_msr;
		struct op_mtcrf		op_mtcrf;
		struct op_mcrf		op_mcrf;
		struct op_mfpscr	op_mfpscr;
		struct op_mfpscri	op_mfpscri;
		struct op_sc		op_sc;
		unsigned long		l;
};

#endif			/* PPCDIS_INST_FMT_H */
