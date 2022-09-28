/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)stubs.c	1.7	96/09/19 SMI"

#include <sys/promif.h>

u_int strlen(register const char *);
char * strcat(register char *, register const char *);
char * strcpy(register char *, register const char *);

/*
 * Ask prom to open a disk file given the device path representing
 * the target drive/partition and the fs-relative path of the
 * file.  Handle file pathnames with or without leading '/'.
 */
/* ARGSUSED */
int
open(char *path, char *fs)
{
	char full_path[OBP_MAXPATHLEN];
	char *fp = full_path;
	int handle;
	int c;

	/*
	 * IEEE 1275 prom needs "device-path,|file-path" where
	 * file-path can have embedded |'s.
	 */
	(void) strcpy(fp, fs);
	fp += strlen(fp);
	*fp++ = ',';
	*fp++ = '|';

	/* Skip a leading slash in file path -- we provided for it above. */
	if (*path == '/')
		path++;

	/* Copy file path and convert separators. */
	while ((c = *path++) != '\0')
		if (c == '/')
			*fp++ = '|';
		else
			*fp++ = c;
	*fp = '\0';

	/* prom_open for IEEE 1275 returns 0 on failure; we return -1 */
	return ((handle = prom_open(full_path)) ? handle : -1);
}

int
read(int fd, caddr_t buf, int len)
{
	return (prom_read(fd, buf, len, 0, 0));
}

int
close(int fd)
{
	return (prom_close(fd));
}
