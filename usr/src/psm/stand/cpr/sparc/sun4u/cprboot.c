/*
 * Copyright (c) 1994 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cprboot.c	1.25	96/10/15 SMI"

/*
 * cprboot - Second pass of the resume bootstrapping process,
 * it is responsible for:
 *	1. Opendebug.new.h cpr statefile /.CPR
 *	2. Read in and validate the ELF header of the statefile
 *	3. Read various headers and necessary information from the statefile
 *	4. Allocate enough virtual memory for the read kernel pages operation
 *	5. Read in kernel pages to their physical location
 *	6. Call cpr kernel entry procedure
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/promif.h>
#include <sys/elf.h>
#include <sys/pte.h>
#include <sys/cpr.h>
#include <sys/cpr_impl.h>
#include <sys/prom_isa.h>	/* for dnode_t */
#include <sys/prom_plat.h>

#define	roundup(x, y)	((((x)+((y)-1))/(y))*(y))

/*
 * External procedure declarations
 */
extern uint_t cpr_decompress(uchar_t *, uint_t, uchar_t *);
extern void cpr_dtlb_wr_entry(int, caddr_t, int, tte_t *);
extern void exit_to_kernel(char *, char *,
    int, int, char *, char *, u_int);
extern void cpr_itlb_wr_entry(int, caddr_t, int, tte_t *);
extern int open(char *, char *);

static void cprboot_spinning_bar();
static int cpr_skip_bitmaps(int);

/* Global variables */
static int totbitmaps, totpages;
static u_int mapva, free_va;	/* temp virtual addresses used for mapping */
static struct cpr_bitmap_desc *bitmap_desc;
static int compressed, no_compress;
static int good_pg;
static int spin_cnt = 0;
static u_int machdep_len;	/* total length of machdep info data */
static struct sun4u_machdep m_info;

int cpr_debug;

/* Following is for freeing our memory */
extern void _start();
extern	char _etext[];
extern	char _end[];		/* cprboot end address */
static	char dummy = 0;		/* dummy to find start of data segment */
#define	_sdata ((caddr_t) &dummy)

caddr_t    starttext, endtext, startdata, enddata;
caddr_t    newstack;
cpr_func_t kernelentry;

static int cprboot_cleanup_setup(caddr_t);
static void cpr_export_prom_words(caddr_t, size_t);


/*
 * cprboot pass in statefile path
 */
main(void *cookie, char *pathname, char *filesystem, caddr_t loadbase)
{
	cpr_func_t cpr_resume;
	caddr_t cpr_thread;
	struct cpr_terminator ct_tmp;	/* cprboot version of terminator */
	u_int cpr_pfn;
	caddr_t qsavp;
	int n_read = 0, rpg = 0;
	int fd, compress, i;
	caddr_t machdep_buf;

	prom_init("cprboot", cookie);

	DEBUG4(errp("\nstart 0x%x, 0x%x etext %x data %x end %x\n",
		(caddr_t) _start, loadbase, _etext, _sdata, _end));

	ct_tmp.tm_cprboot_start.tv_sec = prom_gettime() / 1000;

	if ((fd = open(pathname, filesystem)) == -1) {
		errp("Can't open %s on %s, please reboot\n",
		    pathname, filesystem);
		return (-1);
	}

	if (cpr_read_elf(fd) != 0)
		return (-1);

	if (cpr_read_cdump(fd, &cpr_resume, &cpr_thread,
		&cpr_pfn, &qsavp, &totbitmaps, &totpages) != 0)
		return (-1);

	if ((machdep_len = (u_int) cpr_get_machdep_len(fd)) == (int) -1)
		return (-1);

	if ((machdep_buf =
	    prom_alloc((caddr_t) 0, machdep_len, 0)) == (caddr_t) 0) {
		errp("Can't alloc machdep buffer\n");
		return (-1);
	}

	if (cpr_read_machdep(fd, machdep_buf, machdep_len) == -1)
		return (-1);

	m_info = *(struct sun4u_machdep *)machdep_buf;

	if (cpr_skip_bitmaps(fd) != 0)
		return (-1);

	/*
	 * Reserve prom memory to do mapping operations
	 */
	if ((mapva = (u_int)prom_map(0, 0, MMU_PAGESIZE)) == 0) {
		errp("prom_map operation failed, please reboot\n");
		return (-1);
	}

	if ((free_va = (u_int)prom_map(0, 0,
		(CPR_MAXCONTIG * MMU_PAGESIZE))) == 0) {
		errp("prom_map operation failed, please reboot\n");
		return (-1);
	}

	/*
	 * Read in pages
	 */
	while (good_pg < totpages) {
		if ((rpg = cpr_read_phys_page(fd, free_va, &compress)) != -1) {
			n_read++;
			good_pg += rpg;
			if (compress)
				compressed += rpg;
			else
				no_compress += rpg;
			spin_cnt++;
			if ((spin_cnt & 0x23) == 1)
				cprboot_spinning_bar();
			DEBUG4(errp("cpr_read_phys_page: read %d pages\n",
				rpg));
		} else {
			errp("read phy page error: read=%d good=%d total=%d\n",
				n_read, good_pg, totpages);
			errp("\n Please reboot\n");
			return (-1);
		}
	}

	DEBUG4(errp("Read=%d totpages=%d no_compress=%d compress=%d\n",
		good_pg, totpages, no_compress, compressed));

	if (cpr_read_terminator(fd, &ct_tmp, mapva) != 0)
		return (-1);

	(void) prom_close(fd);

	/*
	 * Map the kernel entry page
	 */
	if ((u_int)prom_map((caddr_t)(BASE_ADDR((u_int)cpr_resume)),
		PN_TO_ADDR(cpr_pfn), MMU_PAGESIZE) == 0) {
		errp("failed to map resume kernel page, please reboot\n");
		return (-1);
	}

	/* Restore dtlb entries */
	DEBUG4(errp("%d dtte entries %d itte entries\n",
		m_info.dtte_cnt, m_info.itte_cnt));

	for (i = 0; i < m_info.dtte_cnt; i++) {
		u_longlong_t pa;
		caddr_t va;
		u_int size;

#define	PFN_TO_ADDR(x)	((u_longlong_t)(x) << MMU_PAGESHIFT)

		va = m_info.dtte[i].va_tag;
		pa = PFN_TO_ADDR(TTE_TO_TTEPFN(&m_info.dtte[i].tte));
		size = TTEBYTES(m_info.dtte[i].tte.tte_size);
		if (prom_map_phys(-1, size, va, pa) != 0) {
			errp("Unable to prom map %x %x\n", va, size);
		}
	}

	for (i = 0; i < m_info.dtte_cnt; i++) {
		DEBUG4(errp("%d dtlb no %d va_tag %x ctx %x tte hi %x lo %x\n",
			i, m_info.dtte[i].no, m_info.dtte[i].va_tag,
			m_info.dtte[i].ctx, m_info.dtte[i].tte.tte_inthi,
			m_info.dtte[i].tte.tte_intlo));

		cpr_dtlb_wr_entry((m_info.dtte[i].no),
			m_info.dtte[i].va_tag, m_info.dtte[i].ctx,
			&(m_info.dtte[i].tte));
			DEBUG4(errp("Stuffed dtte %d\n",
				m_info.dtte[i].no));
	}

	for (i = 0; i < m_info.itte_cnt; i++) {
		DEBUG4(errp("%d itlb no %d va_tag %x ctx %x tte hi %x lo %x\n",
			i, m_info.itte[i].no, m_info.itte[i].va_tag,
			m_info.itte[i].ctx, m_info.itte[i].tte.tte_inthi,
			m_info.itte[i].tte.tte_intlo));

		cpr_itlb_wr_entry((m_info.itte[i].no),
			m_info.itte[i].va_tag, m_info.itte[i].ctx,
			&(m_info.itte[i].tte));

	}

	/* Map curthread and qsav */
	if ((u_int)prom_map((caddr_t)(BASE_ADDR((u_int)cpr_thread)),
		PN_TO_ADDR(m_info.curt_pfn),
			MMU_PAGESIZE)  == 0) {
		DEBUG4(errp("failed to map cpr_thread\n"));
		errp("prom_map operation failed, please reboot\n");
		return (-1);
	}

	if ((u_int)prom_map((caddr_t)(BASE_ADDR((u_int)qsavp)),
		PN_TO_ADDR(m_info.qsav_pfn),
			MMU_PAGESIZE)  == 0) {
		DEBUG4(errp("failed to map qsavp\n"));
		errp("prom_map operation failed, please reboot\n");
		return (-1);
	}

	cpr_export_prom_words(machdep_buf, machdep_len);
	prom_free(machdep_buf, machdep_len);

	DEBUG4(errp("Ready to jump: resume pc %x pfn %x tp %x qsavp %x ",
		cpr_resume, cpr_pfn, cpr_thread, qsavp));

	DEBUG4(errp("pctx %x sctx %x tra_va %x "
	    "mapbuf_va %x mapbuf_pfn %x mapbuf_size %x\n",
		m_info.mmu_ctx_pri, m_info.mmu_ctx_sec, m_info.tra_va,
	    m_info.mapbuf_va, m_info.mapbuf_pfn, m_info.mapbuf_size));

	/* must do before reading the prom mappings */
	if (cprboot_cleanup_setup(loadbase) == 0)
		return (-1);

	if ((u_int) prom_map(m_info.mapbuf_va, PN_TO_ADDR(m_info.mapbuf_pfn),
	    m_info.mapbuf_size) == 0) {
		errp("failed to map translation buffer, please reboot\n");
		return ((size_t) -1);
	}

	kernelentry = cpr_resume;
	DEBUG1(errp("Jumping to kernel <%x> <%x, %x> <%x, %x>\n", kernelentry,
	    starttext, endtext, startdata, enddata));

	/*
	 * call cpr kernel entry procedure
	 */
	exit_to_kernel(cpr_thread, qsavp, m_info.mmu_ctx_pri,
	    m_info.mmu_ctx_sec, m_info.tra_va, m_info.mapbuf_va,
	    m_info.mapbuf_size);

	return (0);
}

static int
cprboot_cleanup_setup(caddr_t loadbase)
{
	caddr_t	page;

	page = prom_alloc((caddr_t) 0, MMU_PAGESIZE, MMU_PAGESIZE);
	if (page == (caddr_t) 0) {
		errp("cpr_cleanup: unable to allocate page, please reboot\n");
		return (0);
	}

	if (loadbase == (caddr_t) 0)
		loadbase = (caddr_t) _start;

	starttext = (caddr_t) ((u_int) loadbase & ~(MMU_PAGESIZE-1));
	endtext = (caddr_t) (((u_int)_etext + MMU_PAGESIZE-1) &
		~(MMU_PAGESIZE-1));

	startdata = (caddr_t) ((u_int)_sdata & ~(MMU_PAGESIZE-1));
	enddata = (caddr_t) (((u_int)_end + MMU_PAGESIZE-1) &
		~(MMU_PAGESIZE-1));

	newstack = page + MMU_PAGESIZE;

	DEBUG4(errp("<%x, %x> <%x, %x> <%x>\n",
		starttext, endtext, startdata, enddata, newstack));

	return (1);
}


/*
 * For sun4u, I don't think we need the bitmap anymore, since we have
 * claimed all the memory that we need already.
 * Instead of reading the bitmaps, we just do seek to skip them.
 * No more prom_alloc() need to be done for the actual bitmaps.
 */
static int
cpr_skip_bitmaps(int fd)
{
	int i, bitmapsize;
	unsigned long long offset;
	cbd_t *bp;

	/*
	 * file offset from start of file
	 * If there is machdep info, add size of machdep info to offset.
	 */
	offset = sizeof (Elf32_Ehdr) + sizeof (cdd_t) + sizeof (cmd_t) +
		machdep_len;

	bitmap_desc = (cbd_t *)prom_alloc((caddr_t)0,
		totbitmaps * sizeof (cbd_t), 0);

	for (i = 0; i < totbitmaps; i++) {
		bp = &bitmap_desc[i];
		if (prom_read(fd, (caddr_t)bp, sizeof (cbd_t), 0, 0)
		    < sizeof (cbd_t)) {
			errp("cpr_read_bitmap: err reading bitmap des %d\n", i);
			return (-1);
		}

		if (bp->cbd_magic != CPR_BITMAP_MAGIC) {
			errp("cpr_read_bitmap: bitmap %d BAD MAGIC %x\n", i,
				bp->cbd_magic);
			return (-1);
		}

		bitmapsize = bp->cbd_size;

		offset = offset + sizeof (cbd_t) + bitmapsize;

		DEBUG4(errp("cpr_skip_bitmaps: seek to offset %llu\n", offset));

		if (prom_seek(fd, offset) == -1) {
			errp("cpr_skip_bitmaps: bitmap %d seek error\n", i);
			return (-1);
		}
	}

	return (0);
}

static void
cprboot_spinning_bar()
{
	static int spinix;

	switch (spinix) {
	case 0:
		prom_printf("|\b");
		break;
	case 1:
		prom_printf("/\b");
		break;
	case 2:
		prom_printf("-\b");
		break;
	case 3:
		prom_printf("\\\b");
		break;
	}
	if ((++spinix) & 0x4) spinix = 0;
}

/*
 * During early startup of a normal (non-cpr) boot, the kernel uses
 * prom_interpret() in several places to define some Forth words to the
 * prom.  We must duplicate those definitions in this standalone boot
 * program before taking the snapshot of the prom's translations.  This
 * can't be done in the resumed kernel because it might change the
 * prom's mappings.
 */
static void
cpr_export_prom_words(caddr_t buf, size_t length)
{
	buf += sizeof (struct sun4u_machdep);
	length -= sizeof (struct sun4u_machdep);

	/*
	 * The variable length machdep section for sun4u consists
	 * of a sequence of null-terminated strings stored contiguously.
	 *
	 * The first string defines Forth words which help the prom
	 * handle kernel translations.
	 *
	 * The second string defines Forth words required by kadb to
	 * interface with the prom when a trap is taken.
	 */
	while (length) {
		size_t str_len;

		prom_interpret(buf, 0, 0, 0, 0, 0);
		str_len = strlen(buf) + 1;	/* include the null */
		length -= str_len;
		buf += str_len;
	}
}
