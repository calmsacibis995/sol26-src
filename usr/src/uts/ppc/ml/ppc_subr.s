/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)ppc_subr.s	1.25	96/07/02 SMI"

/*
 * General assembly language routines.
 * It is the intent of this file to contain routines that are
 * independent of the specific kernel architecture, and those that are
 * common across kernel architectures.
 * As architectures diverge, and implementations of specific
 * architecture-dependent routines change, the routines should be moved
 * from this file into the respective ../`arch -k`/subr.s file.
 */

#include <sys/asm_linkage.h>
#include <sys/machthread.h>
#include <sys/psw.h>

#if defined(lint) || defined(__lint)
#include <sys/types.h>
#include <sys/thread.h>
#include <sys/reg.h>
#else
#include "assym.s"
#endif	/* lint */


/*
 * on_fault()
 * Catch lofault faults. Like setjmp except it returns one
 * if code following causes uncorrectable fault. Turned off
 * by calling no_fault().
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
on_fault(label_t *ljb)
{ return (0); }

#else	/* lint */

	ENTRY(on_fault)
	lis	%r11, catch_fault@ha
	la	%r0, catch_fault@l(%r11)
	stw	%r3, T_ONFAULT(THREAD_REG)	! jumpbuf address for onfault
	stw	%r0, T_LOFAULT(THREAD_REG)	! catch_fault is lofault value
	b	setjmp				! let the setjmp() do the rest

catch_fault:
	li	%r0, 0
	lwz	%r3, T_ONFAULT(THREAD_REG)	! address of save area
	stw	%r0, T_ONFAULT(THREAD_REG)	! turnoff onfault
	stw	%r0, T_LOFAULT(THREAD_REG)	! turnoff lofault
	b	longjmp				! let longjmp() do the rest
	SET_SIZE(on_fault)

#endif	/* lint */

/*
 * no_fault()
 * turn off fault catching.
 */

#if defined(lint) || defined(__lint)

void
no_fault(void)
{}

#else	/* lint */

	ENTRY(no_fault)
	li	%r0, 0
	stw	%r0, T_ONFAULT(THREAD_REG)
	stw	%r0, T_LOFAULT(THREAD_REG)	! turnoff lofault
	blr
	SET_SIZE(no_fault)

#endif	/* lint */

/*
 * Setjmp and longjmp implement non-local gotos using state vectors
 * type label_t.
 *
 * setjmp(lp)
 * label_t *lp;
 *
 * The saved state consists of:
 *
 *		+----------------+  0
 *		|      r1 (sp)   |
 *		+----------------+  4
 *		| pc (ret addr)  |
 *		+----------------+  8
 *		|      cr        |
 *		+----------------+  12
 *		|      r13       |
 *		+----------------+  16
 *		|      ...       |
 *		+----------------+  84
 *		|      r31       |
 *		+----------------+  <------  sizeof (label_t) = 88 bytes
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
setjmp(label_t *lp)
{ return (0); }

#else	/* lint */

	ENTRY(setjmp)
	stw	%r1, 0(%r3)
	mflr	%r5
	stw	%r5, 4(%r3)
	mfcr	%r5
	stw	%r5, 8(%r3)
	stw	%r13, 12(%r3)
	stw	%r14, 16(%r3)
	stw	%r15, 20(%r3)
	stw	%r16, 24(%r3)
	stw	%r17, 28(%r3)
	stw	%r18, 32(%r3)
	stw	%r19, 36(%r3)
	stw	%r20, 40(%r3)
	stw	%r21, 44(%r3)
	stw	%r22, 48(%r3)
	stw	%r23, 52(%r3)
	stw	%r24, 56(%r3)
	stw	%r25, 60(%r3)
	stw	%r26, 64(%r3)
	stw	%r27, 68(%r3)
	stw	%r28, 72(%r3)
	stw	%r29, 76(%r3)
	stw	%r30, 80(%r3)
	stw	%r31, 84(%r3)
	li	%r3,0			! retval is 0
	blr
	SET_SIZE(setjmp)

#endif	/* lint */

/*
 * longjmp(lp)
 * label_t *lp;
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
longjmp(label_t *lp)
{}

#else	/* lint */

	ENTRY(longjmp)
	lwz	%r1, 0(%r3)
	lwz	%r5, 4(%r3)
	mtlr	%r5
	lwz	%r5, 8(%r3)
	mtcrf	0xff, %r5
	lwz	%r13, 12(%r3)
	lwz	%r14, 16(%r3)
	lwz	%r15, 20(%r3)
	lwz	%r16, 24(%r3)
	lwz	%r17, 28(%r3)
	lwz	%r18, 32(%r3)
	lwz	%r19, 36(%r3)
	lwz	%r20, 40(%r3)
	lwz	%r21, 44(%r3)
	lwz	%r22, 48(%r3)
	lwz	%r23, 52(%r3)
	lwz	%r24, 56(%r3)
	lwz	%r25, 60(%r3)
	lwz	%r26, 64(%r3)
	lwz	%r27, 68(%r3)
	lwz	%r28, 72(%r3)
	lwz	%r29, 76(%r3)
	lwz	%r30, 80(%r3)
	lwz	%r31, 84(%r3)
	li	%r3, 1			! return 1
	blr
	SET_SIZE(longjmp)

#endif	/* lint */

/*
 * if a() calls b() calls caller(),
 * caller() returns return address in a().
 * (Note: We assume a() and b() are C routines which do the normal entry/exit
 *  sequence.)
 */

#if defined(lint) || defined(__lint)

caddr_t
caller(void)
{ return (0); }

#else	/* lint */

	ENTRY(caller)
	lwz	%r3, 0(%r1)
	lwz	%r3, 4(%r3)
	blr
	SET_SIZE(caller)

#endif	/* lint */

/*
 * if a() calls callee(), callee() returns the
 * return address in a();
 */

#if defined(lint) || defined(__lint)

caddr_t
callee(void)
{ return (0); }

#else	/* lint */

	ENTRY(callee)
	mflr	%r3
	blr
	SET_SIZE(callee)

#endif	/* lint */

/*
 * if a() calls b() calls c() calls d() calls caller3(),
 * caller3() returns return address in a().
 * (Note: We assume a(), b(), c() and d() are C routines which do the normal
 *  entry/exit sequence.)
 */

#if defined(lint) || defined(__lint)

caddr_t
caller3(void)
{ return (0); }

#else	/* lint */

	ENTRY(caller3)
	lwz	%r3, 0(%r1)		! fp of c()
	lwz	%r3, 0(%r3)		! fp of b()
	lwz	%r3, 0(%r3)		! fp of a()
	lwz	%r3, 4(%r3)		! return pc in a()
	blr
	SET_SIZE(caller3)

#endif	/* lint */
/*
 * if a() calls b() calls c() calls caller2(),
 * caller2() returns return address in a().
 * (Note: We assume a(),b() and c() are C routines which do the normal
 *  entry/exit sequence.)
 */

#if defined(lint) || defined(__lint)

caddr_t
caller2(void)
{ return (0); }

#else	/* lint */

	ENTRY(caller2)
	lwz	%r3, 0(%r1)		! fp of b()
	lwz	%r3, 0(%r3)		! fp of a()
	lwz	%r3, 4(%r3)		! return pc in a()
	blr
	SET_SIZE(caller2)

#endif	/* lint */

/*
 * return the current stack pointer
 */

#if defined(lint) || defined(__lint)

greg_t
getsp(void)
{ return (0); }

#else	/* lint */

	ENTRY(getsp)
	mr	%r3, %r1
	blr
	SET_SIZE(getsp)

#endif	/* lint */

/*
 * return the current frame pointer
 */

#if defined(lint) || defined(__lint)

greg_t
getfp(void)
{ return (0); }

#else	/* lint */

	ENTRY(getfp)
	lwz	%r3, 0(%r1)
	blr
	SET_SIZE(getfp)

#endif	/* lint */

/*
 * Get current processor interrupt level
 */

#if defined(lint) || defined(__lint)

greg_t
getpil(void)
{ return (0); }

#else	/* lint */

	ENTRY(getpil)
	lwz	%r4, T_CPU(THREAD_REG)
	lwz	%r3, CPU_PRI(%r4)		! cpu->cpu_m.cpu_pri
	blr
	SET_SIZE(getpil)

#endif	/* lint */

/*
 * _insque(entryp, predp)
 *
 * Insert entryp after predp in a doubly linked list.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
_insque(caddr_t entryp, caddr_t predp)
{}

#else	/* lint */

	ENTRY(_insque)
	lwz	%r5, 0(%r4)		! predp->forw
	stw	%r4, 4(%r3)		! entryp->back = predp
	stw	%r5, 0(%r3)		! entryp->forw = predp->forw
	stw	%r3, 0(%r4)		! predp->forw = entryp
	stw	%r3, 4(%r5)		! predp->forw->back = entryp
	blr
	SET_SIZE(_insque)

#endif	/* lint */

/*
 * _remque(entryp)
 *
 * Remove entryp from a doubly linked list
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
_remque(caddr_t entryp)
{}

#else	/* lint */

	ENTRY(_remque)
	lwz	%r5, 0(%r3)		! entryp->forw
	lwz	%r6, 4(%r3)		! entryp->back
	stw	%r5, 0(%r6)		! entryp->back->forw = entryp->forw
	stw	%r6, 4(%r5)		! entryp->forw->back = entryp->back
	blr
	SET_SIZE(_remque)

#endif	/* lint */

/*
 *	SPL routines
 */

/*
 * Macro to raise processor priority level.
 * Avoid dropping processor priority if already at high level.
 * Also avoid going below CPU->cpu_base_spl, which could've just been set by
 * a higher-level interrupt thread that just blocked.
 */
#define	RAISE(level) \
	li	%r3, level;\
	b	splr

/*
 * Macro to set the priority to a specified level.
 * Avoid dropping the priority below CPU->cpu_base_spl.
 */
#define SETPRI(level) \
	li	%r3, level;\
	b	spl

	/*
	 * Berkley 4.3 introduced symbolically named interrupt levels
	 * as a way deal with priority in a machine independent fashion.
	 * Numbered priorities are machine specific, and should be
	 * discouraged where possible.
	 *
	 * Note, for the machine specific priorities there are
	 * examples listed for devices that use a particular priority.
	 * It should not be construed that all devices of that
	 * type should be at that priority.  It is currently were
	 * the current devices fit into the priority scheme based
	 * upon time criticalness.
	 *
	 * The underlying assumption of these assignments is that
	 * IPL 10 is the highest level from which a device
	 * routine can call wakeup.  Devices that interrupt from higher
	 * levels are restricted in what they can do.  If they need
	 * kernels services they should schedule a routine at a lower
	 * level (via software interrupt) to do the required
	 * processing.
	 *
	 * Examples of this higher usage:
	 *	Level	Usage
	 *	14	Profiling clock (and PROM uart polling clock)
	 *	12	Serial ports
	 *
	 * The serial ports request lower level processing on level 6.
	 *
	 * Also, almost all splN routines (where N is a number or a
	 * mnemonic) will do a RAISE(), on the assumption that they are
	 * never used to lower our priority.
	 * The exceptions are:
	 *	spl8()		Because you can't be above 15 to begin with!
	 *	splzs()		Because this is used at boot time to lower our
	 *			priority, to allow the PROM to poll the uart.
	 *	spl0()		Used to lower priority to 0.
	 *	splsoftclock()	Used by hardclock to lower priority.
	 */

#if defined(lint) || defined(__lint)

int spl8(void)		{ return (0); }
int spl7(void)		{ return (0); }
int splzs(void)		{ return (0); }
int splhigh(void)	{ return (0); }
int splhi(void)		{ return (0); }
int splclock(void)	{ return (0); }
int spltty(void)	{ return (0); }
int splbio(void)	{ return (0); }
int spl6(void)		{ return (0); }
int spl5(void)		{ return (0); }
int spl4(void)		{ return (0); }
int splimp(void)	{ return (0); }
int spl3(void)		{ return (0); }
int spl2(void)		{ return (0); }
int spl1(void)		{ return (0); }
int splnet(void)	{ return (0); }
int splsoftclock(void)	{ return (0); }
int spl0(void)		{ return (0); }

#else	/* lint */

	/* locks out all interrupts, including memory errors */
	ENTRY(spl8)
	SETPRI(15)
	SET_SIZE(spl8)

	/* just below the level that profiling runs */
	ENTRY(spl7)
	RAISE(13)
	SET_SIZE(spl7)

	/* sun specific - highest priority onboard serial i/o asy ports */
	ALTENTRY(splzs)
	SETPRI(12)	/* Can't be a RAISE, as it's used to lower us */
	SET_SIZE(splzs)

	/*
	 * should lock out clocks and all interrupts,
	 * as you can see, there are exceptions
	 */
	ALTENTRY(splhigh)
	ALTENTRY(splhi)

	/* the standard clock interrupt priority */
	ALTENTRY(splclock)

	/* highest priority for any tty handling */
	ALTENTRY(spltty)

	/* highest priority required for protection of buffered io system */
	ALTENTRY(splbio)

	/* machine specific */
	ENTRY2(spl6,spl5)
	RAISE(10)
	SET_SIZE(splhigh)
	SET_SIZE(splhi)
	SET_SIZE(splclock)
	SET_SIZE(spltty)
	SET_SIZE(splbio)
	SET_SIZE(spl5)
	SET_SIZE(spl6)

	/*
	 * machine specific
	 * for sun, some frame buffers must be at this priority
	 */
	ENTRY(spl4)
	RAISE(8)
	SET_SIZE(spl4)

	/* highest level that any network device will use */
	ALTENTRY(splimp)

	/*
	 * machine specific
	 * for sun, devices with limited buffering: tapes, ethernet
	 */
	ENTRY(spl3)
	RAISE(6)
	SET_SIZE(splimp)
	SET_SIZE(spl3)

	/*
	 * machine specific - not as time critical as above
	 * for sun, disks
	 */
	ENTRY(spl2)
	RAISE(4)
	SET_SIZE(spl2)

	ENTRY(spl1)
	RAISE(2)
	SET_SIZE(spl1)

	/* highest level that any protocol handler will run */
	ENTRY(splnet)
	RAISE(1)
	SET_SIZE(splnet)

	/* softcall priority */
	/* used by hardclock to LOWER priority */
	ENTRY(splsoftclock)
	SETPRI(1)
	SET_SIZE(splsoftclock)

	/* allow all interrupts */
	ENTRY(spl0)
	SETPRI(0)
	SET_SIZE(spl0)

#endif	/* lint */

/*
 * splr is like splx but will only raise the priority and never drop it
 */
#if defined(lint) || defined(__lint)
/*ARGSUSED*/
int
splr(int level)
{ return (0); }
#else	/* lint */
	ENTRY(splr)
	lwz	%r4, T_CPU(THREAD_REG)
	lwz	%r4, CPU_PRI(%r4)	! get current level
	cmpw	%r3, %r4		! if new level > current level
	bgt	spl			! then set ipl to new level
	mr	%r3, %r4		! else return current level
	blr
	SET_SIZE(splr)
#endif	/* lint */

#if defined(lint) || defined(__lint)

/*
 * algorithm for spl:
 *
 *      turn off interrupts
 *
 *	if (CPU->cpu_base_spl > newipl)
 *		newipl = CPU->cpu_base_spl;
 *      oldipl = CPU->cpu_pridata->c_ipl;
 *      CPU->cpu_pridata->c_ipl = newipl;
 *
 *      setspl();  // load new masks into pics
 *
 * Be careful not to set priority lower than CPU->cpu_base_pri,
 * even though it seems we're raising the priority, it could be set
 * higher at any time by an interrupt routine, so we must block interrupts
 * and look at CPU->cpu_base_pri
 */

/*ARGSUSED*/
int
spl(int level)
{ return (0); }

/*
 * splx - set PIL back to that indicated by the old level passed as an argument,
 * or to the CPU's base priority, whichever is higher.
 */
/*ARGSUSED*/
int
splx(int level)
{ return (0); }

#else	/* lint */

#define	FRAMESIZE	SA(MINFRAME+12)

	ENTRY_NP2(spl, splx)
	mflr	%r0
	stw	%r0, 4(%r1)		! save LR contents
	stwu	%r1, -FRAMESIZE(%r1)	! set back chain, allocate stack

	! save r29-r31
	stw	%r29, MINFRAME(%r1)
	stw	%r30, MINFRAME+4(%r1)
	stw	%r31, MINFRAME+8(%r1)

	mfmsr	%r30
	rlwinm	%r11,%r30,0,17,15	! clear MSR_EE
	mtmsr	%r11			! disable interrupts
	lwz	%r5, T_CPU(THREAD_REG)
	lwz	%r6, CPU_BASE_SPL(%r5)	! get base_spl
	cmpw	%r3, %r6		! if (new ipl >= base_spl
	bge	.setprilev		! then skip
	mr	%r3, %r6		! else use base_spl
.setprilev:
	lwz	%r29, CPU_PRI(%r5)	! get current ipl
	stw	%r3, CPU_PRI(%r5)	! set new ipl
	lis	%r4,setspl@ha
	lwz	%r4,setspl@l(%r4)
	mtlr	%r4
	blrl				! call setspl(new ipl)
	mtmsr	%r30			! restore old msr
	mr	%r3, %r29		! return old ipl

	! restore registers r29-r31
	lwz	%r29, MINFRAME(%r1)
	lwz	%r30, MINFRAME+4(%r1)
	lwz	%r31, MINFRAME+8(%r1)
	addi	%r1, %r1, FRAMESIZE	! restore stack pointer
	lwz	%r0, 4(%r1)		! restore return address
	mtlr	%r0			! set LR with return address
	blr
	SET_SIZE(spl)
	SET_SIZE(splx)

#endif /* lint */

/***************************************************************************
 *			C callable in and out routines
 **************************************************************************/

/*
 * Boot uses physical addresses to do ins and outs but in the kernel we
 * would like to use mapped addresses. The following in/out routines
 * assumes PCIISA_VBASE as the virtual base (64k aligned) which is already
 * mapped in the kernel startup.
 */

/*
 * outl(port address, val)
 *   write val to port
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
outl(int port_address, unsigned long val)
{ }

#else	/* lint */

	ENTRY(outl)
	lis	%r5, PCIISA_VBASE >> 16
	!andi.	%r3, %r3, 0xffff	! mask the port number
	eieio
	stwx	%r4, %r3, %r5
	blr
	SET_SIZE(outl)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
outw(int port_address, unsigned short val)
{ }

#else	/* lint */

	ENTRY(outw)
	lis	%r5, PCIISA_VBASE >> 16
	!andi.	%r3, %r3, 0xffff	! mask the port number
	eieio
	sthx	%r4, %r3, %r5
	blr
	SET_SIZE(outw)

#endif  /* lint */


#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
outb(int port_address, unsigned char val)
{ }

#else	/* lint */

	ENTRY(outb)
	lis	%r5, PCIISA_VBASE >> 16
	!andi.	%r3, %r3, 0xffff	! mask the port number
	eieio
	stbx	%r4, %r3, %r5
	blr
	SET_SIZE(outb)

#endif  /* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
unsigned long
inl(int port_address)
{ return (0); }

#else	/* lint */

	ENTRY(inl)
	lis	%r5, PCIISA_VBASE >> 16
	!andi.	%r3, %r3, 0xffff	! mask the port number
	eieio
	lwzx	%r3, %r3, %r5
	blr
	SET_SIZE(inl)

#endif  /* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
unsigned short
inw(int port_address)
{ return (0); }

#else	/* lint */

	ENTRY(inw)
	lis	%r5, PCIISA_VBASE >> 16
	!andi.	%r3, %r3, 0xffff	! mask the port number
	eieio
	lhzx	%r3, %r3, %r5
	blr
	SET_SIZE(inw)

#endif  /* lint */


#if defined(lint) || defined(__lint)

/* ARGSUSED */
unsigned char
inb(int port_address)
{ return ((char)0); }

#else	/* lint */
	ENTRY(inb)
	lis	%r5, PCIISA_VBASE >> 16
	!andi.	%r3, %r3, 0xffff	! mask the port number
	eieio
	lbzx	%r3, %r3, %r5
	blr
	SET_SIZE(inb)

#endif  /* lint */

/*
 * The following routines move strings to and from an I/O port.
 * loutw(port, addr, count);
 * linw(port, addr, count);
 * repinsw(port, addr, cnt) - input a stream of 16-bit words
 * repoutsw(port, addr, cnt) - output a stream of 16-bit words
 *	Note: addr is assumed to be half word aligned.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
loutw(int port, caddr_t address, int count)
{}

/* ARGSUSED */
void
repoutsw(int port, unsigned short *addr, int cnt)
{}

#else	/* lint */

	ENTRY2(loutw,repoutsw)
	cmpwi	%r5, 0			! check for zero count
	beq	.loutw_done
	lis	%r6, PCIISA_VBASE >> 16
	!andi.	%r3, %r3, 0xffff	! mask the port number
	mtctr	%r5			! set count in counter register
	subi	%r4, %r4, 2
.loutw_loop:
	lhzu	%r7, 2(%r4)
	eieio
	sthx	%r7, %r3, %r6
	bdnz	.loutw_loop
.loutw_done:
	blr
	SET_SIZE(loutw)
	SET_SIZE(repoutsw)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
linw(int port, caddr_t addr, int count)
{ }

/* ARGSUSED */
void
repinsw(int port, unsigned short *addr, int count)
{ }

#else	/* lint */
	ENTRY2(linw,repinsw)
	cmpwi	%r5, 0			! check for zero count
	beq	.linw_done
	lis	%r6, PCIISA_VBASE >> 16
	!andi.	%r3, %r3, 0xffff	! mask the port number
	mtctr	%r5			! set count in counter register
	subi	%r4, %r4, 2
.linw_loop:
	eieio
	lhzx	%r7, %r3, %r6
	sthu	%r7, 2(%r4)
	bdnz	.linw_loop
.linw_done:
	blr
	SET_SIZE(linw)
	SET_SIZE(repinsw)

#endif	/* lint */


#if defined(lint) || defined(__lint)

/*
 * repinsb(port, addr, cnt) - input a stream of bytes
 */

/* ARGSUSED */
void
repinsb(int port, unsigned char *addr, int count)
{ }

#else	/* lint */
	ENTRY(repinsb)
	cmpwi	%r5, 0			! check for zero count
	beq	.linb_done
	lis	%r6, PCIISA_VBASE >> 16
	!andi.	%r3, %r3, 0xffff	! mask the port number
	mtctr	%r5			! set count in counter register
	subi	%r4, %r4, 1
.linb_loop:
	eieio
	lbzx	%r7, %r3, %r6
	stbu	%r7, 1(%r4)
	bdnz	.linb_loop
.linb_done:
	blr
	SET_SIZE(repinsb)

#endif	/* lint */


/*
 * repinsd(port, addr, cnt) - output a stream of 32-bit words
 *	Note: addr is assumed to be word aligned.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
repinsd(int port, unsigned long *addr, int count)
{ }

#else	/* lint */

	ENTRY(repinsd)
	cmpwi	%r5, 0			! check for zero count
	beq	.linl_done
	lis	%r6, PCIISA_VBASE >> 16
	!andi.	%r3, %r3, 0xffff	! mask the port number
	mtctr	%r5			! set count in counter register
	subi	%r4, %r4, 4
.linl_loop:
	eieio
	lwzx	%r7, %r3, %r6
	stwu	%r7, 4(%r4)
	bdnz	.linl_loop
.linl_done:
	blr
	SET_SIZE(repinsd)

#endif	/* lint */

/*
 * repoutsb(port, addr, cnt) - output a stream of bytes
 *    NOTE: count is a byte count
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
repoutsb(int port, unsigned char *addr, int count)
{ }

#else	/* lint */

	ENTRY(repoutsb)
	cmpwi	%r5, 0			! check for zero count
	beq	.loutb_done
	lis	%r6, PCIISA_VBASE >> 16
	!andi.	%r3, %r3, 0xffff	! mask the port number
	mtctr	%r5			! set count in counter register
	subi	%r4, %r4, 1
.loutb_loop:
	lbzu	%r7, 1(%r4)
	eieio
	stbx	%r7, %r3, %r6
	bdnz	.loutb_loop
.loutb_done:
	blr
	SET_SIZE(repoutsb)

#endif	/* lint */

/*
 * repoutsd(port, addr, cnt) - output a stream of 32-bit words
 * NOTE: count is a DWORD count. And addr is word aligned.
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
repoutsd(int port, unsigned long *addr, int count)
{ }

#else	/* lint */

	ENTRY(repoutsd)
	cmpwi	%r5, 0			! check for zero count
	beq	.loutl_done
	lis	%r6, PCIISA_VBASE >> 16
	!andi.	%r3, %r3, 0xffff	! mask the port number
	mtctr	%r5			! set count in counter register
	subi	%r4, %r4, 4
.loutl_loop:
	lwzu	%r7, 4(%r4)
	eieio
	stwx	%r7, %r3, %r6
	bdnz	.loutl_loop
.loutl_done:
	blr
	SET_SIZE(repoutsd)

#endif	/* lint */



#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
scanc(u_int size, u_char *cp, u_char *table, u_char mask)
{return 0;}

#else	/* lint */

	ENTRY(scanc)
	cmpwi	%r3, 0
	li	%r7, 0			! set index to zero
	ble-	.scanc_done		! check length
.scanc_loop:
	lbzx	%r10, %r4, %r7		! get next byte in string
	cmpw	%r7, %r3		! while (index < length)
	bge-	.scanc_done
	lbzx	%r10, %r5, %r10		! %r10 = table[*cp]
	addi	%r7, %r7, 1
	and.	%r10, %r10, %r6		! apply the mask
	bz	.scanc_loop
	subi	%r7, %r7, 1		! undo the increment above
.scanc_done:
	subf	%r3, %r7, %r3		! return (length - index)
	blr
	SET_SIZE(scanc)

#endif	/* lint */

/*
 *	Replacement functions for ones that are normally inlined.
 *	In addition to the copy in ppc.il, they are defined here just in case.
 */

#if defined(lint) || defined(__lint)
int
clear_int_flag(void)
{return 0;}

#else	/* lint */

	ENTRY(clear_int_flag)
	mfmsr	%r3
	rlwinm	%r4,%r3,0,17,15		! clear MSR_EE
	mtmsr	%r4			! disable interrupts
	blr				! return old msr
	SET_SIZE(clear_int_flag)
#endif	/* lint */

/*
 *	Enable interrupts, the first time.
 */

#if defined(lint) || defined(__lint)
int
enable_interrupts(void)
{return 0;}

#else	/* lint */

	ENTRY(enable_interrupts)
	mfmsr	%r3
	ori	%r3, %r3, MSR_EE
	mtmsr	%r3			! disable interrupts
	blr				! return old msr
	SET_SIZE(enable_interrupts)
#endif	/* lint */

#if defined(lint) || defined(__lint)
/*ARGSUSED*/
u_long
htonl(u_long i)
{return (0);}

#else	/* lint */

	ENTRY(htonl)
	/* Bit numbering: 0-MSB and 31-LSB assumed */
	!
	! stw	%r3, 24(%r1)	/* store the word in save area
	! lwbrx	%r3, 24(%r1)	/* read the word back with byte reversed */
	! blr
	mr	%r4, %r3
	rlwimi	%r3, %r4, 24, 0, 31	! %r3 = B0 B3 B2 B1
	rlwimi	%r3, %r4, 8, 8, 15	! %r3 = B0 B1 B2 B1
	rlwimi	%r3, %r4, 8, 24, 31	! %r3 = B0 B1 B2 B3
	blr
	SET_SIZE(htonl)
#endif	/* lint */

#if defined(lint) || defined(__lint)
/*ARGSUSED*/
u_short
htons(u_short i)
{return (0);}

#else	/* lint */

	ENTRY(htons)
	/* Bit numbering: 0-MSB and 31-LSB assumed */
	rlwimi	%r3, %r3, 8, 0, 31	! %r3 = X B1 B0 X
	rlwimi	%r3, %r3, 16, 24, 31	! %r3 = X B1 B0 B1
	andi.	%r3, %r3, 0xffff	! %r3 = 0 0 B0 B1
	blr
	SET_SIZE(htons)
#endif	/* lint */

#if defined(lint) || defined(__lint)
/*ARGSUSED*/
u_long
ntohl(u_long i)
{ return (0); }

#else	/* lint */

	ENTRY(ntohl)
	/* Bit numbering: 0-MSB and 31-LSB assumed */
	!
	! stw	%r3, 24(%r1)	/* store the word in save area
	! lwbrx	%r3, 24(%r1)	/* read the word back with byte reversed */
	! blr
	mr	%r4, %r3
	rlwimi	%r3, %r4, 24, 0, 31	! %r3 = B0 B3 B2 B1
	rlwimi	%r3, %r4, 8, 8, 15	! %r3 = B0 B1 B2 B1
	rlwimi	%r3, %r4, 8, 24, 31	! %r3 = B0 B1 B2 B3
	blr
	SET_SIZE(ntohl)
#endif	/* lint */

#if defined(lint) || defined(__lint)
/*ARGSUSED*/
u_short
ntohs(u_short i)
{ return (0); }

#else	/* lint */

	ENTRY(ntohs)
	/* Bit numbering: 0-MSB and 31-LSB assumed */
	rlwimi	%r3, %r3, 8, 0, 31	! %r3 = X B1 B0 X
	rlwimi	%r3, %r3, 16, 24, 31	! %r3 = X B1 B0 B1
	andi.	%r3, %r3, 0xffff	! %r3 = 0 0 B0 B1
	blr
	SET_SIZE(ntohs)
#endif	/* lint */

#if defined(lint) || defined(__lint)
/*ARGSUSED*/
int
cntlzw(int i)
{ return (0); }

#else	/* lint */

	ENTRY(cntlzw)
	cntlzw	%r3, %r3
	blr
	SET_SIZE(cntlzw)
#endif	/* lint */

#if defined(lint) || defined(__lint)
/*ARGSUSED*/
void
restore_int_flag(int i)
{}

#else	/* lint */

	ENTRY(restore_int_flag)
	mtmsr	%r3			! restore old msr
	blr
	SET_SIZE(restore_int_flag)
#endif	/* lint */

#if defined(lint) || defined(__lint)
long
get_msr(void)
{ return (0); }

#else	/* lint */

	ENTRY(get_msr)
	mfmsr	%r3		! get the kernel's msr
	blr
	SET_SIZE(restore_int_flag)
#endif	/* lint */

#if defined(lint) || defined(__lint)
kthread_id_t
threadp(void)
{return ((kthread_id_t)0);}

#else	/* lint */

	ENTRY(threadp)
	mr	%r3, THREAD_REG
	blr
	SET_SIZE(threadp)
#endif	/* lint */

#if defined(lint) || defined(__lint)
void
eieio(void)
{}

#else	/* lint */

	ENTRY(eieio)
	eieio
	blr
	SET_SIZE(eieio)
#endif	/* lint */

#ifdef lint
/*
 *      make the memory at {addr, addr+size} valid for instruction execution.
 *
 *	NOTE: it is assumed that cache blocks are no smaller than 32 bytes.
 */
/*ARGSUSED*/
void
sync_instruction_memory(caddr_t addr, u_int len)
{
}
#else
	ENTRY(sync_instruction_memory)
	li	%r5,32  	! MINIMUM cache block size in bytes
	li	%r8,5		! corresponding shift

	subi	%r7,%r5,1	! cache block size in bytes - 1
	and	%r6,%r3,%r7	! align "addr" to beginning of cache block
	subf	%r3,%r6,%r3	! ... so that termination check is trivial
	add	%r4,%r4,%r6	! increase "len" to reach the end because
				! ... we're starting %r6 bytes before addr
	add	%r4,%r4,%r7	! round "len" up to cache block boundary
!!!	andc	%r4,%r4,%r7	! mask off low bit (not necessary because
				! the following shift throws them away)
	srw	%r4,%r4,%r8	! turn "len" into a loop count
	mr	%r6,%r3		! copy of r3

	mtctr	%r4
1:
	dcbst	%r0,%r3 	! force to memory
	add	%r3,%r3,%r5
	bdnz	1b

	sync			! guarantee dcbst is done before icbi
				! one sync for all the dcbsts

	mr	%r3,%r6
	mtctr	%r4
1:
	icbi	%r0,%r3 	! force out of instruction cache
	add	%r3,%r3,%r5
	bdnz	1b

	sync			! one sync for all the icbis
	isync
	blr  
	SET_SIZE(sync_instruction_memory)
#endif

/*
 * Kernel probes assembly routines
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
tnfw_b_get_lock(u_char *lp)
{ return (0); }

/* ARGSUSED */
void
tnfw_b_clear_lock(u_char *lp)
{}

/* ARGSUSED */
u_long
tnfw_b_atomic_swap(u_long *wp, u_long x)
{ return (0); }

#else  /* lint */

	ENTRY(tnfw_b_get_lock)
	mflr	%r0			# get link register
	stw	%r0, 4(%r1)		# save link register
	stwu	%r1, -SA(MINFRAME)(%r1)	# drop stack
	bl	lock_try		# call lock_try();
	cmpwi	%r3, 0			# did we get the lock
	li	%r3, 0			# return zero in case we did
	bne+	.L1			# got the lock
	li	%r3, 1			# return non-zero if we didn't
.L1:
	addi	%r1, %r1, SA(MINFRAME)	# pop stack
	lwz	%r0, 4(%r1)		# restore link register
	mtlr	%r0
	blr				# return
	SET_SIZE(tnfw_b_get_lock)

	ENTRY(tnfw_b_clear_lock)
	b	lock_clear
	SET_SIZE(tnfw_b_clear_lock)

	ENTRY(tnfw_b_atomic_swap)
	eieio				# synchronize stores
.L2:
	lwarx	%r5, 0, %r3		# load word with reservation
	stwcx.	%r4, 0, %r3		# store conditional
	bne-	.L2			# repeat if something else stored first
	mr	%r3, %r5		# return value
	blr				# return
	SET_SIZE(tnfw_b_atomic_swap)

#endif	/* lint */
