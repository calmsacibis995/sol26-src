/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_PROC_PRDATA_H
#define	_SYS_PROC_PRDATA_H

#pragma ident	"@(#)prdata.h	1.39	96/08/08 SMI"	/* SVr4.0 1.18	*/

#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/prsystm.h>
#include <sys/poll.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	round4(r)	(((r) + 3) & (~3))
#define	round8(r)	(((r) + 7) & (~7))

#define	PNSIZ	5			/* size of /proc name entries */
#define	PLNSIZ	10			/* max size of /proc lwp name entries */

/*
 * Common file object to which all /proc vnodes for a specific process
 * or lwp refer.  One for the process, one for each lwp.
 */
typedef struct prcommon {
	kmutex_t	prc_mutex;	/* to wait for the proc/lwp to stop */
	kcondvar_t	prc_wait;	/* to wait for the proc/lwp to stop */
	u_short		prc_flags;	/* flags */
	u_int		prc_opens;	/* number of opens of prnodes */
	u_int		prc_writers;	/* number of write opens of prnodes */
	proc_t		*prc_proc;	/* process being traced */
	pid_t		prc_pid;	/* process id */
	int		prc_slot;	/* process slot number */
	kthread_t	*prc_thread;	/* thread (lwp) being traced */
	id_t		prc_tid;	/* thread (lwp) id */
	int		prc_tslot;	/* thread (lwp) slot number */
	struct pollhead	prc_pollhead;	/* list of all pollers */
} prcommon_t;

/* prc_flags */
#define	PRC_DESTROY	0x01	/* process or lwp is being destroyed */
#define	PRC_LWP		0x02	/* structure refers to an lwp */
#define	PRC_SYS		0x04	/* process is a system process */
#define	PRC_POLL	0x08	/* poll() in progress on this process/lwp */
#define	PRC_EXCL	0x10	/* exclusive access granted (old /proc) */

/*
 * Macros for mapping between i-numbers and pids.
 */
#define	pmkino(tslot, pslot, nodetype) \
	(((((tslot) << nproc_highbit) | (pslot)) << 6) | (nodetype) + 2)

/* for old /proc interface */
#define	PRBIAS	64
#define	ptoi(n) ((int)(((n) + PRBIAS)))		/* pid to i-number */

/*
 * Node types for /proc files (directories and files contained therein).
 */
typedef enum prnodetype {
	PR_PROCDIR,		/* /proc				*/
	PR_PIDDIR,		/* /proc/<pid>				*/
	PR_AS,			/* /proc/<pid>/as			*/
	PR_CTL,			/* /proc/<pid>/ctl			*/
	PR_STATUS,		/* /proc/<pid>/status			*/
	PR_LSTATUS,		/* /proc/<pid>/lstatus			*/
	PR_PSINFO,		/* /proc/<pid>/psinfo			*/
	PR_LPSINFO,		/* /proc/<pid>/lpsinfo			*/
	PR_MAP,			/* /proc/<pid>/map			*/
	PR_RMAP,		/* /proc/<pid>/rmap			*/
	PR_CRED,		/* /proc/<pid>/cred			*/
	PR_SIGACT,		/* /proc/<pid>/sigact			*/
	PR_AUXV,		/* /proc/<pid>/auxv			*/
#if defined(i386) || defined(__i386)
	PR_LDT,			/* /proc/<pid>/ldt			*/
#endif
	PR_USAGE,		/* /proc/<pid>/usage			*/
	PR_LUSAGE,		/* /proc/<pid>/lusage			*/
	PR_PAGEDATA,		/* /proc/<pid>/pagedata			*/
	PR_WATCH,		/* /proc/<pid>/watch			*/
	PR_OBJECTDIR,		/* /proc/<pid>/object			*/
	PR_OBJECT,		/* /proc/<pid>/object/xxx		*/
	PR_LWPDIR,		/* /proc/<pid>/lwp			*/
	PR_LWPIDDIR,		/* /proc/<pid>/lwp/<lwpid>		*/
	PR_LWPCTL,		/* /proc/<pid>/lwp/<lwpid>/lwpctl	*/
	PR_LWPSTATUS,		/* /proc/<pid>/lwp/<lwpid>/lwpstatus	*/
	PR_LWPSINFO,		/* /proc/<pid>/lwp/<lwpid>/lwpsinfo	*/
	PR_LWPUSAGE,		/* /proc/<pid>/lwp/<lwpid>/lwpusage	*/
	PR_XREGS,		/* /proc/<pid>/lwp/<lwpid>/xregs	*/
#if defined(sparc) || defined(__sparc)
	PR_GWINDOWS,		/* /proc/<pid>/lwp/<lwpid>/gwindows	*/
#endif
	PR_PIDFILE,		/* old process file			*/
	PR_LWPIDFILE,		/* old lwp file				*/
	PR_OPAGEDATA,		/* old page data file			*/
	PR_NFILES		/* number of /proc node types	*/
} prnodetype_t;

typedef struct prnode {
	vnode_t		*pr_next;	/* list of all vnodes for process */
	u_int		pr_flags;	/* private flags */
	kmutex_t	pr_mutex;	/* locks pr_files and child pr_flags */
	prnodetype_t	pr_type;	/* node type */
	mode_t		pr_mode;	/* file mode */
	ino_t		pr_ino;		/* node id (for stat(2)) */
	u_int		pr_hatid;	/* hat layer id for page data files */
	prcommon_t	*pr_common;	/* common data structure */
	prcommon_t	*pr_pcommon;	/* process common data structure */
	vnode_t		*pr_parent;	/* parent directory */
	vnode_t		**pr_files;	/* contained files array (directory) */
	u_int		pr_index;	/* position within parent */
	vnode_t		*pr_pidfile;	/* substitute vnode for old /proc */
	vnode_t		*pr_object;	/* real vnode for file in object dir */
	vnode_t		pr_vnode;	/* embedded vnode */
} prnode_t;

/*
 * Values for pr_flags.
 */
#define	PR_INVAL	0x01		/* vnode is invalidated */

/*
 * Conversion macros.
 */
#define	VTOP(vp)	((struct prnode *)(vp)->v_data)
#define	PTOV(pnp)	(&(pnp)->pr_vnode)

/*
 * Flags to prlock().
 */
#define	ZNO	0	/* Fail on encountering a zombie process. */
#define	ZYES	1	/* Allow zombies. */

/*
 * Assign one set to another (possible different sizes).
 *
 * Assigning to a smaller set causes members to be lost.
 * Assigning to a larger set causes extra members to be cleared.
 */
#define	prassignset(ap, sp)					\
{								\
	register int _i_ = sizeof (*(ap))/sizeof (u_long);	\
	while (--_i_ >= 0)					\
		((u_long*)(ap))[_i_] =				\
		    (_i_ >= sizeof (*(sp))/sizeof (u_long)) ?	\
		    0L : ((u_long*)(sp))[_i_];			\
}

/*
 * Determine whether or not a set (of arbitrary size) is empty.
 */
#define	prisempty(sp) \
	setisempty((uint32_t *)(sp), \
		(u_int)(sizeof (*(sp)) / sizeof (uint32_t)))

/*
 * Resource usage with times as hrtime_t rather than timestruc_t.
 * Each member exactly matches the corresponding member in prusage_t.
 * This is for convenience of internal computation.
 */
typedef struct prhusage {
	id_t		pr_lwpid;	/* lwp id.  0: process or defunct */
	u_long		pr_count;	/* number of contributing lwps */
	hrtime_t	pr_tstamp;	/* current time stamp */
	hrtime_t	pr_create;	/* process/lwp creation time stamp */
	hrtime_t	pr_term;	/* process/lwp termination time stamp */
	hrtime_t	pr_rtime;	/* total lwp real (elapsed) time */
	hrtime_t	pr_utime;	/* user level CPU time */
	hrtime_t	pr_stime;	/* system call CPU time */
	hrtime_t	pr_ttime;	/* other system trap CPU time */
	hrtime_t	pr_tftime;	/* text page fault sleep time */
	hrtime_t	pr_dftime;	/* data page fault sleep time */
	hrtime_t	pr_kftime;	/* kernel page fault sleep time */
	hrtime_t	pr_ltime;	/* user lock wait sleep time */
	hrtime_t	pr_slptime;	/* all other sleep time */
	hrtime_t	pr_wtime;	/* wait-cpu (latency) time */
	hrtime_t	pr_stoptime;	/* stopped time */
	hrtime_t	filltime[6];	/* filler for future expansion */
	u_long		pr_minf;	/* minor page faults */
	u_long		pr_majf;	/* major page faults */
	u_long		pr_nswap;	/* swaps */
	u_long		pr_inblk;	/* input blocks */
	u_long		pr_oublk;	/* output blocks */
	u_long		pr_msnd;	/* messages sent */
	u_long		pr_mrcv;	/* messages received */
	u_long		pr_sigs;	/* signals received */
	u_long		pr_vctx;	/* voluntary context switches */
	u_long		pr_ictx;	/* involuntary context switches */
	u_long		pr_sysc;	/* system calls */
	u_long		pr_ioch;	/* chars read and written */
	u_long		filler[10];	/* filler for future expansion */
} prhusage_t;

#if defined(_KERNEL)

extern	int	prnwatch;	/* number of supported watchpoints */
extern	int	nproc_highbit;	/* highbit(v.v_nproc) */

extern	struct prnode	prrootnode;
extern	struct vfs	*procvfs;
extern	struct vnodeops	prvnodeops;
extern	struct vfsops	prvfsops;

/* kludge to support old /proc interface */
#if !defined(_SYS_OLD_PROCFS_H)
extern	int	prgetmap(proc_t *, int, prmap_t **, size_t *);
#endif /* !defined(_SYS_OLD_PROCFS_H) */

extern	proc_t	*pr_p_lock(prnode_t *);
extern	int	pr_wait(prcommon_t *, time_t, time_t);
extern	void	pr_wait_die(prnode_t *);
extern	int	prusrio(struct as *, enum uio_rw, struct uio *, int);
extern	int	prwritectl(vnode_t *, struct uio *, struct cred *);
extern	int	prlock(prnode_t *, int);
extern	void	prunmark(proc_t *);
extern	void	prunlock(prnode_t *);
extern	long	prpdsize(struct as *);
extern	int	prpdread(struct as *, vnode_t *, u_int, struct uio *);
extern	long	oprpdsize(struct as *);
extern	int	oprpdread(struct as *, u_int, struct uio *);
extern	void	prgetaction(proc_t *, user_t *, u_int, struct sigaction *);
extern	void	prgetusage(kthread_t *, struct prhusage *);
extern	void	praddusage(kthread_t *, struct prhusage *);
extern	void	prcvtusage(struct prhusage *);
extern	kthread_t *prchoose(proc_t *);
extern	void	allsetrun(proc_t *);
extern	int	setisempty(uint32_t *, u_int);
extern	int	pr_utos(u_long, char *, int);
extern	vnode_t	*prlwpnode(pid_t, u_int);
extern	prnode_t *prgetnode(prnodetype_t);
extern	void	prfreenode(prnode_t *);
extern	void	pr_object_name(char *, vnode_t *, struct vattr *);
extern	int	set_watched_area(proc_t *, struct watched_area *,
			struct watched_page *);
extern	int	clear_watched_area(proc_t *, struct watched_area *);
extern	void	pr_free_watchlist(struct watched_area *);
extern	void	pr_free_pagelist(struct watched_page *);
extern	proc_t	*pr_cancel_watch(prnode_t *);

/*
 * Machine-dependent routines (defined in prmachdep.c).
 */
extern	void	prpokethread(kthread_t *t);
extern	user_t	*prumap(proc_t *);
extern	void	prunmap(proc_t *);
extern	void	prgetprregs(klwp_t *, prgregset_t);
extern	void	prsetprregs(klwp_t *, prgregset_t);
extern	int	prgetrvals(klwp_t *, long *, long *);
extern	prgreg_t prgetpc(prgregset_t);
extern	void	prgetprfpregs(klwp_t *, prfpregset_t *);
extern	void	prsetprfpregs(klwp_t *, prfpregset_t *);
extern	void	prgetprxregs(klwp_t *, caddr_t);
extern	void	prsetprxregs(klwp_t *, caddr_t);
extern	int	prgetprxregsize(void);
extern	int	prhasfp(void);
extern	int	prhasx(void);
extern	caddr_t	prgetstackbase(proc_t *);
extern	caddr_t	prgetpsaddr(proc_t *);
extern	int	prisstep(klwp_t *);
extern	void	prsvaddr(klwp_t *, caddr_t);
extern	int	prfetchinstr(klwp_t *, u_long *);
#if defined(i386) || defined(__i386)
struct ssd;
extern	int	prnldt(proc_t *);
extern	void	prgetldt(proc_t *, struct ssd *);
#endif

#endif	/* defined(_KERNEL) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PROC_PRDATA_H */
