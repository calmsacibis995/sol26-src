/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)kobj_boot.c	1.10	96/07/19 SMI"

/*
 * Bootstrap the linker/loader.
 */

#include <sys/types.h>
#include <sys/bootconf.h>
#include <sys/link.h>
#include <sys/auxv.h>
#include <sys/kobj.h>
#include <sys/elf.h>
#include <sys/kobj_impl.h>

#define	MASK(n)		((1<<(n))-1)
#define	IN_RANGE(v, n)	((-(1<<((n)-1))) <= (v) && (v) < (1<<((n)-1)))
#define	ALIGN(x, a)	((a) == 0 ? (int)(x) : \
			(((int)(x) + (a) - 1) & ~((a) - 1)))

#define	roundup		ALIGN

/*
 * Boot transfers control here. At this point,
 * we haven't relocated our own symbols, so the
 * world (as we know it) is pretty small right now.
 */
void
_kobj_boot(cif, dvec, bootops, ebp)
	int (**cif)(void *);
	void *dvec;
	struct bootops *bootops;
	Elf32_Boot *ebp;
{
	Elf32_Shdr *section[24];	/* cache */
	val_t bootaux[BA_NUM];
	struct bootops *bop;
	Elf32_Phdr *phdr;
	auxv_t *auxv = NULL;
	u_int sh, sh_num, sh_size;
	u_int end, edata = 0;
	int i;
	extern void kobj_sync_one_instruction(unsigned long *);

	bop = *(struct bootops **)bootops;

	for (i = 0; i < BA_NUM; i++)
		bootaux[i].ba_val = NULL;
	/*
	 * Check the bootstrap vector.
	 */
	for (; ebp->eb_tag != EB_NULL; ebp++) {
		switch (ebp->eb_tag) {
		case EB_AUXV:
			auxv = (auxv_t *)ebp->eb_un.eb_ptr;
			break;
		case EB_DYNAMIC:
			bootaux[BA_DYNAMIC].ba_ptr = (void *)ebp->eb_un.eb_ptr;
			break;
		}
	}
	if (auxv == NULL)
		return;
	/*
	 * Now the aux vector.
	 */
	for (; auxv->a_type != AT_NULL; auxv++) {
		switch (auxv->a_type) {
		case AT_PHDR:
			bootaux[BA_PHDR].ba_ptr = auxv->a_un.a_ptr;
			break;
		case AT_PHENT:
			bootaux[BA_PHENT].ba_val = auxv->a_un.a_val;
			break;
		case AT_PHNUM:
			bootaux[BA_PHNUM].ba_val = auxv->a_un.a_val;
			break;
		case AT_PAGESZ:
			bootaux[BA_PAGESZ].ba_val = auxv->a_un.a_val;
			break;
		case AT_SUN_LDELF:
			bootaux[BA_LDELF].ba_ptr = auxv->a_un.a_ptr;
			break;
		case AT_SUN_LDSHDR:
			bootaux[BA_LDSHDR].ba_ptr = auxv->a_un.a_ptr;
			break;
		case AT_SUN_LDNAME:
			bootaux[BA_LDNAME].ba_ptr = auxv->a_un.a_ptr;
			break;
		case AT_SUN_LPAGESZ:
			bootaux[BA_LPAGESZ].ba_val = auxv->a_un.a_val;
			break;
		case AT_ENTRY:
			bootaux[BA_ENTRY].ba_ptr = auxv->a_un.a_ptr;
			break;
		}
	}

	sh = (u_int)bootaux[BA_LDSHDR].ba_ptr;
	sh_num = ((Elf32_Ehdr *)bootaux[BA_LDELF].ba_ptr)->e_shnum;
	sh_size = ((Elf32_Ehdr *)bootaux[BA_LDELF].ba_ptr)->e_shentsize;
	/*
	 * Build cache table for section addresses.
	 */
	for (i = 0; i < sh_num; i++) {
		section[i] = (Elf32_Shdr *)sh;
		sh += sh_size;
	}
	/*
	 * Find the end of data
	 * (to allocate bss)
	 */
	phdr = (Elf32_Phdr *)bootaux[BA_PHDR].ba_ptr;
	for (i = 0; i < bootaux[BA_PHNUM].ba_val; i++) {
		if (phdr->p_type == PT_LOAD &&
		    (phdr->p_flags & PF_W) && (phdr->p_flags & PF_X)) {
			edata = end = phdr->p_vaddr + phdr->p_memsz;
			break;
		}
		phdr = (Elf32_Phdr *)((u_int)phdr + bootaux[BA_PHENT].ba_val);
	}
	if (edata == NULL)
		return;

	/*
	 * Find the symbol table, and then loop
	 * through the symbols adjusting their
	 * values to reflect where the sections
	 * were loaded.
	 */
	for (i = 1; i < sh_num; i++) {
		Elf32_Shdr *shp;
		Elf32_Sym *sp;
		u_int off;

		shp = section[i];
		if (shp->sh_type != SHT_SYMTAB)
			continue;

		for (off = 0; off < shp->sh_size; off += shp->sh_entsize) {
			sp = (Elf32_Sym *)(shp->sh_addr + off);

			if (sp->st_shndx == SHN_ABS ||
			    sp->st_shndx == SHN_UNDEF)
				continue;
			/*
			 * Assign the addresses for COMMON
			 * symbols even though we haven't
			 * actually allocated bss yet.
			 */
			if (sp->st_shndx == SHN_COMMON) {
				end = ALIGN(end, sp->st_value);
				sp->st_value = end;
				/*
				 * Squirrel it away for later.
				 */
				if (bootaux[BA_BSS].ba_val == 0)
					bootaux[BA_BSS].ba_val = end;
				end += sp->st_size;
				continue;
			} else if (sp->st_shndx > (Elf32_Half)sh_num)
				return;

			/*
			 * Symbol's new address.
			 */
			sp->st_value += section[sp->st_shndx]->sh_addr;
		}
	}
	/*
	 * Allocate bss for COMMON, if any.
	 */
	if (end > edata) {
		unsigned int va, bva;
		unsigned int asize;
		unsigned int align;

		if (bootaux[BA_LPAGESZ].ba_val) {
			asize = bootaux[BA_LPAGESZ].ba_val;
			align = bootaux[BA_LPAGESZ].ba_val;
		} else {
			asize = bootaux[BA_PAGESZ].ba_val;
			align = BO_NO_ALIGN;
		}
		va = roundup(edata, asize);
		bva = roundup(end, asize);

		if (bva > va) {
			bva = (unsigned int)BOP_ALLOC(bop, (caddr_t)va,
				bva - va, align);
			if (bva == NULL)
				return;
		}
		/*
		 * Zero it.
		 */
		for (va = edata; va < end; va++)
			*(char *)va = 0;
		/*
		 * Update the size of data.
		 */
		phdr->p_memsz += (end - edata);
	}
	/*
	 * Relocate our own symbols.  We'll handle the
	 * undefined symbols later.
	 */
	for (i = 1; i < sh_num; i++) {
		Elf32_Shdr *rshp, *shp, *ssp;
		unsigned long baseaddr, reladdr, rend;
		int relocsize;

		rshp = section[i];

		if (rshp->sh_type != SHT_RELA)
			continue;
		/*
		 * Get the section being relocated
		 * and the symbol table.
		 */
		shp = section[rshp->sh_info];
		ssp = section[rshp->sh_link];

		reladdr = rshp->sh_addr;
		baseaddr = shp->sh_addr;
		rend = reladdr + rshp->sh_size;
		relocsize = rshp->sh_entsize;
		/*
		 * Loop through relocations.
		 */
		while (reladdr < rend) {
			Elf32_Sym *symref;
			Elf32_Rela *reloc;
			register unsigned long stndx;
			unsigned long off, *offptr;
			long addend, value;
			int rtype;

			reloc = (Elf32_Rela *)reladdr;
			off = reloc->r_offset;
			addend = (long)reloc->r_addend;
			rtype = ELF32_R_TYPE(reloc->r_info);
			stndx = ELF32_R_SYM(reloc->r_info);

			reladdr += relocsize;

			if (rtype == R_PPC_NONE) {
				continue;
			}
			off += baseaddr;
			/*
			 * if R_PPC_RELATIVE, simply add base addr
			 * to reloc location
			 */
			if (rtype == R_PPC_RELATIVE) {
				value = baseaddr;
			} else {
				register unsigned int symoff, symsize;

				symsize = ssp->sh_entsize;

				for (symoff = 0; stndx; stndx--)
					symoff += symsize;
				symref = (Elf32_Sym *)(ssp->sh_addr + symoff);

				/*
				 * Check for bad symbol index.
				 */
				if (symoff > ssp->sh_size)
					return;
				/*
				 * Just bind our own symbols at this point.
				 */
				if (symref->st_shndx == SHN_UNDEF) {
					continue;
				}

				value = symref->st_value;
				if (ELF32_ST_BIND(symref->st_info) !=
				    STB_LOCAL) {
					/*
					 * If PC-relative, subtract ref addr.
					 */
					if (rtype == R_PPC_REL24 ||
					    rtype == R_PPC_REL14 ||
					    rtype == R_PPC_REL14_BRTAKEN ||
					    rtype == R_PPC_REL14_BRNTAKEN ||
					    rtype == R_PPC_ADDR30 ||
					    rtype == R_PPC_PLT24)
						value -= off;
				}
			}
			offptr = (unsigned long *)off;
			/*
			 * insert value calculated at reference point
			 * 3 cases - normal byte order aligned, normal byte
			 * order unaligned, and byte swapped
			 * for the swapped and unaligned cases we insert value
			 * a byte at a time
			 */
			switch (rtype) {
			case R_PPC_ADDR32:
			case R_PPC_GLOB_DAT:
			case R_PPC_RELATIVE:
				*offptr = value + addend;
				break;
			case R_PPC_ADDR24:
			case R_PPC_REL24:
			case R_PPC_PLT24:
				value += addend;
				value >>= 2;
				if (IN_RANGE(value, 24)) {
					value &= MASK(24);
					value <<= 2;
				} else {
					return;
				}
				*offptr |= value;
				break;
			case R_PPC_ADDR16:
				value += addend;
				if (IN_RANGE(value, 16))
					*offptr |= value;
				else {
					return;
				}
				break;
			case R_PPC_ADDR16_LO:
				value += addend;
				value &= MASK(16);
				*offptr |= value;
				break;
			case R_PPC_ADDR16_HI:
				value += addend;
				value >>= 16;
				value &= MASK(16);
				*offptr |= value;
				break;
			case R_PPC_ADDR16_HA:
				value += addend;
				if (value & 0x8000) {
					value >>= 16;
					++value;
				}
				else
					value >>= 16;
				value &= MASK(16);
				*offptr |= value;
				break;
			case R_PPC_ADDR14:
			case R_PPC_ADDR14_BRTAKEN:
			case R_PPC_ADDR14_BRNTAKEN:
			case R_PPC_REL14:
			case R_PPC_REL14_BRTAKEN:
			case R_PPC_REL14_BRNTAKEN:
				value += addend;
				if (IN_RANGE(value, 16)) {
					value &= ~3;
					*offptr |= value;
				} else {
					return;
				}
				break;
			case R_PPC_ADDR30:
				value += addend;
				value &= ~3;
				*offptr |= value;
				break;
			default:
				return;
			}

			/* force instruction to be visible to icache */
			kobj_sync_one_instruction(offptr);

			/*
			 * We only need to do it once.
			 */
			reloc->r_info = ELF32_R_INFO(stndx, R_PPC_NONE);
		} /* while */
	}
	/*
	 * Done relocating all of our *defined*
	 * symbols, so we hand off.
	 */
	kobj_init(cif, dvec, bootops, bootaux);
}
