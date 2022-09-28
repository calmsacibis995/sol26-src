	.file	"cuexit.s"

	.ident	"@(#)cuexit.s	1.8	96/06/01 SMI"

/ C library -- exit
/ exit(code)
/ code is return in %edx to system


	.globl	exit
	.align	4

_fgdef_(exit):
	MCOUNT
	_prologue_
	call	_fref_(_exithandle)
	movl	_esp_(4),%edx
	_epilogue_
	movl	$EXIT,%eax
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
