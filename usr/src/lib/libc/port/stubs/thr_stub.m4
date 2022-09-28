
/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */


#pragma	ident	"@(#)thr_stub.m4	1.12	96/07/23 SMI"


/*
 * The table contained in this file defines the list of symbols
 * that libthread interposes on for libc.
 *
 * This table must be maintained in this manner in order to support
 * the linking of libthread behind libc.  If/when this happens
 * then the interface table below will be filled in with the
 * libthread routines and they will be called in the place of libc's
 * entry points.
 *
 * When libthread is linked in before libc this table is not
 * modified and we rely on libthread interposing upon libc's symbols
 * as usual.
 */

#include	<stdarg.h>
#include	<unistd.h>
#include	<sys/errno.h>
#include	<setjmp.h>
#include	<thread.h>
#include	<pthread.h>
#include	<time.h>

#include	"thr_int.h"

typedef void (*PFrV)();
extern unsigned	_libc_alarm(unsigned);
extern int	_libc_close(int);
extern int	_libc_creat(const char *, mode_t);
extern int	_libc_fcntl(int, int, ...);
extern int	_libc_fork();
extern int	_libc_fork1();
extern int	_libc_fsync(int);
extern int	_libc_msync(caddr_t, size_t, int);
extern int	_libc_open(int, int, ...);
extern int	_libc_pause(void);
extern int	_libc_read(int, void *, size_t);
extern int	_libc_setitimer(int, const struct itimerval *,
			struct itimerval *);
extern int	_libc_sigaction(int, const struct sigaction *,
			struct sigaction *);
extern int	_libc_siglongjmp(sigjmp_buf, int);
extern int	_libc_sigpending(sigset_t *);
extern int	_libc_sigprocmask(int, const sigset_t *, sigset_t *);
extern int	_libc_sigsetjmp(sigjmp_buf, int);
extern int	_libc_sigtimedwait(const sigset_t *, siginfo_t *,
			const struct timespec *);
extern int	_libc_sigsuspend(const sigset_t *);
extern int	_libc_sigtimedwait(const sigset_t *, siginfo_t *,
			const struct timespec *);
extern int	_libc_sigwait(sigset_t *);
extern int	_libc_sleep(unsigned);
extern int	_libc_thr_keycreate(thread_key_t *, void(*)(void *));
extern int	_libc_thr_setspecific(thread_key_t, void *);
extern int	_libc_thr_getspecific(thread_key_t, void **);
extern int	_libc_tcdrain(int);
extern int	_libc_wait(int *);
extern int	_libc_waitpid(pid_t, int *, int);
extern int	_libc_write(int, const void *, size_t);
extern int	_libc_thr_keycreate(thread_key_t *, void(*)(void *));
extern int	_libc_thr_setspecific(thread_key_t, void *);
extern int	_libc_thr_getspecific(thread_key_t, void **);
extern int	_libc_nanosleep(const struct timespec *rqtp,
			struct timespec *rmtp);
extern int	_libc_open64(int, int, ...);
extern int	_libc_creat64(const char *, mode_t);

/*
 * M4 macros for the declaration of libc/libthread interface routines.
 *
 * DEFSTUB#	are used to declare both underscore and non-underscore symbol
 *		pairs, return type defaults to int.
 *
 * RDEFSTUB#	used to declare both underscore and non-underscore symbol
 *		pairs that have a non-int return type
 *
 * VDEFSTUB#	used to declare both underscore and non-underscore symbol
 *		pairs that have a void return type
 *
 * SDEFSTUB#	used to only create a 'strong' function declaration
 *
 * WDEFSTUB#	used to only create a 'weak' function declaration
 */

define(DEFSTUB0, `
#pragma weak	_$1
#pragma weak	$1 = _$1
int _$1(void) {
	return ((*ti_jmp_table[$2])());
}')dnl

define(RDEFSTUB0, `
#pragma weak	_$1
#pragma weak	$1 = _$1
$3 _$1(void) {
	return ((*ti_jmp_table[$2])());
}')dnl

define(VDEFSTUB0, `
#pragma weak	_$1
#pragma weak	$1 = _$1
void _$1(void) {
	(*ti_jmp_table[$2])();
	return;
}')dnl

define(SDEFSTUB0, `
int $1(void) {
	return ((*ti_jmp_table[$2])());
}')dnl

define(WDEFSTUB0, `
#pragma weak	$1
int $1(void) {
	return ((*ti_jmp_table[$2])());
}')dnl

define(DEFSTUB1, `
#pragma weak	_$1
#pragma weak	$1 = _$1
int _$1($3 x1) {
	return ((*ti_jmp_table[$2])(x1));
}')dnl

define(RDEFSTUB1, `
#pragma weak	_$1
#pragma weak	$1 = _$1
$3 _$1($4 x1) {
	return (($3)(*ti_jmp_table[$2])(x1));
}')dnl

define(VDEFSTUB1, `
#pragma weak	_$1
#pragma weak	$1 = _$1
void _$1($3 x1) {
	(*ti_jmp_table[$2])(x1);
	return;
}')dnl

define(SDEFSTUB1, `
int $1($3 x1) {
	return ((*ti_jmp_table[$2])(x1));
}')dnl

define(WDEFSTUB1, `
#pragma weak $1
int $1($3 x1) {
	return ((*ti_jmp_table[$2])(x1));
}')dnl

define(DEFSTUB2, `
#pragma weak	_$1
#pragma weak	$1 = _$1
int _$1($3 x1, $4 x2) {
	return ((*ti_jmp_table[$2])(x1, x2));
}')dnl

define(VDEFSTUB2, `
#pragma weak	_$1
#pragma weak	$1 = _$1
void _$1($3 x1, $4 x2) {
	(*ti_jmp_table[$2])(x1, x2);
	return;
}')dnl

define(SDEFSTUB2, `
int $1($3 x1, $4 x2) {
	return ((*ti_jmp_table[$2])(x1, x2));
}')dnl

define(WDEFSTUB2, `
#pragma weak	$1
int $1($3 x1, $4 x2) {
	return ((*ti_jmp_table[$2])(x1, x2));
}')dnl
define(DEFSTUB3, `
#pragma weak	_$1
#pragma weak	$1 = _$1
int _$1($3 x1, $4 x2, $5 x3) {
	return ((*ti_jmp_table[$2])(x1, x2, x3));
}')dnl

define(SDEFSTUB3, `
int $1($3 x1, $4 x2, $5 x3) {
	return ((*ti_jmp_table[$2])(x1, x2, x3));
}')dnl

define(WDEFSTUB3, `
#pragma weak	$1
int $1($3 x1, $4 x2, $5 x3) {
	return ((*ti_jmp_table[$2])(x1, x2, x3));
}')dnl


define(DEFSTUB4, `
#pragma weak	_$1
#pragma weak	$1 = _$1
int _$1($3 x1, $4 x2, $5 x3, $6 x4) {
	return ((*ti_jmp_table[$2])(x1, x2, x3, x4));
}')dnl

define(SDEFSTUB4, `
int $1($3 x1, $4 x2, $5 x3, $6 x4) {
	return ((*ti_jmp_table[$2])(x1, x2, x3, x4));
}')dnl

define(DEFSTUB5, `
#pragma weak	_$1
#pragma weak	$1 = _$1
int _$1($3 x1, $4 x2, $5 x3, $6 x4, $7 x5) {
	return ((*ti_jmp_table[$2])(x1, x2, x3, x4, x5));
}')dnl

define(SDEFSTUB5, `
int $1($3 x1, $4 x2, $5 x3, $6 x4, $7 x5) {
	return ((*ti_jmp_table[$2])(x1, x2, x3, x4, x5));
}')dnl

define(DEFSTUB6, `
#pragma weak	_$1
#pragma weak	$1 = _$1
int _$1($3 x1, $4 x2, $5 x3, $6 x4, $7 x5, $8 x6) {
	return ((*ti_jmp_table[$2])(x1, x2, x3, x4, x5, x6));
}')dnl

define(SDEFSTUB6, `
int $1($3 x1, $4 x2, $5 x3, $6 x4, $7 x5, $8 x6) {
	return ((*ti_jmp_table[$2])(x1, x2, x3, x4, x5, x6));
}')dnl

static int
_return_zero()
{
	return (0);
}

static int
_return_one()
{
	return (1);
}

static int
_return_negone()
{
	return (-1);
}

static int
_return_notsup()
{
	return (ENOTSUP);
}


/*
 * These are the 'default' values for libc's table that point
 * directly to libc's routines.  This table is used to re-initialize
 * the interface table if libthread is 'dlclosed'.
 *
 */
static	int (*	ti_def_table[TI_MAX])() = {
/* 0 */		0,			/* TI_NULL */
/* 1 */		_return_zero,		/* TI_MUTEX_LOCK */
/* 2 */		_return_zero,		/* TI_MUTEX_UNLOCK */
/* 3 */		_return_zero,		/* TI_LRW_RDLOCK */
/* 4 */		_return_zero,		/* TI_LRW_WRLOCK */
/* 5 */		_return_zero,		/* TI_LRW_UNLOCK */
/* 6 */		_return_zero,		/* TI_BIND_GUARD */
/* 7 */		_return_zero,		/* TI_BIND_CLEAR */
/* 8 */		_return_zero,		/* TI_LATFORK */
/* 9 */		_return_one,		/* TI_THRSELF */
/* 10 */	0,			/* TI_VERSION */
/* 11 */	_return_zero,		/* TI_COND_BROAD */
/* 12 */	_return_zero,		/* TI_COND_DESTROY */
/* 13 */	_return_zero,		/* TI_COND_INIT */
/* 14 */	_return_zero,		/* TI_COND_SIGNAL */
/* 15 */	_return_zero,		/* TI_COND_TWAIT */
/* 16 */	_return_zero,		/* TI_COND_WAIT */
/* 17 */	_libc_fork,		/* TI_FORK */
/* 18 */	_libc_fork1,		/* TI_FORK1 */
/* 19 */	_return_zero,		/* TI_MUTEX_DEST */
/* 20 */	_return_one,		/* TI_MUTEX_HELD */
/* 21 */	_return_zero,		/* TI_MUTEX_INIT */
/* 22 */	_return_zero,		/* TI_MUTEX_TRYLCK */
/* 23 */	_return_zero,		/* TI_ATFORK */
/* 24 */	_return_one,		/* TI_RW_RDHELD */
/* 25 */	_return_zero,		/* TI_RW_RDLOCK */
/* 26 */	_return_zero,		/* TI_RW_WRLOCK */
/* 27 */	_return_zero,		/* TI_RW_UNLOCK */
/* 28 */	_return_zero,		/* TI_TRYRDLOCK */
/* 29 */	_return_zero,		/* TI_TRYWRLOCK */
/* 30 */	_return_one,		/* TI_RW_WRHELD */
/* 31 */	_return_zero,		/* TI_RW_LOCKINIT */
/* 32 */	_return_one,		/* TI_SEM_HELD */
/* 33 */	_return_zero,		/* TI_SEM_INIT */
/* 34 */	_return_zero,		/* TI_SEM_POST */
/* 35 */	_return_zero,		/* TI_SEM_TRYWAIT */
/* 36 */	_return_zero,		/* TI_SEM_WAIT */
/* 37 */	_libc_sigaction,	/* TI_SIGACTION */
/* 38 */	_libc_sigprocmask,	/* TI_SIGPROCMASK */
/* 39 */	_libc_sigwait,		/* TI_SIGWAIT */
/* 40 */	_libc_sleep,		/* TI_SLEEP */
/* 41 */	_return_zero,		/* TI_THR_CONT */
/* 42 */	_return_negone,		/* TI_THR_CREATE */
/* 43 */	_return_zero,		/* TI_THR_ERRNOP */
/* 44 */	_return_zero,		/* TI_THR_EXIT */
/* 45 */	_return_zero,		/* TI_THR_GETCONC */
/* 46 */	_return_zero,		/* TI_THR_GETPRIO */
/* 47 */	_libc_thr_getspecific,	/* TI_THR_GETSPEC */
/* 48 */	_return_zero,		/* TI_THR_JOIN */
/* 49 */	_libc_thr_keycreate,	/* TI_THR_KEYCREAT */
/* 50 */	_return_zero,		/* TI_THR_KILL */
/* 51 */	_return_negone,		/* TI_THR_MAIN */
/* 52 */	_return_zero,		/* TI_THR_SETCONC */
/* 53 */	_return_zero,		/* TI_THR_SETPRIO */
/* 54 */	_libc_thr_setspecific,	/* TI_THR_SETSPEC */
/* 55 */	_return_zero,		/* TI_THR_SIGSET */
/* 56 */	_return_notsup,		/* TI_THR_STKSEGMENT */
/* 57 */	_return_zero,		/* TI_THR_SUSPEND */
/* 58 */	_return_zero,		/* TI_THR_YIELD */
/* 59 */	_libc_close,		/* TI_CLOSE */
/* 60 */	_libc_creat,		/* TI_CREAT */
/* 61 */	(int (*)())_libc_fcntl,	/* TI_FCNTL */
/* 62 */	_libc_fsync,		/* TI_FSYNC */
/* 63 */	_libc_msync,		/* TI_MSYNC */
/* 64 */	(int (*)())_libc_open,	/* TI_OPEN */
/* 65 */	_libc_pause,		/* TI_PAUSE */
/* 66 */	_libc_read,		/* TI_READ */
/* 67 */	_libc_sigsuspend,	/* TI_SIGSUSPEND */
/* 68 */	_libc_tcdrain,		/* TI_TCDRAIN */
/* 69 */	_libc_wait,		/* TI_WAIT */
/* 70 */	_libc_waitpid,		/* TI_WAITPID */
/* 71 */	_libc_write,			/* TI_WRITE */
/* 72 */	_return_zero,		/* TI_PCOND_BROAD */
/* 73 */	_return_zero,		/* TI_PCOND_DEST */
/* 74 */	_return_zero,		/* TI_PCOND_INIT */
/* 75 */	_return_zero,		/* TI_PCOND_SIGNAL */
/* 76 */	_return_zero,		/* TI_PCOND_TWAIT */
/* 77 */	_return_zero,		/* TI_PCOND_WAIT */
/* 78 */	_return_zero,		/* TI_PCONDA_DEST */
/* 79 */	_return_zero,		/* TI_PCONDA_GETPS */
/* 80 */	_return_zero,		/* TI_PCONDA_INIT */
/* 81 */	_return_zero,		/* TI_PCONDA_SETPS */
/* 82 */	_return_zero,		/* TI_PMUTEX_DESTROY */
/* 83 */	_return_zero,		/* TI_PMUTEX_GPC */
/* 84 */	_return_zero,		/* TI_PMUTEX_INIT */
/* 85 */	_return_zero,		/* TI_PMUTEX_LOCK */
/* 86 */	_return_zero,		/* TI_PMUTEX_SPC */
/* 87 */	_return_zero,		/* TI_PMUTEX_TRYL */
/* 88 */	_return_zero,		/* TI_PMUTEX_UNLCK */
/* 89 */	_return_zero,		/* TI_PMUTEXA_DEST */
/* 90 */	_return_zero,		/* TI_PMUTEXA_GPC */
/* 91 */	_return_zero,		/* TI_PMUTEXA_GP */
/* 92 */	_return_zero,		/* TI_PMUTEXA_GPS */
/* 93 */	_return_zero,		/* TI_PMUTEXA_INIT */
/* 94 */	_return_zero,		/* TI_PMUTEXA_SPC */
/* 95 */	_return_zero,		/* TI_PMUTEXA_SP */
/* 96 */	_return_zero,		/* TI_PMUTEXA_SPS */
/* 97 */	_return_negone,		/* TI_THR_MINSTACK */
/* 98 */	_libc_sigtimedwait,	/* TI_SIGTIMEDWAIT */
/* 99 */	(int (*)())_libc_alarm,	/* TI_ALARM */
/* 100 */	_libc_setitimer,	/* TI_SETITIMER */
/* 101 */	_libc_siglongjmp,	/* TI_SIGLONGJMP */
/* 102 */	0,			/* TI_SIGSETGJMP */
/* 103 */	_libc_sigpending,	/* TI_SIGPENDING */
/* 104 */	_libc_nanosleep,	/* TI__NANOSLEEP */
/* 105 */	(int (*)())_libc_open64,/* TI_OPEN64 */
/* 106 */	_libc_creat64,		/* TI_CREAT64 */
};



/*
 * Libc/Libthread interface table.
 */
static	int (*	ti_jmp_table[TI_MAX])() = {
/* 0 */		0,			/* TI_NULL */
/* 1 */		_return_zero,		/* TI_MUTEX_LOCK */
/* 2 */		_return_zero,		/* TI_MUTEX_UNLOCK */
/* 3 */		_return_zero,		/* TI_LRW_RDLOCK */
/* 4 */		_return_zero,		/* TI_LRW_WRLOCK */
/* 5 */		_return_zero,		/* TI_LRW_UNLOCK */
/* 6 */		_return_zero,		/* TI_BIND_GUARD */
/* 7 */		_return_zero,		/* TI_BIND_CLEAR */
/* 8 */		_return_zero,		/* TI_LATFORK */
/* 9 */		_return_one,		/* TI_THRSELF */
/* 10 */	0,			/* TI_VERSION */
/* 11 */	_return_zero,		/* TI_COND_BROAD */
/* 12 */	_return_zero,		/* TI_COND_DESTROY */
/* 13 */	_return_zero,		/* TI_COND_INIT */
/* 14 */	_return_zero,		/* TI_COND_SIGNAL */
/* 15 */	_return_zero,		/* TI_COND_TWAIT */
/* 16 */	_return_zero,		/* TI_COND_WAIT */
/* 17 */	_libc_fork,		/* TI_FORK */
/* 18 */	_libc_fork1,		/* TI_FORK1 */
/* 19 */	_return_zero,		/* TI_MUTEX_DEST */
/* 20 */	_return_one,		/* TI_MUTEX_HELD */
/* 21 */	_return_zero,		/* TI_MUTEX_INIT */
/* 22 */	_return_zero,		/* TI_MUTEX_TRYLCK */
/* 23 */	_return_zero,		/* TI_ATFORK */
/* 24 */	_return_one,		/* TI_RW_RDHELD */
/* 25 */	_return_zero,		/* TI_RW_RDLOCK */
/* 26 */	_return_zero,		/* TI_RW_WRLOCK */
/* 27 */	_return_zero,		/* TI_RW_UNLOCK */
/* 28 */	_return_zero,		/* TI_TRYRDLOCK */
/* 29 */	_return_zero,		/* TI_TRYWRLOCK */
/* 30 */	_return_one,		/* TI_RW_WRHELD */
/* 31 */	_return_zero,		/* TI_RW_LOCKINIT */
/* 32 */	_return_one,		/* TI_SEM_HELD */
/* 33 */	_return_zero,		/* TI_SEM_INIT */
/* 34 */	_return_zero,		/* TI_SEM_POST */
/* 35 */	_return_zero,		/* TI_SEM_TRYWAIT */
/* 36 */	_return_zero,		/* TI_SEM_WAIT */
/* 37 */	_libc_sigaction,	/* TI_SIGACTION */
/* 38 */	_libc_sigprocmask,	/* TI_SIGPROCMASK */
/* 39 */	_libc_sigwait,		/* TI_SIGWAIT */
/* 40 */	_libc_sleep,		/* TI_SLEEP */
/* 41 */	_return_zero,		/* TI_THR_CONT */
/* 42 */	_return_negone,		/* TI_THR_CREATE */
/* 43 */	_return_zero,		/* TI_THR_ERRNOP */
/* 44 */	_return_zero,		/* TI_THR_EXIT */
/* 45 */	_return_zero,		/* TI_THR_GETCONC */
/* 46 */	_return_zero,		/* TI_THR_GETPRIO */
/* 47 */	_libc_thr_getspecific,	/* TI_THR_GETSPEC */
/* 48 */	_return_zero,		/* TI_THR_JOIN */
/* 49 */	_libc_thr_keycreate,	/* TI_THR_KEYCREAT */
/* 50 */	_return_zero,		/* TI_THR_KILL */
/* 51 */	_return_negone,		/* TI_THR_MAIN */
/* 52 */	_return_zero,		/* TI_THR_SETCONC */
/* 53 */	_return_zero,		/* TI_THR_SETPRIO */
/* 54 */	_libc_thr_setspecific,	/* TI_THR_SETSPEC */
/* 55 */	_return_zero,		/* TI_THR_SIGSET */
/* 56 */	_return_notsup,		/* TI_THR_STKSEGMENT */
/* 57 */	_return_zero,		/* TI_THR_SUSPEND */
/* 58 */	_return_zero,		/* TI_THR_YIELD */
/* 59 */	_libc_close,		/* TI_CLOSE */
/* 60 */	_libc_creat,		/* TI_CREAT */
/* 61 */	(int (*)())_libc_fcntl,	/* TI_FCNTL */
/* 62 */	_libc_fsync,		/* TI_FSYNC */
/* 63 */	_libc_msync,		/* TI_MSYNC */
/* 64 */	(int (*)())_libc_open,	/* TI_OPEN */
/* 65 */	_libc_pause,		/* TI_PAUSE */
/* 66 */	_libc_read,		/* TI_READ */
/* 67 */	_libc_sigsuspend,	/* TI_SIGSUSPEND */
/* 68 */	_libc_tcdrain,		/* TI_TCDRAIN */
/* 69 */	_libc_wait,		/* TI_WAIT */
/* 70 */	_libc_waitpid,		/* TI_WAITPID */
/* 71 */	_libc_write,			/* TI_WRITE */
/* 72 */	_return_zero,		/* TI_PCOND_BROAD */
/* 73 */	_return_zero,		/* TI_PCOND_DEST */
/* 74 */	_return_zero,		/* TI_PCOND_INIT */
/* 75 */	_return_zero,		/* TI_PCOND_SIGNAL */
/* 76 */	_return_zero,		/* TI_PCOND_TWAIT */
/* 77 */	_return_zero,		/* TI_PCOND_WAIT */
/* 78 */	_return_zero,		/* TI_PCONDA_DEST */
/* 79 */	_return_zero,		/* TI_PCONDA_GETPS */
/* 80 */	_return_zero,		/* TI_PCONDA_INIT */
/* 81 */	_return_zero,		/* TI_PCONDA_SETPS */
/* 82 */	_return_zero,		/* TI_PMUTEX_DESTROY */
/* 83 */	_return_zero,		/* TI_PMUTEX_GPC */
/* 84 */	_return_zero,		/* TI_PMUTEX_INIT */
/* 85 */	_return_zero,		/* TI_PMUTEX_LOCK */
/* 86 */	_return_zero,		/* TI_PMUTEX_SPC */
/* 87 */	_return_zero,		/* TI_PMUTEX_TRYL */
/* 88 */	_return_zero,		/* TI_PMUTEX_UNLCK */
/* 89 */	_return_zero,		/* TI_PMUTEXA_DEST */
/* 90 */	_return_zero,		/* TI_PMUTEXA_GPC */
/* 91 */	_return_zero,		/* TI_PMUTEXA_GP */
/* 92 */	_return_zero,		/* TI_PMUTEXA_GPS */
/* 93 */	_return_zero,		/* TI_PMUTEXA_INIT */
/* 94 */	_return_zero,		/* TI_PMUTEXA_SPC */
/* 95 */	_return_zero,		/* TI_PMUTEXA_SP */
/* 96 */	_return_zero,		/* TI_PMUTEXA_SPS */
/* 97 */	_return_negone,		/* TI_THR_MINSTACK */
/* 98 */	_libc_sigtimedwait,	/* TI_SIGTIMEDWAIT */
/* 99 */	(int (*)())_libc_alarm,	/* TI_ALARM */
/* 100 */	_libc_setitimer,	/* TI_SETITIMER */
/* 101 */	_libc_siglongjmp,	/* TI_SIGLONGJMP */
/* 102 */	0,			/* TI_SIGSETGJMP */
/* 103 */	_libc_sigpending,	/* TI_SIGPENDING */
/* 104 */	_libc_nanosleep,	/* TI__NANOSLEEP */
/* 105 */	(int (*)())_libc_open64,/* TI_OPEN64 */
/* 106 */	_libc_creat64,		/* TI_CREAT64 */
};


/*
 * _thr_libthread() is used to identify the link order
 * of libc.so vs. libthread.so.  Their is a copy of each in
 * both libraries.  They return the following:
 *
 *	libc:_thr_libthread(): returns 0
 *	libthread:_thr_libthread(): returns 1
 *
 * A call to this routine can be used to determine whether or
 * not the libc threads interface needs to be initialized or not.
 */
int
_thr_libthread()
{
	return (0);
}

void
_libc_threads_interface(Thr_interface * ti_funcs)
{
	int tag;
	if (ti_funcs) {
		_libc_set_threaded();
		if (_thr_libthread() != 0)
			return;
		for (tag = ti_funcs->ti_tag; tag; tag = (++ti_funcs)->ti_tag) {
			if (tag >= TI_MAX) {
				const char * err_mesg = "libc: warning: "
					"libc/libthread interface mismatch: "
					"unknown tag value ignored\n";
				_write(2, err_mesg, strlen(err_mesg));
			}
			if (ti_funcs->ti_un.ti_func != 0)
				ti_jmp_table[tag] = ti_funcs->ti_un.ti_func;
		}
	} else {
		_libc_unset_threaded();
		if (_thr_libthread() != 0)
			return;
		for (tag = 0; tag < TI_MAX; tag++)
			ti_jmp_table[tag] = ti_def_table[tag];
	}
}


/*
 * m4 Macros do define 'stub' routines for all libthread/libc
 * interface routines.
 */

DEFSTUB1(mutex_lock, TI_MUTEX_LOCK, mutex_t *)
DEFSTUB1(mutex_unlock, TI_MUTEX_UNLOCK, mutex_t *)
DEFSTUB1(rw_rdlock, TI_RW_RDLOCK, rwlock_t *)
DEFSTUB1(rw_wrlock, TI_RW_WRLOCK, rwlock_t *)
DEFSTUB1(rw_unlock, TI_RW_UNLOCK, rwlock_t *)
RDEFSTUB0(thr_self, TI_THRSELF, thread_t)
DEFSTUB1(cond_broadcast, TI_COND_BROAD, cond_t *)
DEFSTUB1(cond_destroy, TI_COND_DESTROY, cond_t *)
DEFSTUB3(cond_init, TI_COND_INIT, cond_t *, int, void *)
DEFSTUB1(cond_signal, TI_COND_SIGNAL, cond_t *)
DEFSTUB3(cond_timedwait, TI_COND_TWAIT, cond_t *, int, timestruc_t *)
DEFSTUB2(cond_wait, TI_COND_WAIT, cond_t *, mutex_t *)
DEFSTUB0(fork, TI_FORK)
DEFSTUB0(fork1, TI_FORK1)
DEFSTUB1(mutex_destroy, TI_MUTEX_DEST, mutex_t *)
DEFSTUB1(mutex_held, TI_MUTEX_HELD, mutex_t *)
DEFSTUB3(mutex_init, TI_MUTEX_INIT, mutex_t *, int, void *)
DEFSTUB1(mutex_trylock, TI_MUTEX_TRYLCK, mutex_t *)
DEFSTUB1(rw_read_held, TI_RW_RDHELD, rwlock_t *)
DEFSTUB1(rw_tryrdlock, TI_TRYRDLOCK, rwlock_t *)
DEFSTUB1(rw_trywrlock, TI_TRYWRLOCK, rwlock_t *)
DEFSTUB1(rw_write_held, TI_RW_WRHELD, rwlock_t *)
DEFSTUB3(rwlock_init, TI_RWLOCKINIT, rwlock_t *, int, void *)
DEFSTUB1(sema_held, TI_SEM_HELD, sema_t *)
DEFSTUB4(sema_init, TI_SEM_INIT, sema_t *, unsigned int, int, void *)
DEFSTUB1(sema_post, TI_SEM_POST, sema_t *)
DEFSTUB1(sema_trywait, TI_SEM_TRYWAIT, sema_t *)
DEFSTUB1(sema_wait, TI_SEM_WAIT, sema_t *)
DEFSTUB3(sigaction, TI_SIGACTION, int, const struct sigaction *,
		struct sigaction *)
DEFSTUB3(sigprocmask, TI_SIGPROCMASK, int, sigset_t *, sigset_t *)
DEFSTUB1(sigwait, TI_SIGWAIT, sigset_t *)
DEFSTUB1(sleep, TI_SLEEP, unsigned int)
DEFSTUB1(thr_continue, TI_THR_CONT, thread_t)
DEFSTUB6(thr_create, TI_THR_CREATE, void *, size_t, void *,
		void *, long, thread_t *)
DEFSTUB0(thr_errnop, TI_THR_ERRNOP)
VDEFSTUB1(thr_exit, TI_THR_EXIT, void *)
DEFSTUB0(thr_getconcurrency, TI_THR_GETCONC)
DEFSTUB2(thr_getprio, TI_THR_GETPRIO, thread_t, int *)
DEFSTUB2(thr_getspecific, TI_THR_GETSPEC, thread_key_t, void **)
DEFSTUB3(thr_join, TI_THR_JOIN, thread_t, thread_t *, void **)
DEFSTUB2(thr_keycreate, TI_THR_KEYCREAT, thread_key_t, PFrV)
DEFSTUB2(thr_kill, TI_THR_KILL, thread_t, int)
DEFSTUB0(thr_main, TI_THR_MAIN)
DEFSTUB1(thr_setconcurrency, TI_THR_SETCONC, int)
DEFSTUB2(thr_setprio, TI_THR_SETPRIO, thread_t, int)
DEFSTUB2(thr_setspecific, TI_THR_SETSPEC, unsigned int, void *)
DEFSTUB3(thr_sigsetmask, TI_THR_SIGSET, int, const sigset_t *, sigset_t *)
DEFSTUB1(thr_stksegment, TI_THR_STKSEG, stack_t)
DEFSTUB1(thr_suspend, TI_THR_SUSPEND, thread_t)
VDEFSTUB0(thr_yield, TI_THR_YIELD)
WDEFSTUB1(close, TI_CLOSE, int)
WDEFSTUB2(creat, TI_CREAT, const char *, mode_t)
WDEFSTUB3(fcntl, TI_FCNTL, void *, void *, void *)
WDEFSTUB1(fsync, TI_FSYNC, int)
WDEFSTUB3(msync, TI_MSYNC, caddr_t, size_t, int)
WDEFSTUB3(open, TI_OPEN, int, int, mode_t)
WDEFSTUB0(pause, TI_PAUSE)
WDEFSTUB3(read, TI_READ, int, void *, size_t)
WDEFSTUB1(sigsuspend, TI_SIGSUSPEND, sigset_t *)
WDEFSTUB1(tcdrain, TI_TCDRAIN, int)
WDEFSTUB1(wait, TI_WAIT, int *)
WDEFSTUB3(waitpid, TI_WAITPID, pid_t, int *, int)
WDEFSTUB3(write, TI_WRITE, int, const void *, size_t)
DEFSTUB1(pthread_cond_broadcast, TI_PCOND_BROAD, pthread_cond_t *)
DEFSTUB1(pthread_cond_destroy, TI_PCOND_DEST, pthread_cond_t *)
DEFSTUB2(pthread_cond_init, TI_PCOND_INIT, pthread_cond_t *,
	const pthread_condattr_t *)
DEFSTUB1(pthread_cond_signal, TI_PCOND_SIGNAL, pthread_cond_t *)
DEFSTUB3(pthread_cond_timedwait, TI_PCOND_TWAIT, pthread_cond_t *,
	pthread_mutex_t *, const struct timespec *)
DEFSTUB2(pthread_cond_wait, TI_PCOND_WAIT, pthread_cond_t *,
	pthread_mutex_t *)
DEFSTUB1(pthread_condattr_destroy, TI_PCONDA_DEST, pthread_condattr_t *)
DEFSTUB2(pthread_condattr_getpshared, TI_PCONDA_GETPS,
	const pthread_condattr_t *, int *)
DEFSTUB1(pthread_condattr_init, TI_PCONDA_INIT, pthread_condattr_t *)
DEFSTUB2(pthread_condattr_setpshared, TI_PCONDA_SETPS,
	pthread_condattr_t *, int *)
DEFSTUB1(pthread_mutex_destroy, TI_PMUTEX_DEST, pthread_mutex_t *)
DEFSTUB2(pthread_mutex_getprioceiling, TI_PMUTEX_GPC,
	pthread_mutex_t *, int *)
DEFSTUB2(pthread_mutex_init, TI_PMUTEX_INIT, pthread_mutex_t *,
	const pthread_mutexattr_t *)
DEFSTUB1(pthread_mutex_lock, TI_PMUTEX_LOCK, pthread_mutex_t *)
DEFSTUB3(pthread_mutex_setprioceiling, TI_PMUTEX_SPC,
	pthread_mutex_t *, int, int *)
DEFSTUB1(pthread_mutex_trylock, TI_PMUTEX_TRYL, pthread_mutex_t *)
DEFSTUB1(pthread_mutex_unlock, TI_PMUTEX_UNLCK, pthread_mutex_t *)
DEFSTUB1(pthread_mutexattr_destroy, TI_PMUTEXA_DEST, pthread_mutexattr_t *)
DEFSTUB2(pthread_mutexattr_getprioceiling, TI_PMUTEXA_GPC,
	const pthread_mutexattr_t *, int *)
DEFSTUB2(pthread_mutexattr_getprotocol, TI_PMUTEXA_GP,
	const pthread_mutexattr_t *, int *)
DEFSTUB2(pthread_mutexattr_getpshared, TI_PMUTEXA_GPS,
	const pthread_mutexattr_t *, int *)
DEFSTUB1(pthread_mutexattr_init, TI_PMUTEXA_INIT, pthread_mutexattr_t *)
DEFSTUB2(pthread_mutexattr_setprioceiling, TI_PMUTEXA_SPC,
	pthread_mutexattr_t *, int)
DEFSTUB2(pthread_mutexattr_setprotocol, TI_PMUTEXA_SP,
	pthread_mutexattr_t *, int)
DEFSTUB2(pthread_mutexattr_setpshared, TI_PMUTEXA_SPS,
	pthread_mutexattr_t *, int)
RDEFSTUB0(thr_min_stack, TI_THR_MINSTACK, size_t)
SDEFSTUB3(__sigtimedwait, TI_SIGTIMEDWAIT, const sigset_t *,
	siginfo_t *, const struct timespec *)
RDEFSTUB1(alarm, TI_ALARM, unsigned, unsigned)
DEFSTUB3(setitimer, TI_SETITIMER, int, const struct itimerval *,
	struct itimerval *)
VDEFSTUB2(siglongjmp, TI_SIGLONGJMP, sigjmp_buf, int)
WDEFSTUB1(sigpending, TI_SIGPENDING, sigset_t *)
SDEFSTUB2(__nanosleep, TI__NANOSLEEP, const struct timespec *,
	struct timespec *)
WDEFSTUB3(open64, TI_OPEN64, int, int, mode_t)
WDEFSTUB2(creat64, TI_CREAT64, const char *, mode_t)
DEFSTUB1(rwlock_destroy, TI_RWLCKDESTROY, rwlock_t *)
DEFSTUB1(sema_destroy, TI_SEMADESTROY, sema_t *)
