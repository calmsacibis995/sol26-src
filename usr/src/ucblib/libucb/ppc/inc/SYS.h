/*
 *       Copyright (c) 1994 by Sun Microsystems, Inc.
 */

#ifndef	_LIBC_PPC_INC_SYS_H
#define	_LIBC_PPC_INC_SYS_H

#pragma ident "@(#)SYS.h 1.1	94/06/02 SMI"

/*
 * This file defines common code sequences for system calls.
 *
 * NOTE: The branches to _cerror and _cerror64 are unconditional so that
 * they have the large displacement field needed to branch anywhere within
 * the containing module or to the procedure linkage table if the error
 * handling code is in another module.
 *
 * WARNING: Some of these macros contain local labels, and so can only
 * be included once per source file to prevent multiple definitions.
 */
#include <sys/asm_linkage.h>
#include <sys/syscall.h>
#include <sys/errno.h>
#include "synonyms.h"

#undef BUG_1163010		/* "##" macro concatenation bug */

#if defined(_ASM) /* The remainder of this file is only for assembly files */

/*
 * Define the external symbol _cerror for all files.
 */
	.global	_cerror
	.global	_cerror64

/*
 * SYSTRAP provides the actual trap sequence. It assumes that an entry
 * of the form SYS_name exists (probably from syscall.h).
 *
 * NOTE: ## is the ANSI C concatenation operator for token replacement
 * during macro expansion.  If either of the two adjacent tokens is a
 * replaceable parameter, then ## and any whitespace surrounding it are
 * deleted.
 */
#if defined(__STDC__) && defined(BUG_1163010)

#define	SYSTRAP(name) \
	li %r0, SYS_ ## name; \
	sc

#else			/* use the "empty comment" method */

#define	SYSTRAP(name) \
	li %r0, SYS_/**/name; \
	sc

#endif			/* defined(__STDC__) */

/*
 * SYSCERROR provides the sequence to branch to _cerror or (_cerror64) if an
 * error is indicated by cr0[so] being set upon return from a system call.
 */
#define	SYSCERROR \
	bns+	.ok; \
	POTENTIAL_FAR_BRANCH(_cerror); \
.ok:

/*
 * SYSCERROR64 is like SYSCERROR, but is used in cases where the error return
 * is a 64-bit value.
 */
#define	SYSCERROR64 \
	bns+	.ok; \
	POTENTIAL_FAR_BRANCH(_cerror64); \
.ok:

/*
 * SYSLWPERR provides the sequence to return 0 on a successful trap and the
 * error code if unsuccessful.
 * Error is indicated by cr0[so] being set upon return from a system call.
 */
#define	SYSLWPERR \
	bns+	.lwp_ok; \
	cmpwi	%r3, ERESTART; \
	bne+	.lwp_err; \
	li	%r3, EINTR; \
	b	.lwp_err; \
.lwp_ok: \
	li	%r3, 0; \
.lwp_err:

/*
 * SYSREENTRY provides the entry sequence for restartable system calls.
 */
#if defined(__STDC__) && defined(BUG_1163010)

#define	SYSREENTRY(name) \
	.text; \
	.global	name; \
	.align	2; \
	.type	name, @function; \
.restart_ ## name: \
	mr	%r3, %r10; \
	b	.r3saved; \
name: \
	MCOUNT(name); \
	mr	%r10, %r3; \
.r3saved:

#else			/* use the "empty comment" method */

#define	SYSREENTRY(name) \
	.text; \
	.global	name; \
	.align	2; \
	.type	name, @function; \
.restart_/**/name: \
	mr	%r3, %r10; \
	b	.r3saved; \
name: \
	MCOUNT(name); \
	mr	%r10, %r3; \
.r3saved:

#endif			/* defined(__STDC__) */

/*
 * SYSRESTART provides the error handling sequence for restartable
 * system calls.
 */
#define	SYSRESTART(restart_name) \
	bns+	.norestart; \
	cmpwi	%r3, ERESTART; \
	beq+	restart_name; \
	POTENTIAL_FAR_BRANCH(_cerror); \
.norestart:
 
/*
 * SYSINTR_RESTART provides the error handling sequence for restartable
 * system calls in case of EINTR or ERESTART.
 */
#define	SYSINTR_RESTART(restart_name) \
	bns+	.noeintr; \
	cmpwi	%r3, ERESTART; \
	beq+	restart_name; \
	cmpwi	%r3, EINTR; \
	beq+	restart_name; \
.noeintr:
 
/*
 * Standard syscall return sequence.
 */
#define	RET \
	blr-

/*
 * Syscall return sequence with return code forced to zero.
 */
#define	RETZ \
	li	%r3, 0; \
	blr-

#endif /* _ASM */

#endif	/* _LIBC_PPC_INC_SYS_H */
