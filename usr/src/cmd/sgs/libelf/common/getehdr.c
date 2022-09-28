/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */


#pragma ident	"@(#)getehdr.c	1.7	96/03/04 SMI" 	/* SVr4.0 1.8	*/

/*LINTLIBRARY*/

#pragma weak	elf32_getehdr = _elf32_getehdr


#include "syn.h"
#include "libelf.h"
#include "decl.h"
#include "msg.h"


Elf32_Ehdr *
elf32_getehdr(elf)
	Elf	*elf;
{
	Elf32_Ehdr *	rc;
	if (elf == 0)
		return (0);
	ELFWLOCK(elf);
	if (elf->ed_class != ELFCLASS32)
	{
		_elf_seterr(EREQ_CLASS, 0);
		ELFUNLOCK(elf);
		return (0);
	}
	if (elf->ed_ehdr == 0)
		(void) _elf_cook(elf);

	rc = elf->ed_ehdr;
	ELFUNLOCK(elf);

	return (rc);
}
