/*
 *	Copyright (c) 1995 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)relocate.c	1.9	96/05/15 SMI"

#include	<libelf.h>
#include	<dlfcn.h>
#include	"machdep.h"
#include	"reloc.h"
#include	"msg.h"
#include	"_rtld.h"

/*
 * Process all relocation records.  A new `Reloc' structure is allocated to
 * cache the processing decisions deduced, and these will be applied during
 * update_reloc().
 * A count of the number of null relocations (ie, relocations that will be
 * fixed and whos records will be nulled out), data and function relocations is
 * maintained.  This allows the relocation records themselves to be rearranged
 * (localized) later if necessary.
 *
 * The intention behind this file is maintain as much relocation logic as
 * possible in a generic form.
 */
int
count_reloc(Cache * cache, Cache * _cache, Rt_map * lmp, int flags, Addr addr,
    Word * null, Word * data, Word * func)
{
	Rel *		rel;
	Reloc *		reloc;
	Shdr *		shdr;
	int		cnt, _cnt;
	Sym *		syms;
	const char *	strs;
	Cache *		__cache;

	/*
	 * Determine the number of relocation entries we'll be dealing with.
	 */
	shdr = _cache->c_shdr;
	rel = (Rel *)_cache->c_data->d_buf;
	cnt = shdr->sh_size / shdr->sh_entsize;

	/*
	 * Allocate a relocation structure for this relocation section.
	 */
	if ((reloc = (Reloc *)malloc(cnt * sizeof (Reloc))) == 0)
		return (1);
	_cache->c_info = (void *)reloc;

	/*
	 * Determine the relocations associated symbol and string table.
	 */
	__cache = &cache[shdr->sh_link];
	syms = (Sym *)__cache->c_data->d_buf;
	shdr = __cache->c_shdr;
	__cache = &cache[shdr->sh_link];
	strs = (const char *)__cache->c_data->d_buf;

	/*
	 * Loop through the relocation table.
	 */
	for (_cnt = 0; _cnt < cnt; _cnt++, rel++, reloc++) {
		const char *	name;
		Sym *		sym;
		unsigned char	type = ELF_R_TYPE(rel->r_info);
		unsigned char	bind;
		unsigned long	offset = rel->r_offset + addr;
		Rt_map *	_lmp, * head;
		int		bound;

		/*
		 * Analyze the case where no relocations are to be applied.
		 */
		if ((flags & RTLD_REL_ALL) == 0) {
			/*
			 * Don't apply any relocations to the new image but
			 * insure their offsets are incremented to reflect any
			 * new fixed address.
			 */
			reloc->r_flags = FLG_R_INC;

			/*
			 * Undo any relocations that might have already been
			 * applied to the memory image.
			 */
			if (flags & RTLD_MEMORY)
				reloc->r_flags |= FLG_R_UNDO;

			/*
			 * Save the objects new address.
			 */
			reloc->r_value = addr;

			if (type == M_R_JMP_SLOT)
				(*func)++;
			else
				(*data)++;
			continue;
		}

		/*
		 * Determine the symbol binding of the relocation. Don't assume
		 * that relative relocations are simply M_R_RELATIVE.  Although
		 * a pic generated shared object can normally be viewed as
		 * having relative and non-relative relocations, a non-pic
		 * shared object will contain a number of relocations against
		 * local symbols (normally sections).  If a relocation is
		 * against a local symbol it qualifies as a relative relocation.
		 */
		sym = (syms + ELF_R_SYM(rel->r_info));
		if ((type == M_R_RELATIVE) || (type == M_R_NONE) ||
		    (ELF_ST_BIND(sym->st_info) == STB_LOCAL))
			bind = STB_LOCAL;
		else
			bind = STB_GLOBAL;

		/*
		 * Analyze the case where only relative relocations are to be
		 * applied.
		 */
		if ((flags & RTLD_REL_ALL) == RTLD_REL_RELATIVE) {
			if (flags & RTLD_MEMORY) {
				if (bind == STB_LOCAL) {
					/*
					 * Save the relative relocations from
					 * the memory image.  The data itself
					 * has already been relocated, thus
					 * clear the relocation record so that
					 * it will not be performed again.
					 */
					reloc->r_flags = FLG_R_CLR;
					(*null)++;
				} else {
					/*
					 * Any non-relative relocation must be
					 * undone, and the relocation records
					 * offset updated to any new fixed
					 * address.
					 */
					reloc->r_flags =
					    (FLG_R_UNDO | FLG_R_INC);
					reloc->r_value = addr;
					if (type == M_R_JMP_SLOT)
						(*func)++;
					else
						(*data)++;
				}
			} else {
				if (bind == STB_LOCAL) {
					/*
					 * Apply relative relocation to the
					 * file image.  Clear the relocation
					 * record so that it will not be
					 * performed again.
					 */
					reloc->r_flags =
					    (FLG_R_APPLY | FLG_R_CLR);
					reloc->r_value = addr;
					if (IS_PC_RELATIVE(type))
						reloc->r_value -= offset;
					reloc->r_name =
					    MSG_INTL(MSG_STR_UNKNOWN);
					(*null)++;
				} else {
					/*
					 * Any non-relative relocation should be
					 * left alone, but its offset should be
					 * updated to any new fixed address.
					 */
					reloc->r_flags = FLG_R_INC;
					reloc->r_value = addr;
					if (type == M_R_JMP_SLOT)
						(*func)++;
					else
						(*data)++;
				}
			}
			continue;
		}

		/*
		 * Analyze the case where more than just relative relocations
		 * are to be applied.
		 */
		if (bind == STB_LOCAL) {
			if (flags & RTLD_MEMORY) {
				/*
				 * Save the relative relocations from the memory
				 * image.  The data itself has already been
				 * relocated, thus clear the relocation record
				 * so that it will not be performed again.
				 */
				reloc->r_flags = FLG_R_CLR;
			} else {
				/*
				 * Apply relative relocation to the file image.
				 * Clear the relocation record so that it will
				 * not be performed again.
				 */
				reloc->r_flags = (FLG_R_APPLY | FLG_R_CLR);
				reloc->r_value = addr;
				if (IS_PC_RELATIVE(type))
					reloc->r_value -= offset;
				reloc->r_name = MSG_INTL(MSG_STR_UNKNOWN);
			}
			(*null)++;
			continue;
		}

		/*
		 * At this point we're dealing with a non-relative relocation
		 * that requires the symbol definition.
		 */
		name = strs + sym->st_name;

		/*
		 * Find the symbol.  As the object being investigated is already
		 * a part of this process, the symbol lookup will likely
		 * succeed.  However, because of lazy binding, there is still
		 * the possibility of a dangling .plt relocation.  dldump()
		 * users might be encouraged to set LD_BIND_NOW.
		 */
		bound = 0;
		head = LIST(lmp)->lm_head;
		if (type == M_R_COPY)
			head = (Rt_map *)NEXT(head);

		if ((sym = lookup_sym(name, PERMIT(lmp), lmp, head, &_lmp,
		    LKUP_DEFT)) != 0) {
			/*
			 * Determine from the various relocation requirements
			 * whether this binding is appropriate.
			 */
			if (((flags & RTLD_REL_ALL) == RTLD_REL_ALL) ||
			    ((flags & RTLD_REL_EXEC) &&
			    (FLAGS(_lmp) & FLG_RT_ISMAIN)) ||
			    ((flags & RTLD_REL_DEPENDS) &&
			    (!(FLAGS(_lmp) & FLG_RT_ISMAIN))) ||
			    ((flags & RTLD_REL_PRELOAD) &&
			    (FLAGS(_lmp) & FLG_RT_PRELOAD)) ||
			    ((flags & RTLD_REL_SELF) && (lmp == _lmp)))
				bound = 1;
		}

		if (flags & RTLD_MEMORY) {
			if (bound) {
				/*
				 * We know that all data relocations will have
				 * been performed at process startup thus clear
				 * the relocation record so that it will not be
				 * performed again.  However, we don't know what
				 * function relocations have been performed
				 * because of lazy binding - regardless, we can
				 * leave all the function relocation records in
				 * place, because if the function has already
				 * been bound the record won't be referenced
				 * anyway.  In the case of using LD_BIND_NOW,
				 * a function may be bound twice - so what.
				 */
				if (type == M_R_JMP_SLOT) {
					reloc->r_flags = FLG_R_INC;
					(*func)++;
				} else {
					reloc->r_flags = FLG_R_CLR;
					(*null)++;
				}
			} else {
				/*
				 * Clear any unrequired relocation.
				 */
				reloc->r_flags = FLG_R_UNDO | FLG_R_INC;
				reloc->r_value = addr;
				if (type == M_R_JMP_SLOT)
					(*func)++;
				else
					(*data)++;
			}
		} else {
			if (bound) {
				/*
				 * Apply the global relocation to the file
				 * image.  Clear the relocation record so that
				 * it will not be performed again.
				 */
				reloc->r_value = sym->st_value;
				if (IS_PC_RELATIVE(type))
					reloc->r_value -= offset;
				if ((!(FLAGS(_lmp) & FLG_RT_FIXED)) &&
				    (sym->st_shndx != SHN_ABS))
					reloc->r_value += ADDR(_lmp);

				reloc->r_flags = FLG_R_APPLY | FLG_R_CLR;
				reloc->r_name = name;
				(*null)++;
			} else {
				/*
				 * Do not apply any unrequired relocations.
				 */
				reloc->r_flags = FLG_R_INC;
				reloc->r_value = addr;
				if (type == M_R_JMP_SLOT)
					(*func)++;
				else
					(*data)++;
			}
		}
	}
	return (0);
}


/*
 * Perform any relocation updates to the new image using the information from
 * the `Reloc' structure constructed during count_reloc().
 */
void
update_reloc(Cache * ocache, Cache * icache, Cache * _icache, const char * name,
    Rt_map * lmp, Rel ** null, Rel ** data, Rel ** func)
{
	Shdr *		shdr;
	Rel *		rel;
	Reloc *		reloc;
	int		cnt, _cnt;
	Cache *		orcache, * ircache = 0;
	Half		ndx;

	/*
	 * Set up to read the output relocation table.
	 */
	shdr = _icache->c_shdr;
	rel = (Rel *)_icache->c_data->d_buf;
	reloc = (Reloc *)_icache->c_info;
	cnt = shdr->sh_size / shdr->sh_entsize;

	/*
	 * Loop through the relocation table.
	 */
	for (_cnt = 0; _cnt < cnt; _cnt++, rel++, reloc++) {
		unsigned char *	iaddr, * oaddr;
		Addr		off;
		unsigned char	type = ELF_R_TYPE(rel->r_info);

		/*
		 * Ignore null relocations (these may have been created from a
		 * previous dldump() of this image).
		 */
		if (type == M_R_NONE) {
			(*null)++;
			continue;
		}

		/*
		 * Determine the section being relocated if we haven't already
		 * done so (we may have had to skip over some null relocation to
		 * get to the first valid offset).  The System V ABI states that
		 * a relocation sections sh_info field indicates the section
		 * that must be relocated.  However, on Intel it seems that the
		 * .rel.plt sh_info records the section index of the .plt, when
		 * in fact it's the .got that gets relocated.
		 * To generically be able to cope with this anomaly, search for
		 * the appropriate section to be relocated by comparing the
		 * offset of the first relocation record against each sections
		 * offset and size.
		 */
		if (ircache == (Cache *)0) {
			_icache = icache;
			_icache++;

			for (ndx = 1; _icache->c_flags != FLG_C_END; ndx++,
			    _icache++) {
				Addr	bgn, end;

				shdr = _icache->c_shdr;
				bgn = shdr->sh_addr;
				end = bgn + shdr->sh_size;

				if ((rel->r_offset >= bgn) &&
				    (rel->r_offset <= end))
					break;
			}
			ircache = &icache[ndx];
			orcache = &ocache[ndx];
		}

		/*
		 * Determine the relocation location of both the input and
		 * output data.  Take into account that an input section may be
		 * NOBITS (ppc .plt for example).
		 */
		off = rel->r_offset - ircache->c_shdr->sh_addr;
		if (ircache->c_data->d_buf)
			iaddr = (unsigned char *)ircache->c_data->d_buf + off;
		else
			iaddr = 0;
		oaddr = (unsigned char *)orcache->c_data->d_buf + off;

		/*
		 * Apply the relocation to the new output image.  Any base
		 * address, or symbol value, will have been saved in the reloc
		 * structure during count_reloc().
		 */
		if (reloc->r_flags & FLG_R_APPLY)
			apply_reloc(rel, reloc, name, oaddr, lmp);

		/*
		 * Undo a relocation that has already been applied to the
		 * memory image by the runtime linker.  Using the original
		 * file, determine the relocation offset original value and
		 * restore the new image to that value.
		 */
		if (reloc->r_flags & FLG_R_UNDO)
			undo_reloc(rel, oaddr, iaddr, reloc);

		/*
		 * If a relocation has been applied then the relocation record
		 * should be cleared so that the relocation isn't applied again
		 * when the new image is used.
		 */
		if (reloc->r_flags & FLG_R_CLR)
			clear_reloc((*null)++);

		/*
		 * If a relocation isn't applied, update the relocation record
		 * to take into account the new address of the image.
		 */
		if (reloc->r_flags & FLG_R_INC) {
			Rel *	_rel;

			if (type == M_R_JMP_SLOT)
				_rel = (*func)++;
			else
				_rel = (*data)++;

			*_rel = *rel;
			inc_reloc(_rel, reloc, oaddr, iaddr);
		}
	}
}
