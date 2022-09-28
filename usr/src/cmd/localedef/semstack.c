/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)semstack.c 1.3	96/06/28  SMI"

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
static char rcsid[] = "@(#)$RCSfile: semstack.c,v $ $Revision: 1.3.2.3 $ (OSF) $Date: 1992/02/18 20:26:12 $";
#endif
/*
 * COMPONENT_NAME: (CMDLOC) Locale Database Commands
 *
 * FUNCTIONS:
 *
 *
 * (C) COPYRIGHT International Business Machines Corp. 1991
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 * 
 * 1.3  com/cmd/nls/semstack.c, cmdnls, bos320 6/20/91 01:00:59
 */

#include "semstack.h"
#include "err.h"

static int stack_top=SK_MAX_DEPTH;
static item_t *stack[SK_MAX_DEPTH];


/*
*  FUNCTION: sem_push
*
*  DESCRIPTION:
*  Pushes the item 'i' onto the semantic stack.
*/
int
sem_push(item_t *i)
{
    if (stack_top > 0) {
	stack[--stack_top] = i;
	return SK_OK;
    } else
	INTERNAL_ERROR;
}


/*
*  FUNCTION: sem_pop
*
*  DESCRIPTION:
*  Removes the item from the top of the stack and returns it's address
*  to the caller.
*/
item_t *
sem_pop(void)
{
    if (stack_top < SK_MAX_DEPTH)
	return stack[stack_top++];
    else 
	return NULL;
}


/*
*  FUNCTION: create_item
*
*  DESCRIPTION:
*  Creates a typed 'item_t' suitable for pushing on the semantic stack.
*  A value is assigned from arg_list based on the 'type' of the item being 
*  created.
*
*  This routine performs a malloc() to acquire memory necessary to hold
*  string and range data types.
*/
item_t *
create_item(int type, ... )
{
    va_list ap;
    item_t *i;
    char   *s;
    
    va_start(ap, type);

    i = MALLOC(item_t, 1);
    i->type = type;
    
    switch (type) {
      case SK_INT:
	i->value.int_no = va_arg(ap, int);
	break;

      case SK_STR:
	s = va_arg(ap, char *);
	i->value.str = MALLOC(char, strlen(s)+1);
	strcpy(i->value.str, s);
	break;

      case SK_RNG:
	i->value.range = MALLOC(range_t, 1);
	i->value.range->min = va_arg(ap, int);
	i->value.range->max = va_arg(ap, int);
	break;

      case SK_CHR:
	/* 
	  make sure symbol data is globally consistent by not copying symbol
	  passed but referencing it directly.
	*/
	i->value.chr = va_arg(ap,chr_sym_t *);
	break;

      case SK_SUBS:
	i->value.subs = va_arg(ap, _LC_subs_t *);
	break;

      case SK_SYM:
	i->value.sym = va_arg(ap, symbol_t *);
	break;
      default:
	INTERNAL_ERROR;
    }

    va_end(ap);

    return i;
}


/*
*  FUNCTION: destroy_item
*
*  DESCRIPTION:
*  Destroys an item created with create_item.  
*
*  This routine free()s memory for the string and range data types.  All 
*  semantic stack items should therefore be created with 'create_item' to
*  ensure malloc()ed memory integrity.  
*/
void
destroy_item(item_t *i)
{
    switch (i->type) {
      case SK_INT:
	break;
      case SK_STR:
	free(i->value.str);
	break;
      case SK_RNG:
	free(i->value.range);
	break;
      case SK_SUBS:
      case SK_SYM:
      case SK_CHR:
	/* don't free pointer to symbol data, create_item() did not malloc
	   this memory */
	break;
      default:
	INTERNAL_ERROR;
    }
    
    free(i);
}
