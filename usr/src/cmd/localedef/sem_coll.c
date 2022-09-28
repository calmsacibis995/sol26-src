/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)sem_coll.c 1.16	96/07/22  SMI"

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
static char rcsid[] = "@(#)$RCSfile: sem_coll.c,v $ $Revision: 1.4.4.2 $ (OSF) $Date: 1992/10/27 01:54:10 $";
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
 * 1.13  com/cmd/nls/sem_coll.c, cmdnls, bos320, 9137320a 9/6/91 11:41:08
 */

#include <stdlib.h>
#include <sys/localedef.h>
#include "err.h"
#include "locdef.h"
#include "symtab.h"
#include "semstack.h"
#include "method.h"

symbol_list* forw_ref_head = (symbol_list*)NULL;

extern _LC_charmap_t	charmap;

/*
  Regular expression special characters.
*/
#define REGX_SPECIALS  "[*+.?({|^$"

/*
*  FUNCTION: nxt_coll_wgt
*
*  DESCRIPTION:
*  Collation weights cannot have a zero in either the first or second
*  byte (assuming two byte wchar_t).
*/
static
wchar_t nxt_coll_wgt(void)
{
    static wchar_t nxt_coll_val = 0x01010101;	/* No null bytes permitted */
    wchar_t collval;
    extern	_LC_collate_t	collate;

    collval = nxt_coll_val;
    nxt_coll_val++;
    if (nxt_coll_val > INT_MAX)
	INTERNAL_ERROR;
    
    if ((nxt_coll_val & 0x000000ff) == 0)
	nxt_coll_val |= 0x01;
    if ((nxt_coll_val & 0x0000ff00) == 0)
	nxt_coll_val |= 0x0100;
    if ((nxt_coll_val & 0x00ff0000) == 0)
	nxt_coll_val |= 0x010000;

    collate.co_col_max = collval;		/* Remember biggest */
    
    return collval;
}


/*
*  FUNCTION: set_coll_wgt
*
*  DESCRIPTION:
*  Sets the collation weight for order 'ord' in weight structure 'weights'
*  to 'weight'.  If 'ord' is -1, all weights in 'weights' are set to 
*  'weight'.
*/
void set_coll_wgt(_LC_weight_t *weights, wchar_t weight, int ord)
{
    extern _LC_collate_t collate;
    int i;

    if (collate.co_nord < _COLL_WEIGHTS_INLINE) {

	if (ord < 0) {
	    for (i=0; i <= (int)((unsigned int)collate.co_nord); i++)
		weights->n[i] = weight;
	} else
	    if (ord >= 0 && ord <= (int)((unsigned int)collate.co_nord))
		weights->n[ord] = weight;
	    else
		INTERNAL_ERROR;

    } else {
	int i;

	/* check if weights array has been allocated yet */
	if (weights->p == NULL) {
	    weights->p = MALLOC(wchar_t, collate.co_nord + 1);
	    for (i=0; i<=(int)((unsigned int)collate.co_nord); i++)
		weights->p[i] = UNDEFINED;
	}

	if (ord < 0) {
	    for (i=0; i <= (int)((unsigned int)collate.co_nord); i++)
		weights->p[i] = weight;
	} else
	    if (ord >= 0 && ord <= (int)((unsigned int)collate.co_nord))
		weights->p[ord] = weight;
	    else
		INTERNAL_ERROR;
    }
}


/*
*  FUNCTION: get_coll_wgt
*
*  DESCRIPTION:
*  Gets the collation weight for order 'ord' in weight structure 'weights'.
*/
wchar_t get_coll_wgt(_LC_weight_t *weights, int ord)
{
    extern _LC_collate_t collate;

    if (collate.co_nord < _COLL_WEIGHTS_INLINE) {

	if (ord >= 0 && ord <= (int)((unsigned int)collate.co_nord))
	    return weights->n[ord];
	else
	    error(ERR_TOO_MANY_ORDERS);

    } else {

	/* check if weights array has been allocated yet */
	if (weights->p == NULL)
	    INTERNAL_ERROR;

	if (ord >= 0 && ord <= (int)((unsigned int)collate.co_nord))
	    return weights->p[ord];
	else
	    error(ERR_TOO_MANY_ORDERS);
    }
/*NOTREACHED*/
    return 0;
}


/* 
*  FUNCTION: sem_init_colltbl
*
*  DESCRIPTION:
*  Initialize the collation table.  This amounts to setting all collation
*  values to IGNORE, assigning the default collation order (which is 1), 
*  allocating memory to contain the table.
*/
void
sem_init_colltbl(void)
{
    extern wchar_t max_wchar_enc;
    extern _LC_collate_t collate;

    
    /* initialize collation attributes to defaults */
    collate.co_nord   = 0;		/* potentially modified by 'order' */
    collate.co_wc_min = 0;		/* always 0                        */
    collate.co_wc_max = max_wchar_enc;	/* always max_wchar_enc            */
    
    /* allocate and zero fill memory to contain collation table */
    collate.co_coltbl = MALLOC(_LC_coltbl_t, max_wchar_enc+1);
    if (collate.co_coltbl == NULL)
	INTERNAL_ERROR;

    /* set default min and max collation weights */
    collate.co_col_min = collate.co_col_max = 0;

    /* initialize substitution strings */
    collate.co_nsubs = 0;
    collate.co_subs  = NULL;
}


/* 
*  FUNCTION: sem_push_collel();
*  DESCRIPTION:
*  Copies a symbol from the symbol stack to the semantic stack.
*/
void
sem_push_collel()
{
    symbol_t *s;
    item_t   *i;

    s = sym_pop();
    i = create_item(SK_SYM, s);
    sem_push(i);
}


/*
*  FUNCTION: loc_collel
*
*  DESCRIPTION: 
*  Locates a collation element in an array of collation elements.  This
*  function returns the first collation element which matches 'sym'.
*/
_LC_collel_t *
loc_collel(char *sym, wchar_t pc)
{
    extern _LC_collate_t collate;
    _LC_collel_t *ce;

    for (ce = collate.co_coltbl[pc].ct_collel; ce->ce_sym != NULL; ce++) {
	if (strcmp(sym, ce->ce_sym)==0)
	    return ce;
    }
    
    INTERNAL_ERROR;
/*NOTREACHED*/
}


/*
*  FUNCTION: sem_coll_sym_ref
*
*  DESCRIPTION:
*  checks that the symbol referenced has a collation weights structure
*  attached.  If one is not yet present, one is allocated as necessary 
*  for the symbol type.
*/
void
sem_coll_sym_ref(void)
{
    extern _LC_collate_t collate;
    _LC_collel_t *ce;
    symbol_t     *s;

    /* Pop symbol specified off of symbol stack
    */
    s = sym_pop();

    /* 
      Check that this element has a weights array with
      the correct number of orders.  If the element does not, create
      one and initialize the contents to UNDEFINED.
      */
    switch (s->sym_type) {
      case ST_CHR_SYM:
	if (s->data.chr->wgt == NULL)
	    s->data.chr->wgt = 
		&(collate.co_coltbl[s->data.chr->wc_enc].ct_wgt);

	if (collate.co_nord >= _COLL_WEIGHTS_INLINE) {
	    if (s->data.chr->wgt->p == NULL) {
		s->data.chr->wgt->p = MALLOC(wchar_t, collate.co_nord);
		set_coll_wgt(s->data.chr->wgt, UNDEFINED, -1);
	    }
	}
	break;

      case ST_COLL_ELL:
	ce = loc_collel(s->data.collel->sym, s->data.collel->pc);
	if (collate.co_nord >= _COLL_WEIGHTS_INLINE) {
	    if (ce->ce_wgt.p == NULL) {
		ce->ce_wgt.p = MALLOC(wchar_t, collate.co_nord);
		set_coll_wgt(&(ce->ce_wgt), UNDEFINED, -1);
	    }
	}
	break;

      case ST_COLL_SYM:
	if (collate.co_nord >= _COLL_WEIGHTS_INLINE) {
	    if (s->data.collsym->p == NULL) {
		    s->data.collsym->p = MALLOC(wchar_t, collate.co_nord);
		set_coll_wgt(s->data.collsym, UNDEFINED, -1);
	    }
	}
	break;
      default:
	INTERNAL_ERROR;
    }

    sym_push(s);
}


/*
*  FUNCTION: sem_coll_literal_ref
*  
*  DESCRIPTION:
*  A character literal is specified as a collation element.  Take this
*  element and create a dummy symbol for it.  The dummy symbol is pushed
*  onto the symbol stack, but is not added to the symbol table.
*/
void
sem_coll_literal_ref(void)
{
    extern int mb_cur_max;
    extern int max_disp_width;
    symbol_t *dummy;
    item_t   *it;
    wchar_t  pc;
    int      rc;
    int      fc;
    

    /* Pop the file code to use as character off the semantic stack. */
    it = sem_pop();
    fc = it->value.int_no;

    /* Create a dummy symbol with this byte list as its encoding. */
    dummy = MALLOC(symbol_t, 1);
    dummy->sym_type = ST_CHR_SYM;
    dummy->sym_scope = 0;
    dummy->data.chr = MALLOC(chr_sym_t, 1);

    /* save file code for character */
    dummy->data.chr->fc_enc = fc;
    
    /* use hex translation of file code for symbol id (for errors) */
    dummy->sym_id = MALLOC(char, 8*2+3);	/* '0x' + 8 digits + '\0' */
    sprintf(dummy->sym_id, "0x%x", fc);

    /* save length of character */
    dummy->data.chr->len = mbs_from_fc(dummy->data.chr->str_enc, fc);

    /* check if characters this long are valid */
    if (dummy->data.chr->len > mb_cur_max)
	error(ERR_CHAR_TOO_LONG, dummy->sym_id);

    /* define process code for character literal */
    rc = INT_METHOD(METH_OFFS(CHARMAP_MBTOWC_AT_NATIVE))(&charmap, &pc,
					      dummy->data.chr->str_enc,
					      MB_LEN_MAX);
    if (rc < 0)
	error(ERR_UNSUP_ENC, dummy->sym_id);

    dummy->data.chr->wc_enc = pc;
    
    /* define width for character */
    rc = INT_METHOD(METH_OFFS(CHARMAP_WCWIDTH_AT_NATIVE))(&charmap, pc);
    if (rc > max_disp_width)
	max_disp_width = rc;

    dummy->data.chr->width = rc;

    /* clear out wgt and subs_str pointers */
    dummy->data.chr->wgt = NULL;
    dummy->data.chr->subs_str = NULL;

    /* mark character as defined */
    define_wchar(pc);

    destroy_item(it);
    sym_push(dummy);
}


/* 
*  FUNCTION: sem_def_substr
*
*  DESCRIPTION:
*  Defines a substitution string.
*/
void
sem_def_substr(void)
{
    extern _LC_collate_t collate;
    item_t     *it0, *it1;
    char       *src, *tgt;
    _LC_subs_t *subs;
    int        i, j;
    int        flag;

    it1 = sem_pop();		/* target string */
    it0 = sem_pop();		/* source string */

    if (it1->type != SK_STR || it0->type != SK_STR)
	INTERNAL_ERROR;

    /* allocate space for new substitution string */
    subs = MALLOC(_LC_subs_t, collate.co_nsubs+1);

    /* Translate and allocate space for source string */
    src = copy_string(it0->value.str);
    
    /* Translate and allocate space for target string */
    tgt = copy_string(it1->value.str);

    /* Initialize substitution flag */
    flag = _SUBS_ACTIVE;
    
    /* check source and destination strings for regular expression
       special characters. If special characters are found then enable
       regular expressions in substitution string.
       */
    if (strpbrk(src, REGX_SPECIALS) != NULL)
      flag |= _SUBS_REGEXP;
    else if (strpbrk(tgt, REGX_SPECIALS) != NULL)
      flag |= _SUBS_REGEXP;

    /* Add source and target strings to newly allocated substitute list */
    for (i=0,j=0; i< (int)((unsigned int)collate.co_nsubs); i++,j++) {
	int   c;

	c = strcmp(src, collate.co_subs[i].ss_src);
	if (c < 0 && i == j) {
	    subs[j].ss_src = src;
	    subs[j].ss_tgt = tgt;
	    set_coll_wgt(&(subs[j].ss_act), flag, -1);
	    j++;
	} 
	subs[j].ss_src = collate.co_subs[i].ss_src;
	subs[j].ss_tgt = collate.co_subs[i].ss_tgt;
	subs[j].ss_act = collate.co_subs[i].ss_act;
    }
    if (i==j) {			
	/* either subs was empty or new substring is greater than any other
	   to date */
	subs[j].ss_src = src;
	subs[j].ss_tgt = tgt;
	set_coll_wgt(&(subs[j].ss_act), flag, -1);
    }

    /* increment substitute string count */
    collate.co_nsubs++;

    /* free space occupied by old list */
    free(collate.co_subs);
    
    /* attach new list to coll table */
    collate.co_subs = subs;

    destroy_item(it0);
    destroy_item(it1);
}


/*
*  FUNCTION: sem_collel_list
*
*  DESCRIPTION:
*  Process the set of symbols now on the semantic stack for the character 
*  for this particular order.
*/
void
sem_collel_list(_LC_weight_t *w, symbol_t *tgt, int order)
{
    extern _LC_collate_t collate;
    item_t       *i;		/* count item - # of items which follow */
    item_t       *si;		/* symbol item - item containing symbol */
    _LC_collel_t *ce;
    wchar_t	 weight;
    void add_forw_reference(symbol_t*, symbol_t*, int);

    i = sem_pop();		/* pop count item */
    if (i == NULL || i->type != SK_INT)
	INTERNAL_ERROR;

    if (i->value.int_no==1) {
	/* character gets collation value of symbol */
	si = sem_pop();
	if (si == NULL || si->type != SK_SYM)
	    INTERNAL_ERROR;

	switch (si->value.sym->sym_type) {

	  case ST_CHR_SYM:		/* character */
	    weight=get_coll_wgt(si->value.sym->data.chr->wgt,order);
	    if ((weight == UNDEFINED) || (weight == SUB_STRING)) {

		/*
		  a symbol with UNDEFINED collation can only appear on the
		  RHS if it is also the target on the LHS
		*/
		if (si->value.sym == tgt) {
		    /* assign collation weight for self reference */
		    set_coll_wgt(si->value.sym->data.chr->wgt, 
				 nxt_coll_wgt(), -1);
		} else {
		    diag_error(ERR_FORWARD_REF, si->value.sym->sym_id);

		    add_forw_reference(tgt, si->value.sym, order);

		    return;
		}
	    }
	    set_coll_wgt(w, 
			 get_coll_wgt(si->value.sym->data.chr->wgt,
				      order), 
			 order);
	    break;
	    
	  case ST_COLL_ELL:	/* collation element */
	    ce = loc_collel(si->value.sym->data.collel->sym,
			    si->value.sym->data.collel->pc);
	    
	    weight=get_coll_wgt(&(ce->ce_wgt),order);
	    if ((weight == UNDEFINED) || (weight == SUB_STRING)) {
		/*
		  a symbol with UNDEFINED collation can only appear on the
		  RHS if it is also the target on the LHS
		*/
		if (si->value.sym == tgt)
		    set_coll_wgt(&(ce->ce_wgt), nxt_coll_wgt(), -1);
		else {
		    diag_error(ERR_FORWARD_REF, si->value.sym->sym_id);

		    add_forw_reference(tgt, si->value.sym, order);

		    return;
		}
	    }
	    set_coll_wgt(w, get_coll_wgt(&(ce->ce_wgt), order), order);
	    break;

	  case ST_COLL_SYM:	/* collation symbol */
	    weight=get_coll_wgt(si->value.sym->data.collsym,order);
	    if ((weight == UNDEFINED) || (weight == SUB_STRING)) {
		/*
		  a symbol with UNDEFINED collation can only appear on the
		  RHS if it is also the target on the LHS
		*/
		if (si->value.sym == tgt) {
		    set_coll_wgt(tgt->data.collsym, nxt_coll_wgt(), -1);

		} else {
		    diag_error(ERR_FORWARD_REF, si->value.sym->sym_id);

		    add_forw_reference(tgt, si->value.sym, order);

		    return;
		}
	    }
	    set_coll_wgt(w,
			 get_coll_wgt(si->value.sym->data.collsym,
				      order),
			 order);
	    break;
	  default:
	    INTERNAL_ERROR;
	}

    } else {
	/* 
	  collation substitution, i.e. <eszet>   <s><s>; <eszet>
	*/
	item_t **buf;
	item_t *xi;
	int    n;
	char   *subs;
	char   *srcs;
	char   *srcs_temp;

	/* 
	  pop all of the collation elements on the semantic stack and
	  create a string from each of their encodings.
	*/
	subs = MALLOC(char, (i->value.int_no * MB_LEN_MAX) + 1);
	buf = MALLOC(item_t *, i->value.int_no);
	for (n=0; n < i->value.int_no; n++)
	    buf[n] = sem_pop();

	for (n=i->value.int_no-1; n >= 0; n--) {
	    if (buf[n]->type == SK_SYM) 
		strncat(subs, 
			(char *)buf[n]->value.sym->data.chr->str_enc, MB_LEN_MAX);
	    else
		INTERNAL_ERROR;

	    destroy_item(buf[n]);
	}
	free(buf);

	/* 
	  Get source string from target symbol.

	  tgt->data.chr->str_enc must be run through copy_string so that
          it is in the same format as collate substring (which is also
          run through copy_string

	*/
	if (tgt->sym_type == ST_COLL_ELL) {
	    srcs_temp = MALLOC(char, strlen(tgt->data.collel->str)+1);
	    strcpy(srcs_temp, tgt->data.collel->str);
	} else {
	    srcs_temp = MALLOC(char, MB_LEN_MAX+1);
	    strncpy(srcs_temp, (char *)tgt->data.chr->str_enc, MB_LEN_MAX);
	}

	srcs = copy_string(srcs_temp);

	set_coll_wgt(w,SUB_STRING,order);

	/* Set up substring masks. */
	if (collate.co_nord < _COLL_WEIGHTS_INLINE)
		collate.co_sort.n[order] |= _COLL_SUBS_MASK;
	else
		collate.co_sort.p[order] |= _COLL_SUBS_MASK;

	/* 
	  look for the src string in the set of collation substitution
	  strings alread defined.  If it is present, then just enable
	  it for this order.
	*/
	for (n=0; n< (int)((unsigned int)collate.co_nsubs); n++) {
	    _LC_weight_t *w;

	    w = &(collate.co_subs[n].ss_act);

	    if (strcmp(srcs, collate.co_subs[n].ss_src)==0) {
		set_coll_wgt(w, 
			     get_coll_wgt(w,order) & _SUBS_ACTIVE,
			     order);
		free(srcs);
		free(srcs_temp);
		free(subs);

		return;
	    }
	}
		
	/*
	  If this substitution has never been used before, then we
          need to create a new one.  Push source and substitution
	  strings on semantic stack and then call semantic action to
	  process substitution strings.  Reset active flag for all
	  except current order.
	*/
	xi = create_item(SK_STR, srcs_temp);
	sem_push(xi);

	xi = create_item(SK_STR, subs);
	sem_push(xi);

	sem_def_substr();
	
	/*
	  locate source string in substitution string array.  After
	  you locate it, fix the substitution flags to indicate which
	  order the thing is valid for.
        */
	for (n=0; n < (int)((unsigned int)collate.co_nsubs); n++) {
	    if (strcmp(collate.co_subs[n].ss_src, srcs)==0) {
		
		/* 
		  allocate weights array if not already done. turn off 
		  substitution for all but current order.
		*/
		if (collate.co_nord >= _COLL_WEIGHTS_INLINE 
		    && collate.co_subs[n].ss_act.p==NULL) {
		    collate.co_subs[n].ss_act.p = 
			MALLOC(wchar_t, collate.co_nord+1);
		    set_coll_wgt(&(collate.co_subs[n].ss_act), 0, -1);
		}

		/*
		  set action for current order to TRUE
		*/
		set_coll_wgt(&(collate.co_subs[n].ss_act), 
			     _SUBS_ACTIVE, order);

		break;
	    }
	}
	free(srcs);
	free(srcs_temp);
	free(subs);

    } /* .....end collation substitution..... */
}


/*
*  FUNCTION: sem_set_collwgt
*
*  DESCRIPTION:
*  Assigns the collation weights in the argument 'weights' to the character
*  popped off the symbol stack.
*/
void
sem_set_collwgt(_LC_weight_t *weights)
{
    extern _LC_collate_t collate;
    symbol_t     *s;
    int          i;
    _LC_weight_t *tgt;
    _LC_collel_t *ce;

    s = sym_pop();
    switch (s->sym_type) {

      case ST_CHR_SYM:
	tgt = s->data.chr->wgt;
	if (tgt == NULL)
	    s->data.chr->wgt = 
		&(collate.co_coltbl[s->data.chr->wc_enc].ct_wgt);
	break;

      case ST_COLL_ELL:
	ce = loc_collel(s->data.collel->sym, s->data.collel->pc);
	tgt = &(ce->ce_wgt);
	break;

      case ST_COLL_SYM:
	tgt = s->data.collsym;
	break;

      default:
	INTERNAL_ERROR;
    }

    for (i=0; i<=(int)((unsigned int)collate.co_nord); i++)
	set_coll_wgt(tgt, get_coll_wgt(weights,i), i);
}


/*
*  FUNCTION: sem_get_coll_tgt
*
*  DESCRIPTION:
*  Returns a pointer to the symbol on top of the symbol stack.
*/
symbol_t *sem_get_coll_tgt(void)
{
    symbol_t *s;

    s = sym_pop();

    sym_push(s);
    return s;
}


/* 
*  FUNCTION: sem_set_dflt_collwgt
*
*  DESCRIPTION:
*  Assign collation weight to character - set weight in symbol table
*  entry and in coltbl weight array.
*
*  The collation weight assigned is the next one available, i.e. the
*  default collation weight.
*/
void
sem_set_dflt_collwgt(void)
{
    extern _LC_collate_t collate;
    wchar_t      weight;
    symbol_t     *sym;
    wchar_t      pc;
    _LC_collel_t *ce;
    
    
    sym = sym_pop();
    if (sym->sym_type != ST_CHR_SYM 
	&& sym->sym_type != ST_COLL_SYM && sym->sym_type != ST_COLL_ELL)
	INTERNAL_ERROR;

    pc = sym->data.chr->wc_enc;

    switch (sym->sym_type) {	/* handle character */
      case ST_CHR_SYM:
	/* check if character already specified elswhere */
	if (get_coll_wgt(&(collate.co_coltbl[pc].ct_wgt),0) != UNDEFINED) {
	    diag_error(ERR_DUP_COLL_SPEC, sym->sym_id);
	    return;
	}

	/* get next available collation weight */
	weight = nxt_coll_wgt();

	/* place weight in colltbl */
	set_coll_wgt(&(collate.co_coltbl[pc].ct_wgt), 
		     weight, -1);

	/* put weight in symbol table entry for character. */
	sym->data.chr->wgt = 
	    &(collate.co_coltbl[pc].ct_wgt);
	break;

      case ST_COLL_ELL:
	ce = loc_collel(sym->data.collel->sym, sym->data.collel->pc);
	
	/* check if character already specified elswhere */
	if (get_coll_wgt(&(ce->ce_wgt), 0) != UNDEFINED) {
	    diag_error(ERR_DUP_COLL_SPEC, sym->sym_id);
	    return;
	}
	
	/* get next available collation weight */
	weight = nxt_coll_wgt();
	
	/* put weights in symbol table entry for character. */
	set_coll_wgt(&(ce->ce_wgt), weight, -1);
	
	break;

      case ST_COLL_SYM:
	/* check if character already specified elswhere */
	if (get_coll_wgt(sym->data.collsym, 0) != UNDEFINED) {
	    diag_error(ERR_DUP_COLL_SPEC, sym->sym_id);
	    return;
	}

	weight = nxt_coll_wgt();
	set_coll_wgt(sym->data.collsym, weight, -1);
	break;
      default:
	INTERNAL_ERROR;
    }
}


/* 
*  FUNCTION: sem_set_dflt_collwgt_range
*
*  DESCRIPTION:
*  Assign collation weights to a range of characters.  The functions
*  expects to find two character symbols on the semantic stack.
*
*  The collation weight assigned is the next one available, i.e. the
*  default collation weight.
*/
void
sem_set_dflt_collwgt_range(void)
{
    extern _LC_collate_t collate;
    symbol_t *s1, *s0;
    int      start, end;
    wchar_t  weight;
    int      wc;
    int      i;
    
    /* 
      Issue warning message that using KW_ELLIPSES results in the use of
      codeset encoding assumptions by localedef. 

      - required by POSIX.

    diag_error(ERR_CODESET_DEP);
    */

    /* pop symbols of symbol stack */
    s1 = sym_pop();
    s0 = sym_pop();
    
    /* 
      ensure that both of these symbols are characters and not collation
      symbols or elements 
    */
    if (s1->sym_type != ST_CHR_SYM || s0->sym_type != ST_CHR_SYM) {
	diag_error(ERR_INVAL_COLL_RANGE, s0->sym_id, s1->sym_id);
	return;
    }

    /* get starting and ending points in file code */
    start = s0->data.chr->fc_enc;
    end = s1->data.chr->fc_enc;

    /* invalid symbols in range ?*/
    if (start > end)
	error(ERR_INVAL_COLL_RANGE, s0->sym_id, s1->sym_id);
	
    for (i=start; i <= end; i++) {

	if ((wc = wc_from_fc(i)) >= 0) {

	    /*
	     * check if this character is in the charmap
	     * if not then issue a warning
	     */

	    if (wchar_defined(wc) == 0) {
		diag_error(ERR_MISSING_CHAR, i);
	    }

	    /* check if already defined elsewhere in map */
	    if (get_coll_wgt(&(collate.co_coltbl[wc].ct_wgt), 
			     0) != UNDEFINED) {
		diag_error(ERR_DUP_COLL_RNG_SPEC, "<???>"  ,s0->sym_id, s1->sym_id);
		return;
	    }
	    /* get next available collation weight */
	    weight = nxt_coll_wgt();

	    /* collation weights for symbols assigned weights in a range
	       are not accessible from the symbol , i.e.

	       s->data.chr->wgt[x] = weight;

	       cannot be assigned here since we don't have the symbol
	       which refers to the file code.
	    */

	    /* put weight in coll table at spot for wchar encoding */
	    set_coll_wgt(&(collate.co_coltbl[wc].ct_wgt), weight, -1);
	    
	}
    }
}


/*
*  FUNCTION: sem_sort_spec
*
*  DESCRIPTION:
*  This function decrements the global order by one to compensate for the 
*  extra increment done by the grammar, and then copies the sort modifier
*  list to each of the substrings defined thus far.
*/
void
sem_sort_spec(void)
{
    extern _LC_collate_t collate;
    extern symtab_t cm_symtab;
    extern wchar_t max_wchar_enc;
    wchar_t    low;
    symbol_t *s;
    item_t   *it;
    int      i;
    wchar_t    *buf;

    /*
     * The number of collation orders is one-based (at this point)
     * We change it to zero based, which is what the runtime wants
     */
    collate.co_nord--;

    /*
      Get sort values from top of stack and assign to collate.co_sort
    */
    if (collate.co_nord < _COLL_WEIGHTS_INLINE) {
	for (i = collate.co_nord; i>=0; i--) {
	    it = sem_pop();
	    collate.co_sort.n[i] = it->value.int_no;
	    destroy_item(it);
	}
    } else {
	collate.co_sort.p = MALLOC(wchar_t, collate.co_nord+1);
	for (i = (int)((unsigned int)collate.co_nord); i >= 0; i--) {
	    it = sem_pop();
	    collate.co_sort.p[i] = it->value.int_no;
	    destroy_item(it);
	}

	buf = MALLOC(wchar_t, (collate.co_nord+1)*(max_wchar_enc+1));
	for (i=0; i<=max_wchar_enc; i++) {
	    collate.co_coltbl[i].ct_wgt.p = buf;
	    buf += (collate.co_nord+1);
	    set_coll_wgt(&(collate.co_coltbl[i].ct_wgt), UNDEFINED, -1);
	}
    }

    /*
      Turn off the _SUBS_ACTIVE flag for substitution strings in orders
      where this capability is disabled.
      This is now done in setup_substr called from the grammar.
    */
    /* 
      seed the symbol table with IGNORE,  <LOW> and <HIGH> 
    */

    /* 
      IGNORE gets a special collation value .  The xfrm and coll
      logic must recognize zero and skip a character possesing this collation
      value.
    */
    s = create_symbol("IGNORE", 0);
    s->sym_type = ST_CHR_SYM;
    s->data.chr = MALLOC(chr_sym_t, 1);
    s->data.chr->wc_enc = 0;
    s->data.chr->width = 0;
    s->data.chr->len = 0;
    s->data.chr->wgt = MALLOC(_LC_weight_t, 1);
    s->data.chr->wgt->p = NULL;
    set_coll_wgt(s->data.chr->wgt, IGNORE, -1);
    add_symbol(&cm_symtab, s);
    
    /*
      LOW gets the first available collation value.  All subsequent characters
      will get values higher than this unless they collate with <LOW>
    */
    s = create_symbol("<LOW>", 0);	/* <LOW> */
    s->sym_type = ST_CHR_SYM;
    s->data.chr = MALLOC(chr_sym_t, 1);
    s->data.chr->wc_enc = 0;
    s->data.chr->width = 0;
    s->data.chr->len = 0;
    s->data.chr->wgt = MALLOC(_LC_weight_t, 1);
    s->data.chr->wgt->p = NULL;
    set_coll_wgt(s->data.chr->wgt, low = nxt_coll_wgt(), -1);
    collate.co_col_min = low;
    add_symbol(&cm_symtab, s);
    /* 
      HIGH gets the maximum collation value which can be contained in a 
      wchar_t.  This ensures that all values will collate < than <HIGH>

      This is just a temporary until the last collation weight has actually
      been assigned.  We will reset every collation weight which is <HIGH>
      to the current max plus one at gen() time.
    */
    s = create_symbol("<HIGH>", 0);
    s->sym_type = ST_CHR_SYM;
    s->data.chr = MALLOC(chr_sym_t, 1);
    s->data.chr->wc_enc = 0;
    s->data.chr->width = 0;
    s->data.chr->len = 0;
    s->data.chr->wgt = MALLOC(_LC_weight_t, 1);
    s->data.chr->wgt->p = NULL;
    set_coll_wgt(s->data.chr->wgt, ULONG_MAX, -1);
    collate.co_col_max = 0;
    add_symbol(&cm_symtab, s);
    
    /*
      UNDEFINED may collate <HIGH> or <LOW>.  By default, characters not
      specified in the collation order collate <HIGH>.
    */
    s = create_symbol("UNDEFINED", 0);
    s->sym_type = ST_CHR_SYM;
    s->data.chr = MALLOC(chr_sym_t, 1);
    s->data.chr->wc_enc = 0;
    s->data.chr->width = 0;
    s->data.chr->len = 0;
    s->data.chr->wgt = MALLOC(_LC_weight_t, 1);
    s->data.chr->wgt->p = NULL;
    set_coll_wgt(s->data.chr->wgt, ULONG_MAX, -1);
    add_symbol(&cm_symtab, s);
}


/* 
*  FUNCTION: sem_def_collel
*
*  DESCRIPTION:
*  Defines a collation ellement. Creates a symbol for the collation element
*  in the symbol table, creates a collation element data structure for
*  the element and populates the element from the string on the semantic 
*  stack.
*/
void
sem_def_collel(void)
{
    extern _LC_collate_t collate;
    extern symtab_t cm_symtab;
    symbol_t     *sym_name;	/* symbol to be defined                 */
    item_t       *it;		/* string which is the collation symbol */
    wchar_t      pc;		/* process code for collation symbol    */
    _LC_collel_t *coll_sym;	/* collation symbol pointer             */
    char         *sym;		/* translated collation symbol          */
    int          n_syms;	/* no. of coll syms beginning with char */
    int          rc;
    int          i, j, skip;
    char	 *temp_ptr;


    sym_name = sym_pop();	/* get coll symbol name off symbol stack */
    it = sem_pop();		/* get coll symbol string off of stack */

    if (it->type != SK_STR)
	INTERNAL_ERROR;

    /* Create symbol in symbol table for coll symbol name */
    sym_name->sym_type = ST_COLL_ELL;
    sym_name->data.collel = MALLOC(coll_ell_t,1);
    add_symbol(&cm_symtab, sym_name);
    
    temp_ptr = copy(it->value.str); /* Expand without making printable */

    /* Translate collation symbol to file code */
    sym = copy_string(it->value.str);

    /* 
      Determine process code for collation symbol.  The process code for
      a collation symbol is that of the first character in the symbol.
    */
    rc = INT_METHOD(METH_OFFS(CHARMAP_MBTOWC_AT_NATIVE))
					(&charmap, &pc, temp_ptr, MB_LEN_MAX);

    if (rc < 0) {
	diag_error(ERR_ILL_CHAR, it->value.str);
	return;
    }
    skip = 0;
    for (i = 0; i < rc; i++) {
        if ((unsigned char)temp_ptr[i] < 128)
	    skip++;
	else
	    skip +=6;	/* Leave space for \\xnn\0 */
    }

    /* Now finished with the temp array, free it */
    free(temp_ptr);

    /* save process code and matching source str in symbol */
    /* do not put the first character in the src str */
    sym_name->data.collel->pc = pc;
    sym_name->data.collel->str = MALLOC(char, strlen(sym)+1);
    sym_name->data.collel->sym = sym_name->data.collel->str + skip;
    strcpy(sym_name->data.collel->str, sym);
    sym += skip;

    if (collate.co_coltbl[pc].ct_collel != NULL) {
	/* 
	  At least one collation symbol exists already --
	  Count number of collation symbols with the process code 
	*/
	for (i=0;
	     collate.co_coltbl[pc].ct_collel[i].ce_sym != NULL;
	     i++);
    
	/* 
	  Allocate memory for 
	     current number + new symbol + terminating null symbol
	*/
	coll_sym = calloc(i+2,sizeof(_LC_collel_t));
	n_syms = i;
    } else {
	/* 
	  This is the first collation symbol, allocate for 

	  new symbol + terminating null symbol
	*/
	coll_sym = calloc(2,sizeof(_LC_collel_t));
	n_syms = 0;
    }
    
    if (coll_sym == NULL)
	INTERNAL_ERROR;
    
    /* Add collation symbols to list in sorted order */
    for (i=j=0; i < n_syms; i++,j++) {
	int   c;

	c = strcmp(sym, collate.co_coltbl[pc].ct_collel[i].ce_sym);
	if (c < 0 && i == j) {
	    coll_sym[j].ce_sym = sym;
	    set_coll_wgt(&(coll_sym[j].ce_wgt), UNDEFINED, -1);
	    j++;
	} 
	coll_sym[j].ce_sym = collate.co_coltbl[pc].ct_collel[i].ce_sym;
	set_coll_wgt(&(coll_sym[j].ce_wgt), UNDEFINED, -1);
    }
    if (i==j) {
	/* 
	  either subs was empty or new substring is greater than any other
	  to date 
	*/
	coll_sym[j].ce_sym = sym;
	set_coll_wgt(&(coll_sym[j].ce_wgt), UNDEFINED, -1);
	j++;
    }
    /* Add terminating NULL symbol */
    coll_sym[j].ce_sym = NULL;

    /* free space occupied by old list */
    if (n_syms>0)
	free(collate.co_coltbl[pc].ct_collel);
    
    /* attach new list to coll table */
    collate.co_coltbl[pc].ct_collel = coll_sym;

    destroy_item(it);
}


/* 
*  FUNCTION: sem_spec_collsym
*
*  DESCRIPTION:
*  Defines a placeholder collation symbol name.  These symbols are typically
*  used to assign collation values to a set of characters.
*/
void
sem_spec_collsym(void)
{
    extern symtab_t cm_symtab;
    symbol_t *sym,*t;

    sym = sym_pop();		/* get coll symbol name off symbol stack */

    t = loc_symbol(&cm_symtab,sym->sym_id,0);
    if (t != NULL)
	diag_error(ERR_DUP_COLL_SYM,sym->sym_id);
    else {
        /* Create symbol in symbol table for coll symbol name */
        sym->sym_type = ST_COLL_SYM;
        sym->data.collsym = calloc(1, sizeof(_LC_weight_t));
        set_coll_wgt(sym->data.collsym, UNDEFINED, -1);
        add_symbol(&cm_symtab, sym);
    }
}


/*
*  FUNCTION: sem_collate
*
*  DESCRIPTION:
*  Post processing for collation table which consists of the location
*  and assignment of specific value for <HIGH> and UNDEFINED collation
*  weights.
*/
void
sem_collate(void)
{
    extern _LC_collate_t collate;
    extern wchar_t max_wchar_enc;
    extern symtab_t cm_symtab;
    _LC_weight_t *undefined;
    _LC_weight_t *high;
    _LC_collel_t *ce;
    symbol_t     *s;
    int          i, j, k;
    int          warn=FALSE;		/* Local flag to hide extra errors */
    symbol_list* p;
    void delete_symbol_list(symbol_list*);

    s = loc_symbol(&cm_symtab, "UNDEFINED", 0);
    if (s==NULL)
	INTERNAL_ERROR;
    undefined = s->data.chr->wgt;
    if (get_coll_wgt(undefined, 0)==ULONG_MAX)
	warn = TRUE;


    s = loc_symbol(&cm_symtab, "<HIGH>", 0);
    if (s==NULL)
	INTERNAL_ERROR;
    high = s->data.chr->wgt;
    
    /* assign a collation weight to <HIGH> */
    set_coll_wgt(high, nxt_coll_wgt(), -1);

    for (i=0; i<=(int)((unsigned int)collate.co_nord); i++)
	if (collate.co_nord < _COLL_WEIGHTS_INLINE) {
	    if (undefined->n[i] == ULONG_MAX)
		undefined->n[i] = high->n[i];
	} else {
	    if (undefined->p[i] == ULONG_MAX)
		undefined->p[i] = high->p[i];
	}
	    
    /* 
      Substitute symbols with UNDEFINED, and <HIGH> weights
      for the weights ultimately determined for UNDEFINED and <HIGH>.
    */
    if (collate.co_nord < _COLL_WEIGHTS_INLINE) {
	for (i=0; i<=max_wchar_enc; i++) {

	    if (wchar_defined(i)) {

	        for (j=0; j <= (int)((unsigned int)collate.co_nord); j++) {
		    if (collate.co_coltbl[i].ct_wgt.n[j] == UNDEFINED) {
		        collate.co_coltbl[i].ct_wgt.n[j] = undefined->n[j];
		        if (warn)
			    diag_error(ERR_NO_UNDEFINED);
		        warn = FALSE;
		    }
		if (collate.co_coltbl[i].ct_wgt.n[j] == (wchar_t)IGNORE)
		    collate.co_coltbl[i].ct_wgt.n[j] = 0;
	        }

	        if (collate.co_coltbl[i].ct_collel != NULL) {

		    for (j=0, ce=&(collate.co_coltbl[i].ct_collel[j]); 
		         ce->ce_sym != NULL; 
		         ce=&(collate.co_coltbl[i].ct_collel[j++])) {

		        for (k=0; k<=(int)((unsigned int)collate.co_nord); k++) {
			    if (ce->ce_wgt.n[k] == UNDEFINED)
			        ce->ce_wgt.n[k] = undefined->n[k];
			    if (ce->ce_wgt.n[k] == ULONG_MAX)
			        ce->ce_wgt.n[k] = high->n[k];
			    if (ce->ce_wgt.n[k] == (wchar_t) IGNORE)
				ce->ce_wgt.n[k] = 0;
		        }

		    }
	        }
	    }
	}

	/* We only resolve a single level forward reference.
	 * Also, we don't resolve multiple indrect forward reference.
	 * The primary weight of forward referenced collation element will be
	 * assigned to as the weight of the order of the target collation
	 * element. */
	for (p = forw_ref_head; p ; p = p->next) {
	    wchar_t		tw, sw;
	    _LC_collel_t*	ce;
	    _LC_collel_t*	ce2;
	    symbol_t*		s;
	    int			i;

	    if (p->target->sym_type == ST_COLL_SYM)
		continue;

	    switch (p->symbol->sym_type) {
	    case ST_CHR_SYM:
		sw = p->symbol->data.chr->wc_enc;

		if (p->target->sym_type == ST_CHR_SYM) {

		    tw = p->target->data.chr->wc_enc;
		    collate.co_coltbl[tw].ct_wgt.n[p->order] =
					collate.co_coltbl[sw].ct_wgt.n[0];

		} else if (p->target->sym_type == ST_COLL_ELL) {

		    tw = p->target->data.collel->pc;
		    for (i = 0, ce = &(collate.co_coltbl[tw].ct_collel[i]);
			ce->ce_sym != NULL;
		    	    ce = &(collate.co_coltbl[tw].ct_collel[++i]))
			if (!strcmp(ce->ce_sym, p->target->data.collel->sym)) {
			    ce->ce_wgt.n[p->order] =
					collate.co_coltbl[sw].ct_wgt.n[0];
			    break;
			}

		}
		break;
	    case ST_COLL_ELL:
		sw = p->symbol->data.collel->pc;
		for (i = 0, ce = &(collate.co_coltbl[sw].ct_collel[i]);
		    ce->ce_sym != NULL;
		        ce = &(collate.co_coltbl[sw].ct_collel[++i]))
		    if (! strcmp(ce->ce_sym, p->symbol->data.collel->sym))
			break;

		/* Unfortunately, the localedef file wasn't being specified
		 * the collation element in the sort order list. In this case,
		 * we cannot resolve... */
		if (ce->ce_sym == NULL)
			break;

		if (p->target->sym_type == ST_CHR_SYM) {

		    tw = p->target->data.chr->wc_enc;
		    collate.co_coltbl[tw].ct_wgt.n[p->order] = ce->ce_wgt.n[0];

		} else if (p->target->sym_type == ST_COLL_ELL) {

		    tw = p->target->data.collel->pc;
		    ce2 = ce;
		    for (i = 0, ce = &(collate.co_coltbl[tw].ct_collel[i]);
			ce->ce_sym != NULL;
		    	    ce = &(collate.co_coltbl[tw].ct_collel[++i]))
			if (!strcmp(ce->ce_sym, p->target->data.collel->sym)) {
			    ce->ce_wgt.n[p->order] = ce2->ce_wgt.n[0];
			    break;
			}

		}
		break;
	    case ST_COLL_SYM:
		s = loc_symbol(&cm_symtab, p->symbol->sym_id, 0);

		/* Unfortunately, the localedef file wasn't being specified
		 * the collation symbol in the sort order list. In this case,
		 * we cannot resolve... */
		if (s == NULL)
		    break;

		if (p->target->sym_type == ST_CHR_SYM) {

		    tw = p->target->data.chr->wc_enc;
		    collate.co_coltbl[tw].ct_wgt.n[p->order] =
						s->data.collsym->n[0];

		} else if (p->target->sym_type == ST_COLL_ELL) {

		    tw = p->target->data.collel->pc;
		    ce2 = ce;
		    for (i = 0, ce = &(collate.co_coltbl[tw].ct_collel[i]);
			ce->ce_sym != NULL;
		    	    ce = &(collate.co_coltbl[tw].ct_collel[++i]))
			if (!strcmp(ce->ce_sym, p->target->data.collel->sym)) {
			    ce->ce_wgt.n[p->order] = s->data.collsym->n[0];
			    break;
			}

		}
		break;
	    }
	}
    } else {
	for (i=0; i<=max_wchar_enc; i++) {

	    if (wchar_defined(i)){
	        for (j=0; j <= (int)((unsigned int)collate.co_nord); j++) {
		    if (collate.co_coltbl[i].ct_wgt.p[j] == UNDEFINED) {
		        collate.co_coltbl[i].ct_wgt.p[j] = undefined->p[j];
		        if (warn)
			    diag_error(ERR_NO_UNDEFINED);
		        warn = FALSE;
		    }
		if (collate.co_coltbl[i].ct_wgt.p[j] == (wchar_t) IGNORE)
		    collate.co_coltbl[i].ct_wgt.p[j] = 0;
	        }

	        if (collate.co_coltbl[i].ct_collel != NULL) {

		    for (j=0, ce=&(collate.co_coltbl[i].ct_collel[j]); 
		         ce->ce_sym != NULL; 
		         ce=&(collate.co_coltbl[i].ct_collel[j++])) {
		        for (k=0; k<=(int)((unsigned int)collate.co_nord); k++) {
			    if (ce->ce_wgt.p[k] == UNDEFINED)
			        ce->ce_wgt.p[k] = undefined->p[j];
			    if (ce->ce_wgt.p[k] == ULONG_MAX)
			        ce->ce_wgt.p[k] = high->p[j];
			    if (ce->ce_wgt.p[k] == (wchar_t) IGNORE)
				ce->ce_wgt.p[k] = 0;
		        }
		    }
	        }
	    }
        }

	/* We only resolve a single level forward reference.
	 * Also, we don't resolve multiple indrect forward reference.
	 * The primary weight of forward referenced collation element will be
	 * assigned to as the weight of the order of the target collation
	 * element. */
	for (p = forw_ref_head; p ; p = p->next) {
	    wchar_t		tw, sw;
	    _LC_collel_t*	ce;
	    _LC_collel_t*	ce2;
	    symbol_t*		s;
	    int			i;

	    if (p->target->sym_type == ST_COLL_SYM)
		continue;

	    switch (p->symbol->sym_type) {
	    case ST_CHR_SYM:
		sw = p->symbol->data.chr->wc_enc;

		if (p->target->sym_type == ST_CHR_SYM) {

		    tw = p->target->data.chr->wc_enc;
		    collate.co_coltbl[tw].ct_wgt.p[p->order] =
					collate.co_coltbl[sw].ct_wgt.p[0];

		} else if (p->target->sym_type == ST_COLL_ELL) {

		    tw = p->target->data.collel->pc;
		    for (i = 0, ce = &(collate.co_coltbl[tw].ct_collel[i]);
			ce->ce_sym != NULL;
		    	    ce = &(collate.co_coltbl[tw].ct_collel[++i]))
			if (!strcmp(ce->ce_sym, p->target->data.collel->sym)) {
			    ce->ce_wgt.p[p->order] =
					collate.co_coltbl[sw].ct_wgt.p[0];
			    break;
			}

		}
		break;
	    case ST_COLL_ELL:
		sw = p->symbol->data.collel->pc;
		for (i = 0, ce = &(collate.co_coltbl[sw].ct_collel[i]);
		    ce->ce_sym != NULL;
		        ce = &(collate.co_coltbl[sw].ct_collel[++i]))
		    if (! strcmp(ce->ce_sym, p->symbol->data.collel->sym))
			break;

		/* Unfortunately, the localedef file wasn't being specified
		 * the collation element in the sort order list. In this case,
		 * we cannot resolve... */
		if (ce->ce_sym == NULL)
			break;

		if (p->target->sym_type == ST_CHR_SYM) {

		    tw = p->target->data.chr->wc_enc;
		    collate.co_coltbl[tw].ct_wgt.p[p->order] = ce->ce_wgt.p[0];

		} else if (p->target->sym_type == ST_COLL_ELL) {

		    tw = p->target->data.collel->pc;
		    ce2 = ce;
		    for (i = 0, ce = &(collate.co_coltbl[tw].ct_collel[i]);
			ce->ce_sym != NULL;
		    	    ce = &(collate.co_coltbl[tw].ct_collel[++i]))
			if (!strcmp(ce->ce_sym, p->target->data.collel->sym)) {
			    ce->ce_wgt.p[p->order] = ce2->ce_wgt.p[0];
			    break;
			}

		}
		break;
	    case ST_COLL_SYM:
		s = loc_symbol(&cm_symtab, p->symbol->sym_id, 0);

		/* Unfortunately, the localedef file wasn't being specified
		 * the collation symbol in the sort order list. In this case,
		 * we cannot resolve... */
		if (s == NULL)
		    break;

		if (p->target->sym_type == ST_CHR_SYM) {

		    tw = p->target->data.chr->wc_enc;
		    collate.co_coltbl[tw].ct_wgt.p[p->order] =
							s->data.collsym->p[0];

		} else if (p->target->sym_type == ST_COLL_ELL) {

		    tw = p->target->data.collel->pc;
		    ce2 = ce;
		    for (i = 0, ce = &(collate.co_coltbl[tw].ct_collel[i]);
			ce->ce_sym != NULL;
		    	    ce = &(collate.co_coltbl[tw].ct_collel[++i]))
			if (!strcmp(ce->ce_sym, p->target->data.collel->sym)) {
			    ce->ce_wgt.p[p->order] = s->data.collsym->p[0];
			    break;
			}

		}
		break;
	    }
	}
    }

    delete_symbol_list(forw_ref_head);
}

/*
*  FUNCTION: setup_substr
*
*  DESCRIPTION:
*  Set-up the collation weights for the substitute strings defined in
*  the collation section using the keyword "substitute". This is executed
*  after the order keyword (at which time we now know how many orders there
*  are and if any have subs turned off).
*
*/
void
setup_substr(void)
{

	extern _LC_collate_t collate;
	int n, i;
	int flag_subs;
	int flag_nosubs;

	flag_nosubs = 0;
	flag_subs = 1;

	for (n=0; n < (int)((unsigned int)collate.co_nsubs); n++) {

	    if (collate.co_nord >= _COLL_WEIGHTS_INLINE) {
   	        collate.co_subs[n].ss_act.p = MALLOC(wchar_t,collate.co_nord+1);

		for (i = 0; i <= (int)((unsigned int)collate.co_nord); i++) {

		    if (collate.co_sort.p[i] & _COLL_NOSUBS_MASK)
			collate.co_subs[n].ss_act.p[i] = flag_nosubs;
		    else
			collate.co_subs[n].ss_act.p[i] = flag_subs;
		}
	    }
	    else {

		for (i = 0; i <= (int)((unsigned int)collate.co_nord); i++) {

		    if (collate.co_sort.n[i] & _COLL_NOSUBS_MASK)
			collate.co_subs[n].ss_act.n[i] = flag_nosubs;
		    else
			collate.co_subs[n].ss_act.n[i] = flag_subs;
		}
	    }
	}
}


void
add_forw_reference(symbol_t* tgt_sym, symbol_t* wgt_sym, int order)
{
	static symbol_list* p = (symbol_list*)NULL;

	if (forw_ref_head == (symbol_list*)NULL)
		forw_ref_head = p = (symbol_list*)MALLOC(symbol_list, 1);
	else {
		p->next = (symbol_list*)MALLOC(symbol_list, 1);
		p = p->next;
	}

	memset((void*)p, 0, sizeof(symbol_list));

	p->target = tgt_sym;
	p->symbol = wgt_sym;
	p->order = order;
}


void
delete_symbol_list(symbol_list* p)
{
	if (p == (symbol_list*)NULL)
		return;

	delete_symbol_list(p->next);

	free((void*)p);
}
