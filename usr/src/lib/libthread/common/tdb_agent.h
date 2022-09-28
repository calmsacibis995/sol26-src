/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_TDB_AGENT_H
#define	_TDB_AGENT_H

#pragma ident	"@(#)tdb_agent.h	1.7	96/07/24 SMI"

/*
 * Thread debug agent control structures.
 */

#include <thread_db.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Status of libthread_db attachment to this process.
 */
typedef enum {
	TDB_NOT_ATTACHED,	/* must be default: no libthread_db */
	TDB_START_AGENT,	/* tdb attaching:  please start agent */
	TDB_ATTACHED		/* tdb attached; agent started */
} tdb_agt_stat_t;

/*
 * This enumerates the request codes for requests that libthread_db
 * makes to the thread_db agent in the target process.
 */
typedef enum {
	NONE_PENDING,		/* Placeholder */
	THREAD_SUSPEND,		/* td_thr_dbsuspend */
	THREAD_RESUME,		/* td_thr_dbcontinue */
	THREAD_ALLOC_EVENT_STRUCT	/* td_thr_event_enable */
} tdb_opcode_t;

/*
 * This structure contains target process data pertaining to the
 * target process's tdb agent thread.  The agent thread is ready to
 * accept requests when the agent_ready field is non-zero.  After
 * that, libthread_db need not read this structure again.
 */
typedef struct {
	int agent_ready;
	lwpid_t agent_lwpid;
	paddr_t agent_go_addr;
	paddr_t agent_stop_addr;
} tdb_agent_data_t;

typedef struct {
	tdb_opcode_t opcode;
	int result;
	union {
		uthread_t *thr_p;
		int conc_lvl;
	} u;
} tdb_ctl_t;

#define	TDB_SYNC_DESC_HASHSIZE 256

/*
 * An entry in the sync. object registry.
 */
typedef struct _sync_desc {
	struct _sync_desc *next;
	struct _sync_desc *prev;
	int sync_magic;
	caddr_t sync_addr;
} tdb_sync_desc_t;

/*
 * This structure contains target process data that is valid as soon
 * as the startup rtld work is done (e.g., function addresses); the
 * controlling process reads it from the target process once, at attach
 * time.
 */
typedef struct {
	paddr_t tdb_stats_addr;
	paddr_t tdb_stats_enable_addr;
	paddr_t tdb_eventmask_addr;
	paddr_t sync_desc_hash_addr;
	paddr_t nthreads_addr;
	paddr_t nlwps_addr;
	paddr_t tdb_nlwps_req_addr;
	paddr_t tdb_agent_stat_addr;
	paddr_t allthreads_addr;
	paddr_t aslwp_id_addr;
	paddr_t tsd_common_addr;
	paddr_t tdb_agent_data_addr;
	paddr_t tdb_agent_ctl_addr;
	void (*tdb_events[TD_MAX_EVENT_NUM + 1])();
} tdb_invar_data_t;

extern td_thr_events_t __tdb_event_global_mask;
extern int __tdb_stats_enabled;
extern int __tdb_nlwps_req;
extern tdb_agt_stat_t __tdb_attach_stat;	/* is libthread_db attached? */

#define	__td_event_report(t, eventnum)		\
	((t)->t_td_evbuf != NULL && \
	td_eventismember(&(t)->t_td_evbuf->eventmask, TD_EVENTS_ENABLE) && \
	(td_eventismember(&(t)->t_td_evbuf->eventmask, (eventnum)) || \
	td_eventismember(&__tdb_event_global_mask, (eventnum))))

extern void _tdb_sync_obj_register(caddr_t obj, int type);
extern void _tdb_sync_obj_deregister(caddr_t obj);

#ifdef __cplusplus
}
#endif

#endif	/* _TDB_AGENT_H */
