/*	Copyright (c) 1994,1995,1996 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/

#pragma ident	"@(#)socksyscalls.c	1.11	96/09/09 SMI"

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
#include <sys/cmn_err.h>
#include <sys/vmsystm.h>

#include <sys/socket.h>
#include <sys/socketvar.h>
#include <netinet/in.h>
#include <sys/un.h>

#ifdef SOCK_TEST
int do_useracc = 1;		/* Controlled by setting SO_DEBUG to 4 */
#else
#define	do_useracc	1
#endif /* SOCK_TEST */

extern int xnet_truncate_print;

/*
 * Note: DEF_IOV_MAX is defined and used as it is in "fs/vncalls.c"
 *	 as there isn't a formal definition of IOV_MAX ???
 */
#define	MSG_MAXIOVLEN	16

/*
 * Kernel component of socket creation.
 *
 * The socket library determines which version number to use.
 * First the library calls this with a NULL devpath. If this fails
 * to find a transport (using solookup) the library will look in /etc/netconfig
 * for the appropriate transport. If one is found it will pass in the
 * devpath for the kernel to use.
 */
int
so_socket(int domain, int type, int protocol, char *devpath, int version)
{
	vnode_t *accessvp;
	struct sonode *so;
	vnode_t *vp;
	struct file *fp;
	int fd;
	int error;
	int wildcard = 0;

	dprint(1, ("so_socket(%d,%d,%d,%x,%d)\n",
		domain, type, protocol, devpath, version));

	accessvp = solookup(domain, type, protocol, devpath, &error);
	if (accessvp == NULL) {
		if (devpath != NULL || error != EPROTONOSUPPORT ||
		    protocol == 0)
			return (set_errno(error));

		/*
		 * Try wildcard lookup. Never use devpath for wildcards.
		 */
		accessvp = solookup(domain, type, 0, NULL, &error);
		if (accessvp == NULL) {
			/*
			 * Can't find in kernel table - have library
			 * fall back to /etc/netconfig and tell us
			 * the devpath (The library will do this if it didn't
			 * already pass in a devpath).
			 */
			return (set_errno(error));
		}
		wildcard = 1;
	}
	so = socreate(accessvp, domain, type, protocol, version, NULL, &error);
	if (so == NULL) {
		return (set_errno(error));
	}
	vp = SOTOV(so);

	if (wildcard) {
		/*
		 * Issue SO_PROTOTYPE setsockopt.
		 */
		error = sosetsockopt(so, SOL_SOCKET, SO_PROTOTYPE,
				(char *)&protocol, sizeof (protocol));
		if (error) {
			(void) VOP_CLOSE(vp, 0, 1, 0, CRED());
			VN_RELE(vp);
			/*
			 * Setsockopt often fails with ENOPROTOOPT but socket()
			 * should fail with EPROTONOSUPPORT.
			 */
			return (set_errno(EPROTONOSUPPORT));
		}
	}
	if (error = falloc(vp, FWRITE|FREAD, &fp, &fd)) {
		(void) VOP_CLOSE(vp, 0, 1, 0, CRED());
		VN_RELE(vp);
		return (set_errno(error));
	}

	/*
	 * Now fill in the entries that falloc reserved
	 */
	mutex_exit(&fp->f_tlock);
	setf(fd, fp);

	return (fd);
}

/*
 * Map from a file descriptor to a socket node.
 * Returns with the file descriptor held i.e. the caller has to
 * use RELEASEF when done with the file descriptor.
 */
static struct sonode *
getsonode(int sock, int *errorp, file_t **fpp)
{
	file_t *fp;
	register vnode_t *vp;
	struct sonode *so;

	if ((fp = GETF(sock)) == NULL) {
		*errorp = EBADF;
		eprintline(*errorp);
		return (NULL);
	}
	vp = fp->f_vnode;
	/* Check if it is a socket */
	if (vp->v_type != VSOCK) {
		RELEASEF(sock);
		*errorp = ENOTSOCK;
		eprintline(*errorp);
		return (NULL);
	}
	/*
	 * Use the stream head to find the real socket vnode.
	 * This is needed when namefs sits above sockfs.
	 */
	ASSERT(vp->v_stream);
	ASSERT(vp->v_stream->sd_vnode);
	vp = vp->v_stream->sd_vnode;
	ASSERT(vp->v_op == sock_getvnodeops());
	so = VTOSO(vp);
	if (so->so_version == SOV_STREAM) {
		RELEASEF(sock);
		*errorp = ENOTSOCK;
		eprintsoline(so, *errorp);
		return (NULL);
	}
	if (fpp)
		*fpp = fp;
	return (so);
}

/*
 * Allocate and copyin a sockaddr.
 * Ensures NUL termination for AF_UNIX addresses by extending them
 * with one NUL byte if need be.
 * Verifies that the length is not excessive to prevent an application
 * from consuming all of kernel memory.
 * Returns NULL when an error occurred.
 */
static struct sockaddr *
copyin_name(struct sonode *so, struct sockaddr *name, int *namelenp,
	    int *errorp)
{
	char *faddr;
	int namelen = *namelenp;

	ASSERT(namelen != 0);
	if (namelen > SO_MAXARGSIZE || namelen < 0) {
		*errorp = EINVAL;
		eprintsoline(so, *errorp);
		return (NULL);
	}

	faddr = kmem_alloc(namelen, KM_SLEEP);
	if (copyin((char *)name, faddr, namelen)) {
		kmem_free(faddr, namelen);
		*errorp = EFAULT;
		eprintsoline(so, *errorp);
		return (NULL);
	}

	/*
	 * Add space for NUL termination if needed.
	 * Do a quick check if the last byte is NUL.
	 */
	if (so->so_family == AF_UNIX && faddr[namelen - 1] != '\0') {
		/* Check if there is any NUL termination */
		int i;
		int foundnul = 0;

		for (i = sizeof (name->sa_family); i < namelen; i++) {
			if (faddr[i] == '\0') {
				foundnul = 1;
				break;
			}
		}
		if (!foundnul) {
			/* Add extra byte for NUL padding */
			char *nfaddr;

			nfaddr = kmem_alloc(namelen + 1, KM_SLEEP);
			bcopy(faddr, nfaddr, namelen);
			kmem_free(faddr, namelen);
			faddr = nfaddr;

			/* NUL terminate */
			faddr[namelen] = '\0';
			namelen++;
			*namelenp = namelen;

		}
	}
	return ((struct sockaddr *)faddr);
}

/*
 * Copy from kaddr/klen to uaddr/ulen. Updates ulenp if non-NULL.
 */
static int
copyout_arg(char *uaddr, int ulen, int *ulenp,
		char *kaddr, int klen)
{
	if (uaddr != NULL) {
		if (ulen > klen)
			ulen = klen;

		if (ulen > 0) {
			if (copyout((char *)kaddr, (char *)uaddr, ulen))
				return (EFAULT);
		}
	} else
		ulen = 0;

	if (ulenp != NULL) {
		if (copyout((char *)&ulen, (char *)ulenp,
			    sizeof (int)))
			return (EFAULT);
	}
	return (0);
}

/*
 * Copy from kaddr/klen to uaddr/ulen. Updates ulenp if non-NULL.
 * If klen is greater than ulen it still uses the non-truncated
 * klen to update ulenp.
 */
static int
copyout_name(char *uaddr, int ulen, int *ulenp,
		char *kaddr, int klen)
{
	if (uaddr != NULL) {
		if (ulen >= klen)
			ulen = klen;
		else if (ulen > 0 && xnet_truncate_print) {
			cmn_err(CE_WARN,
			    "sockfs: truncating copyout of address using XNET "
			    "semantics for pid = %d. Lengths %d, %d\n",
			    (int)curproc->p_pid, klen, ulen);
		}

		if (ulen > 0) {
			if (copyout((char *)kaddr, (char *)uaddr, ulen))
				return (EFAULT);
		} else
			klen = 0;
	} else
		klen = 0;

	if (ulenp != NULL) {
		if (copyout((char *)&klen, (char *)ulenp,
			    sizeof (int)))
			return (EFAULT);
	}
	return (0);
}

/*
 * The socketpair() code in libsocket creates two sockets (using
 * the /etc/netconfig fallback if needed) before calling this routine
 * to connect the two sockets together.
 *
 * For a SOCK_STREAM socketpair a listener is needed - in that case this
 * routine will create a new file descriptor as part of accepting the
 * connection. The library socketpair() will check if svs[2] has changed
 * in which case it will close the changed fd.
 *
 * Note that this code could use the TPI feature of accepting the connection
 * on the listening endpoint. However, that would require significant changes
 * to soaccept.
 */
int
so_socketpair(int sv[2])
{
	int svs[2];
	struct sonode *so1, *so2;
	int error;
	char *name;
	int namelen;

	dprint(1, ("so_socketpair(0x%x)\n", sv));

	error = useracc((char *)sv, sizeof (svs), B_WRITE);
	if (error && do_useracc)
		return (set_errno(EFAULT));

	if (copyin((char *)sv, (char *)svs, sizeof (svs)))
		return (set_errno(EFAULT));

	if ((so1 = getsonode(svs[0], &error, NULL)) == NULL)
		return (set_errno(error));

	if ((so2 = getsonode(svs[1], &error, NULL)) == NULL) {
		RELEASEF(svs[0]);
		return (set_errno(error));
	}

	if (so1->so_family != AF_UNIX || so2->so_family != AF_UNIX) {
		error = EOPNOTSUPP;
		goto done;
	}
	if (so1->so_type == SOCK_DGRAM) {
		/*
		 * Bind both sockets and connect them with each other.
		 * Need to allocate name/namelen for soconnect.
		 */
		error = sobind(so1, NULL, 0, 0, _SOBIND_UNSPEC);
		if (error) {
			eprintsoline(so1, error);
			goto done;
		}
		error = sobind(so2, NULL, 0, 0, _SOBIND_UNSPEC);
		if (error) {
			eprintsoline(so2, error);
			goto done;
		}
		namelen = sizeof (so2->so_ux_laddr);
		name = kmem_alloc(namelen, KM_SLEEP);
		bcopy((char *)&so2->so_ux_laddr, name, namelen);
		error = soconnect(so1, (struct sockaddr *)name, namelen,
			    0, _SOCONNECT_NOXLATE);
		if (name)
			kmem_free(name, namelen);
		if (error) {
			eprintsoline(so1, error);
			goto done;
		}
		namelen = sizeof (so1->so_ux_laddr);
		name = kmem_alloc(namelen, KM_SLEEP);
		bcopy((char *)&so1->so_ux_laddr, name, namelen);
		error = soconnect(so2, (struct sockaddr *)name, namelen,
			    0, _SOCONNECT_NOXLATE);
		if (name)
			kmem_free(name, namelen);
		if (error) {
			eprintsoline(so2, error);
			goto done;
		}
		RELEASEF(svs[0]);
		RELEASEF(svs[1]);
	} else {
		/*
		 * Bind both sockets, with so1 being a listener.
		 * Connect so2 to so1 - nonblocking to avoid waiting for
		 * soaccept to complete.
		 * Accept a connection on so1. Pass out the new fd as sv[0].
		 * The library will detect the changed fd and close
		 * the original one.
		 */
		struct sonode *nso;
		struct vnode *nvp;
		struct file *nfp;
		int nfd;

		error = sobind(so1, NULL, 0, 1,
				_SOBIND_UNSPEC|_SOBIND_NOXLATE|_SOBIND_LISTEN);
		if (error) {
			eprintsoline(so1, error);
			goto done;
		}
		error = sobind(so2, NULL, 0, 0, _SOBIND_UNSPEC);
		if (error) {
			eprintsoline(so2, error);
			goto done;
		}

		namelen = sizeof (so1->so_ux_laddr);
		name = kmem_alloc(namelen, KM_SLEEP);
		bcopy((char *)&so1->so_ux_laddr, name, namelen);

		error = soconnect(so2, (struct sockaddr *)name, namelen,
			    FNONBLOCK, _SOCONNECT_NOXLATE);
		if (name)
			kmem_free(name, namelen);
		if (error) {
			if (error != EINPROGRESS) {
				eprintsoline(so2, error);
				goto done;
			}
			error = 0;
		}

		error = soaccept(so1, 0, &nso);
		if (error) {
			eprintsoline(so1, error);
			goto done;
		}

		/* wait for so2 being SS_CONNECTED ignoring signals */
		mutex_enter(&so2->so_lock);
		error = sowaitconnected(so2, 0, 1);
		mutex_exit(&so2->so_lock);
		nvp = SOTOV(nso);
		if (error != 0) {
			(void) VOP_CLOSE(nvp, 0, 1, 0, CRED());
			VN_RELE(nvp);
			eprintsoline(so2, error);
			goto done;
		}

		if (error = falloc(nvp, FWRITE|FREAD, &nfp, &nfd)) {
			(void) VOP_CLOSE(nvp, 0, 1, 0, CRED());
			VN_RELE(nvp);
			eprintsoline(nso, error);
			goto done;
		}
		/*
		 * fill in the entries that falloc reserved
		 */
		mutex_exit(&nfp->f_tlock);
		setf(nfd, nfp);

		RELEASEF(svs[0]);
		RELEASEF(svs[1]);
		svs[0] = nfd;

		/*
		 * The socketpair library routine will close the original
		 * svs[0] when this code passes out a different file
		 * descriptor.
		 */
		if (copyout((char *)svs, (char *)sv, sizeof (svs))) {
			setf(nfd, NULLFP);
			(void) closef(nfp);
			eprintline(EFAULT);
			return (set_errno(EFAULT));
		}
	}
	return (0);


done:
	RELEASEF(svs[0]);
	RELEASEF(svs[1]);
	return (set_errno(error));
}

int
bind(int sock, struct sockaddr *name, int namelen, int version)
{
	struct sonode *so;
	int error;

	dprint(1, ("bind(%d, %x, %d)\n",
		sock, name, namelen));

	if ((so = getsonode(sock, &error, NULL)) == NULL)
		return (set_errno(error));

	/* Allocate and copyin name */
	/*
	 * X/Open test does not expect EFAULT with NULL name and non-zero
	 * namelen.
	 */
	if (name != NULL && namelen != 0) {
		name = copyin_name(so, name, &namelen, &error);
		if (name == NULL) {
			RELEASEF(sock);
			return (set_errno(error));
		}
	} else {
		name = NULL;
		namelen = 0;
	}

	switch (version) {
	default:
		error = sobind(so, name, namelen, 0, 0);
		break;
	case SOV_XPG4_2:
		error = sobind(so, name, namelen, 0, _SOBIND_XPG4_2);
		break;
	case SOV_SOCKBSD:
		error = sobind(so, name, namelen, 0, _SOBIND_SOCKBSD);
		break;
	}
	RELEASEF(sock);
	if (name != NULL)
		kmem_free(name, namelen);

	if (error)
		return (set_errno(error));
	return (0);
}

int
listen(int sock, int backlog, int version)
{
	struct sonode *so;
	int error;

	dprint(1, ("listen(%d, %d)\n",
		sock, backlog));

	if ((so = getsonode(sock, &error, NULL)) == NULL)
		return (set_errno(error));

	if (version == SOV_XPG4_2)
		error = solisten(so, backlog, _SOLISTEN_XPG4_2);
	else
		error = solisten(so, backlog, 0);

	RELEASEF(sock);
	if (error)
		return (set_errno(error));
	return (0);
}

/*ARGSUSED3*/
int
accept(int sock, struct sockaddr *name, int *namelenp, int version)
{
	struct sonode *so;
	file_t *fp;
	int error;
	int namelen;
	struct sonode *nso;
	struct vnode *nvp;
	struct file *nfp;
	int nfd;

	dprint(1, ("accept(%d, %x, %x)\n",
		sock, name, namelenp));

	if ((so = getsonode(sock, &error, &fp)) == NULL)
		return (set_errno(error));

	if (name != NULL) {
		if (copyin((char *)namelenp, (char *)&namelen, sizeof (int))) {
			RELEASEF(sock);
			return (set_errno(EFAULT));
		}
		if (namelen != 0) {
			error = useracc((caddr_t)name, (u_int)namelen,
					B_WRITE);
			if (error && do_useracc) {
				RELEASEF(sock);
				return (set_errno(EFAULT));
			}
		} else
			name = NULL;
	} else {
		namelen = 0;
	}

	/*
	 * Allocate the user fd before soaccept in order to
	 * catch EMFILE errors before calling soaccept.
	 */
	if (error = ufalloc(0, &nfd)) {
		eprintsoline(so, error);
		RELEASEF(sock);
		return (set_errno(error));
	}
	error = soaccept(so, fp->f_flag, &nso);
	RELEASEF(sock);
	if (error) {
		setf(nfd, NULLFP);
		return (set_errno(error));
	}

	nvp = SOTOV(nso);

	/* Prevent so_laddr_sa from changing while accessed */
	mutex_enter(&nso->so_lock);
	error = copyout_name((char *)name, namelen, namelenp,
			    (char *)nso->so_faddr_sa, nso->so_faddr_len);
	mutex_exit(&nso->so_lock);
	if (error) {
		setf(nfd, NULLFP);
		(void) VOP_CLOSE(nvp, 0, 1, 0, CRED());
		VN_RELE(nvp);
		return (set_errno(error));
	}
	if (error = falloc(NULL, FWRITE|FREAD, &nfp, NULL)) {
		setf(nfd, NULLFP);
		(void) VOP_CLOSE(nvp, 0, 1, 0, CRED());
		VN_RELE(nvp);
		eprintsoline(so, error);
		return (set_errno(error));
	}
	/*
	 * fill in the entries that falloc reserved
	 */
	nfp->f_vnode = nvp;
	mutex_exit(&nfp->f_tlock);
	setf(nfd, nfp);

	/*
	 * Copy FNDELAY and FNONBLOCK from listener to acceptor
	 */
	if (so->so_state & (SS_NDELAY|SS_NONBLOCK)) {
		char oflag = nfp->f_flag;
		int arg = 0;

		if (so->so_state & SS_NDELAY)
			arg |= FNDELAY;
		if (so->so_state & SS_NONBLOCK)
			arg |= FNONBLOCK;

		/*
		 * This code is a copy of the F_SETFL code in fcntl()
		 * Ignore any errors from VOP_SETFL.
		 */
		if ((arg & (FNONBLOCK|FNDELAY)) == (FNONBLOCK|FNDELAY))
			arg &= ~FNDELAY;
		if ((error = VOP_SETFL(nvp, oflag, arg, nfp->f_cred)) != 0) {
			eprintsoline(so, error);
			error = 0;
		} else {
			arg &= FMASK;
			mutex_enter(&nfp->f_tlock);
			nfp->f_flag &= (FREAD|FWRITE);
			nfp->f_flag |= (arg-FOPEN) & ~(FREAD|FWRITE);
			mutex_exit(&nfp->f_tlock);
		}
	}
	return (nfd);
}

int
connect(int sock, struct sockaddr *name, int namelen, int version)
{
	struct sonode *so;
	file_t *fp;
	int error;

	dprint(1, ("connect(%d, %x, %d)\n",
		sock, name, namelen));

	if ((so = getsonode(sock, &error, &fp)) == NULL)
		return (set_errno(error));

	/* Allocate and copyin name */
	if (namelen != 0) {
		name = copyin_name(so, name, &namelen, &error);
		if (name == NULL) {
			RELEASEF(sock);
			return (set_errno(error));
		}
	} else
		name = NULL;

	if (version == SOV_XPG4_2)
		error = soconnect(so, name, namelen, fp->f_flag,
				_SOCONNECT_XPG4_2);
	else
		error = soconnect(so, name, namelen, fp->f_flag, 0);
	RELEASEF(sock);
	if (name)
		kmem_free(name, namelen);
	if (error)
		return (set_errno(error));
	return (0);
}

/*ARGSUSED2*/
int
shutdown(int sock, int how, int version)
{
	struct sonode *so;
	int error;

	dprint(1, ("shutdown(%d, %d)\n",
		sock, how));

	if ((so = getsonode(sock, &error, NULL)) == NULL)
		return (set_errno(error));

	error = soshutdown(so, how);
	RELEASEF(sock);
	if (error)
		return (set_errno(error));
	return (0);
}

/*
 * Common receive routine.
 */
static ssize_t
recvit(int sock, struct nmsghdr *msg, struct uio *uiop, int flags,
	int *namelenp, int *controllenp, int *flagsp)
{
	struct sonode *so;
	file_t *fp;
	char *name;
	int namelen;
	char *control;
	int controllen;
	int len;
	int error;
	klwp_t *lwp = ttolwp(curthread);

	if ((so = getsonode(sock, &error, &fp)) == NULL)
		return (set_errno(error));

	len = uiop->uio_resid;
	uiop->uio_fmode = fp->f_flag;

	name = msg->msg_name;
	namelen = msg->msg_namelen;
	control = msg->msg_control;
	controllen = msg->msg_controllen;

	msg->msg_flags = flags & (MSG_OOB | MSG_PEEK | MSG_WAITALL |
				MSG_DONTWAIT | MSG_XPG4_2);
	error = sorecvmsg(so, msg, uiop);
	if (error) {
		RELEASEF(sock);
		return (set_errno(error));
	}
	if (lwp != NULL)
		lwp->lwp_ru.msgrcv++;
	so_update_attrs(so, SACC);
	RELEASEF(sock);

	error = copyout_name(name, namelen, namelenp,
			    msg->msg_name, msg->msg_namelen);
	if (error)
		goto err;

	if (flagsp != NULL) {
		/*
		 * Clear internal flag.
		 */
		msg->msg_flags &= ~MSG_XPG4_2;
		if (copyout((char *)&msg->msg_flags, (char *)flagsp,
			    sizeof (msg->msg_flags))) {
			error = EFAULT;
			goto err;
		}
	}
	/*
	 * Note: This MUST be done last. There can be no "goto err" after this
	 * point since it could make so_closefds run twice on some part
	 * of the file descriptor array.
	 */
	if (controllen != 0) {
		if (!(flags & MSG_XPG4_2)) {
			/*
			 * Good old msg_accrights can only return a multiple
			 * of 4 bytes.
			 */
			controllen &= ~(sizeof (int) - 1);
		}
		error = copyout_arg(control, controllen, controllenp,
				msg->msg_control,
				msg->msg_controllen);
		if (error)
			goto err;

		if (msg->msg_controllen > controllen || control == NULL) {
			dprint(1, ("recvit: CTRUNC %d %d 0x%x\n",
				msg->msg_controllen, controllen, control))
			msg->msg_flags |= MSG_CTRUNC;
			if (control == NULL)
				controllen = 0;
			so_closefds(msg->msg_control, msg->msg_controllen,
				    !(flags & MSG_XPG4_2), controllen);
		}
	}
	if (msg->msg_namelen != 0)
		kmem_free(msg->msg_name, msg->msg_namelen);
	if (msg->msg_controllen != 0)
		kmem_free(msg->msg_control, msg->msg_controllen);
	return (len - uiop->uio_resid);

err:
	/*
	 * If we fail and the control part contains file descriptors
	 * we have to close the fd's.
	 */
	if (msg->msg_controllen != 0)
		so_closefds(msg->msg_control, msg->msg_controllen,
			    !(flags & MSG_XPG4_2), 0);
	if (msg->msg_namelen != 0)
		kmem_free(msg->msg_name, msg->msg_namelen);
	if (msg->msg_controllen != 0)
		kmem_free(msg->msg_control, msg->msg_controllen);
	return (set_errno(error));
}

/*
 * Can't make len be size_t since the uiomove code assumes it is a
 * signed type.
 */
ssize_t
recv(int sock, void *buffer, ssize_t len, int flags)
{
	struct nmsghdr lmsg;
	struct uio auio;
	struct iovec aiov[1];

	dprint(1, ("recv(%d, %x, %d, %d)\n",
		sock, buffer, len, flags));

	if (len < 0) {
		return (set_errno(EINVAL));
	}

	aiov[0].iov_base = buffer;
	aiov[0].iov_len = len;
	auio.uio_loffset = 0;
	auio.uio_iov = aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = len;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_limit = 0;

	lmsg.msg_namelen = 0;
	lmsg.msg_controllen = 0;
	lmsg.msg_flags = 0;
	return (recvit(sock, &lmsg, &auio, flags, NULL, NULL, NULL));
}

/*
 * Can't make len be size_t since the uiomove code assumes it is a
 * signed type.
 */
ssize_t
recvfrom(int sock, void *buffer, ssize_t len, int flags,
	struct sockaddr *name, int *namelenp)
{
	struct nmsghdr lmsg;
	struct uio auio;
	struct iovec aiov[1];

	dprint(1, ("recvfrom(%d, %x, %d, %d, %x, %x)\n",
		sock, buffer, len, flags, name, namelenp));

	if (len < 0) {
		return (set_errno(EINVAL));
	}

	aiov[0].iov_base = buffer;
	aiov[0].iov_len = len;
	auio.uio_loffset = 0;
	auio.uio_iov = aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = len;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_limit = 0;

	lmsg.msg_name = (char *)name;
	if (copyin((char *)namelenp, (char *)&lmsg.msg_namelen, sizeof (int)))
		return (set_errno(EFAULT));

	lmsg.msg_controllen = 0;
	lmsg.msg_flags = 0;

	return (recvit(sock, &lmsg, &auio, flags, namelenp, NULL, NULL));
}

/*
 * Uses the MSG_XPG4_2 flag to determine if the caller is using
 * struct omsghdr or struct nmsghdr.
 */
ssize_t
recvmsg(int sock, struct nmsghdr *msg, int flags)
{
	struct nmsghdr lmsg;
	struct uio auio;
	struct iovec aiov[MSG_MAXIOVLEN];
	int iovcnt;
	int len, i;
	int *flagsp;

	dprint(1, ("recvmsg(%d, %x, %d)\n",
		sock, msg, flags));

	if (flags & MSG_XPG4_2) {
		if (copyin((char *)msg, (char *)&lmsg, sizeof (lmsg)))
			return (set_errno(EFAULT));
		flagsp = &msg->msg_flags;
	} else {
		/*
		 * Assumes that nmsghdr and omsghdr are identically shaped
		 * except for the added msg_flags field.
		 */
		if (copyin((char *)msg, (char *)&lmsg,
			    sizeof (struct omsghdr)))
			return (set_errno(EFAULT));
		lmsg.msg_flags = 0;
		flagsp = NULL;
	}

	iovcnt = lmsg.msg_iovlen;

	if (iovcnt <= 0 || iovcnt > MSG_MAXIOVLEN) {
		return (set_errno(EMSGSIZE));
	}
	if (copyin((caddr_t)lmsg.msg_iov, (caddr_t)aiov,
	    (unsigned)iovcnt * sizeof (struct iovec))) {
		return (set_errno(EFAULT));
	}
	len = 0;
	for (i = 0; i < iovcnt; i++) {
		int iovlen = aiov[i].iov_len;
		len += iovlen;
		if (iovlen < 0 || len < 0) {
			return (set_errno(EINVAL));
		}
	}
	auio.uio_loffset = 0;
	auio.uio_iov = aiov;
	auio.uio_iovcnt = iovcnt;
	auio.uio_resid = len;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_limit = 0;

	if (lmsg.msg_control != NULL &&
	    (do_useracc == 0 ||
	    useracc((char *)lmsg.msg_control, lmsg.msg_controllen,
			B_WRITE) != 0)) {
		return (set_errno(EFAULT));
	}

	return (recvit(sock, &lmsg, &auio, flags,
			(int *)&msg->msg_namelen, (int *)&msg->msg_controllen,
			flagsp));
}

/*
 * Common send function.
 */
static ssize_t
sendit(int sock, struct nmsghdr *msg, struct uio *uiop, int flags)
{
	struct sonode *so;
	file_t *fp;
	char *name;
	int namelen;
	char *control;
	int controllen;
	int len;
	int error;
	klwp_t *lwp = ttolwp(curthread);

	if ((so = getsonode(sock, &error, &fp)) == NULL)
		return (set_errno(error));

	uiop->uio_fmode = fp->f_flag;

	/* Allocate and copyin name and control */
	name = msg->msg_name;
	namelen = msg->msg_namelen;
	if (name != NULL && namelen != 0) {
		name = (char *)copyin_name(so,
				(struct sockaddr *)name,
				&namelen, &error);
		if (name == NULL)
			goto done3;
		/* copyin_name null terminates addresses for AF_UNIX */
		msg->msg_namelen = namelen;
		msg->msg_name = name;
	} else {
		msg->msg_name = name = NULL;
		msg->msg_namelen = namelen = 0;
	}

	control = msg->msg_control;
	controllen = msg->msg_controllen;
	if (control != NULL) {
		/*
		 * Verify that the length is not excessive to prevent
		 * an application from consuming all of kernel memory.
		 */
		if (controllen > SO_MAXARGSIZE || controllen < 0) {
			error = EINVAL;
			goto done2;
		}
		control = kmem_alloc(controllen, KM_SLEEP);

		if (copyin(msg->msg_control, control, controllen)) {
			error = EFAULT;
			goto done1;
		}
		msg->msg_control = control;
	} else
		msg->msg_controllen = controllen = 0;

	len = uiop->uio_resid;
	msg->msg_flags = flags;

	error = sosendmsg(so, msg, uiop);
done1:
	if (control)
		kmem_free(control, controllen);
done2:
	if (name)
		kmem_free(name, namelen);
done3:
	if (error) {
		RELEASEF(sock);
		return (set_errno(error));
	}
	if (lwp != NULL)
		lwp->lwp_ru.msgsnd++;
	so_update_attrs(so, SMOD);
	RELEASEF(sock);
	return (len - uiop->uio_resid);
}

/*
 * Can't make len be size_t since the uiomove code assumes it is a
 * signed type.
 */
ssize_t
send(int sock, void *buffer, ssize_t len, int flags)
{
	struct nmsghdr lmsg;
	struct uio auio;
	struct iovec aiov[1];

	dprint(1, ("send(%d, %x, %d, %d)\n",
		sock, buffer, len, flags));

	if (len < 0) {
		return (set_errno(EINVAL));
	}

	aiov[0].iov_base = buffer;
	aiov[0].iov_len = len;
	auio.uio_loffset = 0;
	auio.uio_iov = aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = len;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_limit = 0;

	lmsg.msg_name = NULL;
	lmsg.msg_control = NULL;
	if (!(flags & MSG_XPG4_2)) {
		/*
		 * In order to be compatible with the libsocket/sockmod
		 * implementation we set EOR for all send* calls.
		 */
		flags |= MSG_EOR;
	}
	return (sendit(sock, &lmsg, &auio, flags));
}

/*
 * Uses the MSG_XPG4_2 flag to determine if the caller is using
 * struct omsghdr or struct nmsghdr.
 */
ssize_t
sendmsg(int sock, struct nmsghdr *msg, int flags)
{
	struct nmsghdr lmsg;
	struct uio auio;
	struct iovec aiov[MSG_MAXIOVLEN];
	int iovcnt;
	int len, i;

	dprint(1, ("sendmsg(%d, %x, %d)\n", sock, msg, flags));

	if (flags & MSG_XPG4_2) {
		if (copyin((char *)msg, (char *)&lmsg, sizeof (lmsg)))
			return (set_errno(EFAULT));
	} else {
		/*
		 * Assumes that nmsghdr and omsghdr are identically shaped
		 * except for the added msg_flags field.
		 */
		if (copyin((char *)msg, (char *)&lmsg,
			    sizeof (struct omsghdr)))
			return (set_errno(EFAULT));
		/*
		 * In order to be compatible with the libsocket/sockmod
		 * implementation we set EOR for all send* calls.
		 */
		flags |= MSG_EOR;
	}
	iovcnt = lmsg.msg_iovlen;

	if (iovcnt <= 0 || iovcnt > MSG_MAXIOVLEN) {
		return (set_errno(EMSGSIZE));
	}
	if (copyin((caddr_t)lmsg.msg_iov, (caddr_t)aiov,
	    (unsigned)iovcnt * sizeof (struct iovec))) {
		return (set_errno(EFAULT));
	}
	len = 0;
	for (i = 0; i < iovcnt; i++) {
		int iovlen = aiov[i].iov_len;
		len += iovlen;
		if (iovlen < 0 || len < 0) {
			return (set_errno(EINVAL));
		}
	}
	auio.uio_loffset = 0;
	auio.uio_iov = aiov;
	auio.uio_iovcnt = iovcnt;
	auio.uio_resid = len;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_limit = 0;

	return (sendit(sock, &lmsg, &auio, flags));
}

/*
 * Can't make len be size_t since the uiomove code assumes it is a
 * signed type.
 */
ssize_t
sendto(int sock, void *buffer, ssize_t len, int flags,
	struct sockaddr *name, int namelen)
{
	struct nmsghdr lmsg;
	struct uio auio;
	struct iovec aiov[1];

	dprint(1, ("sendto(%d, %x, %d, %d, %x, %d)\n",
		sock, buffer, len, flags, name, namelen));

	if (len < 0) {
		return (set_errno(EINVAL));
	}

	aiov[0].iov_base = buffer;
	aiov[0].iov_len = len;
	auio.uio_loffset = 0;
	auio.uio_iov = aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = len;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_limit = 0;

	lmsg.msg_name = (char *)name;
	lmsg.msg_namelen = namelen;
	lmsg.msg_control = NULL;
	if (!(flags & MSG_XPG4_2)) {
		/*
		 * In order to be compatible with the libsocket/sockmod
		 * implementation we set EOR for all send* calls.
		 */
		flags |= MSG_EOR;
	}
	return (sendit(sock, &lmsg, &auio, flags));
}

/*ARGSUSED3*/
int
getpeername(int sock, struct sockaddr *name, int *namelenp, int version)
{
	struct sonode *so;
	int error;
	int namelen;

	dprint(1, ("getpeername(%d, %x, %x)\n",
		sock, name, namelenp));

	if ((so = getsonode(sock, &error, NULL)) == NULL)
		return (set_errno(error));

	if (copyin((char *)namelenp, (char *)&namelen, sizeof (int))) {
		RELEASEF(sock);
		return (set_errno(EFAULT));
	}
	error = sogetpeername(so);
	if (error) {
		RELEASEF(sock);
		return (set_errno(error));
	}
	/* Prevent so_faddr_sa from changing while accessed */
	mutex_enter(&so->so_lock);
	if (name == NULL && namelen != 0)
		error = EFAULT;
	else
		error = copyout_name((char *)name, namelen, namelenp,
				(char *)so->so_faddr_sa,
				(so->so_state & SS_FADDR_NOXLATE) ? 0 :
				so->so_faddr_len);
	mutex_exit(&so->so_lock);
	RELEASEF(sock);
	if (error)
		return (set_errno(error));
	return (0);
}

/*ARGSUSED3*/
int
getsockname(int sock, struct sockaddr *name, int *namelenp, int version)
{
	struct sonode *so;
	int error;
	int namelen;

	dprint(1, ("getsockname(%d, %x, %x)\n",
		sock, name, namelenp));

	if ((so = getsonode(sock, &error, NULL)) == NULL)
		return (set_errno(error));

	if (copyin((char *)namelenp, (char *)&namelen, sizeof (int))) {
		RELEASEF(sock);
		return (set_errno(EFAULT));
	}
	error = sogetsockname(so);
	if (error) {
		RELEASEF(sock);
		return (set_errno(error));
	}
	/* Prevent so_laddr from being changed while accessed */
	mutex_enter(&so->so_lock);
	if (name == NULL && namelen != 0)
		error = EFAULT;
	else
		error = copyout_name((char *)name, namelen, namelenp,
			    (char *)so->so_laddr_sa, so->so_laddr_len);
	mutex_exit(&so->so_lock);
	RELEASEF(sock);
	if (error)
		return (set_errno(error));
	return (0);
}

/*ARGSUSED5*/
int
getsockopt(int sock, int level, int option_name,
	void *option_value, int *option_lenp, int version)
{
	struct sonode *so;
	int optlen, optlen_res;
	void *optval;
	int error;

	dprint(1, ("getsockopt(%d, %d, %d, %x, %x)\n",
		sock, level, option_name, option_value, option_lenp));

	if ((so = getsonode(sock, &error, NULL)) == NULL)
		return (set_errno(error));

	if (copyin((char *)option_lenp, (char *)&optlen, sizeof (int))) {
		RELEASEF(sock);
		return (set_errno(EFAULT));
	}
	/*
	 * Verify that the length is not excessive to prevent
	 * an application from consuming all of kernel memory.
	 */
	if (optlen > SO_MAXARGSIZE || optlen < 0) {
		error = EINVAL;
		RELEASEF(sock);
		return (set_errno(error));
	}
	optval = kmem_alloc(optlen, KM_SLEEP);
	optlen_res = optlen;
	error = sogetsockopt(so, level, option_name, optval, &optlen_res);
	RELEASEF(sock);
	if (error)
		return (set_errno(error));
	error = copyout_arg((char *)option_value, optlen, option_lenp,
			    optval, optlen_res);
	kmem_free(optval, optlen);
	if (error)
		return (set_errno(error));
	return (0);
}

/*ARGSUSED5*/
int
setsockopt(int sock, int level, int option_name,
	void *option_value, int option_len, int version)
{
	struct sonode *so;
	void *optval;
	int error;

	dprint(1, ("setsockopt(%d, %d, %d, %x, %d)\n",
		sock, level, option_name, option_value, option_len));

	if ((so = getsonode(sock, &error, NULL)) == NULL)
		return (set_errno(error));

	if (option_len != 0) {
		/*
		 * Verify that the length is not excessive to prevent
		 * an application from consuming all of kernel memory.
		 */
		if (option_len > SO_MAXARGSIZE || option_len < 0) {
			error = EINVAL;
			goto done2;
		}
		optval = kmem_alloc(option_len, KM_SLEEP);
		if (copyin(option_value, optval, option_len)) {
			error = EFAULT;
			goto done1;
		}
	} else
		optval = NULL;

	error = sosetsockopt(so, level, option_name, optval, option_len);
done1:
	kmem_free(optval, option_len);
done2:
	RELEASEF(sock);
	if (error)
		return (set_errno(error));
	return (0);
}

/*
 * Add config info when devpath is non-NULL; delete info when devpath is NULL.
 * devpath is a user address.
 */
int
sockconfig(int domain, int type, int protocol, char *devpath)
{
	char *kdevpath;		/* Copied in devpath string */
	int kdevpathlen;
	size_t size;
	int error;

	dprint(1, ("sockconfig(%d, %d, %d, %x)\n",
		domain, type, protocol, devpath));

	if (!suser(CRED()))
		return (set_errno(EPERM));

	if (devpath == NULL) {
		/* Deleting an entry */
		kdevpath = NULL;
		kdevpathlen = 0;
	} else {
		/*
		 * Adding an entry.
		 * Copyin the devpath.
		 * Leave one byte to ensure NUL termination.
		 * This also makes it possible to check for too long pathnames.
		 * Compress the space needed for the devpath before passing it
		 * to soconfig - soconfig will store the string until
		 * the configuration is removed.
		 */
		char *kdevpath2;

		kdevpathlen = MAXPATHLEN + 1;	/* One for nul termination */
		kdevpath = kmem_zalloc(kdevpathlen, KM_SLEEP);

		if (copyinstr(devpath, kdevpath, kdevpathlen, &size)) {
			error = EFAULT;
			kmem_free(kdevpath, kdevpathlen);
			eprintline(error);
			return (set_errno(error));
		}
		if (size > MAXPATHLEN) {
			error = ENAMETOOLONG;
			kmem_free(kdevpath, kdevpathlen);
			eprintline(error);
			return (set_errno(error));
		}
		kdevpath2 = kmem_alloc(size, KM_SLEEP);
		bcopy(kdevpath, kdevpath2, size);
		kmem_free(kdevpath, kdevpathlen);
		kdevpath = kdevpath2;
		kdevpathlen = size;
	}
	error = soconfig(domain, type, protocol, kdevpath, kdevpathlen);
	if (error) {
		eprintline(error);
		return (set_errno(error));
	}
	return (0);
}
