/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident  "@(#)hwmuldiv.s 1.2     96/02/09 SMI"

	.file   "hwmuldiv.s"

#include <sys/asm_linkage.h>
#include "synonyms.h"
#include <sys/trap.h>
#define bugid1178465


!
!	division, signed
!
		
	ENTRY(__div64)

	sllx 	%o0, 32, %o0		!build arguments
	sllx 	%o2, 32, %o2
	srl	%o1, 0, %o1		!dividend - %o1
	srl	%o3, 0, %o3		!divisor - %o2
	or	%o0, %o1, %o1
	or	%o2, %o3, %o2

! XXX - bug 1178465 - we currently do not generate a division_by_zero
!	exception in libc. We need to be bug to bug compatible :-(
!	We will need to fix this when bug 1178465 gets fixed
	
	brz,a,pn	%o2, 1f		! check division for zero
	clr	%o0

	sdivx	%o1, %o2, %o1
	
	retl
	srax	%o1, 32, %o0

1:
#ifndef bugid1178465
	ta	ST_DIV0
#endif
	retl
	clr	%o1
	SET_SIZE(__div64)

!
!	division, unsigned
!


	ENTRY(__udiv64)

	sllx 	%o0, 32, %o0		! Very similar to above routine
	sllx 	%o2, 32, %o2
	srl	%o1, 0, %o1
	srl	%o3, 0, %o3
	or	%o0, %o1, %o1
	or	%o2, %o3, %o2

	brz,a,pn	%o2, 1f
	clr	%o0

	udivx	%o1, %o2, %o1

	retl
	srax	%o1, 32, %o0

1:
#ifndef bugid1178465
	ta	ST_DIV0
#endif
	retl
	clr	%o1
	SET_SIZE(__udiv64)

!
!	multiplication, signed
!


	ENTRY(__mul64)
	ALTENTRY(__umul64)

	sllx 	%o0, 32, %o0
	sllx 	%o2, 32, %o2
	srl	%o1, 0, %o1
	srl	%o3, 0, %o3
	or	%o0, %o1, %o1
	or	%o2, %o3, %o2

	mulx	%o1, %o2, %o1

	retl
	srax	%o1, 32, %o0
	SET_SIZE(__mul64)
	SET_SIZE(__umul64)


!
!	unsigned remainder 
!
	ENTRY(__urem64)

	sllx 	%o0, 32, %o0
	sllx 	%o2, 32, %o2
	srl	%o1, 0, %o1
	srl	%o3, 0, %o3
	or	%o0, %o1, %o1
	or	%o2, %o3, %o2

	brz,a,pn	%o2, 1f
	clr	%o0

	udivx	%o1, %o2, %o3		!divide
	mulx	%o3, %o2, %o3		!multiply back
        sub     %o1, %o3, %o1		!get difference

	retl
	srax	%o1, 32, %o0

1:
#ifndef bugid1178465
	ta	ST_DIV0
#endif
	retl
	clr	%o1
	SET_SIZE(__urem64)



!
!	signed remainder - implement the same as in crt/divrem64.c
!
	ENTRY(__rem64)

	sllx 	%o0, 32, %o0
	sllx 	%o2, 32, %o2
	srl	%o1, 0, %o1
	srl	%o3, 0, %o3
	or	%o2, %o3, %o2
	or	%o0, %o1, %o1

	brgez,a,pt	%o1, 1f		! if (dividend < 0)
	clr	%o0
	neg	%o1			! dividend = -divdend
	mov	1, %o0			! sign = 1
	
1:
	

	brz,a,pn	%o2, 4f			! we will get an exception
	clr	%o0				! if divided by zero

	brlz,a,pt	%o2, 2f			! divisor = -divisor
	neg	%o2

2:
	udivx	%o1, %o2, %o3			! since we used
	mulx	%o3, %o2, %o3			! abs value, we use unsigned
	sub	%o1, %o3, %o1			! udivx here

	brnz,a,pn	%o0, 3f			! return (sign?-R:R);
	neg	%o1

3:
	retl
	srlx	%o1, 32, %o0

4:
#ifndef bugid1178465
	ta	ST_DIV0
#endif
	retl
	clr	%o1
	SET_SIZE(__rem64)


		

