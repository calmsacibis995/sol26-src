
	.ident "@(#)_times.s 1.2	94/09/09 SMI"
/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	times - get process and child process times
 *
 *   Syntax:	clock_t times(struct tms *buffer);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(times,function)

#include "SYS.h"

	ENTRY(times)

	SYSTRAP(times)
	SYSCERROR

	RET

	SET_SIZE(times)
