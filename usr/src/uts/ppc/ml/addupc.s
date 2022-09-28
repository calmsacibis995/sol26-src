/*
 *	Copyright (c) 1993 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)addupc.s	1.7	94/12/21 SMI"

#include <sys/asm_linkage.h>

/*
 *  struct uprof {
 *  	short	 *pr_base;
 *  	unsigned pr_size;
 *  	unsigned pr_off;
 *  	unsigned pr_scale;
 *  } ;
 *  addupc( pc, pr, incr)
 *  	register void (*pc)();
 *  	register struct uprof *pr;
 *  	int incr;
 *  {
 *  	register short *slot;
 *  	short counter;
 *  
 *  	if (pr->pr_scale == 2)
 *  		slot = pr->pr_base;
 *  	else
 *  		slot = pr->pr_base + 
 *  			((((int)pc-pr->pr_off) * pr->pr_scale) >> 16)/
 * 					(sizeof *slot);
 *  	if (slot >= pr->pr_base &&
 *  	    slot < (short *)(pr->pr_size + (int)pr->pr_base)) {
 *  		if ((counter=fusword(slot))<0) {
 *  			pr->pr_scale = 0;
 *  		} else {
 *  			counter += incr;
 *  			susword(slot, counter);
 *  		}
 *  	}
 *  }
 */

#if defined(lint)

#include <sys/lwp.h>	/* for definition of struct prof */

/* ARGSUSED */
void
addupc(void (*pc)(), struct prof *pr, int incr)
{}

#else	/* lint */

	.text
	.set	PR_BASE,0
	.set	PR_SIZE,4
	.set	PR_OFF,8
	.set	PR_SCALE,12

	ENTRY(addupc)
	lwz	%r6,PR_SCALE(%r4)	! r6 is scale
	lwz	%r7,PR_BASE(%r4)	! r7 is base
	cmpi	%r6,2
	mr	%r9,%r7			! r9 is slot
	beq-	.L1
	lwz	%r8,PR_OFF(%r4)
	subf	%r8,%r8,%r3		! pc - pr->pr_off
	mullw	%r8,%r8,%r6
	rlwinm	%r8,%r8,16,16,30
	add	%r9,%r7,%r8
	cmpl	%r9,%r7			! slot >= pr->pr_base?
	blt	.done
.L1:	lwz	%r8,PR_SIZE(%r4)
	add	%r8,%r8,%r7
	cmpl	%r9,%r8			! slot < size + base?
	bge	.done
! we need to make the calls, so build the frame.
	mflr	%r0
	stwu	%r1,-0x20(%r1)
	stw	%r0,0x24(%r1)
	stw	%r4,0x10(%r1)		! pr
	stw	%r5,0x14(%r1)		! incr
	stw	%r9,0x18(%r1)		! slot
	mr	%r3,%r9
	bl	fusword
	cmpi	%r3,0			! if counter < 0
	blt	.scale0
	lwz	%r4,0x14(%r1)		! incr
	add	%r4,%r4,%r3		!      + counter
	lwz	%r3,0x18(%r1)
	bl	susword
.tail:	lwz	%r0,0x24(%r1)
	addi	%r1,%r1,0x20
	mtlr	%r0
.done:	blr

.scale0:
	lwz	%r3,0x10(%r1)
	li	%r4,0
	stw	%r4,PR_SCALE(%r3)
	b	.tail
	SET_SIZE(addupc)

#endif	/* lint */
