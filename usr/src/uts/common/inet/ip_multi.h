/*
 * Copyright (c) 1992-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_INET_IP_MULTI_H
#define	_INET_IP_MULTI_H

#pragma ident	"@(#)ip_multi.h	1.13	96/10/13 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _IP_MULTI_C
#ifdef __STDC__
extern	int	ip_addmulti(ipaddr_t  group, ipif_t * ipif);

extern	int	ip_delmulti(ipaddr_t group, ipif_t *ipif);

extern	void	ip_multicast_loopback(queue_t * rq, mblk_t * mp_orig);

extern	void	ip_wput_ctl(queue_t * q, mblk_t * mp_orig);

extern	void	ill_add_multicast(ill_t * ill);

extern	void	ill_delete_multicast(ill_t * ill);

extern	ilm_t	*ilm_lookup(ill_t *ill, ipaddr_t group);

extern	ilm_t	*ilm_lookup_exact(ipif_t *ipif, ipaddr_t group);

extern	void	ilm_free(ipif_t *ipif);

extern	int	ip_opt_add_group(ipc_t *ipc, int checkonly, ipaddr_t group,
					ipaddr_t ifaddr);

extern	int	ip_opt_delete_group(ipc_t *ipc, int checkonly, ipaddr_t group,
					ipaddr_t ifaddr);

extern	boolean_t	ilg_member(ipc_t *ipc, ipaddr_t group);

extern	void	ilg_delete_all(ipc_t *ipc);

extern	void	reset_ilg_lower(ipif_t *ipif);

extern	void	reset_ipc_multicast_ipif(ipif_t *ipif);

#else	/* __STDC__ */

extern	int	ip_addmulti();

extern	int	ip_delmulti();

extern	void	ip_multicast_loopback();

extern	void	ip_wput_ctl();

extern	void	ill_add_multicast();

extern	void	ill_delete_multicast();

extern	ilm_t	*ilm_lookup();

extern	ilm_t 	*ilm_lookup_exact();

extern	void	ilm_free();

extern	int 	ip_opt_add_group();

extern	int	ip_opt_delete_group();

extern	boolean_t	ilg_member();

extern	void	ilg_delete_all();

extern	void	reset_ilg_lower();

extern	void	reset_ipc_multicast_ipif();
#endif	/* __STDC__ */
#endif /* _IP_MULTI_C */

#ifdef	__cplusplus
}
#endif

#endif	/* _INET_IP_MULTI_H */
