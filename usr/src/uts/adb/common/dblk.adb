#pragma ident	"@(#)dblk.adb	1.5	96/07/28 SMI"

#include <sys/types.h>
#include <sys/stream.h>

datab
./"cache"16t"base"16t"limit"n{db_cache,X}{db_base,X}{db_lim,X}
+/"ref"8t"type"8t"flags"8t"uioflag"n{db_ref,V}{db_type,B}{db_flags,B}{db_struioflag,B}
+/"lock"
+$<<mutex{OFFSETOK}
+/"mblk"16t"freefunc"16t"lastfreefunc"n{db_mblk,X}{db_free,p}{db_lastfree,p}
+/"uiobase"16t"uiolim"16t"uioptr"n{db_struiobase,X}{db_struiolim,X}{db_struioptr,X}{END}
