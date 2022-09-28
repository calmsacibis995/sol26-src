/*
 * Copyright (c) 1992-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_INET_IP_RTS_H
#define	_INET_IP_RTS_H

#pragma ident	"@(#)ip_rts.h	1.2	96/09/26 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _IP_RTS_C
#ifdef __STDC__
extern	int	ip_rts_request(queue_t * q, mblk_t * mp);

extern	void	ip_rts_change(int type, ipaddr_t dst_addr, ipaddr_t gw_addr,
    ipaddr_t net_mask, ipaddr_t source, int flags, int error, int rtm_addrs);

extern	void	ip_rts_ifmsg(ipif_t *ipif);

extern	void	ip_rts_newaddrmsg(int cmd, int error, ipif_t * ipif);

extern  void    ip_rts_rtmsg(int type, ire_t * ire, int error);

#else /* __STDC__ */

extern	int	ip_rts_request();

extern	void	ip_rts_change();

extern	void	ip_rts_ifmsg();

extern	void	ip_rts_newaddrmsg();

extern  void    ip_rts_rtmsg();
#endif /* __STDC__ */
#endif /* _IP_RTS_C */

#ifdef	__cplusplus
}
#endif

#endif	/* _INET_IP_RTS_H */
