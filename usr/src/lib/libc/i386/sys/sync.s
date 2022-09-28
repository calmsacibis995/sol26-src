.ident	"@(#)sync.s	1.2	96/06/01 SMI"


	.file	"sync.s"
	
	.text

_fwdef_(`sync'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$SYNC,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	ret
