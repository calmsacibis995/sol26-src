	.file	"fpcw.c"
	.version "01.01"
	.ident	"@(#)fpcw.s	1.3 0 SMI"
	.text
	.align	4

_fwdef_(`_getcw'):
	movl	4(%esp), %eax
	fstcw	(%eax)
	ret

_fwdef_(`_putcw'):
	fldcw	4(%esp)
	ret
