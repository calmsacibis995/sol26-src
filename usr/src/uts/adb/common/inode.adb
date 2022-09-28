#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <vm/seg.h>
#include <sys/fs/ufs_inode.h>

inode
./"forw"16t"back"n{i_chain[0],X}{i_chain[1],X}
+$<<dino{OFFSETOK}
+$<<vnode{OFFSETOK}
./"devvp"16t"flag"8t"maj"8t"min"8t"ino"n{i_devvp,X}{i_flag,x}{i_dev,2x}{i_number,D}
+/"diroff"16t"ufsvfs"16t"dquot"n{i_diroff,X}{i_ufsvfs,X}{i_dquot,X}n"rwlock"
+$<<rwlock{OFFSETOK}
+/"contents"
.$<<rwlock{OFFSETOK}
+/"tlock"
.$<<mutex{OFFSETOK}
+/"nextr hi"16t"nextr lo"16t"freef"16t"freeb"n{i_nextr,XX}{i_freef,X}{i_freeb,X}
+/"vcode"16t"mapcnt"16t"map"16t"rdev"n{i_vcode,D}{i_mapcnt,D}{i_map,X}{i_rdev,X}
+/"delaylen"16t"delayoff hi"16t"delayoff lo"n{i_delaylen,D}{i_delayoff,XX}
+/"nextrio hi"16t"nextrio lo"16t"writes"n{i_nextrio,XX}{i_writes,D}
+/"wrcv"n{i_wrcv,x}
+/"owner"16t"doff hi"16t"doff low"16t"acl"n{i_owner,X}{i_doff,2X}{i_ufs_acl,X}{END}
