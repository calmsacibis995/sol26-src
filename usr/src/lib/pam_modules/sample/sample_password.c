/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)sample_password.c	1.2	96/04/09 SMI"	/* PAM 2.6 */

#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <syslog.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pwd.h>

int
pam_sm_chauthtok(
	pam_handle_t	*pamh,
	int		flags,
	int		argc,
	const char	**argv)

{
	return (PAM_SUCCESS);
}
