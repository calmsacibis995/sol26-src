
/ Determine the sign of a double-long number.

	.ident	"@(#)lsign.s	1.1	92/04/17 SMI"
	.file	"lsign.s"
	.text

_fwdef_(`lsign'):

	MCOUNT

	movl	8(%esp),%eax
	roll	%eax
	andl	$1,%eax

	ret
