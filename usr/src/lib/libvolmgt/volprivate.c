/*
 * Copyright (c) 1995-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)volprivate.c	1.10	96/10/10 SMI"

/*
 * routines in this module are meant to be called by other libvolmgt
 * routines only
 */

#include	<stdio.h>
#include	<string.h>
#include	<dirent.h>
#include	<string.h>
#include	<libintl.h>
#include	<limits.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/param.h>
#include	<sys/vol.h>
#ifdef	DEBUG
#include	<sys/varargs.h>
#endif
#include	"volmgt_private.h"



/*
 * We have been passed a path which (presumably) is a volume.
 * We look through the directory until we find a name which is
 * a character device.
 */
char *
getrawpart0(char *path)
{
	DIR		*dirp = NULL;
	struct dirent64	*dp;
	static char	fname[MAXNAMLEN+1];
	struct stat64	sb;
	char		*res;



	/* open the directory */
	if ((dirp = opendir(path)) == NULL) {
		res = NULL;
		goto dun;
	}

	/* scan the directory */
	while (dp = readdir64(dirp)) {

		/* skip "." and ".." */
		if (strcmp(dp->d_name, ".") == 0) {
			continue;
		}
		if (strcmp(dp->d_name, "..") == 0) {
			continue;
		}

		/* create a pathname for this device */
		(void) sprintf(fname, "%s/%s", path, dp->d_name);
		if (stat64(fname, &sb) < 0) {
			continue;		/* this shouldn't happen */
		}
		/* check for a char-spcl device */
		if (S_ISCHR(sb.st_mode)) {
			res = strdup(fname);
			goto dun;
		}
	}

	/* raw part not found */
	res = NULL;
dun:
	if (dirp != NULL) {
		(void) closedir(dirp);
	}
	return (res);
}


/*
 * fix the getfull{raw,blk}name problem for the fd and diskette case
 *
 * return value is malloc'ed, and must be free'd
 *
 * no match gets a malloc'ed null string
 */

char *
volmgt_getfullblkname(char *n)
{
	extern char	*getfullblkname(char *);
	char		*rval;
	char		namebuf[PATH_MAX+1];
	char		*s;
	char		c;
	char		*res;



	/* try to get full block-spcl device name */
	rval = getfullblkname(n);
	if ((rval != NULL) && (*rval != NULLC)) {
		/* found it */
		res = rval;
		goto dun;
	}

	/* we have a null-string result */
	if (rval != NULL) {
		/* free null string */
		free(rval);
	}

	/* ok, so we either have a bad device or a floppy */

	/* try the rfd# form */
	if ((s = strstr(n, "/rfd")) != NULL) {
		c = *++s;			/* save the 'r' */
		*s = NULLC;			/* replace it with a null */
		(void) strcpy(namebuf, n);	/* save first part of it */
		*s++ = c;			/* give the 'r' back */
		(void) strcat(namebuf, s);	/* copy, skipping the 'r' */
		res = strdup(namebuf);
		goto dun;
	}

	/* try the rdiskette form */
	if ((s = strstr(n, "/rdiskette")) != NULL) {
		c = *++s;			/* save the 'r' */
		*s = NULLC;			/* replace it with a null */
		(void) strcpy(namebuf, n);	/* save first part of it */
		*s++ = c;			/* give the 'r' back */
		(void) strcat(namebuf, s);	/* copy, skipping the 'r' */
		res = strdup(namebuf);
		goto dun;
	}

	/* no match found */
	res = strdup("");

dun:
	return (res);
}


char *
volmgt_getfullrawname(char *n)
{
	extern char	*getfullrawname(char *);
	char		*rval;
	char		namebuf[PATH_MAX+1];
	char		*s;
	char		c;
	char		*res;


#ifdef	DEBUG
	denter("volmgt_getfullrawname(%s): entering\n", n);
#endif
	/* try to get full char-spcl device name */
	rval = getfullrawname(n);
	if ((rval != NULL) && (*rval != NULLC)) {
		/* found it */
		res = rval;
		goto dun;
	}

	/* we have a null-string result */
	if (rval) {
		/* free null string */
		free(rval);
	}

	/* ok, so we either have a bad device or a floppy */

	/* try the fd# form */
	if ((s = strstr(n, "/fd")) != NULL) {
		c = *++s;			/* save the 'f' */
		*s = NULLC;			/* replace it with a null */
		(void) strcpy(namebuf, n);	/* save first part of it */
		*s = c;				/* put the 'f' back */
		(void) strcat(namebuf, "r");	/* insert an 'r' */
		(void) strcat(namebuf, s);	/* copy the rest */
		res = strdup(namebuf);
		goto dun;
	}

	/* try the diskette form */
	if ((s = strstr(n, "/diskette")) != NULL) {
		c = *++s;			/* save at 'd' */
		*s = NULLC;			/* replace it with a null */
		(void) strcpy(namebuf, n);	/* save first part */
		*s = c;				/* put the 'd' back */
		(void) strcat(namebuf, "r");	/* insert an 'r' */
		(void) strcat(namebuf, s);	/* copy the rest */
		res = strdup(namebuf);
		goto dun;
	}

	/* no match found */
	res = strdup("");
dun:
#ifdef	DEBUG
	dexit("volmgt_getfullrawname: returning %s\n",
	    res ? res : "<null ptr>");
#endif
	return (res);
}


/*
 * volctl_name -- return name of volctl device
 */
const char *
volctl_name(void)
{
	static char	*dev_name = NULL;
	const char	dev_dir[] = "/dev";


	/* see if name hasn't already been set up */
	if (dev_name == NULL) {
		/* set up name */
		if ((dev_name = (char *)malloc(strlen(dev_dir) +
		    strlen(VOLCTLNAME) + 2)) != NULL) {
			(void) strcpy(dev_name, dev_dir);
			(void) strcat(dev_name, "/");
			(void) strcat(dev_name, VOLCTLNAME);
		}
	}

	return (dev_name);
}



#ifdef	DEBUG

/*
 * debug print routines -- private to libvolmgt
 */

#define	DEBUG_INDENT_SPACES	"  "

int	debug_level = 0;


static void
derrprint(char *fmt, va_list ap)
{
	int		i;
	int		j;
	char		date_buf[256];
	time_t		t;
	struct tm	*tm;


	(void) time(&t);
	tm = localtime(&t);
	(void) fprintf(stderr, "%02d/%02d/%02d %02d:%02d:%02d ",
	    tm->tm_mon+1, tm->tm_mday,
	    (tm->tm_year > 100) ? (tm->tm_year - 100) : tm->tm_year,
	    tm->tm_hour, tm->tm_min, tm->tm_sec);
	for (i = 0; i < debug_level; i++) {
		(void) fprintf(stderr, DEBUG_INDENT_SPACES);
	}
	(void) vfprintf(stderr, fmt, ap);
}

/*
 * denter -- do a derrprint(), then increment debug level
 */
void
denter(char *fmt, ...)
{
	va_list		ap;

	va_start(ap, fmt);
	derrprint(fmt, ap);
	va_end(ap);
	debug_level++;
}

/*
 * dexit -- decrement debug level then do a derrprint()
 */
void
dexit(char *fmt, ...)
{
	va_list		ap;

	if (--debug_level < 0) {
		debug_level = 0;
	}
	va_start(ap, fmt);
	derrprint(fmt, ap);
	va_end(ap);
}

/*
 * dprintf -- print debug info, indenting based on debug level
 */
void
dprintf(char *fmt, ...)
{
	va_list		ap;

	va_start(ap, fmt);
	derrprint(fmt, ap);
	va_end(ap);
}

#endif	/* DEBUG */
