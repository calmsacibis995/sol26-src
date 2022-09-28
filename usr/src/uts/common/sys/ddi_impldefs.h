/*
 * Copyright (c) 1990-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_DDI_IMPLDEFS_H
#define	_SYS_DDI_IMPLDEFS_H

#pragma ident	"@(#)ddi_impldefs.h	1.39	96/10/15 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/t_lock.h>
#include <sys/ddipropdefs.h>
#include <sys/devops.h>
#include <sys/autoconf.h>
#include <sys/mutex.h>
#include <vm/page.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * dev_info: 	The main device information structure this is intended to be
 *		opaque to drivers and drivers should use ddi functions to
 *		access *all* driver accessible fields.
 *
 * devi_parent_data includes property lists (interrupts, registers, etc.)
 * devi_driver_data includes whatever the driver wants to place there.
 */

struct dev_info  {

	struct dev_info *devi_parent;	/* my parent node in tree	*/
	struct dev_info *devi_child;	/* my child list head		*/
	struct dev_info *devi_sibling;	/* next element on my level	*/

	char	*devi_binding_name;	/* name used to bind driver	*/

	char	*devi_addr;		/* address part of name		*/
	int	devi_nodeid;		/* device nodeid		*/
	int	devi_instance;		/* device instance number	*/

	struct dev_ops *devi_ops;	/* driver operations		*/

	caddr_t	devi_parent_data;	/* parent private data		*/
	caddr_t	devi_driver_data;	/* driver private data		*/

	ddi_prop_t *devi_drv_prop_ptr;	/* head of driver prop list */
	ddi_prop_t *devi_sys_prop_ptr;	/* head of system prop list */

	struct ddi_minor_data *devi_minor;	/* head of minor list */
	struct dev_info *devi_next;	/* Next instance of this device */
	kmutex_t devi_lock;		/* Protects per-devinfo data */

	/* logical parents for busop primitives	 */

	struct dev_info *devi_bus_map_fault;	/* bus_map_fault parent	 */
	struct dev_info *devi_bus_dma_map;	/* bus_dma_map parent	 */
	struct dev_info *devi_bus_dma_allochdl; /* bus_dma_newhdl parent */
	struct dev_info *devi_bus_dma_freehdl;  /* bus_dma_freehdl parent */
	struct dev_info *devi_bus_dma_bindhdl;  /* bus_dma_bindhdl parent */
	struct dev_info *devi_bus_dma_unbindhdl; /* bus_dma_unbindhdl parent */
	struct dev_info *devi_bus_dma_flush;    /* bus_dma_flush parent	 */
	struct dev_info *devi_bus_dma_win;	/* bus_dma_win parent	 */
	struct dev_info *devi_bus_dma_ctl;	/* bus_dma_ctl parent	 */
	struct dev_info	*devi_bus_ctl;		/* bus_ctl parent	 */

	void	*devi_pm_info;			/* priv pwr mngmt stuff	*/

	/*
	 * The fields before and including 'devi_pm_info' must not
	 * be changed. Power Management relies on the binary layout.
	 * Add any new fields at the end of the data structure.
	 */
	char	*devi_node_name;		/* The 'name' of the node */
	char	*devi_compat_names;		/* A list of driver names */
	size_t	devi_compat_length;		/* Size of compat_names */

	int (*devi_bus_dma_bindfunc)(dev_info_t *, dev_info_t *,
	    ddi_dma_handle_t, struct ddi_dma_req *, ddi_dma_cookie_t *,
	    u_int *);
	int (*devi_bus_dma_unbindfunc)(dev_info_t *, dev_info_t *,
	    ddi_dma_handle_t);

	ddi_prop_t *devi_hw_prop_ptr;		/* head of hw prop list */

	ddi_devid_t	devi_devid;		/* registered device id */
	/*
	 * power manageable component information
	 */
	int devi_comp_flags;			/* OR of component flags */
	int devi_num_components;		/* number of components */
	size_t devi_comp_size;			/* kmem_free for pm_component */
	struct pm_component *devi_components;	/* array of component structs */
	uint_t		    devi_state;		/* device/bus state flags */
						/* see below for definitions */
	/* some device driver statistical info may go here */
};

#define	DEVI(dev_info_type)	((struct dev_info *)(dev_info_type))

/*
 * NB: The 'name' field, for compatibilty with old code (both existing
 * device drivers and userland code), is now defined as the name used
 * to bind the node to a device driver, and not the device node name.
 * If the device node name does not define a binding to a device driver,
 * and the framework uses a different algorithm to create the binding to
 * the driver, the node name and binding name will be different.
 *
 * Note that this implies that the node name plus instance number does
 * NOT create a unique driver id; only the binding name plus instance
 * number creates a unique driver id.
 *
 * New code should not use 'devi_name'; use 'devi_binding_name' or
 * 'devi_node_name' and/or the routines that access those fields.
 */

#define	devi_name devi_binding_name

/*
 * Test to see that a dev_info node is in canonical form 1.
 *
 * A dev_info node is in canonical form 1 if its address information
 * has been interpreted and the address part of the name has been assigned.
 * This is usually done by the parent dev_info node.
 */
#define	DDI_CF1(devi)		(DEVI(devi)->devi_addr != NULL)

/*
 * Test to see that a dev_info node is in canonical form 2.
 *
 * A dev_info node is in canonical form 2 if it has been successfully
 * probed and attached.  This is indicated by having a driver bound to
 * the dev_info node.  When a driver is unloaded, mod_nodev_ops is
 * bound to the node so that it won't be probed/attached when reloaded.
 */
#define	DDI_CF2(devi)		(DEVI(devi)->devi_ops != NULL)

extern struct dev_ops mod_nodev_ops;
#define	DDI_DRV_UNLOADED(devi)	(DEVI(devi)->devi_ops == &mod_nodev_ops)

/*
 * The device node state (devi_state) contains information regarding
 * the state of the device (Online/Offline/Down).  For bus nexus
 * devices, the device state also contains state information regarding
 * the state of the bus represented by this nexus node.
 *
 * Device state information is stored in bits [0-7], bus state in bits
 * [8-15].
 *
 */

#define	DEVI_DEVICE_OFFLINE	0x1
#define	DEVI_DEVICE_DOWN	0x2
#define	DEVI_BUS_QUIESCED	0x100
#define	DEVI_BUS_DOWN		0x200

#define	DEVI_IS_DEVICE_OFFLINE(dip) \
	((DEVI((dip))->devi_state & DEVI_DEVICE_OFFLINE) == DEVI_DEVICE_OFFLINE)

#define	DEVI_SET_DEVICE_ONLINE(dip) \
	(DEVI((dip))->devi_state &= ~DEVI_DEVICE_OFFLINE)

#define	DEVI_SET_DEVICE_OFFLINE(dip) \
	(DEVI((dip))->devi_state |= DEVI_DEVICE_OFFLINE)

#define	DEVI_IS_DEVICE_DOWN(dip) \
	((DEVI((dip))->devi_state & DEVI_DEVICE_DOWN) == DEVI_DEVICE_DOWN)

#define	DEVI_SET_DEVICE_DOWN(dip) \
	(DEVI((dip))->devi_state |= DEVI_DEVICE_DOWN)

#define	DEVI_SET_DEVICE_UP(dip) \
	(DEVI((dip))->devi_state &= ~DEVI_DEVICE_DOWN)

#define	DEVI_IS_BUS_QUIESCED(dip) \
	((DEVI((dip))->devi_state & DEVI_BUS_QUIESCED) == DEVI_BUS_QUIESCED)

#define	DEVI_SET_BUS_ACTIVE(dip) \
	(DEVI((dip))->devi_state &= ~DEVI_BUS_QUIESCED)

#define	DEVI_SET_BUS_QUIESCE(dip) \
	(DEVI((dip))->devi_state |= DEVI_BUS_QUIESCED)

#define	DEVI_IS_BUS_DOWN(dip) \
	((DEVI((dip))->devi_state & DEVI_BUS_DOWN) == DEVI_BUS_DOWN)

#define	DEVI_SET_BUS_UP(dip) \
	(DEVI((dip))->devi_state &= ~DEVI_BUS_DOWN)

#define	DEVI_SET_BUS_DOWN(dip) \
	(DEVI((dip))->devi_state |= DEVI_BUS_DOWN)

/*
 * attach hotplugged devinfo nodes
 */
void
impl_attach_new_devinfos(struct devnames *dnp);

/*
 * Definitions used in hashing ndi_event_* cookie requests.
 */
void
i_ndi_event_init_hashtable(void);

#define	EVC_BUCKETS		128
#define	EVC_HASH(num)		((int)((num) & (EVC_BUCKETS - 1)))

/*
 * eventcookie descriptor
 */
typedef struct edesc {
	char	*name;
	struct	edesc *next;
}edesc_t;


/*
 * This structure represents one piece of bus space occupied by a given
 * device. It is used in an array for devices with multiple address windows.
 */
struct regspec {
	u_int regspec_bustype;		/* cookie for bus type it's on */
	u_int regspec_addr;		/* address of reg relative to bus */
	u_int regspec_size;		/* size of this register set */
};

/*
 * This structure represents one piece of nexus bus space.
 * It is used in an array for nexi with multiple bus spaces
 * to define the childs offsets in the parents bus space.
 */
struct rangespec {
	u_int rng_cbustype;		/* Child's address, hi order */
	u_int rng_coffset;		/* Child's address, lo order */
	u_int rng_bustype;		/* Parent's address, hi order */
	u_int rng_offset;		/* Parent's address, lo order */
	u_int rng_size;			/* size of space for this entry */
};

/*
 * This structure represents one interrupt possible from the given
 * device. It is used in an array for devices with multiple interrupts.
 */
struct intrspec {
	u_int intrspec_pri;		/* interrupt priority */
	u_int intrspec_vec;		/* vector # (0 if none) */
	u_int (*intrspec_func)();	/* function to call for interrupt, */
					/* If (u_int (*)()) 0, none. */
					/* If (u_int (*)()) 1, then */
					/* this is a 'fast' interrupt. */
};

typedef enum {
	DDM_MINOR = 0,
	DDM_ALIAS,
	DDM_DEFAULT,
	DDM_INTERNAL_PATH
} ddi_minor_type;

struct ddi_minor {
	char		*name;		/* name of node */
	dev_t		dev;		/* device id */
	int		spec_type;	/* block or char */
	char		*node_type;	/* block, byte, serial, network */
};

struct ddi_minor_alias {
	struct ddi_minor_data *dmp;	/* Pointer to real node data */
};

#ifdef _KERNEL

/*
 * Values for request_type
 */
typedef enum {
	PMR_SET_POWER = 1,
	PMR_SUSPEND,
	PMR_RESUME
} pm_request_type;

typedef struct {
	pm_request_type request_type;
	union req {
		struct set_power_req {
			dev_info_t	*who;
			int		cmpt;
			int		level;
		} set_power_req;
		struct suspend_req {
			dev_info_t	*who;
			ddi_detach_cmd_t cmd;
		} suspend_req;
		struct resume_req {
			dev_info_t	*who;
			ddi_attach_cmd_t cmd;
		} resume_req;
	} req;
} power_req;


/*
 * This structure is allocated by i_ddi_add_softintr and its address is used
 * as a cookie passed back to the caller to be used later by
 * i_ddi_remove_softintr
 */
struct soft_intrspec {
	struct dev_info *si_devi;	/* records dev_info of caller */
	struct intrspec si_intrspec;	/* and the intrspec */
};
#endif /* _KERNEL */

/*
 * The ddi_minor_data structure gets filled in by ddi_create_minor_node.
 * It then gets attached to the devinfo node as a property.
 */
struct ddi_minor_data {
	struct ddi_minor_data *next;	/* next one in the chain */
	dev_info_t	*dip;		/* pointer to devinfo node */
	ddi_minor_type	type;		/* Following data type */
	union {
		struct ddi_minor d_minor;	/* Actual minor node data */
		struct ddi_minor_alias	d_alias; /* The minor node actually */
					/* lives under another dev_info node */
		} mu;
};

#define	ddm_name	mu.d_minor.name
#define	ddm_aname	mu.d_alias.dmp->mu.d_minor.name
#define	ddm_dev		mu.d_minor.dev
#define	ddm_adev	mu.d_alias.dmp->mu.d_minor.dev
#define	ddm_spec_type	mu.d_minor.spec_type
#define	ddm_aspec_type	mu.d_alias.dmp->mu.d_minor.spec_type
#define	ddm_node_type	mu.d_minor.node_type
#define	ddm_anode_type	mu.d_alias.dmp->mu.d_minor.node_type
#define	ddm_admp	mu.d_alias.dmp
#define	ddm_atype	mu.d_alias.dmp->type
#define	ddm_adip	mu.d_alias.dmp->dip

/*
 * parent private data structure contains register, interrupt, property
 * and range information.
 */
struct ddi_parent_private_data {
	int par_nreg;			/* number of regs */
	struct regspec *par_reg;	/* array of regs */
	int par_nintr;			/* number of interrupts */
	struct intrspec *par_intr;	/* array of possible interrupts */
	int par_nrng;			/* number of ranges */
	struct rangespec *par_rng;	/* array of ranges */
};
#define	DEVI_PD(d)	\
	((struct ddi_parent_private_data *)DEVI((d))->devi_parent_data)

#define	sparc_pd_getnreg(dev)		(DEVI_PD(dev)->par_nreg)
#define	sparc_pd_getnintr(dev)		(DEVI_PD(dev)->par_nintr)
#define	sparc_pd_getnrng(dev)		(DEVI_PD(dev)->par_nrng)
#define	sparc_pd_getreg(dev, n)		(&DEVI_PD(dev)->par_reg[(n)])
#define	sparc_pd_getintr(dev, n)	(&DEVI_PD(dev)->par_intr[(n)])
#define	sparc_pd_getrng(dev, n)		(&DEVI_PD(dev)->par_rng[(n)])

/*
 * Create a ddi_parent_private_data structure from the properties in the
 * child dev_info node.
 */
#if defined(_KERNEL) && defined(__STDC__)
int impl_ddi_make_ppd(dev_info_t *child, struct ddi_parent_private_data **ppd);
#endif	/* _KERNEL && __STDC__ */

/*
 * Solaris DDI DMA implementation structure and function definitions.
 *
 * Note: no callers of DDI functions must depend upon data structures
 * declared below. They are not guaranteed to remain constant.
 */

/*
 * Implementation DMA mapping structure.
 *
 * The publicly visible ddi_dma_req structure is filled
 * in by a caller that wishes to map a memory object
 * for DMA. Internal to this implementation of the public
 * DDI DMA functions this request structure is put together
 * with bus nexus specific functions that have additional
 * information and constraints as to how to go about doing
 * the requested mapping function
 *
 * In this implementation, some of the information from the
 * orginal requestor is retained throughout the lifetime
 * of the I/O mapping being active.
 */

/*
 * This is the implementation specific description
 * of how we've mapped an object for DMA.
 */

/*
 * Perhaps should be #if sparc / #elif i386/ppc / #else err
 * but maybe not, if the extended i386 code is the wave of the
 * future.
 */
#if defined(sparc) || defined(__sparc)
typedef struct ddi_dma_impl {
	/*
	 * DMA mapping information
	 */
	u_long	dmai_mapping;	/* mapping cookie */

	/*
	 * Size of the current mapping, in bytes.
	 *
	 * Note that this is distinct from the size of the object being mapped
	 * for DVMA. We might have only a portion of the object mapped at any
	 * given point in time.
	 */
	u_int	dmai_size;

	/*
	 * Offset, in bytes, into object that is currently mapped.
	 */
	off_t	dmai_offset;

	/*
	 * Information gathered from the original DMA mapping
	 * request and saved for the lifetime of the mapping.
	 */
	u_int		dmai_minxfer;
	u_int		dmai_burstsizes;
	u_int		dmai_ndvmapages;
	u_int		dmai_pool;	/* cached DVMA space */
	u_int		dmai_rflags;	/* requestor's flags + ours */
	u_int		dmai_inuse;	/* active handle? */
	u_int		dmai_nwin;
	u_int		dmai_winsize;
	caddr_t		dmai_nexus_private;
	void		*dmai_iopte;
	u_int		*dmai_sbi;
	void		*dmai_minfo;	/* random mapping information */
	dev_info_t	*dmai_rdip;	/* original requestor's dev_info_t */
	ddi_dma_obj_t	dmai_object;	/* requestor's object */
	ddi_dma_attr_t	dmai_attr;	/* DMA attributes */
	ddi_dma_cookie_t *dmai_cookie;	/* pointer to first DMA cookie */

} ddi_dma_impl_t;

#else

#define	DMAMI_KVADR		0x05
#define	DMAMI_UVADR		0x09
#define	DMAMI_PAGES		0x0b

typedef struct ddi_dma_impl {

	u_long	dmai_kmsize;

	u_long	dmai_xxx;

	struct impl_dma_segment *dmai_hds;	/* head of list of segments */

	struct impl_dma_segment *dmai_wins;	/* ptr to first segment of */
						/* current window */

	caddr_t		dmai_ibufp;	/* intermediate buffer address */
	paddr_t		dmai_ibpadr;	/* phys adr of intermediate buffer */
	u_long		dmai_ibfsz;	/* intermediate buffer size */

	caddr_t		dmai_kaddr;	/* kernel addr for page mapping */

	/*
	 * Information gathered from the original dma mapping
	 * request and saved for the lifetime of the mapping.
	 */
	u_int		dmai_minxfer;
	u_int		dmai_burstsizes;
	u_int		dmai_rflags;	/* requestor's flags + ours */
	u_int		dmai_inuse;
	int		dmai_nwin;
	void		*dmai_segp;
	void		*dmai_minfo;	/* random mapping information */
	dev_info_t	*dmai_rdip;	/* original requestor's dev_info_t */
	ddi_dma_obj_t	dmai_object;	/* requestor's object */

#if	defined(i386)
	int		(*dmai_mctl)();	/* mctl function addr for */
					/* express processing */
#endif
	ddi_dma_attr_t	dmai_attr;	/* DMA attributes */
	ddi_dma_cookie_t *dmai_cookie;

} ddi_dma_impl_t;
#endif  /* defined(sparc) || defined(__sparc) */

/*
 * For now DMA segments share state with the DMA handle
 */
typedef ddi_dma_impl_t ddi_dma_seg_impl_t;

/*
 * These flags use reserved bits from the dma request flags.
 *
 * A note about the DMP_NOSYNC flags: the root nexus will
 * set these as it sees best. If an intermediate nexus
 * actually needs these operations, then during the unwind
 * from the call to ddi_dma_bind, the nexus driver *must*
 * clear the appropriate flag(s). This is because, as an
 * optimization, ddi_dma_sync(9F) looks at these flags before
 * deciding to spend the time going back up the tree.
 */

#define	_DMCM1	DDI_DMA_RDWR|DDI_DMA_REDZONE|DDI_DMA_PARTIAL
#define	_DMCM2	DDI_DMA_CONSISTENT|DMP_VMEREQ
#define	DMP_DDIFLAGS	(_DMCM1|_DMCM2)
#define	DMP_SHADOW	0x20
#define	DMP_LKIOPB	0x40
#define	DMP_LKSYSV	0x80
#define	DMP_IOCACHE	0x100
#define	DMP_USEHAT	0x200
#define	DMP_PHYSADDR	0x400
#define	DMP_INVALID	0x800
#define	DMP_NOLIMIT	0x1000
#define	DMP_VMEREQ	0x10000000
#define	DMP_BYPASSNEXUS	0x20000000
#define	DMP_NODEVSYNC	0x40000000
#define	DMP_NOCPUSYNC	0x80000000
#define	DMP_NOSYNC	(DMP_NODEVSYNC|DMP_NOCPUSYNC)

/*
 * In order to complete a device to device mapping that
 * has percolated as high as an IU nexus (gone that high
 * because the DMA request is a VADDR type), we define
 * structure to use with the DDI_CTLOPS_DMAPMAPC request
 * that re-traverses the request tree to finish the
 * DMA 'mapping' for a device.
 */
struct dma_phys_mapc {
	struct ddi_dma_req *dma_req;	/* original requst */
	ddi_dma_impl_t *mp;		/* current handle, or none */
	int nptes;			/* number of ptes */
	void *ptes;			/* ptes already read */
};

/*
 * Implementation DMA segment structure.
 *
 * This is a superset of the ddi_dma_cookie structure that describes
 * one of the physical memory segments into which the memory object
 * was broken up.
 */
typedef struct impl_dma_segment {
	struct impl_dma_segment	*dmais_link;	/* to next segment */
	struct ddi_dma_impl	*dmais_hndl;	/* to dma handle */
	ddi_dma_cookie_t	*dmais_cookie;
	union {
		struct impl_dma_segment	*_dmais_nex;	/* to 1st seg of */
							/* next window */
		struct impl_dma_segment	*_dmais_cur;	/* to 1st seg of */
							/* this window */
	} _win;
	ulong_t		dmais_ofst;		/* 32-bit offset */
	union {
		caddr_t		_dmais_va;	/* 32-bit virtual address */
		page_t		*_dmais_pp;	/* page pointer */
	} _vdmu;
	union {
		paddr_t 	_dmais_pd;	/* 32-bit physical address */
		ushort_t	_dmais_pw[2];   /* 2x16-bit address */
		caddr_t		_dmais_kva;	/* pio kernel virtual address */
	} _pdmu;
	ulong_t		dmais_size;		/* size of cookie in bytes */
	ushort_t	dmais_flags;		/* bus specific flag bits */
	ushort_t	dmais_xxx;		/* unused filler */
} impl_dma_segment_t;

/*
 * flags
 */
#define	DMAIS_NEEDINTBUF	0x0100
#define	DMAIS_COMPLEMENT	0x0200
#define	DMAIS_NOMERGE		DMAIS_NEEDINTBUF | DMAIS_COMPLEMENT
#define	DMAIS_MAPPAGE		0x0400
#define	DMAIS_PAGEPTR		0x0800
#define	DMAIS_WINSTRT		0x1000	/* this segment is window start */
#define	DMAIS_WINUIB		0x2000	/* window uses intermediate buffers */
#define	DMAIS_WINEND		0x8000	/* this segment is window end */

/*
 * Interrupt addition preferences
 */
#define	IDDI_INTR_TYPE_NORMAL	1
#define	IDDI_INTR_TYPE_FAST	2
#define	IDDI_INTR_TYPE_SOFT	3

#define	MAXCALLBACK		20

/*
 * Callback definitions
 */
struct ddi_callback {
	struct ddi_callback 	*c_nfree;
	struct ddi_callback 	*c_nlist;
	int			(*c_call)();
	caddr_t			c_arg;
	kmutex_t		*c_mutex;
	int			c_count;
};


/*
 * Device id - Internal definition.
 */
#define	DEVID_MAGIC_MSB		0x69
#define	DEVID_MAGIC_LSB		0x64
#define	DEVID_REV_MSB		0x00
#define	DEVID_REV_LSB		0x01
#define	DEVID_HINT_SIZE		4

typedef struct impl_devid {
	u_char	did_magic_hi;			/* device id magic # (msb) */
	u_char	did_magic_lo;			/* device id magic # (lsb) */
	u_char	did_rev_hi;			/* device id revision # (msb) */
	u_char	did_rev_lo;			/* device id revision # (lsb) */
	u_char	did_type_hi;			/* device id type (msb) */
	u_char	did_type_lo;			/* device id type (lsb) */
	u_char	did_len_hi;			/* length of devid data (msb) */
	u_char	did_len_lo;			/* length of devid data (lsb) */
	char	did_driver[DEVID_HINT_SIZE];	/* driver name - HINT */
	char	did_id[1];			/* start of device id data */
} impl_devid_t;

#define	DEVID_GETTYPE(devid)		((u_short) \
					    (((devid)->did_type_hi << NBBY) + \
					    (devid)->did_type_lo))

#define	DEVID_FORMTYPE(devid, type)	(devid)->did_type_hi = hibyte((type)); \
					(devid)->did_type_lo = lobyte((type));

#define	DEVID_GETLEN(devid)		((u_short) \
					    (((devid)->did_len_hi << NBBY) + \
					    (devid)->did_len_lo))

#define	DEVID_FORMLEN(devid, len)	(devid)->did_len_hi = hibyte((len)); \
					(devid)->did_len_lo = lobyte((len));

/*
 * Power management component definitions, used for tracking
 * idleness of devices.
 * An array these hangs of the devi_pm_components
 * member of the dev_info struct (if initialized by driver or
 * on the driver's behalf by a pm-busy-when property).
 *
 * The array of these structs is followed in the same kmem_zalloc'd chunk by
 * the names pointed to by the structs.
 *
 * See epm.h for the flag defines
 */


struct pm_component {
	u_int pmc_flags;		/* flags this component */
	u_int pmc_busycount;		/* for nesting busy calls */
	time_t pmc_timestamp;		/* timestamp */
	u_int pmc_norm_pwr;		/* normal power value */
};

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DDI_IMPLDEFS_H */
