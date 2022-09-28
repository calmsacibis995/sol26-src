.ident	"@(#)getegid.s	1.2	96/06/01 SMI"

	.file	"getegid.s"

	.text

_fwdef_(`getegid'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$GETGID,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	movl	%edx,%eax
	ret
