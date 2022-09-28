/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)abandon.c	1.2	96/04/10 SMI"


/*
 *  Copyright (c) 1990 Regents of the University of Michigan.
 *  All rights reserved.
 *
 *  abandon.c
 */

#ifndef lint
static char copyright[] = "@(#) Copyright (c) 1990 Regents of the University "
			    "of Michigan.\nAll rights reserved.\n";
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h> /* malloc(), realloc() for Solaris */

#if !defined(MACOS) && !defined(DOS)
#include <sys/types.h>
#include <sys/socket.h>
#endif

#if defined(DOS) || defined(_WIN32)
#include <malloc.h>
#include "msdos.h"
#endif /* DOS */

#ifdef MACOS
#include <stdlib.h>
#include "macos.h"
#endif /* MACOS */

#include "lber.h"
#include "ldap.h"
#include "ldap-int.h"

/*
 * ldap_abandon - perform an ldap (and X.500) abandon operation. Parameters:
 *
 *	ld		LDAP descriptor
 *	msgid		The message id of the operation to abandon
 *
 * ldap_abandon returns 0 if everything went ok, -1 otherwise.
 *
 * Example:
 *	ldap_abandon(ld, msgid);
 */
int
ldap_abandon(LDAP *ld, int msgid)
{
	BerElement	*ber;
	int		i, err;

	/*
	 * An abandon request looks like this:
	 *	AbandonRequest ::= MessageID
	 */

	Debug(LDAP_DEBUG_TRACE, "ldap_abandon %d\n", msgid, 0, 0);

	if (ldap_msgdelete(ld, msgid) == 0) {
		ld->ld_errno = LDAP_SUCCESS;
		return (0);
	}

	/* create a message to send */
	if ((ber = alloc_ber_with_options(ld)) == NULLBER) {
		return (-1);
	}

#ifdef CLDAP
	if (ld->ld_sb.sb_naddr > 0) {
		err = ber_printf(ber, "{isti}", ++ld->ld_msgid, ld->ld_cldapdn,
		    LDAP_REQ_ABANDON, msgid);
	} else {
#endif /* CLDAP */
		err = ber_printf(ber, "{iti}", ++ld->ld_msgid,
		    LDAP_REQ_ABANDON, msgid);
#ifdef CLDAP
	}
#endif /* CLDAP */

	if (err == -1) {
		ld->ld_errno = LDAP_ENCODING_ERROR;
		ber_free(ber, 1);
		return (-1);
	}

	/* send the message */
	if (ber_flush(&ld->ld_sb, ber, 1) != 0) {
		ld->ld_errno = LDAP_SERVER_DOWN;
		return (-1);
	}

	if (ld->ld_abandoned == NULL) {
		if ((ld->ld_abandoned = (int *) malloc(2 * sizeof (int)))
		    == NULL) {
			ld->ld_errno = LDAP_NO_MEMORY;
			return (-1);
		}
		i = 0;
	} else {
		for (i = 0; ld->ld_abandoned[i] != -1; i++)
			/* NULL */;
		if ((ld->ld_abandoned = (int *) realloc((char *)
		    ld->ld_abandoned, (i + 2) * sizeof (int))) == NULL) {
			ld->ld_errno = LDAP_NO_MEMORY;
			return (-1);
		}
	}
	ld->ld_abandoned[i] = msgid;
	ld->ld_abandoned[i + 1] = -1;

	ld->ld_errno = LDAP_SUCCESS;
	return (0);
}
