	.ident	"@(#)waitid.s	1.8	96/06/01 SMI"

/ C library -- waitid

/ error = waitid(idtype,id,&info,options)

	.file	"waitid.s"
	
	.text

	.globl  __cerror

_fwdef_(`waitid'):
	MCOUNT
	movl	$WAITID,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae 	noerror		/ all OK - normal return
	cmpb	$ERESTART,%al	/  else, if ERRESTART
	je	waitid		/    then loop
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp 	_fref_(__cerror)	/  otherwise, error

noerror:
	ret
