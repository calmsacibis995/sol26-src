/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)strptime.c 1.45	96/08/23  SMI"

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
static char rcsid[] = "@(#)$RCSfile: strptime.c,v $ $Revision: 1.4.5.4 $ (OSF)"
" $Date: 1992/11/30 16:15:30 $";
#endif
*/
/*
 * COMPONENT_NAME: LIBCFMT
 *
 * FUNCTIONS:  strptime
 *
 * (C) COPYRIGHT International Business Machines Corp. 1991
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 * 1.3  com/lib/c/fmt/strptime.c, libcfmt, 9130320 7/17/91 15:23:44
 * 1.8  com/lib/c/fmt/__strptime_std.c, libcfmt,9140320 9/26/91 14:00:15
 */

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifdef __STDC__
#pragma weak getdate = _getdate
#endif
#include "synonyms.h"
#include "shlib.h"
#include <sys/localedef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <nl_types.h>
#include <langinfo.h>
#include <thread.h>
#include <ctype.h>
#include <limits.h>

#define	MONTH		12
#define	DAY_MON		31
#define	HOUR_24		23
#define	HOUR_12		11
#define	DAY_YR		366
#define	MINUTE		59
#define	SECOND		61
#define	WEEK_YR		53
#define	DAY_WK		6
#define	YEAR_1900	1900

#define	BUF_SIZE	1000	/* the buffer size of the working buffer */

#define	SKIP_TO_NWHITE(s)	while (*s && (isspace(*s))) s++

#define	GETSTR(f)	t = f; \
			while (*subera && *subera != ':') \
				*t++ = *subera++; \
			*t = '\0'

#define	GET_NUMBER(buf, size, alt_digits, oflag) \
		get_number((char **) buf, size, alt_digits, oflag)

#define	IS_C_LOCALE(hdl)	((hdl) == &__C_time_object)

/*
 * dysize(A) -- calculates the number of days in a year.  The year must be
 *      the current year minus 1900 (i.e. 1990 - 1900 = 90, so 'A' should
 *      be 90).
 */
#define	dysize(A)	((((1900+(A)) % 4 == 0 && (1900+(A)) % 100 != 0) || \
			(1900+(A)) % 400 == 0) ? 366:365)

#ifdef __OSF_SPECIFIERS
#define	STRTONUM(str, num)	num = 0; \
				while (isdigit (*str)) { \
					num *= 10; \
					num += *str++ - '0'; \
				}
#endif

/*
 * Global data for internal routines.
 * Use structure and pass by reference to make code thread safe.
 */
typedef struct strptime_data {
	int set_yr;	/* logical flag to see if year is detected */
	int hour;	/* value of hour from %H */
	int meridian;	/* value of AM or PM */
	int era_name;	/* logical flag for detected era name */
	int era_year;	/* logical flag for detected era year */
	int week_number_u; /* contains the week of the year %U */
	int week_number_w; /* contains the week of the year %U */
	int century;	/* contains the century number */
	int calling_func; /* indicates which function called strptime_recurs */
	int wrong_input;  /* indicates wrong input */
} strptime_data_t;

/* Retain simple names.  */
#define	set_yr		(strptime_data->set_yr)
#define	hour		(strptime_data->hour)
#define	meridian	(strptime_data->meridian)
#define	era_name	(strptime_data->era_name)
#define	era_year	(strptime_data->era_year)
#define	week_number_u    (strptime_data->week_number_u)
#define	week_number_w    (strptime_data->week_number_w)
#define	century		(strptime_data->century)
#define	calling_func	(strptime_data->calling_func)
#define	wrong_input	(strptime_data->wrong_input)

enum {f_getdate, f_strptime};	/* for calling_func */
enum {FILLER, AM, PM};		/* for meridian */

struct era_struct {
	char	dir;		/* dircetion of the current era */
	int	offset;		/* offset of the current era */
	char	st_date[100];	/* start date of the current era */
	char	end_date[100];	/* end date of the current era */
	char	name[100];	/* name of the current era */
	char	form[100];	/* format string of the current era */
};
typedef struct era_struct *era_ptr;

typedef struct simple_date {
	int	day;
	int	month;
	int	year;
} simple_date;

extern int	__xpg4;
extern _LC_time_t	__C_time_object;

extern int __lyday_to_month[];
extern int __yday_to_month[];
extern int __mon_lengths[2][12];

#ifdef __OSF_SPECIFIERS
static const int day_year[] =	{0, 31, 59, 90, 120, 151, 181,
				212, 243, 273, 304, 334, 365};
#endif

static struct	tm  *calc_date(struct tm *, struct tm *);
static int read_tmpl(_LC_time_t *, char *, struct tm *, struct tm *,
	strptime_data_t *);
static void getnow(struct tm *, struct tm *);
static void init_tm(struct tm *);
static void init_str_data(strptime_data_t *, int);
static int verify_getdate(struct tm *, struct tm *, strptime_data_t *);
static int verify_strptime(struct tm *, struct tm *, strptime_data_t *);
static void Day(int, struct tm *);
static void DMY(struct tm *);
static int days(int);
static int jan1();
static int yday(struct tm *, int, struct tm *, strptime_data_t *);
static int week_number_to_yday(struct tm *, int, strptime_data_t *);
static void year(int, struct tm *);
static void MON(int, struct tm *);
static void Month(int, struct tm *);
static void DOW(int, struct tm *);
static void adddays(int, struct tm *);
static void DOY(int, struct tm *);
static int get_number(char **, int, char *, int);
static int number(char **, int);
static int search_alt_digits(char **, char *);

char *__strptime_std(_LC_time_t *, const char *, const char *, struct tm *);
struct  tm *__getdate_std(_LC_time_t *, const char *);
static unsigned char * strptime_recurse(_LC_time_t *, const unsigned char *,
	const unsigned char *, struct tm *, struct tm *, strptime_data_t *,
	int);

/*
 * FUNCTION: strptime() is a method driven functions where the time formatting
 *	     processes are done in the method points by
 *	     __lc_time->core.strptime.
 *           It parse the input buffer according to the format string. If
 *           time related data are recgonized, updates the tm time structure
 *           accordingly.
 *
 * PARAMETERS:
 *           const char *buf - the input data buffer to be parsed for any
 *                             time related information.
 *           const char *fmt - the format string which specifies the expected
 *                             format to be input from the input buf.
 *           struct tm *tm   - the time structure to be filled when appropriate
 *                             time related information is found.
 *
 * RETURN VALUE DESCRIPTIONS:
 *           - if successful, it returns the pointer to the character after
 *             the last parsed character in the input buf string.
 *           - if fail for any reason, it returns a NULL pointer.
 */
char *
strptime(const char *buf, const char *fmt, struct tm *tm)
{
	return (METHOD(__lc_time, strptime)(__lc_time, buf, fmt, tm));
}


struct  tm *
getdate(const char *expression)
{
	return (METHOD(__lc_time, getdate)(__lc_time, expression));
}


#ifdef __OSF_SPEFIERS
/*
 * FUNCTION: set_day_of_year (struct tm *tm)
 *	If the month, day, and year have been determine. It should be able
 * 	to calculate the day-of-year field tm->tm_yday of the tm structure.
 *	It calculates if its leap year by calling the dysize() which
 *	returns 366 for leap year.
 *
 * PARAMETERS:
 *           struct tm *tm - a pointer to the time structure where the
 *			     tm->tm_yday field will be set.
 *
 * RETURN VALUE DESCRIPTIONS:
 *	void.
 */
static void
set_day_of_year(struct tm *tm, strptime_data_t *strptime_data)
{

	if (set_day && set_mon && set_yr) {
		if ((dysize(tm->tm_year) == 366) && (tm->tm_mon >= 2))
			tm->tm_yday = day_year[tm->tm_mon] + tm->tm_mday;
		else
			tm->tm_yday = day_year[tm->tm_mon] + tm->tm_mday - 1;
		set_day = set_mon = set_yr = 0;
	}
}

/*
 * FUNCTION: set_month_of_year (struct tm *tm)
 *	If the day, the week of the year, and year have been determined,
 *      we should calculate the month-of-year (field tm->tm_mon)
 *      of the tm structure.  We also need to fill in the tm_yday field.
 *
 *	Calculate its leap year by calling the dysize() which
 *	returns 366 for leap year.
 *
 * PARAMETERS:
 *           struct tm *tm - a pointer to the time structure where the
 *			     tm->tm_mon field will be set.
 *
 * RETURN VALUE DESCRIPTIONS:
 *	void.
 */
static void
set_month_of_year(struct tm *tm, strptime_data_t *strptime_data,
	int monday_first_flg)
{

	int i, delta;
	struct tm JAN_1_tm;
	time_t t;
	int week_of_year;

	if (week_number_u != -1)
		week_of_year = week_number_u;
	else
		week_of_year = week_number_w;

	if (set_day && set_week && set_yr) {
		/*
		 * first calculate the day of the year if necessary.
		 * To do this, we need to know the day of the week of
		 * the first day of the year.
		 */
		JAN_1_tm.tm_sec = 0;
		JAN_1_tm.tm_min = 0;
		JAN_1_tm.tm_hour = 0;
		JAN_1_tm.tm_mday = 1;
		JAN_1_tm.tm_mon = 0;
		JAN_1_tm.tm_year = tm->tm_year;
		JAN_1_tm.tm_wday = -1;
		JAN_1_tm.tm_yday = -1;
		JAN_1_tm.tm_isdst = -1;

		t = mktime(&JAN_1_tm);

		/*
		 * get the difference between the day of the week for Jan 1
		 * and the day of the week specified.
		 */
		delta = tm->tm_wday - JAN_1_tm.tm_wday;

		tm->tm_yday = ((week_of_year)*7) + delta;

		/*
		 * if we are dealing with Monday as the first day  of
		 * the week (i.e. %W), this means that everything before
		 * the first Monday is in week 0.
		 * If the week of the day we are looking for is 0, then
		 * we need to account for the fact that it will be the last
		 * day of the week instead of the first (must add a week).
		 */
		if (monday_first_flg) {
			if (tm->tm_wday == 0)
				tm->tm_yday = ((week_of_year + 1) * 7) + delta;
		}

		tm->tm_mon = 0;

		i = 0;

		/*
		 * if we are dealing with a leap year, we may need
		 * to adjust the day of year if the day is past February
		 */
		if ((tm->tm_yday > (day_year[2] - 1)) &&
				(dysize(tm->tm_year) == 366)) {
			while (tm->tm_yday > (day_year[i+1]))
				i++;
			if (i > 1)
				tm->tm_mday = (tm->tm_yday - (day_year[i]+1)) +
					1;
			else
				tm->tm_mday = (tm->tm_yday - day_year[i]) + 1;
		} else {
			while (tm->tm_yday > (day_year[i+1] - 1))
				i++;
			tm->tm_mday = (tm->tm_yday - day_year[i]) + 1;
		}

		tm->tm_mon = i;
	}
}
#endif	/* __OSF_SPECIFIERS */


#ifdef __OSF_SPECIFIERS
/*
 * FUNCTION: conv_time (era_ptr era, int year, struct tm *tm)
 *	     By supplying the current era and year of the era, the function
 *	     converts this era/year combination into Christian ear and
 *	     set the tm->tm_year field of the tm time structure.
 *
 * PARAMETERS:
 *           era_ptr era - the era structure provides the related information
 *			   of the current era.
 *           int year - year of the specific era.
 *           struct tm *tm - a pointer to the time structure where the
 *			     tm->tm_year field will be set.
 *
 * RETURN VALUE DESCRIPTIONS:
 *           - returns 1 if the conversion is valid and successful.
 *           - returns 0 if fails.
 */
static int
conv_time(era_ptr era, int year, struct tm *tm, strptime_data_t *strptime_data)
{
	char *str;
	int start_year = 0;
	int end_year = 0;
	int no_limit = 0;
	int i;

	str = era->st_date;
	if (*str == '-') {
		str++;
		STRTONUM(str, start_year);
		start_year = -start_year;
	}
	else
		STRTONUM(str, start_year);

	str = era->end_date;
	if ((*str == '+' && *(str + 1) == '*') ||
			(*str == '-' && *(str+1) == '*'))
		no_limit = 1;
	else if (*str == '-') {
		str++;
		STRTONUM(str, end_year);
		end_year = -end_year;
	}
	else
		STRTONUM(str, end_year);

	if (era->dir == '+') {
		i = year - era->offset + start_year;
		if (no_limit) {
			tm->tm_year = i - YEAR_1900;
			set_yr = 1;
			set_day_of_year(tm, strptime_data);
			return (1);
		}
		if (i <= end_year) {
			tm->tm_year = i - YEAR_1900;
			set_yr = 1;
			set_day_of_year(tm, strptime_data);
			return (1);
		}
		return (0);
	} else {
		if ((i = end_year - year) <= start_year) {
			tm->tm_year = i - YEAR_1900;
			set_yr = 1;
			set_day_of_year(tm, strptime_data);
			return (1);
		}
		return (0);
	}
}
#endif	/* __OSF_SPECIFIERS */

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

/*
 *  This function extracts a date from a
 *  date string of the form:  mm/dd/yy.
 */
static void
extract_era_date(struct simple_date *date, const char *era_str)
{
	char *p = (char *)era_str;

	if (p[1] == '*') {
		if (p[0] == '-') {	/* dawn of time */
			date->day = 1;
			date->month = 0;
			date->year = INT_MIN;
		} else {		/* end of time */
			date->day = 31;
			date->month = 11;
			date->year = INT_MAX;
		}
		return;
	}

	date->year = atoi(p);
	if (strchr(p, ':') < strchr(p, '/')) {	/* date is year only */
		date->month = 0;
		date->day = 1;
		return;
	}
	p = strchr(p, '/') + 1;
	date->month = atoi(p) - 1;
	p = strchr(p, '/') + 1;
	date->day = atoi(p);
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
	if (era->dir != '-' && era->dir != '+') {
		return (-1);
	}
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

static unsigned char *
parse_alternate(_LC_time_t *hdl, const unsigned char *buf,
		const unsigned char *fmt, struct tm *tm, struct tm *ct,
		strptime_data_t *strptime_data)
{
	int	off;
	unsigned char *newbuf;
	unsigned char *efmt;
	char *era_s;
	struct era_struct era_struct;
	era_ptr era;
	simple_date stdate;		/* start date */

	SKIP_TO_NWHITE(buf);

	era = &era_struct;

	if (!hdl->era || !*hdl->era) /* Are there ERAs in this locale? */
		return (NULL);		/* Nope */

	era_s = *hdl->era;
	for (; era_s != NULL; era_s = get_era_segment(era_s)) {

		if (extract_era_info(era, era_s) != 0)
			continue;	/* Malformated era, ignore it */

		efmt = (unsigned char *) era->form;		/* for %EY */

		switch (*fmt) {
		case 'c':		/* Alternative date time */
			efmt = (unsigned char *) nl_langinfo(ERA_D_T_FMT);
recurse:
			/*FALLTHROUGH*/

		case 'Y':
			newbuf = strptime_recurse(hdl, buf, efmt, tm, ct,
					strptime_data, 1);
			if (!newbuf)
				continue;
			buf = newbuf;
			goto matched;

		case 'C':		/* Base year */
			{
			size_t len;
			if (compare_str(buf, era->name, &len))
				continue;
			buf += len;
			goto matched;
			}

		case 'x':		/* Alternative date representation */
			efmt = (unsigned char *) nl_langinfo(ERA_D_FMT);
			goto recurse;

		case 'X':		/* Alternative time format */
			efmt = (unsigned char *) nl_langinfo(ERA_T_FMT);
			goto recurse;

		case 'y':		/* offset from %EC(year only) */
			off = GET_NUMBER(&buf, 8, NULL, NULL);
			tm->tm_year += off;
			goto matched;

		default:	return (NULL);
		}

	} /* end for */

	return (NULL);		/* Fell thru for-loop */

matched:
	/*
	 * Here only when matched on appropriate era construct
	 */
	switch (*fmt++) {
	case 'c':	break;	/* recursion filled in struct tm */

	case 'C':	extract_era_date(&stdate, era->st_date);
			tm->tm_year += stdate.year - era->offset - 1900;
			break;

	case 'x':	break;
	case 'X':	break;

	case 'y':	break;

	case 'Y':	break;
	}
	return ((unsigned char *) buf);
}

/*
 * FUNCTION: This the standard method for function strptime and getdate.
 *	     It parses the input buffer according to the format string. If
 *	     time related data are recgonized, updates the tm time structure
 *	     accordingly.
 *
 * PARAMETERS:
 *           _LC_time_t *hdl - pointer to the handle of the LC_TIME
 *			       catagory which contains all the time related
 *			       information of the specific locale.
 *	     const char *buf - the input data buffer to be parsed for any
 *			       time related information.
 *	     const char *fmt - the format string which specifies the expected
 *			       format to be input from the input buf.
 *	     struct tm *tm   - the time structure to be filled when appropriate
 *			       time related information is found.
 *			       The fields of tm structure are:
 *
 *			       int 	tm_sec		seconds [0,61]
 *			       int	tm_min		minutes [0,61]
 *			       int	tm_hour		hour [0,23]
 *			       int	tm_mday		day of month [1,31]
 *			       int	tm_mon		month of year [0,11]
 *			       int	tm_wday		day of week [0,6] Sun=0
 *			       int	tm_yday		day of year [0,365]
 *			       int 	tm_isdst	daylight saving flag
 *	     struct tm *ct   - the time structure for current time.
 *			       Used mainly to support getdate.
 *	     strptime_data_t *strptime_data   - stores data used between
 *				routines.
 *	     int flag	     - flag for recursive call (1 = recursive call)
 *
 * RETURN VALUE DESCRIPTIONS:
 *           - if successful, for strptime, it returns the pointer to the
 *	       character after the last parsed character in the input buf
 *	       string.  For getdate, it returns 1.
 *           - if fail for any reason, it returns a NULL pointer.
 */
static unsigned char *
strptime_recurse(_LC_time_t *hdl, const unsigned char *buf,
		const unsigned char *fmt, struct tm *tm, struct tm *ct,
		strptime_data_t *strptime_data, int flag)
{
	unsigned char	bufchr;		/* current char in buf string */
	unsigned char	fmtchr;		/* current char in fmt string */
	int	found;		/* boolean flag for a match of buf and fmt */
	int	width;		/* the field width of an locale entry */
	int 	lwidth;		/* the field width of an locale entry */
	int	i;
	unsigned char	*p;			/* temp pointer */
	int	oflag = 0;
	static const char	*xpg4_d_t_fmt = "%a %b %e %H:%M:%S %Y";

#ifdef __OSF_SPECIFIERS
	struct era_struct eras; /* a structure for current era */
	era_ptr era = &eras;	/* pointer to the current era struct */
	int	year = 0;	/* %o value, year in current era */
	char 	*era_s;		/* locale's empiror/era string */
	char	**era_list;	/* points to current candidate ERA */
#endif
	if (flag == 0)
		getnow(tm, ct);
	SKIP_TO_NWHITE(fmt);
	while ((fmtchr = *fmt++) && (bufchr = *buf)) {
						/* stop when buf or fmt ends */
		if (fmtchr != '%') {
			SKIP_TO_NWHITE(buf);
			bufchr = *buf;
			if (tolower(bufchr) == tolower(fmtchr)) {
				buf++;
				SKIP_TO_NWHITE(fmt);
				continue;	/* ordinary char, skip */
			}
			else
				/*
				 * error, ordinary char in fmt
				 * unmatch char in buf
				 */
				return (NULL);
		} else {
			fmtchr = *fmt++;
			oflag = 0;
			if (fmtchr == 'O') {
				oflag++;
				fmtchr = *fmt++;
			}
			switch (fmtchr) {
			case 'a':
			case 'A':
			/* locale's full or abbreviate weekday name */
				SKIP_TO_NWHITE(buf);
				found = 0;
				if ((calling_func == f_strptime) ||
					(fmtchr == 'A')) {
					for (i = 0; i < 7 && !found; i++) {
						if (compare_str(buf,
							hdl->day[i],
							&lwidth) == 0) {
							found = 1;
							buf += lwidth;
							break;
						}
					}
				}
				if ((found == 0) &&
					((calling_func == f_strptime) ||
					(fmtchr == 'a'))) {
					for (i = 0; i < 7 && !found; i++) {
						if (compare_str(buf,
							hdl->abday[i],
							&width) == 0) {
							found = 1;
							buf += width;
							break;
						}
					}
				}
				if (found == 0)
					return (NULL);
				i = i + 1;
				if (tm->tm_wday && tm->tm_wday != i) {
					wrong_input++;
					return (NULL);
				}
				tm->tm_wday = i;
				break;

			case 'b':
			case 'B':
			case 'h':
			/* locale's full or abbreviate month name */
				SKIP_TO_NWHITE(buf);
				found = 0;
				if ((calling_func == f_strptime) ||
					(fmtchr == 'B')) {
					for (i = 0; i < 12 && !found; i++) {
						if (compare_str(buf,
							hdl->mon[i],
							&lwidth) == 0) {
							found = 1;
							buf += lwidth;
							break;
						}
					}
				}
				if ((!found) && ((calling_func == f_strptime) ||
					(fmtchr == 'b') || (fmtchr == 'h'))) {
					for (i = 0; i < 12 && !found; i++) {
						if (compare_str(buf,
							hdl->abmon[i],
							&width) == 0) {
							found = 1;
							buf += width;
							break;
						}
					}
				}
				if (found == 0) {
					return (NULL);
				}
				i = i + 1;
				if (tm->tm_mon && tm->tm_mon != i) {
					wrong_input++;
					return (NULL);
				}
				tm->tm_mon = i;
				break;

			case 'c': 		/* locale's date and time */
				SKIP_TO_NWHITE(buf);
				if (__xpg4 != 0) { /* XPG4 mode */
					if (IS_C_LOCALE(hdl)) {
						p = (unsigned char *)
							xpg4_d_t_fmt;
					} else {
						p = (unsigned char *)
							hdl->d_t_fmt;
					}
				} else {		/* Solaris mode */
					p = (unsigned char *) hdl->d_t_fmt;
				}
				if ((buf = strptime_recurse(hdl, buf, p,
						tm, ct, strptime_data, 1))
				    == NULL)
					return (NULL);
				break;

			case 'd':		/* day of month, 1-31 */
			case 'e':
				SKIP_TO_NWHITE(buf);
				i = GET_NUMBER(&buf, 2, hdl->alt_digits, oflag);
				if (i < 1 || i > DAY_MON) {
					return (NULL);
				}
				if (tm->tm_mday && tm->tm_mday != i) {
					wrong_input++;
					return (NULL);
				}
				tm->tm_mday = i;
				break;

			case 'D':		/* %m/%d/%y */
				SKIP_TO_NWHITE(buf);
				if ((buf = strptime_recurse(hdl, buf,
							(const unsigned char *)
							"%m/%d/%y", tm, ct,
							strptime_data, 1))
				    == NULL)
					return (NULL);
				break;

			case 'E':
				buf = parse_alternate(hdl, buf, fmt, tm, ct,
						strptime_data);
				if (buf == NULL)
				    return (NULL);
				fmt++;
				break;

			case 'H':		/* hour 0-23 */
				SKIP_TO_NWHITE(buf);
				i = GET_NUMBER(&buf, 2, hdl->alt_digits, oflag);
				if (i >= 0 && i <= HOUR_24)
					i = i + 1;
				else
					return (NULL);
				if (hour && hour != i) {
					wrong_input++;
					return (NULL);
				}
				hour = i;
				break;

			case 'I':		/* hour 1-12 */
				SKIP_TO_NWHITE(buf);
				i = GET_NUMBER(&buf, 2, hdl->alt_digits, oflag);
				if (i < 1 || i > HOUR_12 + 1)
					return (NULL);
				if (tm->tm_hour && tm->tm_hour != i) {
					wrong_input++;
					return (NULL);
				}
				tm->tm_hour = i;
				break;

			case 'j':		/* day of year, 1-366 */
				SKIP_TO_NWHITE(buf);
				i = GET_NUMBER(&buf, 3, hdl->alt_digits, oflag);
				if (i < 1 || i > DAY_YR)
					return (NULL);
				if (tm->tm_yday && tm->tm_yday != i) {
					wrong_input++;
					return (NULL);
				}
				tm->tm_yday = i;
				break;

			case 'm':		/* month of year, 1-12 */
				SKIP_TO_NWHITE(buf);
				i = GET_NUMBER(&buf, 2, hdl->alt_digits, oflag);
				if (i <= 0 || i > MONTH)
					return (NULL);
				if (tm->tm_mon && tm->tm_mon != i) {
					wrong_input++;
					return (NULL);
				}
				tm->tm_mon = i;
				break;

			case 'M':		/* minute 0-59 */
				SKIP_TO_NWHITE(buf);
				i = GET_NUMBER(&buf, 2, hdl->alt_digits, oflag);
				if (i >= 0 && i <= MINUTE)
					i = i + 1;
				else
					return (NULL);
				if (tm->tm_min && tm->tm_min != i) {
					wrong_input++;
					return (NULL);
				}
				tm->tm_min = i;
				break;

#ifdef __OSF_SPECIFIERS
			case 'N':
				SKIP_TO_NWHITE(buf);
				if (!hdl->era || !*hdl->era)
				    return (NULL);

				era_list = hdl->era;

				era_name = 0; 		/* No match yet  */

				for (era_s = *era_list; era_s != NULL;
						era_list++) {
				    int nmatch;
				    char dirbuf[2];
				    int i;

				    nmatch = sscanf(era_s,
					"%[+-]:%u:%99[^:]:%99[^:]:%99[^:]:%n",
					    dirbuf,
					    &era->offset,
					    era->st_date,
					    era->end_date,
					    era->name,
					    &i);

				    if (nmatch != 5) 	/* Bad era string */
					continue; 	/* Try next one */

				    era->dir = dirbuf[0];
				    strcpy(era->form, &era_s[i]);

				    /* Match era name and contents of buffer */
				    if (compare_str(buf, era->name, &i) == 0) {
					buf += i;
					era_name = 1; 	/* Found a match */
					break;	/* Stop looking for others */
				    }

				} /* end-for */

				if (era_name) {
				    if (era_year) {
					era_name = 0;
					era_year = 0;
					if (!conv_time(era, year, tm,
							strptime_data))
					    return (NULL);
				    }
				} else
				    return (NULL);
				break;
#endif	/* __OSF_SPECIFIERS */

			case 'n':		/* new line character */
				while (*buf && isspace(*buf))
					buf++;	/* skip all white space */
				break;

#ifdef __OSF_SPECIFIERS
			case 'o':		/* year of era */
				SKIP_TO_NWHITE(buf);
				STRTONUM(buf, year);
				if (year >= 0) {
					era_year = 1;
					if (era_name) {
						era_year = 0;
						era_name = 0;
						if (!conv_time(era, year, tm,
								strptime_data))
							return (NULL);
					}
				}
				break;
#endif	/* __OSF_SPECIFIERS */

			case 'p':		/* locale's AM or PM */
				SKIP_TO_NWHITE(buf);
				if (compare_str(buf, hdl->am_pm[0], &width) ==
						0) {
					i = AM;
					buf += width;
				} else if (compare_str(buf, hdl->am_pm[1],
							&lwidth) == 0) {
					i = PM;
					buf += lwidth;
				}
				else
					return (NULL);

				if (meridian && meridian != i) {
					wrong_input++;
					return (NULL);
				}
				meridian = i;

				break;

			case 'R': 		/* %H:%M */
				SKIP_TO_NWHITE(buf);
				buf = strptime_recurse(hdl, buf,
					(const unsigned char *) "%H:%M", tm,
					ct, strptime_data, 1);
				if (buf == NULL)
					return (NULL);
				break;

			case 'r':		/* locale's am/pm time format */
				SKIP_TO_NWHITE(buf);
				if ((buf = strptime_recurse(hdl, buf,
						(const unsigned char *)
						hdl->t_fmt_ampm, tm,
						ct, strptime_data, 1))
				    == NULL)
					return (NULL);
				break;

			case 'S':		/* second 0-61 */
				SKIP_TO_NWHITE(buf);
				i = GET_NUMBER(&buf, 2, hdl->alt_digits, oflag);
				if (i >= 0 && i <= SECOND)
					i = i + 1;
				else
					return (NULL);
				if (tm->tm_sec && tm->tm_sec != i) {
					wrong_input++;
					return (NULL);
				}
				tm->tm_sec = i;
				break;

			case 't':		/* tab character */
				while (*buf && isspace(*buf))
					buf++;	/* skip all white prior to \n */
				break;

			case 'T':		/* %H:%M:%S */
				SKIP_TO_NWHITE(buf);
				if ((buf = strptime_recurse(hdl, buf, (const
					unsigned char *) "%H:%M:%S", tm, ct,
					strptime_data, 1))
				    == NULL)
					return (NULL);
				break;

			case 'U':
				SKIP_TO_NWHITE(buf);
				i = GET_NUMBER(&buf, 2, hdl->alt_digits, oflag);
				if (i < 0 || i > WEEK_YR)
					return (NULL);
				if (week_number_u != -1 &&
					week_number_u != i) {
					wrong_input++;
					return (NULL);
				}
				week_number_u = i;
				break;

			case 'W':		/* week of year, 0-53 */
				SKIP_TO_NWHITE(buf);
				i = GET_NUMBER(&buf, 2, hdl->alt_digits, oflag);
				if (i < 0 || i > WEEK_YR)
					return (NULL);
				if (week_number_w != -1 &&
					week_number_w != i) {
					wrong_input++;
					return (NULL);
				}
				week_number_w = i;
				break;

			case 'w':		/* day of week, 0-6 */
				SKIP_TO_NWHITE(buf);
				i = GET_NUMBER(&buf, 1, hdl->alt_digits, oflag);
				i = i + 1;
				if (i < 1 || i > 7)
					return (NULL);
				if (tm->tm_wday && tm->tm_wday != i) {
					wrong_input++;
					return (NULL);
				}
				tm->tm_wday = i;
				break;

			case 'x':		/* locale's date format */
				SKIP_TO_NWHITE(buf);
				if ((buf = strptime_recurse(hdl, buf, (const
						unsigned char *) hdl->d_fmt,
						tm, ct, strptime_data, 1))
				    == NULL)
					return (NULL);
				break;

			case 'X':		/* locale's time format */
				SKIP_TO_NWHITE(buf);
				if ((buf = strptime_recurse(hdl, buf, (const
						unsigned char *) hdl->t_fmt,
						tm, ct, strptime_data, 1))
				    == NULL)
					return (NULL);
				break;

			case 'C':		/* century number */
				SKIP_TO_NWHITE(buf);
				i = GET_NUMBER(&buf, 2, hdl->alt_digits, oflag);
				/* Year must be between 1970 and 2038 */
				if (i < 0 || i > 99)
					return (NULL);
				if (century != -1 && century != i) {
					wrong_input++;
					return (NULL);
				}
				century = i;
				break;

			case 'y':		/* year of century, 0-99 */
				SKIP_TO_NWHITE(buf);
				i = GET_NUMBER(&buf, 2, hdl->alt_digits, oflag);
				if (i >= 70 || i < 38) {
					i = (i < 38) ? 100 + i : i;
				} else {
					return (NULL);
				}
				if (tm->tm_year && tm->tm_year != i) {
					wrong_input++;
					return (NULL);
				}
				tm->tm_year = i;
				set_yr = 1;
				break;

			case 'Y':		/* year with century, dddd */
				/*
				 * The last time UNIX can handle is
				 * 1/18/2038; for simplicity stop at 2038.
				 */
				SKIP_TO_NWHITE(buf);
				i = GET_NUMBER(&buf, 4, hdl->alt_digits, oflag);
				if ((i < 1970) || (i > 2037)) {
					return (NULL);
				} else {
					i = i - YEAR_1900;
				}
				if (tm->tm_year && tm->tm_year != i) {
					wrong_input++;
					return (NULL);
				}
				tm->tm_year = i;
				set_yr = 1;
				break;

			case 'Z':		/* time zone name */
				SKIP_TO_NWHITE(buf);
				tzset();
				if (compare_str(buf, tzname[0], &width) == 0) {
					tm->tm_isdst = 1;
					buf += width;
				} else if (compare_str(buf, tzname[1], &lwidth)
								== 0) {
					tm->tm_isdst = 2;
					buf += lwidth;
				} else {
					return (NULL);
				}
				break;

			case '%' :		/* double % character */
				SKIP_TO_NWHITE(buf);
				bufchr = *buf;
				if (bufchr == '%')
					buf++;
				else
					return (NULL);
				break;

			default:
				wrong_input++;
				return (NULL);
			} /* switch */
		} /* else */
		SKIP_TO_NWHITE(fmt);
	} /* while */
	if (fmtchr)
		return (NULL); 		/* buf string ends before fmt string */
	if (flag)
		return ((unsigned char *) buf);
	if (calling_func == f_getdate) {
		while (isspace(*buf))
			buf++;
		if (*buf)
			return (0);
		if (verify_getdate(tm, ct, strptime_data))
			return ((unsigned char *) 1);  /* success completion */
		else
			return (0);
	} else {	/* calling_function == f_strptime */
		if (verify_strptime(tm, ct, strptime_data))
			return ((unsigned char *) buf);	/* success completion */
		else
			return (0);
	}
}


/*
 * Read the user specified template file by line
 * until a match occurs.
 * The DATEMSK environment variable points to the template file.
 */
static int
read_tmpl(hdl, line, t, ct, strptime_data)
_LC_time_t *hdl;
char	*line;
struct tm *t;
struct tm *ct;
strptime_data_t *strptime_data;
{
	FILE  *fp;
	char	*file;
	char *bp, *start;
	struct stat64 sb;
	unsigned char	*ret = NULL;

	if (((file = getenv("DATEMSK")) == 0) || file[0] == '\0') {
		getdate_err = 1;
		return (0);
	}
	if ((start = (char *)malloc(512)) == NULL) {
		getdate_err = 6;
		return (0);
	}
	if (access(file, R_OK) != 0 || (fp = fopen(file, "r")) == NULL) {
		getdate_err = 2;
		free(start);
		return (0);
	}
	if (stat64(file, &sb) < 0) {
		getdate_err = 3;
		goto end;
	}
	if ((sb.st_mode & S_IFMT) != S_IFREG) {
		getdate_err = 4;
		goto end;
	}

	for (;;) {
		bp = start;
		if (!fgets(bp, 512, fp)) {
			if (!feof(fp)) {
				getdate_err = 5;
				ret = 0;
				break;
			}
			getdate_err = 7;
			ret = 0;
			break;
		}
		if (*(bp+strlen(bp)-1) != '\n')  { /* terminating newline? */
			getdate_err = 5;
			ret = 0;
			break;
		}
		*(bp + strlen(bp) - 1) = '\0';
#ifdef DEBUG
printf("line number \"%2d\"---> %s\n", linenum, bp);
#endif
		if (strlen(bp)) {  /*  anything left?  */

			/* Initialiize "hidden" global/static var's */
			init_str_data(strptime_data, f_getdate);

			if (ret = strptime_recurse(hdl, (const unsigned char *)
					line, (const unsigned char *) bp, t,
					ct, strptime_data, 0))
				break;
		}
	}
end:
	free(start);
	(void) fclose(fp);
	if (ret == NULL)
		return (0);
	else
		return (1);
}


/*
 * return time from time structure
 */
static struct  tm *
calc_date(t, ct)
struct tm *t;
struct tm *ct;
{
	long	tv;
	struct  tm nct;

	nct = *ct;
	tv = mktime(ct);
	if (!t->tm_isdst && ct->tm_isdst != nct.tm_isdst) {
		nct.tm_isdst = ct->tm_isdst;
		tv = mktime(&nct);
	}
	ct = localtime_r(&tv, ct);
	return (ct);
}

static void
getnow(t, ct)	/*  get current date */
struct tm *t;
struct tm *ct;
{
	time_t now;

	now = time((time_t *)NULL);
	ct = localtime_r(&now, ct);
	ct->tm_yday += 1;
	init_tm(t);
}

static void
init_tm(t)
struct tm *t;
{
	t->tm_year = t->tm_mon = t->tm_mday = t->tm_wday = t->tm_hour = 0;
	t->tm_min = t->tm_sec = t->tm_isdst = t->tm_yday = 0;
}

static void
init_str_data(strptime_data_t *strptime_data, int func_type)
{
	/*
	 * Initialiize "hidden" global/static var's
	 * (Note:  wrong_input value is saved.)
	 */
	set_yr = 0;
	hour = 0;
	meridian = 0;
	era_name = 0;
	era_year = 0;
	week_number_u = -1;
	week_number_w = -1;
	century = -1;
	calling_func = func_type;
}

/*
 * Check validity of input for strptime
 */
static int
verify_strptime(t, ct, strptime_data)
struct tm *t;
struct tm *ct;
strptime_data_t *strptime_data;
{
	int leap;

	leap = (days(t->tm_year) == 366);

	if (week_number_u != -1 || week_number_w != -1)
		if (week_number_to_yday(t, t->tm_year, strptime_data) == -1)
			return (0);

	if (t->tm_yday)
		if (yday(t, leap, ct, strptime_data) == -1)
			return (0);

	if (t->tm_hour) {
		switch (meridian) {
			case PM:
				t->tm_hour %= 12;
				t->tm_hour += 12;
				break;
			case AM:
				t->tm_hour %= 12;
				break;
		}
	}
	if (hour)
		t->tm_hour = hour - 1;
	if (t->tm_min)
		t->tm_min--;
	if (t->tm_sec)
		t->tm_sec--;

	if (t->tm_wday)
		t->tm_wday--;
	if (t->tm_mon)
		t->tm_mon--;

	/* If century called, but year not called, use century as the year */
	if ((century != -1) && !set_yr) {
		t->tm_year += (100 * century) - YEAR_1900;
	}

	return (1);
}


/*
 * Check validity of input for getdate
 */
static int
verify_getdate(t, ct, strptime_data)
struct tm *t;
struct tm *ct;
strptime_data_t *strptime_data;
{
	int min = 0;
	int sec = 0;
	int hr = 0;
	int leap;

	if (t->tm_year)
		year(t->tm_year, ct);
	leap = (days(ct->tm_year) == 366);

	if (week_number_u != -1 || week_number_w != -1)
		if (week_number_to_yday(t, ct->tm_year, strptime_data) == -1) {
			wrong_input++;
			return (0);
		}
	if (t->tm_yday)
		if (yday(t, leap, ct, strptime_data) == -1) {
			wrong_input++;
			return (0);
		} else
			t->tm_yday = 0;
	if (t->tm_mon)
		MON(t->tm_mon - 1, ct);
	if (t->tm_mday)
		Day(t->tm_mday, ct);
	if (t->tm_wday)
		DOW(t->tm_wday - 1, ct);

	if (((t->tm_mday)&&((t->tm_mday != ct->tm_mday) ||
	    (t->tm_mday > __mon_lengths[leap][ct->tm_mon]))) ||
	    ((t->tm_wday)&&((t->tm_wday-1) != ct->tm_wday)) ||
	    ((t->tm_hour&&hour)||(t->tm_hour&&!meridian) ||
	    (!t->tm_hour&&meridian)||(hour&&meridian))) {
		wrong_input++;
		return (0);
	}
	if (t->tm_hour) {
		switch (meridian) {
			case PM:
				t->tm_hour %= 12;
				t->tm_hour += 12;
				break;
			case AM:
				t->tm_hour %= 12;
				if (t->tm_hour == 0)
					hr++;
				break;
			default:
				return (0);
		}
	}
	if (hour)
		t->tm_hour = hour - 1;
	if (t->tm_min) {
		min++;
		t->tm_min -= 1;
	}
	if (t->tm_sec) {
		sec++;
		t->tm_sec -= 1;
	}

	if ((! t->tm_year && ! t->tm_mon && ! t->tm_mday && ! t->tm_wday) &&
	    ((t->tm_hour || hour || hr || min || sec) &&
	    ((t->tm_hour < ct->tm_hour) || ((t->tm_hour == ct->tm_hour) &&
	    (t->tm_min < ct->tm_min)) || ((t->tm_hour == ct->tm_hour) &&
	    (t->tm_min == ct->tm_min) && (t->tm_sec < ct->tm_sec)))))
		t->tm_hour += 24;

	if (t->tm_hour || hour || hr || min || sec) {
		ct->tm_hour = t->tm_hour;
		ct->tm_min = t->tm_min;
		ct->tm_sec = t->tm_sec;
	}
	if (t->tm_isdst)
		ct->tm_isdst = t->tm_isdst - 1;
	else
		ct->tm_isdst = 0;
	return (1);
}

/*
 *  Parses a number.  If oflag is set, and alternate digits
 *  are defined for the locale, call the routine to
 *  parse alternate digits.  Otherwise, parse ASCII digits.
 */
static int
get_number(char **buf, int length, char *alt_digits, int oflag)
{
	int ret;

	if ((oflag == 0) || (alt_digits == 0) || ((alt_digits != 0) &&
			(*alt_digits == '\0'))) {
		ret = number((char **) buf, length);
	} else {
		ret = search_alt_digits((char **) buf, alt_digits);
	}
	return (ret);
}

/*
 * Parse the number given by the specification.
 * Allow at most length digits.
 */
static int
number(char **input, int length)
{
	int	val;
	unsigned char c;

	val = 0;
	if (!isdigit((unsigned char)**input))
		return (-1);
	while (length--) {
		if (!isdigit(c = **input))
			return (val);
		val = 10*val + c - '0';
		(*input)++;
	}
	return (val);
}

/*
 * Compare input against list of alternate digits.
 */
static int
search_alt_digits(char **buf, char *alt_digits)
{
#define	STRTOK_R(a, b, c)	strtok_r((a), (b), (c))
	int num, i;
	int length = 0, prev_length;
	char *digs, *cand, *tmp;

	if ((digs = strdup(alt_digits)) == NULL)
		return (-1);
	cand = STRTOK_R(digs, ";", &tmp);
	prev_length = 0;

	for (i = 0, num = -1; cand; i++) {
		if (compare_str(*buf, cand, &length) ==
				0) {
			if (length > prev_length) {
				prev_length = length;
				num = i;
			}
		}
		cand = STRTOK_R(NULL, ";", &tmp);
	}
	free(digs);

	if (num == -1)
		return (-1);

	*buf += prev_length;
	return (num);
}

static void
Day(day, ct)
int day;
struct tm *ct;
{
	if (day < ct->tm_mday)
		if (++ct->tm_mon == 12)  ++ct->tm_year;
	ct->tm_mday = day;
	DMY(ct);
}

static void
DMY(struct tm *ct)
{
	int doy;
	if (days(ct->tm_year) == 366)
		doy = __lyday_to_month[ct->tm_mon];
	else
		doy = __yday_to_month[ct->tm_mon];
	ct->tm_yday = doy + ct->tm_mday;
	ct->tm_wday = (jan1(ct->tm_year) + ct->tm_yday - 1) % 7;
}

static int
days(y)
int	y;
{
	y += 1900;
	return (y%4 == 0 && y%100 != 0 || y%400 == 0 ? 366 : 365);
}


/*
 *	return day of the week
 *	of jan 1 of given year
 */
static int
jan1(yr)
{
	register y, d;

/*
 *	normal gregorian calendar
 *	one extra day per four years
 */

	y = yr + 1900;
	d = 4+y+(y+3)/4;

/*
 *	julian calendar
 *	regular gregorian
 *	less three days per 400
 */

	if (y > 1800) {
		d -= (y-1701)/100;
		d += (y-1601)/400;
	}

/*
 *	great calendar changeover instant
 */

	if (y > 1752)
		d += 3;

	return (d%7);
}

static void
year(yr, ct)
int	yr;
struct tm *ct;
{
	ct->tm_mon = 0;
	ct->tm_mday = 1;
	ct->tm_year = yr;
	DMY(ct);
}

static void
MON(month, ct)
int month;
struct tm *ct;
{
	ct->tm_mday = 1;
	Month(month, ct);
}

static void
Month(month, ct)
int month;
struct tm *ct;
{
	if (month < ct->tm_mon)  ct->tm_year++;
	ct->tm_mon = month;
	DMY(ct);
}

static void
DOW(dow, ct)
int	dow;
struct tm *ct;
{
	adddays((dow+7-ct->tm_wday)%7, ct);
}

static void
adddays(n, ct)
int	n;
struct tm *ct;
{
	DOY(ct->tm_yday+n, ct);
}

static void
DOY(doy, ct)
int	doy;
struct tm *ct;
{
	int i, leap;

	if (doy > days(ct->tm_year)) {
		doy -= days(ct->tm_year);
		ct->tm_year++;
	}
	ct->tm_yday = doy;

	leap = (days(ct->tm_year) == 366);
	for (i = 0; doy > __mon_lengths[leap][i]; i++)
		doy -= __mon_lengths[leap][i];
	ct->tm_mday = doy;
	ct->tm_mon = i;
	ct->tm_wday = (jan1(ct->tm_year)+ct->tm_yday-1) % 7;
}

static int
yday(struct tm *t, int leap, struct tm *ct, strptime_data_t *strptime_data)
{
	int	month;
	int	day_of_month;
	int	*days_to_months;

	days_to_months = (int *) (leap ? __lyday_to_month : __yday_to_month);
	t->tm_yday--;

	if (!t->tm_year) {
		t->tm_year = ct->tm_year;
		year(t->tm_year, ct);
	}

	for (month = 1; month < 12; month++)
		if (t->tm_yday < days_to_months[month])
			break;

	if (t->tm_mon && t->tm_mon != month - 1)
		return (-1);

	t->tm_mon = month;
	day_of_month = t->tm_yday - days_to_months[month - 1] + 1;
	if (t->tm_mday && t->tm_mday != day_of_month)
		return (-1);

	t->tm_mday = day_of_month;
	return (0);
}

static int
week_number_to_yday(struct tm *t, int year, strptime_data_t *strptime_data)
{
	int	yday;

	if (week_number_u != -1) {
		yday = 7 * week_number_u + t->tm_wday - jan1(year);
		if (t->tm_yday && t->tm_yday != yday)
			return (-1);
		t->tm_yday = yday;
	}
	if (week_number_w != -1) {
		yday = (8 - jan1(year) % 7) + 7 * (week_number_w - 1) +
		    t->tm_wday - 1;
		if (t->tm_wday == 1)
			yday += 7;

		if (t->tm_yday && t->tm_yday != yday)
			return (-1);
		t->tm_yday = yday;
	}
	return (0);
}

/*
 * FUNCTION:  compare_str()
 *
 * PARAMETERS:
 *	unsigned char *s1 = pointer to the input buffer;
 *				s1 is assumed to begin at
 *				non-whitespace.
 *	unsigned char *s2 = pointer to the string to match;
 *				s2 may begin with whitespace.
 *	int *n		  = pointer to the string length of s2;
 *				if the return value is 0, *n has
 *				the value of the number of bytes
 *				matched.
 * RETURN VALUE DESCRIPTIONS:
 *	Returns 0 if a match is found.  Otherwise, it returns
 *	a positive or negative integer if the string pointed to by s1
 *	is greater than or less than the string pointed to by s2.
 */
static int
compare_str(unsigned char *s1, unsigned char *s2, int *n)
{
	int n2;

	SKIP_TO_NWHITE(s2);	/* assume s1 already points to non-white */
	n2 = strlen((char *) s2);
	*n = n2;		/* return number of bytes */

	while (--n2 >= 0 && (tolower(*s1) == tolower(*s2++)))
		if (*s1++ == '\0')
			return (0);
	return (n2 < 0 ? 0 : tolower(*s1) - tolower(*--s2));
}

/*
 * This is a wrapper for the real function which is recursive.
 * getdate and strptime share the parsing function, strptime_recurse.
 * The global data is encapsulated in a structure which is initialised
 * here and then passed by reference.
 */
struct  tm *
__getdate_std(_LC_time_t *hdl, const char *expression)
{
	struct tm t;
	struct tm *res;
	strptime_data_t real_strptime_data;
	strptime_data_t *strptime_data = &real_strptime_data;
	struct   tm  *ct;

#ifdef _REENTRANT
	static thread_key_t gd_key = 0;
#endif _REENTRANT

#ifdef _REENTRANT
	ct = (struct tm *)_tsdalloc(&gd_key, sizeof (struct tm));
#else
	if (!ct)
		ct = malloc(sizeof (struct tm));
#endif /* _REENTRANT */

	if (ct == NULL) {
		getdate_err = 6;
		return (NULL);
	}
	/* Initialiize "hidden" global/static var's */
	init_str_data(strptime_data, f_getdate);
	wrong_input = 0;	/* only initialize this once */

	if (read_tmpl(hdl, (char *)expression, &t, ct, strptime_data)) {
		res = calc_date(&t, ct);
		return (res);
	} else {
		if (wrong_input)
			getdate_err = 8;
		return (NULL);
	}
}


/*
 * This is a wrapper for the real function which is recursive.
 * The global data is encapsulated in a structure which is initialised
 * here and then passed by reference.
 */
char *
__strptime_std(_LC_time_t *hdl, const char *buf, const char *fmt, struct tm *tm)
{
	strptime_data_t real_strptime_data;
	strptime_data_t *strptime_data = &real_strptime_data;
	struct tm	ct;

	/* Initialiize "hidden" global/static var's */
	init_str_data(strptime_data, f_strptime);
	wrong_input = 0;

	/* Call the recursive, thread-safe routine */
	return ((char *) strptime_recurse(hdl, (const unsigned char *) buf,
				(const unsigned char *) fmt, tm, &ct,
				strptime_data, 0));
}
