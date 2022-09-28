/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)zero.s	1.10	96/06/07 SMI"

#if	defined(lint)

#include <sys/types.h>

int blockzero(int *);

void
zero(char * s, int size, int hint)
{
	/* LINTED */
	int * new = (int *)s;
	char * newc;

top:
	if (size == 0)
		return;

	if (((int)new & (hint - 1)) == 0) {
		int loops = size / hint;
		int cnt = loops;
		while (loops--) {
			blockzero(new);
			*new += hint;
		}
		if (cnt)
			size -= (cnt * hint);
		goto top;
	}
	if (((int)new & (sizeof (int) - 1)) == 0) {
		*new++ = 0;
		size -= sizeof (int);
		goto top;
	}
	newc = (char *)new;
	*newc++ = (char)0;
	size--;
	goto top;
}
#else

#include <sys/asm_linkage.h>

	.file "zero.s"
	.text
	.align	4

	ENTRY(zero)
	li	%r10,0			! r10 is src of zeros
	addi	%r6, %r5, -1		! cache line mask
.top:
	cmpi	%r4,0
	bne+	.loop			! if (size != 0) goto .loop
	blr
.loop:
	and.	%r9,%r3,%r6
	bne+	.align			! are we cacheline aligned
	sri	%r7,%r4,5		! r7 is loops
	li	%r4, 0
	mtctr	%r7
.zloop:
	dcbz	0, %r3
	add	%r3, %r3, %r5
	bdnz+	.zloop
	blr
.align:
	andi.	%r9,%r3,3
	bne-	.byte
	stw	%r10,0(%r3)		! store word 0
	addi	%r3,%r3,4
	addi	%r4,%r4,-4
	b	.top
.byte:
	stb	%r10,0(%r3)		! store byte 0
	addi	%r3,%r3,1
	addi	%r4,%r4,-1
	b	.top
	SET_SIZE(zero)
#endif
