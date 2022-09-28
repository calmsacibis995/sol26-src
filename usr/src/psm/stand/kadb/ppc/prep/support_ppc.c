/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)support_ppc.c	1.7	96/07/03 SMI"

/* from SunOS 4.1 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/user.h>
#include <time.h>
#include <tzfile.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/reg.h>
#include <sys/bootconf.h>
#include <sys/debug/debugger.h>
#include <sys/ptrace.h>
#include <sys/sysmacros.h>
#include <sys/obpdefs.h>
#ifdef	__STDC__
#include <stdarg.h>
#else	/* __STDC__ */
#include <varargs.h>
#endif	/* __STDC__ */
#define	addr_t unsigned
#include "process.h"

extern int pagesize;
extern char *module_path;

static int VOF_probe(void);
extern void install_level0_handlers(int VOF_present);

struct bootops kadb_bootops;
extern char target_bootname[];
extern char target_bootargs[];
extern char aline[];
extern char * module_path;

_exit()
{
	(void) prom_enter_mon();
}

int interrupted = 0;

getchar()
{
	register int c;
	c = prom_getchar();
	if (c == '\r')
		c = '\n';
	if (c == 0177 || c == '\b') {
		putchar('\b');
		putchar(' ');
		c = '\b';
	}
	putchar(c);
	return (c);
}

/*
 * Read a line into the given buffer and handles
 * erase (^H or DEL), kill (^U), and interrupt (^C) characters.
 * This routine ASSUMES a maximum input line size of LINEBUFSZ
 * to guard against overflow of the buffer from obnoxious users.
 * gets_p is same as gets but assumes there is a null terminated
 * primed string in buf
 */
gets_p(buf)
	char buf[];
{
	register char *lp = buf;
	register c;
	while (*lp) {
		putchar(*lp);
		lp++;
	}
	for (;;) {
		c = getchar() & 0177;
		switch(c) {
		case '[':
		case ']':
			if (lp != buf)
				goto defchar;
			*lp++ = c;
		case '\n':
		case '\r':
			*lp++ = '\0';
			return;
		case '\b':
			lp--;
			if (lp < buf)
				lp = buf;
			continue;
		case 'u'&037:			/* ^U */
			lp = buf;
			putchar('^');
			putchar('U');
			putchar('\n');
			continue;
		case 'c'&037:
			dointr(1);
			/*MAYBE REACHED*/
			/* fall through */
		default:
		defchar:
			if (lp < &buf[LINEBUFSZ-1]) {
				*lp++ = c;
			} else {
				putchar('\b');
				putchar(' ');
				putchar('\b');
			}
			break;
		}
	}
}

/*
 * Read a line into the given buffer and handles
 * erase (^H or DEL), kill (^U), and interrupt (^C) characters.
 * This routine ASSUMES a maximum input line size of LINEBUFSZ
 * to guard against overflow of the buffer from obnoxious users.
 */
gets(buf)
	char buf[];
{
	register char *lp = buf;
	register c;

	for (;;) {
		c = getchar() & 0177;
		switch (c)	{
		case '[':
		case ']':
			if (lp != buf)
				goto defchar;
			*lp++ = c;
		case '\n':
		case '\r':
			*lp++ = '\0';
			return;
		case '\b':
			lp--;
			if (lp < buf)
				lp = buf;
			continue;
		case 'u'&037:			/* ^U */
			lp = buf;
			putchar('^');
			putchar('U');
			putchar('\n');
			continue;
		case 'c'&037:
			dointr(1);
			/* MAYBE REACHED */
			/* fall through */
		default:
		defchar:
			if (lp < &buf[LINEBUFSZ-1]) {
				*lp++ = c;
			} else {
				putchar('\b');
				putchar(' ');
				putchar('\b');
			}
			break;
		}
	}
}

dointr(doit)
{

	putchar('^');
	putchar('C');
	interrupted = 1;
	if (abort_jmp && doit) {
		_longjmp(abort_jmp, 1);
		/*NOTREACHED*/
	}
}

/*
 * Check for ^C on input
 */
tryabort(doit)
{

	if (prom_mayget() == ('c' & 037)) {
		dointr(doit);
		/* MAYBE REACHED */
	}
}

/*
 * Implement pseudo ^S/^Q processing along w/ handling ^C
 * We need to strip off high order bits as monitor cannot
 * reliably figure out if the control key is depressed when
 * prom_mayget() is called in certain circumstances.
 * Unfortunately, this means that s/q will work as well
 * as ^S/^Q and c as well as ^C when this guy is called.
 */
trypause()
{
	register int c;

	c = prom_mayget() & 037;

	if (c == ('s' & 037)) {
		while ((c = prom_mayget() & 037) != ('q' & 037)) {
			if (c == ('c' & 037)) {
				dointr(1);
				/* MAYBE REACHED */
			}
		}
	} else if (c == ('c' & 037)) {
		dointr(1);
		/* MAYBE REACHED */
	}
}

/*
 * Scaled down version of C Library printf.
 */
/*VARARGS1*/
#ifdef	__STDC__
	printf(const char *fmt, ...)
#else	/* __STDC__ */
printf(fmt, va_alist)
	char *fmt;
	va_dcl
#endif	/* __STDC__ */
{
	va_list x1;

	tryabort(1);
#ifdef	__STDC__
	va_start(x1, fmt);
#else	/* __STDC__ */
	va_start(x1);
#endif	/* __STDC__ */
	prf(fmt, x1);
	va_end(x1);
}

prf(fmt, adx)
	register char *fmt;
	va_list adx;
{
	register int b, c;
	register char *s;

loop:
	while ((c = *fmt++) != '%') {
	if (c == '\0')
			return;
		if (c == '\\' && *fmt) {
			switch ((c = *fmt++)) {
				case 't':
					putchar(0x9);
					continue;
				case 'n':
					putchar('\n');
				case 'r':
					putchar('\r');
					continue;
				case 'b':
					putchar('\b');
					continue;
				default:
					break;
			}
		}
		putchar(c);
	}
again:
	c = *fmt++;
	switch (c) {

	case 'l':
		goto again;
	case 'x': case 'X':
		b = 16;
		goto number;
	case 'd': case 'D':
	case 'u':		/* what a joke */
		b = 10;
		goto number;
	case 'o': case 'O':
		b = 8;
number:
		printn(va_arg(adx, u_long), b);
		break;
	case 'c':
		b = va_arg(adx, int);
		putchar(b);
		break;
	case 's':
		s = va_arg(adx, char *);
		while (c = *s++)
			putchar(c);
		break;
	}
	goto loop;
}

/*
 * Printn prints a number n in base b.
 * We don't use recursion to avoid deep kernel stacks.
 */
printn(n, b)
	register u_long n;
	register short b;
{
	char prbuf[11];
	register char *cp;

	if (b == 10 && (int)n < 0) {
		putchar('-');
		n = (unsigned)(-(int)n);
	}
	cp = prbuf;
	do {
		*cp++ = "0123456789abcdef"[n%b];
		n /= b;
	} while (n);
	do
		putchar(*--cp);
	while (cp > prbuf);
}

/*
 * Print a character on console.
 */
putchar(c)
	int c;
{
	/* XXX - If this is really necessary, prom_putchar() should do it. */
	if (c == '\n')
		(void) prom_putchar('\r');

	(void) prom_putchar(c);
}

/*
 * Fake getpagesize() system call
 */
getpagesize()
{

	return (pagesize);
}

/*
 * Fake gettimeofday call
 * Needed for ctime - we are lazy and just
 * give a bogus approximate answer
 */
gettimeofday(tp, tzp)
	struct timeval *tp;
	struct timezone *tzp;
{

	tp->tv_sec = (1993 - 1970) * 365 * 24 * 60 * 60;	/* ~1993 */
	tzp->tz_minuteswest = 8 * 60;	/* PDT: California ueber alles */
	tzp->tz_dsttime = DST_USA;
}

int errno;

caddr_t lim;	/* current hard limit (high water) */
caddr_t curbrk;	/* current break value */

caddr_t
_sbrk(incr)
	int incr;
{
/* curbrk and lim are usually the same value; they will only differ */
/* in cases where memory has been freed. In other words, the difference */
/* between lim and curbrk is our own private pool of available memory. */
/* Use this up before calling _sbrk again! */

	extern char end[];
	int pgincr;		/* #pages requested, rounded to next highest page */
	int nreq;		/* #bytes requested, rounded to next highest page */
	caddr_t val;
	register int i;

	if (nobrk) {	/* safety indicator - prevents recursive sbrk's */
		printf("sbrk:  late call\n");
		errno = ENOMEM;
		return ((caddr_t)-1);
	}
	if (lim == 0) {			/* initial _sbrk call */
		lim = (caddr_t)roundup((u_int)end, pagesize);
		curbrk = lim;
	}

	if (incr == 0)
		return curbrk;

	pgincr = btoc(incr);
	if ((curbrk + ctob( pgincr )) < (caddr_t)( end )) {
		printf("sbrk:  lim %x + %x attempting to free program space %x\n",
		lim, ctob( pgincr ), (u_int)end );
		errno = EINVAL;
		return ((caddr_t)-1);
	}

if (curbrk != lim) {
prom_printf("\nWARNING\n");
prom_printf("kadb: _sbrk: curbrk(0x%x) != lim (0x%x)\n", curbrk, lim);
prom_printf("\n");
}
	if ( ( curbrk + ctob( pgincr ) ) <= lim ) {/* have enough mem avail */
		return ( curbrk += incr );
	}
	else {				 /* beyond lim - more pages needed */
		nreq = ( roundup ( incr - ( lim - curbrk ), pagesize ) );
		if ( prom_alloc ( curbrk, nreq, 0 ) == 0 ) {
			errno = ENOMEM;
			return ((caddr_t)-1);
		}
		pagesused += pgincr;
		lim += incr;
	}
	val = curbrk;
	curbrk += incr;
	return (val);
}

#define	PHYSOFF(p, o) \
	((physadr)(p)+((o)/sizeof (((physadr)0)->r[0])))

/* Used for hardware breakpoints below hardware breakpoints */
int	dr_address[5];
int	dr_used[5];
int	dr_dr7;

/*
 * Fake ptrace - ignores pid and signals
 * Otherwise it's about the same except the "child" never runs,
 * flags are just set here to control action elsewhere.
 */
ptrace(request, pid, addr, data, addr2)
	int request;
	char *addr, *addr2;
{
	int rv = 0;
	register int i, val;
	register int *p;

	switch (request) {
	case PTRACE_TRACEME:	/* do nothing */
		break;

#if 0
/* XXXPPC */
/* hardware breakpoints */
	case PTRACE_CLRBKPT:
		setdr7(0);
		dr_dr7 = dr7();
		for ( i=0; i<4; i++)
		{
			dr_used[i] = 0;
			dr_address[i] = 0;
		}
		break;

	case PTRACE_SETWR:
		for ( i=0; i<4; i++)
		{
			if (dr_used[i] == 0)
				break;
		}
		if (i == 4)
			error("hardware break point over flow on write");
		dr_address[i] = (int)addr;
		dr_used[i] = 1;
		dr_dr7 = dr7();
		switch (i)
		{
			case 0:
				setdr0(addr);
				setdr7((data<<18) | (01<<16) | 0x303| dr_dr7);
				dr_dr7 = dr7();
				break;
			case 1:
				setdr1(addr);
				setdr7((data<<22) | (01<<20) | 0x30c | dr_dr7);
				dr_dr7 = dr7();
				break;
			case 2:
				setdr2(addr);
				setdr7((data<<26) | (01<<24) | 0x330 | dr_dr7);
				dr_dr7 = dr7();
				break;
			case 3:
				setdr3(addr);
				setdr7((data<<30) | (01<<28) | 0x3c0 | dr_dr7);
				dr_dr7 = dr7();
				break;
		}
		break;

	case PTRACE_SETAC:
		for ( i=0; i<4; i++)
		{
			if (dr_used[i] == 0)
				break;
		}
		if (i == 4)
			error("hardware break point over flow on access");
		dr_address[i] = (int)addr;
		dr_used[i] = 1;
		dr_dr7 = dr7();
		switch (i)
		{
			case 0:
				setdr0(addr);
				setdr7((data<<18) | (03<<16) | 0x303| dr_dr7);
				dr_dr7 = dr7();
				break;
			case 1:
				setdr1(addr);
				setdr7((data<<22) | (03<<20) | 0x30c | dr_dr7);
				dr_dr7 = dr7();
				break;
			case 2:
				setdr2(addr);
				setdr7((data<<26) | (03<<24) | 0x330 | dr_dr7);
				dr_dr7 = dr7();
				break;
			case 3:
				setdr3(addr);
				setdr7((data<<30) | (03<<28) | 0x3c0 | dr_dr7);
				dr_dr7 = dr7();
				break;
		}
		break;

	case PTRACE_SETBPP:
		for ( i=0; i<4; i++)
		{
			if (dr_used[i] == 0)
				break;
		}
		dr_address[i] = (int)addr;
		dr_used[i] = 1;
		dr_dr7 = dr7();
		switch (i)
		{
			case 0:
				setdr0(addr);
				setdr7( 0x303| dr_dr7);
				dr_dr7 = dr7();
				break;

			case 1:
				setdr1(addr);
				setdr7( 0x30c | dr_dr7);
				dr_dr7 = dr7();
				break;

			case 2:
				setdr2(addr);
				setdr7( 0x330 | dr_dr7);
				dr_dr7 = dr7();
				break;

			case 3:
				setdr3(addr);
				setdr7( 0x3c0 | dr_dr7);
				dr_dr7 = dr7();
				break;

		}
		break;
#endif

	case PTRACE_PEEKTEXT:
	case PTRACE_PEEKDATA:
		rv = peekl(addr);
		break;

	case PTRACE_PEEKUSER:
		break;

	case PTRACE_POKEUSER:
		break;

	case PTRACE_POKETEXT:
		rv = poketext(addr, data);
		break;

	case PTRACE_POKEDATA:
		rv = pokel(addr, data);
		break;

	case PTRACE_SINGLESTEP:
		dotrace = 1;
		/* fall through to ... */
	case PTRACE_CONT:
		dorun = 1;
		if ((int)addr != 1)
			reg->r_pc = (int)addr;
		break;

	case PTRACE_SETREGS:
		rv = scopy(addr, (caddr_t)reg, sizeof (struct regs));
		break;

	case PTRACE_GETREGS:
		rv = scopy((caddr_t)reg, addr, sizeof (struct regs));
		break;

	case PTRACE_WRITETEXT:
	case PTRACE_WRITEDATA:
		rv = scopy(addr2, addr, data);
		break;

	case PTRACE_READTEXT:
	case PTRACE_READDATA:
		rv = scopy(addr, addr2, data);
		break;

	case PTRACE_KILL:
	case PTRACE_ATTACH:
	case PTRACE_DETACH:
	default:
		errno = EINVAL;
		rv = -1;
		break;
	}
	return (rv);
}

/*
 * Return the ptr in sp at which the character c appears;
 * NULL if not found
 */

#define	NULL	0

char *
index(sp, c)
	register char *sp, c;
{

	do {
		if (*sp == c)
			return (sp);
	} while (*sp++);
	return (NULL);
}

/*
 * Return the ptr in sp at which the character c last
 * appears; NULL if not found
 */

char *
rindex(sp, c)
	register char *sp, c;
{
	register char *r;

	r = NULL;
	do {
		if (*sp == c)
			r = sp;
	} while (*sp++);
	return (r);
}

/*
 * Property intercept routines for kadb, so that it can
 * tell unix it's real name, and it's real bootargs. We
 * also let it figure out our virtual start and end addresses
 * rather than hardcoding them somewhere nasty.
 */
static int
kadb_getprop(struct bootops *bop, char *name, void *buf)
{
	extern char	start[];
	u_int start_addr = (u_int)start;

	if (strcmp("whoami", name) == 0) {
		(void) strcpy(buf, aline);
	} else if (strcmp("boot-args", name) == 0) {
		(void) strcpy(buf, target_bootargs);
	} else if (strcmp("debugger-start", name) == 0) {
		bcopy(&start_addr, buf, sizeof (caddr_t));
	} else if (strcmp("module-path", name) == 0) {
		(void) strcpy(buf, module_path);
	} else
		return (BOP_GETPROP(bop->bsys_super, name, buf));
	return (0);
}

static int
kadb_getproplen(struct bootops *bop, char *name)
{
	if (strcmp("whoami", name) == 0) {
		return (strlen(aline) + 1);
	} else if (strcmp("boot-args", name) == 0) {
		return (strlen(target_bootargs) + 1);
	} else if (strcmp("debugger-start", name) == 0) {
		return (sizeof (void *));
	} else if (strcmp("module-path", name) == 0) {
		return (strlen(module_path) + 1);
	} else
		return (BOP_GETPROPLEN(bop->bsys_super, name));
}

/*
 * - init the cif handler
 * - install level 0 handlers
 * - init bootops
 * - fimximp()
 */
void
init_bootops(struct bootops *bop, void *p1275cookie)
{
	prom_init("kadb", p1275cookie);

	install_level0_handlers(VOF_probe());
	bcopy((caddr_t)bop, (caddr_t)&kadb_bootops,
	    sizeof (struct bootops));

	kadb_bootops.bsys_super = bop;
	kadb_bootops.bsys_getprop = kadb_getprop;
	kadb_bootops.bsys_getproplen = kadb_getproplen;

	bootops = &kadb_bootops;

	(void) fiximp();
}

#define	VOF_STR	"SunSoft's Virtual Open Firmware"
#define	VOF_STRLEN	31
/*
 * are we booted from a VOF machine?
 */
static int
VOF_probe(void)
{
	dnode_t nodeid;
	char buf[VOF_STRLEN + 1];
	int proplen;

	nodeid = prom_finddevice("/openprom");
	if (nodeid == (dnode_t)-1) {
		return (0);
	}

	proplen = prom_bounded_getprop(nodeid, "model", buf, VOF_STRLEN);
	if (proplen <= 0) {
		return (0);
	}

	if (strncmp(VOF_STR, buf, VOF_STRLEN) == 0) {
		return (1);
	} else {
		return (0);
	}
}
