/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)error.c	1.7	96/03/11 SMI" 	/* SVr4.0 1.16	*/


/*LINTLIBRARY*/
#pragma weak	elf_errmsg = _elf_errmsg
#pragma weak	elf_errno = _elf_errno

#include	<thread.h>
#include	<stdlib.h>
#include	<string.h>
#include	<stdio.h>
#include	<locale.h>
#include	<nl_types.h>
#include	"syn.h"
#include	"msg.h"
#include	"libelf.h"
#include	"decl.h"

#define	ELFERRSHIFT	16
#define	SYSERRMASK	0xffff


/*
 * _elf_err has two values encoded in it, both the _elf_err # and
 * the system errno value (if relevant).  These values are encoded
 * in the upper & lower 16 bits of the 4 byte integer.
 */
static int		_elf_err = 0;

static mutex_t		keylock;
static thread_key_t	errkey;
static int		keyonce = 0;
NOTE(DATA_READABLE_WITHOUT_LOCK(keyonce))

#ifdef	__lock_lint
#define	MSGLOCK(x)	mutex_lock(&x);
#define	MSGUNLOCK(x)	mutex_unlock(&x);
#else
#define	MSGLOCK(x) \
	if ((_elf_libc_threaded == 0) || *_elf_libc_threaded) \
		mutex_lock(&(x));
#define	MSGUNLOCK(x) \
	if ((_elf_libc_threaded == 0) || *_elf_libc_threaded) \
		mutex_unlock(&(x));
#endif

extern char *	_dgettext(const char *, const char *);

const char *
_libelf_msg(int mid)
{
	return (_dgettext(MSG_ORIG(MSG_SUNW_OST_SGS), MSG_ORIG(mid)));
}


void
_elf_seterr(int lib_err, int sys_err)
{
	int encerr = (lib_err << ELFERRSHIFT) | (sys_err & SYSERRMASK);
#ifndef	__lock_lint
	if (thr_main() == -1) {
		_elf_err = encerr;
		return;
	}
#endif
	if (keyonce == 0) {
		(void) mutex_lock(&keylock);
		if (keyonce == 0) {
			keyonce++;
			(void) thr_keycreate(&errkey, 0);
		}
		(void) mutex_unlock(&keylock);
	}
	(void) thr_setspecific(errkey, (void *)encerr);
}

int
_elf_geterr() {
	int	rc;

#ifndef	__lock_lint
	if (thr_main() == -1)
		return (_elf_err);
#endif
	if (keyonce == 0)
		return (0);
	(void) thr_getspecific(errkey, (void **)(&rc));
	return (rc);
}

const char *
elf_errmsg(int err)
{
	char *			errno_str;
	char *			elferr_str;
	char *			buffer = 0;
	int			syserr;
	int			elferr;
	static char		intbuf[MAXELFERR];
	static thread_key_t	key = 0;

	if (err == 0) {
		if ((err = _elf_geterr()) == 0)
			return (0);
	} else if (err == -1) {
		if ((err = _elf_geterr()) == 0)
			err = EINF_NULLERROR << ELFERRSHIFT;
	}

	if ((_elf_libc_threaded == 0) || *_elf_libc_threaded == 0)
		buffer = intbuf;
	else {
		/*
		 * If this is a threaded APP then we store the
		 * errmsg buffer in Thread Specific Storage.
		 *
		 * Each thread has its own private buffer.
		 */
		if (thr_getspecific(key, (void **)&buffer) != 0) {
			if (thr_keycreate(&key, free) != 0)
				return (MSG_INTL(EBUG_THRDKEY));
		}
		if (!buffer) {
			if ((buffer = malloc(MAXELFERR)) == 0)
				return (MSG_INTL(EMEM_ERRMSG));
			if (thr_setspecific(key, buffer) != 0) {
				free(buffer);
				return (MSG_INTL(EBUG_THRDSET));
			}
		}
	}

	elferr = ((unsigned)err >> ELFERRSHIFT);
	syserr = err & SYSERRMASK;
	elferr_str = (char *)MSG_INTL(elferr);
	if (syserr && (errno_str = strerror(syserr)))
		(void) sprintf(buffer, MSG_ORIG(MSG_FMT_ERR), elferr_str,
		    errno_str);
	else
		(void) strcpy(buffer, elferr_str);

	return (buffer);
}

int
elf_errno()
{
	int	rc = _elf_geterr();

	_elf_seterr(0, 0);
	return (rc);
}
