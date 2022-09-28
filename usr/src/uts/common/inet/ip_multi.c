/*
 * Copyright (c) 1992-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)ip_multi.c 1.24     96/10/14 SMI"

#define	_IP_MULTI_C

#ifndef	MI_HDRS
#include <sys/types.h>
#include <sys/stream.h>
#include <sys/dlpi.h>
#include <sys/stropts.h>
#include <sys/strlog.h>
#include <sys/ddi.h>
#include <sys/cmn_err.h>

#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <sys/sockio.h>
#include <sys/systm.h>
#include <net/route.h>
#include <netinet/in.h>
#include <net/if_dl.h>

#include <inet/common.h>
#include <inet/mi.h>
#include <inet/nd.h>
#include <inet/arp.h>
#include <inet/ip.h>
#include <inet/ip_if.h>
#include <inet/ip_ire.h>

#include <netinet/igmp.h>

#else

#include <types.h>
#include <stream.h>
#include <dlpi.h>
#include <stropts.h>
#include <strlog.h>
#include <tihdr.h>
#include <tiuser.h>

#include <socket.h>
#include <if.h>
#include <if_arp.h>
#include <sockio.h>
#include <route.h>
#include <in.h>

#include <common.h>
#include <mi.h>
#include <nd.h>
#include <lsystm.h>
#include <arp.h>
#include <ip.h>
#include <ip_if.h>
#include <ip_ire.h>

#include <igmp.h>
#endif

int	ip_addmulti(ipaddr_t  group, ipif_t * ipif);
int	ip_delmulti(ipaddr_t  group, ipif_t * ipif);
void	ip_multicast_loopback(queue_t * rq, mblk_t * mp_orig);
void	ip_wput_ctl(queue_t * q, mblk_t * mp_orig);
ilm_t	* ilm_lookup(ill_t * ill, ipaddr_t group);
ilm_t	* ilm_lookup_exact(ipif_t * ipif, ipaddr_t group);
static	int	ilm_numentries(ill_t * ill, ipaddr_t group);
void	ilm_free(ipif_t * ipif);
static int	ilm_add(ipif_t * ipif, ipaddr_t group);
static int	ilm_delete(ipif_t * ipif, ipaddr_t group);
static int	ip_ll_addmulti(ipif_t * ipif, ipaddr_t group);
static int	ip_ll_delmulti(ipif_t * ipif, ipaddr_t group);
static int	ip_join_allmulti(ipif_t * ipif);
static int	ip_leave_allmulti(ipif_t * ipif);
int 	ip_opt_add_group(ipc_t * ipc, int checkonly,
			ipaddr_t group, ipaddr_t ifaddr);
int	ip_opt_delete_group(ipc_t * ipc, int checkonly,
		ipaddr_t group, ipaddr_t ifaddr);
boolean_t	ilg_member(ipc_t * ipc, ipaddr_t group);
static boolean_t	ilg_member_exact(ipc_t * ipc, ipaddr_t group,
    ipif_t * ipif);
static int	ilg_add(ipc_t * ipc, ipaddr_t group, ipif_t * ipif);
static int	ilg_delete(ipc_t * ipc, ipaddr_t group, ipif_t * ipif);
void	ilg_delete_all(ipc_t * ipc);
static	void	ipc_delete_multicast_ipif(ipc_t * ipc, caddr_t arg);

void	reset_ilg_lower(ipif_t * ipif);
void	reset_ipc_multicast_ipif(ipif_t * ipif);
static	mblk_t	* ill_create_dl(ill_t *ill, u32 dl_primitive, u32 length,
    u32 * addr_lenp, u32 * addr_offp);
static	mblk_t	* ill_create_squery(ill_t * ill, ipaddr_t ipaddr, u32 addrlen,
    u32 addroff, mblk_t * mp_tail);

/*
 * INADDR_ANY means all multicast addresses. This is only used
 * by the multicast router.
 */
int
ip_addmulti(group, ipif)
	ipaddr_t	group;
	ipif_t	* ipif;
{
	ill_t	* ill = ipif->ipif_ill;
	ilm_t 	* ilm;
	int	err;

	ip1dbg(("ip_addmulti: 0x%x on %s\n", group, ill->ill_name));
	if (!CLASSD(group) && group != INADDR_ANY)
		return (EINVAL);

	if (ilm = ilm_lookup_exact(ipif, group)) {
		ilm->ilm_refcnt++;
		ip1dbg(("ip_addmulti: already there\n"));
		return (0);
	}

	err = ilm_add(ipif, group);
	if (err)
		return (err);

	if (group == INADDR_ANY) {
		/*
		 * Check how many ipif's that have members in this group -
		 * if more then one we should not tell the driver to join
		 * this time
		 */
		if (ilm_numentries(ill, group) > 1)
			return (0);
		return (ip_join_allmulti(ipif));
	}
	if ((ipif->ipif_flags & IFF_LOOPBACK) == 0)
		igmp_joingroup(ilm_lookup_exact(ipif, group));

	/*
	 * Check how many ipif's that have members in this group -
	 * if more then one we should not tell the driver to join
	 * this time
	 */
	if (ilm_numentries(ill, group) > 1)
		return (0);
	return (ip_ll_addmulti(ipif, group));
}

static int
ip_ll_addmulti(ipif, group)
	ipif_t	*ipif;
	ipaddr_t group;
{
	ill_t	*ill = ipif->ipif_ill;
	mblk_t	*mp;
	u32	addrlen, addroff;

	ip1dbg(("ip_ll_addmulti: 0x%x on %s (0x%x)\n",
		(int)ntohl(group), ill->ill_name,
		(int)ntohl(ipif->ipif_local_addr)));

	if (ill->ill_net_type != IRE_IF_RESOLVER ||
	    ipif->ipif_flags & IFF_POINTOPOINT) {
		ip1dbg(("ip_ll_addmulti: not resolver\n"));
		return (0);	/* Must be IRE_IF_NORESOLVER */
	}

	if (ipif->ipif_flags & IFF_MULTI_BCAST) {
		ip1dbg(("ip_ll_addmulti: MULTI_BCAST\n"));
		return (0);
	}
	if (ill->ill_ipif_up_count == 0) {
		/*
		 * Nobody there. All multicast addresses will be re-joined
		 * when we get the DL_BIND_ACK bringing the interface up.
		 */
		ip1dbg(("ip_ll_addmulti: nobody up\n"));
		return (0);
	}

	/*
	 * Create a AR_ENTRY_SQUERY message with a dl_enabmulti_req tacked
	 * on.
	 */
	mp = ill_create_dl(ill, DL_ENABMULTI_REQ, sizeof (dl_enabmulti_req_t),
	    &addrlen, &addroff);
	if (!mp)
		return (ENOMEM);
	mp = ill_create_squery(ill, group, addrlen, addroff, mp);
	if (!mp)
		return (ENOMEM);
	ip1dbg(("ip_ll_addmulti: putnext 0x%x on %s\n",
	    (int)ntohl(group), ill->ill_name));
	putnext(ill->ill_rq, mp);
	return (0);
}

/*
 * INADDR_ANY means all multicast addresses. This is only used
 * by the multicast router.
 */
int
ip_delmulti(group, ipif)
	ipaddr_t	group;
	ipif_t	*ipif;
{
	ill_t	*ill = ipif->ipif_ill;
	ilm_t *ilm;

	ip1dbg(("ip_delmulti: 0x%x on %s\n", group, ill->ill_name));
	if (!CLASSD(group) && group != INADDR_ANY)
		return (EINVAL);

	if (!(ilm = ilm_lookup_exact(ipif, group))) {
		return (ENOENT);
	}
	ilm->ilm_refcnt--;
	if (ilm->ilm_refcnt > 0) {
		ip1dbg(("ip_delmulti: still %d left\n", ilm->ilm_refcnt));
		return (0);
	}

	if (group == INADDR_ANY) {
		/*
		 * Check how many ipif's that have members in this group -
		 * if there are still some left then don't tell the driver
		 * to drop it.
		 */
		(void) ilm_delete(ipif, group);
		if (ilm_numentries(ill, group) != 0)
			return (0);
		return (ip_leave_allmulti(ipif));
	}

	if ((ipif->ipif_flags & IFF_LOOPBACK) == 0)
		igmp_leavegroup(ilm);
	(void) ilm_delete(ipif, group);
	/*
	 * Check how many ipif's that have members in this group -
	 * if there are still some left then don't tell the driver
	 * to drop it.
	 */
	if (ilm_numentries(ill, group) != 0)
		return (0);
	return (ip_ll_delmulti(ipif, group));
}

static int
ip_ll_delmulti(ipif, group)
	ipif_t	*ipif;
	ipaddr_t group;
{
	ill_t	*ill = ipif->ipif_ill;
	mblk_t	*mp;
	u32	addrlen, addroff;

	ip1dbg(("ip_ll_delmulti: 0x%x on %s (0x%x)\n",
		(int)ntohl(group), ill->ill_name,
		(int)ntohl(ipif->ipif_local_addr)));

	if (ill->ill_net_type != IRE_IF_RESOLVER ||
	    ipif->ipif_flags & IFF_POINTOPOINT) {
		return (0);	/* Must be IRE_IF_NORESOLVER */
	}
	if (ipif->ipif_flags & IFF_MULTI_BCAST) {
		ip1dbg(("ip_ll_delmulti: MULTI_BCAST\n"));
		return (0);
	}
	if (ill->ill_ipif_up_count == 0) {
		/*
		 * Nobody there. All multicast addresses will be re-joined
		 * when we get the DL_BIND_ACK bringing the interface up.
		 */
		ip1dbg(("ip_ll_delmulti: nobody up\n"));
		return (0);
	}

	/*
	 * Create a AR_ENTRY_SQUERY message with a dl_disabmulti_req tacked
	 * on.
	 */
	mp = ill_create_dl(ill, DL_DISABMULTI_REQ,
	    sizeof (dl_disabmulti_req_t), &addrlen, &addroff);
	if (!mp)
		return (ENOMEM);
	mp = ill_create_squery(ill, group, addrlen, addroff, mp);
	if (!mp)
		return (ENOMEM);
	ip1dbg(("ip_ll_delmulti: putnext 0x%x on %s\n",
		(int)ntohl(group), ill->ill_name));
	putnext(ill->ill_rq, mp);
	return (0);
}


/*
 * Make the driver pass up all multicast packets
 */
static int
ip_join_allmulti(ipif)
	ipif_t	*ipif;
{
	ill_t	*ill = ipif->ipif_ill;
	mblk_t	*mp;
	u32	addrlen, addroff;

	ip1dbg(("ip_join_allmulti: on %s addr 0x%x\n", ill->ill_name,
		(int)ntohl(ipif->ipif_local_addr)));

	if (ill->ill_net_type != IRE_IF_RESOLVER ||
	    ipif->ipif_flags & IFF_POINTOPOINT) {
		ip1dbg(("ip_join_allmulti: not resolver\n"));

		return (0);	/* Must be IRE_IF_NORESOLVER */
	}
	if (ipif->ipif_flags & IFF_MULTI_BCAST)
		return (0);

	if (ill->ill_ipif_up_count == 0) {
		/*
		 * Nobody there. All multicast addresses will be re-joined
		 * when we get the DL_BIND_ACK bringing the interface up.
		 */
		return (0);
	}
	/* Create a dl_promiscon_req message */
	mp = ill_create_dl(ill, DL_PROMISCON_REQ, sizeof (dl_promiscon_req_t),
	    &addrlen, &addroff);
	if (!mp)
		return (ENOMEM);
	/*
	 * send this directly to the DLPI provider - not through the resolver
	 */
	putnext(ill->ill_wq, mp);
	return (0);
}

/*
 * Make the driver stop passing up all multicast packets
 */
static int
ip_leave_allmulti(ipif)
	ipif_t	*ipif;
{
	ill_t	*ill = ipif->ipif_ill;
	mblk_t	*mp;
	u32	addrlen, addroff;

	ip1dbg(("ip_leave_allmulti: on %s addr 0x%x\n", ill->ill_name,
		(int)ntohl(ipif->ipif_local_addr)));

	if (ill->ill_net_type != IRE_IF_RESOLVER ||
	    ipif->ipif_flags & IFF_POINTOPOINT) {
		ip1dbg(("ip_leave_allmulti: not resolver\n"));

		return (0);	/* Must be IRE_IF_NORESOLVER */
	}
	if (ipif->ipif_flags & IFF_MULTI_BCAST)
		return (0);

	if (ill->ill_ipif_up_count == 0) {
		/*
		 * Nobody there. All multicast addresses will be re-joined
		 * when we get the DL_BIND_ACK bringing the interface up.
		 */
		return (0);
	}
	/* Create a dl_promiscoff_req message */
	mp = ill_create_dl(ill, DL_PROMISCOFF_REQ,
	    sizeof (dl_promiscoff_req_t), &addrlen, &addroff);
	if (!mp)
		return (ENOMEM);
	/*
	 * send this directly to the DLPI provider - not through the resolver
	 */
	putnext(ill->ill_wq, mp);
	return (0);
}

/*
 * Copy mp_orig and pass it in as a local message
 * Note the rq should be ill_rq and nothing else - icmp_inbound depends
 * on q_ptr being an ill and not an ipc.
 */
void
ip_multicast_loopback(rq, mp_orig)
	queue_t	* rq;
	mblk_t	* mp_orig;
{
	mblk_t	* mp;

	ip1dbg(("ip_multicast_loopback\n"));
	/* TODO this could use dup'ed messages except for the IP header. */
	mp = copymsg(mp_orig);
	if (mp)
		ip_wput_local(rq, (ipha_t *)ALIGN32(mp->b_rptr), mp,
		    IRE_BROADCAST, rq);
#ifdef IP_DEBUG
	else
		ip1dbg(("ip_multicast_loopback: copymsg failed: "
		    "base 0x%x, limit 0x%x, read 0x%x, write 0x%x\n",
		    (int)mp_orig->b_datap->db_base,
		    (int)mp_orig->b_datap->db_lim,
		    (int)mp_orig->b_rptr, (int)mp_orig->b_wptr));
#endif / *IP_DEBUG */

}

static	area_t	ip_aresq_template = {
	AR_ENTRY_SQUERY,		/* cmd */
	sizeof (area_t)+IP_ADDR_LEN,	/* name offset */
	sizeof (area_t),	/* name len (filled by ill_arp_alloc) */
	IP_ARP_PROTO_TYPE,		/* protocol, from arps perspective */
	sizeof (area_t),			/* proto addr offset */
	IP_ADDR_LEN,			/* proto addr_length */
	0,				/* proto mask offset */
	/* Rest is initialized when used */
	0,				/* flags */
	0,				/* hw addr offset */
	0,				/* hw addr length */
};


static mblk_t *
ill_create_squery(ill, ipaddr, addrlen, addroff, mp_tail)
	ill_t	*ill;
	ipaddr_t ipaddr;
	u32	addrlen;
	u32	addroff;	/* Offset into mp_tail */
	mblk_t	*mp_tail;
{
	mblk_t	*mp;
	area_t	*area;

	mp = ill_arp_alloc(ill, (u_char *)&ip_aresq_template, ipaddr);
	if (!mp) {
		freemsg(mp_tail);
		return (nilp(mblk_t));
	}
	area = (area_t *)ALIGN32(mp->b_rptr);
	area->area_hw_addr_length = addrlen;
	area->area_hw_addr_offset = mp->b_wptr - mp->b_rptr + addroff;

	mp->b_cont = mp_tail;
	return (mp);
}

/*
 * Create a dlpi message with room for phys+sap. When we come back in
 * ip_wput_ctl() we will strip the sap for those primitives which
 * only need a physical address.
 */
static mblk_t *
ill_create_dl(ill, dl_primitive, length, addr_lenp, addr_offp)
	ill_t	*ill;
	u32 	dl_primitive;
	u32	length;
	u32	*addr_lenp;
	u32	*addr_offp;
{
	mblk_t	*mp;
	u32	hw_addr_length;
	char 	*cp;
	u32	offset;
	u32 	size;

	*addr_lenp = *addr_offp = 0;

	hw_addr_length = ill->ill_phys_addr_length;
	if (!hw_addr_length) {
		ip0dbg(("ip_create_dl: hw addr length = 0\n"));
		return (nilp(mblk_t));
	}
	hw_addr_length += ((ill->ill_sap_length > 0) ? ill->ill_sap_length :
	    -ill->ill_sap_length);

	size = length;
	switch (dl_primitive) {
	case DL_ENABMULTI_REQ:
	case DL_DISABMULTI_REQ:
	case DL_UNITDATA_REQ:
		size += hw_addr_length;
		break;
	case DL_PROMISCON_REQ:
	case DL_PROMISCOFF_REQ:
		break;
	default:
		return (nilp(mblk_t));
	}
	mp = allocb(size, BPRI_HI);
	if (!mp)
		return (nilp(mblk_t));
	mp->b_wptr += size;
	mp->b_datap->db_type = M_PROTO;

	cp = (char *)mp->b_rptr;
	offset = length;

	switch (dl_primitive) {
	case DL_ENABMULTI_REQ: {
		dl_enabmulti_req_t *dl = (dl_enabmulti_req_t *)ALIGN32(cp);

		dl->dl_primitive = dl_primitive;
		dl->dl_addr_offset = offset;
		dl->dl_addr_length = *addr_lenp = hw_addr_length;
		*addr_offp = offset;
		break;
	}
	case DL_DISABMULTI_REQ: {
		dl_disabmulti_req_t *dl = (dl_disabmulti_req_t *)ALIGN32(cp);

		dl->dl_primitive = dl_primitive;
		dl->dl_addr_offset = offset;
		dl->dl_addr_length = *addr_lenp = hw_addr_length;
		*addr_offp = offset;
		break;
	}
	case DL_UNITDATA_REQ: {
		dl_unitdata_req_t *dl = (dl_unitdata_req_t *)ALIGN32(cp);

		dl->dl_primitive = dl_primitive;
		dl->dl_dest_addr_offset = offset;
		dl->dl_dest_addr_length = *addr_lenp = hw_addr_length;
		*addr_offp = offset;
		break;
	}
	case DL_PROMISCON_REQ:
	case DL_PROMISCOFF_REQ: {
		dl_promiscon_req_t *dl = (dl_promiscon_req_t *)ALIGN32(cp);

		dl->dl_primitive = dl_primitive;
		dl->dl_level = DL_PROMISC_MULTI;
		*addr_lenp = *addr_offp = 0;
		break;
	}
	}
	ip1dbg(("ill_create_dl: addr_len %d, addr_off %d\n",
		*addr_lenp, *addr_offp));
	return (mp);
}

void
ip_wput_ctl(q, mp_orig)
	queue_t	* q;
	mblk_t	* mp_orig;
{
	ill_t	* ill = (ill_t *)q->q_ptr;
	mblk_t 	* mp = mp_orig;
	area_t	* area;
	u8	* cp;

	ip1dbg(("ip_wput_ctl\n"));
	/* Check that we have a AR_ENTRY_SQUERY with a tacked on mblk */
	if ((mp->b_wptr - mp->b_rptr) < sizeof (area_t) ||
	    mp->b_cont == nilp(mblk_t)) {
		putnext(q, mp);
		return;
	}
	area = (area_t *)ALIGN32(mp->b_rptr);
	if (area->area_cmd != AR_ENTRY_SQUERY) {
		putnext(q, mp);
		return;
	}
	mp = mp->b_cont;
	cp = (u8 *)mp->b_rptr;
	/*
	 * Update dl_addr_length and dl_addr_offset for primitives that
	 * have physical addresses as opposed to full saps
	 */
	switch (((union DL_primitives *)ALIGN32(mp->b_rptr))->dl_primitive) {
	case DL_ENABMULTI_REQ: {
		dl_enabmulti_req_t *dl = (dl_enabmulti_req_t *)ALIGN32(cp);

		/*
		 * Remove the sap from the DL address either at the end or
		 * in front of the physical address.
		 */
		if (ill->ill_sap_length < 0)
			dl->dl_addr_length += ill->ill_sap_length;
		else {
			dl->dl_addr_offset += ill->ill_sap_length;
			dl->dl_addr_length -= ill->ill_sap_length;
		}
		ip1dbg(("ip_wput_ctl: ENABMULTI\n"));
		break;
	}
	case DL_DISABMULTI_REQ: {
		dl_disabmulti_req_t *dl = (dl_disabmulti_req_t *)ALIGN32(cp);

		/*
		 * Remove the sap from the DL address either at the end or
		 * in front of the physical address.
		 */
		if (ill->ill_sap_length < 0)
			dl->dl_addr_length += ill->ill_sap_length;
		else {
			dl->dl_addr_offset += ill->ill_sap_length;
			dl->dl_addr_length -= ill->ill_sap_length;
		}
		ip1dbg(("ip_wput_ctl: DELMULTI\n"));
		break;
	}
	default:
		ip1dbg(("ip_wput_ctl: default\n"));
		break;
	}
	freeb(mp_orig);
	putnext(q, mp);
}

void
ill_add_multicast(ill)
	ill_t	* ill;
{
	ilm_t	*ilm;

	for (ilm = ill->ill_ilm; ilm; ilm = ilm->ilm_next) {
		/*
		 * Check how many ipif's that have members in this group -
		 * if more then one we make sure that this entry is first
		 * in the list.
		 */
		if (ilm_numentries(ill, ilm->ilm_addr) > 1 &&
		    ilm_lookup(ill, ilm->ilm_addr) != ilm)
			continue;
		ip1dbg(("ill_add_multicast: 0x%x\n",
			(int)ntohl(ilm->ilm_addr)));
		if (ilm->ilm_addr == INADDR_ANY)
			(void) ip_join_allmulti(ill->ill_ipif);
		else
			(void) ip_ll_addmulti(ill->ill_ipif, ilm->ilm_addr);
	}
}

void
ill_delete_multicast(ill)
	ill_t	* ill;
{
	ilm_t	*ilm;

	for (ilm = ill->ill_ilm; ilm; ilm = ilm->ilm_next) {
		/*
		 * Check how many ipif's that have members in this group -
		 * if more then one we make sure that this entry is first
		 * in the list.
		 */
		if (ilm_numentries(ill, ilm->ilm_addr) > 1 &&
		    ilm_lookup(ill, ilm->ilm_addr) != ilm)
			continue;
		ip1dbg(("ill_delete_multicast: 0x%x\n",
			(int)ntohl(ilm->ilm_addr)));
		if (ilm->ilm_addr == INADDR_ANY)
			(void) ip_leave_allmulti(ill->ill_ipif);
		else
			(void) ip_ll_delmulti(ill->ill_ipif, ilm->ilm_addr);
	}
}


ilm_t *
ilm_lookup(ill, group)
	ill_t	*ill;
	ipaddr_t group;
{
	ilm_t	*ilm;

	for (ilm = ill->ill_ilm; ilm; ilm = ilm->ilm_next)
		if (ilm->ilm_addr == group)
			return (ilm);
	return (nilp(ilm_t));
}

ilm_t *
ilm_lookup_exact(ipif, group)
	ipif_t	*ipif;
	ipaddr_t group;
{
	ill_t	*ill = ipif->ipif_ill;
	ilm_t	*ilm;

	for (ilm = ill->ill_ilm; ilm; ilm = ilm->ilm_next)
		if (ilm->ilm_addr == group && ilm->ilm_ipif == ipif)
			return (ilm);
	return (nilp(ilm_t));
}

static int
ilm_numentries(ill, group)
	ill_t	*ill;
	ipaddr_t group;
{
	ilm_t	*ilm;
	int i = 0;

	for (ilm = ill->ill_ilm; ilm; ilm = ilm->ilm_next)
		if (ilm->ilm_addr == group)
			i++;
	return (i);
}

#define	GETSTRUCT(structure, number)	\
((structure *)ALIGN32(mi_zalloc((u_int)(sizeof (structure) * (number)))))

/* Caller guarantees that the group is not on the list */
static int
ilm_add(ipif, group)
	ipif_t	*ipif;
	ipaddr_t group;
{
	ill_t	*ill = ipif->ipif_ill;
	ilm_t	*ilm;

	ilm = GETSTRUCT(ilm_t, 1);
	if (ilm == nilp(ilm_t))
		return (ENOMEM);
	ilm->ilm_addr = group;
	ilm->ilm_refcnt = 1;
	ilm->ilm_ipif = ipif;
	ilm->ilm_next = ill->ill_ilm;
	ill->ill_ilm = ilm;
	return (0);
}

static int
ilm_delete(ipif, group)
	ipif_t	*ipif;
	ipaddr_t group;
{
	ill_t	*ill = ipif->ipif_ill;
	ilm_t	*ilm;
	ilm_t	**ilmp;

	if (!(ilm = ilm_lookup_exact(ipif, group)))
		return (ENOENT);
	for (ilmp = &ill->ill_ilm; *ilmp; ilmp = &(*ilmp)->ilm_next)
		if (*ilmp == ilm) {
			*ilmp = ilm->ilm_next;
			break;
		}
	mi_free((char *)ilm);
	return (0);
}

void
ilm_free(ipif)
	ipif_t	*ipif;
{
	ill_t	*ill = ipif->ipif_ill;
	ilm_t	*ilm, *next_ilm;
#ifdef lint
	next_ilm = nilp(ilm_t);
#endif
	for (ilm = ill->ill_ilm; ilm; ilm = next_ilm) {
		next_ilm = ilm->ilm_next;
		if (ilm->ilm_ipif == ipif)
			(void) ilm_delete(ilm->ilm_ipif, ilm->ilm_addr);
	}
}

/*
 * These are the handling routines for the current optmgmt calls.
 */
int
ip_opt_add_group(ipc, checkonly, group, ifaddr)
	ipc_t	*ipc;
	int	checkonly;
	ipaddr_t group;
	ipaddr_t ifaddr;
{
	ipif_t	*ipif;

	if (!CLASSD(group))
		return (EINVAL);

	if (ifaddr == 0)
		ipif = ipif_lookup_group(group);
	else
		ipif = ipif_lookup_addr(ifaddr);

	if (ipif == nilp(ipif_t)) {
		ip1dbg((
		    "ip_opt_add_group: no ipif for group 0x%x, ifaddr 0x%x\n",
			(int)ntohl(group), (int)ntohl(ifaddr)));
		return (EADDRNOTAVAIL);
	}
	if (checkonly) {
		/*
		 * do not do operation, just pretend to - new T_CHECK
		 * semantics. The error return case above if encountered
		 * considered a good enough "check" here.
		 */
		return (0);
	}
	return (ilg_add(ipc, group, ipif));
}

int
ip_opt_delete_group(ipc, checkonly, group, ifaddr)
	ipc_t	*ipc;
	int	checkonly;
	ipaddr_t group;
	ipaddr_t ifaddr;
{
	ipif_t	*ipif;

	if (!CLASSD(group))
		return (EINVAL);

	if (ifaddr == 0)
		ipif = ipif_lookup_group(group);
	else
		ipif = ipif_lookup_addr(ifaddr);

	if (ipif == nilp(ipif_t)) {
		ip1dbg(("ip_opt_delete_group: no ipif for group "
		    "0x%x, ifaddr 0x%x\n",
		    (int)ntohl(group), (int)ntohl(ifaddr)));
		return (EADDRNOTAVAIL);
	}
	if (checkonly) {
		/*
		 * do not do operation, just pretend to - new T_CHECK
		 * semantics.The error return case above if encountered
		 * considered a good enough "check" here.
		 */
		return (0);
	}
	return (ilg_delete(ipc, group, ipif));
}

/*
 * Group mgmt for upper ipc that passes things down
 * to the interface multicast list (and DLPI)
 * These routines can handle new style options that specify an interface name
 * as opposed to an interface address (needed for general handling of
 * unnumbered interfaces.)
 */

#define	ILG_ALLOC_CHUNK	16

/*
 * Add a group to an upper ipc group data structure and pass things down
 * to the interface multicast list (and DLPI)
 */
static int
ilg_add(ipc, group, ipif)
	ipc_t	*ipc;
	ipaddr_t group;
	ipif_t	*ipif;
{
	int 	error;

	if (!(ipif->ipif_flags & IFF_MULTICAST))
		return (EADDRNOTAVAIL);

	if (ilg_member_exact(ipc, group, ipif))
		return (EADDRINUSE);

	if (error = ip_addmulti(group, ipif))
		return (error);
	if (!ipc->ipc_ilg) {
		/* Allocate first chunk */
		ipc->ipc_ilg_allocated = ILG_ALLOC_CHUNK;
		ipc->ipc_ilg = GETSTRUCT(ilg_t, ILG_ALLOC_CHUNK);
		ipc->ipc_ilg_inuse = 0;
	}
	if (ipc->ipc_ilg_inuse >= ipc->ipc_ilg_allocated) {
		/* Allocate next larger chunk and copy old into new */
		ilg_t	*new;

		new = GETSTRUCT(ilg_t,
				ipc->ipc_ilg_allocated + ILG_ALLOC_CHUNK);
		bcopy((char *)ipc->ipc_ilg, (char *)new,
		    sizeof (ilg_t) * ipc->ipc_ilg_allocated);
		mi_free((char *)ipc->ipc_ilg);
		ipc->ipc_ilg = new;
		ipc->ipc_ilg_allocated += ILG_ALLOC_CHUNK;
	}
	ipc->ipc_ilg[ipc->ipc_ilg_inuse].ilg_group = group;
	ipc->ipc_ilg[ipc->ipc_ilg_inuse++].ilg_lower = ipif;
	return (0);
}

boolean_t
ilg_member(ipc, group)
	ipc_t	*ipc;
	ipaddr_t group;
{
	int	i;

	for (i = 0; i < ipc->ipc_ilg_inuse; i++)
		if (ipc->ipc_ilg[i].ilg_group == group)
			return (true);
	return (false);
}

static boolean_t
ilg_member_exact(ipc, group, ipif)
	ipc_t	*ipc;
	ipaddr_t group;
	ipif_t	*ipif;
{
	int	i;

	for (i = 0; i < ipc->ipc_ilg_inuse; i++)
		if (ipc->ipc_ilg[i].ilg_group == group &&
		    ipc->ipc_ilg[i].ilg_lower == ipif)
			return (true);
	return (false);
}

static int
ilg_delete(ipc, group, ipif)
	ipc_t	*ipc;
	ipaddr_t group;
	ipif_t	*ipif;
{
	int	i;

	if (!(ipif->ipif_flags & IFF_MULTICAST))
		return (EADDRNOTAVAIL);

	if (!ilg_member_exact(ipc, group, ipif))
		return (ENOENT);

	(void) ip_delmulti(group, ipif);

	for (i = 0; i < ipc->ipc_ilg_inuse; i++)
		if (ipc->ipc_ilg[i].ilg_group == group &&
		    ipc->ipc_ilg[i].ilg_lower == ipif) {
			/* Move other entries up one step */
			ipc->ipc_ilg_inuse--;
			for (; i < ipc->ipc_ilg_inuse; i++)
				ipc->ipc_ilg[i] = ipc->ipc_ilg[i+1];
			break;
		}
	if (ipc->ipc_ilg_inuse == 0) {
		mi_free((char *)ipc->ipc_ilg);
		ipc->ipc_ilg = nilp(ilg_t);
	}
	return (0);
}

void
ilg_delete_all(ipc)
	ipc_t	*ipc;
{
	int	i;

	if (!ipc->ipc_ilg_inuse)
		return;

	for (i = ipc->ipc_ilg_inuse - 1; i >= 0; i--)
		(void) ilg_delete(ipc, ipc->ipc_ilg[i].ilg_group,
		    ipc->ipc_ilg[i].ilg_lower);
}

static void
ipc_delete_ilg_lower(ipc, arg)
	ipc_t	* ipc;
	caddr_t arg;
{
	ipif_t	* ipif = (ipif_t *)ALIGN32(arg);
	int	i;

	for (i = ipc->ipc_ilg_inuse - 1; i >= 0; i--) {
		if (ipc->ipc_ilg[i].ilg_lower == ipif) {
			/* Blow away the membership */
			ip1dbg(("ipc_delete_ilg_lower: 0x%x on 0x%x (%s)\n",
			    (int)ntohl(ipc->ipc_ilg[i].ilg_group),
			    (int)ntohl(ipif->ipif_local_addr),
			    ipif->ipif_ill->ill_name));
			(void) ilg_delete(ipc,
			    ipc->ipc_ilg[i].ilg_group,
			    ipc->ipc_ilg[i].ilg_lower);
		}
	}
}

/*
 * Called when a lower interface is removed (closed)
 * to make sure that there are no dangling ilg_lower to that ipif.
 */
void
reset_ilg_lower(ipif)
	ipif_t	*ipif;
{
	ipc_walk(ipc_delete_ilg_lower, (caddr_t)ipif);
}

static void
ipc_delete_multicast_ipif(ipc, arg)
	ipc_t	*ipc;
	caddr_t arg;
{
	ipif_t	*ipif = (ipif_t *)ALIGN32(arg);

	if (ipc->ipc_multicast_ipif == ipif)
		/* Revert to late binding */
		ipc->ipc_multicast_ipif = nilp(ipif_t);
}

/*
 * Called when an IPIF is deleted...
 * to make sure that there are no dangling ipc_multicast_ipif to that
 * interface.
 */
void
reset_ipc_multicast_ipif(ipif)
	ipif_t	*ipif;
{
	ipc_walk(ipc_delete_multicast_ipif, (caddr_t)ipif);
}
