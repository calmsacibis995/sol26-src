/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)relocate.c	1.15	96/02/27 SMI"

/* LINTLIBRARY */

#include	"msg.h"
#include	"_debug.h"
#include	"libld.h"


void
Dbg_reloc_proc(Os_desc * osp, Is_desc * isp)
{
	const char *	str1, * str2;

	if (DBG_NOTCLASS(DBG_RELOC))
		return;

	if (osp->os_name)
		str1 = osp->os_name;
	else
		str1 =	MSG_INTL(MSG_STR_NULL);
	if (isp->is_file)
		str2 = isp->is_file->ifl_name;
	else
		str2 = MSG_INTL(MSG_STR_NULL);

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_REL_COLLECT), str1, str2);
	if (DBG_NOTDETAIL())
		return;

	/*
	 * Determine the relocation titles from the sections type.
	 */
	if (isp->is_shdr->sh_type == SHT_RELA)
		dbg_print(MSG_INTL(MSG_REL_RELA_TITLE_2));
	else
		dbg_print(MSG_INTL(MSG_REL_REL_TITLE_2));
}

void
Dbg_reloc_doactiverel()
{
	if (DBG_NOTCLASS(DBG_RELOC))
		return;
	if (DBG_NOTDETAIL())
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_REL_ACTIVE));
	dbg_print(MSG_INTL(MSG_REL_TITLE_1));
}

void
Dbg_reloc_doact(Half mach, Word rtype, Word off, Word value, const char * sym,
    Os_desc * osp)
{
	const char *	sec;

	if (DBG_NOTCLASS(DBG_RELOC))
		return;
	if (DBG_NOTDETAIL())
		return;
	if (osp) {
		sec = osp->os_name;
		off += osp->os_shdr->sh_offset;
	} else
		sec = MSG_ORIG(MSG_STR_EMPTY);

	dbg_print(MSG_INTL(MSG_REL_ARGS_6), MSG_ORIG(MSG_STR_EMPTY),
	    conv_reloc_type_str(mach, rtype), off, value, sec, sym);
}

void
Dbg_reloc_dooutrel(Rel_desc * rsp)
{
	if (DBG_NOTCLASS(DBG_RELOC))
		return;
	if (DBG_NOTDETAIL())
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_REL_CREATING));

	if (rsp->rel_osdesc->os_shdr->sh_type == SHT_RELA)
		dbg_print(MSG_INTL(MSG_REL_RELA_TITLE_2));
	else
		dbg_print(MSG_INTL(MSG_REL_REL_TITLE_2));
}

void
Dbg_reloc_apply(Word off, Word value, Os_desc * osp)
{
	const char *	name;

	if (DBG_NOTCLASS(DBG_RELOC))
		return;
	if (DBG_NOTDETAIL())
		return;

	/*
	 * If we've been called from ld(1) then the `osp' is specified.  In this
	 * case add the associated section header offset to the supplied offset,
	 * this allows us to print the relocation address as if it was the true
	 * offset within the output file.  If `osp' is null then we've been
	 * called from ld.so.1 so take the offset as is.  In this case we are
	 * also unaware of the actual section that's being relocated.
	 */
	if (osp) {
		name = osp->os_name ? osp->os_name : MSG_ORIG(MSG_STR_EMPTY);
		off += osp->os_shdr->sh_offset;
	} else
		name = MSG_ORIG(MSG_STR_EMPTY);

	/*
	 * Print the actual relocation being applied to the specified output
	 * section, the offset represents the actual relocation address, and the
	 * value is the new data being written to that address).
	 */
	dbg_print(MSG_INTL(MSG_REL_ARGS_3), off, value, name);
}

void
Dbg_reloc_out(Half mach, Word type, Rel * rel, const char * name, Os_desc * osp)
{
	if (DBG_NOTCLASS(DBG_RELOC))
		return;
	if (DBG_NOTDETAIL())
		return;

	Elf_reloc_entry(MSG_ORIG(MSG_STR_EMPTY), mach, type, (void *)rel,
	    osp->os_relosdesc->os_name, name);
}

/* VARARGS2 */
void
Dbg_reloc_in(Half mach, Word type, Rel * rel, const char * name, Is_desc * isp)
{
	const char *	iname;

	if (DBG_NOTCLASS(DBG_RELOC))
		return;
	if (DBG_NOTDETAIL())
		return;

	if (isp)
		iname = isp->is_name;
	else
		iname = MSG_ORIG(MSG_STR_EMPTY);

	Elf_reloc_entry(MSG_INTL(MSG_STR_IN), mach, type, (void *)rel, iname,
	    (name ? name : MSG_ORIG(MSG_STR_EMPTY)));
}

/*
 * Print a output relocation structure(Rel_desc).
 */
void
Dbg_reloc_ors_entry(Half mach, Rel_desc * orsp)
{
	const char *	os_name;

	if (DBG_NOTCLASS(DBG_RELOC))
		return;
	if (DBG_NOTDETAIL())
		return;

	if (orsp->rel_flags & FLG_REL_GOT)
		os_name = MSG_ORIG(MSG_SCN_GOT);
	else if (orsp->rel_flags & FLG_REL_PLT)
		os_name = MSG_ORIG(MSG_SCN_PLT);
	else if (orsp->rel_flags & FLG_REL_BSS)
		os_name = MSG_ORIG(MSG_SCN_BSS);
	else
		os_name = orsp->rel_osdesc->os_name;

	dbg_print(MSG_INTL(MSG_REL_ARGS_5), MSG_INTL(MSG_STR_OUT),
	    conv_reloc_type_str(mach, orsp->rel_rtype), orsp->rel_roffset,
	    os_name, orsp->rel_sym->sd_name);
}

/*
 * Print a Active relocation structure (Rel_desc).
 */
void
Dbg_reloc_ars_entry(Half mach, Rel_desc * arsp)
{
	const char *	os_name;

	if (DBG_NOTCLASS(DBG_RELOC))
		return;
	if (DBG_NOTDETAIL())
		return;

	if (arsp->rel_flags & FLG_REL_GOT)
		os_name = MSG_ORIG(MSG_SCN_GOT);
	else
		os_name = arsp->rel_osdesc->os_name;

	dbg_print(MSG_INTL(MSG_REL_ARGS_5), MSG_INTL(MSG_STR_ACT),
	    conv_reloc_type_str(mach, arsp->rel_rtype), arsp->rel_roffset,
	    os_name, arsp->rel_sym->sd_name);
}

void
Dbg_reloc_run(const char * file, Word type)
{
	if (DBG_NOTCLASS(DBG_RELOC))
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_REL_RELOC), file);

	if (DBG_NOTDETAIL())
		return;

	if (type == SHT_RELA) {
		dbg_print(MSG_INTL(MSG_REL_RELA_TITLE_3));
		dbg_print(MSG_INTL(MSG_REL_RELA_TITLE_4));
	} else
		dbg_print(MSG_INTL(MSG_REL_REL_TITLE_3));
}

/*
 * Print a relocation entry. This can either be an input or output
 * relocation record, and specifies the relocation section for which is
 * associated.
 */
void
Elf_reloc_entry(const char * prestr, Half mach, Word type, void * reloc,
    const char * sec, const char * name)
{
	if (type == SHT_RELA) {
		Elf32_Rela *	rela = (Elf32_Rela *)reloc;

		dbg_print(MSG_INTL(MSG_REL_ARGS_6), prestr,
		    conv_reloc_type_str(mach, ELF_R_TYPE(rela->r_info)),
			rela->r_offset, rela->r_addend, sec, name);
	} else {
		Elf32_Rel *	rel = (Elf32_Rel *)reloc;

		dbg_print(MSG_INTL(MSG_REL_ARGS_5), prestr,
		    conv_reloc_type_str(mach, ELF_R_TYPE(rel->r_info)),
			rel->r_offset, sec, name);
	}
}
