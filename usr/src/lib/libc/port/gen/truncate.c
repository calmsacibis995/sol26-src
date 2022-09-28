/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)truncate.c	1.12	96/02/26 SMI"	/* SVr4.0 1.3	*/

/*LINTLIBRARY*/
/*
 * ftruncate() and truncate() set a file to a specified
 * length using fcntl(F_FREESP) system call. If the file
 * was previously longer than length, the bytes past the
 * length will no longer be accessible. If it was shorter,
 * bytes not written will be zero filled.
 */

#if _FILE_OFFSET_BITS == 64
#pragma weak ftruncate64 = _ftruncate64
#pragma weak truncate64 = _truncate64
#define	ftruncate64	_ftruncate64
#define	open64		_open64
#define	truncate64	_truncate64
#if defined(ABI) || defined(DSHLIB)
#undef	ftruncate64
#define	ftruncate64	_abi_ftruncate64
#undef	truncate64
#define	truncate64	_abi_truncate64
#endif
#else /* _FILE_OFFSET_BITS == 64 */
#pragma weak ftruncate = _ftruncate
#pragma weak truncate = _truncate
#define	ftruncate	_ftruncate
#define	open		_open
#define	truncate	_truncate
#if defined(ABI) || defined(DSHLIB)
#undef	ftruncate
#define	ftruncate	_abi_ftruncate
#undef	truncate
#define	truncate	_abi_truncate
#endif
#endif /* _FILE_OFFSET_BITS == 64 */

#define	close		_close
#define	fcntl		_fcntl

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>

int
ftruncate(int fildes, off_t len)
{
	struct flock lck;

	lck.l_whence = 0;	/* offset l_start from beginning of file */
	lck.l_start = len;
	lck.l_type = F_WRLCK;	/* setting a write lock */
	lck.l_len = (off_t)0;	/* until the end of the file address space */

	if (fcntl(fildes, F_FREESP, (int)&lck) == -1) {
		return (-1);
	}
	return (0);
}

int
truncate(const char *path, off_t len)
{

	register fd;


	if ((fd = open(path, O_WRONLY)) == -1) {
		return (-1);
	}

	if (ftruncate(fd, len) == -1) {
		close(fd);
		return (-1);
	}

	close(fd);
	return (0);
}
