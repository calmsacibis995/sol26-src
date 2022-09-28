/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ppcdis_utl.c	1.14	95/10/23 SMI"

#include "ppcdis_extern.h"
#include "ppcdis_trap.h"
#include "ppcdis_branch.h"
#include "ppcdis_regs.h"
#if !defined(DIS)
#include "adb.h"
#include "symtab.h"
#endif	/* !DIS */
#if defined(DEBUG)
#include "ppcdis_debug.h"			/* various debug flags */
#endif	/* DEBUG */

/*
 * PowerPC Disassembler - Miscellaneous Utility Functions
 *
 * routines to: decipher the TO and BO operands, and
 * evaluate the Record and Overflow Enable flags.
 */

/*
 * crack the TO (trap operand)
 */
void
parse_TO(register int TO)
{
#if defined(DEBUG)
	if (DEBUG_LEVEL & DEBUG_PARSE_TO)
		PrintF("parse_TO: TO operand value: %d\n", TO);
#endif	/* DEBUG */
	switch (TO) {
	case PPC_TRAP_LT:	PrintF("twlt");
				break;

	case PPC_TRAP_LE:	PrintF("twle");
				break;

	case PPC_TRAP_EQ:	PrintF("tweq");
				break;

	case PPC_TRAP_GE:	PrintF("twge");
				break;

	case PPC_TRAP_GT:	PrintF("twgt");
				break;

	case PPC_TRAP_NE:	PrintF("twne");
				break;

	case PPC_TRAP_LLT:	PrintF("twllt");
				break;

	case PPC_TRAP_LLE:	PrintF("twlle");
				break;

	case PPC_TRAP_LGE:	PrintF("twlge");
				break;

	case PPC_TRAP_LGT:	PrintF("twlgt");
				break;

	case PPC_TRAP:		PrintF("trap");
				break;

	default:		PrintF("Invalid TO operand (0x%x)\n", TO);
				break;
	}
}


/*
 * crack the BO (branch operand)
 */
void
parse_BO(register int BO, register int CR)
{
#if defined(DEBUG)
	if (DEBUG_LEVEL & DEBUG_PARSE_BO)
		PrintF("parse_BO: BO operand value: 0x%x\n", BO);
#endif	/* DEBUG */

	switch (BO >> 1) {
	case PPC_BRANCH_DEC_NZ_FALSE:	PrintF("bdnzf");
					break;

	case PPC_BRANCH_DEC_Z_FALSE:	PrintF("bdzf");
					break;

	case PPC_BRANCH_FALSE:		parse_CR(PPC_BRANCH_FALSE, CR);
					break;

	case PPC_BRANCH_DEC_NZ_TRUE:	PrintF("bdnzt");
					break;

	case PPC_BRANCH_DEC_Z_TRUE:	PrintF("bdzt");
					break;

	case PPC_BRANCH_TRUE:		parse_CR(PPC_BRANCH_TRUE, CR);
					break;

	case PPC_BRANCH_DEC_NZ:		PrintF("bdnz");
					break;

	case PPC_BRANCH_DEC_Z:		PrintF("bdz");
					break;

	case PPC_BRANCH_ALWAYS:		PrintF("b");
					break;

	default:			PrintF("Invalid BO operand (0x%x)\n",
					    BO);
					break;
	}
}


/*
 * crack the conditional BO (branch operand)
 * parse for extended branch mnemonics incorporating conditions
 *
 * NOTE: by using extended conditional branch mnemonics, we completely
 * eliminate the use of the following simple branch mnemonics:
 *	 bt/bta/btlr/btctr/btl/btla/btlrl/btctrl
 *	 bf/bfa/bflr/bfctr/bfl/bfla/bflrl/bfctrl
 */
void
parse_CR(register int TF, register int CR)
{
#if defined(DEBUG)
	if (DEBUG_LEVEL & DEBUG_PARSE_BO)
		PrintF("parse_CR: CR operand value: 0x%x\n", CR);
#endif	/* DEBUG */

	switch (TF) {
	case (PPC_BRANCH_TRUE): {		/* condition is TRUE */

		switch (CR) {
		case PPC_CR_LT:	PrintF("blt");
				break;

		case PPC_CR_EQ:	PrintF("beq");
				break;

		case PPC_CR_GT:	PrintF("bgt");
				break;

/* XXXPPC need a way to distinguish between these two conditions */

/*		case PPC_CR_SO: */
		case PPC_CR_UN:	PrintF("bso");	/* "bns"? */
				break;

		default:	PrintF("Invalid CR 0x%x\n", CR);
				break;
		}
		break;
	}

	case (PPC_BRANCH_FALSE): {	/* condition is FALSE */

		switch (CR) {
		case PPC_CR_LT:	PrintF("bnl");
				break;

		case PPC_CR_GT:	PrintF("bng");
				break;

		case PPC_CR_EQ:	PrintF("bne");
				break;

/* XXXPPC need a way to distinguish between these two conditions */

/*		case PPC_CR_SO: */
		case PPC_CR_UN:	PrintF("bns");	/* "bnu"? */
				break;

		default:	PrintF("Invalid CR 0x%x\n", CR);
				break;
		}
		break;
	}

	default:		PrintF("Invalid TF (BO): 0x%x\n", TF);
				break;
	}
}


/*
 * Translate SPR encodings to register mnemonics for mtspr/mfspr commands.
 */
int
parse_SPR(register int SPR)
{
	switch (SPR) {
#if defined(MC98601)
	case PPC_SPR_MQ:	PrintF("mq");
				break;
#endif	/* MC98601 */
	case PPC_SPR_XER:	PrintF("xer");
				break;
#if defined(MC98601)
	case PPC_SPR_RTCU:	PrintF("rtcu");
				break;

	case PPC_SPR_RTCL:	PrintF("rtcl");
				break;

	case PPC_SPR_DECu:	PrintF("dec");
				break;
#endif	/* MC98601 */
	case PPC_SPR_LR:	PrintF("lr");
				break;

	case PPC_SPR_CTR:	PrintF("ctr");
				break;

	case PPC_SPR_DSISR:	PrintF("dsisr");
				break;

	case PPC_SPR_DAR:	PrintF("dar");
				break;

	case PPC_SPR_DEC:	PrintF("dec");
				break;

	case PPC_SPR_SDR1:	PrintF("sdr1");
				break;

	case PPC_SPR_SRR0:	PrintF("srr0");
				break;

	case PPC_SPR_SRR1:	PrintF("srr1");
				break;

	case PPC_SPR_G0:	PrintF("sprg0");
				break;

	case PPC_SPR_G1:	PrintF("sprg1");
				break;

	case PPC_SPR_G2:	PrintF("sprg2");
				break;

	case PPC_SPR_G3:	PrintF("sprg3");
				break;

	case PPC_SPR_ASR:	PrintF("asr");
				break;

	case PPC_SPR_EAR:	PrintF("ear");
				break;

#if defined(MC98601)

	case PPC_SPR_PVR:	PrintF("pvr");
				break;
#endif	/* MC98601 */
	case PPC_SPR_BAT0U:	PrintF("ibat0u");
				break;

	case PPC_SPR_BAT0L:	PrintF("ibat0l");
				break;

	case PPC_SPR_BAT1U:	PrintF("ibat1u");
				break;

	case PPC_SPR_BAT1L:	PrintF("ibat1l");
				break;

	case PPC_SPR_BAT2U:	PrintF("ibat2u");
				break;

	case PPC_SPR_BAT2L:	PrintF("ibat2l");
				break;

	case PPC_SPR_BAT3U:	PrintF("ibat3u");
				break;

	case PPC_SPR_BAT3L:	PrintF("ibat3l");
				break;

	case PPC_SPR_DBAT0U:	PrintF("dbat0u");
				break;

	case PPC_SPR_DBAT0L:	PrintF("dbat0l");
				break;

	case PPC_SPR_DBAT1U:	PrintF("dbat1u");
				break;

	case PPC_SPR_DBAT1L:	PrintF("dbat1l");
				break;

	case PPC_SPR_DBAT2U:	PrintF("dbat2u");
				break;

	case PPC_SPR_DBAT2L:	PrintF("dbat2l");
				break;

	case PPC_SPR_DBAT3U:	PrintF("dbat3u");
				break;

	case PPC_SPR_DBAT3L:	PrintF("dbat3l");
				break;

#if defined(MC98601) || defined(MC98603)
	case PPC_SPR_HID0:	PrintF("hid0");
				break;

#ifdef notdef
	case PPC_SPR_IABR:	PrintF("iabr");
				break;
#endif
#endif	/* defined(MC98601) || defined(MC98603) */

#if defined(MC98601)
	case PPC_SPR_HID1:	PrintF("hid1");
				break;

	case PPC_SPR_HID2:	PrintF("hid2");
				break;

	case PPC_SPR_HID5:	PrintF("hid5");
				break;

	case PPC_SPR_HID15:	PrintF("hid15");
				break;
#endif	/* MC98601 */

	case PPC_SPR_DMISS:	PrintF("dmiss");
				break;

	case PPC_SPR_DCMP:	PrintF("dcmp");
				break;

	case PPC_SPR_HASH1:	PrintF("hash1");
				break;

	case PPC_SPR_HASH2:	PrintF("hash2");
				break;

	case PPC_SPR_IMISS:	PrintF("imiss");
				break;

	case PPC_SPR_ICMP:	PrintF("icmp");
				break;

	case PPC_SPR_RPA:	PrintF("rpa");
				break;
	default:		PrintF("spr");
				return (-1);
	}
	return (0);
}

/*
 * The SPR number is composed of two five-bit quantities that are swapped
 * in the instruction.  We need to reconstruct this number for printing.
 */
unsigned int
unmunge_SPR(unsigned int SPR)
{
	struct swap_spr {
		unsigned field1 : 5;
		unsigned field2 : 5;
		unsigned unused : 22;
	};

	union op {
		struct swap_spr		munge;
		unsigned int		result;
	};

	union op input;	
	union op output;

	input.result = SPR;
	output.result = 0;
	output.munge.field2 = input.munge.field1;
	output.munge.field1 = input.munge.field2;
	return (output.result);
}

/*
 * check the Rc (Record Bit); modify displayed mnemonic, if necessary.
 */
void
check_Rc(register unsigned record_bit)
{
	if (record_bit)
		PrintF(".");
}


/*
 * check the OE (Overflow Enable flag); modify displayed mnemonic, if necessary
 */
void
check_OE(register unsigned overflow_enable)
{
	if (overflow_enable)
		PrintF("o");
}


/*
 * check the AA (Absolute Address flag); modify displayed mnemonic, if necessary
 *
 * If this bit is set, the EA (branch target) is the 64-bit, sign-extended
 * immediate operand.  Otherwise, EA is computed from the sum of the operand
 * and the address of the branch instruction.
 */
void
check_AA(register unsigned absolute_address)
{
	if (absolute_address)
		PrintF("a");
}


/*
 * check the LK (Link Bit); modify displayed mnemonic, if necessary
 *
 * used by branch instructions; if this flag is set, the address of the
 * instruction following the branch instruction is placed in the link register.
 */
void
check_LK(register unsigned link_bit)
{
	if (link_bit)
		PrintF("l");
}


/*
 * print a signed immediate integer value.
 * Output format depends on command-line switches; signed decimal and octal
 * are available if specified; otherwise, display in hex.
 */
void
print_SIMM(register short SIMM, register int newline)
{
	if (mflag)
		PrintF("%d", SIMM);
	else if (oflag)
#if defined(DIS)		/* standard libc PrintF */
		PrintF("%#08ho", SIMM);
#else					/* debuggers' PrintF doesn't like #'s */
		PrintF("0%o", SIMM);
#endif					/* DIS */
	else
#if defined(DIS)		/* standard libc PrintF */
		PrintF("%#06hx", SIMM);
#else					/* !DIS (adb/kadb) */
		PrintF("0x%x", SIMM);
#endif					/* DIS */

	if (newline)
		PrintF("\n");
}


/*
 * print an unsigned immediate integer value.
 * Output format depends on command-line switches; decimal and octal
 * are available if specified; otherwise, display in hex.
 * NOTE: The debuggers provide their own version of "PrintF", which
 * does not recognize many of the formats supported by the library
 * version.
 */
void
print_UIMM(register unsigned short UIMM)
{
	if (mflag)
		PrintF("%d\n", UIMM);
	else if (oflag)
#if defined(DIS)
		PrintF("%#08ho\n", UIMM);
#else					/* !DIS (adb/kadb) */
		PrintF("0%o\n", UIMM);
#endif					/* DIS */
	else
#if defined(DIS)
		PrintF("%#06hx\n", UIMM);
#else					/* !DIS (adb/kadb) */
		PrintF("0x%x\n", UIMM);
#endif					/* DIS */
}


/*
 * print the branch destination address (with symbol if possible).
 * Switches between the debuggers' method of finding symbols, and
 * the internal disassembler's M.O.
 */
void
print_dest(register int BD, register int pc)
{
	char *symbuf;			/* local pointer to symbol buffer */

	symbuf = target;

	if (BD != pc) { 		/* evaluate relocation symbol */
#if defined(DIS)
		extsympr(BD, &symbuf);
		PrintF("0x%08x\t[%s]\n", BD, target);
#else					/* !DIS (adb/kadb */
		int off;
		off = findsym(BD, ISP);
		if (cursym != NULL) {
			PrintF("%s", cursym->s_name);
			if (off)
				PrintF("+0x%x", off);
			PrintF("\n");
		} else {
			PrintF("0x%x\n", BD);
		}
#endif					/* DIS */
	} else
#if defined(DIS)
		PrintF("0x%08x\n", BD);
#else					/* !DIS (adb/kadb */
		PrintF("0x%x\n", BD);
#endif					/* DIS */
}


char *crbit[] = { "lt", "gt", "eq", "so" };	/* symbolic encoding for cr */

/*
 * routine to print combined mnemonic for condition register field/bit
 *
 * EXAMPLE:		 2	->	eq
 * 				15	->	cr3[so]
 */
void
print_crb(register int crb, register int delimit)
{
	register int crf;   /* field within condition register */
	register int crpos; /* bit position with cr field */

	crf = crb >> 2;
	crpos = crb & CRMASK;

	if (crf)
		PrintF("cr%d[%s]", crf, crbit[crpos]);
	else
		PrintF("%s", crbit[crpos]);

	switch (delimit) {		/* append delimiter to print line */

	case CRB_EOL:		PrintF("\n");
				break;

	case CRB_COMMA:		PrintF(",");
				break;

	case CRB_NO_COMMA:
	default:		break;
	}
}

/*
 * print warning for instructions that are:
 *		POWER instructions
 *		not part of the PowerPC instruction set
 *		implemented on the MC98601
 */
void
print_POWER(void)
{
	PrintF("(POWER/601)\t");
}
