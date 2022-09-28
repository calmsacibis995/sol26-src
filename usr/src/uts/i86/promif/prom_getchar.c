/*
 * Copyright (c) 1992-1993, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)prom_getchar.c	1.10	95/01/10 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/bootsvcs.h>

/*
 * prom_getchar
 *
 * always returns a character; waits for input if none is available.
 */
u_char
prom_getchar(void)
{
	int c;
	while ((c = ischar()) == 0)
		;
	c = getchar();

	return (c);
}

/*
 * prom_mayget
 *
 * returns a character from the keyboard if one is available,
 * otherwise returns a -1.
 */

int
prom_mayget(void)
{
	if (ischar() == 0)
		return (-1);
	else
		return (getchar());

}
