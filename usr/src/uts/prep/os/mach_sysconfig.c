/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)mach_sysconfig.c	1.1	95/08/02 SMI"

#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/sysconfig.h>
#include <sys/sysconfig_impl.h>
#include <vm/hat_ppcmmu.h>

int lwarx_size;
int coherency_size;

int
mach_sysconfig(int which)
{
	switch (which) {

	default:
		return (set_errno(EINVAL));

	case _CONFIG_COHERENCY:
		return (coherency_size);
	case _CONFIG_SPLIT_CACHE:
		return ((unified_cache == 0)?1:0);
	case _CONFIG_ICACHESZ:
		if (mmu601)
			return (set_errno(EINVAL));
		else
			return (icache_size);
	case _CONFIG_DCACHESZ:
		if (mmu601)
			return (set_errno(EINVAL));
		else
			return (dcache_size);
	case _CONFIG_ICACHELINESZ:
	case _CONFIG_ICACHEBLKSZ:
		if (mmu601)
			return (set_errno(EINVAL));
		else
			return (icache_blocksize);
	case _CONFIG_DCACHELINESZ:
	case _CONFIG_DCACHEBLKSZ:
	case _CONFIG_DCACHETBLKSZ:
		if (mmu601)
			return (set_errno(EINVAL));
		else
			return (dcache_blocksize);
	case _CONFIG_ICACHE_ASSOC:
		if (mmu601)
			return (set_errno(EINVAL));
		else
			return (icache_sets);
	case _CONFIG_DCACHE_ASSOC:
		if (mmu601)
			return (set_errno(EINVAL));
		else
			return (dcache_sets);
	case _CONFIG_PPC_GRANULE_SZ:
		return (lwarx_size);
	case _CONFIG_PPC_TB_RATEH:
		if (mmu601)
			return (set_errno(EINVAL));
		else
			return (0);
	case _CONFIG_PPC_TB_RATEL:
		if (mmu601)
			return (set_errno(EINVAL));
		else
			return (timebase_frequency);
	}
}
