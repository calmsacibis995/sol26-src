/*
 * Copyright (c) 1990-1993, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)mp_call.c	1.7	96/05/29 SMI"

/*
 * Facilities for cross-processor subroutine calls using "mailbox" interrupts.
 *
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/thread.h>
#include <sys/cpuvar.h>
#include <sys/machsystm.h>

/*
 * Interrupt another CPU.
 * 	This is useful to make the other CPU go through a trap so that
 *	it recognizes an address space trap (AST) for preempting a thread.
 *
 *	It is possible to be preempted here and be resumed on the CPU
 *	being poked, so it isn't an error to poke the current CPU.
 *	We could check this and still get preempted after the check, so
 *	we don't bother.
 */
/*ARGSUSED*/
void
poke_cpu(int cpun)
{
	extern kthread_id_t panic_thread;

	if (panic_thread)
		return;

	/*
	 *	NOP for now.
	 */
}
