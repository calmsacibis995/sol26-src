/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_PRSYSTM_H
#define	_SYS_PRSYSTM_H

#pragma ident	"@(#)prsystm.h	1.23	96/08/08 SMI"	/* SVr4.0 1.4	*/

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(_KERNEL)

extern kmutex_t pr_pidlock;
extern kcondvar_t *pr_pid_cv;

struct prfpregset;
struct pstatus;
struct lwpstatus;
struct psinfo;
struct lwpsinfo;

/*
 * These are functions in the procfs module that are
 * called from the kernel proper and from other modules.
 */
struct seg;
struct regs;
struct watched_page;
extern u_int pr_getprot(struct seg *, int, void **, caddr_t *, caddr_t *);
extern void prinvalidate(struct user *);
extern void prgetstatus(proc_t *, struct pstatus *);
extern void prgetlwpstatus(kthread_t *, struct lwpstatus *);
extern void prgetpsinfo(proc_t *, struct psinfo *);
extern void prgetlwpsinfo(kthread_t *, struct lwpsinfo *);
extern void prgetprfpregs(klwp_t *, struct prfpregset *);
extern void prgetprxregs(klwp_t *, caddr_t);
extern int  prgetprxregsize(void);
extern int  prnsegs(struct as *, int);
extern void prfree(proc_t *);
extern void prexit(proc_t *);
extern void prlwpexit(kthread_t *);
extern void prexecstart(void);
extern void prexecend(void);
extern void prrelvm(void);
extern void prbarrier(proc_t *);
extern void prstop(int, int);
extern void prnotify(struct vnode *);
extern void prstep(klwp_t *, int);
extern void prnostep(klwp_t *);
extern void prdostep(void);
extern int  prundostep(void);
extern int  prhasfp(void);
extern int  prhasx(void);
extern caddr_t prmapin(struct as *, caddr_t, int);
extern void prmapout(struct as *, caddr_t, caddr_t, int);
extern int  pr_watch_emul(struct regs *, caddr_t, enum seg_rw);
extern void pr_free_my_pagelist(void);
#if defined(sparc) || defined(__sparc)
struct gwindows;
int		prnwindows(klwp_t *);
void		prgetwindows(klwp_t *, struct gwindows *);
#endif

#endif	/* defined (_KERNEL) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PRSYSTM_H */
