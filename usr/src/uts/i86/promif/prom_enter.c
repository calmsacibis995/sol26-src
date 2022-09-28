/*
 * Copyright (c) 1992-1994, by Sun Microsystems, Inc.
 */

#pragma	ident "@(#)prom_enter.c	1.9	94/08/09 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/bootsvcs.h>

/*
 * The Intel cpu does not have an underlying monitor.
 * So, we emulate the best we can.....
 */

void
prom_enter_mon(void)
{
#ifdef KADB
	printf("Reset the system to reboot.\n");
	while (1)
		prom_getchar();
#endif

#if !defined(KADB) && !defined(I386BOOT)
	while (goany())
		int20();
#endif
}
