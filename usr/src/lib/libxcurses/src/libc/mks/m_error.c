/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)m_error.c 1.1	96/01/17 SMI"

/*
 * Copyright 1993 by Mortice Kern Systems Inc.  All rights reserved.
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
static char rcsID[] = "$Id: m_error.c 1.2 1993/05/22 01:37:29 alex Exp $";
#endif
#endif

#include <mks.h>

/*f
 * Invoke common error message function
 */
void
m_error VARARG1(const char *, fmt)
{
	va_list args;

	va_start(args, fmt);
	(*m_errorfn)(fmt, args);
	va_end(args);
}
