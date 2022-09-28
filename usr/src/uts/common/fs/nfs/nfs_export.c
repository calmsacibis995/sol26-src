/*	@(#)nfs_export.c 1.11 88/02/08 SMI	*/

/*
 *		PROPRIETARY NOTICE (Combined)
 *
 *  This source code is unpublished proprietary information
 *  constituting, or derived under license from AT&T's Unix(r) System V.
 *
 *
 *
 *		Copyright Notice
 *
 *  Notice of copyright on this source code product does not indicate
 *  publication.
 *
 *  	(c) 1986, 1987, 1988, 1989, 1995, 1996  Sun Microsystems, Inc.
 *  	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 */

#ident	"@(#)nfs_export.c	1.55	96/10/23 SMI"	/* SVr4.0 1.7	*/

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/file.h>
#include <sys/tiuser.h>
#include <sys/kmem.h>
#include <sys/pathname.h>
#include <sys/debug.h>
#include <sys/vtrace.h>
#include <sys/cmn_err.h>
#include <sys/acl.h>
#include <sys/utsname.h>
#include <netinet/in.h>

#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/svc.h>

#include <nfs/nfs.h>
#include <nfs/export.h>
#include <nfs/nfssys.h>
#include <nfs/nfs_clnt.h>
#include <nfs/nfs_acl.h>


#define	EXPTABLESIZE 16

struct exportinfo *exptable[EXPTABLESIZE];

static int	unexport(fsid_t *, fid_t *);
static int	findexivp(struct exportinfo **, vnode_t *, vnode_t *, cred_t *);
static void	exportfree(struct exportinfo *);
static int	loadindex(struct export *);

extern void	nfsauth_cache_free(struct exportinfo *);
extern int	sec_svc_loadrootnames(int, int, caddr_t **);
extern int	sec_svc_freerootnames(int, int, caddr_t *);

#define	exportmatch(exi, fsid, fid)	\
	(EQFSID(&(exi)->exi_fsid, (fsid)) && EQFID(&(exi)->exi_fid, (fid)))

/*
 * exported_lock	Read/Write lock that protects the exportinfo list.
 *			This lock must be held when searching or modifiying
 *			the exportinfo list.
 */
krwlock_t exported_lock;

/*
 * "public" and default (root) location for public filehandle
 */
struct exportinfo *exi_public, *exi_root;

fhandle_t nullfh2;	/* for comparing V2 filehandles */

#define	exptablehash(fsid, fid) (nfs_fhhash((fsid), (fid)) & (EXPTABLESIZE - 1))

/*
 * File handle hash function, good for producing hash values 16 bits wide.
 */
int
nfs_fhhash(fsid_t *fsid, fid_t *fid)
{
	register short *data;
	register int i, len;
	short h;

	ASSERT(fid != NULL);

	data = (short *)fid->fid_data;

	/* fid_data must be aligned on a short */
	ASSERT((((long)data) & (sizeof (short) - 1)) == 0);

	if (fid->fid_len == 10) {
		/*
		 * probably ufs: hash on bytes 4,5 and 8,9
		 */
		return (fsid->val[0] ^ data[2] ^ data[4]);
	}

	if (fid->fid_len == 6) {
		/*
		 * probably hsfs: hash on bytes 0,1 and 4,5
		 */
		return ((fsid->val[0] ^ data[0] ^ data[2]));
	}

	/*
	 * Some other file system. Assume that every byte is
	 * worth hashing.
	 */
	h = (short)fsid->val[0];

	/*
	 * Sanity check the length before using it
	 * blindly in case the client trashed it.
	 */
	if (fid->fid_len > NFS_FHMAXDATA)
		len = 0;
	else
		len = fid->fid_len / sizeof (short);

	/*
	 * This will ignore one byte if len is not a multiple of
	 * of sizeof (short). No big deal since we at least get some
	 * variation with fsid->val[0];
	 */
	for (i = 0; i < len; i++)
		h ^= data[i];

	return ((int)h);
}

/*
 * Counted byte string compare routine, optimized for file ids.
 */
int
nfs_fhbcmp(char *d1, char *d2, int l)
{
	register int k;

	/*
	 * We are always passed pointers to the data portions of
	 * two fids, where pointers are always 2 bytes from 32 bit
	 * alignment. If the length is also 2 bytes off word alignment,
	 * we can do word compares, because the two bytes before the fid
	 * data are always the length packed into a 16 bit short, so we
	 * can safely start our comparisons at d1-2 and d2-2.
	 * If the length is 2 bytes off word alignment, that probably
	 * means that first two bytes are zeroes. This means that
	 * first word in each fid, including the length are going to be
	 * equal (we wouldn't call fhbcmp if the lengths weren't the
	 * same). Thus it makes the most sense to start comparing the
	 * last words of each data portion.
	 */

	if ((l & 0x3) == 2) {
		/*
		 * We are going move the data pointers to the
		 * last word. Adding just the length, puts us to the
		 * word past end of the data. So reduce length by one
		 * word length.
		 */
		k = l - 4;
		/*
		 * Both adjusted length and the data pointer are offset two
		 * bytes from word alignment. Adding them together gives
		 * us word alignment.
		 */
		d1 += k;
		d2 += k;
		l += 2;
		while (l -= 4) {
			if (*(long *)d1 != *(long *)d2)
				return (1);
			d1 -= 4;
			d2 -= 4;
		}
	} else {
		while (l--) {
			if (*d1++ != *d2++)
				return (1);
		}
	}
	return (0);
}

/*
 * Initialization routine for export routines. Should only be called once.
 */
int
nfs_exportinit(void)
{
	int exporthash;

	rw_init(&exported_lock, "exported_lock", RW_DEFAULT, DEFAULT_WT);

	/*
	 * Allocate the place holder for the public file handle, which
	 * is all zeroes.
	 */
	exi_root = (struct exportinfo *) kmem_zalloc(sizeof (*exi_root),
			KM_SLEEP);
	exi_public = exi_root;

	exi_root->exi_export.ex_flags = EX_PUBLIC;

	/*
	 * Hash for entry (kmem_zalloc() init's fsid and fid to zero)
	 * and place it in table.
	 */
	exporthash = exptablehash(&exi_root->exi_fsid, &exi_root->exi_fid);
	exi_root->exi_hash = exptable[exporthash];
	exptable[exporthash] = exi_root;

	return (0);
}

/*
 *  Check if 2 gss mechanism identifiers are the same.
 *
 *  return FALSE if not the same.
 *  return TRUE if the same.
 */
static bool_t
nfs_mech_equal(mech1, mech2)
	rpc_gss_OID	mech1;
	rpc_gss_OID	mech2;
{
	if ((mech1->length == 0) && (mech2->length == 0))
		return (TRUE);

	if (mech1->length != mech2->length)
		return (FALSE);

	return (bcmp(mech1->elements, mech2->elements, mech1->length) == 0);
}

/*
 *  This routine is used by rpc to map rpc security number
 *  to nfs specific security flavor number.
 *
 *  The gss callback prototype is
 *  callback(struct svc_req *, gss_cred_id_t *, gss_ctx_id_t *,
 *                            rpc_gss_lock_t *, void **),
 *  since nfs does not use the gss_cred_id_t/gss_ctx_id_t arguments
 *  we cast them to void.
 */
/*ARGSUSED*/
bool_t
rfs_gsscallback(struct svc_req *req, void *deleg, void *gss_context,
			rpc_gss_lock_t *lock, void **cookie)
{
	int i, j;
	rpc_gss_rawcred_t *raw_cred;

	raw_cred = lock->raw_cred;
	*cookie = NULL;

	rw_enter(&exported_lock, RW_READER);
	for (i = 0; i < EXPTABLESIZE; i++) {
	    if (exptable[i] && exptable[i]->exi_export.ex_seccnt > 0) {
		struct secinfo *secp;

		secp = exptable[i]->exi_export.ex_secinfo;
		for (j = 0; j < exptable[i]->exi_export.ex_seccnt; j++) {
		/*
		 *  If there is a map of the triplet
		 *  (mechanism, service, qop) between raw_cred and
		 *  the exported flavor, get the psudo flavor number.
		 *  Also qop should not be NULL, it should be "default"
		 *  or something else.
		 */
		    if ((secp[j].s_secinfo.sc_rpcnum == RPCSEC_GSS) &&
			nfs_mech_equal(secp[j].s_secinfo.sc_gss_mech_type,
			raw_cred->mechanism) &&
			(secp[j].s_secinfo.sc_service == raw_cred->service) &&
			(raw_cred->qop == secp[j].s_secinfo.sc_qop)) {
				*cookie = (void *)secp[j].s_secinfo.sc_nfsnum;
				break;
		    }
		}
		if (*cookie)
		    break;
	    }
	}
	rw_exit(&exported_lock);

	if (*cookie) {
	    lock->locked = TRUE;
	    return (TRUE);
	}

	return (FALSE);
}



/*
 * Exportfs system call
 */
int
exportfs(struct exportfs_args *uap, cred_t *cr)
{
	vnode_t *vp;
	struct export *kex;
	struct exportinfo *exi;
	struct exportinfo *ex, *prev;
	fid_t fid;
	fsid_t fsid;
	int error;
	int exporthash;
	int allocsize;
	struct secinfo *sp;
	rpc_gss_callback_t cb;
	char *pathbuf;
	int i, callback;
	u_int vers;

	if (!suser(cr))
		return (EPERM);

	error = lookupname(uap->dname, UIO_USERSPACE, FOLLOW, NULL, &vp);
	if (error)
		return (error);

	/*
	 * 'vp' may be an AUTOFS node, so we perform a
	 * VOP_ACCESS() to trigger the mount of the
	 * intended filesystem, so we can share the intended
	 * filesystem instead of the AUTOFS filesystem.
	 */
	(void) VOP_ACCESS(vp, 0, 0, cr);

	/*
	 * We're interested in the top most filesystem.
	 * This is specially important when uap->dname is a trigger
	 * AUTOFS node, since we're really interested in sharing the
	 * filesystem AUTOFS mounted as result of the VOP_ACCESS()
	 * call not the AUTOFS node itself.
	 */
	if (vp->v_vfsmountedhere != NULL) {
		if (error = traverse(&vp)) {
			VN_RELE(vp);
			return (error);
		}
	}

	/*
	 * Get the vfs id
	 */
	bzero(&fid, sizeof (fid));
	fid.fid_len = MAXFIDSZ;
	error = VOP_FID(vp, &fid);
	fsid = vp->v_vfsp->vfs_fsid;
	VN_RELE(vp);
	if (error) {
		/*
		 * If VOP_FID returns ENOSPC then the fid supplied
		 * is too small.  For now we simply return EREMOTE.
		 */
		if (error == ENOSPC)
			error = EREMOTE;
		return (error);
	}

	if (uap->uex == NULL) {
		error = unexport(&fsid, &fid);
		return (error);
	}
	exi = kmem_zalloc(sizeof (*exi), KM_SLEEP);
	exi->exi_fsid = fsid;
	exi->exi_fid = fid;

	/*
	 * Build up the template fhandle
	 */
	exi->exi_fh.fh_fsid = fsid;
	if (exi->exi_fid.fid_len > sizeof (exi->exi_fh.fh_xdata)) {
		error = EREMOTE;
		goto error_return;
	}
	exi->exi_fh.fh_xlen = exi->exi_fid.fid_len;
	bcopy(exi->exi_fid.fid_data, exi->exi_fh.fh_xdata,
		exi->exi_fid.fid_len);

	exi->exi_fh.fh_len = sizeof (exi->exi_fh.fh_data);

	kex = &exi->exi_export;

	/*
	 * Load in everything, and do sanity checking
	 */
	if (copyin((caddr_t)uap->uex, (caddr_t)kex,
	    (u_int)sizeof (struct export))) {
		error = EFAULT;
		goto error_return;
	}

	/*
	 * Must have at least one security entry
	 */
	if (kex->ex_seccnt < 1) {
		error = EINVAL;
		goto error_return;
	}

	/*
	 * Copy the exported pathname into
	 * an appropriately sized buffer.
	 */
	pathbuf = mem_alloc(MAXPATHLEN);
	if (copyinstr(kex->ex_path, pathbuf, MAXPATHLEN,
		(size_t *)&kex->ex_pathlen)) {
		kmem_free(pathbuf, MAXPATHLEN);
		error = EFAULT;
		goto error_return;
	}
	kex->ex_path = mem_alloc(kex->ex_pathlen + 1);
	bcopy(pathbuf, kex->ex_path, kex->ex_pathlen);
	kex->ex_path[kex->ex_pathlen] = '\0';
	kmem_free(pathbuf, MAXPATHLEN);

	/*
	 * Initialize auth cache lock
	 */
	rw_init(&exi->exi_cache_lock, "exi_cache_lock", RW_DEFAULT, DEFAULT_WT);

	/*
	 * Load the security information for each flavor
	 */
	allocsize = kex->ex_seccnt * sizeof (struct secinfo);
	sp = kmem_zalloc(allocsize, KM_SLEEP);
	if (copyin((caddr_t)kex->ex_secinfo, (caddr_t)sp, allocsize)) {
		kmem_free(sp, allocsize);
		error = EFAULT;
		goto error_return;
	}

	/*
	 * And now copy rootnames for each individual secinfo.
	 */
	callback = 0;
	for (i = 0; i < kex->ex_seccnt; i++) {
		struct secinfo *exs;
		bool_t set_svc_flag;

		exs = &sp[i];
		if (exs->s_rootcnt > 0) {
			if (!sec_svc_loadrootnames(exs->s_secinfo.sc_rpcnum,
				exs->s_rootcnt, &exs->s_rootnames)) {
				error = EFAULT;
				kmem_free(sp, allocsize);
				goto error_return;
			}
		}

		if (exs->s_secinfo.sc_rpcnum == RPCSEC_GSS) {
		    char svcname[MAX_GSS_NAME];
		    rpc_gss_OID	mech_tmp;
		    caddr_t		elements_tmp;

		    /* Copyin mechanism type */
		    mech_tmp = kmem_alloc(sizeof (*mech_tmp), KM_SLEEP);
		    if (copyin(exs->s_secinfo.sc_gss_mech_type, mech_tmp,
					sizeof (*mech_tmp))) {
			kmem_free(mech_tmp, sizeof (*mech_tmp));
			kmem_free(sp, allocsize);
			error = EFAULT;
			goto error_return;
		    }
		    elements_tmp = kmem_alloc(mech_tmp->length, KM_SLEEP);
		    if (copyin(mech_tmp->elements, elements_tmp,
				mech_tmp->length)) {
			kmem_free(elements_tmp, mech_tmp->length);
			kmem_free(mech_tmp, sizeof (*mech_tmp));
			kmem_free(sp, allocsize);
			error = EFAULT;
			goto error_return;
		    }
		    mech_tmp->elements = elements_tmp;
		    exs->s_secinfo.sc_gss_mech_type = mech_tmp;

		    /* Set service information */
		    set_svc_flag = FALSE;
		    sprintf(svcname, "nfs@%s", utsname.nodename);
		    for (vers = NFS_ACL_VERSMIN; vers <= NFS_ACL_VERSMAX;
			vers++) {
			if (rpc_gss_set_svc_name(svcname,
				exs->s_secinfo.sc_gss_mech_type, 0,
				NFS_ACL_PROGRAM, vers)) {
				set_svc_flag = TRUE;
			}
		    }
		    for (vers = NFS_VERSMIN; vers <= NFS_VERSMAX; vers++) {
			if (rpc_gss_set_svc_name(svcname,
				exs->s_secinfo.sc_gss_mech_type, 0,
				NFS_PROGRAM, vers)) {
				set_svc_flag = TRUE;
			}
		    }
		    if (!set_svc_flag) {
			cmn_err(CE_NOTE,
				"exportfs: set gss service %s failed",
				svcname);
			error = EINVAL;
			goto error_return;
		    }

		    callback = 1;
		}
	}
	kex->ex_secinfo = sp;

	/*
	 *  Set up rpcsec_gss callback routine entry if any.
	 */
	if (callback) {
	    cb.callback = rfs_gsscallback;
	    cb.program = NFS_ACL_PROGRAM;
	    for (cb.version = NFS_ACL_VERSMIN; cb.version <= NFS_ACL_VERSMAX;
			cb.version++) {
		sec_svc_control(RPC_SVC_SET_GSS_CALLBACK, (void *)&cb);
	    }

	    cb.program = NFS_PROGRAM;
	    for (cb.version = NFS_VERSMIN; cb.version <= NFS_VERSMAX;
			cb.version++) {
		sec_svc_control(RPC_SVC_SET_GSS_CALLBACK, (void *)&cb);
	    }
	}

	/*
	 * Check the index flag. Do this here to avoid holding the
	 * lock while dealing with the index option (as we do with
	 * the public option).
	 */
	if (kex->ex_flags & EX_INDEX) {
		if (!kex->ex_index) {	/* sanity check */
			error = EINVAL;
			goto error_return;
		}
		if (error = loadindex(kex))
			goto error_return;
	}

	/*
	 * Insert the new entry at the front of the export list
	 */
	rw_enter(&exported_lock, RW_WRITER);
	exporthash = exptablehash(&exi->exi_fsid, &exi->exi_fid);
	exi->exi_hash = exptable[exporthash];
	exptable[exporthash] = exi;

	/*
	 * Check the rest of the list for an old entry for the fs.
	 * If one is found then unlink it, wait until this is the
	 * only reference and then free it.
	 */
	prev = exi;
	for (ex = prev->exi_hash; ex; prev = ex, ex = ex->exi_hash) {
		if (exportmatch(ex, &exi->exi_fsid, &exi->exi_fid)) {
			prev->exi_hash = ex->exi_hash;
			break;
		}
	}

	/*
	 * If the public filehandle is pointing at the
	 * old entry, then point it back at the root.
	 */
	if (ex && ex == exi_public)
		exi_public = exi_root;

	/*
	 * If the public flag is on, make the global exi_public
	 * point to this entry and turn off the public bit so that
	 * we can distinguish it from the place holder export.
	 */
	if (kex->ex_flags & EX_PUBLIC) {
		exi_public = exi;
		kex->ex_flags &= ~EX_PUBLIC;
	}

	rw_exit(&exported_lock);
	if (ex) {
		exportfree(ex);
	}
	return (0);

error_return:
	kmem_free((char *)exi, sizeof (*exi));
	return (error);
}


/*
 * Remove the exported directory from the export list
 */
static int
unexport(fsid_t *fsid, fid_t *fid)
{
	struct exportinfo **tail;
	struct exportinfo *exi;

	rw_enter(&exported_lock, RW_WRITER);
	tail = &exptable[exptablehash(fsid, fid)];
	while (*tail != NULL) {
		if (exportmatch(*tail, fsid, fid)) {
			exi = *tail;
			*tail = (*tail)->exi_hash;

			/*
			 * If this was a public export, restore
			 * the public filehandle to the root.
			 */
			if (exi == exi_public)
				exi_public = exi_root;

			rw_exit(&exported_lock);
			exportfree(exi);
			return (0);
		}
		tail = &(*tail)->exi_hash;
	}
	rw_exit(&exported_lock);
	return (EINVAL);
}

/*
 * Get file handle system call.
 * Takes file name and returns a file handle for it.
 */
int
nfs_getfh(struct nfs_getfh_args *uap, cred_t *cr)
{
	fhandle_t fh;
	vnode_t *vp;
	vnode_t *dvp;
	struct exportinfo *exi;
	int error;

	if (!suser(cr))
		return (EPERM);

	error = lookupname(uap->fname, UIO_USERSPACE, FOLLOW, &dvp, &vp);
	if (error == EINVAL) {
		/*
		 * if fname resolves to / we get EINVAL error
		 * since we wanted the parent vnode. Try again
		 * with NULL dvp.
		 */
		error = lookupname(uap->fname, UIO_USERSPACE, FOLLOW, NULL,
				    &vp);
		dvp = NULL;
	}
	if (!error && vp == NULL) {
		/*
		 * Last component of fname not found
		 */
		if (dvp) {
			VN_RELE(dvp);
		}
		error = ENOENT;
	}
	if (error)
		return (error);

	/*
	 * 'vp' may be an AUTOFS node, so we perform a
	 * VOP_ACCESS() to trigger the mount of the
	 * intended filesystem, so we can share the intended
	 * filesystem instead of the AUTOFS filesystem.
	 */
	(void) VOP_ACCESS(vp, 0, 0, cr);

	/*
	 * We're interested in the top most filesystem.
	 * This is specially important when uap->dname is a trigger
	 * AUTOFS node, since we're really interested in sharing the
	 * filesystem AUTOFS mounted as result of the VOP_ACCESS()
	 * call not the AUTOFS node itself.
	 */
	if (vp->v_vfsmountedhere != NULL) {
		if (error = traverse(&vp)) {
			VN_RELE(vp);
			return (error);
		}
	}

	error = findexivp(&exi, dvp, vp, cr);
	if (!error) {
		error = makefh(&fh, vp, exi);
		rw_exit(&exported_lock);
		if (!error) {
			if (copyout((caddr_t)&fh, (caddr_t)uap->fhp,
			    sizeof (fh)))
				error = EFAULT;
		}
	}
	VN_RELE(vp);
	if (dvp != NULL) {
		VN_RELE(dvp);
	}
	return (error);
}

/*
 * Strategy: if vp is in the export list, then
 * return the associated file handle. Otherwise, ".."
 * once up the vp and try again, until the root of the
 * filesystem is reached.
 */
static int
findexivp(struct exportinfo **exip, vnode_t *dvp, vnode_t *vp, cred_t *cr)
{
	fid_t fid;
	int error;

	VN_HOLD(vp);
	if (dvp != NULL) {
		VN_HOLD(dvp);
	}
	for (;;) {
		bzero((char *)&fid, sizeof (fid));
		fid.fid_len = MAXFIDSZ;
		error = VOP_FID(vp, &fid);
		if (error) {
			/*
			 * If VOP_FID returns ENOSPC then the fid supplied
			 * is too small.  For now we simply return EREMOTE.
			 */
			if (error == ENOSPC)
				error = EREMOTE;
			break;
		}
		*exip = findexport(&vp->v_vfsp->vfs_fsid, &fid);
		if (*exip != NULL) {
			/*
			 * Found the export info
			 */
			break;
		}

		/*
		 * We have just failed finding a matching export.
		 * If we're at the root of this filesystem, then
		 * it's time to stop (with failure).
		 */
		if (vp->v_flag & VROOT) {
			error = EINVAL;
			break;
		}

		/*
		 * Now, do a ".." up vp. If dvp is supplied, use it,
		 * otherwise, look it up.
		 */
		if (dvp == NULL) {
			error = VOP_LOOKUP(vp, "..", &dvp, NULL, 0, NULL, cr);
			if (error)
				break;
		}
		VN_RELE(vp);
		vp = dvp;
		dvp = NULL;
	}
	VN_RELE(vp);
	if (dvp != NULL) {
		VN_RELE(dvp);
	}
	return (error);
}
/*
 * Return exportinfo struct for a given vnode.
 * Like findexivp, but it uses checkexport()
 * since it is called with the export lock held.
 */
struct   exportinfo *
nfs_vptoexi(vnode_t *dvp, vnode_t *vp, cred_t *cr, int *walk)
{
	fid_t fid;
	int error;
	struct exportinfo *exi;

	ASSERT(vp);
	VN_HOLD(vp);
	if (dvp != NULL) {
		VN_HOLD(dvp);
	}
	*walk = 0;

	for (;;) {
		bzero((char *)&fid, sizeof (fid));
		fid.fid_len = MAXFIDSZ;
		error = VOP_FID(vp, &fid);
		if (error) {
			break;
		}
		exi = checkexport(&vp->v_vfsp->vfs_fsid, &fid);
		if (exi != NULL) {
			/*
			 * Found the export info
			 */
			break;
		}

		/*
		 * We have just failed finding a matching export.
		 * If we're at the root of this filesystem, then
		 * it's time to stop (with failure).
		 */
		if (vp->v_flag & VROOT) {
			error = EINVAL;
			break;
		}

		(*walk)++;

		/*
		 * Now, do a ".." up vp. If dvp is supplied, use it,
		 * otherwise, look it up.
		 */
		if (dvp == NULL) {
			error = VOP_LOOKUP(vp, "..", &dvp, NULL, 0, NULL, cr);
			if (error)
				break;
		}
		VN_RELE(vp);
		vp = dvp;
		dvp = NULL;
	}
	VN_RELE(vp);
	if (dvp != NULL) {
		VN_RELE(dvp);
	}
	if (error != 0)
		return (NULL);
	else
		return (exi);
}

/*
 * Make an fhandle from a vnode
 */
int
makefh(fhandle_t *fh, vnode_t *vp, struct exportinfo *exi)
{
	int error;

	ASSERT(RW_READ_HELD(&exported_lock));

	*fh = exi->exi_fh;	/* struct copy */

	error = VOP_FID(vp, (fid_t *)&fh->fh_len);
	if (error) {
		/*
		 * Should be something other than EREMOTE
		 */
		return (EREMOTE);
	}
	return (0);
}

/*
 * Make an nfs_fh3 from a vnode
 */
int
makefh3(nfs_fh3 *fh, vnode_t *vp, struct exportinfo *exi)
{
	int error;

	ASSERT(RW_READ_HELD(&exported_lock));

	fh->fh3_length = sizeof (fh->fh3_u.nfs_fh3_i);
	fh->fh3_u.nfs_fh3_i.fh3_i = exi->exi_fh;	/* struct copy */

	error = VOP_FID(vp, (fid_t *)&fh->fh3_len);

	if (error) {
		/*
		 * Should be something other than EREMOTE
		 */
		return (EREMOTE);
	}
	return (0);
}

/*
 * Convert an fhandle into a vnode.
 * Uses the file id (fh_len + fh_data) in the fhandle to get the vnode.
 * WARNING: users of this routine must do a VN_RELE on the vnode when they
 * are done with it.
 */
vnode_t *
nfs_fhtovp(fhandle_t *fh, struct exportinfo *exi)
{
	register vfs_t *vfsp;
	vnode_t *vp;
	int error;
	fsid_t *fsidp;
	fid_t *fidp;

	TRACE_0(TR_FAC_NFS, TR_FHTOVP_START,
		"fhtovp_start");

	if (exi == NULL) {
		TRACE_1(TR_FAC_NFS, TR_FHTOVP_END,
			"fhtovp_end:(%S)", "exi NULL");
		return (NULL);	/* not exported */
	}

	/*
	 * If it's a public filehandle (all zeros) then
	 * get the export fsid and fid from the exportinfo.
	 */
	if (PUBLIC_FH2(fh, exi)) {
		fsidp = &exi->exi_fsid;
		fidp = &exi->exi_fid;
	} else {
		fsidp = &fh->fh_fsid;
		fidp = (fid_t *)&fh->fh_len;
	}

	vfsp = getvfs(fsidp);
	if (vfsp == NULL) {
		TRACE_1(TR_FAC_NFS, TR_FHTOVP_END,
			"fhtovp_end:(%S)", "getvfs NULL");
		return (NULL);
	}
	error = VFS_VGET(vfsp, &vp, fidp);
	if (error || vp == NULL) {
		TRACE_1(TR_FAC_NFS, TR_FHTOVP_END,
			"fhtovp_end:(%S)", "VFS_GET failed or vp NULL");
		return (NULL);
	}
	TRACE_1(TR_FAC_NFS, TR_FHTOVP_END,
		"fhtovp_end:(%S)", "end");
	return (vp);
}

/*
 * Convert an nfs_fh3 into a vnode.
 * Uses the file id (fh_len + fh_data) in the file handle to get the vnode.
 * WARNING: users of this routine must do a VN_RELE on the vnode when they
 * are done with it.
 */
vnode_t *
nfs3_fhtovp(nfs_fh3 *fh, struct exportinfo *exi)
{
	register vfs_t *vfsp;
	vnode_t *vp;
	int error;
	fsid_t *fsidp;
	fid_t *fidp;

	if (exi == NULL)
		return (NULL);	/* not exported */

	/*
	 * If it's a public filehandle (zero length) then
	 * get the export fsid and fid from the exportinfo.
	 */
	if (PUBLIC_FH3(fh, exi)) {
		fsidp = &exi->exi_fsid;
		fidp = &exi->exi_fid;
	} else {
		if (fh->fh3_length != NFS3_CURFHSIZE)
			return (NULL);

		fsidp = &fh->fh3_fsid;
		fidp = (fid_t *)&fh->fh3_len;
	}

	vfsp = getvfs(fsidp);
	if (vfsp == NULL)
		return (NULL);

	error = VFS_VGET(vfsp, &vp, fidp);
	if (error || vp == NULL)
		return (NULL);

	return (vp);
}

/*
 * Find the export structure associated with the given filesystem
 * If found, then the read lock on the exports list is left to
 * indicate that an entry is still busy.
 */
/*
 * findexport() is split into findexport() and checkexport() to fix
 * 1177604 so that checkexport() can be called by procedures that had
 * already obtained exported_lock to check the exptable.
 */
struct exportinfo *
checkexport(fsid_t *fsid, fid_t *fid)
{
	struct exportinfo *exi;

	ASSERT(RW_READ_HELD(&exported_lock));
	for (exi = exptable[exptablehash(fsid, fid)];
	    exi != NULL;
	    exi = exi->exi_hash) {
		if (exportmatch(exi, fsid, fid)) {
			/*
			 * If this is the place holder for the
			 * public file handle, then return the
			 * real export entry for the public file
			 * handle.
			 */
			if (exi->exi_export.ex_flags & EX_PUBLIC)
				exi = exi_public;

			return (exi);
		}
	}
	return (NULL);
}

struct exportinfo *
findexport(fsid_t *fsid, fid_t *fid)
{
	struct exportinfo *exi;

	rw_enter(&exported_lock, RW_READER);
	if ((exi = checkexport(fsid, fid)) != NULL) {
		return (exi);
	}
	rw_exit(&exported_lock);
	return (NULL);
}

/*
 * Free an entire export list node
 */
static void
exportfree(struct exportinfo *exi)
{
	register int i;
	struct export *ex;

	ex = &exi->exi_export;

	if (ex->ex_flags & EX_INDEX) {
		kmem_free((void *)ex->ex_index, strlen(ex->ex_index) + 1);
	}

	kmem_free(ex->ex_path, ex->ex_pathlen + 1);
	nfsauth_cache_free(exi);

	for (i = 0; i < ex->ex_seccnt; i++) {
		struct secinfo *secp;

		secp = &ex->ex_secinfo[i];
		if (secp->s_rootcnt > 0) {
		    if (secp->s_rootnames != NULL) {
			sec_svc_freerootnames(secp->s_secinfo.sc_rpcnum,
				secp->s_rootcnt, secp->s_rootnames);
		    }
		}

		if ((secp->s_secinfo.sc_rpcnum == RPCSEC_GSS) &&
			(secp->s_secinfo.sc_gss_mech_type)) {
		    kmem_free(secp->s_secinfo.sc_gss_mech_type->elements,
				secp->s_secinfo.sc_gss_mech_type->length);
		    kmem_free(secp->s_secinfo.sc_gss_mech_type,
				sizeof (rpc_gss_OID_desc));
		}
	}
	if (ex->ex_secinfo) {
	    kmem_free((char *)ex->ex_secinfo,
		ex->ex_seccnt * sizeof (struct secinfo));
	}

	kmem_free((char *)exi, sizeof (*exi));
}

/*
 * Free the export lock
 */
void
export_rw_exit(void)
{

	rw_exit(&exported_lock);
}

/*
 * load the index file from user space into kernel space.
 */
static int
loadindex(struct export *kex)
{
	int error;
	char index[MAXNAMELEN+1];
	u_int len;

	/*
	 * copyinstr copies the complete string including the NULL and
	 * returns the len with the NULL byte included in the calculation
	 * as long as the max length is not exceeded.
	 */
	if (error = copyinstr(kex->ex_index, index, sizeof (index), &len))
		return (error);

	kex->ex_index = (char *) kmem_alloc(len, KM_SLEEP);
	bcopy(index, kex->ex_index, len);

	return (0);
}
