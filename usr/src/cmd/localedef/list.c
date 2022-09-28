/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)list.c 1.3	96/06/28  SMI"

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
static char rcsid[] = "@(#)$RCSfile: list.c,v $ $Revision: 1.3.2.3 $ (OSF) $Date: 1992/02/18 20:25:16 $";
#endif
/*
 * COMPONENT_NAME: (CMDLOC) Locale Database Commands
 *
 * FUNCTIONS: create_list, create_list_element, add_list_element, 
 *            loc_list_element.
 *
 *
 * (C) COPYRIGHT International Business Machines Corp. 1991
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 * 
 * 1.1  com/cmd/nls/list.c, bos320 5/2/91 16:01:41
 */

#include "list.h"
#include "err.h"

/*
*  FUNCTION: create_list
*
*  DESCRIPTION:
*  Create a linked list.  The routine takes as an argument the comparator 
*  which is used to add items to the list.  List elements are added in
*  sorted order.
*/
list_t *
create_list( int (*comparator)() )
{
  list_t *l;

  l = MALLOC(list_t, 1);

  l->comparator = comparator;
  l->head.next = NULL;

  return l;
}


/*
*  FUNCTION: create_list_element
*
*  DESCRIPTION:
*  Create an instance of a list element.  'key' is what will be passwd
*  to the comparator function if the list item is added to a list, and 
*  'data' is caller defined.
*/
listel_t *
create_list_element(void *key, void *data)
{
  listel_t *el;

  el = MALLOC(listel_t, 1);
  el->key = key;
  el->data = data;
  el->next = NULL;

  return el;
}


/*
*  FUNCTION: add_list_element
*
*  DESCRIPTION:
*  Adds the list element 'el' to the linked list 'l'.  The function uses the
*  element key and the comparator for the list to add the element in 
*  sorted order.
*/
int
add_list_element(list_t *l, listel_t *el)
{
  int      c;
  listel_t *lp;

  c = -1;
  for (lp = &(l->head); 
       lp->next != NULL && 
         (c=(*(l->comparator))(lp->next->key, el->key)) < 0;
       lp = lp->next);

  if (c==0)
    return -1;
  else {
    el->next = lp->next;
    lp->next = el;
    return 0;
  }
}


/*
*  FUNCTION: loc_list_element
*
*  DESCRIPTION:
*  Locates the first list element in list 'l', with a key matching 'key'. 
*  The list comparator will be used to determine key equivalence.
*/
listel_t *
loc_list_element(list_t *l, void *key)
{
  int      c;
  listel_t *lp;

  c = -1;
  for (lp = &(l->head); 
       lp->next != NULL && 
         (c=(*(l->comparator))(lp->next->key, key)) < 0;
       lp = lp->next);

  if (c!=0)
    return NULL;
  else
    return lp->next;
}

