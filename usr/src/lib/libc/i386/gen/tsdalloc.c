/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)tsdalloc.c	1.1 SMI"

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
	return val;
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
			return 0;
		}
	}
	if (!loc) {
		if (_thr_setspecific(*key, (void **)(loc = malloc(size)))
				!= 0) {
			if (loc)
				(void)free(loc);
			return 0;
		}
	}
	return (loc);
}

