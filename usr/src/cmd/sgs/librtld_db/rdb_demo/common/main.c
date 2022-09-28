
/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided this notice is preserved and that due credit is given
 * to Sun Microsystems, Inc.  The name of Sun Microsystems, Inc. may
 * not be used to endorse or promote products derived from this
 * software without specific prior written permission.  This software
 * is provided ``as is'' without express or implied warranty.
 */
#pragma ident	"@(#)main.c	1.4	96/09/10 SMI"


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <sys/procfs.h>
#include <sys/auxv.h>
#include <libelf.h>
#include <sys/param.h>
#include <stdarg.h>

#include <proc_service.h>

#include "rdb.h"
#include "disasm.h"
#include "gram.h"

char *	aux_str[] = {
	"AT_NULL",
	"AT_IGNORE",
	"AT_EXECFD",
	"AT_PHDR",
	"AT_PHENT",
	"AT_PHNUM",
	"AT_PAGESZ",
	"AT_BASE",
	"AT_FLAGS",
	"AT_ENTRY",
	0
};

char *	sun_aux_str[] = {
	"AT_SUN_UID",
	"AT_SUN_RUID",
	"AT_SUN_GID",
	"AT_SUN_RGID",
	"AT_SUN_LDELF",
	"AT_SUN_LDSHDR",
	"AT_SUN_LDNAME",
	"AT_SUN_LPAGESZ",
	"AT_SUN_PLATFORM",
	"AT_SUN_HWCAP",
	0
};


void
display_status(int pfd)
{
	prstatus_t	pstat;

	if (ioctl(pfd, PIOCSTATUS, &pstat) != 0)
		perr("display_status: PIOCSTATUS");
	printf("pid: %d pc: 0x%x instr: 0x%x\n",
		pstat.pr_pid, pstat.pr_reg[R_PC], pstat.pr_instr);
}


ulong_t
get_ldbase(int pfd)
{
	int		auxnum;
	auxv_t *	auxvp;
	int		i;

	if (ioctl(pfd, PIOCNAUXV, &auxnum) != 0)
		perr("display_auxv: PIOCNAUXV");

	if (auxnum < 1) {
		printf("PIOCNAUXV returned: %d\n", auxnum);
		return ((ulong_t) -1);
	}

	auxvp = (auxv_t *)malloc(sizeof (auxv_t) * auxnum);

	if (ioctl(pfd, PIOCAUXV, auxvp) != 0) {
		free(auxvp);
		printf("PIOCAUXV failed\n");
		return ((ulong_t) -1);
	}

	for (i = 0; i < auxnum; i++) {
		if (auxvp[i].a_type == AT_BASE) {
			ulong_t		rc;
			rc = (ulong_t)auxvp[i].a_un.a_ptr;
			free(auxvp);
			return (rc);
		}
	}

	free(auxvp);
	return ((ulong_t)-1);
}


void
display_psinfo(int pfd)
{
	prpsinfo_t	prinfo;

	if (ioctl(pfd, PIOCPSINFO, (int)&prinfo) != 0)
		perr("PIOCPSINFO");

	printf("info about process:\n");
	printf("pid: %d addr: 0x%x size: 0x%x st: %c name: %s\n",
		prinfo.pr_pid, prinfo.pr_addr, prinfo.pr_size,
		prinfo.pr_sname, prinfo.pr_fname);
}

void
display_auxv(int pfd)
{
	int		auxnum;
	auxv_t *	auxvp;
	int		i;
	if (ioctl(pfd, PIOCNAUXV, &auxnum) != 0)
		perr("display_auxv: PIOCNAUXV");

	if (auxnum < 1) {
		printf("PIOCNAUXV returned: %d\n", auxnum);
		return;
	}

	auxvp = (auxv_t *)malloc(sizeof (auxv_t) * (auxnum + 1));

	if (ioctl(pfd, PIOCAUXV, auxvp) != 0) {
		free(auxvp);
		printf("PIOCAUXV failed\n");
		return;
	}
	printf("\nAUX Entries - %d\n", auxnum);
	for (i = 0; i < auxnum; i++) {
		char *	atstr;
		int	a_type;
		a_type = auxvp[i].a_type;
		if (a_type < 2000)
			atstr = aux_str[a_type];
		else
			atstr = sun_aux_str[a_type - 2000];
		printf("auxv[%d] type: %s(%d) ptr: 0x%x\n",
			i, atstr, a_type, auxvp[i].a_un.a_ptr);
	}
	free(auxvp);
}


retc_t
find_interp(int pfd, char * buf, int bufsiz)
{
	int		efd;
	Elf *		elf;
	Elf32_Ehdr *	ehdr;
	Elf_Scn *	scn = 0;
	Elf32_Shdr *	shdr;
	Elf_Data *	dp;

	if ((efd = ioctl(pfd, PIOCOPENM, 0)) == -1)
		perr("find_interp: PIOCOPENM");

	if ((elf = elf_begin(efd, ELF_C_READ, 0)) == NULL) {
		close(efd);
		fprintf(stderr, "find_interp: elf_begin: %s\n",
			elf_errmsg(0));
		return (RET_FAILED);
	}
	if ((ehdr = elf32_getehdr(elf)) == NULL) {
		fprintf(stderr, "find_interp: elf32_getehdr: %s\n",
			elf_errmsg(0));
		elf_end(elf);
		close(efd);
		return (RET_FAILED);
	}

	while (scn = elf_nextscn(elf, scn)) {
		char *	sname;
		shdr = elf32_getshdr(scn);
		sname = elf_strptr(elf, ehdr->e_shstrndx, shdr->sh_name);
		if (strcmp(sname, INTERPSECT) == 0)
			break;
	}

	if (scn == 0) {
		/*
		 * no .interp section - must be a static executable.
		 */
		elf_end(elf);
		close(efd);
		return (RET_FAILED);
	}

	dp = elf_getdata(scn, 0);

	if (strlen(dp->d_buf) > bufsiz) {
		fprintf(stderr, "find_interp: buffer to small\n");
		return (RET_FAILED);
	}

	(void) strncpy(buf, dp->d_buf, bufsiz);

	elf_end(elf);
	close(efd);
	return (RET_OK);
}

void
init_proc()
{
	int		pfd;
	int		pid;
	char		procname[MAXPATHLEN];
	sigset_t	sigset;
	fltset_t	fltset;
	sysset_t	sysset;
	long		pflags;

	/*
	 * open our own /proc file and set tracing flags
	 */
	pid = getpid();
	(void) sprintf(procname, "/proc/%ld", pid);
	if ((pfd = open(procname, O_RDWR)) < 0) {
		(void) fprintf(stderr, "can't open %s\n",
			procname);
		exit(1);
	}

	/*
	 * inherit on fork
	 */
	pflags = PR_FORK;
	if (ioctl(pfd, PIOCSET, &pflags) != 0)
		perr("init_proc(): PIOCSET");

	/*
	 * no signal tracing
	 */
	premptyset(&sigset);
	if (ioctl(pfd, PIOCSTRACE, (int)&sigset) != 0)
		perr("PIOCSTRACE");

	/*
	 * no fault tracing
	 */
	premptyset(&fltset);
	if (ioctl(pfd, PIOCSFAULT, (int)&fltset) != 0)
		perr("PIOCSFAULT");

	/*
	 * no syscall tracing
	 */
	premptyset(&sysset);
	if (ioctl(pfd, PIOCSENTRY, (int)&sysset) != 0)
		perr("PIOCSENTRY");

	/*
	 * except exit from exec() or execve()
	 */
	premptyset(&sysset);
	praddset(&sysset, SYS_exec);
	praddset(&sysset, SYS_execve);
	if (ioctl(pfd, PIOCSEXIT, (int)&sysset) != 0)
		perr("PIOCSEXIT");

	(void) close(pfd);

}

retc_t
run_til_system(int pfd)
{
	sysset_t	sysset;
	prstatus_t	prstatus;

	prfillset(&sysset);
	if (ioctl(pfd, PIOCSEXIT, (int)&sysset) != 0) {
		perr("rts: PIOCSEXIT");
	}

	if (ioctl(pfd, PIOCRUN, 0) != 0) {
		perr("rtd: PIOCRUN");
	}
	if ((ioctl(pfd, PIOCWSTOP, &prstatus)) != 0)
		perr("PIOCWSTOP stepping");

	if (prstatus.pr_why != PR_SYSEXIT) {
		fprintf(stderr, "rts: stoped for unexpected reason: 0x%x\n",
			prstatus.pr_why);
		return (RET_FAILED);
	}
	printf("rts: SYSEXIT: pr_what: %d\n", prstatus.pr_what);
	premptyset(&sysset);

	if (ioctl(pfd, PIOCSEXIT, (int)&sysset) != 0) {
		perr("rts2: PIOCSEXIT");
	}

	return (RET_OK);
}


main(argc, argv)
int argc;
char *argv[];
{
	int			pfd;
	char			procname[16];
	char *			command;
	char *			rdb_commands = 0;
	pid_t			cpid;
	prstatus_t		prstatus;
	sysset_t		sysset;
	int			c;
	int			error = 0;
	extern FILE *		yyin;

	command = argv[0];

	while ((c = getopt(argc, argv, "f:")) != EOF)
		switch (c) {
		case 'f':
			rdb_commands = optarg;
			break;
		case '?':
			break;
		}

	if (error || (optind == argc)) {
		printf("usage: %s [-f file] executable "
			"[executable arguements ...]\n", command);
		printf("\t-f	command file\n");
		exit(1);
	}

	/*
	 * set up for tracing the child.
	 */
	init_proc();


	/*
	 * create a child to fork and exec from.
	 */
	if ((cpid = fork()) == 0) {

		(void) execvp(argv[optind], &argv[optind]);
		perr(argv[1]);
	}


	if (cpid == -1)	/* fork() failure */
		perr(command);

	/*
	 * initialize libelf
	 */
	if (elf_version(EV_CURRENT) == EV_NONE) {
		fprintf(stderr, "elf_version() failed: %s\n", elf_errmsg(0));
		exit(1);
	}

	/*
	 * initialize librtld_db
	 */
	rd_init(RD_VERSION);

	/*
	 * Child should now be waiting after the successful
	 * exec.
	 */
	(void) sprintf(procname, "/proc/%ld", cpid);
	printf("parent: %d child: %d child procname: %s\n", getpid(),
		cpid, procname);
	if ((pfd = open(procname, O_RDWR)) < 0) {
		(void) fprintf(stderr, "%s: can't open child %s\n",
			command, procname);
		exit(1);
	}

	/*
	 * wait for child process.
	 */
	if (ioctl(pfd, PIOCWSTOP, (int)&prstatus) != 0)
		perr("PIOCWSTOP");

	/*
	 * Make sure that it stopped where we expected.
	 */
	while ((prstatus.pr_why == PR_SYSEXIT) &&
	    ((prstatus.pr_what == SYS_exec) ||
	    (prstatus.pr_what == SYS_execve))) {
		if (!(prstatus.pr_reg[R_PS] & ERRBIT)) {
			/* sucessefull exec(2) */
			break;
		}
		if (ioctl(pfd, PIOCRUN, 0) != 0)
			perr("PIOCRUN1");
		if (ioctl(pfd, PIOCWSTOP, (int)&prstatus) != 0)
			perr("PIOCWSTOP");
	}

	premptyset(&sysset);
	if (ioctl(pfd, PIOCSEXIT, (int)&sysset) != 0)
		perr("PIOCSEXIT");

	/*
	 * Did we stop where we expected ?
	 */
	if ((prstatus.pr_why != PR_SYSEXIT) ||
	    ((prstatus.pr_what != SYS_exec) &&
	    (prstatus.pr_what != SYS_execve))) {
		fprintf(stderr, "Didn't catch the exec, why: %d what: %d\n",
			prstatus.pr_why, prstatus.pr_what);
		if (ioctl(pfd, PIOCRUN, 0) != 0)
			perr("PIOCRUN");
		exit(1);
	}

	ps_init(pfd, &proch);

	if (rdb_commands) {
		if ((yyin = fopen(rdb_commands, "r")) == NULL) {
			printf("unable to open %s for input\n", rdb_commands);
			perr("fopen");
		}
	} else {
		proch.pp_flags |= FLG_PP_PROMPT;
		rdb_prompt();
	}
	yyparse();

	if (proch.pp_flags & FLG_PP_PACT) {
		printf("\ncontinueing the hung process...\n");
		pfd = proch.pp_fd;
		(void) ps_close(&proch);
		if (ioctl(pfd, PIOCRUN, 0) != 0)
			perr("PIOCRUN");
		close(pfd);
	}

	return (0);
}
