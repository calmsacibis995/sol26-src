#include	<sys/types.h>
#include	<sys/thread.h>
#include	<sys/lwp.h>

_klwp
.>D;<_>U;1>_;<D>D
./"oldcontext"16t"ap"16t"errno"n{lwp_oldcontext,X}{lwp_ap,X}{lwp_errno,D}
+/"error"16t"eosys"16t"argsaved"16t"watchtrap"n{lwp_error,B}16t{lwp_eosys,B}16t{lwp_argsaved,B}16t{lwp_watchtrap,B}n"arg(s):"
+/{lwp_arg,8X}
#if	defined(i386)
+/"regs"16t"qsav.pc"16t"qsav.sp"n{lwp_regs,X}{lwp_qsav,6X}
#elif	defined(sparc)
+/"regs"16t"qsav.pc"16t"qsav.sp"n{lwp_regs,X}{lwp_qsav,2X}
#elif	defined(__ppc)
+/"regs"16t"qsav.pc"16t"qsav.sp"n{lwp_regs,X}{lwp_qsav,22X}
#else
#error Don't know how to dump lwp_qsav on this architecture
#endif
+/"cursig"16t"curflt"16t"sysabrt"16t"asleep"n{lwp_cursig,B}16t{lwp_curflt,B}16t{lwp_sysabort,B}16t{lwp_asleep,B}n"sigaltstack:"
+$<<sigaltstack{OFFSETOK}
+/"curinfo"n{lwp_curinfo,X}n"siginfo:"
+$<<ksiginfo{OFFSETOK}
+/"sigoldmask"n{lwp_sigoldmask,2X}n"watch[]:"
+/"wpaddr"16t"wpsize"16t"wpcode"16t"wppc"n{lwp_watch[0].wpaddr,X}{lwp_watch[0].wpsize,D}{lwp_watch[0].wpcode,D}{lwp_watch[0].wppc,X}{lwp_watch[1].wpaddr,X}{lwp_watch[1].wpsize,D}{lwp_watch[1].wpcode,D}{lwp_watch[1].wppc,X}{lwp_watch[2].wpaddr,X}{lwp_watch[2].wpsize,D}{lwp_watch[2].wpcode,D}{lwp_watch[2].wppc,X}
+/"pr_base"16t"pr_size"16t"pr_off"16t"pr_scale"n{lwp_prof.pr_base,X}{lwp_prof.pr_size,X}{lwp_prof.pr_off,X}{lwp_prof.pr_scale,X}
#ifdef i386
+/"mstate"n{lwp_mstate,27X}
#elif	defined(sparc) || defined(__ppc)
+/"mstate"n{lwp_mstate,28X}
#else
#error Don't know how to dump lwp_mstate on this architecture
#endif
+/"ru"n{lwp_ru,12X}
+/"lastfault"16t"lastfaddr"n{lwp_lastfault,D}{lwp_lastfaddr,X}n"timer[]:"
+/"interval.sec"16t"interval.usec"16t"value.sec"16t"value.usec"n{lwp_timer[0].it_interval.tv_sec,X}{lwp_timer[0].it_interval.tv_usec,X}{lwp_timer[0].it_value.tv_sec,X}{lwp_timer[0].it_value.tv_usec,X}{lwp_timer[1].it_interval.tv_sec,X}{lwp_timer[1].it_interval.tv_usec,X}{lwp_timer[1].it_value.tv_sec,X}{lwp_timer[1].it_value.tv_usec,X}{lwp_timer[2].it_interval.tv_sec,X}{lwp_timer[2].it_interval.tv_usec,X}{lwp_timer[2].it_value.tv_sec,X}{lwp_timer[2].it_value.tv_usec,X}
+/"oweupc"16t"state"16t"nostop"16t"cv"n{lwp_oweupc,B}16t{lwp_state,B}16t{lwp_nostop,d}16t{lwp_cv,x}
+/"utime"16t"stime"16t"thread"16t"procp"n{lwp_utime,X}{lwp_stime,X}{lwp_thread,X}{lwp_procp,X}{END}
+>D;<U>_;<D>D
