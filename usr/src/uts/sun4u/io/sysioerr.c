/*
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)sysioerr.c 1.34	96/10/17 SMI"

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/cmn_err.h>
#include <sys/ivintr.h>
#include <sys/async.h>
#include <sys/sysiosbus.h>
#include <sys/sysioerr.h>
#include <sys/x_call.h>
#include <sys/machsystm.h>

/*
 * Set the following variable in /etc/system to tell the kernel
 * not to shutdown the machine if the temperature reaches
 * the Thermal Warning limit.
 */
int oven_test = 0;

/*
 * To indicate if the prom has the property of "thermal-interrupt".
 */
static int thermal_interrupt_enabled = 0;

/*
 * Sbus nexus callback routine for interrupt distribution.
 */
extern void sbus_intrdist(void *, int, u_int);

/* Global lock which protects the interrupt distribution lists */
extern kmutex_t intr_dist_lock;

/*
 * adb debug_sysio_errs to 1 if you don't want your system to panic on
 * sbus ue errors. adb sysio_err_flag to 0 if you don't want your system
 * to check for sysio errors at all.
 */
int sysio_err_flag = 1;
u_int debug_sysio_errs = 0;

/*
 * bto_cnt = number of bus errors and timeouts allowed within bto_secs
 * use /etc/system to change the bto_cnt to a very large number if
 * it's a problem!
 */
int bto_secs = 10;
int bto_cnt = 10;

static u_int
sysio_ue_intr(struct sbus_soft_state *softsp);

static u_int
sysio_ce_intr(struct sbus_soft_state *softsp);

static u_int
sbus_err_intr(struct sbus_soft_state *softsp);

static int
sysio_log_ce_err(struct ecc_flt *ecc, char *unum);

static int
sysio_log_ue_err(struct ecc_flt *ecc, char *unum);

static void
sbus_clear_intr(struct sbus_soft_state *softsp, u_ll_t *pafsr);

static void
sbus_log_error(struct sbus_soft_state *softsp, u_ll_t *pafsr, u_ll_t *pafar,
		u_short id, u_short inst, int cleared);

static int
sbus_check_bto(struct sbus_soft_state *softsp);

static void
sbus_log_csr_error(u_ll_t *psb_csr, u_short id, u_short inst);

static u_int
sbus_ctrl_ecc_err(struct sbus_soft_state *softsp);

static u_int
sysio_dis_err(struct sbus_soft_state *softsp);

static u_int
sysio_init_err(struct sbus_soft_state *softsp);

static u_int
sysio_thermal_warn_intr(struct sbus_soft_state *softsp);

int
sysio_err_init(struct sbus_soft_state *softsp, caddr_t address)
{
	if (sysio_err_flag == 0) {
		cmn_err(CE_CONT, "Warning: sysio errors not initialized\n");
		return (DDI_SUCCESS);
	}

	/*
	 * Get the address of the already mapped-in sysio/sbus error registers.
	 * Simply add each registers offset to the already mapped in address
	 * that was retrieved from the device node's "address" property,
	 * and passed as an argument to this function.
	 *
	 * Define a macro for the pointer arithmetic ...
	 */

#define	REG_ADDR(b, o)	(u_ll_t *)((caddr_t)(b) + (o))

	softsp->sysio_ecc_reg = REG_ADDR(address, OFF_SYSIO_ECC_REGS);
	softsp->sysio_ue_reg = REG_ADDR(address, OFF_SYSIO_UE_REGS);
	softsp->sysio_ce_reg = REG_ADDR(address, OFF_SYSIO_CE_REGS);
	softsp->sbus_err_reg = REG_ADDR(address, OFF_SBUS_ERR_REGS);

#undef	REG_ADDR

	/*
	 * ddi_add_intr *should* work for this purpose, but Srinath recommended
	 * 	using add_ivintr directly.
	 * XXX - we could get the MONDO values from the sbus interrupts prop.
	 */
	add_ivintr((softsp->upa_id << 6 | UE_ECC_MONDO), SBUS_UE_PIL,
		sysio_ue_intr, (caddr_t) softsp, NULL);
	add_ivintr((softsp->upa_id << 6 | CE_ECC_MONDO), SBUS_CE_PIL,
		sysio_ce_intr, (caddr_t) softsp, NULL);
	add_ivintr((softsp->upa_id << 6 | SBUS_ERR_MONDO), SBUS_ERR_PIL,
		sbus_err_intr, (caddr_t) softsp, NULL);

	/*
	 * If the thermal-interrupt property is in place,
	 * then register the thermal warning interrupt handler and
	 * program its mapping register
	 */
	thermal_interrupt_enabled = ddi_getprop(DDI_DEV_T_ANY, softsp->dip,
		DDI_PROP_DONTPASS, "thermal-interrupt", -1);

	if (thermal_interrupt_enabled == 1) {
		add_ivintr((softsp->upa_id << 6 | THERMAL_MONDO),
			SBUS_THERMAL_PIL, sysio_thermal_warn_intr,
			(caddr_t) softsp, NULL);
	}

	register_upa_func(UE_ECC_FTYPE, sbus_ctrl_ecc_err, (caddr_t)softsp);
	register_upa_func(DIS_ERR_FTYPE, sysio_dis_err, (caddr_t)softsp);

	sysio_init_err(softsp);

	return (DDI_SUCCESS);
}

int
sysio_err_resume_init(struct sbus_soft_state *softsp)
{
	sysio_init_err(softsp);
	return (DDI_SUCCESS);
}

static u_int
sysio_init_err(struct sbus_soft_state *softsp)
{
	volatile u_ll_t tmp_mondo_vec, tmpreg;
	volatile u_ll_t *mondo_vec_reg;
	u_int cpu_id;

	/*
	 * Hold the global interrupt distribution lock.
	 */
	mutex_enter(&intr_dist_lock);

	/*
	 * Program the mondo vector accordingly.  This MUST be the
	 * last thing we do.  Once we program the mondo, the device
	 * may begin to interrupt. Store it in the hardware reg.
	 */
	mondo_vec_reg = (u_ll_t *)(softsp->intr_mapping_reg + UE_ECC_MAPREG);
	cpu_id = intr_add_cpu(sbus_intrdist, (void *) softsp->dip,
		(softsp->upa_id << IMR_IGN_SHIFT) | UE_ECC_MONDO, 0);
	tmp_mondo_vec = (cpu_id << INTERRUPT_CPU_FIELD) | INTERRUPT_VALID;
	*mondo_vec_reg = tmp_mondo_vec;

	mondo_vec_reg = (u_ll_t *)(softsp->intr_mapping_reg + CE_ECC_MAPREG);
	cpu_id = intr_add_cpu(sbus_intrdist, (void *) softsp->dip,
		(softsp->upa_id << IMR_IGN_SHIFT) | CE_ECC_MONDO, 0);
	tmp_mondo_vec = (cpu_id << INTERRUPT_CPU_FIELD) | INTERRUPT_VALID;
	*mondo_vec_reg = tmp_mondo_vec;

	mondo_vec_reg = (u_ll_t *)(softsp->intr_mapping_reg + SBUS_ERR_MAPREG);
	cpu_id = intr_add_cpu(sbus_intrdist, (void *) softsp->dip,
		(softsp->upa_id << IMR_IGN_SHIFT) | SBUS_ERR_MONDO, 0);
	tmp_mondo_vec = (cpu_id << INTERRUPT_CPU_FIELD) | INTERRUPT_VALID;
	*mondo_vec_reg = tmp_mondo_vec;

	if (thermal_interrupt_enabled == 1) {
		mondo_vec_reg = (softsp->intr_mapping_reg + THERMAL_MAPREG);
		cpu_id = intr_add_cpu(sbus_intrdist, (void *) softsp->dip,
			(softsp->upa_id << IMR_IGN_SHIFT) | THERMAL_MONDO, 0);
		tmp_mondo_vec = (cpu_id << INTERRUPT_CPU_FIELD) |
			INTERRUPT_VALID;
		*mondo_vec_reg = tmp_mondo_vec;
	}

	/* Flush store buffers */
	tmpreg = *softsp->sbus_ctrl_reg;

	mutex_exit(&intr_dist_lock);

	/*
	 * XXX - This may already be set by the OBP.
	 */
	tmpreg = SYSIO_APCKEN;
	*softsp->sysio_ctrl_reg |= tmpreg;
	tmpreg = (SECR_ECC_EN | SECR_UE_INTEN | SECR_CE_INTEN);
	*softsp->sysio_ecc_reg = tmpreg;
	tmpreg = SB_CSR_ERRINT_EN;
	*softsp->sbus_err_reg |= tmpreg;

	/* Initialize timeout/bus error counter */
	timerclear(&softsp->bto_timestamp);
	softsp->bto_ctr = 0;

	return (0);
}

static u_int
sysio_dis_err(struct sbus_soft_state *softsp)
{
	volatile u_ll_t tmpreg;
	volatile u_ll_t *mondo_vec_reg, *clear_vec_reg;

	*softsp->sysio_ctrl_reg &= ~SYSIO_APCKEN;
	*softsp->sysio_ecc_reg = 0;
	*softsp->sbus_err_reg &= ~SB_CSR_ERRINT_EN;

	/* Lock the global interrupt distribution list lock */
	mutex_enter(&intr_dist_lock);

	/* Flush store buffers */
	tmpreg = *softsp->sbus_ctrl_reg;
#ifdef lint
	tmpreg = tmpreg;
#endif

	/* Unmap mapping registers */
	mondo_vec_reg = (softsp->intr_mapping_reg + UE_ECC_MAPREG);
	clear_vec_reg = (softsp->clr_intr_reg + UE_ECC_CLEAR);

	*mondo_vec_reg = 0;
	intr_rem_cpu((softsp->upa_id << IMR_IGN_SHIFT) | UE_ECC_MONDO);

	*clear_vec_reg = 0;

	mondo_vec_reg = (softsp->intr_mapping_reg + CE_ECC_MAPREG);
	clear_vec_reg = (softsp->clr_intr_reg + CE_ECC_CLEAR);

	*mondo_vec_reg = 0;

	intr_rem_cpu((softsp->upa_id << IMR_IGN_SHIFT) | CE_ECC_MONDO);

	*clear_vec_reg = 0;

	mondo_vec_reg = (softsp->intr_mapping_reg + SBUS_ERR_MAPREG);
	clear_vec_reg = (softsp->clr_intr_reg + SBUS_ERR_CLEAR);

	*mondo_vec_reg = 0;

	intr_rem_cpu((softsp->upa_id << IMR_IGN_SHIFT) | SBUS_ERR_MONDO);

	*clear_vec_reg = 0;

	/* Flush store buffers */
	tmpreg = *softsp->sbus_ctrl_reg;

	mutex_exit(&intr_dist_lock);

	return (0);
}

static u_int
sysio_ue_intr(struct sbus_soft_state *softsp)
{
	union {
		volatile u_ll_t	afsr;
		volatile u_ll_t	afar;
		u_int			i[2];
	} j, k;
	volatile u_ll_t *ue_reg, *afar_reg, *clear_reg;
	u_char size, offset;
	u_short id, inst;
	extern void set_auxioreg();

	/*
	 * Disable all further sbus errors, for this sbus instance, for
	 * what is guaranteed to be a fatal error. And grab any other cpus.
	 * But maybe we can figure out if it's from a user process or not?
	 * XXX - We leave that as an exercise for the server group!
	 */
	if (debug_sysio_errs) {
		set_auxioreg(1, 0);		/* turn led off */
		set_auxioreg(1, 1);		/* turn led on */
	} else {
		sysio_dis_err(softsp);		/* disabled sysio errors */
	}
	/*
	 * Then read and clear the afsr/afar and clear interrupt regs.
	 */
	ue_reg = (u_ll_t *)softsp->sysio_ue_reg;
	j.afsr = *ue_reg;
	afar_reg = (u_ll_t *)ue_reg + 1;
	k.afar = *afar_reg;
	*ue_reg = j.afsr;

	clear_reg = (softsp->clr_intr_reg + UE_ECC_CLEAR);
	*clear_reg = 0;

	/*
	 * size = (afsr & SB_UE_AFSR_SIZE) >> SB_UE_AFSR_SIZE_SHIFT;
	 * offset = (afsr & SB_UE_AFSR_OFF) >> SB_UE_AFSR_DW_SHIFT;
	 */
	size = (u_char) (j.i[0] >> 10) & 7;
	offset = (u_char) (j.i[0] >> 13) & 7;
	id = (u_short) softsp->upa_id;
	inst = (u_short) ddi_get_instance(softsp->dip);

	ue_error((u_ll_t *)&j.afsr, (u_ll_t *)&k.afar, 0, size, offset, id,
		inst, (afunc)sysio_log_ue_err);
	return (DDI_INTR_CLAIMED);
}

static int
sysio_log_ue_err(struct ecc_flt *ecc, char *unum)
{
	union ul {
		u_ll_t	afsr;
		u_ll_t	afar;
		u_ll_t	aligned_addr;
		u_int	i[2];
	} j, k, al;
	u_short id = ecc->flt_upa_id;
	u_short inst = ecc->flt_inst;

	j.afsr = ecc->flt_stat;
	k.afar = ecc->flt_addr;

	if (debug_sysio_errs) {
		register short loop, ce_err = 0;

		/* if (ecc->flt_stat & SB_UE_AFSR_P_PIO) */
		if ((j.i[0] >> 31) & 1) {
			cmn_err(CE_CONT, "SBus%d UE Primary Error from PIO: "
				"AFSR 0x%08x %08x AFAR 0x%08x %08x Id %d\n",
				inst, j.i[0], j.i[1], k.i[0], k.i[1], id);
		}
		/* if (ecc->flt_stat & SB_UE_AFSR_P_DRD) */
		if ((j.i[0] >> 30) & 1) {
			cmn_err(CE_CONT, "SBus%d UE Primary Error DMA read: "
			"AFSR 0x%08x %08x AFAR 0x%08x %08x SIMM %s Id %d\n",
				inst, j.i[0], j.i[1], k.i[0], k.i[1], unum, id);
		}
		/* if (ecc->flt_stat & SB_UE_AFSR_P_DWR) */
		if ((j.i[0] >> 29) & 1) {
			cmn_err(CE_CONT, "SBus%d UE Primary Error DMA write: "
			"AFSR 0x%08x %08x AFAR 0x%08x %08x SIMM %s Id %d\n",
				inst, j.i[0], j.i[1], k.i[0], k.i[1], unum, id);
		}
		loop = 8;
		al.aligned_addr = k.afar;
		al.i[1] &= 0xFFFFFFF0;
		(void) read_ecc_data(al.aligned_addr, loop, ce_err, 1);
		cmn_err(CE_CONT, "\tOffset 0x%x, Size %d, UPA MID 0x%x\n",
			((j.i[0] >> 13) & 7), ((j.i[0] >> 10) & 7),
			((j.i[0] >> 5) & 0x1F));
		return (UE_DEBUG); /* XXX - hack alert, should be fatal */
	}

	/* if (ecc->flt_stat & SB_UE_AFSR_P_PIO) */
	if ((j.i[0] >> 31) & 1) {
		cmn_err(CE_PANIC, "SBus%d UE Primary Error from PIO: "
			"AFSR 0x%08x %08x AFAR 0x%08x %08x Id %d",
			inst, j.i[0], j.i[1], k.i[0], k.i[1], id);
	}
	/* if (ecc->flt_stat & SB_UE_AFSR_P_DRD) */
	if ((j.i[0] >> 30) & 1) {
		cmn_err(CE_PANIC, "SBus%d UE Primary Error DMA read: "
			"AFSR 0x%08x %08x AFAR 0x%08x %08x SIMM %s Id %d",
			inst, j.i[0], j.i[1], k.i[0], k.i[1], unum, id);
	}
	/* if (ecc->flt_stat & SB_UE_AFSR_P_DWR) */
	if ((j.i[0] >> 29) & 1) {
		cmn_err(CE_PANIC, "SBus%d UE Primary Error DVMA write: "
			"AFSR 0x%08x %08x AFAR 0x%08x %08x SIMM %s Id %d",
			inst, j.i[0], j.i[1], k.i[0], k.i[1], unum, id);
	}
	/*
	 * We should never hit the secondary error panics.
	 */
	/* if (ecc->flt_stat & SB_UE_AFSR_S_PIO) */
	if ((j.i[0] >> 28) & 1) {
		cmn_err(CE_PANIC, "SBus%d UE Secondary Error from PIO: "
			"AFSR 0x%08x %08x AFAR 0x%08x %08x Id %d",
			inst, j.i[0], j.i[1], k.i[0], k.i[1], id);
	}
	/* if (ecc->flt_stat & SB_UE_AFSR_S_DRD) */
	if ((j.i[0] >> 27) & 1) {
		cmn_err(CE_PANIC, "SBus%d UE Secondary Error DMA read: "
			"AFSR 0x%08x %08x AFAR 0x%08x %08x SIMM %s Id %d",
			inst, j.i[0], j.i[1], k.i[0], k.i[1], unum, id);
	}
	/* if (ecc->flt_stat & SB_UE_AFSR_S_DWR) */
	if ((j.i[0] >> 26) & 1) {
		cmn_err(CE_PANIC, "SBus%d UE Secondary  Error DMA write: "
			"AFSR 0x%08x %08x AFAR 0x%08x %08x SIMM %s Id %d",
			inst, j.i[0], j.i[1], k.i[0], k.i[1], unum, id);
	}
	/* NOTREACHED */
	return (UE_FATAL);		/* should be always fatal */
}

static u_int
sysio_ce_intr(struct sbus_soft_state *softsp)
{
	union {
		volatile u_ll_t	afsr;
		volatile u_ll_t	afar;
		u_int			i[2];
	} j, k;
	volatile u_ll_t *afar_reg, *clear_reg, *ce_reg;
	u_char ecc_synd, size, offset;
	u_short id, inst;
	extern void set_auxioreg();

	if (debug_sysio_errs) {
		set_auxioreg(1, 0);		/* turn led off */
		set_auxioreg(1, 1);		/* turn led on */
	}
	ce_reg = (u_ll_t *)softsp->sysio_ce_reg;
	j.afsr = *ce_reg;
	afar_reg = (u_ll_t *)ce_reg + 1;
	k.afar = *afar_reg;
	*ce_reg = j.afsr;

	clear_reg = (softsp->clr_intr_reg + CE_ECC_CLEAR);
	*clear_reg = 0;

	/*
	 * offset = (afsr & SB_CE_AFSR_OFF) >> SB_CE_AFSR_OFFSET_SHIFT;
	 * size = (afsr & SB_CE_AFSR_SIZE) >> SB_CE_AFSR_SIZE_SHIFT;
	 * ecc_synd = (afsr & SB_CE_AFSR_SYND) >> SB_CE_SYND_SHIFT;
	 */
	ecc_synd = (u_char) (j.i[0] >> 16) & 0xFF;
	size = (u_char) (j.i[0] >> 10) & 7;
	offset = (u_char) (j.i[0] >> 13) & 7;
	id = (u_short) softsp->upa_id;
	inst = (u_short) ddi_get_instance(softsp->dip);

	ce_error((u_ll_t *)&j.afsr, (u_ll_t *)&k.afar, ecc_synd,
		size, offset, id, inst, (afunc)sysio_log_ce_err);
	return (DDI_INTR_CLAIMED);
}

static int
sysio_log_ce_err(struct ecc_flt *ecc, char *unum)
{
	union ul {
		u_ll_t    afsr;
		u_ll_t    afar;
		u_int		i[2];
	} j, k;
	u_short id = ecc->flt_upa_id;
	u_short inst = ecc->flt_inst;
	int memory_err = 0;
	extern int ce_verbose;

	j.afsr = ecc->flt_stat;
	k.afar = ecc->flt_addr;

	/* if (ecc->flt_stat & SB_CE_AFSR_P_PIO) */
	if ((j.i[0] >> 31) & 1) {
		cmn_err(CE_CONT, "SBus%d CE Primary Error from PIO: "
			"AFSR 0x%08x %08x AFAR 0x%08x %08x Id %d\n",
			inst, j.i[0], j.i[1], k.i[0], k.i[1], id);
	}
	/* if (ecc->flt_stat & SB_CE_AFSR_P_DRD) */
	if ((j.i[0] >> 30) & 1) {
		if ((debug_sysio_errs) || (ce_verbose))
			cmn_err(CE_CONT, "SBus%d CE Primary Error DMA read: "
			"AFSR 0x%08x %08x AFAR 0x%08x %08x SIMM %s Id %d\n",
			inst, j.i[0], j.i[1], k.i[0], k.i[1], unum, id);
		memory_err = 1;
	}
	/* if (ecc->flt_stat & SB_CE_AFSR_P_DWR) */
	if ((j.i[0] >> 29) & 1) {
		if ((debug_sysio_errs) || (ce_verbose))
			cmn_err(CE_CONT, "SBus%d CE Primary Error DMA write: "
			"AFSR 0x%08x %08x AFAR 0x%08x %08x SIMM %s Id %d\n",
			inst, j.i[0], j.i[1], k.i[0], k.i[1], unum, id);
		memory_err = 1;
	}
	/* if (ecc->flt_stat & SB_CE_AFSR_S_PIO) */
	if ((j.i[0] >> 28) & 1) {
		cmn_err(CE_CONT, "SBus%d CE Secondary Error from PIO: "
			"AFSR 0x%08x %08x AFAR 0x%08x %08x Id %d\n",
			inst, j.i[0], j.i[1], k.i[0], k.i[1], id);
	}
	/* if (ecc->flt_stat & SB_CE_AFSR_S_DRD) */
	if ((j.i[0] >> 27) & 1) {
		if ((debug_sysio_errs) || (ce_verbose))
			cmn_err(CE_CONT, "SBus%d CE Secondary Error DMA read: "
			"AFSR 0x%08x %08x AFAR 0x%08x %08x SIMM %s Id %d\n",
			inst, j.i[0], j.i[1], k.i[0], k.i[1], unum, id);
	}
	/* if (ecc->flt_stat & SB_CE_AFSR_S_DWR) */
	if ((j.i[0] >> 26) & 1) {
		if ((debug_sysio_errs) || (ce_verbose))
			cmn_err(CE_CONT, "SBus%d CE Secondary Error DMA write: "
			"AFSR 0x%08x %08x AFAR 0x%08x %08x SIMM %s Id %d\n",
			inst, j.i[0], j.i[1], k.i[0], k.i[1], unum, id);
	}
	if ((memory_err == 0) || (debug_sysio_errs) || (ce_verbose))
		cmn_err(CE_CONT,
		"\tSyndrome 0x%x, Offset 0x%x, Size %d, UPA MID 0x%x\n",
		((j.i[0] >> 16) & 0xFF), ((j.i[0] >> 13) & 7),
		((j.i[0] >> 10) & 7), ((j.i[0] >> 5) & 0x1F));
	return (memory_err);
}

static u_int
sbus_err_intr(struct sbus_soft_state *softsp)
{
	union {
		volatile u_ll_t	afsr;
		volatile u_ll_t	afar;
		u_int			i[2];
	} j, k;
	u_short id, inst;
	int cleared = 0;
	volatile u_ll_t *afar_reg;
	extern void debug_enter();

	j.afsr = *softsp->sbus_err_reg;
	afar_reg = (u_ll_t *)softsp->sbus_err_reg + 1;
	k.afar = *afar_reg;

	if (MUTEX_NOT_HELD(&softsp->pokefault_mutex)) {
		sbus_clear_intr(softsp, (u_ll_t *)&j.afsr);
		cleared = 1;
	}

	id = (u_short) softsp->upa_id;
	inst = (u_short) ddi_get_instance(softsp->dip);
	if (debug_sysio_errs) {
		if (MUTEX_HELD(&softsp->pokefault_mutex)) {
			if (softsp->pokefault == -1)
				softsp->pokefault = 1;
		}
		if (!cleared)
			sbus_clear_intr(softsp, (u_ll_t *)&j.afsr);
		cmn_err(CE_CONT, "SBus%d Error: AFSR 0x%08x %08x "
			"AFAR 0x%08x %08x Id %d\n",
			inst, j.i[0], j.i[1], k.i[0], k.i[1], id);
		debug_enter("sbus_err_intr");
	} else {
		sbus_log_error(softsp, (u_ll_t *)&j.afsr,
				(u_ll_t *)&k.afar, id, inst, cleared);
	}
	if (!cleared) {
		sbus_clear_intr(softsp, (u_ll_t *)&j.afsr);
	}

	if (softsp->pokefault && curthread->t_nofault) {
		longjmp(curthread->t_nofault);
	}
	return (DDI_INTR_CLAIMED);
}

static void
sbus_clear_intr(struct sbus_soft_state *softsp, u_ll_t *pafsr)
{
	volatile u_ll_t *clear_reg;

	*softsp->sbus_err_reg = *pafsr;
	clear_reg = (softsp->clr_intr_reg + SBUS_ERR_CLEAR);
	*clear_reg = 0;
}

static void
sbus_log_error(struct sbus_soft_state *softsp, u_ll_t *pafsr, u_ll_t *pafar,
		u_short id, u_short inst, int cleared)
{
	union ul {
		u_ll_t afsr;
		u_ll_t afar;
		u_int i[2];
	} j, k;
	int level = CE_WARN;

	j.afsr = *pafsr;
	k.afar = *pafar;
	/* if (afsr & SB_AFSR_P_LE) */
	if ((j.i[0] >> 31) & 1) {
		if (!cleared)
			sbus_clear_intr(softsp, (u_ll_t *)&j.afsr);
		cmn_err(CE_PANIC, "SBus%d Primary Error Late PIO: "
			"AFSR 0x%08x %08x AFAR 0x%08x %08x Id %d",
			inst, j.i[0], j.i[1], k.i[0], k.i[1], id);
	}
	/* if (afsr & SB_AFSR_P_TO) */
	if ((j.i[0] >> 30) & 1) {
		if (MUTEX_HELD(&softsp->pokefault_mutex)) {
			if (softsp->pokefault == -1) {
				softsp->pokefault = 1;
				return;
			}
		}
		if (sbus_check_bto(softsp)) {
			if (!cleared)
				sbus_clear_intr(softsp, (u_ll_t *)&j.afsr);
			level = CE_PANIC;
		}
		cmn_err(level, "SBus%d Primary Error Timeout: "
			"AFSR 0x%08x %08x AFAR 0x%08x %08x Id %d",
			inst, j.i[0], j.i[1], k.i[0], k.i[1], id);
	}
	/* if (afsr & SB_AFSR_P_BERR) */
	if ((j.i[0] >> 29) & 1) {
		if (MUTEX_HELD(&softsp->pokefault_mutex)) {
			if (softsp->pokefault == -1) {
				softsp->pokefault = 1;
				return;
			}
		}
		if (sbus_check_bto(softsp)) {
			if (!cleared)
				sbus_clear_intr(softsp, (u_ll_t *)&j.afsr);
			level = CE_PANIC;
		}
		cmn_err(level, "SBus%d Primary Error Bus Error: "
			"AFSR 0x%08x %08x AFAR 0x%08x %08x Id %d\n",
			inst, j.i[0], j.i[1], k.i[0], k.i[1], id);
	}

	/* if (afsr & SB_AFSR_P_LE) */
	if ((j.i[0] >> 28) & 1) {
		if (!cleared)
			sbus_clear_intr(softsp, (u_ll_t *)&j.afsr);
		cmn_err(CE_PANIC, "SBus%d Secondary Late PIO Error: "
			"AFSR 0x%08x %08x AFAR 0x%08x %08x Id %d",
			inst, j.i[0], j.i[1], k.i[0], k.i[1], id);
	}
	/* if ((afsr & SB_AFSR_P_TO) */
	if ((j.i[0] >> 27) & 1) {
		if (sbus_check_bto(softsp)) {
			if (!cleared)
				sbus_clear_intr(softsp, (u_ll_t *)&j.afsr);
			level = CE_PANIC;
		}
		cmn_err(level, "SBus%d Secondary Timeout Error: "
			"AFSR 0x%08x %08x AFAR 0x%08x %08x Id %d",
			inst, j.i[0], j.i[1], k.i[0], k.i[1], id);
	}
	/* if (afsr & SB_AFSR_P_BERR) */
	if ((j.i[0] >> 26) & 1) {
		if (sbus_check_bto(softsp)) {
			if (!cleared)
				sbus_clear_intr(softsp, (u_ll_t *)&j.afsr);
			level = CE_PANIC;
		}
		cmn_err(level, "SBus%d Secondary Bus Error: "
			"AFSR 0x%08x %08x AFAR 0x%08x %08x Id %d",
			inst, j.i[0], j.i[1], k.i[0], k.i[1], id);
	}
}


static int
sbus_check_bto(struct sbus_soft_state *softsp)
{
	struct timeval now, now_diff;

	(void) uniqtime(&now);
	if (timerisset(&softsp->bto_timestamp)) {
		/* CSTYLED */
		if (timercmp(&softsp->bto_timestamp, &now, >)) {
			/* timer has wrapped, so restart it from now */
			softsp->bto_timestamp = now;
			softsp->bto_ctr = 1;
		} else {
			/*
			 * subtract (smaller) initial saved time from the
			 * (larger) now/current time, to get the difference
			 */
			now_diff = now;
			timevalsub(&now_diff, &softsp->bto_timestamp);
			if ((now_diff.tv_sec <= bto_secs) &&
			    (softsp->bto_ctr > bto_cnt)) {
				/* thanks for playing, sorry you lose */
				return (1);
			} else if (now_diff.tv_sec > bto_secs) {
				/* restart timer due to over */
				softsp->bto_timestamp = now;
				softsp->bto_ctr = 1;
			} else {
				softsp->bto_ctr++;
			}
		}
	} else {
		/* initial start of timer */
		softsp->bto_timestamp = now;
		softsp->bto_ctr = 1;
	}
	return (0);
}

static u_int
sbus_ctrl_ecc_err(struct sbus_soft_state *softsp)
{
	register int fatal = 0;
	u_int dma_perr, pio_perr;
	u_short id, inst;
	volatile union {
		u_ll_t sb_csr;
		u_int i[2];
	} j;

	j.sb_csr = *softsp->sbus_ctrl_reg;
	id = (u_short) softsp->upa_id;
	inst = (u_short) ddi_get_instance(softsp->dip);
	if (debug_sysio_errs) {
		cmn_err(CE_CONT,
		"sbus_ctrl_ecc_error: SBus%d Control Reg 0x%08x %08x Id %d\n",
			inst, j.i[0], j.i[1], id);
	}
	/* if (sb_csr & SB_CSR_DMA_PERRS) { */
	dma_perr = ((j.i[0] >> 16) & 0x3F);
	/* if (sb_csr & SB_CSR_PIO_PERRS) { */
	pio_perr = ((j.i[0] >> 8) & 0x7F);
	if ((dma_perr) || (pio_perr)) {		/* clear errors */
		*softsp->sbus_ctrl_reg = j.sb_csr;
		sbus_log_csr_error((u_ll_t *)&j.sb_csr, id, inst);
		/* NOTREACHED */
	}
	return (fatal);
}

static void
sbus_log_csr_error(u_ll_t *psb_csr, u_short id, u_short inst)
{
	union ul {
		u_ll_t sb_csr;
		u_int i[2];
	} j;

	/*
	 * Print out SBus error information.
	 */

	j.sb_csr = *psb_csr;
	/* if (sb_csr & SB_CSR_DPERR_S14) */
	if ((j.i[0] >> 21) & 1) {
		cmn_err(CE_PANIC,
		"SBus%d Slot 14 DVMA Parity Error: AFSR 0x%08x %08x Id %d",
			inst, j.i[0], j.i[1], id);
	}
	/* if (sb_csr & SB_CSR_DPERR_S13) */
	if ((j.i[0] >> 20) & 1) {
		cmn_err(CE_PANIC,
		"SBus%d Slot 13 DVMA Parity Error: AFSR 0x%08x %08x Id %d",
			inst, j.i[0], j.i[1], id);
	}
	/* if (sb_csr & SB_CSR_DPERR_S3) */
	if ((j.i[0] >> 19) & 1) {
		cmn_err(CE_PANIC,
		"SBus%d Slot 3 DVMA Parity Error: AFSR 0x%08x %08x Id %d",
			inst, j.i[0], j.i[1], id);
	}
	/* if (sb_csr & SB_CSR_DPERR_S2) */
	if ((j.i[0] >> 18) & 1) {
		cmn_err(CE_PANIC,
		"SBus%d Slot 2 DVMA Parity Error: AFSR 0x%08x %08x Id %d",
			inst, j.i[0], j.i[1], id);
	}
	/* if (sb_csr & SB_CSR_DPERR_S1) */
	if ((j.i[0] >> 17) & 1) {
		cmn_err(CE_PANIC,
		"SBus%d Slot 1 DVMA Parity Error: AFSR 0x%08x %08x Id %d",
			inst, j.i[0], j.i[1], id);
	}
	/* if (sb_csr & SB_CSR_DPERR_S0) */
	if ((j.i[0] >> 16) & 1) {
		cmn_err(CE_PANIC,
		"SBus%d Slot 0 DVMA Parity Error: AFSR 0x%08x %08x Id %d",
			inst, j.i[0], j.i[1], id);
	}
	/* if (sb_csr & SB_CSR_PPERR_S15) */
	if ((j.i[0] >> 14) & 1) {
		cmn_err(CE_PANIC,
		"SBus%d Slot 15 PIO Parity Error: AFSR 0x%08x %08x Id %d",
			inst, j.i[0], j.i[1], id);
	}
	/* if (sb_csr & SB_CSR_PPERR_S14) */
	if ((j.i[0] >> 13) & 1) {
		cmn_err(CE_PANIC,
		"SBus%d Slot 14 PIO Parity Error: AFSR 0x%08x %08x Id %d",
			inst, j.i[0], j.i[1], id);
	}
	/* if (sb_csr & SB_CSR_PPERR_S13) */
	if ((j.i[0] >> 12) & 1) {
		cmn_err(CE_PANIC,
		"SBus%d Slot 13 PIO Parity Error: AFSR 0x%08x %08x Id %d",
			inst, j.i[0], j.i[1], id);
	}
	/* if (sb_csr & SB_CSR_PPERR_S3) */
	if ((j.i[0] >> 11) & 1) {
		cmn_err(CE_PANIC,
		"SBus%d Slot 3 PIO Parity Error: AFSR 0x%08x %08x Id %d",
			inst, j.i[0], j.i[1], id);
	}
	/* if (sb_csr & SB_CSR_PPERR_S2) */
	if ((j.i[0] >> 10) & 1) {
		cmn_err(CE_PANIC,
		"SBus%d Slot 2 PIO Parity Error: AFSR 0x%08x %08x Id %d",
			inst, j.i[0], j.i[1], id);
	}
	/* if (sb_csr & SB_CSR_PPERR_S1) */
	if ((j.i[0] >> 9) & 1) {
		cmn_err(CE_PANIC,
		"SBus%d Slot 1 PIO Parity Error: AFSR 0x%08x %08x Id %d",
			inst, j.i[0], j.i[1], id);
	}
	/* if (sb_csr & SB_CSR_PPERR_S0) */
	if ((j.i[0] >> 8) & 1) {
		cmn_err(CE_PANIC,
		"SBus%d Slot 0 PIO Parity Error: AFSR 0x%08x %08x Id %d",
			inst, j.i[0], j.i[1], id);
	}
}

/*
 * Sysio Thermal Warning interrupt handler
 */
static u_int
sysio_thermal_warn_intr(struct sbus_soft_state *softsp)
{
	volatile u_longlong_t *clear_reg;
	volatile u_ll_t tmp_mondo_vec;
	volatile u_ll_t *mondo_vec_reg;
	char thermal_warn_msg[] = "Severe over-temperature condition detected!";

	/*
	 * Hold the global interrupt distribution lock.
	 */
	mutex_enter(&intr_dist_lock);

	/*
	 * Take off the Thermal Warning interrupt and
	 * remove its interrupt handler.
	 */
	mondo_vec_reg = (softsp->intr_mapping_reg + THERMAL_MAPREG);
	tmp_mondo_vec = *mondo_vec_reg;
	tmp_mondo_vec &= ~INTERRUPT_VALID;
	*mondo_vec_reg = tmp_mondo_vec;

	intr_rem_cpu((softsp->upa_id << IMR_IGN_SHIFT) | THERMAL_MONDO);

	rem_ivintr((softsp->upa_id << 6 | THERMAL_MONDO), NULL);

	clear_reg = (softsp->clr_intr_reg + THERMAL_CLEAR);
	*clear_reg = 0;

	mutex_exit(&intr_dist_lock);

	if (oven_test) {
		cmn_err(CE_NOTE, "OVEN TEST: %s", thermal_warn_msg);
		return (DDI_INTR_CLAIMED);
	} else {
		extern void power_down();
		extern void do_shutdown();

		cmn_err(CE_WARN, "%s", thermal_warn_msg);
		cmn_err(CE_WARN, "Powering down...");

		do_shutdown();

		/*
		 * just in case do_shutdown() fails
		 */
		timeout((void(*)(caddr_t))power_down, NULL, 100*hz);

		return (DDI_INTR_CLAIMED);
	}
}
