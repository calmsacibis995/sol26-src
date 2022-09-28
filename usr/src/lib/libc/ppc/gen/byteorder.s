/*
 *   Copyright (c) 1994 Sun Microsystems, Inc.  All Rights Reserved.
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	htonl, ntohl, htons, ntohs - byteorder-swapping functions
 *
 *   Syntax:	
 *
 */

.ident "@(#)byteorder.s 1.7      94/09/09 SMI"

#include <sys/asm_linkage.h>

#if defined(__LITTLE_ENDIAN)

	ENTRY2(htonl, ntohl)
        mr      %r4, %r3		! %r4 = B3 B2 B1 B0
        rlwinm  %r3, %r4, 24, 0, 31     ! %r3 = B0 B3 B2 B1
        rlwimi  %r3, %r4, 8, 8, 15      ! %r3 = B0 B1 B2 B1
        rlwimi  %r3, %r4, 8, 24, 31     ! %r3 = B0 B1 B2 B3
        blr
	SET_SIZE(htonl)
	SET_SIZE(ntohl)


	ENTRY2(htons, ntohs)
					! %r3 = 0 0 B1 B0
        rlwinm  %r4, %r3, 8, 16, 23     ! %r4 = 0 0 B0 0
        rlwimi  %r4, %r3, 24, 24, 31    ! %r4 = 0 0 B0 B1
        mr	%r3, %r4		! %r3 = 0 0 B0 B1
        blr
	SET_SIZE(htons)
	SET_SIZE(ntohs)

#else	/* defined(__BIG_ENDIAN) */

	ENTRY2(htonl, ntohl)
	blr
	SET_SIZE(htonl)
	SET_SIZE(ntohl)

	ENTRY2(htons, ntohs)
	blr
	SET_SIZE(htons)
	SET_SIZE(ntohs)

#endif	/* defined(__LITTLE_ENDIAN) */
