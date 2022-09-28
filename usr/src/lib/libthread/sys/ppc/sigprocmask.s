	.ident "@(#)sigprocmask.s 1.2	94/09/09 SMI"
#include <sys/asm_linkage.h>
#include "SYS.h"

	.file "sigprocmask.s"
/*
 * void
 * __sigprocmask(how, set, oset)
 */

	ENTRY(__sigprocmask);
	SYSTRAP(sigprocmask)
	RET
	SET_SIZE(__sigprocmask)
