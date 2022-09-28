.ident	"@(#)getgid.s	1.2	96/06/01 SMI"

	.file	"getgid.s"

	.text

_fwdef_(`getgid'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$GETGID,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	ret
