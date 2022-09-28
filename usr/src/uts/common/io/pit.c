/*
 * Copyright (c) 1990-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1987, 1988 Microsoft Corporation	*/
/*	  All Rights Reserved	*/

/*	This Module contains Proprietary Information of Microsoft  */
/*	Corporation and should be treated as Confidential.	   */

#pragma ident	"@(#)pit.c	1.21	96/05/16 SMI"

#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/archsystm.h>

#include <sys/clock.h>
#include <sys/debug.h>
#include <sys/smp_impldefs.h>
#include <sys/rtc.h>

/*
 * This file contains all generic part of clock and timer handling.
 * Specifics are now in a seperate file and may be overridden by OEM
 * modules which get loaded. Defaults come from i8254.c and hardclk.c
 */
unsigned long microdata = 50;	/* loop count for 10 microsecond wait. */
				/* MUST be initialized for those who */
				/* insist on calling "tenmicrosec" before */
				/* the clock has been initialized. */

timestruc_t (*todgetf)(void) = pc_tod_get;
void (*todsetf)(timestruc_t) = pc_tod_set;

#if defined(i386)
void
clkstart(void)
{
	(*clkinitf)();
}
#elif defined(__ppc)
void
clkstart(void)
{
	extern void enable_interrupts(void);
	extern void init_dec_register(void);

	spl0();
	init_dec_register();
	enable_interrupts();
}
#endif

void
spinwait(int millis)
{
	int i, j;

	for (i = 0; i < millis; i++)
/*
 * 		for (j = 0; j < delaycount; j++)
 *			;
 */
		for (j = 0; j < 100; j++)
			tenmicrosec();
}

#ifndef KADB

long gmt_lag;		/* offset in seconds of gmt to local time */

/*
 * Write the specified time into the clock chip.
 * Must be called with tod_lock held.
 */
void
tod_set(timestruc_t ts)
{
	ASSERT(MUTEX_HELD(&tod_lock));

	(*todsetf)(ts);
}

/*
 * Read the current time from the clock chip and convert to UNIX form.
 * Assumes that the year in the clock chip is valid.
 * Must be called with tod_lock held.
 */
timestruc_t
tod_get(void)
{
	ASSERT(MUTEX_HELD(&tod_lock));

	return ((*todgetf)());
}

void
sgmtl(long arg)
{
	gmt_lag = arg;
}

long
ggmtl(void)
{
	return (gmt_lag);
}

/* rtcsync() - set 'time', assuming RTC and GMT lag are correct */

void
rtcsync(void)
{
	timestruc_t ts;
	int s;

	mutex_enter(&tod_lock);
	ts = (*todgetf)();
	s = CLOCK_LOCK();
	hrestime = ts;
	timedelta = 0;
	CLOCK_UNLOCK(s);
	mutex_exit(&tod_lock);
}

#endif	/* KADB */
