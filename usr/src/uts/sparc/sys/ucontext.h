/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_UCONTEXT_H
#define	_SYS_UCONTEXT_H

#pragma ident	"@(#)ucontext.h	1.16	96/04/24 SMI"	/* from SVr4.0 1.10 */

#include <sys/feature_tests.h>

#include <sys/types.h>
#include <sys/regset.h>
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#include <sys/signal.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Inclusion of <sys/signal.h> for sigset_t and stack_t definitions
 * breaks XPG4v2 namespace.  Therefore we must duplicate the defines
 * for these types here when _XPG4_2 is defined.
 */

#if defined(_XPG4_2) && !defined(__EXTENSIONS__)
#ifndef	_SIGSET_T
#define	_SIGSET_T
typedef	struct {	/* signal set type */
	unsigned long	__sigbits[4];
} sigset_t;
#endif /* _SIGSET_T */

#ifndef	_STACK_T
#define	_STACK_T
typedef	struct {
	void	*ss_sp;
	size_t	ss_size;
	int	ss_flags;
} stack_t;
#endif /* _STACK_T */
#endif /* defined(_XPG4_2) && !defined(__EXTENSIONS__) */

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
typedef	struct ucontext ucontext_t;
#else
typedef	struct __ucontext ucontext_t;
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
struct	ucontext {
#else
struct	__ucontext {
#endif
	ulong_t		uc_flags;
	ucontext_t	*uc_link;
	sigset_t   	uc_sigmask;
	stack_t 	uc_stack;
	mcontext_t 	uc_mcontext;
	long		uc_filler[23];
};

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#define	GETCONTEXT	0
#define	SETCONTEXT	1

/*
 * values for uc_flags
 * these are implementation dependent flags, that should be hidden
 * from the user interface, defining which elements of ucontext
 * are valid, and should be restored on call to setcontext
 */

#define	UC_SIGMASK	001
#define	UC_STACK	002
#define	UC_CPU		004
#define	UC_MAU		010
#define	UC_FPU		UC_MAU
#define	UC_INTR		020

#define	UC_MCONTEXT	(UC_CPU|UC_FPU)

/*
 * UC_ALL specifies the default context
 */

#define	UC_ALL		(UC_SIGMASK|UC_STACK|UC_MCONTEXT)
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#ifdef _KERNEL
void savecontext(ucontext_t *, k_sigset_t);
#endif

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_UCONTEXT_H */
