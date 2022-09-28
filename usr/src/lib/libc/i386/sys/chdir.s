.ident	"@(#)chdir.s	1.7	96/06/01 SMI"

/ chdir

	.file	"chdir.s"

	.text

	.globl	__cerror

_fwdef_(`chdir'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$CHDIR,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror:
	xorl	%eax,%eax
	ret
