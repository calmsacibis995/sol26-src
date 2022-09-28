/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ddi_ppc_asm.s	1.3	96/09/24 SMI"

#if defined(lint) || defined(__lint)
#include <sys/types.h>
#include <sys/sunddi.h>
#else
#include <sys/asm_linkage.h>
#include <sys/asm_misc.h>
#include "assym.s"
#endif

#if defined(lint) || defined(__lint)

#ifdef _LP64

/*ARGSUSED*/
uint8_t
ddi_get8(ddi_acc_handle_t handle, uint8_t *addr)
{
	return (0);
}

/*ARGSUSED*/
uint8_t
ddi_mem_get8(ddi_acc_handle_t handle, uint8_t *addr)
{
	return (0);
}

/*ARGSUSED*/
uint8_t
ddi_io_get8(ddi_acc_handle_t handle, int dev_port)
{
	return (0);
}

/*ARGSUSED*/
uint16_t
ddi_get16(ddi_acc_handle_t handle, uint16_t *addr)
{
	return (0);
}

/*ARGSUSED*/
uint16_t
ddi_mem_get16(ddi_acc_handle_t handle, uint16_t *addr)
{
	return (0);
}

/*ARGSUSED*/
uint16_t
ddi_io_get16(ddi_acc_handle_t handle, int dev_port)
{
	return (0);
}


/*ARGSUSED*/
uint32_t
ddi_get32(ddi_acc_handle_t handle, uint32_t *addr)
{
	return (0);
}

/*ARGSUSED*/
uint32_t
ddi_mem_get32(ddi_acc_handle_t handle, uint32_t *addr)
{
	return (0);
}

/*ARGSUSED*/
uint32_t
ddi_io_get32(ddi_acc_handle_t handle, int dev_port)
{
	return (0);
}

/*ARGSUSED*/
uint64_t
ddi_get64(ddi_acc_handle_t handle, uint64_t *addr)
{
	return (0);
}

/*ARGSUSED*/
uint64_t
ddi_mem_get64(ddi_acc_handle_t handle, uint64_t *addr)
{
	return (0);
}

/*ARGSUSED*/
void
ddi_put8(ddi_acc_handle_t handle, uint8_t *addr, uint8_t value) {}

/*ARGSUSED*/
void
ddi_mem_put8(ddi_acc_handle_t handle, uint8_t *dev_addr, uint8_t value) {}

/*ARGSUSED*/
void
ddi_io_put8(ddi_acc_handle_t handle, int dev_port, uint8_t value) {}

/*ARGSUSED*/
void
ddi_put16(ddi_acc_handle_t handle, uint16_t *addr, uint16_t value) {}

/*ARGSUSED*/
void
ddi_mem_put16(ddi_acc_handle_t handle, uint16_t *dev_addr, uint16_t value) {}

/*ARGSUSED*/
void
ddi_io_put16(ddi_acc_handle_t handle, int dev_port, uint16_t value) {}

/*ARGSUSED*/
void
ddi_put32(ddi_acc_handle_t handle, uint32_t *addr, uint32_t value) {}

/*ARGSUSED*/
void
ddi_mem_put32(ddi_acc_handle_t handle, uint32_t *dev_addr, uint32_t value) {}

/*ARGSUSED*/
void
ddi_io_put32(ddi_acc_handle_t handle, int dev_port, uint32_t value) {}

/*ARGSUSED*/
void
ddi_put64(ddi_acc_handle_t handle, uint64_t *addr, uint64_t value) {}

/*ARGSUSED*/
void
ddi_mem_put64(ddi_acc_handle_t handle, uint64_t *dev_addr, uint64_t value) {}

/*ARGSUSED*/
void
ddi_rep_get8(ddi_acc_handle_t handle, uint8_t *host_addr, uint8_t *dev_addr,
	size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_rep_get16(ddi_acc_handle_t handle, uint16_t *host_addr, uint16_t *dev_addr,
	size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_rep_get32(ddi_acc_handle_t handle, uint32_t *host_addr, uint32_t *dev_addr,
	size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_rep_get64(ddi_acc_handle_t handle, uint64_t *host_addr, uint64_t *dev_addr,
	size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_rep_put8(ddi_acc_handle_t handle, uint8_t *host_addr, uint8_t *dev_addr,
	size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_rep_put16(ddi_acc_handle_t handle, uint16_t *host_addr, uint16_t *dev_addr,
	size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_rep_put32(ddi_acc_handle_t handle, uint32_t *host_addr, uint32_t *dev_addr,
	size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_rep_put64(ddi_acc_handle_t handle, uint64_t *host_addr, uint64_t *dev_addr,
	size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_mem_rep_get8(ddi_acc_handle_t handle, uint8_t *host_addr,
	uint8_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_mem_rep_get16(ddi_acc_handle_t handle, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_mem_rep_get32(ddi_acc_handle_t handle, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_mem_rep_get64(ddi_acc_handle_t handle, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_mem_rep_put8(ddi_acc_handle_t handle, uint8_t *host_addr,
	uint8_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_mem_rep_put16(ddi_acc_handle_t handle, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_mem_rep_put32(ddi_acc_handle_t handle, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_mem_rep_put64(ddi_acc_handle_t handle, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_io_rep_get8(ddi_acc_handle_t handle,
	uint8_t *host_addr, int dev_port, size_t repcount) {}

/*ARGSUSED*/
void
ddi_io_rep_get32(ddi_acc_handle_t handle,
	uint16_t *host_addr, int dev_port, size_t repcount) {}

/*ARGSUSED*/
void
ddi_io_rep_get32(ddi_acc_handle_t handle,
	uint32_t *host_addr, int dev_port, size_t repcount) {}

/*ARGSUSED*/
void
ddi_io_rep_put8(ddi_acc_handle_t handle,
	uint8_t *host_addr, int dev_port, size_t repcount) {}

/*ARGSUSED*/
void
ddi_io_rep_put16(ddi_acc_handle_t handle,
	uint16_t *host_addr, int dev_port, size_t repcount) {}

/*ARGSUSED*/
void
ddi_io_rep_put32(ddi_acc_handle_t handle,
	uint32_t *host_addr, int dev_port, size_t repcount) {}

#else /* _ILP32 */

/*ARGSUSED*/
uint8_t
ddi_getb(ddi_acc_handle_t handle, uint8_t *addr)
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get8)
		((ddi_acc_impl_t *)handle, addr));
}

/*ARGSUSED*/
uint8_t
ddi_mem_getb(ddi_acc_handle_t handle, uint8_t *addr)
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get8)
		((ddi_acc_impl_t *)handle, addr));
}

/*ARGSUSED*/
uint8_t
ddi_io_getb(ddi_acc_handle_t handle, int addr)
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get8)
		((ddi_acc_impl_t *)handle, (uint8_t *)addr));
}

/*ARGSUSED*/
uint16_t
ddi_getw(ddi_acc_handle_t handle, uint16_t *addr)
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get16)
		((ddi_acc_impl_t *)handle, addr));
}

/*ARGSUSED*/
uint16_t
ddi_mem_getw(ddi_acc_handle_t handle, uint16_t *addr)
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get16)
		((ddi_acc_impl_t *)handle, addr));
}

/*ARGSUSED*/
uint16_t
ddi_io_getw(ddi_acc_handle_t handle, int addr)
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get16)
		((ddi_acc_impl_t *)handle, (uint16_t *)addr));
}

/*ARGSUSED*/
uint32_t
ddi_getl(ddi_acc_handle_t handle, uint32_t *addr)
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get32)
		((ddi_acc_impl_t *)handle, addr));
}

/*ARGSUSED*/
uint32_t
ddi_mem_getl(ddi_acc_handle_t handle, uint32_t *addr)
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get32)
		((ddi_acc_impl_t *)handle, addr));
}

/*ARGSUSED*/
uint32_t
ddi_io_getl(ddi_acc_handle_t handle, int addr)
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get32)
		((ddi_acc_impl_t *)handle, (uint32_t *)addr));
}

/*ARGSUSED*/
uint64_t
ddi_getll(ddi_acc_handle_t handle, uint64_t *addr)
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get64)
		((ddi_acc_impl_t *)handle, addr));
}

/*ARGSUSED*/
uint64_t
ddi_mem_getll(ddi_acc_handle_t handle, uint64_t *addr)
{
	return ((((ddi_acc_impl_t *)handle)->ahi_get64)
		((ddi_acc_impl_t *)handle, addr));
}

/*ARGSUSED*/
void
ddi_putb(ddi_acc_handle_t handle, uint8_t *addr, uint8_t value)
{
	(((ddi_acc_impl_t *)handle)->ahi_put8)
		((ddi_acc_impl_t *)handle, addr, value);
}

/*ARGSUSED*/
void
ddi_mem_putb(ddi_acc_handle_t handle, uint8_t *addr, uint8_t value)
{
	(((ddi_acc_impl_t *)handle)->ahi_put8)
		((ddi_acc_impl_t *)handle, addr, value);
}

/*ARGSUSED*/
void
ddi_io_putb(ddi_acc_handle_t handle, int addr, uint8_t value)
{
	(((ddi_acc_impl_t *)handle)->ahi_put8)
		((ddi_acc_impl_t *)handle, (uint8_t *)addr, value);
}

/*ARGSUSED*/
void
ddi_put16(ddi_acc_handle_t handle, uint16_t *addr, uint16_t value)
{
	(((ddi_acc_impl_t *)handle)->ahi_put16)
		((ddi_acc_impl_t *)handle, addr, value);
}

/*ARGSUSED*/
void
ddi_mem_putw(ddi_acc_handle_t handle, uint16_t *addr, uint16_t value)
{
	(((ddi_acc_impl_t *)handle)->ahi_put16)
		((ddi_acc_impl_t *)handle, addr, value);
}

/*ARGSUSED*/
void
ddi_io_putw(ddi_acc_handle_t handle, int addr, uint16_t value)
{
	(((ddi_acc_impl_t *)handle)->ahi_put16)
		((ddi_acc_impl_t *)handle, (uint16_t *)addr, value);
}

/*ARGSUSED*/
void
ddi_putl(ddi_acc_handle_t handle, uint32_t *addr, uint32_t value)
{
	(((ddi_acc_impl_t *)handle)->ahi_put32)
		((ddi_acc_impl_t *)handle, addr, value);
}

/*ARGSUSED*/
void
ddi_mem_putl(ddi_acc_handle_t handle, uint32_t *addr, uint32_t value)
{
	(((ddi_acc_impl_t *)handle)->ahi_put32)
		((ddi_acc_impl_t *)handle, addr, value);
}

/*ARGSUSED*/
void
ddi_io_putl(ddi_acc_handle_t handle, int addr, uint32_t value)
{
	(((ddi_acc_impl_t *)handle)->ahi_put32)
		((ddi_acc_impl_t *)handle, (uint32_t *)addr, value);
}

/*ARGSUSED*/
void
ddi_putll(ddi_acc_handle_t handle, uint64_t *addr, uint64_t value)
{
	(((ddi_acc_impl_t *)handle)->ahi_put64)
		((ddi_acc_impl_t *)handle, addr, value);
}

/*ARGSUSED*/
void
ddi_mem_putll(ddi_acc_handle_t handle, uint64_t *addr, uint64_t value)
{
	(((ddi_acc_impl_t *)handle)->ahi_put64)
		((ddi_acc_impl_t *)handle, addr, value);
}

/*ARGSUSED*/
void
ddi_rep_getb(ddi_acc_handle_t handle, uint8_t *host_addr, uint8_t *dev_addr,
	size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_rep_getw(ddi_acc_handle_t handle, uint16_t *host_addr, uint16_t *dev_addr,
	size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_rep_getl(ddi_acc_handle_t handle, uint32_t *host_addr, uint32_t *dev_addr,
	size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_rep_getll(ddi_acc_handle_t handle, uint64_t *host_addr, uint64_t *dev_addr,
	size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_rep_putb(ddi_acc_handle_t handle, uint8_t *host_addr, uint8_t *dev_addr,
	size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_rep_putw(ddi_acc_handle_t handle, uint16_t *host_addr, uint16_t *dev_addr,
	size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_rep_putl(ddi_acc_handle_t handle, uint32_t *host_addr, uint32_t *dev_addr,
	size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_rep_putll(ddi_acc_handle_t handle, uint64_t *host_addr, uint64_t *dev_addr,
	size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_mem_rep_getb(ddi_acc_handle_t handle, uint8_t *host_addr,
	uint8_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_mem_rep_getw(ddi_acc_handle_t handle, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_mem_rep_getl(ddi_acc_handle_t handle, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_mem_rep_getll(ddi_acc_handle_t handle, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_mem_rep_putb(ddi_acc_handle_t handle, uint8_t *host_addr,
	uint8_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_mem_rep_putw(ddi_acc_handle_t handle, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_mem_rep_putl(ddi_acc_handle_t handle, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags)
{
}

/*ARGSUSED*/
void
ddi_mem_rep_putll(ddi_acc_handle_t handle, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags)
{
}

#endif /* _LP64 */

#else	/* lint */

	ENTRY(ddi_getb)
	ALTENTRY(ddi_get8)
	ALTENTRY(ddi_mem_getb)
	ALTENTRY(ddi_mem_get8)
	ALTENTRY(ddi_io_getb)
	ALTENTRY(ddi_io_get8)
	blr
	SET_SIZE(ddi_getb)

	ENTRY(ddi_getw)
	ALTENTRY(ddi_get16)
	ALTENTRY(ddi_mem_getw)
	ALTENTRY(ddi_mem_get16)
	ALTENTRY(ddi_io_getw)
	ALTENTRY(ddi_io_get16)
	blr
	SET_SIZE(ddi_getw)

	ENTRY(ddi_getl)
	ALTENTRY(ddi_get32)
	ALTENTRY(ddi_mem_getl)
	ALTENTRY(ddi_mem_get32)
	ALTENTRY(ddi_io_getl)
	ALTENTRY(ddi_io_get32)
	blr
	SET_SIZE(ddi_getl)

	ENTRY(ddi_getll)
	ALTENTRY(ddi_get64)
	ALTENTRY(ddi_mem_getll)
	ALTENTRY(ddi_mem_get64)
	blr
	SET_SIZE(ddi_getll)

	ENTRY(ddi_putb)
	ALTENTRY(ddi_put8)
	ALTENTRY(ddi_mem_putb)
	ALTENTRY(ddi_mem_put8)
	ALTENTRY(ddi_io_putb)
	ALTENTRY(ddi_io_put8)
	blr
	SET_SIZE(ddi_putb)

	ENTRY(ddi_putw)
	ALTENTRY(ddi_put16)
	ALTENTRY(ddi_mem_putw)
	ALTENTRY(ddi_mem_put16)
	ALTENTRY(ddi_io_putw)
	ALTENTRY(ddi_io_put16)
	blr
	SET_SIZE(ddi_putw)

	ENTRY(ddi_putl)
	ALTENTRY(ddi_put32)
	ALTENTRY(ddi_mem_putl)
	ALTENTRY(ddi_mem_put32)
	ALTENTRY(ddi_io_putl)
	ALTENTRY(ddi_io_put32)
	blr
	SET_SIZE(ddi_putl)

	ENTRY(ddi_putll)
	ALTENTRY(ddi_put64)
	ALTENTRY(ddi_mem_putll)
	ALTENTRY(ddi_mem_put64)
	blr
	SET_SIZE(ddi_putll)

	ENTRY(ddi_rep_getb)
	ALTENTRY(ddi_rep_get8)
	ALTENTRY(ddi_mem_rep_getb)
	ALTENTRY(ddi_mem_rep_get8)
	blr
	SET_SIZE(ddi_rep_getb)

	ENTRY(ddi_rep_getw)
	ALTENTRY(ddi_rep_get16)
	ALTENTRY(ddi_mem_rep_getw)
	ALTENTRY(ddi_mem_rep_get16)
	blr
	SET_SIZE(ddi_rep_getw)

	ENTRY(ddi_rep_getl)
	ALTENTRY(ddi_rep_get32)
	ALTENTRY(ddi_mem_rep_getl)
	ALTENTRY(ddi_mem_rep_get32)
	blr
	SET_SIZE(ddi_rep_getl)

	ENTRY(ddi_rep_getll)
	ALTENTRY(ddi_rep_get64)
	ALTENTRY(ddi_mem_rep_getll)
	ALTENTRY(ddi_mem_rep_get64)
	blr
	SET_SIZE(ddi_rep_getll)

	ENTRY(ddi_rep_putb)
	ALTENTRY(ddi_rep_put8)
	ALTENTRY(ddi_mem_rep_putb)
	ALTENTRY(ddi_mem_rep_put8)
	blr
	SET_SIZE(ddi_rep_putb)

	ENTRY(ddi_rep_putw)
	ALTENTRY(ddi_rep_put16)
	ALTENTRY(ddi_mem_rep_putw)
	ALTENTRY(ddi_mem_rep_put16)
	blr
	SET_SIZE(ddi_rep_putw)

	ENTRY(ddi_rep_putl)
	ALTENTRY(ddi_rep_put32)
	ALTENTRY(ddi_mem_rep_putl)
	ALTENTRY(ddi_mem_rep_put32)
	blr
	SET_SIZE(ddi_rep_putl)

	ENTRY(ddi_rep_putll)
	ALTENTRY(ddi_rep_put64)
	ALTENTRY(ddi_mem_rep_putll)
	ALTENTRY(ddi_mem_rep_put64)
	blr
	SET_SIZE(ddi_rep_putll)

#endif /* lint */

#if defined(lint) || defined(__lint)

/*ARGSUSED*/
uint8_t
i_ddi_vaddr_get8(ddi_acc_impl_t *hdlp, uint8_t *addr)
{
	return (*addr);
}

/*ARGSUSED*/
uint16_t
i_ddi_vaddr_get16(ddi_acc_impl_t *hdlp, uint16_t *addr)
{
	return (*addr);
}

/*ARGSUSED*/
uint32_t
i_ddi_vaddr_get32(ddi_acc_impl_t *hdlp, uint32_t *addr)
{
	return (*addr);
}

/*ARGSUSED*/
uint64_t
i_ddi_vaddr_get64(ddi_acc_impl_t *hdlp, uint64_t *addr)
{
	return (*addr);
}

#else	/* lint */

	ENTRY(i_ddi_vaddr_get8)
	blr
	SET_SIZE(i_ddi_vaddr_get8)

	ENTRY(i_ddi_vaddr_get16)
	blr
	SET_SIZE(i_ddi_vaddr_get16)

	ENTRY(i_ddi_vaddr_get32)
	blr
	SET_SIZE(i_ddi_vaddr_get32)

	ENTRY(i_ddi_vaddr_get64)
	blr
	SET_SIZE(i_ddi_vaddr_get64)

#endif /* lint */


#if defined(lint) || defined(__lint)

/*ARGSUSED*/
uint8_t
i_ddi_io_get8(ddi_acc_impl_t *hdlp, uint8_t *addr)
{
	return (inb((u_int)addr));
}

/*ARGSUSED*/
uint16_t
i_ddi_io_get16(ddi_acc_impl_t *hdlp, uint16_t *addr)
{
	return (inw((u_int)addr));
}

/*ARGSUSED*/
uint32_t
i_ddi_io_get32(ddi_acc_impl_t *hdlp, uint32_t *addr)
{
	return (inl((int)addr));
}

#else	/* lint */

	ENTRY(i_ddi_vaddr_put8)
	blr
	SET_SIZE(i_ddi_vaddr_put8)

	ENTRY(i_ddi_vaddr_put16)
	blr
	SET_SIZE(i_ddi_vaddr_put16)

	ENTRY(i_ddi_vaddr_put32)
	blr
	SET_SIZE(i_ddi_vaddr_put32)

	ENTRY(i_ddi_vaddr_put64)
	blr
	SET_SIZE(i_ddi_vaddr_put64)

#endif /* lint */
