/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)secmod.c	1.8	96/04/25 SMI"	/* SVr4.0 1.7	*/

#include <sys/types.h>
#include <sys/modctl.h>
#include <sys/errno.h>

char _depends_on[] = "strmod/rpcmod";

/*
 * Module linkage information for the kernel.
 */

static struct modlmisc modlmisc = {
	&mod_miscops, "kernel RPC security module."
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modlmisc,
	NULL
};

extern void sec_subrinit();

_init()
{
	int retval;

	if ((retval = mod_install(&modlinkage)) != 0)
		return (retval);

	sec_subrinit();

	return (0);
}

_fini()
{
	return (EBUSY);
}

_info(modinfop)
	struct modinfo *modinfop;
{
	return (mod_info(&modlinkage, modinfop));
}
