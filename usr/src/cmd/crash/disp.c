#ident	"@(#)disp.c	1.5	95/12/08 SMI"		/* SVr4.0 1.1.1.1 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *		Copyright (C) 1991  Sun Microsystems, Inc
 *			All rights reserved.
 *		Notice of copyright on this source code
 *		product does not indicate publication.
 *
 *		RESTRICTED RIGHTS LEGEND:
 *   Use, duplication, or disclosure by the Government is subject
 *   to restrictions as set forth in subparagraph (c)(1)(ii) of
 *   the Rights in Technical Data and Computer Software clause at
 *   DFARS 52.227-7013 and in similar clauses in the FAR and NASA
 *   FAR Supplement.
 */

/*
 * This file contains code for the crash functions:  dispq
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/var.h>
#include <sys/cpuvar.h>
#include <sys/disp.h>
#include <sys/elf.h>
#include "crash.h"

static void free_cpus(cpu_t *cpu_list);
static void prdispq();

/*
 * Display the dispatch tables.  There is a dispatch table for each cpu
 * struct, plus one for threads that are not bound to a cpu (typically
 * real-time threads, I think).  However, this latter only exists on
 * MP systems, so if we can't find symbol disp_kp_queue we skip that
 * one.
 */
int
getdispq()
{
	int	 slot = -1;
	int	 c;
	long	 arg1 = -1;
	long	 arg2 = -1;
	char	*disp_kp_queue_addr; /* kernel address of disp_kp_queue */
	disp_t	 disp_kp_queue;	   /* copy of disp_kp_queue */
	dispq_t	*disp_kp_tbl;	   /* ptr to copy of disp_kp_queue's tbl */
	char	*cpu_list_addr;	   /* kernel address of "cpu_list" variable */
	cpu_t	*first_cpu_addr;   /* kernel address of first cpu struct */
	cpu_t	*cpu_addr;	   /* kernel address of a cpu struct */
	cpu_t	*cpu_list = NULL;  /* head of list of copied cpu structs */
	cpu_t	*cpu;		   /* pointer to a copy of a cpu struct */
	cpu_t  **link;		   /* pointer to link to current cpu struct */
	dispq_t	*disp_tbl_addr;    /* kernel address of a dispatch table */
	dispq_t	*disp_tbl;	   /* pointer to a copy of a dispatch table */
	size_t	 tbl_size =	   /* size of a dispatch table */
		vbuf.v_nglobpris * sizeof (dispq_t);
	char	*dispqhdg =
	    "SLOT     CPU    DQ_FIRST     DQ_LAST     RUNNABLE COUNT\n\n";

	/* Arrange to abort the command on out-of-memory. */
	set_alloc_err_func(out_of_memory);

	/* Parse the arguments. */
	optind = 1;
	while ((c = getopt(argcnt, args, "w:")) != EOF) {
		switch (c) {
			case 'w' :	redirect();
					break;
			default  :	longjmp(syn, 0);
		}
	}

	/* Look up all symbols before we start allocating memory. */
	disp_kp_queue_addr = try_sym2addr("disp_kp_queue");
	cpu_list_addr	   = sym2addr("cpu_list");

	/* Get the dispatch queue for threads not bound to a cpu. */
	if (disp_kp_queue_addr) {
		disp_kp_tbl = gc_malloc(temp, tbl_size);
		GET_VAR(disp_kp_queue, disp_kp_queue_addr,
			"unbound dispatch info");
		GET_BUF(disp_kp_tbl, tbl_size, disp_kp_queue.disp_q,
			"unbound dispatch table");
	} else
		disp_kp_tbl = NULL;

	/* Get address of cpu list pointer, and address of first cpu. */
	GET_VAR(first_cpu_addr, cpu_list_addr, "cpu list head pointer");

	/*
	 * While there are more active cpus ...
	 * Note that the cpu list in kernel memory is circular, so we
	 * check whether we're at the end by seeing whether we are
	 * pointing to the first.  The test (link == &cpu_list) allows
	 * us through the first time.
	 */
	cpu_addr = first_cpu_addr;
	link = &cpu_list;
	while ((cpu_addr != first_cpu_addr) || (link == &cpu_list)) {

		/* Get a copy of the cpu struct, and add it to the list. */
		cpu = gc_malloc(temp, sizeof (cpu_t));
		*link = cpu;
		GET_BUF(cpu, sizeof (cpu_t), cpu_addr, "cpu struct");
		cpu_addr = cpu->cpu_next;
		link = &cpu->cpu_next;
		*link = NULL;

		/* Get the dispatch table for that cpu struct. */
		disp_tbl_addr = cpu->cpu_disp.disp_q;
		cpu->cpu_disp.disp_q = disp_tbl = gc_malloc(temp, tbl_size);
		GET_BUF(disp_tbl, tbl_size, disp_tbl_addr, "dispatch table");
	}

	/* Display the heading. */
	fprintf(fp, "%s", dispqhdg);

	/* If there are args ... */
	if (args[optind]) {

		/* Display a select set of priority levels. */
		do {
			getargs(vbuf.v_nglobpris, &arg1, &arg2, 0);
			if (arg1 == -1)
				continue;
			if (arg2 != -1)
				for (slot = arg1; slot <= arg2; slot++)
					prdispq(slot, TRUE,
						disp_kp_tbl, cpu_list);
			else {
				if (arg1 < vbuf.v_nglobpris)
					slot = arg1;
				prdispq(slot, TRUE, disp_kp_tbl, cpu_list);
			}
			slot = arg1 = arg2 = -1;
		} while (args[++optind]);

	} else {

		/* Display all priority levels. */
		for (slot = 0; slot < vbuf.v_nglobpris; slot++)
			prdispq(slot, FALSE, disp_kp_tbl, cpu_list);
	}

	return (0);
}


/*
 * Print the dispatch queue entries in all dispatch tables.
 */
static void
prdispq(
	int	  slot,
	int	  force,   /* flag: forces display of empty slots */
	dispq_t  *disp_kp_tbl,
	cpu_t    *cpu_list)
{
	dispq_t  *dispq;

	/* First show the unbound threads at that level, if non-empty. */
	if (disp_kp_tbl) {
		dispq = &disp_kp_tbl[slot];
		if (dispq->dq_first || force) {
			fprintf(fp, "%4d            %8x    %8x          %4d\n",
				slot, dispq->dq_first, dispq->dq_last,
				dispq->dq_sruncnt);
		}
	}

	/* Now show the bound threads at that level, if non-empty. */
	while (cpu_list) {

		dispq = &cpu_list->cpu_disp.disp_q[slot];
		if (dispq->dq_first || force) {
			fprintf(fp, "%4d   %5d    %8x    %8x          %4d\n",
				slot, cpu_list->cpu_id,
				dispq->dq_first, dispq->dq_last,
				dispq->dq_sruncnt);
		}
		cpu_list = cpu_list->cpu_next;
	}
}
