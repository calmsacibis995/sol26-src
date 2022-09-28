/*	Copyright (c) 1996 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/

#pragma ident	"@(#)rts_opt_data.c	1.2	96/10/13 SMI"

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


extern int rts_opt_default(queue_t *q, int level, int name, u_char * ptr);
extern int rts_opt_get(queue_t *q, int level, int name, u_char * ptr);
extern int rts_opt_set(queue_t *q, u_int mgmt_flags, int level, int name,
	u_int inlen, u_char *invalp, u_int *outlenp, u_char *outvalp);

/*
 * Table of all known options handled on a RTS protocol stack.
 *
 * Note: This table contains options processed by both RTS and IP levels
 *       and is the superset of options that can be performed on a RTS over IP
 *       stack.
 */
opdes_t	rts_opt_arr[] = {

{ SO_DEBUG,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_DONTROUTE,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_USELOOPBACK, SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_BROADCAST,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_REUSEADDR, SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_TYPE,	SOL_SOCKET, OA_R, OA_R, OP_PASSNEXT, sizeof (int), 0 },
{ SO_SNDBUF,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_RCVBUF,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },

};


#define	RTS_OPT_ARR_CNT		A_CNT(rts_opt_arr)

u_int rts_max_optbuf_len; /* initialized in _init() */

/*
 * Intialize option database object for RTS
 *
 * This object represents database of options to search passed to
 * {sock,tpi}optcom_req() interface routine to take care of option
 * management and associated methods.
 */

optdb_obj_t rts_opt_obj = {
	rts_opt_default,	/* RTS default value function pointer */
	rts_opt_get,		/* RTS get function pointer */
	rts_opt_set,		/* RTS set function pointer */
	true,			/* RTS is tpi provider */
	RTS_OPT_ARR_CNT,	/* RTS option database count of entries */
	rts_opt_arr		/* RTS option database */
};
