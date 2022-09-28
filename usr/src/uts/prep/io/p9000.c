/*
 * Copyright (c) 1989-1996, Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)p9000.c	1.32	96/09/24 SMI"
#pragma	weak	setintrenable

/*
 * Driver for Weitek P9000
 */

/*
 * accelerated 8 bit color frame buffer driver
 * NEEDSWORK:  well, not accelerated yet
 */

#include <sys/debug.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/buf.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/conf.h>

#include <sys/file.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/vmmac.h>
#include <sys/mman.h>
#include <sys/cred.h>
#include <sys/open.h>
#include <sys/stat.h>

#include <sys/visual_io.h>
#include <sys/fbio.h>

#include <sys/p9000reg.h>
#include <sys/bt485reg.h>
#define	Bt485_CMAP_ENTRIES 256
#include <sys/fairway.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/vnode.h>

#include <vm/page.h>
#include <vm/as.h>
#include <vm/hat.h>
#include <vm/seg.h>

#include <sys/pixrect.h>
#include <sys/pr_impl_util.h>
#include <sys/pr_planegroups.h>
/* #include <sys/pr_util.h> */
#include <sys/memvar.h>
#include <sys/p9000var.h>

#include <sys/kmem.h>
#include <sys/conf.h>
#include <sys/devops.h>
#include <sys/ddi_impldefs.h>
#include <sys/archsystm.h>
#include <sys/fs/snode.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

#include <sys/modctl.h>

#include <sys/machsystm.h>

/* configuration options */

char _depends_on[] = "misc/seg_mapdev";

#if	defined(DEBUG)
#define	DRIVER_DEBUG	127
#else
/*
 * Preprocessor will turn DRIVER_DEBUG refs in #ifs into 0s.
 * This is fine, and allows setting DRIVER_DEBUG as a compile
 * option in a non-DEBUG environment.
 */
#endif

#define	P9000_REG_RNUMBER	1

#define	CMAP_ENTRIES	Bt485_CMAP_ENTRIES

#define	P9000DELAY(c, n)    \
{ \
	register int N = n; \
	while (--N > 0) { \
	    if (c) \
		break; \
	    drv_usecwait(1); \
	} \
}

#if DRIVER_DEBUG
int	p9000_debug = 0;

#define	DEBUGF(level, args) \
		{ if (p9000_debug >= (level)) cmn_err args; }
#define	DUMP_SEGS(level, s, c) \
		{ if (p9000_debug >= (level)) dump_segs(s, c); }
#else
#define	DEBUGF(level, args)	/* nothing */
#define	DUMP_SEGS(level, s, c)	/* nothing */
#endif

#define	getprop(devi, name, def)	\
		ddi_getprop(DDI_DEV_T_ANY, (devi), \
		DDI_PROP_DONTPASS, (name), (def))

/* config info */

static int	p9000_open(dev_t *, int, int, cred_t *);
static int	p9000_close(dev_t, int, int, cred_t *);
static int	p9000_ioctl(dev_t, int, intptr_t, int, cred_t *, int *);
static int	p9000_mmap(dev_t, off_t, int);
static int	p9000_segmap(dev_t, off_t,
			struct as *, caddr_t *, off_t, u_int,
			u_int, u_int, cred_t *);

static struct vis_identifier p9000_ident = { "SUNWp9000" };

static struct cb_ops p9000_cb_ops = {
	p9000_open,		/* open */
	p9000_close,		/* close */
	nodev,			/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	nodev,			/* read */
	nodev,			/* write */
	p9000_ioctl,		/* ioctl */
	nodev,			/* devmap */
	p9000_mmap,		/* mmap */
	p9000_segmap,		/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,		/* cb_prop_op */
	0,			/* streamtab  */
	D_NEW | D_MP		/* Driver compatibility flag */
};

static int p9000_info(dev_info_t *dip, ddi_info_cmd_t infocmd,
		void *arg, void **result);
static int p9000_attach(dev_info_t *, ddi_attach_cmd_t);
static int p9000_detach(dev_info_t *, ddi_detach_cmd_t);
static int p9000_power(dev_info_t *, int, int);
static int p9000_probe(dev_info_t *);

struct dev_ops p9000_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	p9000_info,		/* info */
	nulldev,		/* identify */
	p9000_probe,		/* probe */
	p9000_attach,		/* attach */
	p9000_detach,		/* detach */
	nodev,			/* reset */
	&p9000_cb_ops,		/* driver operations */
	(struct bus_ops *)0,	/* bus operations */
	p9000_power
};

/*
 * This stucture is used to contain the driver
 * private mapping data (one for each requested
 * device mapping).  A pointer to this data is
 * passed into each mapping callback routine.
 */
static
struct p9000map_pvt {
	struct	p9000_softc *softc;
	ddi_mapdev_handle_t handle;	/* handle of mapdev object	*/
	u_int   offset;			/* starting offset of this map	*/
	u_int   len;			/* length of this map		*/
	struct p9000_cntxt *context;	/* associated context		*/
	struct p9000map_pvt *next;	/* List of associated pvt's for	*/
					/* this context			*/
};

/* how much to map */
#define	P9000MAPSIZE	MMAPSIZE(0)

#define	P9000_MEMSIZE_SCALE	(1024*1024)

/* vertical retrace counter page */
#ifndef	P9000_VRT_SZ
#define	P9000_VRT_SZ	8192
#endif

/*
 * Per-context info:
 *	many registers in the tec and fbc do
 *	not need to be saved/restored.
 */
struct p9000_cntxt {
	struct p9000_cntxt *link; /* link to next (private) context if any */
	struct p9000map_pvt *pvt; /* List of associated pvt's for this ctx */
	int	pid;		/* "owner" of this context */
	int	flag;

	struct {
	    u_int   mv;
	    u_int   clip;
	    u_int   vdc;
	    u_int   data[64][2];
	}    tec;

	struct {
	    u_int   status;
	    u_int   clipcheck;
/*	    struct l_fbc_misc misc; XXXppc */
	    u_int   x0, y0, x1, y1, x2, y2, x3, y3;
	    u_int   rasteroffx, rasteroffy;
	    u_int   autoincx, autoincy;
	    u_int   clipminx, clipminy, clipmaxx, clipmaxy;
	    u_int   fcolor, bcolor;
/*	    struct l_fbc_rasterop rasterop; XXXppc */
	    u_int   planemask, pixelmask;
/*	    union l_fbc_pattalign pattalign; XXXppc */
	    u_int   pattern0, pattern1, pattern2, pattern3, pattern4, pattern5,
		pattern6, pattern7;
	}    fbc;
};

/* per-unit data */
struct p9000_softc {
	Pixrect pr;			/* kernel pixrect */
	struct mprp_data prd;	/* pixrect private data */
#define	_w		pr.pr_size.x
#define	_h		pr.pr_size.y
#define	_fb		prd.mpr.md_image
#define	_linebytes	prd.mpr.md_linebytes
	int    size;		/* total size of frame buffer */
	int    ndvramsz;	/* size of non-display Video RAM */
	caddr_t ndvram;		/* Storage for nd-VRAM, while suspended */
	int    dummysize;	/* total size of overlay plane */
#ifdef	XXXppc
	volatile u_long *p4reg;	/* we're a P4 */
#endif
	kmutex_t interlock;	/* interrupt locking */
#ifdef	XXXppc
	off_t   addr_rom;	/* varies between p4 & sbus */
	caddr_t fbctec;	/* fbc&tec kernel map addr. */
	caddr_t cmap;		/* colormap kernel map addr. */
	caddr_t fhcthc;	/* fhc&thc kernel map addr. */
	caddr_t rom;		/* rom kernel map addr. */
	caddr_t dhc;		/* dac hardware */
	caddr_t alt;		/* alt registers */
	caddr_t uart;		/* uart registers */
#else	/* XXXppc */
	u_int   fbpfnum;	/* pfn of fb for mmap() */
	volatile struct p9000 *regs;
	ddi_acc_handle_t regs_handle;
	boolean_t regs_mapped;
#endif	/* XXXppc */

	struct softcur {
	    short   enable;		/* cursor enable */
	    short   pad1;
	    struct fbcurpos pos;	/* cursor position */
	    struct fbcurpos hot;	/* cursor hot spot */
	    struct fbcurpos size;	/* cursor bitmap size */
	    u_long  image[32];		/* cursor image bitmap */
	    u_long  mask[32];		/* cursor mask bitmap */
	}    cur;

	union {			/* shadow overlay color map */
	    u_long  omap_long[2];	/* cheating here to save space */
	    u_char  omap_char[3][2];
	}    omap_image;
#define	omap_rgb	omap_image.omap_char[0]
	u_short omap_update;	/* overlay colormap update flag */

	u_short cmap_index;	/* colormap update index */
	u_short cmap_count;	/* colormap update count */
	union {			/* shadow color map */
	    u_long  cmap_long[CMAP_ENTRIES * 3 / sizeof (u_long)];
	    u_char  cmap_char[3][CMAP_ENTRIES];
	}    cmap_image;
#define	cmap_rgb	cmap_image.cmap_char[0]

#define	P9000VRTIOCTL	1	/* FBIOVERTICAL in effect */
#define	P9000VRTCTR	2	/* OWGX vertical retrace counter */
	unsigned long fbmappable;	/* bytes mappable */
	int		*vrtpage;	/* pointer to VRT page */
	int		*vrtalloc;	/* pointer to VRT allocation */
	int		vrtmaps;	/* number of VRT page maps */
	int		vrtflag;	/* vrt interrupt flag */
	struct p9000_info adpinfo;	/* info about this adapter */
	struct mon_info moninfo;	/* info about this monitor */
	struct p9000_cntxt *curctx;	/* context switching */
	struct p9000_cntxt shared_ctx;	/* shared context */
	struct p9000_cntxt *pvt_ctx;	/* list of non-shared contexts */
	int		chiprev;	/* fbc chip revision # */
	int		emulation;	/* emulation type, normally p9000 */
	dev_info_t	*devi;		/* back pointer */
	ddi_iblock_cookie_t iblock_cookie;	/* block interrupts */
	kmutex_t	mutex;		/* mutex locking */
	kcondvar_t	vrtsleep;	/* for waiting on vertical retrace */
	boolean_t	mapped_by_prom;	/* $#!@ SVr4 */
	int		p9000_suspended; /* true if driver is suspended */
	struct p9000_console_private
			*conspriv;	/* console support */
};

/* console management */

struct  p9000_console_private {
	struct p9000_softc
			*softc;	/* softc for this instance */
	struct p9000_cntxt
			context;	/* pointer to context */
	int		viscursor;
				/* cursor visible on this instance? */
	kmutex_t	pmutex;	/* mutex locking */
	int		fg;	/* foreground color */
	int		bg;	/* background color */
	int		ifg;	/* inverse video foreground color */
	int		ibg;	/* inverse video background color */
};


struct p9000_rect {
	short w;
	short h;
	short x;
	short y;
};

static int	p9000_devinit(dev_t dev, struct vis_devinit *);
static void	p9000_cons_free(struct p9000_console_private *);
static void	p9000_cons_copy(struct p9000_console_private *,
			struct vis_conscopy *);
static void	p9000_cons_display(struct p9000_console_private *,
			struct vis_consdisplay *);
static void	p9000_cons_cursor(struct p9000_console_private *,
			struct vis_conscursor *);

static int p9000map_access(ddi_mapdev_handle_t, void *, off_t);
static void p9000map_free(ddi_mapdev_handle_t, void *);
static int p9000map_dup(ddi_mapdev_handle_t, void *,
		ddi_mapdev_handle_t, void **);

static
struct ddi_mapdev_ctl p9000map_ops = {
	MAPDEV_REV,	/* mapdev_ops version number	*/
	p9000map_access,	/* mapdev access routine	*/
	p9000map_free,	/* mapdev free routine		*/
	p9000map_dup	/* mapdev dup routine		*/
};

static u_int	pagesize;
static void	*p9000_softc_head;

/* default structure for FBIOGATTR ioctl */
static struct fbgattr p9000_attr = {
/*	real_type	 owner */
	FBTYPE_SUNFAST_COLOR, 0,
/* fbtype: type		 h  w  depth    cms  size */
	{FBTYPE_SUNFAST_COLOR, 0, 0, P9000_DEPTH, CMAP_ENTRIES, 0},
/* fbsattr: flags emu_type    dev_specific */
	{0, FBTYPE_SUN4COLOR, {0}},
/*	emu_types */
	{FBTYPE_SUNFAST_COLOR, FBTYPE_SUN3COLOR, FBTYPE_SUN4COLOR, -1}
};


/*
 * handy macros
 */

#define	getsoftc(instance)	\
	((struct p9000_softc *)ddi_get_soft_state(p9000_softc_head, (instance)))

#define	btob(n)			ctob(btoc(n))	/* TODO, change this? */

/* convert softc to data pointers */

#ifdef	XXXppc
#define	S_FBC(softc)	((struct fbc *)(softc)->fbctec)
#define	S_TEC(softc)	((struct tec *)((softc)->fbctec + P9000_TEC_POFF))
#define	S_FHC(softc)	((u_int *)(softc)->fhcthc)
#define	S_THC(softc)	((struct thc *)((softc)->fhcthc + P9000_TEC_POFF))
#endif	/* XXXppc */
#define	S_CMAP(softc)	((struct p9000_cmap *)(softc)->cmap)

#ifdef	XXXppc
#define	p9000_set_video(softc, on)	thc_set_video(S_THC(softc), (on))
#define	p9000_get_video(softc)		thc_get_video(S_THC(softc))

#define	p9000_set_sync(softc, on) \
	    S_THC(softc)->l_thc_hcmisc = \
	    (S_THC(softc)->l_thc_hcmisc & ~THC_HCMISC_SYNCEN | \
	    ((on) ? THC_HCMISC_SYNCEN : 0))


#define	p9000_int_enable(softc) \
	{\
	    thc_int_enable(S_THC(softc)); \
	    if ((softc)->p4reg) \
		(void) setintrenable(1); }

#define	p9000_int_disable_intr(softc) \
	{\
	    if ((softc)->p4reg) \
		(void) setintrenable(0); \
	    thc_int_disable(S_THC(softc)); }

#define	p9000_int_disable(softc) \
	{\
	    mutex_enter(&(softc)->interlock); \
	    p9000_int_disable_intr(softc);    \
	    mutex_exit(&(softc)->interlock); }

#define	p9000_int_pending(softc)		thc_int_pending(S_THC(softc))
#else
#define	p9000_set_video(softc, on)	/* NYI */
#define	p9000_set_sync(softc, on)	/* NYI */
#endif	/* XXXppc */

/* check if color map update is pending */
#define	p9000_update_pending(softc) \
	((softc)->cmap_count || (softc)->omap_update)

/*
 * forward references
 */
#ifdef	XXXppc
static u_int	p9000_intr(caddr_t);
#endif
static void	p9000_reset_cmap(volatile u_char *, u_int);
static void	p9000_update_cmap(struct p9000_softc *, u_int, u_int);
#ifdef	XXXppc
static void	p9000_cmap_bcopy(u_char *, u_char *, u_int);
#endif
static void	p9000_cons_put_cmap(struct p9000_softc *, int,
			unsigned char, unsigned char, unsigned char);
static void	p9000_cons_get_cmap(struct p9000_softc *, int,
			unsigned char *, unsigned char *, unsigned char *);

static void	p9000_setcurpos(struct p9000_softc *);
static void	p9000_setcurshape(struct p9000_softc *);
static void	p9000_reset(struct p9000_softc *);
static void	p9000_smart_bcopy(char *, char *, int);
#if	DRIVER_DEBUG
static void	p9000_register_dump(struct p9000_softc *softc);
#endif
#ifdef	XXXppc
static int	p9000_cntxsave(volatile struct fbc *, volatile struct tec *,
			struct p9000_cntxt *);
static int	p9000_cntxrestore(volatile struct fbc *, volatile struct tec *,
			struct p9000_cntxt *);
#endif	/* XXXppc */
static struct p9000_cntxt *ctx_map_insert(struct p9000_softc *, int);
static int	getpid(void);

/*
 * SunWindows specific stuff
 */
static  p9000_rop(), p9000_putcolormap();

/* kernel pixrect ops vector */
static struct pixrectops p9000_pr_ops = {
	p9000_rop,
	p9000_putcolormap,
	mem_putattributes
};

/* Loadable Driver stuff */

static struct modldrv modldrv = {
	&mod_driverops,		/* Type of module.  This one is a driver */
	"p9000 driver v1.32",	/* Name of the module. */
	&p9000_ops,		/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *) &modldrv, NULL
};

int
_init(void)
{
	register int e;

	DEBUGF(1, (CE_CONT, "p9000: compiled %s, %s\n", __TIME__, __DATE__));

	if ((e = ddi_soft_state_init(&p9000_softc_head,
		    sizeof (struct p9000_softc), 1)) != 0) {
	    DEBUGF(1, (CE_CONT, "done\n"));
	    return (e);
	}

	e = mod_install(&modlinkage);

	if (e) {
	    ddi_soft_state_fini(&p9000_softc_head);
	    DEBUGF(1, (CE_CONT, "done\n"));
	}
	DEBUGF(1, (CE_CONT, "p9000: _init done rtn=%d\n", e));
	return (e);
}

int
_fini(void)
{
	register int e;

#ifdef	XXXppc
	DEBUGF(1, (CE_CONT, "p9000: _fini, mem used=%d\n", total_memory));
#else
	DEBUGF(1, (CE_CONT, "p9000: _fini\n"));
#endif

	if ((e = mod_remove(&modlinkage)) != 0)
	    return (e);

	ddi_soft_state_fini(&p9000_softc_head);

	return (0);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/* XXX cursor artifact avoidance -- there must be a better way to do this */

static  p9000_rop_wait = 50;	/* milliseconds */

static
p9000_rop(Pixrect *dpr,
	int dx,
	int dy,
	int dw,
	int dh,
	int op,
	Pixrect *spr,
	int sx,
	int sy)
{
	Pixrect mpr;
	int    unit = 0;
#ifdef	XXXppc
	volatile struct fbc *fbc = S_FBC(getsoftc(unit));
#endif

#if defined(lint)
	(void) unit;
#endif
	if (spr && spr->pr_ops == &p9000_pr_ops) {
	    unit = mpr_d(spr)->md_primary;
	    mpr = *spr;
	    mpr.pr_ops = &mem_ops;
	    spr = &mpr;
	} else
	    unit = mpr_d(dpr)->md_primary;

	if (p9000_rop_wait) {
#ifdef	XXXppc
	    volatile u_int *statp = &fbc->l_fbc_status;
	    CDELAY(!(*statp & L_FBC_BUSY), p9000_rop_wait << 10);
#else
	    /*EMPTY*/
	    /* NYI:  wait for idle */
#endif
	}
	return (mem_rop(dpr, dx, dy, dw, dh, op, spr, sx, sy));
}

/*ARGSUSED*/
static
p9000_putcolormap(Pixrect *pr,
		int index,
		int count,
		unsigned char *red,
		unsigned char *green,
		unsigned char *blue)
{
#ifdef	XXXppc
	register struct p9000_softc *softc = getsoftc(mpr_d(pr)->md_primary);
	register u_int rindex = (u_int) index;
	register u_int rcount = (u_int) count;
	register u_char *map;
	register u_int entries;

	DEBUGF(5, (CE_CONT, "p9000_putcolormap unit=%d index=%d count=%d\n",
	    mpr_d(pr)->md_primary, index, count));

	map = softc->cmap_rgb;
	entries = CMAP_ENTRIES;

	/* check arguments */
	if (rindex >= entries || rindex + rcount > entries)
	    return (PIX_ERR);

	if (rcount == 0)
	    return (0);

	mutex_enter(&softc->mutex);

#ifdef	XXXppc
	/* lock out updates of the hardware colormap */
	if (p9000_update_pending(softc))
	    p9000_int_disable(softc);
#endif

	map += rindex * 3;
	p9000_cmap_bcopy(red, map, rcount);
	p9000_cmap_bcopy(green, map + 1, rcount);
	p9000_cmap_bcopy(blue, map + 2, rcount);

	p9000_update_cmap(softc, rindex, rcount);

#ifdef	XXXppc
	/* enable interrupt so we can load the hardware colormap */
	p9000_int_enable(softc);
#endif
	mutex_exit(&softc->mutex);
#else
	/* NYI:  not implemented */
#endif
	return (0);
}

/*
 * This function identifies p4 legos.  We really ought to comment this code
 * out on s-bus machines, but the powers-that-be want a single binary to
 * execute on all three architectures (five if they port this to sun3)
 */

static int
p9000_probe(dev_info_t *devi)
{
	int	error = DDI_PROBE_SUCCESS;
#ifdef	XXXppc
	long	*fbc;
	long	id;
#endif

	DEBUGF(1, (CE_CONT, "p9000_probe (%s) unit=%d\n",
	    ddi_get_name(devi), ddi_get_instance(devi)));

	if (ddi_dev_is_sid(devi) == DDI_SUCCESS)
	    return (DDI_PROBE_DONTCARE);

#ifdef	XXXppc
	/*
	 * after this, fbc will point to the first address in
	 * the fbc address space - the p4 register
	 */
	if (ddi_map_regs(devi, P9000_REG_RNUMBER, (caddr_t *)&fbc,
		    P9000_ADDR_P4REG, (off_t)sizeof (long)) != 0) {
	    DEBUGF(2, (CE_CONT, "  map_regs failed, returning failure\n"));
	    return (DDI_FAILURE);
	}
	if (ddi_peekl(devi, fbc, &id) == DDI_FAILURE) {
	    DEBUGF(2, (CE_CONT, "  peek failed, returning failure\n"));
	    error = DDI_FAILURE;
	} else {
	    DEBUGF(2, (CE_CONT, "  P4 register is %x\n", *fbc));
	    if ((((u_long) id >> 24) & P4_ID_MASK) == P4_ID_FASTCOLOR) {
		DEBUGF(2, (CE_CONT, "  returning success\n"));
		error = DDI_SUCCESS;
	    } else {
		DEBUGF(2, (CE_CONT, " not a p9000 - returning failure\n"));
		error = DDI_FAILURE;
	    }
	}

	ddi_unmap_regs(devi, P9000_REG_RNUMBER, (caddr_t *)&fbc,
		P9000_ADDR_P4REG, (off_t)sizeof (long));
#endif
	return (error);
}

static int
p9000_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	register struct p9000_softc *softc;
	int		w, h, bytes;
	char		*tmp;
	char		name[16];
	int		unit = ddi_get_instance(devi);
	int		proplen;
	u_int		power;
#ifdef	XXXppc
	caddr_t		fb_ndvram;
#endif
	static ddi_device_acc_attr_t access_mode = {
		DDI_DEVICE_ATTR_V0,
		DDI_NEVERSWAP_ACC,	/* not portable */
		DDI_STRICTORDER_ACC,
	};

	DEBUGF(1, (CE_CONT, "p9000_attach unit=%d cmd=%d\n", unit, (int)cmd));

	switch (cmd) {
	case DDI_ATTACH:
	    break;

	case DDI_RESUME:
	    if (!(softc = (struct p9000_softc *)ddi_get_driver_private(devi)))
		    return (DDI_FAILURE);
	    if (!softc->p9000_suspended)
		    return (DDI_SUCCESS);
	    mutex_enter(&softc->mutex);
	    p9000_reset(softc);
	    if (softc->curctx) {
#ifdef	XXXppc
		    /* Restore non display RAM */
		    if (ddi_map_regs(devi, P9000_REG_RNUMBER,
			(caddr_t *)&fb_ndvram,
			P9000_ADDR_COLOR + softc->_w * softc->_h,
			softc->ndvramsz) == -1) {
			    mutex_exit(&softc->mutex);
			    return (DDI_FAILURE);
		    }
		    bcopy(softc->ndvram, fb_ndvram, softc->ndvramsz);
		    ddi_unmap_regs(devi, P9000_REG_RNUMBER,
			(caddr_t *)&fb_ndvram,
			P9000_ADDR_COLOR + softc->_w * softc->_h,
			softc->ndvramsz);
		    kmem_free(softc->ndvram, softc->ndvramsz);

		    /* Restore other frame buffer state */
		    (void) p9000_cntxrestore(S_FBC(softc), S_TEC(softc),
			softc->curctx);
		    p9000_setcurpos(softc);
		    p9000_setcurshape(softc);
		    p9000_update_cmap(softc, (u_int)_ZERO_, CMAP_ENTRIES);
		    p9000_int_enable(softc);	/* Schedule the update */
#else
		/*EMPTY*/
		/* NYI:  parts of DDI_RESUME not implemented */
#endif	/* XXXppc */
	    }
	    softc->p9000_suspended = 0;
	    mutex_exit(&softc->mutex);
	    /* Restore brightness level */
	    if ((power = pm_get_normal_power(devi, 1)) == DDI_FAILURE) {
		cmn_err(CE_WARN, "p9000_attach(DDI_RESUME) can't get normal "
		"power");
		return (DDI_FAILURE);
	    }
	    if (p9000_power(devi, 1, power) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "p9000_attach: p9000_power failed");
		return (DDI_FAILURE);
	    }

	    return (DDI_SUCCESS);

	default:
	    return (DDI_FAILURE);
	}

	DEBUGF(1, (CE_CONT, "p9000_attach unit=%d\n", unit));

	pagesize = ddi_ptob(devi, 1);

	/* Allocate softc struct */
	if (ddi_soft_state_zalloc(p9000_softc_head, unit) != 0) {
		return (DDI_FAILURE);
	}

	softc = getsoftc(unit);

	/* link it in */
	softc->devi = devi;
	DEBUGF(1, (CE_CONT, "p9000_attach devi=0x%x unit=%d\n", devi, unit));
	ddi_set_driver_private(devi, (caddr_t)softc);

#ifdef	XXXppc
	softc->p4reg = 0;
	if (ddi_dev_is_sid(devi) != DDI_SUCCESS) {
	    if (ddi_map_regs(devi, P9000_REG_RNUMBER,
			(caddr_t *)&softc->p4reg,
			(off_t)P9000_ADDR_P4REG, sizeof (*softc->p4reg)) != 0) {
		DEBUGF(2, (CE_CONT,
			"p9000_probe: ddi_map_regs (p4reg) FAILED\n"));
		return (DDI_FAILURE);
	    }
	}
#endif

	/*
	 * for P4, we might determine the resolution from the FBC
	 * config register, but since we only built 1152x900 for that
	 * board, the defaults work as well as anything else. Still, I'm
	 * going to separately map in the FBC config (P4) register just
	 * to make it easy for somebody to enhance this. JMP
	 */

	/* Grab properties from PROM */
	/* TODO don't really want default w, h */
	if (ddi_prop_op(DDI_DEV_T_ANY, devi, PROP_LEN_AND_VAL_ALLOC,
		DDI_PROP_DONTPASS, "emulation", (caddr_t)&tmp, &proplen) ==
		    DDI_PROP_SUCCESS) {
	    if (strcmp(tmp, "cgthree+") == 0)
		softc->emulation = FBTYPE_SUN3COLOR;
	    else if (strcmp(tmp, "cgfour+") == 0)
		softc->emulation = FBTYPE_SUN4COLOR;
	    else if (strcmp(tmp, "bwtwo+") == 0)
		softc->emulation = FBTYPE_SUN2BW;
	    else
		softc->emulation = FBTYPE_SUNFAST_COLOR;
	    kmem_free(tmp, proplen);
	} else
	    softc->emulation = FBTYPE_SUNFAST_COLOR;

	/* NYI:  constant vmsize */
	softc->adpinfo.vmsize = 4;
	softc->fbmappable = softc->adpinfo.vmsize * P9000_MEMSIZE_SCALE;
	softc->size = softc->fbmappable;

	/* map device if necessary */
	if (ddi_regs_map_setup(devi, P9000_REG_RNUMBER,
		(caddr_t *)&softc->regs, (off_t)0, (off_t)softc->fbmappable,
		&access_mode, &softc->regs_handle) == -1) {
	    p9000_detach(devi, DDI_DETACH);
	    return (DDI_FAILURE);
	}
	softc->_fb = (short *)softc->regs->p9000_frame_buffer;
	softc->regs_mapped = B_TRUE;

#if	DRIVER_DEBUG
	if (p9000_debug > 0)
	    p9000_register_dump(softc);
#endif

	w = (softc->regs->p9000_hrzbf
	    - softc->regs->p9000_hrzbr)
		* 4;
	h = softc->regs->p9000_vrtbf
	    - softc->regs->p9000_vrtbr;

	softc->_w = w = getprop(devi, "width", w);
	softc->_h = h = getprop(devi, "height", h);
	bytes = getprop(devi, "linebytes", mpr_linebytes(w, 8));

	softc->_linebytes = bytes;

	/* Compute size of dummy overlay/enable planes */
	softc->dummysize = btob(mpr_linebytes(w, 1) * h) * 2;

	softc->adpinfo.line_bytes = softc->_linebytes;
	softc->adpinfo.accessible_width = getprop(devi, "awidth", 1152);
	softc->adpinfo.accessible_height = (u_int)
	    (long)softc->adpinfo.vmsize * P9000_MEMSIZE_SCALE
	    / softc->adpinfo.accessible_width;
	softc->adpinfo.hdb_capable = getprop(devi, "dblbuf", 0);
	softc->adpinfo.boardrev = getprop(devi, "boardrev", 0);
	softc->vrtpage = NULL;
	softc->vrtalloc = NULL;
	softc->vrtmaps = 0;
	softc->vrtflag = 0;

#ifdef	XXXppc
#ifdef DEBUG
	softc->adpinfo.pad1 = P9000_VADDR_COLOR + P9000_FB_SZ;
#endif
#endif	/* XXXppc */

	/*
	 * get monitor attributes
	 */
	softc->moninfo.mon_type = getprop(devi, "montype", 0);
	softc->moninfo.pixfreq = getprop(devi, "pixfreq", 929405);
	softc->moninfo.hfreq = getprop(devi, "hfreq", 61795);
	softc->moninfo.vfreq = getprop(devi, "vfreq", 66);
	softc->moninfo.hfporch = getprop(devi, "hfporch", 32);
	softc->moninfo.vfporch = getprop(devi, "vfporch", 2);
	softc->moninfo.hbporch = getprop(devi, "hbporch", 192);
	softc->moninfo.vbporch = getprop(devi, "vbporch", 31);
	softc->moninfo.hsync = getprop(devi, "hsync", 128);
	softc->moninfo.vsync = getprop(devi, "vsync", 4);

	softc->fbpfnum = hat_getkpfnum((caddr_t)softc->_fb);

	softc->chiprev = (softc->regs->p9000_sysconfig
			    & P9000_SYSCONF_VERSION_MASK);

	p9000_reset(softc);

#ifdef	XXXppc
	/* attach interrupt, notice the dance... see 1102427 */
	if (ddi_add_intr(devi, 0, &softc->iblock_cookie, 0,
		(u_int (*)()) nulldev, (caddr_t)0) != DDI_SUCCESS) {
	    DEBUGF(2, (CE_CONT, "p9000_attach%d add_intr failed\n", unit));
	    (void) p9000_detach(devi, DDI_DETACH);
	    return (DDI_FAILURE);
	}

	mutex_init(&softc->interlock, "p9000_interlock", MUTEX_DRIVER,
	    softc->iblock_cookie);
	mutex_init(&softc->mutex,
		    "p9000_softc_mtx", MUTEX_DRIVER, softc->iblock_cookie);
	cv_init(&softc->vrtsleep,
		    "p9000_vrt_wait", CV_DRIVER, softc->iblock_cookie);

	ddi_remove_intr(devi, 0, softc->iblock_cookie);

	if (ddi_add_intr(devi, 0, &softc->iblock_cookie, 0,
		    p9000_intr, (caddr_t)softc) != DDI_SUCCESS) {
	    DEBUGF(2, (CE_CONT,
		"p9000_attach%d add_intr failed\n", unit));
	    (void) p9000_detach(devi, DDI_DETACH);
	    return (DDI_FAILURE);
	}
#else
	/* NYI:  no interrupt handling yet */
#endif	/* XXXppc */

	/*
	 * Initialize hardware colormap and software colormap images. It might
	 * make sense to read the hardware colormap here.
	 */
	p9000_reset_cmap(softc->cmap_rgb, CMAP_ENTRIES);
	p9000_reset_cmap(softc->omap_rgb, 2);
	p9000_update_cmap(softc, (u_int) _ZERO_, CMAP_ENTRIES);
	p9000_update_cmap(softc, (u_int) _ZERO_, (u_int) _ZERO_);

	softc->regs->p9000_sysconfig |=
		P9000_SYSCONF_PIXEL_SWAP_HALF | P9000_SYSCONF_PIXEL_SWAP_BYTE;
	FAIRWAY_REGS(softc->regs)->ramdac.command_2 =
		(FAIRWAY_REGS(softc->regs)->ramdac.command_2
		    & ~BT485_CURSOR_MODE_SELECT)
		| BT485_CURSOR_DISABLED;

	DEBUGF(2, (CE_CONT,
	    "p9000_attach%d just before create_minor node\n", unit));
	sprintf(name, "p9000_%d", unit);
	if (ddi_create_minor_node(devi, name, S_IFCHR,
			    unit, DDI_NT_DISPLAY, NULL) == DDI_FAILURE) {
	    ddi_remove_minor_node(devi, NULL);
	    DEBUGF(2, (CE_CONT,
		"p9000_attach%d create_minor node failed\n", unit));
	    return (DDI_FAILURE);
	}
	ddi_report_dev(devi);

	cmn_err(CE_CONT,
	    "?p9000%d: screen %dx%d, %s buffered, %ld%sM mappable, rev %u\n",
	    unit, w, h, softc->adpinfo.hdb_capable ? "double" : "single",
	    (long)softc->adpinfo.vmsize * P9000_MEMSIZE_SCALE / 1024 / 1024,
	    ((long)softc->adpinfo.vmsize*P9000_MEMSIZE_SCALE/512/1024) % 2
		? ".5" : "",
	    softc->chiprev);

	softc->pvt_ctx = NULL;

	/*
	 * Initialize power management bookkeeping; components are created idle
	 */
	if (pm_create_components(devi, 2) == DDI_SUCCESS) {
		pm_busy_component(devi, 0);
		pm_set_normal_power(devi, 0, 1);
		pm_set_normal_power(devi, 1, 255);
	} else {
		return (DDI_FAILURE);
	}

	/* Needed so ltem actually prints to the console */
	(void) ddi_prop_create(makedevice(DDI_MAJOR_T_UNKNOWN, unit),
	    devi, DDI_PROP_CANSLEEP, DDI_KERNEL_IOCTL, NULL, 0);

	return (DDI_SUCCESS);
}

static int
p9000_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	int instance = ddi_get_instance(devi);
	register struct p9000_softc *softc = getsoftc(instance);
#ifdef	XXXppc
	caddr_t fb_ndvram;
#endif

	DEBUGF(1, (CE_CONT, "p9000_detach softc=%x, devi=0x%x\n", softc, devi));

	switch (cmd) {
	case DDI_DETACH:
	    break;

	case DDI_SUSPEND:
#ifdef	XXXppc
	    if (softc == NULL)
		    return (DDI_FAILURE);
	    if (softc->p9000_suspended)
		    return (DDI_FAILURE);

	    mutex_enter(&softc->mutex);

	    if (softc->curctx) {
		    /* Save non display RAM */
		    softc->ndvramsz =
			(softc->adpinfo.vmsize * P9000_MEMSIZE_SCALE)
			- (softc->_w * softc->_h);
		    if ((softc->ndvram = kmem_alloc(softc->ndvramsz,
			KM_NOSLEEP)) == NULL) {
			    mutex_exit(&softc->mutex);
			    return (DDI_FAILURE);
		    }
		    if (ddi_map_regs(devi, P9000_REG_RNUMBER, &fb_ndvram,
			P9000_ADDR_COLOR + softc->_w * softc->_h,
			softc->ndvramsz) == -1) {
			    kmem_free(softc->ndvram, softc->ndvramsz);
			    mutex_exit(&softc->mutex);
			    return (DDI_FAILURE);
		    }
		    bcopy(fb_ndvram, softc->ndvram, softc->ndvramsz);
		    ddi_unmap_regs(devi, P9000_REG_RNUMBER, &fb_ndvram,
			P9000_ADDR_COLOR + softc->_w * softc->_h,
			softc->ndvramsz);

		    /* Save other frame buffer state */
		    (void) p9000_cntxsave(S_FBC(softc), S_TEC(softc),
			softc->curctx);
	    }
	    softc->p9000_suspended = 1;
	    mutex_exit(&softc->mutex);
#else
	    /* NYI:  DDI_SUSPEND not implemented */
#endif	/* XXXppc */
	    return (DDI_SUCCESS);

	default:
	    return (DDI_FAILURE);
	}

#ifdef	XXXppc
	/* shut off video if not console */

	if (!softc->mapped_by_prom)
	    p9000_set_video(softc, 0);

	mutex_enter(&softc->mutex);
	p9000_int_disable(softc);
	mutex_exit(&softc->mutex);

	ddi_remove_intr(devi, 0, softc->iblock_cookie);

	if (softc->fbctec)
	    ddi_unmap_regs(devi, P9000_REG_RNUMBER,
		&softc->fbctec, P9000_ADDR_FBC, P9000_FBCTEC_SZ);
	if (softc->cmap)
	    ddi_unmap_regs(devi, P9000_REG_RNUMBER,
		&softc->cmap, P9000_ADDR_CMAP, P9000_CMAP_SZ);
	if (softc->fhcthc)
	    ddi_unmap_regs(devi, P9000_REG_RNUMBER,
		&softc->fhcthc, P9000_ADDR_FHC, P9000_FHCTHC_SZ);
	if (softc->rom)
	    ddi_unmap_regs(devi, P9000_REG_RNUMBER,
		&softc->rom, softc->addr_rom, P9000_ROM_SZ);
	if (softc->dhc)
	    ddi_unmap_regs(devi, P9000_REG_RNUMBER,
		&softc->dhc, P9000_ADDR_DHC, P9000_DHC_SZ);
	if (softc->alt)
	    ddi_unmap_regs(devi, P9000_REG_RNUMBER,
		&softc->alt, P9000_ADDR_ALT, P9000_ALT_SZ);
	/* TODO: uart (future) */
#else
	/* NYI:  skipped various mapping and interrupt things */
#endif	/* XXXppc */

	if (softc->regs_mapped)
	    ddi_regs_map_free(&softc->regs_handle);

	if (softc->vrtalloc != NULL)
	    kmem_free(softc->vrtalloc, pagesize * 2);

	mutex_destroy(&softc->mutex);

	cv_destroy(&softc->vrtsleep);

	ASSERT(softc->curctx == NULL);

	/* free softc struct */
	(void) ddi_soft_state_free(p9000_softc_head, instance);

	pm_destroy_components(devi);
	return (DDI_SUCCESS);
}

static int
p9000_power(dev_info_t *dip, int cmpt, int level)
{
	register struct p9000_softc *softc;

	if (cmpt != 1 || 0 > level || level > 255 ||
	    !(softc = (struct p9000_softc *)ddi_get_driver_private(dip)))
		return (DDI_FAILURE);

#if defined(lint)
	(void) softc;
#endif
	if (level) {
		/*EMPTY*/
		p9000_set_sync(softc, FBVIDEO_ON);
		p9000_set_video(softc, FBVIDEO_ON);
	} else {
		/*EMPTY*/
		p9000_set_video(softc, FBVIDEO_OFF);
		p9000_set_sync(softc, FBVIDEO_OFF);
	}

	(void) ddi_power(dip, cmpt, level);

	return (DDI_SUCCESS);
}

/* ARGSUSED */
static int
p9000_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	register int instance, error = DDI_SUCCESS;
	register struct p9000_softc *softc;

	instance = getminor((dev_t)arg);

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
	    if ((softc = getsoftc(instance)) == NULL) {
		error = DDI_FAILURE;
	    } else {
		*result = (void *) softc->devi;
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

/*ARGSUSED*/
static
p9000_open(dev_t *devp, int flag, int otyp, cred_t *cred)
{
	int	unit = getminor(*devp);
	struct	p9000_softc *softc = getsoftc(unit);
	int	error = 0;

#ifdef	XXXppc
	DEBUGF(2,
	    (CE_CONT, "p9000_open(%d), mem used=%d\n", unit, total_memory));
#else
	DEBUGF(2, (CE_CONT, "p9000_open(%d)\n", unit));
#endif

	/*
	 * is this gorp necessary?
	 */
	if (otyp != OTYP_CHR) {
	    error = EINVAL;
	} else
	if (softc == NULL) {
	    error = ENXIO;
	}

	return (error);
}

/*ARGSUSED*/
static
int
p9000_close(dev_t dev, int flag, int otyp, cred_t *cred)
{
	int    unit = getminor(dev);
	struct p9000_softc *softc = getsoftc(unit);
	int	error = 0;

#ifdef	XXXppc
	DEBUGF(2, (CE_CONT, "p9000_close(%d, %d, %d), mem used=%d\n",
	    unit, flag, otyp, total_memory));
#else
	DEBUGF(2, (CE_CONT, "p9000_close(%d, %d, %d)\n",
	    unit, flag, otyp));
#endif

	if (otyp != OTYP_CHR) {
	    error = EINVAL;
	} else
	if (softc == NULL) {
	    error = ENXIO;
	} else {
	    mutex_enter(&softc->mutex);
	    softc->cur.enable = 0;
	    softc->curctx = NULL;
	    p9000_reset(softc);
	    mutex_exit(&softc->mutex);
	}

	return (error);
}

/*ARGSUSED*/
static
p9000_mmap(dev_t dev, register off_t off, int prot)
{
	register struct p9000_softc *softc = getsoftc(getminor(dev));
	register caddr_t page;
	register int rval = 0;

	DEBUGF(off ? 5 : 1, (CE_CONT, "p9000_mmap(%d, 0x%x)\n",
	    getminor(dev), (u_int) off));

	/*
	 * The theory here is that mmap offset 0 should *always* lead to
	 * the frame buffer on a device that supports a simple frame
	 * buffer.  That way a single user-mode support module can
	 * support any frame buffer by using the ioctls to get parameters
	 * and to play with the color map.
	 *
	 * Other registers should be relocated elsewhere, perhaps to
	 * 0x80000000.  It doesn't matter exactly where - that's a
	 * matter for agreement between the kernel driver and the
	 * user-mode driver.
	 */
	if (off >= 0 && off < sizeof (softc->regs->p9000_frame_buffer))
	    rval = softc->fbpfnum + off / pagesize;
	else
	    page = (caddr_t)-1;

	if (rval == 0)
	    if (page != (caddr_t)-1)
		rval = hat_getkpfnum(page);
	    else
		rval = -1;

	DEBUGF(5, (CE_CONT, "p9000_mmap returning 0x%x\n", rval));

	return (rval);
}

/*ARGSUSED*/
static
p9000_ioctl(dev_t dev, int cmd, intptr_t data,
	int mode, cred_t *cred, int *rval)
{
	register struct p9000_softc *softc = getsoftc(getminor(dev));
	int    cursor_cmap;
	int    i;

	u_char *iobuf_cmap_red;
	u_char *iobuf_cmap_green;
	u_char *iobuf_cmap_blue;
	u_char *stack_cmap;

	struct fbcmap *cmap, cma;
	struct fbcursor cp;

	u_int   index;
	u_int   count;
	/*LINTED*/
	u_char *map;
	u_int   entries;
	int	err;
	static char not_init[] =
		"p9000_ioctl: Console has not been initialized";
	static char kernel_only[] =
		"p9000_ioctl: %s is a kernel only ioctl";
	static char kernel_only2[] =
		"p9000_ioctl: %s and %s are kernel only ioctls";

	DEBUGF(3, (CE_CONT, "p9000_ioctl(%d, 0x%x)\n", getminor(dev), cmd));

	/* default to updating normal colormap */
	cursor_cmap = 0;

	switch (cmd) {

	case VIS_GETIDENTIFIER:

	    if (ddi_copyout((caddr_t)&p9000_ident,
			    (caddr_t)data,
			    sizeof (struct vis_identifier),
			    mode))
		return (EFAULT);
	    break;

	case VIS_DEVINIT:
	    if (!(mode & FKIOCTL)) {
		cmn_err(CE_CONT, kernel_only, "VIS_DEVINIT");
		return (ENXIO);
	    }

	    if ((err = p9000_devinit(dev, (struct vis_devinit *)data)) != 0) {
		cmn_err(CE_CONT, "p9000ioctl: Could not initialize console");
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
	    p9000_cons_free(softc->conspriv);
	    softc->conspriv = NULL;

	    break;

	case VIS_CONSCOPY:	/* copy */
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

	    p9000_cons_copy(softc->conspriv, (struct vis_conscopy *)data);
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

	    p9000_cons_display(softc->conspriv, (struct vis_consdisplay *)data);
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

	    p9000_cons_cursor(softc->conspriv, (struct vis_conscursor *)data);
	    break;

	case VIS_GETCMAP:
	case VIS_PUTCMAP:
		/* fall through */

	case FBIOPUTCMAP:
	case FBIOGETCMAP:

	cmap_ioctl:

		if (ddi_copyin((caddr_t)data,
				(caddr_t)&cma,
				sizeof (struct fbcmap),
				mode))
			return (EFAULT);

		cmap = &cma;
		index = (u_int) cmap->index;
		count = (u_int) cmap->count;

		if (count == 0) {
			return (0);
		}
		if (cursor_cmap == 0) {
			switch (PIX_ATTRGROUP(index)) {

			case 0:
			case PIXPG_8BIT_COLOR:
			    map = softc->cmap_rgb;
			    entries = CMAP_ENTRIES;
			    break;
			default:
			    return (EINVAL);
			}
		} else {
			map = softc->omap_rgb;
			entries = 2;
		}

		if ((index &= PIX_ALL_PLANES) >= entries ||
			index + count > entries) {
			return (EINVAL);
		}
		/*
		 * Allocate memory for color map RGB entries.
		 */
		stack_cmap = kmem_alloc((CMAP_ENTRIES * 3), KM_SLEEP);

		iobuf_cmap_red = stack_cmap;
		iobuf_cmap_green = stack_cmap + CMAP_ENTRIES;
		iobuf_cmap_blue = stack_cmap + (CMAP_ENTRIES * 2);

		if (cmd == FBIOPUTCMAP || cmd == VIS_PUTCMAP) {
			int error;

			DEBUGF(3, (CE_CONT, "FBIOPUTCMAP\n"));

			if (error = ddi_copyin((caddr_t)cmap->red,
						(caddr_t)iobuf_cmap_red,
						count,
						mode)) {
				kmem_free(stack_cmap, (CMAP_ENTRIES * 3));
				return (error);
			}

			if (error = ddi_copyin((caddr_t)cmap->green,
						(caddr_t)iobuf_cmap_green,
						count,
						mode)) {
				kmem_free(stack_cmap, (CMAP_ENTRIES * 3));
				return (error);
			}
			if (error = ddi_copyin((caddr_t)cmap->blue,
						(caddr_t)iobuf_cmap_blue,
						count,
						mode)) {
				kmem_free(stack_cmap, (CMAP_ENTRIES * 3));
				return (error);
			}

			mutex_enter(&softc->mutex);
#ifdef	XXXppc
			map += index * 3;
#ifdef	XXXppc
			if (p9000_update_pending(softc))
			    p9000_int_disable(softc);
#endif

			/*
			 * Copy color map entries from stack to the color map
			 * table in the softc area.
			 */

			p9000_cmap_bcopy(iobuf_cmap_red, map++, count);
			p9000_cmap_bcopy(iobuf_cmap_green, map++, count);
			p9000_cmap_bcopy(iobuf_cmap_blue, map, count);

			/* cursor colormap update */
			if (entries < CMAP_ENTRIES)
			    count = 0;
			p9000_update_cmap(softc, index, count);
#ifdef	XXXppc
			p9000_int_enable(softc);
#endif
#else
			for (i = 0; i < count; i++) {
				p9000_cons_put_cmap(softc,
					index+i,
					iobuf_cmap_red[i],
					iobuf_cmap_green[i],
					iobuf_cmap_blue[i]);
			}
#endif
			mutex_exit(&softc->mutex);

		} else {
			/* FBIOGETCMAP or VIS_GETCMAP */
			DEBUGF(3, (CE_CONT, "FBIOGETCMAP\n"));

			mutex_enter(&softc->mutex);
#ifdef	XXXppc
			map += index * 3;

			/*
			 * Copy color map entries from soft area to
			 * local storage and prepare for a copyout
			 */

			p9000_cmap_bcopy(iobuf_cmap_red, map++, -count);
			p9000_cmap_bcopy(iobuf_cmap_green, map++, -count);
			p9000_cmap_bcopy(iobuf_cmap_blue, map, -count);
#else
			for (i = 0; i < count; i++) {
				p9000_cons_get_cmap(softc,
					index+i,
					&iobuf_cmap_red[i],
					&iobuf_cmap_green[i],
					&iobuf_cmap_blue[i]);
			}
#endif
			mutex_exit(&softc->mutex);

			if (ddi_copyout((caddr_t)iobuf_cmap_red,
					(caddr_t)cmap->red,
					count,
					mode)) {
				kmem_free(stack_cmap, (CMAP_ENTRIES * 3));
				return (EFAULT);
			}
			if (ddi_copyout((caddr_t)iobuf_cmap_green,
					(caddr_t)cmap->green,
					count,
					mode)) {
				kmem_free(stack_cmap, (CMAP_ENTRIES * 3));
				return (EFAULT);
			}
			if (ddi_copyout((caddr_t)iobuf_cmap_blue,
					(caddr_t)cmap->blue,
					count,
					mode)) {
				kmem_free(stack_cmap, (CMAP_ENTRIES * 3));
				return (EFAULT);
			}
		}
		kmem_free(stack_cmap, (CMAP_ENTRIES * 3));
		break;

	case FBIOSATTR: {
		struct fbsattr attr;

		if (ddi_copyin((caddr_t)data,
				(caddr_t)&attr,
				sizeof (attr),
				mode))
			return (EFAULT);
		DEBUGF(3, (CE_CONT, "FBIOSATTR, type=%d\n", attr.emu_type));
		if (attr.emu_type != -1)
			switch (attr.emu_type) {

			case FBTYPE_SUN3COLOR:
			case FBTYPE_SUN4COLOR:
			case FBTYPE_SUN2BW:
			case FBTYPE_SUNFAST_COLOR:
				    mutex_enter(&softc->mutex);
				    softc->emulation = attr.emu_type;
				    mutex_exit(&softc->mutex);
				    break;
			default:
				    return (EINVAL);
		}
		/* ignore device-dependent stuff */
	}
	break;

	case FBIOGATTR: {
		struct fbgattr attr;

		DEBUGF(3, (CE_CONT, "FBIOGATTR, emu_type=%d\n",
			softc->emulation));
		bcopy((caddr_t)&p9000_attr, (caddr_t)&attr, sizeof (attr));
		mutex_enter(&softc->mutex);
		attr.fbtype.fb_type = softc->emulation;
		attr.fbtype.fb_width = softc->_w;
		attr.fbtype.fb_height = softc->_h;
		/* XXX not quite like a cg4 */
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

	/*
	 * always claim to be a cg4 if they call this ioctl.  This is to
	 * support older software which was staticly-linked before p9000 was
	 * invented, and to support newer software which has come to expect
	 * this behavior.
	 */
	case FBIOGTYPE: {
		struct fbtype fb;

		mutex_enter(&softc->mutex);

		bcopy((caddr_t)&p9000_attr.fbtype,
			(caddr_t)&fb,
			sizeof (struct fbtype));
		DEBUGF(3, (CE_CONT, "FBIOGTYPE\n"));
		fb.fb_type = FBTYPE_SUN4COLOR;
		fb.fb_width = softc->_w;
		fb.fb_height = softc->_h;
		/* XXX not quite like a cg4 */
		fb.fb_size = softc->size;

		mutex_exit(&softc->mutex);

		if (ddi_copyout((caddr_t)&fb,
				(caddr_t)data,
				sizeof (struct fbtype),
				mode))
			return (EFAULT);
	}
	break;

	case FBIOGPIXRECT: {

		struct fbpixrect result;

		DEBUGF(3, (CE_CONT, "FBIOGPIXRECT\n"));
		mutex_enter(&softc->mutex);

		result.fbpr_pixrect = &softc->pr;
		/* initialize pixrect and private data */
		softc->pr.pr_ops = &p9000_pr_ops;
		/* pr_size set in attach */
		softc->pr.pr_depth = 8;
		softc->pr.pr_data = (caddr_t)&softc->prd;

		/* md_linebytes, md_image set in attach */
		/* md_offset already zero */
		softc->prd.mpr.md_primary = getminor(dev);
		softc->prd.mpr.md_flags = MP_DISPLAY | MP_PLANEMASK;
		softc->prd.planes = 255;

		/* enable video */
		p9000_set_video(softc, _ONE_);

		mutex_exit(&softc->mutex);

		if (ddi_copyout((caddr_t)&result,
				(caddr_t)data,
				sizeof (struct fbpixrect),
				mode))
			return (EFAULT);
		break;
	}

	case FBIOSVIDEO:

		DEBUGF(3, (CE_CONT, "FBIOSVIDEO\n"));
		if (ddi_copyin((caddr_t)data,
				(caddr_t)&i,
				sizeof (int),
				mode))
			return (EFAULT);
		mutex_enter(&softc->mutex);
		p9000_set_video(softc, i & FBVIDEO_ON);
		mutex_exit(&softc->mutex);
		break;

	case FBIOGVIDEO:

		DEBUGF(3, (CE_CONT, "FBIOGVIDEO\n"));
		mutex_enter(&softc->mutex);
#ifdef	XXXppc
		i = p9000_get_video(softc) ? FBVIDEO_ON : FBVIDEO_OFF;
#else
		/* NYI:  FBIOGVIDEO not implemented */
		i = FBVIDEO_ON;
#endif
		mutex_exit(&softc->mutex);

		if (ddi_copyout((caddr_t)&i,
				(caddr_t)data,
				sizeof (int),
				mode))
			return (EFAULT);
		break;

	/* informational ioctls */

	case FBIOGXINFO:
		if (ddi_copyout((caddr_t)&softc->adpinfo,
				(caddr_t)data,
				sizeof (softc->adpinfo),
				mode))
			return (EFAULT);
		return (0);

	case FBIOMONINFO:
		if (ddi_copyout((caddr_t)&softc->moninfo,
				(caddr_t)data,
				sizeof (struct mon_info),
				mode))
			return (EFAULT);
		return (0);

	/* vertical retrace interrupt */

	case FBIOVERTICAL:

		mutex_enter(&softc->mutex);
		softc->vrtflag |= P9000VRTIOCTL;
#ifdef	XXXppc
		p9000_int_enable(softc);
#endif
		cv_wait(&softc->vrtsleep, &softc->mutex);
		mutex_exit(&softc->mutex);
		return (0);

	case FBIOVRTOFFSET:

#ifdef	XXXppc
		i = P9000_VADDR_VRT;

		if (ddi_copyout((caddr_t)&i,
				(caddr_t)data,
				sizeof (int),
				mode))
			return (EFAULT);
		return (0);
#else
		/* NYI:  FBIOVRTOFFSET not implemented */
		return (EINVAL);
#endif

	/* HW cursor control */
	case FBIOSCURSOR: {

		int    set;
		int    cbytes;
		u_long stack_image[32], stack_mask[32];

		if (ddi_copyin((caddr_t)data,
				(caddr_t)&cp,
				sizeof (struct fbcursor),
				mode))
			return (EFAULT);

		set = cp.set;

		/* compute cursor bitmap bytes */
		cbytes = cp.size.y * sizeof (softc->cur.image[0]);
		if (set & FB_CUR_SETSHAPE) {

			if ((u_int) cp.size.x > 32 || (u_int) cp.size.y > 32) {
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

		p9000_setcurpos(softc);

		if (set & FB_CUR_SETSHAPE) {

			if (cp.image != NULL) {
				bzero((caddr_t)softc->cur.image,
					sizeof (softc->cur.image));
				bcopy((caddr_t)&stack_image,
					(caddr_t)softc->cur.image,
					cbytes);
			}
			if (cp.mask != NULL) {
				bzero((caddr_t)softc->cur.mask,
					sizeof (softc->cur.mask));
				bcopy((caddr_t)&stack_mask,
					(caddr_t)softc->cur.mask,
					cbytes);
			}
			/* load into hardware */
			softc->cur.size = cp.size;
			p9000_setcurshape(softc);
		}
		mutex_exit(&softc->mutex);
		/* load colormap */
		if (set & FB_CUR_SETCMAP) {
			cursor_cmap = 1;
			cmd = FBIOPUTCMAP;
			data = (int)&cp.cmap;
			mode |= FKIOCTL;
			goto cmap_ioctl;
		}
	}
	break;

	case FBIOGCURSOR: {
		int    cbytes;
		u_long stack_image[32], stack_mask[32];

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
		cp.cmap.count = 2;

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
			data = (int)&((struct fbcursor *)data)->cmap;
			goto cmap_ioctl;
		}
	}
	break;

	case FBIOSCURPOS: {

		struct fbcurpos stack_curpos;	/* cursor position */

		if (ddi_copyin((caddr_t)data,
				(caddr_t)&stack_curpos,
				sizeof (struct fbcurpos),
				mode))
			return (EFAULT);

		mutex_enter(&softc->mutex);
		bcopy((caddr_t)&stack_curpos, (caddr_t)&softc->cur.pos,
		    sizeof (struct fbcurpos));
		p9000_setcurpos(softc);
		mutex_exit(&softc->mutex);
	}
	break;

	case FBIOGCURPOS: {
		struct fbcurpos stack_curpos;	/* cursor position */

		mutex_enter(&softc->mutex);
		bcopy((caddr_t)&softc->cur.pos, (caddr_t)&stack_curpos,
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

#if	DRIVER_DEBUG
	case 255:
		p9000_debug = (int)data;
		if (p9000_debug == -1)
		    p9000_debug = DRIVER_DEBUG;
		cmn_err(CE_CONT, "p9000_debug is now %d\n", p9000_debug);
		break;
#endif

	default:
		return (ENOTTY);
	}				/* switch (cmd) */

	return (0);
}

#ifdef	XXXppc
static  u_int
p9000_intr(caddr_t arg)
{
	register struct p9000_softc *softc = (struct p9000_softc *)arg;
	volatile u_long *in;
	volatile u_long *out;
	volatile u_long  tmp;

	DEBUGF(7, (CE_CONT,
		"p9000_intr: softc=%x, vrtflag=%x\n", softc, softc->vrtflag));

	if (!p9000_int_pending(softc)) {
	    return (DDI_INTR_UNCLAIMED);	/* nope, not mine */
	}
	mutex_enter(&softc->mutex);
	mutex_enter(&softc->interlock);

	if (!(p9000_update_pending(softc) || (softc)->vrtflag)) {
	    /* TODO catch stray interrupts? */
	    p9000_int_disable_intr(softc);
	    mutex_exit(&softc->interlock);
	    mutex_exit(&softc->mutex);
	    return (DDI_INTR_CLAIMED);
	}
	if (softc->vrtflag & P9000VRTCTR) {
	    if (softc->vrtmaps == 0) {
		softc->vrtflag &= ~P9000VRTCTR;
	    } else
		*softc->vrtpage += 1;
	}
	if (softc->vrtflag & P9000VRTIOCTL) {
	    softc->vrtflag &= ~P9000VRTIOCTL;
	    cv_broadcast(&softc->vrtsleep);
	}
	if (p9000_update_pending(softc)) {
	    volatile struct p9000_cmap *cmap = S_CMAP(softc);
	    LOOP_T  count = softc->cmap_count;

	    /* load cursor color map */
	    if (softc->omap_update) {
		in = &softc->omap_image.omap_long[0];
		out = (u_long *) & cmap->omap;

		/* background color */
		cmap->addr = 1 << 24;
		tmp = in[0];
		*out = tmp;
		*out = tmp <<= 8;
		*out = tmp <<= 8;

		/* foreground color */
		cmap->addr = 3 << 24;
		*out = tmp <<= 8;
		tmp = in[1];
		*out = tmp;
		*out = tmp <<= 8;
	    }
	    /* load main color map */
	    if (count) {
		LOOP_T  index = softc->cmap_index;

		in = &softc->cmap_image.cmap_long[0];
		out = (u_long *) & cmap->cmap;

		/* count multiples of 4 RGB entries */
		count = (count + (index & 3) + 3) >> 2;

		/* round index to 4 entry boundary */
		index &= ~3;

		cmap->addr = index << 24;
		PTR_INCR(u_long *, in, index * 3);

		/* copy 4 bytes (4/3 RGB entries) per loop iteration */
		count *= 3;
		PR_LOOPV(count,
			tmp = *in++;
		*out = tmp;
		*out = tmp <<= 8;
		*out = tmp <<= 8;
		*out = tmp <<= 8);

		softc->cmap_count = 0;
	    }
	    softc->omap_update = 0;
	}
	p9000_int_disable_intr(softc);
	if (softc->vrtflag)
	    p9000_int_enable(softc);

	mutex_exit(&softc->interlock);
	mutex_exit(&softc->mutex);
	return (DDI_INTR_CLAIMED);
}
#endif	/* XXXppc */

static int
p9000_devinit(dev_t dev, struct vis_devinit *data)
{
	struct p9000_softc		*softc;
	struct p9000_console_private	*private;
	int				unit;

	unit = getminor(dev);
	if (!(softc = getsoftc(unit)))
		return (NULL);

	private = (struct p9000_console_private *)
	    kmem_alloc(sizeof (struct p9000_console_private), KM_SLEEP);

	if (private == NULL)
		return (ENOMEM);

	/* initialize the private data structure */
	private->softc = softc;
	mutex_init(&private->pmutex, "p9000 console lock", MUTEX_DRIVER, NULL);
	private->viscursor = 0;

	/* initialize console instance */
	data->version = VIS_CONS_REV;
	data->height = softc->_h;
	data->width = softc->_w;
	data->depth = 8;
	data->size = softc->size;
	data->linebytes = softc->_linebytes;
	data->mode = (short)VIS_PIXEL;

	mutex_enter(&softc->mutex);
	softc->conspriv = private;
	mutex_exit(&softc->mutex);

	p9000_set_video(softc, FBVIDEO_ON);

	return (0);
}

/* The console routines, and may the Lord Have Mercy On Us All. */

static
void
clip(struct p9000_softc *softc, struct p9000_rect *rp)
{
	/* partial clip to screen */

	rp->h = (rp->y + rp->h > softc->_h) ? softc->_h - rp->y : rp->h;
	rp->w = (rp->x + rp->w > softc->_w) ? softc->_w - rp->x : rp->w;
}


static void
p9000_cons_put_cmap(
	struct p9000_softc *softc,
	int index,
	unsigned char r,
	unsigned char g,
	unsigned char b)
{
	volatile struct p9000 *regs = softc->regs;

	/* outb() does the eieio before the store, so we do too */
	eieio();
	FAIRWAY_REGS(regs)->ramdac.palette_write_index = index;
	eieio();
	FAIRWAY_REGS(regs)->ramdac.palette_data = r;
	eieio();
	FAIRWAY_REGS(regs)->ramdac.palette_data = g;
	eieio();
	FAIRWAY_REGS(regs)->ramdac.palette_data = b;
}

static void
p9000_cons_get_cmap(
	struct p9000_softc *softc,
	int index,
	unsigned char *r,
	unsigned char *g,
	unsigned char *b)
{
	volatile struct p9000 *regs = softc->regs;

	eieio();
	FAIRWAY_REGS(regs)->ramdac.palette_read_index = index;
	eieio();
	*r = FAIRWAY_REGS(regs)->ramdac.palette_data;
	eieio();
	*g = FAIRWAY_REGS(regs)->ramdac.palette_data;
	eieio();
	*b = FAIRWAY_REGS(regs)->ramdac.palette_data;
}

static
void
p9000_cons_free(struct p9000_console_private *cp)
{
	mutex_destroy(&cp->pmutex);

	kmem_free(cp, sizeof (struct p9000_console_private));
}

static void
p9000_cons_copy(struct p9000_console_private *cp, struct vis_conscopy *pma)
{
	register unsigned char	*srcp;
	register unsigned char	*dstp;
	register unsigned char	*sp;
	register unsigned char	*dp;
	register int		i;
	struct p9000_rect	r;

	r.x = pma->t_col;
	r.y = pma->t_row;
	r.w = pma->e_col - pma->s_col + 1;
	r.h = pma->e_row - pma->s_row + 1;

	clip(cp->softc,  &r);

	switch (pma->direction) {
	case VIS_COPY_FORWARD:
		dstp = (unsigned char *)cp->softc->_fb + pma->t_row *
			cp->softc->_w;
		srcp = (unsigned char *)cp->softc->_fb + pma->s_row *
			cp->softc->_w;

		while (r.h--) {
#ifdef	XXXppc
		    bcopy((char *)srcp + pma->s_col,
			(char *)dstp + pma->t_col, r.w);
#else
		    p9000_smart_bcopy((char *)srcp + pma->s_col,
			(char *)dstp + pma->t_col, r.w);
#endif
		    dstp += cp->softc->_w;
		    srcp += cp->softc->_w;
		}
	break;

	case VIS_COPY_BACKWARD:
		dstp = (unsigned char *)cp->softc->_fb +
			(pma->t_row + r.h - 1) * cp->softc->_w;
		srcp = (unsigned char *)cp->softc->_fb + pma->e_row *
			cp->softc->_w;

		while (r.h--) {
			if (((unsigned char *)dstp + pma->t_col) <=
			    ((unsigned char *)srcp + pma->s_col + r.w)) {
				sp = (unsigned char *)srcp + pma->s_col +
					r.w - 1;
				dp = (unsigned char *)dstp + pma->t_col +
					r.w - 1;
				for (i = 0; i < r.w; i++)
					*dp-- = *sp--;
			} else {
#ifdef	XXXppc
				bcopy((char *)srcp + pma->s_col,
				    (char *)dstp + pma->t_col, r.w);
#else
				p9000_smart_bcopy((char *)srcp + pma->s_col,
				    (char *)dstp + pma->t_col, r.w);
#endif
			}
			dstp -= cp->softc->_w;
			srcp -= cp->softc->_w;
		}
		break;
	}
}

static void
p9000_cons_display(struct p9000_console_private *cp,
    struct vis_consdisplay *pda)
{
	register unsigned char	*scrcp;
	register unsigned char	*imgcp;
	struct p9000_rect	r;

	r.x = pda->col;
	r.y = pda->row;
	r.w = pda->width;
	r.h = pda->height;

	clip(cp->softc,  &r);
	scrcp = (unsigned char *)cp->softc->_fb + r.y * cp->softc->_w;

	imgcp = pda->data;
	while (r.h--) {
#ifdef	XXXppc
		bcopy((char *)imgcp, (char *)scrcp + r.x, r.w);
#else
		p9000_smart_bcopy((char *)imgcp, (char *)scrcp + r.x, r.w);
#endif
		imgcp += r.w;
		scrcp += cp->softc->_w;
	}
}

static void
p9000_cons_cursor(struct p9000_console_private *cp, struct vis_conscursor *pca)
{
	register unsigned char	*scrcp;
	register unsigned char	*rp;
	register int i;

	scrcp = (unsigned char *)cp->softc->_fb + pca->row * cp->softc->_w;

	switch (pca->action) {
	case VIS_HIDE_CURSOR:
	    if (cp->viscursor == 0)
		return;
	    cp->viscursor = 0;
	    break;
	case VIS_DISPLAY_CURSOR:
	    if (cp->viscursor)
		return;
	    cp->viscursor = 1;
	    break;
	}

	i = pca->height;
	while (i--) {
	    for (rp = scrcp + pca->col;
		rp < scrcp + pca->col + pca->width; rp++)
		    if (*rp == pca->fg_color.eight)
			*rp = pca->bg_color.eight;
		    else
		    if (*rp == pca->bg_color.eight)
			*rp = pca->fg_color.eight;
	    scrcp += cp->softc->_w;
	}
}

/*
 * Initialize a colormap: background = white, all others = black
 */
static void
p9000_reset_cmap(volatile u_char *cmap, register u_int entries)
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
p9000_update_cmap(struct p9000_softc *softc, u_int index, u_int count)
{
	u_int   high, low;

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

#ifdef	XXXppc
/*
 * Copy colormap entries between red, green, or blue array and
 * interspersed rgb array.
 *
 * count > 0 : copy count bytes from buf to rgb
 * count < 0 : copy -count bytes from rgb to buf
 */
static void
p9000_cmap_bcopy(register u_char *bufp, register u_char *rgb, u_int count)
{
	register LOOP_T rcount = count;

	if (--rcount >= 0)
	    PR_LOOPVP(rcount,
			*rgb = *bufp++;
			rgb += 3);
	else {
	    rcount = -rcount - 2;
	    PR_LOOPVP(rcount,
			*bufp++ = *rgb;
			rgb += 3);
	}
}
#endif

/*
 * enable/disable/update HW cursor
 */
/*ARGSUSED*/
static void
p9000_setcurpos(struct p9000_softc *softc)
{
#ifdef	XXXppc
	volatile struct thc *thc = S_THC(softc);

	thc->l_thc_cursor = softc->cur.enable ?
	    (((softc->cur.pos.x - softc->cur.hot.x) << 16) |
	    ((softc->cur.pos.y - softc->cur.hot.y) & 0xffff)) :
		P9000_CURSOR_OFFPOS;
#else
	/* NYI:  not implemented */
#endif
}

/*
 * load HW cursor bitmaps
 */
/*ARGSUSED*/
static void
p9000_setcurshape(struct p9000_softc *softc)
{
#ifdef	XXXppc
	u_long  tmp, edge = 0;
	volatile u_long *image, *mask, *hw;
	volatile struct thc *thc = S_THC(softc);
	int    i;

	/* compute right edge mask */
	if (softc->cur.size.x)
	    edge = (u_long) ~ 0 << (32 - softc->cur.size.x);

	image = softc->cur.image;
	mask = softc->cur.mask;
	hw = (u_long *) & thc->l_thc_cursora00;

	for (i = 0; i < 32; i++) {
	    hw[i] = (tmp = mask[i] & edge);
	    hw[i + 32] = tmp & image[i];
	}
#else
	/* NYI:  not implemented */
#endif
}

static void
p9000_reset(struct p9000_softc *softc)
{
#ifdef	XXXppc
	volatile struct thc *thc = S_THC(softc);

	/* disable HW cursor */
	thc->l_thc_cursor = P9000_CURSOR_OFFPOS;

	/* reinitialize TEC */
	{
	    volatile struct tec *tec = S_TEC(softc);

	    tec->l_tec_mv = 0;
	    tec->l_tec_clip = 0;
	    tec->l_tec_vdc = 0;
	}

	/* reinitialize FBC config register */
	{
	    volatile u_int  *fhc = S_FHC(softc);
	    u_int rev, conf;

	    rev = *fhc >> FHC_CONFIG_REV_SHIFT & FHC_CONFIG_REV_MASK;
	    if (rev <= 4) {

		/* PROM knows how to deal with LSC and above */
		/* rev == 0 : FBC 0 (not available to customers) */
		/* rev == 1 : FBC 1 */
		/* rev == 2 : FBC 2 */
		/* rev == 3 : Toshiba (never built) */
		/* rev == 4 : Standard Cell (not built yet) */
		/* rev == 5 : LSC rev 2 (buggy) */
		/* rev == 6 : LSC rev 3 */
		conf = *fhc & FHC_CONFIG_RES_MASK |
		    FHC_CONFIG_CPU_68020;

#if FBC_REV0
		/* FBC0: test window = 0, disable fast rops */
		if (rev == 0)
		    conf |= FHC_CONFIG_TEST |
			FHC_CONFIG_FROP_DISABLE;
		else
#endif	/* FBC_REV0 */

		    /* test window = 1K x 1K */
		    conf |= FHC_CONFIG_TEST |
			(10 + 1) << FHC_CONFIG_TESTX_SHIFT |
			(10 + 1) << FHC_CONFIG_TESTY_SHIFT;

		/* FBC[01]: disable destination cache */
		if (rev <= 1)
		    conf |= FHC_CONFIG_DST_DISABLE;

		*fhc = conf;
	    }
	}

	/* reprogram DAC to enable HW cursor use */
	{
	    volatile struct p9000_cmap *cmap = S_CMAP(softc);

	    /* command register */
	    cmap->addr = 6 << 24;

	    /* turn on CR1:0, overlay enable */
	    cmap->ctrl = cmap->ctrl | (0x3 << 24);
	}
#else
	/* NYI:  not implemented */
	p9000_setcurpos(softc);
#endif	/* XXXppc */
}

	/*
	 * This code is no longer used, since OBP proms now do all device
	 * initialization. Nevertheless, it is instructive and I'm going to
	 * keep it in as a comment, should anyone ever want to know how to
	 * do minimal device initialization. Note the c++ style embedded
	 * comments.
	 *
	 * p9000_init(softc)
	 *	struct p9000_softc *softc;
	 *{
	 *	// Initialize DAC
	 *	{
	 *	    register struct p9000_cmap *cmap = S_CMAP(softc);
	 *	    register char *p;
	 *
	 *	    static char dacval[] = {
	 *		4, 0xff,
	 *		5, 0,
	 *		6, 0x73,
	 *		7, 0,
	 *		0
	 *	    };
	 *
	 *	    // initialize DAC
	 *	    for (p = dacval; *p; p += 2) {
	 *		cmap->addr = p[0] << 24;
	 *		cmap->ctrl = p[1] << 24;
	 *	    }
	 *	}
	 *
	 *	// Initialize THC
	 *	{
	 *	    register struct thc *thc = S_THC(softc);
	 *	    int    vidon;
	 *
	 *	    vidon = thc_get_video(thc);
	 *	    thc->l_thc_hcmisc = THC_HCMISC_RESET | THC_HCMISC_INIT;
	 *	    thc->l_thc_hcmisc = THC_HCMISC_INIT;
	 *
	 *	    thc->l_thc_hchs = 0x010009;
	 *	    thc->l_thc_hchsdvs = 0x570000;
	 *	    thc->l_thc_hchd = 0x15005d;
	 *	    thc->l_thc_hcvs = 0x010005;
	 *	    thc->l_thc_hcvd = 0x2403a8;
	 *	    thc->l_thc_hcr = 0x00016b;
	 *
	 *	    thc->l_thc_hcmisc = THC_HCMISC_RESET | THC_HCMISC_INIT;
	 *	    thc->l_thc_hcmisc = THC_HCMISC_INIT;
	 *
	 *	    if (vidon)
	 *		thc_set_video(thc, _ONE_);
	 *
	 *	    DEBUGF(1, (CE_CONT, "TEC rev %d\n",
	 *			thc->l_thc_hcmisc >> THC_HCMISC_REV_SHIFT &
	 *			THC_HCMISC_REV_MASK));
	 *	}
	 *
	 *	//
	 *	// Initialize FHC for 1152 X 900 screen
	 *	//
	 *	{
	 *	    volatile u_int *fhc = S_FHC(softc), rev;
	 *
	 *	    rev = *fhc >> FHC_CONFIG_REV_SHIFT & FHC_CONFIG_REV_MASK;
	 *	    DEBUGF(1, (CE_CONT, "p9000_init: FBC rev %d\n", rev));
	 *
	 *	//
	 *	// FBC0: disable fast rops FBC[01]: disable destination cache
	 *	//
	 *	    *fhc = FHC_CONFIG_1152 |
	 *		FHC_CONFIG_CPU_68020 |
	 *		FHC_CONFIG_TEST |
	 *
	 *#if FBC_REV0
	 *	    (rev == 0 ? FHC_CONFIG_FROP_DISABLE : 0) |
	 *#endif
	 *
	 *	    (rev <= 1 ? FHC_CONFIG_DST_DISABLE : 0);
	 *	}
	 *}
	 */

/*
 * from here on down, is the lego segment driver.  this virtualizes the
 * lego register file by associating a register save area with each
 * mapping of the lego device (each lego segment).  only one of these
 * mappings is valid at any time; a page fault on one of the invalid
 * mappings saves off the current lego context, invalidates the current
 * valid mapping, restores the former register contents appropriate to
 * the faulting mapping, and then validates it.
 *
 * this implements a graphical context switch that is transparent to the user.
 *
 * the TEC and FBC contain the interesting context registers.
 *
 */

/*
 * Per-segment info:
 *	Some, but not all, segments are part of a context.
 *	Any segment that is a MAP_PRIVATE mapping to the TEC or FBC
 *	will be part of a unique context.  MAP_SHARED mappings are part
 *	of the shared context and all such programs must arbitrate among
 *	themselves to keep from stepping on each other's register settings.
 *	Mappings to the framebuffer may or may not be part of a context,
 *	depending on exact hardware type.
 */

#define	P9000MAP_SHARED	0x02	/* shared context */
#define	P9000MAP_VRT	0x04	/* vrt page */
#define	P9000MAP_FBCTEC	0X08	/* mapping includes fbc and/or tec */
#define	P9000MAP_FB	0X10	/* mapping includes framebuffer */

#define	P9000MAP_CTX	(P9000MAP_FBCTEC | P9000MAP_FB)	/* needs context */

static struct p9000map_pvt *
p9000_pvt_alloc(struct p9000_cntxt *ctx,
		u_int off,
		u_int len,
		struct p9000_softc *softc)
{
	struct p9000map_pvt *pvt;

	/*
	 * create the private data portion of the mapdev object
	 */
	pvt = (struct p9000map_pvt *)kmem_zalloc(sizeof (struct p9000map_pvt),
	    KM_SLEEP);
	pvt->offset  = off;
	pvt->len = len;
	pvt->context = ctx;
	pvt->softc = softc;

	/*
	 * Link this pvt into the list of associated pvt's for this
	 * context
	 */
	pvt->next = ctx->pvt;
	ctx->pvt = pvt;

	return (pvt);
}

/*
 * This routine is called through the cb_ops table to handle
 * the creation of lego (p9000) segments.
 */
/*ARGSUSED*/
static int
p9000_segmap(dev_t	dev,
	    off_t	off,
	    struct as	*as,
	    caddr_t	*addrp,
	    off_t	len,
	    u_int	prot,
	    u_int	maxprot,
	    u_int	flags,
	    cred_t	*cred)
{
	register struct p9000_softc *softc = getsoftc(getminor(dev));

	struct p9000_cntxt *ctx		= (struct p9000_cntxt *)NULL;
	struct p9000_cntxt *shared_ctx	= &softc->shared_ctx;
	struct p9000map_pvt *pvt;
	u_int	maptype = 0;
	u_int	ctxmap;
	int	error;

	DEBUGF(3, (CE_CONT, "segmap: off=%x, len=%x\n", off, len));
	mutex_enter(&softc->mutex);

	/*
	* we now support MAP_SHARED and MAP_PRIVATE:
	*
	* MAP_SHARED means you get the shared context which is the traditional
	* mapping method.
	*
	* MAP_PRIVATE means you get your very own LEGO context.
	*
	* Note that you can't get to here without asking for one or the other,
	* but not both.
	*/

	/* classify it */
	if (off + len >
	    (char *)softc->regs->p9000_frame_buffer - (char *)softc->regs &&
	    off < (char *)softc->regs->p9000_frame_buffer -
		(char *)softc->regs + sizeof (softc->regs->p9000_frame_buffer))
	    maptype |= P9000MAP_FB;

#ifdef	XXXppc
	/* decide if this segment is part of a context. */

	ctxmap = (softc->chiprev == 5) ?
		    (P9000MAP_FBCTEC|P9000MAP_FB) : P9000MAP_FBCTEC;
#else
	ctxmap = 0;
#endif

	if (maptype & ctxmap) {

	/*
	 * determine whether this is a shared mapping, or a private one.
	 * if shared, link it onto the shared list, if private, create a
	 * private LEGO context.
	 */

	    if (flags & MAP_SHARED) {	/* shared mapping */
		    ctx = shared_ctx;
		    ctx->flag = P9000MAP_CTX;
	    } else {
		    ctx = ctx_map_insert(softc, maptype);
		    ctx->flag |= maptype;
	    }

	    pvt = p9000_pvt_alloc(ctx, 0, len, softc);

		/*
		 * create the mapdev object
		 */
		error = ddi_mapdev(dev,
				    off,
				    as,
				    addrp,
				    len,
				    prot,
				    maxprot,
				    flags,
				    cred,
				    &p9000map_ops,
				    &pvt->handle,
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

	    error = ddi_segmap(dev,
				off,
				as,
				addrp,
				len,
				prot,
				maxprot,
				flags,
				cred);
	}

	mutex_exit(&softc->mutex);

	return (error);
}

/*
 * An access has been made to a context other than the current one
 */
/* ARGSUSED */
static int
p9000map_access(ddi_mapdev_handle_t handle, void *pvt, off_t offset)
{
#ifdef	XXXppc
	struct p9000map_pvt *p   = (struct p9000map_pvt *)pvt;
	struct p9000map_pvt *pvts;
	struct p9000_softc *softc = p->softc;
	volatile struct fbc *fbc;
	int err = 0;

	ASSERT(pvt);

	mutex_enter(&softc->mutex);

	/*
	 * Do we need to switch contexts?
	 */
	if (softc->curctx != p->context) {

		fbc = S_FBC(softc);

		/*
		 * If there's a current context, save it
		 */
		if (softc->curctx != (struct p9000_cntxt *)NULL) {
			/*
			 * Set mapdev for current context and all associated
			 * handles to intercept references to their addresses
			 */
			ASSERT(softc->curctx->pvt);
			for (pvts = softc->curctx->pvt; pvts != NULL;
			    pvts = pvts->next) {
				err =
				    ddi_mapdev_intercept(pvts->handle,
							pvts->offset,
							pvts->len);
				if (err)
				    return (err);
			}

			if (p9000_cntxsave(fbc, S_TEC(softc),
					softc->curctx) == 0) {
				DEBUGF(1, (CE_CONT,
				    "p9000: context save failed\n"));
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
		P9000DELAY(!(fbc->l_fbc_status & L_FBC_BUSY), P9000_FBC_WAIT);
		if (fbc->l_fbc_status & L_FBC_BUSY) {
			DEBUGF(1, (CE_CONT, "p9000: idle_p9000: status = %x\n",
			fbc->l_fbc_status));
			/*
			 * At this point we have no current context.
			 */
			softc->curctx = NULL;
			mutex_exit(&softc->mutex);
			return (-1);
		}

		DEBUGF(4, (CE_CONT, "loading context %x\n", p->context));

		if (p->context->flag & P9000MAP_FBCTEC)
			if (p9000_cntxrestore(fbc, S_TEC(softc),
						p->context) == 0) {
				DEBUGF(1, (CE_CONT,
				    "p9000: context restore failed\n"));
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

	ASSERT(p->context->pvt);
	for (pvts = p->context->pvt; pvts != NULL; pvts = pvts->next) {
		if ((err = ddi_mapdev_nointercept(pvts->handle,
		    pvts->offset, pvts->len)) != 0) {
			mutex_exit(&softc->mutex);
			return (err);
		}
	}

	mutex_exit(&softc->mutex);

	return (err);
#else
	/* NYI:  not implemented */
	return (DDI_FAILURE);
#endif
}

/* ARGSUSED */
static void
p9000map_free(ddi_mapdev_handle_t handle, void *pvt)
{
	struct p9000map_pvt *p   = (struct p9000map_pvt *)pvt;
	struct p9000map_pvt *pvts;
	struct p9000map_pvt *ppvts;
	struct p9000_cntxt *ctx   = p->context;
	struct p9000_softc *softc = p->softc;
	struct p9000_cntxt *shared_ctx	= &softc->shared_ctx;

	mutex_enter(&softc->mutex);

	/*
	 * Remove the pvt data
	 */
	ppvts = NULL;
	for (pvts = ctx->pvt; pvts != NULL; pvts = pvts->next) {
		if (pvts == pvt) {
			if (ppvts == NULL) {
				ctx->pvt = pvts->next;
			} else {
				ppvts->next = pvts->next;
			}
			kmem_free(pvt, sizeof (struct p9000map_pvt));
			break;
		}
		ppvts = pvts;
	}

	/*
	 * Remove the context if this is not the shared context and there are
	 * no more associated pvt's
	 */
	if ((ctx != shared_ctx) && (ctx->pvt == NULL)) {
		register struct p9000_cntxt *ctxptr;

		if (ctx == softc->curctx)
			softc->curctx = NULL;

		/*
		 * Scan private context list for entry to remove.
		 * Check first to see if it's the head of our list.
		 */
		if (softc->pvt_ctx == ctx) {
			softc->pvt_ctx = ctx->link;
			kmem_free(ctx, sizeof (struct p9000_cntxt));
		} else {
			for (ctxptr = softc->pvt_ctx; ctxptr != NULL;
				ctxptr = ctxptr->link) {
				if (ctxptr->link == ctx) {
					ctxptr->link = ctx->link;
					kmem_free(ctx,
						sizeof (struct p9000_cntxt));
				}
			}
		}
	}

	/*
	 * If the curctx is the shared context, and there are no
	 * more pvt's for the shared context, set the curctx to
	 * NULL to force a context switch on the next device access.
	 */
	if ((softc->curctx == shared_ctx) && (softc->curctx->pvt == NULL)) {
		softc->curctx = NULL;
	}

	mutex_exit(&softc->mutex);
}

/* ARGSUSED */
static int
p9000map_dup(ddi_mapdev_handle_t old_handle,
	    void *oldpvt,
	    ddi_mapdev_handle_t new_handle,
	    void **newpvt)
{
	struct p9000map_pvt *p   = (struct p9000map_pvt *)oldpvt;
	struct p9000_softc *softc = p->softc;
	struct p9000map_pvt *pvt;
	struct p9000_cntxt *ctx;

	mutex_enter(&softc->mutex);
	if (p->context != &softc->shared_ctx) {
		ctx = (struct p9000_cntxt *)
		    kmem_zalloc(sizeof (struct p9000_cntxt), KM_SLEEP);
		*ctx = *p->context;
		ctx->pvt = NULL;
	} else
		ctx = &softc->shared_ctx;

	pvt = p9000_pvt_alloc(ctx, 0, p->len, softc);

	pvt->handle = new_handle;
	*newpvt = pvt;

	if (p->context && (p->context->flag & P9000MAP_VRT)) {
	    softc->vrtflag |= P9000VRTCTR;
#ifdef	XXXppc
	    if (softc->vrtmaps == 0)
		p9000_int_enable(softc);
#endif
	    softc->vrtmaps++;
	}

	mutex_exit(&softc->mutex);
	return (0);
}

#ifdef	XXXppc
 * please don't mess with these defines... they may look like
 * a strange place for defines, but the context management code
 * wants them as they are. JMP
 *
 */
#undef	L_TEC_VDC_INTRNL0
#define	L_TEC_VDC_INTRNL0	0x8000
#undef	L_TEC_VDC_INTRNL1
#define	L_TEC_VDC_INTRNL1	0xa000

static int
p9000_cntxsave(fbc, tec, saved)
	volatile struct fbc *fbc;
	volatile struct tec *tec;
	struct p9000_cntxt *saved;
{
	int    dreg;		/* counts through the data registers */
	u_int  *dp;			/* points to a tec data register */

	DEBUGF(5, (CE_CONT, "saving registers for %d\n", saved->pid));

	CDELAY(!(fbc->l_fbc_status & L_FBC_BUSY), P9000_FBC_WAIT);
	if (fbc->l_fbc_status & L_FBC_BUSY) {
	    DEBUGF(1, (CE_CONT, "p9000: idle_p9000: status = %x\n",
			fbc->l_fbc_status));
	    return (0);
	}

	/*
	* start dumping stuff out.
	*/
	saved->fbc.status = fbc->l_fbc_status;
	saved->fbc.clipcheck = fbc->l_fbc_clipcheck;
	saved->fbc.misc = fbc->l_fbc_misc;
	saved->fbc.x0 = fbc->l_fbc_x0;
	saved->fbc.y0 = fbc->l_fbc_y0;
	saved->fbc.x1 = fbc->l_fbc_x1;
	saved->fbc.y1 = fbc->l_fbc_y1;
	saved->fbc.x2 = fbc->l_fbc_x2;
	saved->fbc.y2 = fbc->l_fbc_y2;
	saved->fbc.x3 = fbc->l_fbc_x3;
	saved->fbc.y3 = fbc->l_fbc_y3;
	saved->fbc.rasteroffx = fbc->l_fbc_rasteroffx;
	saved->fbc.rasteroffy = fbc->l_fbc_rasteroffy;
	saved->fbc.autoincx = fbc->l_fbc_autoincx;
	saved->fbc.autoincy = fbc->l_fbc_autoincy;
	saved->fbc.clipminx = fbc->l_fbc_clipminx;
	saved->fbc.clipminy = fbc->l_fbc_clipminy;
	saved->fbc.clipmaxx = fbc->l_fbc_clipmaxx;
	saved->fbc.clipmaxy = fbc->l_fbc_clipmaxy;
	saved->fbc.fcolor = fbc->l_fbc_fcolor;
	saved->fbc.bcolor = fbc->l_fbc_bcolor;
	saved->fbc.rasterop = fbc->l_fbc_rasterop;
	saved->fbc.planemask = fbc->l_fbc_planemask;
	saved->fbc.pixelmask = fbc->l_fbc_pixelmask;
	saved->fbc.pattalign = fbc->l_fbc_pattalign;
	saved->fbc.pattern0 = fbc->l_fbc_pattern0;
	saved->fbc.pattern1 = fbc->l_fbc_pattern1;
	saved->fbc.pattern2 = fbc->l_fbc_pattern2;
	saved->fbc.pattern3 = fbc->l_fbc_pattern3;
	saved->fbc.pattern4 = fbc->l_fbc_pattern4;
	saved->fbc.pattern5 = fbc->l_fbc_pattern5;
	saved->fbc.pattern6 = fbc->l_fbc_pattern6;
	saved->fbc.pattern7 = fbc->l_fbc_pattern7;

	/*
	* the tec matrix and clipping registers are easy.
	*/
	saved->tec.mv = tec->l_tec_mv;
	saved->tec.clip = tec->l_tec_clip;
	saved->tec.vdc = tec->l_tec_vdc;

	/*
	* the tec data registers are a little more non-obvious. internally, they
	* are 36 bits.  what we see in the register file is a 32-bit window onto
	* the underlying data register.  changing the data-type in the VDC gets
	* us either of two parts of the data register. the internal format is
	* opaque to us.
	*/
	tec->l_tec_vdc = (u_int) L_TEC_VDC_INTRNL0;
	for (dreg = 0, dp = (u_int *)&tec->l_tec_data00; dreg < 64;
				dreg++, dp++) {
	    saved->tec.data[dreg][0] = *dp;
	}
	tec->l_tec_vdc = (u_int) L_TEC_VDC_INTRNL1;
	for (dreg = 0, dp = (u_int *)&tec->l_tec_data00; dreg < 64;
				dreg++, dp++) {
	    saved->tec.data[dreg][1] = *dp;
	}
	return (1);
}

static int
p9000_cntxrestore(fbc, tec, saved)
	volatile struct fbc *fbc;
	volatile struct tec *tec;
	struct p9000_cntxt *saved;
{
	int    dreg;
	u_int  *dp;

	DEBUGF(5, (CE_CONT, "restoring registers for %d\n", saved->pid));

	/*
	* reload the tec data registers.  see above for "how do they get 36 bits
	* in that itty-bitty int"
	*/
	tec->l_tec_vdc = (u_int) L_TEC_VDC_INTRNL0;
	for (dreg = 0, dp = (u_int *)&tec->l_tec_data00;
	    dreg < 64; dreg++, dp++) {
		*dp = saved->tec.data[dreg][0];
	}
	tec->l_tec_vdc = (u_int) L_TEC_VDC_INTRNL1;
	for (dreg = 0, dp = (u_int *)&tec->l_tec_data00;
	    dreg < 64; dreg++, dp++) {
		*dp = saved->tec.data[dreg][1];
	}

	/*
	* the tec matrix and clipping registers are next.
	*/
	tec->l_tec_mv = saved->tec.mv;
	tec->l_tec_clip = saved->tec.clip;
	tec->l_tec_vdc = saved->tec.vdc;

	/*
	* now the FBC vertex and address registers
	*/
	fbc->l_fbc_x0 = saved->fbc.x0;
	fbc->l_fbc_y0 = saved->fbc.y0;
	fbc->l_fbc_x1 = saved->fbc.x1;
	fbc->l_fbc_y1 = saved->fbc.y1;
	fbc->l_fbc_x2 = saved->fbc.x2;
	fbc->l_fbc_y2 = saved->fbc.y2;
	fbc->l_fbc_x3 = saved->fbc.x3;
	fbc->l_fbc_y3 = saved->fbc.y3;
	fbc->l_fbc_rasteroffx = saved->fbc.rasteroffx;
	fbc->l_fbc_rasteroffy = saved->fbc.rasteroffy;
	fbc->l_fbc_autoincx = saved->fbc.autoincx;
	fbc->l_fbc_autoincy = saved->fbc.autoincy;
	fbc->l_fbc_clipminx = saved->fbc.clipminx;
	fbc->l_fbc_clipminy = saved->fbc.clipminy;
	fbc->l_fbc_clipmaxx = saved->fbc.clipmaxx;
	fbc->l_fbc_clipmaxy = saved->fbc.clipmaxy;

	/*
	* restoring the attribute registers
	*/
	fbc->l_fbc_fcolor = saved->fbc.fcolor;
	fbc->l_fbc_bcolor = saved->fbc.bcolor;
	fbc->l_fbc_rasterop = saved->fbc.rasterop;
	fbc->l_fbc_planemask = saved->fbc.planemask;
	fbc->l_fbc_pixelmask = saved->fbc.pixelmask;
	fbc->l_fbc_pattalign = saved->fbc.pattalign;
	fbc->l_fbc_pattern0 = saved->fbc.pattern0;
	fbc->l_fbc_pattern1 = saved->fbc.pattern1;
	fbc->l_fbc_pattern2 = saved->fbc.pattern2;
	fbc->l_fbc_pattern3 = saved->fbc.pattern3;
	fbc->l_fbc_pattern4 = saved->fbc.pattern4;
	fbc->l_fbc_pattern5 = saved->fbc.pattern5;
	fbc->l_fbc_pattern6 = saved->fbc.pattern6;
	fbc->l_fbc_pattern7 = saved->fbc.pattern7;

	fbc->l_fbc_clipcheck = saved->fbc.clipcheck;
	fbc->l_fbc_misc = saved->fbc.misc;

	/*
	* lastly, let's restore the status
	*/
	fbc->l_fbc_status = saved->fbc.status;
	return (1);
}
#endif	/* XXXppc */

/*
 * ctx_map_insert()
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
struct p9000_cntxt *
ctx_map_insert(struct p9000_softc *softc, int maptype)
{
	register struct p9000_cntxt *ctx;
	int curpid = getpid();

	DEBUGF(4, (CE_CONT, "ctx_map_insert: maptype=0x%x curpid=%d\n",
		maptype, curpid));

	/*
	 * If this is the first time we're here, then alloc space
	 * for new context and depart.
	 */
	if (softc->pvt_ctx == NULL) {
		ctx = (struct p9000_cntxt *)
			kmem_zalloc(sizeof (struct p9000_cntxt), KM_SLEEP);
		ctx->pid = curpid;
		ctx->link = NULL;
		softc->pvt_ctx = ctx;
		return (ctx);
	}

	/*
	 * Find existing context if one exists.  We have a match if
	 * we're the same process *and* there's not already a
	 * mapping of this type assigned.
	 */
	for (ctx = softc->pvt_ctx; ctx != NULL; ctx = ctx->link) {
		if (ctx->pid == curpid &&
		    (maptype & ctx->flag & (P9000MAP_FBCTEC|P9000MAP_FB)) == 0)
			break;
	}


	/* no match, create a new one and add to softc list */
	if (ctx == NULL) {
		ctx = (struct p9000_cntxt *)
			kmem_zalloc(sizeof (struct p9000_cntxt), KM_SLEEP);
		ctx->pid = curpid;
		ctx->link = softc->pvt_ctx;
		softc->pvt_ctx = ctx;
	}

	DEBUGF(4, (CE_CONT, "ctx_map_insert: returning ctx=0x%x\n", ctx));

	return (ctx);
}

/*
 * getpid()
 *
 * Simple wrapper around process ID call to drv_getparm(9f).
 */
static int
getpid()
{
	u_long rval;

	if (drv_getparm(PPID, &rval) == -1)
		return (0);
	else
		return ((int)rval);
}

static void
p9000_smart_bcopy(char *from, char *to, int count)
{
	unsigned long *to4;
	unsigned long *from4;

	if (((unsigned long)from & 3) != ((unsigned long)to & 3) ||
	    count < 3+4+3) {
		while (count-- > 0) *to++ = *from++;
		return;
	}
	while ((unsigned long)from & 3) {
		*to++ = *from++;
		count--;
	}
	to4 = (unsigned long *)to;
	from4 = (unsigned long *)from;
	while (count > 3) {
		*to4++ = *from4++;
		count -= 4;
	}
	to = (char *)to4;
	from = (char *)from4;
	while (count-- > 0) *to++ = *from++;
}

#if	DRIVER_DEBUG
static void
p9000_register_dump(struct p9000_softc *softc)
{
	volatile struct p9000 *regs = softc->regs;

#define	P(reg)	cmn_err(CE_CONT, "?%10s %08x\n", #reg, regs->reg);
	P(p9000_sysconfig);
	P(p9000_interrupt);
	P(p9000_interrupt_en);
	P(p9000_hrzt);
	P(p9000_hrzsr);
	P(p9000_hrzbr);
	P(p9000_hrzbf);
	P(p9000_prehrzc);
	P(p9000_vrtt);
	P(p9000_vrtsr);
	P(p9000_vrtbr);
	P(p9000_vrtbf);
	P(p9000_prevrtc);
	P(p9000_sraddr);
	P(p9000_srtctl);
	P(p9000_qsfcounter);
#undef P
}
#endif
