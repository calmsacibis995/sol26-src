
/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)rootfs_start.c 1.3     96/06/06 SMI"


#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/cpu.h>
#include <sys/obpdefs.h>
#include <sys/reboot.h>
#include <sys/promif.h>
#include <sys/stat.h>
#include <sys/dktp/fdisk.h>
#include <sys/vtoc.h>
#include <sys/link.h>
#include <sys/elf.h>
#include <cbootblk.h>

static u_long unix_start;

/*
 * Find the starting block of the root slice in the Solaris
 * fdisk partition.  This routine cracks the fdisk table,
 * finds the solaris fdisk partition, and finds the root
 * slice.
 */
void
get_rootfs_start(char *device)
{
	char disk_buf[2048];
	struct mboot mp;
	struct ipart *ip, ipart[FD_NUMPART];
	struct dk_label lp;
	struct dk_vtoc *dp;
	struct dkl_partition *pp;
	ihandle_t handle;
	int i;
	int len;
	char *dev;
	char p[16];

	/*
	 * For now we will ignore any fdisk partition
	 * specified in boot path.  Just open the entire
	 * disk and find the SUNOS fdisk partition ourselves
	 */

	/*
	 * remove the fdisk partition number (if any) from
	 * device name.
	 * should have the form
	 * <device> : <fdisk partition #>
	 */

	len = strlen(device);
	dev = &device[len];
	while ((*dev != ':') && (*dev != '/'))
		dev--;

	if (*dev != ':') {
		puts("bootblk: no ':' in boot-path\n");
		exit();
	}

	*(++dev) = '0';
	*(++dev) = '\0';

	handle = prom_open(device);

	if (handle == 0) {
		puts("bootblk: Cannot open root-device\n");
		exit();
	}

	if ((i = prom_read(handle, disk_buf, 2048, 0, 0)) < 2048) {
		puts("bootblk: read of partition table failed, ");
		puts("expected 2048, got ");
		(void) utox(p, i);
		puts(p);
		puts("\n");
		puts("bootblk: failed to read FDISK table\n");
		exit();
	}

	(void) prom_close(handle);

	bcopy((caddr_t)disk_buf, (caddr_t)&mp, sizeof (struct mboot));
	if (mp.signature != MBB_MAGIC) {
		puts("bootblk: fdisk sector has incorrect magic number\n");
		exit();
	}

	/* Note: the ipart array is unaligned on disk */
	bcopy((caddr_t)mp.parts, (caddr_t)ipart, sizeof (ipart));

	for (i = 0, ip = ipart; i < FD_NUMPART; i++, ip++) {
		if (ip->systid == SUNIXOS)
			break;
	}
	if (i == FD_NUMPART) {
		puts("bootblk: Could not find Solaris partition ");
		puts("in fdisk table\n");
		exit();
	}

	/* fix up the root-device with the correct fdisk part. # */
	len = strlen(device);
	device[len - 1] = (char)((int)'1' + i);

	handle = prom_open(device);

	/* read vtoc to determine start of Solaris Root Slice */
	if ((i = prom_read(handle, disk_buf, 2048,
	    ip->relsect + (DK_LABEL_LOC & ~3), 0)) < 2048) {
		puts("bootblk: Short read on disk label: expected 2048 ");
		puts("actually returned ");
		(void) utox(p, i);
		puts(p);
		puts(" bytes\n");
		puts("bootblk: Read of disk label failed\n");
		exit();
	}

	(void) prom_close(handle);

	/* get the disk label */
	bcopy((caddr_t)(disk_buf + 512 * (DK_LABEL_LOC % 4)), (caddr_t)&lp,
	    sizeof (struct dk_label));
	dp = &lp.dkl_vtoc;

	if (lp.dkl_magic != DKL_MAGIC) {
		puts("bootblk: disk label has incorrect magic\n");
		exit();
	}

	/* check the VTOC */
	if (dp->v_sanity != VTOC_SANE) {
		puts("bootblk: VTOC has incorrect magic number\n");
		exit();
	}

	/*
	 * find the root partition
	 * Standalone (diskfull) root partitions are tagged V_ROOT
	 * Cache-Only-Clients root and /usr cache partitions are
	 * tagged V_CACHE
	 */
	for (pp = dp->v_part, i = 0; i < NDKMAP; pp++, i++) {
		if (pp->p_tag == V_ROOT || pp->p_tag == V_CACHE)
			break;
	}

	/*
	 * Print a warning and default to partition 0 if we did
	 * not find a partition tagged with V_ROOT or V_CACHE
	 */
	if (i == NDKMAP) {
		puts("bootblk: No root partition found: defaulting to 0\n");
		pp = dp->v_part;
	}

	/*
	 * Used for file system reads  (see devbread())
	 * OpenFirmware does not understand the idea of a Solaris slice -
	 * it only knows about fdisk partitions - so
	 * unix_startblk is the offset from the beginning
	 * of the Solaris fdisk partition to the root slice.
	 */
	unix_start = pp->p_start;
}

u_int
fdisk2rootfs(u_int offset)
{
	return (offset + unix_start);
}
