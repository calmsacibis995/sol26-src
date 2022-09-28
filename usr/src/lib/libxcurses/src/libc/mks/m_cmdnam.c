/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)m_cmdnam.c 1.1	96/01/17 SMI"

/*
 * m_cmdname -- MKS specific library routine.
 *
 * Copyright 1985, 1992 by Mortice Kern Systems Inc.  All rights reserved.
 *
 * This Software is unpublished, valuable, confidential property of
 * Mortice Kern Systems Inc.  Use is authorized only in accordance
 * with the terms and conditions of the source licence agreement
 * protecting this Software.  Any unauthorized use or disclosure of
 * this Software is strictly prohibited and will result in the
 * termination of the licence agreement.
 *
 * If you have any questions, please consult your supervisor.
 */

#ifdef M_RCSID
#ifndef lint
static char rcsID[] = "$Id: m_cmdnam.c 1.16 1993/02/15 14:12:23 fredw Exp $";
#endif
#endif

#include <mks.h>
#include <ctype.h>
#include <string.h>

/*f
 *  MKS private routine to hide o/s dependencies in filenames in argv[0];
 *  cmdname(argv[0]) returns a modified argv[0] which contains only the command
 *  name, prefix, suffix stripped, and lower cased on case-less systems.
 */
LDEFN char *
m_cmdname(cmd)
char *cmd;
{
#if defined(DOS) || defined(OS2) || defined(NT)
	register char *ap;

	/* Lowercase command name on DOS, OS2 and NT. */
	/* The shell needs the whole name lowered. */
	for (ap = cmd; *ap; ap++)
		if (isupper(*ap))
			*ap = _tolower(*ap);

	cmd = basename(cmd);

	/* Strip .com/.exe/.??? suffix on DOS and OS/2 */
	if ((ap = strrchr(cmd, '.')) != NULL)
		*ap = '\0';

	return (cmd);
#else
	return (basename(cmd));
#endif
}
