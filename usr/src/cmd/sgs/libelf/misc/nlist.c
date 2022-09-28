/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)nlist.c	1.8	95/06/14 SMI" 	/* SVr4.0 1.13	*/

/*LINTLIBRARY*/

#include "libelf.h"
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <nlist.h>
#include <note.h>
#include "linenum.h"
#include "syms.h"

#undef n_name		/* This undef is to handle a #define in syms.h */
			/* which conflicts with the member nlist->n_name */
			/* as defined in nlist.h */


#define	SPACE 100		/* number of symbols read at a time */
#define	ISELF (strncmp(magic_buf, ELFMAG, SELFMAG) == 0)


int
end_elf_job(int fd, Elf * elfdes)
{
	(void) elf_end(elfdes);
	(void) close(fd);
	return (-1);
}


int
_elf_nlist(int fd, struct nlist * list)
{
	Elf	   *elfdes;	/* ELF descriptor */
	Elf32_Shdr *s_buf;	/* buffer storing section header */
	Elf_Data   *symdata;	/* buffer points to symbol table */
	Elf_Scn    *secidx = 0;	/* index of the section header table */
	Elf32_Sym  *sym;	/* buffer storing one symbol information */
	Elf32_Sym  *sym_end;	/* end of symbol table */
	unsigned   strtab;	/* index of symbol name in string table */

	if (elf_version(EV_CURRENT) == EV_NONE) {
		(void) close(fd);
		return (-1);
	}
	elfdes = elf_begin(fd, ELF_C_READ, (Elf *)0);
	if (elf32_getehdr(elfdes) == (Elf32_Ehdr *)0)
		return (end_elf_job(fd, elfdes));

	while ((secidx = elf_nextscn(elfdes, secidx)) != 0) {
		if ((s_buf = elf32_getshdr(secidx)) == (Elf32_Shdr *)0)
			return (end_elf_job(fd, elfdes));
		if (s_buf->sh_type != SHT_SYMTAB) /* not symbol table */
			continue;
		symdata = elf_getdata(secidx, (Elf_Data *)0);
		if (symdata == 0)
			return (end_elf_job(fd, elfdes));
		if (symdata->d_size == 0)
			break;
		strtab = s_buf->sh_link;
		sym = (Elf32_Sym *) (symdata->d_buf);
		sym_end = sym + symdata->d_size / sizeof (Elf32_Sym);
		for (; sym < sym_end; ++sym) {
			struct nlist *p;
			register char *name;
			name = elf_strptr(elfdes, strtab, (size_t)sym->st_name);
			if (name == 0)
				continue;
			for (p = list; p->n_name && p->n_name[0]; ++p) {
				if (strcmp(p->n_name, name))
					continue;
				p->n_value = sym->st_value;
				p->n_type = ELF32_ST_TYPE(sym->st_info);
				p->n_scnum = sym->st_shndx;
				break;
			}
		}
		break;
		/*
		 * Currently there is only one symbol table section
		 * in an object file, but this restriction may be
		 * relaxed in the future.
		 */
	}
	(void) elf_end(elfdes);
	(void) close(fd);
	return (0);
}


NOTE(SCHEME_PROTECTS_DATA("user provides buffers", nlist))

int
nlist(const char * name, struct nlist * list)
{
	register struct nlist *p;
	char magic_buf[5];
	int fd;

	for (p = list; p->n_name && p->n_name[0]; p++) { /* n_name can be ptr */
		p->n_type = 0;
		p->n_value = 0L;
		p->n_scnum = 0;
		p->n_sclass = 0;
		p->n_numaux = 0;
	}

	if ((fd = open(name, 0)) < 0)
		return (-1);
	if (read(fd, magic_buf, 4) == -1) {
		(void) close(fd);
		return (-1);
	}
	magic_buf[4] = '\0';
	if (lseek(fd, 0L, 0) == -1L) { /* rewind to beginning of object file */
		(void) close(fd);
		return (-1);
	}

	if (ISELF)
		return (_elf_nlist(fd, list));
	else
		return (-1);
}
