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

.ident "@(#)ladd.s 1.7      94/09/09 SMI"

/*
 * Double long add routine.  Ported from pdp 11/70 version
 * with considerable effort.  All supplied comments were ported.
 *
 * Ported from m32 version to sparc. No comments about difficulty.
 *
 *	dl_t
 *	ladd (lop, rop)
 *		dl_t	lop;
 *		dl_t	rop;
 */

	.file	"ladd.s"

#include	<sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(ladd,function)

#include	"synonyms.h"

	ENTRY(ladd)

	lwz	%r11, 0(%r3)	! fetch lop.dl_lop
	lwz	%r7, 0(%r4)	! fetch rop.dl_lop
	lwz	%r12, 4(%r3)	! fetch lop.dl_hop
	lwz	%r8, 4(%r4)	! fetch rop.dl_hop
#if defined(__LITTLE_ENDIAN)
	addc	%r3, %r11, %r7	! lop.dl_lop + rop.dl_lop , (set carry)
	adde	%r4, %r12, %r8	! lop.dl_hop + rop.dl_hop + <carry>
#else	/* _BIG_ENDIAN */
	addc	%r3, %r12, %r8	! lop.dl_lop + rop.dl_lop , (set carry)
	adde	%r4, %r11, %r7	! lop.dl_hop + rop.dl_hop + <carry>
#endif	/* defined(__LITTLE_ENDIAN) */
	blr			! return result in (r3,r4) because struct is
				! only 8 bytes long (PPCABI)

	SET_SIZE(ladd)
