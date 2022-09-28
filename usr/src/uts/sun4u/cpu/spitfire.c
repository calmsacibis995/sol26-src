/*
 * Copyright (c) 1993-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)spitfire.c	1.32	96/10/07 SMI"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/machparam.h>
#include <sys/machsystm.h>
#include <sys/cpu.h>
#include <sys/elf_SPARC.h>
#include <vm/hat_sfmmu.h>
#include <sys/cpuvar.h>
#include <sys/spitregs.h>
#include <sys/async.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>

/*
 * Support for spitfire modules
 */

extern int use_page_coloring;
extern int do_pg_coloring;
extern int use_virtual_coloring;
extern int do_virtual_coloring;

extern void flush_ecache(u_longlong_t physaddr, u_int size);
extern void enable_internal_caches(void);
extern u_int set_error_enable_tl1(volatile u_longlong_t *neer);

void cpu_ce_error(struct regs *rp, u_int p_afsr1, u_int p_afar1,
	u_int dp_err, u_int p_afar0);
void cpu_async_error(struct regs *rp, u_int p_afsr1, u_int p_afar1,
	u_int dp_err, u_int p_afar0);
static int cpu_log_ce_err(struct ecc_flt *ecc, char *unum);
static int cpu_log_err(u_longlong_t *p_afsr, u_longlong_t *p_afar,
	u_short inst);
static int cpu_log_bto_err(struct regs *rp, u_longlong_t *afsr,
	u_longlong_t *afar, u_short inst);
static int cpu_log_ue_err(struct ecc_flt *ecc, char *unum);

/*
 * Maximum number of contexts for Spitfire.
 */
#define	MAX_NCTXS	(1 << 13)

/*
 * Useful for hardware debugging.
 */
int	async_err_panic = 0;

void
cpu_setup(void)
{
	extern u_int nctxs;
	extern int at_flags;

	cache |= (CACHE_VAC | CACHE_PTAG | CACHE_IOCOHERENT);

	at_flags = EF_SPARC_32PLUS | EF_SPARC_SUN_US1;

	/*
	 * Use the maximum number of contexts available for Spitfire.
	 */
	nctxs = MAX_NCTXS;

	if (use_page_coloring) {
		do_pg_coloring = 1;
		if (use_virtual_coloring)
			do_virtual_coloring = 1;
	}

	isa_list = "sparcv8plus+vis sparvc8plus "
		"sparcv8 sparcv8-fsmuld sparcv7 sparc";
}

void
fini_mondo(void)
{
}

void
syncfpu(void)
{
}

/*
 * correctable ecc errors from the cpu
 * As per machcpuvar.h note: "The mid is the same as the cpu id.
 * We might want to change this later"
 */
/* ARGSUSED */
void
cpu_ce_error(struct regs *rp, u_int p_afsr1, u_int p_afar1,
	u_int dp_err, u_int p_afar0)
{
	u_short sdbh, sdbl;
	u_char e_syndh, e_syndl;
	u_char size = 3;	/* 8 byte alignment */
	u_short id = (u_short) getprocessorid();
	u_short inst = (u_short) CPU->cpu_id;
	union ul {
		u_longlong_t	afsr;
		u_longlong_t	afar;
		u_int		i[2];
	} j, k;

	j.i[0] = (dp_err & 0x1);
	j.i[1] = p_afsr1;
	k.i[0] = p_afar0;
	k.i[1] = p_afar1 & ~0xf;
	sdbh = (u_short) ((dp_err >> 1) & 0x3FF);
	sdbl = (u_short) ((dp_err >> 11) & 0x3FF);
	e_syndh = (u_char) (sdbh & (u_int)P_DER_E_SYND);
	e_syndl = (u_char) (sdbl & (u_int)P_DER_E_SYND);

	if ((sdbl >> 8) & 1) {
		k.i[1] |= 0x8;		/* set bit 3 if error in sdbl */
		(void) ce_error(&j.afsr, &k.afar, e_syndl, size, 0, id, inst,
				(afunc)cpu_log_ce_err);
	}
	if ((sdbh >> 8) & 1) {
		(void) ce_error(&j.afsr, &k.afar, e_syndh, size, 0, id, inst,
				(afunc)cpu_log_ce_err);
	}
	if ((((sdbl >> 8) & 1) == 0) && (((sdbh >> 8) & 1) == 0)) {
		/* ECC error with no SDB info */
		(void) ce_error(&j.afsr, &k.afar, 0, size, 0, id, inst,
				(afunc)cpu_log_ce_err);
	}
}

/*
 * As per machcpuvar.h note: "The mid is the same as the cpu id.
 * We might want to change this later"
 */
static int
cpu_log_ce_err(struct ecc_flt *ecc, char *unum)
{
	union ul {
		u_longlong_t	afsr;
		u_longlong_t	afar;
		u_int		i[2];
	} j, k;
	extern int ce_verbose;

	j.afsr = ecc->flt_stat;
	k.afar = ecc->flt_addr;

		/* overwrite policy - this message should never occur! */
	if (j.afsr & P_AFSR_UE) {
		cmn_err(CE_CONT, "CPU%d CE Error: AFSR 0x%08x %08x "
			"AFAR 0x%08x %08x Overwritten by Harderror\n",
			ecc->flt_inst, j.i[0], j.i[1], k.i[0], k.i[1]);
		return (1);
	}

	if (ecc->flt_synd == 0)
		cmn_err(CE_CONT,
			"CE Error: CPU%d ECC Error With No SDB Info\n",
				ecc->flt_inst);

	if ((ce_verbose) || (ecc->flt_synd == 0)) {
		cmn_err(CE_CONT,
		"CPU%d CE Error: AFSR 0x%08x %08x, AFAR 0x%08x %08x, SIMM %s\n",
			ecc->flt_inst, j.i[0], j.i[1], k.i[0], k.i[1], unum);
		cmn_err(CE_CONT,
			"\tSyndrome 0x%x, Size %d, Offset %d UPA MID %d\n",
			ecc->flt_synd, ecc->flt_offset,
			ecc->flt_size, ecc->flt_upa_id);
	}
	return (1);		/* always memory related */
}

#ifdef DEBUG
int test_mp_cp = 0;
#endif
/*
 * As per machcpuvar.h note: "The mid is the same as the cpu id.
 * We might want to change this later"
 */
u_int
cpu_get_status(struct ecc_flt *ecc)
{
	volatile u_longlong_t	afsr;
	volatile u_longlong_t	afar;
	u_short id = (u_short) getprocessorid();
	u_short inst = (u_short) CPU->cpu_id;

	/* Get afsr to later check for fatal cp bit.  */
	get_asyncflt(&afsr);
	get_asyncaddr(&afar);
#ifdef DEBUG
	if (test_mp_cp)
		afsr |= P_AFSR_CP;
#endif

	ecc->flt_stat = afsr;
	ecc->flt_addr = afar;
	ecc->flt_inst = inst;
	ecc->flt_upa_id = id;

	return (0);
}

/*
 * Access error trap handler for asynchronous cpu errors.
 * This routine is called to handle a data or instruction access error.
 * All fatal failures are completely handled by this routine
 * (by panicing).  Since handling non-fatal failures would access
 * data structures which are not consistent at the time of this
 * interrupt, these non-fatal failures are handled later in a
 * soft interrupt at a lower level.
 *
 * As per machcpuvar.h note: "The mid is the same as the cpu id.
 * We might want to change this later"
 */
/* ARGSUSED */
void
cpu_async_error(struct regs *rp, u_int p_afsr1, u_int p_afar1,
	u_int dp_err, u_int p_afar0)
{
	u_short sdbh, sdbl;
	u_char e_syndh, e_syndl;
	u_short id = (u_short) getprocessorid();
	u_short inst = (u_short) CPU->cpu_id;
	u_int fatal = 0;
	volatile u_longlong_t neer;
	union ul {
		u_longlong_t	afsr;
		u_longlong_t	afar;
		u_int		i[2];
	} j, k;
	volatile greg_t pil;
	int do_longjmp = 0;
	extern greg_t getpil();
	extern void scrub_ecc(void);

	j.i[0] = (dp_err & 0x1);
	j.i[1] = p_afsr1;
	k.i[0] = p_afar0;
	k.i[1] = p_afar1 & ~0xf;
	sdbh = (u_short) ((dp_err >> 1) & 0x3FF);
	sdbl = (u_short) ((dp_err >> 11) & 0x3FF);
	e_syndh = (u_char) (sdbh & (u_int)P_DER_E_SYND);
	e_syndl = (u_char) (sdbl & (u_int)P_DER_E_SYND);

	pil = getpil();
	ASSERT(pil <= PIL_MAX);
	if (async_err_panic) {
		cmn_err(CE_PANIC, "CPU%d Async Err: AFSR 0x%08x %08x "
			"AFAR 0x%08x %08x SDBH 0x%x SDBL 0x%x PIL %d",
			inst, j.i[0], j.i[1], k.i[0], k.i[1], sdbh, sdbl, pil);
	}
	/*
	 * Log the error, check for all miscellaneous fatal errors.
	 */
	fatal = cpu_log_err(&j.afsr, &k.afar, inst);

	/*
	 * UE error is always fatal even if priv bit is not set.
	 * Tip from kbn: if ME and 2 sdb syndromes, then 2 different addresses
	 * else if !ME and 2 sdb syndromes, then same address.
	 */
	if (j.afsr & P_AFSR_UE) {
		u_char size = 3;	/* 8 byte alignment */

		if ((sdbl >> 9) & 1) {
			k.i[1] |= 0x8;	/* set bit 3 if error in sdbl */
			(void) ue_error(&j.afsr, &k.afar, e_syndl, size,
					0, id, inst, (afunc)cpu_log_ue_err);
		}
		if ((sdbh >> 9) & 1) {
			(void) ue_error(&j.afsr, &k.afar, e_syndh, size,
					0, id, inst, (afunc)cpu_log_ue_err);
		}
		if ((((sdbl >> 9) & 1) == 0) && (((sdbh >> 9) & 1) == 0)) {
			(void) ue_error(&j.afsr, &k.afar, 0, size,
					0, id, inst, (afunc)cpu_log_ue_err);
		}
		fatal = 1;
	}

	if ((j.afsr & P_AFSR_TO) || (j.afsr & P_AFSR_BERR)) {
		if (curthread->t_lofault) {
			rp->r_g1 = FC_HWERR;
			rp->r_pc = curthread->t_lofault;
			rp->r_npc = curthread->t_lofault + 4;
		} else if (!(curthread->t_nofault)) {
			fatal = cpu_log_bto_err(rp, &j.afsr, &k.afar, inst);
		} else {
			do_longjmp = 1;
		}
	}

	if (!(fatal)) {
		/*
		 * reenable errors, flush cache
		 */
		scrub_ecc();
		neer = (EER_ISAPEN | EER_NCEEN | EER_CEEN);
		xt_one((int)inst, (u_int)&set_error_enable_tl1, (u_int)&neer,
			(u_int)0, (u_int)0, (u_int)0);

		if (do_longjmp) {
			longjmp(curthread->t_nofault);
		}
	}
}

static int
cpu_log_err(u_longlong_t *p_afsr, u_longlong_t *p_afar, u_short inst)
{
	union ul {
		u_longlong_t	afsr;
		u_longlong_t	afar;
		u_int		i[2];
	} j, k;

	j.afsr = *p_afsr;
	k.afar = *p_afar;

	/*
	 * The ISAP and ETP errors are supposed to cause a POR
	 * from the system, so in theory we never, ever see these messages.
	 */
	if (j.afsr & P_AFSR_ISAP) {
		cmn_err(CE_PANIC, "CPU%d System Address Parity Error: "
			"AFSR 0x%08x %08x AFAR 0x%08x %08x",
			inst, j.i[0], j.i[1], k.i[0], k.i[1]);
	}
	if (j.afsr & P_AFSR_ETP) {
		cmn_err(CE_PANIC, "CPU%d Ecache Tag Parity Error: "
			"AFSR 0x%08x %08x AFAR 0x%08x %08x",
			inst, j.i[0], j.i[1], k.i[0], k.i[1]);
	}
	/*
	 * IVUE, LDP, WP, and EDP are fatal because we have no address.
	 * So even if we kill the curthread, we can't be sure that we have
	 * killed everyone using tha data, and it could be updated incorrectly
	 * because we have a writeback cache.
	 */
	if (j.afsr & P_AFSR_IVUE) {
		cmn_err(CE_PANIC, "CPU%d Interrupt Vector Uncorrectable Error: "
			"AFSR 0x%08x %08x AFAR 0x%08x %08x",
			inst, j.i[0], j.i[1], k.i[0], k.i[1]);
	}
	if (j.afsr & P_AFSR_LDP) {
		cmn_err(CE_PANIC, "CPU%d Load Data Parity Error: "
			"AFSR 0x%08x %08x AFAR 0x%08x %08x",
			inst, j.i[0], j.i[1], k.i[0], k.i[1]);
	}
	if (j.afsr & P_AFSR_WP) {
		cmn_err(CE_PANIC, "CPU%d Writeback Data Parity Error: "
			"AFSR 0x%08x %08x AFAR 0x%08x %08x",
			inst, j.i[0], j.i[1], k.i[0], k.i[1]);
	}
	if (j.afsr & P_AFSR_EDP) {
		cmn_err(CE_PANIC, "CPU%d Ecache SRAM Data Parity Error: "
			"AFSR 0x%08x %08x AFAR 0x%08x %08x",
			inst, j.i[0], j.i[1], k.i[0], k.i[1]);
	}
	/*
	 * CP bit indicates a fatal error.
	 */
	if (j.afsr & P_AFSR_CP) {
		cmn_err(CE_PANIC, "CPU%d Copyout Data Parity Error: "
			"AFSR 0x%08x %08x AFAR 0x%08x %08x",
			inst, j.i[0], j.i[1], k.i[0], k.i[1]);
	}
	return (0);
}

static int
cpu_log_bto_err(struct regs *rp, u_longlong_t *afsr,
		u_longlong_t *afar, u_short inst)
{
	int priv = 0, mult = 0;
	union ul {
		u_longlong_t	afsr;
		u_longlong_t	afar;
		u_int		i[2];
	} j, k;
	proc_t *up;

	j.afsr = *afsr;
	k.afar = *afar;

	/* if (bto->flt_stat & P_AFSR_ME) */
	if (j.i[0] & 1)
		mult = 1;
	if (j.afsr & P_AFSR_PRIV)
		priv = 1;
	/*
	 * Timeout - quiet about t_nofault timeout
	 */
	if (j.afsr & P_AFSR_TO) {
		if ((mult) && (priv)) {
			cmn_err(CE_PANIC, "CPU%d Mult. Priv. Timeout Error: "
				"AFSR 0x%08x %08x AFAR 0x%08x %08x",
				inst, j.i[0], j.i[1], k.i[0], k.i[1]);
		} else if (priv) {
			cmn_err(CE_PANIC, "CPU%d Privileged Timeout Error: "
				"AFSR 0x%08x %08x AFAR 0x%08x %08x",
				inst, j.i[0], j.i[1], k.i[0], k.i[1]);
		} else if (mult) {
			cmn_err(CE_PANIC,
				"CPU%d Timeout Error with Mult. Errors: "
				"AFSR 0x%08x %08x AFAR 0x%08x %08x",
				inst, j.i[0], j.i[1], k.i[0], k.i[1]);
		} else {
			up = ttoproc(curthread);
			bto_error(inst, up, rp->r_pc);
		}
	}
	/*
	 * Bus error
	 */
	if (j.afsr & P_AFSR_BERR) {
		if ((mult) && (priv)) {
			cmn_err(CE_PANIC, "CPU%d Privileged Mult. Bus Error: "
				"AFSR 0x%08x %08x AFAR 0x%08x %08x",
				inst, j.i[0], j.i[1], k.i[0], k.i[1]);
		} else if (priv) {
			cmn_err(CE_PANIC, "CPU%d Privileged Bus Error: "
				"AFSR 0x%08x %08x AFAR 0x%08x %08x",
				inst, j.i[0], j.i[1], k.i[0], k.i[1]);
		} else if (mult) {
			cmn_err(CE_PANIC, "CPU%d Bus Error with Mult. Errors: "
				"AFSR 0x%08x %08x AFAR 0x%08x %08x",
				inst, j.i[0], j.i[1], k.i[0], k.i[1]);
		} else {
			up = ttoproc(curthread);
			bto_error(inst, up, rp->r_pc);
		}
	}
	return (0);
}

static int
cpu_log_ue_err(struct ecc_flt *ecc, char *unum)
{
	u_short inst = ecc->flt_inst;
	int priv = 0, mult = 0;
	union ul {
		u_longlong_t	afsr;
		u_longlong_t	afar;
		u_int		i[2];
	} j, k;

	j.afsr = ecc->flt_stat;
	k.afar = ecc->flt_addr;

	/* if (ecc->flt_stat & P_AFSR_ME) */
	if (j.i[0] & 1)
		mult = 1;
	if (j.afsr & P_AFSR_PRIV)
		priv = 1;

	if ((mult) && (priv)) {
		cmn_err(CE_PANIC, "CPU%d Multiple Privileged UE Error: "
			"AFSR 0x%08x %08x AFAR 0x%08x %08x SIMM %s",
			inst, j.i[0], j.i[1], k.i[0], k.i[1], unum);
	} else if (mult) {
		cmn_err(CE_PANIC, "CPU%d Multiple UE Errors: "
			"AFSR 0x%08x %08x AFAR 0x%08x %08x SIMM %s",
			inst, j.i[0], j.i[1], k.i[0], k.i[1], unum);
	} else if (priv) {
		cmn_err(CE_PANIC, "CPU%d Privileged UE Error: "
			"AFSR 0x%08x %08x AFAR 0x%08x %08x SIMM %s",
			inst, j.i[0], j.i[1], k.i[0], k.i[1], unum);
	} else {
		cmn_err(CE_PANIC, "CPU%d User UE Error: "
			"AFSR 0x%08x %08x AFAR 0x%08x %08x SIMM %s",
			inst, j.i[0], j.i[1], k.i[0], k.i[1], unum);
	}
	return (UE_FATAL);
}

/*
 * Flush the entire ecache using displacement flush by reading through a
 * physical address range as large as the ecache.
 */
void
cpu_flush_ecache(void)
{
	u_int size;

	size = cpunodes[CPU->cpu_id].ecache_size;
	flush_ecache(ecache_flushaddr, size);
}

void
scrub_ecc(void)
{
	u_int size;

	/*
	 * Flush the entire ecache using displacement flush by reading
	 * through a physical address range twice as large as the ecache.
	 * We select twice the size just in case ecc error were to fall
	 * in this range.
	 */
	size = cpunodes[CPU->cpu_id].ecache_size * 2;

	/*
	 * If !WP && !IVUE, need to flush E-cache here, then need to
	 * re-enable I and D caches as per Spitfire manual 9.1.2.
	 */
	flush_ecache(ecache_flushaddr, size);
	enable_internal_caches();
}
