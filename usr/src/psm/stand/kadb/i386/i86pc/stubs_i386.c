/*
 * Copyright (c) 1990-1996, Sun Microsystems, Inc. All Rights Reserved.
 */

#ident "@(#)stubs_i386.c	1.5	96/10/15 SMI"

#include <sys/bootconf.h>

mountroot()
{
}

open(name, mode)
	char *name;
	int mode;
{
	return (prom_open(name));
}

read(fd, buf, count)
	int fd;
	char *buf;
	int count;
{
	return (prom_read(fd, buf, count, 0, 0));
}

close(int fd)
{
	return (prom_close(fd));
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
/* for our port (i386), our bootsvcs call only requires two arguments; */
/* the "whence" value is assumed to be the beginning of the file. */

	return (prom_seek(fd, (unsigned long long)pos));
}

exitto(go2)
	int (*go2)();
{
	_exitto(go2);
}
