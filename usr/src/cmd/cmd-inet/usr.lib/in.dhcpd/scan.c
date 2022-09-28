#ident	"@(#)scan.c	1.10	96/03/04	SMI"

/*
 * Copyright (c) 1996 by Sun Microsystems, Inc. All rights reserved.
 *
 * Routines used to extract/insert DHCP options.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/dhcp.h>

/*
 * Scan field for options.
 */
static void
field_scan(u_char *start, u_char *end,
    DHCP_OPT **options, u_char last_option)
{
	while (start < end) {
		if (*start == CD_PAD) {
			start++;
			continue;
		}
		if (*start == CD_END)
			break;		/* done */
		if (*start > last_option) {
			start++;
			start += *start + 1;
			continue;	/* unrecognized option */
		}
		/* Ignores duplicate options. */
		if (options[*start] == 0)
			options[*start] = (DHCP_OPT *)start;
		start++;		/* length */
		start += *start + 1;	/* step over length byte + len */
	}
}
/*
 * Scan Vendor field for options.
 */
static void
vendor_scan(PKT_LIST *pl)
{
	register u_char	*start, *end, len;

	if (pl->opts[CD_VENDOR_SPEC] == NULL)
		return;
	len = pl->opts[CD_VENDOR_SPEC]->len;
	start = pl->opts[CD_VENDOR_SPEC]->value;
	end = (u_char *)((u_char *) start + len);

	field_scan(start, end, pl->vs, VS_OPTION_END - VS_OPTION_START);
}
/*
 * Load opts table in PKT_LIST entry with PKT's options.
 * Returns 0 if no fatal errors occur, otherwise...
 */
int
_dhcp_options_scan(PKT_LIST *pl)
{
	register PKT 	*pkt = pl->pkt;
	register u_int		opt_size = pl->len - BASE_PKT_SIZE;

	/* check the options field */
	field_scan(pkt->options, &pkt->options[opt_size], pl->opts,
	    DHCP_LAST_OPT);
	/*
	 * process vendor specific options. We look at the vendor options
	 * here, simply because a BOOTP server could fake DHCP vendor
	 * options. This increases our interoperability with BOOTP.
	 */
	if (pl->opts[CD_VENDOR_SPEC])
		vendor_scan(pl);

	if (pl->opts[CD_DHCP_TYPE] == NULL)
		return (0);

	if (pl->opts[CD_DHCP_TYPE]->len != 1)
		return (DHCP_GARBLED_MSG_TYPE);

	if (*pl->opts[CD_DHCP_TYPE]->value < DISCOVER ||
	    *pl->opts[CD_DHCP_TYPE]->value > INFORM)
		return (DHCP_WRONG_MSG_TYPE);

	if (pl->opts[CD_OPTION_OVERLOAD]) {
		if (pl->opts[CD_OPTION_OVERLOAD]->len != 1) {
			pl->opts[CD_OPTION_OVERLOAD] = (DHCP_OPT *) 0;
			return (DHCP_BAD_OPT_OVLD);
		}
		switch (*pl->opts[CD_OPTION_OVERLOAD]->value) {
		case 1:
			field_scan(pkt->file, &pkt->cookie[0], pl->opts,
			    DHCP_LAST_OPT);
			break;
		case 2:
			field_scan(pkt->sname, &pkt->file[0], pl->opts,
			    DHCP_LAST_OPT);
			break;
		case 3:
			field_scan(pkt->file, &pkt->cookie[0], pl->opts,
			    DHCP_LAST_OPT);
			field_scan(pkt->sname, &pkt->file[0], pl->opts,
			    DHCP_LAST_OPT);
			break;
		default:
			pl->opts[CD_OPTION_OVERLOAD] = (DHCP_OPT *) 0;
			return (DHCP_BAD_OPT_OVLD);
		}
	}
	return (0);
}
