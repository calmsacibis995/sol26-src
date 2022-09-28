/*
 * Copyright (c) 1992-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)udp.c	1.52	96/10/28 SMI"

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

#include <sys/socket.h>
#include <sys/vtrace.h>
#include <sys/debug.h>
#include <sys/isa_defs.h>
#include <netinet/in.h>

#include <inet/common.h>
#include <inet/ip.h>
#include <inet/mi.h>
#include <inet/mib2.h>
#include <inet/nd.h>
#include <inet/optcom.h>
#include <inet/snmpcom.h>

#else

#include <types.h>
#include <stream.h>
#include <stropts.h>
#include <strlog.h>
#define	_SUN_TPI_VERSION 1
#include <tihdr.h>
#include <timod.h>

#include <socket.h>
#include <vtrace.h>
#include <debug.h>
#include <isa_defs.h>
#include <in.h>

#include <common.h>
#include <ip.h>
#include <mi.h>
#include <mib2.h>
#include <nd.h>
#include <optcom.h>
#include <snmpcom.h>

#endif

/*
 * Object to represent database of options to search passed to
 * {sock,tpi}optcom_req() interface routine to take care of option
 * management and associated methods.
 * XXX. These and other externs should really move to a udp header.
 */
extern optdb_obj_t	udp_opt_obj;
extern u_int		udp_max_optbuf_len;

/*
 * Synchronization notes:
 *
 * At all points in this code where exclusive, writer, access is required, we
 * pass a message to a subroutine by invoking "become_writer" which will
 * arrange to call the routine only after all reader threads have exited the
 * shared resource, and the writer lock has been acquired.  For uniprocessor,
 * single-thread, nonpreemptive environments, become_writer can simply be a
 * macro which invokes the routine immediately.
 */
#undef become_writer
#define	become_writer(q, mp, func) (*func)(q, mp)

/* UDP Protocol header */
typedef	struct udphdr_s {
	u8	uh_src_port[2];		/* Source port */
	u8	uh_dst_port[2];		/* Destination port */
	u8	uh_length[2];		/* UDP length */
	u8	uh_checksum[2];		/* UDP checksum */
} udph_t;
#define	UDPH_SIZE	8

/* UDP Protocol header aligned */
typedef	struct udpahdr_s {
	u16	uha_src_port;		/* Source port */
	u16	uha_dst_port;		/* Destination port */
	u16	uha_length;		/* UDP length */
	u16	uha_checksum;		/* UDP checksum */
} udpha_t;

/* Internal udp control structure, one per open stream */
typedef	struct ud_s {
	uint	udp_state;		/* TPI state */
	u8	udp_pad[2];
	u16	udp_port;		/* Port number bound to this stream */
	u32	udp_src;		/* Source address of this stream */
	u32	udp_bound_src;		/* Explicitely bound to address */
	uint	udp_hdr_length;		/* number of bytes used in udp_iphc */
	uint	udp_family;		/* Addr family used in bind, if any */
	uint	udp_ip_snd_options_len;	/* Length of IP options supplied. */
	u8	* udp_ip_snd_options;	/* Pointer to IP options supplied */
	uint	udp_ip_rcv_options_len;	/* Length of IP options supplied. */
	u8	* udp_ip_rcv_options;	/* Pointer to IP options supplied */
	union {
		u_char	udpu1_multicast_ttl;	/* IP_MULTICAST_TTL option */
		u_long	udpu1_pad;
	} udp_u1;
#define	udp_multicast_ttl	udp_u1.udpu1_multicast_ttl
	u32	udp_multicast_if_addr;	/* IP_MULTICAST_IF option */
	udpha_t	* udp_udpha;		/* Connected header */
	mblk_t	* udp_hdr_mp;		/* T_UNIDATA_IND for connected */
	uint	udp_priv_stream : 1,	/* Stream opened by privileged user */
		udp_debug : 1,		/* SO_DEBUG "socket" option. */
		udp_dontroute : 1,	/* SO_DONTROUTE "socket" option. */
		udp_broadcast : 1,	/* SO_BROADCAST "socket" option. */

		udp_useloopback : 1,	/* SO_USELOOPBACK "socket" option. */
		udp_reuseaddr : 1,	/* SO_REUSEADDR "socket" option. */
		udp_multicast_loop : 1,	/* IP_MULTICAST_LOOP option */
		udp_dgram_errind : 1,	/* SO_DGRAM_ERRIND option */

		udp_recvdstaddr : 1,	/* IP_RECVDSTADDR option */
		udp_recvopts : 1,	/* IP_RECVOPTS option */

		udp_pad_to_bit_31 : 22;
	union {
		char	udpu2_iphc[IP_MAX_HDR_LENGTH + UDPH_SIZE];
		ipha_t	udpu2_ipha;
		u32	udpu2_ipharr[7];
		double	udpu2_aligner;
	} udp_u2;
#define	udp_iphc	udp_u2.udpu2_iphc
#define	udp_ipha	udp_u2.udpu2_ipha
#define	udp_ipharr	udp_u2.udpu2_ipharr
	u8	udp_pad2[2];
	u8	udp_type_of_service;
	u8	udp_ttl;
} udp_t;

/* Named Dispatch Parameter Management Structure */
typedef struct udpparam_s {
	u_long	udp_param_min;
	u_long	udp_param_max;
	u_long	udp_param_value;
	char	* udp_param_name;
} udpparam_t;

static	void	udp_bind(queue_t * q, MBLKP mp);
static	int	udp_close(queue_t * q);
static	void	udp_connect(queue_t * q, MBLKP mp);
static void	udp_err_ack(queue_t * q, MBLKP mp, int t_error, int sys_error);
static	void	udp_info_req(queue_t * q, MBLKP mp);
static	void	udp_addr_req(queue_t * q, MBLKP mp);
static	int	udp_open(queue_t * q, dev_t * devp, int flag, int sflag,
    cred_t * credp);
static  int	udp_unitdata_opt_process(queue_t *q, mblk_t *mp,
	int *t_errorp);
static	boolean_t udp_allow_udropt_set(int level, int name);
int	udp_opt_default(queue_t * q, int level, int name, u_char * ptr);
int	udp_opt_get(queue_t * q, int level, int name, u_char * ptr);
int	udp_opt_set(queue_t * q, u_int mgmt_flags, int level, int name,
	u_int inlen, u_char *invalp, u_int *outlenp, u_char *outvalp);
static	void	udp_param_cleanup(void);
static int	udp_param_get(queue_t * q, mblk_t * mp, caddr_t cp);
static boolean_t	udp_param_register(udpparam_t * udppa, int cnt);
static int	udp_param_set(queue_t * q, mblk_t * mp, char * value,
    caddr_t cp);
static	void	udp_rput(queue_t * q, MBLKP mp);
static	void	udp_rput_other(queue_t * q, MBLKP mp);
static	int	udp_snmp_get(queue_t * q, mblk_t * mpctl);
static	int	udp_snmp_set(queue_t * q, int level, int name,
    u_char * ptr, int len);
static	int	udp_status_report(queue_t * q, mblk_t * mp, caddr_t cp);
static void	udp_ud_err(queue_t * q, MBLKP mp, int err);
static	void	udp_unbind(queue_t * q, MBLKP mp);
static	void	udp_wput(queue_t * q, MBLKP mp);
static	void	udp_wput_other(queue_t * q, MBLKP mp);

static struct module_info info =  {
	5607, "udp", 1, INFPSZ, 512, 128
};

static struct qinit rinit = {
	(pfi_t)udp_rput, nil(pfi_t), udp_open, udp_close, nil(pfi_t), &info
};

static struct qinit winit = {
	(pfi_t)udp_wput, nil(pfi_t), nil(pfi_t), nil(pfi_t), nil(pfi_t), &info
};

struct streamtab udpinfo = {
	&rinit, &winit
};

	int	udpdevflag = 0;

static	void	* udp_g_head;	/* Head for list of open udp streams. */
static	IDP	udp_g_nd;	/* Points to table of UDP ND variables. */
static	u16	udp_g_next_port_to_try;
kmutex_t	udp_g_lock;	/* Protects the above three variables */

/* MIB-2 stuff for SNMP */
static	mib2_udp_t	udp_mib;	/* SNMP fixed size info */


	/* Default structure copied into T_INFO_ACK messages */
static	struct T_info_ack udp_g_t_info_ack = {
	T_INFO_ACK,
	(64 * 1024) - (UDPH_SIZE + 20),	/* TSDU_size.  max ip less headers */
	T_INVALID,	/* ETSU_size.  udp does not support expedited data. */
	T_INVALID,	/* CDATA_size. udp does not support connect data. */
	T_INVALID,	/* DDATA_size. udp does not support disconnect data. */
	sizeof (ipa_t),	/* ADDR_size. */
	0,		/* OPT_size - not initialized here */
	(64 * 1024) - (UDPH_SIZE + 20),	/* TIDU_size.  max ip less headers */
	T_CLTS,		/* SERV_type.  udp supports connection-less. */
	TS_UNBND,	/* CURRENT_state.  This is set from udp_state. */
	(XPG4_1|SENDZERO) /* PROVIDER_flag */
};

/* largest UDP port number */
#define	UDP_MAX_PORT	65535

/*
 * Table of ND variables supported by udp.  These are loaded into udp_g_nd
 * in udp_open.
 * All of these are alterable, within the min/max values given, at run time.
 */
/* BEGIN CSTYLED */
static	udpparam_t	udp_param_arr[] = {
	/*min	max		value		name */
	{ 0L,	256,		32,		"udp_wroff_extra" },
	{ 1L,	255,		255,		"udp_def_ttl" },
	{ 1024,	(32 * 1024),	1024,		"udp_smallest_nonpriv_port" },
	{ 0,	1,		0,		"udp_trust_optlen" },
	{ 0,	1,		1,		"udp_do_checksum" },
	{ 1024,	UDP_MAX_PORT,	(32 * 1024),	"udp_smallest_anon_port" },
	{ 1024,	UDP_MAX_PORT,	UDP_MAX_PORT,	"udp_largest_anon_port" },
	{ 4096,	65536,		8192,		"udp_xmit_hiwat"},
	{ 0,	65536,		1024,		"udp_xmit_lowat"},
	{ 4096,	65536,		8192,		"udp_recv_hiwat"},
	{ 65536, 1024*1024*1024, 256*1024,	"udp_max_buf"},
};
/* END CSTYLED */
#define	udp_wroff_extra			udp_param_arr[0].udp_param_value
#define	udp_g_def_ttl			udp_param_arr[1].udp_param_value
#define	udp_smallest_nonpriv_port	udp_param_arr[2].udp_param_value
#define	udp_trust_optlen		udp_param_arr[3].udp_param_value
#define	udp_g_do_checksum		udp_param_arr[4].udp_param_value
#define	udp_smallest_anon_port		udp_param_arr[5].udp_param_value
#define	udp_largest_anon_port		udp_param_arr[6].udp_param_value
#define	udp_xmit_hiwat			udp_param_arr[7].udp_param_value
#define	udp_xmit_lowat			udp_param_arr[8].udp_param_value
#define	udp_recv_hiwat			udp_param_arr[9].udp_param_value
#define	udp_max_buf			udp_param_arr[10].udp_param_value

/*
 * This routine is called to handle each O_T_BIND_REQ/T_BIND_REQ message
 * passed to udp_wput.
 * It associates a port number and local address with the stream.
 * The O_T_BIND_REQ/T_BIND_REQ is passed downstream to ip with the UDP
 * protocol type (IPPROTO_UDP) placed in the message following the address.
 * A T_BIND_ACK message is passed upstream when ip acknowledges the request.
 * (Called as writer.)
 */
static void
udp_bind(q, mp)
	queue_t	*q;
	mblk_t	*mp;
{
	ipa_t	*ipa;
	mblk_t	*mp1;
	u16	port;		/* Host byte order */
	u16	requested_port;	/* Host byte order */
	struct	T_bind_req	* tbr;
	udp_t	*udp;
	int	count;
	u32	src;
	int	bind_to_req_port_only;

	udp = (udp_t *)q->q_ptr;
	if ((mp->b_wptr - mp->b_rptr) < sizeof (*tbr)) {
		mi_strlog(q, 1, SL_ERROR|SL_TRACE,
		    "udp_bind: bad req, len %d", mp->b_wptr - mp->b_rptr);
		udp_err_ack(q, mp, TBADADDR, 0);
		return;
	}
	if (udp->udp_state != TS_UNBND) {
		mi_strlog(q, 1, SL_ERROR|SL_TRACE,
		    "udp_bind: bad state, %d", udp->udp_state);
		udp_err_ack(q, mp, TOUTSTATE, 0);
		return;
	}
	/*
	 * Reallocate the message to make sure we have enough room for an
	 * address and the protocol type.
	 */
	mp1 = mi_reallocb(mp, sizeof (struct T_bind_ack) + sizeof (ipa_t) + 1);
	if (!mp1) {
		udp_err_ack(q, mp, TSYSERR, ENOMEM);
		return;
	}

	mp = mp1;
	tbr = (struct T_bind_req *)ALIGN32(mp->b_rptr);
	switch (tbr->ADDR_length) {
	case 0:			/* Request for a generic port */
		tbr->ADDR_offset = sizeof (struct T_bind_req);
		tbr->ADDR_length = sizeof (ipa_t);
		ipa = (ipa_t *)&tbr[1];
		bzero((char *)ipa, sizeof (ipa_t));
		ipa->ip_family = AF_INET;
		mp->b_wptr = (u_char *)&ipa[1];
		port = 0;
		break;
	case sizeof (ipa_t):	/* Complete IP address */
		ipa = (ipa_t *)ALIGN32(mi_offset_param(mp, tbr->ADDR_offset,
							sizeof (ipa_t)));
		if (!ipa) {
			udp_err_ack(q, mp, TSYSERR, EINVAL);
			return;
		}
		port = BE16_TO_U16(ipa->ip_port);
		break;
	default:		/* Invalid request */
		mi_strlog(q, 1, SL_ERROR|SL_TRACE,
		    "udp_bind: bad ADDR_length %d", tbr->ADDR_length);
		udp_err_ack(q, mp, TBADADDR, 0);
		return;
	}

	mutex_enter(&udp_g_lock);
	/* don't let next-port-to-try fall into the privileged range */
	if (udp_g_next_port_to_try < udp_smallest_nonpriv_port)
		udp_g_next_port_to_try = udp_smallest_anon_port;

	requested_port = port;

	if (requested_port == 0 || tbr->PRIM_type == O_T_BIND_REQ)
		bind_to_req_port_only = 0;
	else			/* T_BIND_REQ and requested_port != 0 */
		bind_to_req_port_only = 1;


	/*
	 * If the application passed in zero for the port number, it
	 * doesn't care which port number we bind to.
	 */
	if (port == 0)
		port = udp_g_next_port_to_try;
	/*
	 * If the port is in the well-known privileged range,
	 * make sure the stream was opened by superuser.
	 */
	if (port < udp_smallest_nonpriv_port &&
	    !udp->udp_priv_stream) {
		mutex_exit(&udp_g_lock);
		udp_err_ack(q, mp, TACCES, 0);
		return;
	}
	bcopy((char *)ipa->ip_addr, (char *)&src, IP_ADDR_LEN);

	/*
	 * If udp_reuseaddr is not set, then we have to make sure that
	 * the IP address and port number the application requested
	 * (or we selected for the application) is not being used by
	 * another stream.  If another stream is already using the
	 * requested IP address and port, then we search for any an
	 * unused port to bind to the the stream.
	 *
	 * As per the BSD semantics, if udp_reuseaddr is set, then we use
	 * the requested port, if no other stream is already bound to
	 * the same (IP src, src port).
	 */
	count = 0;
	for (;;) {
		udp_t	* udp1;
		u32	src1;

		/*
		 * Walk through the list of open udp streams looking
		 * for another stream bound to this IP address
		 * and port number.
		 */
		udp1 = (udp_t *)ALIGN32(mi_first_ptr(&udp_g_head));
		for (; udp1; udp1 = (udp_t *)ALIGN32(mi_next_ptr(&udp_g_head,
		    (IDP)udp1))) {
			if (udp1->udp_port == htons(port)) {
				src1 = udp1->udp_bound_src;
				if (!udp->udp_reuseaddr) {
					/*
					 * No socket option SO_REUSEADDR.
					 *
					 * If existing port is bound to a
					 * non-wildcard IP address and
					 * the requesting stream is bound to
					 * a distinct different IP addresses
					 * (non-wildcard, also), keep going.
					 */
					if (src != INADDR_ANY &&
					    src1 != INADDR_ANY && src1 != src)
						continue;
					break;
				} else {
					/*
					 * socket option SO_REUSEADDR is set.
					 *
					 * If two streams are bound to
					 * same IP address or both src
					 * and src1 are wildcards(INADDR_ANY),
					 * we want to stop searching.
					 * We have found a match of IP source
					 * address and source port, which is
					 * refused regardless of the
					 * SO_REUSEADDR setting, so we break.
					 */
					if (src == src1)
						break;
				}
			}
		}

		if (!udp1) {
			/*
			 * No other stream has this IP address
			 * and port number. We can use it.
			 */
			break;
		}

		if (bind_to_req_port_only) {
			/*
			 * We get here only when requested port
			 * is bound (and only first  of the for()
			 * loop iteration).
			 *
			 * The semantics of this bind request
			 * require it to fail so we return from
			 * the routine (and exit the loop).
			 *
			 */
			mutex_exit(&udp_g_lock);
			udp_err_ack(q, mp, TADDRBUSY, 0);
			return;
		}

		/*
		 * Our search for an unused port number is bounded
		 * on the bottom by udp_smallest_anon_port and
		 * on the top by udp_largest_anon_port.
		 */
		if ((count == 0) && (port != udp_g_next_port_to_try))
			port = udp_g_next_port_to_try;
		else
			port++;

		if ((port > udp_largest_anon_port) ||
		    (port < udp_smallest_nonpriv_port))
			port = udp_smallest_anon_port;

		if (++count >= (udp_largest_anon_port -
		    udp_smallest_anon_port + 1)) {
			/*
			 * We've tried every possible port number and
			 * there are none available, so send an error
			 * to the user.
			 */
			mutex_exit(&udp_g_lock);
			udp_err_ack(q, mp, TNOADDR, 0);
			return;
		}
	}

	/*
	 * Copy the source address into our udp structure.  This address
	 * may still be zero; if so, ip will fill in the correct address
	 * each time an outbound packet is passed to it.
	 * If we are binding to a broadcast or multicast address udp_rput
	 * will clear the source address when it receives the T_BIND_ACK.
	 */
	udp->udp_ipha.ipha_src = udp->udp_bound_src = udp->udp_src = src;
	udp->udp_port = htons(port);

	/*
	 * Now reset the the next anonymous port if the application requested
	 * an anonymous port, or we just handed out the next anonymous port.
	 */
	if ((requested_port == 0) ||
	    (port == udp_g_next_port_to_try)) {
		udp_g_next_port_to_try = port + 1;
		if (udp_g_next_port_to_try >=
		    udp_largest_anon_port)
			udp_g_next_port_to_try = udp_smallest_anon_port;
	}

	/* Initialize the O_T_BIND_REQ/T_BIND_REQ for ip. */
	bcopy((char *)&udp->udp_port, (char *)ipa->ip_port,
	    sizeof (udp->udp_port));
	udp->udp_family = ipa->ip_family;
	udp->udp_state = TS_IDLE;

	mutex_exit(&udp_g_lock);
	/* Pass the protocol number in the message following the address. */
	*mp->b_wptr++ = IPPROTO_UDP;
	if (src != INADDR_ANY) {
		/*
		 * Append a request for an IRE if src not 0 (INADDR_ANY)
		 */
		mp->b_cont = allocb(sizeof (ire_t), BPRI_HI);
		if (!mp->b_cont) {
			udp_err_ack(q, mp, TSYSERR, ENOMEM);
			return;
		}
		mp->b_cont->b_wptr += sizeof (ire_t);
		mp->b_cont->b_datap->db_type = IRE_DB_REQ_TYPE;
	}
	putnext(q, mp);
}

/*
 * This routine handles each T_CONN_REQ message passed to udp.  It
 * associates a default destination address with the stream.
 * A default IP header is created and placed into udp_iphc.
 * This header is prepended to subsequent M_DATA messages.
 */
static void
udp_connect(q, mp)
	queue_t	* q;
	mblk_t	* mp;
{
	ipa_t	* ipa;
	ipha_t	* ipha;
	struct T_conn_req	* tcr;
	udp_t	* udp;
	udpha_t	* udpha;

	udp = (udp_t *)q->q_ptr;
	tcr = (struct T_conn_req *)ALIGN32(mp->b_rptr);

	/* Make sure the request contains an IP address. */
	if (tcr->DEST_length != sizeof (ipa_t) ||
	    (mp->b_wptr-mp->b_rptr <
		sizeof (struct T_conn_req)+sizeof (ipa_t))) {
		udp_err_ack(q, mp, TBADADDR, 0);
		return;
	}

	if (udp->udp_state == TS_DATA_XFER) {
		/* Already connected - clear out state */
		udp->udp_src = udp->udp_bound_src;
		udp->udp_state = TS_IDLE;
	}

	if (tcr->OPT_length != 0) {
		udp_err_ack(q, mp, TBADOPT, 0);
		return;
	}

	/*
	 * Create a default IP header with no IP options.
	 */
	ipha = &udp->udp_ipha;
#ifdef	_BIG_ENDIAN
	/* Set version, header length, and tos */
	*(u16 *)&ipha->ipha_version_and_hdr_length =
	    ((((IP_VERSION << 4) |
		(IP_SIMPLE_HDR_LENGTH>>2)) << 8) |
		udp->udp_type_of_service);
	/* Set ttl and protocol */
	*(u16 *)&ipha->ipha_ttl = (udp->udp_ttl << 8) | IPPROTO_UDP;
#else
	/* Set version, header length, and tos */
	*(u16 *)&ipha->ipha_version_and_hdr_length =
	    ((udp->udp_type_of_service << 8) |
		((IP_VERSION << 4) | (IP_SIMPLE_HDR_LENGTH>>2)));
	/* Set ttl and protocol */
	*(u16 *)&ipha->ipha_ttl = (IPPROTO_UDP << 8) | udp->udp_ttl;
#endif
	udp->udp_hdr_length = IP_SIMPLE_HDR_LENGTH + UDPH_SIZE;
	udp->udp_udpha =
		(udpha_t *)ALIGN32(&udp->udp_iphc[IP_SIMPLE_HDR_LENGTH]);

	/* Now, finish initializing the IP and UDP headers. */
	ipa = (ipa_t *)ALIGN32(&mp->b_rptr[tcr->DEST_offset]);
	ipha->ipha_fragment_offset_and_flags = 0;
	ipha->ipha_ident = 0;
	/*
	 * Copy the source address already bound to the stream.
	 * This may still be zero in which case ip will fill it in.
	 */
	ipha->ipha_src = udp->udp_src;

	/*
	 * Copy the destination address from the T_CONN_REQ message.
	 * Translate 0 to INADDR_LOOPBACK.
	 */
	bcopy((char *)ipa->ip_addr, (char *)&ipha->ipha_dst, IP_ADDR_LEN);
	if (ipha->ipha_dst == INADDR_ANY)
		ipha->ipha_dst = htonl(INADDR_LOOPBACK);

	udpha = udp->udp_udpha;
	udpha->uha_src_port = udp->udp_port;
#define	udph	((udph_t *)ALIGN32(udpha))
	udph->uh_dst_port[0] = ipa->ip_port[0];
	udph->uh_dst_port[1] = ipa->ip_port[1];
#undef	udph
	udpha->uha_checksum = 0;

	tcr->PRIM_type = T_UNITDATA_REQ;
	if (udp->udp_hdr_mp != NULL)
		freemsg(udp->udp_hdr_mp);
	udp->udp_hdr_mp = mp;

	udp->udp_state = TS_DATA_XFER;
	if (udp->udp_src == INADDR_ANY) {
		/*
		 * Send down ire_lookup to IP to verify that there is a route
		 * and to determine the source address.
		 * This will come back as an IRE_DB_TYPE in rput.
		 */
		mblk_t *mp1;
		ire_t *ire;

		mp1 = allocb(sizeof (ire_t), BPRI_HI);
		if (mp1 == NULL) {
			udp->udp_hdr_mp = NULL;
			udp_err_ack(q, mp, TSYSERR, ENOMEM);
			return;
		}
		mp1->b_wptr += sizeof (ire_t);
		mp1->b_datap->db_type = IRE_DB_REQ_TYPE;

		ire = (ire_t *)ALIGN32(mp1->b_rptr);
		ire->ire_addr = ipha->ipha_dst;
		putnext(q, mp1);
	} else {
		/* Use ipa before freeing/reusing mp */
		mblk_t *mp1;

		mp1 = mi_tpi_conn_con(nil(MBLKP), (char *)ipa,
		    sizeof (ipa_t), nilp(char), 0);
		/* Acknowledge the request. */
		if ((mp = mi_tpi_ack_alloc(NULL, sizeof (struct T_ok_ack),
		    T_OK_ACK)) != NULL) {
			((struct T_ok_ack *)
			    ALIGN32(mp->b_rptr))->CORRECT_prim = T_CONN_REQ;
			qreply(q, mp);
		}
		/*
		 * We also have to send a connection confirmation to
		 * keep TLI happy
		 */
		if (mp1 != NULL)
			qreply(q, mp1);
	}
}

/* This is the close routine for udp.  It frees the per-stream data. */
static int
udp_close(q)
	queue_t	* q;
{
	int	i1;
	udp_t	* udp = (udp_t *)q->q_ptr;

	TRACE_1(TR_FAC_UDP, TR_UDP_CLOSE,
		"udp_close: q %X", q);

	qprocsoff(q);

	/* If there are any options associated with the stream, free them. */
	if (udp->udp_ip_snd_options)
		mi_free((char *)udp->udp_ip_snd_options);
	if (udp->udp_ip_rcv_options)
		mi_free((char *)udp->udp_ip_rcv_options);

	if (udp->udp_hdr_mp != NULL)
		freemsg(udp->udp_hdr_mp);

	mutex_enter(&udp_g_lock);
	/* Free the udp structure and release the minor device number. */
	i1 = mi_close_comm(&udp_g_head, q);

	/* Free the ND table if this was the last udp stream open. */
	udp_param_cleanup();
	mutex_exit(&udp_g_lock);

	return (i1);
}

/* This routine creates a T_ERROR_ACK message and passes it upstream. */
static void
udp_err_ack(q, mp, t_error, sys_error)
	queue_t	* q;
	mblk_t	* mp;
	int	t_error;
	int	sys_error;
{
	if ((mp = mi_tpi_err_ack_alloc(mp, t_error, sys_error)) != NULL)
		qreply(q, mp);
}

/*
 * udp_icmp_error is called by udp_rput to process ICMP
 * messages passed up by IP.
 * Generates the appropriate T_UDERROR_IND.
 */
static void
udp_icmp_error(q, mp)
	queue_t	* q;
	mblk_t	* mp;
{
	icmph_t * icmph;
	ipha_t	* ipha;
	int	iph_hdr_length;
	udpha_t	* udpha;
	ipa_t	ipaddr;
	mblk_t	* mp1;
	int	error = 0;
	udp_t	* udp = (udp_t *)q->q_ptr;

	ipha = (ipha_t *)mp->b_rptr;
	iph_hdr_length = IPH_HDR_LENGTH(ipha);
	icmph = (icmph_t *)ALIGN32(&mp->b_rptr[iph_hdr_length]);
	ipha = (ipha_t *)&icmph[1];
	iph_hdr_length = IPH_HDR_LENGTH(ipha);
	udpha = (udpha_t *)ALIGN32((char *)ipha + iph_hdr_length);

	switch (icmph->icmph_type) {
	case ICMP_DEST_UNREACHABLE:
		switch (icmph->icmph_code) {
		case ICMP_FRAGMENTATION_NEEDED:
			/*
			 * XXX do something with MTU in UDP?
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
	 * Can not deliver T_UDERROR_IND except when upper layer or
	 * the application has asked for them.
	 */
	if (!udp->udp_dgram_errind) {
		freemsg(mp);
		return;
	}

	bzero((char *)&ipaddr, sizeof (ipaddr));

	ipaddr.ip_family = AF_INET;
	bcopy((char *)&ipha->ipha_dst,
	    (char *)ipaddr.ip_addr,
	    sizeof (ipaddr.ip_addr));
	bcopy((char *)&udpha->uha_dst_port,
	    (char *)ipaddr.ip_port,
	    sizeof (ipaddr.ip_port));
	mp1 = mi_tpi_uderror_ind((char *)&ipaddr, sizeof (ipaddr), NULL, 0,
	    error);
	if (mp1)
		putnext(q, mp1);
	freemsg(mp);
}

/*
 * This routine responds to T_ADDR_REQ messages.  It is called by udp_wput.
 * The local address is filled in if endpoint is bound. The remote address
 * is always null. (The concept of connected CLTS sockets is alien to TPI
 * and we do not currently implement it with UDP.
 */
static void
udp_addr_req(q, mp)
	queue_t	* q;
	mblk_t	* mp;
{
	udp_t	* udp = (udp_t *)q->q_ptr;
	ipa_t	* ipa;
	mblk_t	* ackmp;
	struct T_addr_ack *taa;

	ackmp = mi_reallocb(mp, sizeof (struct T_addr_ack) + sizeof (ipa_t));
	if (! ackmp) {
		udp_err_ack(q, mp, TSYSERR, ENOMEM);
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
	if (udp->udp_state == TS_IDLE) {
		/*
		 * Fill in local address
		 */
		taa->LOCADDR_length = sizeof (ipa_t);
		taa->LOCADDR_offset = sizeof (*taa);

		ipa = (ipa_t *) &taa[1];
		/* Fill zeroes and then initialize non-zero fields */
		bzero((char *)ipa, sizeof (ipa_t));

		ipa->ip_family = AF_INET;

		bcopy((char *)&udp->udp_src, (char *)ipa->ip_addr,
		    sizeof (ipa->ip_addr));
		bcopy((char *)&udp->udp_port, (char *)ipa->ip_port,
		    sizeof (ipa->ip_port));

		ackmp->b_wptr = (u_char *) &ipa[1];
		ASSERT(ackmp->b_wptr <= ackmp->b_datap->db_lim);
	}
	qreply(q, ackmp);
}

/*
 * This routine responds to T_INFO_REQ messages.  It is called by udp_wput.
 * Most of the T_INFO_ACK information is copied from udp_g_t_info_ack.
 * The current state of the stream is copied from udp_state.
 */
static void
udp_info_req(q, mp)
	queue_t	* q;
	mblk_t	* mp;
{
	udp_t	* udp = (udp_t *)q->q_ptr;

	/* Create a T_INFO_ACK message. */
	mp = mi_tpi_ack_alloc(mp, sizeof (udp_g_t_info_ack), T_INFO_ACK);
	if (!mp)
		return;
	bcopy((char *)&udp_g_t_info_ack, (char *)mp->b_rptr,
		sizeof (udp_g_t_info_ack));
	((struct T_info_ack *)ALIGN32(mp->b_rptr))->CURRENT_state =
		udp->udp_state;
	((struct T_info_ack *)ALIGN32(mp->b_rptr))->OPT_size =
		udp_max_optbuf_len;
	qreply(q, mp);
}

/*
 * This is the open routine for udp.  It allocates a udp_t structure for
 * the stream and, on the first open of the module, creates an ND table.
 */
static int
udp_open(q, devp, flag, sflag, credp)
	queue_t	* q;
	dev_t	* devp;
	int	flag;
	int	sflag;
	cred_t	* credp;
{
	int	err;
	boolean_t	privy = drv_priv(credp) == 0;
	udp_t	* udp;

	TRACE_1(TR_FAC_UDP, TR_UDP_OPEN,
		"udp_open: q %X", q);

	/*
	 * Defer the qprocson until everything is initialized since
	 * we are D_MTPERQ and after qprocson the rput routine can
	 * run.
	 */

	/* If the stream is already open, return immediately. */
	if ((udp = (udp_t *)q->q_ptr) != 0) {
		if (udp->udp_priv_stream && !privy)
			return (EPERM);
		return (0);
	}

	/* If this is not a push of udp as a module, fail. */
	if (sflag != MODOPEN)
		return (EINVAL);

	mutex_enter(&udp_g_lock);
	/* If this is the first open of udp, create the ND table. */
	if (!udp_g_nd &&
	    !udp_param_register(udp_param_arr, A_CNT(udp_param_arr))) {
		mutex_exit(&udp_g_lock);
		return (ENOMEM);
	}
	/*
	 * Create a udp_t structure for this stream and link into the
	 * list of open streams.
	 */
	err = mi_open_comm(&udp_g_head, sizeof (udp_t), q, devp,
	    flag, sflag, credp);
	if (err) {
		/*
		 * If mi_open_comm failed and this is the first stream,
		 * release the ND table.
		 */
		udp_param_cleanup();
		mutex_exit(&udp_g_lock);
		return (err);
	}
	mutex_exit(&udp_g_lock);

	/* Set the initial state of the stream and the privilege status. */
	udp = (udp_t *)q->q_ptr;
	udp->udp_state = TS_UNBND;
	udp->udp_hdr_length = IP_SIMPLE_HDR_LENGTH + UDPH_SIZE;

	/*
	 * The receive hiwat is only looked at on the stream head queue.
	 * Store in q_hiwat in order to return on SO_RCVBUF getsockopts.
	 */
	q->q_hiwat = udp_recv_hiwat;

	udp->udp_multicast_ttl = IP_DEFAULT_MULTICAST_TTL;
	udp->udp_multicast_loop = IP_DEFAULT_MULTICAST_LOOP;
	udp->udp_ttl = udp_g_def_ttl;
	udp->udp_type_of_service = 0;	/* XXX should have a global default */
	if (privy)
		udp->udp_priv_stream = 1;

	qprocson(q);

	/*
	 * The transmit hiwat/lowat is only looked at on IP's queue.
	 * Store in q_hiwat in order to return on SO_SNDBUF
	 * getsockopts.
	 */
	WR(q)->q_hiwat = udp_xmit_hiwat;
	WR(q)->q_next->q_hiwat = WR(q)->q_hiwat;
	WR(q)->q_lowat = udp_xmit_lowat;
	WR(q)->q_next->q_lowat = WR(q)->q_lowat;

	mi_set_sth_wroff(q, udp->udp_hdr_length + udp->udp_ip_snd_options_len +
	    udp_wroff_extra);
	mi_set_sth_hiwat(q, q->q_hiwat);
	return (0);
}

/*
 * Which UDP options OK to set through T_UNITDATA_REQ...
 */

/* ARGSUSED */
static boolean_t
udp_allow_udropt_set(level, name)
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
udp_opt_default(q, level, name, ptr)
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
udp_opt_get(q, level, name, ptr)
	queue_t	*q;
	int	level;
	int	name;
	u_char	*ptr;
{
	int	* i1 = (int *)ALIGN32(ptr);
	udp_t	* udp = (udp_t *)q->q_ptr;

	switch (level) {
	case SOL_SOCKET:
		switch (name) {
		case SO_DEBUG:
			*i1 = udp->udp_debug;
			break;
		case SO_REUSEADDR:
			*i1 = udp->udp_reuseaddr;
			break;
		case SO_TYPE:
			*i1 = SOCK_DGRAM;
			break;

		/*
		 * The following three items are available here,
		 * but are only meaningful to IP.
		 */
		case SO_DONTROUTE:
			*i1 = udp->udp_dontroute;
			break;
		case SO_USELOOPBACK:
			*i1 = udp->udp_useloopback;
			break;
		case SO_BROADCAST:
			*i1 = udp->udp_broadcast;
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
			*i1 = udp->udp_dgram_errind;
			break;
		default:
			return (-1);
		}
		break;
	case IPPROTO_IP:
		switch (name) {
		case IP_OPTIONS:
			if (udp->udp_ip_rcv_options_len)
				bcopy((char *)udp->udp_ip_rcv_options,
				    (char *)ptr,
				    udp->udp_ip_rcv_options_len);
			return (udp->udp_ip_rcv_options_len);
		case IP_TOS:
			*i1 = (int) udp->udp_type_of_service;
			break;
		case IP_TTL:
			*i1 = (int) udp->udp_ttl;
			break;
		case IP_MULTICAST_IF:
			/* 0 address if not set */
			bcopy((char *)&udp->udp_multicast_if_addr, (char *)ptr,
			    sizeof (udp->udp_multicast_if_addr));
			return (sizeof (udp->udp_multicast_if_addr));
		case IP_MULTICAST_TTL:
			bcopy((char *)&udp->udp_multicast_ttl, (char *)ptr,
			    sizeof (udp->udp_multicast_ttl));
			return (sizeof (udp->udp_multicast_ttl));
		case IP_MULTICAST_LOOP:
			*ptr = udp->udp_multicast_loop;
			return (sizeof (u8));
		case IP_RECVOPTS:
			*i1 = udp->udp_recvopts;
			break;
		case IP_RECVDSTADDR:
			*i1 = udp->udp_recvdstaddr;
			break;
		case IP_ADD_MEMBERSHIP:
		case IP_DROP_MEMBERSHIP:
			/* cannot "get" the value for these */
			return (-1);
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
udp_opt_set(q, mgmt_flags, level, name, inlen, invalp, outlenp, outvalp)
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
	udp_t	* udp = (udp_t *)q->q_ptr;
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
		if (! udp_allow_udropt_set(level, name)) {
			*outlenp = 0;
			return (EINVAL);
		}
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
				udp->udp_reuseaddr = *i1;
			break;	/* goto sizeof (int) option return */
		case SO_DEBUG:
			if (! checkonly)
				udp->udp_debug = *i1;
			break;	/* goto sizeof (int) option return */
		/*
		 * The following three items are available here,
		 * but are only meaningful to IP.
		 */
		case SO_DONTROUTE:
			if (! checkonly)
				udp->udp_dontroute = *i1;
			break;	/* goto sizeof (int) option return */
		case SO_USELOOPBACK:
			if (! checkonly)
				udp->udp_useloopback = *i1;
			break;	/* goto sizeof (int) option return */
		case SO_BROADCAST:
			if (! checkonly)
				udp->udp_broadcast = *i1;
			break;	/* goto sizeof (int) option return */
		/*
		 * The following four items can be manipulated,
		 * but changing them should do nothing.
		 */
		case SO_SNDBUF:
			if (*i1 > udp_max_buf) {
				*outlenp = 0;
				return (ENOBUFS);
			}
			if (! checkonly) {
				q->q_hiwat = *i1;
				q->q_next->q_hiwat = *i1;
			}
			break;	/* goto sizeof (int) option return */
		case SO_RCVBUF:
			if (*i1 > udp_max_buf) {
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
				udp->udp_dgram_errind = *i1;
			break;	/* goto sizeof (int) option return */
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
			if (udp->udp_ip_snd_options) {
				mi_free((char *)udp->udp_ip_snd_options);
				udp->udp_ip_snd_options_len = 0;
				udp->udp_ip_snd_options = NULL;
			}
			if (inlen) {
				udp->udp_ip_snd_options =
					(u_char *)mi_alloc(inlen, BPRI_HI);
				if (udp->udp_ip_snd_options) {
					bcopy((char *)invalp,
					    (char *)udp->udp_ip_snd_options,
					    inlen);
					udp->udp_ip_snd_options_len = inlen;
				}
			}
			mi_set_sth_wroff(RD(q), udp->udp_hdr_length +
			    udp->udp_ip_snd_options_len +
			    udp_wroff_extra);
			/* OK return - copy input buffer into output buffer */
			if (invalp != outvalp) {
				/* don't trust bcopy for identical src/dst */
				(void) bcopy((char *)invalp,
					(char *)outvalp, inlen);
			}
			*outlenp = inlen;
			return (0);
		case IP_TTL:
			if (! checkonly) {
				/*
				 * save ttl in udp state and connected
				 * ip header
				 */
				udp->udp_ttl = (u_char) *i1;
				udp->udp_ipha.ipha_ttl = (u_char) *i1;
			}
			break;	/* goto sizeof (int) option return */
		case IP_TOS:
			if (! checkonly) {
				/*
				 * save tos in udp state and connected ip
				 * header
				 */
				udp->udp_type_of_service = (u_char) *i1;
				udp->udp_ipha.ipha_type_of_service =
					(u_char) *i1;
			}
			break;	/* goto sizeof (int) option return */
		case IP_MULTICAST_IF: {
			/*
			 * TODO should check OPTMGMT reply and undo this if
			 * there is an error.
			 */
			struct in_addr *inap = (struct in_addr *) invalp;
			if (! checkonly) {
				udp->udp_multicast_if_addr = (u32) inap->s_addr;
			}
			/* struct copy */
			*(struct in_addr *) outvalp = *inap;
			*outlenp = sizeof (struct in_addr);
			return (0);
		}
		case IP_MULTICAST_TTL:
			if (! checkonly)
				udp->udp_multicast_ttl = *invalp;
			*outvalp = *invalp;
			*outlenp = sizeof (u_char);
			return (0);
		case IP_MULTICAST_LOOP:
			if (! checkonly)
				udp->udp_multicast_loop = *invalp;
			*outvalp = *invalp;
			*outlenp = sizeof (u_char);
			return (0);
		case IP_RECVOPTS:
			if (! checkonly)
				udp->udp_recvopts = *i1;
			break;	/* goto sizeof (int) option return */
		case IP_RECVDSTADDR:
			if (! checkonly)
				udp->udp_recvdstaddr = *i1;
			break;	/* goto sizeof (int) option return */
		case IP_ADD_MEMBERSHIP:
		case IP_DROP_MEMBERSHIP:
			/*
			 * "soft" error (negative)
			 * option not handled at this level
			 * Do not modify *outlenp.
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
 * It is called by udp_close and udp_open.
 */
static void
udp_param_cleanup()
{
	ASSERT(MUTEX_HELD(&udp_g_lock));
	if (!udp_g_head)
		nd_free(&udp_g_nd);
}

/*
 * This routine retrieves the value of an ND variable in a udpparam_t
 * structure.  It is called through nd_getset when a user reads the
 * variable.
 */
/* ARGSUSED */
static int
udp_param_get(q, mp, cp)
	queue_t	* q;
	mblk_t	* mp;
	caddr_t	cp;
{
	udpparam_t	* udppa = (udpparam_t *)ALIGN32(cp);

	mi_mpprintf(mp, "%ld", udppa->udp_param_value);
	return (0);
}

/*
 * Walk through the param array specified registering each element with the
 * named dispatch (ND) handler.
 */
static boolean_t
udp_param_register(udppa, cnt)
	udpparam_t	* udppa;
	int	cnt;
{
	ASSERT(MUTEX_HELD(&udp_g_lock));
	for (; cnt-- > 0; udppa++) {
		if (udppa->udp_param_name && udppa->udp_param_name[0]) {
			if (!nd_load(&udp_g_nd, udppa->udp_param_name,
			    udp_param_get, udp_param_set,
			    (caddr_t)udppa)) {
				nd_free(&udp_g_nd);
				return (false);
			}
		}
	}
	if (!nd_load(&udp_g_nd, "udp_status", udp_status_report, nil(pfi_t),
	    nil(caddr_t))) {
		nd_free(&udp_g_nd);
		return (false);
	}
	return (true);
}

/* This routine sets an ND variable in a udpparam_t structure. */
/* ARGSUSED */
static int
udp_param_set(q, mp, value, cp)
	queue_t	* q;
	mblk_t	* mp;
	char	* value;
	caddr_t	cp;
{
	char	* end;
	long	new_value;
	udpparam_t	* udppa = (udpparam_t *)ALIGN32(cp);

	ASSERT(MUTEX_HELD(&udp_g_lock));
	/* Convert the value from a string into a long integer. */
	new_value = mi_strtol(value, &end, 10);
	/*
	 * Fail the request if the new value does not lie within the
	 * required bounds.
	 */
	if (end == value || new_value < udppa->udp_param_min ||
	    new_value > udppa->udp_param_max)
		return (EINVAL);

	/* Set the new value */
	udppa->udp_param_value = new_value;
	return (0);
}

static void
udp_rput(q, mp_orig)
	queue_t	* q;
	mblk_t	* mp_orig;
{
	struct T_unitdata_ind	* tudi;
	mblk_t	* mp = mp_orig;
	u_char	* rptr;
	int	hdr_length;
	int	udi_size;	/* Size of T_unitdata_ind */
	udp_t	* udp;

	TRACE_2(TR_FAC_UDP, TR_UDP_RPUT_START,
	    "udp_rput_start: q %X db_type 0%o", q, mp->b_datap->db_type);

	rptr = mp->b_rptr;
	switch (mp->b_datap->db_type) {
	case M_DATA:
		/*
		 * M_DATA messages contain IP datagrams.  They are handled
		 * after this switch.
		 */
		hdr_length = ((rptr[0] & 0xF) << 2) + UDPH_SIZE;
		udp = (udp_t *)q->q_ptr;
		if ((hdr_length > IP_SIMPLE_HDR_LENGTH + UDPH_SIZE) ||
		    (udp->udp_ip_rcv_options_len)) {
			become_exclusive(q, mp_orig, udp_rput_other);
			TRACE_2(TR_FAC_UDP, TR_UDP_RPUT_END,
				"udp_rput_end: q %X (%S)", q, "end");
			return;
		}
		break;
	case M_PROTO:
	case M_PCPROTO:
		/* M_PROTO messages contain some type of TPI message. */
		if ((mp->b_wptr - rptr) < sizeof (long)) {
			freemsg(mp);
			TRACE_2(TR_FAC_UDP, TR_UDP_RPUT_END,
				"udp_rput_end: q %X (%S)", q, "protoshort");
			return;
		}
		become_exclusive(q, mp_orig, udp_rput_other);
		TRACE_2(TR_FAC_UDP, TR_UDP_RPUT_END,
			"udp_rput_end: q %X (%S)", q, "proto");
		return;
	case IRE_DB_TYPE:
		become_exclusive(q, mp_orig, udp_rput_other);
		TRACE_2(TR_FAC_UDP, TR_UDP_RPUT_END,
			"udp_rput_end: q %X (%S)", q, "proto");
		return;
	case M_FLUSH:
		if (*mp->b_rptr & FLUSHR)
			flushq(q, FLUSHDATA);
		putnext(q, mp);
		TRACE_2(TR_FAC_UDP, TR_UDP_RPUT_END,
			"udp_rput_end: q %X (%S)", q, "flush");
		return;
	case M_CTL:
		/*
		 * ICMP messages.
		 */
		udp_icmp_error(q, mp);
		TRACE_2(TR_FAC_UDP, TR_UDP_RPUT_END,
			"udp_rput_end: q %X (%S)", q, "m_ctl");
		return;
	default:
		putnext(q, mp);
		TRACE_2(TR_FAC_UDP, TR_UDP_RPUT_END,
			"udp_rput_end: q %X (%S)", q, "default");
		return;
	}
	/*
	 * This is the inbound data path.
	 * First, we make sure the data contains both IP and UDP headers.
	 */
	if ((mp->b_wptr - rptr) < hdr_length) {
		if (!pullupmsg(mp, hdr_length)) {
			freemsg(mp_orig);
			BUMP_MIB(udp_mib.udpInErrors);
			TRACE_2(TR_FAC_UDP, TR_UDP_RPUT_END,
				"udp_rput_end: q %X (%S)", q, "hdrshort");
			return;
		}
		rptr = mp->b_rptr;
	}
	/* Walk past the headers. */
	mp->b_rptr = rptr + hdr_length;

	/*
	 * Normally only send up the address.
	 * If IP_RECVDSTADDR is set we include the destination IP address
	 * as an option. With IP_RECVOPTS we include all the IP options.
	 * Only ip_rput_other() handles packets that contain IP options.
	 */
	udi_size = sizeof (struct T_unitdata_ind) + sizeof (ipa_t);
	if (udp->udp_recvdstaddr)
		udi_size += sizeof (struct T_opthdr) + sizeof (struct in_addr);
	ASSERT(IPH_HDR_LENGTH((ipha_t *)rptr) == IP_SIMPLE_HDR_LENGTH);

	/* Allocate a message block for the T_UNITDATA_IND structure. */
	mp = allocb(udi_size, BPRI_MED);
	if (!mp) {
		freemsg(mp_orig);
		TRACE_2(TR_FAC_UDP, TR_UDP_RPUT_END,
			"udp_rput_end: q %X (%S)", q, "allocbfail");
		return;
	}
	mp->b_cont = mp_orig;
	mp->b_datap->db_type = M_PROTO;
	tudi = (struct T_unitdata_ind *)ALIGN32(mp->b_rptr);
	mp->b_wptr = (u_char *)tudi + udi_size;
	tudi->PRIM_type = T_UNITDATA_IND;
	tudi->SRC_length = sizeof (ipa_t);
	tudi->SRC_offset = sizeof (struct T_unitdata_ind);
	tudi->OPT_offset = sizeof (struct T_unitdata_ind) + sizeof (ipa_t);
	udi_size -= (sizeof (struct T_unitdata_ind) + sizeof (ipa_t));
	tudi->OPT_length = udi_size;
#define	ipa	((ipa_t *)&tudi[1])
	*(u32 *)ALIGN32(&ipa->ip_addr[0]) = (((u32 *)ALIGN32(rptr))[3]);
	*(u16 *)ALIGN16(ipa->ip_port) =		/* Source port */
		((u16 *)ALIGN16(mp->b_cont->b_rptr))[-UDPH_SIZE/sizeof (u16)];
	ipa->ip_family = ((udp_t *)q->q_ptr)->udp_family;
	*(u32 *)ALIGN32(&ipa->ip_pad[0]) = 0;
	*(u32 *)ALIGN32(&ipa->ip_pad[4]) = 0;

	/* Add options if IP_RECVDSTADDR has been set. */
	if (udi_size != 0) {
		/*
		 * Copy in destination address before options to avoid any
		 * padding issues.
		 */
		char *dstopt;

		dstopt = (char *)&ipa[1];
		if (udp->udp_recvdstaddr) {
			struct T_opthdr toh;
			u32 *dstptr;

			toh.level = IPPROTO_IP;
			toh.name = IP_RECVDSTADDR;
			toh.len = sizeof (struct T_opthdr) +
			    sizeof (struct in_addr);
			toh.status = 0;
			bcopy((char *)&toh, dstopt, sizeof (toh));
			dstopt += sizeof (toh);
			dstptr = (u32 *)dstopt;
			*dstptr = (((u32 *)ALIGN32(rptr))[4]);
			dstopt += sizeof (struct in_addr);
			udi_size -= toh.len;
		}
		ASSERT(udi_size == 0);	/* "Consumed" all of allocated space */
	}
#undef	ipa

	BUMP_MIB(udp_mib.udpInDatagrams);
	TRACE_2(TR_FAC_UDP, TR_UDP_RPUT_END,
		"udp_rput_end: q %X (%S)", q, "end");
	putnext(q, mp);
}

static void
udp_rput_other(q, mp_orig)
	queue_t	* q;
	mblk_t	* mp_orig;
{
	struct T_unitdata_ind	* tudi;
	mblk_t	* mp = mp_orig;
	u_char	* rptr;
	int	hdr_length;
	int	udi_size;	/* Size of T_unitdata_ind */
	int	opt_len;	/* Length of IP options */
	struct T_error_ack	* tea;
	udp_t	* udp;
	mblk_t *mp1;
	ire_t *ire;

	TRACE_2(TR_FAC_UDP, TR_UDP_RPUT_START,
	    "udp_rput_other: q %X db_type 0%o",	q, mp->b_datap->db_type);




	rptr = mp->b_rptr;

	switch (mp->b_datap->db_type) {
	case M_DATA:
		/*
		 * M_DATA messages contain IP datagrams.  They are handled
		 * after this switch.
		 */
		break;
	case M_PROTO:
	case M_PCPROTO:
		/* M_PROTO messages contain some type of TPI message. */
		if ((mp->b_wptr - rptr) < sizeof (long)) {
			freemsg(mp);
			TRACE_2(TR_FAC_UDP, TR_UDP_RPUT_END,
			    "udp_rput_other_end: q %X (%S)", q, "protoshort");
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
				 * clear out the associated port and source
				 * address before passing the message
				 * upstream.
				 */
				udp = (udp_t *)q->q_ptr;
				mutex_enter(&udp_g_lock);
				udp->udp_port = 0;
				udp->udp_src = 0;
				udp->udp_bound_src = 0;
				udp->udp_ipha.ipha_src = 0;
				udp->udp_state = TS_UNBND;
				mutex_exit(&udp_g_lock);
				break;
			default:
				break;
			}
			break;
		case T_BIND_ACK:
			udp = (udp_t *)q->q_ptr;
			/*
			 * If udp_src is not 0 (INADDR_ANY) already, we
			 * set it to 0 if broadcast address was bound.
			 * This ensures no datagrams with broadcast address
			 * as source address are emitted (which would violate
			 * RFC1122 - Hosts requirements)
			 */
			if (udp->udp_src == INADDR_ANY || mp->b_cont == NULL)
				break;

			mp1 = mp->b_cont;
			mp->b_cont = NULL;
			if (mp1->b_datap->db_type == IRE_DB_TYPE) {
				ire = (ire_t *) mp1->b_rptr;

				if (ire->ire_type == IRE_BROADCAST) {
					udp->udp_src = 0;
					udp->udp_ipha.ipha_src = 0;
				}
			}
			freemsg(mp1);
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
		udp = (udp_t *)q->q_ptr;
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
		udp->udp_src = ire->ire_src_addr;

		/* Now, finish initializing the IP and UDP headers. */
		ipha = &udp->udp_ipha;

		ipha->ipha_src = udp->udp_src;

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
		ipaddr.ip_family = udp->udp_family;

		bcopy((char *)&ipha->ipha_dst,
		    (char *)ipaddr.ip_addr,
		    sizeof (ipaddr.ip_addr));
		bcopy((char *)&udp->udp_udpha->uha_dst_port,
		    (char *)ipaddr.ip_port,
		    sizeof (ipaddr.ip_port));

		if ((mp = mi_tpi_conn_con(NULL, (char *)&ipaddr,
		    sizeof (ipaddr), nilp(char), 0)) != NULL)
			putnext(q, mp);
		return;
	}
	}

	/*
	 * This is the inbound data path.
	 * First, we make sure the data contains both IP and UDP headers.
	 */
	hdr_length = ((rptr[0] & 0xF) << 2) + UDPH_SIZE;
	udp = (udp_t *)q->q_ptr;
	if ((mp->b_wptr - rptr) < hdr_length) {
		if (!pullupmsg(mp, hdr_length)) {
			freemsg(mp_orig);
			BUMP_MIB(udp_mib.udpInErrors);
			TRACE_2(TR_FAC_UDP, TR_UDP_RPUT_END,
			    "udp_rput_other_end: q %X (%S)", q, "hdrshort");
			return;
		}
		rptr = mp->b_rptr;
	}
	/* Walk past the headers. */
	mp->b_rptr = rptr + hdr_length;

	/* Save the options if any */
	opt_len = hdr_length - (IP_SIMPLE_HDR_LENGTH + UDPH_SIZE);
	if (opt_len > 0) {
		if (opt_len > udp->udp_ip_rcv_options_len) {
			if (udp->udp_ip_rcv_options_len)
				mi_free((char *)udp->udp_ip_rcv_options);
			udp->udp_ip_rcv_options_len = 0;
			udp->udp_ip_rcv_options =
			    (u_char *)mi_alloc(opt_len, BPRI_HI);
			if (udp->udp_ip_rcv_options)
				udp->udp_ip_rcv_options_len = opt_len;
		}
		if (udp->udp_ip_rcv_options_len) {
			bcopy((char *)rptr + IP_SIMPLE_HDR_LENGTH,
			    (char *)udp->udp_ip_rcv_options,
			    opt_len);
			/* Adjust length if we are resusing the space */
			udp->udp_ip_rcv_options_len = opt_len;
		}
	} else if (udp->udp_ip_rcv_options_len) {
		mi_free((char *)udp->udp_ip_rcv_options);
		udp->udp_ip_rcv_options = nilp(u8);
		udp->udp_ip_rcv_options_len = 0;
	}

	/*
	 * Normally only send up the address.
	 * If IP_RECVDSTADDR is set we include the destination IP address
	 * as an option. With IP_RECVOPTS we include all the IP options.
	 */
	udi_size = sizeof (struct T_unitdata_ind) + sizeof (ipa_t);
	if (udp->udp_recvdstaddr)
		udi_size += sizeof (struct T_opthdr) + sizeof (struct in_addr);
	if (udp->udp_recvopts && opt_len > 0)
		udi_size += sizeof (struct T_opthdr) + opt_len;

	/* Allocate a message block for the T_UNITDATA_IND structure. */
	mp = allocb(udi_size, BPRI_MED);
	if (!mp) {
		freemsg(mp_orig);
		TRACE_2(TR_FAC_UDP, TR_UDP_RPUT_END,
			"udp_rput_other_end: q %X (%S)", q, "allocbfail");
		return;
	}
	mp->b_cont = mp_orig;
	mp->b_datap->db_type = M_PROTO;
	tudi = (struct T_unitdata_ind *)ALIGN32(mp->b_rptr);
	mp->b_wptr = (u_char *)tudi + udi_size;
	tudi->PRIM_type = T_UNITDATA_IND;
	tudi->SRC_length = sizeof (ipa_t);
	tudi->SRC_offset = sizeof (struct T_unitdata_ind);
	tudi->OPT_offset = sizeof (struct T_unitdata_ind) + sizeof (ipa_t);
	udi_size -= (sizeof (struct T_unitdata_ind) + sizeof (ipa_t));
	tudi->OPT_length = udi_size;
#define	ipa	((ipa_t *)&tudi[1])
					/* First half of source addr */
	*(u16 *)ALIGN16(&ipa->ip_addr[0]) = ((u16 *)ALIGN16(rptr))[6];
					/* Second half of source addr */
	*(u16 *)ALIGN16(&ipa->ip_addr[2]) = ((u16 *)ALIGN16(rptr))[7];
	*(u16 *)ALIGN16(ipa->ip_port) =		/* Source port */
		((u16 *)ALIGN16(mp->b_cont->b_rptr))[-UDPH_SIZE/sizeof (u16)];
	ipa->ip_family = ((udp_t *)q->q_ptr)->udp_family;
	*(u32 *)ALIGN32(&ipa->ip_pad[0]) = 0;
	*(u32 *)ALIGN32(&ipa->ip_pad[4]) = 0;

	/* Add options if IP_RECVOPTS or IP_RECVDSTADDR has been set. */
	if (udi_size != 0) {
		/*
		 * Copy in destination address before options to avoid any
		 * padding issues.
		 */
		char *dstopt;

		dstopt = (char *)&ipa[1];
		if (udp->udp_recvdstaddr) {
			struct T_opthdr toh;
			u32 *dstptr;

			toh.level = IPPROTO_IP;
			toh.name = IP_RECVDSTADDR;
			toh.len = sizeof (struct T_opthdr) +
			    sizeof (struct in_addr);
			toh.status = 0;
			bcopy((char *)&toh, dstopt, sizeof (toh));
			dstopt += sizeof (toh);
			dstptr = (u32 *)dstopt;
			*dstptr = (((u32 *)ALIGN32(rptr))[4]);
			dstopt += sizeof (struct in_addr);
			udi_size -= toh.len;
		}
		if (udp->udp_recvopts && udi_size != 0) {
			struct T_opthdr toh;

			toh.level = IPPROTO_IP;
			toh.name = IP_RECVOPTS;
			toh.len = sizeof (struct T_opthdr) + opt_len;
			toh.status = 0;
			bcopy((char *)&toh, dstopt, sizeof (toh));
			dstopt += sizeof (toh);
			bcopy((char *)rptr + IP_SIMPLE_HDR_LENGTH, dstopt,
				opt_len);
			dstopt += opt_len;
			udi_size -= toh.len;
		}
		ASSERT(udi_size == 0);	/* "Consumed" all of allocated space */
	}
#undef	ipa
	BUMP_MIB(udp_mib.udpInDatagrams);
	TRACE_2(TR_FAC_UDP, TR_UDP_RPUT_END,
	    "udp_rput_other_end: q %X (%S)", q, "end");
	putnext(q, mp);
}

/*
 * return SNMP stuff in buffer in mpdata
 */
static	int
udp_snmp_get(q, mpctl)
	queue_t	* q;
	mblk_t	* mpctl;
{
	mblk_t		* mpdata;
	mblk_t		* mp2ctl;
	struct opthdr	* optp;
	IDP		idp;
	udp_t		* udp;
	char		buf[sizeof (mib2_udpEntry_t)];
	mib2_udpEntry_t	* ude = (mib2_udpEntry_t *)ALIGN32(buf);

	if (!mpctl || !(mpdata = mpctl->b_cont) ||
	    !(mp2ctl = copymsg(mpctl)))
		return (0);

	optp = (struct opthdr *)
	    ALIGN32(&mpctl->b_rptr[sizeof (struct T_optmgmt_ack)]);
	optp->level = MIB2_UDP;
	optp->name = 0;
	snmp_append_data(mpdata, (char *)&udp_mib, sizeof (udp_mib));
	optp->len = msgdsize(mpdata);
	qreply(q, mpctl);

	mpctl = mp2ctl;
	mpdata = mp2ctl->b_cont;
	SET_MIB(udp_mib.udpEntrySize, sizeof (mib2_udpEntry_t));
	mutex_enter(&udp_g_lock);
	for (idp = mi_first_ptr(&udp_g_head);
	    (udp = (udp_t *)ALIGN32(idp)) != 0;
	    idp = mi_next_ptr(&udp_g_head, idp)) {
		bcopy((char *)&udp->udp_bound_src,
		    (char *)&ude->udpLocalAddress, sizeof (IpAddress));
		ude->udpLocalPort = ntohs(udp->udp_port);
		if (udp->udp_state == TS_UNBND)
			ude->udpEntryInfo.ue_state = MIB2_UDP_unbound;
		else if (udp->udp_state == TS_IDLE)
			ude->udpEntryInfo.ue_state = MIB2_UDP_idle;
		else
			ude->udpEntryInfo.ue_state = MIB2_UDP_unknown;
		snmp_append_data(mpdata, buf, sizeof (mib2_udpEntry_t));
	}
	mutex_exit(&udp_g_lock);
	optp = (struct opthdr *)
	    ALIGN32(&mpctl->b_rptr[sizeof (struct T_optmgmt_ack)]);
	optp->level = MIB2_UDP;
	optp->name = MIB2_UDP_5;
	optp->len = msgdsize(mpdata);
	qreply(q, mpctl);
	return (1);
}

/*
 * Return 0 if invalid set request, 1 otherwise, including non-udp requests.
 * NOTE: Per MIB-II, UDP has no writable data.
 * TODO:  If this ever actually tries to set anything, it needs to be
 * called via become_writer.
 */
/* ARGSUSED */
static	int
udp_snmp_set(q, level, name, ptr, len)
	queue_t	* q;
	int	level;
	int	name;
	u_char	* ptr;
	int	len;
{
	switch (level) {
	case MIB2_UDP:
		return (0);
	default:
		return (1);
	}
}

/* Report for ndd "udp_status" */
/* ARGSUSED */
static	int
udp_status_report(q, mp, cp)
	queue_t	* q;
	mblk_t	* mp;
	caddr_t	cp;
{
	IDP	idp;
	udp_t	* udp;
	udpha_t	* udpha;
	char *	state;
	uint	rport;
	u32	addr;

	mi_mpprintf(mp,
	    "UDP      lport src addr        dest addr       port  state");
	/*   01234567 12345 xxx.xxx.xxx.xxx xxx.xxx.xxx.xxx 12345 UNBOUND */


	ASSERT(MUTEX_HELD(&udp_g_lock));
	for (idp = mi_first_ptr(&udp_g_head);
	    (udp = (udp_t *)ALIGN32(idp)) != 0;
	    idp = mi_next_ptr(&udp_g_head, idp)) {
		if (udp->udp_state == TS_UNBND)
			state = "UNBOUND";
		else if (udp->udp_state == TS_IDLE)
			state = "IDLE";
		else if (udp->udp_state == TS_DATA_XFER)
			state = "CONNECTED";
		else
			state = "UnkState";
		addr = udp->udp_ipha.ipha_dst;
		rport = 0;
		if ((udpha = udp->udp_udpha) != NULL)
			rport = ntohs(udpha->uha_dst_port);
		mi_mpprintf(mp,
		    "%08x %05d %03d.%03d.%03d.%03d %03d.%03d.%03d.%03d "
		    "%05d %s",
		    udp, ntohs(udp->udp_port),
		    (udp->udp_bound_src >> 24) & 0xff,
		    (udp->udp_bound_src >> 16) & 0xff,
		    (udp->udp_bound_src >> 8) & 0xff,
		    (udp->udp_bound_src >> 0) & 0xff,
		    (addr >> 24) & 0xff,
		    (addr >> 16) & 0xff,
		    (addr >> 8) & 0xff,
		    (addr >> 0) & 0xff,
		    rport, state);
	}
	return (0);
}

/*
 * This routine creates a T_UDERROR_IND message and passes it upstream.
 * The address and options are copied from the T_UNITDATA_REQ message
 * passed in mp.  This message is freed.
 */
static void
udp_ud_err(q, mp, err)
	queue_t	* q;
	mblk_t	* mp;
	int	err;
{
	mblk_t	* mp1;
	char	* rptr = (char *)mp->b_rptr;
	struct T_unitdata_req	* tudr = (struct T_unitdata_req *)ALIGN32(rptr);

	mp1 = mi_tpi_uderror_ind(&rptr[tudr->DEST_offset],
			tudr->DEST_length, &rptr[tudr->OPT_offset],
			tudr->OPT_length, err);
	if (mp1)
		qreply(q, mp1);
	freemsg(mp);
}

/*
 * This routine removes a port number association from a stream.  It
 * is called by udp_wput to handle T_UNBIND_REQ messages.
 */
static void
udp_unbind(q, mp)
	queue_t	* q;
	mblk_t	* mp;
{
	udp_t	* udp;

	udp = (udp_t *)q->q_ptr;
	/* If a bind has not been done, we can't unbind. */
	if (udp->udp_state == TS_UNBND) {
		udp_err_ack(q, mp, TOUTSTATE, 0);
		return;
	}
	mutex_enter(&udp_g_lock);
	udp->udp_src = udp->udp_bound_src = 0;
	udp->udp_ipha.ipha_src = 0;
	udp->udp_port = 0;
	udp->udp_state = TS_UNBND;
	mutex_exit(&udp_g_lock);

	/* Pass the unbind to IP */
	putnext(q, mp);
}

/*
 * This routine handles all messages passed downstream.  It either
 * consumes the message or passes it downstream; it never queues a
 * a message.
 */
static void
udp_wput(q, mp)
	queue_t	* q;
	mblk_t	* mp;
{
	u_char	* rptr = mp->b_rptr;
	struct datab * db;
	ipha_t	* ipha;
	udpha_t	* udpha;
	mblk_t	* mp1;
	int	ip_hdr_length;
#define	tudr ((struct T_unitdata_req *)ALIGN32(rptr))
	u32	u1;
	udp_t	* udp;

	TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_START,
		"udp_wput_start: q %X db_type 0%o",
		q, mp->b_datap->db_type);

	db = mp->b_datap;
	switch (db->db_type) {
	case M_PROTO:
	case M_PCPROTO:
		u1 = mp->b_wptr - rptr;
		if (u1 >= sizeof (struct T_unitdata_req) + sizeof (ipa_t)) {
			/* Detect valid T_UNITDATA_REQ here */
			if (((union T_primitives *)ALIGN32(rptr))->type
			    == T_UNITDATA_REQ)
				break;
		}
		/* FALLTHRU */
	default:
		become_exclusive(q, mp, udp_wput_other);
		return;
	}

	udp = (udp_t *)q->q_ptr;

	/* Handle UNITDATA_REQ messages here */
	if (udp->udp_state == TS_UNBND) {
		/* If a port has not been bound to the stream, fail. */
		udp_ud_err(q, mp, TOUTSTATE);
		TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_END,
			"udp_wput_end: q %X (%S)", q, "outstate");
		return;
	}
	if (tudr->DEST_length != sizeof (ipa_t) ||
	    !(mp1 = mp->b_cont)) {
		udp_ud_err(q, mp, TBADADDR);
		TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_END,
			"udp_wput_end: q %X (%S)", q, "badaddr");
		return;
	}

	/*
	 * If options passed in, feed it for verification and handling
	 */
	if (tudr->OPT_length != 0) {
		int t_error;

		if (udp_unitdata_opt_process(q, mp, &t_error) < 0) {
			/* failure */
			udp_ud_err(q, mp, t_error);
			return;
		}
		ASSERT(t_error == 0);
		/*
		 * Note: success in processing options.
		 * mp option buffer represented by
		 * OPT_length/offset now potentially modified
		 * and contain option setting results
		 */
	}

	/* If the user did not pass along an IP header, create one. */
	ip_hdr_length = udp->udp_hdr_length + udp->udp_ip_snd_options_len;
	ipha = (ipha_t *)ALIGN32(&mp1->b_rptr[-ip_hdr_length]);
	if ((mp1->b_datap->db_ref != 1) ||
	    ((u_char *)ipha < mp1->b_datap->db_base) ||
	    !OK_32PTR(ipha)) {
		u_char *wptr;

#ifdef DEBUG
		if (!OK_32PTR(ipha))
			printf("udp_wput: unaligned ptr 0x%x for 0x%x/%d\n",
			    (int)ipha, ntohl(udp->udp_src),
			    ntohs(udp->udp_port));

#endif /* DEBUG */
		mp1 = allocb(ip_hdr_length + udp_wroff_extra, BPRI_LO);
		if (!mp1) {
			udp_ud_err(q, mp, TSYSERR);
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_END,
				"udp_wput_end: q %X (%S)", q, "allocbfail2");
			return;
		}
		mp1->b_cont = mp->b_cont;
		wptr = mp1->b_datap->db_lim;
		mp1->b_wptr = wptr;
		ipha = (ipha_t *)ALIGN32(wptr - ip_hdr_length);
	}
	ip_hdr_length -= UDPH_SIZE;
#ifdef	_BIG_ENDIAN
	/* Set version, header length, and tos */
	*(u16 *)&ipha->ipha_version_and_hdr_length =
	    ((((IP_VERSION << 4) | (ip_hdr_length>>2)) << 8) |
		udp->udp_type_of_service);
	/* Set ttl and protocol */
	*(u16 *)&ipha->ipha_ttl = (udp->udp_ttl << 8) | IPPROTO_UDP;
#else
	/* Set version, header length, and tos */
	*(u16 *)&ipha->ipha_version_and_hdr_length =
		((udp->udp_type_of_service << 8) |
		    ((IP_VERSION << 4) | (ip_hdr_length>>2)));
	/* Set ttl and protocol */
	*(u16 *)&ipha->ipha_ttl = (IPPROTO_UDP << 8) | udp->udp_ttl;
#endif
	/*
	 * Copy our address into the packet.  If this is zero,
	 * ip will fill in the real source address.
	 */
	ipha->ipha_src = udp->udp_src;
	ipha->ipha_fragment_offset_and_flags = 0;
	ipha->ipha_ident = 0;

	mp1->b_rptr = (u_char *)ipha;

	rptr = &rptr[tudr->DEST_offset];
	u1 = mp1->b_wptr - (u_char *)ipha;
	{
		mblk_t	* mp2;
		if ((mp2 = mp1->b_cont) != NULL) {
			do {
				u1 += mp2->b_wptr - mp2->b_rptr;
			} while ((mp2 = mp2->b_cont) != NULL);
		}
	}
	ipha->ipha_length = htons(u1);
	u1 -= ip_hdr_length;
#ifdef _LITTLE_ENDIAN
	u1 = ((u1 & 0xFF) << 8) | (u1 >> 8);
#endif
	udpha = (udpha_t *)ALIGN32(((u_char *)ipha) + ip_hdr_length);
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
#define	udph	((udph_t *)udpha)
		udph->uh_dst_port[0] = ipa->ip_port[0];
		udph->uh_dst_port[1] = ipa->ip_port[1];
#undef ipa
#undef	udph
	} else {
#define	ipa	((ipa_t *)ALIGN32(rptr))
		ipha->ipha_dst = *(u32 *)ALIGN32(ipa->ip_addr);
		if (ipha->ipha_dst == INADDR_ANY)
			ipha->ipha_dst = htonl(INADDR_LOOPBACK);
		udpha->uha_dst_port = *(u16 *)ALIGN16(ipa->ip_port);
#undef ipa
	}

	udpha->uha_src_port = udp->udp_port;

	if (ip_hdr_length > IP_SIMPLE_HDR_LENGTH) {
		u32	cksum;

		bcopy((char *)udp->udp_ip_snd_options,
		    (char *)&ipha[1], udp->udp_ip_snd_options_len);
		/*
		 * Massage source route putting first source route in ipha_dst.
		 * Ignore the destination in dl_unitdata_req.
		 * Create an adjustment for a source route, if any.
		 */
		cksum = ip_massage_options(ipha);
		cksum = (cksum & 0xFFFF) + (cksum >> 16);
		cksum -= ((ipha->ipha_dst >> 16) & 0xFFFF) +
		    (ipha->ipha_dst & 0xFFFF);
		if ((int)cksum < 0)
			cksum--;
		cksum = (cksum & 0xFFFF) + (cksum >> 16);
		/*
		 * IP does the checksum if uha_checksum is non-zero,
		 * We make it easy for IP to include our pseudo header
		 * by putting our length in uha_checksum.
		 */
		cksum += u1;
		cksum = (cksum & 0xFFFF) + (cksum >> 16);
#ifdef _LITTLE_ENDIAN
		if (udp_g_do_checksum)
			u1 = (cksum << 16) | u1;
#else
		if (udp_g_do_checksum)
			u1 = (u1 << 16) | cksum;
		else
			u1 <<= 16;
#endif
	} else {
		/*
		 * IP does the checksum if uha_checksum is non-zero,
		 * We make it easy for IP to include our pseudo header
		 * by putting our length in uha_checksum.
		 */
		if (udp_g_do_checksum)
			u1 |= (u1 << 16);
#ifndef _LITTLE_ENDIAN
		else
			u1 <<= 16;
#endif
	}
	/* Set UDP length and checksum */
	*((u32 *)&udpha->uha_length) = u1;

	freeb(mp);

	/* We're done.  Pass the packet to ip. */
	BUMP_MIB(udp_mib.udpOutDatagrams);
	TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_END,
		"udp_wput_end: q %X (%S)", q, "end");
	putnext(q, mp1);
#undef tudr
}

static void
udp_wput_other(q, mp)
	queue_t	* q;
	mblk_t	* mp;
{
	u_char	* rptr = mp->b_rptr;
	struct datab * db;
	struct iocblk * iocp;
	u32	u1;
	mblk_t	* mp2;
	udp_t	* udp;

	TRACE_1(TR_FAC_UDP, TR_UDP_WPUT_OTHER_START,
		"udp_wput_other_start: q %X", q);

	udp = (udp_t *)q->q_ptr;
	db = mp->b_datap;

	switch (db->db_type) {
	case M_DATA:
		/* Prepend the "connected" header */
		if (udp->udp_hdr_mp == NULL) {
			/* Not connected */
			freemsg(mp);
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
				"udp_wput_other_end: q %X (%S)",
				q, "not-connected");
			return;
		}
		mp2 = dupb(udp->udp_hdr_mp);
		if (mp2 == NULL) {
			/* unitdata error indication? or M_ERROR? */
			freemsg(mp);
			return;
		}
		mp2->b_cont = mp;
		TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_END,
			"udp_wput_end: q %X (%S)", q, "mdata");
		udp_wput(q, mp2);
		return;
	case M_PROTO:
	case M_PCPROTO:
		u1 = mp->b_wptr - rptr;
		if (u1 < sizeof (long)) {
			freemsg(mp);
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
				"udp_wput_other_end: q %X (%S)",
				q, "protoshort");
			return;
		}
		switch (((union T_primitives *)ALIGN32(rptr))->type) {
		case T_ADDR_REQ:
			udp_addr_req(q, mp);
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
				"udp_wput_other_end: q %X (%S)", q, "addrreq");
			return;
		case O_T_BIND_REQ:
		case T_BIND_REQ:
			become_writer(q, mp, (pfi_t)udp_bind);
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
				"udp_wput_other_end: q %X (%S)", q, "bindreq");
			return;
		case T_CONN_REQ:
			udp_connect(q, mp);
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
				"udp_wput_other_end: q %X (%S)", q, "connreq");
			return;
		case T_INFO_REQ:
			udp_info_req(q, mp);
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
				"udp_wput_other_end: q %X (%S)", q, "inforeq");
			return;
		case T_UNITDATA_REQ:
			/*
			 * If a T_UNITDATA_REQ gets here, the address must
			 * be bad.  Valid T_UNITDATA_REQs are handled
			 * in udp_wput.
			 */
			udp_ud_err(q, mp, TBADADDR);
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
				"udp_wput_other_end: q %X (%S)",
				q, "unitdatareq");
			return;
		case T_UNBIND_REQ:
			udp_unbind(q, mp);
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
			    "udp_wput_other_end: q %X (%S)", q, "unbindreq");
			return;
		case O_T_OPTMGMT_REQ:
			if (!snmpcom_req(q, mp, udp_snmp_set, udp_snmp_get,
			    udp->udp_priv_stream))
				svr4_optcom_req(q, mp, udp->udp_priv_stream,
				    &udp_opt_obj);
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
			    "udp_wput_other_end: q %X (%S)",
			    q, "optmgmtreq");
			return;

		case T_OPTMGMT_REQ:
			tpi_optcom_req(q, mp, udp->udp_priv_stream,
				&udp_opt_obj);
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
				"udp_wput_other_end: q %X (%S)",
				q, "optmgmtreq");
			return;

		/* The following 2 TPI messages are not supported by udp. */
		case T_CONN_RES:
		case T_DISCON_REQ:
			udp_err_ack(q, mp, TNOTSUPPORT, 0);
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
				"udp_wput_other_end: q %X (%S)",
				q, "connres/disconreq");
			return;

		/* The following 3 TPI messages are illegal for udp. */
		case T_DATA_REQ:
		case T_EXDATA_REQ:
		case T_ORDREL_REQ:
			freemsg(mp);
			putctl1(RD(q), M_ERROR, EPROTO);
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
				"udp_wput_other_end: q %X (%S)",
				q, "data/exdata/ordrel");
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
		/* TODO: M_IOCTL access to udp_wants_header. */
		iocp = (struct iocblk *)ALIGN32(mp->b_rptr);
		switch (iocp->ioc_cmd) {
		case TI_GETPEERNAME:
			if (udp->udp_udpha == NULL) {
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
				TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
					"udp_wput_other_end: q %X (%S)",
					q, "getpeername");
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
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
				"udp_wput_other_end: q %X (%S)",
				q, "getmyname");
			return;
		case ND_SET:
			if (!udp->udp_priv_stream) {
				iocp->ioc_error = EPERM;
				goto err_ret;
			}
			/* FALLTHRU */
		case ND_GET:
			mutex_enter(&udp_g_lock);
			if (nd_getset(q, udp_g_nd, mp)) {
				mutex_exit(&udp_g_lock);
				qreply(q, mp);
				TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
					"udp_wput_other_end: q %X (%S)",
					q, "get");
				return;
			}
			mutex_exit(&udp_g_lock);
			break;
		default:
			break;
		}
		break;
	case M_IOCDATA:
		/* Make sure it is one of ours. */
		switch (((struct iocblk *)ALIGN32(mp->b_rptr))->ioc_cmd) {
		case TI_GETMYNAME:
		case TI_GETPEERNAME:
			break;
		default:
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
				"udp_wput_other_end: q %X (%S)",
				q, "iocdatadef");
			putnext(q, mp);
			return;
		}
		switch (mi_copy_state(q, mp, &mp2)) {
		case -1:
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
				"udp_wput_other_end: q %X (%S)",
				q, "iocdataneg");
			return;
		case MI_COPY_CASE(MI_COPY_IN, 1): {
			/*
			 * Now we have the strbuf structure for TI_GETMYNAME
			 * and TI_GETPEERNAME.  Next we copyout the requested
			 * address and then we'll copyout the strbuf.
			 */
			ipa_t	* ipaddr;
			ipha_t	* ipha;
			udpha_t	* udpha1;
			struct strbuf * sb = (struct strbuf *)
			    ALIGN32(mp2->b_rptr);

			if (sb->maxlen < (int)sizeof (ipa_t)) {
				mi_copy_done(q, mp, EINVAL);
				TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
				    "udp_wput_other_end: q %X (%S)",
				    q, "iocdatashort");
				return;
			}
			/*
			 * Create a message block to hold the addresses for
			 * copying out.
			 */
			mp2 = mi_copyout_alloc(q, mp, sb->buf, sizeof (ipa_t));
			if (!mp2) {
				TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
					"udp_wput_other_end: q %X (%S)",
					q, "allocbfail");
				return;
			}
			ipaddr = (ipa_t *)ALIGN32(mp2->b_rptr);
			bzero((char *)ipaddr, sizeof (ipa_t));
			ipaddr->ip_family = AF_INET;
			switch (((struct iocblk *)
			    ALIGN32(mp->b_rptr))->ioc_cmd) {
			case TI_GETMYNAME:
				bcopy((char *)&udp->udp_src,
				    (char *)ipaddr->ip_addr,
				    sizeof (ipaddr->ip_addr));
				bcopy((char *)&udp->udp_port,
				    (char *)ipaddr->ip_port,
				    sizeof (ipaddr->ip_port));
				break;
			case TI_GETPEERNAME:
				ipha = &udp->udp_ipha;
				bcopy((char *)&ipha->ipha_dst,
				    (char *)ipaddr->ip_addr,
				    sizeof (ipaddr->ip_addr));
				udpha1 = udp->udp_udpha;
				bcopy((char *)&udpha1->uha_dst_port,
				    (char *)ipaddr->ip_port,
				    sizeof (ipaddr->ip_port));
				break;
			default:
				mi_copy_done(q, mp, EPROTO);
				TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
					"udp_wput_other_end: q %X (%S)",
					q, "default");
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
		TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
			"udp_wput_other_end: q %X (%S)", q, "iocdata");
		return;
	default:
		/* Unrecognized messages are passed through without change. */
		break;
	}
	TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
		"udp_wput_other_end: q %X (%S)", q, "end");
	putnext(q, mp);
}

static int
udp_unitdata_opt_process(q, mp, t_errorp)
	queue_t	* q;
	mblk_t	* mp;
	int *t_errorp;
{
	udp_t	*udp;
	int retval;
	struct T_unitdata_req *udreqp;

	ASSERT(((union T_primitives *)ALIGN32(mp->b_rptr))->type);

	udp = (udp_t *)q->q_ptr;

	udreqp = (struct T_unitdata_req *)ALIGN32(mp->b_rptr);
	*t_errorp = 0;

	/* XXX Remove cast when TPI does not have "long" */
	retval = tpi_optcom_buf(q, mp, (int32_t *)&udreqp->OPT_length,
	    udreqp->OPT_offset, udp->udp_priv_stream, &udp_opt_obj);

	switch (retval) {
	case OB_SUCCESS:
		return (0);	/* success */
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
	return (-1);		/* failure */
}

void
udp_ddi_init(void)
{
	udp_max_optbuf_len = optcom_max_optbuf_len(udp_opt_obj.odb_opt_des_arr,
	    udp_opt_obj.odb_opt_arr_cnt);
}
