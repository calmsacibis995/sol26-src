/*
 *	autod_nfs.c
 *
 *	Copyright (c) 1988-1996 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)autod_nfs.c	1.58	96/07/29 SMI"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <syslog.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/signal.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <sys/mount.h>
#include <sys/mntent.h>
#include <sys/mnttab.h>
#include <sys/fstyp.h>
#include <sys/fsid.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netconfig.h>
#include <netdir.h>
#include <errno.h>
#define	NFSCLIENT
#include <nfs/nfs.h>
#include <nfs/mount.h>
#include <rpcsvc/mount.h>
#include <rpc/nettype.h>
#include <locale.h>
#include <setjmp.h>
#include <sys/socket.h>
#include <thread.h>
#include <limits.h>
#include <nss_dbdefs.h>			/* for NSS_BUFLEN_HOSTS */
#include <nfs/nfs_sec.h>
#include "automount.h"
#include "replica.h"
#include "nfs_subr.h"

#define	MAXHOSTS 	512
#define	MAXSUBNETS 	MAXIFS
#define	MAXNETS		MAXIFS

/* number of transports to try */
#define	MNT_PREF_LISTLEN	2
#define	FIRST_TRY		1
#define	SECOND_TRY		2

#define	MNTTYPE_CACHEFS "cachefs"

/*
 * The following definitions must be kept in sync
 * with those in lib/libnsl/rpc/clnt_dg.c
 */
#define	RPC_MAX_BACKOFF	30
#define	CLCR_GET_RPCB_TIMEOUT	1
#define	CLCR_SET_RPCB_TIMEOUT	2
#define	CLCR_SET_RPCB_RMTTIME	5
#define	CLCR_GET_RPCB_RMTTIME	6

/*
 * host cache states
 */
#define	NOHOST		0
#define	GOODHOST	1
#define	DEADHOST	2

#define	NFS_ARGS_EXT2_secdata(args, secdata) \
	{ (args).nfs_args_ext = NFS_ARGS_EXT2, \
	(args).nfs_ext_u.nfs_ext2.secdata = secdata; }

struct cache_entry {
	struct  cache_entry *cache_next;
	char    *cache_host;
	time_t  cache_time;
	int	cache_state;
	u_long	cache_reqvers;
	u_long	cache_outvers;
};

struct cache_entry *cache_head = NULL;
rwlock_t cache_lock;    /* protect the cache chain */

static enum nfsstat nfsmount(struct mapfs *, char *, char *, int, int);
static int is_nfs_port(char *);

void netbuf_free(struct netbuf *);
static struct knetconfig *get_knconf(struct netconfig *);
static void free_knconf(struct knetconfig *);
static struct pathcnf *get_pathconf(CLIENT *, char *, char *);
static struct mapfs *enum_servers(struct mapent *, char *);
static struct mapfs *get_mysubnet_servers(struct mapfs *);
static struct mapfs *get_mynet_servers(struct mapfs *);
static int subnet_matches(u_int *, struct hostent *, int);
static int net_matches(int *, struct hostent *, int);
static int get_myhosts_subnets(u_int *);
static int get_myhosts_nets(int *);
static int getsubnet_byaddr(struct in_addr *, u_int *);
static int nopt(struct mnttab *, char *);
static  struct  netbuf *get_addr(char *, int, int, struct netconfig **,
					char *, int, struct t_info *);
static  struct  netbuf *get_the_addr(char *, int, int, struct netconfig *,
					int, struct t_info *);
struct mapfs *add_mfs(struct mapfs *, int, struct mapfs **, struct mapfs **);
void free_mfs(struct mapfs *);
void dump_mfs(struct mapfs *, char *, int);
static char *dump_distance(struct mapfs *);
static void cache_free(struct cache_entry *);
static int cache_check(char *, u_long *);
static void cache_enter(char *, u_long, u_long, int);
static void destroy_auth_client_handle(CLIENT *cl);

#ifdef CACHE_DEBUG
static void trace_host_cache();
static void trace_portmap_cache();
#endif /* CACHE_DEBUG */

int rpc_timeout = 20;

#ifdef CACHE_DEBUG
/*
 * host cache counters. These variables do not need to be protected
 * by mutex's. They have been added to measure the utility of the
 * goodhost/deadhost cache in the lazy hierarchical mounting scheme.
 */
static int host_cache_accesses = 0;
static int host_cache_lookups = 0;
static int deadhost_cache_hits = 0;
static int goodhost_cache_hits = 0;

/*
 * portmap cache counters. These variables do not need to be protected
 * by mutex's. They have been added to measure the utility of the portmap
 * cache in the lazy hierarchical mounting scheme.
 */
static int portmap_cache_accesses = 0;
static int portmap_cache_lookups = 0;
static int portmap_cache_hits = 0;
#endif /* CACHE_DEBUG */


int
mount_nfs(me, mntpnt, prevhost, overlay)
	struct mapent *me;
	char *mntpnt;
	char *prevhost;
	int overlay;
{
	struct mapfs *mfs;
	int err;
	int cached;

	mfs = enum_servers(me, prevhost);
	if (mfs == NULL)
		return (ENOENT);

	if (self_check(mfs->mfs_host) &&
	    is_nfs_port(me->map_mntopts)) {
		err = loopbackmount(mfs->mfs_dir,
			mntpnt, me->map_mntopts, overlay);
	} else {
		cached = strcmp(me->map_mounter, MNTTYPE_CACHEFS) == 0;
		err = nfsmount(mfs, mntpnt, me->map_mntopts,
				cached, overlay);
		if (err && trace > 1) {
			trace_prt(1, "  Couldn't mount %s:%s, err=%d\n",
				mfs->mfs_host, mfs->mfs_dir, err);
		}
	}
	free_mfs(mfs);
	return (err);
}

static int
getsubnet_byaddr(ptr, subnet)
struct in_addr *ptr;
u_int *subnet;
{
	u_long saddr, netmask;
	struct in_addr net, mask;

	saddr = ntohl(ptr->s_addr);
	if (IN_CLASSA(saddr))
		net.s_addr = htonl(saddr & IN_CLASSA_NET);
	else 	if (IN_CLASSB(saddr))
		net.s_addr = htonl(saddr & IN_CLASSB_NET);
	else 	if (IN_CLASSC(saddr))
		net.s_addr = htonl(saddr & IN_CLASSC_NET);
	else
		return (-1);

	memset((char *)&mask, 0, sizeof (mask));
	if (getnetmaskbynet(net, &mask) != 0)
		return (-1);

	netmask = ntohl(mask.s_addr);
	if (IN_CLASSA(saddr))
		*subnet = IN_CLASSA_HOST & netmask & saddr;
	else if (IN_CLASSB(saddr)) {
		*subnet = IN_CLASSB_HOST & netmask & saddr;
	} else if (IN_CLASSC(saddr))
		*subnet = IN_CLASSC_HOST & netmask & saddr;

	return (0);
}

/*
 * This function is called to get the subnets to which the
 * host is connected to.
 */
static int
get_myhosts_subnets(u_int *my_subnets)
{
	struct myaddrs *addr = myaddrs_head;
	int my_subnet_cnt = 0;

	while (addr) {
		if (my_subnet_cnt < MAXSUBNETS) {
			if (getsubnet_byaddr(&(addr->sin.sin_addr),
				&my_subnets[my_subnet_cnt]) == 0)
				my_subnet_cnt++;
		}
		addr = addr->myaddrs_next;
	}
	return (my_subnet_cnt);
}

/*
 * This function is called to get the nets to which the
 * host is connected to.
 */
static int
get_myhosts_nets(int *my_nets)
{
	struct myaddrs *addr = myaddrs_head;
	int my_net_cnt = 0;

	while (addr) {
		if (my_net_cnt < MAXNETS)
			my_nets[my_net_cnt++] = inet_netof(addr->sin.sin_addr);
		addr = addr->myaddrs_next;
	}
	return (my_net_cnt);
}

/*
 * Given a host-entry, check if it matches any of the subnets
 * to which the localhost is connected to
 */
static int
subnet_matches(u_int *mysubnets, struct hostent *hs, int my_subnet_cnt)
{
	struct in_addr *ptr;
	u_int subnet;
	int i;

	/* LINTED pointer alignment */
	ptr = (struct in_addr *) *hs->h_addr_list;
	if (getsubnet_byaddr(ptr, &subnet) == 0) {
		for (i = 0; i < my_subnet_cnt; i++) {
			if (mysubnets[i] == subnet)
				return (1);
		}
	}
	return (0);
}

/*
 * Given a host-entry, check if it matches any of the nets
 * to which the localhost is connected to
 */
static int
net_matches(int *mynets, struct hostent *hs, int my_net_cnt)
{
	int i, net;

	/* LINTED pointer alignment */
	net = inet_netof(*((struct in_addr *) *hs->h_addr_list));
	for (i = 0; i < my_net_cnt; i++) {
		if (mynets[i] == net)
			return (1);
	}
	return (0);
}

/*
 * Given a list of servers find all the servers who are
 * on the same subnet(s) as the local host. We use
 * a static counter and locking to get local subnet
 * information only once. However, we do the calls over
 * here (vs in the single threaded code) if we want to
 * adapt the code to check subnet information periodically.
 */
static struct mapfs *
get_mysubnet_servers(struct mapfs *mfs_in)
{
	static mutex_t mysubnet_hosts = DEFAULTMUTEX;
	static int my_subnet_cnt = 0;
	static u_int my_subnets[MAXSUBNETS + 1];
	static bool_t gotmysubnets = FALSE;
	struct mapfs *p, *mfs, *mfs_head = NULL, *mfs_tail = NULL;
	struct hostent *hs;
	int cnt = 0;
	char host_buf[NSS_BUFLEN_HOSTS];
	struct hostent he;

	if (!gotmysubnets) {
		mutex_lock(&mysubnet_hosts);
		if (!gotmysubnets)
			my_subnet_cnt = get_myhosts_subnets(my_subnets);
		gotmysubnets = TRUE;
		mutex_unlock(&mysubnet_hosts);
	}

	if (!my_subnet_cnt)
		return (NULL);

	for (mfs = mfs_in; mfs && (cnt < MAXHOSTS); mfs = mfs->mfs_next) {
		if (mfs->mfs_ignore)
			continue;
		hs = (struct hostent *) gethostbyname_r(mfs->mfs_host, &he,
					host_buf, NSS_BUFLEN_HOSTS, &h_errno);
		if (hs == NULL)
			continue;
		if (subnet_matches(my_subnets, hs, my_subnet_cnt)) {
			p = add_mfs(mfs, DIST_MYSUB, &mfs_head, &mfs_tail);
			if (!p)
				return (NULL);
			cnt++;
		}
	}
	return (mfs_head);
}


/*
 * Given a list of servers find all the servers who are
 * on the same network(s) as the local host. We use
 * a static counter and locking to get local net
 * information only once. However, we do the calls over
 * here (vs in the single threaded code) if we want to
 * adapt the code to check net information periodically.
 */
static struct mapfs *
get_mynet_servers(struct mapfs *mfs_in)
{
	static mutex_t mynet_hosts = DEFAULTMUTEX;
	static int my_net_cnt = 0;
	static int my_nets[MAXNETS + 1];
	static bool_t gotmynets = FALSE;
	struct mapfs *p, *mfs, *mfs_head = NULL, *mfs_tail = NULL;
	struct hostent *hs;
	int cnt = 0;
	char host_buf[NSS_BUFLEN_HOSTS];
	struct hostent he;

	if (!gotmynets) {
		mutex_lock(&mynet_hosts);
		if (!gotmynets)
			my_net_cnt = get_myhosts_nets(my_nets);
		gotmynets = TRUE;
		mutex_unlock(&mynet_hosts);
	}

	if (!my_net_cnt)
		return (NULL);

	for (mfs = mfs_in; mfs && (cnt < MAXHOSTS); mfs = mfs->mfs_next) {
		if (mfs->mfs_ignore)
			continue;
		hs = (struct hostent *) gethostbyname_r(mfs->mfs_host, &he,
					host_buf, NSS_BUFLEN_HOSTS, &h_errno);
		if (hs == NULL)
			continue;
		if (net_matches(my_nets, hs, my_net_cnt)) {
			p = add_mfs(mfs, DIST_MYNET, &mfs_head, &mfs_tail);
			if (!p)
				return (NULL);
			cnt++;
		}
	}

	return (mfs_head);
}

/*
 * ping a bunch of hosts at once and sort by who responds first
 */
static struct mapfs *
sort_servers(struct mapfs *mfs_in, int timeout)
{
	struct mapfs *m1 = NULL;
	enum clnt_stat clnt_stat;

	if (!mfs_in)
		return (NULL);

	clnt_stat = nfs_cast(mfs_in, &m1, timeout);

	if (!m1) {
		char buff[2048] = {'\0'};

		for (m1 = mfs_in; m1; m1 = m1->mfs_next) {
			(void) strcat(buff, m1->mfs_host);
			if (m1->mfs_next)
				(void) strcat(buff, ",");
		}

		syslog(LOG_ERR, "servers %s not responding: %s",
			buff, clnt_sperrno(clnt_stat));
	}

	return (m1);
}

/*
 * Add a mapfs entry to the list described by *mfs_head and *mfs_tail,
 * provided it is not marked "ignored" and isn't a dupe of ones we've
 * already seen.
 */
struct mapfs *
add_mfs(struct mapfs *mfs, int distance, struct mapfs **mfs_head,
	struct mapfs **mfs_tail)
{
	struct mapfs *tmp, *new;
	void bcopy();

	for (tmp = *mfs_head; tmp; tmp = tmp->mfs_next)
		if (strcmp(tmp->mfs_host, mfs->mfs_host) == 0 ||
			mfs->mfs_ignore)
			return (*mfs_head);
	new = (struct mapfs *) malloc(sizeof (struct mapfs));
	if (!new) {
		syslog(LOG_ERR, "Memory allocation failed: %m");
		return (NULL);
	}
	bcopy(mfs, new, sizeof (struct mapfs));
	new->mfs_next = NULL;
	if (distance)
		new->mfs_distance = distance;
	if (!*mfs_head)
		*mfs_tail = *mfs_head = new;
	else {
		(*mfs_tail)->mfs_next = new;
		*mfs_tail = new;
	}
	return (*mfs_head);
}

void
dump_mfs(struct mapfs *mfs, char *message, int level)
{
	struct mapfs *m1;

	if (trace <= level)
		return;

	trace_prt(1, "%s", message);
	if (!mfs) {
		trace_prt(0, "mfs is null\n");
		return;
	}
	for (m1 = mfs; m1; m1 = m1->mfs_next)
		trace_prt(0, "%s[%s] ", m1->mfs_host, dump_distance(m1));
	trace_prt(0, "\n");
}

static char *
dump_distance(struct mapfs *mfs)
{
	switch (mfs->mfs_distance) {
	case 0:			return ("zero");
	case DIST_SELF:		return ("self");
	case DIST_MYSUB:	return ("mysub");
	case DIST_MYNET:	return ("mynet");
	case DIST_OTHER:	return ("other");
	default:		return ("other");
	}
}

/*
 * Walk linked list "raw", building a new list consisting of members
 * NOT found in list "filter", returning the result.
 */
struct mapfs *
filter_mfs(struct mapfs *raw, struct mapfs *filter)
{
	struct mapfs *mfs, *p, *mfs_head = NULL, *mfs_tail = NULL;
	int skip;

	if (!raw)
		return (NULL);
	for (mfs = raw; mfs; mfs = mfs->mfs_next) {
		for (skip = 0, p = filter; p; p = p->mfs_next) {
			if (strcmp(p->mfs_host, mfs->mfs_host) == 0) {
				skip = 1;
				break;
			}
		}
		if (skip)
			continue;
		p = add_mfs(mfs, 0, &mfs_head, &mfs_tail);
		if (!p)
			return (NULL);
	}
	return (mfs_head);
}

/*
 * Walk a linked list of mapfs structs, freeing each member.
 */
void
free_mfs(struct mapfs *mfs)
{
	struct mapfs *tmp;

	while (mfs) {
		tmp = mfs->mfs_next;
		free(mfs);
		mfs = tmp;
	}
}

/*
 * New code for NFS client failover: we need to carry and sort
 * lists of server possibilities rather than return a single
 * entry.  It preserves previous behaviour of sorting first by
 * locality (loopback-or-preferred/subnet/net/other) and then
 * by ping times.  We'll short-circuit this process when we
 * have ENOUGH or more entries.
 */
static struct mapfs *
enum_servers(struct mapent *me, char *preferred)
{
	struct mapfs *p, *m1, *m2, *mfs_head = NULL, *mfs_tail = NULL;

	/*
	 * Short-circuit for simple cases.
	 */
	if (!me->map_fs->mfs_next) {
		p = add_mfs(me->map_fs, DIST_OTHER, &mfs_head, &mfs_tail);
		if (!p)
			return (NULL);
		return (mfs_head);
	}

	dump_mfs(me->map_fs, "  enum_servers: mapent: ", 2);

	/*
	 * get addresses & see if any are myself
	 * or were mounted from previously in a
	 * hierarchical mount.
	 */
	if (trace > 2)
		trace_prt(1, "  enum_servers: looking for pref/self\n");
	for (m1 = me->map_fs; m1; m1 = m1->mfs_next) {
		if (m1->mfs_ignore)
			continue;
		if (self_check(m1->mfs_host) ||
		    strcmp(m1->mfs_host, preferred) == 0) {
			p = add_mfs(m1, DIST_SELF, &mfs_head, &mfs_tail);
			if (!p)
				return (NULL);
		}
	}
	if (trace > 2 && m1)
		trace_prt(1, "  enum_servers: pref/self found, %s\n",
			m1->mfs_host);

	/*
	 * look for entries on this subnet
	 */
	m1 = get_mysubnet_servers(me->map_fs);
	dump_mfs(m1, "  enum_servers: output of get_mysubnet_servers: ", 3);
	if (m1 && m1->mfs_next) {
		m2 = sort_servers(m1, rpc_timeout / 2);
		dump_mfs(m2, "  enum_servers: output of sort_servers: ", 3);
		free_mfs(m1);
		m1 = m2;
	}

	for (m2 = m1; m2; m2 = m2->mfs_next) {
		p = add_mfs(m2, 0, &mfs_head, &mfs_tail);
		if (!p)
			return (NULL);
	}
	if (m1)
		free_mfs(m1);

	/*
	 * look for entries on this network, ignoring those we have
	 */
	m1 = filter_mfs(me->map_fs, mfs_head);
	dump_mfs(m1, "  enum_servers: net: output of filter_mfs: ", 3);
	m2 = get_mynet_servers(m1);
	dump_mfs(m2, "  enum_servers: net: output of get_mynet_servers: ", 3);
	if (m1)
		free_mfs(m1);
	m1 = m2;
	if (m1 && m1->mfs_next) {
		m2 = sort_servers(m1, rpc_timeout / 2);
		dump_mfs(m2, "  enum_servers: net: output of sort_servers:", 3);
		free_mfs(m1);
		m1 = m2;
	}
	for (m2 = m1; m2; m2 = m2->mfs_next) {
		p = add_mfs(m2, 0, &mfs_head, &mfs_tail);
		if (!p)
			return (NULL);
	}
	if (m1)
		free_mfs(m1);

	/*
	 * add the rest of the entries at the end
	 */
	m1 = filter_mfs(me->map_fs, mfs_head);
	dump_mfs(m1, "  enum_servers: etc: output of filter_mfs: ", 3);
	m2 = sort_servers(m1, rpc_timeout / 2);
	dump_mfs(m2, "  enum_servers: etc: output of sort_servers: ", 3);
	if (m1)
		free_mfs(m1);
	m1 = m2;
	for (m2 = m1; m2; m2 = m2->mfs_next) {
		p = add_mfs(m2, DIST_OTHER, &mfs_head, &mfs_tail);
		if (!p)
			return (NULL);
	}
	if (m1)
		free_mfs(m1);

done:
	dump_mfs(mfs_head, "  enum_servers: output: ", 1);
	return (mfs_head);
}

static enum nfsstat
nfsmount(mfs_in, mntpnt, opts, cached, overlay)
	struct mapfs *mfs_in;
	char *mntpnt, *opts;
	int cached, overlay;
{
	CLIENT *cl;
	char remname[MAXPATHLEN], *mnttabtext = NULL;
	int mnttabcnt = 0;
	int loglevel;
	struct mnttab m;
	struct nfs_args *argp = NULL, *head = NULL, *tail = NULL,
		*prevhead, *prevtail;
	int flags;
	struct fhstatus fhs;
	struct timeval timeout;
	enum clnt_stat rpc_stat;
	enum nfsstat status;
	struct stat stbuf;
	struct netconfig *nconf;
	u_long vers, versmin;
	u_long outvers;
	u_long nfsvers;
	int posix;
	struct nd_addrlist *retaddrs;
	struct mountres3 res3;
	nfs_fh3 fh3;
	char *fstype;
	int count, i;
	int *auths;
	int delay = 5;
	int retries;
	char *nfs_proto = NULL;
	u_int nfs_port = 0;
	char *p, *host, *dir;
	struct mapfs *mfs = NULL;
	int last_error = 0;
	int replicated;
	int entries = 0;
	int v2cnt = 0, v3cnt = 0;
	int v2near = 0, v3near = 0;
	int skipentry = 0;
	char *nfs_flavor;
	seconfig_t nfs_sec;
	int sec_opt, scerror;
	struct sec_data *secdata;
	int secflags;
	struct netbuf *syncaddr;

	dump_mfs(mfs_in, "  nfsmount: input: ", 2);
	replicated = (mfs_in->mfs_next != NULL);
	m.mnt_mntopts = opts;
	if (replicated && hasmntopt(&m, MNTOPT_SOFT)) {
		syslog(LOG_WARNING,
		    "mount on %s is soft, and will not be replicated.", mntpnt);
		replicated = 0;
	}
	if (replicated && !hasmntopt(&m, MNTOPT_RO)) {
		syslog(LOG_WARNING,
		    "mount on %s is not read-only, and will not be replicated.",
		    mntpnt);
		replicated = 0;
	}
	if (replicated && cached) {
		syslog(LOG_WARNING,
		    "mount on %s is cached, and will not be replicated.",
		    mntpnt);
		replicated = 0;
	}
	if (replicated)
		loglevel = LOG_WARNING;
	else
		loglevel = LOG_ERR;

	if (trace > 1) {
		if (replicated) {
			trace_prt(1, "  nfsmount: replicated mount on %s %s:\n",
				mntpnt, opts);
			for (mfs = mfs_in; mfs; mfs = mfs->mfs_next)
				trace_prt(1, "    %s:%s\n",
					mfs->mfs_host, mfs->mfs_dir);
		}
		else
			trace_prt(1, "  nfsmount: %s:%s %s %s\n",
			    mfs_in->mfs_host, mfs_in->mfs_dir, mntpnt, opts);
	}

	/*
	 * Make sure mountpoint is safe to mount on
	 */
	if (lstat(mntpnt, &stbuf) < 0) {
		syslog(LOG_ERR, "Couldn't stat %s: %m", mntpnt);
		return (NFSERR_NOENT);
	}

	/*
	 * Get protocol specified in options list, if any.
	 */
	if ((str_opt(&m, "proto", &nfs_proto)) == -1) {
		return (NFSERR_NOENT);
	}

	/*
	 * Get port specified in options list, if any.
	 */
	nfs_port = nopt(&m, MNTOPT_PORT);
	if (nfs_port > USHRT_MAX) {
		syslog(LOG_ERR, "%s: invalid port number %d", mntpnt, nfs_port);
		return (NFSERR_NOENT);
	}

	/*
	 * Set mount(2) flags here, outside of the loop.
	 */
	flags = 0;
	flags |= (hasmntopt(&m, MNTOPT_RO) == NULL) ? 0 : MS_RDONLY;
	flags |= (hasmntopt(&m, MNTOPT_NOSUID) == NULL) ? 0 : MS_NOSUID;
	flags |= overlay ? MS_OVERLAY : 0;
	if (mntpnt[strlen(mntpnt) - 1] != ' ')
		/* direct mount point without offsets */
		flags |= MS_OVERLAY;

nextentry:
	skipentry = 0;

	/*
	 * Attempt figure out which version of NFS to use for this mount.
	 * If the version number was specified, then use it.
	 * Otherwise, default to NFS Version 3 with a fallback
	 * to NFS Version 2.
	 */
	nfsvers = nopt(&m, MNTOPT_VERS);
	switch (nfsvers) {
	case 0:
		vers = NFS_V3;
		versmin = NFS_VERSMIN;		/* version 2 */
		break;
	case NFS_V3:
		vers = NFS_V3;
		versmin = NFS_V3;
		break;
	case NFS_VERSION:
		vers = NFS_VERSION;		/* version 2 */
		versmin = NFS_VERSMIN;		/* version 2 */
		break;
	default:
		syslog(LOG_ERR, "Incorrect NFS version specified for %s",
			mntpnt);
		return (NFSERR_NOENT);
	}

	/*
	 * Walk the whole list, pinging and collecting version
	 * info so that we can make sure the mount will be
	 * homogeneous with respect to version.
	 *
	 * If we have a version preference, this is easy; we'll
	 * just reject anything that doesn't match.
	 *
	 * If not, we want to try to provide the best compromise
	 * that considers proximity, preference for a higher version,
	 * sorted order, and number of replicas.  We will count
	 * the number of V2 and V3 replicas and also the number
	 * which are "near", i.e. the localhost or on the same
	 * subnet.
	 */
	for (mfs = mfs_in; mfs; mfs = mfs->mfs_next) {

		if (mfs->mfs_ignore)
			continue;

		host = mfs->mfs_host;
		i = pingnfs(host, get_retry(opts) + 1, &vers, versmin);
		if (i != RPC_SUCCESS) {
			syslog(loglevel, "server %s not responding", host);
			mfs->mfs_ignore = 1;
			last_error = NFSERR_NOENT;
			continue;
		}
		if (nfsvers != 0 && nfsvers != vers) {
			syslog(loglevel,
				"NFS version %d not supported by %s",
				nfsvers, host);
			mfs->mfs_ignore = 1;
			last_error = NFSERR_NOENT;
			continue;
		} else {
			if (vers == NFS_V3)
				v3cnt++;
			else
				v2cnt++;
			if (mfs->mfs_distance &&
			    mfs->mfs_distance <= DIST_MYSUB) {
				if (vers == NFS_V3)
					v3near++;
				else
					v2near++;
			}
		}
		/*
		 * If the mount is not replicated, we don't want to
		 * ping every entry, so we'll stop here.  This means
		 * that we may have to go back to "nextentry" above
		 * to consider another entry if there we can't get
		 * all the way to mount(2) with this one.
		 */
		if (!replicated)
			break;
	}

	if (nfsvers == 0) {
		/*
		 * Choose the NFS version.
		 * V3-capable servers are better, if we only have
		 * V2 nearby, we'd rather use them to avoid going
		 * through a router.  If we downgrade to NFS V2,
		 * we can use the V3 servers that also support V2.
		 */
		if (v3cnt >= v2cnt && (v3near || !v2near))
			nfsvers = NFS_V3;
		else
			nfsvers = NFS_VERSION;
		if (trace > 2)
			trace_prt(1,
			    "  nfsmount: v3=%d[%d],v2=%d[%d] => v%d.\n",
			    v3cnt, v3near, v2cnt, v2near, nfsvers);
	}

	/*
	 * Now choose the proper mount protocol version
	 */
	if (nfsvers == NFS_V3) {
		vers = MOUNTVERS3;
		versmin = MOUNTVERS3;
	} else {
		vers = MOUNTVERS_POSIX;
		versmin = MOUNTVERS;
	}

	/*
	 * Our goal here is to evaluate each of several possible
	 * replicas and try to come up with a list we can hand
	 * to mount(2).  If we don't have a valid "head" at the
	 * end of this process, it means we have rejected all
	 * potential server:/path tuples.  We will fail quietly
	 * in front of mount(2), and will have printed errors
	 * where we found them.
	 * XXX - do option work outside loop w careful design
	 * XXX - use macro for error condition free handling
	 */
	for (mfs = mfs_in; mfs; mfs = mfs->mfs_next) {

		if (mfs->mfs_ignore)
			continue;

		/*
		 * If this is not a replicated mount, we haven't done
		 * a pingnfs() on the next entry, so we don't know if
		 * the next entry is up or if it supports an NFS version
		 * we like.  So if we had a problem with an entry, we
		 * need to go back and run through some new code.
		 */
		if (!replicated && skipentry)
			goto nextentry;

		host = mfs->mfs_host;
		dir = mfs->mfs_dir;
		(void) sprintf(remname, "%s:%s", host, dir);
		if (trace > 4 && replicated)
			trace_prt(1, "  nfsmount: examining %s\n", remname);

		/*
		 * If it's cached we need to get cachefs to mount it.
		 */
		if (cached) {
			last_error = mount_generic(remname, MNTTYPE_CACHEFS,
				opts, mntpnt, overlay);
			if (last_error) {
				skipentry = 1;
				mfs->mfs_ignore = 1;
				continue;
			}
			return (0);
		}

		/*
		 * Allocate nfs_args structure
		 */
		argp = (struct nfs_args *) malloc(sizeof (struct nfs_args));
		if (!argp) {
			syslog(LOG_ERR, "nfsmount: no memory");
			last_error = NFSERR_IO;
			goto out;
		}
		(void) memset(argp, 0, sizeof (*argp));
		prevhead = head;
		prevtail = tail;
		if (!head)
			head = tail = argp;
		else
			tail = tail->nfs_ext_u.nfs_ext2.next = argp;

		timeout.tv_usec = 0;
		timeout.tv_sec = rpc_timeout;
		rpc_stat = RPC_TIMEDOUT;
		retries = get_retry(opts);
retry:
		cl = clnt_create_vers(host, MOUNTPROG, &outvers, versmin,
					vers, "udp");
		if (cl == NULL) {
			free(argp);
			head = prevhead;
			tail = prevtail;
			if (tail)
				tail->nfs_ext_u.nfs_ext2.next = NULL;
			last_error = NFSERR_NOENT;
			syslog(loglevel, "%s %s", host,
				clnt_spcreateerror("server not responding"));
			skipentry = 1;
			mfs->mfs_ignore = 1;
			continue;
		}

		if (__clnt_bindresvport(cl) < 0) {
			free(argp);
			head = prevhead;
			tail = prevtail;
			if (tail)
				tail->nfs_ext_u.nfs_ext2.next = NULL;
			last_error = NFSERR_NOENT;
			syslog(loglevel, "mount %s: %s", host,
				"Couldn't bind to reserved port");
			destroy_auth_client_handle(cl);
			skipentry = 1;
			mfs->mfs_ignore = 1;
			continue;
		}
		cl->cl_auth = authsys_create_default();

		/*
		 * set security options
		 */
		sec_opt = 0;
		(void) memset(&nfs_sec, 0, sizeof (nfs_sec));
		if (hasmntopt(&m, MNTOPT_KERB) != NULL) {
			sec_opt++;
			if (nfs_getseconfig_byname("krb4", &nfs_sec)) {
				syslog(loglevel,
				    "error getting krb4 information from %s",
				    NFSSEC_CONF);
				free(argp);
				head = prevhead;
				tail = prevtail;
				if (tail)
					tail->nfs_ext_u.nfs_ext2.next = NULL;
				last_error = NFSERR_IO;
				destroy_auth_client_handle(cl);
				skipentry = 1;
				mfs->mfs_ignore = 1;
				continue;
			}
		}
		if (hasmntopt(&m, MNTOPT_SECURE) != NULL) {
			if (++sec_opt > 1) {
				syslog(loglevel,
				    "conflicting security options for %s",
				    remname);
				free(argp);
				head = prevhead;
				tail = prevtail;
				if (tail)
					tail->nfs_ext_u.nfs_ext2.next = NULL;
				last_error = NFSERR_IO;
				destroy_auth_client_handle(cl);
				skipentry = 1;
				mfs->mfs_ignore = 1;
				continue;
			}
			if (nfs_getseconfig_byname("dh", &nfs_sec)) {
				syslog(loglevel,
				    "error getting dh information from %s",
				    NFSSEC_CONF);
				free(argp);
				head = prevhead;
				tail = prevtail;
				if (tail)
					tail->nfs_ext_u.nfs_ext2.next = NULL;
				last_error = NFSERR_IO;
				destroy_auth_client_handle(cl);
				skipentry = 1;
				mfs->mfs_ignore = 1;
				continue;
			}
		}

		nfs_flavor = NULL;
		/*
		 * Have to workaround the fact that hasmntopt() returns true
		 * when comparing "secure" (in &m) with "sec".
		 */
		if (hasmntopt(&m, "sec=") != NULL) {
			if ((str_opt(&m, MNTOPT_SEC, &nfs_flavor)) == -1) {
				syslog(LOG_ERR, "nfsmount: no memory");
				last_error = NFSERR_IO;
				destroy_auth_client_handle(cl);
				goto out;
			}
		}

		if (nfs_flavor) {
			if (++sec_opt > 1) {
				syslog(loglevel,
				    "conflicting security options for %s",
				    remname);
				free(nfs_flavor);
				free(argp);
				head = prevhead;
				tail = prevtail;
				if (tail)
					tail->nfs_ext_u.nfs_ext2.next = NULL;
				last_error = NFSERR_IO;
				destroy_auth_client_handle(cl);
				skipentry = 1;
				mfs->mfs_ignore = 1;
				continue;
			}
			if (nfs_getseconfig_byname(nfs_flavor, &nfs_sec)) {
				syslog(loglevel,
				    "error getting %s information from %s",
				    nfs_flavor, NFSSEC_CONF);
				free(nfs_flavor);
				free(argp);
				head = prevhead;
				tail = prevtail;
				if (tail)
					tail->nfs_ext_u.nfs_ext2.next = NULL;
				last_error = NFSERR_IO;
				destroy_auth_client_handle(cl);
				skipentry = 1;
				mfs->mfs_ignore = 1;
				continue;
			}
			free(nfs_flavor);
		}

		posix = (hasmntopt(&m, MNTOPT_POSIX) != NULL) ? 1 : 0;

		/*
		 * Get fhandle of remote path from server's mountd
		 */
		switch (outvers) {
		case MOUNTVERS:
			if (posix) {
				free(argp);
				head = prevhead;
				tail = prevtail;
				if (tail)
					tail->nfs_ext_u.nfs_ext2.next = NULL;
				last_error = NFSERR_NOENT;
				syslog(loglevel, "can't get posix info for %s",
					host);
				destroy_auth_client_handle(cl);
				skipentry = 1;
				mfs->mfs_ignore = 1;
				continue;
			}
		/* FALLTHRU */
		case MOUNTVERS_POSIX:
			if (nfsvers == NFS_V3) {
				free(argp);
				head = prevhead;
				tail = prevtail;
				if (tail)
					tail->nfs_ext_u.nfs_ext2.next = NULL;
				last_error = NFSERR_NOENT;
				syslog(loglevel,
					"%s doesn't support NFS Version 3",
					host);
				destroy_auth_client_handle(cl);
				skipentry = 1;
				mfs->mfs_ignore = 1;
				continue;
			}
			rpc_stat = clnt_call(cl, MOUNTPROC_MNT,
				xdr_dirpath, (caddr_t)&dir,
				xdr_fhstatus, (caddr_t)&fhs, timeout);
			if ((rpc_stat == RPC_TIMEDOUT) && (retries-- > 0)) {
				(void) sleep(delay);
				delay *= 2;
				if (delay > 20)
					delay = 20;
				destroy_auth_client_handle(cl);
				goto retry;
			}
			if (rpc_stat != RPC_SUCCESS) {
				/*
				 * Given the way "clnt_sperror" works, the "%s"
				 * immediately following the "not responding"
				 * is correct.
				 */
				free(argp);
				head = prevhead;
				tail = prevtail;
				if (tail)
					tail->nfs_ext_u.nfs_ext2.next = NULL;
				last_error = NFSERR_NOENT;
				syslog(loglevel, "%s server not responding%s",
				    host, clnt_sperror(cl, ""));
				destroy_auth_client_handle(cl);
				skipentry = 1;
				mfs->mfs_ignore = 1;
				continue;
			}
			if ((errno = fhs.fhs_status) != MNT_OK)  {
				free(argp);
				head = prevhead;
				tail = prevtail;
				if (tail)
					tail->nfs_ext_u.nfs_ext2.next = NULL;
				if (errno == EACCES) {
					status = NFSERR_ACCES;
				} else {
					syslog(loglevel, "%s: %m", host);
					status = NFSERR_IO;
				}
				last_error = status;
				destroy_auth_client_handle(cl);
				skipentry = 1;
				mfs->mfs_ignore = 1;
				continue;
			}
			argp->fh = malloc((sizeof (fhandle)));
			if (!argp->fh) {
				syslog(LOG_ERR, "nfsmount: no memory");
				last_error = NFSERR_IO;
				destroy_auth_client_handle(cl);
				goto out;
			}
			(void) memcpy(argp->fh, &fhs.fhstatus_u.fhs_fhandle,
				sizeof (fhandle));
			fstype = MNTTYPE_NFS;
			break;
		case MOUNTVERS3:
			posix = 0;
			(void) memset((char *)&res3, '\0', sizeof (res3));
			rpc_stat = clnt_call(cl, MOUNTPROC_MNT,
				xdr_dirpath, (caddr_t)&dir,
				xdr_mountres3, (caddr_t)&res3, timeout);
			if ((rpc_stat == RPC_TIMEDOUT) && (retries-- > 0)) {
				(void) sleep(delay);
				delay *= 2;
				if (delay > 20)
					delay = 20;
				destroy_auth_client_handle(cl);
				goto retry;
			}
			if (rpc_stat != RPC_SUCCESS) {
				/*
				 * Given the way "clnt_sperror" works, the "%s"
				 * immediately following the "not responding"
				 * is correct.
				 */
				free(argp);
				head = prevhead;
				tail = prevtail;
				if (tail)
					tail->nfs_ext_u.nfs_ext2.next = NULL;
				last_error = NFSERR_NOENT;
				syslog(loglevel, "%s server not responding%s",
				    remname, clnt_sperror(cl, ""));
				destroy_auth_client_handle(cl);
				skipentry = 1;
				mfs->mfs_ignore = 1;
				continue;
			}
			if ((errno = res3.fhs_status) != MNT_OK)  {
				free(argp);
				head = prevhead;
				tail = prevtail;
				if (tail)
					tail->nfs_ext_u.nfs_ext2.next = NULL;
				if (errno == EACCES) {
					status = NFSERR_ACCES;
				} else {
					syslog(loglevel, "%s: %m", remname);
					status = NFSERR_IO;
				}
				last_error = status;
				destroy_auth_client_handle(cl);
				skipentry = 1;
				mfs->mfs_ignore = 1;
				continue;
			}

			/*
			 *  Negotiate the security flavor for nfs_mount
			 */
			auths =
		    res3.mountres3_u.mountinfo.auth_flavors.auth_flavors_val;
			count =
		    res3.mountres3_u.mountinfo.auth_flavors.auth_flavors_len;

			if (sec_opt) {
				for (i = 0; i < count; i++)
					if (auths[i] == nfs_sec.sc_nfsnum) {
						break;
					}
				if (i >= count) {
					syslog(LOG_ERR,
				    "%s: does not support security \"%s\"\n",
					    remname, nfs_sec.sc_name);
					free(argp);
					head = prevhead;
					tail = prevtail;
					if (tail)
				tail->nfs_ext_u.nfs_ext2.next = NULL;
					last_error = NFSERR_IO;
					destroy_auth_client_handle(cl);
					skipentry = 1;
					mfs->mfs_ignore = 1;
					continue;
				}
			} else {
				if (count > 0) {
					for (i = 0; i < count; i++) {
					    if (!(scerror =
				nfs_getseconfig_bynumber(auths[i], &nfs_sec))) {
						sec_opt++;
						break;
					    }
					}
					if (i >= count) {
						nfs_syslog_scerr(scerror);
						free(argp);
						head = prevhead;
						tail = prevtail;
						if (tail)
					tail->nfs_ext_u.nfs_ext2.next = NULL;
						last_error = NFSERR_IO;
						destroy_auth_client_handle(cl);
						skipentry = 1;
						mfs->mfs_ignore = 1;
						continue;
					}
				}
			}

			fh3.fh3_length =
			    res3.mountres3_u.mountinfo.fhandle.fhandle3_len;
			(void) memcpy(fh3.fh3_u.data,
			    res3.mountres3_u.mountinfo.fhandle.fhandle3_val,
			    fh3.fh3_length);
			argp->fh = malloc(sizeof (nfs_fh3));
			if (!argp->fh) {
				syslog(LOG_ERR, "nfsmount: no memory");
				last_error = NFSERR_IO;
				destroy_auth_client_handle(cl);
				goto out;
			}
			(void) memcpy(argp->fh, &fh3, sizeof (nfs_fh3));
			fstype = MNTTYPE_NFS3;
			break;
		default:
			free(argp);
			head = prevhead;
			tail = prevtail;
			if (tail)
				tail->nfs_ext_u.nfs_ext2.next = NULL;
			last_error = NFSERR_NOENT;
			syslog(loglevel, "unknown MOUNT version %ld on %s",
			    vers, remname);
			destroy_auth_client_handle(cl);
			skipentry = 1;
			mfs->mfs_ignore = 1;
			continue;
		} /* switch */

		if (trace > 4)
			trace_prt(1, "  nfsmount: have %s filehandle for %s\n",
			    fstype, remname);

		argp->flags |= NFSMNT_NEWARGS;
		argp->flags |= NFSMNT_INT;	/* default is "intr" */
		argp->hostname = host;
		argp->flags |= NFSMNT_HOSTNAME;

		nconf = NULL;
		argp->addr = get_addr(host, NFS_PROGRAM, nfsvers,
				&nconf, nfs_proto, nfs_port, NULL);

		if (argp->addr == NULL) {
			free(argp->fh);
			free(argp);
			head = prevhead;
			tail = prevtail;
			if (tail)
				tail->nfs_ext_u.nfs_ext2.next = NULL;
			last_error = NFSERR_NOENT;
			syslog(loglevel, "%s: no NFS service", host);
			destroy_auth_client_handle(cl);
			skipentry = 1;
			mfs->mfs_ignore = 1;
			continue;
		}
		if (trace > 4)
			trace_prt(1, "  nfsmount: have net address for %s\n",
			    remname);

		argp->flags |= NFSMNT_KNCONF;
		argp->knconf = get_knconf(nconf);
		if (argp->knconf == NULL) {
			netbuf_free(argp->addr);
			freenetconfigent(nconf);
			free(argp->fh);
			free(argp);
			head = prevhead;
			tail = prevtail;
			if (tail)
				tail->nfs_ext_u.nfs_ext2.next = NULL;
			last_error = NFSERR_NOSPC;
			destroy_auth_client_handle(cl);
			skipentry = 1;
			mfs->mfs_ignore = 1;
			continue;
		}
		if (trace > 4)
			trace_prt(1, "  nfsmount: have net config for %s\n",
			    remname);

		if (hasmntopt(&m, MNTOPT_SOFT) != NULL) {
			argp->flags |= NFSMNT_SOFT;
		}
		if (hasmntopt(&m, MNTOPT_NOINTR) != NULL) {
			argp->flags &= ~(NFSMNT_INT);
		}
		if (hasmntopt(&m, MNTOPT_NOAC) != NULL) {
			argp->flags |= NFSMNT_NOAC;
		}
		if (hasmntopt(&m, MNTOPT_NOCTO) != NULL) {
			argp->flags |= NFSMNT_NOCTO;
		}

		/*
		 * Set up security data for argp->nfs_ext_u.nfs_ext1.secdata.
		 */
		if (!sec_opt) {
			/*
			 * Get default security mode.
			 */
			if (nfs_getseconfig_default(&nfs_sec)) {
				syslog(loglevel,
				    "error getting default security entry\n");
				free_knconf(argp->knconf);
				netbuf_free(argp->addr);
				freenetconfigent(nconf);
				free(argp->fh);
				free(argp);
				head = prevhead;
				tail = prevtail;
				if (tail)
					tail->nfs_ext_u.nfs_ext2.next = NULL;
				last_error = NFSERR_NOSPC;
				destroy_auth_client_handle(cl);
				skipentry = 1;
				mfs->mfs_ignore = 1;
				continue;
			}
		}

		/*
		 * For AUTH_DES and AUTH_KERB -
		 * get the network address for the time service on
		 * the server.  If an RPC based time service is
		 * not available then try the IP time service.
		 *
		 * Eventurally, we want to move this code to nfs_clnt_secdata()
		 * when autod_nfs.c and mount.c can share the same
		 * get_the_addr() routine.
		 */
		secflags = 0;
		syncaddr = NULL;
		retaddrs = NULL;
		if ((nfs_sec.sc_rpcnum == AUTH_DES) ||
			(nfs_sec.sc_rpcnum == AUTH_KERB)) {
		    syncaddr = get_the_addr(host, RPCBPROG, RPCBVERS, nconf,
						0, NULL);
		    if (syncaddr) {
			secflags |= AUTH_F_RPCTIMESYNC;
		    } else {
			struct nd_hostserv hs;

			hs.h_host = host;
			hs.h_serv = "rpcbind";
			if (netdir_getbyname(nconf, &hs, &retaddrs) != ND_OK) {
				syslog(loglevel,
				    "%s: secure: no time service\n", host);
				free_knconf(argp->knconf);
				netbuf_free(argp->addr);
				freenetconfigent(nconf);
				free(argp->fh);
				free(argp);
				head = prevhead;
				tail = prevtail;
				if (tail)
					tail->nfs_ext_u.nfs_ext2.next = NULL;
				last_error = NFSERR_IO;
				destroy_auth_client_handle(cl);
				skipentry = 1;
				mfs->mfs_ignore = 1;
				continue;
			}
			syncaddr = retaddrs->n_addrs;
			/* LINTED pointer alignment */
			((struct sockaddr_in *) syncaddr->buf)->sin_port
				= htons((u_short)IPPORT_TIMESERVER);
		    }
		} /* if AUTH_DES or AUTH_KERB */

		if (!(secdata = nfs_clnt_secdata(&nfs_sec, host, argp->knconf,
					syncaddr, secflags))) {
			syslog(LOG_ERR,
				"errors constructing security related data\n");
			if (secflags & AUTH_F_RPCTIMESYNC)
				netbuf_free(syncaddr);
			else if (retaddrs)
				netdir_free(retaddrs, ND_ADDRLIST);
			free_knconf(argp->knconf);
			netbuf_free(argp->addr);
			freenetconfigent(nconf);
			free(argp->fh);
			free(argp);
			head = prevhead;
			tail = prevtail;
			if (tail)
				tail->nfs_ext_u.nfs_ext2.next = NULL;
			last_error = NFSERR_IO;
			destroy_auth_client_handle(cl);
			skipentry = 1;
			mfs->mfs_ignore = 1;
			continue;
		}
		NFS_ARGS_EXT2_secdata(*argp, secdata);
		/* end of security stuff */

		if (trace > 4)
			trace_prt(1,
			    "  nfsmount: have secure info for %s\n", remname);

		if (hasmntopt(&m, MNTOPT_GRPID) != NULL) {
			argp->flags |= NFSMNT_GRPID;
		}
		if (argp->rsize = nopt(&m, MNTOPT_RSIZE)) {
			argp->flags |= NFSMNT_RSIZE;
		}
		if (argp->wsize = nopt(&m, MNTOPT_WSIZE)) {
			argp->flags |= NFSMNT_WSIZE;
		}
		if (argp->timeo = nopt(&m, MNTOPT_TIMEO)) {
			argp->flags |= NFSMNT_TIMEO;
		}
		if (argp->retrans = nopt(&m, MNTOPT_RETRANS)) {
			argp->flags |= NFSMNT_RETRANS;
		}
		if (argp->acregmax = nopt(&m, MNTOPT_ACTIMEO)) {
			argp->flags |= NFSMNT_ACREGMAX;
			argp->flags |= NFSMNT_ACDIRMAX;
			argp->flags |= NFSMNT_ACDIRMIN;
			argp->flags |= NFSMNT_ACREGMIN;
			argp->acdirmin = argp->acregmin = argp->acdirmax
				= argp->acregmax;
		} else {
			if (argp->acregmin = nopt(&m, MNTOPT_ACREGMIN)) {
				argp->flags |= NFSMNT_ACREGMIN;
			}
			if (argp->acregmax = nopt(&m, MNTOPT_ACREGMAX)) {
				argp->flags |= NFSMNT_ACREGMAX;
			}
			if (argp->acdirmin = nopt(&m, MNTOPT_ACDIRMIN)) {
				argp->flags |= NFSMNT_ACDIRMIN;
			}
			if (argp->acdirmax = nopt(&m, MNTOPT_ACDIRMAX)) {
				argp->flags |= NFSMNT_ACDIRMAX;
			}
		}

		if (posix) {
			argp->pathconf = get_pathconf(cl, dir, remname);
			if (argp->pathconf == (struct pathcnf *) 0) {
				if (secflags & AUTH_F_RPCTIMESYNC)
					netbuf_free(syncaddr);
				else if (retaddrs)
					netdir_free(retaddrs, ND_ADDRLIST);
				free_knconf(argp->knconf);
				netbuf_free(argp->addr);
				freenetconfigent(nconf);
				nfs_free_secdata(
					argp->nfs_ext_u.nfs_ext2.secdata);
				free(argp->fh);
				free(argp);
				head = prevhead;
				tail = prevtail;
				if (tail)
					tail->nfs_ext_u.nfs_ext2.next = NULL;
				last_error = NFSERR_IO;
				destroy_auth_client_handle(cl);
				skipentry = 1;
				mfs->mfs_ignore = 1;
				continue;
			}
			argp->flags |= NFSMNT_POSIX;
			if (trace > 4)
				trace_prt(1,
				    "  nfsmount: have pathconf for %s\n",
				    remname);
		}

		/*
		 * free loop-specific data structures
		 */
		destroy_auth_client_handle(cl);
		freenetconfigent(nconf);
		if (secflags & AUTH_F_RPCTIMESYNC)
			netbuf_free(syncaddr);
		else if (retaddrs)
			netdir_free(retaddrs, ND_ADDRLIST);

		/* decide whether to use remote host's lockd or local locking */
		if (hasmntopt(&m, MNTOPT_LLOCK))
			argp->flags |= NFSMNT_LLOCK;
		if (!(argp->flags & NFSMNT_LLOCK) && nfsvers == NFS_VERSION &&
			remote_lock(host, argp->fh)) {
			syslog(loglevel, "No network locking on %s : "
			"contact admin to install server change", host);
			argp->flags |= NFSMNT_LLOCK;
		}

		/*
		 * Build a string for /etc/mnttab.
		 * If possible, coalesce strings with same 'dir' info.
		 */
		if (mnttabcnt) {
			p = strrchr(mnttabtext, (int) ':');
			if (!p || strcmp(p+1, dir) != 0) {
				mnttabcnt += strlen(remname) + 2;
			} else {
				*p = '\0';
				mnttabcnt += strlen(host) + 2;
			}
			mnttabtext = (char *) realloc(mnttabtext, mnttabcnt);
			strcat(mnttabtext, ",");
		} else {
			mnttabcnt = strlen(remname) + 1;
			mnttabtext = (char *) malloc(mnttabcnt);
			mnttabtext[0] = '\0';
		}
		if (!mnttabtext) {
			syslog(LOG_ERR, "nfsmount: no memory");
			last_error = NFSERR_IO;
			goto out;
		}
		strcat(mnttabtext, remname);

		/*
		 * At least one entry, can call mount(2).
		 */
		entries++;

		/*
		 * If replication was defeated, don't do more work
		 */
		if (!replicated)
			break;
	}


	/*
	 * Did we get through all possibilities without success?
	 */
	if (!entries)
		goto out;

	/*
	 * Whew; do the mount, at last.
	 */
	if (trace > 1) {
		trace_prt(1, "  mount %s %s (%s)\n", mnttabtext, mntpnt, opts);
	}

	if (mount("", mntpnt, flags | MS_DATA, fstype,
			head, sizeof (*head)) < 0) {
		if (trace > 1)
			trace_prt(1, "  Mount of %s on %s: %d",
			    mnttabtext, mntpnt, errno);
		if (errno != EBUSY || verbose)
			syslog(LOG_ERR,
				"Mount of %s on %s: %m", mnttabtext, mntpnt);
		last_error = NFSERR_IO;
		goto out;
	}

	last_error = NFS_OK;
	if (stat(mntpnt, &stbuf) == 0) {
		if (trace > 1) {
			trace_prt(1, "  mount %s dev=%x rdev=%x OK\n",
				mnttabtext, stbuf.st_dev, stbuf.st_rdev);
		}
		m.mnt_special = mnttabtext;
		m.mnt_mountp = mntpnt;
		m.mnt_fstype = MNTTYPE_NFS;
		m.mnt_mntopts = opts;
		if (add_mnttab(&m, stbuf.st_dev) != 0)  {
			if (trace > 1)
				trace_prt(1, " add_mnttab %s failed\n", mntpnt);

			syslog(LOG_ERR, "cannot add %s to /etc/mnttab", mntpnt);
		}
	} else {
		if (trace > 1) {
			trace_prt(1, "  mount %s OK\n", mnttabtext);
			trace_prt(1, "  stat of %s failed\n", mntpnt);
		}
		syslog(LOG_ERR,
			"cannot add %s to /etc/mnttab - stat failed", mntpnt);
	}

out:
	argp = head;
	while (argp) {
		if (argp->pathconf)
			free(argp->pathconf);
		free_knconf(argp->knconf);
		netbuf_free(argp->addr);
		nfs_free_secdata(argp->nfs_ext_u.nfs_ext2.secdata);
		free(argp->fh);
		head = argp;
		argp = argp->nfs_ext_u.nfs_ext2.next;
		free(head);
	}
	if (nfs_proto)
		free(nfs_proto);
	if (mnttabtext)
		free(mnttabtext);

	return (last_error);
}

/*
 * get_pathconf(cl, path, fsname)
 * ugliness that requires that ppathcnf and pathcnf stay consistent
 */
static struct pathcnf *
get_pathconf(cl, path, fsname)
	CLIENT *cl;
	char *path, *fsname;
{
	struct ppathcnf *p = NULL;
	enum clnt_stat rpc_stat;
	struct timeval timeout;

	p = (struct ppathcnf *) malloc(sizeof (struct ppathcnf));
	if (p == NULL) {
		syslog(LOG_ERR, "get_pathconf: Out of memory");
		return ((struct pathcnf *) 0);
	}
	memset((caddr_t) p, 0, sizeof (struct ppathcnf));

	timeout.tv_sec = 10;
	timeout.tv_usec = 0;
	rpc_stat = clnt_call(cl, MOUNTPROC_PATHCONF,
	    xdr_dirpath, (caddr_t) &path, xdr_ppathcnf, (caddr_t)p, timeout);
	if (rpc_stat != RPC_SUCCESS) {
		syslog(LOG_ERR,
			"get_pathconf: %s: server not responding: %s",
			fsname, clnt_sperror(cl, ""));
		free(p);
		return ((struct pathcnf *) 0);
	}
	if (_PC_ISSET(_PC_ERROR, p->pc_mask)) {
		syslog(LOG_ERR, "get_pathconf: no info for %s", fsname);
		free(p);
		return ((struct pathcnf *) 0);
	}
	return ((struct pathcnf *)p);
}

static struct knetconfig *
get_knconf(nconf)
	struct netconfig *nconf;
{
	struct stat stbuf;
	struct knetconfig *k;

	if (stat(nconf->nc_device, &stbuf) < 0) {
		syslog(LOG_ERR, "get_knconf: stat %s: %m", nconf->nc_device);
		return (NULL);
	}
	k = (struct knetconfig *) malloc(sizeof (*k));
	if (k == NULL)
		goto nomem;
	k->knc_semantics = nconf->nc_semantics;
	k->knc_protofmly = strdup(nconf->nc_protofmly);
	if (k->knc_protofmly == NULL)
		goto nomem;
	k->knc_proto = strdup(nconf->nc_proto);
	if (k->knc_proto == NULL)
		goto nomem;
	k->knc_rdev = stbuf.st_rdev;

	return (k);

nomem:
	syslog(LOG_ERR, "get_knconf: no memory");
	free_knconf(k);
	return (NULL);
}

static void
free_knconf(k)
	struct knetconfig *k;
{
	if (k == NULL)
		return;
	if (k->knc_protofmly)
		free(k->knc_protofmly);
	if (k->knc_proto)
		free(k->knc_proto);
	free(k);
}

void
netbuf_free(nb)
	struct netbuf *nb;
{
	if (nb == NULL)
		return;
	if (nb->buf)
		free(nb->buf);
	free(nb);
}

#define	SMALL_HOSTNAME		20
#define	SMALL_PROTONAME		10

struct portmap_cache {
	int cache_prog;
	int cache_vers;
	time_t cache_time;
	char cache_small_hosts[SMALL_HOSTNAME + 1];
	char *cache_hostname;
	char *cache_proto;
	char cache_small_proto[SMALL_PROTONAME + 1];
	struct netbuf cache_srv_addr;
	struct portmap_cache *cache_prev, *cache_next;
};

rwlock_t portmap_cache_lock;
static int portmap_cache_valid_time = 30;
struct portmap_cache *portmap_cache_head, *portmap_cache_tail;

/*
 * Returns 1 if the entry is found in the cache, 0 otherwise.
 */
static int
portmap_cache_lookup(hostname, prog, vers, nconf, addrp)
	char *hostname;
	int prog, vers;
	struct netconfig *nconf;
	struct netbuf *addrp;
{
	struct 	portmap_cache *cachep, *prev, *next = NULL, *cp;
	int	retval = 0;

	timenow = time(NULL);

	(void) rw_rdlock(&portmap_cache_lock);

	/*
	 * Increment the portmap cache counters for # accesses and lookups
	 * Use a smaller factor (100 vs 1000 for the host cache) since
	 * initial analysis shows this cache is looked up 10% that of the
	 * host cache.
	 */
#ifdef CACHE_DEBUG
	portmap_cache_accesses++;
	portmap_cache_lookups++;
	if ((portmap_cache_lookups%100) == 0)
		trace_portmap_cache();
#endif /* CACHE_DEBUG */

	for (cachep = portmap_cache_head; cachep;
		cachep = cachep->cache_next) {
		if (timenow > cachep->cache_time) {
			/*
			 * We stumbled across an entry in the cache which
			 * has timed out. Free up all the entries that
			 * were added before it, which will positionally
			 * be after this entry. And adjust neighboring
			 * pointers.
			 * When we drop the lock and re-acquire it, we
			 * need to start from the beginning.
			 */
			(void) rw_unlock(&portmap_cache_lock);
			(void) rw_wrlock(&portmap_cache_lock);
			for (cp = portmap_cache_head;
				cp && (cp->cache_time >= timenow);
				cp = cp->cache_next)
				;
			if (cp == NULL)
				goto done;
			/*
			 * Adjust the link of the predecessor.
			 * Make the tail point to the new last entry.
			 */
			prev = cp->cache_prev;
			if (prev == NULL) {
				portmap_cache_head = NULL;
				portmap_cache_tail = NULL;
			} else {
				prev->cache_next = NULL;
				portmap_cache_tail = prev;
			}
			for (; cp; cp = next) {
				if (cp->cache_hostname != NULL &&
				    cp->cache_hostname !=
				    cp->cache_small_hosts)
					free(cp->cache_hostname);
				if (cp->cache_proto != NULL &&
				    cp->cache_proto !=
				    cp->cache_small_proto)
					free(cp->cache_proto);
				if (cp->cache_srv_addr.buf != NULL)
					free(cp->cache_srv_addr.buf);
				next = cp->cache_next;
				free(cp);
			}
			goto done;
		}
		if (cachep->cache_hostname == NULL ||
		    prog != cachep->cache_prog || vers != cachep->cache_vers ||
		    strcmp(nconf->nc_proto, cachep->cache_proto) != 0 ||
		    strcmp(hostname, cachep->cache_hostname) != 0)
			continue;
		/*
		 * Cache Hit.
		 */
#ifdef CACHE_DEBUG
		portmap_cache_hits++;	/* up portmap cache hit counter */
#endif /* CACHE_DEBUG */
		addrp->len = cachep->cache_srv_addr.len;
		memcpy(addrp->buf, cachep->cache_srv_addr.buf, addrp->len);
		retval = 1;
		break;
	}
done:
	(void) rw_unlock(&portmap_cache_lock);
	return (retval);
}

static void
portmap_cache_enter(hostname, prog, vers, nconf, addrp)
	char *hostname;
	int prog, vers;
	struct netconfig *nconf;
	struct netbuf *addrp;
{
	struct portmap_cache *cachep;
	int protolen, hostnamelen;

	timenow = time(NULL);

	cachep = malloc(sizeof (struct portmap_cache));
	if (cachep == NULL)
		return;
	memset((char *) cachep, 0, sizeof (*cachep));

	hostnamelen = strlen(hostname);
	if (hostnamelen <= SMALL_HOSTNAME)
		cachep->cache_hostname = cachep->cache_small_hosts;
	else {
		cachep->cache_hostname = malloc(hostnamelen + 1);
		if (cachep->cache_hostname == NULL)
			goto nomem;
	}
	strcpy(cachep->cache_hostname, hostname);
	protolen = strlen(nconf->nc_proto);
	if (protolen <= SMALL_PROTONAME)
		cachep->cache_proto = cachep->cache_small_proto;
	else {
		cachep->cache_proto = malloc(protolen + 1);
		if (cachep->cache_proto == NULL)
			goto nomem;
	}
	strcpy(cachep->cache_proto, nconf->nc_proto);
	cachep->cache_prog = prog;
	cachep->cache_vers = vers;
	cachep->cache_time = timenow + portmap_cache_valid_time;
	cachep->cache_srv_addr.len = addrp->len;
	cachep->cache_srv_addr.buf = malloc(addrp->len);
	if (cachep->cache_srv_addr.buf == NULL)
		goto nomem;
	memcpy(cachep->cache_srv_addr.buf, addrp->buf, addrp->maxlen);
	cachep->cache_prev = NULL;
	(void) rw_wrlock(&portmap_cache_lock);
	/*
	 * There's a window in which we could have multiple threads making
	 * the same cache entry. This can be avoided by walking the cache
	 * once again here to check and see if there are duplicate entries
	 * (after grabbing the write lock). This isn't fatal and I'm not
	 * going to bother with this.
	 */
#ifdef CACHE_DEBUG
	portmap_cache_accesses++;	/* up portmap cache access counter */
#endif /* CACHE_DEBUG */
	cachep->cache_next = portmap_cache_head;
	portmap_cache_head = cachep;
	(void) rw_unlock(&portmap_cache_lock);
	return;

nomem:
	syslog(LOG_ERR, "portmap_cache_enter: Memory allocation failed");
	if (cachep->cache_srv_addr.buf)
		free(cachep->cache_srv_addr.buf);
	if (cachep->cache_proto && protolen > SMALL_PROTONAME)
		free(cachep->cache_proto);
	if (cachep->cache_hostname && hostnamelen > SMALL_HOSTNAME)
		free(cachep->cache_hostname);
	if (cachep)
		free(cachep);
	cachep = NULL;
}

static int
get_cached_srv_addr(hostname, prog, vers, nconf, addrp)
	char *hostname;
	int prog, vers;
	struct netconfig *nconf;
	struct netbuf *addrp;
{
	if (portmap_cache_lookup(hostname, prog, vers, nconf, addrp))
		return (1);
	if (rpcb_getaddr(prog, vers, nconf, addrp, hostname) == 0)
		return (0);
	portmap_cache_enter(hostname, prog, vers, nconf, addrp);
	return (1);
}

/*
 * Get the network address on "hostname" for program "prog"
 * with version "vers" by using the nconf configuration data
 * passed in.
 *
 * If the address of a netconfig pointer is null then
 * information is not sufficient and no netbuf will be returned.
 *
 * tinfo argument is for matching the get_the_addr() defined in
 * ../nfs/mount/mount.c
 */
static struct netbuf *
get_the_addr(hostname, prog, vers, nconf, port, tinfo)
	char *hostname;
	int prog, vers, port;
	struct netconfig *nconf;
	struct t_info *tinfo;
{
	struct netbuf *nb = NULL;
	struct t_bind *tbind = NULL;
	int fd = -1;
	enum clnt_stat cs;
	CLIENT *cl = NULL;
	struct timeval tv;

	if (nconf == NULL) {
		return (NULL);
	}

	if ((fd = t_open(nconf->nc_device, O_RDWR, tinfo)) < 0) {
		goto done;
	}

	/* LINTED pointer alignment */
	if ((tbind = (struct t_bind *) t_alloc(fd, T_BIND, T_ADDR))
		== NULL) {
		goto done;
	}

	if (get_cached_srv_addr(hostname, prog, vers, nconf, &tbind->addr) == 0)
		goto done;

	if (port) {
		/* LINTED pointer alignment */
		((struct sockaddr_in *) tbind->addr.buf)->sin_port =
					htons((u_short)port);
		cl = clnt_tli_create(fd, nconf, &tbind->addr, prog, vers, 0, 0);
		if (cl == NULL)
			goto done;

		tv.tv_sec = 10;
		tv.tv_usec = 0;
		cs = clnt_call(cl, NULLPROC, xdr_void, 0, xdr_void, 0, tv);
		clnt_destroy(cl);
		if (cs != RPC_SUCCESS)
			goto done;
	}

	/*
	 * Make a copy of the netbuf to return
	 */
	nb = (struct netbuf *) malloc(sizeof (struct netbuf));
	if (nb == NULL) {
		syslog(LOG_ERR, "no memory\n");
		goto done;
	}
	*nb = tbind->addr;
	nb->buf = (char *)malloc(nb->maxlen);
	if (nb->buf == NULL) {
		syslog(LOG_ERR, "no memory\n");
		free(nb);
		nb = NULL;
		goto done;
	}
	(void) memcpy(nb->buf, tbind->addr.buf, tbind->addr.len);

done:
	if (tbind) {
		t_free((char *) tbind, T_BIND);
		tbind = NULL;
	}
	if (fd >= 0)
		(void) t_close(fd);
	return (nb);
}

/*
 * Get a network address on "hostname" for program "prog"
 * with version "vers".  If the port number is specified (non zero)
 * then try for a TCP/UDP transport and set the port number of the
 * resulting IP address.
 *
 * If the address of a netconfig pointer was passed and
 * if it's not null, use it as the netconfig otherwise
 * assign the address of the netconfig that was used to
 * establish contact with the service.
 *
 * tinfo argument is for matching the get_addr() defined in
 * ../nfs/mount/mount.c
 */

static struct netbuf *
get_addr(hostname, prog, vers, nconfp, proto, port, tinfo)
	char *hostname, *proto;
	int prog, vers, port;
	struct netconfig **nconfp;
	struct t_info *tinfo;
{
	struct netbuf *nb = NULL;
	struct netconfig *nconf = NULL;
	NCONF_HANDLE *nc = NULL;
	int nthtry;

	nthtry = FIRST_TRY;

	if (nconfp && *nconfp)
		return (get_the_addr(hostname, prog, vers, *nconfp,
					port, tinfo));

	/*
	 * No nconf passed in.
	 *
	 * Try to get a nconf from /etc/netconfig.
	 * First choice is COTS, second is CLTS unless proto
	 * is specified.  When we retry, we reset the
	 * netconfig list, so that we search the whole list
	 * for the next choice.
	 */
	if ((nc = setnetpath()) == NULL)
		goto done;

	/*
	 * If proto is specified, then only search for the match,
	 * otherwise try COTS first, if failed, then try CLTS.
	 */
	if (proto) {
		while (nconf = getnetpath(nc)) {
			if (strcmp(nconf->nc_netid, proto) == 0) {
			/*
			 * If the port number is specified then TCP/UDP
			 * is needed. Otherwise any cots/clts will do.
			 */
			    if (port == 0)
				break;
			    if (strcmp(nconf->nc_protofmly, NC_INET) == 0 &&
				((strcmp(nconf->nc_proto, NC_TCP) == 0) ||
				(strcmp(nconf->nc_proto, NC_UDP) == 0)))
					break;
			    else {
				nconf = NULL;
				break;
			    }
			}
		}
		if (nconf == NULL)
			goto done;
		if ((nb = get_the_addr(hostname, prog, vers, nconf, port,
					tinfo)) == NULL)
			goto done;
	} else {
retry:
		while (nconf = getnetpath(nc)) {
			if (nconf->nc_flag & NC_VISIBLE) {
			    if (nthtry == FIRST_TRY) {
				if ((nconf->nc_semantics == NC_TPI_COTS_ORD) ||
					(nconf->nc_semantics == NC_TPI_COTS)) {
				    if (port == 0)
					break;
				    if ((strcmp(nconf->nc_protofmly,
					NC_INET) == 0) &&
					(strcmp(nconf->nc_proto, NC_TCP) == 0))
					break;
				}
			    }
			    if (nthtry == SECOND_TRY) {
				if (nconf->nc_semantics == NC_TPI_CLTS) {
				    if (port == 0)
					break;
				    if ((strcmp(nconf->nc_protofmly,
					NC_INET) == 0) &&
					(strcmp(nconf->nc_proto, NC_UDP) == 0))
					break;
				}
			    }
			}
		    } /* while */
		    if (nconf == NULL) {
			if (++nthtry <= MNT_PREF_LISTLEN) {
				endnetpath(nc);
				if ((nc = setnetpath()) == NULL)
					goto done;
				goto retry;
			} else
				goto done;
		    } else {
			if ((nb = get_the_addr(hostname, prog, vers, nconf,
					port, tinfo)) == NULL)
				/*
				 * Continue the same search path in the
				 * netconfig db until no more matched nconf
				 * (nconf == NULL).
				 */
				goto retry;
		    }
	} /* if !proto */

	/*
	 * Got nconf and nb.  Now dup the netconfig structure (nconf)
	 * and return it thru nconfp.
	 */
	*nconfp = getnetconfigent(nconf->nc_netid);
	if (*nconfp == NULL) {
		syslog(LOG_ERR, "no memory\n");
		free(nb);
		nb = NULL;
	}
done:
	if (nc)
		endnetpath(nc);
	return (nb);
}

/*
 * Sends a null call to the remote host's (NFS program, versp). versp
 * may be "NULL" in which case NFS_V3 is used.
 * Upon return, versp contains the maximum version supported iff versp!= NULL.
 */
enum clnt_stat
pingnfs(hostname, attempts, versp, versmin)
	char *hostname;
	int attempts;
	u_long *versp;
	u_long versmin;
{
	CLIENT *cl = NULL;
	struct timeval rpc_to_new = {15, 0};
	static struct timeval rpc_rtrans_new = {-1, -1};
	enum clnt_stat clnt_stat;
	int i, j;
	u_long versmax;
	u_long outvers;		/* version supported by host on last call */

	switch (cache_check(hostname, versp)) {
	case GOODHOST:
		return (RPC_SUCCESS);
	case DEADHOST:
		return (RPC_TIMEDOUT);
	case NOHOST:
	default:
		break;
	}

	/*
	 * XXX The retransmission time rpcbrmttime is a global defined
	 * in the rpc library (rpcb_clnt.c). We use (and like) the default
	 * value of 15 sec in the rpc library. The code below is to protect
	 * us in case it changes. This need not be done under a lock since
	 * any # of threads entering this function will get the same
	 * retransmission value.
	 */
	if (rpc_rtrans_new.tv_sec == -1 && rpc_rtrans_new.tv_usec == -1) {
		__rpc_control(CLCR_GET_RPCB_RMTTIME, (char *)&rpc_rtrans_new);
		if (rpc_rtrans_new.tv_sec != 15 && rpc_rtrans_new.tv_sec != 0)
			if (trace > 1)
				trace_prt(1, "RPC library rttimer changed\n");
	}

	/*
	 * XXX Manipulate the total timeout to get the number of
	 * desired retransmissions. This code is heavily dependant on
	 * the RPC backoff mechanism in clnt_dg_call (clnt_dg.c).
	 */
	for (i = 0, j = rpc_rtrans_new.tv_sec; i < attempts-1; i++) {
		if (j < RPC_MAX_BACKOFF) {
			j *= 2;
		}
		else
			j = RPC_MAX_BACKOFF;
		rpc_to_new.tv_sec += (long)j;
	}

	if (versp != NULL) {
		versmax = *versp;
		/* use versmin passed in */
	} else {
		versmax = NFS_V3;
		versmin = NFS_VERSMIN;
	}

	/*
	 * check the host's version within the timeout
	 */
	if (trace > 1)
		trace_prt(1, "  ping: %s timeout=%ld request version=%d\n",
				hostname, rpc_to_new.tv_sec, versmax);

	cl = clnt_create_vers_timed(hostname, NFS_PROGRAM, &outvers,
			versmin, versmax, "datagram_v", &rpc_to_new);


	if (cl == NULL) {
		if (verbose)
			syslog(LOG_ERR, "pingnfs: %s%s",
				hostname, clnt_spcreateerror(""));
		clnt_stat = RPC_TIMEDOUT;
	} else {
		clnt_destroy(cl);
		clnt_stat = RPC_SUCCESS;
	}

	if (trace > 1)
		clnt_stat == RPC_SUCCESS ?
			trace_prt(1, "  pingnfs OK: nfs version=%d\n", outvers):
			trace_prt(1, "  pingnfs FAIL: can't get nfs version\n");

	if (clnt_stat == RPC_SUCCESS) {
		cache_enter(hostname, versmax, outvers, GOODHOST);
		if (versp != NULL)
			*versp = outvers;
	} else
		cache_enter(hostname, versmax, versmax, DEADHOST);

	return (clnt_stat);
}

#define	RET_ERR		33
#define	MNTTYPE_LOFS    "lofs"

int
loopbackmount(fsname, dir, mntopts, overlay)
	char *fsname; 		/* Directory being mounted */
	char *dir;		/* Directory being mounted on */
	char *mntopts;
	int overlay;
{
	struct mnttab mnt;
	int fs_ind;
	int flags = 0;
	char fstype[] = MNTTYPE_LOFS;
	int dirlen;
	struct stat st;

	dirlen = strlen(dir);
	if (dir[dirlen-1] == ' ')
		dirlen--;

	if (dirlen == strlen(fsname) &&
		strncmp(fsname, dir, dirlen) == 0) {
		syslog(LOG_ERR,
			"Mount of %s on %s would result in deadlock, aborted\n",
			fsname, dir);
		return (RET_ERR);
	}
	mnt.mnt_mntopts = mntopts;
	if (hasmntopt(&mnt, MNTOPT_RO) != NULL)
		flags |= MS_RDONLY;

	if (overlay)
		flags |= MS_OVERLAY;

	if ((fs_ind = sysfs(GETFSIND, MNTTYPE_LOFS)) < 0) {
		syslog(LOG_ERR, "Mount of %s on %s: %m", fsname, dir);
		return (RET_ERR);
	}
	if (trace > 1)
		trace_prt(1,
			"  loopbackmount: fsname=%s, dir=%s, flags=%d\n",
			fsname, dir, flags);

	if (mount(fsname, dir, flags | MS_FSS, fs_ind, 0, 0) < 0) {
		syslog(LOG_ERR, "Mount of %s on %s: %m", fsname, dir);
		return (RET_ERR);
	}

	if (stat(dir, &st) == 0) {
		if (trace > 1) {
			trace_prt(1,
			    "  loopbackmount of %s on %s dev=%x rdev=%x OK\n",
			    fsname, dir, st.st_dev, st.st_rdev);
		}
		mnt.mnt_special = fsname;
		mnt.mnt_mountp  = dir;
		mnt.mnt_fstype  = fstype;
		mnt.mnt_mntopts = (flags & MS_RDONLY) ? MNTOPT_RO : MNTOPT_RW;
		if (add_mnttab(&mnt, st.st_dev) != 0) {
			if (trace > 1)
				trace_prt(1, "  add_mnttab(%s) failed\n", dir);

			syslog(LOG_ERR, "cannot add %s to /etc/mnttab", dir);
		}
	} else {
		if (trace > 1) {
			trace_prt(1,
			    "  loopbackmount of %s on %s OK\n", fsname, dir);
			trace_prt(1, "  stat of %s failed\n", dir);
		}
		syslog(LOG_ERR,
			"cannot add %s to /etc/mnttab - stat failed", dir);
	}

	return (0);
}

/*
 * Return the value of a numeric option of the form foo=x, if
 * option is not found or is malformed, return 0.
 */
static int
nopt(mnt, opt)
	struct mnttab *mnt;
	char *opt;
{
	int val = 0;
	char *equal;
	char *str;

	if (str = hasmntopt(mnt, opt)) {
		if (equal = strchr(str, '=')) {
			val = atoi(&equal[1]);
		} else {
			syslog(LOG_ERR, "Bad numeric option '%s'", str);
		}
	}
	return (val);
}

nfsunmount(mnt)
	struct mnttab *mnt;
{
	struct timeval timeout;
	CLIENT *cl;
	enum clnt_stat rpc_stat;
	char *host, *path;
	struct replica *list;
	int i, count = 0;

	if (trace > 1)
		trace_prt(1, "  nfsunmount: umount %s\n", mnt->mnt_mountp);

	if (umount(mnt->mnt_mountp) < 0) {
		if (trace > 1)
			trace_prt(1, "  nfsunmount: umount %s FAILED\n",
				mnt->mnt_mountp);
		if (errno)
			return (errno);
	}

	/*
	 * The rest of this code is advisory to the server.
	 * If it fails return success anyway.
	 */

	list = parse_replica(mnt->mnt_special, &count);
	if (!list) {
		if (count >= 0)
			syslog(LOG_ERR,
			    "Memory allocation failed: %m");
		return (ENOMEM);
	}

	for (i = 0; i < count; i++) {

		host = list[i].host;
		path = list[i].path;
		cl = clnt_create(host, MOUNTPROG, MOUNTVERS, "datagram_v");
		if (cl == NULL)
			break;
		if (__clnt_bindresvport(cl) < 0) {
			if (verbose)
				syslog(LOG_ERR, "umount %s:%s: %s",
					host, path,
					"Couldn't bind to reserved port");
			destroy_auth_client_handle(cl);
			break;
		}
		cl->cl_auth = authsys_create_default();
		timeout.tv_usec = 0;
		timeout.tv_sec = 5;
		rpc_stat = clnt_call(cl, MOUNTPROC_UMNT, xdr_dirpath,
			    (caddr_t)&path, xdr_void, (char *)NULL, timeout);
		if (verbose && rpc_stat != RPC_SUCCESS)
			syslog(LOG_ERR, "%s: %s",
				host, clnt_sperror(cl, "unmount"));
		destroy_auth_client_handle(cl);
	}

	free_replica(list, count);

	if (trace > 1)
		trace_prt(1, "  nfsunmount: umount %s OK\n", mnt->mnt_mountp);

done:
	return (0);
}

/*
 * Put a new entry in the cache chain by prepending it to the front.
 * If there isn't enough memory then just give up.
 */
static void
cache_enter(host, reqvers, outvers, state)
	char *host;
	u_long reqvers;
	u_long outvers;
	int state;
{
	struct cache_entry *entry;
	int cache_time = 30;	/* sec */

	timenow = time(NULL);

	entry = (struct cache_entry *)malloc(sizeof (struct cache_entry));
	if (entry == NULL)
		return;
	(void) memset((caddr_t) entry, 0, sizeof (struct cache_entry));
	entry->cache_host = strdup(host);
	if (entry->cache_host == NULL) {
		cache_free(entry);
		return;
	}
	entry->cache_reqvers = reqvers;
	entry->cache_outvers = outvers;
	entry->cache_state = state;
	entry->cache_time = timenow + cache_time;
	(void) rw_wrlock(&cache_lock);
#ifdef CACHE_DEBUG
	host_cache_accesses++;		/* up host cache access counter */
#endif /* CACHE DEBUG */
	entry->cache_next = cache_head;
	cache_head = entry;
	(void) rw_unlock(&cache_lock);
}

static int
cache_check(host, versp)
	char *host;
	u_long *versp;
{
	int state = NOHOST;
	struct cache_entry *ce, *prev;

	timenow = time(NULL);

	(void) rw_rdlock(&cache_lock);

#ifdef CACHE_DEBUG
	/* Increment the lookup and access counters for the host cache */
	host_cache_accesses++;
	host_cache_lookups++;
	if ((host_cache_lookups%1000) == 0)
		trace_host_cache();
#endif /* CACHE DEBUG */

	for (ce = cache_head; ce; ce = ce->cache_next) {
		if (timenow > ce->cache_time) {
			(void) rw_unlock(&cache_lock);
			(void) rw_wrlock(&cache_lock);
			for (prev = NULL, ce = cache_head; ce;
				prev = ce, ce = ce->cache_next) {
				if (timenow > ce->cache_time) {
					cache_free(ce);
					if (prev)
						prev->cache_next = NULL;
					else
						cache_head = NULL;
					break;
				}
			}
			(void) rw_unlock(&cache_lock);
			return (state);
		}
		if (strcmp(host, ce->cache_host) != 0)
			continue;
		if (versp == NULL ||
			(versp != NULL && *versp == ce->cache_reqvers) ||
			(versp != NULL && *versp == ce->cache_outvers)) {
				if (versp != NULL)
					*versp = ce->cache_outvers;
				state = ce->cache_state;

				/* increment the host cache hit counters */
#ifdef CACHE_DEBUG
				if (state == GOODHOST)
					goodhost_cache_hits++;
				if (state == DEADHOST)
					deadhost_cache_hits++;
#endif /* CACHE_DEBUG */
				(void) rw_unlock(&cache_lock);
				return (state);
		}
	}
	(void) rw_unlock(&cache_lock);
	return (state);
}

/*
 * Free a cache entry and all entries
 * further down the chain since they
 * will also be expired.
 */
static void
cache_free(entry)
	struct cache_entry *entry;
{
	struct cache_entry *ce, *next = NULL;

	for (ce = entry; ce; ce = next) {
		if (ce->cache_host)
			free(ce->cache_host);
		next = ce->cache_next;
		free(ce);
	}
}

/*
 * Returns 1, if port option is NFS_PORT or
 *	nfsd is running on the port given
 * Returns 0, if both port is not NFS_PORT and nfsd is not
 * 	running on the port.
 */

static int
is_nfs_port(char *opts)
{
	struct mnttab m;
	u_int nfs_port = 0;
	struct servent sv;
	char buf[256];

	m.mnt_mntopts = opts;

	/*
	 * Get port specified in options list, if any.
	 */
	nfs_port = nopt(&m, MNTOPT_PORT);

	/*
	 * if no port specified or it is same as NFS_PORT return nfs
	 * To use any other daemon the port number should be different
	 */
	if (nfs_port == 0 || nfs_port == NFS_PORT)
		return (1);
	/*
	 * If daemon is nfsd, return nfs
	 */
	if (getservbyport_r(nfs_port, NULL, &sv, buf, 256) == &sv &&
		strcmp(sv.s_name, "nfsd") == 0)
		return (1);

	/*
	 * daemon is not nfs
	 */
	return (0);
}


/*
 * destroy_auth_client_handle(cl)
 * destroys the created client handle
 */
static void
destroy_auth_client_handle(CLIENT *cl)
{
	if (cl) {
		if (cl->cl_auth) {
			AUTH_DESTROY(cl->cl_auth);
			cl->cl_auth = NULL;
		}
		clnt_destroy(cl);
	}
}

#ifdef CACHE_DEBUG
/*
 * trace_portmap_cache()
 * traces the portmap cache values at desired points
 */
static void
trace_portmap_cache()
{
	syslog(LOG_ERR, "portmap_cache: accesses=%d lookups=%d hits=%d\n",
		portmap_cache_accesses, portmap_cache_lookups,
		portmap_cache_hits);
}

/*
 * trace_host_cache()
 * traces the host cache values at desired points
 */
static void
trace_host_cache()
{
	syslog(LOG_ERR,
		"host_cache: accesses=%d lookups=%d deadhits=%d goodhits=%d\n",
		host_cache_accesses, host_cache_lookups, deadhost_cache_hits,
		goodhost_cache_hits);
}
#endif /* CACHE_DEBUG */
