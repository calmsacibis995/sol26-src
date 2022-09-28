/*
 * Copyright (c) 1992-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 *
 * Copyright (c) 1983, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)route.c	8.6 (Berkeley) 4/28/95
 *	@(#)linkaddr.c	8.1 (Berkeley) 6/4/93
 */

#pragma ident	"@(#)route.c	1.24	96/10/14 SMI"

#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#ifdef SYSV
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/tihdr.h>
#include <sys/tiuser.h>
#include <sys/timod.h>
#else
#include <sys/mbuf.h>
#include <sys/sysctl.h>
#endif

#include <net/if.h>
#include <net/route.h>
#ifdef SOCKADDR_DL
#include <net/if_dl.h>
#endif
#include <netinet/in.h>
#ifdef NS
#include <netns/ns.h>
#endif
#ifdef ISO
#include <netiso/iso.h>
#endif
#ifdef CCITT
#include <netccitt/x25.h>
#endif
#include <arpa/inet.h>
#include <netdb.h>
#ifdef SYSV
#include <inet/common.h>
#include <inet/mib2.h>
#include <inet/ip.h>
#endif

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#ifdef SYSV
#include <fcntl.h>
#else
#include <paths.h>
#endif

struct keytab {
	char	*kt_cp;
	int	kt_i;
} keywords[] = {
#define	K_ADD		1
	{"add",		K_ADD},
#define	K_BLACKHOLE	2
	{"blackhole",	K_BLACKHOLE},
#define	K_CHANGE	3
	{"change",	K_CHANGE},
#define	K_CLONING	4
	{"cloning",	K_CLONING},
#define	K_DELETE	5
	{"delete",	K_DELETE},
#define	K_DST		6
	{"dst",		K_DST},
#define	K_EXPIRE	7
	{"expire",	K_EXPIRE},
#define	K_FLUSH		8
	{"flush",	K_FLUSH},
#define	K_GATEWAY	9
	{"gateway",	K_GATEWAY},
#define	K_GENMASK	10
	{"genmask",	K_GENMASK},
#define	K_GET		11
	{"get",		K_GET},
#define	K_HOST		12
	{"host",	K_HOST},
#define	K_HOPCOUNT	13
	{"hopcount",	K_HOPCOUNT},
#define	K_IFACE		14
	{"iface",	K_IFACE},
#define	K_INTERFACE	15
	{"interface",	K_INTERFACE},
#define	K_IFA		16
	{"ifa",		K_IFA},
#define	K_IFP		17
	{"ifp",		K_IFP},
#define	K_INET		18
	{"inet",	K_INET},
#define	K_ISO		19
	{"iso",		K_ISO},
#define	K_LINK		20
	{"link",	K_LINK},
#define	K_LOCK		21
	{"lock",	K_LOCK},
#define	K_LOCKREST	22
	{"lockrest",	K_LOCKREST},
#define	K_MASK		23
	{"mask",	K_MASK},
#define	K_MONITOR	24
	{"monitor",	K_MONITOR},
#define	K_MTU		25
	{"mtu",		K_MTU},
#define	K_NET		26
	{"net",		K_NET},
#define	K_NETMASK	27
	{"netmask",	K_NETMASK},
#define	K_NOSTATIC	28
	{"nostatic",	K_NOSTATIC},
#define	K_OSI		29
	{"osi",		K_OSI},
#define	K_PROTO1	30
	{"proto1",	K_PROTO1},
#define	K_PROTO2	31
	{"proto2",	K_PROTO2},
#define	K_RECVPIPE	32
	{"recvpipe",	K_RECVPIPE},
#define	K_REJECT	33
	{"reject",	K_REJECT},
#define	K_RTT		34
	{"rtt",		K_RTT},
#define	K_RTTVAR	35
	{"rttvar",	K_RTTVAR},
#define	K_SA		36
	{"sa",		K_SA},
#define	K_SENDPIPE	37
	{"sendpipe",	K_SENDPIPE},
#define	K_SSTHRESH	38
	{"ssthresh",	K_SSTHRESH},
#define	K_STATIC	39
	{"static",	K_STATIC},
#define	K_X25		40
	{"x25",		K_X25},
#define	K_XNS		41
	{"xns",		K_XNS},
#define	K_XRESOLVE	42
	{"xresolve",	K_XRESOLVE},
	{0, 0}
};

#ifdef SYSV
struct	rtentry route;
#else
struct	ortentry route;
#endif
union	sockunion {
	struct	sockaddr sa;
	struct	sockaddr_in sin;
#ifdef NS
	struct	sockaddr_ns sns;
#endif
#ifdef ISO
	struct	sockaddr_iso siso;
#endif
#ifdef SOCKADDR_DL
	struct	sockaddr_dl sdl;
#endif
#ifdef CCITT
	struct	sockaddr_x25 sx25;
#endif
} so_dst, so_gate, so_mask, so_genmask, so_ifa, so_ifp;

typedef union sockunion *sup;
int	pid, rtm_addrs, uid;
int	s;
int	forcehost, forcenet, doflush, nflag, af, qflag, tflag, keyword();
int	iflag, verbose, aflen = sizeof (struct sockaddr_in);
int	locking, lockrest, debugonly;
int	fflag;
struct	rt_metrics rt_metrics;
u_long  rtm_inits;
struct	in_addr inet_makeaddr();
char	*routename(), *netname();
void	flushroutes(), newroute(), monitor(), sockaddr(), sodump(), bprintf();
void	print_getmsg(), print_rtmsg(), pmsg_common(), pmsg_addrs(), mask_addr();
int	getaddr(), rtmsg(), x25_makemask();
extern	char *inet_ntoa(), *iso_ntoa();
#ifdef SYSV
char	*link_ntoa();
void	link_addr();
#else
extern	char *link_ntoa();
#endif

struct {
	struct	rt_msghdr m_rtm;
	char	m_space[512];
} m_rtmsg;

#ifdef SYSV
typedef struct mib_item_s {
	struct mib_item_s	* next_item;
	long			group;
	long			mib_id;
	long			length;
	char			* valp;
} mib_item_t;

static	mib_item_t	*mibget();

void	link_addr();

#endif

#ifdef SYSV
void
#else
__dead void
#endif
usage(cp)
	char *cp;
{
	if (cp)
		(void) fprintf(stderr, "route: botched keyword: %s\n", cp);
	(void) fprintf(stderr,
	    "usage: route [ -nqv ] cmd [[ -<qualifers> ] args ]\n");
	exit(1);
	/* NOTREACHED */
}

void
quit(s)
	char *s;
{
	int sverrno = errno;

	(void) fprintf(stderr, "route: ");
	if (s)
		(void) fprintf(stderr, "%s: ", s);
	(void) fprintf(stderr, "%s\n", strerror(sverrno));
	exit(1);
	/* NOTREACHED */
}

#define	ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof (long) - 1))) : sizeof (long))
#ifdef SOCKET_LENGTHS
#define	SALEN(n) ((n)->sa_len))
#else
int	salen();
#define	SALEN(n) salen(n)
#endif
#define	ADVANCE(x, n) (x += ROUNDUP(SALEN(n)))

int
main(argc, argv)
	int argc;
	char **argv;
{
	extern int optind;
	int ch;

	if (argc < 2)
		usage((char *)NULL);

	while ((ch = getopt(argc, argv, "nqdtvf")) != EOF)
		switch (ch) {
		case 'n':
			nflag = 1;
			break;
		case 'q':
			qflag = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 't':
			tflag = 1;
			break;
		case 'd':
			debugonly = 1;
			break;
		case 'f':
			fflag = 1;
			break;
		case '?':
		default:
			usage((char *)NULL);
		}
	argc -= optind;
	argv += optind;

	pid = getpid();
	uid = getuid();
	if (tflag)
		s = open("/dev/null", O_WRONLY, 0);
	else
		s = socket(PF_ROUTE, SOCK_RAW, 0);
	if (s < 0)
		quit("socket");
	if (fflag)
		flushroutes(0, NULL);
	if (*argv)
		switch (keyword(*argv)) {
		case K_GET:
			uid = 0;
			/* FALLTHROUGH */

		case K_CHANGE:
		case K_ADD:
		case K_DELETE:
			newroute(argc, argv);
			exit(0);
			/* NOTREACHED */

		case K_MONITOR:
			monitor();
			/* NOTREACHED */

		case K_FLUSH:
			flushroutes(argc, argv);
			exit(0);
			/* NOTREACHED */
		}
	if (!fflag)
		usage(*argv);
	/* NOTREACHED */
}

/*
 * Purge all entries in the routing tables not
 * associated with network interfaces.
 */
void
flushroutes(argc, argv)
	int argc;
	char *argv[];
{
#ifdef SYSV
	int rlen, seqno;
	register struct rt_msghdr *rtm;
	struct	sockaddr_in sin;
	register char *cp;
	int sd;	/* mib stream */
	mib_item_t	* item;
	mib2_ipRouteEntry_t * rp;
#else
	size_t needed;
	int mib[6], rlen, seqno;
	char *buf, *next, *lim;
	register struct rt_msghdr *rtm;
#endif

	if (uid) {
		errno = EACCES;
		quit("must be root to alter routing table");
	}
#ifdef notdef
	shutdown(s, 0); /* Don't want to read back our messages */
#endif
	if (argc > 1) {
		argv++;
		if (argc == 2 && **argv == '-')
		    switch (keyword(*argv + 1)) {
			case K_INET:
				af = AF_INET;
				break;
#ifdef NS
			case K_XNS:
				af = AF_NS;
				break;
#endif
#ifdef SOCKADDR_DL
			case K_LINK:
				af = AF_LINK;
				break;
#endif
#ifdef ISO
			case K_ISO:
			case K_OSI:
				af = AF_ISO;
				break;
#endif
#ifdef CCITT
			case K_X25:
				af = AF_CCITT;
#endif
			default:
				goto bad;
		} else
bad:			usage(*argv);
	}
#ifdef SYSV
	sd = open("/dev/ip", O_RDWR);
	if (sd == -1) {
		perror("can't open mib stream");
		exit(1);
	}
	if ((item = mibget(sd)) == nilp(mib_item_t)) {
		fprintf(stderr, "mibget() failed\n");
		close(sd);
		exit(1);
	}
	if (verbose)
		(void) printf("Examining routing table from T_OPTMGMT_REQ\n");
	seqno = 0;		/* ??? */
	rtm = &m_rtmsg.m_rtm;
	for (; item; item = item->next_item) {
		/* skip all the other trash that comes up the mib stream */
		if ((item->group != MIB2_IP) || (item->mib_id != MIB2_IP_21))
			continue;
		for (rp = (mib2_ipRouteEntry_t *)item->valp;
		    (u_long) rp < (u_long) (item->valp + item->length);
		    rp++) {
			if ((rp->ipRouteInfo.re_ire_type != IRE_DEFAULT) &&
			    (rp->ipRouteInfo.re_ire_type != IRE_PREFIX) &&
			    (rp->ipRouteInfo.re_ire_type != IRE_HOST) &&
			    (rp->ipRouteInfo.re_ire_type != IRE_HOST_REDIRECT))
				continue;

			memset(rtm, 0, sizeof (m_rtmsg));
			rtm->rtm_type = RTM_DELETE;
			rtm->rtm_seq = seqno++;
			rtm->rtm_flags |= RTF_GATEWAY;
			if (rp->ipRouteMask == (IpAddress)-1)
				rtm->rtm_flags |= RTF_HOST;
			rtm->rtm_version = RTM_VERSION;
			rtm->rtm_addrs = RTA_DST | RTA_GATEWAY | RTA_NETMASK;
			cp = m_rtmsg.m_space;
			memset(&sin, 0, sizeof (struct sockaddr_in));
			sin.sin_family = AF_INET;
			sin.sin_addr.s_addr = rp->ipRouteDest;
			(void) memmove((void *) cp, (void *) &sin, aflen);
			cp += aflen;
			sin.sin_addr.s_addr = rp->ipRouteNextHop;
			(void) memmove((void *) cp, (void *) &sin, aflen);
			cp += aflen;
			sin.sin_addr.s_addr = rp->ipRouteMask;
			(void) memmove((void *) cp, (void *) &sin, aflen);
			cp += aflen;
			rtm->rtm_msglen = cp - (char *)&m_rtmsg;
			if (verbose)
				print_rtmsg(rtm, rtm->rtm_msglen);
			if (debugonly)
				continue;

			rlen = write(s, (char *)&m_rtmsg, rtm->rtm_msglen);
			if (rlen < (int)rtm->rtm_msglen) {
				if (rlen < 0) {
					(void) fprintf(stderr,
					    "route: write to routing "
					    "socket: %s\n",
					    strerror(errno));
				} else {
					(void) fprintf(stderr,
					    "route: write to routing socket "
					    "got only %d for rlen\n", rlen);
				}
				continue;
			}
			if (qflag)
				continue;
			if (verbose)
				print_rtmsg(rtm, rlen);
			else {
				struct sockaddr *sa = (struct sockaddr *)
				    (rtm + 1);
				(void) printf("%-20.20s ",
				    rtm->rtm_flags & RTF_HOST ? routename(sa) :
					netname(sa));
				sa = (struct sockaddr *)
				    (salen(sa) + (char *)sa);
				(void) printf("%-20.20s ", routename(sa));
				(void) printf("done\n");
			}
		}
	}
#else
	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;		/* protocol */
	mib[3] = 0;		/* wildcard address family */
	mib[4] = NET_RT_DUMP;
	mib[5] = 0;		/* no flags */
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
		quit("route-sysctl-estimate");
	if ((buf = malloc(needed)) == NULL)
		quit("malloc");
	if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
		quit("actual retrieval of routing table");
	lim = buf + needed;
	if (verbose)
		(void) printf("Examining routing table from sysctl\n");
	seqno = 0;		/* ??? */
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		if (verbose)
			print_rtmsg(rtm, rtm->rtm_msglen);
		if ((rtm->rtm_flags & RTF_GATEWAY) == 0)
			continue;
		if (af) {
			struct sockaddr *sa = (struct sockaddr *)(rtm + 1);

			if (sa->sa_family != af)
				continue;
		}
		if (debugonly)
			continue;
		rtm->rtm_type = RTM_DELETE;
		rtm->rtm_seq = seqno;
		rlen = write(s, next, rtm->rtm_msglen);
		if (rlen < (int)rtm->rtm_msglen) {
			(void) fprintf(stderr,
			    "route: write to routing socket: %s\n",
			    strerror(errno));
			(void) printf("got only %d for rlen\n", rlen);
			break;
		}
		seqno++;
		if (qflag)
			continue;
		if (verbose)
			print_rtmsg(rtm, rlen);
		else {
			struct sockaddr *sa = (struct sockaddr *)(rtm + 1);
			(void) printf("%-20.20s ", rtm->rtm_flags & RTF_HOST ?
			    routename(sa) : netname(sa));
			sa = (struct sockaddr *)(SALEN(sa) + (char *)sa);
			(void) printf("%-20.20s ", routename(sa));
			(void) printf("done\n");
		}
	}
#endif
}

char *
routename(sa)
	struct sockaddr *sa;
{
	register char *cp;
	static char line[50];
	struct hostent *hp;
	static char domain[MAXHOSTNAMELEN + 1];
	static int first = 1;
	char *ns_print();

	if (first) {
		first = 0;
		if (gethostname(domain, MAXHOSTNAMELEN) == 0 &&
		    (cp = strchr(domain, '.')))
			(void) strcpy(domain, cp + 1);
		else
			domain[0] = 0;
	}

	if (SALEN(sa) == 0)
		strcpy(line, "default");
	else switch (sa->sa_family) {

	case AF_INET:
	    {	struct in_addr in;
		in = ((struct sockaddr_in *)sa)->sin_addr;

		cp = 0;
		if (in.s_addr == INADDR_ANY || SALEN(sa) < 4)
			cp = "default";
		if (cp == 0 && !nflag) {
			hp = gethostbyaddr((char *)&in, sizeof (struct in_addr),
				AF_INET);
			if (hp) {
				if (((cp = strchr(hp->h_name, '.')) != NULL) &&
				    (strcmp(cp + 1, domain) == 0))
					*cp = 0;
				cp = hp->h_name;
			}
		}
		if (cp)
			strcpy(line, cp);
		else {
#define	C(x)	((x) & 0xff)
			in.s_addr = ntohl(in.s_addr);
			(void) sprintf(line, "%u.%u.%u.%u", C(in.s_addr >> 24),
			    C(in.s_addr >> 16), C(in.s_addr >> 8),
			    C(in.s_addr));
		}
		break;
	    }

#ifdef NS
	case AF_NS:
		return (ns_print((struct sockaddr_ns *)sa));
#endif

#ifdef SOCKADDR_DL
	case AF_LINK:
		return (link_ntoa((struct sockaddr_dl *)sa));
#endif

#ifdef ISO
	case AF_ISO:
		(void) sprintf(line, "iso %s",
		    iso_ntoa(&((struct sockaddr_iso *)sa)->siso_addr));
		break;
#endif

	default:
	    {	u_short *s = (u_short *)sa;
		u_short *slim = s + ((SALEN(sa) + 1) >> 1);
		char *cp = line + sprintf(line, "(%d)", sa->sa_family);

		while (++s < slim) /* start with sa->sa_data */
			cp += sprintf(cp, " %x", *s);
		break;
	    }
	}
	return (line);
}

/*
 * Return the name of the network whose address is given.
 * The address is assumed to be that of a net or subnet, not a host.
 */
char *
netname(sa)
	struct sockaddr *sa;
{
	char *cp = 0;
	static char line[50];
	struct netent *np = 0;
	u_long net, mask;
	register u_long i;
	int subnetshift;
	char *ns_print();

	switch (sa->sa_family) {

	case AF_INET:
	    {	struct in_addr in;
		in = ((struct sockaddr_in *)sa)->sin_addr;

		i = in.s_addr = ntohl(in.s_addr);
		if (in.s_addr == 0)
			cp = "default";
		else if (!nflag) {
			if (IN_CLASSA(i)) {
				mask = IN_CLASSA_NET;
				subnetshift = 8;
			} else if (IN_CLASSB(i)) {
				mask = IN_CLASSB_NET;
				subnetshift = 8;
			} else {
				mask = IN_CLASSC_NET;
				subnetshift = 4;
			}
			/*
			 * If there are more bits than the standard mask
			 * would suggest, subnets must be in use.
			 * Guess at the subnet mask, assuming reasonable
			 * width subnet fields.
			 */
			while (in.s_addr &~ mask)
				mask = (long)mask >> subnetshift;
			net = in.s_addr & mask;
			while ((mask & 1) == 0)
				mask >>= 1, net >>= 1;
			np = getnetbyaddr(net, AF_INET);
			if (np)
				cp = np->n_name;
		}
		if (cp)
			strcpy(line, cp);
		else if ((in.s_addr & 0xffffff) == 0)
			(void) sprintf(line, "%u", C(in.s_addr >> 24));
		else if ((in.s_addr & 0xffff) == 0)
			(void) sprintf(line, "%u.%u", C(in.s_addr >> 24),
			    C(in.s_addr >> 16));
		else if ((in.s_addr & 0xff) == 0)
			(void) sprintf(line, "%u.%u.%u", C(in.s_addr >> 24),
			    C(in.s_addr >> 16), C(in.s_addr >> 8));
		else
			(void) sprintf(line, "%u.%u.%u.%u", C(in.s_addr >> 24),
			    C(in.s_addr >> 16), C(in.s_addr >> 8),
			    C(in.s_addr));
		break;
	    }

#ifdef NS
	case AF_NS:
		return (ns_print((struct sockaddr_ns *)sa));
		break;
#endif

#ifdef SOCKADDR_DL
	case AF_LINK:
		return (link_ntoa((struct sockaddr_dl *)sa));
#endif

#ifdef ISO
	case AF_ISO:
		(void) sprintf(line, "iso %s",
		    iso_ntoa(&((struct sockaddr_iso *)sa)->siso_addr));
		break;
#endif

	default:
	    {	u_short *s = (u_short *)sa->sa_data;
		u_short *slim = s + ((SALEN(sa) + 1)>>1);
		char *cp = line + sprintf(line, "af %d:", sa->sa_family);

		while (s < slim)
			cp += sprintf(cp, " %x", *s++);
		break;
	    }
	}
	return (line);
}

void
set_metric(value, key)
	char *value;
	int key;
{
	int flag = 0;
	u_long noval, *valp = &noval;

	switch (key) {
#define	caseof(x, y, z)	case x: valp = &rt_metrics.z; flag = y; break
	caseof(K_MTU, RTV_MTU, rmx_mtu);
	caseof(K_HOPCOUNT, RTV_HOPCOUNT, rmx_hopcount);
	caseof(K_EXPIRE, RTV_EXPIRE, rmx_expire);
	caseof(K_RECVPIPE, RTV_RPIPE, rmx_recvpipe);
	caseof(K_SENDPIPE, RTV_SPIPE, rmx_sendpipe);
	caseof(K_SSTHRESH, RTV_SSTHRESH, rmx_ssthresh);
	caseof(K_RTT, RTV_RTT, rmx_rtt);
	caseof(K_RTTVAR, RTV_RTTVAR, rmx_rttvar);
	}
	rtm_inits |= flag;
	if (lockrest || locking)
		rt_metrics.rmx_locks |= flag;
	if (locking)
		locking = 0;
	*valp = atoi(value);
}

void
newroute(argc, argv)
	int argc;
	register char **argv;
{
	char *cmd, *dest = "", *gateway = "", *err;
	int ishost = 0, ret, attempts, oerrno, flags = RTF_STATIC;
	int key;
	struct hostent *hp = 0;

	if (uid) {
		errno = EACCES;
		quit("must be root to alter routing table");
	}
	cmd = argv[0];
#ifdef notdef
	if (*cmd != 'g')
		shutdown(s, 0); /* Don't want to read back our messages */
#endif
	while (--argc > 0) {
		key = keyword(*(++argv));
		if (key == K_HOST)
			forcehost++;
		else if (key == K_NET)
			forcenet++;
		else if (**(argv) == '-') {
			switch (key = keyword(1 + *argv)) {
#ifdef SOCKADDR_DL
			case K_LINK:
				af = AF_LINK;
				aflen = sizeof (struct sockaddr_dl);
				break;
#endif
#ifdef ISO
			case K_OSI:
			case K_ISO:
				af = AF_ISO;
				aflen = sizeof (struct sockaddr_iso);
				break;
#endif
			case K_INET:
				af = AF_INET;
				aflen = sizeof (struct sockaddr_in);
				break;
#ifdef CCITT
			case K_X25:
				af = AF_CCITT;
				aflen = sizeof (struct sockaddr_x25);
				break;
#endif
			case K_SA:
				af = PF_ROUTE;
				aflen = sizeof (union sockunion);
				break;
#ifdef XNS
			case K_XNS:
				af = AF_NS;
				aflen = sizeof (struct sockaddr_ns);
				break;
#endif
			case K_IFACE:
			case K_INTERFACE:
				iflag++;
				/* FALLTHRU */
			case K_NOSTATIC:
				flags &= ~RTF_STATIC;
				break;
			case K_LOCK:
				locking = 1;
				break;
			case K_LOCKREST:
				lockrest = 1;
				break;
			case K_HOST:
				forcehost++;
				break;
			case K_REJECT:
				flags |= RTF_REJECT;
				break;
			case K_BLACKHOLE:
				flags |= RTF_BLACKHOLE;
				break;
			case K_PROTO1:
				flags |= RTF_PROTO1;
				break;
			case K_PROTO2:
				flags |= RTF_PROTO2;
				break;
			case K_CLONING:
				flags |= RTF_CLONING;
				break;
			case K_XRESOLVE:
				flags |= RTF_XRESOLVE;
				break;
			case K_STATIC:
				flags |= RTF_STATIC;
				break;
			case K_IFA:
				argc--;
				(void) getaddr(RTA_IFA, *++argv, 0);
				break;
			case K_IFP:
				argc--;
				(void) getaddr(RTA_IFP, *++argv, 0);
				break;
			case K_GENMASK:
				argc--;
				(void) getaddr(RTA_GENMASK, *++argv, 0);
				break;
			case K_GATEWAY:
				argc--;
				(void) getaddr(RTA_GATEWAY, *++argv, 0);
				break;
			case K_DST:
				argc--;
				ishost = getaddr(RTA_DST, *++argv, &hp);
				dest = *argv;
				break;
			case K_NETMASK:
				argc--;
				(void) getaddr(RTA_NETMASK, *++argv, 0);
				/* FALLTHROUGH */
			case K_NET:
				forcenet++;
				break;
			case K_MTU:
			case K_HOPCOUNT:
			case K_EXPIRE:
			case K_RECVPIPE:
			case K_SENDPIPE:
			case K_SSTHRESH:
			case K_RTT:
			case K_RTTVAR:
				argc--;
				set_metric(*++argv, key);
				break;
			default:
				usage(1+*argv);
			}
		} else {
			if ((rtm_addrs & RTA_DST) == 0) {
				dest = *argv;
				ishost = getaddr(RTA_DST, *argv, &hp);
			} else if ((rtm_addrs & RTA_GATEWAY) == 0) {
				gateway = *argv;
				(void) getaddr(RTA_GATEWAY, *argv, &hp);
			} else {
				int ret = atoi(*argv);

				/*
				 * Assume that small numbers are metric
				 * Needed for compatibility with old route
				 * command syntax.
				 */
				if (ret == 0) {
				    if (strcmp(*argv, "0") == 0) {
					if (verbose) {
					    printf("old usage of trailing 0, "
						    "assuming route to if\n");
					}
				    } else
					usage((char *)NULL);
				    iflag = 1;
				    continue;
				} else if (ret > 0 && ret < 10) {
				    if (verbose) {
					printf("old usage of trailing digit, "
					    "assuming route via gateway\n");
				    }
				    iflag = 0;
				    continue;
				}
				(void) getaddr(RTA_NETMASK, *argv, 0);
			}
		}
	}
	if ((*cmd == 'a' || *cmd == 'd') && ((rtm_addrs & RTA_GATEWAY) == 0))
		usage((char *)NULL);

	/*
	 * If the netmask has been specified use it to determine RTF_HOST.
	 * Otherwise rely on the "-net" and "-host" specifiers.
	 * Final fallback is whether ot not any bits were set in the address
	 * past the classful network component.
	 */
	if (rtm_addrs & RTA_NETMASK) {
		if (so_mask.sin.sin_addr.s_addr == (u_long)-1)
			forcehost = 1;
		else
			forcenet = 1;
	}
	if (forcehost)
		ishost = 1;
	if (forcenet)
		ishost = 0;
	flags |= RTF_UP;
	if (ishost)
		flags |= RTF_HOST;
	if (iflag == 0)
		flags |= RTF_GATEWAY;
	for (attempts = 1; ; attempts++) {
		errno = 0;
		if ((ret = rtmsg(*cmd, flags)) == 0)
			break;
		if (errno != ENETUNREACH && errno != ESRCH)
			break;
		if (af == AF_INET && *gateway && hp && hp->h_addr_list[1]) {
			hp->h_addr_list++;
			memmove(&so_gate.sin.sin_addr, hp->h_addr_list[0],
			    hp->h_length);
		} else
			break;
	}
	oerrno = errno;
	if (*cmd != 'g') {
		(void) printf("%s %s %s", cmd, ishost? "host" : "net", dest);
		if (*gateway) {
		    (void) printf(": gateway %s", gateway);
		    if (attempts > 1 && ret == 0 && af == AF_INET)
			(void) printf(" (%s)", inet_ntoa(
				((struct sockaddr_in *)&route.rt_gateway)->
				sin_addr));
		}
		if (ret == 0)
			(void) printf("\n");
	}
	if (ret != 0) {
		if (*cmd == 'g')
			(void) printf("%s", dest);
		switch (oerrno) {
		case ESRCH:
			err = "not in table";
			break;
		case EBUSY:
			err = "entry in use";
			break;
		case ENOBUFS:
			err = "routing table overflow";
			break;
		case EEXIST:
			err = "entry exists";
			break;
		default:
			err = strerror(oerrno);
			break;
		}
		(void) printf(": %s\n", err);
	}
}


/*
 * Convert a network number to the corresponding IP address.
 * If the RTA_NETMASK hasn't been specified yet set it based
 * on the class of address.
 */
void
inet_makenetandmask(net, sin)
	u_long net;
	register struct sockaddr_in *sin;
{
	u_long addr, mask = 0;
	register char *cp;

	if (net == 0)
		mask = addr = 0;
	else if (net < 128) {
		addr = net << IN_CLASSA_NSHIFT;
		mask = IN_CLASSA_NET;
	} else if (net < 65536) {
		addr = net << IN_CLASSB_NSHIFT;
		mask = IN_CLASSB_NET;
	} else if (net < 16777216L) {
		addr = net << IN_CLASSC_NSHIFT;
		mask = IN_CLASSC_NET;
	} else {
		addr = net;
		if ((addr & IN_CLASSA_HOST) == 0)
			mask =  IN_CLASSA_NET;
		else if ((addr & IN_CLASSB_HOST) == 0)
			mask =  IN_CLASSB_NET;
		else if ((addr & IN_CLASSC_HOST) == 0)
			mask =  IN_CLASSC_NET;
		else
			mask = (u_long)-1;
	}
	sin->sin_addr.s_addr = htonl(addr);

	if (!(rtm_addrs & RTA_NETMASK)) {
		rtm_addrs |= RTA_NETMASK;
		sin = &so_mask.sin;
		sin->sin_addr.s_addr = htonl(mask);
#ifdef SOCKET_LENGTHS
		sin->sin_len = 0;
#endif
		sin->sin_family = AF_INET;
		cp = (char *)(&sin->sin_addr + 1);
		while (*--cp == 0 && cp > (char *)sin)
			;
#ifdef SOCKET_LENGTHS
		sin->sin_len = 1 + cp - (char *)sin;
#endif
	}
}

/*
 * Interpret an argument as a network address of some kind,
 * returning 1 if a host address, 0 if a network address.
 */
int
getaddr(which, s, hpp)
	int which;
	char *s;
	struct hostent **hpp;
{
	register sup su;
	struct ns_addr ns_addr();
	struct iso_addr *iso_addr();
	struct hostent *hp;
	struct netent *np;
	u_long val;

	if (af == 0) {
		af = AF_INET;
		aflen = sizeof (struct sockaddr_in);
	}
	rtm_addrs |= which;
	switch (which) {
	case RTA_DST:
		su = &so_dst;
		su->sa.sa_family = af;
		break;
	case RTA_GATEWAY:
		su = &so_gate;
		su->sa.sa_family = af;
		break;
	case RTA_NETMASK:
		su = &so_mask;
		su->sa.sa_family = af;
		break;
	case RTA_GENMASK:
		su = &so_genmask;
		su->sa.sa_family = af;
		break;
	case RTA_IFP:
		su = &so_ifp;
		su->sa.sa_family = af;
		break;
	case RTA_IFA:
		su = &so_ifa;
		su->sa.sa_family = af;
		break;
	default:
		usage("Internal Error");
		/*NOTREACHED*/
	}
#ifdef SOCKET_LENGTHS
	su->sa.sa_len = aflen;
#endif
	if (strcmp(s, "default") == 0) {
		switch (which) {
		case RTA_DST:
			forcenet++;
			(void) getaddr(RTA_NETMASK, s, NULL);
			break;
#ifdef SOCKET_LENGTHS
		case RTA_NETMASK:
		case RTA_GENMASK:
			su->sa.sa_len = 0;
#endif
		}
		return (0);
	}
	switch (af) {
#ifdef NS
	case AF_NS:
		if (which == RTA_DST) {
			extern short ns_bh[3];
			struct sockaddr_ns *sms = &(so_mask.sns);
			memset(sms, 0, sizeof (*sms));
			sms->sns_family = 0;
#ifdef SOCKET_LENGTHS
			sms->sns_len = 6;
#endif
			sms->sns_addr.x_net = *(union ns_net *)ns_bh;
			rtm_addrs |= RTA_NETMASK;
		}
		su->sns.sns_addr = ns_addr(s);
		return (!ns_nullhost(su->sns.sns_addr));
#endif

#ifdef ISO
	case AF_OSI:
		su->siso.siso_addr = *iso_addr(s);
		if (which == RTA_NETMASK || which == RTA_GENMASK) {
			register char *cp = (char *)TSEL(&su->siso);
			su->siso.siso_nlen = 0;
			do {--cp; } while ((cp > (char *)su) && (*cp == 0));
#ifdef SOCKET_LENGTHS
			su->siso.siso_len = 1 + cp - (char *)su;
#endif
		}
		return (1);
#endif

#ifdef SOCKADDR_DL
	case AF_LINK:
		link_addr(s, &su->sdl);
		return (1);
#endif

#ifdef CCITT
	case AF_CCITT:
		ccitt_addr(s, &su->sx25);
		return (which == RTA_DST ? x25_makemask() : 1);
#endif

	case PF_ROUTE:
#ifdef SOCKET_LENGTHS
		su->sa.sa_len = sizeof (*su);
#endif
		sockaddr(s, &su->sa);
		return (1);

	case AF_INET:
	default:
		break;
	}

	if (hpp == NULL)
		hpp = &hp;
	*hpp = NULL;
	if (((val = inet_addr(s)) != -1) &&
	    (which != RTA_DST || forcenet == 0)) {
		su->sin.sin_addr.s_addr = val;
		if (inet_lnaof(su->sin.sin_addr) != INADDR_ANY)
			return (1);
		else {
			val = ntohl(val);
			goto netdone;
		}
	}
	if ((val = inet_network(s)) != -1 ||
	    ((np = getnetbyname(s)) != NULL && (val = np->n_net) != 0)) {
netdone:
		if (which == RTA_DST)
			inet_makenetandmask(val, &su->sin);
		return (0);
	}
	hp = gethostbyname(s);
	if (hp) {
		*hpp = hp;
		su->sin.sin_family = hp->h_addrtype;
		memmove(&su->sin.sin_addr, hp->h_addr, hp->h_length);
		return (1);
	}
	(void) fprintf(stderr, "%s: bad value\n", s);
	exit(1);
}

#ifdef CCITT
int
x25_makemask()
{
	register char *cp;

	if ((rtm_addrs & RTA_NETMASK) == 0) {
		rtm_addrs |= RTA_NETMASK;
		for (cp = (char *)&so_mask.sx25.x25_net;
		    cp < &so_mask.sx25.x25_opts.op_flags; cp++)
			*cp = -1;
#ifdef SOCKET_LENGTHS
		so_mask.sx25.x25_len = (u_char)&(((sup)0)->sx25.x25_opts);
#endif
	}
	return (0);
}
#endif

#ifdef NS
short ns_nullh[] = {0, 0, 0};
short ns_bh[] = {-1, -1, -1};

char *
ns_print(sns)
	struct sockaddr_ns *sns;
{
	struct ns_addr work;
	union { union ns_net net_e; u_long long_e; } net;
	u_short port;
	static char mybuf[50], cport[10], chost[25];
	char *host = "";
	register char *p;
	register u_char *q;

	work = sns->sns_addr;
	port = ntohs(work.x_port);
	work.x_port = 0;
	net.net_e  = work.x_net;
	if (ns_nullhost(work) && net.long_e == 0) {
		if (!port)
			return ("*.*");
		(void) sprintf(mybuf, "*.%XH", port);
		return (mybuf);
	}

	if (memcmp(ns_bh, work.x_host.c_host, 6) == 0)
		host = "any";
	else if (memcmp(ns_nullh, work.x_host.c_host, 6) == 0)
		host = "*";
	else {
		q = work.x_host.c_host;
		(void) sprintf(chost, "%02X%02X%02X%02X%02X%02XH",
			q[0], q[1], q[2], q[3], q[4], q[5]);
		for (p = chost; *p == '0' && p < chost + 12; p++)
			/* void */;
		host = p;
	}
	if (port)
		(void) sprintf(cport, ".%XH", htons(port));
	else
		*cport = 0;

	(void) sprintf(mybuf, "%XH.%s%s", ntohl(net.long_e), host, cport);
	return (mybuf);
}
#endif

#ifndef SYSV
void
interfaces()
{
	size_t needed;
	int mib[6];
	char *buf, *lim, *next;
	register struct rt_msghdr *rtm;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;		/* protocol */
	mib[3] = 0;		/* wildcard address family */
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;		/* no flags */
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
		quit("route-sysctl-estimate");
	if ((buf = malloc(needed)) == NULL)
		quit("malloc");
	if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
		quit("actual retrieval of interface table");
	lim = buf + needed;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		print_rtmsg(rtm, rtm->rtm_msglen);
	}
}
#endif

void
monitor()
{
	int n;
	char msg[2048];

	verbose = 1;
#ifndef SYSV
	if (debugonly) {
		interfaces();
		exit(0);
	}
#endif
	for (;;) {
		n = read(s, msg, 2048);
		(void) printf("got message of size %d\n", n);
		print_rtmsg((struct rt_msghdr *)msg, n);
	}
}

int
rtmsg(cmd, flags)
	int cmd, flags;
{
	static int seq;
	int rlen;
	register char *cp = m_rtmsg.m_space;
	register int l;

#define	NEXTADDR(w, u) \
	if (rtm_addrs & (w)) {\
	    l = ROUNDUP(SALEN(&u.sa)); memmove(cp, &(u), l); cp += l;\
	    if (verbose) sodump(&(u), #u);\
	}

	errno = 0;
	memset(&m_rtmsg, 0, sizeof (m_rtmsg));
	if (cmd == 'a')
		cmd = RTM_ADD;
	else if (cmd == 'c')
		cmd = RTM_CHANGE;
	else if (cmd == 'g') {
		cmd = RTM_GET;
#ifdef SOCKADDR_DL
		if (so_ifp.sa.sa_family == 0) {
			so_ifp.sa.sa_family = AF_LINK;
#ifdef SOCKET_LENGTHS
			so_ifp.sa.sa_len = sizeof (struct sockaddr_dl);
#endif
			rtm_addrs |= RTA_IFP;
		}
#endif
	} else
		cmd = RTM_DELETE;
#define	rtm m_rtmsg.m_rtm
	rtm.rtm_type = cmd;
	rtm.rtm_flags = flags;
	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_seq = ++seq;
	rtm.rtm_addrs = rtm_addrs;
	rtm.rtm_rmx = rt_metrics;
	rtm.rtm_inits = rtm_inits;

#ifdef SOCKET_LENGTHS
	if (rtm_addrs & RTA_NETMASK)
		mask_addr();
#endif
	NEXTADDR(RTA_DST, so_dst);
	NEXTADDR(RTA_GATEWAY, so_gate);
	NEXTADDR(RTA_NETMASK, so_mask);
	NEXTADDR(RTA_GENMASK, so_genmask);
	NEXTADDR(RTA_IFP, so_ifp);
	NEXTADDR(RTA_IFA, so_ifa);
	rtm.rtm_msglen = l = cp - (char *)&m_rtmsg;
	if (verbose)
		print_rtmsg(&rtm, l);
	if (debugonly)
		return (0);
	if ((rlen = write(s, (char *)&m_rtmsg, l)) < 0) {
		switch (errno) {
		case ESRCH:
		case EBUSY:
		case ENOBUFS:
		case EEXIST:
		case ENETUNREACH:
		case EHOSTUNREACH:
			break;
		default:
			perror("writing to routing socket");
			break;
		}
		return (-1);
	}
	if (cmd == RTM_GET) {
		do {
			l = read(s, (char *)&m_rtmsg, sizeof (m_rtmsg));
		} while (l > 0 && (rtm.rtm_seq != seq || rtm.rtm_pid != pid));
		if (l < 0)
			(void) fprintf(stderr,
			    "route: read from routing socket: %s\n",
			    strerror(errno));
		else
			print_getmsg(&rtm, l);
	}
#undef rtm
	return (0);
}

#ifdef SOCKET_LENGTHS
void
mask_addr()
{
	int olen = SALEN(&so_mask.sa);
	register char *cp1 = olen + (char *)&so_mask, *cp2;

	for (so_mask.sa.sa_len = 0; cp1 > (char *)&so_mask; )
		if (*--cp1 != 0) {
			so_mask.sa.sa_len = 1 + cp1 - (char *)&so_mask;
			break;
		}
	if ((rtm_addrs & RTA_DST) == 0)
		return;
	switch (so_dst.sa.sa_family) {
#ifdef NS
	case AF_NS:
#endif
	case AF_INET:
#ifdef CCITT
	case AF_CCITT:
#endif
	case 0:
		return;
#ifdef ISO
	case AF_ISO:
		olen = MIN(so_dst.siso.siso_nlen,
		    MAX(SALEN(&so_mask.sa) - 6, 0));
		break;
#endif
	}
	cp1 = SALEN(&so_mask.sa) + 1 + (char *)&so_dst;
	cp2 = SALEN(&so_dst.sa) + 1 + (char *)&so_dst;
	while (cp2 > cp1)
		*--cp2 = 0;
	cp2 = SALEN(&so_mask.sa) + 1 + (char *)&so_mask;
	while (cp1 > so_dst.sa.sa_data)
		*--cp1 &= *--cp2;
	switch (so_dst.sa.sa_family) {
#ifdef ISO
	case AF_ISO:
		so_dst.siso.siso_nlen = olen;
		break;
#endif
	}
}
#endif

char *msgtypes[] = {
	"",
	"RTM_ADD: Add Route",
	"RTM_DELETE: Delete Route",
	"RTM_CHANGE: Change Metrics or flags",
	"RTM_GET: Report Metrics",
	"RTM_LOSING: Kernel Suspects Partitioning",
	"RTM_REDIRECT: Told to use different route",
	"RTM_MISS: Lookup failed on this address",
	"RTM_LOCK: fix specified metrics",
	"RTM_OLDADD: caused by SIOCADDRT",
	"RTM_OLDDEL: caused by SIOCDELRT",
	"RTM_RESOLVE: Route created by cloning",
	"RTM_NEWADDR: address being added to iface",
	"RTM_DELADDR: address being removed from iface",
	"RTM_IFINFO: iface status change",
	0,
};

char metricnames[] =
"\011pksent\010rttvar\7rtt\6ssthresh\5sendpipe\4recvpipe\3expire\2hopcount"
	"\1mtu";
char routeflags[] =
"\1UP\2GATEWAY\3HOST\4REJECT\5DYNAMIC\6MODIFIED\7DONE\010MASK_PRESENT"
	"\011CLONING\012XRESOLVE\013LLINFO\014STATIC\015BLACKHOLE"
	"\017PROTO2\020PROTO1";
char ifnetflags[] =
"\1UP\2BROADCAST\3DEBUG\4LOOPBACK\5PTP\6NOTRAILERS\7RUNNING\010NOARP"
	"\011PPROMISC\012ALLMULTI\013INTELLIGENT\014MULTICAST"
	"\015MULTI_BCAST\016UNNUMBERED\0170x4000\020PRIVATE";
char addrnames[] =
"\1DST\2GATEWAY\3NETMASK\4GENMASK\5IFP\6IFA\7AUTHOR\010BRD";

void
print_rtmsg(rtm, msglen)
	register struct rt_msghdr *rtm;
	int msglen;
{
	struct if_msghdr *ifm;
	struct ifa_msghdr *ifam;

	if (verbose == 0)
		return;
	if (rtm->rtm_version != RTM_VERSION) {
		(void) printf("routing message version %d not understood\n",
		    rtm->rtm_version);
		return;
	}
	(void) printf("%s: len %d, ", msgtypes[rtm->rtm_type], rtm->rtm_msglen);
	switch (rtm->rtm_type) {
	case RTM_IFINFO:
		ifm = (struct if_msghdr *)rtm;
		(void) printf("if# %d, flags:", ifm->ifm_index);
		bprintf(stdout, ifm->ifm_flags, ifnetflags);
		pmsg_addrs((char *)(ifm + 1), ifm->ifm_addrs);
		break;
	case RTM_NEWADDR:
	case RTM_DELADDR:
		ifam = (struct ifa_msghdr *)rtm;
		(void) printf("metric %d, flags:", ifam->ifam_metric);
		bprintf(stdout, ifam->ifam_flags, routeflags);
		pmsg_addrs((char *)(ifam + 1), ifam->ifam_addrs);
		break;
	default:
		(void) printf("pid: %d, seq %d, errno %d, flags:",
			rtm->rtm_pid, rtm->rtm_seq, rtm->rtm_errno);
		bprintf(stdout, rtm->rtm_flags, routeflags);
		pmsg_common(rtm);
	}
}

void
print_getmsg(rtm, msglen)
	register struct rt_msghdr *rtm;
	int msglen;
{
	struct sockaddr *dst = NULL, *gate = NULL, *mask = NULL;
#ifdef SOCKADDR_DL
	struct sockaddr_dl *ifp = NULL;
#endif
	register struct sockaddr *sa;
	register char *cp;
	register int i;

	(void) printf("   route to: %s\n", routename(&so_dst.sa));
	if (rtm->rtm_version != RTM_VERSION) {
		(void) fprintf(stderr,
		    "routing message version %d not understood\n",
		    rtm->rtm_version);
		return;
	}
	if (rtm->rtm_msglen > (u_short) msglen) {
		(void) fprintf(stderr,
		    "message length mismatch, in packet %d, returned %d\n",
		    rtm->rtm_msglen, msglen);
	}
	if (rtm->rtm_errno)  {
		(void) fprintf(stderr, "RTM_GET: %s (errno %d)\n",
		    strerror(rtm->rtm_errno), rtm->rtm_errno);
		return;
	}
	cp = ((char *)(rtm + 1));
	if (rtm->rtm_addrs)
		for (i = 1; i; i <<= 1)
			if (i & rtm->rtm_addrs) {
				sa = (struct sockaddr *)cp;
				switch (i) {
				case RTA_DST:
					dst = sa;
					break;
				case RTA_GATEWAY:
					gate = sa;
					break;
				case RTA_NETMASK:
					mask = sa;
					break;
#ifdef SOCKADDR_DL
				case RTA_IFP:
					if (sa->sa_family == AF_LINK &&
					    ((struct sockaddr_dl *)sa)->
						sdl_nlen)
						ifp = (struct sockaddr_dl *)sa;
					break;
#endif
				}
				ADVANCE(cp, sa);
			}
	if (dst && mask)
		mask->sa_family = dst->sa_family;	/* XXX */
	if (dst)
		(void) printf("destination: %s\n", routename(dst));
	if (mask) {
		int savenflag = nflag;

		nflag = 1;
		(void) printf("       mask: %s\n", routename(mask));
		nflag = savenflag;
	}
	if (gate && rtm->rtm_flags & RTF_GATEWAY)
		(void) printf("    gateway: %s\n", routename(gate));
#ifdef SOCKADDR_DL
	if (ifp)
		(void) printf("  interface: %.*s\n",
		    ifp->sdl_nlen, ifp->sdl_data);
#endif
	(void) printf("      flags: ");
	bprintf(stdout, rtm->rtm_flags, routeflags);

#ifdef SYSV
#define	__CONCAT(x, y)	x ## y
#endif
#define	lock(f)	((rtm->rtm_rmx.rmx_locks & __CONCAT(RTV_, f)) ? 'L' : ' ')
#define	msec(u)	(((u) + 500) / 1000)		/* usec to msec */

	(void) printf("\n%s\n", " recvpipe  sendpipe  ssthresh  rtt,msec    "
	    "rttvar  hopcount      mtu     expire");
	printf("%8d%c ", rtm->rtm_rmx.rmx_recvpipe, lock(RPIPE));
	printf("%8d%c ", rtm->rtm_rmx.rmx_sendpipe, lock(SPIPE));
	printf("%8d%c ", rtm->rtm_rmx.rmx_ssthresh, lock(SSTHRESH));
	printf("%8d%c ", msec(rtm->rtm_rmx.rmx_rtt), lock(RTT));
	printf("%8d%c ", msec(rtm->rtm_rmx.rmx_rttvar), lock(RTTVAR));
	printf("%8d%c ", rtm->rtm_rmx.rmx_hopcount, lock(HOPCOUNT));
	printf("%8d%c ", rtm->rtm_rmx.rmx_mtu, lock(MTU));
	if (rtm->rtm_rmx.rmx_expire)
		rtm->rtm_rmx.rmx_expire -= time(0);
	printf("%8d%c\n", rtm->rtm_rmx.rmx_expire, lock(EXPIRE));
#undef lock
#undef msec
#define	RTA_IGN	(RTA_DST|RTA_GATEWAY|RTA_NETMASK|RTA_IFP|RTA_IFA|RTA_BRD)
	if (verbose)
		pmsg_common(rtm);
	else if (rtm->rtm_addrs &~ RTA_IGN) {
		(void) printf("sockaddrs: ");
		bprintf(stdout, rtm->rtm_addrs, addrnames);
		putchar('\n');
	}
#undef	RTA_IGN
}

void
pmsg_common(rtm)
	register struct rt_msghdr *rtm;
{
	(void) printf("\nlocks: ");
	bprintf(stdout, rtm->rtm_rmx.rmx_locks, metricnames);
	(void) printf(" inits: ");
	bprintf(stdout, rtm->rtm_inits, metricnames);
	pmsg_addrs(((char *)(rtm + 1)), rtm->rtm_addrs);
}

void
pmsg_addrs(cp, addrs)
	char	*cp;
	int	addrs;
{
	register struct sockaddr *sa;
	int i;

	if (addrs == 0)
		return;
	(void) printf("\nsockaddrs: ");
	bprintf(stdout, addrs, addrnames);
	(void) putchar('\n');
	for (i = 1; i; i <<= 1)
		if (i & addrs) {
			sa = (struct sockaddr *)cp;
			(void) printf(" %s", routename(sa));
			ADVANCE(cp, sa);
		}
	(void) putchar('\n');
	(void) fflush(stdout);
}

void
bprintf(fp, b, s)
	register FILE *fp;
	register int b;
	register u_char *s;
{
	register int i;
	int gotsome = 0;

	if (b == 0)
		return;
	while ((i = *s++) != 0) {
		if (b & (1 << (i-1))) {
			if (gotsome == 0)
				i = '<';
			else
				i = ',';
			(void) putc(i, fp);
			gotsome = 1;
			for (; (i = *s) > 32; s++)
				(void) putc(i, fp);
		} else
			while (*s > 32)
				s++;
	}
	if (gotsome)
		(void) putc('>', fp);
}

int
keyword(cp)
	char *cp;
{
	register struct keytab *kt = keywords;

	while (kt->kt_cp && strcmp(kt->kt_cp, cp))
		kt++;
	return (kt->kt_i);
}

void
sodump(su, which)
	register sup su;
	char *which;
{
	switch (su->sa.sa_family) {
#ifdef SOCKADDR_DL
	case AF_LINK:
		(void) printf("%s: link %s; ",
		    which, link_ntoa(&su->sdl));
		break;
#endif
#ifdef ISO
	case AF_ISO:
		(void) printf("%s: iso %s; ",
		    which, iso_ntoa(&su->siso.siso_addr));
		break;
#endif
	case AF_INET:
		(void) printf("%s: inet %s; ",
		    which, inet_ntoa(su->sin.sin_addr));
		break;
#ifdef NS
	case AF_NS:
		(void) printf("%s: xns %s; ",
		    which, ns_ntoa(su->sns.sns_addr));
		break;
#endif
	}
	(void) fflush(stdout);
}

/* States */
#define	VIRGIN	0
#define	GOTONE	1
#define	GOTTWO	2
#define	RESET	3
/* Inputs */
#define	DIGIT	(4*0)
#define	END	(4*1)
#define	DELIM	(4*2)
#define	LETTER	(4*3)

void
sockaddr(addr, sa)
	register char *addr;
	register struct sockaddr *sa;
{
	register char *cp = (char *)sa;
	int size = SALEN(sa);
	char *cplim = cp + size;
	register int byte = 0, state = VIRGIN, new;

	memset(cp, 0, size);
	cp++;
	do {
		if ((*addr >= '0') && (*addr <= '9')) {
			new = *addr - '0';
		} else if ((*addr >= 'a') && (*addr <= 'f')) {
			new = *addr - 'a' + 10;
		} else if ((*addr >= 'A') && (*addr <= 'F')) {
			new = *addr - 'A' + 10;
		} else if (*addr == 0)
			state |= END;
		else
			state |= DELIM;
		addr++;
		switch (state /* | INPUT */) {
		case GOTTWO | DIGIT:
			*cp++ = byte; /*FALLTHROUGH*/
		case VIRGIN | DIGIT:
			state = GOTONE; byte = new; continue;
		case GOTONE | DIGIT:
			state = GOTTWO; byte = new + (byte << 4); continue;
		default: /* | DELIM */
			state = VIRGIN; *cp++ = byte; byte = 0; continue;
		case GOTONE | END:
		case GOTTWO | END:
			*cp++ = byte; /* FALLTHROUGH */
		case VIRGIN | END:
			break;
		}
		break;
	} while (cp < cplim);
#ifdef SOCKET_LENGTHS
	sa->sa_len = cp - (char *)sa;
#endif
}

#ifndef SOCKET_LENGTHS
int
salen(sa)
	register struct sockaddr *sa;
{
	switch (sa->sa_family) {
	case AF_INET:
		return (sizeof (struct sockaddr_in));
#ifdef SOCKADDR_DL
	case AF_LINK:
		return (sizeof (struct sockaddr_dl));
#endif
	default:
		return (sizeof (struct sockaddr));
	}
}
#endif

#ifdef SYSV
void
link_addr(addr, sdl)
	register const char *addr;
	register struct sockaddr_dl *sdl;
{
	register char *cp = sdl->sdl_data;
	char *cplim = sizeof (struct sockaddr_dl) + (char *)sdl;
	register int byte = 0, state = VIRGIN, new;

	bzero((char *)sdl, sizeof (struct sockaddr_dl));
	sdl->sdl_family = AF_LINK;
	do {
		state &= ~LETTER;
		if ((*addr >= '0') && (*addr <= '9')) {
			new = *addr - '0';
		} else if ((*addr >= 'a') && (*addr <= 'f')) {
			new = *addr - 'a' + 10;
		} else if ((*addr >= 'A') && (*addr <= 'F')) {
			new = *addr - 'A' + 10;
		} else if (*addr == 0) {
			state |= END;
		} else if (state == VIRGIN &&
		    (((*addr >= 'A') && (*addr <= 'Z')) ||
		    ((*addr >= 'a') && (*addr <= 'z'))))
			state |= LETTER;
		else
			state |= DELIM;
		addr++;
		switch (state /* | INPUT */) {
		case VIRGIN | DIGIT:
		case VIRGIN | LETTER:
			*cp++ = addr[-1];
			continue;
		case VIRGIN | DELIM:
			state = RESET;
			sdl->sdl_nlen = cp - sdl->sdl_data;
			continue;
		case GOTTWO | DIGIT:
			*cp++ = byte;
			/* FALLTHROUGH */
		case RESET | DIGIT:
			state = GOTONE;
			byte = new;
			continue;
		case GOTONE | DIGIT:
			state = GOTTWO;
			byte = new + (byte << 4);
			continue;
		default: /* | DELIM */
			state = RESET;
			*cp++ = byte;
			byte = 0;
			continue;
		case GOTONE | END:
		case GOTTWO | END:
			*cp++ = byte;
			/* FALLTHROUGH */
		case RESET | END:
			break;
		}
		break;
	} while (cp < cplim);
	sdl->sdl_alen = cp - LLADDR(sdl);
}

static char hexlist[] = "0123456789abcdef";

char *
link_ntoa(sdl)
	register const struct sockaddr_dl *sdl;
{
	static char obuf[64];
	register char *out = obuf;
	register int i;
	register u_char *in = (u_char *)LLADDR(sdl);
	u_char *inlim = in + sdl->sdl_alen;
	int firsttime = 1;

	if (sdl->sdl_nlen) {
		bcopy(sdl->sdl_data, obuf, sdl->sdl_nlen);
		out += sdl->sdl_nlen;
		if (sdl->sdl_alen)
			*out++ = ':';
	}
	while (in < inlim) {
		if (firsttime)
			firsttime = 0;
		else
			*out++ = '.';
		i = *in++;
		if (i > 0xf) {
			out[1] = hexlist[i & 0xf];
			i >>= 4;
			out[0] = hexlist[i];
			out += 2;
		} else
			*out++ = hexlist[i];
	}
	*out = 0;
	return (obuf);
}

static mib_item_t *
mibget(sd)
	int		sd;
{
	char			buf[512];
	int			flags;
	int			i, j, getcode;
	struct strbuf		ctlbuf, databuf;
	struct T_optmgmt_req	* tor = (struct T_optmgmt_req *)buf;
	struct T_optmgmt_ack	* toa = (struct T_optmgmt_ack *)buf;
	struct T_error_ack	* tea = (struct T_error_ack *)buf;
	struct opthdr		* req;
	mib_item_t		* first_item = nilp(mib_item_t);
	mib_item_t		* last_item  = nilp(mib_item_t);
	mib_item_t		* temp;

	tor->PRIM_type = T_OPTMGMT_REQ;
	tor->OPT_offset = sizeof (struct T_optmgmt_req);
	tor->OPT_length = sizeof (struct opthdr);
	tor->MGMT_flags = T_CURRENT;
	req = (struct opthdr *)&tor[1];
	req->level = MIB2_IP;		/* any MIB2_xxx value ok here */
	req->name  = 0;
	req->len   = 0;

	ctlbuf.buf = buf;
	ctlbuf.len = tor->OPT_length + tor->OPT_offset;
	flags = 0;
	if (putmsg(sd, &ctlbuf, nilp(struct strbuf), flags) == -1) {
		perror("mibget: putmsg(ctl) failed");
		goto error_exit;
	}
	/*
	 * each reply consists of a ctl part for one fixed structure
	 * or table, as defined in mib2.h.  The format is a T_OPTMGMT_ACK,
	 * containing an opthdr structure.  level/name identify the entry,
	 * len is the size of the data part of the message.
	 */
	req = (struct opthdr *)&toa[1];
	ctlbuf.maxlen = sizeof (buf);
	for (j = 1; ; j++) {
		flags = 0;
		getcode = getmsg(sd, &ctlbuf, nilp(struct strbuf), &flags);
		if (getcode == -1) {
			perror("mibget getmsg(ctl) failed");
			if (verbose) {
				fprintf(stderr, "#   level   name    len\n");
				i = 0;
				for (last_item = first_item; last_item;
					last_item = last_item->next_item)
					printf("%d  %4d   %5d   %d\n", ++i,
						last_item->group,
						last_item->mib_id,
						last_item->length);
			}
			goto error_exit;
		}
		if (getcode == 0 &&
		    ctlbuf.len >= sizeof (struct T_optmgmt_ack) &&
		    toa->PRIM_type == T_OPTMGMT_ACK &&
		    toa->MGMT_flags == T_SUCCESS &&
		    req->len == 0) {
			if (verbose)
				printf("mibget getmsg() %d returned EOD "
				    "(level %d, name %d)\n", j, req->level,
				    req->name);
			return (first_item);		/* this is EOD msg */
		}

		if (ctlbuf.len >= sizeof (struct T_error_ack) &&
		    tea->PRIM_type == T_ERROR_ACK) {
			fprintf(stderr, "mibget %d gives T_ERROR_ACK: "
			    "TLI_error = 0x%x, UNIX_error = 0x%x\n", j, getcode,
			    tea->TLI_error, tea->UNIX_error);
			errno = (tea->TLI_error == TSYSERR)
				? tea->UNIX_error : EPROTO;
			goto error_exit;
		}

		if (getcode != MOREDATA ||
		    ctlbuf.len < sizeof (struct T_optmgmt_ack) ||
		    toa->PRIM_type != T_OPTMGMT_ACK ||
		    toa->MGMT_flags != T_SUCCESS) {
			printf("mibget getmsg(ctl) %d returned %d, "
			    "ctlbuf.len = %d, PRIM_type = %d\n", j, getcode,
			    ctlbuf.len, toa->PRIM_type);
			if (toa->PRIM_type == T_OPTMGMT_ACK)
				printf("T_OPTMGMT_ACK: MGMT_flags = 0x%x, "
				    "req->len = %d\n", toa->MGMT_flags,
				    req->len);
			errno = ENOMSG;
			goto error_exit;
		}

		temp = (mib_item_t *)malloc(sizeof (mib_item_t));
		if (!temp) {
			perror("mibget malloc failed");
			goto error_exit;
		}
		if (last_item)
			last_item->next_item = temp;
		else
			first_item = temp;
		last_item = temp;
		last_item->next_item = nilp(mib_item_t);
		last_item->group = req->level;
		last_item->mib_id = req->name;
		last_item->length = req->len;
		last_item->valp = (char *)malloc(req->len);
		if (verbose)
			printf("msg %d:  group = %4d   mib_id = %5d   "
			    "length = %d\n", j, last_item->group,
			    last_item->mib_id, last_item->length);

		databuf.maxlen = last_item->length;
		databuf.buf    = last_item->valp;
		databuf.len    = 0;
		flags = 0;
		getcode = getmsg(sd, nilp(struct strbuf), &databuf, &flags);
		if (getcode == -1) {
			perror("mibget getmsg(data) failed");
			goto error_exit;
		} else if (getcode != 0) {
			printf("mibget getmsg(data) returned %d, "
			    "databuf.maxlen = %d, databuf.len = %d\n", getcode,
			    databuf.maxlen, databuf.len);
			goto error_exit;
		}
	}

error_exit:;
	while (first_item) {
		last_item = first_item;
		first_item = first_item->next_item;
		free(last_item);
	}
	return (first_item);
}
#endif
