/*
 * Copyright (c) 1993, by Sun Microsystems, Inc.
 */

.ident	"@(#)__signotify.s	1.3	93/12/13 SMI"

/* unpublished system call for libposix4 -- __signotify		*/
/* int _signotify (int cmd, siginfo_t *siginfo,		*/
/*					signotify_id_t *sn_id);	*/

	.file	"__signotify.s"

#include "SYS.h"

	ENTRY(__signotify)
	SYSTRAP(signotify)
	SYSCERROR
	RET

	SET_SIZE(__signotify)
