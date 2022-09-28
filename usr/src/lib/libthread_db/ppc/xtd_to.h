/*
 * Copyright (c) 1994, by Sun Microsytems, Inc.
 * All rights reserved.
 */

#ifndef _XTD_TO_H
#define	_XTD_TO_H

#pragma ident	"@(#)xtd_to.h	1.10	96/06/07 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
* MODULE_XTD_TO_H___________________________________________________
*  Description:
*	Header files for PowerPC specific thread operations.
____________________________________________________________________ */

#define	td_ts_pc_(uthread_t) ((uthread_t).t_pc)
#define	td_ts_sp_(uthread_t) ((uthread_t).t_sp)
#define	td_ts_r13_(uthread_t) ((uthread_t).t_r13)
#define	td_ts_r14_(uthread_t) ((uthread_t).t_r14)
#define	td_ts_r15_(uthread_t) ((uthread_t).t_r15)
#define	td_ts_r16_(uthread_t) ((uthread_t).t_r16)
#define	td_ts_r17_(uthread_t) ((uthread_t).t_r17)
#define	td_ts_r18_(uthread_t) ((uthread_t).t_r18)
#define	td_ts_r19_(uthread_t) ((uthread_t).t_r19)
#define	td_ts_r20_(uthread_t) ((uthread_t).t_r20)
#define	td_ts_r21_(uthread_t) ((uthread_t).t_r21)
#define	td_ts_r22_(uthread_t) ((uthread_t).t_r22)
#define	td_ts_r23_(uthread_t) ((uthread_t).t_r23)
#define	td_ts_r24_(uthread_t) ((uthread_t).t_r24)
#define	td_ts_r25_(uthread_t) ((uthread_t).t_r25)
#define	td_ts_r26_(uthread_t) ((uthread_t).t_r26)
#define	td_ts_r27_(uthread_t) ((uthread_t).t_r27)
#define	td_ts_r28_(uthread_t) ((uthread_t).t_r28)
#define	td_ts_r29_(uthread_t) ((uthread_t).t_r29)
#define	td_ts_r30_(uthread_t) ((uthread_t).t_r30)
#define	td_ts_r31_(uthread_t) ((uthread_t).t_r31)
#define	td_ts_cr_(uthread_t) ((uthread_t).t_cr)

#include "td_to.h"

#ifdef	__cplusplus
}
#endif

#endif /* _XTD_TO_H */
