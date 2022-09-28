#include <sys/door.h>

door_node
./"vnode"
.$<<vnode{OFFSETOK}
+/n"target"16t"ulist"16t"pc"16t"data"n{door_target,X}{door_ulist,X}{door_pc,X}{door_data,X}
+/n"index"16t""16t"flags"16t"active"n{door_index,2X}{door_flags,X}{door_active,D}
+/n"servers"n{door_servers,X}{END}
