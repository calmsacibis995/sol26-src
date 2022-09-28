/*
 * Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident "@(#)ns_rw_xfn.h 1.2	96/04/22 SMI"

#ifndef _NS_RW_XFN_H
#define _NS_RW_XFN_H

extern ns_printer_t *_xfn_get_name(const char *,
				    ns_printer_t *(*)(char *,FN_ref_t *,
						      char *),
				    char *);
extern ns_printer_t **_xfn_get_list(ns_printer_t *(*)(char *, FN_ref_t *,
						      char *),
				    char *);

extern int            _xfn_put_printer(const ns_printer_t *,
				    ns_printer_t *(*)(char *, FN_ref_t *,
						      char *),
				    char *, FN_ref_t *(*)(ns_printer_t *));

#endif /* _NS_RW_XFN_H */
