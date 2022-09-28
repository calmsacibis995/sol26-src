/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)_lwp_self.s	1.3	94/07/04 SMI"

	.file	"_lwp_self.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	_lwp_self - get LWP identifier
 *
 *   Syntax:	lwp_id_t _lwp_self(void);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(_lwp_self,function)

#include "SYS.h"

	ENTRY(_lwp_self)

	SYSTRAP(lwp_self)
	SYSCERROR

	RET

	SET_SIZE(_lwp_self)
