/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */

#ifndef _SYS_FRAME_H
#define	_SYS_FRAME_H

#pragma ident	"@(#)frame.h	1.2	94/02/10 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Definition of the PowerPC stack frame (when it is pushed on the stack).
 * This is only used in kadb's _setjmp/_longjmp.
 */

struct frame {
	struct frame	*fr_savfp;	/* saved frame pointer */
	int	fr_savpc;		/* saved program counter */
	int	fr_cr;			/* saved condition reg. */
	int	fr_nonvols[20];		/* r2,r13-r31 */
};

/*
 * Structure definition for minimum stack frame.
 */
struct minframe {
	struct minframe	*fr_savfp;	/* saved frame pointer */
	int	fr_savpc;		/* saved program counter */
};

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FRAME_H */
