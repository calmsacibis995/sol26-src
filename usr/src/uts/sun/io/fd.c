/*
 * Copyright (c) 1989-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fd.c	1.89	96/10/24 SMI"

/*
 * Intel 82072/82077 Floppy Disk Driver
 */

/*
 * Notes
 *
 *	0. The driver supports 3 flavors of hardware design:
 *		"fd"		- sun4c	- 82072 with Auxio
 *		"fd"		- clone	- 82077 with sun4c style Auxio
 *		"SUNW,fdtwo"	- sun4m	- 82077 with sun4m style Auxio
 *		"fdthree"  - sun4u - 82077 with DMA
 *	   In addition it supports an apparent bug in some versions of
 *	   the 82077 controller.
 *
 *	1. The driver is mostly set up for multiple controllers, multiple
 *	drives. However- we *do* assume the use of the AUXIO register, and
 *	if we ever have > 1 fdc, we'll have to see what that means. This
 *	is all intrinsically machine specific, but there isn't much we
 *	can do about it.
 *
 *	2. The driver also is structured to deal with one drive active at
 *	a time. This is because the 82072 chip is known to be buggy with
 *	respect to overlapped seeks.
 *
 *	3. The high level interrupt code is in assembler, and runs in a
 *	sparc trap window. It acts as a pseudo-dma engine as well as
 *	handles a couple of other interrupts. When it gets its job done,
 *	it schedules a second stage interrupt (soft interrupt) which
 *	is then fielded here in fd_lointr.  When DMA is used, the fdintr_dma
 *	interrupt handler is used.
 *
 *	4. Nearly all locking is done on a lower level MUTEX_DRIVER
 *	mutex. The locking is quite conservative, and is generally
 *	established very close to any of the entries into the driver.
 *	There is nearly no locking done of the high level MUTEX_DRIVER
 *	mutex (which generally is a SPIN mutex because the floppy usually
 *	interrupts above LOCK_LEVEL). The assembler high level interrupt
 *	handler grabs the high level mutex, but the code in the driver
 *	here is especially structured to not need to do this.
 *
 *	5. Fdrawioctl commands that pass data are not optimized for
 *	speed. If they need to be faster, the driver structure will
 *	have to be redone such that fdrawioctl calls physio after
 *	cons'ing up a uio structure and that fdstart will be able
 *	to detect that a particular buffer is a 'special' buffer.
 *
 *	6. Removable media support is not complete.
 *
 */

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/open.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/stat.h>
#include <sys/autoconf.h>

#include <sys/dklabel.h>

#include <sys/vtoc.h>
#include <sys/dkio.h>
#include <sys/fdio.h>

#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/kstat.h>

/*
 * included to check for ELC or SLC which report floppy controller that
 */
#include <sys/cpu.h>

#include "sys/fdvar.h"
#include "sys/fdreg.h"


/*
 * Defines
 */
#define	KIOSP	KSTAT_IO_PTR(un->un_iostat)
#define	KIOIP	KSTAT_INTR_PTR(fdc->c_intrstat)
#define	MEDIUM_DENSITY	0x40
#define	SEC_SIZE_CODE	(fdctlr.c_csb->csb_unit]->un_chars->medium ? 3 : 2)
#define	CMD_READ	(MT + SK + FDRAW_RDCMD + MFM)
#define	CMD_WRITE	(MT + FDRAW_WRCMD + MFM)
#define	C		CE_CONT
#define	FD_POLLABLE_PROP	"pollable"	/* prom property */
#define	FD_MANUAL_EJECT		"manual"	/* prom property */
#define	FD_UNIT			"unit"		/* prom property */

/*
 * Sony MP-F17W-50D Drive Parameters
 *				High Capacity
 *	Capacity unformatted	2Mb
 *	Capacity formatted	1.47Mb
 *	Encoding method	 MFM
 *	Recording density	17434 bpi
 *	Track density		135 tpi
 *	Cylinders		80
 *	Heads			2
 *	Tracks			160
 *	Rotational speed	300 rpm
 *	Transfer rate		250/500 kbps
 *	Latency (average)	100 ms
 *	Access time
 *		Average		95 ms
 *		Track to track	3 ms
 *	Head settling time	15 ms
 *	Motor start time	500 ms
 *	Head load time		? ms
 */

/*
 * Character/block entry points function prototypes
 */
static int fd_open(dev_t *, int, int, cred_t *);
static int fd_close(dev_t, int, int, cred_t *);
static int fd_strategy(struct buf *);
static int fd_read(dev_t, struct uio *, cred_t *);
static int fd_write(dev_t, struct uio *, cred_t *);
static int fd_ioctl(dev_t, int, intptr_t, int, cred_t *, int *);
static int
fd_prop_op(dev_t, dev_info_t *, ddi_prop_op_t, int, char *, caddr_t, int *);

/*
 * Device operations (dev_ops) entries function prototypes
*/
static int fd_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
		void **result);
static int fd_attach(dev_info_t *, ddi_attach_cmd_t);
static int fd_detach(dev_info_t *, ddi_detach_cmd_t);

/*
 * Internal functions
 */
static int fd_attach_check_drive(struct fdctlr *fdc);
static int fd_attach_det_ctlr(dev_info_t *dip, struct fdctlr *fdc);
static int fd_attach_map_regs(dev_info_t *dip, struct fdctlr *fdc);
static int
fd_attach_register_interrupts(dev_info_t *dip, struct fdctlr *fdc,
				int *hard);
static int fd_build_label_vtoc(struct fdunit *, struct vtoc *);
static void fd_build_user_vtoc(struct fdunit *, struct vtoc *);
static int fdcheckdisk(struct fdctlr *fdc, int unit);
static int fd_check_media(dev_t dev, enum dkio_state state);
static void
fd_cleanup(dev_info_t *dip, struct fdctlr *fdc, int hard, int locks);
static void fdeject(struct fdctlr *, int unit);
static int fdexec(struct fdctlr *fdc, int flags);
static void fdexec_turn_on_motor(struct fdctlr *fdc, int flags,  u_int unit);
static int fdformat(struct fdctlr *fdc, int unit, int cyl, int hd);
static caddr_t fd_getauxiova();
static struct fdctlr *fd_getctlr(dev_t);
static void fdgetcsb(struct fdctlr *);
static int fdgetlabel(struct fdctlr *fdc, int unit);
enum dkio_state fd_get_media_state(struct fdctlr *, int);
static u_int fdintr_dma();
static int fd_isauxiodip(dev_info_t *, void *);
static u_int  fd_lointr(caddr_t arg);
static void fd_media_watch(caddr_t);
static int fd_part_is_open(struct fdunit *un, int part);
static int fdrawioctl(struct fdctlr *, int, intptr_t, int);
static int fdrecalseek(struct fdctlr *fdc, int unit, int arg, int execflg);
static int fdrecover(struct fdctlr *);
static void fdretcsb(struct fdctlr *);
static int fdreset(struct fdctlr *);
static int fdrw(struct fdctlr *fdc, int, int, int, int, int, caddr_t, u_int);
static void fdselect(struct fdctlr *fdc, int unit, int onoff);
static int fdsensedrv(struct fdctlr *fdc, int unit);
static int fdsense_chng(struct fdctlr *, int unit);
static void fdstart(struct fdctlr *);
static int fdstart_dma(register struct fdctlr *fdc, caddr_t addr, u_int len);
static int fd_unit_is_open(struct fdunit *);
static void fdunpacklabel(struct packed_label *, struct dk_label *);
static int fd_unbind_handle(struct fdctlr *);
static void fdwatch(caddr_t);
static void  set_rotational_speed(struct fdctlr *, int);

/*
 * External functions
 */
extern u_int fd_intr(caddr_t);	/* defined in fd_asm.s */
extern u_int fd_fastintr(void); /* defined in fd_asm.s */
extern void set_auxioreg();

extern void call_debug();


/*
 * bss (uninitialized data)
 */
struct	fdctlr	*fdctlrs;	/* linked list of controllers */

/*
 * initialized data
 */

static int fd_check_media_time = 5000000;	/* 5 second state check */
static int fd_pollable = 0;
static u_char rwretry = 10;
static u_char skretry = 5;
/* This variable allows the dynamic change of the burst size */
static int fd_burstsize = DCSR_BURST_0 | DCSR_BURST_1;

static struct driver_minor_data {
	char	*name;
	int	minor;
	int	type;
} fd_minor [] = {
	{ "a", 0, S_IFBLK},
	{ "b", 1, S_IFBLK},
	{ "c", 2, S_IFBLK},
	{ "a,raw", 0, S_IFCHR},
	{ "b,raw", 1, S_IFCHR},
	{ "c,raw", 2, S_IFCHR},
	{0}
};

/*
 * If the interrupt handler is invoked and no controllers expect an
 * interrupt, the kernel panics.  The following message is printed out.
 */
char *panic_msg = "fd_intr: unexpected interrupt\n";

/*
 * Specify/Configure cmd parameters
 */
static u_char fdspec[2] = { 0xc2, 0x33 };	/*  "specify" parameters */
static u_char fdconf[3] = { 0x64, 0x58, 0x00 }; /*  "configure" parameters */

/* When DMA is used, set the ND bit to 0 */
#define	SPEC_DMA_MODE	0x32

/*
 * default characteristics
 */
static struct fd_char fdtypes[] = {
	{	/* struct fd_char fdchar_1.7MB density */
		0,		/* medium */
		500,		/* transfer rate */
		80,		/* number of cylinders */
		2,		/* number of heads */
		512,		/* sector size */
		21,		/* sectors per track */
		-1,		/* (NA) # steps per data track */
	},
	{	/* struct fd_char fdchar_highdens */
		0, 		/* medium */
		500, 		/* transfer rate */
		80, 		/* number of cylinders */
		2, 		/* number of heads */
		512, 		/* sector size */
		18, 		/* sectors per track */
		-1, 		/* (NA) # steps per data track */
	},
	{	/* struct fd_char fdchar_meddens */
		1, 		/* medium */
		500, 		/* transfer rate */
		77, 		/* number of cylinders */
		2, 		/* number of heads */
		1024, 		/* sector size */
		8, 		/* sectors per track */
		-1, 		/* (NA) # steps per data track */
	},
	{	/* struct fd_char fdchar_lowdens  */
		0, 		/* medium */
		250, 		/* transfer rate */
		80, 		/* number of cylinders */
		2, 		/* number of heads */
		512, 		/* sector size */
		9, 		/* sectors per track */
		-1, 		/* (NA) # steps per data track */
	}
};


static int nfdtypes = sizeof (fdtypes) / sizeof (fdtypes[0]);



/*
 * Default Label & partition maps
 */

static struct packed_label fdlbl_high_21 = {
	{ "3.5\" floppy cyl 80 alt 0 hd 2 sec 21" },
	300,				/* rotations per minute */
	80,				/* # physical cylinders */
	0,				/* alternates per cylinder */
	1,				/* interleave factor */
	80,				/* # of data cylinders */
	0,				/* # of alternate cylinders */
	2,				/* # of heads in this partition */
	21,				/* # of 512 byte sectors per track */
	{
		{ 0, 79 * 2 * 21 },	/* part 0 - all but last cyl */
		{ 79, 1 * 2 * 21 },	/* part 1 - just the last cyl */
		{ 0, 80 * 2 * 21 },	/* part 2 - "the whole thing" */
	},
	{	0,			/* version */
		"",			/* volume label */
		3,			/* no. of partitions */
		{ 0 },			/* partition hdrs, sec 2 */
		{ 0 },			/* mboot info.  unsupported */
		VTOC_SANE,		/* verify vtoc sanity */
		{ 0 },			/* reserved space */
		0,			/* timestamp */
	},
};

static struct packed_label fdlbl_high_80 = {
	{ "3.5\" floppy cyl 80 alt 0 hd 2 sec 18" },
	300, 				/* rotations per minute */
	80, 				/* # physical cylinders */
	0, 				/* alternates per cylinder */
	1, 				/* interleave factor */
	80, 				/* # of data cylinders */
	0, 				/* # of alternate cylinders */
	2, 				/* # of heads in this partition */
	18, 				/* # of 512 byte sectors per track */
	{
		{ 0, 79 * 2 * 18 }, 	/* part 0 - all but last cyl */
		{ 79, 1 * 2 * 18 }, 	/* part 1 - just the last cyl */
		{ 0, 80 * 2 * 18 }, 	/* part 2 - "the whole thing" */
	},
	{	0,			/* version */
		"",			/* volume label */
		3,			/* no. of partitions */
		{ 0 },			/* partition hdrs, sec 2 */
		{ 0 },			/* mboot info.  unsupported */
		VTOC_SANE,		/* verify vtoc sanity */
		{ 0 },			/* reserved space */
		0,			/* timestamp */
	},
};

/*
 * A medium density diskette has 1024 byte sectors.  The dk_label structure
 * assumes a sector is DEVBSIZE (512) bytes.
 */
static struct packed_label fdlbl_medium_80 = {
	{ "3.5\" floppy cyl 77 alt 0 hd 2 sec 8" },
	360, 				/* rotations per minute */
	77, 				/* # physical cylinders */
	0, 				/* alternates per cylinder */
	1, 				/* interleave factor */
	77, 				/* # of data cylinders */
	0, 				/* # of alternate cylinders */
	2, 				/* # of heads in this partition */
	16, 				/* # of 512 byte sectors per track */
	{
		{ 0, 76 * 2 * 8 * 2 },  /* part 0 - all but last cyl */
		{ 76, 1 * 2 * 8 * 2 },  /* part 1 - just the last cyl */
		{ 0, 77 * 2 * 8 * 2 },  /* part 2 - "the whole thing" */
	},
	{	0,			/* version */
		"",			/* volume label */
		3,			/* no. of partitions */
		{ 0 },			/* partition hdrs, sec 2 */
		{ 0 },			/* mboot info.  unsupported */
		VTOC_SANE,		/* verify vtoc sanity */
		{ 0 },			/* reserved space */
		0,			/* timestamp */
	},
};

static struct packed_label fdlbl_low_80 = {
	{ "3.5\" floppy cyl 80 alt 0 hd 2 sec 9" },
	300, 				/* rotations per minute */
	80, 				/* # physical cylinders */
	0, 				/* alternates per cylinder */
	1, 				/* interleave factor */
	80, 				/* # of data cylinders */
	0, 				/* # of alternate cylinders */
	2, 				/* # of heads in this partition */
	9, 				/* # of 512 byte sectors per track */
	{
		{ 0, 79 * 2 * 9 }, 	/* part 0 - all but last cyl */
		{ 79, 1 * 2 * 9 }, 	/* part 1 - just the last cyl */
		{ 0, 80 * 2 * 9 }, 	/* part 2 - "the whole thing" */
	},
	{	0,			/* version */
		"",			/* volume label */
		3,			/* no. of partitions */
		{ 0 },			/* partition hdrs, sec 2 */
		{ 0 },			/* mboot info.  unsupported */
		VTOC_SANE,		/* verify vtoc sanity */
		{ 0 },			/* reserved space */
		0,			/* timestamp */
	},
};

static struct fdcmdinfo {
	char *cmdname;		/* command name */
	u_char ncmdbytes;	/* number of bytes of command */
	u_char nrsltbytes;	/* number of bytes in result */
	u_char cmdtype;		/* characteristics */
} fdcmds[] = {
	"", 0, 0, 0, 			/* - */
	"", 0, 0, 0, 			/* - */
	"read_track", 9, 7, 1, 		/* 2 */
	"specify", 3, 0, 3, 		/* 3 */
	"sense_drv_status", 2, 1, 3, 	/* 4 */
	"write", 9, 7, 1, 		/* 5 */
	"read", 9, 7, 1, 		/* 6 */
	"recalibrate", 2, 0, 2, 		/* 7 */
	"sense_int_status", 1, 2, 3, 	/* 8 */
	"write_del", 9, 7, 1, 		/* 9 */
	"read_id", 2, 7, 2, 		/* A */
	"motor_on/off", 1, 0, 4, 	/* B */
	"read_del", 9, 7, 1, 		/* C */
	"format_track", 10, 7, 1, 	/* D */
	"dump_reg", 1, 10, 4, 		/* E */
	"seek", 3, 0, 2, 		/* F */
	"", 0, 0, 0, 			/* - */
	"", 0, 0, 0, 			/* - */
	"", 0, 0, 0, 			/* - */
	"configure", 4, 0, 4, 		/* 13 */
	/* relative seek */
};

static struct cb_ops fd_cb_ops = {
	fd_open, 		/* open */
	fd_close, 		/* close */
	fd_strategy, 		/* strategy */
	nodev, 			/* print */
	nodev, 			/* dump */
	fd_read, 		/* read */
	fd_write, 		/* write */
	fd_ioctl, 		/* ioctl */
	nodev, 			/* devmap */
	nodev, 			/* mmap */
	nodev, 			/* segmap */
	nochpoll, 		/* poll */
	fd_prop_op, 		/* cb_prop_op */
	0, 			/* streamtab  */
	D_NEW | D_MP		/* Driver compatibility flag */
};

static struct dev_ops	fd_ops = {
	DEVO_REV, 		/* devo_rev, */
	0, 			/* refcnt  */
	fd_info, 		/* info */
	nulldev, 		/* identify */
	nulldev, 		/* probe */
	fd_attach, 		/* attach */
	fd_detach, 		/* detach */
	nodev, 			/* reset */
	&fd_cb_ops, 		/* driver operations */
	(struct bus_ops *)0,	/* bus operations */
	nulldev			/* power */
};


/*
 * error handling
 *
 * for debugging, set rwretry and skretry = 1
 *		set fderrlevel to 1
 *		set fderrmask  to 224  or 100644
 *
 * after debug set rwretry to 10, skretry to 5, and fderrlevel to 3
 * set fderrmask to FDEM_ALL
 * remove the define FD_DEBUG
 *
 */
static unsigned int fderrmask = (unsigned int)FDEM_ALL;
static int fderrlevel = 3;
static int tosec = 16;  /* long timeouts for sundiag for now */

/*
 * loadable module support
 */

#include <sys/modctl.h>

extern struct mod_ops mod_driverops;
static struct modldrv modldrv = {
	&mod_driverops, 		/* Type of module. driver here */
	"Floppy Driver v1.89", 	/* Name of the module. */
	&fd_ops, 		/* Driver ops vector */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modldrv,
	NULL
};

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

int
_fini(void)
{
	int e;

	if ((e = mod_remove(&modlinkage)) != 0)
		return (e);

	/* ddi_soft_state_fini() */
	return (0);
}

/* ARGSUSED */
static int
fd_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	struct 			fdctlr *fdc;
	struct 			driver_minor_data *dmdp;
	int			instance;
	int			hard_intr_set = 0;
	int 			unit;

	FDERRPRINT(FDEP_L1, FDEM_ATTA, (C, "fd_attach: start\n"));

	switch (cmd) {
		case DDI_ATTACH:
			break;
		case DDI_RESUME:


			instance = ddi_get_instance(dip);
			if (!(fdc = fd_getctlr(instance)))
				return (DDI_FAILURE);

			mutex_enter(&fdc->c_lolock);
			if (!fdc->c_suspended) {
				mutex_exit(&fdc->c_lolock);
				return (DDI_SUCCESS);
			}


			fdgetcsb(fdc);

			/* Reset the dma engine */
			if (fdc->c_fdtype & FDCTYPE_DMA) {
				Reset_dcsr(fdc);
				Set_dcsr(fdc, DCSR_INIT_BITS);
			}

			/* Reset and configure the controller */
			(void) fdreset(fdc);

			unit = fdc->c_un->un_unit_no;


			/* Recalibrate the drive */
			if (fdrecalseek(fdc, unit, -1, 0) != 0) {
				mutex_exit(&fdc->c_lolock);
				return (DDI_FAILURE);
			}


			/* Select the drive through the AUXIO registers */
			fdselect(fdc, unit, 0);

			fdc->c_suspended = 0;
			fdretcsb(fdc);
			mutex_exit(&fdc->c_lolock);
			return (DDI_SUCCESS);

		default:
			return (DDI_FAILURE);
	}


	/*
	 * Check for the pollable property
	 * A pollable floppy drive currently only exists on the
	 * Sparcstation Voyager.  This drive does not need to
	 * be turned on in order to sense whether or not a diskette
	 * is present.
	 */
	if (ddi_getprop(DDI_DEV_T_ANY, dip,
	    DDI_PROP_DONTPASS, FD_POLLABLE_PROP, 0))
		fd_pollable = 1;

	fdc = kmem_zalloc(sizeof (*fdc), KM_SLEEP);
	fdc->c_dip = dip;


	fdc->c_next = fdctlrs;
	fdctlrs = fdc;

	/* Determine which type of controller is present and initialize it */
	if (fd_attach_det_ctlr(dip, fdc) == DDI_FAILURE) {
		fd_cleanup(dip, fdc, hard_intr_set, 0);
		return (DDI_FAILURE);
	}



	/* Finish mapping the device registers & setting up structures */
	if (fd_attach_map_regs(dip, fdc) == DDI_FAILURE) {
		fd_cleanup(dip, fdc, hard_intr_set, 0);
		return (DDI_FAILURE);
	}


	/*
	 * Initialize the DMA limit structures if it's being used.
	 */
	if (fdc->c_fdtype & FDCTYPE_DMA) {
		fdc->c_fd_dma_lim.dma_attr_version = DMA_ATTR_V0;
		fdc->c_fd_dma_lim.dma_attr_addr_lo = 0x00000000ull;
		fdc->c_fd_dma_lim.dma_attr_addr_hi = 0xfffffffeull;
		fdc->c_fd_dma_lim.dma_attr_count_max = 0xffffff;
		fdc->c_fd_dma_lim.dma_attr_align = 1;
		fdc->c_fd_dma_lim.dma_attr_burstsizes = 0x0;
		fdc->c_fd_dma_lim.dma_attr_minxfer = 1;
		fdc->c_fd_dma_lim.dma_attr_maxxfer = 0xffff;
		fdc->c_fd_dma_lim.dma_attr_seg = 0xffff;
		fdc->c_fd_dma_lim.dma_attr_sgllen = 1;
		fdc->c_fd_dma_lim.dma_attr_granular = 512;


		if (ddi_dma_alloc_handle(dip, &fdc->c_fd_dma_lim,
				DDI_DMA_DONTWAIT, 0, &fdc->c_dmahandle)
				!= DDI_SUCCESS) {

			fd_cleanup(dip, fdc, hard_intr_set, 0);
			return (DDI_FAILURE);
		}
	}


	/* Register the interrupts */
	if (fd_attach_register_interrupts(dip, fdc,
			&hard_intr_set)
							== DDI_FAILURE) {
		fd_cleanup(dip, fdc, hard_intr_set, 0);
		FDERRPRINT(FDEP_L1, FDEM_ATTA,
			(C, "fd_attach: registering interrupts failed\n"));
		return (DDI_FAILURE);
	}


	/*
	 * set initial controller/drive/disk "characteristics/geometry"
	 *
	 * NOTE:  The driver only supports one floppy drive.  The hardware
	 * only supports one drive because there is only one auxio register
	 * for one drive.
	 */
	fdc->c_un = (struct fdunit *)
	    kmem_zalloc(sizeof (struct fdunit), KM_SLEEP);
	fdc->c_un->un_chars = (struct fd_char *)
	    kmem_alloc(sizeof (struct fd_char), KM_SLEEP);
	fdc->c_un->un_iostat = kstat_create("fd", 0, "fd0", "disk",
		KSTAT_TYPE_IO, 1, KSTAT_FLAG_PERSISTENT);
	if (fdc->c_un->un_iostat) {
		fdc->c_un->un_iostat->ks_lock = &fdc->c_lolock;
		kstat_install(fdc->c_un->un_iostat);
	}

	fdc->c_un->un_drive = (struct fd_drive *)
		kmem_zalloc(sizeof (struct fd_drive), KM_SLEEP);


	/* check for the manual eject property */
	if (ddi_getprop(DDI_DEV_T_ANY, dip,
	    DDI_PROP_DONTPASS, FD_MANUAL_EJECT, 0)) {

		fdc->c_un->un_drive->fdd_ejectable = 0;

	} else {
		/* an absense of the property indicates auto eject */

		fdc->c_un->un_drive->fdd_ejectable = -1;
	}

	FDERRPRINT(FDEP_L1, FDEM_ATTA,
			(C, "fd_attach: ejectable? %d\n",
				fdc->c_un->un_drive->fdd_ejectable));

	/*
	 * Check for the drive id.  If the drive id property doesn't exist
	 * then the drive id is set to 0
	 */
	fdc->c_un->un_unit_no = ddi_getprop(DDI_DEV_T_ANY, dip,
					DDI_PROP_DONTPASS, FD_UNIT, 0);

	FDERRPRINT(FDEP_L1, FDEM_ATTA,
			(C, "fd_attach: unit %d\n",
				fdc->c_un->un_unit_no));

	/* Initially set the characteristics to high density */
	fdc->c_un->un_curfdtype = 1;
	*fdc->c_un->un_chars = fdtypes[fdc->c_un->un_curfdtype];
	fdunpacklabel(&fdlbl_high_80, &fdc->c_un->un_label);

	/* Make sure drive is present */
	if (fd_attach_check_drive(fdc) == DDI_FAILURE) {
		fd_cleanup(dip, fdc, hard_intr_set, 1);
		return (DDI_FAILURE);
	}

	for (dmdp = fd_minor; dmdp->name != NULL; dmdp++) {
		if (ddi_create_minor_node(dip, dmdp->name, dmdp->type,
		    dmdp->minor, DDI_NT_FD, 0) == DDI_FAILURE) {
			fd_cleanup(dip, fdc, hard_intr_set, 1);
			return (DDI_FAILURE);
		}
	}


	/*
	 * Add a zero-length attribute to tell the world we support
	 * kernel ioctls (for layered drivers)
	 */
	(void) ddi_prop_create(DDI_DEV_T_NONE, dip, DDI_PROP_CANSLEEP,
		DDI_KERNEL_IOCTL, NULL, 0);

	ddi_report_dev(dip);

	FDERRPRINT(FDEP_L1, FDEM_ATTA,
			(C, "attached 0x%x\n", ddi_get_instance(dip)));

	return (DDI_SUCCESS);
}

/*
 * Finish mapping the registers and initializing structures
 */
static int
fd_attach_map_regs(dev_info_t *dip, struct fdctlr *fdc)
{
	ddi_device_acc_attr_t attr;

	attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
	attr.devacc_attr_endian_flags  = DDI_STRUCTURE_LE_ACC;
	attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;

	/* Map the DMA registers of the platform supports DMA */
	if (fdc->c_fdtype & FDCTYPE_DMA) {
		if (ddi_regs_map_setup(dip, 1, (caddr_t *)&fdc->c_dma_regs,
				0, sizeof (struct fdc_dma_reg),
				&attr,
				&fdc->c_handlep_dma)) {
			return (DDI_FAILURE);
		}

	/* Reset the DMA engine and enable floppy interrupts */
	Reset_dcsr(fdc);

	Set_dcsr(fdc, DCSR_INIT_BITS);

	}

	/* Finish initializing structures associated with the device regs */
	switch (fdc->c_fdtype & FDCTYPE_CTRLMASK) {
	case FDCTYPE_82072:

		FDERRPRINT(FDEP_L1, FDEM_ATTA, (C, "type is 82072\n"));
		/*
		 * Initialize addrs of key registers
		 */
		fdc->c_control =
		    (u_char *)&fdc->c_reg->fdc_82072_reg.fdc_control;
		fdc->c_fifo = (u_char *)&fdc->c_reg->fdc_82072_reg.fdc_fifo;
		fdc->c_dor = (u_char *) 0x0;
		fdc->c_dir = (u_char *) 0x0;
		break;

	case FDCTYPE_82077:
		FDERRPRINT(FDEP_L1, FDEM_ATTA, (C, "type is 82077\n"));
		/*
		 * Initialize addrs of key registers
		 */
		fdc->c_control =
		    (u_char *)&fdc->c_reg->fdc_82077_reg.fdc_control;
		fdc->c_fifo = (u_char *)&fdc->c_reg->fdc_82077_reg.fdc_fifo;
		fdc->c_dor = (u_char *)&fdc->c_reg->fdc_82077_reg.fdc_dor;
		fdc->c_dir = (u_char *)&fdc->c_reg->fdc_82077_reg.fdc_dir;


		FDERRPRINT(FDEP_L1, FDEM_ATTA, ((int)C,
			(char *)"fdattach: msr/dsr at 0x%x\n", fdc->c_control));

		/*
		 * The 82077 doesn't use the first configuration parameter
		 * so let's adjust that while we know we're an 82077.
		 */
		fdconf[0] = 0;
		break;
	default:
		break;
	}

	return (0);
}

/*
 * Determine which type of floppy controller is present and
 * initialize the registers accordingly
 */
static int
fd_attach_det_ctlr(dev_info_t *dip, struct fdctlr *fdc)
{
	u_char *tap;
	u_char tap0;
	u_char tap2;
	u_char tap4;


	ddi_device_acc_attr_t attr;
	attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
	/* DDI_NEVERSWAP_ACC since the controller has a byte interface. */
	attr.devacc_attr_endian_flags  = DDI_STRUCTURE_LE_ACC;
	attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;

	/*
	 * See if we have:
	 *
	 *	"fd"
	 *	No controller (SLC/ELC - device tree reports anyway)
	 *	Intel 82072 controller (sun4c)
	 *	Intel 82077 controller (sun4c clone)
	 *
	 *	"SUNW,fdtwo"
	 *	Intel/NCR 82077 controller
	 *
	 *	"SUNW,fdthree"
	 *	Intel/NCR 82077 compatible controller, Cheerio I/O chip
	 *	which performs DMA
	 *
	 * See if we have an 072 or 077.  If we have an 072 the
	 * DSR/MSR will be at location 0x2 since 0x2 maps (wraps) to 0x0.
	 * If we have an 077, the DOR will be at 0x2.
	 *
	 *	82072   82077   driver
	 *	0	msr/dsr statusa	tap[0]
	 *	1	fifo	statusb	tap[1]
	 *	2	msr/dsr	dor	tap[2]
	 *	3	fifo	tdr	tap[3]
	 *	4	msr/dsr	msr/dsr	tap[4]
	 */

	FDERRPRINT(FDEP_L1, FDEM_ATTA,
			    (C, "fdattach_det_cltr: start \n"));

	/*
	 * First, map in the controller's registers
	 * The controller has an 8-bit interface, so byte
	 * swapping isn't needed
	 */


	if (ddi_regs_map_setup(dip, 0, (caddr_t *)&fdc->c_reg,
				0, sizeof (union fdcreg),
				&attr,
				&fdc->c_handlep_cont)) {
			return (DDI_FAILURE);
	}

	FDERRPRINT(FDEP_L1, FDEM_ATTA,
			    (C, "fdattach_det_cltr: mapped floppy regs\n"));


	/*
	 * Set platform specific characteristics based on the device-tree
	 * node name.
	 */

	if (strcmp(ddi_get_name(dip), "SUNW,fdtwo") == 0) {
		fdc->c_fdtype |= FDCTYPE_SLAVIO;
		fdc->c_fdtype |= FDCTYPE_82077;
		fdc->c_auxiova = fd_getauxiova();
		fdc->c_auxiodata = (u_char)(AUX_MBO4M|AUX_TC4M);
		fdc->c_auxiodata2 = (u_char)AUX_TC4M;
		FDERRPRINT(FDEP_L1, FDEM_ATTA,
			    (C, "fdattach: slavio will be used!\n"));


	} else if (strcmp(ddi_get_name(dip), "fdthree") == 0) {
		fdc->c_fdtype |= FDCTYPE_CHEERIO;
		fdc->c_fdtype |= FDCTYPE_82077;

		FDERRPRINT(FDEP_L1, FDEM_ATTA,
			    (C, "fdattach: cheerio will be used!\n"));
		/*
		 * The cheerio auxio register should be memory mapped.  The
		 * auxio register on other platforms is shared and mapped
		 * elswhere in the kernel
		 */
		if (ddi_regs_map_setup(dip, 2,
			(caddr_t *)&fdc->c_auxio_reg,
				0, sizeof (u_long), &attr,
				&fdc->c_handlep_aux)) {
				return (DDI_FAILURE);

		}

		/*
		 * The driver assumes high density characteristics until
		 * the diskette is looked at.
		 */
		Set_auxio(fdc, AUX_HIGH_DENSITY);
		FDERRPRINT(FDEP_L1, FDEM_ATTA,
			    (C, "fdattach: auxio register 0x%x\n",
				 *fdc->c_auxio_reg));

		fdc->c_fdtype |= FDCTYPE_DMA;
		FDERRPRINT(FDEP_L1, FDEM_ATTA, (C, "fd_attach: DMA used\n"));

	} else if (strcmp(ddi_get_name(dip), "fd") == 0) {

		/*
		 * sun4c clones may have either an 82072 or an 82077 type
		 * controller. Figure out which one is present.
		 */


		tap = (u_char *) fdc->c_reg;

		ddi_putb(fdc->c_handlep_cont, (uchar_t *)&tap[4], 0);
		drv_usecwait(100);

		tap0 = ddi_getb(fdc->c_handlep_cont, (uchar_t *)&tap[0]);
		tap2 = ddi_getb(fdc->c_handlep_cont, (uchar_t *)&tap[2]),
		tap4 = ddi_getb(fdc->c_handlep_cont, (uchar_t *)&tap[4]);

		FDERRPRINT(FDEP_L1, FDEM_ATTA,
		(C, "fdattach_det_cltr: tap[0] 0x%x tap[2] 0x%x tap[4] 0x%x\n",
		tap0, tap2, tap4));


		if ((tap0 == tap2) && (tap2 == tap4) &&
			((tap2 & (RQM|CB)) == RQM))  {
			fdc->c_fdtype |= FDCTYPE_82072;
		} else if ((tap2 != tap4) &&
			((tap4 & (RQM|CB)) == RQM)) {
			fdc->c_fdtype |= FDCTYPE_82077;
		}


		/* See if it's in SLC/ELC */
		if (fdc->c_fdtype != 0) {
			fdc->c_fdtype |= FDCTYPE_MACHIO;
			fdc->c_auxiova = fd_getauxiova();
			fdc->c_auxiodata = (u_char)(AUX_MBO|AUX_TC);
			fdc->c_auxiodata2 = (u_char)AUX_TC;

			FDERRPRINT(FDEP_L1, FDEM_ATTA,
			    (C, "fdattach: machio will be used!\n"));

			/* support for possibly buggy 82077 on sun4c */
			if (fdc->c_fdtype & FDCTYPE_82077)
				fdc->c_fdtype |= FDCTYPE_TCBUG;
		} else {
			FDERRPRINT(FDEP_L1, FDEM_ATTA,
			    (C, "fdattach: MSR=%x\n", tap[4]));
		}
	}


	if (fdc->c_fdtype == 0) {
		FDERRPRINT(FDEP_L1, FDEM_ATTA,
			    (C, "fdattach: no controller!\n"));
		return (DDI_FAILURE);
	} else {
		return (0);
	}




}


/*
 * Register the floppy interrupts
 */
static int
fd_attach_register_interrupts(dev_info_t *dip, struct fdctlr *fdc,
				int *hard)
{
	ddi_iblock_cookie_t  iblock_cookie_soft;
	int status;




	/*
	 * First call ddi_get_iblock_cookie() to retrieve the
	 * the interrupt block cookie so that the mutexes may
	 * be initialized before adding the interrupt.  If the
	 * mutexes are initialized after adding the interrupt, there
	 * could be a race condition.
	 */
	if (ddi_get_iblock_cookie(dip, 0, &fdc->c_block) != DDI_SUCCESS) {
		FDERRPRINT(FDEP_L1, FDEM_ATTA,
		(C, "fdattach: ddi_get_iblock_cookie failed\n"));
		return (DDI_FAILURE);

	}


	/* Initalize high level mutex */
	mutex_init(&fdc->c_hilock, "fdh",
		MUTEX_DRIVER, fdc->c_block);


	/*
	 * Try to register fast trap handler, if unable try standard
	 * interrupt handler, else bad
	 */

	if (fdc->c_fdtype & FDCTYPE_DMA) {
		if (ddi_add_intr(dip, 0, &fdc->c_block, 0,
			    fdintr_dma, (caddr_t)0) == DDI_SUCCESS) {
				FDERRPRINT(FDEP_L1, FDEM_ATTA,
				(C, "fdattach: standard intr\n"));

				/*
				 * When DMA is used, the low level lock
				 * is used in the hard interrupt handler.
				 */
				mutex_init(&fdc->c_lolock, "fdl",
					MUTEX_DRIVER, fdc->c_block);

				*hard = 1;
		} else {
			FDERRPRINT(FDEP_L1, FDEM_ATTA,
			(C, "fdattach: can't add dma intr\n"));

			mutex_destroy(&fdc->c_hilock);

			return (DDI_FAILURE);
		}
	} else {


		/*
		 * Platforms that don't support DMA have both hard
		 * and soft interrupts.
		 * First attempt to add a fast interrupt on those platforms
		 * that support fast traps.
		 */
		if (ddi_add_fastintr(dip, 0, &fdc->c_block, 0,
				fd_fastintr) == DDI_SUCCESS) {
			FDERRPRINT(FDEP_L1, FDEM_ATTA,
			(C, "fdattach: fast intr\n"));
			*hard = 1;

			/* fast traps are enabled */
			fdc->c_fasttrap = 1;

		} else if (ddi_add_intr(dip, 0, &fdc->c_block, 0,
			fd_intr, (caddr_t)0) == DDI_SUCCESS) {
				FDERRPRINT(FDEP_L1, FDEM_ATTA,
				(C, "fdattach: standard intr\n"));
				*hard = 1;

			/* fast traps are not enabled */
			fdc->c_fasttrap = 0;

		} else {
			FDERRPRINT(FDEP_L1, FDEM_ATTA,
			(C, "fdattach: can't add intr\n"));


			mutex_destroy(&fdc->c_hilock);

			return (DDI_FAILURE);
		}


		/*
		 * Initialize the soft interrupt handler.  First call
		 * ddi_get_soft_iblock_cookie() so that the mutex may
		 * be initialized before the handler is added.
		 */
		status = ddi_get_soft_iblock_cookie(dip, DDI_SOFTINT_LOW,
				&iblock_cookie_soft);


		if (status != DDI_SUCCESS) {
			mutex_destroy(&fdc->c_hilock);
			return (DDI_FAILURE);
		}

		/*
		 * Initialize low level mutex which is used in the soft
		 * interrupt handler
		 */
		mutex_init(&fdc->c_lolock, "fdl", MUTEX_DRIVER,
			iblock_cookie_soft);

		if (ddi_add_softintr(dip, DDI_SOFTINT_LOW, &fdc->c_softid,
					NULL, NULL,
					fd_lointr,
					(caddr_t)fdc) != DDI_SUCCESS) {

			mutex_destroy(&fdc->c_hilock);
			mutex_destroy(&fdc->c_lolock);

			return (DDI_FAILURE);
		}
	}

	fdc->c_intrstat = kstat_create("fd", 0, "fdc0", "controller",
		KSTAT_TYPE_INTR, 1, KSTAT_FLAG_PERSISTENT);
	if (fdc->c_intrstat) {
		fdc->c_hiintct = &KIOIP->intrs[KSTAT_INTR_HARD];
		kstat_install(fdc->c_intrstat);
	}

	/* condition variable to wait on while an io transaction occurs */
	cv_init(&fdc->c_iocv, "fdio", CV_DRIVER, NULL);

	/* condition variable for the csb */
	cv_init(&fdc->c_csbcv, "fdcsb", CV_DRIVER, NULL);

	/* condition variable for motor on waiting period */
	cv_init(&fdc->c_motoncv, "fdmoton", CV_DRIVER, NULL);

	/* semaphore to serialize opens and closes */
	sema_init(&fdc->c_ocsem, 1, "fdoc", SEMA_DRIVER, NULL);

	return (0);

}

/*
 * Make sure the drive is present
 * 	- acquires the low level lock
 */
static int
fd_attach_check_drive(struct fdctlr *fdc)
{
	int tmp_fderrlevel;
	int timeid;
	int unit = fdc->c_un->un_unit_no;

	FDERRPRINT(FDEP_L1, FDEM_ATTA,
			(C, "fd_attach_check_drive\n"));


	mutex_enter(&fdc->c_lolock);
	switch (fdc->c_fdtype & FDCTYPE_CTRLMASK) {

	/* insure that the eject line is reset */
	case FDCTYPE_82072:
		set_auxioreg(AUX_EJECT, 1);
		if ((fdc->c_fdtype & FDCTYPE_TCBUG) == 0)
			set_auxioreg(AUX_TC, 0);
		break;

	case FDCTYPE_82077:

		/*
		 * Everything but the motor enable, drive select,
		 * and reset bits are turned off.  These three
		 * bits remain as they are.
		 */
		/* LINTED */
		Set_dor(fdc, ~((MOTEN(unit))|DRVSEL|RESET), 0);

		FDERRPRINT(FDEP_L1, FDEM_ATTA,
			(C, "fdattach: Dor 0x%x\n", Dor(fdc)));

		drv_usecwait(5);
		if (unit == 0) {
			/* LINTED */
			Set_dor(fdc, RESET|DRVSEL, 1);
		} else {

			/* LINTED */
			Set_dor(fdc, DRVSEL, 0);
			/* LINTED */
			Set_dor(fdc, RESET, 1);
		}

		drv_usecwait(5);

		FDERRPRINT(FDEP_L1, FDEM_ATTA,
			(C, "fdattach: Dor 0x%x\n", Dor(fdc)));

		if (!(fdc->c_fdtype & FDCTYPE_CHEERIO)) {
			set_auxioreg(AUX_TC4M, 0);
		}
		break;
	default:
		break;
	}


	fdgetcsb(fdc);
	if (fdreset(fdc) != 0) {
		mutex_exit(&fdc->c_lolock);
		return (DDI_FAILURE);
	}


	/* check for drive present */

	tmp_fderrlevel = fderrlevel;


	fderrlevel = FDEP_LMAX;

	FDERRPRINT(FDEP_L1, FDEM_ATTA,
			(C, "fdattach: call fdrecalseek\n"));

	/* Make sure the drive is present */
	if (fdrecalseek(fdc, unit, -1, 0) != 0) {
		fderrlevel = tmp_fderrlevel;

		timeid = fdc->c_mtimeid;
		fdc->c_mtimeid = 0;
		mutex_exit(&fdc->c_lolock);


		/* Do not hold the mutex over the call to untimeout */
		(void) untimeout(timeid);

		FDERRPRINT(FDEP_L2, FDEM_ATTA,
		    (C, "fd_attach: no drive?\n"));

		return (DDI_FAILURE);
	}

	fderrlevel = tmp_fderrlevel;

	fdselect(fdc, unit, 0);    /* deselect drive zero (used in fdreset) */
	fdretcsb(fdc);
	mutex_exit(&fdc->c_lolock);

	return (0);
}

/*
 * Clean up routine used by fd_detach and fd_attach
 *
 * Note: if the soft id is non-zero, then ddi_add_softintr() completed
 * successfully.  I can not make the same assumption about the iblock_cookie
 * for the high level interrupt handler.  So, the hard parameter indicates
 * whether or not a high level interrupt handler has been added.
 *
 * If the locks parameter is nonzero, then all mutexes, semaphores and condition
 * variables will be destroyed.
 *
 * Does not assume the low level mutex is held.
 *
 */
static void
fd_cleanup(dev_info_t *dip, struct fdctlr *fdc, int hard, int locks)
{

	if (fdc == NULL) {
		return;
	}


	FDERRPRINT(FDEP_L1, FDEM_ATTA,
				(C, "fd_cleanup\n"));

	/*
	 * Remove interrupt handlers first before anything else
	 * is deallocated.
	 */

	/* Remove hard interrupt if one is registered */
	if (hard) {
		ddi_remove_intr(dip, (u_int)0, fdc->c_block);
	}

	/* Remove soft interrupt if one is registered */
	if (fdc->c_softid != NULL)
		ddi_remove_softintr(fdc->c_softid);


	/* Remove memory handles */
	if (fdc->c_handlep_cont)
		ddi_regs_map_free(&fdc->c_handlep_cont);

	if (fdc->c_handlep_dma)
		ddi_regs_map_free(&fdc->c_handlep_dma);

	if (fdc->c_dmahandle != NULL)
		ddi_dma_free_handle(&fdc->c_dmahandle);


	/* Remove timers */
	if (fdc->c_mtimeid)
		(void) untimeout(fdc->c_mtimeid);
	if (fdc->c_timeid)
		(void) untimeout(fdc->c_timeid);


	/* Remove all minor nodes */
	ddi_remove_minor_node(dip, NULL);


	/* Remove unit structure if one exists */
	if (fdc->c_un != (struct fdunit *)NULL) {

		ASSERT(!mutex_owned(&fdc->c_lolock));

		if (fdc->c_un->un_iostat)
			kstat_delete(fdc->c_un->un_iostat);
		fdc->c_un->un_iostat = NULL;

		if (fdc->c_un->un_chars)
			kmem_free(fdc->c_un->un_chars,
				sizeof (struct fd_char));

		kmem_free((caddr_t)fdc->c_un, sizeof (struct fdunit));
	}

	if (fdc->c_intrstat) {
		FDERRPRINT(FDEP_L1, FDEM_ATTA,
				(C, "fd_cleanup: delete intrstat\n"));

		kstat_delete(fdc->c_intrstat);
	}

	fdc->c_intrstat = NULL;

	if (locks) {
		cv_destroy(&fdc->c_iocv);
		cv_destroy(&fdc->c_csbcv);
		cv_destroy(&fdc->c_motoncv);
		sema_destroy(&fdc->c_ocsem);
		mutex_destroy(&fdc->c_hilock);
		mutex_destroy(&fdc->c_lolock);
	}


	fdctlrs = fdc->c_next;
	kmem_free(fdc, sizeof (*fdc));


}


static int
fd_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	int instance = ddi_get_instance(devi);
	struct fdctlr *fdc = fd_getctlr(instance);

	FDERRPRINT(FDEP_L1, FDEM_ATTA, (C, "fd_detach\n"));

	switch (cmd) {

	case DDI_DETACH:
		/*
		 * The hard parameter is set to 1.  If detach is called, then
		 * attach must have passed meaning that the high level
		 * interrupt handler was successfully added.
		 * Similarly, the locks parameter is also set to 1.
		 */
		fd_cleanup(devi, fdc, 1, 1);

		ddi_prop_remove_all(devi);

		return (DDI_SUCCESS);

	case DDI_SUSPEND:
		if (!fdc)
			return (DDI_FAILURE);
		fdc->c_suspended = 1;		/* Must be before mutex */
		mutex_enter(&fdc->c_lolock);
		fdgetcsb(fdc);			/* Wait for I/O to finish */
		fdretcsb(fdc);
		mutex_exit(&fdc->c_lolock);
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}
}

/* ARGSUSED */
static int
fd_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	register struct fdctlr *fdc;
	register int error;

	switch (infocmd) {

	case DDI_INFO_DEVT2DEVINFO:
		if ((fdc = fd_getctlr((dev_t)arg)) == NULL) {
			error = DDI_FAILURE;
		} else {
			*result = fdc->c_dip;
			error = DDI_SUCCESS;
		}
		break;

	case DDI_INFO_DEVT2INSTANCE:
		*result = 0;
		error = DDI_SUCCESS;
		break;

	default:
		error = DDI_FAILURE;
	}
	return (error);
}


/*
 * property operation routine.	return the number of blocks for the partition
 * in question or forward the request to the propery facilities.
 */
static int
fd_prop_op(dev_t dev, dev_info_t *dip, ddi_prop_op_t prop_op, int flags,
    char *name, caddr_t valuep, int *lengthp)
{
	if (strcmp(name, "nblocks") == 0) {
		struct dk_map *lp;
		int nblocks, length, km_flags;
		caddr_t buffer;
		register struct fdunit *un;
		register struct fdctlr *fdc;

		fdc = fd_getctlr(dev);
		if (!fdc || !(un = fdc->c_un))
			return (DDI_PROP_NOT_FOUND);

		/*
		 * If the diskette has been opened, the label has been
		 * read.  Return the number of blocks if the label has
		 * been opened.
		 */

		if (fd_unit_is_open(fdc->c_un)) {
			lp = &un->un_label.dkl_map[PARTITION(dev)];
			nblocks = (int)lp->dkl_nblk;
		} else {
			nblocks = -1;
		}

		FDERRPRINT(FDEP_L1, FDEM_OPEN,
			(C, "fd_prop_op: no. blocks is %d\n", nblocks));

		/*
		 * get callers length set return length.
		 */
		length = *lengthp;		/* Get callers length */
		*lengthp = sizeof (int);	/* Set callers length */

		/*
		 * If length only request or prop length == 0, get out now.
		 * (Just return length, no value at this level.)
		 */
		if (prop_op == PROP_LEN)  {
			*lengthp = sizeof (int);
			return (DDI_PROP_SUCCESS);
		}

		/*
		 * Allocate buffer, if required.  Either way,
		 * set `buffer' variable.
		 */
		switch (prop_op)  {
		case PROP_LEN_AND_VAL_ALLOC:
			km_flags = KM_NOSLEEP;
			if (flags & DDI_PROP_CANSLEEP)
				km_flags = KM_SLEEP;
			buffer = kmem_alloc(sizeof (int), km_flags);
			if (buffer == NULL)  {
				return (DDI_PROP_NO_MEMORY);
			}
			*(caddr_t *)valuep = buffer; /* Set callers buf ptr */
			break;

		case PROP_LEN_AND_VAL_BUF:
			if (sizeof (int) > (length))
				return (DDI_PROP_BUF_TOO_SMALL);
			buffer = valuep; /* get callers buf ptr */
			break;
		}
		*((int *)buffer) = nblocks;
		return (DDI_PROP_SUCCESS);
	}
	/*
	 * not mine, pass it on.
	 */
	return (ddi_prop_op(dev, dip, prop_op, flags, name, valuep, lengthp));
}

/* ARGSUSED3 */
static int
fd_open(dev_t *devp, int flag, int otyp, cred_t *cred_p)
{
	dev_t dev;
	int  part;
	struct fdctlr *fdc;
	struct fdunit *un;
	struct dk_map *dkm;
	u_char	pbit;
	int	err, part_is_open;
	int 	unit;

	dev = *devp;
	fdc = fd_getctlr(dev);
	if ((fdc == NULL) || ((un = fdc->c_un) == NULL)) {
		return (ENXIO);
	}

	unit = fdc->c_un->un_unit_no;

	/*
	 * Serialize opens/closes
	 */

	sema_p(&fdc->c_ocsem);

	/* check partition */
	part = PARTITION(dev);
	pbit = 1 << part;
	dkm = &un->un_label.dkl_map[part];
	if (dkm->dkl_nblk == 0) {

		sema_v(&fdc->c_ocsem);
		return (ENXIO);
	}

	FDERRPRINT(FDEP_L1, FDEM_OPEN,
	    (C, "fdopen: ctlr %d unit %d part %d\n",
	    ddi_get_instance(fdc->c_dip), unit, part));

	FDERRPRINT(FDEP_L1, FDEM_OPEN,
	    (C, "fdopen: flag 0x%x", flag));


	/*
	 * Insure that drive is present with a recalibrate on first open.
	 */
	mutex_enter(&fdc->c_lolock);
	if (fdc->c_suspended) {
		mutex_exit(&fdc->c_lolock);
		ddi_dev_is_needed(fdc->c_dip, 0, 1);
		mutex_enter(&fdc->c_lolock);
	}
	if (fd_unit_is_open(un) == 0) {
		fdgetcsb(fdc);
		/*
		 * no check changed!
		 */
		err = fdrecalseek(fdc, unit, -1, 0);
		fdretcsb(fdc);
		if (err) {
			FDERRPRINT(FDEP_L3, FDEM_OPEN,
			    (C, "fd%d: drive not ready\n", 0));
			/* deselect drv on last close */
			fdselect(fdc, unit, 0);
			mutex_exit(&fdc->c_lolock);
			sema_v(&fdc->c_ocsem);
			return (ENXIO);
		}
	}

	/*
	 * Check for previous exclusive open, or trying to exclusive open
	 */
	if (otyp == OTYP_LYR) {
		part_is_open = (un->un_lyropen[part] != 0);
	} else {
		part_is_open = fd_part_is_open(un, part);
	}
	if ((un->un_exclmask & pbit) || ((flag & FEXCL) && part_is_open)) {
		mutex_exit(&fdc->c_lolock);
		sema_v(&fdc->c_ocsem);
		FDERRPRINT(FDEP_L2, FDEM_OPEN, (C, "fd:just return\n"));
		return (EBUSY);
	}

	/* don't attempt access, just return successfully */
	if (flag & (FNDELAY | FNONBLOCK)) {
		FDERRPRINT(FDEP_L2, FDEM_OPEN,
		    (C, "fd: return busy..\n"));
		goto out;
	}

	fdc->c_csb.csb_unit = (u_char)unit;
	if (fdgetlabel(fdc, unit)) {
		/* didn't find label (couldn't read anything) */
		FDERRPRINT(FDEP_L3, FDEM_OPEN,
		    (C,
		    "fd%d: unformatted diskette or no diskette in the drive\n",
		    0));
		if (fd_unit_is_open(un) == 0) {
			/* deselect drv on last close */
			fdselect(fdc, unit, 0);
		}

		mutex_exit(&fdc->c_lolock);
		sema_v(&fdc->c_ocsem);
		return (EIO);
	}

	/*
	 * if opening for writing, check write protect on diskette
	 */
	if (flag & FWRITE) {
		fdgetcsb(fdc);
		err = fdsensedrv(fdc, unit) & WP_SR3;
		fdretcsb(fdc);
		if (err) {
			if (fd_unit_is_open(un) == 0)
				fdselect(fdc, unit, 0);
			mutex_exit(&fdc->c_lolock);
			sema_v(&fdc->c_ocsem);
			return (EROFS);
		}
	}

out:
	/*
	 * mark open as having succeeded
	 */
	if (flag & FEXCL) {
		un->un_exclmask |= pbit;
	}
	if (otyp == OTYP_LYR) {
		un->un_lyropen[part]++;
	} else {
		un->un_regopen[otyp] |= pbit;
	}
	mutex_exit(&fdc->c_lolock);
	sema_v(&fdc->c_ocsem);
	return (0);
}
/*
 * fd_part_is_open
 *	return 1 if the partition is open
 *	return 0 otherwise
 */
static int
fd_part_is_open(struct fdunit *un, int part)
{
	int i;
	for (i = 0; i < OTYPCNT - 1; i++)
		if (un->un_regopen[i] & (1 << part))
			return (1);
	return (0);
}


/* ARGSUSED */
static int
fd_close(dev_t dev, int flag, int otyp, cred_t *cred_p)
{
	int unit, part_is_closed, part;
	register struct fdctlr *fdc;
	register struct fdunit *un;

	fdc = fd_getctlr(dev);
	if (!fdc || !(un = fdc->c_un))
		return (ENXIO);


	unit = fdc->c_un->un_unit_no;
	FDERRPRINT(FDEP_L1, FDEM_CLOS, (C, "fd_close\n"));
	part = PARTITION(dev);

	sema_p(&fdc->c_ocsem);
	mutex_enter(&fdc->c_lolock);

	if (otyp == OTYP_LYR) {
		un->un_lyropen[part]--;
		part_is_closed = (un->un_lyropen[part] == 0);
	} else {
		un->un_regopen[otyp] &= ~(1<<part);
		part_is_closed = 1;
	}
	if (part_is_closed)
		un->un_exclmask &= ~(1<<part);

	if (fd_unit_is_open(un) == 0) {
		/* deselect drive on last close */
		fdselect(fdc, unit, 0);
		un->un_flags &= ~FDUNIT_CHANGED;
	}
	mutex_exit(&fdc->c_lolock);
	sema_v(&fdc->c_ocsem);

	return (0);
}

/*
 * fd_strategy
 *	checks operation, hangs buf struct off fdctlr, calls fdstart
 *	if not already busy.  Note that if we call start, then the operation
 *	will already be done on return (start sleeps).
 */
static int
fd_strategy(register struct buf *bp)
{
	register struct fdctlr *fdc;
	register struct fdunit *un;
	u_int	phys_blkno;
	struct dk_map *dkm;


	FDERRPRINT(FDEP_L1, FDEM_STRA,
	    (C, "fd_strategy: bp = 0x%x, dev = 0x%x\n",
	    (int)bp, (int)bp->b_edev));
	FDERRPRINT(FDEP_L1, FDEM_STRA,
	    (C, "b_blkno=%x b_flags=%x b_count=%x\n",
	    bp->b_blkno, bp->b_flags, bp->b_bcount));
	fdc = fd_getctlr(bp->b_edev);
	un = fdc->c_un;
	dkm = &un->un_label.dkl_map[PARTITION(bp->b_edev)];


	/*
	 * If it's medium density and the block no. isn't a multiple
	 * of 1K, then return an error.
	 */
	if (un->un_chars->fdc_medium) {
		phys_blkno = (u_int) bp->b_blkno >> 1;
		if (bp->b_blkno & 1) {
			FDERRPRINT(FDEP_L3, FDEM_STRA,
			    (C, "b_blkno=0x%x not %% 1k\n", bp->b_blkno));
			bp->b_error = EINVAL;
			bp->b_resid = bp->b_bcount;
			bp->b_flags |= B_ERROR;
			biodone(bp);
			return (0);
		}
	} else {
		phys_blkno = (u_int) bp->b_blkno;
	}


	/* If the block number is past the end, return an error */
	if ((phys_blkno > dkm->dkl_nblk)) {
		FDERRPRINT(FDEP_L3, FDEM_STRA,
		    (C, "fd%d: block %d is past the end! (nblk=%d)\n",
		    0, bp->b_blkno, dkm->dkl_nblk));
		bp->b_error = ENOSPC;
		bp->b_resid = bp->b_bcount;
		bp->b_flags |= B_ERROR;
		biodone(bp);
		return (0);

	}

	/* if at end of file, skip out now */
	if (phys_blkno == dkm->dkl_nblk) {

		FDERRPRINT(FDEP_L1, FDEM_STRA,
			    (C, "b_blkno is at the end!\n"));


		if ((bp->b_flags & B_READ) == 0) {
			/* a write needs to get an error! */
			bp->b_error = ENOSPC;
			bp->b_flags |= B_ERROR;

			FDERRPRINT(FDEP_L1, FDEM_STRA,
			    (C, "block is at end and this is a write\n"));

		}

		bp->b_resid = bp->b_bcount;
		biodone(bp);
		return (0);
	}

	/* if operation not a multiple of sector size, is error! */
	if (bp->b_bcount % un->un_chars->fdc_sec_size)	{
		FDERRPRINT(FDEP_L3, FDEM_STRA,
		    (C, "fd%d: b_bcount 0x%x not %% 0x%x\n",
		    0, bp->b_bcount,
		    un->un_chars->fdc_sec_size));
		FDERRPRINT(FDEP_L3, FDEM_STRA,
		    (C, "	b_blkno=0x%x b_flags=0x%x\n",
		    bp->b_blkno, bp->b_flags));
		bp->b_error = EINVAL;
		bp->b_resid = bp->b_bcount;
		bp->b_flags |= B_ERROR;
		biodone(bp);
		return (0);

	}

	/*
	 * Put the buf request in the controller's queue, FIFO.
	 */
	bp->av_forw = 0;
	sema_p(&fdc->c_ocsem);
	mutex_enter(&fdc->c_lolock);

	if (fdc->c_suspended) {
		mutex_exit(&fdc->c_lolock);
		ddi_dev_is_needed(fdc->c_dip, 0, 1);
		mutex_enter(&fdc->c_lolock);
	}
	if (un->un_iostat) {
		kstat_waitq_enter(KIOSP);
	}
	if (fdc->c_actf)
		fdc->c_actl->av_forw = bp;
	else
		fdc->c_actf = bp;
	fdc->c_actl = bp;

	/* call fdstart to start the transfer */
	fdstart(fdc);

	mutex_exit(&fdc->c_lolock);
	sema_v(&fdc->c_ocsem);
	return (0);
}

/* ARGSUSED2 */
static int
fd_read(dev_t dev, struct uio *uio, cred_t *cred_p)
{
	FDERRPRINT(FDEP_L1, FDEM_RDWR, (C, "fd_read\n"));
	return (physio(fd_strategy, NULL, dev, B_READ, minphys, uio));
}

/* ARGSUSED2 */
static int
fd_write(dev_t dev, struct uio *uio, cred_t *cred_p)
{
	FDERRPRINT(FDEP_L1, FDEM_RDWR, (C, "fd_write\n"));
	return (physio(fd_strategy, NULL, dev, B_WRITE, minphys, uio));
}

static void
fdmotoff(caddr_t arg)
{
	register struct fdctlr *fdc = (struct fdctlr *)arg;
	int unit = fdc->c_un->un_unit_no;

	mutex_enter(&fdc->c_lolock);

	/* Just return if we're about to call untimeout */
	if (fdc->c_mtimeid == 0) {
		mutex_exit(&fdc->c_lolock);
		return;
	}

	FDERRPRINT(FDEP_L1, FDEM_MOFF, (C, "fdmotoff\n"));

	fdc->c_mtimeid = 0;

	if (!(Msr(fdc) & CB) && (Dor(fdc) & (MOTEN(unit)))) {
		/* LINTED */
		Set_dor(fdc, MOTEN(unit), 0);
	}

	mutex_exit(&fdc->c_lolock);
}

/* ARGSUSED */
static int
fd_ioctl(dev_t dev, int cmd, intptr_t arg, int flag,
	cred_t *cred_p, int *rval_p)
{
	union {
		struct dk_cinfo dki;
		struct dk_geom dkg;
		struct dk_allmap dka;
		struct fd_char fdchar;
		struct fd_drive drvchar;
		int	temp;
	} cpy;

	struct vtoc	vtoc;
	register struct fdunit *un;
	register struct fdctlr *fdc;
	int unit, dkunit;
	int err = 0;
	u_int	sec_size;
	enum dkio_state state;
	int	transfer_rate;

	FDERRPRINT(FDEP_L1, FDEM_IOCT,
	    (C, "fd_ioctl: cmd 0x%x, arg 0x%x\n", cmd, arg));

	/* The minor number should always be 0 */
	if (UNIT(dev) != 0)
		return (ENXIO);

	fdc = fd_getctlr(dev);
	unit = fdc->c_un->un_unit_no;
	un = fdc->c_un;
	sec_size = un->un_chars->fdc_sec_size;
	bzero(&cpy, sizeof (cpy));

	switch (cmd) {
	case DKIOCINFO:
		cpy.dki.dki_addr = 0;

		/*
		 * The meaning of the dki_slave and dki_unit fields
		 * is unclear.  The sparc floppy driver follows the same
		 * convention as sd.c in that the instance number is
		 * returned in the dki_unit field.  The dki_slave field is
		 * ignored.
		 */

		/*
		 * The dki_cnum contains the controller instance
		 * Currently, sparc platforms only support one controller
		 * so this value should be zero
		 */
		cpy.dki.dki_cnum = FDCTLR(dev);

		/*
		 * On sparc, the floppy drive instance is the same as the
		 * controller instance.  Sparc platforms only support one
		 * floppy drive.  The device node for the controller is
		 * the same as the device node for the drive.  The x86
		 * driver is different in that it has a node for the controller
		 * and a child node for each drive.
		 */
		cpy.dki.dki_unit = FDCTLR(dev);

		/*
		 * The meaning of the dki_slave field is unclear.
		 * So, I will leave it set to 0.
		 */
		cpy.dki.dki_slave = 0;

		cpy.dki.dki_ctype = (u_short) -1;
		if (fdc->c_fdtype & FDCTYPE_82072)
			cpy.dki.dki_ctype = DKC_INTEL82072;
		if (fdc->c_fdtype & FDCTYPE_82077)
			cpy.dki.dki_ctype = DKC_INTEL82077;
		cpy.dki.dki_flags = DKI_FMTTRK;
		cpy.dki.dki_partition = PARTITION(dev);
		cpy.dki.dki_maxtransfer = maxphys / DEV_BSIZE;
		if (ddi_copyout((caddr_t)&cpy.dki, (caddr_t)arg,
						sizeof (cpy.dki), flag))
			err = EFAULT;
		break;
	case DKIOCGGEOM:
		cpy.dkg.dkg_ncyl = un->un_chars->fdc_ncyl;
		cpy.dkg.dkg_nhead = un->un_chars->fdc_nhead;
		cpy.dkg.dkg_nsect = un->un_chars->fdc_secptrack;
		cpy.dkg.dkg_intrlv = un->un_label.dkl_intrlv;
		cpy.dkg.dkg_rpm = un->un_label.dkl_rpm;
		cpy.dkg.dkg_pcyl = un->un_chars->fdc_ncyl;
		cpy.dkg.dkg_read_reinstruct =
		    (int)(cpy.dkg.dkg_nsect * cpy.dkg.dkg_rpm * 4) / 60000;
		cpy.dkg.dkg_write_reinstruct = cpy.dkg.dkg_read_reinstruct;
		if (ddi_copyout((caddr_t)&cpy.dkg, (caddr_t)arg,
						sizeof (cpy.dkg), flag))
			err = EFAULT;
		break;
	case DKIOCSGEOM:
		FDERRPRINT(FDEP_L3, FDEM_IOCT,
		    (C, "fd_ioctl: DKIOCSGEOM not supported\n"));
		err = ENOTTY;
		break;

	/*
	 * return the map of all logical partitions
	 */
	case DKIOCGAPART:
		if (ddi_copyout((caddr_t)&un->un_label.dkl_map,
			(caddr_t)arg, sizeof (struct dk_allmap), flag))
			err = EFAULT;
		break;

	/*
	 * Set the map of all logical partitions
	 */
	case DKIOCSAPART:
		if (ddi_copyin((const void *)arg, &cpy.dka,
		    sizeof (cpy.dka), flag))
			err = EFAULT;
		else {
			mutex_enter(&fdc->c_lolock);
			for (dkunit = 0; dkunit < NDKMAP; dkunit++)
				un->un_label.dkl_map[dkunit] =
				    cpy.dka.dka_map[dkunit];
			mutex_exit(&fdc->c_lolock);
		}
		break;

	case DKIOCGVTOC:
		mutex_enter(&fdc->c_lolock);

		/*
		 * Exit if the diskette has no label.
		 * Also, get the label to make sure the
		 * correct one is being used since the diskette
		 * may have changed
		 */
		if (fdgetlabel(fdc, unit)) {
			mutex_exit(&fdc->c_lolock);
			err = EINVAL;
			break;
		}

		/* Build a vtoc from the diskette's label */
		fd_build_user_vtoc(un, &vtoc);
		mutex_exit(&fdc->c_lolock);
		if (ddi_copyout(&vtoc, (void *)arg, sizeof (vtoc), flag))
			err = EFAULT;
		break;

	case DKIOCSVTOC:
		if (ddi_copyin((const void *)arg, &vtoc,
		    sizeof (vtoc), flag)) {
			err = EFAULT;
			break;
		}
		mutex_enter(&fdc->c_lolock);
		if (fdc->c_suspended) {
			mutex_exit(&fdc->c_lolock);
			ddi_dev_is_needed(fdc->c_dip, 0, 1);
			mutex_enter(&fdc->c_lolock);
		}

		/*
		 * The characteristics structure must be filled in because
		 * it helps build the vtoc.
		 */
		if ((un->un_chars->fdc_ncyl == 0) ||
				(un->un_chars->fdc_nhead == 0) ||
				(un->un_chars->fdc_secptrack == 0)) {
			mutex_exit(&fdc->c_lolock);
			err = EINVAL;
			break;
		}

		if ((err = fd_build_label_vtoc(un, &vtoc)) != 0) {
			mutex_exit(&fdc->c_lolock);
			break;
		}
		err = fdrw(fdc, unit, FDWRITE, 0, 0, 1,
		    (caddr_t)&un->un_label, sizeof (struct dk_label));
		mutex_exit(&fdc->c_lolock);
		break;

	case DKIOCSTATE:
		if (ddi_copyin((caddr_t)arg, (caddr_t)&state,
					sizeof (int), flag)) {
			err = EFAULT;
			break;
		}

		err = fd_check_media(dev, state);

		if (ddi_copyout((caddr_t)&un->un_media_state,
					(caddr_t)arg, sizeof (int), flag))
			err = EFAULT;
		break;

	case FDIOGCHAR:
		if (ddi_copyout((caddr_t)un->un_chars, (caddr_t)arg,
					sizeof (struct fd_char), flag))
			err = EFAULT;
		break;

	case FDIOSCHAR:
		if (ddi_copyin((caddr_t)arg, (caddr_t)&cpy.fdchar,
				sizeof (struct fd_char), flag)) {
			err = EFAULT;
			break;
		}

		/*
		 * Check the fields in the fdchar structre that are either
		 * driver or controller dependent.
		 */

		transfer_rate = cpy.fdchar.fdc_transfer_rate;
		if ((transfer_rate != 500) && (transfer_rate != 300) &&
		    (transfer_rate != 250) && (transfer_rate != 1000)) {
			FDERRPRINT(FDEP_L3, FDEM_IOCT,
			(C, "fd_ioctl: FDIOSCHAR odd transfer rate %d\n",
			    cpy.fdchar.fdc_transfer_rate));
			err = EINVAL;
			break;
		}

		if ((cpy.fdchar.fdc_nhead < 1) ||
				(cpy.fdchar.fdc_nhead > 2)) {
			FDERRPRINT(FDEP_L3, FDEM_IOCT,
			(C, "fd_ioctl: FDIOSCHAR bad no. of heads %d\n",
			    cpy.fdchar.fdc_nhead));
			err = EINVAL;
			break;
		}

		/*
		 * The number of cylinders must be between 0 and 255
		 */
		if ((cpy.fdchar.fdc_ncyl < 0) || (cpy.fdchar.fdc_ncyl > 255)) {
			FDERRPRINT(FDEP_L3, FDEM_IOCT,
			(C, "fd_ioctl: FDIOSCHAR bad cyl no %d\n",
			    cpy.fdchar.fdc_ncyl));
			err = EINVAL;
			break;
		}

		/* Copy the fdchar structure */

		mutex_enter(&fdc->c_lolock);
		*(un->un_chars) = cpy.fdchar;

		un->un_curfdtype = -1;

		mutex_exit(&fdc->c_lolock);

		break;
	case FDEJECT:  /* eject disk */
	case DKIOCEJECT:

		/*
		 * Fail the ioctl if auto-eject isn't supported
		 */
		if (fdc->c_un->un_drive->fdd_ejectable == 0) {

			err = ENOSYS;

		} else {
			fdselect(fdc, unit, 1);
			fdeject(fdc, unit);
		}

		/*
		 * Make sure the drive is turned off
		 */
		if (fdc->c_fdtype & FDCTYPE_82077) {
			fdc->c_mtimeid =
				timeout(fdmotoff, (caddr_t)fdc, Motoff_delay);
		}

		break;
	case FDGETCHANGE: /* disk changed */

		if (ddi_copyin((caddr_t)arg, (caddr_t)&cpy.temp,
						sizeof (int), flag)) {
			err = EFAULT;
			break;
		}

		/* zero out the user's parameter */
		cpy.temp = 0;

		mutex_enter(&fdc->c_lolock);
		if (fdc->c_suspended) {
			mutex_exit(&fdc->c_lolock);
			ddi_dev_is_needed(fdc->c_dip, 0, 1);
			mutex_enter(&fdc->c_lolock);
		}
		if (un->un_flags & FDUNIT_CHANGED)
			cpy.temp |= FDGC_HISTORY;
		else
			cpy.temp &= ~FDGC_HISTORY;
		un->un_flags &= ~FDUNIT_CHANGED;

		if (fd_pollable) {
			/*
			 * If it's a "pollable" floppy, then we don't
			 * have to do all the fdcheckdisk nastyness to
			 * figure out if the thing is still there.
			 */
			if (fdsense_chng(fdc, unit)) {
				cpy.temp |= FDGC_CURRENT;
			} else {
				cpy.temp &= ~FDGC_CURRENT;
			}
		} else {

			if (fdsense_chng(fdc, unit)) {
				/* check disk only if changed */
				if (fdcheckdisk(fdc, unit)) {
					cpy.temp |= FDGC_CURRENT;
				} else {
					cpy.temp &= ~FDGC_CURRENT;
				}
			} else {
				cpy.temp &= ~FDGC_CURRENT;
			}
		}

		if (un->un_ejected && !(cpy.temp & FDGC_CURRENT)) {
			cpy.temp |= FDGC_HISTORY;
		}
		un->un_ejected = 0;


		/* return the write-protection status */
		fdgetcsb(fdc);
		if (fdsensedrv(fdc, unit) & WP_SR3) {
			cpy.temp |= FDGC_CURWPROT;
		}
		fdretcsb(fdc);

		mutex_exit(&fdc->c_lolock);
		if (ddi_copyout((caddr_t)&cpy.temp, (caddr_t)arg,
						sizeof (int), flag))
			err = EFAULT;
		break;

	case FDGETDRIVECHAR:

		if (ddi_copyin((caddr_t)arg, (caddr_t)&cpy.drvchar,
				sizeof (struct fd_drive), flag)) {
			err = EFAULT;
			break;
		}

		/*
		 * Return the ejectable value based on the FD_MANUAL_EJECT
		 * property
		 */
		cpy.drvchar.fdd_ejectable = fdc->c_un->un_drive->fdd_ejectable;
		cpy.drvchar.fdd_maxsearch = nfdtypes; /* 3 - hi m lo density */
		if (fd_pollable)	/* pollable device */
			cpy.drvchar.fdd_flags |= FDD_POLLABLE;

		/* the rest of the fd_drive struct is meaningless to us */

		if (ddi_copyout((caddr_t)&cpy.drvchar, (caddr_t)arg,
					sizeof (struct fd_drive), flag))
			err = EFAULT;
		break;

	case FDSETDRIVECHAR:
		FDERRPRINT(FDEP_L3, FDEM_IOCT,
		    (C, "fd_ioctl: FDSETDRIVECHAR not supportedn\n"));
		err = ENOTTY;
		break;

	case DKIOCREMOVABLE: {
		int	i = 1;

		/* no brainer: floppies are always removable */
		if (ddi_copyout((caddr_t)&i, (caddr_t)arg, sizeof (int),
		    flag)) {
			err = EFAULT;
		}
		break;
	}

	case FDIOCMD:
	{
		struct fd_cmd fc;
		register int cyl, hd, spc, spt;
		int	nblks;		/* total no. of blocks */

		if (ddi_copyin((caddr_t)arg, (caddr_t)&fc,
						sizeof (fc), flag)) {
			err = EFAULT;
			break;
		}

		if (fc.fdc_cmd == FDCMD_READ || fc.fdc_cmd == FDCMD_WRITE) {
			auto struct iovec aiov;
			auto struct uio auio;
			register struct uio *uio = &auio;

			spc = (fc.fdc_cmd == FDCMD_READ)? B_READ: B_WRITE;

			bzero(&auio, sizeof (struct uio));
			bzero(&aiov, sizeof (struct iovec));
			aiov.iov_base = fc.fdc_bufaddr;
			aiov.iov_len = (u_int)fc.fdc_secnt * sec_size;
			uio->uio_iov = &aiov;

			uio->uio_iovcnt = 1;
			uio->uio_resid = aiov.iov_len;
			uio->uio_segflg = UIO_USERSPACE;
			FDERRPRINT(FDEP_L2, FDEM_IOCT,
			    (C, "fd_ioctl: call physio\n"));
			err = physio(fd_strategy, NULL, dev,
			    spc, minphys, uio);
			break;
		} else if (fc.fdc_cmd != FDCMD_FORMAT_TRACK) {

			/*
			 * The manpage states that only the FDCMD_WRITE,
			 * FDCMD_READ, and the FDCMD_FORMAT_TR are available.
			 */
			FDERRPRINT(FDEP_L1, FDEM_IOCT,
			    (C, "fd_ioctl: FDIOCMD invalid command\n"));
			err = EINVAL;
			break;
		}



		/* The command is FDCMD_FORMAT_TRACK */

		spt = un->un_chars->fdc_secptrack;	/* sec/trk */
		spc = un->un_chars->fdc_nhead * spt;	/* sec/cyl */
		cyl = fc.fdc_blkno / spc;
		hd = (fc.fdc_blkno % spc) / spt;

		/*
		 * Make sure the specified block number is in the correct
		 * range. (block numbers start at 0)
		 */
		nblks = spc * un->un_chars->fdc_ncyl;

		if (fc.fdc_blkno < 0 || fc.fdc_blkno > (nblks - 1)) {
			err = EINVAL;
			break;
		}


		mutex_enter(&fdc->c_lolock);
		if (fdc->c_suspended) {
			mutex_exit(&fdc->c_lolock);
			ddi_dev_is_needed(fdc->c_dip, 0, 1);
			mutex_enter(&fdc->c_lolock);
		}
		if (fdformat(fdc, unit, cyl, hd))
			err = EIO;

		mutex_exit(&fdc->c_lolock);
		break;
	}

	case FDRAW:
		err = fdrawioctl(fdc, unit, arg, flag);
		break;
#ifdef FD_DEBUG
	case IOCTL_DEBUG:
		fderrlevel--;
		if (fderrlevel < 0)
			fderrlevel = 3;
		cmn_err(C, "fdioctl: CHANGING debug to %d", fderrlevel);
		return (0);
#endif FD_DEBUG
	default:
		FDERRPRINT(FDEP_L2, FDEM_IOCT,
		    (C, "fd_ioctl: invalid ioctl 0x%x\n", cmd));
		err = ENOTTY;
		break;
	}

	return (err);
}

/*
 * fdrawioctl
 *
 * 	- acquires the low level lock
 */

static int
fdrawioctl(struct fdctlr *fdc, int unit, intptr_t arg, int mode)
{
	struct fd_raw fdr;
	struct fdcsb *csb;
	int i, err, flag;
	caddr_t fa;
	u_int	fc;
	register struct fdunit *un;
	u_int	real_length;
	int	res;
	ddi_device_acc_attr_t attr;
	ddi_acc_handle_t	mem_handle;

	attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
	attr.devacc_attr_endian_flags  = DDI_STRUCTURE_BE_ACC;
	attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;

	ASSERT(fdc->c_un->un_unit_no == unit);

	FDERRPRINT(FDEP_L1, FDEM_RAWI,
	    (C, "fdrawioctl: cmd[0]=0x%x\n", fdr.fdr_cmd[0]));
	/*
	 * check if medium density is asked but not supported
	 */
	un = fdc->c_un;
	if (un->un_chars->fdc_medium &&
	    (fdc->c_fdtype & FDCTYPE_82072)) {
		cmn_err(C, "fdrawioctl: Medium density not supported\n");
		return (ENXIO);
	}


	flag = B_READ;
	err = 0;
	fa = NULL;
	fc = (u_int)0;

	/* Copy in the arguments */
	if (ddi_copyin((caddr_t)arg, (caddr_t)&fdr, sizeof (fdr), mode)) {
		FDERRPRINT(FDEP_L1, FDEM_RAWI,
		(C, "fdrawioctl: problem with copying in args\n"));
		return (EFAULT);
	}

	mutex_enter(&fdc->c_lolock);
	if (fdc->c_suspended) {
		mutex_exit(&fdc->c_lolock);
		ddi_dev_is_needed(fdc->c_dip, 0, 1);
		mutex_enter(&fdc->c_lolock);
	}
	fdgetcsb(fdc);
	csb = &fdc->c_csb;
	csb->csb_unit = (u_char) unit;

	/* copy cmd bytes into csb */
	for (i = 0; i <= fdr.fdr_cnum; i++)
		csb->csb_cmds[i] = fdr.fdr_cmd[i];
	csb->csb_ncmds = (u_char) fdr.fdr_cnum;

	csb->csb_maxretry = 0;	/* let the application deal with errors */
	csb->csb_retrys = 0;

	switch (fdr.fdr_cmd[0] & 0x0f) {

	case FDRAW_SPECIFY:

		/*
		 * Ensure that the right DMA mode is selected.  There is
		 * currently no way for the user to tell if DMA is
		 * happening so set the value for the user.
		 */


		if (fdc->c_fdtype & FDCTYPE_DMA)
			csb->csb_cmds[2] = csb->csb_cmds[2] & 0xFE;
		else
			csb->csb_cmds[2] = csb->csb_cmds[2] | 0x1;


		csb->csb_opflags = CSB_OFNORESULTS;
		csb->csb_nrslts = 0;
		break;

	case FDRAW_SENSE_DRV:
		/* Insert the appropriate drive number */
		csb->csb_cmds[1] = csb->csb_cmds[1] | (unit & DRV_MASK);
		csb->csb_opflags = CSB_OFIMMEDIATE;
		csb->csb_nrslts = 1;
		break;

	case FDRAW_REZERO:
	case FDRAW_SEEK:
		/* Insert the appropriate drive number */
		csb->csb_cmds[1] = csb->csb_cmds[1] | (unit & DRV_MASK);
		csb->csb_opflags = CSB_OFSEEKOPS + CSB_OFTIMEIT;
		csb->csb_nrslts = 2;
		break;

	case FDRAW_FORMAT:
		FDERRPRINT(FDEP_L1, FDEM_RAWI,
			(C, "fdrawioctl: cmd is fdfraw format\n"));

		/* Insert the appropriate drive number */
		csb->csb_cmds[1] = csb->csb_cmds[1] | (unit & DRV_MASK);
		csb->csb_opflags = CSB_OFXFEROPS + CSB_OFTIMEIT;
		csb->csb_nrslts = NRBRW;
		flag = B_WRITE;


		/*
		 * Allocate memory for the command.
		 * If PIO is being used, then add an extra 16 bytes
		 */
		if (fdc->c_fdtype & FDCTYPE_DMA) {

			fc = (u_int)(fdr.fdr_nbytes);

			res = ddi_dma_mem_alloc(fdc->c_dmahandle, fc,
				&attr, DDI_DMA_STREAMING,
				DDI_DMA_DONTWAIT, 0, &fa, &real_length,
				&mem_handle);

			if (res != DDI_SUCCESS) {
				fdretcsb(fdc);
				mutex_exit(&fdc->c_lolock);
				return (EIO);
			}

			fdc->c_csb.csb_dma_read = CSB_DMA_WRITE;
			if (fdstart_dma(fdc, fa, fc) != 0) {
				ddi_dma_mem_free(&mem_handle);
				fdretcsb(fdc);
				mutex_exit(&fdc->c_lolock);
				return (EIO);
			}

		} else {
			fc = (u_int)(fdr.fdr_nbytes + 16);
			fa = kmem_zalloc(fc, KM_SLEEP);
		}

		/* copy in the user's command bytes */
		if (ddi_copyin(fdr.fdr_addr, fa,
				(u_int)fdr.fdr_nbytes, mode)) {
			fdretcsb(fdc);
			mutex_exit(&fdc->c_lolock);

			if (fdc->c_fdtype & FDCTYPE_DMA) {
				ddi_dma_mem_free(&mem_handle);
				FDERRPRINT(FDEP_L1, FDEM_RAWI,
				(C, "fdrawioctl: (err)free dma memory\n"));
			} else {
				kmem_free(fa, fc);
			}

			return (EFAULT);
		}

		break;
	case FDRAW_WRCMD:
	case FDRAW_WRITEDEL:
		flag = B_WRITE;
		/* FALLTHROUGH */
	case FDRAW_RDCMD:
	case FDRAW_READDEL:
	case FDRAW_READTRACK:
		/* Insert the appropriate drive number */
		csb->csb_cmds[1] = csb->csb_cmds[1] | (unit & DRV_MASK);
		csb->csb_opflags = CSB_OFXFEROPS + CSB_OFTIMEIT;
		csb->csb_nrslts = NRBRW;
		break;

	default:
		fdretcsb(fdc);
		mutex_exit(&fdc->c_lolock);
		return (EINVAL);
	}

	if ((csb->csb_opflags & CSB_OFXFEROPS) && (fdr.fdr_nbytes == 0)) {
		fdretcsb(fdc);
		mutex_exit(&fdc->c_lolock);
		return (EINVAL);
	}
	csb->csb_opflags |= CSB_OFRAWIOCTL;




	FDERRPRINT(FDEP_L1, FDEM_RAWI,
		(C, "fdrawioctl: nbytes = %u\n", fdr.fdr_nbytes));

	if ((fdr.fdr_cmd[0] & 0x0f) != FDRAW_FORMAT) {

		if ((fc = (u_int) fdr.fdr_nbytes) > 0) {
			/*
			 * In SunOS 4.X, we used to as_fault things in.
			 * We really cannot do this in 5.0/SVr4. Unless
			 * someone really believes that speed is of the
			 * essence here, it is just much simpler to do
			 * this in kernel space and use copyin/copyout.
			 */

			if (fdc->c_fdtype & FDCTYPE_DMA) {

				res = ddi_dma_mem_alloc(fdc->c_dmahandle, fc,
					&attr, DDI_DMA_STREAMING,
					DDI_DMA_DONTWAIT, 0, &fa, &real_length,
					&mem_handle);

				if (res != DDI_SUCCESS) {
					fdretcsb(fdc);
					mutex_exit(&fdc->c_lolock);
					return (EIO);
				}

				if (flag == B_WRITE)
					fdc->c_csb.csb_dma_read = CSB_DMA_WRITE;
				else
					fdc->c_csb.csb_dma_read = CSB_DMA_READ;

				if (fdstart_dma(fdc, fa, fc) != 0) {
					ddi_dma_mem_free(&mem_handle);
					fdretcsb(fdc);
					mutex_exit(&fdc->c_lolock);
					return (EIO);
				}

			} else {
				fa = kmem_zalloc(fc, KM_SLEEP);
			}

			if (flag == B_WRITE) {
				if (ddi_copyin(fdr.fdr_addr, fa, fc, mode)) {
					if (fdc->c_fdtype & FDCTYPE_DMA)
						ddi_dma_mem_free(&mem_handle);
					else
						kmem_free(fa, fc);
					fdretcsb(fdc);
					mutex_exit(&fdc->c_lolock);
					FDERRPRINT(FDEP_L1, FDEM_RAWI,
					(C, "fdrawioctl: can't copy data\n"));

					return (EFAULT);
				}
			}
			csb->csb_addr = fa;
			csb->csb_len = fc;
		} else {
			csb->csb_addr = 0;
			csb->csb_len = 0;
		}
	} else {
		csb->csb_addr = fa;
		csb->csb_len = fc;
	}

	FDERRPRINT(FDEP_L1, FDEM_RAWI,
	    (C, "cmd: %x %x %x %x %x %x %x %x %x %x\n", csb->csb_cmds[0],
	    csb->csb_cmds[1], csb->csb_cmds[2], csb->csb_cmds[3],
	    csb->csb_cmds[4], csb->csb_cmds[5], csb->csb_cmds[6],
	    csb->csb_cmds[7], csb->csb_cmds[8], csb->csb_cmds[9]));
	FDERRPRINT(FDEP_L1, FDEM_RAWI,
	    (C, "nbytes: %x, opflags: %x, addr: %x, len: %x\n",
	    csb->csb_ncmds, csb->csb_opflags, (int)csb->csb_addr,
	    csb->csb_len));


	/*
	 * Note that we ignore any error return s from fdexec.
	 * This is the way the driver has been, and it may be
	 * that the raw ioctl senders simply don't want to
	 * see any errors returned in this fashion.
	 */

	if ((csb->csb_opflags & CSB_OFNORESULTS) ||
	    (csb->csb_opflags & CSB_OFIMMEDIATE)) {
		(void) fdexec(fdc, 0); /* don't sleep, don't check change */
	} else {
		(void) fdexec(fdc, FDXC_SLEEP | FDXC_CHECKCHG);
	}


	FDERRPRINT(FDEP_L1, FDEM_RAWI,
	    (C, "rslt: %x %x %x %x %x %x %x %x %x %x\n", csb->csb_rslt[0],
	    csb->csb_rslt[1], csb->csb_rslt[2], csb->csb_rslt[3],
	    csb->csb_rslt[4], csb->csb_rslt[5], csb->csb_rslt[6],
	    csb->csb_rslt[7], csb->csb_rslt[8], csb->csb_rslt[9]));

	if ((fdr.fdr_cmd[0] & 0x0f) != FDRAW_FORMAT && fc &&
	    flag == B_READ && err == 0) {
		if (ddi_copyout(fa, fdr.fdr_addr, fc, mode)) {
			FDERRPRINT(FDEP_L1, FDEM_RAWI,
			(C, "fdrawioctl: can't copy read data\n"));

			err = EFAULT;
		}
	}


	if (fc) {
		if (fdc->c_fdtype & FDCTYPE_DMA) {
			ddi_dma_mem_free(&mem_handle);
			FDERRPRINT(FDEP_L1, FDEM_RAWI,
				(C, "fdrawioctl: free dma memory\n"));
		} else {
			kmem_free(fa, fc);
		}
	}


	/* copy cmd results into fdr */
	for (i = 0; (int)i <= (int)csb->csb_nrslts; i++)
		fdr.fdr_result[i] = csb->csb_rslt[i];
	fdr.fdr_nbytes = fdc->c_csb.csb_rlen; /* return resid */
	if (ddi_copyout(&fdr, (caddr_t)arg, sizeof (fdr), mode)) {
		FDERRPRINT(FDEP_L1, FDEM_RAWI,
		(C, "fdrawioctl: can't copy results\n"));

		err = EFAULT;
	}

	fdretcsb(fdc);
	mutex_exit(&fdc->c_lolock);
	return (0);
}

/*
 * fdformat
 *	format a track
 * For PIO, builds a table of sector data values with 16 bytes
 * (sizeof fdc's fifo) of dummy on end.	 This is so than when fdc->c_len
 * goes to 0 and fd_intr sends a TC that all the real formatting will
 * have already been done.
 *
 *	- called with the low level lock held
 */
static int
fdformat(struct fdctlr *fdc, int unit, int cyl, int hd)
{
	register struct fdcsb *csb;
	register struct fdunit *un;
	register struct fd_char *ch;
	int	cmdresult;
	u_char	*fmthdrs;
	caddr_t fd;
	int	i;
	u_int   real_length;
	ddi_device_acc_attr_t attr;
	ddi_acc_handle_t mem_handle;

	FDERRPRINT(FDEP_L1, FDEM_FORM,
	    (C, "fdformat cyl %d, hd %d\n", cyl, hd));
	fdgetcsb(fdc);

	ASSERT(fdc->c_un->un_unit_no == unit);

	csb = &fdc->c_csb;
	un = fdc->c_un;
	ch = un->un_chars;

	/* setup common things in csb */
	csb->csb_unit = (u_char) unit;

	/*
	 * The controller needs to do a seek before
	 * each format to get to right cylinder.
	 */
	if (fdrecalseek(fdc, unit, cyl, FDXC_CHECKCHG)) {
		fdretcsb(fdc);
		return (EIO);
	}

	/*
	 * now do the format itself
	 */
	csb->csb_nrslts = NRBRW;
	csb->csb_opflags = CSB_OFXFEROPS | CSB_OFTIMEIT;

	csb->csb_cmds[0] = FDRAW_FORMAT;
	/* always or in MFM bit */
	csb->csb_cmds[0] |= MFM;
	csb->csb_cmds[1] = (hd << 2) | (unit & 0x03);
	csb->csb_cmds[2] = ch->fdc_medium ? 3 : 2;
	csb->csb_cmds[3] = ch->fdc_secptrack;
	csb->csb_cmds[4] = GPLF;
	csb->csb_cmds[5] = FDATA;
	csb->csb_ncmds = 6;
	csb->csb_maxretry = rwretry;
	csb->csb_retrys = 0;

	/*
	 * NOTE: have to add size of fifo also - for dummy format action
	 * if PIO is being used.
	 */


	if (fdc->c_fdtype & FDCTYPE_DMA) {

		csb->csb_len = (u_int)4 * ch->fdc_secptrack;

		attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
		attr.devacc_attr_endian_flags  = DDI_STRUCTURE_BE_ACC;
		attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;


		cmdresult = ddi_dma_mem_alloc(fdc->c_dmahandle, csb->csb_len,
			&attr, DDI_DMA_STREAMING,
			DDI_DMA_DONTWAIT, 0, &fd, &real_length,
			&mem_handle);

		if (cmdresult != DDI_SUCCESS)
			return (cmdresult);

		fdc->c_csb.csb_dma_read = CSB_DMA_WRITE;
		if (fdstart_dma(fdc, fd,  csb->csb_len) != 0) {
			ddi_dma_mem_free(&mem_handle);
			return (-1);
		}


	} else {
		csb->csb_len = (u_int)4 * ch->fdc_secptrack + 16;
		fd = kmem_zalloc(csb->csb_len, KM_SLEEP);
		fmthdrs = (u_char *)fd;
	}

	csb->csb_addr = (caddr_t)fd;

	for (i = 1; i <= ch->fdc_secptrack; i++) {
		*fd++ = (u_char) cyl;		/* cylinder */
		*fd++ = (u_char) hd;		/* head */
		*fd++ = (u_char) i;	/* sector number */
		*fd++ = ch->fdc_medium ? 3 : 2; /* sec_size code */
	}

	if ((cmdresult = fdexec(fdc, FDXC_SLEEP | FDXC_CHECKCHG)) == 0) {
		if (csb->csb_cmdstat)
			cmdresult = EIO;	/* XXX TBD NYD for now */
	}

	if (fdc->c_fdtype & FDCTYPE_DMA) {
		ddi_dma_mem_free(&mem_handle);
	} else {
		kmem_free((caddr_t)fmthdrs, csb->csb_len);
	}

	fdretcsb(fdc);

	return (cmdresult);
}

/*
 * fdstart
 *	called from fd_strategy() or from fdXXXX() to setup and
 *	start operations of read or write only (using buf structs).
 *	Because the chip doesn't handle crossing cylinder boundaries on
 *	the fly, this takes care of those boundary conditions.	Note that
 *	it sleeps until the operation is done *within fdstart* - so that
 *	when fdstart returns, the operation is already done.
 *
 *	- called with the low level lock held
 *
 */
static void
fdstart(register struct fdctlr *fdc)
{
	register struct buf *bp;
	register struct fdcsb *csb;
	register struct fdunit *un;
	register struct fd_char *ch;
	struct dk_map *dkm;
	u_int   part;		/* partition number for the transfer */
	u_int	start_part;	/* starting block of the partition */
	u_int   last_part;	/* last block of the parition */
	u_int   blk;		/* starting block of transfer on diskette */
	u_int   sect;		/* starting block's offset into track */
	u_int   cyl;		/* starting cylinder of the transfer */
	u_int   bincyl;		/* starting blocks's offset into cylinder */
	u_int	secpcyl;	/* number of sectors per cylinder */
	u_int	phys_blkno;	/* no. of blocks on the diskette */
	u_int	head;		/* one of two diskette heads */
	u_int	unit;
	u_int	len, tlen;
	caddr_t addr;

	bp = fdc->c_actf;

	while (bp != NULL) {

		fdc->c_actf = bp->av_forw;
		fdc->c_current = bp;

		/*
		 * Initialize the buf structure.  The residual count is
		 * initially the number of bytes to be read or written
		 */
		bp->b_flags &= ~B_ERROR;
		bp->b_error = 0;
		bp->b_resid = bp->b_bcount;
		bp_mapin(bp);			/* map in buffers */

		addr = bp->b_un.b_addr;		/* assign buffer address */

		/*
		 * Find the unit and partition numbers.
		 */
		unit = fdc->c_un->un_unit_no;
		un = fdc->c_un;
		ch = un->un_chars;
		part = PARTITION(bp->b_edev);
		dkm = &un->un_label.dkl_map[part];

		if (un->un_chars->fdc_medium) {
			phys_blkno = bp->b_blkno >> 1;
		} else {
			phys_blkno = bp->b_blkno;
		}

		if (un->un_iostat) {
			kstat_waitq_to_runq(KIOSP);
		}

		FDERRPRINT(FDEP_L1, FDEM_STRT,
		(C, "fdstart: bp=0x%x blkno=0x%x bcount=0x%x\n",
		(int)bp, (int)bp->b_blkno, bp->b_bcount));

		/*
		 * Get the csb and initialize the values that are the same
		 * for DMA and PIO.
		 */
		fdgetcsb(fdc);		/* get csb (maybe wait for it) */
		csb = &fdc->c_csb;
		csb->csb_unit = unit;		/* floppy unit number */

		if (bp->b_flags & B_READ) {
			if (fdc->c_fdtype & FDCTYPE_TCBUG)
				csb->csb_cmds[0] = SK | FDRAW_RDCMD | MFM;
			else
				csb->csb_cmds[0] = MT | SK | FDRAW_RDCMD | MFM;
		} else {
			if (fdc->c_fdtype & FDCTYPE_TCBUG)
				csb->csb_cmds[0] = FDRAW_WRCMD | MFM;
			else
				csb->csb_cmds[0] = MT | FDRAW_WRCMD | MFM;
		}


		if (bp->b_flags & B_READ)
			fdc->c_csb.csb_dma_read = CSB_DMA_READ;
		else
			fdc->c_csb.csb_dma_read = CSB_DMA_WRITE;


		csb->csb_cmds[5] = ch->fdc_medium ? 3 : 2; /* sector size  */
		csb->csb_cmds[6] = ch->fdc_secptrack; /* EOT-# of sectors/trk */
		csb->csb_cmds[7] = GPLN;	/* GPL - gap 3 size code */
		csb->csb_cmds[8] = SSSDTL;	/* DTL - be 0xFF if N != 0 */

		csb->csb_ncmds = NCBRW;		/* number of command bytes */
		csb->csb_nrslts = NRBRW;	/* number of result bytes */


		/*
		 * opflags for interrupt handler, et.al.
		 */
		csb->csb_opflags = CSB_OFXFEROPS | CSB_OFTIMEIT;


		/*
		 * Make sure the transfer does not go off the end
		 * of the partition.  Limit the actual amount transferred
		 * to fit the partition.
		 */

		blk = phys_blkno;
		start_part = (dkm->dkl_cylno * ch->fdc_secptrack
				* ch->fdc_nhead);
		blk = blk + start_part;
		last_part = start_part + dkm->dkl_nblk;

		if ((blk + (bp->b_bcount / ch->fdc_sec_size)) > last_part)
			len = (last_part - blk) * ch->fdc_sec_size;
		else
			len = bp->b_bcount;

		/*
		 * now we have the real start blk,
		 * addr and len for xfer op
		 * sectors per cylinder
		 */
		secpcyl = ch->fdc_nhead * ch->fdc_secptrack;

		/*
		 * The controller can transfer up to a cylinder at a time.
		 * Early revs of the 82077 have a bug that causes the chip to
		 * fail to respond to the Terminal Count signal.  Due to this
		 * bug, controllers with type FDCTYPE_TCBUG, only transfer up
		 * to a track at a time.
		 */

		while (len != 0) {

			cyl = blk / secpcyl;	/* cylinder of transfer */
			bincyl = blk % secpcyl;	/* blk within cylinder */
			head = bincyl / ch->fdc_secptrack;
			sect = (bincyl % ch->fdc_secptrack) + 1;
						/* sect w/in track */

			/*
			 * If the desired block and length will go beyond the
			 * cylinder end, limit it to the cylinder end.
			 */
			if (fdc->c_fdtype & FDCTYPE_TCBUG) {
				tlen = len;
				if (len > ((ch->fdc_secptrack - sect + 1) *
							ch->fdc_sec_size))
					tlen = (ch->fdc_secptrack - sect + 1)
							* ch->fdc_sec_size;
			} else {
				if (len > ((secpcyl - bincyl)
							* ch->fdc_sec_size))
					tlen = (secpcyl - bincyl)
							* ch->fdc_sec_size;

				else
					tlen = len;
			}

			FDERRPRINT(FDEP_L1, FDEM_STRT,
				(C, "         blk 0x%x, addr 0x%x, len 0x%x\n",
				blk, (int)addr, len));
			FDERRPRINT(FDEP_L1, FDEM_STRT,
				(C, "cyl:%x, head:%x, sec:%x\n",
				cyl, head, sect));

			FDERRPRINT(FDEP_L1, FDEM_STRT,
				(C, "         resid 0x%x, tlen %d\n",
				bp->b_resid, tlen));

			/*
			 * Finish programming the command
			 */
			csb->csb_cmds[1] = (head << 2) | unit;
			csb->csb_cmds[2] = cyl;	/* C - cylinder address */
			csb->csb_cmds[3] = head;	/* H - head number */
			csb->csb_cmds[4] = sect;	/* R - sector number */
			if (fdc->c_fdtype & FDCTYPE_TCBUG)
				csb->csb_cmds[6] = sect +
						(tlen / ch->fdc_sec_size) - 1;

			csb->csb_len = tlen;
			csb->csb_addr = addr;

			/* retry this many times max */
			csb->csb_maxretry = rwretry;
			csb->csb_retrys = 0;

			/* If platform supports DMA, set up DMA resources */
			if (fdc->c_fdtype & FDCTYPE_DMA) {

				    if (fdstart_dma(fdc, addr, tlen) != 0) {

					bp->b_flags |= B_ERROR;
					bp->b_error = EAGAIN;

					FDERRPRINT(FDEP_L1, FDEM_STRT,
					(C, "fdstart: no dma resources\n"));

					break;
				}

			}

			bp->b_error = fdexec(fdc, FDXC_SLEEP|FDXC_CHECKCHG);
			if (bp->b_error != 0) {
				/*
				 * error in fdexec
				*/
				FDERRPRINT(FDEP_L1, FDEM_STRT,
				(C, "fdstart: bad exec of bp: 0x%x, err %d\n",
				(int)bp, bp->b_error));

				bp->b_flags |= B_ERROR;
				break;
			}


			blk += tlen / ch->fdc_sec_size;
			len -= tlen;
			addr += tlen;
			bp->b_resid -= tlen;

		}

		FDERRPRINT(FDEP_L1, FDEM_STRT,
		(C, "fdstart done: b_resid %d, b_count %d, csb_rlen %d\n",
		bp->b_resid, bp->b_bcount, fdc->c_csb.csb_rlen));

		fdc->c_current = 0;
		fdretcsb(fdc);
		if (un->un_iostat) {
			if (bp->b_flags & B_READ) {
				KIOSP->reads++;
				KIOSP->nread +=
					(bp->b_bcount - bp->b_resid);
			} else {
				KIOSP->writes++;
				KIOSP->nwritten += (bp->b_bcount - bp->b_resid);
			}
			kstat_runq_exit(KIOSP);
		}
		bp_mapout(bp);
		biodone(bp);

		/*
		 * Look at the next buffer
		 */
		bp = fdc->c_actf;

	}
}

/*
 * Set up DMA resources
 * The DMA handle was initialized in fd_attach()
 * Assumes the handle has already been allocated by fd_attach()
 */
static int
fdstart_dma(register struct fdctlr *fdc, caddr_t addr, u_int len)
{
	int		flags;		/* flags for setting up resources */
	int		res;

	FDERRPRINT(FDEP_L1, FDEM_SDMA, (C, "fdstart_dma: start\n"));

	if (fdc->c_csb.csb_dma_read == CSB_DMA_READ) {
		flags = DDI_DMA_READ;
	} else {
		flags = DDI_DMA_WRITE;
	}


	/* allow partial mapping to maximize the portablility of the driver */
	flags = flags | DDI_DMA_PARTIAL;

	FDERRPRINT(FDEP_L1, FDEM_SDMA, (C, "fdstart_dma: amt. asked for %d\n",
					len));

	/*
	 * Zero out the current cookie.  This is done to ensure that
	 * the previous transfers cookie information can in no way be
	 * used.
	 */
	bzero((char *)&fdc->c_csb.csb_dmacookie,
			sizeof (fdc->c_csb.csb_dmacookie));
	fdc->c_csb.csb_nwin = 0;
	fdc->c_csb.csb_windex = 0;
	fdc->c_csb.csb_ccount = 0;

	res = ddi_dma_addr_bind_handle(fdc->c_dmahandle, NULL, addr, len,
	flags, DDI_DMA_DONTWAIT, 0,  &fdc->c_csb.csb_dmacookie,
	&fdc->c_csb.csb_ccount);

	switch (res) {
		case DDI_DMA_MAPPED:
			/*
			 * There is one window. csb_windex is the index
			 * into the array of windows. If there are n
			 * windows then, (0 <= windex <= n-1).  csb_windex
			 * represents the index of the next window
			 * to be processed.
			 */
			fdc->c_csb.csb_nwin = 1;
			fdc->c_csb.csb_windex = 1;


			FDERRPRINT(FDEP_L1, FDEM_SDMA,
				(C, "fdstart_dma: DDI_DMA_MAPPED\n"));

			break;
		case DDI_DMA_PARTIAL_MAP:

			/*
			 * obtain the number of DMA windows
			 */
			if (ddi_dma_numwin(fdc->c_dmahandle,
				&fdc->c_csb.csb_nwin) != DDI_SUCCESS) {
				return (-1);
			}


			FDERRPRINT(FDEP_L1, FDEM_SDMA,
			(C, "fdstart_dma: partially mapped %d windows\n",
			fdc->c_csb.csb_nwin));

			/*
			 * The DMA window currently in use is window number
			 * one.
			 */
			fdc->c_csb.csb_windex = 1;

			break;
		case DDI_DMA_NORESOURCES:
			FDERRPRINT(FDEP_L1, FDEM_SDMA,
				(C, "fdstart_dma: no resources\n"));
			return (-1);
		case DDI_DMA_NOMAPPING:
			FDERRPRINT(FDEP_L1, FDEM_SDMA,
				(C, "fdstart_dma: no mapping\n"));
			return (-1);
		case DDI_DMA_TOOBIG:
			FDERRPRINT(FDEP_L1, FDEM_SDMA,
				(C, "fdstart_dma: too big\n"));
			return (-1);

		case DDI_DMA_INUSE:
			FDERRPRINT(FDEP_L1, FDEM_SDMA,
				(C, "fdstart_dma: dma inuse\n"));
			return (-1);
		default:
			FDERRPRINT(FDEP_L1, FDEM_SDMA,
				(C, "fdstart_dma: result is 0x%x\n", res));
			return (-1);

	};

	FDERRPRINT(FDEP_L1, FDEM_SDMA,
		(C, "fdstart_dma: bound the handle\n"));

	ASSERT(fdc->c_csb.csb_dmacookie.dmac_size);

	FDERRPRINT(FDEP_L1, FDEM_SDMA, (C, "fdstart_dma: done\n"));
	return (0);
}


/*
 * fd_unbind_handle: unbind a dma handle if one exists
 *		return EIO if unbind failes
 */
static int
fd_unbind_handle(struct fdctlr *fdc)
{
	if ((fdc->c_fdtype & FDCTYPE_DMA) &&
		((fdc->c_csb.csb_dma_read == CSB_DMA_READ) ||
		    (fdc->c_csb.csb_dma_read == CSB_DMA_WRITE))) {

		/*
		 * During positron bringup, there was a problem where the
		 * driver tore down the mappings before the byte count
		 * had gone to zero.  This caused a PCI target abort.
		 * I'm leaving this check here to help debug any future
		 * problems.
		 */

		if (Get_dbcr(fdc) != 0) {
			    FDERRPRINT(FDEP_L1, FDEM_EXEC,
				(C, "unbind & byte count isn't zero\n"));
		}


		if (ddi_dma_unbind_handle(fdc->c_dmahandle) != DDI_SUCCESS) {
			FDERRPRINT(FDEP_L1, FDEM_EXEC,
				(C, "problem unbinding the handle\n"));
			return (EIO);
		}
	}
	return (0);
}

/*
 * fdexec
 *	all commands go through here.  Assumes the command block
 *	fdctlr.c_csb is filled in.  The bytes are sent to the
 *	controller and then we do whatever else the csb says -
 *	like wait for immediate results, etc.
 *
 *	All waiting for operations done is in here - to allow retrys
 *	and checking for disk changed - so we don't have to worry
 *	about sleeping at interrupt level.
 *
 * RETURNS: 0 if all ok,
 *	ENXIO - diskette not in drive
 *	EBUSY - if chip is locked or busy
 *	EIO - for timeout during sending cmds to chip
 *
 * to sleep: set FDXC_SLEEP, to check for disk
 * changed: set FDXC_CHECKCHG
 *
 *	- called with the lock held
 */
static int
fdexec(register struct fdctlr *fdc, int flags)
{
	register struct fdcsb *csb;
	register int	i;
	int	to, unit;
	u_char	tmp;
	caddr_t a = (caddr_t)fdc;

	FDERRPRINT(FDEP_L1, FDEM_EXEC, (C, "fdexec: flags:%x\n", flags));

	ASSERT(mutex_owned(&fdc->c_lolock));

	csb = &fdc->c_csb;
	unit = csb->csb_unit;


	ASSERT(unit == fdc->c_un->un_unit_no);

retry:
	FDERRPRINT(FDEP_L1, FDEM_EXEC, (C, "fdexec: cmd is %s\n",
				fdcmds[csb->csb_cmds[0] & 0x1f].cmdname));
	FDERRPRINT(FDEP_L1, FDEM_EXEC, (C, "fdexec: transfer rate = %d\n",
	    fdc->c_un->un_chars->fdc_transfer_rate));
	FDERRPRINT(FDEP_L1, FDEM_EXEC, (C, "fdexec: sec size = %d\n",
	    fdc->c_un->un_chars->fdc_sec_size));
	/* LINTED */
	FDERRPRINT(FDEP_L1, FDEM_EXEC, (C, "fdexec: nblocks (512) = %d\n",
	    fdc->c_un->un_label.dkl_map[2].dkl_nblk));

	if ((fdc->c_fdtype & FDCTYPE_CTRLMASK) == FDCTYPE_82077) {
		fdexec_turn_on_motor(fdc, flags, unit);
	}


	fdselect(fdc, unit, 1);	/* select drive */

	/*
	 * select data rate for this unit/command
	 */
	switch (fdc->c_un->un_chars->fdc_transfer_rate) {
	case 500:
		Dsr(fdc, 0);
		break;
	case 300:
		Dsr(fdc, 1);
		break;
	case 250:
		Dsr(fdc, 2);
		break;
	}
	drv_usecwait(2);


	/*
	 * If checking for changed is enabled (i.e., not seeking in checkdisk),
	 * we sample the DSKCHG line to see if the diskette has wandered away.
	 */
	if ((flags & FDXC_CHECKCHG) && fdsense_chng(fdc, unit)) {
		FDERRPRINT(FDEP_L1, FDEM_EXEC, (C, "diskette changed\n"));
		fdc->c_un->un_flags |= FDUNIT_CHANGED;

		if (fdcheckdisk(fdc, unit)) {

			(void) fd_unbind_handle(fdc);

			return (ENXIO);

		}
	}

	/*
	 * gather some statistics
	 */
	switch (csb->csb_cmds[0] & 0x1f) {
	case FDRAW_RDCMD:
		fdc->fdstats.rd++;
		break;
	case FDRAW_WRCMD:
		fdc->fdstats.wr++;
		break;
	case FDRAW_REZERO:
		fdc->fdstats.recal++;
		break;
	case FDRAW_FORMAT:
		fdc->fdstats.form++;
		break;
	default:
		fdc->fdstats.other++;
		break;
	}

	/*
	 * Always set the opmode *prior* to poking the chip.
	 * This way we don't have to do any locking at high level.
	 */
	csb->csb_raddr = 0;
	csb->csb_rlen = 0;
	if (csb->csb_opflags & CSB_OFSEEKOPS) {
		csb->csb_opmode = 2;
	} else if (csb->csb_opflags & CSB_OFIMMEDIATE) {
		csb->csb_opmode = 0;
	} else {
		csb->csb_opmode = 1;	/* normal data xfer commands */
		csb->csb_raddr = csb->csb_addr;
		csb->csb_rlen = csb->csb_len;
	}

	bzero((caddr_t)csb->csb_rslt, 10);
	csb->csb_status = 0;
	csb->csb_cmdstat = 0;


	/*
	 * Program the DMA engine with the length and address of the transfer
	 * (DMA is only used on a read or a write)
	 */
	if ((fdc->c_fdtype & FDCTYPE_DMA) &&
			((fdc->c_csb.csb_dma_read == CSB_DMA_READ) ||
			    (fdc->c_csb.csb_dma_read == CSB_DMA_WRITE)))  {

		/* Reset the dcsr to clear it of all errors */
		Reset_dcsr(fdc);


		FDERRPRINT(FDEP_L1, FDEM_EXEC, (C, "cookie addr 0x%x\n",
				fdc->c_csb.csb_dmacookie.dmac_laddress));

		FDERRPRINT(FDEP_L1, FDEM_EXEC, (C, "cookie length %d\n",
				fdc->c_csb.csb_dmacookie.dmac_size));
		ASSERT(fdc->c_csb.csb_dmacookie.dmac_size);

		Set_dbcr(fdc, fdc->c_csb.csb_dmacookie.dmac_size);
		Set_dacr(fdc, fdc->c_csb.csb_dmacookie.dmac_laddress);

		/* Program the DCSR */

		if (fdc->c_csb.csb_dma_read == CSB_DMA_READ)
			Set_dcsr(fdc, DCSR_INIT_BITS | DCSR_WRITE);
		else
			Set_dcsr(fdc, DCSR_INIT_BITS);



	}

	/*
	 * I saw this (chip unexpectedly busy) happen when i shoved the
	 * floppy into the drive while
	 * running a dd if= /dev/rfd0c.	so it *is* possible for this to happen.
	 * we need to do a ctlr reset ...
	 */

	if (Msr(fdc) & CB) {
		/* tried to give command to chip when it is busy! */
		FDERRPRINT(FDEP_L3, FDEM_EXEC,
		    (C, "fdc: unexpectedly busy-stat 0x%x\n", Msr(fdc)));
		csb->csb_cmdstat = 1;	/* XXX TBD ERRS NYD for now */

		(void) fd_unbind_handle(fdc);
		return (EBUSY);
	}

	/* Give command to the controller */
	for (i = 0; i < (int)csb->csb_ncmds; i++) {

		/* Test the readiness of the controller to receive the cmd */
		for (to = FD_CRETRY; to; to--) {
			if ((Msr(fdc) & (DIO|RQM)) == RQM)
				break;
		}
		if (to == 0) {
			FDERRPRINT(FDEP_L2, FDEM_EXEC,
			    (C, "fdc: no RQM - stat 0x%x\n", Msr(fdc)));
			csb->csb_cmdstat = 1;

			(void) fd_unbind_handle(fdc);
			return (EIO);
		}

		Set_Fifo(fdc, csb->csb_cmds[i]);

		FDERRPRINT(FDEP_L1, FDEM_EXEC,
		    (C, "fdexec: sent 0x%x, Msr 0x%x\n", csb->csb_cmds[i],
		    Msr(fdc)));

	}


	/*
	 * Start watchdog timer on data transfer type commands - required
	 * in case a diskette is not present or is unformatted
	 */
	if (csb->csb_opflags & CSB_OFTIMEIT) {
		fdc->c_timeid = timeout(fdwatch, a,
		    tosec * drv_usectohz(1000000));
	}

	FDERRPRINT(FDEP_L1, FDEM_EXEC,
	    (C, "fdexec: cmd sent, Msr 0x%x\n", Msr(fdc)));

	/* If the operation has no results - then just return */
	if (csb->csb_opflags & CSB_OFNORESULTS) {
		if (fdc->c_fdtype & FDCTYPE_82077) {
			fdc->c_mtimeid = timeout(fdmotoff, a, Motoff_delay);
		}
		FDERRPRINT(FDEP_L1, FDEM_EXEC, (C, "fdexec: O K ..\n"));

		/*
		 * An operation that has no results isn't doing DMA so,
		 * there is no reason to try to unbind a handle
		 */
		return (0);
	}

	/*
	 * If this operation has no interrupt AND an immediate result
	 * then we just busy wait for the results and stuff them into
	 * the csb
	 */
	if (csb->csb_opflags & CSB_OFIMMEDIATE) {
		to = FD_RRETRY;
		csb->csb_nrslts = 0;
		/*
		 * Wait while this command is still going on.
		 */
		while ((tmp = Msr(fdc)) & CB) {
			/*
			 * If RQM + DIO, then a result byte is at hand.
			 */
			if ((tmp & (RQM|DIO|CB)) == (RQM|DIO|CB)) {
				csb->csb_rslt[csb->csb_nrslts++] =
								Fifo(fdc);
				/*
				FDERRPRINT(FDEP_L4, FDEM_EXEC,
				    (C, "fdexec: got result 0x%x\n",
				    csb->csb_nrslts));
				*/
			} else if (--to == 0) {
				FDERRPRINT(FDEP_L4, FDEM_EXEC,
				    (C, "fdexec: timeout, Msr%x, nr%x\n",
				    Msr(fdc), csb->csb_nrslts));

				csb->csb_status = 2;
				if (fdc->c_fdtype & FDCTYPE_82077) {
					fdc->c_mtimeid = timeout(fdmotoff, a,
							    Motoff_delay);
				}
				/*
				 * There is no DMA happening.  No need to
				 * try freeing a handle.
				 */

				return (EIO);
			}
		}
	}

	/*
	 * If told to sleep here, well then sleep!
	 */

	if (flags & FDXC_SLEEP) {
		fdc->c_flags |= FDCFLG_WAITING;
		while (fdc->c_flags & FDCFLG_WAITING) {
			cv_wait(&fdc->c_iocv, &fdc->c_lolock);
		}
	}

	/*
	 * kludge for end-of-cylinder error which must be ignored!!!
	 */

	if ((fdc->c_fdtype & FDCTYPE_TCBUG) &&
	    ((csb->csb_rslt[0] & IC_SR0) == 0x40) &&
	    (csb->csb_rslt[1] & EN_SR1))
		csb->csb_rslt[0] &= ~IC_SR0;

	/*
	 * See if there was an error detected, if so, fdrecover()
	 * will check it out and say what to do.
	 *
	 * Don't do this, though, if this was the Sense Drive Status
	 * or the Dump Registers command.
	 */
	if (((csb->csb_rslt[0] & IC_SR0) || (fdc->c_csb.csb_dcsr_rslt) ||
		(csb->csb_status)) &&
		((csb->csb_cmds[0] != FDRAW_SENSE_DRV) &&
		(csb->csb_cmds[0] != DUMPREG))) {
		/* if it can restarted OK, then do so, else return error */
		if (fdrecover(fdc) != 0) {
			if (fdc->c_fdtype & FDCTYPE_82077) {
				fdc->c_mtimeid = timeout(fdmotoff, a,
							Motoff_delay);
			}

			/*
			 * If this was a dma transfer, unbind the handle so
			 * that other transfers may use it.
			 */

			(void) fd_unbind_handle(fdc);
			return (EIO);
		} else {
			/* ASSUMES that cmd is still intact in csb */
			goto retry;
		}
	}

	/* things went ok */
	if (fdc->c_fdtype & FDCTYPE_82077) {
		fdc->c_mtimeid = timeout(fdmotoff, a, Motoff_delay);
	}
	FDERRPRINT(FDEP_L1, FDEM_EXEC, (C, "fdexec: O K ..........\n"));

	if (fd_unbind_handle(fdc))
		return (EIO);

	return (0);
}

/*
 * Turn on the drive's motor
 *
 *	- called with the low level lock held
 */
static void
fdexec_turn_on_motor(struct fdctlr *fdc, int flags,  u_int unit)
{
	clock_t local_lbolt;
	int timeid;


	/*
	 * The low level mutex may not be held over the call to
	 * untimeout().  See the manpage for details.
	 */
	timeid = fdc->c_mtimeid;
	fdc->c_mtimeid = 0;
	mutex_exit(&fdc->c_lolock);
	(void) untimeout(timeid);
	mutex_enter(&fdc->c_lolock);

	ASSERT(fdc->c_un->un_unit_no == unit);


	set_rotational_speed(fdc, unit);

	if (!(Dor(fdc) & (MOTEN(unit)))) {
		/*
		 * Turn on the motor
		 */
		FDERRPRINT(FDEP_L1, FDEM_EXEC,
			(C, "fdexec: turning on motor\n"));

		/* LINTED */
		Set_dor(fdc, (MOTEN(unit)), 1);

		if (flags & FDXC_SLEEP) {
			drv_getparm(LBOLT,
				(unsigned long *)&local_lbolt);
			(void) (cv_timedwait(&fdc->c_motoncv,
			&fdc->c_lolock, local_lbolt + Moton_delay));
		} else {
			drv_usecwait(1000000);
		}
	}

}

/*
 * fdrecover
 *	see if possible to retry an operation.
 *	All we can do is restart the operation.	 If we are out of allowed
 *	retries - return non-zero so that the higher levels will be notified.
 *
 * RETURNS: 0 if ok to restart, !0 if can't or out of retries
 *	- called with the low level lock held
 */
static int
fdrecover(struct fdctlr *fdc)
{
	struct fdcsb *csb;

	FDERRPRINT(FDEP_L1, FDEM_RECO, (C, "fdrecover\n"));
	csb = &fdc->c_csb;

	if (fdc->c_flags & FDCFLG_TIMEDOUT) {
		struct fdcsb savecsb;

		fdc->c_flags ^= FDCFLG_TIMEDOUT;
		csb->csb_rslt[1] |= TO_SR1;
		FDERRPRINT(FDEP_L1, FDEM_RECO,
		    (C, "fd%d: %s timed out\n", csb->csb_unit,
		    fdcmds[csb->csb_cmds[0] & 0x1f].cmdname));

		/* use private csb */
		savecsb = fdc->c_csb;
		bzero(&fdc->c_csb, sizeof (struct fdcsb));
		FDERRPRINT(FDEP_L1, FDEM_RECO, (C, "fdc: resetting\n"));
		(void) fdreset(fdc);
		/* check change first?? */
		/* don't ckchg in fdexec, too convoluted */
		(void) fdrecalseek(fdc, savecsb.csb_unit, -1, 0);
		fdc->c_csb = savecsb; /* restore original csb */
	}

	/*
	 * gather statistics on errors
	 */
	if (csb->csb_rslt[1] & DE_SR1) {
		fdc->fdstats.de++;
	}
	if (csb->csb_rslt[1] & OR_SR1) {
		fdc->fdstats.run++;
	}
	if (csb->csb_rslt[1] & (ND_SR1+MA_SR1)) {
		fdc->fdstats.bfmt++;
	}
	if (csb->csb_rslt[1] & TO_SR1) {
		fdc->fdstats.to++;
	}

	/*
	 * If raw ioctl don't examine results just pass status
	 * back via fdraw. Raw commands are timed too, so put this
	 * after the above check.
	 */
	if (csb->csb_opflags & CSB_OFRAWIOCTL) {
		return (1);
	}


	/*
	 * if there was a pci bus error, do not retry
	 */

	    if (csb->csb_dcsr_rslt == 1) {
			FDERRPRINT(FDEP_L3, FDEM_RECO,
			    (C, "fd%d: host bus error\n", 0));

		return (1);
	    }

	/*
	 * If there was an error with the DMA functions, do not retry
	 */
	if (csb->csb_dma_rslt == 1) {
			FDERRPRINT(FDEP_L1, FDEM_RECO,
			    (C, "fd%d: DMA interface error\n", csb->csb_unit));

		return (1);
	}


	/*
	 * if we have run out of retries, return an error
	 * XXX need better status interp
	 */

	csb->csb_retrys++;
	if (csb->csb_retrys > csb->csb_maxretry) {
		FDERRPRINT(FDEP_L3, FDEM_RECO,
		    (C, "fd%d: %s failed (%x %x %x)\n",
		    0, fdcmds[csb->csb_cmds[0] & 0x1f].cmdname,
		    csb->csb_rslt[0], csb->csb_rslt[1], csb->csb_rslt[2]));
		if (csb->csb_rslt[1] & NW_SR1) {
			FDERRPRINT(FDEP_L3, FDEM_RECO,
			    (C, "fd%d: not writable\n", 0));
		}
		if (csb->csb_rslt[1] & DE_SR1) {
			FDERRPRINT(FDEP_L3, FDEM_RECO,
			    (C, "fd%d: crc error blk %d\n", 0,
			    (int)fdc->c_current->b_blkno));
		}
		if (csb->csb_rslt[1] & OR_SR1) {
			FDERRPRINT(FDEP_L3, FDEM_RECO,
			    (C, "fd%d: over/underrun\n", 0));
		}

		if (csb->csb_rslt[1] & (ND_SR1+MA_SR1)) {
			FDERRPRINT(FDEP_L3, FDEM_RECO,
			    (C, "fd%d: bad format\n", 0));
		}

		if (csb->csb_rslt[1] & TO_SR1) {
			FDERRPRINT(FDEP_L3, FDEM_RECO,
			    (C, "fd%d: timeout\n", 0));
		}

		csb->csb_cmdstat = 1; /* failed - give up */
		return (1);
	}

	if (csb->csb_opflags & CSB_OFSEEKOPS) {
		/* seek, recal type commands - just look at st0 */
		FDERRPRINT(FDEP_L2, FDEM_RECO,
		    (C, "fd%d: %s error : st0 0x%x\n", csb->csb_unit,
		    fdcmds[csb->csb_cmds[0] & 0x1f].cmdname,
		    csb->csb_rslt[0]));
	}
	if (csb->csb_opflags & CSB_OFXFEROPS) {
		/* rd, wr, fmt type commands - look at st0, st1, st2 */
		FDERRPRINT(FDEP_L2, FDEM_RECO,
		    (C, "fd%d: %s error : st0=0x%x st1=0x%x st2=0x%x\n",
		    csb->csb_unit, fdcmds[csb->csb_cmds[0] & 0x1f].cmdname,
		    csb->csb_rslt[0], csb->csb_rslt[1], csb->csb_rslt[2]));
	}

	return (0);	/* tell fdexec to retry */
}

/*
 * Interrupt handle for DMA
 */
static u_int
fdintr_dma()
{
	struct fdctlr   *fdc;
	off_t		off;
	off_t		len;
	u_int		ccount;
	u_int		windex;
	u_int		done = 0;
	long		tmp_dcsr;
	int		to;
	u_char		tmp;
	long		i = 0;
	int		res = DDI_INTR_UNCLAIMED;
	int		timeid;




	/* search for a controller that's expecting an interrupt */
	fdc = fdctlrs;

	if (fdc->c_csb.csb_opmode == 0x0) {
		fdc->c_csb.csb_opmode = 2;
	}


	tmp_dcsr = Get_dcsr(fdc);

	if (!(tmp_dcsr & DCSR_INT_PEND) && !(DCSR_ERR_PEND & tmp_dcsr))
		return (res);

	mutex_enter(&fdc->c_hilock);

	/*
	 * An interrupt can come from either the floppy controller or
	 * or the DMA engine.  The DMA engine will only issue an
	 * interrupt if there was an error.
	 */

	switch (fdc->c_csb.csb_opmode) {
		case 0x1:
			/* read/write/format data-xfer case */

			FDERRPRINT(FDEP_L1, FDEM_INTR,
				(C, "fdintr_dma: opmode 1\n"));

			/*
			 * See if the interrupt is from the floppy
			 * controller.  If there is, take out the status bytes.
			 */

			if (tmp_dcsr & DCSR_INT_PEND) {

				FDERRPRINT(FDEP_L1, FDEM_INTR,
					(C, "fdintr_dma: INT_PEND \n"));

				res = DDI_INTR_CLAIMED;

				to = FD_RRETRY;
				fdc->c_csb.csb_nrslts = 0;

				/* check status */
				i = 0;

				/*
				 * CB turns off once all the result bytes are
				 *  read.
				 *
				 * NOTE: the counters are there so that the
				 * handler will never get stuck in a loop.
				 * If the counters do reach their maximum
				 * values, then a catastrophic error has
				 * occurred.  This should never be the case.
				 * The counters only came into play during
				 * development.
				 */
				while (((tmp = Msr(fdc)) & CB) &&
					(i < 1000001)) {

					/*
					 * If RQM + DIO, then a result byte
					 * is at hand.
					 */
					if ((tmp & (RQM|DIO|CB)) ==
								(RQM|DIO|CB)) {
						fdc->c_csb.csb_rslt
						[fdc->c_csb.csb_nrslts++]
							    = Fifo(fdc);

						FDERRPRINT(FDEP_L1, FDEM_INTR,
						(C, "fdintr_dma: res 0x%x\n",
							fdc->c_csb.csb_rslt
							[fdc->c_csb.csb_nrslts
							- 1]));

					} else if (--to == 0) {
						/*
						 * controller was never
						 * ready to give results
						 */
						fdc->c_csb.csb_status = 2;
						break;
					}
				i++;
				}
				if (i == 10000) {
					FDERRPRINT(FDEP_L1, FDEM_INTR,
						(C, "First loop overran\n"));
				}
			}

			/*
			 * See if the interrupt is from the DMA engine,
			 * which will only interrupt on an error
			 */
			if (tmp_dcsr & DCSR_ERR_PEND) {

				res = DDI_INTR_CLAIMED;

				done = 1;
				fdc->c_csb.csb_dcsr_rslt = 1;
				FDERRPRINT(FDEP_L1, FDEM_INTR,
					(C, "fdintr_dma: Error pending\n"));
				Reset_dcsr(fdc);

				break;
			}

			/* TCBUG kludge */
			if ((fdc->c_fdtype & FDCTYPE_TCBUG) &&
				((fdc->c_csb.csb_rslt[0] & IC_SR0) == 0x40) &&
				(fdc->c_csb.csb_rslt[1] & EN_SR1)) {

				fdc->c_csb.csb_rslt[0] &= ~IC_SR0;

				fdc->c_csb.csb_rslt[1] &= ~EN_SR1;


			}


			/* Exit if there were errors in the DMA */
			if (((fdc->c_csb.csb_rslt[0] & IC_SR0) != 0) ||
			    (fdc->c_csb.csb_rslt[1] != 0) ||
			    (fdc->c_csb.csb_rslt[2] != 0)) {
				done = 1;
				FDERRPRINT(FDEP_L1, FDEM_INTR,
					(C, "fdintr_dma: errors in command\n"));

				break;
			}


			FDERRPRINT(FDEP_L1, FDEM_INTR,
				(C, "fdintr_dma: dbcr 0x%x\n", Get_dbcr(fdc)));
			/*
			 * The csb_ccount is the number of cookies that still
			 * need to be processed.  A cookie was just processed
			 * so decrement the cookie counter.
			 */
			fdc->c_csb.csb_ccount--;
			ccount = fdc->c_csb.csb_ccount;

			windex = fdc->c_csb.csb_windex;

			/*
			 * If there are no more cookies and all the windows
			 * have been DMA'd, then DMA is done.
			 *
			 */
			if ((ccount == 0) && (windex == fdc->c_csb.csb_nwin)) {

				done = 1;

				/*
				 * The handle is unbound in fdexec
				 */

				break;
			}

			if (ccount != 0) {
				/* process the next cookie */
				ddi_dma_nextcookie(
						fdc->c_dmahandle,
						&fdc->c_csb.csb_dmacookie);

				FDERRPRINT(FDEP_L1, FDEM_INTR,
				(C, "cookie addr 0x%x\n",
				fdc->c_csb.csb_dmacookie.dmac_laddress));

				FDERRPRINT(FDEP_L1, FDEM_INTR,
				(C, "cookie length %d\n",
				fdc->c_csb.csb_dmacookie.dmac_size));

			} else {

				ddi_dma_getwin(
					fdc->c_dmahandle,
					fdc->c_csb.csb_windex,
					&off, (uint_t *)&len,
					&fdc->c_csb.csb_dmacookie,
					&fdc->c_csb.csb_ccount);
				fdc->c_csb.csb_windex++;

				FDERRPRINT(FDEP_L1, FDEM_INTR,
				(C, "fdintr_dma: process %d window\n",
				fdc->c_csb.csb_windex));

				FDERRPRINT(FDEP_L1, FDEM_INTR,
				(C, "fdintr_dma: process no. cookies %d\n",
				fdc->c_csb.csb_ccount));


				FDERRPRINT(FDEP_L1, FDEM_INTR,
				(C, "cookie addr 0x%x\n",
				fdc->c_csb.csb_dmacookie.dmac_laddress));

				FDERRPRINT(FDEP_L1, FDEM_INTR,
				(C, "cookie length %d\n",
				fdc->c_csb.csb_dmacookie.dmac_size));

			}

			/*
			 * Program the DMA engine with the length and
			 * the address of the transfer
			 */

			ASSERT(fdc->c_csb.csb_dmacookie.dmac_size);

			Set_dbcr(fdc, fdc->c_csb.csb_dmacookie.dmac_size);
			Set_dacr(fdc, fdc->c_csb.csb_dmacookie.dmac_laddress);

			FDERRPRINT(FDEP_L1, FDEM_INTR,
				    (C,
				    "fdintr_dma: size 0x%x\n",
					fdc->c_csb.csb_dmacookie.dmac_size));


			/* reprogram the controller */
			fdc->c_csb.csb_cmds[2] = fdc->c_csb.csb_rslt[3];
			fdc->c_csb.csb_cmds[3] = fdc->c_csb.csb_rslt[4];
			fdc->c_csb.csb_cmds[4] = fdc->c_csb.csb_rslt[5];
			fdc->c_csb.csb_cmds[1] = (fdc->c_csb.csb_cmds[1]
				& ~0x04) | (fdc->c_csb.csb_rslt[4] << 2);

			for (i = 0; i < (int)fdc->c_csb.csb_ncmds; i++) {

				/*
				 * Test the readiness of the controller
				 * to receive the cmd
				 */
				for (to = FD_CRETRY; to; to--) {
					if ((Msr(fdc) & (DIO|RQM)) == RQM)
						break;
				}
				if (to == 0) {
					FDERRPRINT(FDEP_L2, FDEM_EXEC,
					(C,
					"fdc: no RQM - stat 0x%x\n", Msr(fdc)));
					/* stop the DMA from happening */
					fdc->c_csb.csb_status = 2;
					done = 1;
					break;
				}

				Set_Fifo(fdc, fdc->c_csb.csb_cmds[i]);

				FDERRPRINT(FDEP_L1, FDEM_INTR,
					(C,
					"fdintr_dma: sent 0x%x, Msr 0x%x\n",
					fdc->c_csb.csb_cmds[i], Msr(fdc)));
			}

			/* reenable DMA */
			if (!done)
				Set_dcsr(fdc, tmp_dcsr | DCSR_EN_DMA);
			break;

		case 0x2:
		/* seek/recal type cmd */
			FDERRPRINT(FDEP_L1, FDEM_INTR,
				(C, "fintr_dma: opmode 2\n"));

			/*
			 *  See if the interrupt is from the DMA engine,
			 *  which will only interrupt if there was an error.
			 */
			if (tmp_dcsr & DCSR_ERR_PEND) {
				res = DDI_INTR_CLAIMED;
				done = 1;
				fdc->c_csb.csb_dcsr_rslt = 1;
				Reset_dcsr(fdc);
				break;
			}


			/* See if the interrupt is from the floppy controller */
			if (tmp_dcsr & DCSR_INT_PEND) {

				res = DDI_INTR_CLAIMED;


				/*
				 * Wait until there's no longer a command
				 * in progress
				 */

				FDERRPRINT(FDEP_L1, FDEM_INTR,
					(C, "fdintr_dma: interrupt pending\n"));
				i = 0;
				while (((Msr(fdc) & CB)) && (i < 10000)) {
					i++;
				}

				if (i == 10000)
					FDERRPRINT(FDEP_L1, FDEM_INTR,
						(C, "2nd loop overran !!!\n"));

				/*
				 * Check the RQM bit to see if the controller is
				 * ready to transfer status of the command.
				 */
				i = 0;
				while ((!(Msr(fdc) & RQM)) && (i < 10000)) {
					i++;
				}

				if (i == 10000)
					FDERRPRINT(FDEP_L1, FDEM_INTR,
					    (C, "3rd loop overran !!!\n"));

				/*
				 * Issue the Sense Interrupt Status Command
				 */
				Set_Fifo(fdc, SNSISTAT);

				i = 0;
				while ((!(Msr(fdc) & RQM)) && (i < 10000)) {
					i++;
				}
				if (i == 10000)
					FDERRPRINT(FDEP_L1, FDEM_INTR,
					(C, "4th loop overran !!!\n"));

				/* Store the first result byte */
				fdc->c_csb.csb_rslt[0] = Fifo(fdc);

				i = 0;
				while ((!(Msr(fdc) & RQM)) && (i < 10000)) {
					i++;
				}
				if (i == 10000)
					FDERRPRINT(FDEP_L1, FDEM_INTR,
					(C, "5th loop overran !!!\n"));

				/* Store the second  result byte */
				fdc->c_csb.csb_rslt[1] = Fifo(fdc);


				done = 1;
			}

		}

	/* Make signal and get out of interrupt handler */
	if (done) {

		mutex_enter(&fdc->c_lolock);
		fdc->c_csb.csb_opmode = 0;

		/*  reset watchdog timer if armed and not already triggered */


		if (fdc->c_timeid) {
			timeid = fdc->c_timeid;
			fdc->c_timeid = 0;
			mutex_exit(&fdc->c_lolock);
			(void) untimeout(timeid);
			mutex_enter(&fdc->c_lolock);
		}


		if (fdc->c_flags & FDCFLG_WAITING) {
			/*
			 * somebody's waiting on finish of fdctlr/csb,
			 * wake them
			 */

			FDERRPRINT(FDEP_L1, FDEM_INTR,
				(C, "fdintr_dma: signal the waiter\n"));

			fdc->c_flags ^= FDCFLG_WAITING;
			cv_signal(&fdc->c_iocv);

			/*
			* FDCFLG_BUSY is NOT cleared, NOR is the csb given back;
			* the operation just finished can look at the csb
			 */
		} else {
			FDERRPRINT(FDEP_L3, FDEM_INTR,
				(C, "fdintr_dma: nobody sleeping (%x %x %x)\n",
			fdc->c_csb.csb_rslt[0], fdc->c_csb.csb_rslt[1],
			fdc->c_csb.csb_rslt[2]));
		}
		mutex_exit(&fdc->c_lolock);
	}
	/* update high level interrupt counter */
	if (fdc->c_intrstat)
			KIOIP->intrs[KSTAT_INTR_HARD]++;


	FDERRPRINT(FDEP_L1, FDEM_INTR, (C, "fdintr_dma: done\n"));
	mutex_exit(&fdc->c_hilock);
	return (res);
}

/*
 * fd_lointr
 *	This is the low level SW interrupt handler triggered by the high
 *	level interrupt handler (or by fdwatch).
 */
static u_int
fd_lointr(caddr_t arg)
{
	register struct fdctlr *fdc = (struct fdctlr *)arg;
	register struct fdcsb *csb;
	int timeid;

	csb = &fdc->c_csb;
	FDERRPRINT(FDEP_L1, FDEM_INTR, (C, "fdintr: opmode %d\n",
	    csb->csb_opmode));
	/*
	 * Check that lowlevel interrupt really meant to trigger us.
	 */
	if (csb->csb_opmode != 4) {
		/*
		 * This should probably be protected, but, what the
		 * heck...the cost isn't worth the accuracy for this
		 * statistic.
		 */
		if (fdc->c_intrstat)
			KIOIP->intrs[KSTAT_INTR_SPURIOUS]++;
		return (DDI_INTR_UNCLAIMED);
	}

	mutex_enter(&fdc->c_lolock);
	csb->csb_opmode = 0;

	/*  reset watchdog timer if armed and not already triggered */
	if (fdc->c_timeid) {
		timeid = fdc->c_timeid;
		fdc->c_timeid = 0;
		mutex_exit(&fdc->c_lolock);
		(void) untimeout(timeid);
		mutex_enter(&fdc->c_lolock);

	}

	if (fdc->c_flags & FDCFLG_WAITING) {
		/*
		 * somebody's waiting on finish of fdctlr/csb, wake them
		 */
		fdc->c_flags ^= FDCFLG_WAITING;
		cv_signal(&fdc->c_iocv);

		/*
		 * FDCFLG_BUSY is NOT cleared, NOR is the csb given back; so
		 * the operation just finished can look at the csb
		 */
	} else {
		FDERRPRINT(FDEP_L3, FDEM_INTR,
		    (C, "fdintr: nobody sleeping (%x %x %x)\n",
		    csb->csb_rslt[0], csb->csb_rslt[1], csb->csb_rslt[2]));
	}
	if (fdc->c_intrstat)
		KIOIP->intrs[KSTAT_INTR_SOFT]++;
	mutex_exit(&fdc->c_lolock);
	return (DDI_INTR_CLAIMED);
}

/*
 * fdwatch
 *	is called from timein() when a floppy operation has expired.
 */
static void
fdwatch(caddr_t arg)
{
	register struct fdctlr *fdc = (struct fdctlr *)arg;
	int old_opmode;
	struct fdcsb *csb;

	FDERRPRINT(FDEP_L1, FDEM_WATC,
				(C, "fdwatch\n"));


	mutex_enter(&fdc->c_lolock);
	if (fdc->c_timeid == 0) {
		/*
		 * fdintr got here first, ergo, no timeout condition..
		 */

		FDERRPRINT(FDEP_L1, FDEM_WATC,
				(C, "fdwatch: no timeout\n"));

		mutex_exit(&fdc->c_lolock);
		return;
	}
	fdc->c_timeid = 0;
	csb = &fdc->c_csb;

	mutex_enter(&fdc->c_hilock);
	/*
	 * XXXX: We should probably reset the bloody chip
	 */
	old_opmode = csb->csb_opmode;

	FDERRPRINT(FDEP_L1, FDEM_WATC,
	    (C, "fd%d: timeout, opmode:%d\n", csb->csb_unit, old_opmode));

	csb->csb_opmode = 4;
	mutex_exit(&fdc->c_hilock);

	FDERRPRINT(FDEP_L1, FDEM_WATC, (C, "fdwatch: cmd %s timed out\n",
				fdcmds[csb->csb_cmds[0] & 0x1f].cmdname));
	fdc->c_flags |= FDCFLG_TIMEDOUT;
	csb->csb_status = CSB_CMDTO;

	if ((fdc->c_fdtype & FDCTYPE_DMA) == 0) {
		ddi_trigger_softintr(fdc->c_softid);
		KIOIP->intrs[KSTAT_INTR_WATCHDOG]++;
		mutex_exit(&fdc->c_lolock);
	} else {
		mutex_exit(&fdc->c_lolock);
		fd_lointr((caddr_t)fdctlrs);
	};
}
/*
 * fdgetcsb
 *	wait until the csb is free
 */
static void
fdgetcsb(struct fdctlr *fdc)
{
	FDERRPRINT(FDEP_L1, FDEM_GETC, (C, "fdgetcsb\n"));
	ASSERT(mutex_owned(&fdc->c_lolock));
	while (fdc->c_flags & FDCFLG_BUSY) {
		fdc->c_flags |= FDCFLG_WANT;
		cv_wait(&fdc->c_csbcv, &fdc->c_lolock);
	}
	fdc->c_flags |= FDCFLG_BUSY; /* got it! */
}

/*
 * fdretcsb
 *	return csb
 */
static void
fdretcsb(struct fdctlr *fdc)
{

	ASSERT(mutex_owned(&fdc->c_lolock));
	FDERRPRINT(FDEP_L1, FDEM_RETC, (C, "fdretcsb\n"));
	fdc->c_flags &= ~FDCFLG_BUSY; /* let go */

	fdc->c_csb.csb_dma_read = 0;

	if (fdc->c_flags & FDCFLG_WANT) {
		fdc->c_flags ^= FDCFLG_WANT;
		/*
		 * broadcast the signal.  One thread will wake up and
		 * set the flags to FDCFLG_BUSY.  If more than one thread is
		 * waiting then each thread will wake up in turn.  The first
		 * thread to wake-up will set the FDCFLG_BUSY flag and the
		 * subsequent threads will will wake-up, but reset the
		 * flag to FDCFLG_WANT because the FDCFLG_BUSY bit is set.
		 */
		cv_broadcast(&fdc->c_csbcv);
	}
}


/*
 * fdreset
 *	reset THE controller, and configure it to be
 *	the way it ought to be
 * ASSUMES: that it already owns the csb/fdctlr!
 *
 *	- called with the low level lock held
 */
static int
fdreset(register struct fdctlr *fdc)
{
	register struct fdcsb *csb;
	clock_t local_lbolt = 0;
	int timeid;

	FDERRPRINT(FDEP_L1, FDEM_RESE, (C, "fdreset\n"));

	ASSERT(mutex_owned(&fdc->c_lolock));


	/* count resets */
	fdc->fdstats.reset++;

	/*
	 * On the 82077, the DSR will clear itself after a reset.  Upon exiting
	 * the reset, a polling interrupt will be generated.  If the floppy
	 * interrupt is enabled, it's possible for cv_signal() to be called
	 * before cv_wait().  This will cause the system to hang.  Turn off
	 * the floppy interrupt to avoid this race condition
	 */
	if ((fdc->c_fdtype & FDCTYPE_CTRLMASK) == FDCTYPE_82077) {
		/* LINTED */
		Set_dor(fdc, DMAGATE, 0);
		FDERRPRINT(FDEP_L1, FDEM_RESE, (C, "fdreset: set dor\n"));
	}

	/* toggle software reset */
	Dsr(fdc, SWR);

	drv_usecwait(5);

	FDERRPRINT(FDEP_L1, FDEM_RESE,
			(C, "fdreset: toggled software reset\n"));

	/*
	 * This sets the data rate to 500Kbps (for high density)
	 * XXX should use current characteristics instead XXX
	 */
	Dsr(fdc, 0);
	drv_usecwait(5);
	switch (fdc->c_fdtype & FDCTYPE_CTRLMASK) {
	case FDCTYPE_82077:

		/* Do not hold the mutex across the untimeout call */
		timeid = fdc->c_mtimeid;
		fdc->c_mtimeid = 0;
		mutex_exit(&fdc->c_lolock);
		(void) untimeout(timeid);
		mutex_enter(&fdc->c_lolock);

		/*
		 * when we bring the controller out of reset it will generate
		 * a polling interrupt. fdintr() will field it and schedule
		 * fd_lointr(). There will be no one sleeping but we are
		 * expecting an interrrupt so....
		 */
		fdc->c_flags |= FDCFLG_WAITING;

		/*
		 * The reset bit must be cleared to take the 077 out of
		 * reset state and the DMAGATE bit must be high to enable
		 * interrupts.
		 */
		/* LINTED */
		Set_dor(fdc, DMAGATE|RESET, 1);

		FDERRPRINT(FDEP_L1, FDEM_ATTA,
			(C, "fdattach: Dor 0x%x\n", Dor(fdc)));

		drv_getparm(LBOLT,
				(unsigned long *)&local_lbolt);
		if (cv_timedwait(&fdc->c_iocv, &fdc->c_lolock,
				local_lbolt + drv_usectohz(1000000)) == -1) {

			return (-1);

		}

		break;

	case FDCTYPE_82072:
	default:
		fdc->c_flags |= FDCFLG_WAITING;

		/*
		 * A timed wait is not used for the 82072 and the default case
		 * because it's possible for the timer to go off before the
		 * controller has a chance to interrupt.  The 82072 controller
		 * is found on the slower sun4c platforms.
		 */
		cv_wait(&fdc->c_iocv, &fdc->c_lolock);

		break;
	}
	csb = &fdc->c_csb;

	/* setup common things in csb */
	csb->csb_unit = fdc->c_un->un_unit_no;
	csb->csb_nrslts = 0;
	csb->csb_opflags = CSB_OFNORESULTS;
	csb->csb_maxretry = 0;
	csb->csb_retrys = 0;

	/* send SPECIFY command to fdc */
	/* csb->unit is don't care */
	csb->csb_cmds[0] = FDRAW_SPECIFY;
	csb->csb_cmds[1] = fdspec[0]; /* step rate, head unload time */
	if (fdc->c_fdtype & FDCTYPE_DMA)
		csb->csb_cmds[2] =  SPEC_DMA_MODE;
	else
		csb->csb_cmds[2] = fdspec[1];  /* head load time, DMA mode */

	csb->csb_ncmds = 3;

	/* XXX for now ignore errors, they "CAN'T HAPPEN" */
	(void) fdexec(fdc, 0);	/* no FDXC_CHECKCHG, ... */
	/* no results */

	/* send CONFIGURE command to fdc */
	/* csb->unit is don't care */
	csb->csb_cmds[0] = CONFIGURE;
	csb->csb_cmds[1] = fdconf[0]; /* motor info, motor delays */
	csb->csb_cmds[2] = fdconf[1]; /* enaimplsk, disapoll, fifothru */
	csb->csb_cmds[3] = fdconf[2]; /* track precomp */
	csb->csb_ncmds = 4;

	csb->csb_retrys = 0;

	/* XXX for now ignore errors, they "CAN'T HAPPEN" */
	(void) fdexec(fdc, 0);	/* no FDXC_CHECKCHG, ... */

	return (0);
}

/*
 * fdrecalseek
 *	performs recalibrates or seeks if the "arg" is -1 does a
 *	recalibrate on a drive, else it seeks to the cylinder of
 *	the drive.  The recalibrate is also used to find a drive,
 *	ie if the drive is not there, the controller says "error"
 *	on the operation
 * NOTE: that there is special handling of this operation in the hardware
 * interrupt routine - it causes the operation to appear to have results;
 * ie the results of the SENSE INTERRUPT STATUS that the hardware interrupt
 * function did for us.
 * NOTE: because it uses sleep/wakeup it must be protected in a critical
 * section so create one before calling it!
 *
 * RETURNS: 0 for ok,
 *	else	errno from fdexec,
 *	or	ENODEV if error (infers hardware type error)
 *
 *	- called with the low level lock held
 */
static int
fdrecalseek(struct fdctlr *fdc, int unit, int arg, int execflg)
{
	register struct fdcsb *csb;
	int result;

	ASSERT(fdc->c_un->un_unit_no == unit);

	FDERRPRINT(FDEP_L1, FDEM_RECA, (C, "fdrecalseek to %d\n", arg));

	/* XXX TODO: check see argument for <= num cyls OR < 256 */

	csb = &fdc->c_csb;
	csb->csb_unit = (u_char) unit;
	csb->csb_cmds[1] = unit & 0x03;

	if (arg == -1) {			/* is recal... */
		csb->csb_cmds[0] = FDRAW_REZERO;
		csb->csb_ncmds = 2;
	} else {
		csb->csb_cmds[0] = FDRAW_SEEK;
		csb->csb_cmds[2] = (u_char)arg;
		csb->csb_ncmds = 3;
	}
	csb->csb_nrslts = 2;	/* 2 for SENSE INTERRUPTS */
	csb->csb_opflags = CSB_OFSEEKOPS | CSB_OFTIMEIT;
	/*
	 * MAYBE NYD need to set retries to different values? - depending on
	 * drive characteristics - if we get to high capacity drives
	 */
	csb->csb_maxretry = skretry;
	csb->csb_retrys = 0;

	/* send cmd off to fdexec */
	if (result = fdexec(fdc, FDXC_SLEEP | execflg)) {
		goto out;
	}

	/*
	 * if recal, test for equipment check error
	 * ASSUMES result = 0 from above call
	 */
	if (arg == -1) {
		result = 0;
	} else {
		/* for seeks, any old error will do */
		if ((csb->csb_rslt[0] & IC_SR0) || csb->csb_cmdstat)
			result = ENODEV;
	}

out:
	return (result);
}

/*
 * fdsensedrv
 *	do a sense_drive command.  used by fdopen and fdcheckdisk.
 *
 *	- called with the lock held
 */
static int
fdsensedrv(register struct fdctlr *fdc, int unit)
{
	struct fdcsb *csb;

	ASSERT(fdc->c_un->un_unit_no == unit);

	csb = &fdc->c_csb;

	/* setup common things in csb */
	csb->csb_unit = (u_char) unit;
	csb->csb_opflags = CSB_OFIMMEDIATE;
	csb->csb_cmds[0] = FDRAW_SENSE_DRV;
	/* MOT bit set means don't delay */
	csb->csb_cmds[1] = MOT | (unit & 0x03);
	csb->csb_ncmds = 2;
	csb->csb_nrslts = 1;
	csb->csb_maxretry = skretry;
	csb->csb_retrys = 0;

	/* XXX for now ignore errors, they "CAN'T HAPPEN" */
	(void) fdexec(fdc, 0);	/* DON't check changed!, no sleep */

	FDERRPRINT(FDEP_L1, FDEM_CHEK,
		(C, "fdsensedrv: result 0x%x", csb->csb_rslt[0]));

	return (csb->csb_rslt[0]); /* return status byte 3 */
}

/*
 * fdcheckdisk
 *	check to see if the disk is still there - do a recalibrate,
 *	then see if DSKCHG line went away, if so, diskette is in; else
 *	it's (still) out.
 */

static int
fdcheckdisk(register struct fdctlr *fdc, int unit)
{
	auto struct fdcsb savecsb;
	struct fdcsb *csb;
	int	err, st3;
	int	seekto;			/* where to seek for reset of DSKCHG */

	FDERRPRINT(FDEP_L1, FDEM_CHEK,
	    (C, "fdcheckdisk, unit %d\n", unit));

	ASSERT(fdc->c_un->un_unit_no == unit);

	/*
	 * save old csb
	 */

	csb = &fdc->c_csb;
	savecsb = fdc->c_csb;
	bzero((caddr_t)csb, sizeof (*csb));

	/*
	 * Read drive status to see if at TRK0, if so, seek to cyl 1,
	 * else seek to cyl 0.	We do this because the controller is
	 * "smart" enough to not send any step pulses (which are how
	 * the DSKCHG line gets reset) if it sees TRK0 'cause it
	 * knows the drive is already recalibrated.
	 */
	st3 = fdsensedrv(fdc, unit);

	/* check TRK0 bit in status */
	if (st3 & T0_SR3)
		seekto = 1;	/* at TRK0, seek out */
	else
		seekto = 0;

	/*
	 * DON'T recurse check changed
	 */
	err = fdrecalseek(fdc, unit, seekto, 0);



	/* "restore" old csb, check change state */
	fdc->c_csb = savecsb;

	/* any recal/seek errors are too serious to attend to */
	if (err) {
		FDERRPRINT(FDEP_L2, FDEM_CHEK,
		    (C, "fdcheckdisk err %d\n", err));
		return (err);
	}

	/*
	 * if disk change still asserted, no diskette in drive!
	 */
	if (fdsense_chng(fdc, csb->csb_unit)) {
		FDERRPRINT(FDEP_L2, FDEM_CHEK,
		    (C, "fdcheckdisk no disk\n"));
		return (1);
	}
	return (0);
}

/*
 *	fdselect() - select drive, needed for external to chip select logic
 *	fdeject() - ejects drive, must be previously selected
 *	fdsense_chng() - sense disk changed line from previously selected drive
 *		return s 1 is signal asserted, else 0
 */
/* ARGSUSED */
static void
fdselect(struct fdctlr *fdc, int unit, int on)
{

	ASSERT(fdc->c_un->un_unit_no == unit);

	FDERRPRINT(FDEP_L1, FDEM_DSEL,
	    (C, "fdselect, unit %d, on = %d\n", unit, on));

	switch (fdc->c_fdtype & FDCTYPE_AUXIOMASK) {
	case FDCTYPE_MACHIO:
		set_auxioreg(AUX_DRVSELECT, on);
		break;

	case FDCTYPE_SLAVIO:
	case FDCTYPE_CHEERIO:
		/* LINTED */

		FDERRPRINT(FDEP_L1, FDEM_ATTA,
			(C, "fdselect: (before) Dor 0x%x\n", Dor(fdc)));

		if (unit == 0) {
			Set_dor(fdc, DRVSEL, !on);
		} else {
			Set_dor(fdc, DRVSEL, on);
		}

		FDERRPRINT(FDEP_L1, FDEM_ATTA,
			(C, "fdselect: Dor 0x%x\n", Dor(fdc)));

		break;

	default:
		break;
	}
}

/* ARGSUSED */
static void
fdeject(struct fdctlr *fdc, int unit)
{
	register struct fdunit *un;

	ASSERT(fdc->c_un->un_unit_no == unit);

	un = fdc->c_un;

	FDERRPRINT(FDEP_L1, FDEM_EJEC, (C, "fdeject\n"));
	/*
	 * assume delay of function calling sufficient settling time
	 * eject line is NOT driven by inverter so it is true low
	 */
	switch (fdc->c_fdtype & FDCTYPE_AUXIOMASK) {
	case FDCTYPE_MACHIO:
		set_auxioreg(AUX_EJECT, 0);
		drv_usecwait(2);
		set_auxioreg(AUX_EJECT, 1);
		break;

	case FDCTYPE_SLAVIO:
		if (!(Dor(fdc) & MOTEN(unit))) {
			/* LINTED */
			Set_dor(fdc, MOTEN(unit), 1);
		}
		drv_usecwait(2);	/* just to settle */
		/* LINTED */
		Set_dor(fdc, EJECT, 1);
		drv_usecwait(2);
		/* LINTED */
		Set_dor(fdc, EJECT, 0);
		break;
	case FDCTYPE_CHEERIO:
		if (!(Dor(fdc) & MOTEN(unit))) {
			/* LINTED */
			Set_dor(fdc, MOTEN(unit), 1);
		}
		drv_usecwait(2);	/* just to settle */
		/* LINTED */
		Set_dor(fdc, EJECT_DMA, 1);
		drv_usecwait(2);
		/* LINTED */
		Set_dor(fdc, EJECT_DMA, 0);
		break;
	}
	/*
	 * XXX set ejected state?
	 */
	un->un_ejected = 1;
}

/* ARGSUSED */
static int
fdsense_chng(struct fdctlr *fdc, int unit)
{
	int changed = 0;

	FDERRPRINT(FDEP_L1, FDEM_SCHG, (C, "fdsense_chng:start\n"));

	ASSERT(fdc->c_un->un_unit_no == unit);

	/*
	 * Do not turn on the motor of a pollable drive
	 */
	if (fd_pollable) {
	FDERRPRINT(FDEP_L1, FDEM_SCHG, (C, "pollable: don't turn on motor\n"));
		/*
		 * Invert the sense of the DSKCHG for pollable drives
		 */
		if (Dir(fdc) & DSKCHG)
			changed = 0;
		else
			changed = 1;

		return (changed);
	}

	switch (fdc->c_fdtype & FDCTYPE_AUXIOMASK) {
	case FDCTYPE_MACHIO:
		if (*fdc->c_auxiova & AUX_DISKCHG)
			changed = 1;
		break;

	case FDCTYPE_SLAVIO:
	case FDCTYPE_CHEERIO:
		if (!(Dor(fdc) & MOTEN(unit))) {
			/* LINTED */
			Set_dor(fdc, MOTEN(unit), 1);
		}
		drv_usecwait(2);	/* just to settle */
		if (Dir(fdc) & DSKCHG)
			changed = 1;
		break;
	}

	FDERRPRINT(FDEP_L1, FDEM_SCHG, (C, "fdsense_chng:end\n"));

	return (changed);
}

/*
 *	if it can read a valid label it does so, else it will use a
 *	default.  If it can`t read the diskette - that is an error.
 *
 * RETURNS: 0 for ok - meaning that it could at least read the device,
 *	!0 for error XXX TBD NYD error codes
 *
 *	- called with the low level lock held
 */
static int
fdgetlabel(struct fdctlr *fdc, int unit)
{
	register struct dk_label *label = NULL;
	register struct fdunit *un;
	short *sp;
	short count;
	short xsum;			/* checksum */
	int	i, tries;
	int	err = 0;
	short	oldlvl;

	FDERRPRINT(FDEP_L1, FDEM_GETL,
	    (C, "fdgetlabel: unit %d\n", unit));

	un = fdc->c_un;
	un->un_flags &= ~(FDUNIT_UNLABELED);

	ASSERT(fdc->c_un->un_unit_no == unit);

	/* Do not print errors since this is a private cmd */

	oldlvl = fderrlevel;


	fderrlevel = FDEP_L4;

	label = (struct dk_label *)
				kmem_zalloc(sizeof (struct dk_label), KM_SLEEP);

	/*
	 * try different characteristics (ie densities) by attempting to read
	 * from the diskette.  The diskette may not be present or
	 * is unformatted.
	 *
	 * First, the last sector of the first track is read.  If this
	 * passes, attempt to read the last sector + 1 of the first track.
	 * For example, for a high density diskette, sector 18 is read.  If
	 * the diskette is high density, this will pass.  Next, try to
	 * read sector 19 of the first track.  This should fail.  If it
	 * passes, this is not a high density diskette.  Finally, read
	 * the first sector which should contain a label.
	 *
	 * if un->un_curfdtype is -1 then the current characteristics
	 * were set by FDIOSCHAR and need to try it as well as everything
	 * in the table
	 */
	if (un->un_curfdtype == -1) {
		tries = nfdtypes+1;
		FDERRPRINT(FDEP_L1, FDEM_GETL,
		    (C, "fdgetl: un_curfdtype is -1\n"));

	    } else {
		tries = nfdtypes;

		/* Always start with the highest density (1.7MB) */
		un->un_curfdtype = 0;
		*(un->un_chars) = fdtypes[un->un_curfdtype];
	}

	FDERRPRINT(FDEP_L1, FDEM_GETL,
		    (C, "fdgetl: no. of tries %d\n", tries));
	FDERRPRINT(FDEP_L1, FDEM_GETL,
		    (C, "fdgetl: no. of curfdtype %d\n", un->un_curfdtype));


	for (i = 0; i < tries; i++) {

		FDERRPRINT(FDEP_L1, FDEM_GETL,
		    (C, "fdgetl: trying %d\n", i));

		if (!(err = fdrw(fdc, unit, FDREAD, 0, 0,
			un->un_chars->fdc_secptrack, (caddr_t)label,
			sizeof (struct dk_label))) &&

		    fdrw(fdc, unit, FDREAD, 0, 0,
			un->un_chars->fdc_secptrack + 1,
			(caddr_t)label, sizeof (struct dk_label)) &&

		    !(err = fdrw(fdc, unit, FDREAD, 0, 0, 1, (caddr_t)label,
			sizeof (struct dk_label)))) {

			FDERRPRINT(FDEP_L1, FDEM_GETL,
				(C, "fdgetl: succeeded\n"));

			break;
		}

		/*
		 * try the next entry in the characteristics tbl
		 * If curfdtype is -1, the nxt entry in tbl is 0 (the first).
		 */

		un->un_curfdtype = (un->un_curfdtype + 1) % nfdtypes;
		*(un->un_chars) = fdtypes[un->un_curfdtype];


	}


	/* print errors again */
	fderrlevel = oldlvl;


	/* Couldn't read anything */
	if (err) {

		/* The default characteristics are high density (1.4MB) */
		un->un_curfdtype = 1;
		*(un->un_chars) = fdtypes[un->un_curfdtype];

		fdunpacklabel(&fdlbl_high_80, &un->un_label);

		FDERRPRINT(FDEP_L1, FDEM_GETL,
			(C, "fdgetl: Can't autosense diskette\n"));

		goto out;
	}

	FDERRPRINT(FDEP_L1, FDEM_GETL,
	    (C, "fdgetl: fdtype=%d !!!\n", un->un_curfdtype));
	FDERRPRINT(FDEP_L1, FDEM_GETL,
	    (C, "fdgetl: rate=%d ssize=%d !!!\n",
	    un->un_chars->fdc_transfer_rate, un->un_chars->fdc_sec_size));

	/*
	 * _something_ was read	 -  look for unixtype label
	 */
	if (label->dkl_magic != DKL_MAGIC) {

		/*
		 * The label isn't a unix label.  However, the diskette
		 * is formatted because we were able to read the first
		 * cylinder.
		 */

		FDERRPRINT(FDEP_L1, FDEM_GETL,
		    (C, "fdgetl: not unix label\n"));

		goto nolabel;
	}

	/*
	 * Checksum the label
	 */
	count = sizeof (struct dk_label)/sizeof (short);
	sp = (short *)label;
	xsum = 0;
	while (count--)
		xsum ^= *sp++;	/* should add up to 0 */
	if (xsum) {

		/*
		 * The checksum fails.  However, the diskette is formatted
		 * because we were able to read the first cylinder
		 */

		FDERRPRINT(FDEP_L1, FDEM_GETL,
		    (C, "fdgetl: bad cksum\n"));

		goto nolabel;
	}

	/*
	 * The diskette has a unix label with a correct checksum.
	 * Copy the label into the unit structure
	 */
	un->un_label = *label;

	goto out;

nolabel:
	/*
	 * The diskette doesn't have a correct unix label, but it is formatted.
	 * Use a default label according to the diskette's density
	 * (mark default used)
	 */
	FDERRPRINT(FDEP_L1, FDEM_GETL,
	    (C, "fdgetlabel: unit %d\n", unit));
	un->un_flags |= FDUNIT_UNLABELED;
	switch (un->un_chars->fdc_secptrack) {
	case 9:
		fdunpacklabel(&fdlbl_low_80, &un->un_label);
		break;
	case 8:
		fdunpacklabel(&fdlbl_medium_80, &un->un_label);
		break;
	case 18:
		fdunpacklabel(&fdlbl_high_80, &un->un_label);
		break;
	case 21:
		fdunpacklabel(&fdlbl_high_21, &un->un_label);
		break;
	default:
		fdunpacklabel(&fdlbl_high_80, &un->un_label);
		break;
	}

out:
	if (label != NULL)
		kmem_free((caddr_t)label, sizeof (struct dk_label));
	return (err);
}

/*
 * fdrw- used only for reading labels  and for DKIOCSVTOC ioctl
 *	 which reads the 1 sector.
 */
static int
fdrw(struct fdctlr *fdc, int unit, int rw, int cyl, int head,
    int sector, caddr_t bufp, u_int len)
{
	register struct fdcsb *csb;
	struct	fd_char *ch;
	int	cmdresult = 0;
	caddr_t dma_addr;
	u_int	real_length;
	int	res;
	ddi_device_acc_attr_t attr;
	ddi_acc_handle_t	mem_handle = NULL;

	FDERRPRINT(FDEP_L1, FDEM_RW, (C, "fdrw\n"));

	ASSERT(fdc->c_un->un_unit_no == unit);

	fdgetcsb(fdc);
	csb = &fdc->c_csb;
	ch = fdc->c_un->un_chars;
	if (rw == FDREAD) {
		if (fdc->c_fdtype & FDCTYPE_TCBUG) {
			/*
			 * kludge for lack of Multitrack functionality
			 */
			csb->csb_cmds[0] = SK + FDRAW_RDCMD;
		} else
			csb->csb_cmds[0] = MT + SK + FDRAW_RDCMD;
	} else { /* write */
		if (fdc->c_fdtype & FDCTYPE_TCBUG) {
			/*
			 * kludge for lack of Multitrack functionality
			 */
			csb->csb_cmds[0] = FDRAW_WRCMD;
		} else
			csb->csb_cmds[0] = MT + FDRAW_WRCMD;
	}

	if (rw == FDREAD)
		fdc->c_csb.csb_dma_read = CSB_DMA_READ;
	else
		fdc->c_csb.csb_dma_read = CSB_DMA_WRITE;

	/* always or in MFM bit */
	csb->csb_cmds[0] |= MFM;
	csb->csb_cmds[1] = (u_char) (unit | ((head & 0x1) << 2));
	csb->csb_cmds[2] = (u_char) cyl;
	csb->csb_cmds[3] = (u_char) head;
	csb->csb_cmds[4] = (u_char) sector;
	csb->csb_cmds[5] = ch->fdc_medium ? 3 : 2; /* sector size code */
	/*
	 * kludge for end-of-cylinder error.
	 */
	if (fdc->c_fdtype & FDCTYPE_TCBUG)
		csb->csb_cmds[6] = sector + (len / ch->fdc_sec_size) - 1;
	else
		csb->csb_cmds[6] = (u_char)max(
				fdc->c_un->un_chars->fdc_secptrack,
				sector);
	csb->csb_len = len;
	csb->csb_cmds[7] = GPLN;
	csb->csb_cmds[8] = SSSDTL;
	csb->csb_ncmds = NCBRW;
	csb->csb_len = len;
	csb->csb_maxretry = 2;
	csb->csb_retrys = 0;
	bzero(csb->csb_rslt, NRBRW);
	csb->csb_nrslts = NRBRW;
	csb->csb_opflags = CSB_OFXFEROPS | CSB_OFTIMEIT;

	/* If platform supports DMA, set up DMA resources */
	if (fdc->c_fdtype & FDCTYPE_DMA) {

		attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
		attr.devacc_attr_endian_flags  = DDI_STRUCTURE_BE_ACC;
		attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;

		res = ddi_dma_mem_alloc(fdc->c_dmahandle, len,
			&attr, DDI_DMA_STREAMING,
			DDI_DMA_DONTWAIT, 0, &dma_addr, &real_length,
			&mem_handle);

		if (res != DDI_SUCCESS) {
			FDERRPRINT(FDEP_L1, FDEM_RW,
				(C, "fdrw: dma mem alloc failed\n"));

			fdretcsb(fdc);
			return (EIO);
		}

		FDERRPRINT(FDEP_L1, FDEM_RW, (C, "fdrw: allocated memory"));

		if (fdstart_dma(fdc, dma_addr, len) != 0) {
			fdretcsb(fdc);
			ddi_dma_mem_free(&mem_handle);
			return (-1);

		}

		/*
		 * If the command is a write, copy the data to be written to
		 * dma_addr.
		 */

		if (fdc->c_csb.csb_dma_read == CSB_DMA_WRITE) {
			bcopy((char *)bufp, (char *)dma_addr, len);
		}

		csb->csb_addr = dma_addr;
	} else {
		csb->csb_addr = bufp;
	}


	FDERRPRINT(FDEP_L1, FDEM_RW, (C, "fdrw: call fdexec\n"));

	if (fdexec(fdc, FDXC_SLEEP | FDXC_CHECKCHG) != 0) {
		fdretcsb(fdc);

		if (mem_handle)
			ddi_dma_mem_free(&mem_handle);

		return (EIO);

	}

	FDERRPRINT(FDEP_L1, FDEM_RW, (C, "fdrw: fdexec returned\n"));

	/*
	 * if DMA was used and the command was a read
	 * copy the results into bufp
	 */
	if (fdc->c_fdtype & FDCTYPE_DMA) {
		if (fdc->c_csb.csb_dma_read == CSB_DMA_READ) {
			bcopy((char *)dma_addr, (char *)bufp, len);
		}
		ddi_dma_mem_free(&mem_handle);
	}

	if (csb->csb_cmdstat)
		cmdresult = EIO;	/* XXX TBD NYD for now */

	fdretcsb(fdc);
	return (cmdresult);
}

/*
 * fdunpacklabel
 *	this unpacks a (packed) struct dk_label into a standard dk_label.
 */
static void
fdunpacklabel(struct packed_label *from, struct dk_label *to)
{
	FDERRPRINT(FDEP_L1, FDEM_PACK, (C, "fdpacklabel\n"));
	bzero((caddr_t)to, sizeof (*to));
	bcopy((caddr_t)&from->dkl_vname, (caddr_t)to->dkl_asciilabel,
	    sizeof (to->dkl_asciilabel));
	to->dkl_rpm = from->dkl_rpm;	/* rotations per minute */
	to->dkl_pcyl = from->dkl_pcyl;	/* # physical cylinders */
	to->dkl_apc = from->dkl_apc;	/* alternates per cylinder */
	to->dkl_intrlv = from->dkl_intrlv;	/* interleave factor */
	to->dkl_ncyl = from->dkl_ncyl;	/* # of data cylinders */
	to->dkl_acyl = from->dkl_acyl;	/* # of alternate cylinders */
	to->dkl_nhead = from->dkl_nhead; /* # of heads in this partition */
	to->dkl_nsect = from->dkl_nsect; /* # of 512 byte sectors per track */
	/* logical partitions */
	bcopy((caddr_t)from->dkl_map, (caddr_t)to->dkl_map,
	    sizeof (struct dk_map) * NDKMAP);
	to->dkl_vtoc = from->dkl_vtoc;
}

static struct fdctlr *
fd_getctlr(dev_t dev)
{

	register struct fdctlr *fdc = fdctlrs;
	int ctlr = FDCTLR(dev);

	while (fdc) {
		if (ddi_get_instance(fdc->c_dip) == ctlr)
			return (fdc);
		fdc = fdc->c_next;
	}
	return (fdc);
}

static int
fd_unit_is_open(struct fdunit *un)
{
	register i;
	for (i = 0; i < NDKMAP; i++)
		if (un->un_lyropen[i])
			return (1);
	for (i = 0; i < OTYPCNT - 1; i++)
		if (un->un_regopen[i])
			return (1);
	return (0);
}

/*
 * Return the a vtoc structure in *vtoc.  The vtoc is built from information in
 * the diskette's label.
 */
static void
fd_build_user_vtoc(struct fdunit *un, struct vtoc *vtoc)
{

	int i;
	long nblks;			/* DEV_BSIZE sectors per cylinder */
	struct dk_map2 *lpart;
	struct dk_map	*lmap;
	struct partition *vpart;

	bzero(vtoc, sizeof (struct vtoc));

	/* Initialize info. needed by mboot.  (unsupported) */
	bcopy(un->un_label.dkl_vtoc.v_bootinfo,
	    vtoc->v_bootinfo, sizeof (vtoc->v_bootinfo));

	/* Fill in vtoc sanity and version information */
	vtoc->v_sanity		= un->un_label.dkl_vtoc.v_sanity;
	vtoc->v_version		= un->un_label.dkl_vtoc.v_version;

	/* Copy the volume name */
	bcopy(un->un_label.dkl_vtoc.v_volume,
	    vtoc->v_volume, LEN_DKL_VVOL);

	/*
	 * The dk_map structure is based on DEV_BSIZE byte blocks.
	 * However, medium density diskettes have 1024 byte blocks.
	 * The number of sectors per partition listed in the dk_map structure
	 * accounts for this by multiplying the number of 1024 byte
	 * blocks by 2.  (See the packed_label initializations.)  The
	 * 1024 byte block size can not be listed for medium density
	 * diskettes because the kernel is hard coded for DEV_BSIZE
	 * blocks.
	 */
	vtoc->v_sectorsz = DEV_BSIZE;
	vtoc->v_nparts = un->un_label.dkl_vtoc.v_nparts;

	/* Copy the reserved space */
	bcopy(un->un_label.dkl_vtoc.v_reserved,
	    vtoc->v_reserved, sizeof (vtoc->v_reserved));
	/*
	 * Convert partitioning information.
	 *
	 * Note the conversion from starting cylinder number
	 * to starting sector number.
	 */
	lmap = un->un_label.dkl_map;
	lpart = un->un_label.dkl_vtoc.v_part;
	vpart = vtoc->v_part;

	nblks = (un->un_chars->fdc_nhead * un->un_chars->fdc_secptrack *
		un->un_chars->fdc_sec_size) / DEV_BSIZE;

	for (i = 0; i < V_NUMPAR; i++) {
		vpart->p_tag	= lpart->p_tag;
		vpart->p_flag	= lpart->p_flag;
		vpart->p_start	= lmap->dkl_cylno * nblks;
		vpart->p_size	= lmap->dkl_nblk;

		lmap++;
		lpart++;
		vpart++;
	}

	/* Initialize timestamp and label */
	bcopy(un->un_label.dkl_vtoc.v_timestamp,
	    vtoc->timestamp, sizeof (vtoc->timestamp));

	bcopy(un->un_label.dkl_asciilabel,
	    vtoc->v_asciilabel, LEN_DKL_ASCII);

}

/*
 * Build a label out of a vtoc structure.
 */
static int
fd_build_label_vtoc(struct fdunit *un, struct vtoc *vtoc)
{

	struct dk_map		*lmap;
	struct dk_map2		*lpart;
	struct partition	*vpart;
	long			nblks;	/* no. blocks per cylinder */
	long			ncyl;
	int			i;
	short sum, *sp;

	/* Sanity-check the vtoc */
	if ((vtoc->v_sanity != VTOC_SANE) ||
			(vtoc->v_nparts > NDKMAP) || (vtoc->v_nparts <= 0)) {
		FDERRPRINT(FDEP_L1, FDEM_IOCT,
		    (C, "fd_build_label:  sanity check on vtoc failed\n"));
		return (EINVAL);
	}

	nblks = (un->un_chars->fdc_nhead * un->un_chars->fdc_secptrack *
		un->un_chars->fdc_sec_size) / DEV_BSIZE;

	vpart = vtoc->v_part;

	/*
	 * Check the partition information in the vtoc.  The starting sectors
	 * must lie along partition boundaries. (NDKMAP entries are checked
	 * to ensure that the unused entries are set to 0 if vtoc->v_nparts
	 * is less than NDKMAP)
	 */

	    for (i = 0; i < NDKMAP; i++) {
		if ((vpart->p_start % nblks) != 0) {
			return (EINVAL);
		}
		ncyl = vpart->p_start % nblks;
		ncyl += vpart->p_size % nblks;
		if ((vpart->p_size % nblks) != 0)
			ncyl++;
		if (ncyl > (long)un->un_chars->fdc_ncyl) {
			return (EINVAL);
		}
		vpart++;
	}

	/*
	 * reinitialize the existing label
	 */
	bzero(&un->un_label, sizeof (un->un_label));


	/* Put appropriate vtoc structure fields into the disk label */
	bcopy(vtoc->v_bootinfo, un->un_label.dkl_vtoc.v_bootinfo,
	    sizeof (vtoc->v_bootinfo));

	un->un_label.dkl_vtoc.v_sanity = vtoc->v_sanity;
	un->un_label.dkl_vtoc.v_version = vtoc->v_version;

	bcopy(vtoc->v_volume, un->un_label.dkl_vtoc.v_volume, LEN_DKL_VVOL);

	un->un_label.dkl_vtoc.v_nparts = vtoc->v_nparts;

	bcopy(vtoc->v_reserved, un->un_label.dkl_vtoc.v_reserved,
	    sizeof (vtoc->v_reserved));

	/*
	 * Initialize cylinder information in the label.
	 * Note the conversion from starting sector number
	 * to starting cylinder number.
	 * Return error if division results in a remainder.
	 */
	lmap = un->un_label.dkl_map;
	lpart = un->un_label.dkl_vtoc.v_part;
	vpart = vtoc->v_part;


	for (i = 0; i < (int)vtoc->v_nparts; i++) {
		lpart->p_tag  = vtoc->v_part[i].p_tag;
		lpart->p_flag = vtoc->v_part[i].p_flag;
		lmap->dkl_cylno = vpart->p_start / nblks;
		lmap->dkl_nblk = vpart->p_size;

		lmap++;
		lpart++;
		vpart++;
	}

	/* Copy the timestamp and ascii label */
	for (i = 0; i < NDKMAP; i++) {
		un->un_label.dkl_vtoc.v_timestamp[i] = vtoc->timestamp[i];
	}


	bcopy(vtoc->v_asciilabel, un->un_label.dkl_asciilabel, LEN_DKL_ASCII);

	FDERRPRINT(FDEP_L1, FDEM_IOCT,
		    (C, "fd_build_label: asciilabel %s\n",
			un->un_label.dkl_asciilabel));

	/* Initialize the magic number */
	un->un_label.dkl_magic = DKL_MAGIC;

	un->un_label.dkl_pcyl = un->un_chars->fdc_ncyl;

	/*
	 * The fdc_secptrack filed of the fd_char structure is the number
	 * of sectors per track where the sectors are fdc_sec_size.  The
	 * dkl_nsect field of the dk_label structure is the number of
	 * 512 (DEVBSIZE) byte sectors per track.
	 */
	un->un_label.dkl_nsect = (un->un_chars->fdc_secptrack *
				un->un_chars->fdc_sec_size) / DEV_BSIZE;


	un->un_label.dkl_ncyl = un->un_label.dkl_pcyl;
	un->un_label.dkl_nhead =  un->un_chars->fdc_nhead;
	un->un_label.dkl_rpm = un->un_chars->fdc_medium ? 360 : 300;
	un->un_label.dkl_intrlv = 1;

	/* Create the checksum */
	sum = 0;
	un->un_label.dkl_cksum = 0;
	sp = (short *)&un->un_label;
	i = sizeof (struct dk_label)/sizeof (short);
	while (i--) {
		sum ^= *sp++;
	}
	un->un_label.dkl_cksum = sum;

	return (0);

}

/*
 * Check for auxio register node
 */

int
fd_isauxiodip(dip, dipp)
	dev_info_t *dip;
	void *dipp;
{
	if (strcmp(ddi_get_name(dip), "auxio") == 0 ||
		strcmp(ddi_get_name(dip), "auxiliary-io") == 0) {
		*(dev_info_t **)dipp = dip;
		return (DDI_WALK_TERMINATE);
	}
	return (DDI_WALK_CONTINUE);
}

/*
 * Search for auxio register node, then for address property
 */

caddr_t
fd_getauxiova()
{
	dev_info_t *dip = NULL;
	caddr_t addr;

	/*
	 * Acquire a reader's lock on the devinfo tree.
	 */
	rw_enter(&(devinfo_tree_lock), RW_READER);
	ddi_walk_devs(ddi_root_node(), fd_isauxiodip, (void *)&dip);
	rw_exit(&(devinfo_tree_lock));

	if (dip == NULL)
		return (NULL);

	addr = (caddr_t)ddi_getprop(DDI_DEV_T_ANY,
		dip, DDI_PROP_DONTPASS, "address", 0);

	/*
	 * The device tree on some sun4c machines (SS1+) incorrectly
	 * reports the "auxiliary-io" as being word wide at an
	 * aligned address rather than byte wide at an offset of 3.
	 * Here we correct for this ..
	 */
	if (strcmp(ddi_get_name(dip), "auxiliary-io") == 0 &&
	    (((int)addr & 3) == 0))
		addr += 3;

	return (addr);
}


/*
 * set_rotational speed
 * 300 rpm for high and low density.
 * 360 rpm for medium density.
 * for now, we assume that 3rd density is supported only for Sun4M,
 * not for Clones. (else we would have to check for 82077, and do
 * specific things for the MEDIUM_DENSITY BIT for clones.
 * this code should not break CLONES.
 *
 * REMARK: there is a SOny requirement, to deselect the drive then
 * select it again after the medium density change, since the
 * leading edge of the select line latches the rotational Speed.
 * then after that, we have to wait 500 ms for the rotation to
 * stabilize.
 *
 */
static void
set_rotational_speed(struct fdctlr *fdc, int unit)
{
	int check;
	int is_medium;

	ASSERT(fdc->c_un->un_unit_no == unit);

	/*
	 * if we do not have a Sun4m, medium density is not supported.
	 */
	if (fdc->c_fdtype & FDCTYPE_MACHIO)
		return;

	/*
	 * if there is a change, do it, if not leave it alone.
	 *
	 * there is a change if un->un_chars->fdc_medium does not match
	 * un->un_flags & FDUNIT_MEDIUM
	 * un->un_flags & FDUNIT_MEDIUM specifies the last setting.
	 * un->un_chars->fdc_medium specifies next setting.
	 * if there is a change, wait 500ms according to Sony spec.
	 */

	is_medium = fdc->c_un->un_chars->fdc_medium;
	check = is_medium ^
		    ((fdc->c_un->un_flags & FDUNIT_MEDIUM) ? 1 : 0);

	if (check) {

		fdselect(fdc, unit, 0);
		drv_usecwait(5);

		if ((fdc->c_fdtype & FDCTYPE_AUXIOMASK) == FDCTYPE_SLAVIO) {
			Set_dor(fdc, MEDIUM_DENSITY, is_medium);
		}

		if ((fdc->c_fdtype & FDCTYPE_AUXIOMASK) == FDCTYPE_CHEERIO) {

			if (is_medium) {
				Set_auxio(fdc, AUX_MEDIUM_DENSITY);
			} else {
				Set_auxio(fdc, AUX_HIGH_DENSITY);
			}

		}

		if (is_medium) {
			drv_usecwait(5);
		}

		fdselect(fdc, unit, 1);	/* Sony requirement */
		FDERRPRINT(FDEP_L1, FDEM_EXEC, (C, "rotation:medium\n"));
		drv_usecwait(500000);

		fdc->c_un->un_flags ^= FDUNIT_MEDIUM;

	}
}

static void
fd_media_watch(caddr_t arg)
{
	dev_t		dev;
	register struct fdunit *un;
	register struct fdctlr *fdc;
	int		unit;

	dev = (dev_t)arg;
	fdc = fd_getctlr(dev);
	unit = fdc->c_un->un_unit_no;
	un = fdc->c_un;

	mutex_enter(&fdc->c_lolock);

	if (un->un_media_timeout_id == 0) {
		/*
		 * Untimeout is about to be called.
		 * Don't call fd_get_media_state again
		 */
		mutex_exit(&fdc->c_lolock);
		return;
	}


	un->un_media_state = fd_get_media_state(fdc, unit);
	cv_broadcast(&fdc->c_statecv);

	mutex_exit(&fdc->c_lolock);

	if (un->un_media_timeout) {
		un->un_media_timeout_id = timeout(fd_media_watch,
			(caddr_t)dev, un->un_media_timeout);
	}
}

enum dkio_state
fd_get_media_state(struct fdctlr *fdc, int unit)
{
	enum dkio_state state;

	ASSERT(fdc->c_un->un_unit_no == unit);

	if (fdsense_chng(fdc, unit)) {
		/* check disk only if DSKCHG "high" */
		if (fdcheckdisk(fdc, unit)) {
			state = DKIO_EJECTED;
		} else {
			state = DKIO_INSERTED;
		}
	} else {
		state = DKIO_INSERTED;
	}
	return (state);
}

static int
fd_check_media(dev_t dev, enum dkio_state state)
{
	register struct fdunit *un;
	register struct fdctlr *fdc;
	int		unit;
	int		timeid;

	FDERRPRINT(FDEP_L1, FDEM_RW, (C, "fd_check_media: start\n"));

	fdc = fd_getctlr(dev);
	unit = fdc->c_un->un_unit_no;
	un = fdc->c_un;

	mutex_enter(&fdc->c_lolock);

	un->un_media_state = fd_get_media_state(fdc, unit);

	/* turn on timeout */
	un->un_media_timeout = drv_usectohz(fd_check_media_time);
	un->un_media_timeout_id = timeout(fd_media_watch,
			(caddr_t)dev, un->un_media_timeout);

	while (un->un_media_state == state) {
		if (cv_wait_sig(&fdc->c_statecv, &fdc->c_lolock) == 0) {
			un->un_media_timeout = 0;
			mutex_exit(&fdc->c_lolock);
			return (EINTR);
		}
	}

	if (un->un_media_timeout_id) {
		timeid = un->un_media_timeout_id;
		un->un_media_timeout_id = 0;

		mutex_exit(&fdc->c_lolock);
		(void) untimeout(timeid);
		mutex_enter(&fdc->c_lolock);
	}

	if (un->un_media_state == DKIO_INSERTED) {
		if (fdgetlabel(fdc, unit)) {
			mutex_exit(&fdc->c_lolock);
			return (EIO);
		}
	}
	mutex_exit(&fdc->c_lolock);

	FDERRPRINT(FDEP_L1, FDEM_RW, (C, "fd_check_media: end\n"));
	return (0);
}
