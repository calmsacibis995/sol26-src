/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_ANLYZERR_H
#define	_ANLYZERR_H

#pragma ident	"@(#)anlyzerr.h	1.5	96/02/16 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	MAX_MSGS	64

/*
 * Error analysis routines. These routines decode data from specified
 * error registers. They are meant to be used for decoding the fatal
 * hardware reset data passed to the kernel by sun4u POST.
 */
int analyze_cpu(char **, int, u_longlong_t);
int analyze_ac(char **, u_longlong_t);
int analyze_dc(int, char **, u_longlong_t);

#ifdef	__cplusplus
}
#endif

#endif	/* _ANLYZERR_H */
