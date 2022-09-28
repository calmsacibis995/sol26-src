/*
*
*ident  "@(#)xtd_po.c 1.15     96/06/11 SMI"
*
* Copyright(c) 1993 by Sun Microsystems, Inc.
*
*/


/*
*  Description:
* The functions in this module contain functions that
* interact in a sparc specific way with the process or processes
* that execute the program.
*/

#ifdef __STDC__
#pragma weak td_ta_map_lwp2thr = __td_ta_map_lwp2thr /* i386 work around */
#endif				/* __STDC__ */

#include <sys/sysi86.h>
#include <thread_db.h>

#include "td.h"
#include "xtd_po.h"
#include "xtd_to.h"


/*
* Description:
*   Convert an LWP to a thread object.
*
* Input:
*   *ta_p - process object defined by debugger
*   lwpid - lwpid for LWP on which thread
* is being requested.
*
* Output:
*   *th_p - thread handle for thread executing
* on LWP with lwpid.
*
* Side effects:
*   Register set is read from lwp.
*/

td_err_e
__td_ta_map_lwp2thr(const td_thragent_t *ta_p, lwpid_t lwpid,
	td_thrhandle_t *th_p)
{
	td_err_e	return_val = TD_ERR;
	struct ssd	lwp_ldt;
	paddr_t		curthread_addr;
	ps_err_e	db_return;
	uthread_t	ts;

#ifdef TEST_PS_CALLS
	if (td_noop) {
		(void) ps_pdread(0, 0, 0, 0);
		(void) ps_lgetregs(0, 0, 0);
		(void) ps_lgetLDT(0, 0, 0);
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

	/*
	 * Read the local descriptor table (ldt) from the lwp.  The address
	 * for the thread running on the lwp is in the bo field of the ldt.
	 * This is done so that early use of interface can be made before all
	 * the thread initialization has been done.
	 */

	db_return = ps_lgetLDT(ta_p->ph_p, lwpid, &lwp_ldt);

	if (db_return != PS_OK) {
		return_val = TD_DBERR;
	} else {

		/*
		 * Read the address contained in the "bo" field.
		 */
		db_return = ps_pdread(ta_p->ph_p,
			td_ssd_curthread_(lwp_ldt),
			(char *)&curthread_addr, sizeof (curthread_addr));

		if (db_return != PS_OK) {
			return_val = TD_DBERR;
		} else {

			/*
			 * Fill in the unique identifier part of thread
			 * handle being returned.
			 */
			th_p->th_unique = curthread_addr;

			/*
			 * Check that the thread struct pointer is not NIL.
			 */
			if (th_p->th_unique == NULL) {
				return_val = TD_NOTHR;
			} else {

				/*
				* Check that lwpid matches.
				*/
				if (__td_read_thr_struct(ta_p,
					th_p->th_unique, &ts) == TD_OK) {
					if (ts.t_lwpid == lwpid) {
						th_p->th_ta_p =
							(td_thragent_t *) ta_p;
						return_val = TD_OK;
					} else {
						return_val = TD_NOTHR;
					}
				} else {
					return_val = TD_NOTHR;
				}
			}
		}
	}

	rw_unlock((rwlock_t *)&(ta_p->rwlock));
	return (return_val);

}
