#include <sys/param.h>
#include <sys/types.h>
#include <sys/regset.h>
#include <vm/as.h>

as
.>D;<_>U;1>_;<D=a
./"contents (mutex)"
.$<<mutex{OFFSETOK}
+/"FLAGS"8t"vbits"8t"cv"8t"hat             hrm"nB{OFFSETOK}{a_vbits,B}{a_cv,x}{a_hat,X}{a_hrm,X}
+/"seglast"n{a_cache,X}n"lock (rwlock)"
+$<<rwlock{OFFSETOK}
+/"segs"16t"size"16t"tail"n{a_segs,X}{a_size,X}{a_tail,X}
+/"nsegs"16t"lrep"16t"hilevel"16t"updatedir"n{a_nsegs,D}{a_lrep,b}8t{a_hilevel,b}8t{a_updatedir,b}
+/"objectdir"16t"sizedir"16t"wpage"16t"nwpage"n{a_objectdir,X}{a_sizedir,D}{a_wpage,X}{a_nwpage,D}{END}
+>D;<U>_;<D>D
