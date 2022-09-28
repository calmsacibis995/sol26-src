/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)cwd.c	1.5	96/08/01 SMI"

#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/syscall.h>
#include <sys/dirent.h>
#include <sys/stat.h>

#ifdef _STAT_VER
#define	SYS_STAT	SYS_xstat, _STAT_VER
#define	SYS_FSTAT	SYS_fxstat, _STAT_VER
#else
#define	SYS_STAT	SYS_stat
#define	SYS_FSTAT	SYS_fstat
#endif

#define	NULL	0

#define	MAX_PATH	1024
#define	MAX_NAME	512
#define	BUF_SIZE	1536	/* 3/2 of MAX_PATH for /a/a/a... case */

/*
 * This algorithm does not use chdir.  Instead, it opens a
 * succession of strings for parent directories, i.e. .., ../..,
 * ../../.., and so forth.
 */
typedef struct data {
	char	path[MAX_PATH+4];
	struct stat	cdir;	/* current directory status */
	struct stat	tdir;
	struct stat	pdir;	/* parent directory status */
	char	dotdots[BUF_SIZE+MAX_NAME];
	int	dirsize;
	int	diroffset;
	char	dirbuf[1024];
} data_t;

static	int	opendir(data_t *p, char *path);
static	int	closedir(data_t *p, int pdfd);
static struct dirent *readdir(data_t *p, int pdfd);

static	char	*strcpy(char *t, const char *s);
static	char	*strncpy(char *t, const char *s, size_t n);
static	size_t	strlen(const char *s);

/* inline */
int	syscall(int, ...);

char *
cwd(data_t *p)
{
	register char *str = p->path;
	register int size = sizeof (p->path);
	register int		pdfd;	/* parent directory stream */
	register struct dirent *dir;
	char *dotdot = p->dotdots + BUF_SIZE - 3;
	char *dotend = p->dotdots + BUF_SIZE - 1;
	int i, maxpwd, ret;

	*dotdot = '.';
	*(dotdot+1) = '.';
	*(dotdot+2) = '\0';
	maxpwd = size--;
	str[size] = 0;

	if (syscall(SYS_STAT, dotdot+1, &p->pdir) < 0)
		return (NULL);

	for (;;) {
		/* update current directory */
		p->cdir = p->pdir;

		/* open parent directory */
		if ((pdfd = opendir(p, dotdot)) < 0)
			break;

		if (syscall(SYS_FSTAT, pdfd, &p->pdir) < 0) {
			(void) closedir(p, pdfd);
			break;
		}

		/*
		 * find subdirectory of parent that matches current
		 * directory
		 */
		if (p->cdir.st_dev == p->pdir.st_dev) {
			if (p->cdir.st_ino == p->pdir.st_ino) {
				/* at root */
				(void) closedir(p, pdfd);
				if (size == (maxpwd - 1))
					/* pwd is '/' */
					str[--size] = '/';

				(void) strcpy(str, &str[size]);
				return (str);
			}
			do {
				if ((dir = readdir(p, pdfd)) == NULL) {
					(void) closedir(p, pdfd);
					goto out;
				}
			} while (dir->d_ino != p->cdir.st_ino);
		} else {
			/*
			 * must determine filenames of subdirectories
			 * and do stats
			 */
			*dotend = '/';
			do {
		again:
				if ((dir = readdir(p, pdfd)) == NULL) {
					(void) closedir(p, pdfd);
					goto out;
				}
				if (dir->d_name[0] == '.') {
					if (dir->d_name[1] == '\0')
						goto again;
					if (dir->d_name[1] == '.' &&
					dir->d_name[2] == '\0')
						goto again;
				}
				(void) strcpy(dotend + 1, dir->d_name);
				/*
				 * skip over non-stat'able
				 * entries
				 */
				ret = syscall(SYS_STAT, dotdot, &p->tdir);

			} while (ret == -1 ||
			    p->tdir.st_ino != p->cdir.st_ino ||
			    p->tdir.st_dev != p->cdir.st_dev);
		}
		(void) closedir(p, pdfd);

		i = strlen(dir->d_name);

		if (i > size - 1) {
			break;
		} else {
			/* copy name of current directory into pathname */
			size -= i;
			(void) strncpy(&str[size], dir->d_name, i);
			str[--size] = '/';
		}
		if (dotdot - 3 < p->dotdots)
			break;
		/* update dotdot to parent directory */
		*--dotdot = '/';
		*--dotdot = '.';
		*--dotdot = '.';
		*dotend = '\0';
	}
out:
	return (NULL);
}

static int
opendir(data_t *p, char *path)
{
	p->dirsize = p->diroffset = 0;
	return (syscall(SYS_open, path, O_RDONLY, 0));
}

static int
closedir(data_t *p, int pdfd)
{
	p->dirsize = p->diroffset = 0;
	return (syscall(SYS_close, pdfd));
}

static struct dirent *
readdir(data_t *p, register int pdfd)
{
	register struct dirent *dp;

	if (p->diroffset >= p->dirsize) {
		p->diroffset = 0;
		/* LINTED improper alignment */
		dp = (struct dirent *)p->dirbuf;
		p->dirsize =
			syscall(SYS_getdents, pdfd, dp, sizeof (p->dirbuf));
		if (p->dirsize <= 0) {
			p->dirsize = 0;
			return (NULL);
		}
	}

	/* LINTED improper alignment */
	dp = (struct dirent *)&p->dirbuf[p->diroffset];
	p->diroffset += dp->d_reclen;

	return (dp);
}

static char *
strcpy(register char *t, register const char *s)
{
	register char *p = t;

	while (*t++ = *s++)
		;

	return (p);
}

static char *
strncpy(register char *t, register const char *s, register size_t n)
{
	register char *p = t;

	while (n-- && (*t++ = *s++))
		;

	return (p);
}

static size_t
strlen(register const char *s)
{
	register const char *t = s;

	while (*t++)
		;

	return (t - s - 1);
}

#if defined(i386) || defined(__i386)

/* This is a crock, forced on us by <sys/stat.h> */

/* ARGSUSED */
int
_xstat(const int code, const char *fn, struct stat *statb)
{
	return (0);
}

/* ARGSUSED */
int
_fxstat(const int code, int fd, struct stat *statb)
{
	return (0);
}

/* ARGSUSED */
int
_lxstat(const int code, const char *fn, struct stat *statb)
{
	return (0);
}

/* ARGSUSED */
int
_xmknod(const int code, const char *fn, mode_t mode, dev_t dev)
{
	return (0);
}

#endif
