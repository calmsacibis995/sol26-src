	.ident	"@(#)ucontext.s	1.7	96/06/01 SMI"


	.file	"ucontext.s"

	.globl	__getcontext
	.globl	__cerror

_fgdef_(`__getcontext'):
	popl	%edx
	pushl	$0
	pushl	%edx
	jmp 	sys

_fwdef_(`setcontext'):
	popl	%edx
	pushl	$1
	pushl	%edx
	jmp 	sys

sys:
	MCOUNT	
	movl	$UCONTEXT,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	popl	%edx
	movl	%edx,0(%esp)
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror:
	ret
