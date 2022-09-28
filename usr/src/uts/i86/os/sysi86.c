/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.
 */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1987, 1988 Microsoft Corporation	*/
/*	  All Rights Reserved	*/

/*	This Module contains Proprietary Information of Microsoft  */
/*	Corporation and should be treated as Confidential.	   */

#pragma ident "@(#)sysi86.c	1.26	96/06/03 SMI"
/* from SVR4: sysi86.c 1.3.2.2 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/errno.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <sys/cpuvar.h>
#include <sys/sysi86.h>
#include <sys/v86.h>
#include <sys/psw.h>
#include <sys/cred.h>
#include <sys/thread.h>
#include <sys/debug.h>

#include <sys/map.h>
#include <sys/kmem.h>
#include <sys/mman.h>
#include <sys/archsystm.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <vm/faultcode.h>
#include <sys/vmmac.h>
#include <sys/fp.h>

static int v86syscall(void);
static int setdscr(int ap);
static int v86iopl(long arg);
static int setup_ldt(proc_t *pp);
static int ldt_map(proc_t *pp, int seli);
static void load_ldt_default(void);

extern void rtcsync(void);
extern long ggmtl(void);
extern void sgmtl(long);

extern struct seg_desc ldt_default[];
extern struct seg_desc default_ldt_desc;

/*
 *	sysi86 System Call
 */

struct sysi86 {
	int	cmd;
	int	arg1;
	int	arg2;
	int	arg3;
};

/* ARGSUSED */
int
sysi86(uap, rvp)
	register struct sysi86 *uap;
	rval_t *rvp;
{
	int	error = 0;
	register int	c;

	switch ((short)uap->cmd) {

		/*
		 * The SI86V86 subsystem call of the SYSI86 system call has
		 * sub system calls defined for it v86.h.
		 */
		case SI86V86:
			return (v86syscall());	/* Process V86 system call */

#ifdef MERGE386
		/*
		 *  VM86 command for Merge 386 functions
		 */
		case SI86VM86:
			error = vm86(uap->arg1, rvp);
			break;
#endif

		/*
		 * Set a segment descriptor
		 */
		case SI86DSCR:
			error = setdscr(uap->arg1);
			break;

		case SI86FPHW:
			c = fp_kind & 0xFF;
#ifdef WEITEK
			c |= ((weitek_kind & 0xFF) << 8);
#endif
			if (suword((int *)uap->arg1, c)  == -1)
				error = EFAULT;
			break;

	/* real time clock management commands */

		case WTODC:
			if (!suser(CRED()))
				error = EPERM;
			else {
				timestruc_t ts;
				mutex_enter(&tod_lock);
				gethrestime(&ts);
				tod_set(ts);
				mutex_exit(&tod_lock);
			}
			break;

		case SGMTL:
			if (!suser(CRED()))
				error = EPERM;
			else
				sgmtl(uap->arg1);
			break;

		case GGMTL:
			if (suword((int *)uap->arg1, ggmtl()) == -1)
				error = EFAULT;
			break;

		case RTCSYNC:
			if (!suser(CRED()))
				error = EPERM;
			else
				rtcsync();
			break;

	/* END OF real time clock management commands */

		default:
			error = EINVAL;
	}
	return (error);
}

/*
 *  v86syscall() - SI86V86 subsystem call of SYSI86 system call.
 *  The v86 system call is itself a subsystem call of sysi86 system
 *  call. The subsystem calls of SI86V86 are defined in v86.h and
 *  processed here.
 */

static int
v86syscall(void)
{
	klwp_id_t lwp = ttolwp(curthread);

	register struct a {
		unsigned long	sysi86cmd;	/* SI86V86 number */
		unsigned long	v86cmd;		/* V86 subsystem call */
		long		arg1;		/* Access first argument */
		long		arg2;		/* Rest of the arguments */
	} *uap = (struct a *)lwp->lwp_ap;

	switch (uap->v86cmd) {		/* Process sub system calls */
#ifdef _VPIX
	case V86SC_INIT:		/* v86init() system call   */
		return (v86init((caddr_t)(uap->arg1)));

	case V86SC_SLEEP:		/* v86sleep() system call  */
		return (v86sleep());

	case V86SC_MEMFUNC:		/* v86memfunc() sys call   */
		return (v86memfunc((caddr_t)(uap->arg1)));
#endif

	case V86SC_IOPL:		/* v86iopl() system call */
		return (v86iopl(uap->arg1));

	default:			/* Invalid system call	*/
		return (EINVAL);
	}
}

/*
 * Allow a process to play  with  its  io  privilege  level,
 * allowing the process to do ins and outs.
 */

static int
v86iopl(long arg)
{
	klwp_id_t lwp = ttolwp(curthread);
	struct regs *rp = lwptoregs(lwp);

	/*
	 * Must be privileged to run this system call if giving more
	 * io privilege.
	 */

	if (((rp->r_ps & PS_IOPL) < (arg & PS_IOPL)) && !suser(CRED()))
		return (EPERM);
	else
		rp->r_ps = (rp->r_ps & ~PS_IOPL) | (arg & PS_IOPL);
	return (0);
}

/*
 *  SI86DSCR:
 *  Set a segment or gate descriptor.
 *  The following are accepted:
 *	executable and data segments in the LDT at DPL 3
 *  The request structure is declared in sysi86.h.
 */

/* call gate structure */
struct cg {
	unsigned short off1; /* low order word of offset */
	unsigned short sel;  /* descriptor selector */
	unsigned char  cnt;  /* word count */
	unsigned char  acc1; /* access byte */
	unsigned short off2; /* high order word of offset */
};

static int
setdscr(int ap)
{
	struct ssd ssd;		/* request structure buffer */
	u_short seli;  		/* selector index */
	struct dscr *dscrp;	/* descriptor pointer */
	struct gdscr *gdscrp;   /* descriptor pointer */
	proc_t	*pp = ttoproc(curthread);
	klwp_id_t lwp = ttolwp(curthread);

	if ((copyin((caddr_t)ap, (caddr_t)&ssd, sizeof (struct ssd))) < 0) {
		return (EFAULT);
	}

	/* LDT segments: executable and data at DPL 3 only */

	if (ssd.sel & 4) {	/* test TI bit */
		/* check the selector index */
		seli = seltoi(ssd.sel);
		if (seli >= MAXLDTSZ)
			return (EINVAL);

		mutex_enter(&pp->p_ldtlock);
		/*
		 * If this is the first time for this process then setup a
		 * private LDT for it.
		 */
		if (pp->p_ldt == NULL) {
			if (setup_ldt(pp) == NULL) {
				mutex_exit(&pp->p_ldtlock);
				return (ENOMEM);
			}
			loadldt(LDTSEL);
		}

		if (ldt_map(pp, seli) == NULL) {
			mutex_exit(&pp->p_ldtlock);
			return (ENOMEM);
		}

		ASSERT(seli <= pp->p_ldtlimit);
		dscrp = (struct dscr *)(pp->p_ldt) + seli;
		/* if acc1 is zero, clear the descriptor */
		if (!ssd.acc1) {
			((unsigned int *)dscrp)[0] = 0;
			((unsigned int *)dscrp)[1] = 0;
			mutex_exit(&pp->p_ldtlock);
			return (0);
		}
		/* check segment type, allow segment not present state */

		if ((ssd.acc1 & 0x70) == 0x70) {		/* code, data */
			/* set up the descriptor */
			setdscrbase(dscrp, ssd.bo);
			setdscrlim(dscrp, ssd.ls);
			setdscracc1(dscrp, ssd.acc1);
			setdscracc2(dscrp, ssd.acc2);
			mutex_exit(&pp->p_ldtlock);

		} else if ((ssd.acc1 & 0x7f) == 0x6c) {		/* call gate */
			gdscrp = (struct gdscr *)dscrp;
			gdscrp->gd_off0015 = ssd.bo;
			gdscrp->gd_off1631 = ssd.bo >> 16;
			gdscrp->gd_selector = ssd.ls;
			gdscrp->gd_unused = ssd.acc2;
			gdscrp->gd_acc0007 = ssd.acc1;
			mutex_exit(&pp->p_ldtlock);
		}

		else {
			mutex_exit(&pp->p_ldtlock);
			return (EINVAL);
		}

		/*
		 * Set the flag lwp_gpfault to catch GP faults when going back
		 * to user mode. XXX - needed?
		 */
		lwp->lwp_gpfault = 1;
		return (0);
	}

#ifdef XXX_later
	/* GDT segment: call gate into LDT at DPL 3 only */

	else {
		struct cg *cgp;		/* call gate pointer */

		seli = seltoi(ssd.sel);
		if (seli <= 25 || seli >= GDTSZ)
			goto bad;
		switch (seli) {
		case 40:
		case 41:
		case 42:
		case 43:
		case 44:
		case 45:
		case 46:
		case 50:
			goto bad;
		default:
			break;
		}

		/* if acc1 is zero, clear the descriptor and U-struct */
		cgp = (struct cg *)gdt + seli;
		if (!ssd.acc1) {
			((unsigned int *)cgp)[0] = 0;
			((unsigned int *)cgp)[1] = 0;
			u.u_callgatep = 0;
			u.u_callgate[0] = 0;
			u.u_callgate[1] = 0;
			return (0);
		}

		/* check that a call gate does not already exist */
		if (u.u_callgatep != 0)
			goto bad;

		/* check that call gate points to an LDT descriptor */
		if (((ssd.acc1 & 0xF7) != 0xE4) ||
		    (!(ssd.ls & 4)))	/* LDT */
			goto bad;

		cgp->off1 = ssd.bo;
		cgp->sel  = ssd.ls;
		cgp->cnt  = ssd.acc2;
		cgp->acc1 = ssd.acc1;
		cgp->off2 = ((unsigned short *)&ssd.bo)[1];

		/* copy call gate and its pointer into the user structure */
		u.u_callgatep = (int *)cgp;
		u.u_callgate[0] = ((int *)cgp)[0];
		u.u_callgate[1] = ((int *)cgp)[1];
	}
	return (0);
bad:
#endif

	return (EINVAL);
}

/*
 * setup_ldt():
 *	Allocate a private LDT for this process and initialize it with the
 *	default entries.
 *	Returns 0 for errors, pointer to LDT for success.
 */

static int
setup_ldt(proc_t *pp)
{
	struct dscr *ldtp;	/* descriptor pointer */
	u_long a;
	struct	seg_desc *gdt;

	ASSERT(pp->p_ldt == NULL);

	/*
	 * Allocate maximum virtual space we need for this LDT.
	 */
	a = rmalloc_wait(kernelmap, btopr(MAXLDTSZ * sizeof (struct dscr)));
	ldtp = (struct dscr *)kmxtob(a);

	/*
	 * Allocate the minimum number of physical pages for LDT.
	 */
	if (segkmem_alloc(&kvseg, (caddr_t)ldtp,
	    MINLDTSZ * sizeof (struct dscr), 1) == 0) {
		rmfree(kernelmap, btopr(MAXLDTSZ * sizeof (struct dscr)), a);
		return (0);
	}
	bzero((caddr_t)ldtp, ptob(btopr(MINLDTSZ * sizeof (struct dscr))));

	/*
	 * Copy the default LDT entries into the new table.
	 */
	bcopy((caddr_t)ldt_default, (caddr_t)ldtp,
			MINLDTSZ * sizeof (struct dscr));

	kpreempt_disable();
	/* Update proc structure. XXX - need any locks here??? */
	setdscrbase(&pp->p_ldt_desc, (u_int)ldtp);
	setdscrlim(&pp->p_ldt_desc, MINLDTSZ * sizeof (struct dscr) - 1);
	setdscracc1(&pp->p_ldt_desc, LDT_KACC1);
	setdscracc2(&pp->p_ldt_desc, LDT_ACC2);
	pp->p_ldtlimit = MINLDTSZ-1;
	pp->p_ldt = (struct seg_desc *)ldtp;
	if (pp == curproc) {
		gdt = CPU->cpu_gdt;
		gdt[seltoi(LDTSEL)] = pp->p_ldt_desc;
	}
	kpreempt_enable();

	return ((int)ldtp);
}

/*
 * ldt_map():
 *	Map the page corresponding to the selector entry. If the page is
 *	already mapped then it simply returns with the pointer to the entry.
 *	Otherwise it allocates a physical page for it and returns the pointer
 *	to the entry.
 *	Returns 0 for errors.
 */

static int
ldt_map(proc_t *pp, int seli)
{
	int olimit = pp->p_ldtlimit;
	volatile caddr_t ent_addr;
	label_t *saved_jb;
	label_t jb;

	ASSERT(pp->p_ldt != NULL);

	ent_addr = (caddr_t)&pp->p_ldt[seli];
	saved_jb = curthread->t_nofault;
	curthread->t_nofault = &jb;

	if (!setjmp(&jb)) {
#ifdef bug1139151
		/*LINTED*/ /* since they aren't going to fix it */
		int val = *(int *)ent_addr; /* peek at the address */
#else
		(void) *(int *)ent_addr; /* peek at the address */
#endif
	} else {	/* Allocate a physical page */
		caddr_t base = (caddr_t)((int)ent_addr & (~PAGEOFFSET));

		if (segkmem_alloc(&kvseg, base, PAGESIZE, 1) == 0) {
			curthread->t_nofault = saved_jb;
			return (0);
		}
		bzero((caddr_t)base, PAGESIZE);
	}
	curthread->t_nofault = saved_jb;

	/* XXX - need any locks to update proc_t or gdt ??? */
	if ((u_int)seli > olimit) { /* update the LDT limit */
		struct	seg_desc *gdt;

		kpreempt_disable();
		gdt = CPU->cpu_gdt;
		pp->p_ldtlimit = seli;
		setdscrlim(&pp->p_ldt_desc, (seli+1) * sizeof (struct dscr) -1);
		gdt[seltoi(LDTSEL)] = pp->p_ldt_desc;
		loadldt(LDTSEL);
		kpreempt_enable();
	}
	return ((int)ent_addr);
}

/*
 * Reload LDT register during a thread switch. If the new thread has a
 * seperate LDT then load LDT register with it otherwise use default
 * LDT.
 * Now being done in swtch.s (resume)
 */

/*
 * ldt_free():
 *	Free up the kernel memory used for LDT of this process.
 */

void
ldt_free(proc_t *pp)
{
	label_t *saved_jb;
	label_t jb;
	caddr_t start, end;
	volatile caddr_t addr;

	ASSERT(pp->p_ldt != NULL);

	mutex_enter(&pp->p_ldtlock);
	start = (caddr_t)pp->p_ldt; /* beginning of the LDT */
	end = start + (pp->p_ldtlimit * sizeof (struct dscr));
	saved_jb = curthread->t_nofault;
	curthread->t_nofault = &jb;

	/* Free the physical page(s) used for mapping LDT */
	for (addr = start; addr <= end; addr += PAGESIZE) {
		if (!setjmp(&jb)) {
#ifdef bug1139151
			/*LINTED*/ /* since they aren't going to fix it */
			int val = *(int *)addr; /* peek at the address */
#else
			(void) *(int *)addr; /* peek at the address */
#endif
			segkmem_free(&kvseg, addr, PAGESIZE);
		} else  {
			curthread->t_nofault = &jb;
			continue;
		}
	}
	curthread->t_nofault = saved_jb;
	/* Free up the virutal address space used for this LDT */
	rmfree(kernelmap, btop(MAXLDTSZ * sizeof (struct dscr)),
					btokmx((ulong_t)pp->p_ldt));
	kpreempt_disable();
	pp->p_ldt = NULL;
	pp->p_ldt_desc = default_ldt_desc;
	if (pp == curproc)
		load_ldt_default();
	kpreempt_enable();
	mutex_exit(&pp->p_ldtlock);
}

/* Load LDT register with the default LDT */

static void
load_ldt_default(void)
{

	CPU->cpu_gdt[seltoi(LDTSEL)] = default_ldt_desc;
	loadldt(LDTSEL);
}

int
ldt_dup(
	register proc_t *pp,  /* parent proc */
	register proc_t *cp) /* child proc */
{
	label_t *saved_jb;
	label_t jb;
	caddr_t start, end;
	volatile caddr_t addr;
	caddr_t caddr;
	int	minsize;

	if (pp->p_ldt == NULL) {
		cp->p_ldt_desc = default_ldt_desc;
		return (0);
	}

	if (setup_ldt(cp) == NULL) {
		return (ENOMEM);
	}

	mutex_enter(&pp->p_ldtlock);
	cp->p_ldtlimit = pp->p_ldtlimit;
	setdscrlim(&cp->p_ldt_desc,
	    (pp->p_ldtlimit+1) * sizeof (struct dscr) -1);
	start = (caddr_t)pp->p_ldt; /* beginning of the LDT */
	end = start + (pp->p_ldtlimit * sizeof (struct dscr));
	caddr = (caddr_t)cp->p_ldt; /* child LDT start */
	saved_jb = curthread->t_nofault;
	curthread->t_nofault = &jb;

	minsize = ((MINLDTSZ * sizeof (struct dscr)) + PAGESIZE) & ~PAGEOFFSET;
	/* Walk thru the physical page(s) used for parent's LDT */
	for (addr = start; addr <= end; addr += PAGESIZE, caddr += PAGESIZE) {
		if (!setjmp(&jb)) {
#ifdef bug1139151
			/*LINTED*/ /* since they aren't going to fix it */
			int val = *(int *)addr; /* peek at the address */
#else
			(void) *(int *)addr; /* peek at the address */
#endif
			/* allocate a page if necessary */
			if (caddr >= ((caddr_t)cp->p_ldt + minsize)) {
				if (segkmem_alloc(&kvseg, caddr,
				    PAGESIZE, 1) == 0) {
					curthread->t_nofault = saved_jb;
					ldt_free(cp);
					mutex_exit(&pp->p_ldtlock);
					return (ENOMEM);
				}
			}
			bcopy((caddr_t)addr, (caddr_t)caddr, PAGESIZE);
		} else  {
			curthread->t_nofault = &jb;
			continue;
		}
	}
	curthread->t_nofault = saved_jb;
	mutex_exit(&pp->p_ldtlock);
	return (0);
}
