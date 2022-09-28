	.ident	"@(#)lwp_cond_broadcast.s	1.8	96/06/01 SMI"

	.file	"lwp_cond_broadcast.s"

	.text

_fwdef_(`_lwp_cond_broadcast'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$LWP_COND_BROADCAST,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	1f
	ret
1:
	xorl	%eax,%eax
	ret
	.size	_lwp_cond_broadcast,.-_lwp_cond_broadcast
