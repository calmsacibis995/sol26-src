	.ident	"@(#)pathconf.s	1.7	96/06/01 SMI"

	.file	"pathconf.s"
	
	.text

	.globl	__cerror

_fwdef_(`pathconf'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$PATHCONF,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror:
	ret
