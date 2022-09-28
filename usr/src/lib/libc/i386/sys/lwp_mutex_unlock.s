	.ident	"@(#)lwp_mutex_unlock.s	1.10	96/06/01 SMI"

	.file	"lwp_mutex_unlock.s"

	.text

_fwdef_(`_lwp_mutex_unlock'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	4(%esp),%eax
	addl	$M_LOCK_WORD,%eax
	xorl	%ecx,%ecx
	xchgl	(%eax),%ecx	/ clear lock and get old lock into %ecx
	andl	$M_WAITER_MASK,%ecx / was anyone waiting on it?
	je	noerror
	movl	$LWP_MUTEX_UNLOCK,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	ret
noerror:
	xorl	%eax,%eax
	ret
	.size	_lwp_mutex_unlock,.-_lwp_mutex_unlock
