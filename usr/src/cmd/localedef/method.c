/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)method.c 1.23	96/08/15  SMI"

/*
 * COPYRIGHT NOTICE
 * 
 * This source code is designated as Restricted Confidential Information
 * and is subject to special restrictions in a confidential disclosure
 * agreement between HP, IBM, SUN, NOVELL and OSF.  Do not distribute
 * this source code outside your company without OSF's specific written
 * approval.  This source code, and all copies and derivative works
 * thereof, must be returned or destroyed at request. You must retain
 * this notice on any copies which you make.
 * 
 * (c) Copyright 1990, 1991, 1992, 1993 OPEN SOFTWARE FOUNDATION, INC.
 * ALL RIGHTS RESERVED 
 */
/*
 * OSF/1 1.2
 */
#if !defined(lint) && !defined(_NOIDENT)
static char rcsid[] = "@(#)$RCSfile: method.c,v $ $Revision: 1.5.2.9 $ (OSF) $Date: 1992/03/25 22:30:14 $";
#endif
/*
 * COMPONENT_NAME: (CMDLOC) Locale Database Commands
 *
 * FUNCTIONS:
 *
 *
 * (C) COPYRIGHT International Business Machines Corp. 1991
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 * 1.2  com/cmd/nls/method.c, cmdnls, bos320 6/1/91 14:47:04
 */

#include "method.h"
#include <stdio.h>

/*
extern __mbftowc_dense_pck();
extern __mblen_dense_pck();
extern __mblen_dense_eucjp();
extern __mbstowcs_dense_pck();
extern __mbstowcs_dense_eucjp();
extern __mbtowc_dense_pck();
extern __mbtowc_dense_eucjp();
extern __wcstombs_dense_pck();
extern __wcstombs_dense_eucjp();
extern __wcswidth_dense_pck();
extern __wcswidth_dense_eucjp();
extern __wctomb_dense_pck();
extern __wctomb_dense_eucjp();
extern __wcwidth_dense_pck();
extern __wcwidth_dense_eucjp();
*/

extern __mbftowc_euc();
extern __mbftowc_sb();
extern __fgetwc_sb();
extern __mblen_sb();
extern __mblen_gen();
extern __mbstowcs_sb();
extern __mbtowc_sb();
extern __wcstombs_sb();
extern __wcswidth_sb();
extern __wctomb_sb();
extern __wcwidth_sb();
extern __collate_init();
extern __wctype_std();
extern __ctype_init();
extern __iswctype_std();
extern __strcoll_sb();
extern __strcoll_std();
extern __strxfrm_sb();
extern __strxfrm_std();
extern __towlower_std();
extern __towupper_std();
extern __wcscoll_sb();
extern __wcscoll_std();
extern __wcsxfrm_sb();
extern __wcsxfrm_std();
extern __localeconv_std();
extern __nl_langinfo_std();
extern __strfmon_std();
extern __catclose_std();
extern __catgets_std();
extern __strftime_std();
extern __strptime_std();
extern __getdate_std();
extern __wcsftime_std();
extern __regcomp_std();
extern __regerror_std();
extern __regexec_std();
extern __regfree_std();
extern __fnmatch_std();
extern __fnmatch_sb();
extern __charmap_init();
extern __locale_init();
extern __monetary_init();
extern __numeric_init();
extern __messages_init();
extern __time_init();
extern __eucpctowc_gen();
extern __wctoeucpc_gen();
extern __trwctype_std();
extern __wctrans_std();
extern __towctrans_std();
extern __fgetwc_euc();
extern __mbftowc_euc();
extern __mbstowcs_euc();
extern __mbtowc_euc();
extern __wcstombs_euc();
extern __wctomb_euc();
extern __iswctype_bc();
extern __towlower_bc();
extern __towupper_bc();
extern __wcscoll_bc();
extern __wcsxfrm_bc();
extern __fgetwc_dense();
extern __iswctype_sb();
extern __mbftowc_dense();
extern __mbstowcs_dense();
extern __mbtowc_dense();
extern __wcstombs_dense();
extern __wctomb_dense();
extern __wcswidth_euc();
extern __wcswidth_dense();
extern __wcwidth_euc();
extern __wcwidth_dense();
extern __towctrans_bc();
/*
extern __charmap_destructor();
extern __ctype_destructor();
extern __locale_destructor();
extern __monetary_destructor();
extern __numeric_destructor();
extern __resp_destructor();
extern __time_destructor();
extern __collate_destructor();
*/


/*
 * Define the normal place to search for methods.  The default would be
 * in the shared libc (/usr/shlib/libc.so), but it might be nice to move the
 * SJIS and EUC/JP methods into separate shared libraries (just to keep libc's
 * code size down to a reasonable level
 */
#define LIBC		"/usr/lib/libc.so.1"
#define LIBSJIS		"/usr/lib/libc.so.1"
#define LIBEUCJP	"/usr/lib/libc.so.1"

/*
 * Define the standard package names.  Right now libc is it.
 * The NULL is for the extensible method support
 */
#define PKGLIST		"libc", "libc", "libc", NULL


static method_t std_methods_tbl[LAST_METHOD+1]={
{ "charmap.mbftowc", 		/* EUC method */
 __mbftowc_euc,   __mbftowc_euc,   __mbftowc_euc,  0,
"__mbftowc_euc", "__mbftowc_euc", "__mbftowc_euc", 0,
PKGLIST,
LIBC, LIBSJIS, LIBEUCJP, 0,
"int %s(_LC_charmap_t*, char*, wchar_t*, int (*)(), int *)" },

{ "charmap.fgetwc",		/* EUC method */
 __fgetwc_euc,   __fgetwc_euc,   __fgetwc_euc,  0,
"__fgetwc_euc", "__fgetwc_euc", "__fgetwc_euc", 0,
PKGLIST,
LIBC, LIBSJIS, LIBEUCJP, 0,
"wint_t %s(_LC_charmap_t*, FILE *)" },

{ "charmap.eucpctowc",
 __eucpctowc_gen,   __eucpctowc_gen,   __eucpctowc_gen,  0,
"__eucpctowc_gen", "__eucpctowc_gen", "__eucpctowc_gen", 0,
PKGLIST,
LIBC, LIBSJIS, LIBEUCJP, 0,
"wchar_t %s(_LC_charmap_t *, wchar_t)"},

{ "charmap.wctoeucpc", 
 __wctoeucpc_gen,   __wctoeucpc_gen,   __wctoeucpc_gen,  0,
"__wctoeucpc_gen", "__wctoeucpc_gen", "__wctoeucpc_gen", 0,
PKGLIST,
LIBC, LIBSJIS, LIBEUCJP, 0,
"wchar_t %s(_LC_charmap_t *, wchar_t)"},

{ "charmap.init",  
 __charmap_init,   __charmap_init,   __charmap_init,  0,
"__charmap_init", "__charmap_init", "__charmap_init", 0,
PKGLIST,
LIBC, LIBSJIS, LIBEUCJP, 0,
"_LC_charmap_t *%s(_LC_locale_t*)"},

{ "charmap.destructor",
0, 0, 0, 0,
0, 0, 0, 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
0},
/*
 __charmap_destructor,   __charmap_destructor,   __charmap_destructor, 0,
"__charmap_destructor", "__charmap_destructor", "__charmap_destructor", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"int %s(_LC_locale_t*)"},
*/

{ "charmap.mblen",
 __mblen_sb,   __mblen_gen,   __mblen_gen,   0, 
"__mblen_sb", "__mblen_gen", "__mblen_gen", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"int %s(_LC_charmap_t*, const char*, size_t)" },

{ "charmap.mbstowcs",		/* EUC method */
 __mbstowcs_euc,   __mbstowcs_euc,   __mbstowcs_euc,  0,
"__mbstowcs_euc", "__mbstowcs_euc", "__mbstowcs_euc", 0,
PKGLIST,
LIBC, LIBSJIS, LIBEUCJP, 0,
"size_t %s(_LC_charmap_t*, wchar_t*,const char*,size_t)" },

{ "charmap.mbtowc", 		/* EUC method */
 __mbtowc_euc,   __mbtowc_euc,   __mbtowc_euc,   __mbtowc_euc,
"__mbtowc_euc", "__mbtowc_euc", "__mbtowc_euc", "__mbtowc_euc",
PKGLIST,
LIBC, LIBSJIS, LIBEUCJP, LIBC,
"int %s(_LC_charmap_t*, wchar_t*,const char*,size_t)" },

{ "charmap.nl_langinfo",
0, 0, 0, 0,
0, 0, 0, 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
0 },


/* Filler (was wcsid) */
{ 0,
0,0,0,0,
0, 0, 0, 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
0},


{ "charmap.wcstombs", 			/* EUC method */
 __wcstombs_euc,   __wcstombs_euc,   __wcstombs_euc,  0,
"__wcstombs_euc", "__wcstombs_euc", "__wcstombs_euc", 0,
PKGLIST,
LIBC, LIBSJIS, LIBEUCJP, LIBC,
"size_t %s(_LC_charmap_t*, char*,const wchar_t*,size_t)"},

{ "charmap.wcswidth",			/* EUC method */
 __wcswidth_euc,   __wcswidth_euc,   __wcswidth_euc,  0,
"__wcswidth_euc", "__wcswidth_euc", "__wcswidth_euc", 0,
PKGLIST,
LIBC, LIBSJIS, LIBEUCJP, LIBC,
"int %s(_LC_charmap_t*, const wchar_t *, size_t)" },

{ "charmap.wctomb",			/* EUC method */
 __wctomb_euc,   __wctomb_euc,   __wctomb_euc,  0,
"__wctomb_euc", "__wctomb_euc", "__wctomb_euc", 0,
PKGLIST,
LIBC, LIBSJIS, LIBEUCJP, LIBC,
"int %s(_LC_charmap_t*, char*, wchar_t)" },

{ "charmap.wcwidth",			/* EUC method */
 __wcwidth_euc,   __wcwidth_euc,   __wcwidth_euc,  0, 
"__wcwidth_euc", "__wcwidth_euc", "__wcwidth_euc", 0,
PKGLIST,
LIBC, LIBSJIS, LIBEUCJP, LIBC,
"int %s(_LC_charmap_t*, const wchar_t)" },

{ "collate.fnmatch",
 __fnmatch_sb,   __fnmatch_std,   __fnmatch_std,  0,
"__fnmatch_sb", "__fnmatch_std", "__fnmatch_std", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"int %s(_LC_collate_t*, const char*,const char*,const char*,int)" },

{ "collate.regcomp",
 __regcomp_std,   __regcomp_std,   __regcomp_std,  0,
"__regcomp_std", "__regcomp_std", "__regcomp_std", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"int %s(_LC_collate_t*, regex_t*,const char*,int)" },

{ "collate.regerror",
__regerror_std, __regerror_std, __regerror_std, 0,
"__regerror_std", "__regerror_std", "__regerror_std", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"size_t %s(_LC_collate_t*, int,const regex_t*,char*,size_t)" },

{ "collate.regexec",
__regexec_std, __regexec_std, __regexec_std, 0,
"__regexec_std", "__regexec_std", "__regexec_std", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"int %s(_LC_collate_t*, const regex_t*,const char*,size_t,regmatch_t*,int)" },

{ "collate.regfree",
__regfree_std, __regfree_std, __regfree_std, 0,
"__regfree_std", "__regfree_std", "__regfree_std", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"void %s(_LC_collate_t*, regex_t*)" },

{ "collate.strcoll",
__strcoll_sb, __strcoll_std, __strcoll_std, 0,
"__strcoll_sb", "__strcoll_std", "__strcoll_std", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"int %s(_LC_collate_t*, const char*,const char*)" },

{ "collate.strxfrm",
__strxfrm_sb, __strxfrm_std, __strxfrm_std, 0,
"__strxfrm_sb", "__strxfrm_std", "__strxfrm_std", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"size_t %s(_LC_collate_t*, char*,const char*,size_t)" },

{ "collate.wcscoll",
 __wcscoll_std,   __wcscoll_std,   __wcscoll_std,  0,
"__wcscoll_std", "__wcscoll_std", "__wcscoll_std", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"int %s(_LC_collate_t*, const wchar_t*,const wchar_t*)" },

{ "collate.wcsxfrm",
 __wcsxfrm_std,   __wcsxfrm_std,   __wcsxfrm_std,  0,
"__wcsxfrm_std", "__wcsxfrm_std", "__wcsxfrm_std", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"size_t %s(_LC_collate_t*, wchar_t*,const wchar_t*,size_t)" },

{ "collate.init", 
__collate_init, __collate_init, __collate_init, 0,
"__collate_init", "__collate_init", "__collate_init", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"_LC_collate_t *%s(_LC_locale_t*)" },

{ "ctype.wctype",    
__wctype_std, __wctype_std, __wctype_std,  0,
"__wctype_std",  "__wctype_std", "__wctype_std",  0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"wctype_t %s(_LC_ctype_t*, const char *)" },

{ "ctype.init", 
__ctype_init, __ctype_init, __ctype_init, 0,
"__ctype_init", "__ctype_init", "__ctype_init", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"_LC_ctype_t *%s(_LC_locale_t*)" },

{ "ctype.iswctype",     
 __iswctype_bc,   __iswctype_bc,   __iswctype_bc,  0,
"__iswctype_bc", "__iswctype_bc", "__iswctype_bc", 0,
PKGLIST,
LIBC, LIBSJIS, LIBEUCJP, 0,
"int %s(_LC_ctype_t*, wchar_t, wctype_t)" },

{ "ctype.towlower", 		/* EUC method */
 __towlower_bc,   __towlower_bc,   __towlower_bc,  0,
"__towlower_bc", "__towlower_bc", "__towlower_bc", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"wint_t %s(_LC_ctype_t*, wint_t)" },

{ "ctype.towupper", 
 __towupper_bc,   __towupper_bc,   __towupper_bc,  0,
"__towupper_bc", "__towupper_bc", "__towupper_bc", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"wint_t %s(_LC_ctype_t*, wint_t)" },

{ "locale.init", 
__locale_init, __locale_init, __locale_init, 0,
"__locale_init", "__locale_init", "__locale_init", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"_LC_locale_t *%s(_LC_locale_t*)" },

{ "locale.localeconv", 
__localeconv_std, __localeconv_std, __localeconv_std, 0,
"__localeconv_std", "__localeconv_std", "__localeconv_std", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"struct lconv *%s()" },

{ "locale.nl_langinfo",
__nl_langinfo_std, __nl_langinfo_std, __nl_langinfo_std, 0,
"__nl_langinfo_std", "__nl_langinfo_std", "__nl_langinfo_std", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"char *%s()" },

{ "monetary.init", 
__monetary_init, __monetary_init, __monetary_init, 0,
"__monetary_init", "__monetary_init", "__monetary_init", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"_LC_monetary_t *%s(_LC_locale_t*)" },

{ "monetary.nl_langinfo",
0, 0, 0, 0,
0, 0, 0, 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
0 },

{ "monetary.strfmon",
__strfmon_std, __strfmon_std, __strfmon_std, 0,
"__strfmon_std", "__strfmon_std", "__strfmon_std", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"ssize_t %s(_LC_monetary_t*, char*,size_t,const char *,va_list)" },

/*
 * The msg.* methods are unimplemented place-holders
 */

{ "msg.catclose", 0,0,0,0, 0,0,0,0, PKGLIST, LIBC,LIBSJIS,LIBEUCJP,0, 0 },

{ "msg.catgets",  0,0,0,0, 0,0,0,0, PKGLIST, LIBC,LIBSJIS,LIBEUCJP,0, 0 },

{ "msg.compress",0,0,0,0,  0,0,0,0, PKGLIST, LIBC,LIBSJIS,LIBEUCJP,0, 0 },
{ "msg.decompress",0,0,0,0,  0,0,0,0, PKGLIST, LIBC,LIBSJIS,LIBEUCJP,0, 0 },
{ "msg.end_compress",0,0,0,0,  0,0,0,0, PKGLIST, LIBC,LIBSJIS,LIBEUCJP,0, 0 },
{ "msg.init", 0,0,0,0,  0,0,0,0, PKGLIST, LIBC,LIBSJIS,LIBEUCJP,0, 0 },
{ "msg.start_compress",0,0,0,0,  0,0,0,0, PKGLIST, LIBC,LIBSJIS,LIBEUCJP,0, 0 },

{ "numeric.init", 
__numeric_init, __numeric_init, __numeric_init, 0,
"__numeric_init", "__numeric_init", "__numeric_init", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"_LC_numeric_t *%s(_LC_locale_t*)" },

{ "numeric.nl_langinfo", 
0, 0, 0, 0,
0, 0, 0, 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
0 },

{ "messages.init", 
__messages_init, __messages_init, __messages_init, 0,
"__messages_init", "__messages_init", "__messages_init", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"_LC_messages_t *%s(_LC_locale_t*)" },

{ "messages.nl_langinfo",
0, 0, 0, 0,
0, 0, 0, 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
0 },

/* was rp_match */
{ 0,
0, 0, 0, 0,
0, 0, 0, 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"int %s()" },

{ "time.init", 
__time_init, __time_init, __time_init, 0,
"__time_init", "__time_init", "__time_init", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"_LC_time_t *%s(_LC_locale_t*)" },

{ "time.nl_langinfo",    
0, 0, 0, 0,
0, 0, 0, 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
0 },

{ "time.strftime",
__strftime_std, __strftime_std, __strftime_std, 0, 
"__strftime_std", "__strftime_std", "__strftime_std", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"size_t %s(_LC_time_t*, char*,size_t,const char*,const struct tm*)" },

{ "time.strptime", 
__strptime_std, __strptime_std, __strptime_std, 0,
"__strptime_std", "__strptime_std", "__strptime_std", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"char * %s(_LC_time_t*, const char*,const char*,struct tm*)" },

{ "time.wcsftime", 
__wcsftime_std, __wcsftime_std, __wcsftime_std, 0,
"__wcsftime_std", "__wcsftime_std", "__wcsftime_std", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"size_t %s(_LC_time_t*, wchar_t*,size_t,const char*,const struct tm*)" },

{ "time.getdate",
__getdate_std, __getdate_std, __getdate_std, 0,
"__getdate_std", "__getdate_std", "__getdate_std", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"struct tm *%s(_LC_time_t *, const char *)" },

{ "ctype.destructor",
0, 0, 0, 0,
0, 0, 0, 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
0},
/*
__ctype_destructor, __ctype_destructor, __ctype_destructor, 0,
"__ctype_destructor", "__ctype_destructor", "__ctype_destructor", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"int %s(_LC_locale_t*)"},
*/

{ "locale.destructor",
0, 0, 0, 0,
0, 0, 0, 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
0},
/*
__locale_destructor, __locale_destructor, __charmap_destructor, 0,
"__locale_destructor", "__locale_destructor", "__locale_destructor", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"int %s(_LC_locale_t*)"},
*/

{ "monetary.destructor",
0, 0, 0, 0,
0, 0, 0, 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
0},
/*
__monetary_destructor, __monetary_destructor, __monetary_destructor, 0,
"__monetary_destructor", "__monetary_destructor", "__monetary_destructor", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"int %s(_LC_locale_t*)"},
*/

{ "numeric.destructor",
0, 0, 0, 0,
0, 0, 0, 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
0},
/*
__numeric_destructor, __numeric_destructor, __numeric_destructor, 0,
"__numeric_destructor", "__numeric_destructor", "__numeric_destructor", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"int %s(_LC_locale_t*)"},
*/

{ "resp.destructor",
0, 0, 0, 0,
0, 0, 0, 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
0},
/*
__resp_destructor, __resp_destructor, __resp_destructor, 0,
"__resp_destructor", "__resp_destructor", "__resp_destructor", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"int %s(_LC_locale_t*)"},
*/

{ "time.destructor",
0, 0, 0, 0,
0, 0, 0, 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
0},
/*
__time_destructor, __time_destructor, __time_destructor, 0,
"__time_destructor", "__time_destructor", "__time_destructor", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"int %s(_LC_locale_t*)"},
*/

{ "collate.destructor",
0, 0, 0, 0,
0, 0, 0, 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
0},
/*
__collate_destructor, __collate_destructor, __collate_destructor, 0,
"__collate_destructor", "__collate_destructor", "__collate_destructor", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"int %s(_LC_locale_t*)"},
*/

{ "ctype.trwctype",
__trwctype_std, __trwctype_std, __trwctype_std, 0,
"__trwctype_std", "__trwctype_std", "__trwctype_std", 0,
PKGLIST, LIBC, LIBSJIS, LIBEUCJP, 0,
"wchar_t %s(_LC_ctype_t*, wchar_t, int)"},

{ "ctype.wctrans",
 __wctrans_std,   __wctrans_std,   __wctrans_std,    0,
"__wctrans_std", "__wctrans_std", "__wctrans_std",   0,
PKGLIST,
LIBC,		 LIBSJIS,	  LIBEUCJP,	     0,
"wctrans_t %s(_LC_ctype_t*, const char *)"},

{ "ctype.towctrans",
 __towctrans_bc,   __towctrans_bc,   __towctrans_bc,  0,
"__towctrans_bc", "__towctrans_bc", "__towctrans_bc", 0,
PKGLIST,
LIBC,		   LIBSJIS,	      LIBEUCJP,		 0,
"wint_t %s(_LC_ctype_t*, wint_t, wctrans_t)"},

{ "charmap.fgetwc_at_native",
 __fgetwc_sb,   __fgetwc_dense,   __fgetwc_dense,  0,
"__fgetwc_sb", "__fgetwc_dense", "__fgetwc_dense", 0,
PKGLIST,
LIBC,		   LIBSJIS,	      LIBEUCJP,		 0,
"wint_t %s(_LC_charmap_t*, FILE *)"},

{ "ctype.iswctype_at_native",
 __iswctype_sb,   __iswctype_std,   __iswctype_std,  0,
"__iswctype_sb", "__iswctype_std", "__iswctype_std", 0,
PKGLIST,
LIBC,		   LIBSJIS,	      LIBEUCJP,		 0,
"int %s(_LC_ctype_t*, wchar_t, wctype_t)"},

{ "charmap.mbftowc_at_native",
 __mbftowc_sb,   __mbftowc_dense,   __mbftowc_dense,  0,
"__mbftowc_sb", "__mbftowc_dense", "__mbftowc_dense", 0,
PKGLIST,
LIBC,		   LIBSJIS,	      LIBEUCJP,		 0,
"int %s(_LC_charmap_t*, char*, wchar_t*, int (*)(), int *)"},

{ "charmap.mbstowcs_at_native",
 __mbstowcs_sb,   __mbstowcs_dense,   __mbstowcs_dense,  0,
"__mbstowcs_sb", "__mbstowcs_dense", "__mbstowcs_dense", 0,
PKGLIST,
LIBC,		   LIBSJIS,	      LIBEUCJP,		 0,
"size_t %s(_LC_charmap_t*, wchar_t*,const char*,size_t)"},

{ "charmap.mbtowc_at_native",		/* dense method */
 __mbtowc_sb,   __mbtowc_dense,   __mbtowc_dense,  0,
"__mbtowc_sb", "__mbtowc_dense", "__mbtowc_dense", 0,
PKGLIST,
LIBC,		   LIBSJIS,	      LIBEUCJP,		 0,
"int %s(_LC_charmap_t*, wchar_t*,const char*,size_t)"},

{ "ctype.towlower_at_native",		/* dense method */
 __towlower_std,   __towlower_std,   __towlower_std,  0,
"__towlower_std", "__towlower_std", "__towlower_std", 0,
PKGLIST,
LIBC,		   LIBSJIS,	      LIBEUCJP,		 0,
"wint_t %s(_LC_ctype_t*, wint_t)"},

{ "ctype.towupper_at_native",		/* dense method */
 __towupper_std,   __towupper_std,   __towupper_std,  0,
"__towupper_std", "__towupper_std", "__towupper_std", 0,
PKGLIST,
LIBC,		   LIBSJIS,	      LIBEUCJP,		 0,
"wint_t %s(_LC_ctype_t*, wint_t)"},

{ "collate.wcscoll_at_native",		/* dense method */
 __wcscoll_std,   __wcscoll_std,   __wcscoll_std,  0,
"__wcscoll_std", "__wcscoll_std", "__wcscoll_std", 0,
PKGLIST,
LIBC,		   LIBSJIS,	      LIBEUCJP,		 0,
"int %s(_LC_collate_t*, const wchar_t*,const wchar_t*)"},

{ "ctype.wcstombs_at_native",		/* dense method */
 __wcstombs_sb,   __wcstombs_dense,   __wcstombs_dense,  0,
"__wcstombs_sb", "__wcstombs_dense", "__wcstombs_dense", 0,
PKGLIST,
LIBC,		   LIBSJIS,	      LIBEUCJP,		 0,
"size_t %s(_LC_charmap_t*, char*,const wchar_t*,size_t)"},

{ "collate.wcsxfrm_at_native",		/* dense method */
 __wcsxfrm_std,   __wcsxfrm_std,   __wcsxfrm_std,  0,
"__wcsxfrm_std", "__wcsxfrm_std", "__wcsxfrm_std", 0,
PKGLIST,
LIBC,		   LIBSJIS,	      LIBEUCJP,		 0,
"size_t %s(_LC_collate_t*, wchar_t*,const wchar_t*,size_t)"},

{ "charmap.wctomb_at_native",		/* dense method */
 __wctomb_sb,   __wctomb_dense,   __wctomb_dense,  0,
"__wctomb_sb", "__wctomb_dense", "__wctomb_dense", 0,
PKGLIST,
LIBC,		   LIBSJIS,	      LIBEUCJP,		 0,
"int %s(_LC_charmap_t*, char*, wchar_t)"},

{ "charmap.wcswidth_at_native",		/* DENSE method */
 __wcswidth_sb,   __wcswidth_dense,   __wcswidth_dense,   0, 
"__wcswidth_sb", "__wcswidth_dense", "__wcswidth_dense", 0,
PKGLIST,
LIBC, LIBSJIS, LIBEUCJP, 0,
"int %s(_LC_charmap_t*, const wchar_t *, size_t)" },

{ "charmap.wcwidth_at_native",		/* DENSE method */
 __wcwidth_sb,   __wcwidth_dense,   __wcwidth_dense,  0, 
"__wcwidth_sb", "__wcwidth_dense", "__wcwidth_dense", 0,
PKGLIST,
LIBC, LIBSJIS, LIBEUCJP, 0,
"int %s(_LC_charmap_t*, const wchar_t)" },

{ "ctype.towctrans_at_native",		/* DENSE method */
 __towctrans_std,   __towctrans_std,   __towctrans_std,  0,
"__towctrans_std", "__towctrans_std", "__towctrans_std", 0,
PKGLIST,
LIBC,		   LIBSJIS,	      LIBEUCJP,		 0,
"wint_t %s(_LC_ctype_t*, wint_t, wctrans_t)"},

};

method_t *std_methods = std_methods_tbl;
