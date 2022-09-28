/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.
 */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)ttvx.c	1.7	94/03/31 SMI"	/* from USL/MP:ttvx.c	1.3.3.2 */

/*
 * IWE TTVX module; module to support VP/ix on serial terminal
 */


#include <sys/types.h>
#include <sys/param.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/errno.h>
#include <sys/thread.h>
#include <sys/tss.h>
#include <sys/proc.h>
#include <sys/v86intr.h>
#include <sys/kmem.h>
#include <sys/ddi.h>

#ifdef _VPIX

/*
 * This is the loadable module wrapper.
 */
#include <sys/conf.h>
#include <sys/modctl.h>

#define	TTVX_CONF_FLAG	(D_OLD)

extern struct streamtab	ttvxinfo;

static struct fmodsw	fsw = {
	"ttvx",
	&ttvxinfo,
	TTVX_CONF_FLAG
};

extern struct mod_ops	mod_strmodops;

static struct modlstrmod modlstrmod = {
	&mod_strmodops,
	"VPix tty converter",
	&fsw
};

/*
 * Module linkage information for the kernel.
 */
static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlstrmod, NULL
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

static int ttvx_open(/*queue_t *q, dev_t * devp, int oflag, int sflag*/);
static int ttvx_close(/*queue_t *q*/);
static int ttvx_read_serv(/*queue_t *q*/);
static int ttvx_write_put(/*queue_t *q, mblk_t *mp*/);

static struct module_info ttvx_iinfo = {
	0,
	"ttvx",
	0,
	INFPSZ,
	1000,
	100
};

static struct qinit ttvx_rinit = {
	putq,
	ttvx_read_serv,
	ttvx_open,
	ttvx_close,
	NULL,
	&ttvx_iinfo
};

static struct module_info ttvx_oinfo = {
	0,
	"ttvx",
	0,
	INFPSZ,
	0,
	0
};

static struct qinit ttvx_winit = {
	ttvx_write_put,
	NULL,
	ttvx_open,
	ttvx_close,
	NULL,
	&ttvx_oinfo
};

struct streamtab ttvxinfo = {
	&ttvx_rinit,
	&ttvx_winit,
	NULL,
	NULL
};


/*
 * VP/ix tty module open.
 * Return 0 for sucess, an errno for failure.
 */

/*ARGSUSED*/
static int
ttvx_open(qp, devp, oflag, sflag)
	queue_t *qp;
	dev_t *devp;
	int oflag, sflag;
{
	register v86int_t *v86i;

	/* if q_ptr is non-null, we are already open */
	if (qp->q_ptr != (caddr_t) NULL) {
		return (0);
	}

	/*
	 * Save the VP/ix data structure for delivering pseudorupts.
	 * Refuse to open if not a VP/ix process.
	 */
	if (!curthread->t_v86data) {
		return (EINVAL);
	}

	v86i = kmem_zalloc(sizeof (v86int_t), KM_SLEEP);
	v86stash(v86i, (caddr_t)0);

	/* set ptrs in queues to point to state structure */
	qp->q_ptr = v86i;
	WR(qp)->q_ptr = v86i;

	return (0);
}


static int
ttvx_close(qp)
	register queue_t *qp;
{
	int oldpri;
	v86int_t *v86i = (v86int_t *)qp->q_ptr;

	flushq(qp, FLUSHDATA);
	v86unstash(v86i);
	oldpri = splstr();
	qp->q_ptr = NULL;
	kmem_free(v86i, sizeof (v86int_t));
	(void) splx(oldpri);

	return (0);
}

static int
ttvx_read_serv(qp)
	register queue_t *qp;
{
	register v86int_t *v86i;
	mblk_t *mp;

	while ((mp = getq(qp)) != NULL) {
		if (mp->b_datap->db_type <= QPCTL && !canput(qp->q_next)) {
			(void) putbq(qp, mp);
			return;
		}

		switch (mp->b_datap->db_type) {

		default:
			putnext(qp, mp);
			continue;

		case M_FLUSH:
			flushq(qp, FLUSHDATA);
			putnext(qp, mp);
			continue;

		case M_DATA:
			putnext(qp, mp);
			/* Request pseudorupt */
			v86i = (v86int_t *)qp->q_ptr;
			v86sdeliver(v86i, V86VI_KBD, qp);
			continue;

		} /* switch */
	} /* while */
}

static int
ttvx_write_put(q, mp)
queue_t *q;
mblk_t *mp;
{
	putnext(q, mp);
}
#endif
