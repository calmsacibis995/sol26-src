/*
 * Copyright (c) 1993, 1996, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef _SYS_VISUAL_IO_H
#define	_SYS_VISUAL_IO_H

#pragma ident	"@(#)visual_io.h	1.23	96/02/20 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#define	VIOC	('V' << 8)
#define	VIOCF	('F' << 8)


/*
 * Device Identification
 *
 * VIS_GETIDENTIFIER returns an identifier string to uniquely identify
 * a device type used in the Solaris VISUAL environment.  The identifier
 * must be unique.  We suggest the convention:
 *
 *	<companysymbol><devicetype>
 *
 * for example: SUNWcg6
 */

#define	VIS_MAXNAMELEN 128

struct vis_identifier {
	char name[VIS_MAXNAMELEN];	/* <companysymbol><devicename>	*/
};

#define	VIS_GETIDENTIFIER	(VIOC | 0)



/*
 * Hardware Cursor Control
 *
 * Devices with hardware cursors may implement these ioctls in their
 * kernel device drivers.
 */


struct vis_cursorpos {
	short x;		/* cursor x coordinate	*/
	short y;		/* cursor y coordinate	*/
};

struct vis_cursorcmap {
	int		version;	/* version			*/
	int		reserved;
	unsigned char	*red;		/* red color map elements	*/
	unsigned char	*green;		/* green color map elements	*/
	unsigned char	*blue;		/* blue color map elements	*/
};


/*
 * These ioctls fetch and set various cursor attributes, using the
 * vis_cursor struct.
 */

#define	VIS_SETCURSOR	(VIOCF|24)
#define	VIS_GETCURSOR	(VIOCF|25)

struct vis_cursor {
	short			set;		/* what to set		*/
	short			enable;		/* cursor on/off	*/
	struct vis_cursorpos	pos;		/* cursor position	*/
	struct vis_cursorpos	hot;		/* cursor hot spot	*/
	struct vis_cursorcmap	cmap;		/* color map info	*/
	struct vis_cursorpos	size;		/* cursor bit map size	*/
	char			*image;		/* cursor image bits	*/
	char			*mask;		/* cursor mask bits	*/
};

#define	VIS_CURSOR_SETCURSOR	0x01		/* set cursor		*/
#define	VIS_CURSOR_SETPOSITION	0x02		/* set cursor position	*/
#define	VIS_CURSOR_SETHOTSPOT	0x04		/* set cursor hot spot	*/
#define	VIS_CURSOR_SETCOLORMAP	0x08		/* set cursor colormap	*/
#define	VIS_CURSOR_SETSHAPE	0x10		/* set cursor shape	*/

#define	VIS_CURSOR_SETALL	(VIS_CURSOR_SETCURSOR | \
    VIS_CURSOR_SETPOSITION	| \
    VIS_CURSOR_SETHOTSPOT	| \
    VIS_CURSOR_SETCOLORMAP	| \
    VIS_CURSOR_SETSHAPE)


/*
 * These ioctls fetch and move the current cursor position, using the
 * vis_cursorposition struct.
 */

#define	VIS_MOVECURSOR		(VIOCF|26)
#define	VIS_GETCURSORPOS	(VIOCF|27)

/*
 * VIS_SETCMAP:
 * VIS_GETCMAP:
 * Set/Get the indicated color map entries.  The index states the first
 * color to be update and count specifies the number of entries to be
 * updated from index.  red, green, and blue are arrays of color
 * values.  The length of the arrays is count.
 */
#define	VIS_GETCMAP	(VIOC|9)
#define	VIS_PUTCMAP	(VIOC|10)
struct viscmap {
	int		index; /* Index into colormap to start updating */
	int		count; /* Number of entries to update */
	unsigned char	*red; /* List of red values */
	unsigned char	*green; /* List of green values */
	unsigned char	*blue; /* List of blue values */
};


#ifdef _KERNEL
/*
 * The following ioctls are used for communication between the layered
 * device and the framebuffer.  The layered driver calls the framebuffer
 * with these ioctls.
 *
 * On machines that don't have a prom, kadb uses the kernel to display
 * characters.  The kernel in turn will use the ioctls below that start with
 * VIS_STAND to ask the framebuffer driver to display the data.  The
 * framebuffer driver CANNOT use any DDI services to display this data.  It
 * must just dump the data to the framebuffer.  In particular, the mutex and
 * copy routines do not work.
 *
 * On machines without a prom, the framebuffer driver must implement all
 * of these ioctls to be a console.  On machines with a prom, the
 * framebuffer driver must implement all of these ioctls except for the ones
 * that start with VIS_STAND.
 */
typedef short screen_pos_t;
typedef short screen_size_t;

/*
 * Union of pixel depths
 */
typedef union {
	unsigned char  mono;   /* one-bit */
	unsigned char  four;   /* four bit */
	unsigned char  eight;  /* eight bit */
	unsigned char  twentyfour[3];  /* 24 bit */
} color_t;

/*
 * VIS_DEVINIT:
 * Initialize the framebuffer as a console device.  The terminal emulator
 * will provide the following structure to the device driver to be filled in.
 * The driver is expected to fill it in.
 *
 */
#define	VIS_DEVINIT	(VIOC|1)
struct vis_devinit {
	int		version; /* Console IO interface version */
	screen_size_t	width; /* Width of the device */
	screen_size_t	height; /* Height of the device */
	screen_size_t	linebytes; /* Bytes per scan line */
	uint		size; /* Total size of the device */
	int		depth; /* Device depth */
	short		mode; /* Mode to use when displaying data */
};
/* Version */
#define	VIS_CONS_REV		1 /* Console IO interface version */
/* Modes */
#define	VIS_TEXT		0 /* Use text mode when displaying data */
#define	VIS_PIXEL		1 /* Use pixel mode when displaying data */
#define	VIS_TRANSPARENT		2 /* Just pass data through unmodified */

/*
 * VIS_DEVFINI:
 * Tells the framebuffer that it is no longer being used as a console.
 */
#define	VIS_DEVFINI	(VIOC|2)

/*
 * VIS_CONSCURSOR:
 * VIS_STAND_CONSCURSOR:
 * Display/Hide cursor on the screen.  The layered driver uses this ioctl to
 * display, hide, and move the cursor on the console.  The framebuffer driver
 * is expected to draw a cursor at position (col,row) of size width x height.
 *
 * If the request is from kadb the framebuffer driver is not allowed to
 * use any of the DDI serivces to display the data.
 */
#define	VIS_CONSCURSOR		(VIOC|3)
#define	VIS_STAND_CONSCURSOR	(VIOC|4)
struct vis_conscursor {
	int		version; /* Version of this structure */
	screen_pos_t	row; /* Row to display cursor at */
	screen_pos_t	col; /* Col to display cursor at */
	screen_size_t	width; /* Width of cursor */
	screen_size_t	height; /* Height of cursor */
	color_t		fg_color; /* Foreground color */
	color_t		bg_color; /* Background color */
	short		action; /* Hide or show cursor */
};
/* Version */
#define	VIS_CURSOR_VERSION	1
/* Cursor action - Either display or hide cursor */
#define	VIS_HIDE_CURSOR		0
#define	VIS_DISPLAY_CURSOR	1

/*
 * VIS_CONSDISPLAY:
 * VIS_STAND_CONSDISPLAY:
 * Display data on the framebuffer.  The data will be in the form specified
 * by the driver during console initialization (see VIS_CONSDEVINIT above).
 * The driver is expected to display the data at location (row,col).  Width
 * and height specify the size of the data.
 *
 * If the request is from kadb the framebuffer driver is not allowed to
 * use any of the DDI serivces to display the data.
 */

#define	VIS_CONSDISPLAY		(VIOC|5)
#define	VIS_STAND_CONSDISPLAY	(VIOC|6)
struct vis_consdisplay {
	int		version; /* Version of this structure */
	screen_pos_t	row; /* Row to display data at */
	screen_pos_t	col; /* Col to display data at */
	screen_size_t	width; /* Width of data */
	screen_size_t	height; /* Height of data */
	unsigned char	*data; /* Data to display */
	unsigned char	fg_color; /* Foreground color */
	unsigned char	bg_color; /* Background color */
};
/* Version */
#define	VIS_DISPLAY_VERSION		1

/*
 * VIS_CONSCOPY:
 * VIS_STAND_CONSCOPY:
 * Move data on the framebuffer.  Used to scroll the screen by the terminal
 * emulator or to move data by applications.  The driver is expected to
 * copy the data specified by the rectangle (s_col,s_row),(e_col,e_row) to
 * the location which starts at (t_col,t_row).  The direction of the move
 * is specfied in direction as VIS_COPY_FORWARD and VIS_COPY_BACKWARD.
 *
 * The driver is expected to copy the data from one location to the next.
 * Starting at (s_col,s_row) if direction is VIS_COPY_FORWARD or
 * (e_col,e_row) if direction is VIS_COPY_BACKWARDS.
 *
 * If the request is from kadb the framebuffer driver is not allowed to
 * use any of the DDI serivces to display the data.
 */
#define	VIS_CONSCOPY		(VIOC|7)
#define	VIS_STAND_CONSCOPY		(VIOC|8)
struct vis_conscopy {
	int		version; /* Version of this structure */
	screen_pos_t	s_row; /* Starting row */
	screen_pos_t	s_col; /* Starting col */
	screen_pos_t	e_row; /* Ending row */
	screen_pos_t	e_col; /* Ending col */
	screen_pos_t	t_row; /* Row to move to */
	screen_pos_t	t_col; /* Col to move to */
	short		direction; /* Direction of move */
};
/* Version */
#define	VIS_COPY_VERSION		1
/* Direction to move */
#define	VIS_COPY_FORWARD	0
#define	VIS_COPY_BACKWARD	1

#endif	/* _KERNEL */

/*
 * VIS_CONS_MODE_CHANGE
 * Resets the mode of the console framebuffer.
 */
#define	VIS_CONS_MODE_CHANGE	(VIOC|11)

#ifdef __cplusplus
}
#endif

#endif	/* !_SYS_VISUAL_IO_H */
