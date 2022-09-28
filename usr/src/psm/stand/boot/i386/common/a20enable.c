/*
 * Copyright (c) 1992-1996, Sun Microsystems, Inc.  All Rights Reserved.
 */

/* BEGIN CSTYLED */
/*
 * Functions to enable address bit 20 on x86 PC machines.
 * Taken from probe.c
 *
 * == CAUTION ==== CAUTION ==== CAUTION ==== CAUTION ==
 *
 *	+ a20enable() is called very early before the
 *	+ high address portion of boot has been copied
 *	+ into place. This means that only low memory
 *	+ routines and data may be reference. Don't
 *	+ try putting any printfs here! The initial cut
 *	+ at this file only called doint_asm() and
 *	+ referenced no external data.
 *
 * == CAUTION ==== CAUTION ==== CAUTION ==== CAUTION ==
 */
/* END CSTYLED */

#pragma ident	"@(#)a20enable.c	1.2	96/05/24 SMI"

#include <sys/types.h>
#include <sys/machine.h>
#include <sys/sysmacros.h>
#include <sys/bootconf.h>
#include <sys/booti386.h>
#include <sys/bootdef.h>
#include <sys/sysenvmt.h>
#include <sys/bootinfo.h>
#include <sys/bootlink.h>
#include <sys/promif.h>
#include <sys/ramfile.h>
#include <sys/dosemul.h>
#include <sys/salib.h>

static void a20enable_doit(void);
static int empty8042(void);
static int flush8042(void);
static void mc_gate20(void);
static void localwait(void);
void a20enable(void);

#define	KB_MC_A20	0xd3    /* kb command with A20 bit set */
#define	KB_CMDF		0x08
/* controller flag indicating next out to KB_OUT should be a command */

void
a20enable(void)
{
	u_int *p1meg = (u_int *)0x100000;
	u_int *pzero = (u_int *)0;
	u_int zero_save = *pzero;

	/*
	 * Programming the keyboard controller to enable a20 can
	 * fail if the user is actively typing. Keep trying until
	 * we get it right.
	 */
	do {
		*pzero = 0xfeedface;
		a20enable_doit();
		*p1meg = 0xdeadbeef;
	} while (*pzero != 0xfeedface);
	*pzero = zero_save;
}

static void
a20enable_doit(void)
{

/*
 *	First check to see if the controller has something to send us.
 *      If so take it to flush the controller's output buffer.
 *      Also, we cannot send a command if the input buffer is full,
 *      so wait here for the controller to flush it.
 */

	(void) flush8042();

/*
 *	If the controller is expecting a command, status bit 3 set,
 *	give it one, the gate20 one, to get it out of that mode.
 */
	if (inb(KB_STAT) & KB_CMDF)
		mc_gate20();

/*
 * The controller may want to complain about the command, so make
 *      sure to check the output buffer flag again and flush if neccessary.
 */
	if (flush8042())
		(void) inb(KB_STAT);

/*	Now at last we can tell it what we want to do. */
/*	So send the command to tell it to accept a command. */

	outb(KB_ICMD, KB_WOP);
	localwait();

	/*
	 * Wait for the controllers input buffer to empty and the command
	 * expected bit 3 in the status word to be true.
	 */
	if (empty8042())
		(void) inb(KB_STAT);

/* Send the gate20 command. */

	if ((inb(KB_STAT) & KB_CMDF) == 0) {
		/* make sure we can command */
		localwait();
		outb(KB_ICMD, KB_WOP);
		localwait();
	}
	mc_gate20();

/* Finally, flush the output buffer again if necessary. */

	if (flush8042())
		(void) inb(KB_STAT);

	/* Whew ! */

	(void) inb(KB_STAT);
}

static void
mc_gate20(void)
{
	outb(KB_IDAT, KB_MC_A20);
	localwait();
	(void) empty8042();
}

/*
 *	Flush the keyboard output buffer, return non-zero on error.
 */

static int
flush8042(void)
{
	int	i, v;

	for (i = 0; i < 200; i += 1) {
		if (((v = inb(KB_STAT)) & (KB_OUTBF | KB_INBF)) == 0)
			return (0);
		else {
			if (v & KB_OUTBF) {	/* if output ready */
				v = inb(KB_OUT); /* clear output buffer */
				localwait();
			}
		}
	}
	return (1);
}

/*
 *	Wait for the keyboard input buffer to be empty,
 *	return non-zero on error.
 */

static int
empty8042(void)
{
	register i;

	for (i = 0; i < 200; i++) {
		if ((inb(KB_STAT) & KB_INBF) == 0)
			return (0);
	}
	return (1);
}

static void
localwait(void)
{
	struct real_regs local_regs, *rr;
	extern int doint_asm();

	rr = &local_regs;

	AX(rr) = 0x8600;
	CX(rr) = 1;
	DX(rr) = 0x86a0;

	(void) doint_asm(0x15, rr);
}
