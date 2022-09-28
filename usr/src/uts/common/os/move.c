/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996 Sun Microsystems, Inc.
 * All Rights Reserved.
 */
#ident	"@(#)move.c	1.25	96/09/24 SMI"	/* SVr4.0 1.12	*/

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/param.h>
#include <sys/user.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/strsubr.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <vm/as.h>

/*
 * Move "n" bytes at byte address "cp"; "rw" indicates the direction
 * of the move, and the I/O parameters are provided in "uio", which is
 * update to reflect the data which was moved.  Returns 0 on success or
 * a non-zero errno on failure.
 */
int
uiomove(caddr_t cp, size_t n, enum uio_rw rw, struct uio *uio)
{
	struct iovec *iov;
	ulong_t cnt;
	int error;

	while (n > (size_t)0 && uio->uio_resid) {
		iov = uio->uio_iov;
		cnt = MIN(iov->iov_len, n);
		if (cnt == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			continue;
		}
		switch (uio->uio_segflg) {

		case UIO_USERSPACE:
		case UIO_USERISPACE:
			if (rw == UIO_READ)
				error = xcopyout(cp, iov->iov_base, cnt);
			else
				error = xcopyin(iov->iov_base, cp, cnt);
			if (error)
				return (error);
			break;

		case UIO_SYSSPACE:
			if (rw == UIO_READ)
				error = kcopy((caddr_t)cp, iov->iov_base, cnt);
			else
				error = kcopy(iov->iov_base, (caddr_t)cp, cnt);
			if (error)
				return (error);
			break;
		}
		iov->iov_base += cnt;
		iov->iov_len -= cnt;
		uio->uio_resid -= cnt;
		uio->uio_loffset += cnt;
		cp += cnt;
		n -= cnt;
	}
	return (0);
}



/*
 * transfer a character value into the address space
 * delineated by a uio and update fields within the
 * uio for next character. Return 0 for success, EFAULT
 * for error.
 */
int
ureadc(int val, struct uio *uiop)
{
	struct iovec *iovp;
	unsigned char c;

	/*
	 * first determine if uio is valid.  uiop should be
	 * non-NULL and the resid count > 0.
	 */
	if (!(uiop && uiop->uio_resid > 0))
		return (EFAULT);

	/*
	 * scan through iovecs until one is found that is non-empty.
	 * Return EFAULT if none found.
	 */
	while (uiop->uio_iovcnt > 0) {
		iovp = uiop->uio_iov;
		if (iovp->iov_len <= 0) {
			uiop->uio_iovcnt--;
			uiop->uio_iov++;
		} else
			break;
	}

	if (uiop->uio_iovcnt <= 0)
		return (EFAULT);

	/*
	 * Transfer character to uio space.
	 */

	c = (unsigned char) (val & 0xFF);

	switch (uiop->uio_segflg) {

	case UIO_USERISPACE:
	case UIO_USERSPACE:
		if (copyout((caddr_t)&c, iovp->iov_base,
		    sizeof (unsigned char)))
			return (EFAULT);
		break;

	case UIO_SYSSPACE: /* can do direct copy since kernel-kernel */
		*iovp->iov_base = c;
		break;

	default:
		return (EFAULT); /* invalid segflg value */
	}

	/*
	 * bump up/down iovec and uio members to reflect transfer.
	 */
	iovp->iov_base++;
	iovp->iov_len--;
	uiop->uio_resid--;
	uiop->uio_loffset++;
	return (0); /* success */
}


/*
 * return a character value from the address space
 * delineated by a uio and update fields within the
 * uio for next character. Return the character for success,
 * -1 for error.
 */
int
uwritec(struct uio *uiop)
{
	struct iovec *iovp;
	unsigned char c;

	/*
	 * verify we were passed a valid uio structure.
	 * (1) non-NULL uiop, (2) positive resid count
	 * (3) there is an iovec with positive length
	 */

	if (!(uiop && uiop->uio_resid > 0))
		return (-1);

	while (uiop->uio_iovcnt > 0) {
		iovp = uiop->uio_iov;
		if (iovp->iov_len <= 0) {
			uiop->uio_iovcnt--;
			uiop->uio_iov++;
		} else
			break;
	}

	if (uiop->uio_iovcnt <= 0)
		return (-1);

	/*
	 * Get the character from the uio address space.
	 */
	switch (uiop->uio_segflg) {

	case UIO_USERISPACE:
	case UIO_USERSPACE:
		if (copyin(iovp->iov_base, (caddr_t)&c,
		    sizeof (unsigned char)))
			return (-1);
		break;

	case UIO_SYSSPACE:
		c = *iovp->iov_base;
		break;

	default:
		return (-1); /* invalid segflg */
	}

	/*
	 * Adjust fields of iovec and uio appropriately.
	 */
	iovp->iov_base++;
	iovp->iov_len--;
	uiop->uio_resid--;
	uiop->uio_loffset++;
	return ((int)c & 0xFF); /* success */
}

/*
 * Drop the next n chars out of *uiop.
 */
void
uioskip(uio_t *uiop, size_t n)
{
	if (n > uiop->uio_resid)
		return;
	while (n != 0) {
		register iovec_t	*iovp = uiop->uio_iov;
		register size_t		niovb = MIN(iovp->iov_len, n);

		if (niovb == 0) {
			uiop->uio_iov++;
			uiop->uio_iovcnt--;
			continue;
		}
		iovp->iov_base += niovb;
		uiop->uio_loffset += niovb;
		iovp->iov_len -= niovb;
		uiop->uio_resid -= niovb;
		n -= niovb;
	}
}

/*
 * Move MIN(ruio->uio_resid, wuio->uio_resid) bytes from addresses described
 * by ruio to those described by wuio.  Both uio structures are updated to
 * reflect the move. Returns 0 on success or a non-zero errno on failure.
 */
int
uiomvuio(uio_t *ruio, uio_t *wuio)
{
	iovec_t *riov;
	iovec_t *wiov;
	long n;
	uint cnt;
	int kerncp, err;

	n = MIN(ruio->uio_resid, wuio->uio_resid);
	kerncp = ruio->uio_segflg == UIO_SYSSPACE &&
	    wuio->uio_segflg == UIO_SYSSPACE;

	riov = ruio->uio_iov;
	wiov = wuio->uio_iov;
	while (n) {
		while (!wiov->iov_len) {
			wiov = ++wuio->uio_iov;
			wuio->uio_iovcnt--;
		}
		while (!riov->iov_len) {
			riov = ++ruio->uio_iov;
			ruio->uio_iovcnt--;
		}
		cnt = MIN(wiov->iov_len, MIN(riov->iov_len, n));

		if (kerncp)
			bcopy(riov->iov_base, wiov->iov_base, cnt);
		else if (ruio->uio_segflg == UIO_SYSSPACE) {
			if ((err = xcopyout(riov->iov_base,
			    wiov->iov_base, cnt)) != 0)
				return (err);
		} else {
			if ((err = xcopyin(riov->iov_base,
			    wiov->iov_base, cnt)) != 0)
				return (err);
		}

		riov->iov_base += cnt;
		riov->iov_len -= cnt;
		ruio->uio_resid -= cnt;
		ruio->uio_loffset += cnt;
		wiov->iov_base += cnt;
		wiov->iov_len -= cnt;
		wuio->uio_resid -= cnt;
		wuio->uio_loffset += cnt;
		n -= cnt;
	}
	return (0);
}

/*
 * Dup the suio into the duio and diovec of size diov_cnt. If diov
 * is too small to dup suio then an error will be returned, else 0.
 */
int
uiodup(uio_t  *suio, uio_t  *duio, iovec_t *diov, int diov_cnt)
{
	int ix;
	iovec_t *siov = suio->uio_iov;

	*duio = *suio;
	for (ix = 0; ix < suio->uio_iovcnt; ix++) {
		diov[ix] = siov[ix];
		if (ix >= diov_cnt)
			return (1);
	}
	duio->uio_iov = diov;
	return (0);
}

/*
 * Copyout and IP checksum "n" bytes at byte address "cp"; the I/O parameters
 * are provided in "uio", which is update to reflect the data which was moved,
 * and the partial IP checksum is provided by *sump and is returned in *sump.
 * Returns 0 on success or a non-zero errno on failure.
 */
int
uioipcopyout(cp, n, uio, sump, noblock)
	register caddr_t cp;
	register long n;
	register struct uio *uio;
	register unsigned short *sump;
	int noblock;
{
	register struct iovec *iov;
	u_int cnt;
	int odd = 0;
	int error;
	extern int ip_cksum_copyout();

	while (n > 0 && uio->uio_resid) {
		iov = uio->uio_iov;
		cnt = MIN(iov->iov_len, n);
		if (cnt == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			continue;
		}
		switch (uio->uio_segflg) {

		case UIO_USERSPACE:
		case UIO_USERISPACE:
			if (error = ip_cksum_copyout(
			    cp, iov->iov_base, cnt, sump, &odd, noblock))
				return (error);
			break;

		case UIO_SYSSPACE:
			return (EFAULT);

		default:
			return (EFAULT);
		}
		iov->iov_base += cnt;
		iov->iov_len -= cnt;
		uio->uio_resid -= cnt;
		uio->uio_loffset += cnt;
		cp += cnt;
		n -= cnt;
	}
	return (0);
}

/*
 * Copyin and IP checksum "n" bytes at byte address "cp"; the I/O parameters
 * are provided in "uio", which is update to reflect the data which was moved,
 * and the partial IP checksum is provided by *sump and is returned in *sump.
 * Returns 0 on success or a non-zero errno on failure.
 */
int
uioipcopyin(cp, n, uio, sump, noblock)
	register caddr_t cp;
	register long n;
	register struct uio *uio;
	register unsigned short *sump;
	int noblock;
{
	register struct iovec *iov;
	u_int cnt;
	int odd = 0;
	int error;
	extern int ip_cksum_copyin();

	while (n > 0 && uio->uio_resid) {
		iov = uio->uio_iov;
		cnt = MIN(iov->iov_len, n);
		if (cnt == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			continue;
		}
		switch (uio->uio_segflg) {

		case UIO_USERSPACE:
		case UIO_USERISPACE:
			if (error = ip_cksum_copyin(iov->iov_base, cp,
						    cnt, sump, &odd, noblock))
				return (error);
			break;

		case UIO_SYSSPACE:
			return (EFAULT);

		default:
			return (EFAULT);
		}
		iov->iov_base += cnt;
		iov->iov_len -= cnt;
		uio->uio_resid -= cnt;
		uio->uio_loffset += cnt;
		cp += cnt;
		n -= cnt;
	}
	return (0);
}

/*
 * Note: this does not ones-complement the result since it is used
 * when computing partial checksums.
 */
int
ip_cksum_copyout(kaddr, uaddr, cnt, sump, oddp, noblock)
	register u_char *kaddr;
	register u_char	*uaddr;
	register int cnt;
	register unsigned short *sump;
	register int *oddp;
	int noblock;
{
	unsigned int psum = 0;
	register int odd = cnt & 1;
	register int error;
	extern int ip_ocsum_copyout();
	extern unsigned int ip_ocsum();

	label_t *save_jmpbuf;
	auto label_t jmpbuf;

	if (noblock) {
		save_jmpbuf = curthread->t_nofault;
		curthread->t_nofault = &jmpbuf;
		if (setjmp(&jmpbuf)) {
			error = EWOULDBLOCK;
			goto done;
		}
	}

	if (!(curthread->t_proc_flag & TP_WATCHPT) &&
	    ((unsigned)uaddr & 0x1) == 0 &&
	    ((unsigned)uaddr & 0x3) == ((unsigned)kaddr & 0x3)) {
		/*
		 * We are not the subject of watchpoints and
		 * addresses meet ip_ocsum_copy* routine address
		 * alignment requirements, do things the fast way.
		 */
		if (error = ip_ocsum_copyout(kaddr, cnt >> 1, &psum, uaddr)) {
			if (error == ENOTSUP)
				goto slow;
			goto done;
		}
		if (odd) {
			/*
			 * One byte left over, so do it.
			 *
			 * Note: Change ip_ocsum_copyout() to
			 *	 use byte (! word) counts.
			 */
			cnt--;
			kaddr += cnt;
			uaddr += cnt;
			if (error = copyout((caddr_t)kaddr, (caddr_t)uaddr, 1))
				goto done;
#ifdef _LITTLE_ENDIAN
			psum += *kaddr;
#else
			psum += *kaddr << 8;
#endif
		}
	} else {
		/*
		 * Use seperate copy and checksum, first copyout
		 * the data.
		 */
	slow:;
		if (error = xcopyout((caddr_t)kaddr, (caddr_t)uaddr, cnt))
			goto done;

		if (((unsigned)kaddr & 0x1) != 0) {
			/*
			 * Kaddr isn't 16 bit aligned.
			 */
			unsigned int tsum;

#ifdef _LITTLE_ENDIAN
			psum += *kaddr;
#else
			psum += *kaddr << 8;
#endif
			cnt--;
			kaddr++;
			tsum = ip_ocsum(kaddr, cnt >> 1, 0);
			psum += (tsum << 8) & 0xffff | (tsum >> 8);
			if (cnt & 1) {
				kaddr += cnt - 1;
#ifdef _LITTLE_ENDIAN
				psum += *kaddr << 8;
#else
				psum += *kaddr;
#endif
			}
		} else {
			/*
			 * Kaddr is 16 bit alined.
			 */
			psum = ip_ocsum(kaddr, cnt >> 1, psum);
			if (odd) {
				kaddr += cnt - 1;
#ifdef _LITTLE_ENDIAN
				psum += *kaddr;
#else
				psum += *kaddr << 8;
#endif
			}
		}
	}
	if (*oddp) {
		/*
		 * Previous cnt was odd, so swap the partial checksum
		 * bytes after normalizing psum to 16 bits. The max
		 * psum value before normalization is 0x2FDFF.
		 */
		psum = (psum >> 16) + (psum & 0xFFFF);
		psum = (psum << 8) & 0xffff | (psum >> 8);
		if (odd)
			*oddp = 0;
	} else if (odd)
		*oddp = 1;
	/*
	 * Add in previous partial cheksum (if any) then normalize
	 * psum to 16 bits before returning the new partial checksum.
	 * The max psum value before normalization is 0x3FDFE.
	 */
	psum += *sump;
	psum = (psum >> 16) + (psum & 0xFFFF);
	*sump = psum;
done:
	if (noblock) {
		curthread->t_nofault = save_jmpbuf;
	}
	return (error);
}

/*
 * Note: this does not ones-complement the result since it is used
 * when computing partial checksums.
 */
int
ip_cksum_copyin(uaddr, kaddr, cnt, sump, oddp, noblock)
	register u_char *uaddr;
	register u_char	*kaddr;
	register int cnt;
	register unsigned short *sump;
	register int *oddp;
	int noblock;
{
	unsigned int psum = 0;
	register int odd = cnt & 1;
	register int error;
	extern int ip_ocsum_copyin();
	extern unsigned int ip_ocsum();
	auto label_t jmpbuf;
	label_t *save_jmpbuf;



	if (noblock) {
		save_jmpbuf = curthread->t_nofault;
		curthread->t_nofault = &jmpbuf;

		if (setjmp(&jmpbuf)) {
			error = EWOULDBLOCK;
			goto done;
		}
	}

	if (!(curthread->t_proc_flag & TP_WATCHPT) &&
	    ((unsigned)uaddr & 0x1) == 0 &&
	    ((unsigned)uaddr & 0x3) == ((unsigned)kaddr & 0x3)) {
		/*
		 * We are not the subject of watchpoints and
		 * addresses meet ip_ocsum_copy* routine address
		 * alignment requirements, do things the fast way.
		 */
		if (error = ip_ocsum_copyin(uaddr, cnt >> 1, &psum, kaddr)) {
			if (error == ENOTSUP)
				goto slow;
			goto done;
		}
		if (odd) {
			/*
			 * One byte left over, so do it.
			 *
			 * Note: Change ip_ocsum_copyout() to
			 *	 use byte (! word) counts.
			 */
			cnt--;
			kaddr += cnt;
			uaddr += cnt;
			if (error = copyin((caddr_t)uaddr, (caddr_t)kaddr, 1))
				goto done;
#ifdef _LITTLE_ENDIAN
			psum += *kaddr;
#else
			psum += *kaddr << 8;
#endif
		}
	} else {
		/*
		 * Use seperate copy and checksum, first copyin the data.
		 */
	slow:;
		if (error = xcopyin((caddr_t)uaddr, (caddr_t)kaddr, cnt))
			goto done;

		if (((unsigned)kaddr & 0x1) != 0) {
			/*
			 * Kaddr isn't 16 bit aligned.
			 */
			unsigned int tsum;

#ifdef _LITTLE_ENDIAN
			psum += *kaddr;
#else
			psum += *kaddr << 8;
#endif
			cnt--;
			kaddr++;
			tsum = ip_ocsum(kaddr, cnt >> 1, 0);
			psum += (tsum << 8) & 0xffff | (tsum >> 8);
			if (cnt & 1) {
				kaddr += cnt - 1;
#ifdef _LITTLE_ENDIAN
				psum += *kaddr << 8;
#else
				psum += *kaddr;
#endif
			}
		} else {
			/*
			 * Kaddr is 16 bit alined.
			 */
			psum = ip_ocsum(kaddr, cnt >> 1, psum);
			if (odd) {
				kaddr += cnt - 1;
#ifdef _LITTLE_ENDIAN
				psum += *kaddr;
#else
				psum += *kaddr << 8;
#endif
			}
		}
	}
	if (*oddp) {
		/*
		 * Previous cnt was odd, so swap the partial checksum
		 * bytes after normalizing psum to 16 bits. The max
		 * psum value before normalization is 0x2FDFF.
		 */
		psum = (psum >> 16) + (psum & 0xFFFF);
		psum = (psum << 8) & 0xffff | (psum >> 8);
		if (odd)
			*oddp = 0;
	} else if (odd)
		*oddp = 1;
	/*
	 * Add in previous partial cheksum (if any) then normalize
	 * psum to 16 bits before returning the new partial checksum.
	 * The max psum value before normalization is 0x3FDFE.
	 */
	psum += *sump;
	psum = (psum >> 16) + (psum & 0xFFFF);
	*sump = psum;
done:;
	if (noblock) {
		curthread->t_nofault = save_jmpbuf;
	}
	return (error);
}

/*
 * This function is like uiomove()/UIO_READ except it tries to flip the
 * pages involved, instead of physically copying the data.
 * For those bytes that as_pageflip() fails to flip, unless real error
 * such as protection violation is encountered, this function will
 * call copyout() to complete the job.
 *
 * Besides possible errors returned by uiomove(), a new error ENOTSUP is
 * invented to tell the caller that the VM segment(s) behind the
 * uio buffer is not eligible for page-flipping. Upon seeing that,
 * the caller may want to call straight into uiomove() for future
 * uio requests.
 */
int
uiopageflip(cp, n, uio)
	register caddr_t cp;
	register long n;
	register struct uio *uio;
{
	register struct iovec *iov;
	u_int cnt, remaining;
	int remap_off = 0;
	int error = 0;
	faultcode_t res;

	while (n > 0 && uio->uio_resid) {
		iov = uio->uio_iov;
		cnt = MIN(iov->iov_len, n);
		if (cnt == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			continue;
		}
		switch (uio->uio_segflg) {

		case UIO_USERSPACE:
		case UIO_USERISPACE:
			remaining = cnt;
			if ((cnt & PAGEOFFSET) == 0 &&
			    ((unsigned)cp & PAGEOFFSET) == 0 &&
			    ((unsigned)iov->iov_base & PAGEOFFSET) == 0) {

				res = as_pageflip(curproc->p_as,
				    iov->iov_base, cp, &remaining);
				/* remaining contains # of bytes not flipped. */
				ASSERT(res == 0 || remaining != 0);

				if (res == FC_NOMAP) {
					/*
					 * User memory is not mapped. Page
					 * it in and try as_pageflip again.
					 */
					(void) as_fault(curproc->p_as->a_hat,
					    curproc->p_as,
					    iov->iov_base+(cnt-remaining),
					    remaining, F_INVAL, S_WRITE);
					res = as_pageflip(curproc->p_as,
					    iov->iov_base+(cnt-remaining),
					    cp+(cnt-remaining), &remaining);
				}
				zckstat->zc_pageflips.value.ul +=
				    (cnt-remaining) >> PAGESHIFT;
				if (remaining == 0) {
					/* we're done. */
					break;
				}
				zckstat->zc_misses.value.ul +=
				    remaining >> PAGESHIFT;
				switch (res) {
				case FC_PROT:
					return (EFAULT);
				case FC_NOSUPPORT:
					remap_off = 1;
					break;
				case FC_NOMAP:
				case FC_OBJERR:
				default:
					break;
				}
			}
			error = xcopyout(cp+(cnt-remaining),
			    iov->iov_base+(cnt-remaining), remaining);
			if (error)
				return (error);
			break;

		case UIO_SYSSPACE:
			error = kcopy((caddr_t)cp, iov->iov_base, cnt);
			if (error)
				return (error);
			break;
		}
		iov->iov_base += cnt;
		iov->iov_len -= cnt;
		uio->uio_resid -= cnt;
		uio->uio_loffset += cnt;
		cp += cnt;
		n -= cnt;
	}
	if (remap_off)
		return (ENOTSUP);
	else
		return (0);
}
