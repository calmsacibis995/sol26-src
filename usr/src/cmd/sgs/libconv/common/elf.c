/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)elf.c	1.5	96/02/26 SMI"

/* LINTLIBRARY */

/*
 * String conversion routines for ELF header attributes.
 */
#include	<stdio.h>
#include	"_conv.h"
#include	"elf_msg.h"

static const int classes[] = {
	MSG_ELFCLASSNONE,	MSG_ELFCLASS32,		MSG_ELFCLASS64
};

const char *
conv_eclass_str(Byte class)
{
	static char	string[STRSIZE] = { '\0' };

	if (class >= ELFCLASSNUM)
		return (conv_invalid_str(string, STRSIZE, (int)class, 0));
	else
		return (MSG_ORIG(classes[class]));

}

static const int datas[] = {
	MSG_ELFDATANONE,	MSG_ELFDATA2LSB, 	MSG_ELFDATA2MSB
};

const char *
conv_edata_str(Byte data)
{
	static char	string[STRSIZE] = { '\0' };

	if (data >= ELFDATANUM)
		return (conv_invalid_str(string, STRSIZE, (int)data, 0));
	else
		return (MSG_ORIG(datas[data]));

}

static const int machines[] = {
	MSG_EM_NONE,		MSG_EM_M32,		MSG_EM_SPARC,
	MSG_EM_386,		MSG_EM_68K,		MSG_EM_88K,
	MSG_EM_486,		MSG_EM_860,		MSG_EM_MIPS,
	MSG_EM_UNKNOWN9,	MSG_EM_MIPS_RS3_LE, 	MSG_EM_RS6000,
	MSG_EM_UNKNOWN12,	MSG_EM_UNKNOWN13,	MSG_EM_UNKNOWN14,
	MSG_EM_PA_RISC,		MSG_EM_nCUBE,		MSG_EM_VPP500,
	MSG_EM_SPARC32PLUS,	MSG_EM_UNKNOWN19,	MSG_EM_PPC
};

const char *
conv_emach_str(Half machine)
{
	static char	string[STRSIZE] = { '\0' };

	if (machine >= EM_NUM)
		return (conv_invalid_str(string, STRSIZE, (int)machine, 0));
	else
		return (MSG_ORIG(machines[machine]));

}

static const int etypes[] = {
	MSG_ET_NONE,		MSG_ET_REL,		MSG_ET_EXEC,
	MSG_ET_DYN,		MSG_ET_CORE
};

const char *
conv_etype_str(Half etype)
{
	static char	string[STRSIZE] = { '\0' };

	if (etype >= ET_NUM)
		return (conv_invalid_str(string, STRSIZE, (int)etype, 0));
	else
		return (MSG_ORIG(etypes[etype]));
}

static const int versions[] = {
	MSG_EV_NONE,		MSG_EV_CURRENT
};

const char *
conv_ever_str(Word version)
{
	static char	string[STRSIZE] = { '\0' };

	if (version >= EV_NUM)
		return (conv_invalid_str(string, STRSIZE, (int)version, 0));
	else
		return (MSG_ORIG(versions[version]));
}
