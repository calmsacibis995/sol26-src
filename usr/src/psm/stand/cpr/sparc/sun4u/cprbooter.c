/*
 * Copyright (c) 1994 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cprbooter.c	1.21	96/09/19 SMI"

/*
 * cprbooter - First pass of the resume bootstrapping process, which is
 * architected similar to ufsboot. It is responsible for:
 *	1. Get OBP bootargs info and reset boot environment (OBP boot-file
 *	   property in the /options node) to the original value before the
 *	   system was being suspened.
 * 	2. Open cpr statefile /.CPR
 *	3. Read in and validate the ELF header of the statefile
 *	4. Read in the memory usage information (the memory bitmap) from
 *	   the statefile
 *	5. According to the memory bitmap, allocate all the physical memory
 *	   that is needed by the resume kernel.
 *	6. Allocate all the virtual memory that is needed for the resume
 *	   bootstrapping operation.
 *	7. Close the statefile.
 *	8. Read in the cprboot program.
 *	9. Hands off the execution control to cprboot.
 *
 * Note: The boot-file OBP property has been assigned to the following value
 * by the suspend process:
 *			-F cprbooter cprboot
 * This is to indicate the system has been suspended and the resume
 * bootstrapping will be initiated the next time the system is being booted.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/bootconf.h>
#include <sys/elf.h>
#include <sys/cpr.h>
#include <sys/cpr_impl.h>
#include <sys/prom_plat.h>

#if	(CPR_PROM_RETAIN_CNT != 1)
	ASSERTION FAILURE, can't handle multiple prom retains
#endif

/* Adopted from ../stand/boot/sparc/common/bootops.c */
#define	skip_whitespc(cp) while (cp && (*cp == '\t' || *cp == '\n' || \
	*cp == '\r' || *cp == ' ')) cp++;

/*
 * The elf header of cprboot is validated by the bootblk.
 */

/*
 * External procedures declarations
 */
extern int	(*readfile(int fd, int print))();
extern void	exitto(int (*)(), caddr_t);
extern caddr_t	getloadbase();
extern int cpr_reset_properties();
extern int cpr_locate_statefile(char *, char *);
extern int open(char *, char *);
extern int read(int, caddr_t, int);
extern int close(int);

/*
 * Static local procedures declarations
 */
static int	cpr_read_bitmap(int fd);
static int	cpr_claim_mem();
static void	check_bootargs();
static caddr_t	cpr_alloc(int);

/* static char	*cpr_bootprim = "cprbooter";	 Primary cprboot program */
static char	*cpr_bootsec = "cprboot";	/* Secondary cprboot program */

char cpr_statefile[OBP_MAXPATHLEN];
char cpr_filesystem[OBP_MAXPATHLEN];
static caddr_t machdep_buf;
#define	m_info (*(struct sun4u_machdep *) machdep_buf)

int	claim_phys_page(u_int);

static int totbitmaps, totpages;
static int totpg = 0;
static struct	cpr_bitmap_desc *bitmap_desc;
int	cpr_debug;
static cpr_func_t cpr_resume_pc;
static char cpr_heap[MMU_PAGESIZE * 5];	/* bitmaps for 2.5G minus overhead */

extern void _start(void *, void *, void *, void *);
extern char _end[];			/* cprboot end addr */
static u_longlong_t s_pfn, e_pfn;	/* start, end pfn of cprbooter */

#define	isset(a, i)	((a)[(i) / NBBY] & (1 << ((i) % NBBY)))
#define	max(a, b)	((a) > (b) ? (a) : (b))
#define	min(a, b)	((a) < (b) ? (a) : (b))
#define	roundup(x, y)   ((((x)+((y)-1))/(y))*(y))

/*
 * cookie - ieee1275 cif handle passed from srt0.s.
 *
 */
main(void *cookie)
{
	int	fd;
	caddr_t dummy;
	u_int	machdep_len;

	int (*jmp2)();		/* Start of cprboot */

	prom_init("cprbooter", cookie);
	DEBUG4(errp("\nboot: boot /platform/sun4u/cprbooter\n"));

	/*
	 * Clear console
	 */
	prom_printf("\033[p");
	prom_printf("\014");
	prom_printf("\033[1P");
	prom_printf("\033[18;21H");
	prom_printf("Restoring the System. Please Wait... ");

	check_bootargs();		/* Sanity Check, may not need this */

	/*
	 * Restore the original values of the nvram properties modified
	 * during suspend.  Note: if we can't get this info from the
	 * defaults file, the state file may be obsolete or bad, so we
	 * abort.  However, failure to restore one or more properties
	 * is NOT fatal (better to continue the resume).
	 */
	if (cpr_reset_properties() == -1) {
		errp("cprbooter: Cannot read saved "
			"nvram info, please reboot.\n");
		return (-1);
	}
	/*
	 * Pass the pathname of the statefile and its filesystem
	 * to cprboot, so that cprboot doesn't need to get them again.
	 */
	if (cpr_locate_statefile(cpr_statefile, cpr_filesystem) == -1) {
		errp("cprbooter: Cannot find statefile; "
			"please do a normal boot.\n");

		return (-1);
	}

	if ((fd = open(cpr_statefile, cpr_filesystem)) == -1) {
		errp("Can't open %s on %s, please reboot\n",
		    cpr_statefile, cpr_filesystem);
		return (-1);
	}

	if (cpr_read_elf(fd) != 0) {
		(void) prom_close(fd);
		errp("Can't read statefile elf header, please reboot\n");
		return (-1);
	}

	/*
	 * Don't need the rest of the returns at this time, so pass dummy
	 */
	if (cpr_read_cdump(fd, &cpr_resume_pc, &dummy,
		(u_int *)&dummy, &dummy, &totbitmaps, &totpages) != 0) {
		(void) prom_close(fd);
		errp("Can't read statefile dump header, please reboot\n");
		return (-1);
	}
	DEBUG4(errp("cprbooter:	totbitmaps %d totpages %d\n",
		totbitmaps, totpages));

	if ((machdep_len = (u_int) cpr_get_machdep_len(fd)) == (int) -1)
		return (-1);

	/*
	 * Note: Memory obtained from cpr_alloc() comes from the heap
	 * and doesn't need to be freed, since we are going to exit
	 * to the kernel in any case.
	 */
	if ((machdep_buf = cpr_alloc(machdep_len)) == 0) {
		errp("Can't alloc machdep buffer\n");
		return (-1);
	}

	if (cpr_read_machdep(fd, machdep_buf, machdep_len) == -1)
		return (-1);

#ifdef CPRBOOT_DEBUG
	for (i = 0; i < m_info.dtte_cnt; i++) {
		errp("dtte: no %d va_tag %x ctx %x tte %x\n",
		m_info.dtte[i].no, m_info.dtte[i].va_tag,
		m_info.dtte[i].ctx, m_info.dtte[i].tte);
	}

	for (i = 0; i < m_info.itte_cnt; i++) {
		errp("itte: no %d va_tag %x ctx %x tte %x\n",
		m_info.itte[i].no, m_info.itte[i].va_tag,
		m_info.itte[i].ctx, m_info.itte[i].tte);
	}
#endif CPRBOOT_DEBUG

	/*
	 * Read in the statefile bitmap
	 */
	if (cpr_read_bitmap(fd) != 0) {
		(void) prom_close(fd);
		errp("Can't read statefile bitmap, please reboot\n");
		return (-1);
	}

	/*
	 * Close the statefile and the disk device
	 * So that prom will free up some more memory.
	 */
	(void) prom_close(fd);

	/*
	 * Claim physical memory from OBP for the resume kernel
	 */
	if (cpr_claim_mem() != 0) {
		errp("Can't claim phys memory for resume, please reboot\n");
		return (-1);
	}

	if ((fd = open(CPRBOOT_PATH, prom_bootpath())) == -1) {
		errp("Can't open %s, please reboot\n", CPRBOOT_PATH);
		return (-1);
	}
	DEBUG4(errp("cprbooter: Opened cprboot\n"));

	if ((jmp2 = (int (*)())readfile(fd, 0)) != (int(*)()) -1) {
		DEBUG4(errp("cprbooter: Read cprboot\n"));
		(void) prom_close(fd);
	} else {
		(void) prom_close(fd);
		errp("Failed to read cprboot, please reboot\n");
		return (-1);
	}

	DEBUG4(errp("Calling exitto(%x)\n", jmp2));

	(void) exitto(jmp2, getloadbase());
	/*NOTREACHED*/
}

int
cpr_read_bitmap(int fd)
{
	int i, bitmapsize, size;
	char *bitmap;
	static caddr_t cpr_alloc(int);

	/*
	 * Use cpr_alloc() to dynamically allocate the bitmap desc array.
	 * This comes from BSS to keep us from doing prom_alloc and
	 * getting in trouble with the prom claiming memory we need
	 * (The cprbooter image is not reflected in the proms physavail
	 * list, due to ancient assumptions of bootblock clients)
	 */
	size = totbitmaps * sizeof (struct cpr_bitmap_desc);
	bitmap_desc = (struct cpr_bitmap_desc *)
		cpr_alloc(size);
	DEBUG4(errp("cpr_read_bitmap: totbitmaps %d cpr_alloc size %d\n",
		totbitmaps, size));

	for (i = 0; i < totbitmaps; i++) {
		if (prom_read(fd, (caddr_t)&bitmap_desc[i],
			sizeof (cbd_t), 0, 0) < sizeof (cbd_t)) {
			errp("cpr_read_bitmap: Falied to read bitmap desc %d\n",
				i);
			return (-1);
		}

		if (bitmap_desc[i].cbd_magic != CPR_BITMAP_MAGIC) {
			errp("cpr_read_bitmap: Bitmap %d BAD MAGIC %x\n", i,
				bitmap_desc[i].cbd_magic);
			return (-1);
		}

		bitmapsize = bitmap_desc[i].cbd_size;

		bitmap = cpr_alloc(bitmapsize);

		if (bitmap == NULL) {
			errp("cpr_read_bitmap: Can't cpr_alloc bitmap %x\n",
				bitmap);
			return (-1);
		}

		bitmap_desc[i].cbd_bitmap = bitmap;

		if (prom_read(fd, bitmap, bitmapsize, 0, 0) < bitmapsize) {
			errp("cpr_read_bitmap: Error reading bitmap %d\n", i);
			return (-1);
		}
	}
	return (0);
}

/*
 * According to the bitmaps, claim as much physical memory as we can
 * for the resume kernel.
 *
 * If the un-claimable memory are used by cprbooter or by the previous
 * prom_alloc's (memory that are used by the bitmap_desc[]), we are safe;
 * Otherwise, abort.
 */
static int
cpr_claim_mem()
{
	u_longlong_t s_addr, e_addr;
	u_longlong_t lo_pfn, hi_pfn;	/* lo, high pfn of bitmap chunk */
	u_longlong_t lo_extent, hi_extent;	/* enclosure of cprbooter */
	int valid, mode, handled;
	static int getchunk(u_longlong_t *lopfnp, u_longlong_t *hipfnp);
	static int claim_chunk(u_longlong_t lo_pfn, u_longlong_t hi_pfn);

	/*
	 * Find the physical addresses of cprbooter _start and _end,
	 */
	(void) prom_translate_virt((caddr_t)_start, &valid, &s_addr, &mode);
	if (valid != -1) {
		errp("cpr_claim_mem: Can't xlate _start %x\n", (caddr_t)_start);
		return (-1);
	}
	if (s_addr >> 32)
		DEBUG4(errp("cpr_claim_mem: xlated _start s_addr %x%8x\n",
		(int)(s_addr >> 32), (int)(s_addr)))
	else
		DEBUG4(errp("cpr_claim_mem: xlated _start s_addr %x\n",
		(int)s_addr));

	(void) prom_translate_virt(_end, &valid, &e_addr, &mode);

	if (valid != -1) {
		errp("cpr_claim_mem: Can't xlate _end %x\n", _end);
		return (-1);
	}
	if (e_addr >> 32)
		DEBUG4(errp("cpr_claim_mem: xlated _end e_addr %x%8x\n",
		(int)(e_addr >> 32), (int)(e_addr)))
	else
		DEBUG4(errp("cpr_claim_mem: xlated _end e_addr %x\n",
		(int)e_addr));

	/*
	 * cprbooter start and end pfns
	 */
	s_pfn = ADDR_TO_PN(s_addr);
	e_pfn = ADDR_TO_PN(e_addr);

	handled = 0;			/* haven't done cprbooter yet */
	while (getchunk(&lo_pfn, &hi_pfn)) {
		/*
		 * We need to extend any chunk that touches the image
		 * of cprbooter to include cprbooter as well, so as to keep
		 * the memory cprbooter occupies from being used by OBP
		 * for its own bookkeeping
		 * This requires lookahead in the case where we
		 * overlap or abut the beginning of cprbooter to make sure that
		 * if the next one overlaps or abuts the end we convert the
		 * request to one large contiguous one.
		 *
		 * If we've passed the cprbooter image without handling it, that
		 * means we need to sneak it in here because it did not touch
		 * another chunk
		 */

		if (!handled && hi_pfn < s_pfn - 1) {
			if (claim_chunk(s_pfn, e_pfn))
				return (-1);
			handled++;
		}
		/*
		 * If we've already done the cprbooter image
		 * or the chunk falls entirely above the cprbooter image
		 * or spans it completely
		 */
		if (handled || (lo_pfn > e_pfn + 1) ||
		    (hi_pfn >= e_pfn && lo_pfn <= s_pfn)) {
			if (claim_chunk(lo_pfn, hi_pfn))
				return (-1);
			if (!handled && (lo_pfn <= s_pfn))
				handled++;
		} else {
			/*
			 * If we're here there is overlap or abuttment
			 * but cprbooter image is not completely covered.
			 * Since we're processing from hi address, the max of
			 * the higher ends is the highest end of the enclosing
			 * space
			 */
			hi_extent = max(hi_pfn, e_pfn);		/* absolute */
			lo_extent = min(lo_pfn, s_pfn);		/* trial */
			/*
			 * while we haven't brought the low edge down to cover
			 * the cprbooter image, get a new low pfn
			 */
			while (lo_pfn > lo_extent &&
			    getchunk(&lo_pfn, &hi_pfn)) {
				if (hi_pfn < lo_extent - 1) {	/* now a gap */
					/*
					 * We've passed it and left a gap, so we
					 * have to claim two chunks
					 */
					if (claim_chunk(lo_extent, hi_extent))
						return (-1);
					/*
					 * now deal with the one we read ahead
					 */
					if (claim_chunk(lo_pfn, hi_pfn))
						return (-1);
					handled = 1;
				} else {
					lo_extent = min(lo_pfn, s_pfn);
				}
			}
			/*
			 * If we ran out of chunks without finding one below
			 * the bottom of cprbooter or we found one that exactly
			 * matches the bottom end
			 */
			if (lo_pfn >= lo_extent) {	/* ran out of chunks */
				if (claim_chunk(lo_extent, hi_extent))
					return (-1);
				handled = 1;
			}
		}
	}

	DEBUG4(errp("cpr_claim_mem: totpages %d claimed %d pages\n", totpages,
	    totpg));
	return (0);
}

/*
 * Get bootargs from prom and parse it.
 * For cprbooter, bootargs has to started with "-F"; otherwise, something is
 * wrong.
 */
void
check_bootargs()
{
	char *tp;

	/* Get bootargs from prom */
	tp = prom_bootargs();

	if (!tp || *tp == '\0') {
		errp("Null value in OBP boot-file, please reboot\n");
		return;
	}

	/* Starts with '-', has to be '-F' */
	if (*tp && (*tp == '-')) {
		if (*++tp == 'F') {
			skip_whitespc(tp);
			while (*tp && *tp != ' ') /* Skip over filename1 */
				tp++;
			skip_whitespc(tp);
			if (*tp && (*tp == '/')) {
				tp++;
				if (strcmp(tp, cpr_bootsec)) {
					errp("OBP boot-file value");
					errp(" is wrong, please reboot\n");
					return;
				}
			}
		}
	}
}

/*
 * This memory is never freed
 */
static caddr_t
cpr_alloc(int size)
{
	caddr_t retval;
	static caddr_t nextavail = 0;

	if (nextavail == 0)
		nextavail = (caddr_t)roundup((int)cpr_heap, 4);

	if (nextavail + size >= &cpr_heap[sizeof (cpr_heap)])
		return (0);

	retval = nextavail;
	nextavail = (caddr_t)roundup((int)nextavail + size, 4);

	return (retval);
}
/*
 * This function searches an array of bitmaps, storing in the callers buffers
 * information about the next contiguous set of bits set.  It returns true if
 * it found a chunk, else it returns false.
 *
 * It asserts that the bitmaps are sorted in descending order on start
 * pfn.
 */


static int
getchunk(u_longlong_t *lopfnp, u_longlong_t *hipfnp)
{
	static struct cpr_bitmap_desc *bdp;
	static int bit_offset;
	int endbit, found;
	static int cc = 0;	/* call count */

	if (cc++ == 0) {
		bdp = bitmap_desc;
		bit_offset = (bdp->cbd_size * NBBY) - 1;
	}
	/*
	 * ASSERT previous call stopped on a 0 bit, or a bitmap transition
	 */
	if (cc != 0 && bit_offset >= 0 && isset(bdp->cbd_bitmap, bit_offset)) {
		errp("getchunk: count %d, index %d, offset %d, bad bit set\n",
		    cc, bdp - bitmap_desc, bit_offset);
	}
	/*
	 * if previous call exhausted a bitmap
	 */
	if (bit_offset < 0) {
		/*
		 * if it exhausted the *last* bitmap
		 */
		if (++bdp > bitmap_desc + totbitmaps - 1)
			return (0);
		bit_offset = (bdp->cbd_size * NBBY) - 1;
		/*
		 * the promised assertion
		 */
		if (bdp->cbd_spfn >
		    (bdp - 1)->cbd_spfn + (bdp - 1)->cbd_size * NBBY - 1) {
			errp("getchunk: bitmaps not in correct sort order prev "
			    "(index %d) spf %x, size %x current (index %d) spf "
			    "%x\n", (bdp - 1) - bitmap_desc,
			    (bdp - 1)->cbd_spfn,
			    (bdp - 1)->cbd_size * NBBY - 1, bdp - bitmap_desc,
			    bdp->cbd_spfn);
		}
	}
	found = 0;
	/*
	 * scan past all 0 sequential bits
	 */
	while (found == 0) {
		while (bit_offset >= 0) {
			if (!isset(bdp->cbd_bitmap, bit_offset)) {
				bit_offset--;
			} else {
				found = 1;
				endbit = bit_offset--;
				break;
			}
		}
		/*
		 * if we exhausted a bitmap without finding a bit set
		 * then check the next one
		 */
		if (!found && bit_offset < 0) {
			/*
			 *  if we exhausted the *last* bitmap
			 */
			if (++bdp > bitmap_desc + totbitmaps - 1) {
				return (0);
			}
			bit_offset = (bdp->cbd_size * NBBY) - 1;
			/*
			 * the promised assertion again
			 */
			if (bdp->cbd_spfn >
			    (bdp - 1)->cbd_spfn +
			    (bdp - 1)->cbd_size * NBBY - 1) {
				errp("getchunk: bitmaps not in correct sort "
				    "order prev (index %d) spf %x, size %x "
				    "current (index %d) spf %x\n",
				    (bdp - 1) - bitmap_desc,
				    (bdp - 1)->cbd_spfn,
				    (bdp - 1)->cbd_size * NBBY - 1,
				    bdp - bitmap_desc, bdp->cbd_spfn);
			}
		}
	}
	while ((bit_offset >= 0) && isset(bdp->cbd_bitmap, bit_offset)) {
		bit_offset--;
	}
	/*
	 * if we ran off the end of the bitmap
	 */
	if (bit_offset < 0) {
		*lopfnp = bdp->cbd_spfn;
		*hipfnp = *lopfnp + endbit;
	} else {
		*lopfnp = bdp->cbd_spfn + bit_offset + 1;
		*hipfnp = *lopfnp + endbit - bit_offset - 1;
	}
	return (1);
}

static int
claim_chunk(u_longlong_t lo_pfn, u_longlong_t hi_pfn)
{
	extern int totpg;
	int error;
	int pages;
	u_longlong_t addr;
	struct prom_retain_info *rp;

	pages = (hi_pfn - lo_pfn + 1);
	addr = PN_TO_ADDR(lo_pfn);

	error = prom_claim_phys(pages * MMU_PAGESIZE, addr);
	/*
	 * Might have tried to claim the chunk that includes
	 * the prom_retain'd msgbuf.  If so, trim or split and
	 * try again
	 */
	if (error) {
		rp = m_info.retain;
		/*
		 * If msgbuf is a chunk by itself, or the high
		 * end of a chunk, or the low end, then we can
		 * cope easily.
		 * If it is in the middle, then we just have to
		 * cross our fingers and hope that there is already
		 * a free chunk behind us if the prom needs one.
		 * We know that these are the only choices, since the msgbuf
		 * will be represented in the bitmap and is contiguous.
		 */
		if (lo_pfn == rp->spfn && (rp->spfn + rp->cnt - 1) == hi_pfn) {
			DEBUG4(errp("Prom-retained: skipping %x, %x pages "
			    "totpg %d\n", (int)lo_pfn, (int)hi_pfn, totpg));
			return (0);
		} else if (lo_pfn <= rp->spfn &&
		    (rp->spfn + rp->cnt - 1) == hi_pfn) {
			pages -= rp->cnt;
			error = prom_claim_phys(pages * MMU_PAGESIZE, addr);
		} else if (lo_pfn == rp->spfn &&
		    (rp->spfn + rp->cnt - 1) < hi_pfn) {
			pages -= rp->cnt;
			addr += (rp->cnt * MMU_PAGESIZE);
			error = prom_claim_phys(pages * MMU_PAGESIZE, addr);
		} else if (lo_pfn < rp->spfn &&
		    hi_pfn > rp->spfn + rp->cnt - 1) {
			/*
			 * msgbuf is totally enclosed by the current
			 * chunk, we split into two and hope for
			 * the best
			 */
			pages = hi_pfn - (rp->spfn + rp->cnt) + 1;
			addr = PN_TO_ADDR(rp->spfn + rp->cnt);
			if (error = prom_claim_phys(pages * MMU_PAGESIZE,
			    addr)) {
				errp("cpr_claim_chunk: could not claim "
				    "hi part of chunk containing "
				    "prom_retain pages %x - %x\n",
				    rp->spfn + rp->cnt, (int)hi_pfn);
			} else {
				pages = rp->spfn - lo_pfn;
				addr = PN_TO_ADDR(lo_pfn);
				error = prom_claim_phys(pages * MMU_PAGESIZE,
				    addr);
			}
		}
	}
	if (error) {
		errp("cpr_claim_chunk: can't claim %x, %x totpg %d\n",
		    (int)lo_pfn, (int)hi_pfn, totpg);
		return (-1);
	} else {
		totpg += (hi_pfn - lo_pfn + 1);
		DEBUG4(errp("Claimed %x, %x pages totpg %d\n", (int)lo_pfn,
		    (int)hi_pfn, totpg));
		return (0);
	}
}
