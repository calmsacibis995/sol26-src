#include <sys/elf_notes.h>

#pragma ident  "@(#)notes.s  1.5     96/05/29 SMI"

#if defined(lint)
#include <sys/types.h>
#else


!
! Sun defined names for modules to be packed ppc version
! None yet
!

!
! Define the size for the packing pool
!
	.section        .note
	.align          4
	.4byte           .name1_end - .name1_begin 
	.4byte           .desc1_end - .desc1_begin
	.4byte		ELF_NOTE_MODULE_SIZE
.name1_begin:
	.string         ELF_NOTE_PACKSIZE
.name1_end:
	.align          4
!
! The size is 190 pages
!
.desc1_begin:
	.4byte		0xbe
.desc1_end:
	.align		4
!
! Tag a group of modules as necessary for packing
!
	.section        .note
	.align          4
	.4byte          .name2_end - .name2_begin
	.4byte          .desc2_end - .desc2_begin
	.4byte		ELF_NOTE_MODULE_PACKING
.name2_begin:
	.string         ELF_NOTE_PACKHINT
.name2_end:
	.align          4
!
! These are the modules defined in packing.h
!
.desc2_begin:
.desc2_end:
	.align          4
 
#endif
