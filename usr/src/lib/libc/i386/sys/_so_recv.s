/
/ Copyright (c) 1996, by Sun Microsystems, Inc.
/ All rights reserved.
/

	.ident	"@(#)_so_recv.s	1.5	96/09/23 SMI"


	.file	"_so_recv.s"

	.text

	.globl	__cerror
	.globl	_so_recv

_fgdef_(_so_recv):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$RECV,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	cmpb	$ERESTART,%al
	je	_so_recv
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)

noerror:
	ret
