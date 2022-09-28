/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */

#ifndef _SYS_STACK_H
#define	_SYS_STACK_H

#pragma ident	"@(#)stack.h	1.7	94/06/01 SMI"

#if !defined(_ASM)

#include <sys/types.h>

#endif

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * In the Intel world, a stack frame looks like this:
 *
 * %fp0->|				 |
 *	 |-------------------------------|
 *	 |  Args to next subroutine      |
 *	 |-------------------------------|-\
 * %sp0->|  One word struct-ret address	 | |
 *	 |-------------------------------|  > minimum stack frame (8 bytes)
 *	 |  Previous frame pointer (%fp0)| |
 * %fp1->|-------------------------------|-/
 *	 |  Local variables              |
 * %sp1->|-------------------------------|
 */

/*
 * Stack alignment macros.
 */
#define	STACK_ALIGN	4
#define	SA(X)	(((X)+(STACK_ALIGN-1)) & ~(STACK_ALIGN-1))
#define	MINFRAME	0

#if defined(_KERNEL) && !defined(_ASM)

void traceback(caddr_t);
void tracedump(void);

#endif /* defined(_KERNEL) && !defined(_ASM) */

#define	STACK_GROWTH_DOWN /* stacks grow from high to low addresses */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_STACK_H */
