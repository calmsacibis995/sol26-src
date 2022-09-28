
/*
 * Copyright (c) 1993-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)genalign.s	1.6	96/07/09 SMI"

#if defined(lint)
#include <sys/types.h>
#endif

#include <sys/machparam.h>

#if defined(lint)
void
elfnote()
{}
#else lint

#include "assym.s"

	!
	! this little hack generates a .note section where we tell
	! the booter what alignment we want
	!
	.section	".note"
	.align		4
	.word		.name_end - .name_begin
	.word		.desc_end - .desc_begin
	.word		ELF_NOTE_PAGESIZE_HINT
.name_begin:
	.asciz		ELF_NOTE_SOLARIS
.name_end:
	.align		4
	!
	! The pagesize is the descriptor.
	!
.desc_begin:
	.word		MMU_PAGESIZE4M
.desc_end:
	.align		4

#endif	/* lint */
