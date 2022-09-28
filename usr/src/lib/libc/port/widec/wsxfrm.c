/* wscoll() and wsxfrm(). */
/* This is Sun's propriatry implementation of wsxfrm() and wscoll()	*/
/* using dynamic linking.  It is probably free from AT&T copyright.	*/
/* 	COPYRIGHT (C) 1991 SUN MICROSYSTEMS, INC.			*/
/*	ALL RIGHT RESERVED.						*/

#ident  "@(#)wsxfrm.c 1.14     96/04/08 SMI"

#pragma weak wscoll = _wscoll
#pragma weak wsxfrm = _wsxfrm

#include <wchar.h>

 extern _wcscoll(const wchar_t *s1, const wchar_t *s2);
 extern _wcsxfrm(wchar_t *s1, const wchar_t *s2, size_t n);


size_t
_wsxfrm(wchar_t *s1, const wchar_t *s2, size_t n)
{
	return (_wcsxfrm(s1, s2, n));
}

int
_wscoll(const wchar_t *s1, const wchar_t *s2)
{
	return (_wcscoll(s1, s2));
}

