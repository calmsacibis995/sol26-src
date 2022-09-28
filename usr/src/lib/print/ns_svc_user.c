/*
 * Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident "@(#)ns_svc_user.c	1.5	96/04/22 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <stdarg.h>

#include <print/ns.h>

#include "ns_rw_file.h"
#include "ns_cvt_printers.h"


static char *
user_file()
{
	static char path[BUFSIZ];
	char *home = getenv("HOME");

	sprintf(path, "%s/.printers", (home ? home : "/etc"));
	return (path);
}


ns_printer_t *
user_get_name(const char *name)
{
	return (_file_get_name(user_file(), name, _cvt_user_string_to_printer,
				NS_SVC_USER));
}


ns_printer_t **
user_get_list()
{
	return (_file_get_list(user_file(), _cvt_user_string_to_printer,
				NS_SVC_USER));
}


int
user_put_printer(const ns_printer_t *printer)
{
	return (_file_put_printer(user_file(), printer,
			_cvt_user_string_to_printer, NS_SVC_USER,
			_cvt_printer_to_user_string));
}
