/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved
 */

#pragma	ident	"@(#)__wcswidth_dense.c	1.4	96/07/02 SMI"

#include <stdlib.h>
#include <ctype.h>
#include <sys/localedef.h>

int
__wcswidth_dense(_LC_charmap_t *hdl, const wchar_t *wc, size_t n)
{
	int	col = 0;
	int	l;
	int	cur_max;
	int	base_cs3, base_cs1, end_dense;
	int	scrlen1, scrlen2, scrlen3;


	cur_max = hdl->cm_mb_cur_max;
	scrlen1 = hdl->cm_eucinfo->euc_scrlen1;
	scrlen2 = hdl->cm_eucinfo->euc_scrlen2;
	scrlen3 = hdl->cm_eucinfo->euc_scrlen3;
	end_dense = hdl->cm_eucinfo->dense_end;
	base_cs1 = hdl->cm_eucinfo->cs1_base;
	base_cs3 = hdl->cm_eucinfo->cs3_base;

	while (*wc && n) {
		if (METHOD_NATIVE(__lc_ctype, iswctype)
		    (__lc_ctype, *wc, _ISPRINT))
			return (-1);
		if (*wc < 128) {
			col++;
		} else if (*wc < 256) {
			if (cur_max == 1) {
				col++;
			} else {
				return (-1);
			}
		} else if (*wc < base_cs3) {
			col += scrlen2;
		} else if (*wc < base_cs1) {
			col += scrlen3;
		} else if (*wc < end_dense) {
			col += scrlen1;
		} else {
			return (-1);
		}
		wc++;
		n--;
	}
	return (col);
}
