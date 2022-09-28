/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)lwp_private.c	1.3	94/11/30 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/stack.h>
#include <sys/reg.h>
#include <sys/frame.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/archsystm.h>
#include <sys/debug.h>

/*
 * Get the LWP's "thread specific data".
 */
/* ARGSUSED */
int
lwp_getprivate()
{
	klwp_t *lwp = ttolwp(curthread);
	struct regs *regs = lwptoregs(lwp);

	return (regs->r_r2);
}

/*
 * Setup the LWP's "thread specific data".
 */
/* ARGSUSED */
int
lwp_setprivate(void *bp)
{
	klwp_t *lwp = ttolwp(curthread);
	struct regs *regs = lwptoregs(lwp);

	regs->r_r2 = (int)bp;
	return (0);
}
