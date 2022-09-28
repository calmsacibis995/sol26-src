/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_NIS_INTERNAL_HH
#define	_FNSP_NIS_INTERNAL_HH

#pragma ident	"@(#)fnsp_nis_internal.hh	1.2	96/05/16 SMI"

#include <xfn/xfn.hh>
#include "fnsp_internal_common.hh"

/* Constants defined for NIS maps */
#define	FNS_NIS_SIZE 1024
#define	FNS_MAX_ENTRY (64*1024)
#define	FNS_NIS_INDEX 256

// Routine to bind to specific NIS domain
extern unsigned FNSP_nis_bind(const char *);
extern unsigned FNSP_nis_map_status(int);
extern unsigned FNSP_update_makefile(const char *);
extern unsigned FNSP_update_map(const char *, const char *,
    const char *, const void *, FNSP_map_operation);
extern unsigned FNSP_yp_map_lookup(char *, char *, char *,
    int, char **, int *);
extern int FNSP_is_fns_installed(const FN_ref_addr *);
extern int
FNSP_decompose_nis_index_name(const FN_string &src,
    FN_string &tabname, FN_string &indexname);

extern unsigned FNSP_compose_next_map_name(char *map);


#endif /* _FNSP_NIS_INTERNAL_HH */
