/*
 * Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident "@(#)ns_svc_etc.c 1.5	96/04/22 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdarg.h>

#include <print/ns.h>

#include "ns_rw_file.h"
#include "ns_cvt_pconf.h"

static char *_filename = "/etc/printers.conf";


ns_printer_t *
etc_get_name(const char *name)
{
	return (_file_get_name(_filename, name, _cvt_pconf_entry_to_printer,
				NS_SVC_ETC));
}


ns_printer_t **
etc_get_list()
{
	return (_file_get_list(_filename, _cvt_pconf_entry_to_printer,
				NS_SVC_ETC));
}


int
etc_put_printer(const ns_printer_t *printer)
{
	return (_file_put_printer(_filename, printer,
			_cvt_pconf_entry_to_printer, NS_SVC_ETC,
			_cvt_printer_to_pconf_entry));
}
