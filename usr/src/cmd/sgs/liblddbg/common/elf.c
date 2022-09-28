/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)elf.c	1.9	96/02/27 SMI"

/* LINTLIBRARY */

#include	"msg.h"
#include	"_debug.h"
#include	"libld.h"


void
Elf_elf_data(const char * str, Addr addr, Elf_Data * data,
	const char * file)
{
	dbg_print(MSG_INTL(MSG_ELF_ENTRY), str, addr,
		conv_d_type_str(data->d_type), data->d_size,
		data->d_off, data->d_align, file);
}

void
Elf_elf_data_title()
{
	dbg_print(MSG_INTL(MSG_ELF_TITLE));
}

void
_Dbg_elf_data_in(Os_desc * osp, Is_desc * isp)
{
	Shdr *		shdr = osp->os_shdr;
	Elf_Data *	data = isp->is_indata;
	const char *	file;

	if (isp->is_file)
		file = isp->is_file->ifl_name;
	else
		file = MSG_ORIG(MSG_STR_EMPTY);

	Elf_elf_data(MSG_INTL(MSG_STR_IN), shdr->sh_addr + data->d_off,
	    data, file);
}

void
_Dbg_elf_data_out(Os_desc * osp)
{
	Shdr *		shdr = osp->os_shdr;
	Elf_Data *	data = osp->os_outdata;

	Elf_elf_data(MSG_INTL(MSG_STR_OUT), shdr->sh_addr, data,
	    MSG_ORIG(MSG_STR_EMPTY));
}

void
Elf_elf_header(Ehdr * ehdr)
{
	Byte *	byte =	&(ehdr->e_ident[0]);

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_ELF_HEADER));

	dbg_print(MSG_ORIG(MSG_ELF_MAGIC), byte[EI_MAG0],
	    (byte[EI_MAG1] ? byte[EI_MAG1] : '0'),
	    (byte[EI_MAG2] ? byte[EI_MAG2] : '0'),
	    (byte[EI_MAG3] ? byte[EI_MAG3] : '0'));
	dbg_print(MSG_ORIG(MSG_ELF_CLASS),
	    conv_eclass_str(ehdr->e_ident[EI_CLASS]),
	    conv_edata_str(ehdr->e_ident[EI_DATA]));
	dbg_print(MSG_ORIG(MSG_ELF_MACHINE),
	    conv_emach_str(ehdr->e_machine), conv_ever_str(ehdr->e_version));
	dbg_print(MSG_ORIG(MSG_ELF_TYPE), conv_etype_str(ehdr->e_type));
	dbg_print(MSG_ORIG(MSG_ELF_FLAGS), ehdr->e_flags);
	dbg_print(MSG_ORIG(MSG_ELF_ESIZE), ehdr->e_entry, ehdr->e_ehsize,
	    ehdr->e_shstrndx);
	dbg_print(MSG_ORIG(MSG_ELF_SHOFF), ehdr->e_shoff, ehdr->e_shentsize,
	    ehdr->e_shnum);
	dbg_print(MSG_ORIG(MSG_ELF_PHOFF), ehdr->e_phoff, ehdr->e_phentsize,
	    ehdr->e_phnum);
}
