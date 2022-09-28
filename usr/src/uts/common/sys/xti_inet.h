/*	Copyright (c) 1996 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/


#ifndef _SYS_XTI_INET_H
#define	_SYS_XTI_INET_H

#pragma ident	"@(#)xti_inet.h	1.2	96/10/14 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


/*
 * INTERNET SPECIFIC ENVIRONMENT
 *
 * Note:
 * Unfortunately, XTI specification test assertions require exposing in
 * headers options that are not implemented. They also require exposing
 * Internet and OSI related options as part of inclusion of <xti.h>
 *
 * Also XTI specification intrudes on <netinet/in.h> TCP_ and IP_ namespaces
 * and sometimes redefines the semantics or types of some options with a
 * different history in that namespace. The name and binary value are exposed
 * but option semantics may be different from what is in XTI spec and we defer
 * to the <netinet/in.h> precedent.
 */

/*
 * TCP level
 */

#define	INET_TCP	0x6 /* must be same as IPPROTO_TCP in <netinet/in.h> */

/*
 * TCP level options
 */

#ifndef TCP_NODELAY
#define	TCP_NODELAY	0x1	/* must be same as <netinet/tcp.h> */
#endif

#ifndef TCP_MAXSEG
#define	TCP_MAXSEG	0x2	/* must be same as <netinet/tcp.h> */
#endif

#ifndef TCP_KEEPALIVE
#define	TCP_KEEPALIVE	0x8	/* must be same as <netinet/tcp.h> */
#endif


/*
 * Structure used with TCP_KEEPALIVE option.
 */

struct t_kpalive {
	long	kp_onoff;	/* option on/off */
	long	kp_timeout;	/* timeout in minutes */
};
#define	T_GARBAGE		0x02 /* send garbage byte */


/*
 * UDP level
 */

#define	INET_UDP	0x11 /* must be same as IPPROTO_UDP in <netinet/in.h> */


/*
 * UDP level Options
 */

#ifndef UDP_CHECKSUM
#define	UDP_CHECKSUM	0x0600	/* must be same as in <netinet/udp.h> */
#endif

/*
 * IP level
 */

#define	INET_IP	0x0	/* must be same as IPPROTO_IP in <netinet/in.h> */

/*
 * IP level Options
 */

#ifndef IP_OPTIONS
#define	IP_OPTIONS	0x1	/* must be same as <netinet/in.h> */
#endif

#ifndef IP_TOS
#define	IP_TOS		0x3	/* must be same as <netinet/in.h> */
#endif

#ifndef IP_TTL
#define	IP_TTL		0x4	/* must be same as <netinet/in.h> */
#endif

/*
 * following also added to <netinet/in.h> and be in sync to keep namespace
 * sane
 */

#ifndef IP_REUSEADDR
#define	IP_REUSEADDR	0x104	/* allow local address reuse */
#endif

#ifndef IP_DONTROUTE
#define	IP_DONTROUTE	0x105	/* just use interface addresses */
#endif

#ifndef IP_BROADCAST
#define	IP_BROADCAST	0x106	/* permit sending of broadcast msgs */
#endif

/*
 * IP_TOS precedence level
 */

#define	T_ROUTINE			0
#define	T_PRIORITY			1
#define	T_IMMEDIATE			2
#define	T_FLASH				3
#define	T_OVERRIDEFLASH			4
#define	T_CRITIC_ECP			5
#define	T_INETCONTROL			6
#define	T_NETCONTROL			7


/*
 * IP_TOS type of service
 */

#define	T_NOTOS		0
#define	T_LDELAY	(1<<4)
#define	T_HITHRPT	(1<<3)
#define	T_HIREL		(1<<2)
#define	T_LOCOST	(1<<1)

#define	SET_TOS(prec, tos)	((0x7 & (prec)) << 5 | (0x1e & (tos)))

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_XTI_INET_H */
