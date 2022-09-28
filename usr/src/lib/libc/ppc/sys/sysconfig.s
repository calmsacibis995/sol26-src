/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)sysconfig.s	1.3	94/07/04 SMI"

	.file	"sysconfig.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	_sysconfig - get configurable system variables
 *
 *   Syntax:	int _sysconfig(int name);
 *
 */

#include "SYS.h"

	ENTRY(_sysconfig)

	SYSTRAP(sysconfig)
	SYSCERROR

	RET

	SET_SIZE(_sysconfig)
