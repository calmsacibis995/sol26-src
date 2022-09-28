/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#if !defined(PPCDIS_REGS_H)
#define	PPCDIS_REGS_H

#pragma ident	"@(#)ppcdis_regs.h	1.5	95/10/20 SMI"
#pragma ident "@(#)ppcdis_regs.h 1.4	93/10/09 vla"

/*
 * PowerPC Disassembler - Definitions for Special Purpose/Condition Registers
 *
 * This file contains PowerPC typically used Register and Bit Assignments
 *
 * NOTE 1:	Special register mnemonics taken from pg. 74, App. B "Assembler
 *		Extended Mnemonics", "PowerPC Operating Environment
 *		Architecture".
 *
 */

/*
 * Special Purpose Register Assignments
 *
 * WARNING! some weirdness here - the SPR number (ten bit field) is split into
 * two five-bit fields, which are swapped in the actual instruction; i.e.,
 * the high-order five bits appear in positions 16-20, and the low-order
 * five bits in positions 11-15 of the instruction!
 */
/*
 *			SPR Register Encodings
 *
 *	Register     	SPR    Decimal	 Register
 *        Name         Value    Value	 Description
 * ============================================================
 */
#if defined(MC98601)
#define	PPC_SPR_MQ	 0	/* 0	MQ Register */
#endif	/* defined(MC98601) */

#define	PPC_SPR_XER	 32	/* 1	Fixed-Point Exception Register */

#if defined(MC98601)
#define	PPC_SPR_RTCU	 128	/* 4	*/
#define	PPC_SPR_RTCL	 160	/* 5	*/
#define	PPC_SPR_DECu	 192	/* 6	Decrementer, user mode */
#endif	/* defined(MC98601) */

#define	PPC_SPR_LR	 256	/* 8	Link Register */
#define	PPC_SPR_CTR	 288	/* 9	Count Register */
#define	PPC_SPR_DSISR	 576	/* 18	Data Storage Interrupt Status Reg. */
#define	PPC_SPR_DAR	 608	/* 19	Data Address Register */
#define	PPC_SPR_DEC	 704	/* 22	Decrementer */
#define	PPC_SPR_SDR1	 800	/* 25	Storage Description Register 1 */
#define	PPC_SPR_SRR0	 832	/* 26	Save/Restore Register 0 */
#define	PPC_SPR_SRR1	 864	/* 27	Save/Restore Register 1 */
#define	PPC_SPR_G0	 520	/* 272	Special Purpose Register G0 */
#define	PPC_SPR_G1	 552	/* 273	Special Purpose Register G1 */
#define	PPC_SPR_G2	 584	/* 274	Special Purpose Register G2 */
#define	PPC_SPR_G3	 616	/* 275	Special Purpose Register G3 */
#define	PPC_SPR_ASR	 776	/* 280	Address Space Register */
#define	PPC_SPR_EAR	 840	/* 282	External Access Register */

/*
 * The time base instructions (on the 603 and later models) use different
 * register numbers depending upon the direction of data transfer.
 */
#if defined(MC98603)
#define	PPC_SPR_FTBL	 392	/* 268	From Time Base (Lower) */
#define	PPC_SPR_FTBU	 424	/* 269	From Time Base (Upper) */
#define	PPC_SPR_TTBL	 904	/* 284	To Time Base (Lower) */
#define	PPC_SPR_TTBU	 936	/* 285	To Time Base (Upper) */
#endif	/* defined(MC98603) */
#if defined(MC98601)
#define	PPC_SPR_TBL	 904	/* 284	Time Base (Lower) */
#define	PPC_SPR_TBU	 936	/* 285	Time Base (Upper) */
#define	PPC_SPR_PVR	1000	/* 287	Processor Version Register */
#endif	/* defined(MC98601) */

#define	PPC_SPR_BAT0U	 528	/* 528	IBAT Registers, Upper (528 + 2n) */
#define	PPC_SPR_BAT0L	 560	/* 529	IBAT Registers, Lower (529 + 2n) */
#define	PPC_SPR_BAT1U	 592	/* 530	IBAT Registers, Upper (530 + 2n) */
#define	PPC_SPR_BAT1L	 624	/* 531	IBAT Registers, Lower (531 + 2n) */
#define	PPC_SPR_BAT2U	 656	/* 532	IBAT Registers, Upper (532 + 2n) */
#define	PPC_SPR_BAT2L	 688	/* 533	IBAT Registers, Lower (533 + 2n) */
#define	PPC_SPR_BAT3U	 720	/* 534	IBAT Registers, Upper (534 + 2n) */
#define	PPC_SPR_BAT3L	 752	/* 535	IBAT Registers, Lower (535 + 2n) */
#define	PPC_SPR_DBAT0L	 784	/* 536	DBAT Registers, Upper (536 + 2n) */
#define	PPC_SPR_DBAT0U	 816	/* 537	DBAT Registers, Lower (537 + 2n) */
#define	PPC_SPR_DBAT1L	 848	/* 536	DBAT Registers, Upper (536 + 2n) */
#define	PPC_SPR_DBAT1U	 880	/* 537	DBAT Registers, Lower (537 + 2n) */
#define	PPC_SPR_DBAT2L	 912	/* 536	DBAT Registers, Upper (536 + 2n) */
#define	PPC_SPR_DBAT2U	 944	/* 537	DBAT Registers, Lower (537 + 2n) */
#define	PPC_SPR_DBAT3L	 976	/* 536	DBAT Registers, Upper (536 + 2n) */
#define	PPC_SPR_DBAT3U	 1008	/* 537	DBAT Registers, Lower (537 + 2n) */

#if defined(MC98601) || defined(MC98603)
#define	PPC_SPR_HID0	 543	/* 1008	Checkstop Register		*/
#define	PPC_SPR_IABR	 607	/* 1010 Instruction brkpt register	*/
#endif	/* defined(MC98601) || defined(MC98603) */

#if defined(MC98601)
#define	PPC_SPR_HID1	 575	/* 1009	Debug Mode Register		*/
#define	PPC_SPR_HID2	 607	/* 1010	IABR				*/
#define	PPC_SPR_HID5	 703	/* 1013	DABR				*/
#define	PPC_SPR_HID15	1023	/* 1023	PIR				*/
#endif	/* defined(MC98601) */

#define	PPC_SPR_DMISS	 542	/* 976 data tlb miss address		*/
#define	PPC_SPR_DCMP	 574	/* 977 data tlb miss compare		*/
#define	PPC_SPR_HASH1	 606	/* 978 pteg1 address			*/
#define	PPC_SPR_HASH2	 638	/* 979 pteg2 address			*/
#define	PPC_SPR_IMISS	 670	/* 980 instruction tlb miss address	*/
#define	PPC_SPR_ICMP	 702	/* 981 instruction tlb miss compare	*/
#define	PPC_SPR_RPA	 734	/* 982 TLB replacement entry		*/

/*
 * Condition Register Assignments
 *
 * The Condition Register (CR) is a 32-bit register subdivided into
 * eight four-bit fields, named CR0 ... CR7.
 *
 * CR0 may be used to record information about the result of a fixed point
 * operation; CR1 is correspondingly used for floating-point operations.
 */
#define	PPC_FX_CR	0
#define	PPC_FP_CR	1

#endif			/* PPCDIS_REGS_H */
