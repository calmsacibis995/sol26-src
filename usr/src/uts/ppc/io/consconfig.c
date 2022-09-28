/*
 * Copyright (c) 1991-1994, by Sun Microsystems, Inc.
 */

#pragma	ident  "@(#)consconfig.c 1.23     96/05/13 SMI" 	/* SVr4 */

/*
 * Console configuration - serial console and integral keyboard
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
#include <sys/file.h>
#include <sys/termios.h>
#include <sys/strsubr.h>
#include <sys/consdev.h>
#include <sys/kbio.h>
#include <sys/obpdefs.h>

#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/modctl.h>

/* #define	PATH_DEBUG	1 */
#ifdef	PATH_DEBUG
static int path_debug = PATH_DEBUG;
#endif	PATH_DEBUG

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
 * Configure keyboard.
 */

static char *emsg = "consconfig";

int consconfig_init_kd(dev_t *, struct vnode **);
int consconfig_init_conskbd(struct vnode **, struct vnode *);
int consconfig_init_wc(struct vnode *);
int consconfig_init_asy(struct vnode **, dev_t *);
int consconfig_init_console(void);

void
consconfig(void)
{
	extern int	console;

#ifdef	PATH_DEBUG
	char devname[OBP_MAXPATHLEN];
#endif
	dev_t wsconsdev;
	int major;
	dev_t asy_dev;
	struct vnode *asy_vp;

#ifdef	PATH_DEBUG
	if (path_debug != 0)  {
		prom_printf("consconfig:\n");
		prom_printf("stdin is <%s>, stdout is <%s>\n",
		    prom_stdinpath(cputype), prom_stdoutpath(cputype));
		prom_printf("stripped stdout is ...");
		prom_strip_options(prom_stdoutpath(cputype), devname);
		prom_printf("<%s>\n", devname);
		prom_printf("stdin-stdout-equivalence? <%s>\n",
		    prom_stdin_stdout_equivalence(cputype) ? "yes" : "no");
		(void) strcpy(devname, "unknown!");
		prom_stdin_devname(cputype, devname);
		prom_printf("stdin-devname <%s>\n", devname);
	}
#endif	PATH_DEBUG

	if (console == CONSOLE_IS_ASY) {
		if (consconfig_init_asy(&asy_vp, &asy_dev) != DDI_SUCCESS) {
			cmn_err(CE_PANIC,
				"%s: Can't open serial console!", emsg);
			/* NOTREACHED */
		}
	}

	/*
	 * We set up the console keyboard subsystem whether or not
	 * we're on a serial console, so that it can be used for the
	 * window system.
	 */
	if (consconfig_init_console() != DDI_SUCCESS) {
		if (console != CONSOLE_IS_ASY) {
			cmn_err(CE_PANIC,
			    "%s: can't set up workstation console", emsg);
			/* NOTREACHED */
		}
	}

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
	 * Now, having set everything up, set the global variables.
	 */
	if (console == CONSOLE_IS_ASY) {
		stdindev = asy_dev;
		rconsdev = asy_dev;
		rconsvp = asy_vp;
	} else {
		ddi_hold_installed_driver(major);
		stdindev = kbddev;
		rconsdev = wsconsdev;
		rconsvp = wsconsvp;
	}
}

/*
 * Console is a serial port.
 */

int
consconfig_init_asy(
	struct vnode **pvp,
	dev_t *dev)
{
	int major;
	int rval;
	int error;
	int zeropgrp = 0;

	major = ddi_name_to_major("asy");
	*dev = makedevice(major, 0);

#ifdef	PATH_DEBUG
	if (path_debug)
		prom_printf("asy console dev (%x)  is %d,%d\n", *dev,
			getemajor(*dev), geteminor(*dev));
#endif	PATH_DEBUG

	*pvp = makespecvp(*dev, VCHR);

	/*
	 * Opening causes interrupts, etc. to be initialized.
	 * Console device drivers must be able to do output
	 * after being closed!
	 */

	if (error = VOP_OPEN(pvp, FREAD+FWRITE+FNOCTTY, kcred)) {
		cmn_err(CE_WARN,
		    "%s: serial console open failed: error %d", emsg, error);
		return (DDI_FAILURE);
	}

	/*
	 * "Undo undesired ttyopen side effects" (not needed anymore
	 * in 5.0 -verified by commenting this out and running anyway.
	 * This zeroed u.u_ttyp and u.u_ttyd and u.u_procp->p_pgrp).
	 */

	(void) strioctl(*pvp, TIOCSPGRP, (int)&zeropgrp,
	    FREAD+FNOCTTY, K_TO_K, kcred, &rval);

	/* now we must close it to make console logins happy */
	ddi_hold_installed_driver(getmajor(*dev));
	VOP_CLOSE(*pvp, FREAD+FWRITE, 1, (offset_t)0, kcred);

	return (DDI_SUCCESS);
}

/*
 * Open the keyboard device.
 */

int
consconfig_init_kd(
	dev_t *dev,
	struct vnode **pvp)
{
	int major;
	int error;
	int rval;
	int zeropgrp = 0;
	int kbdtranslatable = TR_CANNOT;

	major = ddi_name_to_major("kd");
	*dev = makedevice(major, 0);
	*pvp = makespecvp(*dev, VCHR);
	if (error = VOP_OPEN(pvp, FREAD+FWRITE+FNOCTTY, kcred)) {
		cmn_err(CE_WARN, "%s: keyboard open failed: error %d",
		    emsg, error);
		goto fail_1;
	}
	(void) strioctl(*pvp, I_FLUSH, FLUSHRW, FREAD+FWRITE+FNOCTTY, K_TO_K,
	    kcred, &rval);
	/*
	 * It would be nice if we could configure the device to autopush this
	 * module, but unfortunately this code executes before the necessary
	 * user-level administrative code has run.
	 */
	if (error = strioctl(*pvp, I_PUSH, (int)"kb", FREAD+FWRITE+FNOCTTY,
				K_TO_K, kcred, &rval)) {
		cmn_err(CE_WARN,
		    "%s: can't push %s line discipline: error %d",
		    emsg, "keyboard", error);
		goto fail_2;
	}

	/*
	 * "Undo undesired ttyopen side effects" (not needed anymore
	 * in 5.0 -verified by commenting this out and running anyway.
	 * This zeroed u.u_ttyp and u.u_ttyd and u.u_procp->p_pgrp).
	 */

	(void) strioctl(*pvp, TIOCSPGRP, (int)&zeropgrp, FREAD+FNOCTTY,
	    K_TO_K, kcred, &rval);

	kbdtranslatable = TR_CAN;
	if (error = strioctl(*pvp, KIOCTRANSABLE, (int)&kbdtranslatable,
	    FREAD+FNOCTTY, K_TO_K, kcred, &rval))
		cmn_err(CE_WARN, "%s: KIOCTRANSABLE failed error: %d",
		    emsg, error);

	return (DDI_SUCCESS);

fail_2:
	VOP_CLOSE(*pvp, FREAD+FWRITE, 1, (offset_t)0, kcred);
fail_1:
	return (DDI_FAILURE);
}

/*
 * Open the "console keyboard" device, and link the keyboard device under it.
 * XXX - there MUST be a better way to do this!
 */

int
consconfig_init_conskbd(
	struct vnode **pvp,
	struct vnode *kbdvp)
{
	int major;
	int error;
	int fd;
	struct file *fp;
	int rval;

	major = ddi_name_to_major("conskbd");
	*pvp = makespecvp(makedevice(major, 1), VCHR);

	if (error = VOP_OPEN(pvp, FREAD+FWRITE+FNOCTTY, kcred)) {
		cmn_err(CE_WARN,
		    "%s: console keyboard device open failed: error %d", emsg,
		    error);
		goto fail_1;
	}

	if (error = falloc(kbdvp, FREAD, &fp, &fd)) {
		cmn_err(CE_WARN,
		    "%s: can't get file descriptor for keyboard: error %d",
		    emsg, error);
		goto fail_2;
	}

	setf(fd, fp);
	/* single theaded - no  close will occur here */
	mutex_exit(&fp->f_tlock);
	if (error = strioctl(*pvp, I_PLINK, fd, FREAD+FWRITE+FNOCTTY,
	    K_TO_K, kcred, &rval)) {
		cmn_err(CE_WARN, "%s: I_PLINK failed: error %d", emsg, error);
		goto fail_3;
	}
	setf(fd, NULLFP);
	/* single theaded - no other open will occur here */
	(void) closef(fp);	/* don't need this any more */

	return (DDI_SUCCESS);

fail_3:
	setf(fd, NULLFP);
	(void) closef(fp);
fail_2:
	VOP_CLOSE(*pvp, FREAD+FWRITE, 1, (offset_t)0, kcred);
fail_1:
	return (DDI_FAILURE);
}

/*
 * Open the "workstation console" device, and link the "console keyboard"
 * device under it.
 * XXX - there MUST be a better way to do this!
 *
 */
int
consconfig_init_wc(
	struct vnode *conskbdvp)
{
	int major;
	int error;
	int fd;
	int rval;
	struct file *fp;
	extern dev_t rwsconsdev;
	int ret;

	ret = DDI_FAILURE;

	major = ddi_name_to_major("wc");
	rwsconsdev = makedevice(major, 0);
	rwsconsvp = makespecvp(rwsconsdev, VCHR);

	if (error = VOP_OPEN(&rwsconsvp, FREAD+FWRITE, kcred)) {
		cmn_err(CE_WARN,
		    "%s: workstation console open failed: error %d",
		    emsg, error);
		goto done_1;
	}

	if (error = falloc(conskbdvp, FREAD+FWRITE+FNOCTTY, &fp, &fd)) {
		cmn_err(CE_WARN,
		    "%s: can't get fd for console keyboard: error %d",
		    emsg, error);
		goto done_2;
	}

	setf(fd, fp);
	mutex_exit(&fp->f_tlock);

	if (error = strioctl(rwsconsvp, I_PLINK, fd, FREAD+FWRITE+FNOCTTY,
	    K_TO_K, kcred, &rval)) {
		cmn_err(CE_WARN, "%s: I_PLINK failed: error %d", emsg, error);
		goto done_3;
	}

	ret = DDI_SUCCESS;

done_3:
	setf(fd, NULLFP);
	(void) closef(fp);	/* don't need this any more */

done_2:
	/* now we must close it to make console logins happy */
	VOP_CLOSE(rwsconsvp, FREAD+FWRITE, 1, (offset_t)0, kcred);

done_1:
	return (ret);
}

int
consconfig_init_console(void)
{
	struct vnode *kbdvp, *conskbdvp;

	if (consconfig_init_kd(&kbddev, &kbdvp) != DDI_SUCCESS) {
		return (DDI_FAILURE);
	}

	if (consconfig_init_conskbd(&conskbdvp, kbdvp) != DDI_SUCCESS) {
		return (DDI_FAILURE);
	}

	if (consconfig_init_wc(conskbdvp) != DDI_SUCCESS) {
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}
