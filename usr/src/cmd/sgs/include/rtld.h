/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)rtld.h	1.15	96/09/30 SMI"

#ifndef		RTLD_DOT_H
#define		RTLD_DOT_H

/*
 * Global include file for the runtime linker support library.
 */
#include	<time.h>
#include	"sgs.h"
#include	"machdep.h"


/*
 * Permission structure.  Used to define access with ld.so.1 link maps.
 */
typedef struct permit {
	unsigned long	p_cnt;		/* No. of p_value entries of which */
	unsigned long	p_value[1];	/* there may be more than one */
} Permit;


/*
 * Linked list of directories or filenames (built from colon seperated string).
 */
typedef struct pnode {
	const char *	p_name;
	int		p_len;
	void *		p_info;
	struct pnode *	p_next;
} Pnode;

typedef struct rt_map	Rt_map;

/*
 * Private structure for communication between rtld_db and rtld.
 */
typedef struct rtld_db_priv {
	long	rtd_version;		/* version no. */
	size_t	rtd_objpad;		/* padding around mmap()ed objects */
} Rtld_db_priv;

#define	R_RTLDDB_VERSION	1	/* current rtld_db/rtld version level */


/*
 * Information for dlopen(), dlsym(), and dlclose() on libraries linked by rtld.
 * Each shared object referred to in a dlopen call has an associated dl_obj
 * structure.  For each such structure there is a list of the shared objects
 * on which the referenced shared object is dependent.
 */
#define	DL_MAGIC	0x580331
#define	DL_DLOPEN_0	0x561109	/* dlopen(0) magic identifier */
#define	DL_CIGAM	0x940212

typedef struct dl_obj {
	long		dl_magic;	/* DL_MAGIC */
	Permit *	dl_permit;	/* permit for this dlopen invocation */
	long		dl_usercnt;	/* count of dlopen invocations */
	long		dl_permcnt;	/* count of permit give-aways */
	List		dl_depends;	/* dependencies applicable for dlsym */
	Rt_map *	dl_lastdep;	/* last applicable dlsym dependency */
	long		dl_cigam;	/* DL_CIGAM */
} Dl_obj;

/*
 * Runtime linker private data maintained for each shared object.  Maps are
 * connected to link map lists for `main' and possibly `rtld'.
 */
typedef	struct lm_list {
	Rt_map *	lm_head;
	Rt_map *	lm_tail;
} Lm_list;

struct rt_map {
	Link_map	rt_public;	/* public data */
	List		rt_alias;	/* list of linked file names */
	void (*		rt_init)();	/* address of _init */
	void (*		rt_fini)();	/* address of _fini */
	char *		rt_runpath;	/* LD_RUN_PATH and its equivalent */
	Pnode *		rt_runlist;	/*	Pnode structures */
	long		rt_count;	/* reference count */
	List		rt_depends;	/* list of dependent libraries */
	Dl_obj *	rt_dlp;		/* pointer to a dlopened object */
	Permit *	rt_permit;	/* ids permitted to access this lm */
	unsigned long	rt_msize;	/* total memory mapped */
	unsigned long	rt_etext;	/* etext address */
	unsigned long	rt_padstart;	/* start of image (including padding) */
	unsigned long	rt_padimlen;	/* size of image (including padding */
	struct fct *	rt_fct;		/* file class table for this object */
	Pnode *		rt_filtees;	/* 	Pnode list of REFNAME(lmp) */
	Rt_map *	rt_lastdep;	/* the last dependency added */
	Sym *(*		rt_symintp)();	/* link map symbol interpreter */
	void *		rt_priv;	/* private data, object type specific */
	Lm_list * 	rt_list;	/* link map list we belong to */
	unsigned long	rt_flags;	/* state flags, see FLG below */
	unsigned long	rt_mode;	/* usage mode, see RTLD mode flags */
	dev_t		rt_stdev;	/* device id and inode number for .so */
	ino_t		rt_stino;	/*	multiple inclusion checks */
	List		rt_refrom;	/* list of referencing objects */
	const char *	rt_dirname;	/* real dirname of loaded object */
	int		rt_dirsz;	/*	and its size */
	List		rt_copy;	/* list of copy relocations */
};

#define	REF_NEEDED	1		/* explicit (needed) dependency */
#define	REF_SYMBOL	2		/* implicit (symbol binding) */
					/*	dependency */
#define	REF_DELETE	3		/* deleted object dependency update */
#define	REF_DLOPEN	4		/* dlopen() dependency update */
#define	REF_DLCLOSE	5		/* dlclose() dependency update */
#define	REF_UNPERMIT	6		/* unnecessary permit removal */


/*
 * Link map state flags.
 */
#define	FLG_RT_ISMAIN	0x00001		/* object represents main executable */
#define	FLG_RT_ANALYZED	0x00002		/* object has been analyzed */
#define	FLG_RT_SETGROUP	0x00004		/* group establishisment required */
#define	FLG_RT_COPYTOOK	0x00008		/* copy relocation taken */
#define	FLG_RT_OBJECT	0x00010		/* object processing (ie. .o's) */
#define	FLG_RT_BOUND	0x00020		/* bound to indicator */
#define	FLG_RT_DELETING	0x00040		/* deletion in progress */
#define	FLG_RT_PROFILE	0x00080		/* image is being profiled */
#define	FLG_RT_IMGALLOC	0x00100		/* image is allocated (not mmap'ed) */
#define	FLG_RT_INITDONE	0x00200		/* objects .init has be called */
#define	FLG_RT_AUX	0x00400		/* filter is an auxiliary filter */
#define	FLG_RT_FIXED	0x00800		/* image location is fixed */
#define	FLG_RT_PRELOAD	0x01000		/* object was preloaded */
#define	FLG_RT_CACHED	0x02000		/* cached version of object used */
#define	FLG_RT_RELOCING	0x04000		/* relocation in progress */
#define	FLG_RT_LOADFLTR	0x08000		/* trigger filtee loading */


/*
 * Macros for getting to link_map data.
 */
#define	ADDR(X)		((X)->rt_public.l_addr)
#define	NAME(X)		((X)->rt_public.l_name)
#define	DYN(X)		((X)->rt_public.l_ld)
#define	NEXT(X)		((X)->rt_public.l_next)
#define	PREV(X)		((X)->rt_public.l_prev)
#define	REFNAME(X)	((X)->rt_public.l_refname)

/*
 * Macros for getting to linker private data.
 */
#define	ALIAS(X)	((X)->rt_alias)
#define	INIT(X)		((X)->rt_init)
#define	FINI(X)		((X)->rt_fini)
#define	RPATH(X)	((X)->rt_runpath)
#define	RLIST(X)	((X)->rt_runlist)
#define	COUNT(X)	((X)->rt_count)
#define	DEPENDS(X)	((X)->rt_depends)
#define	DLP(X)		((X)->rt_dlp)
#define	PERMIT(X)	((X)->rt_permit)
#define	MSIZE(X)	((X)->rt_msize)
#define	ETEXT(X)	((X)->rt_etext)
#define	FCT(X)		((X)->rt_fct)
#define	FILTEES(X)	((X)->rt_filtees)
#define	LASTDEP(X)	((X)->rt_lastdep)
#define	SYMINTP(X)	((X)->rt_symintp)
#define	LIST(X)		((X)->rt_list)
#define	FLAGS(X)	((X)->rt_flags)
#define	MODE(X)		((X)->rt_mode)
#define	REFROM(X)	((X)->rt_refrom)
#define	PADSTART(X)	((X)->rt_padstart)
#define	PADIMLEN(X)	((X)->rt_padimlen)
#define	DIRNAME(X)	((X)->rt_dirname)
#define	DIRSZ(X)	((X)->rt_dirsz)
#define	COPY(X)		((X)->rt_copy)


/*
 * Flags for lookup_sym (and hence find_sym) routines.
 */
#define	LKUP_DEFT	0x0		/* Simple lookup request */
#define	LKUP_SPEC	0x1		/* Special ELF lookup (allows address */
					/*	resolutions to plt[] entries) */
#define	LKUP_LDOT	0x2		/* Indicates the original A_OUT */
					/*	symbol had a leading `.' */
#define	LKUP_FIRST	0x4		/* Lookup symbol in first link map */
					/*	only */

extern Lm_list		lml_main;	/* the `main's link map list */
extern Lm_list		lml_rtld;	/* rtld's link map list */
extern Lm_list *	lml_list[];

extern Sym *		lookup_sym(const char *, Permit *, Rt_map *,
				Rt_map *, Rt_map **, int);

/*
 * Runtime cache structure.
 */
typedef	struct	rtc_head {
	Word	rtc_version;		/* version of cache file */
	time_t	rtc_time;		/* time cache file was created */
	Addr	rtc_base;		/* base address of mapped cache */
	Addr	rtc_begin;		/* address range reservation required */
	Addr	rtc_end;		/*	for cached objects */
	Word	rtc_size;		/* size of cache data in bytes */
	Word	rtc_flags;		/* various flags */
	List *	rtc_objects;		/* offset of object cache list */
	Pnode *	rtc_libpath;		/* offset of libpath list (Pnode) */
	Pnode *	rtc_trusted;		/* offset of trusted dir list (Pnode) */
} Rtc_head;

typedef struct	rtc_obj {
	Word	rtc_hash;		/* hash value of the input filename */
	char *	rtc_name;		/* input filename */
	char *	rtc_cache;		/* associated cache filename */
} Rtc_obj;

#define	RTC_FLG_DEBUG	0x01

/*
 * Cache version definition values.
 */
#define	RTC_VER_NONE	0
#define	RTC_VER_CURRENT	1
#define	RTC_VER_NUM	2

/*
 * Function prototypes.
 */
extern	int	rt_dldump(Rt_map *, const char *, int, int, Addr);

#if	defined(__sparc) || defined(__i386)
extern	void	elf_plt_write(unsigned long *, unsigned long *);
#elif	defined(__ppc)
extern	void	elf_plt_write(unsigned long *, unsigned long *,
		    unsigned long *, Rt_map *);
#else
#error Unknown architecture!
#endif

#endif
