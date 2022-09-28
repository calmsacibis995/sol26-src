/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)scantext.c	1.6	96/08/14 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "pcontrol.h"

#define	BLKSIZE	4096

#if i386
#define	LCALL	0x9a
#define	PCADJ	7
static syscall_t old_lcall = { 0x9a, 0, 0, 0, 0, 0x7, 0 };
static syscall_t new_lcall = { 0x9a, 0, 0, 0, 0, 0x27, 0 };
#elif __ppc
#define	PCADJ	4
#elif sparc
#define	PCADJ	0
#endif

int
scantext(process_t *Pr)		/* look for SYSCALL instruction in process */
{
	char mapfile[100];
	int mapfd;
#if i386
	unsigned char *p;	/* pointer into buf */
	long osysaddr = 0;	/* address of old SYSCALL instruction */
#elif sparc || __ppc
	uint32_t *p;		/* pointer into buf */
#endif
	off_t offset;		/* offset in text section */
	off_t endoff;		/* ending offset in text section */
	long sysaddr;		/* address of SYSCALL instruction */
	int nbytes;		/* number of bytes in buffer */
	int n2bytes;		/* number of bytes in second buffer */
	int nmappings;		/* current number of mappings */
	syscall_t instr;	/* instruction from process */
	prmap_t *pdp;		/* pointer to map descriptor */
	prmap_t *prbuf;		/* buffer for map descriptors */
	unsigned nmap;		/* number of map descriptors */
	char buf[2*BLKSIZE];	/* buffer for reading text */

	/* try the most recently-seen syscall address */
	if ((sysaddr = Pr->sysaddr) != 0)
#if i386
		if (Pread(Pr, sysaddr, instr, (int)sizeof (instr))
		    != sizeof (instr))
			sysaddr = 0;
		else if (memcmp(instr, old_lcall, sizeof (old_lcall)) == 0) {
			osysaddr = sysaddr;
			sysaddr = 0;
		} else if (memcmp(instr, new_lcall, sizeof (new_lcall)) != 0) {
			sysaddr = 0;
		}
#else
		if (Pread(Pr, sysaddr, &instr, (int)sizeof (instr))
		    != sizeof (instr) || instr != (syscall_t)SYSCALL)
			sysaddr = 0;
#endif

	/* try the current PC minus sizeof (syscall) */
	if (sysaddr == 0 && (sysaddr = Pr->REG[R_PC]-PCADJ) != 0)
#if i386
		if (Pread(Pr, sysaddr, instr, (int)sizeof (instr))
		    != sizeof (instr))
			sysaddr = 0;
		else if (memcmp(instr, old_lcall, sizeof (old_lcall)) == 0) {
			osysaddr = sysaddr;
			sysaddr = 0;
		} else if (memcmp(instr, new_lcall, sizeof (new_lcall)) != 0) {
			sysaddr = 0;
		}
#else
		if (Pread(Pr, sysaddr, &instr, (int)sizeof (instr))
		    != sizeof (instr) || instr != (syscall_t)SYSCALL)
			sysaddr = 0;
#endif

	if (sysaddr) {		/* we have the address of a SYSCALL */
		Pr->sysaddr = sysaddr;
		return (0);
	}

	Pr->sysaddr = 0;	/* assume failure */

	/* open the /proc/<pid>/map file */
	(void) sprintf(mapfile, "%s/%ld/map", procdir, Pr->pid);
	if ((mapfd = open(mapfile, O_RDONLY)) < 0) {
		perror("scantext(): cannot open map file");
		return (-1);
	}

	/* allocate a plausible initial buffer size */
	nmap = 50;

	/* read all the map structures, allocating more space as needed */
	for (;;) {
		prbuf = malloc(nmap * sizeof (prmap_t));
		if (prbuf == NULL) {
			(void) close(mapfd);
			perror(
			    "scantext(): cannot allocate buffer for map file");
			return (-1);
		}
		nmappings = pread(mapfd, prbuf, nmap * sizeof (prmap_t), 0L);
		if (nmappings < 0) {
			free(prbuf);
			(void) close(mapfd);
			perror("scantext(): cannot read map file");
			return (-1);
		}
		nmappings /= sizeof (prmap_t);
		if (nmappings < nmap)	/* we read them all */
			break;
		/* allocate a bigger buffer */
		free(prbuf);
		nmap *= 2;
	}
	(void) close(mapfd);

	/* null out the entry following the last valid entry */
	(void) memset((char *)&prbuf[nmappings], 0, sizeof (prmap_t));

	/* scan the mappings looking for an executable mappings */
	for (pdp = &prbuf[0]; sysaddr == 0 && pdp->pr_size; pdp++) {

		offset = (off_t)pdp->pr_vaddr;	/* beginning of text */
		endoff = offset + pdp->pr_size;

		/* avoid non-EXEC mappings; avoid the stack and heap */
		if ((pdp->pr_mflags&MA_EXEC) == 0 ||
		    (endoff > Pr->why.pr_stkbase &&
		    offset < Pr->why.pr_stkbase + Pr->why.pr_stksize) ||
		    (endoff > Pr->why.pr_brkbase &&
		    offset < Pr->why.pr_brkbase + Pr->why.pr_brksize))
			continue;

		(void) lseek(Pr->asfd, (off_t)offset, 0);

		if ((nbytes = read(Pr->asfd, &buf[0], 2*BLKSIZE)) <= 0) {
			if (nbytes < 0)
				perror("scantext(): read()");
			continue;
		}

		if (nbytes < BLKSIZE)
			n2bytes = 0;
		else {
			n2bytes = nbytes - BLKSIZE;
			nbytes  = BLKSIZE;
		}
#if i386
		p = (unsigned char *)&buf[0];
#elif sparc || __ppc
		/* LINTED improper alignment */
		p = (uint32_t *)&buf[0];
#endif
		/* search text for a SYSCALL instruction */
		while (sysaddr == 0 && offset < endoff) {
			if (nbytes <= 0) {	/* shift buffers */
				if ((nbytes = n2bytes) <= 0)
					break;
				(void) memcpy(&buf[0], &buf[BLKSIZE], nbytes);
#if i386
				p = (unsigned char *)&buf[0];
#elif sparc || __ppc
				/* LINTED improper alignment */
				p = (uint32_t *)&buf[0];
#endif
				n2bytes = 0;
				if (nbytes == BLKSIZE &&
				    offset + BLKSIZE < endoff)
					n2bytes = read(Pr->asfd, &buf[BLKSIZE],
						BLKSIZE);
			}
#if i386
			if (*p == LCALL) {
				if (memcmp(p, old_lcall, sizeof (old_lcall))
				    == 0)
					osysaddr = offset;
				if (memcmp(p, new_lcall, sizeof (new_lcall))
				    == 0)
					sysaddr = offset;
			}
			p++;
			offset++;
			nbytes--;
#elif sparc || __ppc
			if (*p++ == SYSCALL)
				sysaddr = offset;
			offset += sizeof (long);
			nbytes -= sizeof (long);
#endif
		}
	}

	free(prbuf);
#if i386
	/* if we failed to find a new syscall, use an old one, if any */
	if (sysaddr == 0)
		sysaddr = osysaddr;
#endif
	Pr->sysaddr = sysaddr;
	return (sysaddr? 0 : -1);
}
