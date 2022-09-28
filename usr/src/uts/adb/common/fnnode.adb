#include <rpc/types.h>
#include <sys/time.h>
#include <sys/t_lock.h>
#include <sys/vfs.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <sys/tiuser.h>
#include <sys/vnode.h>
#include <sys/fs/autofs.h>

fnnode
./"name"16t"symlink"n{fn_name,X}{fn_symlink,X}n
+/"namelen"16t"symlinklen"n{fn_namelen,D}{fn_symlinklen,D}n
+/"linkcnt"16t"mode"16t"uid"16t"gid"16t"error"n{fn_linkcnt,U}{fn_mode,O}{fn_uid,U}{fn_gid,U}{fn_error,D}n
+/"nodeid"16t"offset"16t"flags"16t"size"n{fn_nodeid,D}{fn_offset,X}{fn_flags,X}{fn_size,U}n
+$<<vnode{OFFSETOK}
+/"parent"16t"next"16t"dirents"n{fn_parent,X}{fn_next,X}{fn_dirents,X}n
+/"trigger"16t"alp"16t"cred"n{fn_trigger,X}{fn_alp,X}{fn_cred,X}n
+/"rwlock"
.$<<rwlock{OFFSETOK}
+/"lock"
.$<<mutex{OFFSETOK}
+/"atime.sec"16t"atime.usec"16t"mtime.sec"16t"mtime.usec"n{fn_atime,2X}{fn_mtime,2X}n
+/"ctime.sec"16t"ctime.usec"n{fn_ctime,2X}{END}
