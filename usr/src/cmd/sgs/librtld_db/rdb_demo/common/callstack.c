
/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided this notice is preserved and that due credit is given
 * to Sun Microsystems, Inc.  The name of Sun Microsystems, Inc. may
 * not be used to endorse or promote products derived from this
 * software without specific prior written permission.  This software
 * is provided ``as is'' without express or implied warranty.
 */
#pragma ident	"@(#)callstack.c	1.1	96/09/11 SMI"


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/reg.h>
#include <sys/frame.h>
#include <sys/procfs.h>

#include "rdb.h"

#if defined(sparc)
#define	FRAME_PTR_INDEX R_FP
#endif

#if defined(i386)
#define	FRAME_PTR_INDEX R_FP
#endif

#if defined(__ppc)
#define	FRAME_PTR_INDEX R_R1
#endif

/*
 * Architecture neutral routine to display the callstack.
 */
void
CallStack(struct ps_prochandle * ph)
{
	prstatus_t	prstatus;
	int		pfd = ph->pp_fd;
	greg_t		fp;
	struct frame	frm;
	char *		symstr;

	if (ioctl(pfd, PIOCSTATUS, &prstatus) == -1)
		perr("cs: PIOCSTATUS");

	symstr = print_address_ps(ph, prstatus.pr_reg[R_PC], FLG_PAP_SONAME);
	printf(" 0x%08x:%-17s\n", prstatus.pr_reg[R_PC], symstr);

	fp = prstatus.pr_reg[FRAME_PTR_INDEX];

	while (fp) {
		if (ps_pread(ph, fp, (char *)&frm,
		    sizeof (struct frame)) != PS_OK) {
			printf("stack trace: bad frame pointer: 0x%x\n", fp);
			return;
		}
		if (frm.fr_savpc) {
			symstr = print_address_ps(ph, frm.fr_savpc,
				FLG_PAP_SONAME);
			printf(" 0x%08x:%-17s\n", frm.fr_savpc, symstr);
		}
		fp = (greg_t)frm.fr_savfp;
	};
}
