/*	Copyright (c) 1994,1995,1996 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/

#pragma ident	"@(#)socksubr.c	1.23	96/10/14 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/cred.h>
#include <sys/kmem.h>
#include <sys/sysmacros.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/open.h>
#include <sys/user.h>
#include <sys/termios.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/esunddi.h>
#include <sys/flock.h>
#include <sys/modctl.h>
#include <sys/cmn_err.h>
#include <sys/mkdev.h>
#include <sys/pathname.h>
#include <sys/ddi.h>

#include <sys/socket.h>
#include <sys/socketvar.h>
#include <netinet/in.h>
#include <sys/un.h>

#include <sys/tiuser.h>
#include <sys/tihdr.h>

/*
 * Macros that operate on struct T_opthdr.
 * Note that TOPT_NEXT performs the roundup of the length.
 * The user of TOPT_NEXT must verify that the resulting pointer falls within
 * the option buffer.
 * The TOPT_VALID macro does not assume that the last option buffer is padded.
 */
#define	TOPT_NEXT(tohp)	\
	(struct T_opthdr *)((u_char *)(tohp) + ROUNDUP_tpi_opt((tohp)->len))
#define	TOPT_CONTENT(tohp)	(char *)(&((tohp)[1]))
#define	TOPT_CONTENTLEN(tohp)	((tohp)->len - sizeof (struct T_opthdr))
#define	TOPT_VALID(tohp, start, end)				\
	(ISALIGNED_tpi_opthdr(tohp) &&				\
	((char *)(tohp) >= (start)) &&				\
	((char *)(tohp) < (end)) &&				\
	((int)(tohp)->len >= sizeof (struct T_opthdr)) &&	\
	((char *)(tohp) + (tohp)->len <= (end)))

/*
 * Macros that operate on struct cmsghdr.
 * The CMSG_VALID macro does not assume that the last option buffer is padded.
 */
#define	CMSG_NEXT(cmsg) \
	(struct cmsghdr *)((char *)(cmsg) + ROUNDUP_cmsglen((cmsg)->cmsg_len))
#define	CMSG_CONTENT(cmsg)	(char *)(&((cmsg)[1]))
#define	CMSG_CONTENTLEN(cmsg)	((cmsg)->cmsg_len - sizeof (struct cmsghdr))
#define	CMSG_VALID(cmsg, start, end)	\
	(ISALIGNED_cmsghdr(cmsg) &&	\
	((char *)(cmsg) >= (start)) &&	\
	((char *)(cmsg) < (end)) &&	\
	((int)(cmsg)->cmsg_len >= (int)sizeof (struct cmsghdr)) &&	\
	((char *)(cmsg) + (cmsg)->cmsg_len <= (end)))

struct kmem_cache *sock_cache;
static dev_t sockdev;	/* For fsid in getattr */

struct sockparams *sphead;
krwlock_t splist_lock;

static major_t clonemaj = (major_t)-1;

/*
 * Translate from a device pathname (e.g. "/dev/tcp") to a vnode.
 * Returns with the vnode held.
 * In the non-cloning case sogetvp verifies that the major number
 * is ok and a stream. For the clone case these checks are done in sock_open.
 * TODO: Could make sogetvp do the check against clonemaj (including
 * ddi_hold_installed_driver on the real driver) and record in SCLONE
 * somewhere.
 */
static int
sogetvp(char *devpath, vnode_t **vpp, int uioflag)
{
	vnode_t *vp;
	major_t maj;
	int error;
	struct dev_ops *ops;

	ASSERT(uioflag == UIO_SYSSPACE || uioflag == UIO_USERSPACE);
	/* Lookup the underlying filesystem vnode */
	error = lookupname(devpath, uioflag, FOLLOW, NULLVPP,
				&vp);
	if (error)
		return (error);

	/* Check that it is the correct vnode */
	if (vp->v_type != VCHR) {
		VN_RELE(vp);
		return (ENOTSOCK);
	}
	maj = getmajor(vp->v_rdev);
	if (maj >= devcnt) {
		VN_RELE(vp);
		return (ENXIO);
	}
	/* need to load driver? */
	ops = ddi_hold_installed_driver(maj);
	if ((ops == NULL) || (ops->devo_cb_ops == NULL)) {
		VN_RELE(vp);
		return (ENXIO);
	}
	if (!STREAMSTAB(maj)) {
		ddi_rele_driver(maj);
		VN_RELE(vp);
		return (ENOSTR);
	}

	*vpp = vp;
	return (0);
}

/*
 * Add or delete (latter if devpath is NULL) an enter to the sockparams
 * table. If devpathlen is zero the devpath with not be kmem_freed. Otherwise
 * this routine assumes that the caller has kmem_alloced devpath/devpathlen
 * for this routine to consume.
 * The zero devpathlen could be used if the kernel wants to create entries
 * itself by calling sockconfig(1,2,3, "/dev/tcp", 0);
 */
int
soconfig(int domain, int type, int protocol,
    char *devpath, int devpathlen)
{
	struct sockparams **spp;
	struct sockparams *sp;
	int error = 0;

	dprint(0, ("soconfig(%d,%d,%d,%s,%d)\n",
		domain, type, protocol, devpath, devpathlen));

	/*
	 * Look for an existing match.
	 */
	rw_enter(&splist_lock, RW_WRITER);
	for (spp = &sphead; (sp = *spp) != NULL; spp = &sp->sp_next) {
		if (sp->sp_domain == domain &&
		    sp->sp_type == type &&
		    sp->sp_protocol == protocol) {
			break;
		}
	}
	if (devpath == NULL) {
		/* Delete existing entry */
		if (sp == NULL) {
			error = ENXIO;
			goto done;
		}
		/* Unlink and free existing entry */
		*spp = sp->sp_next;
		ASSERT(sp->sp_vnode);
		ddi_rele_driver(getmajor(sp->sp_vnode->v_rdev));
		VN_RELE(sp->sp_vnode);
		if (sp->sp_devpathlen != 0)
			kmem_free(sp->sp_devpath, sp->sp_devpathlen);
		kmem_free(sp, sizeof (*sp));
	} else {
		vnode_t *vp;

		/* Add new entry */
		if (sp != NULL) {
			error = EEXIST;
			goto done;
		}

		error = sogetvp(devpath, &vp, UIO_SYSSPACE);
		if (error) {
			dprint(0, ("soconfig: vp %s failed with %d\n",
				devpath, error));
			goto done;
		}

		dprint(0, ("soconfig: %s => vp 0x%x, dev 0x%x\n",
			devpath, vp, vp->v_rdev));

		sp = kmem_alloc(sizeof (*sp), KM_SLEEP);
		sp->sp_domain = domain;
		sp->sp_type = type;
		sp->sp_protocol = protocol;
		sp->sp_devpath = devpath;
		sp->sp_devpathlen = devpathlen;
		sp->sp_vnode = vp;
		sp->sp_next = NULL;
		*spp = sp;
	}
done:
	rw_exit(&splist_lock);
#ifdef SOCK_DEBUG
	if (error) {
		eprintline(error);
	}
#endif /* SOCK_DEBUG */
	return (error);
}

/*
 * Lookup an entry in the sockparams list based on the triple.
 * If no entry is found and devpath is not NULL translate devpath to a
 * vnode. Note that devpath is a pointer to a user address!
 * Returns with the vnode held.
 *
 * When this routine uses devpath it does not create an entry in the sockparams
 * list since this routine can run on behalf of any user and one user
 * should be able to effect the transport used by another user.
 *
 * In order to return the correct error this routine has to do wildcard scans
 * of the list. The errors are (in decreasing precedence):
 *	EAFNOSUPPORT - address family not in list
 *	EPROTONOSUPPORT - address family and type supported by not protocol.
 *	EPROTOTYPE - address family supported by not socket type.
 */
vnode_t *
solookup(int domain, int type, int protocol, char *devpath, int *errorp)
{
	struct sockparams *sp;
	int error;
	vnode_t *vp;

	rw_enter(&splist_lock, RW_READER);
	for (sp = sphead; sp != NULL; sp = sp->sp_next) {
		if (sp->sp_domain == domain &&
		    sp->sp_type == type &&
		    sp->sp_protocol == protocol) {
			break;
		}
	}
	if (sp == NULL) {
		dprint(0, ("solookup(%d,%d,%d) not found\n",
			domain, type, protocol));
		if (devpath == NULL) {
			/* Determine correct error code */
			int found = 0;

			for (sp = sphead; sp != NULL; sp = sp->sp_next) {
				if (sp->sp_domain == domain && found < 1)
					found = 1;
				if (sp->sp_domain == domain &&
				    sp->sp_protocol == protocol && found < 2)
					found = 2;
			}
			rw_exit(&splist_lock);
			switch (found) {
			case 0:
				*errorp = EAFNOSUPPORT;
				break;
			case 1:
				*errorp = EPROTONOSUPPORT;
				break;
			case 2:
				*errorp = EPROTOTYPE;
				break;
			}
			return (NULL);
		}
		rw_exit(&splist_lock);

		/*
		 * Return vp based on devpath.
		 * Do not enter into table to avoid random users
		 * modifying the sockparams list.
		 */
		error = sogetvp(devpath, &vp, UIO_USERSPACE);
		if (error) {
			dprint(0, ("solookup: vp 0x%x failed with %d\n",
				devpath, error));
			*errorp = EPROTONOSUPPORT;
			return (NULL);
		}
		dprint(0, ("solookup: 0x%x => vp 0x%x, dev 0x%x\n",
			devpath, vp, vp->v_rdev));

		return (vp);
	}
	dprint(0, ("solookup(%d,%d,%d) vp 0x%x devpath %s\n",
		domain, type, protocol, sp->sp_vnode, sp->sp_devpath));

	vp = sp->sp_vnode;
	VN_HOLD(vp);
	rw_exit(&splist_lock);
	return (vp);
}

/*
 * Return a socket vnode.
 *
 * Assumes that the caller is "passing" an VN_HOLD for accessvp i.e.
 * when the socket is freed a VN_RELE will take place.
 *
 * Note that sockets assume that the driver will clone (either itself
 * or by using the clone driver) i.e. a socket() call will always
 * result in a new vnode being created.
 */
struct vnode *
makesockvp(struct vnode *accessvp, int domain, int type, int protocol)
{
	struct sonode *so;
	struct vnode *vp;
	time_t now;
	dev_t dev;

	if (clonemaj == (major_t)-1)
		clonemaj = ddi_name_to_major("clone");

	so = kmem_cache_alloc(sock_cache, KM_SLEEP);
	vp = SOTOV(so);
	now = hrestime.tv_sec;

	so->so_flag	= 0;
	so->so_accessvp	= accessvp;
	dev = accessvp->v_rdev;

	/*
	 * Handle cloning here - record in so_flag that it is a clone.
	 * In the non-cloning case sogetvp verifies that the major number
	 * is ok and a stream. For the clone case the major number is
	 * verified here and sock_open checks that it is a stream.
	 * Note that the driver has to be loaded before STREAMTAB() can be
	 * checked.
	 */
	if (getmajor(dev) == clonemaj) {
		major_t maj;
		minor_t emaj;

		dprint(0, ("makesockvp: clonemaj\n"));
		so->so_flag |= SCLONE;
		emaj = getminor(dev); /* minor is major for a cloned driver */
		maj = etoimajor(emaj);

		if (maj >= devcnt) {
			/*
			 * The minor number in the clone dev_t is too large
			 * to be a major number.
			 * Use a bogus major number here so that sock_open
			 * can fail the open.
			 */
			eprint(("makesockvp: bad majcnt\n"));
			emaj = (major_t)-1;
		}

		so->so_dev = makedevice(emaj, 0);
	} else
		so->so_dev = dev;

	so->so_state	= 0;
	so->so_mode	= 0;

	so->so_fsid	= sockdev;
	so->so_atime	= now;
	so->so_mtime	= now;
	so->so_ctime	= now;		/* Never modified */
	so->so_count	= 0;

	so->so_family	= domain;
	so->so_type	= type;
	so->so_protocol	= protocol;
	so->so_pushcnt	= 0;

	so->so_options	= 0;
	so->so_linger.l_onoff	= 0;
	so->so_linger.l_linger = 0;
	so->so_sndbuf	= 0;
	so->so_rcvbuf	= 0;
#ifdef notyet
	so->so_sndlowat	= 0;
	so->so_rcvlowat	= 0;
	so->so_sndtimeo	= 0;
	so->so_rcvtimeo	= 0;
#endif /* notyet */
	so->so_error	= 0;
	so->so_delayed_error = 0;

	ASSERT(so->so_oobmsg == NULL);
	so->so_oobcnt	= 0;
	so->so_oobsigcnt = 0;
	so->so_pgrp	= 0;

	so->so_laddr_sa = so->so_faddr_sa = NULL;
	so->so_laddr_len = so->so_faddr_len = 0;
	so->so_laddr_maxlen = so->so_faddr_maxlen = 0;
	so->so_eaddr_mp = NULL;
	so->so_delayed_error = 0;

	ASSERT(so->so_ack_mp == NULL);
	ASSERT(so->so_conn_ind_head == NULL);
	ASSERT(so->so_conn_ind_tail == NULL);
	ASSERT(so->so_ux_bound_vp == NULL);
	ASSERT(so->so_unbind_mp == NULL);

	vp->v_flag	= 0;
	vp->v_count	= 1;
	vp->v_vfsmountedhere = NULL;
	vp->v_vfsp	= rootvfs;
	vp->v_stream	= NULL;
	vp->v_pages	= NULL;
	vp->v_type	= VSOCK;
	vp->v_rdev	= so->so_dev;
	vp->v_filocks	= NULL;
	vp->v_shrlocks	= NULL;

	return (vp);
}

void
sockfree(struct sonode *so)
{
	mblk_t *mp;
	vnode_t *vp;

	ASSERT(so->so_count == 0);
	ASSERT(so->so_accessvp);

	vp = so->so_accessvp;
	VN_RELE(vp);

	if (so->so_laddr_sa) {
		kmem_free(so->so_laddr_sa, so->so_laddr_maxlen);
		so->so_laddr_sa = NULL;
		so->so_laddr_len = so->so_laddr_maxlen = 0;
	}
	if (so->so_faddr_sa) {
		kmem_free(so->so_faddr_sa, so->so_faddr_maxlen);
		so->so_faddr_sa = NULL;
		so->so_faddr_len = so->so_faddr_maxlen = 0;
	}
	if ((mp = so->so_eaddr_mp) != NULL) {
		freemsg(mp);
		so->so_eaddr_mp = NULL;
		so->so_delayed_error = 0;
	}
	if ((mp = so->so_ack_mp) != NULL) {
		freemsg(mp);
		so->so_ack_mp = NULL;
	}
	if ((mp = so->so_conn_ind_head) != NULL) {
		mblk_t *mp1;

		while (mp) {
			mp1 = mp->b_next;
			mp->b_next = NULL;
			freemsg(mp);
			mp = mp1;
		}
		so->so_conn_ind_head = so->so_conn_ind_tail = NULL;
		so->so_state &= ~SS_HASCONNIND;
	}
#ifdef DEBUG
	mutex_enter(&so->so_lock);
	ASSERT(so_verify_oobstate(so));
	mutex_exit(&so->so_lock);
#endif /* DEBUG */
	if ((mp = so->so_oobmsg) != NULL) {
		freemsg(mp);
		so->so_oobmsg = NULL;
		so->so_state &= ~(SS_OOBPEND|SS_HAVEOOBDATA|SS_HADOOBDATA);
	}
	ASSERT(so->so_ux_bound_vp == NULL);
	if ((mp = so->so_unbind_mp) != NULL) {
		freemsg(mp);
		so->so_unbind_mp = NULL;
	}
	kmem_cache_free(sock_cache, so);
}

/*
 * Update the accessed, updated, or changed times in an sonode
 * with the current time.
 *
 * Note that both SunOS 4.X and 4.4BSD sockets do not present reasonable
 * attributes in a fstat call. (They return the current time and 0 for
 * all timestamps, respectively.) We maintain the current timestamps
 * here primarily so that should sockmod be popped the resulting
 * file descriptor will behave like a stream w.r.t. the timestamps.
 */
void
so_update_attrs(register struct sonode *so, register int flag)
{
	time_t now = hrestime.tv_sec;

	mutex_enter(&so->so_lock);
	so->so_flag |= flag;
	if (flag & SACC)
		so->so_atime = now;
	if (flag & SMOD)
		so->so_mtime = now;
	mutex_exit(&so->so_lock);
}

/*ARGSUSED*/
static int
sock_constructor(void *buf, void *cdrarg, int kmflags)
{
	struct sonode *so = buf;
	struct vnode *vp = SOTOV(so);

	so->so_oobmsg		= NULL;
	so->so_ack_mp		= NULL;
	so->so_conn_ind_head	= NULL;
	so->so_conn_ind_tail	= NULL;
	so->so_ux_bound_vp	= NULL;
	so->so_unbind_mp	= NULL;

	vp->v_op = sock_getvnodeops();
	vp->v_data = (caddr_t)so;

	mutex_init(&vp->v_lock, "sockfs v_lock", MUTEX_DEFAULT, NULL);
	cv_init(&vp->v_cv, "sockfs v_cv", CV_DEFAULT, NULL);
	mutex_init(&so->so_lock, "sonode.so_lock", MUTEX_DEFAULT, NULL);
	cv_init(&so->so_state_cv, "sonode.so_state_cv", CV_DEFAULT, NULL);
	cv_init(&so->so_ack_cv, "sonode.so_ack_cv", CV_DEFAULT, NULL);
	cv_init(&so->so_connind_cv, "sonode.so_connind_cv", CV_DEFAULT, NULL);
	cv_init(&so->so_want_cv, "sonode.so_want_cv", CV_DEFAULT, NULL);

	return (0);
}

/*ARGSUSED1*/
static void
sock_destructor(void *buf, void *cdrarg)
{
	struct sonode *so = buf;
	struct vnode *vp = SOTOV(so);

	ASSERT(so->so_oobmsg == NULL);
	ASSERT(so->so_ack_mp == NULL);
	ASSERT(so->so_conn_ind_head == NULL);
	ASSERT(so->so_conn_ind_tail == NULL);
	ASSERT(so->so_ux_bound_vp == NULL);
	ASSERT(so->so_unbind_mp == NULL);

	ASSERT(vp->v_op == sock_getvnodeops());
	ASSERT(vp->v_data == (caddr_t)so);

	mutex_destroy(&vp->v_lock);
	cv_destroy(&vp->v_cv);
	mutex_destroy(&so->so_lock);
	cv_destroy(&so->so_state_cv);
	cv_destroy(&so->so_ack_cv);
	cv_destroy(&so->so_connind_cv);
	cv_destroy(&so->so_want_cv);
}

/*
 * Init function called when sockfs is loaded.
 */
/*ARGSUSED1*/
int
sockinit(struct vfssw *vswp, int fstype)
{
	dev_t dev;

	/*
	 * Create sonode cache
	 */
	sock_cache = kmem_cache_create("sock_cache", sizeof (struct sonode),
		0, sock_constructor, sock_destructor, NULL, NULL, NULL, 0);

	/*
	 * Build initial list mapping socket parameters to vnode.
	 */
	rw_init(&splist_lock, "sockfs lookup", RW_DEFAULT, NULL);
	/*
	 * If sockets are needed before init runs /sbin/soconfig
	 * it is possible to preload the sockparams list here using
	 * calls like:
	 *	sockconfig(1,2,3, "/dev/tcp", 0);
	 */

	/*
	 * Associate vfs and vnode operations.
	 * Create a unique dev_t for use in so_fsid.
	 */
	vswp->vsw_vfsops = &sock_vfsops;
	if ((dev = getudev()) == -1)
		dev = 0;
	sockdev = makedevice(dev, 0);
	return (0);
}

/*
 * Caller must hold the mutex and pass in SLOCKED or SREADLOCKED.
 * If the caller wants nonblocking behavior it should set fmode.
 */
int
so_lock_single(struct sonode *so, int flag, int fmode)
{
	ASSERT(MUTEX_HELD(&so->so_lock));
	ASSERT(flag & (SLOCKED|SREADLOCKED));
	ASSERT((flag & ~(SLOCKED|SREADLOCKED)) == 0);

	while (so->so_flag & flag) {
		if (fmode & (FNDELAY|FNONBLOCK))
			return (EWOULDBLOCK);
		so->so_flag |= SWANT;
		cv_wait(&so->so_want_cv, &so->so_lock);
	}
	so->so_flag |= flag;
	return (0);
}

/*
 * Caller must hold the mutex and pass in SLOCKED or SREADLOCKED.
 */
void
so_unlock_single(struct sonode *so, int flag)
{
	ASSERT(MUTEX_HELD(&so->so_lock));
	ASSERT(flag & (SLOCKED|SREADLOCKED));
	ASSERT((flag & ~(SLOCKED|SREADLOCKED)) == 0);
	ASSERT(so->so_flag & flag);

	if (so->so_flag & SWANT)
		cv_broadcast(&so->so_want_cv);
	so->so_flag &= ~(SWANT|flag);
}

/*
 * Verify that the specified offset falls within the mblk and
 * that the resulting pointer is aligned.
 * Returns NULL if not.
 */
u_char *
sogetoff(mblk_t *mp, int offset, int length, int align_size)
{
	u_char *ptr1, *ptr2;

	ASSERT(mp && mp->b_wptr >= mp->b_rptr);
	if (offset < 0 || length < 0)
		return (NULL);
	ptr1 = mp->b_rptr + offset;
	ptr2 = ptr1 + length;
	if (ptr1 < mp->b_rptr || ptr2 > mp->b_wptr) {
		eprintline(0);
		return (NULL);
	}
	if (((int)ptr1 & (align_size - 1)) != 0) {
		eprintline(0);
		return (NULL);
	}
	return (ptr1);
}

/*
 * Return the AF_UNIX underlying filesystem vnode matching a given name.
 * Makes sure the sending and the destination sonodes are compatible.
 * The vnode is returned held.
 *
 * The underlying filesystem VSOCK vnode has a v_stream pointer that
 * references the actual stream head (hence indirectly the actual sonode).
 */
static int
so_ux_lookup(struct sonode *so, struct sockaddr_un *soun, int checkaccess,
		vnode_t **vpp)
{
	vnode_t		*vp;	/* Underlying filesystem vnode */
	vnode_t		*svp;	/* sockfs vnode */
	struct sonode	*so2;
	int		error;

	dprintso(so, 1, ("so_ux_lookup(0x%x) name <%s>\n",
		so, soun->sun_path));

	error = lookupname(soun->sun_path, UIO_SYSSPACE, FOLLOW, NULLVPP, &vp);
	if (error) {
		eprintsoline(so, error);
		return (error);
	}
	if (vp->v_type != VSOCK) {
		error = ENOTSOCK;
		eprintsoline(so, error);
		goto done2;
	}

	if (checkaccess) {
		/*
		 * Check that we have permissions to access the destination
		 * vnode. This check is not done in BSD but it is required
		 * by X/Open.
		 */
		if (error = VOP_ACCESS(vp, VREAD|VWRITE, 0, CRED())) {
			eprintsoline(so, error);
			goto done2;
		}
	}

	/*
	 * Check if the remote socket has been closed.
	 *
	 * Synchronize with vn_rele_stream by holding v_lock while traversing
	 * v_stream->sd_vnode.
	 */
	mutex_enter(&vp->v_lock);
	if (vp->v_stream == NULL) {
		mutex_exit(&vp->v_lock);
		if (so->so_type == SOCK_DGRAM)
			error = EDESTADDRREQ;
		else
			error = ECONNRESET;

		eprintsoline(so, error);
		goto done2;
	}
	ASSERT(vp->v_stream->sd_vnode);
	svp = vp->v_stream->sd_vnode;
	/*
	 * holding v_lock on underlying filesystem vnode and acquiring
	 * it on sockfs vnode. Assumes that no code ever attempts to
	 * acquire these locks in the reverse order.
	 */
	VN_HOLD(svp);
	mutex_exit(&vp->v_lock);

	if (svp->v_type != VSOCK) {
		error = ENOTSOCK;
		eprintsoline(so, error);
		goto done;
	}

	so2 = VTOSO(svp);

	if (so->so_type != so2->so_type) {
		error = EPROTOTYPE;
		eprintsoline(so, error);
		goto done;
	}

	VN_RELE(svp);
	*vpp = vp;
	return (0);

done:
	VN_RELE(svp);
done2:
	VN_RELE(vp);
	return (error);
}

/*
 * Verify peer address for connect and sendto/sendmsg.
 * Since sendto/sendmsg would not get synchronous errors from the transport
 * provider we have to do these ugly checks in the socket layer to
 * preserve compatibility with SunOS 4.X.
 */
int
so_addr_verify(struct sonode *so, struct sockaddr *name, int namelen)
{
	int		family;

	dprintso(so, 1, ("so_addr_verify(%x, %x, %d)\n", so, name, namelen));

	ASSERT(name != NULL);

	family = so->so_family;
	if (family == AF_INET) {
		if (name->sa_family != family) {
			eprintsoline(so, EAFNOSUPPORT);
			return (EAFNOSUPPORT);
		}
		if (namelen != sizeof (struct sockaddr_in)) {
			eprintsoline(so, EINVAL);
			return (EINVAL);
		}
		return (0);
	} else if (family != AF_UNIX) {
		/*
		 * Don't do any length or sa_family check to allow
		 * non-sockaddr style addresses.
		 */
		return (0);
	}

	if (so->so_state & SS_FADDR_NOXLATE) {
		return (0);
	}

	ASSERT(family == AF_UNIX);
	if (namelen < sizeof (short)) {
		eprintsoline(so, ENOENT);
		return (ENOENT);
	}
	if (name->sa_family != family) {
		eprintsoline(so, EAFNOSUPPORT);
		return (EAFNOSUPPORT);
	}
	/* MAXPATHLEN + soun_family + nul termination */
	if (namelen > MAXPATHLEN + sizeof (short) + 1) {
		eprintsoline(so, ENAMETOOLONG);
		return (ENAMETOOLONG);
	}
	return (0);
}


/*
 * Translate an AF_UNIX sockaddr_un to the transport internal name.
 * Assumes caller has called so_addr_verify first.
 */
int
so_ux_addr_xlate(struct sonode *so, struct sockaddr *name, int namelen,
		int checkaccess, char **addrp, tpi_length_t *addrlenp)
{
	int			error;
	struct sockaddr_un	*soun;
	vnode_t			*vp;
	char			*addr;
	tpi_length_t		addrlen;

	dprintso(so, 1, ("so_ux_addr_xlate(%x, %x, %d, %d)\n",
			so, name, namelen, checkaccess));

	ASSERT(name != NULL);
	ASSERT(so->so_family == AF_UNIX);
	ASSERT(!(so->so_state & SS_FADDR_NOXLATE));
	ASSERT(namelen >= sizeof (short));
	ASSERT(name->sa_family == AF_UNIX);
	soun = (struct sockaddr_un *)name;
	/*
	 * Lookup vnode for the specified path name and verify that
	 * it is a socket.
	 */
	error = so_ux_lookup(so, soun, checkaccess, &vp);
	if (error) {
		eprintsoline(so, error);
		return (error);
	}
	/*
	 * Use the address of the peer vnode as the address to send
	 * to. We release the peer vnode here. In case it has been
	 * closed by the time the T_CONN_REQ or T_UNIDATA_REQ reaches the
	 * transport the message will get an error or be dropped.
	 */
	so->so_ux_faddr.sou_vp = (void *)vp;
	so->so_ux_faddr.sou_magic = SOU_MAGIC_EXPLICIT;
	addr = (char *)&so->so_ux_faddr;
	addrlen = sizeof (so->so_ux_faddr);
	dprintso(so, 1, ("ux_xlate UNIX: addrlen %d, vp 0x%x\n",
				addrlen, vp));
	VN_RELE(vp);
	*addrp = addr;
	*addrlenp = addrlen;
	return (0);
}

/*
 * Esballoc free function for messages that contain SO_FILEP option.
 * Decrement the reference count on the file pointers using closef.
 */
void
fdbuf_free(struct fdbuf *fdbuf)
{
	int	i;

	dprint(1, ("fdbuf_free: %d fds\n", fdbuf->fd_numfd));
	for (i = 0; i < fdbuf->fd_numfd; i++) {
		dprint(1, ("fdbuf_free: [%d] = 0x%x\n", i, fdbuf->fd_fds[i]));
		(void) closef(fdbuf->fd_fds[i]);
	}
	if (fdbuf->fd_ebuf != NULL)
		kmem_free(fdbuf->fd_ebuf, fdbuf->fd_ebuflen);
	kmem_free(fdbuf, fdbuf->fd_size);
}

/*
 * Allocate an esballoc'ed message for use for AF_UNIX file descriptor
 * passing. Sleep waiting for memory unless catching a signal in strwaitbuf.
 */
mblk_t *
fdbuf_allocmsg(int size, struct fdbuf *fdbuf)
{
	char	*buf;
	mblk_t	*mp;

	dprint(1, ("fdbuf_allocmsg: size %d, %d fds\n", size, fdbuf->fd_numfd));
	buf = kmem_alloc(size, KM_SLEEP);
	fdbuf->fd_ebuf = buf;
	fdbuf->fd_ebuflen = size;
	fdbuf->fd_frtn.free_func = fdbuf_free;
	fdbuf->fd_frtn.free_arg = (char *)fdbuf;

	while ((mp = esballoc((unsigned char *)buf, size, BPRI_MED,
	    &fdbuf->fd_frtn)) == NULL) {
		if (strwaitbuf(sizeof (mblk_t), BPRI_MED) != 0) {
			/*
			 * Got EINTR - pass out NULL. Caller will
			 * return something like ENOBUFS.
			 * XXX could use an esballoc_wait() type function.
			 */
			eprintline(ENOBUFS);
			return (NULL);
		}
	}
	if (mp == NULL)
		return (NULL);
	mp->b_datap->db_type = M_PROTO;
	return (mp);
}

/*
 * Extract file descriptors from a fdbuf.
 * Return list in rights/rightslen.
 */
static int
fdbuf_extract(struct fdbuf *fdbuf, char *rights, int rightslen)
{
	int	i, fd;
	int	*rp;
	struct file *fp;
	int	error;
	int	numfd;

	dprint(1, ("fdbuf_extract: %d fds, len %d\n",
		fdbuf->fd_numfd, rightslen));

	numfd = fdbuf->fd_numfd;
	ASSERT(rightslen == numfd * sizeof (int));

	/*
	 * Allocate a file descriptor and increment the f_count.
	 * The latter is needed since we always call fdbuf_free
	 * which performs a closef.
	 */
	rp = (int *)rights;
	for (i = 0; i < numfd; i++) {
		if ((error = ufalloc(0, &fd)) != 0)
			goto cleanup;
		fp = fdbuf->fd_fds[i];
		mutex_enter(&fp->f_tlock);
		fp->f_count++;
		mutex_exit(&fp->f_tlock);
		setf(fd, fp);
		*rp++ = fd;
		dprint(1, ("fdbuf_extract: [%d] = %d, 0x%x refcnt %d\n",
			i, fd, fp, fp->f_count));
	}
	return (0);

cleanup:
	/*
	 * Undo whatever partial work the loop above has done.
	 */
	{
		int j;

		rp = (int *)rights;
		for (j = 0; j < i; j++) {
			fp = fdbuf->fd_fds[j];
			dprint(0, ("fdbuf_extract: cleanup[%d] = %d, 0x%x\n",
				j, *rp, fp));
			/* Decrement f_count */
			closef(fp);
			setf(*rp++, NULLFP);
		}
	}
	return (error);
}

/*
 * Insert file descriptors into an fdbuf.
 * Returns a kmem_alloc'ed fdbuf. The fdbuf should be freed
 * by calling fdbuf_free().
 */
int
fdbuf_create(char *rights, int rightslen, struct fdbuf **fdbufp)
{
	int	numfd, i;
	int	*fds;
	struct file	*fp;
	struct fdbuf	*fdbuf;
	int	fdbufsize;

	dprint(1, ("fdbuf_create: len %d\n", rightslen));

	numfd = rightslen / sizeof (int);

	fdbufsize = FDBUF_HDRSIZE + (numfd * sizeof (struct file *));
	fdbuf = kmem_alloc(fdbufsize, KM_SLEEP);
	fdbuf->fd_size = fdbufsize;
	fdbuf->fd_numfd = 0;
	fdbuf->fd_ebuf = NULL;
	fdbuf->fd_ebuflen = 0;
	fds = (int *)rights;
	for (i = 0; i < numfd; i++) {
		if ((fp = GETF(fds[i])) == NULL) {
			fdbuf_free(fdbuf);
			return (EBADF);
		}
		dprint(1, ("fdbuf_create: [%d] = %d, 0x%x refcnt %d\n",
			i, fds[i], fp, fp->f_count));
		mutex_enter(&fp->f_tlock);
		fp->f_count++;
		mutex_exit(&fp->f_tlock);
		fdbuf->fd_fds[i] = fp;
		fdbuf->fd_numfd++;
		RELEASEF(fds[i]);
	}
	*fdbufp = fdbuf;
	return (0);
}

static int
fdbuf_optlen(int rightslen)
{
	int numfd;

	numfd = rightslen / sizeof (int);

	return (FDBUF_HDRSIZE + (numfd * sizeof (struct file *)));
}

static int
fdbuf_cmsglen(int fdbuflen)
{
	return ((fdbuflen - FDBUF_HDRSIZE) / sizeof (struct file *) *
		sizeof (int));
}


/*
 * Return non-zero if the mblk and fdbuf are consistent.
 */
static int
fdbuf_verify(mblk_t *mp, struct fdbuf *fdbuf, int fdbuflen)
{
	if (fdbuflen >= FDBUF_HDRSIZE &&
	    fdbuflen == fdbuf->fd_size) {
		frtn_t *frp = getfrtn(mp->b_datap);
		/*
		 * Check that the SO_FILEP portion of the
		 * message has not been modified by
		 * the loopback transport. The sending sockfs generates
		 * a message that is esballoc'ed with the free function
		 * being fdbuf_free() and where free_arg contains the
		 * identical information as the SO_FILEP content.
		 *
		 * If any of these constraints are not satisfied we
		 * silently ignore the option.
		 */
		ASSERT(mp);
		if (frp != NULL &&
		    frp->free_func == fdbuf_free &&
		    frp->free_arg != NULL &&
		    bcmp(frp->free_arg, fdbuf, fdbuflen) == 0) {
			dprint(1, ("fdbuf_verify: fdbuf 0x%x len %d\n",
				fdbuf, fdbuflen));
			return (1);
		} else {
			cmn_err(CE_WARN,
			    "sockfs: mismatched fdbuf content (0x%lx)",
			    (u_long)mp);
			return (0);
		}
	} else {
		cmn_err(CE_WARN,
			"sockfs: mismatched fdbuf len %d, %d\n",
			fdbuflen, fdbuf->fd_size);
		return (0);
	}
}

/*
 * When the file descriptors returned by sorecvmsg can not be passed
 * to the application this routine will cleanup the references on
 * the files. Start at startoff bytes into the buffer.
 */
static void
close_fds(char *fdbuf, int fdbuflen, int startoff)
{
	int *fds = (int *)fdbuf;
	int numfd = fdbuflen / sizeof (int);
	struct file *fp;
	int i;

	dprint(1, ("close_fds(0x%x, %d, %d)\n", fdbuf, fdbuflen, startoff));

	for (i = 0; i < numfd; i++) {
		if (startoff < 0)
			startoff = 0;
		if (startoff < sizeof (int)) {
			/*
			 * This file descriptor is partially or fully after
			 * the offset
			 */
			fp = getandset(fds[i]);
			if (fp != NULL) {
				dprint(0,
					("close_fds: cleanup[%d] = %d, 0x%x\n",
					i, fds[i], fp));
				(void) closef(fp);
			}
		}
		startoff -= sizeof (int);
	}
}

/*
 * Close all file descriptors contained in the control part starting at
 * the startoffset.
 */
void
so_closefds(char *control, int controllen, int oldflg, int startoff)
{
	struct cmsghdr *cmsg;

	if (control == NULL)
		return;

	if (oldflg) {
		close_fds(control, controllen, startoff);
		return;
	}
	/* Scan control part for file descriptors. */
	for (cmsg = (struct cmsghdr *)control;
	    CMSG_VALID(cmsg, control, control + controllen);
	    cmsg = CMSG_NEXT(cmsg)) {
		if (cmsg->cmsg_level == SOL_SOCKET &&
		    cmsg->cmsg_type == SCM_RIGHTS) {
			close_fds(CMSG_CONTENT(cmsg), CMSG_CONTENTLEN(cmsg),
				startoff - sizeof (struct cmsghdr));
		}
		startoff -= cmsg->cmsg_len;
	}
}

/*
 * Returns a pointer/length for the file descriptors contained
 * in the control buffer. Returns with *fdlenp == -1 if there are no
 * file descriptor options present. This is different than there being
 * a zero-length file descriptor option.
 * Fail if there are multiple SCM_RIGHT cmsgs.
 */
int
so_getfdopt(char *control, int controllen, int oldflg, char **fdsp, int *fdlenp)
{
	struct cmsghdr *cmsg;
	char *fds;
	int fdlen;

	if (control == NULL) {
		*fdsp = NULL;
		*fdlenp = -1;
		return (0);
	}

	if (oldflg) {
		*fdsp = control;
		if (controllen == 0)
			*fdlenp = -1;
		else
			*fdlenp = controllen;
		dprint(1, ("so_getfdopt: old %d\n", *fdlenp));
		return (0);
	}

	fds = NULL;
	fdlen = 0;

	for (cmsg = (struct cmsghdr *)control;
	    CMSG_VALID(cmsg, control, control + controllen);
	    cmsg = CMSG_NEXT(cmsg)) {
		if (cmsg->cmsg_level == SOL_SOCKET &&
		    cmsg->cmsg_type == SCM_RIGHTS) {
			if (fds != NULL)
				return (EINVAL);
			fds = CMSG_CONTENT(cmsg);
			fdlen = CMSG_CONTENTLEN(cmsg);
			dprint(1, ("so_getfdopt: new %d\n",
				CMSG_CONTENTLEN(cmsg)));
		}
	}
	if (fds == NULL) {
		dprint(1, ("so_getfdopt: NONE\n"));
		*fdlenp = -1;
	} else
		*fdlenp = fdlen;
	*fdsp = fds;
	return (0);
}

/*
 * Return the length of the options including any file descriptor options.
 */
int
so_optlen(char *control, int controllen, int oldflg)
{
	struct cmsghdr *cmsg;
	int optlen = 0;
	int len;

	if (control == NULL)
		return (0);

	if (oldflg)
		return (sizeof (struct T_opthdr) + fdbuf_optlen(controllen));

	for (cmsg = (struct cmsghdr *)control;
	    CMSG_VALID(cmsg, control, control + controllen);
	    cmsg = CMSG_NEXT(cmsg)) {
		if (cmsg->cmsg_level == SOL_SOCKET &&
		    cmsg->cmsg_type == SCM_RIGHTS) {
			len = fdbuf_optlen(CMSG_CONTENTLEN(cmsg));
		} else {
			len = CMSG_CONTENTLEN(cmsg);
		}
		optlen += ROUNDUP_tpi_opt(len) + sizeof (struct T_opthdr);
	}
	dprint(1, ("so_optlen: controllen %d, flg %d -> optlen %d\n",
		controllen, oldflg, optlen));
	return (optlen);
}

/*
 * Copy options from control to the mblk. Skip any file descriptor options.
 */
void
so_cmsg2opt(char *control, int controllen, int oldflg, mblk_t *mp)
{
	struct T_opthdr toh;
	struct cmsghdr *cmsg;

	if (control == NULL)
		return;

	if (oldflg) {
		/* No real options - caller has handled file descriptors */
		return;
	}
	for (cmsg = (struct cmsghdr *)control;
	    CMSG_VALID(cmsg, control, control + controllen);
	    cmsg = CMSG_NEXT(cmsg)) {
		/*
		 * Note: The caller handles file descriptors prior
		 * to calling this function.
		 */
		int len;

		if (cmsg->cmsg_level == SOL_SOCKET &&
		    cmsg->cmsg_type == SCM_RIGHTS)
			continue;

		len = CMSG_CONTENTLEN(cmsg);
		toh.level = cmsg->cmsg_level;
		toh.name = cmsg->cmsg_type;
		toh.len = len + sizeof (struct T_opthdr);
		toh.status = 0;

		soappendmsg(mp, (char *)&toh, sizeof (toh));
		soappendmsg(mp, CMSG_CONTENT(cmsg), len);
		mp->b_wptr += ROUNDUP_tpi_opt(len) - len;
		ASSERT(mp->b_wptr <= mp->b_datap->db_lim);
	}
}

/*
 * Return the length of the control message derived from the options.
 * Exclude SO_SRCADDR and SO_UNIX_CLOSE options. Include SO_FILEP.
 * When oldflg is set only include SO_FILEP.
 */
int
so_cmsglen(mblk_t *mp, char *opt, int optlen, int oldflg)
{
	int cmsglen = 0;
	struct T_opthdr *tohp;
	int len;

	if (opt == NULL)
		return (0);

	ASSERT(ISALIGNED_tpi_opthdr(opt));

	for (tohp = (struct T_opthdr *)opt; TOPT_VALID(tohp, opt, opt + optlen);
	    tohp = TOPT_NEXT(tohp)) {
		dprint(1, ("so_cmsglen: level 0x%x, name %d, len %d\n",
			tohp->level, tohp->name, tohp->len));
		if (tohp->level == SOL_SOCKET && tohp->name == SO_SRCADDR)
			continue;
		if (tohp->level == SOL_SOCKET && tohp->name == SO_UNIX_CLOSE)
			continue;
		if (tohp->level == SOL_SOCKET && tohp->name == SO_FILEP) {
			struct fdbuf *fdbuf;
			int fdbuflen;

			fdbuf = (struct fdbuf *)TOPT_CONTENT(tohp);
			fdbuflen = TOPT_CONTENTLEN(tohp);

			if (!fdbuf_verify(mp, fdbuf, fdbuflen))
				continue;
			if (oldflg) {
				cmsglen += fdbuf_cmsglen(fdbuflen);
				continue;
			}
			len = fdbuf_cmsglen(fdbuflen);
		} else {
			if (oldflg)
				continue;
			len = TOPT_CONTENTLEN(tohp);
		}
		cmsglen += ROUNDUP_cmsglen(len + sizeof (struct cmsghdr));
	}
	dprint(1, ("so_cmsglen: optlen %d, flg %d -> cmsglen %d\n",
		optlen, oldflg, cmsglen));
	return (cmsglen);
}

/*
 * Copy options from options to the control. Convert SO_FILEP to
 * file descriptors.
 * Returns errno or zero.
 */
int
so_opt2cmsg(mblk_t *mp, char *opt, int optlen, int oldflg,
	char *control, int controllen)
{
	struct T_opthdr *tohp;
	struct cmsghdr *cmsg;
	struct fdbuf *fdbuf;
	int fdbuflen;
	int error;

	cmsg = (struct cmsghdr *)control;

	if (opt == NULL)
		return (0);

	ASSERT(ISALIGNED_tpi_opthdr(opt));

	for (tohp = (struct T_opthdr *)opt; TOPT_VALID(tohp, opt, opt + optlen);
	    tohp = TOPT_NEXT(tohp)) {
		ASSERT((char *)cmsg <= control + controllen);
		dprint(1, ("so_opt2cmsg: level 0x%x, name %d, len %d\n",
			tohp->level, tohp->name, tohp->len));
		if (tohp->level == SOL_SOCKET && tohp->name == SO_SRCADDR)
			continue;
		if (tohp->level == SOL_SOCKET && tohp->name == SO_UNIX_CLOSE)
			continue;
		if (tohp->level == SOL_SOCKET && tohp->name == SO_FILEP) {
			fdbuf = (struct fdbuf *)TOPT_CONTENT(tohp);
			fdbuflen = TOPT_CONTENTLEN(tohp);

			if (!fdbuf_verify(mp, fdbuf, fdbuflen))
				return (EPROTO);
			if (oldflg) {
				error = fdbuf_extract(fdbuf, control,
							controllen);
				if (error != 0)
					return (error);
				continue;
			} else {
				int fdlen;

				fdlen = fdbuf_cmsglen(TOPT_CONTENTLEN(tohp));

				cmsg->cmsg_level = tohp->level;
				cmsg->cmsg_type = SCM_RIGHTS;
				cmsg->cmsg_len = fdlen +
					sizeof (struct cmsghdr);

				error = fdbuf_extract(fdbuf,
						CMSG_CONTENT(cmsg), fdlen);
				if (error != 0)
					return (error);
			}
		} else {
			if (oldflg)
				continue;

			cmsg->cmsg_level = tohp->level;
			cmsg->cmsg_type = tohp->name;
			cmsg->cmsg_len = TOPT_CONTENTLEN(tohp) +
			    sizeof (struct cmsghdr);

			/* copy content to control data part */
			bcopy((char *)&tohp[1], CMSG_CONTENT(cmsg),
				CMSG_CONTENTLEN(cmsg));
		}
		/* move to next CMSG structure! */
		cmsg = CMSG_NEXT(cmsg);
	}
	return (0);
}

/*
 * Extract the SO_SRCADDR option value if present.
 */
void
so_getopt_srcaddr(char *opt, tpi_length_t optlen,
		char **srcp, tpi_length_t *srclenp)
{
	struct T_opthdr		*tohp;

	ASSERT(ISALIGNED_tpi_opthdr(opt));

	ASSERT(srcp != NULL && srclenp != NULL);
	*srcp = NULL;
	*srclenp = 0;

	for (tohp = (struct T_opthdr *)opt; TOPT_VALID(tohp, opt, opt + optlen);
	    tohp = TOPT_NEXT(tohp)) {
		dprint(1, ("so_getopt_srcaddr: level 0x%x, name %d, len %d\n",
			tohp->level, tohp->name, tohp->len));
		if (tohp->level == SOL_SOCKET &&
		    tohp->name == SO_SRCADDR) {
			*srcp = (char *)TOPT_CONTENT(tohp);
			*srclenp = TOPT_CONTENTLEN(tohp);
		}
	}
}

/*
 * Verify if the SO_UNIX_CLOSE option is present.
 */
int
so_getopt_unix_close(char *opt, tpi_length_t optlen)
{
	struct T_opthdr		*tohp;

	ASSERT(ISALIGNED_tpi_opthdr(opt));

	for (tohp = (struct T_opthdr *)opt; TOPT_VALID(tohp, opt, opt + optlen);
	    tohp = TOPT_NEXT(tohp)) {
		dprint(1,
			("so_getopt_unix_close: level 0x%x, name %d, len %d\n",
			tohp->level, tohp->name, tohp->len));
		if (tohp->level == SOL_SOCKET &&
		    tohp->name == SO_UNIX_CLOSE)
			return (1);
	}
	return (0);
}

/*
 * Allocate an M_PROTO message.
 *
 * If allocation fails the behavior depends on sleepflg:
 *	_ALLOC_NOSLEEP	fail immediately
 *	_ALLOC_INTR	sleep for memory until a signal is caught
 *	_ALLOC_SLEEP	sleep forever. Don't return NULL.
 */
mblk_t *
soallocproto(int size, int sleepflg)
{
	mblk_t	*mp;

	/* Round up size for reuse */
	size = MAX(size, 64);
	mp = allocb(size, BPRI_MED);
	if (mp == NULL) {
		int error;	/* Dummy - error not returned to caller */

		switch (sleepflg) {
		case _ALLOC_SLEEP:
			mp = allocb_wait(size, BPRI_MED, STR_NOSIG, &error);
			ASSERT(mp);
			break;
		case _ALLOC_INTR:
			mp = allocb_wait(size, BPRI_MED, 0, &error);
			if (mp == NULL) {
				/* Caught signal while sleeping for memory */
				eprintline(ENOBUFS);
				return (NULL);
			}
			break;
		case _ALLOC_NOSLEEP:
		default:
			eprintline(ENOBUFS);
			return (NULL);
		}
	}
	mp->b_datap->db_type = M_PROTO;
	return (mp);
}

/*
 * Allocate an M_PROTO message with a single component.
 * len is the length of bug. size is the amount to allocate.
 *
 * buf can be NULL with a non-zero len.
 * This results in a bzero'ed chunk being placed the message.
 */
mblk_t *
soallocproto1(char *buf, int len, int size, int sleepflg)
{
	mblk_t	*mp;

	if (size == 0)
		size = len;

	ASSERT(size >= len);
	/* Round up size for reuse */
	size = MAX(size, 64);
	mp = soallocproto(size, sleepflg);
	if (mp == NULL)
		return (NULL);
	mp->b_datap->db_type = M_PROTO;
	if (len != 0) {
		if (buf != NULL)
			bcopy(buf, (char *)mp->b_wptr, len);
		else
			bzero((char *)mp->b_wptr, len);
		mp->b_wptr += len;
	}
	return (mp);
}

/*
 * Append buf/len to mp.
 * The caller has to ensure that there is enough room in the mblk.
 *
 * buf can be NULL with a non-zero len.
 * This results in a bzero'ed chunk being placed the message.
 */
void
soappendmsg(mblk_t *mp, char *buf, int len)
{
	ASSERT(mp);

	if (len != 0) {
		/* Assert for room left */
		ASSERT(mp->b_datap->db_lim - mp->b_wptr >= len);
		if (buf != NULL)
			bcopy(buf, (char *)mp->b_wptr, len);
		else
			bzero((char *)mp->b_wptr, len);
	}
	mp->b_wptr += len;
}

/*
 * Create a message using two kernel buffers.
 * If size is set that will determine the allocation size (e.g. for future
 * soappendmsg calls). If size is zero it is derived from the buffer
 * lengths.
 */
mblk_t *
soallocproto2(char *buf1, int len1, char *buf2, int len2,
    int size, int sleepflg)
{
	mblk_t *mp;

	if (size == 0)
		size = len1 + len2;
	ASSERT(size >= len1 + len2);

	mp = soallocproto1(buf1, len1, size, sleepflg);
	if (mp)
		soappendmsg(mp, buf2, len2);
	return (mp);
}

/*
 * Create a message using three kernel buffers.
 * If size is set that will determine the allocation size (for future
 * soappendmsg calls). If size is zero it is derived from the buffer
 * lengths.
 */
mblk_t *
soallocproto3(char *buf1, int len1, char *buf2, int len2,
		char *buf3, int len3, int size, int sleepflg)
{
	mblk_t *mp;

	if (size == 0)
		size = len1 + len2 +len3;
	ASSERT(size >= len1 + len2 + len3);

	mp = soallocproto1(buf1, len1, size, sleepflg);
	if (mp)
		soappendmsg(mp, buf2, len2);
	if (mp)
		soappendmsg(mp, buf3, len3);
	return (mp);
}

#ifdef DEBUG
char *
pr_state(u_int state, u_int mode)
{
	static char buf[1024];

	buf[0] = 0;
	if (state & SS_ISCONNECTED)
		strcat(buf, "ISCONNECTED ");
	if (state & SS_ISCONNECTING)
		strcat(buf, "ISCONNECTING ");
	if (state & SS_ISDISCONNECTING)
		strcat(buf, "ISDISCONNECTING ");
	if (state & SS_CANTSENDMORE)
		strcat(buf, "CANTSENDMORE ");

	if (state & SS_CANTRCVMORE)
		strcat(buf, "CANTRCVMORE ");
	if (state & SS_ISBOUND)
		strcat(buf, "ISBOUND ");
	if (state & SS_NDELAY)
		strcat(buf, "NDELAY ");
	if (state & SS_NONBLOCK)
		strcat(buf, "NONBLOCK ");

	if (state & SS_ASYNC)
		strcat(buf, "ASYNC ");
	if (state & SS_ACCEPTCONN)
		strcat(buf, "ACCEPTCONN ");
	if (state & SS_HASCONNIND)
		strcat(buf, "HASCONNIND ");
	if (state & SS_SAVEDEOR)
		strcat(buf, "SAVEDEOR ");

	if (state & SS_RCVATMARK)
		strcat(buf, "RCVATMARK ");
	if (state & SS_OOBPEND)
		strcat(buf, "OOBPEND ");
	if (state & SS_HAVEOOBDATA)
		strcat(buf, "HAVEOOBDATA ");
	if (state & SS_HADOOBDATA)
		strcat(buf, "HADOOBDATA ");

	if (state & SS_FADDR_NOXLATE)
		strcat(buf, "FADDR_NOXLATE ");
	if (state & SS_WUNBIND)
		strcat(buf, "WUNBIND ");

	if (mode & SM_PRIV)
		strcat(buf, "PRIV ");
	if (mode & SM_ATOMIC)
		strcat(buf, "ATOMIC ");
	if (mode & SM_ADDR)
		strcat(buf, "ADDR ");
	if (mode & SM_CONNREQUIRED)
		strcat(buf, "CONNREQUIRED ");

	if (mode & SM_FDPASSING)
		strcat(buf, "FDPASSING ");
	if (mode & SM_EXDATA)
		strcat(buf, "EXDATA ");
	if (mode & SM_OPTDATA)
		strcat(buf, "OPTDATA ");
	if (mode & SM_BYTESTREAM)
		strcat(buf, "BYTESTREAM ");
	return (buf);
}

char *
pr_addr(int family, struct sockaddr *addr, int addrlen)
{
	static char buf[1024];

	if (addr == NULL || addrlen == 0) {
		sprintf(buf, "(len %d) 0x%x", addrlen, addr);
		return (buf);
	}
	switch (family) {
	case AF_INET: {
		struct sockaddr_in sin;

		bcopy((char *)addr, (char *)&sin, sizeof (sin));

		sprintf(buf, "(len %d) %x/%d",
			addrlen, ntohl(sin.sin_addr.s_addr),
			ntohs(sin.sin_port));
		break;
	}
	case AF_UNIX: {
		struct sockaddr_un *soun = (struct sockaddr_un *)addr;

		sprintf(buf, "(len %d) %s",
			addrlen,
			(soun == NULL) ? "(none)" : soun->sun_path);
		break;
	}
	default:
		sprintf(buf, "(unknown af %d)", family);
		break;
	}
	return (buf);
}

/* The logical equivalence operator (a if-and-only-if b) */
#define	EQUIV(a, b)	(((a) && (b)) || (!(a) && (!(b))))

/*
 * Verify limitations and invariants on oob state.
 * Return 1 if OK, otherwise 0 so that it can be used as
 *	ASSERT(verify_oobstate(so));
 */
int
so_verify_oobstate(struct sonode *so)
{
	ASSERT(MUTEX_HELD(&so->so_lock));

	/*
	 * The possible state combinations are:
	 *	0
	 *	SS_OOBPEND
	 *	SS_OOBPEND|SS_HAVEOOBDATA
	 *	SS_OOBPEND|SS_HADOOBDATA
	 *	SS_HADOOBDATA
	 */
	switch (so->so_state & (SS_OOBPEND|SS_HAVEOOBDATA|SS_HADOOBDATA)) {
	case 0:
	case SS_OOBPEND:
	case SS_OOBPEND|SS_HAVEOOBDATA:
	case SS_OOBPEND|SS_HADOOBDATA:
	case SS_HADOOBDATA:
		break;
	default:
		printf("Bad oob state 1 (0x%x): counts %d/%d state %s\n",
			so, so->so_oobsigcnt,
			so->so_oobcnt, pr_state(so->so_state, so->so_mode));
		return (0);
	}

	/* SS_RCVATMARK should only be set when SS_OOBPEND is set */
	if ((so->so_state & (SS_RCVATMARK|SS_OOBPEND)) == SS_RCVATMARK) {
		printf("Bad oob state 2 (0x%x): counts %d/%d state %s\n",
			so, so->so_oobsigcnt,
			so->so_oobcnt, pr_state(so->so_state, so->so_mode));
		return (0);
	}

	/*
	 * (so_oobsigcnt != 0 or SS_RCVATMARK) iff SS_OOBPEND
	 */
	if (!EQUIV((so->so_oobsigcnt != 0) || (so->so_state & SS_RCVATMARK),
		so->so_state & SS_OOBPEND)) {
		printf("Bad oob state 3 (0x%x): counts %d/%d state %s\n",
			so, so->so_oobsigcnt,
			so->so_oobcnt, pr_state(so->so_state, so->so_mode));
		return (0);
	}

	/*
	 * Unless SO_OOBINLINE we have so_oobmsg != NULL iff SS_HAVEOOBDATA
	 */
	if (!(so->so_options & SO_OOBINLINE) &&
	    !EQUIV(so->so_oobmsg != NULL, so->so_state & SS_HAVEOOBDATA)) {
		printf("Bad oob state 4 (0x%x): counts %d/%d state %s\n",
			so, so->so_oobsigcnt,
			so->so_oobcnt, pr_state(so->so_state, so->so_mode));
		return (0);
	}
	if (so->so_oobsigcnt < so->so_oobcnt) {
		printf("Bad oob state 5 (0x%x): counts %d/%d state %s\n",
			so, so->so_oobsigcnt,
			so->so_oobcnt, pr_state(so->so_state, so->so_mode));
		return (0);
	}
	return (1);
}
#undef	EQUIV

#endif /* DEBUG */
