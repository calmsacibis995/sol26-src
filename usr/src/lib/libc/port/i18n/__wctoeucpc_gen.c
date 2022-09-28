/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)__wctoeucpc_gen.c 1.4	96/07/01  SMI"

#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <sys/localedef.h>
#include <widec.h>

/*
 * Generic EUC version of Dense process code to EUC process code conversion
 */
wchar_t
__wctoeucpc_gen(_LC_charmap_t *hdl, wchar_t densepc)
{
	/*
	 * Currently, Solaris wchar_t is defined as signed long. To
	 * simplify the range checking, convert wchar_t to unsigned int.
	 */
	unsigned int wc = (unsigned int) densepc;
	_LC_euc_info_t *eucinfo;

	/*
	 * if wc is ASCII or C1 control, return it without conversion.
	 */
	if (wc <= 0x9f)
		return (wc);

	/*
	 * If wc is in the single-byte range and the codeset is
	 * single-byte, convert to CS1 process code and return it. If
	 * the codeset is multi-byte, return an error. (0x80-0xFF is
	 * not used in the dense encoding for multi-byte in the
	 * generic case.)
	 */
	if (wc < 0x100) {
		if (hdl->cm_mb_cur_max == 1)
			return ((wc & 0x7f) | WCHAR_CS1);
		return ((wchar_t) WEOF);
	}

	eucinfo = hdl->cm_eucinfo;

	/*
	 * CS2
	 */
	if (wc < eucinfo->cs3_base)
		return (wc - eucinfo->cs2_adjustment);

	/*
	 * CS3
	 */
	if (wc < eucinfo->cs1_base)
		return (wc - eucinfo->cs3_adjustment);

	/*
	 * CS1
	 */
	if (wc <= eucinfo->dense_end)
		return (wc - eucinfo->cs1_adjustment);

	/*
	 * Out of the range
	 */
	return ((wchar_t) WEOF);
}
