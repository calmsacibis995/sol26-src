#pragma ident "@(#)vm_ppcmmu.c 1.2	95/11/13 SMI"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * This file contains code for the crash functions that deal with
 * the srmmu as used in sun4m/sun4d kernel architecture machines.
 */

#include <stdio.h>
#include <sys/types.h>
#include <vm/hat.h>
#if	0
#include <vm/hat_srmmu.h>
#endif
#include <sys/pte.h>
#include "crash.h"


gethat()
{
	char c;
	unsigned long addr = -1;
	int all = 0;

	optind = 1;
	while ((c = getopt(argcnt, args, "aw:")) != EOF) {
		switch (c) {
			case 'a' :	all = 1;
					break;
			case 'w' :	redirect();
					break;
			default  :	longjmp(syn, 0);
		}
	}
	fprintf(fp, "OP       AS       NEXT\n");
	if (args[optind]) {
		do {
			if ((addr = strcon(args[optind++], 'h')) == -1)
				continue;
			prhat(all, addr);
		} while (args[optind]);
	} else longjmp(syn, 0);
}

prhat(all, addr)
int all;
unsigned long addr;
{
	struct hat hbuf;
#if	0	/* NO equiv stuff for ppc */
	struct srmmu srbuf;
	register struct srmmu *srmmu;
#endif

	do {
		readmem(addr, 1, -1, (char *)&hbuf, sizeof (hbuf), "hat entry");
		fprintf(fp, "%8-x %8-x %8-x\n",
			hbuf.hat_op, hbuf.hat_as, hbuf.hat_next);
#if	0
		srmmu = (struct srmmu *)hbuf.hat_data[0];
		readmem((unsigned)srmmu, 1, -1, (char *)&srbuf, sizeof (srbuf),
						"srmmu entry");

		fprintf(fp, "\tSRMMU: L1VADDR	L2PTS	L3PTS\n");
		fprintf(fp, "CTX	CPUSRAN\n");
		fprintf(fp, "\t       %8-x %8-x %8-x\n",
			srbuf.srmmu_l1pt, srbuf.srmmu_l2pts,
			srbuf.srmmu_l3pts);
		fprintf(fp, "%8-x %8-x\n",
			srbuf.srmmu_ctx, srbuf.srmmu_cpusran);
#endif
		addr = (unsigned)hbuf.hat_next;
	} while (all && (addr != NULL));
}

getctx()
{
#if	0
	char c;
	unsigned long addr = -1;

	optind = 1;
	while ((c = getopt(argcnt, args, "w:")) != EOF) {
		switch (c) {
			case 'w' :	redirect();
					break;
			default  :	longjmp(syn, 0);
		}
	}
	fprintf(fp, "NUM      AS\n");
	if (args[optind]) {
		do {
			if ((addr = strcon(args[optind++], 'h')) == -1)
				continue;
			prctx(addr);
		} while (args[optind]);
	} else longjmp(syn, 0);
#endif
	error("This version of crash doesn't understand 'ctx'\n");
}

prctx(addr)
unsigned long addr;
{
#if	0
	struct ctx cbuf;

	readmem(addr, 1, -1, (char *)&cbuf, sizeof (cbuf), "ctx entry");
	fprintf(fp, "%8-x %8-x\n", cbuf.c_num, cbuf.c_as);
#endif
}

/*
 * Crash keeps some happy little functions around for examining
 * various bits of the SunMMU hardware in interesting ways. So
 * we replace them for the SPARC reference MMU by stubs that complain.
 *
 * It's not clear that this is the best way to handle this problem.
 * For example, if we're running crash on a sun4m, we can only look
 * at sun4m crash dumps.  Sigh.
 *
 * It would be nice if we could assemble crash's ops vector on the
 * basis of the namelist and dumpfile architecture so that we could,
 * for example, examine a sun4c crashdump on a sun4m.
 */
getsmgrp()
{
	error("This version of crash doesn't understand 'smgrp'\n");
}

getpmgrp()
{
	error("This version of crash doesn't understand 'pmgrp'\n");
}

getsment()
{
	error("This version of crash doesn't understand 'sment'\n");
}

getpte()
{
	char c;
	unsigned long addr = -1;

	optind = 1;
	while ((c = getopt(argcnt, args, "w:")) != EOF) {
		switch (c) {
			case 'w' :	redirect();
					break;
			default  :	longjmp(syn, 0);
		}
	}
	fprintf(fp, "PROT     CACHE    VALID     REF      MOD      PFNUM\n");
	if (args[optind]) {
		do {
			if ((addr = strcon(args[optind++], 'h')) == -1)
				continue;
			prpte(addr);
		} while (args[optind]);
	} else longjmp(syn, 0);
}

prpte(addr)
unsigned long addr;
{
	struct pte pbuf;

	readmem(addr, 1, -1, (char *)&pbuf, sizeof (pbuf), "pte entry");
	fprintf(fp, "%8-x %8-x %8-x %8-x %8-x %8-d\n",
		pbuf.pte_pp, pbuf.pte_wimg, pbuf.pte_valid,
		pbuf.pte_referenced, pbuf.pte_modified, pbuf.pte_ppn);
}
