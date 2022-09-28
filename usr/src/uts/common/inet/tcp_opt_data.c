/*	Copyright (c) 1996 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/

#pragma ident	"@(#)tcp_opt_data.c	1.2	96/10/13 SMI"

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


extern	int	tcp_opt_default(queue_t *q, int level, int name, u_char * ptr);
extern int	tcp_opt_get(queue_t *q, int level, int name, u_char * ptr);
extern int	tcp_opt_set(queue_t *q, u_int mgmt_flags, int level, int name,
	u_int inlen, u_char *invalp, u_int *outlenp, u_char *outvalp);

/*
 * Table of all known options handled on a TCP protocol stack.
 *
 * Note: This table contains options processed by both TCP and IP levels
 *       and is the superset of options that can be performed on a TCP over IP
 *       stack.
 */
opdes_t	tcp_opt_arr[] = {

{ SO_LINGER,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (struct linger),
	0 },

{ SO_DEBUG,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_KEEPALIVE,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_DONTROUTE,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_USELOOPBACK, SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_BROADCAST,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_REUSEADDR, SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_OOBINLINE, SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_TYPE,	SOL_SOCKET, OA_R, OA_R, OP_PASSNEXT, sizeof (int), 0 },
{ SO_SNDBUF,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_RCVBUF,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_DGRAM_ERRIND, SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },

{ TCP_NODELAY,	IPPROTO_TCP, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ TCP_MAXSEG,	IPPROTO_TCP, OA_R, OA_R, OP_PASSNEXT, sizeof (u_long), 536 },

{ TCP_NOTIFY_THRESHOLD, IPPROTO_TCP, OA_RW, OA_RW, (OP_PASSNEXT|OP_DEF_FN),
	sizeof (int), -1 /* not initialized */ },

{ TCP_ABORT_THRESHOLD, IPPROTO_TCP, OA_RW, OA_RW, (OP_PASSNEXT|OP_DEF_FN),
	sizeof (int), -1 /* not initialized */ },

{ TCP_CONN_NOTIFY_THRESHOLD, IPPROTO_TCP, OA_RW, OA_RW,
	(OP_PASSNEXT|OP_DEF_FN), sizeof (int), -1 /* not initialized */ },

{ TCP_CONN_ABORT_THRESHOLD, IPPROTO_TCP, OA_RW, OA_RW, (OP_PASSNEXT|OP_DEF_FN),
	sizeof (int), -1 /* not initialized */ },

{ IP_OPTIONS,	IPPROTO_IP, OA_RW, OA_RW, (OP_PASSNEXT|OP_VARLEN|OP_NODEFAULT),
	40, -1 /* not initialized */ },

{ IP_TOS,	IPPROTO_IP, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ IP_TTL,	IPPROTO_IP, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
};

#define	TCP_OPT_ARR_CNT		A_CNT(tcp_opt_arr)

u_int tcp_max_optbuf_len; /* initialized when TCP driver is loaded */

/*
 * Initialize option database object for TCP
 *
 * This object represents database of options to search passed to
 * {sock,tpi}optcom_req() interface routine to take care of option
 * management and associated methods.
 */

optdb_obj_t tcp_opt_obj = {
	tcp_opt_default,	/* TCP default value function pointer */
	tcp_opt_get,		/* TCP get function pointer */
	tcp_opt_set,		/* TCP set function pointer */
	true,			/* TCP is tpi provider */
	TCP_OPT_ARR_CNT,	/* TCP option database count of entries */
	tcp_opt_arr		/* TCP option database */
};
