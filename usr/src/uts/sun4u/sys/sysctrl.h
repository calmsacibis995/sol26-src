/*
 * Copyright (c) 1994-1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SYSCTRL_H
#define	_SYS_SYSCTRL_H

#pragma ident	"@(#)sysctrl.h	1.40	96/10/17 SMI"

/*
 * Include any headers you depend on.
 */

#ifdef	__cplusplus
extern "C" {
#endif

/* useful debugging stuff */
#define	SYSCTRL_ATTACH_DEBUG	0x1
#define	SYSCTRL_INTERRUPT_DEBUG	0x2
#define	SYSCTRL_REGISTERS_DEBUG	0x4

/*
 * OBP supplies us with 2 register sets for the clock-board node. The code for
 * the syctrl driver relies on these register sets being presented by the
 * PROM in the order specified below. If this changes, the following comments
 * must be revised and the code in sysctrl_init() must be changed to reflect
 * these revisions.
 *
 * They are:
 * 	0	Clock frequency registers
 *	1	misc registers
 */

/*
 * The offsets are defined as offsets in bytes from the base of the OBP
 * register to which the register belongs to.
 */

/* Register set 0 */
#define	SYS_OFF_CLK_FREQ2	0x2	/* offset of clock register 2 */

/* Important bits for Clock Frequency register 2 */
#define	RCONS_UART_EN	0x80	/* Remote console reset enabled */
#define	GEN_RESET_EN	0x40	/* Enable reset on freq change */
#define	TOD_RESET_EN	0x20	/* Enable reset from TOD watchdog */
#define	CLOCK_FREQ_8	0x01	/* Frequency bit 8 */

/* Register set 1 */
#define	SYS_OFF_CTRL	0x0	/* Offset of System Control register */
#define	SYS_OFF_STAT1	0x10	/* Offset of System Status1 register */
#define	SYS_OFF_STAT2	0x20	/* Offset of System Status2 register */
#define	SYS_OFF_PSSTAT	0x30	/* Offset of Power Supply Status */
#define	SYS_OFF_PSPRES	0x40	/* Offset of Power Supply Presence */
#define	SYS_OFF_TEMP	0x50	/* Offset of temperature register */
#define	SYS_OFF_DIAG	0x60	/* Offset of interrupt diag register */
#define	SYS_OFF_PPPSR	0x70	/* Offset of second Power Supply Status */

#define	RMT_CONS_OFFSET	0x4004	/* Offset of Remote Console UART */
#define	RMT_CONS_LEN	0x8	/* Size of Remote Console UART */

/* Bit field defines for System Control register */
#define	SYS_PPS_FAN_FAIL_EN	0x80	/* PPS Fan Fail Interrupt Enable */
#define	SYS_PS_FAIL_EN		0x40	/* PS DC Fail Interrupt Enable */
#define	SYS_AC_PWR_FAIL_EN	0x20	/* AC Power Fail Interrupt Enable */
#define	SYS_SBRD_PRES_EN	0x10	/* Board Insertion Interrupt En */
#define	SYS_PWR_OFF		0x08	/* Bit to turn system power */
#define	SYS_LED_LEFT		0x04	/* System Left LED. Reverse Logic */
#define	SYS_LED_MID		0x02	/* System Middle LED */
#define	SYS_LED_RIGHT		0x01	/* System Right LED */

/* Bit field defines for System Status1 register */
#define	SYS_SLOTS		0xC0	/* system type slot mask */
#define	SYS_NOT_SECURE		0x20	/* ==0 Keyswitch in secure pos. */
#define	SYS_NOT_P_FAN_PRES	0x10	/* ==0 PPS cooling tray present */
#define	SYS_NOT_BRD_PRES	0x08	/* ==0 When board inserted */
#define	SYS_NOT_PPS0_PRES	0x04	/* ==0 If PPS0 present */
#define	SYS_TOD_NOT_RST		0x02	/* ==0 if TOD reset occurred */
#define	SYS_GEN_NOT_RST		0x01	/* ==0 if clock freq reset occured */

/* Macros to determine system type from System Status1 register */
#define	SYS_TYPE(x)		((x) & SYS_SLOTS)
#define	SYS_16_SLOT		0x40
#define	SYS_8_SLOT		0xC0
#define	SYS_4_SLOT		0x80
#define	SYS_TESTBED		0x00
#define	IS4SLOT(reg)		(SYS_TYPE(reg) == SYS_4_SLOT)
#define	IS8SLOT(reg)		(SYS_TYPE(reg) == SYS_8_SLOT)
#define	IS16SLOT(reg)		(SYS_TYPE(reg) == SYS_16_SLOT)
#define	ISTESTBED(reg)		(SYS_TYPE(reg) == SYS_TESTBED)

/* Bit field defines for System Status2 register */
#define	SYS_RMTE_NOT_RST	0x80	/* Remote Console reset occurred */
#define	SYS_PPS0_OK		0x40	/* ==1 PPS0 OK */
#define	SYS_CLK_33_OK		0x20	/* 3.3V OK on clock board */
#define	SYS_CLK_50_OK		0x10	/* 5.0V OK on clock board */
#define	SYS_AC_FAIL		0x08	/* System lost AC Power source */
#define	SYS_RACK_FANFAIL	0x04	/* Peripheral Rack fan status */
#define	SYS_AC_FAN_OK		0x02	/* Status of 4 AC box fans */
#define	SYS_KEYSW_FAN_OK	0x01	/* Status of keyswitch fan */

/* Bit field defines for Power Supply Presence register */
#define	SYS_NOT_PPS1_PRES	0x80	/* ==0 if PPS1 present in 4slot */

/* Bit field defines for Precharge and Peripheral Power Status register */
#define	SYS_NOT_CURRENT_S	0x80	/* Current share backplane */
#define	SYS_PPPSR_BITS		0x7f	/* bulk test bit mask */
#define	SYS_V5_P_OK		0x40	/* ==1 peripheral 5v ok */
#define	SYS_V12_P_OK		0x20	/* ==1 peripheral 12v ok */
#define	SYS_V5_AUX_OK		0x10	/* ==1 auxiliary 5v ok */
#define	SYS_V5_P_PCH_OK		0x08	/* ==1 peripheral 5v precharge ok */
#define	SYS_V12_P_PCH_OK	0x04	/* ==1 peripheral 12v precharge ok */
#define	SYS_V3_PCH_OK		0x02	/* ==1 system 3.3v precharge ok */
#define	SYS_V5_PCH_OK		0x01	/* ==1 system 5.0v precharge ok */

#ifndef _ASM

#define	SYSCTRL_KSTAT_NAME	"sysctrl"
#define	CSR_KSTAT_NAMED		"csr"
#define	STAT1_KSTAT_NAMED	"status1"
#define	STAT2_KSTAT_NAMED	"status2"
#define	CLK_FREQ2_KSTAT_NAMED	"clk_freq2"
#define	FAN_KSTAT_NAMED		"fan_status"
#define	KEY_KSTAT_NAMED		"key_status"
#define	POWER_KSTAT_NAMED	"power_status"
#define	BDLIST_KSTAT_NAME	"bd_list"

/*
 * The Power Supply shadow kstat is too large to fit in a kstat_named
 * struct, so it has been changed to be a raw kstat.
 */
#define	PSSHAD_KSTAT_NAME	"ps_shadow"

/* States of a power supply DC voltage. */
enum e_state { PS_BOOT = 0, PS_OUT, PS_UNKNOWN, PS_OK, PS_FAIL };
enum e_pres_state { PRES_UNKNOWN = 0, PRES_IN, PRES_OUT };

/*
 * several power supplies are managed -- 8 core power supplies,
 * up to two pps, a couple of clock board powers and a register worth
 * of precharges.
 */
#define	SYS_PS_COUNT 19
/* core PS 0 thru 7 are index 0 thru 7 */
#define	SYS_PPS0_INDEX		8
#define	SYS_CLK_33_INDEX	9
#define	SYS_CLK_50_INDEX	10
#define	SYS_V5_P_INDEX		11
#define	SYS_V12_P_INDEX		12
#define	SYS_V5_AUX_INDEX	13
#define	SYS_V5_P_PCH_INDEX	14
#define	SYS_V12_P_PCH_INDEX	15
#define	SYS_V3_PCH_INDEX	16
#define	SYS_V5_PCH_INDEX	17
#define	SYS_P_FAN_INDEX		18	/* the peripheral fan assy */

/* fan timeout structures */
enum pps_fan_type { RACK = 0, AC = 1, KEYSW = 2 };
#define	SYS_PPS_FAN_COUNT	3

/*
 * States of the secure key switch position.
 */
enum keyswitch_state { KEY_BOOT = 0, KEY_SECURE, KEY_NOT_SECURE };

/* Redundant power states */
enum power_state { BOOT = 0, BELOW_MINIMUM, MINIMUM, REDUNDANT };

#if defined(_KERNEL)

#define	SPUR_TIMEOUT_USEC			1 * MICROSEC
#define	SPUR_LONG_TIMEOUT_USEC			5 * MICROSEC
#define	AC_TIMEOUT_USEC				1 * MICROSEC
#define	PS_FAIL_TIMEOUT_USEC			500 * (MICROSEC / MILLISEC)
#define	PPS_FAN_TIMEOUT_USEC			1 * MICROSEC

#define	BRD_INSERT_DELAY_USEC			500 * (MICROSEC / MILLISEC)
#define	BRD_INSERT_RETRY_USEC			5 * MICROSEC
#define	BRD_REMOVE_TIMEOUT_USEC			2 * MICROSEC
#define	BLINK_LED_TIMEOUT_USEC			300 * (MICROSEC / MILLISEC)
#define	KEYSWITCH_TIMEOUT_USEC			1

#define	PS_INSUFFICIENT_COUNTDOWN_SEC		30

/*
 * how many ticks to wait to register the state change
 * NOTE: ticks are measured in PS_FAIL_TIMEOUT_USEC clicks
 */
#define	PS_PRES_CHANGE_TICKS	1
#define	PS_FROM_BOOT_TICKS	1
#define	PS_FROM_UNKNOWN_TICKS	10
#define	PS_POWER_COUNTDOWN_TICKS 60

/* Note: this timeout needs to be longer than FAN_OK_TIMEOUT_USEC */
#define	PS_P_FAN_FROM_UNKNOWN_TICKS 15

#define	PS_FROM_OK_TICKS	1
#define	PS_PCH_FROM_OK_TICKS	3
#define	PS_FROM_FAIL_TICKS	4

/* NOTE: these ticks are measured in PPS_FAN_TIMEOUT_USEC clicks */
#define	PPS_FROM_FAIL_TICKS	7

/*
 * how many spurious interrupts to take during a SPUR_LONG_TIMEOUT_USEC
 * before complaining
 */
#define	MAX_SPUR_COUNT		2

/*
 * Global driver structure which defines the presence and status of
 * all board power supplies.
 */
struct ps_state {
	int pctr;			/* tick counter for presense deglitch */
	int dcctr;			/* tick counter for dc ok deglitch */
	enum e_pres_state pshadow;	/* presense shadow state */
	enum e_state dcshadow;		/* dc ok shadow state */
};

/*
 * for sysctrl_thread_wakeup()
 */
#define	OVERTEMP_POLL	1
#define	KEYSWITCH_POLL	2

/*
 * Structures used in the driver to manage the hardware
 * XXX will need to add a nodeid
 */
struct sysctrl_soft_state {
	dev_info_t *dip;		/* dev info of myself */
	dev_info_t *pdip;		/* dev info of parent */
	struct sysctrl_soft_state *next;
	int board;			/* Board number for this FHC */
	int mondo;			/* INO for this type of interrupt */
	u_char nslots;			/* bit encoding slots in this system */

	dnode_t options_nodeid;		/* for nvram powerfail-time */

	ddi_iblock_cookie_t iblock;	/* High level interrupt cookie */
	ddi_idevice_cookie_t idevice;	/* TODO - Do we need this? */
	ddi_softintr_t spur_id;		/* when we get a spurious int... */
	ddi_iblock_cookie_t spur_int_c;	/* spur int cookie */
	ddi_softintr_t spur_high_id;	/* when we reenable disabled ints */
	ddi_softintr_t spur_long_to_id;	/* long timeout softint */
	ddi_softintr_t ac_fail_id;	/* ac fail softintr id */
	ddi_softintr_t ac_fail_high_id;	/* ac fail re-enable softintr id */
	ddi_softintr_t ps_fail_int_id;	/* ps fail from intr softintr id */
	ddi_iblock_cookie_t ps_fail_c;	/* ps fail softintr cookie */
	ddi_softintr_t ps_fail_poll_id;	/* ps fail from polling softintr */
	ddi_softintr_t pps_fan_id;	/* pps fan fail softintr id */
	ddi_softintr_t pps_fan_high_id;	/* pps fan re-enable softintr id */
	ddi_softintr_t sbrd_pres_id;	/* sbrd softintr id */
	ddi_softintr_t sbrd_gone_id;	/* sbrd removed softintr id */
	ddi_softintr_t blink_led_id;	/* led blinker softint */
	ddi_iblock_cookie_t sys_led_c;	/* mutex cookie for sys LED lock */

	volatile u_char *clk_freq1;	/* Clock frequency reg. 1 */
	volatile u_char *clk_freq2;	/* Clock frequency reg. 2 */
	volatile u_char *status1;	/* System Status1 register */
	volatile u_char *status2;	/* System Status2 register */
	volatile u_char *ps_stat;	/* Power Supply Status register */
	volatile u_char *ps_pres;	/* Power Supply Presence register */
	volatile u_char	*pppsr;		/* 2nd Power Supply Status register */
	volatile u_char *temp_reg;	/* VA of temperature register */
	volatile u_char *rcons_ctl;	/* VA of Remote console UART */

	/* This mutex protects the following data */
	/* NOTE: *csr should only be accessed from interrupt level */
	kmutex_t csr_mutex;		/* locking for csr enable bits */
	volatile u_char *csr;		/* System Control Register */
	u_char pps_fan_saved;		/* cached pps fanfail state */
	u_char saved_en_state;		/* spurious int cache */
	int spur_count;			/* count multiple spurious ints */

	/* This mutex protects the following data */
	kmutex_t spur_int_lock;		/* lock spurious interrupt data */
	int spur_timeout_id;		/* quiet the int timeout id */
	int spur_long_timeout_id;	/* spurious long timeout interval */

	/* This mutex protects the following data */
	kmutex_t ps_fail_lock;		/* low level lock */
	struct ps_state ps_stats[SYS_PS_COUNT]; /* state struct for all ps */
	enum power_state power_state;	/* redundant power state */
	int power_countdown;		/* clicks until reboot */

	/* This mutex protects the following data */
	kmutex_t sys_led_lock;		/* low level lock */
	int sys_led;			/* on (TRUE) or off (FALSE) */
	int sys_fault;			/* on (TRUE) or off (FALSE) */

	/* various elements protected by their inherent access patterns */
	int pps_fan_external_state;	/* external state of the pps fans */
	int pps_fan_state_count[SYS_PPS_FAN_COUNT]; /* fan state counter */
	struct temp_stats tempstat;	/* in memory storage of temperature */
	enum keyswitch_state key_shadow; /* external state of the key switch */
};

/*
 * Kstat structures used to contain data which is requested by user
 * programs.
 */
struct sysctrl_kstat {
	struct kstat_named	csr;		/* system control register */
	struct kstat_named	status1;	/* system status 1 */
	struct kstat_named	status2;	/* system status 2 */
	struct kstat_named	clk_freq2;	/* Clock register 2 */
	struct kstat_named	fan_status;	/* shadow status 2 for fans */
	struct kstat_named	key_status;	/* shadow status for key */
	struct kstat_named	power_state;	/* redundant power status */
};

#endif /* _KERNEL */
#endif _ASM

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SYSCTRL_H */
