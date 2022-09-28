#include <sys/door_data.h>

door_data
./"args"
.$<<door_arg{OFFSETOK}
+/n"caller"16t"servers"16t"active"16t"pool"n{d_caller,X}{d_servers,X}{d_active,X}{d_pool,X}
+/n"sp"16t"buf"16t"bufsize"16t"error"n{d_sp,X}{d_buf,X}{d_bufsize,D}{d_error,D}
+/n"fpp_size"16t"fpp"16t"upcall"8t"nores"n{d_fpp_size,D}{d_fpp,X}{d_upcall,b}{d_noresults,b}
+/n"oflw"8t"flag"8t"cv"n{d_overflow,b}{d_flag,b}{d_cv,x}{END}
