.ident	"@(#)creat.s	1.11	96/06/01 SMI"

/ Here's what's going on. The top m4 ifdef decides whether
/ to build the transitional 64bit api depending on whether
/ or not _LARGEFILE64_SOURCE is defined.
	
	.file	"creat.s"

	.text


	.globl	__cerror
_m4_ifdef_(`_LARGEFILE_INTERFACE',
`_fwpdef_(`_creat64', `_libc_creat64'):
	MCOUNT			
	movl	$CREAT64,%eax
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
`_fwpdef_(`_creat', `_libc_creat'):
	MCOUNT			
	movl	$CREAT,%eax
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
