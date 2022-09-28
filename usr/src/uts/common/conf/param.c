/*
 * Copyright (c) 1989-1996, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident	"@(#)param.c	2.130	96/10/18 SMI"	/* from SunOS */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/sysmacros.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <sys/var.h>
#include <sys/callo.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/fs/ufs_inode.h>
#include <sys/fs/ufs_quota.h>
#include <sys/dedump.h>
#include <sys/dumphdr.h>
#include <sys/conf.h>
#include <sys/class.h>
#include <sys/ts.h>
#include <sys/rt.h>
#include <sys/exec.h>
#include <sys/exechdr.h>
#include <sys/buf.h>
#include <sys/resource.h>
#include <vm/seg.h>
#include <sys/vmparam.h>
#include <sys/machparam.h>
#include <sys/utsname.h>
#include <sys/kmem.h>
#include <sys/stack.h>
#include <sys/modctl.h>
#include <sys/msgbuf.h>

#include <sys/map.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/clock.h>

/*
 * The following few lines describe generic things that must be compiled
 * into the booted executable (unix) rather than genunix or any other
 * module because they're required by kadb, crash dump readers, etc.
 */
int mod_mix_changed;		/* consumed by kadb and ksyms driver */
struct modctl modules;		/* head of linked list of modules */
char *default_path;		/* default module loading path */
struct swapinfo *swapinfo;	/* protected by the swapinfo_lock */
proc_t *practive;		/* active process list */
u_int nproc;			/* current number of processes */

/*
 * The following are "implementation architecture" dependent constants made
 * available here in the form of initialized data for use by "implementation
 * architecture" independent modules. See machparam.h.
 */
const unsigned int	_pagesize	= (unsigned int)PAGESIZE;
const unsigned int	_pageshift	= (unsigned int)PAGESHIFT;
const unsigned int	_pageoffset	= (unsigned int)PAGEOFFSET;
/*
 * XXX - This value pagemask has to be a 64bit size because
 * large file support uses this mask on offsets which are 64 bit size.
 * using unsigned leaves the higher 32 bits value as zero thus
 * corrupting offset calculations in the file system and VM.
 */
const u_longlong_t	_pagemask	= (u_longlong_t)PAGEMASK;
const unsigned int	_mmu_pagesize	= (unsigned int)MMU_PAGESIZE;
const unsigned int	_mmu_pageshift	= (unsigned int)MMU_PAGESHIFT;
const unsigned int	_mmu_pageoffset	= (unsigned int)MMU_PAGEOFFSET;
const unsigned int	_mmu_pagemask	= (unsigned int)MMU_PAGEMASK;
const unsigned int	_kernelbase	= (unsigned int)KERNELBASE;
const unsigned int	_userlimit	= (unsigned int)USERLIMIT;
const unsigned int	_argsbase	= (unsigned int)ARGSBASE;
const unsigned int	_diskrpm	= (unsigned int)DISKRPM;
const unsigned long	_dsize_limit	= (unsigned long)DSIZE_LIMIT;
const unsigned long	_ssize_limit	= (unsigned long)SSIZE_LIMIT;
const unsigned int	_pgthresh	= (unsigned int)PGTHRESH;
const unsigned int	_maxslp		= (unsigned int)MAXSLP;
const unsigned int	_maxhandspreadpages = (unsigned int)MAXHANDSPREADPAGES;
const int		_ncpu 		= (int)NCPU;
const unsigned int	_defaultstksz	= (unsigned int)DEFAULTSTKSZ;
const unsigned int	_msg_bsize	= (unsigned int)MSG_BSIZE;
const unsigned int	_nbpg		= (unsigned int)NBPG;
const unsigned int	_usrstack	= (unsigned int)USRSTACK;

/*
 * System parameter formulae.
 *
 * This file is copied into each directory where we compile
 * the kernel; it should be modified there to suit local taste
 * if necessary.
 */

/*
 * Default hz is 100, but if we set hires_tick we get higher resolution
 * clock behavior (currently defined to be 1000 hz).  Higher values seem
 * to work, but are not supported.
 */
int hz = 100;
int hires_hz = 1000;
int hires_tick = 0;
int usec_per_tick;	/* microseconds per clock tick */
int nsec_per_tick;	/* nanoseconds per clock tick */
int max_hres_adj;	/* maximum adjustment of hrtime per tick */

/*
 * Tables of initialization functions, called from main().
 */

extern void binit(void);
extern void space_init(void);
extern void cred_init(void);
extern void dnlc_init(void);
extern void vfsinit(void);
extern void finit(void);
extern void strinit(void);
extern void flk_init(void);
#ifdef TRACE
extern void inittrace(void);
#endif /* TRACE */
#ifdef sparc
extern void init_clock_thread(void);
#endif
extern void softcall_init(void);
extern void sadinit(void);
extern void loginit(void);
extern void ttyinit(void);
extern void mp_strinit(void);
extern void schedctl_init(void);

void	(*init_tbl[])(void) = {
#ifdef sparc
	init_clock_thread,
#endif
	binit,
	space_init,
	cred_init,
	dnlc_init,
	vfsinit,
	finit,
	strinit,
#ifdef TRACE
	inittrace,
#endif /* TRACE */
	softcall_init,
	sadinit,
	loginit,
	ttyinit,
	as_init,
	anon_init,
	segvn_init,
	flk_init,
	schedctl_init,
	0
};


/*
 * Any per cpu resources should be initialized via
 * an entry in mp_init_tbl().
 */

void	(*mp_init_tbl[])(void) = {
	mp_strinit,
	0
};

int maxusers;		/* kitchen-sink knob for dynamic configuration */

/*
 * autoup -- used in struct var for dynamic config of the age a delayed-write
 * buffer must be in seconds before bdflush will write it out.
 */
int autoup = 30;

/*
 * bufhwm -- tuneable variable for struct var for v_bufhwm.
 * high water mark for buffer cache mem usage in units of K bytes.
 */
int bufhwm = 0;

/*
 * Process table.
 */
int max_nprocs;		/* set in param_init() */
int maxuprc;		/* set in param_init() */
int reserved_procs;
int nthread = 0;

/*
 * UFS tunables
 */
int ufs_iincr = 30;				/* */
int ufs_allocinode = 0;				/* */
int ufs_ninode;		/* set in param_init() */
int ncsize;		/* set in param_init() # of dnlc entries */

struct dquot *dquot, *dquotNDQUOT;		/* */
int ndquot;		/* set in param_init() */

#if defined(DEBUG)
	/* default: don't do anything */
int ufs_debug = UDBG_OFF;
#endif /* DEBUG */

/*
 * Exec switch table. This is used by the generic exec module
 * to switch out to the desired executable type, based on the
 * magic number. The currently supported types are ELF, a.out
 * (both NMAGIC and ZMAGIC), and interpreter (#!) files.
 */

short elfmagic = 0x7f45;
short intpmagic = 0x2321;
#ifdef sparc
short aout_nmagic = NMAGIC;
short aout_zmagic = ZMAGIC;
short aout_omagic = OMAGIC;
#endif
#ifdef i386
short coffmagic = 0x4c01;	/* octal 0514 byte-flipped */
#endif
short nomagic = 0;

char *execswnames[] = {
#ifdef sparc
	"elfexec", "intpexec", "aoutexec", "aoutexec", "aoutexec",
	NULL, NULL, NULL
#endif
#ifdef i386
	"elfexec", "intpexec", "coffexec", NULL, NULL, NULL, NULL,
#endif
#if defined(__ppc)
	"elfexec", "intpexec", NULL, NULL, NULL, NULL,
#endif
};

struct execsw execsw[] = {
	&elfmagic, NULL, NULL, NULL,
	&intpmagic, NULL, NULL, NULL,
#ifdef sparc
	&aout_zmagic, NULL, NULL, NULL,
	&aout_nmagic, NULL, NULL, NULL,
	&aout_omagic, NULL, NULL, NULL,
#endif
#ifdef i386
	&coffmagic, NULL, NULL, NULL,
	&nomagic, NULL, NULL, NULL,
#endif
	&nomagic, NULL, NULL, NULL,
	&nomagic, NULL, NULL, NULL,
	&nomagic, NULL, NULL, NULL,
};
int nexectype = sizeof (execsw) / sizeof (execsw[0]);	/* # of exec types */
kmutex_t execsw_lock;	/* Used for allocation of execsw entries */

/*
 * symbols added to make changing max-file-descriptors
 * simple via /etc/system
 */
#define	RLIM_FD_CUR 0x40
#define	RLIM_FD_MAX 0x400

u_int rlim_fd_cur = RLIM_FD_CUR;
u_int rlim_fd_max = RLIM_FD_MAX;


/*
 * Default resource limits.
 *
 *	Softlimit	Hardlimit
 */
struct rlimit64 rlimits[RLIM_NLIMITS] = {
	/* max CPU time */
	(rlim64_t)RLIM64_INFINITY,	(rlim64_t)RLIM64_INFINITY,
	/* max file size */
	(rlim64_t)RLIM64_INFINITY,	(rlim64_t)RLIM64_INFINITY,
	/* max data size */
	(rlim64_t)(u_int)DFLDSIZ,	(rlim64_t)(u_int)MAXDSIZ,
	/* max stack */
	(rlim64_t)(u_int)DFLSSIZ,	(rlim64_t)(u_int)MAXSSIZ,
	/* max core file size */
	(rlim64_t)RLIM64_INFINITY,	(rlim64_t)RLIM64_INFINITY,
	/* max file descriptors */
	(rlim64_t)RLIM_FD_CUR,		(rlim64_t)RLIM_FD_MAX,
	/* max mapped memory */
	(rlim64_t)RLIM64_INFINITY,	(rlim64_t)RLIM64_INFINITY,
};

/*
 * Why this special map from infinity to actual values?
 * Infinity is a concept and not a number. Our code treating
 * this RLIM_INFINITY as a number has caused problems with
 * the behaviour of setrlimit/getrlimit functions and future
 * extensions.
 * This array maps the concept of infinity for a resource
 * to a system defined maximum.
 * This means when we extend system to support greater
 * system maximum's we only need to update this array.
 * Note: We haven't actually tried to increase system limits
 * by changing this array value.
 */

rlim64_t	rlim_infinity_map[RLIM_NLIMITS] = {
	/* max CPU time */
	(rlim64_t)ULONG_MAX,
	/* max file size */
	(rlim64_t)MAXOFFSET_T,
	/* max data size */
	(rlim64_t)(u_int)DSIZE_LIMIT,
	/* max stack */
	(rlim64_t)(u_int)SSIZE_LIMIT,
	/* max core file size */
	(rlim64_t)MAXOFF_T,
	/* max file descriptors */
	(rlim64_t)MAXOFF_T,
	/* max mapped memory */
	(rlim64_t)ULONG_MAX,
};
/*
 * file and record locking
 */
struct flckinfo flckinfo;

/*
 * Streams tunables
 */
int	nstrpush = 9;
int	maxsepgcnt = 1;

/*
 * strmsgsz is the size for the maximum streams message a user can create.
 * for Release 4.0, a value of zero will indicate no upper bound.  This
 * parameter will disappear entirely in the next release.
 */

ssize_t	strmsgsz = 0x10000;
ssize_t	strctlsz = 1024;
int	rstchown = 1;		/* POSIX_CHOWN_RESTRICTED is enabled */
int	ngroups_max = NGROUPS_MAX_DEFAULT;

int	nservers = 0;		/* total servers in system */
int	n_idleservers = 0;	/* idle servers in system */
int	n_sr_msgs = 0;		/* receive descriptors in msg queue */

#define	NSTRPIPE 60		/* XXX - need right number for this! */
int spcnt = NSTRPIPE;
struct sp {
	queue_t *sp_rdq;	/* this stream's read queue */
	queue_t *sp_ordq;	/* other stream's read queue */
} sp_sp[NSTRPIPE];

/*
 * This is for the streams message debugging module.
 */

int dump_cnt = 5;
struct dmp *dump;

/*
 * This has to be allocated somewhere; allocating
 * them here forces loader errors if this file is omitted.
 */
struct	inode *ufs_inode;

/*
 * generic scheduling stuff
 *
 * Configurable parameters for RT and TS are in the respective
 * scheduling class modules.
 */
#include <sys/disp.h>

pri_t maxclsyspri = MAXCLSYSPRI;
pri_t minclsyspri = MINCLSYSPRI;

int maxclass_sz = SA(MAX((sizeof (rtproc_t)), (sizeof (tsproc_t))));
int maxclass_szd = (SA(MAX((sizeof (rtproc_t)), (sizeof (tsproc_t)))) /
	sizeof (double));
char	sys_name[] = "SYS";
char	ts_name[] = "TS";
char	rt_name[] = "RT";

extern void sys_init();
extern classfuncs_t sys_classfuncs;

sclass_t sclass[] = {
	"SYS",	sys_init,	&sys_classfuncs, STATIC_SCHED, 0, 0,
	"",	NULL,	NULL,	NULL, 0, 0,
	"",	NULL,	NULL,	NULL, 0, 0,
	"",	NULL,	NULL,	NULL, 0, 0,
	"",	NULL,	NULL,	NULL, 0, 0,
	"",	NULL,	NULL,	NULL, 0, 0,
};
int loaded_classes = 1;		/* for loaded classes */
kmutex_t class_lock;		/* lock for class[] */

int nclass = sizeof (sclass) / sizeof (sclass_t);
char initcls[] = "TS";
char *initclass = initcls;

/*
 * High Resolution Timers
 */
uint	timer_resolution = 100;

/*
 * Streams log driver
 */
#include <sys/log.h>
#ifndef NLOG
#define	NLOG 16
#endif

int		log_cnt = NLOG+CLONEMIN+1;
struct log	log_log[NLOG+CLONEMIN+1];

/*
 * Tunable system parameters.
 */
#include <sys/tuneable.h>

/*
 * The integers tune_* are done this way so that the tune
 * data structure may be "tuned" if necessary from the /etc/system
 * file. The tune data structure is initialized in param_init();
 */

tune_t tune;

/*
 * If freemem < t_getpgslow, then start to steal pages from processes.
 */
int tune_t_gpgslo = 25;

/*
 * Rate at which fsflush is run, in seconds.
 */
int tune_t_fsflushr = 5;

/*
 * The minimum available resident (not swappable) memory to maintain
 * in order to avoid deadlock.  In pages.
 */
int tune_t_minarmem = 25;

/*
 * The minimum available swappable memory to maintain in order to avoid
 * deadlock.  In pages.
 */
int tune_t_minasmem = 25;

int tune_t_flckrec = 512;	/* max # of active frlocks */

struct map *kernelmap;
struct map *ekernelmap;
u_int pages_pp_maximum = 200;

int boothowto;			/* boot flags passed to kernel */
daddr_t swplo;			/* starting disk address of swap area */
struct vnode *dumpvp;		/* ptr to vnode of dump device */
struct var v;			/* System Configuration Information */

/*
 * System Configuration Information
 */

#ifdef sparc
char hw_serial[11];		/* read from prom at boot time */
char architecture[] = "sparc";
char hw_provider[] = "Sun_Microsystems";
#endif
#ifdef i386
/*
 * On x86 machines, read hw_serial, hw_provider and srpc_domain from
 * /etc/bootrc at boot time.
 */
char architecture[] = "i386";
char hw_serial[11] = "0";
char hw_provider[SYS_NMLN] = "";
#endif
#if defined(__ppc)
char architecture[] = "ppc";
char hw_serial[11] = "0";	/* read from prom at boot time */
char hw_provider[SYS_NMLN] = "";
#endif
char srpc_domain[SYS_NMLN] = "";
char platform[SYS_NMLN] = "";	/* read from the devinfo root node */

/* Initialize isa_list */
char *isa_list = architecture;

#ifdef XENIX_COMPAT
int	emgetmap = 0;
int	emsetmap = 0;
int	emuneap = 0;
#endif

void
param_calc(maxusers)
int maxusers;
{
	/*
	 * We need to dynamically change any variables now so that
	 * the setting of maxusers propagates to the other variables
	 * that are dependent on maxusers.
	 */
	reserved_procs = 5;
	max_nprocs = (10 + 16 * maxusers);
	if (max_nprocs > MAXPID) {
		max_nprocs = MAXPID;
	}

	ufs_ninode = (max_nprocs + 16 + maxusers) + 64;
	ndquot =  ((maxusers * NMOUNT) / 4) + max_nprocs;
	maxuprc = (max_nprocs - reserved_procs);
	ncsize = (max_nprocs + 16 + maxusers) + 64; /* # of dnlc entries */
}

void
param_init()
{
	dump = kmem_zalloc(dump_cnt * sizeof (struct dmp), KM_SLEEP);

	/*
	 * Set each individual element of struct var v to be the
	 * default value. This is done this way
	 * so that a user can set the assigned integer value in the
	 * /etc/system file *IF* tuning is needed.
	 */
	v.v_proc = max_nprocs;	/* v_proc - max # of processes system wide */
	v.v_maxupttl = max_nprocs - reserved_procs;
	v.v_maxsyspri = (int)maxclsyspri;  /* max global pri for sysclass */
	v.v_maxup = min(maxuprc, v.v_maxupttl); /* max procs per user */
	v.v_autoup = autoup;	/* v_autoup - delay for delayed writes */

	/*
	 * Set each individual element of struct tune to be the
	 * default value. Each struct element This is done this way
	 *  so that a user can set the assigned integer value in the
	 * /etc/system file *IF* tuning is needed.
	 */
	tune.t_gpgslo = tune_t_gpgslo;
	tune.t_fsflushr = tune_t_fsflushr;
	tune.t_minarmem = tune_t_minarmem;
	tune.t_minasmem = tune_t_minasmem;
	tune.t_flckrec = tune_t_flckrec;

	/*
	 * initialization for max file descriptors
	 */
	if (rlim_fd_cur > rlim_fd_max)
		rlim_fd_cur = rlim_fd_max;

	rlimits[RLIMIT_NOFILE].rlim_cur = rlim_fd_cur;
	rlimits[RLIMIT_NOFILE].rlim_max = rlim_fd_max;

	/*
	 * calculations needed if hz was set in /etc/system
	 */
	if (hires_tick)
		hz = hires_hz;
	usec_per_tick = MICROSEC / hz;
	nsec_per_tick = NANOSEC / hz;
	max_hres_adj = (NANOSEC / hz) >> ADJ_SHIFT;
}

/*
 * check certain configurable parameters.
 */
void
param_check(void)
{
	if (ngroups_max < NGROUPS_UMIN || ngroups_max > NGROUPS_UMAX)
		ngroups_max = NGROUPS_MAX_DEFAULT;
}
