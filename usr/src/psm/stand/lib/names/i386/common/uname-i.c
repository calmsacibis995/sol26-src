/*
 * Copyright (c) 1994-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)uname-i.c	1.9	96/10/15 SMI"

#include <sys/param.h>
#include <sys/platnames.h>

#include <sys/salib.h>

static void
make_platform_path(char *fullpath, char *iarch, char *filename)
{
	(void) strcpy(fullpath, "/platform/");
	(void) strcat(fullpath, iarch);
	(void) strcat(fullpath, "/");
	(void) strcat(fullpath, filename);
}

/*
 * Given a filename, and a function to perform an 'open' on that file,
 * find the corresponding file in the /platform hierarchy, generating
 * the implementation architecture name on the fly.
 *
 * The routine will also set 'impl_arch_name' if non-null, and returns
 * the full pathname of the file opened.
 *
 * We allow the caller to specify the impl_arch_name.  We also allow
 * the caller to specify an absolute pathname, in which case we do
 * our best to generate an impl_arch_name.
 */
int
open_platform_file(
	char *filename,
	int (*openfn)(char *, void *),
	void *arg,
	char *fullpath,
	char *impl_arch_name)
{
	/*
	 * If the caller -specifies- an absolute pathname, then we just
	 * open it after (optionally) determining the impl_arch_name.
	 * Take the same approach if a volume name is specified at the
	 * beginning of the path.
	 *
	 * This is only here for booting non-kernel standalones (or pre-5.5
	 * kernels).  It's debateable that they would ever care what the
	 * impl_arch_name is.
	 */
	extern int volume_specified(char *);

	if ((*filename == '/') || (volume_specified(filename))) {
		(void) strcpy(fullpath, filename);
		if (impl_arch_name)
			(void) strcpy(impl_arch_name, "i86pc");
		return ((*openfn)(fullpath, arg));
		/*NOTREACHED*/
	}

	/*
	 * If the caller -specifies- the impl_arch_name, then there's
	 * not much more to do than just open it.
	 *
	 * This is only here to support the '-I' flag to the boot program.
	 */
	if (impl_arch_name && *impl_arch_name != '\0') {
		make_platform_path(fullpath, impl_arch_name, filename);
		return ((*openfn)(fullpath, arg));
		/*NOTREACHED*/
	}

	/*
	 * Otherwise, we must hunt the filesystem for one that works ..
	 */
	make_platform_path(fullpath, "i86pc", filename);
	return ((*openfn)(fullpath, arg));
}

/*
 * This function provides a hook for enhancing the module_path.
 */
/*ARGSUSED*/
void
mod_path_uname_m(char *mod_path, char *impl_arch_name)
{
	/* "uname -m" is not added to the module_path for i386 platforms */
}
