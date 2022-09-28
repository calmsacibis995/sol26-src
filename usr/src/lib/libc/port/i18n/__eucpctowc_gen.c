/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)__eucpctowc_gen.c 1.5	96/08/07  SMI"

#include <stdlib.h>
#include <widec.h>
#include <sys/localedef.h>

/*
 * Generic EUC version of EUC process code to Dense process code conversion.
 */
wchar_t
__eucpctowc_gen(_LC_charmap_t *hdl, wchar_t eucpc)
{
	_LC_euc_info_t	*eucinfo;
	wchar_t nwc;

	switch (eucpc & WCHAR_CSMASK) {
	case WCHAR_CS1:
		eucinfo = hdl->cm_eucinfo;
		nwc = eucpc + eucinfo->cs1_adjustment;
		if (nwc < eucinfo->cs1_base || nwc > eucinfo->dense_end)
			return ((wchar_t) WEOF);
		return (nwc);

	case WCHAR_CS2:
		eucinfo = hdl->cm_eucinfo;
		nwc = eucpc + eucinfo->cs2_adjustment;
		if (nwc < eucinfo->cs2_base || nwc >= eucinfo->cs3_base)
			return ((wchar_t) WEOF);
		return (nwc);

	case WCHAR_CS3:
		eucinfo = hdl->cm_eucinfo;
		nwc = eucpc + eucinfo->cs3_adjustment;
		if (nwc < eucinfo->cs3_base || nwc >= eucinfo->cs1_base)
			return ((wchar_t) WEOF);
		return (nwc);
	}

	/*
	 * It is assumed that the caller uses the _eucpctowc() macro
	 * to invoke this function. The macro checks ASCII characters
	 * and if the given character is an ASCII character, it
	 * doesn't call this function. This is why this function
	 * checks ASCII at the last.
	 */
	if (eucpc <= 0x9f)
		return (eucpc);
	return ((wchar_t) WEOF);
}
