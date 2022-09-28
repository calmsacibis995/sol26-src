/	.asciz	"@(#)_div64.s 1.4 96/06/07 SMI"

	.file	"div64.s"
/
/ 64-bit integer division.
/
/
/  MetaWare Runtime Support: 64 bit division
/  (c) Copyright by MetaWare,Inc 1992.  
/
/
/	unsigned long long __udiv64(unsigned long long a,
/				    unsigned long long b)
/	   -- 64-bit unsigned division: a/b
/
/  Quotient in %edx/%eax
/
	.globl	__udiv64
_fgdef_(__udiv64):
	pushl	%esi
	call	divmod64
	popl	%esi
	ret	$16
/
/	unsigned long long __urem64(long long a,long long b)
/	      {64 BIT UNSIGNED MOD}
/
	.globl	__urem64
_fgdef_(__urem64):
	pushl	%esi
	call	divmod64
	mov	%esi,%eax
	mov	%ecx,%edx
	popl	%esi
	ret	$16

/
/	long long __div64(long long a, long long b)
/	   -- 64-bit signed division: a/b
/
	.globl	__div64
_fgdef_(__div64):
	pushl	%esi
	cmpl	$0,20(%esp)
/  if B < 0
	jge	Lpos
	negl	16(%esp); adcl $0,20(%esp); negl 20(%esp)
	cmpl	$0,12(%esp)
	jge	Lnegate_result
	negl	8(%esp); adcl $0,12(%esp); negl 12(%esp)
    Ldoit:
	call	divmod64
	popl	%esi
	ret	$16
Lpos:   cmpl	$0,12(%esp)
	jge	Ldoit
	negl	8(%esp); adcl $0,12(%esp); negl 12(%esp)
Lnegate_result:
	call	divmod64
	negl	%eax
	adcl	$0,%edx
	negl	%edx
	popl	%esi
	ret	$16
/
/	long long __rem64(long long a, long long b)
/	   -- 64-bit signed moduloo: a%b
/
	.globl	__rem64
_fgdef_(__rem64):
	pushl	%esi
	cmpl	$0,12(%esp)
/  if A < 0
	jge	L_pos
	negl	8(%esp); adcl $0,12(%esp); negl 12(%esp)
	cmpl	$0,20(%esp)
	jge	L_neg
	negl	16(%esp); adcl $0,20(%esp); negl 20(%esp)
    L_neg:
	call	divmod64
	subl	%edx,%edx
	subl	%eax,%eax
	subl	%esi,%eax
	sbbl	%ecx,%edx
	popl	%esi
	ret	$16
L_pos:   cmpl	$0,20(%esp)
	jge	L_doit
	negl	16(%esp); adcl $0,20(%esp); negl 20(%esp)
L_doit:
	call	divmod64
	movl	%esi,%eax
	movl	%ecx,%edx
	popl	%esi
	ret	$16
/
/	long long __udivrem64(long long a, long long b)
/	   -- 64-bit unsigned division and modulo: a%b
/	quotient in EDX/EAX;   remainder in ECX/ESI
/
	.globl	__udivrem64
_fgdef_(__udivrem64):
	push	%ebx	// Anything to set the stack up right
	call	divmod64
	popl	%ebx
	ret	$16
	.globl	__divrem64
_fgdef_(__divrem64):
/
/	long long __divrem64(long long a, long long b)
/	   -- 64-bit unsigned division and modulo: a%b
/	quotient in EDX/EAX;   remainder in ECX/ESI
/
	cmpl	$0,16(%esp)
	jl	Ldr
	cmpl	$0,8(%esp)
	jl	Ldr
	push	%ebx		// Anything to set the stack up right
	call	divmod64
	popl	%ebx
	ret	$16
Ldr:
	_prologue_
	pushl	_esp_(16)
	pushl	_esp_(16)
	pushl	_esp_(16)
	pushl	_esp_(16)
	call	__div64
	pushl	%edx		// Save quotient
	pushl	%eax
	pushl	_esp_(24)	// high half of divisor (16+8)
	pushl	_esp_(24)	// low half of divisor (12+12)
	pushl	%edx
	pushl	%eax
	call	_fref_(__mul64)
	movl	_esp_(12),%esi	// Low half of dividend
	movl	_esp_(16),%ecx	// High half of dividend
	subl	%eax,%esi
	sbbl	%edx,%ecx
	popl	%eax		//pop quotient
	popl	%edx
	_epilogue_
	ret	$16
	

/
/ General function for computing quotient and remainder for unsigned
/ 64-bit division.
/
/ First argument is at 16(%ebp),   second at 24(%ebp)
/
/ Quotient left in %edx/%eax;   remainder in %ecx,%esi
/
/ NOTE: clobbers %esi as side-effect.

divmod64:
	pushl	%ebp 
	movl	%esp,%ebp
	_prologue_
	movl	28(%ebp),%ecx	/ high half of divisor (operand b)
	andl	%ecx,%ecx
	jnz	L		/ Do full division
	/ If Bhi is 0, we can do long-hand division
	/ using the machine instructions, essentially treating the dividend
	/ as two digits in base 2**32.  For example:
	/                      4 560ffc74
	/             --------------
	/   10000000  | 4560ffc7 43000103
	/              -40000000
	/		--------
	/		 560ffc7 43000103
	/		-560ffc7 40000000
	/	         ----------------
	/			  3000103
	/
	/ Answer: 4_560ffc74, remainder 3000103
	/
	movl	24(%ebp),%ecx	/ Move low half of operand b
	movl	20(%ebp),%eax	/ Move high half of operand a
	subl	%edx,%edx	/ Zero out upper word
	divl	%ecx
	movl	%eax,%esi	/ Save high 32-bits of answer
	movl	16(%ebp),%eax	/ move low half of opereand a
	divl	%ecx            / Divide remainder of previous + low digit.
	subl	%ecx,%ecx	/ Top half of remainder is zero.
	xchg    %esi,%edx	/ Quotient in %edx/%eax, remainder in %ecx/%esi
	_epilogue_
	movl	%ebp,%esp
	popl	%ebp		/ Quotient is in dx:ax; remainder in cx:bx.
	ret
L: 	/ Come here to do full 64-bit division in which divisor is > 32 bits
        / Since the divisor is > 32 bits, the answer cannot be greater than
        / 32 bits worth.  We can divide dividend and divisor by equal amounts
        / and the answer will still be the same, except possibly off by one,
        / as long as we don't divide too much.  This division is done
        / by shifting.
        / We stop such division the first time that the top 32 bits of the
        / divisor are 0; then we use the machine divide.  We then test if
        / we're off by one by comparing the dividend with the quotient*divisor.
        / E.g.:
        / 	Ahi Alo      Ahi
        / 	-------  =>  ---
        / 	Bhi Blo      Bhi
        / If the top bit of Bhi is 1, the two answers are always the same
        / except that the second can possibly be 1 bigger than the first.
        / For example, consider two base 32 digits (rather than base 2**32):
        / 	A   0         A  
        / 	-------  =>  ---
        / 	A   F         A  
        / The second answer is one more than the first.
        / But the second answer can never be less than the first.
        / To prove this, consider minimizing the first divisor digit, 
        / maximizing the second, and maximizing the second dividend digit:           
        / 	X   F         X  
        / 	-------  =>  ---
        / 	1   0         1  
        / Since F is < 10, XF/10 can never be greater than X/1.  qed.
        /
        / To test if the result is 1 less we multiply by the answer and
        / form the remainder.  If the remainder is negative we add 1 to
        / the answer and add the divisor to the remainder.
        /movl	28(%ebp),%ecx		/ Already there.
	movl	24(%ebp),%esi		/ Low half of divisor (b)
	mov	20(%ebp),%edx		/ High half of a (dividend)
	mov	16(%ebp),%eax		/ Low half of dividend (a)
	/ Now shift cx:si and dx:ax right until the last 1 bit enters bx.
	/ Optimize away 16 bits of loop if top 16 bits of divisor <> 0.
	testl	$0xFFFF,%ecx
	jz	Lcheck_ch
	shrdl	$16,%edx,%eax
	shrl	$16,%edx
	shrdl	$16,%ecx,%esi
	shrl	$16,%ecx
Lcheck_ch:
	andb	%ch,%ch
	jz	Lmore
	/ Shift right by 8.
L8:	
	shrdl	$8,%edx,%eax
	shrl	$8,%edx
	shrdl	$8,%ecx,%esi
	shrl	$8,%ecx
	cmpb	$80,%cl  / Rare case: divisor >= 8000_0000.  Shift 8 again.
	jb 	L2    / If we do, we of course won't do it again.
	jmp	L8   
	/ Since Bhi <> 0 we can always shift at least once.
Lmore:  shrl	%ecx
	rcrl	%esi
	shrl	%edx
	rcrl	%eax
L2:	andl	%ecx,%ecx		/ Test top word of divisor.
	jnz	Lmore
	/ Divisor = 0000_xxxx where xxxx >= 8000.
	divl    %esi		/ eax is answer, or answer + 1.
	pushl	%eax		/ Save answer.
	subl	%edx,%edx
	pushl	%edx
	pushl	%eax
	pushl	28(%ebp)             / Multiply answer by divisor.
	pushl	24(%ebp)      / Subtract from dividend to obtain remainder.
	call	_fref_(__mul64)
	/ If product is > dividend, we went over by 1.  
	popl	%ecx		/ Get answer back.
	cmpl	20(%ebp),%edx	/ Compare high half of dividend
	ja	LToo_big
	jne	LNot_too_big
	cmpl	16(%ebp),%eax	/ Compare low half of dividend
	jna     LNot_too_big
LToo_big:	/ Add divisor to remainder; decrement answer.
	decl	%ecx
	subl	24(%ebp),%eax
	sbbl	28(%ebp),%edx
LNot_too_big:	/ Compute remainder.	
	movl	16(%ebp),%esi
	subl	%eax,%esi
	movl	%ecx,%eax	/ put low half of quotient in eax
	movl	20(%ebp),%ecx	/ put high half of quotient in ecx
	sbbl	%edx,%ecx
	subl	%edx,%edx	/ High half of quotient is zero
	/ Top 32 bits of quotient = 0 when top 32 bits of dividend <> 0.
	_epilogue_
	movl	%ebp,%esp
	popl	%ebp		/ Quotient is in dx:ax; remainder in cx:bx.
	ret
