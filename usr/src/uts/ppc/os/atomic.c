/*
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 */

#pragma ident   "@(#)atomic.c 1.1     96/03/04 SMI"

#include <sys/mutex.h>

void
atomic_add_word(u_int *word, int value, struct mutex *lock)
{
	mutex_enter(lock);
	*word += value;
	mutex_exit(lock);
}

void
atomic_add_hword(u_short *hword, int value, struct mutex *lock)
{
	mutex_enter(lock);
	*hword += value;
	mutex_exit(lock);
}
