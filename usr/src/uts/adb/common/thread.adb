#include <sys/thread.h>

_kthread
.>D;<_>U;1>_;<D=a
./"link"16t"stk"16t"startpc"16t"bound"n{t_link,X}{t_stk,X}{t_startpc,X}{t_bound_cpu,X}
+/"affcnt"16t"bind_cpu"16t"flag"16t"procflag"n{t_affinitycnt,d}8t{t_bind_cpu,d}8t{t_flag,x}8t{t_proc_flag,x}
+/"schedflag"16t"preempt"16t"preempt_lk"16t"state"n{t_schedflag,x}8t{t_preempt,B}8t{t_preempt_lk,B}8t{t_state,X}
+/"pri"16t"epri"16t"pc"16t"sp"n{t_pri,d}8t{t_epri,d}8t{t_pcb.val[0],X}{t_pcb.val[1],X}
+/"wchan0"16t"wchan"16t"sobj_ops"16t"cid"n{t_wchan0,X}{t_wchan,X}{t_sobj_ops,X}{t_cid,X}
+/"clfuncs"16t"cldata"16t"ctx"16t"lofault"n{t_clfuncs,X}{t_cldata,X}{t_ctx,X}{t_lofault,X}
+/"onfault"16t"nofault"16t"swap"16t"lock"n{t_onfault,X}{t_nofault,X}{t_swap,X}{t_lock,B}
+/"delay_cv"16t"cpu"16t"intr"16t"did"n{t_delay_cv,x}8t{t_cpu,X}{t_intr,X}{t_did,D}
+/"tnf_tpdp"16t"tid"16t"alarmid"n{t_tnf_tpdp,X}{t_tid,D}{t_alarmid,X}"realitimer"
+$<<itimerval{OFFSETOK}
+/"itimerid"16t"sigqueue"16t"sig"n{t_itimerid,X}{t_sigqueue,X}{t_sig,2X}
+/"hold"16t""16t"forw"16t"back"n{t_hold,2X}{t_forw,X}{t_back,X}
+/"lwp"16t"procp"16t"next"16t"prev"n{t_lwp,X}{t_procp,X}{t_next,X}{t_prev,X}
+/"trace"16t"why"8t"what"8t"dslot"16t"pollstate"n{t_trace,X}{t_whystop,d}{t_whatstop,d}{t_dslot,D}{t_pollstate,X}
+/"cred"16t"lbolt"16t"sysnum"16t"pctcpu"n{t_cred,X}{t_lbolt,X}{t_sysnum,d}8t{t_pctcpu,x}
+/"lockp"16t"oldspl"16t"pre_sys"16t"disp_queue"n{t_lockp,X}{t_oldspl,x}8t{t_pre_sys,B}8t{t_disp_queue,X}
+/"disp_time"16t"kpri_req"16t"astflag"8t"sigchk"8t"postsys"8t"trapret"n{t_disp_time,D}{t_kpri_req,D}{t_astflag,B}{t_sig_check,B}{t_post_sys,B}{t_trapret,B}
+/"waitrq"16t16t"mstate"16t"rprof"n{t_waitrq,2X}{t_mstate,D}{t_rprof,X}
+/"prioinv"16t"ts"16t"mmuctx"16t"tsd"n{t_prioinv,X}{t_ts,X}{t_mmuctx,X}{t_tsd,X}
+/"stime"16t"door"16t"plockp"n{t_stime,X}{t_door,X}{t_plockp,X}
+/"handoff"16t"schedctl"16t"cpupart"16t"bind_pset"n{t_handoff,X}{t_schedctl,X}{t_cpupart,X}{t_bind_pset,D}{END}
+>D;<U>_;<D>D
