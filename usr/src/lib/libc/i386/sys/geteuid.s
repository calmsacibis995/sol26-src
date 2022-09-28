.ident	"@(#)geteuid.s	1.2	96/06/01 SMI"

	.file	"geteuid.s"

	.text

_fwdef_(`geteuid'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$GETUID,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	movl	%edx,%eax
	ret
