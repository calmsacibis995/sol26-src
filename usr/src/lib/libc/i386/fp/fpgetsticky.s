	.file	"fpgetsticky.c"
	.version	"01.01"
	.ident	"@(#)fpgetsticky.s	1.1	92/04/17 SMI"
	.set	EXCPMASK,63
	.text
	.align	4

_fwdef_(`fpgetsticky'):
	pushl	%ebp
	movl	%esp,%ebp
	subl	$8,%esp
	leal	-4(%ebp),%eax
	fstsw   (%eax)
	movl	-4(%ebp),%eax
	andl	$EXCPMASK,%eax
	leave
	ret
