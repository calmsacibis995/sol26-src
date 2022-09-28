
/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 */

#pragma	ident	"@(#)rtld_int.c	1.3	96/03/29 SMI"

#include	"libthread.h"

static void init_rtld();
static void fini_rtld();

#pragma	init(init_rtld)

static void
init_rtld()
{
	_set_rtld_interface();
}


static void
fini_rtld()
{
	_unset_rtld_interface();
}
