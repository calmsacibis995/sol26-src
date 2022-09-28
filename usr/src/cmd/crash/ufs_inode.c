#ident	"@(#)ufs_inode.c	1.27	96/09/11 SMI"
			/* SVr4.0 1.2.3.1 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *	Copyright (c) 1991, 1996 by Sun Microsystems, Inc
 *	All rights reserved.
 *
 *		Notice of copyright on this source code
 *		product does not indicate publication.
 *
 *		RESTRICTED RIGHTS LEGEND:
 *   Use, duplication, or disclosure by the Government is subject
 *   to restrictions as set forth in subparagraph (c)(1)(ii) of
 *   the Rights in Technical Data and Computer Software clause at
 *   DFARS 52.227-7013 and in similar clauses in the FAR and NASA
 *   FAR Supplement.
 */


#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/mntent.h>
#include <stdio.h>
#include <nlist.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/var.h>
#include <sys/vfs.h>
#include <sys/fstyp.h>
#include <sys/fsid.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/elf.h>
#define	_KERNEL
#include <sys/fs/ufs_inode.h>
#undef	_KERNEL
#include <sys/cred.h>
#include <sys/stream.h>
#include "crash.h"

/*
 * Due to namespace conflict with kernel headers we must define this here
 */
extern void *malloc(size_t);

static Elf32_Sym	*Ngrps;
static long		u_ninode;		/* size of UFS inode table */

/*
 * structure for inode-lookup performance
 */
struct icursor {
	long slot;				/* inode "slot" number	*/
	struct inode *nextip;			/* next ip read addr	*/
	union ihead *kern_ihead, *kern_itail;	/* hash-chain head/tail	*/
	union ihead ihbuf;			/* buf for hash chain	*/
	union ihead *ih;			/* hash chain ptr	*/
};

struct fsnames {
	char	*name;
	char	*vnsym;
	long	vnaddr;
};

static struct fsnames fsnames[] = {
	{"AUTO", "auto_vnodeops",	0},
	{"CACH", "cachefs_vnodeops",	0},
	{"DOOR", "door_vnodeops",	0},
	{"FD  ", "fdvnodeops",		0},
	{"FIFO", "fifo_vnodeops",	0},
	{"HSFS", "hsfs_vnodeops",	0},
	{"LOFS", "lo_vnodeops",		0},
	{"NAME", "nm_vnodeops",		0},
	{"NFS2", "nfs_vnodeops",	0},
	{"NFS3", "nfs3_vnodeops",	0},
	{"PCFS", "pcfs_dvnodeops",	0},
	{"PCFS", "pcfs_fvnodeops",	0},
	{"PROC", "prvnodeops",		0},
	{"S5  ", "s5_vnodeops",		0},
	{"SOCK", "sock_vnodeops",	0},
	{"SPEC", "spec_vnodeops",	0},
	{"SWAP", "swap_vnodeops",	0},
	{"TMP ", "tmp_vnodeops",	0},
	{"UFS ", "ufs_vnodeops",	0},
	{" ?? ",  0,			0}
};

static void print_ufs_inode();
static void list_ufs_inode();

extern int prrwlock(struct _krwlock *);
extern int prmutex(kmutex_t *);
extern void prvnode(struct vnode *, int);
extern int prcondvar(struct _kcondvar *, char *);

static void prfile(int, int, void *);
static void kmfile(void *kaddr, void *buf);


/*
 * get_ufsinfo: gets a copy of kernel data needed by
 * "ui" command for ufs-inode lookups.
 */
static void
get_ufsinfo()
{
	static int	got_info = 0;
	struct nlist	UFSNinode;

	if (got_info)
		return;

	if (nl_getsym("ufs_ninode", &UFSNinode))
		error("UFS inode table size not found in symbol table\n");
	readmem(UFSNinode.n_value, 1, -1, (char *)&u_ninode, sizeof (u_ninode),
		"size of UFS inode table");

	got_info++;
}

static int	in_full = 0;
static int	in_all = 0;
static int	in_lock = 0;

static char *in_heading =
/* CSTYLED */
"ADDR      MAJ/MIN     INUMB RCNT LINK  UID   GID          SIZE    MODE  FLAGS\n";

static void
kmprinode(void *kaddr, void *buf)
{
	struct inode    *ip = buf;

	if (in_all || (ip->i_flag & IREF))
		print_ufs_inode((long)kaddr, in_all, in_full, ip,
							in_heading, in_lock);
}

static void
kmprfreeinode(void *kaddr, void *buf)
{
	struct inode    *ip = buf;

	if (in_all || ((ip->i_flag & IREF) == 0))
		print_ufs_inode((long)kaddr, in_all, in_full, ip,
							in_heading, in_lock);
}

/*
 * Get arguments for UFS inode.
 */
int
get_ufs_inode()
{
	int	dump = 0;
	int	phys = 0;
	long	addr = -1;
	long	arg1 = -1;
	long	arg2 = -1;
	int	lfree = 0;
	int	c;
	struct inode    ip;

	in_full = 0;
	in_all = 0;
	in_lock = 0;

	get_ufsinfo();

	optind = 1;
	while ((c = getopt(argcnt, args, "defprlw:")) != EOF) {
		switch (c) {
		case 'e':	in_all = 1;
				break;

		case 'f':	in_full = 1;
				break;

		case 'l':	in_lock = 1;
				break;

		case 'd':	dump = 1;
				break;

		case 'p':	phys = 1;
				break;

		case 'r':	lfree = 1;
				break;

		case 'w':	redirect();
				break;

		default:	longjmp(syn, 0);
		}
	}

	if (dump)
		list_ufs_inode();
	else {
		(void) fprintf(fp, "UFS INODE MAX TABLE SIZE = %ld\n",
								u_ninode);
		if (!in_full && !in_lock) {
			(void) fprintf(fp, "%s", in_heading);
		}
		if (lfree) {
			kmem_cache_apply(kmem_cache_find("ufs_inode_cache"),
								kmprfreeinode);
		} else if (optind < argcnt && args[optind]) {
			in_all = 1;
			do {
				getargs(0, &arg1, &arg2, phys);
				if (arg1 == -1)
					continue;
				addr = arg1;
				readbuf(addr, 0, phys, -1,
						(char *)&ip,
						sizeof (struct inode),
						"inode");
				print_ufs_inode(addr, in_all, in_full,
						    &ip, in_heading, in_lock);
				arg1 = arg2 = -1;
			} while (args[++optind]);
		} else {
			kmem_cache_apply(kmem_cache_find("ufs_inode_cache"),
								kmprinode);
		}
	}
	return (0);
}

static void
kminode(void *kaddr, void *buf)
{
	struct inode    *ip = buf;

	if ((ip->i_vnode.v_count != 0) && (ip->i_flag & IREF))
		(void) fprintf(fp, "   0x%lx    %lu\n", (long)kaddr,
								ip->i_number);
}

static void
kmfreeinode(void *kaddr, void *buf)
{
	struct inode    *ip = buf;

	if ((ip->i_vnode.v_count != 0) && ((ip->i_flag & IREF) == 0))
		(void) fprintf(fp, "   0x%lx    %lu\n", (long)kaddr,
								ip->i_number);
}

static void
list_ufs_inode()
{
	(void) fprintf(fp, "The following UFS inodes are in use:\n");
	(void) fprintf(fp, "    ADDRESS      I-NUMBER\n");
	kmem_cache_apply(kmem_cache_find("ufs_inode_cache"), kminode);

	(void) fprintf(fp, "The following UFS inodes are in use but");
	(void) fprintf(fp, " have zero reference counts (idle):\n");
	(void) fprintf(fp, "    ADDRESS      I-NUMBER\n");
	kmem_cache_apply(kmem_cache_find("ufs_inode_cache"), kmfreeinode);
}

char *vnodeheading =
"VCNT VFSMNTED   VFSP    STREAMP VTYPE   RDEV   VDATA   VFILOCKS   VFLAG\n";

/*
 * Print UFS inode table.
 */

static void
print_ufs_inode(slot, all, full, ip, heading, lock)
	long	slot;
	int	all, full;
	struct inode *ip;
	char	*heading;
	int lock;
{
	char		ch;
	int		i;

	if (!ip->i_vnode.v_count && !all)
		return;
	if (full || lock)
		(void) fprintf(fp, "%s", heading);
	(void) fprintf(fp, "%8lx", slot);
	(void) fprintf(fp, " %4u,%4u %8lu %3lu %4d %5ld %5ld %13llu",
		getemajor(ip->i_dev),
		geteminor(ip->i_dev),
		ip->i_number,
		ip->i_vnode.v_count,
		ip->i_nlink,
		ip->i_uid,
		ip->i_gid,
		ip->i_size);
	switch (ip->i_vnode.v_type) {
		case VDIR: ch = 'd'; break;
		case VCHR: ch = 'c'; break;
		case VBLK: ch = 'b'; break;
		case VREG: ch = 'f'; break;
		case VLNK: ch = 'l'; break;
		case VSOCK: ch = 's'; break;
		case VFIFO: ch = 'p'; break;
		default:    ch = '-'; break;
	}
	(void) fprintf(fp, "  %c", ch);
	(void) fprintf(fp, "%s%s%s%03o",
		ip->i_mode & ISUID ? "u" : "-",
		ip->i_mode & ISGID ? "g" : "-",
		ip->i_mode & ISVTX ? "v" : "-",
		ip->i_mode & 0777);

	(void) fprintf(fp, "%s%s%s%s%s%s%s%s%s\n",
		ip->i_flag & IUPD ? " up" : "",
		ip->i_flag & IACC ? " ac" : "",
		ip->i_flag & IMOD ? " md" : "",
		ip->i_flag & ICHG ? " ch" : "",
		ip->i_flag & INOACC ? " na" : "",
		ip->i_flag & IMODTIME ? " mt" : "",
		ip->i_flag & IREF ? " rf" : "",
		ip->i_flag & ISYNC ? " sy" : "",
		ip->i_flag & IFASTSYMLNK ? " fl" : "");
	if (lock) {
		(void) fprintf(fp, "\ni-rwlock: ");
		prrwlock(&(ip->i_rwlock));
		(void) fprintf(fp, "i-contents: ");
		prrwlock(&(ip->i_contents));
		(void) fprintf(fp, "i_tlock: ");
		prmutex(&(ip->i_tlock));
		prcondvar(&ip->i_wrcv, "i_wrcv");
	}

	if (!full)
		return;

	(void) fprintf(fp, "\t   NEXTR\n");
	(void) fprintf(fp, "\t%8lx\n", ip->i_nextr);

	if ((ip->i_vnode.v_type == VDIR) || (ip->i_vnode.v_type == VREG) ||
		(ip->i_vnode.v_type == VLNK)) {
		for (i = 0; i < NDADDR; i++) {
			if (!(i & 3))
				(void) fprintf(fp, "\n\t");
			(void) fprintf(fp, "[%2d]: %-10lx", i, ip->i_db[i]);
		}
		(void) fprintf(fp, "\n");
		for (i = 0; i < NIADDR; i++) {
			if (!(i & 3))
				(void) fprintf(fp, "\n\t");
			(void) fprintf(fp, "[%2d]: %-10lx", i, ip->i_ib[i]);
		}
		(void) fprintf(fp, "\n");
	} else
		(void) fprintf(fp, "\n");

	/* print vnode info */
	(void) fprintf(fp, "\nVNODE :\n");
	(void) fprintf(fp, "%s", vnodeheading);
	prvnode(&ip->i_vnode, lock);
	(void) fprintf(fp, "\n");
}

/* get arguments for vnode function */
int
getvnode()
{
	long addr = -1;
	int phys = 0;
	int lock = 0;
	int c;
	struct vnode vnbuf;


	optind = 1;
	while ((c = getopt(argcnt, args, "lpw:")) != EOF) {
		switch (c) {
			case 'l' :	lock = 1;
					break;
			case 'p' :	phys = 1;
					break;
			case 'w' :	redirect();
					break;
			default  :	longjmp(syn, 0);
					break;
		}
	}
	if (args[optind]) {
		(void) fprintf(fp, "%s", vnodeheading);
		do {
			if ((addr = strcon(args[optind], 'h')) == -1)
				continue;
			readbuf(addr, 0, phys, -1, (char *)&vnbuf,
				sizeof (vnbuf), "vnode structure");
			prvnode(&vnbuf, lock);
		} while (args[++optind]);
	} else longjmp(syn, 0);

	return (0);
}

void
prvnode(vnptr, lock)
struct vnode *vnptr;
int lock;
{
	(void) fprintf(fp, "%3lu  %8lx %8lx %8lx",
		vnptr->v_count,
		(long)vnptr->v_vfsmountedhere,
		(long)vnptr->v_vfsp,
		(long)vnptr->v_stream);
	switch (vnptr->v_type) {
		case VREG :	(void) fprintf(fp, "   f       - "); break;
		case VDIR :	(void) fprintf(fp, "   d       - "); break;
		case VLNK :	(void) fprintf(fp, "   l       - "); break;
		case VCHR :
				(void) fprintf(fp, "   c %4u,%-3u",
					getemajor(vnptr->v_rdev),
					geteminor(vnptr->v_rdev));
				break;
		case VBLK :
				(void) fprintf(fp, "   b %4u,%-3u",
					getemajor(vnptr->v_rdev),
					geteminor(vnptr->v_rdev));
				break;
		case VSOCK :
				fprintf(fp, "      s  %4u,%-3u",
					getemajor(vnptr->v_rdev),
					geteminor(vnptr->v_rdev));
				break;
		case VFIFO :	(void) fprintf(fp, "   p       - "); break;
		case VNON :	(void) fprintf(fp, "   n       - "); break;
		default :	(void) fprintf(fp, "   -       - "); break;
	}
	(void) fprintf(fp, " %8lx  %8lx",
		(long)vnptr->v_data,
		(long)vnptr->v_filocks);
	(void) fprintf(fp, "    %s\n",
		vnptr->v_flag & VROOT ? "root" : " -");
	if (lock) {
		(void) fprintf(fp, "mutex v_lock:");
		prmutex(&(vnptr->v_lock));
		prcondvar(&vnptr->v_cv, "v_cv");
	}
}

static char *fileheading = "ADDRESS  RCNT    TYPE/ADDR       OFFSET   FLAGS\n";
static int filefull;

/* get arguments for file function */
int
getfile()
{
	struct nlist nl;
	struct fsnames *fsn;
	int all = 0;
	int phys = 0;
	int c;
	long filep;

	filefull = 0;
	optind = 1;
	while ((c = getopt(argcnt, args, "epfw:")) != EOF) {
		switch (c) {
			case 'e' :	all = 1;
					break;
			case 'f' :	filefull = 1;
					break;
			case 'p' :	phys = 1;
					break;
			case 'w' :	redirect();
					break;
			default  :	longjmp(syn, 0);
		}
	}

	for (fsn = fsnames; fsn->vnsym; fsn++) {
		nl_getsym(fsn->vnsym, &nl);
		fsn->vnaddr = nl.n_value;
	}

	if (!filefull)
		(void) fprintf(fp, "%s", fileheading);
	if (args[optind]) {
		all = 1;
		do {
			filep = strcon(args[optind], 'h');
			if (filep == -1)
				continue;
			else
				prfile(all, phys, (void *)filep);
			filep = -1;
		} while (args[++optind]);
	} else {
		kmem_cache_apply(kmem_cache_find("file_cache"), kmfile);
	}
	return (0);
}

/* ARGSUSED */
static void
kmfile(void *kaddr, void *buf)
{
	prfile(0, 0, kaddr);
}

/* print file table */
static void
prfile(int all, int phys, void *addr)
{
	struct fsnames *fsn;
	struct file fbuf;
	struct cred *credbufp;
	int ngrpbuf;
	short i;
	char fstyp[5];
	struct vnode vno;

	readbuf((unsigned)addr, 0, phys, -1, (char *)&fbuf, sizeof (fbuf),
		"file table");
	if (fbuf.f_count == 0 && all == 0)
		return;
	if (filefull)
		(void) fprintf(fp, "\n%s", fileheading);
	(void) fprintf(fp, "%.8lx", (long)addr);
	(void) fprintf(fp, " %3d", fbuf.f_count);

	if (fbuf.f_count && (fbuf.f_vnode != 0)) {
		/* read in vnode */
		readmem((unsigned)fbuf.f_vnode, 1, -1, (char *)&vno,
			sizeof (vno), "vnode");

		for (fsn = fsnames; fsn->vnsym; fsn++)
			if (vno.v_op == (struct vnodeops *)fsn->vnaddr)
				break;
		(void) strcpy(fstyp, fsn->name);

	} else
		(void) strcpy(fstyp, " ?  ");
	(void) fprintf(fp, "    %s/%8lx", fstyp, (long)fbuf.f_vnode);
	(void) fprintf(fp, " %10lld", fbuf.f_offset);
	(void) fprintf(fp, "  %s%s%s%s%s%s%s%s\n",
		fbuf.f_flag & FREAD ? " read" : "",
		fbuf.f_flag & FWRITE ? " write" : "",  /* print the file flag */
		fbuf.f_flag & FAPPEND ? " appen" : "",
		fbuf.f_flag & FSYNC ? " sync" : "",
		fbuf.f_flag & FCREAT ? " creat" : "",
		fbuf.f_flag & FTRUNC ? " trunc" : "",
		fbuf.f_flag & FEXCL ? " excl" : "",
		fbuf.f_flag & FNDELAY ? " ndelay" : "");

	if (!filefull)
		return;

	/* user credentials */
	if (!Ngrps)
		if (!(Ngrps = symsrch("ngroups_max")))
			error("ngroups_max not found in symbol table\n");
	readmem(Ngrps->st_value, 1, -1, (char *)&ngrpbuf,
		sizeof (ngrpbuf), "max groups");

	credbufp = (struct cred *)
		malloc(sizeof (struct cred) + sizeof (uid_t) * (ngrpbuf-1));

	readmem((unsigned)fbuf.f_cred, 1, -1, (char *)credbufp,
		sizeof (struct cred) + sizeof (uid_t) * (ngrpbuf-1),
		"user cred");

	(void) fprintf(fp, "User Credential:\n");
	(void) fprintf(fp, "\trcnt:%lu,  uid:%ld,  gid:%ld,  ruid:%ld,",
		credbufp->cr_ref,
		credbufp->cr_uid,
		credbufp->cr_gid,
		credbufp->cr_ruid);
	(void) fprintf(fp, "  rgid:%ld,  ngroup:%lu",
		credbufp->cr_rgid,
		credbufp->cr_ngroups);
	for (i = 0; i < (short)credbufp->cr_ngroups; i++) {
		if (!(i % 4))
			(void) fprintf(fp, "\n");
		(void) fprintf(fp, "group[%d]:%4ld ",
						i, credbufp->cr_groups[i]);
	}
	(void) fprintf(fp, "\n");
}
