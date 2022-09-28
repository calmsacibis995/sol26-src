/*
 * Copyright (c) 1995,1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)boot_plat.c	1.20	96/09/20 SMI"

#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/cpu.h>
#include <sys/obpdefs.h>
#include <sys/reboot.h>
#include <sys/promif.h>
#include <sys/stat.h>
#include <sys/bootvfs.h>
#include <sys/dktp/fdisk.h>
#include <sys/vtoc.h>
#include <sys/link.h>
#include <sys/elf.h>

#include <sys/platnames.h>
#include <sys/salib.h>

/*
 * boot_plat.c for PowerPC.
 * Contains following functions:
 *		fiximp()
 *		setup_bootargs()
 *		get_rootfs_start()
 *		translate_tov2()
 *		post_mountroot()
 *		openfile()
 *		getargs()
 *		cache_is_unified()
 */

#ifdef DEBUG
static int debug = 1;
#else DEBUG
static int debug = 0;
#endif DEBUG

#define	dprintf		if (debug)	printf

extern	int gets(char *);
extern	void boot_getargs(char *, char *);
extern	char *get_mfg_name(void);
extern	int (*readfile(int fd, int print))();
extern	char *makepath(char *, char *);
extern	char *kernname;			/* Default Kernel name */
extern	char *impl_arch_name;
extern	char *my_own_name;		/* client program name boot/inetboot */
extern	int boothowto;			/* boot options */
extern	int read_redirect(char *redirect);

unsigned long get_rootfs_start(char *);
int	cache_is_unified(void);
void 	exitto(int (*entrypoint)());
int	openfile(char *);

char		*mfg_name;		/* manufacturer name */
Elf32_Boot 	*elfbootvec;		/* ELF bootstrap vector */

int pagesize = PAGESIZE;
static int	root_slice;		/* slice number for root file system */

/*
 * start block of root partition relative to the
 * Solaris fdisk partition
 */
extern unsigned long 	unix_startblk;

/* these need to be fixed with memory allocation scheme */
int cache_state = 1;			/* Cache is turned on by default */
int vac;				/* ??? */
#ifdef	lint
char _end[1];			/* defined by the linker */
#endif	lint

int
cache_is_unified()
{
	dnode_t rnode, cpu;
	pstack_t *stk;
	int ret = 1;
	int unified_cache;
	dnode_t sp[OBP_STACKDEPTH];

	rnode = prom_rootnode();
	stk = prom_stack_init(sp, sizeof (sp));

	/* this needs to be changed for multi-processor systems */
	if ((cpu = prom_findnode_bydevtype(rnode, "cpu", stk))
	    == OBP_NONODE) {
		printf("Error getting cpu node\n");
		prom_stack_fini(stk);
		return (0);
	}
	if (prom_getintprop(cpu, "cache-unified", &unified_cache) == -1) {
		ret = 0;
	}

	prom_stack_fini(stk);

	return (ret);
}

void
translate_tov2(char **v2path, char *bpath)
{
	*v2path = bpath;
}

/*ARGSUSED*/
void
post_mountroot(char *bootfile, char *redirect)
{
	int (*go2)();
	int fd;
	char filename2[MAXPATHLEN];

	/* Save the bootfile, just in case we need it again */
	(void) strcpy(filename2, bootfile);

	for (;;) {
		if (boothowto & RB_ASKNAME) {
			char tmpname[MAXPATHLEN];

			printf("Enter filename [%s]: ", bootfile);
			(void) gets(tmpname);
			if (tmpname[0] != '\0')
				(void) strcpy(bootfile, tmpname);
		}

		if ((fd = openfile(bootfile)) == -1) {

#ifdef	lint
			(void) read_redirect(redirect);	/* XXX: 1242171 */
#endif	lint
			printf("%s: cannot open %s\n", my_own_name, bootfile);
			boothowto |= RB_ASKNAME;

			/* restore that which The User has taken away */
			(void) strcpy(bootfile, filename2);

			continue;
		}

		if ((go2 = readfile(fd, boothowto & RB_VERBOSE)) !=
		    (int(*)()) -1) {
			(void) close(fd);
		} else {
			printf("boot failed\n");
			boothowto |= RB_ASKNAME;
			continue;
		}

		if (boothowto & RB_HALT) {
			printf("Boot halted.\n");
			prom_enter_mon();
		}

		/* update (or create) some bootprops */
		my_own_name = bootfile;
		dprintf("Calling exitto(%x)\n", go2);

		exitto(go2);
	}
}

/* ARGSUSED1 */
static int
boot_open(char *pathname, void *arg)
{
	dprintf("trying '%s'\n", pathname);
	return (open(pathname, O_RDONLY));
}

/*
 * Open the given filename, expanding to it's
 * platform-dependent location if necessary.
 * Note, even though we don't expand absolute
 * pathnames, we go through the exercise of filling
 * in the mfg_name and impl_arch_name for the bootops.
 * Boot supports OpenFirmware.
 */
int
openfile(char *filename)
{
	static char fullpath[MAXPATHLEN];
	static char iarch[MAXPATHLEN];
	int	fd;

	/*
	 * Exported as the 'mfg-name' boot property.
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
setup_bootpath(char *bpath, char *bargs)
{
	int fd;
	dnode_t node;
	int len;
	/*
	 * 1115931 - strip options from network device types
	 * (So standalone can handle boot net:IPADDRESS.)
	 * We don't want to do this for non-network devices,
	 * otherwise we may strip disk partition information.
	 */

	/*
	 * Convert pathname to phandle, so we can get devicetype
	 */
	fd = prom_open(prom_bootpath());
	node = prom_getphandle(fd);
	(void) prom_close(fd);

	if (prom_devicetype(node, "network"))
		prom_strip_options(prom_bootpath(), bpath);
	else {
		/*
		 * the root-device configuration may be used to
		 * specify a device for the root partition other
		 * than the device booted from.  Note that this
		 * option is useful for debugging but is not
		 * fully supported.
		 */
		node = prom_finddevice("/options");
		len = prom_getproplen(node, "root-device");
		if (len == -1) {
			(void) strcpy(bpath, prom_bootpath());
			dprintf("root-device variable not set!\n");
		} else {
			(void) prom_getprop(node, "root-device", bpath);
			bpath[len] = '\0';
		}
		/*
		 * Used for file system reads  (see diskread())
		 * OpenFirmware does not understand
		 * the idea of a Solaris slice -
		 * it only knows about fdisk partitions - so
		 * unix_startblk is the offset from the beginning
		 * of the Solaris fdisk partition to the root slice.
		 * Note that bpath may be modified by
		 * get_rootfs_start().
		 */
		unix_startblk = get_rootfs_start(bpath);
	}
	boot_getargs(kernname, bargs);
}

/*
 * Find the starting block of the root slice in the Solaris
 * fdisk partition.  THis routine cracks the fdisk table,
 * finds the solaris fdisk partition, and finds the root
 * slice.
 * Note that it will modify the string 'device' to contain the
 * correct fdisk partition number device name.
 * We return the offset of the root slice in the Solaris
 * fdisk partition.
 */
unsigned long
get_rootfs_start(char *device)
{
	char disk_buf[2048];
	struct mboot *mp;
	struct ipart *ip, ipart[FD_NUMPART];
	struct dk_label *lp;
	struct dk_vtoc *dp;
	struct dkl_partition *pp;
	ihandle_t handle;
	int i;
	int len;
	char *dev;

	/*
	 * For now we will ignore any fdisk partition
	 * specified in boot path.  Just open the entire
	 * disk and find the SUNOS fdisk partition ourselves
	 */

	/*
	 * remove the fdisk partition number (if any) from
	 * device name.
	 * should have the form
	 * <device>:<fdisk partition #>
	 * If the fdisk partition number does exist, it
	 * will have to be the fdisk partition number
	 * of the DOS fs for us to have gotten this far.
	 */

	len = strlen(device);
	dev = &device[len];
	while ((*dev != ':') && (*dev != '/'))
		dev--;

	if (*dev != ':')
		prom_panic("ufsboot: no ':' in boot-path\n");

	/* append 0 (fdisk part #0) and null terminator */
	*(++dev) = '0';
	*(++dev) = '\0';

	dprintf("Disk device is '%s'\n", device);

	handle = prom_open(device);

	if (handle == (ihandle_t)0) {
		prom_panic("ufsboot: cannot open root-device\n");
	}

	/*
	 * Read in the first 2K.  This will contain the
	 * master boot record.
	 */
	if ((i = prom_read(handle, disk_buf, 2048, 0, 0)) < 2048) {
		prom_printf("ufsboot: read of partition table failed, "
			"expected 2048, got %d\n", i);
		prom_panic("ufsboot: failed to read FDISK table\n");
	}

	(void) prom_close(handle);

	mp = (struct mboot *)disk_buf;

	if (mp->signature != MBB_MAGIC) {
		prom_panic("ufsboot: fdisk sector "
		    "has incorrect magic number\n");
	}

	/* Note: the ipart array is unaligned on disk */
	bcopy((caddr_t)mp->parts, (caddr_t)ipart, sizeof (ipart));

	/*
	 * Loop thru the ipart structs looking for the
	 * Solaris fdisk partition number.
	 */
	for (i = 0, ip = ipart; i < FD_NUMPART && ip != NULL;
	    i++, ip++) {
		if (ip->systid == SUNIXOS)
			break;
	}
	if ((i == FD_NUMPART) || (ip == NULL)) {
		prom_panic("ufsboot: could not find Solaris partition"
		    " in fdisk table\n");
	}

	/* fix up the root-device with the correct fdisk part. # */
	len = strlen(device);
	device[len - 1] = (char)((int)'1' + i);

	dprintf("Solaris disk device is '%s'\n", device);

	/* Now open the Solaris fdisk partition and find the root slice. */
	handle = prom_open(device);

	if (handle == (ihandle_t)0) {
		prom_panic("ufsboot: could not open Solaris"
		    " partition\n");
	}
	/*
	 * read vtoc to determine start of Solaris Root Slice
	 * make sure we start reading on a 2K boundary
	 */
	if ((i = prom_read(handle, disk_buf, 2048,
	    ip->relsect + (DK_LABEL_LOC & ~3), 0)) < 2048) {
		prom_printf("ufsboot: short read on disk label: "
		    "expected 2048 actually returned %d bytes", i);
		prom_panic("ufsboot: read of disk label failed\n");
	}

	(void) prom_close(handle);

	/* get the disk label */
	lp = (struct dk_label *)((char *)disk_buf + 512 * (DK_LABEL_LOC % 4));
	dp = &lp->dkl_vtoc;

	if (lp->dkl_magic != DKL_MAGIC) {
		prom_panic("ufsboot: disk label has incorrect magic\n");
	}

	/* check the VTOC */
	if (dp->v_sanity != VTOC_SANE) {
		prom_panic("ufsboot: VTOC has incorrect magic number\n");
	}

	/*
	 * Find the root partition.
	 * Standalone root paritions are identified by the V_ROOT tag.
	 * Cache-Only-Client cache partitions are identified by the V_CACHE
	 * tag.
	 */
	for (pp = dp->v_part, i = 0; i < NDKMAP && pp != NULL;
		pp++, i++) {
		if (pp->p_tag == V_ROOT || pp->p_tag == V_CACHE)
			break;
	}

	/*
	 * Save the root slice number
	 * Default to partition 0 if we did not find a partition with
	 * the root or cache-only-client tag.
	 */
	if ((i == NDKMAP) || (pp == NULL)) {
		prom_printf("ufsboot: root slice not found in "
		    "Solaris partition. Defaulting to slice 0\n");
		pp = dp->v_part;
	}
	root_slice = pp - dp->v_part;

	/*
	 * return the offset of the root slice in the Solaris
	 * fdisk partition.
	 */
	return (pp->p_start);
}

/*
 * Dummy functions for making linker happy.
 */

void
fiximp(void)
{
}

void
silence_nets(void)
{
}

/* ARGSUSED */
void
setup_bootargs(char *bargs)
{
}

/* are these stubs ok ?? */
int
splnet(void)
{
	return (0);
}

/*ARGSUSED*/
int
splx(int level)
{
	return (0);
}

int
splimp(void)
{
	return (0);
}

/*
 * This will be moved to srt0.s after knowing about the parameters...
 */
extern struct bootops bootops;
static struct bootops *bopp;

extern int  is_cachefs_boot;
extern char *backfs_dev, *frontfs_dev;

static char *elx_after  = "elx@0,0:elx0";
static char *elx_before = "COMS,3C509";
extern void *p1275cif;
void
exitto(int (*entrypoint)())
{
	extern char *v2path;
	static char buffer[OBP_MAXPATHLEN];

	if (prom_devicetype(prom_getphandle(prom_open(prom_bootpath())),
		"network")) {
		int len;
		char *tmp;

		/*
		 * If the device is 3COM 3c509, then call it elx (hack),
		 * otherwise pass it through and hope it's a PCI device.
		 */

		tmp = prom_bootpath();
		dprintf("exitto: reading boot-path as: %s\n", tmp);
		(void) strcpy(buffer, tmp);
		prom_pathname(buffer);
		dprintf("exitto: canonical boot-path as: %s\n", buffer);
		tmp = buffer;
		len = strlen(tmp);
		tmp += len;	/* find end of string */
		while (*tmp != '/')
			tmp--;	/* find the start of the last component */
		tmp++;
		if (*tmp == *elx_before) {
			int prefix_len = tmp - buffer;	/* size of prefix  */
			while (*++tmp == *++elx_before)
				if (*elx_before == 0)
					break;
			if (*elx_before == 0) {	/* we have a match for elx */
				v2path = buffer;
				backfs_dev = buffer;
				tmp = v2path + prefix_len;
				do {
					*tmp++ = *elx_after++;
				} while (*elx_after);
				*tmp = 0;
			}
		}
		dprintf("exitto: boot-path is: %s\n", v2path);
	} else {
		/*
		 * Example prom_bootpath() is:
		 *
		 *	/pci/pci1000,1@1/disk@6,0:0,\solaris.elf
		 *
		 * This needs to be converted to a kernel boot-path property
		 * that is known to be pointed to by the global "v2path".
		 * Assuming that the root slice is slice 0, then the
		 * corresponding kernel boot-path should be:
		 *
		 *	/pci/pci1000,1@1/disk@6,0:a
		 *
		 * We do this transformation as follows:
		 *
		 *	Copy the path into a big buffer.
		 *	Replace what is after the last ":" with the Solaris
		 *	slice letter ("a" for alice 0, "b" for slice 1, etc.)
		 * 	Terminate the string.
		 *
		 * A special case is needed to deal with the case of the
		 * prom_bootpath() having no indication for slice and file.
		 * In this case there is no ":" following the last "/".
		 * The appropriate transformation is to append ":SLICELETTER".
		 */
		int len;
		char *tmp;

		tmp = prom_bootpath();
		dprintf("exitto: reading boot-path as: %s\n", tmp);
		len = strlen(tmp);
		(void) strcpy(buffer, tmp);

		/*
		 * Do the IDE hack here.  Convert:
		 *
		 *	/pci/pci1014,a@b/ide@i1f0/disk@0:,\solaris.elf
		 *
		 * to:
		 *
		 *	/pci/pci1014,a@b/ata@1f0,0/disk@0,0:,\solaris.elf
		 *
		 * The steps are:
		 *
		 *	1. find /ide@ in the string.  If it's not found,
		 *	   then make no change.
		 *	2. change ide to ata.
		 *	3. change ixxx to xxx.
		 *	4. change disk@N to disk@N,0
		 */
		{
			char *ide_before = "ide@";
			char *ide_tmp;

			for (tmp = buffer; ; ) {
				while (*tmp != 0 && *tmp != '/')
					tmp++;
				if (*tmp == 0)
					break;
				tmp++;
				for (ide_tmp = ide_before; *ide_tmp;
				    ide_tmp++, tmp++) {
					if (*ide_tmp != *tmp)
						break;
				}
				if (*ide_tmp == 0) {	/* matched ide@ */
					char c1, c2, c3, c4;
					tmp -= 4;	/* point to 'i' */
					*tmp++ = 'a';
					*tmp++ = 't';
					*tmp++ = 'a';
					tmp++;
					while (*(tmp + 1) != '/') {
						*tmp = *(tmp + 1);
						tmp++;
					}
					*tmp++ = ',';
					c2 = '0';
					while (c2 != '@') {
						c1 = *tmp;
						*tmp++ = c2;
						c2 = c1;
					}
					c1 = *tmp;
					*tmp++ = c2;
					c2 = c1;
					c1 = *tmp;
					*tmp++ = c2;
					c2 = c1;

					c4 = ',';
					c3 = '0';
					while (c4 != 0) {
						c1 = *tmp;
						*tmp++ = c4;
						c4 = c3;
						c3 = c2;
						c2 = c1;
					}
					*tmp = c4;
					break;
				}
			}
		}

		/* do the munging here */
		for (tmp = buffer + len; *tmp != ':' && *tmp != '/'; tmp--)
			/* empty */;	/* find last ":" or last "/" */
		if (*tmp == '/') {
			tmp = buffer + len;
			*tmp = ':';
		}
		*++tmp = 'a' + root_slice;
		*++tmp = 0;
		v2path = buffer;
		frontfs_dev = buffer;
		dprintf("exitto: we see boot-path as '%s'\n", v2path);
	}
	bopp = &bootops;
	(*entrypoint)(&p1275cif, 0, &bopp, elfbootvec);
}

#ifdef DEBUG
/*
 * For debugging only : Print device tree of the machine.
 */
void
show_device_tree()
{
	dnode_t node;
	int done = 0;
	char buff[100];
	dnode_t sp[OBP_STACKDEPTH];
	pstack_t *ps;

	node = prom_rootnode();
	ps = prom_stack_init(sp, sizeof (sp));

	do {
		while (node != 0) {
			if (ps->sp > ps->maxstack)
				printf("maxstack exceeded in showtree\n");
			*(ps->sp)++ = node;
			if (ps->sp > ps->maxstack)
				printf("maxstack exceeded in showtree\n");
			node = prom_childnode(node);
		}

		if (ps->sp > ps->minstack) {
			node = *(--ps->sp);
			/* print out the full path name */
			prom_phandle_to_path(node, buff, 100);
			printf("Pathname = %s\n", buff);
			(void) prom_getprop(node, "name", buff);
			printf("\tname = %s\n", buff);
			buff[0] = '\0';
			(void) prom_getprop(node, "compatible", buff);
			if (buff[0] == '\0')
				printf("\tcompatible = <none>\n");
			else
				printf("\tcompatible = %s\n", buff);
			printf("\n");
			node = prom_nextnode(node);
		} else
			done = 1;
	} while (!done);

	prom_stack_fini(ps);
}

#endif /* DEBUG */

/*
 * this needs to be implemented
 */
/* ARGSUSED */
void
redirect_boot_path(char **v2path_p, char *bpath, char *redirect)
{
}

void
translate_v2tov0(char *v2path, char *v0path)
{
	char *st1, *st2;
	char *cname;

	if ((cname = strstr(v2path, elx_after)) == NULL) {
		strcpy(v2path, v0path);
	} else {

		for (st1 = v2path, st2 = v0path; st1 != cname;
			*st2++ = *st1++);

		for (st1 = elx_before; *st1; *st2++ = *st1++);
		*st2 = '\0';
	}
}
