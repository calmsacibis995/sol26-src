/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _DLFCN_H
#define	_DLFCN_H

#pragma ident	"@(#)dlfcn.h	1.24	96/10/01 SMI"	/* SVr4.0 1.2	*/

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Information structure returned by dladdr().
 */
typedef struct	dl_info {
	const char *	dli_fname;	/* file containing address range */
	void *		dli_fbase;	/* base address of file image */
	const char *	dli_sname;	/* symbol name */
	void *		dli_saddr;	/* symbol address */
} Dl_info;

/*
 * Declarations used for dynamic linking support routines.
 */
#ifdef __STDC__
extern void *		dlopen(const char *, int);
extern void *		dlsym(void *, const char *);
extern int		dlclose(void *);
extern char *		dlerror(void);
extern int		dladdr(void *, Dl_info *);
extern int		dldump(const char *, const char *, int);
#else
extern void *		dlopen();
extern void *		dlsym();
extern int		dlclose();
extern char *		dlerror();
extern int		dladdr();
extern int		dldump();
#endif

/*
 * Valid values for handle argument to dlsym(3x).
 */
#define	RTLD_NEXT		(void *)-1	/* look in `next' dependency */

/*
 * Valid values for mode argument to dlopen.
 */
#define	RTLD_LAZY		0x00001		/* deferred function binding */
#define	RTLD_NOW		0x00002		/* immediate function binding */
#define	RTLD_NOLOAD		0x00004		/* don't load object */

#define	RTLD_GLOBAL		0x00100		/* export symbols to others */
#define	RTLD_LOCAL		0x00000		/* symbols are only available */
						/*	to group members */
#define	RTLD_PARENT		0x00200		/* add parent (caller) to */
						/*	a group dependencies */
#define	RTLD_GROUP		0x00400		/* resolve symbols within */
						/*	members of the group */
#define	RTLD_WORLD		0x00800		/* resolve symbols within */
						/*	global objects */
#define	RTLD_NODELETE		0x01000		/* do not remove members */


/*
 * Valid values for flag argument to dldump.
 */
#define	RTLD_REL_RELATIVE	0x00001		/* apply relative relocs */
#define	RTLD_REL_EXEC		0x00002		/* apply symbolic relocs that */
						/*	bind to main */
#define	RTLD_REL_DEPENDS	0x00004		/* apply symbolic relocs that */
						/*	bind to dependencies */
#define	RTLD_REL_PRELOAD	0x00008		/* apply symbolic relocs that */
						/*	bind to preload objs */
#define	RTLD_REL_SELF		0x00010		/* apply symbolic relocs that */
						/*	bind to overself */
#define	RTLD_REL_ALL		0x00fff 	/* apply all relocs */

#define	RTLD_MEMORY		0x01000		/* use memory sections */
#define	RTLD_STRIP		0x02000		/* retain allocable sections */
						/*	only */
#define	RTLD_NOHEAP		0x04000		/* do no save any heap */


#ifdef	__cplusplus
}
#endif

#endif	/* _DLFCN_H */
