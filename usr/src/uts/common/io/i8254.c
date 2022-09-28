/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/


#pragma ident	"@(#)i8254.c	1.15	96/10/17 SMI"

/*
 *         INTEL CORPORATION PROPRIETARY INFORMATION
 *
 *     This software is supplied under the terms of a license
 *    agreement or nondisclosure agreement with Intel Corpo-
 *    ration and may not be copied or disclosed except in
 *    accordance with the terms of that agreement.
 */

#include <sys/types.h>
#include <sys/dl.h>
#include <sys/param.h>
#include <sys/pit.h>
#include <sys/inline.h>
#include <sys/machlock.h>
#include <sys/avintr.h>
#include <sys/smp_impldefs.h>
#include <sys/archsystm.h>
#include <sys/systm.h>

#ifdef DEBUGGING_TIME
#define	TIME_CHECK		/* Debugging */
#define	TIME_CHECKD		/* detailed Debugging */
#endif

int clock_vector;

int pitctl_port  = PITCTL_PORT;		/* For 386/20 Board */
int pitctr0_port = PITCTR0_PORT;	/* For 386/20 Board */
int pitctr1_port = PITCTR1_PORT;	/* For 386/20 Board */
int pitctr2_port = PITCTR2_PORT;	/* For 386/20 Board */
/* We want PIT 0 in square wave mode */

int sanity_ctl	 = SANITY_CTL;		/* EISA sanity ctl word */
int sanity_ctr0	 = SANITY_CTR0;		/* EISA sanity timer */
int sanity_port	 = SANITY_CHECK;	/* EISA sanity enable/disable port */
int sanity_mode = PIT_C0|PIT_ENDSIGMODE|PIT_READMODE;
int sanity_enable = ENABLE_SANITY;
int sanity_reset = RESET_SANITY;


unsigned int delaycount;		/* loop count in trying to delay for */
					/* 1 millisecond */
extern unsigned long microdata;

unsigned int sanitynum = SANITY_NUM;	/* interrupt interval for sanitytimer */

static void findspeed(void);

void
clksetup(void)
{
#if	TRACEV
	extern (*evtimef)();
	extern pit_microtime();
#endif	/* TRACEV */
#if !defined(__ppc)
	extern void clock(void);
#endif

	findspeed();

	/*
	 * The variable "tick" is used in hrt_alarm for
	 * HRT_RALARM case.
	 */
#if	TRACEV
	/* change evtimef to use high precision timer */
	evtimef = pit_microtime;
#endif	/* TRACEV */

#if !defined(__ppc)
	/* For PowerPC, we use the on-chip decrementer for this */

	/* Register the clock interrupt handler */
	(void) add_avintr((void *)NULL, CLOCK_LEVEL, (avfunc)clock, "clock",
	    clock_vector, 0, NULL);
#endif
}

#define	COUNT	0x2000

static void
findspeed(void)
{
	unsigned char byte;
	unsigned int leftover;
	int s;
#ifdef TIME_CHECK
	register int	tval;
#endif	/* TIME_CHECK */

	s = clear_int_flag();	/* disable interrupts */
	/* Put counter in count down mode */
#define	PIT_COUNTDOWN PIT_READMODE|PIT_NDIVMODE
#ifdef TIME_CHECKD
	for (tval = 0x400; tval <= 0x8000; tval = tval * 2) {
		outb(pitctl_port, PIT_COUNTDOWN);
		outb(pitctr0_port, 0xff);
		outb(pitctr0_port, 0xff);
		delaycount = tval; spinwait(1);
		byte = inb(pitctr0_port);
		leftover = inb(pitctr0_port);
		leftover = (leftover << 8) + byte;

		delaycount = (tval * (PIT_HZ / 1000)) /
		    (0xffff-leftover);
		printf("findspeed:tval=%x, dcnt=%d, left=%x, top=%x, btm=%x\n",
			tval, delaycount, leftover,
			tval * (PIT_HZ / 1000), (0xffff-leftover));
	}
#endif	/* TIME_CHECKD */
	outb(pitctl_port, PIT_COUNTDOWN);
	/* output a count of -1 to counter 0 */
	outb(pitctr0_port, 0xff);
	outb(pitctr0_port, 0xff);
	delaycount = COUNT;
	spinwait(1);
	/* Read the value left in the counter */
	byte = inb(pitctr0_port);	/* least siginifcant */
	leftover = inb(pitctr0_port);	/* most significant */
	leftover = (leftover << 8) + byte;
	/*
	 * Formula for delaycount is :
	 *  (loopcount * timer clock speed)/ (counter ticks * 1000)
	 * 1000 is for figuring out milliseconds
	 */
	delaycount = (COUNT * (PIT_HZ / 1000)) / (0xffff-leftover);
#ifdef TIME_CHECK
	printf("delaycount for one millisecond delay is %d\n", delaycount);
#endif	/* TIME_CHECK */
	restore_int_flag(s);		/* restore interrupt state */
}
#define	MICROCOUNT	0x2000

void
microfind(void)
{
	unsigned char byte;
	unsigned short leftover;
	int s;
#ifdef TIME_CHECK
	register int	tval;
#endif	/* TIME_CHECK */


	s = clear_int_flag();		/* disable interrupts */
#ifdef TIME_CHECKD
	for (tval = 0x80; tval <= 0x80000; tval = tval * 2) {
		outb(pitctl_port, PIT_COUNTDOWN);
		outb(pitctr0_port, 0xff);
		outb(pitctr0_port, 0xff);
		microdata = tval;
		tenmicrosec();
		byte = inb(pitctr0_port);
		leftover = inb(pitctr0_port);
		leftover = (leftover<<8) + byte;
		microdata = (unsigned)(tval * PIT_HZ) /
				((unsigned)(0xffff-leftover)*100000);
		microdata = (unsigned)(tval * (PIT_HZ / hz)) /
				((unsigned)(0xffff-leftover)*(100000/hz));
		printf("tval=%x, mdat=%x, left=%x\n", tval, microdata,
		    leftover);
	}
#endif	/* TIME_CHECKD */

	/* Put counter in count down mode */
	outb(pitctl_port, PIT_COUNTDOWN);
	/* output a count of -1 to counter 0 */
	outb(pitctr0_port, 0xff);
	outb(pitctr0_port, 0xff);
	microdata = MICROCOUNT;
	tenmicrosec();
	/* Read the value left in the counter */
	byte = inb(pitctr0_port);	/* least siginifcant */
	leftover = inb(pitctr0_port);	/* most significant */
	leftover = (leftover << 8) + byte;
	/*
	 * Formula for delaycount is :
	 *  (loopcount * timer clock speed)/ (counter ticks * 1000)
	 *  Note also that 1000 is for figuring out milliseconds
	 */
	microdata = (unsigned)(MICROCOUNT * (PIT_HZ / hz)) /
			((unsigned)(0xffff-leftover)*(100000/hz));
	if (!microdata)
		microdata++;
#ifdef TIME_CHECK
	printf("delaycount for ten microsecond delay is %d\n", microdata);
#endif /* TIME_CHECK */
	restore_int_flag(s);		/* restore interrupt state */
}

/*
 * pit_microtime reads the pit chip for the count down value for next clock
 * interrupt.  From the count down value it determines the time in tenmicrosec.
 */
pit_microtime()
{
	unsigned int byte, leftover;
	unsigned int tt;
	unsigned int pitticks = PIT_HZ / hz;
	static unsigned int lastmicrotime;

	do {
		outb(pitctl_port, 0);
		byte = inb(pitctr0_port);
		leftover = inb(pitctr0_port);
		leftover = pitticks - ((leftover<<8) + byte);
	} while (leftover > pitticks);

	tt = lbolt * (100000 / hz) + (leftover * 100000) / PIT_HZ;
	if (lastmicrotime && tt < lastmicrotime)
		tt += 100000/hz;
	lastmicrotime = tt;
	return (tt);
}
