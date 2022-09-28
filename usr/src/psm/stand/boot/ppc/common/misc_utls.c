/*
 * Copyright (c) 1985 by Sun Microsystems, Inc.
 */

#pragma ident "@(#)misc_utls.c	1.3 96/03/28 SMI"

#include <sys/types.h>
#include <sys/salib.h>
#include <sys/promif.h>

/*
 * The Intel version of the second level boot supports more than one
 * file system type on a hard disks and this is a pointer to the current
 * fs type. pcfs references this variable so it's initialized here to
 * resolve the external reference.
 */

char *new_root_type = "ufs";

/*
 * The Intel port was has the disk handling code in the second level boot
 * is sometimes a little to verbose with the error messages. During certain
 * operations the pcfs code needs to silence those messages.
 */

int SilentDiskFailures;

void
popup_prompt(char *s1, char *s2)
{
	/*
	 * This routine is called by the pcfs code when a different floppy
	 * needs to be inserted into the driver. The two arguments provide
	 * information for the user. In the Intel world we access the
	 * bios functions to display a banner accross the screen and then
	 * remove it afterwards. I don't know how this can be done for
	 * the PowerPC machines.
	 */
	extern int getchar();

	printf("%s\n%s\nPress ENTER to continue: ", s1, s2);
	(void) getchar();
}

/*ARGSUSED*/
is_floppy(int fd)
{
	/*
	 * The Intel side supports pcfs on the traditional drives A: and
	 * B: plus having a file system on the hard disk. The pcfs code
	 * needs to know if the device is a floppy or not. Currently the
	 * PowerPC only supports pcfs on a floppy so return true here.
	 */

	return (1);
}

/*ARGSUSED*/
is_floppy0(int fd)
{
	/*
	 * Only drive A: is supported on PowerPC so always return true.
	 */

	return (1);
}

/*ARGSUSED*/
is_floppy1(int fd)
{
	/*
	 * PowerPC machines only have one floppy right?
	 */

	return (0);
}

/*ARGSUSED*/
prom_devreset(int fd)
{
	/*
	 * This would only need to be done for hard disks. With just
	 * floppies return 0.
	 */

	return (0);
}

void
putchar(char c)
{
	prom_putchar(c);
}

int
toupper(int c)
{
	if (c >= 'a' && c <= 'z')
		c -= ('a' - 'A');
	return (c);
}

int
tolower(int c)
{
	if (c >= 'A' && c <= 'Z') c += ('a' - 'A');
	return (c);
}

/*
 *  str{n}casecmp
 *  Routines for handling strcmp's where we aren't concerned if the
 *  case differs between string elements.
 */
int
strcasecmp(register char *s1, register char *s2)
{
	while (toupper(*s1) == toupper(*s2++))
		if (*s1++ == '\0')
			return (0);
	return (toupper(*s1) - toupper(*--s2));
}

int
strncasecmp(register char *s1, register char *s2, register int n)
{
	while (--n >= 0 && toupper(*s1) == toupper(*s2++))
		if (*s1++ == '\0')
			return (0);
	return (n < 0 ? 0 : toupper(*s1) - toupper(*--s2));
}

char *
memset(char *dest, unsigned char c, int cnt)
{
	char *odest = dest;

	while (cnt-- > 0)
		*dest++ = c;
	return (odest);
}

void *
memcpy(char *dest, char *src, int cnt)
{
	bcopy(src, dest, cnt);
	return ((void *)dest);
}
