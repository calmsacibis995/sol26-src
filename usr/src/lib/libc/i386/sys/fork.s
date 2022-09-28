.ident	"@(#)fork.s	1.8	96/06/01 SMI"

/ pid = fork();

/ %edx == 0 in parent process, %edx = 1 in child process.
/ %eax == pid of child in parent, %eax == pid of parent in child.

	.file	"fork.s"
	
	.text

	.globl	__cerror
	.globl	_libc_fork

_fgdef_(`_libc_fork'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$FORK,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror:
	testl	%edx,%edx
	jz	.parent
	xorl	%eax,%eax
.parent:
	ret
