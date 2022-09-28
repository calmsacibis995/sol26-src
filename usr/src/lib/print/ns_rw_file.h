/*
 * Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident "@(#)ns_rw_file.h 1.4	96/04/22 SMI"

#ifndef _NS_RW_FILE_H
#define _NS_RW_FILE_H

extern ns_printer_t *_file_get_name(const char *,const char *,
				       ns_printer_t *(*)(char *, char *),
				       char *);
extern ns_printer_t **_file_get_list(const char *,
				     ns_printer_t *(*)(char *, char *),
				     char *);
extern int          _file_put_printer(const char *, const ns_printer_t *,
				      ns_printer_t *(*)(char *, char *),
				      char *, char *(*)(ns_printer_t *));

#endif /* _NS_RW_FILE_H */
