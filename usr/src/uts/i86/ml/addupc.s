/*
 *	Copyright (c) 1986 by Sun Microsystems, Inc.
 */

/*#ident	"@(#)addupc.s	1.1 - 92/01/30" */
/*#ident	"@(#)addupc.s	1.3	88/02/08 SMI"*/

#include <sys/asm_linkage.h>

/*
 *  struct uprof {
 *  	short	 *pr_base;
 *  	unsigned pr_size;
 *  	unsigned pr_off;
 *  	unsigned pr_scale;
 *  } ;
 *  addupc( pc, pr, incr)
 *  	register void (*pc)();
 *  	register struct uprof *pr;
 *  	int incr;
 *  {
 *  	register short *slot;
 *  	short counter;
 *  
 *  	if (pr->pr_scale == 2)
 *  		slot = pr->pr_base;
 *  	else
 *  		slot = pr->pr_base + 
 *  			((((int)pc-pr->pr_off) * pr->pr_scale) >> 16)/
 * 					(sizeof *slot);
 *  	if (slot >= pr->pr_base &&
 *  	    slot < (short *)(pr->pr_size + (int)pr->pr_base)) {
 *  		if ((counter=fusword(slot))<0) {
 *  			pr->pr_scale = 0;
 *  		} else {
 *  			counter += incr;
 *  			susword(slot, counter);
 *  		}
 *  	}
 *  }
 */
#if defined(lint)

#include <sys/lwp.h>	/* for definition of struct prof */

/* ARGSUSED */
void
addupc(void (*pc)(), struct prof *pr, int incr)
{}

#else	/* lint */

	.text
	.set	PR_BASE,0
	.set	PR_SIZE,4
	.set	PR_OFF,8
	.set	PR_SCALE,12

	ENTRY(addupc)
	pushl	%ebp
	movl	%esp,%ebp
	subl	$8,%esp
	movl	12(%ebp),%eax
	cmpl	$2,PR_SCALE(%eax)
	jne	.L15
	movl	(%eax),%edx
	movl	%edx,-4(%ebp)
	jmp	.L17
.L15:
	movl	8(%ebp),%edx
	subl	PR_OFF(%eax),%edx
	imull	PR_SCALE(%eax),%edx
	shrl	$17,%edx
	leal	(%edx,%edx),%edx
	movl	(%eax),%eax
	addl	%edx,%eax
	movl	%eax,-4(%ebp)
.L17:
	movl	12(%ebp),%eax
	movl	(%eax),%eax
	cmpl	%eax,-4(%ebp)
	jb	.L24
	movl	12(%ebp),%eax
	movl	(%eax),%ecx
	movl	4(%eax),%eax
	addl	%ecx,%eax
	cmpl	%eax,-4(%ebp)
	jae	.L24
	pushl	-4(%ebp)
	call	fusword
	popl	%ecx
	movw	%ax,-6(%ebp)
	movswl	-6(%ebp),%eax
	cmpl	$0,%eax
	jge	.L22
	movl	12(%ebp),%eax
	movl	$0,12(%eax)
.L24:
	leave	
	ret	
.L22:
	movw	16(%ebp),%ax
	addw	%ax,-6(%ebp)
	movswl	-6(%ebp),%eax
	pushl	%eax
	pushl	-4(%ebp)
	call	susword
	addl	$8,%esp
	jmp	.L24
	SET_SIZE(addupc)

#endif	/* lint */
