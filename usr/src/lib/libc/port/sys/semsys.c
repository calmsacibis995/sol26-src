/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)semsys.c	1.14	96/04/02 SMI"	/* SVr4.0 1.6.1.6	*/

#ifdef __STDC__

#pragma weak semctl = _semctl
#pragma weak semget = _semget
#pragma weak semop = _semop

#endif
#include	"synonyms.h"
#include	<sys/types.h>
#include	<sys/ipc.h>
#include	<sys/sem.h>

#define	SEMCTL	0
#define	SEMGET	1
#define	SEMOP	2

union semun {
	int val;
	struct semid_ds *buf;
	ushort *array;
};

extern int	_semsys();

#ifdef __STDC__

#include <stdarg.h>

/*
 * XXX The kernel implementation of semsys expects a struct containing
 * XXX the "value" of the semun argument, but the compiler passes a
 * XXX pointer to it, since it is a union.  So, we convert here and pass
 * XXX the value, but to keep the naive user from being penalized
 * XXX for the counterintuitive behaviour of the compiler, we ignore
 * XXX the union if it will not be used by the system call (to
 * XXX protect the caller from SIGSEGVs.
 * XXX  e.g. semctl(semid, semnum, cmd, NULL);  which
 * XXX would otherwise always result in a segmentation violation)
 * XXX We do this partly for consistency, since the ICL port did it
 */

int
semctl(int semid, int semnum, int cmd, ...)
{
	int arg;
	va_list ap;

	switch (cmd) {
	case GETVAL:
	case GETPID:
	case GETZCNT:
	case GETNCNT:
	case IPC_RMID:
		arg = 0;
		break;
	default:
		va_start(ap, cmd);
		arg = va_arg(ap, union semun).val;
		va_end(ap);
		break;
	}

	return (_semsys(SEMCTL, semid, semnum, cmd, arg));
}

#else	/* pre-ANSI version */

int
semctl(semid, semnum, cmd, arg)
	int semid, semnum, cmd;
	union semun arg;
{
	int argval;

	switch (cmd) {
	case GETVAL:
	case GETPID:
	case GETZCNT:
	case IPC_RMID:
		argval = 0;
		break;
	default:
		argval = arg.val;
		break;
	}
	return (_semsys(SEMCTL, semid, semnum, cmd, argval));
}

#endif	/* __STDC__ */

int
semget(key, nsems, semflg)
key_t key;
int nsems, semflg;
{
	return (_semsys(SEMGET, key, nsems, semflg));
}

int
semop(semid, sops, nsops)
int semid;
struct sembuf *sops;
unsigned int nsops;
{
	return (_semsys(SEMOP, semid, sops, nsops));
}
