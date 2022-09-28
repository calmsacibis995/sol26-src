#include <sys/param.h>
#include <sys/types.h>
#include <sys/proc.h>

proc
.>D;<_>U;1>_;<D=a
./"exec"16t"as"16t"lockp"n{p_exec,X}{p_as,X}{p_lockp,X}n
+>X
<X+4/"crlock"
.$<<mutex{OFFSETOK}
+/"cred"16t"swapcnt"16t"stat"8t"wcode"16t"wdata"n{p_cred,X}{p_swapcnt,D}{p_stat,B}{p_wcode,B}{p_wdata,X}
+/"ppid"16t"link"16t"parent"16t"sessp"n{p_ppid,U}{p_link,X}{p_parent,X}{p_sessp,X}
+/"child"16t"sibling"16t"psibling"16t"netxofkin"n{p_child,X}{p_sibling,X}{p_psibling,X}{p_nextofkin,X}
+/"next"16t"prev"16t"child_ns"16t"sibling_ns"n{p_next,X}{p_prev,X}{p_child_ns,X}{p_sibling_ns,X}
+/"orphan"16t"nextorph"16t"pglink"16t"ppglink"n{p_orphan,X}{p_nextorph,X}{p_pglink,X}{p_ppglink,X}
+/"pidp"16t"pgidp"16t"cv"8t"flg_cv"n{p_pidp,X}{p_pgidp,X}{p_cv,x}{p_flag_cv,x}
+/"lwpexit"16t"holdlwps"16t"flag"16t"utime"n{p_lwpexit,x}16t{p_holdlwps,x}16t{p_flag,X}{p_utime,X}
+/"stime"16t"cutime"16t"cstime"16t"segacct"n{p_stime,X}{p_cutime,X}{p_cstime,X}{p_segacct,X}
+/"brkbase"16t"brksize"16t"sig"n{p_brkbase,X}{p_brksize,X}{p_sig,2X}
+/"ignore"16t""16t"siginfo"n{p_ignore,2X}{p_siginfo,2X}
+/"sigqueue"16t"stopsig"8t"lwptotal"8t"lwpcnt"8t8t"lwprcnt"n{p_sigqueue,X}{p_stopsig,b}{p_lwptotal,D}{p_lwpcnt,D}{p_lwprcnt,D}
+/"lwpblocked"16t"zombcnt"16t"zomblist"16t"tlist"n{p_lwpblocked,D}{p_zombcnt,D}{p_zomblist,X}{p_tlist,X}
+/"sigmask"16t""16t"fltmask"16t"trace"n{p_sigmask,2X}{p_fltmask,X}{p_trace,X}
+/"plist"16t"warea"16t"nwarea"n{p_plist,X}{p_warea,X}{p_nwarea,D}
+/"wpage"16t"mapcnt"16t"rlink"n{p_wpage,X}{p_mapcnt,D}{p_rlink,X}
+/"srwchan_cv"16t"stksize"16t"aslwptp"16t"p_aio"n{p_srwchan_cv,x}16t{p_stksize,X}{p_aslwptp,X}{p_aio,X}
+/"notifsigs"16t16t"notifcv"16t"alarmid"n{p_notifsigs,2X}{p_notifcv,x}16t{p_alarmid,X}{END}
+>D;<U>_;<D>D
