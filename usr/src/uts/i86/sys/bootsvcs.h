/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.  All Rights Reserved.
 */

#ifndef _SYS_BOOTSVCS_H
#define	_SYS_BOOTSVCS_H

#pragma ident	"@(#)bootsvcs.h	1.9	96/04/08 SMI"

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
extern void	printf(), putchar(), *memcpy(), *memset();
extern int	strlen();
extern char	*strcpy(), *strcat(), *strncpy();
extern int	getchar();
extern int	goany();
extern int	gets(), memcmp(), ischar();
extern int 	open(), read(), close(), fstat();
extern off_t	lseek();
extern char 	*malloc();
extern paddr_t	get_fonts();
extern unsigned int vlimit();
#endif

struct boot_syscalls {				/* offset */
	void	(*printf)(char *, ...);		/* 0  */
	char	*(*strcpy)();			/* 1  */
	char	*(*strncpy)();			/* 2  */
	char	*(*strcat)();			/* 3  */
	int	(*strlen)();			/* 4  */
	void	*(*memcpy)();			/* 5  */
	int	(*memcmp)();			/* 6  */
	int	(*getchar)();			/* 7  */
	void	(*putchar)();			/* 8  */
	int	(*ischar)();			/* 9  */
	int	(*goany)();			/* 10 */
	int	(*gets)();			/* 11 */
	void	*(*memset)();			/* 12 */
	int	(*open)();			/* 13 */
	int	(*read)();			/* 14 */
	off_t	(*lseek)();			/* 15 */
	int	(*close)();			/* 16 */
	int	(*fstat)();			/* 17 */
	char	*(*malloc)();			/* 18 */
	paddr_t	(*get_fonts)();			/* 19 */
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
#endif	/* !_KERNEL  */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_BOOTSVCS_H */
