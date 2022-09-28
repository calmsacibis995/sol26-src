/*	Copyright (c) 1996 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/

#pragma ident	"@(#)udp_opt_data.c	1.2	96/10/13 SMI"

#include <sys/types.h>
#include <sys/stream.h>
#define	_SUN_TPI_VERSION 1
#include <sys/tihdr.h>
#include <sys/socket.h>

#include <inet/common.h>
#include <inet/ip.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/ip_mroute.h>
#include "optcom.h"


extern int udp_opt_default(queue_t *q, int level, int name, u_char * ptr);
extern int udp_opt_get(queue_t *q, int level, int name, u_char * ptr);
extern int udp_opt_set(queue_t *q, u_int mgmt_flags, int level, int name,
	u_int inlen, u_char *invalp, u_int *outlenp, u_char *outvalp);

/*
 * Table of all known options handled on a UDP protocol stack.
 *
 * Note: This table contains options processed by both UDP and IP levels
 *       and is the superset of options that can be performed on a UDP over IP
 *       stack.
 */
opdes_t	udp_opt_arr[] = {

{ SO_DEBUG,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_DONTROUTE,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_USELOOPBACK, SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_BROADCAST,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_REUSEADDR, SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_TYPE,	SOL_SOCKET, OA_R, OA_R, OP_PASSNEXT, sizeof (int), 0 },
{ SO_SNDBUF,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_RCVBUF,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_DGRAM_ERRIND, SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ IP_OPTIONS,	IPPROTO_IP, OA_RW, OA_RW, (OP_PASSNEXT|OP_VARLEN|OP_NODEFAULT),
	40, -1 /* not initiailized */ },

{ IP_TOS,	IPPROTO_IP, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ IP_TTL,	IPPROTO_IP, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ IP_RECVOPTS,	IPPROTO_IP, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ IP_RECVDSTADDR, IPPROTO_IP, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ IP_MULTICAST_IF, IPPROTO_IP, OA_RW, OA_RW, OP_PASSNEXT,
	sizeof (struct in_addr),	0 /* INADDR_ANY */ },

{ IP_MULTICAST_LOOP, IPPROTO_IP, OA_RW, OA_RW, (OP_PASSNEXT|OP_DEF_FN),
	sizeof (u_char), -1 /* not initialized */},

{ IP_MULTICAST_TTL, IPPROTO_IP, OA_RW, OA_RW, (OP_PASSNEXT|OP_DEF_FN),
	sizeof (u_char), -1 /* not initialized */ },

{ IP_ADD_MEMBERSHIP, IPPROTO_IP, OA_X, OA_X, (OP_PASSNEXT|OP_NODEFAULT),
	sizeof (struct ip_mreq), -1 /* not initialized */ },

{ IP_DROP_MEMBERSHIP, 	IPPROTO_IP, OA_X, OA_X, (OP_PASSNEXT|OP_NODEFAULT),
	sizeof (struct ip_mreq), -1 /* not initialized */ },
};


#define	UDP_OPT_ARR_CNT		A_CNT(udp_opt_arr)

u_int udp_max_optbuf_len; /* initialized when UDP driver is loaded */

/*
 * Initialize option database object for UDP
 *
 * This object represents database of options to search passed to
 * {sock,tpi}optcom_req() interface routine to take care of option
 * management and associated methods.
 */

optdb_obj_t udp_opt_obj = {
	udp_opt_default,	/* UDP default value function pointer */
	udp_opt_get,		/* UDP get function pointer */
	udp_opt_set,		/* UDP set function pointer */
	true,			/* UDP is tpi provider */
	UDP_OPT_ARR_CNT,	/* UDP option database count of entries */
	udp_opt_arr		/* UDP option database */
};
