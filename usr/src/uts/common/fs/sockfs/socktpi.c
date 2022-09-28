/*	Copyright (c) 1994,1995,1996 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/

#pragma ident	"@(#)socktpi.c	1.27	96/10/17 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/cred.h>
#include <sys/kmem.h>
#include <sys/sysmacros.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/open.h>
#include <sys/user.h>
#include <sys/termios.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/esunddi.h>
#include <sys/flock.h>
#include <sys/modctl.h>
#include <sys/vtrace.h>
#include <sys/cmn_err.h>
#include <sys/pathname.h>

#include <sys/socket.h>
#include <sys/socketvar.h>
#include <netinet/in.h>
#include <sys/un.h>

#include <sys/tiuser.h>
#include <sys/tihdr.h>
#include <sys/timod.h>		/* TI_GETMYNAME, TI_GETPEERNAME */

#include <c2/audit.h>

/*
 * Possible failures when memory can't be allocated. The documented behavior:
 *
 * 		5.5:			4.X:		XNET:
 * accept:	ENOMEM/ENOSR/EINTR	- (EINTR)	ENOMEM/ENOBUFS/ENOSR/
 *							EINTR
 *	(4.X does not document EINTR but returns it)
 * bind:	ENOSR			-		ENOBUFS/ENOSR
 * connect: 	EINTR			EINTR		ENOBUFS/ENOSR/EINTR
 * getpeername:	ENOMEM/ENOSR		ENOBUFS (-)	ENOBUFS/ENOSR
 * getsockname:	ENOMEM/ENOSR		ENOBUFS (-)	ENOBUFS/ENOSR
 *	(4.X getpeername and getsockname do not fail in practice)
 * getsockopt:	ENOMEM/ENOSR		-		ENOBUFS/ENOSR
 * listen:	-			-		ENOBUFS
 * recv:	ENOMEM/ENOSR/EINTR	EINTR		ENOBUFS/ENOMEM/ENOSR/
 *							EINTR
 * send:	ENOMEM/ENOSR/EINTR	ENOBUFS/EINTR	ENOBUFS/ENOMEM/ENOSR/
 *							EINTR
 * setsockopt:	ENOMEM/ENOSR		-		ENOBUFS/ENOMEM/ENOSR
 * shutdown:	ENOMEM/ENOSR		-		ENOBUFS/ENOSR
 * socket:	ENOMEM/ENOSR		ENOBUFS		ENOBUFS/ENOMEM/ENOSR
 * socketpair:	ENOMEM/ENOSR		-		ENOBUFS/ENOMEM/ENOSR
 *
 * Resolution. When allocation fails:
 *	recv: return EINTR
 *	send: return EINTR
 *	connect, accept: EINTR
 *	bind, listen, shutdown (unbind, unix_close, disconnect): sleep
 *	socket, socketpair: ENOBUFS
 *	getpeername, getsockname: sleep
 *	getsockopt, setsockopt: sleep
 */

#ifdef SOCK_TEST
/*
 * Variables that make sockfs do something other than the standard TPI
 * for the AF_INET transports.
 *
 * solisten_tpi_tcp:
 *	TCP can handle a T_BIND_REQ with an increased backlog even though
 *	the transport is already bound. This is needed to avoid loosing the
 *	port number should listen() do a T_UNBIND_REQ followed by a T_BIND_REQ.
 *
 * soconnect_tpi_udp:
 *	UDP and ICMP can handle a T_CONN_REQ.
 *	This is needed to make the sequence of connect(), getsockname()
 *	return the local IP address used to send packets to the connected to
 *	destination.
 *
 * soconnect_tpi_tcp:
 *	TCP can handle a T_CONN_REQ without seeing a T_BIND_REQ.
 *	Set this to non-zero to send TPI conformant messages to TCP in this
 *	respect. This is a performance optimization.
 *
 * soaccept_tpi_tcp:
 *	TCP can handle a T_CONN_REQ without the acceptor being bound.
 *	This is a performance optimization that has been picked up in XTI.
 *
 * soaccept_tpi_multioptions:
 *	When inheriting SOL_SOCKET options from the listener to the accepting
 *	socket send them as a single message for AF_INET.
 */
int solisten_tpi_tcp = 0;
int soconnect_tpi_udp = 0;
int soconnect_tpi_tcp = 0;
int soaccept_tpi_tcp = 0;
int soaccept_tpi_multioptions = 1;
#else /* SOCK_TEST */
#define	soconnect_tpi_tcp	0
#define	soconnect_tpi_udp	0
#define	solisten_tpi_tcp	0
#define	soaccept_tpi_tcp	0
#define	soaccept_tpi_multioptions	1
#endif /* SOCK_TEST */

#ifdef SOCK_TEST
extern int do_useracc;
extern int sock_test_timelimit;
#endif /* SOCK_TEST */

/*
 * Some X/Open added checks might have to be backed out to keep SunOS 4.X
 * applications working. Turn on this flag to disable these checks.
 */
int xnet_skip_checks = 0;
int xnet_check_print = 0;
int xnet_truncate_print = 1;

/*
 * TPI does not allow the transport to check credentials on
 * the bind - all the transport can do is to check the credentials
 * that was used in the socket() call. For better security sockfs
 * checks at bind time against this variable.
 */
u_int ipport_reserved = IPPORT_RESERVED;

/*
 * Common create code for socket and accept. If tso is set the values
 * from that node is used instead of issuing a T_INFO_REQ.
 *
 * Assumes that the caller has a VN_HOLD on accessvp.
 * The VN_RELE will occur either when socreate fails or when
 * the returned sonode is freed.
 */
struct sonode *
socreate(vnode_t *accessvp, int domain, int type, int protocol, int version,
    struct sonode *tso, int *errorp)
{
	struct sonode	*so;
	vnode_t		*vp;
	int		error;

	ASSERT(accessvp);
	vp = makesockvp(accessvp, domain, type, protocol);
	ASSERT(vp);

	/*
	 * Check permissions. We do it on accessvp since it is faster
	 * than indirecting through vp.
	 */
	if (error = VOP_ACCESS(accessvp, VREAD|VWRITE, 0, CRED())) {
		VN_RELE(vp);
		*errorp = error;
		return (NULL);
	}
	if (error = sock_open(&vp, FREAD|FWRITE, CRED())) {
		VN_RELE(vp);
		*errorp = error;
		return (NULL);
	}

	so = VTOSO(vp);
	if (error = so_strinit(so, tso)) {
		(void) VOP_CLOSE(vp, 0, 1, 0, CRED());
		VN_RELE(vp);
		*errorp = error;
		return (NULL);
	}
	if (version == SOV_DEFAULT)
		version = so_default_version;

	so->so_version = version;
	return (so);
}

/*
 * Bind the socket to an unspecified address in sockfs only.
 * Used for TCP/UDP transports where we know that the T_BIND_REQ isn't
 * required in all cases.
 */
static void
so_automatic_bind(struct sonode *so)
{
	ASSERT(so->so_family == AF_INET);

	ASSERT(MUTEX_HELD(&so->so_lock));
	ASSERT(!(so->so_state & SS_ISBOUND));
	so->so_state |= SS_ISBOUND;
	ASSERT(so->so_unbind_mp);

	ASSERT(so->so_laddr_len <= so->so_laddr_maxlen);
	bzero((char *)so->so_laddr_sa, so->so_laddr_len);
	so->so_laddr_sa->sa_family = so->so_family;
}


/*
 * bind the socket.
 *
 * If the socket is already bound we allow rebinding if:
 * - it is a SOCK_DGRAM, or
 * - it is a SOCK_STREAM/SOCK_SEQPACKET that has not been connected
 *   and no listen() has been done.
 *
 * A null "name" can be used to unbind the socket under the same constraints
 * as for rebinding.
 */
int
sobind(struct sonode *so, struct sockaddr *name, int namelen, int backlog,
	int flags)
{
	struct T_bind_req	bind_req;
	struct T_bind_ack	*bind_ack;
	int			error = 0;
	mblk_t			*mp;
	char			*addr;
	tpi_length_t		addrlen;

	dprintso(so, 1, ("sobind(0x%x, %x, %d, %d, %x) %s\n",
		so, name, namelen, backlog, flags,
		pr_state(so->so_state, so->so_mode)));

	if (!(flags & _SOBIND_LOCK_HELD)) {
		mutex_enter(&so->so_lock);
		(void) so_lock_single(so, SLOCKED, 0);
	} else {
		ASSERT(MUTEX_HELD(&so->so_lock));
		ASSERT(so->so_flag & SLOCKED);
	}

	/*
	 * Make sure that there is a preallocated unbind_req message
	 * before binding. This message allocated when the socket is
	 * created  but it might be have been consumed.
	 */
	if (so->so_unbind_mp == NULL) {
		dprintso(so, 1, ("sobind: allocating unbind_req\n"));
		/* NOTE: holding so_lock while sleeping */
		so->so_unbind_mp =
		    soallocproto(sizeof (struct T_unbind_req), _ALLOC_SLEEP);
	}

	if (flags & _SOBIND_REBIND) {
		/*
		 * Called from solisten after doing an sounbind or
		 * potentially without the unbind (latter for AF_INET).
		 */
		ASSERT(name == NULL && namelen == 0);

		if (so->so_family == AF_UNIX) {
			ASSERT(so->so_ux_bound_vp);
			addr = (char *)&so->so_ux_laddr;
			addrlen = sizeof (so->so_ux_laddr);
			dprintso(so, 1,
			("sobind rebind UNIX: addrlen %d, addr 0x%x, vp 0x%x\n",
				addrlen, *((int *)addr, so->so_ux_bound_vp)));
		} else {
			addr = (char *)so->so_laddr_sa;
			addrlen = so->so_laddr_len;
		}
	} else if (flags & _SOBIND_UNSPEC) {
		ASSERT(name == NULL && namelen == 0);

		/*
		 * The caller checked SS_ISBOUND but not necessarily
		 * under so_lock
		 */
		if (so->so_state & SS_ISBOUND) {
			/* No error */
			goto done;
		}

		/* Set an initial local address */
		switch (so->so_family) {
		case AF_UNIX:
			/*
			 * Use an address with same size as struct sockaddr
			 * just like BSD.
			 */
			so->so_laddr_len = sizeof (struct sockaddr);
			ASSERT(so->so_laddr_len <= so->so_laddr_maxlen);
			bzero((char *)so->so_laddr_sa, so->so_laddr_len);
			so->so_laddr_sa->sa_family = so->so_family;

			/*
			 * Pass down an address with the implicit bind
			 * magic number and the rest all zeros.
			 * The transport will return a unique address.
			 */
			so->so_ux_laddr.sou_vp = NULL;
			so->so_ux_laddr.sou_magic = SOU_MAGIC_IMPLICIT;
			addr = (char *)&so->so_ux_laddr;
			addrlen = sizeof (so->so_ux_laddr);
			break;

		case AF_INET:
			/*
			 * An unspecified bind in TPI has a NULL address.
			 * Set the address in sockfs to have the sa_family.
			 */
			so->so_laddr_len = sizeof (struct sockaddr);
			ASSERT(so->so_laddr_len <= so->so_laddr_maxlen);
			bzero((char *)so->so_laddr_sa, so->so_laddr_len);
			so->so_laddr_sa->sa_family = so->so_family;
			addr = NULL;
			addrlen = 0;
			break;

		default:
			/*
			 * An unspecified bind in TPI has a NULL address.
			 * Set the address in sockfs to be zero length.
			 *
			 * Can not assume there is a sa_family for all
			 * protocol families. For example, AF_X25 does not
			 * have a family field.
			 */
			so->so_laddr_len = 0;	/* XXX correct? */
			bzero((char *)so->so_laddr_sa, so->so_laddr_len);
			addr = NULL;
			addrlen = 0;
			break;
		}

	} else {
		if (so->so_state & SS_ISBOUND) {
			/*
			 * If it is ok to rebind the socket, first unbind
			 * with the transport. A rebind to the NULL address
			 * is interpreted as an unbind.
			 * Note that a bind to NULL in BSD does unbind the
			 * socket but it fails with EINVAL.
			 */
			if (flags & (_SOBIND_SOCKBSD|_SOBIND_XPG4_2)) {
				error = EINVAL;
				eprintsoline(so, error);
				goto done;
			}
			if ((so->so_mode & SM_CONNREQUIRED) &&
			    (so->so_state & SS_CANTREBIND)) {
				error = EINVAL;
				eprintsoline(so, error);
				goto done;
			}
			error = sounbind(so, _SOUNBIND_LOCK_HELD);
			if (error) {
				eprintsoline(so, error);
				goto done;
			}
			ASSERT(!(so->so_state & SS_ISBOUND));
			if (name == NULL) {
				so->so_state &=
					~(SS_ISCONNECTED|SS_ISCONNECTING);
				goto done;
			}
		}
		/* X/Open requires this check */
		if ((so->so_state & SS_CANTSENDMORE) && !xnet_skip_checks) {
			if (xnet_check_print) {
				printf("sockfs: X/Open bind state check "
				    "caused EINVAL\n");
			}
			error = EINVAL;
			goto done;
		}

		switch (so->so_family) {
		case AF_UNIX:
			/*
			 * All AF_UNIX addresses are nul terminated
			 * when copied (copyin_name) in so the minimum
			 * length is 3 bytes.
			 */
			if (name == NULL || (namelen <= sizeof (short) + 1)) {
				error = EISDIR;
				eprintsoline(so, error);
				goto done;
			}
			/*
			 * Verify so_family matches the bound family.
			 * BSD does not check this for AF_UNIX resulting
			 * in funny mknods.
			 */
			if (name->sa_family != so->so_family) {
				error = EAFNOSUPPORT;
				goto done;
			}
			break;
		case AF_INET:
			if (name == NULL) {
				error = EINVAL;
				eprintsoline(so, error);
				goto done;
			}
			if (namelen != sizeof (struct sockaddr_in)) {
				if (name->sa_family != so->so_family)
					error = EAFNOSUPPORT;
				else
					error = EINVAL;
				eprintsoline(so, error);
				goto done;
			}
			if ((flags & _SOBIND_XPG4_2) &&
			    (name->sa_family != so->so_family)) {
				/*
				 * This check has to be made for X/Open
				 * sockets however application failures have
				 * been observed when it is applied to
				 * all sockets.
				 */
				error = EAFNOSUPPORT;
				eprintsoline(so, error);
				goto done;
			}
			/*
			 * Force a zero sa_family to match so_family.
			 *
			 * Some programs like inetd(1M) don't set the
			 * family field. Other programs leave
			 * sin_family set to garbage - SunOS 4.X does
			 * not check the family field on a bind.
			 * We use the family field that
			 * was passed in to the socket() call.
			 */
			name->sa_family = so->so_family;
			break;

		default:
			/*
			 * Don't do any length or sa_family check to allow
			 * non-sockaddr style addresses.
			 */
			if (name == NULL) {
				error = EINVAL;
				eprintsoline(so, error);
				goto done;
			}
			break;
		}

		if (namelen > so->so_laddr_maxlen) {
			error = ENAMETOOLONG;
			eprintsoline(so, error);
			goto done;
		}
		/*
		 * Save local address.
		 */
		so->so_laddr_len = namelen;
		ASSERT(so->so_laddr_len <= so->so_laddr_maxlen);
		bcopy((char *)name, (char *)so->so_laddr_sa, namelen);

		/*
		 * Permissions check for AF_INET reserved port numbers.
		 * The transport can not do this since the credentials are
		 * not passed on the T_BIND_REQ message.
		 */
		addr = (char *)so->so_laddr_sa;
		addrlen = so->so_laddr_len;
		switch (so->so_family) {
		case AF_INET: {
			struct sockaddr_in *sin =
				(struct sockaddr_in *)so->so_laddr_sa;

			if (sin->sin_port != 0 &&
			    ntohs(sin->sin_port) < ipport_reserved &&
			    !suser(CRED())) {
				error = EACCES;
				eprintsoline(so, error);
				goto done;
			}
			break;
		}
		case AF_UNIX: {
			struct sockaddr_un *soun =
				(struct sockaddr_un *)so->so_laddr_sa;
			struct vnode *vp;
			struct vattr vattr;

			ASSERT(so->so_ux_bound_vp == NULL);
			/*
			 * Create vnode for the specified path name.
			 * Keep vnode held with a reference in so_ux_bound_vp.
			 * Use the vnode pointer as the address used in the
			 * bind with the transport.
			 *
			 * Use the same mode as in BSD. In particular this does
			 * not observe the umask.
			 */
			/* MAXPATHLEN + soun_family + nul termination */
			if (so->so_laddr_len >
			    MAXPATHLEN + sizeof (short) + 1) {
				error = ENAMETOOLONG;
				eprintsoline(so, error);
				goto done;
			}
			vattr.va_type = VSOCK;
			vattr.va_mode = 0777;
			vattr.va_mask = AT_TYPE|AT_MODE;
			/* NOTE: holding so_lock */
			error = vn_create(soun->sun_path, UIO_SYSSPACE, &vattr,
						EXCL, 0, &vp, CRMKNOD, 0);
			if (error) {
				if (error == EEXIST)
					error = EADDRINUSE;
				eprintsoline(so, error);
				goto done;
			}
			/*
			 * Establish pointer from the underlying filesystem
			 * vnode to the socket node.
			 * so_ux_bound_vp and v_stream->sd_vnode form the
			 * cross-linkage between the underlying filesystem
			 * node and the socket node.
			 */
			ASSERT(SOTOV(so)->v_stream);
			mutex_enter(&vp->v_lock);
			vp->v_stream = SOTOV(so)->v_stream;
			so->so_ux_bound_vp = vp;
			mutex_exit(&vp->v_lock);

			/*
			 * Use the vnode pointer value as a unique address
			 * (together with the magic number to avoid conflicts
			 * with implicit binds) in the transport provider.
			 */
			so->so_ux_laddr.sou_vp = (void *)so->so_ux_bound_vp;
			so->so_ux_laddr.sou_magic = SOU_MAGIC_EXPLICIT;
			addr = (char *)&so->so_ux_laddr;
			addrlen = sizeof (so->so_ux_laddr);
			dprintso(so, 1, ("sobind UNIX: addrlen %d, addr 0x%x\n",
				addrlen, *((int *)addr)));
			break;
		}
		} /* end switch (so->so_family) */
	}

	/*
	 * Send down T_BIND_REQ
	 */
	bind_req.PRIM_type = T_BIND_REQ;
	bind_req.ADDR_length = addrlen;
	bind_req.ADDR_offset = sizeof (bind_req);
	bind_req.CONIND_number = backlog;
	/* NOTE: holding so_lock while sleeping */
	mp = soallocproto2((char *)&bind_req, sizeof (bind_req),
				addr, addrlen, 0, _ALLOC_SLEEP);
	/* Done using so_laddr_sa - can drop the lock */
	mutex_exit(&so->so_lock);

	error = kstrputmsg(SOTOV(so), mp, NULL, 0, 0,
			MSG_BAND|MSG_HOLDSIG|MSG_IGNERROR, 0);
	if (error) {
		eprintsoline(so, error);
		mutex_enter(&so->so_lock);
		goto done;
	}

	mutex_enter(&so->so_lock);
	error = sowaitprim(so, T_BIND_REQ, T_BIND_ACK, sizeof (*bind_ack),
			    &mp);
	if (error) {
		eprintsoline(so, error);
		goto done;
	}
	ASSERT(mp);
	/*
	 * Even if some TPI message (e.g. T_DISCON_IND) was received in
	 * strsock_proto while the lock was dropped above, the bind
	 * is allowed to complete.
	 */

	/* Mark as bound. This will be undone if we detect errors below. */
	if (flags & _SOBIND_NOXLATE) {
		ASSERT(so->so_family == AF_UNIX);
		so->so_state |= SS_FADDR_NOXLATE;
	}
	ASSERT(!(so->so_state & SS_ISBOUND) || (flags & _SOBIND_REBIND));
	so->so_state |= SS_ISBOUND;
	ASSERT(so->so_unbind_mp);
	if (flags & _SOBIND_LISTEN)
		so->so_state |= SS_ACCEPTCONN;

	/*
	 * Recompute addrlen - an unspecied bind sent down an
	 * address of length zero but we expect the appropriate length
	 * in return.
	 */
	if (so->so_family == AF_UNIX)
		addrlen = sizeof (so->so_ux_laddr);
	else
		addrlen = so->so_laddr_len;

	bind_ack = (struct T_bind_ack *)mp->b_rptr;
	/*
	 * The alignment restriction is really to strict but
	 * we want enough alignment to inspect the fields of
	 * a sockaddr_in.
	 */
	addr = (char *)sogetoff(mp, bind_ack->ADDR_offset,
				bind_ack->ADDR_length,
				ALIGN_TPI_size);
	if (addr == NULL) {
		freemsg(mp);
		error = EPROTO;
		eprintsoline(so, error);
		goto done;
	}
	if (!(flags & _SOBIND_UNSPEC)) {
		/*
		 * Verify that the transport didn't return something we
		 * did not want e.g. an address other than what we asked for.
		 * NOTE: These checks would go away if/when we switch to
		 * using the new TPI (in which the transport would fail
		 * the request instead of assigning a different address).
		 */
		if (bind_ack->ADDR_length != addrlen) {
			/* Assumes that the requested address was in use */
			freemsg(mp);
			error = EADDRINUSE;
			eprintsoline(so, error);
			goto done;
		}

		switch (so->so_family) {
		case AF_INET: {
			struct sockaddr_in *rname, *aname;

			rname = (struct sockaddr_in *)addr;
			aname = (struct sockaddr_in *)so->so_laddr_sa;

			/*
			 * Binding to port 0 has the semantics of letting
			 * the transport bind to any port.
			 */
			if (aname->sin_port != 0 &&
			    aname->sin_port != rname->sin_port) {
				freemsg(mp);
				error = EADDRINUSE;
				eprintsoline(so, error);
				goto done;
			}
			/*
			 * Pick up the new port number if we bound to port 0.
			 */
			aname->sin_port = rname->sin_port;

			if (aname->sin_addr.s_addr != rname->sin_addr.s_addr) {
				freemsg(mp);
				error = EADDRNOTAVAIL;
				eprintsoline(so, error);
				goto done;
			}
			break;
		}
		case AF_UNIX:
			if (bcmp(addr, (char *)&so->so_ux_laddr,
			    addrlen) != 0) {
				freemsg(mp);
				error = EADDRINUSE;
				eprintsoline(so, error);
				eprintso(so,
					("addrlen %d, addr 0x%x, vp 0x%x\n",
					addrlen, *((int *)addr),
					so->so_ux_bound_vp));
				goto done;
			}
			break;
		default:
			/*
			 * NOTE: This assumes that addresses can be
			 * byte-compared for equivalence.
			 */
			if (bcmp(addr, (char *)so->so_laddr_sa,
			    addrlen) != 0) {
				freemsg(mp);
				error = EADDRINUSE;
				eprintsoline(so, error);
				goto done;
			}
			break;
		}
	} else {
		/*
		 * Save for returned address for getsockname.
		 * Needed for unspecific bind unless transport supports
		 * the TI_GETMYNAME ioctl.
		 * We avoid this copy for AF_INET for better performance.
		 */
		switch (so->so_family) {
		case AF_INET:
			/* AF_INET transports support TI_GETMYNAME */
			break;
		case AF_UNIX:
			/*
			 * Record the address bound with the transport
			 * for use by socketpair.
			 */
			bcopy(addr, (char *)&so->so_ux_laddr, addrlen);
			break;
		default:
			ASSERT(so->so_laddr_len <= so->so_laddr_maxlen);
			bcopy(addr, (char *)so->so_laddr_sa, so->so_laddr_len);
			break;
		}
	}
	freemsg(mp);

done:
	if (error) {
		if (so->so_state & SS_ISBOUND) {
			int err;

			err = sounbind(so, _SOUNBIND_LOCK_HELD);
			/* LINTED - statement has no consequent: if */
			if (err) {
				eprintsoline(so, error);
			} else {
				ASSERT(!(so->so_state & SS_ISBOUND));
			}
		}
	}
	if (!(flags & _SOBIND_LOCK_HELD)) {
		so_unlock_single(so, SLOCKED);
		mutex_exit(&so->so_lock);
	} else {
		/* If the caller held the lock don't release it here */
		ASSERT(MUTEX_HELD(&so->so_lock));
		ASSERT(so->so_flag & SLOCKED);
	}
	return (error);
}

/*
 * Unbind a socket - used when bind() fails, when bind() specifies a NULL
 * address, or when listen needs to unbind and bind.
 * If the _SOUNBIND_REBIND flag is specified the addresses are retained
 * so that a sobind can pick them up.
 */
int
sounbind(struct sonode *so, int flags)
{
	struct T_unbind_req	unbind_req;
	int			error = 0;
	mblk_t			*mp;

	dprintso(so, 1, ("sounbind(0x%x, %x) %s\n",
			so, flags, pr_state(so->so_state, so->so_mode)));

	if (!(flags & _SOUNBIND_LOCK_HELD)) {
		mutex_enter(&so->so_lock);
		(void) so_lock_single(so, SLOCKED, 0);
	} else {
		ASSERT(MUTEX_HELD(&so->so_lock));
		ASSERT(so->so_flag & SLOCKED);
	}

	if (!(so->so_state & SS_ISBOUND)) {
		error = EINVAL;
		eprintsoline(so, error);
		goto done;
	}

	mutex_exit(&so->so_lock);

	/*
	 * Flush the read and write side (except stream head read queue)
	 * and send down T_UNBIND_REQ.
	 */
	(void) putnextctl1(strvp2wq(SOTOV(so)), M_FLUSH, FLUSHRW);

	unbind_req.PRIM_type = T_UNBIND_REQ;
	mp = soallocproto1((char *)&unbind_req, sizeof (unbind_req),
	    0, _ALLOC_SLEEP);
	error = kstrputmsg(SOTOV(so), mp, NULL, 0, 0,
			MSG_BAND|MSG_HOLDSIG|MSG_IGNERROR, 0);
	mutex_enter(&so->so_lock);
	if (error) {
		eprintsoline(so, error);
		goto done;
	}

	error = sowaitokack(so, T_UNBIND_REQ);
	if (error) {
		eprintsoline(so, error);
		goto done;
	}

	/*
	 * Even if some TPI message (e.g. T_DISCON_IND) was received in
	 * strsock_proto while the lock was dropped above, the unbind
	 * is allowed to complete.
	 */
	if (!(flags & _SOUNBIND_REBIND)) {
		/*
		 * Clear out bound address.
		 */
		vnode_t *vp;

		if ((vp = so->so_ux_bound_vp) != NULL) {
			ASSERT(vp->v_stream);
			so->so_ux_bound_vp = NULL;
			vn_rele_stream(vp);
		}
		/* Clear out address */
		so->so_laddr_len = 0;
	}
	so->so_state &= ~SS_ISBOUND;
done:
	if (!(flags & _SOUNBIND_LOCK_HELD)) {
		so_unlock_single(so, SLOCKED);
		mutex_exit(&so->so_lock);
	} else {
		/* If the caller held the lock don't release it here */
		ASSERT(MUTEX_HELD(&so->so_lock));
		ASSERT(so->so_flag & SLOCKED);
	}
	return (error);
}

/*
 * Used when unbinding in response to a received message.
 * Can not block since it is called from a put procedure (strsock_proto)
 * i.e. the same rules as for put procedures apply to this routine.
 *
 * If there is no preallocated unbind_mp or the socket is not bound
 * return without doing anything.
 */
void
sounbind_nonblock(struct sonode *so)
{
	struct T_unbind_req	*ubr;
	mblk_t			*mp;
	int			error;

	ASSERT(MUTEX_NOT_HELD(&so->so_lock));

	dprintso(so, 1, ("sounbind_nonblock(0x%x) %s\n",
			so, pr_state(so->so_state, so->so_mode)));

	/*
	 * Check if there is a preallocated unbind message.
	 * If no message exists we have already unbound with the transport
	 * so there is nothing left to do.
	 */
	mutex_enter(&so->so_lock);
	mp = so->so_unbind_mp;
	if (mp == NULL) {
		ASSERT(!(so->so_state & SS_ISBOUND));
		mutex_exit(&so->so_lock);
		return;
	}
	if (!(so->so_state & SS_ISBOUND)) {
		mutex_exit(&so->so_lock);
		return;
	}
	so->so_unbind_mp = NULL;

	/*
	 * Make strsock_proto ignore T_OK_ACK and T_ERROR_ACK for
	 * this unbind.
	 */
	so->so_state |= SS_WUNBIND;
	ASSERT(!(so->so_state & (SS_ISCONNECTED|SS_ISCONNECTING)));
	so->so_state &= ~SS_ISBOUND;
	mutex_exit(&so->so_lock);

	/*
	 * Send down T_UNBIND_REQ ignoring flow control.
	 * XXX Assumes that MSG_IGNFLOW implies that this thread
	 * does not run service procedures.
	 */
	mp->b_datap->db_type = M_PROTO;
	ubr = (struct T_unbind_req *)mp->b_rptr;
	mp->b_wptr += sizeof (*ubr);
	ubr->PRIM_type = T_UNBIND_REQ;

	/*
	 * Flush the read and write side (except stream head read queue)
	 * and send down T_UNBIND_REQ.
	 */
	(void) putnextctl1(strvp2wq(SOTOV(so)), M_FLUSH, FLUSHRW);
	error = kstrputmsg(SOTOV(so), mp, NULL, 0, 0,
			MSG_BAND|MSG_HOLDSIG|MSG_IGNERROR|MSG_IGNFLOW, 0);
	/* LINTED - warning: statement has no consequent: if */
	if (error) {
		eprintsoline(so, error);
	}
}

/*
 * listen on the socket.
 * For TPI conforming transports this has to first unbind with the transport
 * and then bind again using the new backlog.
 */
int
solisten(struct sonode *so, int backlog, int flags)
{
	int		error = 0;

	dprintso(so, 1, ("solisten(0x%x, %d, %d) %s\n",
		so, backlog, flags, pr_state(so->so_state, so->so_mode)));

	if (so->so_serv_type == T_CLTS)
		return (EOPNOTSUPP);

	/*
	 * If the socket is ready to accept connections already, then
	 * return without doing anything.  This avoids a problem where
	 * a second listen() call fails if a connection is pending and
	 * leaves the socket unbound. Only when we are not unbinding
	 * with the transport can we safely increase the backlog.
	 */
	if (so->so_state & SS_ACCEPTCONN &&
	    !(so->so_family == AF_INET && !solisten_tpi_tcp))
		return (0);

	if (so->so_state & SS_ISCONNECTED)
		return (EINVAL);

	mutex_enter(&so->so_lock);
	(void) so_lock_single(so, SLOCKED, 0);

	if (backlog < 0)
		backlog = 0;
	if (!(flags & _SOLISTEN_XPG4_2)) {
		/*
		 * Use the same qlimit as in BSD. BSD checks the qlimit
		 * before queuing the next connection implying that a
		 * listen(sock, 0) allows one connection to be queued.
		 * BSD also uses 1.5 times the requested backlog.
		 * X/Open requires a strict interpretation of the backlog.
		 */
		backlog++;
		backlog = 3 * backlog / 2;
	}

	if (!(so->so_state & SS_ISBOUND)) {
		/*
		 * Must have been explicitly bound in the UNIX domain.
		 */
		if (so->so_family == AF_UNIX) {
			error = EINVAL;
			goto done;
		}
		error = sobind(so, NULL, 0, backlog,
			    _SOBIND_UNSPEC|_SOBIND_LOCK_HELD|_SOBIND_LISTEN);
	} else if (backlog > 0) {
		/*
		 * AF_INET hack to avoid loosing the port.
		 * Assumes that all AF_INET transports can handle a T_BIND_REQ
		 * with a non-zero CONIND_number when the TPI
		 * has already bound thus it is possible to avoid the unbind.
		 */
		if (!(so->so_family == AF_INET && !solisten_tpi_tcp)) {
			error = sounbind(so,
					_SOUNBIND_REBIND|_SOUNBIND_LOCK_HELD);
			if (error)
				goto done;
		}
		error = sobind(so, NULL, 0, backlog,
			    _SOBIND_REBIND|_SOBIND_LOCK_HELD|_SOBIND_LISTEN);
	} else {
		so->so_state |= SS_ACCEPTCONN;
	}
	if (error)
		goto done;
	ASSERT(so->so_state & SS_ACCEPTCONN);
done:
	so_unlock_single(so, SLOCKED);
	mutex_exit(&so->so_lock);
	return (error);
}

/*
 * Disconnect either a specified seqno or all (-1).
 * The former is used on listening sockets only.
 *
 * When seqno == -1 sodisconnect could call sounbind. However,
 * the current use of sodisconnect(seqno == -1) is only for shutdown
 * so there is no point (and potentially incorrect) to unbind.
 */
int
sodisconnect(struct sonode *so, int seqno, int flags)
{
	struct T_discon_req	discon_req;
	int			error = 0;
	mblk_t			*mp;

	dprintso(so, 1, ("sodisconnect(0x%x, %d, %x) %s\n",
			so, seqno, flags, pr_state(so->so_state, so->so_mode)));

	if (!(flags & _SODISCONNECT_LOCK_HELD)) {
		mutex_enter(&so->so_lock);
		(void) so_lock_single(so, SLOCKED, 0);
	} else {
		ASSERT(MUTEX_HELD(&so->so_lock));
		ASSERT(so->so_flag & SLOCKED);
	}

	if (!(so->so_state & (SS_ISCONNECTED|SS_ISCONNECTING|SS_ACCEPTCONN))) {
		error = EINVAL;
		eprintsoline(so, error);
		goto done;
	}

	mutex_exit(&so->so_lock);
	/*
	 * Flush the write side (unless this is a listener)
	 * and then send down a T_DISCON_REQ.
	 * (Don't flush on listener since it could flush T_CONN_RES
	 * and other messages.)
	 */
	if (!(so->so_state & SS_ACCEPTCONN))
		(void) putnextctl1(strvp2wq(SOTOV(so)), M_FLUSH, FLUSHW);

	discon_req.PRIM_type = T_DISCON_REQ;
	discon_req.SEQ_number = seqno;
	mp = soallocproto1((char *)&discon_req, sizeof (discon_req),
	    0, _ALLOC_SLEEP);
	error = kstrputmsg(SOTOV(so), mp, NULL, 0, 0,
			MSG_BAND|MSG_HOLDSIG|MSG_IGNERROR, 0);
	mutex_enter(&so->so_lock);
	if (error) {
		eprintsoline(so, error);
		goto done;
	}

	error = sowaitokack(so, T_DISCON_REQ);
	if (error) {
		eprintsoline(so, error);
		goto done;
	}
	/*
	 * Even if some TPI message (e.g. T_DISCON_IND) was received in
	 * strsock_proto while the lock was dropped above, the disconnect
	 * is allowed to complete. However, it is not possible to
	 * assert that SS_ISCONNECTED|SS_ISCONNECTING are set.
	 */
	so->so_state &= ~(SS_ISCONNECTED|SS_ISCONNECTING);
done:
	if (!(flags & _SODISCONNECT_LOCK_HELD)) {
		so_unlock_single(so, SLOCKED);
		mutex_exit(&so->so_lock);
	} else {
		/* If the caller held the lock don't release it here */
		ASSERT(MUTEX_HELD(&so->so_lock));
		ASSERT(so->so_flag & SLOCKED);
	}
	return (error);
}

static void soinheritoptions_multi(struct sonode *, struct sonode *);
static void soinheritoptions_single(struct sonode *, struct sonode *);

/*
 * Copy all SOL_SOCKET options from so to nso.
 * Since SO_ACCEPTCONN is never set we just transfer all the options.
 * This routine assumes that "value options" (e.g. SO_SNDBUF) only need
 * to be set if the value recorded in sockfs is non-zero i.e. the application
 * performed a setsockopt at some point.
 */
static void
soinheritoptions(struct sonode *so, struct sonode *nso)
{
	if (nso->so_family == AF_INET &&
	    soaccept_tpi_multioptions)
		soinheritoptions_multi(so, nso);
	else
		soinheritoptions_single(so, nso);
}

/*
 * Copy the known SOL_SOCKET options using a single T_OPTMGMT_REQ
 * message.
 * Only used for transport that are known to handle such T_OPTMGMT_REQ
 * messages.
 * Note that such transports must handle all the SOL_SOCKET options.
 * Otherwise the T_OPTMGMT_REQ might fail to set any of the options.
 */
static void
soinheritoptions_multi(struct sonode *so, struct sonode *nso)
{
	int			options;
	int			optlen;
	struct T_optmgmt_req	optmgmt_req;
	struct opthdr		oh;
	mblk_t			*mp;
	int			error;
	int			on = 1;

	options = so->so_options & (SO_DEBUG|SO_REUSEADDR|SO_KEEPALIVE|
	    SO_DONTROUTE|SO_BROADCAST|SO_USELOOPBACK|SO_OOBINLINE|
	    SO_DGRAM_ERRIND|SO_LINGER);

	/*
	 * Set the options and their values in nso while counting
	 * how large the option buffer needs to be
	 */
	optlen = 0;
	nso->so_options |= options;

	if (options & SO_DEBUG)
		optlen += sizeof (struct opthdr) + sizeof (int);
	if (options & SO_REUSEADDR)
		optlen += sizeof (struct opthdr) + sizeof (int);
	if (options & SO_KEEPALIVE)
		optlen += sizeof (struct opthdr) + sizeof (int);
	if (options & SO_DONTROUTE)
		optlen += sizeof (struct opthdr) + sizeof (int);
	if (options & SO_BROADCAST)
		optlen += sizeof (struct opthdr) + sizeof (int);
	if (options & SO_USELOOPBACK)
		optlen += sizeof (struct opthdr) + sizeof (int);
	if (options & SO_OOBINLINE)
		optlen += sizeof (struct opthdr) + sizeof (int);
	if (options & SO_DGRAM_ERRIND)
		optlen += sizeof (struct opthdr) + sizeof (int);

	if (options & SO_LINGER) {
		nso->so_linger = so->so_linger;
		optlen += sizeof (struct opthdr) + sizeof (struct linger);
	}

	if ((nso->so_sndbuf = so->so_sndbuf) != 0)
		optlen += sizeof (struct opthdr) + sizeof (int);
	if ((nso->so_rcvbuf = so->so_rcvbuf) != 0)
		optlen += sizeof (struct opthdr) + sizeof (int);
#ifdef notyet
	/*
	 * We do not implement the semantics of these options
	 * thus we shouldn't implement the options either.
	 */
	if ((nso->so_sndlowat = so->so_sndlowat) != 0)
		optlen += sizeof (struct opthdr) + sizeof (int);
	if ((nso->so_rcvlowat = so->so_rcvlowat) != 0)
		optlen += sizeof (struct opthdr) + sizeof (int);
	if ((nso->so_sndtimeo = so->so_sndtimeo) != 0)
		optlen += sizeof (struct opthdr) + sizeof (int);
	if ((nso->so_rcvtimeo = so->so_rcvtimeo) != 0)
		optlen += sizeof (struct opthdr) + sizeof (int);
#endif /* notyet */

	if (optlen == 0) {
		/* No options to inherit */
		return;
	}

	/*
	 * Allocate the message and add the options.
	 * Assumes that all of the SOL_SOCKET options have sizes
	 * that do not require padding.
	 */
	optmgmt_req.PRIM_type = T_OPTMGMT_REQ;
	optmgmt_req.MGMT_flags = T_NEGOTIATE;
	optmgmt_req.OPT_length = optlen;
	optmgmt_req.OPT_offset = sizeof (optmgmt_req);

	oh.level = SOL_SOCKET;
	oh.name = 0;		/* Set below */
	oh.len = sizeof (int);	/* Default */

	mp = soallocproto1((char *)&optmgmt_req, sizeof (optmgmt_req),
	    sizeof (optmgmt_req) + optlen, _ALLOC_SLEEP);
	if (options & SO_DEBUG) {
		oh.name = SO_DEBUG;
		soappendmsg(mp, (char *)&oh, sizeof (oh));
		soappendmsg(mp, (char *)&on, sizeof (on));
	}
	if (options & SO_REUSEADDR) {
		oh.name = SO_REUSEADDR;
		soappendmsg(mp, (char *)&oh, sizeof (oh));
		soappendmsg(mp, (char *)&on, sizeof (on));
	}
	if (options & SO_KEEPALIVE) {
		oh.name = SO_KEEPALIVE;
		soappendmsg(mp, (char *)&oh, sizeof (oh));
		soappendmsg(mp, (char *)&on, sizeof (on));
	}
	if (options & SO_DONTROUTE) {
		oh.name = SO_DONTROUTE;
		soappendmsg(mp, (char *)&oh, sizeof (oh));
		soappendmsg(mp, (char *)&on, sizeof (on));
	}
	if (options & SO_BROADCAST) {
		oh.name = SO_BROADCAST;
		soappendmsg(mp, (char *)&oh, sizeof (oh));
		soappendmsg(mp, (char *)&on, sizeof (on));
	}
	if (options & SO_USELOOPBACK) {
		oh.name = SO_USELOOPBACK;
		soappendmsg(mp, (char *)&oh, sizeof (oh));
		soappendmsg(mp, (char *)&on, sizeof (on));
	}
	if (options & SO_OOBINLINE) {
		oh.name = SO_OOBINLINE;
		soappendmsg(mp, (char *)&oh, sizeof (oh));
		soappendmsg(mp, (char *)&on, sizeof (on));
	}
	if (options & SO_DGRAM_ERRIND) {
		oh.name = SO_DGRAM_ERRIND;
		soappendmsg(mp, (char *)&oh, sizeof (oh));
		soappendmsg(mp, (char *)&on, sizeof (on));
	}

	if (options & SO_LINGER) {
		oh.name = SO_LINGER;
		oh.len = sizeof (struct linger);
		soappendmsg(mp, (char *)&oh, sizeof (oh));
		soappendmsg(mp, (char *)&nso->so_linger,
		    sizeof (nso->so_linger));
		oh.len = sizeof (int);
	}

	if (nso->so_sndbuf != 0) {
		oh.name = SO_SNDBUF;
		soappendmsg(mp, (char *)&oh, sizeof (oh));
		soappendmsg(mp, (char *)&nso->so_sndbuf, sizeof (int));
	}
	if (nso->so_rcvbuf != 0) {
		oh.name = SO_RCVBUF;
		soappendmsg(mp, (char *)&oh, sizeof (oh));
		soappendmsg(mp, (char *)&nso->so_rcvbuf, sizeof (int));
	}
#ifdef notyet
	if (nso->so_sndlowat != 0) {
		oh.name = SO_SNDLOWAT;
		soappendmsg(mp, (char *)&oh, sizeof (oh));
		soappendmsg(mp, (char *)&nso->so_sndlowat, sizeof (int));
	}
	if (nso->so_rcvlowat != 0) {
		oh.name = SO_RCVLOWAT;
		soappendmsg(mp, (char *)&oh, sizeof (oh));
		soappendmsg(mp, (char *)&nso->so_rcvlowat, sizeof (int));
	}
	if (nso->so_sndtimeo != 0) {
		oh.name = SO_SNDTIMEO;
		soappendmsg(mp, (char *)&oh, sizeof (oh));
		soappendmsg(mp, (char *)&nso->so_sndtimeo, sizeof (int));
	}
	if (nso->so_rcvtimeo != 0) {
		oh.name = SO_RCVTIMEO;
		soappendmsg(mp, (char *)&oh, sizeof (oh));
		soappendmsg(mp, (char *)&nso->so_rcvtimeo, sizeof (int));
	}
#endif /* notyet */
	ASSERT(mp->b_wptr <= mp->b_datap->db_lim);

	/* Let option management work in the presence of data flow control */
	error = kstrputmsg(SOTOV(nso), mp, NULL, 0, 0,
			MSG_BAND|MSG_HOLDSIG|MSG_IGNERROR|MSG_IGNFLOW, 0);
	if (error) {
		eprintsoline(nso, error);
		return;
	}
	mutex_enter(&nso->so_lock);
	error = sowaitprim(nso, T_OPTMGMT_REQ, T_OPTMGMT_ACK,
			    sizeof (struct T_optmgmt_ack), &mp);
	mutex_exit(&nso->so_lock);
	if (error) {
		eprintsoline(nso, error);
#ifdef DEBUG
		cmn_err(CE_WARN,
		    "sockfs: inherit options failed error %d: opt 0x%x, "
		    "s %d, r %d: %s\n",
		    error, options, so->so_sndbuf, so->so_rcvbuf,
		    pr_state(so->so_state, so->so_mode));
#endif /* DEBUG */
		return;
	}
	ASSERT(mp);
	/* No need to verify T_optmgmt_ack */
	freemsg(mp);
}

/*
 * Copy all SOL_SOCKET options using a setsockopt per option.
 */
static void
soinheritoptions_single(struct sonode *so, struct sonode *nso)
{
	int options = so->so_options;
	int on = 1;

	ASSERT(!(options & SO_ACCEPTCONN));

	if (options & SO_DEBUG)
		(void) sosetsockopt(nso, SOL_SOCKET, SO_DEBUG,
			(char *)&on, sizeof (on));
	if (options & SO_REUSEADDR)
		(void) sosetsockopt(nso, SOL_SOCKET, SO_REUSEADDR,
			(char *)&on, sizeof (on));
	if (options & SO_KEEPALIVE)
		(void) sosetsockopt(nso, SOL_SOCKET, SO_KEEPALIVE,
			(char *)&on, sizeof (on));
	if (options & SO_DONTROUTE)
		(void) sosetsockopt(nso, SOL_SOCKET, SO_DONTROUTE,
			(char *)&on, sizeof (on));
	if (options & SO_BROADCAST)
		(void) sosetsockopt(nso, SOL_SOCKET, SO_BROADCAST,
			(char *)&on, sizeof (on));
	if (options & SO_USELOOPBACK)
		(void) sosetsockopt(nso, SOL_SOCKET, SO_USELOOPBACK,
			(char *)&on, sizeof (on));
	if (options & SO_OOBINLINE)
		(void) sosetsockopt(nso, SOL_SOCKET, SO_OOBINLINE,
			(char *)&on, sizeof (on));
	if (options & SO_DGRAM_ERRIND)
		(void) sosetsockopt(nso, SOL_SOCKET, SO_DGRAM_ERRIND,
			(char *)&on, sizeof (on));

	if (options & SO_LINGER)
		(void) sosetsockopt(nso, SOL_SOCKET, SO_LINGER,
			(char *)&so->so_linger, sizeof (so->so_linger));


	if (so->so_sndbuf != 0)
		(void) sosetsockopt(nso, SOL_SOCKET, SO_SNDBUF,
			(char *)&so->so_sndbuf, sizeof (int));
	if (so->so_rcvbuf != 0)
		(void) sosetsockopt(nso, SOL_SOCKET, SO_RCVBUF,
			(char *)&so->so_rcvbuf, sizeof (int));
#ifdef notyet
	if (so->so_sndlowat != 0)
		(void) sosetsockopt(nso, SOL_SOCKET, SO_SNDLOWAT,
			(char *)&so->so_sndlowat, sizeof (int));
	if (so->so_rcvlowat != 0)
		(void) sosetsockopt(nso, SOL_SOCKET, SO_RCVLOWAT,
			(char *)&so->so_rcvlowat, sizeof (int));
	if (so->so_sndtimeo != 0)
		(void) sosetsockopt(nso, SOL_SOCKET, SO_SNDTIMEO,
			(char *)&so->so_sndtimeo, sizeof (int));
	if (so->so_rcvtimeo != 0)
		(void) sosetsockopt(nso, SOL_SOCKET, SO_RCVTIMEO,
			(char *)&so->so_rcvtimeo, sizeof (int));
#endif /* notyet */
}

int
soaccept(struct sonode *so, int fflag, struct sonode **nsop)
{
	struct T_conn_ind	*conn_ind;
	struct T_conn_res	conn_res;
	int			error = 0;
	mblk_t			*mp;
	struct sonode		*nso;
	vnode_t			*nvp;
	queue_t			*q;
	struct sockaddr		*src;
	tpi_length_t		srclen;
	char			*opt;
	tpi_length_t		optlen;

	dprintso(so, 1, ("soaccept(0x%x, %x, 0x%x) %s\n",
		so, fflag, nsop, pr_state(so->so_state, so->so_mode)));

	/*
	 * Defer single-threading the accepting socket until
	 * the T_CONN_IND has been received and parsed and the
	 * new sonode has been opened.
	 */

	/*
	 * Check that we are not already connected
	 */
	if ((so->so_state & SS_ACCEPTCONN) == 0) {
		/* Note: SunOS 4/BSD unconditionally returns EINVAL here */
		if (so->so_type == SOCK_DGRAM || so->so_type == SOCK_RAW)
			error = EOPNOTSUPP;
		else
			error = EINVAL;
		eprintsoline(so, error);
		return (error);
	}
	error = sowaitconnind(so, fflag, &mp);
	if (error) {
		eprintsoline(so, error);
		return (error);
	}
	ASSERT(mp);
	conn_ind = (struct T_conn_ind *)mp->b_rptr;
	/*
	 * Save SEQ_number for error paths.
	 */
	conn_res.SEQ_number = conn_ind->SEQ_number;

	srclen = conn_ind->SRC_length;
	src = (struct sockaddr *)sogetoff(mp, conn_ind->SRC_offset, srclen, 1);
	if (src == NULL) {
		error = EPROTO;
		eprintsoline(so, error);
		goto disconnect_unlocked;
	}
	optlen = conn_ind->OPT_length;
	if (optlen != 0) {
		opt = (char *)sogetoff(mp, conn_ind->OPT_offset, optlen,
					ALIGN_TPI_opthdr_size);
		if (opt == NULL) {
			error = EPROTO;
			eprintsoline(so, error);
			goto disconnect_unlocked;
		}
	}
	if (so->so_family == AF_UNIX) {
		if (!(so->so_state & SS_FADDR_NOXLATE)) {
			src = NULL;
			srclen = 0;
		}
		if (optlen != 0) {
			/*
			 * Extract source address from options
			 */
			so_getopt_srcaddr(opt, optlen, (char **)&src, &srclen);
		}
	}
	/*
	 * Create the new socket.
	 */
	VN_HOLD(so->so_accessvp);
	nso = socreate(so->so_accessvp, so->so_family, so->so_type,
			so->so_protocol, so->so_version, so, &error);
	if (nso == NULL) {
		/*
		 * Accept can not fail with ENOBUFS. socreate sleeps waiting
		 * for memory until a signal is caught so return EINTR.
		 */
		if (error == ENOBUFS)
			error = EINTR;
		eprintsoline(so, error);
		goto disconnect_unlocked;
	}
	nvp = SOTOV(nso);
#ifdef DEBUG
	/*
	 * SO_DEBUG is used to trigger the dprint* and eprint* macros thus
	 * it's inherited early to allow debugging of the accept code itself.
	 */
	nso->so_options |= so->so_options & SO_DEBUG;
#endif /* DEBUG */

	/*
	 * Save the SRC address from the T_CONN_IND
	 * for getpeername to work on AF_UNIX and on transports that do not
	 * support TI_GETPEERNAME.
	 *
	 * NOTE: AF_UNIX NUL termination is ensured by the sender's
	 * copyin_name().
	 */
	if (srclen > nso->so_faddr_maxlen) {
		error = EINVAL;
		eprintsoline(so, error);
		goto disconnect_vp_unlocked;
	}
	nso->so_faddr_len = srclen;
	ASSERT(so->so_faddr_len <= so->so_faddr_maxlen);
	bcopy((char *)src, (char *)nso->so_faddr_sa, srclen);

	/*
	 * New socket must be bound at least in sockfs and, except for AF_INET,
	 * it also has to be bound in the transport provider.
	 * After accepting the connection on nso so_laddr_sa will be set to
	 * contain the same address as the listener's local address
	 * so the address we bind to isn't important.
	 */
	if (nso->so_family == AF_INET && nso->so_type == SOCK_STREAM &&
	    !soaccept_tpi_tcp) {
		/*
		 * Optimization for AF_INET transports
		 * that can handle a T_CONN_RES without being bound.
		 */
		mutex_enter(&nso->so_lock);
		so_automatic_bind(nso);
		mutex_exit(&nso->so_lock);
	} else {
		/*
		 * Perform NULL bind with the transport provider.
		 */
		error = sobind(nso, NULL, 0, 0, _SOBIND_UNSPEC);
		if (error) {
			ASSERT(error != ENOBUFS);
			eprintsoline(nso, error);
			goto disconnect_vp_unlocked;
		}
	}

	/*
	 * Inherit SIOCSPGRP, SS_ASYNC before we send the T_CONN_RES
	 * so that any data arriving on the new socket will cause the
	 * appropriate signals to be delivered for the new socket.
	 *
	 * No other thread (except strsock_proto and strsock_misc)
	 * can access the new socket thus we relax the locking.
	 */
	nso->so_pgrp = so->so_pgrp;
	nso->so_state |= so->so_state & (SS_ASYNC|SS_FADDR_NOXLATE);

	if (nso->so_pgrp) {
		struct strsigset ss;
		int retval;

		ss.ss_pid = nso->so_pgrp;
		ss.ss_events = S_RDBAND | S_BANDURG;
		if (so->so_state & SS_ASYNC)
			ss.ss_events |= S_RDNORM | S_OUTPUT;

		error = strioctl(nvp, I_ESETSIG, (intptr_t)&ss, 0,
				K_TO_K, CRED(), &retval);
		if (error) {
			eprintsoline(nso, error);
			error = 0;
			nso->so_pgrp = NULL;
		}
	}

	/*
	 * Find read queue in driver
	 * Can safely do this since we "own" nso/nvp.
	 */
	q = strvp2wq(nvp)->q_next;
	while (SAMESTR(q))
		q = q->q_next;
	q = RD(q);

	/* Must single-thread the sending of the T_CONN_RES message */
	mutex_enter(&so->so_lock);
	(void) so_lock_single(so, SLOCKED, 0);
	mutex_exit(&so->so_lock);

	/*
	 * Copy local address from listener.
	 */
	nso->so_laddr_len = so->so_laddr_len;
	ASSERT(nso->so_laddr_len <= nso->so_laddr_maxlen);
	bcopy((char *)so->so_laddr_sa, (char *)nso->so_laddr_sa,
	    nso->so_laddr_len);

	/*
	 * Send T_CONN_RES
	 */
	conn_res.PRIM_type = T_CONN_RES;
	conn_res.QUEUE_ptr = q;
	conn_res.OPT_length = 0;
	conn_res.OPT_offset = 0;
	conn_res.SEQ_number = conn_ind->SEQ_number;

	freemsg(mp);

	mp = soallocproto1((char *)&conn_res, sizeof (conn_res),
	    0, _ALLOC_INTR);
	if (mp == NULL) {
		/*
		 * Accept can not fail with ENOBUFS. A signal was caught
		 * so return EINTR.
		 */
		error = EINTR;
		eprintsoline(so, error);
		mutex_enter(&so->so_lock);
		goto disconnect_vp;
	}
	error = kstrputmsg(SOTOV(so), mp, NULL, 0, 0,
			MSG_BAND|MSG_HOLDSIG|MSG_IGNERROR, 0);
	mp = NULL;
	mutex_enter(&so->so_lock);
	if (error) {
		eprintsoline(so, error);
		goto disconnect_vp;
	}
	error = sowaitokack(so, T_CONN_RES);
	if (error) {
		eprintsoline(so, error);
		goto disconnect_vp;
	}
	so_unlock_single(so, SLOCKED);
	mutex_exit(&so->so_lock);

	mutex_enter(&nso->so_lock);
	soisconnected(nso);
	mutex_exit(&nso->so_lock);

	/*
	 * Copy options from listener to acceptor
	 */
	soinheritoptions(so, nso);

	/*
	 * Pass out new socket.
	 */
	if (nsop != NULL)
		*nsop = nso;

	return (0);

disconnect_vp_unlocked:
	(void) VOP_CLOSE(nvp, 0, 1, 0, CRED());
	VN_RELE(nvp);
disconnect_unlocked:
	(void) sodisconnect(so, conn_res.SEQ_number, 0);
	freemsg(mp);
	return (error);


disconnect_vp:
	(void) sodisconnect(so, conn_res.SEQ_number, _SODISCONNECT_LOCK_HELD);
	so_unlock_single(so, SLOCKED);
	mutex_exit(&so->so_lock);
	(void) VOP_CLOSE(nvp, 0, 1, 0, CRED());
	VN_RELE(nvp);
	freemsg(mp);
	return (error);
}

/*
 * connect a socket.
 *
 * Allow SOCK_DGRAM sockets to reconnect (by specifying a new address) and to
 * unconnect (by specifying a null address).
 */
int
soconnect(struct sonode *so, struct sockaddr *name, int namelen, int fflag,
	int flags)
{
	struct T_conn_req	conn_req;
	int			error = 0;
	mblk_t			*mp;
	char			*src;
	tpi_length_t		srclen;
	char			*addr;
	tpi_length_t		addrlen;

	dprintso(so, 1, ("soconnect(%x, %x, %d, %x, %d) %s\n",
		so, name, namelen, fflag, flags,
		pr_state(so->so_state, so->so_mode)));

	mutex_enter(&so->so_lock);
	(void) so_lock_single(so, SLOCKED, 0);

	/*
	 * Can't have done a listen before connecting.
	 */
	if (so->so_state & SS_ACCEPTCONN) {
		error = EOPNOTSUPP;
		goto done;
	}

	/*
	 * Must be bound with the transport
	 */
	if (!(so->so_state & SS_ISBOUND)) {
		if (so->so_family == AF_INET && so->so_type == SOCK_STREAM &&
		    !soconnect_tpi_tcp) {
			/*
			 * Optimization for AF_INET transports
			 * that can handle a T_CONN_REQ without being bound.
			 */

			/*
			 * Make sure that there is a preallocated unbind_req
			 * message before binding. This message allocated when
			 * the socket is created  but it might be have been
			 * consumed.
			 */
			if (so->so_unbind_mp == NULL) {
				dprintso(so, 1,
				    ("soconnect: allocating unbind_req\n"));
				/* NOTE: holding so_lock while sleeping */
				so->so_unbind_mp =
				    soallocproto(sizeof (struct T_unbind_req),
				    _ALLOC_INTR);
				if (so->so_unbind_mp == NULL) {
					error = EINTR;
					goto done;
				}
			}
			so_automatic_bind(so);
		} else {
			error = sobind(so, NULL, 0, 0,
					_SOBIND_UNSPEC|_SOBIND_LOCK_HELD);
			if (error)
				goto done;
		}
		ASSERT(so->so_state & SS_ISBOUND);
		flags |= _SOCONNECT_DID_BIND;
	}

	/*
	 * Check that we are not already connected.
	 * For a connection-oriented socket we can "unconnect" by connecting
	 * to a NULL address; Otherwise this is an error.
	 * A connected connection-less socket can be
	 * - connected to a different address by a subsequent connect
	 * - "unconnected" by a connect to the NULL address
	 */
	if (so->so_state & (SS_ISCONNECTED|SS_ISCONNECTING)) {
		ASSERT(!(flags & _SOCONNECT_DID_BIND));
		if (so->so_mode & SM_CONNREQUIRED) {
			/* Connection-oriented socket */
			if (name == NULL) {
				/* XXX What about implicitly unbinding here? */
				error = sodisconnect(so, -1,
						_SODISCONNECT_LOCK_HELD);
			} else {
				if (so->so_state & SS_ISCONNECTED)
					error = EISCONN;
				else
					error = EALREADY;
			}
			goto done;
		}
		/* Connection-less socket */
		if (name == NULL) {
			/*
			 * Remove the connected state and clear SO_DGRAM_ERRIND
			 * since it was set when the socket was connected.
			 */
			int val;

			so->so_state &= ~(SS_ISCONNECTED|SS_ISCONNECTING);
			so->so_faddr_len = 0;

			so_unlock_single(so, SLOCKED);
			mutex_exit(&so->so_lock);

			val = 0;
			(void) sosetsockopt(so, SOL_SOCKET, SO_DGRAM_ERRIND,
					(char *)&val, sizeof (val));

			mutex_enter(&so->so_lock);
			(void) so_lock_single(so, SLOCKED, 0);
			goto done;
		}
	}
	ASSERT(so->so_state & SS_ISBOUND);

	if (name == NULL || namelen == 0 || namelen > so->so_faddr_maxlen) {
		error = EINVAL;
		goto done;
	}
	/*
	 * Save foreign address. Needed for AF_UNIX as well as
	 * transport providers that do not support TI_GETPEERNAME.
	 */
	so->so_faddr_len = namelen;
	ASSERT(so->so_faddr_len <= so->so_faddr_maxlen);
	bcopy((char *)name, (char *)so->so_faddr_sa, namelen);

	/*
	 * Mark the socket if so_faddr_sa represents the transport level
	 * address.
	 */
	if (flags & _SOCONNECT_NOXLATE) {
		ASSERT(so->so_family == AF_UNIX);
		so->so_state |= SS_FADDR_NOXLATE;
	}

	/*
	 * Length and family checks.
	 */
	error = so_addr_verify(so, so->so_faddr_sa, so->so_faddr_len);
	if (error) {
		eprintsoline(so, error);
		goto done;
	}
	if (so->so_family == AF_UNIX) {
		if (so->so_state & SS_FADDR_NOXLATE) {
			/*
			 * Already have a transport internal address. Do not
			 * pass any (transport internal) source address.
			 */
			addr = (char *)so->so_faddr_sa;
			addrlen = so->so_faddr_len;
			src = NULL;
			srclen = 0;
		} else {
			/*
			 * Pass the sockaddr_un source address as an option
			 * and translate the remote address.
			 * Holding so_lock thus so_laddr_sa can not change.
			 */
			src = (char *)so->so_laddr_sa;
			srclen = so->so_laddr_len;
			dprintso(so, 1,
				("soconnect UNIX: srclen %d, src 0x%x\n",
				srclen, src));
			error = so_ux_addr_xlate(so,
				so->so_faddr_sa, so->so_faddr_len,
				(flags & _SOCONNECT_XPG4_2),
				&addr, &addrlen);
			if (error) {
				eprintsoline(so, error);
				goto done;
			}
		}
	} else {
		addr = (char *)so->so_faddr_sa;
		addrlen = so->so_faddr_len;
		src = NULL;
		srclen = 0;
	}
	/*
	 * When connecting a datagram socket we issue the SO_DGRAM_ERRIND
	 * option which asks the transport provider to send T_UDERR_IND
	 * messages. These T_UDERR_IND messages are used to return connected
	 * style errors (e.g. ECONNRESET) for connected datagram sockets.
	 *
	 * In addition, for UDP (and SOCK_RAW AF_INET sockets)
	 * we send down a T_CONN_REQ. This is needed to let the
	 * transport assign a local address that is consistent with
	 * the remote address. Applications depend on a getsockname()
	 * after a connect() to retrieve the "source" IP address for
	 * the connected socket.
	 */
	if (!(so->so_mode & SM_CONNREQUIRED)) {
		/*
		 * Datagram socket.
		 */
		int val;

		so_unlock_single(so, SLOCKED);
		mutex_exit(&so->so_lock);

		val = 1;
		(void) sosetsockopt(so, SOL_SOCKET, SO_DGRAM_ERRIND,
					(char *)&val, sizeof (val));

		mutex_enter(&so->so_lock);
		(void) so_lock_single(so, SLOCKED, 0);
		if (so->so_family == AF_INET &&
		    (so->so_type == SOCK_DGRAM || so->so_type == SOCK_RAW) &&
		    !soconnect_tpi_udp) {
			/*
			 * Send down T_CONN_REQ etc.
			 * Clear fflag to avoid returning EWOULDBLOCK.
			 */
			fflag = 0;
		} else {
			soisconnected(so);
			goto done;
		}
	}

	/*
	 * Send down T_CONN_REQ
	 */
	conn_req.PRIM_type = T_CONN_REQ;
	conn_req.DEST_length = addrlen;
	conn_req.DEST_offset = sizeof (conn_req);
	if (srclen == 0) {
		conn_req.OPT_length = 0;
		conn_req.OPT_offset = 0;
		/* NOTE: holding so_lock while sleeping */
		mp = soallocproto2((char *)&conn_req, sizeof (conn_req),
		    addr, addrlen, 0, _ALLOC_INTR);
		if (mp == NULL) {
			/*
			 * Connect can not fail with ENOBUFS. A signal was
			 * caught so return EINTR.
			 */
			error = EINTR;
			eprintsoline(so, error);
			goto done;
		}
	} else {
		/*
		 * There is a AF_UNIX sockaddr_un to include as a source
		 * address option.
		 */
		struct T_opthdr toh;
		int size;

		toh.level = SOL_SOCKET;
		toh.name = SO_SRCADDR;
		toh.len = srclen + sizeof (struct T_opthdr);
		toh.status = 0;
		conn_req.OPT_length = sizeof (toh) + ROUNDUP_tpi_opt(srclen);
		conn_req.OPT_offset = sizeof (conn_req) +
			ROUNDUP_tpi_opt(addrlen);

		size = conn_req.OPT_offset + conn_req.OPT_length;
		/* NOTE: holding so_lock while sleeping */
		mp = soallocproto2((char *)&conn_req, sizeof (conn_req),
		    addr, addrlen, size, _ALLOC_INTR);
		if (mp == NULL) {
			/*
			 * Connect can not fail with ENOBUFS. A signal was
			 * caught so return EINTR.
			 */
			error = EINTR;
			eprintsoline(so, error);
			goto done;
		}
		mp->b_wptr += ROUNDUP_tpi_opt(addrlen) - addrlen;
		soappendmsg(mp, (char *)&toh, sizeof (toh));
		soappendmsg(mp, src, srclen);
		mp->b_wptr += ROUNDUP_tpi_opt(srclen) - srclen;
		ASSERT(mp->b_wptr <= mp->b_datap->db_lim);
	}
	/*
	 * Set SS_ISCONNECTING before sending down the T_CONN_REQ
	 * in order to have the right state when the T_CONN_CON shows up.
	 */
	soisconnecting(so);
	mutex_exit(&so->so_lock);

#ifdef C2_AUDIT
	if (audit_active)
		audit_sock(T_CONN_REQ, strvp2wq(SOTOV(so)), mp, 0);
#endif /* C2_AUDIT */

	error = kstrputmsg(SOTOV(so), mp, NULL, 0, 0,
			MSG_BAND|MSG_HOLDSIG|MSG_IGNERROR, 0);
	mutex_enter(&so->so_lock);
	if (error) {
		eprintsoline(so, error);
		goto done;
	}

	error = sowaitokack(so, T_CONN_REQ);
	if (error) {
		eprintsoline(so, error);
		goto done;
	}

	/*
	 * Wait until we get a T_CONN_CON or an error
	 */
	error = sowaitconnected(so, fflag, 0);

done:
	switch (error) {
	case 0:
	case EINPROGRESS:
	case EALREADY:
	case EISCONN:
	case EINTR:
		/* Non-fatal errors */
		break;

	case EHOSTUNREACH:
		if (flags & _SOCONNECT_XPG4_2) {
			/*
			 * X/Open specification contains a requirement that
			 * ENETUNREACH be returned but does not require
			 * EHOSTUNREACH. In order to keep the test suite
			 * happy we mess with the errno here.
			 */
			error = ENETUNREACH;
		}
		/* FALLTHRU */

	default:
		/* clear SS_ISCONNECTING in case it was set */
		so->so_state &= ~SS_ISCONNECTING;
		/* A discon_ind might have already unbound us */
		if ((flags & _SOCONNECT_DID_BIND) &&
		    (so->so_state & SS_ISBOUND)) {
			int err;

			err = sounbind(so, _SOUNBIND_LOCK_HELD);
			/* LINTED - statement has no conseq */
			if (err) {
				eprintsoline(so, err);
			}
		}
		break;
	}
	so_unlock_single(so, SLOCKED);
	mutex_exit(&so->so_lock);
	return (error);
}

int
soshutdown(struct sonode *so, int how)
{
	struct T_ordrel_req	ordrel_req;
	mblk_t			*mp;
	u_int			old_state, state_change;
	int			error = 0;

	dprintso(so, 1, ("soshutdown(0x%x, %d) %s\n",
		so, how, pr_state(so->so_state, so->so_mode)));

	mutex_enter(&so->so_lock);
	(void) so_lock_single(so, SLOCKED, 0);

	/*
	 * SunOS 4.X has no check for datagram sockets.
	 * 5.X checks that it is connected (ENOTCONN)
	 * X/Open requires that we check the connected state.
	 */
	if (!(so->so_state & SS_ISCONNECTED)) {
		if (!xnet_skip_checks) {
			error = ENOTCONN;
			if (xnet_check_print) {
				printf("sockfs: X/Open shutdown check "
					"caused ENOTCONN\n");
			}
		}
		goto done;
	}
	/*
	 * Record the current state and then perform any state changes.
	 * Then use the difference between the old and new states to
	 * determine which messages need to be sent.
	 * This prevents e.g. duplicate T_ORDREL_REQ when there are
	 * duplicate calls to shutdown().
	 */
	old_state = so->so_state;

	switch (how) {
	case 0:
		socantrcvmore(so);
		break;
	case 1:
		socantsendmore(so);
		break;
	case 2:
		socantsendmore(so);
		socantrcvmore(so);
		break;
	default:
		error = EINVAL;
		goto done;
	}

	/*
	 * Assumes that the SS_CANT* flags are never cleared in the above code.
	 */
	state_change = (so->so_state & (SS_CANTRCVMORE|SS_CANTSENDMORE)) -
		(old_state & (SS_CANTRCVMORE|SS_CANTSENDMORE));
	ASSERT((state_change & ~(SS_CANTRCVMORE|SS_CANTSENDMORE)) == 0);

	/*
	 * Get the read lock before potentially flushing data to avoid
	 * problems with the T_EXDATA_IND MSG_PEEK code in sorecvmsg.
	 */
	(void) so_lock_single(so, SREADLOCKED, 0);

	mutex_exit(&so->so_lock);

	switch (state_change) {
	case 0:
		dprintso(so, 1, ("soshutdown: nothing to send in state %x\n",
			so->so_state));
		mutex_enter(&so->so_lock);
		so_unlock_single(so, SREADLOCKED);
		goto done;

	case SS_CANTRCVMORE:
		strseteof(SOTOV(so), 1);
		/*
		 * strseteof takes care of read side wakeups,
		 * pollwakeups, and signals.
		 */
		/* Flush read side queue */
		strflushrq(SOTOV(so), FLUSHALL);
		break;

	case SS_CANTSENDMORE:
		strsetwerror(SOTOV(so), 0, 0, sogetwrerr);
		break;

	case SS_CANTSENDMORE|SS_CANTRCVMORE:
		strsetwerror(SOTOV(so), 0, 0, sogetwrerr);
		strseteof(SOTOV(so), 1);
		/*
		 * strseteof takes care of read side wakeups,
		 * pollwakeups, and signals.
		 */
		/* Flush read side queue */
		strflushrq(SOTOV(so), FLUSHALL);
		break;
	}
	mutex_enter(&so->so_lock);
	so_unlock_single(so, SREADLOCKED);

	/*
	 * If either SS_CANTSENDMORE or SS_CANTRCVMORE or both of them
	 * was set due to this call and the new state has both of them set:
	 *	Send the AF_UNIX close indication
	 *	For T_COTS send a discon_ind
	 *
	 * If cantsend was set due to this call:
	 *	For T_COTSORD send an ordrel_ind
	 *
	 * Note that for T_CLTS there is no message sent here.
	 */
	if ((so->so_state & (SS_CANTRCVMORE|SS_CANTSENDMORE)) ==
	    (SS_CANTRCVMORE|SS_CANTSENDMORE)) {
		/*
		 * For SunOS 4.X compatibility we tell the other end
		 * that we are unable to receive at this point.
		 */
		if (so->so_family == AF_UNIX && so->so_serv_type != T_CLTS)
			so_unix_close(so);

		if (so->so_serv_type == T_COTS)
			error = sodisconnect(so, -1, _SODISCONNECT_LOCK_HELD);
	}
	if ((state_change & SS_CANTSENDMORE) &&
	    (so->so_serv_type == T_COTS_ORD)) {
		/* Send an orderly release */
		ordrel_req.PRIM_type = T_ORDREL_REQ;

		mutex_exit(&so->so_lock);
		mp = soallocproto1((char *)&ordrel_req, sizeof (ordrel_req),
		    0, _ALLOC_SLEEP);
		/*
		 * Send down the T_ORDREL_REQ even if there is flow control.
		 * This prevents shutdown from blocking.
		 * Note that there is no T_OK_ACK for ordrel_req.
		 */
		error = kstrputmsg(SOTOV(so), mp, NULL, 0, 0,
			MSG_BAND|MSG_HOLDSIG|MSG_IGNERROR|MSG_IGNFLOW, 0);
		mutex_enter(&so->so_lock);
		if (error) {
			eprintsoline(so, error);
			goto done;
		}
	}

done:
	so_unlock_single(so, SLOCKED);
	mutex_exit(&so->so_lock);
	return (error);
}

/*
 * For any connected SOCK_STREAM/SOCK_SEQPACKET AF_UNIX socket we send
 * a zero-length T_OPTDATA_REQ with the SO_UNIX_CLOSE option to inform the peer
 * that we have closed.
 * Also, for connected AF_UNIX SOCK_DGRAM sockets we send a zero-length
 * T_UNITDATA_REQ containing the same option.
 *
 * For SOCK_DGRAM half-connections (somebody connected to this end
 * but this end is not connect) we don't know where to send any
 * SO_UNIX_CLOSE.
 *
 * We have to ignore stream head errors just in case there has been
 * a shutdown(output).
 * Ignore any flow control to try to get the message more quickly to the peer.
 * While locally ignoring flow control solves the problem when there
 * is only the loopback transport on the stream it would not provide
 * the correct AF_UNIX socket semantics when one or more modules have
 * been pushed.
 */
void
so_unix_close(struct sonode *so)
{
	int		error;
	struct T_opthdr	toh;
	mblk_t		*mp;

	ASSERT(MUTEX_HELD(&so->so_lock));

	ASSERT(so->so_family == AF_UNIX);

	if ((so->so_state & (SS_ISCONNECTED|SS_ISBOUND)) !=
	    (SS_ISCONNECTED|SS_ISBOUND))
		return;

	dprintso(so, 1, ("so_unix_close(0x%x) %s\n",
		so, pr_state(so->so_state, so->so_mode)));

	toh.level = SOL_SOCKET;
	toh.name = SO_UNIX_CLOSE;
	toh.len = sizeof (struct T_opthdr); /* zero length + header */
	toh.status = 0;

	if (so->so_type == SOCK_STREAM || so->so_type == SOCK_SEQPACKET) {
		struct T_optdata_req tdr;

		tdr.PRIM_type = T_OPTDATA_REQ;
		tdr.DATA_flag = 0;

		tdr.OPT_length = sizeof (toh);
		tdr.OPT_offset = sizeof (tdr);

		/* NOTE: holding so_lock while sleeping */
		mp = soallocproto2((char *)&tdr, sizeof (tdr),
		    (char *)&toh, sizeof (toh), 0, _ALLOC_SLEEP);
	} else {
		struct T_unitdata_req tudr;
		char *addr;
		tpi_length_t	addrlen;
		char *src;
		tpi_length_t	srclen;
		struct T_opthdr toh2;
		int size;

		/* Connecteded DGRAM socket */

		/*
		 * For AF_UNIX the destination address is translated to
		 * an internal name and the source address is passed as
		 * an option.
		 */
		/*
		 * Length and family checks.
		 */
		error = so_addr_verify(so, so->so_faddr_sa, so->so_faddr_len);
		if (error) {
			eprintsoline(so, error);
			return;
		}
		if (so->so_state & SS_FADDR_NOXLATE) {
			/*
			 * Already have a transport internal address. Do not
			 * pass any (transport internal) source address.
			 */
			addr = (char *)so->so_faddr_sa;
			addrlen = so->so_faddr_len;
			src = NULL;
			srclen = 0;
		} else {
			/*
			 * Pass the sockaddr_un source address as an option
			 * and translate the remote address.
			 * Holding so_lock thus so_laddr_sa can not change.
			 */
			src = (char *)so->so_laddr_sa;
			srclen = so->so_laddr_len;
			dprintso(so, 1,
				("so_ux_close: srclen %d, src 0x%x\n",
				srclen, src));
			error = so_ux_addr_xlate(so,
				so->so_faddr_sa, so->so_faddr_len, 0,
				&addr, &addrlen);
			if (error) {
				eprintsoline(so, error);
				return;
			}
		}
		tudr.PRIM_type = T_UNITDATA_REQ;
		tudr.DEST_length = addrlen;
		tudr.DEST_offset = sizeof (tudr);
		if (srclen == 0) {
			tudr.OPT_length = sizeof (toh);
			tudr.OPT_offset = sizeof (tudr) +
				ROUNDUP_tpi_opt(addrlen);

			size = tudr.OPT_offset + tudr.OPT_length;
			/* NOTE: holding so_lock while sleeping */
			mp = soallocproto2((char *)&tudr, sizeof (tudr),
			    addr, addrlen, size, _ALLOC_SLEEP);
			mp->b_wptr += (ROUNDUP_tpi_opt(addrlen) - addrlen);
			soappendmsg(mp, (char *)&toh, sizeof (toh));
		} else {
			/*
			 * There is a AF_UNIX sockaddr_un to include as a
			 * source address option.
			 */
			tudr.OPT_length = 2 * sizeof (toh) +
			    ROUNDUP_tpi_opt(srclen);
			tudr.OPT_offset = sizeof (tudr) +
			    ROUNDUP_tpi_opt(addrlen);

			toh2.level = SOL_SOCKET;
			toh2.name = SO_SRCADDR;
			toh2.len = srclen + sizeof (struct T_opthdr);
			toh2.status = 0;

			size = tudr.OPT_offset + tudr.OPT_length;

			/* NOTE: holding so_lock while sleeping */
			mp = soallocproto2((char *)&tudr, sizeof (tudr),
			    addr, addrlen, size, _ALLOC_SLEEP);
			mp->b_wptr += ROUNDUP_tpi_opt(addrlen) - addrlen;
			soappendmsg(mp, (char *)&toh, sizeof (toh));
			soappendmsg(mp, (char *)&toh2, sizeof (toh2));
			soappendmsg(mp, src, srclen);
			mp->b_wptr += ROUNDUP_tpi_opt(srclen) - srclen;
		}
		ASSERT(mp->b_wptr <= mp->b_datap->db_lim);
	}
	mutex_exit(&so->so_lock);
	error = kstrputmsg(SOTOV(so), mp, NULL, 0, 0,
			MSG_BAND|MSG_HOLDSIG|MSG_IGNERROR|MSG_IGNFLOW, 0);
	mutex_enter(&so->so_lock);
}

/*
 * Handle recv* calls that set MSG_OOB or MSG_OOB together with MSG_PEEK.
 */
int
sorecvoob(struct sonode *so, struct nmsghdr *msg, struct uio *uiop, int flags)
{
	mblk_t		*mp, *nmp;
	int		error;

	dprintso(so, 1, ("sorecvoob(%x, %x, %d)\n", so, msg, flags));

	/*
	 * There is never any oob data with addresses or control since
	 * the T_EXDATA_IND does not carry any options.
	 */
	msg->msg_controllen = 0;
	msg->msg_namelen = 0;

	mutex_enter(&so->so_lock);
	ASSERT(so_verify_oobstate(so));
	if ((so->so_options & SO_OOBINLINE) ||
	    (so->so_state & (SS_OOBPEND|SS_HADOOBDATA)) != SS_OOBPEND) {
		dprintso(so, 1, ("sorecvoob: inline or data consumed\n"));
		mutex_exit(&so->so_lock);
		return (EINVAL);
	}
	if (!(so->so_state & SS_HAVEOOBDATA)) {
		dprintso(so, 1, ("sorecvoob: no data yet\n"));
		mutex_exit(&so->so_lock);
		return (EWOULDBLOCK);
	}
	ASSERT(so->so_oobmsg != NULL);
	mp = so->so_oobmsg;
	if (flags & MSG_PEEK) {
		/*
		 * Since recv* can not return ENOBUFS we can not use dupmsg.
		 * Instead we revert to the consolidation private
		 * allocb_wait plus bcopy.
		 */
		mblk_t *mp1;

		mp1 = allocb_wait(msgdsize(mp), BPRI_MED, STR_NOSIG, NULL);
		ASSERT(mp1);

		while (mp != NULL) {
			int size;

			size = mp->b_wptr - mp->b_rptr;
			bcopy((char *)mp->b_rptr, (char *)mp1->b_wptr, size);
			mp1->b_wptr += size;
			ASSERT(mp1->b_wptr <= mp1->b_datap->db_lim);
			mp = mp->b_cont;
		}
		mp = mp1;
	} else {
		/*
		 * Update the state indicating that the data has been consumed.
		 * Keep SS_OOBPEND set until data is consumed past the mark.
		 */
		so->so_oobmsg = NULL;
		so->so_state ^= SS_HAVEOOBDATA|SS_HADOOBDATA;
	}
	dprintso(so, 1,
		("after recvoob(0x%x): counts %d/%d state %s\n",
		so, so->so_oobsigcnt,
		so->so_oobcnt, pr_state(so->so_state, so->so_mode)));
	ASSERT(so_verify_oobstate(so));
	mutex_exit(&so->so_lock);

	error = 0;
	nmp = mp;
	while (nmp != NULL && uiop->uio_resid > 0) {
		int n = nmp->b_wptr - nmp->b_rptr;

		n = MIN(n, uiop->uio_resid);
		if (n > 0)
			error = uiomove((char *)nmp->b_rptr, n,
					UIO_READ, uiop);
		if (error)
			break;
		nmp = nmp->b_cont;
	}
	freemsg(mp);
	return (error);
}

/*
 * Called by sorecvmsg when reading a non-zero amount of data.
 * In addition, the caller typically verifies that there is some
 * potential state to clear by checking
 *	if (so->so_state & (SS_OOBPEND|SS_HAVEOOBDATA|SS_RCVATMARK))
 * before calling this routine.
 * Note that such a check can be made without holding so_lock since
 * sorecvmsg is single-threaded (using SREADLOCKED) and only sorecvmsg
 * decrements so_oobsigcnt.
 *
 * When data is read *after* the point that all pending
 * oob data has been consumed the oob indication is cleared.
 *
 * This logic keeps select/poll returning POLLRDBAND and
 * SIOCATMARK returning true until we have read past
 * the mark.
 */
static void
sorecv_update_oobstate(struct sonode *so)
{
	mutex_enter(&so->so_lock);
	ASSERT(so_verify_oobstate(so));
	dprintso(so, 1,
		("sorecv_update_oobstate: counts %d/%d state %s\n",
		so->so_oobsigcnt,
		so->so_oobcnt, pr_state(so->so_state, so->so_mode)));
	if (so->so_oobsigcnt == 0) {
		/* No more pending oob indications */
		so->so_state &= ~(SS_OOBPEND|SS_HAVEOOBDATA|SS_RCVATMARK);
		freemsg(so->so_oobmsg);
		so->so_oobmsg = NULL;
	}
	ASSERT(so_verify_oobstate(so));
	mutex_exit(&so->so_lock);
}

/*
 * Receive the next message on the queue.
 * If msg_controllen is non-zero when called the caller is interested in
 * any received control info (options).
 * If msg_namelen is non-zero when called the caller is interested in
 * any received source address.
 * The routine returns with msg_control and msg_name pointing to
 * kmem_alloc'ed memory which the caller has to free.
 */
int
sorecvmsg(struct sonode *so, struct nmsghdr *msg, struct uio *uiop)
{
	union T_primitives	*tpr;
	mblk_t			*mp;
	u_char			pri;
	int			pflag, opflag;
	char			*control;
	int			controllen;
	int			namelen;
	int			so_state = so->so_state; /* Snapshot */
	int			saved_resid;
	int			error;
	rval_t			rval;
	int			flags;
	int			timeout;
	int			first;

	flags = msg->msg_flags;
	msg->msg_flags = 0;

	dprintso(so, 1, ("sorecvmsg(%x, %x, %d) state %s err %d\n",
		so, msg, flags,
		pr_state(so->so_state, so->so_mode), so->so_error));

	/*
	 * If we are not connected because we have never been connected
	 * we return ENOTCONN. If we have been connected (but are no longer
	 * connected) then SS_CANTRCVMORE is set and we let kstrgetmsg return
	 * the EOF.
	 *
	 * An alternative would be to post an ENOTCONN error in stream head
	 * (read+write) and clear it when we're connected. However, that error
	 * would cause incorrect poll/select behavior!
	 */
	if ((so_state & (SS_ISCONNECTED|SS_CANTRCVMORE)) == 0 &&
	    (so->so_mode & SM_CONNREQUIRED)) {
		return (ENOTCONN);
	}

	/*
	 * Note: SunOS 4.X checks uio_resid == 0 before going to sleep (but
	 * after checking that the read queue is empty) and returns zero.
	 * This implementation will sleep (in kstrgetmsg) even if uio_resid
	 * is zero.
	 */

	if (flags & MSG_OOB) {
		/* Check that the transport supports OOB */
		if (!(so->so_mode & SM_EXDATA))
			return (EOPNOTSUPP);
		return (sorecvoob(so, msg, uiop, flags));
	}

	/*
	 * Set msg_controllen and msg_namelen to zero here to make it
	 * simpler in the cases that no control or name is returned.
	 */
	controllen = msg->msg_controllen;
	namelen = msg->msg_namelen;
	msg->msg_controllen = 0;
	msg->msg_namelen = 0;

	dprintso(so, 1, ("sorecvmsg: namelen %d controllen %d\n",
		namelen, controllen));

	/*
	 * Only one reader is allowed at any given time. This is needed
	 * for T_EXDATA handling and, in the future, MSG_WAITALL.
	 *
	 * This is slightly different that BSD behavior in that it fails with
	 * EWOULDBLOCK when using nonblocking io. In BSD the read queue access
	 * is single-threaded using sblock(), which is dropped while waiting
	 * for data to appear. The difference shows up e.g. if one
	 * file descriptor does not have O_NONBLOCK but a dup'ed file descriptor
	 * does use nonblocking io and different threads are reading each
	 * file descriptor. In BSD there would never be an EWOULDBLOCK error
	 * in this case as long as the read queue doesn't get empty.
	 * In this implementation the thread using nonblocking io can
	 * get an EWOULDBLOCK error due to the blocking thread executing
	 * e.g. in the uiomove in kstrgetmsg.
	 * This difference is not believed to be significant.
	 */
	mutex_enter(&so->so_lock);
	error = so_lock_single(so, SREADLOCKED, uiop->uio_fmode);
	mutex_exit(&so->so_lock);
	if (error)
		return (error);

	/*
	 * Tell kstrgetmsg to not inspect the stream head errors until all
	 * queued data has been consumed.
	 * Use a timeout=-1 to wait forever unless MSG_DONTWAIT is set.
	 * Also, If uio_fmode indicates nonblocking kstrgetmsg will not block.
	 *
	 * MSG_WAITALL only applies to M_DATA and T_DATA_IND messages and
	 * to T_OPTDATA_IND that do not contain any user-visible control msg.
	 * Note that MSG_WAITALL set with MSG_PEEK is a noop.
	 */
	pflag = MSG_ANY | MSG_DELAYERROR;
	if (flags & MSG_PEEK) {
		pflag |= MSG_IPEEK;
		flags &= ~MSG_WAITALL;
	}
	if (so->so_mode & SM_ATOMIC)
		pflag |= MSG_DISCARDTAIL;

	if (flags & MSG_DONTWAIT)
		timeout = 0;
	else
		timeout = -1;
	opflag = pflag;
	first = 1;
retry:
	saved_resid = uiop->uio_resid;
	pri = 0;
	mp = NULL;

	error = kstrgetmsg(SOTOV(so), &mp, uiop, &pri, &pflag, timeout, &rval);
	if (error) {
		switch (error) {
		case EINTR:
		case EWOULDBLOCK:
			if (!first)
				error = 0;
			break;
		case ETIME:
			/* Returned from kstrgetmsg when timeout expires */
			if (!first)
				error = 0;
			else
				error = EWOULDBLOCK;
			break;
		default:
			eprintsoline(so, error);
			break;
		}
		mutex_enter(&so->so_lock);
		so_unlock_single(so, SREADLOCKED);
		mutex_exit(&so->so_lock);
		return (error);
	}
	/*
	 * For datagrams the MOREDATA flag is used to set MSG_TRUNC.
	 * For non-datagrams MOREDATA is used to set MSG_EOR.
	 */
	ASSERT(!(rval.r_val1 & MORECTL));
	if ((rval.r_val1 & MOREDATA) && (so->so_mode & SM_ATOMIC))
		msg->msg_flags |= MSG_TRUNC;

	if (mp == NULL) {
		dprintso(so, 1, ("sorecvmsg: got M_DATA\n"));
		/*
		 * 4.3BSD and 4.4BSD clears the mark when peeking across it.
		 * The draft Posix socket spec states that the mark should
		 * not be cleared when peeking. We follow the latter.
		 */
		if ((so->so_state &
		    (SS_OOBPEND|SS_HAVEOOBDATA|SS_RCVATMARK)) &&
		    (uiop->uio_resid != saved_resid) &&
		    !(flags & MSG_PEEK)) {
			sorecv_update_oobstate(so);
		}

		mutex_enter(&so->so_lock);
		/* Set MSG_EOR based on MOREDATA */
		if (!(rval.r_val1 & MOREDATA)) {
			if (so->so_state & SS_SAVEDEOR) {
				msg->msg_flags |= MSG_EOR;
				so->so_state &= ~SS_SAVEDEOR;
			}
		}
		/*
		 * If some data was received (i.e. not EOF) and the
		 * read/recv* has not been satisfied wait for some more.
		 */
		if ((flags & MSG_WAITALL) && !(msg->msg_flags & MSG_EOR) &&
		    uiop->uio_resid != saved_resid && uiop->uio_resid > 0) {
			mutex_exit(&so->so_lock);
			first = 0;
			pflag = opflag | MSG_NOMARK;
			goto retry;
		}
		so_unlock_single(so, SREADLOCKED);
		mutex_exit(&so->so_lock);
		return (0);
	}

	/* strsock_proto has already verified length and alignment */
	tpr = (union T_primitives *)mp->b_rptr;
	dprintso(so, 1, ("sorecvmsg: type %d\n", tpr->type));

	switch (tpr->type) {
	case T_DATA_IND: {
		if ((so->so_state &
		    (SS_OOBPEND|SS_HAVEOOBDATA|SS_RCVATMARK)) &&
		    (uiop->uio_resid != saved_resid) &&
		    !(flags & MSG_PEEK)) {
			sorecv_update_oobstate(so);
		}

		/*
		 * Set msg_flags to MSG_EOR based on
		 * MORE_flag and MOREDATA.
		 */
		mutex_enter(&so->so_lock);
		so->so_state &= ~SS_SAVEDEOR;
		if (!(tpr->data_ind.MORE_flag & 1)) {
			if (!(rval.r_val1 & MOREDATA))
				msg->msg_flags |= MSG_EOR;
			else
				so->so_state |= SS_SAVEDEOR;
		}
		freemsg(mp);
		/*
		 * If some data was received (i.e. not EOF) and the
		 * read/recv* has not been satisfied wait for some more.
		 */
		if ((flags & MSG_WAITALL) && !(msg->msg_flags & MSG_EOR) &&
		    uiop->uio_resid != saved_resid && uiop->uio_resid > 0) {
			mutex_exit(&so->so_lock);
			first = 0;
			pflag = opflag | MSG_NOMARK;
			goto retry;
		}
		so_unlock_single(so, SREADLOCKED);
		mutex_exit(&so->so_lock);
		return (0);
	}
	case T_UNITDATA_IND: {
		char *addr;
		tpi_length_t	addrlen;
		char *abuf;
		tpi_length_t	optlen;
		char *opt;

		if ((so->so_state &
		    (SS_OOBPEND|SS_HAVEOOBDATA|SS_RCVATMARK)) &&
		    (uiop->uio_resid != saved_resid) &&
		    !(flags & MSG_PEEK)) {
			sorecv_update_oobstate(so);
		}

		if (namelen != 0) {
			/* Caller wants source address */
			addrlen = tpr->unitdata_ind.SRC_length;
			addr = (char *)sogetoff(mp,
					tpr->unitdata_ind.SRC_offset,
					addrlen, 1);
			if (addr == NULL) {
				freemsg(mp);
				error = EPROTO;
				eprintsoline(so, error);
				goto err;
			}
			if (so->so_family == AF_UNIX) {
				/*
				 * Can not use the transport level address.
				 * If there is a SO_SRCADDR option carrying
				 * the socket level address it will be
				 * extracted below.
				 */
				addr = NULL;
				addrlen = 0;
			}
		}
		optlen = tpr->unitdata_ind.OPT_length;
		if (optlen != 0) {
			int ncontrollen;

			/*
			 * Extract any source address option.
			 * Determine how large cmsg buffer is needed.
			 */
			opt = (char *)sogetoff(mp,
					tpr->unitdata_ind.OPT_offset,
					optlen, ALIGN_TPI_opthdr_size);

			if (opt == NULL) {
				freemsg(mp);
				error = EPROTO;
				eprintsoline(so, error);
				goto err;
			}
			if (so->so_family == AF_UNIX)
				so_getopt_srcaddr(opt, optlen,
					(char **)&addr, &addrlen);
			ncontrollen = so_cmsglen(mp, opt, optlen,
						!(flags & MSG_XPG4_2));
			if (controllen != 0)
				controllen = ncontrollen;
			else if (ncontrollen != 0)
				msg->msg_flags |= MSG_CTRUNC;
		} else {
			controllen = 0;
		}

		if (namelen != 0) {
			/*
			 * Return address to caller.
			 * Caller handles truncation if length
			 * exceeds msg_namelen.
			 * NOTE: AF_UNIX NUL termination is ensured by
			 * the sender's copyin_name().
			 */
			abuf = kmem_alloc(addrlen, KM_SLEEP);

			bcopy(addr, abuf, addrlen);
			msg->msg_name = abuf;
			msg->msg_namelen = addrlen;
		}

		if (controllen != 0) {
			/*
			 * Return control msg to caller.
			 * Caller handles truncation if length
			 * exceeds msg_controllen.
			 */
			control = kmem_alloc(controllen, KM_SLEEP);

			error = so_opt2cmsg(mp, opt, optlen,
					!(flags & MSG_XPG4_2),
					control, controllen);
			if (error) {
				freemsg(mp);
				if (msg->msg_namelen != 0)
					kmem_free(msg->msg_name,
						msg->msg_namelen);
				kmem_free(control, controllen);
				eprintsoline(so, error);
				goto err;
			}
			msg->msg_control = control;
			msg->msg_controllen = controllen;
		}

		freemsg(mp);
		mutex_enter(&so->so_lock);
		so_unlock_single(so, SREADLOCKED);
		mutex_exit(&so->so_lock);
		return (0);
	}
	case T_OPTDATA_IND: {
		struct T_optdata_req *tdr;
		char *opt;
		tpi_length_t	optlen;

		if ((so->so_state &
		    (SS_OOBPEND|SS_HAVEOOBDATA|SS_RCVATMARK)) &&
		    (uiop->uio_resid != saved_resid) &&
		    !(flags & MSG_PEEK)) {
			sorecv_update_oobstate(so);
		}

		tdr = (struct T_optdata_req *)mp->b_rptr;
		optlen = tdr->OPT_length;
		if (optlen != 0) {
			int ncontrollen;
			/*
			 * Determine how large cmsg buffer is needed.
			 */
			opt = (char *)sogetoff(mp,
					tpr->optdata_ind.OPT_offset,
					optlen, ALIGN_TPI_opthdr_size);

			if (opt == NULL) {
				freemsg(mp);
				error = EPROTO;
				eprintsoline(so, error);
				goto err;
			}
			ncontrollen = so_cmsglen(mp, opt, optlen,
						!(flags & MSG_XPG4_2));
			if (controllen != 0)
				controllen = ncontrollen;
			else if (ncontrollen != 0)
				msg->msg_flags |= MSG_CTRUNC;
		} else {
			controllen = 0;
		}

		if (controllen != 0) {
			/*
			 * Return control msg to caller.
			 * Caller handles truncation if length
			 * exceeds msg_controllen.
			 */
			control = kmem_alloc(controllen, KM_SLEEP);

			error = so_opt2cmsg(mp, opt, optlen,
					!(flags & MSG_XPG4_2),
					control, controllen);
			if (error) {
				freemsg(mp);
				kmem_free(control, controllen);
				eprintsoline(so, error);
				goto err;
			}
			msg->msg_control = control;
			msg->msg_controllen = controllen;
		}

		/*
		 * Set msg_flags to MSG_EOR based on
		 * DATA_flag and MOREDATA.
		 */
		mutex_enter(&so->so_lock);
		so->so_state &= ~SS_SAVEDEOR;
		if (!(tpr->data_ind.MORE_flag & 1)) {
			if (!(rval.r_val1 & MOREDATA))
				msg->msg_flags |= MSG_EOR;
			else
				so->so_state |= SS_SAVEDEOR;
		}
		freemsg(mp);
		/*
		 * If some data was received (i.e. not EOF) and the
		 * read/recv* has not been satisfied wait for some more.
		 * Not possible to wait if control info was received.
		 */
		if ((flags & MSG_WAITALL) && !(msg->msg_flags & MSG_EOR) &&
		    controllen == 0 &&
		    uiop->uio_resid != saved_resid && uiop->uio_resid > 0) {
			mutex_exit(&so->so_lock);
			first = 0;
			pflag = opflag | MSG_NOMARK;
			goto retry;
		}
		so_unlock_single(so, SREADLOCKED);
		mutex_exit(&so->so_lock);
		return (0);
	}
	case T_EXDATA_IND: {
		dprintso(so, 1,
			("sorecvmsg: EXDATA_IND counts %d/%d consumed %d "
			"state %s\n",
			so->so_oobsigcnt, so->so_oobcnt,
			saved_resid - uiop->uio_resid,
			pr_state(so->so_state, so->so_mode)));
		/*
		 * kstrgetmsg handles MSGMARK so there is nothing to
		 * inspect in the T_EXDATA_IND.
		 * strsock_proto makes the stream head queue the T_EXDATA_IND
		 * as a separate message with no M_DATA component. Furthermore,
		 * the stream head does not consolidate M_DATA messages onto
		 * an MSGMARK'ed message ensuring that the T_EXDATA_IND
		 * remains a message by itself. This is needed since MSGMARK
		 * marks both the whole message as well as the last byte
		 * of the message.
		 */
		freemsg(mp);
		ASSERT(uiop->uio_resid == saved_resid);	/* No data */
		if (flags & MSG_PEEK) {
			/*
			 * Even though we are peeking we consume the
			 * T_EXDATA_IND thereby moving the mark information
			 * to SS_RCVATMARK. Then the oob code below will
			 * retry the peeking kstrgetmsg.
			 * Note that the stream head read queue is
			 * never flushed without holding SREADLOCKED
			 * thus the T_EXDATA_IND can not disappear
			 * underneath us.
			 */
			dprintso(so, 1,
				("sorecvmsg: consume EXDATA_IND "
				"counts %d/%d state %s\n",
				so->so_oobsigcnt,
				so->so_oobcnt,
				pr_state(so->so_state, so->so_mode)));

			pflag = MSG_ANY | MSG_DELAYERROR;
			if (so->so_mode & SM_ATOMIC)
				pflag |= MSG_DISCARDTAIL;

			pri = 0;
			mp = NULL;

			error = kstrgetmsg(SOTOV(so), &mp, uiop,
				&pri, &pflag, -1, &rval);
			ASSERT(uiop->uio_resid == saved_resid);

			if (error) {
#ifdef SOCK_DEBUG
				if (error != EWOULDBLOCK && error != EINTR) {
					eprintsoline(so, error);
				}
#endif /* SOCK_DEBUG */
				mutex_enter(&so->so_lock);
				so_unlock_single(so, SREADLOCKED);
				mutex_exit(&so->so_lock);
				return (error);
			}
			ASSERT(mp);
			tpr = (union T_primitives *)mp->b_rptr;
			ASSERT(tpr->type == T_EXDATA_IND);
			freemsg(mp);
		} /* end "if (flags & MSG_PEEK)" */

		/*
		 * Decrement the number of queued and pending oob.
		 *
		 * SS_RCVATMARK is cleared when we read past a mark.
		 * SS_HAVEOOBDATA is cleared when we've read past the
		 * last mark.
		 * SS_OOBPEND is cleared if we've read past the last
		 * mark and no (new) SIGURG has been posted.
		 */
		mutex_enter(&so->so_lock);
		ASSERT(so_verify_oobstate(so));
		ASSERT(so->so_oobsigcnt >= so->so_oobcnt);
		ASSERT(so->so_oobsigcnt > 0);
		so->so_oobsigcnt--;
		ASSERT(so->so_oobcnt > 0);
		so->so_oobcnt--;
		/*
		 * Since the T_EXDATA_IND has been removed from the stream
		 * head, but we have not read data past the mark,
		 * sockfs needs to track that the socket is still at the mark.
		 *
		 * Since no data was received call kstrgetmsg again to wait
		 * for data.
		 */
		so->so_state |= SS_RCVATMARK;
		mutex_exit(&so->so_lock);
		dprintso(so, 1,
			("sorecvmsg: retry EXDATA_IND counts %d/%d state %s\n",
			so->so_oobsigcnt, so->so_oobcnt,
			pr_state(so->so_state, so->so_mode)));
		pflag = opflag;
		goto retry;
	}
	default:
		ASSERT(0);
		freemsg(mp);
		error = EPROTO;
		eprintsoline(so, error);
		goto err;
	}
	/* NOTREACHED */
err:
	mutex_enter(&so->so_lock);
	so_unlock_single(so, SREADLOCKED);
	mutex_exit(&so->so_lock);
	return (error);
}

/*
 * Sending data with options on a datagram socket.
 * Assumes caller has verified that SS_ISBOUND etc. are set.
 */
static int
sosend_dgramcmsg(struct sonode *so, struct sockaddr *name, int namelen,
		struct uio *uiop, char *control, int controllen, int flags)
{
	struct T_unitdata_req	tudr;
	mblk_t			*mp;
	int			error;
	char			*addr;
	tpi_length_t		addrlen;
	char			*src;
	tpi_length_t		srclen;
	int			len;
	int			size;
	struct T_opthdr		toh;
	struct fdbuf		*fdbuf;
	int			optlen;
	char			*fds;
	int			fdlen;

	ASSERT(name && namelen);
	ASSERT(control && controllen);

	len = uiop->uio_resid;
	if (len > so->so_tidu_size) {
		return (EMSGSIZE);
	}

	/*
	 * For AF_UNIX the destination address is translated to an internal
	 * name and the source address is passed as an option.
	 * Also, file descriptors are passed as file pointers in an
	 * option.
	 */

	/*
	 * Length and family checks.
	 */
	error = so_addr_verify(so, name, namelen);
	if (error) {
		eprintsoline(so, error);
		return (error);
	}
	if (so->so_family == AF_UNIX) {
		if (so->so_state & SS_FADDR_NOXLATE) {
			/*
			 * Already have a transport internal address. Do not
			 * pass any (transport internal) source address.
			 */
			addr = (char *)name;
			addrlen = namelen;
			src = NULL;
			srclen = 0;
		} else {
			/*
			 * Pass the sockaddr_un source address as an option
			 * and translate the remote address.
			 *
			 * Note that this code does not prevent so_laddr_sa
			 * from changing while it is being used. Thus
			 * if an unbind+bind occurs concurrently with this
			 * send the peer might see a partially new and a
			 * partially old "from" address.
			 */
			src = (char *)so->so_laddr_sa;
			srclen = so->so_laddr_len;
			dprintso(so, 1,
			    ("sosend_dgramcmsg UNIX: srclen %d, src 0x%x\n",
			    srclen, src));
			error = so_ux_addr_xlate(so, name, namelen,
				(flags & MSG_XPG4_2),
				&addr, &addrlen);
			if (error) {
				eprintsoline(so, error);
				return (error);
			}
		}
	} else {
		addr = (char *)name;
		addrlen = namelen;
		src = NULL;
		srclen = 0;
	}
	optlen = so_optlen(control, controllen, !(flags & MSG_XPG4_2));
	tudr.PRIM_type = T_UNITDATA_REQ;
	tudr.DEST_length = addrlen;
	tudr.DEST_offset = sizeof (tudr);
	if (srclen != 0)
		tudr.OPT_length = optlen + sizeof (toh) +
		    ROUNDUP_tpi_opt(srclen);
	else
		tudr.OPT_length = optlen;
	tudr.OPT_offset = sizeof (tudr) + ROUNDUP_tpi_opt(addrlen);

	size = tudr.OPT_offset + tudr.OPT_length;

	/*
	 * File descriptors only when SM_FDPASSING set.
	 */
	error = so_getfdopt(control, controllen,
			!(flags & MSG_XPG4_2), &fds, &fdlen);
	if (error)
		return (error);
	if (fdlen != -1) {
		if (!(so->so_mode & SM_FDPASSING))
			return (EOPNOTSUPP);

		error = fdbuf_create(fds, fdlen, &fdbuf);
		if (error)
			return (error);
		mp = fdbuf_allocmsg(size, fdbuf);
		if (mp == NULL)
			fdbuf_free(fdbuf);
	} else {
		mp = soallocproto(size, _ALLOC_INTR);
	}
	if (mp == NULL) {
		/*
		 * Caught a signal waiting for memory.
		 * Let send* return EINTR.
		 */
		return (EINTR);
	}
	soappendmsg(mp, (char *)&tudr, sizeof (tudr));
	soappendmsg(mp, addr, addrlen);
	mp->b_wptr += ROUNDUP_tpi_opt(addrlen) - addrlen;

	if (fdlen != -1) {
		ASSERT(fdbuf != NULL);
		toh.level = SOL_SOCKET;
		toh.name = SO_FILEP;
		toh.len = fdbuf->fd_size + sizeof (struct T_opthdr);
		toh.status = 0;
		soappendmsg(mp, (char *)&toh, sizeof (toh));
		soappendmsg(mp, (char *)fdbuf, fdbuf->fd_size);
		ASSERT(ISALIGNED_tpi_opthdr(mp->b_wptr));
	}
	if (srclen != 0) {
		/*
		 * There is a AF_UNIX sockaddr_un to include as a source
		 * address option.
		 */
		toh.level = SOL_SOCKET;
		toh.name = SO_SRCADDR;
		toh.len = srclen + sizeof (struct T_opthdr);
		toh.status = 0;
		soappendmsg(mp, (char *)&toh, sizeof (toh));
		soappendmsg(mp, src, srclen);
		mp->b_wptr += ROUNDUP_tpi_opt(srclen) - srclen;
		ASSERT(ISALIGNED_tpi_opthdr(mp->b_wptr));
	}
	ASSERT(mp->b_wptr <= mp->b_datap->db_lim);
	so_cmsg2opt(control, controllen, !(flags & MSG_XPG4_2), mp);
	/* At most 3 bytes left in the message */
	ASSERT(mp->b_wptr - mp->b_rptr > size - ALIGN_TPI_size);
	ASSERT(mp->b_wptr - mp->b_rptr <= size);

	ASSERT(mp->b_wptr <= mp->b_datap->db_lim);
#ifdef C2_AUDIT
	if (audit_active)
		audit_sock(T_UNITDATA_REQ, strvp2wq(SOTOV(so)), mp, 0);
#endif /* C2_AUDIT */

	error = kstrputmsg(SOTOV(so), mp, uiop, len, 0, MSG_BAND, 0);
#ifdef SOCK_DEBUG
	if (error) {
		eprintsoline(so, error);
	}
#endif /* SOCK_DEBUG */
	return (error);
}

/*
 * Sending data with options on a connected stream socket.
 * Assumes caller has verified that SS_ISCONNECTED is set.
 */
static int
sosend_svccmsg(struct sonode *so, struct uio *uiop, int more,
		char *control, int controllen, int flags)
{
	struct T_optdata_req	tdr;
	mblk_t			*mp;
	int			error;
	int			iosize;
	int			first = 1;
	int			size;
	struct fdbuf		*fdbuf;
	int			optlen;
	char			*fds;
	int			fdlen;
	struct T_opthdr		toh;

	dprintso(so, 1,
		("sosend_svccmsg: resid %d bytes\n", uiop->uio_resid));

	/*
	 * Has to be bound and connected. However, since no locks are
	 * held the state could have changed after sosendmsg checked it
	 * thus it is not possible to ASSERT on the state.
	 */

	/* Options on connection-oriented only when SM_OPTDATA set. */
	if (!(so->so_mode & SM_OPTDATA))
		return (EOPNOTSUPP);

	do {
		/*
		 * Set the MORE flag if uio_resid does not fit in this
		 * message or if the caller passed in "more".
		 * Error for transports with zero tidu_size.
		 */
		tdr.PRIM_type = T_OPTDATA_REQ;
		iosize = so->so_tidu_size;
		if (iosize <= 0)
			return (EMSGSIZE);
		if (uiop->uio_resid > iosize) {
			tdr.DATA_flag = 1;
		} else {
			if (more)
				tdr.DATA_flag = 1;
			else
				tdr.DATA_flag = 0;
			iosize = uiop->uio_resid;
		}
		dprintso(so, 1, ("sosend_svccmsg: sending %d, %d bytes\n",
			tdr.DATA_flag, iosize));

		optlen = so_optlen(control, controllen, !(flags & MSG_XPG4_2));
		tdr.OPT_length = optlen;
		tdr.OPT_offset = sizeof (tdr);

		size = sizeof (tdr) + optlen;
		/*
		 * File descriptors only when SM_FDPASSING set.
		 */
		error = so_getfdopt(control, controllen,
				!(flags & MSG_XPG4_2), &fds, &fdlen);
		if (error)
			return (error);
		if (fdlen != -1) {
			if (!(so->so_mode & SM_FDPASSING))
				return (EOPNOTSUPP);

			error = fdbuf_create(fds, fdlen, &fdbuf);
			if (error)
				return (error);
			mp = fdbuf_allocmsg(size, fdbuf);
			if (mp == NULL)
				fdbuf_free(fdbuf);
		} else {
			mp = soallocproto(size, _ALLOC_INTR);
		}

		if (mp == NULL) {
			/*
			 * Caught a signal waiting for memory.
			 * Let send* return EINTR.
			 */
			if (first)
				return (EINTR);
			else
				return (0);
		}
		soappendmsg(mp, (char *)&tdr, sizeof (tdr));

		if (fdlen != -1) {
			ASSERT(fdbuf != NULL);
			toh.level = SOL_SOCKET;
			toh.name = SO_FILEP;
			toh.len = fdbuf->fd_size + sizeof (struct T_opthdr);
			toh.status = 0;
			soappendmsg(mp, (char *)&toh, sizeof (toh));
			soappendmsg(mp, (char *)fdbuf, fdbuf->fd_size);
			ASSERT(ISALIGNED_tpi_opthdr(mp->b_wptr));
		}
		so_cmsg2opt(control, controllen, !(flags & MSG_XPG4_2), mp);
		/* At most 3 bytes left in the message */
		ASSERT(mp->b_wptr - mp->b_rptr > size - ALIGN_TPI_size);
		ASSERT(mp->b_wptr - mp->b_rptr <= size);

		ASSERT(mp->b_wptr <= mp->b_datap->db_lim);

		error = kstrputmsg(SOTOV(so), mp, uiop, iosize,
					0, MSG_BAND, 0);
		if (error) {
			if (!first && error == EWOULDBLOCK)
				return (0);
			eprintsoline(so, error);
			return (error);
		}
		control = NULL;
		first = 0;
		if (uiop->uio_resid > 0) {
			/*
			 * Recheck for fatal errors. Fail write even though
			 * some data have been written. This is consistent
			 * with strwrite semantics and BSD sockets semantics.
			 */
			if (so->so_state & SS_CANTSENDMORE) {
				psignal(ttoproc(curthread), SIGPIPE);
				eprintsoline(so, error);
				return (EPIPE);
			}
			if (so->so_error != 0) {
				mutex_enter(&so->so_lock);
				error = sogeterr(so, 0);
				mutex_exit(&so->so_lock);
				if (error != 0) {
					eprintsoline(so, error);
					return (error);
				}
			}
		}
	} while (uiop->uio_resid > 0);
	return (0);
}

/*
 * Sending data on a datagram socket.
 * Assumes caller has verified that SS_ISBOUND etc. are set.
 *
 * For AF_UNIX the destination address is translated to an internal
 * name and the source address is passed as an option.
 */
int
sosend_dgram(struct sonode *so, struct sockaddr *name, int namelen,
		struct uio *uiop, int flags)
{
	struct T_unitdata_req	tudr;
	mblk_t			*mp;
	int			error;
	char			*addr;
	tpi_length_t		addrlen;
	char			*src;
	tpi_length_t		srclen;
	int			len;

	ASSERT(name && namelen);

	len = uiop->uio_resid;
	if (len > so->so_tidu_size) {
		error = EMSGSIZE;
		goto done;
	}

	/*
	 * Length and family checks.
	 */
	error = so_addr_verify(so, name, namelen);
	if (error) {
		eprintsoline(so, error);
		goto done;
	}
	if (so->so_family == AF_UNIX) {
		if (so->so_state & SS_FADDR_NOXLATE) {
			/*
			 * Already have a transport internal address. Do not
			 * pass any (transport internal) source address.
			 */
			addr = (char *)name;
			addrlen = namelen;
			src = NULL;
			srclen = 0;
		} else {
			/*
			 * Pass the sockaddr_un source address as an option
			 * and translate the remote address.
			 *
			 * Note that this code does not prevent so_laddr_sa
			 * from changing while it is being used. Thus
			 * if an unbind+bind occurs concurrently with this
			 * send the peer might see a partially new and a
			 * partially old "from" address.
			 */
			src = (char *)so->so_laddr_sa;
			srclen = so->so_laddr_len;
			dprintso(so, 1,
				("sosend_dgram UNIX: srclen %d, src 0x%x\n",
				srclen, src));
			error = so_ux_addr_xlate(so, name, namelen,
				(flags & MSG_XPG4_2),
				&addr, &addrlen);
			if (error) {
				eprintsoline(so, error);
				goto done;
			}
		}
	} else {
		addr = (char *)name;
		addrlen = namelen;
		src = NULL;
		srclen = 0;
	}
	tudr.PRIM_type = T_UNITDATA_REQ;
	tudr.DEST_length = addrlen;
	tudr.DEST_offset = sizeof (tudr);
	if (srclen == 0) {
		tudr.OPT_length = 0;
		tudr.OPT_offset = 0;

		mp = soallocproto2((char *)&tudr, sizeof (tudr),
		    addr, addrlen, 0, _ALLOC_INTR);
		if (mp == NULL) {
			/*
			 * Caught a signal waiting for memory.
			 * Let send* return EINTR.
			 */
			error = EINTR;
			goto done;
		}
	} else {
		/*
		 * There is a AF_UNIX sockaddr_un to include as a source
		 * address option.
		 */
		struct T_opthdr toh;
		int size;

		tudr.OPT_length = sizeof (toh) + ROUNDUP_tpi_opt(srclen);
		tudr.OPT_offset = sizeof (tudr) + ROUNDUP_tpi_opt(addrlen);

		toh.level = SOL_SOCKET;
		toh.name = SO_SRCADDR;
		toh.len = srclen + sizeof (struct T_opthdr);
		toh.status = 0;

		size = tudr.OPT_offset + tudr.OPT_length;
		mp = soallocproto2((char *)&tudr, sizeof (tudr),
		    addr, addrlen, size, _ALLOC_INTR);
		if (mp == NULL) {
			/*
			 * Caught a signal waiting for memory.
			 * Let send* return EINTR.
			 */
			error = EINTR;
			goto done;
		}
		mp->b_wptr += ROUNDUP_tpi_opt(addrlen) - addrlen;
		soappendmsg(mp, (char *)&toh, sizeof (toh));
		soappendmsg(mp, src, srclen);
		mp->b_wptr += ROUNDUP_tpi_opt(srclen) - srclen;
		ASSERT(mp->b_wptr <= mp->b_datap->db_lim);
	}

#ifdef C2_AUDIT
	if (audit_active)
		audit_sock(T_UNITDATA_REQ, strvp2wq(SOTOV(so)), mp, 0);
#endif /* C2_AUDIT */

	error = kstrputmsg(SOTOV(so), mp, uiop, len, 0, MSG_BAND, 0);
done:
#ifdef SOCK_DEBUG
	if (error) {
		eprintsoline(so, error);
	}
#endif /* SOCK_DEBUG */
	return (error);
}

/*
 * Sending data on a connected stream socket.
 * Assumes caller has verified that SS_ISCONNECTED is set.
 */
int
sosend_svc(struct sonode *so, struct uio *uiop, int prim, int more, int sflag)
{
	struct T_data_req	tdr;
	mblk_t			*mp;
	int			error;
	int			iosize;
	int			first = 1;

	dprintso(so, 1,
		("sosend_svc: 0x%x, resid %d bytes, prim %d, sflag 0x%x\n",
		prim, uiop->uio_resid, prim, sflag));

	/*
	 * Has to be bound and connected. However, since no locks are
	 * held the state could have changed after sosendmsg checked it
	 * thus it is not possible to ASSERT on the state.
	 */

	do {
		/*
		 * Set the MORE flag if uio_resid does not fit in this
		 * message or if the caller passed in "more".
		 * Error for transports with zero tidu_size.
		 */
		tdr.PRIM_type = prim;
		iosize = so->so_tidu_size;
		if (iosize <= 0)
			return (EMSGSIZE);
		if (uiop->uio_resid > iosize) {
			tdr.MORE_flag = 1;
		} else {
			if (more)
				tdr.MORE_flag = 1;
			else
				tdr.MORE_flag = 0;
			iosize = uiop->uio_resid;
		}
		dprintso(so, 1, ("sosend_svc: sending 0x%x %d, %d bytes\n",
			prim, tdr.MORE_flag, iosize));
		mp = soallocproto1((char *)&tdr, sizeof (tdr), 0, _ALLOC_INTR);
		if (mp == NULL) {
			/*
			 * Caught a signal waiting for memory.
			 * Let send* return EINTR.
			 */
			if (first)
				return (EINTR);
			else
				return (0);
		}

		error = kstrputmsg(SOTOV(so), mp, uiop, iosize,
					0, sflag | MSG_BAND, 0);
		if (error) {
			if (!first && error == EWOULDBLOCK)
				return (0);
			eprintsoline(so, error);
			return (error);
		}
		first = 0;
		if (uiop->uio_resid > 0) {
			/*
			 * Recheck for fatal errors. Fail write even though
			 * some data have been written. This is consistent
			 * with strwrite semantics and BSD sockets semantics.
			 */
			if (so->so_state & SS_CANTSENDMORE) {
				psignal(ttoproc(curthread), SIGPIPE);
				eprintsoline(so, error);
				return (EPIPE);
			}
			if (so->so_error != 0) {
				mutex_enter(&so->so_lock);
				error = sogeterr(so, 0);
				mutex_exit(&so->so_lock);
				if (error != 0) {
					eprintsoline(so, error);
					return (error);
				}
			}
		}
	} while (uiop->uio_resid > 0);
	return (0);
}

/*
 * Check the state for errors and call the appropriate send function.
 *
 * If MSG_DONTROUTE is set (and SO_DONTROUTE isn't already set)
 * this function issues a setsockopt to toggle SO_DONTROUTE before and
 * after sending the message.
 */
int
sosendmsg(struct sonode *so, struct nmsghdr *msg, struct uio *uiop)
{
	int		so_state;
	int		so_mode;
	int		error;
	struct sockaddr *name;
	int		namelen;
	int		dontroute;
	int		flags;

	dprintso(so, 1, ("sosendmsg(%x, %x, %d) state %s, error %d\n",
		so, msg, msg->msg_flags,
		pr_state(so->so_state, so->so_mode), so->so_error));

	mutex_enter(&so->so_lock);
	so_state = so->so_state;

	if (so_state & SS_CANTSENDMORE) {
		mutex_exit(&so->so_lock);
		psignal(ttoproc(curthread), SIGPIPE);
		return (EPIPE);
	}

	if (so->so_error != 0) {
		error = sogeterr(so, 0);
		if (error != 0) {
			mutex_exit(&so->so_lock);
			return (error);
		}
	}

	name = (struct sockaddr *)msg->msg_name;
	namelen = msg->msg_namelen;

	so_mode = so->so_mode;

	if (name == NULL) {
		if (!(so_state & SS_ISCONNECTED)) {
			mutex_exit(&so->so_lock);
			if (so_mode & SM_CONNREQUIRED)
				return (ENOTCONN);
			else
				return (EDESTADDRREQ);
		}
		if (so_mode & SM_CONNREQUIRED) {
			name = NULL;
			namelen = 0;
		} else {
			/*
			 * Note that this code does not prevent so_faddr_sa
			 * from changing while it is being used. Thus
			 * if an "unconnect"+connect occurs concurrently with
			 * this send the datagram might be delivered to a
			 * garbaled address.
			 */
			ASSERT(so->so_faddr_sa);
			name = so->so_faddr_sa;
			namelen = so->so_faddr_len;
		}
	} else {
		if (!(so_state & SS_ISCONNECTED) &&
		    (so_mode & SM_CONNREQUIRED)) {
			/* Required but not connected */
			mutex_exit(&so->so_lock);
			return (ENOTCONN);
		}
		/*
		 * Ignore the address on connection-oriented sockets.
		 * Just like BSD this code does not generate an error for
		 * TCP (a CONNREQUIRED socket) when sending to an address
		 * passed in with sendto/sendmsg. Instead the data is
		 * delivered on the connection as if no address had been
		 * supplied.
		 */
		if ((so_state & SS_ISCONNECTED) &&
		    !(so_mode & SM_CONNREQUIRED)) {
			mutex_exit(&so->so_lock);
			return (EISCONN);
		}
		if (!(so_state & SS_ISBOUND)) {
			(void) so_lock_single(so, SLOCKED, 0);
			error = sobind(so, NULL, 0, 0,
				    _SOBIND_UNSPEC|_SOBIND_LOCK_HELD);
			so_unlock_single(so, SLOCKED);
			if (error) {
				mutex_exit(&so->so_lock);
				eprintsoline(so, error);
				return (error);
			}
		}
		/*
		 * Handle delayed datagram errors. These are only queued
		 * when the application sets SO_DGRAM_ERRIND.
		 * Return the error if we are sending to the address
		 * that was returned in the last T_UDERROR_IND.
		 * If sending to some other address discard the delayed
		 * error indication.
		 */
		if (so->so_delayed_error) {
			struct T_uderror_ind	*tudi;
			char			*addr;
			tpi_length_t		addrlen;

			ASSERT(so->so_eaddr_mp);
			error = so->so_delayed_error;
			so->so_delayed_error = 0;
			tudi = (struct T_uderror_ind *)so->so_eaddr_mp->b_rptr;
			addrlen = tudi->DEST_length;
			addr = (char *)sogetoff(so->so_eaddr_mp,
						tudi->DEST_offset,
						addrlen, 1);
			ASSERT(addr);	/* Checked by strsock_proto */
			if (namelen == addrlen &&
			    bcmp((char *)name, addr, namelen) == 0) {
				freemsg(so->so_eaddr_mp);
				so->so_eaddr_mp = NULL;
				mutex_exit(&so->so_lock);
#ifdef DEBUG
				dprintso(so, 0,
					("sockfs delayed error %d for %s\n",
					error,
					pr_addr(so->so_family, name, namelen)));
#endif /* DEBUG */
				return (error);
			}
			freemsg(so->so_eaddr_mp);
			so->so_eaddr_mp = NULL;
		}
	}
	mutex_exit(&so->so_lock);

	flags = msg->msg_flags;
	dontroute = 0;
	if ((flags & MSG_DONTROUTE) && !(so->so_options & SO_DONTROUTE)) {
		int	val;

		val = 1;
		error = sosetsockopt(so, SOL_SOCKET, SO_DONTROUTE,
					(char *)&val, sizeof (val));
		if (error)
			return (error);
		dontroute = 1;
	}

	if ((flags & MSG_OOB) && !(so_mode & SM_EXDATA)) {
		error = EOPNOTSUPP;
		goto done;
	}
	if (msg->msg_controllen != 0) {
		if (!(so_mode & SM_CONNREQUIRED)) {
			error = sosend_dgramcmsg(so, name, namelen, uiop,
				msg->msg_control, msg->msg_controllen,
				flags);
		} else {
			if (flags & MSG_OOB) {
				/* Can't generate T_EXDATA_REQ with options */
				error = EOPNOTSUPP;
				goto done;
			}
			error = sosend_svccmsg(so, uiop,
				!(flags & MSG_EOR),
				msg->msg_control, msg->msg_controllen,
				flags);
		}
		goto done;
	}

	if (!(so_mode & SM_CONNREQUIRED)) {
		/*
		 * If there is no SO_DONTROUTE to turn off return immediately
		 * from sosend_dgram. This can allow tail-call optimizations.
		 */
		if (!dontroute) {
			return (sosend_dgram(so, name, namelen, uiop, flags));
		}
		error = sosend_dgram(so, name, namelen, uiop, flags);
	} else {
		int prim;
		int sflag;

		/* Ignore msg_name in the connected state */
		if (flags & MSG_OOB) {
			prim = T_EXDATA_REQ;
			/*
			 * Send down T_EXDATA_REQ even if there is flow
			 * control for data.
			 */
			sflag = MSG_IGNFLOW;
		} else {
			if (so_mode & SM_BYTESTREAM) {
				/* Byte stream transport - use write */

				dprintso(so, 1, ("sosendmsg: write\n"));
				/*
				 * If there is no SO_DONTROUTE to turn off
				 * return immediately from strwrite. This can
				 * allow tail-call optimizations.
				 */
				if (!dontroute)
					return (strwrite(SOTOV(so), uiop,
							CRED()));
				error = strwrite(SOTOV(so), uiop, CRED());
				goto done;
			}
			prim = T_DATA_REQ;
			sflag = 0;
		}
		/*
		 * If there is no SO_DONTROUTE to turn off return immediately
		 * from sosend_svc. This can allow tail-call optimizations.
		 */
		if (!dontroute)
			return (sosend_svc(so, uiop, prim,
				!(flags & MSG_EOR), sflag));
		error = sosend_svc(so, uiop, prim,
				!(flags & MSG_EOR), sflag);
	}
	ASSERT(dontroute);
done:
	if (dontroute) {
		int	val;

		val = 0;
		(void) sosetsockopt(so, SOL_SOCKET, SO_DONTROUTE,
				(char *)&val, sizeof (val));
	}
	return (error);
}

/*
 * Update so_faddr by asking the transport (unless AF_UNIX).
 */
int
sogetpeername(struct sonode *so)
{
	struct strbuf	strbuf;
	int		error = 0, res;
	char		*addr;
	tpi_length_t	addrlen;
	k_sigset_t	smask;
	void sigintr();
	void sigunintr();

	dprintso(so, 1, ("sogetpeername(0x%x) %s\n",
		so, pr_state(so->so_state, so->so_mode)));

	mutex_enter(&so->so_lock);
	(void) so_lock_single(so, SLOCKED, 0);
	if (!(so->so_state & SS_ISCONNECTED)) {
		error = ENOTCONN;
		goto done;
	}
	/* Added this check for X/Open */
	if ((so->so_state & SS_CANTSENDMORE) && !xnet_skip_checks) {
		error = EINVAL;
		if (xnet_check_print) {
			printf("sockfs: X/Open getpeername check => EINVAL\n");
		}
		goto done;
	}
#ifdef DEBUG
	dprintso(so, 1, ("sogetpeername (local): %s\n",
		pr_addr(so->so_family, so->so_faddr_sa, so->so_faddr_len)));
#endif /* DEBUG */

	if (so->so_family == AF_UNIX) {
		/* Transport has different name space - return local info */
		error = 0;
		goto done;
	}

	ASSERT(so->so_faddr_sa);
	/* Allocate local buffer to use with ioctl */
	addrlen = so->so_faddr_maxlen;
	mutex_exit(&so->so_lock);
	addr = kmem_alloc(addrlen, KM_SLEEP);

	/*
	 * Issue TI_GETPEERNAME with signals masked.
	 * Put the result in so_faddr_sa so that getpeername works after
	 * a shutdown(output).
	 * If the ioctl fails (e.g. due to a ECONNRESET) the error is reposted
	 * back to the socket.
	 */
	strbuf.buf = addr;
	strbuf.maxlen = addrlen;
	strbuf.len = 0;

	sigintr(&smask, 0);
	res = 0;
	ASSERT(CRED());
	error = strioctl(SOTOV(so), TI_GETPEERNAME, (intptr_t)&strbuf,
			0, K_TO_K, CRED(), &res);
	sigunintr(&smask);

	mutex_enter(&so->so_lock);
	/*
	 * If there is an error record the error in so_error put don't fail
	 * the getpeername. Instead fallback on the recorded
	 * so->so_faddr_sa.
	 */
	if (error) {
		/*
		 * Various stream head errors can be returned to the ioctl.
		 * However, it is impossible to determine which ones of
		 * these are really socket level errors that were incorrectly
		 * consumed by the ioctl. Thus this code silently ignores the
		 * error - to code explicitly does not reinstate the error
		 * using soseterror().
		 * Experiments have shows that at least this set of
		 * errors are reported and should not be reinstated on the
		 * socket:
		 *	EINVAL	E.g. if an I_LINK was in effect when
		 *		getpeername was called.
		 *	EPIPE	The ioctl error semantics prefer the write
		 *		side error over the read side error.
		 *	ENOTCONN The transport just got disconnected but
		 *		sockfs had not yet seen the T_DISCON_IND
		 *		when issuing the ioctl.
		 */
		error = 0;
	} else if (res == 0 && strbuf.len > 0 &&
	    (so->so_state & SS_ISCONNECTED)) {
		ASSERT(strbuf.len <= so->so_faddr_maxlen);
		so->so_faddr_len = strbuf.len;
		bcopy(addr, (char *)so->so_faddr_sa, so->so_faddr_len);
	}
	kmem_free(addr, addrlen);
#ifdef DEBUG
	dprintso(so, 1, ("sogetpeername (tp): %s\n",
			pr_addr(so->so_family, so->so_faddr_sa,
				so->so_faddr_len)));
#endif /* DEBUG */
done:
	so_unlock_single(so, SLOCKED);
	mutex_exit(&so->so_lock);
	return (error);
}

/*
 * Update so_laddr by asking the transport (unless AF_UNIX).
 */
int
sogetsockname(struct sonode *so)
{
	struct strbuf	strbuf;
	int		error = 0, res;
	char		*addr;
	tpi_length_t	addrlen;
	k_sigset_t	smask;
	void sigintr();
	void sigunintr();

	dprintso(so, 1, ("sogetsockname(0x%x) %s\n",
		so, pr_state(so->so_state, so->so_mode)));

	mutex_enter(&so->so_lock);
	(void) so_lock_single(so, SLOCKED, 0);
	if (!(so->so_state & SS_ISBOUND) && so->so_family != AF_UNIX) {
		/* Return an all zero address except for the family */
		so->so_laddr_len = sizeof (struct sockaddr);
		ASSERT(so->so_laddr_len <= so->so_laddr_maxlen);
		bzero((char *)so->so_laddr_sa, so->so_laddr_len);
		/*
		 * Can not assume there is a sa_family for all
		 * protocol families.
		 */
		if (so->so_family == AF_INET)
			so->so_laddr_sa->sa_family = so->so_family;
	}
#ifdef DEBUG
	dprintso(so, 1, ("sogetsockname (local): %s\n",
		pr_addr(so->so_family, so->so_laddr_sa, so->so_laddr_len)));
#endif /* DEBUG */
	if (so->so_family == AF_UNIX) {
		/* Transport has different name space - return local info */
		error = 0;
		goto done;
	}
	/* Allocate local buffer to use with ioctl */
	addrlen = so->so_laddr_maxlen;
	mutex_exit(&so->so_lock);
	addr = kmem_alloc(addrlen, KM_SLEEP);

	/*
	 * Issue TI_GETMYNAME with signals masked.
	 * Put the result in so_laddr_sa so that getsockname works after
	 * a shutdown(output).
	 * If the ioctl fails (e.g. due to a ECONNRESET) the error is reposted
	 * back to the socket.
	 */
	strbuf.buf = addr;
	strbuf.maxlen = addrlen;
	strbuf.len = 0;

	sigintr(&smask, 0);
	res = 0;
	ASSERT(CRED());
	error = strioctl(SOTOV(so), TI_GETMYNAME, (intptr_t)&strbuf,
			0, K_TO_K, CRED(), &res);
	sigunintr(&smask);

	mutex_enter(&so->so_lock);
	/*
	 * If there is an error record the error in so_error put don't fail
	 * the getsockname. Instead fallback on the recorded
	 * so->so_laddr_sa.
	 */
	if (error) {
		/*
		 * Various stream head errors can be returned to the ioctl.
		 * However, it is impossible to determine which ones of
		 * these are really socket level errors that were incorrectly
		 * consumed by the ioctl. Thus this code silently ignores the
		 * error - to code explicitly does not reinstate the error
		 * using soseterror().
		 * Experiments have shows that at least this set of
		 * errors are reported and should not be reinstated on the
		 * socket:
		 *	EINVAL	E.g. if an I_LINK was in effect when
		 *		getsockname was called.
		 *	EPIPE	The ioctl error semantics prefer the write
		 *		side error over the read side error.
		 */
		error = 0;
	} else if (res == 0 && strbuf.len > 0 &&
	    (so->so_state & SS_ISBOUND)) {
		ASSERT(strbuf.len <= so->so_laddr_maxlen);
		so->so_laddr_len = strbuf.len;
		bcopy(addr, (char *)so->so_laddr_sa, so->so_laddr_len);
	}
	kmem_free(addr, addrlen);
#ifdef DEBUG
	dprintso(so, 1, ("sogetsockname (tp): %s\n",
			pr_addr(so->so_family, so->so_laddr_sa,
				so->so_laddr_len)));
#endif /* DEBUG */
done:
	so_unlock_single(so, SLOCKED);
	mutex_exit(&so->so_lock);
	return (error);
}

/*
 * Get socket options. For SOL_SOCKET options some options are handled
 * by the sockfs while others use the value recorded in the sonode as a
 * fallback should the T_OPTMGMT_REQ fail.
 *
 * On the return most *optlenp bytes are copied to optval.
 */
int
sogetsockopt(struct sonode *so, int level, int option_name,
		void *optval, int *optlenp)
{
	struct T_optmgmt_req	optmgmt_req;
	struct T_optmgmt_ack	*optmgmt_ack;
	struct opthdr		oh;
	struct opthdr		*opt_res;
	mblk_t			*mp = NULL;
	int			error = 0;
	void			*option = NULL;	/* Set if fallback value */
	int			maxlen = *optlenp;
	int			len;
	int			value;

	dprintso(so, 1, ("sogetsockopt(0x%x, 0x%x, 0x%x, 0x%x, 0x%x) %s\n",
			so, level, option_name, optval, optlenp,
			pr_state(so->so_state, so->so_mode)));

	mutex_enter(&so->so_lock);
	(void) so_lock_single(so, SLOCKED, 0);

	/*
	 * Check for SOL_SOCKET options.
	 * Certain SOL_SOCKET options are returned directly whereas
	 * others only provide a default (fallback) value should
	 * the T_OPTMGMT_REQ fail.
	 */
	if (level == SOL_SOCKET) {
		/* Check parameters */
		switch (option_name) {
		case SO_TYPE:
		case SO_ERROR:
		case SO_DEBUG:
		case SO_ACCEPTCONN:
		case SO_REUSEADDR:
		case SO_KEEPALIVE:
		case SO_DONTROUTE:
		case SO_BROADCAST:
		case SO_USELOOPBACK:
		case SO_OOBINLINE:
		case SO_SNDBUF:
		case SO_RCVBUF:
#ifdef notyet
		case SO_SNDLOWAT:
		case SO_RCVLOWAT:
		case SO_SNDTIMEO:
		case SO_RCVTIMEO:
#endif /* notyet */
		case SO_STATE:
		case SO_DGRAM_ERRIND:
			if (maxlen < sizeof (int)) {
				error = EINVAL;
				eprintsoline(so, error);
				goto done2;
			}
			break;
		case SO_LINGER:
			if (maxlen < sizeof (struct linger)) {
				error = EINVAL;
				eprintsoline(so, error);
				goto done2;
			}
			break;
		}

		len = sizeof (int);	/* Default */

		switch (option_name) {
		case SO_TYPE:
			value = so->so_type;
			option = (void *)&value;
			goto copyout;	/* No need to issue T_OPTMGMT_REQ */

		case SO_ERROR:
			value = sogeterr(so, 0);
			option = (void *)&value;
			goto copyout;	/* No need to issue T_OPTMGMT_REQ */

		case SO_ACCEPTCONN:
			if (so->so_state & SS_ACCEPTCONN)
				value = SO_ACCEPTCONN;
			else
				value = 0;
#ifdef DEBUG
			if (value) {
				dprintso(so, 1, ("sogetsockopt: %x is set\n",
					option_name));
			} else {
				dprintso(so, 1, ("sogetsockopt: %x not set\n",
					option_name));
			}
#endif /* DEBUG */
			option = (void *)&value;
			goto copyout;	/* No need to issue T_OPTMGMT_REQ */

		case SO_DEBUG:
		case SO_REUSEADDR:
		case SO_KEEPALIVE:
		case SO_DONTROUTE:
		case SO_BROADCAST:
		case SO_USELOOPBACK:
		case SO_OOBINLINE:
		case SO_DGRAM_ERRIND:
			value = (so->so_options & option_name);
#ifdef DEBUG
			if (value) {
				dprintso(so, 1, ("sogetsockopt: %x is set\n",
					option_name));
			} else {
				dprintso(so, 1, ("sogetsockopt: %x not set\n",
					option_name));
			}
#endif /* DEBUG */
			option = (void *)&value;
			goto copyout;	/* No need to issue T_OPTMGMT_REQ */

		case SO_STATE:
			value = so->so_state;
			option = (void *)&value;
			goto copyout;	/* No need to issue T_OPTMGMT_REQ */

		/*
		 * The following options are only returned by sockfs when the
		 * T_OPTMGMT_REQ fails.
		 */
		case SO_LINGER:
			option = (void *)&so->so_linger;
			len = sizeof (struct linger);
			break;
		case SO_SNDBUF: {
			long lvalue;

			/* Get the initial value from q_hiwat */
			lvalue = so->so_sndbuf;
			if (lvalue == 0) {
				mutex_exit(&so->so_lock);
				(void) strqget(strvp2wq(SOTOV(so))->q_next,
						QHIWAT, 0, &lvalue);
				mutex_enter(&so->so_lock);
				dprintso(so, 1,
				    ("got SO_SNDBUF %ld from q\n", lvalue));
			}
			value = (int)lvalue;
			option = (void *)&value;
			break;
		}
		case SO_RCVBUF: {
			long lvalue;

			/* Get the initial value from the read queue. */
			lvalue = so->so_rcvbuf;
			if (lvalue == 0) {
				mutex_exit(&so->so_lock);
				(void) strqget(RD(strvp2wq(SOTOV(so))),
						QHIWAT, 0, &lvalue);
				mutex_enter(&so->so_lock);
				dprintso(so, 1,
				    ("got SO_RCVBUF %ld from q\n", lvalue));
			}
			value = (int)lvalue;
			option = (void *)&value;
			break;
		}
#ifdef notyet
		/*
		 * We do not implement the semantics of these options
		 * thus we shouldn't implement the options either.
		 */
		case SO_SNDLOWAT:
			value = so->so_sndlowat;
			option = (void *)&value;
			break;
		case SO_RCVLOWAT:
			value = so->so_rcvlowat;
			option = (void *)&value;
			break;
		case SO_SNDTIMEO:
			value = so->so_sndtimeo;
			option = (void *)&value;
			break;
		case SO_RCVTIMEO:
			value = so->so_rcvtimeo;
			option = (void *)&value;
			break;
#endif /* notyet */
		}
	}
	mutex_exit(&so->so_lock);

	/* Send request */
	optmgmt_req.PRIM_type = T_OPTMGMT_REQ;
	optmgmt_req.MGMT_flags = T_CHECK;
	optmgmt_req.OPT_length = sizeof (oh) + maxlen;
	optmgmt_req.OPT_offset = sizeof (optmgmt_req);

	oh.level = level;
	oh.name = option_name;
	oh.len = maxlen;

	mp = soallocproto3((char *)&optmgmt_req, sizeof (optmgmt_req),
	    (char *)&oh, sizeof (oh), NULL, maxlen, 0, _ALLOC_SLEEP);
	/* Let option management work in the presence of data flow control */
	error = kstrputmsg(SOTOV(so), mp, NULL, 0, 0,
			MSG_BAND|MSG_HOLDSIG|MSG_IGNERROR|MSG_IGNFLOW, 0);
	mp = NULL;
	mutex_enter(&so->so_lock);
	if (error) {
		eprintsoline(so, error);
		goto done2;
	}
	error = sowaitprim(so, T_OPTMGMT_REQ, T_OPTMGMT_ACK,
			    sizeof (*optmgmt_ack) + sizeof (*opt_res), &mp);
	if (error) {
		if (option != NULL) {
			/* We have a fallback value */
			error = 0;
			goto copyout;
		}
		eprintsoline(so, error);
		goto done2;
	}
	ASSERT(mp);
	optmgmt_ack = (struct T_optmgmt_ack *)mp->b_rptr;
	opt_res = (struct opthdr *)sogetoff(mp, optmgmt_ack->OPT_offset,
			optmgmt_ack->OPT_length, ALIGN_TPI_opthdr_size);
	if (opt_res == NULL) {
		if (option != NULL) {
			/* We have a fallback value */
			error = 0;
			goto copyout;
		}
		error = EPROTO;
		eprintsoline(so, error);
		goto done;
	}
	option = (void *)&opt_res[1];
	if (opt_res->len < 0 ||
	    (u_char *)option + opt_res->len > mp->b_wptr) {
		if (option != NULL) {
			/* We have a fallback value */
			error = 0;
			goto copyout;
		}
		error = EPROTO;
		eprintsoline(so, error);
		goto done;
	}

	len = opt_res->len;

copyout:
	len = MIN(len, maxlen);
	bcopy((char *)option, (char *)optval, len);
	bcopy((char *)&len, (char *)optlenp, sizeof (len));
done:
	freemsg(mp);
done2:
	so_unlock_single(so, SLOCKED);
	mutex_exit(&so->so_lock);
	return (error);
}

/*
 * Set socket options. All options are passed down in a T_OPTMGMT_REQ.
 * SOL_SOCKET options are also recorded in the sonode. A setsockopt for
 * SOL_SOCKET options will not fail just because the T_OPTMGMT_REQ fails -
 * setsockopt has to work even if the transport does not support the option.
 */
int
sosetsockopt(struct sonode *so, int level, int option_name,
		void *optval, int optlen)
{
	struct T_optmgmt_req	optmgmt_req;
	struct opthdr		oh;
	mblk_t			*mp;
	int			error = 0;

	dprintso(so, 1, ("sosetsockopt(0x%x, 0x%x, 0x%x, 0x%x, %d) %s\n",
			so, level, option_name, optval, optlen,
			pr_state(so->so_state, so->so_mode)));


	/* X/Open requires this check */
	if ((so->so_state & SS_CANTSENDMORE) &&
	    !(level == SOL_SOCKET && option_name == SO_STATE) &&
	    !xnet_skip_checks) {
		if (xnet_check_print) {
			printf("sockfs: X/Open setsockopt check => EINVAL\n");
		}
		return (EINVAL);
	}

	/* Caller allocates aligned optval */
	ASSERT(((int)optval & (sizeof (int) - 1)) == 0);

	mutex_enter(&so->so_lock);
	(void) so_lock_single(so, SLOCKED, 0);
	mutex_exit(&so->so_lock);

	if (level == SOL_SOCKET && option_name == SO_STATE) {
		/* Ignore any flow control problems with the transport. */
		mutex_enter(&so->so_lock);
		goto done;
	}
	optmgmt_req.PRIM_type = T_OPTMGMT_REQ;
	optmgmt_req.MGMT_flags = T_NEGOTIATE;
	optmgmt_req.OPT_length = sizeof (oh) + optlen;
	optmgmt_req.OPT_offset = sizeof (optmgmt_req);

	oh.level = level;
	oh.name = option_name;
	oh.len = optlen;

	mp = soallocproto3((char *)&optmgmt_req, sizeof (optmgmt_req),
	    (char *)&oh, sizeof (oh), (char *)optval, optlen, 0, _ALLOC_SLEEP);
	/* Let option management work in the presence of data flow control */
	error = kstrputmsg(SOTOV(so), mp, NULL, 0, 0,
			MSG_BAND|MSG_HOLDSIG|MSG_IGNERROR|MSG_IGNFLOW, 0);
	mp = NULL;
	mutex_enter(&so->so_lock);
	if (error) {
		eprintsoline(so, error);
		goto done;
	}
	error = sowaitprim(so, T_OPTMGMT_REQ, T_OPTMGMT_ACK,
			    sizeof (struct T_optmgmt_ack), &mp);
	if (error) {
		eprintsoline(so, error);
		goto done;
	}
	ASSERT(mp);
	/* No need to verify T_optmgmt_ack */
	freemsg(mp);
done:
	/*
	 * Check for SOL_SOCKET options and record their values.
	 * If we know about a SOL_SOCKET parameter and the transport
	 * failed it with TBADOPT or TOUTSTATE (i.e. ENOPROTOOPT or
	 * EPROTO) we let the setsockopt succeed.
	 */
	if (level == SOL_SOCKET) {
		int handled = 0;

		/* Check parameters */
		switch (option_name) {
		case SO_DEBUG:
		case SO_REUSEADDR:
		case SO_KEEPALIVE:
		case SO_DONTROUTE:
		case SO_BROADCAST:
		case SO_USELOOPBACK:
		case SO_OOBINLINE:
		case SO_SNDBUF:
		case SO_RCVBUF:
#ifdef notyet
		case SO_SNDLOWAT:
		case SO_RCVLOWAT:
		case SO_SNDTIMEO:
		case SO_RCVTIMEO:
#endif /* notyet */
		case SO_STATE:
		case SO_DGRAM_ERRIND:
			if (optlen != sizeof (int)) {
				error = EINVAL;
				eprintsoline(so, error);
				goto done2;
			}
			ASSERT(optval);
			handled = 1;
			break;
		case SO_LINGER:
			if (optlen != sizeof (struct linger)) {
				error = EINVAL;
				eprintsoline(so, error);
				goto done2;
			}
			ASSERT(optval);
			handled = 1;
			break;
		}

#define	intvalue	(*(int *)optval)

		switch (option_name) {
		case SO_TYPE:
		case SO_ERROR:
		case SO_ACCEPTCONN:
			/* Can't be set */
			error = ENOPROTOOPT;
			goto done2;
		case SO_LINGER: {
			struct linger *l = (struct linger *)optval;

			so->so_linger.l_linger = l->l_linger;
			if (l->l_onoff) {
				so->so_linger.l_onoff = SO_LINGER;
				so->so_options |= SO_LINGER;
			} else {
				so->so_linger.l_onoff = 0;
				so->so_options &= ~SO_LINGER;
			}
			break;
		}

		case SO_DEBUG:
#ifdef SOCK_TEST
			if (intvalue & 2)
				sock_test_timelimit = 10 * hz;
			else
				sock_test_timelimit = 0;

			if (intvalue & 4)
				do_useracc = 0;
			else
				do_useracc = 1;
#endif /* SOCK_TEST */
			/* FALLTHRU */
		case SO_REUSEADDR:
		case SO_KEEPALIVE:
		case SO_DONTROUTE:
		case SO_BROADCAST:
		case SO_USELOOPBACK:
		case SO_OOBINLINE:
		case SO_DGRAM_ERRIND:
			if (intvalue != 0) {
				dprintso(so, 1, ("sosetsockopt: setting %x\n",
					option_name));
				so->so_options |= option_name;
			} else {
				dprintso(so, 1, ("sosetsockopt: clearing %x\n",
					option_name));
				so->so_options &= ~option_name;
			}
			break;
		/*
		 * The following options are only returned by us when the
		 * T_OPTMGMT_REQ fails.
		 */
		case SO_SNDBUF:
			so->so_sndbuf = intvalue;
			break;
		case SO_RCVBUF:
			so->so_rcvbuf = intvalue;
			break;
#ifdef notyet
		/*
		 * We do not implement the semantics of these options
		 * thus we shouldn't implement the options either.
		 */
		case SO_SNDLOWAT:
			so->so_sndlowat = intvalue;
			break;
		case SO_RCVLOWAT:
			so->so_rcvlowat = intvalue;
			break;
		case SO_SNDTIMEO:
			so->so_sndtimeo = intvalue;
			break;
		case SO_RCVTIMEO:
			so->so_rcvtimeo = intvalue;
			break;
#endif /* notyet */
		case SO_STATE: {
			/*
			 * This option is used by _s_fcntl() to modify the
			 * SS_ASYNC flag.
			 */
			int on, off;

			ASSERT(MUTEX_HELD(&so->so_lock));
			on = (~so->so_state & intvalue) & SS_CANCHANGE;
			off = (so->so_state & ~intvalue) & SS_CANCHANGE;
			so->so_state |= on;
			so->so_state &= ~off;
			dprintso(so, 1,
				("setsockopt: so_state on %x, off %x, new %x\n",
				on, off, so->so_state));


			/* Did SS_ASYNC change? */
			if ((on | off) & SS_ASYNC) {
				struct strsigset ss;
				int rval;

				if (so->so_state & SS_ASYNC)
					ss.ss_events = S_RDNORM | S_OUTPUT;
				else
					ss.ss_events = 0;

				if (so->so_pgrp != 0) {
					ss.ss_pid = so->so_pgrp;
					ss.ss_events |= S_RDBAND | S_BANDURG;
					error = strioctl(SOTOV(so), I_ESETSIG,
							(intptr_t)&ss, 0,
							K_TO_K, CRED(),
							&rval);
					if (error)
						goto ret;
				}
			}
			break;
		}
		}
#undef	intvalue

		if (error) {
			if ((error == ENOPROTOOPT || error == EPROTO ||
			    error == EINVAL) && handled) {
				dprintso(so, 1,
				    ("setsockopt: ignoring error %d for 0x%x\n",
				    error, option_name));
				error = 0;
			}
		}
	}
done2:
ret:
	so_unlock_single(so, SLOCKED);
	mutex_exit(&so->so_lock);
	return (error);
}
