.ident	"@(#)getppid.s	1.2	96/06/01 SMI"

	.file	"getppid.s"

	.text

_fwdef_(`getppid'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$GETPID,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	movl	%edx,%eax
	ret
