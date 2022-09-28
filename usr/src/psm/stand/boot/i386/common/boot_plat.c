/*
 * Copyright (c) 1991-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)boot_plat.c	1.15	96/05/17 SMI"

#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/bootvfs.h>
#include <sys/platnames.h>
#include <sys/bootconf.h>
#include <sys/dev_info.h>
#include <sys/bootlink.h>
#include <sys/bootp2s.h>
#include <sys/bsh.h>
#include <sys/salib.h>
#include <sys/promif.h>

#define	VAC_DEFAULT		1
#define	PAGESHIFT_DEFAULT	12
#define	PAGESIZE_DEFAULT	(1 << PAGESHIFT_DEFAULT)

static int debug = 0;
#define	dprintf			if (debug) printf

unchar *BSH_DEFAULT_RUNPATH = (unchar *)"/platform/i86pc:/";
unchar *PATHVAR = (unchar *)"path";

extern	unchar *var_ops(unsigned char *, unsigned char *, int);
extern	char *kernname;
extern	char *impl_arch_name;
extern	struct bootops bootops;
extern	char *makepath(char *mname, char *fname);
extern	void bsh(void);
extern	void v2_getargs(char *defname, char *buf);
extern	void compatboot_bootpath(char *bpath);
extern	int bsetprop(struct bootops *, char *, caddr_t, int, phandle_t);
extern	void dosemul_init(void);

int vac = VAC_DEFAULT;
int pagesize = PAGESIZE_DEFAULT;
int cache_state = 1;
char *mfg_name;

void
setup_bootargs(char *bargs)
{
	/*
	 * Set up some key properties (after bootops initialized)
	 * NOTE: can't set "bootpath" here, need to do some probing
	 * first, to determine bus-type, hba, other such information.
	 */
	(void) bsetprop(&bootops, "boot-args", bargs, 0, 0);
}

void
translate_tov2(char **v2path, char *bpath)
{
	/*
	 *  Note that we have to set both bootpath and boot-path
	 *  properties.  Originally we planned not to have to worry
	 *  about both by making them mirrors of one another.
	 *
	 *  Unfortunately, net booting older kernels requires that
	 *  we now withhold information that we know about network
	 *  devices.  I.E., boot-path for an older netboot must be
	 *  something akin to 'smc@0,0'.  Because we are building
	 *  real device trees now, we actually have a node in the
	 *  device tree something like 'smc@300,d0000'.  We can't
	 *  use this more useful information without causing older
	 *  kernels to choke and die.  Here's the compromise.  For
	 *  net boots, bootpath has the devtree node, but boot-path
	 *  has the old style driver@0,0 path.  Newer kernels look
	 *  first for 'bootpath' and at 'boot-path' only if they
	 *  didn't find the previous.  Older kernels always look for
	 *  'boot-path'.
	 *
	 *  At this point, we don't have any idea if this is a net
	 *  boot or anything, so at this point, the two properties
	 *  should mirror one another.
	 *
	 *  2/15/96 - timh
	 *	I believe we want the 1275 "bootpath" only to be set by
	 *  bootconf. This provides 2.6 kernels a means to determine if
	 *  it they are going to receive a real device tree.  I.E., they
	 *  would only be getting a real tree if 'bootpath' were set. I
	 *  commented out the line below in case we want to put it back
	 *  later when kernels shouldn't need to have to make such a
	 *  determination.
	 *
	 * bsetprop(&bootops, "bootpath", bpath, 0, 0);
	 */
	(void) bsetprop(&bootops, "boot-path", bpath, 0, 0);
	*v2path = bpath;
}

/*ARGSUSED*/
void
set_cache_state(int cache_state)
{
	/* dummy */
}

void
fiximp()
{
	/* dummy */
}

/*ARGSUSED*/
void
post_mountroot(char *bootfile, char *redirect)
{

/*
 *  Redirection on install CD SHOULD go away with our
 *  new booting scheme.
 */
#ifdef	notdef
	int fd;
	static char bootrc[256] = "/etc/bootrc";
	extern	int read_redirect(char *redirect);

	/*
	 * If there's no /etc/bootrc file, then get cautious ..
	 */
	if ((fd = openfile(bootrc)) == -1) {

		/*
		 * There are several reasons why this might've
		 * happened .. but one of them is that we're
		 * on the installation CD, and we need to
		 * revector ourselves off to a different partition
		 * of the CD.  Check for the redirection file.
		 */
		if (redirect != NULL &&
		    read_redirect(redirect)) {
			return;
			/*NOTREACHED*/
		}
	} else
		(void) close(fd);
#endif

	/* Set up DOS emulation */
	dosemul_init();

	/* set a default PATH for boot shell run commands */
	(void) var_ops(PATHVAR, BSH_DEFAULT_RUNPATH, SET_VAR);

	/* Hand control over to the boot shell */
	bsh();
}

/*ARGSUSED1*/
static int
boot_open(char *pathname, void *arg)
{
	dprintf("trying '%s'\n", pathname);
	return (open(pathname, O_RDONLY));
}

/*
 * Open the given filename, expanding to its
 * platform-dependent location if necessary.
 *
 * Only one x86 platform currently.
 */
int
openfile(char *filename)
{
	static char fullpath[MAXPATHLEN];
	static char iarch[MAXPATHLEN];
	int fd;

	/*
	 * Exported as the 'mfg-name' boot property
	 */
	mfg_name = get_mfg_name();

	/*
	 * If the -I flag has been used, impl_arch_name will
	 * be specified .. otherwise we want to supply a buffer
	 * to open_platform_file() so that it can tell us what
	 * impl_arch_name was actually chosen so we can export that
	 * name via the 'impl-arch-name' boot property.
	 */
	if (impl_arch_name == NULL)
		impl_arch_name = iarch;

	fd = open_platform_file(filename, boot_open, NULL, fullpath,
	    impl_arch_name);

	if (fd == -1 || *impl_arch_name == '\0')
		return (-1);

	/*
	 * Copy back the name we actually found
	 */
	(void) strcpy(filename, fullpath);
	return (fd);
}

void
init_memlists()
{
	/*
	 * The memlists are already constructed with setup_memlists()
	 *   in memory.c.
	 * kmem_init() is done in bsetup.c.
	 */
}

void
setup_bootpath(char *bpath, char *bargs)
{
	/* BEGIN CSTYLED */
	/*
	 *  Boot-path assignment mechanism:
	 *
	 *  If we were booted from the old (pre 2.5) "blueboot" off
	 *  of an extended MDB device (i.e., one whose device code
	 *  is other than 0x80), then we assume that we are probably
	 *  installing a new system and no bootpath information exists
	 *  in the /etc/bootrc file.  In this case we generate
	 *  the boot path, by calling the compatboot_bootpath() routine.
	 *
	 *  If we were booted from the new (post 2.5) "strap.com"
	 *  (or if the boot device code is 0x80) we assume that we are
	 *  running a configured system (or one that will configure itself).
	 *  We do not generate a boot path in this case because a default
	 *  value will either be coming from the /etc/bootrc file,
	 *  from the plug-n-play device tree builder ("bootconf"),
	 *  or via user input.
	 *
	 *  NOTE: This code used to live in ".../uts/i86/promif/prom_boot.c",
	 *        but was moved here (and simplified somewhat) as part of the
	 *        version 5 boot changes.
	 *
	 *        Note also that we're assuming that the buffer supplied by
	 *        the caller is big enough to hold the bootpath.  This really
	 *        needs to be changed, either to have a length passed in or
	 *        to dynamically allocate the path and return it.
	 */
	/* END CSTYLED */
	extern struct pri_to_secboot *realp;

	if ((realp != 0) && (realp->bootfrom.ufs.boot_dev != 0) &&
	    (realp->bootfrom.ufs.boot_dev != 0x80)) {

		compatboot_bootpath(bpath);

	} else {
		/*
		 *  We don't have enough information to construct a bootpath
		 *  at this time.  Return a null string; the system will have
		 *  to get the bootpath from the /etc/bootrc file or the user
		 *  will have to type it in at the boot shell command line.
		 */
		*bpath = '\0';
	}

	/*
	 *  2/15/96 - timh
	 *	I believe we want the 1275 "bootpath" only to be set by
	 *  bootconf. This provides 2.6 kernels a means to determine if
	 *  it they are going to receive a real device tree.  I.E., they
	 *  would only be getting a real tree if 'bootpath' were set. I
	 *  commented out the line below in case we want to put it back
	 *  later when kernels shouldn't need to have to make such a
	 *  determination.
	 *
	 *  bsetprop(&bootops, "bootpath", bpath, 0, 0);
	 */
	(void) bsetprop(&bootops, "boot-path", bpath, 0, 0);
	v2_getargs(kernname, bargs);
}

/*
 * Given the boot path, use the redirection string to mutate the boot
 * path to the new device.
 */
void
redirect_boot_path(char **v2path_p, char *bpath, char *redirect)
{
	char slicec = *redirect;
	char *p = bpath + strlen(bpath);

	/*
	 * If the redirection character doesn't fall in this
	 * range, something went horribly wrong.
	 */
	if (!(('0' <= slicec && slicec <= '7') ||
	    ('a' <= slicec && slicec <= 'z'))) {
		printf("boot: bad redirection slice '%c'\n", slicec);
		return;
	}

	/*
	 * Fully qualified OpenBoot-style pathname.
	 */
	while (--p >= bpath && *p != '@' && *p != '/')
		if (*p == ':')
			break;
	if (*p++ == ':') {
		/*
		 * Convert slice number to partition 'letter'.
		 */
		*p++ = (slicec > '9') ?
			'k' + slicec - 'a' : 'a' + slicec - '0';
		*p = '\0';
		translate_tov2(v2path_p, bpath);
		return;
	}
	prom_panic("redirect_boot_path: mangled boot path!");
}
