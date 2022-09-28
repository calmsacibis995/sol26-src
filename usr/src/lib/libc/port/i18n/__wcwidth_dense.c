/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved
 */

#pragma	ident	"@(#)__wcwidth_dense.c	1.4	96/08/07 SMI"

#include <stdlib.h>
#include <ctype.h>
#include <sys/localedef.h>

int
__wcwidth_dense(_LC_charmap_t *hdl, wint_t wc)
{
	_LC_euc_info_t	*eucinfo;

	if (METHOD_NATIVE(__lc_ctype, iswctype)(__lc_ctype, wc, _ISPRINT))
		return (-1);

	if (wc < 128) {
		return (1);
	}

	if (wc < 256) {
		if (hdl->cm_mb_cur_max == 1) {
			return (1);
		} else {
			return (-1);
		}
	}

	eucinfo = hdl->cm_eucinfo;

	if (eucinfo->euc_bytelen2 && (wc < eucinfo->cs3_base)) {
		return (eucinfo->euc_scrlen2);
	} else if (eucinfo->euc_bytelen3 && (wc < eucinfo->cs1_base)) {
		return (eucinfo->euc_scrlen3);
	} else if (eucinfo->euc_bytelen1 && (wc <= eucinfo->dense_end)) {
		return (eucinfo->euc_scrlen1);
	} else {
		return (-1);
	}
}
