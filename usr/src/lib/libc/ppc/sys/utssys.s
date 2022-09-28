/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)utssys.s	1.3	94/07/04 SMI"

	.file	"utssys.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	underlying system call used by setuname(), ustat(),
 *		and fusers().
 *
 *   Syntax:	int utssys(char *cbuf, int mv, int type,
 *			char *outbufp);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(utssys,function)

#include "SYS.h"

	ENTRY(utssys)

	SYSTRAP(utssys)
	SYSCERROR

	RET

	SET_SIZE(utssys)
