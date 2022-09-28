	.ident	"@(#)open.s	1.11	96/06/01 SMI"

/ Here's what's going on. The top m4 ifdef decides whether
/ to build the transitional 64bit api depending on whether
/ or not _LARGEFILE_INTERFACE is defined.
	
	.file	"open.s"
	
	.text

	.globl	__cerror


_m4_ifdef_(`_LARGEFILE_INTERFACE',	
`_fwpdef_(`_open64', `_libc_open64'):
	MCOUNT
	movl	$OPEN64,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror64
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror64:
	ret',
`_fwpdef_(`_open', `_libc_open'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$OPEN,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror:
	ret'
)
