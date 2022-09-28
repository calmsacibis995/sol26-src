/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)_trwctype.c 1.4	96/07/15  SMI"

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T		*/
/*	All Rights Reserved					*/
/*								*/
/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#include <stdlib.h>
#include <wctype.h>
#include <string.h>
#include <sys/localedef.h>

extern char * _lc_get_ctype_flag_name(_LC_ctype_t *, _LC_bind_tag_t,
			_LC_bind_value_t);

wchar_t
__trwctype_std(_LC_ctype_t *hdl, wchar_t wc, int mask)
{
	char		*name;
	wctrans_t	transtype;

	if (wc == WEOF || wc < 0x9f) {
		return (wc);
	} else {

		name = _lc_get_ctype_flag_name(hdl,
				(_LC_bind_tag_t)_LC_TAG_TRANS,
				(_LC_bind_value_t)mask);
		if (name == NULL)
			return (wc);

		transtype = wctrans(name);
		if (transtype == (wctrans_t)0)
			return (wc);

		return (towctrans(wc, transtype));
	}

}

wchar_t
_trwctype(wchar_t wc, int mask)
{
	return (METHOD(__lc_ctype, _trwctype)(__lc_ctype, wc, mask));
}
