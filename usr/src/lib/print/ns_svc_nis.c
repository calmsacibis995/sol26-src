/*
 * Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident "@(#)ns_svc_nis.c	1.4	96/04/22 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdarg.h>

#include <print/ns.h>

#include "ns_rw_nis.h"
#include "ns_cvt_pconf.h"

static char *_map = "printers.conf.byname";


ns_printer_t *
nis_get_name(const char *name)
{
	return (_nis_get_name(_map, name, _cvt_pconf_entry_to_printer,
				NS_SVC_NIS));
}


ns_printer_t **
nis_get_list()
{
	return (_nis_get_list(_map, _cvt_pconf_entry_to_printer,
				NS_SVC_NIS));
}


int
nis_put_printer(const ns_printer_t *printer)
{
	return (_nis_put_printer(_map, printer,
			_cvt_pconf_entry_to_printer, NS_SVC_NIS,
			_cvt_printer_to_pconf_entry));
}
