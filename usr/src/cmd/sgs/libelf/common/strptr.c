/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)strptr.c	1.7	96/03/04 SMI" 	/* SVr4.0 1.6	*/

/*LINTLIBRARY*/

#pragma weak	elf_strptr = _elf_strptr


#include "syn.h"
#include "libelf.h"
#include "decl.h"
#include "msg.h"


char *
elf_strptr(Elf * elf, size_t ndx, size_t off)
{
	Elf_Scn *	s;
	Elf_Data *	d;
	char *		rc;

	if (elf == 0)
		return (0);
	if ((s = elf_getscn(elf, ndx)) == 0) {
		_elf_seterr(EREQ_STRSCN, 0);
		return (0);
	}
	READLOCKS(elf, s)
	if ((s->s_shdr == 0) || (s->s_shdr->sh_type != SHT_STRTAB)) {
		_elf_seterr(EREQ_STRSCN, 0);
		READUNLOCKS(elf, s)
		return (0);
	}

	/*
	 * If the layout bit is set, use the offsets and
	 * sizes in the data buffers.  Otherwise, take
	 * data buffers in order.
	 */

	d = 0;
	if (elf->ed_uflags & ELF_F_LAYOUT) {
		while ((d = _elf_locked_getdata(s, d)) != 0) {
			if (d->d_buf == 0)
				continue;
			if ((off >= d->d_off) &&
			    (off < d->d_off + d->d_size)) {
				rc = (char *)d->d_buf + off - d->d_off;
				READUNLOCKS(elf, s)
				return (rc);
			}
		}
	} else {
		size_t	sz = 0, j;
		while ((d = _elf_locked_getdata(s, d)) != 0) {
			if (((j = d->d_align) > 1) && (sz % j != 0)) {
				j -= sz % j;
				sz += j;
				if (off < j)
					break;
				off -= j;
			}
			if (d->d_buf != 0) {
				if (off < d->d_size) {
					rc = (char *)d->d_buf + off;
					READUNLOCKS(elf, s)
					return (rc);
				}
			}
			sz += d->d_size;
			if (off < d->d_size)
				break;
			off -= d->d_size;
		}
	}
	_elf_seterr(EREQ_STROFF, 0);
	READUNLOCKS(elf, s)
	return (0);
}
