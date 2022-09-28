/*
 * 	Copyright (c) 1991-1994  Sun Microsystems, Inc
 *
 * lib/libsocket/inet/getservbyname_r.c
 *
 * getservbyname_r() is defined in this file. It is implemented on top of
 *   _get_hostserv_inetnetdir_byname() which is also used to implement
 *   netdir_getbyname() for inet family transports.  In turn the common code
 *   uses the name service switch policy for "hosts" and "services" unless
 *   the administrator chooses to bypass the name service switch by
 *   specifying third-party supplied nametoaddr libs for inet transports
 *   in /etc/netconfig.
 *
 * getservbyport_r() is similarly related to _get_hostserv_inetnetdir_byaddr()
 *   and netdir_getbyaddr();
 *
 * The common code lives in lib/libnsl/nss/netdir_inet.c.
 *
 * getservent_r(), setservent() and endservent() are *not* implemented on top
 *   of the common interface;  they go straight to the switch and are
 *   defined in getservent_r.c.
 *
 * There is absolutely no data sharing, not even the stayopen flag or
 *   enumeration state, between getservbyYY_r() and getservent_r();
 */

#ident	"@(#)getservbyname_r.c	1.2	94/03/24	SMI"

#include <netdb.h>
#include <netdir.h>
#include <sys/types.h>
#include <nss_netdir.h>

extern int str2servent(const char *, int, void *, char *, int);
extern struct netconfig *__rpc_getconfip();

struct servent *
getservbyname_r(name, proto, result, buffer, buflen)
	const char	*name;
	const char	*proto;
	struct servent	*result;
	char		*buffer;
	int		buflen;
{
	struct netconfig *nconf;
	struct	nss_netdirbyname_in nssin;
	union	nss_netdirbyname_out nssout;
	int neterr;

	if ((nconf = __rpc_getconfip("udp")) == NULL &&
	    (nconf = __rpc_getconfip("tcp")) == NULL) {
		return ((struct servent *)NULL);
	}
	nssin.op_t = NSS_SERV;
	nssin.arg.nss.serv.name = name;
	nssin.arg.nss.serv.proto = proto;
	nssin.arg.nss.serv.buf = buffer;
	nssin.arg.nss.serv.buflen = buflen;

	nssout.nss.serv = result;

	/*
	 * We pass in nconf and let the implementation of the long-named func
	 * decide whether to use the switch based on nc_nlookups.
	 */
	neterr = _get_hostserv_inetnetdir_byname(nconf, &nssin, &nssout);

	(void) freenetconfigent(nconf);
	if (neterr != ND_OK) {
		return ((struct servent *)NULL);
	}
	return (nssout.nss.serv);
}

struct servent *
getservbyport_r(port, proto, result, buffer, buflen)
	int		port;
	const char	*proto;
	struct servent	*result;
	char		*buffer;
	int		buflen;
{
	struct netconfig *nconf;
	struct	nss_netdirbyaddr_in nssin;
	union	nss_netdirbyaddr_out nssout;
	int neterr;

	if ((nconf = __rpc_getconfip("udp")) == NULL &&
	    (nconf = __rpc_getconfip("tcp")) == NULL) {
		return ((struct servent *)NULL);
	}
	nssin.op_t = NSS_SERV;
	nssin.arg.nss.serv.port = port;
	nssin.arg.nss.serv.proto = proto;
	nssin.arg.nss.serv.buf = buffer;
	nssin.arg.nss.serv.buflen = buflen;

	nssout.nss.serv = result;

	/*
	 * We pass in nconf and let the implementation of this long-named func
	 * decide whether to use the switch based on nc_nlookups.
	 */
	neterr = _get_hostserv_inetnetdir_byaddr(nconf, &nssin, &nssout);

	(void) freenetconfigent(nconf);
	if (neterr != ND_OK) {
		return ((struct servent *)NULL);
	}
	return (nssout.nss.serv);
}
