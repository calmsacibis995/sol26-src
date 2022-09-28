/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)getrlimit.s	1.5	96/02/26 SMI"

	.file	"getrlimit.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	getrlimit - control maximum system resource
 *		consumption
 *
 *   Syntax:	int getrlimit(int resource, struct rlimit *rlp);
 *
 */

#include <sys/asm_linkage.h>

#if (_FILE_OFFSET_BITS != 64)
	ANSI_PRAGMA_WEAK(getrlimit,function)
#else
	ANSI_PRAGMA_WEAK(getrlimit64,function)
#endif

#include "SYS.h"

#if (_FILE_OFFSET_BITS != 64)
	
	ENTRY(getrlimit)

	SYSTRAP(getrlimit)
	SYSCERROR

	RET

	SET_SIZE(getrlimit)

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	getrlimit64 - control maximum system resource
 *		consumption
 *
 *   Syntax:	int getrlimit64(int resource, struct rlimit64 *rlp);
 *
 */

#else
	
	ENTRY(getrlimit64)

	SYSTRAP(getrlimit64)
	SYSCERROR

	RET

	SET_SIZE(getrlimit64)

#endif
