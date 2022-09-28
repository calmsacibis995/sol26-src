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

.ident "@(#)lsub.s 1.7      94/09/09 SMI"

/*
 * Double long subtraction routine.  Ported from pdp 11/70 version
 * with considerable effort.  All supplied comments were ported.
 * Ported from m32 version to sparc. No comments about difficulty.
 *
 *	dl_t
 *	lsub (lop, rop)
 *		dl_t	lop;
 *		dl_t	rop;
 */


#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(lsub,function)

#include "synonyms.h"

	ENTRY(lsub)

	lwz	%r11, 0(%r3)	! fetch lop.dl_lop
	lwz	%r12, 4(%r3)	! fetch lop.dl_hop
	lwz	%r9, 0(%r4)	! fetch rop.dl_lop
	lwz	%r10, 4(%r4)	! fetch rop.dl_hop
#ifdef	__LITTLE_ENDIAN
	subfc	%r3, %r9, %r11	! lop.dl_lop - rop.dl_lop , (set carry)
	subfe	%r4, %r10, %r12	! lop.dl_hop - rop.dl_hop - <carry>
#else	/* _BIG_ENDIAN */
	subfc	%r4, %r10, %r12	! lop.dl_lop - rop.dl_lop , (set carry)
	subfe	%r3, %r9, %r11	! lop.dl_hop - rop.dl_hop - <carry>
#endif	/* __LITTLE_ENDIAN */
	blr			! return (r3,r4) as per ABI for structs smaller
				! than or equal to 8 bytes.

	SET_SIZE(lsub)
