/*
 * Copyright (c) 1992 - 1994-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_OrgContext.cc	1.1	96/03/31 SMI"


#include "FNSP_OrgContext.hh"
#include "FNSP_Impl.hh"
#include "FNSP_Syntax.hh"
#include "fnsp_utils.hh"

static const FN_string empty_name((unsigned char *)"");

FNSP_OrgContext::~FNSP_OrgContext()
{
	// clean up done by subclasses
}

FN_ref *
FNSP_OrgContext::get_ref(FN_status &stat) const
{
	stat.set_success();

	return (new FN_ref(*my_reference));
}

FN_composite_name *
FNSP_OrgContext::equivalent_name(const FN_composite_name &name,
    const FN_string &, FN_status &status)
{
	status.set(FN_E_OPERATION_NOT_SUPPORTED, my_reference, &name);
	return (0);
}

// Given reference for an organization,
// extract the directory name of the org from the reference,
// and construct the object name for the nns context associated
// with the org.
// If 'dirname_holder' is supplied, use it to return the directory name of org.
FN_string *
FNSP_OrgContext::get_nns_objname(const FN_ref &ref,
	unsigned &status,
	FN_string **dirname_holder)
{
	FN_string *dirname = FNSP_reference_to_internal_name(ref);
	FN_string *nnsobjname = 0;

	if (dirname) {
		nnsobjname = ns_impl->get_nns_objname(dirname);
		status = FN_SUCCESS;
	} else
		status = FN_E_MALFORMED_REFERENCE;
	if (dirname_holder)
		*dirname_holder = dirname;
	else
		delete dirname;
	return (nnsobjname);
}

FN_ref *
FNSP_OrgContext::resolve(const FN_string &name, FN_status_csvc &cstat)
{
	int stat_set = 0;
	FN_status stat;
	FN_ref *answer = 0;

	if (name.is_empty()) {
		// No name was given; resolves to current reference of context
		answer = new FN_ref(*my_reference);
		if (answer)
			cstat.set_success();
		else
			cstat.set(FN_E_INSUFFICIENT_RESOURCES);
	} else {
		cstat.set_error(FN_E_NAME_NOT_FOUND, *my_reference, name);
	}
	return (answer);
}

FN_ref *
FNSP_OrgContext::c_lookup(const FN_string &name, unsigned int,
    FN_status_csvc &stat)
{
	return (resolve(name, stat));
}

FN_namelist*
FNSP_OrgContext::c_list_names(const FN_string &name, FN_status_csvc &cstat)
{
	// default implementation is to list the empty set
	FN_nameset* answer = 0;
	FN_ref *ref = resolve(name, cstat);
	if (cstat.is_success()) {
		answer = new FN_nameset;
		if (answer == 0)
			cstat.set(FN_E_INSUFFICIENT_RESOURCES);
		else
			cstat.set_success();
	}
	delete ref;
	if (answer)
		return (new FN_namelist_svc(answer));
	else
		return (0);
}

FN_bindinglist*
FNSP_OrgContext::c_list_bindings(const FN_string &name,
    FN_status_csvc &cstat)
{
	FN_ref *ref = resolve(name, cstat);
	FN_bindingset* answer = 0;
	if (cstat.is_success()) {
		answer = new FN_bindingset;
		if (answer == 0)
			cstat.set(FN_E_INSUFFICIENT_RESOURCES);
		else
			cstat.set_success();
	}
	delete ref;
	if (answer)
		return (new FN_bindinglist_svc(answer));
	else
		return (0);
}

int
FNSP_OrgContext::c_bind(const FN_string &name, const FN_ref &,
    unsigned, FN_status_csvc &cstat)
{
	/* not supported for ORG */
	cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

int
FNSP_OrgContext::c_unbind(const FN_string &name, FN_status_csvc &cstat)
{
	/* not supported for ORG */
	cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}


int
FNSP_OrgContext::c_rename(const FN_string &name, const FN_composite_name &,
    unsigned, FN_status_csvc &cstat)
{
	/* not supported for ORG */
	cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}


FN_ref *
FNSP_OrgContext::c_create_subcontext(const FN_string &name,
    FN_status_csvc& cstat)
{
	// Not supported for ORG, cannot be done in FILES
	cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}


int
FNSP_OrgContext::c_destroy_subcontext(const FN_string &name,
    FN_status_csvc &cstat)
{
	// Should not be supported.  Rather dangerous.
	cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_attrset*
FNSP_OrgContext::c_get_syntax_attrs(const FN_string &name,
    FN_status_csvc &cstat)
{
	FN_ref *ref = resolve(name, cstat);

	if (cstat.is_success()) {
		FN_attrset* answer =
		    FNSP_Syntax(FNSP_organization_context)->get_syntax_attrs();
		delete ref;
		if (answer) {
			return (answer);
		}
		cstat.set_error(FN_E_INSUFFICIENT_RESOURCES, *my_reference,
		    name);
		return (0);
	}
	return (0);
}

FN_attribute*
FNSP_OrgContext::c_attr_get(const FN_string &name,
    const FN_identifier &, unsigned int,
    FN_status_csvc &cs)
{
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

int
FNSP_OrgContext::c_attr_modify(const FN_string &name,
    unsigned int,
    const FN_attribute&, unsigned int,
    FN_status_csvc &cs)
{
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_valuelist*
FNSP_OrgContext::c_attr_get_values(const FN_string &name,
    const FN_identifier &, unsigned int,
    FN_status_csvc &cs)
{
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_attrset*
FNSP_OrgContext::c_attr_get_ids(const FN_string &name, unsigned int,
    FN_status_csvc &cs)
{
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_multigetlist*
FNSP_OrgContext::c_attr_multi_get(const FN_string &name,
    const FN_attrset *, unsigned int,
    FN_status_csvc &cs)
{
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

int
FNSP_OrgContext::c_attr_multi_modify(const FN_string &name,
    const FN_attrmodlist&, unsigned int,
    FN_attrmodlist**,
    FN_status_csvc &cs)
{
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

// == Lookup (name:)
// %%% cannot be linked (reference generated algorithmically)
// %%% If supported, must store link somewhere and change
// %%% entire ctx implementation, which depends on non-linked repr
// %%%
FN_ref *
FNSP_OrgContext::c_lookup_nns(const FN_string &name,
    unsigned int, /* lookup_flags */
    FN_status_csvc& cstat)
{
	FN_ref *answer = 0;
	unsigned status;

	if (name.is_empty()) {
		FNSP_Impl *nns_impl = get_nns_impl(*my_reference, status);
		if (nns_impl != 0) {
			unsigned estatus = nns_impl->context_exists();
			switch (estatus) {
			case FN_SUCCESS:
				answer = nns_impl->get_nns_ref();
				if (answer == 0)
					cstat.set(
					    FN_E_INSUFFICIENT_RESOURCES);
				break;
			default:
				cstat.set_error(FN_E_NAME_NOT_FOUND,
				    *my_reference, name);
			}
			delete nns_impl;
		} else
			cstat.set_error(status, *my_reference, name);
	} else
		cstat.set_error(FN_E_NAME_NOT_FOUND, *my_reference, name);
	return (answer);
}

FN_namelist*
FNSP_OrgContext::c_list_names_nns(const FN_string &name,
    FN_status_csvc& cstat)
{
	unsigned status;
	FN_nameset* answer = 0;
	FN_ref *ref = resolve(name, cstat);

	if (cstat.is_success()) {
		FNSP_Impl *nns_impl = get_nns_impl(*ref, status);
		if (nns_impl) {
			answer = nns_impl->list_names(status);
			delete nns_impl;
			if (status == FN_E_NOT_A_CONTEXT)
				// %%% was CONTEXT_NOT_FOUND
				status = FN_E_NAME_NOT_FOUND;
			if (status != FN_SUCCESS)
				cstat.set_error(status, *ref, empty_name);
		} else
			cstat.set_error(status, *ref, empty_name);
	}

	delete ref;
	if (answer)
		return (new FN_namelist_svc(answer));
	else
		return (0);
}

FN_bindinglist*
FNSP_OrgContext::c_list_bindings_nns(const FN_string &name,
    FN_status_csvc& cstat)
{
	unsigned status;
	FN_bindingset *answer = 0;
	FN_ref *ref = resolve(name, cstat);
	if (cstat.is_success()) {
		FNSP_Impl *nns_impl = get_nns_impl(*ref, status);
		if (nns_impl) {
			answer = nns_impl->list_bindings(status);
			delete nns_impl;
			if (status != FN_SUCCESS)
				cstat.set_error(status, *ref, empty_name);
		} else
			cstat.set_error(status, *ref, empty_name);
	}

	delete ref;
	if (answer)
		return (new FN_bindinglist_svc(answer));
	else
		return (0);
}


// Does it make sense to allow bind_nns, given that we hardwire
// where its contexts are stored (in file organization.fns)?  Probably not.
int
FNSP_OrgContext::c_bind_nns(const FN_string &name,
    const FN_ref &,
    unsigned,
    FN_status_csvc &cstat)
{
	// should we do a lookup first so that we can return
	// NotAContext when appropriate?

	FN_ref *nameref = resolve(name, cstat);

	if (cstat.is_success())
		cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *nameref,
		    empty_name);
	// else keep cstat from resolve

	delete nameref;
	return (0);
}

int
FNSP_OrgContext::c_unbind_nns(const FN_string &name, FN_status_csvc &cstat)
{
	cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

int
FNSP_OrgContext::c_rename_nns(const FN_string &name,
    const FN_composite_name &,
    unsigned,
    FN_status_csvc &cstat)
{
	cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_ref *
FNSP_OrgContext::c_create_subcontext_nns(const FN_string &name,
    FN_status_csvc &cstat)
{
	FN_ref *orgref = resolve(name, cstat);
	FN_ref *ref = 0;
	unsigned status;

	if (cstat.is_success() && orgref) {
		FN_string *dirname = 0;
		FNSP_Impl *nns_impl = get_nns_impl(*orgref, status, &dirname);
		if (nns_impl && dirname) {
			unsigned estatus = nns_impl->context_exists();
			switch (estatus) {
			case FN_SUCCESS:
				status = FN_E_NAME_IN_USE;
				// must destroy explicitly first
				break;
			case FN_E_NOT_A_CONTEXT:
				// %%% was context_not_found
				ref = nns_impl->create_context(status, dirname);
				break;
			default:
				// cannot determine state of subcontext
				status = estatus;
			}
			delete nns_impl;
			delete dirname;
			if (status != FN_SUCCESS)
				cstat.set_error(status, *orgref, empty_name);
		} else
			cstat.set_error(status, *ref, empty_name);
	}
	delete orgref;
	return (ref);
}

int
FNSP_OrgContext::c_destroy_subcontext_nns(const FN_string &name,
    FN_status_csvc &cstat)
{
	FN_ref *orgref = resolve(name, cstat);

	if (cstat.is_success()) {
		FN_string *dirname = 0;
		unsigned status;
		FNSP_Impl *nns_impl = get_nns_impl(*orgref, status, &dirname);
		if (nns_impl && dirname) {
			status = nns_impl->destroy_context(dirname);
			delete nns_impl;
			delete dirname;
			if (status != FN_SUCCESS)
				cstat.set_error(status, *orgref, empty_name);
		} else {
			delete nns_impl;
			delete dirname;
			cstat.set(FN_E_INSUFFICIENT_RESOURCES);
		}
	}

	delete orgref;
	return (cstat.is_success());
}

FN_attrset*
FNSP_OrgContext::c_get_syntax_attrs_nns(const FN_string &name,
    FN_status_csvc& cstat)
{
	FN_ref *nns_ref = c_lookup_nns(name, 0, cstat);

	if (cstat.is_success()) {
		FN_attrset* answer =
		    FNSP_Syntax(FNSP_nsid_context)->get_syntax_attrs();
		delete nns_ref;
		if (answer) {
			return (answer);
		}
		cstat.set(FN_E_INSUFFICIENT_RESOURCES);
		return (0);
	}
	return (0);
}

FN_attribute*
FNSP_OrgContext::c_attr_get_nns(const FN_string &name,
    const FN_identifier &, unsigned int,
    FN_status_csvc &cs)
{
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

int
FNSP_OrgContext::c_attr_modify_nns(const FN_string &name,
    unsigned int,
    const FN_attribute&, unsigned int,
    FN_status_csvc &cs)
{
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_valuelist*
FNSP_OrgContext::c_attr_get_values_nns(const FN_string &name,
    const FN_identifier &, unsigned int,
    FN_status_csvc &cs)
{
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_attrset*
FNSP_OrgContext::c_attr_get_ids_nns(const FN_string &name, unsigned int,
    FN_status_csvc &cs)
{
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_multigetlist*
FNSP_OrgContext::c_attr_multi_get_nns(const FN_string &name,
    const FN_attrset *, unsigned int,
    FN_status_csvc &cs)
{
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

int
FNSP_OrgContext::c_attr_multi_modify_nns(const FN_string &name,
    const FN_attrmodlist&, unsigned int,
    FN_attrmodlist**,
    FN_status_csvc &cs)
{
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

// Extended attribute operations
int
FNSP_OrgContext::c_attr_bind(const FN_string &name,
    const FN_ref & /* ref */, const FN_attrset * /* attrs */,
    unsigned int /* exclusive */, FN_status_csvc &status)
{
	// Not supported for ORG context
	status.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_ref *
FNSP_OrgContext::c_attr_create_subcontext(const FN_string &name,
    const FN_attrset * /* attr */, FN_status_csvc &status)
{
	// Not supported for org context
	status.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_searchlist *
FNSP_OrgContext::c_attr_search(const FN_string &name,
    const FN_attrset * /* match_attrs */, unsigned int /* return_ref */,
    const FN_attrset * /* return_attr_ids */, FN_status_csvc &status)
{
	status.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_ext_searchlist *
FNSP_OrgContext::c_attr_ext_search(const FN_string &name,
    const FN_search_control * /* control */,
    const FN_search_filter * /* filter */,
    FN_status_csvc &status)
{
	status.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

int
FNSP_OrgContext::c_attr_bind_nns(const FN_string &name,
    const FN_ref & /* ref */, const FN_attrset * /* attrs */,
    unsigned int /* exclusive */, FN_status_csvc &status)
{
	FN_ref *nameref = resolve(name, status);

	if (status.is_success())
		status.set_error(FN_E_OPERATION_NOT_SUPPORTED, *nameref,
		    empty_name);

	delete nameref;
	return (0);
}

FN_ref *
FNSP_OrgContext::c_attr_create_subcontext_nns(const FN_string &name,
    const FN_attrset *attrs, FN_status_csvc &status)
{
	unsigned context_type;
	unsigned repr_type;
	FN_identifier *ref_type;
	FN_attrset *rest_attr = FNSP_get_create_params_from_attrs(
	    attrs, context_type, repr_type, &ref_type, FNSP_nsid_context);

	if (context_type != FNSP_nsid_context ||
	    repr_type != FNSP_normal_repr ||
	    ref_type != 0 ||
	    (rest_attr != 0 && rest_attr->count() > 0)) {
		status.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference,
		    name);
		delete ref_type;
		delete rest_attr;
		return (0);
	}

	FN_ref *answer = c_create_subcontext_nns(name, status);

	// Cannot add attributes
	delete rest_attr;
	return (answer);
}

FN_searchlist *
FNSP_OrgContext::c_attr_search_nns(const FN_string &name,
    const FN_attrset *match_attrs, unsigned int return_ref,
    const FN_attrset *return_attr_ids, FN_status_csvc &cstat)
{
	FN_ref *ref = resolve(name, cstat);
	unsigned status;
	FN_searchset* matches = NULL;

	if (cstat.is_success()) {
		FNSP_Impl *nns_impl = get_nns_impl(*ref, status);
		if (nns_impl) {
			matches = nns_impl->search_attrset(
			    match_attrs, return_ref, return_attr_ids,
			    status);
			delete nns_impl;
			// nns context not there -> '/' not found
			if (status == FN_E_NOT_A_CONTEXT)
				// %%% was CONTEXT_NOT_FOUND
				status = FN_E_NAME_NOT_FOUND;
			if (status != FN_SUCCESS)
				cstat.set_error(status, *ref, empty_name);
			else
				cstat.set_success();
		} else
			cstat.set_error(status, *ref, empty_name);
	}

	delete ref;
	if (matches)
		return (new FN_searchlist_svc(matches));
	else
		return (0);
}

FN_ext_searchlist *
FNSP_OrgContext::c_attr_ext_search_nns(const FN_string &name,
    const FN_search_control * /* control */,
    const FN_search_filter * /* filter */,
    FN_status_csvc &status)
{
	status.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}
