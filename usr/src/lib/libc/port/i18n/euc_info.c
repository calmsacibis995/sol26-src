/*
 * Copyright (c) 1995, 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)euc_info.c 1.1	96/08/20  SMI"

#include <sys/localedef.h>

/*
 * These two functions are project private functions of CSI project.
 * They should be used cautiously when dealing with CSIed code.
 *
 */

int
_is_euc_fc()
{
	return (__lc_charmap->cm_fc_type == _FC_EUC);
}

int
_is_euc_pc()
{
	return (__lc_charmap->cm_pc_type == _PC_EUC);
}
