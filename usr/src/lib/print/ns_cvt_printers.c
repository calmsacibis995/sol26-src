/*
 * Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident "@(#)ns_cvt_printers.c 1.4	96/08/22 SMI"

/*
 * This file contains routines necessary to convert a string buffer into
 * a printer object, and a printer object into a string buffer suitable
 * for writing to a .printers file or naming service.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <stdarg.h>

#include <print/ns.h>
#include <print/list.h>
#include <print/misc.h>

#include "ns_cvt_printers.h"


/*
 * FUNCTION:
 *	ns_printer_t *_cvt_user_string_to_printer(char *entry, char *svc)
 * INPUT:
 *	char *entry - the string entry to convert
 *	char *svc - the source name service of the entry
 * OUTPUT:
 *	ns_printer_t *(return) - a printer object
 * DESCRIPTION:
 *	This function parses an entry and returns a printer object from the
 *	data.
 */
ns_printer_t *
_cvt_user_string_to_printer(char *entry, char *svc)
{
	ns_printer_t *printer = NULL;
	char	*tmp,
		**namelist,
		**list;

	if (entry != NULL) {
		tmp = strdup(entry);
		list = (char **)strsplit(tmp, "\t ");

		/* bugid 1256698 */
		if (list[0] == NULL || list[1] == NULL) {
			return (NULL);
		}

		namelist = (char **)strsplit(list[0], "|");

		printer = (ns_printer_t *)
			ns_printer_create(namelist[0],
				(namelist[1] ? &namelist[1] : NULL), svc, NULL);
		if (list[1][strlen(list[1])-1] == '\n')
			list[1][strlen(list[1])-1] = NULL;
		if (ns_printer_match_name(printer, NS_NAME_ALL) != 0) {
			ns_set_value_from_string(NS_KEY_USE, list[1], printer);
		} else {
			ns_set_value_from_string(NS_KEY_ALL, list[1], printer);
		}
	}

	return (printer);
}

/*
 * FUNCTION:
 *	char *_cvt_printer_to_user_string(ns_printer_t *printer)
 * INPUT:
 *	ns_printer_t *printer - the printer object to convert
 * OUTPUT:
 *	char *(return) - the entry for the printers name service
 * DESCRIPTION:
 *	This routine will create an entry from the printer object that is
 *	suitable for writing to the .printers file.
 */
char *
_cvt_printer_to_user_string(ns_printer_t *printer)
{
	char	buf[BUFSIZ],
		*attr,
		*namelist;

	if (printer == NULL)
		return (NULL);

	if (ns_printer_match_name(printer, NS_NAME_ALL) == 0) {
		attr = ns_get_value_string(NS_KEY_ALL, printer);
	} else
		attr = ns_get_value_string(NS_KEY_USE, printer);

	if (attr == NULL)
		return (NULL);

	namelist = (char *)ns_printer_name_list(printer);
	sprintf(buf, "%s\t%s", namelist, attr);
	free(namelist);

	return (strdup(buf));
}
