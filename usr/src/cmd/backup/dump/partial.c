/*
 * Copyright (c) 1989, 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)partial.c 1.8 90/11/09 SMI"

#ident	"@(#)partial.c 1.10 96/04/18"

#include "dump.h"
#include <ftw.h>

static int partial;

#ifdef __STDC__
static dev_t devfromopts(struct mntent *);
static int mark_root(dev_t, char *);
static int lf_mark_root(dev_t, char *);
static int ftw_mark(const char *, const struct stat *, int);
static int lf_ftw_mark(const char *, const struct stat64 *, int);
static void markino(ino_t);
#else
static dev_t devfromopts();
static int mark_root();
static int lf_mark_root();
static int ftw_mark();
static int lf_ftw_mark();
static void markino();
#endif

#ifdef USG
#ifdef USGFS
#define	lftw(a, b, c)	nftw((a), (b), (c), FTW_PHYS|FTW_MOUNT)
#define	lf_lftw(a, b, c) nftw64((a), (b), (c), FTW_PHYS|FTW_MOUNT)
#endif

#else	/* !USG */
#define	mygetmntent	getmntent
#endif

void
#ifdef __STDC__
partial_check(void)
#else
partial_check()
#endif
{
	struct mntent *mnt;
	struct stat st;

	if (stat(disk, &st) < 0 ||
		(st.st_mode & S_IFMT) == S_IFCHR ||
		(st.st_mode & S_IFMT) == S_IFBLK)
		return;

	partial_dev = st.st_dev;

	setmnttab();
	while (mnt = getmnttab()) {
		st.st_dev = devfromopts(mnt);
		if (st.st_dev == NODEV &&
		    stat(mnt->mnt_dir, &st) < 0)
			continue;
		if (partial_dev == st.st_dev) {
			disk = rawname(mnt->mnt_fsname);

			partial = 1;
			incno = '0';
			uflag = 0;
			return;
		}
	}
	msg(gettext("`%s' is not on a locally mounted filesystem\n"), disk);
	dumpabort();
	/* NOTREACHED */
}

void
#ifdef __STDC__
lf_partial_check(void)
#else
lf_partial_check()
#endif
{
	struct mntent *mnt;
	struct stat64 st;

	if (stat64(disk, &st) < 0 ||
		(st.st_mode & S_IFMT) == S_IFCHR ||
		(st.st_mode & S_IFMT) == S_IFBLK)
		return;

	partial_dev = st.st_dev;

	setmnttab();
	while (mnt = getmnttab()) {
		st.st_dev = devfromopts(mnt);
		if (st.st_dev == NODEV &&
		    stat64(mnt->mnt_dir, &st) < 0)
			continue;
		if (partial_dev == st.st_dev) {
			disk = lf_rawname(mnt->mnt_fsname);

			partial = 1;
			incno = '0';
			uflag = 0;
			return;
		}
	}
	msg(gettext("`%s' is not on a locally mounted filesystem\n"), disk);
	dumpabort();
	/* NOTREACHED */
}

/*
 *  The device id for the mount should be available in
 *  the mount option string as "dev=%04x".  If it's there
 *  extract the device id and avoid having to stat.
 */
static dev_t
devfromopts(mnt)
	struct mntent *mnt;
{
	char *str;

	str = hasmntopt(mnt, MNTINFO_DEV);
	if (str != NULL && (str = strchr(str, '=')))
		return ((dev_t)strtol(str + 1, (char **)NULL, 16));

	return (NODEV);
}

int
partial_mark(argc, argv)
	int argc;
	char **argv;
{
	char *path;
	struct stat st;

	if (partial == 0)
		return (1);

	while (--argc >= 0) {
		path = *argv++;

		if (stat(path, &st) < 0 ||
			st.st_dev != partial_dev) {
			msg(gettext("`%s' is not on dump device `%s'\n"),
				path, disk);
			dumpabort();
		}

		if (mark_root(partial_dev, path)) {
			msg(gettext(
			    "Cannot find filesystem mount point for `%s'\n"),
			    path);
			dumpabort();
		}

		if (lftw(path, ftw_mark, getdtablesize() / 2) < 0) {
			msg(gettext("Error in %s (%s)\n"),
				"ftw", strerror(errno));
			dumpabort();
		}
	}

	return (0);
}

int
lf_partial_mark(argc, argv)
	int argc;
	char **argv;
{
	char *path;
	struct stat64 st;

	if (partial == 0)
		return (1);

	while (--argc >= 0) {
		path = *argv++;

		if (stat64(path, &st) < 0 ||
			st.st_dev != partial_dev) {
			msg(gettext("`%s' is not on dump device `%s'\n"),
				path, disk);
			dumpabort();
		}

		if (lf_mark_root(partial_dev, path)) {
			msg(gettext(
			    "Cannot find filesystem mount point for `%s'\n"),
			    path);
			dumpabort();
		}

		if (lf_lftw(path, lf_ftw_mark, getdtablesize() / 2) < 0) {
			msg(gettext("Error in %s (%s)\n"),
				"ftw", strerror(errno));
			dumpabort();
		}
	}

	return (0);
}

/* mark directories between target and root */
static int
mark_root(dev, path)
	dev_t dev;
	char *path;
{
	struct stat st;
	char dotdot[MAXPATHLEN + 16];
	char *slash;

	(void) strcpy(dotdot, path);

	if (stat(dotdot, &st) < 0)
		return (1);

	/* if target is a regular file, find directory */
	if ((st.st_mode & S_IFMT) != S_IFDIR)
		if (slash = strrchr(dotdot, '/'))
			/* "/file" -> "/" */
			if (slash == dotdot)
				slash[1] = 0;
			/* "dir/file" -> "dir" */
			else
				slash[0] = 0;
		else
			/* "file" -> "." */
			(void) strcpy(dotdot, ".");

	/* keep marking parent until we hit mount point */
	do {
		if (stat(dotdot, &st) < 0 ||
			(st.st_mode & S_IFMT) != S_IFDIR ||
			st.st_dev != dev)
			return (1);
		markino(st.st_ino);
		(void) strcat(dotdot, "/..");
	} while (st.st_ino != 2);

	return (0);
}

/* mark directories between target and root */
static int
lf_mark_root(dev, path)
	dev_t dev;
	char *path;
{
	struct stat64 st;
	char dotdot[MAXPATHLEN + 16];
	char *slash;

	(void) strcpy(dotdot, path);

	if (stat64(dotdot, &st) < 0)
		return (1);

	/* if target is a regular file, find directory */
	if ((st.st_mode & S_IFMT) != S_IFDIR)
		if (slash = strrchr(dotdot, '/'))
			/* "/file" -> "/" */
			if (slash == dotdot)
				slash[1] = 0;
			/* "dir/file" -> "dir" */
			else
				slash[0] = 0;
		else
			/* "file" -> "." */
			(void) strcpy(dotdot, ".");

	/* keep marking parent until we hit mount point */
	do {
		if (stat64(dotdot, &st) < 0 ||
			(st.st_mode & S_IFMT) != S_IFDIR ||
			st.st_dev != dev)
			return (1);
		markino(st.st_ino);
		(void) strcat(dotdot, "/..");
	} while (st.st_ino != 2);

	return (0);
}

/*ARGSUSED*/
static int
ftw_mark(name, st, flag)
#ifdef __STDC__
	const char *name;
	const struct stat *st;
#else
	char *name;
	struct stat *st;
#endif
	int flag;
{
	if (flag != FTW_NS)
		markino(st->st_ino);

	return (0);
}

/*ARGSUSED*/
static int
lf_ftw_mark(name, st, flag)
#ifdef __STDC__
	const char *name;
	const struct stat64 *st;
#else
	char *name;
	struct stat64 *st;
#endif
	int flag;
{
	if (flag != FTW_NS)
		markino(st->st_ino);

	return (0);
}

static void
markino(i)
	ino_t i;
{
	struct dinode *dp;

	dp = getino(ino = i);
	mark(dp);
}
