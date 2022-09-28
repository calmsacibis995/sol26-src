/*
 * Copyright (c) 1994-1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_FHC_H
#define	_SYS_FHC_H

#pragma ident	"@(#)fhc.h	1.37	96/08/13 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


/* useful debugging stuff */
#define	FHC_ATTACH_DEBUG	0x1
#define	FHC_INTERRUPT_DEBUG	0x2
#define	FHC_REGISTERS_DEBUG	0x4
#define	FHC_CTLOPS_DEBUG	0x8

/*
 * OBP supplies us with 6 register sets for the FHC. The code for the fhc
 * driver relies on these register sets being presented by the PROM in the
 * order specified below. If this changes, the following comments must be
 * revised and the code in fhc_init() must be changed to reflect these
 * revisions.
 *
 * They are:
 * 	0	FHC internal registers
 * 	1	IGR Interrupt Group Number
 *	2	FanFail IMR, ISMR
 *	3	System IMR, ISMR
 *	4	UART IMR, ISMR
 *	5	TOD IMR, ISMR
 */

/*
 * The offsets are defined as offsets from the base of the OBP register
 * set which the register belongs to.
 */

/* Register set 0 */
#define	FHC_OFF_ID		0x0	/* FHC ID register */
#define	FHC_OFF_RCTRL		0x10	/* FHC Reset Control and Status */
#define	FHC_OFF_CTRL		0x20	/* FHC Control and Status */
#define	FHC_OFF_BSR		0x30	/* FHC Board Status Register */
#define	FHC_OFF_JTAG_CTRL	0xF0	/* JTAG Control Register */
#define	FHC_OFF_JTAG_CMD	0x100	/* JTAG Comamnd Register */

/* Register sets 2-5, the ISMR offset is the same */
#define	FHC_OFF_ISMR		0x10	/* FHC Interrupt State Machine */

/* Bit field defines for FHC Control and Status Register */
#define	FHC_CENTERDIS		0x00100000
#define	FHC_MOD_OFF		0x00008000
#define	FHC_ACDC_OFF		0x00004000
#define	FHC_FHC_OFF		0x00002000
#define	FHC_EPDA_OFF		0x00001000
#define	FHC_EPDB_OFF		0x00000800
#define	FHC_PS_OFF		0x00000400
#define	FHC_NOT_BRD_PRES	0x00000200
#define	FHC_LED_LEFT		0x00000040
#define	FHC_LED_MID		0x00000020
#define	FHC_LED_RIGHT		0x00000010

/* Bit field defines for FHC Reset Control and Status Register */
#define	FHC_POR			0x80000000
#define	FHC_SOFT_POR		0x40000000
#define	FHC_SOFT_XIR		0x20000000

/* Bit field defines for the JTAG control register. */
#define	JTAG_MASTER_EN		0x80000000
#define	JTAG_MASTER_NPRES	0x40000000


/* Macros for decoding UPA speed pins from the Board Status Register */
#define	BSR0_TO_NSEC(bsr)	(15 - (((bsr) >> 10) & 0x7))
#define	CPU_0_SPEED(bsr)	(((20000 / BSR0_TO_NSEC(bsr)) + 5) / 10)

#define	BSR1_TO_NSEC(bsr)	(15 - (((bsr) >> 7) & 0x7))
#define	CPU_1_SPEED(bsr)	(((20000 / BSR1_TO_NSEC(bsr)) + 5) / 10)

/*
 * The following defines are used by the fhc driver to determine the
 * difference between IO and CPU type boards. This will be replaced
 * later by JTAG scan to determine board type.
 */

/* XXX */
#define	FHC_UPADATA64A		0x40000
#define	FHC_UPADATA64B		0x20000
/* XXX */

/* Bit field defines for Board Status Register */
#define	FHC_DIAG_MODE		0x40

/* Bit field defines for the FHC Board Status Register when on a disk board */
#define	FHC_FANFAIL		0x00000040
#define	FHC_SCSI_VDD_OK		0x00000001

/* Size of temperature recording array */
#define	MAX_TEMP_HISTORY	16

/* Maximum number of boards in system. */
#define	MAX_BOARDS		16

/* Maximum number of Board Power Supplies. */
#define	MAX_PS_COUNT	8

/* Use predefined strings to name the kstats from this driver. */
#define	FHC_KSTAT_NAME		"fhc"
#define	CSR_KSTAT_NAMED		"csr"
#define	BSR_KSTAT_NAMED		"bsr"

/*
 * The following defines are for the AC chip, but are needed to be global,
 * so have been put in the fhc header file.
 */

/*
 * Most Sunfire ASICs have the chip rev encoded into bits 31-28 of the
 * component ID register.
 */
#define	CHIP_REV(c)	((c) >> 28)

#ifndef _ASM

/* Use predefined strings to name the kstats from this driver. */

/* Bit field defines for Interrupt Mapping registers */
#define	IMR_VALID	((u_int)1 << INR_EN_SHIFT) /* Mondo valid bit */

/* Bit defines for Interrupt State Machine Register */
#define	INT_PENDING	3	/* state of the interrupt dispatch */

struct intr_regs {
	volatile u_int *mapping_reg;
	volatile u_int *clear_reg;
};

#define	BD_IVINTR_SHFT		0x7

/*
 * Convert the Board Number field in the FHC Board Status Register to
 * a board number. The field in the register is bits 0,3-1 of the board
 * number. Therefore a macro is necessary to extract the board number.
 */
#define	FHC_BSR_TO_BD(bsr)	((((bsr) >> 16) & 0x1)  | \
				(((bsr) >> 12) & 0xE))

#define	FHC_INO(ino) ((ino) & 0x7)

#define	FHC_MAX_INO	4

#define	FHC_SYS_INO		0x0
#define	FHC_UART_INO		0x1
#define	FHC_TOD_INO		0x2
#define	FHC_FANFAIL_INO		0x3

/*
 * Defines for the kstats created for passing temperature values and
 * history out to user level programs. All temperatures passed out
 * will be in degrees Centigrade, corrected for the board type the
 * temperature was read from. Since each Board type has a different
 * response curve for the A/D convertor, the temperatures are all
 * calibrated inside the kernel.
 */

#define	OVERTEMP_KSTAT_NAME	"temperature"

/*
 * Time averaging based method of recording temperature history.
 * Higher level temperature arrays are composed of temperature averages
 * of the array one level below. When the lower array completes a
 * set of data, the data is averaged and placed into the higher
 * level array. Then the lower level array is overwritten until
 * it is once again complete, where the process repeats.
 *
 * This method gives a user a fine grained view of the last minute,
 * and larger grained views of the temperature as one goes back in
 * time.
 *
 * The time units for the longer samples are based on the value
 * of the OVERTEMP_TIMEOUT_SEC and the number of elements in each
 * of the arrays between level 1 and the higher level.
 */

#define	OVERTEMP_TIMEOUT_SEC	2

/* definition of the clock board index */
#define	CLOCK_BOARD_INDEX	16

#define	L1_SZ		30	/* # of OVERTEMP_TIMEOUT_SEC samples */
#define	L2_SZ		15	/* size of array for level 2 samples */
#define	L3_SZ		12	/* size of array for level 3 samples */
#define	L4_SZ		4	/* size of array for level 4 samples */
#define	L5_SZ		2	/* size of array for level 5 samples */

/*
 * Macros for determining when to do the temperature averaging of arrays.
 */
#define	L2_INDEX(i)	((i) / L1_SZ)
#define	L2_REM(i)	((i) % L1_SZ)
#define	L3_INDEX(i)	((i) / (L1_SZ * L2_SZ))
#define	L3_REM(i)	((i) % (L1_SZ * L2_SZ))
#define	L4_INDEX(i)	((i) / (L1_SZ * L2_SZ * L3_SZ))
#define	L4_REM(i)	((i) % (L1_SZ * L2_SZ * L3_SZ))
#define	L5_INDEX(i)	((i) / (L1_SZ * L2_SZ * L3_SZ * L4_SZ))
#define	L5_REM(i)	((i) % (L1_SZ * L2_SZ * L3_SZ * L4_SZ))

/*
 * define for an illegal temperature. This temperature will never be seen
 * in a real system, so it is used as an illegal value in the various
 * functions processing the temperature data structure.
 */
#define	NA_TEMP		0x7FFF

/*
 * State variable for board temperature. Each board has its own
 * temperature state. State transitions from OK -> bad direction
 * happen instantaneously, but use a counter in the opposite
 * direction, so that noise in the A/D counters does not cause
 * a large number of messages to appear.
 */
enum temp_state {	TEMP_OK = 0,		/* normal board temperature */
			TEMP_WARN = 1,		/* start warning operator */
			TEMP_DANGER = 2 };	/* get ready to shutdown */

/*
 * Number of temperature poll counts to wait before printing that the
 * system has cooled down.
 */
#define	TEMP_STATE_TIMEOUT_SEC	20
#define	TEMP_STATE_COUNT	((TEMP_STATE_TIMEOUT_SEC) / \
				(OVERTEMP_TIMEOUT_SEC))

/*
 * Number of poll counts that a system temperature must be at or above danger
 * temperature before system is halted and powers down.
 */
#define	SHUTDOWN_TIMEOUT_SEC	20
#define	SHUTDOWN_COUNT		((SHUTDOWN_TIMEOUT_SEC) / \
				(OVERTEMP_TIMEOUT_SEC))

/*
 * State variable for temperature trend.  Each state represents the
 * current temperature trend for a given device.
 */
enum temp_trend {	TREND_UNKNOWN = 0,	/* Unknown temperature trend */
			TREND_RAPID_FALL = 1,	/* Rapidly falling temp. */
			TREND_FALL = 2,		/* Falling temperature */
			TREND_STABLE = 3,	/* Stable temperature */
			TREND_RISE = 4,		/* Rising temperature */
			TREND_RAPID_RISE = 5,   /* Rapidly rising temperature */
			TREND_NOISY = 6 };	/* Unknown trend (noisy) */

/* Thresholds for temperature trend */
#define	NOISE_THRESH		2
#define	RAPID_RISE_THRESH	4
#define	RAPID_FALL_THRESH	4

/*
 * Main structure for passing the calibrated and time averaged temperature
 * values to user processes. This structure is copied out via the kstat
 * mechanism.
 */
#define	TEMP_KSTAT_VERSION 2	/* version of temp_stats structure */
struct temp_stats {
	u_int index;		/* index of current temperature */
	short l1[L1_SZ];	/* OVERTEMP_TIMEOUT_SEC samples */
	short l2[L2_SZ];	/* level 2 samples */
	short l3[L3_SZ];	/* level 3 samples */
	short l4[L4_SZ];	/* level 4 samples */
	short l5[L5_SZ];	/* level 5 samples */
	short max;		/* maximum temperature recorded */
	short min;		/* minimum temperature recorded */
	enum temp_state state;	/* state of board temperature */
	int temp_cnt;		/* counter for state changes */
	int shutdown_cnt;	/* counter for overtemp shutdown */
	int version;		/* version of this structure */
	enum temp_trend trend;	/* temperature trend for board */
};

/*
 * Enumerated types for defining type and state of system and clock
 * boards. These are used by both the kernel and user programs.
 */
enum board_type {	UNINIT_BOARD = 0,	/* Uninitialized board type */
			UNKNOWN_BOARD,		/* Unknown board type */
			CPU_BOARD,		/* System board CPU(s) */
			MEM_BOARD,		/* System board no CPUs */
			IO_2SBUS_BOARD,		/* 2 SBus IO Board */
			IO_SBUS_FFB_BOARD,	/* SBus and FFB IO Board */
			IO_PCI_BOARD,		/* PCI IO Board */
			DISK_BOARD,		/* Disk Drive Board */
			CLOCK_BOARD };		/* System Clock board */

/*
 * Defined strings for comparing with OBP board-type property. If OBP ever
 * changes the board-type properties, these string defines must be changed
 * as well.
 */
#define	CPU_BD_NAME		"cpu"
#define	MEM_BD_NAME		"mem"
#define	IO_2SBUS_BD_NAME	"dual-sbus"
#define	IO_SBUS_FFB_BD_NAME	"sbus-upa"
#define	IO_PCI_BD_NAME		"dual-pci"
#define	DISK_BD_NAME		"disk"

/*
 * The board states are very important. They are explained below so there
 * is no confusion about them:
 *
 * UNKNOWN_STATE - This is a rare state, in case we detect a board but
 * cannot determine its state. This is currently unused. No softsp is
 * allocated for the board. We cannot access any registers on the board.
 *
 * ACTIVE_STATE - This is the state for all boards present at boot in a
 * working condition. This is the normal state for boards after the OS has
 * booted. If the board is not of type DISK_BOARD, it has a softsp for the
 * bd_list structure and registers can be accessed on the board.
 *
 * HOTPLUG_STATE - This is an intermediate state after a board is hotplugged.
 * The only way it can be accessed is via scan at this point. Later phases
 * of hotplug will have boards in this state which are under JTAG scan
 * testing and maybe even on the backplane and accessible by PIO.
 *
 * LOWPOWER_STATE - This is the state that hotplug boards are transitioned
 * to after all JTAG checking is done. No registers can be accessed. The
 * board LEDs are in the ready to remove state. It is in low power mode
 * and frozen off of the backplane.
 *
 * DISABLED_STATE - This is a board that has been disabled by the PROM
 * because it was listed in the 'disabled-board-list' property in the
 * /options node. The board LEDs are in the ready to remove state. It
 * is in low power mode and frozen off of the backplane.
 *
 * FAILED_STATE - This is a board that is failed by POST and powered off
 * in the testing phase of PROM operations. The board LEDs are in the
 * ready to remove state. It is in low power mode and frozen off of the
 * backplane. It can only be detected by the JTAG priming scan. It is not
 * present in the device tree or in the disabled board list.
 */

enum board_state {	UNKNOWN_STATE = 0,	/* Unknown board */
			ACTIVE_STATE,		/* active and working */
			HOTPLUG_STATE,		/* Hot plugged board */
			LOWPOWER_STATE, 	/* Powered down board */
			DISABLED_STATE,		/* Board disabled by PROM */
			FAILED_STATE };		/* Board failed by POST */

/*
 * The following defines and enum definitions have been created to support
 * the fault list (struct ft_list). These defines must match with the
 * fault string table in fhc.c. If any faults are added, they must be
 * added at the end of this list, and the table must be modified
 * accordingly.
 */
enum ft_type {
	FT_CORE_PS = 0,		/* Core power supply */
	FT_OVERTEMP,		/* Temperature */
	FT_AC_PWR,		/* AC power Supply */
	FT_PPS,			/* Perpheral Power Supply */
	FT_CLK_33,		/* System 3.3 Volt Power */
	FT_CLK_50,		/* System 5.0 Volt Power */
	FT_V5_P,		/* Peripheral 5V Power */
	FT_V12_P,		/* Peripheral 12V Power */
	FT_V5_AUX,		/* Auxiliary 5V Power */
	FT_V5_P_PCH,		/* Peripheral 5V Precharge */
	FT_V12_P_PCH,		/* Peripheral 12V Precharge */
	FT_V3_PCH,		/* System 3V Precharge */
	FT_V5_PCH,		/* System 5V Precharge */
	FT_PPS_FAN,		/* Perpheral Power Supply Fan */
	FT_RACK_EXH,		/* Rack Exhaust Fan */
	FT_DSK_FAN,		/* 4 Slot Disk Fan */
	FT_AC_FAN,		/* AC Box Fan */
	FT_KEYSW_FAN,		/* Key Switch Fan */
	FT_INSUFFICIENT_POWER,	/* System has insufficient power */
	FT_PROM,		/* fault inherited from PROM */
	FT_HOT_PLUG		/* hot plug unavailable */
};

enum ft_class {
	FT_BOARD,
	FT_SYSTEM
};

/*
 * This extern allows other drivers to use the ft_str_table if they
 * have fhc specified as a depends_on driver.
 */
extern char *ft_str_table[];

/*
 * The following structures and union are needed because the bd_info
 * structure describes all types of system boards.
 * XXX - We cannot determine Spitfire rev from JTAG scan, so it is
 * left blank for now. Future implementations might fill in this info.
 */
struct cpu_info {
	int cpu_rev;		/* CPU revision */
	int cpu_speed;		/* rated speed of CPU in MHz */
	int cpu_compid;		/* CPU component ID */
	int sdb0_compid;	/* SDB component ID */
	int sdb1_compid;	/* SDB component ID */
	int ec_compid;		/* Ecache RAM ID, needed for cache size */
	int cache_size;		/* Cache size in bytes */
	int mem0_size;		/* SIMM bank size in bytes */
	int mem1_size;		/* SIMM bank size in bytes */
};

struct io1_info {
	int sio0_compid;	/* Sysio component ID */
	int sio1_compid;	/* Sysio component ID */
	int hme_compid;		/* several revs in existence */
	int soc_compid;		/* SOC or SOC+ */
};

/* Defines for the FFB size field */
#define	FFB_FAILED	-1
#define	FFB_NOT_FOUND	0
#define	FFB_SINGLE	1
#define	FFB_DOUBLE	2

struct io2_info {
	int fbc_compid;		/* FBC component ID */
	int ffb_size;		/* not present, single or dbl buffered */
	int sio1_compid;	/* Sysio component ID */
	int hme_compid;		/* several revs in existence */
	int soc_compid;		/* SOC or SOC+ component ID */
};

/*
 * XXX -
 * This board is not designed yet, so this structure is just a guess.
 */
struct io3_info {
	int psy0_compid;
	int psy1_compid;
	int hme_compid;		/* several revs in existence */
	int soc_compid;		/* SOC or SOC+ component ID */
};

struct dsk_info {
	int disk_pres[2];
	int disk_id[2];
};

union bd_un {
	struct cpu_info cpu[2];
	struct io1_info io1;
	struct io2_info io2;
	struct io3_info io3;
	struct dsk_info dsk;
};

/*
 * The bd_info structure contains the information about hotplugged
 * boards needed by user programs. It is exported using a kstat.
 * There is some duplication of data between this and bd_list, but
 * this is necessary so as to not export kernel private data present
 * in the bd_list structure.
 */
struct bd_info {
	enum board_type type;		/* Type of board */
	enum board_state state;		/* current state of this board */
	int board;			/* board number */
	int fhc_compid;
	int ac_compid;
	char prom_rev[64];		/* best guess as to what is needed */
	union bd_un bd;
};

/* Macro for checking for newly created disabled board list entries. */
#define	UNCKECKED_DISABLED_BD(list)	\
	((list->info.state == DISABLED_STATE) && \
	(list->info.type == UNKNOWN_BOARD))

/* Maximum length of string table entries */
#define	MAX_FT_DESC	64

#define	FT_LIST_KSTAT_NAME	"fault_list"

/*
 * The fault list structure is a structure for holding information on
 * kernel detected faults. The fault list structures are linked into
 * a list and the list is protected by the ftlist_mutex. There are
 * also several routines for manipulating the fault list.
 */
struct ft_list {
	int unit;		/* unit number of faulting device */
	enum ft_type type;	/* type of faulting device */
	struct ft_list *next;
	enum ft_class class;	/* System or board class fault */
	time_t create_time;	/* Time stamp at fault detection */
	char msg[MAX_FT_DESC];	/* fault string */
};

#if defined(_KERNEL)

/*
 * In order to indicate that we are in an environmental chamber, or
 * oven, the test people will set the 'mfg-mode' property in the
 * options node to 'chamber'. Therefore we have the following define.
 */
#define	CHAMBER_VALUE	"chamber"

/*
 * zs design for fhc has two zs' interrupting on same interrupt mondo
 * This requires us to poll for zs and zs alone. The poll list has been
 * defined as a fixed size for simplicity.
 */
#define	MAX_ZS_CNT	2

/* FHC Interrupt routine wrapper structure */
struct fhc_wrapper_arg {
	struct fhc_soft_state *softsp;
	volatile u_int *clear_reg;
	volatile u_int *mapping_reg;
	dev_info_t *child;
	u_int (*funcp)();
	caddr_t arg;
};

/*
 * The JTAG master command structure. It contains the address of the
 * the JTAG controller on this system board. The controller can only
 * be used if this FHC holds the JTAG master signal. This is checked
 * by reading the JTAG control register on this FHC.
 */
struct jt_mstr {
	volatile u_int *jtag_cmd;
	int is_master;
	kmutex_t lock;
};

/*
 * Function shared with child drivers which require fhc
 * support. They gain access to this function through the use of the
 * _depends_on variable.
 */

/*
 * Board list manipulation functions. Certain functions require the
 * board list lock to be held before calling. Read the source for
 * each before using.
 */
struct bd_list *get_and_lock_bdlist(int board);
struct bd_list *get_next_bdlist(struct bd_list *);
struct bd_list *get_bdlist(int board);
void unlock_bdlist(void);
enum board_type get_board_type(int board);
struct bd_list *bdlist_add_board(int, enum board_type, enum board_state,
	struct fhc_soft_state *);
void bdlist_free_board(struct bd_list *);
struct jt_mstr *find_and_lock_jtag_master(void);
void release_jtag_master(struct jt_mstr *);

void update_temp(dev_info_t pdip, struct temp_stats *envstat, u_char value);
enum temp_trend temp_trend(struct temp_stats *);
void fhc_reboot(void);
int overtemp_kstat_update(kstat_t *ksp, int rw);
void init_temp_arrays(struct temp_stats *envstat);
void update_board_leds(struct bd_list *, u_int, u_int);

/* Functions exported to manage the fault list */
void reg_fault(int, enum ft_type, enum ft_class);
void clear_fault(int, enum ft_type, enum ft_class);
int process_fault_list(void);
void create_ft_kstats(int);

/* Structures used in the driver to manage the hardware */
struct fhc_soft_state {
	dev_info_t *dip;		/* dev info of myself */
	struct bd_list *list;		/* pointer to board list entry */
	int is_central;			/* A central space instance of FHC */
	volatile u_int *id;		/* FHC ID register */
	volatile u_int *rctrl;		/* FHC Reset Control and Status */
	volatile u_int *bsr;		/* FHC Board Status register */
	volatile u_int *jtag_ctrl;	/* JTAG Control register */
	volatile u_int *igr;		/* Interrupt Group Number */
	struct intr_regs intr_regs[FHC_MAX_INO];
	struct fhc_wrapper_arg poll_list[MAX_ZS_CNT];
	kmutex_t poll_list_lock;
	u_char spurious_zs_cntr;	/* Spurious counter for zs devices */
	kmutex_t pokefault_mutex;
	int pokefault;

	/* this lock protects the following data */
	/* ! non interrupt use only ! */
	kmutex_t ctrl_lock;		/* lock for access to FHC CSR */
	volatile u_int *ctrl;		/* FHC Control and Status */

	/* The JTAG master structure has internal locking */
	struct jt_mstr jt_master;
};


/*
 * The board list structure is the central storage for the kernel's
 * knowledge of normally booted and hotplugged boards. All Sunfire
 * drivers should use the normal interfaces to this lock protected
 * list.
 */
struct bd_list {
	struct fhc_soft_state *softsp;	/* handle for DDI soft state */
	struct bd_list *next;		/* next bd_list entry */
	struct bd_info info;
	struct kstat *ksp;		/* pointer used in kstat destroy */
	int fault;			/* failure on this board? */
};

/* FHC interrupt specification */
struct fhcintrspec {
	u_int mondo;
	u_int pil;
	dev_info_t *child;
	struct fhc_wrapper_arg *handler_arg;
};

/* kstat structure used by fhc to pass data to user programs. */
struct fhc_kstat {
	struct kstat_named csr;	/* FHC Control and Status Register */
	struct kstat_named bsr;	/* FHC Board Status Register */
};

#endif	/* _KERNEL */

#endif _ASM

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FHC_H */
