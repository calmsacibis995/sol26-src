/*
 * Copyright (c) 1992-1994, by Sun Microsystems, Inc.
 */

#pragma ident "@(#)prom_panic.c	1.8	94/08/09 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

#ifdef I386BOOT
#ifdef i86pc
#include <sys/bootsvcs.h>
#endif
#else
#include <sys/bootsvcs.h>
#endif

void
prom_panic(char *s)
{
#ifdef I386BOOT
	if (!s)
		s = "unknown panic";
	prom_printf("prom_panic: %s\n", s);

	while (1)
		;
#endif

#ifdef KADB
	(printf(s));
	while (1) {
		prom_getchar();
		printf(s);
	}

#endif

#if !defined(KADB) && !defined(I386BOOT)
	printf(s);
	while (goany())
		int20();
#endif
}
