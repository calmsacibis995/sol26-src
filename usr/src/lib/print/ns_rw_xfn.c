/*
 * Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident  "@(#)ns_rw_xfn.c 1.14     96/10/28 SMI"


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <rpc/rpc.h>
#include <xfn/xfn.h>
#include <string.h>
#include <stdarg.h>

#include <print/ns.h>
#include <print/list.h>
#include <print/misc.h>

#include "ns_rw_xfn.h"


#define	UPDATE_CMD	"/usr/bin/fncreate_printer"
#define	DESTROY_CMD	"/usr/sbin/fndestroy"
#define	UNBIND_CMD	"/usr/bin/fnunbind"
#define	CONTEXT		"thisorgunit/service/printer"

/*
 * Ideally, XFN would have some way to determine quickly and efficiently
 * that a given context has children, but that doesn't appear to be the
 * case.  In the files/nis worlds, fn_ctx_handle_from_ref() will return
 * NULL pretty much every time, because there can be no children, but
 * in NIS+, you always get a context handle, soe you have to look for
 * bindings underneath, which is reasonably expensive.
 */

static FN_composite_name_t	*empty_cname = NULL;

static FN_ctx_t *
_xfn_initial_context()
{
	static FN_ctx_t *ctx = NULL;
	FN_status_t *status;
	FN_string_t *string;

	if (ctx != NULL)
		return (ctx);

	if ((status = fn_status_create()) == NULL)
		return (NULL);

	seteuid(getuid());
	ctx = fn_ctx_handle_from_initial(0, status);
	setuid(0);

	if ((string = fn_string_from_str((unsigned char *)"")) == NULL)
		return (NULL);

	empty_cname = fn_composite_name_from_string(string);
	fn_string_destroy(string);
	if (empty_cname == NULL)
		return (NULL);


	/* should check it first */
	fn_status_destroy(status);

	return (ctx);
}


static char *_default_prefix_list[] = {
	"thisuser/service/printer",
	"myorgunit/service/printer",
	"thisorgunit/service/printer",
	NULL
};

static char **
_xfn_prefix_list()
{

	char	*path,
		**list = NULL;

	if ((path = getenv("PRINTER_CONTEXT_PATH")) != NULL)
		list = strsplit(strdup(path), ":");

	list = (char **)list_concatenate((void **)list,
				(void **)_default_prefix_list);

	return (list);
}


static ns_printer_t *
_xfn_find_printer(const char *name, FN_status_t *status, FN_ctx_t *ctx,
	ns_printer_t *(*conv)(char *, FN_ref_t *, char *), char *svc)
{
	ns_printer_t *printer = NULL;
	FN_bindinglist_t *bindings;
	FN_string_t *string;
	FN_composite_name_t *cname;
	FN_ref_t *ref, *link_ref;

	if ((string = fn_string_from_str((const unsigned char *)name)) == NULL)
		return (NULL);

	cname = fn_composite_name_from_string(string);
	fn_string_destroy(string);
	if (cname == NULL)
		return (NULL);

	ref = fn_ctx_lookup(ctx, cname, status);
	fn_composite_name_destroy(cname);
	if (ref != NULL) {
		printer = (conv)((char *)name, ref, svc);
		fn_ref_destroy(ref);
		if (printer != NULL)
			return (printer);
	}

	/* not found, walk the walk */
	if ((bindings = fn_ctx_list_bindings(ctx, empty_cname, status)) == NULL)
		return (NULL);

	while ((string = fn_bindinglist_next(bindings, &ref, status)) != NULL) {
		if (printer == NULL) {
			FN_ctx_t *child;

			/*
			 * The reference could be a link. If so, do
			 * a fn_ctx_lookup() once again
			 */
			if (fn_ref_is_link(ref)) {
				cname = fn_composite_name_from_string(string);
				link_ref = fn_ctx_lookup(ctx, cname, status);
				fn_composite_name_destroy(cname);
				if (fn_status_code(status) != FN_SUCCESS)
					continue;
				fn_ref_destroy(ref);
				ref = link_ref;
			}

			if ((child = fn_ctx_handle_from_ref(ref, 0, status))
			    != NULL) {
				printer = _xfn_find_printer(name, status,
						child, conv, svc);
				fn_ctx_handle_destroy(child);
			}
		}
		fn_string_destroy(string);
		fn_ref_destroy(ref);
	}
	fn_bindinglist_destroy(bindings);
	return (printer);
}


ns_printer_t *
_xfn_get_name(const char *name,
		ns_printer_t *(*conv)(char *, FN_ref_t *, char *), char *svc)
{
	ns_printer_t *printer = NULL;
	FN_ctx_t *ictx;
	FN_status_t *status;
	char **prefix;

	if ((ictx = _xfn_initial_context()) == NULL)
		return (NULL);

	if ((status = fn_status_create()) == NULL)
		return (NULL);

	/* if it has a "/" in it, try the top first */
	if (strchr(name, '/') != NULL) {
		FN_string_t *string;
		FN_composite_name_t *cname;
		FN_ref_t *ref;

		if ((string = fn_string_from_str((const unsigned char *)name))
		    == NULL)
			return (NULL);

		cname = fn_composite_name_from_string(string);
		fn_string_destroy(string);
		if (cname == NULL)
			return (NULL);

		ref = fn_ctx_lookup(ictx, cname, status);
		fn_composite_name_destroy(cname);
		if (ref != NULL) {
			printer = (conv)((char *)name, ref, svc);
			fn_ref_destroy(ref);
			if (printer != NULL)
				return (printer);
		}
	}

	for (prefix = _xfn_prefix_list(); prefix != NULL && *prefix != NULL;
	    prefix++) {
		FN_string_t *string;
		FN_composite_name_t *cname;
		FN_ref_t *ref;
		FN_ctx_t *ctx;

		if ((string = fn_string_from_str(
				(const unsigned char *)*prefix)) == NULL)
			continue;

		cname = fn_composite_name_from_string(string);
		fn_string_destroy(string);
		if (cname == NULL)
			continue;

		ref = fn_ctx_lookup(ictx, cname, status);
		fn_composite_name_destroy(cname);
		if (ref == NULL)
			continue;

		ctx = fn_ctx_handle_from_ref(ref, 0, status);
		fn_ref_destroy(ref);

		printer = _xfn_find_printer(name, status, ctx, conv, svc);
		fn_ctx_handle_destroy(ctx);
		if (printer != NULL)
			break;
	}

	fn_status_destroy(status);
	return (printer);
}


static ns_printer_t **
_xfn_get_sublist(FN_status_t *status, FN_ctx_t *ctx,
	ns_printer_t *(*conv)(char *, FN_ref_t *, char *), char *svc)
{
	ns_printer_t **printers = NULL;
	FN_bindinglist_t *bindings;
	FN_string_t *string;
	FN_ref_t *ref, *link_ref;
	FN_composite_name_t *cname;

	if ((bindings = fn_ctx_list_bindings(ctx, empty_cname, status)) == NULL)
		return (NULL);

	while ((string = fn_bindinglist_next(bindings, &ref, status)) != NULL) {
		unsigned int stat_val;
		const unsigned char *name;

		if ((name = fn_string_str(string, &stat_val)) != NULL) {
			ns_printer_t *printer;
			FN_ctx_t *child;

			/* Check if the reference is a link reference */
			if (fn_ref_is_link(ref)) {
				cname = fn_composite_name_from_string(string);
				link_ref = fn_ctx_lookup(ctx, cname, status);
				fn_composite_name_destroy(cname);
				if (fn_status_code(status) != FN_SUCCESS)
					continue;
				fn_ref_destroy(ref);
				ref = link_ref;
			}

			/* walk the walk */
			if ((child = fn_ctx_handle_from_ref(ref, 0, status))
			    != NULL) {
				printers = (ns_printer_t **)list_concatenate(
					(void **)printers,
					(void *)_xfn_get_sublist(status, child,
							conv, svc));
				fn_ctx_handle_destroy(child);
			}

			/* Add it to the printers list */
			if ((strcmp((char *)name, NS_NAME_DEFAULT) != 0) &&
			    ((printer = (conv)((char *)name, ref, svc))
					!= NULL))
				printers = (ns_printer_t **)list_append(
					(void **)printers, (void *)printer);
		}

		fn_string_destroy(string);
		fn_ref_destroy(ref);
	}

	fn_bindinglist_destroy(bindings);
	return (printers);
}


ns_printer_t **
_xfn_get_list(ns_printer_t *(*conv)(char *, FN_ref_t *, char *), char *svc)
{
	FN_ctx_t *ictx;
	FN_status_t *status;
	ns_printer_t **printers = NULL;
	char **prefix;

	if ((ictx = _xfn_initial_context()) == NULL)
		return (NULL);

	if ((status = fn_status_create()) == NULL)
		return (NULL);

	for (prefix = _xfn_prefix_list(); prefix != NULL && *prefix != NULL;
	    prefix++) {
		FN_string_t *string;
		FN_composite_name_t *cname;
		FN_ref_t *ref;
		FN_ctx_t *ctx;

		if ((string = fn_string_from_str(
				(const unsigned char *)*prefix)) == NULL)
			continue;

		cname = fn_composite_name_from_string(string);
		fn_string_destroy(string);
		if (cname == NULL)
			continue;

		ref = fn_ctx_lookup(ictx, cname, status);
		fn_composite_name_destroy(cname);
		if (ref == NULL)
			continue;

		ctx = fn_ctx_handle_from_ref(ref, 0, status);
		fn_ref_destroy(ref);

		printers = _xfn_get_sublist(status, ctx, conv, svc);
		fn_ctx_handle_destroy(ctx);
		if (printers != NULL)
			break;
	}

	fn_status_destroy(status);

	return (printers);
}


/*
 * This is not pretty.  For the time being, we are calling out to FNS
 *	programs to update the name service.  Hopefully we can fix this
 *	in the future to use the XFN api and do the right thing.
 */
/*ARGSUSED*/
int
_xfn_put_printer(const ns_printer_t *printer,
		ns_printer_t *(*iconv)(char *, FN_ref_t *, char *),
		char *svc, FN_ref_t *(*oconv)(ns_printer_t *))
{
	char cmd[BUFSIZ], *attrs = NULL;
	int rc = -1;

	if ((printer == NULL) || (access(UPDATE_CMD, F_OK) != 0))
		return (rc);

	/* remove the old reference */
	sprintf(cmd, "%s %s/%s >/dev/null 2>&1",
		DESTROY_CMD, CONTEXT, printer->name);
	system(cmd);
	sprintf(cmd, "%s %s/%s >/dev/null 2>&1",
		UNBIND_CMD, CONTEXT, printer->name);
	rc = system(cmd);

	/* add the new one */
	if ((attrs = (char *)(oconv)((ns_printer_t *)printer)) != NULL) {
		sprintf(cmd, "%s -s %s %s %s >/dev/null 2>&1",
			UPDATE_CMD, CONTEXT, printer->name, attrs);

		rc = system(cmd);
		free(attrs);
	}

	return (rc);
}
