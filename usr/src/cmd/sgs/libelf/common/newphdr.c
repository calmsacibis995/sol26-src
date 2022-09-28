/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)newphdr.c	1.9	96/03/04 SMI" 	/* SVr4.0 1.3	*/

/*LINTLIBRARY*/

#pragma weak	elf32_newphdr = _elf32_newphdr


#include "syn.h"
#include <stdlib.h>
#include <memory.h>
#include "libelf.h"
#include "decl.h"
#include "msg.h"


Elf32_Phdr *
elf32_newphdr(Elf * elf, size_t count)
{
	Elf_Void *	ph;
	size_t		sz;
	Elf32_Phdr *	rc;
	unsigned	work;

	if (elf == 0)
		return (0);
	ELFRLOCK(elf)
	if (elf->ed_class != ELFCLASS32) {
		_elf_seterr(EREQ_CLASS, 0);
		ELFUNLOCK(elf)
		return (0);
	}
	ELFUNLOCK(elf)
	if (elf32_getehdr(elf) == 0) {		/* this cooks if necessary */
		_elf_seterr(ESEQ_EHDR, 0);
		return (0);
	}

	/*
	 * Free the existing header if appropriate.  This could reuse
	 * existing space if big enough, but that's unlikely, benefit
	 * would be negligible, and code would be more complicated.
	 */

	ELFWLOCK(elf)
	if (elf->ed_myflags & EDF_PHALLOC) {
		elf->ed_myflags &= ~EDF_PHALLOC;
		rc = elf->ed_phdr;
		free(rc);
	}

	/*
	 * Delete the header if count is zero.
	 */

	ELFACCESSDATA(work, _elf_work)
	if ((sz = count * _elf32_msize(ELF_T_PHDR, work)) == 0) {
		elf->ed_phflags &= ~ELF_F_DIRTY;
		elf->ed_phdr = 0;
		elf->ed_ehdr->e_phnum = 0;
		elf->ed_ehdr->e_phentsize = 0;
		elf->ed_phdrsz = 0;
		ELFUNLOCK(elf)
		return (0);
	}

	if ((ph = malloc(sz)) == 0) {
		_elf_seterr(EMEM_PHDR, 0);
		elf->ed_phflags &= ~ELF_F_DIRTY;
		elf->ed_phdr = 0;
		elf->ed_ehdr->e_phnum = 0;
		elf->ed_ehdr->e_phentsize = 0;
		elf->ed_phdrsz = 0;
		ELFUNLOCK(elf)
		return (0);
	}

	elf->ed_myflags |= EDF_PHALLOC;
	(void) memset(ph, 0, sz);
	elf->ed_phflags |= ELF_F_DIRTY;
	elf->ed_ehdr->e_phnum = (Elf32_Half)count;
	elf->ed_ehdr->e_phentsize = elf32_fsize(ELF_T_PHDR, 1, work);
	elf->ed_phdrsz = sz;
	elf->ed_phdr = rc = ph;

	ELFUNLOCK(elf)
	return (rc);
}
