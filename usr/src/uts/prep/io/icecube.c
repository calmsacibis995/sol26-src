/*
 * Copyright (c) 1989-1996, Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)icecube.c	1.38	96/09/24 SMI"
#pragma	weak	setintrenable

/*
 * Driver for IBM "Icecube" card - S3/864 plus SDAC
 */

/*
 * accelerated 8/24 bit color frame buffer driver
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

#include <sys/vgareg.h>
#include <sys/s3reg.h>
#include <sys/SDACreg.h>
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

#include <sys/kmem.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/devops.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/fs/snode.h>

#include <sys/modctl.h>

#include <sys/machsystm.h>

/* configuration options */

char _depends_on[] = "misc/seg_mapdev";

/* This is to minimize the diffs against other near-identical drivers */
#define	MYNAME	"icecube"

#if	defined(DEBUG)
#define	DRIVER_DEBUG	127
#else
/*
 * Preprocessor will turn DRIVER_DEBUG refs in #ifs into 0s.
 * This is fine, and allows setting DRIVER_DEBUG as a compile
 * option in a non-DEBUG environment.
 */
#endif

/*
 * Unfortunately, early (at least pre-10/24/95) VOF implementations
 * didn't include the ROM tuple.  This means that the hard decodes are
 * at different places in those VOFs and in VOFs that conform to the
 * 1275 PCI binding.  Searching allows us to live with either.
 * Eventually, when all the "old" VOFs have gone away, we should
 * be able to go back to using the constants.
 */
#define	SEARCH_FOR_REG_ENTRY

#define	S3_FB_RNUMBER	1

#if	defined(SEARCH_FOR_REG_ENTRY)
#include <sys/pci.h>
#define	S3_REG_ADDR	0x3c0
#define	S3_MMIO_ADDR	0xa0000
static int get_reg_index(dev_info_t *, unsigned long, unsigned long,
	unsigned long);
#else
/* These are the constants for a proper 1275-compliant node. */
#define	S3_REG_RNUMBER	4
#define	S3_MMIO_RNUMBER	5
#endif

#define	CMAP_ENTRIES	SDAC_CMAP_ENTRIES

#define	S3DELAY(c, n)    \
{ \
	register int N = n; \
	while (--N > 0) { \
	    if (c) \
		break; \
	    drv_usecwait(1); \
	} \
}

#if DRIVER_DEBUG
static int	s3_debug = 0;

#define	DEBUGF(level, args) \
		{ if (s3_debug >= (level)) cmn_err args; }
#define	DUMP_SEGS(level, s, c) \
		{ if (s3_debug >= (level)) dump_segs(s, c); }
#else
#define	DEBUGF(level, args)	/* nothing */
#define	DUMP_SEGS(level, s, c)	/* nothing */
#endif

#define	getprop(devi, name, def)	\
		ddi_getprop(DDI_DEV_T_ANY, (devi), \
		DDI_PROP_DONTPASS, (name), (def))

/* config info */

static int	s3_open(dev_t *, int, int, cred_t *);
static int	s3_close(dev_t, int, int, cred_t *);
static int	s3_ioctl(dev_t, int, intptr_t, int, cred_t *, int *);
static int	s3_mmap(dev_t, off_t, int);
static int	s3_segmap(dev_t, off_t,
			struct as *, caddr_t *, off_t, u_int,
			u_int, u_int, cred_t *);

static struct vis_identifier s3_ident = { "SUNW" MYNAME };

static struct cb_ops s3_cb_ops = {
	s3_open,		/* open */
	s3_close,		/* close */
	nodev,			/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	nodev,			/* read */
	nodev,			/* write */
	s3_ioctl,		/* ioctl */
	nodev,			/* devmap */
	s3_mmap,		/* mmap */
	s3_segmap,		/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,		/* cb_prop_op */
	0,			/* streamtab  */
	D_NEW | D_MP		/* Driver compatibility flag */
};

static int s3_info(dev_info_t *dip, ddi_info_cmd_t infocmd,
		void *arg, void **result);
static int s3_attach(dev_info_t *, ddi_attach_cmd_t);
static int s3_detach(dev_info_t *, ddi_detach_cmd_t);
static int s3_power(dev_info_t *, int, int);
static int s3_probe(dev_info_t *);

static struct dev_ops s3_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	s3_info,		/* info */
	nulldev,		/* identify */
	s3_probe,		/* probe */
	s3_attach,		/* attach */
	s3_detach,		/* detach */
	nodev,			/* reset */
	&s3_cb_ops,		/* driver operations */
	(struct bus_ops *)0,	/* bus operations */
	s3_power
};

/*
 * This stucture is used to contain the driver
 * private mapping data (one for each requested
 * device mapping).  A pointer to this data is
 * passed into each mapping callback routine.
 */
static
struct s3map_pvt {
	struct	s3_softc *softc;
	ddi_mapdev_handle_t handle;	/* handle of mapdev object	*/
	u_int   offset;			/* starting offset of this map	*/
	u_int   len;			/* length of this map		*/
	struct s3_cntxt *context;	/* associated context		*/
	struct s3map_pvt *next;	/* List of associated pvt's for	*/
					/* this context			*/
};

/* how much to map */
#define	S3MAPSIZE	MMAPSIZE(0)

/* vertical retrace counter page */
#ifndef	S3_VRT_SZ
#define	S3_VRT_SZ	8192
#endif

/*
 * Per-context info:
 *	many registers in the tec and fbc do
 *	not need to be saved/restored.
 */
struct s3_cntxt {
	struct s3_cntxt *link;	/* link to next (private) context if any */
	struct s3map_pvt *pvt; /* List of associated pvt's for this context */
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

struct reg {
	caddr_t			addr;
	ddi_acc_handle_t	handle;
	boolean_t		mapped;
};

/* per-unit data */
struct s3_softc {
	Pixrect pr;			/* kernel pixrect */
	struct mprp_data prd;	/* pixrect private data */
#define	_w		pr.pr_size.x
#define	_h		pr.pr_size.y
#define	_fb		prd.mpr.md_image
#define	_linebytes	prd.mpr.md_linebytes
	int	depth;		/* Depth in bits */
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
	struct reg	regs;
	struct reg	fb;
	struct reg	mmio;
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

#define	S3VRTIOCTL	1	/* FBIOVERTICAL in effect */
#define	S3VRTCTR	2	/* OWGX vertical retrace counter */
	unsigned long fbmappable;	/* bytes mappable */
	int		*vrtpage;	/* pointer to VRT page */
	int		*vrtalloc;	/* pointer to VRT allocation */
	int		vrtmaps;	/* number of VRT page maps */
	int		vrtflag;	/* vrt interrupt flag */
	struct s3_info	adpinfo;	/* info about this adapter */
	struct mon_info moninfo;	/* info about this monitor */
	struct s3_cntxt *curctx;	/* context switching */
	struct s3_cntxt shared_ctx;	/* shared context */
	struct s3_cntxt *pvt_ctx;	/* list of non-shared contexts */
	int		chiprev;	/* fbc chip revision # */
	int		emulation;	/* emulation type, normally s3 */
	dev_info_t	*devi;		/* back pointer */
	ddi_iblock_cookie_t iblock_cookie;	/* block interrupts */
	kmutex_t	mutex;		/* mutex locking */
	kcondvar_t	vrtsleep;	/* for waiting on vertical retrace */
	boolean_t	mapped_by_prom;	/* $#!@ SVr4 */
	int		s3_suspended;	/* true if driver is suspended */
	struct s3_console_private
			*conspriv;	/* console support */
	void	(*copy_func)(struct s3_softc *, struct vis_conscopy *);
	void	(*display_func)(struct s3_softc *, struct vis_consdisplay *);
	void	(*cursor_func)(struct s3_softc *, struct vis_conscursor *);
};

/* console management */

struct  s3_console_private {
	struct s3_softc
			*softc;	/* softc for this instance */
	struct s3_cntxt
			context;	/* pointer to context */
	int		viscursor;
				/* cursor visible on this instance? */
	kmutex_t	pmutex;	/* mutex locking */
	int		fg;	/* foreground color */
	int		bg;	/* background color */
	int		ifg;	/* inverse video foreground color */
	int		ibg;	/* inverse video background color */
};


struct s3_rect {
	short w;
	short h;
	short x;
	short y;
};

static int	s3_devinit(dev_t dev, struct vis_devinit *);
static void	s3_cons_free(struct s3_console_private *);

static int s3map_access(ddi_mapdev_handle_t, void *, off_t);
static void s3map_free(ddi_mapdev_handle_t, void *);
static int s3map_dup(ddi_mapdev_handle_t, void *,
		ddi_mapdev_handle_t, void **);

static
struct ddi_mapdev_ctl s3map_ops = {
	MAPDEV_REV,	/* mapdev_ops version number	*/
	s3map_access,	/* mapdev access routine	*/
	s3map_free,	/* mapdev free routine		*/
	s3map_dup	/* mapdev dup routine		*/
};

static u_int	pagesize;
static void	*s3_softc_head;

/* default structure for FBIOGATTR ioctl */
static struct fbgattr s3_attr = {
/*	real_type	 owner */
	FBTYPE_SUNFAST_COLOR, 0,
/* fbtype: type		 h  w  depth    cms  size */
	{FBTYPE_SUNFAST_COLOR, 0, 0, 0, CMAP_ENTRIES, 0},
/* fbsattr: flags emu_type    dev_specific */
	{0, FBTYPE_SUN4COLOR, {0}},
/*	emu_types */
	{FBTYPE_SUNFAST_COLOR, FBTYPE_SUN3COLOR, FBTYPE_SUN4COLOR, -1}
};


/*
 * handy macros
 */

#define	getsoftc(instance)	\
	((struct s3_softc *)ddi_get_soft_state(s3_softc_head, (instance)))

#define	btob(n)			ctob(btoc(n))	/* TODO, change this? */

/* convert softc to data pointers */

#ifdef	XXXppc
#define	S_FBC(softc)	((struct fbc *)(softc)->fbctec)
#define	S_TEC(softc)	((struct tec *)((softc)->fbctec + S3_TEC_POFF))
#define	S_FHC(softc)	((u_int *)(softc)->fhcthc)
#define	S_THC(softc)	((struct thc *)((softc)->fhcthc + S3_TEC_POFF))
#endif	/* XXXppc */
#define	S_CMAP(softc)	((struct s3_cmap *)(softc)->cmap)

#ifdef	XXXppc
#define	s3_set_video(softc, on)	thc_set_video(S_THC(softc), (on))
#define	s3_get_video(softc)		thc_get_video(S_THC(softc))

#define	s3_set_sync(softc, on) \
	    S_THC(softc)->l_thc_hcmisc = \
	    (S_THC(softc)->l_thc_hcmisc & ~THC_HCMISC_SYNCEN | \
	    ((on) ? THC_HCMISC_SYNCEN : 0))


#define	s3_int_enable(softc) \
	{\
	    thc_int_enable(S_THC(softc)); \
	    if ((softc)->p4reg) \
		(void) setintrenable(1); }

#define	s3_int_disable_intr(softc) \
	{\
	    if ((softc)->p4reg) \
		(void) setintrenable(0); \
	    thc_int_disable(S_THC(softc)); }

#define	s3_int_disable(softc) \
	{\
	    mutex_enter(&(softc)->interlock); \
	    s3_int_disable_intr(softc);    \
	    mutex_exit(&(softc)->interlock); }

#define	s3_int_pending(softc)		thc_int_pending(S_THC(softc))
#else
#define	s3_set_video(softc, on)	/* NYI */
#define	s3_set_sync(softc, on)	/* NYI */
#endif	/* XXXppc */

/* check if color map update is pending */
#define	s3_update_pending(softc) \
	((softc)->cmap_count || (softc)->omap_update)

/*
 * forward references
 */
#ifdef	XXXppc
static u_int	s3_intr(caddr_t);
#endif
static void	s3_reset_cmap(volatile u_char *, u_int);
static void	s3_update_cmap(struct s3_softc *, u_int, u_int);
#ifdef	XXXppc
static void	s3_cmap_bcopy(u_char *, u_char *, u_int);
#endif
static void	s3_cons_put_cmap(struct s3_softc *, int,
			unsigned char, unsigned char, unsigned char);
static void	s3_cons_get_cmap(struct s3_softc *, int,
			unsigned char *, unsigned char *, unsigned char *);

static void	s3_setcurpos(struct s3_softc *);
static void	s3_setcurshape(struct s3_softc *);
static void	s3_reset(struct s3_softc *);
static void	s3_set_indexed(struct s3_softc *,
			caddr_t indexreg, caddr_t datareg, int index, int val);
static int	s3_get_indexed(struct s3_softc *,
			caddr_t indexreg, caddr_t datareg, int index);
static void	s3_dac_out(struct s3_softc *softc, int index, int val);
static int	s3_dac_in(struct s3_softc *softc, int index);
#if	DRIVER_DEBUG
static void	s3_register_dump(struct s3_softc *softc);
#endif
#ifdef	XXXppc
static int	s3_cntxsave(volatile struct fbc *, volatile struct tec *,
			struct s3_cntxt *);
static int	s3_cntxrestore(volatile struct fbc *, volatile struct tec *,
			struct s3_cntxt *);
#endif	/* XXXppc */
static struct s3_cntxt *ctx_map_insert(struct s3_softc *, int);
static int	getpid(void);
static void	s3_get_hardware_settings(struct s3_softc *);

/*
 * SunWindows specific stuff
 */
static  s3_rop(), s3_putcolormap();

/* kernel pixrect ops vector */
static struct pixrectops s3_pr_ops = {
	s3_rop,
	s3_putcolormap,
	mem_putattributes
};

/* Loadable Driver stuff */

static struct modldrv modldrv = {
	&mod_driverops,		/* Type of module.  This one is a driver */
	MYNAME " driver v1.38",	/* Name of the module. */
	&s3_ops,		/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *) &modldrv, NULL
};

int
_init(void)
{
	register int e;

	DEBUGF(1, (CE_CONT, MYNAME ": compiled %s, %s\n", __TIME__, __DATE__));

	if ((e = ddi_soft_state_init(&s3_softc_head,
		    sizeof (struct s3_softc), 1)) != 0) {
	    DEBUGF(1, (CE_CONT, "done\n"));
	    return (e);
	}

	e = mod_install(&modlinkage);

	if (e) {
	    ddi_soft_state_fini(&s3_softc_head);
	    DEBUGF(1, (CE_CONT, "done\n"));
	}
	DEBUGF(1, (CE_CONT, "s3: _init done rtn=%d\n", e));
	return (e);
}

int
_fini(void)
{
	register int e;

#ifdef	XXXppc
	DEBUGF(1, (CE_CONT, "s3: _fini, mem used=%d\n", total_memory));
#else
	DEBUGF(1, (CE_CONT, "s3: _fini\n"));
#endif

	if ((e = mod_remove(&modlinkage)) != 0)
	    return (e);

	ddi_soft_state_fini(&s3_softc_head);

	return (0);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/* XXX cursor artifact avoidance -- there must be a better way to do this */

static  s3_rop_wait = 50;	/* milliseconds */

static
s3_rop(Pixrect *dpr,
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
	if (spr && spr->pr_ops == &s3_pr_ops) {
	    unit = mpr_d(spr)->md_primary;
	    mpr = *spr;
	    mpr.pr_ops = &mem_ops;
	    spr = &mpr;
	} else
	    unit = mpr_d(dpr)->md_primary;

	if (s3_rop_wait) {
#ifdef	XXXppc
	    volatile u_int *statp = &fbc->l_fbc_status;
	    CDELAY(!(*statp & L_FBC_BUSY), s3_rop_wait << 10);
#else
	    /*EMPTY*/
	    /* NYI:  wait for idle */
#endif
	}
	return (mem_rop(dpr, dx, dy, dw, dh, op, spr, sx, sy));
}

/*ARGSUSED*/
static
s3_putcolormap(Pixrect *pr,
		int index,
		int count,
		unsigned char *red,
		unsigned char *green,
		unsigned char *blue)
{
#ifdef	XXXppc
	register struct s3_softc *softc = getsoftc(mpr_d(pr)->md_primary);
	register u_int rindex = (u_int) index;
	register u_int rcount = (u_int) count;
	register u_char *map;
	register u_int entries;

	DEBUGF(5, (CE_CONT, "s3_putcolormap unit=%d index=%d count=%d\n",
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
	if (s3_update_pending(softc))
	    s3_int_disable(softc);
#endif

	map += rindex * 3;
	s3_cmap_bcopy(red, map, rcount);
	s3_cmap_bcopy(green, map + 1, rcount);
	s3_cmap_bcopy(blue, map + 2, rcount);

	s3_update_cmap(softc, rindex, rcount);

#ifdef	XXXppc
	/* enable interrupt so we can load the hardware colormap */
	s3_int_enable(softc);
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
s3_probe(dev_info_t *devi)
{
	int	error = DDI_PROBE_SUCCESS;
#ifdef	XXXppc
	long	*fbc;
	long	id;
#endif

	DEBUGF(1, (CE_CONT, "s3_probe (%s) unit=%d\n",
	    ddi_get_name(devi), ddi_get_instance(devi)));

	if (ddi_dev_is_sid(devi) == DDI_SUCCESS)
	    return (DDI_PROBE_DONTCARE);

#ifdef	XXXppc
	/*
	 * after this, fbc will point to the first address in
	 * the fbc address space - the p4 register
	 */
	if (ddi_map_regs(devi, S3_REG_RNUMBER, (caddr_t *)&fbc,
		    S3_ADDR_P4REG, (off_t)sizeof (long)) != 0) {
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
		DEBUGF(2, (CE_CONT, " not a s3 - returning failure\n"));
		error = DDI_FAILURE;
	    }
	}

	ddi_unmap_regs(devi, S3_REG_RNUMBER, (caddr_t *)&fbc,
		S3_ADDR_P4REG, (off_t)sizeof (long));
#endif
	return (error);
}

#define	GET_CRTC(s, i) s3_get_indexed(s, s->regs.addr+S3_CRTC_ADR, \
				s->regs.addr+S3_CRTC_DATA, i)
#define	SET_CRTC(s, i, v) s3_set_indexed(s, s->regs.addr+S3_CRTC_ADR, \
				s->regs.addr+S3_CRTC_DATA, i, v)

static int
s3_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	register struct s3_softc *softc;
	int		bytes;
	char		*tmp;
	char		name[16];
	int		unit = ddi_get_instance(devi);
	int		proplen;
	u_int		power;
#ifdef	XXXppc
	caddr_t		fb_ndvram;
#endif
	int		rnumber;
	static ddi_device_acc_attr_t reg_access_mode = {
		DDI_DEVICE_ATTR_V0,
		DDI_NEVERSWAP_ACC,	/* not portable */
		DDI_STRICTORDER_ACC,
	};
	static ddi_device_acc_attr_t fb_access_mode = {
		DDI_DEVICE_ATTR_V0,
		DDI_NEVERSWAP_ACC,	/* not portable */
		DDI_STRICTORDER_ACC,
	};
	static ddi_device_acc_attr_t mmio_access_mode = {
		DDI_DEVICE_ATTR_V0,
		DDI_NEVERSWAP_ACC,	/* not portable */
		DDI_STRICTORDER_ACC,
	};

	DEBUGF(1, (CE_CONT, "s3_attach unit=%d cmd=%d\n", unit, (int)cmd));

	switch (cmd) {
	case DDI_ATTACH:
	    break;

	case DDI_RESUME:
	    if (!(softc = (struct s3_softc *)ddi_get_driver_private(devi)))
		    return (DDI_FAILURE);
	    if (!softc->s3_suspended)
		    return (DDI_SUCCESS);
	    mutex_enter(&softc->mutex);
	    s3_reset(softc);
	    if (softc->curctx) {
#ifdef	XXXppc
		    /* Restore non display RAM */
		    if (ddi_map_regs(devi, S3_REG_RNUMBER,
			(caddr_t *)&fb_ndvram,
			S3_ADDR_COLOR + softc->_w * softc->_h,
			softc->ndvramsz) == -1) {
			    mutex_exit(&softc->mutex);
			    return (DDI_FAILURE);
		    }
		    bcopy(softc->ndvram, fb_ndvram, softc->ndvramsz);
		    ddi_unmap_regs(devi, S3_REG_RNUMBER,
			(caddr_t *)&fb_ndvram,
			S3_ADDR_COLOR + softc->_w * softc->_h,
			softc->ndvramsz);
		    kmem_free(softc->ndvram, softc->ndvramsz);

		    /* Restore other frame buffer state */
		    (void) s3_cntxrestore(S_FBC(softc), S_TEC(softc),
			softc->curctx);
		    s3_setcurpos(softc);
		    s3_setcurshape(softc);
		    s3_update_cmap(softc, (u_int)_ZERO_, CMAP_ENTRIES);
		    s3_int_enable(softc);	/* Schedule the update */
#else
		/*EMPTY*/
		/* NYI:  parts of DDI_RESUME not implemented */
#endif	/* XXXppc */
	    }
	    softc->s3_suspended = 0;
	    mutex_exit(&softc->mutex);
	    /* Restore brightness level */
	    if ((power = pm_get_normal_power(devi, 1)) == DDI_FAILURE) {
		cmn_err(CE_WARN, "s3_attach(DDI_RESUME) can't get normal "
			"power");
		return (DDI_FAILURE);
	    }
	    if (s3_power(devi, 1, power) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "s3_attach: s3_power failed");
		return (DDI_FAILURE);
	    }

	    return (DDI_SUCCESS);

	default:
	    return (DDI_FAILURE);
	}

	DEBUGF(1, (CE_CONT, "s3_attach unit=%d\n", unit));

	pagesize = ddi_ptob(devi, 1);

	/* Allocate softc struct */
	if (ddi_soft_state_zalloc(s3_softc_head, unit) != 0) {
		return (DDI_FAILURE);
	}

	softc = getsoftc(unit);

	/* link it in */
	softc->devi = devi;
	DEBUGF(1, (CE_CONT, "s3_attach devi=0x%x unit=%d\n", devi, unit));
	ddi_set_driver_private(devi, (caddr_t)softc);

#ifdef	XXXppc
	softc->p4reg = 0;
	if (ddi_dev_is_sid(devi) != DDI_SUCCESS) {
	    if (ddi_map_regs(devi, S3_REG_RNUMBER,
			(caddr_t *)&softc->p4reg,
			(off_t)S3_ADDR_P4REG, sizeof (*softc->p4reg)) != 0) {
		DEBUGF(2, (CE_CONT,
			"s3_probe: ddi_map_regs (p4reg) FAILED\n"));
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

#if	defined(SEARCH_FOR_REG_ENTRY)
	rnumber = get_reg_index(devi, PCI_REG_ADDR_M|PCI_REG_REL_M,
		    PCI_ADDR_IO|PCI_RELOCAT_B, S3_REG_ADDR);
	if (rnumber < 0) {
		s3_detach(devi, DDI_DETACH);
		return (DDI_FAILURE);
	}
#else
	rnumber = S3_REG_RNUMBER;
#endif

	if (ddi_regs_map_setup(devi, rnumber,
		(caddr_t *)&softc->regs.addr, (off_t)0, (off_t)0,
		&reg_access_mode, &softc->regs.handle) == -1) {
	    s3_detach(devi, DDI_DETACH);
	    return (DDI_FAILURE);
	}
	softc->regs.mapped = B_TRUE;

#if	DRIVER_DEBUG
	if (s3_debug > 0)
	    s3_register_dump(softc);
#endif

	s3_get_hardware_settings(softc);

	bytes = getprop(devi, "linebytes", mpr_linebytes(softc->_w, 8));

	softc->_linebytes = bytes;

	/* Compute size of color frame buffer */
	bytes = btob(bytes * softc->_h);
#ifdef	XXXppc
	softc->size = ddi_ptob(devi, ddi_btopr(devi, bytes));

	softc->adpinfo.vmsize = getprop(devi, "vmsize", 1);
	if (softc->adpinfo.vmsize > 1) {
	    softc->size = ddi_ptob(devi, ddi_btopr(devi, 8 * 1024 * 1024));
	    softc->fbmappable = 8 * 1024 * 1024;
	} else
	    softc->fbmappable = 1024 * 1024;
#else
	softc->adpinfo.vmsize = S3_CFG1_MEMSIZE(
				GET_CRTC(softc, S3_CONFG_REG1));
	softc->fbmappable = softc->adpinfo.vmsize * S3_CFG1_MEMSIZE_SCALE;
	softc->size = softc->fbmappable;
#endif

	/* Compute size of dummy overlay/enable planes */
	softc->dummysize = btob(mpr_linebytes(softc->_w, 1) * softc->_h) * 2;

	softc->adpinfo.line_bytes = softc->_linebytes;
	softc->adpinfo.accessible_width = getprop(devi, "awidth", 1152);
	softc->adpinfo.accessible_height = (u_int)
	    (softc->adpinfo.vmsize * S3_CFG1_MEMSIZE_SCALE)
	    / softc->adpinfo.accessible_width;
	softc->adpinfo.hdb_capable = getprop(devi, "dblbuf", 0);
	softc->adpinfo.boardrev = getprop(devi, "boardrev", 0);
	softc->vrtpage = NULL;
	softc->vrtalloc = NULL;
	softc->vrtmaps = 0;
	softc->vrtflag = 0;

#ifdef	XXXppc
#ifdef DEBUG
	softc->adpinfo.pad1 = S3_VADDR_COLOR + S3_FB_SZ;
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

	/* map frame buffer */
	if (ddi_regs_map_setup(devi, S3_FB_RNUMBER,
		(caddr_t *)&softc->_fb, (off_t)0, (off_t)0,
		&fb_access_mode, &softc->fb.handle) == -1) {
	    s3_detach(devi, DDI_DETACH);
	    return (DDI_FAILURE);
	}
	softc->fb.mapped = B_TRUE;

#if	defined(SEARCH_FOR_REG_ENTRY)
	rnumber = get_reg_index(devi, PCI_REG_ADDR_M|PCI_REG_REL_M,
		    PCI_ADDR_MEM32|PCI_RELOCAT_B, S3_MMIO_ADDR);
	if (rnumber < 0) {
		s3_detach(devi, DDI_DETACH);
		return (DDI_FAILURE);
	}
#else
	rnumber = S3_MMIO_RNUMBER,
#endif

	if (ddi_regs_map_setup(devi, rnumber,
		(caddr_t *)&softc->mmio.addr, (off_t)0, (off_t)0,
		&mmio_access_mode, &softc->mmio.handle) == -1) {
	    s3_detach(devi, DDI_DETACH);
	    return (DDI_FAILURE);
	}
	softc->mmio.mapped = B_TRUE;

	softc->chiprev = GET_CRTC(softc, S3_CHIP_ID_REV);

	s3_reset(softc);

#ifdef	XXXppc
	/* attach interrupt, notice the dance... see 1102427 */
	if (ddi_add_intr(devi, 0, &softc->iblock_cookie, 0,
		(u_int (*)()) nulldev, (caddr_t)0) != DDI_SUCCESS) {
	    DEBUGF(2, (CE_CONT, "s3_attach%d add_intr failed\n", unit));
	    (void) s3_detach(devi, DDI_DETACH);
	    return (DDI_FAILURE);
	}

	mutex_init(&softc->interlock, "s3_interlock", MUTEX_DRIVER,
	    softc->iblock_cookie);
	mutex_init(&softc->mutex,
		    "s3_softc_mtx", MUTEX_DRIVER, softc->iblock_cookie);
	cv_init(&softc->vrtsleep,
		    "s3_vrt_wait", CV_DRIVER, softc->iblock_cookie);

	ddi_remove_intr(devi, 0, softc->iblock_cookie);

	if (ddi_add_intr(devi, 0, &softc->iblock_cookie, 0,
		    s3_intr, (caddr_t)softc) != DDI_SUCCESS) {
	    DEBUGF(2, (CE_CONT,
		"s3_attach%d add_intr failed\n", unit));
	    (void) s3_detach(devi, DDI_DETACH);
	    return (DDI_FAILURE);
	}
#else
	/* NYI:  no interrupt handling yet */
#endif	/* XXXppc */

	/*
	 * Initialize hardware colormap and software colormap images. It might
	 * make sense to read the hardware colormap here.
	 */
	s3_reset_cmap(softc->cmap_rgb, CMAP_ENTRIES);
	s3_reset_cmap(softc->omap_rgb, 2);
	s3_update_cmap(softc, (u_int) _ZERO_, CMAP_ENTRIES);
	s3_update_cmap(softc, (u_int) _ZERO_, (u_int) _ZERO_);

	SET_CRTC(softc, S3_BKWD_2,
		GET_CRTC(softc, S3_BKWD_2) | S3_BK2_BDR_SEL);

	DEBUGF(2, (CE_CONT,
	    "s3_attach%d just before create_minor node\n", unit));
	sprintf(name, "s3_%d", unit);
	if (ddi_create_minor_node(devi, name, S_IFCHR,
			    unit, DDI_NT_DISPLAY, NULL) == DDI_FAILURE) {
	    ddi_remove_minor_node(devi, NULL);
	    DEBUGF(2, (CE_CONT,
		"s3_attach%d create_minor node failed\n", unit));
	    return (DDI_FAILURE);
	}
	ddi_report_dev(devi);

	cmn_err(CE_CONT,
	    "?" MYNAME " %d: screen %dx%d, %s buffered, "
		"%ld%sM mappable, rev %u.%u\n",
	    unit, softc->_w, softc->_h,
	    softc->adpinfo.hdb_capable ? "double" : "single",
	    (long)softc->adpinfo.vmsize * S3_CFG1_MEMSIZE_SCALE / 1024 / 1024,
	    ((long)softc->adpinfo.vmsize*S3_CFG1_MEMSIZE_SCALE/512/1024) % 2
		? ".5" : "",
	    softc->chiprev>>4, softc->chiprev & 0x0f);

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

#if	defined(SEARCH_FOR_REG_ENTRY)
/*
 * search the entries of the "reg" property for one which has
 * the desired combination of phys_hi bits and the desired address.
 */
static int
get_reg_index(
	dev_info_t *const devi,
	unsigned long himask,
	unsigned long hival,
	unsigned long addr)
{

	int			length, index;
	pci_regspec_t	*pcireg;

	if (ddi_getlongprop(DDI_DEV_T_ANY, devi, DDI_PROP_DONTPASS,
		"reg", (caddr_t)&pcireg, &length) != DDI_PROP_SUCCESS) {
		return (-1);
	}

	for (index = 0; index < length / sizeof (pci_regspec_t); index++) {
		if (((pcireg[index].pci_phys_hi & himask) == hival) &&
			((pcireg[index].pci_phys_low & PCI_CONF_ADDR_MASK) ==
			addr)) {
		kmem_free(pcireg, (size_t)length);
		return (index);
		}
	}
	kmem_free(pcireg, (size_t)length);

	return (-1);
}
#endif

static int
s3_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	int instance = ddi_get_instance(devi);
	register struct s3_softc *softc = getsoftc(instance);
#ifdef	XXXppc
	caddr_t fb_ndvram;
#endif

	DEBUGF(1, (CE_CONT, "s3_detach softc=%x, devi=0x%x\n", softc, devi));

	switch (cmd) {
	case DDI_DETACH:
	    break;

	case DDI_SUSPEND:
#ifdef	XXXppc
	    if (softc == NULL)
		    return (DDI_FAILURE);
	    if (softc->s3_suspended)
		    return (DDI_FAILURE);

	    mutex_enter(&softc->mutex);

	    if (softc->curctx) {
		    /* Save non display RAM */
		    softc->ndvramsz =
			(softc->adpinfo.vmsize * S3_CFG1_MEMSIZE_SCALE)
			- (softc->_w * softc->_h);
		    if ((softc->ndvram = kmem_alloc(softc->ndvramsz,
			KM_NOSLEEP)) == NULL) {
			    mutex_exit(&softc->mutex);
			    return (DDI_FAILURE);
		    }
		    if (ddi_map_regs(devi, S3_REG_RNUMBER, &fb_ndvram,
			S3_ADDR_COLOR + softc->_w * softc->_h,
			softc->ndvramsz) == -1) {
			    kmem_free(softc->ndvram, softc->ndvramsz);
			    mutex_exit(&softc->mutex);
			    return (DDI_FAILURE);
		    }
		    bcopy(fb_ndvram, softc->ndvram, softc->ndvramsz);
		    ddi_unmap_regs(devi, S3_REG_RNUMBER, &fb_ndvram,
			S3_ADDR_COLOR + softc->_w * softc->_h,
			softc->ndvramsz);

		    /* Save other frame buffer state */
		    (void) s3_cntxsave(S_FBC(softc), S_TEC(softc),
			softc->curctx);
	    }
	    softc->s3_suspended = 1;
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
	    s3_set_video(softc, 0);

	mutex_enter(&softc->mutex);
	s3_int_disable(softc);
	mutex_exit(&softc->mutex);

	ddi_remove_intr(devi, 0, softc->iblock_cookie);

	if (softc->fbctec)
	    ddi_unmap_regs(devi, S3_REG_RNUMBER,
		&softc->fbctec, S3_ADDR_FBC, S3_FBCTEC_SZ);
	if (softc->cmap)
	    ddi_unmap_regs(devi, S3_REG_RNUMBER,
		&softc->cmap, S3_ADDR_CMAP, S3_CMAP_SZ);
	if (softc->fhcthc)
	    ddi_unmap_regs(devi, S3_REG_RNUMBER,
		&softc->fhcthc, S3_ADDR_FHC, S3_FHCTHC_SZ);
	if (softc->rom)
	    ddi_unmap_regs(devi, S3_REG_RNUMBER,
		&softc->rom, softc->addr_rom, S3_ROM_SZ);
	if (softc->dhc)
	    ddi_unmap_regs(devi, S3_REG_RNUMBER,
		&softc->dhc, S3_ADDR_DHC, S3_DHC_SZ);
	if (softc->alt)
	    ddi_unmap_regs(devi, S3_REG_RNUMBER,
		&softc->alt, S3_ADDR_ALT, S3_ALT_SZ);
	/* TODO: uart (future) */
#else
	/* NYI:  skipped various mapping and interrupt things */
#endif	/* XXXppc */

	if (softc->fb.mapped)
	    ddi_regs_map_free(&softc->fb.handle);

	if (softc->regs.mapped)
	    ddi_regs_map_free(&softc->regs.handle);

	if (softc->mmio.mapped)
	    ddi_regs_map_free(&softc->mmio.handle);

	if (softc->vrtalloc != NULL)
	    kmem_free(softc->vrtalloc, pagesize * 2);

	mutex_destroy(&softc->mutex);

	cv_destroy(&softc->vrtsleep);

	ASSERT(softc->curctx == NULL);

	/* free softc struct */
	(void) ddi_soft_state_free(s3_softc_head, instance);

	pm_destroy_components(devi);
	return (DDI_SUCCESS);
}

static int
s3_power(dev_info_t *dip, int cmpt, int level)
{
	register struct s3_softc *softc;

	if (cmpt != 1 || 0 > level || level > 255 ||
	    !(softc = (struct s3_softc *)ddi_get_driver_private(dip)))
		return (DDI_FAILURE);

#if defined(lint)
	(void) softc;
#endif
	if (level) {
		/*EMPTY*/
		s3_set_sync(softc, FBVIDEO_ON);
		s3_set_video(softc, FBVIDEO_ON);
	} else {
		/*EMPTY*/
		s3_set_video(softc, FBVIDEO_OFF);
		s3_set_sync(softc, FBVIDEO_OFF);
	}

	(void) ddi_power(dip, cmpt, level);

	return (DDI_SUCCESS);
}

/* ARGSUSED */
static int
s3_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	register int instance, error = DDI_SUCCESS;
	register struct s3_softc *softc;

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
s3_open(dev_t *devp, int flag, int otyp, cred_t *cred)
{
	int	unit = getminor(*devp);
	struct	s3_softc *softc = getsoftc(unit);
	int	error = 0;

#ifdef	XXXppc
	DEBUGF(2, (CE_CONT, "s3_open(%d), mem used=%d\n", unit, total_memory));
#else
	DEBUGF(2, (CE_CONT, "s3_open(%d)\n", unit));
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
s3_close(dev_t dev, int flag, int otyp, cred_t *cred)
{
	int    unit = getminor(dev);
	struct s3_softc *softc = getsoftc(unit);
	int	error = 0;

#ifdef	XXXppc
	DEBUGF(2, (CE_CONT, "s3_close(%d, %d, %d), mem used=%d\n",
	    unit, flag, otyp, total_memory));
#else
	DEBUGF(2, (CE_CONT, "s3_close(%d, %d, %d)\n",
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
	    s3_reset(softc);
	    mutex_exit(&softc->mutex);
	}

	return (error);
}

/*ARGSUSED*/
static
s3_mmap(dev_t dev, register off_t off, int prot)
{
	register struct s3_softc *softc = getsoftc(getminor(dev));
	register int rval;

	DEBUGF(off ? 5 : 1, (CE_CONT, "s3_mmap(%d, 0x%x)\n",
	    getminor(dev), (u_int) off));

	if (off >= S3_VADDR_FB && off < S3_VADDR_FB+softc->fbmappable)
	    rval = hat_getkpfnum((caddr_t)softc->_fb + off - S3_VADDR_FB);
	else if (off >= S3_VADDR_MMIO && off < S3_VADDR_MMIO+S3_MMIO_SZ)
	    rval = hat_getkpfnum((caddr_t)softc->mmio.addr +
			off - S3_VADDR_MMIO);
	else
	    rval = -1;

	DEBUGF(5, (CE_CONT, "s3_mmap returning 0x%x\n", rval));

	return (rval);
}

/*ARGSUSED*/
static
s3_ioctl(dev_t dev, int cmd, intptr_t data,
	int mode, cred_t *cred, int *rval)
{
	register struct s3_softc *softc = getsoftc(getminor(dev));
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
		"s3_ioctl: Console has not been initialized";
	static char kernel_only[] =
		"s3_ioctl: %s is a kernel only ioctl";
	static char kernel_only2[] =
		"s3_ioctl: %s and %s are kernel only ioctls";

	DEBUGF(3, (CE_CONT, "s3_ioctl(%d, 0x%x)\n", getminor(dev), cmd));

	/* default to updating normal colormap */
	cursor_cmap = 0;

	switch (cmd) {

	case VIS_GETIDENTIFIER:

	    if (ddi_copyout((caddr_t)&s3_ident,
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

	    if ((err = s3_devinit(dev, (struct vis_devinit *)data)) != 0) {
		cmn_err(CE_CONT, "s3ioctl: Could not initialize console");
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
	    s3_cons_free(softc->conspriv);
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
	    s3_get_hardware_settings(softc);
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
			if (s3_update_pending(softc))
			    s3_int_disable(softc);
#endif

			/*
			 * Copy color map entries from stack to the color map
			 * table in the softc area.
			 */

			s3_cmap_bcopy(iobuf_cmap_red, map++, count);
			s3_cmap_bcopy(iobuf_cmap_green, map++, count);
			s3_cmap_bcopy(iobuf_cmap_blue, map, count);

			/* cursor colormap update */
			if (entries < CMAP_ENTRIES)
			    count = 0;
			s3_update_cmap(softc, index, count);
#ifdef	XXXppc
			s3_int_enable(softc);
#endif
#else
			for (i = 0; i < count; i++) {
				s3_cons_put_cmap(softc,
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

			s3_cmap_bcopy(iobuf_cmap_red, map++, -count);
			s3_cmap_bcopy(iobuf_cmap_green, map++, -count);
			s3_cmap_bcopy(iobuf_cmap_blue, map, -count);
#else
			for (i = 0; i < count; i++) {
				s3_cons_get_cmap(softc,
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
		bcopy((caddr_t)&s3_attr, (caddr_t)&attr, sizeof (attr));
		mutex_enter(&softc->mutex);
		attr.fbtype.fb_type = softc->emulation;
		attr.fbtype.fb_width = softc->_w;
		attr.fbtype.fb_height = softc->_h;
		attr.fbtype.fb_depth = softc->depth;
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
	 * support older software which was staticly-linked before s3 was
	 * invented, and to support newer software which has come to expect
	 * this behavior.
	 */
	case FBIOGTYPE: {
		struct fbtype fb;

		mutex_enter(&softc->mutex);

		bcopy((caddr_t)&s3_attr.fbtype,
			(caddr_t)&fb,
			sizeof (struct fbtype));
		DEBUGF(3, (CE_CONT, "FBIOGTYPE\n"));
		fb.fb_type = FBTYPE_SUN4COLOR;
		fb.fb_width = softc->_w;
		fb.fb_height = softc->_h;
		fb.fb_depth = softc->depth;
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
		softc->pr.pr_ops = &s3_pr_ops;
		/* pr_size set in attach */
		softc->pr.pr_depth = softc->depth;
		softc->pr.pr_data = (caddr_t)&softc->prd;

		/* md_linebytes, md_image set in attach */
		/* md_offset already zero */
		softc->prd.mpr.md_primary = getminor(dev);
		softc->prd.mpr.md_flags = MP_DISPLAY | MP_PLANEMASK;
		softc->prd.planes = 255;

		/* enable video */
		s3_set_video(softc, _ONE_);

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
		s3_set_video(softc, i & FBVIDEO_ON);
		mutex_exit(&softc->mutex);
		break;

	case FBIOGVIDEO:

		DEBUGF(3, (CE_CONT, "FBIOGVIDEO\n"));
		mutex_enter(&softc->mutex);
#ifdef	XXXppc
		i = s3_get_video(softc) ? FBVIDEO_ON : FBVIDEO_OFF;
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
		softc->vrtflag |= S3VRTIOCTL;
#ifdef	XXXppc
		s3_int_enable(softc);
#endif
		cv_wait(&softc->vrtsleep, &softc->mutex);
		mutex_exit(&softc->mutex);
		return (0);

	case FBIOVRTOFFSET:

#ifdef	XXXppc
		i = S3_VADDR_VRT;

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

		s3_setcurpos(softc);

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
			s3_setcurshape(softc);
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
		s3_setcurpos(softc);
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
		s3_debug = (int)data;
		if (s3_debug == -1)
		    s3_debug = DRIVER_DEBUG;
		cmn_err(CE_CONT, "s3_debug is now %d\n", s3_debug);
		break;
#endif

	default:
		return (ENOTTY);
	}				/* switch (cmd) */

	return (0);
}

#ifdef	XXXppc
static  u_int
s3_intr(caddr_t arg)
{
	register struct s3_softc *softc = (struct s3_softc *)arg;
	volatile u_long *in;
	volatile u_long *out;
	volatile u_long  tmp;

	DEBUGF(7, (CE_CONT,
		"s3_intr: softc=%x, vrtflag=%x\n", softc, softc->vrtflag));

	if (!s3_int_pending(softc)) {
	    return (DDI_INTR_UNCLAIMED);	/* nope, not mine */
	}
	mutex_enter(&softc->mutex);
	mutex_enter(&softc->interlock);

	if (!(s3_update_pending(softc) || (softc)->vrtflag)) {
	    /* TODO catch stray interrupts? */
	    s3_int_disable_intr(softc);
	    mutex_exit(&softc->interlock);
	    mutex_exit(&softc->mutex);
	    return (DDI_INTR_CLAIMED);
	}
	if (softc->vrtflag & S3VRTCTR) {
	    if (softc->vrtmaps == 0) {
		softc->vrtflag &= ~S3VRTCTR;
	    } else
		*softc->vrtpage += 1;
	}
	if (softc->vrtflag & S3VRTIOCTL) {
	    softc->vrtflag &= ~S3VRTIOCTL;
	    cv_broadcast(&softc->vrtsleep);
	}
	if (s3_update_pending(softc)) {
	    volatile struct s3_cmap *cmap = S_CMAP(softc);
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
	s3_int_disable_intr(softc);
	if (softc->vrtflag)
	    s3_int_enable(softc);

	mutex_exit(&softc->interlock);
	mutex_exit(&softc->mutex);
	return (DDI_INTR_CLAIMED);
}
#endif	/* XXXppc */

static int
s3_devinit(dev_t dev, struct vis_devinit *data)
{
	struct s3_softc		*softc;
	struct s3_console_private	*private;
	int				unit;

	unit = getminor(dev);
	if (!(softc = getsoftc(unit)))
		return (NULL);

	private = (struct s3_console_private *)
	    kmem_alloc(sizeof (struct s3_console_private), KM_SLEEP);

	if (private == NULL)
		return (ENOMEM);

	/* initialize the private data structure */
	private->softc = softc;
	mutex_init(&private->pmutex, "s3 console lock", MUTEX_DRIVER, NULL);
	private->viscursor = 0;

	/* initialize console instance */
	data->version = VIS_CONS_REV;
	data->height = softc->_h;
	data->width = softc->_w;
	data->depth = softc->depth;
	data->size = softc->size;
	data->linebytes = softc->_linebytes;
	data->mode = (short)VIS_PIXEL;

	mutex_enter(&softc->mutex);
	softc->conspriv = private;
	mutex_exit(&softc->mutex);

	s3_set_video(softc, FBVIDEO_ON);

	return (0);
}

/* The console routines, and may the Lord Have Mercy On Us All. */

static
void
clip(struct s3_softc *softc, struct s3_rect *rp)
{
	/* partial clip to screen */

	rp->h = (rp->y + rp->h > softc->_h) ? softc->_h - rp->y : rp->h;
	rp->w = (rp->x + rp->w > softc->_w) ? softc->_w - rp->x : rp->w;
}


static int
s3_dac_in(struct s3_softc *softc, int index)
{
	int old;

	old = GET_CRTC(softc, S3_EX_DAC_CT);
	SET_CRTC(softc, S3_EX_DAC_CT,
		(old & ~S3_864_EX_DAC_CT_DAC_R_SEL_MASK)
		| (index >> 2) << S3_EX_DAC_CT_DAC_R_SEL_SHIFT);
	return (ddi_io_getb(softc->regs.handle,
		(int)softc->regs.addr + S3_DAC_BASE + ((index & 3)^2)));
}

static void
s3_dac_out(struct s3_softc *softc, int index, int val)
{
	int old;

	old = GET_CRTC(softc, S3_EX_DAC_CT);
	SET_CRTC(softc, S3_EX_DAC_CT,
		(old & ~S3_864_EX_DAC_CT_DAC_R_SEL_MASK)
		| (index >> 2) << S3_EX_DAC_CT_DAC_R_SEL_SHIFT);
	ddi_io_putb(softc->regs.handle,
		(int)softc->regs.addr+S3_DAC_BASE + ((index & 3)^2), val);
}

static void
s3_cons_put_cmap(
	struct s3_softc *softc,
	int index,
	unsigned char r,
	unsigned char g,
	unsigned char b)
{
	s3_dac_out(softc, SDAC_PALETTE_WRITE_ADDR, index);
	s3_dac_out(softc, SDAC_PALETTE_DATA, r >> 2);
	s3_dac_out(softc, SDAC_PALETTE_DATA, g >> 2);
	s3_dac_out(softc, SDAC_PALETTE_DATA, b >> 2);
}

static void
s3_cons_get_cmap(
	struct s3_softc *softc,
	int index,
	unsigned char *r,
	unsigned char *g,
	unsigned char *b)
{
	s3_dac_out(softc, SDAC_PALETTE_READ_ADDR, index);
	*r = s3_dac_in(softc, SDAC_PALETTE_DATA) << 2;
	*g = s3_dac_in(softc, SDAC_PALETTE_DATA) << 2;
	*b = s3_dac_in(softc, SDAC_PALETTE_DATA) << 2;
}

static
void
s3_cons_free(struct s3_console_private *cp)
{
	mutex_destroy(&cp->pmutex);

	kmem_free(cp, sizeof (struct s3_console_private));
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
#define	NEED_0RGB	0	/* No 0RGB support needed */
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

static void	s3_cons_copy_8(struct s3_softc *,
			struct vis_conscopy *);
static void	s3_cons_display_8(struct s3_softc *,
			struct vis_consdisplay *);
static void	s3_cons_cursor_8(struct s3_softc *,
			struct vis_conscursor *);
static void	s3_bcopy_8(pixel8 *, pixel8 *, int);
static void	s3_cons_copy_32(struct s3_softc *,
			struct vis_conscopy *);
#if	NEED_0RGB
static void	s3_cons_display_32_0RGB(struct s3_softc *,
			struct vis_consdisplay *);
static void	s3_cons_cursor_32_0RGB(struct s3_softc *,
			struct vis_conscursor *);
#endif
#if	NEED_BGR0
static void	s3_cons_display_32_BGR0(struct s3_softc *,
			struct vis_consdisplay *);
static void	s3_cons_cursor_32_BGR0(struct s3_softc *,
			struct vis_conscursor *);
#endif
static void	s3_bcopy_32(pixel32 *, pixel32 *, int);

static void
s3_cons_copy_8(struct s3_softc *softc, struct vis_conscopy *pma)
{
	register pixel8	*srcp;
	register pixel8	*dstp;
	register pixel8	*sp;
	register pixel8	*dp;
	register int		i;
	struct s3_rect	r;

	r.x = pma->t_col;
	r.y = pma->t_row;
	r.w = pma->e_col - pma->s_col + 1;
	r.h = pma->e_row - pma->s_row + 1;

	clip(softc,  &r);

	switch (pma->direction) {
	case VIS_COPY_FORWARD:
		dstp = (pixel8 *)softc->_fb +
			pma->t_row * softc->_w + pma->t_col;
		srcp = (pixel8 *)softc->_fb +
			pma->s_row * softc->_w + pma->s_col;

		while (r.h--) {
		    s3_bcopy_8(srcp, dstp, r.w);
		    dstp += softc->_w;
		    srcp += softc->_w;
		}
		break;

	case VIS_COPY_BACKWARD:
		dstp = (pixel8 *)softc->_fb +
			(pma->t_row + r.h - 1) * softc->_w + pma->t_col;
		srcp = (pixel8 *)softc->_fb +
			pma->e_row * softc->_w + pma->s_col;

		if (pma->s_row == pma->t_row &&
		    pma->t_col > pma->s_col &&
		    pma->t_col < pma->s_col + r.w) {
			while (r.h--) {
				sp = srcp + r.w - 1;
				dp = dstp + r.w - 1;
				for (i = 0; i < r.w; i++)
					*dp-- = *sp--;
				dstp -= softc->_w;
				srcp -= softc->_w;
			}
		} else {
			while (r.h--) {
				s3_bcopy_8(srcp, dstp, r.w);
				dstp -= softc->_w;
				srcp -= softc->_w;
			}
		}
		break;
	}
}

static void
s3_cons_display_8(struct s3_softc *softc, struct vis_consdisplay *pda)
{
	register pixel8		*scrcp;
	register pixel8		*imgcp;
	struct s3_rect	r;

	r.x = pda->col;
	r.y = pda->row;
	r.w = pda->width;
	r.h = pda->height;

	clip(softc,  &r);

	scrcp = (pixel8 *)softc->_fb +
		r.y * softc->_w + r.x;
	imgcp = (pixel8 *)pda->data;

	while (r.h--) {
		s3_bcopy_8(imgcp, scrcp, r.w);
		imgcp += r.w;
		scrcp += softc->_w;
	}
}

/*
 * Cursor display function for 8-bit pixels
 */
static void
s3_cons_cursor_8(
	struct s3_softc *softc,
	struct vis_conscursor *pca)
{
	register pixel8		*scrcp;
	register pixel8		*rp;
	register int		i;
	register pixel8		fg;
	register pixel8		bg;

	fg = pca->fg_color.eight;
	bg = pca->bg_color.eight;

	scrcp = (pixel8 *)softc->_fb +
		pca->row * softc->_w + pca->col;

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
	    scrcp += softc->_w;
	}
}

/*
 * 32-bit screen-to-screen copy.  Byte order doesn't matter here,
 * because we're always copying 32 bits screen-to-screen.
 */
static void
s3_cons_copy_32(struct s3_softc *softc, struct vis_conscopy *pma)
{
	register pixel32	*srcp;
	register pixel32	*dstp;
	register pixel32	*sp;
	register pixel32	*dp;
	register int		i;
	struct s3_rect	r;

	r.x = pma->t_col;
	r.y = pma->t_row;
	r.w = pma->e_col - pma->s_col + 1;
	r.h = pma->e_row - pma->s_row + 1;

	clip(softc,  &r);

	switch (pma->direction) {
	case VIS_COPY_FORWARD:
		dstp = (pixel32 *)softc->_fb +
			pma->t_row * softc->_w + pma->t_col;
		srcp = (pixel32 *)softc->_fb +
			pma->s_row * softc->_w + pma->s_col;

		while (r.h--) {
		    s3_bcopy_32(srcp, dstp, r.w);
		    dstp += softc->_w;
		    srcp += softc->_w;
		}
		break;

	case VIS_COPY_BACKWARD:
		dstp = (pixel32 *)softc->_fb +
			(pma->t_row + r.h - 1) * softc->_w + pma->t_col;
		srcp = (pixel32 *)softc->_fb +
			pma->e_row * softc->_w + pma->s_col;

		if (pma->s_row == pma->t_row &&
		    pma->t_col > pma->s_col &&
		    pma->t_col < pma->s_col + r.w) {
			while (r.h--) {
				sp = srcp + r.w - 1;
				dp = dstp + r.w - 1;
				for (i = 0; i < r.w; i++)
					*dp-- = *sp--;
				dstp -= softc->_w;
				srcp -= softc->_w;
			}
		} else {
			while (r.h--) {
				s3_bcopy_32(srcp, dstp, r.w);
				dstp -= softc->_w;
				srcp -= softc->_w;
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
s3_cons_display_32_0RGB(
	struct s3_softc *softc,
	struct vis_consdisplay *pda)
{
	register pixel32	*scrcp;
	register pixel32	*imgcp;
	struct s3_rect	r;

	r.x = pda->col;
	r.y = pda->row;
	r.w = pda->width;
	r.h = pda->height;

	clip(softc,  &r);

	scrcp = (pixel32 *)softc->_fb +
		r.y * softc->_w + r.x;
	imgcp = (pixel32 *)pda->data;

	while (r.h--) {
		s3_bcopy_32(imgcp, scrcp, r.w);
		imgcp += r.w;
		scrcp += softc->_w;
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
s3_cons_cursor_32_0RGB(
	struct s3_softc *softc,
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

	scrcp = (pixel32 *)softc->_fb +
		pca->row * softc->_w + pca->col;

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
	    scrcp += softc->_w;
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
s3_cons_display_32_BGR0(
	struct s3_softc *softc,
	struct vis_consdisplay *pda)
{
	register pixel32	*scr_row_ptr;
	register pixel32	*img_row_ptr;
	register pixel32	*scr_ptr;
	register pixel32	*img_ptr;
	struct s3_rect	r;
	union pixel32_BGR0	scr_pix;
	union pixel32_0RGB	img_pix;
	register int		i;

	r.x = pda->col;
	r.y = pda->row;
	r.w = pda->width;
	r.h = pda->height;

	clip(softc,  &r);

	scr_row_ptr = (pixel32 *)softc->_fb +
		r.y * softc->_w + r.x;
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
		scr_row_ptr += softc->_w;
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
s3_cons_cursor_32_BGR0(
	struct s3_softc *softc,
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

	scrcp = (pixel32 *)softc->_fb +
		pca->row * softc->_w + pca->col;

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
	    scrcp += softc->_w;
	}
}
#endif

/* XXXppc:  For unknown reasons, the stock bcopy faults when called from */
/* inside kadb.  Or something like that. */
static void
s3_bcopy_8(pixel8 *from, pixel8 *to, int count)
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
s3_bcopy_32(pixel32 *from, pixel32 *to, int count)
{
	while (count-- > 0) {
		*to++ = *from++;
	}
}

/*
 * And that's the end of the terminal emulation "workhorse" routines.
 * Hope you had fun.
 */

/*
 * Initialize a colormap: background = white, all others = black
 */
static void
s3_reset_cmap(volatile u_char *cmap, register u_int entries)
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
s3_update_cmap(struct s3_softc *softc, u_int index, u_int count)
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
s3_cmap_bcopy(register u_char *bufp, register u_char *rgb, u_int count)
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
static void
s3_setcurpos(struct s3_softc *softc)
{
	int leftx;
	int topy;

	/*
	 * Might as well avoid any potential glitches by turning the
	 * cursor off first, since we can't update its position atomically.
	 */
	SET_CRTC(softc, S3_HGC_MODE,
	    GET_CRTC(softc, S3_HGC_MODE) & ~S3_HGC_MODE_HWGC_ENB);

	/*
	 * and then turn it on only if it's enabled.
	 */
	if (softc->cur.enable) {
		leftx = softc->cur.pos.x - softc->cur.hot.x;
		leftx &= S3_HWGC_ORGX_MASK;
		SET_CRTC(softc, S3_HWGC_ORGXH, leftx >> 8);
		SET_CRTC(softc, S3_HWGC_ORGXL, leftx & 0xff);

		topy = softc->cur.pos.y - softc->cur.hot.y;
		topy &= S3_HWGC_ORGY_MASK;
		SET_CRTC(softc, S3_HWGC_ORGYH, topy >> 8);
		SET_CRTC(softc, S3_HWGC_ORGYL, topy & 0xff);

		SET_CRTC(softc, S3_HGC_MODE,
		    GET_CRTC(softc, S3_HGC_MODE) | S3_HGC_MODE_HWGC_ENB);
	}
}

/*
 * load HW cursor bitmaps
 */
/*ARGSUSED*/
static void
s3_setcurshape(struct s3_softc *softc)
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
s3_reset(struct s3_softc *softc)
{
#ifdef	XXXppc
	volatile struct thc *thc = S_THC(softc);

	/* disable HW cursor */
	thc->l_thc_cursor = S3_CURSOR_OFFPOS;

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
	    volatile struct s3_cmap *cmap = S_CMAP(softc);

	    /* command register */
	    cmap->addr = 6 << 24;

	    /* turn on CR1:0, overlay enable */
	    cmap->ctrl = cmap->ctrl | (0x3 << 24);
	}
#else
	/* NYI:  not implemented */
	s3_setcurpos(softc);
#endif	/* XXXppc */
}

	/*
	 * This code is no longer used, since OBP proms now do all device
	 * initialization. Nevertheless, it is instructive and I'm going to
	 * keep it in as a comment, should anyone ever want to know how to
	 * do minimal device initialization. Note the c++ style embedded
	 * comments.
	 *
	 * s3_init(softc)
	 *	struct s3_softc *softc;
	 *{
	 *	// Initialize DAC
	 *	{
	 *	    register struct s3_cmap *cmap = S_CMAP(softc);
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
	 *	    DEBUGF(1, (CE_CONT, "s3_init: FBC rev %d\n", rev));
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

#define	S3MAP_SHARED	0x02	/* shared context */
#define	S3MAP_VRT	0x04	/* vrt page */
#define	S3MAP_FBCTEC	0X08	/* mapping includes fbc and/or tec */
#define	S3MAP_FB	0X10	/* mapping includes framebuffer */

#define	S3MAP_CTX	(S3MAP_FBCTEC | S3MAP_FB)	/* needs context */

static struct s3map_pvt *
s3_pvt_alloc(struct s3_cntxt *ctx,
		u_int off,
		u_int len,
		struct s3_softc *softc)
{
	struct s3map_pvt *pvt;

	/*
	 * create the private data portion of the mapdev object
	 */
	pvt = (struct s3map_pvt *)kmem_zalloc(sizeof (struct s3map_pvt),
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
 * the creation of lego (s3) segments.
 */
/*ARGSUSED*/
static int
s3_segmap(dev_t	dev,
	    off_t	off,
	    struct as	*as,
	    caddr_t	*addrp,
	    off_t	len,
	    u_int	prot,
	    u_int	maxprot,
	    u_int	flags,
	    cred_t	*cred)
{
	register struct s3_softc *softc = getsoftc(getminor(dev));

	struct s3_cntxt *ctx		= (struct s3_cntxt *)NULL;
	struct s3_cntxt *shared_ctx	= &softc->shared_ctx;
	struct s3map_pvt *pvt;
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
	if (off + len > 0 &&
	    off < softc->fbmappable)
	    maptype |= S3MAP_FB;

#ifdef	XXXppc
	/* decide if this segment is part of a context. */

	ctxmap = (softc->chiprev == 5) ?
		    (S3MAP_FBCTEC|S3MAP_FB) : S3MAP_FBCTEC;
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
		    ctx->flag = S3MAP_CTX;
	    } else {
		    ctx = ctx_map_insert(softc, maptype);
		    ctx->flag |= maptype;
	    }

	    pvt = s3_pvt_alloc(ctx, 0, len, softc);

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
				    &s3map_ops,
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
s3map_access(ddi_mapdev_handle_t handle, void *pvt, off_t offset)
{
#ifdef	XXXppc
	struct s3map_pvt *p   = (struct s3map_pvt *)pvt;
	struct s3map_pvt *pvts;
	struct s3_softc *softc = p->softc;
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
		if (softc->curctx != (struct s3_cntxt *)NULL) {
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

			if (s3_cntxsave(fbc, S_TEC(softc),
					softc->curctx) == 0) {
				DEBUGF(1, (CE_CONT,
				    "s3: context save failed\n"));
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
		S3DELAY(!(fbc->l_fbc_status & L_FBC_BUSY), S3_FBC_WAIT);
		if (fbc->l_fbc_status & L_FBC_BUSY) {
			DEBUGF(1, (CE_CONT, "s3: idle_s3: status = %x\n",
			fbc->l_fbc_status));
			/*
			 * At this point we have no current context.
			 */
			softc->curctx = NULL;
			mutex_exit(&softc->mutex);
			return (-1);
		}

		DEBUGF(4, (CE_CONT, "loading context %x\n", p->context));

		if (p->context->flag & S3MAP_FBCTEC)
			if (s3_cntxrestore(fbc, S_TEC(softc),
						p->context) == 0) {
				DEBUGF(1, (CE_CONT,
				    "s3: context restore failed\n"));
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
s3map_free(ddi_mapdev_handle_t handle, void *pvt)
{
	struct s3map_pvt *p   = (struct s3map_pvt *)pvt;
	struct s3map_pvt *pvts;
	struct s3map_pvt *ppvts;
	struct s3_cntxt *ctx   = p->context;
	struct s3_softc *softc = p->softc;
	struct s3_cntxt *shared_ctx	= &softc->shared_ctx;

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
			kmem_free(pvt, sizeof (struct s3map_pvt));
			break;
		}
		ppvts = pvts;
	}

	/*
	 * Remove the context if this is not the shared context and there are
	 * no more associated pvt's
	 */
	if ((ctx != shared_ctx) && (ctx->pvt == NULL)) {
		register struct s3_cntxt *ctxptr;

		if (ctx == softc->curctx)
			softc->curctx = NULL;

		/*
		 * Scan private context list for entry to remove.
		 * Check first to see if it's the head of our list.
		 */
		if (softc->pvt_ctx == ctx) {
			softc->pvt_ctx = ctx->link;
			kmem_free(ctx, sizeof (struct s3_cntxt));
		} else {
			for (ctxptr = softc->pvt_ctx; ctxptr != NULL;
				ctxptr = ctxptr->link) {
				if (ctxptr->link == ctx) {
					ctxptr->link = ctx->link;
					kmem_free(ctx,
						sizeof (struct s3_cntxt));
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
s3map_dup(ddi_mapdev_handle_t old_handle,
	    void *oldpvt,
	    ddi_mapdev_handle_t new_handle,
	    void **newpvt)
{
	struct s3map_pvt *p   = (struct s3map_pvt *)oldpvt;
	struct s3_softc *softc = p->softc;
	struct s3map_pvt *pvt;
	struct s3_cntxt *ctx;

	mutex_enter(&softc->mutex);
	if (p->context != &softc->shared_ctx) {
		ctx = (struct s3_cntxt *)
		    kmem_zalloc(sizeof (struct s3_cntxt), KM_SLEEP);
		*ctx = *p->context;
		ctx->pvt = NULL;
	} else
		ctx = &softc->shared_ctx;

	pvt = s3_pvt_alloc(ctx, 0, p->len, softc);

	pvt->handle = new_handle;
	*newpvt = pvt;

	if (p->context && (p->context->flag & S3MAP_VRT)) {
	    softc->vrtflag |= S3VRTCTR;
#ifdef	XXXppc
	    if (softc->vrtmaps == 0)
		s3_int_enable(softc);
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
s3_cntxsave(fbc, tec, saved)
	volatile struct fbc *fbc;
	volatile struct tec *tec;
	struct s3_cntxt *saved;
{
	int    dreg;		/* counts through the data registers */
	u_int  *dp;			/* points to a tec data register */

	DEBUGF(5, (CE_CONT, "saving registers for %d\n", saved->pid));

	CDELAY(!(fbc->l_fbc_status & L_FBC_BUSY), S3_FBC_WAIT);
	if (fbc->l_fbc_status & L_FBC_BUSY) {
	    DEBUGF(1, (CE_CONT, "s3: idle_s3: status = %x\n",
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
s3_cntxrestore(fbc, tec, saved)
	volatile struct fbc *fbc;
	volatile struct tec *tec;
	struct s3_cntxt *saved;
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
struct s3_cntxt *
ctx_map_insert(struct s3_softc *softc, int maptype)
{
	register struct s3_cntxt *ctx;
	int curpid = getpid();

	DEBUGF(4, (CE_CONT, "ctx_map_insert: maptype=0x%x curpid=%d\n",
		maptype, curpid));

	/*
	 * If this is the first time we're here, then alloc space
	 * for new context and depart.
	 */
	if (softc->pvt_ctx == NULL) {
		ctx = (struct s3_cntxt *)
			kmem_zalloc(sizeof (struct s3_cntxt), KM_SLEEP);
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
		    (maptype & ctx->flag & (S3MAP_FBCTEC|S3MAP_FB)) == 0)
			break;
	}


	/* no match, create a new one and add to softc list */
	if (ctx == NULL) {
		ctx = (struct s3_cntxt *)
			kmem_zalloc(sizeof (struct s3_cntxt), KM_SLEEP);
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

void
s3_set_indexed(
struct s3_softc *softc,
caddr_t indexreg,
caddr_t datareg,
int index,
int val)
{
	ddi_io_putb(softc->regs.handle, (int)indexreg, index);
	ddi_io_putb(softc->regs.handle, (int)datareg, val);
}

int
s3_get_indexed(
struct s3_softc *softc,
caddr_t indexreg,
caddr_t datareg,
int index)
{
	int i;

	ddi_io_putb(softc->regs.handle, (int)indexreg, index);
	i = ddi_io_getb(softc->regs.handle, (int)datareg);
	return (i);
}

#if	DRIVER_DEBUG
static void
s3_register_dump(struct s3_softc *softc)
{
	int i, j;

	for (i = 0; i < 0x60; i += 0x10) {
		printf("%2x:  ", i);
		for (j = 0; j < 0x08; j++) {
			printf("%2x ", GET_CRTC(softc, i+j));
		}
		printf("- ");
		for (; j < 0x10; j++) {
			printf("%2x ", GET_CRTC(softc, i+j));
		}
		printf("\n");
	}
}
#endif

#define	GET_HORIZ_END(c)	(GET_CRTC(c, VGA_CRTC_H_D_END) \
	+ (((GET_CRTC(c, S3_EXT_H_OVF) >> S3_EXT_H_OVF_HDE8) & 1) << 8))
#define	GET_VERT_END(c)	(GET_CRTC(c, VGA_CRTC_VDE) \
	+ (((GET_CRTC(c, VGA_CRTC_OVFL_REG) >> \
	    VGA_CRTC_OVFL_REG_VDE8) & 1) << 8) \
	+ (((GET_CRTC(c, VGA_CRTC_OVFL_REG) >> \
	    VGA_CRTC_OVFL_REG_VDE9) & 1) << 9) \
	+ (((GET_CRTC(c, S3_EXT_V_OVF) >> S3_EXT_V_OVF_VDE10) & 1) << 10))

#define	GET_INTERLACED(c)	\
	(GET_CRTC(c, S3_MODE_CTL) & S3_MODE_CTL_INTL_MODE_MASK)
#define	GET_VERT_X2(c)	\
	(GET_CRTC(c, VGA_CRTC_CRT_MD) & VGA_CRTC_CRT_MD_VT_X2)

static void
s3_get_hardware_settings(struct s3_softc *softc)
{
	unsigned char pixel_code;

	mutex_enter(&softc->mutex);
	softc->_h = GET_VERT_END(softc)+1;
#ifdef	GET_INTERLACED
	if (GET_INTERLACED(softc)) softc->_h *= 2;
#endif
	if (GET_VERT_X2(softc)) softc->_h *= 2;

	pixel_code = GET_CRTC(softc, S3_EX_SCTL_1) & S3_EX_SCTL_1_PXL_LNGTH;

	switch (pixel_code) {
	case S3_EX_SCTL_1_PXL_LNGTH_32:
	    softc->display_func = s3_cons_display_32_BGR0;
	    softc->cursor_func = s3_cons_cursor_32_BGR0;
	    softc->copy_func = s3_cons_copy_32;
	    softc->depth = 24;	/* XXXPPC:  32?  But ltem wants 24. */
	    softc->_w = (GET_HORIZ_END(softc)+1)*2;
	    break;

	default:
	    cmn_err(CE_WARN,
		MYNAME ":  Pixel size not supported.  Size code 0x%x.",
		pixel_code);
		/*
		 * There's no clean way to fail, and it might be
		 * worse if we did, so...
		 */
		/* FALLTHROUGH */
	case S3_EX_SCTL_1_PXL_LNGTH_8:
	    softc->copy_func = s3_cons_copy_8;
	    softc->display_func = s3_cons_display_8;
	    softc->cursor_func = s3_cons_cursor_8;
	    softc->depth = 8;
	    softc->_w = (GET_HORIZ_END(softc)+1)*8;
	    break;
	}
	mutex_exit(&softc->mutex);
}
