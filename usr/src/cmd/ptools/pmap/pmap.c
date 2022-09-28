/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)pmap.c	1.7	96/08/09 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <limits.h>
#include <link.h>
#include <libelf.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/signal.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <procfs.h>
#include "dynlib.h"

#define	TRUE	1
#define	FALSE	0

/* obsolete flags */
#ifndef	MA_BREAK
#define	MA_BREAK	0
#endif
#ifndef	MA_STACK
#define	MA_STACK	0
#endif

static	int	look(char *);
static	int	perr(char *);
static	char	*mflags(u_int);

static	pid_t	pid;
static	char	procname[64];
static	int	reserved = FALSE;

main(int argc, char **argv)
{
	int rc = 0;
	char *cmd;

	cmd = strrchr(argv[0], '/');
	if (cmd++ == NULL)
		cmd = argv[0];

	if (argc > 1 && strcmp(argv[1], "-r") == 0) {
		reserved = TRUE;
		argc--;
		argv++;
	}

	if (argc <= 1 || *argv[1] == '-') {
		(void) fprintf(stderr, "usage:  %s [-r] pid ...\n", cmd);
		(void) fprintf(stderr, "  (report process address maps)\n");
		(void) fprintf(stderr, "  -r: report reserved addresses\n");
		return (2);
	}

	/*
	 * Some common places where objects are found.
	 * (mostly for ld.so.1)
	 */
	load_lib_dir("/usr/lib");
	load_lib_dir("/etc/lib");

	while (--argc > 0)
		rc += look(*++argv);

	return (rc);
}


static int
look(char *arg)
{
	int fd, asfd, mapfd;
	struct stat statb;
	char *pidp;
	prmap_t *prmapp = NULL;
	prmap_t *pmp;
	int nmap;
	int n;
	char *s;
	int i;
	size_t total;
	pstatus_t pstatus;
	psinfo_t psinfo;
	char pathname[100];

	if (strchr(arg, '/') != NULL)
		(void) strncpy(procname, arg, sizeof (procname));
	else {
		(void) strcpy(procname, "/proc/");
		(void) strncat(procname, arg, sizeof (procname)-6);
	}
	pidp = strrchr(procname, '/')+1;
	while (*pidp == '0' && *(pidp+1) != '\0')
		pidp++;
	pid = atol(pidp);

	(void) strcpy(pathname, procname);
	(void) strcat(pathname, "/as");
	if ((asfd = open(pathname, O_RDONLY)) < 0)
		return (perr(pathname));

	(void) strcpy(pathname, procname);
	(void) strcat(pathname, reserved? "/rmap" : "/map");
	if ((mapfd = open(pathname, O_RDONLY)) < 0) {
		(void) close(asfd);
		return (perr(pathname));
	}
	if (fstat(mapfd, &statb) != 0) {
		(void) close(asfd);
		(void) close(mapfd);
		return (perr(pathname));
	}
	n = statb.st_size / sizeof (prmap_t);

again:
	nmap = n;
	if (prmapp != NULL)
		free((char *)prmapp);
	prmapp = (prmap_t *)malloc((nmap+1)*sizeof (prmap_t));

	if ((n = pread(mapfd, prmapp, (nmap+1)*sizeof (prmap_t), 0L)) < 0) {
		(void) close(asfd);
		(void) close(mapfd);
		return (perr("read map"));
	}
	n /= sizeof (prmap_t);
	if (nmap < n)			/* unlikely */
		goto again;

	(void) strcpy(pathname, procname);
	(void) strcat(pathname, "/status");
	if ((fd = open(pathname, O_RDONLY)) < 0 ||
	    read(fd, &pstatus, sizeof (pstatus)) != sizeof (pstatus)) {
		if (fd >= 0)
			(void) close(fd);
		(void) close(asfd);
		(void) close(mapfd);
		return (perr("read status"));
	}
	(void) close(fd);

	(void) strcpy(pathname, procname);
	(void) strcat(pathname, "/psinfo");
	if ((fd = open(pathname, O_RDONLY)) < 0 ||
	    read(fd, &psinfo, sizeof (psinfo)) != sizeof (psinfo)) {
		if (fd >= 0)
			(void) close(fd);
		(void) close(asfd);
		(void) close(mapfd);
		return (perr("read psinfo"));
	}
	(void) close(fd);

	(void) printf("%s:\t%.70s\n", pidp, psinfo.pr_psargs);

	if (pstatus.pr_flags & PR_ISSYS)
		goto out;

	if ((s = strchr(psinfo.pr_psargs, ' ')) != NULL)
		*s = '\0';
	make_exec_name(psinfo.pr_psargs);
	load_ldd_names(asfd, pid);

	total = 0;
	for (i = 0, pmp = &prmapp[0]; i < nmap; i++, pmp++) {
		char *lname = NULL;
		size_t size = (pmp->pr_size + 1023) / 1024;

		(void) strcpy(pathname, procname);
		(void) strcat(pathname, "/object/");
		(void) strcat(pathname, pmp->pr_mapname);
		if (pmp->pr_mapname[0] && stat(pathname, &statb) == 0)
			lname = lookup_file(statb.st_dev, statb.st_ino);
		if (lname != NULL)
			/* EMPTY */;
		else if (pmp->pr_vaddr + pmp->pr_size > pstatus.pr_stkbase &&
		    pmp->pr_vaddr < pstatus.pr_stkbase + pstatus.pr_stksize)
			lname = "  [ stack ]";
		else if (pmp->pr_vaddr + pmp->pr_size > pstatus.pr_brkbase &&
		    pmp->pr_vaddr < pstatus.pr_brkbase + pstatus.pr_brksize)
			lname = "  [ heap ]";
		(void) printf(
			lname?
			    "%.8X %4dK %-18s %s\n" :
			    "%.8X %4dK %s\n",
			(uintptr_t)pmp->pr_vaddr, size,
			mflags(pmp->pr_mflags & ~(MA_BREAK|MA_STACK)),
			lname);
		total += size;
	}
	(void) printf(" total %6dK\n", total);

out:
	(void) close(asfd);
	(void) close(mapfd);

	if (prmapp != NULL)
		free((char *)prmapp);

	return (0);
}

static int
perr(char *s)
{
	if (s)
		(void) fprintf(stderr, "%s: ", procname);
	else
		s = procname;
	perror(s);
	return (1);
}

static char *
mflags(u_int arg)
{
	static char code_buf[80];
	char *str = code_buf;

	if (arg == 0)
		return ("-");

	if (arg & ~(MA_READ|MA_WRITE|MA_EXEC|MA_SHARED))
		(void) sprintf(str, "0x%x",
			arg & ~(MA_READ|MA_WRITE|MA_EXEC|MA_SHARED));
	else
		*str = '\0';

	if (arg & MA_READ)
		(void) strcat(str, "/read");
	if (arg & MA_WRITE)
		(void) strcat(str, "/write");
	if (arg & MA_EXEC)
		(void) strcat(str, "/exec");
	if (arg & MA_SHARED)
		(void) strcat(str, "/shared");

	if (*str == '/')
		str++;

	return (str);
}
