#ident	"@(#)relay.c	1.31	96/04/22 SMI"

/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <sys/syslog.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/dhcp.h>
#include "dhcpd.h"
#include "per_network.h"
#include "interfaces.h"
#include <locale.h>

#define	MAX_RELAY_IP		5	/* Maximum of destinations */

static struct in_addr relay_ip[MAX_RELAY_IP];   /* IP's of DHCP servers */
static int relay_reply(IF *, PKT_LIST *);
static int relay_request(IF *, PKT_LIST *);

extern IF *if_head;

/*
 * This file contains the code which implements the BOOTP relay agent.
 */

/*
 * Parse arguments.  If an agument begins with a digit, then it's
 * an IP address, otherwise it's a hostname which needs to be
 * resolved into an IP address.
 *
 * Use the arguments to fill in relay_ip array.
 */
int
relay_agent_init(char *args)
{
	register int	i;
	struct hostent	*hp;
	register struct in_addr	*inp;

	for (i = 0; i <= MAX_RELAY_IP; i++) {
		if ((args = strtok(args, ",")) == NULL)
			break;		/* done */

		/*
		 * If there's more than MAX_RELAY_IP addresses
		 * specified that's an error.  If we can't
		 * resolve the host name, that's an error.
		 *
		 * XXX -- should we print better error messages?
		 *	should we make sure that the IP
		 *	address is valid, i.e. not a broadcast
		 *	addr?
		 */
		if (i == MAX_RELAY_IP) {
			(void) fprintf(stderr,
			    gettext("Too many relay agent destinations.\n"));
			return (E2BIG);
		}

		if ((hp = gethostbyname(args)) == NULL) {
			(void) fprintf(stderr, gettext(
			    "Invalid relay agent destination name: %s\n"),
			    args);
			return (EINVAL);
		}
		/* LINTED [will be lw aligned] */
		inp = (struct in_addr *)hp->h_addr;

		/*
		 * Note: no way to guess at destination subnet mask,
		 * and verify that it's not a new broadcast addr.
		 */
		if (inp->s_addr == INADDR_ANY ||
		    inp->s_addr == INADDR_LOOPBACK ||
		    inp->s_addr == INADDR_BROADCAST) {
			(void) fprintf(stderr, gettext("Relay destination \
cannot be 0, loopback, or broadcast address.\n"));
			return (EINVAL);
		}

		/* LINTED [apples == apples] */
		relay_ip[i].s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;
		if (verbose) {
			(void) fprintf(stdout,
			    gettext("Relay destination: %s (%s)\n"),
			    inet_ntoa(relay_ip[i]), args);
		}
		args = NULL;	/* for next call to strtok() */
	}
	if (i == 0) {
		/*
		 * Gotta specify at least one IP addr.
		 */
		(void) fprintf(stderr,
		    gettext("Specify at least one relay agent destination.\n"));
		return (ENOENT);
	}
	if (i < MAX_RELAY_IP)
		relay_ip[i].s_addr = NULL; 	/* terminate the list */

	return (0);
}

int
relay_agent(IF *ifp, PKT_LIST *plp)
{
	if (plp->pkt->op == BOOTREQUEST)
		return (relay_request(ifp, plp));

	return (relay_reply(ifp, plp));
}

static int
relay_request(IF *ifp, PKT_LIST *plp)
{
	register PKT	*pkp;
	struct sockaddr_in to;
	register int	i;

	pkp = plp->pkt;
	if (pkp->giaddr.s_addr == 0L)
		pkp->giaddr.s_addr = ifp->addr.s_addr;
	pkp->hops++;

	/*
	 * Send it on to the next relay(s)/servers
	 */
	to.sin_port = IPPORT_BOOTPS;

	for (i = 0; i < MAX_RELAY_IP; i++) {
		if (relay_ip[i].s_addr == 0L)
			break;		/* we're done */

		if (ifp->addr.s_addr == relay_ip[i].s_addr) {
			if (verbose) {
				dhcpmsg(LOG_INFO, "Relay destination: %s is \
the same as our interface, ignored.\n",
				    inet_ntoa(ifp->addr));
			}
			continue;
		}
		to.sin_addr.s_addr = relay_ip[i].s_addr;

		if (debug) {
			if (to.sin_port == IPPORT_BOOTPS) {
				dhcpmsg(LOG_INFO,
				    "Relaying request to %1$s, server port.\n",
				    inet_ntoa(to.sin_addr));
			} else {
				dhcpmsg(LOG_INFO,
				    "Relaying request to %1$s, client port.\n",
				    inet_ntoa(to.sin_addr));
			}
		}

		if (write_interface(ifp, pkp, plp->len, &to)) {
			dhcpmsg(LOG_INFO, "Cannot relay request to %s\n",
			    inet_ntoa(to.sin_addr));
		}
	}
	return (0);
}

static int
relay_reply(IF *ifp, PKT_LIST *plp)
{
	register IF	*tifp;
	register PKT	*pkp = plp->pkt;
	char		scratch[20];
	struct in_addr	to;

	if (pkp->giaddr.s_addr == 0L) {
		/*
		 * Somehow we picked up a reply packet from a DHCP server
		 * on this net intended for a client on this net. Drop it.
		 */
		if (verbose) {
			dhcpmsg(LOG_INFO,
			    "Reply packet without giaddr set ignored.\n");
		}
		return (0);
	}

	/*
	 * We can assume that the message came directly from a dhcp/bootp
	 * server to us, and we are to address it directly to the client.
	 */
	if (pkp->giaddr.s_addr != ifp->addr.s_addr) {
		/*
		 * It is possible that this is a multihomed host. We'll
		 * check to see if this is the case, and handle it
		 * appropriately.
		 */
		for (tifp = if_head; tifp != NULL; tifp = tifp->next) {
			if (tifp->addr.s_addr == pkp->giaddr.s_addr)
				break;
		}

		if (tifp == NULL) {
			if (verbose) {
				(void) strcpy(scratch,
				    inet_ntoa(pkp->giaddr));
				dhcpmsg(LOG_INFO, "Received relayed reply \
not intended for this interface: %1$s giaddr: %2$s\n",
				    inet_ntoa(ifp->addr), scratch);
			}
			return (0);
		} else
			ifp = tifp;
	}
	if (debug) {
		dhcpmsg(LOG_INFO, "Relaying reply to client %s\n",
		    disp_cid(plp));
	}
	pkp->hops++;

	if ((ntohs(pkp->flags) & BCAST_MASK) == 0) {
		if (pkp->yiaddr.s_addr == htonl(INADDR_ANY)) {
			if (pkp->ciaddr.s_addr == htonl(INADDR_ANY)) {
				dhcpmsg(LOG_INFO, "No destination IP \
address or network IP address; cannot send reply to client: %s.\n",
				    disp_cid(plp));
				return (0);
			}
			to.s_addr = pkp->ciaddr.s_addr;
		} else
			to.s_addr = pkp->yiaddr.s_addr;
	}

	return (send_reply(ifp, pkp, plp->len, &to));
}
