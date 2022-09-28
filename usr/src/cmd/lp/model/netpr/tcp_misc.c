
/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)tcp_misc.c 1.15     96/04/26 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <libintl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "netpr.h"
#include "netdebug.h"

np_tcpjob_t *
create_tcp_job(np_job_t * genjob, int filesize)
{
	np_tcpjob_t * tcpjob;

	if (genjob == NULL)
		return (NULL);

	tcpjob = (np_tcpjob_t *)malloc(sizeof (np_tcpjob_t));
	ASSERT(tcpjob, MALLOC_ERR);
	(void) memset(tcpjob, 0, sizeof (np_tcpjob_t));

	tcpjob->gen_data = genjob;
	tcpjob->gen_data->filesize = filesize;

	return (tcpjob);
}

int
tcp_open(char * dest, np_tcpjob_t * tcpjob, int timeout)
{
	struct hostent *hp;
	struct sockaddr_in serv_addr;
	int	s,
		lport = IPPORT_RESERVED - 1,
		err;
	unsigned timo = 1;

	/*
	 * Get the host address and port number to connect to.
	 */
	if (dest == NULL) {
		return (-1);
	}

	if ((hp = (gethostbyname(dest))) == NULL) {
		(void) fprintf(stderr,
		gettext("Netpr: System call gethostbyname fails\n"));
		syslog(LOG_DEBUG, "System call gethostbyname fails");
		return (-1);
	}

	(void) memset(&serv_addr, 0, sizeof (struct sockaddr_in));
	bcopy(hp->h_addr, (caddr_t)&serv_addr.sin_addr, hp->h_length);
	serv_addr.sin_family = hp->h_addrtype;
	serv_addr.sin_port = (int)htons(atoi(tcpjob->np_port));

	/*
	 * Try connecting to the server.
	 */
retry:
	s = rresvport(&lport);
	if (s < 0) {
		(void) fprintf(stderr,
		gettext("Netpr: System call rresvport fails\n"));
		syslog(LOG_DEBUG, "System call rresvport fails");
		return (-1);
	}

	(void) signal(SIGALRM, null_sighandler);
	(void) alarm(timeout);
	if (connect(s, (struct sockaddr *)&serv_addr, sizeof (serv_addr)) < 0) {
		(void) alarm(0);
		err = errno;
		(void) close(s);
		errno = err;
		if (errno == EADDRINUSE) {
			lport--;
			goto retry;
		}
		if (errno == ECONNREFUSED && timo <= 16) {
			(void) sleep(timo);
			timo *= 2;
			goto retry;
		}
		(void) fprintf(stderr,
			gettext("Netpr: Cannot connect to printer\n"));
			syslog(LOG_DEBUG, "Cannot connect to printer");
		return (-1);
	}
	(void) alarm(0);
	return (s);
}

int
tcp_print(int sockfd, caddr_t pa, np_tcpjob_t * tcpjob)
{
	char c;
	int xfer;
	char buf[BUFSIZ + 1];
	int nr = 0;
	int ctr = 0;
	int msg_given = 0;

	if ((xfer = xfer_file(sockfd, pa,
		tcpjob->gen_data->filesize, tcpjob->gen_data->timeout)) < 0) {
			return (xfer);
	}

	if ((shutdown(sockfd, 1)) != 0) {
		(void) fprintf(stderr,
		gettext("Netpr: System error: possible loss of data\n"));
			syslog(LOG_DEBUG,
				"shutdown error; possible loss of data");
		return (E_SYSTEM_ERROR);
	}


	/* read in single character ack or msg from printer */

	(void) memset(buf, 0, BUFSIZ + 1);
	while (ctr < BUFSIZ) {
		(void) signal(SIGALRM, null_sighandler);
		(void) alarm(2);
		errno = 0;
		nr = read(sockfd, &c, 1);
		(void) alarm(0);
		if (errno == EINTR) {
			if (msg_given == 0) {
				tell_lptell(ERRORMSG,
				gettext("Printer not responding;" \
				"Either warming up or needs attention\n"));
				msg_given++;
			}
		} else {
			if ((buf[ctr++] = c) == '\n' || (nr == 0))
				break;
		}
	}
	if (ctr > 1)
		syslog(LOG_DEBUG, "Message from tcp printer on read: %s",
			buf);

	if (msg_given) {
		(void) fprintf(stderr, gettext("Printer ok\n"));
		tell_lptell(OKMSG, "Current");
	}

	return (E_SUCCESS);
}
