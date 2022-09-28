#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/tiuser.h>
#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <nfs/nfs.h>
#include <nfs/nfs_clnt.h>

mntinfo
./"lock"
.$<<mutex{OFFSETOK}
+/"servers"16t"curr_serv"16t"failover_cv"8t"readers"n{mi_servers,X}{mi_curr_serv,X}{mi_failover_cv,x}8t{mi_readers,D}
+/"rootvp"16t"flags"16t"tsize"16t"stsize"n{mi_rootvp,X}{mi_flags,X}{mi_tsize,X}{mi_stsize,X}
+/"timeo"16t"retrans"n{mi_timeo,D}{mi_retrans,X}
+/"acregmin"16t"acregmax"16t"acdirmin"16t"acdirmax"n{mi_acregmin,U}{mi_acregmax,U}{mi_acdirmin,U}{mi_acdirmax,U}
+/"mi_timers"
+$<<rpctimer{OFFSETOK}
+$<<rpctimer{OFFSETOK}
+$<<rpctimer{OFFSETOK}
+$<<rpctimer{OFFSETOK}
+/"curread"16t"curwrite"n{mi_curread,D}{mi_curwrite,D}
+/"async_reqs"16t"async_tail"16t"async_req_cv"n{mi_async_reqs,X}{mi_async_tail,X}{mi_async_reqs_cv,x}
+/"threads"16t"max_threads"8t"async_cv"8t"count"n{mi_threads,d}8t{mi_max_threads,d}8t{mi_async_cv,x}8t{mi_async_count,D}
+/"async_lock"
+$<<mutex{OFFSETOK}
+/"pathconf"16t"prog"16t"vers"n{mi_pathconf,X}{mi_prog,D}{mi_vers,D}
+/"rfsnames"16t"reqs"n{mi_rfsnames,X}{mi_reqs,X}
+/"call_type"16t"ss_call_type"16t"timer_type"n{mi_call_type,X}{mi_ss_call_type,X}{mi_timer_type,X}
+/"printftime"n{mi_printftime,D}{END}
+/"aclnames"16t"aclreqs"n{mi_aclnames,X}{mi_aclreqs,X}
+/"acl_call_type"16t"ss_acl_call_typ"16t"acl_timer_type"n{mi_acl_call_type,X}{mi_acl_ss_call_type,X}{mi_acl_timer_type,X}
+/"noresponse"16t"failover"16t"remap"n{mi_noresponse,D}{mi_failover,D}{mi_remap,D}
