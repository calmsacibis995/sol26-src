.ident	"@(#)alarm.s	1.6	96/06/01 SMI"

/ alarm 

	.file	"alarm.s"

	.text

	.globl _libc_alarm

_fgdef_(`_libc_alarm'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$ALARM,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	ret
