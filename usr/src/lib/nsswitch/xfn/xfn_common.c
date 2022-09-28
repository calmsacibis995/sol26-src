/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)xfn_common.c	1.1	96/03/26 SMI"


#include <stdlib.h>
#include <string.h>
#include <nss_dbdefs.h>
#include <xfn/xfn.h>
#include "xfn_common.h"


/* Return true if the two identifiers are equal. */
static int ids_equal(const FN_identifier_t *, const FN_identifier_t *);


struct xfn_backend {
	xfn_backend_op_t	*ops;
	nss_dbop_t		n_ops;
};


/* Syntax to use for attribute creation. */
const FN_identifier_t ascii = IDENTIFIER("fn_attr_syntax_ascii");


nss_status_t
_nss_xfn_map_statcode(unsigned int fn_stat)
{
	switch (fn_stat) {
	case FN_SUCCESS:
		return (NSS_SUCCESS);
	case FN_E_CTX_UNAVAILABLE:
		return (NSS_TRYAGAIN);
	case FN_E_NAME_NOT_FOUND:
	case FN_E_NOT_A_CONTEXT:
	case FN_E_CTX_NO_PERMISSION:
	case FN_E_NO_SUPPORTED_ADDRESS:
	case FN_E_NO_SUCH_ATTRIBUTE:
	case FN_E_ATTR_NO_PERMISSION:
	case FN_E_INVALID_ATTR_IDENTIFIER:
	case FN_E_TOO_MANY_ATTR_VALUES:
	case FN_E_OPERATION_NOT_SUPPORTED:
	case FN_E_PARTIAL_RESULT:
	case FN_E_INCOMPATIBLE_CODE_SETS:
	case FN_E_INCOMPATIBLE_LOCALES:
		return (NSS_NOTFOUND);
	default:
		return (NSS_UNAVAIL);
	}
}


nss_status_t
_nss_xfn_get_attrs(const char *name, const FN_identifier_t attr_names[],
    FN_attribute_t *attrs[], int num_attrs)
{
	nss_status_t		stat = NSS_UNAVAIL;
	FN_status_t		*status;
	FN_ctx_t		*init_ctx = NULL;
	FN_composite_name_t	*cname = NULL;
	FN_attribute_t		*attr = NULL;
	FN_attrset_t		*attr_set = NULL;
	FN_multigetlist_t	*attr_list = NULL;
	int			i, j;

	status = fn_status_create();
	if (status == NULL) {
		goto out;
	}
	init_ctx = fn_ctx_handle_from_initial(0, status);
	if (init_ctx == NULL) {
		stat = _nss_xfn_map_statcode(fn_status_code(status));
		goto out;
	}
	cname = fn_composite_name_from_str((const unsigned char *)name);
	if (cname == NULL) {
		goto out;
	}

	/*
	 * Use fn_attr_multi_get() for potential efficiency if there's
	 * more than one attribute, even though it's quite a bit more work.
	 */
	if (num_attrs == 1) {
		attrs[0] =
		    fn_attr_get(init_ctx, cname, &attr_names[0], 1, status);
		stat = _nss_xfn_map_statcode(fn_status_code(status));
	} else {
		attr_set = fn_attrset_create();
		if (attr_set == NULL) {
			goto out;
		}
		for (i = 0; i < num_attrs; i++) {
			attrs[i] = NULL;
			attr = fn_attribute_create(&attr_names[i], &ascii);
			if (attr == NULL ||
			    fn_attrset_add(attr_set, attr, 1) == 0) {
				goto out;
			}
			fn_attribute_destroy(attr);
			attr = NULL;
		}
		attr_list =
		    fn_attr_multi_get(init_ctx, cname, attr_set, 1, status);
		stat = _nss_xfn_map_statcode(fn_status_code(status));
		if (stat != NSS_SUCCESS) {
			goto out;
		}
		for (j = 0; j < num_attrs; j++) {
			attr = fn_multigetlist_next(attr_list, status);
			stat = _nss_xfn_map_statcode(fn_status_code(status));
			if (stat != NSS_SUCCESS) {
				for (i = 0; i < num_attrs; i++) {
					fn_attribute_destroy(attrs[i]);
				}
				goto out;
			}
			/* Find slot in attrs[] that corresponds to attr. */
			for (i = 0; i < num_attrs; i++) {
				if (attrs[i] == NULL &&
		ids_equal(&attr_names[i], fn_attribute_identifier(attr))) {
					attrs[i] = attr;
					attr = NULL;
					break;
				}
			}
			if (i == num_attrs) {	/* shouldn't happen */
				stat = NSS_NOTFOUND;
				for (i = 0; i < num_attrs; i++) {
					fn_attribute_destroy(attrs[i]);
				}
				goto out;
			}
		}
	}

out:
	fn_ctx_handle_destroy(init_ctx);
	fn_composite_name_destroy(cname);
	fn_attribute_destroy(attr);
	fn_attrset_destroy(attr_set);
	fn_multigetlist_destroy(attr_list);
	fn_status_destroy(status);
	return (stat);
}


static int
ids_equal(const FN_identifier_t *a, const FN_identifier_t *b)
{
	return (a->format == b->format &&
		a->length == b->length &&
		memcmp(a->contents, b->contents, a->length) == 0);
}


nss_backend_t *
_nss_xfn_constr(xfn_backend_op_t ops[], int n_ops)
{
	xfn_backend_ptr_t be;

	be = (xfn_backend_ptr_t) malloc(sizeof (*be));
	if (be != NULL) {
		be->ops	= ops;
		be->n_ops = n_ops;
	}
	return ((nss_backend_t *)be);
}


nss_status_t
_nss_xfn_destr(xfn_backend_ptr_t be, void *dummy)
{
	free(be);
	return (NSS_SUCCESS);
}
