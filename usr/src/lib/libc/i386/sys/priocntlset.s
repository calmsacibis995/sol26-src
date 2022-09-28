	.ident	"@(#)priocntlset.s	1.7	96/06/01 SMI"

/ gid = priocntlset();
/ returns effective gid

	.file	"priocntl.s"

	.text

	.globl  __cerror
	.globl	__priocntlset

_fgdef_(`__priocntlset'):
	MCOUNT
	movl	$PRIOCNTLSET,%eax
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
