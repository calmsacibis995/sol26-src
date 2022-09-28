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

.ident "@(#)lsign.s 1.5      94/09/09 SMI"

/*
 * Determine the sign of a double-long number.
 * Ported from m32 version to sparc.
 *
 *	int
 *	lsign (op)
 *		dl_t	op;
 */

	.file	"lsign.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(lsign,function)

#include "synonyms.h"

	ENTRY(lsign)

#ifdef	__LITTLE_ENDIAN
	lwz	%r4, 4(%r3)		! MSB in upper word
#else	/* _BIG_ENDIAN */
	lwz	%r4, 0(%r3)		! MSB in lower word
#endif	/* __LITTLE_ENDIAN */
	rlwinm	%r3, %r4, 1, 31, 31 	! shift MSB right logical to isolate sign
	blr				! return

	SET_SIZE(lsign)
