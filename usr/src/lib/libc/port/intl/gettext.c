/* Copyright (c) 1991 Sun Microsystems */
/* All Rights Reserved */

#ident  "@(#)gettext.c 1.25     96/10/01 SMI"

#ifdef __STDC__
#pragma weak bindtextdomain = _bindtextdomain
#pragma weak textdomain = _textdomain
#pragma weak gettext = _gettext
#pragma weak dgettext = _dgettext
#pragma weak dcgettext = _dcgettext
#else
#define	_textdomain textdomain
#define	_bindtextdomain bindtextdomain
#define	_dcgettext dcgettext
#define	_dgettext dgettext
#define	_gettext gettext
#ifndef const
#define	const
#endif /* const */
#endif /* __STDC__ */

#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <libintl.h>
#include <thread.h>
#include <synch.h>
#include "mtlibintl.h"
#include <limits.h>
#include <nl_types.h>
#include <unistd.h>
#include <signal.h>

#define	DEFAULT_DOMAIN		"messages"
#define	DEFAULT_BINDING		"/usr/lib/locale"
#define	BINDINGLISTDELIM	':'
#define	MAX_DOMAIN_LENGTH	(TEXTDOMAINMAX + 1) /* 256 + Null terminator */
#define	MAX_MSG			64

#define	LEAFINDICATOR		-99 /* must match with msgfmt.h */

/* ************************************************************* */
/*								*/
/*		+-------------------------------+		*/
/*		| (int) middle message id	|		*/
/*		+-------------------------------+		*/
/*		| (int) total # of messages	|		*/
/*		+-------------------------------+		*/
/*		| (int) total msgid length	|		*/
/*		+-------------------------------+		*/
/*		| (int) total msgstr length	|		*/
/*		+-------------------------------+		*/
/*		| (int) size of msg_struct size	|		*/
/*		+-------------------------------+		*/
/*		+-------------------------------+		*/
/*		| (int) less			|		*/
/*		+-------------------------------+		*/
/*		| (int) more			|		*/
/*		+-------------------------------+		*/
/*		| (int) msgid offset		|		*/
/*		+-------------------------------+		*/
/*		| (int) msgstr offseti		|		*/
/*		+-------------------------------+		*/
/*			................			*/
/*		+-------------------------------+		*/
/*		| (variable str) msgid		|		*/
/*		+-------------------------------+		*/
/*		| (variable str) msgid		|		*/
/*		+-------------------------------+		*/
/*			................			*/
/*		+-------------------------------+		*/
/*		| (variable str) msgstr		|		*/
/*		+-------------------------------+		*/
/*		| (variable str) msgstr		|		*/
/*		+-------------------------------+		*/
/*			................			*/
/*		+-------------------------------+		*/
/*		| (variable str) msgstr		|		*/
/*		+-------------------------------+		*/
/* ************************************************************ */

#define	SIGNAL_HOLD \
	(void) sighold(SIGINT); (void) sighold(SIGQUIT);\
	(void) sighold(SIGTERM)

#define	SIGNAL_RELEASE \
	(void) sigrelse(SIGTERM); (void) sigrelse(SIGQUIT);\
	(void) sigrelse(SIGINT)

#define	STRING_COPY_WITH_SLASH(s)	\
	p = (s);\
	while (*p) {\
		*q++ = *p++;\
	}\
	*q++ = '/'\

#define	STRING_COPY(s) \
	p = (s);\
	while (*p) {\
		*q++ = *p++;\
	}\

struct domain_binding {
	char	*domain;	/* domain name */
	char	*binding;	/* binding directory */
	struct domain_binding	*next;
};

struct msg_info {
	int	msg_mid;			/* middle message id */
	int	msg_count;			/* total # of messages */
	int	str_count_msgid;	/* total msgid length */
	int	str_count_msgstr;	/* total msgstr length */
	int	msg_struct_size;	/* size of msg_struct_size */
};

struct msg_struct {
	int	less;				/* index of left leaf */
	int	more;				/* index of right leaf */
	int	msgid_offset;		/* msgid offset */
	int msgstr_offset;		/* msgstr offset */
};

struct msg_head {
	char	*path;		/* name of message catalog */
	int	fd;				/* file descriptor */
	struct msg_info	*msg_file_info;	/* information of msg file */
	struct msg_struct	*msg_list; /* message list */
	char	*msg_ids;			/* actual message ids */
	char	*msg_strs;			/* actual message strs */
};

/*
 * The category_to_name array is used in build_path.
 * The orderring of these names must correspond to the order
 * of the LC_* categories in locale.h.
 * i.e., category_to_name[LC_CTYPE] = "LC_CTYPE".
 */
const char *category_to_name[] =  {   /* must correspont to locale.h order */
	"LC_CTYPE",
	"LC_NUMERIC",
	"LC_TIME",
	"LC_COLLATE",
	"LC_MONETARY",
	"LC_MESSAGES",
	""		/* LC_ALL */
	};

const char	*defaultbind = DEFAULT_BINDING;

/*
 * this structure is used for preserving nlspath templates before
 * passing them to bindtextdomain():
 */
struct nlstmp {
	char	pathname[PATH_MAX + 1];	/* the full pathname to file */
	struct nlstmp	*next;		/* link to the next entry */
};

struct nls_cache {
	char	*domain;	/* key: domain name */
	char	*locale;	/* key: locale name */
	char	*nlspath;	/* key: NLSPATH */
	int	category;		/* key: category */
	char	*ppaths;	/* value: expanded path */
	int	count;			/* value: number of components */
	struct nls_cache	*next;	/* link to the next entry */
};

char *_textdomain(const char *);
char *_bindtextdomain(const char *, const char *);
char *_dcgettext(const char *, const char *, const int);
char *_dgettext(const char *, const char *);
char * _gettext(const char *);

static char *_textdomain_u(const char *, char *);
static char *_bindtextdomain_u(const char *, const char *);
static char *dcgettext_u(const char *, const char *, const int);

static char *key_2_text(struct msg_head *, const char *);
static char *process_nlspath(const char *, const char *,
	const char *, const int, int *);
static void	allfree(int, char *, struct nls_cache *, struct nlstmp *);

#if defined(PIC)
#define	SETLOCALE(cat, loc)	setlocale(cat, loc)
#else	/* !PIC */
#define	SETLOCALE(cat, loc)	_static_setlocale(cat, loc)
static char	*_static_setlocale(int, const char *);
static const char	C[] = "C";
#endif	/* PIC */

#ifdef SUPPORT_PERCENT_X
static char *replace_nls_option(char *, const char *, char *, char *,
	char *, char *, char *, int);
#else
static char *replace_nls_option(char *, const char *, char *, char *,
	char *, char *, char *);
#endif /* SUPPORT_PERCENT_X */

static struct domain_binding *firstbind = 0;
static struct nls_cache	*nls_cache = 0;

#ifdef _REENTRANT
static mutex_t gt_lock = DEFAULTMUTEX;
#endif _REENTRANT

#ifdef DEBUG
#define	ASSERT_LOCK(x)		assert(MUTEX_HELD(x))
#else
#define	ASSERT_LOCK(x)
#endif /* DEBUG */

#if !defined(PIC)
/* _static_setlocale supports only the query */
static char *
_static_setlocale(int category, const char *locale)
{
	char	*name;
	char	*setting;

	if (locale) {	/* trying to set the locale */
		return ((char *)NULL);
		/* NOTREACHED */
	}

	if (category == LC_ALL) {
		/* _static_setlocale always returns "C" for LC_ALL */
		return ((char *)C);
		/* NOTREACHED */
	}
	if ((category >= 0) && (category <= _LastCategory)) {
		name = (char *)category_to_name[category];
	} else {
		return ((char *)NULL);
		/* NOTREACHED */
	}

	setting = getenv(name);
	if (setting) {
		return (setting);
	} else {
		return ((char *)C);
		/* NOTREACHED */
	}
}
#endif	/* !PIC */

char *
_bindtextdomain(
	const char	*domain,
	const char	*binding)
{
	char	*res;

	mutex_lock(&gt_lock);
	res = _bindtextdomain_u(domain, binding);
	mutex_unlock(&gt_lock);
	return (res);
}

static char *
_bindtextdomain_u(
	const char	*domain,
	const char	*binding)
{
	struct domain_binding	*bind, *prev;

#ifdef DEBUG
	printf("_bindtestdomain_u(%s, %s)\n", (domain ? domain : ""),
		(binding ? binding : ""));
#endif

	ASSERT_LOCK(&gt_lock);

	/*
	 * If domain is a NULL pointer, no change will occur regardless
	 * of binding value. Just return NULL.
	 */
	if (!domain) {
		return ((char *)NULL);
	}

	/*
	 * Global Binding is not supported any more.
	 * Just return NULL if domain is NULL string.
	 */
	if (*domain == '\0') {
		return ((char *)NULL);
	}

	/* linear search for binding, rebind if found, add if not */
	bind = firstbind;	/* firstbind is a static pointer */
	prev = 0;	/* Two pointers needed for pointer operations */

	while (bind) {
		if (strcmp(domain, bind->domain) == 0) {
			/*
			 * Domain found.
			 * If binding is NULL, then Query
			 */
			if (!binding) {
				return (bind->binding);
			}

			/* protect from signals */
			SIGNAL_HOLD;
			/* replace existing binding with new binding */
			if (bind->binding) {
				free(bind->binding);
			}
			if ((bind->binding = strdup(binding)) == NULL) {
				SIGNAL_RELEASE;
				return (NULL);
			}
			SIGNAL_RELEASE;
#ifdef DEBUG
			printlist();
#endif
			return (bind->binding);
		}
		prev = bind;
		bind = bind->next;
	} /* while (bind) */

	/* domain has not been found in the list at this point */
	if (binding) {
		/*
		 * domain is not found, but binding is not NULL.
		 * Then add a new node to the end of linked list.
		 */

		if ((bind = (struct domain_binding *)
			malloc(sizeof (struct domain_binding)))
			== (struct domain_binding *)NULL) {
			return (NULL);
		}
		if ((bind->domain = strdup(domain)) == NULL) {
			free(bind);
			return (NULL);
		}
		if ((bind->binding = strdup(binding)) == NULL) {
			free(bind->domain);
			free(bind);
			return (NULL);
		}
		bind->next = 0;

		/* protect from signals */
		SIGNAL_HOLD;
		if (prev) {
			/* reached the end of list */
			prev->next = bind;
		} else {
			/* list was empty */
			firstbind = bind;
		}
		SIGNAL_RELEASE;

#ifdef DEBUG
		printlist();
#endif
		return (bind->binding);
	} else {
		/* Query of domain which is not found in the list */
		return ((char *)defaultbind);
	} /* if (binding) */

	/* Must not reach here */

} /* _bindtextdomain_u */


/*
 * textdomain() sets or queries the name of the current domain of
 * the active LC_MESSAGES locale category.
 */

static char	current_domain[MAX_DOMAIN_LENGTH + 1] = DEFAULT_DOMAIN;

char *
_textdomain(const char *domain)
{
	char	*res;

	_mutex_lock(&gt_lock);
	res = _textdomain_u(domain, current_domain);
	_mutex_unlock(&gt_lock);
	return (res);
}


static char *
_textdomain_u(
	const char	*domain,
	char	*result)
{

#ifdef DEBUG
	printf("_textdomain_u(%s, %s)\n", (domain?domain:""),
	    (result?result:""));
#endif

	ASSERT_LOCK(&gt_lock);

	/* Query is performed for NULL domain pointer */
	if (domain == NULL) {
		if (current_domain != result)
			(void) strcpy(result, current_domain);
		return (result);
	}

	/* check for error. */
	/* I think TEXTDOMAINMAX should used instead of MAX_DOMAIN_LENGTH */
	if (strlen(domain) > (unsigned int) MAX_DOMAIN_LENGTH) {
		return (NULL);
	}

	/*
	 * Calling textdomain() with a null domain string sets
	 * the domain to the default domain.
	 * If non-null string is passwd, current domain is changed
	 * to the new domain.
	 */

	/* actually this if clause should be protected from signals */
	if (*domain == '\0') {
		(void) strcpy(current_domain, DEFAULT_DOMAIN);
	} else {
		(void) strcpy(current_domain, domain);
	}

	if (current_domain != result)
		(void) strcpy(result, current_domain);
	return (result);
} /* textdomain */


/*
 * gettext() is a pass-thru to dcgettext() with a NULL pointer passed
 * for domain and LC_MESSAGES passed for category.
 */
char *
_gettext(const char *msg_id)
{
	char	*return_str;

	mutex_lock(&gt_lock);
	return_str = dcgettext_u(NULL, msg_id, LC_MESSAGES);
	mutex_unlock(&gt_lock);
	return (return_str);
}


/*
 * In dcgettext() call, domain is valid only for this call.
 */
char *
_dgettext(
	const char	*domain,
	const char	*msg_id)
{
	char	*res;

	mutex_lock(&gt_lock);
	res = dcgettext_u(domain, msg_id, LC_MESSAGES);
	mutex_unlock(&gt_lock);
	return (res);
}

char *
_dcgettext(
	const char	*domain,
	const char	*msg_id,
	const int	category)
{
	char	*res;

	mutex_lock(&gt_lock);
	res = dcgettext_u(domain, msg_id, category);
	mutex_unlock(&gt_lock);
	return (res);
}



static char *
dcgettext_u(
	const char	*domain,
	const char	*msg_id,
	const int	category)
{
	char	msgfile[MAXPATHLEN]; 	/* 1024 */
	char	binding[MAXPATHLEN + 1]; /* 1024 + 1 */
	char	mydomain[MAX_DOMAIN_LENGTH + 1]; /* 256 + 1 + 1 */
	char	*cur_binding;	/* points to current binding in list */
	char	*bptr, *cur_locale, *cur_domain, *result, *nlspath;
	char	*p, *q;

	caddr_t	addr;

	int	msg_inc;
	int	msg_count;
	int	path_found;
	int first_free;
	int	errno_save = errno;
	int	fd = -1;
	int	pnp = 0;	/* # nls paths to process	*/
	int	nlsp = 0;	/* if NLSPATH is defined, nlsp is set to 1 */
	int	info_size;
	int	struct_size;

	struct stat64	statbuf;
	struct msg_head	*p_msg;

	static int	last_entry_seen = -1;	/* try this one first */
	static struct msg_head	*msg_head[MAX_MSG];

#ifdef DEBUG
	printf("dcgettext_u(%s, %s, %d)\n",
	    (domain?domain:""), msg_id, category);
#endif

	ASSERT_LOCK(&gt_lock);

	/* category may be LC_MESSAGES or LC_TIME */
	/* cur_locale contains the value of 'category' */
	cur_locale = SETLOCALE(category, NULL);

	nlspath = getenv("NLSPATH"); /* get the content of NLSPATH */
	if ((nlspath == (char *)NULL) || (*nlspath == '\0')) {
		/* no NLSPATH is defined in the environ */
		if ((*cur_locale == 'C') && (*(cur_locale + 1) == '\0')) {
			/* If C locale, */
			/* return the original msgid immediately. */
			errno = errno_save;
			return ((char *)msg_id);
		}
	} else {
		nlsp = 1;	/* NLSPATH is set */
	}

	mydomain[0] = '\0';

	/*
	 * Query the current domain if domain argument is NULL pointer
	 */
	if (domain == NULL) {
		cur_domain = _textdomain_u(NULL, mydomain);
	} else if (strlen(domain) > (unsigned int)MAX_DOMAIN_LENGTH) {
		/* if domain is invalid, return msg_id */
		errno = errno_save;
		return ((char *)msg_id);
	} else if (*domain == '\0') {
		cur_domain = DEFAULT_DOMAIN;
	} else {
		cur_domain = (char *)domain;
	}

	/*
	 * Spec1170 requires that we use NLSPATH if it's defined, to
	 * override any system default variables.  If NLSPATH is not
	 * defined or if a message catalog is not found in any of the
	 * components (bindings) specified by NLSPATH, dcgettext_u() will
	 * search for the message catalog in either a) the binding path set
	 * by any previous application calls to bindtextdomain() or
	 * b) the default binding path (/usr/lib/locale).  Save the original
	 * binding path so that we can search it if the message catalog
	 * is not found via NLSPATH.  The original binding is restored before
	 * returning from this routine because the gettext routines should
	 * not change the binding set by the application.  This allows
	 * bindtextdomain() to be called once for all gettext() calls in the
	 * application.
	 */

	if (nlsp) {
		if ((cur_binding = process_nlspath(cur_domain,
			cur_locale, (const char *)nlspath, category, &pnp))
			== (char *)NULL) {
			if (pnp == -1) {
				/* error occurred */
				/* errno isn't restored */
				return ((char *)msg_id);
			} else {
				/* could not find bindings in NLSPATH */
				if ((cur_binding =
				    _bindtextdomain_u(cur_domain, NULL))
					== (char *)NULL) {
					/* if the current binding is NULL, */
					/* return msg_id */
					/* errno isn't restored */
					return ((char *)msg_id);
				}
			}
		}
	} else {
		/* if the current binding is NULL, */
		/* return msg_id */
		if ((cur_binding = _bindtextdomain_u(cur_domain, NULL))
			== (char *)NULL) {
			/* errno isn't restored */
			return ((char *)msg_id);
		}
	}

	/*
	 * The following while loop is entered whether or not
	 * NLSPATH is set, ie: cur_binding points to either NLSPATH
	 * or textdomain path.
	 * binding is the form of "bind1:bind2:bind3:"
	 */
	while (*cur_binding) {
		/* skip empty binding */
		while (*cur_binding == ':') {
			cur_binding++;
		}

		memset(binding, 0, sizeof (binding));
		bptr = binding;

		/* get binding */

		/*
		 * the previous version of this loop would leave
		 * cur_binding inc'd past the NULL character.
		 */
		while (*cur_binding != ':') {
			if ((*bptr = *cur_binding) == '\0') {
				break;
			}
			bptr++;
			cur_binding++;
		}

		if (binding[0] == '\0') {
			if ((cur_binding = _bindtextdomain_u(cur_domain, NULL))
				== (char *)NULL) {
				/* errno isn't restored */
				return ((char *)msg_id);
			}
			continue;
		}

		if (pnp-- > 0) { /* if we're looking at nlspath */
			/*
			 * process_nlspath() will already have built
			 * up an absolute pathname. So here, we're
			 * dealing with a binding that was in NLSPATH.
			 */
			(void) strcpy(msgfile, binding);
		} else {
			/* (nlsp == 0) || ((nlsp == 1) && (pnp <= 0)) */

			/*
			 * Build textdomain path (regular locale), ie:
			 * <binding>/<locale>/<category_name>/<domain>.mo
			 * where <binding> could be a) set by a previous
			 * call by the application to bindtextdomain(), or
			 * b) the default binding "/usr/lib/locale".
			 * <domain> could be a) set by a previous call by the
			 * application to textdomain(), or b) the default
			 * domain "messages".
			 */

			q = msgfile;
			STRING_COPY_WITH_SLASH(binding);
			STRING_COPY_WITH_SLASH(cur_locale);
			STRING_COPY_WITH_SLASH((char *)category_to_name
			    [category]);
			STRING_COPY(cur_domain);
			STRING_COPY(".mo");
			*q++ = '\0';
		}

		/*
		 * At this point, msgfile contains full path for
		 * domain.
		 * Look up cache entry first. If cache misses,
		 * then search domain look-up table.
		 */
		path_found = 0;
		if ((last_entry_seen >= 0) &&
			(strcmp(msgfile, msg_head[last_entry_seen]->path)
			== 0)) {
			/* if msgfile is the same as the previous  */
			/* message file */
			path_found = 1;
			msg_inc = last_entry_seen;
		} else {
			/* search entries in the cache table */
			msg_inc = 0;
			first_free = 0;
			while (msg_head[msg_inc]) {
				if (strcmp(msgfile, msg_head[msg_inc]->path)
					== 0) {
					path_found = 1;
					break;
				} else {
					msg_inc++;
					first_free++;
				}
			}
		}
		/*
		 * Even if msgfile was found in the table,
		 * It is not guaranteed to be a valid file.
		 * To be a valid file, fd must not be -1 and
		 * mmaped address (mess_file_info) must have
		 * valid contents.
		 */
		if (path_found) {
			last_entry_seen = msg_inc;
			if ((msg_head[msg_inc]->fd != -1) &&
				(msg_head[msg_inc]->msg_file_info !=
				(struct msg_info *)-1)) {
				result = key_2_text(msg_head[msg_inc], msg_id);
				errno = errno_save;
				return (result);
			} else {
				/* invalid file */
				continue;
			}
		}
		/*
		 * Been though entire table and not found.
		 * Open a new entry if there is space.
		 */
		if (msg_inc == MAX_MSG) {
			/* not found, no more space */
			errno = errno_save;
			return ((char *)msg_id);
		}
		if (first_free == MAX_MSG) {
			/* no more space */
			errno = errno_save;
			return ((char *)msg_id);
		}

		/*
		 * There is an available entry in the table, so make
		 * a message_so for it and put it in the table,
		 * return msg_id if message file isn't opened -or-
		 * isn't mmap'd correctly
		 */

		if ((p_msg = (struct msg_head *)
			malloc(sizeof (struct msg_head)))
			== (struct msg_head *)NULL) {
			/* errno isn't restored */
			return ((char *)msg_id);
		}

		fd = open(msgfile, O_RDONLY);
		p_msg->fd = fd;
		if ((p_msg->path = strdup(msgfile))
			== (char *)NULL) {
			free(p_msg);
			/* errno isn't restored */
			return ((char *)msg_id);
		}

		if (fd == -1) {
			SIGNAL_HOLD;
			msg_head[first_free] = p_msg;
			SIGNAL_RELEASE;
			first_free++;
			continue;
		}

		if ((fstat64(fd, &statbuf) == -1) ||
		    (statbuf.st_size > LONG_MAX)) {
			p_msg->fd = -1;
			SIGNAL_HOLD;
			msg_head[first_free] = p_msg;
			SIGNAL_RELEASE;
			first_free++;
			continue;
		}

		addr = mmap(0, statbuf.st_size, PROT_READ,
					MAP_SHARED, fd, 0);
		(void) close(fd);

		p_msg->msg_file_info =
			(struct msg_info *)addr;

		if (addr == (caddr_t)-1) {
			p_msg->fd = -1;
			SIGNAL_HOLD;
			msg_head[first_free] = p_msg;
			SIGNAL_RELEASE;
			first_free++;
			continue;
		}
		msg_count = p_msg->msg_file_info->msg_count;

		info_size = sizeof (struct msg_info);
		struct_size = sizeof (struct msg_struct) * msg_count;

		p_msg->msg_list = (struct msg_struct *)
			(addr + info_size);

		p_msg->msg_ids = (char *)
			(addr + info_size + struct_size);

		p_msg->msg_strs = (char *)
			(addr + info_size + struct_size +
			p_msg->msg_file_info->str_count_msgid);

		SIGNAL_HOLD;
		msg_head[first_free] = p_msg;
		last_entry_seen = first_free;
		SIGNAL_RELEASE;

		result = key_2_text(p_msg, msg_id);
		errno = errno_save;
		return (result);

	} /* while cur_binding */

	errno = errno_save;
	return ((char *)msg_id);
} /* dcgettext_u */


/*
 * key_2_text() translates msd_id into target string.
 */
static char *
key_2_text(
	struct msg_head	*messages,
	const char	*key_string)
{
	int	check;
	int	val;
	char	*msg_id_str;
	struct msg_struct	*check_msg_list;

	ASSERT_LOCK(&gt_lock);

	check = messages->msg_file_info->msg_mid;

	for (;;) {
		check_msg_list = messages->msg_list + check;
		msg_id_str = messages->msg_ids +
			check_msg_list->msgid_offset;
		/*
		 * To maintain the compatibility with Zeus mo file,
		 * msg_id's are stored in descending order.
		 * If the ascending order is desired, change "msgfmt.c"
		 * and switch msg_id_str and key_string in the following
		 * strcmp() statement.
		 */
		val = strcmp(msg_id_str, key_string);
		if (val < 0) {
			if (check_msg_list->less == LEAFINDICATOR) {
				return ((char *)key_string);
			} else {
				check = check_msg_list->less;
			}
		} else if (val > 0) {
			if (check_msg_list->more == LEAFINDICATOR) {
				return ((char *)key_string);
			} else {
				check = check_msg_list->more;
			}
		} else {
			return (messages->msg_strs
				+ check_msg_list->msgstr_offset);
		}
	}

} /* key_2_string */


#ifdef DEBUG
printlist()
{
	struct domain_binding	*ppp;

	fprintf(stderr, "===Printing default list and regural list\n");
	fprintf(stderr, "   Default domain=<%s>, binding=<%s>\n",
		defaultbind->domain, defaultbind->binding);

	ppp = firstbind;
	while (ppp) {
		fprintf(stderr, "   domain=<%s>, binding=<%s>\n",
			ppp->domain, ppp->binding);
		ppp = ppp->next;
	}
}
#endif


/*
 * process_nlspath(): process the NLSPATH environment variable.
 *	output: # of paths in NLSPATH.
 *	description:
 *		this routine looks at NLSPATH in the environment,
 *		and will try to build up the binding list based
 *		on the settings of NLSPATH.
 */


static char *
process_nlspath(
	const char	*current_domain,
	const char	*current_locale,
	const char	*nlspath,
	const int category,
	int	*pnp)
{
	char 	*s;			/* generic string ptr		*/
	char	*locale;		/* what setlocale() tells us	*/
	char	*territory;		/* our current territory element */
	char	*codeset;		/* our current codeset element	*/
	char	*lang;			/* our current language element	*/
	char	*s1;			/* for handling territory	*/
	char	*s2;			/* for handling codeset		*/
	char	pathname[PATH_MAX + 1];	/* the full pathname to the file */
	char	*ppaths;		/* ptr to all of the templates	*/
	struct nlstmp	*nlstmp, *pnlstmp, *qnlstmp;
	char	mydomain[MAX_DOMAIN_LENGTH + 1]; /* 256 + 1 + 1 */
	int	errno_save = errno;	/* preserve errno		*/
	int	nlscount = 0;
	int	nlspath_len, domain_len, locale_len;
	int	ppaths_len = 0;
	struct nls_cache	*p, *q;

#ifdef DEBUG
	printf("process_nlspath(%s, %s, %s, %d)\n", current_domain,
	    current_locale, nlspath, category);
#endif

	ASSERT_LOCK(&gt_lock);

	if (nls_cache) {		/* cache is not empty */
		p = nls_cache;
		while (p) {
			if ((p->category == category) &&
				(strcmp(p->domain, current_domain) == 0) &&
				(strcmp(p->locale, current_locale) == 0) &&
				(strcmp(p->nlspath, nlspath) == 0)) {
				/* entry found in the cache */
				*pnp = p->count; /* set # of components */
				return (p->ppaths);	/* return binding */
			}
			/* forward the link */
			q = p;
			p = p->next;
		}
		/* entry not found in the cache */
		/* then create a new entry */
		if ((p = (struct nls_cache *)malloc(sizeof (struct nls_cache)))
			== (struct nls_cache *)NULL) {
			*pnp = -1;		/* error */
			return ((char *)NULL);
		}
	} else {
		/* cache is empty */
		/* then create a new entry */
		if ((p = (struct nls_cache *)
			malloc(sizeof (struct nls_cache)))
			== (struct nls_cache *)NULL) {
			*pnp = -1;		/* error */
			return ((char *)NULL);
		}
	}

	nlspath_len = strlen(nlspath) + 1;
	locale_len = strlen(current_locale) + 1;
	domain_len = strlen(current_domain) + 1;

	lang = (char *)NULL;
	territory = (char *)NULL;
	codeset = (char *)NULL;

	/* query the current locale */
#ifdef SUPPORT_PERCENT_X
	/* actually, category could be LC_MESSAGES or LC_TIME */
	locale = SETLOCALE(category, NULL);
#else
	/* So far, %L, %I, %t, and %c are made from LC_MESSAGES */
	locale = SETLOCALE(LC_MESSAGES, NULL);
#endif

	if (locale) {
		lang = s = strdup(locale);
		if (lang == (char *)NULL) {
			*pnp = -1;		/* error */
			return ((char *)NULL);
		}
		s1 = s2 = (char *)NULL;
		while (s && *s) {
			if (*s == '_') {
				s1 = s;
				*s1++ = '\0';
			} else if (*s == '.') {
				s2 = s;
				*s2++ = '\0';
			}
			s++;
		}
		territory = s1;
		codeset = s2;
	}

	mydomain[0] = '\0';

	/* create entry for the current domain/locale/NLSPATH */

	nlstmp = 0;

	/*
	 * now that we have the name (domain), we first look through NLSPATH,
	 * in an attempt to get the locale. A locale may be completely
	 * specified as "language_territory.codeset". NLSPATH consists
	 * of templates separated by ":" characters. The following are
	 * the substitution values within NLSPATH:
	 *	%N = DEFAULT_DOMAIN
	 *	%L = The value of the LC_MESSAGES category.
	 *	%I = The language element from the LC_MESSAGES category.
	 *	%t = The territory element from the LC_MESSAGES category.
	 *	%c = The codeset element from the LC_MESSAGES category.
	 *	%% = A single character.
	 * if we find one of these characters, we will carry out the
	 * appropriate substitution.
	 */
	s = (char *)nlspath;		/* s has a content of NLSPATH */
	while (*s) {				/* march through NLSPATH */
		memset((void *)pathname, 0, sizeof (pathname));
		if (*s == ':') {
			/*
			 * this loop only occurs if we have to replace
			 * ":" by "name". replace_nls_option() below
			 * will handle the subsequent ":"'s.
			 */
			if ((pnlstmp = (struct nlstmp *)
				malloc(sizeof (struct nlstmp)))
				== (struct nlstmp *)NULL) {
				allfree(3, lang, p, nlstmp);
				*pnp = -1;	/* error */
				return ((char *)NULL);
			}

			(void) strcpy(pnlstmp->pathname, current_domain);
			ppaths_len += domain_len + 1;

			pnlstmp->next = 0;

			if (!nlstmp) {
				nlstmp = pnlstmp;
				qnlstmp = pnlstmp;
				pnlstmp = 0;
			} else {
				qnlstmp->next = pnlstmp;
				qnlstmp = pnlstmp;
			}

			nlscount++;

			++s;
			continue;
		}
		/* replace Substitution field */
#ifdef SUPPORT_PERCENT_X
		s = replace_nls_option(s, current_domain, pathname,
			locale, lang, territory, codeset, category);
#else
		s = replace_nls_option(s, current_domain, pathname,
			locale, lang, territory, codeset);
#endif

		/* if we've found a valid file: */
		if (pathname[0] != '\0') {
			/* add template to end of chain of pathnames: */
			if ((pnlstmp = (struct nlstmp *)
				malloc(sizeof (struct nlstmp)))
				== (struct nlstmp *)NULL) {
				allfree(3, lang, p, nlstmp);
				*pnp = -1;	/* error */
				return ((char *)NULL);
			}

			(void) strcpy(pnlstmp->pathname, pathname);
			ppaths_len += strlen(pathname) + 1;

			pnlstmp->next = 0;

			if (!nlstmp) {
				nlstmp = pnlstmp;
				qnlstmp = pnlstmp;
				pnlstmp = 0;
			} else {
				qnlstmp->next = pnlstmp;
				qnlstmp = pnlstmp;
			}

			nlscount++;
		}
		if (*s) {
			++s;
		}
	}
	/*
	 * now that we've handled the pathname templates, concatenate them
	 * all into the form "template1:template2:..." for _bindtextdomain_u()
	 */

	if (ppaths_len > 0) {
		if ((ppaths = (char *)malloc(ppaths_len + 1))
			== (char *)NULL) {
			allfree(3, lang, p, nlstmp);
			*pnp = -1;	/* error */
			return ((char *)NULL);
		}
		*ppaths = '\0';
	}
	/*
	 * extract the path templates (fifo), and concatenate them
	 * all into a ":" separated string for _bindtextdomain_u()
	 */
	pnlstmp = nlstmp;
	while (pnlstmp) {
		(void) strcat(ppaths, pnlstmp->pathname);
		(void) strcat(ppaths, ":");
		qnlstmp = pnlstmp->next;
		free(pnlstmp);
		pnlstmp = qnlstmp;
	}

	if ((p->domain = (char *)malloc(domain_len))
		== (char *)NULL) {
		allfree(4, lang, p, (struct nlstmp *)NULL);
		*pnp = -1;	/* error */
		return ((char *)NULL);
	} else {
		(void) strcpy(p->domain, current_domain);
	}
	if ((p->locale = (char *)malloc(locale_len))
		== (char *)NULL) {
		allfree(2, lang, p, (struct nlstmp *)NULL);
		*pnp = -1;	/* error */
		return ((char *)NULL);
	} else {
		(void) strcpy(p->locale, current_locale);
	}
	if ((p->nlspath = (char *)malloc(nlspath_len))
		== (char *)NULL) {
		allfree(1, lang, p, (struct nlstmp *)NULL);
		*pnp = -1;	/* error */
		return ((char *)NULL);
	} else {
		(void) strcpy(p->nlspath, nlspath);
	}
	p->category = category;
	p->ppaths = ppaths;
	p->count = nlscount;
	p->next = 0;

	/* protect from signals */
	SIGNAL_HOLD;
	if (nls_cache) {
		q->next = p;
	} else {
		nls_cache = p;
	}
	SIGNAL_RELEASE;

	*pnp = nlscount;
	allfree(5, lang, (struct nls_cache *)NULL,
		(struct nlstmp *)NULL);
	return (ppaths);
}


/*
 * This routine will replace substitution parameters in NLSPATH
 * with appropiate values.
 */
#ifdef SUPPORT_PERCENT_X
static char *
replace_nls_option(
	char	*s,
	const char	*name,
	char	*pathname,
	char	*locale,
	char	*lang,
	char	*territory,
	char	*codeset,
	int	category)
#else
static char *
replace_nls_option(
	char	*s,
	const char	*name,
	char	*pathname,
	char	*locale,
	char	*lang,
	char	*territory,
	char	*codeset)
#endif
{
	char	*t, *u;
	char	*limit;

	t = pathname;
	limit = pathname + PATH_MAX; /* PATH_MAX == 1024 */

	while (*s && *s != ':') {
		if (t < limit) {
			/*
			 * %% is considered a single % character (XPG).
			 * %L : LC_MESSAGES (XPG4) LANG(XPG3)
			 * %l : The language element from the current locale.
			 *	(XPG3, XPG4)
			 */
			if (*s != '%')
				*t++ = *s;
			else if (*++s == 'N') {
				if (name) {
					u = (char *)name;
					while (*u && (t < limit))
						*t++ = *u++;
				}
			} else if (*s == 'L') {
				if (locale) {
					u = locale;
					while (*u && (t < limit))
						*t++ = *u++;
				}
			} else if (*s == 'l') {
				if (lang) {
					u = lang;
					while (*u && (*u != '_') &&
						(t < limit))
						*t++ = *u++;
				}
			} else if (*s == 't') {
				if (territory) {
					u = territory;
					while (*u && (*u != '.') &&
						(t < limit))
						*t++ = *u++;
				}
			} else if (*s == 'c') {
				if (codeset) {
					u = codeset;
					while (*u && (t < limit))
						*t++ = *u++;
				}
#ifdef SUPPORT_PERCENT_X
			} else if (*s == 'X') {
				u = (char *)category_to_name[category];
				while (*u && (t < limit)) {
					*t++ = *u++;
				}
#endif
			} else {
				if (t < limit)
					*t++ = *s;
			}
		}
		++s;
	}
	*t = '\0';
	return (s);
}

static void
allfree(
	int	status,
	char	*lang,
	struct nls_cache	*tmp_cache,
	struct nlstmp	*nlstmp)
{
	struct nlstmp	*pnlstmp, *qnlstmp;

	switch (status) {
	case 1:
		free(tmp_cache->locale);
	case 2:
		free(tmp_cache->domain);
	case 3:
		pnlstmp = nlstmp;
		while (pnlstmp) {
			qnlstmp = pnlstmp->next;
			free(pnlstmp);
			pnlstmp = qnlstmp;
		}
	case 4:
		free(tmp_cache);
	case 5:
		if (lang) {
			free(lang);
		}
	default:
		break;
	}
}
