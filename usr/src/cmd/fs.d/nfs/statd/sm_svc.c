/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)sm_svc.c	1.23	96/09/27 SMI"	/* SVr4.0 1.2	*/
/*
 *  		PROPRIETARY NOTICE (Combined)
 *
 *  This source code is unpublished proprietary information
 *  constituting, or derived under license from AT&T's Unix(r) System V.
 *  In addition, portions of such source code were derived from Berkeley
 *  4.3 BSD under license from the Regents of the University of
 *  California.
 *
 *
 *
 *  		Copyright Notice
 *
 *  Notice of copyright on this source code product does not indicate
 *  publication.
 *
 *  	(c) 1986-1989,1994-1996  Sun Microsystems, Inc.
 *  	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>
#include <netconfig.h>
#include <unistd.h>
#include <netdb.h>
#include <rpc/rpc.h>
#include <netinet/in.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <rpcsvc/sm_inter.h>
#include <thread.h>
#include <synch.h>
#include "sm_statd.h"


#define	home0		"/var/statmon"
#define	current0	"/var/statmon/sm"
#define	backup0		"/var/statmon/sm.bak"
#define	state0		"/var/statmon/state"

#define	home1		"statmon"
#define	current1	"statmon/sm/"
#define	backup1		"statmon/sm.bak/"
#define	state1		"statmon/state"

char STATE[MAXPATHLEN], CURRENT[MAXPATHLEN], BACKUP[MAXPATHLEN];
char STATD_HOME[MAXPATHLEN];

int debug;
char hostname[MAXHOSTNAMELEN];

/*
 * These variables will be used to store all the
 * alias names for the host, as well as the -a
 * command line hostnames.
 */
char *host_name[MAXIPADDRS]; /* store -a opts */
int  addrix; /* # of -a entries */


/*
 * The following 2 variables are meaningful
 * only under a HA configuration.
 */
char path_name[MAXPATHS][MAXPATHLEN]; /* store -p opts */
int  pathix; /* # of -p entries */

/* Global variables.  Refer to sm_statd.h for description */
mutex_t crash_lock;
int die;
int in_crash;
cond_t crash_finish;
mutex_t sm_trylock;
rwlock_t thr_rwlock;
cond_t retrywait;

/*
 * statd protocol
 * 	commands:
 * 		SM_STAT
 * 			returns stat_fail to caller
 * 		SM_MON
 * 			adds an entry to the monitor_q and the record_q
 *			This message is sent by the server lockd to the server
 *			statd, to indicate that a new client is to be monitored.
 *			It is also sent by the server lockd to the client statd
 *			to indicate that a new server is to be monitored.
 * 		SM_UNMON
 * 			removes an entry from the monitor_q and the record_q
 * 		SM_UNMON_ALL
 * 			removes all entries from a particular host from the
 * 			monitor_q and the record_q.  Our statd has this
 * 			disabled.
 * 		SM_SIMU_CRASH
 * 			simulate a crash.  removes everything from the
 * 			record_q and the recovery_q, then calls statd_init()
 * 			to restart things.  This message is sent by the server
 *			lockd to the server statd to have all clients notified
 *			that they should reclaim locks.
 * 		SM_NOTIFY
 *			Sent by statd on server to statd on client during
 *			crash recovery.  The client statd passes the info
 *			to its lockd so it can attempt to reclaim the locks
 *			held on the server.
 *
 * There are three main hash tables used to keep track of things.
 * 	mon_table
 * 		table that keeps track hosts statd must watch.  If one of
 * 		these hosts crashes, then any locks held by that host must
 * 		be released.
 * 	record_table
 * 		used to keep track of all the hostname files stored in
 * 		the directory /var/statmon/sm.  These are client hosts who
 *		are holding or have held a lock at some point.  Needed
 *		to determine if a file needs to be created for host in
 *		/var/statmon/sm.
 *	recov_q
 *		used to keep track hostnames during a recovery
 *
 * The entries are hashed based upon the name.
 *
 * There is a directory /var/statmon/sm which holds a file named
 * for each host that is holding (or has held) a lock.  This is
 * used during initialization on startup, or after a simulated
 * crash.
 */

static void
sm_prog_1(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	union {
		struct sm_name sm_stat_1_arg;
		struct mon sm_mon_1_arg;
		struct mon_id sm_unmon_1_arg;
		struct my_id sm_unmon_all_1_arg;
		struct stat_chge ntf_arg;
	} argument;

	union {
		sm_stat_res stat_resp;
		sm_stat	mon_resp;
	} result;

	bool_t (*xdr_argument)(), (*xdr_result)();
	char *(*local)();

	switch (rqstp->rq_proc) {
	case NULLPROC:
		svc_sendreply(transp, xdr_void, (caddr_t)NULL);
		return;

	case SM_STAT:
		xdr_argument = xdr_sm_name;
		xdr_result = xdr_sm_stat_res;
		local = (char *(*)()) sm_status;
		break;

	case SM_MON:
		xdr_argument = xdr_mon;
		xdr_result = xdr_sm_stat_res;
		local = (char *(*)()) sm_mon;
		break;

	case SM_UNMON:
		xdr_argument = xdr_mon_id;
		xdr_result = xdr_sm_stat;
		local = (char *(*)()) sm_unmon;
		break;

	case SM_UNMON_ALL:
		xdr_argument = xdr_my_id;
		xdr_result = xdr_sm_stat;
		local = (char *(*)()) sm_unmon_all;
		break;

	case SM_SIMU_CRASH:
		xdr_argument = xdr_void;
		xdr_result = xdr_void;
		local = (char *(*)()) sm_simu_crash;
		break;

	case SM_NOTIFY:
		xdr_argument = xdr_stat_chge;
		xdr_result = xdr_void;
		local = (char *(*)()) sm_notify;
		break;

	default:
		svcerr_noproc(transp);
		return;
	}

	(void) memset(&argument, 0, sizeof (argument));
	if (!svc_getargs(transp, xdr_argument, (caddr_t)&argument)) {
		svcerr_decode(transp);
		return;
	}

	(void) memset(&result, 0, sizeof (result));
	(*local)(&argument, &result);
	if (!svc_sendreply(transp, xdr_result, (caddr_t)&result)) {
		svcerr_systemerr(transp);
	}

	if (!svc_freeargs(transp, xdr_argument, (caddr_t)&argument)) {
			syslog(LOG_ERR, "statd: unable to free arguments\n");
		}
}

/*
 * Remove all files under directory path_dir.
 */
static int
remove_dir(path_dir)
char *path_dir;
{
	DIR	*dp;
	struct dirent   *dirp;
	char tmp_path[MAXPATHLEN];

	if ((dp = opendir(path_dir)) == (DIR *)NULL) {
		if (debug)
		    syslog(LOG_ERR,
			"warning: open directory %s failed: %m\n", path_dir);
		return (1);
	}

	for (dirp = readdir(dp); dirp != (struct dirent *)NULL;
		dirp = readdir(dp)) {
		if (strcmp(dirp->d_name, ".") != 0 &&
			strcmp(dirp->d_name, "..") != 0) {
			(void) strcpy(tmp_path, path_dir);
			(void) strcat(tmp_path, "/");
			(void) strcat(tmp_path, dirp->d_name);
			delete_file(tmp_path);
		}
	}
	(void) closedir(dp);
	return (0);
}

/*
 * Copy all files from directory from_dir to directory to_dir.
 */
void
copydir_from_to(from_dir, to_dir)
char *from_dir;
char *to_dir;
{
	DIR	*dp;
	struct dirent   *dirp;
	char path[MAXPATHLEN+MAXNAMELEN+2];

	if ((dp = opendir(from_dir)) == (DIR *)NULL) {
		if (debug)
		    syslog(LOG_ERR,
			"warning: open directory %s failed: %m\n", from_dir);
		return;
	}

	for (dirp = readdir(dp); dirp != (struct dirent *)NULL;
		dirp = readdir(dp)) {
		if (strcmp(dirp->d_name, ".") != 0 &&
			strcmp(dirp->d_name, "..") != 0) {
			(void) strcpy(path, to_dir);
			(void) strcat(path, "/");
			(void) strcat(path, dirp->d_name);
			(void) create_file(path);
		}
	}
	(void) closedir(dp);
}


main(argc, argv)
	int argc;
	char **argv;
{
	int t;
	int c;
	int ppid;
	extern char *optarg;
	int choice = 0;
	struct rlimit rl;
	int i;
	char buf[MAXPATHLEN+SM_MAXPATHLEN];
	int mode;
	int sz;
	addrix = 0;
	pathix = 0;

	(void) gethostname(hostname, MAXHOSTNAMELEN);

	while ((c = getopt(argc, argv, "Dd:a:p:")) != EOF)
		switch (c) {
		case 'd':
			(void) sscanf(optarg, "%d", &debug);
			break;
		case 'D':
			choice = 1;
			break;
		case 'a':
			if (addrix < MAXIPADDRS) {
				if (strcmp(hostname, optarg) != 0) {
					sz = strlen(optarg);
					if (sz < MAXHOSTNAMELEN) {
						host_name[addrix] =
							(char *) xmalloc(sz+1);
						if (host_name[addrix] !=
						    NULL) {
						(void) sscanf(optarg, "%s",
							host_name[addrix]);
							addrix++;
						}
					} else
					(void) fprintf(stderr,
				    "statd: -a name of host is too long.\n");
				}
			} else
				(void) fprintf(stderr,
				    "statd: -a exceeding maximum hostnames\n");
			break;
		case 'p':
			if (pathix < MAXPATHS) {
				if (strlen(optarg) < MAXPATHLEN) {
					(void) sscanf(optarg, "%s",
							path_name[pathix]);
					pathix++;
				} else
					(void) fprintf(stderr,
					"statd: -p pathname is too long.\n");
			} else
				(void) fprintf(stderr,
				"statd: -p exceeding maximum pathnames\n");
			break;
		default:
			(void) fprintf(stderr,
			"statd [-d level] [-D]\n");
			return (1);
		}

	/* Get other aliases from each interface. */
	merge_hosts();

	if (choice == 0) {
		(void) strcpy(STATD_HOME, home0);
		(void) strcpy(CURRENT, current0);
		(void) strcpy(BACKUP, backup0);
		(void) strcpy(STATE, state0);
	} else {
		(void) strcpy(STATD_HOME, home1);
		(void) strcpy(CURRENT, current1);
		(void) strcpy(BACKUP, backup1);
		(void) strcpy(STATE, state1);
	}
	if (debug)
		(void) printf("debug is on, create entry: %s, %s, %s\n",
			CURRENT, BACKUP, STATE);

	if (getrlimit(RLIMIT_NOFILE, &rl))
		(void) printf("statd: getrlimit failed. \n");

	/* Set maxfdlimit current soft limit */
	rl.rlim_cur = MAX_FDS;
	if (setrlimit(RLIMIT_NOFILE, &rl) != 0)
		syslog(LOG_ERR, "statd: unable to set RLIMIT_NOFILE to %d\n",
			MAX_FDS);

	if (!debug) {
		ppid = fork();
		if (ppid == -1) {
			(void) fprintf(stderr, "statd: fork failure\n");
			(void) fflush(stderr);
			abort();
		}
		if (ppid != 0) {
			exit(0);
		}
		for (t = 0; t < rl.rlim_max; t++)
			(void) close(t);

		(void) open("/dev/null", O_RDONLY);
		(void) open("/dev/null", O_WRONLY);
		(void) dup(1);
		(void) setsid();
		openlog("statd", LOG_PID, LOG_DAEMON);
	}

	/*
	 * Set to automatic mode such that threads are automatically
	 * created
	 */
	mode = RPC_SVC_MT_AUTO;
	if (!rpc_control(RPC_SVC_MTMODE_SET, &mode)) {
		syslog(LOG_ERR,
			"statd:unable to set automatic MT mode.");
		exit(1);
	}

	if (!svc_create(sm_prog_1, SM_PROG, SM_VERS, "netpath")) {
	    syslog(LOG_ERR,
		"statd: unable to create (SM_PROG, SM_VERS) for netpath.");
	    exit(1);
	}

	/*
	 * Copy all clients from alternate paths to /var/statmon/sm
	 * Remove alternate directory when copying is done.
	 */
	for (i = 0; i < pathix; i++) {
		(void) sprintf(buf, "%s/statmon", path_name[i]);
		if ((mkdir(buf, SM_DIRECTORY_MODE)) == -1) {
			if (errno != EEXIST) {
				syslog(LOG_ERR, "statd: mkdir %s error %m\n",
					buf);
				continue;
			}
		}

		(void) sprintf(buf, "%s/statmon/sm", path_name[i]);
		copydir_from_to(buf, CURRENT);
		(void) remove_dir(buf);
		(void) sprintf(buf, "%s/statmon/sm.bak", path_name[i]);
		copydir_from_to(buf, BACKUP);
		(void) remove_dir(buf);
	}

	rwlock_init(&thr_rwlock, USYNC_THREAD, NULL);
	mutex_init(&crash_lock, USYNC_THREAD, NULL);
	cond_init(&crash_finish, USYNC_THREAD, NULL);
	cond_init(&retrywait, USYNC_THREAD, NULL);
	sm_inithash();
	die = 0;
	/*
	 * This variable is set to ensure that an sm_crash
	 * request will not be done at the same time
	 * when a statd_init is being done, since sm_crash
	 * can reset some variables that statd_init will be using.
	 */
	in_crash = 1;
	statd_init();

	if (debug)
		(void) printf("Starting svc_run\n");
	svc_run();
	syslog(LOG_ERR, "statd: svc_run returned\n");
	/* NOTREACHED */
	thr_exit((void *) 1);

}
