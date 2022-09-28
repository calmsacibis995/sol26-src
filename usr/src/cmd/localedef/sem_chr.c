/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)sem_chr.c 1.15	96/08/16  SMI"

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
static char rcsid[] = "@(#)$RCSfile: sem_chr.c,v $ $Revision: 1.4.6.3 $ (OSF) $Date: 1992/12/11 14:36:56 $";
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
 * 1.14  com/cmd/nls/sem_chr.c, cmdnls, bos320, 9138320 9/11/91 16:33:58
 */

#include <limits.h>
#include <sys/localedef.h>
#include "err.h"
#include "symtab.h"
#include <string.h>
#include "semstack.h"
#include "locdef.h"
#include "method.h"

/* static used by wchar_defined() and define_wchar() */
/*	One bit per process code point */

#define MAX_PC		USHRT_MAX*32
#define setbit(n)	(defined_pcs[ n/CHAR_BIT ] |= 1<<(n%CHAR_BIT))
#define testbit(n)	(defined_pcs[ n/CHAR_BIT ] & 1<<(n%CHAR_BIT))

static unsigned char defined_pcs[(MAX_PC+CHAR_BIT)/CHAR_BIT];

extern int wc_from_fc(int);
extern int mbs_from_fc(char*, int);

extern void set_coll_wgt( _LC_weight_t *, wchar_t, int);

/*
 * for storing EUC min and max pc's for euc<-->dense pc conversions
 */
wchar_t	euc_cs1_min = 0x30001020, euc_cs1_max = 0;
wchar_t euc_cs2_min = 0x10000020, euc_cs2_max = 0;
wchar_t euc_cs3_min = 0x20001020, euc_cs3_max = 0;

extern _LC_charmap_t	charmap;
extern int		Native;

/*
*  FUNCTION: define_wchar
*
*  DESCRIPTION: 
*  Adds a wchar_t to the list of characters which are defined in the 
*  codeset which is being defined.
*/
void
define_wchar(wchar_t wc)
{
    extern int warn;

    if (wc > MAX_PC)
	INTERNAL_ERROR;

    if (testbit(wc)) {
	if (warn) 
	    diag_error(ERR_PC_COLLISION, wc);
    } else 
	setbit(wc);
}


/*
*  FUNCTION: wchar_defined
*
*  DESCRIPTION: 
*  Checks if a wide character has been defined.
*
*  RETURNS
*  TRUE if wide char defined, FALSE otherwise.
*/
int
wchar_defined(wchar_t wc)
{
    return testbit(wc);
}

/*
*  FUNCTION: define_all_wchars
*
*  DESCRIPTION
*	When there isn't a charmap, we permit all code points to be
*	implicitly defined.
*  RETURNS
*	None
*/

void
define_all_wchars(void)
{
    memset(defined_pcs, -1, sizeof(defined_pcs));
}


/*
 * CPRINT - copies a byte value 'v' to destination 'p', either directly if
 * 	    'csource' is FALSE, or converting non-printables to compilable
 * 	    values when 'csource' is TRUE.  The destination pointer is
 * 	    updated to point to the next free byte.
 */

#define CPRINT(p,v)	do {						   \
			    if (!csource ||				   \
				((v) != '\\' && v != '\"' && isascii(v) && isprint(v))) { \
			        *(p)++ = (v);				   \
				*(p) = '\0';				   \
			    } else					   \
			        (p) += sprintf((char *)(p), "\\x%02x\"\"",(unsigned char)(v));	   \
			} while(0)

/*
 * FUNCTION: evalsym
 *
 * DESCRIPTION:
 *	Takes the value of a character symbol <name> from the input source
 *	string and copies it into the destination buffer.  The 'csource' flag
 *	controls whether character values are converted into printable
 *	and compilable source form.
 *
 * RETURNS
 *	updated source pointer value.
 *	updates the dest pointer
 *
 */
static const char *
evalsym(  char **dst, const char *src, int csource) {

    extern char 	escape_char;
    extern symtab_t 	cm_symtab;
    symbol_t 		*s;
    char     		*id;

    int i = 0;

    /*
     * Process '<' symbolname '>'.  First determine true length
     * of the <symbolname> string
     */
    while (1) {

	for (; src[i] != '>' && src[i] != escape_char && src[i]; i++);

	if (src[i] == escape_char) {
	    i+=2;		/* Take two bytes verbatim */
	} else if (src[i] == '>') {
	    i++;		/* ONLY WAY OUT!! */
	    break;
	} else
	  error(ERR_BAD_STR_FMT, src);
    }			/* End of <string> form */

    id = MALLOC(char, i+1);
    strncpy(id, (char *)src, i);		/* Copy thru trailing '>' */
    id[i] = '\0';
	     
    s = loc_symbol(&cm_symtab, id, 0);

    if (s==NULL) 
      error(ERR_SYM_UNDEF, id);
    else if (s->sym_type != ST_CHR_SYM)
      error(ERR_WRONG_SYM_TYPE, id);
    else {
	int j;
	for (j=0; j<s->data.chr->len; j++)
	  CPRINT(*dst, s->data.chr->str_enc[j]);
    }

    free(id);

    src+=i;

    return (src);
}

static char *
real_copy_string(const char *src, int csource )
{
    extern char	yytext[];	/* use yytext for str buffer! */
    extern char	escape_char;
    char	*s1;

    int		i, j;
    char 	*endptr;		/* pointer to the number */
    int 	value = 0;

    s1 = (void *) yytext;

    while (*src != '\0') {

	while (*src != escape_char && *src != '<' && *src != '\0') {
	    CPRINT(s1, *src);
	    src++;
	}

	if (*src == escape_char) {
	       /*  If the character pointed to is the escape_char 
		   see if it is the beginning of a character constant
		   otherwise it is an escaped character that needs copied	
		   into the string */

	    switch (*++src) {

	      case 'd':		/* decimal constant - \d999 */
		src++;
		value = strtoul(src, &endptr, 10);

		if (endptr == src || (endptr-src) > 3)
		    diag_error( ERR_ILL_DEC_CONST, src );

		src = endptr;

		CPRINT(s1,value);
		break;

	      case 'x':		/* hex constant - \xdd */
		src++;

		/* Defend against C format 0xnnn */
		/*  should treat like \x0  'x' 'n' 'n' ... */

		if (src[0] == '0' && (src[1] == 'x' || src[1] == 'X')) {
		    *s1++ = '\0';
		    *s1 = '\0';
		    break;
		}

		value = strtoul( (char *)src, (char **)&endptr, 16 );
		
		if (endptr == src || (endptr - src) > 2 )
		    diag_error(ERR_ILL_HEX_CONST,src);

		src = endptr;

		CPRINT(s1, value);
		break;

	      case '0': case '1': case '2': case '3': /* octal constant - \777 */
	      case '4': case '5': case '6': case '7':

		value = strtoul( (char *)src, (char **)&endptr, 8 );
		
		if (src == endptr || (endptr-src) > 3 )
		    diag_error(ERR_ILL_OCT_CONST, src);

		src = endptr;

		CPRINT(s1, value);
		break;

	      case '8':
	      case '9':		/* \8.. and \9... are illegal */
		diag_error(ERR_ILL_OCT_CONST, src );
		src++;
		break;

	      case 0:
		error( ERR_BAD_STR_FMT, src );
		break;

	      case '>':
	      case '<':
	      case '\\':
	      case '\"':
	      case ';':
	      case ',':
		*s1++ = '\\';
		*s1++ = *src++;
		*s1 = '\0';
		break;

	      default:
		CPRINT(s1,*src);
		src++;
	    }			/* end switch(*++src) */
	}
	else if (*src == '<') {
	    src = evalsym( &s1, src, csource);
	}
	else {
	    *s1 = '\0';
	}
    }

    return strdup(yytext);
}

/* 
*  FUNCTION: copy_string
*
*  DESCRIPTION: 
*  Copies a string and replaces occurances of character references with
*  the encoding of that character.  For example:
*  "<A>" would get copied to "\x65".
*
*  If the value of the character is greater than 0x80 than the byte
*  value of the character will be copied into the string in the for
*  \x84"" instead of just putting the byte value into the string.
*
*  This routine malloc()s the storage to contain the copied string.
*/
char *
copy_string(const char *src) {

    return real_copy_string(src, TRUE );	/* Put out C source */
}


/* 
*  FUNCTION: copy
*
*  DESCRIPTION: 
*  Copies a string and replaces occurances of character references with
*  the encoding of that character.  For example:
*  "<A>" would get copied to "\x65".
*  
*  This routine does a straight copy of symbol and character constants
*  to their byte values. Nothing is replaced.
*
*/
char * copy(const char *source)
{
    return real_copy_string( source, FALSE ); /* Don't produce C source */
}

/* 
*  FUNCTION: sem_set_chr
*
*  DESCRIPTION:
*  This routine assigns the value of the mutli-byte strings contained 
*  on the top of the stack to the string pointer 's' passed as an
*  argument.  This routine is used to assign character encodings to 
*  character values such as 'mon_decimal_point'. The productions using
*  this sem-action implement statements like:
*
*     mon_decimal_point     <period>
*
*  This routine performs a malloc() to acquire the memory to contain
*  the bytes forming the character.
*
*/
void sem_set_chr(char **s)
{
    item_t *it;
    int len;

    it = sem_pop();
    if (it->type == SK_CHR) {
        *s = MALLOC(char, it->value.chr->len + 1);
        strncpy(*s, (char *)it->value.chr->str_enc, it->value.chr->len);
        (*s)[it->value.chr->len] = '\0';
    }
    else {
	if (it->type == SK_INT) {
	    *s = MALLOC(char, MB_LEN_MAX + 1);
	    len = mbs_from_fc(*s, it->value.int_no);
	}
	else
	    INTERNAL_ERROR;
    }
    
    destroy_item(it);
}

/* 
*  FUNCTION: sem_set_str_list
*
*  DESCRIPTION:
*  Assign the values of the 'n' strings on the top of the semantic
*  stack to the array of strings 's' passed as a paramter. This routine
*  is used to assign the values to items such as 'abday[7]', i.e. the 
*  seven days of the week.  The productions using this sem-action 
*  implement statements like:
*
*      abday  "Mon";"Tue";"Wed";"Thu";"Fri";"Sat";"Sun"
*
*  This routine performs a malloc() to acquire the memory to build
*  the array of strings.
*
*/
void
sem_set_str_lst(char **s, int n)
{
    item_t *it;
    int i;
    

    for (i=n-1, it=sem_pop(); it != NULL && i >= 0; i--, it=sem_pop()) {
	if (it->type != SK_STR)
	    INTERNAL_ERROR;
	
	if (it->value.str[0] != '\0')
	    s[i] = copy_string(it->value.str);
	else
	    s[i] = NULL;
	
	destroy_item(it);
    }
    
    if (i > 0) 
	error(ERR_N_SARGS, n, i);
    else { 
        if (it != NULL) { 
	    while (it != NULL) {
	        destroy_item(it);
	        it = sem_pop();
	    }
	    diag_error(ERR_TOO_MANY_ARGS,n);
        }
    }
}


/* 
*  FUNCTION: sem_set_str_cat
*
*  DESCRIPTION:
*  Concatenate the values of the 'n' strings on the top of the semantic
*  stack to the string 's' passed as a paramter. This routine
*  is used to assign the values to items such as 'alt_digits'.
*  The productions using this sem-action 
*  implement statements like:
*
*      abday  "<j4727>";"<j2929><j1676>";"<j2745><j2929><j2716>"
*
*  This routine performs a malloc() to acquire the memory to build
*  the string.
*
*/
void
sem_set_str_cat(char **s, int n)
{
    item_t *it;
    int i;
    char **arbp = MALLOC(char*, n+1);
    char **strings;
    int  total_string_length = 0;
    

    for (i=n-1, it=sem_pop(); it != NULL && i >= 0; i--, it=sem_pop()) {
	if (it->type != SK_STR)
	    INTERNAL_ERROR;
	
	if (it->value.str[0] != '\0')
	    arbp[i] = copy_string(it->value.str);
	else
	    arbp[i] = (char *) NULL;
	
	destroy_item(it);
    }
    arbp[n] = (char *) NULL;
    
    if (i > 0) 
	error(ERR_N_SARGS, n, i);
    else { 
        if (it != NULL) { 
	    while (it != NULL) {
	        destroy_item(it);
	        it = sem_pop();
	    }
	    diag_error(ERR_TOO_MANY_ARGS,n);
        }
    }

    /*
     * now concatenate all the strings together into one string
     */
    for (strings = arbp; *strings != NULL; strings++) {	  /* get the length */
	total_string_length += strlen(*strings);
	if ((strings + 1) != NULL)
		total_string_length++;   /* ; */
    }
    total_string_length++;      /* trailing \0 */
    *s = MALLOC(char, total_string_length);
    **s = (char) NULL;
    for (strings = arbp; *strings != NULL; strings++) {   /* concat strings */
	strcat(*s, *strings);
	if (*(strings + 1) != (char *) NULL)
		strcat(*s, ";");
    }
    for (strings = arbp; *strings != NULL; strings++) {
	free(*strings);
    }
    free(arbp);
}


/* 
*  FUNCTION: sem_set_str
*
*  DESCRIPTION:
*  Assign a value to a char pointer passed as a parameter from the value
*  on the top of the stack.  This routine is used to assign string values
*  to locale items such as 'd_t_fmt'.  The productions which use this 
*  sem-action implement statements like:
*
*     d_t_fmt    "%H:%M:%S"
*
*  This routine performs a malloc() to acquire the memory to contain 
*  the string.
*
*/
void
sem_set_str(char **s)
{
    item_t *it;
    
    it = sem_pop();
    if (it == NULL || it->type != SK_STR)
	INTERNAL_ERROR;
    
    if (it->value.str[0]!='\0')
	*s = copy_string(it->value.str);
    else
	*s = NULL;
    
    destroy_item(it);
}


/* 
   FUNCTION: sem_set_int

   DESCRIPTION:
   Assign a value to an int pointer passed as a parameter from the value
   on the top of the stack. This routine is used to assign values to
   integer valued locale items such as 'int_frac_digits'.  The productions
   using this sem-action implement statements like:

     int_frac_digits      -1

   The memory to contain the integer is expected to have been alloc()ed
   by the caller.
*/
void
sem_set_int(signed char *i)
{
    item_t *it;
    
    it = sem_pop();
    if (it == NULL || it->type != SK_INT)
	INTERNAL_ERROR;
    
    if (it->value.int_no != -1)	    /* -1 means default */
	*i = it->value.int_no;
    
    destroy_item(it);
}


/*
*  FUNCTION: sem_digit_list
*
*  DESCRIPTION:
*  Adds the next digit to the digit list on the semantic stack.  The digit 
*  list consists of n stacked int items and a count.  This routine expects
*  the next digit on top of the stack and the count immediately below it.
*  This routine swaps the top two stack items and increments the count.
*/
void
sem_digit_list(void)
{
    item_t *n_digits;
    item_t *next_digit;
	    
    /* swap digit and count and increment count */
    next_digit = sem_pop();
    n_digits = sem_pop();
    if (next_digit->type != SK_INT || n_digits->type != SK_INT)
	INTERNAL_ERROR;
    
    n_digits->value.int_no++;
    sem_push(next_digit);
    sem_push(n_digits);
}


/*
*  FUNCTION: sem_set_diglist
*
*  DESCRIPTION:
*  Creates a string of digits (each less than CHAR_MAX) and sets the argument
*  'group' to point to this string.  
*
*  This routine calls malloc() to obtain the memory to contain the digit 
*  list.
*/
void
sem_set_diglist(char **group)
{
    item_t *n_digits;
    item_t *next_digit;
    char   *buf;
    int    i;

    /* pop digit count off stack */
    n_digits = sem_pop();
    if (n_digits->type != SK_INT)
	INTERNAL_ERROR;

    /* allocate string to contain digit list */
    /* return string holds up to six \x99, followed by \xff<nul> */
    *group = MALLOC(char, (n_digits->value.int_no*4)+5);	/* Space for "\x99" */

    /* temp string big enough for all but the last \xff */
    buf   = MALLOC(char, (n_digits->value.int_no*4)+1);

    (*group)[0] = '\0';
    buf[0] = '\0';

    for (i=n_digits->value.int_no-1; i>=0; i--) {
	int value;
	
	next_digit = sem_pop();
	if (next_digit->type != SK_INT)
	    INTERNAL_ERROR;

	value = next_digit->value.int_no;

	/* 
	 * If -1 is present as last member, then use CHAR_MAX instead
	 */
	if (i==n_digits->value.int_no-1 && value == -1) 
	    value = CHAR_MAX;

	/*
	  Covert grouping digit to a char constant
	*/
	sprintf(buf, "\\x%02x", value);

	/* 
	  prepend this to grouping string 
	*/
	strcat(buf, *group);
	strcpy(*group, buf);

	destroy_item(next_digit);
    }

    destroy_item(n_digits);
    free(buf);
}


/* 
*  FUNCTION: sem_set_sym_val
*
*  DESCRIPTION:
*  Assigns a value to the symbol matching 'id'.  The type of the symbol
*  is indicated by the 'type' parameter.  The productions using this
*  sem-action implement statements like:
* 
*     <code_set_name>    "ISO8859-1"
*                 or
*     <mb_cur_max>       2
*
*  The function will perform a malloc() to contain the string 'type' is
*  SK_STR.
*/
void
sem_set_sym_val(char *id, int type)
{
    extern symtab_t cm_symtab;
    item_t *i;
    symbol_t *s;
    
    i = sem_pop();
    if (i==NULL) INTERNAL_ERROR;
    
    s = loc_symbol(&cm_symtab, id, 0);
    if (s==NULL) INTERNAL_ERROR;
    
    switch (type) {
      case SK_INT:
	s->data.ival = i->value.int_no;
	break;
      case SK_STR:
	s->data.str = copy_string(i->value.str);
	break;
      default:
	INTERNAL_ERROR;
    }
    
    destroy_item(i);
}


/* 
*  FUNCTION: sem_char_ref
*
*  DESCRIPTION:
*  This function pops a symbol of the symbol stack, creates a semantic
*  stack item which references the symbol and pushes the item on the
*  semantic stack.
*/
void sem_char_ref(void)
{
    symbol_t *s;
    item_t   *it;
    
    s = sym_pop();
    if (s==NULL)
	INTERNAL_ERROR;
    
    if (s->sym_type == ST_CHR_SYM)
	it = create_item(SK_CHR, s->data.chr);
    else {
	it = create_item(SK_INT, 0);
	error(ERR_WRONG_SYM_TYPE, s->sym_id);
    }
    sem_push(it);
}


/* 
*  FUNCTION: sem_symbol
*
*  DESCRIPTION:
*  Attempts to locate a symbol in the symbol table - if the symbol is not
*  found, then it creates a new symbol and pushes it on the symbol stack. 
*  Otherwise, the symbol located in the symbol table is pushed on the
*  symbol stack.  This routine is used for productions which may define or
*  redefine a symbol.
*/
void sem_symbol(char *s)
{
    extern symtab_t cm_symtab;
    symbol_t *sym;
    
    /* look for symbol in symbol table */
    sym = loc_symbol(&cm_symtab, s, 0);
    
    /* if not found, create a symbol */
    if (sym==NULL) {
	sym = create_symbol(s, 0);
	if (sym == NULL) 
	    INTERNAL_ERROR;

	sym->sym_type = ST_CHR_SYM;
	sym->data.chr = MALLOC(chr_sym_t, 1);
	sym->data.chr->fc_enc = *s;
    }
    
    /* whether new or old symbol, push on symbol stack */
    sym_push(sym);
}


/* 
*  FUNCTION: sem_existing_symbol
*
*  DESCRIPTION:
*  This function locates a symbol in the symbol table, creates a 
*  semantic stack item from the symbol, and pushes the new item on
*  the semantic stack.  If a symbol cannot be located in the symbol
*  table, an error is reported and a dummy symbol is pushed on the stack.
*/
void sem_existing_symbol(char *s)
{
    extern symtab_t cm_symtab;
    symbol_t *sym;
    
    /* look for symbol in symbol table */
    sym = loc_symbol(&cm_symtab, s, 0);
    
    /*
      if not found, create a symbol, write diagnostic, and set global
      error flag.
      */
    if (sym==NULL) {
	diag_error(ERR_SYM_UNDEF, s);
	
	sym = create_symbol(s, 0);
	if (sym == NULL) 
	    INTERNAL_ERROR;
	
	sym->sym_type = ST_CHR_SYM;
	sym->data.chr = MALLOC(chr_sym_t, 1);
	sym->data.chr->fc_enc = *s;
	sym->data.chr->len = mbs_from_fc( (char *)sym->data.chr->str_enc, *s);
	sym->data.chr->wc_enc = (unsigned char) *s;

	sym->data.chr->wgt = MALLOC(_LC_weight_t, 1);
	sym->data.chr->wgt->p = NULL;
	set_coll_wgt(sym->data.chr->wgt, UNDEFINED, -1);

	define_wchar( (unsigned char) *s );
	add_symbol( &cm_symtab, sym);
    }
    
    /* whether new or old symbol, push on symbol stack */
    sym_push(sym);
}


/* 
*  FUNCTION: sem_symbol_def
*
*  DESCRIPTION:
*  This routine is map a codepoint to a character symbol to implement
*  the 
*      <j0104>     \x81\x51
*  construct.
*
*  The routine expects to find a symbol and a numeric constant on the
*  stack.  From these two, the routine builds a character structure which
*  contains the length of the character in bytes, the file code and
*  process code representations of the character.  The character
*  structure is then pushed onto the semantic stack, and the
*  symbolic representation of the character added to the symbol table.
*
*  The routine also checks if this is the max process code yet
*  encountered, and if so resets the value of max_wchar_enc;
*/
void
sem_symbol_def()
{
    extern symtab_t cm_symtab;
    extern wchar_t  max_wchar_enc;
    extern int      max_disp_width;
    extern int      mb_cur_max;
    int      width;
    symbol_t *s, *t;
    item_t   *it;
    int      fc;		/* file code for character */
    wchar_t  pc;		/* process code for character */
    int      rc;		/* return value from mbtowc_xxx */
    wchar_t  eucpc;
    wchar_t  pc_from_bc;	/* pc from bc method */
    int	     rt_rc;		/* round trip return code */
    char     rt_char[MB_LEN_MAX + 2];	/* round trip character */
    wchar_t  eucpc_from_dense;	/* convert dense back into eucpc */
    
    s = sym_pop();		/* pop symbol off stack */
    it = sem_pop();		/* pop integer to assign off stack */

    /* get file code for character off semantic stack */
    fc = it->value.int_no;

    t = loc_symbol(&cm_symtab, s->sym_id, 0);
    if (t != NULL)
	if (t->data.chr->fc_enc != fc)
	    diag_error(ERR_DUP_CHR_SYMBOL, s->sym_id);

    /* create symbol */
    s->sym_type = ST_CHR_SYM;
    s->data.chr = MALLOC(chr_sym_t, 1);

    /* save integral file code representation of character */
    s->data.chr->fc_enc = fc;

    /* turn integral file code into character string */
    s->data.chr->len = mbs_from_fc((char *)s->data.chr->str_enc, fc);

    if (s->data.chr->len > mb_cur_max)
	error(ERR_CHAR_TOO_LONG, s->sym_id);
    
    /* get process code for this character */
    rc = INT_METHOD(METH_OFFS(CHARMAP_MBTOWC_AT_NATIVE))(&charmap, &pc,
					      s->data.chr->str_enc, 
					      MB_LEN_MAX);
								
    if (rc < 0)
	error(ERR_UNSUP_ENC,s->sym_id);

    s->data.chr->wc_enc = pc;

    /* reset max process code in codeset */
    if (pc > max_wchar_enc) {
	max_wchar_enc = pc;
	charmap.cm_eucinfo->dense_end = pc;
    }

    /*
     * Check the round trip.  Can we get back to the character?
     */

    rt_rc = INT_METHOD(METH_OFFS(CHARMAP_WCTOMB_AT_NATIVE))(&charmap,
						rt_char, pc);

    if (rt_rc < 0)
	fprintf(stderr, "wctomb() failed for %s\n",  s->sym_id);

    if (strncmp((const char *)s->data.chr->str_enc, rt_char, rt_rc) != 0)
	fprintf(stderr, "mbtowc()<-->wctomb() failed for %s\n", s->sym_id);

    /*
     * if we are doing euc bc then check the pc against the bc methods
     */
    if (Native == FALSE) {
	rc = INT_METHOD(METH_OFFS(CHARMAP_MBTOWC))(&charmap, &eucpc,
					      s->data.chr->str_enc, 
					      MB_LEN_MAX);
	if (rc < 0) {
	    fprintf(stderr, "%s failed to convert %s\n",
			METH_NAME(CHARMAP_MBTOWC), s->data.chr->str_enc);
	} else {
	    /*
	     * convert eucpc to dense pc and see if this dense pc matchs
	     * the one from above.
	     */
	    pc_from_bc =
		INT_METHOD(METH_OFFS(CHARMAP_EUCPCTOWC))(&charmap, eucpc);
	    if (pc != pc_from_bc) {
		fprintf(stderr, "pc's don't match for %s\n", s->sym_id);
	    }
	    /*
	     * convert this dense pc back into eucpc and see if it matches
	     * from  above
	     */
	    eucpc_from_dense =
		INT_METHOD(METH_OFFS(CHARMAP_WCTOEUCPC))(&charmap, pc_from_bc);
	    if (eucpc != eucpc_from_dense)
		fprintf(stderr, "eucpc's don't match between "
				"mbtowc() and eucpctowc() for character %s\n",
				s->sym_id);
	}
    }

    /* check display width and reset max if necessary */
    width = INT_METHOD(METH_OFFS(CHARMAP_WCWIDTH_AT_NATIVE))(&charmap, pc);

    if (width > max_disp_width)
	max_disp_width = width;

    /* mark character as defined */
    define_wchar(pc);

    destroy_item(it);
    
    add_symbol(&cm_symtab, s);
}

void
sem_symbol_def_euc()
{
    extern symtab_t cm_symtab;
    extern wchar_t  max_wchar_enc;
    extern int      max_disp_width;
    extern int      mb_cur_max;
    int      width;
    symbol_t *s, *t;
    item_t   *it;
    int      fc;		/* file code for character */
    wchar_t  pc;		/* process code for character */
    int      rc;		/* return value from mbtowc_xxx */
    
    s = sym_pop();		/* pop symbol off stack */
    it = sem_pop();		/* pop integer to assign off stack */

    /* get file code for character off semantic stack */
    fc = it->value.int_no;

    t = loc_symbol(&cm_symtab, s->sym_id, 0);
    if (t != NULL)
	if (t->data.chr->fc_enc != fc)
	    diag_error(ERR_DUP_CHR_SYMBOL, s->sym_id);

    /* create symbol */
    s->sym_type = ST_CHR_SYM;
    s->data.chr = MALLOC(chr_sym_t, 1);

    /* save integral file code representation of character */
    s->data.chr->fc_enc = fc;

    /* turn integral file code into character string */
    s->data.chr->len = mbs_from_fc((char *)s->data.chr->str_enc, fc);

    if (s->data.chr->len > mb_cur_max)
	error(ERR_CHAR_TOO_LONG, s->sym_id);

    /* get EUC process code for this character */
    rc = INT_METHOD(METH_OFFS(CHARMAP_MBTOWC))(&charmap, &pc,
					      s->data.chr->str_enc, 
					      MB_LEN_MAX);

    if (rc < 0)
	error(ERR_UNSUP_ENC,s->sym_id);

    s->data.chr->wc_enc = pc;

    /*
     * find min/max EUC codes
     */
    switch (pc & WCHAR_CSMASK) {
    case WCHAR_CS1:
	if (pc > euc_cs1_max)
		euc_cs1_max = pc;
	if (pc < euc_cs1_min)
		euc_cs1_min = pc;
	break;
    case WCHAR_CS2:
	if (pc > euc_cs2_max)
		euc_cs2_max = pc;
	if (pc < euc_cs2_min)
		euc_cs2_min = pc;
	break;
    case WCHAR_CS3:
	if (pc > euc_cs3_max)
		euc_cs3_max = pc;
	if (pc < euc_cs3_min)
		euc_cs3_min = pc;
	break;
    }

    destroy_item(it);
    free(s->data.chr);
    free(s);
}


/*
*  FUNCTION: extract_digit_list
*
*  DESCRIPTION:
*  This function returns the digit list which may be present in a symbol 
*  of the form <j0102> where 0102 is the desired digit list.
*
*  RETURNS:
*  Number of digits in digit list or 0 if no digit list is present.
*/
int
extract_digit_list(char *s, int *value)
{
    char *endstr;
    int  i;

    /* skip to first digit in list */
    for (i=0; s[i] != '\0' && !isdigit(s[i]); i++);

    /* digit list present? */
    if (s[i] == '\0') 
	return 0;

    /* determine value of digit list */
    *value = strtol(&(s[i]), &endstr, 10);

    /* make sure '>' immediately follows digit list */
    if (*endstr != '>') 
	return 0;

    /* return length of digit list */
    return endstr - &(s[i]);
}


/*
*  FUNCTION: build_symbol_fmt
*
*  DESCRIPTION:
*  This function builds a format strings which describes the symbol
*  passed as an argument.  This format is used to build intermediary
*  symbols required to fill the gaps in charmap statements like:
*     <j0104>...<j0106>
*
*  RETURNS:
*  Format string and 'start/end' which when used with sprintf() results
*  in symbol that looks like sym0->sym_id.
*/
char *
build_symbol_fmt(symbol_t *sym0, symbol_t *sym1, int *start, int *end)
{
    static char fmt[MAX_SYM_LEN+1];

    char *s;
    int  i;
    int  n_dig0;
    int  n_dig1;

    n_dig0 = extract_digit_list(sym0->sym_id, start);
    n_dig1 = extract_digit_list(sym1->sym_id, end);

    /* digit list present and same length in both symbols ? */
    if (n_dig0 != n_dig1 || n_dig0 == 0) 
	return NULL;

    /* the starting symbol is greater than the ending symbol */
    if (*start > *end)
	return NULL;

    /* build format from the start symbol */
    for (i=0, s=sym0->sym_id; !isdigit(s[i]); i++)
	fmt[i] = s[i];

    /* add to end of format "%0nd>" where n is no. of digits in list" */
    fmt[i++] = '%';
    sprintf(&(fmt[i]), "0%dd>", n_dig0);
    return fmt;
}


/*
*  FUNCTION: sem_symbol_range_def
*
*  DESCRIPTION:
*  This routine defines a range of symbol values which are defined via
*  the 
*     <j0104> ... <j0106>   \x81\x50
*  construct.
*/
void
sem_symbol_range_def(void)
{
    extern symtab_t cm_symtab;
    extern wchar_t  max_wchar_enc;
    extern int      max_disp_width;
    extern int      mb_cur_max;
    symbol_t *s, *s0, *s1;	/* symbols pointers */
    item_t   *it;		/* pointer to mb encoding */
    int      width;		/* character display width */
    int      fc;		/* file code for character */
    wchar_t  pc;                /* process code for character */
    char     *fmt;		/* symbol format, e.g. "<%s%04d>" */
    char     tmp_name[MAX_SYM_LEN + 1];
				/* temporary holding array for symbol name */
    int      start;		/* starting symbol number */
    int      end;		/* ending symbol number */
    int      rc;
    int      i;
    wchar_t  eucpc;
    wchar_t  pc_from_bc;
    int	     rt_rc;		/* round trip return code */
    char     rt_char[MB_LEN_MAX + 2];	/* round trip character */
    wchar_t  eucpc_from_dense;

    s1 = sym_pop();		/* symbol at end of symbol range */
    s0 = sym_pop();		/* symbol at start of symbol range */
    it = sem_pop();		/* starting encoding */

    
    /* get file code for character off semantic stack 
     */
    fc = it->value.int_no;

    /* Check if beginning symbol has already been seen
     */
    s = loc_symbol(&cm_symtab, s0->sym_id, 0);
    if (s != NULL)
	if (s->data.chr->fc_enc != fc)
	    diag_error(ERR_DUP_CHR_SYMBOL, s0->sym_id);

    /* Determine symbol format for building intermediary symbols 
     */
    fmt = build_symbol_fmt(s0, s1, &start, &end);
 
    /* Check if ending symbol has already been seen
     */
    s = loc_symbol(&cm_symtab, s1->sym_id, 0);
    if (s != NULL)
	if (s->data.chr->fc_enc != fc + (end - start))
 	    diag_error(ERR_DUP_CHR_SYMBOL, s1->sym_id);

    /* invalid symbols in range ?
     */
    if (fmt==NULL)
	error(ERR_INVALID_SYM_RNG, s0->sym_id, s1->sym_id);
    
    for (i=start; i <= end; i++) {
	
	/* reuse previously allocated symbol 
	 */
	if (i==start)
	    s = s0;
	else if (i == end)
	    s = s1;
	else {
	    sprintf(tmp_name, fmt, i);
	    s = loc_symbol(&cm_symtab,tmp_name,0);
	    if (s != NULL) {
	        if (s->data.chr->fc_enc != fc) {
		   diag_error(ERR_DUP_CHR_SYMBOL,tmp_name);
	        }
	    }
	    else {
	        s = create_symbol(tmp_name, 0);
	        if (s0==NULL)
		    INTERNAL_ERROR;
	    }
	}

	/* flesh out symbol definition 
	 */
	s->sym_type = ST_CHR_SYM;
	s->data.chr = MALLOC(chr_sym_t, 1);
	
	/* save file code */
	s->data.chr->fc_enc = fc;

	/* turn ordinal file code into character string 
	 */
	s->data.chr->len = mbs_from_fc((char *)s->data.chr->str_enc, fc);

	if (s->data.chr->len > mb_cur_max)
	    error(ERR_CHAR_TOO_LONG, s->sym_id);
	
	/* get process code for this character 
	 */
	rc = INT_METHOD(METH_OFFS(CHARMAP_MBTOWC_AT_NATIVE))(&charmap, &pc, 
						s->data.chr->str_enc, 
						MB_LEN_MAX);

	if (rc>=0) {

	    s->data.chr->wc_enc = pc;

	    /* reset max process code in codeset
	     */
	    if (pc > max_wchar_enc) {
		max_wchar_enc = pc;
		charmap.cm_eucinfo->dense_end = pc;
	    }

	    /*
	     * if we are doing euc bc then check the pc against the bc methods
	     */
	    if (Native == FALSE) {
		rc = INT_METHOD(METH_OFFS(CHARMAP_MBTOWC))(&charmap, &eucpc,
					      s->data.chr->str_enc, 
					      MB_LEN_MAX);
		if (rc < 0) {
		    fprintf(stderr, "%s failed to convert %s\n",
			METH_NAME(CHARMAP_MBTOWC), s->data.chr->str_enc);
		} else {
		    /*
		     * convert eucpc to dense pc and see if this dense pc
		     * matches the one from above
		     */
		    pc_from_bc = INT_METHOD(METH_OFFS(CHARMAP_EUCPCTOWC))
							    (&charmap, eucpc);
		    if (pc != pc_from_bc) {
			fprintf(stderr, "pc's don't match for %s\n", s->sym_id);
		    }
		    /*
		     * convert this dense pc back into eucpc and see
		     * if it matches from  above
		     */
		    eucpc_from_dense =
			INT_METHOD(METH_OFFS(CHARMAP_WCTOEUCPC))(&charmap,
								pc_from_bc);
		    if (eucpc != eucpc_from_dense)
		    fprintf(stderr, "eucpc's don't match between "
				"mbtowc() and eucpctowc() for character %s\n",
				s->sym_id);
		}
	    }

	    /* check display width and reset max if necessary
	    */
	    width = INT_METHOD(METH_OFFS(CHARMAP_WCWIDTH_AT_NATIVE))
							(&charmap, pc);

	    if (width > max_disp_width)
	      max_disp_width = width;

	    /* mark character as defined
	     */
	    define_wchar(pc);

	    add_symbol(&cm_symtab, s);
	} else
	  diag_error(ERR_ILL_CHAR, s->data.chr->str_enc[0]);

	/*
	 * Check the round trip.  Can we get back to the character?
	 */

	rt_rc = INT_METHOD(METH_OFFS(CHARMAP_WCTOMB_AT_NATIVE))(&charmap,
						rt_char, pc);

	if (rt_rc < 0)
		fprintf(stderr, "wctomb() failed for %s\n",  s->sym_id);

	if (strncmp((const char *)s->data.chr->str_enc, rt_char, rt_rc) != 0)
		fprintf(stderr, "mbtowc()<-->wctomb() failed for %s\n",
								s->sym_id);

	/* get next file code */
	fc ++;
    }
    
    destroy_item(it);
}

void
sem_symbol_range_def_euc(void)
{
    extern symtab_t cm_symtab;
    extern wchar_t  max_wchar_enc;
    extern int      max_disp_width;
    extern int      mb_cur_max;
    symbol_t *s, *s0, *s1;	/* symbols pointers */
    item_t   *it;		/* pointer to mb encoding */
    int      width;		/* character display width */
    int      fc;		/* file code for character */
    wchar_t  pc;                /* process code for character */
    char     *fmt;		/* symbol format, e.g. "<%s%04d>" */
    char     tmp_name[MAX_SYM_LEN + 1];
				/* temporary holding array for symbol name */
    int      start;		/* starting symbol number */
    int      end;		/* ending symbol number */
    int      rc;
    int      i;

    s1 = sym_pop();		/* symbol at end of symbol range */
    s0 = sym_pop();		/* symbol at start of symbol range */
    it = sem_pop();		/* starting encoding */


    /* get file code for character off semantic stack
     */
    fc = it->value.int_no;

    /* Check if beginning symbol has already been seen
     */
    s = loc_symbol(&cm_symtab, s0->sym_id, 0);
    if (s != NULL)
	if (s->data.chr->fc_enc != fc)
	    diag_error(ERR_DUP_CHR_SYMBOL, s0->sym_id);

    /* Determine symbol format for building intermediary symbols
     */
    fmt = build_symbol_fmt(s0, s1, &start, &end);
 
    /* Check if ending symbol has already been seen
     */
    s = loc_symbol(&cm_symtab, s1->sym_id, 0);
    if (s != NULL)
	if (s->data.chr->fc_enc != fc + (end - start))
 	    diag_error(ERR_DUP_CHR_SYMBOL, s1->sym_id);

    /* invalid symbols in range ?
     */
    if (fmt==NULL)
	error(ERR_INVALID_SYM_RNG, s0->sym_id, s1->sym_id);
    
    for (i=start; i <= end; i++) {

	/* reuse previously allocated symbol 
	 */
	if (i==start)
	    s = s0;
	else if (i == end)
	    s = s1;
	else {
	    sprintf(tmp_name, fmt, i);
	    s = loc_symbol(&cm_symtab,tmp_name,0);
	    if (s != NULL) {
	        if (s->data.chr->fc_enc != fc) {
		   diag_error(ERR_DUP_CHR_SYMBOL,tmp_name);
	        }
	    }
	    else {
	        s = create_symbol(tmp_name, 0);
	        if (s0==NULL)
		    INTERNAL_ERROR;
	    }
	}

	/* flesh out symbol definition 
	 */
	s->sym_type = ST_CHR_SYM;
	s->data.chr = MALLOC(chr_sym_t, 1);
	
	/* save file code */
	s->data.chr->fc_enc = fc;

	/* turn ordinal file code into character string 
	 */
	s->data.chr->len = mbs_from_fc((char *)s->data.chr->str_enc, fc);

	if (s->data.chr->len > mb_cur_max)
	    error(ERR_CHAR_TOO_LONG, s->sym_id);
	
	/* get process code for this character 
	 */
	rc = INT_METHOD(METH_OFFS(CHARMAP_MBTOWC))(&charmap, &pc, 
						s->data.chr->str_enc, 
						MB_LEN_MAX);

	if (rc>=0)
	    s->data.chr->wc_enc = pc;
	else
	    diag_error(ERR_ILL_CHAR, s->data.chr->str_enc[0]);

	/*
	 * find min/max EUC codes
	 */
	switch (pc & WCHAR_CSMASK) {
	case WCHAR_CS1:
		if (pc > euc_cs1_max)
			euc_cs1_max = pc;
		if (pc < euc_cs1_min)
			euc_cs1_min = pc;
		break;
	case WCHAR_CS2:
		if (pc > euc_cs2_max)
			euc_cs2_max = pc;
		if (pc < euc_cs2_min)
			euc_cs2_min = pc;
		break;
	case WCHAR_CS3:
		if (pc > euc_cs3_max)
			euc_cs3_max = pc;
		if (pc < euc_cs3_min)
			euc_cs3_min = pc;
		break;
	}

	/* get next file code */
	fc ++;
    }

    destroy_item(it);
    free(s->data.chr);
    free(s);
}



/*
*  FUNCTION: wc_from_fc
*
*  DESCRIPTION
*  Convert a character encoding (as an integer) into a wide char
*/
int wc_from_fc(int fc)
{
    int i, j;
    wchar_t pc;
    unsigned char s[6];

    memset(s,0, sizeof(s));

    /* shift first non-zero byte to high order byte of int */
    for(i=0; ((fc & 0xff000000) == 0) && i < sizeof(int); i++)
	fc <<= 8;

    /* shift remaining bytes in int off top and into string */
    for (j=0; i < sizeof(int); j++,i++) {
	s[j] = ((unsigned int)fc & 0xff000000) >> 24;
	fc <<= 8;
    }

    /* perform some sanity checking here that can't be done by mbtowc() */
    if (j >= 2) {
	/* if first byte doesn't have high bit or second byte in restricted
	   range.
	*/
	if (s[0] < 0x80 || s[1] < 0x40)
	    return -1;
    }

    i = INT_METHOD(METH_OFFS(CHARMAP_MBTOWC_AT_NATIVE))
					(&charmap, &pc, s, MB_LEN_MAX);

    if (i < 0)
	return i;
    else
	return pc;
}


/*
*  FUNCTION: mbs_from_fc
*
*  DESCRIPTION
*  Convert an integral file code to a string.  The length of the character is 
*  returned.
*/
int mbs_from_fc(char *s, int fc)
{
    int     i, j;

    /* shift first non-zero byte to high order byte of int */
    for(i=0; ((fc & 0xFF000000) == 0) && i < sizeof(int); i++)
	fc <<= 8;
    
    if (i==sizeof(int)) {	/* special case for NUL character */
	s[0] = '\0'; s[1] = '\0';
	return 1;
    }

    /* shift remaining bytes in int off top and into string */
    for (j=0; i < sizeof(int); j++,i++) {
	s[j] = ((unsigned int)fc & 0xff000000) >> 24;
	fc <<= 8;
    }
    
    return j;
}


/* check for digits */
void 
check_digit_values(void)
{
	extern symtab_t cm_symtab;
	symbol_t *s;
	int fc0, fc1;
	wchar_t pc0, pc1;
	int i;
	char *digits[]={
	"<zero>",
	"<one>",
	"<two>",
	"<three>",
	"<four>",
	"<five>",
	"<six>",
	"<seven>",
	"<eight>",
	"<nine>" };

	s = loc_symbol(&cm_symtab,"<zero>",0);
	if (s == NULL)
	    INTERNAL_ERROR;
	fc0 = s->data.chr->fc_enc;
	pc0 = s->data.chr->wc_enc;
	for (i = 1; i <= 9; i++) {
	    s = loc_symbol(&cm_symtab,digits[i],0);
	    if (s == NULL)
		INTERNAL_ERROR;
	    fc1 = s->data.chr->fc_enc;
	    pc1 = s->data.chr->wc_enc;
	    if ((fc0 + 1) != fc1)
		diag_error(ERR_DIGIT_FC_BAD,digits[i],digits[i-1]);
	    if ((pc0 + 1) != pc1)
		diag_error(ERR_DIGIT_PC_BAD,digits[i],digits[i-1]);
	    fc0 = fc1;
	    pc0 = pc1;
	}
	return;
}


void
fill_euc_info(_LC_euc_info_t *euc_info)
{
    /*
     * set proper minimum EUC pc values
     */
    switch (euc_info->euc_bytelen1) {
    case 0:	euc_cs1_min = 0x0;	  break;
    case 1:	euc_cs1_min = 0x30000020; break;
    case 2:	euc_cs1_min = 0x30001020; break;
    case 3:	euc_cs1_min = 0x30081020; break;
    default:	fprintf(stderr, "unsupport length for EUC CS1 length=%d\n",
			euc_info->euc_bytelen1);
    }
    switch (euc_info->euc_bytelen2) {
    case 0:	euc_cs2_min = 0x0;	  break;
    case 1:	euc_cs2_min = 0x10000020; break;
    case 2:	euc_cs2_min = 0x10001020; break;
    case 3:	euc_cs2_min = 0x10081020; break;
    default:	fprintf(stderr, "unsupport length for EUC CS2 length=%d\n",
			euc_info->euc_bytelen2);
    }
    switch (euc_info->euc_bytelen3) {
    case 0:	euc_cs3_min = 0x0;	  break;
    case 1:	euc_cs3_min = 0x20000020; break;
    case 2:	euc_cs3_min = 0x20001020; break;
    case 3:	euc_cs3_min = 0x20081020; break;
    default:	fprintf(stderr, "unsupport length for EUC CS3 length=%d\n",
			euc_info->euc_bytelen3);
    }

    /*
     * EUC CS2 base value
     */
    euc_info->cs2_base = 0x100;		/* assumed */
    if (euc_cs2_max == 0)		/* EUC CS2 not used */
	euc_cs2_min = 0;

    /*
     * EUC CS3 base value
     */
    if (euc_cs2_max == 0)		/* EUC CS2 not used */
	euc_info->cs3_base = euc_info->cs2_base;
    else				/* EUC CS2 used */
	euc_info->cs3_base = euc_cs2_max - euc_cs2_min + euc_info->cs2_base + 1;
    if (euc_cs3_max == 0)		/* EUC CS3 not used */
	euc_cs3_min = 0;

    /*
     * EUC CS1 base value
     */
    if ((euc_cs2_max == 0) &&
	(euc_cs3_max == 0) &&
	(mb_cur_max == 1))
	euc_info->cs1_base = 0x00a0;	/* single byte codeset */
    else if (euc_cs3_max == 0)		/* EUC CS3 not used */
	euc_info->cs1_base = euc_info->cs3_base;
    else				/* EUC CS3 used */
	euc_info->cs1_base = euc_cs3_max - euc_cs3_min + euc_info->cs3_base + 1;

    if (euc_cs1_max == 0)		/* EUC CS1 not used */
	euc_cs1_min = 0;

    /*
     * now compute the adjustments
     */
    if (euc_cs1_max == 0)
	euc_info->cs1_adjustment = 0;
    else
	euc_info->cs1_adjustment = -(euc_cs1_min) + euc_info->cs1_base;
    if (euc_cs2_max == 0)
	euc_info->cs2_adjustment = 0;
    else
	euc_info->cs2_adjustment = -(euc_cs2_min) + euc_info->cs2_base;
    if (euc_cs3_max == 0)
	euc_info->cs3_adjustment = 0;
    else
	euc_info->cs3_adjustment = -(euc_cs3_min) + euc_info->cs3_base;

/*
printf("cs1_min = 0x%x\n", euc_cs1_min);
printf("cs1_max = 0x%x\n", euc_cs1_max);
printf("cs2_min = 0x%x\n", euc_cs2_min);
printf("cs2_max = 0x%x\n", euc_cs2_max);
printf("cs3_min = 0x%x\n", euc_cs3_min);
printf("cs3_max = 0x%x\n", euc_cs3_max);
printf("cs1_base = 0x%x\n", euc_info->cs1_base);
printf("cs2_base = 0x%x\n", euc_info->cs2_base);
printf("cs3_base = 0x%x\n", euc_info->cs3_base);
printf("cs1_adjustment = 0x%x %d\n", euc_info->cs1_adjustment, euc_info->cs1_adjustment);
printf("cs2_adjustment = 0x%x %d\n", euc_info->cs2_adjustment,euc_info->cs2_adjustment);
printf("cs3_adjustment = 0x%x %d\n", euc_info->cs3_adjustment,euc_info->cs3_adjustment);
printf("dense_end = 0x%x %d\n", euc_info->dense_end, euc_info->dense_end);
*/
}
