/*
 * Copyright (c) 1992-1996, Sun Microsystems, Inc.  All Rights Reserved.
 */

#pragma ident "@(#)misc_utls.c 1.26       96/07/30 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/bootlink.h>
#include <sys/bootconf.h>
#include <sys/sysenvmt.h>
#include <sys/booti386.h>
#include <sys/ramfile.h>
#include <sys/dosemul.h>
/* #include <stdarg.h> XXX apparently we're getting this from somewhere... */
#include <sys/salib.h>
#include <sys/promif.h>
#include <ctype.h>

int 		getchar();
void		bootabort();
char		*find_fileext();

extern int	boot_device;

extern int doint_r(struct int_pb *);
extern int doint_asm();
void putchar(int c);
extern caddr_t rm_malloc(u_int, u_int, caddr_t);
extern void rm_free(caddr_t, u_int);
extern int ischar();

static int cons_more = 0;
static int cons_nextchar;

void
printf(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	prom_vprintf(fmt, ap);
	va_end(ap);
}

void
silence_nets()
{
	struct int_pb	ic;

/*	printf("silence nets stub\n"); */
	if (boot_device == BOOT_FROM_NET) {
		/*
		 * close the network device, particularly needed for
		 * token ring
		 */
		ic.intval = 0xfb;
		ic.ax = 0x0200;
		(void) doint_r(&ic);
	}
}

/* misc good stuff from ISC not yet put anywhere particular */
void *
memset(void *dest, int c, size_t cnt)
{
	void *odest = dest;
	u_char *p = dest;

#ifdef DEBUG_MEMSET
	printf("memset addr 0x%x to 0x%x for %d bytes\n",
		dest, c, cnt);
#endif

	while (cnt-- > 0)
		*p++ = (u_char)c;
	return (odest);
}

void
waitEnter()
{
	(void) getchar();
	putchar('\r');
	putchar('\n');
}

int
goany(void)
{
	printf("Press ENTER to continue");
	waitEnter();
	return (0);
}

ushort
bcd_to_bin(ushort n)
{
	return (((n >> 12) & 0xf)*1000 +
	((n >> 8) & 0xf)*100 +
	((n >> 4) & 0xf)*10 +
	((n) & 0xf));
}

struct real_regs *
alloc_regs(void)
{
	struct real_regs *rp;

	rp = (struct real_regs *)rm_malloc(sizeof (struct real_regs), 0, 0);
	if (rp)
		bzero((char *)rp, sizeof (struct real_regs));
	return (rp);
}

void
free_regs(struct real_regs *rp)
{
	if (rp)
		rm_free((caddr_t)rp, sizeof (struct real_regs));
}

#include <sys/cmosram.h>

/*
 * NB: this function does not return valid disk geometry data
 * when the controller is a scsi controller
 */
unsigned char
CMOSread(loc)
int loc;
{
	outb(CMOS_ADDR, loc);	/* request cmos address */
	return (inb(CMOS_DATA));	/* return contents */
}

memcmp(ref, cmp, cnt)
register char *ref, *cmp;
register size_t	cnt;
{
	while ((*ref++ == *cmp++) && (--cnt > 0))
		;
	return ((int)(*--ref & 0xff)-(int)(*--cmp & 0xff));
}

#ifdef notdef
int
getchar()
{
	struct int_pb	ic;

	ic.intval = 0x16;
	for (ic.ax = 0; ic.ax == 0; )

		(void) doint_r(&ic);

	return (ic.ax & 0xFF);
}
#endif

/*	flush keyboard	*/
void
kb_flush()
{

	while (ischar())
		(void) getchar();
}

/*	Terminate boot program	*/
void
bootabort()
{
	void kb_flush();

	printf("\007\nUse Ctl-Alt-Del to reboot\007");
	for (;;) {
		kb_flush();
	}
}

/*
 *  str{n}casecmp
 *  Routines for handling strcmp's where we aren't concerned if the
 *  case differs between string elements.
 */
int
strcasecmp(register char *s1, register char *s2)
{
	while (toupper(*s1) == toupper(*s2++))
		if (*s1++ == '\0')
			return (0);
	return (toupper(*s1) - toupper(*--s2));
}

int
strncasecmp(register char *s1, register char *s2, register int n)
{
	while (--n >= 0 && toupper(*s1) == toupper(*s2++))
		if (*s1++ == '\0')
			return (0);
	return (n < 0 ? 0 : toupper(*s1) - toupper(*--s2));
}

/*
 *  find_fileext
 *	Given a filename string, returns pointer to the name's
 *	extension, if any.  This simply returns a pointer
 *	into the original filename string at the first
 *	character past the last period found in the name.
 *	Returns null if no extension found.
 */
char *
find_fileext(char *pn)
{
	char *rs;

	if ((rs = strrchr(pn, '.')) != NULL) rs++;
	return (rs);
}

#include "chario.h"

char cons_getc(_char_io_p);
void cons_putc(_char_io_p, char);
int cons_avail(_char_io_p);
void cons_clear_screen(_char_io_p);
void cons_set_cursor(_char_io_p, int, int);

_char_io_t cons_in = {
	(_char_io_p)0, "keyboard",
	0, 0, 0,
	CHARIO_IN_ENABLE,
	(char *)0,
	cons_getc,
	cons_putc,
	cons_avail,
	cons_clear_screen,
	cons_set_cursor
};

_char_io_t console = {
	&cons_in,
	"screen",
	0, 0, 0,
	CHARIO_OUT_ENABLE,
	(char *)0,
	cons_getc,
	cons_putc,
	cons_avail,
	cons_clear_screen,
	cons_set_cursor
};

char serial_getc(_char_io_p);
void serial_putc(_char_io_p, char);
int serial_avail(_char_io_p);
void serial_clear_screen(_char_io_p);
void serial_set_cursor(_char_io_p, int, int);

static void
linesplat(_char_io_p p, char *s)
{
	while (s && *s)
		(*p->putc)(p, *s++);
	(*p->putc)(p, '\r');
}

int charsout;

void
putchar(int c)
{
	extern rffd_t *DOSsnarf_fp;
	extern short DOSsnarf_flag;
	extern short DOSsnarf_silent;
	_char_io_p p;
	int s;
	char sc = (char)c;

	if (c == '\t') {
		for (s = 8 - charsout % 8; s > 0; s--)
			putchar(' ');
		return;
	} else if (c == '\n') {
		charsout = 0;
		putchar('\r');
	} else
		charsout++;

	if (DOSsnarf_flag) {
		(void) RAMfile_write(DOSsnarf_fp, &sc, 1);
		if (DOSsnarf_silent)
			return;
	}

	for (p = &console; p; p = p->next)
		if ((p->flags & CHARIO_OUT_ENABLE) &&
		    !(p->flags & CHARIO_IGNORE_ALL))
			(*p->putc)(p, c);
}

int
getchar()
{
	_char_io_p p;
	char c;

	do {
		for (p = &console; p; p = p->next) {
			if ((p->flags & CHARIO_IN_ENABLE) &&
			    !(p->flags & CHARIO_IGNORE_ALL) &&
			    (*p->avail)(p)) {
				c =  (*p->getc)(p);
				return (c);
			}
		}
	/*CONSTCOND*/
	} while (1);
	/*NOTREACHED*/
}

int
ischar(void)
{
	_char_io_p p;

	for (p = &console; p; p = p->next)
		if ((p->flags & CHARIO_IN_ENABLE) &&
		    !(p->flags & CHARIO_IGNORE_ALL) &&
		    (*p->avail)(p))
			return (1);
	return (0);
}

char
cons_getc(_char_io_p p)
{
	struct int_pb	ic;
	int c;

	if (cons_more) {
		cons_more = 0;
		return (cons_nextchar);
	}

	/* ---- Read from keyboard ---- */
	ic.intval = 0x16;
	ic.ax = 0x1000;
	(void) doint_r(&ic);

	p->in++;
	c = ic.ax & 0xff;
	if ((c == 0xe0) || (c == 0xf0) || (c == 0)) {
		/* force arrows to work like DOS, returning zero first */
		cons_more = 1;
		cons_nextchar = (ic.ax >> 8) & 0xff;
		return (0);
	}
	return (c);
}


/*
 * cons_putc - output single character to PC physical console.
 *
 * The caller expects a subset of ANSI escape sequences to be interpreted.
 * Interpret what we know about here and pass the results down via doint_r().
 * For serial consoles, we assume there is an ANSI emulator on the other end.
 */


void
cons_putc(_char_io_p p, char c)
{
#define	MAXARG 10
#define	NORM_ATTRIB 0x7

	struct int_pb ic;
	static int cur_attrib = NORM_ATTRIB;	/* white on black */
	static int arg[MAXARG];			/* attribute array */
	static int argnum;			/* index into attributes */
	static enum { S_NORM, S_ESC, S_BRACKET } state = S_NORM;
	static enum { SCROLL_ALLOW, SCROLL_INHIBIT } scroll = SCROLL_ALLOW;
	static int seen_digits;
	int i;

	c &= 0x7f;
	switch (state) {
	case S_NORM:
		if (c == '\033') {
			state = S_ESC;
			return;
		}
		break;
	case S_ESC:
		if (c == '[') {
			state = S_BRACKET;
			argnum = 0;
			arg[0] = 0;
			seen_digits = 0;
			return;
		} else {
			state = S_NORM;
		}
		break;
	case S_BRACKET:
		if (c >= '0' && c <= '9') {
			arg[argnum] = arg[argnum] * 10 + (c - '0');
			seen_digits = 1;
			return;
		}
		if (seen_digits) {
			argnum++;
			if (argnum == MAXARG)
				argnum--;
			arg[argnum] = 0;
			seen_digits = 0;
		}
		switch (c) {
		case 'l':
			state = S_NORM;
			if (argnum == 1 && arg[0] == 7) {
				scroll = SCROLL_INHIBIT;
				return;
			}
			break;
		case 'h':
			state = S_NORM;
			if (argnum == 1 && arg[0] == 7) {
				scroll = SCROLL_ALLOW;
				return;
			}
			break;
		case ';':
			return;
		case 'm':
			state = S_NORM;
			/*
			 * special case ^[[0m and ^[[m to white on black.
			 */
			if ((argnum == 0) || ((argnum == 1) && (arg[0] == 0))) {
				cur_attrib = NORM_ATTRIB;
				return;
			}
#define	SET_FOREGROUND(x) cur_attrib = cur_attrib&0xf0 | (x)
#define	SET_BACKGROUND(x) cur_attrib = cur_attrib&0xf | (x<<4)

			for (i = 0; i < argnum; i++) {
				switch (arg[i]) {
				case 7:
					cur_attrib = 0x70; /* black on white */
					break;
				case 30:
					SET_FOREGROUND(0);
					break;
				case 31:
					SET_FOREGROUND(4);
					break;
				case 32:
					SET_FOREGROUND(2);
					break;
				case 33:
					SET_FOREGROUND(14);
					break;
				case 34:
					SET_FOREGROUND(1);
					break;
				case 35:
					SET_FOREGROUND(5);
					break;
				case 36:
					SET_FOREGROUND(3);
					break;
				case 37:
					SET_FOREGROUND(7);
					break;
				case 40:
					SET_BACKGROUND(0);
					break;
				case 41:
					SET_BACKGROUND(4);
					break;
				case 42:
					SET_BACKGROUND(2);
					break;
				case 43:
					SET_BACKGROUND(14);
					break;
				case 44:
					SET_BACKGROUND(1);
					break;
				case 45:
					SET_BACKGROUND(5);
					break;
				case 46:
					SET_BACKGROUND(3);
					break;
				case 47:
					SET_BACKGROUND(7);
					break;
				}
			}
#undef SET_FOREGROUND
#undef SET_BACKGROUND
			return;
		case 'J':
			/*
			 * clear screen
			 */
			state = S_NORM;
			ic.ax = 0x600;
			ic.bx = cur_attrib << 8;
			ic.cx = 0;
			ic.dx = (24<<8) | 79;
			ic.intval = 0x10;
			(void) doint_r(&ic);
			/*
			 * put cursor at upper left
			 */
			ic.ax = 0x200;
			ic.bx = 0;
			ic.dx = 0;
			ic.intval = 0x10;
			(void) doint_r(&ic);
			return;
		case 'H':
			/*
			 * x,y cursor positioning
			 */
			state = S_NORM;
			ic.ax = 0x200;
			ic.bx = 0;
			arg[0] -= 1;	/* convert to zero based */
			arg[1] -= 1;	/* convert to zero based */
			if (arg[0] < 0 || arg[0] > 24)
				arg[0] = 0;
			if (arg[1] < 0 || arg[1] > 79)
				arg[0] = 0;
			ic.dx = (arg[0]<<8) | arg[1];
			ic.intval = 0x10;
			(void) doint_r(&ic);
			return;
		case 'A':
			/*
			 * move cursor up arg[0] rows
			 */
			state = S_NORM;
			if (argnum == 0)
				arg[0] = 1;	/* no arguments means 1 row */
			else if (argnum > 1)
				return;		/* invalid sequence */
			ic.ax = 0x300;
			ic.bx = 0;
			ic.intval = 0x10;
			(void) doint_r(&ic);		/* get position */
			ic.ax = 0x200;
			ic.bx = 0;
			if (arg[0] > (ic.dx >> 8))
				arg[0] = (ic.dx >> 8);
			ic.dx -= (arg[0]<<8);
			ic.intval = 0x10;
			(void) doint_r(&ic);
			return;
		case 'B':
			/*
			 * move cursor down arg[0] rows
			 */
			state = S_NORM;
			if (argnum == 0)
				arg[0] = 1;	/* no arguments means 1 row */
			else if (argnum > 1)
				return;		/* invalid sequence */
			ic.ax = 0x300;
			ic.bx = 0;
			ic.intval = 0x10;
			(void) doint_r(&ic);		/* get position */
			ic.ax = 0x200;
			ic.bx = 0;
			if ((arg[0] + (ic.dx >> 8)) > 24)
				arg[0] = 24 - (ic.dx >> 8);
			ic.dx += (arg[0]<<8);
			ic.intval = 0x10;
			(void) doint_r(&ic);
			return;
		case 'C':
			/*
			 * move right arg[0] columns
			 */
			state = S_NORM;
			if (argnum == 0)
				arg[0] = 1;	/* no args means 1 column */
			else if (argnum > 1)
				return;		/* invalid sequence */
			ic.ax = 0x300;
			ic.bx = 0;
			ic.intval = 0x10;
			(void) doint_r(&ic);		/* get position */
			ic.ax = 0x200;
			ic.bx = 0;
			if (arg[0] + (ic.dx & 0xff) > 79)
				arg[0] = 79 - (ic.dx & 0xff);
			ic.dx += arg[0];
			ic.intval = 0x10;
			(void) doint_r(&ic);
			return;
		case 'D':
			/*
			 * move left 1 column
			 */
			if (argnum == 0)
				arg[0] = 1;	/* no args means 1 column */
			else if (argnum > 1)
				return;		/* invalid sequence */
			state = S_NORM;
			ic.ax = 0x300;
			ic.bx = 0;
			ic.intval = 0x10;
			(void) doint_r(&ic);		/* get position */
			ic.ax = 0x200;
			ic.bx = 0;
			if (arg[0] > (ic.dx & 0xff))
				arg[0] = (ic.dx & 0xff);
			ic.dx -= arg[0];
			ic.intval = 0x10;
			(void) doint_r(&ic);
			return;
		}
		state = S_NORM;
		break;
	}
	if (c != '\r' && c != '\n' && c != '\007' && c != '\b') {
		ic.ax = 0x0900 | c;
		ic.bx = (ushort)cur_attrib;
		ic.cx = 0x0001;
		ic.intval = 0x10;
		(void) doint_r(&ic);
	}
	if (scroll == SCROLL_ALLOW) {	/* simplistic for bootconf */
		ic.ax = 0x0e00 | c;
		ic.bx = 0x0000;
		ic.cx = 0x0001;
		ic.intval = 0x10;
		(void) doint_r(&ic);
	}
	p->out++;
}	

/*ARGSUSED*/
int
cons_avail(_char_io_p p)
{
	/*
	 * checking for input needs to be fast, so we call doint_asm
	 * directly supplying a dedicated real_regs buffer.
	 */
	static struct real_regs	*rr = 0;

	if (rr == 0)
		if ((rr = alloc_regs()) == 0)
			prom_panic("Cannot get real_regs for cons_avail");
	if (cons_more)
		return (1);
	AH(rr) = 0x11;
	(void) doint_asm(0x16, rr);
	return (rr->eflags & ZERO_FLAG ? 0 : 1);
}

/*ARGSUSED*/
void
cons_clear_screen(_char_io_p p)
{
	/*
	 * I don't believe the second level boot clears the screen.
	 * If so, we'll worry about that later.
	 */
}

/*ARGSUSED*/
void
cons_set_cursor(_char_io_p p, int row, int col)
{
	struct int_pb	ic;

	ic.ax = 0x0200;
	ic.bx = 0x0000;
	ic.dx = row << 8 | col;
	ic.intval = 0x10;
	(void) doint_r(&ic);
}

void
serial_init(char *name, int port, int port_vals, int port_flags)
{
	_char_io_p s;
	struct int_pb	ic;

	s = (_char_io_p)rm_malloc(sizeof (_char_io_t), 0, 0);
	if (s) {
		s->name = (char *)rm_malloc(strlen(name) + 1, 0, 0);
		if (s->name) {
			strcpy(s->name, name);
			s->cookie = (char *)port;
			s->flags = port_flags;
			s->putc = serial_putc;
			s->getc = serial_getc;
			s->avail = serial_avail;
			s->clear = serial_clear_screen;
			s->set = serial_set_cursor;
			s->next = (_char_io_p)0;
			if (console.next)
				s->next = console.next;
			console.next = s;
		} else {
			prom_panic("No space for new io-device name");
		}
	} else {
		prom_panic("No space for new io-device");
	}
	ic.ax = (ushort)port_vals;
	ic.dx = (ushort)port;
	ic.intval = 0x14;

	(void) doint_r(&ic);
}

int
serial_port_enabled(int port)
{
	_char_io_p p;

	for (p = console.next; p != 0; p = p->next) {
		if (p->cookie == (char *)port)
			if ((p->flags & CHARIO_OUT_ENABLE) &&
			    !(p->flags & CHARIO_IGNORE_ALL))
				return (1);
	}
	return (0);
}

void
serial_putc(_char_io_p p, char c)
{
	struct int_pb	ic;

	ic.ax = 0x0100 | c;
	ic.dx = (u_int)p->cookie;
	ic.intval = 0x14;
	(void) doint_r(&ic);

	if (ic.ax & 0x8000) {
		p->errs++;
		if ((ic.ax & 0xff00) == 0x8000) {
			p->flags |= CHARIO_DISABLED;
			/*
			 * XXX Should we can the input here as well? XXX
			 */
			p->flags &= ~CHARIO_OUT_ENABLE;
		}
	}
	p->out++;
}

char
serial_getc(_char_io_p p)
{
	struct int_pb	ic;

	ic.ax = 0x0200;
	ic.dx = (u_int)p->cookie;
	ic.intval = 0x14;
	(void) doint_r(&ic);

	if (!(ic.ax & 0x8000)) {
		p->in++;
		return (ic.ax & 0x7f);
	} else {
		p->in++;
		p->errs++;
		return (0);
	}
}

int
serial_avail(_char_io_p p)
{
	struct int_pb	ic;

	ic.ax = 0x0300;
	ic.dx = (u_int)p->cookie;
	ic.intval = 0x14;
	(void) doint_r(&ic);
	if ((ic.ax >> 8) & SERIAL_DATA)
		return (1);
	else
		return (0);
}

char serial_clear_str[] = "\033[;H\033[2J";

void
serial_clear_screen(_char_io_p p)
{
	char *cp;

	for (cp = &serial_clear_str[0]; *cp; cp++)
		/* ---- only put characters out if alowed to ---- */
		if ((p->flags & CHARIO_IGNORE_ALL) == 0)
			serial_putc(p, *cp);
}

void
serial_set_cursor(_char_io_p p, int row, int col)
{
	_char_io_p marker;

	/*
	 * Let the system do the work by marking everything but
	 * us to be ignored during the next output.
	 */
	for (marker = &console; marker; marker = marker->next)
		if (marker != p)
			marker->flags |= CHARIO_IGNORE_ALL;

	printf("\033[%d;%dH", row + 1, col + 1);

	/* ---- undo the previous steps ---- */
	for (marker = &console; marker; marker = marker->next)
		if (marker != p)
			marker->flags &= ~CHARIO_IGNORE_ALL;
}

int
toupper(int c)
{
	if (c >= 'a' && c <= 'z') c -= ('a' - 'A');
	return (c);
}

int
tolower(int c)
{
	if (c >= 'A' && c <= 'Z') c += ('a' - 'A');
	return (c);
}

#if defined(lint)
/*
 * These really get picked up as macros from ctypes.h
 */
int isalnum(int c) { return (c); }
int isdigit(int c) { return (c); }
int islower(int c) { return (c); }
int isprint(int c) { return (c); }
int isspace(int c) { return (c); }
int isupper(int c) { return (c); }
int isxdigit(int c) { return (c); }
int isascii(int c) { return (c); }
#endif

/*
 *  Routines for console popup window support.
 */
#define	POPWIN_ROWS	3
#define	POPWIN_COLS	80

#define	POPWIN_CHAR	0
#define	POPWIN_ATTR	1

#define	POPWIN_UL_ROW	12
#define	POPWIN_UL_COL	0

#define	BLK_ON_WHT	0x70

struct cursor_pos {
	int x;
	int y;
};

char underpopup[POPWIN_ROWS][POPWIN_COLS][2];

static void
inchar(char *attr, char *c)
{
	struct int_pb	ic;

	ic.intval = 0x10;
	ic.ax = 0x0800;
	ic.bx = 0x00;
	(void) doint_r(&ic);
	*c = ic.ax & 0xff;
	*attr = (ic.ax >> 8) & 0xff;
}

static void
outchar(char attr, char c)
{
	struct int_pb	ic;

	ic.intval = 0x10;
	ic.ax = 0x0900 | c;
	ic.bx = attr;
	ic.cx = 1;
	(void) doint_r(&ic);
}

static void
get_xy(struct cursor_pos *pos)
{
	struct int_pb	ic;

	ic.intval = 0x10;
	ic.ax = 0x0300;
	ic.bx = 0x0;
	(void) doint_r(&ic);
	pos->x = ic.dx & 0xff;
	pos->y = (ic.dx >> 8) & 0xff;
}

static void
set_xy(struct cursor_pos pos)
{
	struct int_pb	ic;

	ic.intval = 0x10;
	ic.ax = 0x0200;
	ic.bx = 0x0;
	ic.dx = ((pos.y << 8) & 0xff00) | (pos.x & 0xff);
	(void) doint_r(&ic);
}

static void
saveunderpopup()
{
	struct cursor_pos winstart;
	int row, col;

	for (row = 0; row < POPWIN_ROWS; row++) {
		for (col = 0; col < POPWIN_COLS; col++) {
			winstart.x = POPWIN_UL_COL + col;
			winstart.y = POPWIN_UL_ROW + row;
			set_xy(winstart);
			inchar(&underpopup[row][col][POPWIN_ATTR],
			    &underpopup[row][col][POPWIN_CHAR]);
		}
	}
}

static void
restoreunderpopup()
{
	struct cursor_pos winstart;
	int row, col;

	for (row = 0; row < POPWIN_ROWS; row++) {
		for (col = 0; col < POPWIN_COLS; col++) {
			winstart.x = POPWIN_UL_COL + col;
			winstart.y = POPWIN_UL_ROW + row;
			set_xy(winstart);
			outchar(underpopup[row][col][POPWIN_ATTR],
			    underpopup[row][col][0]);
		}
	}
}

static void
centerNdisplay(char *s, char attr, int row)
{
	struct cursor_pos prcursor;
	int len = strlen(s);
	int m = (POPWIN_COLS - len)/2;
	int cp;

	prcursor.x = 0;
	prcursor.y = row;

	for (cp = 0; cp < m; cp++) {
		set_xy(prcursor);
		outchar(attr, ' ');
		prcursor.x++;
	}
	for (cp = 0; cp < len; cp++) {
		set_xy(prcursor);
		outchar(attr, s[cp]);
		prcursor.x++;
	}
	for (cp = 0; cp < POPWIN_COLS-len-m; cp++) {
		set_xy(prcursor);
		outchar(attr, ' ');
		prcursor.x++;
	}
}

static char *askkey = "Press ENTER to Continue";

void
cons_popup(char *sl1, char *sl2)
{
	struct cursor_pos waitpos;

	waitpos.x = POPWIN_UL_COL;
	waitpos.y = POPWIN_UL_ROW;

	centerNdisplay(sl1, BLK_ON_WHT, POPWIN_UL_ROW);
	centerNdisplay(sl2, BLK_ON_WHT, POPWIN_UL_ROW + 1);
	centerNdisplay(askkey, BLK_ON_WHT, POPWIN_UL_ROW + 2);
	set_xy(waitpos);
}

/*
 *  popup_prompt -- Popup a window whose first two lines are the messages
 *	specified as args.  Last line will be "Press ENTER to Continue".
 */
void
popup_prompt(char *sl1, char *sl2)
{
	struct cursor_pos beforepos;
	_char_io_p p;

	p = &console;
	if ((p->flags & CHARIO_OUT_ENABLE) && !(p->flags & CHARIO_IGNORE_ALL)) {
		get_xy(&beforepos);
		saveunderpopup();
		cons_popup(sl1, sl2);
	}

	while ((p = p->next) != 0) {
		if ((p->flags & CHARIO_OUT_ENABLE) &&
		    !(p->flags & CHARIO_IGNORE_ALL)) {
			linesplat(p, sl1);
			linesplat(p, sl2);
			linesplat(p, askkey);
		}
	}

	waitEnter();

	p = &console;
	if ((p->flags & CHARIO_OUT_ENABLE) && !(p->flags & CHARIO_IGNORE_ALL)) {
		restoreunderpopup();
		set_xy(beforepos);
	}
}

/*
 *  Global debug on/off switching routines
 */
extern int int21debug, DOSfile_debug;
extern int ldmemdebug, loaddebug;
extern int RAMfile_debug;
extern int verbose_flag;

static int isave, dsave, l1save, l2save, rsave, vsave;

void
savedebugs(void)
{
	isave = int21debug;
	dsave = DOSfile_debug;
	l1save = ldmemdebug;
	l2save = loaddebug;
	rsave = RAMfile_debug;
	vsave = verbose_flag;
}

void
cleardebugs(void)
{
	int21debug = 0;
	DOSfile_debug = 0;
	ldmemdebug = 0;
	loaddebug = 0;
	RAMfile_debug = 0;
	verbose_flag = 0;
}

void
restoredebugs(void)
{
	int21debug = isave;
	DOSfile_debug = dsave;
	ldmemdebug = l1save;
	loaddebug = l2save;
	RAMfile_debug = rsave;
	verbose_flag = vsave;
}
