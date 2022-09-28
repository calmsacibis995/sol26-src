/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)getshdr.c	1.8	96/03/04 SMI" 	/* SVr4.0 1.4	*/

/*LINTLIBRARY*/

#pragma weak	elf32_getshdr = _elf32_getshdr

#include "syn.h"
#include "libelf.h"
#include "decl.h"
#include "msg.h"


Elf32_Shdr *
elf32_getshdr(Elf_Scn * scn)
{
	Elf32_Shdr *	rc;
	Elf *		elf;
	if (scn == 0)
		return (0);
	elf = scn->s_elf;
	READLOCKS(elf, scn)
	if (elf->ed_class != ELFCLASS32) {
		READUNLOCKS(elf, scn)
		_elf_seterr(EREQ_CLASS, 0);
		return (0);
	}

	rc = scn->s_shdr;
	READUNLOCKS(elf, scn)
	return (rc);
}
