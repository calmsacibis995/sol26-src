/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)ppc_elf.c	1.53	96/10/23 SMI"

/*
 * PPC machine dependent and ELF file class dependent functions.
 * Contains routines for performing function binding and symbol relocations.
 */
#include	"_synonyms.h"

#include	<stdio.h>
#include	<sys/elf.h>
#include	<sys/elf_ppc.h>
#include	<sys/mman.h>
#include	<signal.h>
#include	<dlfcn.h>
#include	<unistd.h>
#include	<synch.h>
#include	"_rtld.h"
#include	"_elf.h"
#include	"msg.h"
#include	"profile.h"
#include	"debug.h"
#include	"reloc.h"
#include	"conv.h"

#define	ABS(a) ((a) >= 0 ? (a) : -(a))

void _dcbst(unsigned long *);
void _sync(void);
void _icbi(unsigned long *);
void _isync(void);

#if defined(PROF)

/*
 * elf_plt_cg_write:
 *	Cause a PLT entry to become a call-graph capture point.
 *	If the pre-allocated dynamic PLT hasn't been initialized
 *	do it now, by putting the symbol's index into %r11 and causing
 *	a branch to elf_plt_cg() which will save everything and call
 *	the call graph interpreter.
 *	Regardless, cause the "real" PLT to point at the dynamic PLT.
 */
static void
elf_plt_cg_write(unsigned long *pc, int symndx, Rt_map *lmp)
{
	extern void elf_plt_cg(int ndx, caddr_t from);
	unsigned long *dyn_plt;

	/* LINTED */
	dyn_plt = (unsigned long *)
			((caddr_t)DYNPLT(lmp) + (symndx * M_DYN_PLT_ENT));

	if (*dyn_plt == 0) {
		/* Only works for the first 64k PLT entries */
		dyn_plt[0] = M_LIR11 | (symndx & 0xffff);
		dyn_plt[1] = M_LISR12 | (((unsigned long) elf_plt_cg) >> 16);
		dyn_plt[2] = M_ORIR12R12 |
					(((unsigned long) elf_plt_cg) & 0xffff);
		dyn_plt[3] = M_MTCTRR12;
		dyn_plt[4] = M_BCTR;
		/* XXXPPC: should use data cache/instr cache block sizes */
		_dcbst(&dyn_plt[0]);	/* initiate write to main memory */
		_dcbst(&dyn_plt[4]);	/* initiate write to main memory */
		_sync();		/* ensure store seen by all MPs */
		_icbi(&dyn_plt[0]);	/* if in instruction cache, toss it */
		_icbi(&dyn_plt[4]);	/* if in instruction cache, toss it */
		_isync();		/* run in new context */
	}
	elf_plt_write(pc, dyn_plt, pc, lmp);
}

#endif /* defined(PROF) */

/*
 * Function binding routine - invoked on the first call to a function through
 * the procedure linkage table;
 * passes first through an assembly language interface.
 *
 * Takes the address of the PLT entry where the call originated,
 * the offset into the relocation table of the associated
 * relocation entry and the address of the link map (rt_private_map struct)
 * for the entry.
 *
 * Returns the address of the function referenced after re-writing the PLT
 * entry to invoke the function directly.
 *
 * On error, causes process to terminate with a signal.
 */

unsigned long
elf_bndr(unsigned long reloc, Rt_map * lmp, caddr_t from)
{
	Rt_map *	nlmp;
	unsigned long	pltadd, symval;
	char *		name;
	Rel *		rptr;
	Sym *		sym, * nsym;
	int		pltndx;		/* array index into .pltaddrs[] */
	int		bind;

	PRF_MCOUNT(1, elf_bndr);
	if ((bind = bind_guard(THR_FLG_BIND)) == 1)
		(void) rw_rdlock(&bindlock);

	/*
	 * reloc is M_PLT_INSSIZE * plt# (not including the reserved plt slots)
	 * pltadd is the address of the plt we need to patch.
	 */
	pltndx = reloc / M_PLT_INSSIZE;
	pltadd = (unsigned long) PLTGOT(lmp) +
			(pltndx + M_PLT_XNumber) * M_PLT_ENTSIZE;

	/*
	 * Perform some basic sanity checks.  If we didn't get a load map
	 * or the plt offset is invalid then its possible someone has walked
	 * over the plt entries.
	 */
	if (!lmp || ((reloc % M_PLT_INSSIZE) != 0)) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_REL_PLTENTRY),
		    conv_reloc_PPC_type_str(R_PPC_JMP_SLOT), pltndx, from,
		    (!lmp ? MSG_INTL(MSG_REL_PLTREF) :
		    MSG_INTL(MSG_REL_PLTOFF)));
		exit(1);
	}
	reloc = pltndx * sizeof (Rel);

	/*
	 * Use relocation entry to get symbol table entry and symbol name.
	 */
	rptr = (Rel *)((unsigned long) JMPREL(lmp) + reloc);
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
	DBG_CALL(Dbg_bind_global(NAME(lmp), from, from - ADDR(lmp), pltndx,
	    NAME(nlmp), (caddr_t)symval, (caddr_t)nsym->st_value, name));

	if (!(rtld_flags & RT_FL_NOBIND)) {
#ifdef	PROF
		if (FLAGS(nlmp) & FLG_RT_PROFILE) {
			int	symndx = (((int)nsym -
				    (int)SYMTAB(nlmp)) / SYMENT(nlmp));

			/*
			 * Write PLT entry to jump to profile counter
			 */
			elf_plt_cg_write((unsigned long *)pltadd, symndx, nlmp);

			/*
			 * Call the call graph interpretor to initialize this
			 * first call.
			 */
			(void) plt_cg_interp(symndx, (caddr_t)from,
			    (caddr_t)symval);
		} else {
#endif
			/*
			 * Write standard PLT entry to jump directly
			 * to newly bound function.
			 */
			elf_plt_write((unsigned long *)pltadd,
			    (unsigned long *)symval, (unsigned long *)pltadd,
			    lmp);
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
	unsigned long	pltbgn, pltend;
	unsigned long	roffset, rsymndx, psymndx = 0, etext;
	unsigned int	rtype;
	long		reladd;
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
	 * procedure linkage table entry to go to elf_rtbndr().  dbx(1) seems
	 * to find this useful.
	 */
	if ((plt == 0) && PLTGOT(lmp))
		elf_plt_init((unsigned long *)PLTGOT(lmp), lmp);

	/*
	 * Find the GOT entry with the blrl instruction (@ GOT - 4)
	 * and mprotect that page to be executable.
	 */
	if ((symdef = elf_find_sym(MSG_ORIG(MSG_SYM_GOT), lmp, &_lmp, 0,
	    elf_hash(MSG_ORIG(MSG_SYM_GOT)))) != 0) {
		value = symdef->st_value;
		/* If shared object add base address */
		if (!(FLAGS(lmp) & FLG_RT_FIXED))
			value += ADDR(lmp);
		/* Backup to &GOT[-1] */
		value -= M_GOT_ENTSIZE;
		/* Now round down to the page in which &GOT[-1] lies */
		pvalue = value & ~(syspagsz - 1);
		/* and finally protect it as executable */
		(void) mprotect((caddr_t)pvalue, syspagsz,
		    (PROT_READ | PROT_WRITE | PROT_EXEC));
		/* Finally force the one word from the data cache */
		_dcbst((unsigned long *) value);
		_sync();
		_icbi((unsigned long *) value);
		_isync();
	}

	/*
	 * Initialize the plt start and end addresses.
	 */
	if ((pltbgn = (unsigned long)JMPREL(lmp)) != 0)
		pltend = pltbgn + (unsigned long)(PLTRELSZ(lmp));

	/*
	 * If we've been called upon to promote an RTLD_LAZY object to an
	 * RTLD_NOW then we're only interested in scaning the .plt table.
	 */
	if (plt) {
		relbgn = pltbgn;
		relend = pltend;
	} else {
		/*
		 * The relocation sections appear to the run-time linker as a
		 * single table.  Determine the address of the beginning and end
		 * of this table.  There are two different interpretations of
		 * the ABI at this point:
		 *
		 *   o  The REL table and its associated RELSZ indicate the
		 *	concatenation of *all* relocation sections (this is the
		 *	model our link-editor constructs).
		 *
		 *   o  The REL table and its associated RELSZ indicate the
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

	relsiz = (unsigned long)(RELENT(lmp));
	basebgn = ADDR(lmp);
	etext = ETEXT(lmp);

	DBG_CALL(Dbg_reloc_run(NAME(lmp), M_REL_SHT_TYPE));

	/*
	 * If we're processing in lazy mode there is no need to scan the
	 * .rela.plt table.
	 */
	if (pltbgn && !(MODE(lmp) & RTLD_NOW))
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
		reladd = (long)(((Rel *)relbgn)->r_addend);
		roffset = ((Rel *)relbgn)->r_offset;
		rel = (Rel *)relbgn;
		relbgn += relsiz;

		/*
		 * Optimizations.
		 */
		if (rtype == R_PPC_NONE)
			continue;
		if (noplt && ((unsigned long)rel >= pltbgn) &&
		    ((unsigned long)rel < pltend))
			continue;

		/*
		 * If this is a shared object, add the base address to offset.
		 */
		if (!(FLAGS(lmp) & FLG_RT_FIXED))
			roffset += basebgn;

		/*
		 * If we're promoting plts determine if this one has already
		 * been written.
		 */
		if (plt) {
			if ((*(unsigned long *)roffset & (~(S_MASK(16)))) !=
			    M_LIR11)
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
				    (rtype != R_PPC_COPY)) {
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
					if (rtype == R_PPC_COPY)
						_lmp = (Rt_map *)NEXT(_lmp);

					if (rtype == R_PPC_JMP_SLOT)
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
					if (rtype != R_PPC_COPY) {
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
		case R_PPC_COPY:
			if (elf_copy_reloc(name, symref, lmp, (void *)roffset,
			    symdef, _lmp, (const void *)value) == 0)
				error = 1;
			if (!(FLAGS(_lmp) & FLG_RT_COPYTOOK)) {
				if (list_append(&COPY(lmp), &COPY(_lmp)) == 0)
					error = 1;
				FLAGS(_lmp) |= FLG_RT_COPYTOOK;
			}
			break;
		case R_PPC_JMP_SLOT:
			value += reladd;
#ifdef	PROF
			if (FLAGS(_lmp) & FLG_RT_PROFILE) {
				int	symndx = (((int)symdef -
					    (int)SYMTAB(_lmp)) / SYMENT(_lmp));

				/*
				 * Write PLT entry to jump to profile counter
				 */
				elf_plt_cg_write((unsigned long *)roffset,
				    symndx, _lmp);

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
				elf_plt_write((unsigned long *)roffset,
				    (unsigned long *)value,
				    (unsigned long *)roffset, lmp);
#ifdef	PROF
			}
#endif
			break;
		default:
			value += reladd;
			/*
			 * Write the relocation out.
			 */
			if (do_reloc(rtype, (unsigned char *)roffset,
			    (Word *)&value, name, NAME(lmp)) == 0)
				error = 1;

			/*
			 * Value now contains the 'bit-shifted' value
			 * that was or'ed into memory (this was set
			 * by do_reloc()).
			 */
			DBG_CALL(Dbg_reloc_apply((unsigned long)roffset,
			    value, 0));

			/*
			 * If this relocation is against a text segment
			 * we must make sure that the instruction
			 * cache is flushed.
			 */
			if (textrel) {
				/*
				 * initiate write to main memory
				 */
				_dcbst((unsigned long *)roffset);
				/*
				 * make sure write is seen by all processors
				 */
				_sync();
				/*
				 * if already in instruction cache, toss it
				 */
				_icbi((unsigned long *)roffset);
			}
		}
	}

	GET_TIME(interval1);

	/*
	 * make sure everything above is completed
	 */
	_isync();

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

void
elf_plt_init(unsigned long *plt, Rt_map *lmp)
{
	extern void elf_rtbndr();
	unsigned long nplts, pltndx, *plttable, *pltp;

	plt[0] = M_LISR12 | (((unsigned long) elf_rtbndr) >> 16);
	plt[1] = M_ORIR12R12 | (((unsigned long) elf_rtbndr) & 0xffff);
	plt[2] = M_MTCTRR12;
	plt[3] = M_LISR12 | (((unsigned long) lmp) >> 16);
	plt[4] = M_ORIR12R12 | (((unsigned long) lmp) & 0xffff);
	plt[5] = M_BCTR;
	nplts = (unsigned long)PLTRELSZ(lmp) / sizeof (Rel);
	{
		caddr_t start_addr;
		size_t plt_bytes;

		/* Get the start of the plt rounded down to a page boundary */
		start_addr = (caddr_t) ((unsigned long) plt & ~(syspagsz - 1));
		/* Now find the number of bytes within the PLT */
		plt_bytes = (nplts + M_PLT_XNumber) * M_PLT_ENTSIZE;
		/* and adjust for the round down above */
		plt_bytes += (caddr_t) plt - start_addr;
		/* and make the PLT executable */
		mprotect(start_addr, plt_bytes,
			PROT_READ | PROT_WRITE | PROT_EXEC);
	}
	/* Find the address of the .PLTtable */
	plttable = &plt[(M_PLT_XNumber + nplts)
			* (M_PLT_ENTSIZE / M_PLT_INSSIZE)];
	plt[6] = M_ADDISR11R11 | (((unsigned long) plttable) >> 16);
	if ((unsigned long) plttable & 0x8000)
		plt[6]++;
	plt[7] = M_LWZR11R11 | (((unsigned long) plttable) & 0xffff);
	plt[8] = M_MTCTRR11;
	plt[9] = M_BCTR;
	/* Start the data cache stores now */
	for (pltndx = 0; pltndx <= 9; ++pltndx)
		_dcbst(&plt[pltndx]);
	/*
	 * Build nplts entries starting at
	 * plt[M_PLT_XNumber * (M_PLT_ENTSIZE / M_PLT_INSSIZE)]
	 */
	pltp = &plt[M_PLT_XNumber * (M_PLT_ENTSIZE / M_PLT_INSSIZE)];
	pltndx = 0;
	while (nplts-- != 0) {
		if (pltndx < M_PLT_MAXSMALL) {
			if (pltp[0] == 0) {
				pltp[0] = M_LIR11 | pltndx * 4;
				pltp[1] = M_B | (S_MASK(26) &
				    ((caddr_t)plt - (caddr_t)&pltp[1]));
				_dcbst(&pltp[0]);
				_dcbst(&pltp[1]);
			}
			++pltndx;
			pltp += 2;
		} else {
			if (pltp[0] == 0) {
				pltp[0] = M_LISR11 | ((pltndx * 4) >> 16);
				pltp[1] = M_ORIR11R11 | ((pltndx * 4) & 0xffff);
				pltp[2] = M_B | (S_MASK(26) &
				    ((caddr_t)plt - (caddr_t)&pltp[2]));
				_dcbst(&pltp[0]);
				_dcbst(&pltp[1]);
				_dcbst(&pltp[2]);
			}
			pltndx += 2;
			pltp += 4;
		}
	}
	/*
	 * Need to make sure that these newly written instruction are seen
	 * by memory (so they aren't only in the data cache and therefore
	 * invisible to the instruction cache).  Further we want other
	 * processors in an MP system to be able to see them as well.
	 */
	_sync();		/* make sure writes are visible everywhere */
	_isync();		/* make sure everything above is completed */
}

/*
 * To allow for dldump() to update a .plt in a memory image (other than the
 * actual mapped object) both the real .plt offset and its virtual offset are
 * required.
 *
 * XXXXXX
 * long plts are not being updated correctly.  This was documented in bug
 * id 4007146 - which, since PPC shutdown, has been closed as `will not fix'.
 * XXXXXX
 */
void
elf_plt_write(unsigned long * rpltaddr, unsigned long * symaddr,
    unsigned long * vpltaddr, Rt_map * lmp)
{
	register unsigned long	offset, pltndx, nplts;

	/*
	 * Can we reach with a bl instruction directly?
	 */
	if (ABS(vpltaddr - symaddr) < (1 << 23)) {
		*rpltaddr = M_B | (S_MASK(26) &
		    ((caddr_t)symaddr - (caddr_t)vpltaddr));
	} else {
		/*
		 * Must use long method - find out which PLT we need to update
		 */
		offset = (unsigned long)vpltaddr - (unsigned long)PLTGOT(lmp);
		pltndx = offset / M_PLT_ENTSIZE - M_PLT_XNumber;

		/*
		 * Determine which plt address slot to patch
		 */
		nplts = PLTRELSZ(lmp) / sizeof (Rel);
		offset = (unsigned long)PLTGOT(lmp);
		offset += (nplts + M_PLT_XNumber) * M_PLT_ENTSIZE;
		offset += sizeof (void *) * pltndx;

		/*
		 * patch the address *before* re-writing branch instruction
		 */
		*(unsigned long *)offset = (unsigned long)symaddr;

		/*
		 * now re-write branch instruction
		 */
		offset = (unsigned long)PLTGOT(lmp)
				+ M_PLT_XCALL * M_PLT_ENTSIZE;
		++rpltaddr;
		*rpltaddr = M_B | (S_MASK(26) &
		    (offset - (unsigned long)vpltaddr));
	}

	/*
	 * Need to make sure that this newly written instruction is seen
	 * by memory (so they aren't only in the data cache and therefore
	 * invisible to the instruction cache).  Further we want other
	 * processors in an MP system to be able to see them as well.
	 */
	_dcbst(rpltaddr);	/* initiate write to main memory */
	_sync();		/* make sure write is seen by all processors */
	_icbi(rpltaddr);	/* if already in instruction cache, toss it */
	_isync();		/* make sure everything above is completed */
}
