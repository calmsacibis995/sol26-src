/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)__var_arg.c	1.4	96/01/29 SMI"


/* va_list: structure describing var arg passing locations */

/*
 * NOTES:
 *	input_arg_area is initially the address at which the first
 *	var arg passed in memory, if any, was passed
 *
 *	reg_save_area is the start of where r3:r10 were stored
 *	The value of reg_save_area must be a multiple of 8, not just 4.
 *
 *	If f1:f8 have been stored (because CR bit 6 was 1),
 *	reg_save_area+4*8 must be the start of where f1:f8 were stored
 */

#include <sys/va_list.h>

/* _va_arg_type: types of arguments in a variable argument (var arg) list */

typedef enum {
	arg_ARGPOINTER,	/* in r3:r10 or in aligned word, but treated as	*/
			/* pointer to actual var arg.  E.g., structs, unions, */
			/* and long doubles passed by value use		*/
			/* arg_ARGPOINTER.				*/
	arg_WORD, 	/* in r3:r10 or in aligned word			*/
	arg_DOUBLEWORD, /* in odd/even pair in r3:r10 or in		*/
			/* aligned doubleword				*/
	arg_REAL	/* in f1:f8 or in aligned doubleword		*/
} _va_arg_type;

#define	GPR_MAX		8  /* number of GPRs saved in register save area */
#define	GPR_SIZE	4  /* size of GPR saved to memory		 */
#define	FPR_MAX		8  /* number of FPRs saved in register save area */
#define	FPR_SIZE	8  /* size of FPR saved to memory		 */

/* macro to align x on a multiple of boundary */
#define	ALIGN_TO(x, boundary)	\
	(((unsigned) (x) + ((boundary)-1)) & ~((boundary)-1))


/*
 * void * __va_arg(va_list argp, _va_arg_type type)
 *
 *  argp - pointer to structure containing information on where the next
 *	   argument may be found
 *  type - type of the next argument
 *
 *  Routine to return the address at which the value of the next
 *  argument in a variable argument list is to be found.
 */

void *
__va_arg(__va_list argp, _va_arg_type type)
{
	char	*result;	/* argument's address in argument area or in  */
				/* register save area			*/
	unsigned gpr;		/* register save area GPR index		*/
	unsigned fpr;		/* register save area FPR index		*/
	unsigned size_align = 0; /* size *AND* alignment, argument passed in  */
				/* input argument area; 0 for register-passed */
				/* arguments				*/

	switch (type) {
	case arg_ARGPOINTER:
	case arg_WORD:
		gpr = argp->__gpr;

		if (gpr < GPR_MAX) {	/* passed in one GPR */
			result = argp->__reg_save_area + gpr*GPR_SIZE;
			argp->__gpr++;
		} else {	/* passed in memory */
			size_align = 4;
		}
		break;

	case arg_DOUBLEWORD:
		/* NOTE: this assumes GPR_MAX is even */
		gpr = argp->__gpr;

		if (gpr < (GPR_MAX - 1)) {
		/*
		 * passed in two GPRs with even/odd gpr indices
		 * gpr indices 0:1 correspond to r3:r4, indices 2:3 to r5:r6,
		 * indices 4:5 to r7:r8, and indices 6:7 to r9:r10
		 */
			if (gpr & 1)
				gpr++;
			result = argp->__reg_save_area + gpr*GPR_SIZE;
			argp->__gpr = gpr + 2;

		} else {	/* passed in memory */
			size_align = 8;
		}
		break;

	case arg_REAL:
		fpr = argp->__fpr;

		if (fpr < FPR_MAX) {	/* passed in one FPR */
			result = argp->__reg_save_area + GPR_MAX * GPR_SIZE +
			    fpr * FPR_SIZE;
			argp->__fpr++;

		} else {	/* passed in memory */
			size_align = 8;
		}
		break;

	default:
		break;
	}

	/* handle var arg passed in the input argument area */
	if (size_align) {
		result = (char *) ALIGN_TO(argp->__input_arg_area, size_align);
		argp->__input_arg_area = result + size_align;
	}

	/*
	 * adjust result for argument passed as address of the "real" argument
	 */
	if (type == arg_ARGPOINTER)
		result = *((char **) result);

	return (result);
}
