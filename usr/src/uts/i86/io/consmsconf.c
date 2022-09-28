/*
 * Copyright (c) 1991-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident "@(#)consmsconf.c	1.23	96/09/24 SMI"

/*
 * Console and mouse configuration
 */

/*
 * XXXX: Sort out the includes later
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/user.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/systm.h>
#include <sys/map.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/vm.h>
#include <sys/file.h>
#include <sys/lwp.h>
#include <sys/termios.h>
#include <sys/termio.h>
#include <sys/ttold.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/mman.h>
#undef NFS
#include <sys/mount.h>
#include <sys/bootconf.h>
#include <sys/fs/snode.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>

#include <sys/kmem.h>
#include <sys/cpu.h>
#include <sys/psw.h>
#include <sys/consdev.h>
#include <sys/kbio.h>
#include <sys/debug.h>
#include <sys/reboot.h>

#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/promif.h>
#include <sys/modctl.h>

/* names of drivers explicitly loaded once we determine the console type */
char	asy_name[]	= "asy";
char	chanmux_name[]	= "chanmux";
char	kd_name[]	= "kd";

extern	struct vnode	*wsconsvp;

/*
 * This is the loadable module wrapper.
 */

extern struct mod_ops mod_miscops;

/*
 * Module linkage information for the kernel.
 */
static struct modlmisc modlmisc = {
	&mod_miscops, "console configuration"
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlmisc, NULL
};

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * Configure keyboard and mouse.
 */

void
consconfig(void)
{
	extern int	console;

	static char *emsg = "consconfig: ";
	static char *nokset = "%scan't set up keyboard";
	static char *cantload = "%scannot load '%s' driver for console";
	int	error;
	int	zeropgrp = 0;
	struct vnode *cmuxvp;
	struct vnode *kdvp;
	struct file *fp;
	int	rval, fd;
	dev_t	wsconsdev;
	int	major;
	int	vt;
	extern dev_t rwsconsdev;
	extern int cn_conf;


	rconsdev = 0;
	if (console == CONSOLE_IS_ASY) {	/* serial console */
		/* try to load the "asy" driver */
		if (ddi_install_driver(asy_name) == DDI_FAILURE) {
			cmn_err(CE_PANIC, cantload, emsg, asy_name);
		}
		major = ddi_name_to_major(asy_name);
		rconsdev = makedevice(major, 0);
	}

	cn_conf = 1;		/* Don't really use rconsvp yet... */

	if (rconsdev) {
		/*
		 * Console is a serial port.
		 */
		rconsvp = makespecvp(rconsdev, VCHR);

		/*
		 * Opening causes interrupts, etc. to be initialized.
		 * Console device drivers must be able to do output
		 * after being closed!
		 */

		if (error = VOP_OPEN(&rconsvp, FREAD+FWRITE+FNOCTTY, kcred)) {
			cmn_err(CE_WARN,
			    "%sserial console open failed: error %d",
			    emsg, error);
		}

		/*
		 * "Undo undesired ttyopen side effects" (not needed anymore
		 * in 5.0 -verified by commenting this out and running anyway.
		 * This zeroed u.u_ttyp and u.u_ttyd and u.u_procp->p_pgrp).
		 */

		(void) strioctl(rconsvp, TIOCSPGRP, (intptr_t)&zeropgrp,
				FREAD + FNOCTTY, K_TO_K, kcred, &rval);

		/* now we must close it to make console logins happy */
		ddi_hold_installed_driver(getmajor(rconsdev));
		VOP_CLOSE(rconsvp, FREAD+FWRITE, 1, (offset_t)0, kcred);
	}

	/*
	 *XXX should we always try to load kd and chanmux?  Maybe we should
	 *XXX always try, but if loading "kd" fails (no video hardware?), then
	 *XXX only panic if we're not using a serial console.  Note that this
	 *XXX would require updating "kd" to probe for video hardware and fail
	 *XXX the probe -- it presently *always* succeeds.
	 */

	/* try to load the "kd" driver */
	if (ddi_install_driver(kd_name) == DDI_FAILURE) {
		cmn_err(CE_PANIC, cantload, emsg, kd_name);
	}

	/* now load "chanmux" too */
	if (ddi_install_driver(chanmux_name) == DDI_FAILURE) {
		cmn_err(CE_PANIC, cantload, emsg, chanmux_name);
	}

	/*
	 * Open each virtual terminal (chanmux module) and link the kd
	 * driver under it.  Unfortunately, this code has too much knowledge
	 * hardwired in it.  We try each possible VT - numbered 0 (aka console)
	 * all the way to 12 (aka vt12), and stop if the chanmux open fails,
	 * as the user may have configured chanmux for fewer than 13 units.
	 */
	/* fake the char module and put it on the stream */
	for (vt = 0; vt <= 12; vt++) {
		dev_t	consdev;

		major = ddi_name_to_major(chanmux_name);
		consdev = makedevice(major, vt);
		cmuxvp = makespecvp(consdev, VCHR);
		if (vt == 0) {		/* console VT */
			rwsconsvp = cmuxvp;
			rwsconsdev = consdev;
			fbvp = cmuxvp;
		}
		if (error = VOP_OPEN(&cmuxvp, FREAD+FWRITE+FNOCTTY, kcred)) {
			VN_RELE(cmuxvp);	/* release the vnode */

			if (rconsvp == NULL && vt == 0) {
				/* this should have been the console */
				cmn_err(CE_WARN, "%schannel multiplexor %d "
				    "open failed: error %d",
				    emsg, vt, error);
				cmn_err(CE_PANIC,
				    "%scan't open keyboard", emsg);
				/* NOTREACHED */
			}

			if (error == ENXIO)
				break;	/* not this many VTs configured */

			/*
			 * Otherwise print a warning and skip if the chanmux
			 * open fails.
			 */
			cmn_err(CE_WARN,
			    "%schannel multiplexor %d open failed: error %d",
			    emsg, vt, error);
			continue;
		}

		major = ddi_name_to_major(kd_name);
		kdvp = makespecvp(makedevice(major, vt), VCHR);
		if (error = VOP_OPEN(&kdvp, FREAD+FWRITE+FNOCTTY, kcred)) {
			cmn_err(CE_WARN,
			    "%s%s driver open failed: error %d",
			    emsg, kd_name, error);
			if (rconsvp == NULL && vt == 0) {
				/* this should have been the console */
				cmn_err(CE_PANIC,
				    "%scan't open keyboard", emsg);
				/* NOTREACHED */
			}
		}
		if (error = falloc(kdvp, FREAD+FWRITE, &fp, &fd)) {
			cmn_err(CE_WARN, "%scan't get file descriptor "
			    "for keyboard: error %d",
			    emsg, error);
			if (rconsvp == NULL && vt == 0) {
				/* this should have been the console */
				cmn_err(CE_PANIC, nokset, emsg);
				/* NOTREACHED */
			}
		}
		setf(fd, fp);
		/* single theaded - no close will occur here */
		mutex_exit(&fp->f_tlock);
		if (error = strioctl(cmuxvp, I_PLINK, (intptr_t)fd,
				FREAD + FWRITE, K_TO_K, kcred, &rval)) {
			cmn_err(CE_WARN,
			    "%sI_PLINK failed: error %d", emsg, error);
			if (rconsvp == NULL && vt == 0) {
				/* this should have been the console */
				cmn_err(CE_PANIC, nokset, emsg);
				/* NOTREACHED */
			}
		}
		setf(fd, NULLFP);
		/* single theaded - no other open will occur here */
		(void) closef(fp);	/* don't need this any more */

		ddi_hold_installed_driver(major);
		VOP_CLOSE(cmuxvp, FREAD+FWRITE+FNOCTTY, 1, (offset_t)0, kcred);
	}

	/*
	 * Log a message indicating how many VTs have been assembled to the
	 * log device (but not to the console)
	 */
	cmn_err(CE_CONT, "!Number of console virtual screens = %d\n", vt);

	/*
	 * Get a vnode for the redirection device.  (It has the
	 * connection to the workstation console device wired into it,
	 * so that it's not necessary to establish the connection
	 * here.  If the redirection device is ever generalized to
	 * handle multiple client devices, it won't be able to
	 * establish the connection itself, and we'll have to do it
	 * here.)
	 */
	major = ddi_name_to_major("iwscn");
	wsconsdev = makedevice(major, 0);
	wsconsvp = makespecvp(wsconsdev, VCHR);

	/*
	 * Use the redirection device/workstation console pair as the "real"
	 * console if the latter hasn't already been set.
	 */
	if (rconsvp == NULL) {
		/*
		 * The workstation console driver needs to see rwsconsvp, but
		 * all other access should be through the redirecting driver.
		 */
		ddi_hold_installed_driver(major);
		rconsdev = wsconsdev;
		rconsvp = wsconsvp;
	}
	cn_conf = 0;		/* OK to use rconsvp, now. */
}
