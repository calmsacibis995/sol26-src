/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)__wcswidth_euc.c 1.2	96/07/02  SMI"

#include <sys/localedef.h>
#include <widec.h>

int
__wcswidth_euc(_LC_charmap_t * hdl, const wchar_t *pwcs, size_t n)
{
	int	col = 0;

	while (*pwcs && n) {
		if (iswprint(*pwcs) == 0)
			return (-1);
		switch (wcsetno(*pwcs)) {
		case	0:
			col += 1;
			break;
		case	1:
			col += hdl->cm_eucinfo->euc_scrlen1;
			break;
		case	2:
			col += hdl->cm_eucinfo->euc_scrlen2;
			break;
		case	3:
			col += hdl->cm_eucinfo->euc_scrlen3;
			break;
		}
		pwcs++;
		n--;
	}
	return (col);
}
