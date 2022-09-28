/*
 * Copyright (c) 1984,1986,1987,1988,1989,1990,1991,1996 by
 *	Sun Microsystems, Inc.
 * All Rights Reserved.
 */


#pragma ident	"@(#)fslib.c	1.11	96/06/11 SMI"

#include	<stdio.h>
#include	<stdarg.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<libintl.h>
#include	<string.h>
#include	<fcntl.h>
#include	<errno.h>
#include	<syslog.h>
#include	<sys/vfstab.h>
#include	<sys/mnttab.h>
#include	<sys/mntent.h>
#include	<sys/mount.h>
#include	<sys/stat.h>
#include	<sys/param.h>
#include	<signal.h>
#include	"fslib.h"

#define	MNT_LOCK	"/etc/.mnttab.lock"
#define	BUFLEN		256

/*
 * Called before /etc/mnttab is read or written to.  This ensures the
 * integrity of /etc/mnttab by using a lock file.  Returns a lock id
 * with which to fsunlock_mnttab() after.
 */
int
fslock_mnttab()
{
	int mlock;
	flock_t flock;
	char buf[BUFLEN];

	if ((mlock = open(MNT_LOCK, O_RDWR|O_CREAT|O_TRUNC, 0644)) == -1) {
		(void) sprintf(buf, "fslock_mnttab: fopen %s", MNT_LOCK);
		perror(buf);
		return (-1);
	}

	(void) memset((void *) &flock, 0, sizeof (flock_t));
	flock.l_type = F_WRLCK;
	if (fcntl(mlock, F_SETLKW, &flock) == -1) {
		(void) sprintf(buf, "fslock_mnttab: fcntl %s", MNT_LOCK);
		perror(buf);
		(void) close(mlock);
		return (-1);
	}

	return (mlock);
}

/*
 * Undos fslock_mnttab().
 */
void
fsunlock_mnttab(int mlock)
{
	if (mlock >= 0)
		if (close(mlock) == -1)
			perror("fsunlock_mnttab: close");
}

/*
 * obtain a read lock on /etc/mnttab before reading it.  This ensures the
 * integrity of /etc/mnttab by using a lock file.  Returns a lock id
 * with which to fsunlock_mnttab() after.
 */
int
fsrdlock_mnttab()
{
	int mlock;
	flock_t	flock;
	char buf[BUFLEN];

	if ((mlock = open(MNT_LOCK, O_RDONLY)) == -1) {
		(void) sprintf(buf, "fsrdlock_mnttab: open %s", MNT_LOCK);
		perror(buf);
		return (-1);
	}

	(void) memset((void *) &flock, 0, sizeof (flock_t));
	flock.l_type = F_RDLCK;
	if (fcntl(mlock, F_SETLKW, &flock) == -1) {
		(void) sprintf(buf, "fsrdlock_mnttab: fcntl %s", MNT_LOCK);
		perror(buf);
		(void) close(mlock);
		return (-1);
	}

	return (mlock);
}


#define	TIME_MAX 16

/*
 * Add a mnttab entry to MNTTAB.  Include the device id with the
 * mount options and set the time field.
 */
int
fsaddtomtab(struct mnttab *mntin)
{
	FILE 	*mfp;
	struct	stat64 st;
	char	obuff[MNT_LINE_MAX], *pb = obuff, *opts;
	char	tbuf[TIME_MAX];
	int	mlock, error = 0;
	char	buf[MAXPATHLEN + BUFLEN];
	flock_t	flock;

	if (stat64(mntin->mnt_mountp, &st) < 0) {
		(void) sprintf(buf, "fsaddtomtab: stat %s", mntin->mnt_mountp);
		perror(buf);
		return (errno);
	}

	opts = mntin->mnt_mntopts;
	if (opts && *opts) {
		(void) strcpy(pb, opts);
		pb += strlen(pb);
		*pb++ = ',';
	}
	(void) sprintf(pb, "%s=%lx", MNTOPT_DEV, st.st_dev);
	mntin->mnt_mntopts = obuff;

	(void) sprintf(tbuf, "%ld", time(0L));
	mntin->mnt_time = tbuf;

	mlock = fslock_mnttab();
	if ((mfp = fopen(MNTTAB, "a+")) == NULL) {
		error = errno;
		(void) sprintf(buf, "fsaddtomtab: fopen %s", MNTTAB);
		perror(buf);
		fsunlock_mnttab(mlock);
		mntin->mnt_mntopts = opts;
		return (error);
	}
	/*
	 * We need to be paranoid here because we can't be sure that
	 * all other mnttab programs use fslock_mnttab().
	 */
	(void) memset((void *) &flock, 0, sizeof (flock_t));
	flock.l_type = F_WRLCK;
	if (fcntl(fileno(mfp), F_SETLKW, &flock) != -1) {
		(void) fseek(mfp, 0L, SEEK_END);	/* guarantee at EOF */
		putmntent(mfp, mntin);
	} else {
		error = errno;
		(void) sprintf(buf, "fsaddtomntab: fcntl %s", MNTTAB);
		perror(buf);
	}
	(void) fclose(mfp);
	fsunlock_mnttab(mlock);
	mntin->mnt_mntopts = opts;
	return (error);
}

/*
 * Remove the last entry in MNTTAB that matches mntin.
 * Returns errno if error.
 */
int
fsrmfrommtab(struct mnttab *mntin)
{
	FILE 		*fp;
	mntlist_t 	*mlist = NULL, *delete;
	int 		mlock;
	int		error;
	char		buf[BUFLEN];
	flock_t		flock;

	if (mntin == NULL)
		return (0);

	mlock = fslock_mnttab();
	if ((fp = fopen(MNTTAB, "r+")) == NULL) {
		error = errno;
		(void) sprintf(buf, "fsrmfrommtab: fopen %s", MNTTAB);
		perror(buf);
		fsunlock_mnttab(mlock);
		return (error);
	}

	(void) memset((void *) &flock, 0, sizeof (flock_t));
	flock.l_type = F_WRLCK;
	if (fcntl(fileno(fp), F_SETLKW, &flock) == -1) {
		error = errno;
		(void) sprintf(buf, "fsrmfrommtab: fcntl %s", MNTTAB);
		perror(buf);
		goto finish;
	}

	/*
	 * Read the entire mnttab into memory.
	 * Remember the *last* instance of the unmounted
	 * mount point (have to take stacked mounts into
	 * account) and make sure that it's not written
	 * back out.
	 */
	if ((mlist = fsmkmntlist(fp)) == NULL) {
		error = ENOENT;
		goto finish;
	}

	delete = fsgetmlast(mlist, mntin);

	if (delete)
		delete->mntl_flags |= MNTL_UNMOUNT;

	/*
	 * Write the mount list back
	 */
	error = fsputmntlist(fp, mlist);

finish:
	(void) fclose(fp);
	fsunlock_mnttab(mlock);
	if (mlist != NULL)
		fsfreemntlist(mlist);
	return (error);
}

/*
 * Locks mnttab, reads all of the entries, unlocks mnttab, and returns the
 * linked list of the entries.
 */
mntlist_t *
fsgetmntlist()
{
	FILE *mfp;
	mntlist_t *mntl;
	int mlock;
	char buf[BUFLEN];
	flock_t flock;

	mlock = fslock_mnttab();
	if ((mfp = fopen(MNTTAB, "r+")) == NULL) {
		(void) sprintf(buf, "fsgetmntlist: fopen %s", MNTTAB);
		perror(buf);
		fsunlock_mnttab(mlock);
		return (NULL);
	}

	(void) memset((void *) &flock, 0, sizeof (flock_t));
	flock.l_type = F_RDLCK;
	if (fcntl(fileno(mfp), F_SETLKW, &flock) == -1) {
		perror("fsgetmntlist: fcntl");
		(void) fclose(mfp);
		fsunlock_mnttab(mlock);
		return (NULL);
	}

	mntl = fsmkmntlist(mfp);

	(void) fclose(mfp);
	fsunlock_mnttab(mlock);
	return (mntl);
}

/*
 * Puts the mntlist out to the mfp mnttab file, except for those with
 * the UNMOUNT bit set.  Expects mfp to be locked.  Returns errno if
 * an error occurred.
 */
int
fsputmntlist(FILE *mfp, mntlist_t *mntl_head)
{
	mntlist_t *mntl;
	sigset_t mask, omask;
	int error;
	char buf[BUFLEN];

	/*
	 * Block SIGHUP, SIGQUIT, SIGINT while manipulating
	 * /etc/mnttab
	 */
	if (sigemptyset(&mask) < 0)
		return (errno);

	if (sigaddset(&mask, SIGHUP) < 0 ||
	    sigaddset(&mask, SIGQUIT) < 0 ||
	    sigaddset(&mask, SIGINT) < 0)
		return (errno);

	if (sigprocmask(SIG_BLOCK, &mask, &omask) < 0)
		return (errno);

	/* now truncate the mnttab and write almost all of it back */

	rewind(mfp);
	if (ftruncate(fileno(mfp), 0) < 0) {
		error = errno;
		(void) sprintf(buf, "fsputmntlist: ftruncate %s", MNTTAB);
		perror(buf);
		/*
		 * Restore old signal mask
		 */
		(void) sigprocmask(SIG_SETMASK, &omask, (sigset_t *) NULL);

		return (error);
	}

	for (mntl = mntl_head; mntl; mntl = mntl->mntl_next) {
		if (mntl->mntl_flags & MNTL_UNMOUNT)
			continue;
		putmntent(mfp, mntl->mntl_mnt);
	}
	/*
	 * Restore old signal mask
	 */
	(void) sigprocmask(SIG_SETMASK, &omask, (sigset_t *) NULL);

	return (0);
}

static struct mnttab zmnttab = { 0 };

struct mnttab *
fsdupmnttab(struct mnttab *mnt)
{
	struct mnttab *new;

	new = (struct mnttab *) malloc(sizeof (*new));
	if (new == NULL)
		goto alloc_failed;

	*new = zmnttab;
	/*
	 * Allocate an extra byte for the mountpoint
	 * name in case a space needs to be added.
	 */
	new->mnt_mountp = (char *) malloc(strlen(mnt->mnt_mountp) + 2);
	if (new->mnt_mountp == NULL)
		goto alloc_failed;
	(void) strcpy(new->mnt_mountp, mnt->mnt_mountp);

	if ((new->mnt_special = strdup(mnt->mnt_special)) == NULL)
		goto alloc_failed;

	if ((new->mnt_fstype = strdup(mnt->mnt_fstype)) == NULL)
		goto alloc_failed;

	if (mnt->mnt_mntopts != NULL)
		if ((new->mnt_mntopts = strdup(mnt->mnt_mntopts)) == NULL)
			goto alloc_failed;

	if (mnt->mnt_time != NULL)
		if ((new->mnt_time = strdup(mnt->mnt_time)) == NULL)
			goto alloc_failed;

	return (new);

alloc_failed:
	(void) fprintf(stderr, gettext("fsdupmnttab: Out of memory\n"));
	fsfreemnttab(new);
	return (NULL);
}

/*
 * Free a single mnttab structure
 */
void
fsfreemnttab(struct mnttab *mnt)
{

	if (mnt) {
		if (mnt->mnt_special)
			free(mnt->mnt_special);
		if (mnt->mnt_mountp)
			free(mnt->mnt_mountp);
		if (mnt->mnt_fstype)
			free(mnt->mnt_fstype);
		if (mnt->mnt_mntopts)
			free(mnt->mnt_mntopts);
		if (mnt->mnt_time)
			free(mnt->mnt_time);
		free(mnt);
	}
}

void
fsfreemntlist(mntlist_t *mntl)
{
	mntlist_t *mntl_tmp;

	while (mntl) {
		fsfreemnttab(mntl->mntl_mnt);
		mntl_tmp = mntl;
		mntl = mntl->mntl_next;
		free(mntl_tmp);
	}
}

/*
 * Read the mnttab file and return it as a list of mnttab structs.
 * Returns NULL if there was a memory failure.
 * This routine expects the mnttab file to be locked.
 */
mntlist_t *
fsmkmntlist(FILE *mfp)
{
	struct mnttab 	mnt;
	mntlist_t 	*mhead, *mtail;
	int 		ret;

	mhead = mtail = NULL;

	while ((ret = getmntent(mfp, &mnt)) != -1) {
		mntlist_t	*mp;

		if (ret != 0)		/* bad entry */
			continue;

		mp = (mntlist_t *) malloc(sizeof (*mp));
		if (mp == NULL)
			goto alloc_failed;
		if (mhead == NULL)
			mhead = mp;
		else
			mtail->mntl_next = mp;
		mtail = mp;
		mp->mntl_next = NULL;
		mp->mntl_flags = 0;
		if ((mp->mntl_mnt = fsdupmnttab(&mnt)) == NULL)
			goto alloc_failed;
	}
	return (mhead);

alloc_failed:
	fsfreemntlist(mhead);
	return (NULL);
}

/*
 * Return the last entry that matches mntin's special
 * device and/or mountpt.
 * Helps to be robust here, so we check for NULL pointers.
 */
mntlist_t *
fsgetmlast(mntlist_t *ml, struct mnttab *mntin)
{
	mntlist_t 	*delete = NULL;

	for (; ml; ml = ml->mntl_next) {
		if (mntin->mnt_mountp && mntin->mnt_special) {
			/*
			 * match if and only if both are equal.
			 */
			if ((strcmp(ml->mntl_mnt->mnt_mountp,
					mntin->mnt_mountp) == 0) &&
			    (strcmp(ml->mntl_mnt->mnt_special,
					mntin->mnt_special) == 0))
				delete = ml;
		} else if (mntin->mnt_mountp) {
			if (strcmp(ml->mntl_mnt->mnt_mountp,
					mntin->mnt_mountp) == 0)
				delete = ml;
		} else if (mntin->mnt_special) {
			if (strcmp(ml->mntl_mnt->mnt_special,
					mntin->mnt_special) == 0)
				delete = ml;
	    }
	}
	return (delete);
}


/*
 * Returns the mountlevel of the pathname in cp.  As examples,
 * / => 1, /bin => 2, /bin/ => 2, ////bin////ls => 3, sdf => 0, etc...
 */
int
fsgetmlevel(char *cp)
{
	int	mlevel;
	char	*cp1;

	if (cp == NULL || *cp == NULL || *cp != '/')
		return (0);	/* this should never happen */

	mlevel = 1;			/* root (/) is the minimal case */

	for (cp1 = cp + 1; *cp1; cp++, cp1++)
		if (*cp == '/' && *cp1 != '/')	/* "///" counts as 1 */
			mlevel++;

	return (mlevel);
}

/*
 * Returns non-zero if string s is a member of the strings in ps.
 */
int
fsstrinlist(const char *s, const char **ps)
{
	const char *cp;
	cp = *ps;
	while (cp) {
		if (strcmp(s, cp) == 0)
			return (1);
		ps++;
		cp = *ps;
	}
	return (0);
}
