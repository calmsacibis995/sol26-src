
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

#ifndef	_RDB_MACH_H
#define	_RDB_MACH_H

#pragma	ident	"@(#)rdb_mach.h	1.2	96/09/10 SMI"

#include <sys/psw.h>
#include <sys/reg.h>
#define	ERRBIT	CR0_SO
#define	R_PS	R_CR

#include <sys/trap.h>

/*
 * Breakpoint stuff
 */
typedef	unsigned	bptinstr_t;
#define	BPINSTR		BPT_INST


/*
 * PLT section type
 */
#define	PLTSECTT	SHT_NOBITS

#endif
