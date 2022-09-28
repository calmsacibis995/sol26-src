/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)getsyntx.c 1.1	96/01/17 SMI"

/*
 * Dummy version of getsyntx() from C/370.
 * This version simply uses the external variable VARIANTS, and assigns them
 * to the variant structure.
 * This is a dummy routine, that would not actually run on any production
 * system.
 *
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
static char const rcsID[] = "$Header: /rd/src/libc/mks/rcs/getsyntx.c 1.4 1995/04/19 23:04:55 ross Exp $";
#endif /* lint */
#endif /* M_RCSID */

#include <mks.h>
#include <variant.h>
#include <stdlib.h>

struct variant *
getsyntx(void)
{
	static struct variant v;
	static char const var[] = "\\][}{^~!#|$@`";
	char const *e;
	
	if ((e = __m_getenv("VARIANTS")) == NULL || *e == '\0')
		e = var;
	else if (strlen(e) != 13) {
		m_error("getsyntx: environment variable VARIANTS: must be exactly 13 bytes long");
		return NULL;
	}
	v.backslash = e[0];
	v.right_bracket = e[1];
	v.left_bracket = e[2];
	v.right_brace = e[3];
	v.left_brace = e[4];
	v.circumflex = e[5];
	v.tilde = e[6];
	v.exclamation_mark = e[7];
	v.number_sign = e[8];
	v.vertical_line = e[9];
	v.dollar_sign = e[10];
	v.commercial_at = e[11];
	v.grave_accent = e[12];
	return &v;
}
