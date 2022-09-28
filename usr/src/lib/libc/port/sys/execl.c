/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)execl.c	1.14	96/03/20 SMI"	/* SVr4.0 1.5.1.6	*/

/*
 *	execl(name, arg0, arg1, ..., argn, 0)
 *	environment automatically passed.
 */

#ifdef __STDC__
	#pragma weak execl = _execl
#endif
#include <alloca.h>
#include <stdarg.h>
#include <sys/types.h>

#include "synonyms.h"

extern int execve();

/*VARARGS1*/
int
#ifdef __STDC__
execl(char *name, ...)
#else
execl(name, va_alist)
	char *name;
	va_dcl
#endif
{
        char **argp;
	va_list args;
	char **argvec;
	extern char **environ;
	int err;
	int nargs = 0;
	char *nextarg;

	/*
	 * count the number of arguments in the variable argument list
	 * and allocate an argument vector for them on the stack,
	 * adding an extra element for a terminating null pointer
	 */

#ifdef __STDC__
	va_start(args,);
#else
	va_start(args);
#endif
	while (va_arg(args, char *) != (char *)0) {
	        nargs++;
	}
	va_end(args);

	/*
	 * load the arguments in the variable argument list
	 * into the argument vector and add the terminating null pointer
	 */

#ifdef __STDC__
	va_start(args,);
#else
	va_start(args);
#endif
	/* workaround for bugid 1242839 */
	argvec = (char **)alloca((size_t)((nargs + 1) * sizeof(char *)));
	nextarg = va_arg(args, char *);
	argp = argvec;
	while (nextarg != (char *)0) {
		*argp = nextarg;
		argp++;
		nextarg = va_arg(args, char *);
	}
	va_end(args);
	*argp = (char *)0;

	/*
	 * call execve()
	 */

	err = execve(name, argvec, environ);
	return(err);
}
