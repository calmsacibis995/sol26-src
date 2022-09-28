/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)get_lcbind.c 1.3	96/07/02  SMI"

#include <sys/localedef.h>

char *
_lc_get_ctype_flag_name(_LC_ctype_t *hdl, _LC_bind_tag_t tag,
			_LC_bind_value_t value)
{

	int	i;

	for (i = 0; i < hdl->nbinds; i++) {
		if ((tag == hdl->bindtab[i].bindtag) &&
		    (value == hdl->bindtab[i].bindvalue)) {
			return (hdl->bindtab[i].bindname);
		}
	}

	return (char *)(NULL);

}
