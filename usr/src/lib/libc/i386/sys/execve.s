.ident	"@(#)execve.s	1.7	96/06/01 SMI"

	.file	"execve.s"

	.text

	.globl	__cerror

_fwdef_(`execve'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$EXECE,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
