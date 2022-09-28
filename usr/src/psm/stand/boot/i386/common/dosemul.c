/*
 * Copyright (c) 1994-1996, Sun Microsystems, Inc.  All Rights Reserved.
 */

#ident "@(#)dosemul.c	1.34	96/05/13 SMI"

#include <sys/bsh.h>
#include <sys/types.h>
#include <sys/ramfile.h>
#include <sys/doserr.h>
#include <sys/dosemul.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/booti386.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/bootvfs.h>
#include <sys/bootconf.h>
#include <sys/salib.h>
#include <sys/promif.h>

#define	EQ(a, b) (strcmp(a, b) == 0)

extern struct real_regs	*alloc_regs(void);
extern void free_regs(struct real_regs *);
extern int boldgetproplen(struct bootops *bop, char *name);
extern int boldgetprop(struct bootops *bop, char *name, void *value);
extern int boldsetprop(struct bootops *bop, char *name, char *value);
extern char *boldnextprop(struct bootops *bop, char *prevprop);
extern int bgetproplen(struct bootops *, char *, phandle_t);
extern int bgetprop(struct bootops *, char *, caddr_t, int, phandle_t);
extern int bnextprop(struct bootops *, char *, char *, phandle_t);
extern int dosCreate(char *fn, int mode);
extern int dosRename(char *nam, char *t);
extern int dosUnlink(char *nam);
extern int ischar();
extern int getchar();
extern void putchar();
extern int read();
extern long lseek();
extern caddr_t rm_malloc(u_int, u_int, caddr_t);
extern int gets();
extern int serial_port_enabled(int port);
extern long strtol(char *, char **, int);
extern void prom_rtc_date();
extern void prom_rtc_time();
extern int doint_asm();
extern int boot_pcfs_close();
extern int boot_compfs_writecheck();
extern int boot_pcfs_open();
extern int boot_pcfs_write();
extern int volume_specified();

extern	struct	bootops *bop;
extern	struct	int_pb ic;
extern	rffd_t	*DOSsnarf_fp;
extern	ushort	RAMfile_doserr;

/* Set to "(handle != 1)" disable debug info for writes to stdout */
#define	SHOW_STDOUT_WRITES 1

int int21debug = 0;

/*
 * Definitions and globals for handling files created or accessed from
 * real mode modules. The first five fd's aren't accessible normally
 * because they are reserved for STDIN, STDOUT, etc.  I've left room
 * for them in the global array so that manipulation or re-use of the
 * first few fd's could be implemented if desired in the future.
 */
static dffd_t DOSfilefds[DOSfile_MAXFDS];

int	DOSfile_debug = 0;
ushort	DOSfile_doserr;

/*
 *  doint -- Wrapper that calls the new re-entrant doint() but maintains
 *		the previous global structure 'ic' interface.
 */
int
doint(void)
{
	struct real_regs *rr;
	struct real_regs *low_regs = (struct real_regs *)0;
	struct real_regs local_regs;
	int rv;
	extern ulong cursp;
	extern void getesp();

	/*
	 * Any pointer we provide to the re-entrant version of doint
	 * must be accessible from real-mode.  Therefore, if we are
	 * running on a kernel stack, we should alloc some low memory
	 * for our registers pointer, otherwise local stack storage
	 * is okay.
	 *
	 * Note that we can't just always alloc registers because we
	 * start doing doints VERY early on, before the memory allocator
	 * has even been set up!!!
	 */
	getesp();
	if (cursp > TOP_RMMEM) {
		if (!(low_regs = alloc_regs()))
			prom_panic("No low memory for a doint");
		rr = low_regs;
	} else {
		rr = &local_regs;
		bzero((char *)rr, sizeof (struct real_regs));
	}

	AX(rr) = ic.ax;
	BX(rr) = ic.bx;
	CX(rr) = ic.cx;
	DX(rr) = ic.dx;
	BP(rr) = ic.bp;
	SI(rr) = ic.si;
	DI(rr) = ic.di;
	rr->es = ic.es;
	rr->ds = ic.ds;

	rv = doint_asm(ic.intval, rr);

	ic.ax = AX(rr);
	ic.bx = BX(rr);
	ic.cx = CX(rr);
	ic.dx = DX(rr);
	ic.bp = BP(rr);
	ic.si = SI(rr);
	ic.di = DI(rr);
	ic.es = rr->es;
	ic.ds = rr->ds;

	if (low_regs)
		free_regs(low_regs);

	return (rv);
}

/*
 * doint_r - doint reentrant. transition tools so callers keep the same
 * basic model. They just supply a pointer to the ic structure which
 * should now be a stack copy.
 */
int
doint_r(struct int_pb *sic)
{
	struct real_regs *rr;
	struct real_regs *low_regs = (struct real_regs *)0;
	struct real_regs local_regs;
	int rv;
	extern ulong cursp;
	extern void getesp();

	/*
	 * Any pointer we provide to the re-entrant version of doint
	 * must be accessible from real-mode.  Therefore, if we are
	 * running on a kernel stack, we should alloc some low memory
	 * for our registers pointer, otherwise local stack storage
	 * is okay.
	 *
	 * Note that we can't just always alloc registers because we
	 * start doing doints VERY early on, before the memory allocator
	 * has even been set up!!!
	 */
	getesp();
	if (cursp > TOP_RMMEM) {
		if (!(low_regs = alloc_regs()))
			prom_panic("No low memory for a doint");
		rr = low_regs;
	} else {
		rr = &local_regs;
		bzero((char *)rr, sizeof (struct real_regs));
	}

	AX(rr) = sic->ax;
	BX(rr) = sic->bx;
	CX(rr) = sic->cx;
	DX(rr) = sic->dx;
	BP(rr) = sic->bp;
	SI(rr) = sic->si;
	DI(rr) = sic->di;
	rr->es = sic->es;
	rr->ds = sic->ds;

	rv = doint_asm(sic->intval, rr);

	sic->ax = AX(rr);
	sic->bx = BX(rr);
	sic->cx = CX(rr);
	sic->dx = DX(rr);
	sic->bp = BP(rr);
	sic->si = SI(rr);
	sic->di = DI(rr);
	sic->es = rr->es;
	sic->ds = rr->ds;

	if (low_regs)
		free_regs(low_regs);

	return (rv);
}

void
dosemul_init(void)
{
	extern dffd_t DOSfilefds[];

	if (int21debug)
		printf("Setting up int21 bootops.\n");

	hook21();

	/*
	 *  Mark STD file descriptors as in use.
	 */
	DOSfilefds[DOS_STDIN].flags  = DOSFD_INUSE | DOSFD_STDDEV;
	DOSfilefds[DOS_STDOUT].flags = DOSFD_INUSE | DOSFD_STDDEV;
	DOSfilefds[DOS_STDERR].flags = DOSFD_INUSE | DOSFD_STDDEV;
	DOSfilefds[DOS_STDAUX].flags = DOSFD_INUSE | DOSFD_STDDEV;
	DOSfilefds[DOS_STDPRN].flags = DOSFD_INUSE | DOSFD_STDDEV;
}

void
hook21(void)
{
	extern void	int21chute();
	extern ulong	old21vec;

	static short	dosemulon = 0;
	ushort	chuteoff;
	ushort	chuteseg;

	chuteoff = (ulong)int21chute%0x10000;
	chuteseg = (ulong)int21chute/0x10000;

	if (!dosemulon) {
		get_dosivec(0x21, ((ushort *)&old21vec)+1,
		    (ushort *)&old21vec);
		set_dosivec(0x21, chuteseg, chuteoff);
		dosemulon = 1;
	}

}

void
get_dosivec(int vector, ushort *vecseg, ushort *vecoff)
{
	u_short	*vecaddr;

	if (int21debug)
		printf("G-%x", vector);
	vecaddr = (u_short *)(vector*DOSVECTLEN);
	*vecseg = peeks(vecaddr+1);
	*vecoff = peeks(vecaddr);
}

void
set_dosivec(int vector, ushort newseg, ushort newoff)
{
	u_short	*vecaddr;

	if (int21debug)
		printf("S-%x", vector);
	vecaddr = (u_short *)(vector*DOSVECTLEN);
	pokes(vecaddr+1, newseg);
	pokes(vecaddr, newoff);
}

/*
 * We have a set of routines for dealing with file descriptor
 * allocations (to real mode modules).  These all have names
 * of the form DOSfile_xxxx.
 */
int
DOSfile_allocfd(void)
{
	extern dffd_t DOSfilefds[];
	int fdc;

	if (DOSfile_debug || int21debug)
		printf("DOSfile_allocfd ");

	/* Skip over in-use fd's */
	for (fdc = 0; (fdc < DOSfile_MAXFDS &&
	    (DOSfilefds[fdc].flags & DOSFD_INUSE)); fdc++);

	if (fdc == DOSfile_MAXFDS) {
		if (DOSfile_debug || int21debug)
			printf("NOFDS.\n");
		DOSfile_doserr = DOSERR_NOMOREFILES;
		return (DOSfile_ERROR);
	} else {
		if (DOSfile_debug || int21debug)
			printf("success (%d).\n", fdc);
		DOSfilefds[fdc].flags |= DOSFD_INUSE;
		return (fdc);
	}
}

int
DOSfile_checkfd(int fd)
{
	return (fd >= 0 && fd < DOSfile_MAXFDS &&
	    (DOSfilefds[fd].flags & DOSFD_INUSE));
}

int
DOSfile_freefd(int fd)
{
	extern dffd_t DOSfilefds[];

	if (DOSfile_debug || int21debug)
		printf("DOSfile_freefd:%d:", fd);

	/*
	 * For now, disallow closing of the STD descriptors.
	 */
	if (fd < DOSfile_MINFD || !DOSfile_checkfd(fd)) {
		if (DOSfile_debug || int21debug)
			printf("Bad handle to free?\n");
		DOSfile_doserr = DOSERR_INVALIDHANDLE;
		return (DOSfile_ERROR);
	}
	DOSfilefds[fd].flags = 0;
	return (DOSfile_OK);
}

static int
DOSfile_closefd(int fd)
{
	extern dffd_t DOSfilefds[];
	dffd_t *cfd;
	int rv = -1;

	if (DOSfile_debug || int21debug)
		printf("DOSfile_closefd:%d:", fd);

	if (fd < DOSfile_MINFD || !DOSfile_checkfd(fd)) {
		DOSfile_doserr = DOSERR_INVALIDHANDLE;
		if (DOSfile_debug || int21debug)
			printf("Out of range?  ");
	} else {
		cfd = &(DOSfilefds[fd]);
		if (cfd->flags & DOSFD_RAMFILE) {
			if (RAMfile_close(cfd->rfp) == RAMfile_ERROR) {
				if (DOSfile_debug || int21debug)
					printf("RAMfile close failure  ");
				DOSfile_doserr = DOSERR_INVALIDHANDLE;
			} else
				rv = 0;
		} else if (cfd->flags & DOSFD_DSKWRT) {
			if (boot_pcfs_close(cfd->actualfd)) {
				DOSfile_doserr = DOSERR_INVALIDHANDLE;
			} else {
				rv = 0;
			}
		} else if (close(cfd->actualfd) < 0) {
			DOSfile_doserr = DOSERR_INVALIDHANDLE;
			if (DOSfile_debug || int21debug)
				printf("fs close failure  ");
		} else
			rv = 0;
	}

	if (rv >= 0)
		(void) DOSfile_freefd(fd);
	return (rv);
}

void
DOSfile_closeall(void)
{
	int f;

	if (DOSfile_debug || int21debug)
		printf("DOSfile_closeall:");

	for (f = DOSfile_MINFD; f <= DOSfile_LASTFD; f++)
		if (DOSfilefds[f].flags & DOSFD_INUSE)
			(void) DOSfile_closefd(f);
}

/*
 * dos_gets
 *
 *	Yet another gets routine.  This one handles the new line,
 *	carriage routine in the way that DOS does.
 */
int
dos_gets(char *str, int n)
{
	int 	c;
	int	t;
	char	*p;

	p = str;
	c = 0;

	while ((t = getchar()) != '\r') {
		putchar(t);
		if (t == '\n')
			continue;
		if (t == '\b') {
			if (c) {
				printf(" \b");
				c--; p--;
			} else
				putchar(' ');
			continue;
		}
		if (c < n - 2) {
			*p++ = t;
			c++;
		}
	}
	putchar('\n');
	putchar('\r');
	*p++ = '\n'; c++;
	*p = '\0';

	return (c);
}

/*
 * dos_formpath
 *
 *	Access to files via dos modules is effectively
 *	subjected to a chroot() because we always prepend the
 *	'boottree' property to any paths accessed by the module.
 *
 *	Note that when dealing with RAMfiles we don't follow through
 *	with prepending the name.  This is because the delayed write
 *	mechanism will automatically put the file under the same
 *	'boottree' root	path we are prepending; hence we don't need
 *	to waste space prepending it to the RAMfile name.
 */
char *
dos_formpath(char *fn)
{
	static char DOSpathbuf[MAXPATHLEN];
	int pplen, tlen;
	int addslash = 0;

	DOSpathbuf[0] = '\0';

	if (!volume_specified(fn)) {
		addslash = (*fn != '/');
		/*
		 * DOS module file access is rooted at whatever path is
		 * defined in the 'boottree' property.
		 */
		pplen = bgetproplen(bop, "boottree", 0);
		tlen = pplen + addslash + strlen(fn);

		if (tlen >= MAXPATHLEN) {
			printf("Maximum path length exceeded.\n");
			DOSfile_doserr = DOSERR_FILENOTFOUND;
			return (NULL);
		} else {
			if (pplen >= 0)
				(void) bgetprop(bop, "boottree",
					DOSpathbuf, pplen, 0);
			if (addslash)
				(void) strcat(DOSpathbuf, "/");
			(void) strcat(DOSpathbuf, fn);
		}
	} else {
		(void) strcpy(DOSpathbuf, fn);
	}

	DOSpathbuf[MAXPATHLEN-1] = '\0';
	return (DOSpathbuf);
}

char *
dosfn_to_unixfn(char *fn)
{
	static char newname[MAXPATHLEN];
	char *ptr;

	/* copy the string converting upper case to lower case and \ to / */
	for (ptr = newname; ptr < &newname[MAXPATHLEN]; ptr++, fn++)
		if ((*fn >= 'A') && (*fn <= 'Z'))
			*ptr = *fn - 'A' + 'a';
		else if (*fn == '\\')
			*ptr = '/';
		else if ((*ptr = *fn) == '\0')
			break;

	newname[MAXPATHLEN - 1] = '\0';
	if (int21debug)
		printf("(new fname \"%s\") ", newname);
	return (newname);
}

/*
 * All the dos function handling routines have names of the form dosxxxx.
 */
void
dosparsefn(struct real_regs *rp)
{
	char *fn;

	fn = DS_SI(rp);

	if (int21debug)
		printf("Parse fn \"%s\"", fn);

	/* XXX for now... */
	AL(rp) = 0;
}

void
doscreatefile(struct real_regs *rp)
{
	char *fn, *afn, *ufn, *mapdos;
	int fd;
	dffd_t *afd;
	ulong attr;

	fn = DS_DX(rp);
	ufn = dosfn_to_unixfn(fn);
	afn = dos_formpath(ufn);
	attr = (ulong)CX(rp);

	if (int21debug)
		printf("Create \"%s\" ", fn);

	/*
	 *  Allocate a file descriptor.
	 */
	if ((fd = DOSfile_allocfd()) == DOSfile_ERROR) {
		if (int21debug)
			printf("(Failed desc alloc)");
		AX(rp) = DOSfile_doserr;
		SET_CARRY(rp);
		return;
	}
	afd = &(DOSfilefds[fd]);

	/*
	 * Check for the special files allowing real-mode
	 * modules access to the boot interpreter.  Writes
	 * and reads to these special files will be handled
	 * as a special case.
	 */
	if (EQ(ufn, DOSBOOTOPC_FN))
		afd->flags |= DOSFD_BOOTOPC;

	if (EQ(ufn, DOSBOOTOPR_FN))
		afd->flags |= DOSFD_BOOTOPR;

	/*
	 *[]------------------------------------------------------------[]
	 * | only try dosCreate if we're not talking about		|
	 * | dtree's special files and we have a hook though compfs to	|
	 * | the dos file system below.					|
	 *[]------------------------------------------------------------[]
	 */
	if (((afd->flags & (DOSFD_BOOTOPR|DOSFD_BOOTOPC)) == 0) && afn &&
	    boot_compfs_writecheck(afn, &mapdos)) {
		/* ---- ufs can't write a file so call directly to dos ---- */
		if ((afd->actualfd = dosCreate(mapdos, attr)) == -1) {
			(void) DOSfile_freefd(fd);
			AX(rp) = DOSERR_FILENOTFOUND;
			SET_CARRY(rp);
			if (int21debug) printf("(dosCreate failed)");
		} else {
			AX(rp) = (ushort)fd;
			CLEAR_CARRY(rp);
			afd->flags |= DOSFD_DSKWRT;
			if (int21debug) printf("(dosCreate success)");
		}
		return;
	}

	if ((afd->rfp = RAMfile_create(ufn, attr)) == (rffd_t *)NULL) {
		/*
		 * Seek to beginning of file.
		 */
		if (int21debug)
			printf("(RAMcreate failed)");
		RAMrewind(afd->rfp);
		(void) DOSfile_freefd(fd);
		AX(rp) = RAMfile_doserr;
		SET_CARRY(rp);
	} else {
		if (int21debug)
			printf("(success)");
		afd->flags |= DOSFD_RAMFILE;
		AX(rp) = (ushort)fd;
		CLEAR_CARRY(rp);
	}
}

void
dosopenfile(struct real_regs *rp)
{
	extern dffd_t DOSfilefds[];
	char *fn, *ufn, *afn, *mapdos;
	int fd;
	dffd_t *afd;
	ulong mode;

	fn = DS_DX(rp);
	ufn = dosfn_to_unixfn(fn);
	mode = (ulong)AL(rp);

	if (int21debug)
		printf("Open \"%s\", %x ", fn, mode);

	/*
	 *  Allocate a file descriptor.
	 */
	if ((fd = DOSfile_allocfd()) == DOSfile_ERROR) {
		if (int21debug)
			printf("(Failed desc alloc)");
		AX(rp) = DOSfile_doserr;
		SET_CARRY(rp);
		return;
	}
	afd = &(DOSfilefds[fd]);

	/*
	 * Check list of RAMfiles first.
	 */
	if ((afd->rfp = RAMfile_open(ufn, mode)) != (rffd_t *)NULL) {
		if (int21debug)
			printf("(handle %x)", fd);
		/*
		 * Seek to beginning of file.
		 */
		RAMrewind(afd->rfp);
		/*
		 * Check for the special files allowing real-mode
		 * modules access to the boot interpreter.  Writes
		 * and reads to these special files will be handled
		 * as a special case.
		 */
		if (EQ(ufn, DOSBOOTOPC_FN))
			afd->flags |= DOSFD_BOOTOPC;

		if (EQ(ufn, DOSBOOTOPR_FN))
			afd->flags |= DOSFD_BOOTOPR;

		afd->flags |= DOSFD_RAMFILE;
		AX(rp) = (ushort)fd;
		CLEAR_CARRY(rp);
	} else if (EQ(ufn, DOSBOOTOPR_FN)) {

		afd->rfp = DOSsnarf_fp = RAMfile_create(ufn, O_RDONLY);
		if (!afd->rfp) {
			if (int21debug)
				printf("(SNARFfile create failed)");
			AX(rp) = RAMfile_doserr;
			SET_CARRY(rp);
		} else {
			if (int21debug)
				printf("(SNARFhandle %x)", fd);
			afd->flags |= (DOSFD_BOOTOPR | DOSFD_RAMFILE);
			AX(rp) = (ushort)fd;
			CLEAR_CARRY(rp);
		}
	} else {
		/*
		 * Is not a RAMfile, nor the bootops results file.
		 * Still could be the bootops call file, in which case
		 * we should fail the open.  Otherwise its a normal
		 * file access they seek.  If they are attempting
		 * read-only access we can just open it.  But if they
		 * want to write it as well, we need to convert it
		 * to a RAMfile now.
		 */
		int success;

		if (strcmp(ufn, DOSBOOTOPC_FN) == 0)
			success = 0;
		else if ((long)mode > DOSACCESS_RDONLY) {
			afn = dos_formpath(ufn);
			/* ---- open real file if we can for writing ---- */
			if (afn && boot_compfs_writecheck(afn, &mapdos)) {
				afd->actualfd = boot_pcfs_open(mapdos,
							(int)mode);
				afd->flags |= DOSFD_DSKWRT;
				success = afd->actualfd != -1;
			} else {
				afd->rfp = RAMcvtfile(ufn, mode);
				afd->flags |= DOSFD_RAMFILE;
				success = (afd->rfp != (rffd_t *)NULL);
			}
		} else {
			if (afn = dos_formpath(ufn)) {
				afd->actualfd = open(afn, O_RDONLY);
				afd->flags &= ~DOSFD_RAMFILE;
				success = afd->actualfd != -1;
			} else {
				success = 0;
			}
		}

		if (!success) {
			/*
			 * Free up descriptor, then set carry flag
			 * and ax to indicate failure.
			 */
			if (int21debug)
				printf("(file not found)");
			(void) DOSfile_freefd(fd);
			AX(rp) = DOSERR_FILENOTFOUND;
			SET_CARRY(rp);
		} else {
			if (int21debug)
				printf("(handle %x)", fd);
			AX(rp) = (ushort)fd;
			CLEAR_CARRY(rp);
		}
	}
}

void
dosclosefile(struct real_regs *rp)
{
	if (int21debug)
		printf("Close %x", BX(rp));

	if (DOSfile_closefd(BX(rp)) < 0) {
		if (int21debug)
			printf("(failed)");
		AX(rp) = DOSfile_doserr;
		SET_CARRY(rp);
	} else {
		if (int21debug)
			printf("(succeeded)");
		CLEAR_CARRY(rp);
	}
}

void
dosgetchar(struct real_regs *rp)
{
	if (int21debug)
		printf("GETC, NOECHO:");

	AL(rp) = getchar();

	if (int21debug)
		printf("Return %x ", AL(rp));
}

void
dosreadfile(struct real_regs *rp)
{
	extern dffd_t DOSfilefds[];
	extern int boot_pcfs_read(int fd, char *buf, int len);
	dffd_t *cfd;
	char *dstbuf;
	int nbytes;
	int handle;
	int cc;
	int (*handler)(int fd, char *buf, int len);

	dstbuf = DS_DX(rp);
	nbytes = CX(rp);
	handle = BX(rp);

	if (int21debug)
		printf("Read(%x, %x, %x) ", handle, dstbuf, nbytes);

	if (!DOSfile_checkfd(handle)) {
		if (int21debug)
			printf("(bad handle)");
		AX(rp) = DOSERR_INVALIDHANDLE;
		SET_CARRY(rp);
		return;
	}
	cfd = &(DOSfilefds[handle]);

	if (cfd->flags & DOSFD_RAMFILE) {
		if ((cc = RAMfile_read(cfd->rfp, dstbuf, nbytes)) < 0) {
			/* Set carry flag and fill ax to indicate failure */
			if (int21debug)
				printf("(RAMfile error)");
			AX(rp) = RAMfile_doserr;
			SET_CARRY(rp);
		} else {
			/* Clear carry flag and fill ax to indicate success */
			if (int21debug)
				printf("(read %x bytes)", cc);
			AX(rp) = (ushort)cc;
			CLEAR_CARRY(rp);
		}
		return;
	} else if (handle == DOS_STDIN) {
		cc = AX(rp) = dos_gets(dstbuf, nbytes);
		if (int21debug)
			printf("(gets read %d bytes)", cc);
		CLEAR_CARRY(rp);
		return;
	}

	/* if we're writing to pcfs, we call the pcfs code directly */
	if (cfd->flags & DOSFD_DSKWRT)
		handler = boot_pcfs_read;
	else
		handler = read;
	if ((cc = (*handler)(cfd->actualfd, dstbuf, nbytes)) < 0) {
		/*
		 * Set carry flag and ax to indicate failure.
		 * XXX fix this, should be:
		 * rp->eax.eax = unix_to_dos_errno(errno);
		 */
		if (int21debug)
			printf("(fs error)");
		AX(rp) = DOSERR_INVALIDHANDLE;
		SET_CARRY(rp);
	} else {
		/* Set carry flag and ax to indicate success */
		if (int21debug)
			printf("(read %x bytes)", cc);
		AX(rp) = (ushort)cc;
		CLEAR_CARRY(rp);
	}
}

void
doswritefile(struct real_regs *rp)
{
	extern dffd_t DOSfilefds[];
	dffd_t *cfd;
	char *srcbuf;
	int nbytes;
	int handle;
	int cc;

	srcbuf = DS_DX(rp);
	nbytes = CX(rp);
	handle = BX(rp);

	if (int21debug && SHOW_STDOUT_WRITES)
		printf("Write(%x, %x, %x) ", handle, srcbuf, nbytes);

	if (!DOSfile_checkfd(handle)) {
		if (int21debug)
			printf("(bad handle)");
		AX(rp) = DOSERR_INVALIDHANDLE;
		SET_CARRY(rp);
		return;
	}
	cfd = &(DOSfilefds[handle]);

	if (cfd->flags & DOSFD_RAMFILE) {

		/*
		 * A 0 byte write is DOS' method of telling us it wants to
		 * truncate the existing file at the current offset.
		 */
		if (nbytes == 0 && (cc = RAMfile_trunc_atoff(cfd->rfp)) < 0) {
			if (int21debug)
				printf("(RAMfile trunc error)");
			AX(rp) = RAMfile_doserr;
			SET_CARRY(rp);
		} else if ((cc = RAMfile_write(cfd->rfp, srcbuf, nbytes)) < 0) {
			/* Set carry flag and fill ax to indicate failure */
			if (int21debug)
				printf("(RAMfile error)");
			AX(rp) = RAMfile_doserr;
			SET_CARRY(rp);
		} else {
			/*
			 * If this is the bootops interface file, direct
			 * this to boot shell engine for interpretation.
			 */
			if (cfd->flags & DOSFD_BOOTOPC)
				dosbootop(cfd, srcbuf, nbytes);

			/* Clear carry flag and fill ax to indicate success */
			if (int21debug)
				printf("(wrote %x bytes)", cc);
			AX(rp) = (ushort)cc;
			CLEAR_CARRY(rp);
		}
	} else if (((handle == DOS_STDOUT) || (handle == DOS_STDERR)) &&
	    (cfd->flags & DOSFD_STDDEV)) {
		while (nbytes-- > 0)
			putchar(*srcbuf++);
		/* Set carry flag and ax to indicate success */
		AX(rp) = CX(rp);
		CLEAR_CARRY(rp);
	} else {
		if (cfd->flags & DOSFD_DSKWRT) {
			if ((cc =
			    boot_pcfs_write(cfd->actualfd, srcbuf,
					    (u_int)nbytes)) < 0) {
				if (int21debug)	printf("(dos disk error)");
				AX(rp) = DOSERR_INSUFFICIENT_MEMORY;
				SET_CARRY(rp);
			} else {
				if (int21debug)	printf("(wrote %x bytes)", cc);
				AX(rp) = (ushort)cc;
				CLEAR_CARRY(rp);
			}
		} else {
			if (int21debug)
				printf("Write req denied\n");
			/*
			 * Set carry flag and ax to indicate failure.
			 * XXX fix this, should be:
			 * rp->eax.eax = unix_to_dos_errno(errno);
			 */
			AX(rp) = DOSERR_INVALIDHANDLE;
			SET_CARRY(rp);
		}
	}
}

void
dosrenamefile(struct real_regs *rp)
{
	char	*fn,		/* "from" file name munged */
		*mapfn,		/* "from" file name munged and mapped */
		*tn,		/* "to" file name munged */
		*maptn;		/* "to" file name munged and mapped */
	ushort	ax;		/* holds error codes */

	/*
	 * This intermediate step of coping the data in "mapfn"
	 * to a malloc'd string is required because both dosfn_to_unixfn()
	 * and dos_formpath() return pointers to static buffers.
	 * This also happens below with boot_compfs_writecheck().
	 */

	fn = (char *)strcpy((char *)bkmem_alloc(MAXPATHLEN),
		    dos_formpath(dosfn_to_unixfn(DS_DX(rp))));
	tn = dos_formpath(dosfn_to_unixfn(ES_DI(rp)));

	printf("(fn %s:tn %s)", fn, tn);
	if (fn && tn && boot_compfs_writecheck(fn, &mapfn)) {
		/*
		 * See comment above as to why we're malloc'ing space
		 * here.
		 */

		mapfn = (char *)strcpy((char *)bkmem_alloc(MAXPATHLEN), mapfn);
		if (boot_compfs_writecheck(tn, &maptn)) {
			ax = dosRename(mapfn, maptn);

		} else {
			ax = DOSERR_ACCESSDENIED;

		}
		bkmem_free(mapfn, MAXPATHLEN);
		bkmem_free(fn, MAXPATHLEN);

	} else {
		ax = DOSERR_ACCESSDENIED;

	}
	if (ax) {
		SET_CARRY(rp);
	} else {
		CLEAR_CARRY(rp);
	}
	AX(rp) = ax;
}

void
dosunlinkfile(struct real_regs *rp)
{
	char	*afn,		/* file name prepend with "boottree" */
		*mapfn;		/* file name mapped by compfs */
	ushort	ax;		/* holds error code */

	afn = dos_formpath(dosfn_to_unixfn(DS_DX(rp)));
	if (afn && boot_compfs_writecheck(afn, &mapfn)) {
		ax = dosUnlink(mapfn);
	} else {
		ax = DOSERR_PATHNOTFOUND;
	}

	AX(rp) = ax;
	if (ax) {
		SET_CARRY(rp);

	} else {
		CLEAR_CARRY(rp);

	}
}

void
dosseekfile(struct real_regs *rp)
{
	extern off_t boot_compfs_getpos(int fd);
	extern off_t boot_pcfs_lseek(int fd, off_t off, int whence);
	extern int boot_pcfs_fstat(int fd, struct stat *sp);

	dffd_t *cfd;
	off_t newoff;
	off_t reqoff;
	int reqtype;
	int handle;
	off_t (*handler)(int fd, off_t off, int whence);
	int (*statter)(int fd, struct stat *sp);

	reqoff = (CX(rp) << 16) + DX(rp);
	handle = BX(rp);

	if (!DOSfile_checkfd(handle)) {
		if (int21debug)
			printf("SEEK %x (bad handle)", handle);
		AX(rp) = DOSERR_INVALIDHANDLE;
		SET_CARRY(rp);
		return;
	}
	cfd = &(DOSfilefds[handle]);

	switch (AL(rp)) {
	case DOSSEEK_TOABS:
		reqtype = SEEK_SET;
		break;
	case DOSSEEK_FROMFP:
		reqtype = SEEK_CUR;
		break;
	case DOSSEEK_FROMEOF:
		reqtype = SEEK_END;
		break;
	default:
		if (int21debug)
			printf("(Bad whence %d)", AL(rp));
		AX(rp) = DOSERR_SEEKERROR;
		SET_CARRY(rp);
		return;
	}

	if (int21debug)
		printf("Seek(%x, %x, %x) ", handle, reqoff, reqtype);

	if (cfd->flags & DOSFD_RAMFILE) {
		if ((newoff = RAMfile_lseek(cfd->rfp, reqoff, reqtype)) < 0) {
			/* Set carry flag and fill ax to indicate failure */
			if (int21debug)
				printf("(RAMfile error)");
			AX(rp) = RAMfile_doserr;
			SET_CARRY(rp);
		} else {
			/* Clear carry flag and fill fields */
			if (int21debug)
				printf("(sought offset %x)", newoff);
			DX(rp) = newoff >> 16;
			AX(rp) = newoff & 0xFFFF;
			CLEAR_CARRY(rp);
		}
	} else if (cfd->flags & DOSFD_STDDEV) {
		/* Set carry flag and fill ax to indicate failure */
		AX(rp) = DOSERR_SEEKERROR;
		SET_CARRY(rp);
	} else {
		/*
		 * None of of our fs's support SEEK_END directly, so
		 * we must convert it to an absolute seek using the
		 * file size.
		 */
		if (reqtype == SEEK_END) {
			static struct stat fs;
			/*
			 * If we're writing to pcfs, we call the pcfs
			 * code directly
			 */
			if (cfd->flags & DOSFD_DSKWRT)
				statter = boot_pcfs_fstat;
			else
				statter = fstat;

			if ((*statter)(cfd->actualfd, &fs) < 0) {
				/* Indicate failure */
				if (int21debug)
					printf("(error)");
				AX(rp) = DOSERR_SEEKERROR;
				SET_CARRY(rp);
				return;
			}
			reqtype = SEEK_SET;
			reqoff = fs.st_size + reqoff;
		}

		/* if we're writing to pcfs, we call the pcfs code directly */
		if (cfd->flags & DOSFD_DSKWRT) {
			handler = boot_pcfs_lseek;
		} else {
			handler = lseek;
		}

		if ((newoff = (*handler)(cfd->actualfd, reqoff, reqtype)) < 0) {
			/* Set carry flag and fill ax to indicate failure */
			if (int21debug)
				printf("(fs error)");
			AX(rp) = DOSERR_SEEKERROR;
			SET_CARRY(rp);
		} else {
			/* Clear carry flag and fill fields */
			if (int21debug)
				printf("(sought offset %x)", reqoff);
			/*
			 * UFS/NFS return 0 on successful seeks rather than
			 * the useful value of the new offset.  DOS needs
			 * to accurately know where it is in the file.  I've
			 * added a bit of a hack function to the compfs library
			 * to give me the current file offset for a descriptor.
			 * Of course the big (and currently safe) assumption
			 * is that all files are being switched through the
			 * compfs file switch.
			 */
			if ((newoff = boot_compfs_getpos(cfd->actualfd)) >= 0) {
				DX(rp) = newoff >> 16;
				AX(rp) = newoff & 0xFFFF;
				CLEAR_CARRY(rp);
			} else {
				AX(rp) = DOSERR_SEEKERROR;
				SET_CARRY(rp);
				return;
			}
		}
	}
}

void
dosattribfile(struct real_regs *rp)
{
	char *fn;

	fn = DS_DX(rp);

	if (int21debug)
		printf("%s Attr \"%s\" ", AL(rp) ? "Set" : "Get", fn);

	CLEAR_CARRY(rp);
	if (AL(rp)) {
		if (int21debug)
			printf("(fail)");	/* XXX for now */
		AX(rp) = DOSERR_INVALIDHANDLE;
		SET_CARRY(rp);
	} else {
		if (int21debug)
			printf("(succeed)");
		CX(rp) = 1;	/* Read-only */
	}
}

void
dosfiletimes(struct real_regs *rp)
{
	if (int21debug)
		printf("%s date & time ", AL(rp) ? "Set" : "Get");

	/* validate the handle we've received */
	if (!(DOSfile_checkfd(BX(rp)))) {
		if (int21debug)
			printf("(bad handle)");
		SET_CARRY(rp);
		AX(rp) = DOSERR_INVALIDHANDLE;
		return;
	}

#ifdef	notdef
	if (AL(rp)) {
		/* Set the file time */
		/* Check that file is writable */
		/* Compute current time here */
	} else {
		/* Look up the file time */
	}
#endif	/* notdef */
	CX(rp) = 0;
	DX(rp) = 0;
	CLEAR_CARRY(rp);
}

void
dosioctl(struct real_regs *rp)
{
	int handle;

	if (int21debug)
		printf("ioctl %x ", AL(rp));

	if (!DOSfile_checkfd(handle = BX(rp))) {
		if (int21debug)
			printf("(bad handle)");
		AX(rp) = DOSERR_INVALIDHANDLE;
		SET_CARRY(rp);
		return;
	}
	CLEAR_CARRY(rp);
	switch (AL(rp)) {
	case 0x0:
		if (int21debug)
			printf("handle %x ", BX(rp));
		if (handle == DOS_STDOUT)
			DX(rp) = 0xC2;		/* XXX for now */
		else if (handle == DOS_STDIN)
			DX(rp) = 0xC1;		/* XXX for now */
		else if (handle == DOS_STDERR)
			DX(rp) = 0xC0;		/* XXX for now */
		else if (handle == DOS_STDAUX)
			DX(rp) = 0xC0;		/* XXX for now */
		else if (handle == DOS_STDPRN)
			DX(rp) = 0xC0;		/* XXX for now */
		else
			DX(rp) = 0x02;		/* XXX for now */
		if (int21debug)
			printf("(return %x)", DX(rp));
		break;

	case 0x1:
		break;

	case 0x2:
		break;

	case 0x3:
		break;

	case 0x4:
		break;

	case 0x5:
		break;

	case 0x6:
		break;

	case 0x7:
		break;

	case 0x8:
		break;

	case 0x9:
		break;

	case 0xa:
		break;

	case 0xb:
		break;

	case 0xc:
		break;

	case 0xd:
		break;

	case 0xe:
		break;

	default:
		if (int21debug)
			printf("(bad sub function)");
		AX(rp) = DOSERR_INVALIDHANDLE;
		SET_CARRY(rp);
		break;
	}
}

void
dosgetdate(struct real_regs *rp)
{
	ushort y, m, d;

	if (int21debug)
		printf("\ngetdate:");

	prom_rtc_date(&y, &m, &d);
	CX(rp) = y;
	DH(rp) = (unchar)m;
	DL(rp) = (unchar)d;

	if (int21debug)

		printf("Year,Mon,Day = %d,%d,%d\n", CX(rp), DH(rp), DL(rp));
}

void
dosgettime(struct real_regs *rp)
{
	ushort h, m, s;

	if (int21debug)
		printf("\ngettime:");

	prom_rtc_time(&h, &m, &s);

	CH(rp) = (unchar)h;
	CL(rp) = (unchar)m;
	DH(rp) = (unchar)s;
	DL(rp) = 0;	/* RTC Not precise to hundredths */

	if (int21debug)
		printf("Hr,Min,Sec = %d,%d,%d\n", CH(rp), CL(rp), DH(rp));
}

void
dosterminate(struct real_regs *rp)
{
	extern void DOSexe_checkmem();
	extern void comeback_with_stack();
	extern ffinfo *DOS_ffinfo_list;
	ffinfo *ffip;


	if (int21debug)
		printf("Terminate retcode %x", AL(rp));

	/* Close all files */
	DOSfile_closeall();

	/* XXX handle psp stuff... */

	/*
	 * using pointer from freed structure, but we shouldn't
	 * be doing anything else.
	 */
	for (ffip = DOS_ffinfo_list; ffip; ffip = ffip->next) {
		if (ffip->curmatchpath)
			bkmem_free(ffip->curmatchpath, MAXPATHLEN);
		if (ffip->curmatchfile)
			bkmem_free(ffip->curmatchfile, MAXPATHLEN);
		bkmem_free((caddr_t)ffip, sizeof (*ffip));
	}
	DOS_ffinfo_list = 0;

	/* vector the calling program back where it belongs */
	rp->ip = (ushort)((unsigned)comeback_with_stack & (unsigned)0xffff);
	rp->cs = 0;
	DOSexe_checkmem();
}

void
dosdrvdata(struct real_regs *rp)
{
	/*
	 * Unsure how important it is to get this info right.
	 * The first stat of a directory seems to be the cause
	 * of this call.  Difficult to get the info right as well;
	 * what the heck would we give it for a boot partition
	 * found solely on the UFS root filesystem?
	 */
	static char *media_id = 0;

	if (int21debug)
		printf("get drive info (%x)", DL(rp));

	if (!media_id && !(media_id = rm_malloc(1, 0, 0))) {
		printf("Failed to allocate media byte!\n");
		AL(rp) = 0xFF;
		return;
	}

	if ((DL(rp) == 0) || (DL(rp) == DOS_CDRIVE + 1)) {
		*media_id = (char)0xf8;	/* Fixed disk */
	} else {
		*media_id = (char)0xf9;
	}

	rp->ds = segpart((ulong)media_id);
	BX(rp) = offpart((ulong)media_id);
	CX(rp) = 512;
	AL(rp) = 1;
	DX(rp) = 1;
}

extern struct dos_fninfo *DOScurrent_dta;

void
dosgetdta(struct real_regs *rp)
{
	if (int21debug)
		printf("dosgetdta ");

	rp->es = segpart((ulong)DOScurrent_dta);
	BX(rp) = offpart((ulong)DOScurrent_dta);

	if (int21debug)
		printf("ES:BX=%x:%x ", rp->es, BX(rp));
}

void
dossetdta(struct real_regs *rp)
{

	if (int21debug)
		printf("dossetdta[%x])", DS_DX(rp));

	DOScurrent_dta = (struct dos_fninfo *)DS_DX(rp);
}

int
handle21(struct real_regs *rp)
{
	extern ulong i21cnt;
	extern void rm_check();
	short funreq;
	char *sp;
	int handled = 1;
	static char buf[20];

	/* make sure the realmem heap looks ok */
	rm_check();

	/* get and check property value length */
	if (bgetproplen(bop, "int21debug", 0) < 0)
		int21debug = 0;
	else {
		(void) bgetprop(bop, "int21debug", buf, sizeof (buf), 0);
		int21debug = strtol(buf, 0, 0);
	}

	/*
	 *  Determine which DOS function was requested.
	 */
	i21cnt++;
	funreq = (short)AH(rp);

	if (int21debug > 1)
		printf("{int 21 func %x cs:ip=%x:%x,ds=%x}", funreq,
		    rp->cs & 0xffff, rp->ip & 0xffff, rp->ds & 0xffff);

	if (int21debug > 2) {
		printf("(paused)");
		(void) gets(buf);
	}

	switch (funreq) {

	case 0x02:	/* output character in dl */
		putchar(DL(rp));
		break;

	case 0x07:	/* input character without echo */
	case 0x08:
		dosgetchar(rp);
		break;

	case 0x09:	/* output string in ds:dx */
		sp = DS_DX(rp);
		while (*sp != '$')
			putchar(*sp++);
		break;

	case 0x0b:	/* check input status */
		if (int21debug)
			printf("ischar ");
		AL(rp) = ischar() ? 0xff : 0x0;
		if (int21debug)
			printf("(%x)", AL(rp));
		break;

	case 0x19:	/* get current disk */
		if (int21debug)
			printf("get drive (%x)", DOS_CDRIVE);
		AL(rp) = DOS_CDRIVE;
		break;

	case 0x1a:	/* set address of disk transfer area */
		dossetdta(rp);
		break;

	case 0x1c:	/* info request about disk */
		dosdrvdata(rp);
		break;

	case 0x25:	/* set interrupt vector */
		set_dosivec(AL(rp), rp->ds, DX(rp));
		break;

	case 0x29:	/* parse filename */
		dosparsefn(rp);
		break;

	case 0x2a:	/* get date */
		dosgetdate(rp);
		break;

	case 0x2c:	/* get time */
		dosgettime(rp);
		break;

	case 0x2f:	/* get address of disk transfer area */
		dosgetdta(rp);
		break;

	case 0x30:	/* get MS-DOS version number */
		if (int21debug)
			printf("Get version (3.10)");
		AL(rp) = 0x3;
		AH(rp) = 0xa;
		BH(rp) = 0xff;
		break;

	case 0x33:
		if (AL(rp) <= 1) {
			if (int21debug)
				printf("(Extended break check %s)",
				    AL(rp) ? "Set" : "Get");
			DL(rp) = 0;
		} else {
			/*
			 * There are three other subfunctions that we
			 * don't handle.
			 * 02 = Get and set extended control-break checking
			 * state
			 * 05 = Get Boot Driver
			 * 06 = Get true version number
			 */
			printf("[21,%x @ cs:ip=%x:%x,ds=%x]", funreq,
			    rp->cs & 0xffff, rp->ip & 0xffff,
			    rp->ds & 0xffff);
			handled = 0;
		}
		break;

	case 0x35:	/* get interrupt vector */
		get_dosivec(AL(rp), &(rp->es), &(BX(rp)));
		break;

	case 0x3b:	/* set current directory */
		/* XXX for now just make this look like it succeeds */
		if (int21debug)
			printf("set directory (act like nop)");
		CLEAR_CARRY(rp);
		break;

	case 0x3c:	/* create file */
		doscreatefile(rp);
		break;

	case 0x3d:	/* open file */
		dosopenfile(rp);
		break;

	case 0x3e:	/* close file */
		dosclosefile(rp);
		break;

	case 0x3f:	/* read file */
		dosreadfile(rp);
		break;

	case 0x40:	/* write file */
		doswritefile(rp);
		break;

	case 0x41:	/* unlink file */
		dosunlinkfile(rp);
		break;

	case 0x42:	/* file seek */
		dosseekfile(rp);
		break;

	case 0x43:	/* get/set file attributes */
		dosattribfile(rp);
		break;

	case 0x44:	/* ioctl */
		dosioctl(rp);
		break;

	case 0x48:
		dosallocpars(rp);
		break;

	case 0x49:
		dosfreepars(rp);
		break;

	case 0x4a:
		dosreallocpars(rp);
		break;

	case 0x4b:	/* exec */
		/* XXX fail for now */
		if (int21debug)
			printf("exec \"%s\" (fail)", DS_DX(rp));
		AX(rp) = DOSERR_INSUFFICIENT_MEMORY;
		SET_CARRY(rp);
		break;

	case 0x4c:	/* terminate with extreme prejudice */
		dosterminate(rp);
		break;

	case 0x4d:	/* get return code */
		/* XXX for now */
		if (int21debug)
			printf("get return code (0)");
		AX(rp) = 0;
		break;

	case 0x47:	/* Get current directory */
		dosgetcwd(rp);
		break;

	case 0x4e:	/* find first file */
		dosfindfirst(rp);
		break;

	case 0x4f:	/* find next file */
		dosfindnext(rp);
		break;

	case 0x56:
		dosrenamefile(rp);
		break;

	case 0x57:	/* get/set file date/time */
		dosfiletimes(rp);
		break;

	case 0xff:	/* check for redirected console */
		if (int21debug)
			printf("check for console redirection");
		if (AL(rp) == 0) {
			if (serial_port_enabled(DX(rp))) {
				SET_CARRY(rp);
			} else {
				CLEAR_CARRY(rp);
			}
			break;
		}
		/* fall through */

	default:
		printf("[21,%x @ cs:ip=%x:%x,ds=%x]", funreq,
		    rp->cs & 0xffff, rp->ip & 0xffff, rp->ds & 0xffff);
		handled = 0;
		printf("(paused)");
		(void) gets(buf);
		break;
	}

	return (handled);
}
