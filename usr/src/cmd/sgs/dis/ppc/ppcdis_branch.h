/*
 * Copyright (c) 1993-1994 Sun Microsystems, Inc.  All Rights Reserved.
 */

#if !defined(PPCDIS_BRANCH_H)
#define	PPCDIS_BRANCH_H

#pragma ident	"@(#)ppcdis_branch.h	1.2	94/03/14 SMI"
#pragma ident "@(#)ppcdis_branch.h 1.3	93/10/19 vla"

/*
 * This file contains the encoding for the Power PC Branch Operand (BO)
 */

/*
 * BO encoding	(5 bit operand)
 *
 *		 -------------------
 *		 |         |branch |
 *		 |         |predict|
 *		 | 0 0 0 0 |flag   |
 *		 |         |       |
 *		 -------------------
 * extended set of codes used for common combinations of branch conditions
 * allows use of specific branch mnemonics, rather than the standard mnemonic
 * plus numeric operand.
 */

#define	PPC_BRANCH_DEC_NZ_FALSE		0		/* 0000y */
#define	PPC_BRANCH_DEC_Z_FALSE		1		/* 0001y */
#define	PPC_BRANCH_FALSE		2		/* 001zy */
#define	PPC_BRANCH_DEC_NZ_TRUE		4		/* 0100y */
#define	PPC_BRANCH_DEC_Z_TRUE		5		/* 0101y */
#define	PPC_BRANCH_TRUE			6		/* 011zy */
#define	PPC_BRANCH_DEC_NZ		8		/* 1z00y */
#define	PPC_BRANCH_DEC_Z		9		/* 1z01y */
#define	PPC_BRANCH_ALWAYS		10		/* 1z1zz */

/*
 * NOTE: Any other BO combinations are invalid!
 * z indicates a bit that must be zero.
 * y indicates the branch prediction flag
 */

/*
 * Bitmask for BO operand branch prediction flag
 */
#define	PPC_BRANCH_YES				0x00001

/*
 * Common Branch Conditions
 *
 */
#define	PPC_BRANCH_LT	0x00008		/* NL == GE */
#define	PPC_BRANCH_EQ	0x00002		/* NE */
#define	PPC_BRANCH_GT	0x00004		/* NG == LE */
#define	PPC_BRANCH_SO	0x00001		/* NS */
#define	PPC_BRANCH_UN	0x00001		/* NU */

/*
 * Condition Register Bit Assignments
 *
 * For most fixed-point instructions, when the Record bit is set, the
 * first three bits of CR field 0 (CR bits 0:2) are set depending on an
 * algebraic comparison of the result to zero.
 *
 * The fourth bit is copied from the SO field of the XER.
 */
#define	PPC_CR_LT	0	/* result is negative */
#define	PPC_CR_GT	1	/* result is positive */
#define	PPC_CR_EQ	2	/* result is zero */
#define	PPC_CR_SO	3	/* final state of SO bit of XER at */
				/* completion of the instruction. */
#define	PPC_CR_UN	3	/* unordered after floating-point compare */

/*
 * CRMASK - given an individual bit number (0-31), applying this mask
 * will result in a field-independent bit position.
 * EXAMPLE: 2 & CRMASK = 2; 15 & CRMASK = 3
 */
#define	CRMASK		0x00000003
#define	CRB_EOL			2	/* append newline  */
#define	CRB_COMMA		1	/* append comma	   */
#define	CRB_NO_COMMA		0	/* no-append print */

/*
 * Condition Register CR Field Identification Symbols
 *
 * The 32-bit Condition Register is subdivided into eight four-bit fields.
 * These are used to store the results of certain operations, and provide
 * a mechanism for testing and branching.
 *
 * EXAMPLE: expressions may be combined, e.g.,
 *	    CR bit 22 == PPC_CR5 + PPC_CR_EQ
 */
#define	PPC_CR0		 0
#define	PPC_CR1		 4
#define	PPC_CR2		 8
#define	PPC_CR3		12
#define	PPC_CR4		16
#define	PPC_CR5		20
#define	PPC_CR6		24
#define	PPC_CR7		28

#endif			/* PPCDIS_BRANCH_H */
