/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_RPC_RPCSYS_H
#define	_RPC_RPCSYS_H

#pragma ident	"@(#)rpcsys.h	1.5	96/09/09 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

enum rpcsys_op  { KRPC_REVAUTH };

/*
 * Private definitions for the krpc_sys/rpcsys system call.
 *
 * flavor_data for AUTH_DES and AUTH_KERB is NULL.
 * flavor_data for RPCSEC_GSS is rpc_gss_OID.
 *
 */
struct krpc_revauth_1 {
	uid_t	uid;
	int	rpcsec_flavor;
	void	*flavor_data;
};

struct krpc_revauth {
	int	version;	/* initially 1 */
	union	{
		struct krpc_revauth_1 r;
	} krpc_revauth_u;
};
#define	uid_1		krpc_revauth_u.r.uid
#define	rpcsec_flavor_1	krpc_revauth_u.r.rpcsec_flavor
#define	flavor_data_1	krpc_revauth_u.r.flavor_data


#ifdef _KERNEL
union rpcsysargs {
	struct krpc_revauth	*krpc_revauth_args_u;	/* krpc_revauth args */
};

struct rpcsysa {
	enum rpcsys_op		opcode;	/* operation discriminator */
	union rpcsysargs	arg;	/* syscall-specific arg pointer */
};
#define	rpcsysarg_revauth	arg.krpc_revauth_args_u
#endif	/* _KERNEL */

#ifdef _KERNEL

#include <sys/systm.h>		/* for rval_t typedef */

extern	int	rpcsys(register struct rpcsysa *, rval_t *);
extern	int	sec_clnt_revoke(int, uid_t, cred_t *, char *);

#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _RPC_RPCSYS_H */
