/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)pstack.c	1.6	96/06/18 SMI"

#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <libelf.h>
#include <link.h>
#include <elf.h>

#include "../libproc/pcontrol.h"
#include "../libproc/ramdata.h"

#if defined(i386)
#define	MAX_ARGS 8	/* be generous here */
	typedef struct {
		greg_t	fr_savfp;
		greg_t	fr_savpc;
		greg_t	fr_args[MAX_ARGS];
		long	fr_argc;
		long	fr_argv;
	} frame_t;
#endif	/* i386 */

typedef struct cache {
	Elf32_Shdr	*c_shdr;
	Elf_Data	*c_data;
	char		*c_name;
} Cache;

typedef struct sym_tbl {
	Elf32_Sym	*syms;	/* start of table	*/
	char		*strs;	/* ptr to strings	*/
	int		symn;	/* number of entries	*/
} sym_tbl_t;

typedef struct map_info {
	int		mapfd;	/* file descriptor for mapping	*/
	Elf		*elf;	/* elf handle so we can close	*/
	Elf32_Ehdr	*ehdr;	/* need the header for type	*/
	sym_tbl_t	symtab;	/* symbol table			*/
	sym_tbl_t	dynsym;	/* dynamic symbol table		*/
} map_info_t;

typedef struct proc_info {
	int		num_mappings;	/* number of mappings	*/
	prmap_t		*mappings;	/* extant mappings	*/
	map_info_t	*map_info;	/* per mapping info	*/
} proc_info_t;

char processdir[100];

static proc_info_t *get_proc_info(void);
static void destroy_proc_info(proc_info_t *);
static char *find_sym_name(caddr_t addr,
		proc_info_t *pip,
		char *func_name,
		u_int *size,
		caddr_t *start);

#define	TRUE	1
#define	FALSE	0

static	void	alrm(int);
static	pid_t	getproc(char *, char **);
static	int	AllCallStacks(process_t *, pid_t, proc_info_t *);
static	void	CallStack(int, lwpstatus_t *, pid_t, proc_info_t *);
static	int	grabit(process_t *, pid_t);

#if defined(sparc)

static	void	PrintFirstFrame(prgregset_t, id_t, proc_info_t *,
			lwpstatus_t *);
static	void	PrintFrame(prgregset_t, id_t, int, proc_info_t *);
static	int	PrevFrame(int,	prgregset_t);

#elif defined(__ppc)

/* PowerPC declared below */

#elif defined(i386)

static	void	PrintFirstFrame(prgreg_t, frame_t *, id_t, int, proc_info_t *);
static	void	PrintFrame(prgreg_t, frame_t *, id_t, int, proc_info_t *);
static	int	PrevFrame(int,	frame_t *);
static	long	argcount(int, long);

#endif

main(int argc, char **argv)
{
	int retc = 0;
	int opt;
	int errflg = FALSE;
	register process_t *Pr = &Proc;

	command = strrchr(argv[0], '/');
	if (command++ == NULL)
		command = argv[0];

	/* allow all accesses for setuid version */
	(void) setuid((int)geteuid());

	/* options */
	while ((opt = getopt(argc, argv, "p:")) != EOF) {
		switch (opt) {
		case 'p':		/* alternate /proc directory */
			procdir = optarg;
			break;
		default:
			errflg = TRUE;
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (errflg || argc <= 0) {
		(void) fprintf(stderr, "usage:\t%s pid ...\n", command);
		(void) fprintf(stderr, "  (show process call stack)\n");
		exit(2);
	}

	if (!isprocdir(procdir)) {
		(void) fprintf(stderr,
			"%s: %s is not a PROC directory\n",
			command, procdir);
		exit(2);
	}

	/* catch alarms */
	(void) sigset(SIGALRM, alrm);

	while (--argc >= 0) {
		pid_t pid;
		char *pdir;
		proc_info_t *proc;

		/* get the specified pid and its /proc directory */
		pid = getproc(*argv++, &pdir);

		if (pid < 0 || grabit(Pr, pid) < 0) {
			retc++;
			continue;
		}

		(void) sprintf(processdir, "%s/%ld", pdir, pid);
		proc = get_proc_info();

		if (AllCallStacks(Pr, pid, proc) != 0)
			retc++;

		Prelease(Pr);
		destroy_proc_info(proc);
	}

	return (retc);
}

/* get process id and /proc directory */
/* return pid on success, -1 on failure */
static pid_t
getproc(char *path,		/* number or /proc/nnn */
	char **pdirp)		/* points to /proc directory on success */
{
	register char *name;
	register pid_t pid;
	char *next;

	if ((name = strrchr(path, '/')) != NULL)	/* last component */
		*name++ = '\0';
	else {
		name = path;
		path = procdir;
	}

	pid = strtol(name, &next, 10);
	if (isdigit(*name) && pid >= 0 && *next == '\0') {
		if (strcmp(procdir, path) != 0 &&
		    !isprocdir(path)) {
			(void) fprintf(stderr,
				"%s: %s is not a PROC directory\n",
				command, path);
			pid = -1;
		}
	} else {
		(void) fprintf(stderr, "%s: invalid process id: %s\n",
			command, name);
		pid = -1;
	}

	if (pid >= 0)
		*pdirp = path;
	return (pid);
}

/* take control of an existing process */
static int
grabit(process_t *Pr, pid_t pid)
{
	int gcode;
	int Fflag = 0;

	gcode = Pgrab(Pr, pid, Fflag);

	if (gcode >= 0)
		return (gcode);

	if (gcode == G_INTR)
		return (-1);

	(void) fprintf(stderr, "%s: cannot grab %ld", command, pid);
	switch (gcode) {
	case G_NOPROC:
		(void) fprintf(stderr, ": No such process");
		break;
	case G_ZOMB:
		(void) fprintf(stderr, ": Zombie process");
		break;
	case G_PERM:
		(void) fprintf(stderr, ": Permission denied");
		break;
	case G_BUSY:
		(void) fprintf(stderr, ": Process is traced");
		break;
	case G_SYS:
		(void) fprintf(stderr, ": System process");
		break;
	case G_SELF:
		(void) fprintf(stderr, ": Cannot dump self");
		break;
	}
	(void) fputc('\n', stderr);

	return (-1);
}

/*ARGSUSED*/
static void
alrm(int sig)
{
	timeout = TRUE;
}

static int
AllCallStacks(process_t *Pr, pid_t pid, proc_info_t *proc)
{
	int asfd = Pr->asfd;	/* process address space file descriptor */
	pstatus_t status;

	if (pread(Pr->statfd, &status, sizeof (status), (off_t)0)
	    != sizeof (status)) {
		perror("AllCallStacks(): read status");
		return (-1);
	}
	(void) printf("%ld:\t%.70s\n", pid, Pr->psinfo.pr_psargs);
	if (status.pr_nlwp <= 1)
		CallStack(asfd, &status.pr_lwp, 0, proc);
	else {
		char lwpdirname[100];
		lwpstatus_t lwpstatus;
		int lwpfd;
		char *lp;
		DIR *dirp;
		struct dirent *dentp;

		(void) sprintf(lwpdirname, "%s/lwp", processdir);
		if ((dirp = opendir(lwpdirname)) == NULL) {
			perror("AllCallStacks(): opendir(lwp)");
			return (-1);
		}
		lp = lwpdirname + strlen(lwpdirname);
		*lp++ = '/';

		/* for each lwp */
		while (dentp = readdir(dirp)) {
			if (dentp->d_name[0] == '.')
				continue;
			(void) strcpy(lp, dentp->d_name);
			(void) strcat(lp, "/lwpstatus");
			if ((lwpfd = open(lwpdirname, O_RDONLY)) < 0) {
				perror("AllCallStacks(): open lwpstatus");
				break;
			} else if (pread(lwpfd, &lwpstatus, sizeof (lwpstatus),
			    (off_t)0) != sizeof (lwpstatus)) {
				perror("AllCallStacks(): read lwpstatus");
				(void) close(lwpfd);
				break;
			}
			(void) close(lwpfd);
			CallStack(asfd, &lwpstatus, lwpstatus.pr_lwpid, proc);
		}
		(void) closedir(dirp);
	}
	return (0);
}

#if defined(sparc)

static void
CallStack(int asfd, lwpstatus_t *psp, id_t lwpid, proc_info_t *proc)
{
	int first = TRUE;
	prgregset_t reg;
	prgreg_t fp;
	int nfp;
	prgreg_t *prevfp;
	u_int size;
	int i;

	(void) memcpy(reg, psp->pr_reg, sizeof (reg));

	if (psp->pr_flags & PR_ASLEEP) {
		PrintFirstFrame(reg, lwpid, proc, psp);
		first = FALSE;
		reg[R_PC] = reg[R_O7];
		reg[R_nPC] = reg[R_PC] + 4;
	}

	size = 16;
	prevfp = malloc(size * sizeof (prgreg_t));
	nfp = 0;
	do {
		/* prevent stack loops from running on forever */
		fp = reg[R_FP];
		for (i = 0; i < nfp; i++) {
			if (fp == prevfp[i]) {
				free(prevfp);
				return;
			}
		}
		if (nfp == size) {
			size *= 2;
			prevfp = realloc(prevfp, size * sizeof (prgreg_t));
		}
		prevfp[nfp++] = fp;

		PrintFrame(reg, lwpid, first, proc);
		first = FALSE;
	} while (PrevFrame(asfd, reg) == 0);

	free(prevfp);
}

static void
PrintFirstFrame(prgregset_t reg, id_t lwpid, proc_info_t *proc,
	lwpstatus_t *psp)
{
	char buff[255];
	u_int size;
	caddr_t start;
	char *sname;
	int match = TRUE;
	u_int i;

	if (lwpid)
		(void) printf("lwp#%ld ----------\n", lwpid);

	sname = sysname(psp->pr_syscall);
	(void) printf(" %.8x %-8s (", reg[R_PC], sname);
	for (i = 0; i < psp->pr_nsysarg; i++) {
		if (i != 0)
			(void) printf(", ");
		(void) printf("%lx", psp->pr_sysarg[i]);
		if (psp->pr_sysarg[i] != reg[R_O0+i])
			match = FALSE;
	}
	(void) printf(")\n");

	(void) sprintf(buff, "%.8x", reg[R_PC]);
	start = (caddr_t)reg[R_PC];
	(void) strcpy(buff+8, " ????????");
	(void) find_sym_name((caddr_t)reg[R_PC], proc, buff+9, &size, &start);
	if (match) {
		register char *s = buff+9;

		while (*s == '_')
			s++;
		if (strcmp(sname, s) != 0)
			match = FALSE;
	}

	if (!match) {
		(void) printf((start != (caddr_t)reg[R_PC])?
			" %-17s (%x, %x, %x, %x, %x, %x) + %x\n" :
			" %-17s (%x, %x, %x, %x, %x, %x)\n",
			buff,
			reg[R_O0],
			reg[R_O1],
			reg[R_O2],
			reg[R_O3],
			reg[R_O4],
			reg[R_O5],
			(uintptr_t)reg[R_PC] - (uintptr_t)start);
	}
}

static void
PrintFrame(prgregset_t reg, id_t lwpid, int first, proc_info_t *proc)
{
	char buff[255];
	u_int size;
	caddr_t start;

	(void) sprintf(buff, "%.8x", reg[R_PC]);
	start = (caddr_t)reg[R_PC];
	(void) strcpy(buff+8, " ????????");
	(void) find_sym_name((caddr_t)reg[R_PC], proc, buff+9, &size, &start);

	if (lwpid && first)
		(void) printf("lwp#%ld ----------\n", lwpid);
	(void) printf((start != (caddr_t)reg[R_PC])?
		" %-17s (%x, %x, %x, %x, %x, %x) + %x\n" :
		" %-17s (%x, %x, %x, %x, %x, %x)\n",
		buff,
		reg[R_I0],
		reg[R_I1],
		reg[R_I2],
		reg[R_I3],
		reg[R_I4],
		reg[R_I5],
		(u_int)reg[R_PC] - (u_int)start);
}

static int
PrevFrame(int asfd, prgregset_t reg)
{
	off_t sp;

	if ((sp = reg[R_I6]) == 0)
		return (-1);
	reg[R_PC] = reg[R_I7];
	reg[R_nPC] = reg[R_PC] + 4;
	(void) memcpy(&reg[R_O0], &reg[R_I0], 8*sizeof (prgreg_t));
	if (pread(asfd, &reg[R_L0], 16*sizeof (prgreg_t), sp)
	    != 16*sizeof (prgreg_t))
		return (-1);
	return (0);
}

#endif	/* sparc */

#if defined(__ppc)

/*
 * PowerPC stack traceback code taken from adb.
 * XXX - lacks knowledge of floating-point arguments.
 *
 * A "stackpos" contains everything we need to know in
 * order to do a stack trace.
 */
struct stackpos {
	u_int	k_pc;		/* where we are in this proc */
	u_int	k_fp;		/* this proc's frame pointer */
	u_int	k_nargs;	/* # of args passed to the func */
	u_int	k_entry;	/* this proc's entry point */
	u_int	k_caller;	/* PC of the call that called us */
	u_int	k_flags;	/* leaf info */
	prgregset_t k_regs;	/* address of register contents */
	char	k_symname[256];	/* buffer for symbol name */
};

/*
 * Flags for k_flags:
 */
#define	K_LEAF		1	/* this is a leaf procedure */

/*
 * Useful stack frame offsets.
 */
#define	FR_SAVFP	0
#define	FR_SAVPC	4

#define	INVAL_PC	((u_long)-1)	/* unknown/invalid PC/FP value */

/*
 * Instruction decoding macros.
 */
#define	MFLR(i)		(i == ((31 << 26) | (0x100 << 11) | (339 << 1)))
#define	STW_R0(i)	(i == 0x90010004)
#define	STWU(i)		((i & 0xffff0000) == \
			(((unsigned)(37 << 10) | 0x21) << 16))
#define	STW(i)		((((i >> 26) & 0x3f) == 36) && \
			(STW_OFF(i) < 0x8000) && \
			(((i >> 16) & 0x1f) == 1))
#define	STW_FROM(i)	((i >> 21) & 0x1f)
#define	STW_OFF(i)	(i & 0xffff)
#define	MR(i)		(((i & 0xfc000000) == (unsigned)(24 << 26)) && \
			((i & 0xffff) == 0) && \
			(MR_TO(i) >= 13) && \
			(MR_FROM(i) <= 10))
#define	MR_TO(i)	((i >> 16) & 0x1f)
#define	MR_FROM(i)	((i >> 21) & 0x1f)


static uint32_t get(off_t addr, int pfd);
static void findentry(struct stackpos *);
static void get_nargs(struct stackpos *);
static void restore_nonvol_top(struct stackpos *);
static void restore_args(struct stackpos *);
static void scan_mr(struct stackpos *, u_long cur_pc,
	int do_move, u_long endpc);
static u_long restore_nonvols(struct stackpos *sp, u_long pc,
	int restore_reg, u_long endpc);
static u_long nextframe(struct stackpos *);
static void stacktop(struct stackpos *, lwpstatus_t *);

static int	cur_fd;		/* current FD for PowerPC stack trace */
static proc_info_t *cur_proc;	/* current process pointer */

#define	ISP	cur_fd		/* use current FD */
#define	DSP	cur_fd		/* use current FD */

/*
 * Get word.
 *	Returns zero if not accessible ... so this isn't general.
 */
static uint32_t
get(off_t addr, int pfd)
{
	long buf;

	if (pread(pfd, &buf, sizeof (buf), addr) != sizeof (buf))
		return (0);
	return (buf);
}


static u_long
preamble(u_long start_pc)
{
	long inst;

	for (;;) {
		inst = get(start_pc, ISP);
		if (MFLR(inst) || STWU(inst) || STW_R0(inst))
			start_pc += 4;
		else
			break;
	}
	return (start_pc);
}

static int
use_lr(struct stackpos *sp)
{
	long inst;
	u_long start_pc = sp->k_entry;

	if (start_pc == INVAL_PC)
		return (0);
	for (;;) {
		if (start_pc == sp->k_pc)
			break;
		inst = get(start_pc, ISP);
		if (MFLR(inst) || STWU(inst) || STW_R0(inst)) {
			if (STWU(inst))
				return (0);
			start_pc += 4;
		} else
			break;
	}
	return (1);
}

/*
 * Modify k_regs based on "stw nonvol,N(sp)" reverse execution.
 * Do not reverse execute beyond endpc if endpc is non-zero.
 * Return value is the pc after all of the "stw" instructions.
 */
static u_long
restore_nonvols(struct stackpos *sp, u_long pc, int restore_reg, u_long endpc)
{
	long inst;

	for (;;) {
		if (endpc && (pc >= endpc))
			break;
		inst = get(pc, ISP);
		if (STW(inst)) {
			if (restore_reg) {
				sp->k_regs[STW_FROM(inst)] =
				    get(sp->k_fp + STW_OFF(inst), cur_fd);
			}
			pc += 4;
		} else
			break;
	}
	return (pc);
}

static void
scan_mr(struct stackpos *sp, u_long cur_pc, int do_move, u_long endpc)
{
	long inst;
	int max_arg_reg = 0;
	int tmp_reg;

	for (;;) {
		if (endpc && (cur_pc >= endpc))
			break;
		inst = get(cur_pc, ISP);
		if (!MR(inst))
			break;
		tmp_reg = MR_FROM(inst);
		if (tmp_reg >= max_arg_reg)
			max_arg_reg = tmp_reg;
		if (do_move) {
			sp->k_regs[tmp_reg] = sp->k_regs[MR_TO(inst)];
		}
		cur_pc += 4;
	}
	sp->k_nargs = (max_arg_reg >= 3) ? max_arg_reg - 2 : 0;
}

/*
 *	Assume current_regs is valid.  Analyze function preamble
 *	to restore argument registers, and determine the number
 *	of arguments (as best we can).
 */

static void
restore_args(struct stackpos *sp)
{
	u_long cur_pc;
	u_long start_pc;
	int tmp_reg;

	for (tmp_reg = R_R3; tmp_reg <= R_R10; tmp_reg++)
		sp->k_regs[tmp_reg] = 0;

	if ((start_pc = sp->k_entry) == INVAL_PC)
		return;

	start_pc = preamble(start_pc);

	/* move beyond the non-volatile register saving */
	cur_pc = restore_nonvols(sp, start_pc, 0, 0);

	/* reverse execute the move register instructions */
	(void) scan_mr(sp, cur_pc, 1, 0);

	/* restore non-volatile registers */
	(void) restore_nonvols(sp, start_pc, 1, 0);
}

/*
 * Assume k_entry is the entry point of interest.  Make no
 * assumptions about whether the stack frame has been created
 * yet.  Do not reverse execute beyond k_pc.  If this function
 * has created a stack frame, adjust k_fp to point to the next
 * stack frame.
 */
static void
restore_nonvol_top(struct stackpos *sp)
{
	u_long start_pc;
	u_long cur_pc;
	long inst;
	int adjust_fp = 0;

	/* find function entry */
	start_pc = sp->k_entry;
	if (start_pc == INVAL_PC)
		return;

	for (;;) {
		if (start_pc == sp->k_pc)
			goto done;
		inst = get(start_pc, ISP);
		if (MFLR(inst) || STWU(inst) || STW_R0(inst)) {
			if (STWU(inst))
				adjust_fp = 1;
			start_pc += 4;
		} else
			break;
	}
	if (start_pc == sp->k_pc)
		goto done;

	/* move beyond the non-volatile register saving */
	cur_pc = restore_nonvols(sp, start_pc, 0, sp->k_pc);

	/* reverse execute the move register instructions */
	(void) scan_mr(sp, cur_pc, 1, sp->k_pc);

	/* restore the non-volatile registers */
	(void) restore_nonvols(sp, start_pc, 1, sp->k_pc);

done:
	if (adjust_fp)
		sp->k_fp = get(sp->k_fp, DSP);
}

/*
 * Assume k_entry is the entry point of interest.  Determine
 * how many arguments there might be based on "mr nonvol,argreg"
 * instructions found in the function preamble.
 */
static void
get_nargs(struct stackpos *sp)
{
	u_long start_pc;

	sp->k_nargs = 0;
	if (sp->k_entry == INVAL_PC)
		return;
	start_pc = preamble(sp->k_entry);

	/* move pass the non-volatile register saving */
	start_pc = restore_nonvols(sp, start_pc, 0, 0);
	scan_mr(sp, start_pc, 0, 0);
}

/*
 * stacktop collects information about the topmost (most recently
 * called) stack frame into its (struct stackpos) argument.
 */
static void
stacktop(register struct stackpos *sp, lwpstatus_t *psp)
{
	sp->k_pc = psp->pr_reg[R_PC];
	sp->k_fp = psp->pr_reg[R_SP];
	sp->k_flags = K_LEAF;
	sp->k_nargs = 0;
	(void) memcpy((void *)sp->k_regs, (void *)psp->pr_reg,
		sizeof (sp->k_regs));
	(void) findentry(sp);
	restore_nonvol_top(sp);
	get_nargs(sp);
}

/*
 * set the k_entry field in the stackpos structure, based on k_pc.
 */
static void
findentry(register struct stackpos *sp)
{
	u_int	size;		/* dummy location for saving routine size */

	if (find_sym_name((caddr_t)sp->k_pc, cur_proc,
	    sp->k_symname, &size, (caddr_t *)&sp->k_entry) == NULL) {
		sp->k_entry = INVAL_PC;
		sp->k_symname[0] = '\0';
	}
}

/*
 * nextframe replaces the info in sp with the info appropriate
 * for the next frame "up" the stack (the current routine's caller).
 */
static u_long
nextframe(register struct stackpos *sp)
{
	/*
	 * Find our entry point. Then find out
	 * which registers we saved, and map them.
	 * Our entry point is the address our caller called.
	 */
	/*
	 * find caller's pc and fp
	 */
	if (sp->k_flags && use_lr(sp)) {
		sp->k_pc = sp->k_regs[R_LR];
	} else {
		sp->k_pc = get(sp->k_fp + FR_SAVPC, DSP);
	}
	if (sp->k_pc == 0) {
		sp->k_fp = 0;
		return (0);
	}

	/*
	 * now that we have assumed the identity of our caller, find
	 * how many longwords of argument WE were called with.
	 */
	sp->k_flags = 0;
	(void) findentry(sp);
	restore_args(sp);

	sp->k_fp = get(sp->k_fp + FR_SAVFP, DSP);

	if ((sp->k_fp & 0x3) != 0)
		sp->k_fp = 0;

	return (sp->k_fp);
}

static void
PrintFrame(struct stackpos *sp, id_t lwpid, int first)
{
	int	i;

	if (lwpid && first)
		(void) printf("lwp#%ld ----------\n", lwpid);

	(void) printf(" %.8x ", sp->k_pc);
	if (sp->k_symname[0] != '\0') {
		(void) printf("%-8s (", sp->k_symname);
	} else {
		(void) printf("???????? (");
	}
	for (i = R_R3; i < R_R3 + sp->k_nargs; i++) {
		if (i > R_R3) {
			(void) printf(", ");
		}
		(void) printf("%x", sp->k_regs[i]);
	}
	if (sp->k_entry != sp->k_pc) {
		(void) printf(") + %x\n", sp->k_pc - sp->k_entry);
	} else {
		(void) printf(")\n");		/* not likely */
	}
}

/* ARGSUSED */
static void
PrintFirstFrame(struct stackpos *sp, id_t lwpid, proc_info_t *proc,
	lwpstatus_t *psp)
{
	char *sname;
	int match = TRUE;
	int i;

	if (lwpid)
		(void) printf("lwp#%ld ----------\n", lwpid);

	sname = sysname(psp->pr_syscall);
	(void) printf(" %.8x %-8s (", sp->k_pc, sname);
	for (i = 0; i < psp->pr_nsysarg; i++) {
		if (i != 0)
			(void) printf(", ");
		(void) printf("%lx", psp->pr_sysarg[i]);
		if (psp->pr_sysarg[i] != sp->k_regs[R_R3+i])
			match = FALSE;
	}
	(void) printf(")\n");

	/*
	 * If all the arguments matched, see if the system call matches
	 * the name of the routine we were in, after skipping leading
	 * underscores.
	 */
	if (match) {
		register char *s = sp->k_symname;

		while (*s == '_')
			s++;
		if (strcmp(sname, s) != 0)
			match = FALSE;
	}

	/*
	 * If not the same as the system call, print the frame.
	 */
	if (!match) {
		PrintFrame(sp, lwpid, 0);
	}
}

static void
CallStack(int asfd, lwpstatus_t *psp, id_t lwpid, proc_info_t *proc)
{
	int first = TRUE;
	struct stackpos s;

	cur_fd = asfd;
	cur_proc = proc;
	stacktop(&s, psp);

	if (psp->pr_flags & PR_ASLEEP) {
		PrintFirstFrame(&s, lwpid, proc, psp);
		first = FALSE;
		if (nextframe(&s) == 0)
			return;
	}

	do {
		PrintFrame(&s, lwpid, first);
		first = FALSE;
	} while (nextframe(&s) != 0);
}

#endif	/* __ppc */

#if defined(i386)

static void
CallStack(int asfd, lwpstatus_t *psp, id_t lwpid, proc_info_t *proc)
{
	int first = TRUE;
	prgregset_t reg;
	frame_t frame;
	prgreg_t pc;
	int i;
	prgreg_t fp;
	int nfp;
	prgreg_t *prevfp;
	u_int size;

	(void) memcpy(reg, psp->pr_reg, sizeof (reg));

	if (psp->pr_flags & PR_ASLEEP) {
		(void) memset((char *)&frame, 0, sizeof (frame));
		frame.fr_savfp = reg[R_SP];
		frame.fr_savpc = reg[R_PC];
		for (i = 0; i < psp->pr_nsysarg && i < MAX_ARGS; i++)
			frame.fr_args[i] = psp->pr_sysarg[i];
		frame.fr_argc = psp->pr_nsysarg;
		frame.fr_argv = NULL;
		PrintFirstFrame(reg[R_PC], &frame, lwpid,
			psp->pr_syscall, proc);
		first = FALSE;
		if (pread(asfd, &reg[R_PC], sizeof (prgreg_t),
		    (long)reg[R_SP]) != sizeof (prgreg_t))
			return;
		reg[R_SP] += 4;
	}

	if (pread(asfd, &frame, sizeof (frame), (long)reg[R_FP])
	    != sizeof (frame))
		return;
	frame.fr_argc = argcount(asfd, (long)frame.fr_savpc);
	frame.fr_argv = (long)reg[R_FP] + 2 * sizeof (long);

	size = 16;
	prevfp = malloc(size * sizeof (prgreg_t));
	nfp = 0;
	pc = reg[R_PC];
	do {
		/* prevent stack recursion from running on forever */
		fp = frame.fr_savfp;
		for (i = 0; i < nfp; i++) {
			if (fp == prevfp[i]) {
				free(prevfp);
				return;
			}
		}
		if (nfp == size) {
			size *= 2;
			prevfp = realloc(prevfp, size * sizeof (prgreg_t));
		}
		prevfp[nfp++] = fp;

		PrintFrame(pc, &frame, lwpid, first, proc);
		first = FALSE;
		pc = frame.fr_savpc;
	} while (PrevFrame(asfd, &frame) == 0);

	free(prevfp);
}

/* ARGSUSED */
static void
PrintFirstFrame(prgreg_t pc, frame_t *frame, id_t lwpid, int sys,
	proc_info_t *proc)
{
	char buff[255];
	int i;

	(void) sprintf(buff, "%.8x ", pc);
	(void) strcpy(buff+9, sysname(sys));

	if (lwpid)
		(void) printf("lwp#%ld ----------\n", lwpid);

	(void) printf(" %-17s (", buff);
	for (i = 0; i < frame->fr_argc && i < MAX_ARGS; i++)
		(void) printf((i+1 == frame->fr_argc)? "%x" : "%x, ",
			frame->fr_args[i]);
	(void) printf(")\n");
}

static void
PrintFrame(prgreg_t pc, frame_t *frame, id_t lwpid, int first,
	proc_info_t *proc)
{
	char buff[255];
	u_int size;
	caddr_t start;
	int i;

	(void) sprintf(buff, "%.8x ", pc);
	start = (caddr_t)pc;
	(void) strcpy(buff+9, "????????");
	(void) find_sym_name((caddr_t)pc, proc, buff+9, &size, &start);

	if (lwpid && first)
		(void) printf("lwp#%ld ----------\n", lwpid);
	(void) printf(" %-17s (", buff);
	for (i = 0; i < frame->fr_argc && i < MAX_ARGS; i++)
		(void) printf((i+1 == frame->fr_argc)? "%x" : "%x, ",
			frame->fr_args[i]);
	if (i != frame->fr_argc)
		(void) printf("...");
	(void) printf((start != (caddr_t)pc)?
		") + %x\n" : ")\n", (u_int)pc - (u_int)start);
}

static int
PrevFrame(int asfd, frame_t *frame)
{
	prgreg_t fp = frame->fr_savfp;

	if (fp == 0 ||
	    pread(asfd, frame, sizeof (*frame), (long)fp) != sizeof (*frame))
		return (-1);
	frame->fr_argc = argcount(asfd, (long)frame->fr_savpc);
	frame->fr_argv = (long)fp + 2 * sizeof (long);

	return (0);
}

/*
 * Given the return PC, return the number of arguments.
 */
static long
argcount(int asfd, long pc)
{
	unsigned char instr[6];
	int count;

	if (pread(asfd, instr, sizeof (instr), pc) != sizeof (instr) ||
	    instr[1] != 0xc4)
		return (0);

	switch (instr[0]) {
	case 0x81:	/* count is a longword */
		count = instr[2]+(instr[3]<<8)+(instr[4]<<16)+(instr[5]<<24);
		break;
	case 0x83:	/* count is a byte */
		count = instr[2];
		break;
	default:
		count = 0;
		break;
	}

	return (count / sizeof (long));
}

#endif	/* i386 */


/*
 * Routines to generate a symbolic traceback from an ELF
 * executable.  It works w/ shared libraries as well.
 * This is a first cut.
 */

/*
 * Define our own standard error routine.
 */
static void
failure(const char *name)
{
	(void) fprintf(stderr, "%s failed: %s\n", name,
	    elf_errmsg(elf_errno()));
	exit(1);
}

static void
destroy_proc_info(proc_info_t *ptr)
{
	map_info_t *mptr = ptr->map_info;
	int i;

	if (ptr) {
		for (i = ptr->num_mappings; i; i--, mptr++) {
			if (mptr->mapfd > 0)
				(void) close(mptr->mapfd);
			if (mptr->elf)
				elf_end(mptr->elf);
		}
		if (ptr->mappings)
			free(ptr->mappings);
		if (ptr->map_info)
			free(ptr->map_info);
		free(ptr);
	}
}

static proc_info_t *
get_proc_info()
{
	char mapfile[100];
	int mapfd;
	struct stat statb;
	int i, n;
	map_info_t *map_info, *mptr;
	proc_info_t *ptr;

	(void) sprintf(mapfile, "%s/map", processdir);
	if ((mapfd = open(mapfile, O_RDONLY)) < 0) {
		perror("cannot open() map file");
		return (NULL);
	}

	ptr = malloc(sizeof (*ptr));
	for (;;) {
		if (fstat(mapfd, &statb) != 0 ||
		    statb.st_size < sizeof (prmap_t)) {
			(void) close(mapfd);
			free(ptr);
			perror("cannot stat() map file");
			return (NULL);
		}
		n = statb.st_size / sizeof (prmap_t);
		ptr->num_mappings = n;
		ptr->mappings = (prmap_t *) malloc(sizeof (prmap_t) * (n+1));
		if ((n = pread(mapfd, ptr->mappings,
		    (n+1)*sizeof (prmap_t), 0L)) < sizeof (prmap_t)) {
			(void) close(mapfd);
			free(ptr->mappings);
			free(ptr);
			perror("cannot read() map file");
			return (NULL);
		}
		n /= sizeof (prmap_t);
		if (ptr->num_mappings >= n) {	/* we got all of the mappings */
			ptr->num_mappings = n;
			break;
		}
		/* loop around and try it again */
		free(ptr->mappings);
	}

	/* zero out the mapping following the last valid one */
	(void) memset((char *)&ptr->mappings[ptr->num_mappings],
		0, sizeof (prmap_t));

	ptr->map_info = mptr = map_info =
		(map_info_t *) malloc(sizeof (map_info_t) * (n+1));

	(void) memset(mptr, 0, sizeof (map_info_t) * (n+1));

	for (i = 0; i < n; i++) {
		mptr = &map_info[i];
		mptr->mapfd = 0;
	}

	return (ptr);
}

static int
build_sym_tab(proc_info_t *ptr, int map_index)
{
	char objectfile[100];
	map_info_t	*mptr = ptr->map_info + map_index;
	prmap_t		*pptr = ptr->mappings + map_index;
	Elf32_Shdr	*shdr;
	Elf		*elf;
	Elf_Scn		*scn;
	Elf32_Ehdr	*ehdr;
	Elf_Data	*data;
	sym_tbl_t	*which;
	Cache		*cache;
	Cache		*_cache;
	char		*names;
	u_int		cnt;

	(void) elf_version(EV_CURRENT);

	if (mptr->mapfd < 0)	/* failed here before */
		return (-1);

	(void) sprintf(objectfile, "%s/object/%s", processdir,
		pptr->pr_mapname);
	if ((mptr->mapfd = open(objectfile, O_RDONLY)) < 0) {
#ifdef DEBUG
		perror(objectfile);
		fprintf(stderr, "Cannot get fd for following mapping:\n");
		fprintf(stderr, "\tpr_vaddr    = 0x%x\n", pptr->pr_vaddr);
		fprintf(stderr, "\tpr_size     = %d\n", pptr->pr_size);
		fprintf(stderr, "\tpr_pagesize = %d\n", pptr->pr_pagesize);
		fprintf(stderr, "\tpr_offset   = %lld\n", pptr->pr_offset);
#endif
		return (-1);
	}

	if ((elf = elf_begin(mptr->mapfd, ELF_C_READ, NULL)) == NULL ||
	    elf_kind(elf) != ELF_K_ELF) {
#ifdef DEBUG
		(void) fprintf(stderr, "file is not elf??\n");
#endif
		(void) close(mptr->mapfd);
		mptr->mapfd = -1;
		return (-1);
	}
	mptr->elf = elf;

	if ((mptr->ehdr = ehdr = elf32_getehdr(elf)) == NULL)
		failure("elf_getehdr");

	/*
	 * Obtain the .shstrtab data buffer to provide the required section
	 * name strings.
	 */
	if ((scn = elf_getscn(elf, ehdr->e_shstrndx)) == NULL)
		failure("elf_getscn");

	if ((data = elf_getdata(scn, NULL)) == NULL)
		failure("elf_getdata");

	names = data->d_buf;

	/*
	 * Fill in the cache descriptor with information for each section.
	 */

	cache = (Cache *)malloc(ehdr->e_shnum * sizeof (Cache));

	_cache = cache;
	_cache++;

	for (scn = NULL; scn = elf_nextscn(elf, scn); _cache++) {
		if ((_cache->c_shdr = elf32_getshdr(scn)) == NULL)
			failure("elf32_getshdr");

		if ((_cache->c_data = elf_getdata(scn, NULL)) == NULL)
			failure("elf_getdata");

		_cache->c_name = names + _cache->c_shdr->sh_name;
	}

	for (cnt = 1; cnt < ehdr->e_shnum; cnt++) {
		_cache = &cache[cnt];
		shdr = _cache->c_shdr;

		if (shdr->sh_type == SHT_SYMTAB)
			which = & mptr->symtab;
		else if (shdr->sh_type == SHT_DYNSYM)
			which = & mptr->dynsym;
		else
			continue;

		/*
		 * Determine the symbol data and number.
		 */
		which->syms = (Elf32_Sym *)_cache->c_data->d_buf;
		which->symn = shdr->sh_size / shdr->sh_entsize;
		which->strs = (char *)cache[shdr->sh_link].c_data->d_buf;
	}

	free(cache);

	return (0);
}

/*
 * Find symbol name containing address addr inside process proc.
 * Return values are:
 *	func_name,
 *	size,
 *	start.
 * Returns non-NULL func_name on success.
 */
static char *
find_sym_name(caddr_t addr, proc_info_t *proc,
	char *func_name, u_int *size, caddr_t *start)
{
	int i;
	int n = proc->num_mappings;
	u_int offset;

	prmap_t *ptr;

	for (i = 0, ptr = proc->mappings; i < n; i++, ptr++) {
		map_info_t	*mptr;
		Elf32_Sym	*syms;
		int		symn;
		char		*strs;
		int		_cnt;

		if (addr < (caddr_t)ptr->pr_vaddr ||
		    addr >= ((caddr_t)ptr->pr_vaddr + ptr->pr_size))
			continue;

		/* found it */
		mptr = proc->map_info + i;

		if (mptr->ehdr == NULL) {
			if (build_sym_tab(proc, i)) {
#ifdef DEBUG
				fprintf(stderr
			"failed to build symbol table for address 0x%x\n",
					addr);
#endif
				return (NULL);
			}
		}

		if (mptr->ehdr->e_type != ET_DYN)
			offset = 0;
		else  {
			offset = (u_int) proc->mappings[i].pr_vaddr
				- (u_int) proc->mappings[i].pr_offset;
			addr -= (u_int)offset;
		}

		syms = mptr->symtab.syms;
		symn = mptr->symtab.symn;
		strs = mptr->symtab.strs;

		for (_cnt = 0; _cnt < symn; _cnt++, syms++) {
			if (ELF32_ST_TYPE(syms->st_info) != STT_FUNC ||
			    (u_int)addr < syms->st_value ||
			    (u_int)addr >= (syms->st_value+syms->st_size))
				continue;

			(void) strcpy(func_name, (strs + syms->st_name));
			*size = syms->st_size;
			*start = (caddr_t)syms->st_value + offset;
			return (func_name);
		}

		syms = mptr->dynsym.syms;
		symn = mptr->dynsym.symn;
		strs = mptr->dynsym.strs;

		for (_cnt = 0; _cnt < symn; _cnt++, syms++) {
			if (ELF32_ST_TYPE(syms->st_info) != STT_FUNC ||
			    (u_int)addr < syms->st_value ||
			    (u_int)addr >= (syms->st_value+syms->st_size))
				continue;
			(void) strcpy(func_name, (strs + syms->st_name));
			*size = syms->st_size;
			*start = (caddr_t)syms->st_value + offset;
			return (func_name);
		}

		return (NULL);
	}

	return (NULL);
}
