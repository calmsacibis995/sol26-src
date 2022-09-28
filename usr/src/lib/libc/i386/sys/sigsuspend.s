	.ident	"@(#)sigsuspend.s	1.8	96/06/01 SMI"

	.file	"sigsuspend.s"

	.globl	__cerror

_fwpdef_(`_sigsuspend', `_libc_sigsuspend'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$SIGSUSPEND,%eax
	_prologue_
	movl	_daref_(_sigreturn),%edx
	_epilogue_
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

_sigreturn:
	addl	$4,%esp		/ return args to user interrupt routine
	lcall   $SIGCLEAN_TRAPNUM,$0    / return to kernel to return to user
