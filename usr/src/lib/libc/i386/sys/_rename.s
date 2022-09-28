.ident	"@(#)_rename.s	1.7	96/06/01 SMI"

/ _rename is the system call version of rename()


	.file	"_rename.s"

	.text

	.globl	_rename
	.globl	__cerror

_fgdef_(_rename):
	MCOUNT
	movl	$RENAME,%eax
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
