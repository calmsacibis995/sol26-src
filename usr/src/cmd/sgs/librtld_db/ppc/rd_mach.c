/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)rd_mach.c	1.4	96/09/09 SMI"

#include	<proc_service.h>
#include	<link.h>
#include	<rtld_db.h>
#include	"_rtld_db.h"


struct op_typ {
	unsigned Rc : 1;	/* record bit */
	unsigned XO : 10;	/* extended opcode */
	unsigned rB : 5;	/* second register operand */
	unsigned rA : 5;	/* first register operand */
	unsigned rD : 5;	/* destination register */
	unsigned op : 6;	/* primary opcode */
};
struct op_br {
	unsigned LK : 1;	/* link bit */
	unsigned AA : 1;	/* absolute address bit */
	unsigned LI : 24;	/* op_immediate 2's complement integer */
	unsigned op : 6;	/* primary opcode */
};
struct op_brc {
	unsigned int LK : 1;	/* link bit */
	unsigned int AA : 1;	/* absolute address bit */
	signed int BD   : 14;	/* branch displacement */
	unsigned int BI : 5;	/* branch condition */
	unsigned int BO : 5;	/* branch instruction options */
	unsigned int op : 6;	/* primary opcode */
};
struct op_bool {
	unsigned Rc : 1;	/* Record Bit/Link Bit */
	unsigned XO : 10;	/* extended opcode */
	unsigned crbB : 5;	/* condition register bit B */
	unsigned crbA : 5;	/* condition register bit A */
	unsigned crbD : 5;	/* destination CR bit D */
	unsigned op : 6;	/* primary opcode */
};
struct op_imm {
	unsigned D   : 16;	/* 16-bit signed 2's complement integer */
				/* sign-extend to 32 bits - op_immed operand */
	unsigned rA : 5;	/* first register operand */
	unsigned rD : 5;	/* target register */
	unsigned op : 6;	/* primary opcode */
};
union op {
	struct op_typ	op_typ;
	struct op_brc	op_brc;
	struct op_br	op_br;
	struct op_bool	op_bool;
	struct op_imm	op_imm;
	unsigned long	l;
};

#define	ADDI_OP			14		/* addi */
#define	PPC_OP_16		16		/* bcx */
#define	PPC_OP_18		18		/* bx */
#define	PPC_OP_19		19		/* op 19 */
#define	PPC_OP19_16		16		/* bclrx */
#define	PPC_OP19_528		528		/* bcctrx */
#define	LR_BIT			1


static int
is_li_instr(unsigned int ins)
{
	union op inst;
	unsigned short op1;
	inst.l = ins;
	op1 = (unsigned short) inst.op_typ.op;
	return ((inst.op_imm.rA == 0) && (op1 & ADDI_OP) != 0);
}

static int
is_br_instr(unsigned int ins)
{
	union op inst;
	unsigned short op1;
	inst.l = ins;
	op1 = (unsigned short) inst.op_br.op;

	return ((op1 == PPC_OP_18) || (op1 == PPC_OP_16) ||
		((op1 ==  PPC_OP_19) && ((inst.op_bool.XO == PPC_OP19_16) ||
		(inst.op_bool.XO == PPC_OP19_528))));
}

static int
is_br_to_plt(unsigned int ins, unsigned int pc, psaddr_t plt_base)
{
	union op inst;
	unsigned short op1;
	int dest;
	inst.l = ins;
	op1 = (unsigned short) inst.op_br.op;

	if (op1 == PPC_OP_18) {			/* bX */
		dest = inst.op_br.LI << 8;
		dest >>= 6;
		if (!inst.op_brc.AA)	/* AA = 0 means compute relative */
			dest += pc;
		if (dest == plt_base)
			return (1);
	}
	if (op1 == PPC_OP_16) {			/* bcX */
		dest = inst.op_brc.BD << 2;
		if (!inst.op_brc.AA)	/* AA = 0 means compute relative */
			dest += pc;
		if (dest == plt_base)
			return (1);
	}

	/* bclrx or bcctrx */
	if ((op1 ==  PPC_OP_19) && ((inst.op_bool.XO == PPC_OP19_16) ||
	    (inst.op_bool.XO == PPC_OP19_528))) {
		/*
		 * Target addr is in count register
		 * Hard to deal with. Should not happen inside a PLT. For
		 * now return false  [VT]
		 */
		return (0);
	}
	return (0);
}


static int
is_first_time(const struct ps_prochandle *p, psaddr_t pc, psaddr_t plt_base)
{
	unsigned ins1, ins2;

	(void) ps_pread(p, pc, (char *)&ins1, sizeof (unsigned));
	(void) ps_pread(p, pc + 4, (char *)&ins2, sizeof (unsigned));
	return (is_li_instr(ins1) && is_br_to_plt(ins2, pc+4, plt_base));
}


static int
is_nth_time(const struct ps_prochandle *p, psaddr_t pc, psaddr_t plt_base)
{
	unsigned ins1, ins2;

	(void) ps_pread(p, pc, (char *)&ins1, sizeof (unsigned));
	(void) ps_pread(p, pc + 4, (char *)&ins2, sizeof (unsigned));
	return (is_br_instr(ins1) || (!is_br_to_plt(ins2, pc + 4, plt_base)));
}



/* ARGSUSED 2 */
rd_err_e
rd_plt_resolution(rd_agent_t * rap, psaddr_t pc, lwpid_t lwpid,
	psaddr_t plt_base, rd_plt_info_t * rpi)
{
	RDAGLOCK(rap);
	if (is_first_time(rap->rd_psp, pc, plt_base)) {
		rpi->pi_skip_method = RD_RESOLVE_TARGET_STEP;
		rpi->pi_nstep = 1;
		RDAGUNLOCK(rap);
		return (rd_binder_exit_addr(rap, &rpi->pi_target));
	}

	if (is_nth_time(rap->rd_psp, pc, plt_base)) {
		rpi->pi_skip_method = RD_RESOLVE_STEP;
		/*
		 * This is only true if we have a 'short' plt.
		 * We need a little more logic here...
		 */
		rpi->pi_nstep = 1;
		RDAGUNLOCK(rap);
		return (RD_OK);
	}

	rpi->pi_skip_method = RD_RESOLVE_NONE;
	rpi->pi_nstep = 0;
	rpi->pi_target = 0;

	RDAGUNLOCK(rap);
	return (RD_OK);
}
