	.ident	"@(#)sbrk.s	1.11	96/06/01 SMI"

	.file	"sbrk.s"

	.text

	.globl	__sbrk_lock
	.globl	_nd

_fwdef_(`sbrk'):
	MCOUNT
	_prologue_
_m4_ifdef_(`DSHLIB',
`	movl	_daref_(__sbrk_lock),%ecx
	pushl	%ecx
	call	_fref_(_mutex_lock)
',
`	pushl	$__sbrk_lock
	call	_mutex_lock
')
	addl	$4,%esp			/ pop _sbrk_mutex pointer

	pushl	_esp_(4)		/ sbrk_unlocked gets sbrks arg
	call	_sbrk_unlocked
	addl	$4,%esp

	pushl	%eax			/ store it away

_m4_ifdef_(`DSHLIB',
`	movl	_daref_(__sbrk_lock),%ecx
	pushl	%ecx
	call	_fref_(_mutex_unlock)
',
`	pushl	$__sbrk_lock
	call	_mutex_unlock
')
	addl	$4,%esp			/ pop _sbrk_mutex pointer
	popl	%eax			/ saved here above
	_epilogue_
	ret

_fwdef_(`brk'):
	MCOUNT
	_prologue_
_m4_ifdef_(`DSHLIB',
`	movl	_daref_(__sbrk_lock),%ecx
	pushl	%ecx
	call	_fref_(_mutex_lock)
',
`	pushl	$__sbrk_lock
	call	_mutex_lock
')
	addl	$4,%esp

	pushl	_esp_(4)		/ brk_unlocked gets brks arg
	call	_brk_unlocked
	addl	$4,%esp

	pushl	%eax			/ save it away...

_m4_ifdef_(`DSHLIB',
`	movl	_daref_(__sbrk_lock),%ecx
	pushl	%ecx
	call	_fref_(_mutex_unlock)
',
`	pushl	$__sbrk_lock
	call	_mutex_unlock
')
	addl	$4,%esp
	popl	%eax			/ saved here above
	_epilogue_
	ret

_fwdef_(`_sbrk_unlocked'):
	MCOUNT			/ subroutine entry counter if profiling
	_prologue_
	movl	_esp_(4),%edx
	testl	%edx,%edx
	jz	.is_zero	/ We know the answer without asking
_m4_ifdef_(`DSHLIB',
`	movl	_daref_(_nd),%ecx
	addl	(%ecx),%edx
',
`	addl	_nd,%edx
')
	pushl	%edx
	call	_brk_unlocked
	addl	$4,%esp
	testl	%eax,%eax
	jnz	.brkerr
.is_zero:
_m4_ifdef_(`DSHLIB',
`	movl	_daref_(_nd),%ecx
	movl	(%ecx),%eax
',
`	movl	_nd,%eax
')
	subl	_esp_(4),%eax
.brkerr:
	_epilogue_
	ret


/ brk(value)
/ as described in brk(2).
/ returns 0 for ok, -1 for error

	.globl	__cerror

_fwdef_(`_brk_unlocked'):
	movl	$BRK,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror:
	_prologue_
	movl	_esp_(4),%edx
_m4_ifdef_(`DSHLIB',
`	movl	_daref_(_nd),%ecx
	movl	%edx,(%ecx)
',
`	movl    %edx,_nd
')
	xorl	%eax,%eax
	_epilogue_
	ret

	.data
_nd:
	.long	end
