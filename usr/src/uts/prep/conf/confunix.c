/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.  All Rights Reserved.
 */

#ident "@(#)confunix.c	1.4	94/08/10 SMI"

#include <sys/bootconf.h>

struct bootobj rootfs = {
	{ "ufs" },	{ "/ramdisk" }
};

struct bootobj frontfs = {
	{ "" },	{ "" }
};

struct bootobj backfs = {
	{ "" },	{ "" }
};

struct bootobj swapfile = {
	{ "" },	{ "" }
};

struct bootobj dumpfile = {
	{ "" },	{ "" }
};

int nswap = 0;
