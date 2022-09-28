/*
 * Copyright 1990 by Sun Microsystems, Inc.
 */

#ident	"@(#)mem_kern.c	1.4	92/07/14 SMI"

/*
 * Memory pixrect (non)creation in kernel
 */

#include <sys/types.h>
#include <sys/pixrect.h>
/* #include "/usr/include/pixrect/pixrect.h" */

extern	int	mem_rop();
extern	int	mem_putcolormap();
extern	int	mem_putattributes();

struct	pixrectops mem_ops = {
	mem_rop,
	mem_putcolormap,
	mem_putattributes,
#ifdef _PR_IOCTL_KERNEL_DEFINED
	0
#endif
};

