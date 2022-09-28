
/*
 * Copyright (c) 1994-1995 by Sun Microsystems, Inc.  All Rights Reserved.
 */

#ifndef	_SYS_TEM_H
#define	_SYS_TEM_H

#pragma ident	"@(#)tem.h	1.17	95/11/13 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/sunddi_lyr.h>
#include <sys/visual_io.h>

/*
 * So the terminal emulator knows who called it....
 */
#define	TEM_NOT_FROM_STAND	0
#define	TEM_FROM_STAND		1

/*
 * definitions for Integrated Workstation Environment ANSI x3.64
 * terminal control language parser
 */

#define	TEM_MAXPARAMS	5	/* maximum number of ANSI paramters */
#define	TEM_MAXTAB	40	/* maximum number of tab stops */
#define	TEM_MAXFKEY	30	/* max length of function key with <ESC>Q */
#define	MAX_TEM		2	/* max number of loadable terminal emulators */


#define	TEMPSZ		64	/* max packet size sent by ANSI */

#define	TEM_ROW			0
#define	TEM_COL			1
#define	TEM_SCROLL_UP		0
#define	TEM_SCROLL_DOWN		1
#define	TEM_SHIFT_LEFT		0
#define	TEM_SHIFT_RIGHT		1

#define	TEM_ATTR_NORMAL		0x0000
#define	TEM_ATTR_REVERSE	0x0001
#define	TEM_ATTR_BOLD		0x0002
#define	TEM_ATTR_BLINK		0x0004
#define	TEM_ATTR_TRANSPARENT	0x0008
#define	TEM_ATTR_SCREEN_REVERSE	0x0010

#define	TEM_TEXT_WHITE		0
#define	TEM_TEXT_BLACK		1
#define	TEM_TEXT_BLACK24_RED	0x00
#define	TEM_TEXT_BLACK24_GREEN	0x00
#define	TEM_TEXT_BLACK24_BLUE	0x00
#define	TEM_TEXT_WHITE24_RED	0xff
#define	TEM_TEXT_WHITE24_GREEN	0xff
#define	TEM_TEXT_WHITE24_BLUE	0xff

#define	A_STATE_START			0
#define	A_STATE_ESC			1
#define	A_STATE_ESC_Q			2
#define	A_STATE_ESC_Q_DELM		3
#define	A_STATE_ESC_Q_DELM_CTRL		4
#define	A_STATE_ESC_C			5
#define	A_STATE_CSI			6
#define	A_STATE_CSI_QMARK		7
#define	A_STATE_CSI_EQUAL		8

/*
 * Default number of rows and columns
 */
#define	TEM_DEFAULT_ROWS	34
#define	TEM_DEFAULT_COLS	80

#define	BUF_LEN		160 /* Two lines of data can be processed at a time */

typedef struct tem_state temstat_t;

#define	get_soft_state(dev)	ddi_get_soft_state(ltem_state_head,\
					getminor(dev))

/*
 * Per instance state structure for ltem.
 */
struct ltem_state {
	ddi_lyr_handle_t	hdl; /* Framework handle for layered on dev */
	temstat_t		*tem_state; /* T.E. State structure */
	screen_size_t		linebytes; /* Layered on bytes per scan line */
	uint			size; /* Layered on driver size */
	u_int			display_mode; /* What mode we are in */
	dev_info_t		*dip; /* Our dip */
	kmutex_t		lock;
	int			stand_writes; /* Ok to do standalone writes */
};

struct in_func_ptrs {
	void (*f_display)(struct ltem_state *, unchar *, int,
	    screen_pos_t, screen_pos_t, cred_t *, u_int);
	void (*f_copy)(struct ltem_state *,
	    screen_pos_t, screen_pos_t, screen_pos_t, screen_pos_t,
	    screen_pos_t, screen_pos_t, int, cred_t *, u_int);
	void (*f_cursor)(struct ltem_state *, short, cred_t *, u_int);
	void (*f_bit2pix)(struct font *, ushort, unchar, unchar *);
	void (*f_cls)(struct ltem_state *, int,
	    screen_pos_t, screen_pos_t, cred_t *, u_int);
};

/*
 * State structure for terminal emulator
 */
struct tem_state {		/* state for tem x3.64 emulator */
	ushort	a_flags;	/* flags for this x3.64 terminal */
	unchar	a_state;	/* state in output esc seq processing */
	unchar	a_gotparam;	/* does output esc seq have a param */
	ushort	a_curparam;	/* current param # of output esc seq */
	ushort	a_paramval;	/* value of current param */
	short	a_params[TEM_MAXPARAMS];  /* parameters of output esc seq */
	char	a_fkey[TEM_MAXFKEY];	/* work space for function key */
	short	a_tabs[TEM_MAXTAB];	/* tab stops */
	short	a_ntabs;		/* number of tabs used */
	ushort	a_nscroll;		/* number of lines to scroll */
	screen_pos_t	a_s_cursor[2];	/* start cursor position */
	screen_pos_t	a_c_cursor[2];	/* current cursor position */
	screen_pos_t	a_r_cursor[2];	/* remembered cursor position */
	screen_size_t	a_c_dimension[2]; /* window dimensions in characters */
	screen_size_t	a_p_dimension[2]; /* screen dimensions in pixels */
	screen_pos_t	a_p_offset[2]; /* pixel offsets to center the display */
	unchar	*a_outbuf;	/* place to keep incomplete lines */
	unchar	*a_blank_line;	/* place to keep a blank line for scrolling */
	short	a_outindex;	/* index into a_outbuf */
	struct in_func_ptrs	in_fp;	/* internal output functions */
	struct font	a_font;	/* font table */
	int	a_pdepth;	/* pixel depth */
	int	a_initialized;	/* initialization flag */
	unchar   *a_pix_data;	/* pointer to tmp bitmap area */
	unchar   *a_cls_pix_data;	/* pointer to cls bitmap area */
	unchar   *a_rcls_pix_data;	/* pointer to reverse cls bitmap area */
	u_int	a_pix_data_size; /* size of bitmap data areas */
};

#ifdef __cplusplus
}
#endif

#endif /* _SYS_TEM_H */
