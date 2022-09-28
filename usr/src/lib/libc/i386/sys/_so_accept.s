/
/ Copyright (c) 1996, by Sun Microsystems, Inc.
/ All rights reserved.
/

	.ident	"@(#)_so_accept.s	1.5	96/09/23 SMI"


	.file	"_so_accept.s"

	.text

	.globl	__cerror
	.globl	_so_accept

_fgdef_(_so_accept):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$ACCEPT,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	cmpb	$ERESTART,%al
	je	_so_accept
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)

noerror:
	ret
