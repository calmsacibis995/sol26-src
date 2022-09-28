/*
 * Copyright (c) 1993,1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SYNCH_H
#define	_SYS_SYNCH_H

#pragma ident	"@(#)synch.h	1.26	96/07/24 SMI"

#include <sys/types.h>
#include <sys/int_types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Thread and LWP mutexes have the same type
 * definitions.
 */
typedef struct _lwp_mutex {
	struct _mutex_flags {
		uint8_t		flag[4];
		uint16_t	type;
		uint16_t	magic;
	} flags;
	union _mutex_lock_un {
		struct _mutex_lock {
			uint8_t	pad[8];
		} lock64;
		upad64_t owner64;
	} lock;
	upad64_t data;
} lwp_mutex_t;

#define	mutex_lockw	lock.lock64.pad[7]

/*
 * Thread and LWP condition variables have the same
 * type definition.
 */
typedef struct _lwp_cond {
	struct _lwp_cond_flags {
		uint8_t		flag[4];
		uint16_t 	type;
		uint16_t 	magic;
	} flags;
	upad64_t data;
} lwp_cond_t;


/*
 * LWP semaphores
 */

typedef struct _lwp_sema {
	uint32_t	count;		/* semaphore count */
	uint32_t	type;
	uint8_t		flags[8];	/* last byte reserved for waiters */
	upad64_t	data;		/* optional data */
} lwp_sema_t;

/*
 * Definitions of synchronization types.
 */
#define	USYNC_THREAD	0		/* private to a process */
#define	USYNC_PROCESS	1		/* shared by processes */
#define	TRACE_TYPE	2

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_SYNCH_H */
