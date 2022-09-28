/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)elf_notes.h	1.1	96/06/18 SMI"

#define	TRUE	1
#define	FALSE	0

typedef struct {
	Elf32_Nhdr	nhdr;
	char		name[8];
} Elf32_Note;

extern void elfnote(int dfd, int type, char *ptr, int size);

int setup_note_header(Elf32_Phdr *, int nlwp, char *pdir, pid_t pid);
int write_elfnotes(int nlwp, int dfd);
void cancel_notes(void);

extern int setup_old_note_header(Elf32_Phdr *, int nlwp, char *pdir, pid_t pid);
extern int write_old_elfnotes(int nlwp, int dfd);
extern void cancel_old_notes(void);
