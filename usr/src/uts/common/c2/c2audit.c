/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ident	"@(#)c2audit.c 1.29	96/09/24 SMI"

/*
 * These routines are responsible for constructing the
 * downloadable module to perform auditing and device
 * allocating.
 *
 * This file normally contains the interface routines between the kernel and
 * the actual code that performs the auditing.
 *
 * Only stubs to those functions are included in the base os.
 */

#include <sys/types.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/cred.h>
#include <sys/stropts.h>
#include <sys/systm.h>
#include <sys/pathname.h>
#include <sys/exec.h>
#include <sys/thread.h>

/*
 * This is the loadable module wrapper.
 */

#include <sys/modctl.h>
#include "sys/syscall.h"

extern auditsys();

static struct sysent auditsysent = {
	6,
	0,
	auditsys
};

struct auditcalls {
	int	code;
	int	a1;
	int	a2;
	int	a3;
	int	a4;
	int	a5;
};

/*
 * Module linkage information for the kernel.
 */
extern struct mod_ops mod_syscallops;


static struct modlsys modlsys = {
	&mod_syscallops, "C2 audit call", &auditsysent
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlsys, 0
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

auditsys(uap, rvp)
	register struct auditcalls *uap;
	rval_t *rvp;
{
	printf("audit_sys code %d...\n", uap->code);
}

audit_init()
{
}

audit_free()
{
}

/*
 * ----------------------------------------------------------------------
 * This hook must be in the (to be added) mutex in lookuppn() about the
 * pn_peekchar "if" statement so that the cwd/root names are correct when
 * used to anchor the path names.
 * ----------------------------------------------------------------------
 */
/*
 * Enter system call. Do any necessary setup here. allocate resouces, etc.
 */
audit_start(type, scid, error, lwp)
	unsigned type;
	unsigned scid;
	int error;
	klwp_t *lwp;
{
}

/*
 * system call has completed. Now determine if we genearate an audit record
 * or not.
 */
audit_finish(type, scid, error, rval)
	unsigned type;
	unsigned scid;
	int error;
	rval_t *rval;
{
}


/*
 * record if suser is called and if it succeeds or fails.
 */
audit_suser(flg)
	int flg;
{
}

/*
 * newproc() audit stub. Duplicate any information associated with proc struct.
 */
audit_newproc(pid)
	proc_t *pid;
{
}

/*
 * free proc audit_data space
 */
audit_pfree(pid)
	proc_t *pid;
{
}

/*
 * clear flags that cause audit record generation. This is mainly used in
 * fork where we return twice. We only want a record generated by the
 * parent process and not the child.
 */
audit_clear(pid)
	proc_t *pid;
{
}

/* not needed, part of audit_finish for chdir system call */
audit_chdir(v)
	int v;
{
}

/* not needed, part of audit_finish for fchdir system call */
audit_fchdir(fd, v)
	file_t *fd;
	int v;
{
}

/* not needed, part of audit_finish for chroot system call */
audit_chroot(v)
	int v;
{
}

/* not needed, part of audit_finish for fchroot system call */
audit_fchroot(fd, v)
	file_t *fd;
	int v;
{
}

audit_falloc(fd)
	file_t *fd;
{
}

audit_unfalloc(fd)
	file_t *fd;
{
}

/*
 * update pwd/root for current process.
 */
audit_chdirec(vp, vpp)
	vnode_t *vp;
	vnode_t **vpp;
{
}

audit_getf(fd)
	int fd;
{
}

audit_closef(fd)
	file_t *fd;
{
}

audit_core_start(sig)
	int sig;
{
}

audit_core_finish(f)
	int f;
{
}

audit_stropen(vp, devp, flag, crp)
	struct vnode *vp;
	dev_t *devp;
	int flag;
	cred_t *crp;
{
}

audit_strclose(vp, flag, crp)
	struct vnode *vp;
	int flag;
	cred_t *crp;
{
}

audit_strioctl(vp, cmd, arg, flag, copyflag, crp, rvalp)
	struct vnode *vp;
	int cmd;
	intptr_t arg;
	int flag;
	int copyflag;
	cred_t *crp;
	int *rvalp;
{
}

audit_strputmsg(vp, mctl, mdata, pri, flag, fmode)
	struct vnode  *vp;
	struct strbuf *mctl;
	struct strbuf *mdata;
	unsigned char  pri;
	int flag;
	int fmode;
{
}

struct fcntla {
	int fdes;
	int cmd;
	int arg;
};

int
audit_c2_revoke(uap, rvp)
	register struct fcntla *uap;
	rval_t *rvp;
{
	return (0);
}

/*
 * We have reached the end of a path in fs/lookup.c . We get three pieces of
 * information here: the path (pnp), the vnode of the last component (vp), and
 * the status of the last access (f).
 */
audit_savepath(pnp, vp, error)
	struct pathname *pnp;
	vnode_t *vp;
	int error;
{
}

/*
 * starting path search in lookup.
 */
audit_anchorpath(pnp, flag)
	struct pathname *pnp;
	int	flag;
{
}

/*
 * Error condition where next component was in error (append to path anyway)
 */
audit_addcomponent(pnp)
	struct pathname *pnp;
{
}

audit_symlink(pnp, linkpath_p)
	struct pathname *pnp, *linkpath_p;
{
}


audit_exit()
{
}

audit_exec(Usrstack, argvp_end, size, na, ne)
u_int Usrstack;
u_int argvp_end;
u_int size;
int  na, ne;
{
}

audit_vncreate_start()
{
}

audit_vncreate_finish(vp, error)
struct vnode *vp;
int error;
{
}

audit_enterprom(flag)
int flag;
{
}

audit_exitprom(flag)
int flag;
{
}

audit_thread_create(t, state)
	kthread_id_t t;
	int state;
{
}

audit_thread_free(t)
	kthread_id_t t;
{
}

audit_copen(fd, fp, vp)
	int fd;
	file_t  *fp;
	vnode_t	*vp;
{
}

audit_setf(fp, fd)
	file_t *fp;
	int fd;

{
}

audit_strgetmsg(vp, mctl, mdata, pri, flag, fmode)
	struct vnode  *vp;
	struct strbuf *mctl;
	struct strbuf *mdata;
	unsigned char  *pri;
	int *flag;
	int fmode;
{
}

audit_sock(type, q, mp, from)
	int type;	/* type of tihdr.h header requests */
	queue_t *q;	/* contains the process and thread audit data */
	mblk_t *mp;	/* contains the tihdr.h header structures */
	int from;	/* timod or sockmod request */
{
}
