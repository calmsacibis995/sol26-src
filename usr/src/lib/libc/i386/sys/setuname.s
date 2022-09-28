	.file	"setuname.s"

	.ident	"@(#)setuname.s	1.5	96/06/01 SMI"


	.globl	setuname
	.set	SETUNAME,3

setuname:
	MCOUNT			/ subroutine entry counter if profiling
	pushl	$SETUNAME
	pushl	$0
	pushl	12(%esp)	/ retaddr+$SETUNAME+$0
	subl	$4,%esp
	movl	$UTSSYS,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jc	.cerror
	addl	$16,%esp
	xorl	%eax,%eax
	ret

.cerror:
	_prologue_
_m4_ifdef_(`DSHLIB',
`	movl	_daref_(errno),%ecx
	movl	%eax,(%ecx)
',
`	movl	%eax,errno
')
	_epilogue_
	movl	$-1,%eax
	addl	$16,%esp
	ret
