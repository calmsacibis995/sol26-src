#ident	"@(#)generic.c	1.42	96/10/04 SMI"

/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * This file contains routines that are shared between the DHCP server
 * implementation and BOOTP server compatibility.
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <sys/syslog.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/dhcp.h>
#include "dhcpd.h"
#include "per_network.h"
#include "interfaces.h"
#include <locale.h>

extern int getnetmaskbyaddr(struct in_addr, struct in_addr *);

/*
 * Get the client id.
 *
 * Sets cid and len.
 */
void
get_client_id(PKT_LIST *plp, u_char *cid, u_int *len)
{
	DHCP_OPT *optp;

	optp = plp->opts[CD_CLIENT_ID];	/* get pointer to options */

	/*
	 * If the client specified the client id option, use that,
	 * otherwise use the client's hardware type and hardware address.
	 */
	if (optp != NULL) {
		*len = optp->len;
		(void) memcpy(cid, optp->value, *len);
	} else {
		*cid++ = plp->pkt->htype;
		*len = plp->pkt->hlen + 1;
		(void) memcpy(cid, plp->pkt->chaddr, *len);
	}
}

/*
 * Return a string representing an ASCII version of the client_id.
 */
char *
disp_cid(PKT_LIST *plp)
{
	DHCP_OPT	*optp = plp->opts[CD_CLIENT_ID];
	register u_char	*cp;
	static char	buf[DT_MAX_CID_LEN * 2];
	register u_char cplen;
	int len;

	if (optp != (DHCP_OPT *)0) {
		cp =  optp->value;
		cplen = optp->len;
	} else {
		cp = plp->pkt->chaddr;
		cplen =  plp->pkt->hlen;
	}

	len = DT_MAX_CID_LEN * 2;
	(void) octet_to_ascii((u_char *)cp, cplen, buf, &len);
	return (buf);
}

/*
 * Based on the contents of the PKT_LIST structure for an incoming
 * packet, determine the net address and subnet mask identifying the
 * dhcp-network database.
 *
 * Returns: 0 for success, 1 if unable to determine settings.
 */
int
determine_network(IF *ifp, PKT_LIST *plp, struct in_addr *netp,
    struct in_addr *subp)
{
	if (!netp || !subp || !ifp || !plp)
		return (1);

	if (plp->pkt->giaddr.s_addr != 0) {
		netp->s_addr = plp->pkt->giaddr.s_addr;
		/*
		 * Packet received thru a relay agent. Calculate the
		 * net's address using subnet mask and giaddr.
		 */
		(void) get_netmask(netp, &subp);
	} else {
		/* Locally connected net. */
		netp->s_addr = ifp->addr.s_addr;
		subp->s_addr = ifp->mask.s_addr;
	}
	return (0);
}

/*
 * Given a network-order address, calculate client's default net mask.
 * Consult netmasks database to see if net is further subnetted.
 * We'll only snag the first netmask that matches our criteria.
 *
 * Returns 0 for success, 1 otherwise.
 */
int
get_netmask(struct in_addr *n_addrp, struct in_addr **s_addrp)
{
	struct in_addr	ti, ts, tp;

	if (n_addrp == NULL || s_addrp == NULL)
		return (1);

	/*
	 * First check if VLSM is in use. Fall back on
	 * standard classed networks.
	 */
	if (getnetmaskbyaddr(*n_addrp, &tp) != 0) {
		ti.s_addr = ntohl(n_addrp->s_addr);
		if (IN_CLASSA(ti.s_addr)) {
			ts.s_addr = (u_long)IN_CLASSA_NET;
		} else if (IN_CLASSB(ti.s_addr)) {
			ts.s_addr = (u_long)IN_CLASSB_NET;
		} else {
			ts.s_addr = (u_long)IN_CLASSC_NET;
		}
		(*s_addrp)->s_addr = htonl(ts.s_addr); /* default */
	} else {
		(*s_addrp)->s_addr = tp.s_addr;
	}

	return (0);
}

/*
 * This function is charged with loading the options field with the
 * configured and/or asked for options. Note that if the packet is too
 * small to fit the options, then option overload is enabled.
 *
 * Returns: The actual size of the utilized packet buffer.
 */
int
load_options(int flags, PKT *r_pktp, int replen, u_char *optp, ENCODE *ecp,
    ENCODE *vecp)
{
	register u_char len;
	register u_char *vp, *vdata, *data, *endp, *main_optp,  *opt_endp;
	register u_short code;
	register u_char overload = 0, using_overload = 0;
	register ENCODE	*ep, *prevep, *tvep;
	register u_int vend_len;

	if (r_pktp == (PKT *)NULL || optp == (u_char *)NULL)
		return (EINVAL);

	if (ecp == (ENCODE *)NULL)
		return (replen);	/* no change. */

	opt_endp = (u_char *)((u_int)r_pktp->options + replen -
	    BASE_PKT_SIZE);
	endp = opt_endp;

	/*
	 * We handle vendor options by fabricating an ENCODE of type
	 * CD_VENDOR_SPEC, and setting it's datafield equal to vecp.
	 *
	 * We assume we've been handed the proper class list.
	 */
	if (vecp != (ENCODE *)NULL && (flags & DHCP_NON_RFC1048) == 0) {
		vend_len = 0;
		for (ep = vecp, vend_len = 0; ep; ep = ep->next)
			vend_len += (ep->len + 2);

		if (vend_len != 0) {
			if (vend_len > (u_int)0xff) {
				dhcpmsg(LOG_WARNING,
				    "Warning: Too many Vendor options\n");
				vend_len = (u_int)0xff;
			}
			vdata = (u_char *)smalloc(vend_len);

			for (vp = vdata, tvep = vecp; tvep &&
			    (u_char *)(vp + tvep->len + 2) <= &vdata[vend_len];
			    tvep = tvep->next) {
				*vp++ = tvep->code;
				*vp++ = tvep->len;
				(void) memcpy(vp, tvep->data, tvep->len);
				vp += tvep->len;
			}

			/* this make_encode *doesn't* copy data */
			tvep = make_encode(CD_VENDOR_SPEC, vend_len,
			    (void *)vdata, 0);

			/* Tack it on the end. */
			for (ep = prevep = ecp; ep; ep = ep->next)
				prevep = ep;
			prevep->next = tvep;
		}
	}

	/*
	 * Scan the options first to determine if we could potentially
	 * option overload.
	 */
	if (flags & DHCP_DHCP_CLNT) {
		for (ep = ecp; ep != (ENCODE *)NULL; ep = ep->next) {
			code = ep->code;
			if (code >= CD_PACKET_START && code <=
			    CD_PACKET_END) {
				switch (code) {
				case CD_SNAME:
					overload += 2;	/* using SNAME */
					break;
				case CD_BOOTFILE:
					overload += 1;	/* using FILE */
					break;
				}
			}
		}
	} else
		overload = 3;	/* No overload for BOOTP */

	/*
	 * Now actually load the options!
	 */
	for (ep = ecp; ep != (ENCODE *)NULL; ep = ep->next) {
		code = ep->code;
		len = ep->len;
		data = ep->data;

		if (code > CD_PACKET_END)
			continue;	/* Internal only code */

		/* non rfc1048 clients can only get packet fields */
		if ((flags & DHCP_NON_RFC1048) && (code < CD_PACKET_START ||
		    code > CD_PACKET_END)) {
			continue;
		}

		if ((flags & DHCP_SEND_LEASE) == 0 && (code == CD_T1_TIME ||
		    code == CD_T2_TIME || code == CD_LEASE_TIME)) {
			continue;
		}

		if (code >= CD_PACKET_START && code <= CD_PACKET_END) {
			switch (code) {
			case CD_SIADDR:
				/*
				 * Configuration includes Boot server addr
				 */
				(void) memcpy(&r_pktp->siaddr, data, len);
				break;
			case CD_SNAME:
				/*
				 * Configuration includes Boot server name
				 */
				(void) memcpy(&r_pktp->sname, data, len);
				break;
			case CD_BOOTFILE:
				/*
				 * Configuration includes boot file.
				 */
				(void) memcpy(&r_pktp->file, data, len);
				break;
			default:
				dhcpmsg(LOG_ERR,
				    "Unknown DHCP packet field: %d\n", code);
				break;
			}
		} else {
			/*
			 * Keep an eye on option field.. Option overload.
			 */
			if (&optp[len + 2] > endp) {
				/*
				 * If overload is 3, we will keep going,
				 * hoping to find an option that will
				 * fit in the remaining space, rather
				 * than just give up.
				 */
				if (overload != 3) {
					if (!using_overload) {
						*optp++ = CD_OPTION_OVERLOAD;
						*optp++ = 1;
						main_optp = optp;
					} else {
						if (optp < endp)
							*optp = CD_END;
						overload += using_overload;
					}
				}

				switch (overload) {
				case 0:
					/* great, can use both */
					/* FALLTHRU */
				case 1:
					/* Can use sname. */
					optp = (u_char *)&r_pktp->sname;
					endp = (u_char *)&r_pktp->file;
					using_overload += 2;
					break;
				case 2:
					/* Using sname, can use file. */
					optp = (u_char *)&r_pktp->file;
					endp = (u_char *)&r_pktp->cookie;
					using_overload += 1;
					break;
				}
			} else {
				/* Load options. */
				*optp++ = (u_char)code;
				*optp++ = len;
				(void) memcpy(optp, data, len);
				optp += len;
			}
		}
	}

	if (using_overload)
		*main_optp++ = using_overload;
	else
		main_optp = optp;	/* no overload */

	if (main_optp < opt_endp)
		*main_optp++ = CD_END;

	if (optp < endp)
		*optp = CD_END;

	return (BASE_PKT_SIZE + (u_int)(main_optp - r_pktp->options));
}

int
idle(void)
{
	register int	err = 0;
	register IF	*ifp;
	extern IF	*if_head;	/* interfaces.c */
	struct in_addr	zeroip;

	if (reinitialize || (abs_rescan != 0 && abs_rescan < time(NULL))) {
		/*
		 * Got a signal to reinitialize
		 */
		if (errno == EINTR)
			errno = 0;

		if (verbose)
			dhcpmsg(LOG_INFO, "Reinitializing server\n");

		if (!no_dhcptab) {
			if ((err = checktab()) != 0) {
				dhcpmsg(LOG_WARNING,
				    "WARNING: Cannot access dhcptab.\n");
			} else {
				if ((err = readtab()) != 0) {
					dhcpmsg(LOG_ERR,
					    "Error reading dhcptab.\n");
					return (err);
				}
			}
		}
		if (verbose)
			dhcpmsg(LOG_INFO,
			    "Total Packets received on all interfaces: %d\n",
			    totpkts);

		/*
		 * Drop all pending offers, display interface statistics.
		 */
		for (ifp = if_head; ifp; ifp = ifp->next) {
			if (verbose)
				disp_if_stats(ifp);
			free_offers(ifp);
		}
		if (verbose)
			dhcpmsg(LOG_INFO, "Server reinitialized.\n");

		reinitialize = 0;	/* reset the flag */
		if (abs_rescan != 0)
			abs_rescan = (rescan_interval * 60L) + time(NULL);

	} else {
		/*
		 * Scan for expired offers; clean up.
		 */
		zeroip.s_addr = INADDR_ANY;
		for (ifp = if_head; ifp; ifp = ifp->next)
			(void) check_offers(ifp, &zeroip);
	}
	return (err);
}
