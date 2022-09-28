/*
 * Copyright (c) 1991-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_EBUS_H
#define	_SYS_EBUS_H

#pragma ident	"@(#)ebus.h	1.6	95/09/18 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * driver state type:
 */
typedef enum { NEW = 0, ATTACHED, RESUMED, DETACHED,
		SUSPENDED, PM_SUSPENDED } driver_state_t;

/*
 * The i86pc specific code fragments are to support the debug of "honeynut"
 * and "multigrain" prototypes on i86pc platform.  Most of the fragments
 * deal with differences in the interrupt dispatching between the prototypes
 * and the cheerio ebus.  On the prototype boards, all interrupt lines are
 * tied together.  For this case, the nexus driver uses a common interrupt
 * handler to poll all of its children.
 */
#if defined(i86pc)
#define	MAX_EBUS_DEVS	6

/*
 * ebus device interrupt info;
 */
typedef struct {
	char *name;
	u_int inuse;
	u_int (*handler)();
	caddr_t arg;
} ebus_intr_slot_t;
#endif

/*
 * driver soft state structure:
 */
typedef struct {
	dev_info_t *dip;
	driver_state_t state;
	pci_regspec_t *reg;
	int nreg;
#if defined(i86pc)
	ddi_iblock_cookie_t iblock;
	ddi_idevice_cookie_t idevice;
	ebus_intr_slot_t intr_slot[MAX_EBUS_DEVS];
#endif
} ebus_devstate_t;

/*
 * definition of ebus reg spec entry:
 */
typedef struct {
	u_int	addr_hi;
	u_int	addr_low;
	u_int	size;
} ebus_regspec_t;


/*
 * use macros for soft state and driver properties:
 */
#define	get_ebus_soft_state(i)	\
	((ebus_devstate_t *)ddi_get_soft_state(per_ebus_state, (i)))

#define	alloc_ebus_soft_state(i)	\
	ddi_soft_state_zalloc(per_ebus_state, (i))

#define	free_ebus_soft_state(i)	\
	ddi_soft_state_free(per_ebus_state, (i))


#define	getprop(dip, name, addr, intp)		\
		ddi_getlongprop(DDI_DEV_T_NONE, (dip), DDI_PROP_DONTPASS, \
				(name), (caddr_t)(addr), (intp))

/*
 * register offsets and lengths:
 */
#define	TCR_OFFSET	0x710000
#define	TCR_LENGTH	12

/*
 * timing control register settings:
 */
#define	TCR1		0x08101008
#define	TCR2		0x08100020
#define	TCR3		0x00000020


#if defined(DEBUG)
#define	D_IDENTIFY	0x00000001
#define	D_ATTACH	0x00000002
#define	D_DETACH	0x00000004
#define	D_MAP		0x00000008
#define	D_CTLOPS	0x00000010
#define	D_G_ISPEC	0x00000020
#define	D_A_ISPEC	0x00000040
#define	D_R_ISPEC	0x00000080
#define	D_INTR		0x00000100

#define	DBG(flag, psp, fmt)	\
	ebus_debug(flag, psp, fmt, 0, 0, 0, 0, 0);
#define	DBG1(flag, psp, fmt, a1)	\
	ebus_debug(flag, psp, fmt, (int)(a1), 0, 0, 0, 0);
#define	DBG2(flag, psp, fmt, a1, a2)	\
	ebus_debug(flag, psp, fmt, (int)(a1), (int)(a2), 0, 0, 0);
#define	DBG3(flag, psp, fmt, a1, a2, a3)	\
	ebus_debug(flag, psp, fmt, (int)(a1), (int)(a2), (int)(a3), 0, 0);
#define	DBG4(flag, psp, fmt, a1, a2, a3, a4)	\
	ebus_debug(flag, psp, fmt, (int)(a1), (int)(a2), (int)(a3), \
		(int)(a4), 0);
#define	DBG5(flag, psp, fmt, a1, a2, a3, a4, a5)	\
	ebus_debug(flag, psp, fmt, (int)(a1), (int)(a2), (int)(a3), \
		(int)(a4), (int)(a5));
static void
ebus_debug(u_int, ebus_devstate_t *, char *, int, int, int, int, int);
#else
#define	DBG(flag, psp, fmt)
#define	DBG1(flag, psp, fmt, a1)
#define	DBG2(flag, psp, fmt, a1, a2)
#define	DBG3(flag, psp, fmt, a1, a2, a3)
#define	DBG4(flag, psp, fmt, a1, a2, a3, a4)
#define	DBG5(flag, psp, fmt, a1, a2, a3, a4, a5)
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_EBUS_H */
