/*
 * Copyright (c) 1994 Sun Microsystems, Inc.
 * All rights reserved.
 *
 * The I/F's described herein are expermental, highly volatile and
 * intended at this time only for use with Sun internal products.
 * SunSoft reserves the right to change these definitions in a minor
 * release.
 */

#ifndef	_SYS_DOOR_DATA_H
#define	_SYS_DOOR_DATA_H

#pragma ident	"@(#)door_data.h	1.5	96/01/16 SMI"

#include <sys/types.h>
#include <sys/door.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(_KERNEL)
/*
 * Data associated with a door invocation
 */
struct _kthread;
struct door_node;
struct file;

typedef struct door_data {
	door_arg_t	d_args;		/* Door arg/results */
	struct _kthread	*d_caller;	/* Door caller */
	struct _kthread *d_servers;	/* List of door servers */
	struct door_node *d_active;	/* Active door */
	struct door_node *d_pool;	/* Private pool of server threads */
	caddr_t		d_sp;		/* Saved thread stack base */
	caddr_t		d_buf;		/* Temp buffer for data transfer */
	int		d_bufsize;	/* Size of temp buffer */
	int		d_error;	/* Error (if any) */
	int		d_fpp_size;	/* Number of File ptrs */
	struct file	**d_fpp;	/* File ptrs  */
	u_char		d_upcall;	/* Kernel level upcall */
	u_char		d_noresults;	/* No results allowed */
	u_char		d_overflow;	/* Result overflow occured */
	u_char		d_flag;		/* State */
	kcondvar_t	d_cv;
} door_data_t;

/* flag values */
#define	DOOR_HOLD	0x01		/* Hold on to client/server */
#define	DOOR_WAITING	0x02		/* Client/server is waiting */

/*
 * Roundup buffer size when passing/returning data via kernel buffer.
 * This cuts down on the number of overflows that occur on return
 */
#define	DOOR_ROUND	128

#endif	/* defined(_KERNEL) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DOOR_DATA_H */
