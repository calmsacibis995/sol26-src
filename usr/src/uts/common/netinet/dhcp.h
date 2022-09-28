/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * dhcp.h - Generic DHCP definitions exported to windows and dos programs.
 */

#ifndef	_DHCP_H
#define	_DHCP_H

#pragma ident	"@(#)dhcp.h	1.13	96/06/24 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Generic DHCP option structure.
 */
typedef struct {
	u_char    code;
	u_char    len;
	u_char    value[1];
} DHCP_OPT;

/*
 * DHCP option codes.
 */

#define	CD_PAD			0
#define	CD_END			255
#define	CD_SUBNETMASK		1
#define	CD_TIMEOFFSET		2
#define	CD_ROUTER		3
#define	CD_TIMESERV		4
#define	CD_IEN116_NAME_SERV	5
#define	CD_DNSSERV		6
#define	CD_LOG_SERV		7
#define	CD_COOKIE_SERV		8
#define	CD_LPR_SERV		9
#define	CD_IMPRESS_SERV		10
#define	CD_RESOURCE_SERV	11
#define	CD_HOSTNAME		12
#define	CD_BOOT_SIZE		13
#define	CD_DUMP_FILE		14
#define	CD_DNSDOMAIN		15
#define	CD_SWAP_SERV		16
#define	CD_ROOT_PATH		17
#define	CD_EXTEND_PATH		18

/* IP layer parameters */
#define	CD_IP_FORWARDING_ON	19
#define	CD_NON_LCL_ROUTE_ON	20
#define	CD_POLICY_FILTER	21
#define	CD_MAXIPSIZE		22
#define	CD_IPTTL		23
#define	CD_PATH_MTU_TIMEOUT	24
#define	CD_PATH_MTU_TABLE_SZ	25

/* IP layer parameters per interface */
#define	CD_MTU			26
#define	CD_ALL_SUBNETS_LCL_ON	27
#define	CD_BROADCASTADDR	28
#define	CD_MASK_DISCVRY_ON	29
#define	CD_MASK_SUPPLIER_ON	30
#define	CD_ROUTER_DISCVRY_ON	31
#define	CD_ROUTER_SOLICIT_SERV	32
#define	CD_STATIC_ROUTE		33

/* Link Layer Parameters per Interface */
#define	CD_TRAILER_ENCAPS_ON	34
#define	CD_ARP_TIMEOUT		35
#define	CD_ETHERNET_ENCAPS_ON	36

/* TCP Parameters */
#define	CD_TCP_TTL		37
#define	CD_TCP_KALIVE_INTVL	38
#define	CD_TCP_KALIVE_GRBG_ON	39

/* Application layer parameters */
#define	CD_NIS_DOMAIN		40
#define	CD_NIS_SERV		41
#define	CD_NTP_SERV		42
#define	CD_VENDOR_SPEC		43

/* NetBIOS parameters */
#define	CD_NETBIOS_NAME_SERV	44
#define	CD_NETBIOS_DIST_SERV	45
#define	CD_NETBIOS_NODE_TYPE	46
#define	CD_NETBIOS_SCOPE	47

/* X Window parameters */
#define	CD_XWIN_FONT_SERV	48
#define	CD_XWIN_DISP_SERV	49

/* DHCP protocol extension options */
#define	CD_REQUESTED_IP_ADDR	50
#define	CD_LEASE_TIME		51
#define	CD_OPTION_OVERLOAD	52
#define	CD_DHCP_TYPE		53
#define	CD_SERVER_ID		54
#define	CD_REQUEST_LIST		55
#define	CD_MESSAGE		56
#define	CD_MAX_DHCP_SIZE	57
#define	CD_T1_TIME		58
#define	CD_T2_TIME		59
#define	CD_CLASS_ID		60
#define	CD_CLIENT_ID		61

/* Netware options */
#define	CD_NW_IP_DOMAIN		62
#define	CD_NW_IP_OPTIONS	63

/* Nisplus options */
#define	CD_NISPLUS_DMAIN	64
#define	CD_NISPLUS_SERVS	65

/* Optional sname/bootfile options */
#define	CD_TFTP_SERV_NAME	66
#define	CD_OPT_BOOTFILE_NAME	67

/* Additional server options */
#define	CD_MOBILE_IP_AGENT	68
#define	CD_SMTP_SERVS		69
#define	CD_POP3_SERVS		70
#define	CD_NNTP_SERVS		71
#define	CD_WWW_SERVS		72
#define	CD_FINGER_SERVS		73
#define	CD_IRC_SERVS		74

/* Streettalk options */
#define	CD_STREETTALK_SERVS	75
#define	CD_STREETTALK_DA_SERVS	76

/* User class identifier */
#define	CD_USER_CLASS_ID	77

#define	DHCP_SITE_OPT		128	/* inclusive */
#define	DHCP_END_SITE		254

#define	DHCP_LAST_OPT		CD_USER_CLASS_ID	/* last op code */

/* Packet fields */
#define	CD_PACKET_START		256
#define	CD_YIADDR		256	/* client's ip address */
#define	CD_SIADDR		257	/* Bootserver's ip address */
#define	CD_SNAME		258	/* Hostname of Bootserver, or opts */
#define	CD_GIADDR		259	/* BOOTP relay agent address */
#define	CD_BOOTFILE		260	/* File to boot or opts */
#define	CD_PACKET_END		260

/* SolarNet PC-Admin Vendor Specific Options */
#define	VS_OPTION_START		511
#define	VS_FW_SERV		512	/* List of framework server ip's */
#define	VS_PCNFS_SERV		513	/* List of pcnfsd ip's */
#define	VS_LICENSE_SERV		514	/* List of license server ip's */
#define	VS_NFS_RSIZE		515	/* NFS read size. */
#define	VS_NFS_WSIZE		516	/* NFS write size */
#define	VS_NFS_TIMEOUT		517	/* NFS op timeout value */
#define	VS_NFS_RETRIES		518	/* NFS op retries */
#define	VS_LOGIN_SNC		519	/* SNC login script. */
#define	VS_LOGOUT_SNC		520	/* SNC logout script. */
#define	VS_SNC_SERV		521	/* SNC script server */
#define	VS_SNC_PATH		522	/* Path to SNC scripts */
#define	VS_SNC_BOOT		523	/* SNC boot script */
#define	VS_TIMEZONE		524	/* rfc 822 ASCII timezone */
#define	VS_SNC_SHUTDOWN		525	/* SNC shutdown script */
#define	VS_OPTION_END		525
#define	VENDOR(v)		((v) - VS_OPTION_START)

/* Boolean flags */
#define	CD_BOOL_START		1024
#define	CD_BOOL_HOSTNAME	1024	/* Entry wants hostname (Nameserv) */
#define	CD_BOOL_LEASENEG	1025	/* Entry's lease is negotiable */
#define	CD_BOOL_END		1025

/*
 * DHCP Packet. What will fit in a ethernet frame. We may use a smaller
 * size, based on what our transport can handle.
 */
#define	PKT_BUFFER	1486		/* max possible size of pkt buffer */
#define	BASE_PKT_SIZE	240		/* everything but the options */
typedef struct dhcp {
	u_char		op;		/* message opcode */
	u_char		htype;		/* Hardware address type */
	u_char		hlen;		/* Hardware address length */
	u_char		hops;		/* Used by relay agents */
	u_long		xid;		/* transaction id */
	u_short		secs;		/* Secs elapsed since client boot */
	u_short		flags;		/* DHCP Flags field */
	struct in_addr	ciaddr;		/* client IP addr */
	struct in_addr	yiaddr;		/* 'Your' IP addr. (from server) */
	struct in_addr	siaddr;		/* Boot server IP addr */
	struct in_addr	giaddr;		/* Relay agent IP addr */
	u_char		chaddr[16];	/* Client hardware addr */
	u_char		sname[64];	/* Optl. boot server hostname */
	u_char		file[128];	/* boot file name (ascii path) */
	u_char		cookie[4];	/* Magic cookie */
	u_char		options[60];	/* Options */
} PKT;

/*
 * Generic DHCP packet list
 */
#define	MAX_PKT_LIST	5	/* maximum list size */
typedef struct  dhcp_list {
	PKT			*pkt;
	u_int			len;
	u_char			points;
	DHCP_OPT		*opts[DHCP_LAST_OPT + 1];
	DHCP_OPT		*vs[VS_OPTION_END - VS_OPTION_START + 1];
	struct dhcp_list 	*prev;
	struct dhcp_list 	*next;
} PKT_LIST;
#define	PKT_LIST_NULL		((PKT_LIST *)0)

/*
 * DHCP packet types. As per protocol.
 */
#define	DISCOVER	((u_char)1)
#define	OFFER		((u_char)2)
#define	REQUEST		((u_char)3)
#define	DECLINE		((u_char)4)
#define	ACK		((u_char)5)
#define	NAK		((u_char)6)
#define	RELEASE		((u_char)7)
#define	INFORM		((u_char)8)

/*
 * Generic DHCP protocol defines
 */
#define	DHCP_PERM	((u_long)0xffffffff)	/* "permanent" lease time */
#define	BOOTREQUEST		(1)		/* BOOTP REQUEST opcode */
#define	BOOTREPLY		(2)		/* BOOTP REPLY opcode */
#define	BOOTMAGIC	{ 99, 130, 83, 99 }	/* rfc1048 magic cookie */

/* Error codes that could be generated while parsing packets */
#define	DHCP_ERR_OFFSET		512
#define	DHCP_GARBLED_MSG_TYPE	(DHCP_ERR_OFFSET+0)
#define	DHCP_WRONG_MSG_TYPE	(DHCP_ERR_OFFSET+1)
#define	DHCP_BAD_OPT_OVLD	(DHCP_ERR_OFFSET+2)

#ifdef	__cplusplus
}
#endif

#endif	/* _DHCP_H */
