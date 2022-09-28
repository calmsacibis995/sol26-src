	.ident	"@(#)getpmsg.s	1.8	96/06/01 SMI"

/ gid = getpmsg();
/ returns effective gid

	.file	"getpmsg.s"

	.text

	.globl  __cerror

_fwdef_(`getpmsg'):
	MCOUNT
	movl	$GETPMSG,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	cmpb	$ERESTART,%al
	je	getpmsg
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror:
	ret
