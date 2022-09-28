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
#pragma ident	"@(#)i386_elf.c	1.33	96/10/07 SMI"

/*
 * x86 machine dependent and ELF file class dependent functions.
 * Contains routines for performing function binding and symbol relocations.
 */
#include	"_synonyms.h"

#include	<stdio.h>
#include	<sys/elf.h>
#include	<sys/elf_386.h>
#include	<sys/mman.h>
#include	<signal.h>
#include	<dlfcn.h>
#include	<unistd.h>
#include	<synch.h>
#include	<string.h>
#include	"_rtld.h"
#include	"_elf.h"
#include	"msg.h"
#include	"profile.h"
#include	"debug.h"
#include	"reloc.h"
#include	"conv.h"


extern void	elf_rtbndr(Rt_map *, unsigned long, caddr_t);

#ifdef	PROF

static const unsigned char dyn_plt_template[] = {
	0x68, 0xaa, 0xff, 0xaa, 0xff,		/* pushl	$0xffaaffaa */
	0xe8, 0xfc, 0xff, 0xff, 0xff,		/* call		0xfffffffc */
	0x83, 0xc4, 0x04,			/* addl		$0x4,%esp */
	0xff, 0xe0,				/* jmp		*%eax */
	0x90					/* nop */
};


/*
 * After the first call to a plt, elf_bndr() will have determined the true
 * address of the function being bound.  If the library to which the function
 * belongs is being profiled then the plt is written to call the 'dynamic plt'
 * which calls elf_plt_cg (which counts the fact that the library was called),
 * and then the real function is called.
 *
 * Note: it is possible that the 'dynamic plt' for the profiled function
 * has already been created.  If this is the case we will just point
 * the plt to this 'dynamic plt'.  By doing this we can re-use the same
 * dynamic plt for multiple 'plt's' which point to the same function.
 */
static void
elf_plt_cg_write(Rel * rptr, Rt_map * lmp, Rt_map * nlmp, int symndx)
{
	unsigned long	got_entry;
	unsigned long	addr;
	caddr_t		dyn_plt;

	dyn_plt = ((caddr_t)DYNPLT(nlmp) + (symndx * M_DYN_PLT_ENT));

	/*
	 * Have we initialized this dynamic plt entry yet?  If we haven't do it
	 * now.  Otherwise this function has been called before, but from a
	 * different plt (ie. from another shared object).  In that case
	 * we just set the plt to point to the new dyn_plt.
	 */
	if (*dyn_plt == 0) {
		(void) memcpy(dyn_plt, dyn_plt_template,
		    sizeof (dyn_plt_template));

		/* LINTED */
		*(unsigned long *)(&dyn_plt[1]) = symndx;
		/*
		 * calls are relative, so I need to figure out the relative
		 * address to plt_cg_interp.
		 */
		/* LINTED */
		*(unsigned long *)(&dyn_plt[6]) = (unsigned long)plt_cg_interp -
		    (unsigned long)(dyn_plt + 10);

	}

	addr = ADDR(lmp);
	got_entry = (unsigned long)rptr->r_offset;
	if (!(FLAGS(lmp) & FLG_RT_FIXED))
		got_entry += addr;

	*(unsigned long *)got_entry = (unsigned long)dyn_plt;
}

#endif

/*
 * Function binding routine - invoked on the first call to a function through
 * the procedure linkage table;
 * passes first through an assembly language interface.
 *
 * Takes the offset into the relocation table of the associated
 * relocation entry and the address of the link map (rt_private_map struct)
 * for the entry.
 *
 * Returns the address of the function referenced after re-writing the PLT
 * entry to invoke the function directly.
 *
 * On error, causes process to terminate with a signal.
 */
/* ARGSUSED2 */
unsigned long
elf_bndr(Rt_map * lmp, unsigned long reloff, caddr_t from)
{
	Rt_map *	nlmp;
	unsigned long	addr, symval;
	char *		name;
	Rel *		rptr;
	Sym *		sym, * nsym;
	int		bind;

	PRF_MCOUNT(1, elf_bndr);
	if ((bind = bind_guard(THR_FLG_BIND)) == 1)
		(void) rw_rdlock(&bindlock);

	/*
	 * Perform some basic sanity checks.  If we didn't get a load map or
	 * the relocation offset is invalid then its possible someone has walked
	 * over the .got entries.  Note, if the relocation offset is invalid
	 * we can't depend on it to deduce the plt offset, so use -1 instead -
	 * at this point you have to wonder if we can depended on anything.
	 */
	if (!lmp || ((reloff % sizeof (Rel)) != 0)) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_REL_PLTENTRY),
		    conv_reloc_386_type_str(R_386_JMP_SLOT),
		    (!lmp ? (reloff / sizeof (Rel)) : -1), from,
		    (!lmp ? MSG_INTL(MSG_REL_PLTREF) :
		    MSG_INTL(MSG_REL_RELOFF)));
		exit(1);
	}

	/*
	 * Use relocation entry to get symbol table entry and symbol name.
	 */
	addr = (unsigned long)JMPREL(lmp);
	rptr = (Rel *)(addr + reloff);
	sym = (Sym *)((unsigned long)SYMTAB(lmp) +
		(ELF_R_SYM(rptr->r_info) * SYMENT(lmp)));
	name = (char *)(STRTAB(lmp) + sym->st_name);

	/*
	 * Find definition for symbol.
	 */
	if ((nsym = lookup_sym(name, PERMIT(lmp), lmp, LIST(lmp)->lm_head,
	    &nlmp, LKUP_DEFT)) != 0) {
		symval = nsym->st_value;
		if (!(FLAGS(nlmp) & FLG_RT_FIXED) &&
		    (nsym->st_shndx != SHN_ABS))
			symval += ADDR(nlmp);
		if ((lmp != nlmp) && (!(MODE(nlmp) & RTLD_NODELETE))) {
			/*
			 * Record that this new link map is now bound to the
			 * caller.
			 */
			if (bound_add(REF_SYMBOL, lmp, nlmp) == 0)
				exit(1);
		}
	} else {
		eprintf(ERR_FATAL, MSG_INTL(MSG_REL_NOSYM), NAME(lmp), name);
		exit(1);
	}

	/*
	 * Print binding information and rebuild PLT entry.
	 */
	DBG_CALL(Dbg_bind_global(NAME(lmp), from, from - ADDR(lmp),
	    (reloff / sizeof (Rel)), NAME(nlmp), (caddr_t)symval,
	    (caddr_t)nsym->st_value, name));

	if (!(rtld_flags & RT_FL_NOBIND)) {
#ifdef	PROF
		if (FLAGS(nlmp) & FLG_RT_PROFILE) {
			int	symndx = (((int)nsym -
				    (int)SYMTAB(nlmp)) / SYMENT(nlmp));

			/*
			 * Write PLT entry to jump to profile counter
			 */
			elf_plt_cg_write(rptr, lmp, nlmp, symndx);

			/*
			 * Call the call graph interpretor to initialize this
			 * first call.
			 */
			(void) plt_cg_interp(symndx, (caddr_t)from,
			    (caddr_t)symval);
		} else {
#endif
		addr = rptr->r_offset;
		if (!(FLAGS(lmp) & FLG_RT_FIXED))
			addr += ADDR(lmp);
		*(unsigned long *)addr = symval;
#ifdef	PROF
		}
#endif
	}
	if (bind) {
		(void) rw_unlock(&bindlock);
		(void) bind_clear(THR_FLG_BIND);
	}

	return (symval);
}

/*
 * Read and process the relocations for one link object, we assume all
 * relocation sections for loadable segments are stored contiguously in
 * the file.
 */
int
elf_reloc(Rt_map * lmp, int plt)
{
	unsigned long	relbgn, relend, relsiz, basebgn;
	unsigned long	pltbgn, pltend, _pltbgn, _pltend;
	unsigned long	roffset, rsymndx, psymndx = 0, etext;
	unsigned char	rtype;
	long		value, pvalue;
	Sym *		symref, * psymref, * symdef, * psymdef;
	char *		name, * pname;
	Rt_map *	_lmp, * plmp;
	int		textrel = 0, bound = 0, error = 0, noplt = 0;
	Rel *		rel;

	DEF_TIME(interval1);
	DEF_TIME(interval2);

	PRF_MCOUNT(2, elf_reloc);
	GET_TIME(interval1);

	/*
	 * Although only necessary for lazy binding, initialize the first
	 * global offset entry to go to elf_rtbndr().  dbx(1) seems
	 * to find this useful.
	 */
	if ((plt == 0) && PLTGOT(lmp))
		elf_plt_init((unsigned long *)PLTGOT(lmp), lmp);

	/*
	 * Initialize the plt start and end addresses.
	 */
	if ((pltbgn = (unsigned long)JMPREL(lmp)) != 0)
		pltend = pltbgn + (unsigned long)(PLTRELSZ(lmp));

	relsiz = (unsigned long)(RELENT(lmp));
	basebgn = ADDR(lmp);
	etext = ETEXT(lmp);

	/*
	 * If we've been called upon to promote an RTLD_LAZY object to an
	 * RTLD_NOW then we're only interested in scaning the .plt table.
	 * An uninitialized .plt is the case where the associated got entry
	 * points back to the plt itself.  Determine the range of the real .plt
	 * entries using the _PROCEDURE_LINKAGE_TABLE_ symbol.
	 */
	if (plt) {
		relbgn = pltbgn;
		relend = pltend;
		if (!relbgn || (relbgn == relend))
			return (1);

		if ((symdef = elf_find_sym(MSG_ORIG(MSG_SYM_PLT), lmp, &_lmp, 0,
		    elf_hash(MSG_ORIG(MSG_SYM_PLT)))) == 0)
			return (1);

		_pltbgn = symdef->st_value;
		if (!(FLAGS(lmp) & FLG_RT_FIXED) &&
		    (symdef->st_shndx != SHN_ABS))
			_pltbgn += basebgn;
		_pltend = _pltbgn + (((PLTRELSZ(lmp) / relsiz) +
		    M_PLT_XNumber) * M_PLT_ENTSIZE);

	} else {
		/*
		 * The relocation sections appear to the run-time linker as a
		 * single table.  Determine the address of the beginning and end
		 * of this table.  There are two different interpretations of
		 * the ABI at this point:
		 *
		 *   o	The REL table and its associated RELSZ indicate the
		 *	concatenation of *all* relocation sections (this is the
		 *	model our link-editor constructs).
		 *
		 *   o	The REL table and its associated RELSZ indicate the
		 *	concatenation of all *but* the .plt relocations.  These
		 *	relocations are specified individually by the JMPREL and
		 *	PLTRELSZ entries.
		 *
		 * Determine from our knowledege of the relocation range and
		 * .plt range, the range of the total relocation table.  Note
		 * that one other ABI assumption seems to be that the .plt
		 * relocations always follow any other relocations, the
		 * following range checking drops that assumption.
		 */
		relbgn = (unsigned long)(REL(lmp));
		relend = relbgn + (unsigned long)(RELSZ(lmp));
		if (pltbgn) {
			if (!relbgn || (relbgn > pltbgn))
				relbgn = pltbgn;
			if (!relbgn || (relend < pltend))
				relend = pltend;
		}
	}
	if (!relbgn || (relbgn == relend))
		return (1);

	DBG_CALL(Dbg_reloc_run(NAME(lmp), M_REL_SHT_TYPE));

	/*
	 * If we're processing a dynamic executable in lazy mode there is no
	 * need to scan the .rel.plt table, however if we're processing a shared
	 * object in lazy mode the .got addresses associated to each .plt must
	 * be relocated to reflect the location of the shared object.
	 */
	if (pltbgn && !(MODE(lmp) & RTLD_NOW) && (FLAGS(lmp) & FLG_RT_FIXED))
		noplt = 1;

	GET_TIME(interval2);
	SAV_TIME(interval1, NAME(lmp));
	SAV_TIME(interval2, "  begin reloc loop");

	/*
	 * Loop through relocations.
	 */
	while (relbgn < relend) {
		rsymndx = ELF_R_SYM(((Rel *)relbgn)->r_info);
		rtype = ELF_R_TYPE(((Rel *)relbgn)->r_info);
		roffset = ((Rel *)relbgn)->r_offset;
		rel = (Rel *)relbgn;
		relbgn += relsiz;

		/*
		 * Optimizations.
		 */
		if (rtype == R_386_NONE)
			continue;
		if (noplt && ((unsigned long)rel >= pltbgn) &&
		    ((unsigned long)rel < pltend))
			continue;

		/*
		 * If this is a shared object, add the base address to offset.
		 */
		if (!(FLAGS(lmp) & FLG_RT_FIXED)) {
			roffset += basebgn;

			/*
			 * If we're processing lazy bindings, we have to step
			 * through the plt entries and add the base address
			 * to the corresponding got entry.
			 */
			if ((plt == 0) && (rtype == R_386_JMP_SLOT) &&
			    !(MODE(lmp) & RTLD_NOW)) {
				*(unsigned long *)roffset += basebgn;
				continue;
			}
		}

		/*
		 * If we're promoting plts determine if this one has already
		 * been written.
		 */
		if (plt) {
			if ((*(unsigned long *)roffset < _pltbgn) ||
			    (*(unsigned long *)roffset > _pltend))
				continue;
		}

		/*
		 * If this relocation is not against part of the image
		 * mapped into memory we skip it.
		 */
		if ((roffset < ADDR(lmp)) || (roffset > (ADDR(lmp) +
		    MSIZE(lmp))))
			continue;

		/*
		 * If this object has relocations in the text segment, turn
		 * off the write protect.
		 */
		if (roffset < etext) {
			if (!textrel) {
				if (elf_set_prot(lmp, PROT_WRITE) == 0)
					return (0);
				textrel = 1;
			}
		}

		/*
		 * If a symbol index is specified then get the symbol table
		 * entry, locate the symbol definition, and determine its
		 * address.
		 */
		if (rsymndx) {
			/*
			 * Get the local symbol table entry.
			 */
			symref = (Sym *)((unsigned long)SYMTAB(lmp) +
					(rsymndx * SYMENT(lmp)));

			/*
			 * If this is a local symbol, just use the base address.
			 * (we should have no local relocations in the
			 * executable).
			 */
			if (ELF_ST_BIND(symref->st_info) == STB_LOCAL) {
				value = basebgn;
				name = (char *)0;
			} else {
				/*
				 * If the symbol index is equal to the previous
				 * symbol index relocation we processed then
				 * reuse the previous values. (Note that there
				 * have been cases where a relocation exists
				 * against a copy relocation symbol, our ld(1)
				 * should optimize this away, but make sure we
				 * don't use the same symbol information should
				 * this case exist).
				 */
				if ((rsymndx == psymndx) &&
				    (rtype != R_386_COPY)) {
					/* LINTED */
					value = pvalue;
					/* LINTED */
					name = pname;
					/* LINTED */
					symdef = psymdef;
					/* LINTED */
					symref = psymref;
					/* LINTED */
					_lmp = plmp;
				} else {
					int	flags;

					/*
					 * Lookup the symbol definition.  If
					 * this is for a copy relocation don't
					 * bother looking in the executable.
					 */
					name = (char *)(STRTAB(lmp) +
					    symref->st_name);

					_lmp = LIST(lmp)->lm_head;
					if (rtype == R_386_COPY)
						_lmp = (Rt_map *)NEXT(_lmp);

					if (rtype == R_386_JMP_SLOT)
						flags = LKUP_DEFT;
					else
						flags = LKUP_SPEC;

					symdef = lookup_sym(name, PERMIT(lmp),
					    lmp, _lmp, &_lmp, flags);

					/*
					 * If the symbol is not found and the
					 * reference was not to a weak symbol,
					 * report an error.  Weak references may
					 * be unresolved.
					 */
					if (symdef == 0) {
					    if (ELF_ST_BIND(symref->st_info) !=
						STB_WEAK) {
						if (rtld_flags & RT_FL_WARN) {
						    (void) printf(MSG_INTL(
							MSG_LDD_SYM_NFOUND),
							name, NAME(lmp));
						    continue;
						} else {
						    eprintf(ERR_FATAL,
							MSG_INTL(MSG_REL_NOSYM),
							NAME(lmp), name);
						    return (0);
						}
					    } else {
						DBG_CALL(Dbg_bind_weak(
						    NAME(lmp), (caddr_t)roffset,
						    (caddr_t)
						    (roffset - basebgn), name));
						continue;
					    }
					}

					/*
					 * If symbol was found in an object
					 * other than the referencing object
					 * then set the boundto bit in the
					 * defining object.
					 */
					if ((lmp != _lmp) &&
					    (!(MODE(_lmp) & RTLD_NODELETE))) {
						FLAGS(_lmp) |= FLG_RT_BOUND;
						bound = 1;
					}

					/*
					 * Calculate the location of definition;
					 * symbol value plus base address of
					 * containing shared object.
					 */
					value = symdef->st_value;
					if (!(FLAGS(_lmp) & FLG_RT_FIXED) &&
					    (symdef->st_shndx != SHN_ABS))
						value += ADDR(_lmp);

					/*
					 * Retain this symbol index and the
					 * value in case it can be used for the
					 * subsequent relocations.
					 */
					if (rtype != R_386_COPY) {
						psymndx = rsymndx;
						pvalue = value;
						pname = name;
						psymdef = symdef;
						psymref = symref;
						plmp = _lmp;
					}
				}

				/*
				 * If relocation is PC-relative, subtract
				 * offset address.
				 */
				if (IS_PC_RELATIVE(rtype))
					value -= roffset;

				DBG_CALL(Dbg_bind_global(NAME(lmp),
				    (caddr_t)roffset,
				    (caddr_t)(roffset - basebgn), -1,
				    NAME(_lmp), (caddr_t)value,
				    (caddr_t)symdef->st_value, name));
			}
		} else {
			value = basebgn;
			name = (char *)0;
		}

		/*
		 * Call relocation routine to perform required relocation.
		 */
		DBG_CALL(Dbg_reloc_in(M_MACH, M_REL_SHT_TYPE, rel, name, 0));

		switch (rtype) {
		case R_386_COPY:
			if (elf_copy_reloc(name, symref, lmp, (void *)roffset,
			    symdef, _lmp, (const void *)value) == 0)
				error = 1;
			if (!(FLAGS(_lmp) & FLG_RT_COPYTOOK)) {
				if (list_append(&COPY(lmp), &COPY(_lmp)) == 0)
					error = 1;
				FLAGS(_lmp) |= FLG_RT_COPYTOOK;
			}
			break;
		case R_386_JMP_SLOT:
#ifdef	PROF
			if (FLAGS(_lmp) & FLG_RT_PROFILE) {
				int	symndx = (((int)symdef -
					    (int)SYMTAB(_lmp)) / SYMENT(_lmp));

				/*
				 * Write PLT entry to jump to profile counter
				 */
				elf_plt_cg_write(rel, lmp, _lmp, symndx);

				/*
				 * Call the call graph interpretor to
				 * initialize this first call element.
				 */
				(void) plt_cg_interp(symndx,
					(caddr_t)PRF_UNKNOWN, (caddr_t)value);
			} else {
#endif
				/*
				 * Write standard PLT entry to jump directly
				 * to newly bound function.
				 */
				DBG_CALL(Dbg_reloc_apply(roffset,
				    (unsigned long)value, 0));
				*(unsigned long *)roffset = value;
#ifdef	PROF
			}
#endif
			break;
		default:
			/*
			 * Write the relocation out.
			 */
			if (do_reloc(rtype, (unsigned char *)roffset,
			    (Word *)&value, name, NAME(lmp)) == 0)
				error = 1;

			DBG_CALL(Dbg_reloc_apply((unsigned long)roffset,
			    value, 0));
		}
	}

	GET_TIME(interval1);

	if (error)
		return (0);

	/*
	 * All objects with BOUND flag set hold definitions for the object
	 * we just relocated.  Call bound_add() to save those references.
	 */
	if (bound) {
		if (bound_add(REF_SYMBOL, lmp, 0) == 0)
			return (0);
	}

	/*
	 * If we write enabled the text segment to perform these relocations
	 * re-protect by disabling writes.
	 */
	if (textrel) {
		(void) elf_set_prot(lmp, 0);
		textrel = 0;
	}

	GET_TIME(interval2);
	SAV_TIME(interval1, "  finished reloc loop - boundto");
	SAV_TIME(interval2, "finished relocation\n");

	return (1);
}

/*
 * Initialize the first few got entries so that function calls go to
 * elf_rtbndr:
 *
 *	GOT[GOT_XLINKMAP] =	the address of the link map
 *	GOT[GOT_XRTLD] =	the address of rtbinder
 */
void
elf_plt_init(unsigned long * got, Rt_map * lmp)
{
	unsigned long *		_got;

	_got = got + M_GOT_XLINKMAP;
	*_got = (unsigned long)lmp;
	_got = got + M_GOT_XRTLD;
	*_got = (unsigned long)elf_rtbndr;
}

/*
 * For SVR4 Intel compatability.  USL uses /usr/lib/libc.so.1 as the run-time
 * linker, so the interpreter's address will differ from /usr/lib/ld.so.1.
 * Further, USL has special _iob[] and _ctype[] processing that makes up for the
 * fact that these arrays do not have associated copy relocations.  So we try
 * and make up for that here.  Any relocations found will be added to the global
 * copy relocation list and will be processed in setup().
 */
static int
_elf_copy_reloc(const char * name, Rt_map * rlmp, Rt_map * dlmp)
{
	Sym *		symref, * symdef;
	caddr_t 	ref, def;
	Rt_map *	_lmp;
	Rel		rel;
	int		error;

	/*
	 * Determine if the special symbol exists as a reference in the dynamic
	 * executable, and that an associated definition exists in libc.so.1.
	 */
	if ((symref = lookup_sym(name, PERMIT(rlmp), rlmp, rlmp, &_lmp,
	    (LKUP_DEFT | LKUP_FIRST))) == 0)
		return (1);
	if ((symdef = lookup_sym(name, PERMIT(dlmp), rlmp, dlmp, &_lmp,
	    (LKUP_DEFT))) == 0)
		return (1);
	if (strcmp(NAME(_lmp), MSG_ORIG(MSG_PTH_LIBC)))
		return (1);

	/*
	 * Determine the reference and definition addresses.
	 */
	ref = (void *)(symref->st_value);
	if (!(FLAGS(rlmp) & FLG_RT_FIXED))
		ref += ADDR(rlmp);
	def = (void *)(symdef->st_value);
	if (!(FLAGS(_lmp) & FLG_RT_FIXED))
		def += ADDR(_lmp);

	/*
	 * Set up a relocation entry for debugging and call the generic copy
	 * relocation function to provide symbol size error checking and to
	 * actually perform the relocation.
	 */
	rel.r_offset = (Addr)ref;
	rel.r_info = (Word)R_386_COPY;
	DBG_CALL(Dbg_reloc_in(M_MACH, M_REL_SHT_TYPE, &rel, name, 0));

	error = elf_copy_reloc((char *)name, symref, rlmp, (void *)ref, symdef,
	    _lmp, (void *)def);

	if (!(FLAGS(_lmp) & FLG_RT_COPYTOOK)) {
		if (list_append(&COPY(rlmp), &COPY(_lmp)) == 0)
			error = 1;
		FLAGS(_lmp) |= FLG_RT_COPYTOOK;
	}
	return (error);
}

int
_elf_copy_gen(Rt_map * lmp)
{
	if (interp && ((unsigned long)interp->i_faddr != r_debug.r_ldbase) &&
	    !(strcmp(interp->i_name, MSG_ORIG(MSG_PTH_LIBC)))) {
		DBG_CALL(Dbg_reloc_run(pr_name, M_REL_SHT_TYPE));
		if (_elf_copy_reloc(MSG_ORIG(MSG_SYM_CTYPE), lmp,
		    (Rt_map *)NEXT(lmp)) == 0)
			return (0);
		if (_elf_copy_reloc(MSG_ORIG(MSG_SYM_IOB), lmp,
		    (Rt_map *)NEXT(lmp)) == 0)
			return (0);
	}
	return (1);
}

#ifdef	DEBUG

/*
 * Plt writing interface to allow debugging initialization to be generic.
 */
void
elf_plt_write(unsigned long * pc, unsigned long * symval)
{
	*pc = (unsigned long)symval;
}

#endif
