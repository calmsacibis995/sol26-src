	.ident "@(#)alarm.s 1.3	95/04/07 SMI"
#include <sys/asm_linkage.h>
#include "SYS.h"

	.file "alarm.s"
/*
 * unsigned
 * _alarm(unsigned t)
 */

	ENTRY(__alarm);
	SYSTRAP(alarm)
	RET
	SET_SIZE(__alarm)

/*
 * unsigned
 * _lwp_alarm(unsigned t)
 */

	ENTRY(__lwp_alarm);
	SYSTRAP(lwp_alarm)
	RET
	SET_SIZE(__lwp_alarm)

/*
 * setitimer(which, v, ov)
 *	int which;
 *	struct itimerval *v, *ov;
 */
	ENTRY(__setitimer)
	SYSTRAP(setitimer)
	RET
	SET_SIZE(__setitimer)
