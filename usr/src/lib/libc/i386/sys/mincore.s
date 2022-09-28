	.ident	"@(#)mincore.s	1.7	96/06/01 SMI"

/ gid = mincore();
/ returns effective gid

	.file	"mincore.s"

	.text

	.globl  __cerror

_fwdef_(`mincore'):
	MCOUNT
	movl	$MINCORE,%eax
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
