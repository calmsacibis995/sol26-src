/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)read_string.c	1.3	96/06/18 SMI"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>

/*
 * Read a string, null-terminated, from addr into buf.
 * Return strlen(buf), always < size.
 */

#define	MYSIZE	40

ssize_t
read_string(int asfd, char *buf, size_t size, off_t addr)
{
	register int nbyte;
	register ssize_t leng = 0;
	char string[MYSIZE+1];

	if (size < 2)
		return (-1);
	size--;

	*buf = '\0';
	string[MYSIZE] = '\0';
	for (nbyte = MYSIZE; nbyte == MYSIZE && leng < size; addr += MYSIZE) {
		if ((nbyte = pread(asfd, string, MYSIZE, addr)) <= 0) {
			buf[leng] = '\0';
			return (leng? leng : -1);
		}
		if ((nbyte = strlen(string)) > 0) {
			if (leng + nbyte > size)
				nbyte = size - leng;
			(void) strncpy(buf+leng, string, nbyte);
			leng += nbyte;
		}
	}
	buf[leng] = '\0';
	return (leng);
}
