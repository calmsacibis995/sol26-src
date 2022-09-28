/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 */

	.ident	"@(#)__signotify.s	1.1	95/11/01 SMI"

	.file	"__signotify.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	__signotify - manipulate notifications
 *
 *   Syntax:	int __signotify(int cmd, siginfo_t *siginfo,
 *				signotify_id_t *sn_id);
 *
 *   unpublished system call, called internally by mq_notify(),
 *   (libposix4/common/sigrt.c)
 */

#include "SYS.h"

	ENTRY(__signotify)

	SYSTRAP(signotify)
	SYSCERROR

	RET

	SET_SIZE(__signotify)
