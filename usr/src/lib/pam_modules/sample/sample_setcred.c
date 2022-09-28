/*
 * Copyright (c) 1992-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)sample_setcred.c	1.2	96/04/09 SMI"	/* PAM 2.6 */

#include <libintl.h>
#include <security/pam_appl.h>
#include <security/pam_modules.h>

#define	PAMTXD	"SUNW_OST_SYSOSPAM"

/*
 * pam_sm_setcred
 */
int
pam_sm_setcred(
	pam_handle_t *pamh,
	int   flags,
	int	argc,
	const char **argv)
{

	/*
	 * Set the credentials
	 */

	return (PAM_SUCCESS);
}
