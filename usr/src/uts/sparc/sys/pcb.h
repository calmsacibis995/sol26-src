/*
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 */

#ifndef _SYS_PCB_H
#define	_SYS_PCB_H

#pragma ident	"@(#)pcb.h	1.24	96/09/05 SMI"

#include <sys/regset.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Sun software process control block
 */

#ifndef _ASM
typedef struct pcb {
	int	pcb_flags;	/* various state flags */
	caddr_t	pcb_trap0addr;	/* addr of user level trap 0 handler */
	long	pcb_instr;	/* /proc: instruction at stop */
	enum { XREGNONE = 0, XREGPRESENT, XREGMODIFIED }
		pcb_xregstat;	/* state of contents of pcb_xregs */
	struct	rwindow pcb_xregs; /* locals+ins fetched/set via /proc */
	enum { STEP_NONE = 0, STEP_REQUESTED, STEP_ACTIVE, STEP_WASACTIVE }
		pcb_step;	/* used while single-stepping */
	caddr_t	pcb_tracepc;	/* used while single-stepping */
} pcb_t;
#endif /* ! _ASM */

/* pcb_flags */
#define	INSTR_VALID	0x02	/* value in pcb_instr is valid (/proc) */
#define	NORMAL_STEP	0x04	/* normal debugger requested single-step */
#define	WATCH_STEP	0x08	/* single-stepping in watchpoint emulation */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PCB_H */
