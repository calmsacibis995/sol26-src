/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)putpmsg.s	1.3	94/07/04 SMI"

	.file	"putpmsg.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	putpmsg - send a message on a stream
 *
 *   Syntax:	int putpmsg(int fildes, const struct strbuf *ctlptr,
 *		const struct strbuf *dataptr, int band, int flags);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(putpmsg,function)

#include "SYS.h"

	SYSREENTRY(putpmsg)

	SYSTRAP(putpmsg)
	SYSRESTART(.restart_putpmsg)

	RET

	SET_SIZE(putpmsg)
