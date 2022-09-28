/*
 * Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident "@(#)ns_rw_nis.h 1.3	96/04/22 SMI"

#ifndef _NS_RW_NIS_H
#define _NS_RW_NIS_H

extern ns_printer_t *_nis_get_name(const char *,const char *,
				      ns_printer_t *(*)(char *, char *),
				      char *);
extern ns_printer_t **_nis_get_list(const char *,
				    ns_printer_t *(*)(char *, char *), char *);
extern int          _nis_put_printer(const char *, const ns_printer_t *,
				     ns_printer_t *(*)(char *, char *), char *,
				     char *(*)(ns_printer_t *));

#endif /* _NS_RW_NIS_H */
