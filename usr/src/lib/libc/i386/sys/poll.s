	.ident	"@(#)poll.s	1.9	96/06/01 SMI"

	.file	"poll.s"

	.text

	.globl	__cerror

_fwdef_(`poll'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$POLL,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror:
	ret
