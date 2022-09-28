/*
 *      Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#ifndef _TD_PO_H
#define	_TD_PO_H

#pragma ident	"@(#)td_po.h	1.31	96/06/07 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include "tdb_agent.h"

/*
* MODULE_td_po.h____________________________________________________
*
*  Description:
*	Header for libthread_db thread agents.
____________________________________________________________________ */

struct td_thragent {
	const struct ps_prochandle	*ph_p;
	rwlock_t			rwlock;
	tdb_agent_data_t		tdb_agent_data;
	tdb_invar_data_t		tdb_invar;
};

/*
 * This is the name of the structure in libthread containing all
 * the addresses we will need.
 */
#define	TD_LIBTHREAD_NAME	"libthread.so"
#define	TD_INVAR_DATA_NAME	"__tdb_invar_data"

#ifdef	__cplusplus
}
#endif

#endif /* _TD_PO_H */
