/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/


	.ident "@(#)_fork1.s 1.3	94/11/02 SMI"
/* C library -- fork1						*/
/* pid_t fork1(void)						*/

/* From the syscall:
 * %r4 == 0 in parent process, %r4 == 1 in child process.
 * %r3 == pid of child in parent, %r3 == pid of parent in child.
 */

	.file	"_fork1.s"

#include "SYS.h"

	ENTRY(__fork1);
	SYSTRAP(fork1);
	SYSCERROR
	cmpi	%r4,0		!test for child
	bz	.parent 	!if 0, then parent - jump
	li	%r3, 0		!child, return (0)

.parent:
	RET			!parent, return (%r3 = child pid)
	SET_SIZE(__fork1)
