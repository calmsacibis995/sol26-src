/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)gcore.c	1.13	96/06/18 SMI"	/* SVr4.0 1.1	*/

/*
 * ******************************************************************
 *
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	(c) 1986, 1987, 1988, 1989  Sun Microsystems,  Inc
 *	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *	          All rights reserved.
 * *******************************************************************
 */

#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/sysmacros.h>
#include <procfs.h>
#include <sys/elf.h>
#include <sys/mman.h>
#include <sys/utsname.h>
#include <sys/systeminfo.h>
#include <sys/auxv.h>
#include "elf_notes.h"

/* Error returns from Pgrab() */
#define	G_NOPROC	(-1)	/* No such process */
#define	G_ZOMB		(-2)	/* Zombie process */
#define	G_PERM		(-3)	/* No permission */
#define	G_BUSY		(-4)	/* Another process has control */
#define	G_SYS		(-5)	/* System process */
#define	G_SELF		(-6)	/* Process is self */
#define	G_STRANGE	(-7)	/* Unanticipated error, perror() was called */
#define	G_INTR		(-8)	/* Interrupt received while grabbing */

static	pid_t	getproc(char *path, char **pdirp);
static	int	dumpcore(char *pdir, pid_t, int ctlfd, int statfd, int asfd);
static	int	grabit(char *dir, pid_t pid, int *, int *, int *);
static	int	isprocdir(char *dir);
static	int	Pgrab(char *pdir, pid_t pid, int *, int *, int *);

static	char	*command = NULL;	/* name of command ("gcore") */
static	char	*filename = "core";	/* default filename prefix */
static	char	*procdir = "/proc";	/* default PROC directory */
static	long	buf[8*1024];	/* big buffer, used for almost everything */

main(argc, argv)
	int argc;
	char **argv;
{
	int retc = 0;
	int opt;
	int errflg = FALSE;

	command = argv[0];

	/* options */
	while ((opt = getopt(argc, argv, "o:p:")) != EOF) {
		switch (opt) {
		case 'o':		/* filename prefix (default "core") */
			filename = optarg;
			break;
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
		(void) fprintf(stderr,
			"usage:\t%s [-o filename] pid ...\n",
			command);
		exit(2);
	}

	if (!isprocdir(procdir)) {
		(void) fprintf(stderr,
			"%s: %s is not a PROC directory\n",
			command, procdir);
		exit(2);
	}

	while (--argc >= 0) {
		char *pdir;
		pid_t pid;
		int ctlfd;
		int statfd;
		int asfd;

		/* get the specified pid and its /proc directory */
		pid = getproc(*argv++, &pdir);

		if (pid < 0 || grabit(pdir, pid, &ctlfd, &statfd, &asfd) < 0) {
			retc++;
			continue;
		}

		if (dumpcore(pdir, pid, ctlfd, statfd, asfd) != 0)
			retc++;

		(void) close(asfd);
		(void) close(statfd);
		(void) close(ctlfd);
	}

	return (retc);
}

/*
 * get process id and /proc directory.
 * return pid on success, -1 on failure.
 */
static pid_t
getproc(register char *path,	/* number or /proc/nnn */
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
grabit(char *dir, pid_t pid, int *ctlp, int *statp, int *asp)
{
	int gcode;

	gcode = Pgrab(dir, pid, ctlp, statp, asp);

	if (gcode >= 0)
		return (gcode);

	if (gcode == G_INTR)
		return (-1);

	(void) fprintf(stderr, "%s: %s.%ld not dumped", command, filename, pid);
	switch (gcode) {
	case G_NOPROC:
		(void) fprintf(stderr, ": %ld: No such process", pid);
		break;
	case G_ZOMB:
		(void) fprintf(stderr, ": %ld: Zombie process", pid);
		break;
	case G_PERM:
		(void) fprintf(stderr, ": %ld: Permission denied", pid);
		break;
	case G_BUSY:
		(void) fprintf(stderr, ": %ld: Process is traced", pid);
		break;
	case G_SYS:
		(void) fprintf(stderr, ": %ld: System process", pid);
		break;
	case G_SELF:
		(void) fprintf(stderr, ": %ld: Cannot dump self", pid);
		break;
	}
	(void) fputc('\n', stderr);

	return (-1);
}

/* ARGSUSED */
static int
dumpcore(char *pdir,		/* "/proc" or another /proc directory */
	pid_t pid,		/* process-id */
	int ctlfd,		/* process file descriptors */
	int statfd,
	int asfd)
{
	int dfd = -1;			/* dump file descriptor */
	int mfd = -1;			/* /proc/pid/map */
	int nsegments;			/* current number of segments */
	Elf32_Ehdr ehdr;		/* ELF header */
	Elf32_Phdr *v = NULL;		/* ELF program header */
	prmap_t *pdp = NULL;
	int nlwp;
	struct stat statb;
	pstatus_t pstatus;
	ulong hdrsz;
	off_t poffset;
	int nhdrs, i;
	int size, count, ncount;
	char fname[MAXPATHLEN];

	/*
	 * Fetch the memory map and look for text, data, and stack.
	 */
	(void) sprintf(fname, "%s/%ld/map", pdir, pid);
	if ((mfd = open(fname, O_RDONLY)) < 0) {
		perror("dumpcore(): open() map");
		goto bad;
	}
	if (fstat(mfd, &statb) != 0 ||
	    (nsegments = statb.st_size / sizeof (prmap_t)) <= 0) {
		perror("dumpcore(): stat() map");
		goto bad;
	}
	if ((pdp = malloc((nsegments+1)*sizeof (prmap_t))) == NULL)
		goto nomalloc;
	if (read(mfd, (char *)pdp, (nsegments+1)*sizeof (prmap_t))
	    != nsegments*sizeof (prmap_t)) {
		perror("dumpcore(): read map");
		goto bad;
	}
	(void) close(mfd);
	mfd = -1;

	if (pread(statfd, (char *)&pstatus, sizeof (pstatus), 0L)
	    != sizeof (pstatus)) {
		perror("dumpcore(): read status");
		goto bad;
	}
	if ((nlwp = pstatus.pr_nlwp) == 0)
		nlwp = 1;

	nhdrs = nsegments + 2;		/* two PT_NOTE headers */
	hdrsz = nhdrs * sizeof (Elf32_Phdr);

	if ((v = malloc(hdrsz)) == NULL)
		goto nomalloc;
	(void) memset(v, 0, hdrsz);

	(void) memset(&ehdr, 0, sizeof (Elf32_Ehdr));
	ehdr.e_ident[EI_MAG0] = ELFMAG0;
	ehdr.e_ident[EI_MAG1] = ELFMAG1;
	ehdr.e_ident[EI_MAG2] = ELFMAG2;
	ehdr.e_ident[EI_MAG3] = ELFMAG3;
	ehdr.e_ident[EI_CLASS] = ELFCLASS32;
#if defined(sparc)
	ehdr.e_ident[EI_DATA] = ELFDATA2MSB;
#elif defined(i386)
	ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
#endif
	ehdr.e_type = ET_CORE;
#if defined(sparc)
	ehdr.e_machine = EM_SPARC;
#elif defined(i386)
	ehdr.e_machine = EM_386;
#endif
	ehdr.e_version = EV_CURRENT;
	ehdr.e_phoff = sizeof (Elf32_Ehdr);
	ehdr.e_ehsize = sizeof (Elf32_Ehdr);
	ehdr.e_phentsize = sizeof (Elf32_Phdr);
	ehdr.e_phnum = (Elf32_Half)nhdrs;

	/*
	 * Create the core dump file.
	 */
	(void) sprintf(fname, "%s.%ld", filename, pid);
	if ((dfd = creat(fname, 0666)) < 0) {
		perror(fname);
		goto bad;
	}

	if (write(dfd, &ehdr, sizeof (Elf32_Ehdr)) != sizeof (Elf32_Ehdr)) {
		perror("dumpcore(): write");
		goto bad;
	}

	poffset = sizeof (Elf32_Ehdr) + hdrsz;

	if (setup_old_note_header(&v[0], nlwp, pdir, pid) != 0)
		goto bad;
	v[0].p_offset = poffset;
	poffset += v[0].p_filesz;

	if (setup_note_header(&v[1], nlwp, pdir, pid) != 0) {
		cancel_old_notes();
		goto bad;
	}
	v[1].p_offset = poffset;
	poffset += v[1].p_filesz;

	for (i = 2; i < nhdrs; i++, pdp++) {
		v[i].p_type = PT_LOAD;
		v[i].p_vaddr = (Elf32_Word) pdp->pr_vaddr;
		size = pdp->pr_size;
		v[i].p_memsz = size;
		if (pdp->pr_mflags & MA_WRITE)
			v[i].p_flags |= PF_W;
		if (pdp->pr_mflags & MA_READ)
			v[i].p_flags |= PF_R;
		if (pdp->pr_mflags & MA_EXEC)
			v[i].p_flags |= PF_X;
		if ((pdp->pr_mflags & MA_READ) &&
		    (pdp->pr_mflags & (MA_WRITE|MA_EXEC)) != MA_EXEC &&
		    !(pdp->pr_mflags & MA_SHARED)) {
			v[i].p_offset = poffset;
			v[i].p_filesz = size;
			poffset += size;
		}
	}

	if (write(dfd, v, hdrsz) != hdrsz) {
		perror("dumpcore(): write");
		cancel_notes();
		cancel_old_notes();
		goto bad;
	}

	if (write_old_elfnotes(nlwp, dfd) != 0) {
		cancel_notes();
		goto bad;
	}

	if (write_elfnotes(nlwp, dfd) != 0)
		goto bad;

	/*
	 * Dump data and stack
	 */
	for (i = 2; i < nhdrs; i++) {
		if (v[i].p_filesz == 0)
			continue;
		(void) lseek(asfd, v[i].p_vaddr, 0);
		count = v[i].p_filesz;
		while (count > 0) {
			ncount = (count < sizeof (buf)) ?
					count : sizeof (buf);
			if ((ncount = read(asfd, buf, ncount)) <= 0)
				break;
			(void) write(dfd, buf, ncount);
			count -= ncount;
		}
	}

	(void) fprintf(stderr, "%s: %s.%ld dumped\n", command, filename, pid);
	(void) close(dfd);
	free((char *)v);
	return (0);
nomalloc:
	(void) fprintf(stderr, "gcore: malloc() failed\n");
bad:
	if (mfd >= 0)
		(void) close(mfd);
	if (dfd >= 0)
		(void) close(dfd);
	if (pdp != NULL)
		free(pdp);
	if (v != NULL)
		free(v);
	return (-1);
}


void
elfnote(int dfd, int type, char *ptr, int size)
{
	Elf32_Note note;		/* ELF note */

	(void) memset(&note, 0, sizeof (Elf32_Note));
	(void) memcpy(note.name, "CORE", 4);
	note.nhdr.n_type = type;
	note.nhdr.n_namesz = 5;
	note.nhdr.n_descsz = roundup(size, sizeof (Elf32_Word));
	(void) write(dfd, &note, sizeof (Elf32_Note));
	(void) write(dfd, ptr, roundup(size, sizeof (Elf32_Word)));
}

static int
isprocdir(char *dir)	/* return TRUE iff dir is a PROC directory */
{
	/*
	 * This is based on the fact that "/proc/<n>" and "/proc/0<n>"
	 * are the same file, namely process <n>.
	 */

	struct stat stat1;	/* dir/<pid>  */
	struct stat stat2;	/* dir/0<pid> */
	char *path = (char *)&buf[0];
	register char *p;
	pid_t pid;

	/* make a copy of the directory name without trailing '/'s */
	if (dir == NULL)
		(void) strcpy(path, ".");
	else {
		(void) strcpy(path, dir);
		p = path + strlen(path);
		while (p > path && *--p == '/')
			*p = '\0';
		if (*path == '\0')
			(void) strcpy(path, ".");
	}

	pid = getpid();

	/* append "/<pid>" to the directory path and lstat() the file */
	p = path + strlen(path);
	(void) sprintf(p, "/%ld", pid);
	if (lstat(path, &stat1) != 0)
		return (FALSE);

	/* append "/0<pid>" to the directory path and lstat() the file */
	(void) sprintf(p, "/0%ld", pid);
	if (lstat(path, &stat2) != 0)
		return (FALSE);

	/* see if we ended up with the same file */
	if (stat1.st_dev   != stat2.st_dev ||
	    stat1.st_ino   != stat2.st_ino ||
	    stat1.st_mode  != stat2.st_mode ||
	    stat1.st_nlink != stat2.st_nlink ||
	    stat1.st_uid   != stat2.st_uid ||
	    stat1.st_gid   != stat2.st_gid ||
	    stat1.st_size  != stat2.st_size)
		return (FALSE);

	return (TRUE);
}

/* grab existing process */
static int
Pgrab(char *pdir,			/* /proc directory */
	pid_t pid,			/* UNIX process ID */
	int *ctlp,
	int *statp,
	int *asp)
{
	int asfd = -1;
	int statfd = -1;
	int ctlfd = -1;
	long ctl[3];
	int err;
	pstatus_t pstatus;
	char *filename = (char *)&buf[0];

again:	/* Come back here if we lose it in the Window of Vulnerability */
	if (asfd >= 0)
		(void) close(asfd);
	if (statfd >= 0)
		(void) close(statfd);
	if (ctlfd >= 0)
		(void) close(ctlfd);
	asfd = -1;
	statfd = -1;
	ctlfd = -1;

	/* open the /proc/pid files */
	/* Request exclusive open to avoid grabbing someone else's	*/
	/* process and to prevent others from interfering afterwards.	*/
	if (((void)sprintf(filename, "%s/%ld/ctl", pdir, pid),
	    ctlfd = open(filename, (O_WRONLY|O_EXCL))) < 0 ||
	    ((void)sprintf(filename, "%s/%ld/status", pdir, pid),
	    statfd = open(filename, O_RDONLY)) < 0 ||
	    ((void)sprintf(filename, "%s/%ld/as", pdir, pid),
	    asfd = open(filename, O_RDONLY)) < 0) {
		err = errno;
		if (asfd >= 0)
			(void) close(asfd);
		if (statfd >= 0)
			(void) close(statfd);
		if (ctlfd >= 0)
			(void) close(ctlfd);
		asfd = -1;
		statfd = -1;
		ctlfd = -1;
		switch (err) {
		case EAGAIN:
			goto again;
		case EBUSY:
			return (G_BUSY);
		case ENOENT:
			return (G_NOPROC);
		case EACCES:
		case EPERM:
			return (G_PERM);
		default:
			errno = err;
			perror("Pgrab open()");
			return (G_STRANGE);
		}
	}

	/* ---------------------------------------------------- */
	/* We are now in the Window of Vulnerability (WoV).	*/
	/* The process may exec() a setuid/setgid or unreadable	*/
	/* object file between the open() and the PCSTOP.	*/
	/* We will get EAGAIN in this case and must start over.	*/
	/* ---------------------------------------------------- */

	/*
	 * Get the process's status.
	 */
	if (pread(statfd, (char *)&pstatus, sizeof (pstatus), 0L)
	    != sizeof (pstatus)) {
		int rc;

		if (errno == EAGAIN)	/* WoV */
			goto again;

		if (errno == ENOENT)	/* Don't complain about zombies */
			rc = G_ZOMB;
		else {
			perror("Pgrab read status");
			rc = G_STRANGE;
		}
		(void) close(asfd);
		(void) close(statfd);
		(void) close(ctlfd);
		return (rc);
	}

	/*
	 * If the process is a system process, we can't dump it.
	 */
	if (pstatus.pr_flags & PR_ISSYS) {
		(void) close(asfd);
		(void) close(statfd);
		(void) close(ctlfd);
		return (G_SYS);
	}

	/*
	 * We can't dump ourself.
	 */
	if (pid == getpid()) {
		/*
		 * Verify that the process is really ourself:
		 * Set a magic number, read it through the
		 * /proc file and see if the results match.
		 */
		long magic1 = 0;
		long magic2 = 2;

		if (lseek(asfd, (off_t)&magic1, 0) == (long)&magic1 &&
		    read(asfd, (char *)&magic2, sizeof (magic2))
		    == sizeof (magic2) &&
		    magic2 == 0 &&
		    (magic1 = 0xfeedbeef) &&
		    lseek(asfd, (off_t)&magic1, 0) == (long)&magic1 &&
		    read(asfd, (char *)&magic2, sizeof (magic2))
		    == sizeof (magic2) &&
		    magic2 == 0xfeedbeef) {
			(void) close(asfd);
			(void) close(statfd);
			(void) close(ctlfd);
			return (G_SELF);
		}
	}

	/*
	 * If the process is already stopped or has been directed
	 * to stop via /proc, there is nothing more to do.
	 */
	if (pstatus.pr_flags & (PR_ISTOP|PR_DSTOP)) {
		*ctlp = ctlfd;
		*statp = statfd;
		*asp = asfd;
		return (0);
	}

	/*
	 * Mark the process run-on-last-close so
	 * it runs even if we die from SIGKILL.
	 */
	ctl[0] = PCSET;
	ctl[1] = PR_RLC;
	if (write(ctlfd, (char *)ctl, 2*sizeof (long)) != 2*sizeof (long)) {
		int rc;

		if (errno == EAGAIN)	/* WoV */
			goto again;

		if (errno == ENOENT)	/* Don't complain about zombies */
			rc = G_ZOMB;
		else {
			perror("Pgrab PIOCSRLC");
			rc = G_STRANGE;
		}
		(void) close(asfd);
		(void) close(statfd);
		(void) close(ctlfd);
		return (rc);
	}

	/*
	 * Direct the process to stop, wait for stop, and read the status.
	 * Set a timeout to avoid waiting forever.
	 */
	ctl[0] = PCDSTOP;
	ctl[1] = PCTWSTOP;
	ctl[2] = 4000;	/* 4 seconds */
	if (write(ctlfd, (char *)ctl, 3*sizeof (long)) != 3*sizeof (long) ||
	    pread(statfd, (char *)&pstatus, sizeof (pstatus), 0L)
	    != sizeof (pstatus)) {
		int rc;

		switch (errno) {
		case EAGAIN:		/* we lost control of the the process */
			goto again;
		case EINTR:		/* user typed SIGINT */
			rc = G_INTR;
			break;
		case ENOENT:
			rc = G_ZOMB;
			break;
		default:
			perror("Pgrab PIOCSTOP");
			rc = G_STRANGE;
			break;
		}
		(void) close(asfd);
		(void) close(statfd);
		(void) close(ctlfd);
		return (rc);
	}

	/*
	 * Process should either be stopped via /proc or
	 * there should be an outstanding stop directive.
	 */
	if ((pstatus.pr_flags & (PR_ISTOP|PR_DSTOP)) == 0) {
		(void) fprintf(stderr, "Pgrab: process is not stopped\n");
		(void) close(asfd);
		(void) close(statfd);
		(void) close(ctlfd);
		return (G_STRANGE);
	}

	*ctlp = ctlfd;
	*statp = statfd;
	*asp = asfd;

	return (0);
}
