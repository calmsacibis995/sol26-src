/*
 * Copyright (c) 1990-1993, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)mp_call.c	1.5	94/03/16 SMI"

/*
 * Facilities for cross-processor subroutine calls using "mailbox" interrupts.
 *
 */

#ifdef MP				/* Around entire file */

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
void
poke_cpu(cpun)
	int	cpun;
{
	extern kthread_id_t panic_thread;

	if (panic_thread)
		return;

	/*
	 * We don't need to receive an ACK from the CPU being poked,
	 * so just send out a directed interrupt.
	 */
	send_dirint(cpun, XC_CPUPOKE_PIL);
}

#endif	MP
