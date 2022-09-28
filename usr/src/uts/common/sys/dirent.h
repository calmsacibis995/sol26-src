/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1995,1996 Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_DIRENT_H
#define	_SYS_DIRENT_H

#pragma ident	"@(#)dirent.h	1.25	96/08/06 SMI"	/* SVr4.0 11.11 */

#include <sys/feature_tests.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * File-system independent directory entry.
 */
typedef struct dirent {
	ino_t		d_ino;		/* "inode number" of entry */
	off_t		d_off;		/* offset of disk directory entry */
	unsigned short	d_reclen;	/* length of this record */
	char		d_name[1];	/* name of file */
} dirent_t;

/* transitional large file interface version */
#ifdef	_LARGEFILE64_SOURCE
typedef struct dirent64 {
	ino64_t		d_ino;		/* "inode number" of entry */
	off64_t		d_off;		/* offset of disk directory entry */
	unsigned short	d_reclen;	/* length of this record */
	char		d_name[1];	/* name of file */
} dirent64_t;
#endif


#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
#if defined(_KERNEL)
#define	DIRENT64_RECLEN(namelen)	\
	(((int)(((dirent64_t *)0)->d_name) + 1 + (namelen) + 7) & ~ 7)
#define	DIRENT32_RECLEN(namelen)	\
	(((int)(((dirent_t *)0)->d_name) + 1 + (namelen) + 3) & ~ 3)
#endif

#if !defined(_KERNEL)

/* large file compilation environment setup */
#if _FILE_OFFSET_BITS == 64
#ifdef	__PRAGMA_REDEFINE_EXTNAME
#pragma redefine_extname	getdents	getdents64
#else
#define	getdents		getdents64
#endif
#endif	/* _FILE_OFFSET_BITS == 64 */

#if defined(__STDC__)
int getdents(int, struct dirent *, unsigned);
#else
int getdents();
#endif

/* N.B.: transitional large file interface version deliberately not provided */

#endif /* !defined(_KERNEL) */
#endif /* (!defined(_POSIX_C_SOURCE)  && !defined(_XOPEN_SOURCE)) ... */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DIRENT_H */
