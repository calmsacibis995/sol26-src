/*
 * Copyright 1991 NCR Corporation - Dayton, Ohio, USA
 *
 *	Copyright (c) 1994 - 1996 Sun Microsystems, Inc.
 *	All rights reserved.
 */

#pragma ident   "@(#)lm_nlm4_server.c 1.39     96/06/25 SMI"

/*
 * These are the interface routines for the server side of the Lock Manager
 * which implements version 4 of the NLM protocol.
 *
 * N.B. There are aspects of the lock manager implemented here that are
 *	not specific to the particular version of the NLM protocol.  The
 *	generic lock manager code found in this file is duplicated in
 *	lm_nlm_server.c.  Be careful to keep these file consistent.
 */
#include <sys/types.h>
#include <sys/cred.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <sys/share.h>
#include <sys/kmem.h>
#include <sys/netconfig.h>
#include <sys/proc.h>
#include <sys/stream.h>
#include <sys/systm.h>
#include <sys/strsubr.h>

#include <sys/sysmacros.h>
#include <sys/user.h>
#include <sys/utsname.h>
#include <sys/vfs.h>
#include <sys/debug.h>
#include <sys/pathname.h>
#include <sys/callb.h>
#include <rpc/rpc.h>
#include <rpcsvc/sm_inter.h>
#include <nfs/nfs.h>
#include <nfs/nfssys.h>
#include <nfs/nfs_clnt.h>
#include <nfs/export.h>
#include <nfs/rnode.h>
#include <nfs/lm.h>
#include <nfs/lm_server.h>

#include <sys/cmn_err.h>
#include <sys/mutex.h>

/*
 * If the l_len field of the alock structure is zero or has
 * all its bits set to 1 then the lock is to extend "to eof"
 */
#define	NLM_TO_EOF	0
#define	NLM_ALT_TO_EOF	0xffffffffffffffff
#define	NLM_IS_TO_EOF(l)	((l) == NLM_TO_EOF || (l) == NLM_ALT_TO_EOF)

/*
 * Static function prototypes.
 */
static struct lm_vnode *lm_get_vnode(netobj *fh);
static vnode_t *lm_nfs3_fhtovp(nfs_fh3 *fh);
static void lm_block_lock(void *, void *, struct lm_nlm_disp *,
		struct lm_sysid *, u_int);
static void lm_notify_granted(struct lm_nlm_disp *, lm_block_t *,
		struct lm_sysid *, nlm4_lockargs *);
static void lm_send_reply(struct lm_sysid *, struct lm_nlm_disp *, nlm4_res *);
static callb_cpr_t *lm_block_callback(void *);
static enum nlm4_stats lm_alk2flk(struct flock64 *, struct nlm4_lock *,
		int type);
static void lm_null(void *, void *, struct lm_nlm_disp *,
		struct lm_sysid *, u_int);
static void lm_test(void *, void *, struct lm_nlm_disp *,
		struct lm_sysid *, u_int);
static void lm_lock(void *, void *, struct lm_nlm_disp *,
		struct lm_sysid *, u_int);
static void lm_cancel(void *, void *, struct lm_nlm_disp *,
		struct lm_sysid *, u_int);
static void lm_unlock(void *, void *, struct lm_nlm_disp *,
		struct lm_sysid *, u_int);
static void lm_granted(void *, void *, struct lm_nlm_disp *,
		struct lm_sysid *, u_int);
static void lm_granted_res(void *, void *, struct lm_nlm_disp *,
		struct lm_sysid *, u_int);
static void lm_share(void *, void *, struct lm_nlm_disp *,
		struct lm_sysid *, u_int);
static void lm_unshare(void *, void *, struct lm_nlm_disp *,
		struct lm_sysid *, u_int);
static void lm_free_all(void *, void *,  struct lm_nlm_disp *,
		struct lm_sysid *, u_int);

/*
 * Convert an nfs_fh3 into a vnode.
 * Uses the file id (fh_len + fh_data) in the file handle to get the vnode.
 * WARNING: users of this routine must do a VN_RELE on the vnode when they
 * are done with it.
 * BTW: This is just like nfs3_fhtovp() but without the exportinfo arg.
 */
static vnode_t *
lm_nfs3_fhtovp(nfs_fh3 *fh)
{
	register vfs_t *vfsp;
	vnode_t *vp;
	int error;

	if (fh->fh3_length != NFS3_CURFHSIZE)
		return (NULL);

	vfsp = getvfs(&fh->fh3_fsid);
	if (vfsp == NULL)
		return (NULL);

	error = VFS_VGET(vfsp, &vp, (fid_t *)&(fh->fh3_len));
	if (error || vp == NULL)
		return (NULL);

	return (vp);
}

/*
 * Return a lm_vnode containing the vnode indicated by fh.
 * Return NULL if no vnode associated with the fh.
 * Search the lm_vnode list. Create an entry if not found.
 */
static struct lm_vnode *
lm_get_vnode(netobj *fh)
{
	struct lm_vnode *lv, *ret;
	struct lm_vnode *lv_free = NULL;

	if (fh->n_bytes == NULL) {
		cmn_err(CE_WARN, "lockd: received NULL file handle");
		return (NULL);
	}

	/*
	 * Search for the lv with a file handle that matches `fh'.
	 * If we can't find one, use the first free lv on the list.
	 */
	mutex_enter(&lm_vnodes_lock);
	mutex_enter(&lm_lck);

	for (lv = lm_vnodes; lv; lv = lv->next) {
		if (lv->vp) {
			if (bcmp(fh->n_bytes, (caddr_t)&lv->fh3.fh3_u.data,
			    fh->n_len) == 0) {
				break;
			}
		} else {
			ASSERT(lv->count == 0);
			ASSERT(lv->blocked == NULL);
			if (lv_free == NULL) {
				lv_free = lv;
			}
		}
	}

	if (!lv) {
		if (lv_free) {
			lv = lv_free;
		} else {
			mutex_exit(&lm_lck);
			lv = kmem_cache_alloc(lm_vnode_cache, KM_SLEEP);
			mutex_enter(&lm_lck);
			lv->next = lm_vnodes;
			lm_vnodes = lv;
			lm_vnode_len++;
		}
		bzero((caddr_t)&lv->fh2, sizeof (nfs_fhandle));
		lv->fh3.fh3_length = fh->n_len;
		bcopy(fh->n_bytes, (caddr_t)&lv->fh3.fh3_u.data, fh->n_len);
		lv->vp = lm_nfs3_fhtovp(&(lv->fh3));
		lv->count = 0;
		lv->blocked = NULL;
	}

	if (lv->vp) {
		/*
		 * Increment count only if we have a vnode.
		 */
		lv->count++;
	}

	lm_debu7(3, "get_vnode4",
		"cnt= %d, vp= %x, v_cnt= %d, v_flk= %x, sh= %x",
		lv->count,
		(int)lv->vp,
		lv->vp ? lv->vp->v_count : -1,
		lv->vp ? (int)lv->vp->v_filocks : NULL);

	ret = (lv->vp ? lv : NULL);
	mutex_exit(&lm_lck);
	mutex_exit(&lm_vnodes_lock);

	return (ret);
}

/*
 * Handle a blocking lock request.
 */
static void
lm_block_lock(void *gen_args, void *gen_res, struct lm_nlm_disp *disp,
	struct lm_sysid *ls, u_int xid)
{
	nlm4_lockargs *nla = (nlm4_lockargs *)gen_args;
	nlm4_res *nr = (nlm4_res *)gen_res;
	lm_blockinfo_t lbi;
	struct flock64 flk;
	struct lm_vnode *lv;
	int flag = FREAD | FWRITE;
	int error;
	enum nlm4_stats res;
	lm_block_t lmb;
	lm_block_t *first_lmbp;
	bool_t remove_lmb = FALSE;	/* cleanup flag */
	netobj req_id;
	lm_match_t block_match;

	lm_debu6(2, "block_lck4", "exclusive= %u, reclaim= %u, state= %u",
		nla->exclusive, nla->reclaim, nla->state);
	lm_debu4(4, "block_lck4", "xid= %u", xid);
	lm_alock4(2, "block_lck4", &nla->alock);

	/*
	 * Zero out lbi so that the cleanup code at "out" does the right
	 * thing.
	 */
	bzero(&lbi, sizeof (lbi));

	res = lm_alk2flk(&flk, &nla->alock, nla->exclusive ? F_WRLCK : F_RDLCK);
	if (res != NLM4_GRANTED) {
		nr->stat.stat = res;
		lm_send_reply(ls, disp, nr);
		return;
	}
	/*
	 * N.B. A successful lm_get_vnode call allocates an lm_vnode struct
	 * which must be freed before returning
	 */
	if (!(lv = lm_get_vnode(&nla->alock.fh))) {
		nr->stat.stat = NLM4_STALE_FH;
		lm_send_reply(ls, disp, nr);
		return;
	}
	flk.l_sysid = ls->sysid;

	/*
	 * Decide which token will be used to identify this request.
	 * Use the cookie for asynch requests; use the RPC xid for
	 * synchronous requests.
	 */
	if (disp->callback_reply == LM_SENDREPLY) {
		req_id.n_len = sizeof (xid);
		req_id.n_bytes = (char *)&xid;
	} else {
		req_id = nla->cookie;
	}

	mutex_enter(&lm_lck);

	/*
	 * Check whether this request matches or intersects a previous one.
	 * If it does, process this request (and possibly the previous
	 * one) depending on (a) how closely this request matches and (b)
	 * what state the previous request is in.  See the comments for
	 * struct lm_block for more details.
	 */

	block_match = lm_find_block(&flk, lv, &req_id, &first_lmbp);
	switch (block_match) {
	case LMM_NONE:
		/* New request, so register it. */
		lm_init_block(&lmb, &flk, lv, &req_id);
		lm_add_block(&lmb);
		remove_lmb = TRUE;
		break;
	case LMM_PARTIAL:
		/*
		 * Generally this shouldn't happen.  It's either an MT
		 * application, or (more likely) the result of a lost
		 * packet (e.g., after the lock was granted, or if the
		 * client bailed out).  See the comments for struct
		 * lm_block.
		 */
		lm_cancel_granted_rxmit(&flk, lv);
		lm_init_block(&lmb, &flk, lv, &req_id);
		lm_add_block(&lmb);
		remove_lmb = TRUE;
		break;
	case LMM_FULL:
		/* Possible retransmission.  */
		switch (first_lmbp->lmb_state) {
		case LMB_PENDING:
			/*
			 * We don't expect the original request to stay
			 * this way for long, and we don't want to queue
			 * retransmissions, so drop this one on the floor.
			 */
			mutex_exit(&lm_lck);
			goto out;
		case LMB_BLOCKED:
			/*
			 * Tell the client it's still blocked.  It's okay
			 * if this is a new request, as the GRANTED
			 * callback will wake it up.
			 */
			mutex_exit(&lm_lck);
			nr->stat.stat = NLM4_BLOCKED;
			lm_send_reply(ls, disp, nr);
			goto out;
		case LMB_GRANTED:
			/*
			 * Cancel any pending GRANTED callback in case it's
			 * a retransmission, but submit it as a new request
			 * in case it's not.
			 */
			lm_cancel_granted_rxmit(&flk, lv);
			lm_init_block(&lmb, &flk, lv, &req_id);
			lm_add_block(&lmb);
			remove_lmb = TRUE;
			break;
		default:
			cmn_err(CE_PANIC,
				"lm_block_lock4: bad lm_block state (%d)",
				first_lmbp->lmb_state);
			break;
		}
		break;
	case LMM_REXMIT:
		/* Definite retransmission. */
		switch (first_lmbp->lmb_state) {
		case LMB_PENDING:
			/*
			 * We don't expect the original request to stay
			 * this way for long, and we don't want to queue
			 * retransmissions, so drop this one on the floor.
			 */
			mutex_exit(&lm_lck);
			goto out;
		case LMB_BLOCKED:
			/* Tell the client that it's still blocked. */
			mutex_exit(&lm_lck);
			nr->stat.stat = NLM4_BLOCKED;
			lm_send_reply(ls, disp, nr);
			goto out;
		case LMB_GRANTED:
			/*
			 * Cancel any pending GRANTED callback and return
			 * success.  There is a slight chance that the lock
			 * has already been released.  In that case, either
			 * the client will ignore this response, or the
			 * application is MT and probably broken.  We don't
			 * resubmit the request because we might not be
			 * able to reserve the service thread.
			 */
			lm_cancel_granted_rxmit(&flk, lv);
			mutex_exit(&lm_lck);
			nr->stat.stat = NLM4_GRANTED;
			lm_send_reply(ls, disp, nr);
			goto out;
		default:
			cmn_err(CE_PANIC,
				"lm_block_lock4: bad lm_block state (%d)",
				first_lmbp->lmb_state);
			break;
		}
	default:
		cmn_err(CE_PANIC, "lm_block_lock4: bad match code (%d)",
			block_match);
		break;
	}

	mutex_exit(&lm_lck);

	/* first_lmbp is not valid once we drop lm_lck, so nil it out */
	first_lmbp = NULL;

	ASSERT(remove_lmb == TRUE);

	/*
	 * Initialize the result and args information, as well as the
	 * callback information.
	 */
	bzero((caddr_t)nr, sizeof (*nr));
	nr->cookie = nla->cookie;

	lbi.vp = lv->vp;
	lbi.ls = ls;
	lm_ref_sysid(ls);
	lbi.disp = disp;
	lbi.unr.nr4 = nr;
	lbi.blocked = 0;
	lbi.lmbp = &lmb;

	/*
	 * Verify with the RPC system that it's okay for us to block.  If
	 * it's not okay, return a "no resources" error.
	 */
	if (svc_reserve_thread(lm_getxprt()->xprt) == 0) {
		lm_debu3(2, "block_lck4", "can't reserve thread");
#ifdef DEBUG
		cmn_err(CE_NOTE,
"lm_block_lock: nlm4, couldn't reserve svc thread: clnt %s, xport %s, pid %u.",
		ls->name, ls->config.knc_proto, flk.l_pid);
#endif
		nr->stat.stat = NLM4_DENIED_NOLOCKS;
		lm_send_reply(ls, disp, nr);
		goto out;
	}

	/*
	 * the old lock manager doesn't retry blocked requests.  Instead it
	 * relies on notification from the status monitor to reissue a
	 * blocked request after a server crash.  Therefore it is
	 * imperative to notify the status monitor on this server about
	 * the client so things can recover if this server restarts
	 */
	lm_sm_server(ls, lm_get_me());

	/*
	 * We are now ready to try to acquire the lock.  We pass our
	 * callback routine into VOP_FRLOCK along with the local vp.
	 * There are two possible scenarios:
	 *
	 *	A. The lock is available.
	 *		1. We get either:
	 *			a. 0 (acquired the lock);
	 *			b. otherwise (EINTR? anyway, no lock).
	 *		Our callback routine is never called in either case.
	 *		2. We send an NLM4_LOCK_RES or do svc_sendreply *here*
	 *			(usually done up in lm_nlm4_dispatch) with
	 *			granted or denied status as appropriate.
	 *		3. Free resources and return.
	 *
	 *	B. The lock is not currently available.
	 *		1. Our callback routine gets called; from it we
	 *			send an NLM4_LOCK_RES or do svc_sendreply
	 *			(usually done up in lm_nlm4_dispatch) to the
	 *			client with the status `nlm4_blocked.'
	 *		2. Our thread blocks in VOP_FRLOCK waiting for
	 *			the lock.
	 *		3. We get either:
	 *			a. 0 (acquired the lock);
	 *			b. otherwise (EINTR? anyway, no lock).
	 *		4. If we acquired the lock, we send an NLM4_GRANTED
	 *			or NLMPROC4_GRANTED_MSG as appropriate.
	 *		5. Free resources and return.
	 *
	 * lm_nlm4_dispatch does no postprocessing (with client) for us in
	 * either case, which is atypical.  We are expected to do all such
	 * closing-of-the-loop here.
	 */
	l_callback(&flk) = lm_block_callback;
	l_cbp(&flk) = &lbi;

	lm_debu3(2, "block_lck4", "before calling VOP_FRLOCK:");
	lm_debu8(2, "block_lck4",
		"type= %u, start= %llu, len= %llu, pid= %u sysid= %x",
		flk.l_type, flk.l_start, flk.l_len, flk.l_pid, flk.l_sysid);
	lm_debu7(2, "block_lck4",
		"lbi: ls= %x cb= %d nr.stat= %d nr.cookie= %d",
		(int)lbi.ls, lbi.disp->callback_proc, (int)nr->stat.stat,
		(int)(nr->cookie.n_len == sizeof (u_int) ?
			*(u_int *)nr->cookie.n_bytes : 0));

	error = VOP_FRLOCK(lv->vp, F_RSETLKW, &flk, flag, (u_offset_t)0,
	    CRED());
	switch (error) {
	case 0:
		nr->stat.stat = NLM4_GRANTED;
		break;

	case ENOLCK:	/* max no. of segments for system locked */
	case ENOMEM:	/* resource shortfall */
	case EINTR: 	/* remote request was cancelled */
		nr->stat.stat = NLM4_DENIED_NOLOCKS;
		break;

	case EDEADLK:	/* deadlock condition detected */
		nr->stat.stat = NLM4_DEADLCK;
		break;
	case EROFS:
		nr->stat.stat = NLM4_ROFS;
		break;
	case EFBIG:
		nr->stat.stat = NLM4_FBIG;
		break;

	default:
		lm_debu4(2, "block_lck4",
			"unexpected VOP_FRLOCK return= %d", error);
		nr->stat.stat = NLM4_DENIED;
		break;
	}

	/*
	 * If the callback wasn't called, release the thread reservation,
	 * send the status back to the client, and bail out.
	 */
	if (lbi.blocked == 0) {
		svc_unreserve_thread(lm_getxprt()->xprt);
		lm_send_reply(ls, disp, nr);
		goto out;
	}

	if (nr->stat.stat != NLM4_GRANTED) {
		/*
		 * XXX: The NCR code did nothing in this case, i.e. it
		 * quietly dropped the request.  Since the callback promised
		 * the client we would grant when possible, this seems
		 * unwise - should at least retry the local lock request
		 * a reasonable number of times before giving up, depending
		 * on the error(s) and whether we are shutting down.
		 */
		lm_debu4(2, "block_lck4", "VOP_FRLOCK returned %d", error);
		goto out;
	}

	mutex_enter(&lm_lck);
	ASSERT(lmb.lmb_state == LMB_BLOCKED);
	lmb.lmb_state = LMB_GRANTED;
	mutex_exit(&lm_lck);

	lm_notify_granted(disp, &lmb, ls, nla);

	/*
	 * Final cleanup.
	 */
out:
	if (remove_lmb) {
		mutex_enter(&lm_lck);
		lm_remove_block(&lmb);
		mutex_exit(&lm_lck);
	}

	if (lbi.ls != NULL)
		lm_rel_sysid(lbi.ls);
	lm_rel_vnode(lv);
}

/*
 * Notify the client that a blocking lock has been granted.
 */
static void
lm_notify_granted(struct lm_nlm_disp *disp, lm_block_t *lmbp,
		struct lm_sysid *ls, nlm4_lockargs *nla)
{
	struct nlm4_testargs nta;
	nlm4_res callback_res;		/* results from GRANTED callback */
	int cookie;
	int error;

	mutex_enter(&lm_lck);
	cookie = lm_stat.cookie++;
	mutex_exit(&lm_lck);

	/*
	 * Build up the args for the GRANTED message.
	 */
	bzero((caddr_t)&callback_res, sizeof (callback_res));
	nta.cookie.n_len = sizeof (cookie);
	nta.cookie.n_bytes = (char *)&cookie;
	nta.exclusive = nla->exclusive;
	nta.alock.caller_name = utsname.nodename;
	nta.alock.fh.n_len = nla->alock.fh.n_len;
	nta.alock.fh.n_bytes = lm_dup(nla->alock.fh.n_bytes,
					nla->alock.fh.n_len);
	nta.alock.oh.n_len = nla->alock.oh.n_len;
	nta.alock.oh.n_bytes = lm_dup(nla->alock.oh.n_bytes,
					nla->alock.oh.n_len);
	nta.alock.svid = nla->alock.svid;
	nta.alock.l_offset = nla->alock.l_offset;
	nta.alock.l_len = nla->alock.l_len;

	/*
	 * Now we actually call the client.
	 * Note: We do not retransmit within client's grace period,
	 * since there is no point in `granting' anything to a new LM!
	 * (XXX the comment above needs to be reworded.)
	 *
	 * If the call times out, keep retrying until we get some sort of
	 * response.  Never free the lock; if the client thinks it has the
	 * lock but is having problems responding to the GRANTED call, we
	 * could end up with two processes thinking they own the lock,
	 * which could lead to file corruption.
	 *
	 * Note that we don't mark this thread as checkpoint-safe during
	 * the call.  There is too much going on in these routines to do
	 * the CPR calls here, and it's too ugly to pass the CPR
	 * information into these routines for them to use it.  It's
	 * possible this could cause a noticeable delay when trying to
	 * checkpoint a server, but this should never happen in practice.
	 */
	do {
		mutex_enter(&lm_lck);
		if (lmbp->lmb_no_callback) {
			lm_debu4(2, "block_lck4",
				"Cancelling wait: 0x%x",
				lmbp->lmb_no_callback);
			mutex_exit(&lm_lck);
			break;
		}
		mutex_exit(&lm_lck);

		if (disp->do_block_reply == LM_REPLY) {
			if (disp->callback_reply == LM_CALLBACK) {
				error = lm_asynrpc(ls, NLM_PROG, NLM4_VERS,
						NLMPROC4_GRANTED_MSG,
						xdr_nlm4_testargs,
						(caddr_t)&nta, cookie,
						(enum nlm_stats *)
							&callback_res.stat.stat,
						LM_GR_TIMOUT, LM_GR_RETRY);
			} else {
				error = lm_callrpc(ls, NLM_PROG, NLM4_VERS,
						NLMPROC4_GRANTED,
						xdr_nlm4_testargs,
						(caddr_t)&nta, xdr_nlm4_res,
						(caddr_t)&callback_res,
						LM_GR_TIMOUT, LM_GR_RETRY);
			}
		}

		lm_debu5(2, "block_lck4",
			"After NLMPROC4_GRANTED: error= %d, stat= %d",
			error, callback_res.stat.stat);
		if (disp->do_block_reply == LM_SENDREPLY)
			xdr_free(xdr_nlm4_res, (char *)&callback_res);
	} while (error != 0);

	kmem_free(nta.alock.fh.n_bytes, nta.alock.fh.n_len);
	kmem_free(nta.alock.oh.n_bytes, nta.alock.oh.n_len);
}

/*
 * Send the appropriate reply back to the client.
 *
 * If disp->callback_reply is LM_CALLBACK, disp->callback_proc is an
 * NLMPROC4_*RES proc to be called back.
 * Otherwise, we do an svc_sendreply to client's NLMPROC4_* request on `xprt'
 * to close the transaction.
 */
static void
lm_send_reply(struct lm_sysid *ls, struct lm_nlm_disp *disp, nlm4_res *nr)
{
	int error;
	struct lm_xprt *lx;

	lm_debu8(2, "send_reply4",
		"ls= %x cb= %d xdrres= %x nr.stat= %d nr.cookie= %d",
		(int)ls, disp->callback_proc, (int)disp->xdrres,
		(int)nr->stat.stat,
		(int)((nr->cookie.n_len == sizeof (u_int)) ?
		    *(u_int *)nr->cookie.n_bytes : 0));

	if (disp->do_block_reply != LM_REPLY) {
	    return;
	}

	if (disp->callback_reply == LM_CALLBACK) {
		lm_debu4(2, "send_reply4", "doing callback %d\n",
			disp->callback_proc);
		/*
		 * This should stay similar to the call in lm_nlm_dispatch().
		 */
		error = lm_callrpc(ls, NLM_PROG, NLM4_VERS,
				disp->callback_proc,
				disp->xdrres, (caddr_t)nr,
				xdr_void, NULL, LM_NO_TIMOUT, LM_RETRY);
			if (error) {
				lm_debu4(2, "send_reply4",
					"lm_callrpc returned %d", error);
			}
	} else {
		lx = lm_getxprt();
		lm_debu6(2, "send_reply4",
			"doing reply lx= %x xprt= %x thread= %x",
			(int)lx, (int)lx->xprt, (int)lx->thread);
		if (!svc_sendreply(lx->xprt, disp->xdrres, (caddr_t)nr)) {
			lm_debu3(2, "send_reply4", "Bad svc_sendreply");
		}
	}
}

/*
 * This routine is called by the local locking code just before the thread
 * blocks on a lock.
 * - It sends `nlm4_blocked' reply back to client to indicate that the
 *   request has blocked, per the NLM protocol.
 * - It also sets lbip->blocked to 1 so that the code that called into the
 *   local locking code knows that the request had blocked.
 * - It marks the block information for the request, so that
 *   retransmissions don't queue up additional threads.
 * - It detaches itself from the RPC transport handle, so that the
 *   transport can be freed while the thread is blocked.
 * - It passes the CPR information from the RPC system to the local locking
 *   code.  This is so that the thread doesn't block a checkpoint
 *   operation.
 */
static callb_cpr_t *
lm_block_callback(void *argp)
{
	struct nlm4_res *nr;
	lm_blockinfo_t *lbip = (lm_blockinfo_t *)argp;
	vnode_t *vp = lbip->vp;

	lm_debu5(2, "block_cb4", "vp= %x lbip= %x", (int)vp, (int)lbip);
	nr = lbip->unr.nr4;

	lm_debu8(2, "block_cb4",
		"ls= %x cb= %d blocked= %d nr.stat= %d nr.cookie= %d",
		(int)lbip->ls, lbip->disp->callback_proc, lbip->blocked,
		(int)nr->stat.stat,
		(int)((nr->cookie.n_len == sizeof (u_int)) ?
		    *(u_int *)nr->cookie.n_bytes : 0));

	lbip->blocked = 1;
	mutex_enter(&lm_lck);
	lbip->lmbp->lmb_state = LMB_BLOCKED;
	mutex_exit(&lm_lck);
	nr->stat.stat = NLM4_BLOCKED;
	lm_send_reply(lbip->ls, lbip->disp, nr);

	return (svc_detach_thread(lm_getxprt()->xprt));
}

/*
 * The following functions treats the incoming calls.
 * The functions are all called through the lm_nlm4_disp table.
 */

/*
 * Initialize the flock64 structure with data from nlm_lock.
 * Also initialize the l_pid field, this is needed by the
 * deadlock detection done in lm_block_lock().
 *
 * N.B. the caller of this routine is required to fill in
 * the l_sysid field of the flock64 structure.
 */
static enum nlm4_stats
lm_alk2flk(struct flock64 *flk, struct nlm4_lock *alk, int type)
{
	flk->l_type = type;
	flk->l_whence = 0;
	flk->l_start = alk->l_offset;
	if (NLM_IS_TO_EOF(alk->l_len)) {
		/* the lock is to the end of file */
		flk->l_len = 0;
	} else {
		/*
		 * does lock fit within unsigned range?
		 * we're actually testing
		 *   alk->l_offset + alk->l_len - 1 > MAX_U_OFFSET_T
		 * without having the numeric range to do that, and
		 * we know alk->l_len != 0 from NLM_IS_TO_EOF above.
		 */
		if (alk->l_offset > (uint64)MAX_U_OFFSET_T - alk->l_len + 1)
			return (NLM4_FBIG);
		flk->l_len = alk->l_len;
	}
	flk->l_pid = alk->svid;
	/* flk->l_sysid filled in by caller */

	lm_debu7(3, "alk2flk4", "type= %u, start= %llu, len= %llu, pid= %u",
		flk->l_type, flk->l_start, flk->l_len, flk->l_pid);

	return (NLM4_GRANTED);
}

/*
 * Dummy procedure for NULL action.  None of the arguments are used.
 */
/* ARGSUSED */
static void
lm_null(void *argp, void *resp, struct lm_nlm_disp *disp,
	struct lm_sysid *ls, u_int xid)
{
	lm_debu3(2, "null4", "Called");
}

/*
 * Test whether a lock can be set.
 */
/* ARGSUSED */
static void
lm_test(void *gen_args,
	void *gen_res,
	struct lm_nlm_disp *disp,	/* unused */
	struct lm_sysid *ls,
	u_int xid)			/* unused */
{
	nlm4_testargs *nta = (nlm4_testargs *)gen_args;
	nlm4_testres *ntr = (nlm4_testres *)gen_res;
	struct lm_vnode *lv;
	struct flock64	   flk;
	int		   flag   = FREAD | FWRITE;
	enum nlm4_stats	res;

	lm_debu4(2, "test4", "exclusive= %u", nta->exclusive);
	lm_alock4(2, "test", &nta->alock);

	res = lm_alk2flk(&flk, &nta->alock, nta->exclusive ? F_WRLCK : F_RDLCK);
	if (res != NLM4_GRANTED) {
		ntr->stat.stat = res;
		return;
	}
	/*
	 * N.B. A successful lm_get_vnode call allocates an lm_vnode struct
	 * which must be freed before returning
	 */
	if (! (lv = lm_get_vnode(&nta->alock.fh))) {
		ntr->stat.stat = NLM4_STALE_FH;
		return;
	}
	flk.l_sysid = ls->sysid;

	/*
	 * Make the system call and treat error codes.
	 */
	switch (VOP_FRLOCK(lv->vp, F_RGETLK, &flk, flag, (u_offset_t)0,
			CRED())) {
	case 0:
		if (flk.l_type == F_UNLCK) {
			ntr->stat.stat = NLM4_GRANTED;
		} else {
			ntr->stat.stat = NLM4_DENIED;
			ntr->stat.nlm4_testrply_u.holder.exclusive =
				flk.l_type == F_WRLCK;
			ntr->stat.nlm4_testrply_u.holder.svid = flk.l_pid;
			ntr->stat.nlm4_testrply_u.holder.oh.n_len = 0;
			ntr->stat.nlm4_testrply_u.holder.oh.n_bytes = NULL;
			ntr->stat.nlm4_testrply_u.holder.l_offset = flk.l_start;
			ntr->stat.nlm4_testrply_u.holder.l_len = flk.l_len;
		}
		break;

	default:
		ntr->stat.stat = NLM4_DENIED;
	}

	lm_rel_vnode(lv);
	lm_debu8(2, "test4",
	    "End: stat= %u, exclusive= %u, svid= %u, offset= %llu, len= %llu",
	    ntr->stat.stat, ntr->stat.nlm4_testrply_u.holder.exclusive,
	    ntr->stat.nlm4_testrply_u.holder.svid,
	    ntr->stat.nlm4_testrply_u.holder.l_offset,
	    ntr->stat.nlm4_testrply_u.holder.l_len);
}

/*
 * Try to set a non-blocking lock.
 */
/* ARGSUSED */
static void
lm_lock(void *gen_args,
	void *gen_res,
	struct lm_nlm_disp *disp,	/* unused */
	struct lm_sysid *ls,
	u_int xid)			/* unused */
{
	nlm4_lockargs *nla = (nlm4_lockargs *)gen_args;
	nlm4_res *nr = (nlm4_res *)gen_res;
	struct lm_vnode *lv;
	struct flock64 flk;
	int flag = FREAD | FWRITE;
	int error;
	enum nlm4_stats res;

	lm_debu6(2, "lock4", "exclusive= %u, reclaim= %u, state= %u",
		nla->exclusive, nla->reclaim, nla->state);
	lm_alock4(2, "lock", &nla->alock);

	res = lm_alk2flk(&flk, &nla->alock, nla->exclusive ? F_WRLCK : F_RDLCK);
	if (res != NLM4_GRANTED) {
		nr->stat.stat = res;
		return;
	}
	/*
	 * N.B. A successful lm_get_vnode call allocates an lm_vnode struct
	 * which must be freed before returning
	 */
	if (!(lv = lm_get_vnode(&nla->alock.fh))) {
		nr->stat.stat = NLM4_STALE_FH;
		return;
	}
	flk.l_sysid = ls->sysid;

	/*
	 * check to see if a request matching this one is waiting to
	 * retransmit the granted response.  If any are waiting, cancel
	 * them.
	 */
	mutex_enter(&lm_lck);
	lm_cancel_granted_rxmit(&flk, lv);
	mutex_exit(&lm_lck);

	/*
	 * Make the system call and treat error codes.
	 */
	error = VOP_FRLOCK(lv->vp, F_RSETLK, &flk, flag, (u_offset_t)0, CRED());
	switch (error) {
	case 0:
		nr->stat.stat = NLM4_GRANTED;
		break;

	case ENOLCK:
		nr->stat.stat = NLM4_DENIED_NOLOCKS;
		break;

	case EAGAIN:	/* Backward compatibility. */
	case EACCES:
		nr->stat.stat = NLM4_DENIED;
		break;
	case EROFS:
		nr->stat.stat = NLM4_ROFS;
		break;
	case EFBIG:
		nr->stat.stat = NLM4_FBIG;
		break;

	default:
		lm_debu4(2, "lock4", "unexpected VOP_FRLOCK return= %d", error);
		nr->stat.stat = NLM4_DENIED;
	}

	lm_rel_vnode(lv);
	lm_debu4(2, "lock4", "End: stat= %u", nr->stat.stat);
}

/*
 * lm_cancel
 *
 * Kill all processes having (sysid,epid) as me - except of course myself.
 * Always return NLM4_GRANTED.
 */
/* ARGSUSED */
static void
lm_cancel(void *gen_args,
	void *gen_res,
	struct lm_nlm_disp *disp,	/* unused */
	struct lm_sysid *ls,
	u_int xid)			/* unused */
{
	nlm4_cancargs *nca = (nlm4_cancargs *)gen_args;
	nlm4_res *nr = (nlm4_res *)gen_res;
	int flag = FREAD | FWRITE;
	struct lm_vnode *lv;
	struct flock64 flk;
	enum nlm4_stats res;

	lm_debu5(2, "cancel4", "block= %u, exclusive= %u",
			nca->block, nca->exclusive);
	lm_alock4(2, "cancel4", &nca->alock);

	nr->stat.stat = NLM4_GRANTED;
	res = lm_alk2flk(&flk, &nca->alock, F_UNLCK);
	if (res != NLM4_GRANTED) {
		/* nr->stat.stat = res; */
		return;
	}
	/*
	 * N.B. A successful lm_get_vnode call allocates an lm_vnode struct
	 * which must be freed before returning
	 */
	if (!(lv = lm_get_vnode(&nca->alock.fh))) {
		/* nr->stat.stat = NLM4_GRANTED; */
		return;
	}
	flk.l_sysid = ls->sysid;

	/*
	 * check to see if a request matching this one is waiting to
	 * retransmit the granted response.  If any are waiting, cancel
	 * them.
	 */
	mutex_enter(&lm_lck);
	lm_cancel_granted_rxmit(&flk, lv);
	mutex_exit(&lm_lck);

	/*
	 * Call local locking code to cancel (deny) all blockers on
	 *	*exactly* this lock on this vp with identity (sysid,pid).
	 */
	(void) VOP_FRLOCK(lv->vp, F_RSETLK, &flk, flag, (u_offset_t)0, CRED());

	lm_rel_vnode(lv);
	lm_debu4(2, "cancel4", "End: stat= %u", nr->stat.stat);
}

/*
 * lm_unlock
 */
/* ARGSUSED */
static void
lm_unlock(void *gen_args,
	void *gen_res,
	struct lm_nlm_disp *disp,	/* unused */
	struct lm_sysid *ls,
	u_int xid)			/* unused */
{
	nlm4_unlockargs *nua = (nlm4_unlockargs *)gen_args;
	nlm4_res *nr = (nlm4_res *)gen_res;
	struct lm_vnode *lv;
	struct flock64	   flk;
	int		   flag   = FREAD | FWRITE;
	enum nlm4_stats	res;

	lm_alock4(2, "unlock", &nua->alock);

	res = lm_alk2flk(&flk, &nua->alock, F_UNLCK);
	if (res != NLM4_GRANTED) {
		nr->stat.stat = res;
		return;
	}
	/*
	 * N.B. A successful lm_get_vnode call allocates an lm_vnode struct
	 * which must be freed before returning
	 */
	if (!(lv = lm_get_vnode(&nua->alock.fh))) {
		nr->stat.stat = NLM4_STALE_FH;
		return;
	}

	flk.l_sysid = ls->sysid;
	ASSERT(flk.l_sysid != 0);

	/*
	 * check to see if a request matching this one is waiting to
	 * retransmit the granted response.  If any are waiting, cancel
	 * them.
	 */
	mutex_enter(&lm_lck);
	lm_cancel_granted_rxmit(&flk, lv);
	mutex_exit(&lm_lck);

	/*
	 * Call and treat error codes.
	 */
	switch (VOP_FRLOCK(lv->vp, F_RSETLK, &flk, flag, (u_offset_t)0,
	    CRED())) {
	case 0:
		nr->stat.stat = NLM4_GRANTED;
		break;

	case ENOLCK:
		nr->stat.stat = NLM4_DENIED_NOLOCKS;
		break;

	case EROFS:
		nr->stat.stat = NLM4_ROFS;
		break;

	case EFBIG:
		nr->stat.stat = NLM4_FBIG;
		break;

	default:
		nr->stat.stat = NLM4_DENIED;
	}

	lm_rel_vnode(lv);
	lm_debu4(2, "unlock4", "End: stat= %u", nr->stat.stat);
}

/*
 * lm_granted
 */
/* ARGSUSED */
static void
lm_granted(void *gen_args,
	void *gen_res,
	struct lm_nlm_disp *disp,	/* unused */
	struct lm_sysid *ls,		/* unused */
	u_int xid)			/* unused */
{
	nlm4_testargs *nta = (nlm4_testargs *)gen_args;
	nlm4_res *nr = (nlm4_res *)gen_res;

	lm_debu4(2, "granted4", "exclusive= %u", nta->exclusive);
	lm_alock4(2, "granted4", &nta->alock);

	/*
	 * Always return NLM4_GRANTED, even if we can't match the request
	 * with a blocked process.  This call may be a retransmission of a
	 * GRANTED call that did unblock a process.
	 */
	(void) lm_signal_granted(nta->alock.svid, &nta->alock.fh,
			&nta->alock.oh, nta->alock.l_offset, nta->alock.l_len);
	nr->stat.stat = NLM4_GRANTED;

	lm_debu4(2, "granted4", "End: stat= %u", nr->stat.stat);
}

/*
 * lm_granted_res
 */
/* ARGSUSED */
static void
lm_granted_res(void *gen_args,
	void *gen_res,			/* unused */
	struct lm_nlm_disp *disp,	/* unused */
	struct lm_sysid *ls,		/* unused */
	u_int xid)			/* unused */
{
	nlm4_res *nr = (nlm4_res *)gen_args;

	lm_debu5(2, "grant_res4", "cookie= %d, stat= %d",
		(int)((nr->cookie.n_len == sizeof (u_int)) ?
		    *(u_int *)nr->cookie.n_bytes : 0),
		nr->stat.stat);
	/*
	 * Guard against broken servers. Check for a valid cookie,
	 * we could panic otherwise.
	 */
	if ((nr->cookie.n_len >= sizeof (u_int)) &&
	    (nr->cookie.n_bytes != NULL))
		lm_asynrply(*(u_int *)nr->cookie.n_bytes,
		    (enum nlm_stats)nr->stat.stat);

	lm_debu3(2, "grant_res4", "End");
}

/*
 * lm_share
 */
/* ARGSUSED */
static void
lm_share(void *gen_args,
	void *gen_res,
	struct lm_nlm_disp *disp,	/* unused */
	struct lm_sysid *ls,
	u_int xid)			/* unused */
{
	nlm4_shareargs *nsa = (nlm4_shareargs *)gen_args;
	nlm4_shareres *nsr = (nlm4_shareres *)gen_res;
	struct lm_vnode *lv;
	struct shrlock shr;
	int error;
	int flag = FREAD|FWRITE;

	lm_d_nsa4(2, "share", nsa);

	/*
	 * N.B. A successful lm_get_vnode call allocates an lm_vnode struct
	 * which must be freed before returning
	 */
	if (!(lv = lm_get_vnode(&nsa->share.fh))) {
		nsr->stat = NLM4_STALE_FH;
		return;
	}

	/*
	 * There is a simpler mapping of these but it would be much less
	 * readable and maintainable.
	 */
	switch (nsa->share.access) {
	case FSA_NONE:
		shr.access = 0;		/* Shouldn't ever happen */
		break;
	case FSA_R:
		shr.access = F_RDACC;
		break;
	case FSA_W:
		shr.access = F_WRACC;
		break;
	case FSA_RW:
		shr.access = F_RWACC;
		break;
	default:
		shr.access = 0;		/* Shouldn't ever happen */
		break;
	}

	switch (nsa->share.mode) {
	case FSM_DN:
		shr.deny = F_NODNY;
		break;
	case FSM_DR:
		shr.deny = F_RDDNY;
		break;
	case FSM_DW:
		shr.deny = F_WRDNY;
		break;
	case FSM_DRW:
		shr.deny = F_RWDNY;
		break;
	default:
		shr.deny = F_NODNY;		/* Shouldn't ever happen */
		break;
	}

	shr.sysid = ls->sysid;
	shr.pid = 0;
	shr.own_len = nsa->share.oh.n_len;
	shr.owner = nsa->share.oh.n_bytes;

	error = VOP_SHRLOCK(lv->vp, F_SHARE, &shr, flag);
	nsr->stat = error ? NLM4_DENIED : NLM4_GRANTED;

	lm_rel_vnode(lv);
	lm_debu4(2, "share4", "End: stat= %u", nsr->stat);
}

/*
 * lm_unshare
 */
/* ARGSUSED */
static void
lm_unshare(void *gen_args,
	void *gen_res,
	struct lm_nlm_disp *disp,	/* unused */
	struct lm_sysid *ls,
	u_int xid)			/* unused */
{
	nlm4_shareargs *nsa = (nlm4_shareargs *)gen_args;
	nlm4_shareres *nsr = (nlm4_shareres *)gen_res;
	struct lm_vnode *lv;
	struct shrlock shr;
	int flag = FREAD|FWRITE;

	lm_d_nsa4(2, "unshare", nsa);

	/*
	 * N.B. A successful lm_get_vnode call allocates an lm_vnode struct
	 * which must be freed before returning
	 */
	if (!(lv = lm_get_vnode(&nsa->share.fh))) {
		nsr->stat = NLM4_STALE_FH;
		return;
	}

	/*
	 * Access and deny don't matter on unshares
	 */
	shr.access = 0;
	shr.deny = 0;
	shr.sysid = ls->sysid;
	shr.pid = 0;
	shr.own_len = nsa->share.oh.n_len;
	shr.owner = nsa->share.oh.n_bytes;

	(void) VOP_SHRLOCK(lv->vp, F_UNSHARE, &shr, flag);

	/*
	 * The only error returned is EINVAL if it is not found, nlm
	 * has no way to indicate this so force it to success.
	 */
	nsr->stat = NLM4_GRANTED;

	lm_rel_vnode(lv);

	lm_debu4(2, "unshare4", "End: stat= %u", nsr->stat);
}

/*
 * lm_free_all
 */
/* ARGSUSED */
static void
lm_free_all(void *gen_args,
	void *resp,			/* unused */
	struct lm_nlm_disp *disp,
	struct lm_sysid *ls,
	u_int xid)			/* unused */
{
	nlm4_notify *nn = (nlm4_notify *)gen_args;
	status s;

	lm_debu5(2, "free_all4", "name= %s, stat= %ld", (int)nn->name,
		nn->state);

	s.mon_name = nn->name;
	s.state = (int)nn->state;
	(void) lm_crash(&s, resp, disp, ls, xid);

	lm_debu3(2, "free_all4", "End");
}

static struct lm_nlm_disp lm_nlm_disp[] = {

	/*
	 * NLM4_VERS
	 */

	/* NLMPROC4_NULL = 0 */
	{ lm_null, LM_REPLY, LM_DONT, LM_SENDREPLY, LM_IGNORED,
		xdr_void, xdr_void },

	/* NLMPROC4_TEST = 1 */
	{ lm_test, LM_REPLY, LM_DONT, LM_SENDREPLY, LM_IGNORED,
		xdr_nlm4_testargs, xdr_nlm4_testres },

	/* NLMPROC4_LOCK = 2 */
	{ lm_lock, LM_REPLY, LM_DONT, LM_SENDREPLY, LM_IGNORED,
		xdr_nlm4_lockargs, xdr_nlm4_res },

	/* NLMPROC4_CANCEL = 3 */
	{ lm_cancel, LM_REPLY, LM_DONT, LM_SENDREPLY, LM_IGNORED,
		xdr_nlm4_cancargs, xdr_nlm4_res },

	/* NLMPROC4_UNLOCK = 4 */
	{ lm_unlock, LM_REPLY, LM_DONT, LM_SENDREPLY, LM_IGNORED,
		xdr_nlm4_unlockargs, xdr_nlm4_res },

	/* NLMPROC4_GRANTED = 5 */
	{ lm_granted, LM_REPLY, LM_DONT, LM_SENDREPLY, LM_IGNORED,
		xdr_nlm4_testargs, xdr_nlm4_res },

	/* NLMPROC4_TEST_MSG = 6 */
	{ lm_test, LM_REPLY, LM_DONT, LM_CALLBACK, NLMPROC4_TEST_RES,
		xdr_nlm4_testargs, xdr_nlm4_testres },

	/* NLMPROC4_LOCK_MSG = 7 */
	{ lm_lock, LM_REPLY, LM_DONT, LM_CALLBACK, NLMPROC4_LOCK_RES,
		xdr_nlm4_lockargs, xdr_nlm4_res },

	/* NLMPROC4_CANCEL_MSG = 8 */
	{ lm_cancel, LM_REPLY, LM_DONT, LM_CALLBACK, NLMPROC4_CANCEL_RES,
		xdr_nlm4_cancargs, xdr_nlm4_res },

	/* NLMPROC4_UNLOCK_MSG = 9 */
	{ lm_unlock, LM_REPLY, LM_DONT, LM_CALLBACK, NLMPROC4_UNLOCK_RES,
		xdr_nlm4_unlockargs, xdr_nlm4_res },

	/* NLMPROC4_GRANTED_MSG = 10 */
	{ lm_granted, LM_REPLY, LM_DONT, LM_CALLBACK, NLMPROC4_GRANTED_RES,
		xdr_nlm4_testargs, xdr_nlm4_res },

	/* NLMPROC4_TEST_RES = 11 */
	{ lm_null, LM_DONT, LM_DONT, LM_SENDREPLY, LM_IGNORED,
		xdr_void, xdr_void },

	/* NLMPROC4_LOCK_RES = 12 */
	{ lm_null, LM_DONT, LM_DONT, LM_SENDREPLY, LM_IGNORED,
		xdr_void, xdr_void },

	/* NLMPROC4_CANCEL_RES = 13 */
	{ lm_null, LM_DONT, LM_DONT, LM_SENDREPLY, LM_IGNORED,
		xdr_void, xdr_void },

	/* NLMPROC4_UNLOCK_RES = 14 */
	{ lm_null, LM_DONT, LM_DONT, LM_SENDREPLY, LM_IGNORED,
		xdr_void, xdr_void },

	/* NLMPROC4_GRANTED_RES = 15 */
	{ lm_granted_res, LM_DONT, LM_DONT, LM_SENDREPLY, LM_IGNORED,
		xdr_nlm4_res, xdr_void },

	/* 16 */
	{ lm_null, LM_REPLY, LM_DONT, LM_SENDREPLY, LM_IGNORED,
		xdr_void, xdr_void },

	/* 17 */
	{ lm_null, LM_REPLY, LM_DONT, LM_SENDREPLY, LM_IGNORED,
		xdr_void, xdr_void },

	/* 18 */
	{ lm_null, LM_REPLY, LM_DONT, LM_SENDREPLY, LM_IGNORED,
		xdr_void, xdr_void },

	/* 19 */
	{ lm_null, LM_REPLY, LM_DONT, LM_SENDREPLY, LM_IGNORED,
		xdr_void, xdr_void },

	/* NLMPROC4_SHARE = 20 */
	{ lm_share, LM_REPLY, LM_DONT, LM_SENDREPLY, LM_IGNORED,
		xdr_nlm4_shareargs, xdr_nlm4_shareres },

	/* NLMPROC4_UNSHARE = 21 */
	{ lm_unshare, LM_REPLY, LM_DONT, LM_SENDREPLY, LM_IGNORED,
		xdr_nlm4_shareargs, xdr_nlm4_shareres },

	/* NLMPROC4_NM_LOCK = 22 */
	{ lm_lock, LM_REPLY, LM_DONT, LM_SENDREPLY, LM_IGNORED,
		xdr_nlm4_lockargs, xdr_nlm4_res },

	/* NLMPROC4_FREE_ALL = 23 */
	{ lm_free_all, LM_REPLY, LM_DONT, LM_SENDREPLY, LM_IGNORED,
		xdr_nlm4_notify, xdr_void }
};

/*
 * Convenient dispatch "entries" for NLMPROC4_LOCK and NLMPROC4_LOCK_MSG
 * blocking lock requests, respectively.
 */
static struct lm_nlm_disp block_lock_disp =
	{ lm_block_lock, LM_DONT, LM_REPLY, LM_SENDREPLY, LM_IGNORED,
		xdr_nlm4_lockargs, xdr_nlm4_res };

static struct lm_nlm_disp block_lock_msg_disp =
	{ lm_block_lock, LM_DONT, LM_REPLY, LM_CALLBACK, NLMPROC4_LOCK_RES,
		xdr_nlm4_lockargs, xdr_nlm4_res };

/*
 * lm_nlm_dispatch is the dispatcher routine for the NLM protocol.
 * The routine is based on the rfs_dispatch() routine.
 */
void
lm_nlm4_dispatch(register struct svc_req *req, register SVCXPRT *xprt)
{
	union {
		nlm4_testargs nta;
		nlm4_lockargs nla;
		nlm4_cancargs nca;
		nlm4_unlockargs nua;
		nlm4_shareargs nsa;
		nlm4_notify nn;
	} arg;

	union {
		nlm4_testres ntr;
		nlm4_res nr;
		nlm4_shareres nsr;
	} res;

	struct lm_nlm_disp *disp = NULL;
	struct lm_sysid	 *ls = NULL;
	struct lm_config *ln = NULL;
	char *name = NULL;
	bool_t reclaim = FALSE;
	int error = 0;			/* number of errors */
	int proc = req->rq_proc;
	int max_proc;

	lm_debu5(2, "nlm_disp4", "vers= %u, proc= %u", req->rq_vers, proc);

	if (nlm_dispatch_enter(xprt) != 0) {
		++error;
		goto done;
	}

	/*
	 * Reset arg and res.
	 */
	bzero((caddr_t)&arg, sizeof (arg));
	bzero((caddr_t)&res, sizeof (res));

	/*
	 * Verify that the version is OK.
	 */
	switch (req->rq_vers) {
	case NLM4_VERS:
		max_proc = NLMPROC4_FREE_ALL;
		break;
	default:
		svcerr_progvers(xprt, NLM_VERS, NLM4_VERS);
		error++;
		goto done;
	};

	/*
	 * Verify that the procedure is OK.
	 */
	if ((proc < 0) || (max_proc < proc)) {
		svcerr_noproc(xprt);
		error++;
		goto done;
	}

	/*
	 * OK, find the dispatcher entry.
	 */
	disp = &lm_nlm_disp[proc];

	/*
	 * Deserialize into arg.
	 */
	if (! SVC_GETARGS(xprt, disp->xdrargs, (caddr_t)&arg)) {
		svcerr_decode(xprt);
		error++;
		goto done;
	}

	/*
	 * If possible, get name and other fields.
	 * Initialize the cookie part of res.
	 * Default res.stat to nlm_denied_grace_period.
	 */
	switch (proc) {
	case NLMPROC4_TEST:
	case NLMPROC4_TEST_MSG:
		name = arg.nta.alock.caller_name;
		res.ntr.cookie = arg.nta.cookie;
		res.ntr.stat.stat = NLM4_DENIED_GRACE_PERIOD;
		break;

	case NLMPROC4_NM_LOCK:
		/*
		 * Silently enforce XNFS spec.
		 */
		arg.nla.block = FALSE;
		arg.nla.state = 0;
		/* FALLTHROUGH */

	case NLMPROC4_LOCK:
	case NLMPROC4_LOCK_MSG:
		if (arg.nla.block) {
			/*
			 * Creating a new dispatch "entry" for
			 * blocking lock requests improves the
			 * partitioning of logic in lm_lock()
			 * and lm_block_lock().
			 */
			(void) lm_savexprt(xprt);
			if (proc == NLMPROC4_LOCK) {
				disp = &block_lock_disp;
			} else {
				disp = &block_lock_msg_disp;
			}
		}
		name = arg.nla.alock.caller_name;
		reclaim = arg.nla.reclaim;
		res.nr.cookie = arg.nla.cookie;
		res.nr.stat.stat = NLM4_DENIED_GRACE_PERIOD;
		break;

	case NLMPROC4_CANCEL:
	case NLMPROC4_CANCEL_MSG:
		name = arg.nca.alock.caller_name;
		res.nr.cookie = arg.nca.cookie;
		res.nr.stat.stat = NLM4_DENIED_GRACE_PERIOD;
		break;

	case NLMPROC4_UNLOCK:
	case NLMPROC4_UNLOCK_MSG:
		name = arg.nua.alock.caller_name;
		res.nr.cookie = arg.nua.cookie;
		res.nr.stat.stat = NLM4_DENIED_GRACE_PERIOD;
		break;

	case NLMPROC4_GRANTED:
	case NLMPROC4_GRANTED_MSG:
		name = arg.nta.alock.caller_name;
		reclaim	 = TRUE;	/* A client function. */
		res.nr.cookie = arg.nta.cookie;
		res.nr.stat.stat = NLM4_DENIED_GRACE_PERIOD;
		break;

	case NLMPROC4_SHARE:
	case NLMPROC4_UNSHARE:
		name = arg.nsa.share.caller_name;
		reclaim = arg.nsa.reclaim;
		res.nsr.cookie = arg.nsa.cookie;
		res.nsr.stat = NLM4_DENIED_GRACE_PERIOD;
		res.nsr.sequence = 0;
		break;

	case NLMPROC4_FREE_ALL:
		name = arg.nn.name;
		reclaim = TRUE;
		break;

	case NLMPROC4_GRANTED_RES:
	case NLMPROC4_TEST_RES:
	case NLMPROC4_LOCK_RES:
	case NLMPROC4_CANCEL_RES:
	case NLMPROC4_UNLOCK_RES:
	case PRV_CRASH:
	case PRV_RECOVERY:
	default:
		/*
		 * Set reclaim. Grace-period has no meaning.
		 */
		reclaim	= TRUE;
	}

	/*
	 * For most of the NLM-calls, we will need the lm_sysid.
	 */
	if (name) {
		struct netbuf *addr;

		ln = lm_getconfig(xprt->xp_fp);
		/*
		 * If no entry is found, generate an error.  This shouldn't
		 * be happening, but it's reproducible, so don't generate
		 * the warning in production kernels just yet.
		 */
		if (ln == (struct lm_config *)NULL) {
#ifdef DEBUG
			cmn_err(CE_WARN, "lm_nlm4_dispatch: no config entry");
#endif
			lm_debu3(2, "nlm_disp4", "no config entry");
			svcerr_systemerr(xprt);	/* just drop on the floor? */
			error++;
			goto done;
		}
		/*
		 * Work around a bug/poor semantics in svc_getrpccaller.
		 * It always gives us maxlen == 0, even though there is
		 * a buf and nonzero len ... so we patch it. XXX
		 */
		addr = svc_getrpccaller(xprt);
		if (addr->maxlen < addr->len)
			addr->maxlen = addr->len;
		rw_enter(&lm_sysids_lock, RW_READER);
		ls = lm_get_sysid(&ln->config, addr, name, TRUE, NULL);
		rw_exit(&lm_sysids_lock);
	}

	/*
	 * Call the procedure if we are not in the grace period or this
	 * is a reclaim.  Note that res has been initialized to
	 * NLM4_DENIED_GRACE_PERIOD.
	 */
	if ((lm_sa.grace < time - lm_stat.start_time) || reclaim) {
		(*disp->proc)(&arg, &res, disp, ls, xprt->xp_xid);
	}

done:
	/*
	 * Return result.
	 * If call back, issue a one-way RPC call.
	 */
	if (!error && (disp->do_disp_reply == LM_REPLY)) {
		if (disp->callback_reply == LM_CALLBACK) {
			if (lm_callrpc(ls, NLM_PROG, NLM4_VERS,
					disp->callback_proc,
					disp->xdrres, (caddr_t)&res,
					xdr_void, NULL, LM_NO_TIMOUT,
					LM_RETRY)) {
				error++;
			}
		} else {
			if (!svc_sendreply(xprt, disp->xdrres, (caddr_t)&res)) {
				lm_debu3(2, "nlm_disp4", "Bad svc_sendreply");
				error++;
			}
		}
	}

	/*
	 * Free arguments.
	 */
	if (disp) {
		if (! SVC_FREEARGS(xprt, disp->xdrargs, (caddr_t)&arg)) {
			error = 1;
		}
	}

	/*
	 * If monitored lock, tell SM.
	 * Because of speed, this is done after replying.
	 *
	 * XXX: looks like we monitor this client even if we didn't
	 *	grant it a lock!
	 */
	switch (proc) {
		case NLMPROC4_LOCK:
		case NLMPROC4_LOCK_MSG:
			if (arg.nla.block) {
				lm_relxprt(xprt);
			}
			if (ls) {
				lm_sm_server(ls, lm_get_me());
			}
			break;
	}

	if (ls != NULL) {
		lm_rel_sysid(ls);
	}

	mutex_enter(&lm_lck);
	lm_stat.tot_in++;
	lm_stat.bad_in += error;
	lm_stat.proc_in[proc]++;
	mutex_exit(&lm_lck);

	nlm_dispatch_exit();

	lm_debu6(2, "nlm_disp4",
		"End: error= %u, tot= %u, bad= %u\n",
		error, lm_stat.tot_in, lm_stat.bad_in);
}

/*
 * reclaim locks associated with a vnode
 */
void
lm_nlm4_reclaim(struct vnode *vp, struct flock64 *flkp)
{
	struct lm_sysid *ls;
	int cookie;
#ifdef SIGLOST_RESTORED
	proc_t  *p;
#endif
	mntinfo_t *mi;
	nlm4_lockargs nla;
	nlm4_res nr;

	/*
	 * Reclaim lock corresponding to `flkp' from server.  `flkp' is
	 *	our cached copy of the lock, kept in the local locking layer.
	 *	`vp' is our cached copy of the remote vp on which we hold
	 *	`flkp'.
	 * If we cannot reclaim the lock, send SIGLOST to the process
	 *	that lost it.
	 */
	bzero((caddr_t)&nla, sizeof (nla));
	bzero((caddr_t)&nr, sizeof (nr));

	mutex_enter(&lm_lck);
	cookie = lm_stat.cookie++;
	mutex_exit(&lm_lck);
	nla.cookie.n_len = sizeof (cookie);
	nla.cookie.n_bytes = (char *)&cookie;
	nla.block = FALSE;
	nla.exclusive = (flkp->l_type == F_WRLCK);
	nla.alock.caller_name = utsname.nodename;
	nla.alock.fh.n_len = VTOFH3(vp)->fh3_length;
	nla.alock.fh.n_bytes = (char *)&(VTOFH3(vp)->fh3_u.data);
	nla.alock.oh.n_len = sizeof (cookie);
	nla.alock.oh.n_bytes = (char *)&cookie;
	nla.alock.svid = flkp->l_pid;
	nla.alock.l_offset = flkp->l_start;
	if (flkp->l_len == (u_offset_t)MAXOFFSET_T + 1)
		nla.alock.l_len = 0;
	else
		nla.alock.l_len = flkp->l_len;
	nla.reclaim = TRUE;
	nla.state = 1;

	/*
	 * Get an lm_sysid for server that has the same semantics we
	 * originally used to obtain this lock.  Note that we expect
	 * this entry to be in the cache - lm_get_sysid() will panic
	 * if it isn't since we pass FALSE for `alloc' - so we cannot
	 * possibly drop the READER lm_sysids_lock during this call.
	 */
	mi = VTOMI(vp);
	ls = lm_get_sysid(mi->mi_knetconfig, &mi->mi_addr, mi->mi_hostname,
			FALSE, NULL);

	(void) lm_callrpc(ls, NLM_PROG, NLM4_VERS, NLMPROC4_LOCK,
			xdr_nlm4_lockargs, (caddr_t)&nla,
			xdr_nlm4_res, (caddr_t)&nr,
			lm_sa.retransmittimeout, LM_RETRY);

	lm_debu4(4, "rlck_serv", "nr.stat= %d", nr.stat.stat);

	if (nr.stat.stat != NLM4_GRANTED) {
#ifdef SIGLOST_RESTORED
		/*
		 * XXX: once SIGLOST is restored to Solaris 2.x,
		 * we can find the proc and signal it that the
		 * lock could not be reclaimed.
		 */
		mutex_enter(&pidlock);
		p = prfind(flkp->l_pid);
		if (p)
			psignal(p, SIGLOST);
		mutex_exit(&pidlock);
#endif

		cmn_err(CE_NOTE,
			"lockd reclaim failed: pid %d lost lock on server %s\n",
			flkp->l_pid, ls->name);
		/*
		 * XXX: do we need to discard the local locking
		 * layer's cached copy of the lock here?
		 */
	}

	lm_rel_sysid(ls);
}
