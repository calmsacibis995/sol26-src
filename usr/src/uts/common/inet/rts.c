/*
 * Copyright (c) 1992-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)rts.c	1.21	96/10/18 SMI"

#ifndef	MI_HDRS

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/stropts.h>
#include <sys/strlog.h>
#define	_SUN_TPI_VERSION 1
#include <sys/tihdr.h>
#include <sys/timod.h>
#include <sys/ddi.h>
#include <sys/cmn_err.h>
#include <sys/proc.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include <inet/common.h>
#include <inet/ip.h>
#include <inet/mi.h>
#include <inet/nd.h>
#include <inet/optcom.h>
#include <netinet/ip_mroute.h>
#include <sys/isa_defs.h>
#include <net/route.h>

#else

#include <types.h>
#include <stream.h>
#include <strsubr.h>
#include <stropts.h>
#include <strlog.h>
#include <tihdr.h>
#include <timod.h>
#include <tiuser.h>

#include <socket.h>
#include <in.h>

#include <common.h>
#include <ip.h>
#include <mi.h>
#include <nd.h>
#include <optcom.h>
#include <ip_mroute.h>
#include <isa_defs.h>
#include <route.h>

#endif

#ifndef	STRMSGSZ
#define	STRMSGSZ	4096
#endif

/*
 * This is a transport provider for routing sockets.  Downstream messages are
 * wrapped with a IP_IOCTL header, and ip_wput_ioctl calls the appropriate entry
 * in the ip_ioctl_ftbl callout table to pass the routing socket data into IP.
 * Upstream messages are generated for listeners of the routing socket as well
 * as the message sender (unless they have turned off their end using
 * SO_USELOOPBACK or shutdown(3n)).  Upstream messages may also be generated
 * asynchronously when:
 *
 *	Interfaces are brought up or down.
 *	Addresses are assigned to interfaces.
 *	ICMP redirects are processed and a IRE_HOST_REDIRECT is installed.
 *	No route is found while sending a packet.
 *	When TCP requests IP to remove an IRE_CACHE of a troubled destination.
 *
 * Synchronization notes:
 *
 * At all points in this code
 * where exclusive, writer, access is required, we pass a message to a
 * subroutine by invoking "become_writer" which will arrange to call the
 * routine only after all reader threads have exited the shared resource, and
 * the writer lock has been acquired. For uniprocessor, single-thread,
 * nonpreemptive environments, become_writer can simply be a macro which
 * invokes the routine immediately.
 */


/*
 * Object to represent database of options to search passed to
 * {sock,tpi}optcom_req() interface routine to take care of option
 * management and associated methods.
 * XXX. These and other externs should really move to a rts header.
 */
extern optdb_obj_t	rts_opt_obj;
extern u_int		rts_max_optbuf_len;


/* Internal routing socket stream control structure, one per open stream */
typedef	struct rts_s {
	uint	rts_state;		/* Provider interface state */
	uint	rts_error;		/* Routing socket error code */
	uint	rts_flag;		/* Pending I/O state */
	uint	rts_priv_stream : 1,	/* Stream opened by privileged user. */
		rts_debug : 1,		/* SO_DEBUG "socket" option. */
		rts_dontroute : 1,	/* SO_DONTROUTE "socket" option. */
		rts_broadcast : 1,	/* SO_BROADCAST "socket" option. */

		rts_reuseaddr : 1,	/* SO_REUSEADDR "socket" option. */
		rts_useloopback : 1,	/* SO_USELOOPBACK "socket" option. */
		rts_multicast_loop : 1,	/* IP_MULTICAST_LOOP option */
		rts_hdrincl : 1,	/* IP_HDRINCL option + RAW and IGMP */

		: 0;
} rts_t;

#define	RTS_WPUT_PENDING	0x1	/* Waiting for write-side to complete */
#define	RTS_WRW_PENDING		0x2	/* Routing socket write in progress */

/* Default structure copied into T_INFO_ACK messages */
static	struct T_info_ack rts_g_t_info_ack = {
	T_INFO_ACK,
	T_INFINITE,	/* TSDU_size. Maximum size messages. */
	T_INVALID,	/* ETSDU_size. No expedited data. */
	T_INVALID,	/* CDATA_size. No connect data. */
	T_INVALID,	/* DDATA_size. No disconnect data. */
	0,		/* ADDR_size. */
	0,		/* OPT_size - not initialized here */
	64 * 1024,	/* TIDU_size. rts allows maximum size messages. */
	T_COTS,		/* SERV_type. rts supports connection oriented. */
	TS_UNBND,	/* CURRENT_state. This is set from rts_state. */
	(XPG4_1)	/* PROVIDER_flag */
};


/* Named Dispatch Parameter Management Structure */
typedef struct rtspparam_s {
	u_long	rts_param_min;
	u_long	rts_param_max;
	u_long	rts_param_value;
	char	*rts_param_name;
} rtsparam_t;

/*
 * Table of ND variables supported by rts. These are loaded into rts_g_nd
 * in rts_open.
 * All of these are alterable, within the min/max values given, at run time.
 */
static	rtsparam_t	rts_param_arr[] = {
	/* min	max	value	name */
	{ 4096,	65536,	8192,	"rts_xmit_hiwat"},
	{ 0,	65536,	1024,	"rts_xmit_lowat"},
	{ 4096,	65536,	8192,	"rts_recv_hiwat"},
	{ 65536, 1024*1024*1024, 256*1024,	"rts_max_buf"},
};
#define	rts_xmit_hiwat			rts_param_arr[0].rts_param_value
#define	rts_xmit_lowat			rts_param_arr[1].rts_param_value
#define	rts_recv_hiwat			rts_param_arr[2].rts_param_value
#define	rts_max_buf			rts_param_arr[3].rts_param_value

static mblk_t	*rts_ioctl_alloc(MBLKP data);
static	int	rts_close(queue_t *q);
static	int	rts_open(queue_t *q, dev_t *devp, int flag, int sflag,
    cred_t *credp);
static	void	rts_wput(queue_t *q, MBLKP mp);
static void 	rts_wput_other(queue_t *q, MBLKP mp);
static	void	rts_wput_iocdata(queue_t * q, MBLKP mp);
static void 	rts_err_ack(queue_t *q, MBLKP mp, int t_error, int sys_error);
int		rts_opt_default(queue_t *q, int level, int name, u_char *ptr);
int		rts_opt_get(queue_t *q, int level, int name, u_char *ptr);
int		rts_opt_set(queue_t *q, u_int mgmt_flags, int level, int name,
		u_int inlen, u_char *invalp, u_int *outlenp, u_char *outvalp);

static	void	rts_rput(queue_t *q, MBLKP mp);
static	int	rts_wrw(queue_t *q, struiod_t *dp);

static	void	rts_param_cleanup(void);
static int	rts_param_get(queue_t *q, mblk_t *mp, caddr_t cp);
static boolean_t rts_param_register(rtsparam_t *rtspa, int cnt);
static int	rts_param_set(queue_t *q, mblk_t *mp, char *value, caddr_t cp);

static struct module_info info = {
	9999, "rts", 1, INFPSZ, 512, 128
};

static struct qinit rinit = {
	(pfi_t)rts_rput, nil(pfi_t), rts_open, rts_close, nil(pfi_t), &info
};

static struct qinit winit = {
	(pfi_t)rts_wput, nil(pfi_t), nil(pfi_t), nil(pfi_t), nil(pfi_t), &info,
	nil(struct module_stat *), (pfi_t)rts_wrw, nil(pfi_t), STRUIOT_STANDARD
};

struct streamtab rtsinfo = {
	&rinit, &winit
};

static	IDP	rts_g_nd;	/* Points to table of RTS ND variables. */
u_int		rts_open_streams = 0;



/*
 * This routine allocates the necessary
 * message blocks for IOCTL wrapping the
 * user data.
 */
static mblk_t *
rts_ioctl_alloc(data)
	mblk_t	*data;
{
	mblk_t	*mp = NULL;
	mblk_t	*mp1 = NULL;
	ipllc_t	*ipllc;
	struct iocblk	*ioc;

	mp = allocb(sizeof (ipllc_t), BPRI_MED);
	if (mp == NULL)
		return (NULL);
	mp1 = allocb(sizeof (struct iocblk), BPRI_MED);
	if (mp1 == NULL) {
		freemsg(mp);
		return (NULL);
	}
	ipllc = (ipllc_t *)mp->b_rptr;
	ipllc->ipllc_cmd = IP_IOC_RTS_REQUEST;
	ipllc->ipllc_name_offset = 0;
	ipllc->ipllc_name_length = 0;
	mp->b_wptr = (u_char *)ALIGN32(mp->b_rptr + sizeof (ipllc_t));
	if (data != NULL)
		linkb(mp, data);

	ioc = (struct iocblk *)mp1->b_rptr;
	ioc->ioc_cmd = IP_IOCTL;
	ioc->ioc_error = 0;
	ioc->ioc_cr = NULL;
	ioc->ioc_count = msgdsize(mp);
	mp1->b_wptr = (u_char *)ALIGN32(mp1->b_rptr + sizeof (struct iocblk));
	mp1->b_datap->db_type = M_IOCTL;

	linkb(mp1, mp);
	return (mp1);
}

/*
 * This routine closes rts stream, by disabling
 * put/srv routines and freeing the this module
 * internal datastructure.
 */
static int
rts_close(q)
	queue_t	*q;
{
	qprocsoff(q);
	mi_free(q->q_ptr);
	rts_open_streams--;
	/*
	 * Free the ND table if this was
	 * the last stream close
	 */
	rts_param_cleanup();
	return (0);
}

/*
 * This is the open routine for routing socket. It allocates
 * rts_t structure for the stream and sends an IOCTL to
 * the down module to indicate that it is a routing socket
 * stream.
 */
/* ARGSUSED */
static int
rts_open(q, devp, flag, sflag, credp)
	queue_t	*q;
	dev_t	*devp;
	int	flag;
	int	sflag;
	cred_t	*credp;
{
	mblk_t	*mp = NULL;
	boolean_t	priv = drv_priv(credp) == 0;
	rts_t	*rts;

	/* If the stream is already open, return immediately. */
	if ((rts = (rts_t *)q->q_ptr) != NULL) {
		if (rts->rts_priv_stream && !priv)
			return (EPERM);
		return (0);
	}
	/* If this is not a push of rts as a module, fail. */
	if (sflag != MODOPEN)
		return (EINVAL);

	/* If this is the first open of rts, create the ND table. */
	if (rts_g_nd == NULL) {
		if (!rts_param_register(rts_param_arr, A_CNT(rts_param_arr)))
			return (ENOMEM);
	}
	q->q_ptr = mi_zalloc((uint) sizeof (rts_t));
	WR(q)->q_ptr = q->q_ptr;
	rts = (rts_t *)q->q_ptr;
	if (priv)
		rts->rts_priv_stream = 1;
	/*
	 * The receive hiwat is only looked at on the stream head queue.
	 * Store in q_hiwat in order to return on SO_RCVBUF getsockopts.
	 */
	q->q_hiwat = rts_recv_hiwat;
	/*
	 * The transmit hiwat/lowat is only looked at on IP's queue.
	 * Store in q_hiwat/q_lowat in order to return on SO_SNDBUF/SO_SNDLOWAT
	 * getsockopts.
	 */
	WR(q)->q_hiwat = rts_xmit_hiwat;
	WR(q)->q_lowat = rts_xmit_lowat;
	qprocson(q);
	/*
	 * Indicate the down IP module that this is
	 * a routing socket client by sending an RTS IOCTL
	 * without any user data.
	 */
	mp = rts_ioctl_alloc(NULL);
	if (mp == NULL) {
		rts_param_cleanup();
		qprocsoff(q);
		return (ENOMEM);
	}
	rts_open_streams++;
	putnext(WR(q), mp);
	rts->rts_state = TS_UNBND;
	return (0);
}

/*
 * rts_err_ack. This routine creates a
 * T_ERROR_ACK message and passes it
 * upstream.
 */
static void
rts_err_ack(q, mp, t_error, sys_error)
	queue_t	*q;
	mblk_t	*mp;
	int	t_error;
	int	sys_error;
{
	if ((mp = mi_tpi_err_ack_alloc(mp, t_error, sys_error)) != NULL)
		qreply(q, mp);
}

/*
 * rts_ok_ack. This routine creates a
 * T_OK_ACK message and passes it
 * upstream.
 */
static void
rts_ok_ack(q, mp)
	queue_t	*q;
	mblk_t	*mp;
{
	if ((mp = mi_tpi_ok_ack_alloc(mp)) != NULL)
		qreply(q, mp);
}

/*
 * This routine is called by rts_wput to handle T_UNBIND_REQ messages.
 * After some error checking, the message is passed downstream to ip.
 */
static void
rts_unbind(q, mp)
	queue_t	*q;
	mblk_t	*mp;
{
	rts_t	*rts;

	rts = (rts_t *)q->q_ptr;
	/* If a bind has not been done, we can't unbind. */
	if (rts->rts_state != TS_IDLE) {
		rts_err_ack(q, mp, TOUTSTATE, 0);
		return;
	}
	rts->rts_state = TS_UNBND;
	rts_ok_ack(q, mp);
}

/*
 * This routine is called to handle each
 * O_T_BIND_REQ/T_BIND_REQ message passed to
 * rts_wput. Note: This routine works with both
 * O_T_BIND_REQ and T_BIND_REQ semantics.
 */
static void
rts_bind(q, mp)
	queue_t	*q;
	mblk_t	*mp;
{
	mblk_t	*mp1;
	struct T_bind_req *tbr;
	rts_t	*rts;

	rts = (rts_t *)q->q_ptr;
	if ((mp->b_wptr - mp->b_rptr) < sizeof (*tbr)) {
		mi_strlog(q, 1, SL_ERROR|SL_TRACE,
			"rts_bind: bad data, %d", rts->rts_state);
		rts_err_ack(q, mp, TBADADDR, 0);
		return;
	}
	if (rts->rts_state != TS_UNBND) {
		mi_strlog(q, 1, SL_ERROR|SL_TRACE,
			"rts_bind: bad state, %d", rts->rts_state);
		rts_err_ack(q, mp, TOUTSTATE, 0);
		return;
	}
	/*
	 * Reallocate the message to make sure we have enough room for an
	 * address and the protocol type.
	 */
	mp1 = mi_reallocb(mp, sizeof (struct T_bind_ack) + sizeof (ipa_t));
	if (mp1 == NULL) {
		rts_err_ack(q, mp, TSYSERR, ENOMEM);
		return;
	}
	mp = mp1;
	tbr = (struct T_bind_req *)ALIGN32(mp->b_rptr);
	if (tbr->ADDR_length != 0) {
		mi_strlog(q, 1, SL_ERROR|SL_TRACE,
			"rts_bind: bad ADDR_length %d", tbr->ADDR_length);
		rts_err_ack(q, mp, TBADADDR, 0);
		return;
	}
	/* Generic request */
	tbr->ADDR_offset = sizeof (struct T_bind_req);
	tbr->ADDR_length = 0;
	tbr->PRIM_type = T_BIND_ACK;
	rts->rts_state = TS_IDLE;
	qreply(q, mp);
}

/*
 * This routine responds to T_INFO_REQ messages. It is called by
 * rts_wput_other.
 * Most of the T_INFO_ACK information is copied from rts_g_t_info_ack.
 * The current state of the stream is copied from rts_state.
 */
static void
rts_info_req(q, mp)
	queue_t	*q;
	mblk_t	*mp;
{
	rts_t	*rts = (rts_t *)q->q_ptr;

	mp = mi_tpi_ack_alloc(mp, sizeof (struct T_info_ack), T_INFO_ACK);
	if (mp == NULL)
		return;
	bcopy((char *)&rts_g_t_info_ack, (char *)mp->b_rptr,
	    sizeof (struct T_info_ack));
	((struct T_info_ack *)ALIGN32(mp->b_rptr))->CURRENT_state =
		rts->rts_state;
	((struct T_info_ack *)ALIGN32(mp->b_rptr))->OPT_size =
		rts_max_optbuf_len;
	qreply(q, mp);
}


/*
 * This routine gets default values of certain options whose default
 * values are maintained by protcol specific code
 */

/* ARGSUSED */
int
rts_opt_default(q, level, name, ptr)
	queue_t	*q;
	int	level;
	int	name;
	u_char	*ptr;
{
	/* no default value processed by protocol specific code currently */
	return (-1);
}

/*
 * This routine retrieves the current status of socket options.
 * It returns the size of the option retrieved.
 */

int
rts_opt_get(q, level, name, ptr)
	queue_t	*q;
	int	level;
	int	name;
	u_char	*ptr;
{
	int	*i1 = (int *)ALIGN32(ptr);
	rts_t	*rts = (rts_t *)q->q_ptr;

	switch (level) {
	case SOL_SOCKET:
		switch (name) {
		case SO_DEBUG:
			*i1 = rts->rts_debug;
			break;
		case SO_REUSEADDR:
			*i1 = rts->rts_reuseaddr;
			break;
		case SO_TYPE:
			*i1 = SOCK_RAW;
			break;

		/*
		 * The following three items are available here,
		 * but are only meaningful to IP.
		 */
		case SO_DONTROUTE:
			*i1 = rts->rts_dontroute;
			break;
		case SO_USELOOPBACK:
			*i1 = rts->rts_useloopback;
			break;
		case SO_BROADCAST:
			*i1 = rts->rts_broadcast;
			break;
		/*
		 * The following two items can be manipulated,
		 * but changing them should do nothing.
		 */
		case SO_SNDBUF:
			*i1 = q->q_hiwat;
			break;
		case SO_RCVBUF:
			*i1 = RD(q)->q_hiwat;
			break;
		default:
			return (-1);
		}
		break;
	default:
		return (-1);
	}
	return (sizeof (int));
}


/* This routine sets socket options. */
int
rts_opt_set(q, mgmt_flags, level, name, inlen, invalp, outlenp, outvalp)
	queue_t	* q;
	u_int	mgmt_flags;
	int	level;
	int	name;
	u_int	inlen;
	u_char	*invalp;
	u_int	*outlenp;
	u_char	*outvalp;
{
	int	* i1 = (int *)ALIGN32(invalp);
	rts_t	* rts = (rts_t *)q->q_ptr;
	int	checkonly;

	if (mgmt_flags == (T_NEGOTIATE|T_CHECK)) {
		/*
		 * both set - magic signal that
		 * negotiation not from T_OPTMGMT_REQ
		 * Note: This is not currently possible
		 * in rts module.
		 */
		return (EINVAL);
	}

	if (mgmt_flags & T_NEGOTIATE) {
		ASSERT(mgmt_flags == T_NEGOTIATE ||
		    mgmt_flags == (T_CHECK|T_NEGOTIATE));
		checkonly = 0;
	} else {
		ASSERT(mgmt_flags == T_CHECK);
		checkonly = 1;
		/*
		 * Note: For T_CHECK,
		 * inlen != 0 implies value supplied and
		 * 	we have to "pretend" to set it.
		 * inlen == 0 implies that there is no
		 * 	value part in T_CHECK request just validation
		 * done elsewhere should be enough, we just return here.
		 */
		if (inlen == 0) {
			*outlenp = 0;
			return (0);
		}
	}
	ASSERT((mgmt_flags & T_NEGOTIATE) ||
		(mgmt_flags == T_CHECK && inlen != 0));

	/*
	 * For fixed length options, no sanity check
	 * of passed in length is done. It is assumed *_optcom_req()
	 * routines do the right thing.
	 */

	switch (level) {
	case SOL_SOCKET:
		switch (name) {
		case SO_REUSEADDR:
			if (! checkonly)
				rts->rts_reuseaddr = *i1;
			break;	/* goto sizeof (int) option return */
		case SO_DEBUG:
			if (! checkonly)
				rts->rts_debug = *i1;
			break;	/* goto sizeof (int) option return */
		/*
		 * The following three items are available here,
		 * but are only meaningful to IP.
		 */
		case SO_DONTROUTE:
			if (! checkonly)
				rts->rts_dontroute = *i1;
			break;	/* goto sizeof (int) option return */
		case SO_USELOOPBACK:
			if (! checkonly)
				rts->rts_useloopback = *i1;
			break;	/* goto sizeof (int) option return */
		case SO_BROADCAST:
			if (! checkonly)
				rts->rts_broadcast = *i1;
			break;	/* goto sizeof (int) option return */
		/*
		 * The following two items can be manipulated,
		 * but changing them should do nothing.
		 */
		case SO_SNDBUF:
			if (*i1 > rts_max_buf) {
				*outlenp = 0;
				return (ENOBUFS);
			}
			if (! checkonly) {
				q->q_hiwat = *i1;
				q->q_next->q_hiwat = *i1;
			}
			break;	/* goto sizeof (int) option return */
		case SO_RCVBUF:
			if (*i1 > rts_max_buf) {
				*outlenp = 0;
				return (ENOBUFS);
			}
			if (! checkonly) {
				RD(q)->q_hiwat = *i1;
				mi_set_sth_hiwat(RD(q), *i1);
			}
			break;	/* goto sizeof (int) option return */
		default:
			*outlenp = 0;
			return (EINVAL);
		}
		break;
	default:
		*outlenp = 0;
		return (EINVAL);
	}
	/*
	 * Common case of return from an option that is sizeof (int)
	 */
	*(int *) outvalp = *i1;
	*outlenp = sizeof (int);
	return (0);
}

/*
 * This routine frees the ND table if all streams have been closed.
 * It is called by rts_close and rts_open.
 */
static void
rts_param_cleanup(void)
{
	if (!rts_open_streams)
		nd_free(&rts_g_nd);
}

/*
 * This routine retrieves the value of an ND variable in a rtsparam_t
 * structure. It is called through nd_getset when a user reads the
 * variable.
 */
/* ARGSUSED */
static int
rts_param_get(q, mp, cp)
	queue_t	*q;
	mblk_t	*mp;
	caddr_t	cp;
{
	rtsparam_t	*rtspa = (rtsparam_t *)ALIGN32(cp);

	mi_mpprintf(mp, "%ld", rtspa->rts_param_value);
	return (0);
}

/*
 * Walk through the param array specified registering each element with the
 * named dispatch (ND) handler.
 */
static boolean_t
rts_param_register(rtspa, cnt)
	rtsparam_t	*rtspa;
	int	cnt;
{
	for (; cnt-- > 0; rtspa++) {
		if (rtspa->rts_param_name != NULL && rtspa->rts_param_name[0]) {
			if (!nd_load(&rts_g_nd, rtspa->rts_param_name,
			    rts_param_get, rts_param_set, (caddr_t)rtspa)) {
				nd_free(&rts_g_nd);
				return (false);
			}
		}
	}
	return (true);
}

/* This routine sets an ND variable in a rtsparam_t structure. */
/* ARGSUSED */
static int
rts_param_set(q, mp, value, cp)
	queue_t	*q;
	mblk_t	*mp;
	char	*value;
	caddr_t	cp;
{
	char	*end;
	long	new_value;
	rtsparam_t	*rtspa = (rtsparam_t *)ALIGN32(cp);

	/* Convert the value from a string into a long integer. */
	new_value = mi_strtol(value, &end, 10);
	/*
	 * Fail the request if the new value does not lie within the
	 * required bounds.
	 */
	if (end == value ||
	    new_value < rtspa->rts_param_min ||
	    new_value > rtspa->rts_param_max)
		return (EINVAL);

	/* Set the new value */
	rtspa->rts_param_value = new_value;
	return (0);
}

/*
 * This routine handles synchronous messages passed downstream. It either
 * consumes the message or passes it downstream; it never queues a
 * a message. The data messages that go down are wrapped in an IOCTL
 * message.
 * Since it is synchronous it waits for the M_IOCACK/M_IOCNAK so that
 * it can return an immediate error (such as ENETUNREACH when adding a route).
 * It uses the RTS_WRW_PENDING to ensure that each rts instance has only
 * one M_IOCTL outstanding at any given time.
 */
static int
rts_wrw(q, dp)
	queue_t	* q;
	struiod_t * dp;
{
	mblk_t	* mp = dp->d_mp;
	int	error;
	rt_msghdr_t	*rtm;
	rts_t	*rts;
	mblk_t	*mp1;

	rts = (rts_t *)q->q_ptr;
	while (rts->rts_flag & RTS_WRW_PENDING)
		qwait_rw(q);
	rts->rts_flag |= RTS_WRW_PENDING;

	if (isuioq(q) && (error = struioget(q, mp, dp, 0)))
		/*
		 * Uio error of some sort, so just return the error.
		 */
		return (error);
	/*
	 * Pass the mblk (chain) onto wput().
	 */
	dp->d_mp = 0;

	switch (mp->b_datap->db_type) {
	case M_PROTO:
	case M_PCPROTO:
		/* Expedite other than T_DATA_REQ to below the switch */
		if (((mp->b_wptr - mp->b_rptr) != sizeof (struct T_data_req)) ||
		    (((union T_primitives *)ALIGN32(mp->b_rptr))->type !=
			T_DATA_REQ))
			break;
		if ((mp1 = mp->b_cont) == NULL)
			return (EINVAL);
		(void) unlinkb(mp);
		freemsg(mp);
		mp = mp1;
		/* FALLTHRU */
	case M_DATA:
		if ((mp->b_wptr - mp->b_rptr) < sizeof (rt_msghdr_t)) {
			if (!pullupmsg(mp, sizeof (rt_msghdr_t)))
				return (EINVAL);
		}
		rtm = (rt_msghdr_t *)ALIGN32(mp->b_rptr);
		if (rtm->rtm_version != RTM_VERSION)
			return (EPROTONOSUPPORT);
		rtm->rtm_pid = curproc->p_pid;
		break;
	default:
		break;
	}
	rts->rts_flag |= RTS_WPUT_PENDING;
	rts_wput(q, mp);
	while (rts->rts_flag & RTS_WPUT_PENDING)
		qwait_rw(q);
	rts->rts_flag &= ~RTS_WRW_PENDING;
	return (rts->rts_error);
}

/*
 * This routine handles all messages passed downstream. It either
 * consumes the message or passes it downstream; it never queues a
 * a message. The data messages that go down are wrapped in an IOCTL
 * message.
 */
static void
rts_wput(q, mp)
	queue_t	*q;
	mblk_t	*mp;
{
	u_char	*rptr = mp->b_rptr;
	mblk_t	*mp1;
	u32	u1;

	switch (mp->b_datap->db_type) {
	case M_DATA:
		break;
	case M_PROTO:
	case M_PCPROTO:
		u1 = mp->b_wptr - rptr;
		if (u1 == sizeof (struct T_data_req)) {
			/* Expedite valid T_DATA_REQ to below the switch */
			if (((union T_primitives *)ALIGN32(rptr))->type ==
			    T_DATA_REQ) {
				if ((mp1 = mp->b_cont) == NULL) {
					freemsg(mp);
					return;
				}
				(void) unlinkb(mp);
				freemsg(mp);
				mp = mp1;
				break;
			}
		}
		/* FALLTHRU */
	default:
		rts_wput_other(q, mp);
		return;
	}
	mp1 = rts_ioctl_alloc(mp);
	if (mp1 == NULL) {
		freemsg(mp);
		return;
	}
	putnext(q, mp1);
}


/*
 * Handles all the control message, if it
 * can not understand it, it will
 * pass down stream.
 */
static void
rts_wput_other(q, mp)
	queue_t	*q;
	mblk_t	*mp;
{
	u_char	*rptr = mp->b_rptr;
	u32	u1;
	rts_t	*rts;
	struct iocblk	*iocp;

	rts = (rts_t *)q->q_ptr;
	switch (mp->b_datap->db_type) {
	case M_PROTO:
	case M_PCPROTO:
		u1 = mp->b_wptr - rptr;
		if (u1 < sizeof (long)) {
			/*
			 * If the message does not contain a PRIM_type,
			 * throw it away.
			 */
			freemsg(mp);
			return;
		}
		switch (((union T_primitives *)ALIGN32(rptr))->type) {
		case T_BIND_REQ:
		case O_T_BIND_REQ:
			rts_bind(q, mp);
			return;
		case T_UNBIND_REQ:
			rts_unbind(q, mp);
			return;
		case T_INFO_REQ:
			rts_info_req(q, mp);
			return;
		case O_T_OPTMGMT_REQ:
			svr4_optcom_req(q, mp, rts->rts_priv_stream,
			    &rts_opt_obj);
			return;
		case T_OPTMGMT_REQ:
			tpi_optcom_req(q, mp, rts->rts_priv_stream,
			    &rts_opt_obj);
			return;
		case T_CONN_RES:
		case T_DISCON_REQ:
			/* Not supported by rts. */
			rts_err_ack(q, mp, TNOTSUPPORT, 0);
			return;
		case T_DATA_REQ:
		case T_EXDATA_REQ:
		case T_ORDREL_REQ:
			/* Illegal for rts. */
			freemsg(mp);
			putnextctl1(RD(q), M_ERROR, EPROTO);
			return;
		default:
			break;
		}
		break;
	case M_IOCTL:
		iocp = (struct iocblk *)ALIGN32(mp->b_rptr);
		switch (iocp->ioc_cmd) {
		case ND_SET:
			if (!rts->rts_priv_stream) {
				iocp->ioc_error = EPERM;
				iocp->ioc_count = 0;
				mp->b_datap->db_type = M_IOCACK;
				qreply(q, mp);
				return;
			}
			/* FALLTHRU */
		case ND_GET:
			if (nd_getset(q, rts_g_nd, mp)) {
				qreply(q, mp);
				return;
			}
			break;
		case TI_GETPEERNAME:
			mi_copyin(q, mp, nilp(char), sizeof (struct strbuf));
			return;
		default:
			break;
		}
	case M_IOCDATA:
		rts_wput_iocdata(q, mp);
		return;
	default:
		break;
	}
	putnext(q, mp);
}

/*
 * rts_wput_iocdata is called by rts_wput_other to handle all M_IOCDATA
 * messages.
 */
static void
rts_wput_iocdata(q, mp)
	queue_t	* q;
	mblk_t	* mp;
{
	struct sockaddr	* rtsaddr;
	mblk_t	* mp1;
	struct strbuf * sb;

	/* Make sure it is one of ours. */
	switch (((struct iocblk *)ALIGN32(mp->b_rptr))->ioc_cmd) {
	case TI_GETPEERNAME:
		break;
	default:
		putnext(q, mp);
		return;
	}
	switch (mi_copy_state(q, mp, &mp1)) {
	case -1:
		return;
	case MI_COPY_CASE(MI_COPY_IN, 1):
		break;
	case MI_COPY_CASE(MI_COPY_OUT, 1):
		/* Copy out the strbuf. */
		mi_copyout(q, mp);
		return;
	case MI_COPY_CASE(MI_COPY_OUT, 2):
		/* All done. */
		mi_copy_done(q, mp, 0);
		return;
	default:
		mi_copy_done(q, mp, EPROTO);
		return;
	}
	sb = (struct strbuf *)ALIGN32(mp1->b_rptr);
	if (sb->maxlen < sizeof (struct sockaddr)) {
		mi_copy_done(q, mp, EINVAL);
		return;
	}
	switch (((struct iocblk *)ALIGN32(mp->b_rptr))->ioc_cmd) {
	case TI_GETPEERNAME:
		break;
	default:
		mi_copy_done(q, mp, EPROTO);
		return;
	}
	mp1 = mi_copyout_alloc(q, mp, sb->buf, sizeof (struct sockaddr));
	if (!mp1)
		return;
	sb->len = sizeof (struct sockaddr);
	rtsaddr = (struct sockaddr *)ALIGN32(mp1->b_rptr);
	mp1->b_wptr = (u_char *)&rtsaddr[1];
	bzero((char *)rtsaddr, sizeof (struct sockaddr));
	rtsaddr->sa_family = AF_ROUTE;
	/* Copy out the address */
	mi_copyout(q, mp);
}

static void
rts_rput(q, mp)
	queue_t	*q;
	mblk_t	*mp;
{
	rts_t	*rts;
	struct iocblk	*iocp;
	mblk_t *mp1;
	struct T_data_ind *tdi;

	rts = (rts_t *)q->q_ptr;
	switch (mp->b_datap->db_type) {
	case M_IOCACK:
	case M_IOCNAK:
		iocp = (struct iocblk *)ALIGN32(mp->b_rptr);
		if (rts->rts_flag & RTS_WPUT_PENDING) {
			rts->rts_error = iocp->ioc_error;
			rts->rts_flag &= ~RTS_WPUT_PENDING;
			freemsg(mp);
			return;
		}
		break;
	case M_DATA:
		/*
		 * Prepend T_DATA_IND to prevent the stream head from
		 * consolidating multiple messages together.
		 * If the allocation fails just send up the M_DATA.
		 */
		mp1 = allocb(sizeof (*tdi), BPRI_MED);
		if (mp1 != NULL) {
			mp1->b_cont = mp;
			mp = mp1;

			mp->b_datap->db_type = M_PROTO;
			mp->b_wptr += sizeof (*tdi);
			tdi = (struct T_data_ind *)mp->b_rptr;
			tdi->PRIM_type = T_DATA_IND;
			tdi->MORE_flag = 0;
		}
		break;
	default:
		break;
	}
	putnext(q, mp);
}


void
rts_ddi_init(void)
{
	rts_max_optbuf_len = optcom_max_optbuf_len(rts_opt_obj.odb_opt_des_arr,
	    rts_opt_obj.odb_opt_arr_cnt);
}
