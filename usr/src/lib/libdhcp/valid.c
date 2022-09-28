/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)valid.c	1.12	96/10/14 SMI"

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <netdb.h>
#include <string.h>

/*
 * XXX exclude T_UNSPEC namespace pollution <arpa/nameser.h> vs
 * <sys/tpicommon.h>
 */
#undef T_UNSPEC
#include <dd_impl.h>

#define	IPADDR_CHARS	"0123456789."
#define	ETHERADDR_CHARS	"0123456789abcdefABCDEF:"

#define	valid_ip_fmt(ia, a) \
((strspn(ia, IPADDR_CHARS) == strlen(ia)) && (ia[strlen(ia) - 1] != '.') && \
	(sscanf(ia, "%u.%u.%u.%u.%u", &a[0], &a[1], &a[2], &a[3], &a[4]) == 4))

/* ARGSUSED */
static int
always_valid(const char *cp)
{
	return (1);
}

/*
 * valid_host_ip_addr - Validate a host IP address
 */
static int
valid_host_ip_addr(const char *ip_addr)
{
	u_int aa[5], i;
	u_long addr, host, net;

	if (valid_ip_fmt(ip_addr, aa)) {
		for (i = 0; i < 4; ++i)
			if (aa[i] > 255)
				return (0);
		addr = (aa[0] << 24) | (aa[1] << 16) | (aa[2] << 8) | aa[3];
		if (IN_CLASSA(addr)) {
			net = IN_CLASSA_NET & addr;
			host = IN_CLASSA_HOST & addr;
			if ((net != 0) && (host != 0) &&
			    (host != IN_CLASSA_HOST))
				return (1);
		} else if (IN_CLASSB(addr)) {
			net = IN_CLASSB_NET & addr;
			host = IN_CLASSB_HOST & addr;
			if ((net != (u_long)0x80000000) &&
			    (host != (u_long)0) && (host != IN_CLASSB_HOST))
				return (1);
		} else if (IN_CLASSC(addr)) {
			net = IN_CLASSC_NET & addr;
			host = IN_CLASSC_HOST & addr;
			if ((net != (u_long)0xc0000000) &&
			    (host != (u_long)0) && (host != IN_CLASSC_HOST))
				return (1);
		}
	}

	return (0);
}

/*
 * valid_domainname - validate a domain name using the recommended syntax from
 * RFC 1035, section 2.3.1, plus allowing for '_'
 */
static int
valid_domainname(const char *domain)
{
	char str[MAXDNAME];
	char *cp;
	int l;

	if (((l = strlen(domain)) < sizeof (str)) && (l > 0) &&
	    (sscanf(domain, "%[0-9a-zA-Z._-]", str) == 1) &&
	    (*domain != '-') && (domain[l - 1] != '-') &&
	    (strcmp(str, domain) == 0)) {
		for (cp = strtok(str, "."); cp != NULL; cp = strtok(NULL, "."))
			if (strlen(cp) > (size_t)MAXLABEL)
				return (0);
		return (1);
	} else
		return (0);
}

/*
 * valid_hostname - ensure that a hostname is compliant with RFC 952+1123 plus
 * domain name rules plus
 * Summary:
 *	Hostname must be less than MAXHOSTNAMELEN and greater than 1 char in
 * 	length.  Must contain only alphanumerics plus '-' '.' and '_', and may
 *	not begin or end with '.' '-' or '_'.
 */
static int
valid_hostname(const char *hostname)
{
	char str[MAXHOSTNAMELEN];
	int l;
	char *cp;

	if (((l = strlen(hostname)) < sizeof (str)) && (l > 1) &&
	    (sscanf(hostname, "%[0-9a-zA-Z._-]", str) == 1) &&
	    (isalnum(*hostname)) && (isalnum(hostname[l - 1])) &&
	    (strcmp(str, hostname) == 0)) {
		for (cp = strtok(str, "."); cp != NULL; cp = strtok(NULL, "."))
			if (strlen(cp) > (size_t)MAXLABEL)
				return (0);
		return (1);
	} else
		return (0);
}

/*
 * valid_client_id - validate a client id.
 */
static int
valid_client_id(const char *cid)
{
	int i = 0;

	while (cid[i])
		if (isspace(cid[i++]))
			return (0);
	if (i > 1)
		return (1);
	else
		return (0);
}

/*
 * valid_dhcp_flag - validate a flag param for the dhcp_ip table
 */
static int
valid_dhcp_flag(const char *flag)
{
	int i = 0;

	while (flag[i])
		if (isspace(flag[i++]))
			return (0);
	if (i)
		return (1);
	else
		return (0);
}

/*
 * valid_lease_expire - validate a lease for the dhcp_ip table
 */
static int
valid_lease_expire(const char *exp)
{
	int i = 0;

	while (exp[i])
		if (isspace(exp[i++]))
			return (0);
	if (i)
		return (1);
	else
		return (0);
}

/*
 * valid_packet_macro - validate a packet macro name
 */
static int
valid_packet_macro(const char *mac)
{
	int i = 0;

	while (mac[i])
		if (isspace(mac[i++]))
			return (0);
	if (i)
		return (1);
	else
		return (0);
}

/*
 * valid_dhcptab_key - validate a dhcptab key
 */
static int
valid_dhcptab_key(const char *key)
{
	int i = 0;

	while (key[i])
		if (isspace(key[i++]))
			return (0);
	if (i)
		return (1);
	else
		return (0);
}

/*
 * valid_dhcptab_type - validate a dhcptab record type
 */
static int
valid_dhcptab_type(const char *type)
{
	if (strcasecmp(type, "s") && strcasecmp(type, "m"))
		return (0);
	else
		return (1);
}

static int (*valid_funcs[])(const char *) = {
	&always_valid,
	&valid_host_ip_addr,
	&valid_domainname,
	&valid_hostname,
	&valid_client_id,
	&valid_dhcp_flag,
	&valid_lease_expire,
	&valid_packet_macro,
	&valid_dhcptab_key,
	&valid_dhcptab_type
};

int
_dd_validate(
	uint_t parm_type,
	const char *parm)
{

	if (parm_type > TBL_MAX_VALID_TYPE)
		return (-1);
	else
		return (valid_funcs[parm_type](parm));
}
