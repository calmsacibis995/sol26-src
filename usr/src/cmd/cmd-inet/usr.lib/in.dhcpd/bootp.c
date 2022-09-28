#ident	"@(#)bootp.c	1.45	96/08/09 SMI"

/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <netinet/dhcp.h>
#include "dhcpd.h"
#include "per_network.h"
#include "interfaces.h"
#include "ethers.h"
#include <locale.h>

/*
 * This file contains the code which implements the BOOTP compatibility.
 */

/*
 * We are guaranteed that the packet received is a BOOTP request packet,
 * e.g., *NOT* a DHCP packet.
 *
 * Returns: 0 for success, or nonfatal errors. Only fatal error is if
 * database write fails.
 */
int
bootp_compatibility(IF *ifp, int is_1048, PKT_LIST *plp)
{
	register int		err = 0;
	register int		pkt_len;
	register int		records, write_needed, write_error = 0;
	register int		flags = 0;
	register int		no_per_net = 0;
	register PKT		*rep_pktp;
	register u_char		*optp;
	struct in_addr		netaddr, subnetaddr, tmpaddr, ciaddr;
	PN_REC			pn;
	PER_NET_DB		pndb;
	u_char			cid[DT_MAX_CID_LEN];
	u_int			cid_len;
	ENCODE			*ecp, *ethers_ecp;
	MACRO			*mp, *nmp, *cmp;
	char			network[20];

	if (determine_network(ifp, plp, &netaddr, &subnetaddr) != 0)
		return (0);

	if ((err = open_per_net(&pndb, &netaddr, &subnetaddr)) != 0) {
		if (!ethers_compat) {
			if (verbose && err == ENOENT) {
				netaddr.s_addr &= subnetaddr.s_addr;
				dhcpmsg(LOG_INFO,
"There is no %s dhcp-network table for BOOTP client's network.\n",
				    inet_ntoa(netaddr));
			}
			return (0);
		} else
			no_per_net = 1;
	}

	records = 0;

	/* don't need separate network address anymore. */
	netaddr.s_addr &= subnetaddr.s_addr;
	(void) sprintf(network, "%s", inet_ntoa(netaddr));

	get_client_id(plp, cid, &cid_len);

	if (!no_per_net) {
		/*
		 * Try to find an entry for the client. We don't care about
		 * lease info here, since a BOOTP client always has a permanent
		 * lease. We also don't care about the entry owner either,
		 * unless we end up allocating a new entry for the client.
		 */
		if ((records = lookup_per_net(&pndb, PN_CID, (void *)cid,
		    cid_len, (struct in_addr *)NULL, &pn)) < 0) {
			close_per_net(&pndb);
			return (0);
		}

		/*
		 * If the client's entry is unusable, then we just print a
		 * message, and give up. We don't try to allocate a new address
		 * to the client. We still consider F_AUTOMATIC to be ok,
		 * for compatibility reasons. We *won't* assign any more
		 * addresses of this type.
		 */
		if (records > 0 && ((pn.flags & F_UNUSABLE) || (pn.flags &
		    (F_AUTOMATIC | F_BOOTP_ONLY)) == 0)) {
			dhcpmsg(LOG_INFO, "The %1$s dhcp-network table entry \
for BOOTP client: %2$s is marked as unusable.\n",
			    network,
			    disp_cid(plp));
			close_per_net(&pndb);
			return (0);	/* not fatal */
		}
	}

	ethers_ecp = NULL;
	if (records == 0 && ethers_compat) {
		/*
		 * Ethers mode. Try to produce a pn record. Also required is
		 * a valid boot file. If we fail on either of these tasks, we'll
		 * pretend we didn't find anything in the ETHERS database.
		 *
		 * Note that we use chaddr directly here. We have to, since
		 * there is no dhcp-network database.
		 */
		records = lookup_ethers(&netaddr, &subnetaddr,
		    *(ether_addr_t *)&plp->pkt->chaddr[0], &pn);

		if (records != 0) {
			if (ethers_encode(ifp, &pn.clientip,
			    &ethers_ecp) != 0) {
				if (verbose) {
					dhcpmsg(LOG_INFO, "No bootfile \
information for Ethers client: %1$s -> %2$s. Ethers record ignored.\n",
					    disp_cid(plp),
					    inet_ntoa(pn.clientip));
				}
				records = 0;
			}
		}
		if (records != 0 && verbose) {
			dhcpmsg(LOG_INFO, "Client: %1$s IP address binding \
(%2$s) found in ETHERS database.\n",
			    disp_cid(plp), inet_ntoa(pn.clientip));
		}
	}

	/*
	 * If the client thinks it knows who it is (ciaddr), and this
	 * doesn't match our registered IP address, then display an error
	 * message and give up.
	 */
	ciaddr.s_addr = plp->pkt->ciaddr.s_addr;
	if (records > 0 && ciaddr.s_addr != 0L && pn.clientip.s_addr !=
	    ciaddr.s_addr) {
		dhcpmsg(LOG_INFO, "BOOTP client: %1$s notion of IP address \
(ciaddr = %2$s) is incorrect.\n",
		    disp_cid(plp), inet_ntoa(ciaddr));
		if (!no_per_net)
			close_per_net(&pndb);
		return (0);
	}

	/*
	 * Neither the dhcp-network table nor the ethers table had any valid
	 * mappings. Try to allocate a new one if possible.
	 */
	if (records == 0) {
		if (no_per_net) {
			/* Nothing to allocate from. */
			if (verbose) {
				dhcpmsg(LOG_INFO,
"There is no %s dhcp-network table for BOOTP client's network.\n",
				    inet_ntoa(netaddr));
			}
			return (0);
		}

		if (be_automatic == 0) {
			/*
			 * Not allowed. Sorry.
			 */
			if (verbose) {
				dhcpmsg(LOG_INFO,
"BOOTP client: %s is looking for a configuration.\n",
				    disp_cid(plp));
			}
			close_per_net(&pndb);
			return (0);
		}

		/*
		 * The client doesn't have an entry, and we are free to
		 * give out F_BOOTP_ONLY addresses to BOOTP clients.
		 */
		write_needed = 1;

		/*
		 * If the client specified an IP address, then let's check
		 * if that one is available, since we have no CID mapping
		 * registered for this client.
		 */
		if (ciaddr.s_addr != 0L) {
			tmpaddr.s_addr = ciaddr.s_addr & subnetaddr.s_addr;
			if (tmpaddr.s_addr != netaddr.s_addr) {
				dhcpmsg(LOG_INFO,
"BOOTP client: %1$s trying to boot on wrong net: %2$s\n",
				    disp_cid(plp), inet_ntoa(tmpaddr));
				close_per_net(&pndb);
				return (0);
			}
			if ((records = lookup_per_net(&pndb, PN_CLIENT_IP,
			    (void *)&ciaddr, sizeof (struct in_addr),
			    &server_ip, &pn)) < 0) {
				close_per_net(&pndb);
				return (0);
			}
			if (records > 0 && ((pn.flags & F_BOOTP_ONLY) == 0 ||
			    (pn.flags & F_UNUSABLE) || cid_len != 0)) {
				dhcpmsg(LOG_INFO,
"BOOTP client: %1$s wrongly believes it is using IP address: %2$s\n",
				    disp_cid(plp), inet_ntoa(ciaddr));
				close_per_net(&pndb);
				return (0);
			}
		}
		if (records == 0) {
			/* Still nothing. Try to pick one. */
			records = select_offer(&pndb, plp, ifp, &pn);
		}

		if (records == 0) {
			dhcpmsg(LOG_INFO,
			    "No more IP addresses for %s network.\n", network);
			close_per_net(&pndb);
			return (0);	/* not fatal */
		}

		/*
		 * check the address. But only if client doesn't
		 * know its address.
		 */
		if (ciaddr.s_addr == 0L) {
			if ((ifp->flags & IFF_NOARP) == 0) {
				(void) set_arp(ifp, &pn.clientip, NULL, 0,
				    DHCP_ARP_DEL);
			}
			if (!noping) {
				(void) sighold(SIGTERM);
				(void) sighold(SIGINT);
				(void) sighold(SIGHUP);
				if (icmp_echo(pn.clientip, plp)) {
					dhcpmsg(LOG_ERR,
"ICMP ECHO reply to address: %1$s intended for BOOTP client: %2$s\n",
					    inet_ntoa(pn.clientip),
					    disp_cid(plp));
					pn.flags |= F_UNUSABLE;
					(void) put_per_net(&pndb, &pn,
					    PN_CLIENT_IP);
					close_per_net(&pndb);
					(void) sigrelse(SIGHUP);
					(void) sigrelse(SIGINT);
					(void) sigrelse(SIGTERM);
					return (0);
				}
				(void) sigrelse(SIGHUP);
				(void) sigrelse(SIGINT);
				(void) sigrelse(SIGTERM);
			}
		}
	} else
		write_needed = 0;

	/*
	 * It is possible that the client could specify a REQUEST list,
	 * but then it would be a DHCP client, wouldn't it? Only copy the
	 * std option list, since that potentially could be changed by
	 * load_options().
	 */
	ecp = NULL;

	if (!no_dhcptab) {
		if ((nmp = get_macro(network)) != NULL)
			ecp = copy_encode_list(nmp->head);
		if ((mp = get_macro(pn.macro)) != NULL)
			ecp = combine_encodes(ecp, mp->head, ENC_DONT_COPY);
		if ((cmp = get_macro(disp_cid(plp))) != NULL)
			ecp = combine_encodes(ecp, cmp->head, ENC_DONT_COPY);
	}

	/* Add ethers "magic" data, if it exists */
	if (ethers_ecp != NULL)
		ecp = combine_encodes(ecp, ethers_ecp, ENC_DONT_COPY);

	/* Produce a BOOTP reply. */
	rep_pktp = gen_bootp_pkt(sizeof (PKT), plp->pkt);

	rep_pktp->op = BOOTREPLY;
	optp = rep_pktp->options;

	/* set the client's IP address */
	rep_pktp->yiaddr.s_addr = pn.clientip.s_addr;

	/*
	 * Omit lease time options implicitly, e. g.
	 * ~(DHCP_DHCP_CLNT | DHCP_SEND_LEASE)
	 */
	if (!is_1048)
		flags |= DHCP_NON_RFC1048;

	/* Now load in configured options */
	pkt_len = load_options(flags, rep_pktp, sizeof (PKT), optp, ecp, NULL);
	if (pkt_len < sizeof (PKT))
		pkt_len = sizeof (PKT);

	(void) sighold(SIGTERM);
	(void) sighold(SIGINT);
	(void) sighold(SIGHUP);

	(void) memcpy(pn.cid, cid, cid_len);
	pn.cid_len = cid_len;
	pn.lease = htonl(DHCP_PERM);

	if (write_needed)
		write_error = put_per_net(&pndb, &pn, PN_CLIENT_IP);
	else {
		if (verbose && !no_per_net) {
			dhcpmsg(LOG_INFO,
"Database write unnecessary for BOOTP client: %1$s, %2$s\n",
			    disp_cid(plp), inet_ntoa(pn.clientip));
		}
	}

	if (!write_needed || write_error == 0) {
		if (send_reply(ifp, rep_pktp, pkt_len,
		    &rep_pktp->yiaddr) != 0) {
			dhcpmsg(LOG_ERR, "Reply to BOOTP client %s failed.\n",
			    disp_cid(plp));
		}
	}

	(void) sigrelse(SIGHUP);
	(void) sigrelse(SIGINT);
	(void) sigrelse(SIGTERM);

	free(rep_pktp);
	free_encode_list(ecp);
	if (ethers_ecp != NULL)
		free_encode_list(ethers_ecp);
	if (!no_per_net)
		close_per_net(&pndb);
	return (0);
}
