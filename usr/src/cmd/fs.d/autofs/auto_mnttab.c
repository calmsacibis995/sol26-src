/*
 *	auto_mnttab.c
 *
 *	Copyright (c) 1988-1996 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)auto_mnttab.c	1.20	96/07/02 SMI"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <locale.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mnttab.h>
#include <sys/mntent.h>
#include <sys/signal.h>
#include <synch.h>
#include "automount.h"

/*
 * The mnttab_lock is needed since record locking doesn't prevent threads
 * of the same process from overwriting each others changes to /etc/mnttab.
 * We still need to 'lock(/etc/mnttab)' to synchonize with other
 * processes modifying it.
 */
static mutex_t mnttab_lock = DEFAULTMUTEX;

void
del_mnttab(mnt)
	struct mnttab *mnt;
{
	(void) mutex_lock(&mnttab_lock);
	(void) fsrmfrommtab(mnt);
	(void) mutex_unlock(&mnttab_lock);
}


#define	TIME_MAX 16
#define	RET_ERR  1

/*
 * Can't use the equivalent routine in ../fslib.c because it will always
 * stat the given mountpoint to get its devid which will cause us to deadlock.
 * Add a new entry to the /etc/mnttab file.
 * Include the device id with the mount options.
 */
int
add_mnttab(mnt, devid)
	struct mnttab *mnt;
	dev_t devid;
{
	FILE *fd;
	char tbuf[TIME_MAX];
	char *opts;
	struct stat st;
	char obuff[256], *pb = obuff;
	int mlock;
	flock_t flock;

	/*
	 * Do the stat only if no devid was provided.
	 */
	if (devid == 0) {
		if (stat(mnt->mnt_mountp, &st) < 0) {
			pr_msg("add_mnttab: can't add entry %s: stat error: %m",
				mnt->mnt_mountp);
			return (ENOENT);
		}
		devid = st.st_dev;
	}

	(void) mutex_lock(&mnttab_lock);

	mlock = fslock_mnttab();
	fd = fopen(MNTTAB, "a");
	if (fd == NULL) {
		pr_msg("add_mnttab: can't add entry %s: fopen %s failed: %m",
			mnt->mnt_mountp, MNTTAB);
		fsunlock_mnttab(mlock);
		(void) mutex_unlock(&mnttab_lock);
		return (RET_ERR);
	}

	(void) memset((void *) &flock, 0, sizeof (flock));
	flock.l_type = F_WRLCK;
	if (fcntl(fileno(fd), F_SETLKW, &flock) == -1) {
		pr_msg("add_mnttab: can't add entry %s: lock %s error: %m",
			mnt->mnt_mountp, MNTTAB);
		(void) fclose(fd);
		fsunlock_mnttab(mlock);
		(void) mutex_unlock(&mnttab_lock);
		return (RET_ERR);
	}

	opts = mnt->mnt_mntopts;
	if (opts && *opts) {
		(void) strcpy(pb, opts);
		trim(pb);
		pb += strlen(pb);
		*pb++ = ',';
	}

	(void) sprintf(pb, "%s=%lx", MNTOPT_DEV, devid);
	mnt->mnt_mntopts = obuff;

	(void) sprintf(tbuf, "%ld", time(0L));
	mnt->mnt_time = tbuf;

	(void) fseek(fd, 0L, SEEK_END); /* guarantee at EOF */

	putmntent(fd, mnt);

	(void) fclose(fd);
	fsunlock_mnttab(mlock);
	(void) mutex_unlock(&mnttab_lock);
	mnt->mnt_mntopts = opts;
	return (0);
}

/*
 * Replace an existing entry with a new one - a remount.
 * Need to keep this here because of the get_devid() call.
 */
void
fix_mnttab(mnt)
	struct mnttab *mnt;
{
	FILE *mnttab;
	struct mntlist *mntl_head;
	struct mntlist *found;
	struct mnttab find_mnt;
	char *opts;
	char tbuf[TIME_MAX];
	char *newopts, *pn;
	int mlock;
	flock_t flock;

	(void) mutex_lock(&mnttab_lock);

	mlock = fslock_mnttab();
	mnttab = fopen(MNTTAB, "r+");
	if (mnttab == NULL) {
		pr_msg("fix_mnttab: can't modify entry %s: fopen %s failed: %m",
			mnt->mnt_mountp, MNTTAB);
		fsunlock_mnttab(mlock);
		(void) mutex_unlock(&mnttab_lock);
		return;
	}
	(void) memset((void *) &flock, 0, sizeof (flock));
	flock.l_type = F_WRLCK;
	if (fcntl(fileno(mnttab), F_SETLKW, &flock) == -1) {
		pr_msg("fix_mnttab: can't modify entry %s: lock %s error: %m",
			mnt->mnt_mountp, MNTTAB);
		(void) fclose(mnttab);
		fsunlock_mnttab(mlock);
		(void) mutex_unlock(&mnttab_lock);
		return;
	}

	/*
	 * Read the list of mounts
	 */
	mntl_head = fsmkmntlist(mnttab);
	if (mntl_head == NULL)
		goto done;

	/*
	 * Find the last entry that matches the
	 * mountpoint.
	 */
	memset((void *) &find_mnt, 0, sizeof (find_mnt));
	find_mnt.mnt_mountp = mnt->mnt_mountp;
	if ((found = fsgetmlast(mntl_head, &find_mnt)) == NULL) {
		pr_msg("fix_mnttab: can't modify entry %s: not in %s",
			mnt->mnt_mountp, MNTTAB);
		goto done;
	}

	newopts = pn = (char *) malloc(256);
	if (newopts == NULL) {
		pr_msg("fix_mnttab: can't modify entry %s: no memory",
			mnt->mnt_mountp);
		goto done;
	}

	opts = mnt->mnt_mntopts;
	if (opts && *opts) {
		(void) strcpy(pn, opts);
		trim(pn);
		pn += strlen(pn);
		*pn++ = ',';
	}

	(void) sprintf(pn, "%s=%x", MNTOPT_DEV, get_devid(found->mntl_mnt));
	mnt->mnt_mntopts = newopts;

	(void) sprintf(tbuf, "%ld", time(0L));
	mnt->mnt_time = tbuf;

	fsfreemnttab(found->mntl_mnt);
	found->mntl_mnt = fsdupmnttab(mnt);

	/*
	 * Write the mount list back
	 */
	(void) fsputmntlist(mnttab, mntl_head);

done:
	(void) fclose(mnttab);
	fsunlock_mnttab(mlock);
	(void) mutex_unlock(&mnttab_lock);
	fsfreemntlist(mntl_head);
}

/*
 * Scan the mount option string and extract
 * the hex device id from the "dev=" string.
 * If the string isn't found get it from the
 * filesystem stats.
 */
dev_t
get_devid(mnt)
	struct mnttab *mnt;
{
	dev_t val = 0;
	char *equal;
	char *str;
	struct stat st;

	if (str = hasmntopt(mnt, MNTOPT_DEV)) {
		if (equal = strchr(str, '='))
			val = strtoul(equal + 1, (char **) NULL, 16);
		else
			pr_msg("get_devid: bad device option '%s'", str);
	}

	if (val == 0) {		/* have to stat the mountpoint */
		if (stat(mnt->mnt_mountp, &st) < 0)
			pr_msg("get_devid: stat %s error: %m", mnt->mnt_mountp);
		else
			val = st.st_dev;
	}

	return (val);
}

struct mntlist *
getmntlist()
{
	struct mntlist *mntl;

	(void) mutex_lock(&mnttab_lock);
	mntl = fsgetmntlist();
	(void) mutex_unlock(&mnttab_lock);
	return (mntl);
}

/*
 * Remove the list of entries from mnttab based on the 'devid' field.
 * The 'rdev' field of the structure 'postumntreq' is not used.
 */
int
postunmount_remove_mnttab(postumntreq *req)
{
	FILE *mnttab;
	struct mntlist *mntl_head, *mntl;
	struct postumntreq *ur;
	int mlock;
	flock_t flock;

	(void) mutex_lock(&mnttab_lock);

	mlock = fslock_mnttab();
	mnttab = fopen(MNTTAB, "r+");
	if (mnttab == NULL) {
		char *s = "postunmount_remove_mnttab";
		pr_msg("%s: can't remove dev=%x rdev=%x: fopen %s error: %m",
			s, req->devid, req->rdevid, MNTTAB);
		fsunlock_mnttab(mlock);
		(void) mutex_unlock(&mnttab_lock);
		return (1);
	}

	(void) memset((void *) &flock, 0, sizeof (flock));
	flock.l_type = F_WRLCK;
	if (fcntl(fileno(mnttab), F_SETLKW, &flock) == -1) {
		char *s = "postunmount_remove_mnttab";
		pr_msg("%s: can't remove dev=%x rdev=%x: lock %s error : %m",
				s, req->devid, req->rdevid, MNTTAB);
		(void) fclose(mnttab);
		fsunlock_mnttab(mlock);
		(void) mutex_unlock(&mnttab_lock);
		return (1);
	}

	if ((mntl_head = fsmkmntlist(mnttab)) == NULL) {
		char *s = "postunmount_remove_mnttab";
		pr_msg("%s: can't remove dev=%x rdev=%x: fsmkmntlist failed",
			s, req->devid, req->rdevid);
		(void) fclose(mnttab);
		fsunlock_mnttab(mlock);
		(void) mutex_unlock(&mnttab_lock);
		return (1);
	}

	for (ur = req; ur; ur = ur->next) {
		/*
		 * Find the first entry with a matching device id.
		 * The devid is assumed to be unique for every filesystem
		 * except LOFS, and since LOFS is not postunmounted, there's
		 * no point trying to find the last entry
		 */
		mntl = mntl_head;
		while (mntl != NULL && ur->devid != get_devid(mntl->mntl_mnt))
			mntl = mntl->mntl_next;

		if (mntl != NULL) {
			if (trace > 1) {
				trace_prt(1, "  postunmount %s\n",
					mntl->mntl_mnt->mnt_mountp);
			}
			mntl->mntl_flags |= MNTL_UNMOUNT;
		} else {
			if (verbose) {
				char *s = "postunmount_remove_mnttab";
				pr_msg("%s: dev %x not in mnttab", s,
					ur->devid);
			}
			if (trace > 1) {
				trace_prt(1,
					"  postunmount: %x = ? "
					"<----- mntpnt not found\n", ur->devid);
			}
		}
	}

	/*
	 * Write the mount list back
	 */
	(void) fsputmntlist(mnttab, mntl_head);

	(void) fclose(mnttab);
	fsunlock_mnttab(mlock);
	(void) mutex_unlock(&mnttab_lock);
	fsfreemntlist(mntl_head);

	return (0);
}
