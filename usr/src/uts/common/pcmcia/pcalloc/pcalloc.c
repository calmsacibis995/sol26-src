/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident "@(#)pcalloc.c	1.13	96/05/31 SMI"

/*
 * pcalloc is a prototype resource allocator
 * for Solaris. It is expected to go away once
 * a real one is available.
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/map.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/kmem.h>
#include <sys/pctypes.h>
#include <sys/pcmcia.h>
#include <sys/debug.h>
#include <sys/spl.h>
#include <sys/pcalloc.h>

#if defined(PCALLOC_DEBUG)
int pcalloc_debug = 0;
#endif
/*
 * resource allocation moved to RA global code
 */

struct ramap *ra_mem;
struct ramap *ra_io;
struct ramap *ra_freelist;
kmutex_t ra_lock;
uint_t ra_intr;
uint_t ra_num_intr;


/*
 * This is the loadable module wrapper.
 * It is essentially boilerplate so isn't documented
 */
extern struct mod_ops mod_miscops;
void ra_resource_init();
void ra_release();
#ifdef PCALLOC_DEBUG
int ra_dump_all(struct ramap *, char *);
#endif

static struct modlmisc modlmisc = {
	&mod_miscops,		/* Type of module. This one is a module */
	"Solaris RA Resource Allocator",	/* Name of the module. */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlmisc, NULL
};

int
_init()
{
	int	ret;

	ra_resource_init();
	if ((ret = mod_install(&modlinkage)) != 0) {
		ra_release();
	}
	return (ret);
}

int
_fini()
{
	int	ret;

	if ((ret = mod_remove(&modlinkage)) == 0) {
		ra_release();
	}
	return (ret);
}

int
_info(struct modinfo *modinfop)

{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * ra_fix_pow2(value)
 *	a utility function which rounds up to the
 *	nearest power of two value.
 */

uint_t
ra_fix_pow2(uint_t value)
{
	int i;

	if (ddi_ffs(value) == ddi_fls(value))
		return (value);
	/* not a power of two so round up */
	i = ddi_fls(value);
	/* this works since ffs/fls is plus 1 */
#if defined(PCALLOC_DEBUG)
	if (pcalloc_debug)  {
		cmn_err(CE_CONT, "ra_fix_pow2(%x)->%x:%x\n", value, i,
			1 << i);
		cmn_err(CE_CONT,
			"\tffs=%d, fls=%d\n", ddi_ffs(value), ddi_fls(value));
	}
#endif
	return (1 << i);
}

/*
 * ra_alloc_map()
 *	allocate an ramap structure.
 */

struct ramap *
ra_alloc_map()
{
	struct ramap *new;
	mutex_enter(&ra_lock);
	new = NULL;
	if (ra_freelist != NULL) {
		new = ra_freelist;
		ra_freelist = new->ra_next;
	}
	mutex_exit(&ra_lock);
	if (new == NULL) {
		new = (struct ramap *)kmem_zalloc(sizeof (struct ramap),
						    KM_SLEEP);
	} else {
		bzero((caddr_t)new, sizeof (struct ramap));
	}
	return (new);
}

/*
 * ra_free_map(map)
 *	return a used map to the freelist.
 *	Should probably check to see if above
 *	some threshold and kmem_free() any excess
 */
void
ra_free_map(struct ramap *map)
{
	if (map != NULL) {
		mutex_enter(&ra_lock);
		map->ra_next = ra_freelist;
		ra_freelist = map;
		mutex_exit(&ra_lock);
	}
}


/*
 * ra_free(map, base, len)
 *	return the specified range (base to base+len)
 *	to the specified map
 */

void
ra_free(struct ramap **map, uint_t base, uint_t len)
{
	struct ramap *newmap, *oldmap = NULL;
	struct ramap *mapp, *backp;
	uint_t newbase, mapend;

	/*
	 * always allocate a map entry so we can manipulate
	 * things without blocking inside our lock
	 */
	newmap = ra_alloc_map();
	ASSERT(newmap);

	mutex_enter(&ra_lock);

	mapp = *map;
	backp = (struct ramap *)map;

	/* now find where range lies and fix things up */
	newbase = base + len;
	for (; mapp != NULL; backp = mapp, mapp = mapp->ra_next) {
		mapend = mapp->ra_base + mapp->ra_len;
		if (newbase == mapp->ra_base) {
			/* simple - on front */
			mapp->ra_base = base;
			mapp->ra_len += len;
			/*
			 * don't need to check if it merges with
			 * previous since that would match on on end
			 */
			break;
		} else if (newbase == mapend) {
			/* simple - on end */
			mapp->ra_len += len;
			if (mapp->ra_next && newbase ==
			    mapp->ra_next->ra_base) {
				/* merge with next node */
				oldmap = mapp->ra_next;
				mapp->ra_len += oldmap->ra_len;
				mapp->ra_next = oldmap->ra_next;
			}
			break;
		} else if (base < mapp->ra_base) {
			/* somewhere in between so just an insert */
			newmap->ra_base = base;
			newmap->ra_len = len;
			newmap->ra_next = mapp;
			backp->ra_next = newmap;
			newmap = NULL;
			break;
		}
		/* else haven't found the spot yet */
	}
	if (mapp == NULL) {
		/* special case of running off the end - stick on end */
		newmap->ra_base = base;
		newmap->ra_len = len;
		backp->ra_next = newmap;
		newmap = NULL;
	}
	mutex_exit(&ra_lock);
	if (newmap != NULL)
		ra_free_map(newmap);
	if (oldmap != NULL)
		ra_free_map(oldmap);
#ifdef PCALLOC_DEBUG
	if (map == &ra_io)
		ra_dump_all(ra_io, "I/O");
	else if (map == &ra_mem)
		ra_dump_all(ra_mem, "memory");
#endif
}

/*
 * ra_alloc(map, reqest, return)
 *	Allocate a memory-like resource (physical memory, I/O space)
 *	subject to the constraints defined in the request structure.
 */

int
ra_alloc(struct ramap **map, ra_request_t *req, ra_return_t *ret)
{
	struct ramap *mapp, *backp;
	struct ramap *newmap, *old = NULL;
	int type = 0, len;
	uint_t mask = 0;
	int newlen, rval = DDI_FAILURE;
	uint_t base, lower, upper;

	if (req->ra_flags & RA_ALLOC_SPECIFIED)
		type = RA_ALLOC_SPECIFIED;
	else
		type = 0;

	if (req->ra_flags & (RA_ALLOC_POW2|RA_ALIGN_SIZE)) {
		if (req->ra_len != ra_fix_pow2(req->ra_len)) {
			cmn_err(CE_WARN, "ra: bad length (pow2) %d\n",
				req->ra_len);
			ret->ra_addr_hi = 0;
			ret->ra_addr_lo = 0;
			ret->ra_len = 0;
			return (DDI_FAILURE);
		}
	}
	mask = req->ra_align;
	if (req->ra_flags & RA_ALIGN_SIZE) {
		len = ra_fix_pow2(req->ra_len);
		mask = len - 1;
#if defined(PCALLOC_DEBUG)
		if (pcalloc_debug)
			cmn_err(CE_CONT, "len=%d, mask=%x\n", len, mask);
#endif
	}

	newmap = ra_alloc_map(); /* just in case */

	mutex_enter(&ra_lock);

	mapp = *map;
	backp = (struct ramap *)map;

	len = req->ra_len;

	if (req->ra_flags & RA_ALLOC_BOUNDED) {
		/* bounded so skip to first possible */
		lower = req->ra_boundbase;
		upper = req->ra_boundlen + lower;
		for (; mapp != NULL && (mapp->ra_base + mapp->ra_len) < lower;
			mapp = mapp->ra_next)
			;
	} else {
		lower = 0;
		upper = ~(uint_t)0;
	}

	if (type != RA_ALLOC_SPECIFIED) {
		/* first fit - not user specified */
#if defined(PCALLOC_DEBUG)
		if (pcalloc_debug)
			cmn_err(CE_CONT, "ra_alloc(unspecified request)"
				"lower=%x, upper=%x\n", lower, upper);
#endif
		for (; mapp != NULL; backp = mapp, mapp = mapp->ra_next) {
			if (mapp->ra_len >= len) {
				/* a candidate -- apply constraints */
				base = mapp->ra_base;
				if (base < lower &&
				    (base + mapp->ra_len) < (lower + len))
					continue;
				if (base < lower)
					base = lower;
#if defined(PCALLOC_DEBUG)
				if (pcalloc_debug)
					cmn_err(CE_CONT,
						"\tbase=%x, ra_base=%x,"
						"mask=%x\n",
						base, mapp->ra_base, mask);
#endif
				if ((mapp->ra_base & mask) != 0) {
					/*
					 * failed a critical constraint
					 * adjust and see if it still fits
					 */
					base = mapp->ra_base & ~mask;
					base += (mask + 1);
#if defined(PCALLOC_DEBUG)
					if (pcalloc_debug)
						cmn_err(CE_CONT,
							"\tnew base=%x\n",
							base);
#endif
					if (len > (mapp->ra_len -
							(base - mapp->ra_base)))
						continue;
				}
				/* we have a fit */
#if defined(PCALLOC_DEBUG)
				if (pcalloc_debug)
					cmn_err(CE_CONT, "\thave a fit\n");
#endif
#ifdef lint
				upper = upper; /* need to check upper bound */
#endif
				if (base != mapp->ra_base) {
					/* in the middle or end */
					newlen = base - mapp->ra_base;
					if ((mapp->ra_len - newlen) == len) {
						/* on the end */
						mapp->ra_len = newlen;
					} else {
						newmap->ra_next = mapp->ra_next;
						newmap->ra_base = base + len;
						newmap->ra_len = mapp->ra_len -
							(len + newlen);
						mapp->ra_len = newlen;
						mapp->ra_next = newmap;
						newmap = NULL;
					}

				} else {
					/* at the beginning */
					mapp->ra_base += len;
					mapp->ra_len -= len;
					if (mapp->ra_len == 0) {
						/* remove the whole node */
						backp->ra_next = mapp->ra_next;
						old = mapp;
					}
				}
				rval = DDI_SUCCESS;
				break;
			}
		}
	} else {
		/* want an exact value/fit */
		base = req->ra_addr_lo;
		len = req->ra_len;
		for (; mapp != NULL; backp = mapp, mapp = mapp->ra_next) {
		    if (base >= mapp->ra_base &&
			base < (mapp->ra_base + mapp->ra_len)) {
			    /* this is the node */
			    if ((base + len) >
				(mapp->ra_base + mapp->ra_len)) {
				    /* no match */
				    base = 0;
			    } else {
				    /* this is the one */
				    if (base == mapp->ra_base) {
					    /* at the front */
					    mapp->ra_base += len;
					    mapp->ra_len -= len;
					    if (mapp->ra_len == 0) {
						    /* used it up */
						    old = mapp;
						    backp->ra_next =
							    mapp->ra_next;
					    }
				    } else {
					    /* on the end or in middle */
					    if ((base + len) ==
						(mapp->ra_base +
						    mapp->ra_len)) {
						    /* on end */
						    mapp->ra_len -= len;
					    } else {
						    uint_t newbase, newlen;
						    /* in the middle */
						    newbase = base + len;
						    newlen = (mapp->ra_base +
								mapp->ra_len) -
								newbase;
						    newmap->ra_base = newbase;
						    newmap->ra_len = newlen;
						    newmap->ra_next =
							    mapp->ra_next;
						    mapp->ra_next = newmap;
						    mapp->ra_len -=
							    newlen + len;
						    newmap = NULL;
					    }
				    }
			    }
			    rval = DDI_SUCCESS;
			    break;
		    }
	    }
	}

	mutex_exit(&ra_lock);

	if (old)
		ra_free_map(old);
	if (newmap)
		ra_free_map(newmap);

#ifdef PCALLOC_DEBUG
	if (map == &ra_io)
		ra_dump_all(ra_io, "I/O");
	else if (map == &ra_mem)
		ra_dump_all(ra_mem, "memory");
#endif

	if (rval == DDI_SUCCESS) {
		ret->ra_addr_hi = 0;
		ret->ra_addr_lo = base;
		ret->ra_len = req->ra_len;
	}
	return (rval);
}

/*
 * ra_alloc_intr(intr, flags)
 *	Interrupts should be allocated subject to constraints
 *	as well.  Sharing and IPL level might be applied.
 *	Sharing definitely should be applied.
 */

ra_alloc_intr(int intr, uint flags)
{
#ifdef lint
	intr = intr;
	flags = flags;
#endif
	return (-1);
}


/*
 * pcm_get_mem(len, *base)
 *	implements the interface originally used in pcic
 */

uint_t
pcm_get_mem(dev_info_t *dip, int len, uint_t *base)
{
	ra_request_t req;
	ra_return_t ret;
#ifdef lint
	dip = dip;
#endif
	bzero((caddr_t)&req, sizeof (req));
	req.ra_addr_lo = *base;
	if (*base != 0)
		req.ra_flags |= RA_ALLOC_SPECIFIED;
	req.ra_len = len;
	(void) ra_alloc(&ra_mem, &req, &ret);
	*base = ret.ra_addr_lo;
	return (ret.ra_addr_lo);
}


void
pcm_return_mem(dev_info_t *dip, uint_t base, int len)
{
#ifdef lint
	dip = dip;
#endif
	ra_free(&ra_mem, base, len);
}

uint_t
pcm_get_io(dev_info_t *dip, int len, uint_t *base)
{
	ra_request_t req;
	ra_return_t ret;

#ifdef lint
	dip = dip;
#endif
	bzero((caddr_t)&req, sizeof (req));
	if (*base != 0)
		req.ra_flags |= RA_ALLOC_SPECIFIED;
	req.ra_flags |= RA_ALIGN_SIZE | RA_ALLOC_POW2;
	req.ra_addr_lo = *base;
	/* only allocate between 0x200 and end of I/O */
	req.ra_boundbase = 0x200;
	req.ra_boundlen = 0xffff;
	req.ra_flags |= RA_ALLOC_BOUNDED;
	req.ra_len = len;
#if defined(PCALLOC_DEBUG)
	if (pcalloc_debug) {
		cmn_err(CE_CONT, "pcm_get_io(%x, %x)\n", len, base);
		if (base != 0)
			cmn_err(CE_CONT, "\t*base=%x\n", *base);
	}
#endif
	(void) ra_alloc(&ra_io, &req, &ret);
	*base = ret.ra_addr_lo;
#if defined(PCALLOC_DEBUG)
	if (pcalloc_debug)
		cmn_err(CE_CONT, "pcm_get_io -> %x\n", ret.ra_addr_lo);
#endif
	return (ret.ra_addr_lo);
}

void
pcm_return_io(dev_info_t *dip, int base, int len)
{
#ifdef lint
	dip = dip;
#endif
#if defined(PCALLOC_DEBUG)
	if (pcalloc_debug)
		cmn_err(CE_CONT, "pcm_return_io(%x, %x)\n", base, len);
#endif
	ra_free(&ra_io, base, len);
}

int
pcm_get_intr(dev_info_t *dip, int request)
{
#ifdef lint
	dip = dip;
#endif
	if (request >= 0) {
		if (request < 32 && ra_intr & (1 << request)) {
			ra_intr &= ~(1 << request);
		} else {
			request = -1;
		}
	} else {
		request = ddi_ffs(ra_intr) - 1;
		if (request >= 0)
			ra_intr &=  ~(1 << request);
	}
	return (request);
}

int
pcm_return_intr(dev_info_t *dip, int request)
{
#ifdef lint
	dip = dip;
#endif
	ra_intr |= (1 << request);
	return (request);
}

/*
 * initialization of resource maps
 *
 * Note that this needs "ra" module setting things up
 * at this point but will ultimately get the properties from
 * devconf.
 */
void
ra_resource_init()
{
	dev_info_t *used;
	struct iorange {
		void *	base;
		int	len;
	} *iorange;
	struct memrange {
		caddr_t	base;
		int	len;
	} *memrange;
	u_int *irq;
	int proplen;
	int i, len;
	int maxrange;
	ra_request_t req;
	ra_return_t ret;

	used = ddi_find_devinfo("used-resources", -1, 0);
	mutex_init(&ra_lock, "resource allocator lock", MUTEX_SPIN,
			(void *) ipltospl(SPL7 - 1));
	if (used == NULL) {
		mutex_destroy(&ra_lock);
		return;
	}
	/*
	 * initialize to all resources being present
	 * and then remove the ones in use.
	 */

	ra_intr = (ushort_t)~0;
#if defined(i386) || defined(__ppc)
	ra_intr &= ~(1 << 2);	/* 2 == 9 so never allow */
#endif

	ra_mem = ra_alloc_map();
	ra_io  = ra_alloc_map();
	ASSERT(ra_mem && ra_io);
	ra_mem->ra_base = 0;
	ra_mem->ra_len  = (uint_t)~0;
	ra_io->ra_base = 0;
	ra_io->ra_len = 0xFFFF;

	if (ddi_getlongprop(DDI_DEV_T_NONE, used, DDI_PROP_DONTPASS,
				"io-space", (caddr_t)&iorange, &proplen) ==
	    DDI_SUCCESS) {
		maxrange = min(proplen / sizeof (struct iorange),
				PR_MAX_IO_RANGES);
		/* remove the "used" I/O resources */
		for (i = 0; i < maxrange; i++) {
			bzero((caddr_t)&req, sizeof (req));
			bzero((caddr_t)&ret, sizeof (ret));
			req.ra_addr_lo = (uint_t)iorange[i].base;
			req.ra_len = iorange[i].len;
			req.ra_flags = RA_ALLOC_SPECIFIED;
			(void) ra_alloc(&ra_io, &req, &ret);
		}

		kmem_free((caddr_t)iorange, proplen);
	}

	if (ddi_getlongprop(DDI_DEV_T_NONE, used, DDI_PROP_DONTPASS,
				"device-memory", (caddr_t)&memrange,
				&proplen) == DDI_SUCCESS) {
		maxrange = min(proplen / sizeof (struct memrange),
				PR_MAX_MEM_RANGES);
		/* remove the "used" memory resources */
		for (i = 0; i < maxrange; i++) {
			bzero((caddr_t)&req, sizeof (req));
			bzero((caddr_t)&ret, sizeof (ret));
			req.ra_addr_lo = (uint_t)memrange[i].base;
			req.ra_len = memrange[i].len;
			req.ra_flags = RA_ALLOC_SPECIFIED;
			(void) ra_alloc(&ra_mem, &req, &ret);
		}

		kmem_free((caddr_t)memrange, proplen);
	}

	if (ddi_getlongprop(DDI_DEV_T_NONE, used, DDI_PROP_DONTPASS,
			    "interrupts", (caddr_t)&irq, &proplen) ==
	    DDI_SUCCESS) {
		/* Initialize available interrupts by negating the used */
		len = (proplen / sizeof (u_int));
		for (i = 0; i < len; i++) {
			ra_intr &= ~(1 << irq[i]);
		}
		kmem_free((caddr_t)irq, proplen);
	}
	ra_freelist = ra_alloc_map(); /* pre-load an extra one */

#ifdef PCALLOC_DEBUG
	ra_dump_all(ra_io, "I/O");
	ra_dump_all(ra_mem, "memory");
#endif

}

void
ra_release()
{
	struct ramap *next;
	mutex_enter(&ra_lock);
	while (ra_mem != NULL) {
		next = ra_mem->ra_next;
		kmem_free((caddr_t)ra_mem, sizeof (struct ramap));
		ra_mem = next;
	}
	while (ra_io != NULL) {
		next = ra_io->ra_next;
		kmem_free((caddr_t)ra_io, sizeof (struct ramap));
		ra_io = next;
	}
	while (ra_freelist != NULL) {
		next = ra_freelist->ra_next;
		kmem_free((caddr_t)ra_freelist, sizeof (struct ramap));
		ra_freelist = next;
	}
	mutex_exit(&ra_lock);
	mutex_destroy(&ra_lock);
}

#ifdef PCALLOC_DEBUG
ra_dump_all(struct ramap *map, char *tag)
{
#if 01
	cmn_err(CE_CONT, "ra: dump %s list:\n", tag);
	for (; map != NULL; map = map->ra_next) {
		cmn_err(CE_CONT, "\t%x,%x\n", map->ra_base, map->ra_len);
	}
#endif
	return (0);
}
#endif
