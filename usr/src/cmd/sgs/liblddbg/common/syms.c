/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)syms.c	1.17	96/02/27 SMI"

/* LINTLIBRARY */

#include	"msg.h"
#include	"_debug.h"
#include	"libld.h"

/*
 * Print out a single `symbol table node' entry.
 */
void
Elf_sym_table_title(const char * index, const char * name)
{
	dbg_print(MSG_INTL(MSG_SYM_TITLE), index, name);
}

void
Elf_sym_table_entry(const char * prestr, Sym * sym, int verndx,
	const char * sec, const char * poststr)
{
	dbg_print(MSG_INTL(MSG_SYM_ENTRY), prestr, sym->st_value, sym->st_size,
		conv_info_type_str(ELF_ST_TYPE(sym->st_info)),
		conv_info_bind_str(ELF_ST_BIND(sym->st_info)),
		verndx, sec ? sec : conv_shndx_str(sym->st_shndx),
		poststr);
}

void
Dbg_syms_ar_title(const char * file, int found)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_SYM_AR_FILE), file,
	    found ? MSG_INTL(MSG_STR_AGAIN) : MSG_ORIG(MSG_STR_EMPTY));
}

void
Dbg_syms_ar_entry(int ndx, Elf_Arsym * arsym)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;

	dbg_print(MSG_INTL(MSG_SYM_AR_ENTRY), ndx, arsym->as_name);
}

void
Dbg_syms_ar_checking(int ndx, Elf_Arsym * arsym, const char * name)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;

	dbg_print(MSG_INTL(MSG_SYM_AR_CHECK), ndx, arsym->as_name, name);
}

void
Dbg_syms_ar_resolve(int ndx, Elf_Arsym * arsym, const char * name)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;

	dbg_print(MSG_INTL(MSG_SYM_AR_RESOLVE), ndx, arsym->as_name, name);
}

void
Dbg_syms_spec_title()
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_SYM_SPECIAL));
}


void
Dbg_syms_entered(Sym * sym, Sym_desc * sdp)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;
	if (DBG_NOTDETAIL())
		return;

	Elf_sym_table_entry(MSG_INTL(MSG_STR_ENTERED), sym,
	    sdp->sd_aux ? sdp->sd_aux->sa_verndx : 0, NULL,
	    conv_deftag_str(sdp->sd_ref));
}

void
Dbg_syms_process(Ifl_desc * ifl)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_SYM_PROCESS), ifl->ifl_name,
	    conv_etype_str(ifl->ifl_ehdr->e_type));
}

void
Dbg_syms_entry(int ndx, Sym_desc * sdp)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;

	dbg_print(MSG_INTL(MSG_SYM_BASIC), ndx, sdp->sd_name);
}

void
Dbg_syms_global(int ndx, const char * name)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;

	dbg_print(MSG_INTL(MSG_SYM_ADDING), ndx, name);
}

void
Dbg_syms_sec_title()
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;
	if (DBG_NOTDETAIL())
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_SYM_INDEX));
}

void
Dbg_syms_sec_entry(int ndx, Sg_desc * sgp, Os_desc * osp)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;
	if (DBG_NOTDETAIL())
		return;

	dbg_print(MSG_INTL(MSG_SYM_SECTION), ndx, osp->os_name,
		(*sgp->sg_name ? sgp->sg_name : MSG_INTL(MSG_STR_NULL)));
}

void
Dbg_syms_up_title()
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;
	if (DBG_NOTDETAIL())
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_SYM_FINAL));
	Elf_sym_table_title(MSG_ORIG(MSG_STR_EMPTY), MSG_ORIG(MSG_STR_EMPTY));
}

void
Dbg_syms_old(Sym_desc *	sdp)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;
	if (DBG_NOTDETAIL())
		return;

	Elf_sym_table_entry(MSG_INTL(MSG_STR_OLD), sdp->sd_sym,
	    sdp->sd_aux ? sdp->sd_aux->sa_verndx : 0, NULL, sdp->sd_name);
}

void
Dbg_syms_new(Sym * sym, Sym_desc * sdp)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;
	if (DBG_NOTDETAIL())
		return;

	Elf_sym_table_entry(MSG_INTL(MSG_STR_NEW), sym,
	    sdp->sd_aux ? sdp->sd_aux->sa_verndx : 0, NULL,
	    conv_deftag_str(sdp->sd_ref));
}

void
Dbg_syms_updated(Sym_desc * sdp, const char * name)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;

	dbg_print(MSG_INTL(MSG_SYM_UPDATE), name);

	if (DBG_NOTDETAIL())
		return;

	Elf_sym_table_entry(MSG_ORIG(MSG_STR_EMPTY), sdp->sd_sym,
	    sdp->sd_aux ? sdp->sd_aux->sa_verndx : 0, NULL,
	    conv_deftag_str(sdp->sd_ref));
}

void
Dbg_syms_created(const char * name)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;

	dbg_print(MSG_INTL(MSG_SYM_CREATE), name);
}

void
Dbg_syms_resolving1(int ndx, const char * name, int row, int col)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;

	dbg_print(MSG_INTL(MSG_SYM_RESOLVING), ndx, name, row, col);
}

void
Dbg_syms_resolving2(Sym * osym, Sym * nsym, Sym_desc * sdp, Ifl_desc * ifl)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;
	if (DBG_NOTDETAIL())
		return;

	Elf_sym_table_entry(MSG_INTL(MSG_STR_OLD), osym,
	    sdp->sd_aux ? sdp->sd_aux->sa_verndx : 0, NULL,
	    sdp->sd_file->ifl_name);
	Elf_sym_table_entry(MSG_INTL(MSG_STR_NEW), nsym, 0, NULL,
	    ifl->ifl_name);
}

void
Dbg_syms_resolved(Sym_desc * sdp)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;
	if (DBG_NOTDETAIL())
		return;

	Elf_sym_table_entry(MSG_INTL(MSG_STR_RESOLVED), sdp->sd_sym,
	    sdp->sd_aux ? sdp->sd_aux->sa_verndx : 0, NULL,
	    conv_deftag_str(sdp->sd_ref));
}

void
Dbg_syms_nl()
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
}

static Boolean	symbol_title = TRUE;

static void
_Dbg_syms_reloc_title()
{
	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_SYM_BSS));

	symbol_title = FALSE;
}
void
Dbg_syms_reloc(Sym_desc * sdp, Boolean copy)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;

	if (symbol_title)
		_Dbg_syms_reloc_title();
	dbg_print(MSG_INTL(MSG_SYM_UPDATE), sdp->sd_name);

	if (DBG_NOTDETAIL())
		return;

	Elf_sym_table_entry(MSG_ORIG(copy ? MSG_SYM_COPY : MSG_STR_EMPTY),
	    sdp->sd_sym, sdp->sd_aux ? sdp->sd_aux->sa_verndx : 0, NULL,
	    conv_deftag_str(sdp->sd_ref));
}

void
Dbg_syms_lookup_aout(const char * name)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;

	dbg_print(MSG_INTL(MSG_SYM_AOUT), name);
}

void
Dbg_syms_lookup(const char * name, const char * file, const char * type)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;

	dbg_print(MSG_INTL(MSG_SYM_LOOKUP), name, file, type);
}

void
Dbg_syms_dlsym(const char * file, const char * name, int next)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_SYM_DLSYM), name, file,
	    MSG_ORIG(next ? MSG_SYM_NEXT : MSG_STR_EMPTY));
}

void
Dbg_syms_reduce(Sym_desc * sdp)
{
	static Boolean	sym_reduce_title = TRUE;

	if (DBG_NOTCLASS(DBG_SYMBOLS | DBG_VERSIONS))
		return;

	if (sym_reduce_title) {
		sym_reduce_title = FALSE;
		dbg_print(MSG_ORIG(MSG_STR_EMPTY));
		dbg_print(MSG_INTL(MSG_SYM_REDUCED));
	}

	dbg_print(MSG_INTL(MSG_SYM_REDUCING), sdp->sd_name);

	if (DBG_NOTDETAIL())
		return;

	Elf_sym_table_entry(MSG_ORIG(MSG_SYM_LOCAL), sdp->sd_sym,
	    sdp->sd_aux ? sdp->sd_aux->sa_verndx : 0, NULL,
	    sdp->sd_file->ifl_name);
}
