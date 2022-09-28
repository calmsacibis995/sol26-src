/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)doscan.c	1.46	96/07/03 SMI"

/*LINTLIBRARY*/
#include "synonyms.h"
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <values.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <thread.h>
#include <synch.h>
#include <mtlib.h>
#include "stdiom.h"
#include <stdlib.h>
#include <limits.h>
#include <wchar.h>

extern int _scrwidth(wchar_t c);

/*
#if defined(i386)
 * __xgetRD() replaces the role of "fp_direction".
#elif defined(sparc) || defined(__ppc)
 * _QgetRD() replaces the role of "fp_direction".
#else
 * someone needs to get the right function named!
#endif
*/
/* ****************************************************** */
/* DO NOT use the global variable "fp_direction" since */
/* it has been removed from the face of earth. */
/* ****************************************************** */

#if defined(i386)
extern enum fp_direction_type __xgetRD();
#elif defined(sparc) || defined(__ppc)
extern enum fp_direction_type _QgetRD();
#else
#error Unknown architecture!
#endif

#define	NCHARS	(1 << BITSPERBYTE)

/* if the _IOWRT flag is set, this must be a call from sscanf */
#define	locgetc(cnt)	(cnt += 1, (iop->_flag & _IOWRT) ? \
				((*iop->_ptr == '\0') ? EOF : *iop->_ptr++) : \
				GETC(iop))
#define	locungetc(cnt, x) (cnt -= 1, (x == EOF) ? EOF : \
				((iop->_flag & _IOWRT) ? *(--iop->_ptr) : \
				    (++iop->_cnt, *(--iop->_ptr))))

#define	wlocgetc()	((iop->_flag & _IOWRT) ? \
				((*iop->_ptr == '\0') ? EOF : *iop->_ptr++) : \
				GETC(iop))
#define	wlocungetc(x) ((x == EOF) ? EOF : \
				((iop->_flag & _IOWRT) ? *(--iop->_ptr) : \
				    UNGETC(x, iop)))

extern int read();

static int 		number();
static int 		readchar();
static unsigned char	*setup();
static int 		string();
static int		wstring();
static int		_mkarglst();
static int		_bi_getwc();
static int		_bi_ungetwc();

#define	MAXARGS	30	/* max. number of args for fast positional paramters */

/*
 * stva_list is used to subvert C's restriction that a variable with an
 * array type can not appear on the left hand side of an assignment operator.
 * By putting the array inside a structure, the functionality of assigning to
 * the whole array through a simple assignment is achieved..
 */
typedef struct stva_list {
	va_list	ap;
} stva_list;

int
_doscan(iop, fmt, va_Alist)
	register FILE *iop;
	register unsigned char *fmt;
	va_list va_Alist;
{
	int ret = 0;
	int __doscan_u();
#ifdef _REENTRANT
	rmutex_t *lk;
#endif _REENTRANT

	if (iop->_flag & _IOWRT)
		ret = __doscan_u(iop, fmt, va_Alist);
	else {
		FLOCKFILE(lk, iop);
		ret = __doscan_u(iop, fmt, va_Alist);
		FUNLOCKFILE(lk);
	}
	return (ret);
}


int
__doscan_u(iop, fmt, va_Alist)
	register FILE *iop;
	register unsigned char *fmt;
	va_list va_Alist;
{
	register int	ch;
	int		nmatch = 0, len, inchar, stow, size;
	int		chcount, flag_eof;
	char	tab[NCHARS];

	/* variables for postional parameters */
	unsigned char	*sformat = fmt;	/* save the beginning of the format */
	int		fpos = 1;	/* 1 if first postional parameter */
	stva_list	args,	/* used to step through the argument list */
		sargs;	/* used to save the start of the argument list */
	stva_list	arglst[MAXARGS];
					/*
					 * array giving the appropriate values
					 * for va_arg() to retrieve the
					 * corresponding argument:
					 * arglst[0] is the first argument
					 * arglst[1] is the second argument,etc.
					 */

	/* Check if readable stream */
	if (!(iop->_flag & (_IOREAD | _IORW))) {
		errno = EBADF;
		return (EOF);
	}

	/*
	 * Initialize args and sargs to the start of the argument list.
	 * We don't know any portable way to copy an arbitrary C object
	 * so we use a system-specific routine(probably a macro) from
	 * stdarg.h.  (Remember that if va_list is an array, in_args will
	 * be a pointer and &in_args won't be what we would want for
	 * memcpy.)
	 */
	va_copy(args.ap, va_Alist);
	sargs = args;

	chcount = 0; flag_eof = 0;

	/*
	 ******************************************************
	 * Main loop: reads format to determine a pattern,
	 *		and then goes to read input stream
	 *		in attempt to match the pattern.
	 ******************************************************
	 */
	for (; ; )
	{
		if ((ch = *fmt++) == '\0') {
			return (nmatch); /* end of format */
		}
		if (isspace(ch)) {
			if (!flag_eof) {
				while (isspace(inchar = locgetc(chcount)))
					;
				if (locungetc(chcount, inchar) == EOF)
					flag_eof = 1;
			}
			continue;
		}
		if (ch != '%' || (ch = *fmt++) == '%') {
			if ((inchar = locgetc(chcount)) == ch)
				continue;
			if (locungetc(chcount, inchar) != EOF) {
				return (nmatch); /* failed to match input */
			}
			break;
		}
	charswitch:
		if (ch == '*') {
			stow = 0;
			ch = *fmt++;
		} else
			stow = 1;

		for (len = 0; isdigit(ch); ch = *fmt++)
			len = len * 10 + ch - '0';
		if (ch == '$') {
			/*
			 * positional parameter handling - the number
			 * specified in len gives the argument to which
			 * the next conversion should be applied.
			 * WARNING: This implementation of positional
			 * parameters assumes that the sizes of all pointer
			 * types are the same. (Code similar to that
			 * in the portable doprnt.c should be used if this
			 * assumption does not hold for a particular
			 * port.)
			 */
			if (fpos) {
				if (_mkarglst(sformat, sargs, arglst) != 0) {
					return (EOF);
				} else {
					fpos = 0;
				}
			}
			if (len <= MAXARGS) {
				args = arglst[len - 1];
			} else {
				args = arglst[MAXARGS - 1];
				for (len -= MAXARGS; len > 0; len--)
					(void) va_arg(args.ap, void *);
			}
			len = 0;
			ch = *fmt++;
			goto charswitch;
		}

		if (len == 0)
			len = MAXINT;
		if ((size = ch) == 'l' || (size == 'h') || (size == 'L') ||
		    (size == 'w'))
			ch = *fmt++;
		if (size == 'l' && ch == 'l') {
			size = 'm';		/* size = 'm' if long long */
			ch = *fmt++;
		}
		if (ch == '\0') {
			return (EOF);		/* unexpected end of format */
		}
		if (ch == '[') {
			int	t = 0;
			int	b, c, d;

			if (*fmt == '^') {
				t++;
				fmt++;
			}
			(void) memset(tab, !t, NCHARS);
			if ((c = *fmt) == ']' || c == '-') {
				tab[c] = t;
				fmt++;
			}

			while ((c = *fmt) != ']') {
				if (c == '\0') {
					return (EOF);
				}
				b = *(fmt - 1);
				d = *(fmt + 1);
				if ((c == '-') && (d != ']') && (b < d)) {
					(void) memset(&tab[b], t, d - b + 1);
					fmt += 2;
				} else {
					tab[c] = t;
					fmt++;
				}
			}
			fmt++;
		}
		if (isupper(ch)) { /* no longer documented */
			if (_lib_version == c_issue_4) {
				if (size != 'm' && size != 'L')
					size = 'l';
			}
			ch = _tolower(ch);
		}
		if (ch != 'n' && !flag_eof) {
			if (ch != 'c' && ch != 'C' && ch != '[') {
				while (isspace(inchar = locgetc(chcount)))
					;
				if (locungetc(chcount, inchar) == EOF)
					break;
			}
		}
		switch (ch) {
		case 'C': /* xpg xsh4 extention */
		case 'S': /* xpg xsh4 extention */
		case 'c':
		case 's':
			if ((size == 'w') || (size == 'C') || (size == 'S')) {
				size = wstring(&chcount, &flag_eof, stow,
					ch, len, iop, &args.ap);
			} else {
				size = string(&chcount, &flag_eof, stow,
					ch, len, tab, iop, &args.ap);
			}
			break;
		case '[':
			size = string(&chcount, &flag_eof, stow,
				ch, len, tab, iop, &args.ap);
			break;
		case 'n':
			if (stow == 0)
				continue;
			if (size == 'h')
				*va_arg(args.ap, short *) = (short) chcount;
			else if (size == 'l')
				*va_arg(args.ap, long *) = (long) chcount;
#if ! defined(_NO_LONGLONG)
			else if (size == 'm') /* long long */
				*va_arg(args.ap, long long *) =
					(long long) chcount;
#endif /* ! defined(_NO_LONGLONG) */
			else
				*va_arg(args.ap, int *) = (int) chcount;
			continue;
		case 'i':
		default:
			size = number(&chcount, &flag_eof, stow, ch,
				len, size, iop, &args.ap);
			break;
		}
		if (size)
			nmatch += stow;
		else {
			return ((flag_eof && !nmatch) ? EOF : nmatch);
		}
		continue;
	}
	return (nmatch != 0 ? nmatch : EOF); /* end of input */
}

/* ****************************************************************** */
/* Functions to read the input stream in an attempt to match incoming */
/* data to the current pattern from the main loop of _doscan(). */
/* ****************************************************************** */
static int
number(chcount, flag_eof, stow, type, len, size, iop, listp)
	int		*chcount, *flag_eof;
	int		stow, type, len, size;
	FILE	*iop;
	va_list			*listp;
{
	char	numbuf[64];
	register char	*np = numbuf;
	register int	c, base, inchar, lookahead;
	int		digitseen = 0, floater = 0, negflg = 0;
#if ! defined(_NO_LONGLONG)
	long long	lcval = 0LL;
#else /* defined(_NO_LONGLONG) */
	/* long */ long	lcval = 0L;
#endif /* defined(_NO_LONGLONG) */

	switch (type) {
	case 'e':
	case 'f':
	case 'g':
		floater++;
		/* FALLTHROUGH */
	case 'd':
	case 'u':
	case 'i':
		base = 10;
		break;
	case 'o':
		base = 8;
		break;
	case 'x':
	case 'p':
		base = 16;
		break;
	default:
		return (0); /* unrecognized conversion character */
	}

	if (floater != 0) {
		/*
		 * Handle floating point with
		 * file_to_decimal.
		 */
		decimal_mode    dm;
		decimal_record  dr;
		fp_exception_field_type efs;
		enum decimal_string_form form;
		char	*echar;
		int		nread, ic;
		char	buffer[1024];
		char	*nb = buffer;

		/* locungetc((*chcount),c); */
		if (len > 1024)
			len = 1024;
		file_to_decimal(&nb, len, 0, &dr, &form, &echar, iop, &nread);
		if (stow && (form != invalid_form)) {
#if defined(i386)
			dm.rd = __xgetRD();
#elif defined(sparc) || defined(__ppc)
			dm.rd = _QgetRD();
#else
#error Unknown architecture!
#endif
			if (size == 'l') {		/* double */
				decimal_to_double((double *)
					va_arg(*listp, double *),
					&dm, &dr, &efs);
			} else if (size == 'L') {	/* quad */
#if defined(i386)
				decimal_to_extended((extended *)
					va_arg(*listp, quadruple *),
					&dm, &dr, &efs);
#elif defined(sparc) || defined(__ppc)
				decimal_to_quadruple((quadruple *)
					va_arg(*listp, double *),
					&dm, &dr, &efs);
#else
#error Unknown architecture!
#endif /* ! defined(i386) */
			} else {	/* single */
				decimal_to_single((float *)
					va_arg(*listp, float *),
					&dm, &dr, &efs);
			}
			if ((efs & (1 << fp_overflow)) != 0) {
				errno = ERANGE;
			}
			if ((efs & (1 << fp_underflow)) != 0) {
				errno = ERANGE;
			}
		}
		(*chcount) += nread;	/* Count characters read. */
		c = *nb;		/* Get first unused character. */
		ic = c;
		if (c == NULL) {
			ic = locgetc((*chcount));
			c = ic;
			/*
			 * If null, first unused may have been put back
			 * already.
			 */
		}
		if (ic == EOF) {
			(*chcount)--;
			*flag_eof = 1;
		} else if (locungetc((*chcount), c) == EOF)
			*flag_eof = 1;
		return ((form == invalid_form) ? 0 : 1);
				/* successful match if non-zero */
	}

	switch (c = locgetc((*chcount))) {
	case '-':
		negflg++;
		/* FALLTHROUGH */
	case '+':
		if (--len <= 0)
			break;
		if ((c = locgetc((*chcount))) != '0')
			break;
		/* FALLTHROUGH */
	case '0':
		/*
		 * If %i or %x, the characters 0x or 0X may optionally precede
		 * the sequence of letters and digits (base 16).
		 */
		if ((type != 'i' && type != 'x') || (len <= 1))
			break;
		if (((inchar = locgetc((*chcount))) == 'x') ||
			(inchar == 'X')) {
			lookahead = readchar(iop, chcount);
			if (isxdigit(lookahead)) {
				base = 16;

				if (len <= 2) {
					locungetc((*chcount), lookahead);
					/* Take into account the 'x' */
					len -= 1;
				} else {
					c = lookahead;
					/* Take into account '0x' */
					len -= 2;
				}
			} else {
				locungetc((*chcount), lookahead);
				locungetc((*chcount), inchar);
			}
		} else {
			/* inchar wans't 'x'. */
			locungetc((*chcount), inchar); /* Put it back. */
			if (type == 'i') /* Only %i accepts an octal. */
				base = 8;
		}
	}
	for (; --len  >= 0; *np++ = (char)c, c = locgetc((*chcount))) {
		if (np > numbuf + 62) {
		    errno = ERANGE;
		    return (0);
		}
		if (isdigit(c) || base == 16 && isxdigit(c)) {
			int digit = c - (isdigit(c) ? '0' :
				isupper(c) ? 'A' - 10 : 'a' - 10);
			if (digit >= base)
				break;
			if (stow)
				lcval = base * lcval + digit;
			digitseen++;


			continue;
		}
		break;
	}


	if (stow && digitseen) {
		/* suppress possible overflow on 2's-comp negation */
#if ! defined(_NO_LONGLONG)
		if (negflg && lcval != (1LL << 63))
#else /* defined(_NO_LONGLONG) */
		if (negflg && lcval != (1L << 31))
#endif /* defined(_NO_LONGLONG) */
			lcval = -lcval;
#if ! defined(_NO_LONGLONG)
		if (size == 'm')
			*va_arg(*listp, long long *) = lcval;
		else
#endif /* ! defined(_NO_LONGLONG) */
			/* else */  if (size == 'l')
				*va_arg(*listp, long *) = (long)lcval;
			else if (size == 'h')
				*va_arg(*listp, short *) = (short)lcval;
			else
				*va_arg(*listp, int *) = (int)lcval;
	}
	if (locungetc((*chcount), c) == EOF)
	    *flag_eof = 1;
	return (digitseen); /* successful match if non-zero */
}

/* Get a character. If not using sscanf and at the buffer's end */
/* then do a direct read(). Characters read via readchar() */
/* can be  pushed back on the input stream by locungetc((*chcount),) */
/* since there is padding allocated at the end of the stream buffer. */
static int
readchar(iop, chcount)
	FILE	*iop;
	int	*chcount;
{
	int	inchar;

	if ((iop->_flag & _IOWRT) || (iop->_cnt != 0))
		inchar = locgetc((*chcount));
	else {
		if (read(FILENO(iop), &inchar, 1) != 1)
			return (EOF);
		(*chcount) += 1;
	}
	return (inchar);
}

static int
string(chcount, flag_eof, stow, type, len, tab, iop, listp)
	int		*chcount, *flag_eof;
	register int	stow, type, len;
	char	*tab;
	register FILE	*iop;
	va_list		*listp;
{
	register int	ch;
	register char	*ptr;
	char	*start;

	start = ptr = stow ? va_arg(*listp, char *) : NULL;
	if (((type == 'c') || (type == 'C')) && len == MAXINT)
		len = 1;
	while ((ch = locgetc((*chcount))) != EOF &&
		!(((type == 's') || (type == 'S')) &&
			isspace(ch) || type == '[' && tab[ch])) {
		if (stow)
			*ptr = (char)ch;
		ptr++;
		if (--len <= 0)
			break;
	}
	if (ch == EOF) {
		(*flag_eof) = 1;
		(*chcount) -= 1;
	} else if (len > 0 && locungetc((*chcount), ch) == EOF)
		(*flag_eof) = 1;
	if (ptr == start)
		return (0);	/* no match */
	if (stow && ((type != 'c') && (type != 'C')))
		*ptr = '\0';
	return (1);	/* successful match */
}

/* This function initializes arglst, to contain the appropriate */
/* va_list values for the first MAXARGS arguments. */
/* WARNING: this code assumes that the sizes of all pointer types */
/* are the same. (Code similar to that in the portable doprnt.c */
/* should be used if this assumption is not true for a */
/* particular port.) */
static int
_mkarglst(fmt, args, arglst)
	char	*fmt;
	stva_list args;
	stva_list arglst[];
{
	int maxnum, n, curargno;

	maxnum = -1;
	curargno = 0;
	while ((fmt = strchr(fmt, '%')) != 0) {
		fmt++;	/* skip % */
		if (*fmt == '*' || *fmt == '%')
			continue;
		if (fmt[n = strspn(fmt, "01234567890")] == '$') {
			/* convert to zero base */
			curargno = atoi(fmt) - 1;
			fmt += n + 1;
		}
		if (maxnum < curargno)
			maxnum = curargno;
		curargno++;	/* default to next in list */

		fmt += strspn(fmt, "# +-.0123456789hL$");
		if (*fmt == '[') {
			int		i;
			fmt++; /* has to be at least on item in scan list */
			if (*fmt == ']') {
				fmt++;
			}
			while (*fmt != ']') {
				if (*fmt == '\0') {
					return (-1); /* bad format */
				} else if (isascii(*fmt)) {
					fmt++;
				} else {
					i = mblen((const char *)
						fmt, MB_CUR_MAX);
					if (i <= 0) {
						return (-1);
					} else {
						fmt += i;
					}
				}
			}
		}
	}
	if (maxnum > MAXARGS)
		maxnum = MAXARGS;
	for (n = 0; n <= maxnum; n++)
	{
		arglst[n] = args;
		(void) va_arg(args.ap, void *);
	}
	return (0);
}


/*
 * For wide character handling
 */

static int
wstring(chcount, flag_eof, stow, type, len, iop, listp)
	int		*chcount, *flag_eof;
	register int	stow, type, len;
	register FILE	*iop;
	va_list			*listp;
{
	register int	wch;
	register wchar_t	*ptr;
	wchar_t	*wstart;

	wstart = ptr = stow ? va_arg(*listp, wchar_t *) : NULL;

	if ((type == 'c') && len == MAXINT)
		len = 1;
	while (((wch = _bi_getwc(iop)) != EOF) &&
		!(type == 's' && (isascii(wch) ? isspace(wch) : 0))) {
		(*chcount) += _scrwidth((wchar_t)wch);
		if (stow)
			*ptr = wch;
		ptr++;
		if (--len <= 0)
			break;
	}
	if (wch == EOF) {
		(*flag_eof) = 1;
		(*chcount) -= 1;
	} else {
		if (len > 0 && _bi_ungetwc(wch, iop) == EOF)
			(*flag_eof) = 1;
	}
	if (ptr == wstart)
		return (0); /* no match */
	if (stow && (type != 'c'))
		*ptr = '\0';
	return (1); /* successful match */
}


/*
 * Locally define getwc and ungetwc
 */
static int
_bi_getwc(FILE *iop)
{
	register int c;
	wchar_t intcode;
	int i, nbytes, cur_max;
	char buff[MB_LEN_MAX];

	if ((c = wlocgetc()) == EOF)
		return (WEOF);

	if (isascii(c))	/* ASCII code */
		return ((wint_t) c);

	buff[0] = (char) c;

	cur_max = (int) MB_CUR_MAX;
	/* MB_CUR_MAX doen't exeed the value of MB_LEN_MAX */
	/* So we use MB_CUR_MAX instead of MB_LEN_MAX for */
	/* improving the performance. */
	for (i = 1; i < cur_max; i++) {
		c = wlocgetc();
		if (c == '\n') {
			wlocungetc(c);
			break;
		}
		if (c == EOF) {
			/* this still may be a valid multibyte character */
			break;
		}
		buff[i] = (char) c;
	}

	if ((nbytes = mbtowc(&intcode, buff, i)) == -1) {
		/*
		 * If mbtowc fails, the input was not a legal character.
		 *	ungetc all but one character.
		 *
		 * Note:  the number of pushback characters that
		 *	ungetc() can handle must be >= (MB_LEN_MAX - 1).
		 *	In Solaris 2.x, the number of pushback
		 *	characters is 4.
		 */
		while (i-- > 1) {
			wlocungetc((signed char)buff[i]);
			/* cast for ppc */
		}
		errno = EILSEQ;
		return (WEOF); /* Illegal EUC sequence. */
	}

	while (i-- > nbytes) {
		/*
		 * Note:  the number of pushback characters that
		 *	ungetc() can handle must be >= (MB_LEN_MAX - 1).
		 *	In Solaris 2.x, the number of pushback
		 *	characters is 4.
		 */
		wlocungetc((signed char)buff[i]);
		/* cast for ppc */
	}
	return ((int) intcode);
}

static int
_bi_ungetwc(wint_t wc, FILE *iop)
{
	char mbs[MB_LEN_MAX];
	register unsigned char *p;
	register int n;

	if (wc == WEOF)
		return (WEOF);

	if ((iop->_flag & _IOREAD) == 0 || iop->_ptr <= iop->_base) {
		if (iop->_base != NULL && iop->_ptr == iop->_base &&
		    iop->_cnt == 0)
			++iop->_ptr;
		else
			return (WEOF);
	}

	n = wctomb(mbs, (wchar_t)wc);
	if (n <= 0)
		return (WEOF);
	p = (unsigned char *)(mbs+n-1); /* p points the last byte */
	/* if _IOWRT is set to iop->_flag, it means this is */
	/* an invocation from sscanf(), and in that time we */
	/* don't touch iop->_cnt.  Otherwise, which means an */
	/* invocation from fscanf() or scanf(), we touch iop->_cnt */
	if ((iop->_flag & _IOWRT) == 0) {
		/* scanf() and fscanf() */
		iop->_cnt += n;
		while (n--) {
			*--iop->_ptr = *(p--);
		}
	} else {
		/* sscanf() */
		iop->_ptr -= n;
	}
	return (wc);
}

