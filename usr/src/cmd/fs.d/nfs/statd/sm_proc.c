/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)sm_proc.c	1.22	96/09/03 SMI"	/* SVr4.0 1.2	*/
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
 * 		Copyright Notice
 *
 *  Notice of copyright on this source code product does not indicate
 *  publication.
 *
 *  	(c) 1986,1987,1988,1989,1994-1996  Sun Microsystems, Inc.
 *  	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 */
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <rpc/rpc.h>
#include <rpcsvc/sm_inter.h>
#include <memory.h>
#include <net/if.h>
#include <sys/sockio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netdir.h>
#include <synch.h>
#include <thread.h>
#include "sm_statd.h"


static int local_state;		/* fake local sm state */

/*
 * maximum size of list of interface addresses if the socket call
 * to get the real number fails for some reason.  The list of
 * interface address is used in getmyaddrs() and my_name_check()
 */
#define	MAXIFS	32
#define	LOGHOST "loghost"

static void delete_mon(char *mon_name, my_id *my_idp);
static void insert_mon(mon *monp);
static void pr_mon(char *);
static int statd_call_lockd(mon *monp, int state);
static struct ifconf *getmyaddrs(void);
static int my_name_check(char *hostname);
static int hostname_eq(char *host1, char *host2);
static char * get_system_id(char *hostname);
static void add_aliases(struct hostent *phost);
static void *thr_send_notice(void *);
static void delete_onemon(char *mon_name, my_id *my_idp,
				mon_entry **monitor_q);
static void send_notice(char *mon_name, int state);
static int in_host_array(char *host);

/* ARGSUSED */
void
sm_status(namep, resp)
	sm_name *namep;
	sm_stat_res *resp;
{

	if (debug)
		(void) printf("proc sm_stat: mon_name = %s\n",
				namep->mon_name);

	/* fake answer */
	resp->res_stat = stat_fail;
	resp->state = -1;
}

/* ARGSUSED */
void
sm_mon(monp, resp)
	mon *monp;
	sm_stat_res *resp;
{
	mon_id *monidp;
	monidp = &monp->mon_id;

	rw_rdlock(&thr_rwlock);
	if (debug) {
		(void) printf("proc sm_mon: mon_name = %s, id = %d\n",
		monidp->mon_name, * ((int *)monp->priv));
		pr_mon(monp->mon_id.mon_name);
	}

	/* only monitor other hosts */
	if (my_name_check(monp->mon_id.mon_name) == 0) {
		/* store monitor request into monitor_q */
		insert_mon(monp);
	}

	pr_mon(monp->mon_id.mon_name);
	resp->res_stat = stat_succ;
	resp->state = local_state;
	rw_unlock(&thr_rwlock);
}

/* ARGSUSED */
void
sm_unmon(monidp, resp)
	mon_id *monidp;
	sm_stat *resp;
{
	rw_rdlock(&thr_rwlock);
	if (debug) {
		(void) printf(
			"proc sm_unmon: mon_name = %s, [%s, %d, %d, %d]\n",
			monidp->mon_name, monidp->my_id.my_name,
			monidp->my_id.my_prog, monidp->my_id.my_vers,
			monidp->my_id.my_proc);
		pr_mon(monidp->mon_name);
	}

	delete_mon(monidp->mon_name, &monidp->my_id);
	pr_mon(monidp->mon_name);
	resp->state = local_state;
	rw_unlock(&thr_rwlock);
}

/* ARGSUSED */
void
sm_unmon_all(myidp, resp)
	my_id *myidp;
	sm_stat *resp;
{
	rw_rdlock(&thr_rwlock);
	if (debug)
		(void) printf("proc sm_unmon_all: [%s, %d, %d, %d]\n",
		myidp->my_name,
		myidp->my_prog, myidp->my_vers,
		myidp->my_proc);
	delete_mon((char *)NULL, myidp);
	pr_mon(NULL);
	resp->state = local_state;
	rw_unlock(&thr_rwlock);
}

/*
 * Notifies lockd specified by name that state has changed for this server.
 */
void
sm_notify(ntfp)
	stat_chge *ntfp;
{
	rw_rdlock(&thr_rwlock);
	if (debug)
		(void) printf("sm_notify: %s state =%d\n", ntfp->mon_name,
		    ntfp->state);
	send_notice(ntfp->mon_name, ntfp->state);
	rw_unlock(&thr_rwlock);
}

/* ARGSUSED */
void
sm_simu_crash(myidp)
	void *myidp;
{
	int i;
	struct mon_entry *monitor_q;
	int found;

	/* Only one crash should be running at a time. */
	mutex_lock(&crash_lock);
	if (debug)
		(void) printf("proc sm_simu_crash\n");
	if (in_crash) {
		cond_wait(&crash_finish, &crash_lock);
		mutex_unlock(&crash_lock);
		return;
	} else {
		in_crash = 1;
	}
	mutex_unlock(&crash_lock);

	for (i = 0; i < MAX_HASHSIZE; i++) {
		mutex_lock(&mon_table[i].lock);
		monitor_q = mon_table[i].sm_monhdp;
		if (monitor_q != (struct mon_entry *)NULL) {
			mutex_unlock(&mon_table[i].lock);
			found = 1;
			break;
		}
		mutex_unlock(&mon_table[i].lock);
	}
	/*
	 * If there are entries found in the monitor table,
	 * initiate a crash, else zero out the in_crash variable.
	 */
	if (found) {
		mutex_lock(&crash_lock);
		die = 1;
		/* Signal sm_retry() thread if sleeping. */
		cond_signal(&retrywait);
		mutex_unlock(&crash_lock);
		rw_wrlock(&thr_rwlock);
		sm_crash();
		rw_unlock(&thr_rwlock);
	} else {
		mutex_lock(&crash_lock);
		in_crash = 0;
		mutex_unlock(&crash_lock);
	}
}


/*
 * Insert an entry into the monitor_q.  Space for the entry is allocated
 * here.  It is then filled in from the information passed in.
 */
static void
insert_mon(monp)
	mon *monp;
{
	mon_entry *new, *found;
	my_id *my_idp, *nl_idp;
	mon_entry *monitor_q;
	unsigned int hash;

	/* Allocate entry for new */
	if ((new = (mon_entry *) malloc(sizeof (mon_entry))) == 0) {
		syslog(LOG_ERR,
			"statd: insert_mon: malloc error on mon %s (id=%d)\n",
			monp->mon_id.mon_name, * ((int *)monp->priv));
		return;
	}

	/* Initialize and copy contents of monp to new */
	(void) memset(new, 0, sizeof (mon_entry));
	(void) memcpy(&new->id, monp, sizeof (mon));

	/* Allocate entry for new mon_name */
	if ((new->id.mon_id.mon_name = strdup(monp->mon_id.mon_name)) == 0) {
		syslog(LOG_ERR,
			"statd: insert_mon: malloc error on mon %s (id=%d)\n",
			monp->mon_id.mon_name, * ((int *)monp->priv));
		free(new);
		return;
	}


	/* Allocate entry for new my_name */
	if ((new->id.mon_id.my_id.my_name =
		strdup(monp->mon_id.my_id.my_name)) == 0) {
		syslog(LOG_ERR,
			"statd: insert_mon: malloc error on mon %s (id=%d)\n",
			monp->mon_id.mon_name, * ((int *)monp->priv));
		free(new->id.mon_id.mon_name);
		free(new);
		return;
	}

	if (debug)
		(void) printf("add_mon(%x) %s (id=%d)\n",
		(int) new, new->id.mon_id.mon_name, * ((int *)new->id.priv));


	record_name(new->id.mon_id.mon_name, 1);

	SMHASH(new->id.mon_id.mon_name, hash);
	mutex_lock(&mon_table[hash].lock);
	monitor_q = mon_table[hash].sm_monhdp;

	/* If mon_table hash list is empty. */
	if (monitor_q == (struct mon_entry *)NULL) {
		if (debug)
			(void) printf("\nAdding to monitor_q hash %d\n", hash);
		new->nxt = new->prev = (mon_entry *)NULL;
		mon_table[hash].sm_monhdp = new;
		mutex_unlock(&mon_table[hash].lock);
		return;
	} else {
		found = 0;
		my_idp = &new->id.mon_id.my_id;
		while (monitor_q != (mon_entry *)NULL)  {
			/*
			 * This list is searched sequentially for the
			 * tuple (hostname, prog, vers, proc). The tuples
			 * are inserted in the beginning of the monitor_q,
			 * if the hostname is not already present in the list.
			 * If the hostname is found in the list, the incoming
			 * tuple is inserted just after all the tuples with the
			 * same hostname. However, if the tuple matches exactly
			 * with an entry in the list, space allocated for the
			 * new entry is released and nothing is inserted in the
			 * list.
			 */

			if (str_cmp_unqual_hostname(
				monitor_q->id.mon_id.mon_name,
				new->id.mon_id.mon_name) == 0) {
				/* found */
				nl_idp = &monitor_q->id.mon_id.my_id;
				if ((str_cmp_unqual_hostname(my_idp->my_name,
					nl_idp->my_name) == 0) &&
					my_idp->my_prog == nl_idp->my_prog &&
					my_idp->my_vers == nl_idp->my_vers &&
					my_idp->my_proc == nl_idp->my_proc) {
					/*
					 * already exists an identical one,
					 * release the space allocated for the
					 * mon_entry
					 */
					free(new->id.mon_id.mon_name);
					free(new->id.mon_id.my_id.my_name);
					free(new);
					mutex_unlock(&mon_table[hash].lock);
					return;
				} else {
					/*
					 * mark the last callback that is
					 * not matching; new is inserted
					 * after this
					 */
					found = monitor_q;
				}
			} else if (found)
				break;
			monitor_q = monitor_q->nxt;
		}
		if (found) {
			/*
			 * insert just after the entry having matching tuple.
			 */
			new->nxt = found->nxt;
			new->prev = found;
			if (found->nxt != (mon_entry *)NULL)
				found->nxt->prev = new;
			found->nxt = new;
		} else {
			/*
			 * not found, insert in front of list.
			 */
			new->nxt = mon_table[hash].sm_monhdp;
			new->prev = (mon_entry *) NULL;
			if (new->nxt != (mon_entry *) NULL)
				new->nxt->prev = new;
			mon_table[hash].sm_monhdp = new;
		}
		mutex_unlock(&mon_table[hash].lock);
		return;
	}
}

/*
 * Deletes a specific monitor name or deletes all monitors with same id
 * in hash table.
 */
static void
delete_mon(mon_name, my_idp)
	char *mon_name;
	my_id *my_idp;
{
	unsigned int hash;

	if (mon_name != (char *)NULL) {
		record_name(mon_name, 0);
		SMHASH(mon_name, hash);
		mutex_lock(&mon_table[hash].lock);
		delete_onemon(mon_name, my_idp, &mon_table[hash].sm_monhdp);
		mutex_unlock(&mon_table[hash].lock);
	} else {
		for (hash = 0; hash < MAX_HASHSIZE; hash++) {
			mutex_lock(&mon_table[hash].lock);
			delete_onemon(mon_name, my_idp,
					&mon_table[hash].sm_monhdp);
			mutex_unlock(&mon_table[hash].lock);
		}
	}
}

/*
 * Deletes a monitor in list.
 * IF mon_name is NULL, delete all mon_names that have the same id,
 * else delete specific monitor.

 */
void
delete_onemon(mon_name, my_idp, monitor_q)
	char *mon_name;
	my_id *my_idp;
	mon_entry **monitor_q;
{

	mon_entry *next, *nl;
	my_id *nl_idp;

	next = *monitor_q;
	while ((nl = next) != (struct mon_entry *)NULL) {
		next = next->nxt;
		if (mon_name == (char *)NULL || (mon_name != (char *)NULL &&
			str_cmp_unqual_hostname(nl->id.mon_id.mon_name,
			mon_name) == 0)) {
			nl_idp = &nl->id.mon_id.my_id;
			if ((str_cmp_unqual_hostname(my_idp->my_name,
					nl_idp->my_name) == 0) &&
				my_idp->my_prog == nl_idp->my_prog &&
				my_idp->my_vers == nl_idp->my_vers &&
				my_idp->my_proc == nl_idp->my_proc) {
				/* found */
				if (debug)
					(void) printf("delete_mon(%x): %s\n",
							(int)nl, mon_name ?
							mon_name : "<NULL>");
				/* if nl is not the first entry on list */
				if (nl->prev != (struct mon_entry *)NULL)
					nl->prev->nxt = nl->nxt;
				else {
					*monitor_q = nl->nxt;
				}
				if (nl->nxt != (struct mon_entry *)NULL)
					nl->nxt->prev = nl->prev;
				free(nl->id.mon_id.mon_name);
				free(nl_idp->my_name);
				free(nl);
			}
		} /* end of if mon */
	}

}
/*
 * Notify lockd of host specified by mon_name that the specified state
 * has changed.
 */
static void
send_notice(mon_name, state)
	char *mon_name;
	int state;
{
	struct mon_entry *next;
	mon_entry *monitor_q;
	unsigned int hash;
	moninfo_t *minfop;
	mon *monp;

	SMHASH(mon_name, hash);
	mutex_lock(&mon_table[hash].lock);
	monitor_q = mon_table[hash].sm_monhdp;

	next = monitor_q;
	while (next != (struct mon_entry *)NULL) {
		if (hostname_eq(next->id.mon_id.mon_name, mon_name)) {
			monp = &next->id;
			/*
			 * Prepare the minfop structure to pass to
			 * thr_create(). This structure is a copy of
			 * mon info and state.
			 */
			if ((minfop =
				(moninfo_t *) xmalloc(sizeof (moninfo_t))) !=
				(moninfo_t *) NULL) {
				(void) memcpy(&minfop->id, monp, sizeof (mon));
				/* Allocate entry for mon_name */
				if ((minfop->id.mon_id.mon_name =
					strdup(monp->mon_id.mon_name)) == 0) {
					syslog(LOG_ERR,
			"statd: send_notice: malloc error on mon %s (id=%d)\n",
						monp->mon_id.mon_name,
						* ((int *)monp->priv));
					free(minfop);
					continue;
				}
				/* Allocate entry for my_name */
				if ((minfop->id.mon_id.my_id.my_name =
				strdup(monp->mon_id.my_id.my_name)) == 0) {
					syslog(LOG_ERR,
			"statd: send_notice: malloc error on mon %s (id=%d)\n",
						monp->mon_id.mon_name,
						* ((int *)monp->priv));
					free(minfop->id.mon_id.mon_name);
					free(minfop);
					continue;
				}
				minfop->state = state;
				/*
				 * Create detached threads to process each host
				 * to notify.  If error, print out msg, free
				 * resources and continue.
				 */
				if (thr_create(NULL, NULL, thr_send_notice,
						(void *)minfop, THR_DETACHED,
						NULL)) {
				    syslog(LOG_ERR,
		"statd: unable to create thread to send_notice to %s.\n",
					mon_name);
				    free(minfop->id.mon_id.mon_name);
				    free(minfop->id.mon_id.my_id.my_name);
				    free(minfop);
				    continue;
				}
			}
		}
		next = next->nxt;
	}
	mutex_unlock(&mon_table[hash].lock);
}

/*
 * Work thread created to do the actual statd_call_lockd
 */
static void *
thr_send_notice(void *arg)
{
	moninfo_t *minfop;

	minfop = (moninfo_t *) arg;

	if (statd_call_lockd(&minfop->id, minfop->state) == -1) {
		if (debug && minfop->id.mon_id.mon_name)
			(void) printf(
		"problem with notifying %s failure, give up\n",
			minfop->id.mon_id.mon_name);
	} else {
		if (debug)
			(void) printf(
		"send_notice: %s, %d notified.\n",
		minfop->id.mon_id.mon_name, minfop->state);
	}

	free(minfop->id.mon_id.mon_name);
	free(minfop->id.mon_id.my_id.my_name);
	free(minfop);

	thr_exit((void *) 0);
}

/*
 * Contact lockd specified by monp.
 */
static int
statd_call_lockd(monp, state)
	mon *monp;
	int state;
{
	enum clnt_stat clnt_stat;
	struct timeval tottimeout;
	struct status stat;
	my_id *my_idp;
	char *mon_name;
	int i, err = 0;
	int rc = 0;
	CLIENT *clnt;

	mon_name = monp->mon_id.mon_name;
	my_idp = &monp->mon_id.my_id;
	(void) memset(&stat, 0, sizeof (struct status));
	stat.mon_name = mon_name;
	stat.state = state;
	for (i = 0; i < 16; i++) {
		stat.priv[i] = monp->priv[i];
	}
	if (debug)
		(void) printf("statd_call_lockd: %s state = %d\n",
			stat.mon_name, stat.state);

	if ((clnt = create_client(my_idp->my_name, my_idp->my_prog,
		my_idp->my_vers)) == (CLIENT *) NULL) {
			return (-1);
	}

	tottimeout.tv_sec = SM_RPC_TIMEOUT;
	tottimeout.tv_usec = 0;
	clnt_stat = clnt_call(clnt, my_idp->my_proc, xdr_status, (char *)&stat,
				xdr_void, NULL, tottimeout);
	if (debug) {
		(void) printf("clnt_stat=%s(%d)\n",
			clnt_sperrno(clnt_stat), clnt_stat);
	}
	if (clnt_stat != (int) RPC_SUCCESS) {
		syslog(LOG_WARNING,
			"statd: cannot talk to statd at %s, %s(%d)\n",
			my_idp->my_name, clnt_sperrno(err), err);
		rc = -1;
	}

	clnt_destroy(clnt);
	return (rc);

}

/*
 * Client handle created.
 */
CLIENT *
create_client(host, prognum, versnum)
	char	*host;
	int	prognum;
	int	versnum;
{
	int		fd;
	struct timeval	timeout;
	CLIENT		*client;
	struct t_info	tinfo;

	if ((client = clnt_create(host, prognum, versnum,
			"netpath")) == NULL) {
		return (NULL);
	}
	(void) CLNT_CONTROL(client, CLGET_FD, (caddr_t)&fd);
	if (t_getinfo(fd, &tinfo) != -1) {
		if (tinfo.servtype == T_CLTS) {
			/*
			 * Set time outs for connectionless case
			 */
			timeout.tv_usec = 0;
			timeout.tv_sec = SM_CLTS_TIMEOUT;
			(void) CLNT_CONTROL(client,
				CLSET_RETRY_TIMEOUT, (caddr_t)&timeout);
		}
	} else
		return (NULL);

	return (client);
}

/*
 * ONLY for debugging.
 * Debug messages which prints out the monitor table information.
 * If name is specified, just print out the hash list corresponding
 * to name, otherwise print out the entire monitor table.
 */
static void
pr_mon(name)
	char *name;
{
	mon_entry *nl;
	int hash;

	if (!debug)
		return;

	/* print all */
	if (name == NULL) {
		for (hash = 0; hash < MAX_HASHSIZE; hash++) {
			mutex_lock(&mon_table[hash].lock);
			nl = mon_table[hash].sm_monhdp;
			if (nl == (struct mon_entry *)NULL) {
				(void) printf(
					"*****monitor_q = NULL hash %d\n",
					hash);
				mutex_unlock(&mon_table[hash].lock);
				continue;
			}
			(void) printf("*****monitor_q:\n ");
			while (nl != (mon_entry *)NULL) {
				(void) printf("%s:(%x), ",
					nl->id.mon_id.mon_name, (int)nl);
				nl = nl->nxt;
			}
			mutex_unlock(&mon_table[hash].lock);
			(void) printf("\n");
		}
	} else { /* print one hash list */
		SMHASH(name, hash);
		mutex_lock(&mon_table[hash].lock);
		nl = mon_table[hash].sm_monhdp;
		if (nl == (struct mon_entry *)NULL) {
			(void) printf("*****monitor_q = NULL hash %d\n", hash);
		} else {
			(void) printf("*****monitor_q:\n ");
			while (nl != (mon_entry *)NULL) {
				(void) printf("%s:(%x), ",
					nl->id.mon_id.mon_name, (int)nl);
				nl = nl->nxt;
			}
			(void) printf("\n");
		}
		mutex_unlock(&mon_table[hash].lock);
	}
}


/*
 * create an ifconf structure that represents all the interfaces
 * configured for this host.  Two buffers are allcated here:
 *	ifc - the ifconf structure returned
 *	ifc->ifc_buf - the list of ifreq structures
 * Both of the buffers must be freed by the calling routine.
 * A NULL pointer is returned upon failure.  In this case any
 * data that was allocated before the failure has already been
 * freed.
 */
struct ifconf *
getmyaddrs()
{
	int sock;
	int numifs;
	char *buf;
	struct ifconf *ifc;

	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		syslog(LOG_ERR, "statd:getmyaddrs socket: %m");
		return ((struct ifconf *) NULL);
	}

	if (ioctl(sock, SIOCGIFNUM, (char *)&numifs) < 0) {
		syslog(LOG_ERR,
		"statd:getmyaddrs, get number of interfaces, error: %m");
		numifs = MAXIFS;
	}

	ifc = (struct ifconf *) malloc(sizeof (struct ifconf));
	if (ifc == NULL) {
		syslog(LOG_ERR,
			"statd:getmyaddrs, malloc for ifconf failed: %m");
		(void) close(sock);
		return ((struct ifconf *) NULL);
	}
	buf = (char *) malloc(numifs * sizeof (struct ifreq));
	if (buf == NULL) {
		syslog(LOG_ERR,
			"statd:getmyaddrs, malloc for ifreq failed: %m");
		(void) close(sock);
		free(ifc);
		return ((struct ifconf *) NULL);
	}

	ifc->ifc_buf = buf;
	ifc->ifc_len = numifs * sizeof (struct ifreq);

	if (ioctl(sock, SIOCGIFCONF, (char *) ifc) < 0) {
		syslog(LOG_ERR, "statd:getmyaddrs, SIOCGIFCONF, error: %m");
		(void) close(sock);
		free(buf);
		free(ifc);
		return ((struct ifconf *) NULL);
	}

	(void) close(sock);

	return (ifc);
}

/*
 * Determine if a hostname refers to the machine currently executing
 * this code.  It looks up the address associated with hostname and
 * compares that with the address of each interface configured into
 * the machine.  If there is a match, my_name_check succeeds and
 * returns the value 1.  If there is no match or another error is
 * detected, my_name_check fails and returns the value 0.
 *
 * The routine getmyaddrs() is used to find the addresses assigned
 * to this machine.  If it succeeds, it will allocate data which
 * must be freed before returning.
 *
 * This code has been extracted from the automountd code, and put
 * into the form here. It used to be called self_check, but some
 * folks found the name misleading.
 */
static int
my_name_check(hostname)
	char *hostname;
{
	int n;
	struct sockaddr_in *s1, *s2;
	struct ifreq *ifr;
	struct nd_hostserv hs;
	struct nd_addrlist *retaddrs;
	struct netconfig *nconfp;
	struct ifconf *ifc;

	/*
	 * Get the IP address for hostname
	 */
	nconfp = getnetconfigent(NC_UDP);
	if (nconfp == NULL) {
		syslog(LOG_ERR, "statd: getnetconfigent failed");
		return (0);
	}

	ifc = getmyaddrs();
	if (ifc == (struct ifconf *) NULL) {
		freenetconfigent(nconfp);
		return (0);
	}

	hs.h_host = hostname;
	hs.h_serv = "rpcbind";
	if (netdir_getbyname(nconfp, &hs, &retaddrs) != ND_OK) {
		freenetconfigent(nconfp);
		free(ifc->ifc_buf);
		free(ifc);
		return (0);
	}
	freenetconfigent(nconfp);

	s1 = (struct sockaddr_in *) retaddrs->n_addrs->buf;

	/*
	 * Now compare it against the list of
	 * addresses for the interfaces on this
	 * host.
	 */
	ifr = ifc->ifc_req;
	n = ifc->ifc_len / sizeof (struct ifreq);
	s2 = NULL;
	for (; n > 0; n--, ifr++) {
		if (ifr->ifr_addr.sa_family != AF_INET)
			continue;

		s2 = (struct sockaddr_in *) &ifr->ifr_addr;

		if (memcmp((char *) &s2->sin_addr,
			(char *) &s1->sin_addr,
			sizeof (s2->sin_addr)) == 0) {
			netdir_free((void *) retaddrs, ND_ADDRLIST);
				free(ifc->ifc_buf);
				free(ifc);
				return (1);	/* it's me */
		}
	}

	netdir_free((void *) retaddrs, ND_ADDRLIST);
	free(ifc->ifc_buf);
	free(ifc);
	return (0);			/* it's not me */
}

/*
 * Statd has trouble dealing with hostname aliases because two
 * different aliases for the same machine don't match each other
 * when using strcmp.  To deal with this, the hostnames must be
 * translated into some sort of universal identifier.  These
 * identifiers can be compared.  Universal network addresses are
 * currently used for this identifier because it is general and
 * easy to do.  Other schemes are possible and this routine
 * could be converted if required.
 *
 * If it can't find an address for some reason, 0 is returned.
 */
static int
hostname_eq(char *host1, char *host2)
{
	char * sysid1;
	char * sysid2;
	int rv;

	sysid1 = get_system_id(host1);
	sysid2 = get_system_id(host2);
	if ((sysid1 == NULL) || (sysid2 == NULL))
		rv = 0;
	else
		rv = (strcmp(sysid1, sysid2) == 0);
	free(sysid1);
	free(sysid2);
	return (rv);
}

/*
 * Convert a hostname character string into its network address.
 * A network address is found by searching through all the entries
 * in /etc/netconfig and doing a netdir_getbyname() for each inet
 * entry found.  The netbuf structure returned is converted into
 * a universal address format.
 *
 * If a NULL hostname is given, then the name of the current host
 * is used.  If the hostname doesn't map to an address, a NULL
 * pointer is returned.
 *
 * N.B. the character string returned is allocated in taddr2uaddr()
 * and should be freed by the caller using free().
 */
static char *
get_system_id(char *hostname)
{
	void *hp;
	struct netconfig *ncp;
	struct nd_hostserv service;
	struct nd_addrlist *addrs;
	char *uaddr;
	int rv;

	if (hostname == NULL)
		service.h_host = HOST_SELF;
	else
		service.h_host = hostname;
	service.h_serv = NULL;
	hp = setnetconfig();
	if (hp == (void *) NULL) {
		return (NULL);
	}
	while ((ncp = getnetconfig(hp)) != (struct netconfig *) NULL) {
		if (strcmp(ncp->nc_protofmly, NC_INET) != 0) {
			continue;
		}
		rv = netdir_getbyname(ncp, &service, &addrs);
		if (rv != 0) {
			continue;
		}
		if (addrs) {
			uaddr = taddr2uaddr(ncp, addrs->n_addrs);
			netdir_free(&addrs, ND_ADDR);
			endnetconfig(hp);
			return (uaddr);
		} else {
			netdir_free(&addrs, ND_ADDR);
		}
	}
	endnetconfig(hp);
	return (NULL);
}

void
merge_hosts(void)
{
	struct ifconf *ifc = NULL;
	int sock = -1;
	struct ifreq *ifrp;
	struct ifreq ifr;
	int n;
	struct sockaddr_in *sin;
	int af = AF_INET;
	int rv;
	struct hostent *phost, ahost;
	char host_data[1024];

	/*
	 * This function will enumerate all the interfaces for
	 * this platform, then get the hostent for each i/f.
	 * With the hostent structure, we can get all of the
	 * aliases for the i/f. Then we'll merge all the aliases
	 * with the existing host_name[] list to come up with
	 * all of the known names for each interface. This solves
	 * the problem of a multi-homed host not knowing which
	 * name to publish when statd is started. All the aliases
	 * will be stored in the array, host_name.
	 *
	 * NOTE: Even though we will use all of the aliases we
	 * can get from the i/f hostent, the receiving statd
	 * will still need to handle aliases with hostname_eq.
	 * This is because the sender's aliases may not match
	 * those of the receiver.
	 */
	ifc = getmyaddrs();
	if (ifc == (struct ifconf *) NULL) {
		goto finish;
	}
	sock = socket(af, SOCK_DGRAM, 0);
	if (sock == -1) {
		syslog(LOG_ERR, "statd: socket failed\n");
		goto finish;
	}
	ifrp = ifc->ifc_req;
	for (n = ifc->ifc_len / sizeof (struct ifreq); n > 0; n--, ifrp++) {

		(void) strncpy(ifr.ifr_name, ifrp->ifr_name,
				sizeof (ifr.ifr_name));

		/* If it's the loopback interface, ignore */
		if (ioctl(sock, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
			syslog(LOG_ERR,
				"statd: SIOCGIFFLAGS failed, error: %m\n");
			goto finish;
		}
		if (ifr.ifr_flags & IFF_LOOPBACK)
			continue;

		if (ioctl(sock, SIOCGIFADDR, (caddr_t)&ifr) < 0) {
			syslog(LOG_ERR,
				"statd: SIOCGIFADDR failed, error: %m\n");
			goto finish;
		}
		sin = (struct sockaddr_in *)&ifr.ifr_addr;

		phost = gethostbyaddr_r((char *)&(sin->sin_addr),
						sizeof (struct in_addr), af,
						&ahost, host_data, 1024, &rv);
		if (phost)
			add_aliases(phost);
	}
	/*
	 * Now, just in case we didn't get them all byaddr,
	 * let's look by name.
	 */
	phost = gethostbyname_r(hostname, &ahost, host_data,
		1024, &rv);

	if (phost)
		add_aliases(phost);

finish:
	if (sock != -1)
		(void) close(sock);
	if (ifc) {
		free(ifc->ifc_buf);
		free(ifc);
	}
}

/*
 * add_aliases traverses a hostent alias list, compares
 * the aliases to the contents of host_name, and if an
 * alias is not already present, adds it to host_name[].
 */

static void
add_aliases(struct hostent *phost)
{
	char **aliases;

	if (!in_host_array(phost->h_name)) {
		if ((host_name[addrix] = strdup(phost->h_name)) != NULL)
			addrix++;
	}

	for (aliases = phost->h_aliases; *aliases != NULL; aliases++) {
		if (!in_host_array(*aliases)) {
			if ((host_name[addrix] = strdup(*aliases)) != NULL)
				addrix++;
		}
	}
}

/*
 * in_host_array checks if the given hostname exists in the host_name
 * array. Returns 0 if the host doesn't exist, and 1 if it does exist
 */
static int
in_host_array(char *host)
{
	int i;

	if (debug)
		(void) printf("%s ", host);

	/* Make sure we don't overrun host_name. */
	if (addrix >= MAXIPADDRS)
		return (1);

	if ((strcmp(hostname, host) == 0) || (strcmp(LOGHOST, host) == 0))
		return (1);

	for (i = 0; i < addrix; i++) {
		if (strcmp(host_name[i], host) == 0)
			return (1);
	}

	return (0);
}

/*
 * Compares the unqualified hostnames for hosts. Returns 0 if the
 * names match, and 1 if the names fail to match.
 */
int
str_cmp_unqual_hostname(char *rawname1, char *rawname2)
{
	size_t unq_len1, unq_len2;

	unq_len1 = strcspn(rawname1, ".");
	unq_len2 = strcspn(rawname2, ".");

	if ((unq_len1 == unq_len2) &&
			(strncmp(rawname1, rawname2, unq_len1) == 0))
		return (0);

	return (1);
}
