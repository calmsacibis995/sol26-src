/*
 * Copyright (c) 1994 - 1996, by Sun Microsystems Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)strsubr.c	1.8	96/05/20 SMI"

#include <sys/types.h>
#include <sys/mkdev.h>

/*
 * Miscellaneous routines used by the standalones.
 */

u_int
strlen(register const char *s)
{
	register int n;

	n = 0;
	while (*s++)
		n++;
	return (n);
}


char *
strcat(register char *s1, register const char *s2)
{
	char *os1 = s1;

	while (*s1++)
		;
	--s1;
	while (*s1++ = *s2++)
		;
	return (os1);
}

char *
strcpy(register char *s1, register const char *s2)
{
	register char *os1;

	os1 = s1;
	while (*s1++ = *s2++)
		;
	return (os1);
}

/*
 * Compare strings:  s1>s2: >0  s1==s2: 0  s1<s2: <0
 */
int
strcmp(register const char *s1, register const char *s2)
{
	while (*s1 == *s2++)
		if (*s1++ == '\0')
			return (0);
	return (*s1 - *--s2);
}

void
bcopy(const void *s, void *d, size_t count)
{
	const char *src = s;
	char *dest = d;

	if (src < dest && (src + count) > dest) {
		/* overlap copy */
		while (--count != -1)
			*(dest + count) = *(src + count);
	} else {
		while (--count != -1)
			*dest++ = *src++;
	}
}
