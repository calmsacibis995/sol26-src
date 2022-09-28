/* @(#)genassym.c	1.1 93/10/08 SMI */
#include <sys/synch.h>
#include "synch32.h"

/*
 * This file generates two values used by _lwp_mutex_unlock.s:
 *	a) the byte offset (in lwp_mutex_t) of the word containing the lock byte
 *	b) a mask to extract the waiter field from the word containing it
 */

main()
{
	lwp_mutex_t *lm = (lwp_mutex_t *)0;
	printf("_m4_define_(`M_LOCK_WORD', 0x%x)\n",
		(void *)&lm->mutex_lockword);
	printf("_m4_define_(`M_WAITER_MASK', 0x00ff0000)\n");
	return(0);
}
