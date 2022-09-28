	.ident	"@(#)writev.s	1.8	96/06/01 SMI"

/ OS library -- writev 

/ error = writev(fd, iovp, iovcnt)

	.file	"writev.s"

	.text

	.globl	__cerror

_fwdef_(`writev'):
	MCOUNT
	movl	$WRITEV,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	cmpb	$ERESTART,%al
	je	writev
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp 	_fref_(__cerror)
noerror:
	ret
