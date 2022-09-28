/*
 * Copyright (c) 1996, Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)bootops.c	1.50	96/09/19 SMI"

#include <sys/types.h>
#include <sys/bootconf.h>
#include <sys/reboot.h>
#include <sys/param.h>
#include <sys/varargs.h>
#include <sys/obpdefs.h>
#include <sys/promif.h>
#include <sys/salib.h>

#if defined(DEBUG) || defined(lint)
static int debug = 0;
#else
#define	debug	0
#endif DEBUG
#define	dprintf	if (debug) printf

extern caddr_t		memlistpage;
extern caddr_t		tablep;
extern char 		filename[];
extern int 		verbosemode, cache_state;
extern struct memlist *pinstalledp, *pfreelistp, *vfreelistp;

extern int		kern_open(char *str, int flags);
extern int		kern_read(int fd, caddr_t buf, u_int size);
extern off_t		kern_lseek(int filefd, off_t hi, off_t lo);
extern caddr_t		kern_resalloc(caddr_t virthint, u_int size, int align);
extern void		kern_killboot(void);
extern int		kern_close(int fd);
extern int		kern_mountroot(char *path);
extern int		kern_unmountroot(void);
extern void		closeall(int);

extern int		bgetprop(struct bootops *, char *name, void *buf);
extern int		bgetproplen(struct bootops *, char *name);
extern char		*bnextprop(struct bootops *, char *prev);

extern void		update_memlist(char *name, char *prop,
				struct memlist **l);
extern void 		print_memlist(struct memlist *av);
extern char 		*translate_v0tov2(char *s);
extern void 		translate_v2tov0(char *v2path, char *v0path);
extern void		install_memlistptrs(void);
extern struct memlist	*fill_memlists(char *name, char *prop,
				struct memlist *old);
extern void		silence_nets(void);
extern caddr_t		kmem_alloc(u_int);

extern char *impl_arch_name;

int boot_version = BO_VERSION;

#define	MAXARGS	8

struct bootops bootops;
extern char *systype;	/* set in filesystem specific library used */

struct bootcode {
	char    letter;
	u_int	bit;
} bootcode[] = {	/* see reboot.h */
	'a',    RB_ASKNAME,
	's',    RB_SINGLE,
	'i',    RB_INITNAME,
	'h',    RB_HALT,
	'b',    RB_NOBOOTRC,
	'd',    RB_DEBUG,
	'w',    RB_WRITABLE,
	'G',	RB_GDB,
	'c',	RB_CONFIG,
	'r',	RB_RECONFIG,
	'v',	RB_VERBOSE,
	'k',	RB_KDBX,
	'f',	RB_FLUSHCACHE,
	0,	0
};


#define	skip_whitespc(cp) while (cp && (*cp == '\t' || *cp == '\n' || \
	*cp == '\r' || *cp == ' ')) cp++;
/*
 *  This routine is for V2+ proms only.  It assumes
 *  and inserts whitespace twixt all arguments.
 *
 *  We have 2 kinds of inputs to contend with:
 *	filename -options
 *		and
 *	-options
 *  This routine assumes buf is a ptr to sufficient space
 *  for all of the goings on here.
 */
void
v2_getargs(char *defname, char *buf)
{
	char *cp, *tp;

	tp = prom_bootargs();

	if (!tp || *tp == '\0') {
		(void) strcpy(buf, defname);
		return;
	}

	skip_whitespc(tp);

	/*
	 * If we don't have an option indicator, then we
	 * already have our filename prepended. Check to
	 * see if the filename is "vmunix" - if it is, sneakily
	 * translate it to the default name.
	 */
	if (*tp && *tp != '-') {
		if (strcmp(tp, "vmunix") == 0 || strcmp(tp, "/vmunix") == 0)
			(void) strcpy(buf, defname);
		else
			(void) strcpy(buf, tp);
		return;
	}

	/* else we have to insert it */

	cp = defname;	/* this used to be a for loop, but cstyle is buggy */
	while (cp && *cp)
		*buf++ = *cp++;

	if (*tp) {
		*buf++ = ' ';	/* whitspc separator */

		/* now copy in the rest of the bootargs, as they were */
		(void) strcpy(buf, tp);
	} else {
		*buf = '\0';
	}
}

/*
 *  Here we know everything we need except for the
 *  name of standalone we want to boot.
 *  This routine for V0/sunmon only.
 */
void
sunmon_getargs(char *defname, char *buf)
{
	struct bootparam *bp;
	char *cp;
	int i;
	extern char *strrchr(char *s, char c);

	bp = prom_bootparam();

	cp = bp->bp_argv[0];

	/*
	 * Since sunmon's consider the filename as part of the device
	 * string, we gotta strip it off to get at it.
	 */
	cp = strrchr(cp, ')');

	cp++;
	if (cp && *cp) {
		/*
		 * There's already a file in the bootparam
		 * so we use what the user wants .. UNLESS
		 * the name is 'vmunix' which we quietly
		 * translate to the default name anyway. Ick.
		 */
		if (strcmp(cp, "vmunix") == 0 || strcmp(cp, "/vmunix") == 0)
			(void) strcpy(buf, defname);
		else
			(void) strcpy(buf, cp);
	} else {
		/*
		 * gotta roll our own anyway
		 */
		(void) strcpy(buf, defname);
	}

	for (i = 1; i < MAXARGS; i++) {
		/* see if we have any more args */
		if ((cp = bp->bp_argv[i]) == NULL)
			break;
		/* if so, then insert blanks twixt them */
		if (*cp) {
			(void) strcat(buf, " ");
			(void) strcat(buf, cp);
		}
	}
}


/*
 * Here are the bootops wrappers
 */
/*ARGSUSED*/
static int
bkern_open(struct bootops *bop, char *str, int flags)
{
	return (kern_open(str, flags));
}

/*ARGSUSED*/
static int
bkern_read(struct bootops *bop, int fd, caddr_t buf, u_int size)
{
	return (kern_read(fd, buf, size));
}

/*ARGSUSED*/
static int
bkern_lseek(struct bootops *bop, int filefd, off_t hi, off_t lo)
{
	return ((int)kern_lseek(filefd, hi, lo));
}

/*ARGSUSED*/
static int
bkern_close(struct bootops *bop, int fd)
{
	return (kern_close(fd));
}

/*ARGSUSED*/
static caddr_t
bkern_resalloc(struct bootops *bop, caddr_t virthint, u_int size, int align)
{
	return (kern_resalloc(virthint, size, align));
}

/*ARGSUSED*/
static void
bkern_free(struct bootops *bop, caddr_t virt, u_int size)
{}

/*ARGSUSED*/
static caddr_t
bkern_map(struct bootops *bop, caddr_t virt, int space, caddr_t phys,
    u_int size)
{
	return ((caddr_t)0);
}

/*ARGSUSED*/
static void
bkern_unmap(struct bootops *bop, caddr_t virt, u_int size)
{}

/*ARGSUSED*/
static void
bkern_killboot(struct bootops *bop)
{
	kern_killboot();
}

/*ARGSUSED*/
/*PRINTFLIKE2*/
static void
bkern_printf(struct bootops *bop, char *fmt, ...)
{
	va_list adx;

	va_start(adx, fmt);
	prom_vprintf(fmt, adx);
	va_end(adx);
}

/*ARGSUSED*/
static int
bkern_mountroot(struct bootops *bop, char *path)
{
	return (kern_mountroot(path));
}

/*ARGSUSED*/
static int
bkern_unmountroot(struct bootops *bop)
{
	return (kern_unmountroot());
}

void
setup_bootops(void)
{
	bootops.bsys_version = boot_version;
	bootops.bsys_super = NULL;
	bootops.bsys_open = bkern_open;   /* set up function ptrs */
	bootops.bsys_read = bkern_read;
	bootops.bsys_seek = bkern_lseek;
	bootops.bsys_close = bkern_close;
	bootops.bsys_alloc = bkern_resalloc;
	bootops.bsys_free = bkern_free;	/* fake deallocator */
	bootops.bsys_map = bkern_map;		/* fake mapper */
	bootops.bsys_unmap = bkern_unmap;	/* fake unmapper */
	bootops.bsys_quiesce_io = bkern_killboot;
	bootops.bsys_getproplen = bgetproplen;
	bootops.bsys_getprop = bgetprop;
	bootops.bsys_nextprop = bnextprop;
	bootops.bsys_printf = bkern_printf;
	bootops.bsys_mountroot = bkern_mountroot;
	bootops.bsys_unmountroot = bkern_unmountroot;

	if (!memlistpage) /* paranoia runs rampant */
		prom_panic("\nMemlistpage not setup yet.");

	bootops.boot_mem = (struct bsys_mem *)memlistpage;

	/*
	 * The memory list should always be updated last.  The prom
	 * calls which are made to update a memory list may have the
	 * undesirable affect of claiming physical memory.  This may
	 * happen after the kernel has created its page free list.
	 * The kernel deals with this by comparing the n and n-1
	 * snapshots of memory.  Updating the memory available list
	 * last guarantees we will have a current, accurate snapshot.
	 * See bug #1260786.
	 */
	update_memlist("virtual-memory", "available", &vfreelistp);
	update_memlist("memory", "available", &pfreelistp);

	dprintf("\nPhysinstalled: ");
	if (debug) print_memlist(pinstalledp);
	dprintf("\nPhysfree: ");
	if (debug) print_memlist(pfreelistp);
	dprintf("\nVirtfree: ");
	if (debug) print_memlist(vfreelistp);
}

void
install_memlistptrs(void)
{
	/* Actually install the list ptrs in the 1st 3 spots */
	/* Note that they are relative to the start of boot_mem */
	bootops.boot_mem->physinstalled = pinstalledp;
	bootops.boot_mem->physavail = pfreelistp;
	bootops.boot_mem->virtavail = vfreelistp;

	/* prob only need 1 page for now */
	bootops.boot_mem->extent = tablep - memlistpage;

	dprintf("physinstalled = %x\n", bootops.boot_mem->physinstalled);
	dprintf("physavail = %x\n", bootops.boot_mem->physavail);
	dprintf("virtavail = %x\n", bootops.boot_mem->virtavail);
	dprintf("extent = %x\n", bootops.boot_mem->extent);
}

/*
 *	A word of explanation is in order.
 *	This routine is meant to be called during
 *	boot_release(), when the kernel is trying
 *	to ascertain the current state of memory
 *	so that it can use a memlist to walk itself
 *	thru kvm_init().
 *	There are 3 stories to tell:
 *	SunMon:  We will have been keeping
 *		memlists for this prom all along,
 *		so we just export the internal list.
 *	V0:	Again, we have been keeping memlists
 *		for V0 all along, so we just export
 *		the internally-kept one.
 *	V2+:	For V2 and later (V2+) we need to
 *		reread the prom memlist structure
 *		since we have been making prom_alloc()'s
 *		fast and furious until now.  We just
 *		call fill_memlists() again to take
 *		another V2 snapshot of memory.
 */

void
update_memlist(char *name, char *prop, struct memlist **list)
{
	if (prom_getversion() > 0) {
	/* Just take another prom snapshot */
		*list = fill_memlists(name, prop, *list);
	}
	install_memlistptrs();
}

/*
 *  This routine is meant to be called by the
 *  kernel to shut down all boot and prom activity.
 *  After this routine is called, PROM or boot IO is no
 *  longer possible, nor is memory allocation.
 */
void
kern_killboot(void)
{
	if (verbosemode) {
		dprintf("Entering boot_release()\n");
		dprintf("\nPhysinstalled: ");
		if (debug) print_memlist(pinstalledp);
		dprintf("\nPhysfree: ");
		if (debug) print_memlist(pfreelistp);
		dprintf("\nVirtfree: ");
		if (debug) print_memlist(vfreelistp);
	}
	if (debug) {
		printf("Calling quiesce_io()\n");
		prom_enter_mon();
	}

	/*
	 *  open and then close all network devices
	 *  must walk devtree for this
	 */
	silence_nets();

	/* close all open devices */
	closeall(1);

	/*
	 *  Now we take YAPS (yet another Prom snapshot) of
	 *  memory, just for safety sake.
	 *
	 * The memory list should always be updated last.  The prom
	 * calls which are made to update a memory list may have the
	 * undesirable affect of claiming physical memory.  This may
	 * happen after the kernel has created its page free list.
	 * The kernel deals with this by comparing the n and n-1
	 * snapshots of memory.  Updating the memory available list
	 * last guarantees we will have a current, accurate snapshot.
	 * See bug #1260786.
	 */
	update_memlist("virtual-memory", "available", &vfreelistp);
	update_memlist("memory", "available", &pfreelistp);

	if (verbosemode) {
	dprintf("physinstalled = %x\n", bootops.boot_mem->physinstalled);
	dprintf("physavail = %x\n", bootops.boot_mem->physavail);
	dprintf("virtavail = %x\n", bootops.boot_mem->virtavail);
	dprintf("extent = %x\n", bootops.boot_mem->extent);
	dprintf("Leaving boot_release()\n");
	dprintf("Physinstalled: \n");
		if (debug) print_memlist(pinstalledp);
		dprintf("Physfree:\n");
		if (debug) print_memlist(pfreelistp);
		dprintf("Virtfree: \n");
		if (debug) print_memlist(vfreelistp);
	}

#ifdef DEBUG_MMU
	dump_mmu();
	prom_enter_mon();
#endif DEBUG_MMU
}

/*
 * Parse command line to determine boot flags.  We create a
 * new string as a result of the parse which has our own
 * set of private flags removed.
 */
int
bootflags(register char *cp)
{
	register int i, boothowto = 0;
	static char buf[256];
	register char *op = buf;
	char *save_cp = cp;
	static char tmp_impl_arch[MAXNAMELEN];

	impl_arch_name = NULL;

	if (cp == NULL)
		return (0);

	/*
	 * skip over filename, if necessary
	 */
	while (*cp && *cp != ' ')
		*op++ = *cp++;
	/*
	 * Skip whitespace.
	 */
	while (*cp && *cp == ' ')
		*op++ = *cp++;
	/*
	 * consume the bootflags, if any.
	 */
	if (*cp && *cp++ == '-') {
		while (*cp && *cp != ' ' && *cp != '\t') {
			if (*cp == 'V')
				verbosemode = 1;
			else if (*cp == 'n') {
				cache_state = 0;
				printf("Warning: boot will not enable cache\n");
			} else if (*cp == 'I') {
				/* toss white space */
				cp++;
				while (*cp == ' ' || *cp == '\t') {
					cp++;
				}
				for (i = 0; *cp && *cp != ' ' &&
						 *cp != '\t'; cp++, i++) {
					tmp_impl_arch[i] = *cp;
				}
				tmp_impl_arch[i] = '\0';
				impl_arch_name = &tmp_impl_arch[0];
			} else
				for (i = 0; bootcode[i].letter; i++) {
					if (*cp == bootcode[i].letter) {
						boothowto |= bootcode[i].bit;
						break;
					}
				}
			cp++;
		}
	} else
		return (0);

	/*
	 * Update the output string only with the bootflags we're
	 * *supposed* to pass on to the standalone.
	 */
	if (boothowto) {
		*op++ = '-';
		for (i = 0; bootcode[i].letter; i++)
			if (bootcode[i].bit & boothowto)
				*op++ = bootcode[i].letter;
	}

	/*
	 * Copy the rest of the string, if any..
	 */
	while (*op++ = *cp++)
		;

	/*
	 * Now copy the resulting buffer back onto the original. Sigh.
	 */
	(void) strcpy(save_cp, buf);

	return (boothowto);
}

/* ARGSUSED */
void
set_cache_state(int cache_state)
{
#ifdef	sun4c
	extern int vac;
	extern void sunm_turnon_cache();
	extern void sunm_cache_prog(char *start, char *end);
	extern char end[], _start[];

	if (vac && cache_state) {
		/* mark boot's pages as cacheable */
		sunm_cache_prog(_start, end);
		/* clear the tags and set the CACHE bit in sys enable reg */
		sunm_turnon_cache();
	}
#endif
}

static char buf[OBP_MAXPATHLEN];

/*
 *  This routine will conz up a name from a V0 PROM which
 *  the kernel can understand as a V2 name.
 *  As long as the kernel gets its device
 *  path from boot, this routine is really only needed by boot.
 */
char *
translate_v0tov2(char *name)
{
	struct bootparam *bp = prom_bootparam();

	switch (prom_getversion()) {

	case OBP_V0_ROMVEC_VERSION:
		if (strncmp(name, "sd", 2) == 0) {
			static char targs[] = "31204567";
			dnode_t opt_node;
			dnode_t sp[OBP_STACKDEPTH];
			pstack_t *stk;

			stk = prom_stack_init(sp, sizeof (sp));
			opt_node = prom_findnode_byname(prom_nextnode(0),
			    "options", stk);
			prom_stack_fini(stk);
			if (prom_getproplen(opt_node, "sd-targets") > 0)
				(void) prom_getprop(opt_node, "sd-targets",
				    targs);
			dprintf("sd-targets is '%s'\n", targs);
			(void) sprintf(buf,
			    "/sbus@1,f8000000/esp@0,800000/sd@%c,0:%c",
			    targs[bp->bp_unit], bp->bp_part + 'a');
			dprintf("boot_dev_name: '%s'\n", buf);
			return (buf);
		} else if (strncmp(name, "le", 2) == 0) {
			(void) strcpy(buf, "/sbus@1,f8000000/le@0,c00000");
			dprintf("boot_dev_name: '%s'\n", buf);
			return (buf);
		} else if (strncmp(name, "fd", 2) == 0) {
			(void) sprintf(buf,
			    "/fd@1,f7200000:%c", bp->bp_part + 'a');
			dprintf("boot_dev_name: '%s'\n", buf);
			return (buf);
		} else {
			printf("boot device '%c%c' not supported by V0 OBP.\n",
			    *name, *(name+1));
			return ((char *)0);
		}

	case OBP_V2_ROMVEC_VERSION:
	case OBP_V3_ROMVEC_VERSION:
		prom_panic("Should not find V0 name on this machine");
		/*NOTREACHED*/

	default:
		prom_panic("Bad romvec version");
		/*NOTREACHED*/
	}
	/*NOTREACHED*/
	return ((char *)0);
}

/*
 *  This routine creates a V0 prom device path from a V2 prom
 *  device path.
 */
/* ARGSUSED */
void
translate_v2tov0(char *v2name, char *netpath)
{
	switch (prom_getversion()) {

	case SUNMON_ROMVEC_VERSION:
	case OBP_V0_ROMVEC_VERSION:
		if (strstr(v2name, "/le@") != NULL) {
			(void) sprintf(netpath, "le(0,0,0)");
		} else {
			(void) sprintf(netpath, "ie(0,0,0)");
		}
		break;

	case OBP_V2_ROMVEC_VERSION:
	case OBP_V3_ROMVEC_VERSION:
	case OBP_PSEUDO_ROMVEC_VERSION:
		(void) strcpy(netpath, v2name);
		break;

	default:
		prom_panic("Bad romvec version");
	}
}
