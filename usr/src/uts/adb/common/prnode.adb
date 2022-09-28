#include <sys/procfs.h>
#include <sys/vnode.h>
#include <fs/proc/prdata.h>

prnode
.>D;1>_;<D>D
./"next"16t"flags"16t"mutex"n{pr_next,X}{pr_flags,X}{pr_mutex,2X}
+/"type"16t"mode"16t"ino"16t"hatid"n{pr_type,D}{pr_mode,X}{pr_ino,X}{pr_hatid,X}
+/"common"16t"pcommon"16t"parent"16t"files"n{pr_common,X}{pr_pcommon,X}{pr_parent,X}{pr_files,X}
+/"index"16t"pidfile"16t"object"n{pr_index,D}{pr_pidfile,X}{pr_object,X}n"pr_vnode:"
+$<<vnode{OFFSETOK}
.>D;0>_;<D>D
