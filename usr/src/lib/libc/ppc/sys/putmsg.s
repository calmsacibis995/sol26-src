/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)putmsg.s	1.3	94/07/04 SMI"

	.file	"putmsg.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	putmsg - send a message on a stream
 *
 *   Syntax:	int putmsg(int fildes, const struct strbuf *ctlptr,
 *		const struct strbuf *dataptr, int flags);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(putmsg,function)

#include "SYS.h"

	SYSREENTRY(putmsg)

	SYSTRAP(putmsg)
	SYSRESTART(.restart_putmsg)

	RET

	SET_SIZE(putmsg)
