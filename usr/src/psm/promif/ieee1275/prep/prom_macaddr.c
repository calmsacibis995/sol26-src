/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_macaddr.c	1.6	96/03/14 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * This function gets ihandle for network device and buffer as
 * arguments.
 */
#define	MAXPROPLEN	128
int
prom_getmacaddr(ihandle_t hd, caddr_t ea)
{
	phandle_t phndl;
	register int i, len;
	char buff[MAXPROPLEN];

	/* convert network device ihandle to phandle */
	phndl = prom_getphandle(hd);

	if ((len = prom_getproplen(phndl, "mac-address")) != -1) {
		/*
		 * try the mac-address property
		 */
		if (len <= MAXPROPLEN) {
			(void) prom_getprop(phndl, "mac-address", buff);
		} else {
			prom_printf(
			    "error retrieving mac-address property\n");
			return (-1);
		}
	} else if ((len = prom_getproplen(phndl, "local-mac-address")) != -1) {
		/*
		 * if the mac-address property is not present then try the
		 * local-mac-address.  It is debatable that this should ever
		 * happen.
		 */
		if (len <= MAXPROPLEN) {
			(void) prom_getprop(phndl, "local-mac-address", buff);
		} else {
			prom_printf(
			    "error retrieving local-mac-address property\n");
			return (-1);
		}
	} else {
		/* neither local-mac-address or mac-address exist */
		prom_printf(
		    "error retrieving mac-address\n");
		return (-1);
	}

	for (i = 0; i < len; i++) {
		ea[i] = buff[i];
	}
	return (0);
}
