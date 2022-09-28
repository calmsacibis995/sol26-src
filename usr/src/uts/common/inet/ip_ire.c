/*
 * Copyright (c) 1992-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)ip_ire.c 1.38     96/10/14 SMI"

/*
 * This file contains routines that manipulate Internet Routing Entries (IREs).
 */

#define	_IP_IRE_C

#ifndef	MI_HDRS
#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/strlog.h>
#include <sys/dlpi.h>
#include <sys/ddi.h>
#include <sys/cmn_err.h>

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
#include <in.h>

#include <common.h>
#include <mi.h>
#include <ip.h>

#endif

#ifndef	STRMSGSZ
#define	STRMSGSZ	4096
#endif

#ifndef	DB_REF_REDZONE
#define	DB_REF_REDZONE	120
#endif

#define	IP_HASH_TABLE_SIZE	32	/* size of each hash table in longs */
#define	IP_CACHE_TABLE_SIZE	256
#define	IP_MASK_TABLE_SIZE	33	/* 33 longs */

ire_t ** ip_forwarding_table[IP_MASK_TABLE_SIZE];
ire_t * ip_cache_table[IP_CACHE_TABLE_SIZE];
u32	ire_handle = 1;

kmutex_t ire_handle_lock;	/* Protects ire_handle */

#define	IP_MASK_BIT_SIZE	32

/*
 * Exclusive-or all bytes in the address thus independent of the byte
 * order as long as TABLE_SIZE does not exceed 256.
 */
#define	INE_ADDR_HASH(addr, TABLE_SIZE) \
	(((((addr) >> 16) ^ (addr)) ^ ((((addr) >> 16) ^ (addr))>> 8)) \
		% TABLE_SIZE)

#define	IP_CTABLE_HASH_PTR(addr) \
	(&(ip_cache_table[INE_ADDR_HASH(addr, IP_CACHE_TABLE_SIZE)]))

#define	IP_FTABLE_HASH_PTR(addr, mask) \
	((ip_forwarding_table[(ip_mask_index(mask))] == NULL) ? NULL : \
	((&(ip_forwarding_table[(ip_mask_index(mask))]) \
		[INE_ADDR_HASH((addr & mask), IP_HASH_TABLE_SIZE)])))

#define	IP_MAP_MSIZE_TO_MASK(i)	\
	(ipaddr_t)htonl(((IP_HOST_MASK)<< (IP_MASK_BIT_SIZE - i)))

int	ip_ire_advise(queue_t * q, mblk_t * mp);
int	ip_ire_delete(queue_t * q, mblk_t * mp);
int	ip_ire_report(queue_t * q, mblk_t * mp, caddr_t arg);
ire_t	* ire_cache_lookup(ipaddr_t addr);
static	void	ire_report_ftable(ire_t * ire, char * mp);
static	void	ire_report_ctable(ire_t * ire, char * mp);
void	ip_ire_req(queue_t * q, mblk_t * mp);
void	ire_add_then_put(queue_t * q, mblk_t * mp);
ire_t	* ire_create(u_char * addr, u_char * mask,
    u_char * src_addr, u_char * gateway, u_int max_frag,
    mblk_t * ll_hdr_mp, queue_t * rfq, queue_t * stq,
    u_int type, u_long rtt, u_int ll_hdr_len, ipif_t * ipif, ire_t * sire,
    u_long flags);
ire_t	** ire_create_bcast(ipif_t * ipif, ipaddr_t addr, ire_t ** irep);
void	ire_expire(ire_t * ire, char * arg);
static void	ire_fastpath(ire_t * ire);
void	ire_fastpath_update(ire_t * ire, char * arg);
ire_t	* ire_lookup_local(void);
void	ire_pkt_count(ire_t * ire, char * ippc);
ill_t	* ire_to_ill(ire_t * ire);
void	ire_walk(pfv_t func, char * arg);
static void	ire_walk1(ire_t * ire, pfv_t func, char * arg);
void	ire_walk_wq(queue_t * wq, pfv_t func, char * arg);
static void	ire_walk_wq1(queue_t * wq, ire_t * ire, pfv_t func,
    char * arg);
ire_t	* ire_add(ire_t * ire);
static	void	ire_delete_host_redirects(ipaddr_t gateway);
void	ire_delete(ire_t * ire);
void	ire_delete_route_gw(ire_t * ire, char * cp);
void	ire_flush_cache(ire_t * ire, int flag);
static	boolean_t ire_match_args(ire_t * ire, ipaddr_t addr, ipaddr_t mask,
    ipaddr_t gateway, int type, ipif_t * ipif, queue_t * wrq, int match_flags);
ire_t	* ire_route_lookup(ipaddr_t addr, ipaddr_t mask, ipaddr_t gateway,
    int type, ipif_t * ipif, ire_t ** pire, queue_t * wrq, int flags);
ire_t	* ire_ftable_lookup(ipaddr_t addr, ipaddr_t mask, ipaddr_t gateway,
    int type, ipif_t * ipif, ire_t ** pire, queue_t * wrq, int flags);
ire_t	* ire_ctable_lookup(ipaddr_t addr, ipaddr_t gateway, int type,
    ipif_t * ipif, queue_t * wrq, int flags);
ire_t	* ire_cache_lookup(ipaddr_t addr);
ire_t	* ipif_to_ire(ipif_t * ipif);


/*
 * This function is associated with the IP_IOC_IRE_ADVISE_NO_REPLY
 * IOCTL.  It is used by TCP (or other ULPs) to supply revised information
 * for an existing CACHED IRE.
 */
/* ARGSUSED */
int
ip_ire_advise(q, mp)
	queue_t	* q;
	mblk_t	* mp;
{
	ipaddr_t	addr;
	u_char	* addr_ucp;
	ipic_t	* ipic;
	ire_t	* ire;

	ipic = (ipic_t *)ALIGN32(mp->b_rptr);
	if (ipic->ipic_addr_length != IP_ADDR_LEN ||
	    !(addr_ucp = mi_offset_param(mp, ipic->ipic_addr_offset,
		ipic->ipic_addr_length)))
		return (EINVAL);
	/* Extract the destination address. */
	addr = *(ipaddr_t *)ALIGN32(addr_ucp);
	/* Find the corresponding IRE. */
	ire = ire_cache_lookup(addr);
	if (!ire)
		return (ENOENT);
	/* Update the round trip time estimate and/or the max frag size. */
	if (ipic->ipic_rtt)
		ire->ire_rtt = ipic->ipic_rtt;
	if (ipic->ipic_max_frag)
		ire->ire_max_frag = ipic->ipic_max_frag;
	return (0);
}

/*
 * This function is associated with the IP_IOC_IRE_DELETE[_NO_REPLY]
 * IOCTL[s].  The NO_REPLY form is used by TCP to delete a route IRE
 * for a host that is not responding.  This will force an attempt to
 * establish a new route, if available.  Management processes may want
 * to use the version that generates a reply.
 */
/* ARGSUSED */
int
ip_ire_delete(q, mp)
	queue_t	* q;
	mblk_t	* mp;
{
	u_char	* addr_ucp;
	ipaddr_t	addr;
	ire_t	* ire;
	ipid_t	* ipid;
	int routing_sock_info = 0;

	ipid = (ipid_t *)ALIGN32(mp->b_rptr);

	/* Only actions on IRE_CACHEs are acceptable at present. */
	if (ipid->ipid_ire_type != IRE_CACHE)
		return (EINVAL);

	addr_ucp = mi_offset_param(mp, ipid->ipid_addr_offset,
		ipid->ipid_addr_length);
	if (!addr_ucp)
		return (EINVAL);
	switch (ipid->ipid_addr_length) {
	case IP_ADDR_LEN:
		/* addr_ucp points at IP addr */
		break;
	case sizeof (ipa_t): {
		ipa_t	* ipa;
		/*
		 * got complete (sockaddr) address - increment addr_ucp to point
		 * at the ip_addr field.
		 */
		ipa = (ipa_t *)ALIGN32(addr_ucp);
		addr_ucp = (u_char *)&ipa->ip_addr;
		break;
		}
	default:
		return (EINVAL);
	}
	/* Extract the destination address. */
	bcopy((char *)addr_ucp, (char *)&addr, IP_ADDR_LEN);

	/* Try to find the CACHED IRE. */
	ire = ire_cache_lookup(addr);

	/* Nail it. */
	if (ire) {
		/* Allow delete only on CACHE entries */
		if (ire->ire_type != IRE_CACHE)
			return (EINVAL);

		/*
		 * Verify that the IRE has been around for a while.
		 * This is to protect against transport protocols
		 * that are too eager in sending delete messages.
		 */
		if (time_in_secs <
		    ire->ire_create_time + ip_ignore_delete_time)
			return (EINVAL);
		/*
		 * Now we have a dead cache entry. we need to remove it.
		 * If this cache entry is generated from a default route,
		 * search the default list and mark it dead and some
		 * background process will try to activate it.
		 */
		if ((ire->ire_gateway_addr) && (ire->ire_cmask == 0)) {
			/*
			 * Make sure that we pick a different
			 * IRE_DEFAULT next time.
			 * The ip_def_gateway_count tracks the number of
			 * IRE_DEFAULT entries. However, the
			 * ip_forwarding_table[0] also contains
			 * interface routes thus the count can be zero.
			 */
			ire_t *gw_ire;

			if ((ip_forwarding_table[0] != NULL) &&
			    (gw_ire = ((ip_forwarding_table[0])[0])) &&
			    ip_def_gateway_count != 0) {
				u_int u1 =
				    ip_def_gateway_index % ip_def_gateway_count;
				while (u1--)
					gw_ire = gw_ire->ire_next;
				/* Skip past the potentially bad gateway */
				if (ire->ire_gateway_addr ==
				    gw_ire->ire_gateway_addr)
					ip_def_gateway_index++;
		    }
		}
		/* report the bad route to routing sockets */
		ip_rts_change(RTM_LOSING, ire->ire_addr, ire->ire_gateway_addr,
		    ire->ire_mask, ire->ire_src_addr, 0, 0,
		    (RTA_DST | RTA_GATEWAY | RTA_NETMASK | RTA_IFA));
		routing_sock_info = 1;
		ire_delete(ire);
	}
	/* Also look for an IRE_HOST_REDIRECT and remove it if present */
	ire = ire_route_lookup(addr, 0, 0, IRE_HOST_REDIRECT, NULL, NULL, NULL,
	    MATCH_IRE_TYPE);

	/* Nail it. */
	if (ire) {
		if (!routing_sock_info)
			ip_rts_change(RTM_LOSING, ire->ire_addr,
			    ire->ire_gateway_addr, ire->ire_mask,
			    ire->ire_src_addr, (int)0, (int)0,
			    (RTA_DST | RTA_GATEWAY | RTA_NETMASK | RTA_IFA));
		ire_delete(ire);
	}
	return (0);
}

/*
 * Named Dispatch routine to produce a formatted report on all IREs.
 * This report is accessed by using the ndd utility to "get" ND variable
 * "ip_ire_status".
 */
/* ARGSUSED */
int
ip_ire_report(q, mp, arg)
	queue_t	* q;
	mblk_t	* mp;
	caddr_t	arg;
{
	mi_mpprintf(mp,
	    "IRE      rfq      stq      addr            mask            "
	    "src             gateway         mxfrg rtt   ref "
	    "in/out/forward type");
	/*
	 *   01234567 01234567 01234567 123.123.123.123 123.123.123.123
	 *   123.123.123.123 123.123.123.123 12345 12345 123
	 *   in/out/forward xxxxxxxxxx
	 */
	ire_walk(ire_report_ftable, (char *)mp);
	ire_walk(ire_report_ctable, (char *)mp);
	return (0);
}

/* ire_walk routine invoked for ip_ire_report for each IRE. */
static void
ire_report_ftable(ire, mp)
	ire_t	* ire;
	char	* mp;
{
	char	buf1[16];
	char	buf2[16];
	char	buf3[16];
	char	buf4[16];
	u_long	fo_pkt_count;
	u_long	ib_pkt_count;
	mblk_t	* ll_hdr_mp;
	int	ref = 0;

	if (ire->ire_type & IRE_CACHETABLE)
	    return;
	ll_hdr_mp = ire->ire_ll_hdr_mp;
	if (ll_hdr_mp)
		ref = ll_hdr_mp->b_datap->db_ref;
	/* "inbound" to a non local address is a forward */
	ib_pkt_count = ire->ire_ib_pkt_count;
	fo_pkt_count = 0;
	if (!(ire->ire_type & (IRE_LOCAL|IRE_BROADCAST))) {
		fo_pkt_count = ib_pkt_count;
		ib_pkt_count = 0;
	}
	mi_mpprintf((mblk_t *)ALIGN32(mp),
	    "%08x %08x %08x %s %s %s %s %05d %05D %03d %d/%d/%d %s",
	    ire, ire->ire_rfq, ire->ire_stq,
	    ip_dot_addr(ire->ire_addr, buf1), ip_dot_addr(ire->ire_mask, buf2),
	    ip_dot_addr(ire->ire_src_addr, buf3),
	    ip_dot_addr(ire->ire_gateway_addr, buf4),
	    ire->ire_max_frag, ire->ire_rtt, ref,
	    ib_pkt_count, ire->ire_ob_pkt_count, fo_pkt_count,
	    ip_nv_lookup(ire_nv_tbl, (int)ire->ire_type));
}


/* ire_walk routine invoked for ip_ire_report for each cached IRE. */
static void
ire_report_ctable(ire, mp)
	ire_t	* ire;
	char	* mp;
{
	char	buf1[16];
	char	buf2[16];
	char	buf3[16];
	char	buf4[16];
	u_long	fo_pkt_count;
	u_long	ib_pkt_count;
	mblk_t	* ll_hdr_mp;
	int	ref = 0;

	if ((ire->ire_type & IRE_CACHETABLE) == 0)
	    return;
	ll_hdr_mp = ire->ire_ll_hdr_mp;
	if (ll_hdr_mp)
		ref = ll_hdr_mp->b_datap->db_ref;
	/* "inbound" to a non local address is a forward */
	ib_pkt_count = ire->ire_ib_pkt_count;
	fo_pkt_count = 0;
	if (!(ire->ire_type & (IRE_LOCAL|IRE_BROADCAST))) {
		fo_pkt_count = ib_pkt_count;
		ib_pkt_count = 0;
	}
	mi_mpprintf((mblk_t *)ALIGN32(mp),
	    "%08x %08x %08x %s %s %s %s %05d %05D %03d %d/%d/%d %s",
	    ire, ire->ire_rfq, ire->ire_stq,
	    ip_dot_addr(ire->ire_addr, buf1), ip_dot_addr(ire->ire_mask, buf2),
	    ip_dot_addr(ire->ire_src_addr, buf3),
	    ip_dot_addr(ire->ire_gateway_addr, buf4),
	    ire->ire_max_frag, ire->ire_rtt, ref,
	    ib_pkt_count, ire->ire_ob_pkt_count, fo_pkt_count,
	    ip_nv_lookup(ire_nv_tbl, (int)ire->ire_type));
}

/*
 * ip_ire_req is called by ip_wput when an IRE_DB_REQ_TYPE message is handed
 * down from the Upper Level Protocol to request a copy of the IRE (to check
 * its type or to extract information like round-trip time estimates or the
 * MTU.)
 * The address is assumed to be in the ire_addr field. If no IRE is found
 * an IRE is returned with ire_type being zero.
 * Note that the upper lavel protocol has to check for broadcast
 * (IRE_BROADCAST) and multicast (CLASSD(addr)).
 * If there is a b_cont the resulting IRE_DB_TYPE mblk is placed at the
 * end of the returned message.
 *
 * TCP sends down a message of this type with a connection request packet
 * chained on. UDP and ICMP send it down to verify that a route exists for
 * the destination address when they get connected.
 */
void
ip_ire_req(q, mp)
	queue_t	* q;
	mblk_t	* mp;
{
	ire_t	* inire;
	ire_t	* ire;
	mblk_t	* mp1;

	if ((mp->b_wptr - mp->b_rptr) < sizeof (ipha_t) ||
	    !OK_32PTR(mp->b_rptr)) {
		freemsg(mp);
		return;
	}
	inire = (ire_t *)ALIGN32(mp->b_rptr);
	/*
	 * Got it, now take our best shot at an IRE.
	 */
	ire = ire_route_lookup(inire->ire_addr, 0, 0, 0, NULL, NULL, NULL,
	    (MATCH_IRE_RECURSIVE | MATCH_IRE_DEFAULT));
	if (ire == NULL) {
		inire->ire_type = 0;
	} else {
		bcopy((char *)ire, (char *)inire, sizeof (ire_t));
	}
	mp->b_wptr = &mp->b_rptr[sizeof (ire_t)];
	mp->b_datap->db_type = IRE_DB_TYPE;

	/* Put the IRE_DB_TYPE mblk last in the chain */
	mp1 = mp->b_cont;
	if (mp1 != NULL) {
		mp->b_cont = nilp(mblk_t);
		linkb(mp1, mp);
		mp = mp1;
	}
	qreply(q, mp);
}

/*
 * ire_add_then_put is called when a new IRE has been created in order to
 * route an outgoing packet.  Typically, it is called from ip_wput when
 * a response comes back down from a resolver.  We add the IRE, and then
 * run the packet through ip_wput or ip_rput, as appropriate.  (Always called
 * as writer.)
 */
void
ire_add_then_put(q, mp)
	queue_t	* q;
	mblk_t	* mp;
{
	mblk_t	* pkt;
	ire_t	* ire = (ire_t *)ALIGN32(mp->b_rptr);

	/*
	 * We are handed a message chain of the form:
	 *	IRE_MBLK-->packet
	 * Unhook the packet from the IRE.
	 */
	pkt = mp->b_cont;
	mp->b_cont = nilp(mblk_t);
	/* Add the IRE. */
	ire = ire_add(ire);
	if (!ire) {
		pkt->b_prev = nilp(mblk_t);
		pkt->b_next = nilp(mblk_t);
		freemsg(pkt);
		return;
	}
	/*
	 * Now we can feed the packet back in and this time it will probably
	 * fly.  If the packet was originally given to ip_wput, we cleared
	 * b_prev.  If it came in to ip_rput, we stored a pointer to
	 * the queue it came in on in b_prev.
	 */
	if (!pkt)
		return;
	/* If the packet originated externally then */
	if (pkt->b_prev) {
		q = (queue_t *)pkt->b_prev;
		pkt->b_prev = nilp(mblk_t);
		mp = allocb(0, BPRI_HI);
		if (!mp) {
			/* TODO source quench */
			pkt->b_next = nilp(mblk_t);
			freemsg(pkt);
			return;
		}
		mp->b_datap->db_type = M_BREAK;
		mp->b_cont = pkt;
		put(q, mp);
	} else if (pkt->b_next) {
		/* Packets from multicast router */
		pkt->b_next = nilp(mblk_t);
		ip_rput_forward(ire, (ipha_t *)ALIGN32(pkt->b_rptr), pkt);
	} else {
		/* Locally originated packets */
		ipha_t *ipha = (ipha_t *)ALIGN32(pkt->b_rptr);

		/*
		 * If we were resolving a router we can not use the
		 * routers IRE for sending the packet (since it would
		 * violate the uniqness of the IP idents) thus we
		 * make another pass through ip_wput to create the IRE_CACHE
		 * for the destination.
		 */
		if (ipha->ipha_dst != ire->ire_addr)
			ip_wput(q, pkt);
		else
			ip_wput_ire(q, pkt, ire);
	}
}

/*
 * ire_create is called to allocate and initialize
 * a new IRE.  (May be called as writer.)
 */
ire_t *
ire_create(addr, mask, src_addr, gateway, max_frag, ll_hdr_mp, rfq, stq,
		type, rtt, ll_hdr_len, ipif, sire, flags)
	u_char	* addr;
	u_char	* mask;
	u_char	* src_addr;
	u_char	* gateway;
	u_int	max_frag;
	mblk_t	* ll_hdr_mp;
	queue_t	* rfq;
	queue_t	* stq;
	u_int	type;
	u_long	rtt;
	u_int	ll_hdr_len;
	ipif_t  * ipif;
	ire_t   *sire;
	u_long  flags;
{
	static	ire_t	ire_nil;
	ire_t	* ire;
	mblk_t	* mp;

	if (ll_hdr_mp) {
		if (ll_hdr_mp->b_datap->db_ref < DB_REF_REDZONE) {
			ll_hdr_mp = dupb(ll_hdr_mp);
		} else {
			ll_hdr_mp = copyb(ll_hdr_mp);
		}
		if (!ll_hdr_mp)
			return (nilp(ire_t));
	}

	/* Check that IRE_IF_RESOLVER and IRE_IF_NORESOLVER have a ll_hdr_mp */
	if ((type & IRE_INTERFACE) &&
	    ll_hdr_mp == nilp(mblk_t)) {
		ip0dbg(("ire_create: no ll_hdr_mp\n"));
		return (nilp(ire_t));
	}

	/* Allocate the new IRE. */
	mp = allocb(sizeof (ire_t), BPRI_MED);
	if (!mp) {
		if (ll_hdr_mp)
			freeb(ll_hdr_mp);
		return (nilp(ire_t));
	}

	ire = (ire_t *)ALIGN32(mp->b_rptr);
	mp->b_wptr = (u_char *)&ire[1];

	/* Start clean. */
	*ire = ire_nil;

	/*
	 * Initialize the atomic ident field, using a possibly environment-
	 * specific macro.
	 */
	ATOMIC_32_INIT(&ire->ire_atomic_ident);
	ire->ire_mp = mp;
	mp->b_datap->db_type = IRE_DB_TYPE;

	bcopy((char *)addr, (char *)&ire->ire_addr, IP_ADDR_LEN);
	if (src_addr)
		bcopy((char *)src_addr, (char *)&ire->ire_src_addr,
		    IP_ADDR_LEN);
	if (mask)
		bcopy((char *)mask, (char *)&ire->ire_mask, IP_ADDR_LEN);
	if (gateway) {
		bcopy((char *)gateway, (char *)&ire->ire_gateway_addr,
			IP_ADDR_LEN);
	}
	ire->ire_max_frag = max_frag;
	ire->ire_frag_flag = (ip_path_mtu_discovery) ? IPH_DF : 0;
	ire->ire_ll_hdr_mp = ll_hdr_mp;
	ire->ire_stq = stq;
	ire->ire_rfq = rfq;
	ire->ire_type = type;
	ire->ire_rtt = rtt;
	ire->ire_flags = RTF_UP | flags;
	ire->ire_ll_hdr_length = ll_hdr_len;
	ATOMIC_32_ASSIGN(&ire->ire_atomic_ident, (u32)LBOLT_TO_MS(lbolt));
	ire->ire_tire_mark = ire->ire_ob_pkt_count + ire->ire_ib_pkt_count;
	ire->ire_create_time = (u32)time_in_secs;

	/*
	 * If this IRE is IRE_CACHE, inherit the handle from
	 * parent IRE. For others assign new ones.
	 *
	 * The mutex protecting ire_handle is because ire_create is not always
	 * called as a writer.
	 */
	if (ire->ire_type != IRE_CACHE) {
		mutex_enter(&ire_handle_lock);
		ire->ire_chandle = (u32)ire_handle++;
		mutex_exit(&ire_handle_lock);
	} else if (sire) {
		ire->ire_chandle = sire->ire_chandle;
		ire->ire_cmask = sire->ire_mask;
	}
	if (ire->ire_type & (IRE_CACHETABLE | IRE_INTERFACE))
		ire->ire_ipif = ipif;
	else
		ire->ire_ipif = NULL;
	return (ire);
}

/*
 * This routine is called repeatedly by ipif_up to create broadcast IREs.
 * It is passed a pointer to a slot in an IRE pointer array into which to
 * place the pointer to the new IRE, if indeed we create one.  If the
 * IRE corresponding to the address passed in would be a duplicate of an
 * existing one, we don't create the new one.  irep is incremented before
 * return only if we do create a new IRE.  (Always called as writer.)
 */
ire_t **
ire_create_bcast(ipif, addr, irep)
	ipif_t	* ipif;
	ipaddr_t  addr;
	ire_t	** irep;
{
	/*
	 * No broadcast IREs for the LOOPBACK interface
	 * or others such as point to point.
	 */
	if (!(ipif->ipif_flags & IFF_BROADCAST))
		return (irep);

	/* If this would be a duplicate, don't bother. */
	if (ire_ctable_lookup(addr, 0, IRE_BROADCAST, ipif, NULL,
	    (MATCH_IRE_TYPE | MATCH_IRE_IPIF)))
		return (irep);

	*irep++ = ire_create(
		(u_char *)&addr,			/* dest addr */
		(u_char *)&ip_g_all_ones,		/* mask */
		(u_char *)&ipif->ipif_local_addr,	/* source addr */
		nilp(u_char),				/* no gateway */
		ipif->ipif_mtu,				/* max frag */
		ipif->ipif_bcast_mp,			/* xmit header */
		ipif->ipif_rq,				/* recv-from queue */
		ipif->ipif_wq,				/* send-to queue */
		IRE_BROADCAST,
		0,					/* rtt */
		0,
		ipif,
		NULL,
		0);
	/*
	 * Create a loopback IRE for the broadcast address.
	 * Note: ire_add() will blow away duplicates thus there is no need
	 * to check for existing entries.
	 */
	*irep++ = ire_create(
		(u_char *)&addr,		/* dest address */
		(u_char *)&ip_g_all_ones,	/* mask */
		(u_char *)&ipif->ipif_local_addr, /* source address */
		nilp(u_char),			/* no gateway */
		STRMSGSZ,			/* max frag size */
		nilp(mblk_t),			/* no xmit header */
		ipif->ipif_rq,			/* recv-from queue */
		nilp(queue_t),			/* no send-to queue */
		IRE_BROADCAST,			/* Needed for fanout in wput */
		0,				/* rtt */
		0,
		ipif,
		NULL,
		0);
	return (irep);
}

/*
 * ire_walk routine to delete any ROUTE IRE's that are stale.
 * We check the current value of the IRE ident field.  If it is unchanged
 * since we last checked, we delete the IRE.  Otherwise, we re-mark the
 * tire and check it next time.  (Always called as writer.)
 */
void
ire_expire(ire, arg)
	ire_t	* ire;
	char	* arg;
{
	int flush_flags = (int)arg;

	if ((flush_flags & FLUSH_REDIRECT_TIME) &&
	    ire->ire_type == IRE_HOST_REDIRECT) {
		/* Make sure we delete the corresponding IRE_CACHE */
		ip1dbg(("ire_expire: all redirects\n"));
		ire_flush_cache(ire, IRE_FLUSH_DELETE);
		ire_delete(ire);
		return;
	}
	if (ire->ire_type != IRE_CACHE)
		return;
	if (flush_flags & FLUSH_CACHE_TIME) {
		/*
		 * Remove all IRE_CACHE.
		 * Verify that create time is more than
		 * ip_ire_flush_interval milliseconds ago.
		 */
		if (((u32)time_in_secs - ire->ire_create_time) * 1000 >
		    ip_ire_flush_interval) {
			ip1dbg(("ire_expire: all IRE_CACHE\n"));
			ire_delete(ire);
			return;
		}
	}
	/*
	 * Garbage collect it if has not been used since the
	 * last time and if it is not on the local network.
	 * Avoid agressive garbage collection if path MTU discovery
	 * has decremented the MTU.
	 */
	if ((ire->ire_ob_pkt_count + ire->ire_ib_pkt_count ==
	    ire->ire_tire_mark) && (ire->ire_gateway_addr != 0) &&
	    (ire->ire_ipif != NULL) &&
	    (ire->ire_max_frag == ire->ire_ipif->ipif_mtu)) {
		ip1dbg(("ire_expire: old IRE_CACHE\n"));
		ire_delete(ire);
		return;
	}
	ire->ire_tire_mark = ire->ire_ob_pkt_count + ire->ire_ib_pkt_count;
	if (ip_path_mtu_discovery && (flush_flags & FLUSH_MTU_TIME) &&
	    (ire->ire_ipif != NULL)) {
		/* Increase pmtu if it is less than the interface mtu */
		ire->ire_max_frag = ire->ire_ipif->ipif_mtu;
		ire->ire_frag_flag = IPH_DF;
	}
}

/*
 * If the device driver supports it, we change the ire_ll_hdr_mp from a
 * dl_unitdata_req to an M_DATA prepend.  (May be called as writer.)
 */
static void
ire_fastpath(ire)
	ire_t	* ire;
{
	ill_t	* ill;
	u_int	len;

	if (ire->ire_ll_hdr_length || !ire->ire_ll_hdr_mp)
		return;
	ill = ire_to_ill(ire);
	if (!ill)
		return;
	len = ill->ill_hdr_length;
	if (len == 0)
		return;
	ill_fastpath_probe(ill, ire->ire_ll_hdr_mp);
}

/*
 * Update all IRE's that are not in fastpath mode and
 * have an ll_hdr_mp that matches mp. mp->b_cont contains
 * the fastpath header.
 */
void
ire_fastpath_update(ire, arg)
	ire_t	*ire;
	char 	*arg;
{
	mblk_t 	* mp, * ll_hdr_mp;
	u_char 	* up, * up2;
	int	cmplen;

	if (!(ire->ire_type & (IRE_CACHE | IRE_BROADCAST)))
		return;
	if (ire->ire_ll_hdr_length != 0 || !ire->ire_ll_hdr_mp)
		return;

	ip2dbg(("ip_fastpath_update: trying\n"));
	mp = (mblk_t *)ALIGN32(arg);
	up = mp->b_rptr;
	cmplen = mp->b_wptr - up;
	up2 = ire->ire_ll_hdr_mp->b_rptr;
	if (ire->ire_ll_hdr_mp->b_wptr - up2 != cmplen ||
	    bcmp((char *)up, (char *)up2, cmplen) != 0)
		return;
	/* Matched - install mp as the ire_ll_hdr_mp */
	ip1dbg(("ip_fastpath_update: match\n"));
	ll_hdr_mp = dupb(mp->b_cont);
	if (ll_hdr_mp) {
		if (ire->ire_ll_hdr_length == 0) {
			/* Save the ll_hdr for mib and SIOC*ARP ioctls */
			if (ire->ire_ll_hdr_saved_mp)
				freeb(ire->ire_ll_hdr_saved_mp);
			ire->ire_ll_hdr_saved_mp = ire->ire_ll_hdr_mp;
		} else
			freeb(ire->ire_ll_hdr_mp);
		ire->ire_ll_hdr_mp = ll_hdr_mp;
		ire->ire_ll_hdr_length = ll_hdr_mp->b_wptr - ll_hdr_mp->b_rptr;
	}
}

/*
 * ire_lookup_loop_multi
 */
ire_t *
ire_lookup_loop_multi(group)
	ipaddr_t group;
{
	ire_t	*ire;

	ire = ire_ftable_lookup(group, 0, 0, 0, NULL, NULL, NULL,
	    MATCH_IRE_DSTONLY);
	while (ire) {
		switch (ire->ire_type) {
		case IRE_DEFAULT:
		case IRE_PREFIX:
		case IRE_HOST:
			ire = ire_ftable_lookup(ire->ire_gateway_addr, 0, 0,
			    IRE_INTERFACE, NULL, NULL, NULL, MATCH_IRE_TYPE);
			break;
		case IRE_IF_NORESOLVER:
		case IRE_IF_RESOLVER:
			return (ire);
		default:
			return (nilp(ire_t));
		}
	}
	return (nilp(ire_t));
}

/*
 * Return any local address.  We use this to target ourselves
 * when the src address was specified as 'default'.
 * Preference for IRE_LOCAL entries.
 */
ire_t *
ire_lookup_local(void)
{
	ire_t	* ire;
	ire_t	* maybe = nilp(ire_t);
	int i;

	for (i = 0; i < IP_CACHE_TABLE_SIZE;  i++) {
		if ((ire = ip_cache_table[i]) == NULL)
			continue;
		for (; ire; ire = ire->ire_next) {
			switch (ire->ire_type) {
			case IRE_LOOPBACK:
				if (maybe == nilp(ire_t))
					maybe = ire;
				break;
			case IRE_LOCAL:
				return (ire);
			}
		}
	}
	return (maybe);
}

/* ire_walk routine to sum all the packets for IREs that match */
void
ire_pkt_count(ire, ippc_arg)
	ire_t	* ire;
	char	* ippc_arg;
{
	ippc_t	* ippc = (ippc_t *)ALIGN32(ippc_arg);

	if (ire->ire_src_addr == ippc->ippc_addr) {
		/* "inbound" to a non local address is a forward */
		if (ire->ire_type & (IRE_LOCAL|IRE_BROADCAST))
			ippc->ippc_ib_pkt_count += ire->ire_ib_pkt_count;
		else
			ippc->ippc_fo_pkt_count += ire->ire_ib_pkt_count;
		ippc->ippc_ob_pkt_count += ire->ire_ob_pkt_count;
	}
}

/*
 * If the specified IRE is associated with a particular ILL, return
 * that ILL pointer.  (May be called as writer.)
 */
ill_t *
ire_to_ill(ire)
	ire_t	* ire;
{
	if (ire != NULL && ire->ire_ipif != NULL)
		return (ire->ire_ipif->ipif_ill);
	return (nilp(ill_t));
}

/* Arrange to call the specified function for every IRE in the world. */
void
ire_walk(func, arg)
	pfv_t	func;
	char	* arg;
{
	ire_t	** hash_ptr;
	int i, j;
	ire_t * ire;

	for (i = (IP_MASK_TABLE_SIZE - 1);  ((i > 0) || (i == 0)); i--) {
	    if (ip_forwarding_table[i] == NULL)
		continue;
	    hash_ptr = ip_forwarding_table[i];
	    for (j = 0; j < IP_HASH_TABLE_SIZE; j++) {
		if (hash_ptr[j] == NULL)
		    continue;
		ire_walk1(hash_ptr[j], func, arg);
	    }
	}
	for (i = 0; i < IP_CACHE_TABLE_SIZE;  i++) {
	    if ((ire = ip_cache_table[i]) == NULL)
		continue;
	    ire_walk1(ire, func, arg);
	}

}

/*
 * Walk the supplied IRE chain calling 'func' with each IRE and 'arg'
 * as parameters.  Note that we walk the chain in a way that permits
 * removal of the IRE by the called function.
 */
static void
ire_walk1(ire, func, arg)
	ire_t	* ire;
	pfv_t	func;
	char	* arg;
{
	ire_t	* ire1;
	u_short original_marks;

#ifdef lint
	ire1 = nilp(ire_t);
#endif
	for (; ire; ire = ire1) {
		/*
		 * Protection against (*func)() deleting ire immediately.
		 * I assume that if (*func)() is destructive, then ire_walk/
		 * ire_walk1 is called as a writer.  If that assumption is
		 * false, consider adding locking.
		 *
		 * The two marks are such that IRE_MARK_CONDEMNED will never
		 * be set unless IRE_MARK_PROTECTED is set.
		 * (See ire_delete(), the only function to set CONDEMNED.)
		 */
		original_marks = ire->ire_marks;
		ire->ire_marks |= IRE_MARK_PROTECTED;
		(*func)(ire, arg);
		ire1 = ire->ire_next;
		/* See if (*func)() would've deleted this ire. */
		if ((original_marks & IRE_MARK_PROTECTED) == 0) {
			/* Reality check. */
			ASSERT((original_marks & IRE_MARK_CONDEMNED) == 0);
			ire->ire_marks &= ~IRE_MARK_PROTECTED;
			if ((ire->ire_marks & IRE_MARK_CONDEMNED) != 0)
			    ire_delete(ire);
		}
		/*
		 * else do nothing, as an ire_walk1() further up on the stack
		 * protected this ire, and it will check for CONDEMNED.
		 */
	}
}

/*
 * Arrange to call the specified
 * function for every IRE that matches the wq.
 */
void
ire_walk_wq(wq, func, arg)
	queue_t	* wq;
	pfv_t	func;
	char	* arg;
{
	ire_t	** hash_ptr;
	ire_t	*ire;
	int i, j;

	for (i = (IP_MASK_TABLE_SIZE - 1);  i > 0; i--) {
	    if (ip_forwarding_table[i] == NULL)
		continue;
	    hash_ptr = ip_forwarding_table[i];
	    for (j = 0; j < IP_HASH_TABLE_SIZE; j++) {
		if (hash_ptr[j] == NULL)
		    continue;
		ire_walk_wq1(wq, hash_ptr[j], func, arg);
	    }
	}
	if (ip_forwarding_table[0]) {
	    ire = *(ip_forwarding_table[0]);
	    ire_walk_wq1(wq, ire, func, arg);
	}
	for (i = 0; i < IP_CACHE_TABLE_SIZE;  i++) {
	    if ((ire = ip_cache_table[i]) == NULL)
		continue;
	    ire_walk_wq1(wq, ire, func, arg);
	}
}

/*
 * Walk the supplied IRE chain calling 'func' with each IRE and 'arg'
 * as parameters.  Note that we walk the chain in a way that permits
 * removal of the IRE by the called function.
 *
 * XXX Shouldn't this walk an ipif or an ill instead of a queue?
 */
static void
ire_walk_wq1(wq, ire, func, arg)
	queue_t	* wq;
	ire_t	* ire;
	pfv_t	func;
	char	* arg;
{
	ire_t	* ire1;
	u_int original_marks;

#ifdef lint
	ire1 = nilp(ire_t);
#endif
	for (; ire; ire = ire1) {
		/* See ire_walk1() for rationales, comments, etc. */
		if (ire->ire_stq == wq) {
			original_marks = ire->ire_marks;
			ire->ire_marks |= IRE_MARK_PROTECTED;
			(*func)(ire, arg);
		}
		ire1 = ire->ire_next;
		if ((ire->ire_stq == wq) &&
		    ((original_marks & IRE_MARK_PROTECTED) == 0)) {
			/* Reality check */
			ASSERT((original_marks & IRE_MARK_CONDEMNED) == 0);
			ire->ire_marks &= ~IRE_MARK_PROTECTED;
			if ((ire->ire_marks & IRE_MARK_CONDEMNED) != 0)
			    ire_delete(ire);
		}
	}
}

/*
 * This function takes a mask and returns
 * number of bits set in the mask. If no
 * bit is set it returns 0.
 */
int
ip_mask_index(mask)
	ipaddr_t mask;
{
	int i;

	mask = ntohl(mask);
	for (i = 0; i < IP_MASK_TABLE_SIZE; i++) {
	    if (mask & (1 << i))
		return (IP_MASK_BIT_SIZE - i);
	}
	return (0);
}

/*
 * Add a fully initialized IRE to an appropriate
 * table based on ire_type. (Always called as writer.)
 *
 * The forward table contains IRE_PREFIX/IRE_HOST/IRE_HOST_REDIRECT
 * IRE_IF_RESOLVER/IRE_IF_NORESOLVER and IRE_DEFAULT.
 *
 * The cache table contains IRE_BROADCAST/IRE_LOCAL/IRE_LOOPBACK
 * and IRE_CACHE.
 */
ire_t *
ire_add(ire)
	ire_t	* ire;
{
	ire_t	* ire1;
	int	mask_table_index;
	ire_t	** hash_ptr;
	ire_t	* old_ire;
	int	flags;

	/* If the ire is in a mblk copy it to a kmem_alloc'ed area */
	if (ire->ire_mp) {
		ire1 = (ire_t *)ALIGN32(mi_zalloc(sizeof (ire_t)));
		if (!ire1) {
			ip1dbg(("ire_add: alloc failed\n"));
			ire_delete(ire);
			return (nilp(ire_t));
		}
		*ire1 = *ire;
		ire1->ire_mp = nilp(mblk_t);
		freeb(ire->ire_mp);
		ire = ire1;
	}

	/* Find the appropriate list head. */
	switch (ire->ire_type) {
	case IRE_HOST:
	case IRE_HOST_REDIRECT:
		ire->ire_mask = IP_HOST_MASK;
		ire->ire_src_addr = 0;
		break;
	case IRE_CACHE:
	case IRE_BROADCAST:
		ire_fastpath(ire);
		/* FALLTHRU */
	case IRE_LOCAL:
	case IRE_LOOPBACK:
		ire->ire_mask = IP_HOST_MASK;
		break;
	case IRE_PREFIX:
		ire->ire_src_addr = 0;
		break;
	case IRE_DEFAULT:
		/*
		 * We keep a count of default gateways which is used when
		 * assigning them as routes.
		 */
		ip_def_gateway_count++;
		ire->ire_src_addr = 0;
		break;
	case IRE_IF_RESOLVER:
	case IRE_IF_NORESOLVER:
		break;
	default:
		printf("ire_add: ire 0x%x has unrecognized IRE type (%d)\n",
		    (int)ire, ire->ire_type);
		ire_delete(ire);
		return (nilp(ire_t));
	}

	/* Make sure the address is properly masked. */
	ire->ire_addr &= ire->ire_mask;

	/*
	 * First search the tables and make sure
	 * that there is no duplicate.
	 */
	flags = (MATCH_IRE_MASK | MATCH_IRE_TYPE | MATCH_IRE_GW);
	if (ire->ire_ipif)
		flags |= (MATCH_IRE_IPIF | MATCH_IRE_WQ);
	if (ire1 = ire_route_lookup(ire->ire_addr, ire->ire_mask,
	    ire->ire_gateway_addr, ire->ire_type, ire->ire_ipif, NULL,
	    ire->ire_stq, flags)) {
		ire_delete(ire);
		return (ire1);	/* keep the old one */
	}
	if ((ire->ire_type & IRE_CACHETABLE) == 0) {
		/* IRE goes into Forward Table */
		mask_table_index = ip_mask_index(ire->ire_mask);
		/* Make sure the address is properly masked. */
		ire->ire_addr &= ire->ire_mask;
		if ((ip_forwarding_table[mask_table_index]) == NULL) {
			ip_forwarding_table[mask_table_index] =
			    (ire_t **)ALIGN32(mi_zalloc((
				IP_HASH_TABLE_SIZE * sizeof (ire_t *))));
			if ((ip_forwarding_table[mask_table_index]) == NULL) {
				ire_delete(ire);
				return (NULL);
			}
		}
		hash_ptr = (ire_t **)IP_FTABLE_HASH_PTR(ire->ire_addr,
		    ire->ire_mask);
	} else
		hash_ptr = (ire_t **)IP_CTABLE_HASH_PTR(ire->ire_addr);
	/*
	 * Make it easy for ip_wput() to hit multiple targets by grouping
	 * identical addresses together on the hash chain.
	 */
	old_ire = *hash_ptr;
	for (; (old_ire && (ire->ire_addr != old_ire->ire_addr));
		old_ire = old_ire->ire_next);
	if (old_ire == NULL) {
		old_ire = *hash_ptr;
		ire->ire_ptpn = hash_ptr;
		*hash_ptr = ire;
	} else {
		ire->ire_ptpn = old_ire->ire_ptpn;
		*(old_ire->ire_ptpn) = ire;
	}
	if (old_ire) {
		ire->ire_next = old_ire;
		old_ire->ire_ptpn = &ire->ire_next;
	} else
		ire->ire_next = NULL;
	return (ire);
}


/*
 * Search for all HOST REDIRECT routes that are
 * pointing at the specified gateway and
 * delete them. This routine is called only
 * when a default gateway is going away.
 * (Always called as writer.)
 */
static void
ire_delete_host_redirects(gateway)
	ipaddr_t gateway;
{
	ire_t ** hash_ptr;
	ire_t * ire;
	ire_t * ire_next;
	int i;

#ifdef lint
	ire_next = NULL;
#endif
	/* get the hash table for HOST routes */
	hash_ptr = ip_forwarding_table[(IP_MASK_TABLE_SIZE - 1)];
	if (hash_ptr == NULL)
		return;
	for (i = 0; (i < IP_HASH_TABLE_SIZE); i++) {
		ire = hash_ptr[i];
		for (; ire; ire = ire_next) {
			ire_next = ire->ire_next;
			if (ire->ire_type != IRE_HOST_REDIRECT)
				continue;
			if (ire->ire_gateway_addr == gateway)
				ire_delete(ire);
		}
	}
}

/*
 * Delete the specified IRE.
 * (Always called as writer.)
 */
void
ire_delete(ire)
	ire_t	* ire;
{
	ire_t	* ire1;
	ire_t	** ptpn = ire->ire_ptpn;
	ipif_t	* ipif;

	/* Check to see if IRE is protected. */
	if ((ire->ire_marks & IRE_MARK_PROTECTED) != 0) {
		/*
		 * Whoever protected this IRE will delete it.
		 * Be warned, this assumes that whoever protected this
		 * IRE will not only delete it, but delete it before giving
		 * up the IP writer lock.  This will become a problem later
		 * when we want to relax IP's concurrency restrictions.
		 */
		ire->ire_marks |= IRE_MARK_CONDEMNED;
		return;
	}
	/* Remove IRE from whatever list it is on. */
	ire1 = ire->ire_next;
	if (ire1)
		ire1->ire_ptpn = ptpn;
	if (ptpn) {
		*ptpn = ire1;
		/* If it is a gateway, decrement the count. */
		if (ire->ire_type == IRE_DEFAULT)
			ip_def_gateway_count--;
	}

	/*
	 * when a default gateway is going away
	 * delete all the host redirects pointing at that
	 * gateway.
	 */
	if (ire->ire_type == IRE_DEFAULT)
		ire_delete_host_redirects(ire->ire_gateway_addr);

	/* Remember the global statistics from the dying */
	if (ire->ire_ipif) {
		ipif = ire->ire_ipif;
		/* "inbound" to a non local address is a forward */
		if (ire->ire_type & (IRE_LOCAL|IRE_BROADCAST))
			ipif->ipif_ib_pkt_count += ire->ire_ib_pkt_count;
		else
			ipif->ipif_fo_pkt_count += ire->ire_ib_pkt_count;
		ipif->ipif_ob_pkt_count += ire->ire_ob_pkt_count;
	}
	/* Free the xmit header, and the IRE itself. */
	if (ire->ire_ll_hdr_saved_mp)
		freeb(ire->ire_ll_hdr_saved_mp);
	if (ire->ire_ll_hdr_mp)
		freeb(ire->ire_ll_hdr_mp);
	if (ire->ire_mp)
		freeb(ire->ire_mp);
	else
		mi_free((char *)ire);
}

/*
 * ire_walk routine to delete all IRE_CACHE/IRE_HOST_REDIRECT entries
 * that have a given gateway address.  (Always called as writer.)
 */
void
ire_delete_route_gw(ire, cp)
	ire_t	* ire;
	char	* cp;
{
	ipaddr_t	gw_addr;

	if (!(ire->ire_type & (IRE_CACHE|IRE_HOST_REDIRECT)))
		return;

	bcopy(cp, (char *)&gw_addr, sizeof (gw_addr));
	if (ire->ire_gateway_addr == gw_addr) {
		ip1dbg(("ire_delete_route_gw: deleted 0x%x type %d to 0x%x\n",
			(int)ntohl(ire->ire_addr), ire->ire_type,
			(int)ntohl(ire->ire_gateway_addr)));
		ire_delete(ire);
	}
}

/*
 * Remove all IRE_CACHE entries that match
 * the ire specified.  (Always called
 * as writer.)
 *
 * The flag argument indicates if the
 * flush request is due to addition
 * of new route (IRE_FLUSH_ADD) or deletion of old
 * route (IRE_FLUSH_DELETE).
 *
 * This routine takes only the irs from forwarding
 * table and flushes the corresponding entries from
 * cache table.
 *
 * When flushing due to deletion of old route, it
 * just checks for the matching of cache handle(chandle)
 * deletes the one that matches.
 *
 * When flushing due to creation of a new route, it checks
 * if a cache entry matches its address with the one in IRE and
 * the cache entry 's parent has less specific mask than the
 * one in IRE.
 */
void
ire_flush_cache(ire, flag)
	ire_t	* ire;
	int	flag;
{
	int i;
	ire_t * cire;
	ire_t * cire_next;

#ifdef lint
	cire_next = NULL;
#endif
	if (ire->ire_type & IRE_CACHETABLE)
	    return;

	/*
	 * If a default is just created, there is no point
	 * in going through the cache, as there will not be any
	 * cached ires.
	 */
	if ((ire->ire_type == IRE_DEFAULT) && (flag))
		return;
	if (flag) {
		/*
		 * This selective flush is
		 * due to the addition of
		 * new IRE .
		 */
		for (i = 0; i < IP_CACHE_TABLE_SIZE; i++) {
			if ((cire = ip_cache_table[i]) == NULL)
				continue;
			for (; cire; cire = cire_next) {
				cire_next = cire->ire_next;
				if (cire->ire_type != IRE_CACHE)
					continue;
				if (((cire->ire_addr & cire->ire_cmask)
				    != (ire->ire_addr & cire->ire_cmask)) ||
				    ((cire->ire_addr & ire->ire_mask)
					!= (ire->ire_addr & ire->ire_mask)) ||
				    (ntohl(cire->ire_cmask) >
					ntohl(ire->ire_mask)))
					continue;
				ire_delete(cire);
			}
		}
	} else {
		/*
		 * delete the cache entries based on
		 * handle in the IRE as this IRE is
		 * being deleted/changed.
		 */
		for (i = 0; i < IP_CACHE_TABLE_SIZE; i++) {
			if ((cire = ip_cache_table[i]) == NULL)
				continue;
			for (; cire; cire = cire_next) {
				cire_next = cire->ire_next;
				if (cire->ire_type != IRE_CACHE)
					continue;
				if (cire->ire_chandle != ire->ire_chandle)
					continue;
				ire_delete(cire);
			}
		}
	}
}

/*
 * Matches the arguments passed with
 * the values in the ire.
 */
static boolean_t
ire_match_args(ire, addr, mask, gateway, type, ipif, wrq, match_flags)
	ire_t *ire;
	ipaddr_t addr;
	ipaddr_t mask;
	ipaddr_t gateway;
	int type;
	ipif_t *ipif;
	queue_t * wrq;
	int match_flags;
{

	if (((ire->ire_addr & ire->ire_mask) == (addr & mask)) &&
	    ((!(match_flags & MATCH_IRE_GW)) ||
		(ire->ire_gateway_addr == gateway)) &&
	    ((!(match_flags & MATCH_IRE_TYPE)) ||
		(ire->ire_type & type)) &&
	    ((!(match_flags & MATCH_IRE_SRC)) ||
		(ire->ire_src_addr == ipif->ipif_local_addr)) &&
	    ((!(match_flags & MATCH_IRE_RQ)) ||
		(ire->ire_stq == ipif->ipif_ill->ill_rq)) &&
	    ((!(match_flags & MATCH_IRE_WQ)) ||
		(ire->ire_stq == wrq)) &&
	    ((!(match_flags & MATCH_IRE_IPIF)) ||
		(ire->ire_ipif == ipif)))
		/* We found the matched IRE */
		return (true);

	return (false);
}


/*
 * Lookup for a route in all the tables
 */
ire_t *
ire_route_lookup(addr, mask, gateway, type, ipif, pire, wrq, flags)
	ipaddr_t addr;
	ipaddr_t mask;
	ipaddr_t gateway;
	int type;
	ipif_t *ipif;
	ire_t **pire;
	queue_t *wrq;
	int flags;
{
	ire_t *ire;

	/*
	 * check if ipif is valid when MATCH_IRE_RQ or
	 * MATCH_IRE_SRC is set.
	 */
	if ((flags & (MATCH_IRE_SRC | MATCH_IRE_RQ)) && (ipif == NULL))
		return (NULL);

	/*
	 * might be asking for a cache lookup,
	 * This is not best way to lookup cache,
	 * user should call ire_cache_lookup directly.
	 */
	if (flags & MATCH_IRE_TYPE) {
		if (type & IRE_CACHETABLE) {
			ire = ire_ctable_lookup(addr, gateway, type, ipif, wrq,
			    flags);
		} else
			ire = ire_ftable_lookup(addr, mask, gateway, type, ipif,
			    pire, wrq, flags);
	} else {
		ire = ire_ctable_lookup(addr, gateway, type, ipif, wrq, flags);
		if (!ire)
			ire = ire_ftable_lookup(addr, mask, gateway, type, ipif,
			    pire, wrq, flags);
	}
	return (ire);
}

/*
 * Lookup a route in forwarding table.
 * specific lookup is indicated by passing the
 * required parameters and indicating the
 * match required in flag field.
 *
 * Looking for default route can be done in three ways
 * 1) pass mask as 0 and set MATCH_IRE_MASK in flags field
 *    along with other matches.
 * 2) pass type as IRE_DEFAULT and set MATCH_IRE_TYPE in flags
 *    field along with other matches.
 * 3) if the destination and mask are passed as zeros.
 *
 * A request to return a default route if no route
 * is found, can be specified by setting MATCH_IRE_DEFAULT
 * in flags.
 *
 * It does not support recursion more than one level. It
 * will do recursive lookup only when the lookup maps to
 * a prefix or default route and MATCH_IRE_RECURSION flag is passed.
 *
 * If the routing table is setup to allow more than one level
 * of recursion, the cleaning up cache table will not work resulting
 * in invalid routing.
 */
ire_t *
ire_ftable_lookup(addr, mask, gateway, type, ipif, pire, wrq, flags)
	ipaddr_t addr;
	ipaddr_t mask;
	ipaddr_t gateway;
	int type;
	ipif_t *ipif;
	ire_t **pire;	/* returns the parent ire in this field */
	queue_t * wrq;
	int flags;
{
	ire_t **hash_ptr;
	u_int index;
	ire_t * ire = NULL;
	int i;

	/*
	 * check if ipif is valid when MATCH_IRE_RQ or
	 * MATCH_IRE_SRC is set.
	 */
	if ((flags & (MATCH_IRE_SRC | MATCH_IRE_RQ)) && (ipif == NULL))
		return (NULL);

	/*
	 * If the mask is known, the lookup
	 * is simple, if the mask is not know
	 * we need to search.
	 */
	if (flags & MATCH_IRE_MASK) {
		hash_ptr = (ire_t **)IP_FTABLE_HASH_PTR(addr, mask);
		if (hash_ptr == NULL)
			return (NULL);
		ire = *hash_ptr;
		for (; ire; ire = ire->ire_next) {
			if (ire_match_args(ire, addr, mask, gateway, type, ipif,
			    wrq, flags))
				goto found_ire;
		}
	} else {
		/*
		 * In this case we don't know the mask, we need to
		 * search the table assuming different mask sizes.
		 * we start with 32 bit mask, we don't allow default here.
		 */
		for (i = (IP_MASK_TABLE_SIZE - 1); i > 0; i--) {
			if ((ip_forwarding_table[i]) == NULL)
				continue;
			ire = *((ire_t **)
			    IP_FTABLE_HASH_PTR(addr, IP_MAP_MSIZE_TO_MASK(i)));
			for (; ire; ire = ire->ire_next) {
				if (ire_match_args(ire, addr, ire->ire_mask,
				    gateway, type, ipif, wrq, flags))
					goto found_ire;
			}
		}
	}
	/* we come here if no route is found */

	/*
	 * Handle the case where default route is
	 * requested by specifying type as one of the possible
	 * types for that can have a zero mask (IRE_DEFAULT and IRE_INTERFACE).
	 */
	if ((flags & MATCH_IRE_TYPE) && (type & (IRE_DEFAULT|IRE_INTERFACE))) {
		if ((ip_forwarding_table[0])) {
			ire = *((ire_t **)
			    IP_FTABLE_HASH_PTR(addr, (ipaddr_t)0));
			for (; ire; ire = ire->ire_next) {
				if (ire_match_args(ire, addr, (ipaddr_t)0,
				    gateway, type, ipif, wrq, flags))
					goto found_ire;
			}
		}
	}
	/*
	 * we come here only if no route is found.
	 * see if the default route can be used which is allowed
	 * only if the default matching criteria is specified.
	 * The ip_def_gateway_count tracks the number of IRE_DEFAULT
	 * entries. However, the ip_forwarding_table[0] also contains
	 * interface routes thus the count can be zero.
	 */
	if (flags & MATCH_IRE_DEFAULT)  {
		if ((ip_forwarding_table[0]) == NULL)
			return (NULL);
		ire = (ip_forwarding_table[0])[0];
		if (ire != NULL && ip_def_gateway_count != 0) {
			index = ip_def_gateway_index % ip_def_gateway_count;
			ip_def_gateway_index++;
			while (ire->ire_next && index--)
				ire = ire->ire_next;
		}
	}
found_ire:
	if (pire)
		*pire = NULL;
	if (ire && (flags & MATCH_IRE_RJ_BHOLE) &&
	    (ire->ire_flags & (RTF_BLACKHOLE | RTF_REJECT)))
		return (ire);
	if (ire && (flags & MATCH_IRE_RECURSIVE)) {
		if (ire->ire_type & (IRE_CACHETABLE | IRE_INTERFACE)) {
			return (ire);
		} else {
			if (pire)
				*pire = ire;
			ire = ire_route_lookup(ire->ire_gateway_addr, 0, 0, 0,
			    NULL, NULL, NULL, MATCH_IRE_DSTONLY);
			if (!ire)
				return (NULL);
			if ((ire->ire_type & (IRE_CACHETABLE|IRE_INTERFACE)) ==
			    0)
				return (NULL);
		}
	}
	return (ire);
}

/*
 * Looks up cache table for a route.
 * specific lookup can be indicated by
 * passing the MATCH_* flags and the
 * necessary parameters.
 */
ire_t *
ire_ctable_lookup(addr, gateway, type, ipif, wrq, flags)
	ipaddr_t addr;
	ipaddr_t gateway;
	int type;
	ipif_t * ipif;
	queue_t * wrq;
	int flags;
{
	ire_t *ire;

	/*
	 * check if ipif is valid when MATCH_IRE_RQ or
	 * MATCH_IRE_SRC is set.
	 */
	if ((flags & (MATCH_IRE_SRC | MATCH_IRE_RQ)) && (ipif == NULL))
		return (NULL);

	ire = *((ire_t **)IP_CTABLE_HASH_PTR(addr));
	for (; ire; ire = ire->ire_next) {
		if (ire_match_args(ire, addr, ire->ire_mask, gateway, type,
		    ipif, wrq, flags))
			return (ire);
	}
	return (NULL);
}

/*
 * Lookup cache
 */
ire_t *
ire_cache_lookup(addr)
	ipaddr_t addr;
{
	ire_t *ire;

	ire = *((ire_t **)IP_CTABLE_HASH_PTR(addr));
	for (; ire; ire = ire->ire_next) {
		if (ire->ire_addr == addr)
			return (ire);
	}
	return (NULL);
}

/*
 * Return the IRE_LOOPBACK, IRE_IF_RESOLVER or IRE_IF_NORESOLVER
 * ire associated with the specified ipif.
 * (May be called as writer.)
 */
ire_t *
ipif_to_ire(ipif)
	ipif_t	* ipif;
{
	ire_t	* ire;

	if (ipif->ipif_ire_type == IRE_LOOPBACK)
		ire = ire_ctable_lookup(ipif->ipif_local_addr, 0, IRE_LOOPBACK,
		    ipif, NULL, (MATCH_IRE_TYPE | MATCH_IRE_IPIF));
	else if (ipif->ipif_flags & IFF_POINTOPOINT)
		/* In this case we need to lookup destination address. */
		ire = ire_ftable_lookup(ipif->ipif_pp_dst_addr, IP_HOST_MASK, 0,
		    IRE_INTERFACE, ipif, NULL, NULL,
		    (MATCH_IRE_TYPE | MATCH_IRE_IPIF | MATCH_IRE_MASK));
	else
		ire = ire_ftable_lookup(ipif->ipif_local_addr,
		    ipif->ipif_net_mask, 0, IRE_INTERFACE, ipif, NULL, NULL,
		    (MATCH_IRE_TYPE | MATCH_IRE_IPIF | MATCH_IRE_MASK));
	return (ire);
}
