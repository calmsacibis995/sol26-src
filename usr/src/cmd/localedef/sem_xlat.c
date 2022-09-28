/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)sem_xlat.c 1.8	96/06/28  SMI"

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
static char rcsid[] = "@(#)$RCSfile: sem_xlat.c,v $ $Revision: 1.4.2.3 $ (OSF) $Date: 1992/02/18 20:26:08 $";
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
 * 1.4  com/cmd/nls/sem_xlat.c, cmdnls, bos320, 9138320 9/11/91 16:35:09
 */

#include <sys/localedef.h>
#include <string.h>
#include "semstack.h"
#include "err.h"
#include "locdef.h"

/*
*  FUNCTION: add_upper
*
*  DESCRIPTION:
*  Build the 'upper' character translation tables from the symbols on the
*  semantic stack.
*/
void
add_upper(_LC_ctype_t *ctype)
{
    extern wchar_t max_wchar_enc;
    item_t *it;
    int i;

    /* check if upper array allocated yet - allocate if NULL */
    if (ctype->upper == NULL)
	ctype->upper = MALLOC(wchar_t, max_wchar_enc+1);

    /* set up default translations - which is identity */
    for (i=0; i <= max_wchar_enc; i++)
	ctype->upper[i] = i;

    /* for each range on stack - the min is the FROM pc, and the max is */
    /* the TO pc.*/
    while ((it = sem_pop()) != NULL) {
	ctype->upper[it->value.range->min] = it->value.range->max;
    }
}    


/*
*  FUNCTION: add_lower
*
*  DESCRIPTION:
*  Build the 'lower' character translation tables from the symbols on the
*  semantic stack.
*/
void
add_lower(_LC_ctype_t *ctype)
{
    extern wchar_t max_wchar_enc;
    item_t *it;
    int i;

    /* check if lower array allocated yet - allocate if NULL */
    if (ctype->lower == NULL)
	ctype->lower = MALLOC(wchar_t, max_wchar_enc+1);

    /* set up default translations which is identity */
    for (i=0; i <= max_wchar_enc; i++)
	ctype->lower[i] = i;

    /* for each range on stack - the min is the FROM pc, and the max is */
    /* the TO pc.*/
    while ((it = sem_pop()) != NULL) {
	ctype->lower[it->value.range->min] = it->value.range->max;
    }
}	      

/* 
*  FUNCTION: sem_push_xlat
*
*  DESCRIPTION:
*  Creates a character range item from two character reference items.
*  The routine pops two character reference items off the semantic stack.
*  These items represent the "to" and "from" pair for a character case
*  translation.  The implementation uses a character range structure to
*  represent the pair.
*/
void
sem_push_xlat(void)
{
  item_t   *it0, *it1;
  item_t   *it;
  it1 = sem_pop();		/* this is the TO member of the pair */
  it0 = sem_pop();		/* this is the FROM member of the pair */

  /* this creates the item and sets the min and max to wc_enc */\

  if (it0->type == it1->type)	/* Same type is easy case */
    switch(it0->type) {
      case SK_CHR:
	it = create_item(SK_RNG, it0->value.chr->wc_enc, it1->value.chr->wc_enc);
	break;
      case SK_INT:
	it = create_item(SK_RNG, it0->value.int_no, it1->value.int_no);
	break;
      default:
	INTERNAL_ERROR;		/* NEVER RETURNS */
    }
  /*
   * Not same types, we can coerce INT and CHR into a valid range
   */
  else if (it0->type == SK_CHR && it1->type == SK_INT)
     it = create_item(SK_RNG, it0->value.chr->wc_enc, it1->value.int_no);
  else if (it0->type == SK_INT && it1->type == SK_CHR)
     it = create_item(SK_RNG, it0->value.int_no, it1->value.chr->wc_enc);
  else
    INTERNAL_ERROR;

  destroy_item(it1);
  destroy_item(it0);

  sem_push(it);
}

void
add_transformation(_LC_ctype_t *ctype, struct lcbind_table *lcbind_table,
		   char *ctype_symbol_name)
{
    extern wchar_t max_wchar_enc;
    item_t *it;
    int i;

    /* check if array allocated yet - allocate if NULL */
    if (ctype->transname == (_LC_transnm_t *) NULL) {
	ctype->transname = MALLOC(_LC_transnm_t, 20);
	ctype->transtabs = MALLOC(wchar_t *, 20);
    }

/* craigm - if it's full reallocate it
    if (ctype->ntrans == 20) {
	realloc
    }
*/

/* craigm
    lookup existing transname entry and add to it
*/
    /* allocate transtab vector */
    if (ctype->transtabs[ctype->ntrans] == (wchar_t *) NULL) {

	ctype->transname[ctype->ntrans].tmax = max_wchar_enc;

	if ( ((strcmp("toupper", ctype_symbol_name) == 0) ||
	      (strcmp("tolower", ctype_symbol_name) == 0)) &&
	      (ctype->transname[ctype->ntrans].tmax < 255))
	    ctype->transname[ctype->ntrans].tmax = 255;

	ctype->transname[ctype->ntrans].tmin = 0;
	ctype->transtabs[ctype->ntrans] =
		MALLOC(wchar_t, ctype->transname[ctype->ntrans].tmax+1);
	ctype->transname[ctype->ntrans].name = strdup(ctype_symbol_name);
	ctype->transname[ctype->ntrans].index = ctype->ntrans;
    }

    /* set up default translations which is identity */
    for (i=0; i <= ctype->transname[ctype->ntrans].tmax; i++)
	ctype->transtabs[ctype->ntrans][i] = i;

    /* for each range on stack - the min is the FROM pc, and the max is */
    /* the TO pc.*/
    while ((it = sem_pop()) != NULL) {
	ctype->transtabs[ctype->ntrans][it->value.range->min] =
							it->value.range->max;
    }

    /*
     * Search the translation for the last character that is case sensitive
     */
    for (i=ctype->transname[ctype->ntrans].tmax; i>0; i--)
	if (i != ctype->transtabs[ctype->ntrans][i])
	    break;
 
    ctype->transname[ctype->ntrans].tmax = i;
 
    /* Check to see if there is value greater than tmax */
    for (; i >= 0; i--)
	if (ctype->transname[ctype->ntrans].tmax <
					ctype->transtabs[ctype->ntrans][i])
	    ctype->transname[ctype->ntrans].tmax =
					ctype->transtabs[ctype->ntrans][i];

    /*
     * Search the translation for the first character that is case sensitive
     */

    for (i=0; i <= ctype->transname[ctype->ntrans].tmax; i++)
	if (i != ctype->transtabs[ctype->ntrans][i])
	    break;

    ctype->transname[ctype->ntrans].tmin = i;

    /* Check to see if there is a value smaller than tmin */
    for (; i <= ctype->transname[ctype->ntrans].tmax; i++)
	if (ctype->transtabs[ctype->ntrans][i] <
					ctype->transname[ctype->ntrans].tmin)
	    ctype->transname[ctype->ntrans].tmin =
					ctype->transtabs[ctype->ntrans][i];
    /*
     * Do the same for the low end but NOT for "toupper" and "tolower"
     */
    if (strcmp("toupper", ctype->transname[ctype->ntrans].name) == 0) {
	if (ctype->transname[ctype->ntrans].tmax < 255)
	    ctype->transname[ctype->ntrans].tmax = 255;
	ctype->transname[ctype->ntrans].tmin = 0;
	ctype->max_upper = ctype->transname[ctype->ntrans].tmax;
	ctype->upper     = ctype->transtabs[ctype->ntrans];
    } else if (strcmp("tolower", ctype->transname[ctype->ntrans].name) == 0) {
	if (ctype->transname[ctype->ntrans].tmax < 255)
	    ctype->transname[ctype->ntrans].tmax = 255;
	ctype->transname[ctype->ntrans].tmin = 0;
	ctype->max_lower = ctype->transname[ctype->ntrans].tmax;
	ctype->lower     = ctype->transtabs[ctype->ntrans];
    }

    ctype->ntrans++;
}
