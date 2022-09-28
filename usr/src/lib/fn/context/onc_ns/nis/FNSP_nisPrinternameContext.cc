/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_nisPrinternameContext.cc	1.5	96/10/22 SMI"

#include <sys/types.h>
#include <rpcsvc/ypclnt.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <xfn/fn_p.hh>
#include "FNSP_nisPrinternameContext.hh"
#include "FNSP_nisImpl.hh"
#include "fnsp_nis_internal.hh"
#include "fnsp_internal_common.hh"

#define	CSIZE 1024
#define	NISSUFFIX "printers.conf.byname"
#define	NISSUFLEN (12)

static FN_string null_name((unsigned char *) "");
static FN_string internal_name((unsigned char *) "printers");

FNSP_nisPrinternameContext::~FNSP_nisPrinternameContext()
{
	delete domain_name;
	delete ns_impl;
}

FNSP_nisPrinternameContext::FNSP_nisPrinternameContext(
    const FN_ref_addr &from_addr, const FN_ref &from_ref)
	: FNSP_PrinternameContext(from_addr, from_ref, 0)
{
	// check if fns is installed
	FNSP_nisAddress *my_address = new FNSP_nisAddress(from_addr);
	if (FNSP_is_fns_installed(&from_addr)) {
		FNSP_nisImpl *nis_impl = new FNSP_nisImpl(my_address);
		ns_impl = nis_impl;
		FN_string *map;
		// Check if it a context or binding
		char *domain;
		yp_get_default_domain(&domain);
		if (strcmp(domain, (char *)
		    (my_address->get_internal_name()).str())
		    == 0) {
			domain_name = new FN_string((unsigned char *)
			    domain);
			fns_org_context = 1;
		} else {
			split_internal_name(my_address->get_table_name(),
			    &map, &domain_name);
			delete map;
			fns_org_context = nis_impl->is_org_context();
		}
	} else {
		domain_name = new FN_string(my_address->get_internal_name());
		delete my_address;
		ns_impl = 0;
		fns_org_context = 1;
	}
}

FNSP_nisPrinternameContext*
FNSP_nisPrinternameContext::from_address(const FN_ref_addr &from_addr,
    const FN_ref &from_ref, FN_status &stat)
{
	FNSP_nisPrinternameContext *answer =
	    new FNSP_nisPrinternameContext(from_addr, from_ref);

	if ((answer) && (answer->my_reference))
		stat.set_success();
	else {
		if (answer) {
			delete answer;
			answer = 0;
		}
		stat.set(FN_E_INSUFFICIENT_RESOURCES);
	}
	return (answer);
}

FN_ref*
FNSP_nisPrinternameContext::resolve(const FN_string &aname,
    FN_status_csvc& cs)
{
	FN_ref *ref_files, *ref_nis, *ref_ns;
	char mapname[CSIZE], *value;
	int len;

	// Lookup names from all the naming services
	ref_files = FNSP_PrinterContext::resolve(aname, cs);

	// Lookup in printers.conf.byname map
	// if it is org's printer context
	if (fns_org_context) {
		sprintf(mapname, "%s", NISSUFFIX);
		char *domain = (char *) domain_name->str();
		if (yp_match(domain, mapname, (char *)aname.str(),
		    aname.charcount(), &value, &len) == 0) {
			value[len] = '\0'; // replace \n
			ref_nis = get_service_ref_from_value(
			    internal_name, value);
			free(value);
			cs.set_success();
		} else
			ref_nis = 0;
	} else
		ref_nis = 0;
	
	// Lookup in NIS tables if FNS is installed
	if (fns_installed()) {
		unsigned status;
		ref_ns = ns_impl->lookup_binding(aname, status);
		if (status == FN_SUCCESS)
			cs.set_success();
	} else
		ref_ns = 0;

	if (!ref_files && !ref_nis && !ref_ns) {
		cs.set_error(FN_E_NAME_NOT_FOUND, *my_reference, aname);
		return (0);
	}

	// If the address is present in files and not in naming service
	// In this case, we do not care about ref_nis
	if (ref_files && !ref_ns) {
		delete ref_nis;
		return (ref_files);
	}

	// Address is present only in NIS map (ref_nis)
	if (!ref_files && ref_nis && !ref_ns)
		return (ref_nis);

	// Addess is present only in the naming service (ref_ns)
	if (!ref_files && !ref_nis && ref_ns)
		return (ref_ns);

	// Possible combinations
	// 1. Address present in files, NIS and naming service
	// 2. Address present in files and naming service
	// 3. Address present in NIS and naming service
	// Now, combine the addresses
	FN_ref *target_ref;
	const FN_ref_addr *address;
	const FN_identifier *type;
	void *ip;
	if (ref_files) {
		target_ref = ref_files;
		delete ref_nis;
	} else
		target_ref = ref_nis;
	for (address = ref_ns->first(ip); address;
	     address = ref_ns->next(ip)) {
		type = address->type();
		if (strncmp((char *) type->str(), "onc_fn_printer_",
		    strlen("onc_fn_printer_")) == 0)
			target_ref->prepend_addr(*address);
	}

	delete ref_ns;
	return (target_ref);
}

FN_nameset*
FNSP_nisPrinternameContext::list(FN_status_csvc &cs)
{
	FN_nameset *ns;
	char mapname[CSIZE], *inkey, *outkey, *val;
	int inkeylen, outkeylen, vallen, ret;

	ns = FNSP_PrinterContext::list(cs);
	if (ns == 0)
		ns = new FN_nameset;

	if (fns_org_context) {
		if (NISSUFLEN >= CSIZE) {
			cs.set_error(FN_E_PARTIAL_RESULT,
			    *my_reference, null_name);
			return (ns);
		}

		sprintf(mapname, "%s", NISSUFFIX);
		char *domain = (char *) domain_name->str();
		ret = yp_first(domain, mapname, &inkey, &inkeylen,
		    &val, &vallen);
		if (ret) {
			cs.set_error(FN_E_PARTIAL_RESULT,
			    *my_reference, null_name);
			return (ns);
		}
		inkey[inkeylen] = '\0'; // replace \n
		ns->add((unsigned char *)inkey);
		free(val);

		while (yp_next(domain, mapname, inkey, inkeylen,
		    &outkey, &outkeylen, &val, &vallen) == 0) {
			outkey[outkeylen] = '\0'; // replace \n
			ns->add((unsigned char *)outkey);
			free(inkey);
			free(val);
			inkey = outkey;
			inkeylen = outkeylen;
		}
		free(inkey);
	}
	cs.set_success();
	return (ns);
}

FN_bindingset*
FNSP_nisPrinternameContext::list_bs(FN_status_csvc &cs)
{
	FN_bindingset *bs;
	FN_ref *ref;
	char mapname[CSIZE], *inkey, *outkey, *val;
	int inkeylen, outkeylen, vallen, ret;

	bs = FNSP_PrinterContext::list_bs(cs);
	if (bs == 0)
		bs = new FN_bindingset;

	if (fns_org_context) {
		if ((internal_name.charcount() + NISSUFLEN) >= CSIZE) {
			cs.set_error(FN_E_PARTIAL_RESULT,
			    *my_reference, null_name);
			return (bs);
		}

		sprintf(mapname, "%s", NISSUFFIX);
		char *domain = (char *) domain_name->str();
		ret = yp_first(domain, mapname, &inkey, &inkeylen,
		    &val, &vallen);
		if (ret) {
			cs.set_error(FN_E_PARTIAL_RESULT,
			    *my_reference, null_name);
			return (bs);
		}
		val[vallen] = '\0'; // replace \n
		ref = get_service_ref_from_value(internal_name, val);
		free(val);
		if (ref) {
			inkey[inkeylen] = '\0'; // replace \n
			bs->add((unsigned char *)inkey, *ref);
			free(ref);
		}
		while (yp_next(domain, mapname, inkey, inkeylen,
		    &outkey, &outkeylen, &val, &vallen) == 0) {
			val[vallen] = '\0'; // replace \n
			ref = get_service_ref_from_value(internal_name, val);
			free(inkey);
			free(val);
			if (ref) {
				outkey[outkeylen] = '\0'; // replace \n
				bs->add((unsigned char *)outkey, *ref);
				free(ref);
			}
			inkey = outkey;
			inkeylen = outkeylen;
		}
		free(inkey);
	}
	cs.set_success();
	return (bs);
}

int
FNSP_nisPrinternameContext::c_bind(const FN_string &name,
    const FN_ref &ref, unsigned excl, FN_status_csvc &cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED,
		    *my_reference, name);
		return (0);
	}
	return (FNSP_PrinternameContext::c_bind(name, ref, excl, cs));
}

int
FNSP_nisPrinternameContext::c_unbind(const FN_string &name,
    FN_status_csvc& cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED,
		    *my_reference, name);
		return (0);
	}
	return (FNSP_PrinternameContext::c_unbind(name, cs));
}

int
FNSP_nisPrinternameContext::c_rename(const FN_string &name,
    const FN_composite_name &newname, unsigned rflags,
    FN_status_csvc &cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED,
		    *my_reference, name);
		return (0);
	}
	return (FNSP_PrinternameContext::c_rename(name, newname,
	    rflags, cs));
}

FN_ref*
FNSP_nisPrinternameContext::c_create_subcontext(const FN_string &name,
    FN_status_csvc &cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
		return (0);
	}
	return (FNSP_PrinternameContext::c_create_subcontext(name, cs));
}

int
FNSP_nisPrinternameContext::c_destroy_subcontext(const FN_string &name,
    FN_status_csvc &cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
		return (0);
	}
	return (FNSP_PrinternameContext::c_destroy_subcontext(name, cs));
}


FN_attribute*
FNSP_nisPrinternameContext::c_attr_get(const FN_string &name,
    const FN_identifier &id, unsigned int follow_link,
    FN_status_csvc &cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
		return (0);
	}
	return (FNSP_PrinternameContext::c_attr_get(name, id, follow_link, cs));
}

int
FNSP_nisPrinternameContext::c_attr_modify(const FN_string &aname,
    unsigned int flags, const FN_attribute &attr,
    unsigned int follow_link, FN_status_csvc& cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED,
		    *my_reference, aname);
		return (0);
	}
	return (FNSP_PrinternameContext::c_attr_modify(aname,
	    flags, attr, follow_link, cs));
}

FN_valuelist*
FNSP_nisPrinternameContext::c_attr_get_values(const FN_string &name,
    const FN_identifier &id, unsigned int follow_link, FN_status_csvc &cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
		return (0);
	}
	return (FNSP_PrinternameContext::c_attr_get_values(name,
	    id, follow_link, cs));
}

FN_attrset*
FNSP_nisPrinternameContext::c_attr_get_ids(const FN_string &name,
    unsigned int follow_link, FN_status_csvc &cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
		return (0);
	}
	return (FNSP_PrinternameContext::c_attr_get_ids(name,
	    follow_link, cs));
}

FN_multigetlist*
FNSP_nisPrinternameContext::c_attr_multi_get(const FN_string &name,
    const FN_attrset *attrset, unsigned int follow_link, FN_status_csvc &cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
		return (0);
	}
	return (FNSP_PrinternameContext::c_attr_multi_get(name,
	    attrset, follow_link, cs));
}

int
FNSP_nisPrinternameContext::c_attr_multi_modify(const FN_string &name,
    const FN_attrmodlist &modlist, unsigned int follow_link,
    FN_attrmodlist **un_modlist,
    FN_status_csvc &cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
		return (0);
	}
	return (FNSP_PrinternameContext::c_attr_multi_modify(name,
	    modlist, follow_link, un_modlist, cs));
}


// Extended attibute operations
int
FNSP_nisPrinternameContext::c_attr_bind(const FN_string &name,
    const FN_ref &ref,
    const FN_attrset *attrs,
    unsigned BindFlags, FN_status_csvc &cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
		return (0);
	}
	return (FNSP_PrinternameContext::c_attr_bind(name, ref, attrs,
	    BindFlags, cs));
}

FN_ref *
FNSP_nisPrinternameContext::c_attr_create_subcontext(const FN_string &name,
    const FN_attrset *attrs, FN_status_csvc &cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
		return (0);
	}
	return (FNSP_PrinternameContext::c_attr_create_subcontext(
	    name, attrs, cs));
}

FN_searchlist *
FNSP_nisPrinternameContext::c_attr_search(const FN_string &name,
    const FN_attrset *match_attrs,
    unsigned int return_ref,
    const FN_attrset *return_attr_ids,
    FN_status_csvc &cs)
{
	if (!fns_installed()) {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
		return (0);
	}
	return (FNSP_PrinternameContext::c_attr_search(name,
	    match_attrs, return_ref, return_attr_ids, cs));
}
