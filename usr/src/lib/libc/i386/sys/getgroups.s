	.ident	"@(#)getgroups.s	1.7	96/06/01 SMI"

/ gid = getgroups();
/ returns effective gid

	.file	"getgroups.s"

	.text

	.globl  __cerror

_fwdef_(`getgroups'):
	MCOUNT
	movl	$GETGROUPS,%eax
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
