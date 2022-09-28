	.ident "@(#)hrestime.s 1.5	94/09/09 SMI"
#include <sys/asm_linkage.h>
#include "SYS.h"
#include <sys/trap.h>

/*
 * hrestime(tval)
 *	timestruc_t *tval;
 */
	ENTRY(hrestime);
	stwu	%r1, -16(%r1)
	stw	%r3, +8(%r1)
	li	%r0, -1
	li	%r3, SC_GETHRESTIME
	sc
	lwz	%r5, +8(%r1)
	addi	%r1, %r1, 16
	stw	%r3, 0(%r5)	! secs
	stw	%r4, 4(%r5)	! nsecs
	blr
	SET_SIZE(hrestime)
