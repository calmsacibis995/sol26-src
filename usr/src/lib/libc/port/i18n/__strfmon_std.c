/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)__strfmon_std.c 1.14	96/07/02  SMI"

/*
 * COMPONENT_NAME: LIBCFMT
 *
 * FUNCTIONS:  __strfmon_std
 *
 * ORIGINS: 27
 *
 * IBM CONFIDENTIAL -- (IBM Confidential Restricted when
 * combined with the aggregated modules for this product)
 * OBJECT CODE ONLY SOURCE MATERIALS
 * (C) COPYRIGHT International Business Machines Corp. 1991, 1992
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 */

#include <sys/localedef.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <values.h>
#include <nan.h>

#define	max(a, b)	(a < b ? b : a)
#define	min(a, b)	(a > b ? b : a)
#define	PUT(c)	(strp < strend ? *strp++  = (c) : toolong++)
#define	MAXFSIG	17 	/* Maximum significant figures in floating-point num */
#define	SIZE	1000	/* Size of a working buffer */
#define	SIGN_BUF_SZ	20 /* size of working buffer for +/- sign */

extern char *fcvt();	/* libc routines for floating conversion */

struct global_flags {
int w_width;		/* minimum width for the current format */
int n_width;		/* width for the #n specifier */
int prec;		/* precision of current format */
int leftflag;	/* logical flag for left justification */
int wflag;		/* logical flag for width */
int nflag;		/* logical flag for #n specifier */
int no_groupflag;	/* logical flag for grouping */
int signflag;	/* logical flag for the + specifier */
int parenflag;	/* logical flag for C parenthesis specifier */
int no_curflag;	/* logical flag for currency symbol supression */
int byte_left;	/* number of byte left in the output buffer */
int mb_length;	/* length of the multi-byte char of current locale */
char *cur_symbol;	/* local or interantional currency symbol */
char fill_char[MB_LEN_MAX+1]; /* the fill character buffer */
};

static int	do_format(_LC_monetary_t *, char *, size_t,
	double, struct global_flags *);
static void	bidi_output(char *, char **, int);
static void	out_cur_sign(_LC_monetary_t *, char **, char **,
	int, struct global_flags *);
static void do_out_cur_sign(char, char, char, char *, char **,
	char **, int, struct global_flags *);
static int	digits_to_left(double);

/*
 * FUNCTION: This is the standard method for function strfmon().
 *	     It formats a list of double values and output them to the
 *	     output buffer s. The values returned are affected by the format
 *	     string and the setting of the locale category LC_MONETARY.
 *
 * PARAMETERS:
 *           _LC_monetary_objhdl_t hdl - the handle of the pointer to
 *			the LC_MONETARY catagory of the specific locale.
 *           char *s - location of returned string
 *           size_t maxsize - maximum length of output including the null
 *			      termination character.
 *           char *format - format that montary value is to be printed out
 *
 * RETURN VALUE DESCRIPTIONS:
 *           - returns the number of bytes that comprise the return string
 *             excluding the terminating null character.
 *           - returns 0 if s is longer than maxsize or error detected.
 */


ssize_t
__strfmon_std(_LC_monetary_t *hdl, char *s, size_t maxsize,
				const char *format, va_list ap)
{
	char *strp;		/* pointer to output buffer s */
	const char *fbad;	/* points to where format start to be invalid */
	char *strend;		/* last availabel byte in output buffer s */
	char ch;		/* the current character in the format string */
	int toolong = 0;	/* logical flag for valid output length */
	int i;			/* a working variable */
	int pflag;		/* logical flag for precision */
	struct global_flags  flags_g;

	pflag = 0;
	flags_g.prec = 0;
	flags_g.wflag = 0;
	flags_g.nflag = 0;
	flags_g.mb_length = 1;
	flags_g.byte_left = maxsize;

	if (!s || !hdl || !format)
		return (-1);
	strp = s;
	strend = s + maxsize - 1;
	while ((ch = *format++) && !toolong) {
		if (ch != '%') {
			PUT(ch);
			flags_g.byte_left--;
		} else {
			fbad = format;
			pflag = 0;
			flags_g.fill_char[0] = ' ';
			flags_g.fill_char[1] = '\0';
			flags_g.prec = 0;
			flags_g.w_width = 0;
			flags_g.n_width = 0;
			flags_g.leftflag = 0;
			flags_g.wflag = 0;
			flags_g.nflag = 0;
			flags_g.no_groupflag = 0;
			flags_g.signflag = 0;
			flags_g.parenflag = 0;
			flags_g.no_curflag = 0;
			/* ------- scan for flags ------- */
			i = 0;
			while (1) {
				switch (*format) {
				case '=': case '^': case '~': case '!':
				case '+': case '(': case '-':
					break;
				default:
					i = 1;	/* there are no more */
						/* optional flags    */
				}
				if (i) break;	/* exit loop */
				if (*format == '=') {	/* =f fill character */
					format++;
					if ((flags_g.mb_length = mblen(format,
							MB_CUR_MAX)) != -1) {
					for (i = flags_g.mb_length; i > 0; i--)
					flags_g.fill_char[flags_g.mb_length-i] =
							*format++;
					flags_g.fill_char[flags_g.mb_length] =
							'\0';
					} else
						return (-1); /* invalid char */
				}
				if (*format == '^' || *format == '~') {
						/* X/open or AIX format */
						/* no grouping for thousands */
					format++;
					flags_g.no_groupflag++;
				}
				if (*format == '+') {
						/* locale's +/- sign used */
					format++;
					flags_g.signflag = 1;
					flags_g.parenflag = 0;
				} else if (*format == '(') {
						/* locale's paren. for neg. */
					format++;
					flags_g.parenflag = 1;
					flags_g.signflag = 0;
				}
				if (*format == '!') {
						/* suppress currency symbol */
					format++;
					flags_g.no_curflag++;
				}
				if (*format == '-') {	/* - left justify */
					format++;
					flags_g.leftflag++;
				}
			} /* end of while(1) loop */
			/* -------- end scan flags -------- */
			while (isdigit(*format)) {	/* w width field */
				flags_g.w_width *= 10;
				flags_g.w_width += *format++ - '0';
				flags_g.wflag++;
			}
			if (*format == '#') {
				/* #n digits preceeds decimal(left precision) */
				flags_g.nflag++;
				format++;
				while (isdigit(*format)) {
				flags_g.n_width *= 10;
				flags_g.n_width += *format++ - '0';
				}
			}
			if (*format == '.') {
					/* .p precision (right precision) */
				pflag++;
				format++;
				while (isdigit(*format)) {
					flags_g.prec *= 10;
					flags_g.prec += *format++ - '0';
				}
			}
			switch (*format++) {
			case '%':
				PUT('%');
				flags_g.byte_left--;
				break;

			case 'i':	/* international currency format */
			case 'a':	/* international currency format */
				flags_g.cur_symbol = hdl->int_curr_symbol;
				if (!pflag &&
				    (flags_g.prec = hdl->int_frac_digits) < 0)
					flags_g.prec = 2;
				if ((i = do_format(hdl, strp, maxsize,
							va_arg(ap, double),
							&flags_g)) == -1)
					return (-1);
				else {
					strp += i;
					flags_g.byte_left -= i;
				}
				break;

			case 'n':	/* local currency format */
			case 'c':	/* local currency format */
				flags_g.cur_symbol = hdl->currency_symbol;
				if (!pflag &&
				    (flags_g.prec = hdl->frac_digits) < 0)
					flags_g.prec = 2;
				if ((i = do_format(hdl, strp, maxsize,
					va_arg(ap, double), &flags_g)) == -1)
					return (-1);
				else {
					strp += i;
					flags_g.byte_left -= i;
				}
				break;
			default:
				format = fbad;
				PUT('%');
				flags_g.byte_left--;
				break;
			}
		} /* else */
	} /* while */
	if (toolong) {
		errno = E2BIG;
		return (-1);
	} else
		return (strp - s);
}

/*
 * FUNCTION: do_format()
 *	     This function performs all the necessary formating for directives
 *	     %i (or %a) and %n (or %c). The output will be written to the output
 *	     buffer str and the number of formatted bytes are returned.
 *
 * PARAMETERS:
 *           _LC_monetary_objhdl_t hdl - the handle of the pointer to
 *		the LC_MONETARY catagory of the specific locale.
 *           char *str - location of returned string
 *           size_t maxsize - maximum length of output including the null
 *                            termination character.
 *           char *dval - the double value to be formatted.
 *
 * RETURN VALUE DESCRIPTIONS:
 *           - returns the number of bytes that comprise the return string
 *             excluding the terminating null character.
 *           - returns 0 if s is longer than maxsize or any error.
 */


static int
do_format(_LC_monetary_t * hdl, char *str, size_t maxsize,
			double dval, struct global_flags *flags_g)
{
	char *s;		/* work buffer for string handling */
	char *number;		/* work buffer for converted string of fcvt() */
	int dig_to_left;	/* number of digits to the left of decimal in */
				/* the actual value of dval */
	int fcvt_prec;		/* number of digits to the right of decimal */
				/* for conversion of fcvt */
	int decpt;		/* a decimal number to show where the radix */
				/* character is when counting from beginning */
	int sign;		/* 1 for negative, 0 for positive */
	char *b;		/* points to the beginning of the output */
	char *e;		/* points to the end of the output */
	char lead_zero[2*MAXFSIG];	/* leading zeros if necessary */
	char *t;		/* the grouping string of the locale */
	char *separator;	/* thousand separator string of the locale */
	char *radix;		/* locale's radix character */
	int i, j, k;		/* working variable as index */
	int gap;		/* number of filling character needed */
	int sep_width;		/* the width of the thousand separator */
	int radix_width;	/* the width of the radix character */
	int sv_i;
	char temp_buff[SIZE];	/* a work buffer for the whole formatted str */
	char *temp;		/* pointer to temp_buff */
	char *temp_m = NULL;	/* pointer to temp buffer created by malloc */
	char *left_parenthesis = "(";
	char *right_parenthesis = ")";

	/*
	* First, convert the double value into a character string by
	* using fcvt(). To determine the precision used by fcvt():
	*
	* if no digits to left of decimal,
	* 	then all requested digits will be emitted to right of decimal.
	*	hence, use max of(max-sig-digits, user's-requested-precision).
	*
	* else if max-sig-digits <= digits-to-left
	* 	then all digits will be emitted to left of decimal point.
	*  	Want to use zero or negative prec value to insure that rounding
	* 	will occur rather than truncation.
	*
	* else
	*	digits can be emitted both to left and right of decimal, but
	*	only potential for rounding/truncation is to right of decimal.
	*	Hence, want to use user's request precision if it will not
	*	cause truncation, else use largest prec that will round
	*	correctly when max. number of digits is emitted.
	*/
	if (flags_g->prec > 20)
		flags_g->prec = 2; /* Compatible to Solaris. */

	if (2*maxsize < SIZE)
		temp = temp_buff;
	else if (temp_m = (char *) malloc(2*maxsize))
		temp = temp_m;
	else
		return (-1);
	/*
	* malloc failed call routine to obtain
	* the number of digits to the left of the decimal point
	*
	* __ld() returns the number of digits to the
	* left of the decimal place.  This value can
	* be 0 or negative for numbers less than 1.0.
	* i.e.:
	* 10.00 = > 2
	*  1.00 = > 1
	*  0.10 = > 0
	*  0.01 = > -1
	*/
	dig_to_left = digits_to_left(dval);
					/* get the num of digit to the left */
	if (dig_to_left <= 0)		/* determine the precision to be used */
		fcvt_prec = min(flags_g->prec, MAXFSIG);
	else if (dig_to_left >= MAXFSIG)
		fcvt_prec = MAXFSIG - dig_to_left;
	else
		fcvt_prec = min(flags_g->prec, MAXFSIG - dig_to_left);
	number = fcvt(dval, fcvt_prec, &decpt, &sign);

	/*
	 * Fixing the position of the radix character(if any). Output the
	 * number using the radix as the reference point. When ouptut grows to
	 * the right, decimal digits are processed and appropriate precision(if
	 * any) is used. When output grows to the left, grouping of digits
	 * (if needed), thousands separator (if any), and filling character
	 * (if any) will be used.
	 *
	 * Begin by processing the decimal digits.
	 */

	b = temp + maxsize;
	if (flags_g->prec) {
		if (*(radix = hdl->mon_decimal_point)) {
				/* set radix character position */
			radix_width = strlen(radix);
			for (i = 1; i <= radix_width; i++)
				*(b+i) = *radix++;
			e = b + radix_width + 1;
		} else {  		/* radix character not defined */
			*(b+1) = '.';	/* default decimal char */
			e = b + 1 + 1;
			}
		s = number + decpt; 	/* copy the digits after the decimal */
		while (*s)
			*e++ = *s++;
	} else		/* zero precision, no radix character needed */
		e = b + 1;

	/*
	* Output the digits preceeding the radix character. Leading zeros
	* (if dig_to_left < #n and filling char is zero) are padded in front
	* of number.
	*/
	if ((i = j = (flags_g->n_width - dig_to_left)) > 0) {
						/* pad with lead_zero */
		s = lead_zero;
		for (; i > 0; i--)
			*s++ = '0';
		*s = '\0';
		number = strcat(lead_zero, number);
		s = number + j + decpt -1;
	} else
		s = number + decpt - 1;	/* points to the digit preceed radix */

	if (flags_g->no_groupflag) 	/* no grouping is needed */
		for (i = 0; i < dig_to_left; i++)
			*b-- = *s--;
	else {
		if (*(t = hdl->mon_grouping)) {
			/* get grouping format,eg: "^C^B\0" == "3;2" */
			j = dig_to_left;
			separator = hdl->mon_thousands_sep;
			sep_width = strlen(separator);
			while (j > 0) {
				if (*t) {
					i = *t++; 	/* get group size */
					sv_i = i;
				}
				else
					i = sv_i;
					/* otherwise, repeat 'n reuse old i */
				if (i > j || i == -1)
					i = j;		/* no more grouping */
				for (; i > 0; i--, j--)
					*b-- = *s--;	/* copy group */
				for (i = sep_width-1; j && *separator &&
								i >= 0; i--)
					*b-- = *(separator + i);
			} /* while */
			/*
			 * Note that in "%#<x>n", <x> is the number of digits,
			 * not spaces, so after the digits are in place, the
			 * rest of the field must be the same size as if the
			 * number took up the whole field.  This means
			 * adding fill chars where there would have been a
			 * separator.  e.g.: "%#10n"
			 *	$9,987,654,321.00
			 *	$@@@@@@@54,321.00	correct
			 *	$@@@@@54,321.00		incorrect
			 * So, n_width should be n_width (i.e. digits) +
			 * number of separators to be inserted.
			 * Solution:  just increment n_width for every
			 * separator that would have been inserted.  In this
			 * case, 3.  Also, follow mon_grouping rules about
			 * repeating and the -1.
			 */
			t = hdl->mon_grouping;
			k = flags_g->n_width;
			if (*t) {
				if ((i = *t++) == -1)
					i = k;
				while (k - i > 0) {
					k -= i;
					flags_g->n_width++;
					if (*t && (i = *t++) == -1)
						break;
					}
				}
		} else /* the grouping format is not defined in this locale */
			for (i = 0; i < dig_to_left; i++)
				*b-- = *s--;
	} /* else */

	/*
	* Determine if padding is needed.
	* If the number of digit prceeding the radix character is greater
	* than #n(if any), #n is ignored. Otherwise, the output is padded
	* with filling character("=f", if any) or blank is used by default.
	 */

	if (flags_g->nflag &&
			(gap = flags_g->n_width - (temp + maxsize - b)) > 0) {
		for (i = 0; i < gap; i++)
			for (j = flags_g->mb_length - 1; j >= 0; j--)
			*b-- = flags_g->fill_char[j]; /* copy fill char */
	}

	/*
	* At here, the quantity value has already been decided. What comes
	* next are the positive/negative sign, monetary symbol, parenthesis.
	* The signflag, parenflag, and no_curflag
	* will be considered first to determine the sign and currency format.
	* If none of them are defined, the locale's defaults are used.
	*/

	if (flags_g->signflag) { /* use locale's +/- sign repesentation */
		if (sign)
			i = hdl->n_sign_posn;
		else
			i = hdl->p_sign_posn;
		switch (i) {
		case 0: 	/* parens enclose currency and quantity */
		case 1: 	/* sign preceed currency and quantity */
		case 2:		/* sign succeed currency and quantity */
		case 3:		/* sign preceed currency symbol */
		case 4:		/* sign succeed currency symbol */
			out_cur_sign(hdl, &b, &e, sign, flags_g);
			break;
		}
	} else if (flags_g->parenflag) {
		if (sign && left_parenthesis && right_parenthesis) {
			out_cur_sign(hdl, &b, &e, sign, flags_g);
			bidi_output(left_parenthesis, &b, 0);
			bidi_output(right_parenthesis, &e, 1);
		} else
			out_cur_sign(hdl, &b, &e, sign, flags_g);
	} else {	/* use all the attributes of the locale's default */
		if (sign)
			i = hdl->n_sign_posn;
		else
			i = hdl->p_sign_posn;
		switch (i) {
		case 0:		/* Paren. around currency and quantity */
			out_cur_sign(hdl, &b, &e, sign, flags_g);
			if (left_parenthesis && right_parenthesis) {
				bidi_output(left_parenthesis, &b, 0);
				bidi_output(right_parenthesis, &e, 1);
			}
			break;

		case 1: 	/* sign preceed currency and quantity */
		case 2:		/* sign succeed currency and quantity */
		case 3:		/* sign preceed currency symbol */
		case 4:		/* sign succedd currency symbol */
			out_cur_sign(hdl, &b, &e, sign, flags_g);
			break;

		default:
			out_cur_sign(hdl, &b, &e, sign, flags_g);
		}
	} /* else */

	/*
	* By setting e(the last byte of the buffer) to \0 and increment
	* b(the first byte of the buffer), now the temp buffer should
	* have a completely formatted null terminated string starting and
	* ending at b and e. Before the result is copied into the s buffer,
	* check if the formatted string is less than the w-field width and
	* determine its left or right justification.
	*/

	b++;
	*e = '\0';
	i = strlen(b);
	if (max(i, flags_g->w_width) > flags_g->byte_left) {
		errno = E2BIG;
		return (-1);
	}
	if (flags_g->wflag && i < flags_g->w_width) {
						/* justification is needed */
		if (flags_g->leftflag) {
			while (*b)
				*str++ = *b++;
			for (i = flags_g->w_width-i; i > 0; i--)
			*str++ = ' ';
		} else {
			for (i = flags_g->w_width-i; i > 0; i--)
				*str++ = ' ';
			while (*b)
				*str++ = *b++;
		}
		*str = '\0';
		if (temp_m)
			free(temp_m);
		return (flags_g->w_width);
	} else {
		strcpy(str, b);
		if (temp_m)
			free(temp_m);
		return (i);
	}
}

/*
 * FUNCTION: out_cur_sign()
 *	     This function ouputs the sign related symbol (if needed) and
 *	     the currency symbol (if needed) to the ouput buffer. It also
 *	     updates the beginning and ending pointers of the formatted
 *	     string. This function indeed extract the sign related information
 *	     of the current formatting value and pass them to the sign
 *	     independent formatting function do_out_cur_sign().
 *
 * PARAMETERS:
 *           _LC_monetary_objhdl_t hdl - the handle of the pointer to the
 *			LC_MONETARY catagory of the specific locale.
 *	     char **begin - The address of the pointer which points to the
 *			    begining of the output buffer.
 *	     char **end - The address of the pointer which points to the end
 *			  of the output buffer.
 *	     int sign - The sign of the current formatting monetary value.
 *
 * RETURN VALUE DESCRIPTIONS:
 *	     void.
 */

static void
out_cur_sign(_LC_monetary_t * hdl, char **begin, char **end,
				int sign, struct global_flags *flags_g)
{
	char	pn_cs_precedes;
	char	pn_sign_posn;
	char	pn_sep_by_space;
	char *pn_sign;
	char *padded, *p;
	int   maxlen;
	int   i;
	char sign_buf[SIGN_BUF_SZ];

	if (sign) {	/* negative number with sign and currency symbol */
		pn_cs_precedes = hdl->n_cs_precedes;
		pn_sign_posn = hdl->n_sign_posn;
		pn_sep_by_space = hdl->n_sep_by_space;
		pn_sign = hdl->negative_sign;
		if (strlen(pn_sign) == 0)
			pn_sign = "-";
	} else {	/* positive number with sign and currency symbol */
		pn_cs_precedes = hdl->p_cs_precedes;
		pn_sign_posn = hdl->p_sign_posn;
		pn_sep_by_space = hdl->p_sep_by_space;
		pn_sign = hdl->positive_sign;
	    }

	if (pn_cs_precedes != 1 && pn_cs_precedes != 0)
		pn_cs_precedes = 1;
	if (pn_sign_posn > 4 || pn_sign_posn == CHAR_MAX)
		pn_sign_posn = 1;
	if (pn_sep_by_space > 3 || pn_sep_by_space == CHAR_MAX)
		pn_sep_by_space = 0;

	if (flags_g->nflag) {
			/* align if left precision is used (i.e. "%#10n") */
		i = 0;
		maxlen = strlen(hdl->negative_sign);
		if (maxlen < strlen(hdl->positive_sign)) {
			maxlen = strlen(hdl->positive_sign);
			i = 1;	/* positive sign string is longer */
			}
		if (maxlen < 20)
			p = padded = sign_buf;
		else
			p = padded = (char *)malloc(maxlen * sizeof (char));

		if (i == sign)
			for (i = maxlen - strlen(pn_sign); i; i--)
				*padded++ = ' ';
		*padded = '\0';
		padded = p;
		strcat(padded, pn_sign);
		}
	else
		padded = pn_sign;

	do_out_cur_sign(pn_cs_precedes, pn_sign_posn,
		pn_sep_by_space, padded, begin, end, sign, flags_g);

	if (flags_g->nflag && maxlen >= 20)
		free(padded);

}


/*
 * FUNCTION: do_out_cur_sign()
 *	This is a common function to process positive and negative
 *	monetary values. It outputs the sign related symbol (if needed)
 *	and the currency symbol (if needed) to the output buffer.
 *
 * PARAMETERS:
 *	     pn_cs_precedes - The p_cs_precedes or n_cs_precedes value.
 *	     pn_sign_posn - The p_sign_posn or n_sign_posn value.
 *	     pn_sep_by_space - The p_sep_by_space or n_sep_by_space value.
 *	     pn_sign - The positive_sign or negative_sign value.
 *	     char **begin - The address of the pointer which points to the
 *			    begining of the output buffer.
 *	     char **end - The address of the pointer which points to the end
 *			  of the output buffer.
 *	     int sign - The sign of the current formatting monetary value.
 *
 * RETURN VALUE DESCRIPTIONS:
 *	     void.
 */

static void
do_out_cur_sign(
	char	pn_cs_precedes,
	char	pn_sign_posn,
	char	pn_sep_by_space,
	char	*pn_sign,
	char	**begin,
	char	**end,
	int	sign,
	struct global_flags	*flags_g)
{
	char *b = *begin;
	char *e = *end;
	int i;

	if (pn_cs_precedes == 1) {	/* cur_sym preceds quantity */
		switch (pn_sign_posn) {
		case 0:
		case 1:
		case 3:
			if (!flags_g->no_curflag) {
				if (pn_sep_by_space == 1)
					*b-- = ' ';
				bidi_output(flags_g->cur_symbol, &b, 0);
			}
			if (!flags_g->parenflag)
				bidi_output(pn_sign, &b, 0);
			break;
		case 2:
			if (!flags_g->no_curflag) {
				if (pn_sep_by_space == 1)
					*b-- = ' ';
				bidi_output(flags_g->cur_symbol, &b, 0);
			}
			if (!flags_g->parenflag)
				bidi_output(pn_sign, &e, 1);
			break;
		case 4:
			if (!flags_g->parenflag)
				bidi_output(pn_sign, &b, 0);
			if (!flags_g->no_curflag) {
				if (pn_sep_by_space == 1)
					*b-- = ' ';
				bidi_output(flags_g->cur_symbol, &b, 0);
			}
			break;
		}
	} else if (pn_cs_precedes == 0) { /* cur_sym after quantity */
		switch (pn_sign_posn) {
		case 0:
		case 1:
			if (!flags_g->parenflag)
				bidi_output(pn_sign, &b, 0);
			if (!flags_g->no_curflag) {
				if (pn_sep_by_space == 1)
					*e++ = ' ';
				bidi_output(flags_g->cur_symbol, &e, 1);
			}
			break;
		case 2:
		case 4:
			if (!flags_g->no_curflag) {
				if (pn_sep_by_space == 1)
					*e++ = ' ';
				bidi_output(flags_g->cur_symbol, &e, 1);
			}
			if (!flags_g->parenflag)
				bidi_output(pn_sign, &e, 1);
			break;
		case 3:
			if (!flags_g->parenflag)
				bidi_output(pn_sign, &e, 1);
			if (!flags_g->no_curflag) {
				if (pn_sep_by_space == 1)
					*e++ = ' ';
				bidi_output(flags_g->cur_symbol, &e, 1);
			}
			break;
		}
	} else {		/* currency position not defined */
		switch (pn_sign_posn) {
		case 1:
			if (!flags_g->parenflag)
				bidi_output(pn_sign, &b, 0);
			break;
		case 2:
			if (!flags_g->parenflag)
				bidi_output(pn_sign, &e, 1);
			break;
		}
	}
	*begin = b;
	*end = e;
}

/*
 * FUNCTION: bidi_output()
 *	     This function copies the infield to output buffer, outfield,
 *	     either by appending data to the end of the buffer when the
 *	     direction (dir) is 1 or by inserting data to the beginning
 *	     of the buffer when the direction (dir) is 0.
 *
 * PARAMETERS:
 *	    char *infield - The character string to be copied into the
 *			    output buffer outfield.
 *	    char *infield - The output buffer.
 *	    int dir - When dir is 1, infield is appended to the end of
 *			the output buffer.
 *			When dir is 0, infield is inserted in front of the
 *			output buffer.
 *
 * RETURN VALUE DESCRIPTIONS:
 *	    void.
 */


static void
bidi_output(char *infield, char **outfield, int dir)
{
	int field_width;
	int i;
	char *out = *outfield;

	if (!(*infield))
		return;
	field_width = strlen(infield);
	if (dir)		/* output from left to right */
		for (i = field_width; i > 0; i--)
			*out++ = *infield++;
	else {				/* output from right to left */
		infield += (field_width - 1);
		for (i = field_width; i > 0; i--)
			*out-- = *infield--;
	}
	*outfield = out;
}
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
/*
 * COMPONENT_NAME: LIBCFMT
 *
 * FUNCTIONS:  strfmon
 *
 *
 * (C) COPYRIGHT International Business Machines Corp. 1991
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 * 1.3  com/lib/c/fmt/strfmon.c, libcfmt, 9130320 7/17/91 15:21:47
 */
/*
 * FUNCTION: digits_to_left()
 *      Compute number of digits to left of decimal point.
 *      Scale number to range 10.0..1.0 counting divides or
 *      deducting multiplies.
 *      Positive value mean digits to left, negative is digits to right.
 *      If the number is very large scale using large divisors.
 *      If its intermediate do it the slow way.
 *      If its very small scale using large multipliers
 *      This replaces the totally IEEE dependent __ld() with a
 *      (mostly) independent version stolen from ecvt.c.
 *      Slight speed mods since ecvt does more work than we need.
 *
 * PARAMETERS:
 *      double dvalue   - value to work with
 *
 * RETURN VALUE DESCRIPTIONS:
 *      int             - number of digits to left of decimal point
 */
static int
digits_to_left(double dvalue)
{
	struct log {	/* scale factors */
		double  pow10;
		int	pow;
	} log[] = {	1e32,   32,
			1e16,   16,
			1e8,    8,
			1e4,    4,
			1e2,    2,
			1e1,    1 };
	register struct log	*scale = log;
	register int		digits = 1;	/* default (no scale) */

	/*
	 * check for fluff.
	 * Original expression
	 * if (IS_NAN(dvalue) || IS_INF(dvalue) || IS_ZERO(dvalue))
	 */
	if (IsNANorINF(dvalue) || dvalue == 0)
		return (0);

	/*
	 * make it positive
	 * Original expression
	 * dvalue = fabs(dvalue);
	 */
	if (dvalue < 0.0)
		dvalue *= -1;

	/* now scale it into 10.0..1.0 range */
	if (dvalue >= 2.0 * DMAXPOWTWO) {	/* big */
		do {    /* divide down */
			for (; dvalue >= scale->pow10; digits += scale->pow)
			dvalue /= scale->pow10;
		} while ((scale++)->pow > 1);

	} else if (dvalue >= 10.0) {		/* medium */
		do {    /* divide down */
			digits++;
		} while ((dvalue /= 10.0) > 10.0);
	} else if (dvalue < 1.0) {		/* small */
			do {    /* multiply up */
				for (; dvalue * scale->pow10 < 10.0;
					digits -= scale->pow)
					dvalue *= scale->pow10;
			} while ((scale++)->pow > 1);
	}
	return (digits);
}
