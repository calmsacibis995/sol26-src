#include <sys/types.h>
#include <sys/t_lock.h>
#include <vm/page.h>

page
./"p_vnode"8t"p_hash"8t"p_vpnext"n{p_vnode,X}{p_hash,X}{p_vpnext,X}
./"p_vpprev"8t"p_next"8t"p_prev"n{p_vpprev,X}{p_next,X}{p_prev,X}
./"p_offset"8t"p_selock"n{p_offset,XX}{p_selock,X}
./"p_lckcnt"8t"p_cowcnt"8t"p_cv"n{p_lckcnt,x}{p_cowcnt,x}{p_cv,x}
./"p_io_cv"8t"p_iolock_state"n{p_io_cv,x}{p_iolock_state,B}
./"p_fsdata"8t"p_state"n{p_fsdata,B}{p_state,B}{END}

