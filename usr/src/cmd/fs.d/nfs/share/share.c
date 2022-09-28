/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)share.c	1.29	96/10/16 SMI"

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *	PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *	Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *  (c) 1986,1987,1988,1989,1995,1996  Sun Microsystems, Inc
 *  (c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	All rights reserved.
 *
 */
/*
 * nfs share
 */
#define	_REENTRANT

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <varargs.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/param.h>	/* for UID_NOBODY */
#include <sys/stat.h>
#include <errno.h>
#include <rpc/rpc.h>
#include <netconfig.h>
#include <netdir.h>
#include <nfs/nfs_sec.h>
#include <nfs/export.h>
#include "../lib/sharetab.h"

#define	RET_OK		0
#define	RET_ERR		32

static int direq(char *, char *);
static int newopts(char *);
static void parseopts_old(struct export *, char *);
static void parseopts_new(struct export *, char *);
static void pr_err();
static int shareable(char *);
static int sharetab_add(char *, char *, char *, char *, int);
static int sharepub_exist(char *);
static caddr_t *get_rootnames(seconfig_t *, char *, int *);
static void usage();
static void exportindex(struct export *, char *);

extern int issubdir();
extern int exportfs();
int nfs_getseconfig_byname(char *, seconfig_t *);
static void printarg(char *, struct export *);
static struct export ex;

int verbose;

main(argc, argv)
	int argc;
	char **argv;
{
	extern int optind;
	extern char *optarg;
	char dir[MAXPATHLEN];
	char *res = "-";
	char *opts = "rw";
	char *descr = "";
	int c;
	int replace = 0;

	/* Don't drop core if the NFS module isn't loaded. */
	signal(SIGSYS, SIG_IGN);

	while ((c = getopt(argc, argv, "o:d:")) != EOF) {
		switch (c) {
		case 'o':
			opts = optarg;
			break;
		case 'd':
			descr = optarg;
			break;
		case 'v':
			verbose++;
			break;
		default:
			usage();
			exit(RET_ERR);
		}
	}

	if (argc <= optind || argc - optind > 2) {
		usage();
		exit(RET_ERR);
	}
	if (realpath(argv[optind], dir) == NULL)
		pr_err("%s: %s\n", argv[optind], strerror(errno));

	if (argc - optind > 1)
		res = argv[optind + 1];

	switch (shareable(dir)) {
	case 0:
		exit(RET_ERR);
		break;
	case 1:
		break;
	case 2:
		replace = 1;
		break;
	}

	ex.ex_path = dir;
	ex.ex_pathlen = strlen(dir) + 1;

	if (newopts(opts))
		parseopts_new(&ex, opts);
	else
		parseopts_old(&ex, opts);

	/*
	 * If -o public was specified, check for any existing directory
	 * shared with -o public. If so, fail.
	 */
	if (ex.ex_flags & EX_PUBLIC) {
		if (sharepub_exist(dir) == 1) {
			errno = 0;
			pr_err("Cannot share more than filesystem with"
				" 'public' option\n");
		}
	}

	if (verbose)
		printarg(dir, &ex);

	if (exportfs(dir, &ex) < 0) {
		if (errno == EREMOTE)
			pr_err("Cannot share remote filesystem: %s\n", dir);
		else
			pr_err("%s: %s\n", dir, strerror(errno));
	}

	if (sharetab_add(dir, res, opts, descr, replace) < 0)
		exit(RET_ERR);

	return (RET_OK);
}

/*
 * Check if there already is an entry shared with -o public.
 */
static int
sharepub_exist(char *dir)
{
	extern int errno;
	struct share *sh;
	int res;
	FILE *f;

	f = fopen(SHARETAB, "r");

	if (f == NULL) {
		if (errno == ENOENT)
			return (0);
		pr_err("%s: %s\n", SHARETAB, strerror(errno));
	}

	while ((res = getshare(f, &sh)) > 0) {
		if (strcmp(sh->sh_fstype, "nfs") != 0)
			continue;

		if (strcmp(sh->sh_path, dir) == 0)
			continue;

		if (getshareopt(sh->sh_opts, SHOPT_PUBLIC)) {
			return (1);
		}
	}

	if (res < 0) {
		pr_err("error reading %s\n", SHARETAB);
		(void) fclose(f);
	}

	(void) fclose(f);
	return (0);
}

/*
 * Check the nfs share entries in sharetab file.
 * Returns:
 *	0  dir not shareable
 *	1  dir is shareable
 *	2  dir is already shared (can modify options)
 */
static int
shareable(path)
	char *path;
{
	FILE *f;
	extern int errno;
	struct share *sh;
	struct stat st;
	int res;

	errno = 0;
	if (*path != '/')
		pr_err("%s: not a full pathname\n", path);

	if (stat(path, &st) < 0) 	/* does it exist ? */
		pr_err("%s: %s\n", path, strerror(errno));

	/*
	 * We make the assumption that if we can't open the SHARETAB
	 * file for some reason other than it doesn't exist, then we
	 * won't share the directory.  Since we can't complete the
	 * operation correctly, then let's not do it at all.
	 */
	f = fopen(SHARETAB, "r");
	if (f == NULL) {
		if (errno == ENOENT)
			return (1);
		pr_err("%s: %s\n", SHARETAB, strerror(errno));
		return (0);
	}

	while ((res = getshare(f, &sh)) > 0) {
		if (strcmp(sh->sh_fstype, "nfs") != 0)
			continue;

		if (direq(path, sh->sh_path)) {
			(void) fclose(f);
			return (2);
		}

		if (issubdir(sh->sh_path, path)) {
			pr_err("%s: sub-directory (%s) already shared\n",
				path, sh->sh_path);
		}
		if (issubdir(path, sh->sh_path)) {
			pr_err("%s: parent-directory (%s) already shared\n",
				path, sh->sh_path);
		}
	}

	if (res < 0)
		pr_err("error reading %s\n", SHARETAB);

	(void) fclose(f);
	return (1);
}

static int
direq(dir1, dir2)
	char *dir1, *dir2;
{
	struct stat st1, st2;

	if (strcmp(dir1, dir2) == 0)
		return (1);
	if (stat(dir1, &st1) < 0 || stat(dir2, &st2) < 0)
		return (0);
	return (st1.st_ino == st2.st_ino && st1.st_dev == st2.st_dev);
}

static char *optlist[] = {
#define	OPT_RO		0
	SHOPT_RO,
#define	OPT_RW		1
	SHOPT_RW,
#define	OPT_ROOT	2
	SHOPT_ROOT,
#define	OPT_SECURE	3
	SHOPT_SECURE,
#define	OPT_ANON	4
	SHOPT_ANON,
#define	OPT_WINDOW	5
	SHOPT_WINDOW,
#define	OPT_KERBEROS	6
	SHOPT_KERBEROS,
#define	OPT_NOSUID	7
	SHOPT_NOSUID,
#define	OPT_ACLOK	8
	SHOPT_ACLOK,
#define	OPT_NOSUB	9
	SHOPT_NOSUB,
#define	OPT_SEC		10
	SHOPT_SEC,
#define	OPT_PUBLIC	11
	SHOPT_PUBLIC,
#define	OPT_INDEX	12
	SHOPT_INDEX,
	NULL
};

/*
 * If the option string contains a "sec="
 * option, then use new option syntax.
 */
static int
newopts(char *opts)
{
	char *p, *val;

	p = strdup(opts);
	if (p == NULL)
		pr_err("opts: no memory\n");

	while (*p)
		if (getsubopt(&p, optlist, &val) == OPT_SEC)
			return (1);

	return (0);
}

#define	badnum(x) ((x) == NULL || !isdigit(*(x)))
#define	DEF_WIN	30000

/*
 * Parse the share options from the "-o" flag.
 * The extracted data is moved into the exports
 * structure which is passed into the kernel via
 * the exportfs() system call.
 */
static void
parseopts_old(exp, opts)
	struct export *exp;
	char *opts;
{
	char *p, *savep, *val, *rootlist;
	struct secinfo *sp;
	int done_aclok = 0;
	int done_nosuid = 0;
	int done_anon = 0;


	p = strdup(opts);
	if (p == NULL)
		pr_err("opts: no memory\n");

	exp->ex_version = 1;
	exp->ex_anon = UID_NOBODY;
	exp->ex_seccnt = 1;
	exp->ex_index = NULL;

	sp = (struct secinfo *) malloc(sizeof (struct secinfo));
	if (sp == NULL)
		pr_err("ex_secinfo: no memory\n");
	exp->ex_secinfo = sp;

	/*
	 * Initialize some fields
	 */
	sp->s_flags = 0;
	sp->s_window = DEF_WIN;
	sp->s_rootcnt = 0;
	sp->s_rootnames = NULL;

	if (nfs_getseconfig_default(&sp->s_secinfo))
		pr_err("failed to get default security mode\n");

	while (*p) {
		savep = p;
		switch (getsubopt(&p, optlist, &val)) {

		case OPT_RO:

			sp->s_flags |= val ? M_ROL : M_RO;

			if (sp->s_flags & M_RO && sp->s_flags & M_RW)
				pr_err("rw vs ro conflict\n");
			if (sp->s_flags & M_RO && sp->s_flags & M_ROL)
				pr_err("Ambiguous ro options\n");
			break;

		case OPT_RW:

			sp->s_flags |= val ? M_RWL : M_RW;

			if (sp->s_flags & M_RO && sp->s_flags & M_RW)
				pr_err("ro vs rw conflict\n");
			if (sp->s_flags & M_RW && sp->s_flags & M_RWL)
				pr_err("Ambiguous rw options\n");
			break;

		case OPT_ROOT:
			if (val == NULL)
				pr_err("missing root list\n");
			rootlist = val;
			sp->s_flags |= M_ROOT;
			break;

		case OPT_SECURE:
			if (nfs_getseconfig_byname("dh", &sp->s_secinfo)) {
				pr_err("invalid sec name\n");
			}
			break;

		case OPT_KERBEROS:
			if (nfs_getseconfig_byname("krb4", &sp->s_secinfo)) {
				pr_err("invalid sec name\n");
			}
			break;

		case OPT_ANON:
			if (done_anon++)
				pr_err("option anon repeated\n");

			/* check for special "-1" value, which is ok */
			if (strcmp(val, "-1") != 0 && badnum(val)) {
				pr_err("invalid anon value\n");
			}
			exp->ex_anon = atoi(val);
			break;

		case OPT_WINDOW:
			if (badnum(val))
				pr_err("invalid window value\n");
			sp->s_window = atoi(val);
			break;

		case OPT_NOSUID:
			if (done_nosuid++)
				pr_err("option nosuid repeated\n");

			exp->ex_flags |= EX_NOSUID;
			break;

		case OPT_ACLOK:
			if (done_aclok++)
				pr_err("option aclok repeated\n");
			exp->ex_flags |= EX_ACLOK;
			break;

		case OPT_NOSUB:
			/*
			 * The "don't allow mount of subdirectories" option.
			 */
			exp->ex_flags |= EX_NOSUB;
			break;

		case OPT_PUBLIC:
			exp->ex_flags |= EX_PUBLIC;
			break;

		case OPT_INDEX:
			exportindex(exp, val);
			break;

		default:
			pr_err("invalid share option: '%s'\n", savep);
		}
	}
	if (sp->s_flags & M_ROOT && sp->s_secinfo.sc_rpcnum != AUTH_UNIX) {
		sp->s_rootnames = get_rootnames(&sp->s_secinfo, rootlist,
				&sp->s_rootcnt);
		if (sp->s_rootnames == NULL)
			pr_err("Bad root list\n");
	}

	/*
	 * Set uninitialized flags to "rw"
	 */
	if ((sp->s_flags & (M_RO|M_RW|M_RWL|M_ROL)) == 0)
		sp->s_flags |= M_RW;
}

/*
 * Parse the new share options from the "-o" flag.
 * Parsing is more complicated than the old case
 * Since we may be setting up multiple secinfo entries.
 * Syntax is more restrictive: the flavor-dependent
 * options: ro, rw, root, window can only follow
 * a sec option.
 */
static void
parseopts_new(exp, opts)
	struct export *exp;
	char *opts;
{
	char *p, *q, *savep, *val;
	char *f, *lasts;
	struct secinfo *sp1, *sp, *pt;
	int i, secopt;
	int count = 0;
	int done_aclok = 0;
	int done_nosuid = 0;
	int done_anon = 0;

	exp->ex_version = 1;
	exp->ex_anon = UID_NOBODY;
	exp->ex_index = NULL;

	p = strdup(opts);
	if (p == NULL)
		pr_err("opts: no memory\n");

	/*
	 * Count the number of security modes
	 */
	while (*p) {
		switch (getsubopt(&p, optlist, &val)) {
		case OPT_SECURE:
			pr_err("Cannot mix options secure and sec\n");
			break;
		case OPT_KERBEROS:
			pr_err("Cannot mix options kerberos and sec\n");
			break;
		case OPT_SEC:
			count++;
			for (q = val; *q; q++)
				if (*q == ':')
					count++;
			break;
		}
	}

	exp->ex_seccnt = count;

	sp = (struct secinfo *) calloc(count, sizeof (struct secinfo));
	if (sp == NULL)
		pr_err("ex_secinfo: no memory\n");

	/*
	 * Initialize some fields
	 */
	for (i = 0; i < count; i++) {
		sp[i].s_flags = 0;
		sp[i].s_window = DEF_WIN;
		sp[i].s_rootcnt = 0;
		sp[i].s_rootnames = NULL;
	}

	exp->ex_secinfo = sp;
	sp1 = sp;

	p = strdup(opts);
	if (p == NULL)
		pr_err("opts: no memory\n");

	if (nfs_getseconfig_default(&sp->s_secinfo))
		pr_err("failed to get default security mode\n");

	secopt = 0;

	while (*p) {
		savep = p;
		switch (getsubopt(&p, optlist, &val)) {

		case OPT_SEC:
			if (secopt)
				sp++;
			sp1 = sp;
			secopt++;

			while ((f = strtok_r(val, ":", &lasts)) != NULL) {
				if (nfs_getseconfig_byname(f, &sp->s_secinfo))
					pr_err("Invalid security mode \"%s\"\n",
						f);
				val = NULL;
				if (lasts)
					sp++;
			}
			break;

		case OPT_RO:
			if (secopt == 0)
				pr_err("need sec option before ro\n");

			sp->s_flags |= val ? M_ROL : M_RO;

			if (sp->s_flags & M_RO && sp->s_flags & M_RW)
				pr_err("rw vs ro conflict\n");
			if (sp->s_flags & M_RO && sp->s_flags & M_ROL)
				pr_err("Ambiguous ro options\n");

			for (pt = sp1; pt < sp; pt++)
				pt->s_flags = sp->s_flags;
			break;

		case OPT_RW:
			if (secopt == 0)
				pr_err("need sec option before rw\n");

			sp->s_flags |= val ? M_RWL : M_RW;

			if (sp->s_flags & M_RO && sp->s_flags & M_RW)
				pr_err("ro vs rw conflict\n");
			if (sp->s_flags & M_RW && sp->s_flags & M_RWL)
				pr_err("Ambiguous rw options\n");

			for (pt = sp1; pt < sp; pt++)
				pt->s_flags = sp->s_flags;
			break;

		case OPT_ROOT:
			if (secopt == 0)
				pr_err("need sec option before root\n");

			if (val == NULL)
				pr_err("missing root list\n");

			for (pt = sp1; pt <= sp; pt++) {
				pt->s_flags |= M_ROOT;

				/*
				 * Can treat AUTH_UNIX root lists
				 * as a special case and have
				 * the nfsauth service check the
				 * list just like any other access
				 * list, i.e. supports netgroups,
				 * domain suffixes, etc.
				 */
				if (pt->s_secinfo.sc_rpcnum == AUTH_UNIX)
					continue;

				/*
				 * Root lists for other sec types
				 * need to be checked in the
				 * kernel. Build a list of names
				 * to be fed into the kernel via
				 * exportfs().
				 */
				pt->s_rootnames =
					get_rootnames(&pt->s_secinfo, val,
						&pt->s_rootcnt);
				if (pt->s_rootnames == NULL)
					pr_err("Bad root list\n");
			}
			break;

		case OPT_WINDOW:
			if (secopt == 0)
				pr_err("need sec option before window\n");

			if (badnum(val))
				pr_err("invalid window value\n");
			sp->s_window = atoi(val);

			for (pt = sp1; pt < sp; pt++)
				pt->s_window = sp->s_window;
			break;

		case OPT_ANON:
			if (done_anon++)
				pr_err("option anon repeated\n");

			/* check for special "-1" value, which is ok */
			if (strcmp(val, "-1") != 0 && badnum(val))
				pr_err("invalid anon value\n");

			exp->ex_anon = atoi(val);
			break;

		case OPT_NOSUID:
			if (done_nosuid++)
				pr_err("option nosuid repeated\n");

			exp->ex_flags |= EX_NOSUID;
			break;

		case OPT_ACLOK:
			if (done_aclok++)
				pr_err("option aclok repeated\n");
			exp->ex_flags |= EX_ACLOK;
			break;

		case OPT_NOSUB:
			/*
			 * The "don't allow mount of subdirectories" option.
			 */
			exp->ex_flags |= EX_NOSUB;
			break;

		case OPT_PUBLIC:
			exp->ex_flags |= EX_PUBLIC;
			break;

		case OPT_INDEX:
			exportindex(exp, val);
			break;

		default:
			pr_err("invalid share option: '%s'\n", savep);
		}
	}

	/*
	 * Set uninitialized flags to "rw"
	 */
	sp = exp->ex_secinfo;
	for (i = 0; i < count; i++) {
		if ((sp[i].s_flags & (M_RO|M_RW|M_RWL|M_ROL)) == 0)
			sp[i].s_flags |= M_RW;
	}
}

/*
 * check the argument specified with the index option and set
 * export index file and flags
 */
static void
exportindex(struct export *exp, char *val)
{
	char *p = val;

	if (val == NULL)
		goto badindexarg;

	p = val;
	while (*p != '\0') {
		if (*p == '/')
			goto badindexarg;
		p++;
	}

	if (strcmp(val, "..") == 0)
		goto badindexarg;

	/*
	 * treat a "." or an empty index string as if the
	 * index option is not present.
	 */
	if (val[0] == '\0' || (strcmp(val, ".") == 0))
		return;

	exp->ex_index = strdup(val);
	if (!exp->ex_index) {
		pr_err("exportindex: out of memory\n");
		return;
	}
	exp->ex_flags |= EX_INDEX;

	return;

badindexarg:
	pr_err("index option requires a filename as argument\n");
}

/*
 * Given a seconfig entry and a colon-separated
 * list of names, allocate an array big enough
 * to hold the root list, then convert each name to
 * a principal name according to the security
 * info and assign it to an array element.
 * Return the array and its size.
 */
static caddr_t *
get_rootnames(seconfig_t *sec, char *list, int *count)
{
	caddr_t *a;
	int c, i;
	char *host, *p;

	list = strdup(list);
	if (list == NULL)
		pr_err("get_rootnames: no memory\n");

	/*
	 * Count the number of strings in the list.
	 * This is the number of colon separators + 1.
	 */
	c = 1;
	for (p = list; *p; p++)
		if (*p == ':')
			c++;
	*count = c;

	a = (caddr_t *) malloc(c * sizeof (char *));
	if (a == NULL)
		pr_err("get_rootnames: no memory\n");

	for (i = 0; i < c; i++) {
		host = strtok(list, ":");
		if (!nfs_get_root_principal(sec, host, &a[i])) {
			a = NULL;
			break;
		}
		list = NULL;
	}

	return (a);
}

/*
 * Append an entry to the sharetab file
 */
static int
sharetab_add(dir, res, opts, descr, replace)
	char *dir, *res, *opts, *descr;
	int replace;
{
	FILE *f;
	struct share sh;

	/*
	 * Open the file for update and create it if necessary.
	 * This may leave the I/O offset at the end of the file,
	 * so rewind back to the beginning of the file.
	 */
	f = fopen(SHARETAB, "a+");
	if (f == NULL) {
		pr_err("%s: %s\n", SHARETAB, strerror(errno));
		return (-1);
	}
	rewind(f);

	if (lockf(fileno(f), F_LOCK, 0L) < 0) {
		pr_err("cannot lock %s: %s\n", SHARETAB, strerror(errno));
		(void) fclose(f);
		return (-1);
	}

	/*
	 * If re-sharing an old share with new options
	 * then first remove the old share entry.
	 */
	if (replace) {
		if (remshare(f, dir) < 0)
			pr_err("remshare\n");
	}

	sh.sh_path = dir;
	sh.sh_res = res;
	sh.sh_fstype = "nfs";
	sh.sh_opts = opts;
	sh.sh_descr = descr;

	if (putshare(f, &sh) < 0)
		pr_err("addshare: couldn't add %s to %s\n",
			dir, SHARETAB);

	(void) fclose(f);
	return (0);
}

static void
usage()
{
	(void) fprintf(stderr,
	    "Usage: share [-o options] [-d description] pathname [resource]\n");
}

/*
 * This is for testing only
 * It displays the export structure that
 * goes into the kernel.
 */
static void
printarg(char *path, struct export *ep)
{
	int i, j;
	struct secinfo *sp;

	printf("%s:\n", path);
	printf("\tex_version = %d\n", ep->ex_version);
	printf("\tex_path = %s\n", ep->ex_path);
	printf("\tex_pathlen = %d\n", ep->ex_pathlen);
	printf("\tex_flags: (0x%02x) ", ep->ex_flags);
	if (ep->ex_flags & EX_NOSUID)
		printf("NOSUID ");
	if (ep->ex_flags & EX_ACLOK)
		printf("ACLOK ");
	if (ep->ex_flags & EX_PUBLIC)
		printf("PUBLIC ");
	if (ep->ex_flags & EX_NOSUB)
		printf("NOSUB ");
	if (ep->ex_flags == 0)
		printf("(none)");
	printf("\n");
	printf("\tex_anon = %d\n", ep->ex_anon);
	printf("\tex_seccnt = %d\n", ep->ex_seccnt);
	printf("\n");
	for (i = 0; i < ep->ex_seccnt; i++) {
		sp = &ep->ex_secinfo[i];
		printf("\t\ts_secinfo = %s\n", sp->s_secinfo.sc_name);
		printf("\t\ts_flags: (0x%02x) ", sp->s_flags);
		if (sp->s_flags & M_ROOT) printf("M_ROOT ");
		if (sp->s_flags & M_RO) printf("M_RO ");
		if (sp->s_flags & M_ROL) printf("M_ROL ");
		if (sp->s_flags & M_RW) printf("M_RW ");
		if (sp->s_flags & M_RWL) printf("M_RWL ");
		if (sp->s_flags == 0) printf("(none)");
		printf("\n");
		printf("\t\ts_window = %d\n", sp->s_window);
		printf("\t\ts_rootcnt = %d ", sp->s_rootcnt);
		for (j = 0; j < sp->s_rootcnt; j++)
			printf("%s ", sp->s_rootnames[j]);
		printf("\n\n");
	}
}

/*VARARGS1*/
static void
pr_err(fmt, va_alist)
	char *fmt;
	va_dcl
{
	va_list ap;

	va_start(ap);
	(void) fprintf(stderr, "share_nfs: ");
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);
	exit(RET_ERR);
}
