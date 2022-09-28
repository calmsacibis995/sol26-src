/*
 * Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident "@(#)ns_cvt_pconf.c	1.4	96/05/02 SMI"

/*
 * This file contains routines necessary to convert a string buffer into
 * a printer object, and a printer object into a string buffer suitable
 * for writing to a printers.conf file or naming service.
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

#include <ns_cvt_pconf.h>


/*
 * FUNCTION:
 *	static void _add_addtribute(char *string, va_list ap)
 * INPUT:
 *	char *string - string to parse
 *	(va_arg) char *seperator - seperator for parsing
 *	(va_arg) ns_printer_t *printer - printer object to at to.
 * DESCRIPTION:
 *	Parses string, and added kvp to printer object
 */
static void
_add_attribute(char *string, va_list ap)
{
	char *seperators = va_arg(ap, char *);
	ns_printer_t *printer = va_arg(ap, ns_printer_t *);

	if ((string != NULL) && (seperators != NULL) && (printer != NULL)) {
		char **list;

		if (((list = strsplit(string, seperators)) != NULL) &&
		    (list[1] != NULL))
			ns_set_value_from_string(list[0], list[1], printer);
	}
}


/*
 * FUNCTION:
 *	ns_printer_t *_cvt_pconf_entry_to_printer(char *entry, char *source)
 * INPUT:
 *	char *entry - entry to parse
 *	char *source - source name service of entry
 * OUTPUT:
 *	ns_printer_t *(return) - a printer object from the parsed entry
 * DESCRIPTION:
 *	Splits the entry up in to printer name, aliases, and kvps.  returns
 *	a printer object.
 */
ns_printer_t *
_cvt_pconf_entry_to_printer(char *entry, char *source)
{
	char	*name = NULL,
		**namelist = NULL,
		**list = NULL;
	ns_printer_t *printer = NULL;

	if (entry == NULL)
		return (NULL);

	if ((list = strsplit(entry, ":\n")) == NULL)
		return (NULL);

	if ((namelist = strsplit(list[0], "|")) == NULL)
		return (NULL);

	if (*(++list) == NULL)  /* no attributes */
		return (NULL);

	name = namelist[0];
	if (*(++namelist) == NULL)
		namelist = NULL;

	if ((printer = (ns_printer_t *)ns_printer_create(name, namelist,
				source, NULL)) != NULL)
		list_iterate((void **)list, (VFUNC_T)_add_attribute, "=",
				printer);

	return (printer);
}


/*
 * FUNCTION:
 *	static void _append_pconf_attr_string(ns_kvp_t *kvp, va_list ap)
 * INPUT:
 *	ns_kvp_t *kvp - key/value pair to convert to a string.
 *	(va_arg) char *buf - buffer to append kvp into.
 * DESCRIPTION:
 *	converts the kvp to a string and appends it to the buffer.
 */
static void
_append_pconf_attr_string(ns_kvp_t *kvp, va_list ap)
{
	char *buf = va_arg(ap, char *);

	if ((kvp != NULL) && (kvp->key != NULL) && (kvp->value != NULL)) {
		strcat(buf, ":\\\n\t:");
		strcat(buf, kvp->key);
		strcat(buf, "=");
		strcat(buf, kvp->value);
	}
}


/*
 * FUNCTION:
 *	char *_cvt_printer_to_pconf_entry(ns_printer_t *printer)
 * INPUT:
 *	ns_printer_t *printer - printer to convert
 * OUTPUT:
 *	char * (return) - a printers.conf entry
 * DESCRIPTION:
 *	convert a printer object to an entry that can be placed in the
 *	printers.conf "db".  This is newly allocated space.
 */
char *
_cvt_printer_to_pconf_entry(ns_printer_t *printer)
{
	char entry[BUFSIZ],
	*namelist;

	memset(entry, 0, sizeof (entry));

	if ((namelist = (char *)ns_printer_name_list(printer)) == NULL)
		return (NULL);

	sprintf(entry, "%s", namelist);

	list_iterate((void **)printer->attributes,
			(VFUNC_T)_append_pconf_attr_string, entry);

	if (strcmp(entry, namelist) == 0) {
		free(namelist);
		return (NULL);
	} else
		free(namelist);

	strcat(entry, ":");

	return (strdup(entry));
}
