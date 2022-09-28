/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ident "@(#)env.c	1.5	96/05/13 SMI"

#include <sys/ramfile.h>
#include <sys/doserr.h>
#include <sys/dosemul.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <sys/bootvfs.h>
#include <sys/salib.h>
#include "devtree.h"

extern void interpline(char *line, int len);

#define	ENV_SRC_FILE	"solaris/bootenv.rc"
#define	ENV_WRK_FILE	"solaris/bootenv.rc.new"
#define	ENVBUFSIZE	1024

static	rffd_t	*envsfp;
static	rffd_t	*envwfp;
static	int	env_setup_done;

/*
 * gettoken
 *	Given buffer, give me pointer to next token.  Token
 *	defined to be any sequence of consecutive chars > ' '.
 *	Has memory between invocations.  Called with NULL srcbuf
 *	means continue in previously known buffer.
 *
 *	Also returns pointer to character following end of this
 *	token in the source string.
 */
static char *
gettoken(char *srcbuf, char **rob)
{
	static char *srccpy;
	static char *laststop;
	static char *nextstart;
	static char lastsave;
	static int  srclen;
	char *fb, *fe;

	if (srcbuf && srccpy) {
		bkmem_free(srccpy, srclen);
		srccpy = (char *)NULL;
	}

	if (srcbuf) {
		srclen = strlen(srcbuf) + 1;
		if ((srccpy = (char *)bkmem_alloc(srclen)) == (char *)NULL) {
			printf("ERROR: No memory for gettoken!");
			return (NULL);
		}
		(void) strcpy(srccpy, srcbuf);
		nextstart = srcbuf;
		laststop = srccpy;
	} else {
		if (laststop)
			*laststop = lastsave;
	}

	fb = laststop;
	while (fb && *fb && (*fb <= ' ')) fb++;
	fe = fb;
	while (fe && (*fe > ' ')) fe++;
	if (fe) {
		lastsave = *fe;
		*fe = '\0';
	}

	*rob = (nextstart += (fe - laststop));
	if (((laststop = fe) - srccpy) >= srclen - 1)
		laststop = (char *)NULL;

	return (fb);
}

/*
 * setupenv
 *	Convert disk version of environment rc file to a RAMfile
 *	for further manipulation (if disk version exists).
 *	Also open an empty working file.
 */
static void
setupenv(void)
{
	extern ushort RAMfile_doserr;

	if (env_setup_done)
		return;

	/*
	 *  Environment file may or may not be converted to a RAMfile
	 *  at this point.  We'll check for it to have been converted
	 *  first.
	 */
	if ((envsfp = RAMfile_open(ENV_SRC_FILE, 0)) == (rffd_t *)NULL &&
	    (envsfp = RAMcvtfile(ENV_SRC_FILE, 0)) == (rffd_t *)NULL) {
		/*
		 *  If we failed because there was no environment
		 *  file on disk, that is ok. Otherwise we're in trouble.
		 */
		if (RAMfile_doserr != DOSERR_FILENOTFOUND) {
			printf("ERROR: Unable to retrieve saved environment!");
			return;
		}
	}

	if ((envwfp = RAMfile_create(ENV_WRK_FILE, 0)) == (rffd_t *)NULL) {
		return;
	}
	env_setup_done = 1;
}

/*
 * xchange_srcnwrk
 *	Make the working RAMfile the new environment source file. Clear
 *	out the old source file for future use as a working file.
 */
static void
xchange_srcnwrk(void)
{
	rffd_t *swap;

	if (envsfp == (rffd_t *)NULL) {
		if (RAMfile_rename(envwfp, ENV_SRC_FILE)) {
			printf("ERROR: Unable to save environment changes!");
			return;
		}
		envsfp = envwfp;
		envwfp = RAMfile_create(ENV_WRK_FILE, 0);
	} else {
		swap = envsfp;

		if (RAMfile_rename(envwfp, ENV_SRC_FILE)) {
			printf("ERROR: Unable to save environment changes!");
			return;
		}
		envsfp = envwfp;
		envwfp = swap;

		/*
		 * We now we have two files with the source name.
		 * If renaming this fails, we have to delete this
		 * older version and give up on having a working
		 * file.
		 */
		if (RAMfile_rename(swap, ENV_WRK_FILE)) {
			(void) RAMfile_free(swap->file);
			(void) RAMfile_freefd(swap);
			envwfp = (rffd_t *)NULL;
			return;
		}
	}

	printf("WARNING: Environment changes will only become ");
	printf("permanent after a successful boot.\n");
}

/*ARGSUSED*/
static char *
build_envcmd(int argc, char **argv)
{
	char *cmdbuf;

	if ((cmdbuf = (char *)bkmem_alloc(ENVBUFSIZE)) == (char *)NULL) {
		printf("ERROR: No space to build a setprop command!");
	} else
		(void) sprintf(cmdbuf, "setprop %s %s\n", argv[1], argv[2]);
	return (cmdbuf);
}

static void
demolish_envcmd(char *cmdbuf)
{
	if (cmdbuf)
		bkmem_free(cmdbuf, ENVBUFSIZE);
}

static void
append_env(char *cmdbuf)
{
	if (!cmdbuf ||
	    RAMfile_write(envwfp, cmdbuf, strlen(cmdbuf)) == RAMfile_ERROR) {
		printf("ERROR: Unable to add new environment value!");
	}
}

static void
copy_nonmatches(char *matchme)
{
	char *parsebuf;
	char *fw, *sw, *rl;

	if ((parsebuf = (char *)bkmem_alloc(ENVBUFSIZE)) == (char *)NULL) {
		printf("ERROR: Unable to retrieve current environment!");
		return;
	}

	RAMrewind(envsfp);
	while (RAMfile_gets(parsebuf, ENVBUFSIZE, envsfp)) {
		fw = gettoken(parsebuf, &rl);
		if (fw && strcmp(fw, "setprop") == 0) {
			sw = gettoken((char *)NULL, &rl);
			if (sw && strcmp(matchme, sw) == 0)
				continue;
			else
				(void) RAMfile_puts(parsebuf, envwfp);
		} else
			(void) RAMfile_puts(parsebuf, envwfp);
	}

	bkmem_free(parsebuf, ENVBUFSIZE);
}

static void
display_matches(char *matchme)
{
	char *parsebuf;
	char *fw, *sw, *rl, *val;

	if ((parsebuf = (char *)bkmem_alloc(ENVBUFSIZE)) == (char *)NULL) {
		printf("ERROR: Unable to retrieve current environment!");
		return;
	}

	RAMrewind(envsfp);
	while (RAMfile_gets(parsebuf, ENVBUFSIZE, envsfp)) {
		fw = gettoken(parsebuf, &rl);
		if (strcmp(fw, "setprop") == 0) {
			sw = gettoken((char *)NULL, &rl);
			if (!matchme || (strcmp(matchme, sw) == 0)) {
				printf("%s=", sw);
				val = gettoken((char *)NULL, &rl);
				printf("%s\n", val);
			}
		}
	}

	bkmem_free(parsebuf, ENVBUFSIZE);
}

void
printenv_cmd(int argc, char **argv)
{
	setupenv();
	if (!env_setup_done || !envsfp)
		return;

	display_matches((argc > 1) ? argv[1] : (char *)NULL);
}

void
setenv_cmd(int argc, char **argv)
{
	char *cmd;

	setupenv();
	if (!env_setup_done || !envwfp) {
		printf("ERROR: Unable to change environment!");
		return;
	}

	if (argc != 3) {
		printf("Usage: setenv property-name value\n");
		return;
	}

	cmd = build_envcmd(argc, argv);
	interpline(cmd, strlen(cmd));
	copy_nonmatches(argv[1]);
	append_env(cmd);
	xchange_srcnwrk();
	demolish_envcmd(cmd);
}

void
saveenv(void)
{
	if (envsfp && (envsfp->file->flags & RAMfp_modified))
		RAMfiletoprop(envsfp);
}
