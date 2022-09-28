/*
 * Copyright (c) 1991-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_panic.c	1.8	95/02/17 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

void
prom_panic(char *s)
{
	if (!s)
		s = "unknown panic";
	prom_printf("panic - %s: %s\n", promif_clntname, s);
	prom_enter_mon();
	prom_printf("panic: prom_enter_mon() returned unexpectedly\n");
	prom_printf("panic: looping forever ...\n");
	for (;;)
		;
}
