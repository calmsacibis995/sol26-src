#include <sys/procfs.h>
#include <sys/vnode.h>
#include <fs/proc/prdata.h>

prcommon
.>D;1>_;<D>D
./"mutex"n{prc_mutex,2X}
+/"wait"8t"flags"16t"opens"16t"writers"n{prc_wait,x}{prc_flags,x}{prc_opens,D}{prc_writers,D}
+/"proc"16t"pid"16t"slot"n{prc_proc,X}{prc_pid,D}{prc_slot,D}
+/"thread"16t"tid"16t"tslot"n{prc_thread,X}{prc_tid,D}{prc_tslot,D}
+/"pollhead"
+$<<pollhead{OFFSETOK}{END}
+>D;0>_;<D>D
