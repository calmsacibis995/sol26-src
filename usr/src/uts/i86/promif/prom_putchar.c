/*
 * Copyright (c) 1992-1994, by Sun Microsystems, Inc.
 */

#pragma ident "@(#)prom_putchar.c	1.10	95/01/10 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/bootsvcs.h>

/*ARGSUSED*/
int
prom_mayput(char c)
{
	return (0);
}

void
prom_putchar(char c)
{
	static int bhcharpos = 0;

	if (c == '\t') {
			do {
				putchar(' ');
			} while (++bhcharpos % 8);
			return;
	} else  if (c == '\n' || c == '\r') {
			bhcharpos = 0;
			putchar(c);
			return;
	} else if (c == '\b') {
			if (bhcharpos)
				bhcharpos--;
			putchar(c);
			return;
	}

	bhcharpos++;
	putchar(c);
}
