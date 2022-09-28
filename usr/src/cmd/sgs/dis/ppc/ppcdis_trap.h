/*
 * Copyright (c) 1993-1994 Sun Microsystems, Inc.  All Rights Reserved.
 */

#if !defined(PPCDIS_TRAP_H)
#define	PPCDIS_TRAP_H

#ident	"@(#)ppcdis_trap.h	1.2	94/03/14 SMI"
#pragma ident "@(#)ppcdis_trap.h 1.2	93/10/04 vla"

/*
 * This file contains the encoding for the Power PC Trap Operand (TO)
 */

/*
 * TO encoding
 *
 * standard set of codes used for common combinations of trap conditions
 * allows use of specific trap mnemonics, rather than the generic mnemonic
 * plus numeric operand.
 */
#define	PPC_TRAP_LT		16
#define	PPC_TRAP_LE		20
#define	PPC_TRAP_EQ		4
#define	PPC_TRAP_GE		12
#define	PPC_TRAP_GT		8
#define	PPC_TRAP_NE		24
#define	PPC_TRAP_LLT		2
#define	PPC_TRAP_LLE		6
#define	PPC_TRAP_LGE		5
#define	PPC_TRAP_LGT		1
#define	PPC_TRAP		31

#endif			/* PPCDIS_TRAP_H */
