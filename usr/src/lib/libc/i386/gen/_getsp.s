	.file	"_getsp.s"

	.ident	"@(#)_getsp.s	1.3	93/05/04 SMI"

	.text
	.globl	_getsp
	.globl	_getfp
	.globl	_getap
	.globl	_getbx
	.align	4

_fgdef_(_getsp):
	MCOUNT
	movl	%esp,%eax
	ret

_fgdef_(_getfp):
	MCOUNT
	movl	%ebp,%eax
	ret

_fgdef_(_getap):
	MCOUNT
	leal	8(%ebp),%eax
	ret

_fgdef_(_getbx):
	MCOUNT
	movl	4(%esp),%eax
	ret
