/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)setrlimit.s	1.7	96/02/26 SMI"

	.file	"setrlimit.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	setrlimit - control maximum system resource
 *		consumption
 *
 *   Syntax:	int setrlimit(int resource, const struct rlimit *rlp);
 *
 */

#include <sys/asm_linkage.h>

#if (_FILE_OFFSET_BITS != 64)
	ANSI_PRAGMA_WEAK(setrlimit,function)
#else
	ANSI_PRAGMA_WEAK(setrlimit64,function)
#endif
	
#include "SYS.h"

#if (_FILE_OFFSET_BITS != 64)

	ENTRY(setrlimit)

	SYSTRAP(setrlimit)
	SYSCERROR

	RET

	SET_SIZE(setrlimit)

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	setrlimit64 - control maximum system resource
 *		consumption
 *
 *   Syntax:	int setrlimit64(int resource, const struct rlimit64 *rlp);
 *
 */

#else	/* _FILE_OFFSET_BITS == 64) */

	ENTRY(setrlimit64)

	SYSTRAP(setrlimit64)
	SYSCERROR

	RET

	SET_SIZE(setrlimit64)

#endif
