/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.  All Rights Reserved.
 */

#ifndef _SYS_BOOTSVCS_H
#define	_SYS_BOOTSVCS_H

#pragma ident	"@(#)bootsvcs.h	1.5	94/03/22 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Boot time configuration information objects
 */

/*
 * Declarations for boot service routines
 */
#ifndef _KERNEL
extern int	printf(), putchar(), strlen();
extern char	*strcpy(), *strcat(), *strncpy(), *memcpy();
extern unchar	getchar();
extern int	goany();
extern int	gets(), memcmp(), ischar(), memset();
extern int 	open(), read(), lseek(), close(), fstat();
extern char 	*malloc();
extern char	*get_fonts();
extern unsigned int vlimit();
#endif

struct boot_syscalls {				/* offset */
	int	(*printf)();			/* 0  */
	char	*(*strcpy)();			/* 1  */
	char	*(*strncpy)();			/* 2  */
	char	*(*strcat)();			/* 3  */
	int	(*strlen)();			/* 4  */
	char	*(*memcpy)();			/* 5  */
	char	*(*memcmp)();			/* 6  */
	unchar	(*getchar)();			/* 7  */
	int	(*putchar)();			/* 8  */
	int	(*ischar)();			/* 9  */
	int	(*goany)();			/* 10 */
	int	(*gets)();			/* 11 */
	int	(*memset)();			/* 12 */
	int	(*open)();			/* 13 */
	int	(*read)();			/* 14 */
	int	(*lseek)();			/* 15 */
	int	(*close)();			/* 16 */
	int	(*fstat)();			/* 17 */
	char	*(*malloc)();			/* 18 */
	char	*(*get_fonts)();		/* 19 */
	unsigned int  (*vlimit)();		/* 20 */
};

extern struct	boot_syscalls *sysp;

/*
 * Boot system syscall functions
 */

#define	printf sysp->printf
#define	getchar sysp->getchar
#define	putchar sysp->putchar
#define	ischar sysp->ischar
#define	get_fonts sysp->get_fonts
#ifndef _KERNEL
#define	strcpy sysp->strcpy
#define	strncpy sysp->strncpy
#define	strcat sysp->strcat
#define	strlen sysp->strlen
#define	memcpy sysp->memcpy
#define	memcmp sysp->memcmp
#define	goany sysp->goany
#define	gets sysp->gets
#define	memset sysp->memset
#define	open sysp->open
#define	read sysp->read
#define	lseek sysp->lseek
#define	close sysp->close
#define	fstat sysp->fstat
#define	malloc sysp->malloc
#define	vlimit sysp->vlimit
#endif

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_BOOTSVCS_H */
