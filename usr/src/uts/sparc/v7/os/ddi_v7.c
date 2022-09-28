/*
 * Copyright (c) 1991-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ddi_v7.c	1.7	96/09/24 SMI"

/*
 * sparc v7 specific DDI implementation
 */

/*
 * indicate that this is the implementation code.
 */
#define	SUNDDI_IMPL

#include <sys/types.h>
#include <sys/kmem.h>

#include <sys/dditypes.h>
#include <sys/ddidmareq.h>
#include <sys/devops.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/ddi_isa.h>
#include <sys/ddi_implfuncs.h>

/*
 * DDI(Sun) Function and flag definitions:
 */

static uintptr_t impl_acc_hdl_id = 0;

/*
 * access handle allocator
 */
ddi_acc_hdl_t *
impl_acc_hdl_get(ddi_acc_handle_t hdl)
{
	/*
	 * recast to ddi_acc_hdl_t instead of
	 * casting to ddi_acc_impl_t and then return the ah_platform_private
	 *
	 * this optimization based on the ddi_acc_hdl_t is the
	 * first member of the ddi_acc_impl_t.
	 */
	return ((ddi_acc_hdl_t *)hdl);
}

ddi_acc_handle_t
impl_acc_hdl_alloc(int (*waitfp)(caddr_t), caddr_t arg)
{
	ddi_acc_impl_t *hp;
	int sleepflag;

	sleepflag = ((waitfp == (int (*)())KM_SLEEP) ? KM_SLEEP : KM_NOSLEEP);
	/*
	 * Allocate and initialize the data access handle.
	 */
	hp = kmem_zalloc(sizeof (ddi_acc_impl_t), sleepflag);
	if (!hp) {
		if ((waitfp != (int (*)())KM_SLEEP) &&
			(waitfp != (int (*)())KM_NOSLEEP))
			ddi_set_callback(waitfp, arg, &impl_acc_hdl_id);
		return (NULL);
	}

	hp->ahi_common.ah_platform_private = (void *)hp;
	return ((ddi_acc_handle_t)hp);
}

void
impl_acc_hdl_free(ddi_acc_handle_t handle)
{
	ddi_acc_impl_t *hp;

	hp = (ddi_acc_impl_t *)handle;
	if (hp) {
		kmem_free(hp, sizeof (*hp));
		if (impl_acc_hdl_id)
			ddi_run_callback(&impl_acc_hdl_id);
	}
}

void
impl_acc_hdl_init(ddi_acc_hdl_t *handlep)
{
	ddi_acc_impl_t *hp;

	if (!handlep)
		return;
	hp = (ddi_acc_impl_t *)handlep->ah_platform_private;

	/*
	 * check for SW byte-swapping
	 */
	hp->ahi_get8 = i_ddi_get8;
	hp->ahi_put8 = i_ddi_put8;
	hp->ahi_rep_get8 = i_ddi_rep_get8;
	hp->ahi_rep_put8 = i_ddi_rep_put8;
	if (handlep->ah_acc.devacc_attr_endian_flags & DDI_STRUCTURE_LE_ACC) {
		hp->ahi_get16 = i_ddi_swap_get16;
		hp->ahi_get32 = i_ddi_swap_get32;
		hp->ahi_get64 = i_ddi_swap_get64;
		hp->ahi_put16 = i_ddi_swap_put16;
		hp->ahi_put32 = i_ddi_swap_put32;
		hp->ahi_put64 = i_ddi_swap_put64;
		hp->ahi_rep_get16 = i_ddi_swap_rep_get16;
		hp->ahi_rep_get32 = i_ddi_swap_rep_get32;
		hp->ahi_rep_get64 = i_ddi_swap_rep_get64;
		hp->ahi_rep_put16 = i_ddi_swap_rep_put16;
		hp->ahi_rep_put32 = i_ddi_swap_rep_put32;
		hp->ahi_rep_put64 = i_ddi_swap_rep_put64;
	} else {
		hp->ahi_get16 = i_ddi_get16;
		hp->ahi_get32 = i_ddi_get32;
		hp->ahi_get64 = i_ddi_get64;
		hp->ahi_put16 = i_ddi_put16;
		hp->ahi_put32 = i_ddi_put32;
		hp->ahi_put64 = i_ddi_put64;
		hp->ahi_rep_get16 = i_ddi_rep_get16;
		hp->ahi_rep_get32 = i_ddi_rep_get32;
		hp->ahi_rep_get64 = i_ddi_rep_get64;
		hp->ahi_rep_put16 = i_ddi_rep_put16;
		hp->ahi_rep_put32 = i_ddi_rep_put32;
		hp->ahi_rep_put64 = i_ddi_rep_put64;
	}
}

/*ARGSUSED*/
void
i_ddi_rep_get8(ddi_acc_impl_t *hp, uint8_t *host_addr, uint8_t *dev_addr,
	size_t repcount, uint_t flags)
{
	uint8_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_NO_AUTOINCR)
		while (repcount--)
			*h++ = *d;
	else
		while (repcount--)
			*h++ = *d++;
}

/*ARGSUSED*/
void
i_ddi_rep_get16(ddi_acc_impl_t *hp, uint16_t *host_addr, uint16_t *dev_addr,
	size_t repcount, uint_t flags)
{
	uint16_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_NO_AUTOINCR)
		while (repcount--)
			*h++ = *d;
	else
		while (repcount--)
			*h++ = *d++;
}

/*ARGSUSED*/
void
i_ddi_swap_rep_get16(ddi_acc_impl_t *hp, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags)
{
	uint16_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_NO_AUTOINCR)
		while (repcount--)
			*h++ = ddi_swap16(*d);
	else
		while (repcount--)
			*h++ = ddi_swap16(*d++);
}

/*ARGSUSED*/
void
i_ddi_rep_get32(ddi_acc_impl_t *hp, uint32_t *host_addr, uint32_t *dev_addr,
	size_t repcount, uint_t flags)
{
	uint32_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_NO_AUTOINCR)
		while (repcount--)
			*h++ = hp->ahi_get32(hp, d);
	else
		while (repcount--)
			*h++ = hp->ahi_get32(hp, d++);
}

/*ARGSUSED*/
void
i_ddi_swap_rep_get32(ddi_acc_impl_t *hp, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags)
{
	uint32_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_NO_AUTOINCR)
		while (repcount--)
			*h++ = ddi_swap32(*d);
	else
		while (repcount--)
			*h++ = ddi_swap32(*d++);
}

/*ARGSUSED*/
void
i_ddi_rep_get64(ddi_acc_impl_t *hp, uint64_t *host_addr, uint64_t *dev_addr,
	size_t repcount, uint_t flags)
{
	uint64_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_NO_AUTOINCR)
		while (repcount--)
			*h++ = *d;
	else
		while (repcount--)
			*h++ = *d++;
}

/*ARGSUSED*/
void
i_ddi_swap_rep_get64(ddi_acc_impl_t *hp, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags)
{
	uint64_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_NO_AUTOINCR)
		while (repcount--)
			*h++ = ddi_swap64(*d);
	else
		while (repcount--)
			*h++ = ddi_swap64(*d++);
}

/*ARGSUSED*/
void
i_ddi_rep_put8(ddi_acc_impl_t *hp, uint8_t *host_addr, uint8_t *dev_addr,
	size_t repcount, uint_t flags)
{
	uint8_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_NO_AUTOINCR)
		while (repcount--)
			*d = *h++;
	else
		while (repcount--)
			*d++ = *h++;
}

/*ARGSUSED*/
void
i_ddi_rep_put16(ddi_acc_impl_t *hp, uint16_t *host_addr, uint16_t *dev_addr,
	size_t repcount, uint_t flags)
{
	uint16_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_NO_AUTOINCR)
		while (repcount--)
			*d = *h++;
	else
		while (repcount--)
			*d++ = *h++;
}

/*ARGSUSED*/
void
i_ddi_swap_rep_put16(ddi_acc_impl_t *hp, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags)
{
	uint16_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_NO_AUTOINCR)
		while (repcount--)
			*d = ddi_swap16(*h++);
	else
		while (repcount--)
			*d++ = ddi_swap16(*h++);
}

/*ARGSUSED*/
void
i_ddi_rep_put32(ddi_acc_impl_t *hp, uint32_t *host_addr, uint32_t *dev_addr,
	size_t repcount, uint_t flags)
{
	uint32_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_NO_AUTOINCR)
		while (repcount--)
			*d = *h++;
	else
		while (repcount--)
			*d++ = *h++;
}

/*ARGSUSED*/
void
i_ddi_swap_rep_put32(ddi_acc_impl_t *hp, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags)
{
	uint32_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_NO_AUTOINCR)
		while (repcount--)
			*d = ddi_swap32(*h++);
	else
		while (repcount--)
			*d++ = ddi_swap32(*h++);
}

/*ARGSUSED*/
void
i_ddi_rep_put64(ddi_acc_impl_t *hp, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags)
{
	uint64_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_NO_AUTOINCR)
		while (repcount--)
			*d = *h++;
	else
		while (repcount--)
			*d++ = *h++;
}

/*ARGSUSED*/
void
i_ddi_swap_rep_put64(ddi_acc_impl_t *hp, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags)
{
	uint64_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_NO_AUTOINCR)
		while (repcount--)
			*d = ddi_swap64(*h++);
	else
		while (repcount--)
			*d++ = ddi_swap64(*h++);
}

/*ARGSUSED*/
uint16_t
i_ddi_swap_get16(ddi_acc_impl_t *hdlp, uint16_t *addr)
{
	return (ddi_swap16(*addr));
}

/*ARGSUSED*/
uint32_t
i_ddi_swap_get32(ddi_acc_impl_t *hdlp, uint32_t *addr)
{
	return (ddi_swap32(*addr));
}

/*ARGSUSED*/
uint64_t
i_ddi_swap_get64(ddi_acc_impl_t *hdlp, uint64_t *addr)
{
	return (ddi_swap64(*addr));
}

/*ARGSUSED*/
void
i_ddi_swap_put16(ddi_acc_impl_t *hdlp, uint16_t *addr, uint16_t value)
{
	*addr = ddi_swap16(value);
}

/*ARGSUSED*/
void
i_ddi_swap_put32(ddi_acc_impl_t *hdlp, uint32_t *addr, uint32_t value)
{
	*addr = ddi_swap32(value);
}

/*ARGSUSED*/
void
i_ddi_swap_put64(ddi_acc_impl_t *hdlp, uint64_t *addr, uint64_t value)
{
	*addr = ddi_swap64(value);
}

void
ddi_io_rep_get8(ddi_acc_handle_t handle,
	uint8_t *host_addr, int dev_port, size_t repcount)
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_get8)
		((ddi_acc_impl_t *)handle, host_addr, (uint8_t *)dev_port,
		repcount, DDI_DEV_NO_AUTOINCR);
}

void
ddi_io_rep_get16(ddi_acc_handle_t handle,
	uint16_t *host_addr, int dev_port, size_t repcount)
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_get16)
		((ddi_acc_impl_t *)handle, host_addr, (uint16_t *)dev_port,
		repcount, DDI_DEV_NO_AUTOINCR);
}

void
ddi_io_rep_get32(ddi_acc_handle_t handle,
	uint32_t *host_addr, int dev_port, size_t repcount)
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_get32)
		((ddi_acc_impl_t *)handle, host_addr, (uint32_t *)dev_port,
		repcount, DDI_DEV_NO_AUTOINCR);
}

void
ddi_io_rep_put8(ddi_acc_handle_t handle,
	uint8_t *host_addr, int dev_port, size_t repcount)
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_put8)
		((ddi_acc_impl_t *)handle, host_addr, (uint8_t *)dev_port,
		repcount, DDI_DEV_NO_AUTOINCR);
}

void
ddi_io_rep_put16(ddi_acc_handle_t handle,
	uint16_t *host_addr, int dev_port, size_t repcount)
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_put16)
		((ddi_acc_impl_t *)handle, host_addr, (uint16_t *)dev_port,
		repcount, DDI_DEV_NO_AUTOINCR);
}

void
ddi_io_rep_put32(ddi_acc_handle_t handle,
	uint32_t *host_addr, int dev_port, size_t repcount)
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_put32)
		((ddi_acc_impl_t *)handle, host_addr, (uint32_t *)dev_port,
		repcount, DDI_DEV_NO_AUTOINCR);
}
