/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)gen.c 1.33	96/08/23  SMI"

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
static char rcsid[] = "@(#)$RCSfile: gen.c,v $ $Revision: 1.5.5.6 $ (OSF) $Date: 1992/10/27 01:54:21 $";
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
 * 1.11  com/cmd/nls/gen.c, cmdnls, bos320, 9137320a 9/4/91 13:44:10
 */

#include <stdio.h>
#include <sys/localedef.h>
#include "method.h"
#include "localedef_msg.h"
#include "err.h"
#include "locdef.h"
#include <string.h>
#include "semstack.h"

static void gen_extern(FILE *);
static void gen_ctype(FILE *, _LC_ctype_t *);
static void gen_charmap(FILE *, _LC_charmap_t *);
static void gen_collate(FILE *, _LC_collate_t *);
static void gen_monetary(FILE *, _LC_monetary_t *);
static void gen_time(FILE *, _LC_time_t *);
static void gen_numeric(FILE *, _LC_numeric_t *);
static void gen_msg(FILE *, _LC_messages_t *);
static void gen_locale(FILE *, _LC_charmap_t *, _LC_collate_t *,  
	   _LC_ctype_t *, _LC_monetary_t *, _LC_numeric_t *, _LC_messages_t *, 
	   _LC_time_t *);

wchar_t		max_wchar_enc;
extern int	max_disp_width;
extern _LC_euc_info_t euc_info;
extern int	Native;			/* running in dense mode? */
extern int	Euc_filecode;

extern int lc_collate_flag;	/* from gram.y */
extern int lc_ctype_flag;	/* from gram.y */
extern int lc_message_flag;	/* from gram.y */
extern int lc_monetary_flag;	/* from gram.y */
extern int lc_numeric_flag;	/* from gram.y */
extern int lc_time_flag;	/* from gram.y */

static void gen_instantiate(FILE *);


/*
*  FUNCTION: fp_putstr
*
*  DESCRIPTION:
*  Standard print out routine for character strings in structure 
*  initialization data.
*/
static void
fp_putstr(FILE *fp, char *s)
{
    extern int	copying;	/* might be using internal, non-escaped, data */
    extern char yytext[];	/* use this cause its big */

    if (s == NULL)
	fprintf(fp, "\t\"\",\n");
    else {
	if (copying) {
	    char *bptr = yytext;
	    unsigned char c;

	    while ((c=*s++)) {
		if (c != '\\' && c != '"' && isascii(c) && isprint(c))
		    *bptr++ = c;
		else
		    bptr += sprintf(bptr, "\\x%02x", c);
	    }
	    *bptr = '\0';
	    fprintf(fp, "\t\"%s\",\n", yytext);
	} else
	    fprintf(fp, "\t\"%s\",\n", s);
    }
}

/*
*  FUNCTION: fp_putsym
*
*  DESCRIPTION:
*  Standard print out routine for symbols in structure initialization
*  data.
*/
static void 
fp_putsym(FILE *fp, char *s)
{
    if (s != NULL)
	fprintf(fp, "\t%s,\n", s);
    else
	fprintf(fp, "\t(void *)0,\n");
}


/*
*  FUNCTION: fp_putdig
*
*  DESCRIPTION:
*  Standard print out routine for integer valued structure initialization
*  data.
*/
static void fp_putdig(FILE *fp, int i)
{
    fprintf(fp, "\t%d,\n", i);
}

/*
*  FUNCTION: fp_puthdr
*
*  DESCRIPTION:
*  Standard print out routine for method headers.
*/
static void fp_puthdr(FILE *fp, int kind, size_t size)
{
     fprintf(fp, "\t{%d, _LC_MAGIC, _LC_VERSION_MAJOR, _LC_VERSION_MINOR, %d},\n",
		kind, size);
}


/*
*  FUNCTION: fp_putmeth
*
*  DESCRIPTION:
*  Standard print out routine for method references.
*/
static void fp_putmeth(FILE *fp, int i)
{
    fp_putsym(fp, METH_NAME(i));
}

/*
*  FUNCTION: gen_hdr
*
*  DESCRIPTION:
*  Generate the header file includes necessary to compile the generated
*  C code.
*/
static void gen_hdr(FILE *fp)
{
    fprintf(fp, "#include <locale.h>\n");
    fprintf(fp, "#include <sys/localedef.h>\n");
    fprintf(fp, "\n");
}


/* 
*  FUNCTION: gen
*
*  DESCRIPTION:
*  Common entry point to code generation.  This function calls each of the
*  functions in turn which generate the locale C code from the in-memory
*  tables built parsing the input files.
*/
void gen(FILE *fp)
{
    extern _LC_charmap_t  charmap;
    extern _LC_collate_t  *collate_ptr;
    extern _LC_ctype_t    *ctype_ptr;
    extern _LC_monetary_t *monetary_ptr;
    extern _LC_numeric_t  *numeric_ptr;
    extern _LC_time_t     *lc_time_ptr;
    extern _LC_messages_t     *messages_ptr;

    gen_hdr(fp);
    gen_extern(fp);
    gen_charmap(fp, &charmap);
    if (lc_ctype_flag != 0)
    	gen_ctype(fp, ctype_ptr);
    if (lc_collate_flag != 0)
	gen_collate(fp, collate_ptr);
    if (lc_monetary_flag != 0)
	gen_monetary(fp, monetary_ptr);
    if (lc_numeric_flag != 0)
	gen_numeric(fp, numeric_ptr);
    if (lc_time_flag != 0)
	gen_time(fp, lc_time_ptr);
    if (lc_message_flag != 0)
	gen_msg(fp, messages_ptr);
    gen_locale(fp, &charmap, collate_ptr, ctype_ptr, 
	       monetary_ptr, numeric_ptr, messages_ptr, lc_time_ptr );
    gen_instantiate(fp);
    
}


/* 
*  FUNCTION: gen_extern
*
*  DESCRIPTION:
*  This function generates the externs which are necessary to reference the
*  function names inside the locale objects.  
*/
static void gen_extern(FILE *fp)
{
  int i;
  char *s;
  char *p;		/* Prototype format string */

  for (i=0; i<=LAST_METHOD; i++) {
      s = METH_NAME(i);
      if (s != NULL) {
	  p = METH_PROTO(i);

	  if(p && *p) {			/* There is a prototype string */
	      fprintf(fp, "extern ");
	      fprintf(fp, p, s);
	      fputs(";\n",fp);
	  } else
	    fprintf(fp, "extern %s();\n", s);
      }
  }
}


/* 
*  FUNCTION: gen_charmap
*
*  DESCRIPTION:
*  This function generates the C code which implements a _LC_charmap_t
*  data structure corresponding to the in memory charmap build parsing the
*  charmap sourcefile.
*/
#define PLACEHOLDER_STRING "(_LC_methods_func_t)0"
static void gen_charmap(FILE *fp, _LC_charmap_t *lc_cmap)
{
  if (Euc_filecode == TRUE) {
	fprintf(fp, "/*------------------------------"
		    "EUC INFO-----------------------------*/\n");
	fprintf(fp, "static _LC_euc_info_t cm_eucinfo={\n");
	fprintf(fp, "\t(char) 1,\n");
	fprintf(fp, "\t(char) %d,\n", euc_info.euc_bytelen1);
	fprintf(fp, "\t(char) %d,\n", euc_info.euc_bytelen2);
	fprintf(fp, "\t(char) %d,\n", euc_info.euc_bytelen3);
	fprintf(fp, "\t(char) 1,\n");
	fprintf(fp, "\t(char) %d,\n", euc_info.euc_scrlen1);
	fprintf(fp, "\t(char) %d,\n", euc_info.euc_scrlen2);
	fprintf(fp, "\t(char) %d,\n", euc_info.euc_scrlen3);
	fp_putdig(fp, euc_info.cs1_base);	/* CS1 dense code base value */
	fp_putdig(fp, euc_info.cs2_base);
	fp_putdig(fp, euc_info.cs3_base);
	fp_putdig(fp, max_wchar_enc);		/* dense code last value */
	fp_putdig(fp, euc_info.cs1_adjustment);	/* CS1 adjustment value */
	fp_putdig(fp, euc_info.cs2_adjustment);
	fp_putdig(fp, euc_info.cs3_adjustment);
	fprintf(fp, "};\n\n");
  }

  fprintf(fp, 
"/*------------------------- CHARMAP OBJECT  -------------------------*/\n");
  fprintf(fp, "static _LC_methods_charmap_t native_methods_charmap={\n");

  /*
  fprintf(fp, "\t(short) %d,\n", lc_cmap->core.native_api->nmethods);
  fprintf(fp, "\t(short) %d,\n", lc_cmap->core.native_api->ndefined);
  */
  fprintf(fp, "\t(short) 0,\n");
  fprintf(fp, "\t(short) 0,\n");
  /* class methods */
  fprintf(fp, "\t(char *(*) ()) 0,\n");	  /* char *(*nl_langinfo)(); */
  fp_putmeth(fp, CHARMAP_MBTOWC_AT_NATIVE);
  fp_putmeth(fp, CHARMAP_MBSTOWCS_AT_NATIVE);
  fp_putmeth(fp, CHARMAP_WCTOMB_AT_NATIVE);
  fp_putmeth(fp, CHARMAP_WCSTOMBS_AT_NATIVE);
  fp_putmeth(fp, CHARMAP_MBLEN);
  fp_putmeth(fp, CHARMAP_WCSWIDTH_AT_NATIVE);
  fp_putmeth(fp, CHARMAP_WCWIDTH_AT_NATIVE);
  fp_putmeth(fp, CHARMAP_MBFTOWC_AT_NATIVE);
  fp_putmeth(fp, CHARMAP_FGETWC_AT_NATIVE);
  /* for future use */
  fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
  fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
  fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
  fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
  fprintf(fp, "\t%s\n",  PLACEHOLDER_STRING);
  fprintf(fp, "};\n\n");

  if (Native == FALSE) {		/* producing a non-native locale */
  	fprintf(fp, "static _LC_methods_charmap_t user_methods_charmap={\n");

	/*
	fprintf(fp, "\t(short) %d,\n", lc_cmap->core.native_api->nmethods);
	fprintf(fp, "\t(short) %d,\n", lc_cmap->core.native_api->ndefined);
	*/
	fprintf(fp, "\t(short) 0,\n");
	fprintf(fp, "\t(short) 0,\n");
	/* class methods */
	fprintf(fp, "\t(char *(*) ()) 0,\n");	  /* char *(*nl_langinfo)(); */
	fp_putmeth(fp, CHARMAP_MBTOWC);
	fp_putmeth(fp, CHARMAP_MBSTOWCS);
	fp_putmeth(fp, CHARMAP_WCTOMB);
	fp_putmeth(fp, CHARMAP_WCSTOMBS);
	fp_putmeth(fp, CHARMAP_MBLEN);
	fp_putmeth(fp, CHARMAP_WCSWIDTH);
	fp_putmeth(fp, CHARMAP_WCWIDTH);
	fp_putmeth(fp, CHARMAP_MBFTOWC);
	fp_putmeth(fp, CHARMAP_FGETWC);
	/* for future use */
	fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
	fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
	fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
	fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
	fprintf(fp, "\t%s\n",  PLACEHOLDER_STRING);
	fprintf(fp, "};\n\n");
  }

  fprintf(fp, "static _LC_charmap_t lc_cmap = {{\n");

  /* class core */
  fp_puthdr(fp, _LC_CHARMAP, sizeof(_LC_charmap_t) );
  fp_putmeth(fp, CHARMAP_CHARMAP_INIT);
  fprintf(fp, "\t0,\t\t\t\t/* charmap_destructor() */\n");
  /* fp_putmeth(fp, CHARMAP_CHARMAP_DESTRUCTOR); */
  if (Native == FALSE)
	fp_putsym(fp, "&user_methods_charmap");
  else
	fp_putsym(fp, "&native_methods_charmap");
  fp_putsym(fp, "&native_methods_charmap");
  fp_putmeth(fp, CHARMAP_EUCPCTOWC);
  fp_putmeth(fp, CHARMAP_WCTOEUCPC);
  fp_putsym(fp, 0);
  fprintf(fp,"\t},\t/* End core */\n");

  /* class extension */
  fp_putstr(fp, lc_cmap->cm_csname);
  switch (lc_cmap->cm_fc_type) {
  case _FC_EUC: fp_putsym(fp, "_FC_EUC");
		break;
  case _FC_UTF8: fp_putsym(fp, "_FC_UTF8");
		 break;
  case _FC_OTHER: fp_putsym(fp, "_FC_OTHER");
		  break;
  default:	fp_putsym(fp, "_FC_OTHER");
		break;
  }
  switch (lc_cmap->cm_pc_type) {
  case _PC_EUC: fp_putsym(fp, "_PC_EUC");
		break;
  case _PC_DENSE: fp_putsym(fp, "_PC_DENSE");
		  break;
  case _PC_UCS4: fp_putsym(fp, "_PC_UCS4");
		 break;
  default:	fp_putsym(fp, "_PC_DENSE");
		break;
  }
  fp_putdig(fp, mb_cur_max);
  fp_putdig(fp, 1);
  fp_putdig(fp, max_disp_width);
  if (Euc_filecode == TRUE)
	fp_putsym(fp, "&cm_eucinfo");
  else
	fprintf(fp, "\t(_LC_euc_info_t *) NULL\n");
  fprintf(fp, "};\n\n");
}


/* 
*  FUNCTION: compress_mask
*
*  DESCRIPTION:
*  Take all masks for codepoints above 255 and assign each unique mask
*  into a secondary array.  Create an unsigned byte array of indices into
*  the mask array for each of the codepoints above 255.
*/
static int compress_masks(_LC_ctype_t *ctype)
{
  static int nxt_idx = 1;
  int    umasks;
  unsigned char *qidx;
  unsigned int	*qmask;
  int i, j;

  if (ctype->mask == NULL)
      return 0;

  /* allocate memory for masks and indices */
  ctype->qidx = qidx = (unsigned char *) calloc(max_wchar_enc - 256 +1,
						sizeof(unsigned char));
  ctype->qmask = qmask = MALLOC(unsigned int, 256);
  
  umasks = 1;
  for (i=256; i<= max_wchar_enc; i++) {      /* for each codepoint > 255 */
    /* 
      Search for a mask in the 'qmask' array which matches the mask for
      the character corresponding to 'i'.
    */
    for (j=0; j < umasks; j++) 
      if (qmask[j] == ctype->mask[i]) {
	/* mask already exists, place index in qidx for character */
	qidx[i-256] = j;	
	break;
      }

    if (j==umasks) {

      /* couldn't find mask which would work, so add new mask to 'qmask' */

      qidx[i-256] = nxt_idx;
      qmask[nxt_idx] = ctype->mask[i];

      nxt_idx++;
      umasks++;
    }

    /* only support 256 unique masks for multi-byte characters */
    if (nxt_idx >= 256) INTERNAL_ERROR;
  }

  return nxt_idx;
}


/* 
*  FUNCTION: gen_ctype
*
*  DESCRIPTION:
*  Generate C code which implements the _LC_ctype_t locale
*  data structures.  These data structures include _LC_classnms_t,
*  an array of wchars for transformations,
*  and the container class _LC_ctype_t itself.
*/
#define N_PER_LINE 4
static void gen_ctype(FILE *fp, _LC_ctype_t *lc_ctype)
{
  int i, j;
  int k;
  int n_idx;
  int bind_index;
  int sizeof_compat_is_table;
  int sizeof_compat_upper_table;
  int sizeof_compat_lower_table;
  char *lc_bind_tag_names[]={
	"_LC_TAG_UNDEF",
	"_LC_TAG_TRANS",
	"_LC_TAG_CCLASS"
  };
  char *supper = "A";
  char *slower = "a";
  extern struct lcbind_symbol_table lcbind_symbol_table[];
  extern int length_lcbind_symbol_table;
  extern struct lcbind_table *Lcbind_Table;
  int line_count;

  fprintf(fp, 
"/*------------------------- CHARACTER CLASSES -------------------------*/\n");

#ifdef undef
  /*
   * can't do this because it could disburb ctype_compat_table[]
   */

  for (i=0; i < length_lcbind_symbol_table; i++)
	fprintf(fp, "#define %s\t0x%08x\n", lcbind_symbol_table[i].symbol_name,
					  lcbind_symbol_table[i].value);
  fprintf(fp, "\n");
#endif

  fprintf(fp, "static const _LC_bind_table_t bindtab[] ={\n");
  for (i=0, bind_index=0; i < lc_ctype->nbinds; i++) {
	if (Lcbind_Table[i].defined == 1) {
	    fprintf(fp, "\t{ \"%s\",\t%s,\t"
		    "(_LC_bind_value_t) 0x%08x },\n",
		    Lcbind_Table[i].lcbind.bindname,
		    lc_bind_tag_names[Lcbind_Table[i].lcbind.bindtag],
		    Lcbind_Table[i].nvalue);
	    bind_index++;
	}
  }
  fprintf(fp, "};\n\n");

  /*
   * if upper or lower not specified then fill in defaults.
   */

  if (lc_ctype->upper == NULL) {
	for (*supper = 'A', *slower = 'a'; *supper <= 'Z';
						(*supper)++, (*slower)++) {
	    item_t *iptr;

	    sem_symbol(slower);
	    iptr = create_item(SK_INT, ((int)(*slower)));
	    sem_push(iptr);
	    sem_symbol_def();
	    sem_existing_symbol(slower);
	    sem_char_ref();

	    sem_symbol(supper);
	    iptr = create_item(SK_INT, ((int)(*supper)));
	    sem_push(iptr);
	    sem_symbol_def();
	    sem_existing_symbol(supper);
	    sem_char_ref();

	    sem_push_xlat();
	}
	add_transformation(lc_ctype, Lcbind_Table, "toupper");
  }

  if (lc_ctype->lower == NULL) {
	for (*supper = 'A', *slower = 'a'; *supper <= 'Z';
						(*supper)++, (*slower)++) {
	    item_t *iptr;

	    sem_symbol(supper);
	    iptr = create_item(SK_INT, ((int)(*supper)));
	    sem_push(iptr);
	    sem_symbol_def();
	    sem_existing_symbol(supper);
	    sem_char_ref();

	    sem_symbol(slower);
	    iptr = create_item(SK_INT, ((int)(*slower)));
	    sem_push(iptr);
	    sem_symbol_def();
	    sem_existing_symbol(slower);
	    sem_char_ref();

	    sem_push_xlat();
	}
	add_transformation(lc_ctype, Lcbind_Table, "tolower");
  }

  /*
   * Generate transformation tables
   */
  
  for (k = 0; k < lc_ctype->ntrans; k++) {

      fprintf(fp, "/*-------------------- %s --------------------*/\n",
	      lc_ctype->transname[k].name);
      fprintf(fp, "static const wchar_t transformation_%s[] = {\n",
	      lc_ctype->transname[k].name);

/* craigm - repair this */
#ifdef craigm
  /*
   * if upper table not set, set default upper values from 
   * 0 to max_wchar_enc, encoding = value, except for a-z = A-Z
   */
  if (!copying_ctype)
      lc_ctype->max_upper = max_wchar_enc;

  if (lc_ctype->upper == NULL) {
      lc_ctype->upper = MALLOC(wchar_t, max_wchar_enc+1);
      for (i=0; i <= max_wchar_enc; i++)
	if ((i >= 'a') && (i <= 'z'))
	  lc_ctype->upper[i] = i + ('A' - 'a');
	else
	  lc_ctype->upper[i] = i;
  }
#endif
/* craigm - repair this */
#ifdef craigm
  if (lc_ctype->lower == NULL) {
      wchar_t *lp = (wchar_t *) calloc(sizeof(wchar_t), lc_ctype->max_upper+1);

      /* tolower unspecified, make it the inverse of toupper */
      lc_ctype->lower = lp;

      /* assign identity */
      for (i=0; i<=lc_ctype->max_upper; i++)
	  lp[i] = i;
      
      /* assign inverse */
      for (i=0; i<=lc_ctype->max_upper; i++)
	  lp[lc_ctype->upper[i]] = i;
      lc_ctype->max_lower = lc_ctype->max_upper;
  } else {
    if (!copying_ctype)
      lc_ctype->max_lower = max_wchar_enc;
  }
#endif

	line_count = 0;
#define N_PER_LINE_TRANSFORMATIONS 8
	if ((strcmp("toupper", lc_ctype->transname[k].name) == 0) ||
	    (strcmp("tolower", lc_ctype->transname[k].name) == 0)) {
		i = 0;
		fprintf(fp, "-1,\t/* %s[EOF] entry */\n",
			lc_ctype->transname[k].name);
	}
	else
		i=lc_ctype->transname[k].tmin;

	for (i; i<=lc_ctype->transname[k].tmax; i+=N_PER_LINE_TRANSFORMATIONS) {

	    for (j=0; (j < N_PER_LINE_TRANSFORMATIONS) &&
		      ((i+j) <= lc_ctype->transname[k].tmax); j++)
		fprintf(fp, "0x%04x, ", lc_ctype->transtabs[k][i+j]);

	    line_count += N_PER_LINE_TRANSFORMATIONS;
	    if ((i == 0) || (i == lc_ctype->transname[k].tmin) ||
			    (line_count % (N_PER_LINE_TRANSFORMATIONS * 2)))
		fprintf(fp, "  /* 0x%04x */", i);
	    fprintf(fp, "\n");

/* craigm - fix this */
#ifdef craigm
#ifndef _NO_GCC_HACK
    line_count += N_PER_LINE;
    if ((line_count % 512) == 0) {
	fprintf(fp,"};\n");
	fprintf(fp,"static const wchar_t upper_%d[] = {\n", line_count);
    }
#endif
#endif
	}
	fprintf(fp, "};\n\n");
  }
  

  fprintf(fp, 
"/*------------------------- CHAR CLASS MASKS -------------------------*/\n");

  /* 
    print the data for the standard linear array of class masks.
  */
  fprintf(fp,"static const unsigned int masks[] = {\n");
  fprintf(fp,"0,\t/* masks[EOF] entry */\n");
  for (i=0; i < 256; i+=N_PER_LINE) {

    for (j=0; j < N_PER_LINE && i+j < 256; j++)
      fprintf(fp, "0x%08x, ", 
	      ((lc_ctype->mask!=NULL)?(lc_ctype->mask[i+j]):0));

    fprintf(fp, "\n");
  }
  fprintf(fp, "};\n\n");

  /* 
    If there are more than 256 codepoints in the codeset, the
    implementation attempts to compress the masks into a two level
    array of indices into masks.
  */
  if ((max_wchar_enc > 255) && !copying_ctype) {
    n_idx = compress_masks(lc_ctype);

    lc_ctype->qidx_hbound = max_wchar_enc;

    /* Print the index array 'qidx' */
    fprintf(fp, "static const unsigned char qidx[] = {\n");
#ifndef _NO_GCC_HACK
    line_count = 0;
#endif
    for (i=256; i <= lc_ctype->qidx_hbound; i+=N_PER_LINE) {
    
      for (j=0; j<N_PER_LINE && i+j <= lc_ctype->qidx_hbound; j++)
	fprintf(fp, "0x%02x, ", lc_ctype->qidx[i+j-256]);
    
      fprintf(fp, "\n");

#ifndef _NO_GCC_HACK
      line_count += N_PER_LINE;

      if ((line_count % 512) == 0) {
	  fprintf(fp,"};\n");
	  fprintf(fp,"static const unsigned char qidx_%d[] = {\n", line_count);
      }
#endif

    }
    fprintf(fp, "};\n\n");
  
    /* Print the mask array 'qmask' */
    fprintf(fp, "static const unsigned int qmask[] = {\n");
    for (i=0; i<n_idx; i+= N_PER_LINE) {
 
      for (j=0; j < N_PER_LINE && i+j < n_idx; j++)
	fprintf(fp, "0x%04x, ", lc_ctype->qmask[i+j]);
      
    }
    fprintf(fp, "};\n\n");
  } else
    n_idx =0;

  /*
   * Write out Solaris _ctype[] table for backwards compatibility.
   */

  fprintf(fp,
"/*-------------------- CTYPE COMPATIBILITY TABLE  --------------------*/\n\n");

  fprintf(fp,"#define SZ_CTYPE	(257 + 257)\n"); /* is* and to{upp,low}er */
  fprintf(fp,"#define SZ_CODESET	7\n");   /* codeset size information */
  fprintf(fp,"#define SZ_TOTAL	(SZ_CTYPE + SZ_CODESET)\n");

  sizeof_compat_is_table = 255;
  sizeof_compat_upper_table = 255;
  sizeof_compat_lower_table = 255;
/* craigm */
fprintf(fp, "/*\n");
fprintf(fp, "sizeof_compat_is_table = %d\n", sizeof_compat_is_table);
fprintf(fp, "sizeof_compat_upper_table = %d\n", sizeof_compat_upper_table);
fprintf(fp, "sizeof_compat_lower_table = %d\n", sizeof_compat_lower_table);
fprintf(fp, "*/\n");

  fprintf(fp,"static unsigned char ctype_compat_table[SZ_TOTAL] = { 0,\n");

  for (i = 0; i <= sizeof_compat_is_table; i++) {
	char ctype_mask[STRING_MAX] = "";

	if (lc_ctype->mask[i] & _ISUPPER)
		strcat(ctype_mask, "_U|");
	if (lc_ctype->mask[i] & _ISLOWER)
		strcat(ctype_mask, "_L|");
	if (lc_ctype->mask[i] & _ISDIGIT)
		strcat(ctype_mask, "_N|");
	if (lc_ctype->mask[i] & _ISSPACE)
		strcat(ctype_mask, "_S|");
	if (lc_ctype->mask[i] & _ISPUNCT)
		strcat(ctype_mask, "_P|");
	if (lc_ctype->mask[i] & _ISCNTRL)
		strcat(ctype_mask, "_C|");
	if (lc_ctype->mask[i] & _ISBLANK)
		if (i != 0x09)		/* if tab then don't mark blank */
			strcat(ctype_mask, "_B|");
	if (lc_ctype->mask[i] & _ISXDIGIT)
		strcat(ctype_mask, "_X|");

	if (strlen(ctype_mask) == 0)
		strcat(ctype_mask, "0|");
	ctype_mask[strlen(ctype_mask) - 1] = '\0';
	fprintf(fp, "\t%s,", ctype_mask);
	if ((i + 1) % 8 == 0)
		fprintf(fp, "\n");
  }
  for (i; i <= sizeof_compat_is_table; i++) {
	fprintf(fp, "\t0,");
	if ((i + 1) % 8 == 0)
		fprintf(fp, "\n");
  }

  /*
   * Now generate the toupper/tolower table.
   */
  fprintf(fp, "\n\t0,\n");
  for (i = 0; i <= sizeof_compat_is_table; i++) {
	if (i <= sizeof_compat_upper_table && lc_ctype->upper[i] != i)
		fprintf(fp, "\t0x%02x,", lc_ctype->upper[i]);
	else if (i <= sizeof_compat_lower_table && lc_ctype->lower[i] != i)
		fprintf(fp, "\t0x%02x,", lc_ctype->lower[i]);
	else
		fprintf(fp, "\t0x%02x,", i);
	if ((i + 1) % 8 == 0)
		fprintf(fp, "\n");
  }
  for (i; i <= sizeof_compat_is_table; i++) {
	fprintf(fp, "\t0,");
	if ((i + 1) % 8 == 0)
		fprintf(fp, "\n");
  }
  fprintf(fp, "\n");

  /*
   * Generate the width information.
   */
  fprintf(fp, "\t/* multiple byte character width information */\n\n");
  fprintf(fp, "\t%d,\t%d,\t%d,\t%d,\t%d,\t%d,\t%d\n",
	euc_info.euc_bytelen1,
	euc_info.euc_bytelen2,
	euc_info.euc_bytelen3,
	euc_info.euc_scrlen1,
	euc_info.euc_scrlen2,
	euc_info.euc_scrlen3,
	mb_cur_max);

  fprintf(fp, "};\n\n");

  fprintf(fp,
"/*---------------------   TRANSFORMATION TABLES   --------------------*/\n");
  fprintf(fp, "static _LC_transnm_t transname[]={\n");
  fprintf(fp, "\t{ NULL,\t\t0,\t0,\t\t0 },\n");
  for (i = 0; i < lc_ctype->ntrans; i++) {
	fprintf(fp, "\t{ \"%s\",\t%d,\t0x%08x,\t0x%08x },\n",
			lc_ctype->transname[i].name,
			i+1,
			lc_ctype->transname[i].tmin,
			lc_ctype->transname[i].tmax);
  }
  fprintf(fp, "};\n\n");

  fprintf(fp, "static const wchar_t *transtabs[]={\n");
  fprintf(fp, "\tNULL,\n");
  for (i = 0; i < lc_ctype->ntrans; i++)
	if ((strcmp("toupper", lc_ctype->transname[i].name) == 0) ||
	    (strcmp("tolower", lc_ctype->transname[i].name) == 0))
	    fprintf(fp, "\t&transformation_%s[1],\n",
						lc_ctype->transname[i].name);
	else
	    fprintf(fp, "\ttransformation_%s,\n", lc_ctype->transname[i].name);
  fprintf(fp, "};\n\n");

  fprintf(fp, 
"/*-------------------------   CTYPE OBJECT   -------------------------*/\n");
  fprintf(fp, "static _LC_methods_ctype_t native_methods_ctype={\n");

  /*
  fprintf(fp, "\t(short) %d,\n", lc_ctype->core.native_api->nmethods);
  fprintf(fp, "\t(short) %d,\n", lc_ctype->core.native_api->ndefined);
  */
  fprintf(fp, "\t(short) 0,\n");
  fprintf(fp, "\t(short) 0,\n");
  /* class methods */
  fp_putmeth(fp, CTYPE_WCTYPE);
  fp_putmeth(fp, CTYPE_ISWCTYPE_AT_NATIVE);
  fp_putmeth(fp, CTYPE_TOWUPPER_AT_NATIVE);
  fp_putmeth(fp, CTYPE_TOWLOWER_AT_NATIVE);
  fp_putmeth(fp, CTYPE_TRWCTYPE);
  fp_putmeth(fp, CTYPE_WCTRANS);
  fp_putmeth(fp, CTYPE_TOWCTRANS_AT_NATIVE);
  /* for future use */
  fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
  fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
  fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
  fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
  fprintf(fp, "\t%s\n",  PLACEHOLDER_STRING);
  fprintf(fp, "};\n\n");

  if (Native == FALSE) {
	fprintf(fp, "static _LC_methods_ctype_t user_methods_ctype={\n");

	/*
	fprintf(fp, "\t(short) %d,\n", lc_ctype->core.native_api->nmethods);
	fprintf(fp, "\t(short) %d,\n", lc_ctype->core.native_api->ndefined);
	*/
	fprintf(fp, "\t(short) 0,\n");
	fprintf(fp, "\t(short) 0,\n");
	/* class methods */
	fp_putmeth(fp, CTYPE_WCTYPE);
	fp_putmeth(fp, CTYPE_ISWCTYPE);
	fp_putmeth(fp, CTYPE_TOWUPPER);
	fp_putmeth(fp, CTYPE_TOWLOWER);
	fp_putmeth(fp, CTYPE_TRWCTYPE);
	fp_putmeth(fp, CTYPE_WCTRANS);
	fp_putmeth(fp, CTYPE_TOWCTRANS);
	/* for future use */
	fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
	fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
	fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
	fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
	fprintf(fp, "\t%s\n",  PLACEHOLDER_STRING);
	fprintf(fp, "};\n\n");
  }

  fprintf(fp,"static _LC_ctype_t lc_ctype = {{\n");

  /* class core */
  fp_puthdr(fp, _LC_CTYPE, sizeof(_LC_ctype_t));
  fp_putmeth(fp, CTYPE_CTYPE_INIT);
  fprintf(fp, "\t0,\t\t\t\t/* ctype_destructor() */\n");
  /* fp_putmeth(fp, CTYPE_CTYPE_DESTRUCTOR); */
  if (Native == FALSE)
	fp_putsym(fp, "&user_methods_ctype");
  else
	fp_putsym(fp, "&native_methods_ctype");
  fp_putsym(fp, "&native_methods_ctype");
  fp_putsym(fp, 0);

  fprintf(fp,"\t},\t/* End core */\n");

  /* class extension */
  fp_putsym(fp, "&lc_cmap");		/* _LC_charmap_t *charmap; */
  /* max and min process code (required by toupper, et al) */
  fp_putdig(fp, 0);
  fp_putdig(fp, max_wchar_enc);
  fp_putdig(fp, lc_ctype->max_upper);
  fp_putdig(fp, lc_ctype->max_lower);

  /* case translation arrays */
  fp_putsym(fp, "&transformation_toupper[1]");
  fp_putsym(fp, "&transformation_tolower[1]");
  fp_putsym(fp, "&masks[1]");

  if (n_idx > 0) {
    fp_putsym(fp, "qmask");
    fp_putsym(fp, "qidx");
  } else {
    fp_putsym(fp, 0);
    fp_putsym(fp, 0);
  }
  fp_putdig(fp, lc_ctype->qidx_hbound);

  fp_putdig(fp, lc_ctype->nbinds);		/* nbinds */
  fp_putsym(fp, "(_LC_bind_table_t*)bindtab");	/* bindtab */
  /* trans name mapping */
  fprintf(fp, "\t/*  transformations */\n");
  fprintf(fp, "\t%d,\n", lc_ctype->ntrans);
  fp_putsym(fp, "transname");
  fp_putsym(fp, "transtabs");
  /* ctype[] table */
  fprintf(fp, "\tSZ_TOTAL,\n");
  fp_putsym(fp, "ctype_compat_table");
  fprintf(fp, "\t{\n");
  fprintf(fp, "\t(void *)0,	/* reserved for future use */\n");
  fp_putsym(fp, 0);
  fp_putsym(fp, 0);
  fp_putsym(fp, 0);
  fp_putsym(fp, 0);
  fp_putsym(fp, 0);
  fp_putsym(fp, 0);
  fp_putsym(fp, 0);
  fprintf(fp, "\t}\n");
  fprintf(fp, "};\n\n");
}

static int
coll_eq( wchar_t last, wchar_t prev )
{
    wchar_t w;

    if (last == IGNORE )
      return (prev == IGNORE);
    else if (last == UNDEFINED )
      return (prev == UNDEFINED);

    w = prev + 1;			/* This breaks with >>65k codepoints! */
    if ( (w & 0xff) == 0) w += 1;

    return (last == w);
}

/*
*  FUNCTION: compress_collate
*
*  DESCRIPTION:
*	Search the collation table and eliminate duplicate entries from the
*	"tail"
*/
static void compress_collate( _LC_collate_t *coll)
{
    int	i,j;

    /*
     * Check that collation table does exist
     */
    if ( !coll->co_coltbl )
        return;

    /*
     * Reverse search table looking for first code-point that has different
     * collation information.  Since the trailing entries could be assigned
     * explicit adjacent values -or- be IGNORED, it's necessary to check for
     * these as special cases.
     */

    for ( i=max_wchar_enc; i>0; i--) {

	if ( coll->co_coltbl[i].ct_collel != coll->co_coltbl[i-1].ct_collel )
	    /*
	     * Different string substitution symbols
	     */
	    goto nomatch;

	for (j = 0; j<= (int)((unsigned int)coll->co_nord); j++) {
	    if ( coll->co_nord < _COLL_WEIGHTS_INLINE ) {
		if ( !coll_eq( coll->co_coltbl[i].ct_wgt.n[j],
			      coll->co_coltbl[i-1].ct_wgt.n[j]))
		    goto nomatch;
	     } else {
		 if ( !coll_eq( coll->co_coltbl[i].ct_wgt.p[j],
			        coll->co_coltbl[i-1].ct_wgt.p[j]))
		     goto nomatch;
	     }
	}
    }

    /*
     * At this point, "i" indexes the last shared collation weight
     */
nomatch:
    coll->co_hbound = i;
    return;
}

static void fp_putcolflag(FILE *fp, int flags)
{

#define PF(sym) ((flags & (sym)) ? #sym : "0")

    fprintf(fp, "%s | %s | %s | %s | %s,",
	    PF(_COLL_FORWARD_MASK),
	    PF(_COLL_BACKWARD_MASK),
	    PF(_COLL_NOSUBS_MASK),
	    PF(_COLL_POSITION_MASK),
	    PF(_COLL_SUBS_MASK));

#undef PF
}
/*
* FUNCTION: fp_putsubsflag
*
*	Prints the collating substitution flags symbolically
*/
static void fp_putsubsflag(FILE *fp, int flags)
{
    switch (flags) {
      case (_SUBS_ACTIVE|_SUBS_REGEXP):
				fputs("_SUBS_REGEXP|", fp);	/*DROP THRU */
      case _SUBS_ACTIVE:	fputs("_SUBS_ACTIVE,", fp);	break;

      case _SUBS_REGEXP:	fputs("_SUBS_REGEXP,", fp);	break;
      default:			fputs("0",fp);
    }
}

/* 
*  FUNCTION: gen_collate
*
*  DESCRIPTION:
*  Generate C code which implements the _LC_collate_t locale 
*  data structure.
*/
static void gen_collate(FILE *fp, _LC_collate_t *coll)
{
    int i, j, k;

    /* 
      Generate local definitions of _LC_coltbl_t, _LC_collel_t, and
      _LC_weight_t to handle the non-default side of the _LC_weight_t union.
      This is necessary to allow auto-initialization of the data types.
    */
    
    /* lc_weight_t */
    if (coll->co_nord < _COLL_WEIGHTS_INLINE) {
      /* you need to make this a union in order to get the correct data
	 alignment from the compiler */
      fprintf(fp, "typedef union {\n\twchar_t x[%d];\n\tconst wchar_t *p;\n"
	      , _COLL_WEIGHTS_INLINE);
    } else
	fprintf(fp, "typedef struct { const wchar_t *p;\n");

    fprintf(fp, "} lc_weight_t;\n");

    /* lc_collel_t */
    fprintf(fp, "typedef struct {\n");
    fprintf(fp, "\tchar *ce_sym;\n\tlc_weight_t ce_wgt;\n");
    fprintf(fp, "} lc_collel_t;\n");

    /* lc_coltbl_t */
    fprintf(fp, "typedef struct {\n");
    fprintf(fp, "\tlc_weight_t ct_wgt;\n\tconst lc_collel_t *ct_collel;\n");
    fprintf(fp, "} lc_coltbl_t;\n");

    /* lc_subs_t */
    fprintf(fp, "typedef struct {\n");
    fprintf(fp, "\tlc_weight_t ss_act;\n");
    fprintf(fp, "\tchar *ss_src;\n");
    fprintf(fp, "\tchar *ss_tgt;\n");
    fprintf(fp, "} lc_subs_t;\n\n");
    
    /* lc_collate_t */
    fprintf(fp, "typedef struct {\n");
    fprintf(fp, "\t_LC_core_collate_t core;\n");
    fprintf(fp, "\t_LC_charmap_t *cmapp;\n");
    fprintf(fp, "\tunsigned char co_nord;\n");
    fprintf(fp, "\tlc_weight_t co_sort;\n");
    fprintf(fp, "\twchar_t co_wc_min;\n");
    fprintf(fp, "\twchar_t co_wc_max;\n");
    fprintf(fp, "\twchar_t co_hbound;\n");
    fprintf(fp, "\twchar_t co_col_min;\n");
    fprintf(fp, "\twchar_t co_col_max;\n");
    fprintf(fp, "\tconst lc_coltbl_t *co_coltbl;\n");
    fprintf(fp, "\tunsigned  char co_nsubs;\n");
    fprintf(fp, "\tconst lc_subs_t *co_subs;\n");
    fprintf(fp, "} lc_collate_t;\n");

    /*
      Generate code to implement collation elements.  
    */

    coll->co_hbound = max_wchar_enc;	/* Save upper bound. */

    fprintf(fp,
"/*------------------------- COLLELLS --------------------------------*/\n");

    /* 
      Generate weights arrays for orders >= _COLL_WEIGHTS_INLINE 
    */
    if (coll->co_coltbl != NULL && coll->co_nord >= _COLL_WEIGHTS_INLINE) {
	for (i=0; i<=coll->co_hbound; i++) {

	    /* if there are any collation elements beginning with i */
	    if (coll->co_coltbl[i].ct_collel != NULL) {
	    
		_LC_collel_t *ce;

		fprintf(fp, "static const wchar_t cew%d[][%d]={ ", i, coll->co_nord+1);

		for (j=0, ce=(_LC_collel_t *)&(coll->co_coltbl[i].ct_collel[j]); 
		     ce->ce_sym != NULL; 
		     ce= (_LC_collel_t *)&(coll->co_coltbl[i].ct_collel[++j])) {

		    /* 
		      Create collation weights for this collation element 
		    */
		    for (k=0; k<= (int)((unsigned int)coll->co_nord); k++)
			fprintf(fp, "%d, ", ce->ce_wgt.p[k]);

		    fprintf(fp, "\n");
		}
		fprintf(fp, "};\n");
	    }
	}
    }

    /*
      Generate collation elements
    */
    if (coll->co_coltbl != NULL) {
	for (i=0; i<=coll->co_hbound; i++) {

	    /* if there are any collation elements beginning with i */
	    if (coll->co_coltbl && coll->co_coltbl[i].ct_collel != NULL) {
	    
		_LC_collel_t *ce;
	    
		fprintf(fp, "static const lc_collel_t ce%d[]={\n", i);

		/* one entry for each collation elementing beginning with i */
		for (j=0, ce=(_LC_collel_t *)&(coll->co_coltbl[i].ct_collel[j]); 
		     ce->ce_sym != NULL; 
		     ce = (_LC_collel_t *)&(coll->co_coltbl[i].ct_collel[++j])) {

		    fprintf(fp, "{ \"%s\", ", ce->ce_sym);
		    if (coll->co_nord >= _COLL_WEIGHTS_INLINE) 
		      fprintf(fp, "{&cew%d[%d][0]} },\n", i, j);
		    else {
			int k;
			fprintf(fp, "{ ");
			for (k=0;k<_COLL_WEIGHTS_INLINE; k++)
			  fprintf(fp, "%d, ", ce->ce_wgt.n[k]);
			fprintf(fp, "} },\n");
		    }
		}
		if (coll->co_nord >= _COLL_WEIGHTS_INLINE)
		    fprintf(fp, "{ 0, 0 },\n");
		else
		    fprintf(fp, "{ 0, { 0, 0 } },\n"); 

		fprintf(fp, "};\n");
	    }
	}
    }

    /*
      If the number of orders is >= _COLL_WEIGHTS_INLINE, then generate array
      of collation table weights
    */
    if (coll->co_nord >= _COLL_WEIGHTS_INLINE) {
	fprintf(fp,
"/*------------------------- COLLTBL WEIGHTS -------------------------*/\n");

	fprintf(fp, "static const wchar_t ct_wgts[][%d]={\n", coll->co_nord+1);
	for (i=0; i<=coll->co_hbound; i++) {
	    
	    for (j=0; j<= (int)((unsigned int)coll->co_nord); j++)
		fprintf(fp, "%3d, ", coll->co_coltbl[i].ct_wgt.p[j]);

	    fprintf(fp, "\n");

	}
	fprintf(fp, "};\n\n");
    }


    fprintf(fp,
"/*------------------------- COLLTBL ---------------------------------*/\n");
    if (coll->co_coltbl != NULL) {
#ifndef _NO_GCC_HACK	
	int line_count = 0;
#endif
	fprintf(fp, "static const lc_coltbl_t colltbl[] ={\n");

	for (i=0; i<=coll->co_hbound; i++) {
	    _LC_coltbl_t *ct;

#ifndef _NO_GCC_HACK
	    line_count++;
	    if ((line_count % 512) == 0) {
		fprintf(fp, "};\n");
		fprintf(fp, 
			"static const lc_coltbl_t colltbl_%d[] ={\n",
			line_count);
	    }
#endif
	    ct = (_LC_coltbl_t *) &(coll->co_coltbl[i]);

	    /* generate weight data */
	    if (coll->co_nord < _COLL_WEIGHTS_INLINE) {
	        fprintf(fp, "{ {{ ");
		for (j=0; j<_COLL_WEIGHTS_INLINE; j++)
		  fprintf(fp, "%d, ", ct->ct_wgt.n[j]);
		fprintf(fp, "}}, ");
	    } else
		fprintf(fp, "{ ct_wgts[%d], ", i);
		
	    /* generate collation elements if present */
	    if (ct->ct_collel != NULL)
		fprintf(fp, "ce%d },\n", i);
	    else
		fprintf(fp, "0 },\n");
	}
	fprintf(fp, "};\n");
    }
    
    if (coll->co_coltbl != NULL && coll->co_nsubs > 0) {
	fprintf(fp,
"/*------------------------- SUBSTITUTION STR-------------------------*/\n");

	/* 
	  generate substitution action arrays
	*/
	if (coll->co_nord >= _COLL_WEIGHTS_INLINE && coll->co_nsubs >0) {
	    fprintf(fp, "static const wchar_t subs_wgts[][%d]={\n", coll->co_nord+1);
	    for (i=0; i< (int)((unsigned int)coll->co_nsubs); i++) {
		for (j=0; j<= (int)((unsigned int)coll->co_nord); j++)
		    fp_putsubsflag( fp, coll->co_subs[i].ss_act.p[j] );

		fprintf(fp, "\n");
	    }
	    fprintf(fp, "};\n");
	}
			
	fprintf(fp, "static const lc_subs_t substrs[] = {\n");
	for (i=0; i< (int)((unsigned int)coll->co_nsubs); i++) {

	    if (coll->co_nord < _COLL_WEIGHTS_INLINE) {
		fprintf(fp, "\t{ {");
	        for (j=0; j<= (int)((unsigned int)coll->co_nord); j++)
		    fp_putsubsflag( fp, coll->co_subs[i].ss_act.n[j] );

		fprintf(fp, "}, \"%s\", \"%s\" },\n", coll->co_subs[i].ss_src,
			coll->co_subs[i].ss_tgt );
	    }
	    else
		fprintf(fp, "\t{ subs_wgts[%d], \"%s\", \"%s\" },\n", 
			i,
			coll->co_subs[i].ss_src, 
			coll->co_subs[i].ss_tgt);
	}
	fprintf(fp, "};\n\n");
    }
    
    fprintf(fp,
"/*------------------------- COLLATE OBJECT  -------------------------*/\n");
    /*
      Generate sort order weights if necessary 
    */
    if (coll->co_coltbl != NULL && coll->co_nord >= _COLL_WEIGHTS_INLINE) {
	fprintf(fp, "static const wchar_t sort[] = {\n");
	for (i=0; i<= (int)((unsigned int)coll->co_nord); i++)
	    fp_putcolflag(fp, coll->co_sort.p[i]);

	fprintf(fp, "\n};\n");
    }
	
    fprintf(fp, "static _LC_methods_collate_t native_methods_collate={\n");

    /*
    fprintf(fp, "\t(short) %d,\n", coll->core.native_api->nmethods);
    fprintf(fp, "\t(short) %d,\n", coll->core.native_api->ndefined);
    */
    fprintf(fp, "\t(short) 0,\n");
    fprintf(fp, "\t(short) 0,\n");
    /* class methods */
    fp_putmeth(fp, COLLATE_STRCOLL);
    fp_putmeth(fp, COLLATE_STRXFRM);
    fp_putmeth(fp, COLLATE_WCSCOLL_AT_NATIVE);
    fp_putmeth(fp, COLLATE_WCSXFRM_AT_NATIVE);
    fp_putmeth(fp, COLLATE_FNMATCH);
    fp_putmeth(fp, COLLATE_REGCOMP);
    fp_putmeth(fp, COLLATE_REGERROR);
    fp_putmeth(fp, COLLATE_REGEXEC);
    fp_putmeth(fp, COLLATE_REGFREE);
    /* for future use */
    fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
    fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
    fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
    fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
    fprintf(fp, "\t%s\n",  PLACEHOLDER_STRING);
    fprintf(fp, "};\n\n");

    if (Native == FALSE) {
	fprintf(fp, "static _LC_methods_collate_t user_methods_collate={\n");

	/*
	fprintf(fp, "\t(short) %d,\n", coll->core.native_api->nmethods);
	fprintf(fp, "\t(short) %d,\n", coll->core.native_api->ndefined);
	*/
	fprintf(fp, "\t(short) 0,\n");
	fprintf(fp, "\t(short) 0,\n");
	/* class methods */
	fp_putmeth(fp, COLLATE_STRCOLL);
	fp_putmeth(fp, COLLATE_STRXFRM);
	fp_putmeth(fp, COLLATE_WCSCOLL);
	fp_putmeth(fp, COLLATE_WCSXFRM);
	fp_putmeth(fp, COLLATE_FNMATCH);
	fp_putmeth(fp, COLLATE_REGCOMP);
	fp_putmeth(fp, COLLATE_REGERROR);
	fp_putmeth(fp, COLLATE_REGEXEC);
	fp_putmeth(fp, COLLATE_REGFREE);
	/* for future use */
	fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
	fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
	fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
	fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
	fprintf(fp, "\t%s\n",  PLACEHOLDER_STRING);
	fprintf(fp, "};\n\n");
    }

    fprintf(fp, "static lc_collate_t lc_coll = {{\n");

    /* class core */
    fp_puthdr(fp, _LC_COLLATE, sizeof(_LC_collate_t));
    fp_putmeth(fp, COLLATE_COLLATE_INIT); /* _LC_collate_t *(*init)(); */
    fprintf(fp, "\t0,\t\t\t\t/* collate_destructor() */\n");
    /* fp_putmeth(fp, COLLATE_COLLATE_DESTRUCTOR); */
    if (Native == FALSE)
	fp_putsym(fp, "&user_methods_collate");
    else
	fp_putsym(fp, "&native_methods_collate");
    fp_putsym(fp, "&native_methods_collate");
    fp_putsym(fp, 0);		          /* void *data; */
    fprintf(fp,"\t},\t/* End core */\n");

    /* class extension */
    fp_putsym(fp, "&lc_cmap");            /* _LC_charmap_t *charmap; */
    fp_putdig(fp, coll->co_nord);
    if (coll->co_nord < _COLL_WEIGHTS_INLINE) {
	fprintf(fp, "\t{ ");
	for (i=0; i<= (int)((unsigned int)coll->co_nord); i++)
	    fp_putcolflag(fp, coll->co_sort.n[i]);

	fprintf(fp, "},\n");
    }
    else
        fp_putsym(fp, "sort");
    
    fp_putdig(fp, 0);			      /* wchar_t co_wc_min; */
    fp_putdig(fp, max_wchar_enc);	      /* wchar_t co_wc_max; */
    fp_putdig(fp, coll->co_hbound);	      /* wchar_t co_hbound; */

    if (coll->co_coltbl != NULL) {

	fprintf(fp, "\t0x%04x,\n",
		coll->co_col_min);	      /* wchar_t co_col_min; */
	fprintf(fp, "\t0x%04x,\n",
		coll->co_col_max);	      /* wchar_t co_col_max; */

	fp_putsym(fp, "colltbl");      /* _LC_coltbl_t *coltbl; */
    } else {

	fprintf(fp, "\t0x%04x,\n", 0);	      /* wchar_t co_col_min; */
	fprintf(fp, "\t0x%04x,\n",
		max_wchar_enc);		      /* wchar_t co_col_max; */
    
	fp_putsym(fp, 0);		      /* _LC_coltbl_t *coltbl; */
    }

    fp_putdig(fp, coll->co_nsubs);	      /* number of subs strs */
    if (coll->co_nsubs > 0)
        fp_putsym(fp, "substrs");	      /* substitution strings */
    else 
        fp_putsym(fp, 0);


    fprintf(fp, "};\n\n");
}


/* 
*  FUNCTION: gen_monetary
*
*  DESCRIPTION:
*  Generate C code which implements the _LC_monetary_t locale 
*  data structure.
*/
static void gen_monetary(FILE *fp, _LC_monetary_t *lc_mon)
{

  fprintf(fp, 
"/*------------------------- MONETARY OBJECT  -------------------------*/\n");

  fprintf(fp, "static _LC_methods_monetary_t native_methods_monetary={\n");

  /*
  fprintf(fp, "\t(short) %d,\n", lc_mon->core.native_api->nmethods);
  fprintf(fp, "\t(short) %d,\n", lc_mon->core.native_api->ndefined);
  */
  fprintf(fp, "\t(short) 0,\n");
  fprintf(fp, "\t(short) 0,\n");
  /* class methods */
  fprintf(fp, "\t(char *(*) ()) 0,\n");	  /* char *(*nl_langinfo)(); */
  fp_putmeth(fp, MONETARY_STRFMON);	
  /* for future use */
  fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
  fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
  fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
  fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
  fprintf(fp, "\t%s\n",  PLACEHOLDER_STRING);
  fprintf(fp, "};\n\n");

  fprintf(fp, "static _LC_monetary_t lc_mon={{\n");

  /* class core */
  fp_puthdr(fp, _LC_MONETARY, sizeof(_LC_monetary_t));
  fp_putmeth(fp, MONETARY_MONETARY_INIT);	
  fprintf(fp, "\t0,\t\t\t\t/* monetary_destructor() */\n");
  /* fp_putmeth(fp, MONETARY_MONETARY_DESTRUCTOR); */
  fp_putsym(fp, "&native_methods_monetary");
  fp_putsym(fp, "&native_methods_monetary");
  fp_putsym(fp, 0);				
  fprintf(fp,"\t},\t/* End core */\n");

  /* class extension */
  fp_putstr(fp, lc_mon->int_curr_symbol);   /* char *int_curr_symbol; */
  fp_putstr(fp, lc_mon->currency_symbol);   /* char *currency_symbol; */
  fp_putstr(fp, lc_mon->mon_decimal_point); /* char *mon_decimal_point; */
  fp_putstr(fp, lc_mon->mon_thousands_sep); /* char *mon_thousands_sep; */
  fp_putstr(fp, lc_mon->mon_grouping);	    /* char *mon_grouping; */
  fp_putstr(fp, lc_mon->positive_sign);	    /* char *positive_sign; */
  fp_putstr(fp, lc_mon->negative_sign);	    /* char *negative_sign; */
  fp_putdig(fp, lc_mon->int_frac_digits);   /* signed char int_frac_digits; */
  fp_putdig(fp, lc_mon->frac_digits);	    /* signed char frac_digits; */
  fp_putdig(fp, lc_mon->p_cs_precedes);	    /* signed char p_cs_precedes; */
  fp_putdig(fp, lc_mon->p_sep_by_space);    /* signed char p_sep_by_space; */
  fp_putdig(fp, lc_mon->n_cs_precedes);	    /* signed char n_cs_precedes; */
  fp_putdig(fp, lc_mon->n_sep_by_space);    /* signed char n_sep_by_space; */
  fp_putdig(fp, lc_mon->p_sign_posn);	    /* signed char p_sign_posn; */
  fp_putdig(fp, lc_mon->n_sign_posn);	    /* signed char n_sign_posn; */

  fprintf(fp, "};\n");
}


/* 
*  FUNCTION: gen_time
*
*  DESCRIPTION:
*  Generate C code which implements the _LC_time_t locale data structure.
*/
static void gen_time(FILE *fp, _LC_time_t *lc_time)
{
    int i;


    if ( lc_time->era ) {
	fprintf(fp, "static char * era_strings[] = {\n");
	for(i=0; lc_time->era[i]; i++) {
	    fprintf(fp, "\t\"%s\"", lc_time->era[i]);
	    if (lc_time->era[i + 1] != NULL)
		fprintf(fp, "\n\t\";\"\n");
	    else
		fprintf(fp, ",\n");
	}
	fprintf(fp, "(char *)0 };\n"); 	    /* Terminate with NULL */
    }

    fprintf(fp, 
"/*-------------------------   TIME OBJECT   -------------------------*/\n");

    fprintf(fp, "static _LC_methods_time_t native_methods_time={\n");

    /*
    fprintf(fp, "\t(short) %d,\n", lc_time->core.native_api->nmethods);
    fprintf(fp, "\t(short) %d,\n", lc_time->core.native_api->ndefined);
    */
    fprintf(fp, "\t(short) 0,\n");
    fprintf(fp, "\t(short) 0,\n");
    /* class methods */
    fprintf(fp, "\t(char *(*) ()) 0,\n");  /* char *(*nl_langinfo)(); */
    fp_putmeth(fp, TIME_STRFTIME);	   /* size_t *(strftime)();   */
    fp_putmeth(fp, TIME_STRPTIME);	   /* char *(*strptime)();    */
    fp_putmeth(fp, TIME_GETDATE);	   /* struct tm *(*getdate)() */
    fp_putmeth(fp, TIME_WCSFTIME);	   /* size_t (*wcsftime)();   */
    /* for future use */
    fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
    fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
    fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
    fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
    fprintf(fp, "\t%s\n",  PLACEHOLDER_STRING);
    fprintf(fp, "};\n\n");

    fprintf(fp, "static _LC_time_t lc_time={{\n");
  
    /* class core */
    fp_puthdr(fp, _LC_TIME, sizeof(_LC_time_t));
    fp_putmeth(fp, TIME_TIME_INIT);        /* _LC_time_t *(*init)()   */
    fprintf(fp, "\t0,\t\t\t\t/* time_destructor() */\n");
    /* fp_putmeth(fp, TIME_TIME_DESTRUCTOR); */
    fp_putsym(fp, "&native_methods_time");
    fp_putsym(fp, "&native_methods_time");
    fp_putsym(fp, 0);			   /* void *data;             */
    fprintf(fp,"\t},\t/* End core */\n");
    
    /* class extension */
    fp_putstr(fp,lc_time->d_fmt);            /* char *d_fmt; */
    fp_putstr(fp, lc_time->t_fmt);	     /* char *t_fmt; */
    fp_putstr(fp, lc_time->d_t_fmt);	     /* char *d_t_fmt; */
    fp_putstr(fp, lc_time->t_fmt_ampm);	     /* char *t_fmt_ampm; */
    fprintf(fp, "\t{\n");
    for (i=0; i<7; i++) 
	fp_putstr(fp, lc_time->abday[i]);    /* char *abday[7]; */
    fprintf(fp, "\t},\n");
    fprintf(fp, "\t{\n");
    for (i=0; i<7; i++) 
	fp_putstr(fp, lc_time->day[i]);	     /* char *day[7]; */
    fprintf(fp, "\t},\n");
    fprintf(fp, "\t{\n");
    for (i=0; i<12; i++) 
	fp_putstr(fp, lc_time->abmon[i]);    /* char *abmon[12]; */
    fprintf(fp, "\t},\n");
    fprintf(fp, "\t{\n");
    for (i=0; i<12; i++) 
	fp_putstr(fp, lc_time->mon[i]);	     /* char *mon[12]; */
    fprintf(fp, "\t},\n");
    fprintf(fp, "\t{\n");
    for (i=0; i<2; i++) 
	fp_putstr(fp, lc_time->am_pm[i]);    /* char *am_pm[2]; */
    fprintf(fp, "\t},\n");

    if(lc_time->era)
	fp_putsym(fp, "era_strings"); 	     /* There is an array of eras */
    else
	fp_putsym(fp, 0);		     /* No eras available */

    fp_putstr(fp, lc_time->era_d_fmt);	     /* char *era_d_fmt; */

    if (lc_time->alt_digits == (char *) NULL)  /* char *alt_digits */
    	fprintf(fp, "\t\"\",\n");
    else {
	fprintf(fp, "\t\"");
    	for (i = 0; lc_time->alt_digits[i] != (char) NULL; i++) {
		if (lc_time->alt_digits[i] != ';')
			fputc(lc_time->alt_digits[i], fp);
		else
			fprintf(fp, "\"\n\t\t\";\"\n\t\t\"");
    	}
    	fprintf(fp, "\",\n");
    }
    fp_putstr(fp, lc_time->era_d_t_fmt);     /* char *era_d_t_fmt */
    fp_putstr(fp, lc_time->era_t_fmt);	     /* char *era_t_fmt */
    fp_putstr(fp, lc_time->date_fmt);	     /* char *date_fmt */
    fprintf(fp, "};\n");
}


/* 
*  FUNCTION: gen_numeric
*
*  DESCRIPTION:
*  Generate C code which implements the _LC_numeric_t locale 
*  data structure.
*/
static void gen_numeric(FILE *fp, _LC_numeric_t *lc_num)
{
  fprintf(fp, 
"/*------------------------- NUMERIC OBJECT  -------------------------*/\n");

  fprintf(fp, "static _LC_methods_numeric_t native_methods_numeric={\n");

  /*
  fprintf(fp, "\t(short) %d,\n", lc_num->core.native_api->nmethods);
  fprintf(fp, "\t(short) %d,\n", lc_num->core.native_api->ndefined);
  */
  fprintf(fp, "\t(short) 0,\n");
  fprintf(fp, "\t(short) 0,\n");
  /* class methods */
  fprintf(fp, "\t(char *(*) ()) 0,\n");	  /* char *(*nl_langinfo)(); */
  /* for future use */
  fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
  fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
  fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
  fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
  fprintf(fp, "\t%s\n",  PLACEHOLDER_STRING);
  fprintf(fp, "};\n\n");

  fprintf(fp, "static _LC_numeric_t lc_num={{\n");

  /* class core */
  fp_puthdr(fp, _LC_NUMERIC, sizeof(_LC_numeric_t));
  fp_putmeth(fp, NUMERIC_NUMERIC_INIT);
  fprintf(fp, "\t0,\t\t\t\t/* numeric_destructor() */\n");
  /* fp_putmeth(fp, NUMERIC_NUMERIC_DESTRUCTOR); */
  fp_putsym(fp, "&native_methods_numeric");
  fp_putsym(fp, "&native_methods_numeric");
  fp_putsym(fp, 0);			   /* void *data; */
  fprintf(fp,"\t},\t/* End core */\n");

  /* class extension */

  fp_putstr(fp, lc_num->decimal_point);
  fp_putstr(fp, lc_num->thousands_sep);	    /* char *thousands_sep; */

  fprintf(fp, "\t(char *)");
  fp_putstr(fp, (char *)lc_num->grouping);  /* (unsigned char *) grouping;      */

  fprintf(fp, "};\n\n");
}


/* 
*  FUNCTION: gen_msg
*
*  DESCRIPTION:
*  Generate C code which implements the _LC_messages_t locale data structure.
*/
static void gen_msg(FILE *fp, _LC_messages_t *lc_messages)
{
  fprintf(fp, 
"/*------------------------- MESSAGE OBJECT  -------------------------*/\n");

  fprintf(fp, "static _LC_methods_messages_t native_methods_messages={\n");

  /*
  fprintf(fp, "\t(short) %d,\n", lc_messages->core.native_api->nmethods);
  fprintf(fp, "\t(short) %d,\n", lc_messages->core.native_api->ndefined);
  */
  fprintf(fp, "\t(short) 0,\n");
  fprintf(fp, "\t(short) 0,\n");
  /* class methods */
  fprintf(fp, "\t(char *(*) ()) 0,\n");  /* char *(*nl_langinfo)(); */
  /* fore future use */
  fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
  fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
  fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
  fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
  fprintf(fp, "\t%s\n",  PLACEHOLDER_STRING);
  fprintf(fp, "};\n\n");

  fprintf(fp, "static _LC_messages_t lc_messages={{\n");

  /* class core */
  fp_puthdr(fp, _LC_MESSAGES, sizeof(_LC_messages_t));
  fp_putmeth(fp, RESP_RESP_INIT);
  fprintf(fp, "\t0,\t\t\t\t/* messages_destructor() */\n");
  /* fp_putmeth(fp, RESP_RESP_DESTRUCTOR); */
  fp_putsym(fp, "&native_methods_messages");
  fp_putsym(fp, "&native_methods_messages");
  fp_putsym(fp, 0);			     /* void *data;               */
  fprintf(fp,"\t},\t/* End core */\n");

  /* class extension */
  fp_putstr(fp, lc_messages->yesexpr);	    /* char *yesexpr;            */
  fp_putstr(fp, lc_messages->noexpr);	    /* char *noexpr;             */
  fp_putstr(fp, lc_messages->yesstr);           /* char *yesstr;             */
  fp_putstr(fp, lc_messages->nostr);	    /* char *nostr;              */

  fprintf(fp, "};\n\n");
}


/* 
*  FUNCTION: gen_locale
*
*  DESCRIPTION:
*  Generate C code which implements the _LC_locale_t locale
*  data structures. This routine collects the data from the various
*  child classes of _LC_locale_t, and outputs the pieces from the child
*  classes as appropriate.
*/
static void
gen_locale(FILE *fp, _LC_charmap_t *lc_cmap, _LC_collate_t *lc_coll,  
	   _LC_ctype_t *lc_ctype, _LC_monetary_t *lc_mon, 
	   _LC_numeric_t *lc_num, _LC_messages_t *lc_messages, 
	   _LC_time_t *lc_time
	   )
{
  int i;

  fprintf(fp, 
"/*-------------------------- LOCALE OBJECT --------------------------*/\n");

  fprintf(fp, "static _LC_methods_locale_t native_methods_locale={\n");

  /*
  fprintf(fp, "\t(short) 7,\n");
  fprintf(fp, "\t(short) 2,\n");
  */
  fprintf(fp, "\t(short) 0,\n");
  fprintf(fp, "\t(short) 0,\n");
  /* class methods */
  fp_putmeth(fp, LOCALE_NL_LANGINFO);
  fp_putmeth(fp, LOCALE_LOCALECONV);
  /* for future use */
  fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
  fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
  fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
  fprintf(fp, "\t%s,\n", PLACEHOLDER_STRING);
  fprintf(fp, "\t%s\n",  PLACEHOLDER_STRING);
  fprintf(fp, "};\n\n");

  fprintf(fp, "static _LC_locale_t lc_loc={{\n");

  /* class core */
  fp_puthdr(fp, _LC_LOCALE, sizeof(_LC_locale_t));
  fp_putmeth(fp, LOCALE_LOCALE_INIT);
  fprintf(fp, "\t0,\t\t\t\t/* locale_destructor() */\n");
  /* fp_putmeth(fp, LOCALE_LOCALE_DESTRUCTOR); */
  fp_putsym(fp, "&native_methods_locale");
  fp_putsym(fp, "&native_methods_locale");
  fp_putsym(fp, 0);
  fprintf(fp,"\t},\t/* End core */\n");

					   /* -- lconv structure -- */
					   /* filled in by setlocale() */
  fp_putsym(fp, 0);		   	   /* struct lconv *lc_lconv; */

					   /* pointers to other classes */
  fp_putsym(fp, "&lc_cmap");		   /* _LC_charmap_t *charmap; */
  if (lc_collate_flag != 0)
	fp_putsym(fp, "(_LC_collate_t *)&lc_coll");/* _LC_collate_t *collate; */
  else
	fp_putsym(fp, 0);
  if (lc_ctype_flag != 0)
	fp_putsym(fp, "&lc_ctype");	   /* _LC_ctype_t *ctype; */
  else
	fp_putsym(fp, 0);
  if (lc_monetary_flag != 0)
	fp_putsym(fp, "&lc_mon");	   /* _LC_monetary_t *monetary; */
  else
	fp_putsym(fp, 0);
  if (lc_numeric_flag != 0)
	fp_putsym(fp, "&lc_num");	   /* _LC_numeric_t *numeric; */
  else
	fp_putsym(fp, 0);
  if(lc_message_flag != 0)
	fp_putsym(fp, "&lc_messages");	   /* _LC_messages_t *messages; */
  else
	fp_putsym(fp, 0);
  if (lc_time_flag != 0)
	fp_putsym(fp, "&lc_time");	   /* _LC_time_t *time; */
  else
	fp_putsym(fp, 0);

  /* class extension */
  fprintf(fp, "\t%d,\n", _NL_NUM_ITEMS);
  fprintf(fp, "\t{");			   /* Bracket array of nl_langinfo stuff */
  fprintf(fp, "  /* nl_langinfo[] filled in at setlocale() time */\n");

  for (i = 0; i < _NL_NUM_ITEMS; i++)
	fp_putsym(fp, 0);

  fprintf(fp, "\t},\n");

  fprintf(fp, "};\n\n");
}


/* 
*  FUNCTION: gen_instantiate
*
*  DESCRIPTION:
*  Generates C code which returns address of lc_locale and serves as
*  entry point to object.
*/
static void
gen_instantiate(FILE *fp)
{
  fprintf(fp, 
	  "_LC_locale_t *instantiate(void)\n{\n\treturn &lc_loc;\n}\n");
}
