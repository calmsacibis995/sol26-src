/*
 * Copyright(c) 1994-1995 by Sun Microsystems, Inc.  All Rights Reserved.
 */

#pragma	ident	"@(#)ltem.c	1.31	95/11/21 SMI"

/*
 * IWE ANSI module; parse for character input string for
 * ANSI X3.64 escape sequences and the like.
 */

#include <sys/types.h>
#include <sys/file.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/open.h>
#include <sys/cred.h>
#include <sys/kmem.h>
#include <sys/ascii.h>
#include <sys/consdev.h>
#include <sys/font.h>
#include <sys/fbio.h>
#include <sys/conf.h>
#include <sys/modctl.h>
#include <sys/strsubr.h>
#include <sys/stat.h>
#include <sys/visual_io.h>
#include <sys/ltem.h>
#include <sys/tem.h>
#include <sys/param.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/console.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sunddi_lyr.h>


/* Terminal emulator functions */
static int	tem_init(struct vis_devinit *, struct ltem_state *, dev_t,
			cred_t *);
static void	tem_control(struct ltem_state *, unchar,
			cred_t *, u_int);
static void	tem_setparam(temstat_t *, int, ushort);
static void	tem_selgraph(temstat_t *);
static void	tem_chkparam(struct ltem_state *, unchar,
			cred_t *, u_int);
static void	tem_getparams(struct ltem_state *, unchar,
			cred_t *, u_int);
static void	tem_outch(struct ltem_state *, unchar,
			cred_t *, u_int);
static void	tem_parse(struct ltem_state *, unchar,
			cred_t *, u_int);
static void	tem_new_line(struct ltem_state *,
			cred_t *, u_int);
static void	tem_cr(temstat_t *);
static void	tem_lf(struct ltem_state *,
			cred_t *, u_int);
static void	tem_send_data(struct ltem_state *, cred_t *, u_int);
static void	tem_align_cursor(temstat_t *);
static void	tem_cls(struct ltem_state *,
			cred_t *, u_int);
static void	tem_clear_entire(struct ltem_state *,
			cred_t *, u_int);
static void	tem_reset_display(struct ltem_state *,
			cred_t *, u_int);
static void	tem_tab(struct ltem_state *,
			cred_t *, u_int);
static void	tem_back_tab(temstat_t *);
static void	tem_clear_tabs(temstat_t *, int);
static void	tem_set_tab(temstat_t *);
static void	tem_mv_cursor(struct ltem_state *, int, int,
			cred_t *, u_int);
static void	tem_shift(struct ltem_state *, int, int,
			cred_t *, u_int);
static void	tem_scroll(struct ltem_state *, int, int,
			int, int, cred_t *, u_int);
static void	tem_free(temstat_t *);
static void	tem_terminal_emulate(struct ltem_state *, struct uio *,
			cred_t *, u_int);
static void	tem_text_display(struct ltem_state *, unchar *, int,
			screen_pos_t, screen_pos_t, cred_t *,
			u_int);
static void	tem_text_copy(struct ltem_state *,
			screen_pos_t, screen_pos_t,
			screen_pos_t, screen_pos_t,
			screen_pos_t, screen_pos_t, int,
			cred_t *, u_int);
static void	tem_text_cursor(struct ltem_state *, short,
			cred_t *, u_int);
static void	tem_text_cls(struct ltem_state *ltem_sp,
			int count, screen_pos_t row, screen_pos_t col,
			cred_t *credp, u_int from_stand);
static void	tem_pix_display(struct ltem_state *, unchar *, int,
			screen_pos_t, screen_pos_t, cred_t *, u_int kadb);
static void	tem_pix_copy(struct ltem_state *,
			screen_pos_t, screen_pos_t,
			screen_pos_t, screen_pos_t,
			screen_pos_t, screen_pos_t, int,
			cred_t *, u_int);
static void	tem_pix_cursor(struct ltem_state *, short, cred_t *, u_int);
static void	tem_pix_cls(struct ltem_state *ltem_sp,
			int count, screen_pos_t row, screen_pos_t col,
			cred_t *credp, u_int from_stand);
static void	tem_bell(void);
static void	tem_reset_colormap(struct ltem_state *,
			cred_t *, u_int);
static void set_font(struct font *, short *, short *, short, short);
#ifdef HAVE_1BIT
static void bit_to_pix1(struct font *, ushort, unchar, unchar *);
#endif
static void bit_to_pix4(struct font *, ushort, unchar, unchar *);
static void bit_to_pix8(struct font *, ushort, unchar, unchar *);
static void bit_to_pix24(struct font *, ushort, unchar, unchar *);

/* Driver entry points */
static int	ltemidentify(dev_info_t *dip);
static int	ltemattach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int	ltemdetach(dev_info_t *dip, ddi_detach_cmd_t cmd);
static int	lteminfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg,
			void **result);
static int	ltemopen(dev_t *devp, int flag, int otyp, cred_t *credp);
static int	ltemclose(dev_t dev, int flag, int otyp, cred_t *credp);
static int	ltemwrite(dev_t dev, struct uio *uiop, cred_t *credp);
static int	ltemprop_op(dev_t dev, dev_info_t *dip, ddi_prop_op_t prop_op,
			int flags, char *name, caddr_t valuep, int *lengthp);
static int	ltemioctl(dev_t dev, int cmd, int arg, int mode,
			cred_t *credp, int *rvalp);

#define	INVERSE(ch) (ch ^ 0xff)

extern bitmap_data_t builtin_font_data;
extern struct fontlist fonts[];


#define	bit_to_pix(ap, c)	{ \
	ASSERT((ap)->in_fp.f_bit2pix != NULL); \
	(void) (*(ap)->in_fp.f_bit2pix)(&(ap)->a_font, (ap)->a_flags, (c), \
	    (ap)->a_pix_data); \
}

static struct cb_ops	ltem_cb_ops = {
	ltemopen,		/* open */
	ltemclose,		/* close */
	nodev,			/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	nodev,			/* read */
	ltemwrite,		/* write */
	ltemioctl,		/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev,			/* segmap */
	nochpoll,		/* poll */
	ltemprop_op,		/* prop_op */
	(struct streamtab *)0,	/* streamtab */
	D_NEW | D_MP,		/* flags */
};

static struct dev_ops	ltem_ops = {
	DEVO_REV,		/* rev */
	0,			/* refcnt */
	lteminfo,		/* info */
	ltemidentify,		/* identify */
	nulldev,		/* probe */
	ltemattach,		/* attach */
	ltemdetach,		/* detach */
	nulldev,		/* reset */
	&ltem_cb_ops,		/* cb_ops */
	(struct bus_ops *)0,	/* bus_ops */
};

static struct modldrv	ltem_driver_info = {
	&mod_driverops,		/* modops */
	"Layered ANSI T.E.",	/* name */
	&ltem_ops,		/* dev_ops */
};

static struct modlinkage ltem_linkage = {
	MODREV_1,			/* rev */
	{				/* linkage */
		&ltem_driver_info,
		NULL,
		NULL,
		NULL,
	},
};

static void	*ltem_state_head;

int
_init()
{
	int e;

	if ((e = ddi_soft_state_init(&ltem_state_head,
		sizeof (struct ltem_state), 1)) != 0) {
			return (e);
	}

	if ((e = mod_install(&ltem_linkage)) != 0)  {
		ddi_soft_state_fini(&ltem_state_head);
	}

	return (e);
}

/*
 * We prevent the module from unloading by returning EBUSY from fini.
 */

int
_fini()
{

	int e;

	if ((e = mod_remove(&ltem_linkage)) != 0)  {
		return (e);
	}

	ddi_soft_state_fini(&ltem_state_head);

	return (e);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&ltem_linkage, modinfop));
}

/*
 * Driver administration entry points
 */

static int
ltemidentify(dev_info_t *dip)
{

	if (strcmp(ddi_get_name(dip), "ltem") == 0)
		return (DDI_IDENTIFIED);
	else
		return (DDI_NOT_IDENTIFIED);
}

static int
ltemattach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{

	int			instance;
	struct ltem_state	*ltem_sp;

	instance = ddi_get_instance(dip);

	/* check command */
	if (cmd != DDI_ATTACH) {
		return (DDI_FAILURE);
	}

	/*
	 * Initialize a softstate structure for this instance of the clone
	 * device
	 */
	if (ddi_soft_state_zalloc(ltem_state_head, instance) !=
	    DDI_SUCCESS) {
		cmn_err(CE_NOTE, "%s%d: can't allocate state",
			ddi_get_name(dip), instance);
		return (DDI_FAILURE);
	}

	ltem_sp = ddi_get_soft_state(ltem_state_head, instance);
	ltem_sp->dip = dip;
	mutex_init(&ltem_sp->lock, "Layered ANSI T.E. Lock",
	    MUTEX_DRIVER, (void *)NULL);

	/* create minor node */
	if (ddi_create_minor_node(dip, "ansi_cons", S_IFCHR, instance,
	    DDI_PSEUDO, 0) != DDI_SUCCESS) {
		cmn_err(CE_NOTE,
		"ltem: attach: %d: ddi_create_minor_node '%s' failed",
		    instance, "ansi_cons");
		mutex_destroy(&ltem_sp->lock);
		ddi_soft_state_free(ltem_state_head, instance);
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}


static int
ltemdetach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	int			instance;
	struct ltem_state	*ltem_sp;

	instance = ddi_get_instance(dip);
	ltem_sp = ddi_get_soft_state(ltem_state_head, instance);

	switch (cmd) {
	case DDI_DETACH:
		ddi_remove_minor_node(dip, (char *)NULL);
		mutex_destroy(&ltem_sp->lock);
		ddi_soft_state_free(ltem_state_head, instance);
		break;

	default:
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}

/* ARGSUSED */
static int
lteminfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg, void **result)
{
	dev_t			dev;
	int			instance;
	struct ltem_state	*ltem_sp;
	int			err = DDI_SUCCESS;

	/* process command */
	switch (cmd) {

	case DDI_INFO_DEVT2DEVINFO:
		dev = (dev_t)arg;
		instance = getminor(dev);
		ltem_sp = get_soft_state(dev);

		if (ltem_sp != NULL) {
			*result = (void *)ltem_sp->dip;
		} else {
			*result = NULL;
			err = DDI_FAILURE;
		}
		break;

	case DDI_INFO_DEVT2INSTANCE:
		dev = (dev_t)arg;
		instance = getminor(dev);

		*result = (void *)instance;
		break;

	default:
		err = DDI_FAILURE;
		break;
	}

	return (err);
}

/*ARGSUSED*/
static int
ltemopen(dev_t *devp, int flag, int otyp, cred_t *credp)
{

	if (otyp != OTYP_CHR)
		return (ENXIO);

	if (flag & FREAD) {
		return (ENXIO);
	}

	return (0);
}

/*ARGSUSED*/
static int
ltemclose(dev_t dev, int flag, int otyp, cred_t *credp)
{
	struct ltem_state	*ltem_sp;
	int lyr_rval;

	ltem_sp = get_soft_state(dev);

	mutex_enter(&ltem_sp->lock);

	if (ltem_sp->hdl != NULL) {
		/*
		 * Allow layered on driver to clean up console private
		 * data.
		 */
		(void) ddi_lyr_ioctl(ltem_sp->hdl, VIS_DEVFINI,
		    0, FKIOCTL, credp, &lyr_rval);

		/*
		 * Close layered on driver
		 */
		ddi_lyr_close(ltem_sp->hdl, credp);
		ltem_sp->hdl = NULL;
	}

	if (ltem_sp->tem_state != NULL)
		tem_free(ltem_sp->tem_state);

	ltem_sp->linebytes = 0;
	ltem_sp->size = 0;
	ltem_sp->display_mode = 0;

	mutex_exit(&ltem_sp->lock);

	return (0);
}

static int
ltemwrite(dev_t dev, struct uio *uiop, cred_t *credp)
{
	struct ltem_state *ltem_sp;

	ltem_sp = get_soft_state(dev);

	mutex_enter(&ltem_sp->lock);

	if (ltem_sp->hdl != NULL) {
		tem_terminal_emulate(ltem_sp, uiop, credp,
		    TEM_NOT_FROM_STAND);
	} else {
		mutex_exit(&ltem_sp->lock);
		return (ENXIO);
	}

	mutex_exit(&ltem_sp->lock);
	return (0);
}

static int
ltemprop_op(dev_t dev, dev_info_t *dip, ddi_prop_op_t prop_op,
	int flags, char *name, caddr_t valuep, int *lengthp)
{
	struct ltem_state *ltem_sp;
	int	ret = DDI_PROP_NOT_FOUND;

	ltem_sp = get_soft_state(dev);

	mutex_enter(&ltem_sp->lock);
	if (ltem_sp->hdl != NULL) {
		ret = ddi_lyr_prop_op(ltem_sp->hdl, prop_op, flags,
		    name, valuep, lengthp);
	}
	mutex_exit(&ltem_sp->lock);

	/*
	 * If the layered on device does not have the property, check to see
	 * if we do.
	 */
	if (ret != DDI_PROP_SUCCESS)
		ret = ddi_prop_op(dev, dip, prop_op, flags, name, valuep,
		    lengthp);

	return (ret);
}

static int
ltemioctl(dev_t dev, int cmd, int arg, int mode, cred_t *credp, int *rvalp)
{
	struct ltem_state *ltem_sp;
	int	err = 0;
	int	lyr_rval;
	u_char	buf[15];
	struct vis_devinit temargs;
	int	otype;
	int	lyrproplen;
	char	*pathname;

	switch (cmd) {
	/*
	 * Open the layered on device.  The argument is a string of
	 * MAXPATHLEN length which is the physical pathname of the device to
	 * layer on.
	 */
	case LTEM_OPEN:

		ltem_sp = get_soft_state(dev);

		mutex_enter(&ltem_sp->lock);

		/*
		 * Layered device is already open so just return
		 */
		if (ltem_sp->hdl != NULL)
			break;

		pathname = kmem_alloc(MAXPATHLEN, KM_SLEEP);
		if (ddi_copyin((caddr_t)arg, pathname, MAXPATHLEN, mode)) {
			kmem_free(pathname, MAXPATHLEN);
			err = EFAULT;
			break;
		}

		/*
		 * Open the layered device using the physical device name
		 */
		if (ddi_lyr_open_by_name(pathname, FWRITE,
		    &otype, credp, &ltem_sp->hdl) != 0) {
			kmem_free(pathname, MAXPATHLEN);
			ltem_sp->hdl = NULL;
			err = ENXIO;
			break;
		}

		/*
		 * Pathname is no longer needed.
		 */
		kmem_free(pathname, MAXPATHLEN);

		/*
		 * Check to see if the layered device supports layering.
		 * If not, just quietly go away and use the prom if we
		 * have one.
		 */
		if (ddi_lyr_prop_op(ltem_sp->hdl, PROP_LEN,
		    DDI_PROP_CANSLEEP, DDI_KERNEL_IOCTL,
		    NULL, &lyrproplen) != DDI_PROP_SUCCESS) {
			ddi_lyr_close(ltem_sp->hdl, credp);
			ltem_sp->hdl = NULL;
			err = ENXIO;
			break;
		}

		/*
		 * Initialize the console and get the device parameters
		 */
		if (ddi_lyr_ioctl(ltem_sp->hdl, VIS_DEVINIT,
		    (int)&temargs, mode|FKIOCTL, credp, &lyr_rval) != 0) {
			ddi_lyr_close(ltem_sp->hdl, credp);
			ltem_sp->hdl = NULL;
			err = ENXIO;
			break;
		}

		ltem_sp->linebytes = temargs.linebytes;
		ltem_sp->size = temargs.size;
		ltem_sp->display_mode = temargs.mode;

		/*
		 * Initialize the terminal emulator
		 */
		if ((err = tem_init(&temargs, ltem_sp, dev, credp)) != 0) {
			/*
			 * Allow layered driver to clean up console private
			 * data.
			 */
			(void) ddi_lyr_ioctl(ltem_sp->hdl, VIS_DEVFINI,
			    0, mode|FKIOCTL,
			    credp, &lyr_rval);
			ddi_lyr_close(ltem_sp->hdl, credp);
			ltem_sp->hdl = NULL;
			break;
		}

		/*
		 * Create the console screen size properties
		 */
		sprintf((char *)buf, "%d",
		    ltem_sp->tem_state->a_c_dimension[TEM_COL]);
		(void) ddi_prop_update_byte_array(dev, ltem_sp->dip,
		    "screen-#columns", buf, strlen((char *)buf));
		sprintf((char *)buf, "%d",
		    ltem_sp->tem_state->a_c_dimension[TEM_ROW]);
		(void) ddi_prop_update_byte_array(dev, ltem_sp->dip,
		    "screen-#rows", buf, strlen((char *)buf));
		sprintf((char *)buf, "%d",
		    ltem_sp->tem_state->a_p_dimension[TEM_COL]);
		(void) ddi_prop_update_byte_array(dev, ltem_sp->dip,
		    "screen-width", buf, strlen((char *)buf));
		sprintf((char *)buf, "%d",
		    ltem_sp->tem_state->a_p_dimension[TEM_ROW]);
		(void) ddi_prop_update_byte_array(dev, ltem_sp->dip,
		    "screen-height", buf, strlen((char *)buf));

		/*
		 * Allow standalone writes.
		 */
		ltem_sp->stand_writes = 1;

		break;

	/*
	 * This path MUST be free of calls to the locking and copy routines.
	 *
	 * It is used by kadb on machines without a prom to display text to
	 * the console.
	 */
	case LTEM_STAND_WRITE:

		ltem_sp = get_soft_state(dev);

		if ((ltem_sp->hdl == NULL) || (ltem_sp->stand_writes == 0)) {
			return (ENXIO);
		}

		if (mode & FKIOCTL) {
			/*
			 * We know that we are coming from kadb so the data in
			 * arg is already in kernel space.  Also, we cannot do
			 * a copy as this will panic the system.
			 */
			tem_terminal_emulate(ltem_sp, (struct uio *)arg,
			    credp, TEM_FROM_STAND);
			return (0);
		} else
			return (ENXIO);

	case VIS_CONS_MODE_CHANGE:

		ltem_sp = get_soft_state(dev);
		err = 0;

		mutex_enter(&ltem_sp->lock);

		if (ltem_sp->hdl == NULL) {
			err =  ENXIO;
			break;
		}

		/*
		 * Stop standalone writes.
		 */
		ltem_sp->stand_writes = 0;

		/*
		 * If VIS_DEVFINI fails, the underlying framebuffer is
		 * supposed to maintain the same state it had before the ioctl
		 * was issued.
		 */
		if (ddi_lyr_ioctl(ltem_sp->hdl, VIS_DEVFINI,
		    0, FKIOCTL, credp, &lyr_rval) != 0) {
			cmn_err(CE_WARN, "Could not reset console resolution");
			err = ENXIO;
			break;
		}

		ltem_sp->linebytes = 0;
		ltem_sp->size = 0;
		ltem_sp->display_mode = 0;

		/*
		 * Destroy our current concept of a terminal
		 */
		if (ltem_sp->tem_state != NULL)
			tem_free(ltem_sp->tem_state);

		/*
		 * Allow framebuffer to reset the mode.
		 * The framebuffer driver must set itself to something that
		 * will work even if its not the requested mode.
		 */
		(void) ddi_lyr_ioctl(ltem_sp->hdl, VIS_CONS_MODE_CHANGE,
		    arg, mode|FKIOCTL, credp, &lyr_rval);

		/*
		 * Initialize the console and get the device parameters
		 * If we can't then give up.
		 */
		if (ddi_lyr_ioctl(ltem_sp->hdl, VIS_DEVINIT,
		    (int)&temargs, mode|FKIOCTL, credp, &lyr_rval) != 0) {
			ddi_lyr_close(ltem_sp->hdl, credp);
			err = ENXIO;
			break;
		}

		ltem_sp->linebytes = temargs.linebytes;
		ltem_sp->size = temargs.size;
		ltem_sp->display_mode = temargs.mode;

		/*
		 * We need to reload the fonts so that we can re-initialize
		 * the console and possibly pick a new font size.
		 */
		consoleloadfonts();

		/*
		 * Initialize the terminal emulator
		 */
		if ((err = tem_init(&temargs, ltem_sp, dev, credp)) != 0) {
			/*
			 * Allow layered driver to clean up console private
			 * data.
			 */
			(void) ddi_lyr_ioctl(ltem_sp->hdl, VIS_DEVFINI,
			    0, mode|FKIOCTL,
			    credp, &lyr_rval);
			ddi_lyr_close(ltem_sp->hdl, credp);
			ltem_sp->hdl = NULL;
			break;
		}

		/*
		 * Update the console screen size properties
		 */
		sprintf((char *)buf, "%d",
		    ltem_sp->tem_state->a_c_dimension[TEM_COL]);
		(void) ddi_prop_update_byte_array(dev, ltem_sp->dip,
		    "screen-#columns", buf, strlen((char *)buf));
		sprintf((char *)buf, "%d",
		    ltem_sp->tem_state->a_c_dimension[TEM_ROW]);
		(void) ddi_prop_update_byte_array(dev, ltem_sp->dip,
		    "screen-#rows", buf, strlen((char *)buf));
		sprintf((char *)buf, "%d",
		    ltem_sp->tem_state->a_p_dimension[TEM_COL]);
		(void) ddi_prop_update_byte_array(dev, ltem_sp->dip,
		    "screen-width", buf, strlen((char *)buf));
		sprintf((char *)buf, "%d",
		    ltem_sp->tem_state->a_p_dimension[TEM_ROW]);
		(void) ddi_prop_update_byte_array(dev, ltem_sp->dip,
		    "screen-height", buf, strlen((char *)buf));

		/*
		 * Allow standalone writes.
		 */
		ltem_sp->stand_writes = 1;

		break;

	default:
		/*
		 * If we don't know about this ioctl, pass it on to the
		 * layered on device and see if it knows about it.
		 */
		ltem_sp = get_soft_state(dev);

		if (ltem_sp->hdl == NULL)
			return (ENXIO);

		return (ddi_lyr_ioctl(ltem_sp->hdl, cmd,
		    arg, mode, credp, rvalp));

	}

	mutex_exit(&ltem_sp->lock);
	return (err);
}

static int
tem_init(struct vis_devinit *tp, struct ltem_state *ltem_sp,
    dev_t dev, cred_t *credp)
{
	temstat_t	*ap;
	u_char		*data;
	u_char		*p;
	u_int		len;

	/* Make sure the fb driver and ltem versions match */
	if (tp->version != VIS_CONS_REV) {
		return (EINVAL);
	}

	ap = (temstat_t *)kmem_alloc(sizeof (temstat_t), KM_SLEEP);

	/*
	 * First check to see if the user has specified a screen size from
	 * the prom.  If so, use those values.  Else use 34x80 as the
	 * default.
	 */
	if (ddi_prop_lookup_byte_array(dev, ltem_sp->dip, 0,
	    "screen-#columns", &data, &len) == DDI_PROP_SUCCESS) {
		p = data;
		data[len] = '\0';
		ap->a_c_dimension[TEM_COL] = stoi((char **)&p);
		ddi_prop_free(data);
	} else {
		ap->a_c_dimension[TEM_COL] = TEM_DEFAULT_COLS;
	}

	if (ddi_prop_lookup_byte_array(dev, ltem_sp->dip, 0,
	    "screen-#rows", &data, &len) == DDI_PROP_SUCCESS) {
		p = data;
		data[len] = '\0';
		ap->a_c_dimension[TEM_ROW] = stoi((char **)&p);
		ddi_prop_free(data);
	} else {
		ap->a_c_dimension[TEM_ROW] = TEM_DEFAULT_ROWS;
	}

	ap->a_pdepth = tp->depth;

	switch (tp->mode) {
	case VIS_TEXT:
		ap->in_fp.f_display = tem_text_display;
		ap->in_fp.f_copy = tem_text_copy;
		ap->in_fp.f_cursor = tem_text_cursor;
		ap->in_fp.f_bit2pix = NULL;
		ap->in_fp.f_cls = tem_text_cls;
		ap->a_blank_line =
		    (unchar *)kmem_alloc(ap->a_c_dimension[TEM_COL], KM_SLEEP);

		break;
	case VIS_PIXEL:
		ap->in_fp.f_display = tem_pix_display;
		ap->in_fp.f_copy = tem_pix_copy;
		ap->in_fp.f_cursor = tem_pix_cursor;
		ap->in_fp.f_cls = tem_pix_cls;
		ap->a_blank_line = NULL;
		ap->a_p_dimension[TEM_ROW] = tp->height;
		ap->a_p_dimension[TEM_COL] = tp->width;
		/*
		 * set_font() will select a appropriate sized font for
		 * the number of rows and columns selected.  If we don't
		 * have a font that will fit, then it will use the
		 * default builtin font and adjust the rows and columns
		 * to fit on the screen.
		 */
		set_font(&ap->a_font,
		    &ap->a_c_dimension[TEM_ROW], &ap->a_c_dimension[TEM_COL],
		    ap->a_p_dimension[TEM_ROW], ap->a_p_dimension[TEM_COL]);
		ap->a_p_offset[TEM_ROW] = (ap->a_p_dimension[TEM_ROW] -
		    (ap->a_c_dimension[TEM_ROW] * ap->a_font.height)) / 2;
		ap->a_p_offset[TEM_COL] = (ap->a_p_dimension[TEM_COL] -
		    (ap->a_c_dimension[TEM_COL] * ap->a_font.width)) / 2;

		switch (tp->depth) {
#if defined(HAVE_1BIT)
		case 1:
			ap->in_fp.f_bit2pix = bit_to_pix1;
			ap->a_pix_data_size = ((ap->a_font.width + NBBY
				- 1) / NBBY) * ap->a_font.height;
			break;
#endif HAVE_1BIT
	case 4:
			ap->in_fp.f_bit2pix = bit_to_pix4;
			ap->a_pix_data_size = (((ap->a_font.width * 4) +
				NBBY - 1) / NBBY) * ap->a_font.height;
			break;
		case 8:
			ap->in_fp.f_bit2pix = bit_to_pix8;
			ap->a_pix_data_size = ap->a_font.width *
				ap->a_font.height;
			break;
		case 24:
			ap->in_fp.f_bit2pix = bit_to_pix24;
			ap->a_pix_data_size = ap->a_font.width *
				ap->a_font.height;
			ap->a_pix_data_size *= 4;
			break;
		}

		ap->a_pix_data =
		    (unchar *)kmem_alloc(ap->a_pix_data_size, KM_SLEEP);
		ap->a_cls_pix_data =
		    (unchar *)kmem_alloc(ap->a_pix_data_size, KM_SLEEP);
		ap->a_rcls_pix_data =
		    (unchar *)kmem_alloc(ap->a_pix_data_size, KM_SLEEP);
		break;

	default:
		tem_free(ap);
		return (ENXIO);
	}

	ap->a_outbuf =
	    (unchar *)kmem_alloc(ap->a_c_dimension[TEM_COL], KM_SLEEP);
	/*
	 * Save ap away in the soft state structure
	 */
	ltem_sp->tem_state = ap;

	tem_reset_display(ltem_sp, credp, TEM_NOT_FROM_STAND);

	return (0);
}


static void
tem_free(temstat_t *ap)
{
	if (ap == NULL)
		return;

	if (ap->a_outbuf != NULL)
		kmem_free(ap->a_outbuf, ap->a_c_dimension[TEM_COL]);
	if (ap->a_blank_line != NULL)
		kmem_free(ap->a_blank_line, ap->a_c_dimension[TEM_COL]);
	if (ap->a_pix_data != NULL)
		kmem_free(ap->a_pix_data, ap->a_pix_data_size);
	if (ap->a_cls_pix_data != NULL)
		kmem_free(ap->a_cls_pix_data, ap->a_pix_data_size);
	if (ap->a_rcls_pix_data != NULL)
		kmem_free(ap->a_rcls_pix_data, ap->a_pix_data_size);
	if (ap->a_font.image_data != NULL && ap->a_font.image_data_size > 0)
		kmem_free(ap->a_font.image_data, ap->a_font.image_data_size);
	kmem_free(ap, sizeof (temstat_t));
}


/*
 * This is the main entry point into the terminal emulator.  It is called
 * from both the layered driver write(9E) and ioctl(9E) entry points when
 * they receive data to display on the console.
 *
 * For each data message coming downstream, ANSI assumes that it is composed
 * of ASCII characters, which are treated as a byte-stream input to the
 * parsing state machine. All data is parsed immediately -- there is
 * no enqueing. Data and Terminal Control Language commands obtained from
 * parsing are sent in the same order in which they occur in the data.
 */
static void
tem_terminal_emulate(struct ltem_state *ltem_sp, struct uio *uiop,
    cred_t *credp, u_int from_stand)
{
	register temstat_t	*ap = ltem_sp->tem_state;
	register int i;
	struct iovec *iov;
	long	cnt;
	char buf[BUF_LEN];

	(*ap->in_fp.f_cursor)(ltem_sp, VIS_HIDE_CURSOR, credp,
	    from_stand);

	/*
	 * If we come from kernel space we don't need to do the copy
	 * that uiomove does.  We can just use the data directly.
	 *
	 * The kadb case passes through this path of the code.
	 */
	if (uiop->uio_segflg == UIO_SYSSPACE) {
		while (uiop->uio_resid > 0) {
			iov = uiop->uio_iov;
			cnt = min(iov->iov_len, uiop->uio_resid);
			ASSERT(cnt >= 0);
			if (cnt == 0) {
				uiop->uio_iov++;
				uiop->uio_iovcnt--;
				continue;
			}
			for (i = 0; i < cnt; i++)
				tem_parse(ltem_sp, iov->iov_base[i],
				    credp, from_stand);
			iov->iov_base += cnt;
			iov->iov_len -= cnt;
			uiop->uio_resid -= cnt;
			uiop->uio_loffset += cnt;
		}
	} else {
		/*
		 * If we come from user space, we need to use uiomove to
		 * copy the data into kernel space.
		 */
		while (uiop->uio_resid > 0) {
			cnt = min(BUF_LEN, uiop->uio_resid);
			if (uiomove(buf, cnt, UIO_WRITE, uiop) == -1) {
				return;
			}
			for (i = 0; i < cnt; i++) {
				ASSERT(from_stand == TEM_NOT_FROM_STAND);
				tem_parse(ltem_sp, buf[i],
				    credp, from_stand);
			}
		}
	}

	/*
	 * Send the data we just got to the framebuffer.
	 */
	tem_send_data(ltem_sp, credp, from_stand);

	(*ap->in_fp.f_cursor)(ltem_sp, VIS_DISPLAY_CURSOR, credp,
	    from_stand);
}

static void
tem_reset_colormap(struct ltem_state *ltem_sp,
    cred_t *credp, u_int from_stand)
{
	struct viscmap	cm;
	unchar r[1], b[1], g[1];
	int rval;

	if (from_stand == TEM_FROM_STAND)
		return;

	cm.red = r;
	cm.blue = b;
	cm.green = g;

	cm.index = TEM_TEXT_WHITE;
	cm.count = 1;
	r[0] = (unchar)0xff;
	b[0] = (unchar)0xff;
	g[0] = (unchar)0xff;
	(void) ddi_lyr_ioctl(ltem_sp->hdl, VIS_PUTCMAP, (int)&cm,
	    FKIOCTL, credp, &rval);

	cm.index = TEM_TEXT_BLACK;
	cm.count = 1;
	r[0] = (unchar)0;
	b[0] = (unchar)0;
	g[0] = (unchar)0;
	(void) ddi_lyr_ioctl(ltem_sp->hdl, VIS_PUTCMAP, (int)&cm,
	    FKIOCTL, credp, &rval);
}

/*
 * send the appropriate control message or set state based on the
 * value of the control character ch
 */

static void
tem_control(struct ltem_state *ltem_sp, unchar ch,
    cred_t *credp, u_int from_stand)
{
	temstat_t	*ap = ltem_sp->tem_state;

	ap->a_state = A_STATE_START;
	switch (ch) {
	case A_BEL:
		tem_bell();
		break;

	case A_BS:
		tem_send_data(ltem_sp, credp, from_stand);
		ap->a_c_cursor[TEM_COL]--;
		if (ap->a_c_cursor[TEM_COL] < 0)
			ap->a_c_cursor[TEM_COL] = 0;
		tem_align_cursor(ltem_sp->tem_state);
		break;

	case A_HT:
		tem_send_data(ltem_sp, credp, from_stand);
		tem_tab(ltem_sp, credp, from_stand);
		break;

	case A_NL:
		/*
		tem_send_data(ltem_sp, credp, from_stand);
		tem_new_line(ltem_sp, credp, from_stand);
		break;
		*/

	case A_VT:
		tem_send_data(ltem_sp, credp, from_stand);
		tem_lf(ltem_sp, credp, from_stand);
		break;

	case A_FF:
		tem_send_data(ltem_sp, credp, from_stand);
		tem_cls(ltem_sp, credp, from_stand);
		break;

	case A_CR:
		tem_send_data(ltem_sp, credp, from_stand);
		tem_cr(ltem_sp->tem_state);
		break;

	case A_ESC:
		ap->a_state = A_STATE_ESC;
		break;

	case A_CSI:
		{
			register i;
			ap->a_curparam = 0;
			ap->a_paramval = 0;
			ap->a_gotparam = 0;
			/* clear the parameters */
			for (i = 0; i < TEM_MAXPARAMS; i++)
				ap->a_params[i] = -1;
			ap->a_state = A_STATE_CSI;
		}
		break;

	case A_GS:
		tem_send_data(ltem_sp, credp, from_stand);
		tem_back_tab(ltem_sp->tem_state);
		break;

	default:
		break;
	}
}


/*
 * if parameters [0..count - 1] are not set, set them to the value of newparam.
 */

static void
tem_setparam(temstat_t *ap, int count, ushort newparam)
{
	register int i;

	for (i = 0; i < count; i++) {
		if (ap->a_params[i] == -1)
			ap->a_params[i] = newparam;
	}
}


/*
 * select graphics mode based on the param vals stored in a_params
 */
static void
tem_selgraph(temstat_t *ap)
{
	register short curparam;
	register int count = 0;
	short param;

	curparam = ap->a_curparam;
	do {
		param = ap->a_params[count];

		switch (param) {
		case -1:
		case 0:
			if (ap->a_flags & TEM_ATTR_SCREEN_REVERSE) {
				ap->a_flags |= TEM_ATTR_REVERSE;
			} else {
				ap->a_flags &= ~TEM_ATTR_REVERSE;
			}
			ap->a_flags &= ~TEM_ATTR_BOLD;
			ap->a_flags &= ~TEM_ATTR_BLINK;
			break;

		case 30: /* black	(grey) 		foreground */
		case 31: /* red		(light red) 	foreground */
		case 32: /* green	(light green) 	foreground */
		case 33: /* brown	(yellow) 	foreground */
		case 34: /* blue	(light blue) 	foreground */
		case 35: /* magenta	(light magenta) foreground */
		case 36: /* cyan	(light cyan) 	foreground */
		case 37: /* white	(bright white) 	foreground */
			break;

		case 40: /* black	(grey) 		background */
		case 44: /* blue	(light blue) 	background */
			ap->a_flags &= ~TEM_ATTR_REVERSE;
			break;

		case 41: /* red		(light red) 	background */
		case 42: /* green	(light green) 	background */
		case 43: /* brown	(yellow) 	background */
		case 45: /* magenta	(light magenta) background */
		case 46: /* cyan	(light cyan) 	background */
		case 47: /* white	(bright white) 	background */
			ap->a_flags |= TEM_ATTR_REVERSE;
			break;

		case 7: /* Reverse video */
			if (ap->a_flags & TEM_ATTR_SCREEN_REVERSE) {
				ap->a_flags &= ~TEM_ATTR_REVERSE;
			} else {
				ap->a_flags |= TEM_ATTR_REVERSE;
			}
			break;

		case 1: /* Bold Intense */
			ap->a_flags |= TEM_ATTR_BOLD;
			break;

		case 5: /* Blink */
			ap->a_flags |= TEM_ATTR_BLINK;
			break;
		default:
			break;
		}
		count++;
		curparam--;

	} while (curparam > 0);


	ap->a_state = A_STATE_START;
}

/*
 * perform the appropriate action for the escape sequence
 */
static void
tem_chkparam(struct ltem_state *ltem_sp, unchar ch,
    cred_t *credp, u_int from_stand)
{
	int i;
	temstat_t *ap = ltem_sp->tem_state;

	register int	row;
	register int	col;

	row = ap->a_c_cursor[TEM_ROW];
	col = ap->a_c_cursor[TEM_COL];

	switch (ch) {

	case 'm': /* select terminal graphics mode */
		tem_send_data(ltem_sp, credp, from_stand);
		tem_selgraph(ltem_sp->tem_state);
		break;

	case '@':		/* insert char */
		tem_setparam(ltem_sp->tem_state, 1, 1);
		tem_shift(ltem_sp, ap->a_params[0], TEM_SHIFT_RIGHT,
		    credp, from_stand);
		break;

	case 'A':		/* cursor up */
		tem_setparam(ltem_sp->tem_state, 1, 1);
		row -= ap->a_params[0];
		if (row < 0)
			row += ap->a_c_dimension[TEM_ROW];
		tem_mv_cursor(ltem_sp, row, col, credp, from_stand);
		break;

	case 'd':		/* VPA - vertical position absolute */
		tem_setparam(ltem_sp->tem_state, 1, 1);
		row = (ap->a_params[0] - 1) % ap->a_c_dimension[TEM_ROW];
		tem_mv_cursor(ltem_sp, row, col, credp, from_stand);
		break;

	case 'e':		/* VPR - vertical position relative */
	case 'B':		/* cursor down */
		tem_setparam(ltem_sp->tem_state, 1, 1);
		row += ap->a_params[0];
		if (row >= ap->a_c_dimension[TEM_ROW])
			row -= ap->a_c_dimension[TEM_ROW];
		tem_mv_cursor(ltem_sp, row, col, credp, from_stand);
		break;

	case 'a':		/* HPR - horizontal position relative */
	case 'C':		/* cursor right */
		tem_setparam(ltem_sp->tem_state, 1, 1);
		col += ap->a_params[0];
		if (col >= ap->a_c_dimension[TEM_COL])
			col -= ap->a_c_dimension[TEM_COL];
		tem_mv_cursor(ltem_sp, row, col, credp, from_stand);
		break;

	case '`':		/* HPA - horizontal position absolute */
		tem_setparam(ltem_sp->tem_state, 1, 1);
		col = (ap->a_params[0] - 1) % ap->a_c_dimension[TEM_COL];
		tem_mv_cursor(ltem_sp, row, col, credp, from_stand);
		break;

	case 'D':		/* cursor left */
		tem_setparam(ltem_sp->tem_state, 1, 1);
		col -= ap->a_params[0];
		if (col < 0)
			col += ap->a_c_dimension[TEM_COL];
		tem_mv_cursor(ltem_sp, row, col, credp, from_stand);
		break;

	case 'E':		/* cursor next line */
		tem_setparam(ltem_sp->tem_state, 1, 1);
		col = 0;
		row += ap->a_params[0];
		if (row >= ap->a_c_dimension[TEM_ROW])
			row -= ap->a_c_dimension[TEM_ROW];
		tem_mv_cursor(ltem_sp, row, col, credp, from_stand);
		break;

	case 'F':		/* cursor previous line */
		tem_setparam(ltem_sp->tem_state, 1, 1);
		col = 0;
		row -= ap->a_params[0];
		if (row < 0)
			row += ap->a_c_dimension[TEM_ROW];
		tem_mv_cursor(ltem_sp, row, col, credp, from_stand);
		break;

	case 'G':		/* cursor horizontal position */
		tem_setparam(ltem_sp->tem_state, 1, 1);
		col = (ap->a_params[0] - 1) % ap->a_c_dimension[TEM_COL];
		tem_mv_cursor(ltem_sp, row, col, credp, from_stand);
		break;

	case 'g':		/* clear tabs */
		tem_setparam(ltem_sp->tem_state, 1, 0);
		tem_clear_tabs(ltem_sp->tem_state, ap->a_params[0]);
		break;

	case 'f':		/* HVP */
	case 'H':		/* position cursor */
		{
			int	row;
			int	col;

			tem_setparam(ltem_sp->tem_state, 2, 1);
			col = (ap->a_params[1] - 1) %
					ap->a_c_dimension[TEM_COL];
			row = (ap->a_params[0] - 1) %
					ap->a_c_dimension[TEM_ROW];
			tem_mv_cursor(ltem_sp, row, col, credp,
			    from_stand);
			break;
		}

	case 'I':		/* NO_OP entry */
		break;

	case 'J':		/* erase screen */
		tem_send_data(ltem_sp, credp, from_stand);
		tem_setparam(ltem_sp->tem_state, 1, 0);
		if (ap->a_params[0] == 0) {
			/* erase cursor to end of screen */
			/* FIRST erase cursor to end of line */
			(*ap->in_fp.f_cls)(ltem_sp,
				ap->a_c_dimension[TEM_COL] -
						ap->a_c_cursor[TEM_COL],
				ap->a_c_cursor[TEM_ROW],
				ap->a_c_cursor[TEM_COL], credp, from_stand);

			/* THEN erase lines below the cursor */
			for (row = ap->a_c_cursor[TEM_ROW] + 1;
				row < ap->a_c_dimension[TEM_ROW];
				row++) {
				(*ap->in_fp.f_cls)(ltem_sp,
					ap->a_c_dimension[TEM_COL],
					row, 0, credp, from_stand);
			}
		} else if (ap->a_params[0] == 1) {
			/* erase beginning of screen to cursor */
			/* FIRST erase lines above the cursor */
			for (row = 0;
				row < ap->a_c_cursor[TEM_ROW];
				row++) {
				(*ap->in_fp.f_cls)(ltem_sp,
					ap->a_c_dimension[TEM_COL],
					row, 0, credp, from_stand);
			}
			/* THEN erase beginning of line to cursor */
			(*ap->in_fp.f_cls)(ltem_sp,
				ap->a_c_cursor[TEM_COL] + 1,
				ap->a_c_cursor[TEM_ROW], 0, credp, from_stand);
		} else {
			/* erase whole screen */
			for (row = 0;
				row < ap->a_c_dimension[TEM_ROW];
				row++) {
				(*ap->in_fp.f_cls)(ltem_sp,
					ap->a_c_dimension[TEM_COL],
					row, 0, credp, from_stand);
			}

		}
		break;

	case 'K':		/* erase line */
		tem_send_data(ltem_sp, credp, from_stand);
		tem_setparam(ltem_sp->tem_state, 1, 0);
		if (ap->a_params[0] == 0) {
			/* erase cursor to end of line */
			(*ap->in_fp.f_cls)(ltem_sp,
				(ap->a_c_dimension[TEM_COL] -
				    ap->a_c_cursor[TEM_COL]),
				ap->a_c_cursor[TEM_ROW],
				ap->a_c_cursor[TEM_COL], credp, from_stand);
		} else if (ap->a_params[0] == 1) {
			/* erase beginning of line to cursor */
			(*ap->in_fp.f_cls)(ltem_sp,
				ap->a_c_cursor[TEM_COL] + 1,
				ap->a_c_cursor[TEM_ROW], 0, credp, from_stand);
		} else {
			/* erase whole line */
			(*ap->in_fp.f_cls)(ltem_sp,
				ap->a_c_dimension[TEM_COL],
				ap->a_c_cursor[TEM_ROW], 0, credp, from_stand);
		}
		break;

	case 'L':		/* insert line */
		tem_send_data(ltem_sp, credp, from_stand);
		tem_setparam(ltem_sp->tem_state, 1, 1);
		tem_scroll(ltem_sp,
			ap->a_c_cursor[TEM_ROW],
			ap->a_c_dimension[TEM_ROW] - 1,
			ap->a_params[0], TEM_SCROLL_DOWN, credp, from_stand);
		break;

	case 'M':		/* delete line */
		tem_send_data(ltem_sp, credp, from_stand);
		tem_setparam(ltem_sp->tem_state, 1, 1);
		tem_scroll(ltem_sp,
			ap->a_c_cursor[TEM_ROW],
			ap->a_c_dimension[TEM_ROW] - 1,
			ap->a_params[0], TEM_SCROLL_UP, credp, from_stand);
		break;

	case 'P':		/* delete char */
		tem_setparam(ltem_sp->tem_state, 1, 1);
		tem_shift(ltem_sp, ap->a_params[0], TEM_SHIFT_LEFT,
		    credp, from_stand);
		break;

	case 'S':		/* scroll up */
		tem_send_data(ltem_sp, credp, from_stand);
		tem_setparam(ltem_sp->tem_state, 1, 1);
		tem_scroll(ltem_sp, 0,
			ap->a_c_dimension[TEM_ROW] - 1,
			ap->a_params[0], TEM_SCROLL_UP, credp, from_stand);
		break;

	case 'T':		/* scroll down */
		tem_send_data(ltem_sp, credp, from_stand);
		tem_setparam(ltem_sp->tem_state, 1, 1);
		tem_scroll(ltem_sp, 0,
			ap->a_c_dimension[TEM_ROW] - 1,
			ap->a_params[0], TEM_SCROLL_DOWN, credp, from_stand);
		break;

	case 'X':		/* erase char */
		tem_setparam(ltem_sp->tem_state, 1, 1);
		(*ap->in_fp.f_cls)(ltem_sp,
			ap->a_params[0],
			ap->a_c_cursor[TEM_ROW],
			ap->a_c_cursor[TEM_COL], credp, from_stand);
		break;

	case 'Z':		/* cursor backward tabulation */
		tem_send_data(ltem_sp, credp, from_stand);
		tem_setparam(ltem_sp->tem_state, 1, 1);
		for (i = 0; i < ap->a_params[0]; i++)
			tem_back_tab(ltem_sp->tem_state);
		break;
	}
	ap->a_state = A_STATE_START;
}


/*
 * Gather the parameters of an ANSI escape sequence
 */
static void
tem_getparams(struct ltem_state *ltem_sp, unchar ch,
    cred_t *credp, u_int from_stand)
{

	temstat_t *ap = ltem_sp->tem_state;

	if ((ch >= '0' && ch <= '9') && (ap->a_state != A_STATE_ESC_Q_DELM)) {
		ap->a_paramval = ((ap->a_paramval * 10) + (ch - '0'));
		ap->a_gotparam++;	/* Remember got parameter */
		return;			/* Return immediately */
	}
	switch (ap->a_state) {		/* Handle letter based on state */

	case A_STATE_ESC_Q:			  /* <ESC>Q<num> ? */
		ap->a_params[1] = ch;		  /* Save string delimiter */
		ap->a_params[2] = 0;		  /* String length 0 to start */
		ap->a_state = A_STATE_ESC_Q_DELM; /* Read string next */
		break;

	case A_STATE_ESC_Q_DELM:		  /* <ESC>Q<num><delm> ? */
		if (ch == ap->a_params[1]) {	/* End of string? */
			ap->a_state = A_STATE_START;
			/* End of <ESC> sequence */
		} else if (ch == '^')
			/* Control char escaped with '^'? */
			ap->a_state = A_STATE_ESC_Q_DELM_CTRL;
			/* Read control character next */

		else if (ch != '\0') {
			/* Not a null? Add to string */
			ap->a_fkey[ap->a_params[2]++] = ch;
			if (ap->a_params[2] >= TEM_MAXFKEY)	/* Full? */
				ap->a_state = A_STATE_START;
				/* End of <ESC> sequence */
		}
		break;

	case A_STATE_ESC_Q_DELM_CTRL:	/* Contrl character escaped with '^' */
		ap->a_state = A_STATE_ESC_Q_DELM; /* Read more string later */
		ch -= ' ';		/* Convert to control character */
		if (ch != '\0') {	/* Not a null? Add to string */
			ap->a_fkey[ap->a_params[2]++] = ch;
			if (ap->a_params[2] >= TEM_MAXFKEY)	/* Full? */
				ap->a_state = A_STATE_START;
				/* End of <ESC> sequence */
		}
		break;

	default:			/* All other states */
		if (ap->a_gotparam) {
			/*
			 * Previous number parameter? Save and
			 * point to next free parameter.
			 */
			ap->a_params[ap->a_curparam] = ap->a_paramval;
			ap->a_curparam++;
		}

		if (ch == ';') {
			/* Multiple param separator? */
			ap->a_gotparam = 0;	/* Restart parameter search */
			ap->a_paramval = 0;	/* No parameter value yet */
		} else if (ap->a_state == A_STATE_CSI_EQUAL ||
			ap->a_state == A_STATE_CSI_QMARK) {
			ap->a_state = A_STATE_START;
		} else	/* Regular letter */
			/* Handle escape sequence */
			tem_chkparam(ltem_sp, ch, credp, from_stand);
		break;
	}
}

/*
 * Add character to internal buffer.
 * When its full, send it to the next layer.
 */

static void
tem_outch(struct ltem_state *ltem_sp, unchar ch,
    cred_t *credp, u_int from_stand)
{
	temstat_t	*ap = ltem_sp->tem_state;
	/* buffer up the character until later */

	ap->a_outbuf[ap->a_outindex++] = ch;
	ap->a_c_cursor[TEM_COL]++;
	if (ap->a_c_cursor[TEM_COL] >= ap->a_c_dimension[TEM_COL]) {
		tem_send_data(ltem_sp, credp, from_stand);
		tem_new_line(ltem_sp, credp, from_stand);
	}
}

static void
tem_new_line(struct ltem_state *ltem_sp,
    cred_t *credp, u_int from_stand)
{
	tem_cr(ltem_sp->tem_state);
	tem_lf(ltem_sp, credp, from_stand);
}

static void
tem_cr(temstat_t *ap)
{
	ap->a_c_cursor[TEM_COL] = 0;
	tem_align_cursor(ap);
}

static void
tem_lf(struct ltem_state *ltem_sp,
    cred_t *credp, u_int from_stand)
{
	temstat_t *ap = ltem_sp->tem_state;

	ap->a_c_cursor[TEM_ROW]++;
	/*
	 * implement Esc[#r when # is zero.  This means no scroll but just
	 * return cursor to top of screen, do not clear screen
	 */
	if (ap->a_c_cursor[TEM_ROW] >= ap->a_c_dimension[TEM_ROW]) {
		if (ap->a_nscroll != 0) {
			tem_scroll(ltem_sp, 0,
			    ap->a_c_dimension[TEM_ROW] - 1,
			    ap->a_nscroll, TEM_SCROLL_UP, credp, from_stand);
			ap->a_c_cursor[TEM_ROW] =
			    ap->a_c_dimension[TEM_ROW] - ap->a_nscroll;
		} else {	/* no scroll */
			tem_mv_cursor(ltem_sp, 0, 0, credp, from_stand);
		}
	}
	if (ap->a_nscroll == 0) {
		/* erase cursor line */
		(*ap->in_fp.f_cls)(ltem_sp,
			ap->a_c_dimension[TEM_COL] - 1,
			ap->a_c_cursor[TEM_ROW],
			ap->a_c_cursor[TEM_COL] + 1, credp, from_stand);

	}
	tem_align_cursor(ap);
}

static void
tem_send_data(struct ltem_state *ltem_sp, cred_t *credp,
    u_int from_stand)
{

	temstat_t *ap = ltem_sp->tem_state;

	if (ap->a_outindex != 0) {
		/*
		 * Call the primitive to render this data.
		 */
		(*ap->in_fp.f_display)(ltem_sp,
			ap->a_outbuf,
			ap->a_outindex,
			ap->a_s_cursor[TEM_ROW],
			ap->a_s_cursor[TEM_COL], credp, from_stand);
		ap->a_outindex = 0;
	}
	tem_align_cursor(ap);
}


static void
tem_align_cursor(temstat_t *ap)
{
	ap->a_s_cursor[TEM_ROW] = ap->a_c_cursor[TEM_ROW];
	ap->a_s_cursor[TEM_COL] = ap->a_c_cursor[TEM_COL];
}



/*
 * State machine parser based on the current state and character input
 * major trtemtions are to control character or normal character
 */

static void
tem_parse(struct ltem_state *ltem_sp, unchar ch,
    cred_t *credp, u_int from_stand)
{
	int	i;
	temstat_t *ap = ltem_sp->tem_state;

	if (ap->a_state == A_STATE_START) {	/* Normal state? */
		if (ch == A_CSI || ch == A_ESC || ch < ' ') /* Control? */
			tem_control(ltem_sp, ch, credp, from_stand);
		else
			/* Display */
			tem_outch(ltem_sp, ch, credp, from_stand);
	} else {	/* In <ESC> sequence */
		/* Need to get parameters? */
		if (ap->a_state != A_STATE_ESC) {
			if (ap->a_state == A_STATE_CSI)
				switch (ch) {
				case '?':
					ap->a_state = A_STATE_CSI_QMARK;
					return;
				case '=':
					ap->a_state = A_STATE_CSI_EQUAL;
					return;
				case 's':
					/*
					 * As defined below, this sequence
					 * saves the cursor.  However, Sun
					 * defines ESC[s as reset.  We resolved
					 * the conflict by selecting reset as it
					 * is exported in the termcap file for
					 * sun-mon, while the "save cursor"
					 * definition does not exist anywere in
					 * /etc/termcap.
					 * However, having no coherent
					 * definition of reset, we have not
					 * implemented it.
					 */

					/*
					 * Original code
					 * ap->a_r_cursor[TEM_ROW] =
					 *	ap->a_c_cursor[TEM_ROW];
					 * ap->a_r_cursor[TEM_COL] =
					 *	ap->a_c_cursor[TEM_COL];
					 * ap->a_state = A_STATE_START;
					 */

					return;
				case 'u':
					tem_mv_cursor(ltem_sp,
						    ap->a_r_cursor[TEM_ROW],
						    ap->a_r_cursor[TEM_COL],
						    credp, from_stand);
						    ap->a_state = A_STATE_START;
					return;
				case 'p': 	/* sunbow */
					/*
					 * Don't set anything if we are
					 * already as we want to be.
					 */
					if (ap->a_flags &
					    TEM_ATTR_SCREEN_REVERSE) {
						ap->a_flags &=
						    ~TEM_ATTR_SCREEN_REVERSE;
						/*
						 * If we have switched the
						 * characters to be the
						 * inverse from the screen,
						 * then switch them as well
						 * to keep them the inverse
						 * of the screen.
						 */
						if (ap->a_flags &
						    TEM_ATTR_REVERSE) {
							ap->a_flags &=
							    ~TEM_ATTR_REVERSE;
						} else {
							ap->a_flags |=
							    TEM_ATTR_REVERSE;
						}
					}
					if (ltem_sp->display_mode ==
					    VIS_PIXEL) {
						tem_clear_entire(ltem_sp,
						    credp, from_stand);
					} else {
						tem_cls(ltem_sp,
						    credp, from_stand);
					}
					return;
				case 'q':  	/* sunwob */
					/*
					 * Don't set anything if we are
					 * already where as we want to be.
					 */
					if (!(ap->a_flags &
					    TEM_ATTR_SCREEN_REVERSE)) {
						ap->a_flags |=
						    TEM_ATTR_SCREEN_REVERSE;
						/*
						 * If we have switched the
						 * characters to be the
						 * inverse from the screen,
						 * then switch them as well
						 * to keep them the inverse
						 * of the screen.
						 */
						if (!(ap->a_flags &
						    TEM_ATTR_REVERSE)) {
							ap->a_flags |=
							    TEM_ATTR_REVERSE;
						} else {
							ap->a_flags &=
							    ~TEM_ATTR_REVERSE;
						}
					}

					if (ltem_sp->display_mode ==
					    VIS_PIXEL) {
						tem_clear_entire(ltem_sp,
						    credp, from_stand);
					} else {
						tem_cls(ltem_sp,
						    credp, from_stand);
					}
					return;
				case 'r':	/* sunscrl */
					ap->a_nscroll = ap->a_paramval;
					if (ap->a_nscroll >
					    (ushort)ap->a_c_dimension[TEM_ROW])
						ap->a_nscroll = 1;
					return;
				}
			tem_getparams(ltem_sp, ch, credp, from_stand);
		} else {	/* Previous char was <ESC> */
			if (ch == '[') {
				ap->a_curparam = 0;
				ap->a_paramval = 0;
				ap->a_gotparam = 0;
				/* clear the parameters */
				for (i = 0; i < TEM_MAXPARAMS; i++)
					ap->a_params[i] = -1;
				ap->a_state = A_STATE_CSI;
			} else if (ch == 'Q') {	/* <ESC>Q ? */
				ap->a_curparam = 0;
				ap->a_paramval = 0;
				ap->a_gotparam = 0;
				for (i = 0; i < TEM_MAXPARAMS; i++)
					ap->a_params[i] = -1;	/* Clear */
				/* Next get params */
				ap->a_state = A_STATE_ESC_Q;
			} else if (ch == 'C') {	/* <ESC>C ? */
				ap->a_curparam = 0;
				ap->a_paramval = 0;
				ap->a_gotparam = 0;
				for (i = 0; i < TEM_MAXPARAMS; i++)
					ap->a_params[i] = -1;	/* Clear */
				/* Next get params */
				ap->a_state = A_STATE_ESC_C;
			} else {
				ap->a_state = A_STATE_START;
				if (ch == 'c')
					/* ESC c resets display */
					tem_reset_display(ltem_sp, credp,
					    from_stand);
				else if (ch == 'H')
					/* ESC H sets a tab */
					tem_set_tab(ap);
				else if (ch == '7') {
					/* ESC 7 Save Cursor position */
					ap->a_r_cursor[TEM_ROW] =
						ap->a_c_cursor[TEM_ROW];
					ap->a_r_cursor[TEM_COL] =
						ap->a_c_cursor[TEM_COL];
				} else if (ch == '8')
					/* ESC 8 Restore Cursor position */
					tem_mv_cursor(ltem_sp,
					    ap->a_r_cursor[TEM_ROW],
					    ap->a_r_cursor[TEM_COL], credp,
						from_stand);
				/* check for control chars */
				else if (ch < ' ')
					tem_control(ltem_sp, ch, credp,
					    from_stand);
				else
					tem_outch(ltem_sp, ch, credp,
					    from_stand);
			}
		}
	}
}

static void
tem_bell(void)
{
	ddi_ring_console_bell(10);
}


static void
tem_scroll(struct ltem_state *ltem_sp,
    int start, int end, int count, int direction,
	cred_t *credp, u_int from_stand)
{
	register int	row;
	temstat_t *ap = ltem_sp->tem_state;


	switch (direction) {
	case TEM_SCROLL_UP:
		(*ap->in_fp.f_copy)(ltem_sp, 0, start + count,
				ap->a_c_dimension[TEM_COL] - 1, end,
				0, start,
				VIS_COPY_FORWARD, credp, from_stand);
		for (row = (end - count) + 1; row <= end; row++) {
			(*ap->in_fp.f_cls)(ltem_sp,
				ap->a_c_dimension[TEM_COL],
				row, 0, credp, from_stand);
		}
		break;

	case TEM_SCROLL_DOWN:
		(*ap->in_fp.f_copy)(ltem_sp, 0, start,
				ap->a_c_dimension[TEM_COL] - 1, end - count,
				0, start + count,
				VIS_COPY_BACKWARD, credp, from_stand);
		for (row = start; row < start + count; row++)
			(*ap->in_fp.f_cls)(ltem_sp,
				ap->a_c_dimension[TEM_COL],
				row, 0, credp, from_stand);
		break;
	}
}

static void
tem_text_display(struct ltem_state *ltem_sp, unchar * string,
	int count, screen_pos_t row, screen_pos_t col, cred_t *credp,
	u_int from_stand)
{
	temstat_t *ap = ltem_sp->tem_state;
	struct vis_consdisplay da;
	int rval;

	da.version = VIS_DISPLAY_VERSION;
	da.data = string;
	da.width = count;
	da.row = row;
	da.col = col;

	if (ap->a_flags & TEM_ATTR_REVERSE) {
		da.fg_color = TEM_TEXT_WHITE;
		da.bg_color = TEM_TEXT_BLACK;
	} else {
		da.fg_color = TEM_TEXT_BLACK;
		da.bg_color = TEM_TEXT_WHITE;
	}

	if (from_stand)
		(void) ddi_lyr_ioctl(ltem_sp->hdl, VIS_STAND_CONSDISPLAY,
		    (int)&da, FKIOCTL, credp, &rval);
	else
		(void) ddi_lyr_ioctl(ltem_sp->hdl, VIS_CONSDISPLAY,
		    (int)&da, FKIOCTL, credp, &rval);
}

static void
tem_text_copy(struct ltem_state *ltem_sp,
	screen_pos_t s_col, screen_pos_t s_row,
	screen_pos_t e_col, screen_pos_t e_row,
	screen_pos_t t_col, screen_pos_t t_row,
	int direction, cred_t *credp, u_int from_stand)
{
	struct vis_conscopy	ma;
	int rval;

	ma.version = VIS_COPY_VERSION;
	ma.s_row = s_row;
	ma.s_col = s_col;
	ma.e_row = e_row;
	ma.e_col = e_col;
	ma.t_row = t_row;
	ma.t_col = t_col;
	ma.direction = direction;

	if (from_stand)
		(void) ddi_lyr_ioctl(ltem_sp->hdl, VIS_STAND_CONSCOPY,
		    (int)&ma, FKIOCTL, credp, &rval);
	else
		(void) ddi_lyr_ioctl(ltem_sp->hdl, VIS_CONSCOPY,
		    (int)&ma, FKIOCTL, credp, &rval);
}

static void
tem_text_cls(struct ltem_state *ltem_sp,
	int count, screen_pos_t row, screen_pos_t col, cred_t *credp,
	u_int from_stand)
{
	temstat_t *ap = ltem_sp->tem_state;
	struct vis_consdisplay da;
	int rval;

	da.version = VIS_DISPLAY_VERSION;
	da.data = ap->a_blank_line;
	da.width = count;
	da.row = row;
	da.col = col;

	if (ap->a_flags & TEM_ATTR_SCREEN_REVERSE) {
		da.fg_color = TEM_TEXT_WHITE;
		da.bg_color = TEM_TEXT_BLACK;
	} else {
		da.fg_color = TEM_TEXT_BLACK;
		da.bg_color = TEM_TEXT_WHITE;
	}

	if (from_stand)
		(void) ddi_lyr_ioctl(ltem_sp->hdl, VIS_STAND_CONSDISPLAY,
		    (int)&da, FKIOCTL, credp, &rval);
	else
		(void) ddi_lyr_ioctl(ltem_sp->hdl, VIS_CONSDISPLAY,
		    (int)&da, FKIOCTL, credp, &rval);
}

void
tem_pix_display(struct ltem_state *ltem_sp,
	unchar *string, int count,
	screen_pos_t row, screen_pos_t col, cred_t *credp,
	u_int from_stand)
{
	temstat_t *ap = ltem_sp->tem_state;
	struct vis_consdisplay		da;
	register int	i;
	int rval;

	da.version = VIS_DISPLAY_VERSION;
	da.data = ap->a_pix_data;
	da.width = ap->a_font.width;
	da.height = ap->a_font.height;
	da.row = (row * da.height) + ap->a_p_offset[TEM_ROW];
	da.col = (col * da.width) + ap->a_p_offset[TEM_COL];

	for (i = 0; i < count; i++) {
		bit_to_pix(ap, string[i]);
		if (from_stand)
			(void) ddi_lyr_ioctl(ltem_sp->hdl,
			    VIS_STAND_CONSDISPLAY,
			    (int)&da, FKIOCTL, credp, &rval);
		else
			(void) ddi_lyr_ioctl(ltem_sp->hdl, VIS_CONSDISPLAY,
			    (int)&da, FKIOCTL, credp, &rval);
		da.col += da.width;
	}
}

static void
tem_pix_copy(struct ltem_state *ltem_sp,
	screen_pos_t s_col, screen_pos_t s_row,
	screen_pos_t e_col, screen_pos_t e_row,
	screen_pos_t t_col, screen_pos_t t_row,
	int direction, cred_t *credp,
	u_int from_stand)
{
	temstat_t *ap = ltem_sp->tem_state;
	struct vis_conscopy ma;
	int rval;

	ma.version = VIS_COPY_VERSION;
	ma.s_row = s_row * ap->a_font.height + ap->a_p_offset[TEM_ROW];
	ma.s_col = s_col * ap->a_font.width + ap->a_p_offset[TEM_COL];
	ma.e_row = (e_row + 1) * ap->a_font.height +
			ap->a_p_offset[TEM_ROW] - 1;
	ma.e_col = (e_col + 1) * ap->a_font.width +
			ap->a_p_offset[TEM_COL] - 1;
	ma.t_row = t_row * ap->a_font.height + ap->a_p_offset[TEM_ROW];
	ma.t_col = t_col * ap->a_font.width + ap->a_p_offset[TEM_COL];
	ma.direction = direction;

	if (from_stand)
		(void) ddi_lyr_ioctl(ltem_sp->hdl, VIS_STAND_CONSCOPY,
		    (int)&ma, FKIOCTL, credp, &rval);
	else
		(void) ddi_lyr_ioctl(ltem_sp->hdl, VIS_CONSCOPY,
		    (int)&ma, FKIOCTL, credp, &rval);
}

void
tem_pix_cls(struct ltem_state *ltem_sp, int count,
	screen_pos_t row, screen_pos_t col, cred_t *credp,
	u_int from_stand)
{
	temstat_t *ap = ltem_sp->tem_state;
	struct vis_consdisplay		da;
	register int	i;
	int rval;

	da.version = VIS_DISPLAY_VERSION;
	da.width = ap->a_font.width;
	da.height = ap->a_font.height;
	da.row = (row * da.height) + ap->a_p_offset[TEM_ROW];
	da.col = (col * da.width) + ap->a_p_offset[TEM_COL];

	if (ap->a_flags & TEM_ATTR_SCREEN_REVERSE)
		da.data = ap->a_rcls_pix_data;
	else
		da.data = ap->a_cls_pix_data;

	for (i = 0; i < count; i++) {
		if (from_stand)
			(void) ddi_lyr_ioctl(ltem_sp->hdl,
			    VIS_STAND_CONSDISPLAY,
			    (int)&da, FKIOCTL, credp, &rval);
		else
			(void) ddi_lyr_ioctl(ltem_sp->hdl, VIS_CONSDISPLAY,
			    (int)&da, FKIOCTL, credp, &rval);
		da.col += da.width;
	}
}

static void
tem_back_tab(temstat_t *ap)
{
	register int	i;
	register int	tabstop = 0;

	for (i = ap->a_ntabs - 1; (i >= 0) && (tabstop == 0); i--) {
		if (ap->a_tabs[i] < ap->a_c_cursor[TEM_COL])
			tabstop = ap->a_tabs[i];
	}
	ap->a_c_cursor[TEM_COL] = tabstop;
	tem_align_cursor(ap);
}


static void
tem_tab(struct ltem_state *ltem_sp,
    cred_t *credp, u_int from_stand)
{
	register int	i;
	register int	tabstop = 0;
	temstat_t *ap = ltem_sp->tem_state;

	for (i = 0; (i < ap->a_ntabs) && (tabstop == 0); i++) {
		if (ap->a_tabs[i] > ap->a_c_cursor[TEM_COL])
			tabstop = ap->a_tabs[i];
	}
	if (tabstop == 0)
		tabstop = ap->a_c_dimension[TEM_COL];

	if (tabstop >= ap->a_c_dimension[TEM_COL]) {
		ap->a_c_cursor[TEM_COL] = tabstop - ap->a_c_dimension[TEM_COL];
		tem_lf(ltem_sp, credp, from_stand);
	} else {
		ap->a_c_cursor[TEM_COL] = tabstop;
		tem_align_cursor(ap);
	}
}

static void
tem_set_tab(temstat_t *ap)
{
	register int	i;
	register int	j;

	if (ap->a_ntabs == TEM_MAXTAB)
		return;
	if (ap->a_ntabs == 0 ||
		ap->a_tabs[ap->a_ntabs] < ap->a_c_cursor[TEM_COL]) {
		ap->a_tabs[ap->a_ntabs++] = ap->a_c_cursor[TEM_COL];
		return;
	}
	for (i = 0; i < ap->a_ntabs; i++) {
		if (ap->a_tabs[i] == ap->a_c_cursor[TEM_COL])
			return;
		if (ap->a_tabs[i] > ap->a_c_cursor[TEM_COL]) {
			for (j = ap->a_ntabs - 1; j >= i; j--)
				ap->a_tabs[j+ 1] = ap->a_tabs[j];
			ap->a_tabs[i] = ap->a_c_cursor[TEM_COL];
			ap->a_ntabs++;
			return;
		}
	}
}


static void
tem_clear_tabs(temstat_t *ap, int action)
{
	register int	i;
	register int	j;

	switch (action) {
	case 3: /* clear all tabs */
		ap->a_ntabs = 0;
		break;
	case 0: /* clr tab at cursor */

		for (i = 0; i < ap->a_ntabs; i++) {
			if (ap->a_tabs[i] == ap->a_c_cursor[TEM_COL]) {
				ap->a_ntabs--;
				for (j = i; j < ap->a_ntabs; j++)
					ap->a_tabs[j] = ap->a_tabs[j + 1];
				return;
			}
		}
		break;
	}
}

static void
tem_clear_entire(struct ltem_state *ltem_sp, cred_t *credp,
    u_int from_stand)
{
	int	row;
	int	nrows;
	int	col;
	int	ncols;
	struct vis_consdisplay	da;
	temstat_t *ap = ltem_sp->tem_state;
	int rval;

	da.version = VIS_DISPLAY_VERSION;
	da.width = ap->a_font.width;
	da.height = ap->a_font.height;
	nrows = (ap->a_p_dimension[TEM_ROW] + (da.height - 1))/ da.height;
	ncols = (ap->a_p_dimension[TEM_COL] + (da.width - 1))/ da.width;

	if (ap->a_flags & TEM_ATTR_SCREEN_REVERSE)
		da.data = ap->a_rcls_pix_data;
	else
		da.data = ap->a_cls_pix_data;

	for (row = 0; row < nrows; row++) {
		da.row = row * da.height;
		da.col = 0;
		for (col = 0; col < ncols; col++) {
			if (from_stand)
				(void) ddi_lyr_ioctl(ltem_sp->hdl,
				    VIS_STAND_CONSDISPLAY,
				    (int)&da, FKIOCTL, credp, &rval);
			else
				(void) ddi_lyr_ioctl(ltem_sp->hdl,
				    VIS_CONSDISPLAY,
				    (int)&da, FKIOCTL, credp, &rval);
			da.col += da.width;
		}
	}

	ap->a_c_cursor[TEM_ROW] = 0;
	ap->a_c_cursor[TEM_COL] = 0;
	tem_align_cursor(ap);
}

static void
tem_cls(struct ltem_state *ltem_sp,
    cred_t *credp, u_int from_stand)
{
	register int	row;
	temstat_t *ap = ltem_sp->tem_state;

	for (row = 0; row < ap->a_c_dimension[TEM_ROW]; row++) {
		(*ap->in_fp.f_cls)(ltem_sp,
			ap->a_c_dimension[TEM_COL],
			row, 0, credp, from_stand);
	}
	ap->a_c_cursor[TEM_ROW] = 0;
	ap->a_c_cursor[TEM_COL] = 0;
	tem_align_cursor(ap);
}

static void
tem_mv_cursor(struct ltem_state *ltem_sp, int row, int col,
    cred_t *credp, u_int from_stand)
{
	temstat_t *ap = ltem_sp->tem_state;

	tem_send_data(ltem_sp, credp, from_stand);
	ap->a_c_cursor[TEM_ROW] = row;
	ap->a_c_cursor[TEM_COL] = col;
	tem_align_cursor(ap);
}


static void
tem_reset_display(struct ltem_state *ltem_sp,
    cred_t *credp, u_int from_stand)
{
	register int j;
	temstat_t *ap = ltem_sp->tem_state;

	ap->a_c_cursor[TEM_ROW] = 0;
	ap->a_c_cursor[TEM_COL] = 0;
	ap->a_r_cursor[TEM_ROW] = 0;
	ap->a_r_cursor[TEM_COL] = 0;
	ap->a_s_cursor[TEM_ROW] = 0;
	ap->a_s_cursor[TEM_COL] = 0;
	ap->a_outindex = 0;
	ap->a_state = A_STATE_START;
	ap->a_gotparam = 0;
	ap->a_curparam = 0;
	ap->a_paramval = 0;
	ap->a_flags = 0;
	ap->a_nscroll = 1;

	tem_reset_colormap(ltem_sp, credp, from_stand);

	/*
	* set up the initial tab stops
	*/
	ap->a_ntabs = 0;
	for (j = 8; j < ap->a_c_dimension[TEM_COL]; j += 8)
		ap->a_tabs[ap->a_ntabs++] = j;

	for (j = 0; j < TEM_MAXPARAMS; j++)
		ap->a_params[j] = 0;

	(*ap->in_fp.f_cursor)(ltem_sp, VIS_HIDE_CURSOR, credp,
	    from_stand);

	if (ltem_sp->display_mode == VIS_PIXEL) {
		ASSERT((ap)->in_fp.f_bit2pix != NULL); \
		(void) (*ap->in_fp.f_bit2pix)(&ap->a_font, TEM_ATTR_NORMAL,
		    ' ', ap->a_cls_pix_data);
		(void) (*ap->in_fp.f_bit2pix)(&ap->a_font, TEM_ATTR_REVERSE,
		    ' ', ap->a_rcls_pix_data);
		tem_clear_entire(ltem_sp, credp, from_stand);
	} else {
		for (j = 0; j < ap->a_c_dimension[TEM_COL]; j++)
			ap->a_blank_line[j] = ' ';
		tem_cls(ltem_sp, credp, from_stand);
	}

	ap->a_initialized = 1;
	(*ap->in_fp.f_cursor)(ltem_sp, VIS_DISPLAY_CURSOR, credp,
	    from_stand);
}


static void
tem_shift(struct ltem_state *ltem_sp, int count, int direction,
    cred_t *credp, u_int from_stand)
{
	temstat_t *ap = ltem_sp->tem_state;

	switch (direction) {
	case TEM_SHIFT_LEFT:
		(*ap->in_fp.f_copy)(ltem_sp,
			ap->a_c_cursor[TEM_COL] + count,
			ap->a_c_cursor[TEM_ROW],
			ap->a_c_dimension[TEM_COL] - 1,
			ap->a_c_cursor[TEM_ROW],
			ap->a_c_cursor[TEM_COL],
			ap->a_c_cursor[TEM_ROW],
			VIS_COPY_FORWARD, credp, from_stand);

		(*ap->in_fp.f_cls)(ltem_sp,
			count,
			ap->a_c_cursor[TEM_ROW],
			(ap->a_c_dimension[TEM_COL] - count), credp,
			    from_stand);
		break;
	case TEM_SHIFT_RIGHT:
		(*ap->in_fp.f_copy)(ltem_sp,
			ap->a_c_cursor[TEM_COL],
			ap->a_c_cursor[TEM_ROW],
			ap->a_c_dimension[TEM_COL] - count - 1,
			ap->a_c_cursor[TEM_ROW],
			ap->a_c_cursor[TEM_COL] + count,
			ap->a_c_cursor[TEM_ROW],
			VIS_COPY_BACKWARD, credp, from_stand);

		(*ap->in_fp.f_cls)(ltem_sp,
			count,
			ap->a_c_cursor[TEM_ROW],
			ap->a_c_cursor[TEM_COL], credp, from_stand);
		break;
	}
}

static void
tem_text_cursor(struct ltem_state *ltem_sp, short action,
    cred_t *credp, u_int from_stand)
{
	temstat_t *ap = ltem_sp->tem_state;
	struct vis_conscursor	ca;
	int rval;

	ca.version = VIS_CURSOR_VERSION;
	ca.row = ap->a_c_cursor[TEM_ROW];
	ca.col = ap->a_c_cursor[TEM_COL];
	ca.action = action;

	if (from_stand)
		(void) ddi_lyr_ioctl(ltem_sp->hdl, VIS_STAND_CONSCURSOR,
		    (int)&ca, FKIOCTL, credp, &rval);
	else
		(void) ddi_lyr_ioctl(ltem_sp->hdl, VIS_CONSCURSOR,
		    (int)&ca, FKIOCTL, credp, &rval);
}


static void
tem_pix_cursor(struct ltem_state *ltem_sp, short action,
    cred_t *credp, u_int from_stand)
{
	temstat_t *ap = ltem_sp->tem_state;
	struct vis_conscursor	ca;
	int rval;

	ca.version = VIS_CURSOR_VERSION;
	ca.row = ap->a_c_cursor[TEM_ROW] * ap->a_font.height +
				ap->a_p_offset[TEM_ROW];
	ca.col = ap->a_c_cursor[TEM_COL] * ap->a_font.width +
				ap->a_p_offset[TEM_COL];
	ca.width = ap->a_font.width;
	ca.height = ap->a_font.height;
	if (ap->a_pdepth == 8 || ap->a_pdepth == 4 || ap->a_pdepth == 1) {
		if (ap->a_flags & TEM_ATTR_REVERSE) {
			ca.fg_color.mono = TEM_TEXT_WHITE;
			ca.bg_color.mono = TEM_TEXT_BLACK;
		} else {
			ca.fg_color.mono = TEM_TEXT_BLACK;
			ca.bg_color.mono = TEM_TEXT_WHITE;
		}
	} else if (ap->a_pdepth == 24) {
		if (ap->a_flags & TEM_ATTR_REVERSE) {
			ca.fg_color.twentyfour[0] = TEM_TEXT_WHITE24_RED;
			ca.fg_color.twentyfour[1] = TEM_TEXT_WHITE24_GREEN;
			ca.fg_color.twentyfour[2] = TEM_TEXT_WHITE24_BLUE;

			ca.bg_color.twentyfour[0] = TEM_TEXT_BLACK24_RED;
			ca.bg_color.twentyfour[1] = TEM_TEXT_BLACK24_GREEN;
			ca.bg_color.twentyfour[2] = TEM_TEXT_BLACK24_BLUE;
		} else {
			ca.fg_color.twentyfour[0] = TEM_TEXT_BLACK24_RED;
			ca.fg_color.twentyfour[1] = TEM_TEXT_BLACK24_GREEN;
			ca.fg_color.twentyfour[2] = TEM_TEXT_BLACK24_BLUE;

			ca.bg_color.twentyfour[0] = TEM_TEXT_WHITE24_RED;
			ca.bg_color.twentyfour[1] = TEM_TEXT_WHITE24_GREEN;
			ca.bg_color.twentyfour[2] = TEM_TEXT_WHITE24_BLUE;
		}
	}

	ca.action = action;

	if (from_stand)
		(void) ddi_lyr_ioctl(ltem_sp->hdl, VIS_STAND_CONSCURSOR,
		    (int)&ca, FKIOCTL, credp, &rval);
	else
		(void) ddi_lyr_ioctl(ltem_sp->hdl, VIS_CONSCURSOR,
		    (int)&ca, FKIOCTL, credp, &rval);
}

static void
set_font(struct font *f, short *rows, short *cols, short height, short width)
{
	bitmap_data_t	*fontToUse = NULL;
	struct fontlist	*fl;

	/*
	 * Find best font for these dimensions, or use default
	 *
	 * The plus 2 is to make sure we have at least a 1 pixel
	 * boarder around the entire screen.
	 */
	for (fl = fonts; fl->name; fl++) {
		if (fl->data &&
		    (((*rows * fl->data->height) + 2) <= height) &&
		    (((*cols * fl->data->width) + 2) <= width)) {
			fontToUse = fl->data;
			break;
		}
	}

	/*
	 * The minus 2 is to make sure we have at least a 1 pixel
	 * boarder around the entire screen.
	 */
	if (fontToUse == NULL) {
		if (((*rows * builtin_font_data.height) > height) ||
		    ((*cols * builtin_font_data.width) > width)) {
			*rows = (height - 2) / builtin_font_data.height;
			*cols = (width - 2) / builtin_font_data.width;
		}
		fontToUse = &builtin_font_data;
	}

	f->width = fontToUse->width;
	f->height = fontToUse->height;
	bcopy((caddr_t)fontToUse->encoding, (caddr_t)f->char_ptr,
			sizeof (f->char_ptr));
	f->image_data = fontToUse->image;
	f->image_data_size = fontToUse->image_size;

	/* Free extra data structures and bitmaps	*/

	for (fl = fonts; fl->name; fl++) {
		if (fl->data) {
			if (fontToUse != fl->data && fl->data->image_size)
			    kmem_free(fl->data->image, fl->data->image_size);
			kmem_free(fl->data->encoding, fl->data->encoding_size);
			kmem_free(fl->data, sizeof (*fl->data));
		}
	}
}

#if defined(HAVE_1BIT)
/*
 * bit_to_pix1 is for 1-bit frame buffers.  It will essentially pass-through
 * the bitmap, possibly inverting it for reverse video.
 *
 * An input data byte of 0x53 will output the bit pattern 01010011.
 */

static void
bit_to_pix1(struct font *f, ushort flags, unchar c, unchar *dest)
{
	int	row;
	int	i;
	unchar	*cp;
	int	bytesWide;
	unchar	data;

	cp = f->char_ptr[c];
	bytesWide = (f->width + 7) / 8;

	for (row = 0; row < f->height; row++) {
		for (i = 0; i < bytesWide; i++) {
			data = *cp++;
			if (flags & TEM_ATTR_REVERSE) {
				*dest++ = INVERSE(data);
			} else {
				*dest++ = data;
			}
		}
	}
}
#endif	HAVE_1BIT
/*
 * bit_to_pix4 is for 4-bit frame buffers.  It will write one output byte
 * for each 2 bits of input bitmap.  It inverts the input bits before
 * doing the output translation, for reverse video.
 *
 * Assuming foreground is 0001 and background is 0000...
 * An input data byte of 0x53 will output the bit pattern
 * 00000001 00000001 00000000 00010001.
 */

static void
bit_to_pix4(struct font *f, ushort flags, unchar c, unchar *dest)
{
	int	row;
	int	byte;
	int	i;
	unchar	*cp;
	unchar	data;
	unchar	nibblett;
	int	bytesWide;
	unchar	fg_color = TEM_TEXT_BLACK;
	unchar	bg_color = TEM_TEXT_WHITE;

	if (flags & TEM_ATTR_REVERSE) {
		fg_color = TEM_TEXT_WHITE;
		bg_color = TEM_TEXT_BLACK;
	}

	cp = f->char_ptr[c];
	bytesWide = (f->width + 7) / 8;

	for (row = 0; row < f->height; row++) {
		for (byte = 0; byte < bytesWide; byte++) {
			data = *cp++;
			for (i = 0; i < 4; i++) {
				nibblett = (data >> ((3-i) * 2)) & 0x3;
				switch (nibblett) {
				case 0x0:
					*dest++ = bg_color << 4 | bg_color;
					break;
				case 0x1:
					*dest++ = bg_color << 4 | fg_color;
					break;
				case 0x2:
					*dest++ = fg_color << 4 | bg_color;
					break;
				case 0x3:
					*dest++ = fg_color << 4 | fg_color;
					break;
				}
			}
		}
	}
}

/*
 * bit_to_pix8 is for 8-bit frame buffers.  It will write one output byte
 * for each bit of input bitmap.  It inverts the input bits before
 * doing the output translation, for reverse video.
 *
 * Assuming foreground is 00000001 and background is 00000000...
 * An input data byte of 0x53 will output the bit pattern
 * 0000000 000000001 00000000 00000001 00000000 00000000 00000001 00000001.
 */


static void
bit_to_pix8(struct font *f, ushort flags, unchar c, unchar *dest)
{
	int	row;
	int	byte;
	int	i;
	unchar	*cp;
	unchar	data;
	int	bytesWide;
	unchar	fg_color = TEM_TEXT_BLACK;
	unchar	bg_color = TEM_TEXT_WHITE;
	unsigned char	mask;
	int	bitsleft, nbits;

	if (flags & TEM_ATTR_REVERSE) {
		fg_color = TEM_TEXT_WHITE;
		bg_color = TEM_TEXT_BLACK;
	}

	cp = f->char_ptr[c];
	bytesWide = (f->width + 7) / 8;

	for (row = 0; row < f->height; row++) {
		bitsleft = f->width;
		for (byte = 0; byte < bytesWide; byte++) {
			data = *cp++;
			mask = 0x80;
			nbits = min(8, bitsleft);
			bitsleft -= nbits;
			for (i = 0; i < nbits; i++) {
				*dest++ = (data & mask ? fg_color: bg_color);
				mask = mask >> 1;
			}
		}
	}
}

/*
 * bit_to_pix24 is for 24-bit frame buffers.  It will write four output bytes
 * for each bit of input bitmap.  It inverts the input bits before
 * doing the output translation, for reverse video.
 *
 * Assuming foreground is 00000000 11111111 11111111 11111111
 * and background is 00000000 00000000 00000000 00000000
 * An input data byte of 0x53 will output the bit pattern
 *
 * 00000000 00000000 00000000 00000000
 * 00000000 11111111 11111111 11111111
 * 00000000 00000000 00000000 00000000
 * 00000000 11111111 11111111 11111111
 * 00000000 00000000 00000000 00000000
 * 00000000 00000000 00000000 00000000
 * 00000000 11111111 11111111 11111111
 * 00000000 11111111 11111111 11111111
 *
 * FYI this is a pad byte followed by 1 byte each for R,G, and B.
 */

/*
 * A 24-bit pixel trapped in a 32-bit body.
 */
typedef unsigned long pixel32;

/*
 * Union for working with 24-bit pixels in 0RGB form, where the
 * bytes in memory are 0 (a pad byte), red, green, and blue in that order.
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

static void
bit_to_pix24(struct font *f, ushort flags, unchar c, unchar *dest)
{
	int	row;
	int	byte;
	int	i;
	unchar	*cp;
	unchar	data;
	int	bytesWide;
	union pixel32_0RGB	fg_color;
	union pixel32_0RGB	bg_color;
	int	bitsleft, nbits;
	pixel32	*destp;

	fg_color.bytes.pad = 0;
	bg_color.bytes.pad = 0;
	if (flags & TEM_ATTR_REVERSE) {
		fg_color.bytes.red = TEM_TEXT_WHITE24_RED;
		fg_color.bytes.green = TEM_TEXT_WHITE24_GREEN;
		fg_color.bytes.blue = TEM_TEXT_WHITE24_BLUE;
		bg_color.bytes.red = TEM_TEXT_BLACK24_RED;
		bg_color.bytes.green = TEM_TEXT_BLACK24_GREEN;
		bg_color.bytes.blue = TEM_TEXT_BLACK24_BLUE;
	} else {
		fg_color.bytes.red = TEM_TEXT_BLACK24_RED;
		fg_color.bytes.green = TEM_TEXT_BLACK24_GREEN;
		fg_color.bytes.blue = TEM_TEXT_BLACK24_BLUE;
		bg_color.bytes.red = TEM_TEXT_WHITE24_RED;
		bg_color.bytes.green = TEM_TEXT_WHITE24_GREEN;
		bg_color.bytes.blue = TEM_TEXT_WHITE24_BLUE;
	}

	/*LINTED*/
	destp = (pixel32 *)dest;
	cp = f->char_ptr[c];
	bytesWide = (f->width + 7) / 8;

	for (row = 0; row < f->height; row++) {
		bitsleft = f->width;
		for (byte = 0; byte < bytesWide; byte++) {
			data = *cp++;
			nbits = min(8, bitsleft);
			bitsleft -= nbits;
			for (i = 0; i < nbits; i++) {
				*destp++ = (data & 0x80 ?
						fg_color.pix : bg_color.pix);
				data <<= 1;
			}
		}
	}
}
