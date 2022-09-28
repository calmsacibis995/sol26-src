/*
 * Copyright (c) 1993 by Sun Microsystems, Inc. All rights reserved.
 */

#ifndef	_INTERFACES_H
#define	_INTERFACES_H

#pragma ident	"@(#)interfaces.h	1.8	95/12/04 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct interfaces {
	char			nm[IFNAMSIZ];	/* Interface name */
	short			mtu;		/* MTU of interface */
	int			senddesc;	/* network send descriptor */
	int			recvdesc;	/* network receive descriptor */
	int			type;		/* descriptor flags */
	short			flags;		/* interface flags */
	struct in_addr		bcast;		/* interface broadcast */
	struct in_addr		mask;		/* interface netmask */
	struct in_addr		addr;		/* interface IP addr */
	PKT_LIST		*pkthead;	/* head of packet list */
	PKT_LIST		*pkttail;	/* tail of packet list */
	ENCODE			*ecp;		/* IF specific options */
	OFFLST			*of_head;	/* IF specific OFFERs */
	u_int			received;	/* # of received pkts */
	u_int			dropped;	/* # of dropped pkts */
	u_int			processed;	/* # of processed pkts */
	struct interfaces	*next;
} IF;

#define	DHCP_SOCKET		0	/* Plain AF_INET socket */
#define	DHCP_DLPI		1	/* DLPI stream */
#define	MAXIFS			256	/* Max number of interfaces */

extern IF	*if_head;	/* head of monitored interfaces */
extern char	*interfaces;	/* list of user-requested interfaces. */
extern int	find_interfaces(void);
extern int	check_interfaces(void);
extern int	open_interfaces(void);
extern int	read_interfaces(int);
extern int	write_interface(IF *, PKT *, int, struct sockaddr_in *);
extern int	close_interfaces(void);
extern int	set_arp(IF *, struct in_addr *, u_char *, int, u_char);
extern int	dhcp(IF *, PKT_LIST *);
extern int	relay_agent(IF *, PKT_LIST *);
extern int	bootp_compatibility(IF *, int, PKT_LIST *);
extern int	determine_network(IF *, PKT_LIST *, struct in_addr *,
		    struct in_addr *);
extern int	get_netmask(struct in_addr *, struct in_addr **);
extern int	send_reply(IF *, PKT *, int, struct in_addr *);
extern int	check_offers(IF *, struct in_addr *);
extern void	free_offers(IF *);
extern void	disp_if_stats(IF *);
extern int	select_offer(PER_NET_DB *, PKT_LIST *, IF *, PN_REC *);

#ifdef	__cplusplus
}
#endif

#endif	/* _INTERFACES_H */
