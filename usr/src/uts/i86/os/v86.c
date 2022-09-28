/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.
 */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident "@(#)v86.c	1.21	96/10/17 SMI"	/* from SVR4/MP: v86.c 1.3.1.2 */

/*
 * Screen Scan Delay change:
 * LIM 4.0 & EMM changes:
 * Dual-Mode floating Point support:
 * Copyright (c) 1989 Phoenix Technologies Ltd.
 * All Rights Reserved
*/

#include <sys/sysmacros.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/reg.h>
#include <sys/stack.h>
#include <sys/tss.h>
#include <sys/user.h>
#include <sys/debug.h>
#include <sys/var.h>
#include <sys/ioctl.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_vpix.h>
#include <vm/seg_kmem.h>
#include <vm/faultcode.h>
#include <sys/v86.h>
#include <sys/map.h>
#include <sys/mman.h>
#include <sys/kmem.h>
#include <sys/cred.h>
#include <sys/thread.h>
#include <sys/memlist.h>
#include <sys/stream.h>
#include <sys/vmmac.h>

/* Scheduler definitions */
#include <sys/class.h>
#include <sys/ts.h>

#ifdef _VPIX

/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>

extern struct mod_ops mod_miscops;

/*
 * Module linkage information for the kernel.
 */
static struct modlmisc modlmisc = {
	&mod_miscops, "VP/ix process support"
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlmisc, NULL
};

static int module_keepcnt = 0;	/* ==0 means the module is unloadable */

_init()
{
	return (mod_install(&modlinkage));
}

_fini()
{
	if (module_keepcnt != 0)
		return (EBUSY);

	return (mod_remove(&modlinkage));
}

_info(modinfop)
	struct modinfo *modinfop;
{
	return (mod_info(&modlinkage, modinfop));
}

static int _v86physmap();
int v86swtch();
int v86rtt();
int v86exit();
int v86sighdlint();

/*
**  VP/ix pseudorupt handling:
**
**	A non-VP/ix thread cannot become a VP/ix thread except via a
**	successful call to v86init().  A  VP/ix thread cannot become
**	a non-VP/ix thread except via a call to v86exit().  These calls
**	are never made from interrupt code.  A thread is considered to
**	be a VP/ix thread when its t_v86data is non-null.
**
**	Device drivers register themselves for delivering pseudorupts to
**	VP/ix processes by calling v86stash() which saves the necessary
**	information in a v86int_t supplied by the device driver.  Part
**	of the logic of v86stash() is to add the v86int_t to a linked
**	list of pseudorupt sources whose head is vp_ilist.  v86stash()
**	also records a copy of the thread structure pointer in the
**	v86int_t to identify the thread.
**
**	Normally a device driver is responsible for retracting its
**	pseudorupt registration via a call to v86unstash().  It does
**	this from an AIOCNONDOSMODE ioctl or a device close.  If there
**	is a problem in the driver, or VP/ix is terminated uncleanly
**	(e.g. via kill -9), the pseudorupt registration is cleaned
**	up in v86exit() which makes a pass through the list of
**	pseudorupt sources doing the equivalent of v86unstash().
**
**	The v86stash()/v86unstash() mechanism is intended to simplify
**	the VP/ix code in device drivers by hiding the verification
**	of the continued existence of the destination process from
**	the device driver writer.  Also the use of the v86int_t
**	structure is intended to allow the interface to remain
**	unchanged as UNIX evolves.
**
**	Device drivers deliver pseudorupts via v86deliver() or
**	v86sdeliver().  v86deliver() attempts to send a pseudorupt
**	to the process identified by a v86int_t.  v86sdeliver() is
**	similar to v86deliver() but is intended for streams-based
**	drivers.  It passes a pseudorupt request packet upstream.
**	The stream head code calls v86deliver() when the packet
**	reaches the stream head.  This mechanism guarantees that the
**	pseudorupt will not be delivered before the data is available.
**	Drivers calling v86sdeliver() should do so AFTER sending any
**	related data upstream.
*/

/*
**  VP/ix MP locking strategy:
**
**	Lock v86int_t (via the mutex lock it contains) while attempting
**	v86stash(), v86unstash(), v86deliver() or the v86unstash()
**	equivalent in v86exit().  (Note that v86sdeliver() does not
**	refer to the contents of a v86int_t so no locking is required.)
**
**	Lock a process via p_lock while making it a VP/ix process,
**	making it a non-VP/ix process, or delivering a pseudorupt.
**
**	The mutex lock in a v86int_t belongs just above p_lock in
**	the process management lock hierarchy.  This ordering
**	corresponds to the typical use where it is necessary to
**	lock the v86int_t and then the process referred to by it.
**	v86exit() requires the opposite locking order, so it uses
**	L_TRY with the v86int_t lock and releases p_lock if it fails
**	(see the code in v86exit() for details).
*/

/* Definitions for v86i_state in v86int_t structure (see v86stash) */
#define UNINITIALIZED		0
#define BEING_INITIALIZED	1
#define INITIALIZED		3

#define OLDXTSSSIZE     301     /* Including bitmap */
#define OLDPARMSIZE	8	/* struct {xtss_t *xtssp; ulong szxtss;} */

#ifndef XXX_MP
#ifndef locked_or
#define locked_or(lhs, rhs)	(lhs |= rhs)
#endif
#endif /* !MP */

extern int v86enable;

/*
 *  v86init() system call.  Enroll the calling thread as a
 *  virtual 8086 ECT, initialize its XTSS, create and map in a
 *  virtual 8086 segment, and set up a task gate so the ECT can
 *  switch modes at the user level.
 */

int
v86init(arg)
caddr_t arg;
{
	v86_t 		*vp;
	register xtss_t *xtp;
	ulong 		xtsssegsize;
	struct v86parm 	viparm;
	int 		oldlvl, error, size;
	struct	seg	*seg;
	struct	as	*as;
	faultcode_t	fc;
	id_t		vpixclass;
	id_t		oldcid;
	caddr_t		clprocp;
        klwp_id_t 	lwp = ttolwp(curthread);
	struct regs 	*rp = lwptoregs(lwp);
	flags_t 	*flags = (flags_t *)&rp->r_ps;
	proc_t 		*pp = ttoproc(curthread);
	struct	seg_desc *gdt;
	struct	tss386	*utss;
	u_int		pfnum;

	/* 
	 * Base vpix segment has 2 holes: from 1M to the XTSS, and after 
	 * the XTSS to 2M.
	 */
	static struct segvpix_crargs vpix_base_args = {
		2, {{ (caddr_t)ptob(V86VIRTSIZE),
			(u_int)XTSSADDR - ptob(V86VIRTSIZE) },
		    { XTSSADDR + ptob(1),
			ptob(2 * V86VIRTSIZE - 1) - (u_int)XTSSADDR }}
	};

#ifdef DEBUG1
	cmn_err(CE_CONT,"v86init: \n"); 
#endif

#ifdef XXX_MP
	/* For MP this should be an atomic operation */
#else
	v86enable++;
#endif
	/*
	 *  v86init() is available only to the superuser.
	 */

	if (!suser(CRED())) {
		cmn_err(CE_WARN, "VP/ix not super user");
		return EPERM; 
	}

	if ((as = pp->p_as) == (struct as *)NULL)
		cmn_err(CE_PANIC, "v86init: no as allocated");

	/*
	 * We have nowhere to put a virtual 8086 if virtual address 0 is 
	 * in use.
	 */

	AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
	if (as_segat(as, 0)) {
		AS_LOCK_EXIT(as, &as->a_lock);
		return ENOMEM;
	}
	AS_LOCK_EXIT(as, &as->a_lock);

	/*
	 *  Copy in the argument structure.
	 */

	if (copyin(arg, (caddr_t)&viparm, sizeof (struct v86parm)))
		return EFAULT;

	/*
	 *  Old ECTs give a single size which is the size of the XTSS structure
	 *  including the I/O bitmap.  Accept that size to mean an old XTSS.
	 *  New ECTs give a separate size for XTSS structure and I/O bitmap.
	 *  For these, check that the XTSS size matches an xtss_t and check
	 *  that the I/O bitmap size is non-neagtive and small enough to fit
	 *  in the same page.
	 */
	if (viparm.szxtss == OLDXTSSSIZE)
		xtsssegsize = OLDXTSSSIZE;
	else {
		if (viparm.magic[0] != V86_MAGIC0 ||
			viparm.magic[1] != V86_MAGIC1)
			return EINVAL;

		xtsssegsize = viparm.szxtss + viparm.szbitmap;
		if (viparm.szxtss != (ulong)sizeof (xtss_t) ||
			xtsssegsize > (ulong)NBPC)
			return EINVAL;
	}

	/* Make sure VP/ix process class is properly configured */
	if (getcid("VC", &vpixclass) || vpixclass <= 0) {
		return EINVAL;
	}

	vp = kmem_zalloc(sizeof(v86_t), KM_SLEEP);

	/*
	 * We allocated a zero'ed out structure.  Initialize any
	 * fields which need to be non-zero.
	 */

	vp->vp_szxtss = xtsssegsize;              /* Set size in V86 struct */
	vp->vp_slice_shft = V86_SHIFT_NORM;       /* Reset time slice shift */
	vp->vp_pri_state = V86_PRI_NORM;	  /* Set normal priority    */

	/*
 	 * Finished all tests which might reasonably be expected to cause
 	 * v86init() to fail.  Proceed with real setup.
 	 */

	/* Need to hold pidlock and p_lock to call CL_ENTERCLASS() */
	mutex_enter(&pidlock);
	mutex_enter(&pp->p_lock);

	/* Enter the process in the VP/ix process class */
	clprocp = (caddr_t)curthread->t_cldata;
	oldcid = curthread->t_cid;
	error = CL_ENTERCLASS(curthread, vpixclass, NULL, CRED());

	mutex_exit(&pp->p_lock);
	mutex_exit(&pidlock);

	/* Fail if could not enter VP/ix class */
	if (error) {
		kmem_free((void *)vp, sizeof (v86_t));
		return error;
	}

	CL_EXITCLASS(oldcid, clprocp); /* Withdraw process from old class */

	/*
	 *  Create and map the virtual 86 segment.
	 */

	/* Get the one megabyte needed  for  the  v86  process,  and */
	/* another  megabyte  where  the  only  memory  that is used */
	/* should be for the xtss.  Except for the xtss, we will try */
	/* not to allow any addition pages in the second megabyte to */
	/* be allocated (although pages may be mapped  to  pages  in */
	/* the  first  megabyte).  This will allow us to put most of */
	/* the second megabyte back into avail[rs]mem.               */
	
	size = ctob(2*V86VIRTSIZE);
	if (error = as_map(as, (caddr_t)0, size, segvpix_create,
			(caddr_t)&vpix_base_args)) {
#ifdef DEBUG1
		cmn_err(CE_CONT,"v86init: as_map failed\n"); 
#endif
		kmem_free((void *)vp, sizeof (v86_t));
		return error;        
	}

	AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
	if ((seg = as_segat(as, (caddr_t)0)) == (struct seg *) NULL) {
		AS_LOCK_EXIT(as, &as->a_lock);
		cmn_err(CE_PANIC,"v86init: No segment for 0 address space\n");
	}

	/* Set up the wrap area equivalence */
	segvpix_range_equiv(seg, V86VIRTSIZE, 0, V86WRAPSIZE);
	AS_LOCK_EXIT(as, &as->a_lock);


	/*
	 * Nail all the pages of the new XTSS.  We mustn't allow them to be
	 * paged out as long as this process is SLOAD'ed.  If we're deactivated,
	 * the XTSS pages can be un-nailed and swapped.
	 *
	 * Note that these pages are zeroed when they are faulted. Therefore
	 * this code zeros out the new XTSS as a side-effect.
	 */

	/*
	 * Fault in the extended TSS pages and lock them in memory.
	 * segvpix_faultpage won't do PAGE_RELE - so pages will be
	 * locked in memory.
	 */
	if ((fc = as_fault(as->a_hat, as, (caddr_t)XTSSADDR, vp->vp_szxtss, 
				F_SOFTLOCK, S_WRITE)) != 0) {
#ifdef DEBUG1
		cmn_err(CE_CONT,"v86init: as_fault failed\n");
#endif
		as_unmap(as, seg->s_base, seg->s_size);
		kmem_free((void *)vp, sizeof (v86_t));
		return FC_MAKE_ERR(fc);
	}

	/*
	 * Create a mapping for XTSS in kernel address space and
	 * remember it.
	 */
	vp->vp_xtss = (xtss_t *)kmxtob(rmalloc(kernelmap,btopr(vp->vp_szxtss)));
	AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
	pfnum = hat_getpfnum(as, XTSSADDR);
	AS_LOCK_EXIT(as, &as->a_lock);
	segkmem_mapin(&kvseg, (caddr_t)vp->vp_xtss, 
		ptob(btopr((u_int)vp->vp_szxtss)),
		(PROT_READ | PROT_WRITE), pfnum, 0);

	/*
	 * Set up the gdt XTSS segment descriptor,
	 * and mark that descriptor BUSY so the ECT can iret into it.
	 */
	gdt = CPU->cpu_gdt;
	setdscrbase(&vp->vp_xtss_desc, (uint)vp->vp_xtss);
	setdscrlim(&vp->vp_xtss_desc, xtsssegsize - 1);
	setdscracc1(&vp->vp_xtss_desc, TSS3_KACC1);
	setdscracc2(&vp->vp_xtss_desc, TSS_ACC2);
	gdt[seltoi(XTSSSEL)] = vp->vp_xtss_desc;
	setdscracc1(&gdt[seltoi(XTSSSEL)], TSS3_KBACC1);

	/*
	 * Copy fields from the regular tss to the xtss, and set the VM flag.
	 * Don't have to do suword()'s...a fault "can't happen" because
	 * we've nailed the pages. ANY FIELDS NOT SET HERE HAVE A VALUE OF ZERO.
	 */
	xtp = vp->vp_xtss;
	utss = (struct tss386 *)CPU->cpu_tss;
	xtp->xt_tss.t_ldt = (u_long)LDTSEL;
	xtp->xt_tss.t_cr3 = CPU->cpu_cr3;
	xtp->xt_tss.t_esp0 = (u_long)curthread->t_stk + sizeof(struct regs) +
					V86FRAME + MINFRAME;
	xtp->xt_tss.t_esp1 = utss->t_esp1;
	xtp->xt_tss.t_esp2 = utss->t_esp2;
	xtp->xt_tss.t_ss0 = utss->t_ss0;
	xtp->xt_tss.t_ss1 = utss->t_ss1;
	xtp->xt_tss.t_ss2 = utss->t_ss2;
	xtp->xt_tss.t_bitmapbase = utss->t_bitmapbase;
	xtp->xt_tss.t_eflags = PS_IE | PS_VM;   /* LEAVES IOPL == 0 ! */
	xtp->xt_tss.t_cs = V86_RCS;             /* RESET to virtual 8086 */
	xtp->xt_tss.t_eip = V86_RIP;

	/*
	 *   Set up some XTSS fields.
	 */
	xtp->xt_magic[0] = V86_MAGIC0;
	xtp->xt_magic[1] = V86_MAGIC1;
	xtp->xt_magicstat = XT_MSTAT_NOPROCESS; /* Do not process virt int */
	xtp->xt_tslice_shft = V86_SHIFT_NORM;	/* Normal time slice shift */
	xtp->xt_timer_bound = V86_TIMER_BOUND;  /* Limit to force ECT in */
	xtp->xt_viurgent = (V86VI_KBD | V86VI_MOUSE);
	xtp->xt_vitoect = ~V86VI_NONE;
	xtp->xt_vp_relcode = VP_RELCODE;

	/*
	 *   The back-link in the UTSS must point to the XTSS
	 *   so the ECT can start the virtual 8086 with an iret.
	 *   Also turn on caller's NT bit in the stack flag image.
	 */
	utss->t_link = XTSSSEL;
	flags->fl_nt = 1;
	/* XXX - temporary fix to make ins/outs work for ECT */
	flags->fl_iopl = 3;

	/* Set V86 task floating point state to NOTVALID */
	vp->vp_fpu.fpu_flags = FPU_INVALID;
	vp->vp_lbolt_update = lbolt;

	/* Setup v86_ops pointers */
	vp->vp_ops.v86_swtch = v86swtch;
	vp->vp_ops.v86_rtt = v86rtt;
	vp->vp_ops.v86_exit = v86exit;
	vp->vp_ops.v86_sighdlint = v86sighdlint;

	/*
	 * Initialize mutex lock and conditional variables.
	 */
	mutex_init(&vp->vp_mutex, "VPix mutex", MUTEX_DEFAULT, NULL);
	cv_init(&vp->vp_cv, "VPix cv", CV_DEFAULT, NULL);

	thread_lock(curthread);
	/*
	 *   Finished setting up the VP/ix data structures.  Attach
	 *   the v86_t to the thread structure and do other interrupt
	 *   sensitive initialization.
	 */
	curthread->t_v86data = (caddr_t)vp;
	CPU->cpu_v86procflag = 1; /* Indicates idt-switch in locore */
	thread_unlock(curthread);

	/*
	 *  Set up the return values in the passed-in argument structure.
	 */
	viparm.xtssp = (xtss_t *)XTSSADDR;
	if (xtsssegsize == OLDXTSSSIZE)
		copyout((caddr_t)&viparm, arg, sizeof (struct v86parm));
	else
		copyout((caddr_t)&viparm, arg, OLDPARMSIZE);

#ifdef DEBUG1
	cmn_err(CE_CONT,"v86init: exiting\n"); 
#endif
	module_keepcnt++;

	return 0;
}


int
v86sleep()
{
	register v86_t *v86p;           /* Current v86 structure */
	xtss_t *xtp;                    /* Pointer to the XTSS */
	time_t sleeptime;		/* Maximum time to sleep */
	void v86wakeup();		/* For use in timeout() */

	/*
	 * Since the XTSS is nailed in no fault can occur. Hence members of
	 * the XTSS can be accessed directly though the XTSS is in user space.
	 */
#ifdef DEBUG1
	cmn_err(CE_CONT,"In v86sleep\n"); 
#endif
	v86p = (v86_t *)curthread->t_v86data;	/* Get current v86 structure */
	if (!v86p)			/* Must be a dual-mode process */
		return EINVAL;
	xtp = v86p->vp_xtss;       	/* Init to user XTSS address */

	/*
	 * The sleep is supposed to last until a pseudorupt in xt_vimask occurs
	 * or until xt_timer_count reaches xt_timer_bound.  Verify that neither
	 * of these has happened yet, set a timeout for when xt_timer_bound
	 * will be reached, then sleep.  v86setpseudo will issue a wakeup if
	 * one of the pseudorupts we want occurs.
	 *
	 */

	sleeptime = v86p->vp_lbolt_update +
		(xtp->xt_timer_bound - xtp->xt_timer_count) * V86TIMER - lbolt;
	if (!(xtp->xt_vimask & xtp->xt_viflag) && sleeptime > 0) {
		v86p->vp_wakeid = timeout(v86wakeup, (caddr_t)v86p, sleeptime);
		mutex_enter(&v86p->vp_mutex);
		cv_wait(&v86p->vp_cv, &v86p->vp_mutex); 
		mutex_exit(&v86p->vp_mutex);
	}
	return 0;
}


void
v86wakeup(v86p)
v86_t *v86p;
{
#if XXX_MP
	if (atomic_write(v86p->vp_wakeid, 0) != 0) {
		cv_broadcast(&v86p->vp_cv);
	}
#else
	if (v86p->vp_wakeid) {
		v86p->vp_wakeid = 0;
		cv_broadcast(&v86p->vp_cv);
	}
#endif /* MP */
}


/* Process mappings and trackings. */

int
v86memfunc(arg)
caddr_t arg;
{
	struct v86memory vmem;          /* User arguments structure */

#ifdef DEBUG1
	cmn_err(CE_CONT,"In v86memfunc\n"); 
#endif

	/*  Get user arguments into local structure */
	if (copyin(arg, (caddr_t)&vmem, sizeof (struct v86memory)))
		return EFAULT;

	/*
 	 *  Process map and track requests
 	 */

	switch (vmem.vmem_cmd)
	{
		case V86MEM_MAP:                /* Map the screen */
		case V86MEM_TRACK:              /* Track the screen */
		case V86MEM_UNTRACK:		/* Untrack the screen */
		case V86MEM_UNMAP:		/* Unmap virt addr */
			return v86scrfunc(&vmem);

		case V86MEM_EMM:		/* Support EMM memory     */
			return v86emmset(&vmem);

		case V86MEM_GROW:		/* Grow EMM memory        */
			return v86emmgrow(&vmem);

		default:
			return EINVAL;		/* Invalid system call */
	}
}


static
v86scrfunc(vmemptr)
struct v86memory *vmemptr;              /* User arguments structure         */
{
	v86_t       *v86p;		/* pointer to the v86 struct        */
	struct	as  *as;		/* process's address space	    */
	struct  seg *seg;		/* vp/ix segment for process	    */
	u_int	    basepage;		/* first screen memory page	    */
	u_int	    npages;		/* number of screen memory pages    */
	u_int	    physpage;		/* physical page to map to (opt.)   */
	int	    err;

#ifdef DEBUG1
	cmn_err(CE_CONT, "In v86scrfunc\n"); 
#endif

	/*
 	 * Validate the request arguments.
 	 */

	/* Make sure this process is a v86 process. */
	if ((v86p = (v86_t *)curthread->t_v86data) == NULL)
		return EACCES;

	/* Only V86MEM_MAP uses vmem_physaddr, so ignore it for all else. */
	if (vmemptr->vmem_cmd != V86MEM_MAP)
		vmemptr->vmem_physaddr = 0;

	/* Make sure the arguments are all page-aligned. */
	if (((u_int)vmemptr->vmem_membase & PAGEOFFSET) != 0 ||
	    ((u_int)vmemptr->vmem_memlen & PAGEOFFSET) != 0 ||
	    ((u_int)vmemptr->vmem_physaddr & PAGEOFFSET) != 0)
		return ENXIO;

	/* Convert the arguments to units of pages. */
	basepage = btop(vmemptr->vmem_membase);
	npages = btop(vmemptr->vmem_memlen);
	physpage = btop(vmemptr->vmem_physaddr);

	/* Find the segment corresponding to the base address. */

	if ((as = ttoproc(curthread)->p_as) == (struct as *)NULL)
		cmn_err(CE_PANIC, "v86scrfunc: no as allocated");

	AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
	if ((seg = as_segat(as, vmemptr->vmem_membase)) == (struct seg *)NULL) {
#ifdef DEBUG1
		cmn_err(CE_CONT, "v86scrfunc: as_segat failed\n");
#endif
		AS_LOCK_EXIT(as, &as->a_lock);
		return EACCES;
	}
	AS_LOCK_EXIT(as, &as->a_lock);

	/* It must be a seg_vpix segment. */

	if (seg->s_ops != &segvpix_ops) {
#ifdef DEBUG1
		cmn_err(CE_CONT, "v86scrfunc: Not a vpix segment\n");
#endif
		return EACCES;
	}

	/*
	 * The desired range, including its tracking equivalence range,
	 * must fall within the segment size.
	 */

	ASSERT(seg->s_base == 0);

	if (ptob(basepage + npages) > seg->s_size ||
	    ptob(basepage + npages + V86VIRTSIZE) > seg->s_size) {
#ifdef DEBUG1
		cmn_err(CE_CONT, "v86scrfunc: Out of range\n");
#endif
		return EACCES;
	}

	/*
	 * Perform the desired screen memory operation(s).
	 */

	AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
	switch (vmemptr->vmem_cmd) {

	case V86MEM_MAP:                /* Map the screen */

		/* Map the address range to the physical screen memory. */

		err = _v86physmap(seg, basepage, physpage, npages);
		if (err) {
			AS_LOCK_EXIT(as, &as->a_lock);
			return err;
		}

		/* Fall through... */

	case V86MEM_TRACK:              /* Track the screen */

		/* Set up an equivalence between this (screen) memory area
		 * and the shadow range used for tracking. */

		err = segvpix_range_equiv(seg, basepage + V86VIRTSIZE,
					       basepage,
					       npages);
		if (err) {
			AS_LOCK_EXIT(as, &as->a_lock);
			return err;
		}

		/* Lock down the screen pages so we don't lose the dirty
		 * bits, which we need to check periodically in v86scrscan. */

		err = SEGOP_LOCKOP(seg, vmemptr->vmem_membase,
					  vmemptr->vmem_memlen,
					  0, MC_LOCK, NULL, 0);
		if (err) {
			segvpix_range_equiv(seg, basepage + V86VIRTSIZE,
						 basepage + V86VIRTSIZE,
						 npages);
			if (vmemptr->vmem_cmd == V86MEM_MAP)
				segvpix_unphys(seg, basepage, npages);
			AS_LOCK_EXIT(as, &as->a_lock);
			return err;
		}

		/*
		 * NOTE: We depend on the XTSS lock to lock the translations
		 * for the screen pages.  This works because they're in the
		 * same page table and the current hat implements translation
		 * locking at the page table level.
		 */

		ASSERT(MMU_L1_INDEX(vmemptr->vmem_membase) == 0);

		/* Remember the screen mapping in vp_mem. */

		v86p->vp_mem = *vmemptr;

		break;

	case V86MEM_UNMAP:		/* Unmap virt addr */

		/* Undo the physical mapping. */

		err = segvpix_unphys(seg, basepage, npages);
		if (err) {
			AS_LOCK_EXIT(as, &as->a_lock);
			return err;
		}

		/* Fall through... */

	case V86MEM_UNTRACK:		/* Untrack the screen */

		/* Undo the screen tracking equivalence. */

		err = segvpix_range_equiv(seg, basepage + V86VIRTSIZE,
					       basepage + V86VIRTSIZE,
					       npages);
		if (err) {
			AS_LOCK_EXIT(as, &as->a_lock);
			return err;
		}

		/* Unlock the pages. */

		err = SEGOP_LOCKOP(seg, vmemptr->vmem_membase,
					  vmemptr->vmem_memlen,
					  0, MC_UNLOCK, NULL, 0);
		if (err) {
			AS_LOCK_EXIT(as, &as->a_lock);
			return err;
		}

		/* Screen mapping no longer active. */

		v86p->vp_mem.vmem_memlen = 0;

		break;

	}
	AS_LOCK_EXIT(as, &as->a_lock);

	return 0;
}


static
v86emmset (vmemptr)
struct v86memory *vmemptr;              /* User arguments structure         */
{
	v86_t       *v86p;		/* pointer to the v86 struct        */
	struct	as  *as;                /* address space for the process    */
	struct	seg *seg;               /* segment pointer for v86 segment  */
	u_int	    basepage;		/* first memory page for emm window */
	u_int	    npages;		/* number of pages in winsow        */
	u_int	    emmpage;		/* emm page to map window to	    */
	int	    err;

#ifdef DEBUG1
	cmn_err(CE_CONT, "In v86emmset\n"); 
#endif

	/*
	 * Validate the request arguments.
	 */

	/* Make sure this process is a v86 process. */
	if ((v86p = (v86_t *)curthread->t_v86data) == NULL)
		return EACCES;

	/* Make sure the arguments are all page-aligned. */
	if (((u_int)vmemptr->vmem_membase & PAGEOFFSET) != 0 ||
	    ((u_int)vmemptr->vmem_memlen & PAGEOFFSET) != 0 ||
	    ((u_int)vmemptr->vmem_physaddr & PAGEOFFSET) != 0)
		return ENXIO;

	/* Convert the arguments to units of pages. */
	basepage = btop(vmemptr->vmem_membase);
	npages = btop(vmemptr->vmem_memlen);
	emmpage = btop(vmemptr->vmem_physaddr);

	/* Make sure EMM window is not in low memory used with 1M wrap. */
	if (basepage < V86WRAPSIZE)
		return ENXIO;

	/* Find the segment corresponding to the base address. */

	if ((as = ttoproc(curthread)->p_as) == (struct as *)NULL)
		cmn_err(CE_PANIC, "v86emmset: no as allocated");

	AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
	if ((seg = as_segat(as, vmemptr->vmem_membase)) == (struct seg *)NULL) {
#ifdef DEBUG1
		cmn_err(CE_CONT, "v86emmset: as_segat failed\n");
#endif
		AS_LOCK_EXIT(as, &as->a_lock);
		return EACCES;
	}
	AS_LOCK_EXIT(as, &as->a_lock);

	/* It must be a seg_vpix segment. */

	if (seg->s_ops != &segvpix_ops) {
#ifdef DEBUG1
		cmn_err(CE_CONT, "v86emmset: Not a vpix segment\n");
#endif
		return EACCES;
	}

	/*
	 * Address validation is done by the LIM 4.0 driver and/or
	 * the ECT.  This is because the LIM 4.0 specification allows,
	 * virtually, any mapping;  the VP/ix environment, however,
	 * imposes some limitation on this.
	 *
	 * Make sure the length is V86EMM_PGSIZE (16K).
	 * Make sure the address being mapped to is on a
	 * page boundary, not in the first two megabytes,
	 * and falls within the vpix segment.
	 */

	ASSERT(seg->s_base == 0);

	if (ptob(npages) != V86EMM_PGSIZE ||
	    ptob(basepage + npages) > seg->s_size ||
	    (emmpage && (ptob(emmpage) < V86EMM_LBASE ||
			 ptob(emmpage + npages) > seg->s_size))) {
#ifdef DEBUG1
		cmn_err(CE_CONT, "v86emmset: Out of range\n");
#endif
		return ENXIO;
	}

	/*
	 * Set up the actual mapping.
	 */

	/* Set up an equivalence for this emm page. */
	AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
	err = segvpix_range_equiv(seg, basepage,
					emmpage? emmpage : basepage,
					btop(V86EMM_PGSIZE));
	AS_LOCK_EXIT(as, &as->a_lock);
	return err;
}


static
v86emmgrow (vmemptr)
struct v86memory *vmemptr;              /* User arguments structure         */
{
	struct	as  *as;                /* address space for the process    */
	struct	seg *seg;               /* segment pointer for v86 segment  */
	caddr_t      growbound;          /* address that we are growing to   */

#ifdef DEBUG1
	cmn_err(CE_CONT, "In v86emmgrow\n"); 
#endif

	/*
	 * Validate the request arguments.
	 */

	/* Make sure this process is a v86 process. */

	if (curthread->t_v86data == NULL)
		return EACCES;

	/* Make sure the arguments are all page-aligned. */

	growbound = vmemptr->vmem_membase;
	if (((u_int)growbound & PAGEOFFSET) != 0) {
#ifdef DEBUG1
		cmn_err(CE_CONT, "v86emmgrow: invalid growbound address\n");
#endif
		return ENXIO;
	}

	/* Find the segment corresponding to the base address. */

	if ((as = ttoproc(curthread)->p_as) == (struct as *)NULL)
		cmn_err(CE_PANIC, "v86emmgrow: no as allocated");

	AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
	if ((seg = as_segat(as, 0)) == (struct seg *)NULL) {
#ifdef DEBUG1
		cmn_err(CE_CONT, "v86emmgrow: as_segat failed\n");
#endif
		AS_LOCK_EXIT(as, &as->a_lock);
		return EACCES;
	}
	AS_LOCK_EXIT(as, &as->a_lock);

	/* It must be a seg_vpix segment. */

	if (seg->s_ops != &segvpix_ops) {
#ifdef DEBUG1
		cmn_err(CE_CONT, "v86emmgrow: Not a vpix segment\n");
#endif
		return EACCES;
	}

	/*
	 * The grow boundary (new seg_vpix size) must be between
	 * 2 and 4 megabytes.
	 */

	if ((u_int)growbound < ptob(2 * V86VIRTSIZE) ||
	  		(u_int)growbound > ptob(V86SIZE)) {
#ifdef DEBUG1
		cmn_err(CE_CONT, "v86emmgrow: address not in 2 to 4 MB range\n");
#endif
		return ENXIO;
	}

	/*
	 * Do the actual grow operation.
	 */

	ASSERT(seg->s_base == 0);

	/* If the segment is already big enough, don't do anything. */

	if (growbound <= (caddr_t)seg->s_size)
		return 0;

	/*
	 * Map in a new vpix segment to cover the new size.
	 * Seg_vpix will coalesce it with the existing segment.
	 */

	if (as_map(as, (caddr_t)seg->s_size,
			growbound - (caddr_t)seg->s_size,
			segvpix_create, vpix_argsp) != 0) {
#ifdef DEBUG1
		cmn_err(CE_CONT, "v86emmgrow: as_map failed for growth\n");
#endif
		return ENXIO;
	}

	return 0;
}


/*
 *  Exit processing for a v86 process thread.
 *  If it's being killed in v86 mode, and is the current thread,
 *  make sure to do an ltr back to UTSSSEL before jumping away
 *  to some other process, because we're de-nailing the XTSS.
 *  Otherwise, just free up v86 struct memory, clear out v86 ptr
 *  in proc table, and de-nail the XTSS.
 *
 *  NOTE: Currently this routine is called only from exit().
 */

v86exit(t)
	kthread_id_t t;
{
	v86_t *vp;
	v86int_t *vip;
	proc_t	*p = ttoproc(t);
	register struct as *as;
	struct seg_desc *gdt = CPU->cpu_gdt;
	int s;
	
#ifdef DEBUG1
	  cmn_err(CE_CONT,"In v86exit\n"); 
#endif

	vp = (v86_t *)t->t_v86data;
#ifdef XXX
	if ((vp == NULL) || !vpix_thread(t))
		return;		/* not a vpix process */
#endif
	if (get_tr() == XTSSSEL) {
		/* 
		 * We are on XTSS, make sure the TSS is marked 
		 * available.
		 */
		setdscracc1(&gdt[seltoi(UTSSSEL)], TSS3_KACC1);
		loadtr(UTSSSEL);
	}
	thread_lock(t);
	t->t_v86data = (caddr_t)NULL;
	CPU->cpu_v86procflag = 0;
	thread_unlock(t);

	/*
	 * We need to get process lock to protect v86int list.
	 */
	mutex_enter(&p->p_lock);

	/*
	 * Normally we expect the v86int_t list to be empty, but
 	 * if the ECT was killed or a driver close routine has no 
	 * unstash or the ECT failed to turn off pseudorupts on a 
	 * device not being closed, there could be a non-empty list.
	 * We must clean it up before exiting so that no one tries to
	 * send pseudorupts to non-existent processes.
 	 */
	while ((vip = vp->vp_ilist) != (v86int_t *)0) {
		/*
		 * Normally we are not allowed to acquire a v86int_t
	 	 * lock when we already have a process lock.  But unless
	 	 * we lock the process, we cannot rely on the v86int_t
	 	 * pointer in it.  Attempt a lock with L_TRY.  If it
	 	 * succeeds we can proceed.  If it fails, unlock the
	 	 * proc in case the v86int_t holder wants it.  Wait
	 	 * a bit then try again. In most cases we will find that
	 	 * the other locker did the unstash, otherwise we will
	 	 * do it ourselves.
	 	 */
		if (mutex_adaptive_tryenter(&vip->v86i_lock) == NULL) {
			mutex_exit(&p->p_lock);
			/* Could sleep on a flag here maybe? */
			delay(1);
			mutex_enter(&p->p_lock);
			continue;
		}
		vp->vp_ilist->v86i_t = 0;
		vp->vp_ilist = vp->vp_ilist->v86i_i;
		mutex_exit(&vip->v86i_lock);
	}
	mutex_exit(&p->p_lock);

	if ((as = p->p_as) == (struct as *) NULL)
		cmn_err(CE_PANIC,"v86exit: no as allocated\n");

	/*
 	 * Mapout the mapping for XTSS in kernel address space.
 	 */
	segkmem_mapout(&kvseg, (caddr_t)vp->vp_xtss,
			ptob(btopr((u_int)vp->vp_szxtss)));
	rmfree(kernelmap, btopr(vp->vp_szxtss), btokmx((ulong_t)vp->vp_xtss));

	/* Unlock the XTSS */
	if (as_fault(as->a_hat, as, (caddr_t)XTSSADDR, vp->vp_szxtss, 
				F_SOFTUNLOCK, S_WRITE) != 0)
	   cmn_err(CE_PANIC,"v86exit: SOFT_UNLOCK on X-TSS failed\n");

	/*
	 * Free up FPU context used by v86mode task.
	 */
	fp_free(&vp->vp_fpu);	/* release any active FPU context */

	/* destroy mutexes and locks */
	cv_destroy (&vp->vp_cv);
	mutex_destroy(&vp->vp_mutex);

	kmem_free((void *)vp, sizeof (v86_t));
#ifdef XXX_LDT
	/*
	 * If the thread uses a private LDT then free it and switch 
	 * the LDT descriptor to the default one and reload LDT register.
	 */
	/**** FIX IT! */
#endif
	module_keepcnt--;
#ifdef XXX_MP
	/* For MP this should be an atomic operation */
#else
	v86enable--;
#endif
}


/*
 *  Force a virtual interrupt and save signal information.
 *  This routine is called from "sendsig" when we are returning
 *  to V86 user mode. Since the handler can only be in 386
 *  user mode, this routine forces a virtual interrupt to get
 *  back into 386 mode and saves the signal number and signal
 *  handler, so that the ECT can call its handler directly if
 *  necessary. Since the XTSS is nailed no fault can occur
 *  accessing fields in the XTSS. The ECT is expected to
 *  zero the "xt_signo" field after it is done processing.
 *  NOTE: There is no race condition as we should switch from
 *  V86 to 386 mode and process the signals. We will switch
 *  back to V86 mode only when the processing is done. This
 *  code is executed only if we are going back into V86 mode.
 */

v86sighdlint(hdlr, signo)
int     (* hdlr)();
unsigned int signo;
{
	register xtss_t *xtp;           /* Pointer to XTSS in user space */
	register v86_t *v86p;           /* Current v86 structure */

#ifdef DEBUG1
	cmn_err(CE_CONT,"In v86sighdlint\n"); 
#endif
	xtp = ((v86_t *)curthread->t_v86data)->vp_xtss; 
				/* Init to user XTSS address */

	if (xtp->xt_signo == 0)         /* If no signal being processed */
	{
		xtp->xt_signo = signo;  /* Save signal number */
		xtp->xt_hdlr = hdlr;    /* Save signal handler */
		v86setpseudo(curthread, V86VI_SIGHDL); /* Force virtual intr */
	}
}


/*
 * v86setpseudo() returns 1 if we set a pseudorupt, 0 if not.
 * It can be called via v86deliver which ensures the continued
 * existence of the process by locking the v86int_t structure.
 * Other calls are from various places in "normal" system code.
 * The code making the call is expected to guarantee the existence
 * of the thread.  
 *
 * XXX - CHECK IF THESE COMMENTS ARE VALID?
 * (Typically the process is u.u_procp which is safe.
 * One possible source of trouble is floating point support.  At the
 * time of writing, MP support for FP is primitive.  Some revision
 * of FP pseudorupt delivery might be required if MP FP support is
 * improved, perhaps including the use of a v86int_t structure
 * per FP unit.)
 */
v86setpseudo(t, intr_bits)
	kthread_id_t	t;		/* Ptr to proc struct       */
	u_int 		intr_bits;	/* Pseudorupt(s) being set  */
{
	register v86_t *v86p;
	register    xtss_t      *xtp;           /* Ptr to xtss in proc p    */
	char t_stat;				/* Local copy of t_stat	    */
	proc_t *p = ttoproc(t);
	int wakeid = 0;
	int s;

#ifdef DEBUG1
        cmn_err(CE_CONT,"In v86setpseudo\n"); 
#endif

#ifdef XXX_later
	if (!vpix_thread(t))
		return 0;
#endif

	mutex_enter(&p->p_lock);
	/* Do a quick check for inability to deliver pseudorupt */
	if ((v86p = (v86_t *)t->t_v86data) == 0 || (xtp = v86p->vp_xtss) == 0) {
		mutex_exit(&p->p_lock);
		return 0;
	}

	/*
	 * Need locked_or here even though process is locked because ECT
	 * might try to examine or change xt_viflag on one processor while
	 * this routine is handling an interrupt on another.
	 */
	locked_or(xtp->xt_viflag, intr_bits);

	if ((t_stat = t->t_state) == TS_RUN) {
		/* Thread is runnable but not actually running.  May need
		 * to boost its priority to get it to run sooner.
		 */
		if (xtp->xt_viflag & xtp->xt_vimask & xtp->xt_viurgent &
				~(V86VI_TIMER | V86VI_LBOLT)) {
			v86p->vp_pri_state = V86_PRI_XHI;
			v86p->vp_slice_shft = xtp->xt_tslice_shft = 0;
			thread_lock(t);
			vc_boost(t, B_TRUE);
			thread_unlock(t);
		}
		else if (xtp->xt_viflag & xtp->xt_vimask &
				~(V86VI_TIMER | V86VI_LBOLT)) {
			v86p->vp_pri_state = V86_PRI_HI;
			v86p->vp_slice_shft = xtp->xt_tslice_shft = 0;
			thread_lock(t);
			vc_boost(t, B_FALSE);
			thread_unlock(t);
		}
	}
#ifdef XXX_MP	/* fix it for MP */
	else if (proc_stat == SONPROC) {
		/* Process is active.  If it on this CPU, return to user
		 * mode will deliver the pseudorupt.  Otherwise send an
		 * interrupt to its CPU.  Kernel entry and exit will force
		 * immediate delivery.  There is a small chance that it
		 * will lose the CPU before the interrupt arrives but the
		 * worst that can happen is that the process will not get
		 * its pseudorupt until next time it starts running.  Note
		 * that we have p_lock, so p_stat can change only between
		 * SRUN and SONPROC.
		 */
		if ((runcpu = p->p_runcpu) != cpuid &&
				runcpu != TRANCPU && runcpu != NULLCPU) {
			xintr(cpuid, XINTR_NULL);
		}
	}
#endif
	else if (t_stat == TS_SLEEP) {
		/* Thread is sleeping.  It might be a regular Unix sleep
		 * or a v86sleep().  First check whether this pseudorupt
		 * is one of the ones that would wake the process from a
		 * v86sleep.  If it is, consult (and clear) the vp_wakeid
		 * field to determine whether the process is in a v86sleep.
		 * If it is, we will wake it up once we have unlocked the
		 * process.
		 */
		if (xtp->xt_vimask & intr_bits) {
#if	XXX_MP
			if ((wakeid = atomic_write(v86p->vp_wakeid, 0)) != 0){
#else
			if (wakeid = v86p->vp_wakeid) {
				v86p->vp_wakeid = 0;
#endif /* MP */
				(void) untimeout(wakeid);
			}
		}
	}

	mutex_exit(&p->p_lock);

	/* In MP we cannot wake up the process while it is locked
	 * so, if necessary, we set wakeid (see above).
	 *
	 * XXX - what?
	 * There is no danger of the process going away between the
	 * mutex_exit and the wakeup.  Either it is the current process
	 * or we were called via a locked v86int_t structure
	 * which is still chained into the v86 structure.
	 */
	if (wakeid)
		cv_broadcast(&v86p->vp_cv);

	return 1;
}


/*
 * Process virtual interrupts before going into user mode.
 * This routine is called from locore.s before exiting to
 * user mode of a dual mode process (V86 mode OR 386 mode).
 * If there are virtual interrupts to be processed and the
 * ECT has enabled processing of virtual interrupts then
 * set the ARPL instruction in the CS:IP of the V86 task
 * and save the opcode at CS:IP in a fixed location in
 * the XTSS.
 */

v86vint(r0ptr, v86flag)
register int *r0ptr;
int v86flag;
{
	register xtss_t *xtp;           /* Pointer to XTSS in user space */
	proc_t *p = ttoproc(curthread); /* Current proc structure */
	register v86_t *v86p;           /* Current v86 structure */
	caddr_t usrmemp;                /* Pointer to user memory */
	char tslice_shft;               /* Time slice shift requested */
	time_t lbolt_sample;		/* Snapshot of lbolt */
	int ticks;			/* Deliverable timer ticks */
	int intr_bits = 0;		/* Locally generated pseudorupts */
	int s;

#ifdef DEBUG1
	cmn_err(CE_CONT,"In v86vint\n"); 
#endif
	v86p = (v86_t *) curthread->t_v86data;  /* Get current v86 structure */

	/*
	 * Since the XTSS is nailed no fault can occur. Transfer the virtual
	 * interrupts to the XTSS and clear the interrupts in the V86 struct.
	 */
	xtp = v86p->vp_xtss;       /* Init to user XTSS address */

	/* A v86 timer tick occurs every V86TIMER Unix clock ticks.
	 * vp_lbolt_update is the lbolt value corresponding to the time
	 * at which the last delivered v86 timer tick was due (not
	 * necessarily the time at which it was delivered).  Determine
	 * whether any timer ticks are pending now.  Use a local copy
	 * of lbolt rather than preventing interrupts.
	 */
	lbolt_sample = lbolt;
	ticks = (lbolt_sample - v86p->vp_lbolt_update) / V86TIMER;
	if (ticks) {
		/* Deliver the pending timer ticks.  Add the equivalent
		 * number of Unix ticks to vp_lbolt_update to determine
		 * when the last of them was due (can be up to V86TIMER - 1
		 * Unix ticks ago).
		 */
		intr_bits |= V86VI_TIMER;
		xtp->xt_timer_count += ticks;
		v86p->vp_lbolt_update += ticks * V86TIMER;
	}

	if (xtp->xt_lbolt != lbolt_sample) {
		intr_bits |= V86VI_LBOLT;
		xtp->xt_lbolt = lbolt_sample;
	}

	/*
	 * If both the viflag bit is clear AND the vimask bit for video
	 * dirty bits are set, call v86scrscan(); otherwise
	 * we either have a video psuedorupt pending, or we
	 * do not want a video psuedorupt at this time.
	 */
	if(!(xtp->xt_viflag&V86VI_MEMORY) && (xtp->xt_vimask&V86VI_MEMORY)) {
		if (v86scrscan()) {
			intr_bits |= V86VI_MEMORY;
		}
	}

	/* Start of critical section.  Need to prevent vp_pri_state
	 * changing due to an interrupt while we manipulate it.
	 */
	mutex_enter(&p->p_lock);

	/*
	 * If scheduled due to high priority revert to normal.  Pay no
	 * attention to xtss slice shift because the pseudorupt could
	 * result in it changing.  We were probably scheduled as a result
	 * of the pseudorupt that gave us high priority.
	 */
	if (v86p->vp_pri_state == V86_PRI_HI ||
			v86p->vp_pri_state == V86_PRI_XHI) {
		v86p->vp_pri_state = V86_PRI_NORM;
		v86p->vp_slice_shft = xtp->xt_tslice_shft;
	}

	/* If the slice shift changed, set the appropriate priority. */
	if (xtp->xt_tslice_shft != v86p->vp_slice_shft) {
		xtp->xt_tslice_shft =
			min (V86_SLICE_SHIFT, xtp->xt_tslice_shft);
		if (xtp->xt_tslice_shft) {
		    v86p->vp_pri_state = V86_PRI_LO;
		}
		else {
		    v86p->vp_pri_state = V86_PRI_NORM;
		}
		thread_lock(curthread);
		vc_busywait(curthread, xtp->xt_tslice_shft);
		thread_unlock(curthread);
		v86p->vp_slice_shft = xtp->xt_tslice_shft;
	}

	/* Add the newly generated pseudorupts to the real mask */
	xtp->xt_viflag |= intr_bits;

	mutex_exit(&p->p_lock);
	/* End of critical segment */

	/*
	 * If the "xt_magicstat" is set to XT_MSTAT_OPSAVED then the
	 * the ECT is still processing a previous virtual interrupt.
	 * If the "xt_magicstat" is set to XT_MSTAT_PROCESS by the
	 * ECT and there are virtual interrupts to process, or the
	 * "xt_timer_count" is greater than or equal to "xt_timer_bound",
	 * then set the ARPL instruction in the next instr to be exec-ed
	 * in V86 mode. The valid CS:IP is on the stack if we are
	 * are going back to V86 user mode, otherwise the valid CS:IP
	 * is the one in the XTSS. The byte at the valid CS:IP location
	 * for the V86 program is savid in "xt_magictrap" in the XTSS
	 * and a ARPL instruction is set in the v86 program at this
	 * location.
	 * NOTE: We can exit to user mode (V86 or 386) only once as this
	 * routine is called just before exit to user mode. If this routine
	 * is interrupted we won't reenter it (as we will be in the kernel
	 * at the end of interrupt processing) and will just resume where
	 * we left off after interrupt processing. Hence there is no race
	 * condition.
	 */
	if ((xtp->xt_viflag & xtp->xt_vitoect) &&
			(xtp->xt_magicstat == XT_MSTAT_PROCESS)) {
		xtp->xt_magicstat = XT_MSTAT_OPSAVED;   /* Tell ECT */
		if (v86flag)            /* Going to V86 mode */
		    usrmemp = (caddr_t)(((r0ptr[CS] & 0x0000FFFF) << 4)
				      + (r0ptr[EIP] & 0x0000FFFF));
		else                    /* Going to 386 mode */
		    usrmemp = (caddr_t)(((xtp->xt_tss.t_cs & 0x0000FFFF) << 4)
				      + (xtp->xt_tss.t_eip & 0x0000FFFF));
		xtp->xt_magictrap = fubyte(usrmemp);    /* Save user byte */
		subyte(usrmemp, ARPL);  /* Set ARPL in user program */
	}
}


int
v86stash(ip, v86b)
	v86int_t *ip;
	struct v86blk *v86b;
{
	proc_t	*p;
	kthread_id_t	t;
	v86_t	*v86p;
	int	s;
	int	answer = 0;

	/* Identify process from the v86blk if given, otherwise use the
	 * current process.
	 */
	if (v86b)
		t = v86b->v86b_t;
	else
		t = curthread;

#if XXX_MP
	/* Rather than requiring drivers which use v86stash to
	 * initialize their own v86int_t structure locks,
	 * we hide the init code here.
	 */
	if (ip->v86i_state != INITIALIZED) {
		/* Disable interrupts before the switch so that this
		 * processor cannot compete with itself.
		 */
		thread_lock(t);
		switch(atomic_or(ip->v86i_state, BEING_INITIALIZED)) {
		    case UNINITIALIZED:
			/* We get to do the initialization.  Do not permit
			 * interrupts until we have completely finished
			 * because a nested attempt to initialize would
			 * cause deadlock.
			 */
			mutex_init(&ip->v86i_lock, "v86i lock", MUTEX_DEFAULT,
				DEFAULT_WT);
			locked_or(ip->v86i_state, INITIALIZED);
			thread_unlock(t);
			break;
		    case BEING_INITIALIZED:
			/* Some other processor has started initializing the
			 * lock but hasn't finished.  Spin until it finishes
			 * because we may not be allowed to sleep and the
			 * wait should be brief anyway.  Allow interrupts
			 * because they are harmless and more productive
			 * than spinning.
			 */
			thread_unlock(t);
			while (ip->v86i_state != INITIALIZED)
				continue;
			break;
		    default:
			/* Should never get here */
			MP_ASSERT(B_FALSE);
		    case INITIALIZED:
			thread_unlock(t);
			break;
		}
	}
#else
	if (ip->v86i_state != INITIALIZED) {
		thread_lock(t);
		mutex_init(&ip->v86i_lock, "v86i lock", MUTEX_DEFAULT,
			DEFAULT_WT);
		locked_or(ip->v86i_state, INITIALIZED);
		thread_unlock(t);
	}
#endif

	mutex_enter(&ip->v86i_lock);

	/* If there is already stashed information, remove it */
	if (ip->v86i_t) {
		v86unlink(ip);
	}

#if 0
	/* Identify process from the v86blk if given, otherwise use the
	 * current process.
	 */
	if (v86b)
		t = v86b->v86b_t;
	else
		t = curthread;
#endif

	/* Get permission to change the v86int_t list */
	p = ttoproc(t);
	mutex_enter(&p->p_lock);

	/* If process is VP/ix, link up this structure and save the
	 * proc structure pointer.
	 */
	if ((v86p = (v86_t *)t->t_v86data) != 0) {
		ip->v86i_i = v86p->vp_ilist;
		v86p->vp_ilist = ip;
		ip->v86i_t = t;
		answer = 1;
	}

	mutex_exit(&p->p_lock);
	mutex_exit(&ip->v86i_lock);
	return (answer);
}


void
v86unstash(ip)
v86int_t *ip;
{

	/* If structure has not been initialized, there cannot be anything
	 * to unstash.
	 */
	if (ip->v86i_state != INITIALIZED) {
		return;
	}

	mutex_enter(&ip->v86i_lock);

	/* Unstash only if really stashed */
	if (ip->v86i_t) {
		v86unlink(ip);
	}

	mutex_exit(&ip->v86i_lock);
}


void
v86deliver(ip, intr_bits)
v86int_t *ip;
int intr_bits;
{

	/* Preliminary check can often save the lock overhead */
	if (ip->v86i_t == 0)
		return;

#if XXX_MP
	/* The following test is a "can't happen" because the above test
	 * is stronger.  The commented out code is left in place as a
	 * reminder in case the earlier test is removed.
	 */
/*	if (ip->v86i_state != INITIALIZED) {		*/
/*		return;					*/
/*	}						*/
#endif

	mutex_enter(&ip->v86i_lock);

	/* Recheck the pointer in case it changed while acquiring lock.
	 * If pointer is still set, send the pseudorupt.
	 */
	if (ip->v86i_t) {
		if (v86setpseudo(ip->v86i_t, intr_bits) == 0) {
			/* We failed to deliver the pseudorupt.  The
			 * process must be in v86exit().  Go ahead
			 * and unlink from here because it is easier
			 * than in v86exit().  See v86exit() for details.
			 */
			v86unlink(ip);
		}
	}

	mutex_exit(&ip->v86i_lock);
}


void
v86sdeliver(ip, intr_bits, qp)
	v86int_t *ip;
	int intr_bits;
	queue_t *qp;
{
	mblk_t *bp;
	v86msg_t *v86m;

	if ((bp = allocb(sizeof(v86msg_t), BPRI_MED)) != 0) {
		bp->b_datap->db_type = M_VPIXINT;
		v86m = (v86msg_t *)bp->b_wptr;
		v86m->v86m_i = ip;
		v86m->v86m_m = intr_bits;
		bp->b_wptr += sizeof(v86msg_t);
		putnext(qp, bp);			
	}
}


static int
v86unlink(ip)
v86int_t *ip;
{
	v86int_t *lp;
	kthread_id_t	t = ip->v86i_t;
	proc_t 		*p = ttoproc(t);
	v86_t 		*v86p = (v86_t *)t->t_v86data;

	/* The hierarchy for mutex locks is such that when locking both
	 * a v86int_t lock and a process structure lock, the v86int_t
	 * lock is acquired first.  v86unlink assumes that the v86int_t
	 * structure is already locked.  Holding p_lock allows us to
	 * change the v86int_t list.
	 */
	mutex_enter(&p->p_lock);
	ip->v86i_t = 0;

	/* Could assert here that v86p and v86p->vp_ilist is non-null */
	if (v86p->vp_ilist == ip)
		v86p->vp_ilist = ip->v86i_i;
	else {
		for (lp = v86p->vp_ilist; lp; lp = lp->v86i_i) {
			if (lp->v86i_i == ip) {
				lp->v86i_i = ip->v86i_i;
				break;
			}
		}
		/* Could assert here that lp is non-null */
	}
	mutex_exit(&p->p_lock);
}


static int
v86scrscan()
{
	v86_t       *v86p;		/* pointer to the v86 struct        */
	struct	as  *as;                /* address space for the process    */
	struct  seg *seg;               /* pointer to vpix segment          */
	int	    ret;

#ifdef DEBUG1
	cmn_err(CE_CONT,"In v86scrscan\n"); 
#endif

	/* Scan the screen memory which is designated by  vp_membase */
	/* to  vp_membase  +  vp_memlen for page modified bits.  For */
	/* each modified bit, set a bit in a word to be  written  to */
	/* the user.  And clear the modified bit for the next scan.  */

	/* This routine is presumed to  be  called  with  interrupts */
	/* disabled.  This is just in case the pager decides to page */
	/* out a page that we are looking at.                        */

	/* Make sure this is a v86 process and it wants tracking.    */

	if ((v86p = (v86_t *) curthread->t_v86data) == NULL ||
		v86p->vp_mem.vmem_memlen == 0 ||
		(v86p->vp_xtss->xt_viflag & V86VI_MEMORY))
	    return;

	/* Find the segment corresponding to the base address. */

	if ((as = ttoproc(curthread)->p_as) == (struct as *)NULL)
		cmn_err(CE_PANIC, "v86scrscan: no as allocated");

	AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
	if ((seg = as_segat(as, v86p->vp_mem.vmem_membase)) ==
				(struct seg *)NULL) {
#ifdef DEBUG1
		cmn_err(CE_CONT, "v86scrscan: as_segat failed\n");
#endif
		AS_LOCK_EXIT(as, &as->a_lock);
		return;
	}
	AS_LOCK_EXIT(as, &as->a_lock);

	/* It must be a seg_vpix segment. */

	if (seg->s_ops != &segvpix_ops) {
#ifdef DEBUG1
		cmn_err(CE_CONT, "v86scrscan: as_segat failed\n");
#endif
		return;
	}

	/*
 	 * Scan the screen memory for modified bits.
	 * XXX - segvpix_modscan() returns 0 if no pages in the specified
	 *	 range are modified, otherwise it returns non zero.
	 */

	AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
	ret =  segvpix_modscan(seg, btop(v86p->vp_mem.vmem_membase),
				      btop(v86p->vp_mem.vmem_memlen));
	AS_LOCK_EXIT(as, &as->a_lock);
	return ret;
}


static
v86physmap(begmapaddr, length, begphysaddr)
	caddr_t begmapaddr;             /* address to start mapping at      */
	int     length;                 /* byte count to map                */
	paddr_t begphysaddr;            /* physical address to map to       */
{
	v86_t       *v86p;		/* pointer to the v86 struct        */
	struct	as  *as;		/* process's address space	    */
	struct  seg *seg;		/* vp/ix segment for process	    */
	u_int	     npages;		/* number of pages to map	    */

#ifdef DEBUG1
	cmn_err(CE_CONT, "In v86physmap\n");
#endif

	/*
	 * Validate the request arguments.
	 */

	/* Make sure this process is a v86 process. */
	if ((v86p = (v86_t *) curthread->t_v86data) == NULL)
		return EACCES;

	/* Find the segment corresponding to the base address. */

	if ((as = ttoproc(curthread)->p_as) == (struct as *)NULL)
		cmn_err(CE_PANIC, "v86physmap: no as allocated");

	AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
	if ((seg = as_segat(as, begmapaddr)) == (struct seg *)NULL) {
#ifdef DEBUG1
		cmn_err(CE_CONT, "v86physmap: as_segat failed\n");
#endif
		AS_LOCK_EXIT(as, &as->a_lock);
		return EACCES;
	}
	AS_LOCK_EXIT(as, &as->a_lock);

	/* It must be a seg_vpix segment. */

	if (seg->s_ops != &segvpix_ops) {
#ifdef DEBUG1
		cmn_err(CE_CONT, "v86physmap: Not a vpix segment\n");
#endif
		return EACCES;
	}

	/* The desired range must fall within the segment size. */

	ASSERT(seg->s_base == 0);

	if (begmapaddr + length > (caddr_t)seg->s_size) {
#ifdef DEBUG1
		cmn_err(CE_CONT, "v86physmap: Out of range\n");
#endif
		return EACCES;
	}

	/*
	 * Perform the actual mapping.
	 */
	npages = btopr(begmapaddr + length) - btop(begmapaddr);

	return _v86physmap(seg, btop(begmapaddr),
				btop(begphysaddr),
				npages);
}

static int
_v86physmap(seg, vpage, ppage, npages)
	struct seg	*seg;
	u_int		vpage;
	u_int		ppage;
	u_int		npages;
{
	paddr_t		seg_start, seg_end;
	u_int		cnt;
	struct memlist	*pmem;

	extern struct	memlist	*phys_install;

	/*
	 * Make sure the physical addresses we wish to map are not part
	 * of system memory.
	 */

	for (pmem = phys_install; pmem; pmem = pmem->next) {
		if (mmu_ptob(ppage) < (pmem->size + pmem->address) &&
		    mmu_ptob(ppage + npages) >= pmem->address) {
			return EIO;
		}
	}

	/* Seg_vpix does the actual mapping. */

	return segvpix_physmap(seg, vpage, ppage, npages);
}

static
v86unphys(begmapaddr, length)
	caddr_t begmapaddr;             /* address of start of mapping      */
	int     length;                 /* length, in bytes, of mapping     */
{
	v86_t       *v86p;		/* pointer to the v86 struct        */
	struct	as  *as;		/* process's address space	    */
	struct  seg *seg;		/* vp/ix segment for process	    */
	u_int	     npages;		/* number of pages to unmap	    */

#ifdef DEBUG1
	cmn_err(CE_CONT, "In v86unphys\n");
#endif

	/*
	 * Validate the request arguments.
	 */

	/* Make sure this process is a v86 process. */
	if ((v86p = (v86_t *)curthread->t_v86data) == NULL)
		return EACCES;

	/* Find the segment corresponding to the base address. */

	if ((as = ttoproc(curthread)->p_as) == (struct as *)NULL)
		cmn_err(CE_PANIC, "v86unphys: no as allocated");

	AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
	if ((seg = as_segat(as, begmapaddr)) == (struct seg *)NULL) {
#ifdef DEBUG1
		cmn_err(CE_CONT, "v86unphys: as_segat failed\n");
#endif
		AS_LOCK_EXIT(as, &as->a_lock);
		return EACCES;
	}
	AS_LOCK_EXIT(as, &as->a_lock);

	/* It must be a seg_vpix segment. */

	if (seg->s_ops != &segvpix_ops) {
#ifdef DEBUG1
		cmn_err(CE_CONT, "v86unphys: Not a vpix segment\n");
#endif
		return EACCES;
	}

	/* The desired range must fall within the segment size. */

	ASSERT(seg->s_base == 0);

	if (begmapaddr + length > (caddr_t)seg->s_size) {
#ifdef DEBUG1
		cmn_err(CE_CONT, "v86unphys: Out of range\n");
#endif
		return EACCES;
	}

	/*
	 * Perform the actual unmapping.
	 */

	npages = btopr(begmapaddr + length) - btop(begmapaddr);

	return segvpix_unphys(seg, btop(begmapaddr), npages);
}

/*
 * v86swtch() called from resume() to setup the TSS/LDT selectors during
 * a thread switch.
 */
int
v86swtch(nt)
	kthread_id_t	nt;	/* in coming thread */
{
	struct seg_desc *gdt = CPU->cpu_gdt;
	v86_t *vp;

#ifdef XXX_LDT
	/*
	 * Note: Since the current VPix support doesn't use private LDT,
	 *	 this code should probably be in a seperate routine
	 *	 when we support private LDTs. For now, this will be
	 *	 useful as a reference.
	 */
	extern seg_desc ldt_default_desc;

	/*
	 * If the new thread uses a private LDT then fix the GDT
	 * entry for LDT selector. Otherwise, use the default descriptor.
	 */

	if (ttolwp(nt)->lwp_ldt != NULL) {
		gdt[seltoi(LDTSEL)] = ttolwp(nt)->lwp_ldt_desc;
		/* Reload ldt register */
	}
	else {
		gdt[seltoi(LDTSEL)] = ldt_default_desc;
		/* Reload ldt register */
	}
#endif

	/*
	 * If the incoming process is a dual-mode process then:
	 *	a) If he entered the kernel in virtual v86 mode, we must
	 *	   map his XTSS.
	 *	b) If he entered the kernel in protected mode then make 
	 *	   sure that the XTSS descriptor is marked BUSY, because
	 *	   the ECT will IRET into it. Also make sure the NT bit
	 *	   in the FLAGS register (on the thread stack) of the
	 *	   new process is set. Also make sure the t_link in 386
	 *	   task is set to XTSSSEL.
	 */
	vp = (v86_t *)nt->t_v86data;
	if (vp) {
		gdt[seltoi(XTSSSEL)] = vp->vp_xtss_desc;
		CPU->cpu_v86procflag = 1; /* user-mode idt switch */
		if (lwptoregs(ttolwp(nt))->r_efl & PS_VM) { /* case (a) */
			/*
			 * Make sure the xtss has interrupts disabled, in
			 * case we were swapped out. XXX - check?
			 */
			vp->vp_xtss->xt_tss.t_eflags &= ~PS_IE;
			loadtr(XTSSSEL);
			setdscracc1(&gdt[seltoi(UTSSSEL)], TSS3_KACC1);
		}
		else { /* case (b) */
			CPU->cpu_tss->t_link = XTSSSEL;
			if (get_tr() != UTSSSEL) {
				setdscracc1(&gdt[seltoi(UTSSSEL)], TSS3_KACC1);
				loadtr(UTSSSEL);
			}
			setdscracc1(&gdt[seltoi(XTSSSEL)], TSS3_KBACC1);
			lwptoregs(ttolwp(nt))->r_efl |= PS_NT;
		}
#if XXX_MP
		/*
		 * Fix cr3, XXX?
		 */
		vp->vp_xtss->xt_tss.t_cr3 = CPU->cpu_cr3;
#endif
	}
	else {
		CPU->cpu_v86procflag = 0; /* no user-mode idt switch */
		if (get_tr() != UTSSSEL) {
			/* 
			 * We are on XTSS, make sure the TSS is marked 
			 * available.
			 */
			setdscracc1(&gdt[seltoi(UTSSSEL)], TSS3_KACC1);
			loadtr(UTSSSEL); /* XXX - redundant? see locore.s */
		}
		/* invalidate access to XTSS */
		setdscracc1(&gdt[seltoi(XTSSSEL)], 0); /* XXX - fix it */
	}
}
#endif /* _VPIX */
