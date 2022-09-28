/*
 * Copyright (c) 1991, by Sun Microsystems, Inc.
 */

#ifndef	_NETINET_IP_MROUTE_H
#define	_NETINET_IP_MROUTE_H

#pragma ident	"@(#)ip_mroute.h	1.13	96/10/14 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Definitions for the kernel part of DVMRP,
 * a Distance-Vector Multicast Routing Protocol.
 * (See RFC-1075.)
 *
 * Written by David Waitzman, BBN Labs, August 1988.
 * Modified by Steve Deering, Stanford, February 1989.
 * Modified by Ajit Thyagarajan, PARC, August 1993.
 * Modified by Ajit Thyagarajan, PARC, August 1994.
 *
 * MROUTING 3.5
 */

/* #define	UPCALL_TIMING */
#ifdef UPCALL_TIMING
#define	UPCALL_MAX_TIME	51
#endif

/*
 * DVMRP-specific setsockopt commands.
 */

#define	MRT_INIT		100	/* initialize forwarder */
#define	MRT_DONE		101	/* shut down forwarder */
#define	MRT_ADD_VIF		102	/* create virtual interface */
#define	MRT_DEL_VIF		103	/* delete virtual interface */
#define	MRT_ADD_MFC		104	/* insert forwarding cache entry */
#define	MRT_DEL_MFC		105	/* delete forwarding cache entry */
#define	MRT_VERSION		106	/* get kernel version number */
#define	MRT_ASSERT		107	/* enable PIM assert processing */

/*
 * Types and macros for handling bitmaps with one bit per virtual interface.
 */
#define	MAXVIFS			32
typedef u_long			vifbitmap_t;
typedef u_short			vifi_t;	/* type of a vif index */
#define	ALL_VIFS		(vifi_t)-1

#define	VIFM_SET(n, m)		((m) |=  (1 << (n)))
#define	VIFM_CLR(n, m)		((m) &= ~(1 << (n)))
#define	VIFM_ISSET(n, m)	((m) &   (1 << (n)))
#define	VIFM_CLRALL(m)		((m) = 0x00000000)
#define	VIFM_COPY(mfrom, mto)	((mto) = (mfrom))
#define	VIFM_SAME(m1, m2)	((m1) == (m2))


/*
 * Argument structure for MRT_ADD_VIF. Also used for netstat.
 * (MRT_DEL_VIF takes a single vifi_t argument.)
 */
struct vifctl {
    vifi_t	vifc_vifi;	/* the index of the vif to be added   */
	u_char	vifc_flags;	/* VIFF_ flags defined below	*/
	u_char	vifc_threshold;		/* min ttl required to forward on vif */
	u_int	vifc_rate_limit;	/* max rate	*/
	struct	in_addr	vifc_lcl_addr;	/* local interface address	*/
	struct	in_addr	vifc_rmt_addr;	/* remote address(tunnels only)	*/
	/*
	 * vifc_pkt_in/out in Solaris, to report out of the kernel.
	 * Not nec. in BSD.
	 */
	u_long	vifc_pkt_in;		/* # Pkts in on interface	*/
	u_long	vifc_pkt_out;		/* # Pkts out on interface	*/
};

#define	VIFF_TUNNEL	0x1		/* vif represents a tunnel end-point */
#define	VIFF_SRCRT	0x2		/* tunnel uses IP src routing	*/

/*
 * Argument structure for MRT_ADD_MFC and MRT_DEL_MFC
 * (mfcc_tos to be added at a future point)
 */
struct mfcctl {
    struct	in_addr	mfcc_origin;	/* ip origin of mcasts	*/
    struct in_addr	mfcc_mcastgrp; 	/* multicast group associated */
    vifi_t		mfcc_parent;	/* incoming vif	*/
    u_long		mfcc_pkt_cnt;	/* pkt count for src-grp	*/
    u_char		mfcc_ttls[MAXVIFS]; 	/* forwarding ttls on vifs    */
};

/*
 * The kernel's multicast routing statistics.
 */
struct mrtstat {
    u_long	mrts_mfcfind_lookups;	/* #forwarding cache table lookups */
    u_long	mrts_mfcfind_misses;    /* # forwarding cache table misses */
    u_long	mrts_mfc_hits;		/* forwarding pkt mfctable hits	   */
    u_long	mrts_mfc_misses;	/* forwarding pkt mfctable misses  */
    u_long	mrts_upcalls;		/* # calls to mrouted		   */
    u_long	mrts_fwd_in;		/* # packets potentially forwarded */
    u_long	mrts_fwd_out;		/* # resulting outgoing packets    */
    u_long	mrts_fwd_drop;		/* # dropped for lack of resources */
    u_long	mrts_bad_tunnel;	/* malformed tunnel options	   */
    u_long	mrts_cant_tunnel;	/* no room for tunnel options	   */
    u_long	mrts_wrong_if;		/* arrived on wrong interface	   */
    u_long	mrts_upq_ovflw;		/* upcall Q overflow		   */
    u_long	mrts_cache_cleanups;	/* # entries with no upcalls	   */
    u_long	mrts_drop_sel;		/* pkts dropped selectively	   */
    u_long	mrts_q_overflow;	/* pkts dropped - Q overflow	   */
    u_long	mrts_pkt2large;		/* pkts dropped - size > BKT SIZE  */
#ifdef UPCALL_TIMING
    hrtime_t	upcall_data[UPCALL_MAX_TIME];	/* upcall delay stats	*/
#endif UPCALL_TIMING
};

/*
 * Argument structure used by mrouted to get src-grp pkt counts
 */
struct sioc_sg_req {
    struct in_addr src;
    struct in_addr grp;
    u_long pktcnt;
    u_long bytecnt;
    u_long wrong_if;
};

/*
 * Argument structure used by mrouted to get vif pkt counts
 */
struct sioc_vif_req {
    vifi_t vifi;		/* vif number				*/
    u_long icount;		/* Input packet count on vif		*/
    u_long ocount;		/* Output packet count on vif		*/
    u_long ibytes;		/* Input byte count on vif		*/
    u_long obytes;		/* Output byte count on vif		*/
};

#ifdef _KERNEL
/*
 * The kernel's virtual-interface structure.
 */
struct vif {
	u_char		v_flags;	/* VIFF_ flags defined above	*/
	u_char		v_threshold;	/* Min ttl required to forward on vif */
	u_int		v_rate_limit;	/* Max rate, in kbits/sec	*/
	struct tbf	*v_tbf;		/* Token bkt structure at intf.	*/
	struct in_addr	v_lcl_addr;	/* Local interface address	*/
	struct in_addr	v_rmt_addr;	/* Remote address(tunnels only)	*/
	struct ipif_s 	*v_ipif;	/* Pointer to logical interface	*/
	u_long		v_pkt_in;	/* # Pkts in on interface	*/
	u_long		v_pkt_out;	/* # Pkts out on interface	*/
	u_long		v_bytes_in;	/* # Bytes in on interface	*/
	u_long		v_bytes_out;	/* # Bytes out on interface	*/
	int		v_timeout_id;	/* Qtimeout return id	*/
	/*
	 * struct route	v_route;	Cached route if this is a tunnel
	 *				Used in bsd for performance
	 */
};

/*
 * The kernel's multicast forwarding cache entry structure
 * (A field for the type of service (mfc_tos) is to be added
 * at a future point)
 */
struct mfc {
    struct in_addr	mfc_origin;	/* ip origin of mcasts	*/
    struct in_addr  	mfc_mcastgrp;	/* multicast group associated */
    vifi_t		mfc_parent;	/* incoming vif	*/
    u_char		mfc_ttls[MAXVIFS];	/* forwarding ttls on vifs    */
    u_long		mfc_pkt_cnt;	/* pkt count for src-grp	*/
    u_long		mfc_byte_cnt;	/* byte count for src-grp	*/
    u_long		mfc_wrong_if;	/* wrong if for src-grp	*/
    struct timespec  	mfc_last_assert;	/* last time I sent an assert */
    struct rtdetq	*mfc_rte;	/* pending upcall	*/
    int			mfc_timeout_id;	/* qtimeout return id	*/
    struct mfc		*mfc_next;
};

/*
 * Argument structure used for pkt info. while upcall is made
 */
struct rtdetq {
    mblk_t		*mp;		/*  A copy of the packet	*/
    ill_t		*ill;		/*  Interface pkt came in on	*/
#ifdef UPCALL_TIMING
    timespec_t		t;		/*  Timestamp			*/
#endif /* UPCALL_TIMING */
    struct rtdetq	*rte_next;
};

/*
 * Struct used to communicate from kernel to multicast router
 * note the convenient similarity to an IP packet
 */
struct igmpmsg {
    u_long	    unused1;
    u_long	    unused2;
    u_char	    im_msgtype;			/* what type of message	    */
#define	IGMPMSG_NOCACHE		1
#define	IGMPMSG_WRONGVIF	2
    u_char	    im_mbz;			/* must be zero		    */
    u_char	    im_vif;			/* vif rec'd on		    */
    u_char	    unused3;
    struct in_addr  im_src, im_dst;
};

#define	MFCTBLSIZ	256
#if (MFCTBLSIZ & (MFCTBLSIZ - 1)) == 0	  /* from sys:route.h */
#define	MFCHASHMOD(h)	((h) & (MFCTBLSIZ - 1))
#else
#define	MFCHASHMOD(h)	((h) % MFCTBLSIZ)
#endif

#define	MAX_UPQ	4		/* max. no of pkts in upcall Q */

/*
 * Token Bucket filter code
 */
#define	MAX_BKT_SIZE	10000		/* 10K bytes size 		*/
#define	MAXQSIZE	10		/* max # of pkts in queue 	*/
#define	TOKEN_SIZE	8		/* number of bits in token	*/

/*
 * The token bucket filter at each vif
 */
struct tbf {
    timespec_t 		tbf_last_pkt_t; /* arr. time of last pkt 	*/
    u_long 		tbf_n_tok;	/* no of tokens in bucket 	*/
    u_long 		tbf_q_len;    	/* length of queue at this vif	*/
    u_long 		tbf_max_q_len;  /* max queue length		*/
    mblk_t		*tbf_q;		/* Packet queue	*/
    mblk_t		*tbf_t;		/* Tail-insertion pointer	*/
    kmutex_t 		tbf_lock;	/* lock on the tbf		*/
};

#if defined(_KERNEL) && defined(__STDC__)

extern	int	ip_mrouter_set(int cmd, queue_t *q, int doit,
	u_char *data, int datalen);
extern	int	ip_mrouter_get(int cmd, queue_t *q, u_char *data);
extern	int	ip_mrouter_done(void);
extern	int	ip_mforward(ill_t *ill, ipha_t *ipha, mblk_t *mp);
extern	void	reset_mrt_vif_ipif(ipif_t *ipif);
extern	void	reset_mrt_ill(ill_t *ill);
extern	int	mrt_ioctcl(int cmd, caddr_t data);
extern	int	ip_mroute_stats(struct opthdr * optp, mblk_t *mp);
extern	int	ip_mroute_vif(struct opthdr * optp, mblk_t *mp);
extern	int	ip_mroute_mrt(struct opthdr * optp, mblk_t *mp);
extern	void	ip_mroute_decap(queue_t * q, mblk_t * mp);

extern	queue_t	*ip_g_mrouter;

#endif	/* defined(_KERNEL) && defined(__STDC__) */

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _NETINET_IP_MROUTE_H */
