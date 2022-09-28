#ident	"@(#)main.c	1.50	96/08/19 SMI"

/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * This file contains the argument parsing routines of the dhcpd daemon.
 * It corresponds to the START state as spec'ed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <time.h>
#include <sys/resource.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/systeminfo.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/dhcp.h>
#include <netdb.h>
#include <dhcdata.h>
#include "dhcpd.h"
#include "per_network.h"
#include "interfaces.h"
#include <locale.h>

extern int optind, opterr;
extern char *optarg;

int verbose;
int debug;
int noping = 0;			/* Always ping before offer by def */
int ethers_compat = 1;		/* set if server should check ethers table */
int server_mode = 1;		/* set if running in server mode */
int no_dhcptab;			/* set if no dhcptab exists */
int bootp_compat;		/* set if bootp compat mode enabled */
int be_automatic;		/* set if bootp server should allocate IPs */
int reinitialize;		/* set to reinitialize when idle */
int max_hops = DHCP_DEF_HOPS;	/* max relay hops before discard */
time_t off_secs = 0L;		/* def ttl of an offer */
time_t rescan_interval = 0L;	/* dhcptab rescan interval */
time_t abs_rescan = 0L;		/* absolute dhcptab rescan time */
struct in_addr	server_ip;	/* IP address of server's default hostname */

/*
 * This global variable keeps the total number of packets waiting to
 * be processed.  read_interfaces() increments this each time a packet
 * is received on an interface.  process_pkts() decrements this each
 * time it processes a packet.  read_interfaces() checks npkts and
 * does a non-blocking poll() if npkts is greater than zero.  If npkts
 * is zero, then read_interfaces() does a blocking poll().
 * This allows read_interfaces() to be called frequently to make sure no
 * packets are lost.
 */
u_long npkts;

/*
 * This global keeps a running total of all packets received by all
 * interfaces.
 */
u_long totpkts;

static void usage(void);
static void local_closelog(void);
static void sighup(int);
static void sigexit(int);

int
main(int argc, char *argv[])
{
	register int	c, i, ns;
	register int	err = 0;
	register int	timeout;
	register char	*tp, *datastore;
	struct rlimit	rl;
	struct hostent	*hp;
	int		tbl_err;
	char		*pathp = NULL;
	char		scratch[MAXHOSTNAMELEN + 1];

	if (geteuid() != (uid_t) 0) {
		(void) fprintf(stderr, gettext("Must be 'root' to run %s.\n"),
		    DHCPD);
		return (EPERM);
	}

	while ((c = getopt(argc, argv, "denvh:o:r:b:i:t:")) != -1) {
		switch (c) {
		case 'd':
			debug = 1;
			break;
		case 'n':
			noping = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'e':
			/* Disable ethers mode for PPC clients. */
			ethers_compat = 0;
			break;
		case 'r':
			/*
			 * Relay Agent Mode.  Arguments better be IP addrs
			 * or hostnames!
			 */
			server_mode = 0;

			if ((err = relay_agent_init(optarg)) != 0) {
				usage();
				return (err);
			}
			break;
		case 'b':
			/* Bootp compatibility mode. */
			bootp_compat = 1;

			if (strcmp(optarg, "automatic") == 0) {
				be_automatic = 1;
			} else if (strcmp(optarg, "manual") == 0) {
				be_automatic = 0;
			} else {
				usage();
				return (1);
			}
			break;
		case 'h':
			/* Non default relay hops maximum. */
			if (!isdigit(*optarg)) {
				(void) fprintf(stderr, gettext(
				    "Maximum relay hops must be numeric.\n"));
				return (1);
			}
			max_hops = atoi(optarg);
			if (max_hops == 0) {
				max_hops = DHCP_DEF_HOPS;
				(void) fprintf(stderr, gettext("Couldn't \
determine max hops from: %s, defaulting to: %d\n"), optarg, max_hops);
			}
			break;
		case 'i':
			/*
			 * Comma-separated list of interfaces.
			 * parsed and verified by find_interfaces().
			 */
			interfaces = optarg;
			break;
		case 'o':
			/* Time to Live (secs) for dhcp Offers. */
			if (!isdigit(*optarg)) {
				(void) fprintf(stderr, gettext(
				    "DHCP Offer TTL must be an integer.\n"));
				return (1);
			}
			off_secs = atoi(optarg);
			if (off_secs == 0) {
				(void) fprintf(stderr, gettext("Could not \
determine DHCP offer TTL from: %s, defaulting to: %d\n"),
				    optarg, DHCP_OFF_SECS);
				off_secs = DHCP_OFF_SECS;
			}
			break;
		case 't':
			/* dhcptab rescan interval (secs). */
			if (!isdigit(*optarg)) {
				(void) fprintf(stderr, gettext(
"dhcptab rescan interval must be an integer (minutes)\n"));
				return (1);
			}
			rescan_interval = atoi(optarg);
			if (rescan_interval == 0) {
				(void) fprintf(stderr, gettext("Zero dhcptab \
rescan interval, defaulting to no rescan.\n"));
			} else {
				abs_rescan = (rescan_interval * 60L) +
				    time(NULL);
			}
			break;
		default:
			usage();
			return (1);
		}
	}

	if (server_mode) {
		if (noping)
			(void) fprintf(stderr, gettext("\nWARNING: Disabling \
duplicate IP address detection!\n\n"));
		if (off_secs == 0L)
			off_secs = DHCP_OFF_SECS;	/* use default */
		if (ethers_compat && stat_boot_server() == 0) {
			/*
			 * Respect user's -b setting. Use -b manual as the
			 * default.
			 */
			if (bootp_compat == 0) {
				bootp_compat = 1;
				be_automatic = 0;
			}
		} else
			ethers_compat = 0;
		ns = dd_ns(&tbl_err, &pathp);
		if (ns == TBL_FAILURE) {
			switch (tbl_err) {
			case TBL_BAD_SYNTAX:
				(void) fprintf(stderr, gettext(
"%s: Bad syntax: keyword is missing colon (:)\n"), TBL_NS_FILE);
				err = 1;
				break;
			case TBL_BAD_NS:
				(void) fprintf(stderr, gettext(
"%s: Bad resource name. Must be 'files' or 'nisplus'.\n"), TBL_NS_FILE);
				err = 1;
				break;
			case TBL_BAD_DIRECTIVE:
				(void) fprintf(stderr, gettext(
"%s: Unsupported keyword. Must be 'resource:' or 'path:'.\n"), TBL_NS_FILE);
				err = 1;
				break;
			case TBL_STAT_ERROR:
				(void) fprintf(stderr, gettext(
"%s: Specified 'path' keyword value does not exist.\n"), TBL_NS_FILE);
				break;
			case TBL_BAD_DOMAIN:
				(void) fprintf(stderr, gettext(
"%s: Specified 'path' keyword value must be a valid nisplus domain name.\n"),
				    TBL_NS_FILE);
				err = 1;
				break;
			case TBL_OPEN_ERROR:
				if (ethers_compat) {
					(void) fprintf(stderr, gettext(
					    "WARNING: %s does not exist.\n"),
					    TBL_NS_FILE);
					err = 0; /* databases not required */
				} else {
					(void) fprintf(stderr, gettext(
					    "FATAL: %s does not exist.\n"),
					    TBL_NS_FILE);
					err = 1;
				}
				break;
			}
			if (err != 0)
				return (err);
		}
	} else {
		ethers_compat = 0;	/* Means nothing here. */
		if (noping)
			(void) fprintf(stderr, gettext(
			    "Option 'n' invalid in relay agent mode.\n"));
		if (rescan_interval)
			(void) fprintf(stderr, gettext(
			    "Option 't' invalid in relay agent mode.\n"));
		if (off_secs)
			(void) fprintf(stderr, gettext(
			    "Option 'o' invalid in relay agent mode.\n"));
		if (bootp_compat)
			(void) fprintf(stderr, gettext(
			    "Option 'b' invalid in relay agent mode.\n"));
		if (noping || rescan_interval || off_secs || bootp_compat) {
			usage();
			return (1);
		}
	}

	if (!debug) {
		/* Daemon (background, detach from controlling tty). */
		switch (fork()) {
		case -1:
			(void) fprintf(stderr,
			    gettext("Daemon cannot fork(): %s\n"),
			    strerror(errno));
			return (errno);
		case 0:
			/* child */
			break;
		default:
			/* parent */
			return (0);
		}

		if ((err = getrlimit(RLIMIT_NOFILE, &rl)) < 0) {
			dhcpmsg(LOG_ERR, "Can't get resource limits: %s\n",
			    strerror(errno));
			return (err);
		}

		for (i = 0; (rlim_t) i < rl.rlim_cur; i++)
			(void) close(i);

		errno = 0;	/* clean up benign bad file no error */

		(void) open("/dev/null", O_RDONLY, 0);
		(void) dup2(0, 1);
		(void) dup2(0, 2);

		/* Detach console */
		(void) setsid();

		(void) openlog(DHCPD, LOG_PID, LOG_DAEMON);
		if (verbose)
			dhcpmsg(LOG_INFO, "Daemon started.\n");
	}

	(void) setlocale(LC_ALL, "");

#if	!defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN	"SYS_TEXT"
#endif	/* ! TEXT_DOMAIN */

	(void) textdomain(TEXT_DOMAIN);

	/* Save away the IP address associated with our HOSTNAME. */
	(void) sysinfo(SI_HOSTNAME, scratch, MAXHOSTNAMELEN + 1);
	if ((tp = strchr(scratch, '.')) != NULL)
		*tp = '\0';

	if ((hp = gethostbyname(scratch)) != NULL &&
	    hp->h_addrtype == AF_INET &&
	    hp->h_length == sizeof (struct in_addr)) {
		(void) memcpy((char *)&server_ip, hp->h_addr_list[0],
		    sizeof (server_ip));
	} else {
		dhcpmsg(LOG_ERR,
		    "Cannot determine server hostname/IP address.\n");
		local_closelog();
		return (1);
	}

	if (verbose) {
		dhcpmsg(LOG_INFO, "Daemon Version: %s\n", DAEMON_VERS);
		dhcpmsg(LOG_INFO, "Maximum relay hops: %d\n", max_hops);
		if (server_mode) {
			dhcpmsg(LOG_INFO, "Run mode is: DHCP Server Mode.\n");
			switch (ns) {
			case TBL_NS_UFS:
				datastore = "files";
				break;
			case TBL_NS_NISPLUS:
				datastore = "nisplus";
				pathp = (
				    (tp = getenv("NIS_PATH")) == NULL ? pathp :
				    tp);
				break;
			default:
				datastore = pathp = "none";
				break;
			}
			dhcpmsg(LOG_INFO, "Datastore: %s\n", datastore);
			dhcpmsg(LOG_INFO, "Path: %s\n", pathp);
			dhcpmsg(LOG_INFO, "DHCP offer TTL: %d\n", off_secs);
			if (ethers_compat)
				dhcpmsg(LOG_INFO,
				    "Ethers compatibility enabled.\n");
			if (bootp_compat)
				dhcpmsg(LOG_INFO,
				    "BOOTP compatibility enabled.\n");
			if (rescan_interval != 0) {
				dhcpmsg(LOG_INFO,
				    "Dhcptab rescan interval: %d minutes.\n",
				    rescan_interval);
			}
		} else {
			dhcpmsg(LOG_INFO,
			    "Run mode is: Relay Agent Mode.\n");
		}
	}

	if ((err = find_interfaces()) != 0) {
		local_closelog();
		return (err);
	}
	if ((err = open_interfaces()) != 0) {
		local_closelog();
		return (err);
	}

	(void) sigset(SIGTERM, sigexit);
	(void) sigset(SIGINT, sigexit);

	if (server_mode) {

		if (inittab() != 0) {
			dhcpmsg(LOG_ERR, "Cannot allocate macro hash table.\n");
			local_closelog();
			return (1);
		}

		if ((err = checktab()) != 0 || (err = readtab()) != 0) {
			if (err == ENOENT || ethers_compat) {
				no_dhcptab = 1;
			} else {
				dhcpmsg(LOG_ERR,
				    "Error reading macro table.\n");
				local_closelog();
				return (err);
			}
		} else
			no_dhcptab = 0;
		(void) sigset(SIGHUP, sighup);		/* catch SIGHUP */
	}

	/*
	 * While forever, read packets off selected/available interfaces
	 * and dispatch off to handle them.
	 */
	for (;;) {
		if (npkts == 0 && server_mode) {
			/*
			 * Perform housekeeping when idle.
			 */
			if ((err = idle()) != 0)
				break;	/* Fatal error. */
		}

		timeout = (npkts != 0) ? 0 : DHCP_POLL_TIME;

		err = read_interfaces(timeout);
		if (err < 0) {
			if (errno == EINTR)
				continue;
			else
				break;
		}

		if ((err = process_pkts()) != 0)
			break;
	}

	/* Daemon terminated. */
	if (server_mode)
		resettab();

	(void) close_interfaces();
	local_closelog();
	return (err);
}

/*
 * SIGHUP signal handler.  Set a flag so we'll re-read dhcptab when we're
 * idle.
 */
void
sighup(int sigval)
{
	reinitialize = sigval;
}

/*
 * SIGTERM signal handler. Prints statistics, closes interfaces, frees
 * hash tables, etc.
 */
/* ARGSUSED */
void
sigexit(int sigval)
{
	char buf[SIG2STR_MAX];

	sig2str(sigval, buf);
	dhcpmsg(LOG_ERR, "Signal: %s received...Exiting\n", buf);

	if (server_mode)
		resettab();

	(void) close_interfaces();
	local_closelog();
	(void) fflush(NULL);
	exit(0);
}

static void
usage(void)
{
	(void) fprintf(stderr, gettext("%s [-d] [-e] [-n] [-v] [-i interface, \
...] [-h hops] [-t rescan_interval]\n\t[-o DHCP_offer_TTL] [-r IP | hostname, \
... | -b automatic | manual]\n"), DHCPD);
}

static void
local_closelog(void)
{
	dhcpmsg(LOG_INFO, "Daemon terminated.\n");
	if (!debug)
		closelog();
}
