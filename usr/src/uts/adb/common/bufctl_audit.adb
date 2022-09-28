#pragma ident	"@(#)bufctl_audit.adb	1.3	96/07/28 SMI"

#include <sys/kmem.h>
#include <sys/kmem_impl.h>

kmem_bufctl_audit
.>b
<_>U;1>_
<b/n"next"16t"addr"16t"slab"16t"cache"n{bc_next,X}{bc_addr,X}{bc_slab,X}{bc_cache,X}
+/n"timestamp"48t"thread"n{bc_timestamp,2X}{bc_thread,X}
+/n"lastlog"16t"contents"16t"stackdepth"n{bc_lastlog,X}{bc_contents,X}{bc_depth,X}
+>s
{*bc_depth,<b}>d
<U>_
<s,<d/np
