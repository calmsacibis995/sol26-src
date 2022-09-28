/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.
 */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any		*/
/*	actual or intended publication of such source code.	*/

#ifndef	_SYS_RMPLATTER_H
#define	_SYS_RMPLATTER_H

#pragma ident	"@(#)rm_platter.h	1.7	95/09/29 SMI"

#include <sys/types.h>
#include <sys/segment.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef	struct rm_platter {
	char	rm_code[1024];
	ushort	rm_debug;
	ushort	rm_gdt_lim;		/* stuff for lgdt */
	struct	seg_desc *rm_gdt_base;
	ushort	rm_filler2;		/* till I am sure that pragma works */
	ushort	rm_idt_lim;		/* stuff for lidt */
	struct  gate_desc *rm_idt_base;
	u_int	rm_pdbr;		/* cr3 value */
	u_int	rm_cpu;			/* easy way to know which CPU we are */
	u_int   rm_x86feature;		/* X86 supported features */
	u_int   rm_cr4;			/* cr4 value on cpu0 */
} rm_platter_t;

/*
 * cpu tables put within a single structure all the tables which need to be
 * allocated when a CPU starts up. Makes it more memory effficient and easier
 * to allocate/release
 */
typedef struct cpu_tables {
	/* 1st level page directory */
	pte_t	ct_pagedir[MMU_STD_PAGESIZE / sizeof (pte_t)];
	char	ct_stack[4096];		/* default stack for tss and startup */
	struct	seg_desc ct_gdt[GDTSZ];
	struct	tss386 ct_tss;
} cpu_tables_t;

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_RMPLATTER_H */
