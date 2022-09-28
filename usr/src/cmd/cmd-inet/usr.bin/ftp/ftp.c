/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)ftp.c	1.22	96/07/02 SMI"	/* SVr4.0 1.6	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986,1987,1988,1989,1996  Sun Microsystems, Inc.
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	All rights reserved.
 *
 */


#include "ftp_var.h"

static struct	sockaddr_in remctladdr;
static struct	sockaddr_in data_addr;
int	data = -1;
static int	abrtflag = 0;
static int	ptflag = 0;
int		connected;
static int	socksize = 24 * 1024;
static struct	sockaddr_in myctladdr;
static jmp_buf	sendabort;
static jmp_buf	recvabort;
static jmp_buf ptabort;
static int ptabflg;

FILE	*cin, *cout;

static void abortsend(int sig);
static void abortpt(int sig);
static void proxtrans(char *cmd, char *local, char *remote);
static void cmdabort(int sig);
static int empty(struct fd_set *mask, int sec);
static void abortrecv(int sig);
static int initconn(void);
static FILE *dataconn(char *mode);
static void ptransfer(char *direction, off_t bytes, struct timeval *t0,
    struct timeval *t1, char *local, char *remote);
static void tvsub(struct timeval *tdiff, struct timeval *t1,
    struct timeval *t0);
static void psabort(int sig);
static char *gunique(char *local);

char *
hookup(char *host, u_short port)
{
	register struct hostent *hp = 0;
	int s, len;
	static char hostnamebuf[80];

	bzero(&remctladdr, sizeof (remctladdr));
	remctladdr.sin_addr.s_addr = inet_addr(host);
	if (remctladdr.sin_addr.s_addr != (unsigned long)-1) {
		remctladdr.sin_family = AF_INET;
		(void) strncpy(hostnamebuf, host, sizeof (hostnamebuf));
		if (strlen(host) >= sizeof (hostnamebuf)) {
			printf("host too long (host %s, len %d)\n",
			    host, strlen(hostnamebuf));
		}
	} else {
		hp = gethostbyname(host);
		if (hp == NULL) {
			printf("%s: unknown host\n", host);
			code = -1;
			return ((char *)0);
		}
		remctladdr.sin_family = hp->h_addrtype;
#ifdef h_addr
		bcopy(hp->h_addr_list[0],
		    (caddr_t)&remctladdr.sin_addr, hp->h_length);
#else /* h_addr */
		bcopy(hp->h_addr, (caddr_t)&remctladdr.sin_addr, hp->h_length);
#endif /* h_addr */
		(void) strcpy(hostnamebuf, hp->h_name);
	}
	hostname = hostnamebuf;
	s = socket(remctladdr.sin_family, SOCK_STREAM, 0);
	if (s < 0) {
		perror("ftp: socket");
		code = -1;
		return (0);
	}
	remctladdr.sin_port = port;
#ifdef h_addr
	while (connect(s, (struct sockaddr *)&remctladdr,
	    sizeof (remctladdr)) < 0) {
		if (hp && hp->h_addr_list[1]) {
			int oerrno = errno;

			fprintf(stderr, "ftp: connect to address %s: ",
				inet_ntoa(remctladdr.sin_addr));
			errno = oerrno;
			perror((char *)0);
			hp->h_addr_list++;
			bcopy(hp->h_addr_list[0],
			    &remctladdr.sin_addr, hp->h_length);
			fprintf(stdout, "Trying %s...\n",
				inet_ntoa(remctladdr.sin_addr));
			(void) close(s);
			s = socket(remctladdr.sin_family, SOCK_STREAM, 0);
			if (s < 0) {
				perror("ftp: socket");
				code = -1;
				return (0);
			}
			continue;
		}
#endif h_addr
		perror("ftp: connect");
		code = -1;
		goto bad;
	}
	len = sizeof (myctladdr);
	if (getsockname(s, (struct sockaddr *)&myctladdr, &len) < 0) {
		perror("ftp: getsockname");
		code = -1;
		goto bad;
	}
	cin = fdopen(s, "r");
	cout = fdopen(s, "w");
	if (cin == NULL || cout == NULL) {
		fprintf(stderr, "ftp: fdopen failed.\n");
		if (cin)
			(void) fclose(cin);
		if (cout)
			(void) fclose(cout);
		code = -1;
		goto bad;
	}
	if (verbose)
		printf("Connected to %s.\n", hostname);
	if (getreply(0) > 2) { 	/* read startup message from server */
		if (cin)
			(void) fclose(cin);
		if (cout)
			(void) fclose(cout);
		code = -1;
		goto bad;
	}
#ifdef SO_OOBINLINE
	{
	int on = 1;

	if (setsockopt(s, SOL_SOCKET, SO_OOBINLINE, (char *)&on,
	    sizeof (on)) < 0 && debug) {
			perror("ftp: setsockopt (SO_OOBINLINE)");
		}
	}
#endif SO_OOBINLINE

	return (hostname);
bad:
	(void) close(s);
	return ((char *)0);
}

int
login(char *host)
{
	char tmp[80];
	char *user, *pass, *acct;
	int n, aflag = 0;

	user = pass = acct = 0;
	if (ruserpass(host, &user, &pass, &acct) < 0) {
		disconnect(0, NULL);
		code = -1;
		return (0);
	}
	if (user == NULL) {
		char *myname = getlogin();

		if (myname == NULL) {
			struct passwd *pp = getpwuid(getuid());

			if (pp != NULL)
				myname = pp->pw_name;
		}
		printf("Name (%s:%s): ", host, (myname == NULL) ? "" : myname);
		(void) fgets(tmp, sizeof (tmp) - 1, stdin);
		tmp[strlen(tmp) - 1] = '\0';
		if (*tmp == '\0' && myname != NULL)
			user = myname;
		else
			user = tmp;
	}
	n = command("USER %s", user);
	if (n == CONTINUE) {
		if (pass == NULL)
			pass = mygetpass("Password:");
		n = command("PASS %s", pass);
	}
	if (n == CONTINUE) {
		aflag++;
		if (acct == NULL)
			acct = mygetpass("Account:");
		n = command("ACCT %s", acct);
	}
	if (n != COMPLETE) {
		fprintf(stderr, "Login failed.\n");
		return (0);
	}
	if (!aflag && acct != NULL)
		(void) command("ACCT %s", acct);
	if (proxy)
		return (1);
	for (n = 0; n < macnum; ++n) {
		if (strcmp("init", macros[n].mac_name) == 0) {
			(void) strcpy(line, "$init");
			makeargv();
			domacro(margc, margv);
			break;
		}
	}
	return (1);
}

/*ARGSUSED*/
static void
cmdabort(int sig)
{
	printf("\n");
	(void) fflush(stdout);
	abrtflag++;
	if (ptflag)
		longjmp(ptabort, 1);
}

int
command(char *fmt, ...)
{
	int r;
	void (*oldintr)();
	va_list ap;

	va_start(ap, fmt);
	abrtflag = 0;
	if (debug) {
		printf("---> ");
		vfprintf(stdout, fmt, ap);
		printf("\n");
		(void) fflush(stdout);
	}
	if (cout == NULL) {
		perror("No control connection for command");
		code = -1;
		return (0);
	}
	oldintr = signal(SIGINT, cmdabort);
	vfprintf(cout, fmt, ap);
	fprintf(cout, "\r\n");
	(void) fflush(cout);
	va_end(ap);
	cpend = 1;
	r = getreply(strcmp(fmt, "QUIT") == 0);
	if (abrtflag && oldintr != SIG_IGN)
		(*oldintr)();
	(void) signal(SIGINT, oldintr);
	return (r);
}

int
getreply(int expecteof)
{
	register int c, n;
	register int dig;
	int originalcode = 0, continuation = 0;
	void (*oldintr)();
	int pflag = 0;
	char *pt = pasv;
	int	len;

	oldintr = signal(SIGINT, cmdabort);
	for (;;) {
		dig = n = code = 0;
		while ((c = fgetwc(cin)) != '\n') {
			if (c == IAC) {	/* handle telnet commands */
				switch (c = fgetwc(cin)) {
				case WILL:
				case WONT:
					c = fgetwc(cin);
					fprintf(cout, "%c%c%wc", IAC, WONT, c);
					(void) fflush(cout);
					break;
				case DO:
				case DONT:
					c = fgetwc(cin);
					fprintf(cout, "%c%c%wc", IAC, DONT, c);
					(void) fflush(cout);
					break;
				default:
					break;
				}
				continue;
			}
			dig++;
			if (c == EOF) {
				if (expecteof) {
					(void) signal(SIGINT, oldintr);
					code = 221;
					return (0);
				}
				lostpeer(0);
				if (verbose) {
					printf(
					    "421 Service not available, remote"
					    " server has closed connection\n");
				}
				else
					printf("Lost connection\n");
				(void) fflush(stdout);
				code = 421;
				return (4);
			}
			if (c != '\r' && (verbose > 0 ||
			    (verbose > -1 && n == '5' && dig > 4))) {
				if (proxflag &&
				    (dig == 1 || dig == 5 && verbose == 0))
					printf("%s:", hostname);
				(void) putwchar(c);
			}
			if (dig < 4 && isascii(c) && isdigit(c))
				code = code * 10 + (c - '0');
			if (!pflag && code == 227)
				pflag = 1;
			if (dig > 4 && pflag == 1 && isascii(c) && isdigit(c))
				pflag = 2;
			if (pflag == 2) {
				if (c != '\r' && c != ')') {
					if ((len = wctomb(pt, c)) <= 0) {
						*pt = (unsigned char)c;
						len = 1;
					}
					pt += len;
				} else {
					*pt = '\0';
					pflag = 3;
				}
			}
			if (dig == 4 && c == '-') {
				if (continuation)
					code = 0;
				continuation++;
			}
			if (n == 0)
				n = c;
		}
		if (verbose > 0 || verbose > -1 && n == '5') {
			(void) putwchar(c);
			(void) fflush(stdout);
		}
		if (continuation && code != originalcode) {
			if (originalcode == 0)
				originalcode = code;
			continue;
		}
		if (n != '1')
			cpend = 0;
		(void) signal(SIGINT, oldintr);
		if (code == 421 || originalcode == 421)
			lostpeer(0);
		if (abrtflag && oldintr != cmdabort && oldintr != SIG_IGN)
			(*oldintr)();
		return (n - '0');
	}
}

static int
empty(struct fd_set *mask, int sec)
{
	struct timeval t;

	t.tv_sec = (long)sec;
	t.tv_usec = 0;
	return (select(32, mask, NULL, NULL, &t));
}

/*ARGSUSED*/
static void
abortsend(int sig)
{
	mflag = 0;
	abrtflag = 0;
	printf("\nsend aborted\n");
	(void) fflush(stdout);
	longjmp(sendabort, 1);
}

void
sendrequest(char *cmd, char *local, char *remote)
{
	FILE *fin, *dout = 0;
	int (*closefunc)();
	void (*oldintr)(), (*oldintp)();
	off_t bytes = 0, hashbytes = FTPBUFSIZ;
	register int c, d;
	struct stat st;
	struct timeval start, stop;

	if (proxy) {
		proxtrans(cmd, local, remote);
		return;
	}
	closefunc = NULL;
	oldintr = NULL;
	oldintp = NULL;
	if (setjmp(sendabort)) {
		while (cpend) {
			(void) getreply(0);
		}
		if (data >= 0) {
			(void) close(data);
			data = -1;
		}
		if (oldintr)
			(void) signal(SIGINT, oldintr);
		if (oldintp)
			(void) signal(SIGPIPE, oldintp);
		code = -1;
		return;
	}
	oldintr = signal(SIGINT, abortsend);
	if (strcmp(local, "-") == 0)
		fin = stdin;
	else if (*local == '|') {
		oldintp = signal(SIGPIPE, SIG_IGN);
		fin = mypopen(local + 1, "r");
		if (fin == NULL) {
			perror(local + 1);
			(void) signal(SIGINT, oldintr);
			(void) signal(SIGPIPE, oldintp);
			code = -1;
			return;
		}
		closefunc = mypclose;
	} else {
		fin = fopen(local, "r");
		if (fin == NULL) {
			perror(local);
			(void) signal(SIGINT, oldintr);
			code = -1;
			return;
		}
		closefunc = fclose;
		if (fstat(fileno(fin), &st) < 0 ||
		    (st.st_mode&S_IFMT) != S_IFREG) {
			fprintf(stdout, "%s: not a plain file.\n", local);
			(void) signal(SIGINT, oldintr);
			code = -1;
			fclose(fin);
			return;
		}
	}
	if (initconn()) {
		(void) signal(SIGINT, oldintr);
		if (oldintp)
			(void) signal(SIGPIPE, oldintp);
		code = -1;
		return;
	}
	if (setjmp(sendabort))
		goto abort;
	if (remote) {
		if (command("%s %s", cmd, remote) != PRELIM) {
			(void) signal(SIGINT, oldintr);
			if (oldintp)
				(void) signal(SIGPIPE, oldintp);
			return;
		}
	} else
		if (command("%s", cmd) != PRELIM) {
			(void) signal(SIGINT, oldintr);
			if (oldintp)
				(void) signal(SIGPIPE, oldintp);
			return;
		}
	dout = dataconn("w");
	if (dout == NULL)
		goto abort;
	(void) gettimeofday(&start, (struct timezone *)0);
	switch (type) {

	case TYPE_I:
	case TYPE_L:
		errno = d = 0;
		while ((c = read(fileno(fin), buf, FTPBUFSIZ)) > 0) {
			if ((d = write(fileno(dout), buf, c)) < 0)
				break;
			bytes += c;
			if (hash) {
				while (bytes >= hashbytes) {
					(void) putchar('#');
					hashbytes += FTPBUFSIZ;
				}
				(void) fflush(stdout);
			}
		}
		if (hash && bytes > 0) {
			if (bytes < hashbytes)
				(void) putchar('#');
			(void) putchar('\n');
			(void) fflush(stdout);
		}
		if (c < 0)
			perror(local);
		if (d < 0)
			perror("netout");
		break;

	case TYPE_A:
		while ((c = getc(fin)) != EOF) {
			if (c == '\n') {
				while (hash && (bytes >= hashbytes)) {
					(void) putchar('#');
					(void) fflush(stdout);
					hashbytes += FTPBUFSIZ;
				}
				if (ferror(dout))
					break;
				(void) putc('\r', dout);
				bytes++;
			}
			(void) putc(c, dout);
			bytes++;
#ifdef notdef
			if (c == '\r') {
				(void) putc('\0', dout); /* this violates rfc */
				bytes++;
			}
#endif
		}
		if (hash) {
			if (bytes < hashbytes)
				(void) putchar('#');
			(void) putchar('\n');
			(void) fflush(stdout);
		}
		if (ferror(fin))
			perror(local);
		if (ferror(dout))
			perror("netout");
		break;
	}
	(void) gettimeofday(&stop, (struct timezone *)0);
	if (closefunc != NULL)
		(*closefunc)(fin);
	(void) fclose(dout); data = -1;
	(void) getreply(0);
	(void) signal(SIGINT, oldintr);

        /* 
	 * Only print the transfer successful message if the code returned
	 * from remote is 226 or 250. All other codes are error codes.
	 */
        if ((bytes > 0) && verbose && ((code == 226) || (code == 250)))
		ptransfer("sent", bytes, &start, &stop, local, remote);
	return;
abort:
	(void) gettimeofday(&stop, (struct timezone *)0);
	(void) signal(SIGINT, oldintr);
	if (oldintp)
		(void) signal(SIGPIPE, oldintp);
	if (!cpend) {
		code = -1;
		return;
	}
	if (data >= 0) {
		(void) close(data);
		data = -1;
	}
	if (dout) {
		(void) fclose(dout);
		data = -1;
	}
	(void) getreply(0);
	code = -1;
	if (closefunc != NULL && fin != NULL)
		(*closefunc)(fin);
        /* 
	 * Only print the transfer successful message if the code returned
	 * from remote is 226 or 250. All other codes are error codes.
	 */
        if ((bytes > 0) && verbose && ((code == 226) || (code == 250)))
		ptransfer("sent", bytes, &start, &stop, local, remote);
}

/*ARGSUSED*/
static void
abortrecv(int sig)
{

	mflag = 0;
	abrtflag = 0;
	printf("\n");
	(void) fflush(stdout);
	longjmp(recvabort, 1);
}

void
recvrequest(char *cmd, char *local, char *remote, char *mode)
{
	FILE *fout, *din = 0;
	int (*closefunc)();
	void (*oldintr)(), (*oldintp)();
	int oldverbose, oldtype = 0, tcrflag, nfnd;
	char msg;
	off_t bytes = 0, hashbytes = FTPBUFSIZ;
	struct fd_set mask;
	register int c, d;
	struct timeval start, stop;
        int errflg = 0;

	if (proxy && strcmp(cmd, "RETR") == 0) {
		proxtrans(cmd, local, remote);
		return;
	}
	closefunc = NULL;
	oldintr = NULL;
	oldintp = NULL;
	tcrflag = !crflag && (strcmp(cmd, "RETR") == 0);
	if (setjmp(recvabort)) {
		while (cpend) {
			(void) getreply(0);
		}
		if (data >= 0) {
			(void) close(data);
			data = -1;
		}
		if (oldintr)
			(void) signal(SIGINT, oldintr);
		code = -1;
		return;
	}
	oldintr = signal(SIGINT, abortrecv);
	if (strcmp(local, "-") && *local != '|') {
		if (access(local, 2) < 0) {
			char *dir = rindex(local, '/');

			if (errno != ENOENT && errno != EACCES) {
				perror(local);
				(void) signal(SIGINT, oldintr);
				code = -1;
				return;
			}
			if (dir != NULL)
				*dir = 0;
			d = access(dir ? local : ".", 2);
			if (dir != NULL)
				*dir = '/';
			if (d < 0) {
				perror(local);
				(void) signal(SIGINT, oldintr);
				code = -1;
				return;
			}
			if (!runique && errno == EACCES &&
			    chmod(local, 0600) < 0) {
				perror(local);
				(void) signal(SIGINT, oldintr);
				code = -1;
				return;
			}
			if (runique && errno == EACCES &&
			    (local = gunique(local)) == NULL) {
				(void) signal(SIGINT, oldintr);
				code = -1;
				return;
			}
		} else if (runique && (local = gunique(local)) == NULL) {
			(void) signal(SIGINT, oldintr);
			code = -1;
			return;
		}
	}
	if (initconn()) {
		(void) signal(SIGINT, oldintr);
		code = -1;
		return;
	}
	if (setjmp(recvabort))
		goto abort;
	if (strcmp(cmd, "RETR") && type != TYPE_A) {
		oldtype = type;
		oldverbose = verbose;
		if (!debug)
			verbose = 0;
		setascii(0, NULL);
		verbose = oldverbose;
	}
	if (remote) {
		if (command("%s %s", cmd, remote) != PRELIM) {
			(void) signal(SIGINT, oldintr);
			if (oldtype) {
				if (!debug)
					verbose = 0;
				switch (oldtype) {
					case TYPE_I:
						setbinary(0, NULL);
						break;
					case TYPE_E:
						setebcdic(0, NULL);
						break;
					case TYPE_L:
						settenex(0, NULL);
						break;
				}
				verbose = oldverbose;
			}
			return;
		}
	} else {
		if (command("%s", cmd) != PRELIM) {
			(void) signal(SIGINT, oldintr);
			if (oldtype) {
				if (!debug)
					verbose = 0;
				switch (oldtype) {
					case TYPE_I:
						setbinary(0, NULL);
						break;
					case TYPE_E:
						setebcdic(0, NULL);
						break;
					case TYPE_L:
						settenex(0, NULL);
						break;
				}
				verbose = oldverbose;
			}
			return;
		}
	}
	din = dataconn("r");
	if (din == NULL)
		goto abort;
	if (strcmp(local, "-") == 0)
		fout = stdout;
	else if (*local == '|') {
		oldintp = signal(SIGPIPE, SIG_IGN);
		fout = mypopen(local + 1, "w");
		if (fout == NULL) {
			perror(local+1);
			goto abort;
		}
		closefunc = mypclose;
	} else {
		fout = fopen(local, mode);
		if (fout == NULL) {
			perror(local);
			goto abort;
		}
		closefunc = fclose;
	}
	(void) gettimeofday(&start, (struct timezone *)0);
	switch (type) {

	case TYPE_I:
	case TYPE_L:
		errno = d = 0;
		while ((c = read(fileno(din), buf, FTPBUFSIZ)) > 0) {
			if ((d = write(fileno(fout), buf, c)) != c)
				goto writeerr;
			bytes += c;
			if (hash) {
				while (bytes >= hashbytes) {
					(void) putchar('#');
					hashbytes += FTPBUFSIZ;
				}
				(void) fflush(stdout);
			}
		}
		if (hash && bytes > 0) {
			if (bytes < hashbytes)
				(void) putchar('#');
			(void) putchar('\n');
			(void) fflush(stdout);
		}
		if (c < 0) {
			errflg = 1;
			perror("netin");
		}
		if ((d < 0) || ((c == 0) && (fsync(fileno(fout)) == -1))) {
writeerr:
			errflg = 1;
			perror(local);
		}
		break;

	case TYPE_A:
		while ((c = getc(din)) != EOF) {
			while (c == '\r') {
				while (hash && (bytes >= hashbytes)) {
					(void) putchar('#');
					(void) fflush(stdout);
					hashbytes += FTPBUFSIZ;
				}
				bytes++;
				if ((c = getc(din)) != '\n' || tcrflag) {
					if (ferror(fout))
						break;
					if (putc('\r', fout) == EOF)
						goto writer_ascii_err;
				}
#ifdef notdef
				if (c == '\0') {
					bytes++;
					continue;
				}
#endif
			}
			if (putc(c, fout) == EOF)
				goto writer_ascii_err;
			bytes++;
		}
		if (hash) {
			if (bytes < hashbytes)
				(void) putchar('#');
			(void) putchar('\n');
			(void) fflush(stdout);
		}
		if (ferror(din)) {
			errflg = 1;
			perror("netin");
		}
		if ((fflush(fout) == EOF) || ferror(fout) ||
			(fsync(fileno(fout)) == -1)) {
writer_ascii_err:
			errflg = 1;
			perror(local);
		}
		break;
	}
	if (closefunc != NULL)
		(*closefunc)(fout);
	(void) signal(SIGINT, oldintr);
	if (oldintp)
		(void) signal(SIGPIPE, oldintp);
	(void) gettimeofday(&stop, (struct timezone *)0);
	(void) fclose(din); data = -1;
	(void) getreply(0);
	if (bytes > 0 && verbose && !errflg)
		ptransfer("received", bytes, &start, &stop, local, remote);
	if (oldtype) {
		if (!debug)
			verbose = 0;
		switch (oldtype) {
			case TYPE_I:
				setbinary(0, NULL);
				break;
			case TYPE_E:
				setebcdic(0, NULL);
				break;
			case TYPE_L:
				settenex(0, NULL);
				break;
		}
		verbose = oldverbose;
	}
	return;
abort:

/* abort using RFC959 recommended IP, SYNC sequence  */

	(void) gettimeofday(&stop, (struct timezone *)0);
	if (oldintp)
		(void) signal(SIGPIPE, oldintr);
	(void) signal(SIGINT, SIG_IGN);
	if (oldtype) {
		if (!debug)
			verbose = 0;
		switch (oldtype) {
			case TYPE_I:
				setbinary(0, NULL);
				break;
			case TYPE_E:
				setebcdic(0, NULL);
				break;
			case TYPE_L:
				settenex(0, NULL);
				break;
		}
		verbose = oldverbose;
	}
	if (!cpend) {
		code = -1;
		(void) signal(SIGINT, oldintr);
		return;
	}

	fprintf(cout, "%c%c", IAC, IP);
	(void) fflush(cout);
	msg = (char)IAC;
	/*
	 * send IAC in urgent mode instead of DM because UNIX places oob
	 * mark after urgent byte rather than before as now is protocol
	 */
	if (send(fileno(cout), &msg, 1, MSG_OOB) != 1) {
		perror("abort");
	}
	fprintf(cout, "%cABOR\r\n", DM);
	(void) fflush(cout);
	FD_ZERO(&mask);
	FD_SET(fileno(cin), &mask);
	if (din) {
		FD_SET(fileno(din), &mask);
	}
	if ((nfnd = empty(&mask, 10)) <= 0) {
		if (nfnd < 0) {
			perror("abort");
		}
		code = -1;
		lostpeer(0);
	}
	if (din && FD_ISSET(fileno(din), &mask)) {
		while ((c = read(fileno(din), buf, FTPBUFSIZ)) > 0)
			;
	}
	if ((c = getreply(0)) == ERROR && code == 552) {
		/* needed for nic style abort */
		if (data >= 0) {
			(void) close(data);
			data = -1;
		}
		(void) getreply(0);
	}
	(void) getreply(0);
	code = -1;
	if (data >= 0) {
		(void) close(data);
		data = -1;
	}
	if (closefunc != NULL && fout != NULL)
		(*closefunc)(fout);
	if (din) {
		(void) fclose(din);
		data = -1;
	}
	if (bytes > 0 && verbose)
		ptransfer("received", bytes, &start, &stop, local, remote);
	(void) signal(SIGINT, oldintr);
}

/*
 * Need to start a listen on the data channel
 * before we send the command, otherwise the
 * server's connect may fail.
 */

static int
initconn(void)
{
	unsigned char *p, *a;
	int result, len, tmpno = 0;
	int on = 1;

noport:
	data_addr = myctladdr;
	if (sendport)
		data_addr.sin_port = 0;	/* let system pick one */
	if (data != -1)
		(void) close(data);
	data = socket(AF_INET, SOCK_STREAM, 0);
	if (data < 0) {
		perror("ftp: socket");
		if (tmpno)
			sendport = 1;
		return (1);
	}
	if (!sendport)
		if (setsockopt(data, SOL_SOCKET, SO_REUSEADDR,
		    (char *)&on, sizeof (on)) < 0) {
			perror("ftp: setsockopt (SO_REUSEADDR)");
			goto bad;
		}
	if (bind(data,
	    (struct sockaddr *)&data_addr, sizeof (data_addr)) < 0) {
		perror("ftp: bind");
		goto bad;
	}
	if (options & SO_DEBUG &&
	    setsockopt(data, SOL_SOCKET, SO_DEBUG,
	    (char *)&on, sizeof (on)) < 0)
		perror("ftp: setsockopt (SO_DEBUG - ignored)");
	if (setsockopt(data, SOL_SOCKET, SO_SNDBUF, (char *)&socksize,
				sizeof (socksize)) < 0)
		perror("ftp: setsockopt (SO_SNDBUF - ignored)");
	if (setsockopt(data, SOL_SOCKET, SO_RCVBUF, (char *)&socksize,
				sizeof (socksize)) < 0)
		perror("ftp: setsockopt (SO_RCVBUF - ignored)");
	len = sizeof (data_addr);
	if (getsockname(data, (struct sockaddr *)&data_addr, &len) < 0) {
		perror("ftp: getsockname");
		goto bad;
	}
	if (listen(data, 1) < 0)
		perror("ftp: listen");
	if (sendport) {
		a = (unsigned char *)&data_addr.sin_addr;
		p = (unsigned char *)&data_addr.sin_port;
#define	UC(b)	((b)&0xff)
		result =
		    command("PORT %d,%d,%d,%d,%d,%d",
			UC(a[0]), UC(a[1]), UC(a[2]), UC(a[3]),
			UC(p[0]), UC(p[1]));
		if (result == ERROR && sendport == -1) {
			sendport = 0;
			tmpno = 1;
			goto noport;
		}
		return (result != COMPLETE);
	}
	if (tmpno)
		sendport = 1;
	return (0);
bad:
	(void) close(data), data = -1;
	if (tmpno)
		sendport = 1;
	return (1);
}

static FILE *
dataconn(char *mode)
{
	struct sockaddr_in from;
	int s, fromlen = sizeof (from);

	s = accept(data, (struct sockaddr *)&from, &fromlen);
	if (s < 0) {
		perror("ftp: accept");
		(void) close(data), data = -1;
		return (NULL);
	}
	(void) close(data);
	data = s;
	return (fdopen(data, mode));
}

static void
ptransfer(char *direction, off_t bytes, struct timeval *t0,
    struct timeval *t1, char *local, char *remote)
{
	struct timeval td;
	float s, bs;

	tvsub(&td, t1, t0);
	s = td.tv_sec + (td.tv_usec / 1000000.);
#define	nz(x)	((x) == 0 ? 1 : (x))
	bs = bytes / nz(s);
	if (local && *local != '-')
		printf("local: %s ", local);
	if (remote)
		printf("remote: %s\n", remote);
	printf("%lld bytes %s in %.2g seconds (%.2f Kbytes/s)\n",
		(longlong_t) bytes, direction, s, bs / 1024.);
}

static void
tvsub(struct timeval *tdiff, struct timeval *t1, struct timeval *t0)
{

	tdiff->tv_sec = t1->tv_sec - t0->tv_sec;
	tdiff->tv_usec = t1->tv_usec - t0->tv_usec;
	if (tdiff->tv_usec < 0)
		tdiff->tv_sec--, tdiff->tv_usec += 1000000;
}


/*ARGSUSED*/
static void
psabort(int sig)
{
	abrtflag++;
}

void
pswitch(int flag)
{
	void (*oldintr)();
	static struct comvars {
		int connect;
		char name[MAXHOSTNAMELEN];
		struct sockaddr_in mctl;
		struct sockaddr_in hctl;
		FILE *in;
		FILE *out;
		int tpe;
		int cpnd;
		int sunqe;
		int runqe;
		int mcse;
		int ntflg;
		char nti[17];
		char nto[17];
		int mapflg;
		char mi[MAXPATHLEN];
		char mo[MAXPATHLEN];
		} proxstruct, tmpstruct;
	struct comvars *ip, *op;

	abrtflag = 0;
	oldintr = signal(SIGINT, psabort);
	if (flag) {
		if (proxy)
			return;
		ip = &tmpstruct;
		op = &proxstruct;
		proxy++;
	} else {
		if (!proxy)
			return;
		ip = &proxstruct;
		op = &tmpstruct;
		proxy = 0;
	}
	ip->connect = connected;
	connected = op->connect;
	if (hostname) {
		(void) strncpy(ip->name, hostname, sizeof (ip->name) - 1);
		ip->name[strlen(ip->name)] = '\0';
	} else
		ip->name[0] = 0;
	hostname = op->name;
	ip->hctl = remctladdr;
	remctladdr = op->hctl;
	ip->mctl = myctladdr;
	myctladdr = op->mctl;
	ip->in = cin;
	cin = op->in;
	ip->out = cout;
	cout = op->out;
	ip->tpe = type;
	type = op->tpe;
	if (!type)
		type = 1;
	ip->cpnd = cpend;
	cpend = op->cpnd;
	ip->sunqe = sunique;
	sunique = op->sunqe;
	ip->runqe = runique;
	runique = op->runqe;
	ip->mcse = mcase;
	mcase = op->mcse;
	ip->ntflg = ntflag;
	ntflag = op->ntflg;
	(void) strncpy(ip->nti, ntin, 16);
	(ip->nti)[strlen(ip->nti)] = '\0';
	(void) strcpy(ntin, op->nti);
	(void) strncpy(ip->nto, ntout, 16);
	(ip->nto)[strlen(ip->nto)] = '\0';
	(void) strcpy(ntout, op->nto);
	ip->mapflg = mapflag;
	mapflag = op->mapflg;
	(void) strncpy(ip->mi, mapin, MAXPATHLEN - 1);
	(ip->mi)[strlen(ip->mi)] = '\0';
	(void) strcpy(mapin, op->mi);
	(void) strncpy(ip->mo, mapout, MAXPATHLEN - 1);
	(ip->mo)[strlen(ip->mo)] = '\0';
	(void) strcpy(mapout, op->mo);
	(void) signal(SIGINT, oldintr);
	if (abrtflag) {
		abrtflag = 0;
		(*oldintr)();
	}
}

/*ARGSUSED*/
static void
abortpt(int sig)
{
	printf("\n");
	(void) fflush(stdout);
	ptabflg++;
	mflag = 0;
	abrtflag = 0;
	longjmp(ptabort, 1);
}

static void
proxtrans(char *cmd, char *local, char *remote)
{
	void (*oldintr)();
	int tmptype, oldtype = 0, secndflag = 0, nfnd;
	extern jmp_buf ptabort;
	char *cmd2;
	struct fd_set mask;

	if (strcmp(cmd, "RETR"))
		cmd2 = "RETR";
	else
		cmd2 = runique ? "STOU" : "STOR";
	if (command("PASV") != COMPLETE) {
		printf("proxy server does not support third part transfers.\n");
		return;
	}
	tmptype = type;
	pswitch(0);
	if (!connected) {
		printf("No primary connection\n");
		pswitch(1);
		code = -1;
		return;
	}
	if (type != tmptype) {
		oldtype = type;
		switch (tmptype) {
			case TYPE_A:
				setascii(0, NULL);
				break;
			case TYPE_I:
				setbinary(0, NULL);
				break;
			case TYPE_E:
				setebcdic(0, NULL);
				break;
			case TYPE_L:
				settenex(0, NULL);
				break;
		}
	}
	if (command("PORT %s", pasv) != COMPLETE) {
		switch (oldtype) {
			case 0:
				break;
			case TYPE_A:
				setascii(0, NULL);
				break;
			case TYPE_I:
				setbinary(0, NULL);
				break;
			case TYPE_E:
				setebcdic(0, NULL);
				break;
			case TYPE_L:
				settenex(0, NULL);
				break;
		}
		pswitch(1);
		return;
	}
	if (setjmp(ptabort))
		goto abort;
	oldintr = signal(SIGINT, (void (*)())abortpt);
	if (command("%s %s", cmd, remote) != PRELIM) {
		(void) signal(SIGINT, oldintr);
		switch (oldtype) {
			case 0:
				break;
			case TYPE_A:
				setascii(0, NULL);
				break;
			case TYPE_I:
				setbinary(0, NULL);
				break;
			case TYPE_E:
				setebcdic(0, NULL);
				break;
			case TYPE_L:
				settenex(0, NULL);
				break;
		}
		pswitch(1);
		return;
	}
	sleep(2);
	pswitch(1);
	secndflag++;
	if (command("%s %s", cmd2, local) != PRELIM)
		goto abort;
	ptflag++;
	(void) getreply(0);
	pswitch(0);
	(void) getreply(0);
	(void) signal(SIGINT, oldintr);
	switch (oldtype) {
		case 0:
			break;
		case TYPE_A:
			setascii(0, NULL);
			break;
		case TYPE_I:
			setbinary(0, NULL);
			break;
		case TYPE_E:
			setebcdic(0, NULL);
			break;
		case TYPE_L:
			settenex(0, NULL);
			break;
	}
	pswitch(1);
	ptflag = 0;
	printf("local: %s remote: %s\n", local, remote);
	return;
abort:
	(void) signal(SIGINT, SIG_IGN);
	ptflag = 0;
	if (strcmp(cmd, "RETR") && !proxy)
		pswitch(1);
	else if ((strcmp(cmd, "RETR") == 0) && proxy)
		pswitch(0);
	if (!cpend && !secndflag) {  /* only here if cmd = "STOR" (proxy=1) */
		if (command("%s %s", cmd2, local) != PRELIM) {
			pswitch(0);
			switch (oldtype) {
				case 0:
					break;
				case TYPE_A:
					setascii(0, NULL);
					break;
				case TYPE_I:
					setbinary(0, NULL);
					break;
				case TYPE_E:
					setebcdic(0, NULL);
					break;
				case TYPE_L:
					settenex(0, NULL);
					break;
			}
			if (cpend) {
				char msg[2];

				fprintf(cout, "%c%c", IAC, IP);
				(void) fflush(cout);
				*msg = (char)IAC;
				*(msg+1) = (char)DM;
				if (send(fileno(cout), msg, 2, MSG_OOB) != 2)
					perror("abort");
				fprintf(cout, "ABOR\r\n");
				(void) fflush(cout);
				FD_ZERO(&mask);
				FD_SET(fileno(cin), &mask);
				if ((nfnd = empty(&mask, 10)) <= 0) {
					if (nfnd < 0) {
						perror("abort");
					}
					if (ptabflg)
						code = -1;
					lostpeer(0);
				}
				(void) getreply(0);
				(void) getreply(0);
			}
		}
		pswitch(1);
		if (ptabflg)
			code = -1;
		(void) signal(SIGINT, oldintr);
		return;
	}
	if (cpend) {
		char msg[2];

		fprintf(cout, "%c%c", IAC, IP);
		(void) fflush(cout);
		*msg = (char)IAC;
		*(msg+1) = (char)DM;
		if (send(fileno(cout), msg, 2, MSG_OOB) != 2)
			perror("abort");
		fprintf(cout, "ABOR\r\n");
		(void) fflush(cout);
		FD_ZERO(&mask);
		FD_SET(fileno(cin), &mask);
		if ((nfnd = empty(&mask, 10)) <= 0) {
			if (nfnd < 0) {
				perror("abort");
			}
			if (ptabflg)
				code = -1;
			lostpeer(0);
		}
		(void) getreply(0);
		(void) getreply(0);
	}
	pswitch(!proxy);
	if (!cpend && !secndflag) {  /* only if cmd = "RETR" (proxy=1) */
		if (command("%s %s", cmd2, local) != PRELIM) {
			pswitch(0);
			switch (oldtype) {
				case 0:
					break;
				case TYPE_A:
					setascii(0, NULL);
					break;
				case TYPE_I:
					setbinary(0, NULL);
					break;
				case TYPE_E:
					setebcdic(0, NULL);
					break;
				case TYPE_L:
					settenex(0, NULL);
					break;
			}
			if (cpend) {
				char msg[2];

				fprintf(cout, "%c%c", IAC, IP);
				(void) fflush(cout);
				*msg = (char)IAC;
				*(msg+1) = (char)DM;
				if (send(fileno(cout), msg, 2, MSG_OOB) != 2)
					perror("abort");
				fprintf(cout, "ABOR\r\n");
				(void) fflush(cout);
				FD_ZERO(&mask);
				FD_SET(fileno(cin), &mask);
				if ((nfnd = empty(&mask, 10)) <= 0) {
					if (nfnd < 0) {
						perror("abort");
					}
					if (ptabflg)
						code = -1;
					lostpeer(0);
				}
				(void) getreply(0);
				(void) getreply(0);
			}
			pswitch(1);
			if (ptabflg)
				code = -1;
			(void) signal(SIGINT, oldintr);
			return;
		}
	}
	if (cpend) {
		char msg[2];

		fprintf(cout, "%c%c", IAC, IP);
		(void) fflush(cout);
		*msg = (char)IAC;
		*(msg+1) = (char)DM;
		if (send(fileno(cout), msg, 2, MSG_OOB) != 2)
			perror("abort");
		fprintf(cout, "ABOR\r\n");
		(void) fflush(cout);
		FD_ZERO(&mask);
		FD_SET(fileno(cin), &mask);
		if ((nfnd = empty(&mask, 10)) <= 0) {
			if (nfnd < 0) {
				perror("abort");
			}
			if (ptabflg)
				code = -1;
			lostpeer(0);
		}
		(void) getreply(0);
		(void) getreply(0);
	}
	pswitch(!proxy);
	if (cpend) {
		FD_ZERO(&mask);
		FD_SET(fileno(cin), &mask);
		if ((nfnd = empty(&mask, 10)) <= 0) {
			if (nfnd < 0) {
				perror("abort");
			}
			if (ptabflg)
				code = -1;
			lostpeer(0);
		}
		(void) getreply(0);
		(void) getreply(0);
	}
	if (proxy)
		pswitch(0);
	switch (oldtype) {
		case 0:
			break;
		case TYPE_A:
			setascii(0, NULL);
			break;
		case TYPE_I:
			setbinary(0, NULL);
			break;
		case TYPE_E:
			setebcdic(0, NULL);
			break;
		case TYPE_L:
			settenex(0, NULL);
			break;
	}
	pswitch(1);
	if (ptabflg)
		code = -1;
	(void) signal(SIGINT, oldintr);
}

/*ARGSUSED*/
void
reset(int argc, char *argv[])
{
	struct fd_set mask;
	int nfnd = 1;

	FD_ZERO(&mask);
	while (nfnd) {
		FD_SET(fileno(cin), &mask);
		if ((nfnd = empty(&mask, 0)) < 0) {
			perror("reset");
			code = -1;
			lostpeer(0);
		} else if (nfnd) {
			(void) getreply(0);
		}
	}
}

static char *
gunique(char *local)
{
	static char new[MAXPATHLEN];
	char *cp = rindex(local, '/');
	int d, count = 0;
	char ext = '1';

	if (cp)
		*cp = '\0';
	d = access(cp ? local : ".", 2);
	if (cp)
		*cp = '/';
	if (d < 0) {
		perror(local);
		return ((char *)0);
	}
	(void) strncpy(new, local, sizeof (new));
	if (strlen(local) >= sizeof (new)) {
		printf("gunique: too long: local %s, %d, new %d\n",
		    local, strlen(local), sizeof (new));
		new[MAXPATHLEN - 1] = '\0';
	}

	cp = new + strlen(new);
	*cp++ = '.';
	while (!d) {
		if (++count == 100) {
			printf("runique: can't find unique file name.\n");
			return ((char *)0);
		}
		*cp++ = ext;
		*cp = '\0';
		if (ext == '9')
			ext = '0';
		else
			ext++;
		if ((d = access(new, 0)) < 0)
			break;
		if (ext != '0')
			cp--;
		else if (*(cp - 2) == '.')
			*(cp - 1) = '1';
		else {
			*(cp - 2) = *(cp - 2) + 1;
			cp--;
		}
	}
	return (new);
}
