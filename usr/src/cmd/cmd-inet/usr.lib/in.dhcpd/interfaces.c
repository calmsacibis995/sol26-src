#ident  "@(#)interfaces.c	1.72	96/11/04 SMI"

/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <stropts.h>
#include <sys/dlpi.h>
#include <stdio.h>
#include <ctype.h>
#include <syslog.h>
#include <netinet/dhcp.h>
#include "dhcpd.h"
#include "per_network.h"
#include "interfaces.h"
#include "pf.h"
#include <locale.h>

static int strioctl(int, int, int, int, char *);
static int dev_ppa(char *);
static char *device_path(char *);
static IF *find_ie_using_fd(int);
static int get_packets();
static void link_pkt_to_if(IF *, struct ip *, int);
static u_short udp_chksum(char *);
static void disp_if(IF *);

/*
 * Network interface configuration. This file contains routines which
 * handle the input side of the DHCP/BOOTP/Relay agent. Multiple interfaces
 * are handled by identifying explicitly each interface, and creating a
 * stream for each. If only one usable interface exists, then a "normal"
 * UDP socket is used for simplicity's sake.
 */

IF	*if_head;		/* head of interfaces list */
char	*interfaces;		/* user specified interfaces */
static int	num_interfaces;	/* # of usable interfaces on the system */
static struct pollfd pfd[MAXIFS]; 	/* fd's of interesting if's */

extern struct packetfilt dhcppf;	/* packet filter in pf.c */

/*
 * RFC 768 pseudo header. Used in calculating UDP checksums.
 */
static struct pseudo_udp {
	struct in_addr	src;
	struct in_addr	dst;
	u_char		notused;	/* always zero */
	u_char		proto;		/* protocol used */
	u_short		len;		/* UDP len */
	struct udphdr	hdr;		/* UDP header */
} udp_ck;

/*
 * DLPI routines we need.
 */
extern int dlinforeq(int, dl_info_ack_t *);
extern int dlattachreq(int, u_long);
extern int dldetachreq(int);
extern int dlbindreq(int, u_long, u_long, u_short, u_short);
extern int dlunbindreq(int);

/*
 * Queries the IP transport layer for configured interfaces. Those that
 * are acceptable for use by our daemon have these characteristics:
 *
 *	Not loopback
 *	Is UP
 *
 * Returns: 0 for success, the appropriate errno on failure.
 *
 * Notes: Code gleaned from the in.rarpd, solaris 2.2.
 */
int
find_interfaces(void)
{
	register int		i;
	register int		ip;
	register int		reqsize;
	u_short			mtu_tmp;
	int			numifs;
	struct ifreq		*reqbuf, *ifr;
	struct ifconf		ifconf;
	IF			*ifp, *if_tail;
	struct sockaddr_in	*sin;
	char			*user_if[MAXIFS];
	register ENCODE 	*hecp;

	if ((ip = open("/dev/ip", 0)) < 0) {
		dhcpmsg(LOG_ERR, "Error: opening /dev/ip: %s\n",
		    strerror(errno));
		return (1);
	}

#ifdef	SIOCGIFNUM
	if (ioctl(ip, SIOCGIFNUM, (char *)&numifs) < 0)
		numifs = MAXIFS;
#else
	numifs = MAXIFS;
#endif	/* SIOCGIFNUM */

	reqsize = numifs * sizeof (struct ifreq);
	/* LINTED [smalloc()/malloc returns longword aligned addresses] */
	reqbuf = (struct ifreq *)smalloc(reqsize);

	ifconf.ifc_len = reqsize;
	ifconf.ifc_buf = (caddr_t)reqbuf;

	if (ioctl(ip, SIOCGIFCONF, (char *)&ifconf) < 0) {
		dhcpmsg(LOG_ERR,
		    "Error getting network interface information: %s\n",
		    strerror(errno));
		free((char *)reqbuf);
		return (1);
	}

	/*
	 * Verify that user specified interfaces are valid.
	 */
	if (interfaces != NULL) {
		for (i = 0; i < MAXIFS; i++) {
			user_if[i] = strtok(interfaces, ",");
			if (user_if[i] == NULL)
				break;		/* we're done */
			interfaces = NULL; /* for next call to strtok() */

			for (ifr = ifconf.ifc_req; ifr < &ifconf.ifc_req[
			    ifconf.ifc_len / sizeof (struct ifreq)]; ifr++) {
				if (strcmp(user_if[i], ifr->ifr_name) == 0)
					break;
			}
			if (ifr->ifr_name[0] == '\0') {
				dhcpmsg(LOG_ERR,
				    "Invalid network interface:  %s\n",
				    user_if[i]);
				free((char *)reqbuf);
				return (1);
			}
		}
		if (i < MAXIFS)
			user_if[i] = NULL;
	} else
		user_if[0] = NULL;

	/*
	 * For each interface, build an interface structure.
	 */
	if_tail = if_head;
	for (ifr = ifconf.ifc_req;
	    ifr < &ifconf.ifc_req[ifconf.ifc_len / sizeof (struct ifreq)];
	    ifr++) {
		if (ioctl(ip, SIOCGIFFLAGS, (char *)ifr) < 0) {
			dhcpmsg(LOG_ERR, "Error encountered getting network \
interface flags: %s\n", strerror(errno));
			free((char *)reqbuf);
			return (1);
		}
		if ((ifr->ifr_flags & IFF_LOOPBACK) ||
		    !(ifr->ifr_flags & IFF_UP))
			continue;

		num_interfaces++;	/* all possible interfaces counted */

		/*
		 * If the user specified a list of interfaces,
		 * we'll only consider the ones specified.
		 */
		if (user_if[0] != NULL) {
			for (i = 0; i < MAXIFS; i++) {
				if (user_if[i] == NULL)
					break; /* skip this interface */
				if (strcmp(user_if[i], ifr->ifr_name) == 0)
					break;	/* user wants this one */
			}
			if (i == MAXIFS || user_if[i] == NULL)
				continue;	/* skip this interface */
		}

		/* LINTED [smalloc returns longword aligned addresses] */
		ifp = (IF *)smalloc(sizeof (IF));
		if (!if_tail)
			if_tail = if_head = ifp;
		else
			if_tail->next = ifp;

		(void) strcpy(ifp->nm, ifr->ifr_name);

		/* flags */
		ifp->flags = ifr->ifr_flags;

		/*
		 * Broadcast address. Not valid for POINTOPOINT
		 * connections.
		 */
		if ((ifp->flags & IFF_POINTOPOINT) == 0) {
			if (ifp->flags & IFF_BROADCAST) {
				if (ioctl(ip, SIOCGIFBRDADDR,
				    (caddr_t)ifr) < 0) {
					dhcpmsg(LOG_ERR, "Error encountered \
getting network interface broadcast address: %s\n", strerror(errno));
					free((char *)reqbuf);
					return (1);
				}
				/* LINTED [alignment ok] */
				sin = (struct sockaddr_in *)&ifr->ifr_addr;
				ifp->bcast = sin->sin_addr;
			} else
				ifp->bcast.s_addr = htonl(INADDR_ANY);

			hecp = make_encode(CD_BROADCASTADDR,
			    sizeof (struct in_addr), (void *)&ifp->bcast, 1);
			ifp->ecp = hecp;
		} else
			hecp = NULL;

		/* Subnet mask */
		if (ioctl(ip, SIOCGIFNETMASK, (caddr_t)ifr) < 0) {
			dhcpmsg(LOG_ERR, "Error encountered getting network \
interface netmask: %s\n", strerror(errno));
			free((char *)reqbuf);
			return (1);
		}
		/* LINTED [alignment ok] */
		sin = (struct sockaddr_in *)&ifr->ifr_addr;
		ifp->mask = sin->sin_addr;
		if (hecp == NULL) {
			hecp = make_encode(CD_SUBNETMASK,
			    sizeof (struct in_addr), (void *)&ifp->mask, 1);
			ifp->ecp = hecp;
		} else {
			hecp->next = make_encode(CD_SUBNETMASK,
			    sizeof (struct in_addr), (void *)&ifp->mask, 1);
			hecp = hecp->next;
		}

		/* Address */
		if (ioctl(ip, SIOCGIFADDR, (caddr_t)ifr) < 0) {
			dhcpmsg(LOG_ERR, "Error encountered getting network \
interface address: %s\n", strerror(errno));
			free((char *)reqbuf);
			return (1);
		}
		/* LINTED [alignment ok] */
		sin = (struct sockaddr_in *)&ifr->ifr_addr;
		ifp->addr = sin->sin_addr;

		/* MTU */
		if (ioctl(ip, SIOCGIFMTU, (caddr_t)ifr) < 0) {
			dhcpmsg(LOG_ERR, "Error encountered getting network \
interface MTU: %s\n", strerror(errno));
			free((char *)reqbuf);
			return (1);
		}
		ifp->mtu = ifr->ifr_metric;
		mtu_tmp = htons(ifp->mtu);
		hecp->next = make_encode(CD_MTU, 2, (void *)&mtu_tmp, 1);
		if_tail = ifp;
	}

	free((char *)reqbuf);
	(void) close(ip);
	return (0);
}

/*
 * Based on the list generated by find_interfaces(), possibly modified by
 * user arguments, open a stream for each valid / requested interface.
 *
 * If:
 *
 *	1) Only one interface exists, open a standard bidirectional UDP
 *		socket. Note that this is different than if only ONE
 *		interface is requested (but more exist).
 *
 *	2) If more than one valid interface exists, then attach to the
 *		datalink layer, push on the packet filter and buffering
 *		modules, and wait for IP packets that contain UDP packets
 *		with port 67 (server port).
 *
 *	Comments:
 *		Using DLPI to identify the interface thru which BOOTP
 *		packets pass helps in providing the correct response.
 *		Note that I will open a socket for use in transmitting
 *		responses, suitably specifying the destination relay agent
 *		or host. Note that if I'm unicasting to the client (broadcast
 *		flag not set), that somehow I have to clue the IP layer about
 *		the client's hw address. The only way I can see doing this is
 *		making the appropriate ARP table entry.
 *
 *		The only remaining unknown is dealing with clients that
 *		require broadcasting, and multiple interfaces exist. I assume
 *		that if I specify the interface's source address when
 *		opening the socket, that a limited broadcast will be
 *		directed to the correct net, and only the correct net.
 *
 *	Returns: 0 for success, errno for failure.
 */
int
open_interfaces(void)
{
	register int		inum;
	register int		err = 0;
	register char		*devpath;
	register IF		*ifp;
	extern int		errno;
	union	DL_primitives	dl;
	struct sockaddr_in	sin;
	register int		sndsock;
	int			sockoptbuf;

	switch (num_interfaces) {
	case 0:
		dhcpmsg(LOG_ERR, "No valid network interfaces.\n");
		err =  ENOENT;
		break;
	case 1:
		/*
		 * Single valid interface.
		 */
		if_head->recvdesc = socket(AF_INET, SOCK_DGRAM, 0);
		if (if_head->recvdesc < 0) {
			dhcpmsg(LOG_ERR, "Error opening socket for \
receiving UDP datagrams: %s\n", strerror(errno));
			return (errno);
		}

		if_head->senddesc = if_head->recvdesc;
		if (setsockopt(if_head->senddesc, SOL_SOCKET, SO_BROADCAST,
		    (char *)&sockoptbuf, (int)sizeof (sockoptbuf)) < 0) {
			dhcpmsg(LOG_ERR, "Setting socket option to allow \
broadcast on send descriptor failed: %s\n", strerror(errno));
			return (errno);
		}

		/*
		 * Ideally we'd have another socket of type SOCK_DGRAM
		 * that we could send on but this doesn't seem to work
		 * because the first socket is already bound to the port.
		 */
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = INADDR_ANY;
		sin.sin_port = htons((short)IPPORT_BOOTPS);
		if (bind(if_head->recvdesc, (struct sockaddr *)&sin,
		    sizeof (sin)) < 0) {
			dhcpmsg(LOG_ERR,
			    "Error binding to UDP receive socket: %s\n",
			    strerror(errno));
			return (errno);
		}
		if_head->type = DHCP_SOCKET;

		pfd[0].fd = if_head->recvdesc;
		pfd[0].events = POLLIN | POLLPRI;
		pfd[0].revents = 0;

		/* OFFER list */
		if_head->of_head = NULL;

		/* Accounting */
		if_head->received = if_head->processed = 0;

		if (verbose)
			disp_if(if_head);
		break;
	default:
		/* Set up a SOCK_DGRAM socket for sending packets. */
		if ((sndsock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			dhcpmsg(LOG_ERR, "Error opening socket for \
sending UDP datagrams: %s\n", strerror(errno));
			return (errno);
		}

		if (setsockopt(sndsock, SOL_SOCKET, SO_BROADCAST,
		    (char *)&sockoptbuf, (int)sizeof (sockoptbuf)) < 0) {
			dhcpmsg(LOG_ERR, "Setting socket option to allow \
broadcast on send descriptor failed: %s\n", strerror(errno));
			return (errno);
		}

		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = INADDR_ANY;
		sin.sin_port = htons((short)IPPORT_BOOTPS);
		if (bind(sndsock, (struct sockaddr *)&sin,
		    sizeof (sin)) < 0) {
			dhcpmsg(LOG_ERR, "BIND: %s\n", strerror(errno));
			return (errno);
		}

		/*
		 * Multiple valid interfaces. Build DLPI receive streams for
		 * each.
		 */
		for (ifp = if_head, inum = 0; ifp != NULL;
		    ifp = ifp->next, inum++) {
			ifp->senddesc = sndsock;
			ifp->type = DHCP_DLPI;
			devpath = device_path(ifp->nm);
			if ((ifp->recvdesc = open(devpath, O_RDWR)) < 0) {
				dhcpmsg(LOG_ERR, "Open: %s, interface: %s\n",
				    strerror(errno), ifp->nm);
				return (errno);
			}

			/*
			 * Check for DLPI Version 2.
			 */
			if ((err = dlinforeq(ifp->recvdesc,
			    (dl_info_ack_t *)&dl)) != 0) {
				(void) close(ifp->recvdesc);
				return (err);
			}
			if (dl.info_ack.dl_version != DL_VERSION_2) {
				dhcpmsg(LOG_ERR,
				    "Incompatible DLPI version %d\n",
				    dl.info_ack.dl_version);
				(void) close(ifp->recvdesc);
				return (errno);
			}

			switch (dl.info_ack.dl_mac_type) {
			case DL_CSMACD:
			case DL_ETHER:
			case DL_TPB:
			case DL_TPR:
			case DL_FDDI:
			/* FALLTHRU */
				initialize_pf();
				break;
			default:
				dhcpmsg(LOG_ERR,
				    "Unsupported Mac type: 0x%x\n",
				    dl.info_ack.dl_mac_type);
				return (1);
			}

			err = dlattachreq(ifp->recvdesc, dev_ppa(ifp->nm));
			if (err != 0) {
				(void) close(ifp->recvdesc);
				return (err);
			}

			/*
			 * Push and configure the packet filtering module.
			 */
			if (ioctl(ifp->recvdesc, I_PUSH, "pfmod") < 0) {
				(void) dlunbindreq(ifp->recvdesc);
				(void) dldetachreq(ifp->recvdesc);
				(void) close(ifp->recvdesc);
				dhcpmsg(LOG_ERR, "I_PUSH: %s, on %s\n",
				    strerror(errno), devpath);
				return (errno);
			}
			if (strioctl(ifp->recvdesc, PFIOCSETF, -1,
			    sizeof (dhcppf), (char *)&dhcppf) < 0) {
				(void) dlunbindreq(ifp->recvdesc);
				(void) dldetachreq(ifp->recvdesc);
				(void) close(ifp->recvdesc);
				dhcpmsg(LOG_ERR,
				    "Error setting BOOTP packet filter.\n");
				(void) ioctl(ifp->recvdesc, I_POP, "pfmod");
				return (errno);
			}
			if ((err = dlbindreq(ifp->recvdesc,
			    (u_long)ETHERTYPE_IP, 0, DL_CLDLS, 0)) != 0) {
				(void) dldetachreq(ifp->recvdesc);
				(void) close(ifp->recvdesc);
				return (err);
			}
			pfd[inum].fd = ifp->recvdesc;
			pfd[inum].events = POLLIN | POLLPRI;
			pfd[inum].revents = 0;

			/* OFFER list */
			ifp->of_head = NULL;

			/* Accounting */
			ifp->received = ifp->processed = 0;

			if (verbose)
				disp_if(ifp);
		}
		break;
	}

	return (err);
}

/*
 * For each interesting interface in if_head, poll() for received packets.
 *
 * This routine can be called as often as desired to receive packets to
 * prevent packet loss.
 *
 * Returns 0 if successful.
 */
int
read_interfaces(int timeout)
{
	register int err;

	if ((err = poll(&pfd[0], num_interfaces, timeout)) < 0) {
		if (errno != EINTR) {
			dhcpmsg(LOG_ERR,
			    "Error: (%s) polling network interfaces.\n",
			    strerror(errno));
		} else
			errno = 0;	/* signal interupted us. */
		return (errno);
	}

	/*
	 * If err > 0, then new packets came in.
	 */
	if (err > 0)
		err = get_packets();

	return (err);
}

/*
 * If there is a packet available for an interface, a PKT structure is
 * allocated and linked to the interface structure.
 *
 * Returns 0 if successful.
 */
static int
get_packets()
{
	register int		k, err = 0;
	register u_int		verify_len;
	int			flags;
	char 			cbuf[BUFSIZ];
	register IF		*ifp, *tifp;
	struct ip		*ipp;
	struct udphdr		*udpp;
	register char		*datap;
	register u_short	ip_hlen;
	struct strbuf		ctl, data;
	union DL_primitives	*dlp;
	struct in_addr		ta;

	/*
	 * Figure out which interface the packet came in on, and check
	 * the revents.
	 */
	for (k = 0; k < num_interfaces; k++) {
		if (pfd[k].revents) {
			if ((ifp = find_ie_using_fd(pfd[k].fd)) == NULL) {
				dhcpmsg(LOG_ERR,
				    "Fatal error: bad interface list.\n");
				exit(1);
			}
			if (pfd[k].revents & (POLLERR | POLLHUP | POLLNVAL)) {
				/* Bad interface. */
				dhcpmsg(LOG_ERR,
				    "Network interface error on device: %s\n",
				    ifp->nm);
				continue;
			}
			if (!(pfd[k].revents & (POLLIN | POLLRDNORM))) {
				dhcpmsg(LOG_INFO,
				    "Unsupported event on device %s: %d\n",
				    ifp->nm, pfd[k].revents);
				continue;
			}

			switch (ifp->type) {
			case DHCP_SOCKET:
				data.buf = smalloc(ifp->mtu);
				if ((err = recv(ifp->recvdesc, data.buf,
				    ifp->mtu, 0)) < 0) {
					dhcpmsg(LOG_ERR, "Error: %s \
receiving UDP datagrams from socket\n", strerror(errno));
					free(data.buf);
					continue;
				}

				/* LINTED [buf will be long word aligned] */
				link_pkt_to_if(ifp, (struct ip *)data.buf,
				    err);
				err = 0;
				break;

			case DHCP_DLPI:
				/*
				 * We need to flush the socket we're using
				 * to send data because data will be received
				 * there also, even though we really want
				 * to use DLPI to receive packets.  (We use
				 * a socket for sending simply because we
				 * don't want to do checksums and routing
				 * ourselves!)
				 */
				(void) ioctl(ifp->senddesc, I_FLUSH, FLUSHR);
				flags = 0;
				ctl.maxlen = sizeof (cbuf);
				ctl.len = 0;
				ctl.buf = &cbuf[0];

				data.maxlen = ifp->mtu;
				data.len = 0;
				data.buf = smalloc(ifp->mtu);

				if ((err = getmsg(ifp->recvdesc, &ctl,
				    &data, &flags)) < 0) {
					dhcpmsg(LOG_ERR, "Error receiving \
UDP datagrams from DLPI: %s\n",
					    strerror(errno));
					free(data.buf);
					continue;
				}

				/* LINTED [buf is long word aligned] */
				dlp = (union DL_primitives *)ctl.buf;

				if (ctl.len < DL_UNITDATA_IND_SIZE ||
				    dlp->dl_primitive != DL_UNITDATA_IND) {
					dhcpmsg(LOG_ERR,
					    "Unexpected DLPI message.\n");
					free(data.buf);
					continue;
				}

				/*
				 * Checksum IP header.
				 */
				/* LINTED [smalloc lw aligns addresses] */
				ipp = (struct ip *)data.buf;
				ip_hlen = ipp->ip_hl << 2;
				if (ip_hlen < sizeof (struct ip)) {
					free(data.buf);
					continue;	/* too short */
				}

				if ((err = ip_chksum((char *)ipp,
				    ip_hlen)) != 0) {
					if (debug) {
						dhcpmsg(LOG_INFO, "Bad IP \
checksum: 0x%x != 0x%x\n",
						    ipp->ip_sum, err);
					}
					free(data.buf);
					continue;	/* bad checksum */
				}

				/*
				 * Verify that it is for us.
				 */
				ta.s_addr = ipp->ip_dst.s_addr;
				if (ta.s_addr != ifp->addr.s_addr &&
				    ta.s_addr != (u_long)0xffffffff &&
				    ta.s_addr != ifp->bcast.s_addr) {
					for (tifp = if_head; tifp != NULL;
					    tifp = tifp->next) {
						if (ta.s_addr ==
						    tifp->addr.s_addr ||
						    ta.s_addr ==
						    tifp->bcast.s_addr) {
							break;
						}
					}
					if (tifp == NULL) {
						free(data.buf);
						continue;	/* not ours */
					}
				}

				/*
				 * Checksum UDP Header plus data.
				 */
				udpp = (struct udphdr *)((u_int)data.buf +
				    sizeof (struct ip));
				datap = (char *)((u_int)udpp +
				    sizeof (struct udphdr));
				if (udpp->uh_sum != 0) {
					/* some struct copies */
					udp_ck.src = ipp->ip_src;
					udp_ck.dst = ipp->ip_dst;
					udp_ck.hdr = *udpp;
					udp_ck.proto = ipp->ip_p;
					udp_ck.len = udpp->uh_ulen;

					verify_len = data.len -
					    sizeof (struct ip);
					if (verify_len < ntohs(udp_ck.len)) {
						dhcpmsg(LOG_ERR,
						    "Bad UDP length: %d < %d\n",
						    verify_len,
						    ntohs(udp_ck.len));
						free(data.buf);
						continue;
					}

					if ((err = udp_chksum(datap)) != 0) {
						if (debug) {
							dhcpmsg(LOG_INFO,
							    "Bad UDP \
checksum:  0x%x != 0x%x, source: %s\n",
							    ntohs(udpp->uh_sum),
							    err,
							    inet_ntoa(
							    ipp->ip_src));
						}
						free(data.buf);
						continue;
					}
				}

				/*
				 * Link the packet to the interface.
				 */
				/* LINTED [buf is longword aligned] */
				link_pkt_to_if(ifp, (struct ip *)data.buf,
				    data.len);
				err = 0;
				break;

			default:
				dhcpmsg(LOG_ERR,
				    "Unsupported interface type: %d\n",
				    ifp->type);
				err = EINVAL;
				break;
			}

			if (debug) {
				dhcpmsg(LOG_INFO,
				    "Datagram received on network device: %s\n",
				    ifp->nm);
			}
		}
	}
	return (err);
}

/*
 * Because the buffer will potentially contain the ip/udp headers, we flag
 * this by setting the 'points' field to the length of the two headers so that
 * free_plp() can "do the right thing"
 */
static void
link_pkt_to_if(IF *ifp, struct ip *buf, int len)
{
	register PKT_LIST *plp, *wplp;
	register u_char hlen;

	/* LINTED [smalloc returns long word aligned addresses */
	plp = (PKT_LIST *)smalloc(sizeof (PKT_LIST));
	if (ifp->type == DHCP_DLPI) {
		plp->points = (u_char) (sizeof (struct ip) +
		    sizeof (struct udphdr));
		plp->len = len - plp->points;
		plp->pkt = (PKT *)((u_int)buf + plp->points);
	} else {
		plp->points = 0;
		plp->len = len;
		plp->pkt = (PKT *)buf;
	}

	/*
	 * Scan thru current list, and check for the same chaddr. We throw
	 * out packets from the same chaddr, considering them moldy.
	 */
	for (wplp = ifp->pkthead; wplp != NULL; wplp = wplp->next) {
		hlen = wplp->pkt->hlen;
		if (hlen == plp->pkt->hlen && memcmp(wplp->pkt->chaddr,
		    plp->pkt->chaddr, hlen) == 0) {
			if (wplp == ifp->pkthead) {
				wplp->prev = NULL;
				ifp->pkthead = wplp->next;
			} else
				wplp->prev->next = wplp->next;
			free_plp(wplp);
			++ifp->dropped;
			--npkts;
		}
	}

	/*
	 * Link the new packet to the list of packets
	 * for this interface.
	 */
	if (ifp->pkthead == NULL)
		ifp->pkthead = plp;
	else {
		ifp->pkttail->next = plp;
		plp->prev = ifp->pkttail;
	}
	ifp->pkttail = plp;

	/*
	 * Update counters
	 */
	ifp->received++;
	npkts++;
	totpkts++;
}

/*
 * Write a packet to an interface.
 *
 * Returns 0 on success otherwise an errno.
 */
int
write_interface(IF *ifp, PKT *pktp, int len, struct sockaddr_in *to)
{
	register int 		err;

	to->sin_family = AF_INET;

	if ((err = sendto(ifp->senddesc, (caddr_t)pktp, len, 0,
	    (struct sockaddr *)to, sizeof (struct sockaddr))) < 0) {
		dhcpmsg(LOG_ERR, "SENDTO: %s.\n", strerror(errno));
		return (err);
	}
	return (0);
}

/*
 * Pop any packet filters, buffering modules, close stream, mark interface
 * as uninteresting. Note that encode list is *NOT* released.
 */
int
close_interfaces(void)
{
	register IF 		*ifp;
	register PKT_LIST	*plp, *tmpp;
	register int		close_send = 0;

	for (ifp = if_head; ifp; ifp = ifp->next) {
		if (ifp->recvdesc) {
			if (verbose) {
				dhcpmsg(LOG_INFO, "Closing interface: %s\n",
				    ifp->nm);
			}
			if (ifp->type == DHCP_DLPI) {
				if (ioctl(ifp->recvdesc, I_POP,  "pfmod") < 0) {
					dhcpmsg(LOG_ERR, "Error popping \
BOOTP packet filter module.\n");
				}
				(void) dlunbindreq(ifp->recvdesc);
				(void) dldetachreq(ifp->recvdesc);
				(void) close(ifp->recvdesc);
			}
			if (close_send == 0) {
				/* same as rcv on SOCKETS */
				(void) close(ifp->senddesc);
				ifp->senddesc = 0;
				close_send = 1;
			}
			ifp->recvdesc = 0;

			/*
			 * Free outstanding packets
			 */
			plp = ifp->pkthead;
			while (plp) {
				tmpp = plp;
				plp = plp->next;
				free_plp(tmpp);
				npkts--;
			}
			ifp->pkthead = ifp->pkttail = PKT_LIST_NULL;

			/* display statistics */
			disp_if_stats(ifp);

			/*
			 * Free pending offers
			 */
			free_offers(ifp);

			ifp->received = ifp->processed = 0;
		} else {
			if (debug) {
				dhcpmsg(LOG_INFO,
				    "Uninteresting interface: %s\n", ifp->nm);
			}
		}
	}
	return (0);
}

static IF *
find_ie_using_fd(int fd)
{
	register IF *ifp;

	for (ifp = if_head; ifp; ifp = ifp->next) {
		if (ifp->recvdesc == fd)
			return (ifp);
	}
	return (NULL);
}

static int
strioctl(int fd, int cmd, int timeout, int len, char *dp)
{
	struct strioctl	si;

	si.ic_cmd = cmd;
	si.ic_timout = timeout;
	si.ic_len = len;
	si.ic_dp = dp;

	return (ioctl(fd, I_STR, &si));
}

/*
 * Convert a device id to a ppa value. From snoop.
 * e.g. "le0" -> 0
 */
static int
dev_ppa(char *device)
{
	char *p;

	p = strpbrk(device, "0123456789");
	if (p == NULL)
		return (0);
	return (atoi(p));
}

/*
 * Convert a device id to a pathname
 * e.g. "le0" -> "/dev/le"
 */
static char *
device_path(char *device)
{
	static char buff[16];
	char *p;

	(void) strcpy(buff, "/dev/");
	(void) strcat(buff, device);
	for (p = buff + (strlen(buff) - 1); p > buff; p--)
		if (isdigit(*p))
			*p = '\0';
	return (buff);
}

/*
 * display IF info.
 */
static void
disp_if(IF *ifp)
{
	dhcpmsg(LOG_INFO, "Monitoring Interface: %s ******************\n",
	    ifp->nm);
	dhcpmsg(LOG_INFO, "MTU: %d\tType: %s\n", ifp->mtu,
	    (ifp->type == DHCP_SOCKET) ? "SOCKET" : "DLPI");
	if ((ifp->flags & IFF_POINTOPOINT) == 0)
		dhcpmsg(LOG_INFO, "Broadcast: %s\n", inet_ntoa(ifp->bcast));
	dhcpmsg(LOG_INFO, "Netmask: %s\n", inet_ntoa(ifp->mask));
	dhcpmsg(LOG_INFO, "Address: %s\n\n", inet_ntoa(ifp->addr));
}

/*
 * Display IF statistics.
 */
void
disp_if_stats(IF *ifp)
{
	register int	offers = 0;
	register OFFLST	*offerp;

	dhcpmsg(LOG_CRIT, "Interface statistics for: %s **************\n",
	    ifp->nm);
	for (offerp = ifp->of_head; offerp; offerp = offerp->next)
		offers++;
	dhcpmsg(LOG_CRIT, "Pending DHCP offers: %d\n", offers);
	dhcpmsg(LOG_CRIT, "Total Packets Received: %d\n", ifp->received);
	dhcpmsg(LOG_CRIT, "Total Packets Dropped: %d\n", ifp->dropped);
	dhcpmsg(LOG_CRIT, "Total Packets Processed: %d\n", ifp->processed);
}

/*
 * Setup the arp cache so that IP address 'ia' will be temporarily
 * bound to hardware address 'ha' of length 'len'.
 *
 * Returns: 0 if the arp entry was made, 1 otherwise.
 */
int
set_arp(IF *ifp, struct in_addr *ia, u_char *ha, int len, u_char flags)
{
	register struct sockaddr_in	*si;
	struct arpreq			arpreq;
	register int			err = 0;
	char				scratch[DHCP_SCRATCH];
	int				scratch_len;

	(void) memset((caddr_t)&arpreq, 0, sizeof (arpreq));

	arpreq.arp_pa.sa_family = AF_INET;

	/* LINTED [alignment is ok] */
	si = (struct sockaddr_in *)&arpreq.arp_pa;
	si->sin_family = AF_INET;
	si->sin_addr = *ia;	/* struct copy */

	switch (flags) {
	case DHCP_ARP_ADD:
		if (debug) {
			scratch_len = DHCP_SCRATCH;
			if (octet_to_ascii(ha, len, scratch,
			    &scratch_len) != 0) {
				dhcpmsg(LOG_DEBUG, "Cannot convert ARP \
request to ASCII: %s: len: %d\n",
				    inet_ntoa(*ia), len);
			} else {
				dhcpmsg(LOG_DEBUG,
				    "Adding ARP entry: %s == %s\n",
				    inet_ntoa(*ia), scratch);
			}
		}
		arpreq.arp_flags = ATF_INUSE | ATF_COM;
		(void) memcpy(arpreq.arp_ha.sa_data, ha, len);

		if (ioctl(ifp->senddesc, SIOCSARP, (caddr_t)&arpreq) < 0) {
			dhcpmsg(LOG_ERR,
			    "ADD: Cannot modify ARP table to add: %s\n",
			    inet_ntoa(*ia));
			err = 1;
		}
		break;
	case DHCP_ARP_DEL:
		/* give it a good effort, but don't worry... */
		(void) ioctl(ifp->senddesc, SIOCDARP, (caddr_t)&arpreq);
		break;
	default:
		err = 1;
		break;
	}

	return (err);
}

/*
 * Do a one's complement checksum. From Solaris's standalone boot code.
 */
u_short
ip_chksum(char *p, u_short len)
{
	/* LINTED [IP headers will start on a short boundary at least] */
	register u_short	*sp = (u_short *)p;
	register u_long		sum = 0;

	if (len == 0)
		return (0);

	len >>= 1;
	while (len--) {
		sum += *sp++;
		if (sum >= 0x00010000L) { /* Wrap carries into low bit */
			sum -= 0x00010000L;
			sum++;
		}
	}
	return ((u_short)~sum);
}

/*
 * Perform UDP checksum as per RFC 768. From the solaris 2.X standalone
 * boot code.
 *
 * One's complement checksum of pseudo header, udp header, and data.
 */
static u_short
udp_chksum(char *data)
{
	/* variables */
	register u_long		end_hdr;
	register u_long		sum = 0L;
	register u_short	cnt;
	register u_short	*sp;
	register int		flag = 0;

	/*
	 * Start on the pseudo header. Note that pseudo_udp already takes
	 * acount for the udphdr...
	 */
	sp = (u_short *)&udp_ck;
	cnt = ntohs(udp_ck.len) + sizeof (struct pseudo_udp) -
	    sizeof (struct udphdr);
	end_hdr = (u_long)sp + (u_long)sizeof (struct pseudo_udp);

	/*
	 * If the packet is an odd length, zero the pad byte for checksum
	 * purposes (doesn't hurt data)
	 */
	if (cnt & 1) {
		data[ntohs(udp_ck.len) - sizeof (struct udphdr)] = '\0';
		cnt++;	/* make even */
	}
	cnt >>= 1;
	while (cnt--) {
		sum += *sp++;
		if (sum >= 0x00010000L) { /* Wrap carries into low bit */
			sum -= 0x00010000L;
			sum++;
		}
		if (!flag && ((u_long)sp >= end_hdr)) {
			/* LINTED [udp headers are at least short aligned] */
			sp = (u_short *)data;	/* Start on the data */
			flag = 1;
		}
	}
	return ((u_short)~sum);
}

/*
 * Address and send a BOOTP reply packet appropriately. Does right thing
 * based on BROADCAST flag. Also checks if giaddr field is set, and
 * WE are the relay agent...
 *
 * Returns: 0 for success, nonzero otherwise (fatal)
 */
int
send_reply(IF *ifp, PKT *pp, int len, struct in_addr *dstp)
{
	register int		local = 0;
	struct sockaddr_in	to;
	struct in_addr		if_in, cl_in;

	if (pp->giaddr.s_addr != 0L && ifp->addr.s_addr !=
	    pp->giaddr.s_addr) {
		/* Going thru a relay agent */
		to.sin_addr.s_addr = pp->giaddr.s_addr;
		to.sin_port = IPPORT_BOOTPS;
	} else {
		to.sin_port = IPPORT_BOOTPC;

		if (ntohs(pp->flags) & BCAST_MASK) {
			/*
			 * XXXX - what should we do if broadcast
			 * flag is set, but ptp connection?
			 */
			if (debug)
				dhcpmsg(LOG_INFO,
				    "Sending datagram to broadcast address.\n");
			to.sin_addr.s_addr = INADDR_BROADCAST;
		} else {
			/*
			 * By default, we assume unicast!
			 */
			to.sin_addr.s_addr = dstp->s_addr;

			if (debug) {
				dhcpmsg(LOG_INFO,
				    "Unicasting datagram to %s address.\n",
				    inet_ntoa(*dstp));
			}
			if (ifp->addr.s_addr == pp->giaddr.s_addr) {
				/*
				 * No doubt a reply packet which we, as
				 * the relay agent, are supposed to deliver.
				 * Local Delivery!
				 */
				local = 1;
			} else {
				/*
				 * We can't use the giaddr field to
				 * determine whether the client is local
				 * or remote. Use the client's address,
				 * our interface's address,  and our
				 * interface's netmask to make this
				 * determination.
				 */
				if_in.s_addr = ntohl(ifp->addr.s_addr);
				if_in.s_addr &= ntohl(ifp->mask.s_addr);
				cl_in.s_addr = ntohl(dstp->s_addr);
				cl_in.s_addr &= ntohl(ifp->mask.s_addr);
				if (if_in.s_addr == cl_in.s_addr)
					local = 1;
			}

			if (local) {
				/*
				 * Local delivery. If we can make an
				 * ARP entry we'll unicast.
				 */
				if ((ifp->flags & IFF_NOARP) ||
				    set_arp(ifp, dstp, pp->chaddr,
				    pp->hlen, DHCP_ARP_ADD) == 0) {
					to.sin_addr.s_addr = dstp->s_addr;
				} else {
					to.sin_addr.s_addr =
					    INADDR_BROADCAST;
				}
			}
		}
	}
	return (write_interface(ifp, pp, len, &to));
}
