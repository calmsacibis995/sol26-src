/*
 * Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident "@(#)ns_cvt_xfn.h	1.2	96/04/22 SMI"

#ifndef _NS_CVT_XFN_H
#define _NS_CVT_XFN_H

extern ns_printer_t *_cvt_xfn_entry_to_printer(char *,FN_ref_t *, char *);
extern FN_ref_t *_cvt_printer_to_xfn_entry(ns_printer_t *);

#endif /* _NS_CVT_XFN_H */
