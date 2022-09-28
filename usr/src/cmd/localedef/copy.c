/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)copy.c 1.5	96/06/28  SMI"

/*
 * COPYRIGHT NOTICE
 * 
 * This source code is designated as Restricted Confidential Information
 * and is subject to special restrictions in a confidential disclosure
 * agreement between HP, IBM, SUN, NOVELL and OSF.  Do not distribute
 * this source code outside your company without OSF's specific written
 * approval.  This source code, and all copies and derivative works
 * thereof, must be returned or destroyed at request. You must retain
 * this notice on any copies which you make.
 * 
 * (c) Copyright 1990, 1991, 1992, 1993 OPEN SOFTWARE FOUNDATION, INC.
 * ALL RIGHTS RESERVED 
 */
/*
 * OSF/1 1.2
 */
#if !defined(lint) && !defined(_NOIDENT)
static char rcsid[] = "@(#)$RCSfile: copy.c,v $ $Revision: 1.1.2.2 $ (OSF) $Date: 1992/08/10 14:43:44 $";
#endif

#include <sys/localedef.h>
#include "semstack.h"
#include "err.h"
#include <locale.h>

int 	copying_collate = 0;	/* are we copying a collation table */
int 	copying_ctype = 0;	/* are we copying a ctype table */
int	copying = 0;		/* general flag to indicate copying */

extern _LC_collate_t	*collate_ptr;
extern _LC_ctype_t	*ctype_ptr;
extern _LC_monetary_t	*monetary_ptr;
extern _LC_numeric_t	*numeric_ptr;
extern _LC_time_t	*lc_time_ptr;
extern _LC_messages_t	*messages_ptr;

/*
 * Copy_locale  - routine to copy section of locale input files
 * 		   from an existing, installed, locale.
 *
 * 	  We reassign pointers so gen() will use the existing structures.
 */

void
copy_locale(int category)
{
	char *ret;
	item_t	*it;
	char *source;		/* user provided locale to copy from */
	char *orig_loc;		/* orginal locale */

	it = sem_pop();
	if (it->type != SK_STR)
		INTERNAL_ERROR;
	source = it->value.str;

	orig_loc = setlocale(category, NULL);
	if ((ret = setlocale(category, source)) == NULL) 
		error(CANT_LOAD_LOCALE, source);

	copying = 1;			/* make sure gen() puts out C code */
	switch(category) {

		case LC_COLLATE:
		collate_ptr = __lc_collate;
		copying_collate = 1;		/* to avoid re-compressing */
		break;

		case LC_CTYPE:
		ctype_ptr = __lc_ctype;
		copying_ctype = 1;		/* to use max_{upper,lower} */
		break;

		case LC_MONETARY:
		monetary_ptr = __lc_monetary;
		break;

		case LC_NUMERIC:
		numeric_ptr = __lc_numeric;
		break;

		case LC_TIME:
		lc_time_ptr = __lc_time;
		break;

		case LC_MESSAGES:
		messages_ptr = __lc_messages;
		break;
	}

	ret = setlocale(category, orig_loc);
}
