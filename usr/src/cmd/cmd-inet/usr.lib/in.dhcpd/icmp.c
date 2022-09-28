#ident	"@(#)icmp.c	1.19	96/04/22	SMI"

/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/dhcp.h>
#include "dhcpd.h"
#include <locale.h>

#define	ICMP_ECHO_SIZE	(sizeof (struct icmp) + 36)
extern void dhcpmsg();

/*
 * An implementation of ICMP ECHO for use in detecting addresses already
 * in use. Address argument expected in network order. The PKT_LIST ptr
 * is a ptr to the received packet which generated the response. We look
 * at the giaddr field in deciding how long to wait for a response.
 *
 * NOTES: Not interface specific. We use our routing tables to route the
 * messages correctly, and collect responses. This may mean that we
 * receive an ICMP ECHO reply thru an interface the daemon has not been
 * directed to watch. However, I believe that *ANY* echo reply means
 * trouble, regardless of the route taken!
 *
 * Returns: TRUE if address responded, FALSE otherwise.
 */
int
icmp_echo(struct in_addr ip, PKT_LIST *plp)
{
	int			s;
	register int		retval = FALSE;
	static u_long		outpack[DHCP_SCRATCH/sizeof (u_long)];
	static u_long		inpack[DHCP_SCRATCH/sizeof (u_long)];
	struct icmp 		*icp;
	register int		sequence = 0;
	struct sockaddr_in	to, from;
	int			fromlen;
	struct ip		*ipp;
	int			icmp_identifier;
	register int		s_cnt, r_cnt;
	register u_short	ip_hlen;
	struct timeval		tv;
	register time_t		fin_time;
	static fd_set		readfd;
	register int		i;

	/*
	 * Calculate the timeout value, based on giaddr and hops fields.
	 */
	tv.tv_usec = 0;
	if (plp->pkt->giaddr.s_addr != 0) {
		if (plp->pkt->hops != 0)
			tv.tv_sec = DHCP_ICMP_TIMEOUT * plp->pkt->hops;
		else
			tv.tv_sec = DHCP_ICMP_TIMEOUT * 2;
	} else
		tv.tv_sec = (time_t)DHCP_ICMP_TIMEOUT;

	/*
	 * To be conservative, return TRUE on failure.
	 */
	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) {
		dhcpmsg(LOG_ERR, "Error opening raw socket for ICMP.\n");
		return (TRUE);
	}

	if (fcntl(s, F_SETFL, O_NDELAY) == -1) {
		dhcpmsg(LOG_ERR, "Error setting ICMP socket to no delay.\n");
		(void) close(s);
		return (TRUE);
	}

	icmp_identifier = (int)getpid() & (u_short)-1;
	outpack[10] = 0x12345678;
	icp = (struct icmp *)outpack;
	icp->icmp_code = 0;
	icp->icmp_type = ICMP_ECHO;
	icp->icmp_id = icmp_identifier;

	(void) memset((void *)&to, 0, sizeof (struct sockaddr_in));
	to.sin_family = AF_INET;
	to.sin_addr.s_addr = ip.s_addr;

	/*
	 * We make DHCP_ICMP_ATTEMPTS attempts to contact the target. We
	 * wait the same length of time for a response in both cases.
	 */
	for (i = 0, fin_time = time(NULL) + tv.tv_sec;
	    i < DHCP_ICMP_ATTEMPTS && retval != TRUE; i++, fin_time =
	    time(NULL) + tv.tv_sec) {
		icp->icmp_seq = sequence++;
		icp->icmp_cksum = 0;
		icp->icmp_cksum = ip_chksum((char *)icp, ICMP_ECHO_SIZE);

		/*
		 * Deliver our ECHO.
		 */
		s_cnt = sendto(s, (char *)outpack, ICMP_ECHO_SIZE, 0,
		    (struct sockaddr *)&to, sizeof (struct sockaddr));

		if (s_cnt < 0 || s_cnt != ICMP_ECHO_SIZE) {
			dhcpmsg(LOG_ERR, "Error sending ICMP message.\n");
			(void) close(s);
			return (TRUE);
		}

		/*
		 * Collect replies.
		 */
		while (time(NULL) < fin_time) {
			FD_ZERO(&readfd);
			FD_SET(s, &readfd);
			if (select(FD_SETSIZE, &readfd, NULL, NULL,
			    &tv) < 0) {
				if (errno != EINTR) {
					dhcpmsg(LOG_ERR,
					    "Error polling for ICMP reply.\n");
					(void) close(s);
					return (TRUE);
				}
				continue;
			}

			if (!FD_ISSET(s, &readfd))
				break;	/* no data, timeout */

			fromlen = sizeof (from);
			if ((r_cnt = recvfrom(s, (char *)inpack,
			    sizeof (inpack), 0, (struct sockaddr *)&from,
			    &fromlen)) < 0) {
				if (errno != EINTR) {
					dhcpmsg(LOG_ERR,
					    "Error receiving ICMP reply.\n");
					(void) close(s);
					return (TRUE);
				}
				continue;
			}

			ipp = (struct ip *)inpack;
			ip_hlen = ipp->ip_hl << 2;
			if (r_cnt < (int)(ip_hlen + ICMP_MINLEN))
				continue;	/* too small */
			icp = (struct icmp *)((u_int)inpack + ip_hlen);

			if (ip_chksum((char *)icp, ipp->ip_len) != 0) {
				if (debug) {
					dhcpmsg(LOG_NOTICE,
"Note: Bad checksum on incoming ICMP echo reply.\n");
				}
			}

			if (icp->icmp_type == ICMP_ECHOREPLY) {
				if (icp->icmp_id == icmp_identifier) {
					if (debug && icp->icmp_seq !=
					    (sequence - 1)) {
						dhcpmsg(LOG_NOTICE,
"ICMP sequence mismatch: %d != %d\n",
						    icp->icmp_seq,
						    sequence - 1);
					}
					retval = TRUE;
					break;
				}
			}
		}
	}

	(void) close(s);
	return (retval);
}
