/*
 *	keyserv.c
 *
 *	Copyright (c) 1988-1996 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)keyserv.c	1.20	96/06/17 SMI"

/*
 * Keyserver
 * Store secret keys per uid. Do public key encryption and decryption
 * operations. Generate "random" keys.
 * Do not talk to anything but a local root
 * process on the local transport only
 */


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <string.h>
#include <rpc/rpc.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <pwd.h>
#include <rpc/des_crypt.h>
#include <rpc/key_prot.h>
#include <thread.h>
#include "rpc/svc_mt.h"

#ifdef KEYSERV_RANDOM
extern long random();
#endif
#ifndef NGROUPS
#define	NGROUPS 16
#endif

extern keystatus pk_setkey();
extern keystatus pk_encrypt();
extern keystatus pk_decrypt();
extern keystatus pk_netput();
extern keystatus pk_netget();
extern keystatus pk_get_conv_key();
extern bool_t svc_get_local_cred();
static void randomize();
static void usage();
static int getrootkey();
static bool_t get_auth();

#ifdef DEBUG
static int debugging = 1;
#else
static int debugging = 0;
#endif

static void keyprogram();
static des_block masterkey;
char *getenv();
static char ROOTKEY[] = "/etc/.rootkey";

/*
 * Hack to allow the keyserver to use AUTH_DES (for authenticated
 * NIS+ calls, for example).  The only functions that get called
 * are key_encryptsession_pk, key_decryptsession_pk, and key_gendes.
 *
 * The approach is to have the keyserver fill in pointers to local
 * implementations of these functions, and to call those in key_call().
 */

bool_t __key_encrypt_pk_2_svc();
bool_t __key_decrypt_pk_2_svc();
bool_t __key_gen_1_svc();

extern bool_t (*__key_encryptsession_pk_LOCAL)();
extern bool_t (*__key_decryptsession_pk_LOCAL)();
extern bool_t (*__key_gendes_LOCAL)();

static int nthreads = 32;

main(argc, argv)
	int argc;
	char *argv[];
{
	int nflag = 0;
	extern char *optarg;
	extern int optind;
	int c;
	struct rlimit rl;
	int mode = RPC_SVC_MT_AUTO;
	int detachfromtty();
	int setmodulus();
	int pk_nodefaultkeys();
	int svc_create_local_service();

	/*
	 * Set our allowed number of file descriptors to the max
	 * of what the system will allow, limited by FD_SETSIZE.
	 */
	if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
		rlim_t limit;

		if ((limit = rl.rlim_max) > FD_SETSIZE)
			limit = FD_SETSIZE;
		rl.rlim_cur = limit;
		(void) setrlimit(RLIMIT_NOFILE, &rl);
	}

	__key_encryptsession_pk_LOCAL = &__key_encrypt_pk_2_svc;
	__key_decryptsession_pk_LOCAL = &__key_decrypt_pk_2_svc;
	__key_gendes_LOCAL = &__key_gen_1_svc;

	while ((c = getopt(argc, argv, "ndDt:")) != -1)
		switch (c) {
		case 'n':
			nflag++;
			break;
		case 'd':
			pk_nodefaultkeys();
			break;
		case 'D':
			debugging = 1;
			break;
		case 't':
			nthreads = atoi(optarg);
			break;
		default:
			usage();
			break;
		}

	if (optind != argc) {
		usage();
	}

	/*
	 * Initialize
	 */
	(void) umask(066);	/* paranoia */
	if (geteuid() != 0) {
		(void) fprintf(stderr, "%s must be run as root\n", argv[0]);
		exit(1);
	}
	setmodulus(HEXMODULUS);
	getrootkey(&masterkey, nflag);

	/*
	 * Set MT mode
	 */
	if (nthreads > 0) {
		(void) rpc_control(RPC_SVC_MTMODE_SET, &mode);
		(void) rpc_control(RPC_SVC_THRMAX_SET, &nthreads);
	}

	if (svc_create_local_service(keyprogram, KEY_PROG, KEY_VERS,
		"netpath", "keyserv") == 0) {
		(void) fprintf(stderr,
			"%s: unable to create service\n", argv[0]);
		exit(1);
	}

	if (svc_create_local_service(keyprogram, KEY_PROG, KEY_VERS2,
		"netpath", "keyserv") == 0) {
		(void) fprintf(stderr,
			"%s: unable to create service\n", argv[0]);
		exit(1);
	}

	if (!debugging) {
		detachfromtty();
	}

	if (svc_create(keyprogram, KEY_PROG, KEY_VERS, "door") == 0) {
		(void) fprintf(stderr,
			"%s: unable to create service over doors\n", argv[0]);
		exit(1);
	}

	if (svc_create(keyprogram, KEY_PROG, KEY_VERS2, "door") == 0) {
		(void) fprintf(stderr,
			"%s: unable to create service over doors\n", argv[0]);
		exit(1);
	}

	svc_run();
	abort();
	/* NOTREACHED */
	return (0);
}


/*
 * In the event that we don't get a root password, we try to
 * randomize the master key the best we can
 */
static void
randomize(master)
	des_block *master;
{
	int i;
	int seed;
	struct timeval tv;
	int shift;

	seed = 0;
	for (i = 0; i < 1024; i++) {
		(void) gettimeofday(&tv, (struct timezone *) NULL);
		shift = i % 8 * sizeof (int);
		seed ^= (tv.tv_usec << shift) | (tv.tv_usec >> (32 - shift));
	}
#ifdef KEYSERV_RANDOM
	srandom(seed);
	master->key.low = random();
	master->key.high = random();
	srandom(seed);
#else
	/* use stupid dangerous bad rand() */
	srand(seed);
	master->key.low = rand();
	master->key.high = rand();
	srand(seed);
#endif
}

/*
 * Try to get root's secret key, by prompting if terminal is a tty, else trying
 * from standard input.
 * Returns 1 on success.
 */
static
getrootkey(master, prompt)
	des_block *master;
	int prompt;
{
	char *passwd;
	char name[MAXNETNAMELEN + 1];
	char secret[HEXKEYBYTES + 1];
	key_netstarg netstore;
	int fd;
	int passwd2des();

	if (!prompt) {
		/*
		 * Read secret key out of ROOTKEY
		 */
		fd = open(ROOTKEY, O_RDONLY, 0);
		if (fd < 0) {
			randomize(master);
			return (0);
		}
		if (read(fd, secret, HEXKEYBYTES) < HEXKEYBYTES) {
			(void) fprintf(stderr,
			    "keyserv: the key read from %s was too short.\n",
			    ROOTKEY);
			(void) close(fd);
			return (0);
		}
		(void) close(fd);
		if (!getnetname(name)) {
		    (void) fprintf(stderr, "keyserv: \
failed to generate host's netname when establishing root's key.\n");
		    return (0);
		}
		memcpy(netstore.st_priv_key, secret, HEXKEYBYTES);
		memset(netstore.st_pub_key, 0, HEXKEYBYTES);
		netstore.st_netname = name;
		if (pk_netput(0, &netstore) != KEY_SUCCESS) {
		    (void) fprintf(stderr,
			"keyserv: could not set root's key and netname.\n");
		    return (0);
		}
		return (1);
	}
	/*
	 * Decrypt yellow pages publickey entry to get secret key
	 */
	passwd = getpass("root password:");
	passwd2des(passwd, master);
	getnetname(name);
	if (!getsecretkey(name, secret, passwd)) {
		(void) fprintf(stderr,
		"Can't find %s's secret key\n", name);
		return (0);
	}
	if (secret[0] == 0) {
		(void) fprintf(stderr,
	"Password does not decrypt secret key for %s\n", name);
		return (0);
	}
	(void) pk_setkey(0, secret);
	/*
	 * Store it for future use in $ROOTKEY, if possible
	 */
	fd = open(ROOTKEY, O_WRONLY|O_TRUNC|O_CREAT, 0);
	if (fd > 0) {
		char newline = '\n';

		write(fd, secret, strlen(secret));
		write(fd, &newline, sizeof (newline));
		close(fd);
	}
	return (1);
}

/*
 * Procedures to implement RPC service.  These procedures are named
 * differently from the definitions in key_prot.h (generated by rpcgen)
 * because they take different arguments.
 */
char *
strstatus(status)
	keystatus status;
{
	switch (status) {
	case KEY_SUCCESS:
		return ("KEY_SUCCESS");
	case KEY_NOSECRET:
		return ("KEY_NOSECRET");
	case KEY_UNKNOWN:
		return ("KEY_UNKNOWN");
	case KEY_SYSTEMERR:
		return ("KEY_SYSTEMERR");
	default:
		return ("(bad result code)");
	}
}

bool_t
__key_set_1_svc(uid, key, status)
	uid_t uid;
	keybuf key;
	keystatus *status;
{
	if (debugging) {
		(void) fprintf(stderr, "set(%d, %.*s) = ", uid,
				sizeof (keybuf), key);
	}
	*status = pk_setkey(uid, key);
	if (debugging) {
		(void) fprintf(stderr, "%s\n", strstatus(*status));
		(void) fflush(stderr);
	}
	return (TRUE);
}

bool_t
__key_encrypt_pk_2_svc(uid, arg, res)
	uid_t uid;
	cryptkeyarg2 *arg;
	cryptkeyres *res;
{

	if (debugging) {
		(void) fprintf(stderr, "encrypt(%d, %s, %08x%08x) = ", uid,
				arg->remotename, arg->deskey.key.high,
				arg->deskey.key.low);
	}
	res->cryptkeyres_u.deskey = arg->deskey;
	res->status = pk_encrypt(uid, arg->remotename, &(arg->remotekey),
				&res->cryptkeyres_u.deskey);
	if (debugging) {
		if (res->status == KEY_SUCCESS) {
			(void) fprintf(stderr, "%08x%08x\n",
					res->cryptkeyres_u.deskey.key.high,
					res->cryptkeyres_u.deskey.key.low);
		} else {
			(void) fprintf(stderr, "%s\n", strstatus(res->status));
		}
		(void) fflush(stderr);
	}
	return (TRUE);
}

bool_t
__key_decrypt_pk_2_svc(uid, arg, res)
	uid_t uid;
	cryptkeyarg2 *arg;
	cryptkeyres *res;
{

	if (debugging) {
		(void) fprintf(stderr, "decrypt(%d, %s, %08x%08x) = ", uid,
				arg->remotename, arg->deskey.key.high,
				arg->deskey.key.low);
	}
	res->cryptkeyres_u.deskey = arg->deskey;
	res->status = pk_decrypt(uid, arg->remotename, &(arg->remotekey),
				&res->cryptkeyres_u.deskey);
	if (debugging) {
		if (res->status == KEY_SUCCESS) {
			(void) fprintf(stderr, "%08x%08x\n",
					res->cryptkeyres_u.deskey.key.high,
					res->cryptkeyres_u.deskey.key.low);
		} else {
			(void) fprintf(stderr, "%s\n", strstatus(res->status));
		}
		(void) fflush(stderr);
	}
	return (TRUE);
}

bool_t
__key_net_put_2_svc(uid, arg, status)
	uid_t uid;
	key_netstarg *arg;
	keystatus *status;
{

	if (debugging) {
		(void) fprintf(stderr, "net_put(%s, %.*s, %.*s) = ",
			arg->st_netname, sizeof (arg->st_pub_key),
			arg->st_pub_key, sizeof (arg->st_priv_key),
			arg->st_priv_key);
	};

	*status = pk_netput(uid, arg);

	if (debugging) {
		(void) fprintf(stderr, "%s\n", strstatus(*status));
		(void) fflush(stderr);
	}

	return (TRUE);
}

/* ARGSUSED */
bool_t
__key_net_get_2_svc(uid, arg, keynetname)
	uid_t uid;
	void *arg;
	key_netstres *keynetname;
{

	if (debugging)
		(void) fprintf(stderr, "net_get(%d) = ", uid);

	keynetname->status = pk_netget(uid, &keynetname->key_netstres_u.knet);
	if (debugging) {
		if (keynetname->status == KEY_SUCCESS) {
			fprintf(stderr, "<%s, %.*s, %.*s>\n",
			keynetname->key_netstres_u.knet.st_netname,
			sizeof (keynetname->key_netstres_u.knet.st_pub_key),
			keynetname->key_netstres_u.knet.st_pub_key,
			sizeof (keynetname->key_netstres_u.knet.st_priv_key),
			keynetname->key_netstres_u.knet.st_priv_key);
		} else {
			(void) fprintf(stderr, "NOT FOUND\n");
		}
		(void) fflush(stderr);
	}

	return (TRUE);

}

bool_t
__key_get_conv_2_svc(uid, arg, res)
	uid_t uid;
	keybuf arg;
	cryptkeyres *res;
{

	if (debugging)
		(void) fprintf(stderr, "get_conv(%d, %.*s) = ", uid,
			sizeof (arg), arg);


	res->status = pk_get_conv_key(uid, arg, res);

	if (debugging) {
		if (res->status == KEY_SUCCESS) {
			(void) fprintf(stderr, "%08x%08x\n",
				res->cryptkeyres_u.deskey.key.high,
				res->cryptkeyres_u.deskey.key.low);
		} else {
			(void) fprintf(stderr, "%s\n", strstatus(res->status));
		}
		(void) fflush(stderr);
	}
	return (TRUE);
}


bool_t
__key_encrypt_1_svc(uid, arg, res)
	uid_t uid;
	cryptkeyarg *arg;
	cryptkeyres *res;
{

	if (debugging) {
		(void) fprintf(stderr, "encrypt(%d, %s, %08x%08x) = ", uid,
				arg->remotename, arg->deskey.key.high,
				arg->deskey.key.low);
	}
	res->cryptkeyres_u.deskey = arg->deskey;
	res->status = pk_encrypt(uid, arg->remotename, NULL,
				&res->cryptkeyres_u.deskey);
	if (debugging) {
		if (res->status == KEY_SUCCESS) {
			(void) fprintf(stderr, "%08x%08x\n",
					res->cryptkeyres_u.deskey.key.high,
					res->cryptkeyres_u.deskey.key.low);
		} else {
			(void) fprintf(stderr, "%s\n", strstatus(res->status));
		}
		(void) fflush(stderr);
	}
	return (TRUE);
}

bool_t
__key_decrypt_1_svc(uid, arg, res)
	uid_t uid;
	cryptkeyarg *arg;
	cryptkeyres *res;
{
	if (debugging) {
		(void) fprintf(stderr, "decrypt(%d, %s, %08x%08x) = ", uid,
				arg->remotename, arg->deskey.key.high,
				arg->deskey.key.low);
	}
	res->cryptkeyres_u.deskey = arg->deskey;
	res->status = pk_decrypt(uid, arg->remotename, NULL,
				&res->cryptkeyres_u.deskey);
	if (debugging) {
		if (res->status == KEY_SUCCESS) {
			(void) fprintf(stderr, "%08x%08x\n",
					res->cryptkeyres_u.deskey.key.high,
					res->cryptkeyres_u.deskey.key.low);
		} else {
			(void) fprintf(stderr, "%s\n", strstatus(res->status));
		}
		(void) fflush(stderr);
	}
	return (TRUE);
}

/* ARGSUSED */
bool_t
__key_gen_1_svc(v, s, key)
	void *v;
	struct svc_req *s;
	des_block *key;
{
	struct timeval time;
	static des_block keygen;
	static mutex_t keygen_mutex = DEFAULTMUTEX;

	(void) gettimeofday(&time, (struct timezone *)NULL);
	(void) mutex_lock(&keygen_mutex);
	keygen.key.high += (time.tv_sec ^ time.tv_usec);
	keygen.key.low += (time.tv_sec ^ time.tv_usec);
	ecb_crypt((char *)&masterkey, (char *)&keygen, sizeof (keygen),
		DES_ENCRYPT | DES_HW);
	*key = keygen;
	mutex_unlock(&keygen_mutex);

	des_setparity((char *)key);
	if (debugging) {
		(void) fprintf(stderr, "gen() = %08x%08x\n", key->key.high,
					key->key.low);
		(void) fflush(stderr);
	}
	return (TRUE);
}

/* ARGSUSED */
bool_t
__key_getcred_1_svc(uid, name, res)
	uid_t uid;
	netnamestr *name;
	getcredres *res;
{
	struct unixcred *cred;

	cred = &res->getcredres_u.cred;
	if (!netname2user(*name, (uid_t *) &cred->uid, (gid_t *) &cred->gid,
			(int *)&cred->gids.gids_len,
					(gid_t *)cred->gids.gids_val)) {
		res->status = KEY_UNKNOWN;
	} else {
		res->status = KEY_SUCCESS;
	}
	if (debugging) {
		(void) fprintf(stderr, "getcred(%s) = ", *name);
		if (res->status == KEY_SUCCESS) {
			(void) fprintf(stderr, "uid=%d, gid=%d, grouplen=%d\n",
				cred->uid, cred->gid, cred->gids.gids_len);
		} else {
			(void) fprintf(stderr, "%s\n", strstatus(res->status));
		}
		(void) fflush(stderr);
	}
	return (TRUE);
}

/*
 * RPC boilerplate
 */
static void
keyprogram(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	union {
		keybuf key_set_1_arg;
		cryptkeyarg key_encrypt_1_arg;
		cryptkeyarg key_decrypt_1_arg;
		netnamestr key_getcred_1_arg;
		cryptkeyarg key_encrypt_2_arg;
		cryptkeyarg key_decrypt_2_arg;
		netnamestr key_getcred_2_arg;
		cryptkeyarg2 key_encrypt_pk_2_arg;
		cryptkeyarg2 key_decrypt_pk_2_arg;
		key_netstarg key_net_put_2_arg;
		netobj  key_get_conv_2_arg;
	} argument;
	union {
		keystatus status;
		cryptkeyres cres;
		des_block key;
		getcredres gres;
		key_netstres keynetname;
	} result;
	u_int gids[NGROUPS];
	char netname_str[MAXNETNAMELEN + 1];
	bool_t (*xdr_argument)(), (*xdr_result)();
	bool_t (*local)();
	bool_t retval;
	uid_t uid;
	int check_auth;

	switch (rqstp->rq_proc) {
	case NULLPROC:
		svc_sendreply(transp, xdr_void, (char *)NULL);
		return;

	case KEY_SET:
		xdr_argument = xdr_keybuf;
		xdr_result = xdr_int;
		local = __key_set_1_svc;
		check_auth = 1;
		break;

	case KEY_ENCRYPT:
		xdr_argument = xdr_cryptkeyarg;
		xdr_result = xdr_cryptkeyres;
		local = __key_encrypt_1_svc;
		check_auth = 1;
		break;

	case KEY_DECRYPT:
		xdr_argument = xdr_cryptkeyarg;
		xdr_result = xdr_cryptkeyres;
		local = __key_decrypt_1_svc;
		check_auth = 1;
		break;

	case KEY_GEN:
		xdr_argument = xdr_void;
		xdr_result = xdr_des_block;
		local = __key_gen_1_svc;
		check_auth = 0;
		break;

	case KEY_GETCRED:
		xdr_argument = xdr_netnamestr;
		xdr_result = xdr_getcredres;
		local = __key_getcred_1_svc;
		result.gres.getcredres_u.cred.gids.gids_val = gids;
		check_auth = 0;
		break;

	case KEY_ENCRYPT_PK:
		xdr_argument = xdr_cryptkeyarg2;
		xdr_result = xdr_cryptkeyres;
		local = __key_encrypt_pk_2_svc;
		check_auth = 1;
		break;

	case KEY_DECRYPT_PK:
		xdr_argument = xdr_cryptkeyarg2;
		xdr_result = xdr_cryptkeyres;
		local = __key_decrypt_pk_2_svc;
		check_auth = 1;
		break;


	case KEY_NET_PUT:
		xdr_argument = xdr_key_netstarg;
		xdr_result = xdr_keystatus;
		local = __key_net_put_2_svc;
		check_auth = 1;
		break;

	case KEY_NET_GET:
		xdr_argument = (xdrproc_t) xdr_void;
		xdr_result = xdr_key_netstres;
		local = __key_net_get_2_svc;
		result.keynetname.key_netstres_u.knet.st_netname = netname_str;
		check_auth = 1;
		break;

	case KEY_GET_CONV:
		xdr_argument = (xdrproc_t) xdr_keybuf;
		xdr_result = xdr_cryptkeyres;
		local = __key_get_conv_2_svc;
		check_auth = 1;
		break;

	default:
		svcerr_noproc(transp);
		return;
	}
	if (check_auth) {
		if (!get_auth(transp, rqstp, &uid)) {
			if (debugging) {
				(void) fprintf(stderr,
					"not local privileged process\n");
			}
			svcerr_weakauth(transp);
			return;
		}
	}

	memset((char *) &argument, 0, sizeof (argument));
	if (!svc_getargs(transp, xdr_argument, (caddr_t)&argument)) {
		svcerr_decode(transp);
		return;
	}
	retval = (*local)(uid, &argument, &result);
	if (retval && !svc_sendreply(transp, xdr_result, (char *)&result)) {
		if (debugging)
			(void) fprintf(stderr, "unable to reply\n");
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, xdr_argument, (caddr_t)&argument)) {
		if (debugging)
			(void) fprintf(stderr,
			"unable to free arguments\n");
		exit(1);
	}
}

static bool_t
get_auth(trans, rqstp, uid)
	SVCXPRT *trans;
	struct svc_req *rqstp;
	uid_t *uid;
{
	svc_local_cred_t cred;

	if (!svc_get_local_cred(trans, &cred)) {
		if (debugging)
			fprintf(stderr, "svc_get_local_cred failed %s %s\n",
				trans->xp_netid, trans->xp_tp);
		return (FALSE);
	}
	if (debugging)
		fprintf(stderr, "local_uid  %d\n", cred.euid);
	if (rqstp->rq_cred.oa_flavor == AUTH_SYS) {
/* LINTED pointer alignment */
		*uid = ((struct authunix_parms *)rqstp->rq_clntcred)->aup_uid;
		return (*uid == cred.euid || cred.euid == 0);
	} else {
		*uid = cred.euid;
		return (TRUE);
	}
}

static void
usage()
{
	(void) fprintf(stderr, "usage: keyserv [-n] [-D] [-d]\n");
	(void) fprintf(stderr, "-d disables the use of default keys\n");
	exit(1);
}
