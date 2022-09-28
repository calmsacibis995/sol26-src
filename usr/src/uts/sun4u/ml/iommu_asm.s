/*
 * Copyright (c) 1990-1994, by Sun Microsystems, Inc.
 */

#ident	"@(#)iommu_asm.s	1.5	95/10/26 SMI"

#include <sys/spitasi.h>
#include <sys/asm_linkage.h>
#include <sys/iommu.h>
#ifndef lint
#include <assym.s>
#endif

/*
 * Function: iommu_tlb_flush
 * Args: 	%o0: struct sbus_soft_state *softsp
 *		%o1: caddr_t flush_addr
 */
#ifdef lint
/*ARGSUSED*/
void
iommu_tlb_flush(struct sbus_soft_state *softsp, u_long flush_addr)
{
	return;
}
#else
	ENTRY(iommu_tlb_flush)
	srl	%o1, 0, %o1			/* Clear upper 32 bits */
	or	%o1, %g0, %g1			/* Put flush data in global */
	ld	[%o0 + IOMMU_FLUSH_REG], %o1	/* Get the flush register */
	stx	%g1, [%o1]			/* Bang the flush reg */
	ld	[%o0 + SBUS_CTRL_REG], %o1	/* Get the sbus ctrl reg */
	retl
	ldx	[%o1], %g0			/* Flush write buffers */
	SET_SIZE(iommu_tlb_flush)
#endif
