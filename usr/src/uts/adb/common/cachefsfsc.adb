#include <sys/fs/cachefs_fs.h>
#include <sys/fs/cachefs_fscache.h>

fscache
./"cfsid"n{fs_cfsid,2X}
+/"flags"16t"fscdirvp"16t"attrdvp"n{fs_flags,X}{fs_fscdirvp,X}{fs_fsattrdir,X}
+/"infovp"16t"cachep"n{fs_infovp,X}{fs_cache,X}
+/"info"n{fs_info,10X}
+/"cfs vfsp"16t"back vfsp"16t"rootvp"n{fs_cfsvfsp,X}{fs_backvfsp,X}{fs_rootvp,X}
+/"ref"16t"cncnt"16t"consttype"n{fs_ref,D}{fs_cnodecnt,D}{fs_consttype,X}
+/"cfsops"n{fs_cfsops,X}
+/"acregmin"16t"acregmax"n{fs_acregmin,D}{fs_acregmax,X}
+/"acdirmin"16t"acdirmax"n{fs_acdirmin,D}{fs_acdirmax,X}
+/"next"n{fs_next,X}
+/"q head"16t"q tail"n{fs_workq.wq_head,X}{fs_workq.wq_tail,X}
+/"q len"16t"q threadcnt"n{fs_workq.wq_length,X}{fs_workq.wq_thread_count,X}
+/"q max"16t"q halt"n{fs_workq.wq_max_len,X}
+/"dlogvp"16t"dlogoff"16t"dlogseq"n{fs_dlogfile,X}{fs_dlogoff,X}{fs_dlogseq,D}
+/"dmapvp"16t"dmapoff"16t"dmapsize"n{fs_dmapfile,X}{fs_dmapoff,X}{fs_dmapsize,X}
+$<<mutex{OFFSETOK}
+$<<mutex{OFFSETOK}
+/"idlcnt"16t"idleclean"n{fs_idlecnt,D}{fs_idleclean,D}
+/"idlfront"n{fs_idlefront,X}
+$<<mutex{OFFSETOK}
+/"connected"16t"transition"n{fs_cdconnected,D}{fs_cdtransition,D}
+/"daemonpid"16t"threadcnt"n{fs_cddaemonid,D}{fs_cdrefcnt,D}
+/"inumtrns"16t"inumsize"n{fs_inum_trans,X}{fs_inum_size,D}
+/"mntpt"16t"host"16t"backnm"n{fs_mntpt,X}{fs_hostname,X}{fs_backfsname,X}{END}
