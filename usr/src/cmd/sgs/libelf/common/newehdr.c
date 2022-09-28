/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)newehdr.c	1.8	96/03/04 SMI" 	/* SVr4.0 1.4	*/

/*LINTLIBRARY*/

#pragma weak	elf32_newehdr = _elf32_newehdr

#include "syn.h"
#include <stdlib.h>
#include "libelf.h"
#include "decl.h"
#include "msg.h"


Elf32_Ehdr *
elf32_newehdr(Elf * elf)
{
	register
	Elf32_Ehdr	*eh;

	if (elf == 0)
		return (0);

	/*
	 * If reading file, return its hdr
	 */

	ELFWLOCK(elf)
	if (elf->ed_myflags & EDF_READ) {
		ELFUNLOCK(elf)
		if ((eh = elf32_getehdr(elf)) != 0) {
			ELFWLOCK(elf)
			elf->ed_ehflags |= ELF_F_DIRTY;
			ELFUNLOCK(elf)
		}
		return (eh);
	}

	/*
	 * Writing file
	 */

	if (elf->ed_class == ELFCLASSNONE)
		elf->ed_class = ELFCLASS32;
	else if (elf->ed_class != ELFCLASS32) {
		_elf_seterr(EREQ_CLASS, 0);
		ELFUNLOCK(elf)
		return (0);
	}
	if ((eh = elf32_getehdr(elf)) != 0) {	/* this cooks if necessary */
		elf->ed_ehflags |= ELF_F_DIRTY;
		ELFUNLOCK(elf)
		return (eh);
	}

	if ((eh = (Elf32_Ehdr *)malloc(sizeof (Elf32_Ehdr))) == 0) {
		_elf_seterr(EMEM_EHDR, 0);
		ELFUNLOCK(elf)
		return (0);
	}
	*eh = _elf32_ehdr_init;
	elf->ed_myflags |= EDF_EHALLOC;
	elf->ed_ehflags |= ELF_F_DIRTY;
	elf->ed_ehdr = eh;
	ELFUNLOCK(elf)
	return (eh);
}
