	.ident	"@(#)putpmsg.s	1.8	96/06/01 SMI"

/ gid = putpmsg();
/ returns effective gid

	.file	"putpmsg.s"

	.text

	.globl  __cerror

_fwdef_(`putpmsg'):
	MCOUNT
	movl	$PUTPMSG,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	cmpb	$ERESTART,%al
	je	putpmsg
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror:
	ret
