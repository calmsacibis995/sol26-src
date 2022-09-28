	.ident	"@(#)getmsg.s	1.8	96/06/01 SMI"

	.file	"getmsg.s"

	.text

	.globl	__cerror

_fwdef_(`getmsg'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$GETMSG,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	cmpb	$ERESTART,%al
	je	getmsg
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror:
	ret
