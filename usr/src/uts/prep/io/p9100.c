/*
 * Copyright (c) 1995-1996, Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)p9100.c	1.37	96/09/24 SMI"

/*
 * P9000/P9100 theory of operation:
 *
 * Most P9000/P9100 operations are done by mapping the P9000 components into
 * user process memory.  User processes that share mappings (typically
 * pixrect programs) must cooperate among themselves to prevent damaging
 * the state of the P9000/P9100.  User processes may also acquire private
 * mappings (MAP_PRIVATE flag to mmap(2)), in which case the P9000
 * driver will preserve device state for each mapping.
 *
 * The Weitek P9000/P9100 maps in 4 megabytes into the address space.
 * The first 2 megabytes are the P9000/P9100 registers.  The second
 * 2 megabytes are the frame buffer.
 *
 * Mappings to the registers may be MAP_PRIVATE, in which case
 * the driver keeps a per-context copy of the fbc and tec registers in
 * local memory.  Only one context at a time may have valid mappings.
 * If a process tries to access the registers through an invalid
 * mapping, the driver in invoked to swap register state and validate
 * and validate the mappings.
 *
 *
 * The P9000 driver also provides a "lockpage" mechanism.  A requesting
 * process uses the GRABPAGEALLOC ioctl to create a lock page.  The ioctl
 * returns a device offset unique to that lock page.  Any process may then
 * map that offset twice in order to get lock and unlock mappings.  When a
 * process writes a '1' to its lock mapping, it acquires a lock and no
 * other process may access the lock page -- if they try they will get a
 * page fault and go to sleep.  When the locking process writes a '0' to
 * the unlock mapping, the driver wakes up any sleeping process(es).
 *
 *
 * Finally, processes have the option of mapping the "vertical retrace
 * page".  This is a page in shared memory containing a 32-bit integer
 * that is incremented each time a vertical retrace interrupt occurs.  It
 * is used so that programs may synchronize themselves with vertical
 * retrace.
 */

#define	ALWAYS_NO_INTERRUPTS 0

/*
 * Weitek P9100 8 bit color frame buffer driver
 */

#include <sys/debug.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/errno.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/map.h>
#include <sys/cred.h>
#include <sys/open.h>
#include <sys/stat.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/kmem.h>
#include <sys/conf.h>
#include <sys/devops.h>
#include <sys/sysmacros.h>
#include <sys/modctl.h>

#include <sys/visual_io.h>
#include <sys/fbio.h>
#include <sys/ddi.h>
#include <sys/esunddi.h>
#include <sys/sunddi.h>
#include "sys/p9100reg.h"
#include "sys/bt485reg.h"

#define	BT485_BASE	0x0000
#define	BT485_A0	0x0001
#define	BT485_A1	0x0002
#define	BT485_A2	0x0004
#define	BT485_A3	0x0008

#include    "sys/rgb525reg.h"

#define	RGB525_BASE	0
#define	RGB525_A0	1
#define	RGB525_A1	2
#define	RGB525_A2	4

#include "sys/viperreg.h"
#include "sys/viperio.h"

/* configuration options */

#define	CONSOLE_REDESIGN    1

#define	P9100_REG_RNUMBER	1

#ifndef lint
static char _depends_on[] = "misc/seg_mapdev";
#endif /* lint */

#define	P9100DEBUG	0
#if P9100DEBUG >= 1
#define	ASSERT2(e) \
{\
    if (!(e)) \
	cmn_err(CE_NOTE, \
		"p9100: assertion failed \"%s\", line %d\n", #e, __LINE__); }
#else
#define	ASSERT2(e)		/* nothing */
#endif

#if P9100DEBUG >= 2
int	p9100_debug = 2;

#define	DEBUGF(level, args) \
    { if (p9100_debug >= (level)) cmn_err args; }
#define	DUMP_SEGS(level, s, c) \
    { if (p9100_debug == (level)) dump_segs(s, c); }
#else
#define	DEBUGF(level, args)	/* nothing */
#define	DUMP_SEGS(level, s, c)	/* nothing */
#endif

/* Data access requirements. */
#ifdef  _LITTLE_ENDIAN
struct ddi_device_acc_attr endian_attr = {
	DDI_DEVICE_ATTR_V0,
	DDI_STRUCTURE_LE_ACC,
	DDI_STRICTORDER_ACC
};
#else /* _LITTLE_ENDIAN */
struct ddi_device_acc_attr endian_attr = {
	DDI_DEVICE_ATTR_V0,
	DDI_STRUCTURE_BE_ACC,
	DDI_STRICTORDER_ACC
};
#endif /* _LITTLE_ENDIAN */

struct ddi_device_acc_attr nosw_attr = {
	DDI_DEVICE_ATTR_V0,
	DDI_NEVERSWAP_ACC,
	DDI_STRICTORDER_ACC
};

#define	getprop(devi, name, def)	\
		ddi_getprop(DDI_DEV_T_ANY, (devi), \
		DDI_PROP_DONTPASS, (name), (def))

/* config info */

static int	p9100_open(dev_t *, int, int, cred_t *);
static int	p9100_close(dev_t, int, int, cred_t *);
static int	p9100_read(dev_t, struct uio *, cred_t *);
static int	p9100_write(dev_t, struct uio *, cred_t *);
static int	p9100_ioctl(dev_t, int, intptr_t, int, cred_t *, int *);
static int	p9100_mmap(dev_t, off_t, int);
static int	p9100_segmap(dev_t, off_t, struct as *, caddr_t *, off_t,
				u_int, u_int, u_int, cred_t *);

static struct cb_ops p9100_cb_ops = {
    p9100_open,		/* open */
    p9100_close,	/* close */
    nodev,		/* strategy */
    nodev,		/* print */
    nodev,		/* dump */
    p9100_read,		/* read */
    p9100_write,	/* write */
    p9100_ioctl,	/* ioctl */
    nodev,		/* devmap */
    p9100_mmap,		/* mmap */
    p9100_segmap,	/* segmap */
    nochpoll,		/* poll */
    ddi_prop_op,	/* cb_prop_op */
    0,			/* streamtab  */
    D_NEW | D_MP	/* Driver compatibility flag */
};

static int	p9100_info(dev_info_t *, ddi_info_cmd_t, void *, void **);
static int	p9100_identify(dev_info_t *);
static int	p9100_attach(dev_info_t *, ddi_attach_cmd_t);
static int	p9100_detach(dev_info_t *, ddi_detach_cmd_t);
static int	p9100_probe(dev_info_t *);
#ifdef POWER
static int	p9100_power(dev_info_t *, int, int);
#endif

static struct dev_ops p9100_ops = {
    DEVO_REV,			/* devo_rev, */
    0,				/* refcnt  */
    p9100_info,			/* info */
    p9100_identify,		/* identify */
    p9100_probe,			/* probe */
    p9100_attach,		/* attach */
    p9100_detach,		/* detach */
    nodev,			/* reset */
    &p9100_cb_ops,		/* driver operations */
    (struct bus_ops *)0,	/* bus operations */
#ifdef POWER
    p9100_power,			/* power operations */
#else
    nulldev,
#endif
};

static struct p9100_map_pvt {
    struct p9100_softc	*softc;
    ddi_mapdev_handle_t handle;		/* handle of mapdev object	*/
    u_int		offset;		/* starting offset of this map	*/
    u_int		len;		/* length of this map		*/
    struct p9100_cntxt	*context;	/* associated context		*/
    struct p9100_map_pvt	*next;	/* List of associated pvt's for */
					/* this context			*/
};

#define	P9100_CMAP_ENTRIES	256
#define	P9100_CURSOR_ENTRIES	2

/*
 * Per-context info:
 *	many registers in the tec and fbc do
 *	not need to be saved/restored.
 */
struct p9100_cntxt {
    struct p9100_cntxt	*link;
    struct p9100_map_pvt	*p9100_ctx_pvt;	/* List of associated pvt's */
						/* for this context */
    int		pid;				/* owner of mapping */
    int		p9100_ctx_flag;
    u_long	p9100_ctx_interrupt;		/* 0x000z0008 rw */
    u_long	p9100_ctx_interrupt_en;		/* 0x000z000c rw */
    ulong_t	p9100_ctx_cindex;		/* 0x000s018c rw */
    long	p9100_ctx_w_off_xy;		/* 0x000s0190 rw */
    ulong_t	p9100_ctx_fground;		/* 0x000s0200 rw */
    ulong_t	p9100_ctx_bground;		/* 0x000s0204 rw */
    ulong_t	p9100_ctx_color2;		/* 0x000s0238 rw */
    ulong_t	p9100_ctx_color3;		/* 0x000s023c rw */
    ulong_t	p9100_ctx_pmask;		/* 0x000s0208 rw */
    ulong_t	p9100_ctx_draw_mode;		/* 0x000s020c rw */
    long	p9100_ctx_pat_originx;		/* 0x000s0210 rw */
    long	p9100_ctx_pat_originy;		/* 0x000s0214 rw */
    ulong_t	p9100_ctx_raster;		/* 0x000s0218 rw */
    ulong_t	p9100_ctx_pixel8_reg;		/* 0x000s021c rw */
    long	p9100_ctx_w_min;		/* 0x000s0220 rw */
    long	p9100_ctx_w_max;		/* 0x000s0224 rw */
    long	p9100_ctx_b_w_min;		/* 0x000s02a4 rw */
    long	p9100_ctx_b_w_max;		/* 0x000s02a0 rw */
    ulong_t	p9100_ctx_pattern[4];		/* 0x000s0280 rw */
    long	p9100_ctx_x0;			/* 0x000s1008 rw */
    long	p9100_ctx_y0;			/* 0x000s1010 rw */
    long	p9100_ctx_x1;			/* 0x000s1048 rw */
    long	p9100_ctx_y1;			/* 0x000s1050 rw */
    long	p9100_ctx_x2;			/* 0x000s1088 rw */
    long	p9100_ctx_y2;			/* 0x000s1090 rw */
    long	p9100_ctx_x3;			/* 0x000s10c8 rw */
    long	p9100_ctx_y3;			/* 0x000s10d0 rw */
};

/* per-unit data */
struct p9100_softc {
    p9100p_t	p9100;		/* p9100 hardware mapping structure */
    int		w;
    int		h;
    int		depth;
    int		tem_depth;
    int		size;		/* total size of frame buffer */
    kmutex_t	interlock;	/* interrupt locking */
    void	(*setcmap)(struct p9100_softc *);
    void	(*setcurpos)(struct p9100_softc *);
    void	(*setcurshape)(struct p9100_softc *);
    void	(*setcurcolor)(struct p9100_softc *);
    ddi_acc_handle_t	p9100_config_handle;		/* handle to regs */
    ddi_acc_handle_t	p9100_regs_handle;		/* handle to regs */
    u_char *	ndvram;		/* for non display VRAM while suspended */
    int		ndvramsz;	/* size of non display VRAM */

    struct softcur {
	short	enable;		/* cursor enable */
	short	pad1;
	struct fbcurpos	pos;	/* cursor position */
	struct fbcurpos	hot;	/* cursor hot spot */
	struct fbcurpos	size;	/* cursor bitmap size */
	u_long	image[32];	/* cursor image bitmap */
	u_long	mask[32];	/* cursor mask bitmap */
    } cur;

    u_char	omap[P9100_CURSOR_ENTRIES][3];
    u_char	cmap[P9100_CMAP_ENTRIES][3]; /* shadow color map */
    u_short	omap_update;		/* overlay colormap update flag */

    u_short	cmap_index;		/* colormap update index */
    u_short	cmap_count;		/* colormap update count */

#define	P9100_VRTIOCTL	1		/* FBIOVERTICAL in effect */
#define	P9100_VRTCTR	2		/* OWGX vertical retrace counter */

    int		*vrtpage;		/* pointer to VRT page */
    int		*vrtalloc;		/* pointer to VRT allocation */
    int		vrtmaps;		/* number of VRT page maps */
    int		vrtflag;		/* vrt interrupt flag */
    struct p9100_cntxt	*curctx;	/* context switching */
    struct p9100_cntxt	shared_ctx;	/* shared context */
    struct p9100_cntxt	*pvt_ctx;	/* list of non-shared contexts */
    int		emulation;		/* emulation type */
    dev_info_t	*devi;			/* back pointer */
    int		have_intr;		/* do interrupts work */
    ddi_iblock_cookie_t	iblock_cookie;	/* block interrupts */
    kmutex_t	mutex;			/* mutex locking */
    kcondvar_t	vrtsleep;		/* for sleeping */
    int		suspended;		/* true if driver is suspended */
    viper_init_t	viperinit;
    short	bus_type;
    short	dac_type;
    struct	vis_identifier p9100_ident;
#if CONSOLE_REDESIGN
    struct	p9100_console_private *conspriv;	/* console support */
    void	(*copy_func)(struct p9100_softc *, struct vis_conscopy *);
    void	(*display_func)(struct p9100_softc *, struct vis_consdisplay *);
    void	(*cursor_func)(struct p9100_softc *, struct vis_conscursor *);
#endif
};

#if CONSOLE_REDESIGN

/* console management */

struct  p9100_console_private {
	struct p9100_softc
			*softc;	/* softc for this instance */
	struct p9100_cntxt
			context;	/* pointer to context */
	int		viscursor;
				/* cursor visible on this instance? */
};

struct p9100_rect {
	short w;
	short h;
	short x;
	short y;
};

static int	p9100_devinit(dev_t dev, struct vis_devinit *);
static void	p9100_cons_free(struct p9100_console_private *);

#endif

static int	p9100_map_access(ddi_mapdev_handle_t, void *, off_t);
static void	p9100_map_free(ddi_mapdev_handle_t, void *);
static int	p9100_map_dup(ddi_mapdev_handle_t, void *,
				ddi_mapdev_handle_t, void **);

static struct ddi_mapdev_ctl p9100_map_ops = {
    MAPDEV_REV,		/* mapdev_ops version number	*/
    p9100_map_access,	/* mapdev access routine	*/
    p9100_map_free,	/* mapdev free routine		*/
    p9100_map_dup	/* mapdev dup routine		*/
};

static void	*p9100_softc_head;
static u_int	p9100_pagesize;
#if ALWAYS_NO_INTERRUPTS
static u_int	p9100_pageoffset;
#endif

/* default structure for FBIOGATTR ioctl */
static struct fbgattr	p9100_attr;

/*
 * handy macros
 */

#define	getsoftc(unit) \
    ((struct p9100_softc *)ddi_get_soft_state(p9100_softc_head, (unit)))

#define	btob(n)			ctob(btoc(n))

#define	P9100_WAIT	500000  /* .5 seconds */

#define	p9100_int_enable(softc)	P9100_SET_VBLANK_INTR((softc)->p9100)

#define	p9100_int_disable_intr(softc)	P9100_STOP_VBLANK_INTR((softc)->p9100)

#define	p9100_int_clear(softc)	P9100_CLEAR_VBLANK_INTR((softc)->p9100)

/* check if color map update is pending */
#define	p9100_update_pending(softc) \
    ((softc)->cmap_count || (softc)->omap_update)

/*
 * forward references
 */
static u_int	p9100_intr(caddr_t);
static void	p9100_reset_cmap(u_char *, register u_int);
static void	p9100_update_cmap(struct p9100_softc *, u_int, u_int);
static void	p9100_update_omap(struct p9100_softc *);
static void	p9100_cmap_copyout(u_char *, u_char *, u_int);
static void	p9100_cmap_copyin(u_char *, u_char *, u_int);
static void	p9100_set_video(struct p9100_softc *, int);
static void	p9100_reset(struct p9100_softc *);
static int	p9100_cntxsave(p9100p_t, struct p9100_cntxt *);
static int	p9100_cntxrestore(p9100p_t, struct p9100_cntxt *);
static struct	p9100_cntxt *p9100_ctx_map_insert(struct p9100_softc *,
		    int);
static int	p9100_getpid(void);
static void	p9100_get_hardware_settings(struct p9100_softc *softc);

static int	viper_vlb_probe(dev_info_t *);
static void	viper_vlb(dev_info_t *);
static int	viper_ioctl(dev_t, int, int, int, cred_t *, int *);
static void	bt485_setcurpos(struct p9100_softc *);
static void	bt485_setcurshape(struct p9100_softc *);
static void	bt485_setcurcolor(struct p9100_softc *);
static void	bt485_putcmap(struct p9100_softc *);
static void	rgb525_setcurpos(struct p9100_softc *);
static void	rgb525_setcurshape(struct p9100_softc *);
static void	rgb525_setcurcolor(struct p9100_softc *);
static void	rgb525_putcmap(struct p9100_softc *);
static void	null_routine(struct p9100_softc *);
static void	p9100_putpci(struct p9100_softc *const, unsigned long,
		    unsigned long);
static void	viper_write_ic2061a(struct p9100_softc *, u_long);

/* Loadable Driver stuff */

extern struct mod_ops	mod_driverops;

static struct modldrv	p9100_modldrv = {
    &mod_driverops,		/* Type of module.  This one is a driver */
    "weitek p9000/p9100 driver v94/07/18",	/* Name of the module. */
    &p9100_ops,			/* driver ops */
};

static struct modlinkage	p9100_modlinkage = {
    MODREV_1,
    (void *)&p9100_modldrv,
    NULL,
};

unsigned short G91masktable[256] = {
/* 0x0 */	 0x0000,
/* 0x1 */	 0x0002,
/* 0x2 */	 0x0008,
/* 0x3 */	 0x000a,
/* 0x4 */	 0x0020,
/* 0x5 */	 0x0022,
/* 0x6 */	 0x0028,
/* 0x7 */	 0x002a,
/* 0x8 */	 0x0080,
/* 0x9 */	 0x0082,
/* 0xa */	 0x0088,
/* 0xb */	 0x008a,
/* 0xc */	 0x00a0,
/* 0xd */	 0x00a2,
/* 0xe */	 0x00a8,
/* 0xf */	 0x00aa,
/* 0x10 */	 0x0200,
/* 0x11 */	 0x0202,
/* 0x12 */	 0x0208,
/* 0x13 */	 0x020a,
/* 0x14 */	 0x0220,
/* 0x15 */	 0x0222,
/* 0x16 */	 0x0228,
/* 0x17 */	 0x022a,
/* 0x18 */	 0x0280,
/* 0x19 */	 0x0282,
/* 0x1a */	 0x0288,
/* 0x1b */	 0x028a,
/* 0x1c */	 0x02a0,
/* 0x1d */	 0x02a2,
/* 0x1e */	 0x02a8,
/* 0x1f */	 0x02aa,
/* 0x20 */	 0x0800,
/* 0x21 */	 0x0802,
/* 0x22 */	 0x0808,
/* 0x23 */	 0x080a,
/* 0x24 */	 0x0820,
/* 0x25 */	 0x0822,
/* 0x26 */	 0x0828,
/* 0x27 */	 0x082a,
/* 0x28 */	 0x0880,
/* 0x29 */	 0x0882,
/* 0x2a */	 0x0888,
/* 0x2b */	 0x088a,
/* 0x2c */	 0x08a0,
/* 0x2d */	 0x08a2,
/* 0x2e */	 0x08a8,
/* 0x2f */	 0x08aa,
/* 0x30 */	 0x0a00,
/* 0x31 */	 0x0a02,
/* 0x32 */	 0x0a08,
/* 0x33 */	 0x0a0a,
/* 0x34 */	 0x0a20,
/* 0x35 */	 0x0a22,
/* 0x36 */	 0x0a28,
/* 0x37 */	 0x0a2a,
/* 0x38 */	 0x0a80,
/* 0x39 */	 0x0a82,
/* 0x3a */	 0x0a88,
/* 0x3b */	 0x0a8a,
/* 0x3c */	 0x0aa0,
/* 0x3d */	 0x0aa2,
/* 0x3e */	 0x0aa8,
/* 0x3f */	 0x0aaa,
/* 0x40 */	 0x2000,
/* 0x41 */	 0x2002,
/* 0x42 */	 0x2008,
/* 0x43 */	 0x200a,
/* 0x44 */	 0x2020,
/* 0x45 */	 0x2022,
/* 0x46 */	 0x2028,
/* 0x47 */	 0x202a,
/* 0x48 */	 0x2080,
/* 0x49 */	 0x2082,
/* 0x4a */	 0x2088,
/* 0x4b */	 0x208a,
/* 0x4c */	 0x20a0,
/* 0x4d */	 0x20a2,
/* 0x4e */	 0x20a8,
/* 0x4f */	 0x20aa,
/* 0x50 */	 0x2200,
/* 0x51 */	 0x2202,
/* 0x52 */	 0x2208,
/* 0x53 */	 0x220a,
/* 0x54 */	 0x2220,
/* 0x55 */	 0x2222,
/* 0x56 */	 0x2228,
/* 0x57 */	 0x222a,
/* 0x58 */	 0x2280,
/* 0x59 */	 0x2282,
/* 0x5a */	 0x2288,
/* 0x5b */	 0x228a,
/* 0x5c */	 0x22a0,
/* 0x5d */	 0x22a2,
/* 0x5e */	 0x22a8,
/* 0x5f */	 0x22aa,
/* 0x60 */	 0x2800,
/* 0x61 */	 0x2802,
/* 0x62 */	 0x2808,
/* 0x63 */	 0x280a,
/* 0x64 */	 0x2820,
/* 0x65 */	 0x2822,
/* 0x66 */	 0x2828,
/* 0x67 */	 0x282a,
/* 0x68 */	 0x2880,
/* 0x69 */	 0x2882,
/* 0x6a */	 0x2888,
/* 0x6b */	 0x288a,
/* 0x6c */	 0x28a0,
/* 0x6d */	 0x28a2,
/* 0x6e */	 0x28a8,
/* 0x6f */	 0x28aa,
/* 0x70 */	 0x2a00,
/* 0x71 */	 0x2a02,
/* 0x72 */	 0x2a08,
/* 0x73 */	 0x2a0a,
/* 0x74 */	 0x2a20,
/* 0x75 */	 0x2a22,
/* 0x76 */	 0x2a28,
/* 0x77 */	 0x2a2a,
/* 0x78 */	 0x2a80,
/* 0x79 */	 0x2a82,
/* 0x7a */	 0x2a88,
/* 0x7b */	 0x2a8a,
/* 0x7c */	 0x2aa0,
/* 0x7d */	 0x2aa2,
/* 0x7e */	 0x2aa8,
/* 0x7f */	 0x2aaa,
/* 0x80 */	 0x8000,
/* 0x81 */	 0x8002,
/* 0x82 */	 0x8008,
/* 0x83 */	 0x800a,
/* 0x84 */	 0x8020,
/* 0x85 */	 0x8022,
/* 0x86 */	 0x8028,
/* 0x87 */	 0x802a,
/* 0x88 */	 0x8080,
/* 0x89 */	 0x8082,
/* 0x8a */	 0x8088,
/* 0x8b */	 0x808a,
/* 0x8c */	 0x80a0,
/* 0x8d */	 0x80a2,
/* 0x8e */	 0x80a8,
/* 0x8f */	 0x80aa,
/* 0x90 */	 0x8200,
/* 0x91 */	 0x8202,
/* 0x92 */	 0x8208,
/* 0x93 */	 0x820a,
/* 0x94 */	 0x8220,
/* 0x95 */	 0x8222,
/* 0x96 */	 0x8228,
/* 0x97 */	 0x822a,
/* 0x98 */	 0x8280,
/* 0x99 */	 0x8282,
/* 0x9a */	 0x8288,
/* 0x9b */	 0x828a,
/* 0x9c */	 0x82a0,
/* 0x9d */	 0x82a2,
/* 0x9e */	 0x82a8,
/* 0x9f */	 0x82aa,
/* 0xa0 */	 0x8800,
/* 0xa1 */	 0x8802,
/* 0xa2 */	 0x8808,
/* 0xa3 */	 0x880a,
/* 0xa4 */	 0x8820,
/* 0xa5 */	 0x8822,
/* 0xa6 */	 0x8828,
/* 0xa7 */	 0x882a,
/* 0xa8 */	 0x8880,
/* 0xa9 */	 0x8882,
/* 0xaa */	 0x8888,
/* 0xab */	 0x888a,
/* 0xac */	 0x88a0,
/* 0xad */	 0x88a2,
/* 0xae */	 0x88a8,
/* 0xaf */	 0x88aa,
/* 0xb0 */	 0x8a00,
/* 0xb1 */	 0x8a02,
/* 0xb2 */	 0x8a08,
/* 0xb3 */	 0x8a0a,
/* 0xb4 */	 0x8a20,
/* 0xb5 */	 0x8a22,
/* 0xb6 */	 0x8a28,
/* 0xb7 */	 0x8a2a,
/* 0xb8 */	 0x8a80,
/* 0xb9 */	 0x8a82,
/* 0xba */	 0x8a88,
/* 0xbb */	 0x8a8a,
/* 0xbc */	 0x8aa0,
/* 0xbd */	 0x8aa2,
/* 0xbe */	 0x8aa8,
/* 0xbf */	 0x8aaa,
/* 0xc0 */	 0xa000,
/* 0xc1 */	 0xa002,
/* 0xc2 */	 0xa008,
/* 0xc3 */	 0xa00a,
/* 0xc4 */	 0xa020,
/* 0xc5 */	 0xa022,
/* 0xc6 */	 0xa028,
/* 0xc7 */	 0xa02a,
/* 0xc8 */	 0xa080,
/* 0xc9 */	 0xa082,
/* 0xca */	 0xa088,
/* 0xcb */	 0xa08a,
/* 0xcc */	 0xa0a0,
/* 0xcd */	 0xa0a2,
/* 0xce */	 0xa0a8,
/* 0xcf */	 0xa0aa,
/* 0xd0 */	 0xa200,
/* 0xd1 */	 0xa202,
/* 0xd2 */	 0xa208,
/* 0xd3 */	 0xa20a,
/* 0xd4 */	 0xa220,
/* 0xd5 */	 0xa222,
/* 0xd6 */	 0xa228,
/* 0xd7 */	 0xa22a,
/* 0xd8 */	 0xa280,
/* 0xd9 */	 0xa282,
/* 0xda */	 0xa288,
/* 0xdb */	 0xa28a,
/* 0xdc */	 0xa2a0,
/* 0xdd */	 0xa2a2,
/* 0xde */	 0xa2a8,
/* 0xdf */	 0xa2aa,
/* 0xe0 */	 0xa800,
/* 0xe1 */	 0xa802,
/* 0xe2 */	 0xa808,
/* 0xe3 */	 0xa80a,
/* 0xe4 */	 0xa820,
/* 0xe5 */	 0xa822,
/* 0xe6 */	 0xa828,
/* 0xe7 */	 0xa82a,
/* 0xe8 */	 0xa880,
/* 0xe9 */	 0xa882,
/* 0xea */	 0xa888,
/* 0xeb */	 0xa88a,
/* 0xec */	 0xa8a0,
/* 0xed */	 0xa8a2,
/* 0xee */	 0xa8a8,
/* 0xef */	 0xa8aa,
/* 0xf0 */	 0xaa00,
/* 0xf1 */	 0xaa02,
/* 0xf2 */	 0xaa08,
/* 0xf3 */	 0xaa0a,
/* 0xf4 */	 0xaa20,
/* 0xf5 */	 0xaa22,
/* 0xf6 */	 0xaa28,
/* 0xf7 */	 0xaa2a,
/* 0xf8 */	 0xaa80,
/* 0xf9 */	 0xaa82,
/* 0xfa */	 0xaa88,
/* 0xfb */	 0xaa8a,
/* 0xfc */	 0xaaa0,
/* 0xfd */	 0xaaa2,
/* 0xfe */	 0xaaa8,
/* 0xff */	 0xaaaa,
};

unsigned short G91imagetable[256] = {
/* 0x0 */	 0x0000,
/* 0x1 */	 0x0001,
/* 0x2 */	 0x0004,
/* 0x3 */	 0x0005,
/* 0x4 */	 0x0010,
/* 0x5 */	 0x0011,
/* 0x6 */	 0x0014,
/* 0x7 */	 0x0015,
/* 0x8 */	 0x0040,
/* 0x9 */	 0x0041,
/* 0xa */	 0x0044,
/* 0xb */	 0x0045,
/* 0xc */	 0x0050,
/* 0xd */	 0x0051,
/* 0xe */	 0x0054,
/* 0xf */	 0x0055,
/* 0x10 */	 0x0100,
/* 0x11 */	 0x0101,
/* 0x12 */	 0x0104,
/* 0x13 */	 0x0105,
/* 0x14 */	 0x0110,
/* 0x15 */	 0x0111,
/* 0x16 */	 0x0114,
/* 0x17 */	 0x0115,
/* 0x18 */	 0x0140,
/* 0x19 */	 0x0141,
/* 0x1a */	 0x0144,
/* 0x1b */	 0x0145,
/* 0x1c */	 0x0150,
/* 0x1d */	 0x0151,
/* 0x1e */	 0x0154,
/* 0x1f */	 0x0155,
/* 0x20 */	 0x0400,
/* 0x21 */	 0x0401,
/* 0x22 */	 0x0404,
/* 0x23 */	 0x0405,
/* 0x24 */	 0x0410,
/* 0x25 */	 0x0411,
/* 0x26 */	 0x0414,
/* 0x27 */	 0x0415,
/* 0x28 */	 0x0440,
/* 0x29 */	 0x0441,
/* 0x2a */	 0x0444,
/* 0x2b */	 0x0445,
/* 0x2c */	 0x0450,
/* 0x2d */	 0x0451,
/* 0x2e */	 0x0454,
/* 0x2f */	 0x0455,
/* 0x30 */	 0x0500,
/* 0x31 */	 0x0501,
/* 0x32 */	 0x0504,
/* 0x33 */	 0x0505,
/* 0x34 */	 0x0510,
/* 0x35 */	 0x0511,
/* 0x36 */	 0x0514,
/* 0x37 */	 0x0515,
/* 0x38 */	 0x0540,
/* 0x39 */	 0x0541,
/* 0x3a */	 0x0544,
/* 0x3b */	 0x0545,
/* 0x3c */	 0x0550,
/* 0x3d */	 0x0551,
/* 0x3e */	 0x0554,
/* 0x3f */	 0x0555,
/* 0x40 */	 0x1000,
/* 0x41 */	 0x1001,
/* 0x42 */	 0x1004,
/* 0x43 */	 0x1005,
/* 0x44 */	 0x1010,
/* 0x45 */	 0x1011,
/* 0x46 */	 0x1014,
/* 0x47 */	 0x1015,
/* 0x48 */	 0x1040,
/* 0x49 */	 0x1041,
/* 0x4a */	 0x1044,
/* 0x4b */	 0x1045,
/* 0x4c */	 0x1050,
/* 0x4d */	 0x1051,
/* 0x4e */	 0x1054,
/* 0x4f */	 0x1055,
/* 0x50 */	 0x1100,
/* 0x51 */	 0x1101,
/* 0x52 */	 0x1104,
/* 0x53 */	 0x1105,
/* 0x54 */	 0x1110,
/* 0x55 */	 0x1111,
/* 0x56 */	 0x1114,
/* 0x57 */	 0x1115,
/* 0x58 */	 0x1140,
/* 0x59 */	 0x1141,
/* 0x5a */	 0x1144,
/* 0x5b */	 0x1145,
/* 0x5c */	 0x1150,
/* 0x5d */	 0x1151,
/* 0x5e */	 0x1154,
/* 0x5f */	 0x1155,
/* 0x60 */	 0x1400,
/* 0x61 */	 0x1401,
/* 0x62 */	 0x1404,
/* 0x63 */	 0x1405,
/* 0x64 */	 0x1410,
/* 0x65 */	 0x1411,
/* 0x66 */	 0x1414,
/* 0x67 */	 0x1415,
/* 0x68 */	 0x1440,
/* 0x69 */	 0x1441,
/* 0x6a */	 0x1444,
/* 0x6b */	 0x1445,
/* 0x6c */	 0x1450,
/* 0x6d */	 0x1451,
/* 0x6e */	 0x1454,
/* 0x6f */	 0x1455,
/* 0x70 */	 0x1500,
/* 0x71 */	 0x1501,
/* 0x72 */	 0x1504,
/* 0x73 */	 0x1505,
/* 0x74 */	 0x1510,
/* 0x75 */	 0x1511,
/* 0x76 */	 0x1514,
/* 0x77 */	 0x1515,
/* 0x78 */	 0x1540,
/* 0x79 */	 0x1541,
/* 0x7a */	 0x1544,
/* 0x7b */	 0x1545,
/* 0x7c */	 0x1550,
/* 0x7d */	 0x1551,
/* 0x7e */	 0x1554,
/* 0x7f */	 0x1555,
/* 0x80 */	 0x4000,
/* 0x81 */	 0x4001,
/* 0x82 */	 0x4004,
/* 0x83 */	 0x4005,
/* 0x84 */	 0x4010,
/* 0x85 */	 0x4011,
/* 0x86 */	 0x4014,
/* 0x87 */	 0x4015,
/* 0x88 */	 0x4040,
/* 0x89 */	 0x4041,
/* 0x8a */	 0x4044,
/* 0x8b */	 0x4045,
/* 0x8c */	 0x4050,
/* 0x8d */	 0x4051,
/* 0x8e */	 0x4054,
/* 0x8f */	 0x4055,
/* 0x90 */	 0x4100,
/* 0x91 */	 0x4101,
/* 0x92 */	 0x4104,
/* 0x93 */	 0x4105,
/* 0x94 */	 0x4110,
/* 0x95 */	 0x4111,
/* 0x96 */	 0x4114,
/* 0x97 */	 0x4115,
/* 0x98 */	 0x4140,
/* 0x99 */	 0x4141,
/* 0x9a */	 0x4144,
/* 0x9b */	 0x4145,
/* 0x9c */	 0x4150,
/* 0x9d */	 0x4151,
/* 0x9e */	 0x4154,
/* 0x9f */	 0x4155,
/* 0xa0 */	 0x4400,
/* 0xa1 */	 0x4401,
/* 0xa2 */	 0x4404,
/* 0xa3 */	 0x4405,
/* 0xa4 */	 0x4410,
/* 0xa5 */	 0x4411,
/* 0xa6 */	 0x4414,
/* 0xa7 */	 0x4415,
/* 0xa8 */	 0x4440,
/* 0xa9 */	 0x4441,
/* 0xaa */	 0x4444,
/* 0xab */	 0x4445,
/* 0xac */	 0x4450,
/* 0xad */	 0x4451,
/* 0xae */	 0x4454,
/* 0xaf */	 0x4455,
/* 0xb0 */	 0x4500,
/* 0xb1 */	 0x4501,
/* 0xb2 */	 0x4504,
/* 0xb3 */	 0x4505,
/* 0xb4 */	 0x4510,
/* 0xb5 */	 0x4511,
/* 0xb6 */	 0x4514,
/* 0xb7 */	 0x4515,
/* 0xb8 */	 0x4540,
/* 0xb9 */	 0x4541,
/* 0xba */	 0x4544,
/* 0xbb */	 0x4545,
/* 0xbc */	 0x4550,
/* 0xbd */	 0x4551,
/* 0xbe */	 0x4554,
/* 0xbf */	 0x4555,
/* 0xc0 */	 0x5000,
/* 0xc1 */	 0x5001,
/* 0xc2 */	 0x5004,
/* 0xc3 */	 0x5005,
/* 0xc4 */	 0x5010,
/* 0xc5 */	 0x5011,
/* 0xc6 */	 0x5014,
/* 0xc7 */	 0x5015,
/* 0xc8 */	 0x5040,
/* 0xc9 */	 0x5041,
/* 0xca */	 0x5044,
/* 0xcb */	 0x5045,
/* 0xcc */	 0x5050,
/* 0xcd */	 0x5051,
/* 0xce */	 0x5054,
/* 0xcf */	 0x5055,
/* 0xd0 */	 0x5100,
/* 0xd1 */	 0x5101,
/* 0xd2 */	 0x5104,
/* 0xd3 */	 0x5105,
/* 0xd4 */	 0x5110,
/* 0xd5 */	 0x5111,
/* 0xd6 */	 0x5114,
/* 0xd7 */	 0x5115,
/* 0xd8 */	 0x5140,
/* 0xd9 */	 0x5141,
/* 0xda */	 0x5144,
/* 0xdb */	 0x5145,
/* 0xdc */	 0x5150,
/* 0xdd */	 0x5151,
/* 0xde */	 0x5154,
/* 0xdf */	 0x5155,
/* 0xe0 */	 0x5400,
/* 0xe1 */	 0x5401,
/* 0xe2 */	 0x5404,
/* 0xe3 */	 0x5405,
/* 0xe4 */	 0x5410,
/* 0xe5 */	 0x5411,
/* 0xe6 */	 0x5414,
/* 0xe7 */	 0x5415,
/* 0xe8 */	 0x5440,
/* 0xe9 */	 0x5441,
/* 0xea */	 0x5444,
/* 0xeb */	 0x5445,
/* 0xec */	 0x5450,
/* 0xed */	 0x5451,
/* 0xee */	 0x5454,
/* 0xef */	 0x5455,
/* 0xf0 */	 0x5500,
/* 0xf1 */	 0x5501,
/* 0xf2 */	 0x5504,
/* 0xf3 */	 0x5505,
/* 0xf4 */	 0x5510,
/* 0xf5 */	 0x5511,
/* 0xf6 */	 0x5514,
/* 0xf7 */	 0x5515,
/* 0xf8 */	 0x5540,
/* 0xf9 */	 0x5541,
/* 0xfa */	 0x5544,
/* 0xfb */	 0x5545,
/* 0xfc */	 0x5550,
/* 0xfd */	 0x5551,
/* 0xfe */	 0x5554,
/* 0xff */	 0x5555,
};

int
_init(void)
{
    register int	e;

    DEBUGF(1, (CE_CONT, "p9100: compiled %s, %s\n", __TIME__, __DATE__));

    if ((e = ddi_soft_state_init(&p9100_softc_head,
				sizeof (struct p9100_softc), 1)) != 0) {
	DEBUGF(1, (CE_CONT, "done\n"));
	return (e);
    }

    e = mod_install(&p9100_modlinkage);

    if (e) {
	ddi_soft_state_fini(&p9100_softc_head);
	DEBUGF(1, (CE_CONT, "done\n"));
    }
    DEBUGF(1, (CE_CONT, "p9100: _init done rtn=%d\n", e));
    return (e);
}

int
_fini(void)
{
    register int	e;

    if ((e = mod_remove(&p9100_modlinkage)) != 0)
	return (e);

    ddi_soft_state_fini(&p9100_softc_head);

    return (0);
}

int
_info(struct modinfo *const modinfop)
{
    return (mod_info(&p9100_modlinkage, modinfop));
}

static
p9100_identify(register dev_info_t *const devi)
{
    register char *const	name = ddi_get_name(devi);

    DEBUGF(1, (CE_CONT, "p9100_identify (%s) unit=%d\n",
	ddi_get_name(devi), ddi_get_instance(devi)));

    if (strncmp(name, "p9100", 4) == 0 ||
	strncmp(name, "SUNW,p9100", 9) == 0 ||
	strncmp(name, "pci", 3) == 0)
	return (DDI_IDENTIFIED);
    else
	return (DDI_NOT_IDENTIFIED);
}

static int
p9100_probe(register dev_info_t *const devi)
{
    DEBUGF(1, (CE_CONT, "p9100_probe (%s) unit=%d\n",
	ddi_get_name(devi), ddi_get_instance(devi)));

    if (ddi_dev_is_sid(devi) == DDI_SUCCESS) {
	DEBUGF(2, (CE_CONT, "p9100_probe dev is sid\n"));
	return (DDI_PROBE_DONTCARE);
    }

    return (viper_vlb_probe(devi));
}


static int
p9100_attach(register dev_info_t *const devi,
	register ddi_attach_cmd_t const cmd)
{
    register struct p9100_softc	*softc;
    register int const		unit = ddi_get_instance(devi);
    p9100p_t    p9100reg;
    char	name[16];
#ifdef POWER
    int		proplen;
    ulong_t	timestamp[2];
    int		power[2];
#endif
    int		length;

    DEBUGF(1, (CE_CONT, "p9100_attach unit=%d cmd=%d\n", unit, (int)cmd));

    switch (cmd) {
    case DDI_ATTACH:
	break;

    case DDI_RESUME:
	if (!(softc = (struct p9100_softc *)ddi_get_driver_private(devi)))
	    return (DDI_FAILURE);
	if (!softc->suspended)
	    return (DDI_SUCCESS);
	mutex_enter(&softc->mutex);

	p9100_reset(softc);

	if (softc->curctx) {
#if 0
	    u_int	display_size = softc->w * softc->h;
#endif

	    p9100reg = softc->p9100;

	    /* Restore non display RAM */
#if 0
	    ddi_rep_putb(softc->p9100_fb_handle, softc->ndvram,
		((u_char *)&p9100reg->p9100_frame_buffer[display_size]),
		softc->ndvramsz, DDI_DEV_AUTOINCR);
	    kmem_free(softc->ndvram, softc->ndvramsz);
#endif

	    /* Restore other frame buffer state */
	    (void) p9100_cntxrestore(p9100reg, softc->curctx);
	    (*softc->setcurshape)(softc);
	    p9100_update_cmap(softc, (u_int)0, P9100_CMAP_ENTRIES);
	    p9100_update_omap(softc);

	    p9100_int_enable(softc);	/* Schedule the update */
	}

	softc->suspended = 0;
	mutex_exit(&softc->mutex);
#ifdef POWER
	/* Restore brightness level */
	if (ddi_getlongprop_buf(DDI_DEV_T_ANY, devi, 0,
			"pm_norm_pwr", (caddr_t)power, &proplen)
			== DDI_PROP_SUCCESS)

	    if (p9100_power(devi, 1, power[1]) != DDI_SUCCESS)
		return (DDI_FAILURE);
#endif

	return (DDI_SUCCESS);

    default:
	return (DDI_FAILURE);
    }

    DEBUGF(1, (CE_CONT, "p9100_attach unit=%d\n", unit));

    p9100_pagesize = ddi_ptob(devi, 1);
#if ALWAYS_NO_INTERRUPTS
    p9100_pageoffset = p9100_pagesize - 1;
#endif

    /* Allocate softc struct */
    if (ddi_soft_state_zalloc(p9100_softc_head, unit))
	return (DDI_FAILURE);

    softc = getsoftc(unit);

    /* link it in */
    softc->devi = devi;

    DEBUGF(1, (CE_CONT, "p9100_attach devi=0x%x unit=%d\n", devi, unit));

    ddi_set_driver_private(devi, (caddr_t)softc);

    softc->dac_type = VIPER_DAC_UNKNOWN;
    /* Check the bus, either PCI or VLB */
    length = sizeof (name);
    if (ddi_getlongprop_buf(DDI_DEV_T_NONE, ddi_get_parent(devi),
	DDI_PROP_DONTPASS, "device_type", (caddr_t)name,
	&length) != DDI_PROP_SUCCESS) {
	softc->bus_type = VIPER_PORT_BUS_VLB;
    } else if (strcmp(name, "pci") == 0)
	softc->bus_type = VIPER_PORT_BUS_PCI;
    else
	softc->bus_type = VIPER_PORT_BUS_VLB;

    strcpy((caddr_t)&softc->p9100_ident, "SUNWp9100");
    softc->setcmap = null_routine;
    softc->setcurpos = null_routine;
    softc->setcurshape = null_routine;
    softc->setcurcolor = null_routine;
#if __ppc
    softc->dac_type = VIPER_DAC_RGB525;
    softc->setcmap = rgb525_putcmap;
    softc->setcurpos = rgb525_setcurpos;
    softc->setcurshape = rgb525_setcurshape;
    softc->setcurcolor = rgb525_setcurcolor;
#endif

    if (softc->bus_type == VIPER_PORT_BUS_PCI) {
	/* P9100 on PCI */

	if (ddi_regs_map_setup(devi, P9100_REG_RNUMBER,
		(caddr_t *)&softc->p9100,
		0x0, sizeof (p9100_t), &endian_attr,
		&softc->p9100_regs_handle) != DDI_SUCCESS) {
	    DEBUGF(2, (CE_CONT,
		    "p9100_attach: ddi_map_regs failed\n"));
	    return (DDI_FAILURE);
	}
#if 0
	/*
	 * not sure what to do here, since we don't necc. know how
	 * big the frame buffer is.
	 */
	/* map in the framebuffer */
	if (ddi_regs_map_setup(devi, P9100_REG_RNUMBER,
		(caddr_t *)&p9100reg,
		P9100_FRAME_BUFFER_BASE, P9100_FRAME_BUFFER_SIZE,
		&nosw_attr, &softc->p9100_fb_handle)) {
	    DEBUGF(2, (CE_CONT,
		    "p9100_attach: ddi_map_regs fb FAILED\n"));
	    return (DDI_FAILURE);
	}
#endif

	if (pci_config_setup(devi, &softc->p9100_config_handle)) {
	    DEBUGF(2, (CE_CONT,
		    "p9100_attach: pci_config_setup FAILED\n"));
	    return (DDI_FAILURE);
	}
    } else {
	/*
	 *
	 * P9000 assumes that VLB regspecs conform to the proposed
	 * format:
	 *
	 *  <drv-reg=i,address,size>,<drv-intr=irq>,<drv-dmachan=dmachan>,
	 *      <drv-ipl=interrupt-priority>
	 *           where i=0 for Memory space
	 *                 i=1 for I/O space
	 * viper-reg=0,0xa0000000,0x40000,
	 *           1,0x0,0x10000
	 *       viper-intr=11 viper-ipl=5
	 */

	/*
	 * Map in the device registers.
	 */
	if (ddi_regs_map_setup(devi, 0,
		(caddr_t *)&softc->p9100,
		0x0, sizeof (p9100_t), &endian_attr,
		&softc->p9100_regs_handle) != DDI_SUCCESS) {
	    DEBUGF(2, (CE_CONT,
		    "p9100_attach: ddi_map_regs (p9100) FAILED\n"));
	    return (DDI_FAILURE);
	}
    }

    softc->vrtpage = NULL;
    softc->vrtalloc = NULL;
    softc->vrtmaps = 0;
    softc->vrtflag = 0;

    p9100_get_hardware_settings(softc);

    p9100_reset(softc);

#if ALWAYS_NO_INTERRUPTS
    /* attach interrupt, notice the dance... see 1102427 */
    if (ddi_add_intr(devi, 0, &softc->iblock_cookie, 0,
		    (u_int (*)()) nulldev, (caddr_t)0) != DDI_SUCCESS) {
	DEBUGF(2, (CE_CONT, "p9100_attach%d add_intr failed\n", unit));
	(void) p9100_detach(devi, DDI_DETACH);
	return (DDI_FAILURE);
    }

    mutex_init(&softc->interlock, "p9100_interlock", MUTEX_DRIVER,
    softc->iblock_cookie);
    mutex_init(&softc->mutex, "p9100", MUTEX_DRIVER, softc->iblock_cookie);
    cv_init(&softc->vrtsleep, "p9100", CV_DRIVER, softc->iblock_cookie);

    ddi_remove_intr(devi, 0, softc->iblock_cookie);

    if (ddi_add_intr(devi, 0, &softc->iblock_cookie, 0,
		    p9100_intr, (caddr_t)softc) != DDI_SUCCESS) {
	DEBUGF(2, (CE_CONT,
		    "p9100_attach%d add_intr failed\n", unit));
	(void) p9100_detach(devi, DDI_DETACH);
	return (DDI_FAILURE);
    }

    softc->have_intr = 1;

#else /* ALWAYS_NO_INTERRUPTS */

    softc->have_intr = 0;
    mutex_init(&softc->interlock, "p9100_interlock", MUTEX_DRIVER,
    softc->iblock_cookie);
    mutex_init(&softc->mutex, "p9100", MUTEX_DRIVER, softc->iblock_cookie);
    cv_init(&softc->vrtsleep, "p9100", CV_DRIVER, softc->iblock_cookie);

#endif /* ALWAYS_NO_INTERRUPTS */

    /*
     * Initialize hardware colormap and software colormap images. It might
     * make sense to read the hardware colormap here.
     */
    p9100_reset_cmap(softc->cmap[0], P9100_CMAP_ENTRIES);
    p9100_reset_cmap(softc->omap[0], P9100_CURSOR_ENTRIES);
    p9100_update_cmap(softc, (u_int)0, P9100_CMAP_ENTRIES);
    p9100_update_omap(softc);

    DEBUGF(2, (CE_CONT,
	"p9100_attach%d just before create_minor node\n", unit));

    (void) sprintf(name, "p9100_%d", unit);

    if (ddi_create_minor_node(devi, name, S_IFCHR,
				unit, DDI_NT_DISPLAY, 0) == DDI_FAILURE) {
	ddi_remove_minor_node(devi, NULL);
	DEBUGF(2, (CE_CONT,
	    "p9100_attach%d create_minor node failed\n", unit));
	return (DDI_FAILURE);
    }

    ddi_report_dev(devi);

    /*
     * Initialize the shared context for this unit
     */
    softc->shared_ctx.p9100_ctx_pvt = NULL;
    softc->pvt_ctx = NULL;

#ifdef POWER
    /* Create power management properties */
    timestamp[0] = 0;
    drv_getparm(TIME, timestamp+1);
    (void) ddi_prop_create(DDI_DEV_T_NONE, devi, DDI_PROP_CANSLEEP,
			    "pm_timestamp", (caddr_t)timestamp,
			    sizeof (timestamp));
    power[0] = 1;
    power[1] = 255;
    (void) ddi_prop_create(DDI_DEV_T_NONE, devi, DDI_PROP_CANSLEEP,
			    "pm_norm_pwr", (caddr_t)power, sizeof (power));
#endif

#if CONSOLE_REDESIGN
    (void) ddi_prop_create(makedevice(DDI_MAJOR_T_UNKNOWN, unit),
      devi, DDI_PROP_CANSLEEP, DDI_KERNEL_IOCTL, NULL, 0);
#endif
    return (DDI_SUCCESS);
}


static int
p9100_detach(register dev_info_t *const devi,
	register ddi_detach_cmd_t const cmd)
{
    register int const	unit = ddi_get_instance(devi);
    register struct p9100_softc *const	softc = getsoftc(unit);
    register p9100p_t const p9100 = softc->p9100;

    DEBUGF(1, (CE_CONT, "p9100_detach softc=%x, devi=0x%x\n", softc, devi));

    switch (cmd) {
    case DDI_DETACH:
	break;

    case DDI_SUSPEND:
	if (softc == NULL)
	    return (DDI_FAILURE);
	if (softc->suspended)
	    return (DDI_FAILURE);

	mutex_enter(&softc->mutex);

	if (softc->curctx) {
#if 0
	    register u_int  const display_size = softc->w * softc->h;
#endif

	    /* Save non display RAM */
	    /* 2Mb - displayable memory */
	    /* NUST be changed for other boards or worked out dynamically */
#if 0
	    softc->ndvramsz = (2 * 1024 * 1024) - display_size;
	    if ((softc->ndvram = kmem_alloc(softc->ndvramsz,
					    KM_NOSLEEP)) == NULL) {
		mutex_exit(&softc->mutex);
		return (DDI_FAILURE);
	    }
#endif

#if 0
	    ddi_rep_getb(softc->p9100_fb_handle, softc->ndvram,
		(u_char *)&p9100->p9100_frame_buffer[display_size],
		softc->ndvramsz, DDI_DEV_AUTOINCR);
#endif

	    /* Save other frame buffer state */
	    (void) p9100_cntxsave(p9100, softc->curctx);
	}

	softc->suspended = 1;

	mutex_exit(&softc->mutex);

	return (DDI_SUCCESS);

    default:
	return (DDI_FAILURE);
    }

    /* shut off video */

    P9100_CLEAR_ENABLE_VIDEO(softc->p9100);

    mutex_enter(&softc->mutex);
    mutex_enter(&(softc)->interlock);
    p9100_int_disable_intr(softc);
    mutex_exit(&(softc)->interlock);
    mutex_exit(&softc->mutex);

    if (softc->have_intr)
	ddi_remove_intr(devi, 0, softc->iblock_cookie);

    if (softc->bus_type == VIPER_PORT_BUS_PCI)
	pci_config_teardown(&softc->p9100_config_handle);
    ddi_regs_map_free(&softc->p9100_regs_handle);
#if 0
    /* unmap the framebuffer */
    ddi_regs_map_free(&softc->p9100_fb_handle);
#endif


    if (softc->vrtalloc != NULL)
	kmem_free(softc->vrtalloc, p9100_pagesize * 2);

    mutex_destroy(&softc->mutex);
    cv_destroy(&softc->vrtsleep);

    ASSERT2(softc->curctx == NULL);

    ddi_soft_state_free(p9100_softc_head, unit);
    return (DDI_SUCCESS);
}


#ifdef POWER
static int
p9100_power(dev_info_t *dip, int cmpt, int level)
{
    int		power[2];
    register struct p9100_softc	*softc;

    if (cmpt != 1 || 0 > level || level > 255 ||
	    !(softc = (struct p9100_softc *)ddi_get_driver_private(dip)))
	return (DDI_FAILURE);

    if (level) {
	p9100_set_video(softc, FBVIDEO_ON);
	power[0] = 1;
	power[1] = level;
	ddi_prop_modify(DDI_DEV_T_NONE, dip, DDI_PROP_CANSLEEP,
			"pm_norm_pwr", (caddr_t)power, sizeof (power));
    } else {
	p9100_set_video(softc, FBVIDEO_OFF);
    }

    (void) ddi_power(dip, cmpt, level);

    return (DDI_SUCCESS);
}
#endif

/* ARGSUSED */
static int
p9100_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
    register int	instance;
    register int	error = DDI_SUCCESS;
    register struct p9100_softc	*softc;

    instance = getminor((dev_t)arg);
    switch (infocmd) {
    case DDI_INFO_DEVT2DEVINFO:
	softc = getsoftc(instance);
	if (softc == NULL) {
	    error = DDI_FAILURE;
	} else {
	    *result = (void *)softc->devi;
	    error = DDI_SUCCESS;
	}
	break;
    case DDI_INFO_DEVT2INSTANCE:
	*result = (void *)instance;
	error = DDI_SUCCESS;
	break;
    default:
	error = DDI_FAILURE;
    }
    return (error);
}

/* ARGSUSED */
static
p9100_open(register dev_t *const devp, register int const flag,
	register int const otyp, register cred_t *const cred)
{
    register int const	unit = getminor(*devp);
    struct p9100_softc	*softc = getsoftc(unit);
    int	error = 0;

    DEBUGF(2, (CE_CONT, "p9100_open(%d)\n", unit));

    /*
     * is this gorp necessary?
     */
    if (otyp != OTYP_CHR) {
	error = EINVAL;
    } else if (softc == NULL) {
	error = ENXIO;
    }
    return (error);
}


/* ARGSUSED */
static
p9100_close(register dev_t const dev, register int const flag,
	register int const otyp, register cred_t *const cred)
{
    register int const	unit = getminor(dev);
    struct p9100_softc	*softc = getsoftc(unit);
    int	error = 0;

    DEBUGF(2, (CE_CONT, "p9100_close(%d, %d, %d)\n", unit, flag, otyp));

    if (otyp != OTYP_CHR) {
	error = EINVAL;
    } else if (softc == NULL) {
	error = ENXIO;
    } else {
	mutex_enter(&softc->mutex);
	softc->cur.enable = 0;
	softc->curctx = NULL;
	p9100_reset(softc);
	mutex_exit(&softc->mutex);
    }
    return (error);
}

long    p9100_read_addr;

/* ARGSUSED */
static int
p9100_read(register dev_t const dev,
	    register struct uio *const uio,
	    register cred_t *const cred)
{
    register struct p9100_softc *const  softc = getsoftc(getminor(dev));
    register p9100p_t   const p9100 = softc->p9100;
    register iovec_t	*iov;
    register long	volatile   *sptr;
    register long	*dptr;
    register long	srcpos;
    register long	length;
    register long	offset;
    register long	count;
    register int	error = 0;
    long		buffer[32];

    while (uio->uio_resid > 0 && error == 0) {
	iov = uio->uio_iov;
	if (iov->iov_len == 0) {
	    uio->uio_iov++;
	    uio->uio_iovcnt--;
	    continue;
	}

	if (uio->uio_offset >= P9100_SIZE)
	return (0);

	length = iov->iov_len;
	if (uio->uio_offset + length > P9100_SIZE)
	    length = P9100_SIZE - uio->uio_offset;

	offset = uio->uio_offset & (sizeof (long) - 1);

	if (length + offset > sizeof (buffer))
	    length = sizeof (buffer) - offset;

	srcpos = uio->uio_offset / sizeof (long) * sizeof (long);
	sptr = (long volatile *)((long)p9100 + srcpos);
	dptr = buffer;
	count = (length + offset + sizeof (long) - 1) / sizeof (long);

	while (count > 0) {
	    p9100_read_addr = srcpos;
	    if (!P9100_BAD_ADDRESSES(srcpos)) {
		if ((srcpos & 0xfff8ff00) == 0)
		    (void) P9100_READ_FRAMEBUFFER_FOR_SYSCTRL(p9100);
		else if ((srcpos & 0xfff8fe00) ==
			(long)&((p9100p_t)0)->p9100_regs[0].p9100_cr_ramdac)
		    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100);
		*dptr = *sptr;
	    }
	    else
		*dptr = 0xffffffff;
	    dptr++;
	    sptr++;
	    srcpos += sizeof (long);
	    count--;
	}
	error = uiomove((caddr_t)buffer + offset, length, UIO_READ, uio);
    }
    return (0);
}


/* ARGSUSED */
static int
p9100_write(register dev_t const dev,
	    register struct uio *const uio,
	    register cred_t *const cred)
{
    register struct p9100_softc *const  softc = getsoftc(getminor(dev));
    register p9100p_t   const p9100 = softc->p9100;
    register iovec_t	*iov;
    register long	volatile   *dptr;
    register long	*sptr;
    register long	dstpos;
    register long	length;
    register long	offset;
    register long	count;
    register int	error = 0;
    long		buffer[32];

    while (uio->uio_resid > 0 && error == 0) {
	iov = uio->uio_iov;
	if (iov->iov_len == 0) {
	    uio->uio_iov++;
	    uio->uio_iovcnt--;
	    continue;
	}

	if (uio->uio_offset >= P9100_SIZE)
	    return (0);

	length = iov->iov_len;
	if (uio->uio_offset + length > P9100_SIZE)
	    length = P9100_SIZE - uio->uio_offset;

	offset = uio->uio_offset & (sizeof (long) - 1);

	if (length + offset > sizeof (buffer))
	    length = sizeof (buffer) - offset;

	dstpos = uio->uio_offset / sizeof (long) * sizeof (long);
	dptr = (long volatile *)((long)softc->p9100 + dstpos);
	sptr = buffer;
	count = (length + offset + sizeof (long) - 1) / sizeof (long);

	if (offset) {
	    if (!P9100_BAD_ADDRESSES(dstpos)) {
		if ((dstpos & 0xfff8ff00) == 0)
		    (void) P9100_READ_FRAMEBUFFER_FOR_SYSCTRL(p9100);
		else if ((dstpos & 0xfff8fe00) ==
			(long)&((p9100p_t)0)->p9100_regs[0].p9100_cr_ramdac)
		    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100);
		sptr[0] = dptr[0];
	    }
	    else
		sptr[0] = 0xffffffff;
	}

	if ((offset + length) & (sizeof (long) - 1) && count > 1) {
	    if (!P9100_BAD_ADDRESSES(dstpos + (count - 1) * sizeof (long))) {
		if (((dstpos + (count - 1) * sizeof (long)) & 0xfff8ff00) == 0)
		    (void) P9100_READ_FRAMEBUFFER_FOR_SYSCTRL(p9100);
		else if (((dstpos + (count - 1) * sizeof (long)) & 0xfff8fe00) == (long)&((p9100p_t)0)->p9100_regs[0].p9100_cr_ramdac)
		    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100);
		sptr[count - 1] = dptr[count - 1];
	    }
	    else
		sptr[count - 1] = 0xffffffff;
	}

	error = uiomove((caddr_t)buffer + offset, length, UIO_WRITE, uio);

	while (count > 0) {
	    if (!P9100_BAD_ADDRESSES(dstpos)) {
		if ((dstpos & 0xfff8ff00) == 0)
		    (void) P9100_READ_FRAMEBUFFER_FOR_SYSCTRL(p9100);
		else if ((dstpos & 0xfff8fe00) ==
			(long)&((p9100p_t)0)->p9100_regs[0].p9100_cr_ramdac)
		    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100);
		*dptr = *sptr;
	    }
	    dptr++;
	    sptr++;
	    dstpos += sizeof (long);
	    count--;
	}
    }
    return (0);
}


/* ARGSUSED */
static
p9100_mmap(register dev_t const dev, register off_t const off,
	register int const prot)
{
    register struct p9100_softc *const	softc = getsoftc(getminor(dev));
    register int	diff;
    register caddr_t	vaddr;
    register int	rval;

    DEBUGF(off ? 5 : 1, (CE_CONT, "p9100_mmap(%d, 0x%x)\n",
			    getminor(dev), (u_int)off));

    if ((diff = off - P9100_VBASE) >= 0 && diff < P9100_SIZE)
	vaddr = (caddr_t)softc->p9100 + diff;

#if ALWAYS_NO_INTERRUPTS

    else if ((diff = off - P9100_VRT_VADDR) >= 0 && diff < P9100_VRT_SIZE)
	vaddr = softc->vrtpage ?
		    (caddr_t)softc->vrtpage + diff : (caddr_t)-1;

#endif /* ALWAYS_NO_INTERRUPTS */

    else
	vaddr = (caddr_t)-1;

    if (vaddr != (caddr_t)-1)
	rval = hat_getkpfnum(vaddr);
    else
	rval = -1;

    DEBUGF(5, (CE_CONT, "p9100_mmap returning 0x%x\n", rval));

    return (rval);
}

/* ARGSUSED */
static
p9100_ioctl(dev_t dev, int cmd, intptr_t data,
	int mode, cred_t *cred, int *rval)
{
    register struct p9100_softc *const	softc = getsoftc(getminor(dev));
    register int	cursor_cmap;
    int		i;

    u_char	*iobuf_cmap_red;
    u_char	*iobuf_cmap_green;
    u_char	*iobuf_cmap_blue;
    u_char	*stack_cmap;

    struct fbcmap	*cmap, cma;
    struct fbcursor	cp;

    u_int	index;
    u_int	count;
    u_char	*map;
    u_int	entries;
#if CONSOLE_REDESIGN
    int		err;
    static char not_init[] =
	    "p9100_ioctl: Console has not been initialized";
    static char kernel_only[] =
	    "p9100_ioctl: %s is a kernel only ioctl";
    static char kernel_only2[] =
	    "p9100_ioctl: %s and %s are kernel only ioctls";
#endif

    if (cmd != VIS_DEVINIT && cmd != VIS_DEVFINI &&
        cmd != VIS_CONSCOPY && cmd != VIS_STAND_CONSCOPY &&
        cmd != VIS_CONSDISPLAY && cmd != VIS_STAND_CONSDISPLAY &&
        cmd != VIS_CONSCURSOR && cmd != VIS_STAND_CONSCURSOR &&
        cmd != VIS_GETCMAP && cmd != VIS_PUTCMAP) {
	/*EMPTY*/
	DEBUGF(3, (CE_CONT, "p9100_ioctl(%d, 0x%x, 0x%x)\n",
	    getminor(dev), cmd, (int)data));
    }

    /* default to updating normal colormap */
    cursor_cmap = 0;

    switch (cmd) {
    case VIS_GETIDENTIFIER:
	if (ddi_copyout((caddr_t)&softc->p9100_ident,
			(caddr_t)data,
			sizeof (struct vis_identifier),
			mode))
	    return (EFAULT);
	break;

#if CONSOLE_REDESIGN
    case VIS_DEVINIT:
	if (!(mode & FKIOCTL)) {
	    cmn_err(CE_CONT, kernel_only, "VIS_DEVINIT");
	    return (ENXIO);
	}
	if ((err = p9100_devinit(dev, (struct vis_devinit *)data)) != 0) {
	    cmn_err(CE_CONT, "p9100ioctl: Could not initialize console");
	    return (err);
	}
	break;

    case VIS_DEVFINI:
	if (softc->conspriv == NULL) {
	    cmn_err(CE_CONT, not_init);
	    return (EFAULT);
	}

	if (!(mode & FKIOCTL)) {
	    cmn_err(CE_CONT, kernel_only, "VIS_DEVFINI");
	    return (ENXIO);
	}
	p9100_cons_free(softc->conspriv);
	softc->conspriv = NULL;

	break;

    case VIS_CONSCOPY:		/* copy */
    case VIS_STAND_CONSCOPY:	/* copy */
	if (softc->conspriv == NULL) {
	    cmn_err(CE_CONT, not_init);
	    return (EFAULT);
	}

	if (!(mode & FKIOCTL)) {
	    cmn_err(CE_CONT, kernel_only2,
		    "VIS_CONSCOPY", "VIS_STAND_CONSCOPY");
	    return (ENXIO);
	}

	(*softc->copy_func)(softc, (struct vis_conscopy *)data);
	break;

    case VIS_CONSDISPLAY:	/* display */
    case VIS_STAND_CONSDISPLAY:	/* display with no system services */
	if (softc->conspriv == NULL) {
	    cmn_err(CE_CONT, not_init);
	    return (EFAULT);
	}

	if (!(mode & FKIOCTL)) {
	    cmn_err(CE_CONT, kernel_only2,
		    "VIS_CONSDISPLAY", "VIS_STAND_CONSDISPLAY");
	    return (ENXIO);
	}

	(*softc->display_func)(softc, (struct vis_consdisplay *)data);
	break;

    case VIS_CONSCURSOR:	/* cursor */
    case VIS_STAND_CONSCURSOR:	/* cursor */
	if (softc->conspriv == NULL) {
	    cmn_err(CE_CONT, not_init);
	    return (EFAULT);
	}

	if (!(mode & FKIOCTL)) {
	    cmn_err(CE_CONT, kernel_only2,
		    "VIS_CONSCURSOR", "VIS_STAND_CONSCURSOR");
	    return (ENXIO);
	}

	(*softc->cursor_func)(softc, (struct vis_conscursor *)data);
	break;

    case VIS_CONS_MODE_CHANGE:
	p9100_get_hardware_settings(softc);
	break;

    case VIS_GETCMAP:
    case VIS_PUTCMAP:
    /* fall through */

#endif  /* CONSOLE_REDESIGN */

    case FBIOPUTCMAP:
    case FBIOGETCMAP: {

cmap_ioctl:

	if (ddi_copyin((caddr_t)data,
			(caddr_t)&cma,
			sizeof (struct fbcmap),
			mode)) {
	    return (EFAULT);
	}

	cmap = &cma;
	index = (u_int)cmap->index;
	count = (u_int)cmap->count;

	if (count == 0) {
	    return (0);
	}

	if (cursor_cmap == 0) {
	    map = softc->cmap[0];
	    entries = P9100_CMAP_ENTRIES;
	} else {
	    map = softc->omap[0];
	    entries = P9100_CURSOR_ENTRIES;
	}

	if (index >= entries ||
	    index + count > entries) {
	    return (EINVAL);
	}
	/*
	 * Allocate memory for color map RGB entries.
	 */
	stack_cmap = kmem_alloc((P9100_CMAP_ENTRIES * 3), KM_SLEEP);

	iobuf_cmap_red = stack_cmap;
	iobuf_cmap_green = stack_cmap + P9100_CMAP_ENTRIES;
	iobuf_cmap_blue = stack_cmap + (P9100_CMAP_ENTRIES * 2);

	if (cmd == FBIOPUTCMAP || cmd == VIS_PUTCMAP) {
	    int error;

	    DEBUGF(3, (CE_CONT, "FBIOPUTCMAP\n"));
	    if (error = ddi_copyin((caddr_t)cmap->red,
				    (caddr_t)iobuf_cmap_red,
				    count,
				    mode)) {
		kmem_free(stack_cmap, (P9100_CMAP_ENTRIES * 3));
		return (error);
	    }

	    if (error = ddi_copyin((caddr_t)cmap->green,
				    (caddr_t)iobuf_cmap_green,
				    count,
				    mode)) {
		kmem_free(stack_cmap, (P9100_CMAP_ENTRIES * 3));
		return (error);
	    }
	    if (error = ddi_copyin((caddr_t)cmap->blue,
				    (caddr_t)iobuf_cmap_blue,
				    count,
				    mode)) {
		kmem_free(stack_cmap, (P9100_CMAP_ENTRIES * 3));
		return (error);
	    }

	    mutex_enter(&softc->mutex);
	    map += index * 3;
	    if (p9100_update_pending(softc)) {
		mutex_enter(&(softc)->interlock);
		p9100_int_disable_intr(softc);
		mutex_exit(&(softc)->interlock);
	    }

	    /*
	     * Copy color map entries from stack to the color map
	     * table in the softc area.
	     */

	    p9100_cmap_copyin(map++, iobuf_cmap_red, count);
	    p9100_cmap_copyin(map++, iobuf_cmap_green, count);
	    p9100_cmap_copyin(map, iobuf_cmap_blue, count);

	    /* cursor colormap update */
	    if (entries < P9100_CMAP_ENTRIES)
		p9100_update_omap(softc);
	    else
		p9100_update_cmap(softc, index, count);
	    p9100_int_enable(softc);
	    mutex_exit(&softc->mutex);

	    } else {
		/* FBIOGETCMAP */
		DEBUGF(3, (CE_CONT, "FBIOGETCMAP\n"));

		mutex_enter(&softc->mutex);
		map += index * 3;

		/*
		 * Copy color map entries from soft area to
		 * local storage and prepare for a copyout
		 */

		p9100_cmap_copyout(iobuf_cmap_red, map++, count);
		p9100_cmap_copyout(iobuf_cmap_green, map++, count);
		p9100_cmap_copyout(iobuf_cmap_blue, map, count);

		mutex_exit(&softc->mutex);

		if (ddi_copyout((caddr_t)iobuf_cmap_red,
				(caddr_t)cmap->red,
				count,
				mode)) {
		    kmem_free(stack_cmap, (P9100_CMAP_ENTRIES * 3));
		    return (EFAULT);
		}
		if (ddi_copyout((caddr_t)iobuf_cmap_green,
				(caddr_t)cmap->green,
				count,
				mode)) {
		    kmem_free(stack_cmap, (P9100_CMAP_ENTRIES * 3));
		    return (EFAULT);
		}
		if (ddi_copyout((caddr_t)iobuf_cmap_blue,
				(caddr_t)cmap->blue,
				count,
				mode)) {
		    kmem_free(stack_cmap, (P9100_CMAP_ENTRIES * 3));
		    return (EFAULT);
		}
	    }
	    kmem_free(stack_cmap, (P9100_CMAP_ENTRIES * 3));
	    if (!softc->have_intr) {
		(void) p9100_intr((caddr_t)softc);
	    }
	}
	break;


	case FBIOSATTR: {
	    struct fbsattr	attr;

	    if (ddi_copyin((caddr_t)data,
			    (caddr_t)&attr,
			    sizeof (attr),
			    mode))
		return (EFAULT);
	    DEBUGF(3, (CE_CONT, "FBIOSATTR, type=%d\n", attr.emu_type));
	    if (attr.emu_type != -1)
		return (EINVAL);

	    /* ignore device-dependent stuff */
	}
	break;


	case FBIOGATTR: {
	    struct fbgattr	attr;

	    DEBUGF(3, (CE_CONT, "FBIOGATTR, emu_type=%d\n", softc->emulation));
	    bcopy((caddr_t)&p9100_attr, (caddr_t)&attr, sizeof (attr));
	    mutex_enter(&softc->mutex);
	    attr.fbtype.fb_type = softc->emulation;
	    attr.fbtype.fb_width = softc->w;
	    attr.fbtype.fb_height = softc->h;
	    attr.fbtype.fb_depth = softc->depth;
	    attr.fbtype.fb_size = softc->size;
	    attr.sattr.emu_type = softc->emulation;
	    mutex_exit(&softc->mutex);

	    if (ddi_copyout((caddr_t)&attr,
			    (caddr_t)data,
			    sizeof (struct fbgattr),
			    mode))
		return (EFAULT);
	}
	break;

	case FBIOGTYPE: {
	    struct fbtype	fb;

	    mutex_enter(&softc->mutex);
	    bcopy((caddr_t)&p9100_attr.fbtype, (caddr_t)&fb,
		    sizeof (struct fbtype));
	    DEBUGF(3, (CE_CONT, "FBIOGTYPE\n"));
	    fb.fb_width = softc->w;
	    fb.fb_height = softc->h;
	    fb.fb_depth = softc->depth;
	    fb.fb_size = softc->size;
	    mutex_exit(&softc->mutex);

	    if (ddi_copyout((caddr_t)&fb,
			    (caddr_t)data,
			    sizeof (struct fbtype),
			    mode))
		return (EFAULT);
	}
	break;

	case FBIOSVIDEO:
	    DEBUGF(3, (CE_CONT, "FBIOSVIDEO\n"));
	    mutex_enter(&softc->mutex);
	    p9100_set_video(softc, (int)data);
	    mutex_exit(&softc->mutex);
	break;

	case FBIOGVIDEO:
	    DEBUGF(3, (CE_CONT, "FBIOGVIDEO\n"));
	    mutex_enter(&softc->mutex);
	    if (softc->dac_type == VIPER_DAC_RGB525) {
		(void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(softc->p9100);
		softc->p9100 -> p9100_ramdac[RGB525_INDEX_HIGH] = 0;
		(void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(softc->p9100);
		softc->p9100 -> p9100_ramdac[RGB525_INDEX_LOW] =
		    RGB525_INDEX_MISCELLANEOUS_CONTROL_2;
		(void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(softc->p9100);
		index = softc->p9100 -> p9100_ramdac[RGB525_INDEX_DATA];

		i = (index & RGB525_MISC2_BLANKED)
		    ? FBVIDEO_OFF : FBVIDEO_ON;

	    } else
		i = (softc->p9100->p9100_ramdac[BT485_COMREG0] &
		    BT485_POWER_DOWN_ENABLE) ? FBVIDEO_OFF : FBVIDEO_ON;
	    mutex_exit(&softc->mutex);

	    if (ddi_copyout((caddr_t)&i,
			    (caddr_t)data,
			    sizeof (int),
			    mode))
		return (EFAULT);
	break;

#if ALWAYS_NO_INTERRUPTS
	/* vertical retrace interrupt */

	case FBIOVERTICAL:

	    if (softc->have_intr) {
		mutex_enter(&softc->mutex);
		softc->vrtflag |= P9100_VRTIOCTL;
		p9100_int_enable(softc);
		cv_wait(&softc->vrtsleep, &softc->mutex);
		mutex_exit(&softc->mutex);
	    }
	    else
		P9100_WAIT_VSYNC(softc->p9100);
	return (0);

	case FBIOVRTOFFSET:
	    i = P9100_VRT_VADDR;

	    if (ddi_copyout((caddr_t)&i,
			    (caddr_t)data,
			    sizeof (int),
			    mode))
		return (EFAULT);
	    return (0);

#endif /* ALWAYS_NO_INTERRUPTS */

	/* HW cursor control */
	case FBIOSCURSOR: {
	    int		set;
	    int		cbytes;
	    u_long	stack_image[32];
	    u_long	stack_mask[32];

	    if (ddi_copyin((caddr_t)data,
			    (caddr_t)&cp,
			    sizeof (struct fbcursor),
			    mode))
		return (EFAULT);

	    set = cp.set;
	    softc->cur.size = cp.size;

	    /* compute cursor bitmap bytes */
	    cbytes = softc->cur.size.y * sizeof (softc->cur.image[0]);
	    if (set & FB_CUR_SETSHAPE) {

		if ((u_int)cp.size.x > 32 || (u_int)cp.size.y > 32) {
		    return (EINVAL);
		}

		/* copy cursor image into softc */
		if (cp.image != NULL) {
		    if (ddi_copyin((caddr_t)cp.image,
				    (caddr_t)&stack_image,
				    cbytes,
				    mode))
			return (EFAULT);
		}
		if (cp.mask != NULL) {
		    if (ddi_copyin((caddr_t)cp.mask,
				    (caddr_t)&stack_mask,
				    cbytes,
				    mode))
			return (EFAULT);
		}
	    }

	    mutex_enter(&softc->mutex);
	    if (set & FB_CUR_SETCUR)
		softc->cur.enable = cp.enable;

	    if (set & FB_CUR_SETPOS)
		softc->cur.pos = cp.pos;

	    if (set & FB_CUR_SETHOT)
		softc->cur.hot = cp.hot;

	    /* update hardware */
	    (*softc->setcurpos)(softc);


	    if (set & FB_CUR_SETSHAPE) {

		if (cp.image != NULL) {
		    bzero((caddr_t)softc->cur.image, sizeof (softc->cur.image));
		    bcopy((caddr_t)&stack_image,
			    (caddr_t)softc->cur.image,
			    cbytes);
		}
		if (cp.mask != NULL) {
		    bzero((caddr_t)softc->cur.mask, sizeof (softc->cur.mask));
		    bcopy((caddr_t)&stack_mask,
			    (caddr_t)softc->cur.mask,
			    cbytes);
		}
		/* load into hardware */
		(*softc->setcurshape)(softc);
	    }
	    mutex_exit(&softc->mutex);

	    /* load colormap */
	    if (set & FB_CUR_SETCMAP) {
		cursor_cmap = 1;
		cmd = FBIOPUTCMAP;
		data = (int)&((struct fbcursor *)data) -> cmap;
		goto cmap_ioctl;
	    }
	    break;
	}

	case FBIOGCURSOR: {
	    int		cbytes;
	    u_long	stack_image[32];
	    u_long	stack_mask[32];

	    if (ddi_copyin((caddr_t)data,
			    (caddr_t)&cp,
			    sizeof (struct fbcursor),
			    mode))
		return (EFAULT);

	    mutex_enter(&softc->mutex);
	    cp.set = 0;
	    cp.enable = softc->cur.enable;
	    cp.pos = softc->cur.pos;
	    cp.hot = softc->cur.hot;
	    cp.size = softc->cur.size;
	    cp.cmap.index = 0;
	    cp.cmap.count = P9100_CURSOR_ENTRIES;

	    /* compute cursor bitmap bytes */
	    cbytes = softc->cur.size.y * sizeof (softc->cur.image[0]);

	    bcopy((caddr_t)softc->cur.image, (caddr_t)&stack_image, cbytes);
	    bcopy((caddr_t)softc->cur.mask, (caddr_t)&stack_mask, cbytes);

	    mutex_exit(&softc->mutex);

	    if (ddi_copyout((caddr_t)&cp,
			    (caddr_t)data,
			    sizeof (struct fbcursor),
			    mode))
		return (EFAULT);

	    /* if image pointer is non-null copy both bitmaps */
	    if (cp.image != NULL) {
		if (ddi_copyout((caddr_t)&stack_image,
				(caddr_t)cp.image,
				cbytes,
				mode))
		    return (EFAULT);
		if (ddi_copyout((caddr_t)&stack_mask,
				(caddr_t)cp.mask,
				cbytes,
				mode))
		    return (EFAULT);
	    }

	    /* if red pointer is non-null copy colormap */
	    if (cp.cmap.red) {
		cursor_cmap = 1;
		cmd = FBIOGETCMAP;
		data = (int)&((struct fbcursor *)data) -> cmap;
		goto cmap_ioctl;
	    }
	}
	break;

	case FBIOSCURPOS: {

	    struct fbcurpos	stack_curpos;   /* cursor position */

	    if (ddi_copyin((caddr_t)data,
			    (caddr_t)&stack_curpos,
			    sizeof (struct fbcurpos),
			    mode))
		return (EFAULT);

	    mutex_enter(&softc->mutex);
	    bcopy((caddr_t)&stack_curpos,
		    (caddr_t)&softc->cur.pos,
		    sizeof (struct fbcurpos));
	    (*softc->setcurpos)(softc);
	    mutex_exit(&softc->mutex);
	}
	break;

	case FBIOGCURPOS: {

	    struct fbcurpos	stack_curpos;   /* cursor position */

	    mutex_enter(&softc->mutex);
	    bcopy((caddr_t)&softc->cur.pos,
		    (caddr_t)&stack_curpos,
		    sizeof (struct fbcurpos));
	    mutex_exit(&softc->mutex);
	    if (ddi_copyout((caddr_t)&stack_curpos,
			    (caddr_t)data,
			    sizeof (struct fbcurpos),
			    mode))
		return (EFAULT);
	}
	break;

	case FBIOGCURMAX: {
	    static struct fbcurpos curmax = {32, 32};

	    if (ddi_copyout((caddr_t)&curmax,
			    (caddr_t)data,
			    sizeof (struct fbcurpos),
			    mode))
		return (EFAULT);
	}
	break;

#if P9100DEBUG >= 3
	case 253:
	    break;

	case 255:
	    p9100_debug = (int)data;
	    if (p9100_debug == -1)
		p9100_debug = P9100DEBUG;
	    cmn_err(CE_CONT, "p9100_debug is now %d\n", p9100_debug);
	    break;

#endif /* P9100DEBUG */

	default:
	    return (viper_ioctl(dev, cmd, (int)data, mode, cred, rval));
    }				/* switch(cmd) */
    return (0);
}


#if CONSOLE_REDESIGN

static int
p9100_devinit(dev_t dev, struct vis_devinit *data)
{
	struct p9100_softc		*softc;
	struct p9100_console_private	*private;
	int				unit;

	unit = getminor(dev);
	if (!(softc = getsoftc(unit)))
		return (NULL);

	private = (struct p9100_console_private *)
	    kmem_alloc(sizeof (struct p9100_console_private), KM_SLEEP);

	if (private == NULL)
		return (ENOMEM);

	/* initialize the private data structure */
	private->softc = softc;
	private->viscursor = 0;

	/* initialize console instance */
	data->version = VIS_CONS_REV;
	data->height = softc->h;
	data->width = softc->w;
	data->depth = softc->tem_depth;
	data->size = softc->h * softc->w;
	data->linebytes = softc->w;
	data->mode = (short)VIS_PIXEL;

	mutex_enter(&softc->mutex);
	softc->conspriv = private;
	mutex_exit(&softc->mutex);

	p9100_set_video(softc, FBVIDEO_ON);

	return (0);
}

/* The console routines, and may the Lord Have Mercy On Us All. */

static
void
clip(struct p9100_softc *softc, struct p9100_rect *rp)
{
	/* partial clip to screen */

	rp->h = (rp->y + rp->h > softc->h) ? softc->h - rp->y : rp->h;
	rp->w = (rp->x + rp->w > softc->w) ? softc->w - rp->x : rp->w;
}

static
void
p9100_cons_free(struct p9100_console_private *cp)
{
	kmem_free(cp, sizeof (struct p9100_console_private));
}

/*
 * The following are the "workhorse" routines for console terminal
 * emulation.
 *
 * They are as follows:
 *
 *   *copy*	Copy pixels from one place on the screen to another.
 *   *display*	Copy pixels from a memory buffer to the screen.
 *   *cursor*	Display or hide cursor by swapping foreground and background
 *
 * There are as many as three versions of each routine:
 *
 *   *_8	8-bit pixels
 *   *_32_0RGB	24-bit pixels stored as 32 bits, as 0, red, green, blue
 *   *_32_BGR0	24-bit pixels stored as 32 bits, as blue, green, red, 0
 *
 * Generally, the various versions are very similar, with only data type
 * differences.  Many display devices require only one or the other
 * byte order on 24-bit pixels.  This code is very nearly hardware and
 * driver independent, however, so it includes both variants with
 * unused ones #if'ed out.  This helps to facilitate keeping the
 * various drivers in sync.
 *
 *
 * The most interesting set of routines is the *copy* set, because they have
 * to avoid destroying data by copying in the wrong order but at the same
 * time are the most performance-critical routines in the terminal emulation
 * subsystem.  (Typically, you write fewer than 80 characters, then copy
 * about 2700 to scroll.)
 *
 * The *copy* routines have to deal with two major cases and about 4 or 5
 * minor cases.  The major cases are "forwards" and "backwards", referring
 * to the order in which the bytes are copied.  "Forwards" refers to copying
 * the low-address bytes first; "backwards" refers to copying the high-address
 * bytes first.  The caller will tell us which to do.  (I'm not sure why,
 * since we could just as easily compare the start positions.)
 *
 * Here's a diagram that may help to demonstrate the problem that requires
 * backwards copying:
 *
 *   +-----------------------+
 *   |                       |
 *   |     ////////          |
 *   |     ////////          |
 *   |     ////XXXX\\\\      |
 *   |     ////XXXX\\\\      |
 *   |         \\\\\\\\      |
 *   |         \\\\\\\\      |
 *   |                       |
 *   +-----------------------+
 *
 * The project is to copy the /// area to the \\\ area.  The XXX area is
 * where they overlap.
 *
 * If we straightforwardly start copying at the top left corner of the ///
 * area, we will be destroying the bottom right corner before copying it.
 * The solution is to start copying at the bottom right corner, so that
 * we copy stuff before overwriting it.
 *
 * But wait!  We have highly optimized routines for copying blocks of
 * memory forwards... but not for backwards.  They're probably possible,
 * but mind-bending.  For the case shown, it's only necessary that we
 * copy from bottom to top, not that we copy from right to left.  We can
 * start at the bottom left corner of the /// area and use "forward"
 * copies for each individual line, as long as we start at the bottom
 * and work to the top.
 *
 * The only case where we need to do each individual line backwards is
 * where we're copying horizontally:
 *
 *   +-----------------------+
 *   |                       |
 *   |                       |
 *   |     ////XXXX\\\\      |
 *   |     ////XXXX\\\\      |
 *   |     ////XXXX\\\\      |
 *   |     ////XXXX\\\\      |
 *   |                       |
 *   |                       |
 *   +-----------------------+
 *
 * Even for horizontal copies, we don't always need to go backwards:
 *
 *   +-----------------------+
 *   |                       |
 *   |                       |
 *   | //////// \\\\\\\\     |
 *   | //////// \\\\\\\\     |
 *   | //////// \\\\\\\\     |
 *   | //////// \\\\\\\\     |
 *   |                       |
 *   |                       |
 *   +-----------------------+
 *
 * We could do a similar optimization vertically, going forward when the
 * source and destination don't overlap vertically, but running the
 * vertical loop backwards is basically the same as running it forwards,
 * so there's no advantage.
 *
 */

/*
 * Here's where we pick which variations are needed for this particular
 * driver.
 */
#define	NEED_0RGB	1	/* 0RGB support is needed */
#define	NEED_BGR0	1	/* BGR0 support is needed */

/*
 * An 8-bit pixel.
 */
typedef unsigned char	pixel8;

/*
 * A 24-bit pixel trapped in a 32-bit body.  Format is dependent
 * on context.
 */
typedef unsigned long pixel32;

/*
 * Union for working with 24-bit pixels in 0RGB form, where the
 * bytes in memory are 0 (a pad byte), red, green, and blue in that order.
 *
 * Note that this is the format that "tem" uses, and so this definition
 * is needed even if the hardware uses only BGR0.
 */
union pixel32_0RGB {
	struct {
		char pad;
		char red;
		char green;
		char blue;
	} bytes;
	pixel32	pix;
};

/*
 * Union for working with 24-bit pixels in BGR0 form, where the
 * bytes in memory are blue, green, red, and 0 (a pad byte) in that order.
 */
union pixel32_BGR0 {
	struct {
		char blue;
		char green;
		char red;
		char pad;
	} bytes;
	pixel32	pix;
};

static void	p9100_cons_copy_8(struct p9100_softc *,
			struct vis_conscopy *);
static void	p9100_cons_display_8(struct p9100_softc *,
			struct vis_consdisplay *);
static void	p9100_cons_cursor_8(struct p9100_softc *,
			struct vis_conscursor *);
static void	p9100_bcopy_8(pixel8 *, pixel8 *, int);
static void	p9100_cons_copy_32(struct p9100_softc *,
			struct vis_conscopy *);
#if	NEED_0RGB
static void	p9100_cons_display_32_0RGB(struct p9100_softc *,
			struct vis_consdisplay *);
static void	p9100_cons_cursor_32_0RGB(struct p9100_softc *,
			struct vis_conscursor *);
#endif
#if	NEED_BGR0
static void	p9100_cons_display_32_BGR0(struct p9100_softc *,
			struct vis_consdisplay *);
static void	p9100_cons_cursor_32_BGR0(struct p9100_softc *,
			struct vis_conscursor *);
#endif
static void	p9100_bcopy_32(pixel32 *, pixel32 *, int);

static void
p9100_cons_copy_8(struct p9100_softc *softc, struct vis_conscopy *pma)
{
	register pixel8	*srcp;
	register pixel8	*dstp;
	register pixel8	*sp;
	register pixel8	*dp;
	register int		i;
	struct p9100_rect	r;

	r.x = pma->t_col;
	r.y = pma->t_row;
	r.w = pma->e_col - pma->s_col + 1;
	r.h = pma->e_row - pma->s_row + 1;

	clip(softc,  &r);

	switch (pma->direction) {
	case VIS_COPY_FORWARD:
		dstp = (pixel8 *)softc->p9100->p9100_frame_buffer +
			pma->t_row * softc->w + pma->t_col;
		srcp = (pixel8 *)softc->p9100->p9100_frame_buffer +
			pma->s_row * softc->w + pma->s_col;

		while (r.h--) {
		    p9100_bcopy_8(srcp, dstp, r.w);
		    dstp += softc->w;
		    srcp += softc->w;
		}
		break;

	case VIS_COPY_BACKWARD:
		dstp = (pixel8 *)softc->p9100->p9100_frame_buffer +
			(pma->t_row + r.h - 1) * softc->w + pma->t_col;
		srcp = (pixel8 *)softc->p9100->p9100_frame_buffer +
			pma->e_row * softc->w + pma->s_col;

		if (pma->s_row == pma->t_row &&
		    pma->t_col > pma->s_col &&
		    pma->t_col < pma->s_col + r.w) {
			while (r.h--) {
				sp = srcp + r.w - 1;
				dp = dstp + r.w - 1;
				for (i = 0; i < r.w; i++)
					*dp-- = *sp--;
				dstp -= softc->w;
				srcp -= softc->w;
			}
		} else {
			while (r.h--) {
				p9100_bcopy_8(srcp, dstp, r.w);
				dstp -= softc->w;
				srcp -= softc->w;
			}
		}
		break;
	}
}

static void
p9100_cons_display_8(struct p9100_softc *softc, struct vis_consdisplay *pda)
{
	register pixel8		*scrcp;
	register pixel8		*imgcp;
	struct p9100_rect	r;

	r.x = pda->col;
	r.y = pda->row;
	r.w = pda->width;
	r.h = pda->height;

	clip(softc,  &r);

	scrcp = (pixel8 *)softc->p9100->p9100_frame_buffer +
		r.y * softc->w + r.x;
	imgcp = (pixel8 *)pda->data;

	while (r.h--) {
		p9100_bcopy_8(imgcp, scrcp, r.w);
		imgcp += r.w;
		scrcp += softc->w;
	}
}

/*
 * Cursor display function for 8-bit pixels
 */
static void
p9100_cons_cursor_8(
	struct p9100_softc *softc,
	struct vis_conscursor *pca)
{
	register pixel8		*scrcp;
	register pixel8		*rp;
	register int		i;
	register pixel8		fg;
	register pixel8		bg;

	fg = pca->fg_color.eight;
	bg = pca->bg_color.eight;

	scrcp = (pixel8 *)softc->p9100->p9100_frame_buffer +
		pca->row * softc->w + pca->col;

	switch (pca->action) {
	case VIS_HIDE_CURSOR:
	    if (softc->conspriv->viscursor == 0)
		return;
	    softc->conspriv->viscursor = 0;
	    break;
	case VIS_DISPLAY_CURSOR:
	    if (softc->conspriv->viscursor)
		return;
	    softc->conspriv->viscursor = 1;
	    break;
	}

	for (i = pca->height; i-- > 0; ) {
	    for (rp = scrcp; rp < scrcp + pca->width; rp++)
		    if (*rp == fg)
			*rp = bg;
		    else if (*rp == bg)
			*rp = fg;
	    scrcp += softc->w;
	}
}

/*
 * 32-bit screen-to-screen copy.  Byte order doesn't matter here,
 * because we're always copying 32 bits screen-to-screen.
 */
static void
p9100_cons_copy_32(struct p9100_softc *softc, struct vis_conscopy *pma)
{
	register pixel32	*srcp;
	register pixel32	*dstp;
	register pixel32	*sp;
	register pixel32	*dp;
	register int		i;
	struct p9100_rect	r;

	r.x = pma->t_col;
	r.y = pma->t_row;
	r.w = pma->e_col - pma->s_col + 1;
	r.h = pma->e_row - pma->s_row + 1;

	clip(softc,  &r);

	switch (pma->direction) {
	case VIS_COPY_FORWARD:
		dstp = (pixel32 *)softc->p9100->p9100_frame_buffer +
			pma->t_row * softc->w + pma->t_col;
		srcp = (pixel32 *)softc->p9100->p9100_frame_buffer +
			pma->s_row * softc->w + pma->s_col;

		while (r.h--) {
		    p9100_bcopy_32(srcp, dstp, r.w);
		    dstp += softc->w;
		    srcp += softc->w;
		}
		break;

	case VIS_COPY_BACKWARD:
		dstp = (pixel32 *)softc->p9100->p9100_frame_buffer +
			(pma->t_row + r.h - 1) * softc->w + pma->t_col;
		srcp = (pixel32 *)softc->p9100->p9100_frame_buffer +
			pma->e_row * softc->w + pma->s_col;

		if (pma->s_row == pma->t_row &&
		    pma->t_col > pma->s_col &&
		    pma->t_col < pma->s_col + r.w) {
			while (r.h--) {
				sp = srcp + r.w - 1;
				dp = dstp + r.w - 1;
				for (i = 0; i < r.w; i++)
					*dp-- = *sp--;
				dstp -= softc->w;
				srcp -= softc->w;
			}
		} else {
			while (r.h--) {
				p9100_bcopy_32(srcp, dstp, r.w);
				dstp -= softc->w;
				srcp -= softc->w;
			}
		}
		break;
	}
}

#if	NEED_0RGB
/*
 * Memory-to-screen copy in 0RGB form.
 * 0RGB is "tem" standard representation for 24-bit pixels, so
 * no swapping is needed.  We just move the 32-bit values unmodified.
 */
static void
p9100_cons_display_32_0RGB(
	struct p9100_softc *softc,
	struct vis_consdisplay *pda)
{
	register pixel32	*scrcp;
	register pixel32	*imgcp;
	struct p9100_rect	r;

	r.x = pda->col;
	r.y = pda->row;
	r.w = pda->width;
	r.h = pda->height;

	clip(softc,  &r);

	scrcp = (pixel32 *)softc->p9100->p9100_frame_buffer +
		r.y * softc->w + r.x;
	imgcp = (pixel32 *)pda->data;

	while (r.h--) {
		p9100_bcopy_32(imgcp, scrcp, r.w);
		imgcp += r.w;
		scrcp += softc->w;
	}
}

/*
 * Cursor display function in 0RGB form.
 * Mostly, we don't need to care about byte order.  All we need to do
 * is to construct hardware-format pixel values for the foreground and
 * background, and use them in the loop.
 *
 * This means that the only difference between this routine and
 * *_cursor_32_BGR0 is which union gets used to construct the "test"
 * pixel values.
 */
static void
p9100_cons_cursor_32_0RGB(
	struct p9100_softc *softc,
	struct vis_conscursor *pca)
{
	register pixel32	*scrcp;
	register pixel32	*rp;
	register int		i;
	register pixel32	fg;
	register pixel32	bg;
	union pixel32_0RGB	xfer;

	/*
	 * The union takes care of byte order.
	 */
	xfer.bytes.pad = 0;
	xfer.bytes.red = pca->fg_color.twentyfour[0];
	xfer.bytes.green = pca->fg_color.twentyfour[1];
	xfer.bytes.blue = pca->fg_color.twentyfour[2];
	fg = xfer.pix;

	xfer.bytes.red = pca->bg_color.twentyfour[0];
	xfer.bytes.green = pca->bg_color.twentyfour[1];
	xfer.bytes.blue = pca->bg_color.twentyfour[2];
	bg = xfer.pix;

	scrcp = (pixel32 *)softc->p9100->p9100_frame_buffer +
		pca->row * softc->w + pca->col;

	switch (pca->action) {
	case VIS_HIDE_CURSOR:
	    if (softc->conspriv->viscursor == 0)
		return;
	    softc->conspriv->viscursor = 0;
	    break;
	case VIS_DISPLAY_CURSOR:
	    if (softc->conspriv->viscursor)
		return;
	    softc->conspriv->viscursor = 1;
	    break;
	}

	for (i = pca->height; i-- > 0; ) {
	    for (rp = scrcp; rp < scrcp + pca->width; rp++)
		    if (*rp == fg)
			*rp = bg;
		    else if (*rp == bg)
			*rp = fg;
	    scrcp += softc->w;
	}
}
#endif

#if	NEED_BGR0
/*
 * Memory-to-screen copy in BGR0 form.
 * 0RGB is "tem" standard representation for 24-bit pixels, so
 * we need to swap the bytes.  This could probably be improved
 * by recoding in assembler to take advantage of the byte-swap
 * load/store instructions, and perhaps to take advantage of
 * the register-to-register bit instructions.
 */
static void
p9100_cons_display_32_BGR0(
	struct p9100_softc *softc,
	struct vis_consdisplay *pda)
{
	register pixel32	*scr_row_ptr;
	register pixel32	*img_row_ptr;
	register pixel32	*scr_ptr;
	register pixel32	*img_ptr;
	struct p9100_rect	r;
	union pixel32_BGR0	scr_pix;
	union pixel32_0RGB	img_pix;
	register int		i;

	r.x = pda->col;
	r.y = pda->row;
	r.w = pda->width;
	r.h = pda->height;

	clip(softc,  &r);

	scr_row_ptr = (pixel32 *)softc->p9100->p9100_frame_buffer +
		r.y * softc->w + r.x;
	img_row_ptr = (pixel32 *)pda->data;

	scr_pix.bytes.pad = 0;

	while (r.h--) {
		img_ptr = img_row_ptr;
		scr_ptr = scr_row_ptr;
		for (i = r.w; i > 0; i--) {
			/*
			 * The two unions take care of byte order.
			 * Note that they are *not* the same union.
			 */
			img_pix.pix = *img_ptr++;
			scr_pix.bytes.red   = img_pix.bytes.red;
			scr_pix.bytes.green = img_pix.bytes.green;
			scr_pix.bytes.blue  = img_pix.bytes.blue;
			*scr_ptr++ = scr_pix.pix;
		}
		img_row_ptr += r.w;
		scr_row_ptr += softc->w;
	}
}

/*
 * Cursor display function in BGR0 form.
 * Mostly, we don't need to care about byte order.  All we need to do
 * is to construct hardware-format pixel values for the foreground and
 * background, and use them in the loop.
 *
 * This means that the only difference between this routine and
 * *_cursor_32_RGB0 is which union gets used to construct the "test"
 * pixel values.
 */
static void
p9100_cons_cursor_32_BGR0(
	struct p9100_softc *softc,
	struct vis_conscursor *pca)
{
	register pixel32	*scrcp;
	register pixel32	*rp;
	register int		i;
	register pixel32	fg;
	register pixel32	bg;
	union pixel32_BGR0	xfer;

	/*
	 * The union takes care of byte order.
	 */
	xfer.bytes.pad = 0;
	xfer.bytes.red = pca->fg_color.twentyfour[0];
	xfer.bytes.green = pca->fg_color.twentyfour[1];
	xfer.bytes.blue = pca->fg_color.twentyfour[2];
	fg = xfer.pix;

	xfer.bytes.red = pca->bg_color.twentyfour[0];
	xfer.bytes.green = pca->bg_color.twentyfour[1];
	xfer.bytes.blue = pca->bg_color.twentyfour[2];
	bg = xfer.pix;

	scrcp = (pixel32 *)softc->p9100->p9100_frame_buffer +
		pca->row * softc->w + pca->col;

	switch (pca->action) {
	case VIS_HIDE_CURSOR:
	    if (softc->conspriv->viscursor == 0)
		return;
	    softc->conspriv->viscursor = 0;
	    break;
	case VIS_DISPLAY_CURSOR:
	    if (softc->conspriv->viscursor)
		return;
	    softc->conspriv->viscursor = 1;
	    break;
	}

	for (i = pca->height; i-- > 0; ) {
	    for (rp = scrcp; rp < scrcp + pca->width; rp++)
		    if (*rp == fg)
			*rp = bg;
		    else if (*rp == bg)
			*rp = fg;
	    scrcp += softc->w;
	}
}
#endif

/* XXXppc:  For unknown reasons, the stock bcopy faults when called from */
/* inside kadb.  Or something like that. */
static void
p9100_bcopy_8(pixel8 *from, pixel8 *to, int count)
{
	unsigned long *to4;
	unsigned long *from4;

	/*
	 * If the two buffers aren't similarly aligned, or we're only
	 * copying a few bytes, do it the slow stupid way.
	 */
	if (((unsigned long)from & 3) != ((unsigned long)to & 3) ||
	    count < 3+4+3) {
		while (count-- > 0)
			*to++ = *from++;
		return;
	}

	/*
	 * Copy up to three bytes to get aligned.
	 */
	while ((unsigned long)from & 3) {
		*to++ = *from++;
		count--;
	}

	/*
	 * Now shift into high gear to copy the bulk of the data, 4 bytes
	 * at a time.
	 */
	to4 = (unsigned long *)to;
	from4 = (unsigned long *)from;
	while (count > 3) {
		*to4++ = *from4++;
		count -= 4;
	}

	/*
	 * Now finish off the final up to three bytes one byte at a time.
	 */
	to = (pixel8 *)to4;
	from = (pixel8 *)from4;
	while (count-- > 0)
		*to++ = *from++;
}

/*
 * This bcopy-like function operates on data known to be 32-bit aligned,
 * sized, etc.
 */
static void
p9100_bcopy_32(pixel32 *from, pixel32 *to, int count)
{
	while (count-- > 0) {
		*to++ = *from++;
	}
}

/*
 * And that's the end of the terminal emulation "workhorse" routines.
 * Hope you had fun.
 */
#endif /* CONSOLE_REDESIGN */

static  u_int
p9100_intr(caddr_t arg)
{
    register struct p9100_softc *const softc = (struct p9100_softc *)arg;

    DEBUGF(7, (CE_CONT,
		"p9100_intr: softc=%x, vrtflag=%x\n", softc, softc->vrtflag));

    mutex_enter(&softc->mutex);
    mutex_enter(&softc->interlock);

    if (!(p9100_update_pending(softc) || (softc)->vrtflag)) {

	/* TODO catch stray interrupts? */
	p9100_int_disable_intr(softc);
	p9100_int_clear(softc);
	mutex_exit(&softc->interlock);
	mutex_exit(&softc->mutex);

	return (DDI_INTR_CLAIMED);
    }

#if ALWAYS_NO_INTERRUPTS

    if (softc->vrtflag & P9100_VRTCTR) {
	if (softc->vrtmaps == 0) {
	    softc->vrtflag &= ~P9100_VRTCTR;
	}
	else
	    *softc->vrtpage += 1;
    }

    if (softc->vrtflag & P9100_VRTIOCTL) {
	softc->vrtflag &= ~P9100_VRTIOCTL;
	cv_broadcast(&softc->vrtsleep);
    }

#endif /* ALWAYS_NO_INTERRUPTS */

    if (p9100_update_pending(softc)) {

	/* load cursor color map */
	if (softc->omap_update) {
	    (*softc->setcurcolor)(softc);
	    softc->omap_update = 0;
	}

	/* load main color map */
	if (softc->cmap_count) {
	    (*softc->setcmap)(softc);
	    softc->cmap_count = 0;
	}
    }

    p9100_int_disable_intr(softc);
    p9100_int_clear(softc);

    if (softc->vrtflag)
	p9100_int_enable(softc);

#ifdef	COMMENT

    if (!softc->vrtflag)
	p9100_int_disable_intr(softc);

#endif	/* COMMENT */

    mutex_exit(&softc->interlock);
    mutex_exit(&softc->mutex);

    return (DDI_INTR_CLAIMED);
}

/*
 * Initialize a colormap: background = white, all others = black
 */
static void
p9100_reset_cmap(register u_char *const cmap, register u_int const entries)
{
    bzero((char *)cmap, entries * 3);
    cmap[0] = 255;
    cmap[1] = 255;
    cmap[2] = 255;
}

/*
 * Compute color map update parameters: starting index and count.
 * If count is already nonzero, adjust values as necessary.
 * Zero count argument indicates cursor color map update desired.
 */
static void
p9100_update_cmap(register struct p9100_softc *softc, register u_int index,
	register u_int count)
{
    register u_int	high, low;

    if (count == 0) {
	softc->omap_update = 1;
	return;
    }

    high = softc->cmap_count;

    if (high != 0) {
	high += (low = softc->cmap_index);

	if (index < low)
	    softc->cmap_index = low = index;

	if (index + count > high)
	    high = index + count;

	softc->cmap_count = high - low;
    } else {
	softc->cmap_index = index;
	softc->cmap_count = count;
    }
}


static void
p9100_update_omap(register struct p9100_softc *const softc)
{
    softc->omap_update = 1;
}

/*
 * Copy colormap entries between red, green, or blue array to
 * interspersed rgb array.
 */
static  void
p9100_cmap_copyin(register u_char *rgb, register u_char *buf,
	register u_int count)
{
    while (count != 0) {
	*rgb = *buf;
	rgb += 3;
	buf++;
	count--;
    }
}

/*
 * Copy colormap entries between interspersed rgb array to
 * red, green, or blue array.
 */
static  void
p9100_cmap_copyout(register u_char *buf, register u_char *rgb,
	register u_int count)
{
    while (count != 0) {
	*buf = *rgb;
	rgb += 3;
	buf++;
	count--;
    }
}

/*ARGSUSED*/
static void
p9100_reset(register struct p9100_softc *const softc)
{
}

/*
 * P9000/P9100 Context Management
 *
 * This virtualizes the P9000/P9100 register file by associating a register
 * save area with each mapping of the device and using the ddi_mapdev
 * mechanism to allow us to detect mapping changes.
 * Only one of these contexts is valid at any time; an access to one
 * of the invalid contexts saves the current P9000/P9100 context, invalidates
 * the current valid mapping, restores the former register contents
 * appropriate to the faulting mapping, and then validates it.
 *
 * This implements a graphical context switch that is transparent to
 * the user.
 *
 * the REGS contain the interesting context registers.
 *
 * Per-mapping info:
 *	Some, but not all, mappings are part of a context.
 *      Any mapping that is MAP_PRIVATE to the REGS will be
 *	part of a unique context.  MAP_SHARED mappings are part
 *	of the shared context and all such programs must arbitrate among
 *	themselves to keep from stepping on each other's register settings.
 *	Mappings to the framebuffer may or may not be part of a context,
 *	depending on exact hardware type.
 */

#define	P9100_MAP_SHARED	0x02    /* shared context */
#define	P9100_MAP_LOCK		0x04    /* lock page */
#if ALWAYS_NO_INTERRUPTS
#define	P9100_MAP_VRT		0x08    /* vrt page */
#endif
#define	P9100_MAP_REGS		0x10    /* mapping includes registers */
#define	P9100_MAP_FB		0x40    /* mapping includes framebuffer */

#define	P9100_MAP_CTX	(P9100_MAP_REGS | P9100_MAP_FB) /* needs context */

static struct p9100_map_pvt *
p9100_pvt_alloc(register struct p9100_cntxt *const ctx,
	register u_int const off, register u_int const len,
	register struct p9100_softc *const softc)
{
    register struct p9100_map_pvt	*pvt;

    /*
     * create the private data portion of the mapdev object
     */
    pvt = (struct p9100_map_pvt *)kmem_zalloc(sizeof (struct p9100_map_pvt),
						KM_SLEEP);
    pvt->offset = off;
    pvt->len = len;
    pvt->context = ctx;
    pvt->softc = softc;

    /*
     * Link this pvt into the list of associated pvt's for this
     * context
     */
    pvt->next = ctx->p9100_ctx_pvt;
    ctx->p9100_ctx_pvt = pvt;

    return (pvt);
}

/*
 * This routine is called through the cb_ops table to handle
 * p9100 mmap requests.
 */
/* ARGSUSED */
static int
p9100_segmap(register dev_t const dev, register off_t const off,
	register struct as *const as, register caddr_t *const addrp,
	register off_t const len, register u_int const prot,
	register u_int const maxprot, register u_int flags,
	register cred_t *const cred)
{
    register struct p9100_softc *const	softc = getsoftc(getminor(dev));
    register struct p9100_cntxt	*ctx  = (struct p9100_cntxt *)NULL;
    register struct p9100_cntxt *const	shared_ctx = &softc->shared_ctx;
    register struct p9100_map_pvt	*pvt;
    register u_int	maptype = 0;
    register int	error;
    register int	i;

    DEBUGF(3, (CE_CONT, "segmap: off=%x, len=%x\n", off, len));

    mutex_enter(&softc->mutex);

    /*
     * Validate and categorize the map request.  Valid mmaps to
     * the p9100 driver are to device hardware or to the vertical
     * retrace counter page
     *
     * VRT page mapping?  If so, be sure to count VRT events
     */
#if ALWAYS_NO_INTERRUPTS
    if (off == P9100_VRT_VADDR) {
	if (!softc->have_intr)
	    return (EIO);

	if (len != p9100_pagesize) {
	    mutex_exit(&softc->mutex);
	    DEBUGF(3, (CE_CONT,
			"rejecting because off=vrt and len=%x\n", len));
	    return (EINVAL);
	}

	maptype = P9100_MAP_VRT;

	if (softc->vrtmaps++ == 0) {
	    if (softc->vrtpage == NULL)
		softc->vrtalloc = (int *)kmem_alloc(p9100_pagesize * 2,
						    KM_SLEEP);
	    softc->vrtpage = (int *)
				(((ulong_t)softc->vrtalloc + p9100_pagesize) &
				~p9100_pageoffset);
	    *softc->vrtpage = 0;
	    softc->vrtflag |= P9100_VRTCTR;
	    p9100_int_enable(softc);
	}
    }

    else {

#else /* ALWAYS_NO_INTERRUPTS */

    {

#endif /* ALWAYS_NO_INTERRUPTS */

	/*
	 * Check to insure that the entire range is legal and we are
	 * not trying to map more than defined by the device.
	 */
	for (i = 0; i < len; i += p9100_pagesize) {
	    if (p9100_mmap(dev, (off_t)off + i, (int)maxprot) == -1) {
		mutex_exit(&softc->mutex);
		DEBUGF(3, (CE_CONT,
			    "rejecting because mmap returns -1, off=%x\n",
			    off+i));
		return (ENXIO);
	    }
	}

	/* classify it */
	if (off + len > P9100_REGISTER_BASE &&
	    off < P9100_REGISTER_BASE + P9100_REGISTER_SIZE) {
	    maptype |= P9100_MAP_CTX;
	}
    }


    /*
     * Is this mapping for part of the p9100 context?
     * If so, splice it into the softc context list.
     */
    if (maptype & P9100_MAP_CTX) {

	/*
	 * Is this a shared mapping.  If so use the shared_ctx.
	 */
	if (flags & MAP_SHARED) {
	    ctx = shared_ctx;
	    ctx->p9100_ctx_flag = P9100_MAP_CTX;   /* XXX: move to attach */
	} else {
	    ctx = p9100_ctx_map_insert(softc, maptype);
	    ctx->p9100_ctx_flag |= maptype;
	}

	pvt = p9100_pvt_alloc(ctx, 0, (u_int)len, softc);

	/*
	 * create the mapdev object
	 */
	error = ddi_mapdev(dev, off, as, addrp, len, prot, maxprot,
		flags, cred, &p9100_map_ops, &pvt->handle,
		(void *)pvt);

	if (error != 0) {
	    cmn_err(CE_WARN, "ddi_mapdev failure\n");
	}

    } else {

	/*
	 * Yet another kludge: some programs may decide to map random
	 * parts of the device MAP_PRIVATE, even those that don't form
	 * part of the context. This has the effect of potentially asking
	 * ddi_segmap (aka spec_segmap) to create a MAP_PRIVATE mapping,
	 * which it will not do. We turn all gratuitous private mappings
	 * into shared ones here.
	 */

	if (flags & MAP_PRIVATE)
	flags = (flags & ~MAP_TYPE) | MAP_SHARED;

	error = ddi_segmap(dev, off, as, addrp, len, prot,
		maxprot, flags, cred);
    }
    mutex_exit(&softc->mutex);
    return (error);
}

/*
 * An access has been made to a context other than the current one
 */
/*ARGSUSED*/
static int
p9100_map_access(register ddi_mapdev_handle_t const handle,
	register void *const pvt,
	register off_t const offset)
{
    register struct p9100_map_pvt *const	p = (struct p9100_map_pvt *)pvt;
    register struct p9100_map_pvt	*pvts;
    register struct p9100_softc *const	softc = p->softc;
    register p9100p_t	const p9100reg = softc->p9100;
    register int	err = 0;

    ASSERT(pvt);

    mutex_enter(&softc->mutex);

    /*
     * Do we need to switch contexts?
     */
    if (softc->curctx != p->context) {

	/*
	 * If there's a current context, save it
	 */
	if (softc->curctx != (struct p9100_cntxt *)NULL) {
	    /*
	     * Set mapdev for current context and all associated handles
	     * to intercept references to their addresses
	     */
	    ASSERT(softc->curctx->p9100_ctx_pvt);
	    for (pvts = softc->curctx->p9100_ctx_pvt; pvts != NULL;
		    pvts = pvts->next) {
		err = ddi_mapdev_intercept(pvts->handle,
		pvts->offset, pvts->len);
		if (err)
		    return (err);
	    }

	    if (p9100_cntxsave(p9100reg, softc->curctx) == 0) {
		DEBUGF(1, (CE_CONT, "p9100: context save failed\n"));
		/*
		 * At this point we have no current context.
		 */
		softc->curctx = NULL;
		mutex_exit(&softc->mutex);
		    return (-1);
	    }
	}

	/*
	 * Idle the chips
	 */
	CDELAY(!(p9100reg->p9100_status & P9100_STATUS_QUAD_OR_BUSY),
		P9100_WAIT);

	if (p9100reg->p9100_status & P9100_STATUS_QUAD_OR_BUSY) {
	    DEBUGF(1, (CE_CONT, "p9100: idle_p9100: status = %x\n",
			p9100reg->p9100_status));

	    /*
	     * At this point we have no current context.
	     */
	    softc->curctx = NULL;
	    mutex_exit(&softc->mutex);
		return (-1);
	}

	DEBUGF(4, (CE_CONT, "loading context %x\n", p->context));

	if (p->context->p9100_ctx_flag & P9100_MAP_REGS)
	    if (!p9100_cntxrestore(p9100reg, p->context)) {
		DEBUGF(1, (CE_CONT, "p9100: context restore failed\n"));
	    /*
	     * At this point we have no current context.
	     */
	    softc->curctx = NULL;
	    mutex_exit(&softc->mutex);
		return (-1);
	}

	/*
	 * switch software "context"
	 */
	softc->curctx = p->context;
    }

    ASSERT(p->context->p9100_ctx_pvt);

    for (pvts = p->context->p9100_ctx_pvt; pvts != NULL; pvts = pvts->next) {
	if ((err = ddi_mapdev_nointercept(pvts->handle,
					    pvts->offset, pvts->len)) != 0) {
	    mutex_exit(&softc->mutex);
	    return (err);
	}
    }

    mutex_exit(&softc->mutex);
    return (err);
}


/*ARGSUSED*/
static void
p9100_map_free(register ddi_mapdev_handle_t const handle,
	register void *const pvt)
{
    register struct p9100_map_pvt *const	p = (struct p9100_map_pvt *)pvt;
    register struct p9100_map_pvt	*pvts;
    register struct p9100_map_pvt	*ppvts;
    register struct p9100_cntxt *const	ctx = p->context;
    register struct p9100_softc *const	softc = p->softc;
    register struct p9100_cntxt *const	shared_ctx = &softc->shared_ctx;

    mutex_enter(&softc->mutex);

    DEBUGF(4, (CE_CONT, "p9100_map_free: cleaning up pid %d\n", ctx->pid));

    /*
     * Remove the pvt data
     */
    ppvts = NULL;
    for (pvts = ctx->p9100_ctx_pvt; pvts != NULL; pvts = pvts->next) {
	if (pvts == pvt) {
	    if (ppvts == NULL) {
		ctx->p9100_ctx_pvt = pvts->next;
	    } else {
		ppvts->next = pvts->next;
	    }
	    kmem_free(pvt, sizeof (struct p9100_map_pvt));
	    break;
	}
	ppvts = pvt;
    }

    /*
     * Remove the context if this is not the shared context and there are
     * no more associated pvt's
     */
    if ((ctx != shared_ctx) && (ctx->p9100_ctx_pvt == NULL)) {
	register struct p9100_cntxt *ctxptr;

	if (ctx == softc->curctx)
	    softc->curctx = NULL;

	/*
	 * Scan private context list for entry to remove.
	 * Check first to see if it's the head of our list.
	 */
	if (softc->pvt_ctx == ctx) {
	    softc->pvt_ctx = ctx->link;
	    kmem_free(ctx, sizeof (struct p9100_cntxt));
	} else {
	    for (ctxptr = softc->pvt_ctx; ctxptr != NULL;
		    ctxptr = ctxptr->link) {
		if (ctxptr->link == ctx) {
		    ctxptr->link = ctx->link;
		    kmem_free(ctx, sizeof (struct p9100_cntxt));
		}
	    }
	}
    }

    /*
     * If the curctx is the shared context, and there are no
     * more pvt's for the shared context, set the curctx to
     * NULL to force a context switch on the next device access.
     */
    if ((softc->curctx == shared_ctx) &&
	    (softc->curctx->p9100_ctx_pvt == NULL)) {
	softc->curctx = NULL;
    }

    mutex_exit(&softc->mutex);
}


/*ARGSUSED*/
static int
p9100_map_dup(register ddi_mapdev_handle_t const old_handle,
	register void *const oldpvt, register ddi_mapdev_handle_t new_handle,
	register void **const newpvt)
{
    register struct p9100_map_pvt *const p = (struct p9100_map_pvt *)oldpvt;
    register struct p9100_softc *const	softc = p->softc;
    register struct p9100_map_pvt	*pvt;
    register struct p9100_cntxt		*ctx;

    mutex_enter(&softc->mutex);
    if (p->context != &softc->shared_ctx) {
	ctx = (struct p9100_cntxt *)
	    kmem_zalloc(sizeof (struct p9100_cntxt), KM_SLEEP);

	*ctx = *p->context;
	ctx->p9100_ctx_pvt = NULL;
    }
    else
	ctx = &softc->shared_ctx;

    pvt = p9100_pvt_alloc(ctx, 0, p->len, softc);

    pvt->handle = new_handle;
    *newpvt = pvt;

#if ALWAYS_NO_INTERRUPTS

    if (p->context && (p->context->p9100_ctx_flag & P9100_MAP_VRT)) {
	softc->vrtflag |= P9100_VRTCTR;
	if (softc->vrtmaps == 0)
	    p9100_int_enable(softc);
	softc->vrtmaps++;
    }

#endif /* ALWAYS_NO_INTERRUPTS */

    mutex_exit(&softc->mutex);
    return (0);
}

static void
p9100_set_video(struct p9100_softc *softc, int v)
{
    register p9100p_t p9100ptr = softc->p9100;
    int i;

    if (softc->dac_type != VIPER_DAC_RGB525) {
	if (v & FBVIDEO_ON)
	    softc->p9100->p9100_ramdac[6] =
		softc->p9100->p9100_ramdac[6] & ~
		BT485_POWER_DOWN_ENABLE;
	else
	    softc->p9100->p9100_ramdac[6] =
		softc->p9100->p9100_ramdac[6] |
		BT485_POWER_DOWN_ENABLE;
    } else {
	(void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
	p9100ptr -> p9100_ramdac[RGB525_INDEX_HIGH] = 0;
	(void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
	p9100ptr -> p9100_ramdac[RGB525_INDEX_LOW] =
	    RGB525_INDEX_MISCELLANEOUS_CONTROL_2;
	(void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
	i = p9100ptr -> p9100_ramdac[RGB525_INDEX_DATA];

	if (v & FBVIDEO_ON)
	    i &= ~RGB525_MISC2_BLANKED;
	else
	    i |= RGB525_MISC2_BLANKED;

	(void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
	p9100ptr -> p9100_ramdac[RGB525_INDEX_LOW] =
	    RGB525_INDEX_MISCELLANEOUS_CONTROL_2;
	(void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
	p9100ptr -> p9100_ramdac[RGB525_INDEX_DATA] = i;
    }
}

static int
p9100_cntxsave(register p9100p_t p9100reg, struct p9100_cntxt *const saved)
{
    DEBUGF(5, (CE_CONT, "saving registers\n"));

    CDELAY(!(p9100reg->p9100_status & P9100_STATUS_QUAD_OR_BUSY), P9100_WAIT);
    if (p9100reg->p9100_status & P9100_STATUS_QUAD_OR_BUSY) {
	DEBUGF(1, (CE_CONT,
		"p9100: cntxsave: status = %x\n", p9100reg->p9100_status));
	return (0);
    }

    /*
     * start dumping stuff out.
     */

#if ALWAYS_NO_INTERRUPTS
    saved->p9100_ctx_interrupt = p9100reg->p9100_interrupt;
    saved->p9100_ctx_interrupt_en = p9100reg->p9100_interrupt_en;
#endif
    saved->p9100_ctx_cindex = p9100reg->p9100_cindex;
    saved->p9100_ctx_w_off_xy = p9100reg->p9100_w_off_xy;
    saved->p9100_ctx_fground = p9100reg->p9100_fground;
    saved->p9100_ctx_bground = p9100reg->p9100_bground;
    saved->p9100_ctx_color2 = p9100reg->p9100_color2;
    saved->p9100_ctx_color3 = p9100reg->p9100_color3;
    saved->p9100_ctx_pmask = p9100reg->p9100_pmask;
    saved->p9100_ctx_draw_mode = p9100reg->p9100_draw_mode;
    saved->p9100_ctx_pat_originx = p9100reg->p9100_pat_originx;
    saved->p9100_ctx_pat_originy = p9100reg->p9100_pat_originy;
    saved->p9100_ctx_raster = p9100reg->p9100_raster;
    saved->p9100_ctx_pixel8_reg = p9100reg->p9100_pixel8_reg;
    saved->p9100_ctx_w_min = p9100reg->p9100_pe_w_min;
    saved->p9100_ctx_w_max = p9100reg->p9100_pe_w_max;
    saved->p9100_ctx_b_w_min = p9100reg->p9100_b_w_min;
    saved->p9100_ctx_b_w_max = p9100reg->p9100_b_w_max;
    P9100_PAT_COPY(saved->p9100_ctx_pattern, p9100reg->p9100_pattern);
    saved->p9100_ctx_x0 = p9100reg->p9100_x0;
    saved->p9100_ctx_y0 = p9100reg->p9100_y0;
    saved->p9100_ctx_x1 = p9100reg->p9100_x1;
    saved->p9100_ctx_y1 = p9100reg->p9100_y1;
    saved->p9100_ctx_x2 = p9100reg->p9100_x2;
    saved->p9100_ctx_y2 = p9100reg->p9100_y2;
    saved->p9100_ctx_x3 = p9100reg->p9100_x3;
    saved->p9100_ctx_y3 = p9100reg->p9100_y3;
    return (1);
}

static int
p9100_cntxrestore(register p9100p_t const p9100reg,
	register struct p9100_cntxt *const saved)
{
    DEBUGF(5, (CE_CONT, "restoring registers\n"));

    CDELAY(!(p9100reg->p9100_status & P9100_STATUS_QUAD_OR_BUSY), P9100_WAIT);
    if (p9100reg->p9100_status & P9100_STATUS_QUAD_OR_BUSY) {
	DEBUGF(1, (CE_CONT,
		"p9100: cntxrestore: status = %x\n", p9100reg->p9100_status));
	return (0);
    }

    /*
     * start restoring stuff in.
     */

#if ALWAYS_NO_INTERRUPTS
    p9100reg->p9100_interrupt = saved->p9100_ctx_interrupt |
				    P9100_INT_VBLANKED_CTRL |
				    P9100_INT_PICKED_CTRL |
				    P9100_INT_DE_IDLE_CTRL;
    p9100reg->p9100_interrupt_en = saved->p9100_ctx_interrupt_en |
				    P9100_INTEN_MEN_CTRL |
				    P9100_INTEN_VBLANKED_EN_CTRL |
				    P9100_INTEN_PICKED_EN_CTRL |
				    P9100_INTEN_DE_IDLE_EN_CTRL;
#endif
    p9100reg->p9100_cindex = saved->p9100_ctx_cindex;
    p9100reg->p9100_w_off_xy = saved->p9100_ctx_w_off_xy;
    p9100reg->p9100_fground = saved->p9100_ctx_fground;
    p9100reg->p9100_bground = saved->p9100_ctx_bground;
    p9100reg->p9100_color2 = saved->p9100_ctx_color2;
    p9100reg->p9100_color3 = saved->p9100_ctx_color3;
    p9100reg->p9100_pmask = saved->p9100_ctx_pmask;
    p9100reg->p9100_draw_mode = saved->p9100_ctx_draw_mode;
    p9100reg->p9100_pat_originx = saved->p9100_ctx_pat_originx;
    p9100reg->p9100_pat_originy = saved->p9100_ctx_pat_originy;
    p9100reg->p9100_raster = saved->p9100_ctx_raster;
    p9100reg->p9100_pixel8_reg = saved->p9100_ctx_pixel8_reg;
    p9100reg->p9100_w_min = saved->p9100_ctx_w_min;
    p9100reg->p9100_w_max = saved->p9100_ctx_w_max;
    p9100reg->p9100_b_w_min = saved->p9100_ctx_b_w_min;
    p9100reg->p9100_b_w_max = saved->p9100_ctx_b_w_max;
    P9100_PAT_COPY(p9100reg->p9100_pattern, saved->p9100_ctx_pattern);
    p9100reg->p9100_x0 = saved->p9100_ctx_x0;
    p9100reg->p9100_y0 = saved->p9100_ctx_y0;
    p9100reg->p9100_x1 = saved->p9100_ctx_x1;
    p9100reg->p9100_y1 = saved->p9100_ctx_y1;
    p9100reg->p9100_x2 = saved->p9100_ctx_x2;
    p9100reg->p9100_y2 = saved->p9100_ctx_y2;
    p9100reg->p9100_x3 = saved->p9100_ctx_x3;
    p9100reg->p9100_y3 = saved->p9100_ctx_y3;
    return (1);
}


/*
 * p9100_ctx_map_insert()
 *
 * Insert a mapping into the mapping list of a private context.  First
 * determine if there's an existing context (e.g. one with the same PID
 * as the current  one and that does not already have a mapping of this
 * type yet).  If not, allocate a new one.  Then insert mapping into this
 * context's list.
 *
 * The softc mutex must be held across calls to this routine.
 */
static
struct p9100_cntxt *
p9100_ctx_map_insert(struct p9100_softc *softc, int maptype)
{
    register struct p9100_cntxt	*ctx;
    register struct p9100_cntxt	*pvt_ctx_list;
    int		curpid = p9100_getpid();

    DEBUGF(4, (CE_CONT, "p9100_ctx_map_insert: maptype=0x%x curpid=%d\n",
		maptype, curpid));

    /*
     * If this is the first time we're here, then alloc space
     * for new context and depart.
     */
    if (softc->pvt_ctx == NULL) {
	ctx = (struct p9100_cntxt *)
	kmem_zalloc(sizeof (struct p9100_cntxt), KM_SLEEP);
	ctx->pid = curpid;
	ctx->link = NULL;
	softc->pvt_ctx = ctx;
	return (ctx);
    }

    pvt_ctx_list = softc->pvt_ctx;

    /*
     * Find existing context if one exists.  We have a match if
     * we're the same process *and* there's not already a
     * mapping of this type assigned.
     */
    for (ctx = pvt_ctx_list; ctx != NULL; ctx = ctx->link) {
	if (ctx->pid == curpid &&
	    (maptype & ctx->p9100_ctx_flag & P9100_MAP_CTX) == 0)
	break;
    }

    /* no match, create a new one and add to softc list */
    if (ctx == NULL) {
	ctx = (struct p9100_cntxt *)
	kmem_zalloc(sizeof (struct p9100_cntxt), KM_SLEEP);
	ctx->pid = curpid;
	ctx->link = softc->pvt_ctx;
	softc->pvt_ctx = ctx;
    }

    return (ctx);
}

/*
 * p9100_getpid()
 *
 * Simple wrapper around process ID call to drv_getparm(9f).
 */
static int
p9100_getpid(void)
{
    u_long	rval;

    if (drv_getparm(PPID, &rval) == -1)
	return (0);
    else
	return ((int)rval);
}


/*
 * Viper specific routines
 */

viper_vlb_probe(dev_info_t *devi)
{
    p9100p_t	p9100;
    ulong_t	value;
    ddi_acc_handle_t p9100_regs_handle;

    DEBUGF(1, (CE_CONT, "viper_vlb_probe (%s) unit=%d\n",
		ddi_get_name(devi), ddi_get_instance(devi)));

    viper_vlb(devi);

    if (ddi_getprop(DDI_DEV_T_NONE, devi, DDI_PROP_DONTPASS,
	"present", 0) <= 0)
	return (DDI_PROBE_FAILURE);

    if (ddi_regs_map_setup(devi, 1,
	    (caddr_t *)&p9100,
	    0x0, sizeof (p9100_t), &endian_attr,
	    &p9100_regs_handle) != DDI_SUCCESS) {
	DEBUGF(2, (CE_CONT,
	    "p9100_probe: ddi_map_regs failed\n"));
	return (DDI_FAILURE);
    }

    /*
     * This is about the best we can do.  If we put the
     * board into Native mode, we will lose a bunch of
     * text-mode info.  Make sure that the bits we know
     * should be zero actually are zero.
     */
    outb(0x9100, 0x0);
    value = inb(0x9104) & 0xff;
cmn_err(CE_CONT, "value %x\n", value);
    if (value != 0x0e)
	goto failure;
    outb(0x9100, 0x1);
    value = inb(0x9104) & 0xff;
cmn_err(CE_CONT, "value 0x%x\n", value);
    if (value != 0x10)
	goto failure;
    outb(0x9100, 0x2);
    value = inb(0x9104) & 0xff;
cmn_err(CE_CONT, "value 0x%x\n", value);
    if (value != 0x0)
	goto failure;
    outb(0x9100, 0x3);
    value = inb(0x9104) & 0xff;
cmn_err(CE_CONT, "value 0x%x\n", value);
    if (value != 0x91)
	goto failure;

    ddi_regs_map_free(&p9100_regs_handle);

    return (DDI_PROBE_SUCCESS);

failure:
    DEBUGF(2, (CE_CONT,
	"p9100_probe: probe of VLB config regs failed\n"));
    ddi_regs_map_free(&p9100_regs_handle);

    return (DDI_PROBE_FAILURE);
}


static void
viper_vlb(register dev_info_t *devi)
{
	e_ddi_prop_remove(DDI_DEV_T_NONE, devi, "device_type");
	e_ddi_prop_create(DDI_DEV_T_NONE, devi, DDI_PROP_CANSLEEP,
	    "device_type", "vlb", 4);
}


static int
viper_ioctl(register dev_t const dev, register int cmd, register int data,
	register int mode, register cred_t *const cred,
	register int *const rval)
{
    register struct p9100_softc *const	softc = getsoftc(getminor(dev));
    register int	status;
    viper_config_t	temp;

    switch (cmd) {
    case VIPERIO_PUT_INIT:
	mutex_enter(&softc->mutex);
	status = ddi_copyin((caddr_t)data,
			    (caddr_t)&softc->viperinit,
			    sizeof (viper_init_t),
			    mode);
	if (!status) {
	    softc->w = softc->viperinit.vi_width;
	    softc->h = softc->viperinit.vi_height;
	    softc->depth = softc->viperinit.vi_depth;
	    softc->size = softc->h *
		P9100_LINEBYTES(softc->viperinit.vi_sysconfig);
	}
	mutex_exit(&softc->mutex);
	if (status)
	    return (EFAULT);
	break;

    case VIPERIO_GET_MECH:
	*rval = (softc->bus_type == VIPER_PORT_BUS_PCI) ? 1 : 0;
	break;

    case VIPERIO_GET_CONFIG_REG:
	status = ddi_copyin((caddr_t)data,
			    (caddr_t)&temp.vp_config_offset,
			    sizeof (viper_config_t),
			    mode);
	if (status) {
	    return (EFAULT);
	}
	if (softc->bus_type == VIPER_PORT_BUS_PCI) {
	    temp.vp_config_value =
		pci_config_getl(softc->p9100_config_handle,
		    temp.vp_config_offset);
	} else {
	    mutex_enter(&softc->mutex);
	    outb(0x9100, (temp.vp_config_offset << 2) & 0xff);
	    temp.vp_config_value = inb(0x9104) & 0xff;
	    outb(0x9100, ((temp.vp_config_offset << 2) + 1) & 0xff);
	    temp.vp_config_value |= (inb(0x9104) & 0xff) << 8;
	    outb(0x9100, ((temp.vp_config_offset << 2) + 2) & 0xff);
	    temp.vp_config_value |= (inb(0x9104) & 0xff) << 16;
	    outb(0x9100, ((temp.vp_config_offset << 2) + 3) & 0xff);
	    temp.vp_config_value |= (inb(0x9104) & 0xff) << 24;
	    mutex_exit(&softc->mutex);
	}
	status = ddi_copyout((caddr_t)&temp,
			    (caddr_t)data,
			    sizeof (viper_config_t),
			    mode);
	if (status)
	    return (EFAULT);
	break;

    case VIPERIO_PUT_CONFIG_REG:
	if (drv_priv(cred) == EPERM)
	    return (EPERM);
	status = ddi_copyin((caddr_t)data,
			    (caddr_t)&temp,
			    sizeof (viper_config_t),
			    mode);
	if (status) {
	    return (EFAULT);
	}
#if 1
	p9100_putpci(softc, temp.vp_config_offset,
	    temp.vp_config_value);
#else
	if (softc->bus_type == VIPER_PORT_BUS_PCI) {
	    p9100_putpci(softc, temp.vp_config_offset,
		temp.vp_config_value);
	} else {
	    mutex_enter(&softc->mutex);
	    outb(0x9100, (temp.vp_config_offset << 2) & 0xff);
	    outb(0x9104, (temp.vp_config_value & 0xff));
	    outb(0x9100, ((temp.vp_config_offset << 2) + 1) & 0xff);
	    outb(0x9104, (temp.vp_config_value & 0xff) << 8);
	    outb(0x9100, ((temp.vp_config_offset << 2) + 2) & 0xff);
	    outb(0x9104, (temp.vp_config_value & 0xff) << 16);
	    outb(0x9100, ((temp.vp_config_offset << 2) + 3) & 0xff);
	    outb(0x9104, (temp.vp_config_value & 0xff) << 24);
	    mutex_exit(&softc->mutex);
	}
#endif
	/*
	 * This is gross.  We intercept any calls to enable
	 * Native mode, then read the extended registers.
	 */
	if ((softc->dac_type == VIPER_DAC_UNKNOWN) &&
		(temp.vp_config_offset >= 0x40 &&
		    temp.vp_config_offset <= 0x43) &&
		!(temp.vp_config_value & 0x0000020)) {
	    if ((softc->p9100->p9100_pu_config
		& P9100_PUCONF_RAMDAC_MASK)
		== P9100_PUCONF_RAMDAC_IBM525) {
		softc->dac_type = VIPER_DAC_RGB525;
		softc->setcmap = rgb525_putcmap;
		softc->setcurpos = rgb525_setcurpos;
		softc->setcurshape = rgb525_setcurshape;
		softc->setcurcolor = rgb525_setcurcolor;
	    } else {
		softc->dac_type = VIPER_DAC_BT485;
		softc->setcmap = bt485_putcmap;
		softc->setcurpos = bt485_setcurpos;
		softc->setcurshape = bt485_setcurshape;
		softc->setcurcolor = bt485_setcurcolor;
	    }
	}
	break;

	case VIPERIO_SET_ICD_CLOCK:
	    mutex_enter(&softc->mutex);
	    viper_write_ic2061a(softc, data);
	    mutex_exit(&softc->mutex);
	break;

    default:
	return (ENOTTY);
    }
    return (0);
}

/*ARGSUSED*/
void
null_routine(register struct p9100_softc *const softc)
{
    cmn_err(CE_WARN, "p9100: routine referenced before set\n");
}

void
bt485_setcurpos(register struct p9100_softc *const softc)
{
    p9100p_t p9100ptr = softc->p9100;
    struct softcur *cur = &softc->cur;
    if (cur->enable) {
	(void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
	p9100ptr -> p9100_ramdac[BT485_CURSOR_X_LOW] =
              (cur->pos.x - cur->hot.x + 32) & 0xff;
	(void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
	p9100ptr -> p9100_ramdac[BT485_CURSOR_X_HIGH] =
              (cur->pos.x - cur->hot.x + 32) >> 8;
	(void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
	p9100ptr -> p9100_ramdac[BT485_CURSOR_Y_LOW] =
              (cur->pos.y - cur->hot.y + 32) & 0xff;
	(void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
	p9100ptr -> p9100_ramdac[BT485_CURSOR_Y_HIGH] =
              (cur->pos.y - cur->hot.y + 32) >> 8;
    } else {
	(void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
	p9100ptr -> p9100_ramdac[BT485_CURSOR_X_LOW] =
              BT485_CURSOR_OFFPOS & 0xff;
	(void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
	p9100ptr -> p9100_ramdac[BT485_CURSOR_X_HIGH] =
              BT485_CURSOR_OFFPOS >> 8;
	(void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
	p9100ptr -> p9100_ramdac[BT485_CURSOR_Y_LOW] =
              BT485_CURSOR_OFFPOS & 0xff;
	(void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
	p9100ptr -> p9100_ramdac[BT485_CURSOR_Y_HIGH] =
              BT485_CURSOR_OFFPOS >> 8;
    }
}

void
rgb525_setcurpos(register struct p9100_softc *const softc)
{
    register p9100p_t p9100ptr = softc->p9100;
    struct fbcurpos	*position = &softc->cur.pos;

    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);  /* Errata # 028 */
    p9100ptr -> p9100_ramdac[RGB525_INDEX_CONTROL] =
	(unsigned char)RGB525_INDEXCTRL_AUTO_INCREMENT;
    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);  /* Errata # 028 */
    p9100ptr -> p9100_ramdac[RGB525_INDEX_HIGH] = 0;
    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);  /* Errata # 028 */
    p9100ptr -> p9100_ramdac[RGB525_INDEX_LOW] = (unsigned char)
	RGB525_INDEX_CURSOR_X_LOW;
    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);  /* Errata # 028 */
    p9100ptr -> p9100_ramdac[RGB525_INDEX_DATA] =
	(unsigned char)(position->x & 0xff);
    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);  /* Errata # 028 */
    p9100ptr -> p9100_ramdac[RGB525_INDEX_DATA] =
	(unsigned char)(position->x >> 0x8);
    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);  /* Errata # 028 */
    p9100ptr -> p9100_ramdac[RGB525_INDEX_DATA] =
	(unsigned char)(position->y & 0xff);
    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);  /* Errata # 028 */
    p9100ptr -> p9100_ramdac[RGB525_INDEX_DATA] =
	(unsigned char)(position->y >> 0x8);
    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);  /* Errata # 028 */
    p9100ptr -> p9100_ramdac[RGB525_INDEX_CONTROL] =
	(unsigned char)0x0 /* RGB525_INDEXCTRL_AUTO_INCREMENT */;
}

/*
 * load HW cursor bitmaps
 */
static void
bt485_setcurshape(struct p9100_softc *const softc)
{
    register u_long	edge = 0xffffffffUL;
    register u_long	*image, *mask;
    register int	i;
    register p9100p_t	p9100ptr = softc->p9100;
    u_long		tmp;

    if (softc->cur.size.x < 32)
	edge = (1 << softc->cur.size.x) - 1;

    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
    p9100ptr->p9100_ramdac[0] = 0;
    for (i = 0, image = softc->cur.image;
	    i < 32; i++, image++) {
	tmp = *image;
	(void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
	p9100ptr->p9100_bitflip = tmp & 0xff;
	(void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
	p9100ptr->p9100_bitflip = (tmp >> 8) & 0xff;
	(void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
	p9100ptr->p9100_bitflip = (tmp >> 16) & 0xff;
	(void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
	p9100ptr->p9100_bitflip = (tmp >> 24) & 0xff;
    }

    for (i = 0, mask = softc->cur.mask; i < 32; i++, mask++) {
	tmp = *mask & edge;
	(void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
	p9100ptr->p9100_bitflip = tmp & 0xff;
	(void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
	p9100ptr->p9100_bitflip = (tmp >> 8) & 0xff;
	(void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
	p9100ptr->p9100_bitflip = (tmp >> 16) & 0xff;
	(void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
	p9100ptr->p9100_bitflip = (tmp >> 24) & 0xff;
    }
}

static void
rgb525_setcurshape(register struct p9100_softc *const softc)
{
    p9100p_t		p9100ptr = softc->p9100;
    int i;
    register unsigned char	*image, *mask;
    struct softcur    *cur = &softc->cur;

    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);  /* Errata # 028 */
    /* can't have the cursor on while we fiddle with the hot spots */
    p9100ptr -> p9100_ramdac[RGB525_INDEX_HIGH] = 0;
    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);  /* Errata # 028 */
    p9100ptr -> p9100_ramdac[RGB525_INDEX_LOW] = RGB525_INDEX_CURSOR_CONTROL;
    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);  /* Errata # 028 */
    p9100ptr -> p9100_ramdac[RGB525_INDEX_DATA] = (unsigned char)
	0x00 |	/* bit 7&6 : partition */
	RGB525_CURSOR_RIGHT_TO_LEFT |
	0x00 |	/* bit 4 : loc read - written value */
	RGB525_CURSOR_IMMEDIATE_UPDATE |
	RGB525_CURSOR_SIZE_32X32 |
	RGB525_CURSOR_MODE_OFF;
    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);  /* Errata # 028 */
    p9100ptr -> p9100_ramdac[RGB525_INDEX_LOW] =
	RGB525_INDEX_CURSOR_HOT_SPOT_X;
    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);  /* Errata # 028 */
    p9100ptr -> p9100_ramdac[RGB525_INDEX_DATA] =
	(unsigned char)cur->hot.x;
    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);  /* Errata # 028 */
    p9100ptr -> p9100_ramdac[RGB525_INDEX_LOW] =
	RGB525_INDEX_CURSOR_HOT_SPOT_Y;
    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);  /* Errata # 028 */
    p9100ptr -> p9100_ramdac[RGB525_INDEX_DATA] =
	(unsigned char)cur->hot.y;
    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);  /* Errata # 028 */
    p9100ptr -> p9100_ramdac[RGB525_INDEX_CONTROL] =
	(unsigned char)RGB525_INDEXCTRL_AUTO_INCREMENT;
    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);  /* Errata # 028 */
    p9100ptr -> p9100_ramdac[RGB525_INDEX_HIGH] = 1;
    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);  /* Errata # 028 */
    p9100ptr -> p9100_ramdac[RGB525_INDEX_LOW] =
	RGB525_INDEX_CURSOR_ARRAY;
    image = (unsigned char *)cur->image;
    mask = (unsigned char *)cur->mask;
    for (i = 0; i < 256; i++) {
	unsigned short	temp;

	temp = G91imagetable[(unsigned char)*image++];
	temp |= G91masktable[(unsigned char)*mask++];
	(void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
	p9100ptr -> p9100_ramdac[RGB525_INDEX_DATA] =
	    (unsigned char)temp;
	(void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
	p9100ptr -> p9100_ramdac[RGB525_INDEX_DATA] =
	    (unsigned char)(temp >> 8);
    }
    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
    p9100ptr -> p9100_ramdac[RGB525_INDEX_CONTROL] =
	(unsigned char)0;	/* ~RGB525_INDEXCTRL_AUTO_INCREMENT */
    /* turn cursor back on again */
    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
    p9100ptr -> p9100_ramdac[RGB525_INDEX_HIGH] = 0;
    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
    p9100ptr -> p9100_ramdac[RGB525_INDEX_LOW] = RGB525_INDEX_CURSOR_CONTROL;
    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
    p9100ptr -> p9100_ramdac[RGB525_INDEX_DATA] = (unsigned char)
	0x00 |	/* bit 7&6 : partition */
	RGB525_CURSOR_RIGHT_TO_LEFT |
	0x00 |	/* bit 4 : loc read - written value */
	RGB525_CURSOR_IMMEDIATE_UPDATE |
	RGB525_CURSOR_SIZE_32X32 |
	RGB525_CURSOR_MODE_MODE_2;
    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
}

static void
bt485_setcurcolor(register struct p9100_softc *const softc)
{
    register p9100p_t p9100ptr = softc->p9100;
    register int i;
    register unsigned char *omap;

    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
    p9100ptr->p9100_ramdac[4] = BT485_CURSOR1_COLOR;

    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
    omap = &softc->omap[0][0];
    for (i = 0; i < sizeof (softc->omap); i++) {
	(void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
	p9100ptr->p9100_ramdac[5] = *omap++;
    }
}

static void
rgb525_setcurcolor(register struct p9100_softc *const softc)
{
    register p9100p_t p9100ptr = softc->p9100;
    register int i;
    register unsigned char *omap;

    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
    p9100ptr -> p9100_ramdac[RGB525_INDEX_CONTROL] =
	(unsigned char)RGB525_INDEXCTRL_AUTO_INCREMENT;
    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
    p9100ptr -> p9100_ramdac[RGB525_INDEX_HIGH] = 0;
    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
    p9100ptr -> p9100_ramdac[RGB525_INDEX_LOW] =
	RGB525_INDEX_CURSOR_COLOR_1_RED;

    omap = &softc->omap[0][0];
    for (i = 0; i < sizeof (softc->omap); i++) {
	(void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
	p9100ptr -> p9100_ramdac[RGB525_INDEX_DATA] = *omap++;
    }
    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
    p9100ptr -> p9100_ramdac[RGB525_INDEX_CONTROL] =
	(unsigned char)0;	/* ~RGB525_INDEXCTRL_AUTO_INCREMENT */
}


static void
bt485_putcmap(softc)
    struct p9100_softc *const softc;
{
    p9100p_t p9100ptr;
    int i;
    unsigned char *map;
    int	pixel;

    pixel = softc->cmap_index;
    map = softc->cmap[pixel];

    p9100ptr = softc->p9100;
    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);  /* Errata # 028 */
    p9100ptr -> p9100_ramdac[BT485_RAM_WRITE] = pixel;
    for (i = 0; i < (int)softc->cmap_count * 3; i++) {
	(void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
	p9100ptr -> p9100_ramdac[BT485_PALET_DATA] =
	    map[i];
    }
}

static void
rgb525_putcmap(softc)
    struct p9100_softc *const softc;
{
    volatile p9100p_t p9100ptr = softc->p9100;
    int i;
    unsigned char *map;
    int	pixel;

    pixel = softc->cmap_index;
    map = softc->cmap[pixel];

    (void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);  /* Errata # 028 */
    p9100ptr -> p9100_ramdac[RGB525_PALET_WRITE] = pixel;
    for (i = 0; i < (int)softc->cmap_count * 3; i++) {
	(void) P9100_READ_FRAMEBUFFER_FOR_RAMDAC(p9100ptr);
	p9100ptr -> p9100_ramdac[RGB525_PALET_DATA] =
	    map[i];
    }
}


void
p9100_putpci(softc, reg, val)
    struct p9100_softc *const softc;
    unsigned long reg;
    unsigned long val;
{
    if (softc->bus_type == VIPER_PORT_BUS_PCI) {
#if __ppc
	asm (" sync");
#endif
	pci_config_putl(softc->p9100_config_handle,
	    reg, val);
#if __ppc
	asm (" sync");
#endif
    } else {
#if 0
	reg <<= 2;
#endif
	outb(0x9100, reg & 0xff);
	outb(0x9104, val & 0xff);
	outb(0x9100, (reg + 1) & 0xff);
	outb(0x9104, (val >> 8) & 0xff);
	outb(0x9100, (reg + 2) & 0xff);
	outb(0x9104, (val >> 16) & 0xff);
	outb(0x9100, (reg + 3) & 0xff);
	outb(0x9104, (val >> 24) & 0xff);
    }

}

#define	CLK_LOW		(0x00 << 16)
#define	CLK_HIGH	(0x04 << 16)
#define	DATA_LOW	(0x00 << 16)
#define	DATA_HIGH	(0x08 << 16)

static void
icd_pllout(softc, val)
    struct p9100_softc *const softc;
    unsigned long val;
{
    p9100_putpci(softc, 0x40, val);
    p9100_putpci(softc, 0x40, val);
    drv_usecwait(1);
}

u_long  p9100_icd_debug_cnt = 0;
u_long  p9100_icd_debug_array[16];

static  void
viper_write_ic2061a(register struct p9100_softc *const softc,
	register u_long reg)
{
  int		i /* , j */;
  unsigned	long    temp;
  unsigned long flags;

  p9100_icd_debug_array[p9100_icd_debug_cnt] = reg;
  p9100_icd_debug_cnt = (p9100_icd_debug_cnt + 1) & 0xf;

    DEBUGF(2,
	(CE_CONT, "viper_write_ic2061a\n"));
  temp = (unsigned long)(/* SCR_spec(p9100_scr) & */
      ~(PCI_9100SPEC1_MODESELECT_VGA << 8) &
      ~(CLK_HIGH | DATA_HIGH) &
      ~(/* PCI_9100SPEC2_ENABLE_INGR_REGS */ 0x10 << 16)) |
      ((PCI_9100SPEC1_ALT_COMMAND_SWAP_HALF |
      PCI_9100SPEC1_ALT_COMMAND_SWAP_BYTE |
      PCI_9100SPEC1_ALT_COMMAND_SWAP_BITS) << 8);

  flags = ddi_enter_critical();
  for (i = 0; i < 7; i++) { /* toggle clk 7 times with data high */
    icd_pllout(softc, temp | DATA_HIGH | CLK_LOW);
    icd_pllout(softc, temp | DATA_HIGH | CLK_HIGH);
  }

  icd_pllout(softc, temp | DATA_HIGH | CLK_LOW);
  icd_pllout(softc, temp | DATA_LOW  | CLK_LOW);
  icd_pllout(softc, temp | DATA_LOW  | CLK_HIGH);
  icd_pllout(softc, temp | DATA_LOW  | CLK_LOW);
  icd_pllout(softc, temp | DATA_LOW  | CLK_HIGH);

  for (i = 0; i < 24; i++, reg >>= 1) {
    icd_pllout(softc, temp | (reg&1 ? DATA_LOW  : DATA_HIGH) | CLK_HIGH);
    icd_pllout(softc, temp | (reg&1 ? DATA_LOW  : DATA_HIGH) | CLK_LOW);
    icd_pllout(softc, temp | (reg&1 ? DATA_HIGH : DATA_LOW)  | CLK_LOW);
    icd_pllout(softc, temp | (reg&1 ? DATA_HIGH : DATA_LOW)  | CLK_HIGH);
  }

  icd_pllout(softc, temp | DATA_HIGH | CLK_HIGH);
  icd_pllout(softc, temp | DATA_HIGH | CLK_LOW);
  icd_pllout(softc, temp | DATA_HIGH | CLK_HIGH);
  ddi_exit_critical(flags);

    /* The clock settings become  effective  when  the  watchdog */
    /* timer times out, 20ms. */
    delay(drv_usectohz(20000));
}

static void
p9100_get_hardware_settings(struct p9100_softc *softc)
{
    softc->h = softc->p9100->p9100_vrtbf - softc->p9100->p9100_vrtbr;

    switch (softc->p9100->p9100_sysconfig & P9100_SYSCONF_PIXEL_SIZE_MASK) {
    case P9100_SYSCONF_PIXEL_SIZE_32BPP:
#if	defined(CONSOLE_REDESIGN)
#define	SWAPMASK	(P9100_SYSCONF_PIXEL_SWAP_HALF | \
			P9100_SYSCONF_PIXEL_SWAP_BYTE | \
			P9100_SYSCONF_PIXEL_SWAP_BITS)
#define	SWAP_0RGB	0
#define	SWAP_BGR0	(P9100_SYSCONF_PIXEL_SWAP_HALF | \
			P9100_SYSCONF_PIXEL_SWAP_BYTE)

	switch (softc->p9100->p9100_sysconfig & SWAPMASK) {
	default:
	    cmn_err(CE_WARN,
		"p9100:  Swap mode not supported.  Swap code 0x%x.",
		softc->p9100->p9100_sysconfig & SWAPMASK);
	    /*
	     * There's no clean way to fail, and it might be worse if we did,
	     * so...
	     */
	    /* FALLTHROUGH */
	case SWAP_0RGB:
	    softc->display_func = p9100_cons_display_32_0RGB;
	    softc->cursor_func = p9100_cons_cursor_32_0RGB;
	    break;
	case SWAP_BGR0:
	    softc->display_func = p9100_cons_display_32_BGR0;
	    softc->cursor_func = p9100_cons_cursor_32_BGR0;
	    break;
	}
	softc->copy_func = p9100_cons_copy_32;
#endif
	softc->depth = 32;	/* for user program consumers */
	softc->tem_depth = 24;	/* for terminal emulator interface */
	softc->w = (softc->p9100->p9100_hrzbf - softc->p9100->p9100_hrzbr) * 2;
	break;

    case P9100_SYSCONF_PIXEL_SIZE_16BPP:
    case P9100_SYSCONF_PIXEL_SIZE_24BPP:
    default:
	cmn_err(CE_WARN, "p9100:  Pixel size not supported.  Size code 0x%x.",
	    (softc->p9100->p9100_sysconfig & P9100_SYSCONF_PIXEL_SIZE_MASK) >>
		P9100_SYSCONF_PIXEL_SIZE_SHIFT);
	/*
	 * There's no clean way to fail, and it might be worse if we did,
	 * so...
	 */
	/* FALLTHROUGH */
    case P9100_SYSCONF_PIXEL_SIZE_8BPP:
#if	defined(CONSOLE_REDESIGN)
	softc->copy_func = p9100_cons_copy_8;
	softc->display_func = p9100_cons_display_8;
	softc->cursor_func = p9100_cons_cursor_8;
#endif
	softc->depth = 8;	/* for user program consumers */
	softc->tem_depth = 8;	/* for terminal emulator interface */
	softc->w = (softc->p9100->p9100_hrzbf - softc->p9100->p9100_hrzbr) * 8;
	break;
    }
}
