.ident	"@(#)acl.s	1.2	96/06/01 SMI"

	.file	"acl.s"

	.text

	.globl	__cerror

_fwdef_(`acl'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$ACL,%eax
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
