/*
 * Copyright (c) 1991-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_vername.c	1.2	95/06/28 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * Return a character string in buf,buflen representing the running
 * version of the firmware. Systems that have no concept of such a
 * string may return the string "unknown".
 *
 * Return the actual length of the string, including the NULL terminator.
 * Copy at most buflen bytes into the caller's buffer, always providing
 * NULL termination.
 *
 * Returns the actual length of the string, plus copies data in the callers
 * buf copying at most buflen bytes.  Returns -1 if an internal error occurs.
 */

int
prom_version_name(char *buf, int buflen)
{
	dnode_t nodeid;
	int proplen;
	char *unknown = "unknown";

	*buf = *(buf + buflen - 1) = (char)0;	/* Force NULL termination */

	/*
	 * Virtual Open Firmware seems to keep it's version in
	 * the /openprom "model" property.
	 * If we cannot find that, return "unknown".
	 */

	nodeid = prom_finddevice("/openprom");
	if (nodeid == (dnode_t)-1)
		return (-1);

	proplen = prom_bounded_getprop(nodeid, "model", buf, buflen - 1);
	if (proplen <= 0) {
		(void) prom_strncpy(buf, unknown, buflen - 1);
		return (prom_strlen(unknown) + 1);
	}

	return (proplen);
}
