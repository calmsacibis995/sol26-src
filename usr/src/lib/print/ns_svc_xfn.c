/*
 * Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident "@(#)ns_svc_xfn.c 1.3	96/04/22 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdarg.h>
#include <xfn/xfn.h>

#include <print/ns.h>

#include "ns_rw_xfn.h"
#include "ns_cvt_xfn.h"


ns_printer_t *
xfn_get_name(const char *name)
{
	return (_xfn_get_name(name, _cvt_xfn_entry_to_printer, NS_SVC_XFN));
}


ns_printer_t **
xfn_get_list()
{
	return (_xfn_get_list(_cvt_xfn_entry_to_printer, NS_SVC_XFN));
}


int
xfn_put_printer(const ns_printer_t *printer)
{
	return (_xfn_put_printer(printer, _cvt_xfn_entry_to_printer,
				NS_SVC_XFN, _cvt_printer_to_xfn_entry));
}
