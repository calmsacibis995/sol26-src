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
 * 	(c) 1986, 1987, 1988, 1989, 1996  Sun Microsystems, Inc
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *			All rights reserved.
 *
 */

#ident	"@(#)lseek.c	1.10	96/04/23 SMI"	/* SVr4 1.103	*/

#include <sys/param.h>
#include <sys/isa_defs.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/cred.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>

static int lseek_common(file_t *, int, offset_t, offset_t, offset_t *);

/*
 * Seek on file.
 * Large Files: lseek returns EOVERFLOW if we cannot represent the
 * resulting offset from seek in an off_t.
 * The following routines are sensitive to sign extensions and
 * calculations and if ever you change this make sure it works for
 * special files.
 */

off_t
lseek(int fdes, off_t off, int sbase)
{
	file_t *fp;
	int error;
	offset_t retoff;

	if ((fp = GETF(fdes)) == NULL)
		return ((offset_t)set_errno(EBADF));

	/*
	 * Nasty..but we have to do this. If VREG is set
	 * lseek_common checks for overflow to conform to
	 * large files API. But when VREG is not set we do the
	 * check for sbase != SEEK_SET to send the exact
	 * value to lseek_common and not the sign extended value.
	 * The maximum representable value is not checked by
	 * lseek_common for special files.
	 * Reverse programming: We suite this interface to work
	 * for programs using special files not the programs to
	 * use the right API!
	 */

	if (fp->f_vnode->v_type == VREG || sbase != 0) {
		error = lseek_common(fp, sbase, (offset_t)off,
			(offset_t)MAXOFF_T, &retoff);
	} else if (sbase == 0) {

		error = lseek_common(fp, sbase, (offset_t)(u_int)off,
			(offset_t)(u_int)UINT_MAX, &retoff);
	}

	RELEASEF(fdes);
	if (!error)
		return ((off_t)(retoff));
	return ((off_t)set_errno(error));
}

/*
 * Large Files: lseek64 traps to llseek.
 */


offset_t
llseek(int fdes, offset_t off, int sbase)
{
	file_t *fp;
	int error;
	offset_t retoff;

	if ((fp = GETF(fdes)) == NULL)
		return ((offset_t)set_errno(EBADF));
	error = lseek_common(fp, sbase, off, MAXOFFSET_T, &retoff);

	RELEASEF(fdes);
	if (!error)
		return (retoff);
	return ((offset_t)set_errno(error));
}

/*
 * max represents the maximum possible representation of offset
 * in the data type corresponding to lseek and llseek. It is
 * MAXOFF_T for off_t and MAXOFFSET_T for off64_t.
 * We return EOVERFLOW if we cannot represent the resulting offset
 * in the data type.
 * We provide support for character devices to be seeked beyond MAXOFF_T
 * by lseek and to maintain compatibility in such cases lseek passes
 * the arguments carefully to lseek_common when file is not regular.
 */

int lseek_debug = 0;

int
lseek_common(file_t *fp, int sbase, offset_t off, offset_t max,
		offset_t *retoff)
{
	vnode_t *vp;
	struct vattr vattr;
	register int error;
	u_offset_t noff;
	offset_t curoff, newoff;
	int reg;

	vp = fp->f_vnode;
	reg = (vp->v_type == VREG);

	curoff = fp->f_offset;

	/*
	 * Check for VREG because we have some special
	 * files /dev/kmem, character and block
	 * files that can be seeked to, using this
	 * interface beyond 2GB.
	 */

	switch (sbase) {

	case 0: /* SEEK_SET */
		noff = (u_offset_t)off;
		if (reg && noff > max) {
			error = EINVAL;
			goto out;
		}
		break;
	case 1: /* SEEK_CUR */
		if (reg && off > (max - curoff)) {
			error = EOVERFLOW;
			goto out;
		}
		noff = (u_offset_t)(off + curoff);
		if (reg && noff > max) {
			error = EINVAL;
			goto out;
		}
		break;
	case 2: /* SEEK_END */
		vattr.va_mask = AT_SIZE;
		if (error = VOP_GETATTR(vp, &vattr, 0, fp->f_cred)) {
			goto out;
		}
		if (reg && (off  > (max - (offset_t)vattr.va_size))) {
			error = EOVERFLOW;
			goto out;
		}
		noff = (u_offset_t)(off + (offset_t)vattr.va_size);
		if (reg && noff > max) {
			error = EINVAL;
			goto out;
		}
		break;
	default:
		error = EINVAL;
		goto out;
	}
	ASSERT((reg && noff <= max) || !reg);
	newoff = (offset_t)noff;
	if ((error = VOP_SEEK(vp, curoff, &newoff)) == 0) {
		fp->f_offset = newoff;
		(*retoff) = newoff;
		return (0);
	}
out:
	return (error);
}
