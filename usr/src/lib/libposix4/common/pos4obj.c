/*
 * Copyright (c) 1993-1996, by Sun Microsystems, Inc.
 * All Rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)pos4obj.c	1.7	96/04/18 SMI"

#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <pthread.h>

static char	objroot[] = "/tmp/";
static char	lcktype[] = ".LCKXXXXXX";

int
__open_nc(const char *path, int oflag, mode_t mode)
{
	int	canstate, val;

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &canstate);
	val = open(path, oflag, mode);
	pthread_setcancelstate(canstate, &canstate);
	return (val);
}

int
__close_nc(int fildes)
{
	int	canstate, val;

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &canstate);
	val = close(fildes);
	pthread_setcancelstate(canstate, &canstate);
	return (val);
}


int
__pos4obj_name(const char *path, char *dfile, char *type)
{
	char *name = (char *)path;

	if (*name++ != '/') {
		errno = EINVAL;
		return (-1);
	}

	while (*name != '\0') {
		if (*name++ == '/') {
			errno = EINVAL;
			return (-1);
		}
	}

	if (name - path >
		PATH_MAX - strlen(type) - strlen(objroot)) {
		errno = ENAMETOOLONG;
		return (-1);
	}

	strcpy(dfile, objroot);
	strcat(dfile, type);
	/* skip first `/` */
	strcat(dfile, path + 1);
	return (0);
}

/*
 * This open function assume that there is no simultaneous
 * open/unlink operation is going on. The caller is supposed
 * to ensure that both open in O_CREAT mode happen atomically.
 * It returns the crflag as 1 if file is created else 0.
 */
int
__pos4obj_open(const char *name, char *type, int oflag,
					mode_t mode, int *crflag)
{
	int fd;
	char dfile[PATH_MAX + 1];

	errno = 0;
	*crflag = 0;

	if (__pos4obj_name(name, dfile, type) < 0)
		return (-1);

	if (oflag & O_CREAT) {
		if ((fd = __open_nc(dfile, (oflag|O_EXCL), mode)) < 0) {

			/* either error other than EEXIST or EXCL mode */
			if ((errno != EEXIST) || ((oflag & O_EXCL) != 0))
				return (-1);
		} else {
			/* file got created */
			*crflag = 1;
			return (fd);
		}
	}
	/* file exists or to be opened in non-create mode */
	fd = __open_nc(dfile, oflag, mode);
	return (fd);

}

int
__pos4obj_unlink(const char *name, char *type)
{
	char dfile[PATH_MAX + 1];

	if (__pos4obj_name(name, dfile, type) < 0)
		return (-1);

	return (unlink(dfile));
}

/*
 * This function opens the lock file for each named object
 * try to lock it for exclusive use. It blocks in lockf()
 * if other process/thread has the lock, else it returns
 * 'fd' of locked file. We use advisory locks so file is
 * created with 0666 mode.
 *
 */
int
__pos4obj_lock(const char *name, char *ltype)
{
	char	lfile[PATH_MAX + 1];
	char	tfile[PATH_MAX + 1];
	int	fd;
	int	flag;
	struct	stat64	fstatbuf;
	struct	stat64	statbuf;
	int	olderr = errno;
	int	locked = 0;
	int	limit = 32;

	if (__pos4obj_name(name, lfile, ltype) < 0)
		return (-1);

	while (!locked && (limit-- > 0)) {
		if ((fd = __pos4obj_open(name, ltype, O_RDWR,
		    0666, &flag)) < 0) {
			strcpy(tfile, objroot);
			strcat(tfile, lcktype);
			mktemp(tfile);
			if ((fd = __open_nc(tfile, O_RDWR | O_CREAT | O_EXCL,
			    0666)) < 0) {
				locked = -1;
				continue;
			}
			(void) fchmod(fd, 0666);
			if (link(tfile, lfile) < 0) {
				(void) unlink(tfile);
				__close_nc(fd);
				continue;
			}
			(void) unlink(tfile);
		}

		(void) fstat64(fd, &fstatbuf);

		if (lockf(fd, F_LOCK, 0) < 0)
			locked = -2;

		else {
			/* find the inode # of same filename */
			if (stat64((const char *)lfile, &statbuf) == -1) {
				if (errno == ENOENT) {
					lockf(fd, F_ULOCK, 0);
					__close_nc(fd);
				} else
					locked = -2;
			} else {
				if (fstatbuf.st_ino == statbuf.st_ino)
					locked = 1;
				else {
					lockf(fd, F_ULOCK, 0);
					__close_nc(fd);
				}
			}
		}
	}

	if (locked > 0) {
		errno = olderr;
		return (fd);
	}

	if (locked == -2)
		__close_nc(fd);

	return (-1);
}

/*
 * Unlocks the file for given fd and then closes it
 */
void
__pos4obj_unlock(int fd)
{
	lockf(fd, F_ULOCK, 0);
	__close_nc(fd);
}
