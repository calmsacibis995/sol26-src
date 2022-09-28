/*
 * Copyright (c) 1990 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)stubs.c	1.8	96/09/19 SMI"

#include <sys/bootconf.h>
#include <sys/promif.h>
#include <sys/fcntl.h>

extern struct bootops *bootops;
extern char *strcpy(register char *, register const char *);
extern int strcmp(register const char *, register const char *);

/*
 * Ask prom to open a disk file given the device path representing
 * the target drive/partition and the fs-relative path of the
 * file.
 */
int
open(char *path, char *fs)
{
	/*
	 * sun4m prom (OBP) and ufsboot allow files on only one
	 * filesystem to be open simultaneously.  The root fs is
	 * mounted during ufsboot initialization.  We use current_fs
	 * to cache the name of the currently mouted fs to avoid
	 * unecessary mounts when not changing fs.
	 */
	static char current_fs[OBP_MAXPATHLEN];

	/*
	 * XXX To avoid the use of this obnoxious "first time" test,
	 * we could make current_fs a file static and write a function
	 * to initialize it, calling this platform-specific function
	 * from common init code.  Possibly not worth it.
	 */
	if (current_fs[0] == '\0')
		(void) strcpy(current_fs, prom_bootpath());

	if (strcmp(fs, current_fs)) {
		if (BOP_UNMOUNTROOT(bootops) == -1)
			return (-1);
		if (BOP_MOUNTROOT(bootops, fs) == -1)
			return (-1);
		(void) strcpy(current_fs, fs);
	}

	return (BOP_OPEN(bootops, path, O_RDONLY));
}

int
read(int fd, char *buf, int count)
{
	return (BOP_READ(bootops, fd, buf, count));
}

int
close(int fd)
{
	return (BOP_CLOSE(bootops, fd));
}
