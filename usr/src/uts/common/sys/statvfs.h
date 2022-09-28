/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1996 Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_STATVFS_H
#define	_SYS_STATVFS_H

#pragma ident	"@(#)statvfs.h	1.18	96/06/05 SMI"	/* SVr4.0 1.10 */

#include <sys/feature_tests.h>
#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Structure returned by statvfs(2).
 */

#define	_FSTYPSZ	16
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#ifndef FSTYPSZ
#define	FSTYPSZ	_FSTYPSZ
#endif
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

typedef struct statvfs {
	unsigned long	f_bsize;	/* fundamental file system block size */
	unsigned long	f_frsize;	/* fragment size */
	fsblkcnt_t	f_blocks;	/* total blocks of f_frsize on fs */
	fsblkcnt_t	f_bfree;	/* total free blocks of f_frsize */
	fsblkcnt_t	f_bavail;	/* free blocks avail to non-superuser */
	fsfilcnt_t	f_files;	/* total file nodes (inodes) */
	fsfilcnt_t	f_ffree;	/* total free file nodes */
	fsfilcnt_t	f_favail;	/* free nodes avail to non-superuser */
	unsigned long	f_fsid;		/* file system id (dev for now) */
	char		f_basetype[_FSTYPSZ];	/* target fs type name, */
						/* null-terminated */
	unsigned long	f_flag;		/* bit-mask of flags */
	unsigned long	f_namemax;	/* maximum file name length */
	char		f_fstr[32];	/* filesystem-specific string */
	unsigned long	f_filler[16];	/* reserved for future expansion */
} statvfs_t;

/* transitional large file interface version */
#if	defined(_LARGEFILE64_SOURCE)
typedef struct statvfs64 {
	u_int	f_bsize;	/* preferred file system block size */
	u_int	f_frsize;	/* fundamental file system block size */
	fsblkcnt64_t f_blocks;	/* total # of blocks of f_frsize on fs */
	fsblkcnt64_t f_bfree;	/* total # of free blocks of f_frsize */
	fsblkcnt64_t f_bavail;	/* # of free blocks avail to non-superuser */
	fsfilcnt64_t f_files;	/* total # of file nodes (inodes) */
	fsfilcnt64_t f_ffree;	/* total # of free file nodes */
	fsfilcnt64_t f_favail;	/* # of free nodes avail to non-superuser */
	u_int	f_fsid;		/* file system id (dev for now) */
	char	f_basetype[FSTYPSZ]; /* target fs type name, null-terminated */
	u_int	f_flag;		/* bit-mask of flags */
	u_int	f_namemax;	/* maximum file name length */
	char	f_fstr[32];	/* filesystem-specific string */
	u_long	f_filler[16];	/* reserved for future expansion */
} statvfs64_t;
#endif

/*
 * Flag definitions.
 */

#define	ST_RDONLY	0x01	/* read-only file system */
#define	ST_NOSUID	0x02	/* does not support setuid/setgid semantics */
#define	ST_NOTRUNC	0x04	/* does not truncate long file names */

/* large file compilation environment setup */
#if _FILE_OFFSET_BITS == 64
#ifdef	__PRAGMA_REDEFINE_EXTNAME
#pragma redefine_extname	statvfs		statvfs64
#pragma redefine_extname	fstatvfs	fstatvfs64
#else
#define	statvfs			statvfs64
#define	fstatvfs		fstatvfs64
#endif
#endif	/* _FILE_OFFSET_BITS == 64 */

#if defined(__STDC__) && !defined(_KERNEL)
int statvfs(const char *, struct statvfs *);
int fstatvfs(int, struct statvfs *);

/* transitional large file interface versions */
#if	defined(_LARGEFILE64_SOURCE) && !((_FILE_OFFSET_BITS == 64) && \
	    !defined(__PRAGMA_REDEFINE_EXTNAME))
int statvfs64(const char *, struct statvfs64 *);
int fstatvfs64(int, struct statvfs64 *);
#endif	/* _LARGEFILE64_SOURCE... */
#endif	/* defined(__STDC__) && !defined(_KERNEL) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_STATVFS_H */
