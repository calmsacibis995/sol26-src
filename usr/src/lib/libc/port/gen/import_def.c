/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)import_def.c	1.5	92/07/14 SMI"	/* SVr4.0 1.1	*/

#include "synonyms.h"
#include <stdlib.h>

VOID * (* _libc_malloc)() = &malloc;
VOID * (* _libc_realloc)() = &realloc;
VOID * (* _libc_calloc)() = &calloc;
void (* _libc_free)() = &free;
