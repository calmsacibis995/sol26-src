/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)mount.c	1.61	96/10/21 SMI"	/* SVr4.0 1.18	*/

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
 *  (c) 1986,1987,1988,1989,1996  Sun Microsystems, Inc
 *  (c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	All rights reserved.
 *
 */
/*
 * nfs mount
 */

#define	NFSCLIENT
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <varargs.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/param.h>
#include <rpc/rpc.h>
#include <errno.h>
#include <sys/stat.h>
#include <netdb.h>
#include <sys/mount.h>
#include <sys/mntent.h>
#include <sys/mnttab.h>
#include <nfs/nfs.h>
#include <nfs/mount.h>
#include <rpcsvc/mount.h>
#include <sys/pathconf.h>
#include <netdir.h>
#include <netconfig.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <syslog.h>
#include <fslib.h>
#include "replica.h"
#include <netinet/in.h>
#include <nfs/nfs_sec.h>
#include "nfs_subr.h"

#define	RET_OK		0
#define	RET_RETRY	32
#define	RET_ERR		33

/* number of transports to try */
#define	MNT_PREF_LISTLEN	2
#define	FIRST_TRY		1
#define	SECOND_TRY		2

#define	BIGRETRY	10000

/* maximum length of RPC header for NFS messages */
#define	NFS_RPC_HDR	432

#define	NFS_ARGS_EXT2_secdata(args, secdata) \
	{ (args)->nfs_args_ext = NFS_ARGS_EXT2, \
	(args)->nfs_ext_u.nfs_ext2.secdata = secdata; }

extern int __clnt_bindresvport();

static int retry(struct mnttab *, int);
static int set_args(int *, struct nfs_args *, char *, struct mnttab *);
static int get_fh(struct nfs_args *, char *, char *, int *);
static int make_secure(struct nfs_args *, char *, struct netconfig *);
static int mount_nfs(struct mnttab *, int);
static int getaddr_nfs(struct nfs_args *, char *, struct netconfig **);
#ifdef __STDC__
static void pr_err(const char *fmt, ...);
#else
static void pr_err(char *fmt, va_dcl);
#endif
static void usage(void);
static struct netbuf *get_addr(char *, u_long, u_long, struct netconfig **,
			char *, u_short, struct t_info *);
static struct netbuf *get_the_addr(char *, u_long, u_long,
			struct netconfig *, u_short, struct t_info *);
static void fix_remount(char *);
static void getmyaddrs(struct ifconf *);
static int self_check(char *);

static char typename[64];

static int bg;
static int posix = 0;
static int retries = BIGRETRY;
static u_short nfs_port = 0;
static char *nfs_proto = NULL;

static int mflg = 0;
static int Oflg = 0;	/* Overlay mounts */

static char *fstype = MNTTYPE_NFS;

static seconfig_t nfs_sec;
static int sec_opt = 0;	/* any security option ? */

/*
 * These two variables control the NFS version number to be used.
 *
 * nfsvers defaults to 0 which means to use the highest number that
 * both the client and the server support.  It can also be set to
 * a particular value, either 2 or 3, to indicate the version
 * number of choice.  If the server (or the client) do not support
 * the version indicated, then the mount attempt will be failed.
 *
 * nfsvers_to_use is the actual version number found to use.  It
 * is determined in get_fh by pinging the various versions of the
 * NFS service on the server to see which responds positively.
 */
static u_long nfsvers = 0;
static u_long nfsvers_to_use = 0;

main(int argc, char **argv)
{
	struct mnttab mnt;
	extern char *optarg;
	extern int optind;
	char optbuf[256];
	int ro = 0;
	int r;
	int c;
	char *myname;

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	myname = strrchr(argv[0], '/');
	myname = myname ? myname + 1 : argv[0];
	(void) sprintf(typename, "%s %s", MNTTYPE_NFS, myname);
	argv[0] = typename;

	mnt.mnt_mntopts = optbuf;
	(void) strcpy(optbuf, "rw");

	/*
	 * Set options
	 */
	while ((c = getopt(argc, argv, "ro:mO")) != EOF) {
		switch (c) {
		case 'r':
			ro++;
			break;
		case 'o':
			(void) strcpy(mnt.mnt_mntopts, optarg);
#ifdef LATER					/* XXX */
			if (strstr(optarg, MNTOPT_REMOUNT)) {
				/*
				 * If remount is specified, only rw is allowed.
				 */
				if ((strcmp(optarg, MNTOPT_REMOUNT) != 0) &&
				    (strcmp(optarg, "remount,rw") != 0) &&
				    (strcmp(optarg, "rw,remount") != 0)) {
					pr_err(gettext("Invalid options\n"));
					exit(RET_ERR);
				}
			}
#endif /* LATER */				/* XXX */
			break;
		case 'm':
			mflg++;
			break;
		case 'O':
			Oflg++;
			break;
		default:
			usage();
			exit(RET_ERR);
		}
	}
	if (argc - optind != 2) {
		usage();
		exit(RET_ERR);
	}

	mnt.mnt_special = argv[optind];
	mnt.mnt_mountp = argv[optind+1];

	if (geteuid() != 0) {
		pr_err(gettext("not super user\n"));
		exit(RET_ERR);
	}

	r = mount_nfs(&mnt, ro);
	if (r == RET_RETRY && retries)
		r = retry(&mnt, ro);

	/*
	 * exit(r);
	 */
	return (r);
}

static void
#ifdef __STDC__
pr_err(const char *fmt, ...)
#else
pr_err(fmt, va_alist)
char *fmt;
va_dcl
#endif
{
	va_list ap;

	va_start(ap);
	(void) fprintf(stderr, "%s: ", typename);
	(void) vfprintf(stderr, fmt, ap);
	(void) fflush(stderr);
	va_end(ap);
}

static void
usage()
{
	(void) fprintf(stderr,
	    gettext("Usage: nfs mount [-r] [-o opts] server:path dir\n"));
	exit(RET_ERR);
}

static int
mount_nfs(struct mnttab *mntp, int ro)
{
	struct nfs_args *args = NULL, *argp = NULL, *prev_argp = NULL;
	struct netconfig *nconf = NULL;
	struct replica *list = NULL;
	int mntflags = 0;
	int i, r, n;
	int oldvers = 0, vers = 0;
	int last_error = RET_OK;
	int replicated = 0;
	char *p;

	mntp->mnt_fstype = MNTTYPE_NFS;

	if (ro) {
		mntflags |= MS_RDONLY;
		/* convert "rw"->"ro" */
		if (p = strstr(mntp->mnt_mntopts, "rw")) {
			if (*(p+2) == ',' || *(p+2) == '\0')
				*(p+1) = 'o';
		}
	}

	if (Oflg)
		mntflags |= MS_OVERLAY;

	list = parse_replica(mntp->mnt_special, &n);
	if (list == NULL) {
		if (n < 0)
			pr_err(gettext("nfs file system; use host:path\n"));
		else
			pr_err(gettext("no memory\n"));
		last_error = RET_ERR;
		goto out;
	}

	replicated = (n > 1);

	for (i = 0; i < n; i++) {

		argp = (struct nfs_args *) malloc(sizeof (*argp));
		if (argp == NULL) {
			pr_err(gettext("no memory\n"));
			last_error = RET_ERR;
			goto out;
		}
		memset(argp, 0, sizeof (*argp));
		if (prev_argp == NULL)
			args = argp;
		else
			prev_argp->nfs_ext_u.nfs_ext2.next = argp;
		prev_argp = argp;

		memset(&nfs_sec, 0, sizeof (nfs_sec));
		sec_opt = 0;

		if (r = set_args(&mntflags, argp, list[i].host, mntp)) {
			last_error = r;
			goto out;
		}

		if (replicated && !(mntflags & MS_RDONLY)) {
			pr_err(gettext(
				"replicated mounts must be read-only\n"));
			last_error = RET_ERR;
			goto out;
		}

		if (replicated && (argp->flags & NFSMNT_SOFT)) {
			pr_err(gettext(
				"replicated mounts must not be soft\n"));
			last_error = RET_ERR;
			goto out;
		}

		oldvers = vers;

		if (r = get_fh(argp, list[i].host, list[i].path, &vers)) {
			last_error = r;
			goto out;
		}

		if (oldvers && vers != oldvers) {
			pr_err(
			    gettext("replicas must have the same version\n"));
			last_error = RET_ERR;
			goto out;
		}

		/*
		 * decide whether to use remote host's
		 * lockd or do local locking
		 */
		if (!(argp->flags & NFSMNT_LLOCK) && vers == NFS_VERSION &&
			remote_lock(list[i].host, argp->fh)) {
			(void) printf(gettext(
			    "WARNING: No network locking on %s:%s:"),
			    list[i].host, list[i].path);
			(void) printf(gettext(
			    " contact admin to install server change\n"));
				argp->flags |= NFSMNT_LLOCK;
		}

		if (self_check(list[i].host))
			argp->flags |= NFSMNT_LOOPBACK;

		nconf = NULL;
		if (r = getaddr_nfs(argp, list[i].host, &nconf)) {
			last_error = r;
			goto out;
		}

		if (make_secure(argp, list[i].host, nconf) < 0) {
			last_error = RET_ERR;
			goto out;
		}
	}

	if (mount("", mntp->mnt_mountp, mntflags | MS_DATA, fstype,
		args, sizeof (*args)) < 0) {
		pr_err(gettext("mount: %s: %s\n"),
		    mntp->mnt_mountp, strerror(errno));
		last_error = RET_ERR;
		goto out;
	}

	if (!mflg) {
		if (mntflags & MS_REMOUNT)
			fix_remount(mntp->mnt_mountp);
		else {
			if (fsaddtomtab(mntp)) {
				last_error = RET_ERR;
				goto out;
			}
		}
	}
out:
	if (list)
		free_replica(list, n);
	argp = args;
	while (argp != NULL) {
		if (argp->fh)
			free(argp->fh);
		if (argp->pathconf)
			free(argp->pathconf);
		if (argp->knconf)
			free(argp->knconf);
		if (argp->addr)
			free(argp->addr);
		nfs_free_secdata(argp->nfs_ext_u.nfs_ext2.secdata);
		prev_argp = argp;
		argp = argp->nfs_ext_u.nfs_ext2.next;
		free(prev_argp);
	}

	return (last_error);
}

static char *optlist[] = {
#define	OPT_RO		0
	MNTOPT_RO,
#define	OPT_RW		1
	MNTOPT_RW,
#define	OPT_QUOTA	2
	MNTOPT_QUOTA,
#define	OPT_NOQUOTA	3
	MNTOPT_NOQUOTA,
#define	OPT_SOFT	4
	MNTOPT_SOFT,
#define	OPT_HARD	5
	MNTOPT_HARD,
#define	OPT_SUID	6
	MNTOPT_SUID,
#define	OPT_NOSUID	7
	MNTOPT_NOSUID,
#define	OPT_GRPID	8
	MNTOPT_GRPID,
#define	OPT_REMOUNT	9
	MNTOPT_REMOUNT,
#define	OPT_NOSUB	10
	MNTOPT_NOSUB,
#define	OPT_INTR	11
	MNTOPT_INTR,
#define	OPT_NOINTR	12
	MNTOPT_NOINTR,
#define	OPT_PORT	13
	MNTOPT_PORT,
#define	OPT_SECURE	14
	MNTOPT_SECURE,
#define	OPT_RSIZE	15
	MNTOPT_RSIZE,
#define	OPT_WSIZE	16
	MNTOPT_WSIZE,
#define	OPT_TIMEO	17
	MNTOPT_TIMEO,
#define	OPT_RETRANS	18
	MNTOPT_RETRANS,
#define	OPT_ACTIMEO	19
	MNTOPT_ACTIMEO,
#define	OPT_ACREGMIN	20
	MNTOPT_ACREGMIN,
#define	OPT_ACREGMAX	21
	MNTOPT_ACREGMAX,
#define	OPT_ACDIRMIN	22
	MNTOPT_ACDIRMIN,
#define	OPT_ACDIRMAX	23
	MNTOPT_ACDIRMAX,
#define	OPT_BG		24
	MNTOPT_BG,
#define	OPT_FG		25
	MNTOPT_FG,
#define	OPT_RETRY	26
	MNTOPT_RETRY,
#define	OPT_NOAC	27
	MNTOPT_NOAC,
#define	OPT_KERB	28
	MNTOPT_KERB,
#define	OPT_NOCTO	29
	MNTOPT_NOCTO,
#define	OPT_LLOCK	30
	MNTOPT_LLOCK,
#define	OPT_POSIX	31
	MNTOPT_POSIX,
#define	OPT_VERS	32
	MNTOPT_VERS,
#define	OPT_PROTO	33
	MNTOPT_PROTO,
#define	OPT_SEMISOFT	34
	MNTOPT_SEMISOFT,
#define	OPT_NOPRINT	35
	MNTOPT_NOPRINT,
#define	OPT_SEC		36
	MNTOPT_SEC,
#define	OPT_LARGEFILES	37
	MNTOPT_LARGEFILES,
#define	OPT_NOLARGEFILES	38
	MNTOPT_NOLARGEFILES,
	NULL
};

#define	bad(val) (val == NULL || !isdigit(*val))

static int
set_args(int *mntflags, struct nfs_args *args, char *fshost, struct mnttab *mnt)
{
	char *saveopt, *optstr, *opts, *val;
	int largefiles = 0;

	args->flags = NFSMNT_INT;	/* default is "intr" */
	args->flags |= NFSMNT_HOSTNAME;
	args->flags |= NFSMNT_NEWARGS;	/* using extented nfs_args structure */
	args->hostname = fshost;

	optstr = opts = strdup(mnt->mnt_mntopts);
	if (opts == NULL) {
		pr_err(gettext("no memory"));
		return (RET_ERR);
	}

	while (*opts) {
		saveopt = opts;
		switch (getsubopt(&opts, optlist, &val)) {
		case OPT_RO:
			*mntflags |= MS_RDONLY;
			break;
		case OPT_RW:
			*mntflags &= ~(MS_RDONLY);
			break;
		case OPT_QUOTA:
		case OPT_NOQUOTA:
			break;
		case OPT_SOFT:
			args->flags |= NFSMNT_SOFT;
			args->flags &= ~(NFSMNT_SEMISOFT);
			break;
		case OPT_SEMISOFT:
			args->flags |= NFSMNT_SOFT;
			args->flags |= NFSMNT_SEMISOFT;
			break;
		case OPT_HARD:
			args->flags &= ~(NFSMNT_SOFT);
			args->flags &= ~(NFSMNT_SEMISOFT);
			break;
		case OPT_SUID:
			*mntflags &= ~(MS_NOSUID);
			break;
		case OPT_NOSUID:
			*mntflags |= MS_NOSUID;
			break;
		case OPT_GRPID:
			args->flags |= NFSMNT_GRPID;
			break;
		case OPT_REMOUNT:
			*mntflags |= MS_REMOUNT;
			break;
		case OPT_INTR:
			args->flags |= NFSMNT_INT;
			break;
		case OPT_NOINTR:
			args->flags &= ~(NFSMNT_INT);
			break;
		case OPT_NOAC:
			args->flags |= NFSMNT_NOAC;
			break;
		case OPT_PORT:
			if (bad(val))
				goto badopt;
			nfs_port = htons((u_short)atoi(val));
			break;

		case OPT_SECURE:
			if (nfs_getseconfig_byname("dh", &nfs_sec)) {
			    pr_err(gettext("can not get \"dh\" from %s\n"),
						NFSSEC_CONF);
			    goto badopt;
			}
			sec_opt++;
			break;

		case OPT_KERB:
			if (nfs_getseconfig_byname("krb4", &nfs_sec)) {
			    pr_err(gettext("can not get \"krb4\" from %s\n"),
						NFSSEC_CONF);
			    goto badopt;
			}
			sec_opt++;
			break;

		case OPT_NOCTO:
			args->flags |= NFSMNT_NOCTO;
			break;

		case OPT_RSIZE:
			args->flags |= NFSMNT_RSIZE;
			if (bad(val))
				goto badopt;
			args->rsize = atoi(val);
			break;
		case OPT_WSIZE:
			args->flags |= NFSMNT_WSIZE;
			if (bad(val))
				goto badopt;
			args->wsize = atoi(val);
			break;
		case OPT_TIMEO:
			args->flags |= NFSMNT_TIMEO;
			if (bad(val))
				goto badopt;
			args->timeo = atoi(val);
			break;
		case OPT_RETRANS:
			args->flags |= NFSMNT_RETRANS;
			if (bad(val))
				goto badopt;
			args->retrans = atoi(val);
			break;
		case OPT_ACTIMEO:
			args->flags |= NFSMNT_ACDIRMAX;
			args->flags |= NFSMNT_ACREGMAX;
			args->flags |= NFSMNT_ACDIRMIN;
			args->flags |= NFSMNT_ACREGMIN;
			if (bad(val))
				goto badopt;
			args->acdirmin = args->acregmin = args->acdirmax
				= args->acregmax = atoi(val);
			break;
		case OPT_ACREGMIN:
			args->flags |= NFSMNT_ACREGMIN;
			if (bad(val))
				goto badopt;
			args->acregmin = atoi(val);
			break;
		case OPT_ACREGMAX:
			args->flags |= NFSMNT_ACREGMAX;
			if (bad(val))
				goto badopt;
			args->acregmax = atoi(val);
			break;
		case OPT_ACDIRMIN:
			args->flags |= NFSMNT_ACDIRMIN;
			if (bad(val))
				goto badopt;
			args->acdirmin = atoi(val);
			break;
		case OPT_ACDIRMAX:
			args->flags |= NFSMNT_ACDIRMAX;
			if (bad(val))
				goto badopt;
			args->acdirmax = atoi(val);
			break;
		case OPT_BG:
			bg++;
			break;
		case OPT_FG:
			bg = 0;
			break;
		case OPT_RETRY:
			if (bad(val))
				goto badopt;
			retries = atoi(val);
			break;
		case OPT_LLOCK:
			args->flags |= NFSMNT_LLOCK;
			break;
		case OPT_POSIX:
			posix = 1;
			break;
		case OPT_VERS:
			if (bad(val))
				goto badopt;
			nfsvers = (u_long)atoi(val);
			break;
		case OPT_PROTO:
			nfs_proto = (char *)malloc(strlen(val)+1);
			(void) strcpy(nfs_proto, val);
			break;
		case OPT_NOPRINT:
			args->flags |= NFSMNT_NOPRINT;
			break;
		case OPT_LARGEFILES:
			largefiles = 1;
			break;
		case OPT_NOLARGEFILES:
			pr_err(gettext("NFS can't support \"nolargefiles\"\n"));
			free(optstr);
			return (RET_ERR);

		case OPT_SEC:
			if (nfs_getseconfig_byname(val, &nfs_sec)) {
			    pr_err(gettext("can not get \"%s\" from %s\n"),
						val, NFSSEC_CONF);
			    return (RET_ERR);
			}
			sec_opt++;
			break;

		default:
			pr_err(gettext("ignoring invalid option \"%s\"\n"),
					val);
			break;
		}
	}
	free(optstr);

	/* ensure that only one secure mode is requested */
	if (sec_opt > 1) {
		pr_err(gettext("Security options conflict\n"));
		return (RET_ERR);
	}

	/* ensure that the user isn't trying to get large files over V2 */
	if (nfsvers == NFS_VERSION && largefiles) {
		pr_err(gettext("NFS V2 can't support \"largefiles\"\n"));
		return (RET_ERR);
	}

	return (RET_OK);

badopt:
	pr_err(gettext("invalid option: \"%s\"\n"), saveopt);
	free(optstr);
	return (RET_ERR);
}

static int
make_secure(struct nfs_args *args, char *hostname, struct netconfig *nconf)
{
	sec_data_t *secdata;
	int flags;
	struct netbuf *syncaddr;

	/*
	 * check to see if any secure mode is requested.
	 * if not, use default security mode.
	 */
	if (!sec_opt) {
		/*
		 *  Get default security mode.
		 *  AUTH_UNIX has been the default choice for a long time.
		 *  The better NFS security service becomes, the better chance
		 *  we will set stronger security service as the default NFS
		 *  security mode.
		 *
		 */
		if (nfs_getseconfig_default(&nfs_sec)) {
		    pr_err(gettext("error getting default security entry\n"));
		    return (-1);
		}
	}

	/*
	 * Get the network address for the time service on
	 * the server.  If an RPC based time service is
	 * not available then try the IP time service.
	 */
	flags = (int) 0;
	syncaddr = NULL;
	/*
	 * Get the network address for the time service on
	 * the server.  If an RPC based time service is
	 * not available then try the IP time service.
	 *
	 * Eventurally, we want to move this code to nfs_clnt_secdata()
	 * when autod_nfs.c and mount.c can share the same get_the_addr()
	 * routine.
	 */
	if ((nfs_sec.sc_rpcnum == AUTH_DES) ||
	    (nfs_sec.sc_rpcnum == AUTH_KERB)) {
		syncaddr = get_the_addr(hostname, RPCBPROG, RPCBVERS, nconf,
						0, NULL);
		if (syncaddr != NULL) {
			flags |= AUTH_F_RPCTIMESYNC;
		} else {
			struct nd_hostserv hs;
			struct nd_addrlist *retaddrs;

			hs.h_host = hostname;
			hs.h_serv = "rpcbind";
			if (netdir_getbyname(nconf, &hs, &retaddrs) != ND_OK)
				goto err;
			syncaddr = retaddrs->n_addrs;
			/* LINTED pointer alignment */
			((struct sockaddr_in *) syncaddr->buf)->sin_port
				= htons(IPPORT_TIMESERVER);
		}
	}

	if (!(secdata = nfs_clnt_secdata(&nfs_sec, hostname, args->knconf,
					syncaddr, flags))) {
		pr_err("errors constructing security related data\n");
		return (-1);
	}

	NFS_ARGS_EXT2_secdata(args, secdata);
	return (0);

err:
	pr_err(gettext("%s: secure: no time service\n"), hostname);
	return (-1);
}

/*
 * Get the network address on "hostname" for program "prog"
 * with version "vers" by using the nconf configuration data
 * passed in.
 *
 * If the address of a netconfig pointer is null then
 * information is not sufficient and no netbuf will be returned.
 *
 * Finally, ping the null procedure of that service.
 *
 * A similar routine is also defined in ../../autofs/autod_nfs.c.
 * This is a potential routine to move to ../lib for common usage.
 */
static struct netbuf *
get_the_addr(char *hostname, u_long prog, u_long vers,
	struct netconfig *nconf, u_short port, struct t_info *tinfo)
{
	struct netbuf *nb = NULL;
	struct t_bind *tbind = NULL;
	enum clnt_stat cs;
	CLIENT *cl = NULL;
	struct timeval tv;
	int fd = -1;

	if (nconf == NULL)
		return (NULL);

	if ((fd = t_open(nconf->nc_device, O_RDWR, tinfo)) == -1)
		    goto done;

	/* LINTED pointer alignment */
	if ((tbind = (struct t_bind *) t_alloc(fd, T_BIND, T_ADDR))
		== NULL)
		goto done;

	if (rpcb_getaddr(prog, vers, nconf, &tbind->addr, hostname) == 0)
		goto done;

	if (port) {
		/* LINTED pointer alignment */
		((struct sockaddr_in *) tbind->addr.buf)->sin_port = port;
	}

	cl = clnt_tli_create(fd, nconf, &tbind->addr, prog, vers, 0, 0);
	if (cl == NULL)
		goto done;

	tv.tv_sec = 5;
	tv.tv_usec = 0;
	cs = clnt_call(cl, NULLPROC, xdr_void, 0, xdr_void, 0, tv);
	if (cs != RPC_SUCCESS)
		goto done;

	/*
	 * Make a copy of the netbuf to return
	 */
	nb = (struct netbuf *) malloc(sizeof (*nb));
	if (nb == NULL) {
		pr_err(gettext("no memory\n"));
		goto done;
	}
	*nb = tbind->addr;
	nb->buf = (char *)malloc(nb->maxlen);
	if (nb->buf == NULL) {
		pr_err(gettext("no memory\n"));
		free(nb);
		nb = NULL;
		goto done;
	}
	(void) memcpy(nb->buf, tbind->addr.buf, tbind->addr.len);

done:
	if (cl) {
		clnt_destroy(cl);
		cl = NULL;
	}
	if (tbind) {
		t_free((char *) tbind, T_BIND);
		tbind = NULL;
	}
	if (fd >= 0)
		(void) t_close(fd);
	return (nb);
}

/*
 * Get a network address on "hostname" for program "prog"
 * with version "vers".  If the port number is specified (non zero)
 * then try for a TCP/UDP transport and set the port number of the
 * resulting IP address.
 *
 * If the address of a netconfig pointer was passed and
 * if it's not null, use it as the netconfig otherwise
 * assign the address of the netconfig that was used to
 * establish contact with the service.
 *
 * A similar routine is also defined in ../../autofs/autod_nfs.c.
 * This is a potential routine to move to ../lib for common usage.
 */
static struct netbuf *
get_addr(char *hostname, u_long prog, u_long vers, struct netconfig **nconfp,
	char *proto, u_short port, struct t_info *tinfo)
{
	struct netbuf *nb = NULL;
	struct netconfig *nconf = NULL;
	NCONF_HANDLE *nc = NULL;
	int nthtry = FIRST_TRY;

	if (nconfp && *nconfp)
		return (get_the_addr(hostname, prog, vers, *nconfp,
					port, tinfo));
	/*
	 * No nconf passed in.
	 *
	 * Try to get a nconf from /etc/netconfig filtered by
	 * the NETPATH environment variable.
	 * First search for COTS, second for CLTS unless proto
	 * is specified.  When we retry, we reset the
	 * netconfig list so that we would search the whole list
	 * all over again.
	 */
	if ((nc = setnetpath()) == NULL)
		goto done;

	/*
	 * If proto is specified, then only search for the match,
	 * otherwise try COTS first, if failed, try CLTS.
	 */
	if (proto) {
		while (nconf = getnetpath(nc)) {
			if (strcmp(nconf->nc_netid, proto) == 0) {
			/*
			 * If the port number is specified then TCP/UDP
			 * is needed. Otherwise any cots/clts will do.
			 */
			    if (port == 0)
				break;
			    if ((strcmp(nconf->nc_protofmly, NC_INET) == 0) &&
				((strcmp(nconf->nc_proto, NC_TCP) == 0) ||
				(strcmp(nconf->nc_proto, NC_UDP) == 0)))
					break;
			    else {
				nconf = NULL;
				break;
			    }
			}
		}
		if (nconf == NULL)
			goto done;
		if ((nb = get_the_addr(hostname, prog, vers, nconf, port,
					tinfo)) == NULL)
			goto done;
	} else {
retry:
		while (nconf = getnetpath(nc)) {
			if (nconf->nc_flag & NC_VISIBLE) {
			    if (nthtry == FIRST_TRY) {
				if ((nconf->nc_semantics == NC_TPI_COTS_ORD) ||
					(nconf->nc_semantics == NC_TPI_COTS)) {
				    if (port == 0)
					break;
				    if ((strcmp(nconf->nc_protofmly,
					NC_INET) == 0) &&
					(strcmp(nconf->nc_proto, NC_TCP) == 0))
					break;
				}
			    }
			    if (nthtry == SECOND_TRY) {
				if (nconf->nc_semantics == NC_TPI_CLTS) {
				    if (port == 0)
					break;
				    if ((strcmp(nconf->nc_protofmly,
					NC_INET) == 0) &&
					(strcmp(nconf->nc_proto, NC_UDP) == 0))
					break;
				}
			    }
			}
		} /* while */
		if (nconf == NULL) {
			if (++nthtry <= MNT_PREF_LISTLEN) {
				endnetpath(nc);
				if ((nc = setnetpath()) == NULL)
					goto done;
				goto retry;
			} else
				goto done;
		} else {
		    if ((nb = get_the_addr(hostname, prog, vers, nconf,
					port, tinfo)) == NULL) {
			/*
			 * Continue the same search path in the netconfig db
			 * until no more matched nconf (nconf == NULL).
			 */
			goto retry;
		    }

		}
	}

	/*
	 * Got nconf and nb.  Now dup the netconfig structure (nconf)
	 * and return it thru nconfp.
	 */
	*nconfp = getnetconfigent(nconf->nc_netid);
	if (*nconfp == NULL) {
		syslog(LOG_ERR, "no memory\n");
		free(nb);
		nb = NULL;
	}
done:
	if (nc)
		endnetpath(nc);
	return (nb);
}

/*
 * get fhandle of remote path from server's mountd
 */
static int
get_fh(struct nfs_args *args, char *fshost, char *fspath, int *versp)
{
	static struct fhstatus fhs;
	static struct mountres3 mountres3;
	static struct pathcnf p;
	nfs_fh3 *fh3p;
	struct timeval timeout = { 25, 0};
	CLIENT *cl;
	enum clnt_stat rpc_stat;
	u_long outvers = 0;
	u_long vers_to_try;
	u_long vers_min;
	static int printed = 0;
	int count, i, *auths;
	char *msg;

	if (nfsvers == 2) {
		vers_to_try = MOUNTVERS_POSIX;
		vers_min = MOUNTVERS;
	} else if (nfsvers == 3) {
		vers_to_try = MOUNTVERS3;
		vers_min = MOUNTVERS3;
	} else {
		vers_to_try = MOUNTVERS3;
		vers_min = MOUNTVERS;
	}

	while ((cl = clnt_create_vers(fshost, MOUNTPROG, &outvers,
			vers_min, vers_to_try, "datagram_v")) == NULL) {
		if (rpc_createerr.cf_stat == RPC_UNKNOWNHOST) {
			pr_err(gettext("%s: %s\n"), fshost,
			    clnt_spcreateerror(""));
			return (RET_ERR);
		}

		/*
		 * back off and try the previous version - patch to the
		 * problem of version numbers not being contigous and
		 * clnt_create_vers failing (SunOS4.1 clients & SGI servers)
		 * The problem happens with most non-Sun servers who
		 * don't support mountd protocol #2. So, in case the
		 * call fails, we re-try the call anyway.
		 */
		vers_to_try--;
		if (vers_to_try < vers_min) {
			if (rpc_createerr.cf_stat == RPC_PROGVERSMISMATCH) {
				if (nfsvers == 0) {
					pr_err(gettext(
			"%s:%s: no applicable versions of NFS supported\n"),
					    fshost, fspath);
				} else {
					pr_err(gettext(
			"%s:%s: NFS Version %d not supported\n"),
					    fshost, fspath, nfsvers);
				}
				return (RET_ERR);
			}
			if (!printed) {
				pr_err(gettext("%s: %s\n"), fshost,
				    clnt_spcreateerror(""));
				printed = 1;
			}
			return (RET_RETRY);
		}
	}
	if (posix && outvers < MOUNTVERS_POSIX) {
		pr_err(gettext("%s: %s: no pathconf info\n"),
		    fshost, clnt_sperror(cl, ""));
		clnt_destroy(cl);
		return (RET_ERR);
	}

	if (__clnt_bindresvport(cl) < 0) {
		pr_err(gettext("Couldn't bind to reserved port\n"));
		clnt_destroy(cl);
		return (RET_RETRY);
	}

	cl->cl_auth = authsys_create_default();

	switch (outvers) {
	case MOUNTVERS:
	case MOUNTVERS_POSIX:
		nfsvers_to_use = NFS_VERSION;
		rpc_stat = clnt_call(cl, MOUNTPROC_MNT, xdr_dirpath,
			(caddr_t)&fspath, xdr_fhstatus, (caddr_t)&fhs, timeout);
		if (rpc_stat != RPC_SUCCESS) {
			pr_err(gettext("%s:%s: server not responding %s\n"),
			    fshost, fspath, clnt_sperror(cl, ""));
			clnt_destroy(cl);
			return (RET_RETRY);
		}

		if ((errno = fhs.fhs_status) != MNT_OK) {
			if (errno == EACCES) {
				pr_err(gettext("%s:%s: access denied\n"),
				    fshost, fspath);
			} else {
				pr_err(gettext("%s:%s: "), fshost, fspath);
				perror("");
			}
			clnt_destroy(cl);
			return (RET_ERR);
		}
		args->fh = malloc(sizeof (fhs.fhstatus_u.fhs_fhandle));
		if (args->fh == NULL) {
			pr_err(gettext("no memory\n"));
			return (RET_ERR);
		}
		memcpy((caddr_t)args->fh, (caddr_t)&fhs.fhstatus_u.fhs_fhandle,
			sizeof (fhs.fhstatus_u.fhs_fhandle));
		if (!errno && posix) {
			rpc_stat = clnt_call(cl, MOUNTPROC_PATHCONF,
				xdr_dirpath, (caddr_t)&fspath, xdr_ppathcnf,
				(caddr_t)&p, timeout);
			if (rpc_stat != RPC_SUCCESS) {
				pr_err(gettext(
				    "%s:%s: server not responding %s\n"),
				    fshost, fspath, clnt_sperror(cl, ""));
				free(args->fh);
				clnt_destroy(cl);
				return (RET_RETRY);
			}
			if (_PC_ISSET(_PC_ERROR, p.pc_mask)) {
				pr_err(gettext(
				    "%s:%s: no pathconf info\n"),
				    fshost, fspath);
				free(args->fh);
				clnt_destroy(cl);
				return (RET_ERR);
			}
			args->flags |= NFSMNT_POSIX;
			args->pathconf = malloc(sizeof (p));
			if (args->pathconf == NULL) {
				pr_err(gettext("no memory\n"));
				free(args->fh);
				clnt_destroy(cl);
				return (RET_ERR);
			}
			memcpy((caddr_t)args->pathconf, (caddr_t)&p,
				sizeof (p));
		}
		break;

	case MOUNTVERS3:
		nfsvers_to_use = NFS_V3;
		rpc_stat = clnt_call(cl, MOUNTPROC_MNT, xdr_dirpath,
				(caddr_t)&fspath,
				xdr_mountres3, (caddr_t)&mountres3, timeout);
		if (rpc_stat != RPC_SUCCESS) {
			pr_err(gettext("%s:%s: server not responding %s\n"),
			    fshost, fspath, clnt_sperror(cl, ""));
			clnt_destroy(cl);
			return (RET_RETRY);
		}

		/*
		 * Assume here that most of the MNT3ERR_*
		 * codes map into E* errors.
		 */
		if ((errno = mountres3.fhs_status) != MNT_OK) {
			switch (errno) {
			case MNT3ERR_NAMETOOLONG:
				msg = "path name is too long";
				break;
			case MNT3ERR_NOTSUPP:
				msg = "operation not supported";
				break;
			case MNT3ERR_SERVERFAULT:
				msg = "server fault";
				break;
			default:
				msg = NULL;
				break;
			}
			if (msg)
				pr_err(gettext("%s:%s: "), fshost, fspath);
			else
				perror("");
			clnt_destroy(cl);
			return (RET_ERR);
		}

		fh3p = (nfs_fh3 *)malloc(sizeof (*fh3p));
		if (fh3p == NULL) {
			pr_err(gettext("no memory\n"));
			return (RET_ERR);
		}
		fh3p->fh3_length =
			mountres3.mountres3_u.mountinfo.fhandle.fhandle3_len;
		(void) memcpy(fh3p->fh3_u.data,
			mountres3.mountres3_u.mountinfo.fhandle.fhandle3_val,
			fh3p->fh3_length);
		args->fh = (caddr_t)fh3p;
		fstype = MNTTYPE_NFS3;

		/*
		 * Check the security flavor to be used.
		 *
		 * If "secure" or "kerberos" or "sec=flavor" is a mount
		 * option, check if the server supports the "flavor".
		 * If the server does not support the flavor, return
		 * error.
		 *
		 * If no mount option is given then use the first supported
		 * security flavor (by the client) in the auth list returned
		 * from the server.
		 *
		 */
		auths =
		mountres3.mountres3_u.mountinfo.auth_flavors.auth_flavors_val;
		count =
		mountres3.mountres3_u.mountinfo.auth_flavors.auth_flavors_len;

		if (sec_opt) {
			for (i = 0; i < count; i++) {
				if (auths[i] == nfs_sec.sc_nfsnum)
				    break;
			}
			if (i >= count) {
				goto autherr;
			}
		} else {
		    if (count > 0) {
			for (i = 0; i < count; i++) {
			    if (!nfs_getseconfig_bynumber(auths[i], &nfs_sec)) {
				sec_opt++;
				break;
			    }
			}
			if (i >= count) {
			    goto autherr;
			}
		    }
		}
		break;
	default:
		pr_err(gettext("%s:%s: Unknown MOUNT version %d\n"),
		    fshost, fspath, outvers);
		clnt_destroy(cl);
		return (RET_ERR);
	}
	*versp = outvers;

	clnt_destroy(cl);
	return (RET_OK);

autherr:
pr_err(gettext("server %s shares %s with these security modes, none\n"
	"\tof which are configured on this client:"), fshost, fspath);
	for (i = 0; i < count; i++) {
		printf(" %d", auths[i]);
	}
	printf("\n");
	clnt_destroy(cl);
	return (RET_ERR);
}

/*
 * Fill in the address for the server's NFS service and
 * fill in a knetconfig structure for the transport that
 * the service is available on.
 */
static int
getaddr_nfs(struct nfs_args *args, char *fshost, struct netconfig **nconfp)
{
	struct stat sb;
	struct netconfig *nconf;
	struct knetconfig *knconfp;
	static int printed = 0;
	struct t_info tinfo;

	args->addr = get_addr(fshost, NFS_PROGRAM, nfsvers_to_use,
			nconfp, nfs_proto, nfs_port, &tinfo);

	if (args->addr == NULL) {
		if (!printed) {
			pr_err(gettext("%s: NFS service not responding\n"),
			    fshost);
			printed = 1;
		}
		return (RET_RETRY);
	}
	nconf = *nconfp;

	if (stat(nconf->nc_device, &sb) < 0) {
		pr_err(gettext("getaddr_nfs: couldn't stat: %s: %s\n"),
		    nconf->nc_device, strerror(errno));
		return (RET_ERR);
	}

	knconfp = (struct knetconfig *) malloc(sizeof (*knconfp));
	if (!knconfp) {
		pr_err(gettext("no memory\n"));
		return (RET_ERR);
	}
	knconfp->knc_semantics = nconf->nc_semantics;
	knconfp->knc_protofmly = nconf->nc_protofmly;
	knconfp->knc_proto = nconf->nc_proto;
	knconfp->knc_rdev = sb.st_rdev;

	/* make sure we don't overload the transport */
	if (tinfo.tsdu > 0 && tinfo.tsdu < NFS_MAXDATA + NFS_RPC_HDR) {
		args->flags |= (NFSMNT_RSIZE | NFSMNT_WSIZE);
		if (args->rsize == 0 || args->rsize > tinfo.tsdu - NFS_RPC_HDR)
			args->rsize = tinfo.tsdu - NFS_RPC_HDR;
		if (args->wsize == 0 || args->wsize > tinfo.tsdu - NFS_RPC_HDR)
			args->wsize = tinfo.tsdu - NFS_RPC_HDR;
	}

	args->flags |= NFSMNT_KNCONF;
	args->knconf = knconfp;
	return (RET_OK);
}

static int
retry(struct mnttab *mntp, int ro)
{
	int delay = 5;
	int count = retries;
	int r;

	if (bg) {
		if (fork() > 0)
			return (RET_OK);
		pr_err(gettext("backgrounding: %s\n"), mntp->mnt_mountp);
	} else
		pr_err(gettext("retrying: %s\n"), mntp->mnt_mountp);

	while (count--) {
		if ((r = mount_nfs(mntp, ro)) == RET_OK) {
			pr_err(gettext("%s: mounted OK\n"), mntp->mnt_mountp);
			return (RET_OK);
		}
		if (r != RET_RETRY)
			break;

		(void) sleep(delay);
		delay *= 2;
		if (delay > 120)
			delay = 120;
	}
	pr_err(gettext("giving up on: %s\n"), mntp->mnt_mountp);
	return (RET_ERR);
}



/*
 * Fix remount entry in /etc/mnttab.
 * This routine is modified from delete_mnttab in umount.
 */
static void
fix_remount(char *mntpnt)
{
	FILE *fp;
	struct mnttab mnt;
	mntlist_t *mntl_head = NULL;
	mntlist_t *mntl_prev = NULL, *mntl;
	mntlist_t *modify = NULL;
	int mlock;

	mlock = fslock_mnttab();

	fp = fopen(MNTTAB, "r+");
	if (fp == NULL) {
		perror(MNTTAB);
		fsunlock_mnttab(mlock);
		exit(RET_ERR);
	}

	(void) lockf(fileno(fp), F_LOCK, 0L);

	/*
	 * Read the entire mnttab into memory.
	 * Remember the *last* instance of the mounted
	 * mount point (have to take stacked mounts into
	 * account) and make sure that it's updated.
	 */
	while (getmntent(fp, &mnt) == 0) {
		mntl = (mntlist_t *) malloc(sizeof (*mntl));
		if (mntl == NULL) {
			pr_err(gettext("no memory\n"));
			(void) fclose(fp);
			fsunlock_mnttab(mlock);
			exit(RET_ERR);
		}
		if (mntl_head == NULL)
			mntl_head = mntl;
		else
			mntl_prev->mntl_next = mntl;
		mntl_prev = mntl;
		mntl->mntl_next = NULL;
		mntl->mntl_mnt = fsdupmnttab(&mnt);
		if (mntl->mntl_mnt == NULL) {
			(void) fclose(fp);
			fsunlock_mnttab(mlock);
			exit(RET_ERR);
		}
		if (strcmp(mnt.mnt_mountp, mntpnt) == 0)
			modify = mntl;
	}

	/* Now truncate the mnttab and write it back with the modified entry. */

	(void) signal(SIGHUP, SIG_IGN);
	(void) signal(SIGQUIT, SIG_IGN);
	(void) signal(SIGINT, SIG_IGN);

	rewind(fp);
	if (ftruncate(fileno(fp), 0) < 0) {
		pr_err(gettext("truncate %s: %s\n"), MNTTAB, strerror(errno));
		(void) fclose(fp);
		fsunlock_mnttab(mlock);
		exit(RET_ERR);
	}

	for (mntl = mntl_head; mntl; mntl = mntl->mntl_next) {
		if (mntl == modify) {
			char	*p;

			/* 'ro' -> 'rw' */
			if (p = strstr((modify->mntl_mnt)->mnt_mntopts, "ro"))
				if (*(p+2) == ',' || *(p+2) == '\0')
					*(p+1) = 'w';
			(void) sprintf((modify->mntl_mnt)->mnt_time, "%ld",
				time(0L));
		}

		if (putmntent(fp, mntl->mntl_mnt) <= 0) {
			pr_err(gettext("putmntent"));
			perror("");
			(void) fclose(fp);
			fsunlock_mnttab(mlock);
			exit(RET_ERR);
		}
	}

	(void) fclose(fp);
	fsunlock_mnttab(mlock);
}

#define	MAXIFS 32

/*
 * XXX - The following is stolen from autod_nfs.c, this should probably
 * be moved to a library.
 */
static void
getmyaddrs(struct ifconf *ifc)
{
	int sock;
	int numifs;
	char *buf;

	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("getmyaddrs(): socket");
		return;
	}

	if (ioctl(sock, SIOCGIFNUM, (char *)&numifs) < 0) {
		perror("getmyaddrs(): SIOCGIFNUM");
		numifs = MAXIFS;
	}

	buf = (char *) malloc(numifs * sizeof (struct ifreq));
	if (buf == NULL) {
		fprintf(stderr, "getmyaddrs(): malloc failed\n");
		(void) close(sock);
		return;
	}

	ifc->ifc_buf = buf;
	ifc->ifc_len = numifs * sizeof (struct ifreq);

	if (ioctl(sock, SIOCGIFCONF, (char *) ifc) < 0)
		perror("getmyaddrs(): SIOCGIFCONF");

	(void) close(sock);
}

static int
self_check(char *hostname)
{
	int n;
	struct sockaddr_in *s1, *s2;
	struct ifreq *ifr;
	struct nd_hostserv hs;
	struct nd_addrlist *retaddrs;
	struct netconfig *nconfp;
	struct ifconf *ifc;
	int retval;

	ifc = malloc(sizeof (struct ifconf));
	if (ifc == NULL)
		return (1);
	memset((char *)ifc, 0, sizeof (struct ifconf));
	getmyaddrs(ifc);
	/*
	 * Get the IP address for hostname
	 */
	nconfp = getnetconfigent("udp");
	if (nconfp == NULL) {
		fprintf(stderr, "self_check(): getnetconfigent failed\n");
		retval = 1;
		goto out;
	}
	hs.h_host = hostname;
	hs.h_serv = "rpcbind";
	if (netdir_getbyname(nconfp, &hs, &retaddrs) != ND_OK) {
		freenetconfigent(nconfp);
		retval = 1;
		goto out;
	}
	freenetconfigent(nconfp);
	/* LINTED pointer alignment */
	s1 = (struct sockaddr_in *) retaddrs->n_addrs->buf;

	/*
	 * Now compare it against the list of
	 * addresses for the interfaces on this
	 * host.
	 */
	ifr = ifc->ifc_req;
	n = ifc->ifc_len / sizeof (struct ifreq);
	s2 = NULL;
	for (; n > 0; n--, ifr++) {
		if (ifr->ifr_addr.sa_family != AF_INET)
			continue;

		/* LINTED pointer alignment */
		s2 = (struct sockaddr_in *) &ifr->ifr_addr;

		if (memcmp((char *) &s2->sin_addr,
			(char *) &s1->sin_addr, 4) == 0) {
			netdir_free((void *) retaddrs, ND_ADDRLIST);
			retval = 1;
			goto out;	/* it's me */
		}
	}
	netdir_free((void *) retaddrs, ND_ADDRLIST);
	retval = 0;

out:
	if (ifc->ifc_buf != NULL)
		free(ifc->ifc_buf);
	free(ifc);
	return (retval);
}
