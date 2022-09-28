/*
 * Copyright (c) 1990-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)ddi_v9_asm.s 1.7     96/09/24 SMI"

#include <sys/asi.h>
#include <sys/asm_linkage.h>
#ifndef lint
#include <assym.s>
#endif

#if defined(lint)
#include <sys/types.h>
#include <sys/sunddi.h>
#endif  /* lint */

/*
 * This file implements the following ddi common access 
 * functions:
 *
 *	ddi_get{b,h,l,ll}
 *	ddi_put{b,h,l.ll}
 *
 * Assumptions:
 *
 *	There is no need to check the access handle.  We assume
 *	byte swapping will be done by the mmu and the address is
 *	always accessible via ld/st instructions.
 */

#if defined(lint)

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
	return (0);
}

/*ARGSUSED*/
uint8_t
ddi_mem_getb(ddi_acc_handle_t handle, uint8_t *host_addr)
{
	return (0);
}

/*ARGSUSED*/
uint8_t
ddi_io_getb(ddi_acc_handle_t handle, int dev_port)
{
	return (0);
}

/*ARGSUSED*/
uint16_t
ddi_getw(ddi_acc_handle_t handle, uint16_t *addr)
{
	return (0);
}

/*ARGSUSED*/
uint16_t
ddi_mem_getw(ddi_acc_handle_t handle, uint16_t *host_addr)
{
	return (0);
}

/*ARGSUSED*/
uint16_t
ddi_io_getw(ddi_acc_handle_t handle, int dev_port)
{
	return (0);
}

/*ARGSUSED*/
uint32_t
ddi_getl(ddi_acc_handle_t handle, uint32_t *addr)
{
	return (0);
}

/*ARGSUSED*/
uint32_t
ddi_mem_getl(ddi_acc_handle_t handle, uint32_t *host_addr)
{
	return (0);
}

/*ARGSUSED*/
uint32_t
ddi_io_getl(ddi_acc_handle_t handle, int dev_port)
{
	return (0);
}

/*ARGSUSED*/
uint64_t
ddi_getll(ddi_acc_handle_t handle, uint64_t *addr)
{
	return (0);
}

/*ARGSUSED*/
uint64_t
ddi_mem_getll(ddi_acc_handle_t handle, uint64_t *host_addr)
{
	return (0);
}

/*ARGSUSED*/
void
ddi_putb(ddi_acc_handle_t handle, uint8_t *addr, uint8_t value) {}

/*ARGSUSED*/
void
ddi_mem_putb(ddi_acc_handle_t handle, uint8_t *dev_addr, uint8_t value) {}

/*ARGSUSED*/
void
ddi_io_putb(ddi_acc_handle_t handle, int dev_port, uint8_t value) {}

/*ARGSUSED*/
void
ddi_putw(ddi_acc_handle_t handle, uint16_t *addr, uint16_t value) {}

/*ARGSUSED*/
void
ddi_mem_putw(ddi_acc_handle_t handle, uint16_t *dev_addr, uint16_t value) {}

/*ARGSUSED*/
void
ddi_io_putw(ddi_acc_handle_t handle, int dev_port, uint16_t value) {}

/*ARGSUSED*/
void
ddi_putl(ddi_acc_handle_t handle, uint32_t *addr, uint32_t value) {}

/*ARGSUSED*/
void
ddi_mem_putl(ddi_acc_handle_t handle, uint32_t *dev_addr, uint32_t value) {}

/*ARGSUSED*/
void
ddi_io_putl(ddi_acc_handle_t handle, int dev_port, uint32_t value) {}

/*ARGSUSED*/
void
ddi_putll(ddi_acc_handle_t handle, uint64_t *addr, uint64_t value) {}

/*ARGSUSED*/
void
ddi_mem_putll(ddi_acc_handle_t handle, uint64_t *dev_addr, uint64_t value) {}

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

/*ARGSUSED*/
void
ddi_io_rep_getb(ddi_acc_handle_t handle,
	uint8_t *host_addr, int dev_port, size_t repcount) {}

/*ARGSUSED*/
void
ddi_io_rep_getw(ddi_acc_handle_t handle,
	uint16_t *host_addr, int dev_port, size_t repcount) {}

/*ARGSUSED*/
void
ddi_io_rep_getl(ddi_acc_handle_t handle,
	uint32_t *host_addr, int dev_port, size_t repcount) {}

/*ARGSUSED*/
void
ddi_io_rep_putb(ddi_acc_handle_t handle,
	uint8_t *host_addr, int dev_port, size_t repcount) {}

/*ARGSUSED*/
void
ddi_io_rep_putw(ddi_acc_handle_t handle,
	uint16_t *host_addr, int dev_port, size_t repcount) {}

/*ARGSUSED*/
void
ddi_io_rep_putl(ddi_acc_handle_t handle,
	uint32_t *host_addr, int dev_port, size_t repcount) {}

#endif /* _LP64 */

#else
	ENTRY(ddi_getb)
	ALTENTRY(ddi_get8)
	ALTENTRY(ddi_mem_getb)
	ALTENTRY(ddi_mem_get8)
	ALTENTRY(ddi_io_getb)
	ALTENTRY(ddi_io_get8)
	retl
	ldub	[%o1], %o0
	SET_SIZE(ddi_getb)

	ENTRY(ddi_getw)
	ALTENTRY(ddi_get16)
	ALTENTRY(ddi_mem_getw)
	ALTENTRY(ddi_mem_get16)
	ALTENTRY(ddi_io_getw)
	ALTENTRY(ddi_io_get16)
	retl
	lduh	[%o1], %o0
	SET_SIZE(ddi_getw)

	ENTRY(ddi_getl)
	ALTENTRY(ddi_get32)
	ALTENTRY(ddi_mem_getl)
	ALTENTRY(ddi_mem_get32)
	ALTENTRY(ddi_io_getl)
	ALTENTRY(ddi_io_get32)
	retl
	ld	[%o1], %o0
	SET_SIZE(ddi_getl)

	ENTRY(ddi_getll)
	ALTENTRY(ddi_get64)
	ALTENTRY(ddi_mem_getll)
	ALTENTRY(ddi_mem_get64)
	ALTENTRY(ddi_io_getll)
	ALTENTRY(ddi_io_get64)
	retl
	ldd	[%o1], %o0
	SET_SIZE(ddi_getll)

	ENTRY(ddi_putb)
	ALTENTRY(ddi_put8)
	ALTENTRY(ddi_mem_putb)
	ALTENTRY(ddi_mem_put8)
	ALTENTRY(ddi_io_putb)
	ALTENTRY(ddi_io_put8)
	retl
	stub	%o2, [%o1]
	SET_SIZE(ddi_putb)

	ENTRY(ddi_putw)
	ALTENTRY(ddi_put16)
	ALTENTRY(ddi_mem_putw)
	ALTENTRY(ddi_mem_put16)
	ALTENTRY(ddi_io_putw)
	ALTENTRY(ddi_io_put16)
	retl
	stuh	%o2, [%o1]
	SET_SIZE(ddi_putw)

	ENTRY(ddi_putl)
	ALTENTRY(ddi_put32)
	ALTENTRY(ddi_mem_putl)
	ALTENTRY(ddi_mem_put32)
	ALTENTRY(ddi_io_putl)
	ALTENTRY(ddi_io_put32)
	retl
	st	%o2, [%o1]
	SET_SIZE(ddi_putl)

	ENTRY(ddi_putll)
	ALTENTRY(ddi_put64)
	ALTENTRY(ddi_mem_putll)
	ALTENTRY(ddi_mem_put64)
	ALTENTRY(ddi_io_putll)
	ALTENTRY(ddi_io_put64)
	retl
	std	%o2, [%o1]
	SET_SIZE(ddi_putll)

#define DDI_REP_GET(n,s)		\
	mov %o1, %g1;			\
	mov %o2, %g2;			\
	cmp %o4, 1;			\
	be 2f;				\
	mov %o3, %g3;			\
1:	tst	%g3;			\
	be	3f;			\
	nop;				\
	ld/**/s	[%g2], %g4;		\
	st/**/s	%g4, [%g1];		\
	add	%g1, n, %g1;		\
	ba	1b;			\
	dec	%g3;			\
2:	tst	%g3;			\
	be	3f;			\
	nop;				\
	ld/**/s	[%g2], %g4;		\
	st/**/s	%g4, [%g1];		\
	add	%g1, n, %g1;		\
	add	%g2, n, %g2;		\
	ba	2b;			\
	dec	%g3;			\
3:	retl;				\
	nop

	ENTRY(ddi_rep_getb)
	ALTENTRY(ddi_rep_get8)
	ALTENTRY(ddi_mem_rep_getb)
	ALTENTRY(ddi_mem_rep_get8)
	DDI_REP_GET(1,ub)
	SET_SIZE(ddi_rep_getb)

	ENTRY(ddi_rep_getw)
	ALTENTRY(ddi_rep_get16)
	ALTENTRY(ddi_mem_rep_getw)
	ALTENTRY(ddi_mem_rep_get16)
	DDI_REP_GET(2,uh)
	SET_SIZE(ddi_rep_getw)

	ENTRY(ddi_rep_getl)
	ALTENTRY(ddi_rep_get32)
	ALTENTRY(ddi_mem_rep_getl)
	ALTENTRY(ddi_mem_rep_get32)
	DDI_REP_GET(4,/**/)
	SET_SIZE(ddi_rep_getl)

	ENTRY(ddi_rep_getll)
	ALTENTRY(ddi_rep_get64)
	ALTENTRY(ddi_mem_rep_getll)
	ALTENTRY(ddi_mem_rep_get64)
	DDI_REP_GET(8,x)
	SET_SIZE(ddi_rep_getll)

#define DDI_REP_PUT(n,s)		\
	mov %o1, %g1;			\
	mov %o2, %g2;			\
	cmp %o4, 1;			\
	be 2f;				\
	mov %o3, %g3;			\
1:	tst	%g3;			\
	be	3f;			\
	nop;				\
	ld/**/s	[%g1], %g4;		\
	st/**/s	%g4, [%g2];		\
	add	%g1, n, %g1;		\
	ba	1b;			\
	dec	%g3;			\
2:	tst	%g3;			\
	be	3f;			\
	nop;				\
	ld/**/s	[%g1], %g4;		\
	st/**/s	%g4, [%g2];		\
	add	%g1, n, %g1;		\
	add	%g2, n, %g2;		\
	ba	2b;			\
	dec	%g3;			\
3:	retl;				\
	nop

	ENTRY(ddi_rep_putb)
	ALTENTRY(ddi_rep_put8)
	ALTENTRY(ddi_mem_rep_putb)
	ALTENTRY(ddi_mem_rep_put8)
	DDI_REP_PUT(1,ub)
	SET_SIZE(ddi_rep_putb)

	ENTRY(ddi_rep_putw)
	ALTENTRY(ddi_rep_put16)
	ALTENTRY(ddi_mem_rep_putw)
	ALTENTRY(ddi_mem_rep_put16)
	DDI_REP_PUT(2,uh)
	SET_SIZE(ddi_rep_putw)

	ENTRY(ddi_rep_putl)
	ALTENTRY(ddi_rep_put32)
	ALTENTRY(ddi_mem_rep_putl)
	ALTENTRY(ddi_mem_rep_put32)
	DDI_REP_PUT(4,/**/)
	SET_SIZE(ddi_rep_putl)

	ENTRY(ddi_rep_putll)
	ALTENTRY(ddi_rep_put64)
	ALTENTRY(ddi_mem_rep_putll)
	ALTENTRY(ddi_mem_rep_put64)
	DDI_REP_PUT(8,x)
	SET_SIZE(ddi_rep_putll)

#define DDI_IO_REP_GET(n,s)		\
	mov %o1, %g1;			\
	mov %o2, %g2;			\
	mov %o3, %g3;			\
1:	tst	%g3;			\
	be	2f;			\
	nop;				\
	ld/**/s	[%g2], %g4;		\
	st/**/s	%g4, [%g1];		\
	add	%g1, n, %g1;		\
	ba	1b;			\
	dec	%g3;			\
2:	retl;				\
	nop

	ENTRY(ddi_io_rep_getb)
	ALTENTRY(ddi_io_rep_get8)
	DDI_IO_REP_GET(1,ub)
	SET_SIZE(ddi_io_rep_getb)

	ENTRY(ddi_io_rep_getw)
	ALTENTRY(ddi_io_rep_get16)
	DDI_IO_REP_GET(2,uh)
	SET_SIZE(ddi_io_rep_getw)

	ENTRY(ddi_io_rep_getl)
	ALTENTRY(ddi_io_rep_get32)
	DDI_IO_REP_GET(4,/**/)
	SET_SIZE(ddi_io_rep_getl)

#define DDI_IO_REP_PUT(n,s)		\
	mov %o1, %g1;			\
	mov %o2, %g2;			\
	mov %o3, %g3;			\
1:	tst	%g3;			\
	be	2f;			\
	nop;				\
	ld/**/s	[%g1], %g4;		\
	st/**/s	%g4, [%g2];		\
	add	%g1, n, %g1;		\
	ba	1b;			\
	dec	%g3;			\
2:	retl;				\
	nop

	ENTRY(ddi_io_rep_putb)
	ALTENTRY(ddi_io_rep_put8)
	DDI_IO_REP_PUT(1,ub)
	SET_SIZE(ddi_io_rep_putb)

	ENTRY(ddi_io_rep_putw)
	ALTENTRY(ddi_io_rep_put16)
	DDI_IO_REP_PUT(2,uh)
	SET_SIZE(ddi_io_rep_putw)

	ENTRY(ddi_io_rep_putl)
	ALTENTRY(ddi_io_rep_put32)
	DDI_IO_REP_PUT(4,/**/)
	SET_SIZE(ddi_io_rep_putl)
#endif
