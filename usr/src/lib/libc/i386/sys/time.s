.ident	"@(#)time.s	1.2	96/06/01 SMI"


	.file	"time.s"

	.text

_fwdef_(`time'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$TIME,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	movl	4(%esp),%edx
	testl	%edx,%edx
	jz	.nostore
	movl	%eax,(%edx)
.nostore:
	ret
