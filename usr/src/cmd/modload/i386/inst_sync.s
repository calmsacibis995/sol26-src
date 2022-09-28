	.ident	"@(#)inst_sync.s 1.1     93/09/03 SMI"

/
/ System call:
/		int inst_sync(char *pathname, int flags);
/

#include <sys/asm_linkage.h>
#include <sys/trap.h>
#include <sys/syscall.h>

	.file	"inst_sync.s"

	ENTRY(inst_sync)

	movl	$SYS_inst_sync,%eax
	lcall	$0x7,$0
	jc	.error
	ret
.error:
	movl	%eax,errno
	movl	$-1,%eax
	ret

	SET_SIZE(inst_sync)
