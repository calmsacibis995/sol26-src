/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */
#pragma ident	"@(#)ip_ocsum.s	1.6	95/05/08 SMI"

#include <sys/asm_linkage.h>

/*
 *   Checksum routine for Internet Protocol Headers
 *
 *   unsigned int
 *   ip_ocsum (address, count, sum)
 *      u_short   *address;		! Ptr to 1st message buffer
 *      int        halfword_count;	! Length of data
 *	unsigned int    sum;		! partial checksum
 *
 *   Do a 16 bit one's complement sum of a given number of (16-bit)
 *   halfwords. The halfword pointer must not be odd.
 *
 *   This is based on the equivalent code for SPARC.
 *
 *   Register usage:
 *	%r3 address; %r4 count; %r5 sum accumulator; %r6, %r7, %r8, and
 * 	%r9 are temps
 */
#if defined(lint)
/*ARGSUSED*/
unsigned int
ip_ocsum(u_short *address, int halfword_count, unsigned int sum)
{ return (0); }
#else

	.text
	ENTRY(ip_ocsum)
	cmpi	%r4,31		! less than 62 bytes?
	ble	.dohw		!   just do halfwords
	andi.	%r6,%r3,31	! if 32 byte aligned, skip
	beq-	.L2		!

	!
	! Do first halfwords until 32-byte aligned
	!
.L1:
	lhz	%r8,0(%r3)	! read data
	addi	%r3,%r3,2	! increment address
	subi	%r4,%r4,1	! decrement count
	andi.	%r6,%r3,31	! if not 32 byte aligned, go back
	add	%r5,%r8,%r5	! add to accumulator, don't need carry yet
	bne+	.L1
	!
	! loop to add in 32 byte chunks
	! The loads and adds are staggered to help avoid load/use
	! interlocks on highly pipelined implementations, and double
	! loads are used for 64-bit wide memory systems.
	!
.L2:
	subi	%r4,%r4,16	! decrement count to aid testing
.L4:
	lwz	%r6,0(%r3)	! read data
	lwz	%r7,4(%r3)	! read data
	addc	%r5,%r6,%r5	! add to accumulator
	lwz	%r8,8(%r3)	! read more data
	adde	%r5,%r7,%r5	! add to accumulator with carry
	lwz	%r9,12(%r3)	! read more data
	adde	%r5,%r8,%r5	! add to accumulator with carry
	lwz	%r6,16(%r3)	! read data
	adde	%r5,%r9,%r5	! add to accumulator with carry
	lwz	%r7,20(%r3)	! read data
	adde	%r5,%r6,%r5	! add to accumulator with carry
	lwz	%r8,24(%r3)	! read more data
	adde	%r5,%r7,%r5	! add to accumulator with carry
	lwz	%r9,28(%r3)	! read more data
	adde	%r5,%r8,%r5	! add to accumulator
	adde	%r5,%r9,%r5	! add to accumulator with carry
	addze	%r5,%r5		! if final carry, add it in
	addic.	%r4,%r4,-16	! decrement count (in halfwords)
	addi	%r3,%r3,32	! increment address
	bge+	.L4

	addi	%r4,%r4,16	! add back in
	!
	! Do any remaining halfwords
	!
	b	.dohw

.L3:
	lhz	%r8,0(%r3)	! read data
	addi	%r3,%r3,2	! increment address
	addc	%r5,%r8,%r5	! add to accumulator
	subi	%r4,%r4,1	! decrement count
	addze	%r5,%r5		! if carry, add it in
.dohw:
	cmpi	%r4,0		! more to do?
	bgt+	.L3

	!
	! at this point the 32-bit accumulator
	! has the result that needs to be returned in 16-bits
	!
	slwi	%r6,%r5,16	! put low halfword in high halfword %r6
	addc	%r5,%r5,%r6	! add the 2 halfwords in high %r5, set carry
	srwi	%r5,%r5,16	! shift to low halfword
	addze	%r3,%r5		! add in carry if any. result in %r3
	blr
	SET_SIZE(ip_ocsum)
#endif

#if defined(lint)
/*ARGSUSED*/
unsigned int
ip_ocsum_copy(u_short *address, int halfword_count,
    unsigned int sum, u_short *dest)
{ return (0); }
#else
		! for now this is just a dummy entry point
		! if it is ever used, it will need to do
		! the equivalent of ip_ocsum followed by a bcopy
	.text
	.align	4
	ENTRY(ip_ocsum_copy)
	blr
	SET_SIZE(ip_ocsum_copy)
#endif

#if defined(lint)
/*ARGSUSED*/
int
ip_ocsum_copyin(caddr_t uaddr, u_int len, u_int *sump, caddr_t kaddr)
{ return (0); }
#else
		! For now, we don't implement it.
	.text
	.align	4
	ENTRY(ip_ocsum_copyin)
	li	%r3,48
	blr
	SET_SIZE(ip_ocsum_copyin)
#endif

#if defined(lint)
/*ARGSUSED*/
int
ip_ocsum_copyout(caddr_t kaddr, u_int len, u_int *sump, caddr_t uaddr)
{ return (0); }
#else
		! For now, we don't implement it.
	.text
	.align	4
	ENTRY(ip_ocsum_copyout)
	li	%r3,48
	blr
	SET_SIZE(ip_ocsum_copyout)
#endif
