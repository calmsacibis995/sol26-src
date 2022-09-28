/*
 *	Copyright (c) 1994, by Sun Microsystems, Inc.
 */
#pragma ident	"@(#)uppc_ml.s	1.8	95/07/19 SMI"

#include <sys/asm_linkage.h>

#if defined(lint) || defined(__lint)
#include <sys/types.h>
#include <sys/time.h>
#else	/* lint */
#include "assym.s"
#endif	/* lint */

#define	inb(PORT, RETREG) \
	addis	RETREG,0,PCIISA_VBASE >> 16; \
	addi	RETREG,RETREG,PORT; \
	eieio; \
	lbz	RETREG,0(RETREG)

#define	outb(PORT, VALUEREG, TMPREG) \
	addis	TMPREG,0,PCIISA_VBASE >> 16; \
	addi	TMPREG,TMPREG,PORT; \
	eieio; \
	stb	VALUEREG,0(TMPREG)

/*
 * uppc_intr_enter() raises the ipl to the level of the current interrupt,
 * and sends EOI to the pics.
 * uppc_intr_enter() returns the new priority level, 
 * or -1 for spurious interrupt 
 *
 *	if (slave pic) {
 *		if ((intno & 7) == 7) {
 *			if (!(inb(SCMD_PORT) & 0x80) {
 *				outb(MCMD_PORT, PIC_NSEOI);
 *				return (-1);
 *			}
 *		}
 *		uppc_setspl(newipl);
 *		outb(MCMD_PORT, PIC_NSEOI);
 *		outb(SCMD_PORT, PIC_NSEOI);
 *	} else {
 *		if ((intno & 7) == 7) {
 *			if (!(inb(SCMD_PORT) & 0x80) 
 *				return (-1);
 *		}
 *		uppc_setspl(newipl);
 *		outb(MCMD_PORT, PIC_NSEOI);
 *	}
 *      return newipl;
 *
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
uppc_intr_enter(int current_ipl, int *vector)
{ return (0); }

#else	/* lint */

	.extern	uppc_iack

	ENTRY_NP(uppc_intr_enter)
	mflr	%r0
	lis	%r5,uppc_iack@ha
	lwz	%r5,uppc_iack@l(%r5)	! pointer to iack register
	lbz	%r5,0(%r5)		! %r5 - intr vector
	stw	%r5,0(%r4)		! return through *vector

	! setup for detecting spurious interrupt reflected to IRQ7 of PIC
	andi.	%r6,%r5,7		! (intno & 7)

	cmpi	%r5,0x8			! if master pic 
	blt+	master_int		! then check for master interrupt

	cmpi	%r6,7			! if intno != IRQ7 of slave
	bne+	slave_int		! then handle interrupt 

	inb(SCMD_PORT, %r6)		! read ISR from slave PIC
	andi.	%r6,%r6,0x80		! if IS bit for IRQ 7 is not set
	beq-	slave_spur		! then recover from slave spurious

slave_int:
	sli	%r6,%r5,3		! vector*8 (for index into autovect below)
	addi	%r6,%r6,AVH_HI_PRI	! adjust autovect pointer by HI_PRI offset
	addis	%r6,%r6,autovect@ha
	lhz	%r3,autovect@l(%r6)	! r3 is now the priority level

	mr	%r11,%r3		! save return value
	bl	uppc_setspl		! load masks for new level

	li	%r6,PIC_NSEOI
	outb(MCMD_PORT, %r6, %r7)	! send EOI to master pic

	outb(SCMD_PORT, %r6, %r7)	! send EOI to slave pic

	mr	%r3,%r11		! restore return value
	mtlr	%r0
	blr

master_int:
	cmpi	%r6,7			! if intno != IRQ7 of master
	bne	m_int			! then handle interrupt 

	inb(MCMD_PORT, %r6)		! read ISR from master PIC
	andi.	%r6,%r6,0x80		! if IS bit for IRQ 7 is not set
	beq-	spurintret		! then return spurious intr

m_int:
	sli	%r6,%r5,3		! vector*8 (for index into autovect below)
	addi	%r6,%r6,AVH_HI_PRI	! adjust autovect pointer by HI_PRI offset
	addis	%r6,%r6,autovect@ha
	lhz	%r3,autovect@l(%r6)	! r3 is now the priority level

	mr	%r11,%r3		! save return value
	bl	uppc_setspl		! load masks for new level

	li	%r6,PIC_NSEOI
	outb(MCMD_PORT, %r6, %r7)	! send EOI to master pic

	mr	%r3,%r11		! restore return value
	mtlr	%r0
	blr

slave_spur:
	li	%r6,PIC_NSEOI
	outb(MCMD_PORT, %r6, %r7)	! send EOI to master pic

spurintret:
	li	%r3,-1			! return spurious
	mtlr	%r0
	blr

	SET_SIZE(uppc_intr_enter)

#endif	/* lint */

/*
 * uppc_intr_exit() restores the old interrupt
 * priority level after processing an interrupt.
 * It is called with interrupts disabled, and does not enable interrupts.
 * The new interrupt level is passed as an arg on the stack.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
uppc_intr_exit(int ipl, int vector)
{}

#else	/* lint */

	ENTRY_NP(uppc_intr_exit)
!	mr	%r3,%r3			! pass first argument to setspl
!	b	uppc_setspl		! load masks for new level
					! let it return to our caller
! WARNING:
! Since uppc_setspl is the next function, just fall through to it.
	SET_SIZE(uppc_intr_exit)

#endif	/* lint */


/*
 * uppc_setspl() loads new interrupt masks into the pics 
 * based on input ipl.
 *
 * NOTE: uppc_intr_enter() assumes %r0 and %r11 is not disturbed!
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
uppc_setspl(int ipl)
{}

#else	/* lint */

	ENTRY_NP(uppc_setspl)

	lis	%r4,pics0@ha
	sli	%r5,%r3,1		! ipl * 2
	la	%r4,pics0@l(%r4)
	addi	%r5,%r5,C_IPLMASK
	lhz	%r7,C_CURMASK(%r4)
	lhzx	%r6,%r5,%r4
	cmp	%r6,%r7
	beq	setpicmasksret

	sth	%r6,C_CURMASK(%r4)	! set C_IPLMASK to new mask

	! program the slave
	andi.	%r8,%r6,0xff
	andi.	%r9,%r7,0xff
	cmp	%r8,%r9			! compare to current mask
	beq      pset_master		! skip if identical

	outb(SIMR_PORT, %r8, %r9)

	! program the master
pset_master:
	rlwinm	%r8,%r6,24,24,31
	rlwinm	%r9,%r7,24,24,31
	cmp	%r8,%r9			! compare to current mask
	beq	picskip			! skip if identical

	outb(MIMR_PORT, %r8, %r9)

picskip:
	inb(MIMR_PORT, %r9)		! read master
					! to allow the pics to settle
					
setpicmasksret:
	blr
	SET_SIZE(uppc_setspl)

#endif	/* lint */

/*
 * uppc_gethrtime() returns high resolution timer value
 *
 * This functionality is NOT platform specific.  gethrtime() is
 * fully implemented in locore.s and not here, thus here we have
 * a stub.
 */
#if defined(lint) || defined(__lint)

hrtime_t
uppc_gethrtime()
{ return (0); }

#else	/* lint */

	ENTRY(uppc_gethrtime)
	blr

	SET_SIZE(uppc_gethrtime)
#endif	/* lint */
