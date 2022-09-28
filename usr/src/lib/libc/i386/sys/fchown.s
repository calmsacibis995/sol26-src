.ident	"@(#)fchown.s	1.7	96/06/01 SMI"

/ error = fchown(fd,owner,group)

	.file	"fchown.s"

	.text
	
	.globl  __cerror

_fwdef_(`fchown'):
	MCOUNT
	movl	$FCHOWN,%eax
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
