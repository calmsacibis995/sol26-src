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
#pragma ident	"@(#)globals.c	1.39	96/09/11 SMI"

#include	<signal.h>
#include	<dlfcn.h>
#include	<synch.h>
#include	"_rtld.h"
#include	"msg.h"

/*
 * Declarations of global variables used in ld.so.
 */
int		bind_mode =	RTLD_LAZY;
rwlock_t	bindlock =	DEFAULTRWLOCK;
rwlock_t	malloclock =	DEFAULTRWLOCK;
rwlock_t	printlock =	DEFAULTRWLOCK;
rwlock_t	boundlock =	DEFAULTRWLOCK;
rwlock_t	protolock =	DEFAULTRWLOCK;	/* used to re-init the */
						/* above locks */
mutex_t *	profilelock =	0;

/*
 * Major link-map lists.
 */
Lm_list		lml_main =	{ 0, 0 };	/* the `main's link map list */
Lm_list		lml_rtld =	{ 0, 0 };	/* rtld's link map list */

Lm_list *	lml_list[] =	{ &lml_main, &lml_rtld, 0 };

/*
 * Debugger interface structure.
 */
struct r_debug r_debug = {
	R_DEBUG_VERSION,			/* version no. */
	0,					/* r_map */
	(unsigned long)rtld_db_dlactivity,	/* r_brk */
	RT_CONSISTENT,				/* r_state */
	0,					/* r_ldbase */
	0,					/* r_ldsomap */
	RD_NONE,				/* r_rdevent */
	RD_FL_NONE				/* r_flags */
};

/*
 * Private structure for passing of information between librltd_db
 * and rtld.
 *
 * Note:  Any data that's being 'exported' to librtld_db must not
 *	  require any 'relocations' before it can be examined.  That's
 *	  because librtld_db will examine this structure before rtld has
 *	  started to execute (and before it's relocated itself).  So - all
 *	  data in this structure must be available at that point.
 */
Rtld_db_priv	rtld_db_priv = {
	R_RTLDDB_VERSION,			/* version no. */
	0					/* rtd_objpad */
};

const char *	pr_name =		/* Initialize the process name */
		MSG_ORIG(MSG_STR_UNKNOWN); /*	incase exec() is called */
					/*	with a null argv[0] */
					/*	specified. */
int		lc_version = 0;		/* ld_concurrency() version number */
const char *	rt_name;		/* the run time linkers name */
char *		lasterr = (char *)0;	/* string describing last error */
					/*	cleared by each dlerror() */
Interp *	interp = 0;		/* ELF interpreter info */
List		preload = { 0, 0 };	/* LD_PRELOAD objects */
const char *	envdirs = 0;		/* LD_LIBRARY_PATH and its */
Pnode *		envlist = 0;		/*	associated Pnode list */
size_t		syspagsz = 0;		/* system page size */
unsigned long	flags = 0;		/* machine specific file flags */
int		tracing = 0;		/* tracing loaded objects? */
#ifdef	AT_SUN_EXECNAME
char *		isa = 0;		/* machine name from AT_SUN_???? */
int		isa_sz = 0;		/* machine string length */
#endif
char *		platform = 0;		/* platform name from AT_SUN_PLATFORM */
int		platform_sz = 0;	/* platform string length */

#ifdef	ENABLE_CACHE
int		rtld_flags = 0;		/* status flags for RTLD */
#else
int		rtld_flags = RT_FL_NOCACHE;
#endif

int		cachelinesz = 0;	/* cacheline size */
void *		rtld_lib = 0;		/* Handle for support library */
Fmap *		fmap = 0;		/* Initial file mapping info */
Rtc_head *	cachehead = 0;		/* head of the cache structure */

const char *	locale = 0;		/* locale environment definition */

const char *	cd_dir = 0;		/* Cache directory */
const char *	cd_file = 0;		/* Cache diagnostic file. */

#ifdef DEBUG
const char *	dbg_str = 0;		/* debugging tokens */
int		dbg_mask;		/* debugging classes */
const char * 	dbg_file = 0;		/* debugging directed to file */
#endif

#pragma weak	environ = _environ	/* environ for PLT tracing - we */
char **		_environ = 0;		/* supply the pair to satisfy any */
					/* libc requirements (hwmuldiv) */

#ifdef TIMING
Rt_times	__r_times[TIM_FLG_SIZE];
Rt_times *	_r_times = __r_times;
#endif
