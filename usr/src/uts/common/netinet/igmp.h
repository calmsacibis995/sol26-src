/*
 * Copyright (c) 1991, by Sun Microsystems, Inc.
 */

#ifndef	_NETINET_IGMP_H
#define	_NETINET_IGMP_H

#pragma ident	"@(#)igmp.h	1.11	96/04/08 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Internet Group Management Protocol (IGMP) definitions.
 *
 * Written by Steve Deering, Stanford, May 1988.
 * Modified by Rosen Sharma, Stanford, Aug 1994
 * Modified by Bill Fenner, Xerox PARC, April 1995
 *
 * MULTICAST 3.5.1.1
 */

/*
 * IGMP packet format.
 */
struct igmp {
	u_char		igmp_type;	/* version & type of IGMP message  */
	u_char		igmp_code;	/* code for routing sub-msgs	   */
	u_short		igmp_cksum;	/* IP-style checksum		   */
	struct in_addr	igmp_group;	/* group address being reported	   */
};					/*  (zero for queries)		   */

#ifdef _KERNEL
typedef struct igmp_s {
	u_char		igmp_type;	/* version & type of IGMP message  */
	u_char		igmp_code;	/* code for routing sub-msgs	   */
	u_char		igmp_cksum[2];	/* IP-style checksum		   */
	u_char		igmp_group[4];	/* group address being reported	   */
} igmp_t;				/*  (zero for queries)		   */

/* Aligned igmp header */
typedef struct igmpa_s {
	u_char		igmpa_type;	/* version & type of IGMP message  */
	u_char		igmpa_code;	/* code for routing sub-msgs	   */
	u_short		igmpa_cksum;	/* IP-style checksum		   */
	u_long		igmpa_group;	/* group address being reported	   */
} igmpa_t;				/*  (zero for queries)		   */
#endif	/* _KERNEL */


#define	IGMP_MINLEN			8

/*
 * Message types, including version number.
 */

#define	IGMP_MEMBERSHIP_QUERY		0x11	/* membership query    */
#define	IGMP_V1_MEMBERSHIP_REPORT	0x12	/* Vers.1 membership report */
#define	IGMP_V2_MEMBERSHIP_REPORT	0x16	/* Vers.2 membership report */
#define	IGMP_V2_LEAVE_GROUP		0x17	/* Leave-group message	    */
#define	IGMP_DVMRP			0x13	/* DVMRP routing message    */
#define	IGMP_PIM			0x14	/* PIM routing message	    */

#define	IGMP_MTRACE_RESP		0x1e  	/* traceroute resp to sender */
#define	IGMP_MTRACE			0x1f	/* mcast traceroute messages */

#define	IGMP_MAX_HOST_REPORT_DELAY	10	/* max delay for response to */
						/* query (in seconds)	*/
						/* according to RFC1112 */

#define	IGMP_TIMER_SCALE		10 	/* denotes that igmp->timer  */
						/* field specifies time in   */
						/* 10ths of seconds	*/

/*
* The following four defininitions are for backwards compatibility.
* They should be removed as soon as all applications are updated to
* use the new constant names.
*/
#define	IGMP_HOST_MEMBERSHIP_QUERY	IGMP_MEMBERSHIP_QUERY
#define	IGMP_HOST_MEMBERSHIP_REPORT	IGMP_V1_MEMBERSHIP_REPORT
#define	IGMP_HOST_NEW_MEMBERSHIP_REPORT	IGMP_V2_MEMBERSHIP_REPORT
#define	IGMP_HOST_LEAVE_MESSAGE		IGMP_V2_LEAVE_GROUP

#define	IGMP_SLOWTIMO_INTERVAL		500	/* milliseconds */

#if defined(_KERNEL) && defined(__STDC__)

extern	int	igmp_timeout_handler(void);
extern	void	igmp_timeout_start(int);
extern	int	igmp_input(queue_t *q, mblk_t *mp, ipif_t *ipif);
extern	void	igmp_joingroup(ilm_t *ilm);
extern	void	igmp_leavegroup(ilm_t *ilm);
extern  void    igmp_slowtimo(queue_t *q);

#endif	/* defined(_KERNEL) && defined(__STDC__) */

#ifdef	__cplusplus
}
#endif

#endif	/* _NETINET_IGMP_H */
