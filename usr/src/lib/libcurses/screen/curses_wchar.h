/*  Copyright (c) 1988 AT&T */
/*    All Rights Reserved   */
 
/*  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T */
/*  The copyright notice above does not evidence any    */
/*  actual or intended publication of such source code. */
 
#ident  "@(#)curses_wchar.h 1.1 93/05/10 SMI"
 
#define P00     WCHAR_CS0           /* Code Set 0 */
#define P11     WCHAR_CS1           /* Code Set 1 */
#define P01     WCHAR_CS2           /* Code Set 2 */
#define P10     WCHAR_CS3           /* Code Set 3 */

#ifdef __STDC__
#define _ctype __ctype
#endif
extern unsigned char _ctype[];

#define _mbyte  _ctype[520]
