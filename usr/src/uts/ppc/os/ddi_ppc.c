/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ddi_ppc.c	1.20	96/09/24 SMI"

#include <sys/conf.h>
#include <sys/kmem.h>
#include <sys/ddi_impldefs.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>


/*
 * DDI DMA Engine functions for x86 and PowerPC.
 * These functions are more naturally generic, but do not apply to SPARC.
 */

int
ddi_dmae_alloc(dev_info_t *dip, int chnl, int (*dmae_waitfp)(), caddr_t arg)
{
	return (ddi_dma_mctl(dip, dip, 0, DDI_DMA_E_ACQUIRE,
	    (off_t *)dmae_waitfp, (u_int *)arg, (caddr_t *)chnl, 0));
}

int
ddi_dmae_release(dev_info_t *dip, int chnl)
{
	return (ddi_dma_mctl(dip, dip, 0, DDI_DMA_E_FREE, 0, 0,
	    (caddr_t *)chnl, 0));
}

int
ddi_dmae_getlim(dev_info_t *dip, ddi_dma_lim_t *limitsp)
{
	return (ddi_dma_mctl(dip, dip, 0, DDI_DMA_E_GETLIM, 0, 0,
	    (caddr_t *)limitsp, 0));
}

int
ddi_dmae_getattr(dev_info_t *dip, ddi_dma_attr_t *attrp)
{
	return (ddi_dma_mctl(dip, dip, 0, DDI_DMA_E_GETATTR, 0, 0,
	    (caddr_t *)attrp, 0));
}

int
ddi_dmae_1stparty(dev_info_t *dip, int chnl)
{
	return (ddi_dma_mctl(dip, dip, 0, DDI_DMA_E_1STPTY, 0, 0,
	    (caddr_t *)chnl, 0));
}

int
ddi_dmae_prog(dev_info_t *dip, struct ddi_dmae_req *dmaereqp,
	ddi_dma_cookie_t *cookiep, int chnl)
{
	return (ddi_dma_mctl(dip, dip, 0, DDI_DMA_E_PROG, (off_t *)dmaereqp,
	    (u_int *)cookiep, (caddr_t *)chnl, 0));
}

int
ddi_dmae_swsetup(dev_info_t *dip, struct ddi_dmae_req *dmaereqp,
	ddi_dma_cookie_t *cookiep, int chnl)
{
	return (ddi_dma_mctl(dip, dip, 0, DDI_DMA_E_SWSETUP, (off_t *)dmaereqp,
	    (u_int *)cookiep, (caddr_t *)chnl, 0));
}

int
ddi_dmae_swstart(dev_info_t *dip, int chnl)
{
	return (ddi_dma_mctl(dip, dip, 0, DDI_DMA_E_SWSTART, 0, 0,
	    (caddr_t *)chnl, 0));
}

int
ddi_dmae_stop(dev_info_t *dip, int chnl)
{
	return (ddi_dma_mctl(dip, dip, 0, DDI_DMA_E_STOP, 0, 0,
	    (caddr_t *)chnl, 0));
}

int
ddi_dmae_enable(dev_info_t *dip, int chnl)
{
	return (ddi_dma_mctl(dip, dip, 0, DDI_DMA_E_ENABLE, 0, 0,
	    (caddr_t *)chnl, 0));
}

int
ddi_dmae_disable(dev_info_t *dip, int chnl)
{
	return (ddi_dma_mctl(dip, dip, 0, DDI_DMA_E_DISABLE, 0, 0,
	    (caddr_t *)chnl, 0));
}

int
ddi_dmae_getcnt(dev_info_t *dip, int chnl, int *countp)
{
	return (ddi_dma_mctl(dip, dip, 0, DDI_DMA_E_GETCNT, 0, (u_int *)countp,
	    (caddr_t *)chnl, 0));
}

/*
 * implementation specific access handle and routines:
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
	hp = (ddi_acc_impl_t *)kmem_zalloc(sizeof (ddi_acc_impl_t), sleepflag);
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

	hp->ahi_get8 = i_ddi_vaddr_get8;
	hp->ahi_put8 = i_ddi_vaddr_put8;
	hp->ahi_rep_get8 = i_ddi_vaddr_rep_get8;
	hp->ahi_rep_put8 = i_ddi_vaddr_rep_put8;
	/*
	 * check for BIG endian access
	 */
	if (handlep->ah_acc.devacc_attr_endian_flags ==
		DDI_STRUCTURE_BE_ACC) {

		hp->ahi_get16 = i_ddi_vaddr_swap_get16;
		hp->ahi_get32 = i_ddi_vaddr_swap_get32;
		hp->ahi_get64 = i_ddi_vaddr_swap_get64;
		hp->ahi_put16 = i_ddi_vaddr_swap_put16;
		hp->ahi_put32 = i_ddi_vaddr_swap_put32;
		hp->ahi_put64 = i_ddi_vaddr_swap_put64;
		hp->ahi_rep_get16 = i_ddi_vaddr_swap_rep_get16;
		hp->ahi_rep_get32 = i_ddi_vaddr_swap_rep_get32;
		hp->ahi_rep_get64 = i_ddi_vaddr_swap_rep_get64;
		hp->ahi_rep_put16 = i_ddi_vaddr_swap_rep_put16;
		hp->ahi_rep_put32 = i_ddi_vaddr_swap_rep_put32;
		hp->ahi_rep_put64 = i_ddi_vaddr_swap_rep_put64;
	} else {
		hp->ahi_get16 = i_ddi_vaddr_get16;
		hp->ahi_get32 = i_ddi_vaddr_get32;
		hp->ahi_get64 = i_ddi_vaddr_get64;
		hp->ahi_put16 = i_ddi_vaddr_put16;
		hp->ahi_put32 = i_ddi_vaddr_put32;
		hp->ahi_put64 = i_ddi_vaddr_put64;
		hp->ahi_rep_get16 = i_ddi_vaddr_rep_get16;
		hp->ahi_rep_get32 = i_ddi_vaddr_rep_get32;
		hp->ahi_rep_get64 = i_ddi_vaddr_rep_get64;
		hp->ahi_rep_put16 = i_ddi_vaddr_rep_put16;
		hp->ahi_rep_put32 = i_ddi_vaddr_rep_put32;
		hp->ahi_rep_put64 = i_ddi_vaddr_rep_put64;
	}
}

/*
 * XXXPPC:  The following should perhaps be implemented in assembler,
 * or, as suggested in 1225648, macros.
 */
#ifdef _LP64
uint8_t
ddi_get8(ddi_acc_handle_t handle, uint8_t *addr)
#else /* _ILP32 */
uint8_t
ddi_getb(ddi_acc_handle_t handle, uint8_t *addr)
#endif
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get8)
		((ddi_acc_impl_t *)handle, addr));
}

#ifdef _LP64
uint8_t
ddi_mem_get8(ddi_acc_handle_t handle, uint8_t *addr)
#else /* _ILP32 */
uint8_t
ddi_mem_getb(ddi_acc_handle_t handle, uint8_t *addr)
#endif
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get8)
		((ddi_acc_impl_t *)handle, addr));
}

#ifdef _LP64
uint8_t
ddi_io_get8(ddi_acc_handle_t handle, int addr)
#else /* _ILP32 */
uint8_t
ddi_io_getb(ddi_acc_handle_t handle, int addr)
#endif
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get8)
		((ddi_acc_impl_t *)handle, (uint8_t *)addr));
}

#ifdef _LP64
uint16_t
ddi_get16(ddi_acc_handle_t handle, uint16_t *addr)
#else /* _ILP32 */
uint16_t
ddi_getw(ddi_acc_handle_t handle, uint16_t *addr)
#endif
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get16)
		((ddi_acc_impl_t *)handle, addr));
}

#ifdef _LP64
uint16_t
ddi_mem_get16(ddi_acc_handle_t handle, uint16_t *addr)
#else /* _ILP32 */
uint16_t
ddi_mem_getw(ddi_acc_handle_t handle, uint16_t *addr)
#endif
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get16)
		((ddi_acc_impl_t *)handle, addr));
}

#ifdef _LP64
uint16_t
ddi_io_get16(ddi_acc_handle_t handle, int addr)
#else /* _ILP32 */
uint16_t
ddi_io_getw(ddi_acc_handle_t handle, int addr)
#endif
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get16)
		((ddi_acc_impl_t *)handle, (uint16_t *)addr));
}

#ifdef _LP64
uint32_t
ddi_get32(ddi_acc_handle_t handle, uint32_t *addr)
#else /* _ILP32 */
uint32_t
ddi_getl(ddi_acc_handle_t handle, uint32_t *addr)
#endif
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get32)
		((ddi_acc_impl_t *)handle, addr));
}

#ifdef _LP64
uint32_t
ddi_mem_get32(ddi_acc_handle_t handle, uint32_t *addr)
#else /* _ILP32 */
uint32_t
ddi_mem_getl(ddi_acc_handle_t handle, uint32_t *addr)
#endif
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get32)
		((ddi_acc_impl_t *)handle, addr));
}

#ifdef _LP64
uint32_t
ddi_io_get32(ddi_acc_handle_t handle, int addr)
#else /* _ILP32 */
uint32_t
ddi_io_getl(ddi_acc_handle_t handle, int addr)
#endif
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get32)
		((ddi_acc_impl_t *)handle, (uint32_t *)addr));
}

#ifdef _LP64
uint64_t
ddi_get64(ddi_acc_handle_t handle, uint64_t *addr)
#else /* _ILP32 */
uint64_t
ddi_getll(ddi_acc_handle_t handle, uint64_t *addr)
#endif
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get64)
		((ddi_acc_impl_t *)handle, addr));
}

#ifdef _LP64
uint64_t
ddi_mem_get64(ddi_acc_handle_t handle, uint64_t *addr)
#else /* _ILP32 */
uint64_t
ddi_mem_getll(ddi_acc_handle_t handle, uint64_t *addr)
#endif
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get64)
		((ddi_acc_impl_t *)handle, addr));
}

#ifdef _LP64
void
ddi_put8(ddi_acc_handle_t handle, uint8_t *addr, uint8_t value)
#else /* _ILP32 */
void
ddi_putb(ddi_acc_handle_t handle, uint8_t *addr, uint8_t value)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_put8)
		((ddi_acc_impl_t *)handle, addr, value);
}

#ifdef _LP64
void
ddi_mem_put8(ddi_acc_handle_t handle, uint8_t *addr, uint8_t value)
#else /* _ILP32 */
void
ddi_mem_putb(ddi_acc_handle_t handle, uint8_t *addr, uint8_t value)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_put8)
		((ddi_acc_impl_t *)handle, addr, value);
}

#ifdef _LP64
void
ddi_io_put8(ddi_acc_handle_t handle, int addr, uint8_t value)
#else /* _ILP32 */
void
ddi_io_putb(ddi_acc_handle_t handle, int addr, uint8_t value)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_put8)
		((ddi_acc_impl_t *)handle, (uint8_t *)addr, value);
}

#ifdef _LP64
void
ddi_put16(ddi_acc_handle_t handle, uint16_t *addr, uint16_t value)
#else /* _ILP32 */
void
ddi_putw(ddi_acc_handle_t handle, uint16_t *addr, uint16_t value)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_put16)
		((ddi_acc_impl_t *)handle, addr, value);
}

#ifdef _LP64
void
ddi_mem_put16(ddi_acc_handle_t handle, uint16_t *addr, uint16_t value)
#else /* _ILP32 */
void
ddi_mem_putw(ddi_acc_handle_t handle, uint16_t *addr, uint16_t value)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_put16)
		((ddi_acc_impl_t *)handle, addr, value);
}

#ifdef _LP64
void
ddi_io_put16(ddi_acc_handle_t handle, int addr, uint16_t value)
#else /* _ILP32 */
void
ddi_io_putw(ddi_acc_handle_t handle, int addr, uint16_t value)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_put16)
		((ddi_acc_impl_t *)handle, (uint16_t *)addr, value);
}

#ifdef _LP64
void
ddi_put32(ddi_acc_handle_t handle, uint32_t *addr, uint32_t value)
#else /* _ILP32 */
void
ddi_putl(ddi_acc_handle_t handle, uint32_t *addr, uint32_t value)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_put32)
		((ddi_acc_impl_t *)handle, addr, value);
}

#ifdef _LP64
void
ddi_mem_put32(ddi_acc_handle_t handle, uint32_t *addr, uint32_t value)
#else /* _ILP32 */
void
ddi_mem_putl(ddi_acc_handle_t handle, uint32_t *addr, uint32_t value)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_put32)
		((ddi_acc_impl_t *)handle, addr, value);
}

#ifdef _LP64
void
ddi_io_put32(ddi_acc_handle_t handle, int addr, uint32_t value)
#else /* _ILP32 */
void
ddi_io_putl(ddi_acc_handle_t handle, int addr, uint32_t value)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_put32)
		((ddi_acc_impl_t *)handle, (uint32_t *)addr, value);
}

#ifdef _LP64
void
ddi_put64(ddi_acc_handle_t handle, uint64_t *addr, uint64_t value)
#else /* _ILP32 */
void
ddi_putll(ddi_acc_handle_t handle, uint64_t *addr, uint64_t value)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_put64)
		((ddi_acc_impl_t *)handle, addr, value);
}

#ifdef _LP64
void
ddi_mem_put64(ddi_acc_handle_t handle, uint64_t *addr, uint64_t value)
#else /* _ILP32 */
void
ddi_mem_putll(ddi_acc_handle_t handle, uint64_t *addr, uint64_t value)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_put64)
		((ddi_acc_impl_t *)handle, addr, value);
}

#ifdef _LP64
void
ddi_rep_get8(ddi_acc_handle_t handle,
	uint8_t *host_addr, uint8_t *addr, size_t repcount, uint_t flags)
#else /* _ILP32 */
void
ddi_rep_getb(ddi_acc_handle_t handle,
	uint8_t *host_addr, uint8_t *addr, size_t repcount, uint_t flags)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_get8)
		((ddi_acc_impl_t *)handle, host_addr, addr, repcount, flags);
}

#ifdef _LP64
void
ddi_mem_rep_get8(ddi_acc_handle_t handle,
	uint8_t *host_addr, uint8_t *addr, size_t repcount, uint_t flags)
#else /* _ILP32 */
void
ddi_mem_rep_getb(ddi_acc_handle_t handle,
	uint8_t *host_addr, uint8_t *addr, size_t repcount, uint_t flags)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_get8)
		((ddi_acc_impl_t *)handle, host_addr, addr, repcount, flags);
}

#ifdef _LP64
void
ddi_io_rep_get8(ddi_acc_handle_t handle,
	uint8_t *host_addr, int addr, size_t repcount)
#else /* _ILP32 */
void
ddi_io_rep_getb(ddi_acc_handle_t handle,
	uint8_t *host_addr, int addr, size_t repcount)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_get8)
		((ddi_acc_impl_t *)handle, host_addr, (uint8_t *)addr,
		repcount, DDI_DEV_NO_AUTOINCR);
}

#ifdef _LP64
void
ddi_rep_get16(ddi_acc_handle_t handle,
	uint16_t *host_addr, uint16_t *addr, size_t repcount, uint_t flags)
#else /* _ILP32 */
void
ddi_rep_getw(ddi_acc_handle_t handle,
	uint16_t *host_addr, uint16_t *addr, size_t repcount, uint_t flags)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_get16)
		((ddi_acc_impl_t *)handle, host_addr, addr, repcount, flags);
}

#ifdef _LP64
void
ddi_mem_rep_get16(ddi_acc_handle_t handle,
	uint16_t *host_addr, uint16_t *addr, size_t repcount, uint_t flags)
#else /* _ILP32 */
void
ddi_mem_rep_getw(ddi_acc_handle_t handle,
	uint16_t *host_addr, uint16_t *addr, size_t repcount, uint_t flags)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_get16)
		((ddi_acc_impl_t *)handle, host_addr, addr, repcount, flags);
}

#ifdef _LP64
void
ddi_io_rep_get16(ddi_acc_handle_t handle,
	uint16_t *host_addr, int addr, size_t repcount)
#else /* _ILP32 */
void
ddi_io_rep_getw(ddi_acc_handle_t handle,
	uint16_t *host_addr, int addr, size_t repcount)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_get16)
		((ddi_acc_impl_t *)handle, host_addr, (uint16_t *)addr,
		repcount, DDI_DEV_NO_AUTOINCR);
}

#ifdef _LP64
void
ddi_rep_get32(ddi_acc_handle_t handle,
	uint32_t *host_addr, uint32_t *addr, size_t repcount, uint_t flags)
#else /* _ILP32 */
void
ddi_rep_getl(ddi_acc_handle_t handle,
	uint32_t *host_addr, uint32_t *addr, size_t repcount, uint_t flags)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_get32)
		((ddi_acc_impl_t *)handle, host_addr, addr, repcount, flags);
}

#ifdef _LP64
void
ddi_mem_rep_get32(ddi_acc_handle_t handle,
	uint32_t *host_addr, uint32_t *addr, size_t repcount, uint_t flags)
#else /* _ILP32 */
void
ddi_mem_rep_getl(ddi_acc_handle_t handle,
	uint32_t *host_addr, uint32_t *addr, size_t repcount, uint_t flags)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_get32)
		((ddi_acc_impl_t *)handle, host_addr, addr, repcount, flags);
}

#ifdef _LP64
void
ddi_io_rep_get32(ddi_acc_handle_t handle,
	uint32_t *host_addr, int addr, size_t repcount)
#else /* _ILP32 */
void
ddi_io_rep_getl(ddi_acc_handle_t handle,
	uint32_t *host_addr, int addr, size_t repcount)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_get32)
		((ddi_acc_impl_t *)handle, host_addr, (uint32_t *)addr,
		repcount, DDI_DEV_NO_AUTOINCR);
}

#ifdef _LP64
void
ddi_rep_get64(ddi_acc_handle_t handle, uint64_t *host_addr, uint64_t *addr,
	size_t repcount, uint_t flags)
#else /* _ILP32 */
void
ddi_rep_getll(ddi_acc_handle_t handle, uint64_t *host_addr, uint64_t *addr,
	size_t repcount, uint_t flags)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_get64)
		((ddi_acc_impl_t *)handle, host_addr, addr, repcount, flags);
}

#ifdef _LP64
void
ddi_mem_rep_get64(ddi_acc_handle_t handle, uint64_t *host_addr, uint64_t *addr,
	size_t repcount, uint_t flags)
#else /* _ILP32 */
void
ddi_mem_rep_getll(ddi_acc_handle_t handle, uint64_t *host_addr, uint64_t *addr,
	size_t repcount, uint_t flags)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_get64)
		((ddi_acc_impl_t *)handle, host_addr, addr, repcount, flags);
}

#ifdef _LP64
void
ddi_rep_put8(ddi_acc_handle_t handle,
	uint8_t *host_addr, uint8_t *addr, size_t repcount, uint_t flags)
#else /* _ILP32 */
void
ddi_rep_putb(ddi_acc_handle_t handle,
	uint8_t *host_addr, uint8_t *addr, size_t repcount, uint_t flags)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_put8)
		((ddi_acc_impl_t *)handle, host_addr, addr, repcount, flags);
}

#ifdef _LP64
void
ddi_mem_rep_put8(ddi_acc_handle_t handle,
	uint8_t *host_addr, uint8_t *addr, size_t repcount, uint_t flags)
#else /* _ILP32 */
void
ddi_mem_rep_putb(ddi_acc_handle_t handle,
	uint8_t *host_addr, uint8_t *addr, size_t repcount, uint_t flags)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_put8)
		((ddi_acc_impl_t *)handle, host_addr, addr, repcount, flags);
}

#ifdef _LP64
void
ddi_io_rep_put8(ddi_acc_handle_t handle,
	uint8_t *host_addr, int addr, size_t repcount)
#else /* _ILP32 */
void
ddi_io_rep_putb(ddi_acc_handle_t handle,
	uint8_t *host_addr, int addr, size_t repcount)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_put8)
		((ddi_acc_impl_t *)handle, host_addr, (uint8_t *)addr,
		repcount, DDI_DEV_NO_AUTOINCR);
}

#ifdef _LP64
void
ddi_rep_put16(ddi_acc_handle_t handle,
	uint16_t *host_addr, uint16_t *addr, size_t repcount, uint_t flags)
#else /* _ILP32 */
void
ddi_rep_putw(ddi_acc_handle_t handle,
	uint16_t *host_addr, uint16_t *addr, size_t repcount, uint_t flags)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_put16)
		((ddi_acc_impl_t *)handle, host_addr, addr, repcount, flags);
}

#ifdef _LP64
void
ddi_mem_rep_put16(ddi_acc_handle_t handle,
	uint16_t *host_addr, uint16_t *addr, size_t repcount, uint_t flags)
#else /* _ILP32 */
void
ddi_mem_rep_putw(ddi_acc_handle_t handle,
	uint16_t *host_addr, uint16_t *addr, size_t repcount, uint_t flags)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_put16)
		((ddi_acc_impl_t *)handle, host_addr, addr, repcount, flags);
}

#ifdef _LP64
void
ddi_io_rep_put16(ddi_acc_handle_t handle,
	uint16_t *host_addr, int addr, size_t repcount)
#else /* _ILP32 */
void
ddi_io_rep_putw(ddi_acc_handle_t handle,
	uint16_t *host_addr, int addr, size_t repcount)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_put16)
		((ddi_acc_impl_t *)handle, host_addr, (uint16_t *)addr,
		repcount, DDI_DEV_NO_AUTOINCR);
}

#ifdef _LP64
void
ddi_rep_put32(ddi_acc_handle_t handle,
	uint32_t *host_addr, uint32_t *addr, size_t repcount, uint_t flags)
#else /* _ILP32 */
void
ddi_rep_putl(ddi_acc_handle_t handle,
	uint32_t *host_addr, uint32_t *addr, size_t repcount, uint_t flags)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_put32)
		((ddi_acc_impl_t *)handle, host_addr, addr, repcount, flags);
}

#ifdef _LP64
void
ddi_mem_rep_put32(ddi_acc_handle_t handle,
	uint32_t *host_addr, uint32_t *addr, size_t repcount, uint_t flags)
#else /* _ILP32 */
void
ddi_mem_rep_putl(ddi_acc_handle_t handle,
	uint32_t *host_addr, uint32_t *addr, size_t repcount, uint_t flags)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_put32)
		((ddi_acc_impl_t *)handle, host_addr, addr, repcount, flags);
}

#ifdef _LP64
void
ddi_io_rep_put32(ddi_acc_handle_t handle,
	uint32_t *host_addr, int addr, size_t repcount)
#else /* _ILP32 */
void
ddi_io_rep_putl(ddi_acc_handle_t handle,
	uint32_t *host_addr, int addr, size_t repcount)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_put32)
		((ddi_acc_impl_t *)handle, host_addr, (uint32_t *)addr,
		repcount, DDI_DEV_NO_AUTOINCR);
}

#ifdef _LP64
void
ddi_rep_put64(ddi_acc_handle_t handle, uint64_t *host_addr,
	uint64_t *addr, size_t repcount, uint_t flags)
#else /* _ILP32 */
void
ddi_rep_putll(ddi_acc_handle_t handle, uint64_t *host_addr,
	uint64_t *addr, size_t repcount, uint_t flags)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_put64)
		((ddi_acc_impl_t *)handle, host_addr, addr, repcount, flags);
}

#ifdef _LP64
void
ddi_mem_rep_put64(ddi_acc_handle_t handle, uint64_t *host_addr,
	uint64_t *addr, size_t repcount, uint_t flags)
#else /* _ILP32 */
void
ddi_mem_rep_putll(ddi_acc_handle_t handle, uint64_t *host_addr,
	uint64_t *addr, size_t repcount, uint_t flags)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_put64)
		((ddi_acc_impl_t *)handle, host_addr, addr, repcount, flags);
}

/*
 * The followings are low-level routines for data access.
 *
 * All of these routines should be implemented in assembly. Those
 * that have been rewritten be found in ~ml/ddi_ppc_asm.s
 *
 * We do the eieio before the operation (as opposed to after)
 * because that's how inb/outb do it.
 */
extern void eieio(void);

/*ARGSUSED*/
uint16_t
i_ddi_vaddr_swap_get16(ddi_acc_impl_t *hdlp, uint16_t *addr)
{
	eieio();
	return (ddi_swap16(*addr));
}

/*ARGSUSED*/
uint32_t
i_ddi_vaddr_swap_get32(ddi_acc_impl_t *hdlp, uint32_t *addr)
{
	eieio();
	return (ddi_swap32(*addr));
}

/*ARGSUSED*/
uint64_t
i_ddi_vaddr_swap_get64(ddi_acc_impl_t *hdlp, uint64_t *addr)
{
	eieio();
	return (ddi_swap64(*addr));
}

/*ARGSUSED*/
void
i_ddi_vaddr_swap_put16(ddi_acc_impl_t *hdlp, uint16_t *addr, uint16_t value)
{
	eieio();
	*addr = ddi_swap16(value);
}

/*ARGSUSED*/
void
i_ddi_vaddr_swap_put32(ddi_acc_impl_t *hdlp, uint32_t *addr, uint32_t value)
{
	eieio();
	*addr = ddi_swap32(value);
}

/*ARGSUSED*/
void
i_ddi_vaddr_swap_put64(ddi_acc_impl_t *hdlp, uint64_t *addr, uint64_t value)
{
	eieio();
	*addr = ddi_swap64(value);
}

/*ARGSUSED*/
void
i_ddi_vaddr_rep_get8(ddi_acc_impl_t *hdlp, uint8_t *host_addr,
	uint8_t *dev_addr, size_t repcount, uint_t flags)
{
	uint8_t	*h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR) {
		for (; repcount; repcount--) {
			eieio();
			*h++ = *d++;
		}
	} else {
		/*
		 * eieio can be outside loop here because processor guarantees
		 * order of accesses to a single address
		 */
		eieio();
		for (; repcount; repcount--)
			*h++ = *d;
	}
}

/*ARGSUSED*/
void
i_ddi_vaddr_rep_get16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags)
{
	uint16_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR) {
		for (; repcount; repcount--) {
			eieio();
			*h++ = *d++;
		}
	} else {
		/*
		 * eieio can be outside loop here because processor guarantees
		 * order of accesses to a single address
		 */
		eieio();
		for (; repcount; repcount--)
			*h++ = *d;
	}
}

/*ARGSUSED*/
void
i_ddi_vaddr_swap_rep_get16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags)
{
	uint16_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR) {
		for (; repcount; repcount--) {
			eieio();
			*h++ = ddi_swap16(*d++);
		}
	} else {
		/*
		 * eieio can be outside loop here because processor guarantees
		 * order of accesses to a single address
		 */
		eieio();
		for (; repcount; repcount--)
			*h++ = ddi_swap16(*d);
	}
}

/*ARGSUSED*/
void
i_ddi_vaddr_rep_get32(ddi_acc_impl_t *hdlp, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags)
{
	uint32_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR) {
		for (; repcount; repcount--) {
			eieio();
			*h++ = *d++;
		}
	} else {
		/*
		 * eieio can be outside loop here because processor guarantees
		 * order of accesses to a single address
		 */
		eieio();
		for (; repcount; repcount--)
			*h++ = *d;
	}
}

/*ARGSUSED*/
void
i_ddi_vaddr_swap_rep_get32(ddi_acc_impl_t *hdlp, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags)
{
	uint32_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR) {
		for (; repcount; repcount--) {
			eieio();
			*h++ = ddi_swap32(*d++);
		}
	} else {
		/*
		 * eieio can be outside loop here because processor guarantees
		 * order of accesses to a single address
		 */
		eieio();
		for (; repcount; repcount--)
			*h++ = ddi_swap32(*d);
	}
}

/*ARGSUSED*/
void
i_ddi_vaddr_rep_get64(ddi_acc_impl_t *hdlp, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags)
{
	uint64_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR) {
		for (; repcount; repcount--) {
			eieio();
			*h++ = *d++;
		}
	} else {
		/*
		 * eieio can be outside loop here because processor guarantees
		 * order of accesses to a single address
		 */
		eieio();
		for (; repcount; repcount--)
			*h++ = *d;
	}
}

/*ARGSUSED*/
void
i_ddi_vaddr_swap_rep_get64(ddi_acc_impl_t *hdlp, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags)
{
	uint64_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR) {
		for (; repcount; repcount--) {
			eieio();
			*h++ = ddi_swap64(*d++);
		}
	} else {
		/*
		 * eieio can be outside loop here because processor guarantees
		 * order of accesses to a single address
		 */
		eieio();
		for (; repcount; repcount--)
			*h++ = ddi_swap64(*d);
	}
}

/*ARGSUSED*/
void
i_ddi_vaddr_rep_put8(ddi_acc_impl_t *hdlp, uint8_t *host_addr,
	uint8_t *dev_addr, size_t repcount, uint_t flags)
{
	uint8_t	*h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR) {
		for (; repcount; repcount--) {
			eieio();
			*d++ = *h++;
		}
	} else {
		/*
		 * eieio can be outside loop here because processor guarantees
		 * order of accesses to a single address
		 */
		eieio();
		for (; repcount; repcount--)
			*d = *h++;
	}
}

/*ARGSUSED*/
void
i_ddi_vaddr_rep_put16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags)
{
	uint16_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR) {
		for (; repcount; repcount--) {
			eieio();
			*d++ = *h++;
		}
	} else {
		/*
		 * eieio can be outside loop here because processor guarantees
		 * order of accesses to a single address
		 */
		eieio();
		for (; repcount; repcount--)
			*d = *h++;
	}
}

/*ARGSUSED*/
void
i_ddi_vaddr_swap_rep_put16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags)
{
	uint16_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR) {
		for (; repcount; repcount--) {
			eieio();
			*d++ = ddi_swap16(*h++);
		}
	} else {
		/*
		 * eieio can be outside loop here because processor guarantees
		 * order of accesses to a single address
		 */
		eieio();
		for (; repcount; repcount--)
			*d = ddi_swap16(*h++);
	}
}

/*ARGSUSED*/
void
i_ddi_vaddr_rep_put32(ddi_acc_impl_t *hdlp, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags)
{
	uint32_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR) {
		for (; repcount; repcount--) {
			eieio();
			*d++ = *h++;
		}
	} else {
		/*
		 * eieio can be outside loop here because processor guarantees
		 * order of accesses to a single address
		 */
		eieio();
		for (; repcount; repcount--)
			*d = *h++;
	}
}

/*ARGSUSED*/
void
i_ddi_vaddr_swap_rep_put32(ddi_acc_impl_t *hdlp, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags)
{
	uint32_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR) {
		for (; repcount; repcount--) {
			eieio();
			*d++ = ddi_swap32(*h++);
		}
	} else {
		/*
		 * eieio can be outside loop here because processor guarantees
		 * order of accesses to a single address
		 */
		eieio();
		for (; repcount; repcount--)
			*d = ddi_swap32(*h++);
	}
}

/*ARGSUSED*/
void
i_ddi_vaddr_rep_put64(ddi_acc_impl_t *hdlp, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags)
{
	uint64_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR) {
		for (; repcount; repcount--) {
			eieio();
			*d++ = *h++;
		}
	} else {
		/*
		 * eieio can be outside loop here because processor guarantees
		 * order of accesses to a single address
		 */
		eieio();
		for (; repcount; repcount--)
			*d = *h++;
	}
}

/*ARGSUSED*/
void
i_ddi_vaddr_swap_rep_put64(ddi_acc_impl_t *hdlp, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags)
{
	uint64_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR) {
		for (; repcount; repcount--) {
			eieio();
			*d++ = ddi_swap64(*h++);
		}
	} else {
		/*
		 * eieio can be outside loop here because processor guarantees
		 * order of accesses to a single address
		 */
		eieio();
		for (; repcount; repcount--)
			*d = ddi_swap64(*h++);
	}
}

/*
 * Below are some common implementations.  Add appropriate ifdefs if
 * you have implemented these in assembler on your ISA, or if you need
 * less "usual" implemenations.
 */
/*ARGSUSED*/
uint8_t
i_ddi_vaddr_get8(ddi_acc_impl_t *hdlp, uint8_t *addr)
{
	return (*(volatile uint8_t *)addr);
}

/*ARGSUSED*/
uint16_t
i_ddi_vaddr_get16(ddi_acc_impl_t *hdlp, uint16_t *addr)
{
	return (*(volatile uint16_t *)addr);
}

/*ARGSUSED*/
uint32_t
i_ddi_vaddr_get32(ddi_acc_impl_t *hdlp, uint32_t *addr)
{
	return (*(volatile uint32_t *)addr);
}

/*ARGSUSED*/
uint64_t
i_ddi_vaddr_get64(ddi_acc_impl_t *hdlp, uint64_t *addr)
{
	return (*(volatile uint64_t *)addr);
}

/*ARGSUSED*/
void
i_ddi_vaddr_put8(ddi_acc_impl_t *hdlp, uint8_t *addr, uint8_t value)
{
	*(volatile uint8_t *)addr = value;
}

/*ARGSUSED*/
void
i_ddi_vaddr_put16(ddi_acc_impl_t *hdlp, uint16_t *addr, uint16_t value)
{
	*(volatile uint16_t *)addr = value;
}

/*ARGSUSED*/
void
i_ddi_vaddr_put32(ddi_acc_impl_t *hdlp, uint32_t *addr, uint32_t value)
{
	*(volatile uint32_t *)addr = value;
}

/*ARGSUSED*/
void
i_ddi_vaddr_put64(ddi_acc_impl_t *hdlp, uint64_t *addr, uint64_t value)
{
	*(volatile uint64_t *)addr = value;
}
