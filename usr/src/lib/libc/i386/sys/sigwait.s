	.ident	"@(#)sigwait.s	1.5	96/06/01 SMI"

	.file	"sigwait.s"

	.text

	.globl	__cerror
	.globl	_libc_sigwait

_fgdef_(`_libc_sigwait'):
	MCOUNT			/ subroutine entry counter if profiling
	pushl	$0
	pushl	$0
	movl	$SIGTIMEDWAIT,%eax
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
