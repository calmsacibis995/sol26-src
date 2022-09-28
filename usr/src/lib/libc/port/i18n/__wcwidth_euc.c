/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)__wcwidth_euc.c 1.2	96/07/02  SMI"

#include <sys/localedef.h>
#include <widec.h>

int
__wcwidth_euc(_LC_charmap_t * hdl, wchar_t wc)
{
	if (wc) {
		if (iswprint(wc) == 0)
			return (-1);
		switch (wcsetno(wc)) {
		case	0:
			return (1);
		case	1:
			return (hdl->cm_eucinfo->euc_scrlen1);
		case	2:
			return (hdl->cm_eucinfo->euc_scrlen2);
		case	3:
			return (hdl->cm_eucinfo->euc_scrlen3);
		}
	}
	return (0);
}
