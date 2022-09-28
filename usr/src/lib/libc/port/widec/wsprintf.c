/*	Copyright (c) 1986 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
#ident	"@(#)wsprintf.c	1.9	96/08/19 SMI"

/*LINTLIBRARY*/
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <widec.h>

/*
 * 	wsprintf -- this function will output a wchar_t string
 *		    according to the conversion format.
 *		    Note that the maximum length of the output
 *		    string is 1024 bytes.
 */

/*VARARGS2*/

wsprintf(wchar_t *wstring, const char *format, ...)
{
	va_list	ap;
	char	tempstring[1024];
	char *p2;
	int len;
	int malloced = 0;
	char *p1 = (char *) wstring;
	int retcode;
	int	i;

	va_start(ap, format);
	if (vsprintf(p1, format, ap) == -1) {
		va_end(ap);
		return (-1);
	}
	va_end(ap);
	len = strlen(p1) + 1;
	if (len > 1024) {
		p2 = (char *) malloc(len);
		if (p2 == NULL)
			return (-1);
		malloced = 1;
	} else
		p2 = tempstring;
	strcpy(p2, p1);

	if (mbstowcs(wstring, p2, len) == -1) {
		for (i = 0; i < len; i++) {
			if ((retcode = mbtowc(wstring, p2, MB_CUR_MAX)) == -1) {
				*wstring = (wchar_t)*p2 & 0xff;
				p2++;
			} else {
				p2 += retcode;
			}
			if (*wstring++ == (wchar_t)0) {
				break;
			}
		}
	}

	if (malloced == 1)
		free(p2);
	return (wslen(wstring));
}
