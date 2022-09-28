/*
 *   Copyright (c) 1994 Sun Microsystems, Inc.  All Rights Reserved.
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	
 *
 *   Syntax:	
 *
 */

.ident "@(#)gethrtime.s 1.6      94/09/09 SMI"

#include "SYS.h"
#include <sys/trap.h>

/*
 * hrtime_t gethrtime(void)
 * Fast syscall
 * Returns the current hi-res real time.
 */

	ENTRY(gethrtime)
	li	%r0, -1
	li	%r3, SC_GETHRTIME
	sc
	blr
	SET_SIZE(gethrtime)

/*
 * hrtime_t gethrvtime(void)
 * Fast syscall
 * Returns the current hi-res LWP virtual time.
 */

	ENTRY(gethrvtime)
	li	%r0, -1
	li	%r3, SC_GETHRVTIME
	sc
	blr
	SET_SIZE(gethrvtime)
