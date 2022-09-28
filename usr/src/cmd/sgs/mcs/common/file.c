/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1995 by Sun Microsystems, Inc.
 *	All Rights Reserved
 */
#pragma ident	"@(#)file.c	1.14	96/10/17 SMI"

#include "mcs.h"
#include "extern.h"
extern int errno;

static int Sect_exists = 0;
static int notesegndx = -1;
static int notesctndx = -1;
static Seg_Table *b_e_seg_table;

static section_info_table *sec_table;
static Elf32_Off *off_table;	/* array maintains section's offset; */
				/* set to retain old offset, else 0 */
static int *nobits_table;  	/* array maintains NOBITS sections */

static char *new_sec_string = NULL;
/*
 * Function prototypes.
 */
static void copy_file(char *, char *);
static void
copy_non_elf_to_temp_ar(int, Elf *, int, Elf_Arhdr *, char *, Cmd_Info *);
static void copy_elf_file_to_temp_ar_file(int, Elf_Arhdr *, char *);
static int process_file(Elf *, char *, Cmd_Info *);
static void initialize(Elf32_Ehdr *);
static int build_segment_table(Elf32_Phdr *, Elf32_Ehdr *);
static int traverse_file(Elf *, Elf32_Phdr *, Elf32_Ehdr *, char *, Cmd_Info *);
static int location(int, int, Elf32_Ehdr *);
static int shdr_location(Elf32_Shdr *, Elf32_Ehdr *);
static int build_file(Elf *, Elf32_Phdr *, Elf32_Ehdr *, Cmd_Info *);
static char *get_dir(char *);

int
each_file(char *cur_file, Cmd_Info *cmd_info)
{
	Elf *elf = 0;
	Elf_Cmd cmd;
	Elf *arf = 0;
	Elf_Arhdr *mem_header;
	char *cur_filenm = NULL;
	int code = 0;
	int error = 0, err = 0;
	int ar_file = 0;
	int fdartmp;
	int fd;
	int mode;

	if (cmd_info->flags & MIGHT_CHG)
		mode = R_OK|W_OK;
	else
		mode = R_OK;

	if (access(cur_file, mode) < 0) {
		error_message(ACCESS_ERROR,
		SYSTEM_ERROR, strerror(errno), prog, cur_file);
		return (FAILURE);
	}

	if ((fd = open(cur_file, O_RDONLY)) == -1) {
		error_message(OPEN_ERROR,
		SYSTEM_ERROR, strerror(errno), prog, cur_file);
		return (FAILURE);
	}

	cmd = ELF_C_READ;
	if ((arf = elf_begin(fd, cmd, (Elf *)0)) == 0) {
		error_message(LIBELF_ERROR,
		LIBelf_ERROR, elf_errmsg(-1), prog);
		(void) elf_end(arf);
		(void) close(fd);   /* done processing this file */
		return (FAILURE);
	}

	if ((elf_kind(arf) == ELF_K_AR)) {
		ar_file = 1;
		if (CHK_OPT(cmd_info, MIGHT_CHG)) {
			artmpfile = tempnam(TMPDIR, "mcs2");
			if ((fdartmp = open(artmpfile,
			    O_WRONLY | O_APPEND | O_CREAT,
			    (mode_t) 0666)) == NULL) {
				error_message(OPEN_TEMP_ERROR,
				SYSTEM_ERROR, strerror(errno),
				prog, artmpfile);
				(void) elf_end(arf);
				(void) close(fd);
				exit(FAILURE);
			}
			/* write magic string to artmpfile */
			if ((write(fdartmp, ARMAG, SARMAG)) != SARMAG) {
				error_message(WRITE_ERROR,
				SYSTEM_ERROR, strerror(errno),
				prog, artmpfile, cur_file);
				mcs_exit(FAILURE);
			}
		}
	} else {
		ar_file = 0;
		cur_filenm = cur_file;
	}

	/*
	 * Holds temporary file;
	 * if archive, holds the current member file if it has an ehdr,
	 * and there were no errors in
	 * processing the object file.
	 */
	elftmpfile = tempnam(TMPDIR, "mcs1");

	while ((elf = elf_begin(fd, cmd, arf)) != 0) {
		if (ar_file) /* get header info */ {
			if ((mem_header = elf_getarhdr(elf)) == NULL) {
				error_message(GETARHDR_ERROR,
				LIBelf_ERROR, elf_errmsg(-1),
				prog, cur_file, elf_getbase(elf));
				(void) elf_end(elf);
				(void) elf_end(arf);
				(void) close(fd);
				(void) unlink(artmpfile);
				return (FAILURE);
			}

			if (cur_filenm != NULL)
				free(cur_filenm);
			if ((cur_filenm = (char *)malloc((strlen(cur_file) + 3 +
			    strlen(mem_header->ar_name)))) == NULL) {
				error_message(MALLOC_ERROR,
				PLAIN_ERROR, (char *)0,
				prog);
				mcs_exit(FAILURE);
			}

			(void) sprintf(cur_filenm, "%s[%s]",
				cur_file, mem_header->ar_name);
		}

		if (elf_kind(elf) == ELF_K_ELF) {
			if ((code = process_file(elf, cur_filenm, cmd_info)) ==
			    FAILURE) {
				if (!ar_file) {
					(void) elf_end(arf);
					(void) elf_end(elf);
					(void) close(fd);
					return (FAILURE);
				} else {
					copy_non_elf_to_temp_ar(
					fd, elf, fdartmp, mem_header,
					cur_file, cmd_info);
					error++;
				}
			} else if (ar_file && CHK_OPT(cmd_info, MIGHT_CHG)) {
				if (code == DONT_BUILD)
					copy_non_elf_to_temp_ar(
					fd, elf, fdartmp, mem_header,
					cur_file, cmd_info);
				else
					copy_elf_file_to_temp_ar_file(
						fdartmp, mem_header, cur_file);
			}
		} else {
			/*
			 * decide what to do with non-ELF file
			 */
			if (!ar_file) {
				error_message(FILE_TYPE_ERROR,
				PLAIN_ERROR, (char *)0,
				prog, cur_filenm);
				(void) close(fd);
				return (FAILURE);
			} else {
				if (CHK_OPT(cmd_info, MIGHT_CHG))
					copy_non_elf_to_temp_ar(
					fd, elf, fdartmp, mem_header,
					cur_file, cmd_info);
			}
		}
		cmd = elf_next(elf);
		(void) elf_end(elf);
	}

	err = elf_errno();
	if (err != 0) {
		error_message(LIBELF_ERROR,
		LIBelf_ERROR, elf_errmsg(err), prog);
		error_message(NOT_MANIPULATED_ERROR,
		PLAIN_ERROR, (char *)0,
		prog, cur_file);
		return (FAILURE);
	}

	(void) elf_end(arf);
	(void) close(fd);   /* done processing this file */

	if (ar_file && CHK_OPT(cmd_info, MIGHT_CHG)) {
		(void) close(fdartmp); /* done writing to ar_temp_file */
		copy_file(cur_file, artmpfile); /* copy ar_temp_file to FILE */
	} else if (code != DONT_BUILD && CHK_OPT(cmd_info, MIGHT_CHG))
		copy_file(cur_file, elftmpfile); /* copy temp_file to FILE */
	return (error);
}

static int
process_file(Elf *elf, char *cur_file, Cmd_Info *cmd_info)
{
	int error = SUCCESS;
	Elf32_Phdr *phdr = 0;
	Elf32_Ehdr *ehdr = 0;
	int x;

	/*
	 * Initialize
	 */
	if ((ehdr = elf32_getehdr(elf)) != 0)
		initialize(ehdr);
	else {
		error_message(LIBELF_ERROR,
		LIBelf_ERROR, elf_errmsg(-1), prog);
		return (FAILURE);
	}
	if ((phdr = elf32_getphdr(elf)) != NULL) {
		if (build_segment_table(phdr, ehdr) == FAILURE)
			return (FAILURE);
	}

	if ((x = traverse_file(elf, phdr, ehdr, cur_file, cmd_info)) ==
	    FAILURE) {
		error_message(WRN_MANIPULATED_ERROR,
		PLAIN_ERROR, (char *)0,
		prog, cur_file);
		error = FAILURE;
	} else if (x != DONT_BUILD && x != FAILURE) {
		if (build_file(elf, phdr, ehdr, cmd_info) == FAILURE) {
			error_message(WRN_MANIPULATED_ERROR,
			PLAIN_ERROR, (char *)0,
			prog, cur_file);
			error = FAILURE;
		}
	}

	if (x == DONT_BUILD)
		return (DONT_BUILD);
	else {
		free(off_table);
		free(sec_table);
		free(nobits_table);
		return (error);
	}
}

static int
traverse_file(Elf *elf, Elf32_Phdr *phdr, Elf32_Ehdr *ehdr,
	char *cur_file, Cmd_Info *cmd_info)
{
	Elf_Scn *scn;
	Elf_Scn *temp_scn;
	Elf32_Shdr *temp_shdr;
	Elf_Data *data;
	Elf32_Shdr *shdr;
	int ret = 0;
	int SYM = 0;		/* used by strip command */
	int x;

	unsigned int ndx, i, scn_index = 1;
	char *temp_name;

	Sect_exists = 0;

	ndx = ehdr->e_shstrndx; /* used to find section's name */
	scn = 0;
	while ((scn = elf_nextscn(elf, scn)) != 0) {
		char *name = "";

		if ((shdr = elf32_getshdr(scn)) == 0) {
			error_message(NO_SECT_TABLE_ERROR,
			LIBelf_ERROR, elf_errmsg(-1),
			prog, cur_file);
			return (FAILURE);
		} else {
			name = elf_strptr(elf, ndx, (size_t)shdr->sh_name);
			if (name == NULL)
				name = "_@@@###";
		}

		sec_table[scn_index].scn = scn;
		sec_table[scn_index].shdr = shdr;
		sec_table[scn_index+1].scn = (Elf_Scn *) -1;
		sec_table[scn_index].secno = scn_index;
		sec_table[scn_index].osecno = scn_index;
		SET_ACTION(sec_table[scn_index].flags, ACT_NOP);
		sec_table[scn_index].name = strdup(name);
		if (phdr == NULL)
			SET_LOC(sec_table[scn_index].flags, NOSEG);
		else
			SET_LOC(sec_table[scn_index].flags, \
				shdr_location(shdr, ehdr));

		/*
		 * If the target section is pointed by a section
		 * holding relocation infomation, then the
		 * pointing section would be useless if the
		 * target section is removed.
		 */
		if ((shdr->sh_type == SHT_REL || shdr->sh_type == SHT_RELA) &&
		    (shdr->sh_info != SHN_UNDEF &&
		    (temp_scn = elf_getscn(elf, shdr->sh_info)) != 0)) {
			if ((temp_shdr = elf32_getshdr(temp_scn)) != 0) {
				temp_name = elf_strptr(elf, ndx,
					(size_t)temp_shdr->sh_name);
				sec_table[scn_index].rel_name = temp_name;
				sec_table[scn_index].rel_scn_index =
				    shdr->sh_info;
				if (phdr == NULL)
					sec_table[scn_index].rel_loc = NOSEG;
				    else
					sec_table[scn_index].rel_loc =
						shdr_location(temp_shdr, ehdr);
			}
		}
		data = 0;
		if ((data = elf_getdata(scn, data)) == NULL) {
			error_message(LIBELF_ERROR,
			LIBelf_ERROR, elf_errmsg(-1), prog);
			return (FAILURE);
		}
		sec_table[scn_index].data = data;

		/*
		 * Check if this section is a candidate for
		 * action to be processes.
		 */
		if (sectcmp(name) == 0) {
			SET_CANDIDATE(sec_table[scn_index].flags);

			/*
			 * This flag just shows that there was a
			 * candidate.
			 */
			Sect_exists++;
		}
		x = GET_LOC(sec_table[scn_index].flags);

		/*
		 * Remeber the note sections index so that we can
		 * reset the NOTE segments offset to point to it.
		 *
		 * It may have been assigned a new location in the
		 * resulting output elf image.
		 */
		if (shdr->sh_type == SHT_NOTE)
			notesctndx = scn_index;

		if (x == IN || x == PRIOR)
			off_table[scn_index] = shdr->sh_offset;
		if (shdr->sh_type == SHT_NOBITS)
			nobits_table[scn_index] = 1;

		/*
		 * If this section satisfies the condition,
		 * apply the actions specified.
		 */
		if (ISCANDIDATE(sec_table[scn_index].flags)) {
			ret += apply_action(&sec_table[scn_index],
				cur_file, cmd_info);
		}

		/*
		 * If I am strip command, determin if symtab can go or not.
		 */
		if (CHK_OPT(cmd_info, I_AM_STRIP) &&
		    (CHK_OPT(cmd_info, xFLAG) == 0) &&
		    (CHK_OPT(cmd_info, lFLAG) == 0)) {
			if (shdr->sh_type == SHT_SYMTAB &&
			    GET_LOC(sec_table[scn_index].flags) == AFTER) {
				SYM = scn_index;
			}
		}
		scn_index++;
	}

	/*
	 * If there were any errors traversing the file,
	 * just return error.
	 */
	if (ret != 0)
		return (FAILURE);

	/*
	 * Remove symbol table if possible
	 */
	if (CHK_OPT(cmd_info, I_AM_STRIP) && SYM != 0) {
		Elf32_Word sh_link;
		section_info_table *i = &sec_table[0];

		sh_link = (sec_table[SYM].shdr)->sh_link;
		sec_table[SYM].secno = DELETED;
		++(cmd_info->no_of_nulled);
		if (Sect_exists == 0)
			++Sect_exists;
		SET_ACTION(sec_table[SYM].flags, ACT_DELETE);
		off_table[SYM] = 0;
		/*
		 * Can I remove section header
		 * string table ?
		 */
		if ((sh_link != SHN_UNDEF) &&
		    (sh_link != ehdr->e_shstrndx) &&
		    (GET_LOC(i[sh_link].flags) == AFTER)) {
			i[sh_link].secno = DELETED;
			++(cmd_info->no_of_nulled);
			if (Sect_exists == 0)
				++Sect_exists;
			SET_ACTION(i[sh_link].flags, ACT_DELETE);
			off_table[sh_link] = 0;
		}
	}

	/*
	 * If I only printed the contents, then
	 * just report so.
	 */
	if (CHK_OPT(cmd_info, pFLAG) && !CHK_OPT(cmd_info, MIGHT_CHG))
		return (DONT_BUILD); /* don't bother creating a new file */
				/* since the file has not changed */

	/*
	 * I might need to add a new section. Check it.
	 */
	if (Sect_exists == 0 && CHK_OPT(cmd_info, aFLAG)) {
		int act = 0;
		for (act = 0; act < actmax; act++) {
			if (Action[act].a_action == ACT_APPEND) {
				new_sec_string = Action[act].a_string;
				cmd_info->no_of_append = 1;
				break;
			}
		}
	}

	/*
	 * If I did not append any new sections, and I did not
	 * modify/delete any sections, then just report so.
	 */
	if ((Sect_exists == 0 && cmd_info->no_of_append == 0) ||
	    !CHK_OPT(cmd_info, MIGHT_CHG))
		return (DONT_BUILD);

	/*
	 * Found at least one section which was processed.
	 *	Deleted or Appended or Compressed.
	 */
	if (Sect_exists) {
		/*
		 * First, handle the deleted sections.
		 */
		if (cmd_info->no_of_delete != 0 ||
		    cmd_info->no_of_nulled != 0) {
			int acc = 0;
			int rel_idx;

			/*
			 * Handle relocation/target
			 * sections.
			 */
			for (i = 1; i < ehdr->e_shnum; i++) {
				rel_idx = sec_table[i].rel_scn_index;
				if (rel_idx == 0)
					continue;

				/*
				 * If I am removed, then remove my
				 * target section.
				 */
				if ((sec_table[i].secno == DELETED ||
				    sec_table[i].secno == NULLED) &&
				    sec_table[i].rel_loc != IN) {
					if (GET_LOC(sec_table[rel_idx].flags) ==
					    PRIOR)
						sec_table[rel_idx].secno =
							NULLED;
					else
						sec_table[rel_idx].secno =
							DELETED;
					SET_ACTION(sec_table[rel_idx].flags,\
						ACT_DELETE);
				}

				/*
				 * I am not removed. Check if my target is
				 * removed or nulled. If so, let me try to
				 * remove my self.
				 */
				if ((sec_table[rel_idx].secno == DELETED ||
				    sec_table[rel_idx].secno == NULLED) &&
				    GET_LOC(sec_table[i].flags) != IN) {
					if (GET_LOC(sec_table[i].flags) ==
					    PRIOR)
						sec_table[i].secno =
							NULLED;
					else
						sec_table[i].secno =
							DELETED;
					SET_ACTION(sec_table[i].flags,\
						ACT_DELETE);
				}
			}

			/*
			 * Now, take care of DELETED sections
			 */
			for (i = 1; i < ehdr->e_shnum; i++) {
				if (sec_table[i].secno == DELETED)
					acc++;
				else
					sec_table[i].secno -= acc;
			}
		}
	}

	/*
	 * I know that the file has been modified.
	 * A new file need to be created.
	 */
	return (SUCCESS);
}

static int
build_file(Elf *elf, Elf32_Phdr *phdr, Elf32_Ehdr *ehdr, Cmd_Info *cmd_info)
{
	Elf_Scn *scn;
	Elf_Scn *elf_scn;
	Elf32_Sym	*p, *q;
	Elf32_Off	new_offset = 0, r;
	Elf32_Word	new_sh_name = 0;  /* to hold the offset for the new */
					/* section's name */
	Elf *elffile = 0;
	Elf_Data *elf_data;
	Elf_Data *data;
	Elf32_Phdr *elf_phdr = NULL;
	Elf32_Shdr *elf_shdr;
	Elf32_Shdr *shdr;
	Elf32_Ehdr *elf_ehdr;
	int scn_no, x;
	unsigned int no_of_symbols = 0;
	section_info_table *info;
	unsigned int    c = 0;
	int fdtmp;

	if ((fdtmp = open(elftmpfile, O_RDWR |
		O_TRUNC | O_CREAT, (mode_t) 0666)) == -1) {
		error_message(OPEN_TEMP_ERROR,
		SYSTEM_ERROR, strerror(errno),
		prog, elftmpfile);
		return (FAILURE);
	}

	if ((elffile = elf_begin(fdtmp, ELF_C_WRITE, (Elf *) 0)) == NULL) {
		error_message(READ_ERROR,
		LIBelf_ERROR, elf_errmsg(-1),
		prog, elftmpfile);
		(void) close(fdtmp);
		return (FAILURE);
	}

	if ((elf_ehdr = elf32_newehdr(elffile)) == NULL) {
		error_message(LIBELF_ERROR,
		LIBelf_ERROR, elf_errmsg(-1), prog);
		return (FAILURE);
	}
	(void) memcpy(elf_ehdr, ehdr, sizeof (Elf32_Ehdr));

	if (phdr != NULL) {
		elf_flagelf(elffile, ELF_C_SET, ELF_F_LAYOUT);

		if ((elf_phdr =
		    elf32_newphdr(elffile, ehdr->e_phnum)) == NULL) {
			error_message(LIBELF_ERROR,
			LIBelf_ERROR, elf_errmsg(-1), prog);
			return (FAILURE);
		}
		(void) memcpy(elf_phdr,
			phdr,
			ehdr->e_phentsize * ehdr->e_phnum);

		x = location(elf_ehdr->e_phoff, 0, ehdr);
		if (x == AFTER)
			new_offset = (Elf32_Off) ehdr->e_ehsize;
	}

	scn = 0;
	scn_no = 1;
	while ((scn = sec_table[scn_no].scn) != (Elf_Scn *) -1) {
		info = &sec_table[scn_no];
		/*  If section should be copied to new file NOW */
		if (info->secno != DELETED && info->secno <= scn_no) {
			shdr = info->shdr;
			if ((elf_scn = elf_newscn(elffile)) == NULL) {
				error_message(LIBELF_ERROR,
				LIBelf_ERROR, elf_errmsg(-1), prog);
				return (FAILURE);
			}
			if ((elf_shdr = elf32_getshdr(elf_scn)) == NULL) {
				error_message(LIBELF_ERROR,
				LIBelf_ERROR, elf_errmsg(-1), prog);
				return (FAILURE);
			}
			(void) memcpy(elf_shdr, shdr, sizeof (Elf32_Shdr));

			/* update link and info fields */
			if (sec_table[shdr->sh_link].secno < 0)
				elf_shdr->sh_link = 0;
			else
				elf_shdr->sh_link =
				(Elf32_Word)sec_table[shdr->sh_link].secno;

			if (shdr->sh_type == SHT_REL) {
				if (sec_table[shdr->sh_info].secno < 0)
					elf_shdr->sh_info = 0;
				else
					elf_shdr->sh_info = (Elf32_Word)
						sec_table[shdr->sh_info].secno;
			}

			data = sec_table[scn_no].data;
			if ((elf_data = elf_newdata(elf_scn)) == NULL) {
				error_message(LIBELF_ERROR,
				LIBelf_ERROR, elf_errmsg(-1), prog);
				return (FAILURE);
			}
			*elf_data = *data;

			/* SYMTAB might need some change */
			if (shdr->sh_type == SHT_SYMTAB &&
			    shdr->sh_entsize > 0 &&
			    (cmd_info->no_of_delete != 0 ||
			    cmd_info->no_of_nulled != 0)) {
				Elf32_Sym * new_sym;
				no_of_symbols = shdr->sh_size/shdr->sh_entsize;
				p = (Elf32_Sym *)data->d_buf;
				new_sym = (Elf32_Sym *)
				malloc(no_of_symbols * sizeof (Elf32_Sym));
				if (new_sym == NULL) {
					error_message(MALLOC_ERROR,
					PLAIN_ERROR, (char *)0, prog);
					mcs_exit(FAILURE);
				}
				q = new_sym;

				for (c = 0; c < no_of_symbols; c++, p++, q++) {
					*q = *p;
					if (p->st_shndx <= ehdr->e_shnum &&
					p->st_shndx > 0) {
						section_info_table *i;
						i = &sec_table[p->st_shndx];
						if (i->secno != DELETED &&
						i->secno != NULLED)
							q->st_shndx = i->secno;
						else {
						/*
						 * The section which this symbol
						 * relates to is removed.
						 * There is no way to specify
						 * this fact, just change the
						 * shndx to 1.
						 */
							q->st_shndx = 1;
						}
					}
				}
				/* CSTYLED */
				elf_data->d_buf = (void *) new_sym;
			}

			/*
			 * If the section is to be updated,
			 * do so.
			 */
			if (ISCANDIDATE(info->flags)) {
				if (GET_LOC(info->flags) == PRIOR &&
				    (info->secno == NULLED ||
				    info->secno == EXPANDED ||
				    info->secno == SHRUNK)) {
					/*
					* The section is updated,
					* but the position is not too
					* good. Need to NULL this out.
					*/
					elf_shdr->sh_name = 0;
					elf_shdr->sh_type = SHT_PROGBITS;
					if (info->secno != NULLED) {
						(cmd_info->no_of_moved)++;
						SET_MOVING(info->flags);
					}
				} else {
					/*
					 * The section is positioned AFTER,
					 * or there are no segments.
					 * It is safe to update this section.
					 */
					data = sec_table[scn_no].mdata;
					*elf_data = *data;
					elf_shdr->sh_size = elf_data->d_size;
				}
			}
			/* add new section name to shstrtab? */
			else if (!Sect_exists &&
			    new_sec_string != NULL &&
			    scn_no == ehdr->e_shstrndx &&
			    elf_shdr->sh_type == SHT_STRTAB &&
			    (phdr == NULL ||
			    (x = shdr_location(elf_shdr, ehdr)) != IN ||
			    x != PRIOR)) {
				int sect_len;

				sect_len = strlen(SECT_NAME);
				if ((elf_data->d_buf = (char *)
				malloc((elf_shdr->sh_size +
				sect_len + 1))) == NULL) {
					error_message(MALLOC_ERROR,
					PLAIN_ERROR, (char *)0, prog);
					mcs_exit(FAILURE);
				}
				/* put original data plus new data in section */
				(void) memcpy(elf_data->d_buf,
					data->d_buf, data->d_size);
				(void) memcpy(&((char *) elf_data->d_buf)
					[data->d_size],
					SECT_NAME,
					sect_len + 1);
				new_sh_name = elf_shdr->sh_size;
				elf_shdr->sh_size += sect_len + 1;
				elf_data->d_size += sect_len + 1;
			}

			/*
			 * Compute offsets.
			 */
			if (phdr != NULL) {
				/*
				 * Compute section offset.
				 */
				if (off_table[scn_no] == 0) {
					if (elf_shdr->sh_addralign != 0) {
						r = new_offset %
						    elf_shdr->sh_addralign;
						if (r)
						    new_offset +=
						    elf_shdr->sh_addralign - r;
					}
					elf_shdr->sh_offset = new_offset;
					elf_data->d_off = 0;
				} else {
					if (nobits_table[scn_no] == 0)
						new_offset = off_table[scn_no];
				}
				if (nobits_table[scn_no] == 0)
					new_offset += elf_shdr->sh_size;
			}
		}
		scn_no++;
	}

	/*
	 * This is the real new section.
	 */
	if (!Sect_exists && new_sec_string != NULL) {
		int string_size;
		string_size = strlen(new_sec_string) + 1;
		if ((elf_scn = elf_newscn(elffile)) == NULL) {
			error_message(LIBELF_ERROR,
			LIBelf_ERROR, elf_errmsg(-1), prog);
			return (FAILURE);
		}

		if ((elf_shdr = elf32_getshdr(elf_scn)) == NULL) {
			error_message(LIBELF_ERROR,
			LIBelf_ERROR, elf_errmsg(-1), prog);
			return (FAILURE);
		}

		elf_shdr->sh_name = new_sh_name;
		elf_shdr->sh_type = SHT_PROGBITS;
		elf_shdr->sh_flags = 0;
		elf_shdr->sh_addr = 0;
		if (phdr != NULL)
			elf_shdr->sh_offset = new_offset;
		else
			elf_shdr->sh_offset = 0;
		elf_shdr->sh_size = string_size + 1;
		elf_shdr->sh_link = 0;
		elf_shdr->sh_info = 0;
		elf_shdr->sh_addralign = 1;
		elf_shdr->sh_entsize = 0;

		if ((elf_data = elf_newdata(elf_scn)) == NULL) {
			error_message(LIBELF_ERROR,
			LIBelf_ERROR, elf_errmsg(-1), prog);
			return (FAILURE);
		}
		elf_data->d_size = string_size + 1;
		if ((elf_data->d_buf = (char *)
		    calloc(1, string_size + 1)) == NULL) {
			error_message(MALLOC_ERROR,
			PLAIN_ERROR, (char *)0,
			prog);
			mcs_exit(FAILURE);
		}
		(void) memcpy(&((char *) elf_data->d_buf)[1],
			new_sec_string, string_size);
		elf_data->d_align = 1;
		new_offset += string_size + 1;
	}

	/*
	 * If there are sections which needed to be moved,
	 * then do it here.
	 */
	if (cmd_info->no_of_moved != 0) {
		int cnt;
		info = &sec_table[0];

		for (cnt = 0; cnt < (int)ehdr->e_shnum; cnt++, info++) {
			if ((GET_MOVING(info->flags)) == 0)
				continue;

			if ((scn = elf_getscn(elf, info->osecno)) == NULL) {
				error_message(LIBELF_ERROR,
				LIBelf_ERROR, elf_errmsg(-1), prog);
				return (FAILURE);
			}
			if ((shdr = elf32_getshdr(scn)) == NULL) {
				error_message(LIBELF_ERROR,
				LIBelf_ERROR, elf_errmsg(-1), prog);
				return (FAILURE);
			}
			if ((elf_scn = elf_newscn(elffile)) == NULL) {
				error_message(LIBELF_ERROR,
				LIBelf_ERROR, elf_errmsg(-1), prog);
				return (FAILURE);
			}
			if ((elf_shdr = elf32_getshdr(elf_scn)) == NULL) {
				error_message(LIBELF_ERROR,
				LIBelf_ERROR, elf_errmsg(-1), prog);
				return (FAILURE);
			}
			(void) memcpy(elf_shdr, shdr, sizeof (Elf32_Shdr));

			data = info->mdata;

			elf_shdr->sh_offset = new_offset;  /* UPDATE fields */
			elf_shdr->sh_size = data->d_size;
			elf_shdr->sh_link =
				(Elf32_Word) sec_table[shdr->sh_link].osecno;
			elf_shdr->sh_info =
				(Elf32_Word) sec_table[shdr->sh_info].osecno;
			if ((elf_data = elf_newdata(elf_scn)) == NULL) {
				error_message(LIBELF_ERROR,
				LIBelf_ERROR, elf_errmsg(-1), prog);
				return (FAILURE);
			}
			(void) memcpy(elf_data, data, sizeof (Elf_Data));

			new_offset += data->d_size;
		}
	}

	/*
	 * In the event that the position of the sting table has changed,
	 * as a result of deleted sections, update the ehdr->e_shstrndx.
	 */
	if (elf_ehdr->e_shstrndx > 0 && elf_ehdr->e_shnum > 0 &&
	    (sec_table[elf_ehdr->e_shstrndx].secno <
	    (int)  elf_ehdr->e_shnum)) {
		elf_ehdr->e_shstrndx =
			(Elf32_Half) sec_table[elf_ehdr->e_shstrndx].secno;
	}


	if (phdr != NULL) {
		/* UPDATE location of program header table */
		if (location(elf_ehdr->e_phoff, 0, ehdr) == AFTER) {
			r = new_offset % 4;
			if (r)
				new_offset += 4 - r;

			elf_ehdr->e_phoff = new_offset;
			new_offset += elf_ehdr->e_phnum*elf_ehdr->e_phentsize;
		}
		/* UPDATE location of section header table */
		if ((location(elf_ehdr->e_shoff, 0, ehdr) == AFTER) ||
		    ((location(elf_ehdr->e_shoff, 0, ehdr) == PRIOR) &&
		    (!Sect_exists && new_sec_string != NULL))) {
			r = new_offset % 4;
			if (r)
				new_offset += 4 - r;

			elf_ehdr->e_shoff = new_offset;
		}
		free(b_e_seg_table);

		/*
		 * The NOTE segment is the one segments whos
		 * sections might get moved by mcs processing.
		 * Make sure that the NOTE segments offset points
		 * to the .note section.
		 */
		if ((notesegndx != -1) && (notesctndx != -1) &&
		    (sec_table[notesctndx].secno)) {
			Elf32_Phdr *	newphdr;
			Elf_Scn *	notescn;
			Elf32_Shdr *	noteshdr;
			notescn = elf_getscn(elffile,
				sec_table[notesctndx].secno);
			noteshdr = elf32_getshdr(notescn);
			newphdr = elf_phdr + notesegndx;
			newphdr->p_offset = noteshdr->sh_offset;
		}
	}

	if (elf_update(elffile, ELF_C_WRITE) < 0) {
		error_message(LIBELF_ERROR,
		LIBelf_ERROR, elf_errmsg(-1), prog);
		return (FAILURE);
	}

	(void) elf_end(elffile);
	(void) close(fdtmp);
	return (SUCCESS);
}

/*
 * Search through PHT saving the beginning and ending segment offsets
 */
static int
build_segment_table(Elf32_Phdr *phdr, Elf32_Ehdr *ehdr)
{
	unsigned int i;
	Elf32_Phdr *phdr1;

	if ((b_e_seg_table = (Seg_Table *)
		calloc(ehdr->e_phnum, sizeof (Seg_Table))) == NULL) {
		error_message(MALLOC_ERROR,
		PLAIN_ERROR, (char *)0,
		prog);
		mcs_exit(FAILURE);
	}

	phdr1 = phdr;
	for (i = 0; i < ehdr->e_phnum; i++, phdr1++) {
		/*
		 * remember the note SEGMENTS index so that we can
		 * re-set it's p_offset later if needed.
		 */
		if (phdr1->p_type & PT_NOTE)
			notesegndx = i;

		b_e_seg_table[i].p_offset = phdr1->p_offset;
		b_e_seg_table[i].p_memsz = phdr1->p_offset + phdr1->p_memsz;
		b_e_seg_table[i].p_filesz = phdr1->p_offset + phdr1->p_filesz;
	}
	return (SUCCESS);
}

static void
copy_elf_file_to_temp_ar_file(
	int fdartmp,
	Elf_Arhdr *mem_header,
	char *cur_file)
{
	char *buf;
	char mem_header_buf[sizeof (struct ar_hdr) + 1];
	int fdtmp3;
	struct stat stbuf;

	if ((fdtmp3 = open(elftmpfile, O_RDONLY)) == -1) {
		error_message(OPEN_TEMP_ERROR,
		SYSTEM_ERROR, strerror(errno),
		prog, elftmpfile);
		mcs_exit(FAILURE);
	}

	(void) stat(elftmpfile, &stbuf); /* for size of file */

	if ((buf = (char *)
	    malloc(ROUNDUP(stbuf.st_size))) == NULL) {
		error_message(MALLOC_ERROR,
		PLAIN_ERROR, (char *)0,
		prog);
		mcs_exit(FAILURE);
	}

	if (read(fdtmp3, buf, (unsigned)stbuf.st_size) !=
	    (unsigned)stbuf.st_size) {
		error_message(READ_MANI_ERROR,
		SYSTEM_ERROR, strerror(errno),
		prog, elftmpfile, cur_file);
		mcs_exit(FAILURE);
	}

	(void) sprintf(mem_header_buf, FORMAT,
		mem_header->ar_rawname,
		mem_header->ar_date,
		(unsigned) mem_header->ar_uid,
		(unsigned) mem_header->ar_gid,
		(unsigned) mem_header->ar_mode,
		stbuf.st_size, ARFMAG);

	if (write(fdartmp, mem_header_buf,
	    (unsigned) sizeof (struct ar_hdr)) !=
	    (unsigned) sizeof (struct ar_hdr)) {
		error_message(WRITE_MANI_ERROR,
		SYSTEM_ERROR, strerror(errno),
		prog, elftmpfile, cur_file);
		mcs_exit(FAILURE);
	}

	if (stbuf.st_size & 0x1) {
		buf[stbuf.st_size] = '\n';
		if (write(fdartmp, buf, (unsigned)ROUNDUP(stbuf.st_size)) !=
		    (unsigned) ROUNDUP(stbuf.st_size)) {
			error_message(WRITE_MANI_ERROR,
			SYSTEM_ERROR, strerror(errno),
			prog, elftmpfile, cur_file);
			mcs_exit(FAILURE);
		}
	} else if (write(fdartmp, buf, (unsigned)stbuf.st_size) !=
	    (unsigned) stbuf.st_size) {
			error_message(WRITE_MANI_ERROR,
			SYSTEM_ERROR, strerror(errno),
			prog, elftmpfile, cur_file);
			mcs_exit(FAILURE);
	}
	free(buf);
	(void) close(fdtmp3);
}

static void
copy_non_elf_to_temp_ar(
	int fd,
	Elf *elf,
	int fdartmp,
	Elf_Arhdr *mem_header,
	char *cur_file,
	Cmd_Info *cmd_info)
{
	char    mem_header_buf[sizeof (struct ar_hdr) + 1];
	char *file_buf;

	if (strcmp(mem_header->ar_name, "/") != 0) {
		(void) sprintf(mem_header_buf, FORMAT,
			mem_header->ar_rawname,
			mem_header->ar_date,
			(unsigned) mem_header->ar_uid,
			(unsigned) mem_header->ar_gid,
			(unsigned) mem_header->ar_mode,
			mem_header->ar_size, ARFMAG);

		if (write(fdartmp, mem_header_buf, sizeof (struct ar_hdr)) !=
		    sizeof (struct ar_hdr)) {
			error_message(WRITE_MANI_ERROR,
			SYSTEM_ERROR, strerror(errno),
			prog, cur_file);
			mcs_exit(FAILURE);
		}
		if ((file_buf = (char *)
		    malloc(ROUNDUP(mem_header->ar_size))) == NULL) {
			error_message(MALLOC_ERROR,
			PLAIN_ERROR, (char *)0,
			prog);
			mcs_exit(FAILURE);
		}

		if (lseek(fd, elf_getbase(elf), 0) != elf_getbase(elf)) {
			error_message(WRITE_MANI_ERROR,
			prog, cur_file);
			mcs_exit(FAILURE);
		}

		if (read(fd, file_buf,
		    (unsigned) ROUNDUP(mem_header->ar_size)) !=
		    (unsigned) ROUNDUP(mem_header->ar_size)) {
			error_message(READ_MANI_ERROR,
			SYSTEM_ERROR, strerror(errno),
			prog, cur_file);
			mcs_exit(FAILURE);
		}
		if (write(fdartmp,
		    file_buf,
		    (unsigned) ROUNDUP(mem_header->ar_size)) !=
		    (unsigned) ROUNDUP(mem_header->ar_size)) {
			error_message(WRITE_MANI_ERROR,
			SYSTEM_ERROR, strerror(errno),
			prog, cur_file);
			mcs_exit(FAILURE);
		}
		free(file_buf);
	} else if (CHK_OPT(cmd_info, MIGHT_CHG)) {
		error_message(SYM_TAB_AR_ERROR,
		PLAIN_ERROR, (char *)0,
		prog, cur_file);
		error_message(EXEC_AR_ERROR,
		PLAIN_ERROR, (char *)0,
		cur_file);
	}
}

static void
copy_file(fname, temp_file_name)
char *fname;
char *temp_file_name;
{
	register int i;
	char *temp_fname;
	int fd;
	int fdtmp2;
	struct stat stbuf;
	char *buf;
	mode_t mode;

	for (i = 0; signum[i]; i++) /* started writing, cannot interrupt */
		(void) signal(signum[i], SIG_IGN);

	(void) stat(fname, &stbuf); /* for mode of original file */
	mode = stbuf.st_mode;

	if ((fdtmp2 = open(temp_file_name, O_RDONLY)) == -1) {
		error_message(OPEN_TEMP_ERROR,
		SYSTEM_ERROR, strerror(errno),
		prog, temp_file_name);
		mcs_exit(FAILURE);
	}

	(void) stat(temp_file_name, &stbuf); /* for size of file */

	if ((buf = (char *) malloc(stbuf.st_size * sizeof (char))) == NULL) {
		error_message(MALLOC_ERROR,
		PLAIN_ERROR, (char *)0,
		prog);
		mcs_exit(FAILURE);
	}

	if (read(fdtmp2, buf, (unsigned) stbuf.st_size) !=
		(unsigned) stbuf.st_size) {
		error_message(READ_SYS_ERROR,
		SYSTEM_ERROR, strerror(errno),
		prog, temp_file_name);
		mcs_exit(FAILURE);
	}

	temp_fname = get_dir(fname);

	if ((fd = open(temp_fname, O_WRONLY |
	    O_TRUNC | O_CREAT, (mode_t) 0666)) == -1) {
		error_message(OPEN_WRITE_ERROR,
		SYSTEM_ERROR, strerror(errno),
		prog, temp_fname);
		mcs_exit(FAILURE);
	}

	(void) chmod(temp_fname, mode);

	if ((write(fd, buf, (unsigned) stbuf.st_size)) !=
	    (unsigned) stbuf.st_size) {
		error_message(WRITE_MANI_ERROR,
		SYSTEM_ERROR, strerror(errno),
		prog, temp_fname, fname);
		(void) unlink(temp_fname);
		mcs_exit(FAILURE);
	}

	free(buf);
	(void) close(fdtmp2);
	(void) close(fd);
	(void) unlink(temp_file_name); 	/* temp file */
	(void) unlink(fname); 		/* original file */
	(void) link(temp_fname, fname);
	(void) unlink(temp_fname);
}

static int
location(int offset, int mem_search, Elf32_Ehdr *ehdr)
{
	int i;
	int upper;


	for (i = 0; i < (int)ehdr->e_phnum; i++) {
		if (mem_search)
			upper = b_e_seg_table[i].p_memsz;
		else
			upper = b_e_seg_table[i].p_filesz;
		if ((offset >= b_e_seg_table[i].p_offset) &&
		    (offset <= upper))
			return (IN);
		else if (offset < b_e_seg_table[i].p_offset)
			return (PRIOR);
	}
	return (AFTER);
}

static int
shdr_location(Elf32_Shdr * shdr, Elf32_Ehdr *ehdr)
{
	/*
	 * If the section is not a NOTE section and it has no
	 * virtual address then it is not part of a mapped segment.
	 */
	if (shdr->sh_addr == 0)
		return (location(shdr->sh_offset + shdr->sh_size, 0, ehdr));

	return (location(shdr->sh_offset + shdr->sh_size, 1, ehdr));
}

static void
initialize(Elf32_Ehdr *ehdr)
{
	if ((sec_table = (section_info_table *)
		calloc(ehdr->e_shnum + 1,
		sizeof (section_info_table))) == NULL) {
		error_message(MALLOC_ERROR,
		PLAIN_ERROR, (char *)0,
		prog);
		exit(FAILURE);
	}

	if ((off_table = (Elf32_Off *)
		calloc(ehdr->e_shnum,
		sizeof (Elf32_Off))) == NULL) {
		error_message(MALLOC_ERROR,
		PLAIN_ERROR, (char *)0,
		prog);
		exit(FAILURE);
	}

	if ((nobits_table = (int *)
		calloc(ehdr->e_shnum, sizeof (int))) == NULL) {
		error_message(MALLOC_ERROR,
		PLAIN_ERROR, (char *)0,
		prog);
		exit(FAILURE);
	}
}

static char *
get_dir(char *pathname)
{
	char *directory;
	char *p;
	unsigned int position = 0;

	if ((p = strrchr(pathname, '/')) == NULL) {

		return (mktemp("./mcs3XXXXXX"));
	} else {
		p++;
		position = strlen(pathname) - strlen(p);
		if ((directory = (char *)malloc(position+10+1)) == NULL) {
			error_message(MALLOC_ERROR,
			PLAIN_ERROR, (char *)0,
			prog);
			mcs_exit(FAILURE);
		}
		(void) strncpy(directory, pathname, position);
		directory[position] = '\0';
		(void) strcat(directory, "mcs3XXXXXX");

		return (mktemp(directory));
	}
}
