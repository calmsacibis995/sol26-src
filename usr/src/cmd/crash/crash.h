/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *		Copyright (C) 1991  Sun Microsystems, Inc
 *			All rights reserved.
 *		Notice of copyright on this source code
 *		product does not indicate publication.
 *
 *		RESTRICTED RIGHTS LEGEND:
 *   Use, duplication, or disclosure by the Government is subject
 *   to restrictions as set forth in subparagraph (c)(1)(ii) of
 *   the Rights in Technical Data and Computer Software clause at
 *   DFARS 52.227-7013 and in similar clauses in the FAR and NASA
 *   FAR Supplement.
 */

#pragma	ident	"@(#)crash.h	1.13	96/07/28 SMI"	/* SVr4.0 1.8 */

#include <setjmp.h>
#include <string.h>
#include <nlist.h>
#include <sys/elf.h>
#include <sys/thread.h>
#include <sys/types.h>
#include <sys/proc.h>
#include <sys/kmem.h>
#include <sys/kmem_impl.h>
#include <kvm.h>
#include <nlist.h>

#include "mem.h"
#include "gc.h"

#ifndef TRUE
#define	TRUE  1
#define	FALSE 0
#endif

/*
 * Macros to read from kernel memory, checking for errors:
 *	GET_VAR(LocalVar,   KernelAddr, "Description");
 *	GET_BUF(Ptr, Bytes, KernelAddr, "Description");
 * Notice that these macros use error(), which means that they abort the
 * command if they fail.  For that reason we must use gc_malloc, etc.
 * when using these macros.
 */
#define	GET_VAR(v, addr, what)	\
    if (sizeof (v) != kvm_read(kd, (u_long)(addr), (char *)(&v), sizeof (v))) \
	error("kernel memory read error on \"%s\" at 0x%08x\n", what, addr); \
    else
#define	GET_BUF(p, n, addr, what)	\
    if (n != kvm_read(kd, (u_long)(addr), (char *)(p), n)) \
	error("kernel memory read error on \"%s\" at 0x%08x\n", what, addr); \
    else

/* This file should include only command independent declarations */

#define	ARGLEN 40	/* max length of argument */

extern FILE	*fp;		/* output file */
extern int	 Procslot;	/* current process slot number */
extern int	 Virtmode;	/* current address translation mode */
extern int	 mem;		/* file descriptor for dumpfile */
extern jmp_buf	 syn;		/* syntax error label */
extern char	*args[];	/* argument array */
extern int	 argcnt;	/* number of arguments */
extern int	 optind;	/* argument index */
extern char	*optarg;	/* getopt argument */
extern char	*strtbl;	/* pointer to string table */
extern kvm_t	*kd;		/* descriptor for accessing kernel memory */
extern struct var    vbuf;	/* tunable variables buffer */
extern gc_handle_t   temp;	/* pool for garbage-collected memory */
extern kthread_id_t  Curthread; /* pointer to current thread */

struct procslot {
	proc_t *p;
	pid_t   pid;
};

typedef struct kmem_cache_stat {
	int	kcs_buf_size;
	int	kcs_slab_size;
	int	kcs_alloc;
	int	kcs_alloc_fail;
	int	kcs_buf_constructed;
	int	kcs_buf_avail;
	int	kcs_buf_total;
	int	kcs_buf_max;
	int	kcs_slab_create;
	int	kcs_slab_destroy;
} kmem_cache_stat_t;

extern long getargs(int, long *, long *, int);	/* function to get arguments */
extern long strcon(char *, char); /* function to convert strings to long */
extern long eval(char *);	/* function to evaluate expressions */
extern Elf32_Sym *symsrch(char *);	/* function for symbol search */
extern Elf32_Sym *findsym(unsigned long);	/* function for symbol search */
extern int nl_getsym(char *, struct nlist *);
extern int proc_to_slot(long);
extern int error();

#if !defined(i386)
extern longlong_t vtop(long, int);
#endif
extern int getvtop(void);
extern int getmode(void);

extern void (*pipesig)(int);
extern struct procslot *slottab;
extern jmp_buf  jmp;

extern Elf32_Sym *Curproc, *Start, *cpu_sym;
extern Elf32_Sym *Panic, *V;

extern unsigned procv;
extern int opipe;
extern FILE *rp;
extern void sigint(int);
extern int resetfp(void);
extern long hextol(char *);
extern long stol(char *);
extern long octol(char *);
extern long btol(char *);
extern int readmem(unsigned, int, int, void *, unsigned, char *);
extern int readsym(char *, void *, unsigned);
extern int readbuf(unsigned, unsigned, int, int, void *, unsigned, char *);
extern void makeslottab(void);
extern pid_t slot_to_pid(int);
extern proc_t *slot_to_proc(int);
extern int getcurproc(void);
extern kthread_id_t getcurthread(void);
extern int procntry(int, struct proc *);
extern int getslot(long, long, int, int, long);
extern FILE *fopen_output(char *);
extern void redirect(void);
extern int putch(char);
extern int setproc(void);
extern int isasymbol(char *);
extern int range(int, long *, long *);
extern kmem_cache_t *kmem_cache_find(char *);
extern int kmem_cache_find_all(char *, kmem_cache_t **, int);
extern int kmem_cache_apply(kmem_cache_t *, void (*)(void *, void *));
extern int kmem_cache_audit_apply(kmem_cache_t *cp,
		void (*func)(void *kaddr, void *buf, u_int size,
			    kmem_bufctl_audit_t *bcp));
extern int kmem_cache_getstats(kmem_cache_t *, kmem_cache_stat_t *);

extern void init_owner(void);
extern void add_owner(kmem_bufctl_audit_t *bcp, u_int size, u_int data_size);
extern void print_owner(char *itemstr, int mem_threshold, int cnt_threshold);

extern struct user *ubp;	/* ublock pointer */
extern int active;		/* active system flag */

extern int getublock(int, long);
extern int getlwpblock(struct _kthread *, struct _klwp *);
extern unsigned setbf(long top, long bottom, int slot);
extern int getuser(void);
extern int getpcb(void);
extern int getstack(void);
extern int gettrace(void);
extern int getkfp(void);

extern int getproc(void);
extern int getdefproc(void);
extern int readsid(struct sess *);
extern int readpid(struct pid *);

extern int getbufhdr(void);
extern int getbuffer(void);
extern int getod(void);

extern char *dumpfile;
extern char *namelist;

extern int init(void);

extern void *sym2addr();
extern char *addr2sym();

extern void *try_sym2addr();
extern char *try_addr2sym();

extern int out_of_memory(size_t);

extern uint_t _kernelbase;
extern uint_t _userlimit;
extern uint_t _mmu_pagesize;
