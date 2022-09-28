/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */


#pragma ident	"@(#)output.c	1.12	96/03/04 SMI" 	/* SVr4.0 1.3	*/

/*LINTLIBRARY*/

#include "syn.h"

#include <sys/mman.h>

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <libelf.h>
#include <errno.h>
#include "decl.h"
#include "msg.h"

/*
 * File output
 *	These functions write output files.
 *	On SVR4 and newer systems use mmap(2).  On older systems (or on
 *	file systems that don't support mmap), use write(2).
 */


/*ARGSUSED*/
char *
_elf_outmap(int fd, size_t sz, unsigned int * pflag)
{
	char		*p;

	*pflag = 0;
	/*
	 *	Note: Some NFS implimentations do not provide from enlarging a
	 *	file via ftruncate(), thus this may fail with ENOSUP.  In this
	 *	case the fallthrough to the malloc() mechanism will occur.
	 */

	if ((!*pflag) && (ftruncate(fd, (off_t)sz) == 0) &&
	    (p = mmap((char *)0, sz, PROT_READ+PROT_WRITE,
	    MAP_SHARED, fd, (off_t)0)) != (char *)-1) {
		*pflag = 1;
		return (p);
	}

	*pflag = 0;

	/*
	 * If mmap fails, try malloc.  Some file systems don't mmap
	 */
	if ((p = (char *)malloc(sz)) == 0)
		_elf_seterr(EMEM_OUT, 0);
	return (p);
}


/*ARGSUSED*/
size_t
_elf_outsync(int fd, char * p, size_t sz, unsigned int flag)
{
	if (flag != 0) {
		int	err;
		fd = msync(p, sz, MS_SYNC);
		if (fd == -1)
			err = errno;
		(void) munmap(p, sz);
		if (fd == 0)
			return (sz);
		_elf_seterr(EIO_SYNC, err);
		return (0);
	}
	if (lseek(fd, 0L, SEEK_SET) == 0) {
		if (write(fd, p, sz) == sz) {
			(void) free(p);
			return (sz);
		}
	}
	_elf_seterr(EIO_WRITE, errno);
	return (0);
}
