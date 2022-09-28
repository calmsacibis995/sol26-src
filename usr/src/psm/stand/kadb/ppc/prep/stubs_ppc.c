/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)stubs_ppc.c	1.2	95/02/22 SMI"

#include <sys/bootconf.h>

extern struct bootops *bootops;

mountroot()
{
}

open(char *name, int mode)
{
	return (BOP_OPEN(bootops, name, mode));
}

read(int fd, char *buf, int count)
{
	return (BOP_READ(bootops, fd, buf, count));
}

close()
{
}

reopen()
{
}

/* ARGSUSED3 */
lseek(int fd, int pos, int whence)
{
	/*
	 * Current version of standalone lseek() uses high and low offsets
	 * for the second and third parameters, apparently anticipating
	 * long long offsets.  That's completely bogus for kadb, since
	 * kadb gets its seek offsets from ELF header fields that are
	 * limited by the ABI to 32 bits.  Rather than change lseek()s
	 * all over kadb, the swindle is done here.
	 */
	return (BOP_SEEK(bootops, fd, 0, pos));
}

exitto(int (*go2)())
{
	_exitto(go2);
}
