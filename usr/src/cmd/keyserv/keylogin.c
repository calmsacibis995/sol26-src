/*
 *	keylogin.c
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)keylogin.c	1.11	95/11/29 SMI"

/*
 * Set secret key on local machine
 */
#include <stdio.h>
#include <rpc/rpc.h>
#include <rpc/key_prot.h>
#include <nfs/nfs.h>				/* to revoke existing creds */
#include <nfs/nfssys.h>
#include <string.h>

#define	ROOTKEY_FILE "/etc/.rootkey"

static void logout_curr_key();


void
usage(cmd)
	char *cmd;
{
	fprintf(stderr, "usage: %s [-r]\n", cmd);
	exit(1);
}


main(argc, argv)
	int argc;
	char *argv[];
{
	char secret[HEXKEYBYTES + 1];
	char fullname[MAXNETNAMELEN + 1];
	char *getpass();
	struct key_netstarg netst;
	int mkrootkey, fd;

	if (argc == 1)
		mkrootkey = 0;
	else if (argc == 2 && (strcmp(argv[1], "-r") == 0)) {
		if (geteuid() != 0) {
			fprintf(stderr, "Must be root to use -r option.\n");
			exit(1);
		}
		mkrootkey = 1;
	} else
		usage(argv[0]);

	if (getnetname(fullname) == 0) {
		fprintf(stderr, "Could not generate netname\n");
		exit(1);
	}

	if (getsecretkey(fullname, secret, getpass("Password:")) == 0) {
		fprintf(stderr, "Could not find %s's secret key\n", fullname);
		exit(1);
	}

	if (secret[0] == 0) {
		fprintf(stderr, "Password incorrect for %s\n", fullname);
		exit(1);
	}

	/* revoke any existing (lingering) credentials... */
	logout_curr_key();

	memcpy(netst.st_priv_key, secret, HEXKEYBYTES);
	memset(secret, 0, HEXKEYBYTES);

	netst.st_pub_key[0] = 0;
	netst.st_netname = strdup(fullname);

	/* do actual key login */
	if (key_setnet(&netst) < 0) {
		fprintf(stderr, "Could not set %s's secret key\n", fullname);
		fprintf(stderr, "May be the keyserv is down?\n");
		if (mkrootkey == 0)   /* nothing else to do */
			exit(1);
	}

	/* write unencrypted secret key into root key file */
	if (mkrootkey == 1) {
		strcat(netst.st_priv_key, "\n");
		unlink(ROOTKEY_FILE);
		if ((fd = open(ROOTKEY_FILE, O_WRONLY+O_CREAT, 0600)) != -1) {
			write(fd, netst.st_priv_key,
						strlen(netst.st_priv_key)+1);
			close(fd);
			fprintf(stderr, "Wrote secret key into %s\n",
				ROOTKEY_FILE);
		} else {
			fprintf(stderr, "Could not open %s for update\n",
				ROOTKEY_FILE);
			exit(1);
		}
	}

	exit(0);
}


/*
 * Revokes the existing credentials for Secure-RPC and Secure-NFS.
 * This should only be called if the user entered the correct password;
 * sorta like the way "su" doesn't force a login if you enter the wrong
 * password.
 */

static void
logout_curr_key()
{
	static char		secret[HEXKEYBYTES + 1];
	struct nfs_revauth_args	nra;

	/*
	 * try to revoke the existing key/credentials, assuming
	 * one exists.  this will effectively mark "stale" any
	 * cached credientials...
	 */
	if (key_setsecret(secret) < 0) {
		return;
	}

	/*
	 * it looks like a credential already existed, so try and
	 * revoke any lingering Secure-NFS privledges.
	 */

	nra.authtype = AUTH_DES;
	nra.uid = getuid();

	(void) _nfssys(NFS_REVAUTH, &nra);
}
