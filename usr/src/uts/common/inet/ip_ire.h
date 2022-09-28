/*
 * Copyright (c) 1992-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_INET_IP_IRE_H
#define	_INET_IP_IRE_H

#pragma ident	"@(#)ip_ire.h	1.22	96/09/26 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * match parameter definitions for
 * IRE lookup routines.
 */

#define	MATCH_IRE_DSTONLY	0x0	/* Match just the address */
#define	MATCH_IRE_TYPE		0x0001	/* Match IRE type */
#define	MATCH_IRE_SRC		0x0002	/* Match IRE source address */
#define	MATCH_IRE_MASK		0x0004	/* Match IRE mask */
#define	MATCH_IRE_RQ		0x0008	/* Match IRE Receive Q */
#define	MATCH_IRE_WQ		0x0010	/* Match IRE Write Q */
#define	MATCH_IRE_GW		0x0020	/* Match IRE gateway */
#define	MATCH_IRE_IPIF		0x0040	/* Match IRE ipif */
#define	MATCH_IRE_RECURSIVE	0x0080	/* Do recursive lookup if necessary */
#define	MATCH_IRE_DEFAULT	0x0100	/* Return default route if no route */
					/* found. */
#define	MATCH_IRE_RJ_BHOLE	0x0200	/* During lookup if we hit an ire */
					/* with RTF_REJECT or RTF_BLACKHOLE, */
					/* return the ire. No recursive */
					/* lookup should be done. */

#ifndef _IP_IRE_C
#ifdef __STDC__
extern	int	ip_ire_advise(queue_t * q, mblk_t * mp);

extern	int	ip_ire_delete(queue_t * q, mblk_t * mp);

extern	int	ip_ire_report(queue_t * q, mblk_t * mp, caddr_t arg);

extern	void	ip_ire_req(queue_t * q, mblk_t * mp);

extern	ire_t	*ire_add(ire_t * ire);

extern	void	ire_add_then_put(queue_t * q, mblk_t * mp);

extern	ire_t	*ire_create(u_char * addr, u_char * mask, u_char * src_addr,
				u_char * gateway, u_int max_frag,
				mblk_t * ll_hdr_mp, queue_t * rfq,
				queue_t * stq, u_int type, u_long rtt,
				u_int ll_hdr_len, ipif_t *ipif, ire_t * sire,
				u_long flags);

extern	ire_t	**ire_create_bcast(ipif_t * ipif, ipaddr_t addr, ire_t ** irep);

extern	void	ire_delete(ire_t * ire);

extern	void	ire_delete_route_gw(ire_t * ire, char * cp);

extern	void	ire_expire(ire_t * ire, char * arg);

extern	ire_t	*ire_route_lookup(ipaddr_t addr, ipaddr_t mask,
				ipaddr_t gateway, int type, ipif_t * ipif,
				ire_t ** gw, queue_t * wrq, int flags);

extern	ire_t	*ire_ctable_lookup(ipaddr_t addr, ipaddr_t gateway,
				int type, ipif_t * ipif, queue_t * wrq,
				int flags);

extern	ire_t	*ire_ftable_lookup(ipaddr_t addr, ipaddr_t mask,
				ipaddr_t gateway, int type,
				ipif_t * ipif, ire_t ** pire, queue_t * wrq,
				int flag);

extern	ire_t	*ire_cache_lookup(ipaddr_t addr);

extern	ire_t 	*ire_lookup_local(void);

extern  ire_t	*ire_lookup_loop_multi(ipaddr_t group);

extern	void	ire_flush_cache(ire_t * ire, int flag);

extern	ire_t	*ipif_to_ire(ipif_t * ipif);

extern	void	ire_pkt_count(ire_t * ire, char * ippc_arg);

extern	ill_t	*ire_to_ill(ire_t * ire);

extern	void	ire_walk(pfv_t func, char * arg);

extern	void	ire_walk_wq(queue_t *wq, pfv_t func, char * arg);

#else /* __STDC__ */

extern	int	ip_ire_advise();

extern	int	ip_ire_delete();

extern	int	ip_ire_report();

extern	void	ip_ire_req();

extern	ire_t	*ire_add();

extern	void	ire_add_then_put();

extern	ire_t *	ire_create();

extern	ire_t **ire_create_bcast();

extern	void	ire_delete();

extern	void	ire_delete_route_gw();

extern	void	ire_expire();

extern	ire_t	*ire_route_lookup();

extern	ire_t	*ire_ctable_lookup();

extern	ire_t	*ire_ftable_lookup();

extern	ire_t	*ire_cache_lookup();

extern	ire_t 	*ire_lookup_local();

extern  ire_t	*ire_lookup_loop_multi();

extern	void	ire_flush_cache();

extern	ire_t	*ipif_to_ire(ipif_t * ipif);

extern	void	ire_pkt_count();

extern	ill_t *	ire_to_ill();

extern	void	ire_walk();

extern	void	ire_walk_wq();
#endif /* __STDC__ */
#endif /* _IP_IRE_C */

#ifdef	__cplusplus
}
#endif

#endif	/* _INET_IP_IRE_H */
