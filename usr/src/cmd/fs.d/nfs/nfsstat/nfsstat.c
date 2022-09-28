/* LINTLIBRARY */
/* PROTOLIB1 */

/*
 * Copyright (c) 1984,1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)nfsstat.c	1.28	96/04/08 SMI"	/* SVr4.0 1.9	*/

/*
 * nfsstat: Network File System statistics
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <nlist.h>
#include <fcntl.h>
#include <kvm.h>
#include <kstat.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/tiuser.h>
#include <sys/statvfs.h>
#include <sys/mntent.h>
#include <sys/mnttab.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <nfs/nfs.h>
#include <nfs/nfs_clnt.h>
#include <nfs/nfs_sec.h>

static struct nlist nl[] = {
#define	X_ROOTVFS	0
			{ "rootvfs" },
#define	X_NFS_VFSOPS	1
			{ "nfs_vfsops" },
#define	X_NFS3_VFSOPS	2
			{ "nfs3_vfsops" },
#define	X_END		3
			{ "" }
};

static kvm_t *kd;			/* kernel id from kvm_open */
static kstat_ctl_t *kc = NULL;		/* libkstat cookie */
static kstat_t *rpc_clts_client_kstat, *rpc_clts_server_kstat;
static kstat_t *rpc_cots_client_kstat, *rpc_cots_server_kstat;
static kstat_t *nfs_client_kstat, *nfs_server_kstat;
static kstat_t *rfsproccnt_v2_kstat, *rfsproccnt_v3_kstat;
static kstat_t *rfsreqcnt_v2_kstat, *rfsreqcnt_v3_kstat;
static kstat_t *aclproccnt_v2_kstat, *aclproccnt_v3_kstat;
static kstat_t *aclreqcnt_v2_kstat, *aclreqcnt_v3_kstat;

static void kio(int, int, char *, int);
static void getstats(void);
static void putstats(void);
static void setup(int);
static void cr_print(int);
static void sr_print(int);
static void cn_print(int);
static void sn_print(int);
static void ca_print(int);
static void sa_print(int);
static void req_print(kstat_t *, ulong_t);
static void stat_print(kstat_t *);

static void fail(int, char *, ...);
static kid_t safe_kstat_read(kstat_ctl_t *, kstat_t *, void *);
static kid_t safe_kstat_write(kstat_ctl_t *, kstat_t *, void *);

static void usage(void);
static void mi_print(void);
static int ignore(char *);
static int get_fsid(char *);

#define	MAX_COLUMNS	80

static int field_width = 0;
static int ncolumns;

static void req_width(kstat_t *);
static void stat_width(kstat_t *);

main(int argc, char *argv[])
{
	int c;
	int cflag = 0;		/* client stats */
	int sflag = 0;		/* server stats */
	int nflag = 0;		/* nfs stats */
	int rflag = 0;		/* rpc stats */
	int zflag = 0;		/* zero stats after printing */
	int mflag = 0;		/* mount table stats */
	int aflag = 0;		/* print acl statistics */

	while ((c = getopt(argc, argv, "cnrsmza")) != EOF) {
		switch (c) {
		case 'c':
			cflag++;
			break;
		case 'n':
			nflag++;
			break;
		case 'r':
			rflag++;
			break;
		case 's':
			sflag++;
			break;
		case 'm':
			mflag++;
			break;
		case 'z':
			if (geteuid())
				fail(0, "Must be root for z flag");
			zflag++;
			break;
		case 'a':
			aflag++;
			break;
		case '?':
		default:
			usage();
		}
	}

	if (argc - optind >= 1)
		usage();

	setup(zflag);

	if (mflag) {
		mi_print();
		exit(0);
	}

	getstats();

	ncolumns = (MAX_COLUMNS - 1) / field_width;

	if (sflag &&
	    (rpc_clts_server_kstat == NULL || nfs_server_kstat == NULL)) {
		fprintf(stderr,
			"nfsstat: kernel is not configured with "
			"the server nfs and rpc code.\n");
	}
	if (sflag || (!sflag && !cflag)) {
		if (rflag || (!rflag && !nflag && !aflag))
			sr_print(zflag);
		if (nflag || (!rflag && !nflag && !aflag))
			sn_print(zflag);
		if (aflag || (!rflag && !nflag && !aflag))
			sa_print(zflag);
	}
	if (cflag &&
	    (rpc_clts_client_kstat == NULL || nfs_client_kstat == NULL)) {
		fprintf(stderr,
			"nfsstat: kernel is not configured with "
			"the client nfs and rpc code.\n");
	}
	if (cflag || (!sflag && !cflag)) {
		if (rflag || (!rflag && !nflag && !aflag))
			cr_print(zflag);
		if (nflag || (!rflag && !nflag && !aflag))
			cn_print(zflag);
		if (aflag || (!rflag && !nflag && !aflag))
			ca_print(zflag);
	}

	if (zflag)
		putstats();

	return (0);
	/* NOTREACHED */
}

static void
kio(int rdwr, int id, char *buf, int len)
{

	if (nl[id].n_type == 0) {
		fprintf(stderr, "nfsstat: '%s' not in namelist\n",
			nl[id].n_name);
		memset(buf, 0, len);
		return;
	}
	if (rdwr == 0) {
		if (kvm_read(kd, nl[id].n_value, buf, len) != len)
			fail(1, "kernel read error");
	} else {
		if (kvm_write(kd, nl[id].n_value, buf, len) != len)
			fail(1, "kernel write error");
	}
}

#define	kread(id, buf, len)  kio(0, id, (char *)(buf), len)
#define	kwrite(id, buf, len) kio(1, id, (char *)(buf), len)

static void
getstats(void)
{

	if (rpc_clts_client_kstat != NULL) {
		safe_kstat_read(kc, rpc_clts_client_kstat, NULL);
		stat_width(rpc_clts_client_kstat);
	}
	if (rpc_cots_client_kstat != NULL) {
		safe_kstat_read(kc, rpc_cots_client_kstat, NULL);
		stat_width(rpc_cots_client_kstat);
	}
	if (rpc_clts_server_kstat != NULL) {
		safe_kstat_read(kc, rpc_clts_server_kstat, NULL);
		stat_width(rpc_clts_server_kstat);
	}
	if (rpc_cots_server_kstat != NULL) {
		safe_kstat_read(kc, rpc_cots_server_kstat, NULL);
		stat_width(rpc_cots_server_kstat);
	}
	if (nfs_client_kstat != NULL) {
		safe_kstat_read(kc, nfs_client_kstat, NULL);
		stat_width(nfs_client_kstat);
	}
	if (nfs_server_kstat != NULL) {
		safe_kstat_read(kc, nfs_server_kstat, NULL);
		stat_width(nfs_server_kstat);
	}
	if (rfsproccnt_v2_kstat != NULL) {
		safe_kstat_read(kc, rfsproccnt_v2_kstat, NULL);
		req_width(rfsproccnt_v2_kstat);
	}
	if (rfsproccnt_v3_kstat != NULL) {
		safe_kstat_read(kc, rfsproccnt_v3_kstat, NULL);
		req_width(rfsproccnt_v3_kstat);
	}
	if (rfsreqcnt_v2_kstat != NULL) {
		safe_kstat_read(kc, rfsreqcnt_v2_kstat, NULL);
		req_width(rfsreqcnt_v2_kstat);
	}
	if (rfsreqcnt_v3_kstat != NULL) {
		safe_kstat_read(kc, rfsreqcnt_v3_kstat, NULL);
		req_width(rfsreqcnt_v3_kstat);
	}
	if (aclproccnt_v2_kstat != NULL) {
		safe_kstat_read(kc, aclproccnt_v2_kstat, NULL);
		req_width(aclproccnt_v2_kstat);
	}
	if (aclproccnt_v3_kstat != NULL) {
		safe_kstat_read(kc, aclproccnt_v3_kstat, NULL);
		req_width(aclproccnt_v3_kstat);
	}
	if (aclreqcnt_v2_kstat != NULL) {
		safe_kstat_read(kc, aclreqcnt_v2_kstat, NULL);
		req_width(aclreqcnt_v2_kstat);
	}
	if (aclreqcnt_v3_kstat != NULL) {
		safe_kstat_read(kc, aclreqcnt_v3_kstat, NULL);
		req_width(aclreqcnt_v3_kstat);
	}
}

static void
putstats(void)
{

	if (rpc_clts_client_kstat != NULL)
		safe_kstat_write(kc, rpc_clts_client_kstat, NULL);
	if (rpc_cots_client_kstat != NULL)
		safe_kstat_write(kc, rpc_cots_client_kstat, NULL);
	if (nfs_client_kstat != NULL)
		safe_kstat_write(kc, nfs_client_kstat, NULL);
	if (rpc_clts_server_kstat != NULL)
		safe_kstat_write(kc, rpc_clts_server_kstat, NULL);
	if (rpc_cots_server_kstat != NULL)
		safe_kstat_write(kc, rpc_cots_server_kstat, NULL);
	if (nfs_server_kstat != NULL)
		safe_kstat_write(kc, nfs_server_kstat, NULL);
	if (rfsproccnt_v2_kstat != NULL)
		safe_kstat_write(kc, rfsproccnt_v2_kstat, NULL);
	if (rfsproccnt_v3_kstat != NULL)
		safe_kstat_write(kc, rfsproccnt_v3_kstat, NULL);
	if (rfsreqcnt_v2_kstat != NULL)
		safe_kstat_write(kc, rfsreqcnt_v2_kstat, NULL);
	if (rfsreqcnt_v3_kstat != NULL)
		safe_kstat_write(kc, rfsreqcnt_v3_kstat, NULL);
	if (aclproccnt_v2_kstat != NULL)
		safe_kstat_write(kc, aclproccnt_v2_kstat, NULL);
	if (aclproccnt_v3_kstat != NULL)
		safe_kstat_write(kc, aclproccnt_v3_kstat, NULL);
	if (aclreqcnt_v2_kstat != NULL)
		safe_kstat_write(kc, aclreqcnt_v2_kstat, NULL);
	if (aclreqcnt_v3_kstat != NULL)
		safe_kstat_write(kc, aclreqcnt_v3_kstat, NULL);
}

static void
setup(int zflag)
{

	kd = kvm_open(NULL, NULL, NULL, zflag ? O_RDWR : O_RDONLY, "nfsstat");
	if (kd == NULL)
		exit(1);

	if (kvm_nlist(kd, nl) < 0)
		fail(0, "bad namelist");

	if ((kc = kstat_open()) == NULL)
		fail(1, "kstat_open(): can't open /dev/kstat");

	rpc_clts_client_kstat = kstat_lookup(kc, "unix", 0, "rpc_clts_client");
	rpc_clts_server_kstat = kstat_lookup(kc, "unix", 0, "rpc_clts_server");
	rpc_cots_client_kstat = kstat_lookup(kc, "unix", 0, "rpc_cots_client");
	rpc_cots_server_kstat = kstat_lookup(kc, "unix", 0, "rpc_cots_server");
	nfs_client_kstat = kstat_lookup(kc, "nfs", 0, "nfs_client");
	nfs_server_kstat = kstat_lookup(kc, "nfs", 0, "nfs_server");
	rfsproccnt_v2_kstat = kstat_lookup(kc, "nfs", 0, "rfsproccnt_v2");
	rfsproccnt_v3_kstat = kstat_lookup(kc, "nfs", 0, "rfsproccnt_v3");
	rfsreqcnt_v2_kstat = kstat_lookup(kc, "nfs", 0, "rfsreqcnt_v2");
	rfsreqcnt_v3_kstat = kstat_lookup(kc, "nfs", 0, "rfsreqcnt_v3");
	aclproccnt_v2_kstat = kstat_lookup(kc, "nfs_acl", 0, "aclproccnt_v2");
	aclproccnt_v3_kstat = kstat_lookup(kc, "nfs_acl", 0, "aclproccnt_v3");
	aclreqcnt_v2_kstat = kstat_lookup(kc, "nfs_acl", 0, "aclreqcnt_v2");
	aclreqcnt_v3_kstat = kstat_lookup(kc, "nfs_acl", 0, "aclreqcnt_v3");
}

static void
req_width(kstat_t *req)
{
	int i, nreq, per, len;
	char fixlen[128];
	kstat_named_t *knp;
	ulong_t tot;

	tot = 0;
	knp = KSTAT_NAMED_PTR(req);
	for (i = 0; i < req->ks_ndata; i++)
		tot += knp[i].value.ul;

	knp = kstat_data_lookup(req, "null");
	nreq = req->ks_ndata - (knp - KSTAT_NAMED_PTR(req));

	for (i = 0; i < nreq; i++) {
		len = strlen(knp[i].name) + 1;
		if (field_width < len)
			field_width = len;
		if (tot)
			per = (int)(knp[i].value.ul * 100LL / tot);
		else
			per = 0;
		sprintf(fixlen, "%lu %d%%", knp[i].value.ul, per);
		len = strlen(fixlen) + 1;
		if (field_width < len)
			field_width = len;
	}
}

static void
stat_width(kstat_t *req)
{
	int i, nreq, len;
	char fixlen[128];
	kstat_named_t *knp;

	knp = KSTAT_NAMED_PTR(req);
	nreq = req->ks_ndata;

	for (i = 0; i < nreq; i++) {
		len = strlen(knp[i].name) + 1;
		if (field_width < len)
			field_width = len;
		sprintf(fixlen, "%lu", knp[i].value.ul);
		len = strlen(fixlen) + 1;
		if (field_width < len)
			field_width = len;
	}
}

static void
cr_print(int zflag)
{
	int i;
	kstat_named_t *kptr;

	printf("\nClient rpc:\n");

	if (rpc_cots_client_kstat != NULL) {
		printf("Connection oriented:\n");
		stat_print(rpc_cots_client_kstat);
		if (zflag) {
			kptr = KSTAT_NAMED_PTR(rpc_cots_client_kstat);
			for (i = 0; i < rpc_cots_client_kstat->ks_ndata; i++)
				kptr[i].value.ul = 0;
		}
	}
	if (rpc_clts_client_kstat != NULL) {
		printf("Connectionless:\n");
		stat_print(rpc_clts_client_kstat);
		if (zflag) {
			kptr = KSTAT_NAMED_PTR(rpc_clts_client_kstat);
			for (i = 0; i < rpc_clts_client_kstat->ks_ndata; i++)
				kptr[i].value.ul = 0;
		}
	}
}

static void
sr_print(int zflag)
{
	int i;
	kstat_named_t *kptr;

	printf("\nServer rpc:\n");

	if (rpc_cots_server_kstat != NULL) {
		printf("Connection oriented:\n");
		stat_print(rpc_cots_server_kstat);
		if (zflag) {
			kptr = KSTAT_NAMED_PTR(rpc_cots_server_kstat);
			for (i = 0; i < rpc_cots_server_kstat->ks_ndata; i++)
				kptr[i].value.ul = 0;
		}
	}
	if (rpc_clts_server_kstat != NULL) {
		printf("Connectionless:\n");
		stat_print(rpc_clts_server_kstat);
		if (zflag) {
			kptr = KSTAT_NAMED_PTR(rpc_clts_server_kstat);
			for (i = 0; i < rpc_clts_server_kstat->ks_ndata; i++)
				kptr[i].value.ul = 0;
		}
	}
}

static void
cn_print(int zflag)
{
	int i;
	ulong_t tot;
	kstat_named_t *kptr;

	if (nfs_client_kstat == NULL)
		return;

	printf("\nClient nfs:\n");

	stat_print(nfs_client_kstat);
	if (zflag) {
		kptr = KSTAT_NAMED_PTR(nfs_client_kstat);
		for (i = 0; i < nfs_client_kstat->ks_ndata; i++)
			kptr[i].value.ul = 0;
	}

	if (rfsreqcnt_v2_kstat != NULL) {
		tot = 0;
		kptr = KSTAT_NAMED_PTR(rfsreqcnt_v2_kstat);
		for (i = 0; i < rfsreqcnt_v2_kstat->ks_ndata; i++)
			tot += kptr[i].value.ul;
		printf("Version 2: (%lu calls)\n", tot);
		req_print(rfsreqcnt_v2_kstat, tot);
		if (zflag) {
			for (i = 0; i < rfsreqcnt_v2_kstat->ks_ndata; i++)
				kptr[i].value.ul = 0;
		}
	}

	if (rfsreqcnt_v3_kstat) {
		tot = 0;
		kptr = KSTAT_NAMED_PTR(rfsreqcnt_v3_kstat);
		for (i = 0; i < rfsreqcnt_v3_kstat->ks_ndata; i++)
			tot += kptr[i].value.ul;
		printf("Version 3: (%lu calls)\n", tot);
		req_print(rfsreqcnt_v3_kstat, tot);
		if (zflag) {
			for (i = 0; i < rfsreqcnt_v3_kstat->ks_ndata; i++)
				kptr[i].value.ul = 0;
		}
	}
}

static void
sn_print(int zflag)
{
	int i;
	ulong_t tot;
	kstat_named_t *kptr;

	if (nfs_server_kstat == NULL)
		return;

	printf("\nServer nfs:\n");

	stat_print(nfs_server_kstat);
	if (zflag) {
		kptr = KSTAT_NAMED_PTR(nfs_server_kstat);
		for (i = 0; i < nfs_server_kstat->ks_ndata; i++)
			kptr[i].value.ul = 0;
	}

	if (rfsproccnt_v2_kstat != NULL) {
		tot = 0;
		kptr = KSTAT_NAMED_PTR(rfsproccnt_v2_kstat);
		for (i = 0; i < rfsproccnt_v2_kstat->ks_ndata; i++)
			tot += kptr[i].value.ul;
		printf("Version 2: (%lu calls)\n", tot);
		req_print(rfsproccnt_v2_kstat, tot);
		if (zflag) {
			for (i = 0; i < rfsproccnt_v2_kstat->ks_ndata; i++)
				kptr[i].value.ul = 0;
		}
	}

	if (rfsproccnt_v3_kstat) {
		tot = 0;
		kptr = KSTAT_NAMED_PTR(rfsproccnt_v3_kstat);
		for (i = 0; i < rfsproccnt_v3_kstat->ks_ndata; i++)
			tot += kptr[i].value.ul;
		printf("Version 3: (%lu calls)\n", tot);
		req_print(rfsproccnt_v3_kstat, tot);
		if (zflag) {
			for (i = 0; i < rfsproccnt_v3_kstat->ks_ndata; i++)
				kptr[i].value.ul = 0;
		}
	}
}

static void
ca_print(int zflag)
{
	int i;
	ulong_t tot;
	kstat_named_t *kptr;

	if (nfs_client_kstat == NULL)
		return;

	printf("\nClient nfs_acl:\n");

	if (aclreqcnt_v2_kstat != NULL) {
		tot = 0;
		kptr = KSTAT_NAMED_PTR(aclreqcnt_v2_kstat);
		for (i = 0; i < aclreqcnt_v2_kstat->ks_ndata; i++)
			tot += kptr[i].value.ul;
		printf("Version 2: (%lu calls)\n", tot);
		req_print(aclreqcnt_v2_kstat, tot);
		if (zflag) {
			for (i = 0; i < aclreqcnt_v2_kstat->ks_ndata; i++)
				kptr[i].value.ul = 0;
		}
	}

	if (aclreqcnt_v3_kstat) {
		tot = 0;
		kptr = KSTAT_NAMED_PTR(aclreqcnt_v3_kstat);
		for (i = 0; i < aclreqcnt_v3_kstat->ks_ndata; i++)
			tot += kptr[i].value.ul;
		printf("Version 3: (%lu calls)\n", tot);
		req_print(aclreqcnt_v3_kstat, tot);
		if (zflag) {
			for (i = 0; i < aclreqcnt_v3_kstat->ks_ndata; i++)
				kptr[i].value.ul = 0;
		}
	}
}

static void
sa_print(int zflag)
{
	int i;
	ulong_t tot;
	kstat_named_t *kptr;

	if (nfs_server_kstat == NULL)
		return;

	printf("\nServer nfs_acl:\n");

	if (aclproccnt_v2_kstat != NULL) {
		tot = 0;
		kptr = KSTAT_NAMED_PTR(aclproccnt_v2_kstat);
		for (i = 0; i < aclproccnt_v2_kstat->ks_ndata; i++)
			tot += kptr[i].value.ul;
		printf("Version 2: (%lu calls)\n", tot);
		req_print(aclproccnt_v2_kstat, tot);
		if (zflag) {
			for (i = 0; i < aclproccnt_v2_kstat->ks_ndata; i++)
				kptr[i].value.ul = 0;
		}
	}

	if (aclproccnt_v3_kstat) {
		tot = 0;
		kptr = KSTAT_NAMED_PTR(aclproccnt_v3_kstat);
		for (i = 0; i < aclproccnt_v3_kstat->ks_ndata; i++)
			tot += kptr[i].value.ul;
		printf("Version 3: (%lu calls)\n", tot);
		req_print(aclproccnt_v3_kstat, tot);
		if (zflag) {
			for (i = 0; i < aclproccnt_v3_kstat->ks_ndata; i++)
				kptr[i].value.ul = 0;
		}
	}
}

#define	MIN(a, b)	((a) < (b) ? (a) : (b))

static void
req_print(kstat_t *req, ulong_t tot)
{
	int i, j, nreq, per;
	char fixlen[128];
	kstat_named_t *knp;

	knp = kstat_data_lookup(req, "null");
	nreq = req->ks_ndata - (knp - KSTAT_NAMED_PTR(req));

	for (i = 0; i < nreq; i += ncolumns) {
		for (j = i; j < MIN(i + ncolumns, nreq); j++) {
			printf("%-*s", field_width, knp[j].name);
		}
		printf("\n");
		for (j = i; j < MIN(i + ncolumns, nreq); j++) {
			if (tot)
				per = (int)(knp[j].value.ul * 100LL / tot);
			else
				per = 0;
			sprintf(fixlen, "%lu %d%% ", knp[j].value.ul, per);
			printf("%-*s", field_width, fixlen);
		}
		printf("\n");
	}
}

static void
stat_print(kstat_t *req)
{
	int i, j, nreq;
	char fixlen[128];
	kstat_named_t *knp;

	knp = KSTAT_NAMED_PTR(req);
	nreq = req->ks_ndata;

	for (i = 0; i < nreq; i += ncolumns) {
		for (j = i; j < MIN(i + ncolumns, nreq); j++) {
			printf("%-*s", field_width, knp[j].name);
		}
		printf("\n");
		for (j = i; j < MIN(i + ncolumns, nreq); j++) {
			sprintf(fixlen, "%lu ", knp[j].value.ul);
			printf("%-*s", field_width, fixlen);
		}
		printf("\n");
	}
}

/*
 * Print the mount table info
 */
static struct vfs vfsrec;
static struct mntinfo mi;
static struct servinfo csi;


/*
* my_dir and my_path could be pointers
*/
struct myrec {
	u_long my_fsid;
	char my_dir[MAXPATHLEN];
	char *my_path;
	char *ig_path;
	struct myrec *next;
};


static void
mi_print(void)
{
	struct vfs *vfs;	/* "current" VFS pointer */
	struct vfsops *nfs_vfsops;
	struct vfsops *nfs3_vfsops;
	FILE *mt;
	struct mnttab m;
	struct myrec *list, *mrp, *pmrp;
	struct knetconfig knc;
	char proto[KNC_STRSIZE];
	int psize;
	char *flavor;
	int ignored = 0;
	char hostname[HOSTNAMESZ];
	sec_data_t secdata;
	seconfig_t nfs_sec;

	mt = fopen(MNTTAB, "r");
	if (mt == NULL) {
		perror(MNTTAB);
		exit(0);
	}

	list = NULL;
	while (getmntent(mt, &m) == 0) {
		/* ignore non "nfs" and save the "ignore" entries */
		if (strcmp(m.mnt_fstype, MNTTYPE_NFS) != 0)
			continue;

		if ((mrp = (struct myrec *) malloc(sizeof (struct myrec)))
			== 0) {
			fprintf(stderr, "nfsstat: not enough memory\n");
			exit(1);
		}
		mrp->my_fsid = get_fsid(m.mnt_mntopts);
		if (ignore(m.mnt_mntopts)) {
			/*
			* ignored entries cannot be ignored for this
			* option. We have to display the info for this
			* nfs mount. The ignore is an indication
			* that the actual mount point is different and
			* something is in between the nfs mount.
			* So save the mount point now
			*/
			if ((mrp->ig_path = (char *)malloc(
					strlen(m.mnt_mountp) + 1)) == 0) {
				fprintf(stderr, "nfsstat: not enough memory\n");
				exit(1);
			}
			(void) strcpy(mrp->ig_path, m.mnt_mountp);
			ignored++;
		} else {
			mrp->ig_path = 0;
			(void) strcpy(mrp->my_dir, m.mnt_mountp);
		}
		if ((mrp->my_path = strdup(m.mnt_special)) == NULL) {
			fprintf(stderr, "nfsstat: not enough memory\n"); 
			exit(1);
		}
		mrp->next = list;
		list = mrp;
	}

	/*
	* If something got ignored, go to the beginning of the mnttab
	* and look for the cachefs entries since they are the one
	* causing this. The mount point saved for the ignored entries
	* is matched against the special to get the actual mount point.
	* We are interested in the acutal mount point so that the output
	* look nice too.
	*/
	if (ignored) {
		rewind(mt);
		while (getmntent(mt, &m) == 0) {

			/* ignore non "cachefs" */
			if (strcmp(m.mnt_fstype, MNTTYPE_CACHEFS) != 0)
				continue;

			for (mrp = list; mrp; mrp = mrp->next) {
				if (mrp->ig_path == 0)
					continue;
				if (strcmp(mrp->ig_path, m.mnt_special) == 0) {
					mrp->ig_path = 0;
					(void) strcpy(mrp->my_dir,
							m.mnt_mountp);
				}
			}
		}
		/*
		* Now ignored entries which do not have
		* the my_dir initialized are really ignored; This never
		* happens unless the mnttab is corrupted.
		*/
		for (pmrp = 0, mrp = list; mrp; mrp = mrp->next) {
			if (mrp->ig_path == 0)
				pmrp = mrp;
			else if (pmrp)
				pmrp->next = mrp->next;
			else
				list = mrp->next;
		}
	}

	(void) fclose(mt);


	nfs_vfsops = (struct vfsops *)nl[X_NFS_VFSOPS].n_value;
	nfs3_vfsops = (struct vfsops *)nl[X_NFS3_VFSOPS].n_value;

	kread(X_ROOTVFS, &vfs, sizeof (vfs));

	for (; vfs != NULL; vfs = vfsrec.vfs_next) {
		if (kvm_read(kd, (u_long)vfs, (char *)&vfsrec,
		    sizeof (vfsrec)) != sizeof (vfsrec))
			fail(1, "kernel read error");
		if (vfsrec.vfs_data == NULL)
			continue;
		if (vfsrec.vfs_op != nfs_vfsops &&
		    vfsrec.vfs_op != nfs3_vfsops) {
			continue;
		}
		if (kvm_read(kd, (u_long)vfsrec.vfs_data, (char *)&mi,
		    sizeof (mi)) != sizeof (mi))
			fail(1, "kernel read error");
		for (mrp = list; mrp; mrp = mrp->next) {
			if (mrp->my_fsid == vfsrec.vfs_fsid.val[0])
				break;
		}
		if (mrp == 0)
			continue;

		/* 
		 * read the current server information
		 */
		if (kvm_read(kd, (u_long)mi.mi_curr_serv, (char *)&csi,
		    sizeof (csi)) != sizeof (csi)) {
			fprintf(stderr, "nfsstat: kernel read error\n");
			exit(1);
		}
		mi.mi_curr_serv = &csi;

		/*
		 * Now that we've found the file system,
		 * read the netconfig information.
		 */
		if (kvm_read(kd, (u_long)mi.mi_knetconfig, (char *)&knc,
		    sizeof (knc)) != sizeof (knc))
			fail(1, "kernel read error");

		psize = kvm_read(kd, (u_long)knc.knc_proto, proto, KNC_STRSIZE);
		if (psize != KNC_STRSIZE) {
			/*
			 * On diskless systems, the proto in
			 * root's knentconfig could be a
			 * statically allocated string which
			 * will undoubtably be shorter than
			 * KNC_STRSIZE.
			 */
			psize = 0;
			while (kvm_read(kd, (u_long)&knc.knc_proto[psize],
			    &proto[psize], 1) == 1) {
				if (proto[psize++] == '\0')
					break;
				if (psize >= KNC_STRSIZE)
					break;
			}
			if (psize == 0)
				fail(1, "kernel read error");
		}

		/* 
		 * read the current server hostname
		 */
		if (kvm_read(kd, (u_long)mi.mi_hostname, (char *)&hostname,
		    sizeof (hostname)) != sizeof (hostname)) {
			fprintf(stderr, "nfsstat: kernel read error\n");
			exit(1);
		}
				
		printf("%s from %s\n", mrp->my_dir, mrp->my_path);

		printf(" Flags:   vers=%lu,proto=%s", mi.mi_vers, proto);

		/*
		 *  Now read the mi_secdata from the kernel and
		 *  get the security flavor name.
		 */
		if (kvm_read(kd, (u_long)mi.mi_secdata, (char *)&secdata,
				sizeof (secdata)) != sizeof (secdata)) {
			fprintf(stderr, "nfsstat: kernel read error\n");
			exit(1);
		}
		/*
		 *  get the secmode name from /etc/nfssec.conf.
		 */
		if (!nfs_getseconfig_bynumber(secdata.secmod, &nfs_sec)) {
			flavor = nfs_sec.sc_name;
		} else
			flavor = NULL;

		if (flavor != NULL)
			printf(",sec=%s", flavor);
		else
			printf(",sec#=%d", secdata.secmod);

		printf(",%s", (mi.mi_flags & MI_HARD) ? "hard" : "soft");
		if (mi.mi_flags & MI_PRINTED)
			printf(",printed");
		printf(",%s", (mi.mi_flags & MI_INT) ? "intr" : "nointr");
		if (mi.mi_flags & MI_DOWN)
			printf(",down");
		if (mi.mi_flags & MI_NOAC)
			printf(",noac");
		if (mi.mi_flags & MI_NOCTO)
			printf(",nocto");
		if (mi.mi_flags & MI_DYNAMIC)
			printf(",dynamic");
		if (mi.mi_flags & MI_LLOCK)
			printf(",llock");
		if (mi.mi_flags & MI_GRPID)
			printf(",grpid");
		if (mi.mi_flags & MI_RPCTIMESYNC)
			printf(",rpctimesync");
		if (mi.mi_flags & MI_LINK)
			printf(",link");
		if (mi.mi_flags & MI_SYMLINK)
			printf(",symlink");
		if (mi.mi_flags & MI_READDIR)
			printf(",readdir");
		if (mi.mi_flags & MI_ACL)
			printf(",acl");
		printf(",rsize=%ld,wsize=%ld", mi.mi_curread, mi.mi_curwrite);
		printf(",retrans=%d", mi.mi_retrans);
		printf("\n");

#define	srtt_to_ms(x) x, (x * 2 + x / 2)
#define	dev_to_ms(x) x, (x * 5)

		if (mi.mi_timers[0].rt_srtt || mi.mi_timers[0].rt_rtxcur) {
			printf(
		" Lookups: srtt=%d (%dms), dev=%d (%dms), cur=%lu (%lums)\n",
				srtt_to_ms(mi.mi_timers[0].rt_srtt),
				dev_to_ms(mi.mi_timers[0].rt_deviate),
				mi.mi_timers[0].rt_rtxcur,
				mi.mi_timers[0].rt_rtxcur * 20);
		}
		if (mi.mi_timers[1].rt_srtt || mi.mi_timers[1].rt_rtxcur) {
			printf(
		" Reads:   srtt=%d (%dms), dev=%d (%dms), cur=%lu (%lums)\n",
				srtt_to_ms(mi.mi_timers[1].rt_srtt),
				dev_to_ms(mi.mi_timers[1].rt_deviate),
				mi.mi_timers[1].rt_rtxcur,
				mi.mi_timers[1].rt_rtxcur * 20);
		}
		if (mi.mi_timers[2].rt_srtt || mi.mi_timers[2].rt_rtxcur) {
			printf(
		" Writes:  srtt=%d (%dms), dev=%d (%dms), cur=%lu (%lums)\n",
				srtt_to_ms(mi.mi_timers[2].rt_srtt),
				dev_to_ms(mi.mi_timers[2].rt_deviate),
				mi.mi_timers[2].rt_rtxcur,
				mi.mi_timers[2].rt_rtxcur * 20);
		}
		if (mi.mi_timers[2].rt_srtt || mi.mi_timers[2].rt_rtxcur) {
			printf(
		" All:     srtt=%d (%dms), dev=%d (%dms), cur=%lu (%lums)\n",
			srtt_to_ms(mi.mi_timers[3].rt_srtt),
			dev_to_ms(mi.mi_timers[3].rt_deviate),
			mi.mi_timers[3].rt_rtxcur,
			mi.mi_timers[3].rt_rtxcur * 20);
		}
		if (strchr(mrp->my_path, ',')) 
			printf(
			" Failover:noresponse=%ld, failover=%ld, remap=%ld, currserver=%s\n",
				mi.mi_noresponse, mi.mi_failover, 
				mi.mi_remap, hostname);
		printf("\n");
	}

}

static char *mntopts[] = { MNTOPT_IGNORE, MNTOPT_DEV, NULL };
#define	IGNORE  0
#define	DEV	1

/*
 * Return 1 if "ignore" appears in the options string
 */
static int
ignore(char *opts)
{
	char *value;
	char *s = strdup(opts);

	if (s == NULL)
		return (0);
	opts = s;

	while (*opts != '\0') {
		if (getsubopt(&opts, mntopts, &value) == IGNORE) {
			free(s);
			return (1);
		}
	}

	free(s);
	return (0);
}

/*
 * Get the fsid from the "dev=" option
 * actually, the device number.
 */
static int
get_fsid(char *opts)
{
	char *devid;
	char *s = strdup(opts);
	int dev;

	if (s == NULL)
		return (0);
	opts = s;

	while (*opts != '\0') {
		if (getsubopt(&opts, mntopts, &devid) == DEV)
			goto found;
	}

	free(s);
	return (0);

found:
	dev = strtol(devid, (char **) NULL, 16);
	free(s);
	return (dev);
}

void
usage(void)
{

	fprintf(stderr, "Usage: nfsstat [-cnrsmza]\n");
	exit(1);
}

static void
fail(int do_perror, char *message, ...)
{
	va_list args;

	va_start(args, message);
	fprintf(stderr, "nfsstat: ");
	vfprintf(stderr, message, args);
	va_end(args);
	if (do_perror)
		fprintf(stderr, ": %s", strerror(errno));
	fprintf(stderr, "\n");
	exit(1);
}

kid_t
safe_kstat_read(kstat_ctl_t *kc, kstat_t *ksp, void *data)
{
	kid_t kstat_chain_id = kstat_read(kc, ksp, data);

	if (kstat_chain_id == -1)
		fail(1, "kstat_read(%x, '%s') failed", kc, ksp->ks_name);
	return (kstat_chain_id);
}

kid_t
safe_kstat_write(kstat_ctl_t *kc, kstat_t *ksp, void *data)
{
	kid_t kstat_chain_id = kstat_write(kc, ksp, data);

	if (kstat_chain_id == -1)
		fail(1, "kstat_write(%x, '%s') failed", kc, ksp->ks_name);
	return (kstat_chain_id);
}
