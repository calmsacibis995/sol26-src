/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)m_getenv.c 1.1	96/01/17 SMI"

/*
 * MKS interface extension.
 * A version of getenv() that doesn't overwrite it's return value
 * on each call.
 *
 * Copyright 1995 by Mortice Kern Systems Inc.  All rights reserved.
 *
 * This Software is unpublished, valuable, confidential property of
 * Mortice Kern Systems Inc.  Use is authorized only in accordance
 * with the terms and conditions of the source licence agreement
 * protecting this Software.  Any unauthorized use or disclosure of
 * this Software is strictly prohibited and will result in the
 * termination of the licence agreement.
 *
 * If you have any questions, please consult your supervisor.
 * 
 */

#ifdef M_RCSID
#ifndef lint
static char const rcsID[] = "$Header: /rd/src/libc/mks/rcs/m_getenv.c 1.2 1995/07/11 16:53:01 ross Exp $";
#endif /*lint*/
#endif /*M_RCSID*/

#include <mks.h>
#include <stdlib.h>
#include <string.h>

#ifdef M_NON_STATIC_GETENV

#undef __m_getenv

/*f
 *  Assume getenv() works the way we expect it to on PC systems.
 */
char *
__m_getenv(char const *name) {
	return getenv(name);
}

#else /* M_NON_STATIC_GETENV */

extern char **environ;

/*f
 *  A version of getenv safe to use in library functions.  According to
 *  ANSI C and XPG 4 no library function shall behave as if it called
 *  getenv.  This is a problem on systems that have getenv functions
 *  that overwrite their return value on each call.
 */

char *
__m_getenv(char const *name) {
	if (m_setenv() != NULL) {
		int len = strlen(name);
		char **envp = environ;
		char *s = *envp++;

		while(s != NULL) {
			if (strncmp(name, s, len) == 0 && s[len] == '=') {
				return s + len + 1;
			}
			s = *envp++;
		}
	}
	return NULL;
}
	
#endif /* M_NON_STATIC_GETENV */
