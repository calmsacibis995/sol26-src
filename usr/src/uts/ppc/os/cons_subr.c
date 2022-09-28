/*
 * Copyright (c) 1989-1995, by Sun Microsystems, Inc.
 */

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/promif.h>
#include <sys/cred.h>
#include <sys/uio.h>
#include <sys/file.h>
#include <sys/visual_io.h>
#include <sys/ltem.h>
#include <sys/console.h>
#include <sys/reboot.h>
#include <sys/consdev.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

#pragma ident "@(#)cons_subr.c	1.12	96/02/12 SMI"

extern void (*putcharptr)(char);
extern void (*putstrptr)(char *, u_int);
extern int (*putsyscharptr)(char);
int (*prev_putsyscharptr)(char);

extern vnode_t *ltemvp;

void
console_get_devname(char *devname)
{
	prom_stdout_devname(devname);
}

void
console_connect(void)
{
	putcharptr = console_write_char;
	putstrptr = console_write_str;
	prev_putsyscharptr = putsyscharptr;
	putsyscharptr = console_kadb_write_char;
}

void
console_disconnect(void)
{
		putcharptr = prom_putchar;
		putstrptr = prom_writestr;
		putsyscharptr = prev_putsyscharptr;
}

/*
 * Ring the bell for the specified duration.
 */
/* ARGSUSED */
void
console_default_bell(clock_t duration)
{
	/*
	 * Bell support is supplied by the keyboard driver "kd".
	 * Until it's loaded, there's nothing we can do.
	 * Having the bell support be in the keyboard driver
	 * isn't really right, since they aren't really related
	 * subsystems in the hardware.
	 */
}

/*
 * console_kadb_write_char is specific to machines without proms and machines
 * that unmap their prom after startup.  It allows kadb to use the kernel
 * to display characters on the console device.
 */

int
console_kadb_write_char(char c)
{
	int rvalp;
	char buf[1];
	struct uio uio;
	struct iovec iov;

	buf[0] = c;

	/*
	 * Create a uio struct
	 */
	iov.iov_base = buf;
	iov.iov_len = 1;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_loffset = 0;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_resid = 1;
	uio.uio_limit = 2;
	uio.uio_fmode = FWRITE;

	/*
	 * We ignore the ioctl result code.  After all, what could we do?
	 * Write something to the console??
	 */
	(void) VOP_IOCTL(ltemvp, LTEM_STAND_WRITE, (int)&uio,
	    FKIOCTL, kcred, &rvalp);
	return (0);
}

/*
 * Putsyscharptr is initialized to point here.
 * That pointer is used only after OF has been discarded, and
 * normally by that time the console has been initialized and
 * putsyscharptr has been pointed at a real output routine.
 * If, however, the console can't be initialized, it'll still
 * point here.  The console will be unusable, but maybe the
 * system will come up enough to allow network or serial login.
 */
/*ARGSUSED*/
int
console_char_no_output(char c)
{
	return (0);
}

int
use_bifont()
{
	char    buf[8];
	char    *bufp = buf;

	*bufp = ' ';

	printf("Use Builtin Font: [n] ");
	gets(buf);
	if (*bufp == 'y' || *bufp == 'Y')
		return (1);
	else
		return (0);
}

int
stdout_is_framebuffer(void)
{
	return (prom_stdout_is_framebuffer());
}

char *
stdout_path()
{
	return (prom_stdoutpath());
}
