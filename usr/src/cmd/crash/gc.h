/*
 *		Copyright (C) 1995  Sun Microsystems, Inc
 *			All rights reserved.
 *		Notice of copyright on this source code
 *		product does not indicate publication.
 *
 *		RESTRICTED RIGHTS LEGEND:
 *   Use, duplication, or disclosure by the Government is subject
 *   to restrictions as set forth in subparagraph (c)(1)(ii) of
 *   the Rights in Technical Data and Computer Software clause at
 *   DFARS 52.227-7013 and in similar clauses in the FAR and NASA
 *   FAR Supplement.
 */

#ifndef _GC_H
#define	_GC_H

#pragma ident	"@(#)gc.h	1.2	95/11/13 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This is the header corresponding to a set of routines that do
 * memory allocation with the ability to free everything allocated
 * en masse.  For code which periodically returns to a quiescent
 * state (e.g. an interactive program which allocates memory in
 * response to a user request and frees it before issuing the next
 * prompt), this primitive form of garbage-collection can be quite
 * effective, saving a lot of situation-specific cleanup code.
 */

typedef struct gc_handle *gc_handle_t;

gc_handle_t gc_create();  /* creates a garbage-collection "pool" */
void	    gc_destroy(gc_handle_t);  /* destroys pool (also frees all) */

void   *gc_malloc(gc_handle_t, size_t);
void  *gc_realloc(gc_handle_t, void *, size_t);
void   *gc_calloc(gc_handle_t, size_t, size_t);
void	  gc_free(gc_handle_t, void *);
void  gc_free_all(gc_handle_t);  /* frees everything in the pool */
void	  gc_move(gc_handle_t, void *, gc_handle_t); /* moves buf (from,to) */
void  gc_move_all(gc_handle_t, gc_handle_t);  /* moves pool (from,to) */

void  gc_dump    (gc_handle_t);  /* DEBUG version only; shows gc buffers */

#ifdef	__cplusplus
}
#endif

#endif /* _GC_H */
