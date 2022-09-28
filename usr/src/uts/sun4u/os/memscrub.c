/*
 * Copyright (c) 1993-1996 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)memscrub.c 1.18	96/10/17 SMI"

/*
 * sun4u Memory Scrubbing
 *
 * On detection of a correctable memory ECC error, the sun4u kernel
 * returns the corrected data to the requester and re-writes it
 * to memory (DRAM).  So if the correctable error was transient,
 * the read has effectively been cleaned (scrubbed) from memory.
 *
 * Scrubbing thus reduces the likelyhood that multiple transient errors
 * will occur in the same memory word, making uncorrectable errors due
 * to transients less likely.
 *
 * Thus is born the desire that every memory location be periodically
 * accessed.
 *
 * This file implements a memory scrubbing thread.  This scrubber
 * guarantees that all of physical memory is accessed periodically
 * (memscrub_period_sec -- 12 hours).
 *
 * It attempts to do this as unobtrusively as possible.  The thread
 * schedules itself to wake up at an interval such that if it reads
 * memscrub_span_pages (8MB) on each wakeup, it will read all of physical
 * memory in in memscrub_period_sec (12 hours).
 *
 * The scrubber uses the block load hardware to read memory @ 268MB/s,
 * so it reads spans of 8MB in 0.03 seconds.  Unlike the original sun4d
 * scrubber the sun4u scrubber does not read ahead if the system is idle
 * because we can read memory very efficently.
 *
 * Maximal Cost Estimate:  30GB @ 268MB/s = 112 seconds spent in 3750 wakeups
 * that run for 0.03 seconds at intervals of 12 seconds.
 *
 * The scrubber maintains a private copy of the phys_install memory list
 * to keep track of what memory should be scrubbed.
 *
 * The global routines memscrub_add_span() and memscrub_delete_span() are
 * used to add and delete from this list.  If hotplug memory is later
 * supported these two routines can be used to notify the scrubber of
 * memory configuration changes.
 *
 * The following parameters can be set via /etc/system
 *
 * memscrub_span_pages = MEMSCRUB_DFL_SPAN_PAGES (8MB)
 * memscrub_period_sec = MEMSCRUB_DFL_PERIOD_SEC (12 hours)
 * memscrub_thread_pri = MEMSCRUB_DFL_THREAD_PRI (MINCLSYSPRI)
 * memscrub_delay_start_sec = (5 minutes)
 * memscrub_verbose = (0)
 * disable_memscrub = (0)
 * pause_memscrub = (0)
 * read_all_memscrub = (0)
 *
 * The scrubber will print NOTICE messages of what it is doing if
 * "memscrub_verbose" is set.
 *
 * The scrubber will exit (or never be started) if it finds the variable
 * "disable_memscrub" set.
 *
 * The scrubber will pause (not read memory) when "pause_memscrub"
 * is set.  It will check the state of pause_memscrub at each wakeup
 * period.  The scrubber will not make up for lost time.  If you
 * pause the scrubber for a prolonged period of time you can use
 * the "read_all_memscrub" switch (see below) to catch up.
 *
 * The scrubber will read all memory if "read_all_memscrub" is set.
 * The normal span read will also occur during the wakeup.
 *
 * MEMSCRUB_MIN_PAGES (32MB) is the minimum amount of memory a system
 * must have before we'll start the scrubber.
 *
 * MEMSCRUB_DFL_SPAN_PAGES (8MB) is based on the guess that 0.03 sec
 * is a "good" amount of minimum time for the thread to run at a time.
 *
 * MEMSCRUB_DFL_PERIOD_SEC (12 hours) is nearly a total guess --
 * twice the frequency the hardware folk estimated would be necessary.
 *
 * MEMSCRUB_DFL_THREAD_PRI (MINCLSYSPRI) is based on the assumption
 * that the scurbber should get its fair share of time (since it
 * is short).  At a priority of 0 the scrubber will be starved.
 */

#include <sys/systm.h>		/* timeout, types, t_lock */
#include <sys/cmn_err.h>
#include <sys/sysmacros.h>	/* MIN */
#include <sys/memlist.h>	/* memlist */
#include <sys/kmem.h>		/* KMEM_NOSLEEP */
#include <sys/cpuvar.h>		/* ncpus_online */
#include <sys/debug.h>		/* ASSERTs */
#include <sys/machsystm.h>	/* lddphys */
#include <sys/cpu_module.h>	/* vtag_flushpage */

#include <vm/hat.h>
#include <vm/seg_kmem.h>
#include <vm/hat_sfmmu.h>		/* XXX FIXME - delete */

#include <sys/time.h>

/*
 * External Data:
 */
extern struct memlist *phys_install;

/*
 * Global Routines:
 */
int memscrub_add_span(u_int pfn, u_int pages);
int memscrub_delete_span(u_int pfn, u_int bytes);
int memscrub_init(void);

/*
 * Global Data:
 */

/*
 * scrub if we have at least this many pages
 */
#define	MEMSCRUB_MIN_PAGES (32 * 1024 * 1024 / PAGESIZE)

/*
 * scan all of physical memory at least once every MEMSCRUB_PERIOD_SEC
 */
#define	MEMSCRUB_DFL_PERIOD_SEC	(12 * 60 * 60)	/* 12 hours */

/*
 * scan at least MEMSCRUB_DFL_SPAN_PAGES each iteration
 */
#define	MEMSCRUB_DFL_SPAN_PAGES	((8 * 1024 * 1024) / PAGESIZE)

/*
 * almost anything is higher priority than scrubbing
 */
#define	MEMSCRUB_DFL_THREAD_PRI	MINCLSYSPRI

/*
 * size used when scanning memory
 */
#define	MEMSCRUB_BLOCK_SIZE		256
#define	MEMSCRUB_BLOCK_SIZE_SHIFT	8 	/* log2(MEMSCRUB_BLOCK_SIZE) */
#define	MEMSCRUB_BLOCKS_PER_PAGE	(PAGESIZE >> MEMSCRUB_BLOCK_SIZE_SHIFT)

#define	MEMSCRUB_BPP4M		MMU_PAGESIZE4M >> MEMSCRUB_BLOCK_SIZE_SHIFT
#define	MEMSCRUB_BPP512K	MMU_PAGESIZE512K >> MEMSCRUB_BLOCK_SIZE_SHIFT
#define	MEMSCRUB_BPP64K		MMU_PAGESIZE64K >> MEMSCRUB_BLOCK_SIZE_SHIFT
#define	MEMSCRUB_BPP		MMU_PAGESIZE >> MEMSCRUB_BLOCK_SIZE_SHIFT

/*
 * we can patch these defaults in /etc/system if necessary
 */
u_int disable_memscrub = 0;
u_int pause_memscrub = 0;
u_int read_all_memscrub = 0;
u_int memscrub_verbose = 0;
u_int memscrub_all_idle = 0;
u_int memscrub_span_pages = MEMSCRUB_DFL_SPAN_PAGES;
u_int memscrub_period_sec = MEMSCRUB_DFL_PERIOD_SEC;
u_int memscrub_thread_pri = MEMSCRUB_DFL_THREAD_PRI;
u_int memscrub_delay_start_sec = 5 * 60;

/*
 * Static Routines
 */
static void memscrubber(void);
static void memscrub_cleanup(void);
static int memscrub_verify_span(u_longlong_t *addrp, u_int *pagesp);
static void memscrub_scan(u_int blks, u_longlong_t src);

/*
 * Static Data
 */

static struct memlist *memscrub_memlist;
static u_int memscrub_phys_pages;

static kcondvar_t memscrub_cv;
static kmutex_t memscrub_lock;
/*
 * memscrub_lock protects memscrub_memlist, interval_sec, ...
 */

#ifdef DEBUG
u_int memscrub_scans_regular;
u_int memscrub_scans_idle;

u_int memscrub_done_early;
u_int memscrub_early_sec;

u_int memscrub_done_late;
u_int memscrub_late_sec;

u_int memscrub_interval_sec;
#endif /* DEBUG */

/*
 * create memscrub_memlist from phys_install list
 * initialize locks, set memscrub_phys_pages.
 */
int
memscrub_init(void)
{
	struct memlist *src;
	extern int physmem;

	/*
	 * only startup the scrubber if we have a minimum
	 * number of pages
	 */
	if (physmem >= MEMSCRUB_MIN_PAGES) {

		/*
		 * initialize locks
		 */
		mutex_init(&memscrub_lock, "memscrub mutex",
			MUTEX_DRIVER, NULL);
		cv_init(&memscrub_cv, "memscrub cv", CV_DRIVER, NULL);

		/*
		 * copy phys_install to memscrub_memlist
		 */
		for (src = phys_install; src; src = src->next) {
			if (memscrub_add_span(
				    (u_int) (src->address >> PAGESHIFT),
				    (u_int) (src->size >> PAGESHIFT))) {
				memscrub_cleanup();
				return (-1);
			}
		}


		/*
		 * create memscrubber thread
		 */
		if (thread_create(NULL, PAGESIZE, (void (*)())memscrubber,
		    0, 0, &p0, TS_RUN,  memscrub_thread_pri) == NULL) {
			cmn_err(CE_WARN, "unable to create memscrubber()");
			memscrub_cleanup();
			return (-1);
		}
	}

	return (0);
}

static void
memscrub_cleanup(void)
{
	while (memscrub_memlist) {
		memscrub_delete_span((memscrub_memlist->address >> PAGESHIFT),
		    (memscrub_memlist->size >> PAGESHIFT));
	}
	cv_destroy(&memscrub_cv);
	mutex_destroy(&memscrub_lock);
}

#ifdef MEMSCRUB_DEBUG
static void
memscrub_printmemlist(char *title, struct memlist *listp)
{
	struct memlist *list;

	cmn_err(CE_CONT, "%s:\n", title);

	for (list = listp; list; list = list->next) {
		cmn_err(CE_CONT, "addr = 0x%llx, size = 0x%llx\n",
		    list->address, list->size);
	}
}
#endif /* MEMSCRUB_DEBUG */

/* ARGSUSED */
static void
memscrub_wakeup(caddr_t c)
{
	/*
	 * grab mutex to guarantee that our wakeup call
	 * arrives after we go to sleep -- so we can't sleep forever.
	 */
	mutex_enter(&memscrub_lock);
	cv_signal(&memscrub_cv);
	mutex_exit(&memscrub_lock);
}

/*
 * this calculation doesn't account for the time
 * that the actual scan consumes -- so we'd fall
 * slightly behind schedule with this interval_sec.
 * It's very small.
 */

static int
compute_interval_sec(void)
{
	/*
	 * We use msp_safe mpp_safe below to insure somebody
	 * doesn't set it to 0 on us.
	 */
	static u_int msp_safe, mpp_safe;
	static int interval_sec;
	msp_safe = memscrub_span_pages;
	mpp_safe = memscrub_phys_pages;
	interval_sec = memscrub_period_sec;

	ASSERT(mutex_owned(&memscrub_lock));

	if ((msp_safe != 0) && (mpp_safe != 0)) {
		if (memscrub_phys_pages <= msp_safe)
		    interval_sec = memscrub_period_sec;
		else
		    interval_sec = (memscrub_period_sec/(mpp_safe/msp_safe));
	}
	return (interval_sec);
}

void
memscrubber(void)
{
	u_longlong_t address, addr;
	time_t deadline;
	u_int pages;
	u_int reached_end = 1;
	u_int paused_message = 0;
	int interval_sec = 0;

	if (memscrub_memlist == NULL) {
		cmn_err(CE_WARN, "memscrub_memlist not initialized.");
		goto memscrub_exit;
	}

	address = memscrub_memlist->address;

	deadline = hrestime.tv_sec + memscrub_delay_start_sec;

	for (;;) {
		if (disable_memscrub)
			break;

		mutex_enter(&memscrub_lock);

		/*
		 * compute interval_sec
		 */
		interval_sec = compute_interval_sec();

#ifdef DEBUG
		memscrub_interval_sec = interval_sec;
#endif /* DEBUG */

		/*
		 * did we just reach the end of memory?
		 */
		if (reached_end) {
			time_t now = hrestime.tv_sec;

			if (now >= deadline) {
#ifdef DEBUG
				memscrub_done_late++;
				memscrub_late_sec += (now - deadline);
#endif /* DEBUG */
				/*
				 * past deadline, start right away
				 */
				interval_sec = 0;

				deadline = now + memscrub_period_sec;
			} else {
				/*
				 * we finished ahead of schedule.
				 * wait till previous dealine before re-start.
				 */
				interval_sec = deadline - now;
#ifdef DEBUG
				memscrub_done_early++;
				memscrub_early_sec += interval_sec;
#endif /* DEBUG */
				deadline += memscrub_period_sec;
			}
			reached_end = 0;
		}

		if (interval_sec != 0) {
			/*
			 * hit the snooze bar
			 */
			(void) timeout(memscrub_wakeup, NULL,
				interval_sec * hz);

			/*
			 * go to sleep
			 */
			cv_wait(&memscrub_cv, &memscrub_lock);
		}

		mutex_exit(&memscrub_lock);

		if (memscrub_phys_pages == 0) {
			cmn_err(CE_WARN, "Memory scrubber has 0 pages to read");
			goto memscrub_exit;
		}

		if (!pause_memscrub) {
			if (paused_message) {
				paused_message = 0;
				if (memscrub_verbose)
					cmn_err(CE_NOTE,
						"Memory scrubber resuming");
			}

			if (read_all_memscrub) {
				if (memscrub_verbose)
					cmn_err(CE_NOTE, "Memory scrubber "
					    "reading all memory per request");

				addr = memscrub_memlist->address;
				reached_end = 0;
				while (!reached_end) {
					pages = memscrub_phys_pages;
					if (disable_memscrub)
						break;
					mutex_enter(&memscrub_lock);
					reached_end
						= memscrub_verify_span(&addr,
							&pages);
					memscrub_scan(pages
						* MEMSCRUB_BLOCKS_PER_PAGE,
						addr);
					addr += ((u_longlong_t)pages
						* PAGESIZE);
					mutex_exit(&memscrub_lock);
				}
				read_all_memscrub = 0;
			}

			/*
			 * read 1 span
			 */
			pages = memscrub_span_pages;

			if (disable_memscrub)
				break;

			mutex_enter(&memscrub_lock);
			/*
			 * determine physical address range
			 */
			reached_end = memscrub_verify_span(&address,
				&pages);

			memscrub_scan(pages
				* MEMSCRUB_BLOCKS_PER_PAGE, address);

			address = address + ((u_longlong_t)pages
				* PAGESIZE);
			mutex_exit(&memscrub_lock);
		}

		if (pause_memscrub && !paused_message) {
			paused_message = 1;
			if (memscrub_verbose)
				cmn_err(CE_NOTE, "Memory scrubber paused");
		}
	}

memscrub_exit:
	cmn_err(CE_NOTE, "Memory scrubber exiting");
	memscrub_cleanup();
	thread_exit();
}

/*
 * condition address and size
 * such that they span legal physical addresses.
 *
 * when appropriate, address will be rounded up to start of next
 * struct memlist, and pages will be rounded down to the end of the
 * memlist size.
 *
 * returns 1 if reached end of list, else returns 0.
 */
static int
memscrub_verify_span(u_longlong_t *addrp, u_int *pagesp)
{
	struct memlist *mlp;
	u_longlong_t address = *addrp;
	u_longlong_t bytes = (u_longlong_t)*pagesp * PAGESIZE;
	u_longlong_t bytes_remaining;
	int reached_end = 0;

	ASSERT(mutex_owned(&memscrub_lock));

	/*
	 * find memlist struct that contains addrp
	 * assumes memlist is sorted by ascending address.
	 */
	for (mlp = memscrub_memlist; mlp; mlp = mlp->next) {
		/*
		 * if before this chunk, round up to beginning
		 */
		if (address < mlp->address) {
			address = mlp->address;
			break;
		}
		/*
		 * if before end of chunk, then we found it
		 */
		if (address < (mlp->address + mlp->size)) {
			break;
		}

		/* else go to next struct memlist */
	}
	/*
	 * if we hit end of list, start at beginning
	 */
	if (mlp == NULL) {
		mlp = memscrub_memlist;
		address = mlp->address;
	}

	/*
	 * now we have legal address, and its mlp, condition bytes
	 */
	bytes_remaining = (mlp->address + mlp->size) - address;

	if (bytes > bytes_remaining)
		bytes = bytes_remaining;

	/*
	 * will this span take us to end of list?
	 */
	if ((mlp->next == NULL) &&
	    ((mlp->address + mlp->size) == (address + bytes)))
		reached_end = 1;

	/* return values */
	*addrp = address;
	*pagesp = bytes/PAGESIZE;

	return (reached_end);
}

/*
 * add a span to the memscrub list
 * add to memscrub_phys_pages
 */
int
memscrub_add_span(u_int pfn, u_int pages)
{
	u_longlong_t address = (u_longlong_t)pfn << PAGESHIFT;
	u_longlong_t bytes = (u_longlong_t)pages << PAGESHIFT;
	struct memlist *dst;
	struct memlist *prev, *next;
	int retval = 0;

	mutex_enter(&memscrub_lock);

#ifdef MEMSCRUB_DEBUG
	memscrub_printmemlist("memscrub_memlist before", memscrub_memlist);
	cmn_err(CE_CONT, "memscrub_phys_pages: 0x%x\n", memscrub_phys_pages);
	cmn_err(CE_CONT, "memscrub_add_span: address: 0x%llx"
		" size: 0x%llx\n", address, bytes);
#endif /* MEMSCRUB_DEBUG */

	/*
	 * allocate a new struct memlist
	 */

	dst = (struct memlist *)
	    kmem_alloc(sizeof (struct memlist), KM_NOSLEEP);

	if (dst == NULL) {
		retval = -1;
		goto add_done;
	}

	dst->address = address;
	dst->size = bytes;

	/*
	 * first insert
	 */
	if (memscrub_memlist == NULL) {
		dst->prev = NULL;
		dst->next = NULL;
		memscrub_memlist = dst;

		goto add_done;
	}

	/*
	 * insert into sorted list
	 */
	for (prev = NULL, next = memscrub_memlist;
	    next;
	    prev = next, next = next->next) {
		if (address > next->address + next->size)
			continue;

		/*
		 * else insert here
		 */

		/*
		 * prepend to next
		 */
		if ((address + bytes) == next->address) {
			kmem_free(dst, sizeof (struct memlist));

			next->address = address;
			next->size += bytes;

			goto add_done;
		}

		/*
		 * append to next
		 */
		if (address == (next->address + next->size)) {
			kmem_free(dst, sizeof (struct memlist));

			if (next->next) {
				/*
				 * don't overlap with next->next
				 */
				if ((address + bytes) > next->next->address) {
					retval = -1;
					goto add_done;
				}
				/*
				 * concatenate next and next->next
				 */
				if ((address + bytes) == next->next->address) {
					struct memlist *mlp = next->next;

					if (next == memscrub_memlist)
						memscrub_memlist = next->next;

					mlp->address = next->address;
					mlp->size += next->size;
					mlp->size += bytes;

					if (next->prev)
						next->prev->next = mlp;
					mlp->prev = next->prev;

					kmem_free(next,
						sizeof (struct memlist));
					goto add_done;
				}
			}

			next->size += bytes;

			goto add_done;
		}

		/*
		 * insert before first
		 */
		if (prev == NULL) {
			dst->prev = NULL;
			dst->next = memscrub_memlist;

			memscrub_memlist = dst;
			next->prev = dst;
			goto add_done;
		} else {
		/*
		 * insert between prev and next
		 */
			/* don't overlap with next */
			if (address + bytes > next->address) {
				retval = -1;
				kmem_free(dst, sizeof (struct memlist));
				goto add_done;
			}

			dst->prev = prev;
			dst->next = next;

			prev->next = dst;
			next->prev = dst;
			goto add_done;
		}
	}	/* end for */

	/*
	 * end of list, prev is valid and next is NULL
	 */
	prev->next = dst;
	dst->prev = prev;
	dst->next = NULL;

add_done:

	if (retval != -1)
		memscrub_phys_pages += pages;


#ifdef MEMSCRUB_DEBUG
	memscrub_printmemlist("memscrub_memlist after", memscrub_memlist);
	cmn_err(CE_CONT, "memscrub_phys_pages: 0x%x\n", memscrub_phys_pages);
#endif /* MEMSCRUB_DEBUG */

	mutex_exit(&memscrub_lock);
	return (retval);
}

/*
 * delete a span from the memscrub list
 * subtract from memscrub_phys_pages
 */
int
memscrub_delete_span(u_int pfn, u_int pages)
{
	u_longlong_t address = (u_longlong_t)pfn << PAGESHIFT;
	u_longlong_t bytes = (u_longlong_t)pages << PAGESHIFT;
	struct memlist *dst, *next;
	int retval = 0;

	mutex_enter(&memscrub_lock);

#ifdef MEMSCRUB_DEBUG
	memscrub_printmemlist("memscrub_memlist Before", memscrub_memlist);
	cmn_err(CE_CONT, "memscrub_phys_pages: 0x%x\n", memscrub_phys_pages);
	cmn_err(CE_CONT, "memscrub_delete_span: 0x%llx 0x%llx\n",
		    address, bytes);
#endif /* MEMSCRUB_DEBUG */

	/*
	 * find struct memlist containing page
	 */
	for (next = memscrub_memlist; next; next = next->next) {
		if ((address >= next->address) &&
		    (address < next->address + next->size))
			break;
	}

	/*
	 * if start address not in list
	 */
	if (next == NULL) {
		retval = -1;
		goto delete_done;
	}

	/*
	 * error if size goes off end of this struct memlist
	 */
	if (address + bytes > next->address + next->size) {
		retval = -1;
		goto delete_done;
	}

	/*
	 * pages at beginning of struct memlist
	 */
	if (address == next->address) {
		/*
		 * if start & size match, delete from list
		 */
		if (bytes == next->size) {
			if (next == memscrub_memlist)
				memscrub_memlist = next->next;
			if (next->prev != NULL)
				next->prev->next = next->next;
			if (next->next != NULL)
				next->next->prev = next->prev;

			kmem_free(next, sizeof (struct memlist));
		} else {
		/*
		 * increment start address by bytes
		 */
			next->address += bytes;
			next->size -= bytes;
		}
		goto delete_done;
	}

	/*
	 * pages at end of struct memlist
	 */
	if (address + bytes == next->address + next->size) {
		/*
		 * decrement size by bytes
		 */
		next->size -= bytes;
		goto delete_done;
	}

	/*
	 * delete a span in the middle of the struct memlist
	 */
	{
		/*
		 * create a new struct memlist
		 */
		dst = (struct memlist *)
		    kmem_alloc(sizeof (struct memlist), KM_NOSLEEP);

		if (dst == NULL) {
			retval = -1;
			goto delete_done;
		}

		/*
		 * existing struct memlist gets address
		 * and size up to pfn
		 */
		dst->address = address + bytes;
		dst->size = (next->address + next->size) - dst->address;
		next->size = address - next->address;

		/*
		 * new struct memlist gets address starting
		 * after pfn, until end
		 */

		/*
		 * link in new memlist after old
		 */
		dst->next = next->next;
		dst->prev = next;

		if (next->next != NULL)
			next->next->prev = dst;
		next->next = dst;
	}

delete_done:
	if (retval != -1) {
		memscrub_phys_pages -= pages;
		if (memscrub_phys_pages == 0)
			disable_memscrub = 1;
	}

#ifdef MEMSCRUB_DEBUG
	memscrub_printmemlist("memscrub_memlist After", memscrub_memlist);
	cmn_err(CE_CONT, "memscrub_phys_pages: 0x%x\n", memscrub_phys_pages);
#endif /* MEMSCRUB_DEBUG */

	mutex_exit(&memscrub_lock);
	return (retval);
}

static void
memscrub_scan(u_int blks, u_longlong_t src)
{
	extern void memscrub_read(caddr_t src, u_int blks);

	u_int 		cpu, pfn, sz, psz, bpp, pgsread;
	u_longlong_t	pa;
	caddr_t		va;
	tte_t		tte;

	ASSERT(mutex_owned(&memscrub_lock));

	pgsread = 0;
	pa = src;
	while (blks != 0) {
		if (((pa & MMU_PAGEMASK4M) == pa) &&
			(blks >= MEMSCRUB_BPP4M)) {
			sz = TTE4M;
			psz = MMU_PAGESIZE4M;
			bpp = MEMSCRUB_BPP4M;
		} else if (((pa & MMU_PAGEMASK512K) == pa) &&
			(blks >= MEMSCRUB_BPP512K)) {
			sz = TTE512K;
			psz = MMU_PAGESIZE512K;
			bpp = MEMSCRUB_BPP512K;
		} else if (((pa & MMU_PAGEMASK64K) == pa) &&
			(blks >= MEMSCRUB_BPP64K)) {
			sz = TTE64K;
			psz = MMU_PAGESIZE64K;
			bpp = MEMSCRUB_BPP64K;
		} else if ((pa & MMU_PAGEMASK) == pa) {
			sz = TTE8K;
			psz = MMU_PAGESIZE;
			bpp = MEMSCRUB_BPP;
		} else {
			if (memscrub_verbose) {
				cmn_err(CE_NOTE,
				"Memory scrubber ignoring non-page aligned "
				"block starting at 0x%llx", src);
			}
			return;
		}
		if (blks < bpp) bpp = blks;

#ifdef MEMSCRUB_DEBUG
		cmn_err(CE_NOTE, "Going to run sz=%x, psz=%x, "
			"bpp=%x pa=%llx\n", sz, psz, bpp, pa);
#endif /* MEMSCRUB_DEBUG */

		/*
		 * MEMSCRUBBASE is a 4MB aligned page in the
		 * kernel so that we can quickly map the PA
		 * to a VA for the block loads performed in
		 * memscrub_read.  The basic logic is taken
		 * from ppcopy() in os/ppage.c.
		 */
		kpreempt_disable();
		cpu = CPU->cpu_id;
		pfn = pa >> MMU_PAGESHIFT;
		va = (caddr_t)MEMSCRUBBASE;
		tte.tte_inthi = TTE_VALID_INT | TTE_SZ_INT(sz) |
			TTE_PFN_INTHI(pfn);
		tte.tte_intlo = TTE_PFN_INTLO(pfn) | TTE_CP_INT |
			TTE_CV_INT | TTE_PRIV_INT | TTE_LCK_INT;
		ASSERT(CPU->cpu_id == cpu);
		sfmmu_dtlb_ld(va, KCONTEXT, &tte);
		memscrub_read(va, bpp);
		ASSERT(CPU->cpu_id == cpu);
		vtag_flushpage(va, KCONTEXT); /* sfmmu_tlbflush_page */
		kpreempt_enable();

		blks -= bpp;
		pa += psz;
		pgsread++;
	}
	if (memscrub_verbose) {
		cmn_err(CE_NOTE, "Memory scrubber read 0x%x pages starting "
			"at 0x%llx", pgsread, src);
	}
}
