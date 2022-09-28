/*
 * Copyright (c) 1987-1996, Sun Microsystems, Inc.  All Rights Reserved.
 */

#ident "@(#)support_i386.c	1.12	96/06/18 SMI"

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
#include <sys/debugreg.h>
#ifdef	__STDC__
#include <stdarg.h>
#else	/* __STDC__ */
#include <varargs.h>
#endif	/* __STDC__ */
#define	addr_t	unsigned
#include "process.h"

extern int pagesize;
extern char *module_path;

struct bootops kadb_bootops;
struct bootops *bootops;

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
		switch (c) {
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

	tp->tv_sec = (1989 - 1970) * 365 * 24 * 60 * 60;	/* ~1989 */
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
	int pgincr;	/* #pages requested, rounded to next highest page */
	int nreq;	/* #bytes requested, rounded to next highest page */
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
		return (curbrk);

	pgincr = btoc(incr);
	if ((curbrk + ctob(pgincr)) < (caddr_t)(end)) {
		printf("sbrk:  lim %x + %x "
		    "attempting to free program space %x\n",
		    lim, ctob(pgincr), (u_int)end);
		errno = EINVAL;
		return ((caddr_t)-1);
	}

	if ((curbrk + ctob(pgincr)) <= lim) {	/* have enough mem avail */
		return (curbrk += incr);
	} else {			 /* beyond lim - more pages needed */
		nreq = (roundup(incr - (lim - curbrk), pagesize));
		if (prom_alloc(0, nreq, 0) == 0) {
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

dbregset_t  dr_registers;
extern uchar_t const dbreg_control_enable_to_bkpts_table[256];
extern int cur_cpuid; /* cpu currently running kadb */

/* Given a data breakpoint mask, compute next available breakpoint */
/* Pick from 3 to 0, allowing user processes to use 0 to 3 */

static int  dr_nextbkpt[16] = {
	3,  /* 0x0: 3 clr, 2 clr, 1 clr, 0 clr */
	3,  /* 0x1: 3 clr, 2 clr, 1 clr, 0 set */
	3,  /* 0x2: 3 clr, 2 clr, 1 set, 0 clr */
	3,  /* 0x3: 3 clr, 2 clr, 1 set, 0 set */
	3,  /* 0x4: 3 clr, 2 set, 1 clr, 0 clr */
	3,  /* 0x5: 3 clr, 2 set, 1 clr, 0 set */
	3,  /* 0x6: 3 clr, 2 set, 1 set, 0 clr */
	3,  /* 0x7: 3 clr, 2 set, 1 set, 0 set */
	2,  /* 0x8: 3 set, 2 clr, 1 clr, 0 clr */
	2,  /* 0x9: 3 set, 2 clr, 1 clr, 0 set */
	2,  /* 0xa: 3 set, 2 clr, 1 set, 0 clr */
	2,  /* 0xb: 3 set, 2 clr, 1 set, 0 set */
	1,  /* 0xc: 3 set, 2 set, 1 clr, 0 clr */
	1,  /* 0xd: 3 set, 2 set, 1 clr, 0 set */
	0,  /* 0xe: 3 set, 2 set, 1 set, 0 clr */
	-1, /* 0xf: 3 set, 2 set, 1 set, 0 set */
};

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
	register int rw;
	register int len;

	switch (request) {
	case PTRACE_TRACEME:	/* do nothing */
		break;

/* hardware breakpoints */
	case PTRACE_CLRBKPT:
		dr_registers.debugreg[0] = 0;
		dr_registers.debugreg[1] = 0;
		dr_registers.debugreg[2] = 0;
		dr_registers.debugreg[3] = 0;
		dr_registers.debugreg[DR_CONTROL] = 0;

		clear_kadb_debug_registers(cur_cpuid);
		break;

	case PTRACE_SETWR:
		i = dr_nextbkpt[dbreg_control_enable_to_bkpts_table
		    [(uchar_t)dr_registers.debugreg[DR_CONTROL]]];
		if (i < 0)
			error("hardware break point over flow on write");

		dr_registers.debugreg[DR_CONTROL] &= ~
		    DR_ENABLE0 << (i * DR_ENABLE_SIZE);
		dr_registers.debugreg[DR_CONTROL] &= ~
		    (DR_RW_READ | DR_LEN_MASK) <<
		    (DR_CONTROL_SHIFT + i * DR_CONTROL_SIZE);
		dr_registers.debugreg[i] = (int)addr;
		dr_registers.debugreg[DR_CONTROL] |=
		    (DR_RW_WRITE | ((data - 1) << 2)) <<
		    (DR_CONTROL_SHIFT + i * DR_CONTROL_SIZE);
		dr_registers.debugreg[DR_CONTROL] |=
		    DR_ENABLE0 << (i * DR_ENABLE_SIZE);

		set_kadb_debug_registers(cur_cpuid);
		break;

	case PTRACE_SETAC:
		i = dr_nextbkpt[dbreg_control_enable_to_bkpts_table
		    [(uchar_t)dr_registers.debugreg[DR_CONTROL]]];
		if (i < 0)
			error("hardware break point over flow on access");

		dr_registers.debugreg[DR_CONTROL] &= ~
		    DR_ENABLE0 << (i * DR_ENABLE_SIZE);
		dr_registers.debugreg[DR_CONTROL] &= ~
		    (DR_RW_READ | DR_LEN_MASK) <<
		    (DR_CONTROL_SHIFT + i * DR_CONTROL_SIZE);
		dr_registers.debugreg[i] = (int)addr;
		dr_registers.debugreg[DR_CONTROL] |=
		    (DR_RW_READ | ((data - 1) << 2)) <<
		    (DR_CONTROL_SHIFT + i * DR_CONTROL_SIZE);
		dr_registers.debugreg[DR_CONTROL] |=
		    DR_ENABLE0 << (i * DR_ENABLE_SIZE);

		set_kadb_debug_registers(cur_cpuid);
		break;

	case PTRACE_SETBPP:
		i = dr_nextbkpt[dbreg_control_enable_to_bkpts_table
		    [(uchar_t)dr_registers.debugreg[DR_CONTROL]]];
		if (i < 0)
			error("hardware break point over flow on instruction");

		dr_registers.debugreg[DR_CONTROL] &= ~
		    DR_ENABLE0 << (i * DR_ENABLE_SIZE);
		dr_registers.debugreg[DR_CONTROL] &= ~
		    (DR_RW_READ | DR_LEN_MASK) <<
		    (DR_CONTROL_SHIFT + i * DR_CONTROL_SIZE);
		dr_registers.debugreg[i] = (int)addr;
		dr_registers.debugreg[DR_CONTROL] |=
		    DR_RW_EXECUTE << (DR_CONTROL_SHIFT + i * DR_CONTROL_SIZE);
		dr_registers.debugreg[DR_CONTROL] |=
		    DR_ENABLE0 << (i * DR_ENABLE_SIZE);

		set_kadb_debug_registers(cur_cpuid);
		break;

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

#define	NULL 0

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
 * Property intercept routines for kadb.
 */
static int
kadb_getprop(struct bootops *bop, char *name, void *buf)
{
	if (strcmp("module-path", name) == 0) {
		(void) strcpy(buf, module_path);
		return (strlen(module_path) + 1);
	} else {
		return (BOP_GETPROP(bop->bsys_super, name, buf));
	}
}

static int
kadb_getproplen(struct bootops *bop, char *name)
{
	if (strcmp("module-path", name) == 0) {
		return (strlen(module_path) + 1);
	} else {
		return (BOP_GETPROPLEN(bop->bsys_super, name));
	}
}

void
init_bootops(bop)
	struct bootops *bop;
{
	bcopy((caddr_t)bop, (caddr_t)&kadb_bootops,
	    sizeof (struct bootops));

	kadb_bootops.bsys_super = bop;
	kadb_bootops.bsys_getprop = kadb_getprop;
	kadb_bootops.bsys_getproplen = kadb_getproplen;

	bootops = &kadb_bootops;
}
