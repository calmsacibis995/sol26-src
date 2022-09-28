	.ident "@(#)lock.s	1.2 SMI"

	.file	"lock.s"

	.text

/
/ lock_try(lp)
/	- returns non-zero on success.
/
_fwdef_(`_lock_try'):
	movl	$1,%eax
	movl	4(%esp),%ecx
	xchgb	%al, (%ecx)
	xorb	$1, %al
	ret

/
/ lock_clear(lp)
/	- clear lock and force it to appear unlocked in memory.
/
_fwdef_(`_lock_clear'):
	movl	4(%esp),%eax
	movb	$0, (%eax)
	ret

