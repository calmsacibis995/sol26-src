/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_PCALLOC_H
#define	_PCALLOC_H

#pragma ident	"@(#)pcalloc.h	1.7	96/05/31 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

typedef unsigned long long u64bit_t;

struct ramap {
	struct ramap *ra_next;
	uint_t ra_base;
	uint_t ra_len;
};

#define	RA_MAP_IO	0x0
#define	RA_MAP_MEM	0x1

/*
 * range/bounds specifications
 */

typedef struct ra_bound {
	uint_t		ra_base;
	uint_t		ra_len;
} ra_bound_t;

#define	RA_ALIGN_MASK		0x0001
#define	RA_ALIGN_SIZE		0x0002
#define	RA_ALLOC_BOUNDED	0x0004
#define	RA_ALLOC_POW2		0x0020
#define	RA_ALLOC_SPECIFIED	0x0040

typedef struct ra_request {
				/* general flags */
	uint_t	ra_flags;
				/* length of resource */
	uint_t	ra_len;
				/* specific address */
	uint_t	ra_addr_hi;
	uint_t	ra_addr_lo;
				/* address mask */
	uint_t	ra_mask;
				/* bounds on addresses */
	uint_t	ra_boundbase;
	uint_t	ra_boundlen;
				/* alignment mask */
	uint_t	ra_align;
} ra_request_t;

typedef struct ra_return {
	uint_t	ra_addr_hi;
	uint_t	ra_addr_lo;
	uint_t	ra_len;
	int	ra_error;
} ra_return_t;

typedef struct ra_req_intr {
	uint_t	ra_flags;
	uint_t	ra_intr;	/* requested interrupt if specific request */
	int	ra_usable_len;
	uint_t	*ra_usable_list; /* list of interrupts to choose from */
} ra_req_intr_t;

typedef struct ra_ret_intr {
	uint_t ra_intr;
	uint_t ra_error;
} ra_ret_intr_t;

#define	RA_INTR_SHARE		0x0001
#define	RA_INTR_HOST		0x0002
#define	RA_INTR_LEVEL		0x0004

#define	RA_SUCCESS		0
#define	RA_ERROR_NO_RESOURCE	1
#define	RA_ERROR_BAD_REQUEST	2

/*
 * prototypes for entities using this mechanism
 */

struct ramap *ra_alloc_map();
void ra_free_map(struct ramap *);
void ra_free(struct ramap **, uint_t, uint_t);
int ra_alloc(struct ramap **, ra_request_t *, ra_return_t *);

/*
 * global resource lists
 */
extern struct ramap *ra_mem;
extern struct ramap *ra_io;
extern struct ramap *ra_freelist;
extern uint_t ra_intr;
extern uint_t ra_num_intr;

#ifdef	__cplusplus
}
#endif

#endif	/* _PCALLOC_H */
