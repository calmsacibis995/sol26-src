/*
 * Copyright (c) 1995,1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)rpcsec_gssmod.c	1.8	96/04/25 SMI"	/* SVr4.0 1.7	*/

#include <sys/types.h>
#include <sys/modctl.h>
#include <sys/errno.h>

char _depends_on[] = "strmod/rpcmod";

/*
 * Module linkage information for the kernel.
 */

static struct modlmisc modlmisc = {
	&mod_miscops, "kernel RPCSEC_GSS security service."
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modlmisc,
	NULL
};

_init()
{
	int retval;

	if ((retval = mod_install(&modlinkage)) != 0)
		return (retval);

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
