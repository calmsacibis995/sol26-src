/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)_nfssys.s	1.3	94/07/04 SMI"

	.file	"_nfssys.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	nfssys - nfs-related system calls
 *
 *   Syntax:	int _nfssys(enum nfssys_op opcode, union nfssysargs arg);
 *
 *   called internally by exportfs(), nfs_getfh(), nfssvc(),
 *						(port/sys/nfssys.c)
 */

#include "SYS.h"

	ENTRY(_nfssys)

	SYSTRAP(nfssys)
	SYSCERROR

	RETZ

	SET_SIZE(_nfssys)
