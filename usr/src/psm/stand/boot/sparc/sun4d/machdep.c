/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */

#ident	"@(#)machdep.c	1.1	96/05/17 SMI" /* From SunOS 4.1.1 */

#include <sys/types.h>
#include <sys/machparam.h>
#include <sys/promif.h>

int pagesize = PAGESIZE;
int vac = 1;
union sunromvec *romp = (union sunromvec *)0xffe81000;

void
fiximp(void)
{
	extern int use_align;

	use_align = 1;
}


void
setup_aux(void)
{
	extern int icache_flush;

	icache_flush = 1;
}

/*
 * Stub for sun4c/sunmmu routines.
 */
void sunm_vac_flush(caddr_t v, u_int nbytes) {}
