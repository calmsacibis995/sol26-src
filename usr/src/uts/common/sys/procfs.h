/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_PROCFS_H
#define	_SYS_PROCFS_H

#pragma ident	"@(#)procfs.h	1.4	96/08/08 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This definition is temporary.  Structured proc is the preferred API,
 * and the older ioctl-based interface will be removed in a future version
 * of Solaris.  Until then, by default, including <sys/procfs.h> will
 * provide the older ioctl-based /proc definitions.  To get the structured
 * /proc definitions, either include <procfs.h> or define _STRUCTURED_PROC
 * to be 1 before including <sys/procfs.h>.
 */
#ifndef	_STRUCTURED_PROC
#define	_STRUCTURED_PROC	0
#endif

#if !defined(_KERNEL) && _STRUCTURED_PROC == 0

#include <sys/old_procfs.h>

#else	/* !defined(_KERNEL) && _STRUCTURED_PROC == 0 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <sys/siginfo.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <sys/pset.h>
#include <sys/procfs_isa.h>

/*
 * System call interfaces for /proc.
 */

/*
 * Control codes (long values) for messages written to ctl and lwpctl files.
 */
#define	PCNULL   0L	/* null request, advance to next message */
#define	PCSTOP   1L	/* direct process or lwp to stop and wait for stop */
#define	PCDSTOP  2L	/* direct process or lwp to stop */
#define	PCWSTOP  3L	/* wait for process or lwp to stop, no timeout */
#define	PCTWSTOP 4L	/* wait for stop, with long millisecond timeout arg */
#define	PCRUN    5L	/* make process/lwp runnable, w/ long flags argument */
#define	PCCSIG   6L	/* clear current signal from lwp */
#define	PCCFAULT 7L	/* clear current fault from lwp */
#define	PCSSIG   8L	/* set current signal from siginfo_t argument */
#define	PCKILL   9L	/* post a signal to process/lwp, long argument */
#define	PCUNKILL 10L	/* delete a pending signal from process/lwp, long arg */
#define	PCSHOLD  11L	/* set lwp signal mask from sigset_t argument */
#define	PCSTRACE 12L	/* set traced signal set from sigset_t argument */
#define	PCSFAULT 13L	/* set traced fault set from fltset_t argument */
#define	PCSENTRY 14L	/* set traced syscall entry set from sysset_t arg */
#define	PCSEXIT  15L	/* set traced syscall exit set from sysset_t arg */
#define	PCSET    16L	/* set modes from long argument */
#define	PCUNSET  17L	/* unset modes from long argument */
#define	PCSREG   18L	/* set lwp general registers from prgregset_t arg */
#define	PCSFPREG 19L	/* set lwp floating-point registers from prfpregset_t */
#define	PCSXREG  20L	/* set lwp extra registers from prxregset_t arg */
#define	PCNICE   21L	/* set nice priority from long argument */
#define	PCSVADDR 22L	/* set %pc virtual address from long argument */
#define	PCWATCH  23L	/* set/unset watched memory area from prwatch_t arg */

/*
 * PCRUN long operand flags.
 */
#define	PRCSIG		0x01	/* clear current signal, if any */
#define	PRCFAULT	0x02	/* clear current fault, if any */
#define	PRSTEP		0x04	/* direct the lwp to single-step */
#define	PRSABORT	0x08	/* abort syscall, if in syscall */
#define	PRSTOP		0x10	/* set directed stop request */

/*
 * lwp status file.
 */
#define	PRCLSZ		8	/* maximum size of scheduling class name */
#define	PRSYSARGS	8	/* maximum number of syscall arguments */
typedef struct lwpstatus {
	int	pr_flags;	/* flags (see below) */
	id_t	pr_lwpid;	/* specific lwp identifier */
	short	pr_why;		/* reason for lwp stop, if stopped */
	short	pr_what;	/* more detailed reason */
	short	pr_cursig;	/* current signal, if any */
	short	pr_pad1;
	siginfo_t pr_info;	/* info associated with signal or fault */
	sigset_t pr_lwppend;	/* set of signals pending to the lwp */
	sigset_t pr_lwphold;	/* set of signals blocked by the lwp */
	struct sigaction pr_action;	/* signal action for current signal */
	stack_t	pr_altstack;	/* alternate signal stack info */
	uintptr_t pr_oldcontext;	/* address of previous ucontext */
	short	pr_syscall;	/* system call number (if in syscall) */
	short	pr_nsysarg;	/* number of arguments to this syscall */
	int	pr_errno;	/* errno for failed syscall, 0 if successful */
	long	pr_sysarg[PRSYSARGS];	/* arguments to this syscall */
	long	pr_rval1;	/* primary syscall return value */
	long	pr_rval2;	/* second syscall return value, if any */
	char	pr_clname[PRCLSZ];	/* scheduling class name */
	timestruc_t pr_tstamp;	/* real-time time stamp of stop */
	int	pr_filler[12];	/* reserved for future use */
	u_long	pr_pad2;
	u_long	pr_instr;	/* current instruction */
	prgregset_t pr_reg;	/* general registers */
	prfpregset_t pr_fpreg;	/* floating-point registers */
} lwpstatus_t;

/*
 * process status file.
 */
typedef struct pstatus {
	int	pr_flags;	/* flags (see below) */
	int	pr_nlwp;	/* number of lwps in the process */
	pid_t	pr_pid;		/* process id */
	pid_t	pr_ppid;	/* parent process id */
	pid_t	pr_pgid;	/* process group id */
	pid_t	pr_sid;		/* session id */
	id_t	pr_aslwpid;	/* lwp id of the aslwp, if any */
	id_t	pr_pad;
	sigset_t pr_sigpend;	/* set of process pending signals */
	uintptr_t pr_brkbase;	/* address of the process heap */
	size_t	pr_brksize;	/* size of the process heap, in bytes */
	uintptr_t pr_stkbase;	/* address of the process stack */
	size_t	pr_stksize;	/* size of the process stack, in bytes */
	timestruc_t pr_utime;	/* process user cpu time */
	timestruc_t pr_stime;	/* process system cpu time */
	timestruc_t pr_cutime;	/* sum of children's user times */
	timestruc_t pr_cstime;	/* sum of children's system times */
	sigset_t pr_sigtrace;	/* set of traced signals */
	fltset_t pr_flttrace;	/* set of traced faults */
	sysset_t pr_sysentry;	/* set of system calls traced on entry */
	sysset_t pr_sysexit;	/* set of system calls traced on exit */
	int	pr_filler[20];	/* reserved for future use */
	lwpstatus_t pr_lwp;	/* status of the representative lwp */
} pstatus_t;

/*
 * pr_flags (same values appear in both pstatus_t and lwpstatus_t pr_flags).
 */
/* The following flags apply to the specific or representative lwp */
#define	PR_STOPPED 0x00000001	/* lwp is stopped */
#define	PR_ISTOP   0x00000002	/* lwp is stopped on an event of interest */
#define	PR_DSTOP   0x00000004	/* lwp has a stop directive in effect */
#define	PR_STEP    0x00000008	/* lwp has a single-step directive in effect */
#define	PR_ASLEEP  0x00000010	/* lwp is sleeping in a system call */
#define	PR_PCINVAL 0x00000020	/* contents of pr_instr undefined */
#define	PR_ASLWP   0x00000040	/* this lwp is the aslwp */
/* The following flags apply to the process, not to an individual lwp */
#define	PR_ISSYS   0x00001000	/* this is a system process */
#define	PR_VFORKP  0x00002000	/* process is the parent of a vfork()d child */
/* The following process flags are modes settable by PCSET/PCUNSET */
#define	PR_FORK    0x00100000	/* inherit-on-fork is in effect */
#define	PR_RLC	   0x00200000	/* run-on-last-close is in effect */
#define	PR_KLC	   0x00400000	/* kill-on-last-close is in effect */
#define	PR_ASYNC   0x00800000	/* asynchronous-stop is in effect */
#define	PR_MSACCT  0x01000000	/* micro-state usage accounting is in effect */
#define	PR_BPTADJ  0x02000000	/* breakpoint trap pc adjustment is in effect */
#define	PR_PTRACE  0x04000000	/* ptrace-compatibility mode is in effect */

/*
 * Reasons for stopping (pr_why).
 */
#define	PR_REQUESTED	1
#define	PR_SIGNALLED	2
#define	PR_SYSENTRY	3
#define	PR_SYSEXIT	4
#define	PR_JOBCONTROL	5
#define	PR_FAULTED	6
#define	PR_SUSPENDED	7
#define	PR_CHECKPOINT	8

/*
 * lwp ps(1) information file.
 */
#define	PRFNSZ		16	/* Maximum size of execed filename */
typedef struct lwpsinfo {
	int	pr_flag;	/* lwp flags */
	id_t	pr_lwpid;	/* lwp id */
	uintptr_t pr_addr;	/* internal address of lwp */
	uintptr_t pr_wchan;	/* wait addr for sleeping lwp */
	char	pr_stype;	/* synchronization event type */
	char	pr_state;	/* numeric lwp state */
	char	pr_sname;	/* printable character for pr_state */
	char	pr_nice;	/* nice for cpu usage */
	short	pr_syscall;	/* system call number (if in syscall) */
	char	pr_oldpri;	/* pre-SVR4, low value is high priority */
	char	pr_cpu;		/* pre-SVR4, cpu usage for scheduling */
	int	pr_pri;		/* priority, high value is high priority */
			/* The following percent number is a 16-bit binary */
			/* fraction [0 .. 1] with the binary point to the */
			/* right of the high-order bit (1.0 == 0x8000) */
	u_short	pr_pctcpu;	/* % of recent cpu time used by this lwp */
	u_short	pr_pad;
	timestruc_t pr_start;	/* lwp start time, from the epoch */
	timestruc_t pr_time;	/* usr+sys cpu time for this lwp */
	char	pr_clname[PRCLSZ];	/* scheduling class name */
	char	pr_name[PRFNSZ];	/* name of system lwp */
	processorid_t pr_onpro;		/* processor which last ran this lwp */
	processorid_t pr_bindpro;	/* processor to which lwp is bound */
	psetid_t pr_bindpset;	/* processor set to which lwp is bound */
	int	pr_filler[5];	/* reserved for future use */
} lwpsinfo_t;

/*
 * process ps(1) information file.
 */
#define	PRARGSZ		80	/* number of chars of arguments */
typedef struct psinfo {
	int	pr_flag;	/* process flags */
	int	pr_nlwp;	/* number of lwps in process */
	pid_t	pr_pid;		/* unique process id */
	pid_t	pr_ppid;	/* process id of parent */
	pid_t	pr_pgid;	/* pid of process group leader */
	pid_t	pr_sid;		/* session id */
	uid_t	pr_uid;		/* real user id */
	uid_t	pr_euid;	/* effective user id */
	gid_t	pr_gid;		/* real group id */
	gid_t	pr_egid;	/* effective group id */
	uintptr_t pr_addr;	/* address of process */
	size_t	pr_size;	/* size of process image in Kbytes */
	size_t	pr_rssize;	/* resident set size in Kbytes */
	size_t	pr_pad;
	dev_t	pr_ttydev;	/* controlling tty device (or PRNODEV) */
			/* The following percent numbers are 16-bit binary */
			/* fractions [0 .. 1] with the binary point to the */
			/* right of the high-order bit (1.0 == 0x8000) */
	u_short	pr_pctcpu;	/* % of recent cpu time used by all lwps */
	u_short	pr_pctmem;	/* % of system memory used by process */
	timestruc_t pr_start;	/* process start time, from the epoch */
	timestruc_t pr_time;	/* usr+sys cpu time for this process */
	timestruc_t pr_ctime;	/* usr+sys cpu time for reaped children */
	char	pr_fname[PRFNSZ];	/* name of execed file */
	char	pr_psargs[PRARGSZ];	/* initial characters of arg list */
	int	pr_wstat;	/* if zombie, the wait() status */
	int	pr_argc;	/* initial argument count */
	uintptr_t pr_argv;	/* address of initial argument vector */
	uintptr_t pr_envp;	/* address of initial environment vector */
	int	pr_filler[8];	/* reserved for future use */
	lwpsinfo_t pr_lwp;	/* information for representative lwp */
} psinfo_t;

#define	PRNODEV	(dev_t)(-1)	/* non-existent device */

/*
 * Memory-management interface.
 */
#define	PRMAPSZ	64
typedef struct prmap {
	uintptr_t pr_vaddr;	/* virtual address of mapping */
	size_t	pr_size;	/* size of mapping in bytes */
	char	pr_mapname[PRMAPSZ];	/* name in /proc/pid/object */
	offset_t pr_offset;	/* offset into mapped object, if any */
	int	pr_mflags;	/* protection and attribute flags */
	int	pr_pagesize;	/* pagesize (bytes) for this mapping */
	int	pr_filler[2];	/* filler for future expansion */
} prmap_t;

/* Protection and attribute flags */
#define	MA_READ		0x04	/* readable by the traced process */
#define	MA_WRITE	0x02	/* writable by the traced process */
#define	MA_EXEC		0x01	/* executable by the traced process */
#define	MA_SHARED	0x08	/* changes are shared by mapped object */
/*
 * These are obsolete and unreliable.
 * They are included here only for historical compatibility.
 */
#define	MA_BREAK	0x10	/* grown by brk(2) */
#define	MA_STACK	0x20	/* grown automatically on stack faults */

/*
 * Process credentials.
 */
typedef struct prcred {
	uid_t	pr_euid;	/* effective user id */
	uid_t	pr_ruid;	/* real user id */
	uid_t	pr_suid;	/* saved user id (from exec) */
	gid_t	pr_egid;	/* effective group id */
	gid_t	pr_rgid;	/* real group id */
	gid_t	pr_sgid;	/* saved group id (from exec) */
	int	pr_ngroups;	/* number of supplementary groups */
	gid_t	pr_groups[1];	/* array of supplementary groups */
} prcred_t;

/*
 * Watchpoint interface.
 */
typedef struct prwatch {
	uintptr_t pr_vaddr;	/* virtual address of watched area */
	size_t	pr_size;	/* size of watched area in bytes */
	int	pr_wflags;	/* watch type flags */
	int	pr_pad;
} prwatch_t;

/* pr_wflags */
#define	WA_READ		0x04	/* trap on read access */
#define	WA_WRITE	0x02	/* trap on write access */
#define	WA_EXEC		0x01	/* trap on execute access */
#define	WA_TRAPAFTER	0x08	/* trap after instruction completes */

/*
 * Resource usage.
 */
typedef struct prusage {
	id_t		pr_lwpid;	/* lwp id.  0: process or defunct */
	int		pr_count;	/* number of contributing lwps */
	timestruc_t	pr_tstamp;	/* current time stamp */
	timestruc_t	pr_create;	/* process/lwp creation time stamp */
	timestruc_t	pr_term;	/* process/lwp termination time stamp */
	timestruc_t	pr_rtime;	/* total lwp real (elapsed) time */
	timestruc_t	pr_utime;	/* user level cpu time */
	timestruc_t	pr_stime;	/* system call cpu time */
	timestruc_t	pr_ttime;	/* other system trap cpu time */
	timestruc_t	pr_tftime;	/* text page fault sleep time */
	timestruc_t	pr_dftime;	/* data page fault sleep time */
	timestruc_t	pr_kftime;	/* kernel page fault sleep time */
	timestruc_t	pr_ltime;	/* user lock wait sleep time */
	timestruc_t	pr_slptime;	/* all other sleep time */
	timestruc_t	pr_wtime;	/* wait-cpu (latency) time */
	timestruc_t	pr_stoptime;	/* stopped time */
	timestruc_t	filltime[6];	/* filler for future expansion */
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
} prusage_t;

/*
 * Page data file.
 */

/* page data file header */
typedef struct prpageheader {
	timestruc_t	pr_tstamp;	/* real time stamp */
	long		pr_nmap;	/* number of address space mappings */
	long		pr_npage;	/* total number of pages */
} prpageheader_t;

/* page data mapping header */
typedef struct prasmap {
	uintptr_t pr_vaddr;	/* virtual address of mapping */
	size_t	pr_npage;	/* number of pages in mapping */
	char	pr_mapname[PRMAPSZ];	/* name in /proc/pid/object */
	offset_t pr_offset;	/* offset into mapped object, if any */
	int	pr_mflags;	/* protection and attribute flags */
	int	pr_pagesize;	/* pagesize (bytes) for this mapping */
	int	pr_filler[2];	/* filler for future expansion */
} prasmap_t;

/*
 * pr_npage bytes (plus 0-7 null bytes to round up to an 8-byte boundary)
 * follow each mapping header, each containing zero or more of these flags.
 */
#define	PG_REFERENCED	0x02		/* page referenced since last read */
#define	PG_MODIFIED	0x01		/* page modified since last read */
#define	PG_HWMAPPED	0x04		/* page is present and mapped */

/*
 * Header for the lstatus, lpsinfo, and lusage files.
 */
typedef struct prheader {
	int	pr_nent;	/* number of entries */
	int	pr_entsize;	/* size of each entry, in bytes */
} prheader_t;

/*
 * Macros for manipulating sets of flags.
 * sp must be a pointer to one of sigset_t, fltset_t, or sysset_t.
 * flag must be a member of the enumeration corresponding to *sp.
 */

/* turn on all flags in set */
#define	prfillset(sp) \
	{ register int _i_ = sizeof (*(sp))/sizeof (uint32_t); \
		while (_i_) ((uint32_t *)(sp))[--_i_] = (uint32_t)0xFFFFFFFF; }

/* turn off all flags in set */
#define	premptyset(sp) \
	{ register int _i_ = sizeof (*(sp))/sizeof (uint32_t); \
		while (_i_) ((uint32_t *)(sp))[--_i_] = (uint32_t)0; }

/* turn on specified flag in set */
#define	praddset(sp, flag) \
	((void)(((unsigned)((flag)-1) < 32*sizeof (*(sp))/sizeof (uint32_t)) ? \
	(((uint32_t *)(sp))[((flag)-1)/32] |= (1U<<(((flag)-1)%32))) : 0))

/* turn off specified flag in set */
#define	prdelset(sp, flag) \
	((void)(((unsigned)((flag)-1) < 32*sizeof (*(sp))/sizeof (uint32_t)) ? \
	    (((uint32_t *)(sp))[((flag)-1)/32] &= ~(1U<<(((flag)-1)%32))) : 0))

/* query: != 0 iff flag is turned on in set */
#define	prismember(sp, flag) \
	(((unsigned)((flag)-1) < 32*sizeof (*(sp))/sizeof (uint32_t)) && \
	    (((uint32_t *)(sp))[((flag)-1)/32] & (1U<<(((flag)-1)%32))))

#endif	/* !defined(_KERNEL) && _STRUCTURED_PROC == 0 */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PROCFS_H */
