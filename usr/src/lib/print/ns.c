/*
 * Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident  "@(#)ns.c	1.9	96/04/22 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <stdarg.h>


#include <print/ns.h>
#include <print/list.h>
#include <print/misc.h>


extern char **ns_order();

/*
 * FUNCTION:
 *	void ns_printer_destroy(ns_printer_t *printer)
 * INPUT:
 *	ns_printer_t *printer - a pointer to the printer "object" to destroy
 * DESCRIPTION:
 *	This function will free all of the memory associated with a printer
 *	object.  It does this by walking the structure ad freeing everything
 *	underneath it, with the exception of the object source field.  This
 *	field is not filled in with newly allocated space when it is
 *	generated
 */
void
ns_printer_destroy(ns_printer_t *printer)
{
	if (printer != NULL) {
		if (printer->attributes != NULL) {	/* attributes */
			extern void ns_kvp_destroy(ns_kvp_t *);

			list_iterate((void **)printer->attributes,
				(VFUNC_T)ns_kvp_destroy);
			free(printer->attributes);
		}
		if (printer->aliases != NULL) {		/* aliases */
			free(printer->aliases);
		}
		if (printer->name != NULL)		/* primary name */
			free(printer->name);
	}
}


/*
 * FUNCTION:
 *	ns_printer_t **ns_printer_get_list()
 * OUTPUT:
 *	ns_printer_t ** (return value) - an array of pointers to printer
 *					 objects.
 * DESCRIPTION:
 *	This function will return a list of all printer objects found in every
 *	configuration interface.
 */
ns_printer_t **
ns_printer_get_list(const char *ns)
{
	char **list;
	ns_printer_t **printer_list = NULL;
	char func[32];
	ns_printer_t **(*fpt)();


	if (ns != NULL) {
		sprintf(func, "%s_get_list", ns);
		if ((fpt = (ns_printer_t **(*)())dynamic_function(ns, func))
		    != NULL)
			printer_list = (fpt)();
	} else {
		for (list = ns_order(); list != NULL && *list != NULL;
				list++) {
			ns_printer_t **plist = NULL;

			sprintf(func, "%s_get_list", *list);
			if ((fpt = (ns_printer_t **(*)())dynamic_function(*list,
							func)) == NULL)
				continue;

			if ((plist = (fpt)()) == NULL)
				continue;

			printer_list = (ns_printer_t **)
				list_concatenate((void **)printer_list,
						(void **)plist);
		}
	}

	return (printer_list);
}


/*
 * FUNCTION:
 *	ns_printer_t *ns_printer_get_name(const char *name)
 * INPUTS:
 *	const char *name - name of printer object to locate
 * DESCRIPTION:
 *	This function looks through each "name service" until if finds the
 *	requested printer or exausts the name services.
 */
ns_printer_t *
ns_printer_get_name(const char *name, const char *ns)
{
	char **list;
	ns_printer_t *printer = NULL;
	char func[32];
	ns_printer_t *(*fpt)();


	if (name == NULL)
		return (NULL);

	if ((printer = (ns_printer_t *)posix_name(name)) != NULL)
		return (printer);

	if (ns != NULL) {
		sprintf(func, "%s_get_name", ns);
		if ((fpt = (ns_printer_t *(*)())dynamic_function(ns, func))
		    != NULL)
			printer = (fpt)(name);
	} else {
		for (list = ns_order(); list != NULL && *list != NULL;
				list++) {
			sprintf(func, "%s_get_name", *list);
			if ((fpt = (ns_printer_t *(*)())dynamic_function(*list,
						func)) == NULL)
				continue;

			if ((printer = (fpt)(name)) != NULL)
				break;
		}
	}
	return (printer);
}


/*
 * FUNCTION:
 *	int ns_printer_put(const ns_printer_t *printer)
 * INPUT:
 *	const ns_printer_t *printer - a printer object
 * DESCRIPTION:
 *	This function attempts to put the data in the printer object back
 *	to the "name service" specified in the source field of the object.
 */
int
ns_printer_put(const ns_printer_t *printer)
{
	char func[32];
	int (*fpt)();

	if ((printer == NULL) || (printer->source == NULL))
		return (-1);

	sprintf(func, "%s_put_printer", printer->source);
	if ((fpt = (int (*)())dynamic_function(printer->source, func)) != NULL)
		return ((*fpt)(printer));

	return (-1);
}
