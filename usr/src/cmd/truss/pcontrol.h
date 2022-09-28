/*
 * Copyright (c) 1992 Sun Microsystems, Inc.  All Rights Reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident "@(#)pcontrol.h	1.20	96/10/19 SMI"	/* SVr4.0 1.2	*/

/* include file for process management */

/*
 * requires:
 *	<stdio.h>
 *	<signal.h>
 *	<sys/types.h>
 *	<sys/fault.h>
 *	<sys/syscall.h>
 */
#include <procfs.h>

#if sparc
/*
 * Unfortunately, the sparc ABI reserved the register type names:
 *	greg_t
 *	gregset_t
 *	fpregset_t
 * and the defined constant name:
 *	NGREG
 * with different meanings from that used by /proc.
 *
 * We define the names that truss(1) uses in terms of
 * the names defined in <sys/procfs.h>, not <sys/reg.h>.
 */
#include <sys/psw.h>	/* for PSR_C definition */
#define	NREGS	NPRGREG
#define	GREG	prgreg_t
#define	GREGSET	prgregset_t
#define	FPREGSET prfpregset_t
#define	R_CCREG	R_PSR
#define	R_RVAL0	R_O0
#define	R_RVAL1	R_O1
#define	SYSCALL	(instr_t)0x91d02008	/* syscall (ta 8) instruction */
#define	ERRBIT	PSR_C		/* bit in R_CCREG for syscall error */
typedef	u_long	syscall_t;		/* holds a syscall instruction */
#endif

#if i386
#include <sys/regset.h>
#include <sys/psw.h>	/* for PS_C definition */
#define	NREGS	NGREG
#define	GREG	greg_t
#define	GREGSET	gregset_t
#define	FPREGSET fpregset_t
#define	R_PC	EIP
#define	R_SP	UESP
#define	R_CCREG	EFL
#define	R_RVAL0	EAX
#define	R_RVAL1	EDX
#define	SYSCALL	(instr_t)0x9a	/* syscall (lcall) instruction opcode */
#define	ERRBIT	PS_C		/* bit in R_CCREG for syscall error */
typedef u_char syscall_t[7];		/* holds a syscall instruction */
#endif

#if __ppc
#include <sys/reg.h>
#include <sys/psw.h>	/* for CR0_SO definition */
#include <sys/frame.h>
#define	NREGS	NGREG
#define	GREG	greg_t
#define	GREGSET	gregset_t
#define	FPREGSET fpregset_t
#define	R_SP	R_R1
#define	R_CCREG	R_CR
#define	R_RVAL0	R_R3
#define	R_RVAL1	R_R4
#define	SYSCALL	(instr_t)0x44000002	/* syscall (sc) instruction */
#define	ERRBIT	CR0_SO		/* bit in R_CCREG for syscall error */
typedef	u_long	syscall_t;		/* holds a syscall instruction */
#endif

#define	TRUE	1
#define	FALSE	0

#define	PRWAIT		0x10000

/* definition of the process (program) table */
typedef struct {
	char	cntrl;		/* if TRUE then controlled process */
	char	child;		/* TRUE :: process created by fork() */
	char	state;		/* state of the process, see flags below */
	pid_t	pid;		/* UNIX process ID */
	int	ctlfd;		/* /proc/<pid>/ctl filedescriptor */
	int	asfd;		/* /proc/<pid>/as filedescriptor */
	int	statfd;		/* /proc/<pid>/status filedescriptor */
	int	lwpctlfd;	/* /proc/<pid>/lwp/<lwpid>/ctl */
	int	lwpstatfd;	/* /proc/<pid>/lwp/<lwpid>/status */
	long	sysaddr;	/* address of most recent syscall instruction */
	pstatus_t why;		/* from /proc -- status values when stopped */
	sigset_t sigmask;	/* signals which stop the process */
	fltset_t faultmask;	/* faults which stop the process */
	sysset_t sysentry;	/* system calls which stop process on entry */
	sysset_t sysexit;	/* system calls which stop process on exit */
	char	setsig;		/* set signal mask before continuing */
	char	sethold;	/* set signal hold mask before continuing */
	char	setfault;	/* set fault mask before continuing */
	char	setentry;	/* set sysentry mask before continuing */
	char	setexit;	/* set sysexit mask before continuing */
	char	setregs;	/* set registers before continuing */
} process_t;

/* shorthand for register array */
#define	REG	why.pr_lwp.pr_reg

/* state values */
#define	PS_NULL	0	/* no process in this table entry */
#define	PS_RUN	1	/* process running */
#define	PS_STEP	2	/* process running single stepped */
#define	PS_STOP	3	/* process stopped */
#define	PS_LOST	4	/* process lost to control (EAGAIN) */
#define	PS_DEAD	5	/* process terminated */

struct	argdes	{	/* argument descriptor for system call (Psyscall) */
	int	value;		/* value of argument given to system call */
	char	*object;	/* pointer to object in controlling process */
	char	type;		/* AT_BYVAL, AT_BYREF */
	char	inout;		/* AI_INPUT, AI_OUTPUT, AI_INOUT */
	short	len;		/* if AT_BYREF, length of object in bytes */
};

struct	sysret	{	/* return values from system call (Psyscall) */
	int	errno;		/* syscall error number */
	GREG	r0;		/* %r0 from system call */
	GREG	r1;		/* %r1 from system call */
};

/* values for type */
#define	AT_BYVAL	0
#define	AT_BYREF	1

/* values for inout */
#define	AI_INPUT	0
#define	AI_OUTPUT	1
#define	AI_INOUT	2

/* maximum number of syscall arguments */
#define	MAXARGS		8

/* maximum size in bytes of a BYREF argument */
#define	MAXARGL		(4*1024)


/* external data used by the package */
extern int debugflag;		/* for debugging */
extern char *procdir;		/* "/proc" */

/*
 * Function prototypes for routines in the process control package.
 */

extern	int	Pcreate(process_t *, char **);
extern	int	Pgrab(process_t *, pid_t, int);
extern	int	Pchoose_lwp(process_t *);
extern	void	Punchoose_lwp(process_t *);
extern	int	Preopen(process_t *);
extern	int	Prelease(process_t *);
extern	int	Phang(process_t *);
extern	int	Pwait(process_t *, unsigned);
extern	int	Pstop(process_t *, unsigned);
extern	int	Pstatus(process_t *, int, unsigned);
extern	int	Pgetsysnum(process_t *);
extern	int	Pgetsubcode(process_t *);
extern	int	Psetsysnum(process_t *, int);
extern	int	Pgetregs(process_t *);
extern	int	Pgetareg(process_t *, int);
extern	int	Pputregs(process_t *);
extern	int	Pputareg(process_t *, int);
extern	int	Psetrun(process_t *, int, int, int);
extern	int	Pstart(process_t *, int);
extern	int	Pterm(process_t *);
extern	int	Pread(process_t *, long, char *, int);
extern	int	Pwrite(process_t *, long, const char *, int);
extern	int	Pgetcred(process_t *, prcred_t *);
extern	int	Psetflags(process_t *, long);
extern	int	Presetflags(process_t *, long);
extern	int	Psignal(process_t *, int, int);
extern	int	Pfault(process_t *, int, int);
extern	int	Psysentry(process_t *, int, int);
extern	int	Psysexit(process_t *, int, int);
extern	struct sysret	Psyscall(process_t *, int, int, struct argdes *);
extern	int	is_empty(const uint32_t *, unsigned);

/*
 * Test for empty set.
 * is_empty() should not be called directly.
 */
#define	isemptyset(sp) \
	is_empty((uint32_t *)(sp), sizeof (*(sp)) / sizeof (uint32_t))
