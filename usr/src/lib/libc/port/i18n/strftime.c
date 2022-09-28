/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)strftime.c 1.21	96/08/16  SMI"

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
#if !defined(lint) && !defined(_NOIDENT)
static char rcsid[] = "@(#)$RCSfile: strftime.c,v $ $Revision: 1.13.6.6"
	" $ (OSF) $Date: 1992/11/13 22:27:45 $";
#endif
*/
/*
 * COMPONENT_NAME: (LIBCGEN) Standard C Library General Functions
 *
 * FUNCTIONS:  strftime
 *
 *
 * (C) COPYRIGHT International Business Machines Corp. 1991
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 * 1.3  com/lib/c/fmt/strftime.c, libcfmt, 9130320 7/17/91 15:22:50
 * 1.7  com/lib/c/fmt/__strftime_std.c, libcfmt, 9140320 9/26/91 13:59:49
 */
/*
 * The following #ifdefs are used:
 *
 *  __OSF_SPECIFIERS - flag for OSF private conversion specifiers
 *	If defined, OSF private conversion specifiers are compiled in.
 *	This is turned off by default for SunOS.
 *
 *  __OSF_PREC    - flag for OSF private precision and width functionality
 *	If defined, OSF precision and width formatting is compiled in.
 *	This is turned off by default for SunOS.
 */
#include "synonyms.h"
#include <sys/localedef.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <langinfo.h>
#include <stdlib.h>

#define	IS_C_LOCALE(hdl)	((hdl) == &__C_time_object)

#define	BADFORMAT  do { format = fbad; \
			bufp = "%"; } while (0)
#define	BUFSIZE		1000
#define	PUT(c)		((size + 1) < maxsize ? (*strp++  = (c), size++) : \
				toolong++)
#define	GETSTR(f)	do { \
				t = f; \
				while (*subera && *subera != ':') \
					*t++ = *subera++; \
				*t = '\0'; \
			} while (0)
#define	STRTONUM(str, num)	do { num = 0; 		 \
				while (isdigit (*str)) { \
					num *= 10; 	 \
					num += *str++ - '0'; \
				} } while (0)

#define	CHECKFMT(f, s) (((f) && *(f))? (f) : (s))    /* f is non-null string */

#define	ERASTR		era
#define	GETNUM		getnum
#define	GETTIMEZONE	gettimezone
#define	CONV_TIME	conv_time

#ifdef __OSF_SPECIFIERS
#define	DOFORMAT	doformat
#else
#define	DOFORMAT(a, b, c, d, e, f)	doformat(a, b, c, d, e)
#endif  /* __OSF_SPECIFIERS */

#ifdef __OSF_SPECIFIERS
/* codes used by doformat() */
#define	NOYEAR	2
#define	NOSECS	3
#define	SKIP	for (strp = lastf; (i = *format) && i != '%'; format++)
#define	GETNUMBER(v) (sprintf(locbuffer, "%d", v), locbuffer)
#endif	/* __OSF_SPECIFIERS */

#ifdef __OSF_PREC
#define	WIDTH(a)	(wpflag ? 0  : (a))
#else
#define	WIDTH(a)	(a)
#endif	/* __OSF_PREC */

struct era_struct {
	char	dir;		/* direction of the current era */
	int	offset;		/* offset of the current era */
	char	st_date[100];   /* start date of the current era */
	char	end_date[100];  /* end date of the current era */
	char	name[100];	/* name of the current era */
	char	form[100];	/* format string of the current era */
};
typedef struct era_struct *era_ptr;

extern int	__xpg4;
extern _LC_time_t	__C_time_object;

/*
 * FUNCTION: getnum()
 *	     This function convert a integral numeric value i into
 *	     character string with a fixed field.
 *
 * PARAMETERS:
 *	     int i - an integral value.
 *	     int n - output field width.
 *	     char *altnum - pointer to list of alternate digits, or
 *				empty string if none exists.
 *	     char *buffer - address of user-supplied buffer.
 *
 * RETURN VALUE DESCRIPTIONS:
 *	     It returns the character string of the integral value.
 */
static char *
getnum(int i, int n, char *altnum, char *buffer)
{
	char *s = buffer;

#ifdef __OSF_PREC
	s += (n ? n : 19);
#else
	s += n;
#endif

	*s = 0;
	if (altnum && (*altnum != '\0')) {
		char	*p;	/* Points to front of i-th string */
		char	*q;	/* Points to terminator of i-th string */

		p = altnum;
		/*
		 * Search thru semicolon-separated strings for i-th alternate
		 * value
		 */
		while (i) {
			q = strchr(p, ';');	/* Possible terminator */
			if (!q) {		/* Ran off end of string? */
				p = q = "";
				break;
			}
			p = q+1;
			i--;
		}

		q = strchr(p, ';');
		if (!q)
			q = p + strlen(p);
		while (q > p)
			*--s = *--q;

	} else {
		while (s > buffer) {
#ifdef __OSF_PREC
			if (i == 0 && n == 0) break;
#endif
			*--s = (i % 10) + '0';
			i /= 10;
		}
	}
	return (s);
}


/*
 * FUNCTION: gettimezone()
 *	     This function returns the name of the current time zone.
 *
 * PARAMETERS:
 *	     struct tm *timeptr - time structure.
 *
 * RETURN VALUE DESCRIPTIONS:
 *	     It returns the current time zone.
 */
static char *
gettimezone(struct tm *timeptr)
{
	if (daylight && timeptr->tm_isdst)
		return (tzname[1]);
	return (tzname[0]);
}

/*
 *  This function returns the next era description segment
 *  in a semi-colon separated string of concatenated era
 *  description segments.
 */
static char *
get_era_segment(char *era_string)
{
	char *ptr;

	if (ptr = strchr(era_string, ';')) {
		return (++ptr);
	} else
		return (NULL);
}

#define	ADVANCE_ERASTRING(p) (p = strchr(p, ':') + 1)

/*
 *  This function extracts the fields of an era description
 *  segment.
 */
static int
extract_era_info(era_ptr era, char *era_str)
{
	char *endptr;
	int len;

	era->dir = era_str[0];
	if (era->dir != '-' && era->dir != '+')
		return (-1);
	ADVANCE_ERASTRING(era_str);
	era->offset = atoi(era_str);
	ADVANCE_ERASTRING(era_str);
	if ((endptr = strchr(era_str, ':')) == NULL)
		return (-2);
	len = endptr - era_str;
	strncpy(era->st_date, era_str, len);
	*(era->st_date + len) = '\0';
	ADVANCE_ERASTRING(era_str);
	if ((endptr = strchr(era_str, ':')) == NULL)
		return (-3);
	len = endptr - era_str;
	strncpy(era->end_date, era_str, len);
	*(era->end_date + len) = '\0';
	ADVANCE_ERASTRING(era_str);
	if ((endptr = strchr(era_str, ':')) == NULL)
		return (-4);
	len = endptr - era_str;
	strncpy(era->name, era_str, len);
	*(era->name + len) = '\0';
	ADVANCE_ERASTRING(era_str);
	if ((endptr = strchr(era_str, ';')) == NULL) {
		if ((endptr = era_str + strlen(era_str)) <= era_str)
			return (-5);
	}
	len = endptr - era_str;
	strncpy(era->form, era_str, len);
	*(era->form + len) = '\0';
	return (0);
}

/*
 * FUNCTION: conv_time()
 *	     This function converts the current Christian year into year
 *	     of the appropriate era. The era chosen such that the current
 *	     Chirstian year should fall between the start and end date of
 *	     the first matched era in the hdl->era string. All the era
 *	     associated information of a matched era will be stored in the era
 *	     structure and the era year will be stored in the year
 *	     variable.
 *
 * PARAMETERS:
 *	   _LC_TIME_t *hdl - the handle of the pointer to the LC_TIME
 *			     catagory of the specific locale.
 *	   struct tm *timeptr - date to be printed
 *	     era_ptr era - pointer to the current era.
 *	     int *year - year of the current era.
 *
 * RETURN VALUE DESCRIPTIONS:
 *	   - returns 1 if the current Christian year fall into an
 *	       valid era of the locale.
 *	   - returns 0 if not.
 */
static int
conv_time(_LC_time_t *hdl, struct tm *tm, era_ptr era, int *year)
{
	char *era_s;
	char *str;
	int start_year = 0;
	int start_month = 0;
	int start_day = 0;
	int end_year = 0;
	int end_month = 0;
	int end_day = 0;
	int cur_year = 0;
	int cur_month = 0;
	int cur_day = 0;
	int no_limit = 0;
	int found = 0;
				/*
				 * extra = 1 when current date is less than
				 * the start date, otherwise 0. This is the
				 * adjustment for correct counting up to the
				 * month and day of the start date
				 */
	int extra = 0;
	cur_year = tm->tm_year + 1900;
	cur_month = tm->tm_mon + 1;
	cur_day = tm->tm_mday;

	era_s = *hdl->era;
	for (; era_s != NULL; era_s = get_era_segment(era_s)) {

	    if (extract_era_info(era, era_s) != 0)
		continue;		/* Bad era string, try again */

	    str = era->st_date;
	    if (*str == '-') {
		str++;
		STRTONUM(str, start_year);
		start_year = -start_year;
	    } else
		STRTONUM(str, start_year);

	    str++;			/* skip the slash */
	    STRTONUM(str, start_month);
	    str++;			/* skip the slash */
	    STRTONUM(str, start_day);

	    str = era->end_date;
	    if ((*str == '+' && *(str+1) == '*') ||
			(*str == '-' && *(str+1) == '*'))
		no_limit = 1;
	    else {
		no_limit = 0;
		if (*str == '-') {
		    str++;
		    STRTONUM(str, end_year);
		    end_year = -end_year;
		} else
		    STRTONUM(str, end_year);
		str++;		/* skip the slash */
		STRTONUM(str, end_month);
		str++;		/* skip the slash */
		STRTONUM(str, end_day);
	    }
	    if (no_limit && cur_year >= start_year) {
		found = 1;
	    } else if (((cur_year > start_year) ||
			(cur_year == start_year && cur_month > start_month) ||
			(cur_year == start_year && cur_month == start_month &&
			cur_day >= start_day)) &&
			((cur_year < end_year) ||
			(cur_year == end_year && cur_month < end_month) ||
			(cur_year == end_year && cur_month == end_month &&
			cur_day <= end_day))) {
		found = 1;
	    } else
		continue;

	    if ((cur_month < start_month) ||
		(cur_month == start_month && cur_day < start_day))
		extra = 1;
	    if (era->dir == '+')
		*year = cur_year - start_year + era->offset - extra;
	    else
		*year = end_year - cur_year - extra;

	    if (found)
		return (1);
	}
	return (0);		/* No match for era times */
}


/*
 * FUNCTION: This function performs the actual formatting and it may
 *	     be called recursively with different values of code.
 *
 * PARAMETERS:
 *	   _LC_TIME_t *hdl - the handle of the pointer to the LC_TIME
 *			     category of the specific locale.
 *	   char *s - location of returned string
 *	   size_t maxsize - maximum length of output string
 *	   char *format - format that date is to be printed out
 *	   struct tm *timeptr - date to be printed
 *	     int code - this special attribute controls the outupt of
 *			certain field (eg: twelve hour form, without
 *			year or second for time and date format).
 *
 * RETURN VALUE DESCRIPTIONS:
 *	   - returns the number of bytes that comprise the return string
 *	     excluding the terminating null character.
 *	   - returns 0 if s is longer than maxsize
 */
static size_t
#ifdef __OSF_SPECIFIERS
doformat(_LC_time_t *hdl, char *s, size_t maxsize, char *format,
	struct tm *timeptr, int code)
#else
doformat(_LC_time_t *hdl, char *s, size_t maxsize, char *format,
	struct tm *timeptr)
#endif  /* __OSF_SPECIFIERS */
{
	int i;
	int firstday;		/* first day of the year */
	int toolong = 0;
	int weekno;
	char *strp;		/* pointer into output buffer str */
	char locbuffer[BUFSIZE]; /* local temporary buffer */
	int year;		/* %o value, year in current era */
	char *fbad;		/* points to where format start to be invalid */
	char *p;		/* temp pointer */
	int altera = 0;		/* Recursive call should reset 'altera' */
	struct era_struct eras;	/* the structure for current era */
	era_ptr era = &eras;	/* pointer to the current era */
	char *altnum = NULL;	/* Points to alternate numeric representation */
	int size;		/* counter of number of chars printed */
	static const char *xpg4_d_t_fmt = "%a %b %e %H:%M:%S %Y";
#ifdef __OSF_PREC
	int width;		/* width in current format or 0 */
	int prec;		/* precision in current format or 0 */
	int wpflag;		/* true if width or precision specified */
	char fill_char;		/* filling char which may be blank or zero */
#endif /* __OSF_PREC */
#ifdef __OSF_SPECIFIERS
	static int era_name = 0; /* logical flag for detected era name */
	static int era_yr = 0;	/* logical flag for detected era year */
	char *lastf;		/* last byte of str made by a format */
	char *f;		/* era format of subera */
#endif /* __OSF_SPECIFIERS */

#ifdef __OSF_SPECIFIERS
	lastf = s;
#endif
	strp = s;
	size = 0;
	while (i = *format++) {
		if (i != '%')
			PUT(i);
		else {
			char *bufp = "";    /* This should get set in loop */
			fbad = format;
#ifdef __OSF_PREC
			wpflag = width = prec = 0;
			fill_char = ' ';	/* blank is default fill char */

			/*
			 * get field width & precision
			 */
			if (*format == '0')		/* Zero-fill instead */
				fill_char = '0';

			width = strtol((char *)format, &p, 10);
			if (p != format) {
				format = p;
				wpflag++;
			}

			if (*format == '.') {
				prec = strtoul((char *) ++format, &p, 10);
				if (p != format) {
					format = p;
					wpflag++;
				}
			}
#endif /* __OSF_PREC */

			switch (*format) {
			case 'O':
				format++;
				if (!hdl->alt_digits ||
					!strchr("deHImMSuUVwWy", *format))
					BADFORMAT;
				else
					altnum = hdl->alt_digits;
				break;

			case 'E':
				format++;
				if (!hdl->era || !strchr("cCxXyY", *format))
					BADFORMAT;
				else
					altera = 1;
				break;
			}

			switch (*format++) {
			case '%':
				bufp = "%";	/* X/Open - percent sign */
				break;

			case 'n':	/* X/Open - newline character */
				bufp = "\n";
				break;

			case 't':	/* X/Open - tab character */
				bufp = "\t";
				break;

			case 'm':	/* X/Open - month in decimal number */
				bufp = GETNUM(timeptr->tm_mon + 1, WIDTH(2),
						altnum, locbuffer);
				altnum = NULL;
				break;

			case 'd': 	/* X/Open - day of month in decimal */
				bufp = GETNUM(timeptr->tm_mday, WIDTH(2),
						altnum, locbuffer);
				altnum = NULL;
				break;

			case 'e':	/* day of month with leading space */
				bufp = GETNUM(timeptr->tm_mday, WIDTH(2),
						altnum, locbuffer);
				if ((!altnum ||
					(altnum && (*altnum == '\0'))) &&
					(*bufp == '0'))
					*bufp = ' ';
				altnum = NULL;
				break;

			case 'y':	/* X/Open - year w/o century 00-99 */
#ifdef __OSF_SPECIFIERS
				if (code == NOYEAR)
					SKIP;
				else
#endif /* __OSF_SPECIFIERS */
				if (altera) {
					if (CONV_TIME(hdl, timeptr, ERASTR,
							&year)) {
						bufp = GETNUM(year, WIDTH(4),
							NULL, locbuffer);
						while (*bufp == '0')
							bufp++;
						altera = 0;
					} else {
						/*
						 * if era_year or era->form
						 * is not specified, %Ey will
						 * display %y output.
						 */
						bufp = GETNUM(timeptr->tm_year,
							WIDTH(2), altnum,
							locbuffer);
						altnum = NULL;
					}
				} else {
					bufp = GETNUM(timeptr->tm_year,
						WIDTH(2), altnum, locbuffer);
					altnum = NULL;
				}
				break;

			case 'H':	/* X/Open - hour (0-23) in decimal */
				bufp = GETNUM(timeptr->tm_hour, WIDTH(2),
						altnum, locbuffer);
				altnum = NULL;
				break;

			case 'M':	/* X/Open - minute in decimal */
				bufp = GETNUM(timeptr->tm_min, WIDTH(2),
						altnum, locbuffer);
				altnum = NULL;
				break;

			case 'S':	/* X/Open - second in decimal */
#ifdef __OSF_SPECIFIERS
				if (code == NOSECS)
					SKIP;
				else
#endif /* __OSF_SPECIFIERS */
				{
					bufp = GETNUM(timeptr->tm_sec,
						WIDTH(2), altnum, locbuffer);
					altnum = NULL;
				}
				break;

			case 'j': 	/* X/Open - day of year in decimal */
				bufp = GETNUM(timeptr->tm_yday + 1, WIDTH(3),
						altnum, locbuffer);
				altnum = NULL;
				break;

			case 'w': 	/* X/Open - weekday in decimal */
				bufp = GETNUM(timeptr->tm_wday, WIDTH(1),
						altnum, locbuffer);
				altnum = NULL;
				break;

			case 'r': 	/* X/Open - time in AM/PM notation */
				DOFORMAT(hdl, locbuffer, BUFSIZE,
				    CHECKFMT(hdl->t_fmt_ampm, "%I:%M:%S %p"),
				    timeptr, 0);
				bufp = locbuffer;
				break;

			case 'R':	/* X/Open - time as %H:%M */
				DOFORMAT(hdl, locbuffer, BUFSIZE, "%H:%M",
					timeptr, 0);
				bufp = locbuffer;
				break;

			case 'T': 	/* X/Open - time in %H:%M:%S notation */
				DOFORMAT(hdl, locbuffer, BUFSIZE,
					"%H:%M:%S", timeptr, 0);
				bufp = locbuffer;
				break;

			case 'X': 	/* X/Open - the locale time notation */
				if (altera && hdl->core.hdr.size >
					offsetof(_LC_time_t, era_t_fmt))
					/*
					 * locale object is recent enough to
					 * have era_t_fmt field.
					 */
					p = CHECKFMT(hdl->era_t_fmt,
						hdl->t_fmt);
				else
					p = hdl->t_fmt;
				altera = 0;

				DOFORMAT(hdl, locbuffer, BUFSIZE, p,
					timeptr, 0);

				bufp = locbuffer;
				break;

			case 'a': 	/* X/Open - locale's abv weekday name */
				bufp = strcpy(locbuffer,
						hdl->abday[timeptr->tm_wday]);
				break;

			case 'h':	/* X/Open - locale's abv month name */

			case 'b':
				bufp = strcpy(locbuffer,
						hdl->abmon[timeptr->tm_mon]);
				break;

			case 'p': 	/* X/Open - locale's equivalent AM/PM */
				if (timeptr->tm_hour < 12)
					strcpy(locbuffer, hdl->am_pm[0]);
				else
					strcpy(locbuffer, hdl->am_pm[1]);
				bufp = locbuffer;
				break;

			case 'Y':	/* X/Open - year w/century in decimal */
#ifdef __OSF_SPECIFIERS
				if (code == NOYEAR)
					SKIP;
				else
#endif /* __OSF_SPECIFIERS */
				if (altera) { /* POSIX.2 %EY full altrnate yr */
					if (CONV_TIME(hdl, timeptr, ERASTR,
							&year)) {
					    DOFORMAT(hdl, locbuffer, BUFSIZE,
						ERASTR->form, timeptr, 0);
					    bufp = locbuffer;
					} else {
						/*
						 * if era_year or era->form
						 * is not specified, %EY will
						 * display %Y output.
						 */
						bufp = GETNUM(timeptr->tm_year
							+ 1900, WIDTH(4),
							altnum, locbuffer);
						altnum = NULL;
					}
					altera = 0;
				} else {
					bufp = GETNUM(timeptr->tm_year + 1900,
							WIDTH(4), altnum,
							locbuffer);
					altnum = NULL;
				}
				break;

#ifdef __OSF_SPECIFIERS
			case 'z':	/* IBM - timezone name if it exists */
#endif /* __OSF_SPECIFIERS */

			case 'Z':	/* X/Open - timezone name if exists */
				bufp = GETTIMEZONE(timeptr);
				break;

			case 'A': 	/* X/Open -locale's full weekday name */
				bufp = strcpy(locbuffer,
						hdl->day[timeptr->tm_wday]);
				break;

			case 'B':	/* X/Open - locale's full month name */
				bufp = strcpy(locbuffer,
						hdl->mon[timeptr->tm_mon]);
				break;

			case 'I': 	/* X/Open - hour (1-12) in decimal */
				i = timeptr->tm_hour;
				bufp = GETNUM(i > 12 ? i - 12 : i ? i : 12,
						WIDTH(2), altnum, locbuffer);
				altnum = NULL;
				break;

			case 'k': 	/* SunOS - hour (0-23) precede blank */
				bufp = GETNUM(timeptr->tm_hour, WIDTH(2),
						altnum, locbuffer);
				if (!altnum && *bufp == '0')
					*bufp = ' ';
				altnum = NULL;
				break;

			case 'l': 	/* SunOS - hour (1-12) precede blank */
				i = timeptr->tm_hour;
				bufp = GETNUM(i > 12 ? i - 12 : i ? i : 12,
						WIDTH(2), altnum, locbuffer);
				if (!altnum && *bufp == '0')
					*bufp = ' ';
				altnum = NULL;
				break;

			case 'D': 	/* X/Open - date in %m/%d/%y format */
				DOFORMAT(hdl, locbuffer, BUFSIZE,
					"%m/%d/%y", timeptr, 0);
				bufp = locbuffer;
				break;

			case 'x': 	/* X/Open - locale's date */
				if (altera)
				    p = CHECKFMT(hdl->era_d_fmt, hdl->d_fmt);
				else
				    p = hdl->d_fmt;

				altera = 0;
				DOFORMAT(hdl, locbuffer, BUFSIZE,
					p, timeptr, 0);
				bufp = locbuffer;
				break;

			case 'c': 	/* X/Open - locale's date and time */
				if (altera) {
					/*
					 * %Ec and era_d_t_fmt field exists
					 */
					p = CHECKFMT(hdl->era_d_t_fmt,
						hdl->d_t_fmt);
					altera = 0;
				} else {
					if (__xpg4 != 0) { /* XPG4 mode */
						if (IS_C_LOCALE(hdl)) {
							p = (char *)
								xpg4_d_t_fmt;
						} else {
							p = hdl->d_t_fmt;
						}
					} else {	/* Solaris mode */
						p = hdl->d_t_fmt;
					}
				}

				DOFORMAT(hdl, locbuffer, BUFSIZE, p,
						timeptr, 0);
				bufp = locbuffer;
				break;

			case 'u':
				/*
				 * X/Open - week day as a number [1-7]
				 * (Monday as 1)
				 */
				bufp = GETNUM(timeptr->tm_wday + 1, WIDTH(1),
						altnum, locbuffer);
				altnum = NULL;
				break;

			case 'U':
				/*
				 * X/Open - week number of the year (0-53)
				 * (Sunday is the first day of week 1)
				 */
				weekno = (timeptr->tm_yday + 7 -
						timeptr->tm_wday) / 7;
				bufp = GETNUM(weekno, WIDTH(2), altnum,
						locbuffer);
				altnum = NULL;
				break;

			case 'V':
				/*
				 * X/Open - week number of the year, (Mon=1)
				 * as [01,53]
				 */

				/*
				 * Week number of year, taking Monday as
				 * the first day of the week.
				 * If the week containing January 1 has
				 * four or more days in the new year, then it
				 * is considered week 1; otherwise it is
				 * considered week 53 of the previous year
				 * and the next week is week 1.
				 */

				/*
				 * i = the day number of the first Monday in
				 * the year, in the range 0-6.  The "+ 8"
				 * (rather than "+ 1") guarantees that the
				 * left operand of '%' is positive.
				 */
				i = (timeptr->tm_yday - timeptr->tm_wday + 8)
						% 7;

				/*
				 * If i <= 3, the first Monday of the current
				 * year begins the first week of the current
				 * year, because the current year contains
				 * fewer than 4 days of the previous week.
				 * In that case, days for which tm_yday < i
				 * are included in the last week of the
				 * previous year, or **week 53** of the
				 * previous year, and only days for which
				 * tm_yday >=i are included in weeks of the
				 * current year.
				 *
				 * If i > 3, the first Monday of the current
				 * year begins the second week of the current
				 * year, because the current year contains
				 * 4 or more days of the week that began on
				 * the Monday before. In that case, days for
				 * which tm_yday < i are included in week 1
				 * of the current year, and days for which
				 * tm_yday >= i are included in weeks 2
				 * and later.
				 */

				if (i <= 3) {
				/*
				 * First Monday of the year begins week 1 of
				 * the year.
				 */
					if (timeptr->tm_yday < i)
					/*
					 * tm_yday is in the last week of
					 * previous year.
					 */
						weekno = 53;
					else
					/*
					 * tm_yday is in some week of the
					 * current year, which could be week 1.
					 */
						weekno = (timeptr->tm_yday -
							i + 7) / 7;
				} else
				/*
				 * First Monday of the year begins week 2 of
				 * the year, and tm_yday is in week 1 or later.
				 */
					weekno = (timeptr->tm_yday - i + 7) /
						7 + 1;

				bufp = GETNUM(weekno, WIDTH(2),
						altnum, locbuffer);
				altnum = NULL;
				break;


			case 'W':
				/*
				 * X/Open - week number of the year (0-53)
				 * (Monday is the first day of week 1)
				 */
				firstday =
				    (timeptr->tm_wday + 6) % 7;	/* Prev day */
				weekno = (timeptr->tm_yday + 7 - firstday) / 7;

				bufp = GETNUM(weekno, WIDTH(2),
						altnum, locbuffer);
				altnum = NULL;
				break;

			case 'C':
				if ((altera) && (CONV_TIME(hdl, timeptr,
						ERASTR, &year) == 1)) {
					bufp = ERASTR->name;
				} else if (__xpg4 == 0) { /* Solaris mode */
					p = hdl->date_fmt;
					DOFORMAT(hdl, locbuffer, BUFSIZE, p,
						timeptr, 0);
					bufp = locbuffer;
				} else {	/* XPG4 mode */
					bufp = GETNUM(((timeptr->tm_year+1900)
						/100), WIDTH(2), altnum,
						locbuffer);
				}
				altera = 0;
				break;

#ifdef __OSF_SPECIFIERS
			case 'l':
				/*
				 * IBM-long day name, long month name,
				 * locale date representation
				 */
				switch (*format++) {
				case 'a':
					bufp = strcpy(locbuffer,
						hdl->day[timeptr->tm_wday]);
					break;

				case 'h':
					bufp = strcpy(locbuffer,
						hdl->mon[timeptr->tm_mon]);
					break;

				case 'D':
					DOFORMAT(hdl, locbuffer, BUFSIZE,
						"%b %d %Y", timeptr, 0);
					bufp = locbuffer;
					break;
				default :
					BADFORMAT;
				}
				break;

			case 's':
				/*
				 * IBM-hour(12 hour clock), long date
				 * w/o year, long time w/o secs
				 */
				switch (*format++) {
				case 'H':
					i = timeptr->tm_hour;
					bufp = GETNUM(i > 12 ? i - 12 : i,
						WIDTH(2), altnum, locbuffer);
					break;

				case 'D':
					DOFORMAT(hdl, locbuffer, BUFSIZE,
						"%b %d %Y", timeptr, NOYEAR);
						bufp = locbuffer;
					break;

				case 'T':
					DOFORMAT(hdl, locbuffer, BUFSIZE,
						hdl->t_fmt, timeptr, NOSECS);
					bufp = locbuffer;
					break;
				default :
					BADFORMAT;
				}
				break;

			/*
			 * This is the additional code to support non-Christian
			 * eras. The formatter %Jy will display the relative
			 * year from the relevant era entry in NLYEAR, %Js will
			 * display the era name.
			 */
			case 'J': 	/* IBM - era and year of the Emperor */
				switch (*format++) {
				case 'y':
					if (hdl->era == NULL) {
						BADFORMAT;
					} else if (CONV_TIME(hdl, timeptr,
							ERASTR, &year)) {
						bufp = GETNUMBER(year);
						era_name = 1;
					} else {
						BADFORMAT;
					}
					break;

				case 's':
					if (! *hdl->era) {
						BADFORMAT;
					} else if (era_yr) {
						bufp = ERASTR->name;
						era_yr = 0;
						era_name = 0;
					} else if (CONV_TIME(hdl, timeptr,
							ERASTR, &year)) {
						bufp = ERASTR->name;
						era_name = 1;
					} else {
						BADFORMAT;
					}
					break;
				default:
					BADFORMAT;
					break;
				}
				break;

			case 'N':	/* locale's era name */
				if (! *hdl->era) {
					BADFORMAT;
				} else if (era_yr) {
					bufp = ERASTR->name;
					era_yr = 0;
					era_name = 0;
				} else if (CONV_TIME(hdl, timeptr, ERASTR,
						&year)) {
					bufp = ERASTR->name;
					era_name = 1;
				} else {
					BADFORMAT;
					}
				break;

			case 'o':			/* era year */
				if (hdl->era == NULL) {
					BADFORMAT;
				} else if (era_name) {
					bufp = GETNUMBER(year);
					era_yr = 0;
					era_name = 0;
				} else if (CONV_TIME(hdl, timeptr, ERASTR,
						&year)) {
					bufp = GETNUMBER(year);
					era_name = 1;
				} else {
					BADFORMAT;
				}
				break;
#endif /* __OSF_SPECIFIERS */

			default:			 /* badformat */
				BADFORMAT;
				break;
			} /* switch */

#ifdef __OSF_PREC
			/* output bufp with appropriate padding */

			i = strlen(bufp);

			if (prec && prec < i) {	 /* truncate on right */
				*(bufp + prec) = '\0';
				i = prec;
			}
			if (width > 0)
				while (!toolong && i++ < width)
					PUT(fill_char);
#endif /* __OSF_PREC */

			while (!toolong && *bufp)
				PUT(*bufp++);

#ifdef __OSF_PREC
			if (width < 0)
				while (!toolong && i++ < -width)
					PUT(fill_char);
#endif /* __OSF_PREC */

#ifdef __OSF_SPECIFIERS
			lastf = strp;
#endif /* __OSF_SPECIFIERS */

		} 	/* i == '%' */

		if (toolong)
			break;
	}
	*strp = 0;
	if (toolong)
		return (0);
	return (strp - s);
}


/*
 * FUNCTION: strfmon_std()
 *	     This is the standard method to format the date and ouput to
 *	     the output buffer s. The values returned are affected by
 *	     the setting of the locale category LC_TIME and the time
 *	     information in the tm time structure.
 *
 * PARAMETERS:
 *	     _LC_TIME_t *hdl - the handle of the pointer to the LC_TIME
 *			       catagory of the specific locale.
 *	   char *s - location of returned string
 *	   size_t maxsize - maximum length of output string
 *	   char *format - format that date is to be printed out
 *	   struct tm *timeptr - date to be printed
 *
 * RETURN VALUE DESCRIPTIONS:
 *	   - returns the number of bytes that comprise the return string
 *	       excluding the terminating null character.
 *	   - returns 0 if s is longer than maxsize
 */
size_t
__strftime_std(_LC_time_t *hdl, char *s, size_t maxsize, const char *format,
	const struct tm *timeptr)
{
	return (DOFORMAT(hdl, s, maxsize, (char *)format,
			(struct tm *)timeptr, 0));
}


/*
 * FUNCTION: strftime() is a method driven function where the time formatting
 *           processes are done the method points by __lc_time->core.strftime.
 *           It formats the date and output to the output buffer s. The values
 *           returned are affected by the setting of the locale category
 *           LC_TIME and the time information in the tm time structure.
 *
 * PARAMETERS:
 *           char *s - location of returned string
 *           size_t maxsize - maximum length of output string
 *           char *format - format that date is to be printed out
 *           struct tm *timeptr - date to be printed
 *
 * RETURN VALUE DESCRIPTIONS:
 *           - returns the number of bytes that comprise the return string
 *             excluding the terminating null character.
 *           - returns 0 if s is longer than maxsize
 */

size_t
strftime(char *s, size_t maxsize, const char *format,
		const struct tm *timeptr)
{
	if (format == NULL)
	    format = "%c";	/* SVVS 4.0 */

	tzset();		/* POSIX says every time... */

	return (METHOD(__lc_time, strftime)(__lc_time, s, maxsize,
					format, timeptr));
}
