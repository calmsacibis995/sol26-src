/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1994, 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_FLOCK_H
#define	_SYS_FLOCK_H

#pragma ident	"@(#)flock.h	1.28	96/04/19 SMI"

#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/t_lock.h>		/* for <sys/callb.h> */
#include <sys/callb.h>
#include <sys/param.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Private declarations and instrumentation for local locking.
 */

/*
 * The command passed to reclock() is made by ORing together one or more of
 * the following values.
 */

#define	INOFLCK		1	/* Vnode is locked when reclock() is called. */
#define	SETFLCK		2	/* Set a file lock. */
#define	SLPFLCK		4	/* Wait if blocked. */
#define	RCMDLCK		8	/* RGETLK/RSETLK/RSETLKW specified */

/*
 * Special pid value that can be passed to cleanlocks().  It means that
 * cleanlocks() should flush all locks for the given sysid, not just the
 * locks owned by a specific process.
 */

#define	IGN_PID		(-1)

/* file locking structure (connected to vnode) */

#define	l_end		l_len

/*
 * The lock manager is allowed to use unsigned offsets and lengths, though
 * regular Unix processes are still required to use signed offsets and
 * lengths.
 */
typedef ulong_t u_off_t;

#define	MAX_U_OFF_T	((u_off_t) ~0)
#define	MAX_U_OFFSET_T	((u_offset_t) ~0)

/*
 * define MAXEND as the largest positive value the signed offset_t will hold.
 */
#define	MAXEND		MAXOFFSET_T

/*
 * Definitions for accessing the l_pad area of struct flock.  The
 * descriminant of the pad_info_t union is the fcntl command used in
 * conjunction with the flock struct.
 */

typedef struct {
	callb_cpr_t	*(*cb_callback)(void *); /* callback function */
	void		*cb_cbp;	/* ptr to callback data */
} flk_callback_t;

typedef union {
	long	pi_pad[4];		/* (original pad area) */
	int	pi_has_rmt;		/* F_HASREMOTELOCKS */
	flk_callback_t pi_cback;	/* F_RSETLKW */
} pad_info_t;

#define	l_has_rmt(flockp)	(((pad_info_t *)((flockp)->l_pad))->pi_has_rmt)
#define	l_callback(flockp)	\
		(((pad_info_t *)((flockp)->l_pad))->pi_cback.cb_callback)
#define	l_cbp(flockp)	(((pad_info_t *)((flockp)->l_pad))->pi_cback.cb_cbp)

/*
 * This structure members are not used any more inside the kernel.
 * The structure is used for casting some pointer assignments only.
 */

typedef struct filock {
	kcondvar_t cv;
	struct	flock set;	/* contains type, start, and end */
	struct	{
		int granted_flag;	/* granted flag */
		struct filock *blk;	/* for sleeping locks only */
		struct attacher *blocking_list;
		struct attacher *my_attacher;
	}	stat;
	struct	filock *prev;
	struct	filock *next;
} filock_t;

#define	FLP_DELAYED_FREE	-1	/* special value for granted_flag */

/* file and record locking configuration structure */
/* record use total may overflow */
struct flckinfo {
	long reccnt;	/* number of records currently in use */
	long rectot;	/* number of records used since system boot */
};

/* structure that contains list of locks to be granted */

#define	MAX_GRANT_LOCKS		52

typedef struct grant_lock {
	struct filock *grant_lock_list[MAX_GRANT_LOCKS];
	struct grant_lock *next;
} grant_lock_t;


/*
 * Provide a way to cleanly enable and disable lock manager locking
 * requests (i.e., requests from remote clients).  FLK_WAKEUP_SLEEPERS
 * forces all blocked lock manager requests to bail out and return ENOLCK.
 * FLK_LOCKMGR_DOWN clears all granted lock manager locks.  Both status
 * codes cause new lock manager requests to fail immediately with ENOLCK.
 */

typedef enum {
    FLK_LOCKMGR_UP,
    FLK_WAKEUP_SLEEPERS,
    FLK_LOCKMGR_DOWN
} flk_lockmgr_status_t;

/*
 * The following structure is used to hold a list of locks returned
 * by the F_ACTIVELIST or F_SLEEPINGLIST commands to fs_frlock.
 *
 * N.B. The lists returned by these commands are dynamically
 * allocated and must be freed by the caller.  The vnodes returned
 * in the lists are held and must be released when the caller is done.
 */

#if defined(_KERNEL)

typedef struct locklist {
	struct vnode *ll_vp;
	struct flock64 ll_flock;
	struct locklist *ll_next;
} locklist_t;



extern struct flckinfo	flckinfo;

int	reclock(struct vnode *, struct flock64 *, int, int, u_offset_t);
void	kill_proc_locks(struct vnode *, struct flock64 *);
int	chklock(struct vnode *, int, u_offset_t, int, int);
int	convoff(struct vnode *, struct flock64 *, int, offset_t);
int	cleanlocks(struct vnode *, pid_t, sysid_t);
locklist_t *flk_get_sleeping_locks(long sysid, pid_t pid);
locklist_t *flk_get_active_locks(long sysid, pid_t pid);
int	flk_convert_lock_data(struct vnode *, struct flock64 *,
		u_offset_t *, u_offset_t *, offset_t, offset_t);
int	flk_check_lock_data(u_offset_t, u_offset_t, offset_t);
int	flk_has_remote_locks(struct vnode *vp);
int	flk_vfs_has_locks(struct vfs *vfsp);
void	flk_set_lockmgr_status(flk_lockmgr_status_t status);
int	flk_sysid_has_locks(long sysid);
#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FLOCK_H */
