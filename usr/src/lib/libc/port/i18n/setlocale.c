/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#ident	"@(#)setlocale.c	1.19	96/09/26 SMI"

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
static char rcsid[] = "@(#)$RCSfile: setlocale.c,v $ $Revision: 1.14.6.4"
	" $ (OSF) $Date: 1992/11/21 18:54:54 $";
#endif
*/
/*
 * COMPONENT_NAME: (LIBCCHR) LIBC Character Classification Funcions
 *
 * FUNCTIONS: setlocale
 *
 *
 * (C) COPYRIGHT International Business Machines Corp. 1989
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 * 1.12  com/lib/c/loc/setlocale.c, libcloc, bos320, 9132320m 8/11/91 14:14:10
 */


#pragma weak setlocale = _setlocale

#include "synonyms.h"
#include "shlib.h"
#include <locale.h>
#include "_locale.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/localedef.h>
#include <thread.h>
#include <synch.h>
#include <mtlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#if defined(PIC)
#include <dlfcn.h>
#include <sys/types.h>
#include <unistd.h>
#endif

/*
** global variables set up by setlocale()
**
**	INVARIANT:	If any global variable changes, setlocale() returns
**			successfully.
*/
extern _LC_charmap_t	*__lc_charmap;
extern _LC_ctype_t	*__lc_ctype;
extern _LC_collate_t	*__lc_collate;
extern _LC_numeric_t	*__lc_numeric;
extern _LC_monetary_t	*__lc_monetary;
extern _LC_time_t	*__lc_time;
extern _LC_messages_t	*__lc_messages_t;
extern _LC_locale_t	*__lc_locale;

#if defined(PIC)
static void	*__lc_load(const char *, const char *, void *(*)());
static void	*load(const char *);

#ifdef SUPPORT_XPG4_MODE
static int	__ispriv(void);
#endif /* SUPPORT_XPG4_MODE */

#define	_LC_SYMBOL	"instantiate"
#define	_LC_SHARED_OBJ_SUFFIX	".so.%d"
#endif

extern _LC_locale_t	*__C_locale;	/* Hardwired default */

static char *_real_setlocale(int, const char *, char **);
static _LC_locale_t	*load_all_locales(const char *, const char *,
	const char *, char **);
static _LC_locale_t	*load_locale(const char *, const char *);
static char	*expand_locale_name(const char *, const char *, const int);
static void	create_composite_locale(char *, char **);

/*
** The following symbols are for the backward compatibility with libw.a.
*/
#ifdef _REENTRANT
extern mutex_t	_locale_lock;
#endif /* _REENTRANT */
extern int	_lflag;

/*
** Static strings to record locale names used to load
** specific locale categories.
**
** This data is used to build the locale state string.
*/

static const char	POSIX[] = "POSIX";
static const char	C[] = "C";

#define	FREEP(a) {if (((a) != C) && ((a) != POSIX)) free((a)); }
#define	LOAD_LOC(loc) \
	{ \
		lp = load_locale((loc), locpath); \
		if (!lp) { \
			return ((_LC_locale_t *)NULL); \
		} \
	}

/*
 * FUNCTION: _free_tsd()
 *
 * DESCRIPTION:
 * Releases thread specific memories allocated by _thr_setspecific().
 * This destructor is called when the thread is killed.
 */
static void
_free_tsd(void *tmp_ans)
{
	char	**ans;
	ans = (char **)tmp_ans;
	if (*ans &&
		(*ans != (char *)C) && (*ans != (char *)POSIX)) {
		free(*ans);
	}
	free(tmp_ans);
}

/*
*  FUNCTION: setlocale
*
*  DESCRIPTION:
*  Allocates memory for each thread and calls _real_setlocale.
*
*  RETURNS:
*  A string of names separated by '/' which represents the effective locale
*  names for each of the categories affected by 'category'.
*  If every category has the same effective locale, a string of the name
*  is returned.
*  A space separated string of names which represents the effective locale
*  names for each of the categories affected by 'category' in XPG4 mode.
*/
char *
setlocale(int cat, const char *loc)
{
	static char	*stat_ans = (char *)NULL;
	static thread_key_t	ans_key = 0;
	char	**ans = (char **)NULL;

	if (_thr_main()) {			/* Main thread? */
		/* Use static area for the storage */
		ans = &stat_ans;
		/* Call real setlocale */
		*ans = _real_setlocale(cat, loc, ans);
		return (*ans);
	}

	/* setlocale was invoked by sub thread */
	if (_thr_getspecific(ans_key, (void **)&ans) != 0) {
		/* This is the first time the thread invoked setlocale */
		if (_thr_keycreate(&ans_key, (void (*)(void *))_free_tsd)
			!= 0) {
			/* Failed to create key for the thread */
			return ((char *)NULL); /* setlocale failed */
		}
	}
	if (!ans) {
		/* Create thread specific data area */
		if (_thr_setspecific(ans_key,
			(void *)(ans = (char **)malloc(sizeof (char **))))
			!= 0) {
			/* Failed to create TSD */
			if (ans) {
				free(ans);
			}
			return ((char *)NULL); /* setlocale failed */
		} else {
			*ans = (char *)NULL; /* initialize to NULL */
		}
	}
	*ans = _real_setlocale(cat, loc, ans);
	return (*ans);
}

/*
*  FUNCTION: _real_setlocale
*
*  DESCRIPTION:
*  Loads the specified 'category' from the locale database 'locname'.
*
*  RETURNS:
*  A string of names separated by '/' which represents the effective locale
*  names for each of the categories affected by 'category'.
*  If every category has the same effective locale, a string of the name
*  is returned.
*  A space separated string of names which represents the effective locale
*  names for each of the categories affected by 'category' in XPG4 mode.
*/

static char *
_real_setlocale(int category, const char *locname, char **ans)
{
	_LC_locale_t	*lp;	/* locale temporory */
	_LC_locale_t	*ret;	/* return value from init function */

	char	*lc_all;	/* value of LC_ALL environment variable */
	char	*locpath = "";
	char	*s, *q;		/* temporary string pointer */
	char	**p;	/* points to locale_names or tmp_locale_names */
	char	*oans, *op;
#ifndef SUPPORT_XPG4_MODE
	char	path[PATH_MAX + 1];
	struct stat64	stat_buf;
#endif /* !SUPPORT_XPG4_MODE */
	int	i;
	int	isset = 1;	/* setting or query */
	int	composite = 0;	/* if 1, return a composite locale */
	int	is_c_locale = 0;	/* if 1, return C or POSIX locale */
	size_t	len = 0;
	static char	*locale_names[_LastCategory+1] = {
		(char *)C, (char *)C, (char *)C,
		(char *)C, (char *)C, (char *)C};
	static struct lconv	*local_lconv = (struct lconv *)NULL;
	static _LC_locale_t	*locale = (_LC_locale_t *)NULL;


	/* temporary */
	_LC_locale_t	tmp_locale;
	struct lconv	tmp_local_lconv;
	char	*tmp_locale_names[_LastCategory+1];


	/* Verify category parameter */
	if (!_ValidCategory(category)) {
		return ((char *)NULL);
	}


	/*
	 * Check special locname parameter overload which indicates that current
	 * locale state is to be saved.
	 */
	if (locname == NULL) {
		isset = 0;
		goto getlocale;
	}


	/*
	 * get values of environment variables which are globally usefull to
	 * avoid repeated calls to getenv()
	 */
#ifndef SUPPORT_XPG4_MODE
	locpath = _DFLT_LOC_PATH;
#else /* SUPPORT_XPG4_MODE */
	locpath = getenv("LOCPATH");
	if (locpath == (char *)NULL) {
		locpath = "";
	}
#endif /* !SUPPORT_XPG4_MODE */

	lc_all = getenv("LC_ALL");
	if (lc_all == (char *)NULL) {
		lc_all = "";
	}

	/* copy locale_names to tmp_locale_names */
	for (i = 0; i <= _LastCategory; i++) {
		tmp_locale_names[i] = locale_names[i];
	}

	/*
	 * Split logic for loading all categories versus loading a single
	 * category.
	 */
	if (category == LC_ALL) {
		/* load all locale categories */
		lp = load_all_locales(lc_all, (char *)locname, locpath,
			tmp_locale_names);
		if (!lp) {
			return ((char *)NULL);
		}
		if (lp == __C_locale) {	/* C or POSIX locale */
			is_c_locale = 1;
			if (__lc_locale == __C_locale) {
				goto getlocale;
			}
		} else {
			is_c_locale = 0;
		}

		tmp_locale.lc_collate = lp->lc_collate;
		tmp_locale.lc_ctype = lp->lc_ctype;
		tmp_locale.lc_charmap = lp->lc_charmap;
		tmp_locale.lc_monetary = lp->lc_monetary;
		tmp_locale.lc_numeric = lp->lc_numeric;
		tmp_locale.lc_time = lp->lc_time;
		tmp_locale.lc_messages = lp->lc_messages;

		tmp_locale.core.init = lp->core.init;
		tmp_locale.core.destructor = lp->core.destructor;
		tmp_locale.core.user_api = lp->core.user_api;
		tmp_locale.core.native_api = lp->core.native_api;
		tmp_locale.core.data = lp->core.data;

		tmp_locale.nl_lconv = &tmp_local_lconv;

		if (lp->core.init) {
			ret = (*(lp->core.init))(&tmp_locale);
		}
		if (ret == (_LC_locale_t *)-1) {
			return ((char *)NULL);	/* setlocale failed */
		}
	} else {
		/* load a specific category of locale information */
		s = expand_locale_name(locname, lc_all, (const int)category);
		lp = load_locale(s, locpath);
		if (!lp) {
#ifndef SUPPORT_XPG4_MODE
			if (category == LC_MESSAGES) {
				(void) strcpy(path, _DFLT_LOC_PATH);
				(void) strcat(path, s);
				(void) strcat(path, "/LC_MESSAGES");
				if ((stat64(path, &stat_buf) != 0) ||
					(!(stat_buf.st_mode & S_IFDIR))) {
					/* /usr/lib/locale/<loc>/LC_MESSAGES */
					/* directory does not exist. */
					return ((char *)NULL);
				} else {
					/* Evenif load for the locale object */
					/* failed setting of LC_MESSAGES */
					/* category must succeed as long as */
					/* /usr/lib/locale/<loc>/LC_MESSAGES */
					/* directory exists. */
					lp = __C_locale;
				}
			} else {
				return ((char *)NULL);
			}
#else /* SUPPORT_XPG4_MODE */
			return ((char *)NULL);
#endif /* !SUPPORT_XPG4_MODE */
		}

		if ((s != (char *)C) && (s != (char *)POSIX)) {
			/* Setup saved locale string */
			if ((s = strdup(s)) == NULL) {
				/* Couldn't dup the string */
				return ((char *)NULL);
			}
		}

		FREEP(tmp_locale_names[category]);
		tmp_locale_names[category] = s;

		/* call init method for category changed */
		tmp_locale.lc_charmap = __lc_charmap;
		tmp_locale.lc_collate = __lc_collate;
		tmp_locale.lc_ctype = __lc_ctype;
		tmp_locale.lc_monetary = __lc_monetary;
		tmp_locale.lc_numeric = __lc_numeric;
		tmp_locale.lc_time  = __lc_time;
		tmp_locale.lc_messages = __lc_messages;

		tmp_locale.core.init = lp->core.init;
		tmp_locale.core.destructor = lp->core.destructor;
		tmp_locale.core.user_api = lp->core.user_api;
		tmp_locale.core.native_api = lp->core.native_api;
		tmp_locale.core.data = lp->core.data;

		tmp_locale.nl_lconv = &tmp_local_lconv;

		/* call init method for category changed */

		switch (category) {
		case LC_COLLATE:
			tmp_locale.lc_collate = lp->lc_collate;
			tmp_locale.lc_charmap = lp->lc_charmap;
			break;
		case LC_CTYPE:
			tmp_locale.lc_ctype = lp->lc_ctype;
			tmp_locale.lc_charmap = lp->lc_charmap;
			break;
		case LC_MONETARY:
			tmp_locale.lc_monetary = lp->lc_monetary;
			break;
		case LC_NUMERIC:
			tmp_locale.lc_numeric = lp->lc_numeric;
			break;
		case LC_TIME:
			tmp_locale.lc_time = lp->lc_time;
			break;
		case LC_MESSAGES:
			tmp_locale.lc_messages = lp->lc_messages;
			break;
		}

		if (lp->core.init) {
			ret = (*(lp->core.init))(&tmp_locale);
		}

		if (ret == (_LC_locale_t *)-1) {
			return ((char *)NULL);	/* setlocale failed */
		}
	}


	/*
	 * Locale is loaded, now return descriptor that shows current settings
	 */

getlocale:

	if (category != LC_ALL) {
		if (!isset) {	/* query */
			if ((locale_names[category] == (char *)C) ||
				(locale_names[category] == (char *)POSIX)) {
				/* locale is C or POSIX */
				if (*ans &&
					(*ans != (char *)C) &&
					(*ans != (char *)POSIX)) {
					free(*ans);
				}
				*ans = locale_names[category];
				return (*ans);
				/* NOTREACHED */
			} else {	/* locale is neither C nor POSIX */
				if (*ans) {	/* answer has been set */
					if (strcmp(*ans, locale_names[category])
						== 0) {
						/* locale not changed */
						return (*ans);
						/* NOTREACHED */
					} else {
		/* locale has been changed */
		if ((*ans != (char *)C) &&
			(*ans != (char *)POSIX)) {
			free(*ans);
		}
					}
				}
				/* answer has not been set or */
				/* locale has been changed */
				len = strlen(locale_names[category]) + 1;
				*ans = (char *)malloc(len);
				if (*ans == NULL) {
					return ((char *)NULL);
					/* NOTREACHED */
				}
				/* copy locale name to the answer */
				oans = *ans;
				op = locale_names[category];
				while (*oans++ = *op++);
				return (*ans);
				/* NOTREACHED */
			}
		} else {	/* set */
			if (!locale) {
				locale = (_LC_locale_t *)
					malloc(sizeof (_LC_locale_t));
				if (locale == NULL) { /* malloc failed */
					return ((char *)NULL);
					/* NOTREACHED */
				}
			}
			if (!local_lconv) {
				local_lconv = (struct lconv *)
					malloc(sizeof (struct lconv));
				if (local_lconv == NULL) { /* malloc failed */
					return ((char *)NULL);
					/* NOTREACHED */
				}
			}
			if ((tmp_locale_names[category] == (char *)C) ||
				(tmp_locale_names[category] == (char *)POSIX)) {
				/* locale is C or POSIX */
				if (*ans &&
					(*ans != (char *)C) &&
					(*ans != (char *)POSIX)) {
					free(*ans);
				}
				*ans = tmp_locale_names[category];
			} else {	/* locale is neither C nor POSIX */
				if (*ans &&
					(*ans != (char *)C) &&
					(*ans != (char *)POSIX)) {
					free(*ans);
				}
				len = strlen(tmp_locale_names[category]) + 1;
				*ans = (char *)malloc(len);
				if (*ans == NULL) {
					return ((char *)NULL);
					/* NOTREACHED */
				}
				/* copy locale name to the answer */
				oans = *ans;
				op = tmp_locale_names[category];
				while (*oans++ = *op++);
			}

			/* For the backward compatibility with libw.a */
#ifdef _REENTRANT
			mutex_lock(&_locale_lock);
#endif /* _REENTRANT */
			_lflag = 0;
#ifdef _REENTRANT
			mutex_unlock(&_locale_lock);
#endif /* _REENTRANT */

			__lc_collate = tmp_locale.lc_collate;
			__lc_ctype = tmp_locale.lc_ctype;
			__lc_charmap = tmp_locale.lc_charmap;
			__lc_monetary = tmp_locale.lc_monetary;
			__lc_numeric = tmp_locale.lc_numeric;
			__lc_time = tmp_locale.lc_time;
			__lc_messages = tmp_locale.lc_messages;
			memcpy((void *)locale, (const void *)&tmp_locale,
				sizeof (_LC_locale_t));
			memcpy((void *)local_lconv,
				(const void *)&tmp_local_lconv,
				sizeof (struct lconv));
			locale->nl_lconv = local_lconv;
			__lc_locale = locale;

			/* copy tmp_locale_names to locale_names */
			for (i = 0; i <= _LastCategory; i++) {
				locale_names[i] = tmp_locale_names[i];
			}
			return (*ans);
			/* NOTREACHED */
		}
	} else {	/* LC_ALL */
		if (!isset) {	/* query */
#ifndef SUPPORT_XPG4_MODE
			len = strlen(locale_names[0]) + 1;
			for (q = locale_names[0], i = 1;
				i <= _LastCategory; i++) {
				if ((composite == 0) &&
					(strcmp(q, locale_names[i]) != 0)) {
					composite = 1;
				}
				q = locale_names[i];
				len += strlen(locale_names[i]) + 1;
			}
#else /* SUPPORT_XPG4_MODE */
			for (len = 0, i = 0; i <= _LastCategory; i++) {
				/* category plus blank or NULL */
				len += strlen(locale_names[i]) + 1;
			}
			composite = 1;	/* everytime composite = 1 */
#endif /* !SUPPORT_XPG4_MODE */

			if (composite == 1) { /* composite locale */
				if (*ans &&
					(*ans != (char *)C) &&
					(*ans != (char *)POSIX)) {
					free(*ans);
				}
				*ans = (char *)malloc(len + 1);
				if (*ans == NULL) {
					return ((char *)NULL);
					/* NOTREACHED */
				}
				create_composite_locale(*ans, locale_names);

				return (*ans);
				/* NOTREACHED */
			} else {	/* simple locale  */
				if ((locale_names[LC_CTYPE] == (char *)C) ||
					(locale_names[LC_CTYPE] ==
					(char *)POSIX)) {
					/* locale is C or POSIX */
					if (*ans &&
						(*ans != (char *)C) &&
						(*ans != (char *)POSIX)) {
						free(*ans);
					}
					*ans = locale_names[LC_CTYPE];
					return (*ans);
					/* NOTREACHED */
				} else {
					/* locale is neither C nor POSIX */
					if (*ans) {
						/* answer has been set */
						if (strcmp(*ans,
							locale_names[LC_CTYPE])
							== 0) {
							/* locale not changed */
							return (*ans);
							/* NOTREACHED */
						} else {
	/* locale has been changed */
	if ((*ans != (char *)C) &&
		(*ans != (char *)POSIX)) {
		free(*ans);
	}
						}
					}
					/* answer has not been set or */
					/* locale has been changed */
					len = strlen(locale_names[LC_CTYPE])
						+ 1;
					*ans = (char *)malloc(len);
					if (*ans == NULL) {
						return ((char *)NULL);
						/* NOTREACHED */
					}
					/* copy locale name to the answer */
					oans = *ans;
					op = locale_names[LC_CTYPE];
					while (*oans++ = *op++);

					return (*ans);
					/* NOTREACHED */
				}
			}
		} else {	/* set */
			if (!is_c_locale) {
				if (!locale) {
					locale = (_LC_locale_t *)
						malloc(sizeof (_LC_locale_t));
					if (locale == NULL) {
						/* malloc failed */
						return ((char *)NULL);
						/* NOTREACHED */
					}
				}
				if (!local_lconv) {
					local_lconv = (struct lconv *)
						malloc(sizeof (struct lconv));
					if (local_lconv == NULL) {
						/* malloc failed */
						return ((char *)NULL);
						/* NOTREACHED */
					}
				}
			}
#ifndef SUPPORT_XPG4_MODE
			len = strlen(tmp_locale_names[0]) + 1;
			for (q = tmp_locale_names[0], i = 1;
				i <= _LastCategory; i++) {
				if ((composite == 0) &&
					(strcmp(q, tmp_locale_names[i]) != 0)) {
					composite = 1;
				}
				q = tmp_locale_names[i];
				len += strlen(tmp_locale_names[i]) + 1;
			}
#else /* SUPPORT_XPG4_MODE */
			for (len = 0, i = 0; i <= _LastCategory; i++) {
				/* category plus blank or NULL */
				len += strlen(tmp_locale_names[i]) + 1;
			}
			composite = 1;	/* everytime composite = 1 */
#endif /* !SUPPORT_XPG4_MODE */

			if (composite == 1) { /* composite locale */
				if (*ans &&
					(*ans != (char *)C) &&
					(*ans != (char *)POSIX)) {
					free(*ans);
				}
				*ans = (char *)malloc(len + 1);
				if (*ans == NULL) {
					return ((char *)NULL);
				}

				create_composite_locale(*ans, tmp_locale_names);
			} else {	/* simple locale  */
				if ((tmp_locale_names[LC_CTYPE] == (char *)C) ||
					(tmp_locale_names[LC_CTYPE] ==
					(char *)POSIX)) {
					/* locale is C or POSIX */
					if (*ans &&
						(*ans != (char *)C) &&
						(*ans != (char *)POSIX)) {
						free(*ans);
					}
					*ans = tmp_locale_names[LC_CTYPE];
				} else {
					/* locale is neither C nor POSIX */
					if (*ans &&
						(*ans != (char *)C) &&
						(*ans != (char *)POSIX)) {
						free(*ans);
					}
					len = strlen(tmp_locale_names[LC_CTYPE])
						+ 1;
					*ans = (char *)malloc(len);
					if (*ans == NULL) {
						return ((char *)NULL);
					}
					oans = *ans;
					op = tmp_locale_names[LC_CTYPE];
					while (*oans++ = *op++);
				}
			}

			/* For the backward compatibility with libw.a */
#ifdef _REENTRANT
			mutex_lock(&_locale_lock);
#endif /* _REENTRANT */
			_lflag = 0;
#ifdef _REENTRANT
			mutex_unlock(&_locale_lock);
#endif /* _REENTRANT */

			if (is_c_locale == 1) {
				__lc_collate = lp->lc_collate;
				__lc_ctype = lp->lc_ctype;
				__lc_charmap = lp->lc_charmap;
				__lc_monetary = lp->lc_monetary;
				__lc_numeric = lp->lc_numeric;
				__lc_time = lp->lc_time;
				__lc_messages = lp->lc_messages;
				__lc_locale = lp;
			} else {
				__lc_collate = tmp_locale.lc_collate;
				__lc_ctype = tmp_locale.lc_ctype;
				__lc_charmap = tmp_locale.lc_charmap;
				__lc_monetary = tmp_locale.lc_monetary;
				__lc_numeric = tmp_locale.lc_numeric;
				__lc_time = tmp_locale.lc_time;
				__lc_messages = tmp_locale.lc_messages;
				memcpy((void *)locale,
					(const void *)&tmp_locale,
					sizeof (_LC_locale_t));
				memcpy((void *)local_lconv,
					(const void *)&tmp_local_lconv,
					sizeof (struct lconv));
				locale->nl_lconv = local_lconv;
				__lc_locale = locale;
			}
			/* copy tmp_locale_names to locale_names */
			for (i = 0; i <= _LastCategory; i++) {
				locale_names[i] = tmp_locale_names[i];
			}
			return (*ans);
			/* NOTREACHED */
		}
	}
}

/*
*	FUNCTION: create_composite_locale
*/
static void
create_composite_locale(char *answer, char **loc_name)
{
	char	*op;
	int	i;

#ifndef SUPPORT_XPG4_MODE
	*answer++ = '/';

	for (i = 0; i <= _LastCategory; i++) {
		op = *(loc_name + i);
		while (*answer++ = *op++);
		*(answer - 1) = '/';
	}
	*(answer - 1) = '\0';
#else /* SUPPORT_XPG4_MODE */
	for (i = 0; i <= _LastCategory; i++) {
		op = *(loc_name + i);
		while (*answer++ = *op++);
		*(answer - 1) = ' ';
	}
	*(answer - 1) = '\0';
#endif /* !SUPPORT_XPG4_MODE */
}

/*
*  FUNCTION: load_all_locales
*
*  DESCRIPTION:
*  loads all of the locales from the appropriate locale databases.
*
*  RETURNS:
*  a pointer to a locale handle containing an object for each of the
*  locale categories, or NULL on errors
*
*  This function sets up the locale name for each of the categories
*/
static _LC_locale_t *
load_all_locales(
	const char	*lc_all,
	const char	*locname,
	const char	*locpath,
	char	*tmp_locale_names[])
{
	int	i;
	char	*s[_LastCategory + 1];
	char	*q;
	char	*loc;
	int	diff_flag = 0;
	_LC_locale_t	*lp;
#ifndef SUPPORT_XPG4_MODE
	char	path[PATH_MAX + 1];
	struct stat64	stat_buf;
#endif /* !SUPPORT_XPG4_MODE */
	/* Only main thread can call setlocale() for setting locale. */
	/* In this restriction, load_all_locales can use static variable */
	/* to store the temporary locale information */
	static _LC_locale_t	*locale_local = (_LC_locale_t *)NULL;

	loc = expand_locale_name(locname, lc_all, (const int)LC_ALL);
	if (loc == (char *)NULL) {
		return ((_LC_locale_t *)NULL);	/* error */
	}
	/* special case for C locale */
#ifndef SUPPORT_XPG4_MODE
	if ((*loc == 'C' && *(loc + 1) == '\0') ||
		strcmp(loc, "/C/C/C/C/C/C") == 0) {
		for (i = 0; i <= _LastCategory; i++) {
			FREEP(tmp_locale_names[i]);
			tmp_locale_names[i] = (char *)C;
		}
		return (__C_locale);
	} else if ((strcmp(loc, POSIX) == 0) ||
		strcmp(loc, "/POSIX/POSIX/POSIX/POSIX/POSIX/POSIX") == 0) {
		for (i = 0; i <= _LastCategory; i++) {
			FREEP(tmp_locale_names[i]);
			tmp_locale_names[i] = (char *)POSIX;
		}
		return (__C_locale);
#else /* SUPPORT_XPG4_MODE */
	if ((*loc == 'C' && *(loc + 1) == '\0') ||
		strcmp(loc, "C C C C C C") == 0) {
		for (i = 0; i <= _LastCategory; i++) {
			FREEP(tmp_locale_names[i]);
			tmp_locale_names[i] = (char *)C;
		}
		return (__C_locale);
	} else if ((strcmp(loc, POSIX) == 0) ||
		strcmp(loc, "POSIX POSIX POSIX POSIX POSIX POSIX") == 0) {
		for (i = 0; i <= _LastCategory; i++) {
			FREEP(tmp_locale_names[i]);
			tmp_locale_names[i] = (char *)POSIX;
		}
		return (__C_locale);
#endif /* !SUPPORT_XPG4_MODE */
#if defined(PIC)
	} else {
		/* build locale piecemeal from each of the categories */

		for (i = 0; i <= _LastCategory; i++) {
			s[i] = expand_locale_name(loc, (const char *)NULL,
				(const int)i);
			if ((s[i] != (char *)C) &&
				(s[i] != (char *)POSIX)) {
				/* Setup saved locale string */
				if ((s[i] = strdup(s[i])) == NULL) {
					/* Couldn't dup the string */
					return ((_LC_locale_t *)NULL);
				}
			}
			if (i == 0) {
				q = s[0];
			} else if (!diff_flag) {
				if (strcmp(s[i], q) != 0) {
					diff_flag = 1;
				} else {
					q = s[i];
				}
			}
		}

		if (diff_flag == 0) {
			/* simple locale */
			LOAD_LOC(s[0]);
			for (i = 0; i <= _LastCategory; i++) {
				FREEP(tmp_locale_names[i]);
				tmp_locale_names[i] = s[i];
			}
			return (lp);
		} else {
			/* composite locale */
			if (!locale_local) {
				locale_local = (_LC_locale_t *)
					malloc(sizeof (_LC_locale_t));
				if (locale_local == NULL) {
					/* malloc failed */
					return ((_LC_locale_t *)NULL);
				}
			}
			LOAD_LOC(s[LC_COLLATE]);
			FREEP(tmp_locale_names[LC_COLLATE]);
			tmp_locale_names[LC_COLLATE] = s[LC_COLLATE];
			locale_local->lc_collate = lp->lc_collate;

			if (strcmp(s[LC_CTYPE], s[LC_COLLATE]) != 0) {
				LOAD_LOC(s[LC_CTYPE]);
			}
			FREEP(tmp_locale_names[LC_CTYPE]);
			tmp_locale_names[LC_CTYPE] = s[LC_CTYPE];
			locale_local->lc_ctype = lp->lc_ctype;
			locale_local->lc_charmap = lp->lc_charmap;

			if (strcmp(s[LC_MONETARY], s[LC_CTYPE]) != 0) {
				LOAD_LOC(s[LC_MONETARY]);
			}
			FREEP(tmp_locale_names[LC_MONETARY]);
			tmp_locale_names[LC_MONETARY] = s[LC_MONETARY];
			locale_local->lc_monetary = lp->lc_monetary;

			if (strcmp(s[LC_NUMERIC], s[LC_MONETARY]) != 0) {
				LOAD_LOC(s[LC_NUMERIC]);
			}
			FREEP(tmp_locale_names[LC_NUMERIC]);
			tmp_locale_names[LC_NUMERIC] = s[LC_NUMERIC];
			locale_local->lc_numeric = lp->lc_numeric;

			if (strcmp(s[LC_TIME], s[LC_NUMERIC]) != 0) {
				LOAD_LOC(s[LC_TIME]);
			}
			FREEP(tmp_locale_names[LC_TIME]);
			tmp_locale_names[LC_TIME] = s[LC_TIME];
			locale_local->lc_time = lp->lc_time;

			if (strcmp(s[LC_MESSAGES], s[LC_TIME]) != 0) {
				lp = load_locale(s[LC_MESSAGES], locpath);
				if (!lp) {
#ifndef SUPPORT_XPG4_MODE
	(void) strcpy(path, _DFLT_LOC_PATH);
	(void) strcat(path, s[LC_MESSAGES]);
	(void) strcat(path, "/LC_MESSAGES");
	if ((stat64(path, &stat_buf) != 0) || (!(stat_buf.st_mode & S_IFDIR))) {
		/* /usr/lib/locale/<loc>/LC_MESSAGES */
		/* directory does not exist */
		return ((_LC_locale_t *)NULL);
	} else {
		/* Evenif load for the locale object */
		/* failed setting of LC_MESSAGES */
		/* category must succeed as long as */
		/* /usr/lib/locale/<loc>/LC_MESSAGES */
		/* directory exists. */
		lp = __C_locale;
	}
#else /* SUPPORT_XPG4_MODE */
					return ((_LC_locale_t *)NULL);
#endif /* !SUPPORT_XPG4_MODE */
				}
			}
			FREEP(tmp_locale_names[LC_MESSAGES]);
			tmp_locale_names[LC_MESSAGES] = s[LC_MESSAGES];
			locale_local->lc_messages = lp->lc_messages;

			/* set up core part of locale container object */
			locale_local->core.init = lp->core.init;
			locale_local->core.destructor = lp->core.destructor;
			locale_local->core.user_api = lp->core.user_api;
			locale_local->core.native_api = lp->core.native_api;
			locale_local->core.data = lp->core.data;

			return (locale_local);
		}
	}
#else /* !PIC */
	} else {
		/* Only C and POSIX locales are valid in static library */
		return ((_LC_locale_t *)NULL);
	}
#endif /* PIC */
}


/*
 *  FUNCTION: expand_locale_name
 */
static char *
expand_locale_name(
	const char	*locname,
	const char	*lc_all,
	const int	category)
{
	static const char	*category_name[] = {
		"LC_CTYPE",   "LC_NUMERIC",  "LC_TIME",
		"LC_COLLATE", "LC_MONETARY", "LC_MESSAGES"};
	char	*env_name[_LastCategory + 1];
	char	*p, *q;
	char	*head, *end, *name;
	char	*lang_name;
	static char	*saved_for_query = (char *)NULL;
	static char	*saved_for_set = (char *)NULL;
	int	mark = 0;
	int	len = 0;
	int	composite = 0;
	int	i;

#ifndef SUPPORT_XPG4_MODE
	if (*locname) {
		if (*locname == '/') {	/* composite locale */
			if (category == LC_ALL) {
				return ((char *)locname);
			} else {
				for (name = (char *)locname + 1, i = 0;
					i <= category; i++) {
					head = name;
					end = strchr(head, '/');
					if (end == NULL) {
						break;
					}
					name = end + 1;
				}
				if (end == NULL) {
					end = head + strlen(head);
				}
				len = end - head;
				if (saved_for_query) {
					free(saved_for_query);
				}
				len++;
				if ((saved_for_query =
					(char *)malloc(sizeof (char) * len))
					== NULL) {
					return ((char *)NULL);
				}
				p = saved_for_query;
				q = head;
				while (--len != 0) {
					*p++ = *q++;
				}
				*p = '\0';
				return (saved_for_query);
			}
		} else {		/* simple locale */
			return ((char *)locname);
		}
	} else {
		/* LC_ALL overrides all other environment variables */
		if (*lc_all != '\0') {
			if ((*lc_all == 'C' && *(lc_all + 1)) ||
				strcmp(lc_all, "/C/C/C/C/C/C") == 0) {
				return ((char *)C);
			} else if ((strcmp(lc_all, POSIX) == 0) ||
				(strcmp(lc_all,
				"/POSIX/POSIX/POSIX/POSIX/POSIX/POSIX") == 0)) {
				return ((char *)POSIX);
			}
			return ((char *)lc_all);
		}
		/* check environment variable for specified category */
		lang_name = getenv("LANG");
		if ((lang_name == (char *)NULL) ||
			(*lang_name == '\0')) {
			lang_name = (char *)C;
		} else if ((*lang_name == 'C') && (*(lang_name + 1) == '\0')) {
			lang_name = (char *)C;
		} else if (strcmp(lang_name, POSIX) == 0) {
			lang_name = (char *)POSIX;
		}
		if (category == (const int)LC_ALL) {
			for (i = 0; i <= _LastCategory; i++) {
				env_name[i] = getenv(category_name[i]);
				if ((env_name[i] != NULL) &&
					(*env_name[i] != '\0')) {
					mark = 1;
					if ((*env_name[i] == 'C') &&
						(*(env_name[i] + 1) == '\0')) {
						env_name[i] = (char *)C;
					} else if (strcmp(env_name[i], POSIX)
						== 0) {
						env_name[i] = (char *)POSIX;
					}
				} else {
					env_name[i] = lang_name;
				}
			}
			if (mark == 0) {
				return (lang_name);
			} else {
				q = env_name[0];
				for (i = 0; i <= _LastCategory; i++) {
					if (composite == 0) {
						if (strcmp(q, env_name[i])
							!= 0) {
							composite = 1;
						} else {
							q = env_name[i];
						}
					}
					len += strlen(env_name[i]) + 1;
				}
				if (composite == 0) { /* simple locale */
					return (env_name[0]);
				} else {	/* composite locale */
					if (saved_for_set) {
						free(saved_for_set);
					}
					if ((saved_for_set = (char *)
						malloc(sizeof (char) *
						(len + 1))) == NULL) {
						return ((char *)NULL);
					}
					q = saved_for_set;
					*q++ = '/';
					for (i = 0; i <= _LastCategory; i++) {
						p = env_name[i];
						while (*q++ = *p++);
						*(q - 1) = '/';
					}
					*(q - 1) = '\0';
					return (saved_for_set);
				}
			}
		} else {
			p = getenv(category_name[category]);
			if ((p == NULL) || (*p == '\0')) {
				return (lang_name);
			} else {
				if ((*p == 'C') && (*(p + 1) == '\0')) {
					return ((char *)C);
				} else if (strcmp(p, POSIX) == 0) {
					return ((char *)POSIX);
				}
				return (p);
			}
		}
	}
#else /* SUPPORT_XPG4_MODE */
	if (*locname) {
		if (strchr(locname, ' ')) {	/* composite locale */
			if (category == LC_ALL) {
				return ((char *)locname);
			}
			for (name = (char *)locname, i = 0;
				i <= category; i++) {
				head = name;
				end = strchr(head, ' ');
				if (end == NULL) {
					break;
				}
				name = end + 1;
			}
			if (end == NULL) {
				end = head + strlen(head);
			}
			len = end - head;
			if (saved_for_query) {
				free(saved_for_query);
			}
			len++;
			if ((saved_for_query = (char *)
				malloc(sizeof (char) * len))
				== NULL) {
				return ((char *)NULL);
			}
			p = saved_for_query;
			q = head;
			while (--len != 0) {
				*p++ = *q++;
			}
			*p = '\0';
			return (saved_for_query);
		} else {		/* simple locale */
			return ((char *)locname);
		}
	} else {
		/* LC_ALL overrides all other environment variables */
		if (*lc_all != '\0') {
			if ((*lc_all == 'C' && *(lc_all + 1)) ||
				strcmp(lc_all, "C C C C C C") == 0) {
				return ((char *)C);
			} else if ((strcmp(lc_all, POSIX) == 0) ||
				(strcmp(lc_all,
				"POSIX POSIX POSIX POSIX POSIX POSIX") == 0)) {
				return ((char *)POSIX);
			}
			return ((char *)lc_all);
		}
		/* check environment variable for specified category */
		lang_name = getenv("LANG");
		if ((lang_name == (char *)NULL) ||
			(*lang_name == '\0')) {
			lang_name = (char *)C;
		} else if ((*lang_name == 'C') && (*(lang_name + 1) == '\0')) {
			lang_name = (char *)C;
		} else if (strcmp(lang_name, POSIX) == 0) {
			lang_name = (char *)POSIX;
		}
		if (category == (const int)LC_ALL) {
			for (i = 0; i <= _LastCategory; i++) {
				env_name[i] = getenv(category_name[i]);
				if ((env_name[i] != NULL) &&
					(*env_name[i] != '\0')) {
					mark = 1;
					if ((*env_name[i] == 'C') &&
						(*(env_name[i] + 1) == '\0')) {
						env_name[i] = (char *)C;
					} else if (strcmp(env_name[i], POSIX)
						== 0) {
						env_name[i] = (char *)POSIX;
					}
				} else {
					env_name[i] = lang_name;
				}
			}
			if (mark == 0) {
				return (lang_name);
			} else {
				q = env_name[0];
				for (i = 0; i <= _LastCategory; i++) {
					if (composite == 0) {
						if (strcmp(q, env_name[i])
							!= 0) {
							composite = 1;
						} else {
							q = env_name[i];
						}
					}
					len += strlen(env_name[i]) + 1;
				}
				if (composite == 0) { /* simple locale */
					return (env_name[0]);
				} else {	/* composite locale */
					if (saved_for_set) {
						free(saved_for_set);
					}
					if ((saved_for_set = (char *)
						malloc(sizeof (char) * len))
						== NULL) {
						return ((char *)NULL);
					}
					q = saved_for_set;
					for (i = 0; i <= _LastCategory; i++) {
						p = env_name[i];
						while (*q++ = *p++);
						*(q - 1) = ' ';
					}
					*(q - 1) = '\0';
					return (saved_for_set);
				}
			}
		} else {
			p = getenv(category_name[category]);
			if ((p == NULL) || (*p == '\0')) {
				return (lang_name);
			} else {
				if ((*p == 'C') && (*(p + 1) == '\0')) {
					return ((char *)C);
				} else if (strcmp(p, POSIX) == 0) {
					return ((char *)POSIX);
				}
				return (p);
			}
		}
	}
#endif /* !SUPPORT_XPG4_MODE */
}

/*
*  FUNCTION: load_locale
*
*  DESCRIPTION:
*  This function loads the locale specified by the locale name 'locale',
*  from the list of pathnames provided in 'locpath'.
*
*  RETURNS:
*  Pointer to locale database found in 'locpath'.
*/
static _LC_locale_t *
load_locale(
	const char	*name,
	const char	*locpath)
{
#if defined(PIC)
#ifdef SUPPORT_XPG4_MODE
	int	privileged;
	char	path[PATH_MAX + 1];
	int	i;
#endif /* SUPPORT_XPG4_MODE */
	_LC_object_t	*handle;
#endif /* PIC */

	/* Check for special case. */
#if defined(PIC)
	if (((name[0] == 'C') && (name[1] == '\0')) ||
		(strcmp(name, "POSIX") == 0)) {
		return (__C_locale);
	}
#else /* !PIC */
	/* Check for special case. */
	if (((name[0] == 'C') && (name[1] == '\0')) ||
		(strcmp(name, "POSIX") == 0)) {
		return (__C_locale);
	} else {
		/* Only C and POSIX locales are valid in static library */
		return (NULL);
	}
#endif /* PIC */

#if defined(PIC)
	/*
	 * check if this is a privileged program.
	 * If so, don't load untrusted code
	 */

#ifndef SUPPORT_XPG4_MODE

	handle = __lc_load(locpath, name, (void *(*)())0);
	if (handle) {
		return ((_LC_locale_t *)(handle));
	}

	/* No locale loaded! */
	return (NULL);

#else /* SUPPORT_XPG4_MODE */

	privileged = __ispriv();

	if (privileged) {
		if (strchr(name, '/')) {	/* Explicit path */
			return (NULL);	/* Not legal in privileged program */
		}

		if (locpath) {
			/* Ignore non-default paths */
			locpath = _DFLT_LOC_PATH;
		}
	}


	if (strchr(name, '/')) {
		return (__lc_load((const char *)NULL, name, (void *(*)())0));
	}

	if (!*locpath) {
		locpath = _DFLT_LOC_PATH;
	}

	while (*locpath) {

		for (i = 0; *locpath != ':' && *locpath != '\0' &&
			i <= PATH_MAX + 1; i++) {
			path[i] = *locpath++;
		}

		if (*locpath == ':') {
			locpath++;
		}

		if (i == 0) {
			path[i++] = '.';
		}
		/* append '/' */
		path[i++] = '/';
		path[i] = '\0';

		handle = __lc_load(path, name, (void *(*)())0);
		if (handle) {
			return ((_LC_locale_t *)(handle));
		}
	}

	/* No locale loaded! */
	return (NULL);

#endif /* !SUPPORT_XPG4_MODE */
#endif /* PIC */
}

#if defined(PIC)
#ifdef SUPPORT_XPG4_MODE
/*
 *  FUNCTION: __ispriv()
 *
 *  DESCIPTION:
 *  Determines if the current program is running in a secure context outside
 *  that of the user executing the program.
 */
static int
__ispriv(void)
{
	uid_t	euid, ruid;
	gid_t	egid, rgid;

	ruid = getuid();
	euid = geteuid();

	rgid = getgid();
	egid = getegid();

	/* check if privilege acquired somehow */
	if (euid != ruid || egid != rgid) {
		return (TRUE);
	} else {
		return (FALSE);
	}
}
#endif /* SUPPORT_XPG4_MODE */

/*
*  FUNCTION: __lc_load
*
*  DESCIPTION:
*  The function loads an object file from 'path'.  If the object file is
*  successfully loaded, the routine invokes the return address.  The
*  pointer returned is assumed to be an LC_LOAD_OBJECT_T.  If the routine
*  cannot load 'path', the function invokes the 'instantiate' method if
*  this method is not null.
*/
static void *
__lc_load(
	const char	*locpath,
	const char	*name,
	void	*(*instantiate)())
{

	char	path[PATH_MAX + 1];
#ifndef SUPPORT_XPG4_MODE
	char	suffix[12];			/* max length of ".so.??????" */
	int	len_suffix;
#endif
	char	*t;
	const char	*f;
	int	len_locpath, len_name, len;
	_LC_object_t	*p;

#ifndef SUPPORT_XPG4_MODE

	if ((locpath == NULL) || (name == NULL)) {
		return ((void *)NULL);
	}

	(void) sprintf(suffix, _LC_SHARED_OBJ_SUFFIX, _LC_VERSION_MAJOR);
	len_suffix = strlen(suffix);
	len_locpath = strlen(locpath);
	len_name = strlen(name);
	len = len_locpath + len_name + 1 + len_name + len_suffix;
		/* <locpath>/<name>/<name>.so.?????? */
	if (len > PATH_MAX) {
		return ((void *)NULL);	/* pathname is too long */
	}

	t = path;
	f = locpath;
	while (*t++ = *f++);
	t--;
	f = name;
	while (*t++ = *f++);
	*(t - 1) = '/';
	f = name;
	while (*t++ = *f++);
	t--;
	f = suffix;
	while (*t++ = *f++);

#else /* SUPPORT_XPG4_MODE */
	if (name == NULL) {
		return ((void *)NULL);
	}

	if (locpath == NULL) {
		len_name = strlen(name);
		if (len_name > PATH_MAX) {
			return ((void *)NULL); /* pathname is too long */
		}
		t = path;
		f = name;
		while (*t++ = *f++);
	} else {
		len_locpath = strlen(locpath);
		len_name = strlen(name);
		len = len_locpath + len_name;
		if (len > PATH_MAX) {
			return ((void *)NULL); /* pathname is too long */
		}
		t = path;
		f = locpath;
		while (*t++ = *f++);
		t--;
		f = name;
		while (*t++ = *f++);
	}
#endif /* ! SUPPORT_XPG4_MODE */

	/* load specified object */
	p = (_LC_object_t *)load(path);

/*
 *	If load() succeeded,
 *	execute the method pointer which was returned.
 *	return the return value of the method.
 *	else if an instantiation method was provided,
 *	execute the instantiation method, and
 *	return the return value of the instantiation method.
 *	else return NULL
*/
	if (p != NULL) {
		/* verify that what was returned was actually an object */
		if (((_LC_core_locale_t *)p)->hdr.magic == _LC_MAGIC) {
			/* versioning control */
			if (((_LC_core_locale_t *)p)->hdr.major_ver !=
				_LC_VERSION_MAJOR) {
				/* if major_ver doesn't match with */
				/* _LC_VERSION_MAJOR.  The following 6 lines */
				/* must be removed before the putback */
				/* to the gate. */
				(void) fprintf(stderr,
					"Major version number mismatch\n");
				(void) fprintf(stderr, "library: %d.%d\n",
					_LC_VERSION_MAJOR, _LC_VERSION_MINOR);
				(void) fprintf(stderr, "locale object: %d.%d\n",
				((_LC_core_locale_t *)p)->hdr.major_ver,
				((_LC_core_locale_t *)p)->hdr.minor_ver);
				return (NULL);
			}
			if (((_LC_core_locale_t *)p)->hdr.minor_ver >
				_LC_VERSION_MINOR) {
				/* if minor_ver larger than _LC_VERSION_MINOR */
				/* The following 6 lines must be removed */
				/* before the putback to the gate. */
				(void) fprintf(stderr,
					"Unsupported minor version number\n");
				(void) fprintf(stderr, "library: %d.%d\n",
					_LC_VERSION_MAJOR, _LC_VERSION_MINOR);
				(void) fprintf(stderr, "locale object: %d.%d\n",
				((_LC_core_locale_t *)p)->hdr.major_ver,
				((_LC_core_locale_t *)p)->hdr.minor_ver);
				return (NULL);
			}
			return (p);
		}
	}

/*
 *	At this point, either the object couldn't be loaded, or the
 *	instantiation method did not return a valid object.
 */

	/* invoke the instantiate method if it is not null */
	if (instantiate != NULL) {
		/* invoke the caller supplied instantiate method */
		p = (*instantiate)(path, p);
		return (p);
	}

	/* could not load specified object */
	return (NULL);
}

static void *
load(const char *filenameparm)
{
	void	*handle;
	_LC_object_t	*p;
	_LC_object_t	*(*init)();

	handle = dlopen(filenameparm, RTLD_LAZY);
	if (handle == NULL) {
#ifdef DEBUG
		printf("%s\n", dlerror());
#endif
		return (NULL);
	}
	init = (_LC_object_t *(*)())dlsym(handle, _LC_SYMBOL);
	if (init == (_LC_object_t *(*)())NULL) {
		return (NULL);
	}
	p = (*init)();
	return ((void *)p);
	/* handle should be taken care later for dlclose */
}

#endif /* PIC */
