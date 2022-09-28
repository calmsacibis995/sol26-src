#pragma ident	"@(#)module.adb	1.4	96/07/28 SMI"

#include <sys/kobj.h>

module
.>K
./"alloced"n{total_allocated,D}
+/"shdrs"16t"symhdr"16t"strhdr"n{shdrs,X}{symhdr,X}{strhdr,X}
+/"depends_on"n{depends_on,X}
+/"symsize"16t"symspace"n{symsize,U}{symspace,X}
+/"flags"n{flags,X}
+/"textsize"16t"datasize"n{text_size,U}{data_size,U}
+/"text"16t"data"n{text,X}{data,X}
+/"symtbl_sec"16t"symtbl"16t"strings"n{symtbl_section,U}{symtbl,X}{strings,X}
+/"hashsize"16t"buckets"16t"chains"n{hashsize,U}{buckets,X}{chains,X}
+/"nsyms"16t"bss_align"16t"bss_size"n{nsyms,U}{bss_align,U}{bss_size,U}
+/"bss"n{bss,U}
+/"mod_lst_hdp"16t"mod_lst_tlp"n{head,X}{tail,X}
+/"filename"n;{*filename,<K}/s
