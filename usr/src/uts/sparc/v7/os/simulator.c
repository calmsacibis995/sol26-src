
/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */
#ident	"@(#)simulator.c	1.22	96/09/12 SMI"

/* common code with bug fixes from original version in trap.c */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/inline.h>
#include <sys/debug.h>
#include <sys/privregs.h>
#include <sys/machpcb.h>
#include <sys/simulate.h>
#include <sys/proc.h>
#include <sys/cmn_err.h>
#include <sys/stack.h>
#include <sys/watchpoint.h>

static int aligndebug = 0;

static int getreg(u_int *, u_int *, u_int, u_int *, caddr_t *, u_int);
static int putreg(u_int, u_int *, u_int *, u_int, caddr_t *, u_int);

/*
 * For the sake of those who must be compatible with unaligned
 * architectures, users can link their programs to use a
 * corrective trap handler that will fix unaligned references
 * a special trap #6 (T_FIX_ALIGN) enables this 'feature'.
 * Returns 1 for success, 0 for failure.
 */
char *sizestr[] = {"word", "byte", "halfword", "double"};

int
do_unaligned(rp, badaddr)
	register struct regs	*rp;
	caddr_t			*badaddr;
{
	proc_t	*p = curproc;
	int	mapped = 0;
	u_int	inst;
	u_int	rd, rs1, rs2;
	u_int	*rgs;
	u_int	*rw;
	int	sz;
	int	floatflg;
	int	immflg;
	int 	addr;
	u_int		val;
	extern void	_fp_read_pfreg();
	extern void	_fp_write_pfreg();
	extern int 	fpu_exists;
	union ud {
		double	d;
		u_int	i[2];
		u_short	s[4];
		u_char	c[8];
	} data;

	ASSERT(USERMODE(rp->r_psr));

	/* get the instruction */
	addr = rp->r_pc;
	if (p->p_warea)		/* watchpoints in effect */
		mapped = pr_mappage((caddr_t)addr, sizeof (int), S_READ, 1);
	inst = _fuiword((int *)addr);
	if (mapped)
		pr_unmappage((caddr_t)addr, sizeof (int), S_READ, 1);

	rd = (inst >> 25) & 0x1f;
	rs1 = (inst >> 14) & 0x1f;
	rs2 = inst & 0x1f;
	floatflg = (inst >> 24) & 1;
	immflg = (inst >> 13) & 1;

	switch ((inst >> 19) & 3) {	/* map size bits to a number */
	case 0: sz = 4; break;
	case 1: sz = 1; break;
	case 2: sz = 2; break;
	case 3: sz = 8; break;
	}

	if (aligndebug) {
		printf("unaligned access at 0x%x, instruction: 0x%x\n",
		    rp->r_pc, inst);
		printf("type %s %s %s\n",
			(((inst >> 21) & 1) ? "st" : "ld"),
			(((inst >> 22) & 1) ? "signed" : "unsigned"),
			sizestr[((inst >> 19) & 3)]);
		printf("rd = %d, rs1 = %d, rs2 = %d, imm13 = 0x%x\n",
		    rd, rs1, rs2, (inst & 0x1fff));
	}

	/* if not load or store, or to alternate space do nothing */
	if (((inst >> 30) != 3) ||
	    (immflg == 0 && ((inst >> 5) & 0xff)))
		return (0);		/* don't support alternate ASI */

	(void) flush_user_windows_to_stack(NULL); /* flush windows to memory */
	rgs = (u_int *)&rp->r_y;		/* globals and outs */
	rw = (u_int *)rp->r_sp;			/* ins and locals */

	if (getreg(rgs, rw, rs1, &val, badaddr, KERNELBASE - 1))
		return (SIMU_FAULT);
	addr = val;

	/* check immediate bit and use immediate field or reg (rs2) */
	if (immflg) {
		register int imm;
		imm  = inst & 0x1fff;		/* mask out immediate field */
		imm <<= 19;			/* sign extend it */
		imm >>= 19;
		addr += imm;			/* compute address */
	} else {
		if (getreg(rgs, rw, rs2, &val, badaddr, KERNELBASE - 1))
			return (SIMU_FAULT);
		addr += val;
	}

	if (aligndebug)
		printf("addr = 0x%x\n", addr);
	if ((u_int)addr >= KERNELBASE) {
		*badaddr = (caddr_t)addr;
		goto badret;
	}

	/* a single bit differentiates ld and st */
	if ((inst >> 21) & 1) {			/* store */
		if (floatflg) {
			/* if fp read fpu reg */
			if (fpu_exists) {
				if ((inst >> 19) & 0x3f == 0x25) {
					_fp_read_pfsr(&data.i[0]);
				} else {
					_fp_read_pfreg(&data.i[0], rd);
					if (sz == 8)
					    _fp_read_pfreg(&data.i[1], rd+1);
				}
			} else {
				klwp_id_t lwp = ttolwp(curthread);
				kfpu_t *fp = lwptofpu(lwp);
				if ((inst >> 19) & 0x3f == 0x25) {
					data.i[0] = (u_int)fp->fpu_fsr;
				} else {
					data.i[0] =
					    (u_int)fp->fpu_fr.fpu_regs[rd];
					if (sz == 8)
					    data.i[1] = (u_int)
						fp->fpu_fr.fpu_regs[rd+1];
				}
			}
		} else {
			if (getreg(rgs, rw, rd, &data.i[0], badaddr,
			    KERNELBASE - 1))
				return (SIMU_FAULT);
			if (sz == 8 &&
			    getreg(rgs, rw, rd+1, &data.i[1], badaddr,
			    KERNELBASE - 1))
				return (SIMU_FAULT);
		}

		if (aligndebug) {
			printf("data %x %x %x %x %x %x %x %x\n",
				data.c[0], data.c[1], data.c[2], data.c[3],
				data.c[4], data.c[5], data.c[6], data.c[7]);
		}

		if (sz == 1) {
			if (copyout((caddr_t)&data.c[3], (caddr_t)addr,
			    (u_int)sz) == -1)
				goto badret;
		} else if (sz == 2) {
			if (copyout((caddr_t)&data.s[1], (caddr_t)addr,
			    (u_int)sz) == -1)
				goto badret;
		} else {
			if (copyout((caddr_t)&data.i[0], (caddr_t)addr,
			    (u_int)sz) == -1)
				goto badret;
		}
	} else {				/* load */
		if (sz == 1) {
			if (copyin((caddr_t)addr, (caddr_t)&data.c[3],
			    (u_int)sz) == -1)
				goto badret;
			/* if signed and the sign bit is set extend it */
			if (((inst >> 22) & 1) && ((data.c[3] >> 7) & 1)) {
				data.s[0] = (u_short)-1; /* extend sign bit */
				data.c[2] = (u_char)-1;
			} else {
				data.s[0] = 0;	/* clear upper 24 bits */
				data.c[2] = 0;
			}
		} else if (sz == 2) {
			if (copyin((caddr_t)addr, (caddr_t)&data.s[1],
			    (u_int)sz) == -1)
				goto badret;
			/* if signed and the sign bit is set extend it */
			if (((inst >> 22) & 1) && ((data.s[1] >> 15) & 1))
				data.s[0] = (u_short)-1; /* extend sign bit */
			else
				data.s[0] = 0;	/* clear upper 16 bits */
		} else
			if (copyin((caddr_t)addr, (caddr_t)&data.i[0],
			    (u_int)sz) == -1)
				goto badret;

		if (aligndebug) {
			printf("data %x %x %x %x %x %x %x %x\n",
				data.c[0], data.c[1], data.c[2], data.c[3],
				data.c[4], data.c[5], data.c[6], data.c[7]);
		}

		if (floatflg) {		/* if fp, write fpu reg */
			if (fpu_exists) {
				if ((inst >> 19) & 0x3f == 0x21) {
					_fp_write_pfsr(&data.i[0]);
				} else {
					_fp_write_pfreg(&data.i[0], rd);
					if (sz == 8)
					    _fp_write_pfreg(&data.i[1], rd+1);
				}
			} else {
				klwp_id_t lwp = ttolwp(curthread);
				kfpu_t *fp = lwptofpu(lwp);
				if ((inst >> 19) & 0x3f == 0x21) {
					fp->fpu_fsr = data.i[0];
				} else {
					fp->fpu_fr.fpu_regs[rd] = data.i[0];
					if (sz == 8)
						fp->fpu_fr.fpu_regs[rd+1] =
						    data.i[1];
				}
			}
		} else {
			if (putreg(data.i[0], rgs, rw, rd, badaddr,
			    KERNELBASE - 1))
				goto badret;
			if (sz == 8 &&
			    putreg(data.i[1], rgs, rw, rd+1, badaddr,
			    KERNELBASE - 1))
				goto badret;
		}
	}
	return (SIMU_SUCCESS);
badret:
	return (SIMU_FAULT);
}

/*
 * simulate unimplemented instructions (swap, mul, div)
 */
int
simulate_unimp(rp, badaddr)
	struct regs	*rp;
	caddr_t		*badaddr;
{
	u_int		inst;
	int		rv;
	int		do_swap();
	proc_t		*p = curproc;
	caddr_t		addr = (caddr_t)rp->r_pc;
	int		mapped = 0;

	if (rp->r_pc < KERNELBASE) {	/* user land */
		if (p->p_warea)		/* watchpoints in effect */
			mapped = pr_mappage(addr, sizeof (int), S_READ, 1);
		inst = _fuiword((int *)addr);
		if (mapped)
			pr_unmappage(addr, sizeof (int), S_READ, 1);
		if (inst == (u_int)(-1)) {
			/*
			 * -1 is an illegal instruction or a error in fuiword,
			 * give up now
			 */
			return (SIMU_ILLEGAL);
		}

		/*
		 * Simulation depends on the stack having the most
		 * up-to-date copy of the register windows.
		 */
		(void) flush_user_windows_to_stack(NULL);
	} else	/* kernel land */
		inst = *(int *)rp->r_pc;

	/*
	 * Check for the unimplemented swap instruction.
	 *
	 * Note:  there used to be support for simulating swap
	 * instructions used by the kernel.  The kernel doesn't
	 * use swap, and shouldn't use it unless it's in hardware.
	 * If the kernel needs swap and it isn't implemented, it'll
	 * be extremely difficult to simulate.
	 */

	if ((inst & 0xc1f80000) == 0xc0780000)
		return (do_swap(rp, inst, badaddr));

#ifndef	__sparcv9cpu
	/*
	 * for mul/div instruction switch on op3 field of instruction
	 * if the two bit op field is 0x2
	 */
	if ((inst >> 30) == 0x2) {
		register u_int *rgs;		/* pointer to struct regs */
		register u_int *rw;		/* pointer to frame */
		u_int rs1, rs2, rd;
		u_int	dest;			/* place for destination */

		rd =  (inst >> 25) & 0x1f;
		rgs = (u_int *)&rp->r_y;	/* globals and outs */
		rw = (u_int *)rp->r_sp;		/* ins and locals */

		/* generate first operand rs1 */
		if (getreg(rgs, rw, ((inst >> 14) & 0x1f), &rs1, badaddr,
		    rp->r_pc))
			return (SIMU_FAULT);

		/* check immediate bit and use immediate field or reg (rs2) */
		if ((inst >> 13) & 1) {
			register int imm;
			imm  = inst & 0x1fff;	/* mask out immediate field */
			imm <<= 19;		/* sign extend it */
			imm >>= 19;
			rs2 = imm;		/* compute address */
		} else
			if (getreg(rgs, rw, (inst & 0x1f), &rs2, badaddr,
			    rp->r_pc))
				return (SIMU_FAULT);

		switch ((inst & 0x01f80000) >> 19) {
		case 0xa:
			rv = _ip_umul(rs1, rs2, &dest, &rp->r_y, &rp->r_psr);
			break;
		case 0xb:
			rv = _ip_mul(rs1, rs2, &dest, &rp->r_y, &rp->r_psr);
			break;
		case 0xe:
			rv = _ip_udiv(rs1, rs2, &dest, &rp->r_y, &rp->r_psr);
			break;
		case 0xf:
			rv = _ip_div(rs1, rs2, &dest, &rp->r_y, &rp->r_psr);
			break;
		case 0x1a:
			rv = _ip_umulcc(rs1, rs2, &dest, &rp->r_y, &rp->r_psr);
			break;
		case 0x1b:
			rv = _ip_mulcc(rs1, rs2, &dest, &rp->r_y, &rp->r_psr);
			break;
		case 0x1e:
			rv = _ip_udivcc(rs1, rs2, &dest, &rp->r_y, &rp->r_psr);
			break;
		case 0x1f:
			rv = _ip_divcc(rs1, rs2, &dest, &rp->r_y, &rp->r_psr);
			break;
		default:
			return (SIMU_ILLEGAL);
		}
		if (rv != SIMU_SUCCESS)
			return (rv);
		if (putreg(dest, rgs, rw, rd, badaddr, rp->r_pc))
			return (SIMU_FAULT);

		/*
		 * If it was a multiply, the routine in crt.s already moved
		 * the high result into rp->r_y
		 */
		return (SIMU_SUCCESS);
	}
#endif	/* __sparcv9cpu */

	/*
	 * Otherwise, we can't simulate instruction, its illegal.
	 */
	return (SIMU_ILLEGAL);
}

int
do_swap(rp, inst, badaddr)
	register struct regs	*rp;
	register u_int		inst;
	caddr_t			*badaddr;
{
	register u_int rd, rs1, rs2;
	register u_int *rgs, *rw;
	register int immflg, s;
	u_int src, srcaddr, srcval;
	u_int dstval;
	proc_t *p = curproc;
	int mapped = 0;

	/*
	 * Check for the unimplemented swap instruction.
	 *
	 * Note:  there used to be support for simulating swap
	 * instructions used by the kernel.  The kernel doesn't
	 * use swap, and shouldn't use it unless it's in hardware.
	 * If the kernel needs swap and it isn't implemented, it'll
	 * be extremely difficult to simulate.
	 */
	if ((u_int)rp->r_pc > KERNELBASE)
		panic("kernel use of swap instruction!");

	rgs = (u_int *)&rp->r_y;		/* globals and outs */
	rw = (u_int *)rp->r_sp;			/* ins and locals */

	/*
	 * Calculate address, get first src register.
	 */
	rs1 = (inst >> 14) & 0x1f;
	if (getreg(rgs, rw, rs1, &src, badaddr, KERNELBASE - 1))
		return (SIMU_FAULT);
	srcaddr = src;

	/*
	 * Check immediate bit and use immediate field or rs2
	 * to get the second operand to build source address.
	 */
	immflg = (inst >> 13) & 1;
	if (immflg) {
		s  = inst & 0x1fff;		/* mask out immediate field */
		s <<= 19;			/* sign extend it */
		s >>= 19;
		srcaddr += s;			/* compute address */
	} else {
		rs2 =  inst & 0x1f;
		if (getreg(rgs, rw, rs2, &src, badaddr, KERNELBASE - 1))
			return (SIMU_FAULT);
		srcaddr += src;
	}

	/*
	 * Check for unaligned address.
	 */
	if ((srcaddr&3) != 0) {
		*badaddr = (caddr_t)srcaddr;
		return (SIMU_UNALIGN);
	}

	if (p->p_warea)		/* watchpoints in effect */
		mapped = pr_mappage((caddr_t)srcaddr, sizeof (int), S_WRITE, 1);

	/*
	 * Raise priority (atomic swap operation).
	 */
	s = splhigh();

	/*
	 * Read memory at source address.
	 */
	if ((srcval = _fuword((int *)srcaddr)) == -1) {
		if (_fubyte((caddr_t)srcaddr) == -1) {
			*badaddr = (caddr_t)srcaddr;
			goto badret;
		}
	}

	/*
	 * Get value from destination register.
	 */
	rd =  (inst >> 25) & 0x1f;
	if (getreg(rgs, rw, rd, &dstval, badaddr, KERNELBASE - 1))
		goto badret;


	/*
	 * Write src address with value from destination register.
	 */
	if (_suword((int *)srcaddr, dstval)) {
		*badaddr = (caddr_t)srcaddr;
		goto badret;
	}

	/*
	 * Update destination reg with value from memory.
	 */
	if (putreg(srcval, rgs, rw, rd, badaddr, KERNELBASE -1))
		goto badret;

	/*
	 * Restore priority and return success or failure.
	 */
	(void) splx(s);
	if (mapped)
		pr_unmappage((caddr_t)srcaddr, sizeof (int), S_WRITE, 1);
	return (SIMU_SUCCESS);
badret:
	(void) splx(s);
	if (mapped)
		pr_unmappage((caddr_t)srcaddr, sizeof (int), S_WRITE, 1);
	return (SIMU_FAULT);
}

/*
 * Get the value of a register for instruction simulation
 * by using the regs or window structure pointers.
 * Return 0 for success -1 failure.  If there is a failure,
 * save the faulting address using badaddr pointer.
 */
static int
getreg(rgs, rw, reg, val, badaddr, srcaddr)
	u_int	*rgs, *rw, reg;
	u_int 	*val;
	caddr_t	*badaddr;
	u_int srcaddr;
{
	int rv = 0;

	if (reg == 0)
		*val = 0;
	else if (reg < 16)
		*val = rgs[reg];
	else if (srcaddr >= KERNELBASE)
		*val = rw[reg - 16];
	else {
		proc_t *p = curproc;
		caddr_t addr = (caddr_t)&rw[reg - 16];
		int mapped = 0;

		if (p->p_warea)		/* watchpoints in effect */
			mapped = pr_mappage(addr, sizeof (int), S_READ, 1);
		if ((*val = _fuword((int *)addr)) == -1) {
			if (_fubyte(addr) == -1) {
				*badaddr = addr;
				rv = -1;
			}
		}
		if (mapped)
			pr_unmappage(addr, sizeof (int), S_READ, 1);
	}
	return (rv);
}

/*
 * Set the value of a register after instruction simulation
 * by using the regs or window structure pointers.
 * Return 0 for success -1 failure.
 * save the faulting address using badaddr pointer.
 */
static int
putreg(data, rgs, rw, reg, badaddr, srcaddr)
	u_int	data, *rgs, *rw, reg;
	caddr_t	*badaddr;
	u_int srcaddr;
{
	int rv = 0;

	if (reg == 0)
		return (0);
	if (reg < 16)
		rgs[reg] = data;
	else if (srcaddr >= KERNELBASE)
		rw[reg - 16] = data;
	else {
		struct machpcb *mpcb = lwptompcb(curthread->t_lwp);
		proc_t *p = curproc;
		caddr_t addr = (caddr_t)&rw[reg - 16];
		int mapped = 0;

		if (p->p_warea)		/* watchpoints in effect */
			mapped = pr_mappage(addr, sizeof (int), S_WRITE, 1);
		if (_suword((int *)addr, (int)data) != 0) {
			*badaddr = addr;
			rv = -1;
		}
		if (mapped)
			pr_unmappage(addr, sizeof (int), S_WRITE, 1);
		/*
		 * We have changed a local or in register;
		 * nuke the watchpoint return windows.
		 */
		mpcb->mpcb_rsp[0] = NULL;
		mpcb->mpcb_rsp[1] = NULL;
	}
	return (rv);
}

/*
 * Calculate a memory reference address from instruction
 * operands, used to return the address of a fault, instead
 * of the instruction when error occur.  This is code is
 * common with most of the routines that simulate instructions.
 */
int
calc_memaddr(rp, badaddr)
	struct regs	*rp;
	caddr_t		*badaddr;
{
	proc_t *p = curproc;
	int mapped = 0;
	u_int	inst;
	u_int	rs1, rs2;
	u_int	*rgs;
	u_int	*rw;
	int	sz;
	int	immflg;
	int 	addr;
	u_int	val;

	ASSERT(USERMODE(rp->r_psr));
	/* get the instruction */
	addr = rp->r_pc;
	if (p->p_warea)		/* watchpoints in effect */
		mapped = pr_mappage((caddr_t)addr, sizeof (int), S_READ, 1);
	inst = _fuiword((int *)addr);
	if (mapped)
		pr_unmappage((caddr_t)addr, sizeof (int), S_READ, 1);

	rs1 = (inst >> 14) & 0x1f;
	rs2 = inst & 0x1f;
	immflg = (inst >> 13) & 1;

	switch ((inst >> 19) & 3) {	/* map size bits to a number */
	case 0: sz = 4; break;
	case 1: return (0);
	case 2: sz = 2; break;
	case 3: sz = 8; break;
	}
	(void) flush_user_windows_to_stack(NULL); /* flush windows to memory */
	rgs = (u_int *)&rp->r_y;		/* globals and outs */
	rw = (u_int *)rp->r_sp;			/* ins and locals */

	if (getreg(rgs, rw, rs1, &val, badaddr, KERNELBASE - 1))
		return (SIMU_FAULT);
	addr = val;

	/* check immediate bit and use immediate field or reg (rs2) */
	if (immflg) {
		register int imm;
		imm  = inst & 0x1fff;		/* mask out immediate field */
		imm <<= 19;			/* sign extend it */
		imm >>= 19;
		addr += imm;			/* compute address */
	} else {
		if (getreg(rgs, rw, rs2, &val, badaddr, KERNELBASE - 1))
			return (SIMU_FAULT);
		addr += val;
	}

	*badaddr = (caddr_t)addr;
	return (addr & (sz - 1) ? SIMU_UNALIGN : SIMU_SUCCESS);
}

/*
 * Return the size of a load or store instruction (1, 2, 4, 8).
 * Return 0 on failure (not a load or store instruction).
 */
/* ARGSUSED */
int
instr_size(rp, addrp, rw)
	register struct regs *rp;
	caddr_t *addrp;
	enum seg_rw rw;
{
	proc_t	*p = curproc;
	caddr_t	addr = (caddr_t)rp->r_pc;
	int	mapped = 0;
	u_int	inst;
	int	sz;
	int	immflg;

	ASSERT(USERMODE(rp->r_psr));

	if (rw == S_EXEC)
		return (4);

	/* get the instruction */
	if (p->p_warea)		/* watchpoints in effect */
		mapped = pr_mappage(addr, sizeof (int), S_READ, 1);
	inst = _fuiword((int *)addr);
	if (mapped)
		pr_unmappage(addr, sizeof (int), S_READ, 1);

	immflg = (inst >> 13) & 1;

	switch ((inst >> 19) & 3) {	/* map size bits to a number */
	case 0: sz = 4; break;
	case 1: sz = 1; break;
	case 2: sz = 2; break;
	case 3: sz = 8; break;
	}

	/* if not load or store, or to alternate space, return 0 */
	if (((inst >> 30) != 3) ||
	    (immflg == 0 && ((inst >> 5) & 0xff)))
		sz = 0;

	return (sz);
}

/*
 * Check for an atomic (LDST, SWAP) instruction
 * returns 1 if yes, 0 if no.
 */
int
is_atomic(rp)
	struct regs	*rp;
{
	u_int inst;

	if (!USERMODE(rp->r_psr))
		inst = *(u_int *)rp->r_pc;
	else {
		proc_t *p = curproc;
		caddr_t addr = (caddr_t)rp->r_pc;
		int mapped = 0;

		if (p->p_warea)		/* watchpoints in effect */
			mapped = pr_mappage(addr, sizeof (int), S_READ, 1);
		inst = _fuiword((int *)addr);
		if (mapped)
			pr_unmappage(addr, sizeof (int), S_READ, 1);
	}
	/* True for LDSTUB(A) and SWAP(A) */
	return ((inst & 0xc1680000) == 0xc0680000);
}
