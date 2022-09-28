/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_TYPES_H
#define	_SYS_TYPES_H

#pragma ident	"@(#)types.h	1.49	96/08/14 SMI"

#include <sys/feature_tests.h>
#include <sys/isa_defs.h>

/*
 * Machine dependent definitions moved to <sys/machtypes.h>.
 */
#include <sys/machtypes.h>

/*
 * Include fixed width type declarations proposed by the ISO/JTC1/SC22/WG14 C
 * committee's working draft for the revision of the current ISO C standard,
 * ISO/IEC 9899:1990 Programming language - C.  These are not currently
 * required by any standard but constitute a useful, general purpose set
 * of type definitions which is namespace clean with respect to all standards.
 */
#include <sys/int_types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * The following protects users who use other than Sun compilers
 * (eg, GNU C) that don't support long long, and need to include
 * this header file.
 */
#if __STDC__ - 0 == 0 && !defined(_NO_LONGLONG)
typedef	long long		longlong_t;
typedef	unsigned long long	u_longlong_t;
#else
/* used to reserve space and generate alignment */
typedef union {
	double	_d;
	long	_l[2];
} longlong_t;
typedef union {
	double		_d;
	unsigned long	_l[2];
} u_longlong_t;
#endif	/* __STDC__ - 0 == 0 && !defined(_NO_LONGLONG) */

/*
 * POSIX Extensions
 */
typedef	unsigned char	uchar_t;
typedef	unsigned short	ushort_t;
typedef	unsigned int	uint_t;
typedef	unsigned long	ulong_t;

typedef	char *		caddr_t;	/* ?<core address> type */
typedef	long		daddr_t;	/* <disk address> type */
typedef	short		cnt_t;		/* ?<count> type */

typedef	ulong_t		paddr_t;	/* <physical address> type */
typedef	uchar_t		use_t;		/* use count for swap.  */
typedef	short		sysid_t;
typedef	short		index_t;

/*
 * The size of off_t and related types depends on the setting of
 * _FILE_OFFSET_BITS.  (Note that other system headers define other types
 * related to those defined here.)
 *
 * If _LARGEFILE64_SOURCE is defined, variants of these types that are
 * explicitly 64 bits wide become available.
 */
#ifndef _OFF_T
#define	_OFF_T
#if _FILE_OFFSET_BITS == 32
typedef long		off_t;		/* offsets within files */
#elif _FILE_OFFSET_BITS == 64
typedef longlong_t	off_t;		/* offsets within files */
#endif
#if	defined(_LARGEFILE64_SOURCE)
typedef longlong_t	off64_t;	/* offsets within files */
#endif
#endif /* _OFF_T */

#if _FILE_OFFSET_BITS == 32
typedef ulong_t		ino_t;		/* expanded inode type	*/
typedef long		blkcnt_t;	/* count of file blocks */
typedef ulong_t		fsblkcnt_t;	/* count of file system blocks */
typedef ulong_t		fsfilcnt_t;	/* count of files */
#elif _FILE_OFFSET_BITS == 64
typedef u_longlong_t	ino_t;		/* expanded inode type	*/
typedef longlong_t	blkcnt_t;	/* count of file blocks */
typedef u_longlong_t	fsblkcnt_t;	/* count of file system blocks */
typedef u_longlong_t	fsfilcnt_t;	/* count of files */
#endif
#if	defined(_LARGEFILE64_SOURCE)
typedef u_longlong_t	ino64_t;	/* expanded inode type	*/
typedef longlong_t	blkcnt64_t;	/* count of file blocks */
typedef u_longlong_t	fsblkcnt64_t;	/* count of file system blocks */
typedef u_longlong_t	fsfilcnt64_t;	/* count of files */
#endif

#if defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE)
typedef enum { _B_FALSE, _B_TRUE } boolean_t;
#else
typedef enum { B_FALSE, B_TRUE } boolean_t;
#endif /* defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE) */

/*
 * The [u]pad64_t is to be used in structures such that those structures
 * may be accessed by code produced by compilation environments which don't
 * support a 64 bit integral datatype.  This intention is not to allow
 * use of these fields in such environments, but to maintain the alignment
 * and offsets of the structure.
 */
#if __STDC__ - 0 == 0 && !defined(_NO_LONGLONG)
typedef int64_t		pad64_t;
typedef	uint64_t	upad64_t;
#else
typedef union {
	double   _d;
	int32_t  _l[2];
} pad64_t;
typedef union {
	double   _d;
	uint32_t _l[2];
} upad64_t;
#endif

typedef	longlong_t	offset_t;
typedef	u_longlong_t	u_offset_t;
typedef u_longlong_t	len_t;
typedef	longlong_t	diskaddr_t;

/*
 * Definitions remaining from previous partial support for 64-bit file
 * offsets.  This partial support for devices greater than 2gb requires
 * compiler support for long long.
 */
#ifdef _LONG_LONG_LTOH
typedef union {
	offset_t	_f;	/* Full 64 bit offset value */
	struct {
		off_t _l;	/* lower 32 bits of offset value */
		long _u;	/* upper 32 bits of offset value */
	} _p;
} lloff_t;
#endif

#ifdef _LONG_LONG_HTOL
typedef union {
	offset_t	_f;	/* Full 64 bit offset value */
	struct {
		long _u;	/* upper 32 bits of offset value */
		off_t _l;	/* lower 32 bits of offset value */
	} _p;
} lloff_t;
#endif

#ifdef _LONG_LONG_LTOH
typedef union {
	diskaddr_t	_f;	/* Full 64 bit disk address value */
	struct {
		daddr_t _l;	/* lower 32 bits of disk address value */
		long _u;	/* upper 32 bits of disk address value */
	} _p;
} lldaddr_t;
#endif

#ifdef _LONG_LONG_HTOL
typedef union {
	diskaddr_t	_f;	/* Full 64 bit disk address value */
	struct {
		long _u;	/* upper 32 bits of disk address value */
		daddr_t _l;	/* lower 32 bits of disk address value */
	} _p;
} lldaddr_t;
#endif

typedef ulong_t k_fltset_t;	/* kernel fault set type */

/*
 * The following type is for various kinds of identifiers.  The
 * actual type must be the same for all since some system calls
 * (such as sigsend) take arguments that may be any of these
 * types.  The enumeration type idtype_t defined in sys/procset.h
 * is used to indicate what type of id is being specified.
 */
typedef long		id_t;		/* A process id,	*/
					/* process group id,	*/
					/* session id, 		*/
					/* scheduling class id,	*/
					/* user id, or group id */
/*
 * Type useconds_t is an unsigned integral type capable of storing
 * values at least in the range of zero to 1,000,000.
 */
typedef uint_t 		useconds_t;	/* Time, in microseconds */

/*
 * Typedefs for dev_t components.
 */
typedef ulong_t	major_t;	/* major part of device number */
typedef ulong_t	minor_t;	/* minor part of device number */

/*
 * The data type of a thread priority.
 */
typedef short	pri_t;

/*
 * For compatibility reasons the following typedefs (prefixed o_)
 * can't grow regardless of the EFT definition. Although,
 * applications should not explicitly use these typedefs
 * they may be included via a system header definition.
 * WARNING: These typedefs may be removed in a future
 * release.
 *		ex. the definitions in s5inode.h remain small
 *			to preserve compatibility in the S5
 *			file system type.
 */
typedef	ushort_t o_mode_t;		/* old file attribute type */
typedef short	o_dev_t;		/* old device type	*/
typedef	ushort_t o_uid_t;		/* old UID type		*/
typedef	o_uid_t	o_gid_t;		/* old GID type		*/
typedef	short	o_nlink_t;		/* old file link type	*/
typedef short	o_pid_t;		/* old process id type	*/
typedef ushort_t o_ino_t;		/* old inode type	*/


/*
 * POSIX and XOPEN Declarations
 */
typedef	int	key_t;			/* IPC key type		*/
typedef	ulong_t	mode_t;			/* file attribute type	*/

#ifndef	_UID_T
#define	_UID_T
typedef	long	uid_t;			/* UID type		*/
#endif

typedef	uid_t	gid_t;			/* GID type		*/
typedef	ulong_t nlink_t;		/* file link type	*/
typedef ulong_t	dev_t;			/* expanded device type */
typedef long	pid_t;			/* process id type	*/

#ifndef _SIZE_T
#define	_SIZE_T
typedef	uint_t	size_t;		/* len param for string funcs */
#endif

#ifndef _SSIZE_T
#define	_SSIZE_T
typedef int	ssize_t;	/* used by functions which return a */
				/* count of bytes or an error indication */
#endif

#ifndef _TIME_T
#define	_TIME_T
typedef	long		time_t;	/* time of day in seconds */
#endif	/* END _TIME_T */

#ifndef _CLOCK_T
#define	_CLOCK_T
typedef	long		clock_t; /* relative time in a specified resolution */
#endif	/* ifndef _CLOCK_T */

#ifndef _CLOCKID_T
#define	_CLOCKID_T
typedef	int	clockid_t;	/* clock identifier type */
#endif	/* ifndef _CLOCKID_T */

#ifndef _TIMER_T
#define	_TIMER_T
typedef	int	timer_t;	/* timer identifier type */
#endif	/* ifndef _TIMER_T */

#if defined(__EXTENSIONS__) || \
	(!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))

typedef	unsigned char	unchar;
typedef	unsigned short	ushort;
typedef	unsigned int	uint;
typedef	unsigned long	ulong;

#if defined(_KERNEL)

#define	SHRT_MIN	(-32768)	/* min value of a "short int" */
#define	SHRT_MAX	32767		/* max value of a "short int" */
#define	USHRT_MAX	65535		/* max of "unsigned short int" */
#define	INT_MIN		(-2147483647-1) /* min value of an "int" */
#define	INT_MAX		2147483647	/* max value of an "int" */
#define	UINT_MAX	4294967295U	/* max value of an "unsigned int" */
#if defined(_LP64)
#define	LONG_MIN	(-9223372036854775807L-1L)
					/* min value of a "long int" */
#define	LONG_MAX	9223372036854775807L
					/* max value of a "long int" */
#define	ULONG_MAX	18446744073709551615UL
					/* max of "unsigned long int" */
#else /* _ILP32 */
#define	LONG_MIN	(-2147483647L-1L)
					/* min value of a "long int" */
#define	LONG_MAX	2147483647L	/* max value of a "long int" */
#define	ULONG_MAX	4294967295UL	/* max of "unsigned long int" */
#endif

#endif	/* defined(_KERNEL) */

#define	P_MYPID	((pid_t)0)

/*
 * The following is the value of type id_t to use to indicate the
 * caller's current id.  See procset.h for the type idtype_t
 * which defines which kind of id is being specified.
 */
#define	P_MYID	(-1)
#define	NOPID (pid_t)(-1)

#ifndef NODEV
#define	NODEV (dev_t)(-1)
#endif

/*
 * A host identifier is used to uniquely define a particular node
 * on an rfs network.  Its type is as follows.
 */
typedef	long	hostid_t;

/*
 * The following value of type hostid_t is used to indicate the
 * current host.  The actual hostid for each host is in the
 * kernel global variable rfs_hostid.
 */
#define	P_MYHOSTID	((hostid_t)-1)

typedef unsigned char	u_char;
typedef unsigned short	u_short;
typedef unsigned int	u_int;
typedef unsigned long	u_long;
typedef struct _quad { long val[2]; } quad;		/* used by UFS */

/*
 * Nested include for BSD/sockets source compatibility.
 * (The select macros used to be defined here).
 */
#include <sys/select.h>

#endif	/* defined(__EXTENSIONS__) || (!defined(_POSIX_C_SOURCE) && ... */

/*
 * _VOID was defined to be either void or char but this is not
 * required because previous SunOS compilers have accepted the void
 * type. However, because many system header and source files use the
 * void keyword, the volatile keyword, and ANSI C function prototypes,
 * non-ANSI compilers cannot compile the system anyway. The _VOID macro
 * should therefore not be used and remains for source compatibility
 * only.
 */
/* CSTYLED */
#define	_VOID	void

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_TYPES_H */
