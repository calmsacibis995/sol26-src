/*
 * Copyright (c) 1986-1996 by Sun Microsystems Inc.
 * All rights reserved.
 */

#ident	"@(#)key_call.c	1.41	96/09/08 SMI"

/*
 * key_call.c, Interface to keyserver
 *
 * setsecretkey(key) - set your secret key
 * encryptsessionkey(agent, deskey) - encrypt a session key to talk to agent
 * decryptsessionkey(agent, deskey) - decrypt ditto
 * gendeskey(deskey) - generate a secure des key
 */

#include "rpc_mt.h"
#include <errno.h>
#include <rpc/rpc.h>
#include <rpc/trace.h>
#include <rpc/key_prot.h>
#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#if defined(sparc)
#define	_FSTAT _fstat
#else  /* !sparc */
#define	_FSTAT fstat
#endif /* sparc */

#ifdef DEBUG
#define	debug(msg)	(void) fprintf(stderr, "%s\n", msg);
#else
#define	debug(msg)
#endif /* DEBUG */

int key_call();

/*
 * Hack to allow the keyserver to use AUTH_DES (for authenticated
 * NIS+ calls, for example).  The only functions that get called
 * are key_encryptsession_pk, key_decryptsession_pk, and key_gendes.
 *
 * The approach is to have the keyserver fill in pointers to local
 * implementations of these functions, and to call those in key_call().
 */

cryptkeyres *(*__key_encryptsession_pk_LOCAL)() = 0;
cryptkeyres *(*__key_decryptsession_pk_LOCAL)() = 0;
des_block *(*__key_gendes_LOCAL)() = 0;


key_setsecret(secretkey)
	const char *secretkey;
{
	keystatus status;

	trace1(TR_key_setsecret, 0);
	if (!key_call((u_long) KEY_SET, xdr_keybuf, (char *) secretkey,
			xdr_keystatus, &status)) {
		trace1(TR_key_setsecret, 1);
		return (-1);
	}
	if (status != KEY_SUCCESS) {
		debug("set status is nonzero");
		trace1(TR_key_setsecret, 1);
		return (-1);
	}
	trace1(TR_key_setsecret, 1);
	return (0);
}


/*
 * key_secretkey_is_set() returns 1 if the keyserver has a secret key
 * stored for the caller's effective uid; it returns 0 otherwise
 *
 * N.B.:  The KEY_NET_GET key call is undocumented.  Applications shouldn't
 * be using it, because it allows them to get the user's secret key.
 */
int
key_secretkey_is_set(void)
{
	struct key_netstres 	kres;

	trace1(TR_key_secretkey_is_set, 0);
	memset((void*)&kres, 0, sizeof (kres));
	if (key_call((u_long) KEY_NET_GET, xdr_void, (char *)NULL,
			xdr_key_netstres, (char *) &kres) &&
	    (kres.status == KEY_SUCCESS) &&
	    (kres.key_netstres_u.knet.st_priv_key[0] != 0)) {
		/* avoid leaving secret key in memory */
		memset(kres.key_netstres_u.knet.st_priv_key, 0, HEXKEYBYTES);
		trace1(TR_key_secretkey_is_set, 1);
		return (1);
	}
	trace1(TR_key_secretkey_is_set, 1);
	return (0);
}


key_encryptsession_pk(remotename, remotekey, deskey)
	char *remotename;
	netobj *remotekey;
	des_block *deskey;
{
	cryptkeyarg2 arg;
	cryptkeyres res;

	trace1(TR_key_encryptsession_pk, 0);
	arg.remotename = remotename;
	arg.remotekey = *remotekey;
	arg.deskey = *deskey;
	if (!key_call((u_long)KEY_ENCRYPT_PK, xdr_cryptkeyarg2, (char *)&arg,
			xdr_cryptkeyres, (char *)&res)) {
		trace1(TR_key_encryptsession_pk, 1);
		return (-1);
	}
	if (res.status != KEY_SUCCESS) {
		debug("encrypt status is nonzero");
		trace1(TR_key_encryptsession_pk, 1);
		return (-1);
	}
	*deskey = res.cryptkeyres_u.deskey;
	trace1(TR_key_encryptsession_pk, 1);
	return (0);
}

key_decryptsession_pk(remotename, remotekey, deskey)
	char *remotename;
	netobj *remotekey;
	des_block *deskey;
{
	cryptkeyarg2 arg;
	cryptkeyres res;

	trace1(TR_key_decryptsession_pk, 0);
	arg.remotename = remotename;
	arg.remotekey = *remotekey;
	arg.deskey = *deskey;
	if (!key_call((u_long)KEY_DECRYPT_PK, xdr_cryptkeyarg2, (char *)&arg,
			xdr_cryptkeyres, (char *)&res)) {
		trace1(TR_key_decryptsession_pk, 1);
		return (-1);
	}
	if (res.status != KEY_SUCCESS) {
		debug("decrypt status is nonzero");
		trace1(TR_key_decryptsession_pk, 1);
		return (-1);
	}
	*deskey = res.cryptkeyres_u.deskey;
	trace1(TR_key_decryptsession_pk, 1);
	return (0);
}

key_encryptsession(remotename, deskey)
	const char *remotename;
	des_block *deskey;
{
	cryptkeyarg arg;
	cryptkeyres res;

	trace1(TR_key_encryptsession, 0);
	arg.remotename = (char *) remotename;
	arg.deskey = *deskey;
	if (!key_call((u_long)KEY_ENCRYPT, xdr_cryptkeyarg, (char *)&arg,
			xdr_cryptkeyres, (char *)&res)) {
		trace1(TR_key_encryptsession, 1);
		return (-1);
	}
	if (res.status != KEY_SUCCESS) {
		debug("encrypt status is nonzero");
		trace1(TR_key_encryptsession, 1);
		return (-1);
	}
	*deskey = res.cryptkeyres_u.deskey;
	trace1(TR_key_encryptsession, 1);
	return (0);
}

key_decryptsession(remotename, deskey)
	const char *remotename;
	des_block *deskey;
{
	cryptkeyarg arg;
	cryptkeyres res;

	trace1(TR_key_decryptsession, 0);
	arg.remotename = (char *) remotename;
	arg.deskey = *deskey;
	if (!key_call((u_long)KEY_DECRYPT, xdr_cryptkeyarg, (char *)&arg,
			xdr_cryptkeyres, (char *)&res)) {
		trace1(TR_key_decryptsession, 1);
		return (-1);
	}
	if (res.status != KEY_SUCCESS) {
		debug("decrypt status is nonzero");
		trace1(TR_key_decryptsession, 1);
		return (-1);
	}
	*deskey = res.cryptkeyres_u.deskey;
	trace1(TR_key_decryptsession, 1);
	return (0);
}

key_gendes(key)
	des_block *key;
{
	trace1(TR_key_gendes, 0);
	if (!key_call((u_long)KEY_GEN, xdr_void, (char *)NULL,
			xdr_des_block, (char *)key)) {
		trace1(TR_key_gendes, 1);
		return (-1);
	}
	trace1(TR_key_gendes, 1);
	return (0);
}

key_setnet(arg)
struct key_netstarg *arg;
{
	keystatus status;


	if (!key_call((u_long) KEY_NET_PUT, xdr_key_netstarg, (char *) arg,
		xdr_keystatus, (char *) &status)) {
		return (-1);
	}

	if (status != KEY_SUCCESS) {
		debug("key_setnet status is nonzero");
		return (-1);
	}
	return (1);
}


int
key_get_conv(pkey, deskey)
	char *pkey;
	des_block *deskey;
{
	cryptkeyres res;

	trace1(TR_key_get_conv, 0);
	if (!key_call((u_long) KEY_GET_CONV, xdr_keybuf, pkey,
		xdr_cryptkeyres, (char *)&res)) {
		trace1(TR_key_get_conv, 1);
		return (-1);
	}
	if (res.status != KEY_SUCCESS) {
		debug("get_conv status is nonzero");
		trace1(TR_key_get_conv, 1);
		return (-1);
	}
	*deskey = res.cryptkeyres_u.deskey;
	trace1(TR_key_get_conv, 1);
	return (0);
}

struct  key_call_private {
	CLIENT	*client;	/* Client handle */
	pid_t	pid;		/* process-id at moment of creation */
	int	fd;		/* client handle fd */
	dev_t	rdev;		/* device client handle is using */
};
static struct key_call_private *key_call_private_main;

static void set_rdev(struct key_call_private *);
static int check_rdev(struct key_call_private *);

static void
key_call_destroy(void *vp)
{
	register struct key_call_private *kcp = (struct key_call_private *)vp;

	if (kcp != NULL) {
		if (kcp->client != NULL) {
			check_rdev(kcp);
			clnt_destroy(kcp->client);
			free(kcp);
		}
	}
}

/*
 * Keep the handle cached.  This call may be made quite often.
 */
static CLIENT *
getkeyserv_handle(vers, stale)
	int			vers;
	int			stale;
{
	static thread_key_t	key_call_key;
	struct key_call_private	*kcp = NULL;
	int			main_thread;
	extern mutex_t		tsd_lock;
	int			_update_did();

	if ((main_thread = _thr_main()))
		kcp = key_call_private_main;
	else {
		if (key_call_key == 0) {
			mutex_lock(&tsd_lock);
			if (key_call_key == 0)
				thr_keycreate(&key_call_key, key_call_destroy);
			mutex_unlock(&tsd_lock);
		}
		thr_getspecific(key_call_key, (void **) &kcp);
	}
	if (kcp == NULL) {
		kcp = (struct key_call_private *) malloc(sizeof (*kcp));
		if (kcp == NULL) {
			syslog(LOG_CRIT, "getkeyserv_handle: out of memory");
			return (NULL);
		}
		if (main_thread)
			key_call_private_main = kcp;
		else
			thr_setspecific(key_call_key, (void *) kcp);
		kcp->client = NULL;
	}

	/*
	 * if pid has changed, destroy client and rebuild
	 * or if stale is '1' then destroy client and rebuild
	 */
	if (kcp->client &&
	    (!check_rdev(kcp) || kcp->pid != getpid() || stale)) {
		clnt_destroy(kcp->client);
		kcp->client = NULL;
	}
	if (kcp->client) {
		/*
		 * Change the version number to the new one.
		 */
		clnt_control(kcp->client, CLSET_VERS, (void *)&vers);
		_update_did(kcp->client, vers);
		return (kcp->client);
	}

	if ((kcp->client = clnt_door_create(KEY_PROG, vers, 0)) == NULL)
		return (NULL);

	kcp->pid = getpid();
	set_rdev(kcp);
	_fcntl(kcp->fd, F_SETFD, 1);	/* make it "close on exec" */

	return (kcp->client);
}

/* returns  0 on failure, 1 on success */

key_call(proc, xdr_arg, arg, xdr_rslt, rslt)
	u_long proc;
	xdrproc_t xdr_arg;
	char *arg;
	xdrproc_t xdr_rslt;
	char *rslt;
{
	CLIENT		*clnt;
	struct timeval	wait_time = {0, 0};
	enum clnt_stat	status;
	int		vers;

	if (proc == KEY_ENCRYPT_PK && __key_encryptsession_pk_LOCAL) {
		cryptkeyres *res;
		res = (*__key_encryptsession_pk_LOCAL)(geteuid(), arg);
/* LINTED pointer alignment */
		*(cryptkeyres*)rslt = *res;
		return (1);
	} else if (proc == KEY_DECRYPT_PK && __key_decryptsession_pk_LOCAL) {
		cryptkeyres *res;
		res = (*__key_decryptsession_pk_LOCAL)(geteuid(), arg);
/* LINTED pointer alignment */
		*(cryptkeyres*)rslt = *res;
		return (1);
	} else if (proc == KEY_GEN && __key_gendes_LOCAL) {
		des_block *res;
		res = (*__key_gendes_LOCAL)(geteuid(), 0);
/* LINTED pointer alignment */
		*(des_block*)rslt = *res;
		return (1);
	}

	if ((proc == KEY_ENCRYPT_PK) || (proc == KEY_DECRYPT_PK) ||
	    (proc == KEY_NET_GET) || (proc == KEY_NET_PUT) ||
	    (proc == KEY_GET_CONV))
		vers = 2;	/* talk to version 2 */
	else
		vers = 1;	/* talk to version 1 */

	clnt = getkeyserv_handle(vers, 0);
	if (clnt == NULL)
		return (0);

	status = CLNT_CALL(clnt, proc, xdr_arg, arg, xdr_rslt,
			rslt, wait_time);

	switch (status) {
	case RPC_SUCCESS:
		return (1);

	case RPC_CANTRECV:
		/*
		 * keyserv was probably restarted, so we'll try once more
		 */
		if ((clnt = getkeyserv_handle(vers, 1)) == NULL)
			return (0);
		if (CLNT_CALL(clnt, proc, xdr_arg, arg, xdr_rslt, rslt,
						wait_time) == RPC_SUCCESS)
			return (1);
		return (0);

	default:
		return (0);
	}
}

static
void
set_rdev(kcp)
	struct key_call_private *kcp;
{
	int fd;
	struct stat stbuf;

	if (clnt_control(kcp->client, CLGET_FD, (char *)&fd) != TRUE ||
	    _FSTAT(fd, &stbuf) == -1) {
		syslog(LOG_DEBUG, "keyserv_client:  can't get info");
		kcp->fd = -1;
		return;
	}
	kcp->fd = fd;
	kcp->rdev = stbuf.st_rdev;
}

static
int
check_rdev(kcp)
	struct key_call_private *kcp;
{
	struct stat stbuf;

	if (kcp->fd == -1)
		return (1);    /* can't check it, assume it is okay */

	if (_FSTAT(kcp->fd, &stbuf) == -1) {
		syslog(LOG_DEBUG, "keyserv_client:  can't stat %d", kcp->fd);
		/* could be because file descriptor was closed */
		/* it's not our file descriptor, so don't try to close it */
		clnt_control(kcp->client, CLSET_FD_NCLOSE, (char *)NULL);

		return (0);
	}
	if (kcp->rdev != stbuf.st_rdev) {
		syslog(LOG_DEBUG,
		    "keyserv_client:  fd %d changed, old=0x%x, new=0x%x",
		    kcp->fd, kcp->rdev, stbuf.st_rdev);
		/* it's not our file descriptor, so don't try to close it */
		clnt_control(kcp->client, CLSET_FD_NCLOSE, (char *)NULL);
		return (0);
	}
	return (1);    /* fd is okay */
}
