/*
 * Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident "@(#)bsd-functions.h	1.2	96/04/22 SMI"

#ifndef _BSD_FUNCTIONS_H
#define _BSD_FUNCTIONS_H

extern int bsd_queue(ns_bsd_addr_t *binding, int format, int ac, char *av[]);
extern int vprint_job(job_t *job, va_list ap);
extern int vjob_count(job_t *job, va_list ap);
extern void clear_screen();

#endif /* _BSD_FUNCTIONS_H */
