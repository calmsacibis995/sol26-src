/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)getmsg.s	1.3	94/07/04 SMI"

	.file	"getmsg.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	getmsg - get next message off a stream
 *
 *   Syntax:	int getmsg(int fildes, struct strbuf *ctlptr,
 *		struct strbuf *dataptr, int *flagsp);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(getmsg,function)

#include "SYS.h"

	SYSREENTRY(getmsg)

	SYSTRAP(getmsg)
	SYSRESTART(.restart_getmsg)

	RET

	SET_SIZE(getmsg)
