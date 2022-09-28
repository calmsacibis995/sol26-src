/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */

#ifndef _SYS_STACK_H
#define	_SYS_STACK_H

#pragma ident	"@(#)stack.h	1.7	94/08/10 SMI"

#if !defined(_ASM)

#include <sys/types.h>

#endif	/* !defined(_ASM) */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * A stack frame looks like:
 *
 *		|-------------------------------|	HIGH ADDRESS
 *		|  Locals, temps, saved globals |
 *		|  and floats			|
 *		|-------------------------------|
 *		|  aligned outgoing parameters	|
 *		|  that do not fit in registers	|
 *		|-------------------------------|-.
 *		|  LR save area for callee	| |
 *		|-------------------------------| |--> minimum stack frame
 * 		|  Back chain (saved r1)	| |
 * %sp = %r1--->|-------------------------------|-'	LOW ADDRESS
 */

/*
 * Constants defining a stack frame.
 */
#define	MINFRAME	8 		/* min frame */

/*
 * Stack alignment macros.
 */
#define	STACK_ALIGN	16
#define	SA(X)	(((X)+(STACK_ALIGN-1)) & ~(STACK_ALIGN-1))

#if defined(_KERNEL) && !defined(_ASM)

void traceback(caddr_t);
void tracedump(void);

#endif	/* defined(_KERNEL) && !defined(_ASM) */

#define	STACK_GROWTH_DOWN /* stacks grow from high to low addresses */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_STACK_H */
