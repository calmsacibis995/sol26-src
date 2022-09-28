/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)_lwp_mutex_lock.s	1.4	94/07/04 SMI"

	.file	"_lwp_mutex_lock.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	_lwp_mutex_lock - mutual exclusion
 *
 *   Syntax:	int _lwp_mutex_lock(lwp_mutex_t *mp);
 *
 *   called internally by _lwp_mutex_lock(), (port/sys/lwp.c)
 */

#include "SYS.h"

	SYSREENTRY(___lwp_mutex_lock)

	SYSTRAP(lwp_mutex_lock)
	SYSINTR_RESTART(.restart____lwp_mutex_lock)

	RET

	SET_SIZE(___lwp_mutex_lock)
