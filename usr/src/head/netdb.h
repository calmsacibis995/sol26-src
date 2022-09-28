/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	(c) 1986,1987,1988,1989,1996  Sun Microsystems, Inc
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 * BIND 4.9.3:
 *
 * Copyright (c) 1980, 1983, 1988, 1993
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
 * -
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * --Copyright--
 *
 * End BIND 4.9.3
 */

/*
 * Structures returned by network data base library.
 * All addresses are supplied in host order, and
 * returned in network order (suitable for use in system calls).
 */

#ifndef _NETDB_H
#define	_NETDB_H

#pragma ident	"@(#)netdb.h	1.18	96/09/24 SMI"

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/feature_tests.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	_PATH_HEQUIV	"/etc/hosts.equiv"
#define	_PATH_HOSTS	"/etc/hosts"
#define	_PATH_NETMASKS	"/etc/netmasks"
#define	_PATH_NETWORKS	"/etc/networks"
#define	_PATH_PROTOCOLS	"/etc/protocols"
#define	_PATH_SERVICES	"/etc/services"

struct	hostent {
	char	*h_name;	/* official name of host */
	char	**h_aliases;	/* alias list */
	int	h_addrtype;	/* host address type */
	int	h_length;	/* length of address */
	char	**h_addr_list;	/* list of addresses from name server */
#define	h_addr	h_addr_list[0]	/* address, for backward compatiblity */
};

/*
 * Assumption here is that a network number
 * fits in 32 bits -- probably a poor one.
 */
struct	netent {
	char		*n_name;	/* official name of net */
	char		**n_aliases;	/* alias list */
	int		n_addrtype;	/* net address type */
	in_addr_t	n_net;		/* network # */
};

struct	protoent {
	char	*p_name;	/* official protocol name */
	char	**p_aliases;	/* alias list */
	int	p_proto;	/* protocol # */
};

struct	servent {
	char	*s_name;	/* official service name */
	char	**s_aliases;	/* alias list */
	int	s_port;		/* port # */
	char	*s_proto;	/* protocol to use */
};

#ifdef	__STDC__
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
struct hostent	*gethostbyname_r
	(const char *, struct hostent *, char *, int, int *h_errnop);
struct hostent	*gethostbyaddr_r
	(const char *, int, int, struct hostent *, char *, int, int *h_errnop);
struct hostent	*gethostent_r(struct hostent *, char *, int, int *h_errnop);

struct servent	*getservbyname_r
	(const char *name, const char *, struct servent *, char *, int);
struct servent	*getservbyport_r
	(int port, const char *, struct servent *, char *, int);
struct servent	*getservent_r(struct	servent *, char *, int);

struct netent	*getnetbyname_r
	(const char *, struct netent *, char *, int);
struct netent	*getnetbyaddr_r(long, int, struct netent *, char *, int);
struct netent	*getnetent_r(struct netent *, char *, int);

struct protoent	*getprotobyname_r
	(const char *, struct protoent *, char *, int);
struct protoent	*getprotobynumber_r
	(int, struct protoent *, char *, int);
struct protoent	*getprotoent_r(struct protoent *, char *, int);

int getnetgrent_r(char **, char **, char **, char *, int);
int innetgr(const char *, const char *, const char *, const char *);
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

/* Old interfaces that return a pointer to a static area;  MT-unsafe */
struct hostent	*gethostbyname(const char *);
struct hostent	*gethostent(void);
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
struct netent	*getnetbyaddr(long, int);
#else
struct netent	*getnetbyaddr(in_addr_t, int);
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */
struct netent	*getnetbyname(const char *);
struct netent	*getnetent(void);
struct protoent	*getprotobyname(const char *);
struct protoent	*getprotobynumber(int);
struct protoent	*getprotoent(void);
struct servent	*getservbyname(const char *, const char *);
struct servent	*getservbyport(int, const char *);
struct servent	*getservent(void);
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
int		 getnetgrent(char **, char **, char **);
struct hostent	*gethostbyaddr(const char *, int, int);
#else
struct hostent	*gethostbyaddr(const void *, size_t, int);
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
int endhostent(void);
int endnetent(void);
int endprotoent(void);
int endservent(void);
int sethostent(int);
int setnetent(int);
int setprotoent(int);
int setservent(int);
#else
void endhostent(void);
void endnetent(void);
void endprotoent(void);
void endservent(void);
void sethostent(int);
void setnetent(int);
void setprotoent(int);
void setservent(int);
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
int setnetgrent(const char *);
int endnetgrent(void);
int rcmd(char **, unsigned short,
	const char *, const char *, const char *, int *);
int rexec(char **, unsigned short,
	const char *, const char *, const char *, int *);
int rresvport(int *);
int ruserok(const char *, int, const char *, const char *);
/* BIND 4.9.3 */
struct hostent	*gethostbyname2(const char *, int);
void		herror(const char *);
const char	*hstrerror(int);
/* End BIND 4.9.3 */
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */
#else	/* __STDC__ */
struct hostent	*gethostbyname_r();
struct hostent	*gethostbyaddr_r();
struct hostent	*gethostent_r();
struct servent	*getservbyname_r();
struct servent	*getservbyport_r();
struct servent	*getservent_r();
struct netent	*getnetbyname_r();
struct netent	*getnetbyaddr_r();
struct netent	*getnetent_r();
struct protoent	*getprotobyname_r();
struct protoent	*getprotobynumber_r();
struct protoent	*getprotoent_r();
int		 getnetgrent_r();
int		 innetgr();

/* Old interfaces that return a pointer to a static area;  MT-unsafe */
struct hostent	*gethostbyname();
struct hostent	*gethostbyaddr();
struct hostent	*gethostent();
struct netent	*getnetbyname();
struct netent	*getnetbyaddr();
struct netent	*getnetent();
struct servent	*getservbyname();
struct servent	*getservbyport();
struct servent	*getservent();
struct protoent	*getprotobyname();
struct protoent	*getprotobynumber();
struct protoent	*getprotoent();
int		 getnetgrent();

int sethostent();
int endhostent();
int setnetent();
int endnetent();
int setservent();
int endservent();
int setprotoent();
int endprotoent();
int setnetgrent();
int endnetgrent();
int rcmd();
int rexec();
int rresvport();
int ruserok();
/* BIND 4.9.3 */
struct hostent	*gethostbyname2();
void		herror();
char		*hstrerror();
/* End BIND 4.9.3 */
#endif	/* __STDC__ */

/*
 * Error return codes from gethostbyname() and gethostbyaddr()
 * (when using the resolver)
 */

extern  int h_errno;

#define	HOST_NOT_FOUND	1 /* Authoritive Answer Host not found */
#define	TRY_AGAIN	2 /* Non-Authoritive Host not found, or SERVERFAIL */
#define	NO_RECOVERY	3 /* Non recoverable errors, FORMERR, REFUSED, NOTIMP */
#define	NO_DATA		4 /* Valid name, no data record of requested type */

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#define	NO_ADDRESS	NO_DATA		/* no address, look for MX record */

/* BIND 4.9.3 */
#define	NETDB_INTERNAL	-1	/* see errno */
#define	NETDB_SUCCESS	0	/* no problem */
/* End BIND 4.9.3 */

#define	MAXHOSTNAMELEN	256

#define	MAXALIASES	35
#define	MAXADDRS	35
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#ifdef	__cplusplus
}
#endif

#endif	/* _NETDB_H */
