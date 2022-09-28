
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
#pragma ident	"@(#)regs.c	1.2	96/09/10 SMI"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/param.h>
#include <sys/reg.h>

#include "rdb.h"

static void
disp_reg_line(struct ps_prochandle * ph, prstatus_t * prst,
	char * r1, int ind1, char * r2, int ind2)
{
	char	str1[MAXPATHLEN];
	char	str2[MAXPATHLEN];
	strcpy(str1, print_address_ps(ph, prst->pr_reg[ind1],
		FLG_PAP_NOHEXNAME));
	strcpy(str2, print_address_ps(ph, prst->pr_reg[ind2],
		FLG_PAP_NOHEXNAME));
	printf("%8s: 0x%08x %-16s %8s: 0x%08x %-16s\n",
		r1, prst->pr_reg[ind1], str1,
		r2, prst->pr_reg[ind2], str2);
}

retc_t
display_all_regs(struct ps_prochandle *ph)
{
	prstatus_t	prstatus;
	if (ioctl(ph->pp_fd, PIOCSTATUS, &prstatus) == -1) {
		perror("dar: PIOCSTATUS");
		return (RET_FAILED);
	}
	printf("registers:\n");
	disp_reg_line(ph, &prstatus, "r0", R_R0, "r19", R_R19);
	disp_reg_line(ph, &prstatus, "r1", R_R1, "r20", R_R20);
	disp_reg_line(ph, &prstatus, "r2", R_R2, "r21", R_R21);
	disp_reg_line(ph, &prstatus, "r3", R_R3, "r22", R_R22);
	disp_reg_line(ph, &prstatus, "r4", R_R4, "r23", R_R23);
	disp_reg_line(ph, &prstatus, "r5", R_R5, "r24", R_R24);
	disp_reg_line(ph, &prstatus, "r6", R_R6, "r25", R_R25);
	disp_reg_line(ph, &prstatus, "r7", R_R7, "r26", R_R26);
	disp_reg_line(ph, &prstatus, "r8", R_R8, "r27", R_R27);
	disp_reg_line(ph, &prstatus, "r9", R_R9, "r28", R_R28);
	disp_reg_line(ph, &prstatus, "r10", R_R10, "r29", R_R29);
	disp_reg_line(ph, &prstatus, "r11", R_R11, "r30", R_R30);
	disp_reg_line(ph, &prstatus, "r12", R_R12, "r31", R_R31);
	disp_reg_line(ph, &prstatus, "r13", R_R13, "cr", R_CR);
	disp_reg_line(ph, &prstatus, "r14", R_R14, "lr", R_LR);
	disp_reg_line(ph, &prstatus, "r15", R_R15, "pc", R_PC);
	disp_reg_line(ph, &prstatus, "r16", R_R16, "msr", R_MSR);
	disp_reg_line(ph, &prstatus, "r17", R_R17, "ctr", R_CTR);
	disp_reg_line(ph, &prstatus, "r18", R_R18, "xer", R_XER);
	return (RET_OK);
}
