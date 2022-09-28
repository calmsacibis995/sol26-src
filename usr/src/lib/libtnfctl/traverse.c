/*
 * Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#pragma ident	"@(#)traverse.c	1.7	96/07/22 SMI"

/*
 * Generic functions that know how to traverse elf sections in an object.
 * Also functions that know how to traverse records in a section.
 *
 */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/procfs.h>
#include <sys/stat.h>

#include "tnfctl_int.h"
#include "dbg.h"


/*
 * _tnfctl_traverse_object() - traverses all of the elf sections in an object,
 * calling the supplied function on each.
 */
tnfctl_errcode_t
_tnfctl_traverse_object(int objfd, paddr_t addr,
			tnfctl_elf_search_t *search_info_p)
{
	Elf		*elf;
	Elf32_Ehdr	*ehdr;
	char		*strs;
	Elf32_Shdr	*shdr;
	Elf_Data	*data;
	u_int		idx;
	tnfctl_errcode_t	prexstat = TNFCTL_ERR_NONE;

	DBG_TNF_PROBE_1(_tnfctl_traverse_object_1, "libtnfctl",
			"sunw%verbosity 3",
			tnf_opaque, obj_addr, addr);

	if (elf_version(EV_CURRENT) == EV_NONE)
		return (TNFCTL_ERR_INTERNAL);

	/* open elf descriptor on the fd */
	elf = elf_begin(objfd, ELF_C_READ, NULL);
	if (elf == NULL || elf_kind(elf) != ELF_K_ELF) {
		DBG_TNF_PROBE_0(_tnfctl_traverse_object_2, "libtnfctl",
			"sunw%verbosity 3; sunw%debug 'not elf object'");
		return (TNFCTL_ERR_INTERNAL);
	}
	/* get the elf header */
	if ((ehdr = elf32_getehdr(elf)) == NULL) {
		DBG((void) fprintf(stderr,
			"_tnfctl_traverse_object: elf32_getehdr failed\n"));
		(void) elf_end(elf);
		return (TNFCTL_ERR_INTERNAL);
	}
	if ((ehdr->e_type != ET_EXEC) && (ehdr->e_type != ET_DYN)) {
		DBG((void) fprintf(stderr,
			"_tnfctl_traverse_object: not an "
			"executable or a shared object\n"));
		(void) elf_end(elf);
		return (TNFCTL_ERR_INTERNAL);
	}
	/* if an executable file, the base address is 0 */
	if (ehdr->e_type == ET_EXEC)
		addr = 0;
	/* get a pointer to the elf header string table */
	strs = elf_strptr(elf, ehdr->e_shstrndx, NULL);

	DBG_TNF_PROBE_1(_tnfctl_traverse_object_3, "libtnfctl",
			"sunw%verbosity 3",
			tnf_long, num_sections_found, ehdr->e_shnum);

	for (idx = 1; idx < ehdr->e_shnum; idx++) {
		Elf_Scn		*scn;

		if ((scn = elf_getscn(elf, idx)) == NULL) {
			DBG((void) fprintf(stderr,
			    "_tnfctl_traverse_object: elf_getscn failed\n"));
			prexstat = TNFCTL_ERR_INTERNAL;
			break;
		}
		if ((shdr = elf32_getshdr(scn)) == NULL) {
			DBG((void) fprintf(stderr,
				"_tnfctl_traverse_obj:elf32_getshdr failed\n"));
			prexstat = TNFCTL_ERR_INTERNAL;
			break;
		}

		if ((data = elf_getdata(scn, NULL)) == NULL) {
			DBG((void) fprintf(stderr,
				"_tnfctl_traverse_obj:elf32_getdata failed\n"));
			prexstat = TNFCTL_ERR_INTERNAL;
			break;
		}
		/* call the supplied function */
		prexstat = search_info_p->section_func(elf,
			strs, scn, shdr, data, addr, search_info_p);
		if (prexstat)
			break;
	}

	(void) elf_end(elf);

	return (prexstat);

}				/* end _tnfctl_traverse_object */


/*
 * _tnfctl_traverse_rela() - this function traverses a .rela section calling the
 * supplied function on each relocation record.
 */
/*ARGSUSED*/
tnfctl_errcode_t
_tnfctl_traverse_rela(Elf * elf, char *strs, Elf_Scn * rel_scn,
	Elf32_Shdr * rel_shdr, Elf_Data * rel_data, paddr_t baseaddr,
	tnfctl_elf_search_t * search_info_p)
{
	Elf_Scn		*sym_scn;
	Elf32_Shdr	*sym_shdr;
	Elf_Data	*sym_data;
	Elf32_Sym	*sym_table;
	Elf_Scn		*str_scn;
	Elf32_Shdr	*str_shdr;
	Elf_Data	*str_data;
	char		*str_table;
	unsigned	nrels, i;
	boolean_t	isrela;
	size_t		rela_sz;
	char		*ptr;

	DBG_TNF_PROBE_0(_tnfctl_traverse_rela_1, "libtnfctl",
				"sunw%verbosity 4");

	/* bail if this isn't a rela (or rel) section */
	if (rel_shdr->sh_type == SHT_RELA) {
		isrela = B_TRUE;
	} else if (rel_shdr->sh_type == SHT_REL) {
		isrela = B_FALSE;
	} else
		return (TNFCTL_ERR_NONE);

	/* find the symbol table section associated with this rela section */
	sym_scn = elf_getscn(elf, rel_shdr->sh_link);
	if (sym_scn == NULL) {
		DBG((void) fprintf(stderr,
			"_tnfctl_traverse_rela:elf_getscn (sym) failed\n"));
		return (TNFCTL_ERR_INTERNAL);
	}
	sym_shdr = elf32_getshdr(sym_scn);
	if (sym_shdr == NULL) {
		DBG((void) fprintf(stderr,
			"_tnfctl_traverse_rela:elf32_getshdr (sym) failed\n"));
		return (TNFCTL_ERR_INTERNAL);
	}
	sym_data = elf_getdata(sym_scn, NULL);
	if (sym_data == NULL) {
		DBG((void) fprintf(stderr,
			"_tnfctl_traverse_rela:elf_getdata (sym) failed\n"));
		return (TNFCTL_ERR_INTERNAL);
	}
	sym_table = (Elf32_Sym *) sym_data->d_buf;

	/* find the string table associated with the symbol table */
	str_scn = elf_getscn(elf, sym_shdr->sh_link);
	if (str_scn == NULL) {
		DBG((void) fprintf(stderr,
			"_tnfctl_traverse_rela:elf_getscn (str) failed\n"));
		return (TNFCTL_ERR_INTERNAL);
	}
	str_shdr = elf32_getshdr(str_scn);
	if (str_shdr == NULL) {
		DBG((void) fprintf(stderr,
			"_tnfctl_traverse_rela:elf32_getshdr (str) failed\n"));
		return (TNFCTL_ERR_INTERNAL);
	}
	str_data = elf_getdata(str_scn, NULL);
	if (str_data == NULL) {
		DBG((void) fprintf(stderr,
			"_tnfctl_traverse_rela: elf_getdata (str) failed\n"));
		return (TNFCTL_ERR_INTERNAL);
	}
	str_table = (char *) str_data->d_buf;

	/* loop over each relocation record */
	nrels = rel_shdr->sh_size / rel_shdr->sh_entsize;

	DBG_TNF_PROBE_1(_tnfctl_traverse_rela_2, "libtnfctl",
			"sunw%verbosity 3",
			tnf_long, relocations_found, nrels);

	ptr = rel_data->d_buf;
	rela_sz = (isrela) ? sizeof (Elf32_Rela) : sizeof (Elf32_Rel);
	for (i = 0; i < nrels; i++, ptr += rela_sz) {
		Elf32_Word	syminfo;
		Elf32_Sym	*sym;
		Elf32_Addr	offset;
		char		*name;
		paddr_t		addr;
		tnfctl_errcode_t	prexstat;

		/* decode the r_info field of the relocation record */
		if (isrela) {
			Elf32_Rela	 *rela_p;

			/*LINTED pointer cast may result in improper alignment*/
			rela_p = (Elf32_Rela *) ptr;
			syminfo = ELF32_R_SYM(rela_p->r_info);
			offset = rela_p->r_offset;
		} else {
			Elf32_Rel	  *rel_p;

			/*LINTED pointer cast may result in improper alignment*/
			rel_p = (Elf32_Rel *) ptr;
			syminfo = ELF32_R_SYM(rel_p->r_info);
			offset = rel_p->r_offset;
		}

		/* find the associated symbol table entry */
		if (!syminfo)
			continue;
		sym = sym_table + syminfo;

		/* find the associated string table entry */
		if (!sym->st_name)
			continue;
		name = str_table + sym->st_name;
		addr = offset + baseaddr;

		prexstat = search_info_p->record_func(name, addr, ptr,
							search_info_p);
		if (prexstat)
			break;
	}

	return (TNFCTL_ERR_NONE);

}				/* end _tnfctl_traverse_rela */


/*
 * _tnfctl_traverse_dynsym() - this function traverses a dynsym section calling
 * the supplied function on each symbol.
 */

/*ARGSUSED*/
tnfctl_errcode_t
_tnfctl_traverse_dynsym(Elf * elf,
			char *elfstrs,
			Elf_Scn * scn,
			Elf32_Shdr * shdr,
			Elf_Data * data,
			paddr_t baseaddr,
			tnfctl_elf_search_t * search_info_p)
{
	int		nsyms, i;
	char		*strs;
	tnfctl_errcode_t	prexstat;
	Elf32_Sym	*syms;

	/* bail if this isn't a dynsym section */
	if (shdr->sh_type != SHT_DYNSYM)
		return (TNFCTL_ERR_NONE);

	syms = data->d_buf;
	nsyms = shdr->sh_size / shdr->sh_entsize;
	strs = elf_strptr(elf, shdr->sh_link, 0);

	DBG_TNF_PROBE_1(_tnfctl_traverse_dynsym_1, "libtnfctl",
			"sunw%verbosity 3",
			tnf_long, symbols_found, nsyms);

	for (i = 0; i < nsyms; i++) {
		Elf32_Sym	  *sym = &syms[i];
		char		   *name;
		paddr_t		 addr;

		name = strs + sym->st_name;
		addr = baseaddr + sym->st_value;

		prexstat = search_info_p->record_func(name,
			addr, sym, search_info_p);
		if (prexstat)
			break;
	}

	return (prexstat);

}				/* end _tnfctl_traverse_dynsym */
