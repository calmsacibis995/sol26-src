/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)old_notes.c	1.1	96/06/18 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/thread.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/errno.h>
#include <sys/vnode.h>
#include <sys/mman.h>
#include <sys/kmem.h>
#include <sys/proc.h>
#include <sys/pathname.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/elf.h>
#include <sys/vmsystm.h>
#include <sys/debug.h>
#include <sys/old_procfs.h>
#ifdef sparc
#include <sys/elf_SPARC.h>
#endif /* sparc */
#ifdef i386
#include <sys/elf_386.h>
#endif /* i386 */
#include <sys/auxv.h>
#include <sys/exec.h>
#include <sys/prsystm.h>

extern void oprgetstatus(kthread_t *, prstatus_t *);
extern void oprgetpsinfo(proc_t *, prpsinfo_t *, kthread_t *);

#include <vm/as.h>
#include <vm/rm.h>
#include <sys/modctl.h>
#include <sys/systeminfo.h>

int elfnote(vnode_t *, off_t *, int, int, caddr_t, rlim64_t, struct cred *);

typedef struct {
	Elf32_Nhdr	Nhdr;
	char		name[8];
} Elf32_Note;

void
setup_old_note_header(Elf32_Phdr *v, proc_t *p)
{
	int nlwp = p->p_lwpcnt + p->p_zombcnt;
	u_int size;

	v[0].p_type = PT_NOTE;
	v[0].p_flags = PF_R;
	v[0].p_filesz = (sizeof (Elf32_Note) * (3 + nlwp))
	    + roundup(sizeof (prpsinfo_t), sizeof (Elf32_Word))
	    + roundup(strlen(platform) + 1, sizeof (Elf32_Word))
	    + roundup(sizeof (u.u_auxv), sizeof (Elf32_Word))
	    + nlwp * roundup(sizeof (prstatus_t), sizeof (Elf32_Word));
	if (prhasfp())
		v[0].p_filesz += nlwp * sizeof (Elf32_Note)
		    + nlwp*roundup(sizeof (prfpregset_t), sizeof (Elf32_Word));
	if ((size = prhasx()? prgetprxregsize() : 0) != 0)
		v[0].p_filesz += nlwp * sizeof (Elf32_Note)
		    + nlwp * roundup(size, sizeof (Elf32_Word));

#if defined(sparc) || defined(__sparc)
	/*
	 * Figure out the number and sizes of register windows.
	 */
	{
		kthread_t *t = p->p_tlist;
		do {
			if ((size = prnwindows(ttolwp(t))) != 0) {
				size = sizeof (gwindows_t) -
				    (SPARC_MAXREGWINDOW - size) *
				    sizeof (struct rwindow);
				v[0].p_filesz += sizeof (Elf32_Note) +
				    roundup(size, sizeof (Elf32_Word));
			}
		} while ((t = t->t_forw) != p->p_tlist);
	}
#endif /* sparc */
}

int
write_old_elfnotes(proc_t *p, int sig,
	vnode_t *vp, off_t *offsetp, rlim64_t rlimit, cred_t *credp)
{
	union {
		prpsinfo_t	psinfo;
		prstatus_t	prstat;
		prfpregset_t	fpregs;
#if defined(sparc) || defined(__sparc)
		gwindows_t	gwindows;
#endif /* sparc */
		char		xregs[1];
	} *bigwad;
	u_int xregsize = prhasx()? prgetprxregsize() : 0;
	u_int bigsize = max(sizeof (*bigwad), xregsize);
	kthread_t *t;
	klwp_t *lwp;
	int nlwp;
	int error;

	bigwad = kmem_alloc(bigsize, KM_SLEEP);

	/*
	 * The order of the elfnote entries should be same here
	 * and in the gcore(1) command.  Synchronization is
	 * needed between the kernel and gcore(1).
	 */

	mutex_enter(&p->p_lock);
	oprgetpsinfo(p, &bigwad->psinfo, NULL);
	mutex_exit(&p->p_lock);
	error = elfnote(vp, offsetp, NT_PRPSINFO, sizeof (bigwad->psinfo),
	    (caddr_t)&bigwad->psinfo, rlimit, credp);
	if (error)
		goto done;

	error = elfnote(vp, offsetp, NT_PLATFORM, strlen(platform) + 1,
	    platform, rlimit, credp);
	if (error)
		goto done;

	error = elfnote(vp, offsetp, NT_AUXV, sizeof (PTOU(p)->u_auxv),
	    (caddr_t)PTOU(p)->u_auxv, rlimit, credp);
	if (error)
		goto done;

	t = curthread;
	nlwp = p->p_lwpcnt;
	do {
		ASSERT(nlwp != 0);
		nlwp--;
		lwp = ttolwp(t);

		mutex_enter(&p->p_lock);
		if (t == curthread) {
			/*
			 * Restore current signal information
			 * in order to get a correct prstatus.
			 */
			lwp->lwp_cursig = (u_char)sig;
			t->t_whystop = PR_FAULTED;	/* filthy */
			oprgetstatus(t, &bigwad->prstat);
			bigwad->prstat.pr_why = 0;
			t->t_whystop = 0;
			lwp->lwp_cursig = 0;
		} else {
			oprgetstatus(t, &bigwad->prstat);
		}
		mutex_exit(&p->p_lock);
		error = elfnote(vp, offsetp, NT_PRSTATUS,
		    sizeof (bigwad->prstat), (caddr_t)&bigwad->prstat,
		    rlimit, credp);
		if (error)
			goto done;

		if (prhasfp()) {
			prgetprfpregs(lwp, &bigwad->fpregs);
			error = elfnote(vp, offsetp, NT_PRFPREG,
			    sizeof (bigwad->fpregs), (caddr_t)&bigwad->fpregs,
			    rlimit, credp);
			if (error)
				goto done;
		}

#if defined(sparc) || defined(__sparc)
		/*
		 * Unspilled SPARC register windows.
		 */
		{
			u_int size = prnwindows(lwp);

			if (size != 0) {
				size = sizeof (gwindows_t) -
				    (SPARC_MAXREGWINDOW - size) *
				    sizeof (struct rwindow);
				prgetwindows(lwp, &bigwad->gwindows);
				error = elfnote(vp, offsetp, NT_GWINDOWS,
				    size, (caddr_t)&bigwad->gwindows,
				    rlimit, credp);
				if (error)
					goto done;
			}
		}
#endif /* sparc */

		if (xregsize) {
			prgetprxregs(lwp, bigwad->xregs);
			error = elfnote(vp, offsetp, NT_PRXREG,
			    xregsize, bigwad->xregs, rlimit, credp);
			if (error)
				goto done;
		}
	} while ((t = t->t_forw) != curthread);
	ASSERT(nlwp == 0);

	/* dump out zombied LWPs */
	nlwp = p->p_zombcnt;
	for (t = p->p_zomblist; nlwp-- > 0; t = t->t_forw) {
		lwp = ttolwp(t);

		mutex_enter(&p->p_lock);
		oprgetstatus(t, &bigwad->prstat);
		mutex_exit(&p->p_lock);
		error = elfnote(vp, offsetp, NT_PRSTATUS,
		    sizeof (bigwad->prstat), (caddr_t)&bigwad->prstat,
		    rlimit, credp);
		if (error)
			goto done;

		if (prhasfp()) {
			prgetprfpregs(lwp, &bigwad->fpregs);
			error = elfnote(vp, offsetp, NT_PRFPREG,
			    sizeof (bigwad->fpregs), (caddr_t)&bigwad->fpregs,
			    rlimit, credp);
			if (error)
				goto done;
		}

		if (xregsize) {
			prgetprxregs(lwp, bigwad->xregs);
			error = elfnote(vp, offsetp, NT_PRXREG,
			    xregsize, bigwad->xregs, rlimit, credp);
			if (error)
				goto done;
		}
	}
	ASSERT(t == p->p_zomblist);

done:
	kmem_free(bigwad, bigsize);
	return (error);
}
