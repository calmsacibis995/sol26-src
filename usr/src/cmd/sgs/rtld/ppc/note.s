/*
 *	Copyright (c) 1991,1992,1993,1994 by Sun Microsystems, Inc.
 */
#pragma ident	"@(#)note.s	1.6	94/10/12 SMI"

#include	"sgs.h"
#include	"profile.h"

#ifndef	lint

	.file	"note.s"

	.section	".note"
	.align	4
	.word	.L20 - .L10
	.word	0
	.word	0
.L10:
	.ascii		SGU_PKG
	.byte		" "
	.ascii		SGU_REL
	.byte		"\n"
#ifdef	PROF
	.ascii		"\tprofiling enabled"
#ifdef	PRF_RTLD
	.ascii		"  (with mcount)"
#endif
	.ascii		"\n"
#endif
	.ascii		"\tdebugging enabled\n"
.L20:

#endif
