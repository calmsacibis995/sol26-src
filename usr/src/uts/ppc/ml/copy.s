/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
#pragma ident	"@(#)copy.s	1.20	96/06/18 SMI"

#include <sys/errno.h>
#include <sys/asm_linkage.h>
#include <sys/machthread.h>

#if defined(lint)

#else	/* lint */
#include "assym.s"
#endif	/* lint */

/*
 * Interposition for watchpoint support.
 */
#define WATCH_ENTRY(name)			\
	ENTRY(name);				\
	lhz	%r0, T_PROC_FLAG(THREAD_REG);	\
	andi.	%r0, %r0, TP_WATCHPT;		\
	bz+	_/**/name;			\
	b	watch_/**/name;			\
	.globl	_/**/name;			\
_/**/name:

#define WATCH_ENTRY2(name1,name2)		\
	ENTRY2(name1,name2);			\
	lhz	%r0, T_PROC_FLAG(THREAD_REG);	\
	andi.	%r0, %r0, TP_WATCHPT;		\
	bz+	_/**/name1;			\
	b	watch_/**/name1;		\
	.globl	_/**/name1;			\
	.globl	_/**/name2;			\
_/**/name1:	;				\
_/**/name2:

/*
 * Copy a block of storage, returning an error code if `from' or
 * `to' takes a kernel pagefault which cannot be resolved.
 * Returns errno value on pagefault error, 0 if all ok
 */

#if defined(lint)

/* ARGSUSED */
int
kcopy(const void *from, void *to, size_t count)
{ return(0); }

#else	/* lint */

	.text
	.align	2

	ENTRY(kcopy)
	lis	%r11, .bcpfault@ha
	la	%r0, .bcpfault@l(%r11)	! .bcpfault is the lofault value
	b	.do_copy		! common code
	SET_SIZE(kcopy)

#endif	/* lint */

/*
 * Copy a block of storage - must not overlap (from + len <= to).
 *
 * There are several cases when addresses are not word aligned. If the
 * source and destination addresses are not aligned on the same byte boundary
 * (within a word) then copying words require packing the data which is endian
 * dependent. For this case we use half word copy if both are half word
 * aligned, otherwise we use byte copy. For all other cases we optimize it by
 * doing word copy as much as possible. The cases include:
 *
 *	1. If the count is less than SMALL_BLK_SIZE then just do byte copy.
 *	2. If the From and To are aligned on the same byte boundary then:
 *		- Copy byte(s) if any using byte copy until they are word
 *		  aligned.
 *		- Copy words using word copy loop and then copy remaining
 *		  byte(s) if any using byte copy.
 *	3. Else if From and To are both half word aligned then:
 *		- Copy half words in half word loop and then copy remaining
 *		  byte(s) if any using byte copy.
 *	4. Else do byte copy in byte copy loop.
 *
 * WARNING: To take the advantage of cache-fill-zero instruction we assume 
 *	    that the destination page involved in this copy operation is
 *	    cachable page. If not, it would cause an alignement exception
 *	    on PPC.
 *
 */

#if defined(lint)

/* ARGSUSED */
void
bcopy(const void *from, void *to, size_t count)
{}

#else	/* lint */

	!
	! If the block size is less than SMALL_BLK_SIZE it is optimal
	! to just do as byte copy loop.
	!
	.set	SMALL_BLK_SIZE, 8

	ENTRY(bcopy)
	lwz	%r0, T_LOFAULT(THREAD_REG)	! current lofault address
.do_copy:
	cmpi	%r5, 0
	lwz	%r10, T_LOFAULT(THREAD_REG)	! save the old lofault in r10
	stw	%r0, T_LOFAULT(THREAD_REG)	! and install the new one
	beq-	.bcpdone		! count is 0

	cmpwi	%r5, SMALL_BLK_SIZE	! for small blocks use byte copy.
	blt-	.bcptail		! use byte copy
	!
	! Check if both From and To are on the same byte alignment. If the
	! alignment is not the same then check if they are half word
	! aligned and if so do half word copy, otherwise do byte copy.
	!
	xor	%r7, %r3, %r4		! xor from and to
	andi.	%r0, %r7, NBPW-1	! check if the alignment is same
	bnz-	.bcp_ck2align		! alignment is different
	!
	! copy bytes until source address is word aligned 
	!
.bcp_loop1:
	andi.	%r0, %r3, NBPW-1	! is source address word aligned?
	beq+	.bcpalign		! do copy from word-aligned source
	lbz	%r7, 0(%r3)
	stb	%r7, 0(%r4)
	subi	%r5, %r5, 1
	addi	%r4, %r4, 1
	addi	%r3, %r3, 1
	b	.bcp_loop1

	!
	! Word copy loop.
	!
	! We can use cache-fill-zero whenever possible to optimize the
	! memory accesses in the loop. This could cause alignment exception
	! if the destination page is not a cacheable page.
	!
	! %r3 = word aligned source address
	! %r4 = destination address
	! %r5 = count of bytes to copy
.bcpalign:
#ifdef XXX_WORKS	/* Enable this if this code works! */
	srwi	%r6, %r5, 2			! %r6 = word count for copy
	lis	%r11, dcache_blocksize@ha
	lwz	%r7, dcache_blocksize@l(%r11)	! %r7 = line size in bytes
	lis	%r11, cache_blockmask@ha
	lwz	%r8, cache_blockmask@l(%r11)	! %r8 = line mask
	srwi	%r9, %r7, 2			! %r9 = line size in words
.bcpalgn_loop:
	cmpw	%r6, %r9
	blt-	.bcpalgn3		! count < cache block size
	and.	%r0, %r4, %r8		! next write at a cache boundary?
	bne-	.bcpalgn2		! no
	! next write is at a cache boundary, flush the cache block first
	dcbz	%r0, %r4		! clear the cache block
	mtctr	%r9			! loop for cache block size
	subf	%r6, %r9, %r6		! count -= cache block size
	subi	%r3, %r3, 4
	subi	%r4, %r4, 4
.bcpalgn1_loop:
	lwzu	%r0, 4(%r3)
	stwu	%r0, 4(%r4)
	bdnz	.bcpalgn1_loop
	addi	%r3, %r3, 4			! src_addr += 4
	addi	%r4, %r4, 4			! dst_addr += 4
	b	.bcpalgn_loop

	! next write is not at a cache boundary, do the partial copy first
.bcpalgn2:
	subf	%r0, %r0, %r7
	srwi	%r0, %r0, 2
	subi	%r3, %r3, 4
	subi	%r4, %r4, 4
	subf	%r6, %r0, %r6		! update the count
	mtctr	%r0			! loop until the end of the cache block
.bcpalgn2_loop:
	lwzu	%r0, 4(%r3)
	stwu	%r0, 4(%r4)
	bdnz	.bcpalgn2_loop
	addi	%r3, %r3, 4			! src_addr += 4
	addi	%r4, %r4, 4			! dst_addr += 4
	b	.bcpalgn_loop

	! count is less than the cache block size.
.bcpalgn3:
	subi	%r3, %r3, 4
	subi	%r4, %r4, 4
	mtctr	%r6
.bcpalgn3_loop:
	lwzu	%r0, 4(%r3)
	stwu	%r0, 4(%r4)
	bdnz	.bcpalgn3_loop
	addi	%r3, %r3, 4			! src_addr += 4
	addi	%r4, %r4, 4			! dst_addr += 4
#else
	srwi	%r6, %r5, 2		! convert to count of words to copy
	mtctr	%r6
	subi	%r3, %r3, 4
	subi	%r4, %r4, 4
.bcp_wloop:
	lwzu	%r0, 4(%r3)
	stwu	%r0, 4(%r4)
	bdnz	.bcp_wloop
	addi	%r3, %r3, 4			! src_addr += 4
	addi	%r4, %r4, 4			! dst_addr += 4
#endif
	andi.	%r5, %r5, NBPW-1	! last word is partial word?
	beq-	.bcpdone
	!
	! Byte copy loop.
	!
	! %r3 = source address
	! %r4 = dest address
	! %r5 = byte count to copy
.bcptail:
	subi	%r3, %r3, 1
	subi	%r4, %r4, 1
	mtctr	%r5
.bcp_loop4:
	lbzu	%r7, 1(%r3)
	stbu	%r7, 1(%r4)
	bdnz	.bcp_loop4
.bcpdone:
	li	%r3, 0			! return zero
.bcpfault:
	! restore the original lofault; errno is in the %r3
	stw	%r10, T_LOFAULT(THREAD_REG)	! original lofault
	blr

	!
	! If both From and To are aligned on a half word boundary then
	! use half word copy loop. Otherwise, use byte copy loop.
	!
.bcp_ck2align:
	andi.	%r0, %r3, 1		! if lower one bit is zero
	bnz-	.bcptail		! not half word alignment
	andi.	%r0, %r4, 1		! is dest half word aligned?
	bnz-	.bcptail		! nope, do it the hard way.
	! do half word copy loop
	subi	%r3, %r3, 2
	subi	%r4, %r4, 2
	srwi	%r6, %r5, 1
	mtctr	%r6
.bcp_loop3:
	lhzu	%r7, 2(%r3)
	sthu	%r7, 2(%r4)
	bdnz	.bcp_loop3

	andi.	%r5, %r5, 1		! byte(s) left in the last half word?
	beq-	.bcpdone		! no
	lbz	%r7, 2(%r3)
	stb	%r7, 2(%r4)
	b	.bcpdone
	SET_SIZE(bcopy)

#endif	/* lint */

/*
 * Zero a block of storage, returning an error code if we
 * take a kernel pagefault which cannot be resolved.
 * Returns errno value on pagefault error, 0 if all ok
 */

#if defined(lint)

/* ARGSUSED */
int
kzero(void *addr, size_t count)
{ return(0); }

#else	/* lint */

	ENTRY(kzero)
	lis	%r11, .bzfault@ha
	la	%r0, .bzfault@l(%r11)	! .bzfault is the lofault value
	b	.do_zero		! common code
	SET_SIZE(kzero)

#endif	/* lint */

/*
 * Zero a block of storage. (also known as blkclr)
 *
 * WARNING: To take the advantage of cache-fill-zero instruction we assume 
 *	    that the destination page involved in this copy operation is
 *	    cachable page. If not, it would cause an alignement exception
 *	    on PPC.
 */

#if defined(lint)

/* ARGSUSED */
void
bzero(void *addr, size_t count)
{}

/* ARGSUSED */
void
blkclr(void *addr, size_t count)
{}

#else	/* lint */

	ENTRY2(bzero,blkclr)
	lwz	%r0, T_LOFAULT(THREAD_REG)	! current lofault address
.do_zero:
	cmpi	%r4, 0
	lwz	%r10, T_LOFAULT(THREAD_REG)	! save the old lofault in r10
	stw	%r0, T_LOFAULT(THREAD_REG)	! and install the new one
	beq-	.bzdone			! count is zero
	li	%r0, 0
	!
	! if number of bytes to clear is less than in a word
	! jump to tail clear
	!
	cmpwi	%r4, NBPW
	blt-	.bztail
	andi.	%r6, %r3, NBPW-1	! is source address word aligned?
	beq+	.bzalign		! do copy from word-aligned source

	! zero the bytes until the address is word aligned
.bz_loop1:
	stb	%r0, 0(%r3)
	subi	%r4, %r4, 1
	addi	%r3, %r3, 1
	andi.	%r6, %r3, NBPW-1
	bne	.bz_loop1

	! %r3= word aligned source address
	! %r4 = byte count
.bzalign:
#ifdef XXX_WORKS
	srwi	%r5, %r4, 2		! convert to count of words to clear
	andi.	%r4, %r4, NBPW-1	! %r4 gets remainder byte count
	li	%r6, 0
	subi	%r3, %r3, 4
	lis	%r11, dcache_blocksize@ha
	lwz	%r8, dcache_blocksize@l(%r11)	! %r8 = line size in bytes
	lis	%r11, cache_blockmask@ha
	lwz	%r9, cache_blockmask@l(%r11)	! %r9 = line mask
	mtctr	%r5
	li	%r6, 4
.bz_loop2:
	and.	%r7, %r3, %r8
	bne-	.L3			! not a cache block boundary
	mfctr	%r7
.L2:
	cmpw	%r7, %r9
	ble-	.L3			! not going to write a full cache block
	dcbz	%r3, %r6 		! clear the cache block
	add	%r7, %r6, %r8
	mtctr	%r7			! reset the count
	b	.L2
.L3:
	stwux	%r0, %r3, %r6
	bdnz	.bz_loop2
#else
	srwi	%r5, %r4, 2		! convert to count of words to clear
	andi.	%r4, %r4, NBPW-1	! %r4 gets remainder byte count
	li	%r6, 0			! length copied = 0
	mtctr	%r5
.bz_loop2:
	stwx	%r0, %r3, %r6
	addi	%r6, %r6, 4			! length_copied += 4
	bdnz	.bz_loop2
	add	%r3, %r3, %r6		! addr += length_copied
#endif /* XXX_WORKS */
	cmpwi	%r4, 0			! remainder byte count == 0 ?
	beq-	.bzdone			! done
.bztail:
	subi	%r3, %r3, 1
	mtctr	%r4
.bz_loop3:
	stbu	%r0, 1(%r3)
	bdnz	.bz_loop3
.bzdone:
	li	%r3, 0
.bzfault:
	stw	%r10, T_LOFAULT(THREAD_REG)	! restore the original lofault
	blr
	SET_SIZE(bzero)
	SET_SIZE(blkclr)

#endif	/* lint */

/*
 * Transfer data to and from user space -
 * Note that these routines can cause faults
 * It is assumed that the kernel has nothing at
 * less than KERNELBASE in the virtual address space.
 *
 * Note that copyin(9F) and copyout(9F) are part of the
 * DDI/DKI which specifies that they return '-1' on "errors."
 *
 * Sigh.
 *
 * So there's two extremely similar routines - xcopyin() and xcopyout()
 * which return the errno that we've faithfully computed.  This
 * allows other callers (e.g. uiomove(9F)) to work correctly.
 * Given that these are used pretty heavily, we expand the calling
 * sequences inline for all flavours (rather than making wrappers).
 */

/*
 * Copy user data to kernel space.
 *
 * int
 * copyin(const void *uaddr, void *kaddr, size_t count)
 *
 * int
 * xcopyin(const void *uaddr, void *kaddr, size_t count)
 */

#if defined(lint)

/* ARGSUSED */
int
copyin(const void *uaddr, void *kaddr, size_t count)
{ return(0); }

#else	/* lint */

	WATCH_ENTRY(copyin)
	lis	%r0, KERNELBASE >> 16
	!ori	%r0, %r0, KERNELBASE & 0xFFFF	! %r0 = KERNELBASE
	cmplw	%r3, %r0		! test uaddr < KERNELBASE
	bge-	.copyin_err
	lis	%r11, copyioerr@ha
	la	%r0, copyioerr@l(%r11)	! copyioerr is the lofault value
	b	.do_copy
.copyin_err:
	li	%r3, -1			! return failure
	blr

/*
 * We got here because of a fault during copy{in,out}.
 * Errno value is in %r3, but DDI/DKI says return -1 (sigh).
 */
copyioerr:
	li	%r3, -1			! return failure
	b	.bcpfault
	SET_SIZE(copyin)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
int
xcopyin(const void *uaddr, void *kaddr, size_t count)
{ return(0); }

#else	/* lint */

	WATCH_ENTRY(xcopyin)
	lis	%r0, KERNELBASE >> 16
	!ori	%r0, %r0, KERNELBASE & 0xFFFF	! %r0 = KERNELBASE
	cmplw	%r3, %r0		! test uaddr < KERNELBASE
	bge-	.xcopyin_err
	lis	%r11, .bcpfault@ha
	la	%r0, .bcpfault@l(%r11)	! .bcpfault is the lofault value
	b	.do_copy

.xcopyin_err:
	li	%r3, EFAULT		! return failure
	blr
	SET_SIZE(xcopyin)

#endif	/* lint */

/*
 * Copy kernel data to user space.
 *
 * int
 * copyout(const void *kaddr, void *uaddr, size_t count)
 *
 * int
 * xcopyout(const void *kaddr, void *uaddr, size_t count)
 */

#if defined(lint)

/* ARGSUSED */
int
copyout(const void *kaddr, void *uaddr, size_t count)
{ return(0); }

#else	/* lint */

	WATCH_ENTRY(copyout)
	lis	%r0, KERNELBASE >> 16
	!ori	%r0, %r0, KERNELBASE & 0xFFFF	! %r0 = KERNELBASE
	cmplw	%r4, %r0		! test uaddr < KERNELBASE
	bge-	.copyout_err
	lis	%r11, copyioerr@ha
	la	%r0, copyioerr@l(%r11)	! copyioerr is the lofault value
	b	.do_copy

.copyout_err:
	li	%r3, -1			! return failure
	blr
	SET_SIZE(copyout)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
int
xcopyout(const void *kaddr, void *uaddr, size_t count)
{ return(0); }

#else	/* lint */

	WATCH_ENTRY(xcopyout)
	lis	%r0, KERNELBASE >> 16
	!ori	%r0, %r0, KERNELBASE & 0xFFFF	! %r0 = KERNELBASE
	cmplw	%r4, %r0		! test uaddr < KERNELBASE
	bge-	.xcopyout_err
	lis	%r11, .bcpfault@ha
	la	%r0, .bcpfault@l(%r11)	! .bcpfault is the lofault value
	b	.do_copy

.xcopyout_err:
	li	%r3, EFAULT		! return failure
	blr
	SET_SIZE(xcopyout)

#endif	/* lint */

/*
 * Copy a null terminated string from one point to another in
 * the kernel address space.
 *
 * copystr(from, to, maxlength, lencopied)
 *	caddr_t from, to;
 *	u_int maxlength, *lencopied;
 */

#if defined(lint)

/* ARGSUSED */
int
copystr(char *from, char *to, size_t maxlength, size_t *lencopied)
{ return(0); }

#else	/* lint */

	ENTRY(copystr)
	lwz	%r0, T_LOFAULT(THREAD_REG)	! current lofault address
.do_copystr:
	cmpwi	%r5, 0
	mtctr	%r5
	lwz	%r10, T_LOFAULT(THREAD_REG)	! save the old lofault in r10
	stw	%r0, T_LOFAULT(THREAD_REG)	! and install the new one
	beq-	.copystr_err1		! maxlength == 0
	bgt+	.copystr_doit		! maxlength > 0
	li	%r3, EFAULT		! return EFAULT
.copystr_done:
	stw	%r10, T_LOFAULT(THREAD_REG)	! restore the original lofault
	blr				! return
.copystr_doit:
	! %r3 = source address
	! %r4 = destination address
	! %r5 = maxlength
	subi	%r3, %r3, 1
	subi	%r4, %r4, 1
.copystr_loop:
	lbzu	%r0, 1(%r3)
	stbu	%r0, 1(%r4)
	cmpwi	%r0, 0
	beq	.copystr_null		! null char
	bdnz	.copystr_loop

.copystr_err1:
	li	%r3, ENAMETOOLONG	! ret code = ENAMETOOLONG
	b	.copystr_out
.copystr_null:
	li	%r3, 0
	addi	%r5, %r5,1		! to account for null char
.copystr_out:
	cmpwi	%r6, 0			! want length?
	beq+	.copystr_done
	mfctr	%r0
	subf	%r5, %r0, %r5		! compute length and store it
	stw	%r5, 0(%r6)
	b	.copystr_done
	SET_SIZE(copystr)

#endif	/* lint */

/*
 * Copy a null terminated string from the user address space into
 * the kernel address space.
 *
 * copyinstr(uaddr, kaddr, maxlength, lencopied)
 *	caddr_t uaddr, kaddr;
 *	size_t maxlength, *lencopied;
 */

#if defined(lint)

/* ARGSUSED */
int
copyinstr(char *uaddr, char *kaddr, size_t maxlength, size_t *lencopied)
{ return(0); }

#else	/* lint */

	WATCH_ENTRY(copyinstr)
	lis	%r0, KERNELBASE >> 16
	!ori	%r0, %r0, KERNELBASE & 0xFFFF	! %r0 = KERNELBASE
	cmplw	%r3, %r0		! test uaddr < KERNELBASE
	bge-	.copyinstr_err
	lis	%r11, copystrerr@ha
	la	%r0, copystrerr@l(%r11)	! copystrerr is the lofault value
	b	.do_copystr

.copyinstr_err:
	li	%r3, EFAULT		! return EFAULT
	blr
	SET_SIZE(copyinstr)

#endif	/* lint */

/*
 * Copy a null terminated string from the kernel
 * address space to the user address space.
 *
 * copyoutstr(kaddr, uaddr, maxlength, lencopied)
 *	caddr_t kaddr, uaddr;
 *	size_t maxlength, *lencopied;
 */

#if defined(lint)

/* ARGSUSED */
int
copyoutstr(char *kaddr, char *uaddr, size_t maxlength, size_t *lencopied)
{ return(0); }

#else	/* lint */

	WATCH_ENTRY(copyoutstr)
	lis	%r0, KERNELBASE >> 16
	!ori	%r0, %r0, KERNELBASE & 0xFFFF	! %r0 = KERNELBASE
	cmplw	%r4, %r0		! test uaddr < KERNELBASE
	bge-	.copyoutstr_err
	lis	%r11, copystrerr@ha
	la	%r0, copystrerr@l(%r11)	! copystrerr is the lofault value
	b	.do_copystr

.copyoutstr_err:
	li	%r3, EFAULT		! return EFAULT
	blr
/*
 * Fault while trying to move from or to user space.
 * Set and return error code.
 */
copystrerr:
	li	%r3, EFAULT		! return EFAULT
	b	.copystr_done
	SET_SIZE(copyoutstr)

#endif	/* lint */

/*
 * Fetch user (long) word.
 *
 * int
 * fuword(addr)
 *	int *addr;
 */

#if defined(lint)

/* ARGSUSED */
int
fuword(int *addr)
{ return(0); }

/* ARGSUSED */
int
fuiword(int *addr)
{ return(0); }

#else	/* lint */

	WATCH_ENTRY2(fuword,fuiword)
	lis	%r0, KERNELBASE >> 16
	!ori	%r0, %r0, KERNELBASE & 0xFFFF	! %r0 = KERNELBASE
	cmplw	%r3, %r0		! test uaddr < KERNELBASE
	bge-	fsuerr			! if (KERNELBASE >= addr) error
	! set t_lofault to catch any fault
	lis	%r11, fsuerr@ha
	la	%r0, fsuerr@l(%r11)	! fsuerr is the lofault value
	stw	%r0, T_LOFAULT(THREAD_REG)	! fsuerr is lofault value
	lwz	%r3, 0(%r3)		! get the word
	li	%r0, 0
	stw	%r0, T_LOFAULT(THREAD_REG)	! clear t_lofault
	blr
	SET_SIZE(fuword)
	SET_SIZE(fuiword)

#endif	/* lint */

/*
 * Fetch user byte.
 *
 * int
 * fubyte(addr)
 *	caddr_t addr;
 */

#if defined(lint)

/* ARGSUSED */
int
fubyte(caddr_t addr)
{ return(0); }

/* ARGSUSED */
int
fuibyte(caddr_t addr)
{ return(0); }

#else	/* lint */

	WATCH_ENTRY2(fubyte,fuibyte)
	lis	%r0, KERNELBASE >> 16
	!ori	%r0, %r0, KERNELBASE & 0xFFFF	! %r0 = KERNELBASE
	cmplw	%r3, %r0		! test uaddr < KERNELBASE
	bge-	fsuerr			! if (KERNELBASE >= addr) error
	! set t_lofault to catch any fault
	lis	%r11, fsuerr@ha
	la	%r0, fsuerr@l(%r11)	! fsuerr is the lofault value
	stw	%r0, T_LOFAULT(THREAD_REG)	! fsuerr is lofault value
	lbz	%r3, 0(%r3)		! get the byte
	li	%r0, 0
	stw	%r0, T_LOFAULT(THREAD_REG)	! clear t_lofault
	blr
	SET_SIZE(fubyte)
	SET_SIZE(fuibyte)

#endif	/* lint */

/*
 * Set user (long) word.
 *
 * int
 * suword(addr, value)
 *	int *addr;
 *	int value;
 */

#if defined(lint)

/* ARGSUSED */
int
suword(int *addr, int value)
{ return(0); }

/* ARGSUSED */
int
suiword(int *addr, int value)
{ return(0); }

#else	/* lint */

	WATCH_ENTRY2(suword,suiword)
	lis	%r0, KERNELBASE >> 16
	!ori	%r0, %r0, KERNELBASE & 0xFFFF	! %r0 = KERNELBASE
	cmplw	%r3, %r0		! test uaddr < KERNELBASE
	bge-	fsuerr			! if (KERNELBASE >= addr) error
	! set t_lofault to catch any fault
	lis	%r11, fsuerr@ha
	la	%r0, fsuerr@l(%r11)	! fsuerr is the lofault value
	stw	%r0, T_LOFAULT(THREAD_REG)	! fsuerr is lofault value
	stw	%r4, 0(%r3)		! store the word
	li	%r3, 0			! indicate success
	stw	%r3, T_LOFAULT(THREAD_REG)	! clear t_lofault
	blr
	SET_SIZE(suword)
	SET_SIZE(suiword)

#endif	/* lint */

/*
 * Set user byte.
 *
 * int
 * subyte(addr, value)
 *	caddr_t addr;
 *	char value;
 */

#if defined(lint)

/* ARGSUSED */
int
subyte(caddr_t addr, char value)
{ return(0); }

/* ARGSUSED */
int
suibyte(caddr_t addr, char value)
{ return(0); }

#else	/* lint */

	WATCH_ENTRY2(subyte,suibyte)
	lis	%r0, KERNELBASE >> 16
	!ori	%r0, %r0, KERNELBASE & 0xFFFF	! %r0 = KERNELBASE
	cmplw	%r3, %r0		! test uaddr < KERNELBASE
	bge-	fsuerr			! if (KERNELBASE >= addr) error
	! set t_lofault to catch any fault
	lis	%r11, fsuerr@ha
	la	%r0, fsuerr@l(%r11)	! fsuerr is the lofault value
	stw	%r0, T_LOFAULT(THREAD_REG)	! fsuerr is lofault value
	stb	%r4, 0(%r3)		! store the byte
	li	%r3, 0			! indicate success
	stw	%r3, T_LOFAULT(THREAD_REG)	! clear t_lofault
	blr
	SET_SIZE(subyte)
	SET_SIZE(suibyte)

#endif	/* lint */

/*
 * Fetch user short (half) word.
 *
 * int
 * fusword(addr)
 *	caddr_t addr;
 */

#if defined(lint)

/* ARGSUSED */
int
fusword(caddr_t addr)
{ return(0); }

#else	/* lint */

	WATCH_ENTRY(fusword)
	lis	%r0, KERNELBASE >> 16
	!ori	%r0, %r0, KERNELBASE & 0xFFFF	! %r0 = KERNELBASE
	cmplw	%r3, %r0		! test uaddr < KERNELBASE
	bge-	fsuerr			! if (KERNELBASE >= addr) error
	! set t_lofault to catch any fault
	lis	%r11, fsuerr@ha
	la	%r0, fsuerr@l(%r11)	! fsuerr is the lofault value
	stw	%r0, T_LOFAULT(THREAD_REG)	! fsuerr is lofault value
	lhz	%r3, 0(%r3)		! get the half word
	li	%r0, 0
	stw	%r0, T_LOFAULT(THREAD_REG)	! clear t_lofault
	blr
	SET_SIZE(fusword)

#endif	/* lint */

/*
 * Set user short word.
 *
 * int
 * susword(addr, value)
 *	caddr_t addr;
 *	int value;
 */

#if defined(lint)

/* ARGSUSED */
int
susword(caddr_t addr, int value)
{ return(0); }

#else	/* lint */

	WATCH_ENTRY(susword)
	lis	%r0, KERNELBASE >> 16
	!ori	%r0, %r0, KERNELBASE & 0xFFFF	! %r0 = KERNELBASE
	cmplw	%r3, %r0		! test uaddr < KERNELBASE
	bge-	fsuerr			! if (KERNELBASE >= addr) error
	! set t_lofault to catch any fault
	lis	%r11, fsuerr@ha
	la	%r0, fsuerr@l(%r11)	! fsuerr is the lofault value
	stw	%r0, T_LOFAULT(THREAD_REG)	! fsuerr is lofault value
	sth	%r4, 0(%r3)		! store the half word
	li	%r3, 0			! indicate success
	stw	%r3, T_LOFAULT(THREAD_REG)	! clear t_lofault
	blr
fsuerr:
	li	%r3, -1			! return error
	blr
	SET_SIZE(susword)

#endif	/* lint */

/*
 * Copy a block of words -- must not overlap
 * (It is assumed that the 'from' and 'to' addresses are word aligned)
 *
 * wcopy(from, to, count)
 *	int *from;
 *	int *to;
 *	size_t count;
 */

#if defined(lint)

/* ARGSUSED */
void
wcopy(int *from, int *to, size_t count)
{}

#else	/* lint */

	ENTRY(wcopy)
	cmpi	%r5, 0			! count == 0?
	bz	.wcopy_done
	! copy the words:
	! 	%r3 = source address
	! 	%r4 = destination address
	! 	%r5 = count in words
	subi	%r3, %r3, 4
	subi	%r4, %r4, 4
	mtctr	%r5
.wcopy_loop:
	lwzu	%r0, 4(%r3)
	stwu	%r0, 4(%r4)
	bdnz	.wcopy_loop
.wcopy_done:
	blr
	SET_SIZE(wcopy)

#endif	/* lint */

/*
 * Overlapping bcopy (source and target may overlap arbitrarily).
 */
#if defined(lint)

/* ARGSUSED */
void
ovbcopy(const void *from, void *to, size_t count)
{}

#else	/* lint */

	ENTRY(ovbcopy)
	cmpwi	%r5, 0			! check count
	bgt	.ov1			! positive count
	blr				! nothing to do or bad arguments
					! return
.ov1:
	subf.	%r6, %r3, %r4		! difference of 'from' and 'to' address
	bgt+	.ov2
	neg	%r6, %r6		! if < 0, make it positive
.ov2:
	cmpw	%r5, %r6		! if size <= abs(diff): use bcopy
	ble-	bcopy
	cmplw	%r3, %r4		! if from < to, copy backwards
	blt-	.ov_bkwd

	!
	! copy forwards.
	!
	subi	%r3, %r3, 1
	subi	%r4, %r4, 1
	mtctr	%r5
.ov_fwd_loop:
	lbzu	%r6, 1(%r3)		! read from address
	stbu	%r6, 1(%r4)		! write to address
	bdnz	.ov_fwd_loop
	blr				! return
	
.ov_bkwd:
	!
	! copy backwards.
	!
	add	%r3, %r3, %r5
	add	%r4, %r4, %r5
	mtctr	%r5
.ov_bkwd_loop:
	lbzu	%r6, -1(%r3)		! read from address
	stbu	%r6, -1(%r4)		! write to address
	bdnz	.ov_bkwd_loop
	blr				! return
	SET_SIZE(ovbcopy)

#endif	/* lint */

/*
 * strlen(str), ustrlen(str)
 *	ustrlen gets strlen from an user space address
 *	On x86 it is almost the same. We added additional checks
 *	with the arguments.
 *
 * Returns the number of
 * non-NULL bytes in string argument.
 *
 */

#if defined(lint)

/* ARGSUSED */
size_t
strlen(const char *str)
{ return (0); }

/* ARGSUSED */
size_t
ustrlen(const char *str)
{ return (0); }

#else	/* lint */

	ENTRY(ustrlen)
	lis	%r4, USERLIMIT >> 16
	ori	%r4, %r4, USERLIMIT & 0xffff
	cmpl	%r3, %r4		! check for legal user address
	blt+	strlen			! address is OK
	lwz	%r5, 0(%r4)		! dereference USERLIMIT to force fault
	.word	0			! illegal instr if USERLIMIT is valid
	SET_SIZE(ustrlen)

	ENTRY(strlen)
	mr	%r4, %r3
	li	%r3, 0			! length of non zero bytes
	andi.	%r0, %r4, 3		! is src word aligned
	bz-	.algnd
.algn_loop0:
	lbzu	%r5, 0(%r4)
	cmpwi	%r5, 0			! null byte?
	bz	.strlen_done
	addi	%r3, %r3, 1
	addi	%r4, %r4, 1
	andi.	%r0, %r4, NBPW-1	! word aligned now?
	bnz	.algn_loop0

.algnd:
	! use trick to check if any read bytes of a word are zero
	! the following two constants will generate "byte carries"
	! and check if any bit in a byte is set, if all characters
	! are 7bits (unsigned) this allways works, otherwise
	! there is a specil case that rarely happens, see below
	lis	%r6, 0x7efe
	ori	%r6, %r6, 0xfeff	! %r6 = 0x7efefeff
	lis	%r7, EXT16(0x8101)
	ori	%r7, %r7, 0x0100	! %r7 = 0x81010100
	subi	%r4, %r4, 4
	subi	%r3, %r3, 4
.algn_loop1:				! main loop
	addi	%r3, %r3, 4
	lwzu	%r8, 4(%r4)
	add	%r9, %r8, %r6		! generate byte-carries
	xor	%r9, %r8, %r9		! see if original bits set
	and	%r9, %r7, %r9
	cmpw	%r9, %r7		! if ==, no zero bytes
	beq	.algn_loop1

	! check for the zero byte and increment the count appropriately
	! some information (the carry bit) is lost if bit 31
	! was set (very rare), if this is the rare condition,
	! return to the main loop again
#ifdef _LITTLE_ENDIAN
	li	%r6, 0xff		! mask used to test for terminator
	and.	%r0, %r6, %r8		! check if first byte was zero
	bnz-	.algn1
	blr
.algn1:
	slwi	%r6, %r6, 8
	and.	%r0, %r6, %r8		! check if second byte was zero
	bnz-	.algn2
	addi	%r3, %r3, 1
	blr
.algn2:
	slwi	%r6, %r6, 8
	and.	%r0, %r6, %r8		! check if third byte was zero
	bnz-	.algn3
	addi	%r3, %r3, 2
	blr
.algn3:
	slwi	%r6, %r6, 8
	and.	%r0, %r6, %r8		! check if last byte was zero
	bnz-	.algn_loop1
	addi	%r3, %r3, 3
#else
	lis	%r6, 0xff00		! mask used to test for terminator
	and.	%r0, %r6, %r9		! check if first byte was zero
	bnz-	.algn1
	blr
.algn1:
	srwi	%r6, %r6, 8
	and.	%r0, %r6, %r8		! check if second byte was zero
	bnz-	.algn2
	addi	%r3, %r3, 1
	blr
.algn2:
	srwi	%r6, %r6, 8
	and.	%r0, %r6, %r8		! check if third byte was zero
	bnz-	.algn3
	addi	%r3, %r3, 2
	blr
.algn3:
	srwi	%r6, %r6, 8
	and.	%r0, %r6, %r8		! check if last byte was zero
	bnz-	.algn_loop1
	addi	%r3, %r3, 3
#endif	/* _LITTLE_ENDIAN */
.strlen_done:
	blr
	SET_SIZE(strlen)

#endif	/* lint */

/*
 * copyinstr_noerr(s1, s2, len)
 *
 * Copy string s2 to s1.  s1 must be large enough and len contains the
 * number of bytes copied.  s1 is returned to the caller.
 * s2 is user space and s1 is in kernel space
 *
 * copyoutstr_noerr(s1, s2, len)
 *
 * Copy string s2 to s1.  s1 must be large enough and len contains the
 * number of bytes copied.  s1 is returned to the caller.
 * s2 is kernel space and s1 is in user space
 *
 * knstrcpy(s1, s2, len)
 *
 * This routine copies a string s2 in the kernel address space to string
 * s1 which is also in the kernel address space.
 *
 */

#if defined(lint) || defined(__lint)

/*ARGSUSED*/
char *
copyinstr_noerr(char *s1, char *s2, size_t *len)
{ return ((char *)0); }
 
/*ARGSUSED*/
char *
copyoutstr_noerr(char *s1, char *s2, size_t *len)
{ return ((char *)0); }

/* ARGSUSED */
char *
knstrcpy(char *s1, const char *s2, size_t *len)
{ return ((char)0); }

#else	/* lint */

	ENTRY(copyinstr_noerr)
#ifdef DEBUG
	lis	%r0, KERNELBASE >> 16
	!ori	%r0, %r0, KERNELBASE & 0xFFFF	! %r0 = KERNELBASE
	cmplw	%r3, %r0		! test uaddr < KERNELBASE
	bge+	cpyins1

cpyins3:
	
	lis	%r3,.cpyin_panic_msg@ha
	la	%r3,.cpyin_panic_msg@l(%r3)
	b	panic

cpyins1:
	cmplw	%r4, %r0
	blt+	.do_cpy
	b	cpyins3

#endif DEBUG

	ALTENTRY(copyoutstr_noerr)
#ifdef DEBUG
	lis	%r0, KERNELBASE >> 16
	!ori	%r0, %r0, KERNELBASE & 0xFFFF	! %r0 = KERNELBASE
	cmplw	%r4, %r0		! test uaddr < KERNELBASE
	bge+	cpyouts1

cpyouts3:
	
	lis	%r3,.cpyout_panic_msg@ha
	la	%r3,.cpyout_panic_msg@l(%r3)
	b	panic

cpyouts1:
	cmplw	%r3, %r0
	blt+	.do_cpy
	b	cpyouts3

#endif DEBUG

	ALTENTRY(knstrcpy)
#ifdef DEBUG
	lis	%r0, KERNELBASE >> 16
	!ori	%r0, %r0, KERNELBASE & 0xFFFF	! %r0 = KERNELBASE
	cmplw	%r3, %r0		! test uaddr < KERNELBASE
	bge+	knstr1

knstr3:
	
	lis	%r3,.knstrcpy_panic_msg@ha
	la	%r3,.knstrcpy_panic_msg@l(%r3)
	b	panic

knstr1:
	cmplw	%r4, %r0
	blt-	knstr3

#endif DEBUG

.do_cpy:
	li	%r10, 0			! set count to zero
	!
	! Check if both From and To are on the same byte alignment. If the
	! alignment is not the same or not word aligned then do byte copy.
	!
	xor	%r0, %r3, %r4		! xor from and to
	andi.	%r0, %r0, 3		! if lower two bits zero
	bnz-	.kstr_byteloop		! alignment is not the same
	andi.	%r0, %r4, NBPW-1	! is address word aligned?
	beq+	.kstrwcopy		! do copy from word-aligned source
	!
	! copy bytes until source/destination address is word aligned 
	!
	mr	%r9, %r4
.kstr_loop1:
	lbz	%r7, 0(%r9)		! load a byte
	addi	%r9, %r9, 1
	stbx	%r7, %r3, %r10		! store a byte
	addi	%r10, %r10, 1		! count += 1
	cmpwi	%r7, 0			! null byte
	beq-	.kstrdone
	andi.	%r0, %r9, NBPW-1	! is address word aligned?
	bne-	.kstr_loop1

	!
	! Word copy loop.
	!
.kstrwcopy:
	! use trick to check if any read bytes of a word are zero
	! the following two constants will generate "byte carries"
	! and check if any bit in a byte is set, if all characters
	! are 7bits (unsigned) this allways works, otherwise
	! there is a specil case that rarely happens, see below
	lis	%r6, 0x7efe
	ori	%r6, %r6, 0xfeff	! %r6 = 0x7efefeff
	lis	%r7, EXT16(0x8101)
	ori	%r7, %r7, 0x0100	! %r7 = 0x81010100
.kstr_loop2:				! main loop
	lwzx	%r8, %r4, %r10		! read the word
	add	%r9, %r8, %r6		! generate byte-carries
	xor	%r9, %r8, %r9		! see if original bits set
	and	%r9, %r7, %r9
	cmpw	%r9, %r7		! if ==, no zero bytes
	bne-	.kstralign
	stwx	%r8, %r3, %r10		! store the word
	addi	%r10, %r10, 4		! count += 4
	b	.kstr_loop2

	! check for the zero byte and increment the count appropriately
	! some information (the carry bit) is lost if bit 31
	! was set (very rare), if this is the rare condition,
	! return to the main loop again
.kstralign:
#ifdef _LITTLE_ENDIAN
	li	%r6, 0xff		! mask used to test for terminator
	and.	%r0, %r6, %r8		! check if first byte was zero
	bnz-	.kstralgn1
	stbx	%r8, %r3, %r10		! write the null byte
	addi	%r10, %r10, 1		! count += 1
	stw	%r10, 0(%r5)		! copy the length info
	blr
.kstralgn1:
	slwi	%r6, %r6, 8
	and.	%r0, %r6, %r8		! check if second byte was zero
	bnz-	.kstralgn2
	sthx	%r8, %r3, %r10		! write the two bytes (including null)
	addi	%r10, %r10, 2		! count += 2
	stw	%r10, 0(%r5)		! copy the length info
	blr
.kstralgn2:
	slwi	%r6, %r6, 8
	and.	%r0, %r6, %r8		! check if third byte was zero
	bnz-	.kstralgn3
	sthx	%r8, %r3, %r10		! write first two bytes
	li	%r0, 0
	addi	%r10, %r10, 2		! count += 2
	stbx	%r0, %r3, %r10		! write null byte
	addi	%r10, %r10, 1		! count += 1
	stw	%r10, 0(%r5)		! copy the length info
	blr
.kstralgn3:
	stwx	%r8, %r3, %r10		! write the word
	addi	%r10, %r10, 4		! count += 4
	slwi	%r6, %r6, 8
	and.	%r0, %r6, %r8		! check if last byte was zero
	bnz-	.kstr_loop2		! if not continue the loop
	stw	%r10, 0(%r5)		! copy the length info
	blr
#else	/* _BIG_ENDIAN */
	lis	%r6, 0xff00		! mask used to test for terminator
	and.	%r0, %r6, %r8		! check if first byte was zero
	bnz-	.kstralgn1
	li	%r0, 0
	stbx	%r0, %r3, %r10		! write the null byte
	addi	%r10, %r10, 1		! count += 1
	stw	%r10, 0(%r5)		! copy the length info
	blr
.kstralgn1:
	slwi	%r6, %r6, 8
	and.	%r0, %r6, %r8		! check if second byte was zero
	bnz-	.kstralgn2
	srwi	%r8, %r8, 16		! get to the second byte
	sthx	%r8, %r3, %r10		! write the two bytes (including null)
	addi	%r10, %r10, 2		! count += 2
	stw	%r10, 0(%r5)		! copy the length info
	blr
.kstralgn2:
	slwi	%r6, %r6, 8
	and.	%r0, %r6, %r8		! check if third byte was zero
	bnz-	.kstralgn3
	srwi	%r8, %r8, 16		! get to the second byte
	sthx	%r8, %r3, %r10		! write first two bytes
	li	%r0, 0
	addi	%r10, %r10, 2		! count += 2
	stbx	%r0, %r3, %r10		! write null byte
	addi	%r10, %r10, 1		! count += 1
	stw	%r10, 0(%r5)		! copy the length info
	blr
.kstralgn3:
	stwx	%r8, %r3, %r10		! write the word
	addi	%r10, %r10, 4		! count += 4
	slwi	%r6, %r6, 8
	and.	%r0, %r6, %r8		! check if last byte was zero
	bnz-	.kstr_loop2		! if not continue the loop
	stw	%r10, 0(%r5)		! copy the length info
	blr
#endif	/* _LITTLE_ENDIAN */
	!
	! Byte copy loop.
	!
.kstr_byteloop:
	lbzx	%r7, %r4, %r10		! read the byte
	stbx	%r7, %r3, %r10		! store the byte
	addi	%r10, %r10, 1		! count += 1
	cmpwi	%r7, 0
	bne	.kstr_byteloop
.kstrdone:
	stw	%r10, 0(%r5)		! copy the length info
	blr
	SET_SIZE(copyinstr_noerr)
	SET_SIZE(copyoutstr_noerr)
	SET_SIZE(knstrcpy)

#ifdef DEBUG
	.data
.cpyout_panic_msg:
	.string "copyoutstr_noerr: argument not in correct address space"
.cpyin_panic_msg:
	.string "copyinstr_noerr: argument not in correct address space"
.knstrcpy_panic_msg:
	.string "knstrcpy: argument not in kernel address space"
	.text
#endif /* DEBUG */


#endif	/* lint */

/*
 * Copy a page of memory.
 * Assumes locked memory pages and pages are cache block aligned.
 *
 * locked_pgcopy(from, to)
 *	caddr_t from, to;
 *
 * XXXPPC - Optimize the unwind pgcopy_loop1 to use multiple load/store
 *	    instructions to take the advantage of multiple instruction
 *	    execution.
 */
#if defined(lint)

/* ARGSUSED */
void
locked_pgcopy(caddr_t from, caddr_t to)
{}

#else	/* lint */

	ENTRY(locked_pgcopy)
	li	%r5, PAGESIZE >> 2	! word count in %r5
	!
	! We can use cache-fill-zero whenever possible to optimize the
	! memory accesses in the loop. This could cause alignment exception
	! if the destination page is not a cacheable page.
	!
	! %r3 = page aligned source address
	! %r4 = page aligned destination address
	! %r5 = word count (page size)
	lis	%r11, dcache_blocksize@ha
	lwz	%r8, dcache_blocksize@l(%r11)	! %r8 = line size in bytes
	srwi	%r9, %r8, 2			! %r9 = line size in words
	addi	%r7, %r8, 4
	subi	%r3, %r3, 4
	subi	%r4, %r4, 4
	li	%r10, 4
.pgcopy_loop:
	dcbz	%r10, %r4		! Clear the cache block for dest addr
	dcbt	%r7, %r3		! Preload the next cache block for
					!	the src page
	mtctr	%r9			! loop for cache block size
.pgcopy_loop1:
	lwzu	%r6, 4(%r3)
	stwu	%r6, 4(%r4)
	bdnz+	.pgcopy_loop1
	subf.	%r5, %r9, %r5		! count -= cache block size
	bnz+	.pgcopy_loop
	blr				! return
	SET_SIZE(locked_pgcopy)

#endif	/* lint */

/* The nofault version of the fu* and su* routines */

#if defined(lint)

/* ARGSUSED */
int
fuword_noerr(int *addr)
{ return(0); }
 
/* ARGSUSED */
int
fubyte_noerr(caddr_t addr)
{ return(0); }
 
/* ARGSUSED */
int
fusword_noerr(caddr_t addr)
{ return(0); }

/* ARGSUSED */
void
suword_noerr(int *addr, int value)
{}

/* ARGSUSED */
void
susword_noerr(caddr_t addr, int value)
{}

/* ARGSUSED */
void
subyte_noerr(caddr_t addr, char value)
{}

#else   /* lint */

	ENTRY(fuword_noerr)
	lwz	%r3, 0(%r3)
	blr
	SET_SIZE(fuword_noerr)

	ENTRY(fubyte_noerr)
	lbz	%r3, 0(%r3)
	blr
	SET_SIZE(fubyte_noerr)

	ENTRY(fusword_noerr)
	lhz	%r3, 0(%r3)
	blr
	SET_SIZE(fusword_noerr)

	ENTRY(suword_noerr)
	stw	%r4, 0(%r3)
	li	%r3, 0
	blr
	SET_SIZE(suword_noerr)

	ENTRY(subyte_noerr)
	stb	%r4, 0(%r3)
	li	%r3, 0
	blr
	SET_SIZE(subyte_noerr)

	ENTRY(susword_noerr)
	sth	%r4, 0(%r3)
	li	%r3, 0
	blr
	SET_SIZE(susword_noerr)

	
#endif
	
#if defined(lint)

/*
 * Copy a block of storage - must not overlap (from + len <= to).
 * No fault handler installed (to be called under on_fault())
 */

/* ARGSUSED */
void
copyout_noerr(const void *kfrom, void *uto, size_t count)
{}

/* ARGSUSED */
void
copyin_noerr(const void *ufrom, void *kto, size_t count)
{}

/*
 * Zero a block of storage in user space
 */

/* ARGSUSED */
void
uzero(void *addr, size_t count)
{}

/*
 * copy a block of storage in user space
 */

/* ARGSUSED */
void
ucopy(const void *ufrom, void *uto, size_t ulength)
{}

#else lint

	ENTRY(copyin_noerr)

#ifdef DEBUG
	lis	%r0, KERNELBASE >> 16
	!ori	%r0, %r0, KERNELBASE & 0xFFFF	! %r0 = KERNELBASE
	cmplw	%r3, %r0		! test uaddr < KERNELBASE
	bge-	copyin_ne_panic

	cmplw	%r4, %r0
	bge+	do_copyin_ne
copyin_ne_panic:

	lis	%r3,.cpyin_ne_pmsg@ha
	la	%r3,.cpyin_ne_pmsg@l(%r3)
	b	panic
#endif
do_copyin_ne:

	b	bcopy

	SET_SIZE(copyin_noerr)

#ifdef DEBUG
	.data
.cpyin_ne_pmsg:
	.string "copyin_noerr: arguments not in correct address space"
	.text
#endif

	ENTRY(copyout_noerr)
#ifdef DEBUG
	lis	%r0, KERNELBASE >> 16
	!ori	%r0, %r0, KERNELBASE & 0xFFFF	! %r0 = KERNELBASE
	cmplw	%r4, %r0		! test uaddr < KERNELBASE
	bge-	copyout_ne_panic

	cmplw	%r3, %r0
	bge+	do_copyout_ne
copyout_ne_panic:

	lis	%r3,.cpyout_ne_pmsg@ha
	la	%r3,.cpyout_ne_pmsg@l(%r3)
	b	panic
#endif DEBUG
do_copyout_ne:

	b	bcopy

	SET_SIZE(copyout_noerr)

#ifdef DEBUG
	.data
.cpyout_ne_pmsg:
	.string "copyout_noerr: arguments not in correct address space"
	.text
#endif



	ENTRY(uzero)
#ifdef	DEBUG
	lis	%r0, KERNELBASE >> 16
	!ori	%r0, %r0, KERNELBASE & 0xFFFF	! %r0 = KERNELBASE
	cmplw	%r3, %r0		! test uaddr < KERNELBASE
	bge-	uzero_panic
#endif DEBUG

	b	bzero
#ifdef DEBUG
uzero_panic:

	lis	%r3,.uzero_panic_msg@ha
	la	%r3,.uzero_panic_msg@l(%r3)
	b	panic
#endif
	SET_SIZE(uzero)

#ifdef DEBUG
	.data
.uzero_panic_msg:
	.string "uzero: argument is not in user space"
	.text

#endif DEBUG
	
	
	ENTRY(ucopy)

#ifdef DEBUG
	lis	%r0, KERNELBASE >> 16
	!ori	%r0, %r0, KERNELBASE & 0xFFFF	! %r0 = KERNELBASE
	cmplw	%r3, %r0		! test uaddr < KERNELBASE
	bge-	ucopy_panic

	cmplw	%r4, %r0
	bge-	ucopy_panic
#endif DEBUG

	b	bcopy

#ifdef DEBUG
ucopy_panic:

	lis	%r3,.ucopy_panic_msg@ha
	la	%r3,.ucopy_panic_msg@l(%r3)
	b	panic
#endif
	SET_SIZE(ucopy)

#ifdef DEBUG
	.data
.ucopy_panic_msg:
	.string "ucopy: argument is not in user space"
	.text

#endif DEBUG
#endif lint
