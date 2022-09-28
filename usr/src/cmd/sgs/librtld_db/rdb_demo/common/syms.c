
/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided this notice is preserved and that due credit is given
 * to Sun Microsystems, Inc.  The name of Sun Microsystems, Inc. may
 * not be used to endorse or promote products derived from this
 * software without specific prior written permission.  This software
 * is provided ``as is'' without express or implied warranty.
 */
#pragma ident	"@(#)syms.c	1.3	96/09/10 SMI"

#include <stdio.h>
#include <string.h>
#include <libelf.h>
#include "rdb.h"

void
display_syms(map_info_t * mp)
{
	sym_tbl_t *	symp;
	Elf32_Sym *	syms;
	char *		strs;
	int		i;

	if (mp->mi_symtab.st_syms)
		symp = &(mp->mi_symtab);
	else if (mp->mi_dynsym.st_syms)
		symp = &(mp->mi_dynsym);
	else
		return;

	syms = symp->st_syms;
	strs = symp->st_strs;
	for (i = 0; i < symp->st_symn; i++, syms++) {
		if (syms->st_name == 0)
			continue;
		printf("[%3d]: 0x%08x: %s\n", i, syms->st_value,
			strs + syms->st_name);
	}
}





retc_t
str_map_sym(const char * symname, map_info_t * mp, Elf32_Sym * symptr)
{
	sym_tbl_t *	symp;
	Elf32_Sym *	syms;
	char *		strs;
	int		i;

	if (mp->mi_symtab.st_syms)
		symp = &(mp->mi_symtab);
	else if (mp->mi_dynsym.st_syms)
		symp = &(mp->mi_dynsym);
	else
		return (RET_FAILED);

	syms = symp->st_syms;
	strs = symp->st_strs;
	for (i = 0; i < symp->st_symn; i++, syms++) {
		if (syms->st_name == 0)
			continue;
		if ((syms->st_shndx == SHN_UNDEF) ||
		    (strcmp(strs + syms->st_name, symname) != 0))
			continue;
		*symptr = *syms;
		symptr->st_name += (unsigned int)strs;
		if ((mp->mi_flags & FLG_MI_EXEC) == 0)
			symptr->st_value += (unsigned int)(mp->mi_addr);
		return (RET_OK);
	}
	return (RET_FAILED);
}


/*
 * If two syms are of equal value this routine will
 * favor one over the other based off of it's symbol
 * type.
 */
static Elf32_Sym *
sym_swap(Elf32_Sym * s1, Elf32_Sym * s2)
{
	int	t1 = ELF32_ST_TYPE(s1->st_info);
	int	t2 = ELF32_ST_TYPE(s2->st_info);

	if ((t1 == STT_FUNC) || (t2 == STT_FUNC)) {
		if (t1 == STT_FUNC)
			return (s1);
		return (s2);
	}

	if ((t1 == STT_OBJECT) || (t2 == STT_OBJECT)) {
		if (t1 == STT_OBJECT)
			return (s1);
		return (s2);
	}

	if ((t1 == STT_OBJECT) || (t2 == STT_OBJECT)) {
		if (t1 == STT_OBJECT)
			return (s1);
		return (s2);
	}
	return (s1);
}



/*
 * Find a symbol by address from within the specfied map_info_t
 */
retc_t
addr_map_sym(map_info_t * mp, ulong_t addr, Elf32_Sym * symptr)
{
	sym_tbl_t *	symp;
	Elf32_Sym *	syms;
	Elf32_Sym *	symr = 0;
	Elf32_Sym *	lsymr = 0;
	ulong_t		baseaddr = 0;
	int		i;

	if ((mp->mi_flags & FLG_MI_EXEC) == 0)
		baseaddr = (ulong_t)mp->mi_addr;

	if (mp->mi_symtab.st_syms)
		symp = &(mp->mi_symtab);
	else if (mp->mi_dynsym.st_syms)
		symp = &(mp->mi_dynsym);
	else
		return (RET_FAILED);

	syms = symp->st_syms;

	/*
	 * normalize address
	 */
	addr -= baseaddr;
	for (i = 0; i < symp->st_symn; i++, syms++) {
		ulong_t	svalue;

		if ((syms->st_name == 0) || (syms->st_shndx == SHN_UNDEF))
			continue;

		svalue = (ulong_t)syms->st_value;

		if (svalue <= addr) {
			/*
			 * track both the best local and best
			 * global fit for this address.  Later
			 * we will favor the global over the local
			 */
			if ((ELF32_ST_BIND(syms->st_info) == STB_LOCAL) &&
			    ((lsymr == 0) ||
			    (svalue >= (ulong_t)lsymr->st_value))) {
				if (lsymr && (lsymr->st_value == svalue))
					lsymr = sym_swap(lsymr, syms);
				else
					lsymr = syms;
			} else if ((symr == 0) ||
			    (svalue >= (ulong_t)symr->st_value)) {
				if (symr && (symr->st_value == svalue))
					symr = sym_swap(symr, syms);
				else
					symr = syms;
			}
		}
	}
	if ((symr == 0) && (lsymr == 0))
		return (RET_FAILED);

	if (lsymr) {
		/*
		 * If a possible local symbol was found should
		 * we use it.
		 */
		if (symr && (lsymr->st_value > symr->st_value))
			symr = lsymr;
		else if (symr == 0)
			symr = lsymr;
	}

	*symptr = *symr;
	symptr->st_name += (unsigned int)symp->st_strs;
	symptr->st_value += (unsigned int)baseaddr;
	return (RET_OK);
}

retc_t
addr_to_sym(struct ps_prochandle * ph, ulong_t addr, Elf32_Sym * symp)
{
	map_info_t *	mip;

	if ((mip = addr_to_map(ph, addr)) == 0)
		return (RET_FAILED);

	return (addr_map_sym(mip, addr, symp));
}


retc_t
str_to_sym(struct ps_prochandle * ph, const char * name,
	Elf32_Sym * symp)
{
	map_info_t *	mip;
	if (ph->pp_lmaplist.ml_head == 0) {
		if (str_map_sym(name, &(ph->pp_ldsomap), symp) == RET_OK)
			return (RET_OK);

		return (str_map_sym(name, &(ph->pp_execmap), symp));
	}
	for (mip = ph->pp_lmaplist.ml_head; mip; mip = mip->mi_next)
		if (str_map_sym(name, mip, symp) == RET_OK)
			return (RET_OK);

	return (RET_FAILED);
}
