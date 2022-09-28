/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)conv.h	1.10	96/09/11 SMI"

#ifndef		CONV_DOT_H
#define		CONV_DOT_H

/*
 * Global include file for conversion library.
 */
#include	<stdlib.h>
#include	<libelf.h>
#include	<dlfcn.h>
#include	"libld.h"
#include	"sgs.h"
#include	"machdep.h"

/*
 * Functions
 */
extern	const char *	conv_d_type_str(Elf_Type);
extern	const char *	conv_deftag_str(Symref);
extern	const char *	conv_dlflag_str(int);
extern	const char *	conv_dlmode_str(int);
extern	const char *	conv_dyntag_str(Sword);
extern	const char *	conv_dynflag_1_str(Word);
extern	const char *	conv_eclass_str(Byte);
extern	const char *	conv_edata_str(Byte);
extern	const char *	conv_emach_str(Half);
extern	const char *	conv_ever_str(Word);
extern	const char *	conv_etype_str(Half);
extern	const char *	conv_info_bind_str(unsigned char);
extern	const char *	conv_info_type_str(unsigned char);
extern	const char *	conv_phdrflg_str(unsigned int);
extern	const char *	conv_phdrtyp_str(unsigned int);
extern	const char *	conv_reloc_type_str(Half, Word);
extern	const char *	conv_reloc_SPARC_type_str(Word);
extern	const char *	conv_reloc_386_type_str(Word);
extern	const char *	conv_reloc_PPC_type_str(Word);
extern	const char *	conv_secflg_str(unsigned int);
extern	const char *	conv_sectyp_str(unsigned int);
extern	const char *	conv_segaflg_str(unsigned int);
extern	const char *	conv_shndx_str(Half);
extern	const char *	conv_verflg_str(Half);

#endif
