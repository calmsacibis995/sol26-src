/*
 * Copyright (c) 1987 by Sun Microsystems, Inc.
 */

#ifndef _SYS_FRAME_H
#define	_SYS_FRAME_H

#pragma ident	"@(#)frame.h	1.4	94/05/13 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * In the Intel world, a stack frame looks like this:
 *
 * %fp0->|				 |
 *	 |-------------------------------|
 *	 |  Args to next subroutine	 |
 *	 |-------------------------------|-\
 * %sp0->|  One word struct-ret address	 | |
 *	 |-------------------------------|  > minimum stack frame (8 bytes)
 * %fp1->|  Previous frame pointer (%fp0)| |
 *	 |-------------------------------|-/
 *	 |  Local variables		 |
 * %sp1->|-------------------------------|
 */

struct frame {
	greg_t fr_savfp;
	greg_t fr_savpc;
};

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FRAME_H */
