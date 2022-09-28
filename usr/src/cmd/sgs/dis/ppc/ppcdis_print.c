/*
 * Copyright (c) 1994-1996, Sun Microsystems, Inc.  All Rights Reserved.
 */

#pragma ident	"@(#)ppcdis_print.c	1.4	96/07/22 SMI"

#include "ppcdis_extern.h"
#include "ppcdis_branch.h"
#if defined(DEBUG)
#include "ppcdis_debug.h"			/* various debug flags */
#endif	/* defined(DEBUG) */

/*
 * PowerPC Disassembler - Print Routines for Various Instruction Formats
 *
 * routines organized by instruction formats, as defined in the
 * PowerPC User Instruction Set Architecture, Book I, vsn. 1.05.
 *
 */

/*
 * print D-Format Instruction.
 * EXAMPLES: mulli	%rD,%rA,SIMM
 *			 andi.	%rA,%rS,UIMM
 */
void
printi_D(register char *op, register int rD, register int rA,
	register short D, register int optype)
{
	PrintF("%s\t%s%d,%s%d,", op, REG, rD, REG, rA);
	(optype == OP_SIMM) ? print_SIMM(D, YES_EOL) : print_UIMM(D);
}


/*
 * print D-Format Instruction. (effective address)
 * EXAMPLES: lbz	%rD,d(%rA)
 *			 lfd	%frD,d(%rA)
 *			 stfu	%frS,d(%rA)
 */
void
printi_D_EA(register char *op, register int rD, register int rA,
	register short D, register int regtype)
{
	PrintF("%s\t%s%d,", op, (regtype == FLOTREG) ? FREG : REG, rD);
	print_SIMM(D, NO_EOL);				/* signed integer */
	PrintF("(%s%d)\n", REG, rA);
}


/*
 * print X-Format Instruction.	(two register arguments)
 * EXAMPLES: dcbt	%rA,%rB (effective address = rA|0 + rB)
 *			 extsb	%rA,%rS
 *			 fabs.	%fD,%fB
 */
void
printi_X2(register char *op, register int rA, register int rB,
	register int chkRc, register int regtype)
{
	register char *preg;

	preg = (regtype == GENREG) ? REG : FREG;

	PrintF("%s", op);
	if (chkRc > NO_RC)
		check_Rc(chkRc);
	PrintF("\t%s%d,%s%d\n", preg, rA, preg, rB);
}


/*
 * print X-Format Instruction.	(three register arguments)
 * EXAMPLES: and.	%rA,%rS,%rB
 *			 lfsux	%fD,%rA,%rB
 */
void
printi_X3(register char *op, register int rA, register int rB,
	register int rD, register int chkRc, register int regtype)
{
	PrintF("%s", op);

	if (chkRc > NO_RC)
		check_Rc(chkRc);

	PrintF("\t%s%d,%s%d,%s%d\n", (regtype == GENREG) ? REG : FREG,
		rA, REG, rB, REG, rD);
}


/*
 * print XO-Format Instruction.	(two register arguments)
 * EXAMPLES: subfme	%rD,%rA
 *			 addme.	%rD,%rA
 */
void
printi_XO2(register char *op, register struct op_imath xo)
{
	PrintF("%s", op);

	check_OE(xo.OE);
	check_Rc(xo.Rc);

	PrintF("\t%s%d,%s%d\n", REG, xo.rD, REG, xo.rA);
}


/*
 * print X-Format Instruction.	(three register arguments)
 * EXAMPLES: mullwo	%rD,%rA,%rB
 *			 subfc.	%rD,%rA,%rB
 */
void
printi_XO3(register char *op, register struct op_imath xo)
{
	PrintF("%s", op);

	check_OE(xo.OE);
	check_Rc(xo.Rc);

	PrintF("\t%s%d,%s%d,%s%d\n", REG, xo.rD, REG, xo.rA, REG, xo.rB);
}


/*
 * print X-Format Instruction.	(three register arguments)
 * EXAMPLES: 	sub	%rD,%rB,%rA	(%rA and %rB reversed)
 *			 subfc.	%rD,%rB,%rA
 */
void
printi_XO3r(register char *op, register struct op_imath xo)
{
	PrintF("%s", op);

	check_OE(xo.OE);
	check_Rc(xo.Rc);

	PrintF("\t%s%d,%s%d,%s%d\n", REG, xo.rD, REG, xo.rB, REG, xo.rA);
}


/*
 * print A-Format Instruction.	(three register arguments)
 * EXAMPLES: fdivs.	%fD,%fA,%fB
 *			 fmul.	%fD,%fA,%fC
 */
void
printi_A3(register char *op, register int fA, register int fB,
	register int fC, register int chkRc)
{
	PrintF("%s", op);

	check_Rc(chkRc);

	PrintF("\t%s%d,%s%d,%s%d\n", FREG, fA, FREG, fB, FREG, fC);
}


/*
 * print A-Format Instruction.	(four register arguments)
 * EXAMPLES: fnmadds.	%fD,%fA,%fC,%fB
 *			 fnmsub	%fD,%fA,%fC,%fB
 */
void
printi_A4(register char *op, register int fD, register int fA, register int fC,
	register int fB, register int chkRc)
{
	PrintF("%s", op);

	check_Rc(chkRc);

	PrintF("\t%s%d,%s%d,%s%d,%s%d\n", FREG, fD, FREG, fA, FREG, fC, FREG,
	    fB);
}


/*
 * print XL-Format Instruction.	(three register arguments)
 * EXAMPLES: crand	crbD,crbA,crbB
 *			 fmul.	%fD,%fA,%fC
 */
void
printi_XL3(register char *op, register struct op_bool xl)
{
	PrintF("%s\t", op);
	print_crb(xl.crbD, CRB_COMMA);
	print_crb(xl.crbA, CRB_COMMA);
	print_crb(xl.crbB, CRB_EOL);
}
