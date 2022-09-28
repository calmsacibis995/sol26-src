#ident	"@(#)packet.c	1.33	96/06/15 SMI"

/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * this file contains code that processes incoming packets and vectors off
 * to the appropriate function based on our run mode and the packet type.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <string.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/dhcp.h>
#include "dhcpd.h"
#include "per_network.h"
#include "interfaces.h"
#include <locale.h>

extern IF	*if_head;	/* head of interfaces list, interfaces.c */

static const u_char magic_cookie[] = BOOTMAGIC;

/*
 * We break apart the packet, then vector off based on our run mode and
 * the packet type.
 *
 * Returns: 0 for success, errno otherwise. Nonfatal conditions are
 * ignored.
 */
int
process_pkts()
{
	register int err = 0, is_rfc1048;
	register IF *ifp;
	register PKT_LIST *plp;

	/*
	 * We loop through each interface and process one packet per interface.
	 */
	for (ifp = if_head; ifp != NULL; ifp = ifp->next) {
		if ((plp = ifp->pkthead) == NULL)
			continue;  /* nothing on this interface */

		/*
		 * Remove the packet from the list for the interface.
		 */
		ifp->pkthead = plp->next;
		if (ifp->pkthead == NULL)
			ifp->pkttail = NULL;

		npkts--;	/* one less packet to process. */
		ifp->processed++;

		/*
		 * If packet is too short, discard.
		 */
		if (plp->len < sizeof (PKT)) {
			if (verbose) {
				dhcpmsg(LOG_INFO,
				    "Short packet on %s ignored.\n",
				    ifp->nm);
			}
			free_plp(plp);
			continue;
		}
		if (plp->pkt->hops >= (u_char)(max_hops + 1)) {
			if (verbose) {
				dhcpmsg(LOG_INFO,
				    "Packet dropped: too many relay hops: %d\n",
				    plp->pkt->hops);
			}
			free_plp(plp);
			continue;
		}

		if (debug && plp->pkt->giaddr.s_addr != 0L &&
		    plp->pkt->giaddr.s_addr != ifp->addr.s_addr) {
			dhcpmsg(LOG_INFO,
			    "Packet received from relay agent: %s\n",
			    inet_ntoa(plp->pkt->giaddr));
		}

		/*
		 * Based on the packet type, process accordingly.
		 */
		switch (plp->pkt->op) {
		case BOOTREPLY:
			if (!server_mode) {
				/*
				 * We're in relay agent mode.
				 */
				err = relay_agent(ifp, plp);
				if (err != 0) {
					dhcpmsg(LOG_ERR,
"Relay agent mode failed: %d (reply)\n",
					    err);
					free_plp(plp);
					return (err);
				}
			}
			break;

		case BOOTREQUEST:
			if (!server_mode) {
				/*
				 * we're a relay agent!
				 */
				err = relay_agent(ifp, plp);
				if (err != 0) {
					dhcpmsg(LOG_ERR,
"Relay agent mode failed: %d (request)\n",
					    err);
					free_plp(plp);
					return (err);
				}
				break;
			}

			/*
			 * Allow packets without RFC1048 magic cookies. Just
			 * don't do an options scan on them, thus we treat
			 * them as plain BOOTP packets. The BOOTP server
			 * can deal with requests of this type.
			 */
			if (memcmp(plp->pkt->cookie, magic_cookie,
			    sizeof (magic_cookie)) != 0) {
				if (verbose) {
					dhcpmsg(LOG_INFO, "Client: %s using \
non-RFC1048 BOOTP cookie.\n",
					    disp_cid(plp));
				}
				is_rfc1048 = FALSE;
			} else {
				/*
				 * Scan the options in the packet and
				 * fill in the opts and vs fields in the
				 * pktlist structure.  If there's a
				 * DHCP message type in the packet then
				 * it's a DHCP packet; otherwise it's
				 * a BOOTP packet. Standard options are
				 * RFC1048 style.
				 */
				if (_dhcp_options_scan(plp) != 0) {
					dhcpmsg(LOG_ERR, "Garbled \
DHCP/BOOTP datagram received on interface: %s\n",
					    ifp->nm);
					break;
				}
				is_rfc1048 = TRUE;
			}

			if (plp->opts[CD_DHCP_TYPE]) {
				/*
				 * DHCP packet
				 */
				err = dhcp(ifp, plp);
				if (err != 0) {
					free_plp(plp);
					return (err);	/* fatal */
				}
			} else {
				/*
				 * BOOTP packet
				 */
				if (!bootp_compat) {
					dhcpmsg(LOG_INFO, "BOOTP request \
received on interface: %s ignored.\n",
					    ifp->nm);
					break;
				}
				err = bootp_compatibility(ifp, is_rfc1048, plp);
				if (err != 0) {
					dhcpmsg(LOG_ERR, "BOOTP \
compatibility mode failed: %d\n",
					    err);
					free_plp(plp);
					return (err);
				}
			}
			break;
		default:
			dhcpmsg(LOG_ERR, "Unexpected packet received on \
BOOTP server port. Interface: %s. Ignored.\n",
			    ifp->nm);
			break;
		}
		/*
		 * Free the packet.
		 */
		free_plp(plp);
	}
	return (err);
}

/*
 * Given a received BOOTP packet, generate an appropriately sized,
 * and generically initialized BOOTP packet.
 */
PKT *
gen_bootp_pkt(int size, PKT *srcpktp)
{
	/* LINTED [smalloc returns lw aligned addresses.] */
	register PKT *pkt = (PKT *)smalloc(size);

	pkt->htype = srcpktp->htype;
	pkt->hlen = srcpktp->hlen;
	pkt->xid = srcpktp->xid;
	pkt->secs = srcpktp->secs;
	pkt->flags = srcpktp->flags;
	pkt->giaddr.s_addr = srcpktp->giaddr.s_addr;
	(void) memcpy(pkt->cookie, srcpktp->cookie, 4);
	(void) memcpy(pkt->chaddr, srcpktp->chaddr, srcpktp->hlen);

	return (pkt);
}

/*
 * Points field serves to identify those packets whose allocated size
 * and address is not represented by the address in pkt.
 */
void
free_plp(PKT_LIST *plp)
{
	register char *tmpp;

	if (plp->pkt) {
		if (plp->points != 0)
			tmpp = (char *)((u_int)plp->pkt - plp->points);
		else
			tmpp = (char *)plp->pkt;
		free(tmpp);
	}
	free(plp);
	plp = PKT_LIST_NULL;
}
