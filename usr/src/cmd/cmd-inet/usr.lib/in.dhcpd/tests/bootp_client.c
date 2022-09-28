#ident	"@(#)bootp_client.c	1.4	96/07/08 SMI"

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>

struct bootp {
	u_char	op;		/* message opcode */
	u_char	htype;		/* Hardware address type */
	u_char	hlen;		/* Hardware address length */
	u_char	hops;		/* Used by relay agents */
	u_long	xid;		/* transaction id */
	u_short	secs;		/* Secs elapsed since client boot */
	u_short	flags;		/* DHCP Flags field */
	struct in_addr	ciaddr;	/* client IP addr */
	struct in_addr	yiaddr;	/* 'Your' IP addr. (from server) */
	struct in_addr	siaddr;	/* Boot server IP addr */
	struct in_addr	giaddr;	/* Relay agent IP addr */
	u_char	chaddr[16];	/* Client hardware addr */
	u_char	sname[64];	/* Optl. boot server hostname */
	u_char	file[128];	/* boot file name (ascii path) */
	u_char	cookie[4];	/* magic cookie */
	u_char	options[60];	/* Options */
};

int
main(int argc, char *argv[])
{
	static struct bootp request;
	register int try, count, i, numtries;
	struct sockaddr_in	to, from;
	int s, sockoptbuf;
	time_t	start_time = time(NULL);
	register char *endp, *octet;
	u_int buf;

	if (argc != 4) {
		fprintf(stderr, "%s <ether_addr> <retrans> <trys>\n",
		    argv[0]);
		return (1);
	}

	try = atoi(argv[2]);
	numtries = atoi(argv[3]);

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("Socket");
		return (1);
	}

	if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, (char *)&sockoptbuf,
	    (int)sizeof (sockoptbuf)) < 0) {
		perror("Setsockopt");
		return (2);
	}

	from.sin_family = AF_INET;
	from.sin_addr.s_addr = INADDR_ANY;
	from.sin_port = htons(68);
	if (bind(s, (struct sockaddr *)&from, sizeof (from)) < 0) {
		perror("Bind");
		return (3);
	}

	request.op = 1;		/* BOOTP request */
	request.htype = 1;	/* Ethernet */
	request.hlen = 6;	/* Ethernet addr len */

	endp = octet = argv[1];
	for (i = 0; i < (int)request.hlen && octet != NULL; i++) {
		if ((endp = (char *)strchr(endp, ':')) != NULL)
			*endp++ = '\0';
		(void) sscanf(octet, "%x", &buf);
		request.chaddr[i] = (u_char)buf;
		octet = endp;
	}

	/* magic cookie */
	request.cookie[0] = 99;
	request.cookie[1] = 130;
	request.cookie[2] = 83;
	request.cookie[3] = 99;

	to.sin_addr.s_addr = INADDR_BROADCAST;
	to.sin_port = htons(67);

	for (count = 0; count < numtries; try *= 2, count++) {
		(void) fprintf(stdout, "Sending BOOTP request...\n");
		request.secs = (u_short)(time(NULL) - start_time);
		request.xid = request.secs ^ 2;
		if (sendto(s, (char *)&request, sizeof (struct bootp), 0,
		    (struct sockaddr *)&to, sizeof (struct sockaddr)) < 0) {
			perror("Sendto");
			return (4);
		}
		if (try > 64)
			try = 64;
		sleep(try);
	}
	return (0);
}
