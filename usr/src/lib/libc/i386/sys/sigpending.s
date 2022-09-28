	.ident	"@(#)sigpending.s	1.9	96/06/01 SMI"

/ C library -- setsid, setpgid, getsid, getpgid


	.file	"sigpending.s"

	.text

	.globl	__sigfillset
	.globl  __cerror

_fwpdef_(`_sigpending', `_libc_sigpending'):
	popl	%edx
	pushl	$1
	pushl	%edx
	jmp	sys

_fwdef_(`__mt_sigpending'):
        popl    %edx
        pushl   $3
        pushl   %edx
        jmp     sys

_fgdef_(`__sigfillset'):
	popl	%edx
	pushl	$2
	pushl	%edx
	jmp	sys

sys:
	movl	$SIGPENDING,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	popl	%edx
	movl	%edx,0(%esp)	/ Remove extra word
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror:
	ret
