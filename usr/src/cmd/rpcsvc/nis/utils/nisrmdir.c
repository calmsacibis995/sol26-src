/*
 *	nisrmdir.c
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)nisrmdir.c	1.16	96/05/13 SMI"

/*
 * nisrmdir.c
 *
 * nis+ dir remove utility
 */

#include <stdio.h>
#include <string.h>
#include <rpc/rpc.h>
#include <rpcsvc/nis.h>
#include <netdb.h>
#include <signal.h>
#include <netdir.h>
#include <netconfig.h>
#include <sys/socket.h>
#include <rpcsvc/nis.h>
#include <netinet/in.h>

extern int 	optind;
extern char	*optarg;

#define	ROOT_OBJ "root.object"
extern nis_name __nis_local_root();

char fname[NIS_MAXNAMELEN];
nis_object *obj;
int nserv;
nis_server *servers;

static
int
match_host(char *host, char *target)
{
	int len = strlen(host);

	if (strncasecmp(host, target, len) == 0 &&
	    (target[len] == '.' || target[len] == '\0'))
		return (1);

	return (0);
}

void
cleanup_rmdir()
{
	int ns, i;

	/*
	 * put any servers that weren't removed (non-nil name) back into the
	 * directory.
	 */
	for (ns = 0, i = 0; i < nserv; i++) {
		if (servers[i].name) {
			if (ns == i)
				ns++;
			else
				servers[ns++] = servers[i];
		}
	}

	if (ns) {
		obj->DI_data.do_servers.do_servers_len = ns;
		(void) nis_add(fname, obj);
	}

	exit(1);
}

nis_server sserv;
int sservi;

void
cleanup_rmslave()
{
	nis_result *res;

	/*
	 * put the slave back in the directory.
	 */
	res = nis_lookup(fname, MASTER_ONLY);
	if (res->status == NIS_SUCCESS) {
		obj->zo_oid = NIS_RES_OBJECT(res)[0].zo_oid;
		obj->DI_data.do_servers.do_servers_len++;
		servers[sservi] = sserv;
		(void) nis_modify(fname, obj);
	}

	exit(1);
}

/*
 * remove_directory is a special rmdir that takes care of the root case.
 * This is needed only for the -s option when the host or directory object
 * specified in the removal no longer exists.  Otherwise,
 * the nis_remove operation on the directory object would result in
 * the replicas receiving pings to remove the root object in the root case.
 */
nis_error
remove_directory(nis_name directory, nis_server* server, int verbose)
{
	nis_error s = nis_rmdir(directory, server);

	if (s != NIS_SUCCESS) {
		if (verbose)
			fprintf(stderr, "cannot remove replica %s: %s.\n",
				server->name, nis_sperrno(s));
		return (s);
	}
	/* root replica: send ping to remove root object. */
	if (nis_dir_cmp(__nis_local_root(), directory) == SAME_NAME) {
		__nis_pingproc(server, ROOT_OBJ, time(0));
	}

	return (s);
}

/*
 * get_server()
 *
 * (snatched from nisinit.c and modified slightly)
 *
 * This function constructs a local server description of the current
 * server and returns it as a nis_server structure.
 */
nis_server *
get_server(host)
	char	*host;
{
	static endpoint  	addr[20];
	static nis_server 	hostinfo;
	int			num_ep = 0, i;
	struct netconfig	*nc;
	void			*nch;
	struct nd_hostserv	hs;
	struct nd_addrlist	*addrs;

	hs.h_host = host;
	hs.h_serv = "rpcbind";
	nch = setnetconfig();
	while (nc = getnetconfig(nch)) {
		if (! netdir_getbyname(nc, &hs, &addrs)) {
			for (i = 0; i < addrs->n_cnt; i++, num_ep++) {
				addr[num_ep].uaddr =
				taddr2uaddr(nc, &(addrs->n_addrs[i]));
				addr[num_ep].family =
					    strdup(nc->nc_protofmly);
				addr[num_ep].proto =
					    strdup(nc->nc_proto);
			}
			netdir_free((char *)addrs, ND_ADDRLIST);
		}
	}
	endnetconfig(nch);

	if (! num_ep) {
		fprintf(stderr,
		    "\nError: Unable to find address of host '%s'\n",
			hs.h_host);
		exit(1);
	}
	hostinfo.name = strdup(host);
	hostinfo.ep.ep_len = num_ep;
	hostinfo.ep.ep_val = &addr[0];

	/* no need for keys because nisrmdir does no access control */
	hostinfo.key_type = NIS_PK_NONE;
	hostinfo.pkey.n_bytes = NULL;
	hostinfo.pkey.n_len = 0;
	return (&hostinfo);
}

#define	OP_RMDIR 0
#define	OP_RMSLAVE 2

void
usage()
{
	fprintf(stderr, "usage: nisrmdir [-if] [-s hostname] dirname\n");
	exit(1);
}

main(argc, argv)
	int argc;
	char *argv[];
{
	int c;
	char ask_remove = 0, force_remove = 0;
	int op = OP_RMDIR;
	u_long expand;
	char buf[BUFSIZ];
	char *host = 0;
	char *name;
	nis_result *res, *rres, *mres;
	nis_error s;
	int i, nur, found;

	while ((c = getopt(argc, argv, "ifs:")) != -1) {
		switch (c) {
		case 'i':
			ask_remove = 1;
			break;
		case 'f':
			force_remove = 1;
			break;
		case 's':
			op = OP_RMSLAVE;
			host = optarg;
			break;
		default:
			usage();
		}
	}

	if (argc - optind != 1)
		usage();

	name = argv[optind];

	if (name[strlen(name)-1] != '.')
		expand = EXPAND_NAME;
	else
		expand = 0;

	/*
	 * Get the directory object.
	 */
	res = nis_lookup(name, expand|MASTER_ONLY);
	if (res->status != NIS_SUCCESS) {
		if (force_remove) {
			/*
			 * If specifying host, maybe trying to clean up
			 * directory that has already been removed.
			 */
			if (host && (res->status == NIS_NOTFOUND ||
				    res->status == NIS_NOSUCHNAME)) {
				sserv = *get_server(host);
				if (expand == 0)
					strcpy(fname, name);
				else
					sprintf(fname, "%s.%s", name,
						nis_local_directory());
				s = remove_directory(fname, &sserv, 0);
			}
			exit(0);
		}
		nis_perror(res->status, name);
		exit(1);
	}

	sprintf(fname, "%s.", res->objects.objects_val[0].zo_name);
	if (*(res->objects.objects_val[0].zo_domain) != '.')
		strcat(fname, res->objects.objects_val[0].zo_domain);

	if (res->objects.objects_val[0].zo_data.zo_type != NIS_DIRECTORY_OBJ) {
		fprintf(stderr, "%s is not a directory!\n", fname);
		exit(1);
	}

	if (ask_remove || expand) {
		printf("remove %s? ", fname);
		gets(buf);
		if (tolower(*buf) != 'y')
			exit(0);
	}

	obj = &(NIS_RES_OBJECT(res)[0]);
	nserv = obj->DI_data.do_servers.do_servers_len;
	servers = obj->DI_data.do_servers.do_servers_val;

	switch (op) {
	case OP_RMDIR:
		/*
		 * remove directory object
		 */
		rres = nis_remove(fname, 0);
		if ((rres->status == NIS_PERMISSION) && force_remove) {
			obj->zo_access |= 0x08080808;
			nis_freeresult(rres);
			rres = nis_modify(fname, obj);
			if (rres->status == NIS_SUCCESS) {
				nis_freeresult(rres);
				rres = nis_remove(fname, 0);
			}
		}
		if (rres->status != NIS_SUCCESS) {
			if (force_remove)
				exit(0);
			nis_perror(rres->status, "can't remove directory");
			exit(1);
		}

		/*
		 * fork a child and *try* to do nis_rmdirs.  this may take
		 * a while since we may try to talk to servers that aren't
		 * up/responding.
		 */
		if (force_remove)
			switch (fork()) {
			case 0:
				for (i = nserv-1; i >= 0; i--)
					nis_rmdir(fname, &(servers[i]));
			default:
				exit(0);
			}

		signal(SIGINT, (void(*)(int))cleanup_rmdir);

		/*
		 * remove slave directories
		 */
		for (nur = 0, i = 1; i < nserv; i++) {
			s = nis_rmdir(fname, &(servers[i]));
			if (s != NIS_SUCCESS) {
				nur++;
				fprintf(stderr,
					"cannot remove replica %s: %s.\n",
					servers[i].name, nis_sperrno(s));
			} else
				servers[i].name = 0;
		}
		if (nur)
			cleanup_rmdir();

		/*
		 * if all slave directories were removed, remove master
		 */
		s = nis_rmdir(fname, &(servers[0]));
		if (s != NIS_SUCCESS) {
			fprintf(stderr, "cannot remove master %s: %s.\n",
				servers[0].name, nis_sperrno(s));
			cleanup_rmdir();
		}

		exit(0);

	case OP_RMSLAVE:
		/*
		 * find the slave.
		 */
		for (found = -1, i = 0; i < nserv; i++) {
			if (match_host(host, servers[i].name)) {
					if (found >= 0) {
						fprintf(stderr,
			"%s is not unique, please use full host name.\n",
							host);
						exit(1);
					}
					found = i;
			}
		}

		/*
		 * Host no longer listed as a replica.  Try anyhow; perhaps
		 * user trying to clean up.
		 */
		if (found == -1) {
			if (!force_remove) {
				fprintf(stderr,
		"Host %s is not listed as a replica for %s.\n", host, fname);
				fprintf(stderr,
		"Use the -fs option to attempt rmdir anyhow.\n");
				exit(1);
			}

			sserv = *get_server(host);
			s = remove_directory(fname, &sserv, 0);
			exit(0);
		}

		/* Host named is master */
		if (found == 0) {
			if (force_remove)
				exit(0);
			fprintf(stderr, "%s is master for %s!\n",
				servers[0].name, fname);
			exit(1);
		}
		sserv = servers[found];
		sservi = found;

		/*
		 * remove slave from the directory object.
		 */
		nserv = --(obj->DI_data.do_servers.do_servers_len);
		if (found < nserv) {
			servers[found] = servers[nserv];
		}
		mres = nis_modify(fname, obj);
		if (mres->status != NIS_SUCCESS) {
			if (force_remove)
				exit(0);
			fprintf(stderr, "cannot remove replica %s: %s.\n",
				sserv.name, nis_sperrno(mres->status));
			exit(1);
		}

		/*
		 * fork a child and *try* to do nis_rmdir.  this may take
		 * a while since we may try to talk to servers that aren't
		 * up/responding.
		 */
		if (force_remove)
			switch (fork()) {
			case 0:
				nis_rmdir(fname, &sserv);
			default:
				exit(0);
			}

		signal(SIGINT, (void(*)(int))cleanup_rmslave);

		/*
		 * remove the actual directory.
		 */
		s = nis_rmdir(fname, &sserv);
		if (s != NIS_SUCCESS) {
			fprintf(stderr, "cannot remove replica %s: %s.\n",
				sserv.name, nis_sperrno(s));
			cleanup_rmslave();
			exit(1);
		}
		exit(0);
	}
}
