#include <sys/fs/cachefs_fs.h>

cnode
./"flags"n{c_flags,X}
+/"frontvp"16t"backvp"n{c_frontvp,X}{c_backvp,X}
+/"acldirvp"16t"size"n{c_acldirvp,X}{c_size,2X}
+/"filegrp"16t"fileno"16t"invals"n{c_filegrp,X}{c_id.cid_fileno,2X}{c_invals,D}
+/"cid_flags"n{c_id.cid_flags,X}
+/"usage"n{c_usage,D}
+$<<vnode{OFFSETOK}
+$<<cachefsmeta{OFFSETOK}
+/"error"n{c_error,D}
+$<<mutex{OFFSETOK}
+$<<rwlock{OFFSETOK}
+/"unldirvp"16t"unlname"16t"unlcred"n{c_unldvp,X}{c_unlname,X}{c_unlcred,X}
+/"nio"16t"ioflags"n{c_nio,D}{c_ioflags,X}
+/"cred"n{c_cred,X}{END}
