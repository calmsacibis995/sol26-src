/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpr_mappings.c	1.8	96/09/19 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/promif.h>
#include <sys/cpr.h>
#include <sys/cpr_impl.h>
#include <sys/prom_isa.h>	/* for dnode_t */
#include <sys/prom_plat.h>	/* XXX debug only (for translation str */
#include <vm/hat_sfmmu.h>

extern void sfmmu_set_itsb(int, uint, uint);
extern void sfmmu_set_dtsb(int, uint, uint);
extern int getprocessorid();

/*
 * The following routine is called from assembly language.  Because
 * the generated code must live in the same page as the assembly language
 * caller (the cpr "jumpback" page), the makefile links this module
 * adjacent to cpr_resume_setup.o at the front of the image.  AVOID
 * PUTTING UNECESSARY CODE IN THIS SOURCE MODULE.  The .o from this
 * module and cpr_resume_setup.s must not exceed one page.  Also, DO
 * NOT REARRANGE THE ORDER OF THE ROUTINES IN THIS MODULE OR PLACE
 * A NEW ROUTINE BEFORE I_CPR_READ_MAPS().
 *
 * The code in the jumpback page is restricted to referencing other
 * addresses in its own page or in the nucleus (because its mapping is
 * locked in the tlb).  In particular, this means that we can't use the
 * normal method of specifying string constants (i.e. enclosed in quotes)
 * because they might be placed outside of this page.  Therefore, all
 * such data needed by this routine is defined in cpr_resume_setup.s
 * where we can use the assembler to position the data.
 */


/*
 * Map the buffer pre-allocated in cpr_suspend and read prom mappings.
 * Leave a dummy zeroed entry at the end of the mappings.
 */
/*ARGSUSED*/
void
i_cpr_read_maps(caddr_t map_buf, u_int map_buf_size)
{
	extern char i_cpr_trans_string[];
	extern char i_cpr_avail_string[];
	dnode_t virtmem_node, physmem_node;
	int phys_avail_l = 0, old_phys_avail_l;
	int virt_avail_l = 0, old_virt_avail_l;
	int virt_trans_l = 0, old_virt_trans_l;

	/*
	 * During the startup phase of a normal (non-cpr) boot, the
	 * kernel reads the "translations" property of the
	 * "virtual-memory" node and the "available" property of the
	 * "memory" node.  These properties can be quite large, and
	 * just getting the properties from the prom can cause it to
	 * move the upper limit of its dictionary and map in a new
	 * page to address this space.	In order for the resumed kernel
	 * to be able to make these calls (from prtconf -vp, for example)
	 * we must force the prom to allocate and map this space now.
	 * Then, when we take a snapshot of the "translations" property
	 * (below) the new mapping will be present and thus the resumed
	 * kernel will know about it.
	 *
	 * The three properties which are variable length and which can
	 * be large are the two mentioned above, plus the "available"
	 * property of the "virtual-memory" node.  It is sufficient
	 * to do a prom_getproplen() on each of them.  Bug 1226867
	 */

	virtmem_node = (dnode_t)prom_getphandle(prom_mmu_ihandle());
	physmem_node = (dnode_t)prom_getphandle(prom_memory_ihandle());

	/*
	 * Since these properties could be interdependent, keep getting
	 * them until they no longer change.
	 */
	do {
		old_phys_avail_l = phys_avail_l;
		old_virt_avail_l = virt_avail_l;
		old_virt_trans_l = virt_trans_l;

		phys_avail_l =
		    prom_getproplen(physmem_node, i_cpr_avail_string);
		virt_avail_l =
		    prom_getproplen(virtmem_node, i_cpr_avail_string);
		virt_trans_l =
		    prom_getproplen(virtmem_node, i_cpr_trans_string);
	} while (phys_avail_l != old_phys_avail_l ||
	    virt_avail_l != old_virt_avail_l ||
	    virt_trans_l != old_virt_trans_l);

	if (map_buf_size < virt_trans_l + sizeof (struct translation))
		prom_panic("i_cpr_read_maps: Buffer too small to save "
		    "prom mappings.  Please reboot.\n");

	bzero((void *) map_buf, map_buf_size);

	if (prom_getprop(virtmem_node, i_cpr_trans_string, map_buf) == -1)
		prom_panic("cpr_read_mappings: Cannot read \"translations\" "
		    "property.  Please reboot\n");
} 

/*
 * Called from assembly language to set up the data and instruction
 * tsbs.
 */
void
i_cpr_setup_tsb()
{
	int	cpuid = getprocessorid();

	sfmmu_set_itsb(tsb_bases[cpuid].tsb_pfnbase,
	    TSB_SPLIT_CODE, tsb_szcode);
	sfmmu_set_dtsb(tsb_bases[cpuid].tsb_pfnbase,
	    TSB_SPLIT_CODE, tsb_szcode);
}
