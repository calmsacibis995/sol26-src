/*
 *	nfs_subr.c
 *
 *	Copyright (c) 1996 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)nfs_subr.c	1.2	96/06/06 SMI"

#include <sys/types.h>
#include <rpcsvc/nlm_prot.h>
#include <sys/utsname.h>
#include <nfs/nfs.h>
#include "nfs_subr.h"

/*
 * This function is added to detect compatibility problem with SunOS4.x.
 * The compatibility problem exists when fshost cannot decode the request
 * arguments for NLM_GRANTED procedure.
 * Only in this case  we use local locking.
 * In any other case we use fshost's lockd for remote file locking.
 */
int
remote_lock(char *fshost, caddr_t fh)
{
	nlm_testargs rlm_args;
	nlm_res rlm_res;
	struct timeval timeout = { 5, 0};
	CLIENT *cl;
	enum clnt_stat rpc_stat;
	struct utsname myid;

	(void) memset((char *) &rlm_args, 0, sizeof (nlm_testargs));
	(void) memset((char *) &rlm_res, 0, sizeof (nlm_res));
	/*
	 * Assign the hostname and the file handle for the
	 * NLM_GRANTED request below.  If for some reason the uname call fails,
	 * list the server as the caller so that caller_name has some
	 * reasonable value.
	 */
	if (uname(&myid) == -1)  {
		rlm_args.alock.caller_name = fshost;
	} else {
		rlm_args.alock.caller_name = myid.nodename;
	}
	rlm_args.alock.fh.n_len = sizeof (fhandle_t);
	rlm_args.alock.fh.n_bytes = fh;

	cl = clnt_create(fshost, NLM_PROG, NLM_VERS, "datagram_v");
	if (cl == NULL)
		return (1);

	rpc_stat = clnt_call(cl, NLM_GRANTED,
			xdr_nlm_testargs, (caddr_t)&rlm_args,
			xdr_nlm_res, (caddr_t)&rlm_res, timeout);
	clnt_destroy(cl);

	return (rpc_stat == RPC_CANTDECODEARGS);
}
