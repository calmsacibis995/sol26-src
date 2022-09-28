/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident "@(#)archdep.c	1.36	96/06/20 SMI"	/* from SVr4.0 1.83 */
/* Was sparcdep.c - OSAC naming decision */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/vmparam.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/stack.h>
#include <sys/reg.h>
#include <sys/frame.h>
#include <sys/proc.h>
#include <sys/psw.h>
#include <sys/siginfo.h>
#include <sys/cpuvar.h>
#include <sys/asm_linkage.h>
#include <sys/kmem.h>
#include <sys/errno.h>
#include <sys/callo.h>
#include <sys/bootconf.h>
#include <sys/archsystm.h>
#include <sys/debug.h>
#include <sys/elf.h>
#include <sys/spl.h>

extern struct bootops *bootops;
extern void fp_save();
extern void fp_restore();
extern void fp_fork();
extern void fp_free();


/*
 * Advertised via /etc/system.
 * Does execution of coff binaries bring in /usr/lib/cbcp.
 */
int enable_cbcp = 1;

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

	pcb = &lwp->lwp_pcb;
	pfp = &pcb->pcb_fpu.fpu_regs;

	if (pcb->pcb_fpu.fpu_flags & FPU_EN) {
		if (!(pcb->pcb_fpu.fpu_flags & FPU_VALID)) {
			/*
			 * FPU context is still active, release the
			 * ownership.
			 */
			fp_free(&pcb->pcb_fpu);
		}
		kcopy((caddr_t)fp, (caddr_t)pfp, sizeof (struct fpu));
		pcb->pcb_fpu.fpu_flags |= FPU_VALID;
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
	register fpregset_t *pfp;
	register struct pcb *pcb;
	extern int fpu_exists;

	pcb = &lwp->lwp_pcb;
	pfp = &pcb->pcb_fpu.fpu_regs;

	kpreempt_disable();
	if (pcb->pcb_fpu.fpu_flags & FPU_EN) {
		/*
		 * If we have FPU hw and the thread's pcb doesn't have
		 * a valid FPU state then get the state from the hw.
		 */
		if (fpu_exists && !(pcb->pcb_fpu.fpu_flags & FPU_VALID))
			fp_save(&pcb->pcb_fpu); /* get the current FPU state */
		kcopy((caddr_t)pfp, (caddr_t)fp, sizeof (struct fpu));
	}
	kpreempt_enable();
}

/*
 * Return the general registers
 */
void
getgregs(lwp, rp)
	register klwp_id_t lwp;
	register gregset_t rp;
{
	register greg_t *reg;

	reg = (greg_t *)lwp->lwp_regs;
	bcopy((caddr_t)reg, (caddr_t)rp, sizeof (gregset_t));
}

/*
 * Return the user-level PC.
 * If in a system call, return the address of the syscall trap.
 */
greg_t
getuserpc()
{
	greg_t eip = lwptoregs(ttolwp(curthread))->r_eip;
	if (curthread->t_sysnum)
		eip -= 7;	/* size of an lcall instruction */
	return (eip);
}

/*
 * Set general registers.
 */
void
setgregs(lwp, rp)
	register klwp_id_t lwp;
	register gregset_t rp;
{
	register struct regs *reg;
	void chksegregval();

	reg = lwptoregs(lwp);

	/*
	 * Only certain bits of the EFL can be modified.
	 */
	rp[EFL] = (reg->r_ps & ~PSL_USERMASK) | (rp[EFL] & PSL_USERMASK);

	/* copy saved registers from user stack */
	bcopy((caddr_t)rp, (caddr_t)reg, sizeof (gregset_t));

	/*
	 * protect segment registers from non-user privilege levels,
	 * and GDT selectors other than FPESEL
	 */
	chksegregval(&(reg->r_fs));
	chksegregval(&(reg->r_gs));
	chksegregval(&(reg->r_ss));
	chksegregval(&(reg->r_cs));
	chksegregval(&(reg->r_ds));
	chksegregval(&(reg->r_es));
	/*
	 * Set the flag lwp_gpfault to catch GP faults when going back
	 * to user mode.
	 */
	lwp->lwp_gpfault = 1;
}

/*
 * Get a pc-only stacktrace.  Used for kmem_alloc() buffer ownership tracking.
 * Returns MIN(current stack depth, pcstack_limit).
 */
int
getpcstack(u_int *pcstack, int pcstack_limit)
{
	struct frame *stacktop = (struct frame *)curthread->t_stk;
	struct frame *prevfp = (struct frame *)KERNELBASE;
	struct frame *fp = (struct frame *)getfp();
	int depth = 0;

	while (fp > prevfp && fp < stacktop && depth < pcstack_limit) {
		pcstack[depth++] = (u_int)fp->fr_savpc;
		prevfp = fp;
		fp = (struct frame *)fp->fr_savfp;
	}
	return (depth);
}

/*
 * Check segment register value that will be restored when going to
 * user mode.
 */

void
chksegregval(srp)
	register int *srp;
{
	register int sr;

	sr = *srp;
	if ((sr & 0x0000FFFF) != 0 && (sr & 0x0000FFFF) != FPESEL)
		*srp |= 7;	/* LDT and RPL 3 */
	/* else null selector or FPESEL OK */
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
	if ((e_data != ELFDATA2LSB) || (e_machine != EM_386))
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
	*e_machine = EM_386;
	*e_flags = 0;
}

/*
 *	sync_icache() - this is called
 *	in proc/fs/prusrio.c. x86 has an unified cache and therefore
 *	this is a nop.
 */
/* ARGSUSED */
void
sync_icache(caddr_t addr, u_int len)
{
	/* Do nothing for now */
}

int
__ipltospl(int ipl)
{
	return (ipltospl(ipl));
}
