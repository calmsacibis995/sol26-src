.ident	"@(#)exit.s	1.3	96/10/08 SMI"

	.file	"exit.s"

	.text

	.globl	_exit

_fgdef_(_exit):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$EXIT,%eax
	lcall	$0x7,$0
