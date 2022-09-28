/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)sample_close_session.c	1.2	96/04/09 SMI"	/* PAM 2.6 */

#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <syslog.h>

int
pam_sm_close_session(
	pam_handle_t *pamh,
	int	flags,
	int	argc,
	const char **argv)
{
	return (PAM_SUCCESS);
}
