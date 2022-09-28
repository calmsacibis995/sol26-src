	.ident	"@(#)fxstat.s	1.7	96/06/01 SMI"

/ error = _fxstat(file, statbuf);
/ char statbuf[34]


	.file	"fxstat.s"

	.text
	
	.globl  __cerror
	.globl  _fxstat

_fgdef_(`_fxstat'):
	MCOUNT
	movl	$FXSTAT,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp 	_fref_(__cerror)
noerror:
	xorl	%eax,%eax
	ret
