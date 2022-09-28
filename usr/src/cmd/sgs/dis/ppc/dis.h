/*
 * Copyright (c) 1990-1994, by Sun Microsystems, Inc.
 */

#if !defined(PPC_DIS_H)
#define	PPC_DIS_H

#pragma ident	"@(#)dis.h	1.8	94/05/04 SMI"

/*
 *	This is the header file for the m32A disassembler.
 *	The information contained in the first part of the file
 *	is common to each version, while the last part is dependent
 *	on the particular machine architecture being used.
 */

#define	APNO	10
#define	FPNO	9
#define	PCNO	15
#define	NCPS	10	/* number of chars per symbol		*/
#define	NHEX	80	/* max # chars in object per line	*/
#define	NLINE	256	/* max # chars in mnemonic per line	*/
#define	FAIL	0
#define	TRUE	1
#define	FALSE	0
#define	LEAD	1
#define	NOLEAD	0
#define	TERM	0	/* used in tables.c to indicate	that the	*/
			/* 'indirect' field of the 'instable'		*/
			/* terminates - no pointer.  Is also checked	*/
			/* in 'dis_text()' in bits.c			*/

#if defined(AR32W)
#define	LNNOBLKSZ	1024	/* size of blocks of line numbers	*/
#define	SYMBLKSZ	1024	/* size if blocks of symbol table entries */
#else	/* !defined(AR32W) */
#define	LNNOBLKSZ	512	/* size of blocks of line numbers	*/
#define	SYMBLKSZ	512	/* size of blocks of symbol table entries */
#endif	/* defined(AR32W) */
#define	STRNGEQ 0	/* used in string compare operation	*/
			/* in 'disassembly' routine		*/

/*
 *	The following are constants that are used in the disassembly
 *	of floating point immediate operands.
 */
#define	NOTFLOAT	0
#define	FPSINGLE	1
#define	FPDOUBLE	2
#define	TWO_23	8388608		/* 2 ** 23 used in conversion of floating */
				/* point object to a decimal number in utls.c */
#define	TWO_32	4294967296.	/* 2 ** 32 also used in floating-point number */
				/* conversion in utls.c */
#define	TWO_52	4503599627370496.	/* 2 ** 52 also used in floating */
					/* point conversion routines. */
#define	BIAS	127	/* bias on 8 bit exponent of floating-point */
			/* number in utls.c */
#define	DBIAS	1023	/* bias on 11 bit exponent of double precision */
			/* floating-point number in utls.c */

/*
 * The following are the 7 possible types of floating point immediate
 * operands. These are the possible values of [s|d]fpconv() which
 * are in utls.c.
 */

#define		NOTANUM		0
#define		NEGINF		1
#define		INFINITY	2
#define		ZERO		3
#define		NEGZERO		4
#define		FPNUM		5
#define		FPBIGNUM	6

/*
 *	This is a symbolic representation of all support processor
 *	identifiers.
 */

#define		MAU_ID		0

/*
 *	This is the structure that will be used for storing all the
 *	op code information.  The structure values themselves are
 *	in 'tables.c'.
 */

struct	instable {
	char		name[NCPS];
	unsigned	class;
};
/*
 *	This is the structure that will be used for storing all the
 *	address modification information.  The structure values
 *	themselves are in 'tables.c'.
 */
struct	formtable {
	char		name[NCPS];
	unsigned	typ;
};
#endif (PPC_DIS_H)
