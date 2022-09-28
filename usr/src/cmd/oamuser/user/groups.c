/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)groups.c	1.7	94/09/12	SMI"	/* SVr4.0 1.5 */

#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>
#include <grp.h>
#include <unistd.h>
#include <userdefs.h>
#include "users.h"

struct group *getgrnam();
struct group *getgrgid();
struct group *fgetgrent();
extern void putgrent();
extern long strtol();
extern pid_t getpid();
extern int strcmp(), rename(), unlink();

int
edit_group(login, new_login, gids, overwrite)
char *login, *new_login;
gid_t gids[];		/* group id to add login to */
int overwrite;	/* overwrite != 0 means replace existing ones */
{
	char **memptr, *t_name;
	FILE *e_fptr, *t_fptr;
	struct group *g_ptr;	/* group structure from fgetgrent */
	register i, modified = 0;
	struct stat sbuf;

	if ((e_fptr = fopen(GROUP, "r")) == NULL) {
		return (EX_UPDATE);
	}

	if (fstat(fileno(e_fptr), &sbuf) != 0)
		return (EX_UPDATE);

	if ((t_name = tempnam("/etc", "gtmp.")) == NULL) {
		return (EX_UPDATE);
	}

	if ((t_fptr = fopen(t_name, "w+")) == NULL) {
		return (EX_UPDATE);
	}

	/*
	 * Get ownership and permissions correct
	 */

	if (fchmod(fileno(t_fptr), sbuf.st_mode) != 0 ||
	    fchown(fileno(t_fptr), sbuf.st_uid, sbuf.st_gid) != 0) {
		(void) unlink(t_name);
		return (EX_UPDATE);
	}

	/* Make TMP file look like we want GROUP file to look */
	while ((g_ptr = fgetgrent(e_fptr)) != NULL) {

		/* first delete the login from the group, if it's there */
		if (overwrite || !gids) {
			if (g_ptr->gr_mem != NULL) {
				for (memptr = g_ptr->gr_mem; *memptr;
				    memptr++) {
					if (strcmp(*memptr, login) == 0) {
						/* Delete this one */
						char **from = memptr + 1;

						do {
							*(from - 1) = *from;
						} while (*from++);

						modified++;
						break;
					}
				}
			}
		}

		/* now check to see if group is one to add to */
		if (gids) {
			for (i = 0; gids[ i ] != -1; i++) {
				if (g_ptr->gr_gid == gids[i]) {
					/* Find end */
					for (memptr = g_ptr->gr_mem; *memptr;
						memptr++)
						;

					*memptr++ = new_login? new_login: login;
					*memptr = NULL;

					modified++;
				}
			}
		}


		putgrent(g_ptr, t_fptr);

	}

	(void) fclose(e_fptr);
	(void) fclose(t_fptr);

	/* Now, update GROUP file, if it was modified */
	if (modified && rename(t_name, GROUP) < 0) {
		(void) unlink(t_name);
		return (EX_UPDATE);
	}

	(void) unlink(t_name);

	return (EX_SUCCESS);
}
