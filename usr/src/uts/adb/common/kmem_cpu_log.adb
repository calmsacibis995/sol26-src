#pragma ident	"@(#)kmem_cpu_log.adb	1.1	96/07/28 SMI"

#include <sys/kmem.h>
#include <sys/kmem_impl.h>

kmem_cpu_log_header
./"lock"
+$<<mutex{OFFSETOK}
+/"current"16t"avail"16t"chunk"16t"hits"n{clh_current,X}{clh_avail,D}{clh_chunk,D}{clh_hits,D}
