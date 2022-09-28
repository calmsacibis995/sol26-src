/*
 * Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */
#ifndef _LIST_H
#define _LIST_H

#pragma ident	"@(#)list.h	1.4	96/05/30 SMI"

#include <sys/va_list.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	 VFUNC_T	int (*)(void *, __va_list)	/* for casting */
#define	 COMP_T		int (*)(void *, void *)		/* for casting */

extern void **list_append(void **, void *);
extern void **list_append_unique(void **, void *, int (*)(void *, void*));
extern void **list_concatenate(void **, void **);
extern void * list_locate(void **, int (*)(void *, void *), void *);
extern int list_iterate(void **, int (*)(void *, __va_list), ...);

#ifdef __cplusplus
}
#endif

#endif /* _LIST_H */
