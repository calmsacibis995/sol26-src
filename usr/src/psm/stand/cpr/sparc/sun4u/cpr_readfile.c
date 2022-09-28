/*
 * Copyright (c) 1985 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpr_readfile.c	1.14	96/10/15 SMI"

#include <sys/sysmacros.h>
#include <sys/exechdr.h>
#include <sys/elf.h>
#include <sys/elf_notes.h>
#include <sys/bootconf.h>
#include <sys/reboot.h>
#include <sys/fcntl.h>
#include <sys/modctl.h>
#include <sys/link.h>
#include <sys/cpr.h>
#include <sys/cpr_impl.h>

union {
	struct exec X;
	Elf32_Ehdr Elfhdr;
} ex;

#define	x ex.X
#define	elfhdr ex.Elfhdr

typedef int	(*func_t)();

#define	FAIL	((func_t)-1)
#define	ALIGN(x, a)	\
	((a) == 0 ? (int)(x) : (((int)(x) + (a) - 1) & ~((a) - 1)))
#ifndef	MIN
#define	MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

extern int read(int, caddr_t, int);

void cpr_bzero(register char *, register int);
static caddr_t loadbase = 0;

int use_align = 1;
int npagesize = 0;

/*
 * Read in a Unix executable file and return its entry point.
 * Handle the various a.out formats correctly.
 * "fd" is the standalone file descriptor to read from.
 * Print informative little messages if "verbosemode" is on.
 * Returns -1 for errors.
 */

static func_t 	readelf(int, int, Elf32_Ehdr *);

caddr_t
getloadbase()
{
	return (loadbase);
}

func_t
readfile(int fd, int verbosemode)
{
	register int i;
	register int shared = 0;

	if (verbosemode)
		errp("fd = %x\n", fd);

	i = read(fd, (char *)&elfhdr, sizeof (elfhdr));
	if (x.a_magic == ZMAGIC || x.a_magic == NMAGIC)
		shared = 1;
	if (i != sizeof (elfhdr)) {
		errp("Error reading ELF header.\n");
		return (FAIL);
	}

	if (!shared && x.a_magic != OMAGIC) {
	    if (*(int *)(((Elf32_Ehdr *)&x)->e_ident) == *(int *)(ELFMAG)) {
		if (verbosemode) {
			errp("calling readelf, elfheader is:\n");
			errp("e_ident\t0x%x, 0x%x, 0x%x, 0x%x\n",
			    *(int *)&elfhdr.e_ident[0],
			    *(int *)&elfhdr.e_ident[4],
			    *(int *)&elfhdr.e_ident[8],
			    *(int *)&elfhdr.e_ident[12]);
			errp("e_machine\t0x%x\n", elfhdr.e_machine);
			errp("e_entry\t0x%x\n", elfhdr.e_entry);
			errp("e_shoff\t0x%x\n", elfhdr.e_shoff);
			errp("e_shnentsize\t0x%x\n", elfhdr.e_shentsize);
			errp("e_shnum\t0x%x\n", elfhdr.e_shnum);
			errp("e_shstrndx\t0x%x\n", elfhdr.e_shstrndx);
		}
			return (readelf(fd, verbosemode, &elfhdr));
	    } else {
		DEBUG4(errp("File not executable.\n"));
		return (FAIL);
	    }
	}
	return (FAIL);
}

static func_t
readelf(int fd, int verbosemode, Elf32_Ehdr *elfhdrp)
{

	Elf32_Phdr *phdr;	/* program header */
	Elf32_Nhdr *nhdr;	/* note header */
	int nphdrs, phdrsize;
	caddr_t allphdrs;
	caddr_t	namep, descp;
	u_int loadaddr, size, base;
	int off, i;
	func_t entrypt;				/* entry point of standalone */
	int m_alloc, left_to_do, rsize, rc;

	if (elfhdrp->e_phnum == 0 || elfhdrp->e_phoff == 0)
		goto elferror;

	entrypt = (func_t)elfhdrp->e_entry;
	loadbase = (caddr_t) elfhdrp->e_entry;
	if (verbosemode)
		errp("Entry point: 0x%x\n", entrypt);

	/*
	 * Allocate and read in all the program headers.
	 */
	allphdrs = NULL;
	nhdr = NULL;
	nphdrs = elfhdrp->e_phnum;
	phdrsize = nphdrs * elfhdrp->e_phentsize;
	allphdrs = (caddr_t)prom_alloc((caddr_t)allphdrs, phdrsize, 1);
	if (allphdrs == NULL)
		goto elferror;
	if (verbosemode)
		errp("lseek: args = %x %x %x\n", fd, elfhdrp->e_phoff, 0);
	if (prom_seek(fd, elfhdrp->e_phoff) == -1)
		goto elferror;
	if (read(fd, allphdrs, phdrsize) != phdrsize)
		goto elferror;

	/*
	 * First look for PT_NOTE headers that tell us what pagesize to
	 * use in allocating program memory.
	 */
	npagesize = 0;

	for (i = 0; i < nphdrs; i++) {
		phdr = (Elf32_Phdr *)(allphdrs + elfhdrp->e_phentsize * i);
		if (phdr->p_type != PT_NOTE)
			continue;

		if (verbosemode) {
			errp("allocating %d bytes for note hdr\n",
				phdr->p_filesz);
		}
		nhdr = (Elf32_Nhdr *)prom_alloc((caddr_t)nhdr,
					phdr->p_filesz, 1);
		if (nhdr == NULL)
			goto elferror;
		if (verbosemode)
			errp("seeking to 0x%x\n", phdr->p_offset);
		if (prom_seek(fd, phdr->p_offset) == -1)
			goto elferror;
		if (verbosemode) {
			errp("reading %d bytes into 0x%x\n",
				phdr->p_filesz, nhdr);
		}
		if (read(fd, (caddr_t)nhdr, phdr->p_filesz) != phdr->p_filesz)
			goto elferror;
		namep = (caddr_t)(nhdr + 1);
		if (verbosemode) {
			errp("p_note namesz %x descsz %x type %x\n",
				nhdr->n_namesz, nhdr->n_descsz, nhdr->n_type);
		}
		if (nhdr->n_namesz == strlen(ELF_NOTE_SOLARIS) + 1 &&
		    strcmp(namep, ELF_NOTE_SOLARIS) == 0 &&
		    nhdr->n_type == ELF_NOTE_PAGESIZE_HINT) {
			descp = namep + roundup(nhdr->n_namesz, 4);
			npagesize = *(int *)descp;
			if (verbosemode)
				errp("pagesize is 0x%x\n", npagesize);
		}
		prom_free((caddr_t)nhdr, phdr->p_filesz);
		nhdr = NULL;
	}

	/*
	 * Next look for PT_LOAD headers to read in.
	 */
	DEBUG4(errp("readelf: %d PT_LOAD headers\n", nphdrs));

	for (i = 0; i < nphdrs; i++) {
		phdr = (Elf32_Phdr *)(allphdrs + elfhdrp->e_phentsize * i);
		if (verbosemode) {
			errp("Doing header %d\n", i);
			errp("phdr\n");
			errp("\tp_offset = 0x%x, p_vaddr = 0x%x\n",
				phdr->p_offset, phdr->p_vaddr);
			errp("\tp_memsz = %d, p_filesz = %d\n",
				phdr->p_memsz, phdr->p_filesz);
		}
		if (phdr->p_type == PT_LOAD) {
			if (verbosemode)
				errp("seeking to 0x%x\n", phdr->p_offset);

			if (prom_seek(fd, phdr->p_offset) == -1)
				goto elferror;

			/*
			 * If we found a new pagesize above, use it to adjust
			 * the memory allocation.
			 * Otherwise, use MMU_PAGESIZE to do alignment.
			 */
			if (npagesize == 0)
				npagesize = MMU_PAGESIZE;
			loadaddr = phdr->p_vaddr;
			if (use_align && npagesize != 0) {
				off = loadaddr & (npagesize - 1);
				size = roundup(phdr->p_memsz + off, npagesize);
				base = loadaddr - off;
			} else {
				npagesize = 0;
				size = phdr->p_memsz;
				base = loadaddr;
			}

			if (verbosemode)
				errp("allocating memory: base 0x%x size %d\n",
					base, size);
			/*
			 * Allocate memory, 1 page at a time, so that it
			 * does not rely on consecutive memory.
			 */
			loadbase = (caddr_t) MIN((u_int)loadbase, base);
			m_alloc = 0;
			while (m_alloc < size) {
				if (prom_alloc((caddr_t)base,
					MMU_PAGESIZE, 1) == NULL)
					goto elferror;

				base += MMU_PAGESIZE;
				m_alloc += MMU_PAGESIZE;
			}

			/*
			 * Now read in text
			 */
			left_to_do = phdr->p_filesz;
			base = loadaddr;
			rsize = 0;
			while (left_to_do) {
				if (left_to_do > MMU_PAGESIZE)
					rsize = MMU_PAGESIZE;
				else
					rsize = left_to_do;

				if ((rc = read(fd, (char *)base, rsize))
						!= rsize) {
					DEBUG4(errp("readelf: read to base %x ",
						base));
					DEBUG4(errp("failed rc %d\n", rc));
					goto elferror;
				}
				left_to_do -= rsize;

				if (verbosemode) {
					errp("read %d bytes into 0x%x\n",
						rsize, base);
					errp("remaining to read %d\n",
						left_to_do);
				}
				base += MMU_PAGESIZE;
			}

			/* zero out BSS */
			if (phdr->p_memsz > phdr->p_filesz) {
				loadaddr += phdr->p_filesz;
				base = loadaddr;
				size = phdr->p_memsz - phdr->p_filesz;
				rsize = 0;
				if (verbosemode)
					errp("bss from 0x%x size %d\n",
					    loadaddr, size);
				left_to_do = size;
				while (left_to_do) {
					if (left_to_do > MMU_PAGESIZE)
						rsize = MMU_PAGESIZE;
					else
						rsize = left_to_do;

					cpr_bzero((char *)base, rsize);
					left_to_do -= rsize;
					if (verbosemode) {
						errp("bsero %d bytes from %x\n",
							rsize, base);
						errp("%d bytes left\n",
							left_to_do);
					}
					base += MMU_PAGESIZE;
				}
			}

		}
	}

	prom_free((caddr_t)allphdrs, phdrsize);

	return (entrypt);
elferror:
	if (allphdrs != NULL)
		prom_free((caddr_t)allphdrs, phdrsize);

	if (nhdr != NULL)
		prom_free((caddr_t)nhdr, phdr->p_filesz);

	errp("Elf read error.\n");
	return (FAIL);
}

void
cpr_bzero(register char *p, register int n)
{
	register char zeero = 0;

	while (n > 0)
		*p++ = zeero, n--;
}
