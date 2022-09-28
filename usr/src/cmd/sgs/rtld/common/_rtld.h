/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#ifndef	_RTLD_DOT_H
#define	_RTLD_DOT_H

#pragma ident	"@(#)_rtld.h	1.60	96/10/11 SMI"


/*
 * Common header for run-time linker.
 */
#include	<link.h>
#include	<sys/types.h>
#include	<stdarg.h>
#include	<synch.h>
#include	<signal.h>
#include	<errno.h>
#include	<unistd.h>
#include	"rtld.h"
#include	"sgs.h"
#include	"machdep.h"

#define		DEBUG

/*
 * Types of directory search rules.
 */
#define	ENVDIRS 1
#define	RUNDIRS 2
#define	DEFAULT 3


/*
 * Data structure for file class specific functions and data.
 */
typedef struct fct {
	int (*		fct_are_u_this)();	/* determine type of object */
	unsigned long (*fct_entry_pt)();	/* get entry point */
	Rt_map * (*	fct_map_so)();		/* map in a shared object */
	Rt_map * (*	fct_new_lm)();		/* make a new link map */
	int (*		fct_unmap_so)();	/* unmap a shared object */
	int (*		fct_ld_needed)();	/* determine needed objects */
	Sym * (*	fct_lookup_sym)();	/* initialize symbol lookup */
	Sym * (*	fct_find_sym)();	/* find symbol in load map */
	int (*		fct_reloc)();		/* relocate shared object */
	int *		fct_search_rules;	/* search path rules */
	Pnode * 	fct_dflt_dirs;		/* list of default dirs to */
						/*	search */
	Pnode * 	fct_secure_dirs;	/* list of secure dirs to */
						/*	search (set[ug]id) */
	const char * (*	fct_fix_name)();	/* prepend ./ to pathname */
						/*	without a slash */
	char * (*	fct_get_so)();		/* get shared object */
	void (*		fct_dladdr)();		/* get symbolic address */
	Sym * (*	fct_dlsym)();		/* process dlsym request */
} Fct;


/*
 * Macros for getting to the file class table.
 */
#define	LM_ENTRY_PT(X)		((X)->rt_fct->fct_entry_pt)
#define	LM_UNMAP_SO(X)		((X)->rt_fct->fct_unmap_so)
#define	LM_NEW_LM(X)		((X)->rt_fct->fct_new_lm)
#define	LM_LD_NEEDED(X)		((X)->rt_fct->fct_ld_needed)
#define	LM_LOOKUP_SYM(X)	((X)->rt_fct->fct_lookup_sym)
#define	LM_FIND_SYM(X)		((X)->rt_fct->fct_find_sym)
#define	LM_RELOC(X)		((X)->rt_fct->fct_reloc)
#define	LM_SEARCH_RULES(X)	((X)->rt_fct->fct_search_rules)
#define	LM_DFLT_DIRS(X)		((X)->rt_fct->fct_dflt_dirs)
#define	LM_SECURE_DIRS(X)	((X)->rt_fct->fct_secure_dirs)
#define	LM_FIX_NAME(X)		((X)->rt_fct->fct_fix_name)
#define	LM_GET_SO(X)		((X)->rt_fct->fct_get_so)
#define	LM_DLADDR(X)		((X)->rt_fct->fct_dladdr)
#define	LM_DLSYM(X)		((X)->rt_fct->fct_dlsym)

/*
 * Size of buffer for building error messages.
 */
#define	ERRSIZE		1024

/*
 * Data structure to hold interpreter information.
 */
typedef struct interp {
	char *		i_name;		/* interpreter name */
	caddr_t		i_faddr;	/* address interpreter is mapped at */
} Interp;

/*
 * Data structure used to keep track of copy relocations.  These relocations
 * are collected during initial relocation processing and maintained on the
 * COPY(lmp) list of the defining object.  Each copy list is also added to the
 * COPY(lmp) of the head object (normally the application dynamic executable)
 * from which they will be processed after all relocations are done.
 *
 * The use of RTLD_GROUP will also reference individual objects COPY(lmp) lists
 * in case a bound symbol must be assigned to it actual copy relocation.
 */
typedef struct rel_copy	{
	const char *	r_name;		/* symbol name */
	Sym *		r_rsym;		/* reference symbol table entry */
	Rt_map *	r_rlmp;		/* reference link map */
	Sym *		r_dsym;		/* definition symbol table entry */
	void *		r_radd;		/* copy to address */
	const void *	r_dadd;		/* copy from address */
	unsigned long	r_size;		/* copy size bytes */
} Rel_copy;

/*
 * Data structure to hold initial file mapping information.  Used to
 * communicate during initial object mapping and provide for error recovery.
 */
typedef struct fil_map {
	int		fm_fd;		/* File descriptor */
	char *		fm_maddr;	/* Address of initial mapping */
	size_t		fm_msize;	/* Size of initial mapping */
	short		fm_mflags;	/* mmaping flags */
	size_t		fm_fsize;	/* Actual file size */
	unsigned long	fm_etext;	/* End of text segment */
} Fmap;

/*
 * File descriptor availability flag.
 */
#define	FD_UNAVAIL	-1

/*
 * Status flags for rtld_flags
 */
#define	RT_FL_THREADS	0x00000001	/* Are threads enabled */
#define	RT_FL_SEARCH	0x00000002	/* tracing search paths */
#define	RT_FL_WARN	0x00000004	/* print warnings for undefines? */
#define	RT_FL_VERBOSE	0x00000008	/* verbose (versioning) tracing */
#define	RT_FL_NOBIND	0x00000010	/* carry out plt binding? */
#define	RT_FL_NOVERSION	0x00000020	/* disable version checking? */
#define	RT_FL_SECURE	0x00000040	/* setuid/segid flag */
#define	RT_FL_APPLIC	0x00000080	/* have we started the application? */
#define	RT_FL_NOCACHE	0x00000100	/* don't process cache information */
#define	RT_FL_CACHEAVL	0x00000400	/* cache objects are available */
#define	RT_FL_DEBUGGER	0x00000800	/* a debugger is monitoring us */
#ifdef	TIMING
#define	RT_FL_TIMING	0x00001000	/* obtain timing information */
#endif
#define	RT_FL_DBNOTIF	0x00002000	/* bindings activity going on */
#define	RT_FL_DELNEEDED	0x00004000	/* link-map deletions required */
#define	RT_FL_DELINPROG	0x00008000	/* link-map deletions in progress */
#define	RT_FL_NOAUXFLTR	0x00010000	/* disable auxiliary filters */
#define	RT_FL_LOADFLTR	0x00020000	/* force loading of filtees */
#define	RT_FL_SETGROUP	0x00040000	/* groups need to be established */

/*
 * Binding flags for the bindguard routines
 */
#define	THR_FLG_BIND	0x00000001	/* BINDING bindguard flag */
#define	THR_FLG_MALLOC	0x00000002	/* MALLOC bindguard flag */
#define	THR_FLG_PRINT	0x00000004	/* PRINT bindguard flag */
#define	THR_FLG_BOUND	0x00000008	/* BOUNDTO bindguard flag */
#define	THR_FLG_STRERR	0x00000010	/* libc:strerror bindguard flag */
#define	THR_FLG_MASK	THR_FLG_BIND | THR_FLG_MALLOC | \
			THR_FLG_PRINT | THR_FLG_BOUND
					/* mask for all THR_FLG flags */

/*
 * Macro to control librtld_db interface information.
 */
#define	rd_event(e, s, func) \
	r_debug.r_state = (r_state_e)s; \
	r_debug.r_rdevent = e; \
	func; \
	r_debug.r_rdevent = RD_NONE;


#ifdef	TIMING
/*
 * Various hi-res timer macros for use in analyzing ld.so.1's performance.
 * GET_TIME can use gethrtime(3c) or gethrvtime(3c), that latter requires
 * the process to be initialized correctly, the easiest was is to run the
 * process via ptime(1).
 */

#include	<sys/time.h>

/*
 * Timing values and associated comments are stored in an __r_times[] array.
 * Normally a couple of hundred entries is sufficient (TIM_FLG_SIZE), however
 * if something like the relocation loop is instrumented this array might have
 * to be very large (or collect only a portion of the total relocation loop
 * output).
 */
typedef	struct rt_times {
	union {
		hrtime_t	hr;
		long		lg[2];
	} tm_u;
	const char *	tm_d;
} Rt_times;

#define	TIM_FLG_SIZE	1000

#define	DEF_TIME(x)	hrtime_t x
#define	_GET_TIME(x)	x = gethrtime()
#define	GET_TIME(x)	if (rtld_flags & RT_FL_TIMING) { \
				x = gethrtime(); \
			}

#define	_SAV_TIME(x, y)	_r_times->tm_u.hr = x; \
			_r_times->tm_d = y; \
			_r_times++
#define	SAV_TIME(x, y)	if (rtld_flags & RT_FL_TIMING) { \
				_r_times->tm_u.hr = x; \
				_r_times->tm_d = y; \
				_r_times++; \
			}

extern	Rt_times *	_r_times;
extern	Rt_times	__r_times[];

extern	void		r_times();

#else

#define	DEF_TIME(x)
#define	_GET_TIME(x)
#define	GET_TIME(x)
#define	_SAV_TIME(x, y)
#define	SAV_TIME(x, y)

#endif

/*
 * Data declarations.
 */
extern rwlock_t		bindlock;	/* readers/writers binding lock */
extern rwlock_t		malloclock;	/* readers/writers malloc lock */
extern rwlock_t		printlock;	/* readers/writers print lock */
extern rwlock_t		boundlock;	/* readers/writers BOUNDTO lock */
extern rwlock_t		protolock;	/* readers/writers PROTO lock */
extern mutex_t *	profilelock;	/* mutex lock for profiling */
extern int		lc_version;	/* current version of libthread int. */

extern unsigned long	flags;		/* machine specific file flags */
extern List		preload;	/* preloadable file list */
extern List		dump;		/* dump file list */
extern const char *	dump_dir;	/* location for dumped objects */
extern const char *	pr_name;	/* file name of executing process */
extern struct r_debug	r_debug;	/* debugging information */
extern Rtld_db_priv	rtld_db_priv;	/* rtld/rtld_db information */
extern char *		lasterr;	/* string describing last error */
extern Interp *		interp;		/* ELF executable interpreter info */
extern const char *	rt_name;	/* name of the dynamic linker */
extern int		bind_mode;	/* object binding mode (RTLD_LAZY?) */
extern const char *	envdirs;	/* env variable LD_LIBRARY_PATH */
extern Pnode *		envlist;	/*	and its associated Pnode list */
extern int		tracing;	/* tracing loaded objects? */
extern size_t		syspagsz;	/* system page size */
extern char *		isa; 		/* architecture name */
extern int		isa_sz; 	/* architecture string size */
extern char *		platform; 	/* platform name */
extern int		platform_sz; 	/* platform string size */
extern int		rtld_flags;	/* status flags for RTLD */
extern int		cachelinesz;	/* cache line size for ppc */
extern void *		rtld_lib;	/* handle for support library */
extern Fmap *		fmap;		/* Initial file mapping info */
extern Rtc_head *	cachehead;	/* head of the cache structure */

extern Fct		elf_fct;	/* ELF file class dependent data */
extern Fct		aout_fct;	/* a.out (4.x) file class dependent */
					/*	data */

extern const char * 	locale;		/* locale environment setting */

extern const char *	cd_dir;		/* Cache directory */
extern const char *	cd_file;	/* Cache diagnostic file */

extern const char *	ldso_path;
extern const char *	ldso_name;

extern char **		_environ;

#ifdef	DEBUG
extern const char *	dbg_str;	/* debugging tokens */
extern const char *	dbg_file;	/* debugging directed to a file */
#endif

/*
 * Function declarations.
 */
extern void		addfree(void *, size_t);
extern int		analyze_so(Lm_list *, Rt_map *);
extern Fct *		are_u_this(const char *);
extern int		bind_guard(int);
extern int		bind_clear(int);
extern int		bound_add(int, Rt_map *, Rt_map *);
extern void		call_fini(void);
extern void		call_init(Rt_map *);
extern unsigned long	caller();
extern void *		calloc(size_t, size_t);
extern int		cd_open();
extern void		cleanup();
extern int		dbg_setup(const char *);
extern int		dlclose_core(Rt_map *, Dl_obj *);
extern char *		dlerror(void);
extern Dl_obj *		dlp_create(Rt_map *, Rt_map *, int);
extern void		dlp_delete(Dl_obj *);
extern void		dlp_free(Dl_obj *);
extern void *		dlsym_core(void *, const char *, Rt_map *);
extern void *		dlused_core(const char *, int mode, Rt_map *);
extern Dl_obj *		dl_new_so(const char *, Rt_map *, Rt_map **, int);
extern int		doprf(const char *, va_list, char *);
extern void		dz_init(int);
extern int		dz_open();
extern int		elf_cache(const char **);
extern unsigned long	elf_hash(const char *);
extern void		eprintf(Error, const char *, ...);
extern int		expand(const char **, int *, Rt_map *, int);
extern Fmap *		fm_init();
extern Pnode *		get_next_dir(Pnode **, Rt_map *);
extern Rt_map *		is_so_loaded(Lm_list *, const char *);
extern Listnode *	list_append(List *, const void *);
extern void		lm_append(Lm_list *, Rt_map *);
extern Rt_map *		load_so(Lm_list *, const char *, Rt_map *);
extern int		nu_open();
extern void *		malloc(size_t);
extern Pnode *		make_pnode_list(const char *, int, Pnode *, Rt_map *);
extern void		perm_free(Permit *);
extern Permit *		perm_get();
extern int		perm_test(Permit *, Permit *);
extern Permit *		perm_set(Permit *, Permit *);
extern Permit *		perm_unset(Permit *, Permit *);
extern int		pr_open();
extern int		readenv(const char **, int);
extern int		relocate_so(Rt_map *);
extern void		remove_so(Lm_list *);
extern int		rt_atfork(void (*)(void), void (*)(void),
				void (*)(void));
extern int		rt_mutex_lock(mutex_t *, sigset_t *);
extern int		rt_mutex_unlock(mutex_t *, sigset_t *);
extern void		rtld_db_dlactivity(void);
extern void		rtld_db_preinit(void);
extern void		rtld_db_postinit(void);
extern void		security(uid_t, uid_t, gid_t, gid_t);
extern int		setup(Rt_map *, unsigned long, unsigned long);
extern int		so_find(const char *, Rt_map *);
extern const char *	so_gen_path(Pnode *, char *, Rt_map *);
extern void		zero(caddr_t, int, int);

extern long		_sysconfig(int);

/*
 * Maximum range of Thread_Interface functions that ld.so.1 is
 * interested in.
 */
#define	LC_MAX	11

#endif
