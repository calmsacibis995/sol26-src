/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fhc.c 1.52	96/09/03 SMI"

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/obpdefs.h>
#include <sys/promif.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/sysmacros.h>
#include <sys/ivintr.h>
#include <sys/intr.h>
#include <sys/intreg.h>
#include <sys/autoconf.h>
#include <sys/modctl.h>
#include <sys/spl.h>
#include <sys/time.h>
#include <sys/machsystm.h>
#include <sys/machcpuvar.h>
#include <sys/procset.h>
#include <sys/fhc.h>
#include <sys/jtag.h>

/* Useful debugging Stuff */
#include <sys/nexusdebug.h>

/*
 * This table represents the FHC interrupt priorities.  They range from
 * 1-15, and have been modeled after the sun4d interrupts. The mondo
 * number anded with 0x7 is used to index into this table. This was
 * done to save table space.
 */
static int fhc_int_priorities[] = {
	PIL_15,			/* System interrupt priority */
	PIL_12,			/* zs interrupt priority */
	PIL_15,			/* TOD interrupt priority */
	PIL_15			/* Fan Fail priority */
};

/*
 * The dont_calibrate variable is meant to be set to one in /etc/system
 * or by boot -h so that the calibration tables are not used. This
 * is useful for checking thermistors whose output seems to be incorrect.
 */
static int dont_calibrate = 0;

/* Only one processor should powerdown the system. */
static int powerdown_started = 0;

/* Let user disable overtemp powerdown. */
int enable_overtemp_powerdown = 1;

/*
 * The following tables correspond to the degress Celcius for each count
 * value possible from the 8-bit A/C convertors on each type of system
 * board for the UltraSPARC Server systems. To access a temperature,
 * just index into the correct table using the count from the A/D convertor
 * register, and that is the correct temperature in degress Celsius. These
 * values can be negative.
 */
static short cpu_table[] = {
-16,	-14,	-12,	-10,	-8,	-6,	-4,	-2,	/* 0-7 */
1,	4,	6,	8,	10,	12,	13,	15,	/* 8-15 */
16,	18,	19,	20,	22,	23,	24,	25,	/* 16-23 */
26,	27,	28,	29,	30,	31,	32,	33,	/* 24-31 */
34,	35,	35,	36,	37,	38,	39,	39,	/* 32-39 */
40,	41,	41,	42,	43,	44,	44,	45,	/* 40-47 */
46,	46,	47,	47,	48,	49,	49,	50,	/* 48-55 */
51,	51,	52,	53,	53,	54,	54,	55,	/* 56-63 */
55,	56,	56,	57,	57,	58,	58,	59,	/* 64-71 */
60,	60,	61,	61,	62,	62,	63,	63,	/* 72-79 */
64,	64,	65,	65,	66,	66,	67,	67,	/* 80-87 */
68,	68,	69,	69,	70,	70,	71,	71,	/* 88-95 */
72,	72,	73,	73,	74,	74,	75,	75,	/* 96-103 */
76,	76,	77,	77,	78,	78,	79,	79,	/* 104-111 */
80,	80,	81,	81,	82,	82,	83,	83,	/* 112-119 */
84,	84,	85,	85,	86,	86,	87,	87,	/* 120-127 */
88,	88,	89,	89,	90,	90,	91,	91,	/* 128-135 */
92,	92,	93,	93,	94,	94,	95,	95,	/* 136-143 */
96,	96,	97,	98,	98,	99,	99,	100,	/* 144-151 */
100,	101,	101,	102,	103,	103,	104,	104,	/* 152-159 */
105,	106,	106,	107,	107,	108,	109,	109,	/* 160-167 */
110,								/* 168 */
};

#define	CPU_MX_CNT	(sizeof (cpu_table)/sizeof (short))

static short cpu2_table[] = {
-17,	-16,	-15,	-14,	-13,	-12,	-11,	-10,	/* 0-7 */
-9,	-8,	-7,	-6,	-5,	-4,	-3,	-2,	/* 8-15 */
-1,	0,	1,	2,	3,	4,	5,	6,	/* 16-23 */
7,	8,	9,	10,	11,	12,	13,	13,	/* 24-31 */
14,	15,	16,	16,	17,	18,	18,	19,	/* 32-39 */
20,	20,	21,	22,	22,	23,	24,	24,	/* 40-47 */
25,	25,	26,	26,	27,	27,	28,	28,	/* 48-55 */
29,	30,	30,	31,	31,	32,	32,	33,	/* 56-63 */
33,	34,	34,	35,	35,	36,	36,	37,	/* 64-71 */
37,	37,	38,	38,	39,	39,	40,	40,	/* 72-79 */
41,	41,	42,	42,	43,	43,	43,	44,	/* 80-87 */
44,	45,	45,	46,	46,	46,	47,	47,	/* 88-95 */
48,	48,	49,	49,	50,	50,	50,	51,	/* 96-103 */
51,	52,	52,	53,	53,	53,	54,	54,	/* 104-111 */
55,	55,	56,	56,	56,	57,	57,	58,	/* 112-119 */
58,	59,	59,	59,	60,	60,	61,	61,	/* 120-127 */
62,	62,	63,	63,	63,	64,	64,	65,	/* 128-135 */
65,	66,	66,	67,	67,	68,	68,	68,	/* 136-143 */
69,	69,	70,	70,	71,	71,	72,	72,	/* 144-151 */
73,	73,	74,	74,	75,	75,	76,	76,	/* 152-159 */
77,	77,	78,	78,	79,	79,	80,	80,	/* 160-167 */
81,	81,	82,	83,	83,	84,	84,	85,	/* 168-175 */
85,	86,	87,	87,	88,	88,	89,	90,	/* 176-183 */
90,	91,	92,	92,	93,	94,	94,	95,	/* 184-191 */
96,	96,	97,	98,	99,	99,	100,	101,	/* 192-199 */
102,	103,	103,	104,	105,	106,	107,	108,	/* 200-207 */
109,	110,							/* 208-209 */
};

#define	CPU2_MX_CNT	(sizeof (cpu2_table)/sizeof (short))

static short io_table[] = {
0,	0,	0,	0,	0,	0,	0,	0,	/* 0-7 */
0,	0,	0,	0,	0,	0,	0,	0,	/* 8-15 */
0,	0,	0,	0,	0,	0,	0,	0,	/* 16-23 */
0,	0,	0,	0,	0,	0,	0,	0,	/* 24-31 */
0,	0,	0,	0,	0,	0,	0,	0,	/* 32-39 */
0,	3,	7,	10,	13,	15,	17,	19,	/* 40-47 */
21,	23,	25,	27,	28,	30,	31,	32,	/* 48-55 */
34,	35,	36,	37,	38,	39,	41,	42,	/* 56-63 */
43,	44,	45,	46,	46,	47,	48,	49,	/* 64-71 */
50,	51,	52,	53,	53,	54,	55,	56,	/* 72-79 */
57,	57,	58,	59,	60,	60,	61,	62,	/* 80-87 */
62,	63,	64,	64,	65,	66,	66,	67,	/* 88-95 */
68,	68,	69,	70,	70,	71,	72,	72,	/* 96-103 */
73,	73,	74,	75,	75,	76,	77,	77,	/* 104-111 */
78,	78,	79,	80,	80,	81,	81,	82,	/* 112-119 */
};

#define	IO_MN_CNT	40
#define	IO_MX_CNT	(sizeof (io_table)/sizeof (short))

static short clock_table[] = {
0,	0,	0,	0,	0,	0,	0,	0,	/* 0-7 */
0,	0,	0,	0,	1,	2,	4,	5,	/* 8-15 */
7,	8,	10,	11,	12,	13,	14,	15,	/* 16-23 */
17,	18,	19,	20,	21,	22,	23,	24,	/* 24-31 */
24,	25,	26,	27,	28,	29,	29,	30,	/* 32-39 */
31,	32,	32,	33,	34,	35,	35,	36,	/* 40-47 */
37,	38,	38,	39,	40,	40,	41,	42,	/* 48-55 */
42,	43,	44,	44,	45,	46,	46,	47,	/* 56-63 */
48,	48,	49,	50,	50,	51,	52,	52,	/* 64-71 */
53,	54,	54,	55,	56,	57,	57,	58,	/* 72-79 */
59,	59,	60,	60,	61,	62,	63,	63,	/* 80-87 */
64,	65,	65,	66,	67,	68,	68,	69,	/* 88-95 */
70,	70,	71,	72,	73,	74,	74,	75,	/* 96-103 */
76,	77,	78,	78,	79,	80,	81,	82,	/* 104-111 */
};

#define	CLK_MN_CNT	11
#define	CLK_MX_CNT	(sizeof (clock_table)/sizeof (short))

/*
 * System temperature limits.
 *
 * The following variables are the warning and danger limits for the
 * different types of system boards. The limits are different because
 * the various boards reach different nominal temperatures because
 * of the different components that they contain.
 *
 * The warning limit is the temperature at which the user is warned.
 * The danger limit is the temperature at which the system is shutdown.
 * In the case of CPU/Memory system boards, the system will attempt
 * to offline and power down processors on a board in an attempt to
 * bring the board back into the nominal temperature range before
 * shutting down the system.
 *
 * These values can be tuned via /etc/system or boot -h.
 */
short cpu_warn_temp = 73;	/* CPU/Memory Warning Temperature */
short cpu_danger_temp = 83;	/* CPU/Memory Danger Temperature */
short io_warn_temp = 60;	/* IO Board Warning Temperature */
short io_danger_temp = 68;	/* IO Board Danger Temperature */
short clk_warn_temp = 60;	/* Clock Board Warning Temperature */
short clk_danger_temp = 68;	/* Clock Board Danger Temperature */

short dft_warn_temp = 60;	/* default warning temp value */
short dft_danger_temp = 68;	/* default danger temp value */

/*
 * This variable tells us if we are in a heat chamber. It is set
 * early on in boot, after we check the OBP 'mfg-mode' property in
 * the options node.
 */
static int temperature_chamber = -1;

/*
 * Driver global board list mutex and list head pointer. The list is
 * protected by the mutex and contains a record of all known boards,
 * whether they are active and running in the kernel, boards disabled
 * by OBP, or boards hotplugged after the system has booted UNIX.
 */
static kmutex_t bdlist_mutex;
static struct bd_list *bd_list = NULL;

/*
 * Driver global fault list mutex and list head pointer. The list is
 * protected by the mutex and contains a record of all known faults.
 * Faults can be inherited from the PROM or detected by the kernel.
 */
static kmutex_t ftlist_mutex;
static struct ft_list *ft_list = NULL;
static int ft_nfaults = 0;

/*
 * Table of all known fault strings. This table is indexed by the fault
 * type. Do not change the ordering of the table without redefining the
 * fault type enum list on fhc.h.
 */
char *ft_str_table[] = {
	{ "Core Power Supply" },		/* FT_CORE_PS */
	{ "Overtemp" },				/* FT_OVERTEMP */
	{ "AC Power" },				/* FT_AC_PWR */
	{ "Peripheral Power Supply" },		/* FT_PPS */
	{ "System 3.3 Volt Power" },		/* FT_CLK_33 */
	{ "System 5.0 Volt Power" },		/* FT_CLK_50 */
	{ "Peripheral 5.0 Volt Power" },	/* FT_V5_P */
	{ "Peripheral 12 Volt Power" },		/* FT_V12_P */
	{ "Auxiliary 5.0 Volt Power" },		/* FT_V5_AUX */
	{ "Peripheral 5.0 Volt Precharge" },	/* FT_V5_P_PCH */
	{ "Peripheral 12 Volt Precharge" },	/* FT_V12_P_PCH */
	{ "System 3.3 Volt Precharge" },	/* FT_V3_PCH */
	{ "System 5.0 Volt Precharge" },	/* FT_V5_PCH */
	{ "Peripheral Power Supply Fans" },	/* FT_PPS_FAN */
	{ "Rack Exhaust Fan" },			/* FT_RACK_EXH */
	{ "Disk Drive Fan" },			/* FT_DSK_FAN */
	{ "AC Box Fan" },			/* FT_AC_FAN */
	{ "Key Switch Fan" },			/* FT_KEYSW_FAN */
	{ "Minimum Power" },			/* FT_INSUFFICIENT_POWER */
	{ "PROM detected" },			/* FT_PROM */
	{ "Hot Plug Support System" }		/* FT_HOT_PLUG */
};

int ft_max_index = (sizeof (ft_str_table) / sizeof (char *));

/*
 * Function prototypes
 */
static int fhc_ctlops(dev_info_t *, dev_info_t *, ddi_ctl_enum_t,
			void *, void *);

static ddi_intrspec_t fhc_get_intrspec(dev_info_t *dip,
					dev_info_t *rdip,
					u_int inumber);

static int fhc_add_intrspec(dev_info_t *dip, dev_info_t *rdip,
	ddi_intrspec_t intrspec, ddi_iblock_cookie_t *iblock_cookiep,
	ddi_idevice_cookie_t *idevice_cookiep,
	u_int (*int_handler)(caddr_t int_handler_arg),
	caddr_t int_handler_arg, int kind);

static void fhc_remove_intrspec(dev_info_t *dip, dev_info_t *rdip,
	ddi_intrspec_t intrspec, ddi_iblock_cookie_t iblock_cookiep);

static int fhc_identify(dev_info_t *devi);
static int fhc_attach(dev_info_t *devi, ddi_attach_cmd_t cmd);
static int fhc_detach(dev_info_t *devi, ddi_detach_cmd_t cmd);
static int fhc_init(struct fhc_soft_state *, enum board_type);

static void fhc_uninit_child(dev_info_t *child);

static int fhc_ctl_xlate_intrs(dev_info_t *dip,
	dev_info_t *rdip, int *in,
	struct ddi_parent_private_data *pdptr);

static void fhc_add_kstats(struct fhc_soft_state *);
static int fhc_kstat_update(kstat_t *, int);
static int check_for_chamber(void);
static int ft_ks_snapshot(struct kstat *, void *, int);
static int ft_ks_update(struct kstat *, int);

/*
 * board type and A/D convertor output passed in and real temperature
 * is returned.
 */
static short calibrate_temp(enum board_type, u_char, u_int);
static enum temp_state get_temp_state(enum board_type, short);

/* Routine to determine if there are CPUs on this board. */
static int cpu_on_board(int);

static void build_bd_display_str(char *, enum board_type, int);

/* Interrupt distribution callback function. */
static void fhc_intrdist(void *, int, u_int);

extern struct cb_ops no_cb_ops;
extern struct cpu_node cpunodes[];
extern void halt(char *);
extern void power_down(char *);

/*
 * Configuration data structures
 */
static struct bus_ops fhc_bus_ops = {
	BUSO_REV,
	ddi_bus_map,		/* map */
	fhc_get_intrspec,	/* get_intrspec */
	fhc_add_intrspec,	/* add_intrspec */
	fhc_remove_intrspec,	/* remove_intrspec */
	i_ddi_map_fault,	/* map_fault */
	ddi_no_dma_map,		/* dma_map */
	ddi_no_dma_allochdl,
	ddi_no_dma_freehdl,
	ddi_no_dma_bindhdl,
	ddi_no_dma_unbindhdl,
	ddi_no_dma_flush,
	ddi_no_dma_win,
	ddi_dma_mctl,		/* dma_ctl */
	fhc_ctlops,		/* ctl */
	ddi_bus_prop_op,	/* prop_op */
	0,			/* (*bus_get_eventcookie)();	*/
	0,			/* (*bus_add_eventcall)();	*/
	0,			/* (*bus_remove_eventcall)();	*/
	0			/* (*bus_post_event)();		*/
};

static struct dev_ops fhc_ops = {
	DEVO_REV,		/* rev */
	0,			/* refcnt  */
	ddi_no_info,		/* getinfo */
	fhc_identify,		/* identify */
	nulldev,		/* probe */
	fhc_attach,		/* attach */
	fhc_detach,		/* detach */
	nulldev,		/* reset */
	&no_cb_ops,		/* cb_ops */
	&fhc_bus_ops,		/* bus_ops */
	nulldev			/* power */
};

/*
 * Driver globals
 * TODO - We need to investigate what locking needs to be done here.
 */
void *fhcp;				/* fhc soft state hook */

extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
	&mod_driverops,		/* Type of module.  This one is a driver */
	"FHC Nexus",		/* Name of module. */
	&fhc_ops,		/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,		/* rev */
	(void *)&modldrv,
	NULL
};

/*
 * These are the module initialization routines.
 */

int
_init(void)
{
	int error;

	if ((error = ddi_soft_state_init(&fhcp,
	    sizeof (struct fhc_soft_state), 1)) != 0)
		return (error);

	mutex_init(&bdlist_mutex, "Board list lock",
		MUTEX_DEFAULT, DEFAULT_WT);

	mutex_init(&ftlist_mutex, "Fault list lock",
		MUTEX_DEFAULT, DEFAULT_WT);

	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	int error;

	if ((error = mod_remove(&modlinkage)) != 0)
		return (error);

	mutex_destroy(&bdlist_mutex);

	mutex_destroy(&ftlist_mutex);

	ddi_soft_state_fini(&fhcp);
	return (0);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
fhc_identify(dev_info_t *devi)
{
	char *name = ddi_get_name(devi);
	int rc = DDI_NOT_IDENTIFIED;

	if (strcmp(name, "fhc") == 0) {
		rc = DDI_IDENTIFIED;
	}

	return (rc);
}

static int
fhc_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	struct fhc_soft_state *softsp;
	int instance;
	char *board_type;
	int proplen;
	enum board_type type;

	switch (cmd) {
	case DDI_ATTACH:
		break;

	case DDI_RESUME:
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}

	instance = ddi_get_instance(devi);

	if (ddi_soft_state_zalloc(fhcp, instance) != DDI_SUCCESS)
		return (DDI_FAILURE);

	softsp = ddi_get_soft_state(fhcp, instance);

	/* Set the dip in the soft state */
	softsp->dip = devi;

	DPRINTF(FHC_ATTACH_DEBUG, ("fhc: devi= 0x%x\n, softsp=0x%x\n",
		devi, softsp));

	if (ddi_getlongprop(DDI_DEV_T_ANY, softsp->dip,
	    DDI_PROP_DONTPASS, "board-type", (caddr_t) &board_type,
	    &proplen) == DDI_PROP_SUCCESS) {
		/* match the board-type string */
		if (strcmp(CPU_BD_NAME, board_type) == 0) {
			type = CPU_BOARD;
		} else if (strcmp(MEM_BD_NAME, board_type) == 0) {
			type = MEM_BOARD;
		} else if (strcmp(IO_2SBUS_BD_NAME, board_type) == 0) {
			type = IO_2SBUS_BOARD;
		} else if (strcmp(IO_SBUS_FFB_BD_NAME, board_type) == 0) {
			type = IO_SBUS_FFB_BOARD;
		} else if (strcmp(IO_PCI_BD_NAME, board_type) == 0) {
			type = IO_PCI_BOARD;
		} else {
			type = UNKNOWN_BOARD;
		}
		kmem_free(board_type, proplen);
	} else {
		type = UNKNOWN_BOARD;
	}

	if (fhc_init(softsp, type) != DDI_SUCCESS)
		goto bad;

	mutex_init(&softsp->pokefault_mutex, "pokefault lock",
		MUTEX_SPIN_DEFAULT, (void *)ipltospl(15));

	ddi_report_dev(devi);

	return (DDI_SUCCESS);

bad:
	ddi_soft_state_free(fhcp, instance);
	return (DDI_FAILURE);
}

/* ARGSUSED */
static int
fhc_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_SUSPEND:
	case DDI_DETACH:
	default:
		return (DDI_FAILURE);
	}
}

static int
fhc_init(struct fhc_soft_state *softsp, enum board_type type)
{
	int i;
	u_int tmp_reg;
	char namebuf[128];
	int board;
	struct bd_list *list;

	/*
	 * XXX
	 * returning DDI_FAILURE without unmapping the registers can
	 * cause a kernel map leak. This should be fixed at some
	 * point in the future.
	 */

	/*
	 * Map in the FHC registers. Specifying length and offset of
	 * zero maps in the entire OBP register set.
	 */

	/* map in register set 0 */
	if (ddi_map_regs(softsp->dip, 0,
	    (caddr_t *)&softsp->id, 0, 0)) {
		cmn_err(CE_WARN, "fhc%d: unable to map internal "
			"registers", ddi_get_instance(softsp->dip));
		return (DDI_FAILURE);
	}

	/*
	 * Fill in the virtual addresses of the registers in the
	 * fhc_soft_state structure.
	 */
	softsp->rctrl = (u_int *)((char *)(softsp->id) +
		FHC_OFF_RCTRL);
	softsp->ctrl = (u_int *)((char *)(softsp->id) +
		FHC_OFF_CTRL);
	softsp->bsr = (u_int *)((char *)(softsp->id) +
		FHC_OFF_BSR);
	softsp->jtag_ctrl = (u_int *)((char *)(softsp->id) +
		FHC_OFF_JTAG_CTRL);
	softsp->jt_master.jtag_cmd = (u_int *)((char *)(softsp->id) +
		FHC_OFF_JTAG_CMD);

	/* map in register set 1 */
	if (ddi_map_regs(softsp->dip, 1,
	    (caddr_t *)&softsp->igr, 0, 0)) {
		cmn_err(CE_WARN, "fhc%d: unable to map IGR "
			"register", ddi_get_instance(softsp->dip));
		return (DDI_FAILURE);
	}

	/*
	 * map in register set 2
	 * XXX this can never be used as an interrupt generator
	 * (hardware queue overflow in fhc)
	 */
	if (ddi_map_regs(softsp->dip, 2,
	    (caddr_t *)&softsp->intr_regs[FHC_FANFAIL_INO].mapping_reg,
	    0, 0)) {
		cmn_err(CE_WARN, "fhc%d: unable to map Fan Fail "
			"IMR register", ddi_get_instance(softsp->dip));
		return (DDI_FAILURE);
	}

	/* map in register set 3 */
	if (ddi_map_regs(softsp->dip, 3,
	    (caddr_t *)&softsp->intr_regs[FHC_SYS_INO].mapping_reg,
	    0, 0)) {
		cmn_err(CE_WARN, "fhc%d: unable to map System "
			"IMR register\n", ddi_get_instance(softsp->dip));
		return (DDI_FAILURE);
	}

	/* map in register set 4 */
	if (ddi_map_regs(softsp->dip, 4,
	    (caddr_t *)&softsp->intr_regs[FHC_UART_INO].mapping_reg,
	    0, 0)) {
		cmn_err(CE_WARN, "fhc%d: unable to map UART "
			"IMR register\n", ddi_get_instance(softsp->dip));
		return (DDI_FAILURE);
	}

	/* map in register set 5 */
	if (ddi_map_regs(softsp->dip, 5,
	    (caddr_t *)&softsp->intr_regs[FHC_TOD_INO].mapping_reg,
	    0, 0)) {
		cmn_err(CE_WARN, "fhc%d: unable to map FHC TOD "
			"IMR register", ddi_get_instance(softsp->dip));
		return (DDI_FAILURE);
	}

	/* Loop over all intr sets and setup the VAs for the ISMR */
	/* TODO - Make sure we are calculating the ISMR correctly. */
	for (i = 0; i < FHC_MAX_INO; i++) {
		softsp->intr_regs[i].clear_reg =
			(u_int *)((char *)(softsp->intr_regs[i].mapping_reg) +
			FHC_OFF_ISMR);
		/* Now clear the state machines to idle */
		*(softsp->intr_regs[i].clear_reg) = ISM_IDLE;
	}

	/*
	 * It is OK to not have a OBP_BOARDNUM property. This happens for
	 * the board which is a child of central. However this FHC
	 * still needs a proper Interrupt Group Number programmed
	 * into the Interrupt Group register, because the other
	 * instance of FHC, which is not under central, will properly
	 * program the IGR. The numbers from the two settings of the
	 * IGR need to be the same. One driver cannot wait for the
	 * other to program the IGR, because there is no guarantee
	 * which instance of FHC will get attached first.
	 */
	if ((board = (int) ddi_getprop(DDI_DEV_T_ANY, softsp->dip,
	    DDI_PROP_DONTPASS, OBP_BOARDNUM, -1)) == -1) {
		/*
		 * Now determine the board number by reading the
		 * hardware register.
		 */
		board = FHC_BSR_TO_BD(*(softsp->bsr));
		softsp->is_central = 1;
	}

	/*
	 * If this fhc holds JTAG master line, and is not the central fhc,
	 * (this avoids two JTAG master nodes) then initialize the
	 * mutex and set the flag in the structure.
	 */
	if ((*(softsp->jtag_ctrl) & JTAG_MASTER_EN) && !softsp->is_central) {
		mutex_init(&(softsp->jt_master.lock), "JTAG Mutex",
			MUTEX_DEFAULT, DEFAULT_WT);
		softsp->jt_master.is_master = 1;
	} else {
		softsp->jt_master.is_master = 0;
	}

	if ((list = get_and_lock_bdlist(board)) == NULL) {
		list = bdlist_add_board(board, type, ACTIVE_STATE,
			softsp);
	} else if (softsp->is_central) {
		list = bdlist_add_board(board, CLOCK_BOARD, ACTIVE_STATE,
			softsp);
	} else {
		cmn_err(CE_PANIC, "fhc%d: Board %d duplicate database "
			"entry 0x%x", ddi_get_instance(softsp->dip),
			board, (int) list);
	}

	softsp->list = list;
	unlock_bdlist();

	/* Initialize the mutex guarding the poll_list. */
	(void) sprintf(namebuf, "fhc poll mutex softsp 0x%x", (int)softsp);
	mutex_init(&softsp->poll_list_lock, namebuf, MUTEX_DRIVER, NULL);

	/* Initialize the mutex guarding the FHC CSR */
	(void) sprintf(namebuf, "fhc csr mutex softsp 0x%x", (int)softsp);
	mutex_init(&softsp->ctrl_lock, namebuf, MUTEX_DRIVER, NULL);

	/* Initialize the poll_list to be empty */
	for (i = 0; i < MAX_ZS_CNT; i++) {
		softsp->poll_list[i].funcp = NULL;
	}

	/* Modify the various registers in the FHC now */

	/*
	 * We know this board to be present now, record that state and
	 * remove the NOT_BRD_PRES condition
	 */
	if (!(softsp->is_central)) {
		mutex_enter(&softsp->ctrl_lock);
		*(softsp->ctrl) |= FHC_NOT_BRD_PRES;
		tmp_reg = *(softsp->ctrl);
#ifdef lint
		tmp_reg = tmp_reg;
#endif
		/* XXX record the board state in global space */
		mutex_exit(&softsp->ctrl_lock);

		/* Add kstats for all non-central instances of the FHC. */
		fhc_add_kstats(softsp);
	}

	/*
	 * Read the device tree to see if this system is in an environmental
	 * chamber.
	 */
	if (temperature_chamber == -1) {
		temperature_chamber = check_for_chamber();
	}

	/* Check for inherited faults from the PROM. */
	if (*softsp->ctrl & FHC_LED_MID) {
		reg_fault(softsp->list->info.board, FT_PROM, FT_BOARD);
	}

	/*
	 * setup the IGR. Shift the board number over by one to get
	 * the UPA MID.
	 */
	*(softsp->igr) = (softsp->list->info.board) << 1;

	/* Now flush the hardware store buffers. */
	tmp_reg = *(softsp->id);
#ifdef lint
	tmp_reg = tmp_reg;
#endif

	return (DDI_SUCCESS);
}

static ddi_intrspec_t
fhc_get_intrspec(dev_info_t *dip, dev_info_t *rdip, u_int inumber)
{
	struct ddi_parent_private_data *ppdptr;

#ifdef	lint
	dip = dip;
#endif

	/*
	 * convert the parent private data pointer in the childs dev_info
	 * structure to a pointer to a sunddi_compat_hack structure
	 * to get at the interrupt specifications.
	 */
	ppdptr = (struct ddi_parent_private_data *)ddi_get_parent_data(rdip);

	/*
	 * validate the interrupt number.
	 */
	if (inumber >= ppdptr->par_nintr) {
		cmn_err(CE_WARN, "fhc%d: Inumber 0x%x is out of range of "
			"par_nintr %d", ddi_get_instance(dip),
			inumber, ppdptr->par_nintr);
		return (NULL);
	}

	/*
	 * return the interrupt structure pointer.
	 */
	return ((ddi_intrspec_t)&ppdptr->par_intr[inumber]);
}

static u_int
fhc_intr_wrapper(caddr_t arg)
{
	u_int intr_return;
	u_int tmpreg;
	struct fhc_wrapper_arg *intr_info;

	tmpreg = ISM_IDLE;
	intr_info = (struct fhc_wrapper_arg *) arg;
	intr_return = (*intr_info->funcp)(intr_info->arg);

	/* Idle the state machine. */
	*(intr_info->clear_reg) = tmpreg;

	/* Flush the hardware store buffers. */
	tmpreg = *(intr_info->clear_reg);
#ifdef lint
	tmpreg = tmpreg;
#endif	/* lint */

	return (intr_return);
}

/*
 * fhc_zs_intr_wrapper
 *
 * This function handles intrerrupts where more than one device may interupt
 * the fhc with the same mondo.
 */

#define	MAX_INTR_CNT 10

static u_int
fhc_zs_intr_wrapper(caddr_t arg)
{
	struct fhc_soft_state *softsp = (struct fhc_soft_state *) arg;
	u_int (*funcp0)(caddr_t);
	u_int (*funcp1)(caddr_t);
	caddr_t arg0, arg1;
	u_int tmp_reg;
	u_int result = DDI_INTR_UNCLAIMED;
	volatile u_int *clear_reg;
	u_char *spurious_cntr = &softsp->spurious_zs_cntr;

	funcp0 = softsp->poll_list[0].funcp;
	funcp1 = softsp->poll_list[1].funcp;
	arg0 = softsp->poll_list[0].arg;
	arg1 = softsp->poll_list[1].arg;
	clear_reg = softsp->intr_regs[FHC_UART_INO].clear_reg;

	if (funcp0 != NULL) {
		if ((funcp0)(arg0) == DDI_INTR_CLAIMED) {
			result = DDI_INTR_CLAIMED;
		}
	}

	if (funcp1 != NULL) {
		if ((funcp1)(arg1) == DDI_INTR_CLAIMED) {
			result = DDI_INTR_CLAIMED;
		}
	}

	if (result == DDI_INTR_UNCLAIMED) {
		(*spurious_cntr)++;

		if (*spurious_cntr < MAX_INTR_CNT) {
			result = DDI_INTR_CLAIMED;
		} else {
			*spurious_cntr = (u_char) 0;
		}
	} else {
		*spurious_cntr = (u_char) 0;
	}

	/* Idle the state machine. */
	*(clear_reg) = ISM_IDLE;

	/* flush the store buffers. */
	tmp_reg = *(clear_reg);

#ifdef lint
	tmp_reg = tmp_reg;
#endif

	return (result);
}


/*
 * add_intrspec - Add an interrupt specification.
 */
static int
fhc_add_intrspec(
	dev_info_t *dip,
	dev_info_t *rdip,
	ddi_intrspec_t intrspec,
	ddi_iblock_cookie_t *iblock_cookiep,
	ddi_idevice_cookie_t *idevice_cookiep,
	u_int (*int_handler)(caddr_t int_handler_arg),
	caddr_t int_handler_arg,
	int kind)
{
	int mondo;
	struct fhc_wrapper_arg *fhc_arg;
	int hot;

	ASSERT(intrspec != 0);
	ASSERT(rdip != 0);
	ASSERT(ddi_get_driver(rdip) != 0);

	if (int_handler == NULL) {
		cmn_err(CE_WARN, "fhc%d: invalid interrupt handler",
			ddi_get_instance(dip));
		return (DDI_FAILURE);
	}
	switch (kind) {
	case IDDI_INTR_TYPE_NORMAL: {
		struct fhc_soft_state *softsp = (struct fhc_soft_state *)
		    ddi_get_soft_state(fhcp, ddi_get_instance(dip));
		struct fhcintrspec *ispec =
			(struct fhcintrspec *) intrspec;
		volatile u_int *mondo_vec_reg;
		u_int tmp_mondo_vec;
		u_int tmpreg; /* HW flush reg */
		u_int cpu_id;
		struct dev_ops *dops = ddi_get_driver(rdip);

		/* Is the child MT-hot? */
		if (dops->devo_bus_ops) {
			hot = 1;	/* Nexus drivers MUST be MT-safe */
		} else if (dops->devo_cb_ops->cb_flag & D_MP) {
			hot = 1;	/* Most leaves are MT-safe */
		} else {
			hot = 0;	/* MT-unsafe drivers ok (for now) */
		}

		/* get the mondo number */
		mondo = ispec->mondo;
		mondo_vec_reg = softsp->intr_regs[FHC_INO(mondo)].
			mapping_reg;

		/* Program the iblock cookie */
		if (iblock_cookiep) {
			*iblock_cookiep = (ddi_iblock_cookie_t)
				ipltospl(ispec->pil);
		}

		/* Program the device cookie */
		if (idevice_cookiep) {
			idevice_cookiep->idev_vector = 0;
			/*
			 * The idevice cookie contains the priority as
			 * understood by the device itself on the bus it
			 * lives on.  Let the nexi beneath sort out the
			 * translation (if any) that's needed.
			 */
			idevice_cookiep->idev_priority =
				(u_short) ispec->pil;
		}

		/*
		 * Grab the global system, interrupt distribution lock
		 * here.
		 */
		mutex_enter(&intr_dist_lock);

		/*
		 * If the interrupt is for the zs chips, use the vector
		 * polling lists. Otherwise use a straight handler.
		 */
		if (FHC_INO(mondo) == FHC_UART_INO) {
			/* First lock the mutex for this poll_list */
			mutex_enter(&softsp->poll_list_lock);

			/*
			 * If polling list is empty, then install handler
			 * and enable interrupts for this mondo.
			 */
			if ((softsp->poll_list[0].funcp == NULL) &&
			    (softsp->poll_list[1].funcp == NULL)) {
				add_ivintr((u_int) ((softsp->list->info.board
					<< BD_IVINTR_SHFT)|mondo),
				    (u_int) ispec->pil, fhc_zs_intr_wrapper,
				    softsp, (hot) ? NULL : &unsafe_driver);
			}

			/*
			 * Add this interrupt to the polling list.
			 */

			/* figure out where to add this item in the list */
			if (softsp->poll_list[0].funcp == NULL) {
				softsp->poll_list[0].arg = int_handler_arg;
				softsp->poll_list[0].funcp = int_handler;
			} else if (softsp->poll_list[1].funcp == NULL) {
				softsp->poll_list[1].arg = int_handler_arg;
				softsp->poll_list[1].funcp = int_handler;
			} else {	/* two elements already in list */
				cmn_err(CE_WARN,
					"fhc%d: poll list overflow",
					ddi_get_instance(dip));
				mutex_exit(&softsp->poll_list_lock);
				mutex_exit(&intr_dist_lock);
				return (DDI_FAILURE);
			}

			/*
			 * If both zs handlers are active, then this is the
			 * second add_intrspec called, so do not enable
			 * the IMR_VALID bit, it is already on.
			 */
			if ((softsp->poll_list[0].funcp != NULL) &&
			    (softsp->poll_list[1].funcp != NULL)) {
				/* now release the mutex and return */
				mutex_exit(&softsp->poll_list_lock);
				mutex_exit(&intr_dist_lock);
				return (DDI_SUCCESS);
			} else {
				/* just release the nutex */
				mutex_exit(&softsp->poll_list_lock);
			}
		} else {	/* normal interrupt installation */
			/* Allocate a nexus interrupt data structure */
			fhc_arg = (struct fhc_wrapper_arg *) kmem_alloc(
				sizeof (struct fhc_wrapper_arg), KM_SLEEP);
			fhc_arg->child = ispec->child;
			fhc_arg->mapping_reg = mondo_vec_reg;
			fhc_arg->clear_reg =
				(softsp->intr_regs[FHC_INO(mondo)].clear_reg);
			fhc_arg->softsp = softsp;
			fhc_arg->funcp = int_handler;
			fhc_arg->arg = int_handler_arg;

			/*
			 * Save the fhc_arg in the ispec so we can use this info
			 * later to uninstall this interrupt spec.
			 */
			ispec->handler_arg = fhc_arg;
			add_ivintr((u_int) ((softsp->list->info.board <<
				BD_IVINTR_SHFT) | mondo),
				(u_int) ispec->pil, fhc_intr_wrapper,
				fhc_arg, (hot) ? NULL : &unsafe_driver);
		}

		/*
		 * Clear out a stale 'pending' or 'transmit' state in
		 * this device's ISM that might have been left from a
		 * previous session.
		 *
		 * Since all FHC interrupts are level interrupts, any
		 * real interrupting condition will immediately transition
		 * the ISM back to pending.
		 */
		*(softsp->intr_regs[FHC_INO(mondo)].clear_reg) = ISM_IDLE;

		/*
		 * Program the mondo vector accordingly.  This MUST be the
		 * last thing we do.  Once we program the mondo, the device
		 * may begin to interrupt.
		 */
		cpu_id = intr_add_cpu(fhc_intrdist, (void *) dip,
			(softsp->list->info.board << BD_IVINTR_SHFT) |
			mondo, 0);

		tmp_mondo_vec = cpu_id << INR_PID_SHIFT;

		/* don't do this for fan because fan has a special control */
		if (FHC_INO(mondo) == FHC_FANFAIL_INO)
			cmn_err(CE_PANIC, "fhc%d: enabling fanfail interrupt",
			    ddi_get_instance(dip));
		else
			tmp_mondo_vec |= IMR_VALID;

		DPRINTF(FHC_INTERRUPT_DEBUG,
		    ("Mondo 0x%x mapping reg: 0x%x", mondo_vec_reg));

		/* Store it in the hardware reg. */
		*mondo_vec_reg = tmp_mondo_vec;

		/* Read a FHC register to flush store buffers */
		tmpreg = *(softsp->id);
#ifdef lint
		tmpreg = tmpreg;
#endif
		/* release the interrupt distribution lock */
		mutex_exit(&intr_dist_lock);

		return (DDI_SUCCESS);
	}
	default:
		/*
		 * If we can't do it here, our parent can't either, so
		 * fail the request.
		 */
		cmn_err(CE_WARN, "fhc%d: fhc_addintrspec() unknown request",
			ddi_get_instance(dip));
		return (DDI_INTR_NOTFOUND);
	}
}

/*
 * remove_intrspec - Remove an interrupt specification.
 */
static void
fhc_remove_intrspec(
	dev_info_t *dip,
	dev_info_t *rdip,
	ddi_intrspec_t intrspec,
	ddi_iblock_cookie_t iblock_cookiep)
{
	volatile u_int *mondo_vec_reg;
	volatile u_int tmpreg;
	int i;

	struct fhcintrspec *ispec = (struct fhcintrspec *) intrspec;
	struct fhc_wrapper_arg *arg = ispec->handler_arg;
	struct fhc_soft_state *softsp = (struct fhc_soft_state *)
		ddi_get_soft_state(fhcp, ddi_get_instance(dip));
	int mondo;

#ifdef lint
	rdip = rdip;
	iblock_cookiep = iblock_cookiep;
#endif
	mondo = ispec->mondo;

	mutex_enter(&intr_dist_lock);

	if (FHC_INO(mondo) == FHC_UART_INO) {
		int intr_found = 0;

		/* Lock the poll_list first */
		mutex_enter(&softsp->poll_list_lock);

		/*
		 * Find which entry in the poll list belongs to this
		 * intrspec.
		 */
		for (i = 0; i < MAX_ZS_CNT; i++) {
			if (softsp->poll_list[i].funcp ==
			    ispec->handler_arg->funcp) {
				softsp->poll_list[i].funcp = NULL;
				intr_found++;
			}
		}

		/* If we did not find an entry, then we have a problem */
		if (!intr_found) {
			cmn_err(CE_WARN, "fhc%d: Intrspec not found in"
				" poll list", ddi_get_instance(dip));
			mutex_exit(&softsp->poll_list_lock);
			mutex_exit(&intr_dist_lock);
			return;
		}

		/*
		 * If we have removed all active entries for the poll
		 * list, then we have to disable interupts at this point.
		 */
		if ((softsp->poll_list[0].funcp == NULL) &&
		    (softsp->poll_list[1].funcp == NULL)) {
			mondo_vec_reg = softsp->intr_regs[FHC_UART_INO].
				mapping_reg;
			*mondo_vec_reg &= ~IMR_VALID;

			intr_rem_cpu((softsp->list->info.board <<
				BD_IVINTR_SHFT) | mondo);

			/* flush the hardware buffers */
			tmpreg = *(softsp->ctrl);

			/* Eliminate the particular handler from the system. */
			rem_ivintr((softsp->list->info.board <<
				BD_IVINTR_SHFT) | mondo,
				(struct intr_vector *)NULL);
		}

		mutex_exit(&softsp->poll_list_lock);
	} else {

		mondo_vec_reg = arg->mapping_reg;

		/* Turn off the valid bit in the mapping register. */
		/* XXX what about FHC_FANFAIL owned imr? */
		*mondo_vec_reg &= ~IMR_VALID;

		intr_rem_cpu((softsp->list->info.board <<
			BD_IVINTR_SHFT) | mondo);

		/* flush the hardware store buffers */
		tmpreg = *(softsp->id);
#ifdef lint
		tmpreg = tmpreg;
#endif

		/* Eliminate the particular handler from the system. */
		rem_ivintr((softsp->list->info.board << BD_IVINTR_SHFT) |
			mondo, (struct intr_vector *)NULL);

		kmem_free(ispec->handler_arg, sizeof (struct fhc_wrapper_arg));
	}
	mutex_exit(&intr_dist_lock);

	ispec->handler_arg = (struct fhc_wrapper_arg *) 0;
}

static void
fhc_uninit_child(dev_info_t *child)
{
	struct ddi_parent_private_data *pdptr;
	int n;

	/*
	 * strip out any interrupt info. This was built by the
	 * fhc driver and is not understood by generic DDI
	 * routines.
	 */
	if ((pdptr = (struct ddi_parent_private_data *)
	    ddi_get_parent_data(child)) != NULL)  {
		if ((n = (size_t)pdptr->par_nintr) != 0) {
			kmem_free(pdptr->par_intr, n *
				sizeof (struct fhcintrspec));
			pdptr->par_nintr = 0;
			pdptr->par_intr = NULL;
		}
	}
	/* now call the generic DDI routine. */
	impl_ddi_sunbus_removechild(child);
}

/*
 * FHC Control Ops routine
 *
 * Requests handled here:
 *	DDI_CTLOPS_INITCHILD	see impl_ddi_sunbus_initchild() for details
 *	DDI_CTLOPS_UNINITCHILD	see fhc_uninit_child() for details
 *	DDI_CTLOPS_XLATE_INTRS	see fhc_ctl_xlate_intrs() for details
 *	DDI_CTLOPS_REPORTDEV	TODO - need to implement this.
 *	DDI_CTLOPS_INTR_HILEVEL
 *	DDI_CTLOPS_POKE_INIT	TODO - need to remove this support later
 *	DDI_CTLOPS_POKE_FLUSH	TODO - need to remove this support later
 *	DDI_CTLOPS_POKE_FINI	TODO - need to remove this support later
 */
static int
fhc_ctlops(dev_info_t *dip, dev_info_t *rdip,
	ddi_ctl_enum_t op, void *arg, void *result)
{
	struct fhc_soft_state *softsp = (struct fhc_soft_state *)
		ddi_get_soft_state(fhcp, ddi_get_instance(dip));

	switch (op) {
	case DDI_CTLOPS_INITCHILD:
		DPRINTF(FHC_CTLOPS_DEBUG, ("DDI_CTLOPS_INITCHILD\n"));
		return (impl_ddi_sunbus_initchild((dev_info_t *)arg));

	case DDI_CTLOPS_UNINITCHILD:
		DPRINTF(FHC_CTLOPS_DEBUG, ("DDI_CTLOPS_UNINITCHILD\n"));
		fhc_uninit_child((dev_info_t *)arg);
		return (DDI_SUCCESS);

	case DDI_CTLOPS_XLATE_INTRS:
		DPRINTF(FHC_CTLOPS_DEBUG, ("DDI_CTLOPS_XLATE_INTRS\n"));
		return (fhc_ctl_xlate_intrs(dip, rdip, arg, result));

	case DDI_CTLOPS_REPORTDEV:
		/*
		 * TODO - Figure out what makes sense to report here.
		 */
		return (DDI_SUCCESS);

	case DDI_CTLOPS_INTR_HILEVEL:
		/*
		 * Indicate whether the interrupt specified is to be handled
		 * above lock level.  In other words, above the level that
		 * cv_signal and default type mutexes can be used.
		 *
		 * TODO - We should call a kernel specific routine to
		 * determine LOCK_LEVEL.
		 */
		*(int *) result =
			(((struct fhcintrspec *)arg)->pil > LOCK_LEVEL);
		return (DDI_SUCCESS);

	case DDI_CTLOPS_POKE_INIT:
		mutex_enter(&softsp->pokefault_mutex);
		softsp->pokefault = -1;
		return (DDI_SUCCESS);

	case DDI_CTLOPS_POKE_FLUSH:

		/*
		 * TODO - Figure out which AC to check to see that we have
		 * not issued a write to a non-replying UPA address. Then
		 * read this AC's error register and checkl if a fault
		 * occurred. On Sunfire hardware, this kind of write
		 * fails silently, and is only recorded in the AC's
		 * error register.
		 */
		return (softsp->pokefault == 1 ? DDI_FAILURE : DDI_SUCCESS);

	case DDI_CTLOPS_POKE_FINI:
		softsp->pokefault = 0;
		mutex_exit(&softsp->pokefault_mutex);
		return (DDI_SUCCESS);

	default:
		return (ddi_ctlops(dip, rdip, op, arg, result));
	}
}


/*
 * We're prepared to claim that the interrupt string is in
 * the form of a list of <FHCintr> specifications, or we're dealing
 * with on-board devices and we have an interrupt_number property which
 * gives us our mondo number.
 * Translate the mondos into fhcintrspecs.
 */
/* ARGSUSED */
static int
fhc_ctl_xlate_intrs(dev_info_t *dip, dev_info_t *rdip, int *in,
	struct ddi_parent_private_data *pdptr)
{
	register int n;
	register size_t size;
	register struct fhcintrspec *new;

	/*
	 * The list consists of <mondo interrupt level> elements
	 */
	if ((n = *in++) < 1)
		return (DDI_FAILURE);

	pdptr->par_nintr = n;
	size = n * sizeof (struct fhcintrspec);
	new = kmem_zalloc(size, KM_SLEEP);
	pdptr->par_intr = (struct intrspec *) new;

	while (n--) {
		int level = *in++, mondo;

		/*
		 * Create the FHC mondo number. Devices will have
		 * an "interrupts" property, that is equal to the mondo number.
		 */
		mondo = level;

		/* Sanity check the mondos range */
		if (FHC_INO(mondo) >= FHC_MAX_INO) {
			cmn_err(CE_WARN, "fhc%d: Mondo vector 0x%x out "
				"of range",
				ddi_get_instance(dip), mondo);
			goto broken;
		}

		new->mondo = mondo;
		new->pil = fhc_int_priorities[FHC_INO(mondo)];
		new->child = rdip;
		DPRINTF(FHC_INTERRUPT_DEBUG, ("Interrupt info for device %s"
		    "Mondo: 0x%x, Pil: 0x%x, 0x%x\n",
		    ddi_get_name(rdip), new->mondo, new->pil));
		new++;
	}

	return (DDI_SUCCESS);
	/*NOTREACHED*/

broken:
	cmn_err(CE_WARN, "fhc%d: fhc_ctl_xlate_intrs() failed",
		ddi_get_instance(dip));
	kmem_free(pdptr->par_intr, size);
	pdptr->par_intr = (void *)0;
	pdptr->par_nintr = 0;
	return (DDI_FAILURE);
}

/*
 * This function initializes the temperature arrays for use. All
 * temperatures are set in to invalid value to start.
 */
void
init_temp_arrays(struct temp_stats *envstat)
{
	int i;

	envstat->index = 0;

	for (i = 0; i < L1_SZ; i++) {
		envstat->l1[i] = NA_TEMP;
	}

	for (i = 0; i < L2_SZ; i++) {
		envstat->l2[i] = NA_TEMP;
	}

	for (i = 0; i < L3_SZ; i++) {
		envstat->l3[i] = NA_TEMP;
	}

	for (i = 0; i < L4_SZ; i++) {
		envstat->l4[i] = NA_TEMP;
	}

	for (i = 0; i < L5_SZ; i++) {
		envstat->l5[i] = NA_TEMP;
	}

	envstat->max = NA_TEMP;
	envstat->min = NA_TEMP;
	envstat->trend = TREND_UNKNOWN;
	envstat->version = TEMP_KSTAT_VERSION;
}

/*
 * This function manages the temperature history in the temperature
 * statistics buffer passed in. It calls the temperature calibration
 * routines and maintains the time averaged temperature data.
 */
void
update_temp(dev_info_t pdip, struct temp_stats *envstat, u_char value)
{
	u_int index;		    /* The absolute temperature counter */
	u_int tmp_index;	    /* temp index into upper level array */
	int count;		    /* Count of non-zero values in array */
	int total;		    /* sum total of non-zero values in array */
	short real_temp;	    /* calibrated temperature */
	int i;
	struct fhc_soft_state *softsp;
	char buffer[256];	    /* buffer for warning of overtemp */
	enum temp_state temp_state; /* Temperature state */
	/*
	 * NOTE: This global counter is not protected since we're called
	 * serially for each board.
	 */
	static int shutdown_msg = 0; /* Flag if shutdown warning issued */

	/* determine soft state pointer of parent */
	softsp = ddi_get_soft_state(fhcp, ddi_get_instance(pdip));

	envstat->index++;
	index = envstat->index;

	/*
	 * You need to update the level 5 intervals first, since
	 * they are based on the data from the level 4 intervals,
	 * and so on, down to the level 1 intervals.
	 */

	/* update the level 5 intervals if it is time */
	if (((tmp_index = L5_INDEX(index)) > 0) && (L5_REM(index) == 0)) {
		/* Generate the index within the level 5 array */
		tmp_index -= 1;		/* decrement by 1 for indexing */
		tmp_index = tmp_index % L5_SZ;

		/* take an average of the level 4 array */
		for (i = 0, count = 0, total = 0; i < L4_SZ; i++) {
			/* Do not include zero values in average */
			if (envstat->l4[i] != NA_TEMP) {
				total += (int) envstat->l4[i];
				count++;
			}
		}

		/*
		 * If there were any level 4 data points to average,
		 * do so.
		 */
		if (count != 0) {
			envstat->l5[tmp_index] = total/count;
		} else {
			envstat->l5[tmp_index] = NA_TEMP;
		}
	}

	/* update the level 4 intervals if it is time */
	if (((tmp_index = L4_INDEX(index)) > 0) && (L4_REM(index) == 0)) {
		/* Generate the index within the level 4 array */
		tmp_index -= 1;		/* decrement by 1 for indexing */
		tmp_index = tmp_index % L4_SZ;

		/* take an average of the level 3 array */
		for (i = 0, count = 0, total = 0; i < L3_SZ; i++) {
			/* Do not include zero values in average */
			if (envstat->l3[i] != NA_TEMP) {
				total += (int) envstat->l3[i];
				count++;
			}
		}

		/*
		 * If there were any level 3 data points to average,
		 * do so.
		 */
		if (count != 0) {
			envstat->l4[tmp_index] = total/count;
		} else {
			envstat->l4[tmp_index] = NA_TEMP;
		}
	}

	/* update the level 3 intervals if it is time */
	if (((tmp_index = L3_INDEX(index)) > 0) && (L3_REM(index) == 0)) {
		/* Generate the index within the level 3 array */
		tmp_index -= 1;		/* decrement by 1 for indexing */
		tmp_index = tmp_index % L3_SZ;

		/* take an average of the level 2 array */
		for (i = 0, count = 0, total = 0; i < L2_SZ; i++) {
			/* Do not include zero values in average */
			if (envstat->l2[i] != NA_TEMP) {
				total += (int) envstat->l2[i];
				count++;
			}
		}

		/*
		 * If there were any level 2 data points to average,
		 * do so.
		 */
		if (count != 0) {
			envstat->l3[tmp_index] = total/count;
		} else {
			envstat->l3[tmp_index] = NA_TEMP;
		}
	}

	/* update the level 2 intervals if it is time */
	if (((tmp_index = L2_INDEX(index)) > 0) && (L2_REM(index) == 0)) {
		/* Generate the index within the level 2 array */
		tmp_index -= 1;		/* decrement by 1 for indexing */
		tmp_index = tmp_index % L2_SZ;

		/* take an average of the level 1 array */
		for (i = 0, count = 0, total = 0; i < L1_SZ; i++) {
			/* Do not include zero values in average */
			if (envstat->l1[i] != NA_TEMP) {
				total += (int) envstat->l1[i];
				count++;
			}
		}

		/*
		 * If there were any level 1 data points to average,
		 * do so.
		 */
		if (count != 0) {
			envstat->l2[tmp_index] = total/count;
		} else {
			envstat->l2[tmp_index] = NA_TEMP;
		}
	}

	/* Run the calibration function using this board type */
	real_temp = calibrate_temp(softsp->list->info.type, value,
		softsp->list->info.ac_compid);

	envstat->l1[index % L1_SZ] = real_temp;

	/* check if the temperature state for this device needs to change */
	temp_state = get_temp_state(softsp->list->info.type, real_temp);

	/* has the state changed? Then get the board string ready */
	if (temp_state != envstat->state) {
		int board = softsp->list->info.board;
		enum board_type type = softsp->list->info.type;

		build_bd_display_str(buffer, type, board);

		if (temp_state > envstat->state) {
			if (envstat->state == TEMP_OK) {
				if (type == CLOCK_BOARD) {
					reg_fault(0, FT_OVERTEMP, FT_SYSTEM);
				} else {
					reg_fault(board, FT_OVERTEMP,
						FT_BOARD);
				}
			}

			/* heating up, change state now */
			envstat->temp_cnt = 0;
			envstat->state = temp_state;

			/* now warn the user of the problem */
			if (temp_state == TEMP_WARN) {
				cmn_err(CE_WARN,
					"%s is warm (temperature: %dC). "
					"Please check system cooling", buffer,
					real_temp);
			} else if (temp_state == TEMP_DANGER) {
				cmn_err(CE_WARN,
					"%s is very hot (temperature: %dC)",
					buffer, real_temp);
				envstat->shutdown_cnt = 1;
				if (temperature_chamber == -1)
					temperature_chamber =
						check_for_chamber();
				if ((temperature_chamber == 0) &&
				    enable_overtemp_powerdown) {
					/*
					 * NOTE: The "%d seconds" is not
					 * necessarily accurate in the case
					 * where we have multiple boards
					 * overheating and subsequently cooling
					 * down.
					 */
					if (shutdown_msg == 0) {
						cmn_err(CE_WARN, "System "
							"shutdown scheduled "
							"in %d seconds due to "
							"over-temperature "
							"condition on %s",
							SHUTDOWN_TIMEOUT_SEC,
							buffer);
					}
					shutdown_msg++;
				}
			}
		} else if (temp_state < envstat->state) {
			/*
			 * Avert the sigpower that would
			 * otherwise be sent to init.
			 */
			envstat->shutdown_cnt = 0;

			/* cooling down, use state counter */
			if (envstat->temp_cnt == 0) {
				envstat->temp_cnt = TEMP_STATE_COUNT;
			} else if (--envstat->temp_cnt == 0) {
				if (temp_state == TEMP_WARN) {
					cmn_err(CE_NOTE,
						"%s is cooling "
						"(temperature: %dC)", buffer,
						real_temp);
				} else if (temp_state == TEMP_OK) {
					cmn_err(CE_NOTE,
						"%s has cooled down "
						"(temperature: %dC), system OK",
						buffer, real_temp);
					if (type == CLOCK_BOARD) {
						clear_fault(0, FT_OVERTEMP,
							FT_SYSTEM);
					} else {
						clear_fault(board, FT_OVERTEMP,
							FT_BOARD);
					}
				}

				/*
				 * If we just came out of TEMP_DANGER, and
				 * a warning was issued about shutting down,
				 * let the user know it's been cancelled
				 */
				if (envstat->state == TEMP_DANGER &&
				    (temperature_chamber == 0) &&
				    enable_overtemp_powerdown &&
				    (powerdown_started == 0) &&
				    (--shutdown_msg == 0)) {
					cmn_err(CE_NOTE, "System "
						"shutdown due to over-"
						"temperature "
						"condition cancelled");
				}

				envstat->state = temp_state;
			}
		}
	} else {
		envstat->temp_cnt = 0;

		if (temp_state == TEMP_DANGER) {
			if (temperature_chamber == -1) {
				temperature_chamber = check_for_chamber();
			}

			if ((envstat->shutdown_cnt++ >= SHUTDOWN_COUNT) &&
			    (temperature_chamber == 0) &&
			    enable_overtemp_powerdown &&
			    (powerdown_started == 0)) {
				powerdown_started = 1;

				/* the system is still too hot */
				build_bd_display_str(buffer,
					softsp->list->info.type,
					softsp->list->info.board);

				cmn_err(CE_WARN, "%s still too hot "
					"(temperature: %dC)."
					" Overtemp shutdown started", buffer,
					real_temp);

				fhc_reboot();
			}
		}
	}

	/* update the maximum and minimum temperatures if necessary */
	if ((envstat->max == NA_TEMP) || (real_temp > envstat->max)) {
		envstat->max = real_temp;
	}

	if ((envstat->min == NA_TEMP) || (real_temp < envstat->min)) {
		envstat->min = real_temp;
	}

	/*
	 * Update the temperature trend.  Currently, the temperature
	 * trend algorithm is based on the level 2 stats.  So, we
	 * only need to run every time the level 2 stats get updated.
	 */
	if (((tmp_index = L2_INDEX(index)) > 0) && (L2_REM(index) == 0))  {
		envstat->trend = temp_trend(envstat);

		/* Issue a warning if the temperature is rising rapidly. */
		if (envstat->trend == TREND_RAPID_RISE)  {
			int board = softsp->list->info.board;
			enum board_type type = softsp->list->info.type;

			build_bd_display_str(buffer, type, board);
			cmn_err(CE_WARN, "%s temperature is rising rapidly!  "
				"Current temperature is %dC", buffer,
				real_temp);
		}
	}
}

#define	PREV_L2_INDEX(x)    ((x) ? ((x) - 1) : (L2_SZ - 1))

/*
 * This routine determines if the temp of the device passed in is heating
 * up, cooling down, or staying stable.
 */
enum temp_trend
temp_trend(struct temp_stats *tempstat)
{
	int		ii;
	u_int		curr_index;
	int		curr_temp;
	u_int		prev_index;
	int		prev_temp;
	int		trail_temp;
	int		delta;
	int		read_cnt;
	enum temp_trend	result = TREND_STABLE;

	if (tempstat == NULL)
		return (TREND_UNKNOWN);

	curr_index = (L2_INDEX(tempstat->index) - 1) % L2_SZ;
	curr_temp = tempstat->l2[curr_index];

	/* Count how many temperature readings are available */
	prev_index = curr_index;
	for (read_cnt = 0; read_cnt < L2_SZ - 1; read_cnt++) {
		if (tempstat->l2[prev_index] == NA_TEMP)
			break;
		prev_index = PREV_L2_INDEX(prev_index);
	}

	switch (read_cnt) {
	case 0:
	case 1:
		result = TREND_UNKNOWN;
		break;

	default:
		delta = curr_temp - tempstat->l2[PREV_L2_INDEX(curr_index)];
		prev_index = curr_index;
		trail_temp = prev_temp = curr_temp;
		if (delta >= RAPID_RISE_THRESH) {	    /* rapid rise? */
			result = TREND_RAPID_RISE;
		} else if (delta > 0) {			    /* rise? */
			for (ii = 1; ii < read_cnt; ii++) {
				prev_index = PREV_L2_INDEX(prev_index);
				prev_temp = tempstat->l2[prev_index];
				if (prev_temp > trail_temp) {
					break;
				}
				trail_temp = prev_temp;
				if (prev_temp <= curr_temp - NOISE_THRESH) {
					result = TREND_RISE;
					break;
				}
			}
		} else if (delta <= -RAPID_FALL_THRESH) {   /* rapid fall? */
			result = TREND_RAPID_FALL;
		} else if (delta < 0) {			    /* fall? */
			for (ii = 1; ii < read_cnt; ii++) {
				prev_index = PREV_L2_INDEX(prev_index);
				prev_temp = tempstat->l2[prev_index];
				if (prev_temp < trail_temp) {
					break;
				}
				trail_temp = prev_temp;
				if (prev_temp >= curr_temp + NOISE_THRESH) {
					result = TREND_FALL;
					break;
				}
			}
		}
	}
	return (result);
}

/*
 * Reboot the system if we can, otherwise attempt a power down
 */
void
fhc_reboot()
{
	proc_t *initpp;

	/* send a SIGPWR to init process */
	mutex_enter(&pidlock);
	initpp = prfind(P_INITPID);
	mutex_exit(&pidlock);

	/*
	 * If we're still booting and init(1) isn't
	 * set up yet, simply halt.
	 */
	if (initpp != NULL) {
		psignal(initpp, SIGFPE);	/* init 6 */
	} else {
		power_down("Environmental Shutdown");
		halt("Power off the System");
	}
}

int
overtemp_kstat_update(kstat_t *ksp, int rw)
{
	struct temp_stats *tempstat;
	char *kstatp;
	int i;

	kstatp = (char *) ksp->ks_data;
	tempstat = (struct temp_stats *) ksp->ks_private;

	/*
	 * Kstat reads are used to retrieve the current system temperature
	 * history. Kstat writes are used to reset the max and min
	 * temperatures.
	 */
	if (rw == KSTAT_WRITE) {
		short max;	/* temporary copy of max temperature */
		short min;	/* temporary copy of min temperature */

		/*
		 * search for and reset the max and min to the current
		 * array contents. Old max and min values will get
		 * averaged out as they move into the higher level arrays.
		 */
		max = tempstat->l1[0];
		min = tempstat->l1[0];

		/* Pull the max and min from Level 1 array */
		for (i = 0; i < L1_SZ; i++) {
			if ((tempstat->l1[i] != NA_TEMP) &&
			    (tempstat->l1[i] > max)) {
				max = tempstat->l1[i];
			}

			if ((tempstat->l1[i] != NA_TEMP) &&
			    (tempstat->l1[i] < min)) {
				min = tempstat->l1[i];
			}
		}

		/* Pull the max and min from Level 2 array */
		for (i = 0; i < L2_SZ; i++) {
			if ((tempstat->l2[i] != NA_TEMP) &&
			    (tempstat->l2[i] > max)) {
				max = tempstat->l2[i];
			}

			if ((tempstat->l2[i] != NA_TEMP) &&
			    (tempstat->l2[i] < min)) {
				min = tempstat->l2[i];
			}
		}

		/* Pull the max and min from Level 3 array */
		for (i = 0; i < L3_SZ; i++) {
			if ((tempstat->l3[i] != NA_TEMP) &&
			    (tempstat->l3[i] > max)) {
				max = tempstat->l3[i];
			}

			if ((tempstat->l3[i] != NA_TEMP) &&
			    (tempstat->l3[i] < min)) {
				min = tempstat->l3[i];
			}
		}

		/* Pull the max and min from Level 4 array */
		for (i = 0; i < L4_SZ; i++) {
			if ((tempstat->l4[i] != NA_TEMP) &&
			    (tempstat->l4[i] > max)) {
				max = tempstat->l4[i];
			}

			if ((tempstat->l4[i] != NA_TEMP) &&
			    (tempstat->l4[i] < min)) {
				min = tempstat->l4[i];
			}
		}

		/* Pull the max and min from Level 5 array */
		for (i = 0; i < L5_SZ; i++) {
			if ((tempstat->l5[i] != NA_TEMP) &&
			    (tempstat->l5[i] > max)) {
				max = tempstat->l5[i];
			}

			if ((tempstat->l5[i] != NA_TEMP) &&
			    (tempstat->l5[i] < min)) {
				min = tempstat->l5[i];
			}
		}
	} else {
		/*
		 * copy the temperature history buffer into the
		 * kstat structure.
		 */
		bcopy((caddr_t) tempstat, kstatp, sizeof (struct temp_stats));
	}
	return (0);
}

/*
 * This function uses the calibration tables at the beginning of this file
 * to lookup the actual temperature of the thermistor in degrees Celcius.
 * If the measurement is out of the bounds of the acceptable values, the
 * closest boundary value is used instead.
 */
static short
calibrate_temp(enum board_type type, u_char temp, u_int ac_comp)
{
	short result = NA_TEMP;

	if (dont_calibrate == 1) {
		return ((short) temp);
	}

	switch (type) {
	case CPU_BOARD:
		/*
		 * If AC chip revision is >= 4 or if it is unitialized,
		 * then use the new calibration tables.
		 */
		if ((CHIP_REV(ac_comp) >= 4) || (CHIP_REV(ac_comp) == 0)) {
			if (temp >= CPU2_MX_CNT) {
				result = cpu2_table[CPU2_MX_CNT-1];
			} else {
				result = cpu2_table[temp];
			}
		} else {
			if (temp >= CPU_MX_CNT) {
				result = cpu_table[CPU_MX_CNT-1];
			} else {
				result = cpu_table[temp];
			}
		}
		break;

	case IO_2SBUS_BOARD:
	case IO_SBUS_FFB_BOARD:
	case IO_PCI_BOARD:
		if (temp < IO_MN_CNT) {
			result = io_table[IO_MN_CNT];
		} else if (temp >= IO_MX_CNT) {
			result = io_table[IO_MX_CNT-1];
		} else {
			result = io_table[temp];
		}
		break;

	case CLOCK_BOARD:
		if (temp < CLK_MN_CNT) {
			result = clock_table[CLK_MN_CNT];
		} else if (temp >= CLK_MX_CNT) {
			result = clock_table[CLK_MX_CNT-1];
		} else {
			result = clock_table[temp];
		}
		break;

	default:
		break;
	}

	return (result);
}

/*
 * Determine the temperature state of this board based on its type and
 * the actual temperature in degrees Celcius.
 */
static enum temp_state
get_temp_state(enum board_type type, short temp)
{
	enum temp_state state = TEMP_OK;
	short warn_limit;
	short danger_limit;

	switch (type) {
	case CPU_BOARD:
		warn_limit = cpu_warn_temp;
		danger_limit = cpu_danger_temp;
		break;

	case IO_2SBUS_BOARD:
	case IO_SBUS_FFB_BOARD:
	case IO_PCI_BOARD:
		warn_limit = io_warn_temp;
		danger_limit = io_danger_temp;
		break;

	case CLOCK_BOARD:
		warn_limit = clk_warn_temp;
		danger_limit = clk_danger_temp;
		break;

	case UNINIT_BOARD:
	case UNKNOWN_BOARD:
	case MEM_BOARD:
	default:
		warn_limit = dft_warn_temp;
		danger_limit = dft_danger_temp;
		break;
	}

	if (temp >= danger_limit) {
		state = TEMP_DANGER;
	} else if (temp >= warn_limit) {
		state = TEMP_WARN;
	}

	return (state);
}

static void
fhc_add_kstats(struct fhc_soft_state *softsp)
{
	struct kstat *fhc_ksp;
	struct fhc_kstat *fhc_named_ksp;

	if ((fhc_ksp = kstat_create("unix", softsp->list->info.board,
	    FHC_KSTAT_NAME, "misc", KSTAT_TYPE_NAMED,
	    sizeof (struct fhc_kstat) / sizeof (kstat_named_t),
	    KSTAT_FLAG_PERSISTENT)) == NULL) {
		cmn_err(CE_WARN, "fhc%d kstat_create failed",
			ddi_get_instance(softsp->dip));
		return;
	}

	fhc_named_ksp = (struct fhc_kstat *)(fhc_ksp->ks_data);

	/* initialize the named kstats */
	kstat_named_init(&fhc_named_ksp->csr,
		CSR_KSTAT_NAMED,
		KSTAT_DATA_ULONG);

	kstat_named_init(&fhc_named_ksp->bsr,
		BSR_KSTAT_NAMED,
		KSTAT_DATA_ULONG);

	fhc_ksp->ks_update = fhc_kstat_update;
	fhc_ksp->ks_private = (void *)softsp;
	kstat_install(fhc_ksp);
}

static int
fhc_kstat_update(kstat_t *ksp, int rw)
{
	struct fhc_kstat *fhcksp;
	struct fhc_soft_state *softsp;

	fhcksp = (struct fhc_kstat *) ksp->ks_data;
	softsp = (struct fhc_soft_state *) ksp->ks_private;

	/* this is a read-only kstat. Bail out on a write */
	if (rw == KSTAT_WRITE) {
		return (EACCES);
	} else {
		/*
		 * copy the current state of the hardware into the
		 * kstat structure.
		 */
		fhcksp->csr.value.ul = *softsp->ctrl;
		fhcksp->bsr.value.ul = *softsp->bsr;
	}
	return (0);
}

static int
cpu_on_board(int board)
{
	int upa_a = board << 1;
	int upa_b = (board << 1) + 1;

	if ((cpunodes[upa_a].nodeid != NULL) ||
	    (cpunodes[upa_b].nodeid != NULL)) {
		return (1);
	} else {
		return (0);
	}
}

/*
 * bdlist_add_board()
 *
 * This is the generic add board to the list function. It should be
 * called by any routine that wants to add a board to the board list.
 * It is assumed that the requested board does not exist, and that the
 * caller has already checked this.
 */
struct bd_list *
bdlist_add_board(int board, enum board_type type, enum board_state state,
	struct fhc_soft_state *softsp)
{
	struct bd_list *list;	/* temporary list pointer */

	ASSERT(mutex_owned(&bdlist_mutex));

	/*
	 * Allocate a new board list element and link it into
	 * the list.
	 */
	if ((list = (struct bd_list *) kmem_zalloc(sizeof (struct bd_list),
	    KM_SLEEP)) == NULL) {
		cmn_err(CE_WARN, "fhc: unable to allocate "
			"board %d list structure", board);
		return (NULL);
	}

	/* fill in the standard board list elements */
	list->info.board = board;
	list->info.state = state;
	list->softsp = softsp;

	/* if the board type is indeterminate, it must be determined */
	if (type == UNKNOWN_BOARD) {
		if (softsp != NULL) {
			/*
			 * If the softsp is known, use the UPA64 bits from
			 * the FHC. This is not the best solution since we
			 * cannot fully type the IO boards.
			 */
			if (softsp->is_central == 1) {
				list->info.type = CLOCK_BOARD;
			} else if (cpu_on_board(board)) {
				list->info.type = CPU_BOARD;
			} else if ((*(softsp->bsr) & FHC_UPADATA64A) ||
			    (*(softsp->bsr) & FHC_UPADATA64B)) {
				list->info.type = IO_2SBUS_BOARD;
			} else {
				list->info.type = MEM_BOARD;
			}
		} else {
			/*
			 * If both means fail, then leave the board type
			 * unknown.
			 */
			list->info.type = UNKNOWN_BOARD;
		}
	} else {
		list->info.type = type;
	}

	/* now link the new board into the list. */
	list->next = bd_list;
	bd_list = list;

	return (list);
}

void
bdlist_free_board(struct bd_list *board)
{
	struct bd_list *list = bd_list;
	struct bd_list **vect = &bd_list;

	ASSERT(mutex_owned(&bdlist_mutex));
	ASSERT(board != NULL);

	/* find the board in the board list */
	for (list = bd_list, vect = &bd_list; list != NULL;
	    vect = &list->next, list = list->next) {
		if (list == board) {
			/* remove the item from the list */
			*vect = list->next;

			/*
			 * XXX - delete soft state instance here? This could be
			 * done by calling fhc_detach to do most of the work.
			 */
			kmem_free(board, sizeof (struct bd_list));
			return;
		}
	}
	cmn_err(CE_PANIC, "Could not find requested bd_list to delete");
}

/*
 * This function searches the board list database and returns a pointer
 * to the selected structure if it is found or NULL if it isn't.
 * The database is _always_ left in a locked state so that a subsequent
 * update can occur atomically.
 */
struct bd_list *
get_and_lock_bdlist(int board)
{
	ASSERT(!mutex_owned(&bdlist_mutex));
	mutex_enter(&bdlist_mutex);

	return (get_bdlist(board));
}

/*
 * Return the next element on the list. If the input is NULL, the function
 * returns NULL.
 */
struct bd_list *
get_next_bdlist(struct bd_list *list)
{
	ASSERT(mutex_owned(&bdlist_mutex));

	if (list != NULL) {
		list = list->next;
	}

	return (list);
}

/*
 * Search for the bd_list entry for the specified board.
 * Passing in a -1 for the board argument means to access the
 * first element in the board list.
 */
struct bd_list *
get_bdlist(int board)
{
	struct bd_list *list = bd_list;


	ASSERT(mutex_owned(&bdlist_mutex));
	while (list != NULL) {
		if (board == -1) {
			return (list);
		}

		/* skip the clock board */
		if (list->info.type == CLOCK_BOARD) {
			list = list->next;
			continue;
		}

		/* see if the board number matches. */
		if (list->info.board == board) {
			break;
		}
		list = list->next;
	}
	return (list);
}

/* unlock the database */
void
unlock_bdlist(void)
{
	ASSERT(mutex_owned(&bdlist_mutex));
	mutex_exit(&bdlist_mutex);
}

/*
 * return the type of a board based on its board number
 */
enum board_type
get_board_type(int board)
{
	struct bd_list *list;
	enum board_type type = -1;

	if ((list = get_and_lock_bdlist(board)) != NULL) {
		type = list->info.type;
	}
	unlock_bdlist();

	return (type);
}

/*
 * find_and_lock_jtag_master()
 *
 * This routine searches thru the current board list and if it finds
 * an fhc that holds the hardware JTAG master line, it locks the
 * mutex and returns a pointer to the jtag master structure, which
 * contains a pointer to the hardware register and the mutex
 * structure itself. If no JTAG master can be found, it returns NULL.
 *
 * This routine assumes that the board list is locked upon entry,
 * and there is an ASSERT for this early on.
 */
struct jt_mstr *
find_and_lock_jtag_master(void)
{
	struct bd_list *bd_list;
	struct jt_mstr *master = NULL;

	ASSERT(mutex_owned(&bdlist_mutex));

	/*
	 * Now search for the JTAG master and place the addresses for
	 * command into the sysctrl soft state structure.
	 */
	for (bd_list = get_bdlist(-1); bd_list != NULL;
	    bd_list = get_next_bdlist(bd_list)) {
		if (bd_list->softsp == NULL) {
			continue;
		}

		if (bd_list->softsp->jt_master.is_master == 1) {
			master = &bd_list->softsp->jt_master;
			mutex_enter(&master->lock);
			break;
		}
	}
	return (master);
}

/*
 * release_jtag_master()
 *
 *
 */
void
release_jtag_master(struct jt_mstr *mstr)
{
	ASSERT(mutex_owned(&bdlist_mutex));
	mutex_exit(&mstr->lock);
}

/*
 * This function uses the board list and toggles the OS green board
 * LED. The mask input tells which bit fields are being modified,
 * and the value input tells the states of the bits.
 */
void
update_board_leds(struct bd_list *board, u_int mask, u_int value)
{
	u_int temp;

	/* mask off mask and value for only the LED bits */
	mask &= (FHC_LED_LEFT|FHC_LED_MID|FHC_LED_RIGHT);
	value &= (FHC_LED_LEFT|FHC_LED_MID|FHC_LED_RIGHT);

	if (board != NULL) {
		mutex_enter(&board->softsp->ctrl_lock);

		/* read the current register state */
		temp = *board->softsp->ctrl;

		/* mask off the bits to change */
		temp &= ~mask;

		/* or in the new values of the bits. */
		temp |= value;

		/* update the register */
		*board->softsp->ctrl = temp;

		mutex_exit(&board->softsp->ctrl_lock);
	}
}

static int
check_for_chamber(void)
{
	int chamber = 0;
	dev_info_t *options_dip;
	dnode_t options_node_id;
	int mfgmode_len;
	int retval;
	char *mfgmode;

	/*
	 * The operator can disable overtemp powerdown from /etc/system or
	 * boot -h.
	 */
	if (!enable_overtemp_powerdown) {
		cmn_err(CE_WARN, "Operator has disabled overtemp powerdown");
		return (1);

	}

	/*
	 * An OBP option, 'mfg-mode' is being used to inform us as to
	 * whether we are in an enviromental chamber. It exists in
	 * the 'options' node. This is where all OBP 'setenv' (eeprom)
	 * parameters live.
	 */
	if ((options_dip = ddi_find_devinfo("options", -1, 0)) != NULL) {
		options_node_id = (dnode_t) ddi_get_nodeid(options_dip);
		mfgmode_len = prom_getproplen(options_node_id, "mfg-mode");
		if (mfgmode_len == -1) {
			return (chamber);
		}

		if ((mfgmode = kmem_alloc(mfgmode_len+1, KM_SLEEP)) ==
		    NULL) {
			return (chamber);
		}

		retval = prom_getprop(options_node_id, "mfg-mode", mfgmode);
		if (retval != -1) {
			mfgmode[retval] = 0;
			if (strcmp(mfgmode, CHAMBER_VALUE) == 0) {
				chamber = 1;
				cmn_err(CE_WARN, "System in Temperature"
					" Chamber Mode. Overtemperature"
					" Shutdown disabled");
			}
		}
		kmem_free(mfgmode, mfgmode_len+1);
	}
	return (chamber);
}

static void
build_bd_display_str(char *buffer, enum board_type type, int board)
{
	if (buffer == NULL) {
		return;
	}

	/* fill in board type to display */
	switch (type) {
	case UNINIT_BOARD:
		sprintf(buffer, "Uninitialized Board type board %d", board);
		break;

	case UNKNOWN_BOARD:
		sprintf(buffer, "Unknown Board type board %d", board);
		break;

	case CPU_BOARD:
	case MEM_BOARD:
		sprintf(buffer, "CPU/Memory board %d", board);
		break;

	case IO_2SBUS_BOARD:
		sprintf(buffer, "2 SBus IO board %d", board);
		break;

	case IO_SBUS_FFB_BOARD:
		sprintf(buffer, "SBus FFB IO board %d", board);
		break;

	case IO_PCI_BOARD:
		sprintf(buffer, "PCI IO board %d", board);
		break;

	case CLOCK_BOARD:
		sprintf(buffer, "Clock board");
		break;

	default:
		sprintf(buffer, "Unrecognized board type board %d",
			board);
		break;
	}
}

void
fhc_intrdist(void *arg, int mondo, u_int cpu_id)
{
	struct fhc_soft_state *softsp;
	dev_info_t *dip = (dev_info_t *) arg;
	volatile u_int *mondo_vec_reg;
	volatile u_int *intr_state_reg;
	u_int mondo_vec;
	u_int tmp_reg;

	ASSERT(MUTEX_HELD(&intr_dist_lock));

	/* extract the soft state pointer */
	softsp = ddi_get_soft_state(fhcp, ddi_get_instance(dip));

	mondo_vec_reg = softsp->intr_regs[FHC_INO(mondo)].mapping_reg;
	intr_state_reg = softsp->intr_regs[FHC_INO(mondo)].clear_reg;

	/* Check the current target of the mondo */
	if (((*mondo_vec_reg & INR_PID_MASK) >> INR_PID_SHIFT) == cpu_id) {
		/* It is the same, don't reprogram */
		return;
	}

	/* So it's OK to reprogram the CPU target */

	/* turn off the valid bit */
	*mondo_vec_reg &= ~IMR_VALID;

	/* flush the hardware registers */
	tmp_reg = *softsp->id;

	/*
	 * wait for the state machine to idle. Do not loop on panic, so
	 * that system does not hang.
	 */
	while (((*intr_state_reg & INT_PENDING) == INT_PENDING) && !panicstr);

	/* re-target the mondo and turn it on */
	mondo_vec = (cpu_id << INR_PID_SHIFT) | IMR_VALID;

	/* write it back to the hardware. */
	*mondo_vec_reg = mondo_vec;

	/* flush the hardware buffers. */
	tmp_reg = *(softsp->id);

#ifdef	lint
	tmp_reg = tmp_reg;
#endif	/* lint */
}

/*
 * reg_fault
 *
 * This routine registers a fault in the fault list. If the fault
 * is unique (does not exist in fault list) then a new fault is
 * added to the fault list, with the appropriate structure elements
 * filled in.
 */
void
reg_fault(int unit, enum ft_type type, enum ft_class class)
{
	struct ft_list *list;	/* temporary list pointer */

	if (type >= ft_max_index) {
		cmn_err(CE_WARN, "Illegal Fault type %x", type);
		return;
	}

	mutex_enter(&ftlist_mutex);

	/* Search for the requested fault. If it already exists, return. */
	for (list = ft_list; list != NULL; list = list->next) {
		if ((list->unit == unit) && (list->type == type) &&
		    (list->class == class)) {
			mutex_exit(&ftlist_mutex);
			return;
		}
	}

	/* Allocate a new fault structure. */
	if ((list = (struct ft_list *) kmem_zalloc(sizeof (struct ft_list),
	    KM_SLEEP)) == NULL) {
		cmn_err(CE_WARN, "fhc: unable to allocate "
			"unit %d fault list structure", unit);
		mutex_exit(&ftlist_mutex);
		return;
	}

	/* fill in the fault list elements */
	list->unit = unit;
	list->type = type;
	list->class = class;
	list->create_time = hrestime.tv_sec;
	strncpy(list->msg, ft_str_table[type], MAX_FT_DESC);

	/* link it into the list. */
	list->next = ft_list;
	ft_list = list;

	/* Update the total fault count */
	ft_nfaults++;

	mutex_exit(&ftlist_mutex);
}

/*
 * clear_fault
 *
 * This routine finds the fault list entry specified by the caller,
 * deletes it from the fault list, and frees up the memory used for
 * the entry. If the requested fault is not found, it exits silently.
 */
void
clear_fault(int unit, enum ft_type type, enum ft_class class)
{
	struct ft_list *list;		/* temporary list pointer */
	struct ft_list **vect;

	mutex_enter(&ftlist_mutex);

	list = ft_list;
	vect = &ft_list;

	/*
	 * Search for the requested fault. If it exists, delete it
	 * and relink the fault list.
	 */
	for (; list != NULL; vect = &list->next, list = list->next) {
		if ((list->unit == unit) && (list->type == type) &&
		    (list->class == class)) {
			/* remove the item from the list */
			*vect = list->next;

			/* free the memory allocated */
			kmem_free(list, sizeof (struct ft_list));

			/* Update the total fault count */
			ft_nfaults--;
			break;
		}
	}
	mutex_exit(&ftlist_mutex);
}

/*
 * process_fault_list
 *
 * This routine walks the global fault list and updates the board list
 * with the current status of each Yellow LED. If any faults are found
 * in the system, then a non-zero value is returned. Else zero is returned.
 */
int
process_fault_list(void)
{
	int fault = 0;
	struct ft_list *ftlist;		/* fault list pointer */
	struct bd_list *bdlist;		/* board list pointer */

	/*
	 * Note on locking. The bdlist mutex is always acquired and
	 * held around the ftlist mutex when both are needed for an
	 * operation. This is to avoid deadlock.
	 */

	/* First lock the board list */
	bdlist = get_and_lock_bdlist(-1);

	/* Grab the fault list lock first */
	mutex_enter(&ftlist_mutex);

	/* clear the board list of all faults first */
	for (; bdlist != NULL; bdlist = get_next_bdlist(bdlist)) {
		bdlist->fault = 0;
	}

	/* walk the fault list here */
	for (ftlist = ft_list; ftlist != NULL; ftlist = ftlist->next) {
		fault++;

		/*
		 * If this is a board level fault, find the board, The
		 * unit number for all board class faults must be the
		 * actual board number. The caller of reg_fault must
		 * ensure this for FT_BOARD class faults.
		 */
		if (ftlist->class == FT_BOARD) {
			bdlist = get_bdlist(ftlist->unit);

			/* Sanity check the pointer first */
			if (bdlist != NULL) {
				bdlist->fault = 1;
			} else {
				cmn_err(CE_WARN, "No board %d list entry found",
					ftlist->unit);
			}
		}
	}

	/* now unlock the fault list */
	mutex_exit(&ftlist_mutex);

	/* unlock the board list before leaving */
	unlock_bdlist();

	return (fault);
}

/*
 * Creates a variable sized virtual kstat with a snapshot routine in order
 * to pass the linked list fault list up to userland. Also creates a
 * virtual kstat to pass up the string table for faults.
 */
void
create_ft_kstats(int instance)
{
	struct kstat *ksp;

	ksp = kstat_create("unix", instance, FT_LIST_KSTAT_NAME, "misc",
		KSTAT_TYPE_RAW, 1, KSTAT_FLAG_VIRTUAL|KSTAT_FLAG_VAR_SIZE);

	if (ksp != NULL) {
		ksp->ks_data = NULL;
		ksp->ks_update = ft_ks_update;
		ksp->ks_snapshot = ft_ks_snapshot;
		ksp->ks_data_size = 1;
		ksp->ks_lock = &ftlist_mutex;
		kstat_install(ksp);
	}
}

/*
 * This routine creates a snapshot of all the fault list data. It is
 * called by the kstat framework when a kstat read is done.
 */
static int
ft_ks_snapshot(struct kstat *ksp, void *buf, int rw)
{
	struct ft_list *ftlist;

	if (rw == KSTAT_WRITE) {
		return (EACCES);
	}

	ksp->ks_snaptime = gethrtime();

	for (ftlist = ft_list; ftlist != NULL; ftlist = ftlist->next) {
		bcopy((char *) ftlist, (char *) buf,
			sizeof (struct ft_list));
		buf = ((struct ft_list *) buf) + 1;
	}
	return (0);
}

/*
 * Setup the kstat data size for the kstat framework. This is used in
 * conjunction with the ks_snapshot routine. This routine sets the size,
 * the kstat framework allocates the memory, and ks_shapshot does the
 * data transfer.
 */
static int
ft_ks_update(struct kstat *ksp, int rw)
{
	if (rw == KSTAT_WRITE) {
		return (EACCES);
	} else {
		if (ft_nfaults) {
			ksp->ks_data_size = ft_nfaults *
				sizeof (struct ft_list);
		} else {
			ksp->ks_data_size = 1;
		}
	}

	return (0);
}
