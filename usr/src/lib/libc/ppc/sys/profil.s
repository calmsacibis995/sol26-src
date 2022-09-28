/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)profil.s	1.3	94/07/04 SMI"

	.file	"profil.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	profil - execution time profile
 *
 *   Syntax:	void profil(unsigned short *buff, unsigned int bufsiz,
 *			unsigned int offset, unsigned int scale);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(profil,function)

#include "SYS.h"

	ENTRY(profil)

	SYSTRAP(profil)

	RET

	SET_SIZE(profil)
