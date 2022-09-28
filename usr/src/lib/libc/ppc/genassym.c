/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */
#pragma ident "@(#)genassym.c 1.5       96/11/25 SMI"

#include <sys/synch.h>
#include "synch32.h"
#include <ucontext.h>
#include <sys/reg.h>
#include <sys/isa_defs.h>

#define	OFFSET(type, field)	((int)(&((type *)0)->field))


/*
 * This file generates two values used by _lwp_mutex_unlock.s:
 *	a) the byte offset (in lwp_mutex_t) of the word containing the lock byte
 *	b) a mask to extract the waiter field from the word containing it
 */

lwp_mutex_t lm;
#if defined(_BIG_ENDIAN)
#define	default_waiter_mask	0xff000000
#elif defined(_LITTLE_ENDIAN)
#define	default_waiter_mask	0x000000ff
#else
#error Unknown byte ordering!
#endif

main()
{

	printf("#define MUTEX_LOCK_WORD 0x%x\n",
	    (char *)&lm.mutex_lockword - (char *)&lm);
#if defined(_BIG_ENDIAN)
	printf("#define WAITER_MASK 0x%08x\n",
	    (uint_t)default_waiter_mask >>
	    (8*((char *)&lm.mutex_waiters - (char *)&lm.mutex_lockword)));
#elif defined(_LITTLE_ENDIAN)
	printf("#define WAITER_MASK 0x%08x\n",
	    (uint_t)default_waiter_mask <<
	    (8*((char *)&lm.mutex_waiters - (char *)&lm.mutex_lockword)));
#else
#error Unknown byte ordering!
#endif
	printf("#define	UC_SIGMASK	0x%x\n", UC_SIGMASK);
	printf("#define	UC_STACK	0x%x\n", UC_STACK);
	printf("#define	UC_ALL	0x%x\n", UC_ALL);

	return (0);
}
