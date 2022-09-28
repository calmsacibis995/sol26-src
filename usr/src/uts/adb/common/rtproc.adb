#include <sys/param.h>
#include <sys/types.h>
#include <sys/rt.h>

rtproc
./"pquantum"16t"timeleft"16t"pri"8t"flags"8t"tp"n{rt_pquantum,D}{rt_timeleft,D}{rt_pri,d}{rt_flags,x}{rt_tp,X}
+/"pstatp"16t"pprip"16t"pflagp"n{rt_pstatp,X}{rt_pprip,X}{rt_pflagp,X}
+/"next"16t"prev"n{rt_next,X}{rt_prev,X}{END}
