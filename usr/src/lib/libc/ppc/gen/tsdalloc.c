/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */
#pragma ident "@(#)tsdalloc.c 1.3	94/09/09 SMI"
#include <thread.h>
#include <synch.h>
#include <mtlib.h>
#include <errno.h>

#ifdef OUT
static mutex_t tsd_lock = DEFAULTMUTEX;

int *
_tsdalloc(thread_key_t *key, int size)
{
	int *val = 0;
	extern void *free();
	extern int *malloc();

	mutex_lock(&tsd_lock);
	if (*key == 0)
		_thr_keycreate(key, free);
	_thr_getspecific(*key, &val);
	if (val == 0)
		_thr_setspecific(*key, (val = malloc(size)));
	mutex_unlock(&tsd_lock);
	return (val);
}
#endif

int *
_tsdalloc(thread_key_t *key, int size)
{
	extern void *free();
	extern int *malloc();
	int *loc = 0;

	if (_thr_getspecific(*key, &loc) != 0) {
		if (_thr_keycreate(key, free) != 0) {
			return (0);
		}
	}
	if (!loc) {
		if (_thr_setspecific(*key, (void **)(loc = malloc(size)))
				!= 0) {
			if (loc)
				(void) free(loc);
			return (0);
		}
	}
	return (loc);
}
