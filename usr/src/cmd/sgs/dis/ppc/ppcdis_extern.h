/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#if !defined(PPCDIS_EXTERN_H)
#define	PPCDIS_EXTERN_H

#pragma ident	"@(#)ppcdis_extern.h	1.6	95/10/23 SMI"

/*
 * PowerPC Disassembler - Component Function Prototypes
 *
 * This file contains external function declarations used by the
 * PowerPC disassembler.
 */

#include "ppcdis_inst_fmt.h"

extern void complain();
extern void parse_instruction(register union op, register ulong);
extern void parse_19(register union op, register ushort);
extern int parse_31(register union op, register ushort);
extern void parse_59(register union op, register ushort);
extern int parse_63(register union op, register ushort);

/*
 * operand crackers
 */
extern void parse_BO(register int, register int); /* branch operand */
extern void parse_CR(register int, register int); /* branch condition */
extern void parse_TO(register int);		/* trap operand	 */
extern int  parse_SPR(register int);		/* SPR encodings */
extern unsigned int unmunge_SPR(unsigned int); /* reconstruct SPR# */
extern void check_AA(register unsigned);	/* absolute address flag */
extern void check_LK(register unsigned);	/* link bit */
extern void check_OE(register unsigned);	/* overflow enable */
extern void check_Rc(register unsigned);	/* record bit */
extern void print_SIMM(register short, register int); /* display signed args */
extern void print_UIMM(register unsigned short); /* display unsigned args */
extern void print_dest(register int, register int); /* display branch dest */
extern void print_crb(register int, register int); /* display CR field[bit] */
extern void print_POWER(void);	/* special denotation for POWER instructions */

/*
 * output functions for various instruction types
 */
extern void printi_D(register char *, register int, register int,
		     register short, register int);
extern void printi_D_EA(register char *, register int, register int,
			register short, register int);
extern void printi_X2(register char *, register int, register int,
		      register int, register int);
extern void printi_X3(register char *, register int, register int,
		      register int, register int, register int);
extern void printi_XO2(register char *, register struct op_imath);
extern void printi_XO3(register char *, register struct op_imath);
extern void printi_A3(register char *, register int, register int,
		      register int, register int);
extern void printi_A4(register char *, register int, register int,
		      register int, register int, register int);
extern void printi_XL3(register char *, register struct op_bool);

extern void PrintF(const char *fmt, ...);

extern unsigned long vdot;		/* current disassembly address */
extern int DEBUG_LEVEL;
extern int mflag, oflag;
extern char target[];			/* symbol output buffer */

#define	YES_EOL	1	/* used by print_SIMM to include/exclude newline */
#define	NO_EOL	0

#define	OP_SIMM 1	/* used by printi_D to determine signed/unsigned args */
#define	OP_UIMM 0

#define	GENREG	0	/* used by printi_D_EA, printi_X2, printi_X3 */
#define	FLOTREG	1	/* to determine register type */

#define	NO_RC	-1	/* used by printi_X2, printi_x3 to check Rc */

#define	REG	"%r"	/* registers can be shown as "rN" or "%rN"; both */
#define	FREG	"%f"	/* methods are ABI-compliant. */

#endif			/* PPC_EXTERN_H */
