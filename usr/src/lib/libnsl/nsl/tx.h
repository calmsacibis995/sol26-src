/*	Copyright (c) 1992-96 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/

#ifndef	_TX_H
#define	_TX_H

#pragma ident	"@(#)tx.h	1.4	96/10/31 SMI"

/*
 * This file contains declarations local to the TLI/XTI implmentation
 */

/*
 * Look buffer list
 * Could be multiple buffers for MT case
 */
struct _ti_lookbufs {
	struct _ti_lookbufs *tl_next; /* next in list   */
	int	tl_lookclen;	/* "look" ctl part length */
	char	*tl_lookcbuf;	/* pointer to "look" ctl	*/
	int	tl_lookdlen;	/* "look" data length	*/
	char	*tl_lookdbuf;	/* pointer to "look" data */
};

/* TI interface user level structure - one per open file */

struct _ti_user {
	struct _ti_user	*ti_next; /* next one		*/
	struct _ti_user	*ti_prev; /* previous one	*/
	int	ti_fd;		/* file descriptor	*/
	struct  _ti_lookbufs ti_lookbufs; /* head of list of look buffers */
	int	ti_lookcnt;	/* buffered look flag	*/
	ushort	ti_flags;	/* flags		*/
	int	ti_rcvsize;	/* connect or disconnect data buf size */
	char	*ti_rcvbuf;	/* connect or disconnect data buffer */
	int	ti_ctlsize;	/* ctl buffer size	*/
	char	*ti_ctlbuf;	/* ctl buffer		*/
	int	ti_state;	/* user level state	*/
	int	ti_ocnt;	/* # outstanding connect indications */
	int32_t	ti_maxpsz;	/* TIDU size		*/
	int32_t	ti_tsdusize;	/* TSDU size		*/
	int32_t	ti_etsdusize;	/* ETSDU size		*/
	int32_t	ti_cdatasize;	/* CDATA_size		*/
	int32_t	ti_ddatasize;	/* DDATA_size		*/
	int32_t	ti_servtype;	/* service type		*/
	int32_t	ti_prov_flag;	/* TPI PROVIDER_flag	*/
	uint	ti_qlen;	/* listener backlog limit */
#ifdef _REENTRANT
	mutex_t ti_lock;	/* lock to protect this data structure */
#endif /* _REENTRANT */
};

/*
 * Local flags used with ti_flags field in instance structure of
 * type 'struct _ti_user' declared above. Historical note:
 * This namespace constants were previously declared in a
 * avery messed up namespace in timod.h
 */
#define	USED		0x0001	/* data structure in use		*/
#define	MORE		0x0008	/* more data				*/
#define	EXPEDITED	0x0010	/* processing expedited TSDU		*/

#define	_T_MAX(x, y) 		((x) > (y) ? (x) : (y))

/*
 * Following are used to indicate which API entry point is calling common
 * routines
 */
#define		TX_TLI_API	1
#define		TX_XTI_API	2

/*
 * Note: T_BADSTATE also defined in <sys/tiuser.h>
 */
#define	T_BADSTATE 8

#ifdef DEBUG
#include <syslog.h>
#define	_T_TX_SYSLOG2(tiptr, X, Y) if ((tiptr)->ti_state == T_BADSTATE)\
	syslog(X, Y)
#else
#define	_T_TX_SYSLOG2(tiptr, X, Y)
#endif DEBUG

/*
 * Macro to change state and log invalid state error
 */

#define	_T_TX_NEXTSTATE(event, tiptr, errstr)	\
	{	tiptr->ti_state = tiusr_statetbl[event][(tiptr)->ti_state]; \
		_T_TX_SYSLOG2((tiptr), LOG_ERR, errstr); \
	}

/*
 * External declarations
 */
extern mutex_t _ti_userlock;

/*
 * Useful shared local constants
 */

/*
 * TX_XTI_LEVEL_MAX_OPTBUF:
 * 	Max option buffer requirement reserved for any XTI level options
 *	passed in an option buffer. This is intended as an upper bound.
 *	Regardless of what the providers states in OPT_size of T_info_ack,
 *	XTI level options can also be added to the option buffer and XTI
 *	test suite in particular stuffs XTI level options whether we support
 *	them or not.
 * Note: sizeof (long) is used here but anticipating future XTI changes to
 * spec anticipate, it will most likely change to sizeof(t_scalar_t) and stay
 * a 32-bit type.
 *
 * Here is the heuristic used to arrive at a value:
 *	2* [		// factor of 2 for "repeat options" type testing
 *		(sizeof(struct t_opthdr)+10*sizeof(long)) // XTI_DEBUG
 *	       +(sizeof(struct t_opthdr)+ 2*sizeof(long)) // XTI_LINGER
 *	       +(sizeof(struct t_opthdr)+ sizeof(long))	  // XTI_RCVBUF
 *	       +(sizeof(struct t_opthdr)+ sizeof(long))	  // XTI_RCVLOWAT
 *	       +(sizeof(struct t_opthdr)+ sizeof(long))	  // XTI_SNDBUF
 *	       +(sizeof(struct t_opthdr)+ sizeof(long))	  // XTI_SNDLOWAT
 *	   ]
 * => 2* [ 56+24+20+20+20+20 ]
 * =>
 */
#define	TX_XTI_LEVEL_MAX_OPTBUF	320


/*
 * Historic information note:
 * The libnsl/nsl code implements TLI and XTI interfaces using common
 * code. Most data structures are similar in the exposed interfaces for
 * the two interfaces (<tiuser.h> and <xti.h>).
 * The common implementation C files include only <xti.h> which is the
 * superset in terms of the exposed interfaces. However the file <tiuser.h>
 * exposes (via <sys/tiuser.h>), in the past contained certain declarations
 * that are strictly internal to the implementation but were exposed through
 * their presence in the public header (<tiuser.h>).
 * Since the implmentation still needs these declarations, they follow
 * in this file and are removed from exposure through the TLI public header
 * (<tiuser.h>) which exposed them in the past.
 */

/*
 * The following are TLI/XTI user level events which cause
 * state changes.
 * NOTE: Historical namespace pollution warning.
 * Some of the event names share the namespace with structure tags
 * so there are defined inside comments here and exposed through
 * TLI and XTI headers (<tiuser.h> and <xti.h>
 */

#define	T_OPEN 		0
/* #define	T_BIND		1 */
/* #define	T_OPTMGMT	2 */
#define	T_UNBIND	3
#define	T_CLOSE		4
#define	T_SNDUDATA	5
#define	T_RCVUDATA	6
#define	T_RCVUDERR	7
#define	T_CONNECT1	8
#define	T_CONNECT2	9
#define	T_RCVCONNECT	10
#define	T_LISTN		11
#define	T_ACCEPT1	12
#define	T_ACCEPT2	13
#define	T_ACCEPT3	14
#define	T_SND		15
#define	T_RCV		16
#define	T_SNDDIS1	17
#define	T_SNDDIS2	18
#define	T_RCVDIS1	19
#define	T_RCVDIS2	20
#define	T_RCVDIS3	21
#define	T_SNDREL	22
#define	T_RCVREL	23
#define	T_PASSCON	24

#define	T_NOEVENTS	25

#define	T_NOSTATES 	9	/* number of legal states */

extern char tiusr_statetbl[T_NOEVENTS][T_NOSTATES];

/*
 * Band definitions for data flow.
 */
#define	TI_NORMAL	0
#define	TI_EXPEDITED	1

/*
 * Bogus states from tiuser.h
 */
#define	T_FAKE		8	/* fake state used when state	*/
				/* cannot be determined		*/
/*
 * Obsolete error event for t_look() in TLI, still needed for compatibility
 * to broken apps that are affected (e.g nfsd,lockd) if real error returned.
 */
#define	T_ERROR 0x0020

/*
 * GENERAL UTILITY MACROS
 */
#define	A_CNT(arr)	(sizeof (arr)/sizeof (arr[0]))
#define	A_END(arr)	(&arr[A_CNT(arr)])
#define	A_LAST(arr)	(&arr[A_CNT(arr)-1])

/*
 * Following macro compares a signed size obtained from TPI primitive
 * to unsigned size of buffer where it needs to go into passed using
 * the "struct netbuf" type.
 * Since many programs are buggy and forget to initialize "netbuf" or
 * (while unlikely!) allocated buffer can legally even be larger than
 * max signed integer, we use the following macro to do unsigned comparison
 * after verifying that signed quantity is positive.
 */
#define	TLEN_GT_NLEN(tpilen, netbuflen) \
	(((tpilen) > 0) && ((unsigned int)(tpilen) > (netbuflen)))


/*
 *	N.B.:  this interface is deprecated.  Use t_strerror() instead.
 */
extern char *t_errlist[];
extern int t_nerr;

/*
 * UTILITY ROUTINES FUNCTION PROTOTYPES
 */

struct _ti_user *_t_checkfd(int fd, int force_sync, int api_semantics);
int _t_delete_tilink(int s);
int _t_rcv_conn_con(struct _ti_user *tiptr, struct t_call *call,
	sigset_t *maskp, struct strbuf *ctlbufp, int api_semantics);
int _t_snd_conn_req(struct _ti_user *tiptr, struct t_call *call,
	struct strbuf *ctlbufp);
int _t_aligned_copy(struct strbuf *strbufp, int len, int init_offset,
	char *datap, long *rtn_offset);
struct _ti_user *_t_create(int fd, struct t_info *info, int api_semantics);
int _t_do_ioctl(int fd, char *buf, int size, int cmd, int *retlen);
int _t_is_event(int fd, struct _ti_user *tiptr);
int _t_is_ok(int fd, struct _ti_user *tiptr, int32_t type);
int _t_look_locked(int fd, struct _ti_user *tiptr, int api_semantics);
int _t_register_lookevent(struct _ti_user *tiptr, caddr_t dptr, int dsize,
	caddr_t cptr, int csize);
void _t_free_looklist_head(struct _ti_user *tiptr);
void _t_flush_lookevents(struct _ti_user *tiptr);
void _t_blockallsignals(sigset_t *maskp);
void _t_restoresigmask(sigset_t *maskp);
int _t_acquire_ctlbuf(struct _ti_user *tiptr, struct strbuf *ctlbufp,
		int *didallocp);
int _t_acquire_databuf(struct _ti_user *tiptr, struct strbuf *databufp,
		int *didallocp);

/*
 * Core function TLI/XTI routines function prototypes
 */
extern int _tx_accept(int fildes, int resfd, struct t_call *call,
	int api_semantics);
extern char *_tx_alloc(int fildes, int struct_type, int fields,
	int api_semantics);
extern int _tx_bind(int fildes, struct t_bind *req, struct t_bind *ret,
	int api_semantics);
extern int _tx_close(int fildes, int api_semantics);
extern int _tx_connect(int fildes, struct t_call *sndcall,
	struct t_call *rcvcall, int api_semantics);
extern int _tx_error(char *errmsg, int api_semantics);
extern int _tx_free(char *ptr, int struct_type, int api_semantics);
extern int _tx_getinfo(int fildes, struct t_info *info, int api_semantics);
extern int _tx_getstate(int fildes, int api_semantics);
extern int _tx_getprotaddr(int filedes, struct t_bind *boundaddr,
	struct t_bind *peer, int api_semantics);
extern int _tx_listen(int fildes, struct t_call *call, int api_semantics);
extern int _tx_look(int fildes, int api_semantics);
extern int _tx_open(char *path, int oflag, struct t_info *info,
		int api_semantics);
extern int _tx_optmgmt(int fildes, struct t_optmgmt *req,
	struct t_optmgmt *ret, int api_semantics);
extern int _tx_rcv(int fildes, char *buf, unsigned nbytes, int *flags,
		int api_semantics);
extern int _tx_rcvconnect(int fildes, struct t_call *call, int api_semantics);
extern int _tx_rcvdis(int fildes, struct t_discon *discon, int api_semantics);
extern int _tx_rcvrel(int fildes, int api_semantics);
extern int _tx_rcvudata(int fildes, struct t_unitdata *unitdata, int *flags,
			int api_semantics);
extern int _tx_rcvuderr(int fildes, struct t_uderr *uderr, int api_semantics);
extern int _tx_snd(int fildes, char *buf, unsigned nbytes, int flags,
		int api_semantics);
extern int _tx_snddis(int fildes, struct t_call *call, int api_semantics);
extern int _tx_sndrel(int fildes, int api_semantics);
extern int _tx_sndudata(int fildes, struct t_unitdata *unitdata,
			int api_semantics);
extern char *_tx_strerror(int errnum, int api_semantics);
extern int _tx_sync(int fildes, int api_semantics);
extern int _tx_unbind(int fildes, int api_semantics);
extern int _tx_unbind_locked(int fd, struct _ti_user *tiptr,
	struct strbuf *ctlbufp);




#endif	/* _TX_H */
