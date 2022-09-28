/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */

#ifndef _SYS_PCB_H
#define	_SYS_PCB_H

#pragma ident	"@(#)pcb.h	1.8	96/06/18 SMI"

#include <sys/reg.h>
#include <sys/debug/debugreg.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if !defined(_ASM)

typedef struct pcb {
	struct fpu	pcb_fpu;	/* fpu state */
	int		pcb_flags;	/* various state flags */
	dbregset_t 	pcb_dregs;	/* debug registers (HID regs 1,2,5) */
	long		pcb_instr;	/* /proc: instruction at stop */
	long		pcb_reserved;	/* padding for 8byte alignement */
} pcb_t;

#endif /* !defined(_ASM) */

/*
 * Bit definitions for pcb_flags
 */
#define	PCB_DEBUG_EN		0x01  /* debug registers are in use */
#define	PCB_DEBUG_MODIFIED	0x02  /* debug registers are modified (/proc) */
#define	PCB_FPU_INITIALIZED	0x04  /* flag signifying fpu in use */
#define	PCB_INSTR_VALID		0x08  /* value in pcb_instr is valid */
#define	PCB_FPU_STATE_MODIFIED  0x10  /* fpu state is modified (/proc) */
#define	PCB_NORMAL_STEP	0x20	/* normal debugger-requested single-step */
#define	PCB_WATCH_STEP	0x40	/* single-stepping in watchpoint emulation */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PCB_H */
