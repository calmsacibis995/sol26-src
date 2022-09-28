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

.ident "@(#)lshiftl.s 1.7      94/09/09 SMI"

/*
 * Shift a double long value. Ported from m32 version to sparc.
 *
 *	dl_t
 *	lshiftl (op, cnt)
 *		dl_t	op;
 *		int	cnt;
 */

/*
 * typedef struct dl {
 *	unsigned l;
 *	long	h;
 * } dl_t;
 *
 * dl_t
 * lshiftl( dl_t op, int cnt)
 * {
 *	dl_t ret;
 *
 *	if (cnt > 0) {
 *		if (cnt > 63) {
 *			ret.l = 0;
 *			ret.h = 0;
 *			return (ret);
 *		}
 *		if (cnt >= 32) {
 *			ret.l = 0;
 *			ret.h = (op.l << (cnt-32));
 *			return ret;
 *		}
 *		ret.l = (op.l << cnt);
 *		ret.h = (op.h << cnt) | (op.l >> (cnt-32));
 *		return ret;
 *	}
 *
 *	if (cnt < 0) {
 *		if (cnt < -63) {
 *			ret.l = 0;
 *			ret.h = 0;
 *			return (ret);
 *		}
 *		if (cnt >= 32){
 *			ret.l = op.h >> (cnt-32);
 *			ret.h = 0;
 *			return ret;
 *		}
 *		ret.l = (op.l >> cnt) | (op.h << (32 -cnt));
 *		ret.h = (op.h >> cnt);
 *		return ret;
 *	}
 *
 *
 *	ret.l = op.l;
 *	ret.h = op.h;
 *	return ret;
 * }
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(lshiftl,function)

#include "synonyms.h"

	ENTRY(lshiftl)

	cmpi	%r4, 0		! test cnt < 0 and 
	bgt	.shiftl		! shift left
	blt	.shiftr		! shift right
#ifdef	__LITTLE_ENDIAN
	lwz	%r11, 0(%r3)
	lwz	%r12, 4(%r3)	! fetch op.lop, op.hop
#else	/* _BIG_ENDIAN */
	lwz	%r11, 4(%r3)
	lwz	%r12, 0(%r3)	! fetch op.lop, op.hop
#endif	/* __LITTLE_ENDIAN */
	mr	%r3, %r11
	mr	%r4, %r12
	blr			! return unshifted in (r3,r4)

.shiftl:
				! Positive shift (left)
#ifdef	__LITTLE_ENDIAN
	lwz	%r11, 0(%r3)
	lwz	%r12, 4(%r3)	! fetch op.lop, op.hop
#else	/* _BIG_ENDIAN */
	lwz	%r11, 4(%r3)
	lwz	%r12, 0(%r3)	! fetch op.lop, op.hop
#endif
	cmpi	%r4, 63		! Reduce range to 0..63
	ble	.doshfl
	li	%r3, 0
	li	%r4, 0
	blr			! return (dl_t)0;

.doshfl:
	cmpi	%r4, 32
	blt	.smallshfl
	li	%r0, 0		! ret.l = 0;
	subi	%r4, %r4, 32
#ifdef	__LITTLE_ENDIAN
	slw	%r4, %r11, %r4	! ret.h = (op.l << (cnt-32));
	mr	%r3, %r0
#else	/* _BIG_ENDIAN */
	slw	%r3, %r11, %r4	! ret.h = (op.l << (cnt-32));
	mr	%r4, %r0
#endif	/* __LITTLE_ENDIAN */
	blr			! return (r3,r4);

.smallshfl:
	slw	%r0, %r11, %r4		! ret.l = (op.l << cnt);
	slw	%r3, %r12, %r4		! r3 = (op.h << cnt);
	neg	%r6, %r4
	addi	%r5, %r6, 32		! r5 = (32 - cnt);
	srw	%r12, %r11, %r5		! r12 = (op.l >> (32 - cnt));
#ifdef	__LITTLE_ENDIAN
	or	%r4, %r12, %r3		! ret.h = r3 | r12
	mr	%r3, %r0
#else	/* _BIG_ENDIAN */
	or	%r3, %r12, %r3		! ret.h = r3 | r12
	mr	%r4, %r0
#endif	/* __LITTLE_ENDIAN */
	blr				! return (r3,r4);

.shiftr:			! Negative shift (right)

#ifdef	__LITTLE_ENDIAN
	lwz	%r11, 0(%r3)
	lwz	%r12, 4(%r3)	! fetch op.lop, op.hop
#else	/* _BIG_ENDIAN */
	lwz	%r11, 4(%r3)
	lwz	%r12, 0(%r3)	! fetch op.lop, op.hop
#endif
	cmpi	%r4, -63	! Reduce range to 0..63
	bge	.doshfr
	li	%r3, 0
	li	%r4, 0
	blr			! return (dl_t)0;

.doshfr:
	neg	%r6, %r4	! r6 = -(cnt);
	cmpi	%r6, 32
	blt	.smallshfr

	subi	%r6, %r6, 32	! r6 = (cnt - 32)
#ifdef	__LITTLE_ENDIAN
	li	%r4, 0		! ret.h = 0
	srw	%r3, %r12, %r6	! ret.l = op.h >> (cnt-32);
#else	/* _BIG_ENDIAN */
	li	%r3, 0		! ret.h = 0
	srw	%r4, %r12, %r6	! ret.l = op.h >> (cnt-32);
#endif
	blr			! return (r3,r4)

.smallshfr:
	srw	%r3, %r12, %r6	! ret.h = (op.h >> cnt);
	srw	%r0, %r11, %r6	! r0 = (op.l >> cnt);
	neg	%r6, %r6
	addi	%r5, %r6, 32	! r5 = (32 - cnt);
	slw	%r11, %r12, %r5	! r11= (op.h << (32 - cnt));
#ifdef	__LITTLE_ENDIAN
	mr	%r4, %r3
	or	%r3, %r11, %r0	! ret.l = r0 | r11
#else	/* _BIG_ENDIAN */
	or	%r4, %r11, %r0	! ret.l = r0 | r11
#endif	/* __LITTLE_ENDIAN */
	blr			! return (r3,r4)

	SET_SIZE(lshiftl)
