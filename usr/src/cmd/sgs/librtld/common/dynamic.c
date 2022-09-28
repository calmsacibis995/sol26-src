/*
 *	Copyright (c) 1995 by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 * Update any dynamic entry offsets.  One issue with dynamic entries is that
 * you only know whether they refer to a value or an offset if you know each
 * type.  Thus we check for all types we know about, it a type is found that
 * we don't know about then return and error as we have no idea what to do.
 */
#pragma ident	"@(#)dynamic.c	1.4	96/02/27 SMI"

#include	<libelf.h>
#include	<link.h>
#include	"libld.h"
#include	"msg.h"
#include	"_rtld.h"

int
update_dynamic(Cache * _cache, Addr addr, Off off, const char * file, Word null,
    Word data, Word func)
{
	Dyn *	dyn = (Dyn *)_cache->c_data->d_buf;

	/*
	 * Loop through the dynamic table updating all offsets.
	 */
	while (dyn->d_tag != DT_NULL) {
		switch (dyn->d_tag) {
		case DT_NEEDED:
		case DT_RELAENT:
		case DT_STRSZ:
		case DT_SYMENT:
		case DT_SONAME:
		case DT_RPATH:
		case DT_SYMBOLIC:
		case DT_RELENT:
		case DT_PLTREL:
		case DT_TEXTREL:
		case DT_VERDEFNUM:
		case DT_VERNEEDNUM:
		case DT_AUXILIARY:
		case DT_USED:
		case DT_FILTER:
			break;

		case DT_PLTGOT:
		case DT_HASH:
		case DT_STRTAB:
		case DT_SYMTAB:
		case DT_INIT:
		case DT_FINI:
		case DT_VERDEF:
		case DT_VERNEED:
			dyn->d_un.d_ptr += addr;
			break;

		/*
		 * If the memory image is being used, this element would have
		 * been initialized to the runtime linkers internal link-map
		 * list.  Clear it.
		 */
		case DT_DEBUG:
			dyn->d_un.d_val = 0;
			break;

		/*
		 * The number of relocations may have been reduced if
		 * relocations have been saved in the new image.  Thus we
		 * compute the new relocation size and start.
		 */
		case DT_RELASZ:
		case DT_RELSZ:
			dyn->d_un.d_val = ((data + func) * sizeof (Rel));
			break;

		case DT_RELA:
		case DT_REL:
			dyn->d_un.d_ptr = (addr + off + (null * sizeof (Rel)));
			break;

		case DT_PLTRELSZ:
			dyn->d_un.d_val = (func * sizeof (Rel));
			break;

		case DT_JMPREL:
			dyn->d_un.d_ptr = (addr + off +
				((null + data) * sizeof (Rel)));
			break;

		default:
			eprintf(ERR_FATAL, MSG_INTL(MSG_DT_UNKNOWN), file,
			    dyn->d_tag);
			return (1);
		}
		dyn++;
	}
	return (0);
}
