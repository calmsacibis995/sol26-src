/*
*
*ident	"@(#)td_po.c	1.60	96/08/27 SMI"
*
* Copyright 1993, 1994 by Sun Microsystems, Inc.
*
*/


/*
*
* Description:
*	The functions in this module contain functions that
* interact with the process or processes that execute the program.
*/


#ifdef __STDC__
#pragma weak td_ta_get_ph = __td_ta_get_ph
#pragma weak td_ta_get_nthreads = __td_ta_get_nthreads

#pragma weak td_ta_setconcurrency = __td_ta_setconcurrency
#pragma weak td_ta_enable_stats = __td_ta_enable_stats
#pragma weak td_ta_reset_stats = __td_ta_reset_stats
#pragma weak td_ta_sync_iter = __td_ta_sync_iter
#pragma weak td_ta_get_stats = __td_ta_get_stats

#pragma weak td_ta_tsd_iter = __td_ta_tsd_iter
#pragma weak td_ta_thr_iter = __td_ta_thr_iter
#pragma weak td_ta_map_addr2thr = __td_ta_map_addr2thr
#endif				/* __STDC__ */

#include <thread_db.h>

#include "td.h"
#include "td_po_impl.h"
#include "td_to.h"

#define	TDP_M1 "State mapping error - td_ta_thr_iter"

static int
td_counter(const td_thrhandle_t *th_p, void *data);


/*
*
* Description:
*   Change the target process' desired concurrency level.
* This is similar to thr_setconcurrency but can be used from
* the debugger to change the number of lwp's being requested
* for the target process.
*
* Input:
*   *ta_p - thread agent
*   level - number > 0 that is the level of concurrency
* being requested.
*
* Output:
*   td_ta_setconcurrency() return value.
*
* Side effects:
*   The level of concurrency for the target process is
* changed.
*   Imported functions called: ps_pglobal_lookup,
* ps_pdwrite.
*/
td_err_e
__td_ta_setconcurrency(const td_thragent_t *ta_p, int level)
{
	td_err_e	return_val = TD_OK;
	ps_err_e	ps_ret = PS_OK;
	ps_err_e	(*ps_kill_p)(const struct ps_prochandle *, const int);


#if TEST_PS_CALLS
	if (td_noop) {
		(void) ps_pglobal_lookup(0, 0, 0, 0);
		(void) ps_pdwrite(0, 0, 0, 0);
		return (TD_OK);
	}
#endif
	/*
	 * Older clients may not define ps_kill; they should not be
	 * calling this routine.
	 */
	ps_kill_p = ps_kill;
	if (ps_kill_p == NULL)
		return (TD_NOCAPAB);

	/*
	 * Sanity checks.
	 */
	if (ta_p == NULL) {
		return (TD_BADTA);
	}
	/*
	 * Get a reader lock.
	 */
	if (rw_rdlock((rwlock_t *)&(ta_p->rwlock))) {
		return_val = TD_ERR;
		return (return_val);
	}
	if (ta_p->ph_p == NULL) {
		rw_unlock((rwlock_t *)&(ta_p->rwlock));
		return (TD_BADPH);
	}
	if ((ps_ret = ps_pstop(ta_p->ph_p)) == PS_OK)
		ps_ret = ps_pdwrite(ta_p->ph_p,
		    ta_p->tdb_invar.tdb_nlwps_req_addr,
		    (char *) &level, sizeof (level));
	if (ps_ret != PS_OK)
		return_val = TD_DBERR;
	/*
	 * Goose the ASLWP in the target process to create more
	 * LWP's, if necessary.  It's not an error if the ASLWP doesn't
	 * exist; in that case, we're just starting up, and the ASLWP
	 * will adjust the concurrency level when it starts.
	 */
	if (return_val == TD_OK && ps_kill(ta_p->ph_p, SIGLWP) != PS_OK)
		return_val = TD_DBERR;

	ps_ret = ps_pcontinue(ta_p->ph_p);
	if (ps_ret != PS_OK)
		return_val = TD_DBERR;

	rw_unlock((rwlock_t *) &ta_p->rwlock);
	return (return_val);
}


/*
* Description:
*   Get the process handle out of a thread agent and return it.
*
* Input:
*   *ta_p - thread agent
*
* Output:
*   *ph_pp - pointer to process handle
*   td_ta_get_ph - return value
*
* Side effects:
*   none
*   Imported functions called:
*/
td_err_e
__td_ta_get_ph(const td_thragent_t *ta_p, struct ps_prochandle **ph_pp)
{
	td_err_e	return_val = TD_OK;

#if TEST_PS_CALLS
	if (td_noop) {
		return (TD_OK);
	}
#endif

	/*
	 * If the thread agent pointer is not NULL, extract the process
	 * handle and return it.
	 */
	if (ta_p != NULL) {

		/*
		 * Get a reader lock.
		 */
		if (rw_rdlock((rwlock_t *)&ta_p->rwlock)) {
			return_val = TD_ERR;
		} else {
			/*
			 * Check for NULL prochandle pointer.
			 */
			if (ph_pp != NULL) {
				/* Cast is to throw away const. */
				*ph_pp = (struct ps_prochandle *) ta_p->ph_p;
			} else {
				return_val = TD_ERR;
			}

			rw_unlock((rwlock_t *) &ta_p->rwlock);
		}
	} else {
		return_val = TD_BADTA;
		if (ph_pp != NULL) {
			/*
			 * If the ph_pp is not NULL, set it
			 * to NULL.  This is a friendly
			 * value to return.
			 */
			*ph_pp = NULL;
		}
	}

	return (return_val);
}

/*
* Description:
*   Count the number of times called by incrementing *data.
*
* Input:
*   *th_p - thread handle
*   *data - count of number of times called.
*
* Output:
*   td_counter - returns 0
*   *data - number of times called.
*
* Side effects:
*/
static int
td_counter(const td_thrhandle_t *th_p, void *data)
{
	(*(int *) data)++;

	return (0);
}


/*
* Description:
*   Get the total number of threads in a process. This
* number includes both user and system threads.
*
* Input:
*   *ta_p - thread agent
*
* Output:
*   td_ta_get_nthreads - return value
*   *nthread_p - number of threads
*
* Side effects:
*   none
*   Imported functions called: ps_pglobal_lookup,
* ps_pdread, ps_pstop, ps_pcontinue.
*/
td_err_e
__td_ta_get_nthreads(const td_thragent_t *ta_p, int *nthread_p)
{
	int		thr_count = 0;
	td_err_e	return_val;

#if TEST_PS_CALLS
	if (td_noop) {
		(void) ps_pstop(0);
		(void) ps_pglobal_lookup(0, 0, 0, 0);
		(void) ps_pdread(0, 0, 0, 0);
		(void) ps_pcontinue(0);
		return (TD_OK);
	}
#endif

	/*
	 * Sanity checks.
	 */
	if (ta_p == NULL) {
		return (TD_BADTA);
	}

	/*
	* LOCKING EXCEPTION - Locking is not required
	* here because the locking and checking will be
	* done in __td_ta_thr_iter.  If __td_ta_thr_iter
	* is not used or if some use of the thread agent
	* is made other than the sanity checks, ADD
	* locking.
	*/

	if (ta_p->ph_p == NULL) {
		return (TD_BADPH);
	}
	if (nthread_p == NULL) {
		return (TD_ERR);
	}

	return_val = __td_ta_thr_iter(ta_p,
		td_counter, &thr_count,
		TD_THR_ANY_STATE, TD_THR_LOWEST_PRIORITY,
		TD_SIGNO_MASK, TD_THR_ANY_USER_FLAGS);

	if (return_val != TD_OK) {
		*nthread_p = 0;
	} else {
		*nthread_p = thr_count;
	}

	return (return_val);

}


/*
* Description:
*   Reset thread agent statistics to zero.
*
* Input:
*   *ta_p - thread agent
*
* Output:
*   td_ta_reset_stats - return value
*
* Side effects:
*   Process statistics are reset to zero.
* Imported functions called: ps_pglobal_lookup,
* ps_pdread, ps_pstop, ps_pcontinue.
*/
td_err_e
__td_ta_reset_stats(const td_thragent_t *ta_p)
{
	td_err_e	return_val = TD_OK;
	td_ta_stats_t	tstats;

#if TEST_PS_CALLS
	if (td_noop) {
		(void) ps_pstop(0);
		(void) ps_pglobal_lookup(0, 0, 0, 0);
		if (return_val != TD_NOT_DONE) {
			(void) ps_pdwrite(0, 0, 0, 0);
		}
		(void) ps_pcontinue(0);
		return (TD_OK);
	}
#endif

	/*
	 * Sanity checks.
	 */
	if (ta_p == NULL) {
		return (TD_BADTA);
	}
	/*
	 * Get a reader lock.
	 */
	if (rw_rdlock((rwlock_t *) &ta_p->rwlock)) {
		return_val = TD_ERR;
		return (return_val);
	}
	if (ta_p->ph_p == NULL) {
		rw_unlock((rwlock_t *)&(ta_p->rwlock));
		return (TD_BADPH);
	}

	/*
	 * Stop process.
	 */
	if (ps_pstop(ta_p->ph_p) == PS_OK) {
		memset(&tstats, 0, sizeof (tstats));
		if (ps_pdwrite(ta_p->ph_p, ta_p->tdb_invar.tdb_stats_addr,
		    (char *) &tstats, sizeof (tstats)) != PS_OK)
			return_val = TD_DBERR;
		if (ps_pcontinue(ta_p->ph_p) != PS_OK) {
			return_val = TD_DBERR;
		}
	} else {
		return_val = TD_DBERR;
	}

	(void) rw_unlock((rwlock_t *)&(ta_p->rwlock));
	return (return_val);

}

/*
*
* Description:
*   Enable or disable gathering of thread agent
*	statistics.
*
* Input:
*   *ta_p - thread agent
*   onoff - 0 disables statistics gathering, non-zero enables
* statistics gathering
*
* Output:
*   td_ta_enable_stats() return value
*
* Side effect:
*   Statistics gathering for thread agent is turned on
* or off.
*   Imported functions called: ps_pglobal_lookup,
* ps_pdwrite.
*/
td_err_e
__td_ta_enable_stats(const td_thragent_t *ta_p, int onoff)
{
	td_err_e	return_val = TD_OK;

#if TEST_PS_CALLS
	if (td_noop) {
		(void) ps_pglobal_lookup(0, 0, 0, 0);
		if (return_val != TD_NOT_DONE) {
			(void) ps_pdwrite(0, 0, 0, 0);
		}
		return (return_val);
	}
#endif
	/*
	 * Sanity checks.
	 */
	if (ta_p == NULL) {
		return (TD_BADTA);
	}

	/*
	 * Get a reader lock.
	 */
	if (rw_rdlock((rwlock_t *) &ta_p->rwlock)) {
		return (TD_ERR);
	}
	if (ta_p->ph_p == NULL) {
		rw_unlock((rwlock_t *) &ta_p->rwlock);
		return (TD_BADPH);
	}

	/*
	 * Reset stats before turning them on.
	 */
	if (ps_pstop(ta_p->ph_p) == PS_OK) {
		if (onoff)
			return_val = __td_ta_reset_stats(ta_p);
		if (return_val == TD_OK && ps_pdwrite(ta_p->ph_p,
		    ta_p->tdb_invar.tdb_stats_enable_addr,
		    (char *) &onoff, sizeof (onoff)) != PS_OK)
		return_val = TD_DBERR;
		if (ps_pcontinue(ta_p->ph_p) != PS_OK)
			return_val = TD_DBERR;
	} else {
		return_val = TD_DBERR;
	}

	rw_unlock((rwlock_t *)&(ta_p->rwlock));
	return (return_val);
}


/*
* Description:
*   Copy the current set of thread library statistics in to
* *tstats. Statistics are not reset.
*
* Input:
*   *ta_p - thread agent
*
* Output:
*   *tstats - process statistics
*
* Side effects:
*   none
*   Imported functions called: ps_pdread, ps_pstop, ps_pcontinue.
*/
td_err_e
__td_ta_get_stats(const td_thragent_t *ta_p, td_ta_stats_t *tstats)

{
	td_err_e	return_val = TD_OK;

#if TEST_PS_CALLS
	if (td_noop) {
		(void) ps_pstop(0);
		(void) ps_pglobal_lookup(0, 0, 0, 0);
		if (return_val != TD_NOT_DONE) {
			(void) ps_pdread(0, 0, 0, 0);
		}
		(void) ps_pcontinue(0);
		return (TD_OK);
	}
#endif

	if (ta_p == NULL) {
		return (TD_BADTA);
	}

	if (rw_rdlock((rwlock_t *) &ta_p->rwlock)) {
		return (TD_ERR);
	}
	if (ta_p->ph_p == NULL) {
		rw_unlock((rwlock_t *)&(ta_p->rwlock));
		return (TD_BADPH);
	}

	if (ps_pstop(ta_p->ph_p) != PS_OK) {
		rw_unlock((rwlock_t *)&(ta_p->rwlock));
		return (TD_BADPH);
	}

	if (ps_pdread(ta_p->ph_p, ta_p->tdb_invar.tdb_stats_addr,
	    (char *) tstats, sizeof (*tstats)) != PS_OK)
		return_val = TD_DBERR;

	if (return_val == TD_OK && ps_pdread(ta_p->ph_p,
	    ta_p->tdb_invar.nthreads_addr,
	    (char *) &tstats->nthreads, sizeof (tstats->nthreads)) != PS_OK)
		return_val = TD_DBERR;
	if (return_val == TD_OK && ps_pdread(ta_p->ph_p,
	    ta_p->tdb_invar.nlwps_addr,
	    (char *) &tstats->r_concurrency,
	    sizeof (tstats->r_concurrency)) != PS_OK)
		return_val = TD_DBERR;

	if (ps_pcontinue(ta_p->ph_p) != PS_OK)
		return_val = TD_DBERR;

	rw_unlock((rwlock_t *) &ta_p->rwlock);
	return (return_val);
}


/*
* Description:
*   Iterate over the set of global TSD keys. The
* call back function is called with three
* arguments, a key, a pointer to
* a destructor function, and an extra pointer
* which can be NULL depending on the call back.
*
* Input:
*   *ta_p - thread agent
*   cb - call back function to be called once for
* each key. When return value of cb is
* non-zero, terminate iterations.
*   cbdata_p - data pointer to be passed to cb
*
* Output:
*   td_ta_tsd_iter() return value
*
* Side effects:
*   Call back function is called.
*   Imported functions called: ps_pglobal_lookup,
* ps_pdread, ps_pstop, ps_pcontinue.
*/
td_err_e
__td_ta_tsd_iter(const td_thragent_t *ta_p, td_key_iter_f *cb, void *cbdata_p)
{
	td_err_e		return_val = TD_OK;
	struct tsd_common	tsd_common;
	int			key;

#if TEST_PS_CALLS
	if (td_noop) {
		(void) ps_pstop(0);
		(void) ps_pglobal_lookup(0, 0, 0, 0);
		(void) ps_pdread(0, 0, 0, 0);
		(void) ps_pcontinue(0);
		return (TD_OK);
	}
#endif

	/*
	 * Sanity checks.
	 */
	if (ta_p == NULL) {
		return (TD_BADTA);
	}

	/*
	 * Get a reader lock.
	 */
	if (rw_rdlock((rwlock_t *)&(ta_p->rwlock))) {
		return (TD_ERR);
	}
	if (ta_p->ph_p == NULL) {
		rw_unlock((rwlock_t *)&(ta_p->rwlock));
		return (TD_BADPH);
	}

	if (cb == NULL) {
		rw_unlock((rwlock_t *)&(ta_p->rwlock));
		return (TD_ERR);
	}

	/*
	 * Stop process.
	 */
	if (ps_pstop(ta_p->ph_p) != PS_OK)
		return_val = TD_DBERR;

	/* This function only has to pass the key, not the TSD data. */

	if (return_val == TD_OK &&
	    ps_pdread(ta_p->ph_p, ta_p->tdb_invar.tsd_common_addr,
	    (char *) &tsd_common, sizeof (tsd_common)) != PS_OK)
		return_val = TD_DBERR;

	if (return_val == TD_OK) {
		/*
		 * Walk down the array of keys calling the call-back
		 * function. The check agains tsd_thread.count is not
		 * done because there is no current thread to get the
		 * tsd_thread (normally taken off thread struct of
		 * current thread.
		 */
		for (key = 1; key <= tsd_common.nkeys; key++) {
			(*cb)(key, tsd_common.destructors[key],	cbdata_p);
		}
	}

	if (ps_pcontinue(ta_p->ph_p) != PS_OK)
			return_val = TD_DBERR;

	rw_unlock((rwlock_t *) &ta_p->rwlock);
	return (return_val);
}

/*
* Description:
*   Iterate over all threads. For each thread call
* the function pointed to by "cb" with a pointer
* to a thread handle, and a pointer to data which
* can be NULL. Only call td_thr_iter_f() on threads
* which match the properties of state, ti_pri,
* ti_sigmask_p, and ti_user_flags.  If cb returns
* a non-zero value, terminate iterations.
*
* Input:
*   *ta_p - thread agent
*   *cb - call back function defined by user.
* td_thr_iter_f() takes a thread handle and
* cbdata_p as a parameter.
*   cbdata_p - parameter for td_thr_iter_f().
*
*   state - state of threads of interest.  A value of
* TD_THR_ANY_STATE from enum td_thr_state_e
* does not restrict iterations by state.
*   ti_pri - lower bound of priorities of threads of
* interest.  A value of TD_THR_LOWEST_PRIORITY
* defined in thread_db.h does not restrict
* iterations by priority.  A thread with priority
* less than ti_pri will NOT be passed to the callback
* function.
*   ti_sigmask_p - signal mask of threads of interest.
* A value of TD_SIGNO_MASK defined in thread_db.h
* does not restrict iterations by signal mask.
*   ti_user_flags - user flags of threads of interest.  A
* value of TD_THR_ANY_USER_FLAGS defined in thread_db.h
* does not restrict iterations by user flags.
*
* Output:
*   td_ta_thr_iter() return value
*
* Side effects:
*   cb is called for each thread handle which match state,
* ti_pri, ti_sigmask_p, and ti_user_flags.
*   Imported functions called:
* ps_pdread, ps_pstop, ps_pcontinue.
*/
td_err_e
__td_ta_thr_iter(const td_thragent_t *ta_p, td_thr_iter_f * cb,
	void *cbdata_p, td_thr_state_e state, int ti_pri,
	sigset_t * ti_sigmask_p, unsigned ti_user_flags)
{

	thrtab_t	*hash_tab_p;
	int		hash_tab_size;
	paddr_t	curr_thr_addr;
	uthread_t	curr_thr_struct;
	td_thrhandle_t th = {
		0
	};
	td_err_e	return_val;
	td_err_e	to_return;
	int		i;
	int		iter_exit = 0;

#if TEST_PS_CALLS
	if (td_noop) {
		(void) ps_pstop(0);

		(void) ps_pdread(0, 0, 0, 0);
		(void) ps_pcontinue(0);
		return (TD_OK);
	}
#endif

	/*
	 * Sanity checks.
	 */
	if (ta_p == NULL) {
		return (TD_BADTA);
	}
	/*
	 * Get a reader lock.
	 */
	if (rw_rdlock((rwlock_t *)&(ta_p->rwlock))) {
		return_val = TD_ERR;
		return (return_val);
	}
	if (ta_p->ph_p == NULL) {
		rw_unlock((rwlock_t *)&(ta_p->rwlock));
		return (TD_BADPH);
	}
	if (cb == NULL) {
		rw_unlock((rwlock_t *)&(ta_p->rwlock));
		return (TD_ERR);
	}

	/*
	 * If state is not within bound, short circuit.
	 */
	if ((state < TD_THR_ANY_STATE) || (state > TD_THR_STOPPED_ASLEEP)) {
		rw_unlock((rwlock_t *)&(ta_p->rwlock));
		return (TD_OK);
	}

	/*
	 * Stop process.
	 */
	if (ps_pstop(ta_p->ph_p) == PS_OK) {

		/*
		 * (1) Read the hash table from the debug process.  The hash
		 * table is of fixed size.  Then read down the list of of
		 * threads on each bucket.  (2) Filter each thread. (3)
		 * Create the thread_object for each thread that passes. (4)
		 * Call the call back function on each thread.
		 */

		hash_tab_size = TD_HASH_TAB_SIZE * sizeof (thrtab_t);
		hash_tab_p = (thrtab_t *) malloc(hash_tab_size);
		return_val = __td_read_thread_hash_tbl(ta_p, hash_tab_p,
			hash_tab_size);

		if (return_val != TD_OK) {
			__td_report_po_err(return_val,
				"Reading hash table - td_ta_thr_iter");
			/*
			* Use a goto here so that the indenting
			* doesn't get too deep.
			*/
			goto cleanup;
		}

		/*
		 * Look in all the hash buckets.
		 */
		for (i = 0; i < TD_HASH_TAB_SIZE; i++) {

			/*
			 * Run down the list of threads in each
			 * bucket. Buckets are circular lists but
			 * check for NULL anyway.
			 */
			curr_thr_addr = (paddr_t) td_hash_first_(hash_tab_p[i]);
			while (curr_thr_addr) {

				td_thr_state_e ts_state;

				/*
				 * Read the thread struct.
				 */
				to_return = __td_read_thr_struct(ta_p,
					curr_thr_addr, &curr_thr_struct);

				if (to_return != TD_OK) {
					return_val = TD_ERR;
				}

				/*
				 * Map thread struct state to thread
				 * object state.
				 */

				/*
				 * td_thr_state_e
				 */
				if ((__td_thr_map_state(
					curr_thr_struct.t_state,
					&ts_state) != TD_OK)) {
					return_val = TD_ERR;
					__td_report_po_err(return_val, TDP_M1);
					break;
				}

				/*
				 * Filter on state, sigmask,
				 * priority, and user flags.
				 */

				/*
				 * state
				 */

				if ((state != ts_state) &&
					(state != TD_THR_ANY_STATE)) {
					goto next;
				}

				/*
				 * priority
				 */
				if (ti_pri >
					curr_thr_struct.t_pri) {
					goto next;
				}

				/*
				 * Signal mask
				 */
				if ((ti_sigmask_p != TD_SIGNO_MASK) &&
				    !__td_sigmask_are_equal(ti_sigmask_p,
				    &(curr_thr_struct.t_hold))) {
					goto next;
				}

				/*
				 * User flags.
				 */
				if ((ti_user_flags !=
				    curr_thr_struct.t_usropts) &&
				    (ti_user_flags != (unsigned)
				    TD_THR_ANY_USER_FLAGS)) {
					goto next;
				}
				th.th_ta_p = (td_thragent_t *) ta_p;
				th.th_unique = curr_thr_addr;

				/*
				 * Call back - break if the return
				 * from the call back is non-zero.
				 */
				iter_exit = (*cb) (&th, cbdata_p);
				if (iter_exit) {
					break;
				}
		next:
				curr_thr_addr = (paddr_t)
					curr_thr_struct.t_next;
				if (curr_thr_addr == (paddr_t)
					td_hash_first_(hash_tab_p[i])) {
					break;
				}
			}
			if (iter_exit || (return_val != TD_OK)) {
				break;
			}
		}
cleanup:
		free(hash_tab_p);

		/*
		 * Continue process.
		 */
		if (ps_pcontinue(ta_p->ph_p) != PS_OK) {
			return_val = TD_DBERR;
		}
	} else {
		return_val = TD_DBERR;
	}

	rw_unlock((rwlock_t *)&(ta_p->rwlock));
	return (return_val);

}


/*
* Description:
*   Iterate over all known synchronization
* variables. It is very possible that the list
* generated is incomplete, because the iterator can
* only find synchronization variables with waiters.
* The call back function cb is called for each
* synchronization variable with a pointer to the
* synchronization handle, and a pointer to data,
* cbdata_p, which can be NULL. If cb returns a non-zero
* value, iterations are terminated.
*
* Input:
*   *ta_p - thread agent
*   cb - call back function called once for each
* synchronization variables.
*   cbdata_p - data pointer passed to cb
*
* Output:
*   td_ta_sync_iter - return value
*
* Side effects:
*   cb is called once for each synchronization
* variable found.
*   Imported functions called: ps_pglobal_lookup,
* ps_pdread, ps_pstop, ps_pcontinue.
*/
td_err_e
__td_ta_sync_iter(const td_thragent_t *ta_p,
		td_sync_iter_f * cb, void *cbdata_p)
{
	td_err_e	return_val = TD_OK;
	int		i;
	tdb_sync_desc_t *s;
	tdb_sync_desc_t sync_desc;
	cond_t		cv;
	td_synchandle_t	synchandle;
	tdb_sync_desc_t *sync_desc_hash[TDB_SYNC_DESC_HASHSIZE];

#if TEST_PS_CALLS
	if (td_noop) {
		(void) ps_pstop(0);
		(void) ps_pcontinue(0);
		return (return_val);
	}
#endif

	/*
	 * Sanity checks.
	 */
	if (ta_p == NULL) {
		return (TD_BADTA);
	}
	/*
	 * Get a reader lock.
	 */
	if (rw_rdlock((rwlock_t *)&(ta_p->rwlock))) {
		return_val = TD_ERR;
		return (return_val);
	}
	if (ta_p->ph_p == NULL) {
		rw_unlock((rwlock_t *)&(ta_p->rwlock));
		return (TD_BADPH);
	}

	if (ps_pstop(ta_p->ph_p) != PS_OK)
		return_val = TD_DBERR;

	if (return_val == TD_OK && ps_pdread(ta_p->ph_p,
		    ta_p->tdb_invar.sync_desc_hash_addr,
		    (caddr_t) sync_desc_hash,
		    sizeof (sync_desc_hash)) != PS_OK)
		return_val = TD_DBERR;

	if (return_val == TD_OK) {
		for (i = 0; i != TDB_SYNC_DESC_HASHSIZE; ++i) {
			if (sync_desc_hash[i] == NULL)
				continue;
			s = sync_desc_hash[i];
			do {
				if (ps_pdread(ta_p->ph_p,
				    (paddr_t) s,
				    (caddr_t) &sync_desc,
				    sizeof (sync_desc)) != PS_OK) {
					return_val = TD_DBERR;
					goto out;
				}
				/*
				 * Condvar is the smallest of the
				 * sync. objects.
				 */
				if (ps_pdread(ta_p->ph_p,
				    (paddr_t) sync_desc.sync_addr,
				    (caddr_t) &cv,
				    sizeof (cv)) != PS_OK) {
					/*
					 * This may not be an error:
					 * the sync. object may simply
					 * have been deallocated.
					 */
					goto next;
				}
				if (cv.cond_magic != sync_desc.sync_magic ||
				    (cv.cond_magic != MUTEX_MAGIC &&
				    cv.cond_magic != SEMA_MAGIC &&
				    cv.cond_magic != COND_MAGIC &&
				    cv.cond_magic != RWL_MAGIC))
					goto next;
				synchandle.sh_ta_p = (td_thragent_t *) ta_p;
				synchandle.sh_unique =
				    (paddr_t) sync_desc.sync_addr;
				if ((*cb)(&synchandle, cbdata_p) != 0)
					goto out;
			next:
				s = sync_desc.next;
			} while (s != sync_desc_hash[i]);
		}
	}
out:
	if (ps_pcontinue(ta_p->ph_p) != PS_OK)
		return_val = TD_DBERR;

	rw_unlock((rwlock_t *)&(ta_p->rwlock));
	return (return_val);
}


/*
* Description:
*   Given an address of a thread
* structure and a thread agent, load the thread
* handle.
*
* Input:
*   *ta_p - thread agent
*   thread - address of thread data structure
*
* Output:
*   *th_p - thread handle
*
* Side effects:
*   none
*/
td_err_e
__td_ta_map_addr2thr(const td_thragent_t *ta_p, paddr_t thread,
	td_thrhandle_t *th_p)
{
	td_err_e	return_val = TD_ERR;

#if TEST_PS_CALLS
	if (td_noop) {
		return (TD_OK);
	}
#endif

	/*
	 * Sanity checks.
	 */
	if (ta_p == NULL) {
		return (TD_BADTA);
	}
	/*
	 * Get a reader lock.
	 */
	if (rw_rdlock((rwlock_t *)&(ta_p->rwlock))) {
		return_val = TD_ERR;
		return (return_val);
	}
	if (ta_p->ph_p == NULL) {
		rw_unlock((rwlock_t *)&(ta_p->rwlock));
		return (TD_BADPH);
	}

	th_p->th_ta_p = (td_thragent_t *) ta_p;
	th_p->th_unique = thread;
	return_val = TD_OK;

	rw_unlock((rwlock_t *)&(ta_p->rwlock));
	return (return_val);
}
