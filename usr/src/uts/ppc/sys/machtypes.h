/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_MACHTYPES_H
#define	_SYS_MACHTYPES_H

#pragma ident	"@(#)machtypes.h	1.7	96/04/29 SMI"

#include <sys/feature_tests.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Machine dependent types:
 *
 *	PowerPC Version
 */

#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)

typedef struct  _physadr_t { int r[1]; } *physadr_t;

typedef	struct	_label_t { int	val[22]; } label_t;

#endif /* !defined(_POSIX_C_SOURCE)... */

typedef	unsigned char	lock_t;		/* lock work for busy wait */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MACHTYPES_H */
