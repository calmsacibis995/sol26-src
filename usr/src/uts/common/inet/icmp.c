/*
 * Copyright (c) 1992-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)icmp.c 1.39     96/10/16 SMI"

#ifndef	MI_HDRS

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/strlog.h>
#define	_SUN_TPI_VERSION 1
#include <sys/tihdr.h>
#include <sys/timod.h>
#include <sys/ddi.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>

#include <sys/socket.h>
#include <sys/isa_defs.h>

#include <net/route.h>

#include <inet/common.h>
#include <inet/ip.h>
#include <inet/mi.h>
#include <inet/nd.h>
#include <inet/optcom.h>

#include <netinet/in.h>
#include <netinet/ip_mroute.h>


#else

#include <types.h>
#include <stream.h>
#include <stropts.h>
#include <strlog.h>
#define	_SUN_TPI_VERSION 1
#include <tihdr.h>
#include <timod.h>
#include <debug.h>

#include <socket.h>
#include <isa_defs.h>

#include <route.h>

#include <common.h>
#include <ip.h>
#include <mi.h>
#include <nd.h>
#include <optcom.h>

#include <in.h>
#include <ip_mroute.h>

#endif

/*
 * Object to represent database of options to search passed to
 * {sock,tpi}optcom_req() interface routine to take care of option
 * management and associated methods.
 * XXX These and other extern's should really move to a icmp header.
 */
extern optdb_obj_t	icmp_opt_obj;
extern u_int		icmp_max_optbuf_len;

#ifndef	STRMSGSZ
#define	STRMSGSZ	4096
#endif

/*
 * Synchronization notes:
 *
 * At all points in this code
 * where exclusive, writer, access is required, we pass a message to a
 * subroutine by invoking "become_writer" which will arrange to call the
 * routine only after all reader threads have exited the shared resource, and
 * the writer lock has been acquired.  For uniprocessor, single-thread,
 * nonpreemptive environments, become_writer can simply be a macro which
 * invokes the routine immediately.
 */

/* Internal icmp control structure, one per open stream */
typedef	struct icmp_s {
	uint	icmp_state;		/* TPI state */
	u32	icmp_src;		/* Source address of this stream */
	u32	icmp_bound_src;		/* Explicitely bound to address */
	mblk_t	* icmp_hdr_mp;		/* IP header if "connected" */
	uint	icmp_hdr_length;	/* Number of bytes in icmp_hdr_mp */
	uint	icmp_family;	/* Address family used in bind,if any */
	uint	icmp_proto;
	uint	icmp_ip_snd_options_len; /* Length of IP options supplied. */
	u8	* icmp_ip_snd_options;	/* Pointer to IP options supplied */
	u8	icmp_multicast_ttl;	/* IP_MULTICAST_TTL option */
	u32	icmp_multicast_if_addr;	/* IP_MULTICAST_IF option */
	uint	icmp_priv_stream : 1,	/* Stream opened by privileged user. */
	    icmp_debug : 1,		/* SO_DEBUG "socket" option. */
	    icmp_dontroute : 1,	/* SO_DONTROUTE "socket" option. */
	    icmp_broadcast : 1,	/* SO_BROADCAST "socket" option. */

	    icmp_reuseaddr : 1,	/* SO_REUSEADDR "socket" option. */
	    icmp_useloopback : 1,	/* SO_USELOOPBACK "socket" option. */
	    icmp_multicast_loop : 1,	/* IP_MULTICAST_LOOP option */
	    icmp_hdrincl : 1,	/* IP_HDRINCL option + RAW and IGMP */

	    icmp_dgram_errind : 1,	/* SO_DGRAM_ERRIND option */

	    icmp_pad_to_bit_31: 23;
	u8	icmp_type_of_service;
	u8	icmp_ttl;
} icmp_t;

/* Named Dispatch Parameter Management Structure */
typedef struct icmpparam_s {
	u_long	icmp_param_min;
	u_long	icmp_param_max;
	u_long	icmp_param_value;
	char	* icmp_param_name;
} icmpparam_t;

static	void	icmp_bind(queue_t * q, MBLKP mp);
static	void	icmp_bind_proto(queue_t * q);
static	int	icmp_close(queue_t * q);
static	void	icmp_connect(queue_t * q, MBLKP mp);
static void	icmp_err_ack(queue_t * q, MBLKP mp, int t_error,
    int sys_error);
static	void	icmp_info_req(queue_t * q, MBLKP mp);
static	void	icmp_addr_req(queue_t * q, MBLKP mp);
static	int	icmp_open(queue_t * q, dev_t * devp, int flag,
    int sflag, cred_t * credp);
static int icmp_unitdata_opt_process(queue_t *q, mblk_t *mp, int *t_errorp);
static	boolean_t icmp_allow_udropt_set(int level, int name);
int	icmp_opt_default(queue_t * q, int level, int name, u_char * ptr);
int	icmp_opt_get(queue_t * q, int level, int name, u_char * ptr);
int	icmp_opt_set(queue_t * q, u_int mgmt_flags, int level, int name,
	u_int inlen, u_char *invalp, u_int *outlenp, u_char *outvalp);
static	void	icmp_param_cleanup(void);
static int	icmp_param_get(queue_t * q, mblk_t * mp, caddr_t cp);
static boolean_t	icmp_param_register(icmpparam_t * icmppa, int cnt);
static int	icmp_param_set(queue_t * q, mblk_t * mp, char * value,
    caddr_t cp);
static	void	icmp_rput(queue_t * q, MBLKP mp);
static	int	icmp_status_report(queue_t * q, mblk_t * mp, caddr_t cp);
static void	icmp_ud_err(queue_t * q, MBLKP mp, int err);
static	void	icmp_unbind(queue_t * q, MBLKP mp);
static	void	icmp_wput(queue_t * q, MBLKP mp);
static	void	icmp_wput_other(queue_t * q, MBLKP mp);


static struct module_info info =  {
	5707, "icmp", 1, INFPSZ, 512, 128
};

static struct qinit rinit = {
	(pfi_t)icmp_rput, nil(pfi_t), icmp_open, icmp_close, nil(pfi_t), &info
};

static struct qinit winit = {
	(pfi_t)icmp_wput, nil(pfi_t), nil(pfi_t), nil(pfi_t), nil(pfi_t), &info
};

struct streamtab icmpinfo = {
	&rinit, &winit
};

int	icmpdevflag = 0;

static	ipa_t	ipa_null;	/* Zero address for quick clears */
static	void	* icmp_g_head;	/* Head for list of open icmp streams. */
static	IDP	icmp_g_nd;	/* Points to table of ICMP ND variables. */

/* Default structure copied into T_INFO_ACK messages */
static	struct T_info_ack icmp_g_t_info_ack = {
	T_INFO_ACK,
	(64 * 1024),	/* TSDU_size.  icmp allows maximum size messages. */
	T_INVALID,	/* ETSDU_size.  icmp does not support expedited data. */
	T_INVALID,	/* CDATA_size. icmp does not support connect data. */
	T_INVALID,	/* DDATA_size. icmp does not support disconnect data. */
	sizeof (ipa_t),	/* ADDR_size. */
	0,		/* OPT_size - not initialized here */
	64*1024,	/* TIDU_size.  icmp allows maximum size messages. */
	T_CLTS,		/* SERV_type.  icmp supports connection-less. */
	TS_UNBND,	/* CURRENT_state.  This is set from icmp_state. */
	(XPG4_1|SENDZERO) /* PROVIDER_flag */
};


/*
 * Table of ND variables supported by icmp.  These are loaded into icmp_g_nd
 * in icmp_open.
 * All of these are alterable, within the min/max values given, at run time.
 */
static	icmpparam_t	icmp_param_arr[] = {
	/* min	max	value	name */
	{ 0L,	128,	32,	"icmp_wroff_extra" },
	{ 1L,	255,	255,	"icmp_def_ttl" },
	{ 0,	1,	1,	"icmp_bsd_compat" },
	{ 4096,	65536,	8192,	"icmp_xmit_hiwat"},
	{ 0,	65536,	1024,	"icmp_xmit_lowat"},
	{ 4096,	65536,	8192,	"icmp_recv_hiwat"},
	{ 65536, 1024*1024*1024, 256*1024,	"icmp_max_buf"},
};
#define	icmp_wroff_extra		icmp_param_arr[0].icmp_param_value
#define	icmp_g_def_ttl			icmp_param_arr[1].icmp_param_value
#define	icmp_bsd_compat			icmp_param_arr[2].icmp_param_value
#define	icmp_xmit_hiwat			icmp_param_arr[3].icmp_param_value
#define	icmp_xmit_lowat			icmp_param_arr[4].icmp_param_value
#define	icmp_recv_hiwat			icmp_param_arr[5].icmp_param_value
#define	icmp_max_buf			icmp_param_arr[6].icmp_param_value

/*
 * This routine is called to handle each O_T_BIND_REQ/T_BIND_REQ message
 * passed to icmp_wput.
 * The O_T_BIND_REQ/T_BIND_REQ is passed downstream to ip with the ICMP
 * protocol type placed in the message following the address. A T_BIND_ACK
 * message is passed upstream when ip acknowledges the request.
 * (Called as writer.)
 */
static void
icmp_bind(q, mp)
	queue_t	* q;
	mblk_t	* mp;
{
	ipa_t	* ipa;
	mblk_t	* mp1;
	struct T_bind_req	* tbr;
	icmp_t	* icmp;
	u32	src;

	icmp = (icmp_t *)q->q_ptr;
	if ((mp->b_wptr - mp->b_rptr) < sizeof (*tbr)) {
		mi_strlog(q, 1, SL_ERROR|SL_TRACE,
		    "icmp_bind: bad req, len %d", mp->b_wptr - mp->b_rptr);
		icmp_err_ack(q, mp, TBADADDR, 0);
		return;
	}
	if (icmp->icmp_state != TS_UNBND) {
		mi_strlog(q, 1, SL_ERROR|SL_TRACE,
		    "icmp_bind: bad state, %d", icmp->icmp_state);
		icmp_err_ack(q, mp, TOUTSTATE, 0);
		return;
	}
	/*
	 * Reallocate the message to make sure we have enough room for an
	 * address and the protocol type.
	 */
	mp1 = mi_reallocb(mp, sizeof (struct T_bind_ack) + sizeof (ipa_t) + 1);
	if (!mp1) {
		icmp_err_ack(q, mp, TSYSERR, ENOMEM);
		return;
	}
	mp = mp1;
	tbr = (struct T_bind_req *)ALIGN32(mp->b_rptr);
	switch (tbr->ADDR_length) {
	case 0:			/* Generic request */
		tbr->ADDR_offset = sizeof (struct T_bind_req);
		tbr->ADDR_length = sizeof (ipa_t);
		ipa = (ipa_t *)&tbr[1];
		bzero((char *)ipa, sizeof (ipa_t));
		ipa->ip_family = AF_INET;
		mp->b_wptr = (u_char *)&ipa[1];
		break;
	case sizeof (ipa_t):	/* Complete IP address */
		ipa = (ipa_t *)ALIGN32(mi_offset_param(mp, tbr->ADDR_offset,
		    sizeof (ipa_t)));
		if (!ipa) {
			icmp_err_ack(q, mp, TSYSERR, EINVAL);
			return;
		}
		break;
	default:
		mi_strlog(q, 1, SL_ERROR|SL_TRACE,
		    "icmp_bind: bad ADDR_length %d", tbr->ADDR_length);
		icmp_err_ack(q, mp, TBADADDR, 0);
		return;
	}
	/*
	 * Copy the source address into our icmp structure.  This address
	 * may still be zero; if so, ip will fill in the correct address
	 * each time an outbound packet is passed to it.
	 * If we are binding to a broadcast or multicast address icmp_rput
	 * will clear the source address when it receives the T_BIND_ACK.
	 */
	bcopy((char *)ipa->ip_addr, (char *)&src, IP_ADDR_LEN);
	icmp->icmp_bound_src = icmp->icmp_src = src;

	icmp->icmp_family = ipa->ip_family;
	icmp->icmp_state = TS_IDLE;
	/*
	 * Place protocol type in the O_T_BIND_REQ/T_BIND_REQ following
	 * the address.
	 */
	*mp->b_wptr++ = icmp->icmp_proto;
	if (src != INADDR_ANY) {
		/*
		 * Append a request for an IRE if src not 0 (INADDR_ANY)
		 */
		mp->b_cont = allocb(sizeof (ire_t), BPRI_HI);
		if (!mp->b_cont) {
			icmp_err_ack(q, mp, TSYSERR, ENOMEM);
			return;
		}
		mp->b_cont->b_wptr += sizeof (ire_t);
		mp->b_cont->b_datap->db_type = IRE_DB_REQ_TYPE;
	}

	/* Pass the O_T_BIND_REQ/T_BIND_REQ to ip. */
	putnext(q, mp);
}

static void
icmp_bind_proto(q)
	queue_t	* q;
{
	ipa_t	* ipa;
	mblk_t	* mp;
	struct T_bind_req	* tbr;
	icmp_t	* icmp;

	icmp = (icmp_t *)q->q_ptr;
	mp = allocb(sizeof (struct T_bind_req) + sizeof (ipa_t) + 1,
	    BPRI_MED);
	if (!mp) {
		return;
	}
	mp->b_datap->db_type = M_PROTO;
	tbr = (struct T_bind_req *)ALIGN32(mp->b_rptr);
	tbr->PRIM_type = O_T_BIND_REQ; /* change to T_BIND_REQ ? */
	tbr->ADDR_offset = sizeof (struct T_bind_req);
	tbr->ADDR_length = sizeof (ipa_t);
	ipa = (ipa_t *)&tbr[1];
	bzero((char *)ipa, sizeof (ipa_t));
	ipa->ip_family = AF_INET;
	mp->b_wptr = (u_char *)&ipa[1];

	/* Place protocol type in the O_T_BIND_REQ following the address. */
	*mp->b_wptr++ = icmp->icmp_proto;

	/* Pass the O_T_BIND_REQ to ip. */
	putnext(q, mp);
}

/*
 * This routine handles each T_CONN_REQ message passed to icmp.  It
 * associates a default destination address with the stream.
 * A default IP header is created and pointed to by icmp_hdr_mp.
 * This header is prepended to subsequent M_DATA messages.
 */
static void
icmp_connect(q, mp)
	queue_t	* q;
		mblk_t	* mp;
{
	ipa_t	* ipa;
	ipha_t	* ipha;
	mblk_t	* mp1;
	struct T_conn_req	* tcr;
	icmp_t	* icmp;

	icmp = (icmp_t *)q->q_ptr;
	tcr = (struct T_conn_req *)ALIGN32(mp->b_rptr);
	/* Make sure the request contains an address. */
	if (tcr->DEST_length != sizeof (ipa_t) ||
	    (mp->b_wptr-mp->b_rptr <
		sizeof (struct T_conn_req)+sizeof (ipa_t))) {
		icmp_err_ack(q, mp, TBADADDR, 0);
		return;
	}
	if (icmp->icmp_state == TS_DATA_XFER) {
		/* Already connected - clear out state */
		icmp->icmp_src = icmp->icmp_bound_src;
		icmp->icmp_state = TS_IDLE;
	}


	if (tcr->OPT_length != 0) {
		icmp_err_ack(q, mp, TBADOPT, 0);
		return;
	}

	/*
	 * XXX Following comment obsolete ?
	 * Since the user did not pass in an IP header, we create
	 * a default one with no IP options.
	 */
	icmp->icmp_hdr_length = IP_SIMPLE_HDR_LENGTH;
#define	allocb_size	(IP_SIMPLE_HDR_LENGTH + icmp_wroff_extra)
	if (!(mp1 = allocb(allocb_size, BPRI_MED))) {
		icmp_err_ack(q, mp, TSYSERR, ENOMEM);
		return;
	}
#undef allocb_size
	mp1->b_rptr = mp1->b_datap->db_lim - icmp->icmp_hdr_length;
	bzero((char *)mp1->b_rptr, IP_SIMPLE_HDR_LENGTH);
	mp1->b_wptr = mp1->b_rptr + IP_SIMPLE_HDR_LENGTH;
	ipha = (ipha_t *)mp1->b_rptr;
#ifdef	_BIG_ENDIAN
	/* Set version, header length, and tos */
	*(u16 *)&ipha->ipha_version_and_hdr_length =
	    ((((IP_VERSION << 4) |
		(IP_SIMPLE_HDR_LENGTH>>2)) << 8) |
		icmp->icmp_type_of_service);
	/* Set ttl and protocol */
	*(u16 *)&ipha->ipha_ttl = (icmp->icmp_ttl << 8) |
	    icmp->icmp_proto;
#else
	/* Set version, header length, and tos */
	*(u16 *)&ipha->ipha_version_and_hdr_length =
	    ((icmp->icmp_type_of_service << 8) |
		((IP_VERSION << 4) | (IP_SIMPLE_HDR_LENGTH>>2)));
	/* Set ttl and protocol */
	*(u16 *)&ipha->ipha_ttl = (icmp->icmp_proto << 8) |
	    icmp->icmp_ttl;
#endif
	/* Now, finish initializing the IP header. */
	ipa = (ipa_t *)ALIGN32(&mp->b_rptr[tcr->DEST_offset]);
	ipha->ipha_fragment_offset_and_flags = 0;
	/*
	 * Copy the source address already bound to the stream.
	 * This may still be zero in which case ip will fill it in.
	 */
	ipha->ipha_src = icmp->icmp_src;

	/*
	 * Copy the destination address from the T_CONN_REQ message.
	 * Translate 0 to INADDR_LOOPBACK.
	 */
	bcopy((char *)ipa->ip_addr, (char *)&ipha->ipha_dst, IP_ADDR_LEN);
	if (ipha->ipha_dst == INADDR_ANY)
		ipha->ipha_dst = htonl(INADDR_LOOPBACK);

	icmp->icmp_state = TS_DATA_XFER;

	/* Save the new header */
	icmp->icmp_hdr_mp = mp1;

	if (icmp->icmp_src == INADDR_ANY) {
		/*
		 * Send down ire_lookup to IP to verify that there is a route
		 * and to determine the source address.
		 * This will come back as an IRE_DB_TYPE in rput.
		 */
		ire_t *ire;

		mp1 = allocb(sizeof (ire_t), BPRI_HI);
		if (mp1 == NULL) {
			icmp_err_ack(q, mp, TSYSERR, ENOMEM);
			return;
		}
		mp1->b_wptr += sizeof (ire_t);
		mp1->b_datap->db_type = IRE_DB_REQ_TYPE;

		ire = (ire_t *)ALIGN32(mp1->b_rptr);
		ire->ire_addr = ipha->ipha_dst;
		freemsg(mp);
		putnext(q, mp1);
	} else {
		/* Use ipa before freeing/reusing mp */
		mp1 = mi_tpi_conn_con(nil(MBLKP), (char *)ipa,
		    sizeof (ipa_t), nilp(char), 0);
		/* Acknowledge the request. */
		if ((mp = mi_tpi_ok_ack_alloc(mp)) != NULL)
			qreply(q, mp);
		/*
		 * We also have to send a connection confirmation to
		 * keep TLI happy
		 */
		if (mp1 != NULL)
			qreply(q, mp1);
	}
}

static int
icmp_close(q)
	queue_t	* q;
{
	icmp_t	* icmp = (icmp_t *)q->q_ptr;
	int	i1;

	qprocsoff(q);

	/* If there are any options associated with the stream, free them. */
	if (icmp->icmp_ip_snd_options)
		mi_free((char *)icmp->icmp_ip_snd_options);

	/* If there is a default header associated with the stream, free it. */
	if (icmp->icmp_hdr_mp)
		freemsg(icmp->icmp_hdr_mp);
	icmp->icmp_hdr_mp = NULL;

	/* Free the icmp structure and release the minor device number. */
	i1 = mi_close_comm(&icmp_g_head, q);

	/* Free the ND table if this was the last icmp stream open. */
	icmp_param_cleanup();

	return (i1);
}

/* This routine creates a T_ERROR_ACK message and passes it upstream. */
static void
icmp_err_ack(q, mp, t_error, sys_error)
	queue_t	* q;
	mblk_t	* mp;
	int	t_error;
	int	sys_error;
{
	if ((mp = mi_tpi_err_ack_alloc(mp, t_error, sys_error)) != NULL)
		qreply(q, mp);
}

/*
 * icmp_icmp_error is called by icmp_rput to process ICMP
 * messages passed up by IP.
 * Generates the appropriate T_UDERROR_IND.
 */
static void
icmp_icmp_error(q, mp)
	queue_t	* q;
		mblk_t	* mp;
{
	icmph_t * icmph;
	ipha_t	* ipha;
	int	iph_hdr_length;
	ipa_t	ipaddr;
	mblk_t	* mp1;
	int	error = 0;
	icmp_t	* icmp = (icmp_t *)q->q_ptr;

	ipha = (ipha_t *)mp->b_rptr;
	iph_hdr_length = IPH_HDR_LENGTH(ipha);
	icmph = (icmph_t *)ALIGN32(&mp->b_rptr[iph_hdr_length]);
	ipha = (ipha_t *)&icmph[1];
	iph_hdr_length = IPH_HDR_LENGTH(ipha);

	switch (icmph->icmph_type) {
	case ICMP_DEST_UNREACHABLE:
		switch (icmph->icmph_code) {
		case ICMP_FRAGMENTATION_NEEDED:
			/*
			 * XXX do something with MTU in rawip?
			 */
			break;
		case ICMP_PORT_UNREACHABLE:
		case ICMP_PROTOCOL_UNREACHABLE:
			error = ECONNREFUSED;
			break;
		default:
			break;
		}
		break;
	}
	if (error == 0) {
		freemsg(mp);
		return;
	}
	/*
	 * Can not deliver T_UDERROR_IND except when app has asked for them.
	 */
	if (!icmp->icmp_dgram_errind) {
		freemsg(mp);
		return;
	}
	bzero((char *)&ipaddr, sizeof (ipaddr));

	ipaddr.ip_family = AF_INET;
	bcopy((char *)&ipha->ipha_dst,
	    (char *)ipaddr.ip_addr,
	    sizeof (ipaddr.ip_addr));
	mp1 = mi_tpi_uderror_ind((char *)&ipaddr, sizeof (ipaddr), NULL, 0,
	    error);
	if (mp1)
		putnext(q, mp1);
	freemsg(mp);
}

/*
 * This routine responds to T_ADDR_REQ messages.  It is called by icmp_wput.
 * The local address is filled in if endpoint is bound. The remote address
 * is always null. (The concept of connected CLTS sockets is alien to TPI
 * and we do not currently implement it with ICMP.
 */
static void
icmp_addr_req(q, mp)
	queue_t	* q;
	mblk_t	* mp;
{
	icmp_t	* icmp = (icmp_t *)q->q_ptr;
	ipa_t	* ipa;
	mblk_t	* ackmp;
	struct T_addr_ack *taa;

	/* Note: allocation for remote address too though we don't use it */
	ackmp = mi_reallocb(mp, sizeof (struct T_addr_ack) + 2*sizeof (ipa_t));
	if (! ackmp) {
		icmp_err_ack(q, mp, TSYSERR, ENOMEM);
		return;
	}
	taa = (struct T_addr_ack *) ALIGN32(ackmp->b_rptr);

	bzero((char *)taa, sizeof (struct T_addr_ack));
	ackmp->b_wptr = (u_char *) &taa[1];

	taa->PRIM_type = T_ADDR_ACK;
	ackmp->b_datap->db_type = M_PCPROTO;

	/*
	 * Note: Following code assumes 32 bit alignment of basic
	 * data structures like ipa_t and struct T_addr_ack.
	 */
	if (icmp->icmp_state == TS_IDLE) {
		/*
		 * Fill in local address
		 */
		taa->LOCADDR_length = sizeof (ipa_t);
		taa->LOCADDR_offset = sizeof (*taa);

		ipa = (ipa_t *) &taa[1];
		/* Fill zeroes and then intialize non-zero fields */
		bzero((char *)ipa, sizeof (ipa_t));

		ipa->ip_family = AF_INET;

		bcopy((char *)&icmp->icmp_src, (char *)ipa->ip_addr,
		    sizeof (ipa->ip_addr));
		ipa->ip_port[0] = 0;
		ipa->ip_port[1] = 0;

		ackmp->b_wptr = (u_char *) &ipa[1];
	}
	qreply(q, ackmp);
}

/*
 * This routine responds to T_INFO_REQ messages.  It is called by icmp_wput.
 * Most of the T_INFO_ACK information is copied from icmp_g_t_info_ack.
 * The current state of the stream is copied from icmp_state.
 */
static void
icmp_info_req(q, mp)
	queue_t	* q;
	mblk_t	* mp;
{
	icmp_t	* icmp = (icmp_t *)q->q_ptr;

	mp = mi_tpi_ack_alloc(mp, sizeof (icmp_g_t_info_ack), T_INFO_ACK);
	if (!mp)
		return;
	bcopy((char *)&icmp_g_t_info_ack, (char *)mp->b_rptr,
	    sizeof (icmp_g_t_info_ack));
	((struct T_info_ack *)ALIGN32(mp->b_rptr))->CURRENT_state =
	    icmp->icmp_state;
	((struct T_info_ack *)ALIGN32(mp->b_rptr))->OPT_size =
		icmp_max_optbuf_len;
	qreply(q, mp);
}

/*
 * This is the open routine for icmp.  It allocates a icmp_t structure for
 * the stream and, on the first open of the module, creates an ND table.
 */
static int
icmp_open(q, devp, flag, sflag, credp)
	queue_t	* q;
	dev_t	* devp;
	int	flag;
	int	sflag;
	cred_t	* credp;
{
	int	err;
	boolean_t	privy = drv_priv(credp) == 0;
	icmp_t	* icmp;

	/* If the stream is already open, return immediately. */
	if ((icmp = (icmp_t *)q->q_ptr) != 0) {
		if (icmp->icmp_priv_stream && !privy)
			return (EPERM);
		return (0);
	}

	/* If this is not a push of icmp as a module, fail. */
	if (sflag != MODOPEN)
		return (EINVAL);

	/*
	 * Defer the qprocson until everything is initialized since
	 * we are D_MTPERQ and after qprocson the rput routine can
	 * run. (Could do qprocson earlier since icmp currently
	 * has an outer perimeter.)
	 */

	/* If this is the first open of icmp, create the ND table. */
	if (!icmp_g_nd &&
	    !icmp_param_register(icmp_param_arr, A_CNT(icmp_param_arr)))
		return (ENOMEM);
	/*
	 * Create a icmp_t structure for this stream and link into the
	 * list of open streams.
	 */
	err = mi_open_comm(&icmp_g_head, sizeof (icmp_t), q, devp,
	    flag, sflag, credp);
	/*
	 * If mi_open_comm failed and this is the first stream,
	 * release the ND table.
	 */
	icmp_param_cleanup();
	if (err)
		return (err);

	/*
	 * The receive hiwat is only looked at on the stream head queue.
	 * Store in q_hiwat in order to return on SO_RCVBUF getsockopts.
	 */
	q->q_hiwat = icmp_recv_hiwat;

	/* Set the initial state of the stream and the privilege status. */
	icmp = (icmp_t *)q->q_ptr;
	icmp->icmp_state = TS_UNBND;
	/* The protocol type may be changed by a SO_PROTOTYPE socket option. */
	icmp->icmp_proto = IPPROTO_ICMP;
	icmp->icmp_multicast_ttl = IP_DEFAULT_MULTICAST_TTL;
	icmp->icmp_multicast_loop = IP_DEFAULT_MULTICAST_LOOP;
	icmp->icmp_ttl = icmp_g_def_ttl;
	icmp->icmp_type_of_service = 0; /* XXX should have a global default */
	if (privy)
		icmp->icmp_priv_stream = 1;

	qprocson(q);

	/*
	 * The transmit hiwat is only looked at on IP's queue.
	 * Store in q_hiwat in order to return on SO_SNDBUF
	 * getsockopts.
	 */
	WR(q)->q_hiwat = icmp_xmit_hiwat;
	WR(q)->q_next->q_hiwat = WR(q)->q_hiwat;
	WR(q)->q_lowat = icmp_xmit_lowat;
	WR(q)->q_next->q_lowat = WR(q)->q_lowat;

	/* Set the Stream head write offset. */
	mi_set_sth_wroff(q, IP_SIMPLE_HDR_LENGTH + icmp_wroff_extra);
	mi_set_sth_hiwat(q, q->q_hiwat);

	return (0);
}


/*
 * Which UDP options OK to set through T_UNITDATA_REQ...
 */

/* ARGSUSED */
static boolean_t
icmp_allow_udropt_set(level, name)
	int	level;
	int	name;
{

	return (true);
}


/*
 * This routine gets default values of certain options whose default
 * values are maintained by protcol specific code
 */

/* ARGSUSED */
int
icmp_opt_default(q, level, name, ptr)
	queue_t	*q;
	int	level;
	int	name;
	u_char	*ptr;
{
	switch (level) {
	case IPPROTO_IP:
		switch (name) {
		case IP_MULTICAST_TTL:
			/* XXX - ndd variable someday ? */
			*ptr = (u_char) IP_DEFAULT_MULTICAST_LOOP;
			return (sizeof (u_char));
		case IP_MULTICAST_LOOP:
			/* XXX - ndd variable someday ? */
			*ptr = (u_char) IP_DEFAULT_MULTICAST_LOOP;
			return (sizeof (u_char));
		default:
			return (-1);
		}
	default:
		return (-1);
	}
	/* NOTREACHED */
}

/*
 * This routine retrieves the current status of socket options.
 * It returns the size of the option retrieved.
 */
int
icmp_opt_get(q, level, name, ptr)
	queue_t	* q;
	int	level;
	int	name;
	u_char	* ptr;
{
	int	* i1 = (int *)ALIGN32(ptr);
	icmp_t	* icmp = (icmp_t *)q->q_ptr;

	switch (level) {
	case SOL_SOCKET:
		switch (name) {
		case SO_DEBUG:
			*i1 = icmp->icmp_debug;
			break;
		case SO_TYPE:
			*i1 = SOCK_RAW;
			break;
		case SO_PROTOTYPE:
			*i1 = icmp->icmp_proto;
			break;
		case SO_REUSEADDR:
			*i1 = icmp->icmp_reuseaddr;
			break;
			/*
			 * The following three items are available here,
			 * but are only meaningful to IP.
			 */
		case SO_DONTROUTE:
			*i1 = icmp->icmp_dontroute;
			break;
		case SO_USELOOPBACK:
			*i1 = icmp->icmp_useloopback;
			break;
		case SO_BROADCAST:
			*i1 = icmp->icmp_broadcast;
			break;
			/*
			 * The following four items can be manipulated,
			 * but changing them should do nothing.
			 */
		case SO_SNDBUF:
			*i1 = q->q_hiwat;
			break;
		case SO_RCVBUF:
			*i1 = RD(q)->q_hiwat;
			break;
		case SO_DGRAM_ERRIND:
			*i1 = icmp->icmp_dgram_errind;
			break;
		/*
		 * Following three not meaningful for icmp
		 * Action is same as "default" to which we fallthrough
		 * so we keep them in comments.
		 * case SO_LINGER:
		 * case SO_KEEPALIVE:
		 * case SO_OOBINLINE:
		 */
		default:
			return (-1);
		}
		break;
	case IPPROTO_IP:
		switch (name) {
		case IP_OPTIONS:
			/* Options are passed up with each packet */
			return (0);
		case IP_HDRINCL:
			*i1 = (int) icmp->icmp_hdrincl;
			break;
		case IP_TOS:
			*i1 = (int) icmp->icmp_type_of_service;
			break;
		case IP_TTL:
			*i1 = (int) icmp->icmp_ttl;
			break;
		case IP_MULTICAST_IF:
			/* 0 address if not set */
			bcopy((char *)&icmp->icmp_multicast_if_addr,
			    (char *)ptr,
			    sizeof (icmp->icmp_multicast_if_addr));
			return (sizeof (icmp->icmp_multicast_if_addr));
		case IP_MULTICAST_TTL:
			bcopy((char *)&icmp->icmp_multicast_ttl,
			    (char *)ptr,
			    sizeof (icmp->icmp_multicast_ttl));
			return (sizeof (icmp->icmp_multicast_ttl));
		case IP_MULTICAST_LOOP:
			*ptr = icmp->icmp_multicast_loop;
			return (sizeof (u8));
		/*
		 * Cannot "get" the value of following options
		 * at this level. Action is same as "default" to
		 * which we fallthrough so we keep them in comments.
		 *
		 * case IP_ADD_MEMBERSHIP:
		 * case IP_DROP_MEMBERSHIP:
		 * case MRT_INIT:
		 * case MRT_DONE:
		 * case MRT_ADD_VIF:
		 * case MRT_DEL_VIF:
		 * case MRT_ADD_MFC:
		 * case MRT_DEL_MFC:
		 * case MRT_VERSION:
		 * case MRT_ASSERT:
		 */
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
icmp_opt_set(q, mgmt_flags, level, name, inlen, invalp, outlenp, outvalp)
	queue_t	*q;
	u_int	mgmt_flags;
	int	level;
	int	name;
	u_int	inlen;
	u_char	*invalp;
	u_int	*outlenp;
	u_char	*outvalp;
{
	int	*i1 = (int *)ALIGN32(invalp);
	icmp_t	*icmp = (icmp_t *)q->q_ptr;
	int	checkonly;

	if (mgmt_flags == (T_NEGOTIATE|T_CHECK)) {
		/*
		 * both set - magic signal that
		 * negotiation not from T_OPTMGMT_REQ
		 *
		 * Negotiating local and "association-related" options
		 * through T_UNITDATA_REQ.
		 *
		 * Following routine can filter out ones we do not
		 * want to be "set" this way.
		 */
		if (! icmp_allow_udropt_set(level, name)) {
			*outlenp = 0;
			return (EINVAL);
		}
	}

	if (mgmt_flags == T_NEGOTIATE) {

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
		 * 	value part in T_CHECK request and validation done
		 * elsewhere should be enough, we just return here.
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
		case SO_DEBUG:
			if (! checkonly)
				icmp->icmp_debug = *i1;
			break;	/* goto sizeof (int) option return */
		case SO_PROTOTYPE:
			if ((*i1 & 0xFF) != IPPROTO_ICMP &&
			    !icmp->icmp_priv_stream) {
				*outlenp = 0;
				return (EACCES);
			}
			if (checkonly) {
				/* T_CHECK case */
				*(int *) outvalp = (*i1 & 0xFF);
				break;
			}
			icmp->icmp_proto = *i1 & 0xFF;
			if (icmp->icmp_proto == IPPROTO_RAW ||
			    icmp->icmp_proto == IPPROTO_IGMP)
				icmp->icmp_hdrincl = 1;
			else
				icmp->icmp_hdrincl = 0;
			icmp_bind_proto(q);
			*outlenp = sizeof (int);
			*(int *) outvalp = *i1 & 0xFF;
			return (0);
		case SO_REUSEADDR:
			if (! checkonly)
				icmp->icmp_reuseaddr = *i1;
			break;	/* goto sizeof (int) option return */

			/*
			 * The following three items are available here,
			 * but are only meaningful to IP.
			 */
		case SO_DONTROUTE:
			if (! checkonly)
				icmp->icmp_dontroute = *i1;
			break;	/* goto sizeof (int) option return */
		case SO_USELOOPBACK:
			if (! checkonly)
				icmp->icmp_useloopback = *i1;
			break;	/* goto sizeof (int) option return */
		case SO_BROADCAST:
			if (! checkonly)
				icmp->icmp_broadcast = *i1;
			break;	/* goto sizeof (int) option return */

			/*
			 * The following four items can be manipulated,
			 * but changing them should do nothing.
			 */
		case SO_SNDBUF:
			if (*i1 > icmp_max_buf) {
				*outlenp = 0;
				return (ENOBUFS);
			}
			if (! checkonly) {
				q->q_hiwat = *i1;
				q->q_next->q_hiwat = *i1;
			}
			break;	/* goto sizeof (int) option return */
		case SO_RCVBUF:
			if (*i1 > icmp_max_buf) {
				*outlenp = 0;
				return (ENOBUFS);
			}
			if (! checkonly) {
				RD(q)->q_hiwat = *i1;
				mi_set_sth_hiwat(RD(q), *i1);
			}
			break;	/* goto sizeof (int) option return */
		case SO_DGRAM_ERRIND:
			if (! checkonly)
				icmp->icmp_dgram_errind = *i1;
			break;	/* goto sizeof (int) option return */
		/*
		 * Following three not meaningful for icmp
		 * Action is same as "default" so we keep them
		 * in comments.
		 * case SO_LINGER:
		 * case SO_KEEPALIVE:
		 * case SO_OOBINLINE:
		 */
		default:
			*outlenp = 0;
			return (EINVAL);
		}
		break;
	case IPPROTO_IP:
		switch (name) {
		case IP_OPTIONS:
			/* Save options for use by IP. */
			if (checkonly) {
				if (inlen & 0x3) {
					/* validate as in real "set" */
					*outlenp = 0;
					return (EINVAL);
				}
				/*
				 * OK return - copy input buffer
				 * into output buffer
				 */
				if (invalp != outvalp) {
					/*
					 * don't trust bcopy for
					 * identical src/dst
					 */
					(void) bcopy((char *)invalp,
					    (char *)outvalp, inlen);
				}
				*outlenp = inlen;
				return (0);
			}
			if (inlen & 0x3) {
				/* XXX check overflow inlen too as in tcp.c ? */
				*outlenp = 0;

				return (EINVAL);
			}
			if (icmp->icmp_ip_snd_options) {
				mi_free((char *)icmp->icmp_ip_snd_options);
				icmp->icmp_ip_snd_options_len = 0;
				icmp->icmp_ip_snd_options = NULL;
			}
			if (inlen) {
				icmp->icmp_ip_snd_options =
				    (u_char *)mi_alloc(inlen, BPRI_HI);
				if (icmp->icmp_ip_snd_options) {
					bcopy((char *)invalp,
					    (char *)icmp->icmp_ip_snd_options,
					    inlen);
					icmp->icmp_ip_snd_options_len = inlen;
				}
			}
			mi_set_sth_wroff(RD(q), IP_SIMPLE_HDR_LENGTH +
			    icmp->icmp_ip_snd_options_len +
			    icmp_wroff_extra);
			/* OK return - copy input buffer into output buffer */
			if (invalp != outvalp) {
				/* don't trust bcopy for identical src/dst */
				(void) bcopy((char *)invalp,
					(char *)outvalp, inlen);
			}
			*outlenp = inlen;
			return (0);
		case IP_HDRINCL:
			if (! checkonly)
				icmp->icmp_hdrincl = *i1;
			break;	/* goto sizeof (int) option return */
		case IP_TOS:
			if (! checkonly) {
				/*
				 * save tos in icmp state and connected
				 * ip header
				 */
				icmp->icmp_type_of_service = (u8) *i1;
				if (icmp->icmp_hdr_mp) {
					ipha_t *ipha =
					    (ipha_t *)icmp->icmp_hdr_mp->b_rptr;
					ipha->ipha_type_of_service = (u8) *i1;
				}
			}
			break;	/* goto sizeof (int) option return */
		case IP_TTL:
			if (! checkonly) {
				/*
				 * save ttl in icmp state and connected
				 * ip header
				 */
				icmp->icmp_ttl = (u8) *i1;
				if (icmp->icmp_hdr_mp) {
					ipha_t *ipha =
					    (ipha_t *)icmp->icmp_hdr_mp->b_rptr;
					ipha->ipha_ttl = (u8) *i1;
				}
			}
			break;	/* goto sizeof (int) option return */
		case IP_MULTICAST_IF:
			/*
			 * TODO should check OPTMGMT reply and undo this if
			 * there is an error.
			 */
			if (! checkonly)
				icmp->icmp_multicast_if_addr = *i1;
			break;	/* goto sizeof (int) option return */
		case IP_MULTICAST_TTL:
			if (! checkonly)
				icmp->icmp_multicast_ttl = *invalp;
			*outvalp = *invalp;
			*outlenp = sizeof (u_char);
			return (0);
		case IP_MULTICAST_LOOP:
			if (! checkonly)
				icmp->icmp_multicast_loop = *invalp;
			*outvalp = *invalp;
			*outlenp = sizeof (u_char);
			return (0);

		case IP_ADD_MEMBERSHIP:
		case IP_DROP_MEMBERSHIP:
		case MRT_INIT:
		case MRT_DONE:
		case MRT_ADD_VIF:
		case MRT_DEL_VIF:
		case MRT_ADD_MFC:
		case MRT_DEL_MFC:
		case MRT_VERSION:
		case MRT_ASSERT:
			/*
			 * "soft" error (negative)
			 * option not handled at this level
			 * Note: Do not modify *outlenp
			 */
			return (-EINVAL);
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
 * It is called by icmp_close and icmp_open.
 */
static void
icmp_param_cleanup()
{
	if (!icmp_g_head)
		nd_free(&icmp_g_nd);
}

/*
 * This routine retrieves the value of an ND variable in a icmpparam_t
 * structure.  It is called through nd_getset when a user reads the
 * variable.
 */
/* ARGSUSED */
static int
icmp_param_get(q, mp, cp)
	queue_t	* q;
	mblk_t	* mp;
	caddr_t	cp;
{
	icmpparam_t	* icmppa = (icmpparam_t *)ALIGN32(cp);

	mi_mpprintf(mp, "%ld", icmppa->icmp_param_value);
	return (0);
}

/*
 * Walk through the param array specified registering each element with the
 * named dispatch (ND) handler.
 */
static boolean_t
icmp_param_register(icmppa, cnt)
	icmpparam_t	* icmppa;
	int	cnt;
{
	for (; cnt-- > 0; icmppa++) {
		if (icmppa->icmp_param_name && icmppa->icmp_param_name[0]) {
			if (!nd_load(&icmp_g_nd, icmppa->icmp_param_name,
			    icmp_param_get, icmp_param_set,
			    (caddr_t)icmppa)) {
				nd_free(&icmp_g_nd);
				return (false);
			}
		}
	}
	if (!nd_load(&icmp_g_nd, "icmp_status", icmp_status_report, nil(pfi_t),
	    nil(caddr_t))) {
		nd_free(&icmp_g_nd);
		return (false);
	}
	return (true);
}

/* This routine sets an ND variable in a icmpparam_t structure. */
/* ARGSUSED */
static int
icmp_param_set(q, mp, value, cp)
	queue_t	* q;
	mblk_t	* mp;
	char	* value;
	caddr_t	cp;
{
	char	* end;
	long	new_value;
	icmpparam_t	* icmppa = (icmpparam_t *)ALIGN32(cp);

	/* Convert the value from a string into a long integer. */
	new_value = mi_strtol(value, &end, 10);
	/*
	 * Fail the request if the new value does not lie within the
	 * required bounds.
	 */
	if (end == value || new_value < icmppa->icmp_param_min ||
	    new_value > icmppa->icmp_param_max)
		return (EINVAL);
	/* Set the new value */
	icmppa->icmp_param_value = new_value;
	return (0);
}

static void
icmp_rput(q, mp_orig)
	queue_t	* q;
	mblk_t	* mp_orig;
{
	struct T_unitdata_ind	* tudi;
	mblk_t	* mp = mp_orig;
	u_char	* rptr;
	struct T_error_ack	* tea;
	icmp_t	* icmp;
	mblk_t *mp1;
	ire_t *ire;

	rptr = mp->b_rptr;
	switch (mp->b_datap->db_type) {
	case M_DATA:
		/*
		 * M_DATA messages contain IP packets.  They are handled
		 * following the switch.
		 */
		break;
	case M_PROTO:
	case M_PCPROTO:
		/* M_PROTO messages contain some type of TPI message. */
		if ((mp->b_wptr - rptr) < sizeof (long)) {
			freemsg(mp);
			return;
		}
		tea = (struct T_error_ack *)ALIGN32(rptr);
		switch (tea->PRIM_type) {
		case T_ERROR_ACK:
			switch (tea->ERROR_prim) {
			case O_T_BIND_REQ:
			case T_BIND_REQ:
				/*
				 * If our O_T_BIND_REQ/T_BIND_REQ fails,
				 * clear out the source address before
				 * passing the message upstream.
				 */
				icmp = (icmp_t *)q->q_ptr;
				if (icmp->icmp_state != TS_IDLE) {
					/*
					 * TPI has not yet bound - bind sent by
					 * icmp_bind_proto.
					 */
					freemsg(mp);
					return;
				}
				icmp->icmp_src = 0;
				icmp->icmp_bound_src = 0;
				if (icmp->icmp_hdr_mp) {
					ipha_t *ipha =
					    (ipha_t *)icmp->icmp_hdr_mp->b_rptr;

					ipha->ipha_src = 0;
				}
				icmp->icmp_state = TS_UNBND;
				break;
			default:
				break;
			}
			break;
		case T_BIND_ACK:
			/*
			 * We know if headers are included or not so we can
			 * safely do this.
			 */
			icmp = (icmp_t *)q->q_ptr;
			if (icmp->icmp_state != TS_IDLE) {
				/*
				 * TPI has not yet bound - bind sent by
				 * icmp_bind_proto.
				 */
				freemsg(mp);
				return;
			}
			/*
			 * If src_addr is not 0 (INADDR_ANY) already, we
			 * set it to 0 if broadcast address was bound.
			 * This ensures no datagrams with broadcast address
			 * as source address are emitted (which would violate
			 * RFC1122 - Hosts requirements)
			 */
			if (icmp->icmp_src == INADDR_ANY || mp->b_cont == NULL)
				break;
			mp1 = mp->b_cont;
			mp->b_cont = NULL;
			if (mp1->b_datap->db_type == IRE_DB_TYPE) {
				ire = (ire_t *) mp1->b_rptr;

				if (ire->ire_type == IRE_BROADCAST) {
					icmp->icmp_src = 0;
					icmp->icmp_bound_src = 0;
					if (icmp->icmp_hdr_mp) {
						ipha_t *ipha = (ipha_t *)
						    icmp->icmp_hdr_mp->b_rptr;
						ipha->ipha_src = 0;
					}
				}
				freemsg(mp1);
			}
			break;
		case T_OPTMGMT_ACK:
		case T_OK_ACK:
			break;
		default:
			freemsg(mp);
			return;
		}
		putnext(q, mp);
		return;
	case M_CTL:
		/* Contains ICMP packet from IP */
		icmp_icmp_error(q, mp);
		return;
	case M_FLUSH:
		if (*mp->b_rptr & FLUSHR)
			flushq(q, FLUSHDATA);
		putnext(q, mp);
		return;
	case IRE_DB_TYPE: {
		/*
		 * A response to an IRE_DB_REQ_TYPE sent when connecting.
		 * Verify that there is a route (ire_type != 0) and
		 * extract the source address.
		 */
		ire_t *ire;
		ipha_t *ipha;
		ipa_t ipaddr;

		if (mp->b_wptr - mp->b_rptr < sizeof (ire_t) ||
		    !OK_32PTR(mp->b_rptr)) {
			freemsg(mp);
			return;
		}
		icmp = (icmp_t *)q->q_ptr;
		ire = (ire_t *)ALIGN32(mp->b_rptr);
		if (ire->ire_type == 0) {
			/*
			 * No IRE found. Fail the T_CONN_REQ.
			 */
			if ((mp = mi_tpi_ack_alloc(mp,
			    sizeof (struct T_error_ack),
			    T_ERROR_ACK)) != NULL) {
				struct T_error_ack *tea;

				tea = (struct T_error_ack *)ALIGN32(mp->b_rptr);
				tea->ERROR_prim = T_CONN_REQ;
				tea->TLI_error = TSYSERR;
				tea->UNIX_error = ENETUNREACH;
				putnext(q, mp);
			}
			return;
		}
		icmp->icmp_src = ire->ire_src_addr;
		/* Now, finish initializing the IP header. */
		ASSERT(icmp->icmp_hdr_mp);
		ipha = (ipha_t *)icmp->icmp_hdr_mp->b_rptr;
		ipha->ipha_src = icmp->icmp_src;

		/* Acknowledge the T_CONN_REQ. */
		if ((mp = mi_tpi_ack_alloc(mp, sizeof (struct T_ok_ack),
		    T_OK_ACK)) != NULL) {
			((struct T_ok_ack *)ALIGN32(mp->b_rptr))->CORRECT_prim =
			    T_CONN_REQ;
			putnext(q, mp);
		}
		/*
		 * We also have to send a connection confirmation to
		 * keep TLI happy.
		 */
		bzero((char *)&ipaddr, sizeof (ipaddr));
		ipaddr.ip_family = icmp->icmp_family;

		bcopy((char *)&ipha->ipha_dst,
		    (char *)ipaddr.ip_addr,
		    sizeof (ipaddr.ip_addr));

		if ((mp = mi_tpi_conn_con(NULL, (char *)&ipaddr,
		    sizeof (ipaddr), nilp(char), 0)) != NULL)
			putnext(q, mp);
		return;
	}
	default:
		putnext(q, mp);
		return;
	}

	/* Handle T_UNITDATA_IND messages */
	if (icmp_bsd_compat) {
		ushort len;
#define	ipha ((ipha_t *)ALIGN32(rptr))
		int ip_hdr_len = IPH_HDR_LENGTH((ipha_t *)rptr);
		len = BE16_TO_U16(&ipha->ipha_length);

		if (mp->b_datap->db_ref > 1) {
			/*
			 * Allocate a new IP header so that we can modify
			 * ipha_length.
			 */
			mblk_t	*mp1;

			mp1 = allocb(ip_hdr_len, BPRI_MED);
			if (!mp1) {
				freemsg(mp);
				return;
			}
			bcopy((char *)rptr, (char *)mp1->b_rptr, ip_hdr_len);
			mp->b_rptr = rptr + ip_hdr_len;
			rptr = mp1->b_rptr;
			mp1->b_cont = mp;
			mp1->b_wptr = rptr + ip_hdr_len;
			mp_orig = mp = mp1;
		}
		len -= ip_hdr_len;
		ipha->ipha_length = BE16_TO_U16(&len);
#undef ipha
	}

	/*
	 * This is the inbound data path.  Packets are passed upstream as
	 * T_UNITDATA_IND messages with full IP headers still attached.
	 */
	/* Allocate a T_UNITDATA_IND message block. */
	mp = allocb((sizeof (ipa_t) + sizeof (struct T_unitdata_ind)),
	    BPRI_MED);
	if (!mp) {
		freemsg(mp_orig);
		return;
	}
	mp->b_cont = mp_orig;
	tudi = (struct T_unitdata_ind *)ALIGN32(mp->b_rptr);
	mp->b_datap->db_type = M_PROTO;
	mp->b_wptr = (u_char *)tudi +
	    (sizeof (ipa_t) +
		sizeof (struct T_unitdata_ind));
	tudi->PRIM_type = T_UNITDATA_IND;
	tudi->SRC_length = sizeof (ipa_t);
	tudi->SRC_offset = sizeof (struct T_unitdata_ind);
#define	ipa	((ipa_t *)ALIGN32(&tudi[1]))
	*ipa = ipa_null;
	ipa->ip_family = AF_INET;
	/* First half of source addr */
	*(u16 *)ALIGN16(&ipa->ip_addr[0]) = ((u16 *)ALIGN16(rptr))[6];
	/* Second half of source addr */
	*(u16 *)ALIGN16(&ipa->ip_addr[2]) = ((u16 *)ALIGN16(rptr))[7];

	tudi->OPT_offset = 0;	/* No options on inbound packets. */
	tudi->OPT_length = 0;
#undef	ipa
	putnext(q, mp);
}

/* Report for ndd "icmp_status" */
/* ARGSUSED */
static	int
icmp_status_report(q, mp, cp)
	queue_t	* q;
	mblk_t	* mp;
	caddr_t	cp;
{
	IDP	idp;
	icmp_t	* icmp;
	char *	state;
	u32	addr;

	mi_mpprintf(mp,
	    "RAWIP      src addr        dest addr       state");
	/*   01234567 xxx.xxx.xxx.xxx xxx.xxx.xxx.xxx UNBOUND */


	for (idp = mi_first_ptr(&icmp_g_head);
	    (icmp = (icmp_t *)ALIGN32(idp)) != 0;
	    idp = mi_next_ptr(&icmp_g_head, idp)) {
		if (icmp->icmp_state == TS_UNBND)
			state = "UNBOUND";
		else if (icmp->icmp_state == TS_IDLE)
			state = "IDLE";
		else if (icmp->icmp_state == TS_DATA_XFER)
			state = "CONNECTED";
		else
			state = "UnkState";

		addr = 0;
		if (icmp->icmp_hdr_mp) {
			ipha_t *ipha = (ipha_t *)icmp->icmp_hdr_mp->b_rptr;

			addr = ipha->ipha_dst;
		}
		mi_mpprintf(mp,
		    "%08x %03d.%03d.%03d.%03d %03d.%03d.%03d.%03d %s",
		    icmp,
		    (icmp->icmp_src >> 24) & 0xff,
		    (icmp->icmp_src >> 16) & 0xff,
		    (icmp->icmp_src >> 8) & 0xff,
		    (icmp->icmp_src >> 0) & 0xff,
		    (addr >> 24) & 0xff,
		    (addr >> 16) & 0xff,
		    (addr >> 8) & 0xff,
		    (addr >> 0) & 0xff,
		    state);
	}
	return (0);
}

/*
 * This routine creates a T_UDERROR_IND message and passes it upstream.
 * The address and options are copied from the T_UNITDATA_REQ message
 * passed in mp.  This message is freed.
 */
static void
icmp_ud_err(q, mp, err)
	queue_t	* q;
	mblk_t	* mp;
	int	err;
{
	mblk_t	* mp1;
	u_char	* rptr = mp->b_rptr;
	struct T_unitdata_req *tudr = (struct T_unitdata_req *)
	    ALIGN32(rptr);

	mp1 = mi_tpi_uderror_ind((char *)&rptr[tudr->DEST_offset],
	    tudr->DEST_length, (char *)&rptr[tudr->OPT_offset],
	    tudr->OPT_length, err);
	if (mp1)
		qreply(q, mp1);
	freemsg(mp);
}

/*
 * This routine is called by icmp_wput to handle T_UNBIND_REQ messages.
 * After some error checking, the message is passed downstream to ip.
 */
static void
icmp_unbind(q, mp)
	queue_t	* q;
	mblk_t	* mp;
{
	icmp_t	* icmp = (icmp_t *)q->q_ptr;

	/* If a bind has not been done, we can't unbind. */
	if (icmp->icmp_state == TS_UNBND) {
		icmp_err_ack(q, mp, TOUTSTATE, 0);
		return;
	}
	icmp->icmp_state = TS_UNBND;

	/* Pass the unbind to IP. */
	putnext(q, mp);
}

static void
icmp_wput_hdrincl(q, mp, icmp)
	queue_t	* q;
	mblk_t	* mp;
	icmp_t	* icmp;
{
	ipha_t	* ipha;
	int	ip_hdr_length;
	int	tp_hdr_len;
	mblk_t	* mp1;

	ipha = (ipha_t *)mp->b_rptr;
	ip_hdr_length = IP_SIMPLE_HDR_LENGTH + icmp->icmp_ip_snd_options_len;
	ipha->ipha_version_and_hdr_length =
	    (IP_VERSION<<4) | (ip_hdr_length>>2);

	/*
	 * For the socket of SOCK_RAW type, the checksum is provided in the
	 * pre-built packet. We set the ipha_ident field to NO_IP_TP_CKSUM to
	 * tell IP not to compute the transport checksum for the packet.
	 */
	ipha->ipha_ident = NO_IP_TP_CKSUM;
	ipha->ipha_hdr_checksum = 0;
	ipha->ipha_fragment_offset_and_flags = 0;
	/* Insert options if any */
	if (ip_hdr_length > IP_SIMPLE_HDR_LENGTH) {
		/*
		 * Put the IP header plus any transport header that is
		 * checksumed by ip_wput into the first mblk. (ip_wput assumes
		 * that at least the checksum field is in the first mblk.)
		 */
		switch (ipha->ipha_protocol) {
		case IPPROTO_UDP:
			tp_hdr_len = 8;
			break;
		case IPPROTO_TCP:
			tp_hdr_len = 20;
			break;
		default:
			tp_hdr_len = 0;
			break;
		}
		if (!(mp1 = allocb(ip_hdr_length + icmp_wroff_extra +
		    tp_hdr_len, BPRI_LO))) {
			icmp_ud_err(q, mp, TSYSERR);
			return;
		}
		mp1->b_rptr += icmp_wroff_extra;
		mp1->b_wptr = mp1->b_rptr + ip_hdr_length;
		ipha->ipha_length += icmp->icmp_ip_snd_options_len;
		bcopy((char *)ipha, (char *)mp1->b_rptr, IP_SIMPLE_HDR_LENGTH);

		/* Copy transport header if any */
		bcopy((char *)&ipha[1], (char *)mp1->b_wptr, tp_hdr_len);
		mp1->b_wptr += tp_hdr_len;

		/* Add options */
		ipha = (ipha_t *)mp1->b_rptr;
		bcopy((char *)icmp->icmp_ip_snd_options, (char *)&ipha[1],
		    icmp->icmp_ip_snd_options_len);

		/* Drop IP header and transport header from original */
		adjmsg(mp, IP_SIMPLE_HDR_LENGTH + tp_hdr_len);

		mp1->b_cont = mp;
		mp = mp1;
		/*
		 * Massage source route putting first source
		 * route in ipha_dst.
		 */
		(void) ip_massage_options(ipha);
	}
	putnext(q, mp);
}

/*
 * This routine handles all messages passed downstream.  It either
 * consumes the message or passes it downstream; it never queues a
 * a message.
 */
static void
icmp_wput(q, mp)
	queue_t	* q;
	mblk_t	* mp;
{
	u_char	* rptr = mp->b_rptr;
	ipha_t	* ipha;
	mblk_t	* mp1;
	int	ip_hdr_length;
#define	tudr ((struct T_unitdata_req *)ALIGN32(rptr))
	u32	u1;
	icmp_t	* icmp;

	icmp = (icmp_t *)q->q_ptr;
	switch (mp->b_datap->db_type) {
	case M_DATA:
		if (icmp->icmp_hdrincl) {
			icmp_wput_hdrincl(q, mp, icmp);
			return;
		}
		/* Prepend the "connected" header */
		/* TODO? append icmp_ip_snd_options? */
		ip_hdr_length = icmp->icmp_hdr_length +
		    icmp->icmp_ip_snd_options_len;
		mp1 = icmp->icmp_hdr_mp;
		if (!mp1) {
			/* Not connected */
			freemsg(mp);
			return;
		}
		if ((rptr - mp->b_datap->db_base) < ip_hdr_length ||
		    mp->b_datap->db_ref != 1) {
			/*
			 * If there is not enough room for the header
			 * in the existing M_DATA block, then copy the
			 * default header into a new block.
			 */
			if (!(mp1 = copyb(mp1))) {
				freemsg(mp);
				return;
			}
			mp1->b_cont = mp;
			mp = mp1;
			rptr = mp->b_rptr;
			u1 = msgdsize(mp);
		} else {
			/*
			 * If there is enough room remaining in the
			 * M_DATA block for the header, just copy it in.
			 */
			rptr -= ip_hdr_length;
			mp->b_rptr = rptr;
			bcopy((char *)mp1->b_rptr, (char *)rptr, ip_hdr_length);
			u1 = mp->b_cont ? msgdsize(mp) :
			    mp->b_wptr - rptr;
		}
		/* Set the length into the IP header. */
		ipha = (ipha_t *)rptr;
		ipha->ipha_length = htons(u1);
		putnext(q, mp);
		return;
	case M_PROTO:
	case M_PCPROTO:
		u1 = mp->b_wptr - rptr;
		if (u1 >= sizeof (struct T_unitdata_req) + sizeof (ipa_t)) {
			/* Expedite valid T_UNITDATA_REQ to below the switch */
			if (((union T_primitives *)ALIGN32(rptr))->type
			    == T_UNITDATA_REQ)
				break;
		}
		/* FALLTHRU */
	default:
		icmp_wput_other(q, mp);
		return;
	}

	/* Handle T_UNITDATA_REQ messages here. */

	if (icmp->icmp_state == TS_UNBND) {
		/* If a port has not been bound to the stream, fail. */
		icmp_ud_err(q, mp, TOUTSTATE);
		return;
	}
	if (tudr->DEST_length != sizeof (ipa_t) || !(mp1 = mp->b_cont)) {
		icmp_ud_err(q, mp, TBADADDR);
		return;
	}

	/*
	 * If options passed in, feed it for verification and handling
	 */
	if (tudr->OPT_length != 0) {
		int t_error;
		if (icmp_unitdata_opt_process(q, mp, &t_error) < 0) {
			/* failure */
			icmp_ud_err(q, mp, t_error);
			return;
		}
		/*
		 * Note: Success in processing options.
		 * mp option buffer represented by
		 * OPT_length/offset now potentially modified
		 * and contain option setting results
		 */
	}

	/* Protocol 255 contains full IP headers */
	if (icmp->icmp_hdrincl) {
		freeb(mp);
		icmp_wput_hdrincl(q, mp1, icmp);
		return;
	}
	/* Ignore options in the unitdata_req */
	/* If the user did not pass along an IP header, create one. */
	ip_hdr_length = IP_SIMPLE_HDR_LENGTH + icmp->icmp_ip_snd_options_len;
	ipha = (ipha_t *)&mp1->b_rptr[-ip_hdr_length];
	if ((u_char *)ipha < mp1->b_datap->db_base ||
	    mp1->b_datap->db_ref != 1) {
		if (!(mp1 = allocb(ip_hdr_length + icmp_wroff_extra,
		    BPRI_LO))) {
			icmp_ud_err(q, mp, TSYSERR);
			return;
		}
		mp1->b_cont = mp->b_cont;
		ipha = (ipha_t *)mp1->b_datap->db_lim;
		mp1->b_wptr = (u_char *)ipha;
		ipha = (ipha_t *)((u_char *)ipha - ip_hdr_length);
	}
#ifdef	_BIG_ENDIAN
	/* Set version, header length, and tos */
	*(u16 *)&ipha->ipha_version_and_hdr_length =
	    ((((IP_VERSION << 4) | (ip_hdr_length>>2)) << 8) |
		icmp->icmp_type_of_service);
	/* Set ttl and protocol */
	*(u16 *)&ipha->ipha_ttl = (icmp->icmp_ttl << 8) | icmp->icmp_proto;
#else
	/* Set version, header length, and tos */
	*(u16 *)&ipha->ipha_version_and_hdr_length =
	    ((icmp->icmp_type_of_service << 8) |
		((IP_VERSION << 4) | (ip_hdr_length>>2)));
	/* Set ttl and protocol */
	*(u16 *)&ipha->ipha_ttl = (icmp->icmp_proto << 8) | icmp->icmp_ttl;
#endif
	/*
	 * Copy our address into the packet.  If this is zero,
	 * ip will fill in the real source address.
	 */
	ipha->ipha_src = icmp->icmp_src;
	ipha->ipha_fragment_offset_and_flags = 0;

	/*
	 * For the socket of SOCK_RAW type, the checksum is provided in the
	 * pre-built packet. We set the ipha_ident field to NO_IP_TP_CKSUM to
	 * tell IP not to compute the transport checksum for the packet.
	 */
	ipha->ipha_ident = NO_IP_TP_CKSUM;

	/* Finish common formatting of the packet. */
	mp1->b_rptr = (u_char *)ipha;
	rptr = &rptr[tudr->DEST_offset];
	u1 = mp1->b_cont ? msgdsize(mp1) : mp1->b_wptr - (u_char *)ipha;
	ipha->ipha_length = htons(u1);
	/*
	 * Copy in the destination address from the T_UNITDATA
	 * request
	 */
	if (!OK_32PTR(rptr)) {
		/*
		 * Copy the long way if rptr is not aligned for long
		 * word access.
		 */
#define	ipa	((ipa_t *)ALIGN32(rptr))
		bcopy((char *)ipa->ip_addr, (char *)&ipha->ipha_dst,
		    IP_ADDR_LEN);
		if (ipha->ipha_dst == INADDR_ANY)
			ipha->ipha_dst = htonl(INADDR_LOOPBACK);
#undef ipa
	} else {
#define	ipa	((ipa_t *)ALIGN32(rptr))
		ipha->ipha_dst = *(u32 *)ALIGN32(ipa->ip_addr);
		if (ipha->ipha_dst == INADDR_ANY)
			ipha->ipha_dst = htonl(INADDR_LOOPBACK);
#undef ipa
	}
	/* Copy in options if any */
	if (ip_hdr_length > IP_SIMPLE_HDR_LENGTH) {
		bcopy((char *)icmp->icmp_ip_snd_options,
		    (char *)&ipha[1], icmp->icmp_ip_snd_options_len);
		/*
		 * Massage source route putting first source route in ipha_dst.
		 * Ignore the destination in dl_unitdata_req.
		 */
		(void) ip_massage_options(ipha);
	}
	freeb(mp);
	putnext(q, mp1);
#undef	ipha
#undef tudr
}

static void
icmp_wput_other(q, mp)
	queue_t	* q;
	mblk_t	* mp;
{
	u_char	* rptr = mp->b_rptr;
	struct iocblk * iocp;
	mblk_t	* mp1;
	auto	mblk_t	* mp2;
#define	tudr ((struct T_unitdata_req *)ALIGN32(rptr))
	u32	u1;
	icmp_t	* icmp;

	icmp = (icmp_t *)q->q_ptr;
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
		case T_ADDR_REQ:
			icmp_addr_req(q, mp);
			return;
		case O_T_BIND_REQ:
		case T_BIND_REQ:
			become_writer(q, mp, (pfi_t)icmp_bind);
			return;
		case T_CONN_REQ:
			icmp_connect(q, mp);
			return;
		case T_INFO_REQ:
			icmp_info_req(q, mp);
			return;
		case T_UNITDATA_REQ:
			/*
			 * If a T_UNITDATA_REQ gets here, the address must
			 * be bad.  Valid T_UNITDATA_REQs are found above
			 * and break to below this switch.
			 */
			icmp_ud_err(q, mp, TBADADDR);
			return;
		case T_UNBIND_REQ:
			icmp_unbind(q, mp);
			return;

		case O_T_OPTMGMT_REQ:
			svr4_optcom_req(q, mp, icmp->icmp_priv_stream,
			    &icmp_opt_obj);
			return;

		case T_OPTMGMT_REQ:
			tpi_optcom_req(q, mp, icmp->icmp_priv_stream,
			    &icmp_opt_obj);
			return;

		/* The following 2 TPI requests are not supported by icmp. */
		case T_CONN_RES:
		case T_DISCON_REQ:
			icmp_err_ack(q, mp, TNOTSUPPORT, 0);
			return;

		/* The following 3 TPI requests are illegal for icmp. */
		case T_DATA_REQ:
		case T_EXDATA_REQ:
		case T_ORDREL_REQ:
			freemsg(mp);
			putctl1(RD(q), M_ERROR, EPROTO);
			return;
		default:
			break;
		}
		break;
	case M_FLUSH:
		if (*rptr & FLUSHW)
			flushq(q, FLUSHDATA);
		break;
	case M_IOCTL:
		iocp = (struct iocblk *)ALIGN32(mp->b_rptr);
		switch (iocp->ioc_cmd) {
		case TI_GETPEERNAME:
			mp1 = icmp->icmp_hdr_mp;
			if (!mp1) {
				/*
				 * If a default destination address has not
				 * been associated with the stream, then we
				 * don't know the peer's name.
				 */
				iocp->ioc_error = ENOTCONN;
			    err_ret:;
				iocp->ioc_count = 0;
				mp->b_datap->db_type = M_IOCACK;
				qreply(q, mp);
				return;
			}
			/* FALLTHRU */
		case TI_GETMYNAME:
			/*
			 * For TI_GETPEERNAME and TI_GETMYNAME, we first
			 * need to copyin the user's strbuf structure.
			 * Processing will continue in the M_IOCDATA case
			 * below.
			 */
			mi_copyin(q, mp, nilp(char), sizeof (struct strbuf));
			return;
		case ND_SET:
			if (!icmp->icmp_priv_stream) {
				iocp->ioc_error = EPERM;
				goto err_ret;
			}
			/* FALLTHRU */
		case ND_GET:
			if (nd_getset(q, icmp_g_nd, mp)) {
				qreply(q, mp);
				return;
			}
			break;
		default:
			break;
		}
		break;
	case M_IOCDATA:
		/* Make sure it is for us. */
		switch (((struct iocblk *)ALIGN32(mp->b_rptr))->ioc_cmd) {
		case TI_GETMYNAME:
		case TI_GETPEERNAME:
			break;
		default:
			putnext(q, mp);
			return;
		}
		switch (mi_copy_state(q, mp, &mp2)) {
		case -1:
			return;
		case MI_COPY_CASE(MI_COPY_IN, 1): {
			/*
			 * Now we have the strbuf structure for TI_GETMYNAME
			 * and TI_GETPEERNAME.  Next we copyout the requested
			 * address and then we'll copyout the strbuf.
			 */
			ipa_t	* ipaddr;
			ipha_t	* ipha;
			struct strbuf * sb = (struct strbuf *)
			    ALIGN32(mp2->b_rptr);
			if (sb->maxlen < (int)sizeof (ipa_t)) {
				mi_copy_done(q, mp, EINVAL);
				return;
			}
			/*
			 * Create an mblk to hold the addresses for copying out.
			 */
			mp2 = mi_copyout_alloc(q, mp, sb->buf, sizeof (ipa_t));
			if (!mp2)
				return;
			ipaddr = (ipa_t *)ALIGN32(mp2->b_rptr);
			bzero((char *)ipaddr, sizeof (ipa_t));
			ipaddr->ip_family = AF_INET;
			switch (((struct iocblk *)
			    ALIGN32(mp->b_rptr))->ioc_cmd) {
			case TI_GETMYNAME:
				bcopy((char *)&icmp->icmp_src,
				    (char *)ipaddr->ip_addr,
				    sizeof (ipaddr->ip_addr));
				break;
			case TI_GETPEERNAME:
				ASSERT(icmp->icmp_hdr_mp);
				ipha = (ipha_t *)icmp->icmp_hdr_mp->b_rptr;
				bcopy((char *)&ipha->ipha_dst,
				    (char *)ipaddr->ip_addr,
				    sizeof (ipaddr->ip_addr));
				break;
			default:
				mi_copy_done(q, mp, EPROTO);
				return;
			}
			sb->len = (int)sizeof (ipa_t);
			mp2->b_wptr = mp2->b_rptr + sizeof (ipa_t);
			/* Copy out the address */
			mi_copyout(q, mp);
			break;
		}
		case MI_COPY_CASE(MI_COPY_OUT, 1):
			/*
			 * The address has been copied out, so now
			 * copyout the strbuf.
			 */
			mi_copyout(q, mp);
			break;
		case MI_COPY_CASE(MI_COPY_OUT, 2):
			/*
			 * The address and strbuf have been copied out.
			 * We're done, so just acknowledge the original
			 * M_IOCTL.
			 */
			mi_copy_done(q, mp, 0);
			break;
		default:
			/*
			 * Something strange has happened, so acknowledge
			 * the original M_IOCTL with an EPROTO error.
			 */
			mi_copy_done(q, mp, EPROTO);
			break;
		}
		return;
	default:
		break;
	}
	putnext(q, mp);
}

static int
icmp_unitdata_opt_process(q, mp, t_errorp)
	queue_t	* q;
	mblk_t	* mp;
	int *t_errorp;
{
	icmp_t	*icmp;
	int retval;
	struct T_unitdata_req *udreqp;

	icmp = (icmp_t *)q->q_ptr;

	udreqp = (struct T_unitdata_req *)ALIGN32(mp->b_rptr);
	*t_errorp = 0;

	/* XXX Remove cast when TPI does not have "long" */
	retval = tpi_optcom_buf(q, mp, (int32_t *)&udreqp->OPT_length,
	    udreqp->OPT_offset, icmp->icmp_priv_stream, &icmp_opt_obj);

	switch (retval) {
	case OB_SUCCESS:
		return (0);
	case OB_BADOPT:
		*t_errorp = TBADOPT;
		break;
	case OB_NOMEM:
		*t_errorp = TSYSERR;
		break;
	case OB_NOACCES:
		*t_errorp = TACCES;
		break;
	case OB_ABSREQ_FAIL:
		/*
		 * no suitable error in t_errno namespace, really
		 */
		*t_errorp = TSYSERR;
		break;
	case OB_INVAL:
		*t_errorp = TSYSERR;
		break;
	}
	return (-1);
}

void
icmp_ddi_init(void)
{
	icmp_max_optbuf_len =
	    optcom_max_optbuf_len(icmp_opt_obj.odb_opt_des_arr,
		icmp_opt_obj.odb_opt_arr_cnt);
}
