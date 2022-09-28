	
.ident	"@(#)getpid.s	1.2	96/06/01 SMI"

	.file	"getpid.s"

	.text


_fwdef_(`getpid'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$GETPID,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	ret
