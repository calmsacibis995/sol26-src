/*
 * Copyright(c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)xtd_to.c	1.31	96/06/05 SMI"

/*
* MODULE_xtd_to.c___________________________________________________
*
*  Description:
*	This module contains functions for interacting with
* the threads within the program.
____________________________________________________________________ */

#ifdef __STDC__
#pragma weak td_thr_getgregs = __td_thr_getgregs
#pragma weak td_thr_setgregs = __td_thr_setgregs

#pragma weak td_thr_getxregsize = __td_thr_getxregsize
#pragma weak td_thr_setxregs = __td_thr_setxregs
#pragma weak td_thr_getxregs = __td_thr_getxregs
#endif				/* __STDC__ */

#include <thread_db.h>

#include <signal.h>
#include "td.h"
#include "xtd_to.h"
#include "td.extdcl.h"
#include "xtd.extdcl.h"

#define	XTDT_M1 "Writing rwin to stack: td_thr_setregs"
#define	XTDT_M2 "Writing process: __td_write_thr_struct"
#define	XTDT_M3 "Reading rwin from stack: td_thr_getregs"


/*
*	Description:
*	   Get the general registers for a given thread.  For a
*	thread that is currently executing on an LWP, (td_thr_state_e)
*	TD_THR_ACTIVE, all registers in regset will be read for the
*	thread.  For a thread not executing on an LWP, only the
*	following registers will be read.
*
*	   %r13 - %r31
*	   %cr, %lr, %pc, %sp(%r1).
*
*	%pc and %sp will be the program counter and stack pointer
*	at the point where the thread will resume execution
*	when it becomes active, (td_thr_state_e) TD_THR_ACTIVE.
*
*	Input:
*	   *th_p - thread handle
*
*	Output:
*	   *regset - Values of general purpose registers(see
*			sys/procfs.h)
*	   td_thr_getgregs - return value
*
*	Side effect:
*	   none
*	   Imported functions called: ps_pstop, ps_pcontinue, ps_pdread,
*					ps_lgetregs.
*
*
*/

td_err_e
__td_thr_getgregs(const td_thrhandle_t *th_p, prgregset_t regset)
{
	td_err_e	return_val = TD_OK;
	td_err_e	td_return;
	uthread_t	thr_struct;
	paddr_t		thr_sp;

#ifdef TEST_PS_CALLS
	if (td_noop) {
		(void) ps_pstop(0);
		(void) ps_pcontinue(0);
		(void) ps_pdread(0, 0, 0, 0);
		(void) ps_lgetregs(0, 0, 0);
		return (TD_OK);
	}
#endif

	/*
	 * Sanity checks.
	 */
	if (th_p == NULL) {
		return (TD_BADTA);
	}
	if ((th_p->th_ta_p == NULL) || (th_p->th_unique == NULL)) {
		return (TD_BADTA);
	}
	if (regset == NULL) {
		return (TD_ERR);
	}

	/*
	 * Get a reader lock.
	 */
	if (rw_rdlock(&(th_p->th_ta_p->rwlock))) {
		return_val = TD_ERR;
		return (return_val);
	}
	if (th_p->th_ta_p->ph_p == NULL) {
		rw_unlock(&(th_p->th_ta_p->rwlock));
		return (TD_BADTA);
	}

	/*
	 * More than 1 byte is being read.  Stop the process.
	 */

	if (ps_pstop(th_p->th_ta_p->ph_p) == PS_OK) {

		/*
		 * Extract the thread struct address
		 * from the thread handle and read
		 * the thread struct.
		 */

		td_return = __td_read_thr_struct(th_p->th_ta_p, th_p->th_unique,
			&thr_struct);


		if (td_return == TD_OK) {
			if (ISVALIDLWP(&thr_struct)) {
				if (ps_lgetregs(th_p->th_ta_p->ph_p,
					thr_struct.t_lwpid, regset) == PS_ERR) {
					return_val = TD_DBERR;
				}
			} else {
				/*
				 * PowerPC: only partial set of registers
				 * are saved.
				 * (see libthread/ppc/ml/machlibthread.h)
				 */
				/*
				 * Set all regset to zero so that values not
				 * set below are zero.  This is a friendly
				 * value.
				 */

				memset(regset, 0, sizeof (prgregset_t));
				return_val = TD_PARTIALREG;

				regset[R_PC] = td_ts_pc_(thr_struct);
				regset[R_R1] = td_ts_sp_(thr_struct);
				regset[R_R13] = td_ts_r13_(thr_struct);
				regset[R_R14] = td_ts_r14_(thr_struct);
				regset[R_R15] = td_ts_r15_(thr_struct);
				regset[R_R16] = td_ts_r16_(thr_struct);
				regset[R_R17] = td_ts_r17_(thr_struct);
				regset[R_R18] = td_ts_r18_(thr_struct);
				regset[R_R19] = td_ts_r19_(thr_struct);
				regset[R_R20] = td_ts_r20_(thr_struct);
				regset[R_R21] = td_ts_r21_(thr_struct);
				regset[R_R22] = td_ts_r22_(thr_struct);
				regset[R_R23] = td_ts_r23_(thr_struct);
				regset[R_R24] = td_ts_r24_(thr_struct);
				regset[R_R25] = td_ts_r25_(thr_struct);
				regset[R_R26] = td_ts_r26_(thr_struct);
				regset[R_R27] = td_ts_r27_(thr_struct);
				regset[R_R28] = td_ts_r28_(thr_struct);
				regset[R_R29] = td_ts_r29_(thr_struct);
				regset[R_R30] = td_ts_r30_(thr_struct);
				regset[R_R31] = td_ts_r31_(thr_struct);
				regset[R_CR] = td_ts_cr_(thr_struct);
			}
		} else {
			return_val = TD_ERR;
		}
		/*
		 * Continue process.
		 */
		if (ps_pcontinue(th_p->th_ta_p->ph_p) != PS_OK) {
			return_val = TD_DBERR;
		}
	}
	/* ps_pstop succeeded   */
	else {
		return_val = TD_DBERR;
	}

	rw_unlock(&(th_p->th_ta_p->rwlock));
	return (return_val);
}


/*
*	Description:
*	   Set the general registers for a given
*	thread.  For a thread that is currently executing on
*	an LWP, (td_thr_state_e) TD_THR_ACTIVE, all registers
*	in regset will be written for the thread.  For a thread
*	not executing on an LWP, only the following registers
*	will be written
*
*	   %r13 - %r31
*	   %cr, %lr, %pc, %sp(%r1).
*
*	%pc and %sp will be the program counter and stack pointer
*	at the point where the thread will resume execution
*	when it becomes active, (td_thr_state_e) TD_THR_ACTIVE.
*
*	Input:
*	   *th_p -  thread handle
*	   *regset - Values of general purpose registers(see
*		sys/procfs.h)
*
*	Output:
*	   td_thr_setgregs - return value
*
*	Side effect:
*	   The general purpose registers for the thread are changed.
*	   Imported functions called: ps_pstop, ps_pcontinue, ps_pdread,
*					ps_lsetregs
*
*/
td_err_e
__td_thr_setgregs(const td_thrhandle_t *th_p, const prgregset_t regset)
{
	td_err_e	return_val = TD_OK;
	td_err_e	td_return;
	uthread_t	thr_struct;
	paddr_t		thr_sp;

#ifdef TEST_PS_CALLS
	if (td_noop) {
		(void) ps_pstop(0);
		(void) ps_pcontinue(0);
		(void) ps_pdread(0, 0, 0, 0);
		(void) ps_pdwrite(0, 0, 0, 0);
		(void) ps_lsetregs(0, 0, 0);
		return (TD_OK);
	}
#endif

	/*
	 * Sanity checks.
	 */
	if (th_p == NULL) {
		return_val = TD_BADTH;
		return (return_val);
	}
	if ((th_p->th_ta_p == NULL) || (th_p->th_unique == NULL)) {
		return_val = TD_BADTA;
		return (return_val);
	}
	if (regset == NULL) {
		return (TD_ERR);
	}

	/*
	 * Get a reader lock.
	 */
	if (rw_rdlock(&(th_p->th_ta_p->rwlock))) {
		return_val = TD_ERR;
		return (return_val);
	}

	if (th_p->th_ta_p->ph_p == NULL) {
		rw_unlock(&(th_p->th_ta_p->rwlock));
		return_val = TD_BADTA;
		return (return_val);
	}


	/*
	 * More than 1 byte is being read.  Stop the process.
	 */
	if (ps_pstop(th_p->th_ta_p->ph_p) == PS_OK) {

		/*
		 * Extract the thread struct address
		 * from the thread handle and read
		 * the thread struct.
		 */

		td_return = __td_read_thr_struct(th_p->th_ta_p,
			th_p->th_unique, &thr_struct);


		if (td_return == TD_OK) {
			if (ISONPROC(&thr_struct) || ISBOUND(&thr_struct) ||
					ISPARKED(&thr_struct)) {

				/*
				 * Thread has an associated lwp.
				 * Write regsiters
				 * back to lwp.
				 */
				if (ps_lsetregs(th_p->th_ta_p->ph_p,
					thr_struct.t_lwpid, regset) == PS_ERR) {
					return_val = TD_DBERR;
				}
			} else {
				thr_sp = td_ts_sp_(thr_struct);

				if (thr_sp) {
					/*
					 * Thread does not have associated lwp.
					 * Modify registers held in
					 * thread struct.
					 */

					/*
					 * For PowerPC, only these
					 * registers are saved.
					 */
					td_ts_sp_(thr_struct) = regset[R_R1];
					td_ts_pc_(thr_struct) = regset[R_PC];
					td_ts_r13_(thr_struct) = regset[R_R13];
					td_ts_r14_(thr_struct) = regset[R_R14];
					td_ts_r15_(thr_struct) = regset[R_R15];
					td_ts_r16_(thr_struct) = regset[R_R16];
					td_ts_r17_(thr_struct) = regset[R_R17];
					td_ts_r18_(thr_struct) = regset[R_R18];
					td_ts_r19_(thr_struct) = regset[R_R19];
					td_ts_r20_(thr_struct) = regset[R_R20];
					td_ts_r21_(thr_struct) = regset[R_R21];
					td_ts_r22_(thr_struct) = regset[R_R22];
					td_ts_r23_(thr_struct) = regset[R_R23];
					td_ts_r24_(thr_struct) = regset[R_R24];
					td_ts_r25_(thr_struct) = regset[R_R25];
					td_ts_r26_(thr_struct) = regset[R_R26];
					td_ts_r27_(thr_struct) = regset[R_R27];
					td_ts_r28_(thr_struct) = regset[R_R28];
					td_ts_r29_(thr_struct) = regset[R_R29];
					td_ts_r30_(thr_struct) = regset[R_R30];
					td_ts_r31_(thr_struct) = regset[R_R31];
					td_ts_cr_(thr_struct) = regset[R_CR];

					/*
					 * Write back the thread struct.
					 */
					td_return = __td_write_thr_struct(
						th_p->th_ta_p, th_p->th_unique,
						&thr_struct);
					if (td_return != TD_OK) {
						return_val = TD_DBERR;
						__td_report_po_err(return_val,
							XTDT_M1);
					} else {
						return_val = TD_PARTIALREG;
					}
				}
			}   /*   Thread not on lwp  */
		}   /*   Read thread data ok  */
		else {
			return_val = TD_ERR;
		}

		/*
		 * Continue process.
		 */
		if (ps_pcontinue(th_p->th_ta_p->ph_p) != PS_OK) {
			return_val = TD_DBERR;
		}
	}
	/* ps_pstop succeeded   */
	else {
		return_val = TD_DBERR;
	}

	rw_unlock(&(th_p->th_ta_p->rwlock));
	return (return_val);
}



/*
* Description:
* Get the size of the SPARC-specific extra register set for the given thread.
* On non-SPARC architectures, return an error code.
*/
td_err_e
__td_thr_getxregsize(const td_thrhandle_t *th_p, int *xregsize)
{
	return (TD_NOXREGS);
}

/*
* Description:
* Get the SPARC-specific extra registers for the given thread.  On
* non-SPARC architectures, return an error code.
*/
td_err_e
__td_thr_getxregs(const td_thrhandle_t *th_p, const caddr_t xregset)
{
	return (TD_NOXREGS);
}

/*
* Description:
* Set the SPARC-specific extra registers for the given thread.  On
* non-SPARC architectures, return an error code.
*/

td_err_e
__td_thr_setxregs(const td_thrhandle_t *th_p, const caddr_t xregset)
{
	return (TD_NOXREGS);
}
