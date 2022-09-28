/*
 *	rmtab.c
 *
 *	Copyright (c) 1988-1995 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)rmtab.c 1.6     96/04/26 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <rpcsvc/mount.h>
#include <sys/pathconf.h>
#include <sys/systeminfo.h>
#include <sys/utsname.h>
#include <signal.h>
#include <locale.h>
#include <unistd.h>
#include <thread.h>
#include "../lib/sharetab.h"

#define	MAXHOSTNAMELEN	64
#define	MAXRMTABLINELEN		(MAXPATHLEN + MAXHOSTNAMELEN + 2)
static char RMTAB[] = "/etc/rmtab";
static FILE *rmtabf = NULL;

/*
 * There is nothing magic about the value selected here. Too low,
 * and mountd might spend too much time rewriting the rmtab file.
 * Too high, it won't do it frequently enough.
 */
static int rmtab_del_thresh = 250;

#define	RMTAB_TOOMANY_DELETED()	\
	((rmtab_deleted > rmtab_del_thresh) && (rmtab_deleted > rmtab_inuse))

/*
 * mountd's version of a "struct mountlist". It is the same except
 * for the added ml_pos field.
 */
struct mountdlist {
/* same as XDR mountlist */
	char *ml_name;
	char *ml_path;
	struct mountdlist *ml_nxt;
/* private to mountd */
	long ml_pos;		/* position of mount entry in RMTAB */
};

struct mountdlist *mntlist;

void rmtab_load(void);
void mntlist_new(char *host, char *path);
void mntlist_delete(char *host, char *path);
void mntlist_delete_all(char *host);
void mntlist_send(SVCXPRT *trans);

static void rmtab_delete(long);
static long rmtab_insert(char *, char *);
static int in_mount_list(char *, char *);
static void rmtab_rewrite(void);

extern char *exmalloc(int);
extern void log_cant_reply(SVCXPRT *transp);

extern int errno;

static int rmtab_inuse;
static int rmtab_deleted;

rwlock_t rmtab_lock;	/* lock to protect rmtab list */

/*
 * Read in and internalize the contents of rmtab.
 * Rewrites the file to get rid of unused entries.
 */
void
rmtab_load()
{
	char *path;
	char *name;
	char *end;
	struct mountdlist *ml;
	char line[MAXRMTABLINELEN];

	(void) rwlock_init(&rmtab_lock, USYNC_THREAD, NULL);

	/*
	 * Don't need to lock the list at this point
	 * because there's only a single thread running.
	 */

	mntlist = NULL;
	rmtabf = fopen(RMTAB, "r");
	if (rmtabf != NULL) {
		while (fgets(line, sizeof (line), rmtabf) != NULL) {
			name = line;
			path = strchr(name, ':');
			if (*name != '#' && path != NULL) {
				*path = 0;
				path++;
				end = strchr(path, '\n');
				if (end != NULL) {
					*end = 0;
				}
				if (in_mount_list(name, path)) {
					continue; /* skip duplicates */
				}
				/* LINTED pointer alignment */
				ml = (struct mountdlist *)
					exmalloc(sizeof (struct mountdlist));
				ml->ml_path = (char *)
					exmalloc(strlen(path) + 1);
				(void) strcpy(ml->ml_path, path);
				ml->ml_name = (char *)
					exmalloc(strlen(name) + 1);
				(void) strcpy(ml->ml_name, name);
				ml->ml_nxt = mntlist;
				mntlist = ml;
			}
		}
		(void) fclose(rmtabf);
		rmtabf = NULL;
	}
	rmtab_rewrite();
}

/*
 *  Add an entry to the mount list.
 *  First check whether it's there already - the client
 *  may have crashed and be rebooting.
 */
void
mntlist_new(host, path)
	char *host;
	char *path;
{
	struct mountdlist *ml;

	if (in_mount_list(host, path))
		return;

	/*
	 * Add this mount to the mount list.
	 */

	(void) rw_wrlock(&rmtab_lock);

	/* LINTED pointer alignment */
	ml = (struct mountdlist *) exmalloc(sizeof (struct mountdlist));
	ml->ml_path = (char *) exmalloc(strlen(path) + 1);
	(void) strcpy(ml->ml_path, path);
	ml->ml_name = (char *) exmalloc(strlen(host) + 1);
	(void) strcpy(ml->ml_name, host);
	ml->ml_nxt = mntlist;
	ml->ml_pos = rmtab_insert(host, path);
	rmtab_inuse++;
	mntlist = ml;

	(void) rw_unlock(&rmtab_lock);
}

/*
 * Delete an entry from the mount list.
 */
void
mntlist_delete(host, path)
	char *host;
	char *path;
{
	struct mountdlist *ml, *prev;

	(void) rw_wrlock(&rmtab_lock);

	prev = mntlist;
	for (ml = mntlist; ml; prev = ml, ml = ml->ml_nxt) {
		if (strcmp(ml->ml_path, path) == 0 &&
			strcmp(ml->ml_name, host) == 0) {
			if (ml == mntlist) {
				mntlist = ml->ml_nxt;
			} else {
				prev->ml_nxt = ml->ml_nxt;
			}
			rmtab_delete(ml->ml_pos);
			free(ml->ml_path);
			free(ml->ml_name);
			free((char *) ml);
			rmtab_inuse--;
			rmtab_deleted++;
			break;
		}
	}

	if (RMTAB_TOOMANY_DELETED())
		rmtab_rewrite();

	(void) rw_unlock(&rmtab_lock);
}

/*
 * Delete all entries for a host from the mount list
 */
void
mntlist_delete_all(host)
	char *host;
{
	struct mountdlist *ml, *prev, *next;

	(void) rw_wrlock(&rmtab_lock);

	prev = mntlist;
	for (ml = mntlist; ml; ml = next) {
		next = ml->ml_nxt;

		if (strcmp(ml->ml_name, host) == 0) {
			if (ml == mntlist) {
				mntlist = ml->ml_nxt;
				prev = mntlist;
			} else {
				prev->ml_nxt = ml->ml_nxt;
			}
			rmtab_delete(ml->ml_pos);
			free(ml->ml_path);
			free(ml->ml_name);
			free((char *)ml);
			rmtab_inuse--;
			rmtab_deleted++;
		} else {
			prev = ml;
		}
	}

	if (RMTAB_TOOMANY_DELETED())
		rmtab_rewrite();

	(void) rw_unlock(&rmtab_lock);
}

void
mntlist_send(transp)
	SVCXPRT *transp;
{
	(void) rw_rdlock(&rmtab_lock);

	errno = 0;
	if (!svc_sendreply(transp, xdr_mountlist, (char *)&mntlist))
		log_cant_reply(transp);

	(void) rw_unlock(&rmtab_lock);
}

static void
rmtab_rewrite()
{
	struct mountdlist *ml;

	if (rmtabf != NULL) {
		(void) fclose(rmtabf);
	}

	(void) truncate(RMTAB, (off_t)0);

	/* Rewrite the file. */
	rmtabf = fopen(RMTAB, "w+");
	if (rmtabf != NULL) {
		rmtab_inuse = rmtab_deleted = 0;
		for (ml = mntlist; ml; ml = ml->ml_nxt) {
			ml->ml_pos = rmtab_insert(ml->ml_name, ml->ml_path);
			rmtab_inuse++;
		}
	}
}

/*
 * Check whether the given client/path combination
 * already appears in the mount list.
 */

static int
in_mount_list(hostname, directory)
	char *hostname;			/* client to check for */
	char *directory;		/* path to check for */
{
	struct mountdlist *ml;

	(void) rw_rdlock(&rmtab_lock);

	for (ml = mntlist; ml; ml = ml->ml_nxt) {
		if (strcmp(hostname, ml->ml_name) == 0 &&
		    strcmp(directory, ml->ml_path) == 0) {
			(void) rw_unlock(&rmtab_lock);
			return (1);
		}
	}

	(void) rw_unlock(&rmtab_lock);

	return (0);
}

/*
 * Write an entry at the current location in rmtab
 * for the given client and path.
 *
 * Returns the starting position of the entry
 * or -1 if there was an error.
 */

long
rmtab_insert(hostname, path)
	char *hostname;			/* name of client */
	char *path;
{
	long pos;

	if (rmtabf == NULL || fseek(rmtabf, 0L, 2) == -1) {
		return (-1);
	}
	pos = ftell(rmtabf);
	if (fprintf(rmtabf, "%s:%s\n", hostname, path) == EOF) {
		return (-1);
	}
	if (fflush(rmtabf) == EOF) {
		return (-1);
	}
	return (pos);
}

/*
 * Mark as unused the rmtab entry at the given offset in the file.
 */

void
rmtab_delete(pos)
	long pos;
{
	if (rmtabf != NULL && pos != -1 && fseek(rmtabf, pos, 0) == 0) {
		(void) fprintf(rmtabf, "#");
		(void) fflush(rmtabf);
	}
}
