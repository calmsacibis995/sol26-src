/*
 * Copyright (c) 1989-1993, by Sun Microsystems, Inc.
 */

#pragma ident "@(#)console.c	1.17	96/03/14 SMI"

#include <sys/types.h>

#if defined(__ppc)
#include <sys/kmem.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/visual_io.h>
#include <sys/ltem.h>
#include <sys/font.h>
#endif /* defined(__ppc) */

#include <sys/modctl.h>
#include <sys/vnode.h>
#include <sys/console.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/esunddi.h>

#define	MINLINES	10
#define	MAXLINES	48
#define	LOSCREENLINES	34
#define	HISCREENLINES	48

#define	MINCOLS		10
#define	MAXCOLS		120
#define	LOSCREENCOLS	80
#define	HISCREENCOLS	120

vnode_t *ltemvp = NULL;
dev_t ltemdev;

#if defined(__ppc)
struct fontlist fonts[] = {
	{"fonts/large.pcf", NULL, NULL},
	{"fonts/med.pcf", NULL, NULL},
	{"fonts/small.pcf", NULL, NULL},
	{NULL, NULL, NULL}
};
#endif
/*
 * Gets the number of rows and columns (in char's) and the
 * width and height (in pixels) of the console.
 */
void
console_get_size(ushort *r, ushort *c, ushort *x, ushort *y)
{
	u_char *data;
	u_char *p;
	static char *cols = "screen-#columns";
	static char *rows = "screen-#rows";
	static char *width = "screen-width";
	static char *height = "screen-height";
	u_int len;
	int	rel_needed = 0;
	dev_info_t *dip;
	dev_t dev;

	/*
	 * If we have loaded the console IO stuff, then ask for the screen
	 * size properties from the layered terminal emulator.  Else ask for
	 * them from the root node, which will eventually fall through to the
	 * options node and get them from the prom.
	 */
	if (ltemvp == NULL) {
		dip = ddi_root_node();
		dev = DDI_DEV_T_ANY;
	} else {
		dip = e_ddi_get_dev_info(ltemdev, VCHR);
		dev = ltemdev;
		rel_needed = 1;
	}

	/*
	 * If we have not initialized a console yet and don't have a root
	 * node (ie. we have not initialized the DDI yet) return our default
	 * size for the screen.
	 */
	if (dip == NULL) {
		*r = LOSCREENLINES;
		*c = LOSCREENCOLS;
		*x = *y = 0;
		return;
	}

	*c = 0;
	/*
	 * Get the number of columns
	 */
	if (ddi_prop_lookup_byte_array(dev, dip, 0, cols, &data, &len) ==
	    DDI_PROP_SUCCESS) {
		p = data;
		data[len] = '\0';
		*c = stoi((char **)&p);
		ddi_prop_free(data);
	}

	if (*c < MINCOLS)
		*c = LOSCREENCOLS;
	else if (*c > MAXCOLS)
		*c = HISCREENCOLS;

	*r = 0;
	/*
	 * Get the number of rows
	 */
	if (ddi_prop_lookup_byte_array(dev, dip, 0, rows, &data, &len) ==
	    DDI_PROP_SUCCESS) {
		p = data;
		data[len] = '\0';
		*r = stoi((char **)&p);
		ddi_prop_free(data);
	}

	if (*r < MINLINES)
		*r = LOSCREENLINES;
	else if (*r > MAXLINES)
		*r = HISCREENLINES;

	*x = 0;
	/*
	 * Get the number of pixels wide
	 */
	if (ddi_prop_lookup_byte_array(dev, dip, 0, width, &data, &len) ==
	    DDI_PROP_SUCCESS) {
		p = data;
		data[len] = '\0';
		*x = stoi((char **)&p);
		ddi_prop_free(data);
	}

	*y = 0;
	/*
	 * Get the number of pixels high
	 */
	if (ddi_prop_lookup_byte_array(dev, dip, 0, height, &data, &len) ==
	    DDI_PROP_SUCCESS) {
		p = data;
		data[len] = '\0';
		*y = stoi((char **)&p);
		ddi_prop_free(data);
	}

	if (rel_needed)
		ddi_rele_driver(getmajor(ltemdev));

}

#if defined(__ppc)
static void
consolereadfonts(void)
{
	struct fontlist *fl;

	for (fl = fonts; fl->name != NULL; fl++)
		if (fl->fontload != NULL)
			if ((fl->data = (bitmap_data_t *)
			    (fl->fontload)(fl->name)) == NULL)
				cmn_err(CE_NOTE,
				    "Could not load font file %s\n",
				    fl->name);
}

void
consoleloadfonts(void)
{
	int rfmodid;

	if ((rfmodid = modload("misc", "readfont")) != -1) {
		(void) consolereadfonts();
		(void) modunload(rfmodid);
	} else {
		cmn_err(CE_NOTE, "Could not load readfont module\n");
	}
}

void
consoleconfig(void)
{
	char *s;
	char *fbdriver_path;
	major_t ltem_major;
	int rvalp = 0;

	/*
	 * Used by output_line() in cmn_err.c.  Marks progress
	 * in bringup.   We can now start using softcall() if the
	 * other conditions are met.
	 */
	post_consoleconfig = 1;

	if (!stdout_is_framebuffer())
		return;
	/*
	 * Find pathname of framebuffer driver
	 */
	if ((s = stdout_path()) == NULL) {
		cmn_err(CE_NOTE, "No console frame buffer driver\n");
		return;
	}
	fbdriver_path = kmem_alloc(MAXPATHLEN, KM_SLEEP);
	strncpy(fbdriver_path, s, MAXPATHLEN);

	/*
	 * Build a vnode for the layered terminal emultor
	 */
	if ((ltem_major = ddi_name_to_major("ltem")) == (major_t)-1) {
		cmn_err(CE_NOTE, "No major number for: ltem\n");
		kmem_free(fbdriver_path, MAXPATHLEN);
		return;
	}

	/*
	 * Build a dev_t for the layered terminal emulator
	 */
	ltemdev = makedevice(ltem_major, 0);

	if ((ltemvp = makespecvp(ltemdev, VCHR)) == NULL) {
		cmn_err(CE_NOTE, "Can't build vnode for ltem\n");
		kmem_free(fbdriver_path, MAXPATHLEN);
		return;
	}

	/*
	 * Open the layered terminal emulator
	 */
	if (VOP_OPEN(&ltemvp, FWRITE|FNOCTTY, kcred)) {
		cmn_err(CE_NOTE, "Could not open ltem\n");
		ltemvp = NULL;
		kmem_free(fbdriver_path, MAXPATHLEN);
		return;
	}

	/*
	 * Layer the framebuffer driver under the terminal emulator
	 */
	if (VOP_IOCTL(ltemvp, LTEM_OPEN, (int)fbdriver_path, FKIOCTL,
	    kcred, &rvalp)) {
		VOP_CLOSE(ltemvp, FWRITE+FNOCTTY, 1, 0, kcred);
		ltemvp = NULL;
		kmem_free(fbdriver_path, MAXPATHLEN);
		return;
	}

	kmem_free(fbdriver_path, MAXPATHLEN);

	console_connect();
	ddi_set_console_bell(console_default_bell);
}

void
console_write_str(char *str, u_int len)
{
	struct uio uio;
	struct iovec iov;

	/*
	 * Create a uio struct
	 */
	iov.iov_base = str;
	iov.iov_len = len;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_loffset = 0;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_resid = len;
	uio.uio_limit = len + 1;
	uio.uio_fmode = FWRITE;

	/*
	 * Write the data to the console
	 */
	VOP_RWLOCK(ltemvp, 1);
	(void) VOP_WRITE(ltemvp, &uio, 0, kcred);
	VOP_RWUNLOCK(ltemvp, 1);
}

void
console_write_char(char c)
{
	char str[1];

	str[0] = c;

	console_write_str(str, 1);
}
#endif /* defined(__ppc) */
