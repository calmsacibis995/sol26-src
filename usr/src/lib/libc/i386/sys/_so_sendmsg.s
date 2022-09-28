/
/ Copyright (c) 1996, by Sun Microsystems, Inc.
/ All rights reserved.
/

	.ident	"@(#)_so_sendmsg.s	1.5	96/09/23 SMI"


	.file	"_so_sendmsg.s"

	.text

	.globl	__cerror
	.globl	_so_sendmsg

_fgdef_(_so_sendmsg):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$SENDMSG,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	cmpb	$ERESTART,%al
	je	_so_sendmsg
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)

noerror:
	ret
