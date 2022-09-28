/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)door.s	1.7	96/06/14 SMI"

	.file	"door.s"

#include <sys/asm_linkage.h>
#include <sys/door.h>

	ANSI_PRAGMA_WEAK(_door_create,function)
	ANSI_PRAGMA_WEAK(_door_call,function)
	ANSI_PRAGMA_WEAK(_door_return,function)
	ANSI_PRAGMA_WEAK(_door_revoke,function)
	ANSI_PRAGMA_WEAK(_door_info,function)
	ANSI_PRAGMA_WEAK(_door_cred,function)
	ANSI_PRAGMA_WEAK(_door_bind,function)
	ANSI_PRAGMA_WEAK(_door_unbind,function)

#include "SYS.h"
#include "PIC.h"

	.global	_getpid

/*
 * Pointer to server create function
 */
	.section	".bss"
	.common	__door_server_func, 4, 4
	.common	__thr_door_server_func, 4, 4
	.common	__door_create_pid, 4, 4

	.text

/*
 * int
 * _door_create(void (*)(), void *, u_int)
 */
	ENTRY(_door_create)
	addi	%r8, 0, DOOR_CREATE	! subcode
	SYSTRAP(door)
	SYSCERROR
	RET
	SET_SIZE(_door_create)

/*
 * int
 * _door_revoke(int)
 */
	ENTRY(_door_revoke)
	addi	%r8, 0, DOOR_REVOKE	! subcode
	SYSTRAP(door)
	SYSCERROR
	RET
	SET_SIZE(_door_revoke)
/*
 * int
 * _door_info(int, door_info_t *)
 */
	ENTRY(_door_info)
	addi	%r8, 0, DOOR_INFO	! subcode
	SYSTRAP(door)
	SYSCERROR
	RET
	SET_SIZE(_door_info)

/*
 * int
 * _door_cred(door_cred_t *)
 */
	ENTRY(_door_cred)
	addi	%r8, 0, DOOR_CRED	! subcode
	SYSTRAP(door)
	SYSCERROR
	RET
	SET_SIZE(_door_cred)

/*
 * int
 * _door_bind(int d)
 */
	ENTRY(_door_bind)
	addi	%r8, 0, DOOR_BIND	! subcode
	SYSTRAP(door)
	SYSCERROR
	RET
	SET_SIZE(_door_bind)

/*
 * int
 * _door_unbind()
 */
	ENTRY(_door_unbind)
	addi	%r8, 0, DOOR_UNBIND	! subcode
	SYSTRAP(door)
	SYSCERROR
	RET
	SET_SIZE(_door_unbind)

/*
 * int
 * _door_call(int d, door_arg_t *param)
 */
	ENTRY(_door_call);
	addi	%r8, 0, DOOR_CALL	! subcode
	SYSTRAP(door)
	SYSCERROR
	RET
	SET_SIZE(_door_call)

/*
 * Offsets of struct door_results members passed on the stack from the kernel
 */
#define	DOOR_COOKIE	(SA(MINFRAME) + 0)
#define	DOOR_DATA_PTR	(SA(MINFRAME) + 4)
#define	DOOR_DATA_SIZE	(SA(MINFRAME) + 8)
#define	DOOR_DESC_PTR	(SA(MINFRAME) + 12)
#define	DOOR_DESC_SIZE	(SA(MINFRAME) + 16)
#define	DOOR_PC		(SA(MINFRAME) + 20)
#define	DOOR_SERVERS	(SA(MINFRAME) + 24)
#define	DOOR_INFO_PTR	(SA(MINFRAME) + 28)

/*
 * _door_return(void *, char *, int, door_desc_t *, caddr_t)
 */
	ENTRY(_door_return)
	subi	%r7, %r2, SA(MINFRAME)	! base of thread stack
door_restart:
	addi	%r8, 0, DOOR_RETURN	! subcode
	SYSTRAP(door)
	bns+	2f
	/* Error during door_return call. errno set */
	cmpi	0, %r3, EINTR
	bne	1f

	MINSTACK_SETUP
	POTENTIAL_FAR_CALL(_getpid)	! get current pid
	PIC_SETUP()
	mflr	%r31
	lwz	%r12, __thr_door_server_func@got(%r31)
	lwz	%r12, 0(%r12)
	MINSTACK_RESTORE

	cmp	%r3, %r12	! compare with "old" pid
	beq	door_restart
	li	%r3, EINTR		/* return EINTR to child of fork */
1:
	POTENTIAL_FAR_BRANCH(_cerror)
	RET
2:
	/*
	 * All new invocations come here
	 *
	 * on return, we're serving a door_call:
	 *	descriptors (if any)
	 *	data (if any)
	 *	struct door_results
	 *	MINFRAME
	 * sp ->
	 */
	lwz	%r3, DOOR_SERVERS(%r1)
	cmpi	0, %r3, 0
	bne+	3f
	/* 
	 * This is the last server thread, create more (maybe)
	 */
	lwz	%r3, DOOR_INFO_PTR(%r1)	! door_info pointer
	mflr	%r0
	stw	%r0, 4(%r1)
	stwu	%r1, -32(%r1)	! drop stack, establish frame

	bl	_GLOBAL_OFFSET_TABLE_@local-4
	mflr	%r31
	lwz	%r12, __thr_door_server_func@got(%r31)
	lwz	%r12, 0(%r12)
	mtlr	%r12		! create more server threads
	blrl

	addi	%r1, %r1, +32
	lwz	%r0, 4(%r1)
	mtlr	%r0
3:
	/* Invoke the door server function */
	lwz	%r3, DOOR_COOKIE(%r1)
	lwz	%r4, DOOR_DATA_PTR(%r1)
	lwz	%r5, DOOR_DATA_SIZE(%r1)
	lwz	%r6, DOOR_DESC_PTR(%r1)
	lwz	%r7, DOOR_DESC_SIZE(%r1)
	lwz	%r8, DOOR_PC(%r1)
	mtlr	%r8		! invoke the server function
	blrl

	/*
	 * If we get here, it means the server function didn't call
	 * door_return, so we just exit.
	 */
	POTENTIAL_FAR_CALL(thr_exit)
	SET_SIZE(_door_return)
