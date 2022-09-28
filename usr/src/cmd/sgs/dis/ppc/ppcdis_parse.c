/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ppcdis_parse.c	1.14	96/07/22 SMI"

/*
 * PowerPC Disassembler - Main Instruction Parser
 */
#include <sys/types.h>
#include <stdarg.h>
#include "ppcdis_mnemonics.h"		/* the mnemonic dictionary	*/
#include "ppcdis_opcodes.h"		/* opcode define's		*/
#include "ppcdis_inst_fmt.h"		/* instruction format		*/
#include "ppcdis_branch.h"		/* branch conditions		*/
#include "ppcdis_trap.h"		/* trap conditions		*/
#include "ppcdis_extern.h"		/* extern function def's	*/
#include "ppcdis_regs.h"		/* SPR assignments		*/

#if ! defined(DIS)
#include "adb.h"
#include "symtab.h"
#endif	/* ! defined(DIS) */

#if defined(DEBUG)
#include "ppcdis_debug.h"		/* various debug flags		*/
#endif	/* defined(DEBUG) */

#define	ERROR	-1
#define	xIMM	inst.op_imm
#define	xTYP	inst.op_typ
#define	xMATH	inst.op_imath
#define	xFP	inst.op_fmath
#define	xFCNV	inst.op_fpconv

int DEBUG_LEVEL = 0x0;			/* debug level indicator	*/
int mflag = 0;				/* decimal print flag		*/
char savtarget[80];			/* symbol output buffer		*/
char target[80];			/* symbol output buffer		*/
int lastsym_addr;

/*
 * The buffer, pointer and routines below are support routines for disassembly
 */

static char dResult[128];
static char *bufp = NULL;

void
PrintF(const char *fmt, ...)
{
	va_list vp;

	va_start(vp, fmt);

	if (bufp == NULL)
		bufp = dResult;
	vsprintf(bufp, fmt, vp);
	bufp = dResult + strlen(dResult);

	va_end(vp);
}

/*
 * Main (dis)assembly line......
 */

char *
disassemble(register union op inst, register ulong pc)
{
	register ushort ppc_op1 = (ushort)inst.op_typ.op;
	register ushort ppc_op2;
	register int tmpvar;
	register short IMM;		/* SIMM/UIMM 16-bit immediate operand */
	register int BO;		/* BO - branch operand */
	register int BI;		/* BI - branch condition operand */
	register int BD;		/* BD - branch displacement */
	register int CR;		/* a single Condition Register field */
	register int CRnum;		/* Condition Register field number */
	char *symbufp;			/* local pointer to symbol buffer */

	bufp = NULL;			/* Reset the pointer at every call */
/*
 * temporary hack to provide numeric labels and display original numeric
 * instruction.
 */
#if defined(DIS)	/* DIS */
	symbufp = target;
	extsympr(pc, &symbufp);
	/*
	 * if the returned symbol buffer contains an address, then
	 * use the saved symbol, and print the address as "symbol+offset:"
	 * Otherwise, print the new symbol, and save it in other buffer.
	 */
	if (strncmp(target, "0x", 2) == 0) {	/* address */
		BD = pc - lastsym_addr;
		tmpvar = strlen(savtarget);
		PrintF("0x%lx:\t%s+0x%lx:\t0x%lx\t", pc, savtarget, BD, inst.l);
	} else {				/* found a new symbol */
		lastsym_addr = pc;
		strcpy(savtarget, target);
		PrintF("0x%lx:\t%s:     \t0x%lx\t", pc, target, inst.l);
	}
#endif				/* defined(DIS) */

#if defined(DEBUG)
	if (DEBUG_LEVEL & DEBUG_MAIN_PARSER)
		PrintF("(0x%lx) instruction: 0x%lx\t",
		    (unsigned long)pc, (ulong)inst.l);
#endif	/* defined(DEBUG) */

	switch (ppc_op1) {

	case PPC_OP_3:				/* twi */
/*
 * vla fornow......
 * need to add an instruction format check - unconditional trap is
 * invalid with this instruction?
 */
		parse_TO(xIMM.rD);
		PrintF("i\t%s%d,", REG, xIMM.rA);
		print_SIMM((short)xIMM.D, YES_EOL);	/* signed integer */
		break;

	case PPC_OP_7:				/* mulli */
		printi_D(PPC_INST_7, xIMM.rD, xIMM.rA, (short)xIMM.D, OP_SIMM);
		break;

	case PPC_OP_8:				/* subfic */
		printi_D(PPC_INST_8, xIMM.rD, xIMM.rA, (short)xIMM.D, OP_SIMM);
		break;

	case PPC_OP_9:				/* dozi */
		print_POWER();
		printi_D(PPC_INST_9, xIMM.rD, xIMM.rA, (short)xIMM.D, OP_SIMM);
		break;

	case PPC_OP_10:				/* cmpli */
						/* SIMM=UIMM */
		if (inst.op_cmpi.L == 1)
#if defined(MC98601)
			PrintF("64-bit compares not implemented on the %s.\n",
			    "MC98601");
#else	/* !defined(MC98601) */
						{
			PrintF("cmpldi\t");

			/* destination is assumed to be CR0 if not specified. */
			if (inst.op_cmpi.crD != 0)
				PrintF("crf%d,", inst.op_cmpi.crD);

			PrintF("%s%d,", REG, inst.op_cmpi.rA);
			print_SIMM((short)inst.op_cmpi.SIMM, YES_EOL);
		}
#endif	/* defined(MC98601) */
		else {				/* 32-bit operands */
			PrintF("cmplwi\t");

			/* destination is assumed to be CR0 if not specified. */
			if (inst.op_cmpi.crD != 0)
				PrintF("crf%d,", inst.op_cmpi.crD);

				PrintF("%s%d,", REG, inst.op_cmpi.rA);
				print_SIMM((short)inst.op_cmpi.SIMM, YES_EOL);
		}
		break;

	case PPC_OP_11:				/* cmpi */
						/* signed decimal */
/*
 * vla fornow.....
 * XXX does not handle double-word comparisons, which are only available
 * in 64-bit implementations of this architecture, i.e., not the MC98601.
 */
		if (inst.op_cmpi.L == 1)
#if defined(MC98601)
			PrintF("64-bit compares not implemented on the %s.\n",
			    "MC98601");
#else	/* !defined(MC98601) */
						{
			PrintF("cmpdi\t");

			/* destination is assumed to be CR0 if not specified. */
			if (inst.op_cmpi.crD != 0)
				PrintF("crf%d,", inst.op_cmpi.crD);

			PrintF("%s%d,", REG, inst.op_cmpi.rA);
			print_SIMM((short)inst.op_cmpi.SIMM, YES_EOL);
		}
#endif	/* defined(MC98601) */
		else {				/* 32-bit operands */
			PrintF("cmpwi\t");

			/* destination is assumed to be CR0 if not specified. */
			if (inst.op_cmpi.crD != 0)
				PrintF("crf%d,", inst.op_cmpi.crD);

			PrintF("%s%d,", REG, inst.op_cmpi.rA);
			print_SIMM((short)inst.op_cmpi.SIMM, YES_EOL);
		}
		break;

	case PPC_OP_12:				/* addic */
						/* D=SIMM (signed decimal) */
		tmpvar = (short)xIMM.D;
		if (tmpvar >= 0)
			printi_D(PPC_INST_12, xIMM.rD, xIMM.rA, tmpvar,
			    OP_SIMM);

		else {			/* use the extended mmenomic "subic" */
			/*
			 * simplified mnemonic, recommended by 601 User's Manual
			 * if immediate argument is negative, use the "subic"
			 * extended mnemonic, and the absolute value of the
			 * immediate operand.
			 */
			printi_D("subic", xIMM.rD, xIMM.rA, abs(tmpvar),
			    OP_UIMM);
		}
		break;

	case PPC_OP_13:				/* addic. */
						/* D=SIMM (signed integer) */
		tmpvar = (short)xIMM.D;
		if (tmpvar >= 0)
			printi_D(PPC_INST_13, xIMM.rD, xIMM.rA, tmpvar,
			    OP_SIMM);

		else {			/* use the extended mnemonic "subic." */
			/*
			 * simplified mnemonic, recommended by 601 User's Manual
			 * if immediate argument is negative, use the "subic."
			 * extended mnemonic, and the absolute value of the
			 * immediate operand.
			 */
			printi_D("subic.", xIMM.rD, xIMM.rA, abs(tmpvar),
			    OP_UIMM);
		}
		break;

	case PPC_OP_14:				/* addi */
						/* D=SIMM (signed integer) */
		/*
		 * XXX vla fornow.....
		 * when we add symbol table support, we can also use the
		 * extended mnemonic for la Rx,D(Ry); i.e., base-displacement
		 * addressing.
		/*
		 * The addi instruction may be used to load an immediate
		 * value into a register.  Use the extended mnemonic in
		 * this case, (i.e., data movement, no addition.)
		 */
		tmpvar = (short)xIMM.D;
		if (xIMM.rA == 0) {		/* li extended mnemonic */
			PrintF("li\t%s%d,", REG, xIMM.rD);
			print_SIMM(tmpvar, YES_EOL);	/* signed integer */
		} else if ((short)xIMM.rD < 0) { /* subi ext'd mnemonic */
			/*
			 * simplified mnemonic, recommended by 601 User's Manual
			 * if immediate argument is negative, use the "subi"
			 * extended mnemonic, and the absolute value of the
			 * immediate operand.
			 */
			printi_D("subi", xIMM.rD, xIMM.rA, abs(tmpvar),
			    OP_UIMM);
		} else 		/* default - use the standard mnemonic */
			printi_D(PPC_INST_14, xIMM.rD, xIMM.rA, tmpvar,
			    OP_SIMM);
		break;

	case PPC_OP_15:				/* addis */
						/* D=SIMM (signed integer) */
		/*
		 * The addis instruction may be used to load an immediate
		 * value into a register.  Use the extended mnemonic in
		 * this case, (i.e., data movement, no addition.)
		 * NOTE: disable the extended mnemonic in the special case
		 * where both rA and SIMM are 0; this is usually part of an
		 * address load instruction.
		 */
		tmpvar = (short)xIMM.D;

#if 0
		if ((xIMM.rA == 0) && (tmpvar != 0)) { /* lis ext'd mnemonic */
#endif
		if (xIMM.rA == 0) {		/* lis ext'd mnemonic */
			PrintF("lis\t%s%d,", REG, xIMM.rD);
#if defined(DIS)
			PrintF("%#010lx\n", tmpvar);
#else	/* !defined(DIS) adb/kadb */
			PrintF("0x%x\n", tmpvar);
#endif	/* defined(DIS) */
		} else if (tmpvar < 0) {	/* subis ext'd mnemonic */
			/*
			 * simplified mnemonic, recommended by 601 User's Manual
			 * if immediate argument is negative, use the "subis"
			 * extended mnemonic, and the absolute value of the
			 * immediate operand.
			 */
			printi_D("subis", xIMM.rD, xIMM.rA, abs(tmpvar),
			    OP_UIMM);
		} else 		/* default - use the standard mnemonic */
			printi_D(PPC_INST_15, xIMM.rD, xIMM.rA, tmpvar,
			    OP_SIMM);
		break;

	case PPC_OP_16:				/* bcx */
		BO = (int)inst.op_brc.BO;
		BI = (int)inst.op_brc.BI;
		BD = inst.op_brc.BD << 2;
		CR = BI % 4;			/* CR flag number */
		CRnum = BI / 4;			/* CR field number */
		parse_BO(BO, CR);
		check_LK(inst.op_brc.LK);
		check_AA(inst.op_brc.AA);
/*
 * vla fornow - do I have to change the sense of this +/-, depending upon
 * whether BD is positive/negative?
 */
		/*
		 * check branch prediction flag
		 */
		if (BO & PPC_BRANCH_YES)	/* default: branch not taken */
			PrintF("+");		/* if BD is non-negative */
		else
			PrintF("-");
		PrintF("\t");
		if (CRnum)
			PrintF("cr%d, ", CRnum);

		if (!inst.op_brc.AA)	/* compute relative branch dest */
			BD += pc;

/* XXXPPC do we have to sign-extend this value to 64 bits? */
		print_dest(BD, pc);		/* evaluate relocation symbol */
		break;

	case PPC_OP_17:				/* sc */
		PrintF("%s\n", PPC_INST_17);
		break;

	case PPC_OP_18:				/* bx */
		BD = inst.op_br.LI << 8;
		BD >>= 6;
		PrintF("b");
		check_LK(inst.op_br.LK);
		check_AA(inst.op_br.AA);
		PrintF("\t");

		if (!inst.op_brc.AA)	/* compute relative branch dest */
			BD += pc;

		print_dest(BD, inst.op_br.AA ? BD : pc); /* evaluate relocation symbol */
		break;

	case PPC_OP_19:
		parse_19(inst, (ushort)inst.op_bool.XO);
		break;

	case PPC_OP_20:				/* rlwimix */
		/*
		 * The rlwimi instruction may be used to insert a right- or
		 * left-justified field of n bits from a source register to
		 * a target register, starting at bit position b; other bits
		 * in the target register are unchanged.  Use the extended
		 * mnemonic for this case to clarify this usage.
		 */

		tmpvar = inst.op_rot.SH + inst.op_rot.MB;
		if (tmpvar == WORD_SIZE) {	/* insert from left immediate */
			tmpvar = inst.op_rot.ME + 1 - inst.op_rot.MB;
			PrintF("inslwi");
			check_Rc(inst.op_rot.Rc);
			PrintF("\t%s%d,%s%d,0x%x,0x%x\n", REG, inst.op_rot.rA,
			    REG, inst.op_rot.rS, tmpvar, inst.op_rot.MB);
		} else if ((inst.op_rot.SH + inst.op_rot.ME) ==
		    (WORD_SIZE - 1)) { /* insert from right immediate */
			tmpvar = (inst.op_rot.ME + 1) - inst.op_rot.MB;
			PrintF("insrwi");
			check_Rc(inst.op_rot.Rc);
			PrintF("\t%s%d,%s%d,0x%x,0x%x\n", REG, inst.op_rot.rA,
			    REG, inst.op_rot.rS, tmpvar, inst.op_rot.MB);
		} else {
			PrintF("%s", PPC_INST_20);
			check_Rc(inst.op_rot.Rc);
			PrintF("\t%s%d,%s%d,0x%x,0x%x,0x%x\n", REG,
			    inst.op_rot.rA, REG, inst.op_rot.rS, inst.op_rot.SH,
			    inst.op_rot.MB, inst.op_rot.ME);
		}
		break;

	case PPC_OP_21:				/* rlwinmx */
		/*
		 * The rlwinm instruction may be used in many ways to rotate
		 * and/or shift register contents.  Extended mnemonics will
		 * clarify some of the various types of operations:
		 *   extract - select n bits starting at position b in the
		 *             source register; rt or lft justify in target
		 *             register; clear other target bits to zero.
		 *   insert  - select n bits (rt or lft justified) in source
		 *             register; insert this field in target register,
		 *             starting at bit position b.  Other bits
		 *             unchanged.
		 *   rotate  - rotate register contents without masking.
		 *   shift   - shift register contents, clearing vacated bits.
		 *   clear   - clear the left- or right-most bits to zero.
		 *   clear left and shift left -  clear leftmost b bits, then
		 *             shift left by n bits; used to scale an array
		 *             index by the width of an element.
		 */
		if (inst.op_rot.MB == 0) { /* check for extended mnemonics */
			if (inst.op_rot.ME == (WORD_SIZE - 1)) { /* rot imm */
				if (inst.op_rot.SH < HWORD_SIZE) {
					PrintF("rotlwi");
					check_Rc(inst.op_rot.Rc);
					PrintF("\t%s%d,%s%d,0x%x\n", REG,
					    inst.op_rot.rA, REG,
					    inst.op_rot.rS, inst.op_rot.SH);
				} else {
					PrintF("rotrwi");
					check_Rc(inst.op_rot.Rc);
					PrintF("\t%s%d,%s%d,0x%x\n", REG,
					    inst.op_rot.rA, REG,
					    inst.op_rot.rS, inst.op_rot.SH);
				}
			} else if (inst.op_rot.SH == 0) { /* clear rt imm */
				PrintF("clrrwi");
				check_Rc(inst.op_rot.Rc);
				PrintF("\t%s%d,%s%d,0x%x\n", REG,
				    inst.op_rot.rA, REG, inst.op_rot.rS,
				    (WORD_SIZE - 1) - inst.op_rot.ME);
			} else if ((inst.op_rot.SH + inst.op_rot.ME) ==
			    (WORD_SIZE - 1)) { /* shift left immediate */
				PrintF("slwi");
				check_Rc(inst.op_rot.Rc);
				PrintF("\t%s%d,%s%d,0x%x\n", REG,
				    inst.op_rot.rA, REG, inst.op_rot.rS,
				    inst.op_rot.SH);
			} else { /* extract and left justify immediate */
				PrintF("extlwi");
				check_Rc(inst.op_rot.Rc);
				PrintF("\t%s%d,%s%d,0x%x,0x%x\n", REG,
				    inst.op_rot.rA, REG, inst.op_rot.rS,
				    inst.op_rot.ME + 1, inst.op_rot.SH);
			}
		} else if (inst.op_rot.ME == (WORD_SIZE - 1)) {
			if (inst.op_rot.SH == 0) { /* clear left immediate */
				PrintF("clrlwi");
				check_Rc(inst.op_rot.Rc);
				PrintF("\t%s%d,%s%d,0x%x\n", REG,
				    inst.op_rot.rA, REG, inst.op_rot.rS,
				    inst.op_rot.MB);
			} else if ((inst.op_rot.SH + inst.op_rot.MB) ==
			    WORD_SIZE) { /* shift right immediate */
				PrintF("srwi");
				check_Rc(inst.op_rot.Rc);
				PrintF("\t%s%d,%s%d,0x%x\n", REG,
				    inst.op_rot.rA, REG, inst.op_rot.rS,
				    inst.op_rot.MB);
			} else { /* extract and right justify immediate */
				tmpvar = WORD_SIZE - inst.op_rot.MB;
				PrintF("extrwi");
				check_Rc(inst.op_rot.Rc);
				PrintF("\t%s%d,%s%d,0x%x,0x%x\n", REG,
				    inst.op_rot.rA, REG, inst.op_rot.rS,
				    tmpvar, inst.op_rot.SH - tmpvar);
			}
		} else if ((inst.op_rot.SH + inst.op_rot.ME) ==
		    (WORD_SIZE - 1)) { /* clear left and shift lft imm */
			PrintF("clrlslwi");
			check_Rc(inst.op_rot.Rc);
			PrintF("\t%s%d,%s%d,0x%x,0x%x\n", REG, inst.op_rot.rA,
			    REG, inst.op_rot.rS, inst.op_rot.MB +
			    inst.op_rot.SH, inst.op_rot.SH);
		} else {
			PrintF("%s", PPC_INST_21);
			check_Rc(inst.op_rot.Rc);
			PrintF("\t%s%d,%s%d,0x%x,0x%x,0x%x\n", REG,
			    inst.op_rot.rA, REG, inst.op_rot.rS, inst.op_rot.SH,
			    inst.op_rot.MB, inst.op_rot.ME);
		}
		break;

	case PPC_OP_23:				/* rlwnmx */
		/*
		 * The rlwnm instruction may be used to rotate the contents of
		 * a register left or right n bits without masking.  Use the
		 * extended mnemonic to indicate this usage.
		 */
		if (inst.op_rot.MB == 0 &&
		    inst.op_rot.ME == (WORD_SIZE - 1)) { /* rotate left */
			PrintF("rotlw");
			check_Rc(inst.op_rot.Rc);
			PrintF("\t%s%d,%s%d,0x%x\n", REG, inst.op_rot.rA, REG,
			    inst.op_rot.rS, inst.op_rot.SH);
		} else {
			PrintF("%s", PPC_INST_23);
			check_Rc(inst.op_rot.Rc);
			PrintF("\t%s%d,%s%d,%d,0x%x,0x%x\n", REG,
			    inst.op_rot.rA, REG, inst.op_rot.rS, inst.op_rot.SH,
			    inst.op_rot.MB, inst.op_rot.ME);
		}
		break;

	case PPC_OP_24:				/* ori */
						/* D=UIMM, rD=rS */
		/*
		 * check for special case - ori 0,0,0 is the preferred
		 * form of "no-op"; if this is the case, use extended mnemonic.
		 */
		if (((tmpvar = xIMM.rA) == 0) && (tmpvar == xIMM.rD) &&
		    (tmpvar == xIMM.D))
			PrintF("nop\n");
		/*
		 * The ori instruction may also be used to move the contents of
		 * one register to another.  Use the extended mnemonic in this
		 * case to clarify this usage, (i.e., data movement, not a
		 * logical operation).
		 * NOTE: this differs somewhat from the orx usage; this form
		 * contains no Record Bit; i.e., no side effect to CR.
		 * NOTE2: disable the extended mnemonic in the special case
		 * where both rA and UIMM are 0; this is usually part of an
		 * address load instruction.
		 */
		else if ((xIMM.D == 0) && (tmpvar != xIMM.rD))
			printi_X2("mr", tmpvar, xIMM.rD, NO_RC, GENREG);
#if 0
		else if (tmpvar == xIMM.rD) {
			PrintF("la\t%s%d,", REG, tmpvar);
#if defined(DIS)
			PrintF("%#010lx\n", xIMM.D);
#else	/* !defined(DIS) adb/kadb */
			PrintF("0x%x\n", xIMM.D);
#endif	/* defined(DIS) */
		}
#endif
		else
			printi_D(PPC_INST_24, tmpvar, xIMM.rD, (short)xIMM.D,
			    OP_UIMM);
		break;

	case PPC_OP_25:				/* oris */
						/* D=UIMM, rD=rS */
		printi_D(PPC_INST_25, xIMM.rA, xIMM.rD, (short)xIMM.D, OP_UIMM);
		break;

	case PPC_OP_26:				/* xori */
						/* D=UIMM, rD=rS */
		printi_D(PPC_INST_26, xIMM.rA, xIMM.rD, (short)xIMM.D, OP_UIMM);
		break;

	case PPC_OP_27:				/* xoris */
						/* D=UIMM, rD=rS */
		printi_D(PPC_INST_27, xIMM.rA, xIMM.rD, (short)xIMM.D, OP_UIMM);
		break;

	case PPC_OP_28:				/* andi. */
						/* D=UIMM, rD=rS */
		printi_D(PPC_INST_28, xIMM.rA, xIMM.rD, (short)xIMM.D, OP_UIMM);
		break;

	case PPC_OP_29:				/* andis. */
						/* D=UIMM, rD=rS */
		printi_D(PPC_INST_29, xIMM.rA, xIMM.rD, (short)xIMM.D, OP_UIMM);
		break;

	case PPC_OP_31:				/* xx */
		ppc_op2 = (ushort)inst.op_typ.XO;
		if (parse_31(inst, ppc_op2) == ERROR) {
			ppc_op2 = (ushort)inst.op_imath.XO;
			if (parse_31(inst, ppc_op2) == ERROR) {
				PrintF(
				    "OPCODE 31: invalid extended opcode %d\n",
				    ppc_op2);
			}
		}
		break;

	case PPC_OP_32:				/* lwz */
		printi_D_EA(PPC_INST_32, xIMM.rD, xIMM.rA, (short)xIMM.D,
		    GENREG);
		break;

	case PPC_OP_33:				/* lwzu */
		printi_D_EA(PPC_INST_33, xIMM.rD, xIMM.rA, (short)xIMM.D,
		    GENREG);
		break;

	case PPC_OP_34:				/* lbz */
		printi_D_EA(PPC_INST_34, xIMM.rD, xIMM.rA, (short)xIMM.D,
		    GENREG);
		break;

	case PPC_OP_35:				/* lbzu */
		printi_D_EA(PPC_INST_35, xIMM.rD, xIMM.rA, (short)xIMM.D,
		    GENREG);
		break;

	case PPC_OP_36:				/* stw */
						/* rD=rS */
		printi_D_EA(PPC_INST_36, xIMM.rD, xIMM.rA, (short)xIMM.D,
		    GENREG);
		break;

	case PPC_OP_37:				/* stwu */
						/* rD=rS */
		printi_D_EA(PPC_INST_37, xIMM.rD, xIMM.rA, (short)xIMM.D,
		    GENREG);
		break;

	case PPC_OP_38:				/* stb */
						/* rD=rS */
		printi_D_EA(PPC_INST_38, xIMM.rD, xIMM.rA, (short)xIMM.D,
		    GENREG);
		break;

	case PPC_OP_39:				/* stbu */
						/* rD=rS */
		printi_D_EA(PPC_INST_39, xIMM.rD, xIMM.rA, (short)xIMM.D,
		    GENREG);
		break;

	case PPC_OP_40:				/* lhz */
		printi_D_EA(PPC_INST_40, xIMM.rD, xIMM.rA, (short)xIMM.D,
		    GENREG);
		break;

	case PPC_OP_41:				/* lhzu */
		printi_D_EA(PPC_INST_41, xIMM.rD, xIMM.rA, (short)xIMM.D,
		    GENREG);
		break;

	case PPC_OP_42:				/* lha */
		printi_D_EA(PPC_INST_42, xIMM.rD, xIMM.rA, (short)xIMM.D,
		    GENREG);
		break;

	case PPC_OP_43:				/* lhau */
		printi_D_EA(PPC_INST_43, xIMM.rD, xIMM.rA, (short)xIMM.D,
		    GENREG);
		break;

	case PPC_OP_44:				/* sth */
						/* rD=rS */
		printi_D_EA(PPC_INST_44, xIMM.rD, xIMM.rA, (short)xIMM.D,
		    GENREG);
		break;

	case PPC_OP_45:				/* sthu */
						/* rD=rS */
		if (xIMM.rA == 0) {
#if defined(MC98601)
			PrintF("WARNING: invalid PPC instruction for %s...\n",
			    "MC90601");
#else	/* !defined(MC98601) */
			PrintF("ERROR: rA = 0 is invalid for sthu %s.\n",
			    "instruction");
#endif	/* defined(MC98601) */
		}
		printi_D_EA(PPC_INST_45, xIMM.rD, xIMM.rA, (short)xIMM.D,
		    GENREG);
		break;

	case PPC_OP_46:				/* lmw */
		printi_D_EA(PPC_INST_46, xIMM.rD, xIMM.rA, (short)xIMM.D,
		    GENREG);
		break;

	case PPC_OP_47:				/* stmw */
						/* rD=rS */
		printi_D_EA(PPC_INST_47, xIMM.rD, xIMM.rA, (short)xIMM.D,
		    GENREG);
		break;

	case PPC_OP_48:				/* lfs */
						/* rD=frD */
		printi_D_EA(PPC_INST_48, xIMM.rD, xIMM.rA, (short)xIMM.D,
		    FLOTREG);
		break;

	case PPC_OP_49:				/* lfsu */
						/* rD=frD */
		printi_D_EA(PPC_INST_49, xIMM.rD, xIMM.rA, (short)xIMM.D,
		    FLOTREG);
		break;

	case PPC_OP_50:				/* lfd */
						/* rD=frD */
		printi_D_EA(PPC_INST_50, xIMM.rD, xIMM.rA, (short)xIMM.D,
		    FLOTREG);
		break;

	case PPC_OP_51:				/* lfdu */
						/* rD=frD */
		printi_D_EA(PPC_INST_51, xIMM.rD, xIMM.rA, (short)xIMM.D,
		    FLOTREG);
		break;

	case PPC_OP_52:				/* stfs */
						/* rD=frS */
		printi_D_EA(PPC_INST_52, xIMM.rD, xIMM.rA, (short)xIMM.D,
		    FLOTREG);
		break;

	case PPC_OP_53:				/* stfsu */
						/* rD=frS */
		printi_D_EA(PPC_INST_53, xIMM.rD, xIMM.rA, (short)xIMM.D,
		    FLOTREG);
		break;

	case PPC_OP_54:				/* stfd */
						/* rD=frS */
		printi_D_EA(PPC_INST_54, xIMM.rD, xIMM.rA, (short)xIMM.D,
		    FLOTREG);
		break;

	case PPC_OP_55:				/* stfdu */
						/* rD=frS */
		printi_D_EA(PPC_INST_55, xIMM.rD, xIMM.rA, (short)xIMM.D,
		    FLOTREG);
		break;

	case PPC_OP_59:				/* xx */
		parse_59(inst, (ushort)inst.op_fmath.XO);
		break;

	case PPC_OP_63:				/* xx */
		ppc_op2 = (ushort)inst.op_typ.XO;
		if (parse_63(inst, ppc_op2) == ERROR) {

			ppc_op2 = (ushort)inst.op_fmath.XO;
			if (parse_63(inst, ppc_op2) == ERROR) {
				PrintF("OPCODE 63: invalid extended %s%d\n",
				    "opcode ", ppc_op2);
			}
		}
		break;

	default:				/* invalid primary opcodes */
		PrintF("invalid opcode %d\n", ppc_op1);
		break;

	}
	return (dResult);
}


void
parse_19(register union op inst, register ushort ppc_op2)
{
register int BO;		/* BO - branch operand */
register int BI;		/* BI - branch condition operand */
register int TF;		/* TF - branch if TRUE/FALSE condition */
register int CR;		/* a single Condition Register field */
register int CRnum;		/* Condition Register field number */

#if defined(DEBUG)
	if (DEBUG_LEVEL & DEBUG_PARSE_19)
		PrintF("Beginning parse_19: opcode: %d  (0x%lx)\n",
		    ppc_op2, (unsigned long)inst.l);
#endif	/* defined(DEBUG) */

	switch (ppc_op2) {

	case PPC_OP19_0:			/* mcrf */
		PrintF("%s\tcrf%d,crf%d\n", PPC_INST19_0,
				inst.op_mcrf.BF, inst.op_mcrf.BFA);
		break;

	case PPC_OP19_16:			/* bclrx */
		BO = (int)inst.op_brc.BO;
		BI = (int)inst.op_brc.BI;
		CR = BI % 4;		/* retrieve single CR field */
		CRnum = BI / 4;		/* compute CR field number */
		parse_BO(BO, CR);
		PrintF("lr");
		check_LK(inst.op_brc.LK);
		/*
		 * check branch prediction flag
		 */
		if (BO & PPC_BRANCH_YES)	/* default: branch not taken */
			PrintF("+");
		else
			PrintF("-");
		if (CRnum)		/* CR0 assumed if not specified */
			PrintF("\tcrf%d\n", CRnum);
		else
			PrintF("\n");

		/* no destination, target addr is in link register..... */
		break;

	case PPC_OP19_33:			/* crnor */
		/*
		 * The crnor instruction may be used to invert a given
		 * Condition Register bit.  Use the extended mnemonic
		 * to clarify this usage.
		 */
		if (inst.op_bool.crbA == inst.op_bool.crbB) { /* crnot */
			PrintF("crnot\t");
			print_crb((int)inst.op_bool.crbD, CRB_COMMA);
			print_crb((int)inst.op_bool.crbA, CRB_EOL);
		} else		/* default - use the standard mnemonic */
			printi_XL3(PPC_INST19_33, inst.op_bool);
		break;

	case PPC_OP19_50:			/* rfi */
		/*
		 * This is a supervisor-level, context-synchronizing
		 * instruction.
		 */
		PrintF("%s\n", PPC_INST19_50);
		break;

	case PPC_OP19_129:			/* crandc */
		printi_XL3(PPC_INST19_129, inst.op_bool);
		break;

	case PPC_OP19_150:			/* isync */
		/*
		 * This is a context-synchronizing instruction.
		 */
		if (inst.op_typ.Rc || inst.op_typ.rD ||
		    inst.op_typ.rB || inst.op_typ.rB)
			PrintF("Invalid `isync' instruction format.\n");
		else
			PrintF("%s\n", PPC_INST19_150);
		break;

	case PPC_OP19_193:			/* crxor */
		/*
		 * The crxor instruction may be used to clear a given Condition
		 * Register bit (=0).  Use the extended mnemonic to clarify
		 * this usage.
		 */
		if (inst.op_bool.crbD == inst.op_bool.crbA &&
		    inst.op_bool.crbD == inst.op_bool.crbB) { /* crclr */
			PrintF("crclr\t");
			print_crb((int)inst.op_bool.crbD, CRB_EOL);
		} else		/* default - use the standard mnemonic */
			printi_XL3(PPC_INST19_193, inst.op_bool);
		break;

	case PPC_OP19_225:			/* crnand */
		printi_XL3(PPC_INST19_225, inst.op_bool);
		break;

	case PPC_OP19_257:			/* crand */
		printi_XL3(PPC_INST19_257, inst.op_bool);
		break;

	case PPC_OP19_289:			/* creqv */
		/*
		 * The creqv instruction may be used to set a given Condition
		 * Register bit (=1).  Use the extended mnemonic to clarify
		 * this usage.
		 */
		if (inst.op_bool.crbD == inst.op_bool.crbA &&
		    inst.op_bool.crbD == inst.op_bool.crbB) { /* crset */
			PrintF("crset\t");
			print_crb(inst.op_bool.crbD, CRB_EOL);
		} else		/* default - use the standard mnemonic */
			printi_XL3(PPC_INST19_289, inst.op_bool);
		break;

	case PPC_OP19_417:			/* crorc */
		printi_XL3(PPC_INST19_417, inst.op_bool);
		break;

	case PPC_OP19_449:			/* cror */
		/*
		 * The cror instruction may be used to copy a given Condition
		 * Register bit.  Use the extended mnemonic to clarify
		 * this usage.
		 */
		if (inst.op_bool.crbA == inst.op_bool.crbB) { /* crmove */
			PrintF("crmove\t");
			print_crb(inst.op_bool.crbD, CRB_COMMA);
			print_crb(inst.op_bool.crbB, CRB_EOL);
		} else		/* default - use the standard mnemonic */
			printi_XL3(PPC_INST19_449, inst.op_bool);
		break;

	case PPC_OP19_528:			/* bcctrx */
		/*
		 * don't call parse_BO for this instruction - most of simple
		 * branch mnemonics are invalid, because the "decrement/test
		 * CTR" option cannot be used with this instruction.
		 */
		BO = (int)inst.op_brc.BO;
		BI = (int)inst.op_brc.BI;
		TF = BO >> 1;		/* remove branch prediction bit */
		CR = BI % 4;		/* retrieve single CR field */
		CRnum = BI / 4;		/* compute CR field number */

		if (TF == PPC_BRANCH_ALWAYS)
			PrintF("b");
		else if (TF == PPC_BRANCH_TRUE || TF == PPC_BRANCH_FALSE)
			parse_CR(TF, CR);
		else {
			PrintF("Invalid bcctrx BO/BI: 0x%xy 0x%x\n", TF, BI);
			break;
		}
		PrintF("ctr");
		check_LK(inst.op_brc.LK);
		/*
		 * check branch prediction flag
		 */
		if (BO & PPC_BRANCH_YES)	/* default: branch not taken */
			PrintF("+");
		else
			PrintF("-");
		if (CRnum)		/* if CR0, don't need to specify */
			PrintF("\tcr%d\n", CRnum);
		else
			PrintF("\n");

		/* no destination, target addr is in count register... */
		break;

	default:				/* xx */
		PrintF("OPCODE 19: invalid extended opcode %d\n",
			inst.op_bool.XO);
		break;

	}
}


parse_31(register union op inst, register ushort ppc_op2)
{
	register int rc;
	register int spr;
#if defined(DEBUG)
	if (DEBUG_LEVEL & DEBUG_PARSE_31)
		PrintF("Beginning parse_31: opcode: %d  (0x%lx)\n", ppc_op2,
		    (unsigned long)inst.l);
#endif	/* defined(DEBUG) */

	switch (ppc_op2) {

	case PPC_OP31_0:			/* cmp */
		if (inst.op_cmpi.L == 1)
/* XXXPPC  is this an optional instruction? */
#if defined(MC98601)
			PrintF("64-bit compares not implemented on the %s.\n",
			    "MC98601");
#else	/* !defined(MC98601) */
						{
			PrintF("cmpd\t");
			if (inst.op_cmp.crD == 0) { /* results go to CR0 */
				PrintF("%s%d,%s%d\n", REG, (int)inst.op_cmp.rA,
				    REG, inst.op_cmp.rB);
			} else { /* explicitly specify destination if not CR0 */
				PrintF("crf%d,%s%d,%s%d\n", inst.op_cmp.crD,
				    REG, inst.op_cmp.rA, REG, inst.op_cmp.rB);
			}
		}
#endif	/* !defined(MC98601) */
		else {				/* 32-bit operands */
			PrintF("cmpw\t");
			if (inst.op_cmp.crD == 0) { /* results go to CR0 */
				PrintF("%s%d,%s%d\n", REG, (int)inst.op_cmp.rA,
				    REG, inst.op_cmp.rB);
			} else { /* explicitly specify destination if not CR0 */
				PrintF("crf%d,%s%d,%s%d\n", inst.op_cmp.crD,
				    REG, inst.op_cmp.rA, REG, inst.op_cmp.rB);
			}
		}
/*
 * original version with standard mnemonic
 *		PrintF("%s\tcrf%d,1,%s%d,%s%d\n", PPC_INST31_0,
 *		    inst.op_cmp.crD, REG, inst.op_cmp.rA, REG, inst.op_cmp.rB);
 */
		break;

	case PPC_OP31_4:			/* tw */
						/* rD=TO */
		parse_TO(inst.op_typ.rD);
		if (inst.op_typ.rD != PPC_TRAP)
			PrintF("\t%s%d,%s%d\n", REG, inst.op_typ.rA, REG,
			    inst.op_typ.rB);
		else
			PrintF("\n");
		break;

	case PPC_OP31_8:			/* subfcx */
		/*
		 * The extended mnemonic for this instruction provides a more
		 * natural operand order; i.e., the third operand is subtracted
		 * from the second. (The first operand is still the destination
		 * register for the difference.
		 */
		printi_XO3r("subc", inst.op_imath);	/* extended mnemonic */

/*		printi_XO3(PPC_INST31_8, inst.op_imath); standard mnemonic */
		break;

	case PPC_OP31_10:			/* addcx */
		printi_XO3(PPC_INST31_10, inst.op_imath);
		break;

	case PPC_OP31_11:					/* mulhwux */
		printi_X3(PPC_INST31_11, xMATH.rD, xMATH.rA, xMATH.rB, xMATH.Rc,
		    GENREG);
		break;

	case PPC_OP31_19:			/* mfcr */
		PrintF("%s\t%s%d\n", PPC_INST31_19, REG, inst.op_imath.rD);
		break;

	case PPC_OP31_20:			/* lwarx */
		printi_X3(PPC_INST31_20, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    GENREG);
		break;

	case PPC_OP31_23:			/* lwzx */
		printi_X3(PPC_INST31_23, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    GENREG);
		break;

	case PPC_OP31_24:			/* slwx */
						/* rD=rS */
		printi_X3(PPC_INST31_24, xTYP.rA, xTYP.rD, xTYP.rB, xTYP.Rc,
		    GENREG);
		break;

	case PPC_OP31_26:			/* cntlzwx */
						/* rD=rS,rB=0 */
		printi_X2(PPC_INST31_26, xTYP.rA, xTYP.rD, xTYP.Rc, GENREG);
		break;

	case PPC_OP31_28:			/* andx */
						/* rD=rS */
		/*
		 * a special case -
		 * The and instruction may be used to move the contents of one
		 * register to another.  Use the extended mnemonic in this
		 * case to clarify this usage, (i.e., data movement, not
		 * a logical operation).
		 */
		if (inst.op_typ.rD == inst.op_typ.rB)
			printi_X2("mr", xTYP.rA, xTYP.rD, xTYP.Rc, GENREG);
		else
			printi_X3(PPC_INST31_28, xTYP.rA, xTYP.rD, xTYP.rB,
			    xTYP.Rc, GENREG);
		break;

	case PPC_OP31_29:			/* maskgx */
		print_POWER();			/* POWER/601 only */
		printi_X3(PPC_INST31_29, xTYP.rA, xTYP.rD, xTYP.rB, xTYP.Rc,
		    GENREG);
		break;

	case PPC_OP31_32:			/* cmpl */
		if (inst.op_cmp.L == 1)
/* XXXPPC L bit is ignored on MC98601 - optional instruction? */
#if defined(MC98601)
			PrintF("64-bit compares not implemented on the %s.\n",
			    "MC98601");
#else	/* !defined(MC98601) */
					{
			PrintF("cmpld\t");
			if (inst.op_cmp.crD == 0) { /* results go to CR0 */
				PrintF("%s%d,%s%d\n", inst.op_cmp.rA,
				    inst.op_cmp.rB);
			} else { /* explicitly specify destination if not CR0 */
				PrintF("crf%d,%s%d,%s%d\n",
				    REG, inst.op_cmp.crD, REG, inst.op_cmp.rA,
				    inst.op_cmp.rB);
			}
		}
#endif	/* defined(MC98601) */
		else {				/* 32-bit operands */
			PrintF("cmplw\t");
			if (inst.op_cmp.crD == 0) { /* results go to CR0 */
				PrintF("%s%d,%s%d\n", REG, inst.op_cmp.rA, REG,
				    inst.op_cmp.rB);
			} else { /* explicitly specify destination if not CR0 */
				PrintF("crf%d,%s%d,%s%d\n", inst.op_cmp.crD,
				    REG, inst.op_cmp.rA, REG, inst.op_cmp.rB);
			}
		}
/*
*		PrintF("%s\tcrf%d,1,%s%d,%s%d\n", PPC_INST31_32,
*		    inst.op_cmp.crD, REG, inst.op_cmp.rA, REG, inst.op_cmp.rB);
*/
		break;

	case PPC_OP31_40:			/* subfx */

		/*
		 * The extended mnemonic for this instruction provides a more
		 * natural operand order; i.e., the third operand is subtracted
		 * from the second. (The first operand is still the destination
		 * register for the difference.
		 */
		printi_XO3r("sub", inst.op_imath); /* extended mnemonic */

/*		printi_XO3(PPC_INST31_40, inst.op_imath); standard mnemonic */
		break;

	case PPC_OP31_54:			/* dcbst */
		printi_X2(PPC_INST31_54, xTYP.rA, xTYP.rB, NO_RC, GENREG);
		break;

	case PPC_OP31_55:			/* lwzux */
		printi_X3(PPC_INST31_55, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    GENREG);
		break;

	case PPC_OP31_60:			/* andcx */
						/* rD=rS */
		printi_X3(PPC_INST31_60, xTYP.rA, xTYP.rD, xTYP.rB, xTYP.Rc,
		    GENREG);
		break;

	case PPC_OP31_75:			/* mulhwx */
		printi_X3(PPC_INST31_75, xMATH.rD, xMATH.rA, xMATH.rB, xMATH.Rc,
		    GENREG);
		break;

	case PPC_OP31_83:			/* mfmsr */
		PrintF("%s\t%s%d\n", PPC_INST31_83, REG, inst.op_typ.rD);
		break;

	case PPC_OP31_86:			/* dcbf */
		printi_X2(PPC_INST31_86, xTYP.rA, xTYP.rB, NO_RC, GENREG);
		break;

	case PPC_OP31_87:			/* lbzx */
		printi_X3(PPC_INST31_87, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    GENREG);
		break;

	case PPC_OP31_104:			/* negx */
		printi_XO2(PPC_INST31_104, inst.op_imath);
		break;

	case PPC_OP31_107:			/* mulx */
		print_POWER();			/* POWER/601 only */
		printi_XO3(PPC_INST31_107, inst.op_imath);
		break;

	case PPC_OP31_115:			/* mfpmr */
/* XXXPPC what is this instruction??? preliminary book??? */
#if defined(MC98601)
		PrintF("mfpmr not implemented on the MC98601.\n");
#else	/* !defined(MC98601) */
		PrintF("%s\t%d,r%d,%ld\n", PPC_INST31_115, inst.op_typ.rD);
#endif	/* defined(MC98601) */
		break;

	case PPC_OP31_119:			/* lbzux */
		printi_X3(PPC_INST31_119, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    GENREG);
		break;

	case PPC_OP31_124:			/* norx */
						/* rD = rS */
		/*
		 * The nor instruction may be used to complement the contents
		 * of one register and to place the result into another
		 * register.
		 * Use the extended mnemonic in this case to clarify this usage,
		 * (i.e., data movement, not a logical operation).
		 */
		if (inst.op_typ.rD == inst.op_typ.rB)
			printi_X2("not", xTYP.rA, xTYP.rD, xTYP.Rc, GENREG);
		else
			printi_X3(PPC_INST31_124, xTYP.rA, xTYP.rD, xTYP.rB,
			    xTYP.Rc, GENREG);
		break;

	case PPC_OP31_136:			/* subfex */
		printi_XO3(PPC_INST31_136, inst.op_imath);
		break;

	case PPC_OP31_138:			/* addex */
		printi_XO3(PPC_INST31_138, inst.op_imath);
		break;

	case PPC_OP31_144:			/* mtcrf */
		/*
		 * NOTE from Instruction Set Manual: "Updating a proper set of
		 * the eight fields of the Condition Register may have sub-
		 * stantially poorer performance on some implementations than
		 * updating all of the fields."   (Book I, p. 82)
		 */
		PrintF("%s\t0x%x,%s%d\n", PPC_INST31_144, inst.op_mtcrf.FXM,
		    REG, inst.op_mtcrf.rS);
		break;

	case PPC_OP31_146:			/* mtmsr */
		PrintF("%s\t%s%d\n", PPC_INST31_146, REG, inst.op_typ.rD);
		break;

	case PPC_OP31_150:			/* stwcx. */
						/* rD=rS */
		printi_X3(PPC_INST31_150, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    GENREG);
		break;

	case PPC_OP31_151:			/* stwx */
						/* rD=rS */
		printi_X3(PPC_INST31_151, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    GENREG);
		break;

	case PPC_OP31_152:			/* slqx */
		print_POWER();			/* POWER/601 only */
		printi_X3(PPC_INST31_152, xTYP.rA, xTYP.rD, xTYP.rB, xTYP.Rc,
		    GENREG);
		break;

	case PPC_OP31_153:			/* slex */
		print_POWER();			/* POWER/601 only */
		printi_X3(PPC_INST31_153, xTYP.rA, xTYP.rD, xTYP.rB, xTYP.Rc,
		    GENREG);
		break;

	case PPC_OP31_178:			/* mtpmr */
						/* rD=rS */
/* XXXPPC where did this instruction come from?  preliminary manual? */
#if defined(MC98601)
		PrintF("mtpmr not implemented on the MC98601.\n");
#else	/* !defined(MC98601) */
		PrintF("%s\t%s%d\n", PPC_INST31_178, REG, inst.op_typ.rD);
#endif	/* defined(MC98601) */
		break;

	case PPC_OP31_183:			/* stwux */
						/* rD=rS */
		printi_X3(PPC_INST31_183, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    GENREG);
		break;

	case PPC_OP31_184:			/* sliqx */
		print_POWER();			/* POWER/601 only */
		PrintF("%s\t%s%d,%s%d,", PPC_INST31_184, REG, xTYP.rA, REG,
		    xTYP.rD);
		print_UIMM((short)xTYP.rB);
		break;

	case PPC_OP31_200:			/* subfzex */
		printi_XO2(PPC_INST31_200, inst.op_imath);
		break;

	case PPC_OP31_202:			/* addzex */
		printi_XO2(PPC_INST31_202, inst.op_imath);
		break;

	case PPC_OP31_210:			/* mtsr */
						/* rD=rS */
/*
 * XXXPPC - this instruction will only work on a 32-bit implementation.
 * an attempt to use it on a 64-bit implementation will cause a program
 * exception.  Don't know if we want to create a 32-bit ifdef yet,
 * so leave the comment for now.........
 */
		PrintF("%s\t0x%x,%s%d\n", PPC_INST31_210, inst.op_msr.SR,
		    REG, inst.op_msr.rD);
		break;

	case PPC_OP31_215:			/* stbx */
						/* rD=rS */
		printi_X3(PPC_INST31_215, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    GENREG);
		break;

	case PPC_OP31_216:			/* sliqx */
		print_POWER();			/* POWER/601 only */
		printi_X3(PPC_INST31_216, xTYP.rA, xTYP.rD, xTYP.rB, xTYP.Rc,
		    GENREG);
		break;

	case PPC_OP31_217:			/* sleqx */
		print_POWER();			/* POWER/601 only */
		printi_X3(PPC_INST31_217, xTYP.rA, xTYP.rD, xTYP.rB, xTYP.Rc,
		    GENREG);
		break;

	case PPC_OP31_232:			/* subfmex */
		printi_XO2(PPC_INST31_232, inst.op_imath);
		break;

	case PPC_OP31_234:			/* addmex */
		printi_XO2(PPC_INST31_234, inst.op_imath);
		break;

	case PPC_OP31_235:			/* mullwx */
		printi_XO3(PPC_INST31_235, inst.op_imath);
		break;

	case PPC_OP31_242:			/* mtsrin */
						/* rD=rS */
		printi_X2(PPC_INST31_242, xTYP.rD, xTYP.rB, NO_RC, GENREG);
		break;

	case PPC_OP31_246:			/* dcbtst */
		printi_X2(PPC_INST31_246, xTYP.rA, xTYP.rB, NO_RC, GENREG);
		break;

	case PPC_OP31_247:			/* stbux */
						/* rD=rS */
		printi_X3(PPC_INST31_247, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    GENREG);
		break;

	case PPC_OP31_248:			/* slliqx */
		print_POWER();			/* POWER/601 only */
		PrintF("%s\t%s%d,%s%d,", PPC_INST31_248, REG, xTYP.rA, REG,
			xTYP.rD);
		print_UIMM((short)xTYP.rB);
		break;

	case PPC_OP31_264:			/* dozx */
		print_POWER();			/* POWER/601 only */
		printi_XO3(PPC_INST31_264, inst.op_imath);
		break;

	case PPC_OP31_266:			/* addx */
		printi_XO3(PPC_INST31_266, inst.op_imath);
		break;

	case PPC_OP31_277:			/* lscbxx */
		print_POWER();			/* POWER/601 only */
		printi_X3(PPC_INST31_277, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    GENREG);
		break;

	case PPC_OP31_278:			/* dcbt */
		printi_X2(PPC_INST31_278, xTYP.rA, xTYP.rB, NO_RC, GENREG);
		break;

	case PPC_OP31_279:			/* lhzx */
		printi_X3(PPC_INST31_279, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    GENREG);
		break;

	case PPC_OP31_284:			/* eqvx */
						/* rD=rS */
		printi_X3(PPC_INST31_284, xTYP.rA, xTYP.rD, xTYP.rB, NO_RC,
		    GENREG);
		break;

	case PPC_OP31_306:			/* tlbie */
		PrintF("%s\t%s%d\n", PPC_INST31_306, REG, inst.op_typ.rB);
		break;

	case PPC_OP31_310:			/* eciwx */
		/* NOTE: optional PowerPC architecture instruction */
		printi_X3(PPC_INST31_310, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    GENREG);
		break;

	case PPC_OP31_311:			/* lhzux */
		printi_X3(PPC_INST31_311, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    GENREG);
		break;

	case PPC_OP31_316:			/* xorx */
						/* rD=rS */
		printi_X3(PPC_INST31_316, xTYP.rA, xTYP.rD, xTYP.rB, xTYP.Rc,
		    GENREG);
		break;

	case PPC_OP31_331:			/* divx */
		print_POWER();			/* POWER/601 only */
		printi_XO3(PPC_INST31_331, inst.op_imath);
		break;

	case PPC_OP31_339:			/* mfspr */
						/* rS=spr */
		/*
		 * The mfspr instruction moves a value from a Special
		 * Purpose Register.  Use one of the extended mnemonics,
		 * if one is available for the given SPR.
		 */
		spr = inst.op_mspr.rS;
		if (spr == PPC_SPR_MQ || spr == PPC_SPR_RTCU ||
		    spr == PPC_SPR_RTCL || spr == PPC_SPR_DECu ||
		    spr == PPC_SPR_HID1 || spr == PPC_SPR_HID2 ||
		    spr == PPC_SPR_HID5 || spr == PPC_SPR_HID15)
			print_POWER();
		PrintF("mf");
		rc = parse_SPR(spr);
		PrintF("\t%s%d", REG, inst.op_mspr.rD);
		if (rc) {
			PrintF(", ");
			print_SIMM(unmunge_SPR(spr), YES_EOL);
		} else
			PrintF("\n");
		break;

	case PPC_OP31_343:			/* lhax */
		printi_X3(PPC_INST31_343, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    GENREG);
		break;

	case PPC_OP31_360:			/* absx */
		print_POWER();			/* POWER/601 only */
		printi_XO2(PPC_INST31_360, inst.op_imath);
		break;

	case PPC_OP31_363:			/* divsx */
		print_POWER();			/* POWER/601 only */
		printi_XO3(PPC_INST31_363, inst.op_imath);
		break;

	case PPC_OP31_371:			/* mftb */
		if (inst.op_mspr.rS == PPC_SPR_FTBU)
			PrintF("%su\t%s%d\n", PPC_INST31_371, REG, inst.op_mspr.rD);
		else
			PrintF("%s\t%s%d\n", PPC_INST31_371, REG, inst.op_mspr.rD);
		break;

	case PPC_OP31_375:			/* lhaux */
		printi_X3(PPC_INST31_375, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    GENREG);
		break;

	case PPC_OP31_407:			/* sthx */
						/* rD=rS */
		printi_X3(PPC_INST31_407, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    GENREG);
		break;

	case PPC_OP31_412:			/* orcx */
						/* rD=rS */
		printi_X3(PPC_INST31_412, xTYP.rA, xTYP.rD, xTYP.rB, xTYP.Rc,
		    GENREG);
		break;

	case PPC_OP31_434:			/* slbie */
#if defined(MC98601)
		PrintF("slbie not implemented on the MC98601.\n");
#else	/* !defined(MC98601) */
		PrintF("%s\t%s%d\n", PPC_INST31_434, REG, inst.op_typ.rB);
#endif	/* defined(MC98601) */
		break;

	case PPC_OP31_438:			/* ecowx */
						/* rD=rS */
		/* NOTE: optional PowerPC architecture instruction */
		printi_X3(PPC_INST31_438, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    GENREG);
		break;

	case PPC_OP31_439:			/* sthux */
						/* rD=rS */
		printi_X3(PPC_INST31_439, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    GENREG);
		break;

	case PPC_OP31_444:			/* orx */
						/* rD=rS */
		/*
		 * The or instruction may be used to move the contents of one
		 * register to another.  Use the extended mnemonic in this
		 * case to clarify this usage, (i.e., data movement, not
		 * a logical operation).
		 */
		if (inst.op_typ.rD == inst.op_typ.rB)
			printi_X2("mr", xTYP.rA, xTYP.rD, xTYP.Rc, GENREG);
		else 			/* typical usage */
			printi_X3(PPC_INST31_444, xTYP.rA, xTYP.rD, xTYP.rB,
			    xTYP.Rc, GENREG);
		break;

	case PPC_OP31_459:			/* divwux */
		printi_XO3(PPC_INST31_459, inst.op_imath);
		break;

	case PPC_OP31_466:			/* slbiex */
#if defined(MC98601)
		PrintF("slbiex not implemented on the MC98601.\n");
#else	/* !defined(MC98601) */
		PrintF("%s\t%s%d\n", PPC_INST31_466, REG, inst.op_typ.rB);
#endif	/* defined(MC98601) */
		break;

	case PPC_OP31_467:			/* mtspr */
						/* rS=spr */
		/*
		 * The mtspr instruction moves a value into a Special
		 * Purpose Register.  Use one of the extended mnemonics,
		 * if one is available for the given SPR.
		 */
		spr = inst.op_mspr.rS;
		if (spr == PPC_SPR_MQ || spr == PPC_SPR_RTCU ||
		    spr == PPC_SPR_RTCL || spr == PPC_SPR_DECu ||
		    spr == PPC_SPR_HID1 || spr == PPC_SPR_HID2 ||
		    spr == PPC_SPR_HID5 || spr == PPC_SPR_HID15)
			print_POWER();
		
		PrintF("mt");
		rc = parse_SPR(spr);
		PrintF("\t");
		if (rc) {
			print_SIMM(unmunge_SPR(spr), NO_EOL);
			PrintF(", ");
		}
		PrintF("%s%d\n", REG, inst.op_mspr.rD);
		break;

	case PPC_OP31_470:			/* dcbi */
		printi_X2(PPC_INST31_470, xTYP.rA, xTYP.rB, NO_RC, GENREG);
		break;

	case PPC_OP31_476:			/* nandx */
						/* rD=rS */
		printi_X3(PPC_INST31_476, xTYP.rA, xTYP.rD, xTYP.rB, xTYP.Rc,
		    GENREG);
		break;

	case PPC_OP31_488:			/* nabsx */
		print_POWER();			/* POWER/601 only */
		printi_XO2(PPC_INST31_488, inst.op_imath);
		break;

	case PPC_OP31_491:			/* divwx */
		printi_XO3(PPC_INST31_491, inst.op_imath);
		break;

	case PPC_OP31_498:			/* slbia */
#if defined(MC98601)
		PrintF("slbia not implemented on the MC98601.\n");
#else	/* !defined(MC98601) */
		PrintF("%s\n", PPC_INST31_498);
#endif	/* defined(MC98601) */
		break;

	case PPC_OP31_512:			/* mcrxr */
		PrintF("%s\tcrf%d\n", PPC_INST31_512, inst.op_cmp.crD);
		break;

	case PPC_OP31_531:			/* clcs */
		print_POWER();			/* POWER/601 only */
		printi_X2(PPC_INST31_531, xTYP.rD, xTYP.rA, xTYP.Rc, GENREG);
		break;

	case PPC_OP31_533:			/* lswx */
		printi_X3(PPC_INST31_533, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    GENREG);
		break;

	case PPC_OP31_534:			/* lwbrx */
		printi_X3(PPC_INST31_534, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    GENREG);
		break;

	case PPC_OP31_535:			/* lfsx */
						/* rD=frD */
		printi_X3(PPC_INST31_535, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    FLOTREG);
		break;

	case PPC_OP31_536:			/* srwx */
						/* rD=rS */
		printi_X3(PPC_INST31_536, xTYP.rA, xTYP.rD, xTYP.rB, xTYP.Rc,
		    GENREG);
		break;

	case PPC_OP31_537:			/* rribx */
		print_POWER();			/* POWER/601 only */
		printi_X3(PPC_INST31_537, xTYP.rA, xTYP.rD, xTYP.rB, xTYP.Rc,
		    GENREG);
		break;

	case PPC_OP31_541:			/* maskirx */
		print_POWER();			/* POWER/601 only */
		printi_X3(PPC_INST31_541, xTYP.rA, xTYP.rD, xTYP.rB, xTYP.Rc,
		    GENREG);
		break;

	case PPC_OP31_566:			/* tlbsync */
		PrintF("%s\n", PPC_INST31_566);
		break;

	case PPC_OP31_567:			/* lfsux */
		printi_X3(PPC_INST31_567, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    FLOTREG);
		break;

	case PPC_OP31_595:			/* mfsr */
/*
 * XXXPPC this instruction is only defined for 32-bit implementations...
 * if used on a 64-bit implementation, it will cause an illegal instruction
 * exception.
 */
		PrintF("%s\t%s%d,0x%x\n", PPC_INST31_595, REG,
				inst.op_msr.rD, inst.op_msr.SR);
		break;

	case PPC_OP31_597:			/* lswi */
						/* rB=NB */
		/*
		 * NOTE: n=NB if NB != 0, n=32 if NB == 0
		 * loads n consecutive bytes into sequential GPR's starting
		 * with rD
		 */
/* XXXPPC for the NB=0 case, should we show 32? */
		PrintF("%s\t%s%d,%s%d,0x%x\n", PPC_INST31_597, REG,
		    inst.op_typ.rD, REG, inst.op_typ.rA, inst.op_typ.rB);
		break;

	case PPC_OP31_598:			/* sync */
		PrintF("%s\n", PPC_INST31_598);
		break;

	case PPC_OP31_599:			/* lfdx */
						/* rD=frD */
		printi_X3(PPC_INST31_599, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    FLOTREG);
		break;

	case PPC_OP31_631:			/* lfdux */
						/* rD=frD */
		printi_X3(PPC_INST31_631, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    FLOTREG);
		break;

	case PPC_OP31_659:			/* mfsrin */
		printi_X2(PPC_INST31_659, xTYP.rD, xTYP.rB, NO_RC, GENREG);
		break;

	case PPC_OP31_661:			/* stswx */
						/* rD=rS */
		printi_X3(PPC_INST31_661, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    GENREG);
		break;

	case PPC_OP31_662:			/* stwbrx */
						/* rD=rS */
		printi_X3(PPC_INST31_662, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    GENREG);
		break;

	case PPC_OP31_663:			/* stfsx */
						/* rD=frS */
		printi_X3(PPC_INST31_663, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    FLOTREG);
		break;

	case PPC_OP31_664:			/* srqx */
		print_POWER();			/* POWER/601 only */
		printi_X3(PPC_INST31_664, xTYP.rA, xTYP.rD, xTYP.rB, xTYP.Rc,
		    GENREG);
		break;

	case PPC_OP31_665:			/* srex */
		print_POWER();			/* POWER/601 only */
		printi_X3(PPC_INST31_665, xTYP.rA, xTYP.rD, xTYP.rB, xTYP.Rc,
		    GENREG);
		break;

	case PPC_OP31_695:			/* stfsux */
						/* rD=frS */
		printi_X3(PPC_INST31_695, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    FLOTREG);
		break;

	case PPC_OP31_696:			/* sriqx */
		print_POWER();			/* POWER/601 only */
		PrintF("%s\t%s%d,%s%d,", PPC_INST31_696, REG, xTYP.rA, REG,
		    xTYP.rD);
		print_UIMM((short)xTYP.rB);
		break;

	case PPC_OP31_725:			/* stswi */
						/* rD=rS,rB=NB */
		PrintF("%s\t%s%d,%s%d,0x%x\n", PPC_INST31_725, REG,
		    inst.op_typ.rD, REG, inst.op_typ.rA, inst.op_typ.rB);
		break;

	case PPC_OP31_727:			/* stfdx */
						/* rD=frS */
		printi_X3(PPC_INST31_727, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    FLOTREG);
		break;

	case PPC_OP31_728:			/* srlqx */
		print_POWER();			/* POWER/601 only */
		printi_X3(PPC_INST31_728, xTYP.rA, xTYP.rD, xTYP.rB, xTYP.Rc,
		    GENREG);
		break;

	case PPC_OP31_729:			/* sreqx */
		print_POWER();			/* POWER/601 only */
		printi_X3(PPC_INST31_729, xTYP.rA, xTYP.rD, xTYP.rB, xTYP.Rc,
		    GENREG);
		break;

	case PPC_OP31_759:			/* stfdux */
						/* rD=frS */
		printi_X3(PPC_INST31_759, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    FLOTREG);
		break;

	case PPC_OP31_760:			/* srliqx */
		print_POWER();			/* POWER/601 only */
		PrintF("%s\t%s%d,%s%d,", PPC_INST31_760, REG, xTYP.rA, REG,
		    xTYP.rD);
		print_UIMM((short)xTYP.rB);
		break;

	case PPC_OP31_790:			/* lhbrx */
		printi_X3(PPC_INST31_790, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    GENREG);
		break;

	case PPC_OP31_792:			/* srawx */
						/* rD=rS */
		printi_X3(PPC_INST31_792, xTYP.rA, xTYP.rD, xTYP.rB, xTYP.Rc,
		    GENREG);
		break;

	case PPC_OP31_824:			/* srawix */
						/* rD=rS,rB=SH */
		PrintF("%s", PPC_INST31_824);
		check_Rc(inst.op_typ.Rc);
		PrintF("\t%s%d,%s%d,0x%x\n", REG, inst.op_typ.rA, REG,
		    inst.op_typ.rD, inst.op_typ.rB);
		break;

	case PPC_OP31_854:			/* eieio */
		PrintF("%s\n", PPC_INST31_854);
		break;

	case PPC_OP31_918:			/* sthbrx */
						/* rD=rS */
		printi_X3(PPC_INST31_918, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    GENREG);
		break;

	case PPC_OP31_920:			/* sraqx */
		print_POWER();			/* POWER/601 only */
		printi_X3(PPC_INST31_920, xTYP.rA, xTYP.rD, xTYP.rB, xTYP.Rc,
		    GENREG);
		break;

	case PPC_OP31_921:			/* sreax */
		print_POWER();			/* POWER/601 only */
		printi_X3(PPC_INST31_921, xTYP.rA, xTYP.rD, xTYP.rB, xTYP.Rc,
		    GENREG);
		break;

	case PPC_OP31_922:			/* extshx */
						/* rD=rS */
		printi_X2(PPC_INST31_922, xTYP.rA, xTYP.rD, xTYP.Rc, GENREG);
		break;

	case PPC_OP31_952:			/* sraiqx */
		print_POWER();			/* POWER/601 only */
		PrintF("%s\t%s%d,%s%d,", PPC_INST31_952, REG, xTYP.rA, REG,
		    xTYP.rD);
		print_UIMM((short)xTYP.rB);
		break;

	case PPC_OP31_954:			/* extsbx */
						/* rD=rS */
		printi_X2(PPC_INST31_954, xTYP.rA, xTYP.rD, xTYP.Rc, GENREG);
		break;

	case PPC_OP31_978:			/* tlbld */
		PrintF("%s\t%s%d\n", PPC_INST31_978, REG, inst.op_typ.rB);
		break;

	case PPC_OP31_982:			/* icbi */
		printi_X2(PPC_INST31_982, xTYP.rA, xTYP.rB, NO_RC, GENREG);
		break;

	case PPC_OP31_983:			/* stfiwx */
#if defined(MC98601)
		PrintF("stfiwx not implemented on the MC98601.\n");
#else	/* !defined(MC98601) */
		printi_X3(PPC_INST31_983, xTYP.rD, xTYP.rA, xTYP.rB, NO_RC,
		    GENREG);
#endif	/* defined(MC98601) */
		break;

	case PPC_OP31_1010:			/* tlbli */
		PrintF("%s\t%s%d\n", PPC_INST31_1010, REG, inst.op_typ.rB);
		break;

	case PPC_OP31_1014:			/* dcbz */
		printi_X2(PPC_INST31_1014, xTYP.rA, xTYP.rB, NO_RC, GENREG);
		break;

	default:				/* xx */
		/*
		 * WARNING: what are the extended opcodes for tlbia and tlbiex?
		 */
		return (-1);
		break;

	}
	return (0);
}


void
parse_59(register union op inst, register ushort ppc_op2)
{
#if defined(DEBUG)
	if (DEBUG_LEVEL & DEBUG_PARSE_59)
		PrintF("Beginning parse_59: opcode: %d  (0x%lx)\n",
		    ppc_op2, (unsigned long)inst.l);
#endif	/* defined(DEBUG) */

	switch (ppc_op2) {

	case PPC_OP59_18:			/* fdivsx */
		printi_A3(PPC_INST59_18, xFP.frD, xFP.frA, xFP.frB, xFP.Rc);
		break;

	case PPC_OP59_20:			/* fsubsx */
		printi_A3(PPC_INST59_20, xFP.frD, xFP.frA, xFP.frB, xFP.Rc);
		break;

	case PPC_OP59_21:			/* faddsx */
		printi_A3(PPC_INST59_21, xFP.frD, xFP.frA, xFP.frB, xFP.Rc);
		break;

	case PPC_OP59_22:			/* fsqrtsx */
/* check the documentation PPC - called frsqrtsx? */
#if defined(MC98601)
		PrintF("fsqrtsx not implemented on the MC98601.\n");
#else	/* !defined(MC98601) */
		PrintF("%s", PPC_INST59_22);
		check_Rc(inst.op_fmath.Rc);
		PrintF("\t%s%d,%s%d\n", FREG,
		    inst.op_fmath.frD, FREG, inst.op_fmath.frB);
#endif	/* defined(MC98601) */
		break;

	case PPC_OP59_24:			/* fresx */
#if defined(MC98601)
		PrintF("fresx not implemented on the MC98601.\n");
#else	/* !defined(MC98601) */
		PrintF("%s", PPC_INST59_24);
		check_Rc(inst.op_fmath.Rc);
		PrintF("\t%s%d,%s%d\n", FREG,
		    inst.op_fmath.frD, FREG, inst.op_fmath.frB);
#endif	/* defined(MC98601) */
		break;

	case PPC_OP59_25:			/* fmulsx */
		printi_A3(PPC_INST59_25, xFP.frD, xFP.frA, xFP.frC, xFP.Rc);
		break;

	case PPC_OP59_28:			/* fmsubsx */
		printi_A4(PPC_INST59_28, xFP.frD, xFP.frA, xFP.frC, xFP.frB,
		    xFP.Rc);
		break;

	case PPC_OP59_29:			/* fmaddsx */
		printi_A4(PPC_INST59_29, xFP.frD, xFP.frA, xFP.frC, xFP.frB,
		    xFP.Rc);
		break;

	case PPC_OP59_30:			/* fnmsubsx */
		printi_A4(PPC_INST59_30, xFP.frD, xFP.frA, xFP.frC, xFP.frB,
		    xFP.Rc);
		break;

	case PPC_OP59_31:			/* fnmaddsx */
		printi_A4(PPC_INST59_31, xFP.frD, xFP.frA, xFP.frC, xFP.frB,
		    xFP.Rc);
		break;

	default:				/* xx */
		PrintF("OPCODE 59: invalid extended opcode %d\n", ppc_op2);
		break;

	}
}


parse_63(register union op inst, register ushort ppc_op2)
{
#if defined(DEBUG)
	if (DEBUG_LEVEL & DEBUG_PARSE_63)
		PrintF("Beginning parse_63: opcode: %d  (0x%lx)\n",
		    ppc_op2, (unsigned long)inst.l);
#endif	/* defined(DEBUG) */

	switch (ppc_op2) {

	case PPC_OP63_0:			/* fcmpu */
		PrintF("%s\tcr%d,%s%d,%s%d\n", PPC_INST63_0, inst.op_cmp.crD,
		    FREG, inst.op_cmp.rA, FREG, inst.op_cmp.rB);
		break;

	case PPC_OP63_12:			/* frspx */
		printi_X2(PPC_INST63_12, xTYP.rD, xTYP.rB, xTYP.Rc, FLOTREG);
		break;

	case PPC_OP63_14:			/* fctiwx */
		printi_X2(PPC_INST63_14, xFCNV.frT, xFCNV.frB, xFCNV.Rc,
		    FLOTREG);
		break;

	case PPC_OP63_15:			/* fctiwzx */
		printi_X2(PPC_INST63_15, xFCNV.frT, xFCNV.frB, xFCNV.Rc,
		    FLOTREG);
		break;

	case PPC_OP63_18:			/* fdivx */
		printi_A3(PPC_INST63_18, xFP.frD, xFP.frA, xFP.frB, xFP.Rc);
		break;

	case PPC_OP63_20:			/* fsubx */
		printi_A3(PPC_INST63_20, xFP.frD, xFP.frA, xFP.frB, xFP.Rc);
		break;

	case PPC_OP63_21:			/* faddx */
		printi_A3(PPC_INST63_21, xFP.frD, xFP.frA, xFP.frB, xFP.Rc);
		break;

	case PPC_OP63_22:			/* fsqrtx */
/* check the documentation PPC - called frsqrtx? */
#if defined(MC98601)
		PrintF("fsqrtx not implemented on the MC98601.\n");
#else	/* !defined(MC98601) */
		printi_X2(PPC_INST63_22, xTYP.rD, xTYP.rB, xTYP.Rc, FLOTREG);
#endif	/* defined(MC98601) */
		break;

	case PPC_OP63_23:			/* fselx */
#if defined(MC98601)
		PrintF("fselx not implemented on the MC98601.\n");
#else	/* !defined(MC98601) */
		printi_A4(PPC_INST63_23, xFP.frD, xFP.frA, xFP.frC, xFP.frB,
		    xFP.Rc);
#endif	/* defined(MC98601) */
		break;

	case PPC_OP63_25:			/* fmulx */
		printi_A3(PPC_INST63_25, xFP.frD, xFP.frA, xFP.frC, xFP.Rc);
		break;

	case PPC_OP63_26:			/* frsqrtex */
#if defined(MC98601)
		PrintF("frsqrtex not implemented on the MC98601.\n");
#else	/* !defined(MC98601) */
		printi_X2(PPC_INST63_26, xTYP.rD, xTYP.rB, xTYP.Rc, FLOTREG);
#endif	/* defined(MC98601) */
		break;

	case PPC_OP63_28:			/* fmsubx */
		printi_A4(PPC_INST63_28, xFP.frD, xFP.frA, xFP.frC, xFP.frB,
		    xFP.Rc);
		break;

	case PPC_OP63_29:			/* fmaddx */
		printi_A4(PPC_INST63_29, xFP.frD, xFP.frA, xFP.frC, xFP.frB,
		    xFP.Rc);
		break;

	case PPC_OP63_30:			/* fnmsubx */
		printi_A4(PPC_INST63_30, xFP.frD, xFP.frA, xFP.frC, xFP.frB,
		    xFP.Rc);
		break;

	case PPC_OP63_31:			/* fnmaddx */
		printi_A4(PPC_INST63_31, xFP.frD, xFP.frA, xFP.frC, xFP.frB,
		    xFP.Rc);
		break;

	case PPC_OP63_32:			/* fcmpo */
		PrintF("%s\tcr%d,%s%d,%s%d\n", PPC_INST63_32,
		    inst.op_cmp.crD, FREG, inst.op_cmp.rA, FREG,
		    inst.op_cmp.rB);
		break;

	case PPC_OP63_38:			/* mtfsb1x */
		PrintF("%s", PPC_INST63_38);
		check_Rc(inst.op_bool.Rc);
		PrintF("\t");
		print_crb(inst.op_bool.crbD, CRB_EOL);
		break;

	case PPC_OP63_40:			/* fnegx */
						/* rD=frD,rB=frB */
		printi_X2(PPC_INST63_40, xTYP.rD, xTYP.rB, xTYP.Rc, FLOTREG);
		break;

	case PPC_OP63_64:			/* mcrfs */
		PrintF("%s\tcrf%d,crf%d", PPC_INST63_64,
		    inst.op_mcrf.BF, inst.op_mcrf.BFA);
#if (VERIFY)
		if (inst.op_mcrf.Rc || inst.op_mcrf.ZR1 ||
		    inst.op_mcrf.ZR2 || inst.op_mcrf.ZR3)
			PrintF("\t\tinvalid instruction form");
#endif
		PrintF("\n");
		break;

	case PPC_OP63_70:			/* mtfsb0x */
		PrintF("%s", PPC_INST63_70);
		check_Rc(inst.op_bool.Rc);
		PrintF("\t");
		print_crb(inst.op_bool.crbD, CRB_EOL);
		break;

	case PPC_OP63_72:			/* fmrx */
						/* rB=frB,rD=frD */
		printi_X2(PPC_INST63_72, xTYP.rD, xTYP.rB, xTYP.Rc, FLOTREG);
		break;

	case PPC_OP63_134:			/* mtfsfix */
		PrintF("%s", PPC_INST63_134);
		check_Rc(inst.op_mfpscri.Rc);
		PrintF("\tcrf%d,0x%x\n", inst.op_mfpscri.crfD,
		    inst.op_mfpscri.IMM);
		break;

	case PPC_OP63_136:			/* fnabsx */
						/* rB=frB,rD=frD */
		printi_X2(PPC_INST63_136, xTYP.rD, xTYP.rB, xTYP.Rc, FLOTREG);
		break;

	case PPC_OP63_264:			/* fabsx */
						/* rB=frB,rD=frD */
		printi_X2(PPC_INST63_264, xTYP.rD, xTYP.rB, xTYP.Rc, FLOTREG);
		break;

	case PPC_OP63_583:			/* mffsx */
						/* rD=frD */
		/*
		 * The PowerPC architecture defines frD bits 0-31 as undefined
		 * In the MPC601, the value is always 0xFFF80000.
		 */
		PrintF("%s", PPC_INST63_583);
		check_Rc(inst.op_typ.Rc);
		PrintF("\t%s%d\n", FREG, inst.op_typ.rD);
		break;

	case PPC_OP63_711:			/* mtfsfx */
		PrintF("%s", PPC_INST63_711);
		check_Rc(inst.op_mfpscr.Rc);
		PrintF("\t0x%x,%s%d\n", inst.op_mfpscr.FM, FREG,
		    inst.op_mfpscr.frB);
		break;

	default:				/* xx */
		return (-1);
		break;

	}
	return (0);
}
