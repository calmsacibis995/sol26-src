/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */

	.ident	"@(#)thread.il	1.3	94/11/10 SMI"

	.file	"thread.il"

/*
 * _curthread()
 *
 * returns the value of the currently active thread.
 */
	.inline	_curthread, 4
	mr	%r3, %r2
	.end

	.inline _flush_store, 0
	eieio
	.end
