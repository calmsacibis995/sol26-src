/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_VA_LIST_H
#define	_SYS_VA_LIST_H

#pragma ident	"@(#)va_list.h	1.6	96/01/26 SMI"

/*
 * This file is system implementation and generally should not be
 * included directly by applications.  It serves to resolve the
 * conflict in ANSI-C where the prototypes for v*printf are required
 * to be in <stdio.h> but only applications which reference these
 * routines are required to have previously included <stdarg.h>.
 * It also provides a clean way to allow either the ANSI <stdarg.h>
 * or the historical <varargs.h> to be used.
 */

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(__ppc)

typedef struct __va_list_tag {
	char __gpr;		/* index into the array of 8 GPRs	*/
				/* stored in the register save area;	*/
				/* gpr=0 corresponds to r3, gpr=1 to r4	*/
	char __fpr;		/* index into the array of 8 FPRs	*/
				/* stored in the register save area;	*/
				/* fpr=0 corresponds to f1, fpr=1 to f2	*/
	char *__input_arg_area;	/* location in input argument area	*/
				/* which may have the next var arg that	*/
				/* was passed in memory.		*/
	char *__reg_save_area;	/* where r3:r10 and f1:f8 (if saved)	*/
				/* are stored				*/
} __va_list[1];

#else	/* defined(__ppc) -- SPARC and Intel x86 */

#ifdef __STDC__
typedef void *__va_list;
#else
typedef char *__va_list;
#endif

#endif	/* defined(__ppc) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_VA_LIST_H */
