	.ident	"@(#)sysinfo.s	1.8	96/06/01 SMI"


	.file	"sysinfo.s"

	.text

	.globl	__cerror

_fwdef_(`sysinfo'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$SYSINFO,%eax
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
