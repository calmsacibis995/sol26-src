/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

/*
 * Udp protocol header.
 * Per RFC 768, September, 1981.
 */

#ifndef	_NETINET_UDP_H
#define	_NETINET_UDP_H

#pragma ident	"@(#)udp.h	1.3	96/10/14 SMI"
/* udp.h 1.7 88/08/19 SMI; from UCB 7.1 6/5/86	*/

#ifdef	__cplusplus
extern "C" {
#endif

struct udphdr {
	u_short	uh_sport;		/* source port */
	u_short	uh_dport;		/* destination port */
	short	uh_ulen;		/* udp length */
	u_short	uh_sum;			/* udp checksum */
};

/*
 * Following option in UDP_ namespace required to be exposed through
 * <xti.h> (It also requires exposing options not implemented). The options
 * with potential for conflicts use #ifndef guards.
 *
 */
#ifndef UDP_CHECKSUM
#define	UDP_CHECKSUM	0x0600
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _NETINET_UDP_H */
