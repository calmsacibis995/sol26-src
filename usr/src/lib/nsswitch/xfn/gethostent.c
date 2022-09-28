/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)gethostent.c	1.1	96/03/26 SMI"


#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <nss_common.h>
#include <nss_dbdefs.h>
#include <xfn/xfn.h>
#include "xfn_common.h"


/*
 * Construct a hostent given an array of the host's attributes.
 * Use buf for storage.
 */
static nss_status_t attrs2hostent(FN_attribute_t *attrs[], int n_attrs,
    struct hostent *he, char *buf, int bufsz);


/* Names of hosts' attributes. */
static const FN_identifier_t attr_ids[] = {
	IDENTIFIER("onc_host_name"),		/* cname */
	IDENTIFIER("onc_host_aliases"),
	IDENTIFIER("onc_host_ip_addresses"),
};
#define	NUM_ATTRS (sizeof (attr_ids) / sizeof (attr_ids[0]))

/* For indexing into the above array and the like. */
enum {ATTR_NAME, ATTR_ALIASES, ATTR_ADDRS};


#define	IP_SIZE	(4 * 4)	/* max size of an IP string (eg: "129.144.40.71") */


static nss_status_t
getbyname(xfn_backend_ptr_t be, void *a)
{
	nss_XbyY_args_t		*args = (nss_XbyY_args_t *)a;
	nss_status_t		stat;
	FN_attribute_t		*attrs[NUM_ATTRS];
	int i;

	/*
	 * Fail quickly if name does not contain a slash.  Don't need to
	 * deal with "thishost":  it does not have attributes, so we
	 * cannot find its address.
	 */
	if (strchr(args->key.name, '/') == NULL) {
		return (NSS_NOTFOUND);
	}

	/* Get values of attributes. */
	stat = _nss_xfn_get_attrs(args->key.name, attr_ids, attrs, NUM_ATTRS);
	if (stat != NSS_SUCCESS) {
		return (stat);
	}

	/* Construct hostent. */
	switch (attrs2hostent(attrs, NUM_ATTRS,
	    args->buf.result, args->buf.buffer, args->buf.buflen)) {
	case NSS_STR_PARSE_SUCCESS:
		args->returnval = args->buf.result;
		stat = NSS_SUCCESS;
		break;
	case NSS_STR_PARSE_ERANGE:
		args->erange = 1;
		/* FALL THROUGH */
	default:
		stat = NSS_NOTFOUND;
	}

	for (i = 0; i < NUM_ATTRS; i++) {
		fn_attribute_destroy(attrs[i]);
	}
	return (stat);
}


static nss_status_t
attrs2hostent(FN_attribute_t *attrs[], int n_attrs, struct hostent *he,
    char *buf, int bufsz)
{
	const FN_attrvalue_t *val;
	void *iter;
	struct in_addr *addrs;
	char ip_str[IP_SIZE];
	int n;

/*
 * Space within buf is allocated like this:
 *
 * |-------|------------|------------|   |----------------|----------------|
 * | host  | aliases    | addresses  |   | pointer vector | pointer vector |
 * | name  | grow this  | grow this  |...| for addresses  | for aliases    |
 * |       | way  ->    | way  ->    |   | <-  this way   | <-  this way   |
 * |-------|------------|------------|   |----------------|----------------|
*/

	/* Name */
	val = fn_attribute_first(attrs[ATTR_NAME], &iter);
	if (val == NULL) {
		return (NSS_STR_PARSE_PARSE);
	} else if (val->length >= bufsz) {
		return (NSS_STR_PARSE_ERANGE);
	}
	memcpy(buf, val->contents, val->length);
	buf[val->length] = '\0';
	he->h_name = buf;
	buf += val->length + 1;
	bufsz -= val->length + 1;

	/* Aliases */
	n = fn_attribute_valuecount(attrs[ATTR_ALIASES]);
	he->h_aliases = (char **)ROUND_DOWN(buf + bufsz, sizeof (char *));
	--he->h_aliases;
	if (buf > (char *)he->h_aliases) {
		return (NSS_STR_PARSE_ERANGE);
	}
	*he->h_aliases = NULL;
	for (val = fn_attribute_first(attrs[ATTR_ALIASES], &iter);
	    val != NULL;
	    val = fn_attribute_next(attrs[ATTR_ALIASES], &iter)) {
		--he->h_aliases;
		if (buf + val->length + 1 > (char *)he->h_aliases) {
			return (NSS_STR_PARSE_ERANGE);
		}
		*he->h_aliases = buf;
		memcpy(buf, val->contents, val->length);
		buf[val->length] = '\0';
		buf += val->length + 1;
	}
	bufsz = (char *)he->h_aliases - buf;

	/* Addresses */
	n = fn_attribute_valuecount(attrs[ATTR_ADDRS]);
	addrs = (struct in_addr *)ROUND_UP(buf, sizeof (addrs[0]));
	he->h_addr_list =
	    (char **)ROUND_DOWN(buf + bufsz, sizeof (he->h_addr_list[0]));
	/* Allow room for trailing NULL in size calculation. */
	if ((char *)(addrs + n) >
	    (char *)(he->h_addr_list - n - 1)) {
		return (NSS_STR_PARSE_ERANGE);
	}
	*--he->h_addr_list = NULL;
	for (val = fn_attribute_first(attrs[ATTR_ADDRS], &iter);
	    val != NULL;
	    val = fn_attribute_next(attrs[ATTR_ADDRS], &iter)) {
		if (val->length >= sizeof (ip_str)) {
			return (NSS_STR_PARSE_PARSE);
		}
		memcpy(ip_str, val->contents, val->length);
		ip_str[val->length] = '\0';
		addrs[0].s_addr = inet_addr(ip_str);
		if (addrs[0].s_addr != (u_long)-1) {
			*--he->h_addr_list = (char *)addrs++;
		}
	}

	he->h_addrtype = AF_INET;
	he->h_length = sizeof (addrs[0]);
	return (NSS_STR_PARSE_SUCCESS);
}


static xfn_backend_op_t host_ops[] = {
	_nss_xfn_destr,
	NULL,	/* endent */
	NULL,	/* setent */
	NULL,	/* getent */
	getbyname
};


nss_backend_t *
_nss_xfn_hosts_constr(const char *dummy1, const char *dummy2,
    const char *dummy3)
{
	int n_ops = sizeof (host_ops) / sizeof (host_ops[0]);

	return (_nss_xfn_constr(host_ops, n_ops));
}
