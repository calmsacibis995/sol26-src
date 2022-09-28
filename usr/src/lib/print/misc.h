/*
 * Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */
#ifndef	_MISC_H
#define	_MISC_H

#pragma ident	"@(#)misc.h	1.5	96/04/23 SMI"


#ifdef __cplusplus
extern "C" {
#endif

/*
 *  so code could be built on 4.X with minimal changes.
 */
#ifdef SUNOS_4
#ifndef	MAP_FAILED
#define	MAP_FAILED	((void *) -1)
#endif
#define gettext		(char *)
extern char *strtok_r(char *, char *, char **);
extern char *cftime(char *buf, char *fmt, time_t *curr);
extern char *index(char *a, char b);
extern char *rindex(char *a, char b);
#endif


/* Protocol Defined Requests */
#define PRINT_REQUEST	    1	/* \1printer\n */
#define XFER_REQUEST	    2	/* \2printer\n */
#define     XFER_CLEANUP	1 	/* \1 */
#define     XFER_CONTROL	2	/* \2size name\n */
#define     XFER_DATA		3	/* \3size name\n */

#define SHOW_QUEUE_SHORT_REQUEST  3	/* \3printer [users|jobs ...]\n */
#define SHOW_QUEUE_LONG_REQUEST  4	/* \4printer [users|jobs ...]\n */
#define REMOVE_REQUEST	    5	/* \5printer person [users|jobs ...]\n */

#define ACK_BYTE	0
#define NACK_BYTE	1

#define MASTER_NAME	"printd"
#define MASTER_LOCK	"/tmp/.printd.lock"
#define MASTER_PID	"/tmp/.printd.pid"
#define SPOOL_DIR	"/var/spool/print"
#define TBL_NAME	"printers.conf"


extern char *long_date();
extern char *short_date();
extern int check_client_spool(char *printer);
extern int get_lock(char *name, int write_pid);
extern uid_t get_user_id();
extern char *get_user_name();
extern char *strcdup(char *, char);
extern char *strndup(char *, int);
extern char **strsplit(char *, char *);
extern int  file_size(char *);
extern int  copy_file(char *src, char *dst);
extern int  map_in_file(const char *name, char **buf);
extern int  write_buffer(char *name, char *buf, int len);
extern void start_daemon(int do_fork);
extern int  kill_process(char *file);
extern void *dynamic_function(const char *, const char *);

#ifdef __cplusplus
}
#endif

#endif /* _MISC_H */  
