/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)rpcsys.c	1.2	96/09/09 SMI"	/* SVr4.0 1.5	*/

/*
 *  		PROPRIETARY NOTICE (Combined)
 *
 *  This source code is unpublished proprietary information
 *  constituting, or derived under license from AT&T's Unix(r) System V.
 *
 *
 *		Copyright Notice
 *
 *  Notice of copyright on this source code product does not indicate
 *  publication.
 *
 *  	Copyright (c) 1986-1989,1993,1994,1995 by Sun Microsystems, Inc.
 *  	Copyright (c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	All rights reserved.
 */

#include <sys/types.h>
#include <rpc/types.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/errno.h>
#include <sys/cred.h>
#include <rpc/rpcsys.h>


/*ARGSUSED*/
int
rpcsys(struct rpcsysa *uap, rval_t *rvp)
{

	switch ((int) uap->opcode) {
		case KRPC_REVAUTH:
			/* revoke the cached credentials for the given uid */
			{
				struct krpc_revauth	nra;
				int			result;

				if (copyin((caddr_t) uap->rpcsysarg_revauth,
					    (caddr_t) &nra, sizeof (nra))) {
					return (EFAULT);
				} else {
					result = sec_clnt_revoke(
						nra.rpcsec_flavor_1,
						nra.uid_1, CRED(),
						nra.flavor_data_1);
					return (result);
				}
			}

		default:
			return (EINVAL);
	}
}
