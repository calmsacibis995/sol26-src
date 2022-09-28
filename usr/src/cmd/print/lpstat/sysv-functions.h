/*	Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.	*/
/*	All Rights Reserved						*/

#ifndef	_SYSV_FUNCTIONS_H
#define	_SYSV_FUNCTIONS_H

#pragma ident	"@(#)sysv-functions.h	1.4	96/08/23 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

extern void vsysv_queue_entry(job_t *job, va_list ap);
extern int sysv_queue_state(print_queue_t *qp, char *printer, int verbose,
		int description);
extern int sysv_accept(ns_bsd_addr_t *binding);
extern int sysv_system(ns_bsd_addr_t *binding);
extern void sysv_running();
extern void sysv_default();
extern int sysv_local_status(char *, char *, int, char *);
extern print_queue_t *sysv_get_queue(ns_bsd_addr_t *binding, int local);

#ifdef	__cplusplus
}
#endif

#endif /* _SYSV_FUNCTIONS_H */
