/*	Copyright (c) 1996 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/

#pragma ident	"@(#)ip_opt_data.c	1.2	96/10/13 SMI"

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


extern	int	ip_opt_default(queue_t *q, int level, int name, u_char * ptr);
extern	int	ip_opt_get(queue_t *q, int level, int name, u_char * ptr);
extern int	ip_opt_set(queue_t *q, u_int mgmt_flags, int level, int name,
	u_int inlen, u_char *invalp, u_int *outlenp, u_char *outvalp);

/*
 * Table of all known options handled on a IP protocol stack.
 *
 * Note: Not all of these options are available through all protocol stacks
 *	 For example, multicast options are not accessible in TCP over IP.
 *	 The filtering for that happens in option table at transport level.
 *	 Also, this table excludes any options processed exclusively at the
 *	 transport protocol level.
 */
opdes_arr_t	ip_opt_arr = {

{ SO_DONTROUTE,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_BROADCAST,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_REUSEADDR, SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },

{ IP_OPTIONS,	IPPROTO_IP, OA_RW, OA_RW, (OP_PASSNEXT|OP_VARLEN|OP_NODEFAULT),
	40, -1 /* not initialized */ },

{ IP_TOS,	IPPROTO_IP, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ IP_TTL,	IPPROTO_IP, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
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
{ IP_RECVOPTS,	IPPROTO_IP, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ IP_RECVDSTADDR, IPPROTO_IP, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },

{ MRT_INIT, IPPROTO_IP, 0, OA_X, (OP_PASSNEXT|OP_NODEFAULT), sizeof (int),
	-1 /* not initialized */ },

{ MRT_DONE, IPPROTO_IP, 0, OA_X, (OP_PASSNEXT|OP_NODEFAULT), 0,
	-1 /* not initialized */ },

{ MRT_ADD_VIF, IPPROTO_IP, 0, OA_X, (OP_PASSNEXT|OP_NODEFAULT),
	sizeof (struct vifctl), -1 /* not initialized */ },

{ MRT_DEL_VIF, 	IPPROTO_IP, 0, OA_X, (OP_PASSNEXT|OP_NODEFAULT),
	sizeof (vifi_t), -1 /* not initialized */ },

{ MRT_ADD_MFC, 	IPPROTO_IP, 0, OA_X, (OP_PASSNEXT|OP_NODEFAULT),
	sizeof (struct mfcctl), -1 /* not initialized */ },

{ MRT_DEL_MFC, 	IPPROTO_IP, 0, OA_X, (OP_PASSNEXT|OP_NODEFAULT),
	sizeof (struct mfcctl), -1 /* not initialized */ },

{ MRT_VERSION, 	IPPROTO_IP, OA_R, OA_R, (OP_PASSNEXT|OP_NODEFAULT),
	sizeof (int), -1 /* not initialized */ },

{ MRT_ASSERT, 	IPPROTO_IP, 0, OA_RW, (OP_PASSNEXT|OP_NODEFAULT),
	sizeof (int), -1 /* not initialized */ },
};


#define	IP_OPT_ARR_CNT		A_CNT(ip_opt_arr)


/*
 * Initialize option database object for IP
 *
 * This object represents database of options to search passed to
 * {sock,tpi}optcom_req() interface routine to take care of option
 * management and associated methods.
 */

optdb_obj_t ip_opt_obj = {
	ip_opt_default,		/* IP default value function pointer */
	ip_opt_get,		/* IP get function pointer */
	ip_opt_set,		/* IP set function pointer */
	false,			/* IP is NOT a tpi provider */
	IP_OPT_ARR_CNT,		/* IP option database count of entries */
	ip_opt_arr		/* IP option database */
};
