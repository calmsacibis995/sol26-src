/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */

#pragma	ident	"@(#)sbrk.c	1.3	94/04/25 SMI"

/*
 * Implements void *sbrk(int incr) and int brk(void *) over
 * int _brk_u(void *addr)
 */

#include <synch.h>

extern char _end[];

static void *_nd = (void *)_end;
extern mutex_t __sbrk_lock;

extern int _brk_u(void *);

#pragma	weak sbrk = _sbrk

void *
_sbrk(int incr)
{
	void *old_brk;
	int ret;

	if (incr == 0)
		return (_nd);
	_mutex_lock(&__sbrk_lock);
	old_brk = _nd;
	ret = _brk_u((void *) ((char *) _nd + incr));
	if (ret == 0)
		_nd = (void *) ((char *) _nd + incr);
	_mutex_unlock(&__sbrk_lock);
	return (ret ? (void *) -1 : old_brk);
}

#pragma weak brk = _brk

int
_brk(void *addr)
{
	int ret;

	_mutex_lock(&__sbrk_lock);
	ret = _brk_u(addr);
	if (ret == 0)
		_nd = addr;
	_mutex_unlock(&__sbrk_lock);
	return (ret);
}
