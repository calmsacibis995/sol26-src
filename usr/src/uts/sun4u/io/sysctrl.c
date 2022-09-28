/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sysctrl.c 1.51	96/10/17 SMI"

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/obpdefs.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/sysmacros.h>
#include <sys/ivintr.h>
#include <sys/autoconf.h>
#include <sys/intreg.h>
#include <sys/proc.h>
#include <sys/modctl.h>
#include <sys/fhc.h>
#include <sys/sysctrl.h>
#include <sys/jtag.h>
#include <sys/clock.h>
#include <sys/promif.h>

/* Useful debugging Stuff */
#include <sys/nexusdebug.h>


#ifndef	FALSE
#define	FALSE	0
#endif
#ifndef	TRUE
#define	TRUE	1
#endif

#define	HOTPLUG_DISABLED_PROPERTY "hotplug-disabled"

/*
 * Function prototypes
 */
static int sysctrl_identify(dev_info_t *devi);

static int sysctrl_attach(dev_info_t *devi, ddi_attach_cmd_t cmd);

static int sysctrl_detach(dev_info_t *devi, ddi_detach_cmd_t cmd);

static u_int system_high_handler(caddr_t arg);

static u_int spur_delay(caddr_t arg);

static void spur_retry(caddr_t arg);

static u_int spur_reenable(caddr_t arg);

static void spur_long_timeout(caddr_t arg);

static u_int spur_clear_count(caddr_t arg);

static u_int ac_fail_handler(caddr_t arg);

static void ac_fail_retry(caddr_t arg);

static u_int ac_fail_reenable(caddr_t arg);

static u_int ps_fail_int_handler(caddr_t arg);

static u_int ps_fail_poll_handler(caddr_t arg);

static u_int ps_fail_handler(struct sysctrl_soft_state * softsp, int fromint);

static enum power_state compute_power_state(struct sysctrl_soft_state *softsp);

static void ps_log_state_change(struct sysctrl_soft_state * softsp,
					int index, int present);

static void ps_log_pres_change(struct sysctrl_soft_state * softsp,
					int index, int present);

static void ps_fail_retry(caddr_t arg);

static u_int pps_fanfail_handler(caddr_t arg);

static void pps_fanfail_retry(caddr_t arg);

static u_int pps_fanfail_reenable(caddr_t arg);

static void pps_fan_poll(caddr_t arg);

static void pps_fan_state_change(struct sysctrl_soft_state * softsp,
					int index, int fan_ok);

static u_int bd_insert_handler(caddr_t arg);

static void bd_insert_timeout(caddr_t arg);

static void bd_remove_timeout(caddr_t arg);

static u_int bd_insert_normal(caddr_t arg);

static void preload_bd_list(struct sysctrl_soft_state *);

static void sysctrl_add_kstats(struct sysctrl_soft_state *softsp);

static int sysctrl_kstat_update(kstat_t *ksp, int rw);

static int psstat_kstat_update(kstat_t *, int);

static void init_remote_console_uart(struct sysctrl_soft_state *);

static void blink_led_timeout(caddr_t arg);

static u_int blink_led_handler(caddr_t arg);

static char *get_board_typestr(enum board_type);

static void sysctrl_thread_wakeup(int type);

static void sysctrl_overtemp_poll(void);

static void sysctrl_keyswitch_poll(void);

static void update_key_state(struct sysctrl_soft_state *);

static void sysctrl_abort_seq_handler(char *msg);

static void nvram_update_powerfail(struct sysctrl_soft_state * softsp);

void toggle_board_green_leds(int);

static void set_cpu_speeds(struct bd_info *, u_int);

/*
 * Configuration data structures
 */
static struct cb_ops sysctrl_cb_ops = {
	nulldev,		/* open */
	nulldev,		/* close */
	nulldev,		/* strategy */
	nulldev,		/* print */
	nulldev,		/* dump */
	nulldev,		/* read */
	nulldev,		/* write */
	nulldev,		/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev,			/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,		/* cb_prop_op */
	0,			/* streamtab */
	D_MP|D_NEW		/* Driver compatibility flag */
};

static struct dev_ops sysctrl_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* refcnt */
	ddi_no_info,		/* getinfo */
	sysctrl_identify,	/* identify */
	nulldev,		/* probe */
	sysctrl_attach,		/* attach */
	sysctrl_detach,		/* detach */
	nulldev,		/* reset */
	&sysctrl_cb_ops,	/* cb_ops */
	(struct bus_ops *)0,	/* bus_ops */
	nulldev			/* power */
};

void *sysctrlp;				/* sysctrl soft state hook */

/* # of secs to silence spurious interrupts */
static int spur_timeout_hz;

/* # of secs to count spurious interrupts to print message */
static int spur_long_timeout_hz;

/* # of secs between AC failure polling */
static int ac_timeout_hz;

/* # of secs between Power Supply Failure polling */
static int ps_fail_timeout_hz;

/*
 * # of secs between Peripheral Power Supply failure polling
 * (used both for interrupt retry timeout and polling function)
 */
static int pps_fan_timeout_hz;

/* # of secs delay after board insert interrupt */
static int bd_insert_delay_hz;

/* # of secs to wait before restarting poll if we cannot clear interrupts */
static int bd_insert_retry_hz;

/* # of secs between Board Removal polling */
static int bd_remove_timeout_hz;

/* # of secs between toggle of OS LED */
static int blink_led_timeout_hz;

/* overtemp polling routine timeout delay */
static int overtemp_timeout_hz;

/* key switch polling routine timeout delay */
static int keyswitch_timeout_hz;

/* Specify which system interrupt condition to monitor */
int enable_sys_interrupt = SYS_AC_PWR_FAIL_EN | SYS_PPS_FAN_FAIL_EN |
			SYS_PS_FAIL_EN | SYS_SBRD_PRES_EN;

/* Should the overtemp_poll thread be running? */
static int sysctrl_do_overtemp_thread = 1;

/* Should the keyswitch_poll thread be running? */
static int sysctrl_do_keyswitch_thread = 1;

/*
 * This timeout ID is for hotplug board remove polling routine. It is
 * protected by the bd_list mutex.
 * XXX - This will not work for wildfire. A different scheme must be
 * used since there will be multiple sysctrl nodes, each with its
 * own list of hotplugged boards to scan.
 */
static int hp_to_id = 0;

int enable_remote_console_reset = 0;

/*
 * If this is set, the system will not shutdown when insufficient power
 * condition persists.
 */
int disable_insufficient_power_reboot = 0;

/* Indicates whether or not the overtemp thread has been started */
static int sysctrl_overtemp_thread_started = 0;

/* Indicates whether or not the key switch thread has been started */
static int sysctrl_keyswitch_thread_started = 0;

/* *Mutex used to protect the soft state list */
static kmutex_t sslist_mutex;

/* The CV is used to wakeup the overtemp thread when needed. */
static kcondvar_t overtemp_cv;

/* The CV is used to wakeup the key switch thread when needed. */
static kcondvar_t keyswitch_cv;

/*
 * Linked list of all syctrl soft state structures.
 * Used for polling sysctrl state changes, i.e. temperature.
 */
struct sysctrl_soft_state *sys_list = NULL;


extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
	&mod_driverops,		/* Type of module.  This one is a driver */
	"Clock Board",		/* name of module */
	&sysctrl_ops,		/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,		/* rev */
	(void *)&modldrv,
	NULL
};

#ifndef lint
static char _depends_on[] = "drv/fhc";
#endif  /* lint */

/*
 * These are the module initialization routines.
 */

int
_init(void)
{
	int error;

	if ((error = ddi_soft_state_init(&sysctrlp,
	    sizeof (struct sysctrl_soft_state), 1)) != 0)
		return (error);

	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	int error;

	if ((error = mod_remove(&modlinkage)) != 0)
		return (error);

	ddi_soft_state_fini(&sysctrlp);
	return (0);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
sysctrl_identify(dev_info_t *devi)
{
	char *name = ddi_get_name(devi);
	int rc = DDI_NOT_IDENTIFIED;

	/*
	 * Our device name is kind of wierd here. 'clock-board' would not
	 * be a good name for a device driver, so we call it system
	 * control, or 'sysctrl'.
	 */
	if ((strcmp(name, "clock-board") == 0) ||
	    (strcmp(name, "sysctrl") == 0)) {
		rc = DDI_IDENTIFIED;
	}

	return (rc);
}

static int
sysctrl_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	struct sysctrl_soft_state *softsp;
	int instance;
	u_char tmp_reg;
	char namebuf[128];
	dev_info_t *dip;
	char *propval;
	int proplen;

	switch (cmd) {
	case DDI_ATTACH:
		break;

	case DDI_RESUME:
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}

	instance = ddi_get_instance(devi);

	if (ddi_soft_state_zalloc(sysctrlp, instance) != DDI_SUCCESS)
		return (DDI_FAILURE);

	softsp = ddi_get_soft_state(sysctrlp, instance);

	/* Set the dip in the soft state */
	softsp->dip = devi;

	/* Set up the parent dip */
	softsp->pdip = ddi_get_parent(softsp->dip);

	DPRINTF(SYSCTRL_ATTACH_DEBUG, ("sysctrl: devi= 0x%x\n, softsp=0x%x\n",
		devi, softsp));

	/* First set all of the timeout values */
	spur_timeout_hz = drv_usectohz(SPUR_TIMEOUT_USEC);
	spur_long_timeout_hz = drv_usectohz(SPUR_LONG_TIMEOUT_USEC);
	ac_timeout_hz = drv_usectohz(AC_TIMEOUT_USEC);
	ps_fail_timeout_hz = drv_usectohz(PS_FAIL_TIMEOUT_USEC);
	pps_fan_timeout_hz = drv_usectohz(PPS_FAN_TIMEOUT_USEC);
	bd_insert_delay_hz = drv_usectohz(BRD_INSERT_DELAY_USEC);
	bd_insert_retry_hz = drv_usectohz(BRD_INSERT_RETRY_USEC);
	bd_remove_timeout_hz = drv_usectohz(BRD_REMOVE_TIMEOUT_USEC);
	blink_led_timeout_hz = drv_usectohz(BLINK_LED_TIMEOUT_USEC);
	overtemp_timeout_hz = drv_usectohz(OVERTEMP_TIMEOUT_SEC * MICROSEC);
	keyswitch_timeout_hz = drv_usectohz(KEYSWITCH_TIMEOUT_USEC);

	/*
	 * Map in the registers sets that OBP hands us. According
	 * to the sun4u device tree spec., the register sets are as
	 * follows:
	 *
	 *	0	Clock Frequency Registers (contains the bit
	 *		for enabling the remote console reset)
	 *	1	misc (has all the registers that we need
	 */
	if (ddi_map_regs(softsp->dip, 0,
	    (caddr_t *)&softsp->clk_freq1, 0, 0)) {
		cmn_err(CE_WARN, "sysctrl%d: unable to map clock frequency "
			"registers", instance);
		goto bad;
	}

	if (ddi_map_regs(softsp->dip, 1,
	    (caddr_t *)&softsp->csr, 0, 0)) {
		cmn_err(CE_WARN, "sysctrl%d: unable to map internal"
			"registers", instance);
		goto bad;
	}

	/*
	 * Fill in the virtual addresses of the registers in the
	 * sysctrl_soft_state structure. We do not want to calculate
	 * them on the fly. This way we waste a little memory, but
	 * avoid bugs down the road.
	 */
	softsp->clk_freq2 = (u_char *) ((caddr_t)softsp->clk_freq1 +
		SYS_OFF_CLK_FREQ2);

	softsp->status1 = (u_char *) ((caddr_t)softsp->csr +
		SYS_OFF_STAT1);

	softsp->status2 = (u_char *) ((caddr_t)softsp->csr +
		SYS_OFF_STAT2);

	softsp->ps_stat = (u_char *) ((caddr_t)softsp->csr +
		SYS_OFF_PSSTAT);

	softsp->ps_pres = (u_char *) ((caddr_t)softsp->csr +
		SYS_OFF_PSPRES);

	softsp->pppsr = (u_char *) ((caddr_t)softsp->csr +
		SYS_OFF_PPPSR);

	softsp->temp_reg = (u_char *) ((caddr_t)softsp->csr +
		SYS_OFF_TEMP);

	/*
	 * Enable the hardware watchdog gate on the clock board if
	 * map_wellknown has kicked off the timer.
	 */
	if (watchdog_activated)
		*(softsp->clk_freq2) |= TOD_RESET_EN;
	else
		*(softsp->clk_freq2) &= ~TOD_RESET_EN;

	/* Check for inherited faults from the PROM. */
	if (*softsp->csr & SYS_LED_MID) {
		reg_fault(0, FT_PROM, FT_SYSTEM);
	}

	/*
	 * cache the number of slots on this system
	 */
	softsp->nslots = *softsp->status1;

	/*
	 * Map in and configure the remote console UART if it is not
	 * already enabled by the PROM.
	 */
	if ((enable_remote_console_reset != 0) &&
	    ((*(softsp->clk_freq2) & RCONS_UART_EN) == 0)) {
		/*
		 * There is no OBP register set for the remote console UART,
		 * so offset from the last register set, the misc register
		 * set, in order to map in the remote console UART.
		 */
		if (ddi_map_regs(softsp->dip, 1, (caddr_t *)&softsp->rcons_ctl,
		    RMT_CONS_OFFSET, RMT_CONS_LEN)) {
			cmn_err(CE_NOTE, "sysctrl%d: unable to map remote "
				"console UART registers", instance);
		} else {
			/*
			 * Program the UART to watch ttya console.
			 */
			init_remote_console_uart(softsp);

			/* Now enable the remote console reset control bits. */
			*(softsp->clk_freq2) |= RCONS_UART_EN;

			/* print warning if OS enabled remote console */
			cmn_err(CE_WARN,
				"sysctrl%d: Remote Console Reset Enabled",
				instance);

			/* flush the hardware buffers */
			tmp_reg = *(softsp->csr);
		}
	}

	/* create the fault list kstat */
	create_ft_kstats(instance);

	/*
	 * Do a priming read on the ADC, and throw away the first value
	 * read. This is a feature of the ADC hardware. After a power cycle
	 * it does not contains valid data until a read occurs.
	 */
	tmp_reg = *(softsp->temp_reg);

	/* Wait 30 usec for ADC hardware to stabilize. */
	DELAY(30);

	/* shut off all interrupt sources */
	*(softsp->csr) &= ~(SYS_PPS_FAN_FAIL_EN | SYS_PS_FAIL_EN |
				SYS_AC_PWR_FAIL_EN | SYS_SBRD_PRES_EN);
	tmp_reg = *(softsp->csr);
#ifdef lint
	tmp_reg = tmp_reg;
#endif

	/*
	 * Now register our high interrupt with the system.
	 */
	if (ddi_add_intr(devi, 0, &softsp->iblock,
	    &softsp->idevice, (u_int (*)(caddr_t))nulldev, NULL) !=
	    DDI_SUCCESS)
		goto bad;

	(void) sprintf(namebuf, "sysctrl high mutex softsp 0x%0x",
		(int)softsp);
	mutex_init(&softsp->csr_mutex, namebuf, MUTEX_DRIVER,
		(void *)softsp->iblock);

	ddi_remove_intr(devi, 0, softsp->iblock);

	if (ddi_add_intr(devi, 0, &softsp->iblock,
	    &softsp->idevice, system_high_handler, (caddr_t) softsp) !=
	    DDI_SUCCESS)
		goto bad;

	if (ddi_add_softintr(devi, DDI_SOFTINT_LOW, &softsp->spur_id,
	    &softsp->spur_int_c, NULL, spur_delay, (caddr_t) softsp) !=
	    DDI_SUCCESS)
		goto bad;

	(void) sprintf(namebuf, "sysctrl spur int mutex softsp 0x%0x",
		(int)softsp);
	mutex_init(&softsp->spur_int_lock, namebuf, MUTEX_DRIVER,
		(void *)softsp->spur_int_c);


	if (ddi_add_softintr(devi, DDI_SOFTINT_LOW, &softsp->spur_high_id,
	    NULL, NULL, spur_reenable, (caddr_t) softsp) != DDI_SUCCESS)
		goto bad;

	if (ddi_add_softintr(devi, DDI_SOFTINT_LOW, &softsp->spur_long_to_id,
	    NULL, NULL, spur_clear_count, (caddr_t) softsp) != DDI_SUCCESS)
		goto bad;

	/*
	 * Now register low-level ac fail handler
	 */
	if (ddi_add_softintr(devi, DDI_SOFTINT_HIGH, &softsp->ac_fail_id,
	    NULL, NULL, ac_fail_handler, (caddr_t)softsp) != DDI_SUCCESS)
		goto bad;

	if (ddi_add_softintr(devi, DDI_SOFTINT_LOW, &softsp->ac_fail_high_id,
	    NULL, NULL, ac_fail_reenable, (caddr_t)softsp) != DDI_SUCCESS)
		goto bad;

	/*
	 * Now register low-level ps fail handler
	 */

	if (ddi_add_softintr(devi, DDI_SOFTINT_HIGH, &softsp->ps_fail_int_id,
	    &softsp->ps_fail_c, NULL, ps_fail_int_handler, (caddr_t)softsp) !=
	    DDI_SUCCESS)
		goto bad;

	(void) sprintf(namebuf, "sysctrl ps fail mutex softsp 0x%0x",
		(int)softsp);
	mutex_init(&softsp->ps_fail_lock, namebuf, MUTEX_DRIVER,
		(void *)softsp->ps_fail_c);

	if (ddi_add_softintr(devi, DDI_SOFTINT_LOW, &softsp->ps_fail_poll_id,
	    NULL, NULL, ps_fail_poll_handler, (caddr_t)softsp) !=
	    DDI_SUCCESS)
		goto bad;

	/*
	 * Now register low-level pps fan fail handler
	 */
	if (ddi_add_softintr(devi, DDI_SOFTINT_LOW, &softsp->pps_fan_id,
	    NULL, NULL, pps_fanfail_handler, (caddr_t)softsp) !=
	    DDI_SUCCESS)
		goto bad;

	if (ddi_add_softintr(devi, DDI_SOFTINT_LOW, &softsp->pps_fan_high_id,
	    NULL, NULL, pps_fanfail_reenable, (caddr_t)softsp) !=
	    DDI_SUCCESS)
		goto bad;

	/*
	 * Based upon a check for a current share backplane, advise
	 * that system does not support hot plug
	 *
	 */
	if ((*(softsp->pppsr) & SYS_NOT_CURRENT_S) != 0) {
		cmn_err(CE_NOTE, "Hot Plug not supported in this system");
	}

	/*
	 * If the trigger circuit is busted or the NOT_BRD_PRES line
	 * is stuck then OBP will publish this property stating that
	 * hot plug is not available.  If this happens we will complain
	 * to the console and register a system fault.  We will also
	 * not enable the board insert interrupt for this session.
	 */
	if (ddi_prop_op(DDI_DEV_T_ANY, softsp->dip, PROP_LEN_AND_VAL_ALLOC,
	    DDI_PROP_DONTPASS, HOTPLUG_DISABLED_PROPERTY,
	    (caddr_t)&propval, &proplen) == DDI_PROP_SUCCESS) {
		cmn_err(CE_WARN, "Hot Plug Unavailable [%s]", propval);
		reg_fault(0, FT_HOT_PLUG, FT_SYSTEM);
		enable_sys_interrupt &= ~SYS_SBRD_PRES_EN;
		kmem_free(propval, proplen);
	}

	/*
	 * Call the following function to preload the diabled boar
	 * list and any disk boards in the device tree.
	 */
	preload_bd_list(softsp);

	/*
	 * call the bd_insert_handler to look for disabled boards in
	 * the system before enabling the interrupt.
	 */
	bd_insert_timeout((caddr_t) softsp);

	/*
	 * Now register low-level board insert handler
	 */
	if (ddi_add_softintr(devi, DDI_SOFTINT_LOW, &softsp->sbrd_pres_id,
	    NULL, NULL, bd_insert_handler, (caddr_t)softsp) != DDI_SUCCESS)
		goto bad;

	if (ddi_add_softintr(devi, DDI_SOFTINT_LOW, &softsp->sbrd_gone_id,
	    NULL, NULL, bd_insert_normal, (caddr_t)softsp) != DDI_SUCCESS)
		goto bad;

	/*
	 * Now register led blink handler (interrupt level)
	 */
	if (ddi_add_softintr(devi, DDI_SOFTINT_LOW, &softsp->blink_led_id,
	    &softsp->sys_led_c, NULL, blink_led_handler, (caddr_t)softsp) !=
	    DDI_SUCCESS)
		goto bad;
	(void) sprintf(namebuf, "sysctrl System OS LED mutex softsp 0x%x",
		(int)softsp);
	mutex_init(&softsp->sys_led_lock, namebuf, MUTEX_DRIVER,
		(void *)softsp->sys_led_c);

	/* initialize the bit field for all pps fans to assumed good */
	softsp->pps_fan_saved = softsp->pps_fan_external_state =
		SYS_AC_FAN_OK | SYS_KEYSW_FAN_OK;

	/* prime the power supply state machines */
	if (enable_sys_interrupt & SYS_PS_FAIL_EN)
		ddi_trigger_softintr(softsp->ps_fail_poll_id);

	/* kick off the OS led blinker */
	softsp->sys_led = FALSE;
	ddi_trigger_softintr(softsp->blink_led_id);

	/* Now enable selected interrupt sources */
	mutex_enter(&softsp->csr_mutex);
	*(softsp->csr) |= enable_sys_interrupt &
		(SYS_AC_PWR_FAIL_EN | SYS_PS_FAIL_EN |
		SYS_PPS_FAN_FAIL_EN | SYS_SBRD_PRES_EN);
	tmp_reg = *(softsp->csr);
#ifdef lint
	tmp_reg = tmp_reg;
#endif
	mutex_exit(&softsp->csr_mutex);

	/* Initialize the temperature */
	init_temp_arrays(&softsp->tempstat);

	/*
	 * initialize key switch shadow state
	 */
	softsp->key_shadow = KEY_BOOT;

	/*
	 * Now add this soft state structure to the front of the linked list
	 * of soft state structures.
	 */
	if (sys_list == (struct sysctrl_soft_state *)NULL) {
		mutex_init(&sslist_mutex, "Soft State List Mutex",
			MUTEX_DEFAULT, DEFAULT_WT);
	}
	mutex_enter(&sslist_mutex);
	softsp->next = sys_list;
	sys_list = softsp;
	mutex_exit(&sslist_mutex);

	/* Setup the kstats for this device */
	sysctrl_add_kstats(softsp);

	/* kick off the PPS fan poll routine */
	pps_fan_poll((caddr_t)softsp);

	if (sysctrl_overtemp_thread_started == 0) {
		/*
		 * set up the overtemp condition variable before
		 * starting the thread.
		 */
		cv_init(&overtemp_cv, "Overtemp CV", CV_DRIVER, NULL);

		/*
		 * start up the overtemp polling thread
		 */
		if (thread_create(NULL, PAGESIZE,
		    (void (*)())sysctrl_overtemp_poll, 0, 0, &p0, TS_RUN, 60)
		    == NULL) {
			cmn_err(CE_WARN,
			    "sysctrl%d: cannot start overtemp thread",
			    instance);
			cv_destroy(&overtemp_cv);
		} else {
			sysctrl_overtemp_thread_started++;
		}
	}

	if (sysctrl_keyswitch_thread_started == 0) {
		extern void (*abort_seq_handler)();

		/*
		 * interpose sysctrl's abort sequence handler
		 */
		abort_seq_handler = sysctrl_abort_seq_handler;

		/*
		 * set up the key switch condition variable before
		 * starting the thread
		 */
		cv_init(&keyswitch_cv, "KeySwitch CV", CV_DRIVER, NULL);

		/*
		 * start up the key switch polling thread
		 */
		if (thread_create(NULL, PAGESIZE,
		    (void (*)())sysctrl_keyswitch_poll, 0, 0, &p0, TS_RUN, 60)
		    == NULL) {
			cmn_err(CE_WARN,
			    "sysctrl%d: cannot start key switch thread",
			    instance);
			cv_destroy(&keyswitch_cv);
		} else {
			sysctrl_keyswitch_thread_started++;
		}
	}

	/*
	 * perform initialization to allow setting of powerfail-time
	 */
	if ((dip = ddi_find_devinfo("options", -1, 0)) == NULL)
		softsp->options_nodeid = (dnode_t)NULL;
	else
		softsp->options_nodeid = (dnode_t)ddi_get_nodeid(dip);

	ddi_report_dev(devi);

	return (DDI_SUCCESS);

bad:
	/* XXX I'm sure there is more cleanup needed here */
	ddi_soft_state_free(sysctrlp, instance);
	cmn_err(CE_WARN,
	    "sysctrl%d: Initialization failure. Some system level events,"
	    " {AC Fail, Fan Failure, PS Failure} not detected", instance);
	return (DDI_FAILURE);
}

/* ARGSUSED */
static int
sysctrl_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_SUSPEND:
	case DDI_DETACH:
	default:
		return (DDI_FAILURE);
	}
}

/*
 * system_high_handler()
 * This routine handles system interrupts.
 *
 * This routine goes through all the interrupt sources and masks
 * off the enable bit if interrupting.  Because of the special
 * nature of the pps fan source bits, we also cache the state
 * of the fan bits for that special case.
 *
 * The rest of the work is done in the low level handlers
 */
static u_int
system_high_handler(caddr_t arg)
{
	struct sysctrl_soft_state *softsp = (struct sysctrl_soft_state *) arg;
	u_char csr;
	u_char status2;
	u_char tmp_reg;
	int serviced = 0;

	ASSERT(softsp);

	mutex_enter(&softsp->csr_mutex);

	/* read in the hardware registers */
	csr = *(softsp->csr);
	status2 = *(softsp->status2);

	if (csr & SYS_AC_PWR_FAIL_EN) {
		if (status2 & SYS_AC_FAIL) {

			/* save the powerfail state in nvram */
			nvram_update_powerfail(softsp);

			/* disable this interrupt source */
			csr &= ~SYS_AC_PWR_FAIL_EN;

			ddi_trigger_softintr(softsp->ac_fail_id);
			serviced++;
		}
	}

	if (csr & SYS_PS_FAIL_EN) {
		if ((*(softsp->ps_stat) != 0xff) ||
		    ((~status2) & (SYS_PPS0_OK | SYS_CLK_33_OK |
			SYS_CLK_50_OK)) ||
		    (~(*(softsp->pppsr)) & SYS_PPPSR_BITS)) {

			/* disable this interrupt source */
			csr &= ~SYS_PS_FAIL_EN;

			ddi_trigger_softintr(softsp->ps_fail_int_id);
			serviced++;
		}
	}

	if (csr & SYS_PPS_FAN_FAIL_EN) {
		if (status2 & SYS_RACK_FANFAIL ||
		    !(status2 & SYS_AC_FAN_OK) ||
		    !(status2 & SYS_KEYSW_FAN_OK)) {

			/*
			 * we must cache the fan status because it goes
			 * away when we disable interrupts !?!?!
			 */
			softsp->pps_fan_saved = status2;

			/* disable this interrupt source */
			csr &= ~SYS_PPS_FAN_FAIL_EN;

			ddi_trigger_softintr(softsp->pps_fan_id);
			serviced++;
		}
	}

	if (csr & SYS_SBRD_PRES_EN) {
		if (!(*(softsp->status1) & SYS_NOT_BRD_PRES)) {

			/* disable this interrupt source */
			csr &= ~SYS_SBRD_PRES_EN;

			ddi_trigger_softintr(softsp->sbrd_pres_id);
			serviced++;
		}
	}

	if (!serviced) {

		/*
		 * if we get here than it is likely that contact bounce
		 * is messing with us.  so, we need to shut this interrupt
		 * up for a while to let the contacts settle down.
		 * Then we will re-enable the interrupts that are enabled
		 * right now.  The trick is to disable the appropriate
		 * interrupts and then to re-enable them correctly, even
		 * though intervening handlers might have been working.
		 */

		/* remember all interrupts that could have caused it */
		softsp->saved_en_state |= csr &
		    (SYS_AC_PWR_FAIL_EN | SYS_PS_FAIL_EN |
		    SYS_PPS_FAN_FAIL_EN | SYS_SBRD_PRES_EN);

		/* and then turn them off */
		csr &= ~(SYS_AC_PWR_FAIL_EN | SYS_PS_FAIL_EN |
			SYS_PPS_FAN_FAIL_EN | SYS_SBRD_PRES_EN);

		/* and then bump the counter */
		softsp->spur_count++;

		/* and kick off the timeout */
		ddi_trigger_softintr(softsp->spur_id);
	}

	/* update the real csr */
	*(softsp->csr) = csr;
	tmp_reg = *(softsp->csr);
#ifdef lint
	tmp_reg = tmp_reg;
#endif
	mutex_exit(&softsp->csr_mutex);

	return (DDI_INTR_CLAIMED);
}

/*
 * we've detected a spurious interrupt.
 * determine if we should log a message and if we need another timeout
 */
static u_int
spur_delay(caddr_t arg)
{
	struct sysctrl_soft_state *softsp = (struct sysctrl_soft_state *) arg;

	ASSERT(softsp);

	/* do we need to complain? */
	mutex_enter(&softsp->csr_mutex);

	/* NOTE: this is == because we want one message per long timeout */
	if (softsp->spur_count == MAX_SPUR_COUNT) {
		char buf[128];

		/* print out the candidates known at this time */
		/* XXX not perfect because of re-entrant nature but close */
		buf[0] = '\0';
		if (softsp->saved_en_state & SYS_AC_PWR_FAIL_EN)
			strcat(buf, "AC FAIL");
		if (softsp->saved_en_state & SYS_PPS_FAN_FAIL_EN)
			strcat(buf, buf[0] ? "|PPS FANS" : "PPS FANS");
		if (softsp->saved_en_state & SYS_PS_FAIL_EN)
			strcat(buf, buf[0] ? "|PS FAIL" : "PS FAIL");
		if (softsp->saved_en_state & SYS_SBRD_PRES_EN)
			strcat(buf, buf[0] ? "|BOARD INSERT" : "BOARD INSERT");

		cmn_err(CE_WARN, "sysctrl%d: unserviced interrupt."
				" possible sources [%s].",
				ddi_get_instance(softsp->dip), buf);
	}
	mutex_exit(&softsp->csr_mutex);

	mutex_enter(&softsp->spur_int_lock);

	/* do we need to start the short timeout? */
	if (softsp->spur_timeout_id == 0) {
		softsp->spur_timeout_id = timeout(spur_retry,
			(caddr_t) softsp, spur_timeout_hz);
	}

	/* do we need to start the long timeout? */
	if (softsp->spur_long_timeout_id == 0) {
		softsp->spur_long_timeout_id = timeout(spur_long_timeout,
			(caddr_t) softsp, spur_long_timeout_hz);
	}

	mutex_exit(&softsp->spur_int_lock);

	return (DDI_INTR_CLAIMED);
}

/*
 * spur_retry
 *
 * this routine simply triggers the interrupt which will re-enable
 * the interrupts disabled by the spurious int detection.
 */
static void
spur_retry(caddr_t arg)
{
	struct sysctrl_soft_state *softsp = (struct sysctrl_soft_state *) arg;

	ASSERT(softsp);

	ddi_trigger_softintr(softsp->spur_high_id);

	mutex_enter(&softsp->spur_int_lock);
	softsp->spur_timeout_id = 0;
	mutex_exit(&softsp->spur_int_lock);
}

/*
 * spur_reenable
 *
 * OK, we've been slient for a while.   Go ahead and re-enable the
 * interrupts that were enabled at the time of the spurious detection.
 */
static u_int
spur_reenable(caddr_t arg)
{
	struct sysctrl_soft_state *softsp = (struct sysctrl_soft_state *) arg;
	u_char tmp_reg;

	ASSERT(softsp);

	mutex_enter(&softsp->csr_mutex);

	/* reenable those who were spurious candidates */
	*(softsp->csr) |= softsp->saved_en_state &
		(SYS_AC_PWR_FAIL_EN | SYS_PS_FAIL_EN |
		SYS_PPS_FAN_FAIL_EN | SYS_SBRD_PRES_EN);
	tmp_reg = *(softsp->csr);
#ifdef lint
	tmp_reg = tmp_reg;
#endif

	/* clear out the saved state */
	softsp->saved_en_state = 0;

	mutex_exit(&softsp->csr_mutex);

	return (DDI_INTR_CLAIMED);
}

/*
 * spur_long_timeout
 *
 * this routine merely resets the spurious interrupt counter thus ending
 * the interval of interest.  of course this is done by triggering a
 * softint because the counter is protected by an interrupt mutex.
 */
static void
spur_long_timeout(caddr_t arg)
{
	struct sysctrl_soft_state *softsp = (struct sysctrl_soft_state *) arg;

	ASSERT(softsp);

	ddi_trigger_softintr(softsp->spur_long_to_id);

	mutex_enter(&softsp->spur_int_lock);
	softsp->spur_long_timeout_id = 0;
	mutex_exit(&softsp->spur_int_lock);
}

/*
 * spur_clear_count
 *
 * simply clear out the spurious interrupt counter.
 *
 * softint level only
 */
static u_int
spur_clear_count(caddr_t arg)
{
	struct sysctrl_soft_state *softsp = (struct sysctrl_soft_state *) arg;

	ASSERT(softsp);

	mutex_enter(&softsp->csr_mutex);
	softsp->spur_count = 0;
	mutex_exit(&softsp->csr_mutex);

	return (DDI_INTR_CLAIMED);
}

/*
 * ac_fail_handler
 *
 * This routine polls the AC power failure bit in the system status2
 * register.  If we get to this routine, then we sensed an ac fail
 * condition.  Note the fact and check again in a few.
 *
 * Called as softint from high interrupt.
 */
static u_int
ac_fail_handler(caddr_t arg)
{
	struct sysctrl_soft_state *softsp = (struct sysctrl_soft_state *) arg;

	ASSERT(softsp);

	cmn_err(CE_WARN, "%s failure detected", ft_str_table[FT_AC_PWR]);
	reg_fault(0, FT_AC_PWR, FT_SYSTEM);
	(void) timeout(ac_fail_retry, (caddr_t) softsp, ac_timeout_hz);

	return (DDI_INTR_CLAIMED);
}

/*
 * The timeout from ac_fail_handler() that checks to see if the
 * condition persists.
 */
static void
ac_fail_retry(caddr_t arg)
{
	struct sysctrl_soft_state *softsp = (struct sysctrl_soft_state *) arg;

	ASSERT(softsp);

	if (*softsp->status2 & SYS_AC_FAIL) {	/* still bad? */
		(void) timeout(ac_fail_retry, (caddr_t) softsp, ac_timeout_hz);
	} else {
		cmn_err(CE_NOTE, "%s failure no longer detected",
			ft_str_table[FT_AC_PWR]);
		clear_fault(0, FT_AC_PWR, FT_SYSTEM);
		ddi_trigger_softintr(softsp->ac_fail_high_id);
	}
}

/*
 * The interrupt routine that we use to re-enable the interrupt.
 * Called from ddi_trigger_softint() in the ac_fail_retry() when
 * the AC is better.
 */
static u_int
ac_fail_reenable(caddr_t arg)
{
	struct sysctrl_soft_state *softsp = (struct sysctrl_soft_state *) arg;
	u_char tmp_reg;

	ASSERT(softsp);

	mutex_enter(&softsp->csr_mutex);
	*(softsp->csr) |= SYS_AC_PWR_FAIL_EN;
	tmp_reg = *(softsp->csr);
#ifdef lint
	tmp_reg = tmp_reg;
#endif
	mutex_exit(&softsp->csr_mutex);

	return (DDI_INTR_CLAIMED);
}

/*
 * ps_fail_int_handler
 *
 * Handle power supply failure interrupt.
 *
 * This wrapper is called as softint from hardware interrupt routine.
 */
static u_int
ps_fail_int_handler(caddr_t arg)
{
	return (ps_fail_handler((struct sysctrl_soft_state *)arg, 1));
}

/*
 * ps_fail_poll_handler
 *
 * Handle power supply failure interrupt.
 *
 * This wrapper is called as softint from power supply poll routine.
 */
static u_int
ps_fail_poll_handler(caddr_t arg)
{
	return (ps_fail_handler((struct sysctrl_soft_state *)arg, 0));
}

/*
 * ps_fail_handler
 *
 * This routine checks all eight of the board power supplies that are
 * installed plus the Peripheral power supply and the two DC OK. Since the
 * hardware bits are not enough to indicate Power Supply failure
 * vs. being turned off via software, the driver must maintain a
 * shadow state for the Power Supply status and monitor all changes.
 *
 * Called as a softint only.
 */
static u_int
ps_fail_handler(struct sysctrl_soft_state *softsp, int fromint)
{
	int i;
	struct ps_state *pstatp;
	int poll_needed = 0;
	u_char ps_stat, ps_pres, status1, status2, pppsr;
	u_char tmp_reg;
	enum power_state current_power_state;

	ASSERT(softsp);

	/* pre-read the hardware state */
	ps_stat = *softsp->ps_stat;
	ps_pres = *softsp->ps_pres;
	status1 = *softsp->status1;
	status2 = *softsp->status2;
	pppsr	= *softsp->pppsr;

	mutex_enter(&softsp->ps_fail_lock);

	for (i = 0, pstatp = &softsp->ps_stats[0]; i < SYS_PS_COUNT;
	    i++, pstatp++) {
		int	temp_psok;
		int	temp_pres;
		int	is_precharge = FALSE;
		int	is_fan_assy = FALSE;

		/*
		 * pre-compute the presence and ok bits for this
		 * power supply from the hardware registers.
		 * NOTE: 4-slot pps1 is the same as core ps 7...
		 */
		switch (i) {
		/* the core power supplies */
		case 0: case 1: case 2: case 3:
		case 4: case 5: case 6: case 7:
			temp_pres = !((ps_pres >> i) & 0x1);
			temp_psok = (ps_stat >> i) & 0x1;
			break;

		/* the first peripheral power supply */
		case SYS_PPS0_INDEX:
			temp_pres = !(status1 & SYS_NOT_PPS0_PRES);
			temp_psok = status2 & SYS_PPS0_OK;
			break;

		/* shared 3.3v clock power */
		case SYS_CLK_33_INDEX:
			temp_pres = TRUE;
			temp_psok = status2 & SYS_CLK_33_OK;
			break;

		/* shared 5.0v clock power */
		case SYS_CLK_50_INDEX:
			temp_pres = TRUE;
			temp_psok = status2 & SYS_CLK_50_OK;
			break;

		/* peripheral 5v */
		case SYS_V5_P_INDEX:
			temp_pres = !(status1 & SYS_NOT_PPS0_PRES) ||
				(IS4SLOT(softsp->nslots) &&
				!(ps_pres & SYS_NOT_PPS1_PRES));
			temp_psok = pppsr & SYS_V5_P_OK;
			break;

		/* peripheral 12v */
		case SYS_V12_P_INDEX:
			temp_pres = !(status1 & SYS_NOT_PPS0_PRES) ||
				(IS4SLOT(softsp->nslots) &&
				!(ps_pres & SYS_NOT_PPS1_PRES));
			temp_psok = pppsr & SYS_V12_P_OK;
			break;

		/* aux 5v */
		case SYS_V5_AUX_INDEX:
			temp_pres = !(status1 & SYS_NOT_PPS0_PRES);
			temp_psok = pppsr & SYS_V5_AUX_OK;
			break;

		/* peripheral 5v precharge */
		case SYS_V5_P_PCH_INDEX:
			temp_pres = !(status1 & SYS_NOT_PPS0_PRES);
			temp_psok = pppsr & SYS_V5_P_PCH_OK;
			is_precharge = TRUE;
			break;

		/* peripheral 12v precharge */
		case SYS_V12_P_PCH_INDEX:
			temp_pres = !(status1 & SYS_NOT_PPS0_PRES);
			temp_psok = pppsr & SYS_V12_P_PCH_OK;
			is_precharge = TRUE;
			break;

		/* 3.3v precharge */
		case SYS_V3_PCH_INDEX:
			temp_pres = !(status1 & SYS_NOT_PPS0_PRES);
			temp_psok = pppsr & SYS_V3_PCH_OK;
			is_precharge = TRUE;
			break;

		/* 5v precharge */
		case SYS_V5_PCH_INDEX:
			temp_pres = !(status1 & SYS_NOT_PPS0_PRES);
			temp_psok = pppsr & SYS_V5_PCH_OK;
			is_precharge = TRUE;
			break;

		/* peripheral fan assy */
		case SYS_P_FAN_INDEX:
			temp_pres = IS4SLOT(softsp->nslots) &&
				!(status1 & SYS_NOT_P_FAN_PRES);
			temp_psok = softsp->pps_fan_saved &
				SYS_AC_FAN_OK;
			is_fan_assy = TRUE;
			break;
		}

		/* *** Phase 1 -- power supply presence tests *** */

		/* do we know the presence status for this power supply? */
		if (pstatp->pshadow == PRES_UNKNOWN) {
			pstatp->pshadow = temp_pres ? PRES_IN : PRES_OUT;
			pstatp->dcshadow = temp_pres ? PS_BOOT : PS_OUT;
		} else {
			/* has the ps presence state changed? */
			if (!temp_pres ^ (pstatp->pshadow == PRES_IN)) {
				pstatp->pctr = 0;
			} else {
				/* a change! are we counting? */
				if (pstatp->pctr == 0) {
					pstatp->pctr = PS_PRES_CHANGE_TICKS;
				} else if (--pstatp->pctr == 0) {
					pstatp->pshadow = temp_pres ?
						PRES_IN : PRES_OUT;
					pstatp->dcshadow = temp_pres ?
						PS_UNKNOWN : PS_OUT;

					/*
					 * Now we know the state has
					 * changed, so we should log it.
					 */
					ps_log_pres_change(softsp,
						i, temp_pres);
				}
			}
		}

		/* *** Phase 2 -- power supply status tests *** */

		/* check if the Power Supply is removed or same as before */
		if ((pstatp->dcshadow == PS_OUT) ||
		    ((pstatp->dcshadow == PS_OK) && temp_psok) ||
		    ((pstatp->dcshadow == PS_FAIL) && !temp_psok)) {
			pstatp->dcctr = 0;
		} else {

			/* OK, a change, do we start the timer? */
			if (pstatp->dcctr == 0) {
				switch (pstatp->dcshadow) {
				case PS_BOOT:
					pstatp->dcctr = PS_FROM_BOOT_TICKS;
					break;

				case PS_UNKNOWN:
					pstatp->dcctr = is_fan_assy ?
						PS_P_FAN_FROM_UNKNOWN_TICKS :
						PS_FROM_UNKNOWN_TICKS;
					break;

				case PS_OK:
					pstatp->dcctr = is_precharge ?
						PS_PCH_FROM_OK_TICKS :
						PS_FROM_OK_TICKS;
					break;

				case PS_FAIL:
					pstatp->dcctr = PS_FROM_FAIL_TICKS;
					break;

				default:
					cmn_err(CE_PANIC,
						"sysctrl%d: Unknown Power "
						"Supply State %d",
						pstatp->dcshadow,
						ddi_get_instance(softsp->dip));
				}
			}

			/* has the ticker expired? */
			if (--pstatp->dcctr == 0) {

				/* we'll skip OK messages during boot */
				if (!((pstatp->dcshadow == PS_BOOT) &&
				    temp_psok)) {
					ps_log_state_change(softsp,
						i, temp_psok);
				}

				/* regardless, update the shadow state */
				pstatp->dcshadow = temp_psok ? PS_OK : PS_FAIL;
			}
		}

		/*
		 * We will need to continue polling for three reasons:
		 * - a failing power supply is detected and we haven't yet
		 *   determined the power supplies existence.
		 * - the power supply is just installed and we're waiting
		 *   to give it a change to power up,
		 * - a failed power supply state is recognized
		 *
		 * NOTE: PS_FAIL shadow state is not the same as !temp_psok
		 * because of the persistence of PS_FAIL->PS_OK.
		 */
		if (!temp_psok ||
		    (pstatp->dcshadow == PS_UNKNOWN) ||
		    (pstatp->dcshadow == PS_FAIL)) {
			poll_needed++;
		}
	}

	/*
	 * Now, get the current power state for this instance.
	 * If the current state is different than what was known, complain.
	 */
	current_power_state = compute_power_state(softsp);

	if (softsp->power_state != current_power_state) {
		switch (current_power_state) {
		case BELOW_MINIMUM:
			cmn_err(CE_WARN,
				"Insufficient power available to system");
			if (!disable_insufficient_power_reboot) {
				cmn_err(CE_WARN, "System reboot in %d seconds",
					PS_INSUFFICIENT_COUNTDOWN_SEC);
			}
			reg_fault(1, FT_INSUFFICIENT_POWER, FT_SYSTEM);
			softsp->power_countdown = PS_POWER_COUNTDOWN_TICKS;
			break;

		case MINIMUM:
			/* If we came from REDUNDANT, complain */
			if (softsp->power_state == REDUNDANT) {
				cmn_err(CE_WARN, "Redundant power lost");
			/* If we came from BELOW_MINIMUM, hurrah! */
			} else if (softsp->power_state == BELOW_MINIMUM) {
				cmn_err(CE_NOTE, "Minimum power available");
				clear_fault(1, FT_INSUFFICIENT_POWER,
					FT_SYSTEM);
			}
			break;

		case REDUNDANT:
			/* If we aren't from boot, spread the good news */
			if (softsp->power_state != BOOT) {
				cmn_err(CE_NOTE, "Redundant power available");
				clear_fault(1, FT_INSUFFICIENT_POWER,
					FT_SYSTEM);
			}
			break;

		default:
			break;
		}
	}
	softsp->power_state = current_power_state;
	mutex_exit(&softsp->ps_fail_lock);

	/*
	 * Are we in insufficient powerstate?
	 * If so, is it time to take action?
	 */
	if (softsp->power_state == BELOW_MINIMUM &&
	    softsp->power_countdown > 0 && --(softsp->power_countdown) == 0 &&
	    !disable_insufficient_power_reboot) {
		cmn_err(CE_WARN,
		    "Insufficient power. System Reboot Started...");

		fhc_reboot();
	}

	/*
	 * If we don't have ps problems that need to be polled for, then
	 * enable interrupts.
	 */
	if (!poll_needed) {
		mutex_enter(&softsp->csr_mutex);
		*(softsp->csr) |= SYS_PS_FAIL_EN;
		tmp_reg = *(softsp->csr);
#ifdef lint
		tmp_reg = tmp_reg;
#endif
		mutex_exit(&softsp->csr_mutex);
	}

	/*
	 * Only the polling loop re-triggers the polling loop timeout
	 */
	if (!fromint) {
		(void) timeout(ps_fail_retry, (caddr_t) softsp,
			ps_fail_timeout_hz);
	}

	return (DDI_INTR_CLAIMED);
}

/*
 * Compute the current power configuration for this system.
 * Disk boards and Clock boards are not counted.
 *
 * This function must be called with the ps_fail_lock held.
 */
static enum power_state
compute_power_state(struct sysctrl_soft_state *softsp)
{
	int i;
	int ok_supply_count = 0;
	int load_count = 0;
	int minimum_power_count;
	int pps_ok;
	struct bd_list *bd_list;

	ASSERT(mutex_owned(&softsp->ps_fail_lock));

	/*
	 * Walk down the interesting power supplies and
	 * count the operational power units
	 */
	for (i = 0; i < 8; i++) {
		/*
		 * power supply id 7 on a 4 slot system is PPS1.
		 * don't include it in the redundant core power calculation.
		 */
		if (i == 7 && IS4SLOT(softsp->nslots))
			continue;

		if (softsp->ps_stats[i].dcshadow == PS_OK)
			ok_supply_count++;
	}

	/* Note the state of the PPS... */
	pps_ok = (softsp->ps_stats[SYS_PPS0_INDEX].dcshadow == PS_OK);

	/*
	 * Dynamically compute the load count in the system.
	 * Don't count disk boards or boards in low power state.
	 */
	for (bd_list = get_and_lock_bdlist(-1); bd_list != NULL;
	    bd_list = get_next_bdlist(bd_list)) {

		/* Skip boards that don't count as a load */
		switch (bd_list->info.type) {
		case CLOCK_BOARD:
		/* XXX disk boards */
			continue;
		}

		switch (bd_list->info.state) {
		case UNKNOWN_STATE:
		case ACTIVE_STATE:
		case HOTPLUG_STATE:
			load_count++;
		}
	}
	unlock_bdlist();

	/*
	 * If we are 8 slot and we have 7 or 8 boards, then the PPS
	 * can count as a power supply...
	 */
	if (IS8SLOT(softsp->nslots) && load_count >= 7 && pps_ok)
		ok_supply_count++;

	/*
	 * Determine our power situation.  This is a simple step
	 * function right now:
	 *
	 * minimum power count = min(7, floor((board count + 1) / 2))
	 */
	minimum_power_count = (load_count + 1) / 2;
	if (minimum_power_count > 7)
		minimum_power_count = 7;

	if (ok_supply_count > minimum_power_count)
		return (REDUNDANT);
	else if (ok_supply_count == minimum_power_count)
		return (MINIMUM);
	else
		return (BELOW_MINIMUM);
}

/*
 * log the change of power supply presence
 */
static void
ps_log_pres_change(struct sysctrl_soft_state *softsp, int index, int present)
{
	char	*trans = present ? "Installed" : "Removed";

	switch (index) {
	/* the core power supplies (except for 7) */
	case 0: case 1: case 2: case 3:
	case 4: case 5: case 6:
		cmn_err(CE_NOTE, "%s %d %s", ft_str_table[FT_CORE_PS], index,
		    trans);
		if (!present) {
		    clear_fault(index, FT_CORE_PS, FT_SYSTEM);
		}
		break;

	/* power supply 7 / pps 1 */
	case 7:
		if (IS4SLOT(softsp->nslots)) {
		    cmn_err(CE_NOTE, "%s 1 %s", ft_str_table[FT_PPS], trans);
		    if (!present) {
			clear_fault(1, FT_PPS, FT_SYSTEM);
		    }
		} else {
		    cmn_err(CE_NOTE, "%s %d %s", ft_str_table[FT_CORE_PS],
			index, trans);
		    if (!present) {
			clear_fault(7, FT_CORE_PS, FT_SYSTEM);
		    }
		}
		break;

	/* the peripheral power supply 0 */
	case SYS_PPS0_INDEX:
		cmn_err(CE_NOTE, "%s 0 %s", ft_str_table[FT_PPS], trans);
		if (!present) {
			clear_fault(0, FT_PPS, FT_SYSTEM);
		}
		break;

	/* the peripheral rack fan assy */
	case SYS_P_FAN_INDEX:
		cmn_err(CE_NOTE, "%s %s", ft_str_table[FT_PPS_FAN], trans);
		if (!present) {
			clear_fault(0, FT_PPS_FAN, FT_SYSTEM);
		}
		break;

	/* we don't mention a change of presence state for any other power */
	}
}

/*
 * log the change of power supply status
 */
static void
ps_log_state_change(struct sysctrl_soft_state *softsp, int index, int ps_ok)
{
	int level = ps_ok ? CE_NOTE : CE_WARN;
	char *s = ps_ok ? "OK" : "Failing";

	switch (index) {
	/* the core power supplies (except 7) */
	case 0: case 1: case 2: case 3:
	case 4: case 5: case 6:
		cmn_err(level, "%s %d %s", ft_str_table[FT_CORE_PS], index, s);
		if (ps_ok) {
			clear_fault(index, FT_CORE_PS, FT_SYSTEM);
		} else {
			reg_fault(index, FT_CORE_PS, FT_SYSTEM);
		}
		break;

	/* power supply 7 / pps 1 */
	case 7:
		if (IS4SLOT(softsp->nslots)) {
			cmn_err(level, "%s 1 %s", ft_str_table[FT_PPS], s);
			if (ps_ok) {
				clear_fault(1, FT_PPS, FT_SYSTEM);
			} else {
				reg_fault(1, FT_PPS, FT_SYSTEM);
			}
		} else {
			cmn_err(level, "%s %d %s", ft_str_table[FT_CORE_PS],
				index, s);
			if (ps_ok) {
				clear_fault(index, FT_CORE_PS, FT_SYSTEM);
			} else {
				reg_fault(index, FT_CORE_PS, FT_SYSTEM);
			}
		}
		break;

	/* the peripheral power supply */
	case SYS_PPS0_INDEX:
		cmn_err(level, "%s %s", ft_str_table[FT_PPS], s);
		if (ps_ok) {
			clear_fault(0, FT_PPS, FT_SYSTEM);
		} else {
			reg_fault(0, FT_PPS, FT_SYSTEM);
		}
		break;

	/* shared 3.3v clock power */
	case SYS_CLK_33_INDEX:
		cmn_err(level, "%s %s", ft_str_table[FT_CLK_33], s);
		if (ps_ok) {
			clear_fault(0, FT_CLK_33, FT_SYSTEM);
		} else {
			reg_fault(0, FT_CLK_33, FT_SYSTEM);
		}
		break;

	/* shared 5.0v clock power */
	case SYS_CLK_50_INDEX:
		cmn_err(level, "%s %s", ft_str_table[FT_CLK_50], s);
		if (ps_ok) {
			clear_fault(0, FT_CLK_50, FT_SYSTEM);
		} else {
			reg_fault(0, FT_CLK_50, FT_SYSTEM);
		}
		break;

	/* peripheral 5v */
	case SYS_V5_P_INDEX:
		cmn_err(level, "%s %s", ft_str_table[FT_V5_P], s);
		if (ps_ok) {
			clear_fault(0, FT_V5_P, FT_SYSTEM);
		} else {
			reg_fault(0, FT_V5_P, FT_SYSTEM);
		}
		break;

	/* peripheral 12v */
	case SYS_V12_P_INDEX:
		cmn_err(level, "%s %s", ft_str_table[FT_V12_P], s);
		if (ps_ok) {
			clear_fault(0, FT_V12_P, FT_SYSTEM);
		} else {
			reg_fault(0, FT_V12_P, FT_SYSTEM);
		}
		break;

	/* aux 5v */
	case SYS_V5_AUX_INDEX:
		cmn_err(level, "%s %s", ft_str_table[FT_V5_AUX], s);
		if (ps_ok) {
			clear_fault(0, FT_V5_AUX, FT_SYSTEM);
		} else {
			reg_fault(0, FT_V5_AUX, FT_SYSTEM);
		}
		break;

	/* peripheral 5v precharge */
	case SYS_V5_P_PCH_INDEX:
		cmn_err(level, "%s %s", ft_str_table[FT_V5_P_PCH], s);
		if (ps_ok) {
			clear_fault(0, FT_V5_P_PCH, FT_SYSTEM);
		} else {
			reg_fault(0, FT_V5_P_PCH, FT_SYSTEM);
		}
		break;

	/* peripheral 12v precharge */
	case SYS_V12_P_PCH_INDEX:
		cmn_err(level, "%s %s", ft_str_table[FT_V12_P_PCH], s);
		if (ps_ok) {
			clear_fault(0, FT_V12_P_PCH, FT_SYSTEM);
		} else {
			reg_fault(0, FT_V12_P_PCH, FT_SYSTEM);
		}
		break;

	/* 3.3v precharge */
	case SYS_V3_PCH_INDEX:
		cmn_err(level, "%s %s", ft_str_table[FT_V3_PCH], s);
		if (ps_ok) {
			clear_fault(0, FT_V3_PCH, FT_SYSTEM);
		} else {
			reg_fault(0, FT_V3_PCH, FT_SYSTEM);
		}
		break;

	/* 5v precharge */
	case SYS_V5_PCH_INDEX:
		cmn_err(level, "%s %s", ft_str_table[FT_V5_PCH], s);
		if (ps_ok) {
			clear_fault(0, FT_V5_PCH, FT_SYSTEM);
		} else {
			reg_fault(0, FT_V5_PCH, FT_SYSTEM);
		}
		break;

	/* peripheral power supply fans */
	case SYS_P_FAN_INDEX:
		cmn_err(level, "%s %s", ft_str_table[FT_PPS_FAN], s);
		if (ps_ok) {
			clear_fault(0, FT_PPS_FAN, FT_SYSTEM);
		} else {
			reg_fault(0, FT_PPS_FAN, FT_SYSTEM);
		}
		break;
	}
}

/*
 * The timeout from ps_fail_handler() that simply re-triggers a check
 * of the ps condition.
 */
static void
ps_fail_retry(caddr_t arg)
{
	struct sysctrl_soft_state *softsp = (struct sysctrl_soft_state *) arg;

	ASSERT(softsp);

	ddi_trigger_softintr(softsp->ps_fail_poll_id);
}

/*
 * pps_fanfail_handler
 *
 * This routine is called from the high level handler.
 */
static u_int
pps_fanfail_handler(caddr_t arg)
{
	struct sysctrl_soft_state *softsp = (struct sysctrl_soft_state *) arg;

	ASSERT(softsp);

	/* always check again in a bit by re-enabling the fan interrupt */
	(void) timeout(pps_fanfail_retry, (caddr_t) softsp,
		pps_fan_timeout_hz);

	return (DDI_INTR_CLAIMED);
}

/*
 * After a bit of waiting, we simply re-enable the interrupt to
 * see if we get another one.  The softintr triggered routine does
 * the dirty work for us since it runs in the interrupt context.
 */
static void
pps_fanfail_retry(caddr_t arg)
{
	struct sysctrl_soft_state *softsp = (struct sysctrl_soft_state *) arg;

	ASSERT(softsp);

	ddi_trigger_softintr(softsp->pps_fan_high_id);
}

/*
 * The other half of the retry handler run from the interrupt context
 */
static u_int
pps_fanfail_reenable(caddr_t arg)
{
	struct sysctrl_soft_state *softsp = (struct sysctrl_soft_state *) arg;
	u_char tmp_reg;

	ASSERT(softsp);

	mutex_enter(&softsp->csr_mutex);

	/*
	 * re-initialize the bit field for all pps fans to assumed good.
	 * If the fans are still bad, we're going to get an immediate system
	 * interrupt which will put the correct state back anyway.
	 *
	 * NOTE: the polling routines that use this state understand the
	 * pulse resulting from above...
	 */
	softsp->pps_fan_saved = SYS_AC_FAN_OK | SYS_KEYSW_FAN_OK;

	*(softsp->csr) |= SYS_PPS_FAN_FAIL_EN;
	tmp_reg = *(softsp->csr);
#ifdef lint
	tmp_reg = tmp_reg;
#endif
	mutex_exit(&softsp->csr_mutex);

	return (DDI_INTR_CLAIMED);
}

/*
 *
 * Poll the hardware shadow state to determine the pps fan status.
 * The shadow state is maintained by the system_high handler and its
 * associated pps_* functions (above).
 *
 * There is a short time interval where the shadow state is pulsed to
 * the OK state even when the fans are bad.  However, this polling
 * routine has some built in hysteresis to filter out those _normal_
 * events.
 */
static void
pps_fan_poll(caddr_t arg)
{
	struct sysctrl_soft_state *softsp = (struct sysctrl_soft_state *) arg;
	int i;

	ASSERT(softsp);

	for (i = 0; i < SYS_PPS_FAN_COUNT; i++) {
		int fanfail = FALSE;

		/* determine fan status */
		switch (i) {
		case RACK:
			fanfail = softsp->pps_fan_saved & SYS_RACK_FANFAIL;
			break;

		case AC:
			/*
			 * Don't bother polling the AC fan on 4slot systems.
			 * Rather, it is handled by the power supply loop.
			 */
			fanfail = !IS4SLOT(softsp->nslots) &&
				!(softsp->pps_fan_saved & SYS_AC_FAN_OK);
			break;

		case KEYSW:
			/*
			 * This signal is not usable if aux5v is missing
			 * so we will synthesize a failed fan when aux5v
			 * fails or when pps0 is out.
			 */
			fanfail = (!IS4SLOT(softsp->nslots) &&
			    (softsp->ps_stats[SYS_V5_AUX_INDEX].dcshadow !=
				PS_OK)) ||
			    !(softsp->pps_fan_saved & SYS_KEYSW_FAN_OK);
			break;

		}

		/* is the fan bad? */
		if (fanfail) {

			/* is this condition different than we know? */
			if (softsp->pps_fan_state_count[i] == 0) {

				/* log the change to failed */
				pps_fan_state_change(softsp, i, FALSE);
			}

			/* always restart the fan OK counter */
			softsp->pps_fan_state_count[i] = PPS_FROM_FAIL_TICKS;
		} else {

			/* do we currently know the fan is bad? */
			if (softsp->pps_fan_state_count[i]) {

				/* yes, but has it been stable? */
				if (--softsp->pps_fan_state_count[i] == 0) {

					/* log the change to OK */
					pps_fan_state_change(softsp, i, TRUE);
				}
			}
		}
	}

	/* always check again in a bit by re-enabling the fan interrupt */
	(void) timeout(pps_fan_poll, (caddr_t) softsp, pps_fan_timeout_hz);
}

/*
 * pps_fan_state_change()
 *
 * Log the changed fan condition and update the external status.
 */
static void
pps_fan_state_change(struct sysctrl_soft_state *softsp, int index, int fan_ok)
{
	char *fan_type;
	char *state = fan_ok ? "fans OK" : "fan failure detected";

	switch (index) {
	case RACK:
		fan_type = IS4SLOT(softsp->nslots) ?
				"Disk Drive" : "Rack Exhaust";
		if (fan_ok) {
			softsp->pps_fan_external_state &= ~SYS_RACK_FANFAIL;
			clear_fault(0, IS4SLOT(softsp->nslots) ? FT_DSK_FAN :
				FT_RACK_EXH, FT_SYSTEM);
		} else {
			softsp->pps_fan_external_state |= SYS_RACK_FANFAIL;
			reg_fault(0, IS4SLOT(softsp->nslots) ? FT_DSK_FAN :
				FT_RACK_EXH, FT_SYSTEM);
		}
		break;

	case AC:
		fan_type = "AC Box";
		if (fan_ok) {
			softsp->pps_fan_external_state |= SYS_AC_FAN_OK;
			clear_fault(0, FT_AC_FAN, FT_SYSTEM);
		} else {
			softsp->pps_fan_external_state &= ~SYS_AC_FAN_OK;
			reg_fault(0, FT_AC_FAN, FT_SYSTEM);
		}
		break;

	case KEYSW:
		fan_type = "Keyswitch";
		if (fan_ok) {
			softsp->pps_fan_external_state |= SYS_KEYSW_FAN_OK;
			clear_fault(0, FT_KEYSW_FAN, FT_SYSTEM);
		} else {
			softsp->pps_fan_external_state &= ~SYS_KEYSW_FAN_OK;
			reg_fault(0, FT_KEYSW_FAN, FT_SYSTEM);
		}
		break;
	default:
		fan_type = "[invalid fan id]";
		break;
	}

	/* now log the state change */
	cmn_err(fan_ok ? CE_NOTE : CE_WARN, "%s %s", fan_type, state);
}

static u_int
bd_insert_handler(caddr_t arg)
{
	struct sysctrl_soft_state *softsp = (struct sysctrl_soft_state *) arg;

	ASSERT(softsp);

	/*
	 * This means the operator has hotplugged a board.
	 * In this case the clock registers are cleared to force a
	 * POST full retest.
	 */
	*softsp->clk_freq1 = 0;
	*softsp->clk_freq2 &= ~CLOCK_FREQ_8;

	(void) timeout(bd_insert_timeout, (caddr_t) softsp,
		bd_insert_delay_hz);

	return (DDI_INTR_CLAIMED);
}

/*
 * bd_insert_timeout()
 *
 * This routine handles the board insert interrupt. It is called from a
 * timeout so that it does not run at interrupt level. The main job
 * of this routine is to find hotplugged boards and de-assert the
 * board insert interrupt coming from the board. For hotplug phase I,
 * the routine also powers down the board.
 * JTAG scan is used to find boards which have been inserted.
 * All other control of the boards is also done by JTAG scan.
 */
static void
bd_insert_timeout(caddr_t arg)
{
	struct bd_list *bd_list;
	int board;
	/*
	 * This variable is used by the bd_insert_timeout routine. If this
	 * variable is set to zero, the routine knows it is not a hotplug
	 * that occurred, but a pre-scan to look for disabled boards in the
	 * system.
	 */
	static int been_here = 0;
	int start;		/* start index for scan loop */
	int limit;		/* board number limit for scan loop */
	int bd_incr;		/* amount to incr each pass thru loop */
	int board_found = 0;
	enum board_type type;
	u_int fhc_csr;
	u_int fhc_bsr;
	struct jt_mstr *jt_master;
	struct sysctrl_soft_state *softsp = (struct sysctrl_soft_state *) arg;

	ASSERT(softsp);

	/*
	 * lock the board list mutex. Keep it locked until all work
	 * is done.
	 */
	bd_list = get_and_lock_bdlist(-1);

	/*
	 * For some reason the JTAG master lock cannot be acquired. The
	 * software must attempt to scan again, so re-enable the
	 * interrupts.
	 */
	if ((jt_master = find_and_lock_jtag_master()) == NULL) {
		/* try and enable interrupts */
		ddi_trigger_softintr(softsp->sbrd_gone_id);
		unlock_bdlist();
		return;
	}

	/*
	 * loop thru the bd_list and only scan non-existing
	 * boards. Limit the scan based on number of slots in
	 * system (nslots).
	 */

	if (IS4SLOT(softsp->nslots)) {
		start = 1;
		limit = 8;
		bd_incr = 2;
	} else if (IS8SLOT(softsp->nslots)) {
		start = 0;
		limit = 8;
		bd_incr = 1;
	} else {
		start = 0;
		limit = 16;
		bd_incr = 1;
	}

	for (board = start; board < limit; board += bd_incr) {
		int replug_check = 0;
		struct bd_info tmp_info;

		/* if board is running in OS, do not scan */
		if (((bd_list = get_bdlist(board)) != NULL) &&
		    (bd_list->info.state == ACTIVE_STATE) &&
		    (bd_list->info.type != DISK_BOARD)) {
			continue;
		}

		/* if no board present, go to next one */
		if ((type = jtag_get_board_type(jt_master->jtag_cmd,
		    board)) == -1) {
			if ((bd_list != NULL) &&
			    UNCKECKED_DISABLED_BD(bd_list)) {
				/*
				 * Remove disabled board silently, as it
				 * never really existed. It was placed in
				 * disabled-boad list prop, but no board
				 * was present.
				 */
				if (bd_list->ksp != NULL) {
					kstat_delete(bd_list->ksp);
					bd_list->ksp = NULL;
				}

				bdlist_free_board(bd_list);
			}
			continue;
		}

		/*
		 * allocate and link in a bd_list element for
		 * a hot plug board. At present a hot plug board
		 * has no softsp pointer in its bd_list element.
		 */
		if (bd_list == NULL) {
			bd_list = bdlist_add_board(board, type,
				been_here ? HOTPLUG_STATE :
				FAILED_STATE, NULL);
		} else {
			replug_check = 1;
			if (UNCKECKED_DISABLED_BD(bd_list)) {
				cmn_err(CE_NOTE,
					"disabled %s board in slot %d",
					get_board_typestr(type),
					board);
			}
		}

		ASSERT(bd_list != NULL);

		/*
		 * XXX - This must be added later.
		 * check the power supply for the hotplug board
		 * If the fan is failed, or if there are no boards
		 * attached and the PS is failed, or if there is
		 * no PS, then we *MUST* power down this hot-plugged
		 * board.
		 */

		/*
		 * Give the scratch bd_info struct enough info
		 * to run.
		 */
		tmp_info.board = board;
		tmp_info.type = type;

		if (type != UNKNOWN_BOARD) {
			if (jtag_get_board_info(jt_master->jtag_cmd,
			    &tmp_info) == -1) {
				goto scan_failed;
			}

			/*
			 * XXX - Later enhancements needed jtag_integrity
			 * scan varies from a simple component ID verifica-
			 * tion up to full board ATPG testing.
			 */
		}

scan_failed:

		/* power down the board, if it is not a disk board. */
		if (type != DISK_BOARD) {
			if (jtag_powerdown_board(jt_master->jtag_cmd,
			    board, type, &fhc_csr, &fhc_bsr) == -1) {
				cmn_err(CE_WARN, "powerdown of board"
					" %d failed", board);
				tmp_info.state = bd_list->info.state;
			} else {
				if (bd_list->info.state == HOTPLUG_STATE) {
					tmp_info.state = LOWPOWER_STATE;
				} else {
					tmp_info.state = bd_list->info.state;
				}
			}
		} else {
			int hotplug;

			/*
			 * Scan the FHC to turn off the board insert
			 * interrupt. Don't change the power control.
			 * Modify LEDs based on hotplug status.
			 */
			if (bd_list->info.state == ACTIVE_STATE) {
				hotplug = 0;
				tmp_info.state = ACTIVE_STATE;
			} else {
				hotplug = 1;
				tmp_info.state = LOWPOWER_STATE;
			}
			jtag_init_disk_board(jt_master->jtag_cmd, board,
				hotplug, &fhc_csr, &fhc_bsr);
		}

		if (replug_check) {
			/*
			 * check for board insert interrupt being asserted
			 * in the scanned out data. This indicates a newly
			 * plugged in board.
			 */
			if ((fhc_csr & FHC_NOT_BRD_PRES) == 0) {
				cmn_err(CE_NOTE, "board %d has been removed",
					board);
				if (bd_list->ksp != NULL) {
					kstat_delete(bd_list->ksp);
					bd_list->ksp = NULL;
				}
			} else {
				/*
				 * Previously present board is still in same
				 * state since last pass thru. Leave it alone.
				 */
				if (UNCKECKED_DISABLED_BD(bd_list)) {
					if (type == CPU_BOARD) {
						set_cpu_speeds(&tmp_info,
							fhc_bsr);
					}

					/*
					 * copy the temporary board info
					 * into the database.
					 */
					tmp_info.state = DISABLED_STATE;
					bcopy((caddr_t) &tmp_info,
						(caddr_t) &bd_list->info,
						sizeof (struct bd_info));
				}
				continue;
			}
		}

		/* Set the CPU speeds */
		if (type == CPU_BOARD)
			set_cpu_speeds(&tmp_info, fhc_bsr);

		/* copy the temporary board info into the database */
		bcopy((caddr_t) &tmp_info, (caddr_t) &bd_list->info,
			sizeof (struct bd_info));

		board_found++;

		/* create the kstat for the hotplugged board */
		bd_list->ksp = kstat_create("unix", board,
			BDLIST_KSTAT_NAME, "misc", KSTAT_TYPE_RAW,
			sizeof (struct bd_info), KSTAT_FLAG_VIRTUAL);

		if (bd_list->ksp != NULL) {
			bd_list->ksp->ks_data = &bd_list->info;
			kstat_install(bd_list->ksp);
		}

		/* notify user of the board hotplugged */
		if (been_here) {
			cmn_err(CE_NOTE, "%s board hotplugged into slot %d",
				get_board_typestr(type), board);
		} else {
			char *state;

			if (bd_list->info.state == DISABLED_STATE) {
				state = "disabled";
			} else {
				state = "failed";
			}

			cmn_err(CE_NOTE, "%s %s board present in slot %d",
				state, get_board_typestr(type), board);
		}

		/* notify user ready to remove board */
		if (bd_list->info.state == LOWPOWER_STATE)
			cmn_err(CE_NOTE, "board %d is ready to remove", board);
	}

	if (!board_found && been_here) {
		cmn_err(CE_WARN, "Could not find hotplugged core system "
		"board");
	}

	release_jtag_master(jt_master);

	/*
	 * Set up a board remove timeout cal if there is not already one
	 * present.
	 */
	if (!hp_to_id) {
		hp_to_id = timeout(bd_remove_timeout, (caddr_t) softsp,
			bd_remove_timeout_hz);
	}

	unlock_bdlist();

	if (been_here) {
		/*
		 * In this case, the routine was called because of real
		 * hardware interrupt, not because of the priming call.
		 * Try and enable interrupts.
		 */

		ddi_trigger_softintr(softsp->sbrd_gone_id);
	}

	been_here = 1;
}

static void
bd_remove_timeout(caddr_t arg)
{
	struct sysctrl_soft_state *softsp = (struct sysctrl_soft_state *) arg;
	struct bd_list *bd_list;
	struct bd_list *tmp_list;
	struct jt_mstr *jt_master;
	int keep_polling = 0;

	/*
	 * lock the board list mutex. Keep it locked until all work
	 * is done.
	 */
	bd_list = get_and_lock_bdlist(-1);

	hp_to_id = 0;	/* delete our timeout ID */

	if ((jt_master = find_and_lock_jtag_master()) == NULL) {
		keep_polling = 1;
	} else {
		while (bd_list != NULL) {

			tmp_list = bd_list;
			bd_list = bd_list->next;

			if ((tmp_list->info.state != ACTIVE_STATE) ||
			    (tmp_list->info.type == DISK_BOARD)) {
				/* scan to see if the board is still in */
				if (jtag_get_board_type(jt_master->jtag_cmd,
				    tmp_list->info.board) == -1) {
					cmn_err(CE_NOTE, "board %d has been"
						" removed",
						tmp_list->info.board);

					if (tmp_list->ksp != NULL) {
						kstat_delete(tmp_list->ksp);
					}

					bdlist_free_board(tmp_list);
				} else {
					keep_polling = 1;
				}
			}
		}
		release_jtag_master(jt_master);
	}

	if (keep_polling) {
		hp_to_id =  timeout(bd_remove_timeout, (caddr_t) softsp,
			bd_remove_timeout_hz);
	}
	unlock_bdlist();
}

static u_int
bd_insert_normal(caddr_t arg)
{
	struct sysctrl_soft_state *softsp = (struct sysctrl_soft_state *) arg;
	u_char tmp_reg;

	ASSERT(softsp);

	/* has the condition been removed? */
	/* XXX add deglitch state machine here */
	if (!(*(softsp->status1) & SYS_NOT_BRD_PRES)) {
		/* check again in a few */
		(void) timeout(bd_insert_timeout, (caddr_t) softsp,
			bd_insert_retry_hz);
	} else {
		/* Turn on the enable bit for this interrupt */
		mutex_enter(&softsp->csr_mutex);
		*(softsp->csr) |= SYS_SBRD_PRES_EN;
		/* flush the hardware store buffer */
		tmp_reg = *(softsp->csr);
#ifdef lint
		tmp_reg = tmp_reg;
#endif
		mutex_exit(&softsp->csr_mutex);
	}

	return (DDI_INTR_CLAIMED);
}

#define	MAX_DIS_BRD_LEN		64

/*
 */
static void
preload_bd_list(struct sysctrl_soft_state *softsp)
{
	dnode_t node;
	dev_info_t *dnode;
	char buff[MAX_DIS_BRD_LEN];
	int len;
	int i;
	int id;
	int board;
	struct bd_list *list;
	int maxslot;

	/*
	 * find the disabled board list property if present. It is in
	 * the options node under root.
	 */
	if ((node = prom_finddevice("/options")) == OBP_BADNODE) {
		return;
	}

	/*
	 * The disabled board list is a null terminated list of boards
	 * in a string. Each char represents a board. The driver must
	 * reject illegal chars in case a user places them in the
	 * property.
	 */
	if ((len = prom_getproplen(node, "disabled-board-list")) == -1) {
		return;
	}

	if (len > MAX_DIS_BRD_LEN) {
		return;
	}

	prom_getprop(node, "disabled-board-list", buff);

	/* determine max board slot for this system. */
	if (IS4SLOT(softsp->nslots)) {
		maxslot = 7;
	} else if (IS8SLOT(softsp->nslots)) {
		maxslot = 7;
	} else {
		maxslot = 15;
	}

	list = get_and_lock_bdlist(-1);

	/*
	 * now loop thru the string, and create disabled board list
	 * entries for all legal boards in the list.
	 */
	for (i = 0; (i < len) && (buff[i] != 0); i++) {
		char ch = buff[i];

		if (ch >= '0' && ch <= '9') {
			board = ch - '0';
		} else if (ch >= 'A' && ch <= 'F') {
			board = ch - 'A' + 10;
		} else if (ch >= 'a' && ch <= 'f') {
			board = ch - 'a' + 10;
		} else {
			continue;
		}

		/*
		 * search for throw out disabled boards that cannot exist
		 * in this chassis.
		 */
		if (board > maxslot) {
			continue;
		}

		/* no even board numbers allowed on 4 Slot chassis. */
		if (IS4SLOT(softsp->nslots) && ((board % 2) == 0)) {
			continue;
		}

		/*
		 * The board does not really 'exist' yet. It's existence
		 * must be verified by the JTAG priming scan. If it is
		 * not found to be present, this record will be deleted.
		 */

		if ((list = get_bdlist(board)) == NULL) {
			list = bdlist_add_board(board, UNKNOWN_BOARD,
				DISABLED_STATE, NULL);

			/* Now create the kstat for this board */
			list->ksp = kstat_create("unix", board,
				BDLIST_KSTAT_NAME, "misc", KSTAT_TYPE_RAW,
				sizeof (struct bd_info), KSTAT_FLAG_VIRTUAL);

			if (list->ksp != NULL) {
				list->ksp->ks_data = &list->info;
				kstat_install(list->ksp);
			}
		} else if (list->info.state == ACTIVE_STATE) {
			cmn_err(CE_WARN, "Disabled board %d was really active",
				board);
		} else {
			/* XXX delete this */
			cmn_err(CE_WARN, "Duplicate boards in "
				"disabled boardlist");
		}
	}

	/*
	 * search the children of root to see if there are any
	 * disk boards in the tree.
	 */
	dnode = ddi_get_child(ddi_root_node());
	for (i = 0; dnode != NULL; dnode = ddi_get_next_sibling(dnode)) {
		if (strcmp(ddi_node_name(dnode), "disk-board") == 0) {

			/* get the board number property */

			if ((board = (int) ddi_getprop(DDI_DEV_T_ANY, dnode,
			    DDI_PROP_DONTPASS, OBP_BOARDNUM, -1)) == -1) {
				cmn_err(CE_WARN,
					"Could not find board number");
				continue;
			}

			/*
			 * Now actually add the bd_list entry, since we
			 * have a board number for it.
			 */
			list = bdlist_add_board(board, DISK_BOARD,
				ACTIVE_STATE, NULL);

			if ((id = (int) ddi_getprop(DDI_DEV_T_ANY, dnode,
			    DDI_PROP_DONTPASS, "disk0-scsi-id", -1)) != -1) {
				list->info.bd.dsk.disk_pres[0] = 1;
				list->info.bd.dsk.disk_id[0] = id;
			} else {
				list->info.bd.dsk.disk_pres[0] = 0;
			}

			if ((id = (int) ddi_getprop(DDI_DEV_T_ANY, dnode,
			    DDI_PROP_DONTPASS, "disk1-scsi-id", -1)) != -1) {
				list->info.bd.dsk.disk_pres[1] = 1;
				list->info.bd.dsk.disk_id[1] = id;
			} else {
				list->info.bd.dsk.disk_pres[1] = 0;
			}

			/* Now create the kstat for this board */
			list->ksp = kstat_create("unix", board,
				BDLIST_KSTAT_NAME, "misc", KSTAT_TYPE_RAW,
				sizeof (struct bd_info), KSTAT_FLAG_VIRTUAL);

			if (list->ksp != NULL) {
				list->ksp->ks_data = &list->info;
				kstat_install(list->ksp);
			}
		}
	}
	unlock_bdlist();
}

/*
 * blink LED handler.
 *
 * The actual bit manipulation needs to occur at interrupt level
 * because we need access to the CSR with its CSR mutex
 */
static u_int
blink_led_handler(caddr_t arg)
{
	struct sysctrl_soft_state *softsp = (struct sysctrl_soft_state *) arg;
	u_char tmp_reg;

	ASSERT(softsp);

	mutex_enter(&softsp->csr_mutex);

	/*
	 * XXX - The lock for the sys_led is not held here. If more
	 * complicated tasks are done with the System LED, then
	 * locking should be done here.
	 */

	/* read the hardware register. */
	tmp_reg = *(softsp->csr);

	/* Only turn on the OS System LED bit if the softsp state is on. */
	if (softsp->sys_led) {
		tmp_reg |= SYS_LED_RIGHT;
	} else {
		tmp_reg &= ~SYS_LED_RIGHT;
	}

	/* Turn on the yellow LED if system fault status is set. */
	if (softsp->sys_fault) {
		tmp_reg |=  SYS_LED_MID;
	} else {
		tmp_reg &= ~SYS_LED_MID;
	}

	/* write to the hardware register */
	*(softsp->csr) = tmp_reg;

	/* flush the hardware store buffer */
	tmp_reg = *(softsp->csr);
#ifdef lint
	tmp_reg = tmp_reg;
#endif
	mutex_exit(&softsp->csr_mutex);

	(void) timeout(blink_led_timeout, (caddr_t) softsp,
		blink_led_timeout_hz);

	return (DDI_INTR_CLAIMED);
}

/*
 * simply re-trigger the interrupt handler on led timeout
 */
static void
blink_led_timeout(caddr_t arg)
{
	struct sysctrl_soft_state *softsp = (struct sysctrl_soft_state *) arg;
	int led_state;

	ASSERT(softsp);

	/*
	 * Process the system fault list here. This is where the driver
	 * must decide what yellow LEDs to turn on if any. The fault
	 * list is walked and each bd_list entry is updated with it's
	 * yellow LED status. This info is used later by the routine
	 * toggle_board_green_leds().
	 *
	 * The variable system_fault is non-zero if any non-
	 * suppressed faults are found in the system.
	 */
	softsp->sys_fault = process_fault_list();

	/* blink the system board OS LED */
	mutex_enter(&softsp->sys_led_lock);
	softsp->sys_led = !softsp->sys_led;
	led_state = softsp->sys_led;
	mutex_exit(&softsp->sys_led_lock);

	toggle_board_green_leds(led_state);

	ddi_trigger_softintr(softsp->blink_led_id);
}

void
toggle_board_green_leds(int led_state)
{
	struct bd_list *list;
	u_int value = 0;

	if (led_state) {
		value = FHC_LED_RIGHT;
	}

	for (list = get_and_lock_bdlist(-1); list != NULL;
	    list = get_next_bdlist(list)) {
		if ((list->info.state != ACTIVE_STATE) ||
		    (list->info.type == DISK_BOARD)) {
			continue;
		}

		/*
		 * Do not twiddle the LEDs on the FHC that belongs to
		 * the clock board.
		 */
		if (list->info.type == CLOCK_BOARD) {
			continue;
		}

		ASSERT(list->softsp);

		if (list->fault) {
			value |= FHC_LED_MID;
		} else {
			value &= ~FHC_LED_MID;
		}

		update_board_leds(list, FHC_LED_RIGHT|FHC_LED_MID, value);
	}
	unlock_bdlist();
}

static char *
get_board_typestr(enum board_type type)
{
	char *type_str;

	switch (type) {
	case MEM_BOARD:
		type_str = MEM_BD_NAME;
		break;

	case CPU_BOARD:
		type_str = CPU_BD_NAME;
		break;

	case IO_2SBUS_BOARD:
		type_str = IO_2SBUS_BD_NAME;
		break;

	case IO_SBUS_FFB_BOARD:
		type_str = IO_SBUS_FFB_BD_NAME;
		break;

	case IO_PCI_BOARD:
		type_str = IO_PCI_BD_NAME;
		break;

	case DISK_BOARD:
		type_str = DISK_BD_NAME;
		break;

	case UNKNOWN_BOARD:
	default:
		type_str = "unknown";
		break;
	}
	return (type_str);
}

/*
 * set_cpu_speeds
 *
 * This routine extracts CPU speed pins from the FHC board status register
 * passed in and stores them in the board info structure passed in. This
 * is used to determine the rated speeds of CPUs on hotplugged boards.
 */
static void
set_cpu_speeds(struct bd_info *info, u_int fhc_bsr)
{
	if (info->bd.cpu[0].cache_size != 0) {
		info->bd.cpu[0].cpu_speed = CPU_0_SPEED(fhc_bsr);
	} else {
		info->bd.cpu[0].cpu_speed = 0;
	}

	if (info->bd.cpu[1].cache_size != 0) {
		info->bd.cpu[1].cpu_speed = CPU_1_SPEED(fhc_bsr);
	} else {
		info->bd.cpu[1].cpu_speed = 0;
	}
}

/*
 * timestamp an AC power failure in nvram
 */
static void
nvram_update_powerfail(struct sysctrl_soft_state *softsp)
{
	char buf[80];
	int len = 0;

	numtos(hrestime.tv_sec, buf);

	if (softsp->options_nodeid) {
		len = prom_setprop(softsp->options_nodeid, "powerfail-time",
			buf, strlen(buf)+1);
	}

	if (len <= 0) {
		cmn_err(CE_WARN, "sysctrl%d: failed to set powerfail-time "
			"to %s\n", ddi_get_instance(softsp->dip), buf);
	}
}

void
sysctrl_add_kstats(struct sysctrl_soft_state *softsp)
{
	struct kstat	*ksp;		/* Generic sysctrl kstats */
	struct kstat	*pksp;		/* Power Supply kstat */
	struct kstat	*tksp;		/* Sysctrl temperatrure kstat */
	struct sysctrl_kstat *sysksp;

	if ((ksp = kstat_create("unix", ddi_get_instance(softsp->dip),
	    SYSCTRL_KSTAT_NAME, "misc", KSTAT_TYPE_NAMED,
	    sizeof (struct sysctrl_kstat) / sizeof (kstat_named_t),
	    KSTAT_FLAG_PERSISTENT)) == NULL) {
		cmn_err(CE_WARN, "sysctrl%d: kstat_create failed",
			ddi_get_instance(softsp->dip));
	}

	if ((tksp = kstat_create("unix", CLOCK_BOARD_INDEX,
	    OVERTEMP_KSTAT_NAME, "misc", KSTAT_TYPE_RAW,
	    sizeof (struct temp_stats), KSTAT_FLAG_PERSISTENT)) == NULL) {
		cmn_err(CE_WARN, "sysctrl%d: kstat_create failed",
			ddi_get_instance(softsp->dip));
	}

	if ((pksp = kstat_create("unix", ddi_get_instance(softsp->dip),
	    PSSHAD_KSTAT_NAME, "misc", KSTAT_TYPE_RAW,
	    SYS_PS_COUNT, KSTAT_FLAG_PERSISTENT)) == NULL) {
		cmn_err(CE_WARN, "sysctrl%d: kstat_create failed",
			ddi_get_instance(softsp->dip));
	}

	sysksp = (struct sysctrl_kstat *)(ksp->ks_data);

	/* now init the named kstats */
	kstat_named_init(&sysksp->csr, CSR_KSTAT_NAMED,
		KSTAT_DATA_CHAR);

	kstat_named_init(&sysksp->status1, STAT1_KSTAT_NAMED,
		KSTAT_DATA_CHAR);

	kstat_named_init(&sysksp->status2, STAT2_KSTAT_NAMED,
		KSTAT_DATA_CHAR);

	kstat_named_init(&sysksp->clk_freq2, CLK_FREQ2_KSTAT_NAMED,
		KSTAT_DATA_CHAR);

	kstat_named_init(&sysksp->fan_status, FAN_KSTAT_NAMED,
		KSTAT_DATA_CHAR);

	kstat_named_init(&sysksp->key_status, KEY_KSTAT_NAMED,
		KSTAT_DATA_CHAR);

	kstat_named_init(&sysksp->power_state, POWER_KSTAT_NAMED,
		KSTAT_DATA_LONG);

	ksp->ks_update = sysctrl_kstat_update;
	ksp->ks_private = (void *)softsp;

	pksp->ks_update = psstat_kstat_update;
	pksp->ks_private = (void *)softsp;

	tksp->ks_update = overtemp_kstat_update;
	tksp->ks_private = (void *) &softsp->tempstat;

	kstat_install(ksp);
	kstat_install(pksp);
	kstat_install(tksp);
}

static int
sysctrl_kstat_update(kstat_t *ksp, int rw)
{
	struct sysctrl_kstat *sysksp;
	struct sysctrl_soft_state *softsp;

	sysksp = (struct sysctrl_kstat *)(ksp->ks_data);
	softsp = (struct sysctrl_soft_state *)(ksp->ks_private);

	/* this is a read-only kstat. Exit on a write */

	if (rw == KSTAT_WRITE) {
		return (EACCES);
	} else {
		/*
		 * copy the current state of the hardware into the
		 * kstat structure.
		 */
		sysksp->csr.value.c[0] = *(softsp->csr);
		sysksp->status1.value.c[0] = *(softsp->status1);
		sysksp->status2.value.c[0] = *(softsp->status2);
		sysksp->clk_freq2.value.c[0] = *(softsp->clk_freq2);

		sysksp->fan_status.value.c[0] = softsp->pps_fan_external_state;
		sysksp->key_status.value.c[0] = softsp->key_shadow;
		sysksp->power_state.value.l = softsp->power_state;
	}
	return (0);
}

static int
psstat_kstat_update(kstat_t *ksp, int rw)
{
	struct sysctrl_soft_state *softsp;
	u_char *ptr = (u_char *) (ksp->ks_data);
	int ps;

	softsp = (struct sysctrl_soft_state *)(ksp->ks_private);

	if (rw == KSTAT_WRITE) {
		return (EACCES);
	} else {
		for (ps = 0; ps < SYS_PS_COUNT; ps++) {
			*ptr++ = softsp->ps_stats[ps].dcshadow;
		}
	}
	return (0);
}

static void
sysctrl_thread_wakeup(int type)
{
	/*
	 * grab mutex to guarantee that our wakeup call
	 * arrives after we go to sleep -- so we can't sleep forever.
	 */
	mutex_enter(&sslist_mutex);
	switch (type) {
	case OVERTEMP_POLL:
		cv_signal(&overtemp_cv);
		break;
	case KEYSWITCH_POLL:
		cv_signal(&keyswitch_cv);
		break;
	default:
		cmn_err(CE_WARN, "sysctrl: invalid type %d to wakeup\n", type);
		break;
	}
	mutex_exit(&sslist_mutex);
}

static void
sysctrl_overtemp_poll(void)
{
	struct sysctrl_soft_state *list;

	/* The overtemp data strcutures are protected by a mutex. */
	mutex_enter(&sslist_mutex);

	while (sysctrl_do_overtemp_thread) {

		for (list = sys_list; list != NULL; list = list->next) {
			if (list->temp_reg != NULL) {
				update_temp(list->pdip, &list->tempstat,
					*(list->temp_reg));
			}
		}

		/* now have this thread sleep for a while */
		(void) timeout(sysctrl_thread_wakeup, (caddr_t)OVERTEMP_POLL,
			overtemp_timeout_hz);

		cv_wait(&overtemp_cv, &sslist_mutex);
	}
	mutex_exit(&sslist_mutex);

	thread_exit();
	/* NOTREACHED */
}

static void
sysctrl_keyswitch_poll(void)
{
	struct sysctrl_soft_state *list;

	/* The keyswitch data strcutures are protected by a mutex. */
	mutex_enter(&sslist_mutex);

	while (sysctrl_do_keyswitch_thread) {

		for (list = sys_list; list != NULL; list = list->next) {
			if (list->status1 != NULL)
				update_key_state(list);
		}

		/* now have this thread sleep for a while */
		(void) timeout(sysctrl_thread_wakeup, (caddr_t)KEYSWITCH_POLL,
			keyswitch_timeout_hz);

		cv_wait(&keyswitch_cv, &sslist_mutex);
	}
	mutex_exit(&sslist_mutex);

	thread_exit();
	/* NOTREACHED */
}

/*
 * check the key switch position for state changes
 */
static void
update_key_state(struct sysctrl_soft_state *list)
{
	enum keyswitch_state key;

	/*
	 * snapshot current hardware key position
	 */
	if (*(list->status1) & SYS_NOT_SECURE)
		key = KEY_NOT_SECURE;
	else
		key = KEY_SECURE;

	/*
	 * check for state transition
	 */
	if (key != list->key_shadow) {

		/*
		 * handle state transition
		 */
		switch (list->key_shadow) {
		case KEY_BOOT:
			cmn_err(CE_CONT, "?sysctrl%d: Key switch is%sin the "
			    "secure position\n", ddi_get_instance(list->dip),
			    (key == KEY_SECURE) ? " " : " not ");
			list->key_shadow = key;
			break;
		case KEY_SECURE:
		case KEY_NOT_SECURE:
			cmn_err(CE_NOTE, "sysctrl%d: Key switch has changed"
			    " to the %s position",
			    ddi_get_instance(list->dip),
			    (key == KEY_SECURE) ? "secure" : "not-secure");
			list->key_shadow = key;
			break;
		default:
			cmn_err(CE_CONT,
			    "?sysctrl%d: Key switch is in an unknown position,"
			    "treated as being in the %s position\n",
			    ddi_get_instance(list->dip),
			    (list->key_shadow == KEY_SECURE) ?
			    "secure" : "not-secure");
			break;
		}
	}
}

/*
 * consider key switch position when handling an abort sequence
 */
static void
sysctrl_abort_seq_handler(char *msg)
{
	struct sysctrl_soft_state *list;
	u_int secure = 0;
	char buf[64], inst[4];


	/*
	 * if any of the key switch positions are secure,
	 * then disallow entry to the prom/debugger
	 */
	mutex_enter(&sslist_mutex);
	buf[0] = (char)0;
	for (list = sys_list; list != NULL; list = list->next) {
		if (list->key_shadow == KEY_SECURE) {
			if (secure++)
				strcat(buf, ",");
			/*
			 * XXX: later, replace instance number with nodeid
			 */
			sprintf(inst, "%d", ddi_get_instance(list->dip));
			strcat(buf, inst);
		}
	}
	mutex_exit(&sslist_mutex);

	if (secure) {
		cmn_err(CE_CONT,
			"!sysctrl(%s): ignoring debug enter sequence\n", buf);
	} else {
		cmn_err(CE_CONT, "!sysctrl: allowing debug enter\n");
		debug_enter(msg);
	}
}

#define	TABLE_END	0xFF

struct uart_cmd {
	u_char reg;
	u_char data;
};

/*
 * Time constant defined by this formula:
 *	((4915200/32)/(baud) -2)
 */

struct uart_cmd uart_table[] = {
	{ 0x09, 0xc0 },	/* Force hardware reset */
	{ 0x04, 0x46 },	/* X16 clock mode, 1 stop bit/char, no parity */
	{ 0x03, 0xc0 },	/* Rx is 8 bits/char */
	{ 0x05, 0xe2 },	/* DTR, Tx is 8 bits/char, RTS */
	{ 0x09, 0x02 },	/* No vector returned on interrupt */
	{ 0x0b, 0x55 },	/* Rx Clock = Tx Clock = BR generator = ~TRxC OUT */
	{ 0x0c, 0x0e },	/* Time Constant = 0x000e for 9600 baud */
	{ 0x0d, 0x00 },	/* High byte of time constant */
	{ 0x0e, 0x02 },	/* BR generator comes from Z-SCC's PCLK input */
	{ 0x03, 0xc1 },	/* Rx is 8 bits/char, Rx is enabled */
	{ 0x05, 0xea },	/* DTR, Tx is 8 bits/char, Tx is enabled, RTS */
	{ 0x0e, 0x03 },	/* BR comes from PCLK, BR generator is enabled */
	{ 0x00, 0x30 },	/* Error reset */
	{ 0x00, 0x30 },	/* Error reset */
	{ 0x00, 0x10 },	/* external status reset */
	{ 0x03, 0xc1 },	/* Rx is 8 bits/char, Rx is enabled */
	{ TABLE_END, 0x0 }
};

static void
init_remote_console_uart(struct sysctrl_soft_state *softsp)
{
	int i = 0;

	/*
	 * Serial chip expects software to write to the control
	 * register first with the desired register number. Then
	 * write to the control register with the desired data.
	 * So walk thru table writing the register/data pairs to
	 * the serial port chip.
	 */
	while (uart_table[i].reg != TABLE_END) {
		*(softsp->rcons_ctl) = uart_table[i].reg;
		*(softsp->rcons_ctl) = uart_table[i].data;
		i++;
	}
}
