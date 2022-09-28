/*
 * Copyright (c) 1991, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident  "@(#)consconfig.c 1.16     96/02/07 SMI"		/* SVr4 */

/*
 * Console and mouse configuration
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/user.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/klwp.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/strsubr.h>

#include <sys/consdev.h>
#include <sys/kbio.h>
#include <sys/debug.h>
#include <sys/reboot.h>
#include <sys/termios.h>
#include <sys/clock.h>

#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/modctl.h>

#define	PRTPATH(name, dev) {\
	cmn_err(CE_CONT, "?%s is <%s> major <%d> minor <%d>\n", name, \
		path ? path : "unknown", \
		(int)(dev == NODEV ? -1 : getmajor(dev)), \
		(int)(dev == NODEV ? -1 : getminor(dev))); }

extern	dev_t	kbddev;
extern	dev_t	mousedev;
extern	dev_t	rwsconsdev;
extern	int	cn_conf;

extern	struct vnode	*wsconsvp;

extern	char	*i_kbdpath(void);
extern	char	*i_mousepath(void);
extern	char	*i_stdinpath(void);
extern	char	*i_stdoutpath(void);
extern	int	i_stdin_is_keyboard(void);
extern	int	i_stdout_is_framebuffer(void);
extern	int	i_setmodes(dev_t, struct termios *);

/*
 * This is the loadable module wrapper.
 */

extern struct mod_ops mod_miscops;

/*
 * Module linkage information for the kernel.
 */
static struct modlmisc modlmisc = {
	&mod_miscops, "console configuration 1.16"
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlmisc, NULL
};

_init()
{
	return (mod_install(&modlinkage));
}

_fini()
{
	return (mod_remove(&modlinkage));
}

_info(modinfop)
	struct modinfo *modinfop;
{
	return (mod_info(&modlinkage, modinfop));
}


static const char *discmsg = "%s: can't push %s line discipline: error %d";
static const char *emsg = "consconfig";

static	struct vnode *conskbdvp; /* Shared between various routines */

#ifndef	MPSAS

static int
mouseconfig(dev_t msdev)
{
	static char *emsg = "mouseconfig";
	int error;
	struct vnode *mousevp;
	struct vnode *consmousevp;
	struct file *fp;
	int fd, rval;
	int major;

	/* Open the mouse device. */
	mousevp = makespecvp(msdev, VCHR);
	if (error = VOP_OPEN(&mousevp, FREAD+FNOCTTY, kcred))
		return (error);
	(void) strioctl(mousevp, I_FLUSH, FLUSHRW, FREAD+FNOCTTY,
	    K_TO_K, kcred, &rval);
	if (error = strioctl(mousevp, I_PUSH, (int)"ms", FREAD+FNOCTTY,
	    K_TO_K, kcred, &rval)) {
		cmn_err(CE_WARN, (char *)discmsg, emsg, "mouse", error);
		(void) VOP_CLOSE(mousevp, FREAD+FNOCTTY, 1, (offset_t)0,
		    kcred);
		return (error);
	}

	/*
	 * Open the "console mouse" device, and link the mouse device
	 * under it.
	 */
	major = ddi_name_to_major("consms");
	consmousevp = makespecvp(makedevice(major, 0), VCHR);
	if (error = VOP_OPEN(&consmousevp, FREAD+FWRITE+FNOCTTY, kcred)) {
		(void) VOP_CLOSE(mousevp, FREAD+FNOCTTY, 1, (offset_t)0, kcred);
		return (error);
	}
	if (error = falloc(mousevp, FREAD+FNOCTTY, &fp, &fd)) {
		cmn_err(CE_WARN,
		    "%s: can't get file descriptor for mouse: error %d",
		    emsg, error);
		(void) VOP_CLOSE(consmousevp, FREAD+FWRITE+FNOCTTY, 1,
		    (offset_t)0, kcred);
		(void) VOP_CLOSE(mousevp, FREAD+FNOCTTY, 1, (offset_t)0,
		    kcred);
		return (error);
	}
	setf(fd, fp);
	/* single theaded - no  close will occure here */
	mutex_exit(&fp->f_tlock);
	if (error = strioctl(consmousevp, I_PLINK, fd,
	    FREAD+FWRITE+FNOCTTY, K_TO_K, kcred, &rval)) {
		cmn_err(CE_WARN, "%s: mouse I_PLINK failed: error %d",
		    emsg, error);
		(void) VOP_CLOSE(consmousevp, FREAD+FWRITE+FNOCTTY, 1,
		    (offset_t)0, kcred);
		(void) VOP_CLOSE(mousevp, FREAD+FNOCTTY, 1, (offset_t)0,
		    kcred);
		return (error);
	}
	setf(fd, NULL);
	/* single theaded - no other open will occure here */
	(void) closef(fp);	/* don't need this any more */
	(void) ddi_hold_installed_driver(major);
	(void) VOP_CLOSE(consmousevp, FREAD+FWRITE+FNOCTTY, 1,
	    (offset_t)0, kcred);

	return (error);
}

/*
 * Push "kb" on top of either keyboard or stdin as necessary,
 * and deal with keyboard translatable property.
 */

static int
kbconfig(struct vnode *vp, int kbdtranslatable)
{
	static char *emsg = "kbconfig";
	int error, rval;

	if (error = VOP_OPEN(&vp, FREAD+FNOCTTY, kcred)) {
		cmn_err(CE_WARN, "%s: keyboard open failed: error %d",
		    emsg, error);
		return (1);
	}
	(void) strioctl(vp, I_FLUSH, FLUSHRW, FREAD+FNOCTTY, K_TO_K,
	    kcred, &rval);
	/*
	 * It would be nice if we could configure the device to autopush this
	 * module, but unfortunately this code executes before the necessary
	 * user-level administrative code has run.
	 */
	if (error = strioctl(vp, I_PUSH, (int)"kb", FREAD+FNOCTTY, K_TO_K,
	    kcred, &rval)) {
		cmn_err(CE_WARN, (char *)discmsg, emsg, "keyboard", error);
		return (1);
	}

	if (error = strioctl(vp, KIOCTRANSABLE, (int)&kbdtranslatable,
	    FREAD+FNOCTTY, K_TO_K, kcred, &rval))
		cmn_err(CE_WARN, "%s: KIOCTRANSABLE failed error: %d",
		    emsg, error);
	return (0);
}


static int
conskbdconfig(struct vnode *kbdvp)
{
	static char *emsg = "conskbdconfig";

	int error;
	struct file *fp;
	int rval, fd;
	int major;

	/*
	 * Open the "console keyboard" device, and link the keyboard
	 * device under it.
	 */

	major = ddi_name_to_major("conskbd");
	conskbdvp = makespecvp(makedevice(major, 1), VCHR);
	if (error = VOP_OPEN(&conskbdvp, FREAD+FWRITE+FNOCTTY, kcred)) {
		cmn_err(CE_WARN,
		    "%s: console keyboard device open failed: error %d", emsg,
		    error);
		return (1);
	}
	if (error = falloc(kbdvp, FREAD, &fp, &fd)) {
		cmn_err(CE_WARN,
		    "%s: can't get file descriptor for keyboard: error %d",
		    emsg, error);
		return (1);
	}
	setf(fd, fp);
	/* single theaded - no  close will occure here */
	mutex_exit(&fp->f_tlock);
	if (error = strioctl(conskbdvp, I_PLINK, fd, FREAD+FWRITE+FNOCTTY,
	    K_TO_K, kcred, &rval)) {
		cmn_err(CE_WARN, "%s: conskbd I_PLINK failed: error %d",
			emsg, error);
		return (1);
	}
	setf(fd, NULLFP);
	/* single theaded - no other open will occure here */
	(void) closef(fp);	/* don't need this any more */
	return (0);
}

/*
 * Configure keyboard and mouse. Main entry here.
 */

void
consconfig(void)
{
	int error;
	dev_t stdoutdev;
	dev_t wsconsdev;
	struct file *fp;
	int rval, fd;
	klwp_t *lwp = ttolwp(curthread);
	int major;
	struct vnode *stdinvp = NULL;
	struct vnode *kbdvp = NULL;
	char *path;

	/*
	 * Find keyboard, mouse, stdin and stdout devices,
	 * if they exist on this platform, and convert the
	 * pathnames to dev_t's.
	 */

	kbddev = NODEV;
	if ((path = i_kbdpath()) != NULL)
		kbddev = ddi_pathname_to_dev_t(path);
	PRTPATH("keyboard", kbddev);

	mousedev = NODEV;
	if ((path = i_mousepath()) != NULL)
		mousedev = ddi_pathname_to_dev_t(path);
	PRTPATH("mouse", mousedev);

	stdindev = NODEV;
	if ((path = i_stdinpath()) != NULL)
		stdindev = ddi_pathname_to_dev_t(path);
	PRTPATH("stdin", stdindev);

	stdoutdev = NODEV;
	if ((path = i_stdoutpath()) != NULL)
		stdoutdev = ddi_pathname_to_dev_t(path);
	PRTPATH("stdout", stdoutdev);

	cn_conf = 1;		/* Don't really use rconsvp yet... */

	/*
	 * NON-DDI COMPLIANT CALL
	 */
	stop_mon_clock();	/* turn off monitor polling clock */


	/*
	 * A reliable indicator that we are doing a remote console is that
	 * stdin and stdout are the same.
	 */

	if ((stdindev != NODEV) && (stdindev == stdoutdev))
	    rconsdev = stdindev;

	if (rconsdev) {
		/*
		 * Console is a serial port. Can be any kind, as long as the
		 * prom was willing to tolerate it.
		 */
		rconsvp = makespecvp(rconsdev, VCHR);

		/*
		 * Opening causes interrupts, etc. to be initialized.
		 * Console device drivers must be able to do output
		 * after being closed!
		 */

		if (error = VOP_OPEN(&rconsvp, FREAD+FWRITE+FNOCTTY, kcred)) {
			cmn_err(CE_WARN,
			    "%s: console open failed: error %d", emsg, error);
		}

		/* now we must close it to make console logins happy */
		(void) ddi_hold_installed_driver(getmajor(rconsdev));
		VOP_CLOSE(rconsvp, FREAD+FWRITE, 1, (offset_t)0, kcred);

	}


	if (i_stdout_is_framebuffer()) {

		/*
		 * Console output is a framebuffer.
		 * Find the framebuffer driver if we can, and make
		 * ourselves a shadow vnode to track it with.
		 */
		fbdev = stdoutdev;
		if (fbdev == NODEV) {
			/*
			 * Look at 1097995 if you want to know why this
			 * might be a problem ..
			 */
			cmn_err(CE_NOTE,
			    "Can't find driver for console framebuffer");
		} else
			fbvp = makespecvp(fbdev, VCHR);
	}

#ifdef	PATH_DEBUG
	printf("mousedev %X\nkbddev %X\nfbdev %X\nrconsdev %X\n",
		    mousedev,  kbddev, fbdev, rconsdev);
#endif	PATH_DEBUG

	if (kbddev == NODEV)
	    return;		/* This is wrong. See bug 1209014 */

	/*
	 * Try to configure mouse.
	 */

	if (mousedev != NODEV && mouseconfig(mousedev)) {
		cmn_err(CE_NOTE, "%s: No mouse found", emsg);
		mousedev = NODEV;
		lwp->lwp_error = 0;
	}

	/*
	 * Try to configure keyboard.
	 */

	if (kbddev != NODEV)
	    kbdvp = makespecvp(kbddev, VCHR);
	if ((kbddev == NODEV) ||
	    kbconfig(kbdvp, TR_CAN) ||
	    conskbdconfig(kbdvp)) {
		cmn_err(CE_NOTE, "%s: Cannot configure keybord", emsg);
		kbddev = NODEV;
		if (!rconsdev)
		    cmn_err(CE_PANIC,
			    "%s: No keyboard and no rconsdev", emsg);
	}

	/*
	 * Setup stdin from appropriate source
	 */

	if (i_stdin_is_keyboard())  {
#ifdef PATH_DEBUG
		printf("stdin is keyboard\n");
#endif
		stdindev = kbddev;		/* Using physical keyboard */
		stdinvp = conskbdvp;
	} else if (rconsdev) {
#ifdef PATH_DEBUG
		printf("stdin is rconsdev\n");
#endif
		stdindev = rconsdev;		/* Console is tty[a-z] */
		stdinvp = conskbdvp;		/* input/output are same dev */
	} else {
		struct termios termios;
#ifdef PATH_DEBUG
		printf("stdin is serial keyboard\n");
#endif
		/*
		 * Non-keyboard input device, but not rconsdev.
		 * This is known as the "serial keyboard" case - the
		 * most likely use is someone has a keyboard attached
		 * to a serial port and still has output on a framebuffer.
		 */
		stdinvp = makespecvp(stdindev, VCHR);
		(void) kbconfig(stdinvp, TR_CANNOT);

		/* Re-set baud rate */
		(void) strioctl(stdinvp, TCGETS, (int)&termios, FREAD+FNOCTTY,
		    K_TO_K, kcred, &rval);

		if (i_setmodes(stdindev, &termios) == 0) { /* set baud rate */
			if (error = strioctl(stdinvp, TCSETSF, (int)&termios,
				FREAD+FNOCTTY, K_TO_K, kcred, &rval)) {
				cmn_err(CE_WARN, "%s: TCSETSF error %d", emsg,
					error);
				lwp->lwp_error = 0;
			}
		}
	}

	/*
	 * Open the "workstation console" device, and link the
	 * standard input device under it. (Which may or may not be the
	 * physical console keyboard.)
	 */
	major = ddi_name_to_major("wc");
	rwsconsdev = makedevice(major, 0);
	rwsconsvp = makespecvp(rwsconsdev, VCHR);
	if (error = VOP_OPEN(&rwsconsvp, FREAD+FWRITE, kcred)) {
		cmn_err(CE_PANIC,
		    "%s: workstation console open failed: error %d",
		    emsg, error);
		/* NOTREACHED */
	}

	if (error = falloc(stdinvp, FREAD+FWRITE+FNOCTTY, &fp, &fd)) {
		cmn_err(CE_PANIC,
		    "%s: can't get fd for console keyboard: error %d",
		    emsg, error);
		/* NOTREACHED */
	}
	setf(fd, fp);
	/* single theaded - no  close will occure here */
	mutex_exit(&fp->f_tlock);
	if (error = strioctl(rwsconsvp, I_PLINK, fd, FREAD+FWRITE+FNOCTTY,
	    K_TO_K, kcred, &rval)) {
		cmn_err(CE_PANIC, "%s: rwscons I_PLINK failed: error %d",
			emsg, error);
		/* NOTREACHED */
	}
	setf(fd, NULLFP);
	/* single theaded - no other open will occure here */
	(void) closef(fp);	/* don't need this any more */

	/* now we must close it to make console logins happy */
	(void) ddi_hold_installed_driver(getmajor(rwsconsdev));
	VOP_CLOSE(rwsconsvp, FREAD+FWRITE, 1, (offset_t)0, kcred);

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
	if (!rconsvp) {
		/*
		 * The workstation console driver needs to see rwsconsvp, but
		 * all other access should be through the redirecting driver.
		 */
		(void) ddi_hold_installed_driver(major);
		rconsdev = wsconsdev;
		rconsvp = wsconsvp;
	}
	cn_conf = 0;		/* OK to use rconsvp, now. */

}

#else	/* !MPSAS */

extern char *get_sim_console_name();

void
consconfig(void)
{
	int error, rval;
	int zeropgrp = 0;
	int major;
	char *simc = get_sim_console_name();

	stop_mon_clock();	/* turn off monitor polling clock */

	ddi_install_driver(simc);
	major = ddi_name_to_major("simc");
	rconsdev = makedevice(major, 0);
	/*
	 * Console is a CPU serial port.
	 */

	rconsvp = makespecvp(rconsdev, VCHR);

	/*
	 * Opening causes interrupts, etc. to be initialized.
	 * Console device drivers must be able to do output
	 * after being closed!
	 */

	if (error = VOP_OPEN(&rconsvp, FREAD+FWRITE+FNOCTTY, kcred))
		printf("console open failed: error %d\n", error);

	/*
	 * "Undo undesired ttyopen side effects" (not needed anymore
	 * in 5.0 -verified by commenting this out and running anyway.
	 * This zereod u.u_ttyp and u.u_ttyd and u.u_procp->p_pgrp).
	 */

	(void) strioctl(rconsvp, TIOCSPGRP, (int)&zeropgrp,
	    FREAD+FNOCTTY, K_TO_K, kcred, &rval);

	/* now we must close it to make console logins happy */
	(void) ddi_hold_installed_driver(major);
	VOP_CLOSE(rconsvp, FREAD+FWRITE, 1, (offset_t)0, kcred);
	rwsconsvp = rconsvp;
}

#endif /* !SAS && !MPSAS */
