/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 */

#pragma	ident	"@(#)libc_int.c	1.3	96/03/29 SMI"

#include	"libthread.h"

static void init_libc();
static void fini_libc();

#pragma	init(init_libc)

static void
init_libc()
{
	_set_libc_interface();
}


static void
fini_libc()
{
	_unset_libc_interface();
}
