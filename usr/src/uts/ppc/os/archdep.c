/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)archdep.c	1.54	96/06/18 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/vmparam.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/stack.h>
#include <sys/reg.h>
#include <sys/frame.h>
#include <sys/proc.h>
#include <sys/ucontext.h>
#include <sys/siginfo.h>
#include <sys/cpuvar.h>
#include <sys/asm_linkage.h>
#include <sys/kmem.h>
#include <sys/errno.h>
#include <sys/callo.h>
#include <sys/bootconf.h>
#include <sys/archsystm.h>
#include <sys/fpu/fpusystm.h>
#include <sys/debug.h>
#include <sys/elf.h>
#include <sys/psw.h>
#include <sys/spl.h>

extern struct bootops *bootops;
extern struct fpu initial_fpu_state;
extern void installctx(kthread_id_t, int, void (*)(),
	void (*)(), void (*)(), void (*)());

/*
 * Set floating-point registers.
 */
void
setfpregs(
	register klwp_id_t lwp,
	register fpregset_t *fp)
{

	register struct pcb *pcb;
	register fpregset_t *pfp;
	register struct regs *reg;


	if (fp->fpu_valid) { /* User specified a valid FP state */
		pcb = &lwp->lwp_pcb;
		pfp = &pcb->pcb_fpu;
		if (!(pcb->pcb_flags & PCB_FPU_INITIALIZED)) {
			/*
			 * This lwp didn't have a FP context setup before,
			 * do it now and copy the user specified state into
			 * pcb.
			 */
			installctx(lwptot(lwp), (int)pfp, fp_save, fp_restore,
				fp_fork, fp_free);
			pcb->pcb_flags |= PCB_FPU_INITIALIZED;
		} else if (!(pcb->pcb_fpu.fpu_valid)) {
			/*
			 * FPU context is still active, release the
			 * ownership.
			 */
			fp_free(&pcb->pcb_fpu);
		}
		kcopy((caddr_t)fp, (caddr_t)pfp, sizeof (struct fpu));
		pfp->fpu_valid = 1;
		pcb->pcb_flags |= PCB_FPU_STATE_MODIFIED;
		reg = lwptoregs(lwp);
		reg->r_msr &= ~MSR_FP; /* disable FP */
	}
}

/*
 * Get floating-point registers.  The u-block is mapped in here (not by
 * the caller).
 */
void
getfpregs(
	register klwp_id_t lwp,
	register fpregset_t *fp)
{
	register struct pcb *pcb;

	pcb = &lwp->lwp_pcb;

	if (pcb->pcb_flags & PCB_FPU_INITIALIZED) {
		kpreempt_disable();
		/*
		 * If we have FPU hw and the thread's pcb doesn't have
		 * a valid FPU state then get the state from the hw.
		 */
		if (!(pcb->pcb_fpu.fpu_valid)) {
			fp_save(&pcb->pcb_fpu); /* get the current FPU state */
		}
		kpreempt_enable();
		kcopy((caddr_t)&pcb->pcb_fpu, (caddr_t)fp, sizeof (struct fpu));
	} else {
		/*
		 * This lwp doesn't have a FP context setup so we just
		 * initialize the user buffer with initial state which is
		 * same as fpu_hw_init().
		 */
		kcopy((caddr_t)&initial_fpu_state, (caddr_t)fp,
			sizeof (struct fpu));
	}
}

/*
 * Return the general registers
 */
void
getgregs(lwp, rp)
	register klwp_id_t lwp;
	register gregset_t rp;
{
	bcopy((caddr_t)lwp->lwp_regs, (caddr_t)rp, sizeof (gregset_t));
}

/*
 * Return the user-level PC.
 * If in a system call, return the address of the syscall trap.
 */
greg_t
getuserpc()
{
	greg_t pc = lwptoregs(ttolwp(curthread))->r_pc;
	if (curthread->t_sysnum)
		pc -= 4;	/* size of an sc instruction */
	return (pc);
}

/*
 * Set general registers.
 */
void
setgregs(lwp, rp)
	register klwp_id_t lwp;
	register gregset_t rp;
{
	register greg_t *reg;
	register greg_t old_msr;

	reg = (greg_t *)lwp->lwp_regs;

	/*
	 * Except MSR register other registers are user modifiable.
	 */
	old_msr = reg[R_MSR]; /* save the old MSR */

	/* copy saved registers from user stack */
	bcopy((caddr_t)rp, (caddr_t)reg, sizeof (gregset_t));

	reg[R_MSR] = old_msr; /* restore the MSR */

	if (lwptot(lwp) == curthread) {
		lwp->lwp_eosys = JUSTRETURN;	/* so cond reg. wont change */
		curthread->t_post_sys = 1;	/* so regs will be restored */
	}
}

/*
 * Get a pc-only stacktrace.  Used for kmem_alloc() buffer ownership tracking.
 * Returns MIN(current stack depth, pcstack_limit).
 */
int
getpcstack(u_int *pcstack, int pcstack_limit)
{
	struct minframe *stacktop = (struct minframe *)curthread->t_stk;
	struct minframe *prevfp = (struct minframe *)KERNELBASE;
	struct minframe *fp = (struct minframe *)getfp();
	int depth = 0;

	while (fp > prevfp && fp < stacktop && depth < pcstack_limit) {
		pcstack[depth++] = (u_int)fp->fr_savpc;
		prevfp = fp;
		fp = (struct minframe *)fp->fr_savfp;
	}
	return (depth);
}

/*
 * Allocator for routines which need memory before kmem_init has
 * finished.   Storage returned must be freed via startup_free().
 *
 * Assumes single-threaded, which is right until kmem_init() (at least).
 *
 * This was put here for mutex statistics, and isn't very space efficient.
 * Do not overuse this.
 */
extern caddr_t startup_alloc_vaddr;
extern caddr_t startup_alloc_size;

extern u_int	startup_alloc_chunk_size;

void *
startup_alloc(size_t size, void ** header)
{
	caddr_t	p, q;
	extern u_int kernelmap;

	ASSERT(size < MMU_PAGESIZE);
	ASSERT(!kmem_ready);
	ASSERT(size != 0);

	/*
	 * Make sure size is at least big enough for a double.
	 * This also guarantees double-word alignment.
	 * PTR24_ALIGN is bigger than a double and a power of 2.
	 */
	size = (size + PTR24_ALIGN - 1) & ~(PTR24_ALIGN - 1);

	if (*header == NULL) {
		u_int	chunk_size = startup_alloc_chunk_size;

		ASSERT(kernelmap == 0);	/* don't alloc after kernelmap setup */
		/*
		 * nothing free, grab some pages from the boot and put pieces
		 * on the free list.  This works best when size is small.
		 */
		p = (caddr_t)BOP_ALLOC(bootops,
			startup_alloc_vaddr, chunk_size, BO_NO_ALIGN);
		startup_alloc_vaddr = p + chunk_size;
		startup_alloc_size += chunk_size;
		q = p + chunk_size;
		for (; p + size < q; p = p + size) {
			* (void **)p = *header; /* add p to free list */
			*header = (void *)p;
		}
	}

	p = (caddr_t)*header;
	*header = (void *)*(caddr_t *)p;
	bzero(p, size);
	return ((void *)p);
}

/* ARGSUSED */
void
startup_free(void *p, size_t size, void **header)
{
	*(void **)p = *header;
	*header = p;
}

/*
 * The following ELF header fields are defined as processor-specific
 * in the V8 ABI:
 *
 *	e_ident[EI_DATA]	encoding of the processor-specific
 *				data in the object file
 *	e_machine		processor identification
 *	e_flags			processor-specific flags associated
 *				with the file
 */

/*
 * The value of at_flags reflects a platform's cpu module support.
 * at_flags is used to check for allowing a binary to execute and
 * is passed as the value of the AT_FLAGS auxiliary vector.
 */
int at_flags = 0;

/*
 * Check the processor-specific fields of an ELF header.
 *
 * returns 1 if the fields are valid, 0 otherwise
 */
/*ARGSUSED2*/
int
elfheadcheck(
	unsigned char e_data,
	Elf32_Half e_machine,
	Elf32_Word e_flags)
{
	if ((e_data != ELFDATA2LSB) || (e_machine != EM_PPC))
		return (0);
	return (1);
}

/*
 * Set the processor-specific fields of an ELF header.
 */
void
elfheadset(
	unsigned char *e_data,
	Elf32_Half *e_machine,
	Elf32_Word *e_flags)
{
	*e_data = ELFDATA2LSB;
	*e_machine = EM_PPC;
	*e_flags = 0;
}

/*
 *	sync_icache() - this is called
 *	in proc/fs/prusrio.c.
 */
/* ARGSUSED */
void
sync_icache(caddr_t addr, u_int len)
{
	sync_instruction_memory(addr, len);
}

int
__ipltospl(int ipl)
{
	return (ipltospl(ipl));
}
