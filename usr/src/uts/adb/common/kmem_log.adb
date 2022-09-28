#pragma ident	"@(#)kmem_log.adb	1.1	96/07/28 SMI"

#include <sys/kmem.h>
#include <sys/kmem_impl.h>

kmem_log_header
.>k
<_>U;1>_
<k/n"lock"
+$<<mutex{OFFSETOK}
+/"base"16t"free"16t"chunksize"16t"chunks"n{lh_base,X}{lh_free,X}{lh_chunksize,D}{lh_nchunks,D}
+/"head"16t"tail"16t"hits"n{lh_head,D}{lh_tail,D}{lh_hits,D}
+>k
*ncpus>n
$<kmem_cpu_log.nxt{OFFSETOK}
<U>_
