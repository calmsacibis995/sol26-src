/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)debug.c	1.7	96/02/28 SMI"

#include	<stdio.h>
#include	<stdarg.h>
#include	<dlfcn.h>
#include	"debug.h"
#include	"msg.h"
#include	"_ld.h"

/*
 * It's possible that dbg_setup may be called more than once.
 */
static int	dbg_init = 0;

#ifndef	DEBUG

int
/* ARGSUSED0 */
dbg_setup(const char * options)
{
	if (!dbg_init) {
		eprintf(ERR_WARNING, MSG_INTL(MSG_DBG_NOINABLE));
		dbg_init++;
	}
	return (0);
}

#else

int
dbg_setup(const char * options)
{
	if (dbg_init)
		return (0);

	/*
	 * Open the linker debugging library
	 */
	if (dlopen(MSG_ORIG(MSG_FIL_LIBDBG),
	    (RTLD_LAZY | RTLD_GLOBAL)) == NULL) {
		eprintf(ERR_WARNING, MSG_INTL(MSG_DBG_DISABLE), dlerror());
		dbg_init++;
		return (0);
	}

	/*
	 * Call the debugging setup routine to initialize the mask and
	 * debug function array.
	 */
	return (Dbg_setup(options));
}

/* VARARGS1 */
void
dbg_print(const char * format, ...)
{
	va_list		args;

	(void) fputs(MSG_INTL(MSG_DBG_FMT), stderr);
	va_start(args, format);
	(void) vfprintf(stderr, format, args);
	(void) fprintf(stderr, MSG_ORIG(MSG_STR_NL));
	va_end(args);
}

#endif
