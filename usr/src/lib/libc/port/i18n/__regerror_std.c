/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#ident	"@(#)__regerror_std.c	1.6	96/07/02 SMI"

/*
static char sccsid[] = "@(#)43	1.2.1.1  "
"src/bos/usr/ccs/lib/libc/__regerror_std.c, bos, bos410 5/25/92 14:04:19";
*/
/*
 * COMPONENT_NAME: (LIBCPAT) Standard C Library Pattern Functions
 *
 * FUNCTIONS: regerror
 *
 * ORIGINS: 27
 *
 * This module contains IBM CONFIDENTIAL code. -- (IBM
 * Confidential Restricted when combined with the aggregated
 * modules for this product)
 * OBJECT CODE ONLY SOURCE MATERIALS
 *
 * (C) COPYRIGHT International Business Machines Corp. 1991, 1992
 * All Rights Reserved
 * Licensed Materials - Property of IBM
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 */

#include <nl_types.h>
#include <sys/types.h>
#include <sys/localedef.h>
#include <stdlib.h>
#include <regex.h>
#include <string.h>

#if !defined(TEXT_DOMAIN)	/* Should be defined thru -D flag. */
#define	TEXT_DOMAIN	"SYS-TEST"
#endif

#define	m_strmsg(str)		_libc_gettext(str)

char * _dgettext(const char *, const char *);
#define	_libc_gettext(msg_id)   _dgettext(TEXT_DOMAIN, msg_id)
#undef	m_textstr
#define	m_textstr(a, b, c)	(b)
#define	m_textmsg(id, str, cls)	_libc_gettext(str)
/*
 * Error messages for regerror
 */
static char *regerrors[] = {
m_textstr(646, "success", "I"),				/* REG_OK */
m_textstr(647, "failed to match", "E"),			/* REG_NOMATCH */
m_textstr(648, "invalid collation element", "E"),	/* REG_ECOLLATE */
m_textstr(649, "trailing \\ in pattern", "E"),		/* REG_EESCAPE */
m_textstr(650, "newline found before end of pattern", "E"), /* REG_ENEWLINE */
"",							/* REG_ENSUB (OBS) */
m_textstr(652, "number in \\[0-9] invalid", "E"),	/* REG_ESUBREG */
m_textstr(653, "[ ] imbalance or syntax error", "E"),	/* REG_EBRACK */
m_textstr(654, "( ) or \\( \\) imbalance", "E"),	/* REG_EPAREN */
m_textstr(655, "{ } or \\{ \\} imbalance", "E"),	/* REG_EBRACE */
m_textstr(656, "invalid endpoint in range", "E"),	/* REG_ERANGE */
m_textstr(133, "out of memory", "E"),			/* REG_ESPACE */
m_textstr(5031, "?, *, +, or { } not preceded by valid regular expression",
	"E"),		/* REG_BADRPT */
m_textstr(658, "invalid character class type", "E"),	/* REG_ECTYPE */
m_textstr(659, "syntax error", "E"),			/* REG_BADPAT */
m_textstr(660, "contents of { } or \\{ \\} invalid", "E"), /* REG_BADBR */
m_textstr(661, "internal error", "E"),			/* REG_EFATAL */
m_textstr(3366, "invalid multibyte character", "E"),	/* REG_ECHAR */
m_textstr(3641, "backtrack stack overflow: expression generates too many "
	"alternatives", "E"),	/* REG_STACK */
m_textstr(3642, "^ anchor not at beginning of pattern", "E"), /* REG_EBOL */
m_textstr(3643, "$ anchor not at end of pattern", "E"),  /* REG_EEOL */
};

/*
 * Map error number to text message.
 * preg is supplied to allow an error message with perhaps pieces of
 * the offending regular expression embeded in it.
 * preg is permitted to be zero, results still have to be returned.
 * In this implementation, preg is currently unused.
 * The string is returned into errbuf, which is errbuf_size bytes
 * long, and is possibly truncated.  If errbuf_size is zero, the string
 * is not returned.  The length of the error message is returned.
 */

/* ******************************************************************** */
/* __regerror_std() - Get Text for RE Error Message			*/
/* ******************************************************************** */

size_t
__regerror_std(_LC_collate_t *hdl, int errcode, const regex_t *preg,
		char *errbuf, size_t errbuf_size)
{
	char *s;
	int	erroff;		/* index into local error text table */
#define	MAXERROR	200	/* max error index   */
#define	REG_OK		0	/* success (non-standard) */
#define	REG__LAST	20	/* first unused code */
/*
 * verify error code is a valid RE error
 * return error status if invalid error code
 */
	erroff = errcode - REG_NOMATCH;
	if (erroff < 0 || erroff >= MAXERROR)
		return (0);

	if (errcode < REG_OK || errcode >= REG__LAST)
		s = m_textmsg(662, "unknown regex error", "E");
	else
		s = m_strmsg(regerrors[errcode]);
	if (errbuf_size != 0) {
		strncpy(errbuf, s, errbuf_size);
		errbuf[errbuf_size-1] = '\0';
	}
	return (strlen(s) + 1);
}
