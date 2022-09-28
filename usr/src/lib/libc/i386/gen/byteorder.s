	.file	"byteorder.s"

	.ident	"@(#)byteorder.s	1.2	93/06/24 SMI"

	.text
	.globl	htonl
	.globl	ntohl
	.globl	htons
	.globl	ntohs
	.align	4

/	unsigned long htonl( hl )
/	unsigned long ntohl( hl )
/	long hl;
/	reverses the byte order of 'long hl'
/

_fgdef_(htonl):
_fgdef_(ntohl):
	MCOUNT
	movl	4(%esp), %eax
	xchgb	%ah, %al
	rorl	$16, %eax
	xchgb	%ah, %al
	clc
	ret

/	unsigned short htons( hs )
/	short hs;
/
/	reverses the byte order in hs.
/

_fgdef_(htons):
_fgdef_(ntohs):
	MCOUNT
	movzwl	4(%esp), %eax
	xchgb	%ah, %al
	clc
	ret
