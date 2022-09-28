/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.
 */

#pragma ident   "@(#)igmp.c 1.27     96/10/13 SMI"

/*
 * Internet Group Management Protocol (IGMP) routines.
 *
 * Written by Steve Deering, Stanford, May 1988.
 * Modified by Rosen Sharma, Stanford, Aug 1994.
 * Modified by Bill Fenner, Xerox PARC, Feb. 1995.
 *
 * MULTICAST 3.5.1.1
 */

#define	IGMP_DEBUG

#ifndef	MI_HDRS

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/dlpi.h>
#include <sys/stropts.h>
#include <sys/strlog.h>
#include <sys/systm.h>
#include <sys/ddi.h>
#include <sys/cmn_err.h>

#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <sys/sockio.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/igmp_var.h>

#include <inet/common.h>
#include <inet/mi.h>
#include <inet/nd.h>
#include <inet/arp.h>
#include <inet/ip.h>
#include <inet/ip_multi.h>
#include <netinet/igmp.h>

#else

#include <types.h>
#include <stream.h>
#include <dlpi.h>
#include <stropts.h>
#include <strlog.h>
#include <lsystm.h>
#include <tihdr.h>
#include <tiuser.h>

#include <param.h>
#include <socket.h>
#include <if.h>
#include <if_arp.h>
#include <sockio.h>
#include <route.h>
#include <in.h>
#include <igmp_var.h>

#include <common.h>
#include <mi.h>
#include <nd.h>
#include <arp.h>
#include <ip.h>
#include <ip_multi.h>
#include <igmp.h>
#endif

kmutex_t igmp_ilm_lock;	/* Protect ilm_state */
int	igmp_slowtimeout_id = 0;

static void	igmp_sendpkt(ilm_t *ilm, int type, unsigned long addr);


/*
 * NOTE: BSD timer is based on fastimo, which is 200ms. Solaris
 * is based on IGMP_TIMEOUT_INTERVAL which is 100ms.
 * Therefore, scalin factor is different, and Solaris timer value
 * is twice that of BSD's
 */
static int	igmp_timers_are_running;
static int 	igmp_time_since_last;	/* Time since last timeout */

/*
 * igmp_start_timers:
 * The unit for next is milliseconds.
 */
void
igmp_start_timers(next)
    unsigned next;
{
	if (next != (unsigned)-1 && !igmp_timers_are_running) {
		igmp_timers_are_running = 1;
		igmp_time_since_last = next;
		igmp_timeout_start(next);
	}

}


/*
 * igmp_input:
 * Return 0 if the message is OK and should be handed to "raw" receivers.
 */
/* ARGSUSED */
int
igmp_input(q, mp, ipif)
	queue_t	* q;
	mblk_t	* mp;
	ipif_t	* ipif;
{
	igmpa_t 	* igmpa;
	ipha_t 	* ipha = (ipha_t *)ALIGN32(mp->b_rptr);
	int 	iphlen;
	int 	igmplen;
	ilm_t 	* ilm;
	u32		src, dst, group;
	unsigned	next;
	int	timer;	/* timer value in the igmp query header */
	ipif_t 	*ipif0;
	u32		lcladdr;

	++igmpstat.igps_rcv_total;

	iphlen = IPH_HDR_LENGTH(ipha);
	if ((mp->b_wptr - mp->b_rptr) < (iphlen + IGMP_MINLEN)) {
		if (!pullupmsg(mp, iphlen + IGMP_MINLEN)) {
			++igmpstat.igps_rcv_tooshort;
			freemsg(mp);
			return (-1);
		}
		ipha = (ipha_t *)ALIGN32(mp->b_rptr);
	}
	igmplen = ntohs(ipha->ipha_length) - iphlen;

	/*
	 * Validate lengths
	 */
	if (igmplen < IGMP_MINLEN) {
		++igmpstat.igps_rcv_tooshort;
		freemsg(mp);
		return (-1);
	}
	/*
	 * Validate checksum
	 */
	if (IP_CSUM(mp, iphlen, 0)) {
		++igmpstat.igps_rcv_badsum;
		freemsg(mp);
		return (-1);
	}
	igmpa = (igmpa_t *)ALIGN32(&mp->b_rptr[iphlen]);
	src = ipha->ipha_src;
	dst = ipha->ipha_dst;
#ifdef IGMP_DEBUG
	if ((ipif != NULL) && (ip_debug > 1))
		mi_strlog(ipif->ipif_ill->ill_rq, 1, SL_TRACE,
		    "igmp_input: src 0x%x, dst 0x%x on %s\n",
		    (int)ntohl(src), (int)ntohl(dst),
		    ipif->ipif_ill->ill_name);
#endif IGMP_DEBUG

	/*
	 * In the IGMPv2 specification, there are 3 states and a flag.
	 *
	 * In Non-Member state, we simply don't have a membership record.
	 * In Delaying Member state, our timer is running(ilm->ilm_timer)
	 * In Idle Member state, our timer is not running(ilm->ilm_timer == 0)
	 *
	 * The flag is ilm->ilm_state, it is set to IGMP_OTHERMEMBER if
	 * we have heard a report from another member, or IGMP_IREPORTEDLAST
	 * if I sent the last report.
	 */
	timer = (int) igmpa->igmpa_code *
	    IGMP_TIMEOUT_FREQUENCY/IGMP_TIMER_SCALE;

	switch (igmpa->igmpa_type) {

	case IGMP_MEMBERSHIP_QUERY:
		++igmpstat.igps_rcv_queries;

		if (!ipif) {
			ip0dbg((
			    "igmp_input: membership query without ipif set\n"));
			break;
		}

		if (igmpa->igmpa_code == 0) {
		/*
		 * Query from a old router.
		 * Remember that the querier on this
		 * interface is old, and set the timer to the
		 * value in RFC 1112.
		 */

			ipif->ipif_ill->ill_multicast_type = IGMP_V1_ROUTER;
			ipif->ipif_ill->ill_multicast_time = 0;

		/*
		 * BSD uses PR_FASTHZ, which is 5. See NOTE on top of the file
		 */
			timer =  IGMP_MAX_HOST_REPORT_DELAY *
			    IGMP_TIMEOUT_FREQUENCY;

			if (dst != ntohl(INADDR_ALLHOSTS_GROUP) ||
			    igmpa->igmpa_group != 0) {
				++igmpstat.igps_rcv_badqueries;
				freemsg(mp);
				return (-1);
			}

		} else {
		/*
		 * Query from a new router
		 * Simply do a validity check
		 */
			group = igmpa->igmpa_group;
			if (group != 0 &&
			    (!CLASSD(group))) {
				++igmpstat.igps_rcv_badqueries;
				freemsg(mp);
				return (-1);
			}
		}

#ifdef IGMP_DEBUG
		if (ipif) {
			if (ip_debug > 1)
				mi_strlog(ipif->ipif_ill->ill_rq, 1, SL_TRACE,
				    "igmp_input: TIMER = igmp_code %d "
				    "igmp_type 0x%x",
				    (int) ntohs(igmpa->igmpa_code),
				    (int) ntohs(igmpa->igmpa_type));
		}
#endif IGMP_DEBUG
		/*
		* -Start the timers in all of our membership records for
		* the physical interface on which the query arrived,
		* excl. those that belong to the "all hosts" group(224.0.0.1).
		* -Restart any timer that is already running but has a value
		* longer that the requested timeout.
		* -Use the value specified in the query message as the
		* maximum timeout.
		*/
		mutex_enter(&igmp_ilm_lock);
		next = (unsigned)-1;
		for (ilm = ipif->ipif_ill->ill_ilm; ilm;
		    ilm = ilm->ilm_next) {
			if (ilm->ilm_addr == ntohl(INADDR_ANY))
				continue;
			if (ilm->ilm_addr != ntohl(INADDR_ALLHOSTS_GROUP) &&
			    (igmpa->igmpa_group == 0) ||
			    (igmpa->igmpa_group == ilm->ilm_addr)) {
				if (ilm->ilm_timer == 0 ||
				    ilm->ilm_timer > timer) {
					ilm->ilm_timer =
					    IGMP_RANDOM_DELAY(ilm->ilm_addr,
					    ilm->ilm_ipif, timer);
					if (ilm->ilm_timer < next)
						next = ilm->ilm_timer;
				}
			}
		}
		igmp_start_timers(next);
		mutex_exit(&igmp_ilm_lock);
		break;

	case IGMP_V1_MEMBERSHIP_REPORT:
	case IGMP_V2_MEMBERSHIP_REPORT:

		if (!ipif) {
			ip0dbg((
			    "igmp_input: "
			    "membership report without ipif set\n"));
			break;
		}
		/*
		 * For fast leave to work, we have to know that we are the
		 * last person to send a report for this group.  Reports
		 * can potentially get looped back if we are a multicast
		 * router, so discard reports sourced by me.
		 */

		lcladdr = ipif->ipif_net_mask & ipif->ipif_local_addr;
		for (ipif0 = ipif->ipif_ill->ill_ipif; ipif0;
		    ipif0 = ipif0->ipif_next) {

			if (ipif0->ipif_local_addr == lcladdr) {
#ifdef IGMP_DEBUG
				if (ip_debug > 1)
					mi_strlog(ipif->ipif_ill->ill_rq, 1,
					    SL_TRACE,
					    "igmp_input: we are only "
					    "member src 0x%x ipif_local 0x%x",
					    (int)ntohl(lcladdr),
					    (int)
					    ntohl(ipif0->ipif_local_addr));
#endif IGMP_DEBUG
				break;
			}
		}

		++igmpstat.igps_rcv_reports;
		group = igmpa->igmpa_group;
		if (!CLASSD(group)) {
			++igmpstat.igps_rcv_badreports;
			freemsg(mp);
			return (-1);
		}

	/*
	 * KLUDGE: if the IP source address of the report has an
	 * unspecified (i.e., zero) subnet number, as is allowed for
	 * a booting host, replace it with the correct subnet number
	 * so that a process-level multicast routing demon can
	 * determine which subnet it arrived from.  This is necessary
	 * to compensate for the lack of any way for a process to
	 * determine the arrival interface of an incoming packet.
	 */
	/*
	 * This requires that a copy of *this* message it passed up
	 * to the raw interface which is done by our caller.
	 */
		if ((src & htonl(0xFF000000)) == 0) {	/* Minimum net mask */
			src = ipif->ipif_net_mask & ipif->ipif_local_addr;
			ip1dbg(("igmp_input: changed src to 0x%x\n",
			    (int)ntohl(src)));
			ipha->ipha_src = src;
		}

		/*
		 * If we belong to the group being reported,
		 * stop our timer for that group. Do this for all
		 * logical interfaces on the given physical interface.
		 */
		for (ipif = ipif->ipif_ill->ill_ipif; ipif;
		    ipif = ipif->ipif_next) {
			ilm = ilm_lookup_exact(ipif, group);
			if (ilm != NULL) {
				ilm->ilm_timer = 0;
				++igmpstat.igps_rcv_ourreports;

				mutex_enter(&igmp_ilm_lock);
				ilm->ilm_state = IGMP_OTHERMEMBER;
				mutex_exit(&igmp_ilm_lock);
			}
		} /* for */
		break;
	}
	/*
	 * Pass all valid IGMP packets up to any process(es) listening
	 * on a raw IGMP socket. Do not free the packet.
	 */
	return (0);
}

void
igmp_joingroup(ilm)
	ilm_t *ilm;
{

	if (ilm->ilm_addr == ntohl(INADDR_ALLHOSTS_GROUP)) {
		ilm->ilm_timer = 0;
		ilm->ilm_state = IGMP_OTHERMEMBER;
	} else {
		if (ilm->ilm_ipif->ipif_ill->ill_multicast_type ==
		    IGMP_V1_ROUTER)
			igmp_sendpkt(ilm, IGMP_V1_MEMBERSHIP_REPORT, 0);
		else
			igmp_sendpkt(ilm, IGMP_V2_MEMBERSHIP_REPORT, 0);
		ilm->ilm_timer = IGMP_RANDOM_DELAY(ilm->ilm_addr,
		    ilm->ilm_ipif,
		    IGMP_MAX_HOST_REPORT_DELAY *
		    IGMP_TIMEOUT_FREQUENCY);
		igmp_start_timers(ilm->ilm_timer);
		ilm->ilm_state = IGMP_IREPORTEDLAST;
	}
#ifdef IGMP_DEBUG
	if (ip_debug > 1)
		mi_strlog(ilm->ilm_ipif->ipif_ill->ill_rq, 1, SL_TRACE,
		    "igmp_joingroup: multicast_type %d timer %d",
		    (int)ntohl(ilm->ilm_ipif->ipif_ill->ill_multicast_type),
		    (int)ntohl(ilm->ilm_timer));
#endif IGMP_DEBUG

}

void
igmp_leavegroup(ilm)
	ilm_t *ilm;
{

	if (ilm->ilm_state == IGMP_IREPORTEDLAST &&
	    ilm->ilm_addr != ntohl(INADDR_ALLHOSTS_GROUP) &&
	    (ilm->ilm_ipif->ipif_ill->ill_multicast_type != IGMP_V1_ROUTER))
		igmp_sendpkt(ilm, IGMP_V2_LEAVE_GROUP,
		    (ntohl(INADDR_ALLRTRS_GROUP)));
}


/*
 * igmp_timeout_handler:
 * Called when there are timeout events, every next * TMEOUT_INTERVAL (tick).
 * Returns number of ticks to next event (or 0 if none).
 */
int
igmp_timeout_handler()
{
	ill_t	*ill;
	ilm_t 	*ilm;
	u_long	next = 0xffffffffU;
	int		elapsed;	/* Since last call */

	/*
	 * Quick check to see if any work needs to be done, in order
	 * to minimize the overhead of fasttimo processing.
	 */
	if (!igmp_timers_are_running)
		return (0);

	elapsed = igmp_time_since_last;
	if (elapsed == 0)
		elapsed = 1;

	igmp_timers_are_running = 0;
	for (ill = ill_g_head; ill; ill = ill->ill_next)
		for (ilm = ill->ill_ilm; ilm; ilm = ilm->ilm_next) {

			if (ilm->ilm_timer != 0) {
				if (ilm->ilm_timer <= elapsed) {
					ilm->ilm_timer = 0;
					if (ill->ill_multicast_type ==
					    IGMP_V1_ROUTER)
						igmp_sendpkt(ilm,
						    IGMP_V1_MEMBERSHIP_REPORT,
						    0);
					else
						igmp_sendpkt(ilm,
						    IGMP_V2_MEMBERSHIP_REPORT,
						    0);
					ilm->ilm_state = IGMP_IREPORTEDLAST;
#ifdef IGMP_DEBUG
					if (ip_debug > 1)
						mi_strlog(ill->ill_rq, 1,
						    SL_TRACE,
						    "igmp_timo_hlr 1: ilm_"
						    "timr %d elap %d typ %d"
						    " nxt %d",
						    (int)ntohl(ilm->ilm_timer),
						    elapsed,
						    (int)ntohl(
							ill->
							ill_multicast_type),
						    next);
#endif IGMP_DEBUG
				} else {
					ilm->ilm_timer -= elapsed;
					igmp_timers_are_running = 1;
					if (ilm->ilm_timer < next)
						next = ilm->ilm_timer;
#ifdef IGMP_DEBUG
					if (ip_debug > 1)
						mi_strlog(ill->ill_rq, 1,
						    SL_TRACE,
						    "igmp_timo_hlr 2: ilm_timr"
						    " %d elap %d typ %d nxt"
						    " %d",
						    (int)ntohl(ilm->ilm_timer),
						    elapsed,
						    (int)ntohl(
							ill->
							ill_multicast_type),
						    next);
#endif IGMP_DEBUG
				}
			}
		}
	if (next == (unsigned)-1)
		next = 0;
	igmp_time_since_last = next;
	return (next);
}


/*
 * igmp_slowtimo:
 * Resets to new router and timer to IGMP_AGE_THRESHOLD.
 * Resets slowtimeout.
 */
void
igmp_slowtimo(q)
	queue_t	*q;  /* the ill_rq */
{
	ill_t	*ill = ill_g_head;

	for (ill = ill_g_head; ill; ill = ill->ill_next) {
		if (ill->ill_multicast_type == IGMP_V1_ROUTER) {
			ill->ill_multicast_time++;
			if (ill->ill_multicast_time >= IGMP_AGE_THRESHOLD) {
				ill->ill_multicast_type = IGMP_V2_ROUTER;
			}
		}
	}
	igmp_slowtimeout_id =
	    qtimeout(q, igmp_slowtimo, (caddr_t)q,
		MS_TO_TICKS(IGMP_SLOWTIMO_INTERVAL));

}


/*
 * igmp_sendpkt:
 * This will send to ip_wput like icmp_inbound.
 * Note that the lower ill (on which the membership is kept) is used
 * as an upper ill to pass in the multicast parameters.
 */
static void
igmp_sendpkt(ilm, type, addr)
	ilm_t *ilm;
	int type;
	unsigned long addr;
{
	mblk_t	*mp;
	igmpa_t 	*igmpa;
	ipha_t 	*ipha;
	int		size	= sizeof (ipha_t) + sizeof (igmpa_t);
	ipif_t 	*ipif 	= ilm->ilm_ipif;
	ill_t 	*ill	= ipif->ipif_ill;	/* Will be the "lower" ill */

	ip1dbg(("igmp_sendpkt: for 0x%x on %s\n", (int) ntohl(ilm->ilm_addr),
	    ill->ill_name));
	mp = allocb(size, BPRI_HI);
	if (mp == NULL)
		return;
	bzero((char *)mp->b_rptr, size);
	mp->b_wptr = mp->b_rptr + size;
	mp->b_datap->db_type = M_DATA;

	ipha = (ipha_t *)ALIGN32(mp->b_rptr);
	igmpa = (igmpa_t *)ALIGN32(mp->b_rptr + sizeof (ipha_t));
	igmpa->igmpa_type   = (u_char)type;
	igmpa->igmpa_code   = 0;
	igmpa->igmpa_group  = ilm->ilm_addr;
	igmpa->igmpa_cksum  = 0;
	igmpa->igmpa_cksum  = IP_CSUM(mp, sizeof (ipha_t), 0);

	ipha->ipha_version_and_hdr_length = (IP_VERSION << 4)
	    | IP_SIMPLE_HDR_LENGTH_IN_WORDS;
	ipha->ipha_type_of_service 	= 0;
	ipha->ipha_length 	= htons(IGMP_MINLEN + IP_SIMPLE_HDR_LENGTH);
	ipha->ipha_fragment_offset_and_flags = 0;
	ipha->ipha_ttl 		= 1;
	ipha->ipha_protocol 	= IPPROTO_IGMP;
	ipha->ipha_hdr_checksum 	= 0;
	ipha->ipha_dst 		= addr ? addr : igmpa->igmpa_group;
	ipha->ipha_src 		= ipif->ipif_local_addr;
	/*
	 * Request loopback of the report if we are acting as a multicast
	 * router, so that the process-level routing demon can hear it.
	 */
	/*
	 * This will run multiple times for the same group if there are members
	 * on the same group for multiple ipif's on the same ill. The
	 * igmp_input code will suppress this due to the loopback thus we
	 * always loopback membership report.
	 */

	ip_multicast_loopback(ill->ill_rq, mp);

	ip_wput_multicast(ill->ill_wq, mp, ipif);

	++igmpstat.igps_snd_reports;
}
