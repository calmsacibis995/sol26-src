	.ident	"@(#)lwp_cond_signal.s	1.8	96/06/01 SMI"

	.file	"lwp_cond_signal.s"

	.text

	.globl	__cerror

_fwdef_(`_lwp_cond_signal'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$LWP_COND_SIGNAL,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	1f
	ret
1:
	xorl	%eax,%eax
	ret
	.size	_lwp_cond_signal,.-_lwp_cond_signal
