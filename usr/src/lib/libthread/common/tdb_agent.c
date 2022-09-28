/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)tdb_agent.c	1.13	96/07/24	SMI"

/*
 * This file contains most of the functionality added to libthread to
 * support libthread_db; particularly, the "agent thread" that is
 * created on demand by libthread_db, and that executes code at
 * libthread_db's request.
 */

#include "libthread.h"
#include "tdb_agent.h"
#include <sys/time.h>

/*
 * The "agent control" structure is used to store an op code and argument
 * for the agent thread, and collect the result.
 */
static volatile tdb_ctl_t __tdb_agent_ctl;

/*
 * The "agent data" structure stores information needed by libthread_db
 * to access and run the agent thread.  It needs to be volatile to
 * prevent the optimizer from messing up the control flow in the agent.
 */
volatile tdb_agent_data_t __tdb_agent_data;

/*
 * The threading statistics structure filled in when libthread_db enables
 * statistics gathering.
 */
static td_ta_stats_t __tdb_stats;

int __tdb_stats_enabled;		/* Is statistics gathering enabled? */

/*
 * When libthread_db wants to increase the number of LWP's
 * (td_ta_setconcurrency), it sets the value in this variable.
 * Every time a thread switch occurs, this variable gets checked;
 * if its value is larger than the actual number of LWP's, new ones
 * get created.
 */
int __tdb_nlwps_req;

/*
 * The set of globally enabled events to report to libthread_db.
 */
td_thr_events_t __tdb_event_global_mask;

/*
 * When the agent thread is not executing, it parks on this semaphore.
 */
static lwp_sema_t blocksem;

/*
 * Set to non-0 by libthread_db when it attaches; this (plus goosing
 * the ASLWP with a SIGLWP will cause the libthread_db agent thread
 * to be created.
 */
tdb_agt_stat_t __tdb_attach_stat;

/*
 * Statistics gathering requires that we keep track of LWP virtual time
 * for each LWP in the process.  (What we really want to keep track of
 * is *process* virtual time -- the sum of all the LWP times -- but that's
 * not available.)  This structure stores the virtual times for up to
 * 63 LWPs.  If there are more LWP's, we chain additional structures off
 * this one.
 */
#define	LWPTIMESPERBLOCK 63
typedef struct _lwpvtimes {
	struct _lwpvtimes *next;
	hrtime_t lasttime[LWPTIMESPERBLOCK];
} lwpvtimes_t;

static lwpvtimes_t lwpvtimes;

/*
 * This is a pool from which we allocate sync_desc_t structures.
 * The sync_desc_t structures are used to keep a catalogue of the
 * synchronization objects (mutexes, condition variables, semaphores,
 * r-w locks) in use.
 */
#define	SYNCDESCPOOLSIZE 63
typedef struct _sync_desc_pool {
	int next_avail;
	tdb_sync_desc_t pool[SYNCDESCPOOLSIZE];
} sync_desc_pool_t;

/*
 * The free list of sync_desc_t descriptors.  We obtain sync_desc_t
 * descriptors in 3 ways:
 *	(1) peel one off the free list;
 *	(2) if the free list is empty, grab one from the current pool;
 *	(3) if the current pool is used up, allocate a new pool.
 */
static tdb_sync_desc_t *sync_desc_free;

/*
 * The hash table of sync_desc_t descriptors.
 */
static tdb_sync_desc_t *sync_desc_hash[TDB_SYNC_DESC_HASHSIZE];

/*
 * The current sync_desc_t pool.
 */
static sync_desc_pool_t *sync_desc_cur_pool;


/*
 * A pool of thread event buffers.  These hold both the thread's
 * current event-enable mask and the most recent event message,
 * if any.
 */
#define	EVENTBUFPOOLSIZE 63
typedef struct _td_eventbuf_pool {
	int next_avail;
	td_eventbuf_t pool[EVENTBUFPOOLSIZE];
} td_eventbuf_pool_t;

/*
 * The current event buffer pool.
 */
static td_eventbuf_pool_t *event_buf_cur_pool;

/*
 * The event buffer free list.
 */
static td_eventbuf_t *event_buf_free;

/*
 * Event "reporting" functions.  A thread reports an event by calling
 * one of these empty functions; a debugger can set a breakpoint
 * at the address of any of these functions to determine that an
 * event is being reported.
 *
 */

void
	tdb_event_ready(void),
	tdb_event_sleep(void),
	tdb_event_switchto(void),
	tdb_event_switchfrom(void),
	tdb_event_lock_try(void),
	tdb_event_catchsig(void),
	tdb_event_idle(void),
	tdb_event_create(void),
	tdb_event_death(void),
	tdb_event_preempt(void),
	tdb_event_pri_inherit(void),
	tdb_event_reap(void),
	tdb_event_concurrency(void),
	tdb_event_timeout(void);

/*
 * The set of compile-time-constant data of interest to libthread_db;
 * read by libthread_db only once.
 */
tdb_invar_data_t __tdb_invar_data = {
	(paddr_t) &__tdb_stats,		/* stats structure */
	(paddr_t) &__tdb_stats_enabled,	/* stats gathering on? */
	(paddr_t) &__tdb_event_global_mask, /* Global tdb event mask */
	(paddr_t) sync_desc_hash,	/* sync. obj. hash table */
	(paddr_t) &_nthreads,		/* total thread count */
	(paddr_t) &_nlwps,		/* lwp count */
	(paddr_t) &__tdb_nlwps_req,	/* lwps requested by libthread_db */
	(paddr_t) &__tdb_attach_stat,	/* Please start the TDB agent thread */
	(paddr_t) &_allthreads,		/* hash table of threads */
	(paddr_t) &__dynamic_lwpid,	/* lwpid of the ASLWP */
	(paddr_t) &tsd_common,		/* TSD common structure */
	(paddr_t) &__tdb_agent_data,	/* data about tdb agent */
	(paddr_t) &__tdb_agent_ctl,	/* agent request/response struct */
	{				/* event functions */
		tdb_event_ready,
		tdb_event_sleep,
		tdb_event_switchto,
		tdb_event_switchfrom,
		tdb_event_lock_try,
		tdb_event_catchsig,
		tdb_event_idle,
		tdb_event_create,
		tdb_event_death,
		tdb_event_preempt,
		tdb_event_pri_inherit,
		tdb_event_reap,
		tdb_event_concurrency,
		tdb_event_timeout
	},
};


void
tdb_event_ready() {}

void
tdb_event_sleep() {}

void
tdb_event_switchto() {}

void
tdb_event_switchfrom() {}

void
tdb_event_lock_try() {}

void
tdb_event_catchsig() {}

void
tdb_event_idle() {}

void
tdb_event_create() {}

void
tdb_event_death() {}

void
tdb_event_preempt() {}

void
tdb_event_pri_inherit() {}

void
tdb_event_reap() {}

void
tdb_event_concurrency() {}

void
tdb_event_timeout() {}


/*
 * See if the libthread_db agent thread needs to be started.
 * If so, fire it up.  This gets called by the ASLWP when it
 * is sent a SIGLWP signal.
 */
void
_tdb_agent_check()

{
	if (__tdb_attach_stat == TDB_START_AGENT) {
		_sys_thread_create(_tdb_agent, 0);
		__tdb_attach_stat = TDB_ATTACHED;
	}
	if (__tdb_nlwps_req > _nlwps) {
		_thr_setconcurrency(__tdb_nlwps_req);
		__tdb_nlwps_req = 0;
	}
}

/*
 * The libthread_db agent thread.  When not running, it is sleeping
 * on "blocksem".  To execute a request, libthread_db resumes this
 * LWP at "agent_go_addr".  The agent thread inspects its request
 * block for an opcode and argument, executes the request, and puts
 * the return status back into the request block.
 */
void
_tdb_agent()

{
	/*
	 * Force one run through the loop to initialize go_addr
	 * and stop_addr.
	 */
	_lwp_sema_init(&blocksem, 1);
	__tdb_agent_ctl.opcode = NONE_PENDING;
	__tdb_agent_data.agent_lwpid = _lwp_self();
	while (1) {
		_whereami(&__tdb_agent_data.agent_stop_addr);
		_lwp_sema_wait(&blocksem);
		_whereami(&__tdb_agent_data.agent_go_addr);
		__tdb_agent_data.agent_ready = 1;
		switch (__tdb_agent_ctl.opcode) {
		case NONE_PENDING:
			break;
		case THREAD_SUSPEND:
			__tdb_agent_ctl.result =
			    _thr_dbsuspend(__tdb_agent_ctl.u.thr_p->t_tid);
			break;
		case THREAD_RESUME:
			__tdb_agent_ctl.result =
			    _thr_dbcontinue(__tdb_agent_ctl.u.thr_p->t_tid);
			break;
		case THREAD_ALLOC_EVENT_STRUCT:
			__tdb_agent_ctl.result =
			    _thr_alloc_ev_struct(__tdb_agent_ctl.u.thr_p);
			break;
		default:
			__tdb_agent_ctl.result = TD_ERR;
			break;
		}
	}
}


/*
 * The lowest-level memory allocator.  Call _alloc_chunk to mmap
 * chunks of zero-ed memory.  The initial chunk is of size TDB_ALLOC_MINCHUNK.
 * Additional chunks are doubled in size.  Most programs will never
 * allocate anything here, as a modest pool of data structures is
 * statically allocated; this routine is only called if the static
 * allocation is not enough.
 */
#define	TDB_ALLOC_MINCHUNK	(8 * 1024)
/*
 * Maximum single allocation.
 */
#define	TDB_ALLOC_MAXSIZE	(8 * 1024)

void *
tdb_alloc(int size)

{
	static int kerchunksize = TDB_ALLOC_MINCHUNK;
	static caddr_t kerchunk;	/* The chunk we're currently using */
	static int kerchunk_room_left;	/* How much room left in this chunk? */
	static int alloc_failed;	/* If alloc fails, don't try any more */
	void *ret_val;

	ASSERT(size <= TDB_ALLOC_MAXSIZE);
	if (!alloc_failed && (kerchunk == NULL || kerchunk_room_left < size)) {
		if (_alloc_chunk(NULL, kerchunksize, &kerchunk)) {
			kerchunk_room_left = kerchunksize;
			kerchunksize *= 2;
		} else {
			alloc_failed = 1;
			return (NULL);
		}
	}
	ret_val = kerchunk;
	kerchunk += size;
	kerchunk_room_left -= size;
	return (ret_val);
}

/*
 * Update the tdb_stats structure.  Called at thread switch time if
 * statistics gathering was enabled.
 *
 * To determine how long the current thread was running on this LWP,
 * we need to do keep track of gethrvtime() results for each LWP.
 * This is a pain.
 */
void
_tdb_update_stats()

{
	lwpid_t mylid = _lwp_self();
	hrtime_t now = gethrvtime();
	hrtime_t last;
	long delta_ms;
	lwpvtimes_t *lv;
	static char buf[64];

	/*
	 * Get the last hrvtime recorded for this LWP.
	 */

	lv = &lwpvtimes;
	while (mylid >= LWPTIMESPERBLOCK && lv != NULL) {
		if (lv->next == NULL) {
			lv->next = (lwpvtimes_t *)
			    tdb_alloc(sizeof (lwpvtimes_t));
		}
		lv = lv->next;
		mylid -= LWPTIMESPERBLOCK;
	}
	if (lv == NULL) {
		/* Punt, as memory was not available */
		return;
	}
	last = lv->lasttime[mylid];

	delta_ms = (now - last) >> 20;
	lv->lasttime[mylid] = now;
	__tdb_stats.nrunnable_num += (_nrunnable + _onprocq_size) * delta_ms;
	__tdb_stats.nrunnable_den += delta_ms;
	__tdb_stats.a_concurrency_num += _onprocq_size * delta_ms;
	__tdb_stats.a_concurrency_den += delta_ms;
	__tdb_stats.nlwps_num += _nlwps * delta_ms;
	__tdb_stats.nlwps_den += delta_ms;
	__tdb_stats.nidle_num += (_nlwps - _onprocq_size) * delta_ms;
	__tdb_stats.nidle_den += delta_ms;
}




/*
 * Obtain a sync_desc_t descriptor.  Try the free list first, then the
 * current pool.  If there is no current pool, get a new pool from
 * tdb_alloc.
 */
static tdb_sync_desc_t *
tdb_sync_desc_get()

{
	register tdb_sync_desc_t *s;
	if (sync_desc_free != NULL) {
		s = sync_desc_free;
		sync_desc_free = (s == s->next) ? NULL : s->next;
		return (s);
	}
	if (sync_desc_cur_pool == NULL)
		sync_desc_cur_pool =
		    (sync_desc_pool_t *) tdb_alloc(sizeof (sync_desc_pool_t));
	if (sync_desc_cur_pool == NULL)
		return (NULL);

	s = &sync_desc_cur_pool->pool[sync_desc_cur_pool->next_avail++];
	if (sync_desc_cur_pool->next_avail == SYNCDESCPOOLSIZE)
		sync_desc_cur_pool = NULL;
	return (s);
}

/*
 * Register a sync. object in our catalog.  If it's already there,
 * do nothing.  Otherwise, get a new sync. object descriptor, and
 * enter it into the hash table.
 *
 * If we fail to allocate memory, simply do nothing.  This is a
 * "best effort" catalog of synchronization objects.
 */
void
_tdb_sync_obj_register(caddr_t sync_addr, int sync_magic)

{
	register tdb_sync_desc_t *s, *sh, *snew;
	register unsigned addr_as_int = (unsigned) sync_addr;
	int bucketnum;
	int rls_schedlock = 0;

	if (!curthread->t_schedlocked) {
		_sched_lock();
		rls_schedlock = 1;
	}

	bucketnum = (addr_as_int ^ (addr_as_int >> 8) ^
	    (addr_as_int >> 16) ^ (addr_as_int >> 24))
	    % TDB_SYNC_DESC_HASHSIZE;
	sh = sync_desc_hash[bucketnum];

	if (sh != NULL) {
		s = sh;
		while (s->sync_addr != sync_addr && s->next != sh) {
			s = s->next;
		}
		if (s->sync_addr == sync_addr) {
			s->sync_magic = sync_magic;
			if (rls_schedlock)
				_sched_unlock();
			return;
		}
		if ((snew = tdb_sync_desc_get()) == NULL) {
			/* Punt, memory failure */
			if (rls_schedlock)
				_sched_unlock();
			return;
		}
		snew->next = sh;
		snew->prev = sh->prev;
		snew->prev->next = snew;
		sh->prev = snew;
		sync_desc_hash[bucketnum] = snew;
	} else {
		if ((snew = tdb_sync_desc_get()) == NULL) {
			/* Punt, memory failure */
			if (rls_schedlock)
				_sched_unlock();
			return;
		}
		sync_desc_hash[bucketnum] = snew->prev = snew->next = snew;
	}
	snew->sync_magic = sync_magic;
	snew->sync_addr = sync_addr;
	if (rls_schedlock)
		_sched_unlock();
}


/*
 * Remove a sync. object descriptor from the catalog and put it on the
 * free list.
 */
void
_tdb_sync_obj_deregister(caddr_t sync_addr)

{
	register tdb_sync_desc_t *s, *sh;
	register unsigned addr_as_int = (unsigned) sync_addr;
	int bucketnum;
	int rls_schedlock = 0;

	if (!curthread->t_schedlocked) {
		_sched_lock();
		rls_schedlock = 1;
	}

	bucketnum = (addr_as_int ^ (addr_as_int >> 8) ^
	    (addr_as_int >> 16) ^ (addr_as_int >> 24))
	    % TDB_SYNC_DESC_HASHSIZE;
	sh = sync_desc_hash[bucketnum];

	if (sh == NULL) {
		if (rls_schedlock)
			_sched_unlock();
		return;
	}

	for (s = sh; s->sync_addr != sync_addr && s->next != sh; s = s->next)
		;
	if (s->sync_addr != sync_addr) {
		if (rls_schedlock)
			_sched_unlock();
		return;
	}
	if (s->next == s) {
		sync_desc_hash[bucketnum] = NULL;
	} else {
		s->prev->next = s->next;
		s->next->prev = s->prev;
		if (s == sh)
			sync_desc_hash[bucketnum] = s->next;
	}
	s->next = sync_desc_free;
	sync_desc_free = s;
	if (rls_schedlock)
		_sched_unlock();
}



/*
 * Allocate an event buffer for a thread.  This happens when
 * td_thr_event_enable is called on a thread for the first time.
 * As with the other libthread_db allocators, we first try to
 * pick up an event buffer from the current free list, then from
 * the current pool if the free list is empty.  If there is no
 * current pool, we go get one.
 */
int
_thr_alloc_ev_struct(uthread_t *t)

{
	if (t->t_td_evbuf != NULL)
		/*
		 * "Can't happen":  we only call this when there is
		 * no event buffer already.
		 */
		return (TD_ERR);
	if (event_buf_free != NULL) {
		t->t_td_evbuf = event_buf_free;
		event_buf_free = event_buf_free->eventdata;
		return (TD_OK);
	}
	if (event_buf_cur_pool == NULL)
		event_buf_cur_pool = tdb_alloc(sizeof (*event_buf_cur_pool));
	if (event_buf_cur_pool == NULL)
		return (TD_MALLOC);
	t->t_td_evbuf =
	    &event_buf_cur_pool->pool[event_buf_cur_pool->next_avail++];
	if (event_buf_cur_pool->next_avail == EVENTBUFPOOLSIZE)
		event_buf_cur_pool = NULL;
	return (TD_OK);
}
