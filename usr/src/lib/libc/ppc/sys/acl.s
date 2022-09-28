/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)acl.s	1.1	94/10/12 SMI"

	.file	"acl.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	acl - access control list support
 *
 *   Syntax:	acl(const char *path, int cmd, int cnt, struct aclent *buf)
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(acl,function)

#include "SYS.h"

	ENTRY(acl)

	SYSTRAP(acl)
	SYSCERROR

	RET

	SET_SIZE(acl)
