/*
 * Copyright (c) 1996 Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)share.c 1.5     96/06/24 SMI"

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fcntl.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/share.h>
#include <sys/cmn_err.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/t_lock.h>
#include <sys/errno.h>

#ifdef DEBUG
int share_debug = 0;
static void print_shares(struct vnode *);
static void print_share(struct shrlock *);
#endif

static int isreadonly(struct vnode *);

int
add_share(struct vnode *vp, struct shrlock *shr)
{
	struct shrlocklist *shrl;

	/*
	 * Sanity check to make sure we have valid options.
	 * There is known overlap but it doesn't hurt to be careful.
	 */
	if ((shr->access == 0) || (shr->access & ~(F_RDACC|F_WRACC|F_RWACC))) {
		return (EINVAL);
	}
	if (shr->deny & ~(F_NODNY|F_RDDNY|F_WRDNY|F_RWDNY|F_COMPAT)) {
		return (EINVAL);
	}

	mutex_enter(&vp->v_lock);
	for (shrl = vp->v_shrlocks; shrl != NULL; shrl = shrl->next) {
		/*
		 * If the share owner matches previous request
		 * do special handling.
		 */
		if ((shrl->shr->sysid == shr->sysid) &&
			(shrl->shr->pid == shr->pid) &&
			(shrl->shr->own_len == shr->own_len) &&
			bcmp(shrl->shr->owner, shr->owner, shr->own_len) == 0) {

			/*
			 * If the exact same mode and access allow it
			 */
			if ((shrl->shr->access == shr->access) &&
			    (shrl->shr->deny == shr->deny)) {
				mutex_exit(&vp->v_lock);
				return (EEXIST);
			}
			/*
			 * If the existing request is F_COMPAT and
			 * is the first share then allow any F_COMPAT
			 * from the same process.  Trick:  If the existing
			 * F_COMPAT is write access then it must have
			 * the same owner as the first.
			 */
			if ((shrl->shr->deny & F_COMPAT) &&
			    (shr->deny & F_COMPAT) &&
			    ((shrl->next == NULL) ||
				(shrl->shr->access & F_WRACC)))
				break;
		}

		/*
		 * If a first share has been done in compatibility mode
		 * handle the special cases.
		 */
		if ((shrl->shr->deny & F_COMPAT) &&
				(shrl->next == NULL)) {

			if (!(shr->deny & F_COMPAT)) {
				/*
				 * If not compat and want write access or
				 * want to deny read or
				 * write exists, fails
				 */
				if ((shr->access & F_WRACC) ||
						(shr->deny & F_RDDNY) ||
						(shrl->shr->access & F_WRACC)) {
					mutex_exit(&vp->v_lock);
					return (EAGAIN);
				}
				/*
				 * If read only file allow, this may allow
				 * a deny write but that is meaningless on
				 * a read only file.
				 */
				if (isreadonly(vp))
					break;
				mutex_exit(&vp->v_lock);
				return (EAGAIN);
			}
			/*
			 * This is a compat request and read access
			 * and the first was also read access
			 * we always allow it, otherwise we reject because
			 * we have handled the only valid write case above.
			 */
			if ((shr->access == F_RDACC) &&
				(shrl->shr->access == F_RDACC))
				break;
			mutex_exit(&vp->v_lock);
			return (EAGAIN);
		}

		/*
		 * If we are trying to share in compatibility mode
		 * and the current share is compat (and not the first)
		 * we don't know enough.
		 */
		if ((shrl->shr->deny & F_COMPAT) && (shr->deny & F_COMPAT))
			continue;

		/*
		 * If this is a compat we check for what can't succeed.
		 */
		if (shr->deny & F_COMPAT) {
			/*
			 * If we want write access or
			 * if anyone is denying read or
			 * if anyone has write access we fail
			 */
			if ((shr->access & F_WRACC) ||
			    (shrl->shr->deny & F_RDDNY) ||
			    (shrl->shr->access & F_WRACC)) {
				mutex_exit(&vp->v_lock);
				return (EAGAIN);
			}
			/*
			 * If the first was opened with only read access
			 * and is a read only file we allow.
			 */
			if (shrl->next == NULL) {
				if ((shrl->shr->access == F_RDACC) &&
						isreadonly(vp)) {
					break;
				}
				mutex_exit(&vp->v_lock);
				return (EAGAIN);
			}
			/*
			 * We still can't determine our fate so continue
			 */
			continue;
		}

		/*
		 * Simple bitwise test, if we are trying to access what
		 * someone else is denying or we are trying to deny
		 * what someone else is accessing we fail.
		 */
		if ((shr->access & shrl->shr->deny) ||
			(shr->deny & shrl->shr->access)) {
			mutex_exit(&vp->v_lock);
			return (EAGAIN);
		}
	}

	shrl = kmem_alloc(sizeof (struct shrlocklist), KM_SLEEP);
	shrl->shr = kmem_alloc(sizeof (struct shrlock), KM_SLEEP);
	shrl->shr->access = shr->access;
	shrl->shr->deny = shr->deny;

	/*
	 * Make sure no other deny modes are also set with F_COMPAT
	 */
	if (shrl->shr->deny & F_COMPAT)
		shrl->shr->deny = F_COMPAT;
	shrl->shr->sysid = shr->sysid;		/* XXX ref cnt? */
	shrl->shr->pid = shr->pid;
	shrl->shr->own_len = shr->own_len;
	shrl->shr->owner = kmem_alloc(shr->own_len, KM_SLEEP);
	bcopy(shr->owner, shrl->shr->owner, shr->own_len);
	shrl->next = vp->v_shrlocks;
	vp->v_shrlocks = shrl;
#ifdef DEBUG
	if (share_debug)
		print_shares(vp);
#endif
	mutex_exit(&vp->v_lock);
	return (0);
}

int
del_share(struct vnode *vp, struct shrlock *shr)
{
	struct shrlocklist *shrl;
	struct shrlocklist **shrlp;
	int found = 0;

	mutex_enter(&vp->v_lock);
	/*
	 * Delete the shares with the matching sysid and owner
	 * But if own_len == 0 and sysid == 0 delete all with matching pid
	 * But if own_len == 0 delete all with matching sysid.
	 */
	shrlp = &vp->v_shrlocks;
	while (*shrlp) {
		if ((shr->own_len == (*shrlp)->shr->own_len &&
				    (bcmp(shr->owner, (*shrlp)->shr->owner,
						shr->own_len) == 0)) ||
			(shr->own_len == 0 &&
			    ((shr->sysid == 0 &&
				shr->pid == (*shrlp)->shr->pid) ||
			    (shr->sysid != 0 &&
				shr->sysid == (*shrlp)->shr->sysid)))) {
			shrl = *shrlp;
			*shrlp = shrl->next;

			/* XXX deref sysid */
			kmem_free(shrl->shr->owner, shrl->shr->own_len);
			kmem_free(shrl->shr, sizeof (struct shrlock));
			kmem_free(shrl, sizeof (struct shrlocklist));
			found++;
			continue;
		}
		shrlp = &(*shrlp)->next;
	}

	mutex_exit(&vp->v_lock);
	return (found ? 0 : EINVAL);
}

/*
 * Clean up all local share reservations
 */
void
cleanshares(struct vnode *vp, pid_t pid)
{
	struct shrlock shr;

	if (vp->v_shrlocks == NULL)
		return;

	shr.access = 0;
	shr.deny = 0;
	shr.pid = pid;
	shr.sysid = 0;
	shr.own_len = 0;
	shr.owner = NULL;

	(void) del_share(vp, &shr);
}

/*
 * Determine whether there are any shares for the given vnode
 * with a remote sysid. Returns zero if not, non-zero if there are.
 * If sysid is non-zero then determine if this sysid has a share.
 *
 * Note that the return value from this function is potentially invalid
 * once it has been returned.  The caller is responsible for providing its
 * own synchronization mechanism to ensure that the return value is useful.
 */
int
shr_has_remote_shares(vnode_t *vp, sysid_t sysid)
{
	struct shrlocklist *shrl;
	int result = 0;

	mutex_enter(&vp->v_lock);
	shrl = vp->v_shrlocks;
	while (shrl) {
		if ((sysid != 0 && shrl->shr->sysid == sysid) ||
		    (sysid == 0 && shrl->shr->sysid != 0)) {
			result = 1;
			break;
		}
		shrl = shrl->next;
	}
	mutex_exit(&vp->v_lock);
	return (result);
}

static int
isreadonly(struct vnode *vp)
{
	return (vp->v_type != VCHR && vp->v_type != VBLK &&
		vp->v_type != VFIFO && (vp->v_vfsp->vfs_flag & VFS_RDONLY));
}

#ifdef DEBUG
static void
print_shares(struct vnode *vp)
{
	struct shrlocklist *shrl;

	if (vp->v_shrlocks == NULL) {
		printf("<NULL>\n");
		return;
	}

	shrl = vp->v_shrlocks;
	while (shrl) {
		print_share(shrl->shr);
		shrl = shrl->next;
	}
}

static void
print_share(struct shrlock *shr)
{
	int i;

	if (shr == NULL) {
		printf("<NULL>\n");
		return;
	}

	printf("    access(%d):	", shr->access);
	if (shr->access & F_RDACC)
		printf("R");
	if (shr->access & F_WRACC)
		printf("W");
	if ((shr->access & F_RDACC|F_WRACC) == 0)
		printf("N");
	printf("\n");
	printf("    deny:	");
	if (shr->deny & F_COMPAT)
		printf("C");
	if (shr->deny & F_RDDNY)
		printf("R");
	if (shr->deny & F_WRDNY)
		printf("W");
	if (shr->deny == F_NODNY)
		printf("N");
	printf("\n");
	printf("    sysid:	%d\n", shr->sysid);
	printf("    pid:	%d\n", shr->pid);
	printf("    owner:	[%d]", shr->own_len);
	printf("'");
	for (i = 0; i < shr->own_len; i++)
		printf("%02x", (unsigned)shr->owner[i]);
	printf("'\n");
}
#endif
