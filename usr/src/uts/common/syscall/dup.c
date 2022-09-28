/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986, 1987, 1988, 1989  Sun Microsystems, Inc
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *			All rights reserved.
 *
 */

#ident	"@(#)dup.c	1.4	96/05/15 SMI"	/* SVr4 1.103	*/

#include <sys/param.h>
#include <sys/isa_defs.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/debug.h>


/*
 * common code for dup() and F_DUPFD
 */
int
dupfdes(file_t *fp, int start)
{
	int fd, error;

	if (error = ufalloc(start, &fd))
		return (set_errno(error));

	/*
	 * incr ref count before setf() to avoid race condition
	 */
	mutex_enter(&fp->f_tlock);
	fp->f_count++;
	mutex_exit(&fp->f_tlock);

	setf(fd, fp);

	/*
	 * since we know fd is good, lock once and load/store
	 * file flags instead of calling getpof() and setpof()
	 * which both get/drop u_flock.
	 */
	mutex_enter(&u.u_flock);
	u.u_flist[fd].uf_pofile &= ~FCLOSEXEC;
	mutex_exit(&u.u_flock);

	return (fd);
}


/*
 * Duplicate a file descriptor.
 */
int
dup(int fdes)
{
	file_t *fp;
	int fd;

	if ((fp = GETF(fdes)) == NULL)
		return (set_errno(EBADF));
	fd = dupfdes(fp, 0);
	RELEASEF(fdes);
	return (fd);
}


/*
 * dup a file descriptor to a specified slot
 */
int
dupdest(file_t *fp, int dest)
{
	register struct uf_entry *ufp;
	register struct user *up;
	register struct proc *p;
	file_t *fp2;

	mutex_enter(&fp->f_tlock);
	fp->f_count++;
	mutex_exit(&fp->f_tlock);

	p = curproc;
	up = PTOU(p);
	mutex_enter(&up->u_flock2);
	mutex_enter(&up->u_flock);

	/*
	 * if the target fd is in range, wait til the ref count
	 * is zero or wait for an in-progress close() to finish;
	 * otherwise, resize the u_flist array
	 */
	if (dest < up->u_nofiles) {
		ufp = &up->u_flist[dest];
		ufp->uf_pofile |= FRESERVED2;

		if ((fp2 = ufp->uf_ofile) != NULL) {
			ufp->uf_ofile = NULLFP;
			ufp->uf_pofile |= (FRESERVED|FCLOSING);
			while (ufp->uf_refcnt > 0) {
				cv_wait(&p->p_flock, &up->u_flock);
				ufp = &up->u_flist[dest];
			}
		} else if (ufp->uf_pofile & FCLOSING)
			cv_wait(&p->p_flock2, &up->u_flock);
	} else {
		fp2 = NULLFP;
		flist_realloc(up, dest);
	}

	ufp = &up->u_flist[dest];
	ufp->uf_ofile = fp;
	ufp->uf_pofile &= ~(FRESERVED2|FRESERVED|FCLOSING|FCLOSEXEC);

	mutex_exit(&up->u_flock);
	mutex_exit(&up->u_flock2);

	if (fp2)
		(void) closef(fp2);

	return (dest);
}
