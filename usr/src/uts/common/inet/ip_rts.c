/*
 * Copyright (c) 1992-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Copyright (c) 1988, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)rtsock.c	8.6 (Berkeley) 2/11/95
 */

#pragma ident	"@(#)ip_rts.c	1.25	96/10/14 SMI"

/*
 * This file contains routines that processes routing socket requests.
 */

#define	_IP_RTS_C

#ifndef	MI_HDRS
#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/strlog.h>
#include <sys/dlpi.h>
#include <sys/ddi.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>

#include <sys/systm.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <net/if_dl.h>

#include <inet/common.h>
#include <inet/mi.h>
#include <inet/ip.h>
#include <inet/ip_if.h>
#include <inet/ip_ire.h>
#include <inet/ip_rts.h>
#include <inet/ip_multi.h>

#else

#include <types.h>
#include <stream.h>
#include <stropts.h>
#include <strlog.h>
#include <tihdr.h>
#include <tiuser.h>
#include <dlpi.h>

#include <socket.h>
#include <if.h>
#include <route.h>
#include <in.h>

#include <common.h>
#include <mi.h>
#include <ip.h>
#include <ip_ire.h>
#include <ip_rts.h>
#include <ip_multi.h>

#endif

#define	RTS_MSG_SIZE(type, rtm_addrs) \
	(rts_data_msg_size(rtm_addrs) + rts_header_msg_size(type))
ipc_t *rts_clients = NULL;

int	ip_rts_request(queue_t *q, mblk_t *mp);
static void	rts_getifdata(if_data_t *if_data, ipif_t *ipif);
static void	rts_setgwr(ire_t *ire, ipaddr_t gw_addr);
static void	rts_setmetrics(ire_t *ire, u_long which, rt_metrics_t *metrics);
static int	rts_getmetrics(ire_t *ire, rt_metrics_t *metrics);
static int	rts_get_addrs(rt_msghdr_t *rtm, ipaddr_t *dst_addrp,
    ipaddr_t *gw_addrp, ipaddr_t *net_maskp, ipaddr_t *if_addrp,
    ipaddr_t *authorp);
static void	rts_fill_msg(int type, int rtm_addrs, ipaddr_t dst,
    ipaddr_t mask, ipaddr_t gateway, ipaddr_t src_addr, ipaddr_t brd_addr,
    ipaddr_t autho, ipif_t *ipif, mblk_t *mp);
static	mblk_t	*rts_alloc_msg(int type, int rtm_addrs, mblk_t *mp);
static	int	rts_header_msg_size(int type);
static	int	rts_data_msg_size(int rtm_addrs);
static void	rts_queue_input(mblk_t *mp, queue_t *q);
void	ip_rts_change(int type, ipaddr_t dst_addr, ipaddr_t gw_addr,
    ipaddr_t net_mask, ipaddr_t source, int flags, int error, int rtm_addrs);
void	ip_rts_ifmsg(ipif_t *ipif);
void	ip_rts_newaddrmsg(int cmd, int error, ipif_t *ipif);
void	ip_rts_rtmsg(int type, ire_t *ire, int error);

/*
 * rts_queue_input
 * Send the ack to all the routing queues.
 * In case of originating queue, send it to
 * it only if the loopback is set.
 */
static void
rts_queue_input(mp, q)
	mblk_t	*mp;
	queue_t	*q;
{
	mblk_t	*mp1;
	ipc_t	*ipc;

	for (ipc = rts_clients; ipc; ipc = ipc->ipc_hash_next) {
		/*
		 * for originating queue, if
		 * loopback is not set don't send any ack
		 */
		if ((q != NULL) && (ipc->ipc_rq == RD(q))) {
			if (ipc->ipc_loopback == 0)
				continue;
		}
		mp1 = dupmsg(mp);
		if (mp1 == NULL)
			mp1 = copymsg(mp);
		if (mp1 != NULL)
			putnext(ipc->ipc_rq, mp1);
	}
	freemsg(mp);
}

/*
 * ip_rts_rtmsg
 * Takes an ire and sends an ack to all
 * the routing sockets. This routine is used only
 * when a route is created/deleted through
 * ioctl interface.
 */
void
ip_rts_rtmsg(type, ire, error)
	int	type;
	ire_t	*ire;
	int	error;
{
	mblk_t		*mp;
	rt_msghdr_t	*rtm;
	int		rtm_addrs = (RTA_DST | RTA_NETMASK | RTA_GATEWAY);

	if (ire == NULL)
		return;
	mp = rts_alloc_msg(type, rtm_addrs, NULL);
	if (mp == NULL)
		return;
	rts_fill_msg(type, rtm_addrs, ire->ire_addr, ire->ire_mask,
	    ire->ire_gateway_addr, ire->ire_src_addr, 0, 0, NULL, mp);
	rtm = (rt_msghdr_t *)ALIGN32(mp->b_rptr);
	mp->b_wptr = (u_char *)ALIGN32(&mp->b_rptr[rtm->rtm_msglen]);
	rtm->rtm_addrs = rtm_addrs;
	rtm->rtm_flags = ire->ire_flags;
	if (error)
		rtm->rtm_errno = error;
	else
		rtm->rtm_flags |= RTF_DONE;
	rts_queue_input(mp, NULL);
}

/*
 * ip_rts_request Processes requests received
 * on Routing socket. It extracts all the arguments
 * and calls the appropriate function to process
 * the request.
 */
int
ip_rts_request(q, mp)
	queue_t	*q;
	mblk_t	*mp;
{
	rt_msghdr_t	*rtm;
	ipaddr_t	dst_addr = 0;
	ipaddr_t	gw_addr = 0;
	ipaddr_t	net_mask = 0;
	ipaddr_t	author = 0;
	ipaddr_t	if_addr = 0;
	int		error = 0;
	mblk_t		*new_mp;
	ire_t		*ire = NULL;
	int		rtm_addrs;
	rt_msghdr_t	*new_rtm;
	ipc_t		*ipc;
	int		match_flags = MATCH_IRE_DSTONLY;
	u_int		ire_type;
	ire_t		*sire = NULL;
	int		rtm_flags;

	/*
	 * The Routing Socket data starts on
	 * next block. If there is no next block
	 * this is an indication from routing module
	 * that it is a routing socket stream queue.
	 */
	ipc = (ipc_t *)q->q_ptr;
	if (mp->b_cont != NULL) {
		mp = dupmsg(mp->b_cont);
		if (mp == NULL)
			return (ENOBUFS);
	} else
		mp = NULL;

	if (mp == NULL) {
		/*
		 * This is a message from RTS module
		 * indicating that this is a Routing Socket
		 * Stream. Insert this ipc in routing
		 * socket client list.
		 */
		ipc->ipc_loopback = 1;
		ipc_hash_insert_last(&rts_clients, ipc);
		return (0);
	}

	rtm = (rt_msghdr_t *)ALIGN32(mp->b_rptr);
	if (rtm->rtm_version != RTM_VERSION) {
		ip0dbg(("ip_rts_request: Invalid version %d ",
			rtm->rtm_version));
		error = EPROTONOSUPPORT;
		goto bad;
	}
	if ((u_short)(mp->b_wptr - mp->b_rptr) < rtm->rtm_msglen) {
		if (!pullupmsg(mp, rtm->rtm_msglen)) {
			ip0dbg(("ip_rts_request: Invalid message size %d",
				rtm->rtm_msglen));
			error = EINVAL;
			goto bad;
		}
	}
	error = rts_get_addrs(rtm, &dst_addr, &gw_addr, &net_mask, &if_addr,
	    &author);
	if (error != 0)
		goto bad;
	if ((rtm->rtm_addrs & RTA_DST) == 0) {
		error = EINVAL;
		goto bad;
	}
	switch (rtm->rtm_type) {
	case RTM_ADD:
		/* if we are adding a route, gateway is a must */
		if ((rtm->rtm_addrs & RTA_GATEWAY) == 0) {
			error = EINVAL;
			goto bad;
		}

		/* Determine netmask */
		if (((rtm->rtm_addrs & RTA_NETMASK) == 0) ||
		    (rtm->rtm_flags & RTF_HOST)) {
			net_mask = IP_HOST_MASK;
			ire_type = IRE_HOST;
		} else if (dst_addr == 0) {
			ire_type = IRE_DEFAULT;
			net_mask = 0;
		} else
			ire_type = IRE_PREFIX;

		error = ip_rt_add(dst_addr, net_mask, gw_addr, ire_type,
		    rtm->rtm_flags, false);
		if (error != 0)
			break;

		/*
		 * Retrieve the IRE just created so that its associated metrics
		 * can be set.
		 */
		if (rtm->rtm_addrs & RTA_NETMASK)
			match_flags |= MATCH_IRE_MASK;
		match_flags |= MATCH_IRE_GW;
		ire = ire_ftable_lookup(dst_addr, net_mask, gw_addr, 0, NULL,
		    NULL, NULL, match_flags);
		if (ire != NULL)
			rts_setmetrics(ire, rtm->rtm_inits, &rtm->rtm_rmx);
		break;
	case RTM_DELETE:
		/* if we are deleting a route, gateway is a must */
		if ((rtm->rtm_addrs & RTA_GATEWAY) == 0) {
			error = EINVAL;
			goto bad;
		}

		/*
		 * it can come in the form of
		 * destination, gateway (longest match will be deleted)
		 * destination, mask, gateway.
		 */
		if (dst_addr == 0)
			net_mask = 0;

		error = ip_rt_delete(dst_addr, net_mask, gw_addr,
		    rtm->rtm_addrs, rtm->rtm_flags, false);
		break;
	case RTM_GET:
	case RTM_CHANGE:
		/*
		 * If a netmask wasn't supplied, or if it was and it is
		 * IP_HOST_MASK, then first check for an IRE_LOOPBACK or
		 * IRE_LOCAL entry.
		 */
		if (((rtm->rtm_addrs & RTA_NETMASK) == 0) ||
		    (net_mask == IP_HOST_MASK)) {
			ire = ire_ctable_lookup(dst_addr, gw_addr, 0, NULL,
			    NULL, MATCH_IRE_GW);
			if (ire && ire->ire_type != IRE_LOOPBACK &&
			    ire->ire_type != IRE_LOCAL)
				ire = NULL;
		}

		/*
		 * If we didn't check for or find an IRE_LOOPBACK or IRE_LOCAL
		 * entry, then look in the forwarding table.
		 */
		if (ire == NULL) {
			/*
			 * In case of RTM_CHANGE, you should not
			 * use gateway passed for looking up as it is
			 * the new gateway.
			 *
			 * In case of RTM_GET, you should search recursively
			 * and match MATCH_IRE_DEFAULT if there isn't a
			 * specfic match.
			 */
			if (rtm->rtm_addrs & RTA_NETMASK)
				match_flags |= MATCH_IRE_MASK;
			if (rtm->rtm_addrs & RTA_GATEWAY)
				match_flags |= MATCH_IRE_GW;
			if (rtm->rtm_type == RTM_CHANGE)
				match_flags &= ~MATCH_IRE_GW;
			else
				match_flags |=
				    (MATCH_IRE_DEFAULT | MATCH_IRE_RECURSIVE);
			ire = ire_ftable_lookup(dst_addr, net_mask, gw_addr, 0,
			    NULL, &sire, NULL, match_flags);
		}

		if (ire == NULL) {
			error = ESRCH;
			goto bad;
		}
		/* we know the IRE before we come here */
		switch (rtm->rtm_type) {
		case RTM_GET:
			ASSERT(ire->ire_ipif != NULL);
			/*
			 * Always return RTA_DST, RTA_GATEWAY and RTA_NETMASK.
			 *
			 * The 4.4BSD-Lite2 code (net/rtsock.c) returns both
			 * RTA_IFP and RTA_IFA if either is defined, and also
			 * returns RTA_BRD if the appropriate interface is
			 * point-to-point.
			 */
			rtm_addrs = (RTA_DST | RTA_GATEWAY | RTA_NETMASK);
			if (rtm->rtm_addrs & (RTA_IFP | RTA_IFA)) {
				rtm_addrs |= (RTA_IFP | RTA_IFA);
				if (ire->ire_ipif->ipif_flags & IFF_POINTOPOINT)
					rtm_addrs |= RTA_BRD;
			}

			new_mp = rts_alloc_msg(RTM_GET, rtm_addrs, mp);
			if (new_mp == NULL) {
				error = ENOBUFS;
				goto bad;
			}
			/*
			 * We set the destination address, gateway address,
			 * netmask and flags in the RTM_GET response depending
			 * on whether we found a parent IRE or not.
			 * In particular, if we did find a parent IRE during the
			 * recursive search, use that IRE's gateway address.
			 * Otherwise, we use the IRE's source address for the
			 * gateway address.
			 */
			if (sire == NULL) {
				dst_addr = ire->ire_addr;
				gw_addr = ire->ire_src_addr;
				net_mask = ire->ire_mask;
				rtm_flags = ire->ire_flags;
			} else {
				dst_addr = sire->ire_addr;
				gw_addr = sire->ire_gateway_addr;
				net_mask = sire->ire_mask;
				rtm_flags = sire->ire_flags;
			}
			rts_fill_msg(RTM_GET, rtm_addrs, dst_addr, net_mask,
				gw_addr, ire->ire_src_addr,
				ire->ire_ipif->ipif_pp_dst_addr, 0,
				ire->ire_ipif, mp);
			new_rtm = (rt_msghdr_t *)ALIGN32(new_mp->b_rptr);
			/*
			 * The rtm_msglen, rtm_version and rtm_type fields in
			 * RTM_GET response are filled in by rts_fill_msg.
			 *
			 * rtm_addrs and rtm_flags are filled in based on what
			 * was requested and the state of the IREs looked up
			 * above.
			 *
			 * We copy the others from the request message if the
			 * message pointers are different.
			 *
			 * TODO: rtm_index and rtm_use should probably be
			 * filled in with something resonable here and not just
			 * copied from the request.
			 */
			if (new_rtm != rtm) {
				new_rtm->rtm_index = rtm->rtm_index;
				new_rtm->rtm_pid = rtm->rtm_pid;
				new_rtm->rtm_seq = rtm->rtm_seq;
				new_rtm->rtm_use = rtm->rtm_use;
			}
			new_rtm->rtm_addrs = rtm_addrs;
			new_rtm->rtm_flags = rtm_flags;
			if (mp != new_mp)
				freemsg(mp);
			rtm = new_rtm;
			if (sire == NULL)
				rtm->rtm_inits = rts_getmetrics(ire,
				    &rtm->rtm_rmx);
			else
				rtm->rtm_inits = rts_getmetrics(sire,
				    &rtm->rtm_rmx);
			mp = new_mp;
			break;
		case RTM_CHANGE:
			if (gw_addr)
				rts_setgwr(ire, gw_addr);
			rts_setmetrics(ire, rtm->rtm_inits, &rtm->rtm_rmx);
			break;
		}
		break;
	default:
		error = EOPNOTSUPP;
	}
bad:
	if (rtm != NULL) {
		mp->b_wptr = &mp->b_rptr[rtm->rtm_msglen];
		ASSERT(mp->b_wptr <= mp->b_datap->db_lim);
		if (error) {
			rtm->rtm_errno = error;
			/* Send error ACK */
			ip1dbg(("ip_routing_req: error %d\n", error));
		} else {
			rtm->rtm_flags |= RTF_DONE;
			/* OK ACK already set up by caller except this */
			ip2dbg(("ip_routing_req: OK ACK\n"));
		}
		rts_queue_input(mp, q);
	}
	return (error);
}

/*
 * rts_setgwr
 * If necessary change the
 * gateway and cleanup the
 * cache.
 */
static void
rts_setgwr(ire, gw_addr)
	ire_t		*ire;
	ipaddr_t	gw_addr;
{

	/*
	 * Flush the cache that is associated with
	 * this ire.
	 */
	if (ire->ire_gateway_addr != gw_addr) {
		ire_flush_cache(ire, IRE_FLUSH_DELETE);
		ire->ire_gateway_addr = gw_addr;
	}
}

/*
 * rts_setifdata
 * Fill the given data structure
 * with interface statistics.
 */
static void
rts_getifdata(if_data, ipif)
	if_data_t	*if_data;
	ipif_t		*ipif;
{
	if_data->ifi_type = ipif->ipif_type;	/* ethernet, tokenring, etc */
	if_data->ifi_addrlen = 0;		/* media address length */
	if_data->ifi_hdrlen = 0;		/* media header length */
	if_data->ifi_mtu = ipif->ipif_mtu;	/* maximum transmission unit */
	if_data->ifi_metric = ipif->ipif_metric; /* metric (external only) */
	if_data->ifi_baudrate = 0;		/* linespeed */

	if_data->ifi_ipackets = 0;		/* packets received on if */
	if_data->ifi_ierrors = 0;		/* input errors on interface */
	if_data->ifi_opackets = 0;		/* packets sent on interface */
	if_data->ifi_oerrors = 0;		/* output errors on if */
	if_data->ifi_collisions = 0;		/* collisions on csma if */
	if_data->ifi_ibytes = 0;		/* total number received */
	if_data->ifi_obytes = 0;		/* total number sent */
	if_data->ifi_imcasts = 0;		/* multicast packets received */
	if_data->ifi_omcasts = 0;		/* multicast packets sent */
	if_data->ifi_iqdrops = 0;		/* dropped on input */
	if_data->ifi_noproto = 0;		/* destined for unsupported */
						/* protocol. */
}

/*
 * rts_setmetric, we set them
 * here on forwarding table routes
 * but they are never propagated
 * to the cache route. We just them
 * here only to support 4.4BSD interface.
 */
static void
rts_setmetrics(ire, which, metrics)
	ire_t		*ire;
	u_long		which;
	rt_metrics_t	*metrics;
{
	if (which & RTV_RTT)
		/*
		 * ire_rtt is in milliseconds, but 4.4BSD-Lite2's <net/route.h>
		 * says: rmx_rtt and rmx_rttvar are stored as microseconds
		 */
		ire->ire_rtt = metrics->rmx_rtt / 1000;
	if (which & RTV_MTU)
		ire->ire_max_frag = metrics->rmx_mtu;
}

/*
 * rts_getmetrics gets the metrics
 * from ire
 */
static int
rts_getmetrics(ire, metrics)
	ire_t		*ire;
	rt_metrics_t	*metrics;
{
	int		metrics_set = 0;

	bzero((char *)metrics, sizeof (rt_metrics_t));
	/*
	 * ire_rtt is in milliseconds, but 4.4BSD-Lite2's <net/route.h>
	 * says: rmx_rtt and rmx_rttvar are stored as microseconds
	 */
	metrics->rmx_rtt = ire->ire_rtt * 1000;
	metrics_set |= RTV_RTT;
	metrics->rmx_mtu = ire->ire_max_frag;
	metrics_set |= RTV_MTU;
	return (metrics_set);
}

/*
 * rts_get_addrs
 * Takes a pointer to routing message and
 * extracts necessary info by looking at rtm->rtm_addrs
 * bits and returns them in the pointers passed.
 * For the present we ignore RTA_IFP.
 */
static int
rts_get_addrs(rtm, dst_addrp, gw_addrp, net_maskp, if_addrp, authorp)
	rt_msghdr_t	*rtm;
	ipaddr_t	*dst_addrp;
	ipaddr_t	*gw_addrp;
	ipaddr_t	*net_maskp;
	ipaddr_t	*if_addrp;
	ipaddr_t	*authorp;
{
	struct	sockaddr *sa;
	int	i;
	int	addr_bits;
	caddr_t	cp;
	int	length;

	/*
	 * At present we handle only,
	 * RTA_DST, RTA_GATEWAY, RTA_NETMASK,
	 * RTA_IFA and RTA_AUTHOR. The rest will be
	 * added as we need them.
	 */
	cp = (caddr_t)&rtm[1];
	length = rtm->rtm_msglen;
	for (i = 0; (i < RTA_NUMBITS) && ((cp - (caddr_t)rtm) < length); i++) {
		sa = (struct sockaddr *) cp;
		addr_bits = (rtm->rtm_addrs & (1 << i));
		if (addr_bits == 0)
			continue;
		switch (sa->sa_family) {
		case AF_INET:
		default:
			switch (addr_bits) {
			case RTA_DST:
				*dst_addrp =
				    ((struct sockaddr_in *)sa)->sin_addr.s_addr;
				break;
			case RTA_GATEWAY:
				*gw_addrp =
				    ((struct sockaddr_in *)sa)->sin_addr.s_addr;
				break;
			case RTA_NETMASK:
				*net_maskp =
				    ((struct sockaddr_in *)sa)->sin_addr.s_addr;
				break;
			case RTA_IFA:
				*if_addrp =
				    ((struct sockaddr_in *)sa)->sin_addr.s_addr;
				break;
			case RTA_AUTHOR:
				*authorp =
				    ((struct sockaddr_in *)sa)->sin_addr.s_addr;
				break;
			default:
				return (EINVAL);
			}
			cp += sizeof (struct sockaddr_in);
			break;
		case AF_LINK:
			switch (addr_bits) {
			case RTA_IFP:
				break;
			default:
				return (EINVAL);
			}
			cp += sizeof (struct sockaddr_dl);
			break;
		}
	}
	return (0);
}

/*
 * rts_fill_msg Fills the message with the
 * given info.
 */
static void
rts_fill_msg(type, rtm_addrs, dst, mask, gateway, src_addr, brd_addr, autho,
    ipif, mp)
	int		type;
	int		rtm_addrs;
	ipaddr_t	dst;
	ipaddr_t	mask;
	ipaddr_t	gateway;
	ipaddr_t	src_addr;
	ipaddr_t	brd_addr;
	ipaddr_t	autho;
	ipif_t		*ipif;
	mblk_t		*mp;
{
	rt_msghdr_t	*rtm;
	struct		sockaddr_in *sa;
	int		data_size, header_size;
	u_char		*cp;
	int		i, size;

	ASSERT(mp != NULL);
	/*
	 * First find the type of the message
	 * and its length.
	 */
	header_size = rts_header_msg_size(type);
	/*
	 * Now find the size of the data
	 * that follows the message header.
	 */
	data_size = rts_data_msg_size(rtm_addrs);

	rtm = (rt_msghdr_t *)ALIGN32(mp->b_rptr);
	mp->b_wptr = &mp->b_rptr[header_size];
	cp = (u_char *)ALIGN32(mp->b_wptr);
	bzero((char *)cp, data_size);
	for (i = 0; i < RTA_NUMBITS; i++) {
		sa = (struct sockaddr_in *)cp;
		switch (rtm_addrs & (1 << i)) {
		case RTA_DST:
			sa->sin_addr.s_addr = dst;
			sa->sin_family = AF_INET;
			cp += sizeof (struct sockaddr_in);
			break;
		case RTA_GATEWAY:
			sa->sin_addr.s_addr = gateway;
			sa->sin_family = AF_INET;
			cp += sizeof (struct sockaddr_in);
			break;
		case RTA_NETMASK:
			sa->sin_addr.s_addr = mask;
			sa->sin_family = AF_INET;
			cp += sizeof (struct sockaddr_in);
			break;
		case RTA_IFA:
			sa->sin_addr.s_addr = src_addr;
			sa->sin_family = AF_INET;
			cp += sizeof (struct sockaddr_in);
			break;
		case RTA_IFP:
			size = ill_dls_info((struct sockaddr_dl *)cp, ipif);
			cp += size;
			break;
		case RTA_AUTHOR:
			sa->sin_addr.s_addr = autho;
			sa->sin_family = AF_INET;
			cp += sizeof (struct sockaddr_in);
			break;
		case RTA_BRD:
			sa->sin_addr.s_addr = brd_addr;
			sa->sin_family = AF_INET;
			cp += sizeof (struct sockaddr_in);
			break;
		}
	}
	mp->b_wptr = cp;
	mp->b_cont = nilp(mblk_t);
	/*
	 * set the fields that are common to
	 * to different messages.
	 */
	rtm->rtm_msglen = header_size + data_size;
	rtm->rtm_version = RTM_VERSION;
	rtm->rtm_type = (u_char)type;
}

/*
 * rts_alloc_msg Allocates a message.  if a message is passed
 * through mp, it will try to use it, if it is smaller
 * than what it needs, it will alloc a new message.
 */
static mblk_t *
rts_alloc_msg(type, rtm_addrs, mp)
	int	type;
	int	rtm_addrs;
	mblk_t	*mp;
{
	int	length;
	u_char	*cp;

	length = RTS_MSG_SIZE(type, rtm_addrs);
	if (mp == NULL) {
		mp = allocb(length, BPRI_MED);
		if (mp == NULL)
			return (mp);
		cp = (u_char *)ALIGN32(mp->b_rptr);
		bzero((char *)cp, length);
	} else
		mp = mi_reallocb(mp, length);
	return (mp);
}

/*
 * Returns the size of the routing
 * socket message header size.
 */
int
rts_header_msg_size(type)
	int	type;
{
	switch (type) {
	case RTM_DELADDR:
	case RTM_NEWADDR:
		return (sizeof (ifa_msghdr_t));
	case RTM_IFINFO:
		return (sizeof (if_msghdr_t));
	default:
		return (sizeof (rt_msghdr_t));
	}
}

/*
 * Returns the size of the message
 * needed with the given rtm_addr
 */
int
rts_data_msg_size(rtm_addrs)
	int	rtm_addrs;
{
	int	i;
	int	length = 0;

	for (i = 0; i < RTA_NUMBITS; i++) {
		switch (rtm_addrs & (1 << i)) {
		case RTA_IFP:
			length += sizeof (struct sockaddr_dl);
			break;
		case RTA_DST:
		case RTA_GATEWAY:
		case RTA_NETMASK:
		case RTA_IFA:
		case RTA_AUTHOR:
		case RTA_BRD:
			length += sizeof (struct sockaddr_in);
			break;
		}
	}
	return (length);
}

/*
 * This routine is called to generate a message to the routing
 * socket indicating that a redirect has occured, a routing lookup
 * has failed, or that a protocol has detected timeouts to a particular
 * destination. This routine is called for message types RTM_LOSING,
 * RTM_REDIRECT, and RTM_MISS.
 */
void
ip_rts_change(type, dst_addr, gw_addr, net_mask, source, flags, error,
    rtm_addrs)
	int		type;
	ipaddr_t	dst_addr;
	ipaddr_t	gw_addr;
	ipaddr_t	net_mask;
	ipaddr_t	source;
	int		flags;
	int		error;
	int		rtm_addrs;
{
	rt_msghdr_t	*rtm;
	mblk_t		*mp;

	if (rtm_addrs == 0)
		return;
	mp = rts_alloc_msg(type, rtm_addrs, NULL);
	if (mp == NULL)
		return;
	rts_fill_msg(type, rtm_addrs, dst_addr, net_mask, gw_addr, source, 0, 0,
	    NULL, mp);
	rtm = (rt_msghdr_t *)ALIGN32(mp->b_rptr);
	rtm->rtm_flags = flags;
	rtm->rtm_errno = error;
	rtm->rtm_flags |= RTF_DONE;
	rtm->rtm_addrs = rtm_addrs;
	rts_queue_input(mp, NULL);
}

/*
 * This routine is called to generate a message to the routing
 * socket indicating that the status of a network interface has changed.
 * Message type generated RTM_IFINFO.
 */
void
ip_rts_ifmsg(ipif)
	ipif_t		*ipif;
{
	if_msghdr_t	*ifm;
	mblk_t		*mp;

	/*
	 * This message should be generated only
	 * when the physical device is changing
	 * state.
	 */
	if (ipif->ipif_id != 0)
		return;
	mp = rts_alloc_msg(RTM_IFINFO, RTA_IFP, NULL);
	if (mp == NULL)
		return;
	rts_fill_msg(RTM_IFINFO, RTA_IFP, 0, 0, 0, 0, 0, 0, ipif, mp);
	ifm = (if_msghdr_t *)ALIGN32(mp->b_rptr);
	ifm->ifm_index = ipif->ipif_index;
	ifm->ifm_flags = ipif->ipif_flags;
	rts_getifdata(&ifm->ifm_data, ipif);
	ifm->ifm_addrs = RTA_IFP;
	rts_queue_input(mp, NULL);
}

/*
 * This is called to generate messages to the routing socket
 * indicating a network interface has had addresses associated with it.
 * The structure of the code is based on the 4.4BSD-Lite2 <net/rtsock.c>.
 */
void
ip_rts_newaddrmsg(cmd, error, ipif)
	int	cmd;
	int	error;
	ipif_t	*ipif;
{
	int		pass;
	int		ncmd;
	mblk_t		*mp;
	ifa_msghdr_t	*ifam;
	ipaddr_t	brdaddr = 0;
	rt_msghdr_t	*rtm;
	int		rtm_addrs;

	/*
	 * If the request is DELETE, send RTM_DELETE and RTM_DELADDR.
	 * if the request is ADD, send RTM_NEWADDR and RTM_ADD.
	 */
	for (pass = 1; pass < 3; pass++) {
		if ((cmd == RTM_ADD && pass == 1) ||
		    (cmd == RTM_DELETE && pass == 2)) {
			ncmd = ((cmd == RTM_ADD) ? RTM_NEWADDR : RTM_DELADDR);
			brdaddr = ipif->ipif_pp_dst_addr;

			rtm_addrs = (RTA_IFA | RTA_NETMASK | RTA_BRD);
			mp = rts_alloc_msg(ncmd, rtm_addrs, NULL);
			if (mp == NULL)
				continue;
			rts_fill_msg(ncmd, rtm_addrs, 0, ipif->ipif_net_mask, 0,
			    ipif->ipif_local_addr, brdaddr, 0, NULL, mp);
			ifam = (ifa_msghdr_t *)ALIGN32(mp->b_rptr);
			ifam->ifam_index = ipif->ipif_index;
			ifam->ifam_metric = ipif->ipif_metric;
			ifam->ifam_flags = ((cmd == RTM_ADD) ? RTF_UP : 0);
			ifam->ifam_addrs = rtm_addrs;
			rts_queue_input(mp, NULL);
		}
		if ((cmd == RTM_ADD && pass == 2) ||
		    (cmd == RTM_DELETE && pass == 1)) {
			rtm_addrs = (RTA_DST | RTA_NETMASK);
			mp = rts_alloc_msg(cmd, rtm_addrs, NULL);
			if (mp == NULL)
				continue;
			rts_fill_msg(cmd, rtm_addrs, ipif->ipif_local_addr,
			    ipif->ipif_net_mask, 0, 0, 0, 0, NULL, mp);
			rtm = (rt_msghdr_t *)ALIGN32(mp->b_rptr);
			rtm->rtm_index = ipif->ipif_index;
			rtm->rtm_flags = ((cmd == RTM_ADD) ? RTF_UP : 0);
			rtm->rtm_errno = error;
			if (error == 0)
				rtm->rtm_flags |= RTF_DONE;
			rtm->rtm_addrs = rtm_addrs;
			rts_queue_input(mp, NULL);
		}
	}
}
