/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)util.c	1.10	96/02/28 SMI"

/*
 * Utility functions
 */
#include	<stdarg.h>
#include	<unistd.h>
#include	<stdio.h>
#include	<signal.h>
#include	<locale.h>
#include	"msg.h"
#include	"_ld.h"

static Ofl_desc *	Ofl;

/*
 * Exit after cleaning up
 */
void
ldexit()
{
	(void) signal(SIGINT, SIG_IGN);
	(void) signal(SIGQUIT, SIG_DFL);
	(void) signal(SIGTERM, SIG_IGN);
	(void) signal(SIGHUP, SIG_DFL);

	/*
	 * If we have created an output file remove it.
	 */
	if (Ofl->ofl_fd > 0)
		(void) unlink(Ofl->ofl_name);
	ld_atexit(EXIT_FAILURE);
	exit(EXIT_FAILURE);
	/* NOTREACHED */
}

/*
 * Print a message to stdout
 */
/* VARARGS2 */
void
eprintf(Error error, const char * format, ...)
{
	va_list			args;
	static const char *	strings[ERR_NUM] = {MSG_ORIG(MSG_STR_EMPTY)};


	if (error > ERR_NONE) {
		if (error == ERR_WARNING) {
			if (strings[ERR_WARNING] == 0)
			    strings[ERR_WARNING] = MSG_INTL(MSG_ERR_WARNING);
		} else if (error == ERR_FATAL) {
			if (strings[ERR_FATAL] == 0)
			    strings[ERR_FATAL] = MSG_INTL(MSG_ERR_FATAL);
		} else if (error == ERR_ELF) {
			if (strings[ERR_ELF] == 0)
			    strings[ERR_ELF] = MSG_INTL(MSG_ERR_ELF);
		}
		(void) fputs(MSG_ORIG(MSG_STR_LDDIAG), stderr);
	}
	(void) fputs(strings[error], stderr);

	va_start(args, format);
	(void) vfprintf(stderr, format, args);
	if (error == ERR_ELF) {
		int	elferr;

		if ((elferr = elf_errno()) != 0)
			(void) fprintf(stderr, MSG_ORIG(MSG_STR_ELFDIAG),
			    elf_errmsg(elferr));
	}
	(void) fprintf(stderr, MSG_ORIG(MSG_STR_NL));
	(void) fflush(stderr);
	va_end(args);
}

/*
 * Trap signals so as to call ldexit(), and initialize allocator symbols.
 */
int
init(Ofl_desc * ofl)
{
	/*
	 * Initialize the output file descriptor address for use in the
	 * signal handler routine.
	 */
	Ofl = ofl;

	if (signal(SIGINT, (void (*)(int)) ldexit) == SIG_IGN)
		(void) signal(SIGINT, SIG_IGN);
	if (signal(SIGHUP, (void (*)(int)) ldexit) == SIG_IGN)
		(void) signal(SIGHUP, SIG_IGN);
	if (signal(SIGQUIT, (void (*)(int)) ldexit) == SIG_IGN)
		(void) signal(SIGQUIT, SIG_IGN);

	return (1);
}


const char *
_ld_msg(int mid)
{
	return (gettext(MSG_ORIG(mid)));
}
