/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)facl.s	1.1	94/10/12 SMI"

	.file	"facl.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	facl - acl with file descriptor
 *
 *   Syntax:	int facl(int fd, int cmd, int cnt, struct aclent *buf)
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(facl,function)

#include "SYS.h"

	ENTRY(facl)

	SYSTRAP(facl)
	SYSCERROR

	RET

	SET_SIZE(facl)
