/*
 *	Copyright (c) 1994, Sun Microsystems, Inc.
 */

#ident	"@(#)lddstub.s	1.1	94/04/03 SMI"

/*
 * Stub file for ldd(1).  Provides for preloading shared libraries.
 */
#include <sys/syscall.h>

	.file	"lddstub.s"
	.text
	.align	2
	.global	stub

stub:
	li	%r3, 0
	li	%r0, SYS_exit
	sc
