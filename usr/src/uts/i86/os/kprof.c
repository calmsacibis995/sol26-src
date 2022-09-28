/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.
 */

#pragma	ident	"@(#)@(#)kprof.c	1.5	94/03/09 SMI"

/*
 * profile driver - initializes profile arrays for the kernel and
 * and loaded modules.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/t_lock.h>
#include <sys/errno.h>
#include <sys/gprof.h>
#include <sys/cpuvar.h>
#include <sys/conf.h>

#ifdef GPROF

int
kprof()
{
	extern char *profiling_pc;
	char *frompc = profiling_pc;
	register long offset;
	register struct kern_profiling *prof;

	/* NEEDSWORK?  if not supervisor, return */

	prof = curcpup()->cpu_profiling;

	if (prof == NULL || prof->profiling != PROFILE_ON)
		return;

	if (frompc >= prof->module_lowpc && frompc < prof->module_highpc) {
		offset = frompc - prof->kernel_lowpc;
		offset -= prof->module_lowpc - prof->kernel_highpc;
	} else if (frompc >= prof->kernel_lowpc &&
	    frompc < prof->kernel_highpc) {
		offset = frompc - prof->kernel_lowpc;
	} else
		return;

#ifdef XXX_PROF_FIXED
	++prof->kcount[offset >> 2];	/* use offset/4 as index */
#endif

	return;
}

#endif	/* GPROF */
