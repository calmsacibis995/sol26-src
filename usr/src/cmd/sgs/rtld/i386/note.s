/*
 *	Copyright (c) 1994 Sun Microsystems, Inc.
 */
#pragma ident	"@(#)note.s	1.3	94/09/13 SMI"

#include	"sgs.h"
#include	"profile.h"

#ifndef	lint

	.file	"note.s"

	.section	.note
	.align	4
	.long	.L20 - .L10
	.long	0
	.long	0
.L10:
	.string		SGU_PKG
	.byte		32 / " "
	.string		SGU_REL
	.byte		10 / "\n"
#ifdef	PROF
	.string		"\tprofiling enabled"
#ifdef	PRF_RTLD
	.string		"  (with mcount)"
#endif
	.string		"\n"
#endif
	.string		"\tdebugging enabled\n"
.L20:

#endif
