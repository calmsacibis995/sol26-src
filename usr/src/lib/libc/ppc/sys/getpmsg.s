/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)getpmsg.s	1.3	94/07/04 SMI"

	.file	"getpmsg.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	getpmsg - get next message off a stream
 *
 *   Syntax:	int getpmsg(int fildes, struct strbuf *ctlptr,
 *		struct strbuf *dataptr, int *bandp, int *flagsp);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(getpmsg,function)

#include "SYS.h"

	SYSREENTRY(getpmsg)

	SYSTRAP(getpmsg)
	SYSRESTART(.restart_getpmsg)

	RET

	SET_SIZE(getpmsg)
