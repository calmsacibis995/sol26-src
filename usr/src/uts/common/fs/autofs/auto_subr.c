/*
 *	Copyright (c) 1993-1996 by Sun Microsystems, Inc.
 *	All Rights Reserved.
 */

#pragma ident	"@(#)auto_subr.c	1.44	96/10/17 SMI"

#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/pathname.h>
#include <sys/cred.h>
#include <sys/mount.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/systm.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <sys/ticotsord.h>
#include <sys/dirent.h>
#include <fs/fs_subr.h>
#include <rpcsvc/autofs_prot.h>
#include <sys/fs/autofs.h>

#define	TYPICALMAXPATHLEN	64

kmutex_t fnnode_count_lock;
static int fnnode_cnt = 0;

static int auto_perform_link(fnnode_t *, struct linka *, cred_t *);
static int auto_perform_actions(fninfo_t *, fnnode_t *,
				action_list *, cred_t *);
static int auto_getmntpnt(vnode_t *, char *, vnode_t **, cred_t *);
static int auto_lookup_request(fninfo_t *, char *, struct linka *,
				cred_t *, bool_t);
static int auto_mount_request(fninfo_t *, char *, action_list **,
				cred_t *, bool_t);
static int autofs_verbose = 0;

/*
 * Clears the MF_INPROG flag, and wakes up those threads sleeping on
 * fn_cv_mount if MF_WAITING is set.
 */
void
auto_unblock_others(
	fnnode_t *fnp,
	u_int operation		/* either MF_INPROG or MF_LOOKUP */
)
{
	ASSERT(operation & (MF_INPROG | MF_LOOKUP));
	mutex_enter(&fnp->fn_lock);
	fnp->fn_flags &= ~operation;
	if (fnp->fn_flags & MF_WAITING) {
		fnp->fn_flags &= ~MF_WAITING;
		cv_broadcast(&fnp->fn_cv_mount);
	}
	mutex_exit(&fnp->fn_lock);
}

int
auto_wait4mount(fnnode_t *fnp)
{
	int error;
	k_sigset_t smask;

	AUTOFS_DPRINT((4, "auto_wait4mount: fnp=%x\n", fnp));

	mutex_enter(&fnp->fn_lock);
	while (fnp->fn_flags & (MF_INPROG | MF_LOOKUP)) {
		/*
		 * There is a mount or a lookup in progress.
		 */
		fnp->fn_flags |= MF_WAITING;
		sigintr(&smask, 1);
		if (!cv_wait_sig(&fnp->fn_cv_mount, &fnp->fn_lock)) {
			/*
			 * Decided not to wait for operation to
			 * finish after all.
			 */
			sigunintr(&smask);
			mutex_exit(&fnp->fn_lock);
			return (EINTR);
		}
		sigunintr(&smask);
	}
	error = fnp->fn_error;

	if (error == EINTR) {
		/*
		 * The thread doing the mount got interrupted, we need to
		 * try again, by returning EAGAIN.
		 */
		error = EAGAIN;
	}
	mutex_exit(&fnp->fn_lock);

	AUTOFS_DPRINT((5, "auto_wait4mount: fnp=%x error=%d\n", fnp, error));
	return (error);
}

int
auto_lookup_aux(
	fnnode_t *fnp,
	char *name,
	cred_t *cred
)
{
	struct fninfo *fnip;
	struct linka link;
	int error = 0;

	fnip = vfstofni(fntovn(fnp)->v_vfsp);
	bzero((caddr_t)&link, sizeof (link));
	error = auto_lookup_request(fnip, name, &link, cred, TRUE);
	if (!error && link.link != NULL) {
		/*
		 * This node should be a symlink
		 */
		error = auto_perform_link(fnp, &link, cred);
		kmem_free(link.dir, strlen(link.dir) + 1);
		kmem_free(link.link, strlen(link.link) + 1);
	}

	mutex_enter(&fnp->fn_lock);
	fnp->fn_error = error;
	mutex_exit(&fnp->fn_lock);

	/*
	 * Notify threads waiting for lookup that
	 * it's done.
	 */
	AUTOFS_UNBLOCK_OTHERS(fnp, MF_LOOKUP);
	return (error);
}

static void
auto_mount_thread(
	struct autofs_callargs *argsp
)
{
	struct fninfo *fnip;
	fnnode_t *fnp;
	vnode_t *vp;
	char *name;
	int namelen;
	cred_t *cred;
	action_list *alp = NULL;
	int error;

	fnp = argsp->fnc_fnp;
	vp = fntovn(fnp);
	fnip = vfstofni(vp->v_vfsp);
	name = argsp->fnc_name;
	cred = argsp->fnc_cred;

	error = auto_mount_request(fnip, name, &alp, cred, TRUE);
	if (!error)
		error = auto_perform_actions(fnip, fnp, alp, cred);
	fnp->fn_error = error;

	/*
	 * Notify threads waiting for mount that
	 * it's done.
	 */
	AUTOFS_UNBLOCK_OTHERS(fnp, MF_INPROG);

	VN_RELE(vp);
	crfree(argsp->fnc_cred);
	namelen = strlen(argsp->fnc_name) + 1;
	kmem_free(argsp->fnc_name, namelen);
	kmem_free((caddr_t)argsp, sizeof (*argsp));

	thread_exit();
	/* NOTREACHED */
}

static int autofs_thr_success = 0;
static int autofs_thr_fail = 0;

/*
 * Creates new thread which calls auto_mount_thread which does
 * the bulk of the work calling automountd and doing the postmount
 * work, via 'auto_perform_actions'.
 */
void
auto_new_mount_thread(
	fnnode_t *fnp,
	char *name,
	cred_t *cred
)
{
	struct autofs_callargs *argsp;

	argsp = (struct autofs_callargs *)
		kmem_alloc(sizeof (*argsp), KM_SLEEP);
	VN_HOLD(fntovn(fnp));
	argsp->fnc_fnp = fnp;
	argsp->fnc_name = (char *) kmem_alloc(strlen(name) + 1, KM_SLEEP);
	(void) strcpy(argsp->fnc_name, name);
	argsp->fnc_origin = curthread;
	crhold(cred);
	argsp->fnc_cred = cred;

	while (thread_create(NULL, NULL, auto_mount_thread, (caddr_t)argsp,
				0, &p0, TS_RUN, 60) == NULL) {
		/*
		 * thread creation failed, delay for seconds and
		 * try again
		 */
		autofs_thr_fail++;
		delay(20 * hz);
	}
	autofs_thr_success++;
}

static int auto_daemon_ok = 1;

int
auto_calldaemon(
	fninfo_t *fnip,
	int which,
	xdrproc_t xdrargs,
	caddr_t argsp,
	xdrproc_t xdrres,
	caddr_t resp,
	cred_t *cred,
	bool_t hard)				/* retry forever? */
{
	CLIENT *client;
	enum clnt_stat status;
	struct rpc_err rpcerr;
	struct timeval wait;
	bool_t tryagain;
	int error = 0;
	k_sigset_t smask;

	AUTOFS_DPRINT((4, "auto_calldaemon\n"));

	error = clnt_tli_kcreate(&fnip->fi_knconf, &fnip->fi_addr,
		AUTOFS_PROG, AUTOFS_VERS, 0, INT_MAX, cred, &client);

	if (error) {
		auto_log(CE_WARN,
			"autofs: clnt_tli_kcreate: error %d\n", error);
		goto done;
	}

	wait.tv_sec = fnip->fi_rpc_to;
	wait.tv_usec = 0;
	do {
		tryagain = FALSE;
		error = 0;

		/*
		 * Mask out all signals except SIGHUP, SIGINT, SIGQUIT
		 * and SIGTERM. (Preserving the existing masks)
		 */
		sigintr(&smask, 1);

		status = CLNT_CALL(client, which, xdrargs, argsp,
				xdrres, resp, wait);

		/*
		 * Restore original signal mask
		 */
		sigunintr(&smask);

		switch (status) {
		case RPC_SUCCESS:
			break;

		case RPC_INTR:
			error = EINTR;
			break;

		case RPC_TIMEDOUT:
			tryagain = TRUE;
			error = ETIMEDOUT;
			break;

		case RPC_CANTCONNECT:
		case RPC_CANTCREATESTREAM:
			/*
			 * The connection could not be established
			 */
			/* fall thru */
		case RPC_XPRTFAILED:
			/*
			 * The connection could not be established or
			 * was dropped, we differentiate between the two
			 * conditions by calling CLNT_GETERR and look at
			 * rpcerror.re_errno.
			 * If rpcerr.re_errno == ECONNREFUSED, then the
			 * connection could not be established at all,
			 * at which point we should return if hard is not set.
			 */
			error = ECONNREFUSED;
			if (status == RPC_XPRTFAILED) {
				CLNT_GETERR(client, &rpcerr);
				if (rpcerr.re_errno != ECONNREFUSED) {
					/*
					 * The connection was dropped, treat it
					 * as a timeout, and retry indefinitely
					 * since non-idempotent operations may
					 * have been performed already.
					 */
					error = ETIMEDOUT;
					hard = TRUE;
				}
			}
			tryagain = hard;
			if (auto_daemon_ok) {
				auto_daemon_ok = 0;
				if (tryagain) {
				    printf(
				    "automountd not running, retrying\n");
				}
			}
			break;

		default:
			auto_log(CE_WARN, "autofs: %s\n", clnt_sperrno(status));
			error = ENOENT;
			break;
		}
	} while (tryagain);

	if (status == RPC_SUCCESS && auto_daemon_ok == 0) {
		auto_daemon_ok  = 1;
		printf("automountd OK\n");
	}

	auth_destroy(client->cl_auth);
	clnt_destroy(client);

done:	ASSERT(status == RPC_SUCCESS || error != 0);

	AUTOFS_DPRINT((5, "auto_calldaemon error=%d\n", error));
	return (error);
}

static int
auto_null_request(
	fninfo_t *fnip,
	cred_t *cred,
	bool_t hard
)
{
	int error;

	AUTOFS_DPRINT((4, "\tauto_null_request\n"));

	error = auto_calldaemon(fnip, NULLPROC,
				xdr_void, (char *)NULL,
				xdr_void, (char *)NULL,
				cred, hard);

	AUTOFS_DPRINT((5, "\tauto_null_request: error=%d\n", error));
	return (error);
}

static int
auto_lookup_request(
	fninfo_t *fnip,
	char *key,
	struct linka *lnp,
	cred_t *cred,
	bool_t hard
)
{
	int error;
	struct autofs_lookupargs request;
	struct autofs_lookupres result;
	struct linka *p;

	AUTOFS_DPRINT((4, "auto_lookup_request: path=%s name=%s\n",
		fnip->fi_path, key));

	request.map = fnip->fi_map;
	request.path = fnip->fi_path;

	if (fnip->fi_flags & MF_DIRECT)
		request.name = fnip->fi_key;
	else
		request.name = key;
	AUTOFS_DPRINT((4, "auto_lookup_request: using key=%s\n", request.name));

	request.subdir = fnip->fi_subdir;
	request.opts = fnip->fi_opts;
	request.isdirect = fnip->fi_flags & MF_DIRECT ? TRUE : FALSE;

	bzero((caddr_t)&result, sizeof (result));
	error = auto_calldaemon(fnip, AUTOFS_LOOKUP,
				xdr_autofs_lookupargs, (caddr_t) &request,
				xdr_autofs_lookupres, (caddr_t) &result,
				cred, hard);
	if (!error) {
		autofs_verbose = result.lu_verbose;
		switch (result.lu_res) {
		case AUTOFS_OK:
			switch (result.lu_type.action) {
			case AUTOFS_MOUNT_RQ:
				lnp->link = NULL;
				lnp->dir = NULL;
				break;
			case AUTOFS_LINK_RQ:
				p =
				&result.lu_type.lookup_result_type_u.lt_linka;
				lnp->dir =
				    kmem_alloc(strlen(p->dir) + 1, KM_SLEEP);
				strcpy(lnp->dir, p->dir);
				lnp->link =
				    kmem_alloc(strlen(p->link) + 1, KM_SLEEP);
				strcpy(lnp->link, p->link);
				break;
			default:
				auto_log(CE_WARN,
				    "auto_lookup_request: bad action type %d\n",
				    result.lu_res);
				error = ENOENT;
			}
			break;
		case AUTOFS_NOENT:
			error = ENOENT;
			break;
		default:
			error = ENOENT;
			auto_log(CE_WARN,
				"auto_lookup_request: unknown result: %d\n",
				result.lu_res);
			break;
		}
	}

done:
	xdr_free(xdr_autofs_lookupres, (char *) &result);

	AUTOFS_DPRINT((5, "auto_lookup_request: path=%s name=%s error=%d\n",
		fnip->fi_path, key, error));
	return (error);
}

static int
auto_mount_request(
	fninfo_t *fnip,
	char *key,
	action_list **alpp,
	cred_t *cred,
	bool_t hard
)
{
	int error;
	struct autofs_lookupargs request;
	struct autofs_mountres *result;
	int need2free = 1;	/* free result upon return? */

	AUTOFS_DPRINT((4, "auto_mount_request: path=%s name=%s\n",
		fnip->fi_path, key));

	request.map = fnip->fi_map;
	request.path = fnip->fi_path;

	if (fnip->fi_flags & MF_DIRECT)
		request.name = fnip->fi_key;
	else
		request.name = key;
	AUTOFS_DPRINT((4, "auto_mount_request: using key=%s\n", request.name));

	request.subdir = fnip->fi_subdir;
	request.opts = fnip->fi_opts;
	request.isdirect = fnip->fi_flags & MF_DIRECT ? TRUE : FALSE;

	*alpp = NULL;
	result =
	    (struct autofs_mountres *)kmem_zalloc(sizeof (*result), KM_SLEEP);
	error = auto_calldaemon(fnip, AUTOFS_MOUNT,
				xdr_autofs_lookupargs, (caddr_t) &request,
				xdr_autofs_mountres, (caddr_t) result,
				cred, hard);
	if (!error) {
		autofs_verbose = result->mr_verbose;
		switch (result->mr_type.status) {
		case AUTOFS_ACTION:
			error = 0;
			*alpp = result->mr_type.mount_result_type_u.list;
			need2free = 0;
			break;
		case AUTOFS_DONE:
			error = result->mr_type.mount_result_type_u.error;
			break;
		default:
			error = ENOENT;
			auto_log(CE_WARN,
				"auto_mount_request: unknown status %d\n",
				result->mr_type.status);
			break;
		}
	}

done:
	if (need2free) {
		/*
		 * Free storage allocated by xdr only if no action
		 * is further needed, and no communication error occurred.
		 */
		xdr_free(xdr_autofs_mountres, (char *) result);
		kmem_free(result, sizeof (*result));
	}

	AUTOFS_DPRINT((5, "auto_mount_request: path=%s name=%s error=%d\n",
		fnip->fi_path, key, error));
	return (error);
}

static int
auto_postmount_request(
	struct mounta *m,
	dev_t devid,
	fninfo_t *fnip,
	bool_t hard,
	cred_t *cred
)
{
	postmountreq req;
	postmountres result;
	struct autofs_args *argsp;
	int error;

	argsp = (struct autofs_args *)m->dataptr;

	AUTOFS_DPRINT((4, "auto_postmount_request: path: %s\n", argsp->path));

	req.special = m->spec;
	req.mountp = argsp->path;
	req.fstype = m->fstype;
	req.mntopts = argsp->opts;
	req.devid = devid;

	error = auto_calldaemon(fnip, AUTOFS_POSTMOUNT,
				xdr_postmountreq, (caddr_t) &req,
				xdr_postmountres, (caddr_t) &result,
				cred, hard);
	if (!error)
		error = result.status;

	AUTOFS_DPRINT((5, "auto_postmount_request: error=%d\n", error));

	return (error);
}

static int
auto_send_unmount_request(
	fninfo_t *fnip,
	umntrequest *ul,
	cred_t *cred,
	bool_t hard
)
{
	int error;
	umntres result;

	AUTOFS_DPRINT((4, "\tauto_send_unmount_request: dev=%x rdev=%x\n",
		ul->devid, ul->rdevid));

	error = auto_calldaemon(fnip, AUTOFS_UNMOUNT,
				xdr_umntrequest, (caddr_t) ul,
				xdr_umntres, (caddr_t) &result,
				cred, hard);
	if (!error)
		error = result.status;

	AUTOFS_DPRINT((5, "\tauto_send_unmount_request: error=%d\n", error));

	return (error);
}

static int
auto_postunmount_request(
	fninfo_t *fnip,
	postumntreq *ul,
	cred_t *cred,
	bool_t hard
)
{
	int error;
	postumntres result;

	AUTOFS_DPRINT((4, "auto_postunmount_request: u=%x\n", ul));

	error = auto_calldaemon(fnip, AUTOFS_POSTUNMOUNT,
				xdr_postumntreq, (caddr_t) ul,
				xdr_postumntres, (caddr_t) &result,
				cred, hard);
	if (!error)
		error = result.status;

	AUTOFS_DPRINT((5, "auto_postunmount_request: error=%d\n", error));

	return (error);
}

static int
auto_perform_link(
	fnnode_t *fnp,
	struct linka *linkp,
	cred_t *cred
)
{
	vnode_t *vp;
	int len;
	char *tmp;

	AUTOFS_DPRINT((3, "auto_perform_link: fnp=%x dir=%s link=%s\n",
		fnp, linkp->dir, linkp->link));

	len = strlen(linkp->link) + 1;		/* include '\0' */
	tmp = (char *) kmem_zalloc(len, KM_SLEEP);
	kcopy(linkp->link, tmp, (u_int)len);
	mutex_enter(&fnp->fn_lock);
	fnp->fn_symlink = tmp;
	fnp->fn_symlinklen = len;
	fnp->fn_flags |= MF_THISUID_MATCH_RQD;
	crhold(cred);
	fnp->fn_cred = cred;
	mutex_exit(&fnp->fn_lock);

	vp = fntovn(fnp);
	vp->v_type = VLNK;

	return (0);
}

static int
auto_perform_actions(
	fninfo_t *dfnip,
	fnnode_t *dfnp,
	action_list *alp,
	cred_t *cred
)
{
	action_list *p;
	struct mounta *m;
	struct autofs_args *argsp;
	int error = 0, success = 0;
	vnode_t *mvp, *dvp, *newvp;
	fnnode_t *newfnp, *mfnp;
	int auto_mount = 0;
	int save_triggers = 0;		/* set when we need to save at least */
					/* one trigger node */
	int update_times = 0;
	dev_t devid;
	char *mntpnt;
	char buff[AUTOFS_MAXPATHLEN];

	AUTOFS_DPRINT((4, "auto_perform_actions: alp=%x\n", alp));

	dvp = fntovn(dfnp);
	if (dvp->v_vfsmountedhere != NULL) {
		/*
		 * The daemon successfully mounted a filesystem
		 * on the AUTOFS root node.
		 */
		success++;
	}

	for (p = alp; p != NULL; p = p->next) {
		auto_mount = 0;
		m = (struct mounta *) &p->action.action_list_entry_u.mounta;
		argsp = (struct autofs_args *)m->dataptr;
		/*
		 * use the parent directory's timeout since it's the
		 * one specified/inherited by automount.
		 */
		argsp->mount_to = dfnip->fi_mount_to;
		/*
		 * The mountpoint is relative, and it is guaranteed to
		 * begin with "."
		 */
		ASSERT(m->dir[0] == '.');
		if (m->dir[0] == '.' && m->dir[1] == '\0') {
			/*
			 * mounting on the trigger node
			 */
			mvp = dvp;
			VN_HOLD(mvp);
			goto mount;
		}
		/*
		 * ignore "./" in front of mountpoint
		 */
		ASSERT(m->dir[1] == '/');
		mntpnt = m->dir + 2;

AUTOFS_DPRINT((10, "\tdfnip->fi_path=%s\n", dfnip->fi_path));
AUTOFS_DPRINT((10, "\tdfnip->fi_flags=%x\n", dfnip->fi_flags));
AUTOFS_DPRINT((10, "\tmntpnt=%s\n", mntpnt));

		if (dfnip->fi_flags & MF_DIRECT) {
			AUTOFS_DPRINT((10, "\tDIRECT\n"));
			sprintf(buff, "%s/%s", dfnip->fi_path, mntpnt);
		} else {
			AUTOFS_DPRINT((10, "\tINDIRECT\n"));
			sprintf(buff, "%s/%s/%s", dfnip->fi_path,
				dfnp->fn_name, mntpnt);
		}

		if (dvp->v_vfsmountedhere == NULL) {
			/*
			 * Daemon didn't mount anything on the root
			 * We have to create the mountpoint if it doesn't
			 * exist already
			 */
			rw_enter(&dfnp->fn_rwlock, RW_WRITER);
			if (auto_search(dfnp, mntpnt, &mfnp, cred) == 0) {
				/*
				 * AUTOFS mountpoint exists
				 */
				error = 0;
				if (fntovn(mfnp)->v_vfsmountedhere != NULL) {
				    cmn_err(CE_PANIC,
				    "auto_perform_actions: mfnp=%x covered\n",
				    mfnp);
				}
			} else {
				/*
				 * Create AUTOFS mountpoint
				 */
				error = auto_enter(dfnp, mntpnt, &mfnp, kcred);
				ASSERT(mfnp->fn_linkcnt == 1);
				mfnp->fn_linkcnt++;
			}
			if (!error)
				update_times = 1;
			rw_exit(&dfnp->fn_rwlock);
			ASSERT(error != EEXIST);
			if (!error) {
				mvp = fntovn(mfnp);
			} else {
				auto_log(CE_WARN,
			"autofs: mount of %s failed - can't create mountpoint.",
				buff);
				continue;
			}
		} else {
			/*
			 * Find mountpoint in VFS mounted here. If not found,
			 * fail the submount, though the overall mount has
			 * succeded since the root is mounted.
			 */
			if (error = auto_getmntpnt(dvp, mntpnt, &mvp, kcred)) {
auto_log(CE_WARN,
"autofs: mount of %s failed - mountpoint doesn't exist.", buff);
				continue;
			}
			if (mvp->v_type == VLNK) {
auto_log(CE_WARN,
"autofs: %s symbolic link: not a valid mountpoint - mount failed\n", buff);
				VN_RELE(mvp);
				error = ENOENT;
				continue;
			}
		}
mount:
		m->flags |= MS_SYSSPACE;
		if ((error = auto_inkernel_mount(m, mvp)) != 0) {
			auto_log(CE_WARN,
			    "autofs: inkernel mount of %s failed error=%d\n",
			    buff, error);
			continue;
		}
		/*
		 * If mountpoint is an AUTOFS node, then I'm going to
		 * flag it that the Filesystem mounted on top was mounted
		 * in the kernel so that the unmount can be done inside the
		 * kernel as well.
		 * I don't care to flag non-AUTOFS mountpoints when an AUTOFS
		 * in-kernel mount was done on top, because the unmount
		 * routine already knows that such case was done in the kernel.
		 */
		if (dvp->v_vfsp->vfs_op == mvp->v_vfsp->vfs_op) {
			mfnp = vntofn(mvp);
			mutex_enter(&mfnp->fn_lock);
			mfnp->fn_flags |= MF_IK_MOUNT;
			mutex_exit(&mfnp->fn_lock);
		}
		(void) VFS_ROOT(mvp->v_vfsmountedhere, &newvp);
		devid = newvp->v_vfsp->vfs_dev;
		error = auto_postmount_request(m, devid, dfnip, TRUE, cred);
		if (error) {
			/*
			 * XXX I should cleanup here. If I couldn't
			 * successfully add to mnttab, the unmount thread
			 * will be confused.
			 */
			auto_log(CE_WARN,
				"autofs: postmount of %s failed error=%d\n",
				mntpnt, error);
			error = 0;	/* mount didn't fail */
		}
		auto_mount = (dvp->v_vfsp->vfs_op == newvp->v_vfsp->vfs_op);
		newfnp = vntofn(newvp);
		newfnp->fn_parent = dfnp;

		/*
		 * At this time we want to save the AUTOFS filesystem as
		 * a trigger node. (We only do this if the mount occured
		 * on a node different from the root.
		 * We look at the trigger nodes during
		 * the automatic unmounting to make sure we remove them
		 * as a unit and remount them as a unit if the filesystem
		 * mounted at the root could not be unmounted.
		 */
		if (auto_mount && (error == 0) && (mvp != dvp)) {
			save_triggers++;
			/*
			 * Add AUTOFS mount to hierarchy
			 */
			newfnp->fn_flags |= MF_TRIGGER;
			rw_enter(&newfnp->fn_rwlock, RW_WRITER);
			newfnp->fn_next = dfnp->fn_trigger;
			rw_exit(&newfnp->fn_rwlock);
			rw_enter(&dfnp->fn_rwlock, RW_WRITER);
			dfnp->fn_trigger = newfnp;
			rw_exit(&dfnp->fn_rwlock);
			/*
			 * Don't VN_RELE(newvp) here since dfnp now holds
			 * reference to it as its trigger node.
			 */
			AUTOFS_DPRINT((10, "\tadding trigger %s to %s\n",
				newfnp->fn_name, dfnp->fn_name));
			AUTOFS_DPRINT((10, "\tfirst trigger is %s\n",
				dfnp->fn_trigger->fn_name));
			if (newfnp->fn_next != NULL)
				AUTOFS_DPRINT((10, "\tnext trigger is %s\n",
				    newfnp->fn_next->fn_name));
			else
				AUTOFS_DPRINT((10, "\tno next trigger\n"));
		} else
			VN_RELE(newvp);

		if (!error)
			success++;

		if (update_times)
			dfnp->fn_atime = dfnp->fn_mtime = hrestime;
	}

	/*
	 * Return failure if daemon didn't mount anything, and all
	 * kernel mounts attempted failed.
	 */
	error = success ? 0 :ENOENT;

done:
	if (alp != NULL) {
		if ((error == 0) && save_triggers) {
			/*
			 * Save action_list information, so that we can use it
			 * when it comes time to remount the trigger nodes
			 * The action list is freed when the directory node
			 * containing the reference to it is unmounted in
			 * unmount_tree().
			 */
			mutex_enter(&dfnp->fn_lock);
			ASSERT(dfnp->fn_alp == NULL);
			dfnp->fn_alp = alp;
			mutex_exit(&dfnp->fn_lock);
		} else {
			/*
			 * free the action list now,
			 */
			xdr_free(xdr_action_list, (char *)alp);
		}
	}

	AUTOFS_DPRINT((5, "auto_perform_actions: error=%d\n", error));
	return (error);
}

fnnode_t *
auto_makefnnode(
	vtype_t type,
	vfs_t *vfsp,
	char *name,
	cred_t *cred
)
{
	fnnode_t *fnp;
	vnode_t *vp;
	char *tmpname;
	/*
	 * autofs uses odd inode numbers
	 * automountd uses even inode numbers
	 */
	static ino_t nodeid = 3;

	fnp = (fnnode_t *) kmem_zalloc(sizeof (*fnp), KM_SLEEP);
	vp = fntovn(fnp);
	tmpname = (char *) kmem_alloc(strlen(name) + 1, KM_SLEEP);
	(void) strcpy(tmpname, name);
	fnp->fn_name = &tmpname[0];
	fnp->fn_namelen = strlen(tmpname) + 1;	/* include '\0' */
	fnp->fn_uid = cred->cr_uid;
	fnp->fn_gid = cred->cr_gid;
	/*
	 * ".." is added in auto_enter and auto_mount.
	 * "." is added in auto_mkdir and auto_mount.
	 */
	/*
	 * Note that fn_size and fn_linkcnt are already 0 since
	 * we used kmem_zalloc to allocated fnp
	 */
	fnp->fn_mode = AUTOFS_MODE;
	fnp->fn_atime = fnp->fn_mtime = fnp->fn_ctime = hrestime;
	fnp->fn_ref_time = fnp->fn_atime.tv_sec;
	mutex_enter(&fnnode_count_lock);
	fnp->fn_nodeid = nodeid;
	nodeid += 2;
	fnnode_cnt++;
	mutex_exit(&fnnode_count_lock);
	vp->v_op = &auto_vnodeops;
	vp->v_type = type;
	vp->v_data = (caddr_t) fnp;
	vp->v_vfsp = vfsp;
	mutex_init(&vp->v_lock, "vnode lock", MUTEX_DEFAULT, NULL);
	cv_init(&vp->v_cv, "vnode cv", CV_DEFAULT, NULL);
	mutex_init(&fnp->fn_lock, "fnnode lock", MUTEX_DEFAULT, NULL);
	rw_init(&fnp->fn_rwlock, "fnnode rwlock", RW_DEFAULT, NULL);
	cv_init(&fnp->fn_cv_mount, "autofs mount cv", CV_DEFAULT, NULL);

	return (fnp);
}


void
auto_freefnnode(fnnode_t *fnp)
{
	vnode_t *vp = fntovn(fnp);

	AUTOFS_DPRINT((4, "auto_freefnnode: fnp=%x\n", fnp));

	ASSERT(fnp->fn_linkcnt == 0);
	ASSERT(vp->v_count == 0);
	ASSERT(fnp->fn_dirents == NULL);
	ASSERT(fnp->fn_parent == NULL);

	kmem_free(fnp->fn_name, fnp->fn_namelen);
	if (fnp->fn_symlink) {
		ASSERT(fnp->fn_flags & MF_THISUID_MATCH_RQD);
		kmem_free(fnp->fn_symlink, fnp->fn_symlinklen);
	}
	if (fnp->fn_cred)
		crfree(fnp->fn_cred);
	mutex_destroy(&vp->v_lock);
	cv_destroy(&vp->v_cv);
	mutex_destroy(&fnp->fn_lock);
	rw_destroy(&fnp->fn_rwlock);
	cv_destroy(&fnp->fn_cv_mount);
	kmem_free(fnp, sizeof (*fnp));
	mutex_enter(&fnnode_count_lock);
	fnnode_cnt--;
	mutex_exit(&fnnode_count_lock);
}

void
auto_disconnect(
	fnnode_t *dfnp,
	fnnode_t *fnp
)
{
	fnnode_t *tmp, **fnpp;
	vnode_t *vp = fntovn(fnp);

	AUTOFS_DPRINT((4,
		"auto_disconnect: dfnp=%x fnp=%x linkcnt=%d\n v_count=%d",
		dfnp, fnp, fnp->fn_linkcnt, vp->v_count));

	ASSERT(RW_WRITE_HELD(&dfnp->fn_rwlock));
	ASSERT(fnp->fn_linkcnt == 1);

	if (vp->v_vfsmountedhere != NULL)
		cmn_err(CE_PANIC, "auto_disconnect: vp %x mounted on\n", vp);

	/*
	 * Decrement by 1 because we're removing the entry in dfnp.
	 */
	fnp->fn_linkcnt--;
	fnp->fn_size--;

	/*
	 * only changed while holding parent's (dfnp) rw_lock
	 */
	fnp->fn_parent = NULL;

	fnpp = &dfnp->fn_dirents;
	for (;;) {
		tmp = *fnpp;
		if (tmp == NULL) {
			cmn_err(CE_PANIC,
			    "auto_disconnect: %x not in %x dirent list\n",
			    fnp, dfnp);
		}
		if (tmp == fnp) {
			*fnpp = tmp->fn_next; 	/* remove it from the list */
			ASSERT(vp->v_count == 0);
			/* child had a pointer to parent ".." */
			dfnp->fn_linkcnt--;
			dfnp->fn_size--;
			break;
		}
		fnpp = &tmp->fn_next;
	}

	mutex_enter(&fnp->fn_lock);
	fnp->fn_atime = fnp->fn_mtime = hrestime;
	mutex_exit(&fnp->fn_lock);

	AUTOFS_DPRINT((5, "auto_disconnect: done\n"));
}

int
auto_enter(
	fnnode_t *dfnp,
	char *name,
	fnnode_t **fnpp,
	cred_t *cred
)
{
	struct fnnode *cfnp, **spp;
	vnode_t *dvp = fntovn(dfnp);
	u_short offset = 0;
	u_short diff;

	AUTOFS_DPRINT((4, "auto_enter: dfnp=%x, name=%s ", dfnp, name));

	ASSERT(RW_WRITE_HELD(&dfnp->fn_rwlock));

	cfnp = dfnp->fn_dirents;
	if (cfnp == NULL) {
		/*
		 * offset = 0 for '.' and offset = 1 for '..'
		 */
		spp = &dfnp->fn_dirents;
		offset = 2;
	}

	for (; cfnp; cfnp = cfnp->fn_next) {
		if (strcmp(cfnp->fn_name, name) == 0) {
			if (cfnp->fn_flags & MF_THISUID_MATCH_RQD) {
				/*
				 * "thisuser" kind of node, need to
				 * match CREDs as well
				 */
				if (crcmp(cfnp->fn_cred, cred) == 0)
					return (EEXIST);
			} else
				return (EEXIST);
		}

		if (cfnp->fn_next != NULL) {
			diff = cfnp->fn_next->fn_offset - cfnp->fn_offset;
			ASSERT(diff != 0);
			if ((diff > 1) && (offset == 0)) {
				offset = cfnp->fn_offset + 1;
				spp = &cfnp->fn_next;
			}
		} else if (offset == 0) {
			offset = cfnp->fn_offset + 1;
			spp = &cfnp->fn_next;
		}
	}

	*fnpp = auto_makefnnode(VDIR, dvp->v_vfsp, name, cred);
	if (*fnpp == NULL)
		return (ENOMEM);

	/*
	 * I don't hold the mutex on fnpp because I created it, and
	 * I'm already holding the writers lock for it's parent
	 * directory, therefore nobody can reference it without first
	 * I releasing the writers lock.
	 */
	(*fnpp)->fn_offset = offset;
	(*fnpp)->fn_next = *spp;
	*spp = *fnpp;
	(*fnpp)->fn_parent = dfnp;
	(*fnpp)->fn_linkcnt++;	/* parent now holds reference to entry */
	(*fnpp)->fn_size++;

	/*
	 * dfnp->fn_linkcnt and dfnp->fn_size protected by dfnp->rw_lock
	 */
	dfnp->fn_linkcnt++;	/* child now holds reference to parent '..' */
	dfnp->fn_size++;

	dfnp->fn_ref_time = hrestime.tv_sec;
	VN_HOLD(fntovn(*fnpp));		/* return an VN_HELD fnnode */

	AUTOFS_DPRINT((5, "*fnpp=%x\n", *fnpp));
	return (0);
}

int
auto_search(
	fnnode_t *dfnp,
	char *name,
	fnnode_t **fnpp,
	cred_t *cred
)
{
	vnode_t *dvp;
	fnnode_t *p;
	int error = ENOENT, match = 0;

	AUTOFS_DPRINT((4, "auto_search: dfnp=%x, name=%s... ", dfnp, name));

	dvp = fntovn(dfnp);
	if (dvp->v_type != VDIR)
		cmn_err(CE_PANIC, "auto_search: dvp=%x not a directory\n", dvp);

	ASSERT(RW_LOCK_HELD(&dfnp->fn_rwlock));
	for (p = dfnp->fn_dirents; p != NULL; p = p->fn_next) {
		if (strcmp(p->fn_name, name) == 0) {
			if (p->fn_flags & MF_THISUID_MATCH_RQD) {
				/*
				 * "thisuser" kind of node
				 * Need to match CREDs as well
				 */
				match = crcmp(p->fn_cred, cred) == 0;
			} else {
				/*
				 * No need to check CRED
				 */
				match = 1;
			}
		}
		if (match) {
			error = 0;
			if (fnpp) {
				*fnpp = p;
				VN_HOLD(fntovn(*fnpp));
			}
			break;
		}
	}

	AUTOFS_DPRINT((5, "auto_search: error=%d\n", error));
	return (error);
}

/*
 * If dvp is mounted on, get path's vnode in the mounted on filesystem,
 * Path is relative to dvp, ie "./path"
 * If successful, *mvp points to a the held mountpoint vnode.
 */
static int
auto_getmntpnt(
	vnode_t *dvp,
	char *path,
	vnode_t **mvpp,		/* vnode for mountpoint */
	cred_t *cred
)
{
	int error = 0;
	vnode_t *newvp;
	char namebuf[TYPICALMAXPATHLEN + 4];
	struct pathname lookpn;

#ifdef lint
	cred = cred;
#endif

	AUTOFS_DPRINT((4, "auto_getmntpnt: path=%s\n", path));

	ASSERT(dvp->v_vfsmountedhere);
	/*
	 * We better be mounted on, otherwise we shouldn't have
	 * end up in here in the first place.
	 * Since mounted on, lookup "path" in the new filesystem,
	 * it is important that we do the filesystem jump here to
	 * avoid lookuppn() calling auto_lookup on dvp and deadlock.
	 */
	if (error = VFS_ROOT(dvp->v_vfsmountedhere, &newvp))
		goto done;

	/*
	 * Now create the pathname struct so we can make use of lookuppnvp,
	 * and pn_getcomponent.
	 * This code is similar to lookupname() in fs/lookup.c.
	 */
	lookpn.pn_buf = namebuf;
	lookpn.pn_path = namebuf;
	lookpn.pn_pathlen = 0;
	lookpn.pn_bufsize = TYPICALMAXPATHLEN;

	error = copystr(path, namebuf, TYPICALMAXPATHLEN, &lookpn.pn_pathlen);
	lookpn.pn_pathlen--;		/* don't count the null byte */
	if (error == 0) {
		error = lookuppnvp(&lookpn, NO_FOLLOW, NULLVPP,
			mvpp, rootdir, newvp, cred);
	}
	if (error == ENAMETOOLONG) {
		/*
		 * This thread used a pathname > TYPICALMAXPATHLEN
		 * bytes long.
		 */
		if ((error = pn_get(path, UIO_SYSSPACE, &lookpn)) == 0) {
			error = lookuppnvp(&lookpn, NO_FOLLOW, NULLVPP,
				mvpp, rootdir, newvp, cred);
			pn_free(&lookpn);
		}
	}

	/*
	 * newvp is VN_RELE'd by lookuppnvp
	 */

done:	AUTOFS_DPRINT((5, "auto_getmntpnt: path=%s *mvpp=%x error=%d\n",
		path, *mvpp, error));
	return (error);
}

#define	DEEPER(x) (((x)->fn_dirents != NULL) || \
			(fntovn((x))->v_vfsmountedhere != NULL))

/*
 * The caller, should have already VN_RELE'd its reference to the
 * root vnode of this filesystem.
 */
static int
auto_inkernel_unmount(
	vfs_t *vfsp
)
{
	vnode_t *cvp = vfsp->vfs_vnodecovered;
	int error;

	AUTOFS_DPRINT((4,
		"auto_inkernel_unmount: devid=%x mntpnt(%x) count %d\n",
		vfsp->vfs_dev, cvp, cvp->v_count));

	ASSERT(cvp->v_flag & VVFSLOCK);

	/*
	 * Perform the unmount
	 * The mountpoint has already been locked by the caller.
	 */
	error = dounmount(vfsp, kcred);

	AUTOFS_DPRINT((5, "auto_inkernel_unmount: exit count %d\n",
		cvp->v_count));
	return (error);
}

/*
 * unmounts trigger nodes in the kernel and then requests daemon to
 * perform the postunmount operations.
 */
static void
unmount_triggers(
	fnnode_t *fnp,
	action_list **alp
)
{
	fnnode_t *tp, *next;
	int error = 0;
	fninfo_t *vfsip;
	vfs_t *vfsp;
	vnode_t *tvp;
	postumntreq *p = NULL, *p1 = NULL;
	postumntreq *ul = NULL;

	AUTOFS_DPRINT((4, "unmount_triggers: fnp=%x\n", fnp));
	ASSERT(RW_WRITE_HELD(&fnp->fn_rwlock));

	*alp = fnp->fn_alp;
	next = fnp->fn_trigger;
	while ((tp = next) != NULL) {
		tvp = fntovn(tp);
		ASSERT(tvp->v_count >= 2);
		next = tp->fn_next;
		/*
		 * drop writer's lock since the unmount will end up
		 * disconnecting this node from fnp and needs to acquire
		 * the writer's lock again.
		 * next has at least a reference count > 2 since it's
		 * a trigger node, therefore can not be accidentally freed
		 * by a VN_RELE
		 */
		rw_exit(&fnp->fn_rwlock);

		vfsip = vfstofni(fntovn(fnp->fn_parent)->v_vfsp);
		vfsp = tvp->v_vfsp;

		/*
		 * Prepare postunmount information
		 */
		p1 = p;
		p = (postumntreq *)kmem_zalloc(sizeof (*p), KM_SLEEP);
		p->devid = vfsp->vfs_dev;
		if (ul == NULL)
			ul = p;
		else
			p1->next = p;

		/*
		 * Its parent was holding a reference to it, since this
		 * is a trigger vnode.
		 */
		VN_RELE(tvp);
		if (error = auto_inkernel_unmount(vfsp)) {
			cmn_err(CE_PANIC,
			"unmount_triggers: unmount of vp=%x failed error=%d\n",
			tvp, error);
		}
		/*
		 * reacquire writer's lock
		 */
		rw_enter(&fnp->fn_rwlock, RW_WRITER);
	}

	if (error = auto_postunmount_request(vfsip, ul, kcred, TRUE))
		auto_log(CE_WARN,
		    "unmount_triggers: postunmount failed fnp=%x error=%d\n",
		    fnp, error);
	/*
	 * Free the post unmount list
	 */
	p1 = ul->next;
	while ((p = p1) != NULL) {
		p1 = p->next;
		kmem_free(p, sizeof (*p));
	}

	fnp->fn_trigger = NULL;
	fnp->fn_alp = NULL;

	AUTOFS_DPRINT((5, "unmount_triggers: finished\n"));
}

/*
 * This routine locks the mountpoint of every trigger node if they're
 * not busy, or returns EBUSY if any node is busy. If a trigger node should
 * be unmounted first, then it sets nfnp to point to it, otherwise nfnp
 * points to NULL.
 */
static int
triggers_busy(
	fnnode_t *fnp,
	fnnode_t **nfnp
)
{
	int error = 0, done;
	fnnode_t *tp, *t1p;
	vfs_t *vfsp;

	ASSERT(RW_WRITE_HELD(&fnp->fn_rwlock));

	*nfnp = NULL;
	for (tp = fnp->fn_trigger; tp; tp = tp->fn_next) {
		AUTOFS_DPRINT((10, "\ttrigger: %s\n", tp->fn_name));
		vfsp = fntovn(tp)->v_vfsp;
		/*
		 * The vn_vfsunlock will be done in auto_inkernel_unmount.
		 */
		error = vn_vfslock(vfsp->vfs_vnodecovered);
		if (error == 0) {
			mutex_enter(&tp->fn_lock);
			ASSERT((tp->fn_flags & MF_LOOKUP) == 0);
			if (tp->fn_flags & MF_INPROG) {
				/*
				 * a mount is in progress
				 */
				error = EBUSY;
			}
			mutex_exit(&tp->fn_lock);
		}
		if (error || DEEPER(tp) ||
		    ((fntovn(tp))->v_count) > 2) {
			/*
			 * couldn't lock it because it's busy,
			 * It is mounted on or has dirents?
			 * If reference count is greater than two, then
			 * somebody else is holding a reference to this vnode.
			 * One reference is for the mountpoint, and the second
			 * is for the trigger node.
			 */
			AUTOFS_DPRINT((10, "\ttrigger busy\n"));
			if (error == 0) {
				*nfnp = tp;
				/*
				 * The matching VN_RELE is done in
				 * unmount_tree().
				 */
				VN_HOLD(fntovn(*nfnp));
			}
			/*
			 * Unlock previously locked mountpoints
			 */
			for (done = 0, t1p = fnp->fn_trigger; !done;
			    t1p = t1p->fn_next) {
				vfsp = fntovn(t1p)->v_vfsp;
				vn_vfsunlock(vfsp->vfs_vnodecovered);
				done = (t1p == tp);
			}
			error = EBUSY;
			break;
		}
	}

	AUTOFS_DPRINT((4, "triggers_busy: error=%d\n", error));
	return (error);
}

/*
 * Unlock previously locked trigger nodes.
 */
static int
triggers_unlock(
	fnnode_t *fnp
)
{
	fnnode_t *tp;
	vfs_t *vfsp;

	ASSERT(RW_WRITE_HELD(&fnp->fn_rwlock));

	for (tp = fnp->fn_trigger; tp; tp = tp->fn_next) {
		AUTOFS_DPRINT((10, "\tunlock trigger: %s\n", tp->fn_name));
		vfsp = fntovn(tp)->v_vfsp;
		vn_vfsunlock(vfsp->vfs_vnodecovered);
	}

	return (0);
}

static int
unmount_node(
	vfs_t *vfsp
)
{
	int error = 0;
	vnode_t *root_vp;
	vnode_t *cvp;
	fnnode_t *cfnp;
	umntrequest ul;
	fninfo_t *fnip;
	postumntreq pul;
	struct vattr vattr;

	AUTOFS_DPRINT((4, "\tunmount_node vfsp=%x\n", vfsp));

	cvp = vfsp->vfs_vnodecovered;
	cfnp = vntofn(cvp);

	if ((error = VFS_ROOT(vfsp, &root_vp)) == 0) {
		vattr.va_mask = AT_FSID | AT_RDEV;
		if (error = VOP_GETATTR(root_vp, &vattr, 0, kcred)) {
			auto_log(CE_WARN,
			    "unmount_node: VOP_GETATTR(%x) failed\n", root_vp);
		}
		VN_RELE(root_vp);
	} else {
		auto_log(CE_WARN,
			"unmount_node: couldn't get rootvp vfs=%x\n", vfsp);
	}

	if (error)
		goto done;

	if (cfnp->fn_flags & MF_IK_MOUNT) {
		/*
		 * Mount was performed in the kernel,
		 * do an in-kernel unmount.
		 */
		if ((error = vn_vfslock(cvp)) == 0) {
			/*
			 * Prepare post unmount information
			 */
			fnip = vfstofni((fntovn(cfnp->fn_parent))->v_vfsp);
			pul.devid = vattr.va_fsid;
			pul.rdevid = vattr.va_rdev;
			pul.next = NULL;

			/*
			 * perform the unmount
			 */
			if ((error = auto_inkernel_unmount(vfsp)) == 0) {
				error = auto_postunmount_request(
				    fnip, &pul, kcred, TRUE);
				if (error) {
					/*
					 * retry until daemon comes back up.
					 * if daemon is up, and other kind
					 * of error happened, then panic.
					 */
					auto_log(CE_WARN,
			"unmount_node: postunmount failed dev=%x error=%d\n",
						pul.devid, error);
					error = 0;
				}
			}
		}
	} else {
		/*
		 * Ask the daemon to unmount it.
		 */
		fnip = vfstofni(cvp->v_vfsp);
		ul.devid = vattr.va_fsid;
		ul.rdevid = vattr.va_rdev;
		ul.isdirect = fnip->fi_flags & MF_DIRECT ? TRUE : FALSE;
		ul.next = NULL;
		/*
		 * XXX
		 * Should be ok to do this as a "soft" request
		 * and fail the unmount if the connection could
		 * not  be established.
		 */
		error = auto_send_unmount_request(fnip, &ul, kcred, TRUE);
	}

done:	AUTOFS_DPRINT((5, "\tunmount_node vfsp=%x error=%d\n", vfsp, error));
	return (error);
}

/*
 * vp is the "root" of the AUTOFS filesystem.
 * return EBUSY if any thread is holding a reference to this vnode
 * other than us.
 */
static int
check_auto_node(
	vnode_t *vp
)
{
	fnnode_t *fnp;
	int error = 0;
	/*
	 * number of references to expect for
	 * a non-busy vnode.
	 */
	int count;

	AUTOFS_DPRINT((4, "\tcheck_auto_node vp=%x ", vp));
	fnp = vntofn(vp);
	ASSERT(fnp->fn_flags & MF_INPROG);
	ASSERT((fnp->fn_flags & MF_LOOKUP) == 0);

	count = 1;		/* we are holding a reference to vp */
	if (fnp->fn_flags & MF_TRIGGER) {
		/*
		 * parent holds a pointer to us (trigger)
		 */
		count++;
	}
	mutex_enter(&vp->v_lock);
	if (vp->v_flag & VROOT)
		count++;
	ASSERT(vp->v_count > 0);
	AUTOFS_DPRINT((10, "\tcount=%d ", vp->v_count));
	if (vp->v_count > count)
		error = EBUSY;
	mutex_exit(&vp->v_lock);

	AUTOFS_DPRINT((5, "\tcheck_auto_node error=%d ", error));
	return (error);
}

/*
 * rootvp is the root of the AUTOFS filesystem.
 * If rootvp is busy (v_count > 1) returns EBUSY.
 * else removes every vnode under this tree.
 * ASSUMPTION: Assumes that the only node which can be busy is
 * the root vnode. This filesystem better be two levels deep only,
 * the root and its immediate subdirs.
 * The daemon will "AUTOFS direct-mount" only one level below the root.
 */
static int
unmount_autofs(
	vnode_t *rootvp
)
{
	fnnode_t *fnp, *rootfnp, *nfnp;
	int error;

	AUTOFS_DPRINT((4, "\tunmount_autofs rootvp=%x ", rootvp));

	error = check_auto_node(rootvp);
	if (error == 0) {
		/*
		 * Remove all its immediate subdirectories.
		 */
		rootfnp = vntofn(rootvp);
		rw_enter(&rootfnp->fn_rwlock, RW_WRITER);
		nfnp = NULL;	/* lint clean */
		for (fnp = rootfnp->fn_dirents; fnp != NULL; fnp = nfnp) {
			ASSERT(fntovn(fnp)->v_count == 0);
			ASSERT(fnp->fn_dirents == NULL);
			ASSERT(fnp->fn_linkcnt == 2);
			fnp->fn_linkcnt--;
			auto_disconnect(rootfnp, fnp);
			nfnp = fnp->fn_next;
			auto_freefnnode(fnp);
		}
		rw_exit(&rootfnp->fn_rwlock);
	}
	AUTOFS_DPRINT((5, "\tunmount_autofs error=%d ", error));
	return (error);
}

/*
 * max number of unmount threads running
 */
static int autofs_unmount_threads = 5;

/*
 * protects autofs_unmount_threads
 */
static kmutex_t autofs_unmount_threads_lock;

static void
unmount_tree(void)
{
	vnode_t *vp, *newvp;
	fnnode_t *fnp, *nfnp, *pfnp;
	action_list *alp;
	int error, ilocked_it = 0;
	fninfo_t *fnip;
	long ref_time;
	int autofs_busy_root, unmount_as_unit;

	/*
	 * Got to release lock before attempting unmount in case
	 * it hangs.
	 */
	mutex_enter(&fnnode_list_lock);
	if ((fnp = fnnode_list) == NULL) {
		ASSERT(fnnode_cnt == 0);
		/*
		 * no autofs mounted, done.
		 */
		mutex_exit(&fnnode_list_lock);
		goto done;
	}
	VN_HOLD(fntovn(fnp));
	mutex_exit(&fnnode_list_lock);

	vp = fntovn(fnp);
	fnip = vfstofni(vp->v_vfsp);
	if (auto_null_request(fnip, kcred, FALSE) != 0) {
		/*
		 * automountd not running,
		 * don't attempt unmounting this round.
		 */
		VN_RELE(vp);
		goto done;
	}

	/* reference time for this unmount round */
	ref_time = hrestime.tv_sec;

	AUTOFS_DPRINT((4, "unmount_tree (ID=%ld)\n", ref_time));

top:	AUTOFS_DPRINT((10, "unmount_tree: %s\n", fnp->fn_name));
	ASSERT(fnp);
	vp = fntovn(fnp);
	if (vp->v_type == VLNK) {
		/*
		 * can't unmount symbolic links
		 */
		goto next;
	}
	fnip = vfstofni(vp->v_vfsp);
	ASSERT(vp->v_count > 0);
	error = 0;
	autofs_busy_root = unmount_as_unit = 0;
	alp = NULL;

	ilocked_it = 0;
	mutex_enter(&fnp->fn_lock);
	if (fnp->fn_flags & (MF_INPROG | MF_LOOKUP)) {
		/*
		 * Either a mount, lookup or another unmount of this
		 * subtree is in progress, don't attempt to unmount at
		 * this time.
		 */
		mutex_exit(&fnp->fn_lock);
		error = EBUSY;
		goto next;
	}
	if (fnp->fn_unmount_ref_time >= ref_time) {
		/*
		 * Already been here, try next node.
		 */
		mutex_exit(&fnp->fn_lock);
		error = EBUSY;
		goto next;
	}
	fnp->fn_unmount_ref_time = ref_time;

	if (fnp->fn_ref_time + fnip->fi_mount_to > hrestime.tv_sec) {
		/*
		 * Node has been referenced recently, try the
		 * unmount of its children if any.
		 */
		mutex_exit(&fnp->fn_lock);
AUTOFS_DPRINT((10, "fn_ref_time within range\n"));
		rw_enter(&fnp->fn_rwlock, RW_READER);
		if (fnp->fn_dirents) {
			/*
			 * Has subdirectory, attempt their
			 * unmount first
			 */
			nfnp = fnp->fn_dirents;
			VN_HOLD(fntovn(nfnp));
			rw_exit(&fnp->fn_rwlock);

			VN_RELE(vp);
			fnp = nfnp;
			goto top;
		}
		rw_exit(&fnp->fn_rwlock);
		/*
		 * No children, try next node.
		 */
		error = EBUSY;
		goto next;
	}

	AUTOFS_BLOCK_OTHERS(fnp, MF_INPROG);
	mutex_exit(&fnp->fn_lock);
	ilocked_it = 1;

	rw_enter(&fnp->fn_rwlock, RW_WRITER);
	if (fnp->fn_trigger) {
		unmount_as_unit = 1;
		if ((vp->v_vfsmountedhere == NULL) && (check_auto_node(vp))) {
			/*
			 * AUTOFS mountpoint is busy, there's
			 * no point trying to unmount. Fall through
			 * to attempt to unmount subtrees rooted
			 * at a possible trigger node, but remember
			 * not to unmount this tree.
			 */
			autofs_busy_root = 1;
		}

		if (triggers_busy(fnp, &nfnp)) {
			rw_exit(&fnp->fn_rwlock);
			if (nfnp == NULL) {
				error = EBUSY;
				goto next;
			}
			/*
			 * nfnp is busy, try to unmount it first
			 */
			AUTOFS_UNBLOCK_OTHERS(fnp, MF_INPROG);
			VN_RELE(vp);
			ASSERT(fntovn(nfnp)->v_count > 1);
			fnp = nfnp;
			goto top;
		}

		/*
		 * At this point, we know all trigger nodes are locked,
		 * and they're not busy or mounted on.
		 */

		if (autofs_busy_root) {
			/*
			 * Got to unlock the the trigger nodes since
			 * I'm not really going to unmount the filesystem.
			 */
			(void) triggers_unlock(fnp);
		} else {
			/*
			 * Attempt to unmount all the trigger nodes,
			 * save the action_list in case we need to
			 * remount them later. The action_list will be XDR
			 * freed later if there was no need to remount the
			 * trigger nodes.
			 */
			unmount_triggers(fnp, &alp);
		}
	}
	rw_exit(&fnp->fn_rwlock);

	if (autofs_busy_root)
		goto next;

	if (vp->v_vfsmountedhere != NULL) {
		/*
		 * Node is mounted on.
		 */
		AUTOFS_DPRINT((10, "\tNode is mounted on\n"));

		/*
		 * Deal with /xfn/host/jurassic alikes here...
		 */
		if (vp->v_vfsmountedhere->vfs_op == vp->v_vfsp->vfs_op) {
			/*
			 * If the filesystem mounted here is AUTOFS, and it
			 * is busy, try to unmount the tree rooted on it
			 * first.
			 */
			AUTOFS_DPRINT((10, "\t\tAUTOFS mounted here\n"));
			if (VFS_ROOT(vp->v_vfsmountedhere, &newvp))
				cmn_err(CE_PANIC,
				    "unmount_tree: VFS_ROOT(vfs=%x) failed\n",
				    vp->v_vfsmountedhere);
			nfnp = vntofn(newvp);
			if (DEEPER(nfnp)) {
				AUTOFS_UNBLOCK_OTHERS(fnp, MF_INPROG);
				VN_RELE(vp);
				fnp = nfnp;
				goto top;
			}
			/*
			 * Fall through to unmount this filesystem
			 */
			VN_RELE(newvp);
		}

		error = unmount_node(vp->v_vfsmountedhere);
	} else {
		AUTOFS_DPRINT((10, "\tNode is AUTOFS\n"));
		if (unmount_as_unit) {
			AUTOFS_DPRINT((10, "\tunmount as unit\n"));
			error = unmount_autofs(vp);
		} else {
			AUTOFS_DPRINT((10, "\tunmount one at a time\n"));
			rw_enter(&fnp->fn_rwlock, RW_READER);
			if (fnp->fn_dirents) {
				/*
				 * Has subdirectory, attempt their
				 * unmount first
				 */
				nfnp = fnp->fn_dirents;
				VN_HOLD(fntovn(nfnp));
				rw_exit(&fnp->fn_rwlock);

				AUTOFS_UNBLOCK_OTHERS(fnp, MF_INPROG);
				VN_RELE(vp);
				fnp = nfnp;
				goto top;
			}
			rw_exit(&fnp->fn_rwlock);
			goto next;
		}
	}

	if (error) {
		AUTOFS_DPRINT((10, "\tUnmount failed\n"));
		if (alp != NULL) {
			/*
			 * Unmount failed, got to remount triggers.
			 */
			ASSERT((fnp->fn_flags & MF_THISUID_MATCH_RQD) == 0);
			error = auto_perform_actions(fnip, fnp, alp, kcred);
			if (error) {
				auto_log(CE_WARN,
			    "autofs: can't remount triggers fnp=%x error=%d\n",
					fnp, error);
				error = 0;
				/*
				 * The action list should have been
				 * xdr_free'd by auto_perform_actions
				 * since an error occured
				 */
				alp = NULL;
			}
		}
	} else {
		/*
		 * Other threads may be waiting for this unmount to
		 * finish. We must let it know that in order to
		 * proceed, it must trigger the mount itself.
		 */
		mutex_enter(&fnp->fn_lock);
		fnp->fn_flags &= ~MF_IK_MOUNT;
		if (fnp->fn_flags & MF_WAITING)
			fnp->fn_error = EAGAIN;
		mutex_exit(&fnp->fn_lock);
		/*
		 * The unmount succeeded, which will cause this node to
		 * be removed from its parent if its an indirect mount,
		 * therefore update the parent's atime and mtime now.
		 * I don't update them in auto_disconnect() because I
		 * don't want atime and mtime changing every time a
		 * lookup goes to the daemon and creates a new node.
		 */
		if ((fnip->fi_flags & MF_DIRECT) == 0)
			fnp->fn_parent->fn_atime =
			    fnp->fn_parent->fn_mtime = hrestime;

		/*
		 * Free the action list here
		 */
		if (alp != NULL) {
			xdr_free(xdr_action_list, (char *)alp);
			alp = NULL;
		}
	}

	fnp->fn_ref_time = hrestime.tv_sec;

next:
	/*
	 * Obtain parent's readers lock before grabbing
	 * reference to next sibling.
	 * XXX Note that nodes in the top level list (mounted
	 * in user space not by the daemon in the kernel) parent is itself,
	 * therefore grabbing the lock makes no sense, but doesn't
	 * hurt either.
	 */
	pfnp = fnp->fn_parent;
	ASSERT(pfnp != NULL);
	rw_enter(&pfnp->fn_rwlock, RW_READER);
	if ((nfnp = fnp->fn_next) != NULL)
		VN_HOLD(fntovn(nfnp));
	rw_exit(&pfnp->fn_rwlock);

	if (ilocked_it)
		AUTOFS_UNBLOCK_OTHERS(fnp, MF_INPROG);

	if (nfnp != NULL) {
		VN_RELE(vp);
		fnp = nfnp;
		/*
		 * Unmount next element
		 */
		goto top;
	}
	if (pfnp != fnp) {
		/*
		 * Now attempt to unmount my parent
		 */
		VN_HOLD(fntovn(pfnp));
		VN_RELE(vp);
		fnp = pfnp;

		goto top;
	}

	VN_RELE(vp);

	/*
	 * At this point we've walked the entire tree and attempted to unmount
	 * as much as we can one level at a time.
	 */
done:
	mutex_enter(&autofs_unmount_threads_lock);
	autofs_unmount_threads++;
	mutex_exit(&autofs_unmount_threads_lock);

	AUTOFS_DPRINT((5, "unmount_tree done. Thread exiting.\n"));
	thread_exit();
	/* NOTREACHED */
}

static int autofs_unmount_thread_timer = 120;	/* in seconds */

void
auto_do_unmount(void)
{
	mutex_init(&autofs_unmount_threads_lock, "autofs list lock",
		MUTEX_DEFAULT, NULL);
	for (;;) {	/* forever */
		delay(autofs_unmount_thread_timer * hz);
		mutex_enter(&autofs_unmount_threads_lock);
		if (autofs_unmount_threads) {
			autofs_unmount_threads--;
			mutex_exit(&autofs_unmount_threads_lock);

			if (thread_create(NULL, NULL, unmount_tree, NULL,
				0, &p0, TS_RUN, 60) == NULL) {
				auto_log(CE_WARN,
				    "autofs: unmount thread create failure\n");

				mutex_enter(&autofs_unmount_threads_lock);
				autofs_unmount_threads++;
				mutex_exit(&autofs_unmount_threads_lock);
			}
		} else
			mutex_exit(&autofs_unmount_threads_lock);
	}
	/* NOTREACHED */
}

/*
 * used to log warnings only if automountd is running
 * with verbose mode set
 */

/*PRINTFLIKE2*/
void
auto_log(int level, char *fmt, ...)
{
	va_list args;

	if (autofs_verbose > 0) {
		va_start(args, fmt);
		vcmn_err(level, fmt, args);
		va_end(args);
	}
}

#ifdef DEBUG
static int autofs_debug = 0;

/*
 * Utilities used by both client and server
 * Standard levels:
 * 0) no debugging
 * 1) hard failures
 * 2) soft failures
 * 3) current test software
 * 4) main procedure entry points
 * 5) main procedure exit points
 * 6) utility procedure entry points
 * 7) utility procedure exit points
 * 8) obscure procedure entry points
 * 9) obscure procedure exit points
 * 10) random stuff
 * 11) all <= 1
 * 12) all <= 2
 * 13) all <= 3
 * ...
 */

/*PRINTFLIKE2*/
void
auto_dprint(int level, char *fmt, ...)
{
	va_list args;

	if (autofs_debug == level ||
	    (autofs_debug > 10 && (autofs_debug - 10) >= level)) {
		va_start(args, fmt);
		(void) vprintf(fmt, args);
		va_end(args);
	}
}
#endif /* DEBUG */
