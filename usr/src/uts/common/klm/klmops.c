/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident  "@(#)klmops.c	1.1	95/03/16 SMI"

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/modctl.h>

static struct modlmisc modlmisc = {
	&mod_miscops, "lock mgr calls"
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlmisc, NULL
};

char _depends_on[] = "strmod/rpcmod fs/nfs misc/klmmod";

_init()
{
	return (mod_install(&modlinkage));
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
