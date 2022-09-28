.ident	"@(#)fchmod.s	1.7	96/06/01 SMI"

/ error = fchmod(fd)

	.file	"fchmod.s"

	.text

	.globl  __cerror

_fwdef_(`fchmod'):
	MCOUNT
	movl	$FCHMOD,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp 	_fref_(__cerror)
noerror:
	xorl	%eax,%eax
	ret
