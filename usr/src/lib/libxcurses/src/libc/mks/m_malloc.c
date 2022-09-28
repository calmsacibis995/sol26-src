/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)m_malloc.c 1.1	96/01/17 SMI"

/*
 * MKS interface extension.
 * Ensure that errno is set if malloc() fails.
 *
 * Copyright 1992 by Mortice Kern Systems Inc.  All rights reserved.
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
static char rcsID[] = "$Header: /rd/src/libc/mks/rcs/m_malloc.c 1.4 1993/12/17 15:22:04 rog Exp $";
#endif /*lint*/
#endif /*M_RCSID*/

#include <mks.h>
#include <errno.h>
#include <stdlib.h>

#ifdef __STDC__
#define _VOID	void
#else
#define _VOID	char
#endif

#undef m_malloc	   /* in case <mks.h> included in <errno.h> or <stdlib.h> */

/*f
 * m_malloc: 
 *   Portable replacement for malloc().
 *   If malloc() fails (e.g returns NULL)
 *   then return ENOMEM unless malloc() sets errno for us on this system
 *   and ensure malloc(0) returns a non-NULL pointer.
 *
 */
_VOID*
m_malloc(amount)
size_t amount;
{
	_VOID* ptr;

	/*l
	 * Prob 1:
	 *  ANSI does not insist setting errno when malloc() fails.
	 *  But UNIX existing practice (which MKS relies on) always returns
	 *  an errno when malloc() fails.
	 *  Thus, on systems that implement malloc() where an errno is not
	 *  returned, we set ENOMEM.
	 *
	 *  Note: we don't care about previous value of errno since
	 *        POSIX.1 (Section 2.4) says you can only look at errno
	 *        after a function returns a status indicating an error.
	 *        (and the function explicitly states an errno value can be
	 *         returned - Well, m_malloc() is so stated.)
	 *
	 * Prob 2:
         *  MKS code seems to rely on malloc(0) returning a valid pointer.
	 *  This allows it to realloc() later when actual size is determined.
	 *
	 *  According to ANSI (4.10.3 line 18-19) the result of malloc(0) is
	 *  implementation-defined.
	 */

	errno = 0;
	if ((ptr = malloc(amount)) == NULL) {
		if (amount == 0) {
			/*
			 *  confirm we are really out of memory
			 */
			return (m_malloc(1));
		}
		if (errno==0) {
			/*
			 *  ensure errno is always set
			 */
			errno = ENOMEM;
		}
	}
	return (ptr);
}
