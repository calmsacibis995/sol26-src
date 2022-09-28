/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)timod.c	1.44	96/10/17 SMI"	/* SVr4.0 1.11	*/

/*
 * Transport Interface Library cooperating module - issue 2
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#define	_SUN_TPI_VERSION 1
#include <sys/tihdr.h>
#include <sys/timod.h>
#include <sys/debug.h>
#include <sys/strlog.h>
#include <sys/signal.h>
#include <sys/errno.h>
#include <sys/cred.h>
#include <sys/cmn_err.h>
#include <sys/kmem.h>
#include <sys/sysmacros.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/strsun.h>
#include <c2/audit.h>


/*
 * This is the loadable module wrapper.
 */
#include <sys/conf.h>
#include <sys/modctl.h>

static struct streamtab timinfo;

static struct fmodsw fsw = {
	"timod",
	&timinfo,
	D_NEW|D_MTQPAIR|D_MP,
};

/*
 * Module linkage information for the kernel.
 */

static struct modlstrmod modlstrmod = {
	&mod_strmodops, "transport interface str mod", &fsw
};

static struct modlinkage modlinkage = {
	MODREV_1, &modlstrmod, NULL
};

static krwlock_t	tim_list_rwlock;

struct tim_tim {
	long		tim_flags;
	queue_t		*tim_rdq;
	mblk_t		*tim_iocsave;
	int		tim_mymaxlen;
	int		tim_mylen;
	caddr_t		tim_myname;
	int		tim_peermaxlen;
	int		tim_peerlen;
	caddr_t		tim_peername;
	mblk_t		*tim_consave;
	int		tim_wbufcid;
	int		tim_rbufcid;
	int		tim_wtimoutid;
	int		tim_rtimoutid;
	/* Protected by the global tim_list_rwlock for all instances */
	struct tim_tim	*tim_next;
	struct tim_tim	**tim_ptpn;
	queue_t		*tim_driverq;
	uint32_t	tim_backlog;
};

/*
 * Local flags used with tim_flags field in instance structure of
 * type 'struct _ti_user' declared above. Historical note:
 * This namespace constants were previously declared in a
 * a very messed up namespace in timod.h
 */
#define	WAITIOCACK	0x0004	/* waiting for info for ioctl act	*/
#define	CLTS		0x0020	/* connectionless transport		*/
#define	COTS		0x0040	/* connection-oriented transport	*/
#define	CONNWAIT	0x0100	/* waiting for connect confirmation	*/
#define	LOCORDREL	0x0200	/* local end has orderly released	*/
#define	REMORDREL	0x0400	/* remote end had orderly released	*/
#define	NAMEPROC	0x0800	/* processing a NAME ioctl		*/
#define	DO_MYNAME	0x2000	/* timod handles TI_GETMYNAME		*/
#define	DO_PEERNAME	0x4000	/* timod handles TI_GETPEERNAME		*/


/* Sleep timeout in tim_recover() */
#define	TIMWAIT	(1*hz)

/*
 * Return values for ti_doname().
 */
#define	DONAME_FAIL	0	/* failing ioctl (done) */
#define	DONAME_DONE	1	/* done processing */
#define	DONAME_CONT	2	/* continue proceesing (not done yet) */

/*
 * Function prototypes
 */
static int ti_doname(queue_t *q, mblk_t *mp, caddr_t lname, uint llen,
			caddr_t rname, uint rlen);

int
_init(void)
{
	int	error;

	rw_init(&tim_list_rwlock, "timod: tim main list", RW_DRIVER, NULL);
	error = mod_install(&modlinkage);
	if (error != 0) {
		rw_destroy(&tim_list_rwlock);
		return (error);
	}

	return (0);
}

int
_fini(void)
{
	int	error;

	error = mod_remove(&modlinkage);
	if (error != 0)
		return (error);
	rw_destroy(&tim_list_rwlock);
	return (0);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}


/*
 * Hash list for all instances. Used to find tim_tim structure based on
 * QUEUE_ptr in T_CON_RES. Protected by tim_list_rwlock.
 */
#define	TIM_HASH_SIZE	256
#define	TIM_HASH(driverq) (((unsigned long)(driverq) >> 8) % TIM_HASH_SIZE)
static struct tim_tim	*tim_hash[TIM_HASH_SIZE];
int		tim_cnt = 0;

static void tilog(char *, int);
static int tim_setname(queue_t *q, mblk_t *mp);
static mblk_t *tim_filladdr();
static void tim_bcopy();
static void tim_addlink();
static void tim_dellink();
static struct tim_tim *tim_findlink();
static void tim_recover(queue_t *, mblk_t *, int);

int dotilog = 0;

#define	TIMOD_ID	3

/* stream data structure definitions */

static int timodopen(), timodclose();
static void timodwput(), timodrput(), timodrsrv(), timodwsrv();
static int	timodrproc(queue_t *q, mblk_t *mp),
		timodwproc(queue_t *q, mblk_t *mp);
static struct module_info timod_info =
	{TIMOD_ID, "timod", 0, INFPSZ, 512, 128};
static struct qinit timodrinit = {
	(int (*)())timodrput,
	(int (*)())timodrsrv,
	timodopen,
	timodclose,
	nulldev,
	&timod_info,
	NULL
};
static struct qinit timodwinit = {
	(int (*)())timodwput,
	(int (*)())timodwsrv,
	timodopen,
	timodclose,
	nulldev,
	&timod_info,
	NULL
};
static struct streamtab timinfo = { &timodrinit, &timodwinit, NULL, NULL };

static void send_ERRORW();

/*
 * timodopen -	open routine gets called when the module gets pushed
 *		onto the stream.
 */
/*ARGSUSED*/
static int
timodopen(q, devp, flag, sflag, crp)
	register queue_t *q;
	dev_t *devp;
	int flag;
	int sflag;
	cred_t *crp;
{
	struct tim_tim *tp;

	ASSERT(q != NULL);

	if (q->q_ptr) {
		return (0);
	}

	tp = kmem_zalloc(sizeof (struct tim_tim), KM_SLEEP);
	tp->tim_rdq = q;
	tp->tim_iocsave = NULL;
	tp->tim_consave = NULL;

	/*
	 * Defer allocation of the buffers for the local address and
	 * the peer's address until we need them.
	 * Assume that timod has to handle getname until we here
	 * an iocack from the transport provider.
	 */
	tp->tim_flags |= DO_MYNAME|DO_PEERNAME;
	q->q_ptr = (caddr_t)tp;
	WR(q)->q_ptr = (caddr_t)tp;

	tilog("timodopen: Allocated for tp %x\n", (int)tp);
	tilog("timodopen: Allocated for q %x\n", (int)q);

	qprocson(q);

	/*
	 * Add this one to the list.
	 */
	tim_addlink(tp);

	return (0);
}

static void
tim_timer(q)
	queue_t	*q;
{
	struct tim_tim *tp = (struct tim_tim *)q->q_ptr;

	ASSERT(tp);

	if (q->q_flag & QREADR) {
		ASSERT(tp->tim_rtimoutid);
		tp->tim_rtimoutid = 0;
	} else {
		ASSERT(tp->tim_wtimoutid);
		tp->tim_wtimoutid = 0;
	}
	enableok(q);
	qenable(q);
}

static void
tim_buffer(q)
	queue_t	*q;
{
	struct tim_tim *tp = (struct tim_tim *)q->q_ptr;

	ASSERT(tp);

	if (q->q_flag & QREADR) {
		ASSERT(tp->tim_rbufcid);
		tp->tim_rbufcid = 0;
	} else {
		ASSERT(tp->tim_wbufcid);
		tp->tim_wbufcid = 0;
	}
	enableok(q);
	qenable(q);
}

/*
 * timodclose - This routine gets called when the module gets popped
 * off of the stream.
 */
/*ARGSUSED*/
static int
timodclose(q, flag, crp)
	register queue_t *q;
	int flag;
	cred_t *crp;
{
	register struct tim_tim *tp;
	register mblk_t *mp;
	register mblk_t *nmp;

	ASSERT(q != NULL);

	tp = (struct tim_tim *)q->q_ptr;
	q->q_ptr = NULL;

	ASSERT(tp != NULL);

	tilog("timodclose: Entered for tp %x\n", (int)tp);
	tilog("timodclose: Entered for q %x\n", (int)q);

	qprocsoff(q);

	/*
	 * Cancel any outstanding bufcall
	 * or timeout requests.
	 */
	if (tp->tim_wbufcid) {
		qunbufcall(q, tp->tim_wbufcid);
		tp->tim_wbufcid = 0;
	}
	if (tp->tim_rbufcid) {
		qunbufcall(q, tp->tim_rbufcid);
		tp->tim_rbufcid = 0;
	}
	if (tp->tim_wtimoutid) {
		(void) quntimeout(q, tp->tim_wtimoutid);
		tp->tim_wtimoutid = 0;
	}
	if (tp->tim_rtimoutid) {
		(void) quntimeout(q, tp->tim_rtimoutid);
		tp->tim_rtimoutid = 0;
	}

	if (tp->tim_iocsave != NULL)
		freemsg(tp->tim_iocsave);
	mp = tp->tim_consave;
	while (mp) {
		nmp = mp->b_next;
		mp->b_next = NULL;
		freemsg(mp);
		mp = nmp;
	}
	if (tp->tim_mymaxlen != 0)
		kmem_free(tp->tim_myname, tp->tim_mymaxlen);
	if (tp->tim_peermaxlen != 0)
		kmem_free(tp->tim_peername, tp->tim_peermaxlen);

	q->q_ptr = WR(q)->q_ptr = NULL;
	tim_dellink(tp);


	return (0);
}

/*
 * timodrput -	Module read put procedure.  This is called from
 *		the module, driver, or stream head upstream/downstream.
 *		Handles M_FLUSH, M_DATA and some M_PROTO (T_DATA_IND,
 *		and T_UNITDATA_IND) messages. All others are queued to
 *		be handled by the service procedures.
 */
static void
timodrput(q, mp)
register queue_t *q;
register mblk_t *mp;
{
	register union T_primitives *pptr;

	/*
	 * During flow control and other instances when messages
	 * are on queue, queue up a non high priority message
	 */
	if (q->q_first != 0 && mp->b_datap->db_type < QPCTL) {
		(void) putq(q, mp);
		return;
	}

	/*
	 * Inline processing of data (to avoid additional procedure call).
	 * Rest is handled in timodrproc.
	 */

	switch (mp->b_datap->db_type) {
	case M_DATA:
		if (bcanput(q->q_next, mp->b_band))
			putnext(q, mp);
		else
			(void) putq(q, mp);
		break;
	case M_PROTO:
	case M_PCPROTO:
		pptr = (union T_primitives *)mp->b_rptr;
		switch (pptr->type) {
		case T_EXDATA_IND:
		case T_DATA_IND:
		case T_UNITDATA_IND:
			if (bcanput(q->q_next, mp->b_band))
				putnext(q, mp);
			else
				(void) putq(q, mp);
			break;
		default:
			(void) timodrproc(q, mp);
			break;
		}
		break;
	default:
		(void) timodrproc(q, mp);
		break;
	}
}

/*
 * timodrsrv -	Module read queue service procedure.  This is called when
 *		messages are placed on an empty queue, when high priority
 *		messages are placed on the queue, and when flow control
 *		restrictions subside.  This code used to be included in a
 *		put procedure, but it was moved to a service procedure
 *		because several points were added where memory allocation
 *		could fail, and there is no reasonable recovery mechanism
 *		from the put procedure.
 */
/*ARGSUSED*/
static void
timodrsrv(q)
register queue_t *q;
{
	register mblk_t *mp;
	register struct tim_tim *tp;

	ASSERT(q != NULL);

	tp = (struct tim_tim *)q->q_ptr;
	if (!tp)
	    return;

	while ((mp = getq(q)) != NULL) {
		if (timodrproc(q, mp)) {
			/*
			 * timodwproc did a putbq - stop processing
			 * messages.
			 */
			return;
		}
	}
}


static int
timodrproc(q, mp)
register queue_t *q;
register mblk_t *mp;
{
	register union T_primitives *pptr;
	register struct tim_tim *tp;
	register struct iocblk *iocbp;
	register mblk_t *nbp;
	mblk_t *tmp;
	int size;

	tp = (struct tim_tim *)q->q_ptr;

	switch (mp->b_datap->db_type) {
	default:
		putnext(q, mp);
		break;

	case M_DATA:
		if (!bcanput(q->q_next, mp->b_band)) {
			(void) putbq(q, mp);
			return (1);
		}
		putnext(q, mp);
		break;

	case M_PROTO:
	case M_PCPROTO:
	    /* assert checks if there is enough data to determine type */

	    ASSERT((mp->b_wptr - mp->b_rptr) >= sizeof (long));

	    pptr = (union T_primitives *)mp->b_rptr;
	    switch (pptr->type) {
	    default:

#ifdef C2_AUDIT
		if (audit_active)
		    audit_sock(T_UNITDATA_IND, q, mp, TIMOD_ID);
#endif
		putnext(q, mp);
		break;

	    case T_ERROR_ACK:

		tilog("timodrproc: Got T_ERROR_ACK\n", 0);

		/* Restore db_type - recover() might have changed it */
		mp->b_datap->db_type = M_PCPROTO;
error_ack:
		ASSERT((mp->b_wptr - mp->b_rptr) ==
		    sizeof (struct T_error_ack));

		if (tp->tim_flags & WAITIOCACK) {

		    ASSERT(tp->tim_iocsave != NULL);

		    if (tp->tim_iocsave->b_cont == NULL ||
			pptr->error_ack.ERROR_prim !=
			*(long *)tp->tim_iocsave->b_cont->b_rptr) {
			putnext(q, mp);
			break;
		    }

		    tilog("timodrproc: T_ERROR_ACK: prim %d\n",
				pptr->error_ack.ERROR_prim);

		    switch (pptr->error_ack.ERROR_prim) {
		    default:

			tilog(
			"timodrproc: Unknown T_ERROR_ACK:  tlierror %d\n",
			pptr->error_ack.TLI_error);

			putnext(q, mp);
			break;

		    case T_INFO_REQ:
		    case O_T_OPTMGMT_REQ:
		    case T_OPTMGMT_REQ:
		    case O_T_BIND_REQ:
		    case T_BIND_REQ:
		    case T_UNBIND_REQ:
		    case T_ADDR_REQ:

			tilog(
			"timodrproc: T_ERROR_ACK: tlierror %x\n",
			pptr->error_ack.TLI_error);

			/* get saved ioctl msg and set values */
			iocbp = (struct iocblk *)tp->tim_iocsave->b_rptr;
			iocbp->ioc_error = 0;
			iocbp->ioc_rval = pptr->error_ack.TLI_error;
			if (iocbp->ioc_rval == TSYSERR)
			    iocbp->ioc_rval |= pptr->error_ack.UNIX_error << 8;
			tp->tim_iocsave->b_datap->db_type = M_IOCACK;
			putnext(q, tp->tim_iocsave);
			tp->tim_iocsave = NULL;
			tp->tim_flags &= ~WAITIOCACK;
			freemsg(mp);
			break;
		    }
		    break;
		}
		putnext(q, mp);
		break;

	    case T_OK_ACK:

		tilog("timodrproc: Got T_OK_ACK\n", 0);

		/* Restore db_type - recover() might have change it */
		mp->b_datap->db_type = M_PCPROTO;

		if (tp->tim_flags & WAITIOCACK) {

		    ASSERT(tp->tim_iocsave != NULL);

		    if (tp->tim_iocsave->b_cont == NULL ||
			pptr->ok_ack.CORRECT_prim !=
			*(long *)tp->tim_iocsave->b_cont->b_rptr) {
			    putnext(q, mp);
			    break;
		    }
		    if (pptr->ok_ack.CORRECT_prim == T_UNBIND_REQ)
			tp->tim_mylen = 0;
		    goto out;
		}
		putnext(q, mp);
		break;

	    case T_BIND_ACK: {
		    struct T_bind_ack *ackp =
			    (struct T_bind_ack *)mp->b_rptr;

		    tilog("timodrproc: Got T_BIND_ACK\n", 0);

		    ASSERT((mp->b_wptr - mp->b_rptr)
			>= sizeof (struct T_bind_ack));

		    /* Restore db_type - recover() might have change it */
		    mp->b_datap->db_type = M_PCPROTO;

		    /* save negotiated backlog */
		    tp->tim_backlog = ackp->CONIND_number;

		    if (tp->tim_flags & WAITIOCACK) {
			    caddr_t p;

			    ASSERT(tp->tim_iocsave != NULL);

			    if (tp->tim_iocsave->b_cont == NULL ||
				!(*(long *)tp->tim_iocsave->b_cont->b_rptr ==
				O_T_BIND_REQ ||
				*(long *)tp->tim_iocsave->b_cont->b_rptr ==
				T_BIND_REQ)) {
				    putnext(q, mp);
				    break;
			    }
			    if (tp->tim_flags & DO_MYNAME) {
				    if (ackp->ADDR_length > tp->tim_mymaxlen) {
					    p = kmem_alloc(ackp->ADDR_length,
							KM_NOSLEEP);
					    if (p == NULL) {

						    tilog(
			"timodrproc: kmem_alloc failed attempt recovery\n", 0);

					tim_recover(q, mp, ackp->ADDR_length);
						    return (1);
					    }
					    if (tp->tim_mymaxlen)
						    kmem_free(tp->tim_myname,
							tp->tim_mymaxlen);
					    tp->tim_myname = p;
					    tp->tim_mymaxlen =
						    ackp->ADDR_length;
				    }
				    tp->tim_mylen = ackp->ADDR_length;
				    p = (caddr_t)mp->b_rptr + ackp->ADDR_offset;
				    bcopy(p, tp->tim_myname, tp->tim_mylen);
			    }
			    goto out;
		    }
		    putnext(q, mp);
		    break;
	    }

	    case T_OPTMGMT_ACK:

		tilog("timodrproc: Got T_OPTMGMT_ACK\n", 0);

		/* Restore db_type - recover() might have change it */
		mp->b_datap->db_type = M_PCPROTO;

		if (tp->tim_flags & WAITIOCACK) {

		    ASSERT(tp->tim_iocsave != NULL);

		    if (tp->tim_iocsave->b_cont == NULL ||
			! (*(long *)tp->tim_iocsave->b_cont->b_rptr ==
			    O_T_OPTMGMT_REQ ||
			    *(long *)tp->tim_iocsave->b_cont->b_rptr ==
			    T_OPTMGMT_REQ)) {
			    putnext(q, mp);
			    break;
		    }
		    goto out;
		}
		putnext(q, mp);
		break;

	    case T_INFO_ACK: {
		    uint expected_ack_size;
		    uint deficit;
		    struct ti_sync_ack *tsap;

		    tilog("timodrproc: Got T_INFO_ACK\n", 0);

		    /* Restore db_type - recover() might have change it */
		    mp->b_datap->db_type = M_PCPROTO;

		    if (tp->tim_flags & WAITIOCACK) {

			ASSERT(tp->tim_iocsave != NULL);
			iocbp = (struct iocblk *)tp->tim_iocsave->b_rptr;
			size = mp->b_wptr - mp->b_rptr;
			ASSERT(size == sizeof (struct T_info_ack));
			if (tp->tim_iocsave->b_cont == NULL ||
			    *(long *)tp->tim_iocsave->b_cont->b_rptr !=
			    T_INFO_REQ) {
				putnext(q, mp);
				return (1);
			}
			(void) strqset(q, QMAXPSZ, 0, pptr->info_ack.TIDU_size);
			(void) strqset(OTHERQ(q), QMAXPSZ, 0,
					pptr->info_ack.TIDU_size);
			if ((pptr->info_ack.SERV_type == T_COTS) ||
			    (pptr->info_ack.SERV_type == T_COTS_ORD)) {
				tp->tim_flags = (tp->tim_flags & ~CLTS) | COTS;
			} else if (pptr->info_ack.SERV_type == T_CLTS) {
				tp->tim_flags = (tp->tim_flags & ~COTS) | CLTS;
			}

			/*
			 * make sure the message sent back is the size of
			 * the "expected ack"
			 * For TI_GETINFO, expected ack size is
			 *	sizeof (T_info_ack)
			 * For TI_SYNC, expected ack size is
			 *	sizeof (struct ti_sync_ack);
			 */
			expected_ack_size =
				sizeof (struct T_info_ack); /* TI_GETINFO */
			if (iocbp->ioc_cmd == TI_SYNC)
				expected_ack_size = sizeof (struct ti_sync_ack);
			deficit = expected_ack_size - (uint) size;

			if (deficit != 0) {
				if (mp->b_datap->db_lim - mp->b_wptr <
				    deficit) {
				    tmp = allocb(expected_ack_size,
						BPRI_HI);
				    if (tmp == NULL) {
					ASSERT((mp->b_datap->db_lim -
						mp->b_datap->db_base) <
						sizeof (struct T_error_ack));

					tilog(
			"timodrproc: allocb failed no recovery attempt\n", 0);

					mp->b_rptr = mp->b_datap->db_base;
					mp->b_wptr = mp->b_rptr +
						sizeof (struct T_error_ack);
					pptr = (union T_primitives *)
						mp->b_rptr;
					pptr->error_ack.ERROR_prim = T_INFO_ACK;
					pptr->error_ack.TLI_error = TSYSERR;
					pptr->error_ack.UNIX_error = EAGAIN;
					pptr->error_ack.PRIM_type = T_ERROR_ACK;
					mp->b_datap->db_type = M_PCPROTO;
					goto error_ack;
				} else {
					bcopy((char *)mp->b_rptr,
						(char *)tmp->b_rptr,
						size);
					tmp->b_wptr += size;
					pptr = (union T_primitives *)
					    tmp->b_rptr;
					freemsg(mp);
					mp = tmp;
				}
			    }
			}
			/*
			 * We now have "mp" which has enough space for an
			 * appropriate ack and contains struct T_info_ack
			 * that the transport provider returned. We now
			 * stuff it with more stuff to fullfill
			 * TI_SYNC ioctl needs, as necessary
			 */
			if (iocbp->ioc_cmd == TI_SYNC) {
				/*
				 * Assumes struct T_info_ack is first embedded
				 * type in struct ti_sync_ack so it is
				 * automatically there.
				 */
				tsap = (struct ti_sync_ack *) mp->b_rptr;
				tsap->qlen = tp->tim_backlog;
				mp->b_wptr += sizeof (uint);
			}
			goto out;
		}
		putnext(q, mp);
		break;
	    }

	case T_ADDR_ACK:
		tilog("timodrproc: Got T_ADDR_ACK\n", 0);

		/* Restore db_type - recover() might have change it */
		mp->b_datap->db_type = M_PCPROTO;

		if (tp->tim_flags & WAITIOCACK) {

		    ASSERT(tp->tim_iocsave != NULL);

		    if (tp->tim_iocsave->b_cont == NULL ||
			*(long *)tp->tim_iocsave->b_cont->b_rptr !=
			    T_ADDR_REQ) {
			    putnext(q, mp);
			    break;
		    }
		    goto out;
		}
		putnext(q, mp);
		break;

out:
		iocbp = (struct iocblk *)tp->tim_iocsave->b_rptr;
		ASSERT(tp->tim_iocsave->b_datap != NULL);
		tp->tim_iocsave->b_datap->db_type = M_IOCACK;
		mp->b_datap->db_type = M_DATA;
		freemsg(tp->tim_iocsave->b_cont);
		tp->tim_iocsave->b_cont = mp;
		iocbp->ioc_error = 0;
		iocbp->ioc_rval = 0;
		iocbp->ioc_count = mp->b_wptr - mp->b_rptr;
		putnext(q, tp->tim_iocsave);
		tp->tim_iocsave = NULL;
		tp->tim_flags &= ~WAITIOCACK;
		break;

	    case T_CONN_IND:

		tilog("timodrproc: Got T_CONN_IND\n", 0);

		if (tp->tim_flags & DO_PEERNAME) {
		    if (((nbp = dupmsg(mp)) != NULL) ||
			((nbp = copymsg(mp)) != NULL)) {
			nbp->b_next = tp->tim_consave;
			tp->tim_consave = nbp;
		    } else {
			tim_recover(q, mp, sizeof (mblk_t));
			return (1);
		    }
		}
#ifdef C2_AUDIT
	if (audit_active)
		audit_sock(T_CONN_IND, q, mp, TIMOD_ID);
#endif
		putnext(q, mp);
		break;

	    case T_CONN_CON:

		tilog("timodrproc: Got T_CONN_CON\n", 0);

		tp->tim_flags &= ~CONNWAIT;
		putnext(q, mp);
		break;

	    case T_DISCON_IND: {
		struct T_discon_ind *disp;
		struct T_conn_ind *conp;
		mblk_t *pbp = NULL;

		if (q->q_first != 0)
			tilog("timodrput: T_DISCON_IND - flow control", 0);

		disp = (struct T_discon_ind *)mp->b_rptr;

		tilog("timodrproc: Got T_DISCON_IND Reason: %d\n",
			disp->DISCON_reason);

		tp->tim_flags &= ~(CONNWAIT|LOCORDREL|REMORDREL);
		tp->tim_peerlen = 0;
		for (nbp = tp->tim_consave; nbp; nbp = nbp->b_next) {
		    conp = (struct T_conn_ind *)nbp->b_rptr;
		    if (conp->SEQ_number == disp->SEQ_number)
			break;
		    pbp = nbp;
		}
		if (nbp) {
		    if (pbp)
			pbp->b_next = nbp->b_next;
		    else
			tp->tim_consave = nbp->b_next;
		    nbp->b_next = NULL;
		    freemsg(nbp);
		}
		putnext(q, mp);
		break;
	    }

	    case T_ORDREL_IND:

		tilog("timodrproc: Got T_ORDREL_IND\n", 0);

		if (tp->tim_flags & LOCORDREL) {
		    tp->tim_flags &= ~(LOCORDREL|REMORDREL);
		    tp->tim_peerlen = 0;
		} else {
		    tp->tim_flags |= REMORDREL;
		}
		putnext(q, mp);
		break;

	    case T_EXDATA_IND:
	    case T_DATA_IND:
	    case T_UNITDATA_IND:
		if (pptr->type == T_EXDATA_IND)
			tilog("timodrproc: Got T_EXDATA_IND\n", 0);

		if (!bcanput(q->q_next, mp->b_band)) {
			(void) putbq(q, mp);
			return (1);
		}
		putnext(q, mp);
		break;
	    }
	    break;

	case M_FLUSH:

		tilog("timodrproc: Got M_FLUSH\n", 0);

		if (*mp->b_rptr & FLUSHR) {
			if (*mp->b_rptr & FLUSHBAND)
				flushband(q, *(mp->b_rptr + 1), FLUSHDATA);
			else
				flushq(q, FLUSHDATA);
		}
		putnext(q, mp);
		break;

	case M_IOCACK:
	    iocbp = (struct iocblk *)mp->b_rptr;

	    tilog("timodrproc: Got M_IOCACK\n", 0);

	    if (iocbp->ioc_cmd == TI_GETMYNAME) {

		/*
		 * Transport provider supports this ioctl,
		 * so I don't have to.
		 */
		tp->tim_flags &= ~DO_MYNAME;
		if (tp->tim_mymaxlen != 0) {
		    kmem_free(tp->tim_myname, tp->tim_mymaxlen);
		    tp->tim_myname = NULL;
		    tp->tim_mymaxlen = 0;
		    freemsg(tp->tim_iocsave);
		    tp->tim_iocsave = NULL;
		}
	    } else if (iocbp->ioc_cmd == TI_GETPEERNAME) {
		register mblk_t *bp;

		/*
		 * Transport provider supports this ioctl,
		 * so I don't have to.
		 */
		tp->tim_flags &= ~DO_PEERNAME;
		if (tp->tim_peermaxlen != 0) {
		    kmem_free(tp->tim_peername, tp->tim_peermaxlen);
		    tp->tim_peername = NULL;
		    tp->tim_peermaxlen = 0;
		    freemsg(tp->tim_iocsave);
		    tp->tim_iocsave = NULL;
		    bp = tp->tim_consave;
		    while (bp) {
			nbp = bp->b_next;
			bp->b_next = NULL;
			freemsg(bp);
			bp = nbp;
		    }
		    tp->tim_consave = NULL;
		}
	    }
	    putnext(q, mp);
	    break;

	case M_IOCNAK:

	    tilog("timodrproc: Got M_IOCNAK\n", 0);

	    iocbp = (struct iocblk *)mp->b_rptr;
	    if (((iocbp->ioc_cmd == TI_GETMYNAME) ||
		(iocbp->ioc_cmd == TI_GETPEERNAME)) &&
		((iocbp->ioc_error == EINVAL) || (iocbp->ioc_error == 0))) {
			freemsg(mp);
			if (tp->tim_iocsave) {
			    mp = tp->tim_iocsave;
			    tp->tim_iocsave = NULL;
			    tp->tim_flags |= NAMEPROC;
			    if (ti_doname(WR(q), mp, tp->tim_myname,
				(uint) tp->tim_mylen, tp->tim_peername,
				(uint) tp->tim_peerlen) != DONAME_CONT) {
				    tp->tim_flags &= ~NAMEPROC;
			    }
			    break;
			}
	    }
	    putnext(q, mp);
	    break;
	}

	return (0);
}

/*
 * timodwput -	Module write put procedure.  This is called from
 *		the module, driver, or stream head upstream/downstream.
 *		Handles M_FLUSH, M_DATA and some M_PROTO (T_DATA_REQ,
 *		and T_UNITDATA_REQ) messages. All others are queued to
 *		be handled by the service procedures.
 */
static void
timodwput(q, mp)
register queue_t *q;
register mblk_t *mp;
{
	register union T_primitives *pptr;
	register struct tim_tim *tp;

	/*
	 * During flow control and other instances when messages
	 * are on queue, queue up a non high priority message
	 */
	if (q->q_first != 0 && mp->b_datap->db_type < QPCTL) {
		(void) putq(q, mp);
		return;
	}

	/*
	 * Inline processing of data (to avoid additional procedure call).
	 * Rest is handled in timodwproc.
	 */

	switch (mp->b_datap->db_type) {
	case M_DATA:
		tp = (struct tim_tim *)q->q_ptr;
		ASSERT(tp);
		if (tp->tim_flags & CLTS) {
			mblk_t	*tmp;

			if ((tmp = tim_filladdr(q, mp)) == NULL) {
				(void) putq(q, mp);
				break;
			} else {
				mp = tmp;
			}
		}
		if (bcanput(q->q_next, mp->b_band))
			putnext(q, mp);
		else
			(void) putq(q, mp);
		break;
	case M_PROTO:
	case M_PCPROTO:
		pptr = (union T_primitives *)mp->b_rptr;
		switch (pptr->type) {
		case T_UNITDATA_REQ:
			tp = (struct tim_tim *)q->q_ptr;
			ASSERT(tp);
			if (tp->tim_flags & CLTS) {
				mblk_t	*tmp;

				if ((tmp = tim_filladdr(q, mp)) == NULL) {
					(void) putq(q, mp);
					break;
				} else {
					mp = tmp;
				}
			}
			if (bcanput(q->q_next, mp->b_band))
				putnext(q, mp);
			else
				(void) putq(q, mp);
			break;

		case T_DATA_REQ:
		case T_EXDATA_REQ:
			if (bcanput(q->q_next, mp->b_band))
				putnext(q, mp);
			else
				(void) putq(q, mp);
			break;
		default:
			(void) timodwproc(q, mp);
			break;
		}
		break;
	default:
		(void) timodwproc(q, mp);
		break;
	}
}

/*
 * timodwsrv -	Module write queue service procedure.
 *		This is called when messages are placed on an empty queue,
 *		when high priority messages are placed on the queue, and
 *		when flow control restrictions subside.  This code used to
 *		be included in a put procedure, but it was moved to a
 *		service procedure because several points were added where
 *		memory allocation could fail, and there is no reasonable
 *		recovery mechanism from the put procedure.
 */
static void
timodwsrv(q)
register queue_t *q;
{
	register mblk_t *mp;

	ASSERT(q != NULL);
	if (q->q_ptr == NULL)
	    return;

	while ((mp = getq(q)) != NULL) {
		if (timodwproc(q, mp)) {
			/*
			 * timodwproc did a putbq - stop processing
			 * messages.
			 */
			return;
		}
	}
}

/*
 * Common routine to process write side messages
 */

static int
timodwproc(q, mp)
register queue_t *q;
register mblk_t *mp;
{
	register union T_primitives *pptr;
	register struct tim_tim *tp;
	register mblk_t *tmp;
	struct iocblk *iocbp;

	tp = (struct tim_tim *)q->q_ptr;

	switch (mp->b_datap->db_type) {
	default:
	    putnext(q, mp);
	    break;

	case M_DATA:
	    if (tp->tim_flags & CLTS) {
		if ((tmp = tim_filladdr(q, mp)) == NULL) {
			tim_recover(q, mp, sizeof (struct T_unitdata_req) +
				tp->tim_peerlen);
			return (1);
		} else {
			mp = tmp;
		}
	    }
	    if (!bcanput(q->q_next, mp->b_band)) {
		(void) putbq(q, mp);
		return (1);
	    }
	    putnext(q, mp);
	    break;

	case M_IOCTL:

	    tilog("timodwproc: Got M_IOCTL\n", 0);

	    iocbp = (struct iocblk *)mp->b_rptr;

	    ASSERT((mp->b_wptr - mp->b_rptr) == sizeof (struct iocblk));

	    if (tp->tim_flags & WAITIOCACK) {
		mp->b_datap->db_type = M_IOCNAK;
		iocbp->ioc_error = EPROTO;
		qreply(q, mp);
		break;
	    }

	    switch (iocbp->ioc_cmd) {
	    default:
		putnext(q, mp);
		break;

	    case TI_BIND:
	    case TI_UNBIND:
	    case TI_GETINFO:
	    case TI_OPTMGMT:
	    case TI_SYNC:
	    case TI_GETADDRS:
		if (iocbp->ioc_count == TRANSPARENT) {
		    mp->b_datap->db_type = M_IOCNAK;
		    iocbp->ioc_error = EINVAL;
		    qreply(q, mp);
		    break;
		}
		if (mp->b_cont == NULL) {
		    mp->b_datap->db_type = M_IOCNAK;
		    iocbp->ioc_error = EINVAL;
		    qreply(q, mp);
		    break;
		}
		if (!pullupmsg(mp->b_cont, -1)) {
		    mp->b_datap->db_type = M_IOCNAK;
		    iocbp->ioc_error = EAGAIN;
		    qreply(q, mp);
		    break;
		}
		if ((tmp = copymsg(mp->b_cont)) == NULL) {
		    int i = 0;

		    for (tmp = mp; tmp; tmp = tmp->b_next)
			i += (int)(tmp->b_wptr - tmp->b_rptr);
		    tim_recover(q, mp, i);
		    return (1);
		}
		tp->tim_iocsave = mp;
		tp->tim_flags |= WAITIOCACK;
		if (iocbp->ioc_cmd == TI_GETINFO ||
		    iocbp->ioc_cmd == TI_SYNC)
		    tmp->b_datap->db_type = M_PCPROTO;
		else
		    tmp->b_datap->db_type = M_PROTO;
		putnext(q, tmp);
		break;

	    case TI_GETMYNAME:

		tilog("timodwproc: Got TI_GETMYNAME\n", 0);

		if (!(tp->tim_flags & DO_MYNAME)) {
		    putnext(q, mp);
		    break;
		}
		goto getname;

	    case TI_GETPEERNAME:

		tilog("timodwproc: Got TI_GETPEERNAME\n", 0);

		if (!(tp->tim_flags & DO_PEERNAME)) {
		    putnext(q, mp);
		    break;
		}
getname:
		if ((tmp = copymsg(mp)) == NULL) {
		    int i = 0;

		    for (tmp = mp; tmp; tmp = tmp->b_next)
			i += (int)(tmp->b_wptr - tmp->b_rptr);
		    tim_recover(q, mp, i);
		    return (1);
		}
		tp->tim_iocsave = mp;
		putnext(q, tmp);
		break;

	    case TI_SETMYNAME:

		tilog("timodwproc: Got TI_SETMYNAME\n", 0);

		/*
		 * Kludge ioctl for root only.  If TIMOD is pushed
		 * on a stream that is already "bound", we want
		 * to be able to support the TI_GETMYNAME ioctl if the
		 * transport provider doesn't support it.
		 */
		if (iocbp->ioc_uid != 0)
		    iocbp->ioc_error = EPERM;

		/*
		 * If DO_MYNAME is not set, the transport provider supports
		 * the TI_GETMYNAME ioctl, so setting the name here won't
		 * be of any use.
		 */
		if (!(tp->tim_flags & DO_MYNAME))
		    iocbp->ioc_error = EBUSY;

		goto setname;

	    case TI_SETPEERNAME:

		tilog("timodwproc: Got TI_SETPEERNAME\n", 0);

		/*
		 * Kludge ioctl for root only.  If TIMOD is pushed
		 * on a stream that is already "connected", we want
		 * to be able to support the TI_GETPEERNAME ioctl if the
		 * transport provider doesn't support it.
		 */
		if (iocbp->ioc_uid != 0)
		    iocbp->ioc_error = EPERM;

		/*
		 * If DO_PEERNAME is not set, the transport provider supports
		 * the TI_GETPEERNAME ioctl, so setting the name here won't
		 * be of any use.
		 */
		if (!(tp->tim_flags & DO_PEERNAME))
		    iocbp->ioc_error = EBUSY;

setname:
		if (iocbp->ioc_error == 0) {
		    if (!tim_setname(q, mp))
			return (1);
		} else {
		    mp->b_datap->db_type = M_IOCNAK;
		    freemsg(mp->b_cont);
		    mp->b_cont = NULL;
		    qreply(q, mp);
		}
		break;
	    }
	    break;

	case M_IOCDATA:

	    tilog("timodwproc: Got TI_SETPEERNAME\n", 0);

	    if (tp->tim_flags & NAMEPROC) {
		if (ti_doname(q, mp, tp->tim_myname, (uint) tp->tim_mylen,
		    tp->tim_peername, (uint) tp->tim_peerlen) != DONAME_CONT) {
			tp->tim_flags &= ~NAMEPROC;
		}
	    } else
		putnext(q, mp);
	    break;

	case M_PROTO:
	case M_PCPROTO:
	    /* assert checks if there is enough data to determine type */
	    if ((mp->b_wptr - mp->b_rptr) < sizeof (long)) {
		send_ERRORW(q, mp);
		return (1);
	    }

	    pptr = (union T_primitives *)mp->b_rptr;
	    switch (pptr->type) {
	    default:
		putnext(q, mp);
		break;

	    case T_EXDATA_REQ:
	    case T_DATA_REQ:
		if (pptr->type == T_EXDATA_REQ)
			tilog("timodwproc: Got T_EXDATA_REQ\n", 0);

		if (!bcanput(q->q_next, mp->b_band)) {
			(void) putbq(q, mp);
			return (1);
		}
		putnext(q, mp);
		break;

	    case T_UNITDATA_REQ:
		if (tp->tim_flags & CLTS) {
			if ((tmp = tim_filladdr(q, mp)) == NULL) {
				tim_recover(q, mp,
					    sizeof (struct T_unitdata_req) +
					    tp->tim_peerlen);
				return (1);
			} else {
				mp = tmp;
			}
		}
#ifdef C2_AUDIT
	if (audit_active)
		audit_sock(T_UNITDATA_REQ, q, mp, TIMOD_ID);
#endif
		if (!bcanput(q->q_next, mp->b_band)) {
			(void) putbq(q, mp);
			return (1);
		}
		putnext(q, mp);
		break;

	    case T_CONN_REQ: {
		struct T_conn_req *reqp = (struct T_conn_req *)mp->b_rptr;
		caddr_t p;

		tilog("timodwproc: Got T_CONN_REQ\n", 0);

		if ((mp->b_wptr - mp->b_rptr) < sizeof (struct T_conn_req)) {
			send_ERRORW(q, mp);
			return (1);
		}

		if (tp->tim_flags & DO_PEERNAME) {
		    if (((caddr_t)mp->b_rptr +
			reqp->DEST_offset + reqp->DEST_length) >
			(caddr_t)mp->b_wptr) {
			send_ERRORW(q, mp);
			return (1);
		    }
		    if (reqp->DEST_length > tp->tim_peermaxlen) {
			p = kmem_alloc(reqp->DEST_length, KM_NOSLEEP);
			if (p == NULL) {

				tilog(
			"timodwproc: kmem_alloc failed attempt recovery\n", 0);

			    tim_recover(q, mp, reqp->DEST_length);
			    return (1);
			}
			if (tp->tim_peermaxlen)
			    kmem_free(tp->tim_peername, tp->tim_peermaxlen);
			tp->tim_peername = p;
			tp->tim_peermaxlen = reqp->DEST_length;
		    }
		    tp->tim_peerlen = reqp->DEST_length;
		    p = (caddr_t)mp->b_rptr + reqp->DEST_offset;
		    bcopy(p, tp->tim_peername, tp->tim_peerlen);
		    if (tp->tim_flags & COTS)
			tp->tim_flags |= CONNWAIT;
		}
#ifdef C2_AUDIT
	if (audit_active)
		audit_sock(T_CONN_REQ, q, mp, TIMOD_ID);
#endif
		putnext(q, mp);
		break;
	    }

	    case T_CONN_RES: {
		struct T_conn_res *resp;
		struct T_conn_ind *indp;
		mblk_t *pmp = NULL;
		struct tim_tim *ntp;
		caddr_t p;

		if ((mp->b_wptr - mp->b_rptr) < sizeof (struct T_conn_res)) {
			send_ERRORW(q, mp);
			return (1);
		}

		resp = (struct T_conn_res *)mp->b_rptr;
		for (tmp = tp->tim_consave; tmp; tmp = tmp->b_next) {
		    indp = (struct T_conn_ind *)tmp->b_rptr;
		    if (indp->SEQ_number == resp->SEQ_number)
			break;
		    pmp = tmp;
		}
		if (!tmp)
		    goto cresout;
		if (pmp)
		    pmp->b_next = tmp->b_next;
		else
		    tp->tim_consave = tmp->b_next;
		tmp->b_next = NULL;

		rw_enter(&tim_list_rwlock, RW_READER);
		if ((ntp = tim_findlink(resp->QUEUE_ptr)) == NULL) {
			rw_exit(&tim_list_rwlock);
			goto cresout;
		}
		if (ntp->tim_flags & DO_PEERNAME) {
		    if (indp->SRC_length > ntp->tim_peermaxlen) {
			p = kmem_alloc(indp->SRC_length, KM_NOSLEEP);
			if (p == NULL) {

				tilog(
			"timodwproc: kmem_alloc failed attempt recovery\n", 0);

			    tmp->b_next = tp->tim_consave;
			    tp->tim_consave = tmp;
			    tim_recover(q, mp, indp->SRC_length);
			    rw_exit(&tim_list_rwlock);
			    return (1);
			}
			if (ntp->tim_peermaxlen)
			    kmem_free(ntp->tim_peername, ntp->tim_peermaxlen);
			ntp->tim_peername = p;
			ntp->tim_peermaxlen = indp->SRC_length;
		    }
		    ntp->tim_peerlen = indp->SRC_length;
		    p = (caddr_t)tmp->b_rptr + indp->SRC_offset;
		    bcopy(p, ntp->tim_peername, ntp->tim_peerlen);
		}
		rw_exit(&tim_list_rwlock);
cresout:
		freemsg(tmp);
		putnext(q, mp);
		break;
	    }

	    case T_DISCON_REQ: {
		struct T_discon_req *disp;
		struct T_conn_ind *conp;
		mblk_t *pmp = NULL;

		if ((mp->b_wptr - mp->b_rptr) < sizeof (struct T_discon_req)) {
			send_ERRORW(q, mp);
			return (1);
		}

		disp = (struct T_discon_req *)mp->b_rptr;
		tp->tim_flags &= ~(CONNWAIT|LOCORDREL|REMORDREL);
		tp->tim_peerlen = 0;

		/*
		 * If we are already connected, there won't
		 * be any messages on tim_consave.
		 */
		for (tmp = tp->tim_consave; tmp; tmp = tmp->b_next) {
		    conp = (struct T_conn_ind *)tmp->b_rptr;
		    if (conp->SEQ_number == disp->SEQ_number)
			break;
		    pmp = tmp;
		}
		if (tmp) {
		    if (pmp)
			pmp->b_next = tmp->b_next;
		    else
			tp->tim_consave = tmp->b_next;
		    tmp->b_next = NULL;
		    freemsg(tmp);
		}
		putnext(q, mp);
		break;
	    }

	    case T_ORDREL_REQ:
		if (tp->tim_flags & REMORDREL) {
		    tp->tim_flags &= ~(LOCORDREL|REMORDREL);
		    tp->tim_peerlen = 0;
		} else {
		    tp->tim_flags |= LOCORDREL;
		}
		putnext(q, mp);
		break;
	    }
	    break;

	case M_FLUSH:

	    tilog("timodwproc: Got M_FLUSH\n", 0);

	    if (*mp->b_rptr & FLUSHW) {
		if (*mp->b_rptr & FLUSHBAND)
			flushband(q, *(mp->b_rptr + 1), FLUSHDATA);
		else
			flushq(q, FLUSHDATA);
	    }
	    putnext(q, mp);
	    break;
	}

	return (0);
}

static void
send_ERRORW(q, mp)
register queue_t *q;
register mblk_t *mp;
{

	register mblk_t *mp1;

	if ((MBLKSIZE(mp) < 1) || (DB_REF(mp) > 1)) {
		mp1 = allocb(1, BPRI_HI);
		if (!mp1) {
		    tilog(
			"timodrproc: allocb failed attempt recovery\n", 0);
		    tim_recover(q, mp, 1);
		    return;
		}
		freemsg(mp);
		mp = mp1;
	} else if (mp->b_cont) {
		freemsg(mp->b_cont);
		mp->b_cont = NULL;
	}

	mp->b_datap->db_type = M_ERROR;
	mp->b_rptr = mp->b_datap->db_base;
	*mp->b_rptr = EPROTO;
	mp->b_wptr = mp->b_rptr + sizeof (char);
	qreply(q, mp);
}

static void
tilog(str, arg)
char *str;
int arg;
{
	if (dotilog) {
		if (dotilog & 2)
			(void) cmn_err(CE_CONT, str, arg);
		else
			(void) strlog(TIMOD_ID, -1, 0, SL_TRACE, str, arg);
	}
}



/*
 * Process the TI_GETNAME ioctl.  If no name exists, return len = 0
 * in strbuf structures.  The state transitions are determined by what
 * is hung of cq_private (cp_private) in the copyresp (copyreq) structure.
 * The high-level steps in the ioctl processing are as follows:
 *
 * 1) we recieve an transparent M_IOCTL with the arg in the second message
 *	block of the message.
 * 2) we send up an M_COPYIN request for the strbuf structure pointed to
 *	by arg.  The block containing arg is hung off cq_private.
 * 3) we receive an M_IOCDATA response with cp->cp_private->b_cont == NULL.
 *	This means that the strbuf structure is found in the message block
 *	mp->b_cont.
 * 4) we send up an M_COPYOUT request with the strbuf message hung off
 *	cq_private->b_cont.  The address we are copying to is strbuf.buf.
 *	we set strbuf.len to 0 to indicate that we should copy the strbuf
 *	structure the next time.  The message mp->b_cont contains the
 *	address info.
 * 5) we receive an M_IOCDATA with cp_private->b_cont != NULL and
 *	strbuf.len == 0.  Restore strbuf.len to either llen ot rlen.
 * 6) we send up an M_COPYOUT request with a copy of the strbuf message
 *	hung off mp->b_cont.  In the strbuf structure in the message hung
 *	off cq_private->b_cont, we set strbuf.len to 0 and strbuf.maxlen
 *	to 0.  This means that the next step is to ACK the ioctl.
 * 7) we receive an M_IOCDATA message with cp_private->b_cont != NULL and
 *	strbuf.len == 0 and strbuf.maxlen == 0.  Free up cp->private and
 *	send an M_IOCACK upstream, and we are done.
 *
 */
static int
ti_doname(q, mp, lname, llen, rname, rlen)
	queue_t *q;		/* queue message arrived at */
	mblk_t *mp;		/* M_IOCTL or M_IOCDATA message only */
	caddr_t lname;		/* local name */
	uint llen;		/* length of local name (0 if not set) */
	caddr_t rname;		/* remote name */
	uint rlen;		/* length of remote name (0 if not set) */
{
	struct iocblk *iocp;
	struct copyreq *cqp;
	struct copyresp *csp;
	struct strbuf *np;
	int ret;
	mblk_t *bp;

	switch (mp->b_datap->db_type) {
	case M_IOCTL:
		iocp = (struct iocblk *)mp->b_rptr;
		if ((iocp->ioc_cmd != TI_GETMYNAME) &&
		    (iocp->ioc_cmd != TI_GETPEERNAME)) {
			cmn_err(CE_WARN, "ti_doname: bad M_IOCTL command\n");
			iocp->ioc_error = EINVAL;
			freemsg(mp->b_cont);
			mp->b_cont = NULL;
			mp->b_datap->db_type = M_IOCNAK;
			qreply(q, mp);
			ret = DONAME_FAIL;
			break;
		}
		if ((iocp->ioc_count != TRANSPARENT) ||
		    (mp->b_cont == NULL) || ((mp->b_cont->b_wptr -
		    mp->b_cont->b_rptr) != sizeof (caddr_t))) {
			iocp->ioc_error = EINVAL;
			freemsg(mp->b_cont);
			mp->b_cont = NULL;
			mp->b_datap->db_type = M_IOCNAK;
			qreply(q, mp);
			ret = DONAME_FAIL;
			break;
		}
		cqp = (struct copyreq *)mp->b_rptr;
		cqp->cq_private = mp->b_cont;
		cqp->cq_addr = (caddr_t)*(long *)mp->b_cont->b_rptr;
		mp->b_cont = NULL;
		cqp->cq_size = sizeof (struct strbuf);
		cqp->cq_flag = 0;
		mp->b_datap->db_type = M_COPYIN;
		mp->b_wptr = mp->b_rptr + sizeof (struct copyreq);
		qreply(q, mp);
		ret = DONAME_CONT;
		break;

	case M_IOCDATA:
		csp = (struct copyresp *)mp->b_rptr;
		iocp = (struct iocblk *)mp->b_rptr;
		cqp = (struct copyreq *)mp->b_rptr;
		if ((csp->cp_cmd != TI_GETMYNAME) &&
		    (csp->cp_cmd != TI_GETPEERNAME)) {
			cmn_err(CE_WARN, "ti_doname: bad M_IOCDATA command\n");
			iocp->ioc_error = EINVAL;
			freemsg(mp->b_cont);
			mp->b_cont = NULL;
			mp->b_datap->db_type = M_IOCNAK;
			qreply(q, mp);
			ret = DONAME_FAIL;
			break;
		}
		if (csp->cp_rval) {	/* error */
			freemsg(csp->cp_private);
			freemsg(mp);
			ret = DONAME_FAIL;
			break;
		}
		ASSERT(csp->cp_private != NULL);
		if (csp->cp_private->b_cont == NULL) {	/* got strbuf */
			ASSERT(mp->b_cont);
			np = (struct strbuf *)mp->b_cont->b_rptr;
			if (csp->cp_cmd == TI_GETMYNAME) {
				if (llen == 0) {
					np->len = 0;	/* copy just strbuf */
				} else if (llen > np->maxlen) {
					iocp->ioc_error = ENAMETOOLONG;
					freemsg(csp->cp_private);
					freemsg(mp->b_cont);
					mp->b_cont = NULL;
					mp->b_datap->db_type = M_IOCNAK;
					qreply(q, mp);
					ret = DONAME_FAIL;
					break;
				} else {
					np->len = llen;	/* copy buffer */
				}
			} else {	/* REMOTENAME */
				if (rlen == 0) {
					np->len = 0;	/* copy just strbuf */
				} else if (rlen > np->maxlen) {
					iocp->ioc_error = ENAMETOOLONG;
					freemsg(mp->b_cont);
					mp->b_cont = NULL;
					mp->b_datap->db_type = M_IOCNAK;
					qreply(q, mp);
					ret = DONAME_FAIL;
					break;
				} else {
					np->len = rlen;	/* copy buffer */
				}
			}
			csp->cp_private->b_cont = mp->b_cont;
			mp->b_cont = NULL;
		}
		np = (struct strbuf *)csp->cp_private->b_cont->b_rptr;
		if (np->len == 0) {
			if (np->maxlen == 0) {

				/*
				 * ack the ioctl
				 */
				freemsg(csp->cp_private);
				iocp->ioc_count = 0;
				iocp->ioc_rval = 0;
				iocp->ioc_error = 0;
				mp->b_datap->db_type = M_IOCACK;
				freemsg(mp->b_cont);
				mp->b_cont = NULL;
				qreply(q, mp);
				ret = DONAME_DONE;
				break;
			}

			/*
			 * copy strbuf to user
			 */
			if (csp->cp_cmd == TI_GETMYNAME)
				np->len = llen;
			else	/* TI_GETPEERNAME */
				np->len = rlen;
			if ((bp = allocb(sizeof (struct strbuf), BPRI_MED))
			    == NULL) {

				tilog(
			"ti_doname: allocb failed no recovery attempt\n", 0);

				iocp->ioc_error = EAGAIN;
				freemsg(csp->cp_private);
				freemsg(mp->b_cont);
				bp->b_cont = NULL;
				mp->b_datap->db_type = M_IOCNAK;
				qreply(q, mp);
				ret = DONAME_FAIL;
				break;
			}
			bp->b_wptr += sizeof (struct strbuf);
			bcopy(np, bp->b_rptr, sizeof (struct strbuf));
			cqp->cq_addr =
			    (caddr_t)*(long *)csp->cp_private->b_rptr;
			cqp->cq_size = sizeof (struct strbuf);
			cqp->cq_flag = 0;
			mp->b_datap->db_type = M_COPYOUT;
			mp->b_cont = bp;
			np->len = 0;
			np->maxlen = 0; /* ack next time around */
			qreply(q, mp);
			ret = DONAME_CONT;
			break;
		}

		/*
		 * copy the address to the user
		 */
		if ((bp = allocb(np->len, BPRI_MED)) == NULL) {

		    tilog("ti_doname: allocb failed no recovery attempt\n", 0);

		    iocp->ioc_error = EAGAIN;
		    freemsg(csp->cp_private);
		    freemsg(mp->b_cont);
		    mp->b_cont = NULL;
		    mp->b_datap->db_type = M_IOCNAK;
		    qreply(q, mp);
		    ret = DONAME_FAIL;
		    break;
		}
		bp->b_wptr += np->len;
		if (csp->cp_cmd == TI_GETMYNAME)
			bcopy(lname, bp->b_rptr, llen);
		else	/* TI_GETPEERNAME */
			bcopy(rname, bp->b_rptr, rlen);
		cqp->cq_addr = (caddr_t)np->buf;
		cqp->cq_size = np->len;
		cqp->cq_flag = 0;
		mp->b_datap->db_type = M_COPYOUT;
		mp->b_cont = bp;
		np->len = 0;	/* copy the strbuf next time around */
		qreply(q, mp);
		ret = DONAME_CONT;
		break;

	default:
		cmn_err(CE_WARN,
		    "ti_doname: freeing bad message type = %d\n",
		    mp->b_datap->db_type);
		freemsg(mp);
		ret = DONAME_FAIL;
		break;
	}
	return (ret);
}

static int
tim_setname(q, mp)
	register queue_t *q;
	register mblk_t *mp;
{
	register struct iocblk *iocp;
	register struct copyreq *cqp;
	register struct copyresp *csp;
	struct tim_tim *tp;
	struct strbuf *netp;
	unsigned int len;
	caddr_t p;

	tp = (struct tim_tim *)q->q_ptr;
	iocp = (struct iocblk *)mp->b_rptr;
	cqp = (struct copyreq *)mp->b_rptr;
	csp = (struct copyresp *)mp->b_rptr;

	switch (mp->b_datap->db_type) {
	case M_IOCTL:
		if ((iocp->ioc_cmd != TI_SETMYNAME) &&
		    (iocp->ioc_cmd != TI_SETPEERNAME)) {
			cmn_err(CE_PANIC, "ti_setname: bad M_IOCTL command\n");
		}
		if ((iocp->ioc_count != TRANSPARENT) ||
		    (mp->b_cont == NULL) || ((mp->b_cont->b_wptr -
		    mp->b_cont->b_rptr) != sizeof (caddr_t))) {
			iocp->ioc_error = EINVAL;
			freemsg(mp->b_cont);
			mp->b_cont = NULL;
			mp->b_datap->db_type = M_IOCNAK;
			qreply(q, mp);
			break;
		}
		cqp->cq_addr = (caddr_t)*(long *)mp->b_cont->b_rptr;
		freemsg(mp->b_cont);
		mp->b_cont = NULL;
		cqp->cq_size = sizeof (struct strbuf);
		cqp->cq_flag = 0;
		cqp->cq_private = NULL;
		mp->b_datap->db_type = M_COPYIN;
		mp->b_wptr = mp->b_rptr + sizeof (struct copyreq);
		qreply(q, mp);
		break;

	case M_IOCDATA:
		if (csp->cp_rval) {
			freemsg(mp);
			break;
		}
		if (csp->cp_private == NULL) {	/* got strbuf */
			netp = (struct strbuf *)mp->b_cont->b_rptr;
			csp->cp_private = mp->b_cont;
			mp->b_cont = NULL;
			cqp->cq_addr = netp->buf;
			cqp->cq_size = netp->len;
			cqp->cq_flag = 0;
			mp->b_datap->db_type = M_COPYIN;
			mp->b_wptr = mp->b_rptr + sizeof (struct copyreq);
			qreply(q, mp);
			break;
		} else {			/* got addr */
			len = msgdsize(mp->b_cont);
			if (csp->cp_cmd == TI_SETMYNAME) {
				if (len > tp->tim_mymaxlen) {
					p = kmem_alloc(len, KM_NOSLEEP);
					if (p == NULL) {

				tilog(
			"tim_setname: kmem_alloc failed attempt recovery\n", 0);

					    tim_recover(q, mp, len);
					    return (0);
					}
					if (tp->tim_mymaxlen)
					    kmem_free(tp->tim_myname,
						tp->tim_mymaxlen);
					tp->tim_myname = p;
					tp->tim_mymaxlen = len;
				}
				tp->tim_mylen = len;
				tim_bcopy(mp->b_cont, tp->tim_myname, len);
			} else if (csp->cp_cmd == TI_SETPEERNAME) {
				if (len > tp->tim_peermaxlen) {
					p = kmem_alloc(len, KM_NOSLEEP);
					if (p == NULL) {

				tilog(
			"tim_setname: kmem_alloc failed attempt recovery\n", 0);

						tim_recover(q, mp, len);
						return (0);
					}
					if (tp->tim_peermaxlen)
					    kmem_free(tp->tim_peername,
						tp->tim_peermaxlen);
					tp->tim_peername = p;
					tp->tim_peermaxlen = len;
				}
				tp->tim_peerlen = len;
				tim_bcopy(mp->b_cont, tp->tim_peername, len);
			} else {
				cmn_err(CE_PANIC,
				    "ti_setname: bad M_IOCDATA command\n");
			}
			freemsg(csp->cp_private);
			iocp->ioc_count = 0;
			iocp->ioc_rval = 0;
			iocp->ioc_error = 0;
			mp->b_datap->db_type = M_IOCACK;
			freemsg(mp->b_cont);
			mp->b_cont = NULL;
			qreply(q, mp);
		}
		break;

	default:
		cmn_err(CE_PANIC, "ti_setname: bad message type = %d\n",
		    mp->b_datap->db_type);
	}
	return (1);
}

/*
 * Copy data from a message to a buffer taking into account
 * the possibility of the data being split between multiple
 * message blocks.
 */
static void
tim_bcopy(frommp, to, len)
	mblk_t *frommp;
	register caddr_t to;
	register unsigned int len;
{
	register mblk_t *mp;
	register int size;

	mp = frommp;
	while (mp && len) {
		size = MIN((mp->b_wptr - mp->b_rptr), len);
		bcopy((caddr_t)mp->b_rptr, to, size);
		len -= size;
		to += size;
		mp = mp->b_cont;
	}
}

/*
 * Fill in the address of a connectionless data packet if a connect
 * had been done on this endpoint.
 */
static mblk_t *
tim_filladdr(q, mp)
	register queue_t *q;
	register mblk_t *mp;
{
	register mblk_t *bp;
	register struct tim_tim *tp;
	struct T_unitdata_req *up;
	struct T_unitdata_req *nup;

	tp = (struct tim_tim *)q->q_ptr;
	if (mp->b_datap->db_type == M_DATA) {
		bp = allocb(sizeof (struct T_unitdata_req) + tp->tim_peerlen,
		    BPRI_MED);
		if (bp == NULL) {

		    tilog("tim_filladdr: allocb failed no recovery attempt\n",
				0);

		    return (bp);
		}
		bp->b_datap->db_type = M_PROTO;
		up = (struct T_unitdata_req *)bp->b_rptr;
		up->PRIM_type = T_UNITDATA_REQ;
		up->DEST_length = tp->tim_peerlen;
		bp->b_wptr += sizeof (struct T_unitdata_req);
		up->DEST_offset = sizeof (struct T_unitdata_req);
		up->OPT_length = 0;
		up->OPT_offset = 0;
		if (tp->tim_peerlen) {
		    bcopy(tp->tim_peername, bp->b_wptr, tp->tim_peerlen);
		    bp->b_wptr += tp->tim_peerlen;
		}
		bp->b_cont = mp;
		return (bp);
	} else {
		ASSERT(mp->b_datap->db_type == M_PROTO);
		up = (struct T_unitdata_req *)mp->b_rptr;
		ASSERT(up->PRIM_type == T_UNITDATA_REQ);
		if (up->DEST_length != 0)
			return (mp);
		bp = allocb(sizeof (struct T_unitdata_req) + up->OPT_length +
		    tp->tim_peerlen, BPRI_MED);
		if (bp == NULL) {

		    tilog("tim_filladdr: allocb failed no recovery attempt\n",
				0);

			return (NULL);
		}
		bp->b_datap->db_type = M_PROTO;
		nup = (struct T_unitdata_req *)bp->b_rptr;
		nup->PRIM_type = T_UNITDATA_REQ;
		nup->DEST_length = tp->tim_peerlen;
		bp->b_wptr += sizeof (struct T_unitdata_req);
		nup->DEST_offset = sizeof (struct T_unitdata_req);
		if (tp->tim_peerlen) {
		    bcopy(tp->tim_peername, bp->b_wptr, tp->tim_peerlen);
		    bp->b_wptr += tp->tim_peerlen;
		}
		if (up->OPT_length == 0) {
			nup->OPT_length = 0;
			nup->OPT_offset = 0;
		} else {
			nup->OPT_length = up->OPT_length;
			nup->OPT_offset = sizeof (struct T_unitdata_req) +
			    tp->tim_peerlen;
			bcopy((mp->b_wptr + up->OPT_offset),
			    bp->b_wptr, up->OPT_length);
			bp->b_wptr += up->OPT_length;
		}
		bp->b_cont = mp->b_cont;
		mp->b_cont = NULL;
		freeb(mp);
		return (bp);
	}
}

static void
tim_addlink(tp)
	register struct tim_tim	*tp;
{
	queue_t *driverq;
	struct tim_tim **tpp;
	struct tim_tim	*next;

	/*
	 * Find my driver's read queue (for T_CON_RES handling)
	 */
	driverq = WR(tp->tim_rdq);
	while (SAMESTR(driverq))
		driverq = driverq->q_next;

	driverq = RD(driverq);

	tpp = &tim_hash[TIM_HASH(driverq)];
	rw_enter(&tim_list_rwlock, RW_WRITER);

	tp->tim_driverq = driverq;

	if ((next = *tpp) != NULL)
		next->tim_ptpn = &tp->tim_next;
	tp->tim_next = next;
	tp->tim_ptpn = tpp;
	*tpp = tp;

	tim_cnt++;

	rw_exit(&tim_list_rwlock);
}

static void
tim_dellink(tp)
	register struct tim_tim	*tp;
{
	register struct tim_tim	*next;

	rw_enter(&tim_list_rwlock, RW_WRITER);

	if ((next = tp->tim_next) != NULL)
		next->tim_ptpn = tp->tim_ptpn;
	*(tp->tim_ptpn) = next;

	tim_cnt--;
	if (tp->tim_rdq != NULL)
		tp->tim_rdq->q_ptr = WR(tp->tim_rdq)->q_ptr = NULL;

	kmem_free(tp, sizeof (struct tim_tim));

	rw_exit(&tim_list_rwlock);
}

static struct tim_tim *
tim_findlink(driverq)
	queue_t *driverq;
{
	register struct tim_tim	*tp;

	ASSERT(rw_lock_held(&tim_list_rwlock));

	for (tp = tim_hash[TIM_HASH(driverq)]; tp != NULL; tp = tp->tim_next) {
		if (tp->tim_driverq == driverq) {
			break;
		}
	}
	return (tp);
}

static void
tim_recover(q, mp, size)
	queue_t		*q;
	mblk_t		*mp;
	int		size;
{
	struct tim_tim	*tp;
	int		id;

	tp = (struct tim_tim *)q->q_ptr;

	/*
	 * Avoid re-enabling the queue.
	 */
	if (mp->b_datap->db_type == M_PCPROTO)
		mp->b_datap->db_type = M_PROTO;
	noenable(q);
	(void) putbq(q, mp);

	/*
	 * Make sure there is at most one outstanding request per queue.
	 */
	if (q->q_flag & QREADR) {
		if (tp->tim_rtimoutid || tp->tim_rbufcid)
			return;
	} else {
		if (tp->tim_wtimoutid || tp->tim_wbufcid)
			return;
	}
	if (!(id = qbufcall(RD(q), size, BPRI_MED, tim_buffer, (long)q))) {
		id = qtimeout(RD(q), tim_timer, (caddr_t)q, TIMWAIT);
		if (q->q_flag & QREADR)
			tp->tim_rtimoutid = id;
		else	tp->tim_wtimoutid = id;
	} else	{
		if (q->q_flag & QREADR)
			tp->tim_rbufcid = id;
		else	tp->tim_wbufcid = id;
	}
}
