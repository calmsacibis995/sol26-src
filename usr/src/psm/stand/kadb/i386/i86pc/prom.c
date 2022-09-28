/*
 *  Copyright (c) 1995, by Sun Microsystems, Inc.
 *
 *  Promlib extensions for kadb:
 *
 *    The following (prom-like) routines have special behavior when called
 *    from KADB.  For the most part, it's simply a matter of vectoring off
 *    to the appropriate standalone subroutine ...
 */

#pragma ident	"@(#)prom.c	1.3	96/11/07 SMI"

#undef _KERNEL
#include <sys/fcntl.h>
#include <sys/bootsvcs.h>

void
prom_panic(char *s)
{
	for (;;) {
		printf("PANIC: %s\n", s);
		prom_getchar();
	}
}

void
prom_enter_mon(void)
{
	printf("Press <CTL>-<ALT>-<DEL> to reboot.\n");
	for (;;)
		prom_getchar();
}

void
prom_exit_to_mon(void)
{
	prom_enter_mon();
}

char *
prom_alloc(caddr_t virt, unsigned size, int align)
{
	return (malloc(virt, size, align));
}

int
prom_open(char *path)
{
	return (open(path, O_RDONLY));
}

int
prom_close(int fd)
{
	return (close(fd));
}

int
prom_seek(int fd, unsigned long long offset)
{
	/* NB: lseek takes whence, prom_seek doesn't */
	return (lseek(fd, offset, 0));
}

int
prom_read(int fd, caddr_t buf, unsigned len, unsigned off, char dev)
{
	return (read(fd, buf, len));
}
