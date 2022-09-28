/*
 * Copyright (c) 1994-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)uname-i.c	1.13	96/10/15 SMI"

#include <sys/param.h>
#include <sys/cpu.h>
#include <sys/idprom.h>
#include <sys/promif.h>
#include <sys/salib.h>

#include <sys/platnames.h>

/*
 * This source is (and should be ;-) shared between the boot blocks
 * and the boot programs.  So if you change it, be sure to test them all!
 */

#define	MAXNMLEN	1024		/* # of chars in a property */

/*
 * Supplied by modpath.c
 *
 * Making these externs here allows all sparc machines to share
 * get_impl_arch_name().
 */
extern char *default_name;
extern char *default_path;

short
_get_cputype(void)
{
	dnode_t n;
	struct idprom idprom;

	if ((n = prom_rootnode()) != OBP_NONODE &&
	    prom_getprop(n, OBP_IDPROM, (caddr_t)&idprom) == sizeof (idprom))
		return (idprom.id_machine);
	return (CPU_NONE);
}

/*
 * Use the cputype to determine the impl_arch name
 * Really only needed for sun4c clone machines.
 */
static char *
get_iarch_from_cputype(void)
{
	struct cputype2name *p;
	short cputype;

	if ((cputype = _get_cputype()) == 0x20)
		return ("sun4c");	/* SI bogosity! */
	for (p = _cputype2name_tbl; p->cputype != CPU_NONE; p++)
		if (p->cputype == cputype)
			return (p->iarch);
	if ((CPU_ARCH & cputype) == SUN4C_ARCH)
		return ("sun4c");	/* naughty clone */
	return (NULL);
}

enum ia_state_mach {
	STATE_NAME,
	STATE_COMPAT_INIT,
	STATE_COMPAT,
	STATE_DEFAULT,
	STATE_CPUTYPE,
	STATE_FINI
};

/*
 * Return the implementation architecture name (uname -i) for this platform.
 *
 * Use the named rootnode property to determine the iarch; if the name is
 * an empty string, use the cputype.
 */
static char *
get_impl_arch_name(enum ia_state_mach *state)
{
	static char iarch[MAXNMLEN];
	static int len;
	static char *ia;

	dnode_t n;
	char *cp;
	struct cputype2name *p;
	char *namename;

newstate:
	switch (*state) {
	case STATE_NAME:
		*state = STATE_COMPAT_INIT;
		namename = "name";
		n = (dnode_t)prom_rootnode();
		len = prom_getproplen(n, namename);
		if (len <= 0 || len >= MAXNMLEN)
			goto newstate;
		(void) prom_getprop(n, namename, iarch);
		iarch[len] = '\0';	/* fix broken clones */
		ia = iarch;
		break;

	case STATE_COMPAT_INIT:
		*state = STATE_COMPAT;
		namename = "compatible";
		n = (dnode_t)prom_rootnode();
		len = prom_getproplen(n, namename);
		if (len <= 0 || len >= MAXNMLEN) {
			*state = STATE_DEFAULT;
			goto newstate;
		}
		(void) prom_getprop(n, namename, iarch);
		iarch[len] = '\0';	/* ensure null termination */
		ia = iarch;
		break;

	case STATE_COMPAT:
		/*
		 * Advance 'ia' to point to next string in
		 * compatible property array (if any).
		 */
		while (*ia++)
			;
		if ((ia - iarch) >= len) {
			*state = STATE_DEFAULT;
			goto newstate;
		}
		break;

	case STATE_DEFAULT:
		*state = STATE_CPUTYPE;
		if (default_name == NULL)
			goto newstate;
		(void) strcpy(iarch, default_name);
		ia = iarch;
		break;

	case STATE_CPUTYPE:
		*state = STATE_FINI;
		return (get_iarch_from_cputype());

	case STATE_FINI:
		return (NULL);
	}

	/*
	 * Crush filesystem-awkward characters.  See PSARC/1992/170.
	 * (Convert the property to a sane directory name in UFS)
	 */
	for (cp = ia; *cp; cp++)
		if (*cp == '/' || *cp == ' ' || *cp == '\t')
			*cp = '_';
	/*
	 * Convert old sun4c names to 'SUNW,' prefix form
	 */
	for (p = _cputype2name_tbl; p->iarch != (char *)0; p++)
		if (strcmp(p->iarch + 5, ia) == 0)
			return (p->iarch);
	return (ia);
}

static void
make_platform_path(char *fullpath, char *iarch, char *filename)
{
	(void) strcpy(fullpath, "/platform/");
	(void) strcat(fullpath, iarch);
	if (filename != NULL) {
		(void) strcat(fullpath, "/");
		(void) strcat(fullpath, filename);
	}
}

/*
 * Generate impl_arch_name by searching the /platform hierarchy
 * for a matching directory.  We are not looking for any particular
 * file here, but for a directory hierarchy for the module path.
 */
int
find_platform_dir(int (*isdirfn)(char *), char *iarch)
{
	char fullpath[MAXPATHLEN];
	char *ia;
	enum ia_state_mach state = STATE_NAME;

	/*
	 * Hunt the filesystem looking for a directory hierarchy.
	 */
	while ((ia = get_impl_arch_name(&state)) != NULL) {
		make_platform_path(fullpath, ia, NULL);
		if (((*isdirfn)(fullpath)) != 0) {
			strcpy(iarch, ia);
			return (1);
		}
	}
	return (0);
}

/*
 * Search the /platform hierarchy looking for a particular file.
 *
 * impl_arch_name is given as an optional hint as to where the
 * file might be found.
 */
int
open_platform_file(
	char *filename,
	int (*openfn)(char *, void *),
	void *arg,
	char *fullpath,
	char *impl_arch_name)
{
	char *ia;
	int fd;
	enum ia_state_mach state = STATE_NAME;

	/*
	 * First try the impl_arch_name hint.
	 *
	 * This is only here to support the -I flag to boot.
	 */
	if (impl_arch_name != NULL) {
		make_platform_path(fullpath, impl_arch_name, filename);
		return ((*openfn)(fullpath, arg));
	}

	/*
	 * Hunt the filesystem for one that works ..
	 */
	while ((ia = get_impl_arch_name(&state)) != NULL) {
		make_platform_path(fullpath, ia, filename);
		if ((fd = (*openfn)(fullpath, arg)) != -1)
			return (fd);
	}

	return (-1);
}
