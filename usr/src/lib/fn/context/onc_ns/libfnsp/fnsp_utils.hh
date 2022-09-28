/*
 * Copyright (c) 1992 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_UTIL_HH
#define	_FNSP_UTIL_HH

#pragma ident	"@(#)fnsp_utils.hh	1.1	96/03/31 SMI"

#include <xfn/xfn.hh>
#include "FNSP_Address.hh"

extern FN_attrset *
FNSP_get_create_params_from_attrs(const FN_attrset *attrs,
    unsigned int &context_type,
    unsigned int &repr_type,
    FN_identifier **ref_type,
    unsigned int default_context_type = 0);

extern FN_attrset *
FNSP_get_selected_attrset(const FN_attrset &wholeset,
    const FN_attrset &selection);

extern int
FNSP_is_attr_subset(const FN_attribute &wholeset,
    const FN_attribute &subset);

extern int
FNSP_is_attrset_subset(const FN_attrset &haystack,
	const FN_attrset &needles);

// Returns 0 is not host or user context type
// Returns 1 if user context and
// Returns 2 if host context
extern int
FNSP_is_hostuser_ctx_type(const FNSP_Address &ctx,
    const FN_string &atomic_name);

extern int
FNSP_does_builtin_attrset_exist(const FNSP_Address &ctx,
    const FN_string &atomic_name);

extern unsigned
FNSP_check_builtin_attrset(const FNSP_Address &ctx,
    const FN_string &atomic_name, const FN_attrset &attrset);

extern unsigned
FNSP_check_builtin_attribute(const FNSP_Address &ctx,
    const FN_string &atomic_name, const FN_attribute &attr);

extern FN_attrset *
FNSP_get_host_builtin_attrset(const char *hostname,
    const char *hostentry, unsigned &status);

extern FN_attrset *
FNSP_get_user_builtin_attrset(const char *passwdentry,
    const char *shadowentry, unsigned &status);

extern int
FNSP_is_attribute_in_builtin_attrset(const FN_attribute &attr);

extern int
FNSP_is_attrset_in_builtin_attrset(const FN_attrset &attrset);

extern unsigned
FNSP_remove_builtin_attrset(FN_attrset &attrset);

extern FN_string *
FNSP_homedir_from_passwd_entry(const char *, unsigned &status);

typedef FN_string *(*FNSP_get_homedir_fn)(
    const FNSP_Address &,
    const FN_string &username,
    unsigned &status);

extern void
FNSP_process_user_fs(
    const FNSP_Address &,
    FN_ref &,
    FNSP_get_homedir_fn,
    unsigned &status);

#endif /* _FNSP_UTIL_HH */
