/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights Reserved.
 */

#ifndef	_DHCPD_H
#define	_DHCPD_H

#pragma ident	"@(#)dhcpd.h	1.61	96/06/15 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * dhcpd.h -- common header file for all the modules of the in.dhcpd program.
 */

#ifndef	TRUE
#define	TRUE	1
#endif	/* TRUE */

#ifndef	FALSE
#define	FALSE	0
#endif	/* FALSE */

/*
 * Raw encoded packet data. The final state. Note that 'code' not only
 * describes options: predefinied: 1-60, site: 128-254, vendor: 42(*),
 * but it also defines packet fields for packet data as well.
 */
typedef	struct encoded {
	u_short	code;	/* Option code: 1--254, pkt loc */
	u_char	len;	/* len of data */
	u_char	*data;	/* Encoded DHCP packet field / option */
	struct encoded	*prev;	/* previous in list */
	struct encoded	*next;	/* next in list */
} ENCODE;

#define	DHCP_CLASS_SIZE		128
#define	DHCP_MAX_CLASSES	10
#define	DHCP_MAX_CLASS_SIZE	(DHCP_CLASS_SIZE * DHCP_MAX_CLASSES)
typedef struct {
	char	class[DHCP_CLASS_SIZE + 1];	/* client class */
	ENCODE	*head;				/* options of this class */
} VNDLIST;

#define	DHCP_MACRO_SIZE	64			/* Max Len of a macro name */
typedef struct {
	char	nm[DHCP_MACRO_SIZE + 1];	/* Macro name */
	ENCODE	*head;				/* head of encoded opts */
	int	classes;			/* num of client classes */
	VNDLIST	**list;				/* table of client classes */
} MACRO;

#define	DHCPD			"in.dhcpd"	/* daemon's name */
#define	DAEMON_VERS		"3.0"		/* daemon's version number */
#define	BCAST_MASK		0x8000		/* BROADCAST flag */
#define	ENC_COPY		0		/* Copy encode list */
#define	ENC_DONT_COPY		1		/* don't copy encode list */
#define	DHCP_MAX_REPLY_SIZE	8192		/* should be big enough */
#define	DHCP_ICMP_ATTEMPTS	2		/* Number of ping attempts */
#define	DHCP_ICMP_TIMEOUT	1		/* Wait # seconds for resp */
#define	DHCP_ARP_ADD		0		/* Add an ARP table entry */
#define	DHCP_ARP_DEL		1		/* Del an ARP table entry */
#define	DHCP_OFF_SECS		10		/* def ttl of an offer */
#define	DHCP_DEF_HOPS		4		/* def relay agent hops */
#define	DHCP_POLL_TIME		60000		/* interface poll (msec) */
#define	DHCP_SCRATCH		128		/* scratch buffer size */

/* load option flags */
#define	DHCP_DHCP_CLNT		1		/* It's a DHCP client */
#define	DHCP_SEND_LEASE		2		/* Send lease parameters */
#define	DHCP_NON_RFC1048	4		/* non-rfc1048 magic cookie */
#define	DHCP_MASK		7		/* legal values */

/*
 * Number of seconds 'secs' field in packet must be before a DHCP server
 * responds to a client who is requesting verification of it's IP address
 * *AND* renegotiating its lease on an address that is owned by another
 * server. This is done to give the *OWNER* server time to respond to
 * the client first.
 */
#define	DHCP_RENOG_WAIT		8

extern int	debug;
extern int	verbose;
extern int	noping;
extern int	ethers_compat;
extern int	no_dhcptab;
extern int	server_mode;
extern int	bootp_compat;
extern int	be_automatic;
extern int	max_hops;
extern time_t	off_secs;
extern time_t	rescan_interval;
extern time_t	abs_rescan;
extern int	reinitialize;
extern u_long	npkts;		/* total packets waiting to be processed */
extern u_long	totpkts;	/* total packets received */
extern struct in_addr server_ip;

extern int	idle(void);
extern int	process_pkts(void);
extern PKT	*gen_bootp_pkt(int, PKT *);
extern int	inittab(void);
extern int	checktab(void);
extern int	readtab(void);
extern void	resettab(void);
extern int	relay_agent_init(char *);
extern void	dhcpmsg();
extern char 	*smalloc(unsigned);
extern ENCODE 	*combine_encodes(ENCODE *, ENCODE *, int);
extern MACRO	*get_macro(char *);
extern ENCODE	*find_encode(ENCODE *, u_short);
extern ENCODE	*dup_encode(ENCODE *);
extern ENCODE	*make_encode(u_short, u_char, void *, int);
extern ENCODE	*copy_encode_list(ENCODE *);
extern void	free_encode_list(ENCODE *);
extern void	free_encode(ENCODE *);
extern void	replace_encode(ENCODE **, ENCODE *, int);
extern ENCODE	*vendor_encodes(MACRO *, char *);
extern char 	*disp_cid(PKT_LIST *);
extern int	icmp_echo(struct in_addr, PKT_LIST *);
extern u_short	ip_chksum(char *, u_short);
extern void	get_client_id(PKT_LIST *, u_char *, u_int *);
extern char	*disp_cid(PKT_LIST *);
extern u_char	*get_octets(u_char **, u_char *);
extern int	get_number(char **, void *, int);
extern int	load_options(int, PKT *, int, u_char *, ENCODE *, ENCODE *);
extern int	_dhcp_options_scan(PKT_LIST *);
extern void	free_plp(PKT_LIST *);
extern int	stat_boot_server(void);

#ifdef	__cplusplus
}
#endif

#endif	/* _DHCPD_H */
