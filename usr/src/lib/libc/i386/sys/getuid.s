.ident	"@(#)getuid.s	1.2	96/06/01 SMI"

	.file	"getuid.s"

	.text

_fwdef_(`getuid'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$GETUID,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	ret
