#include <sys/param.h>
#include <sys/types.h>
#include <sys/pcb.h>
#include <sys/user.h>

pcb
#ifdef __ppc
./"flags"16t"instr"n{pcb_flags,X}{pcb_instr,X}{END}
#endif /* __ppc */
#ifdef i386
./"flags"16t"gpfault"16t"instr"n{pcb_flags,X}{pcb_gpfault,X}{pcb_instr,b}{END}
#endif /* m68k */
#ifdef sparc
./"flags"16t"t0addr"16t"instr"n{pcb_flags,X}{pcb_trap0addr,X}{pcb_instr,X}
+/n"regstat"16t"step"16t"tracepc"n{pcb_xregstat,U}{pcb_step,U}{pcb_tracepc,X}{END}
#endif /* sparc */
