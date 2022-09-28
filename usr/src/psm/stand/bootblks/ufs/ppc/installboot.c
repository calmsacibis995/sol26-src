/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)installboot.c	1.6	96/01/19 SMI"

#include <sys/types.h>
#include <sys/dktp/fdisk.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/fs/pc_label.h>

/*
 *	installboot (for PowerPC)
 *
 *	Requirements
 *
 *	    The result of running installboot is that the hard disk (argument
 *	    required) is made bootable, using scripts, programs, and data files
 *	    that exist on a running system.
 *
 *	Usage
 *
 *	    /usr/sbin/installboot [ -f openfirmware ] bootblk raw-disk-device
 *
 *	    Example arguments are:
 *
 *		bookblk			/usr/platform/prep/lib/fs/ufs/bootblk
 *		openfirmware		/platform/`uname -i`/openfirmware.x41
 *		raw-disk-device		/dev/rdsk/c0t6d0p0
 *
 *	Assumptions
 *
 *	    PowerPC bootable disks must have a DOS file system as specified
 *	    in the 1275 Open Firmware PowerPC Processor binding.  installboot
 *	    fails unless this is so.  The PCFS mount device is the Primary DOS
 *	    partition, which has a name of the form /dev/dsk/c?t?d?p0:c.  This
 *	    is the same as originally defined for x86.
 *
 *	    A PowerPC system that does not have Open Firmware is required to
 *	    have a PowerPC Boot (0x41) fdisk partition.
 *
 *	Algorithm (what installboot does)
 *
 *	    if raw-disk-device is not valid or if there is
 *	    no primary DOS partition in the fdisk table
 *		exit 3
 *	    Attempt to mount the primary DOS partition as PCFS.
 *	    If the mount fails,
 *		if there is a PCFS mkfs utility
 *		    run the utility on the primary DOS partition
 *		    if that fails
 *			exit 2
 *		else if the primary DOS partition is at least as big as 1.44Mb
 *		    run the internal copy of DOS mkfs (from fdformat)
 *		else
 *		    exit 2
 *		Attempt to mount the primary DOS partition as PCFS.
 *		If the mount fails,
 *		    exit 1
 *	    The "bootblk" argument is copied into the PCFS as "SOLARIS.ELF".
 *	    if the copy fails
 *		unmount the PCFS file system
 *		exit 1
 *
 *	    If the fdisk partition table defines a type 0x41 PowerPC Boot
 *	    partition, and the optional "-f openfirmware" argument is provided,
 *		install "openfirmware" onto the type 0x41 partition,
 *		which is just dd'ing the file onto the partition.
 *	    exit 0
 */

int pcfs_mkfs(char *);
int write_DOS_label(int);

void
usage(char *arg0)
{
	printf("usage: %s [ -f openfirmware ] bootblk raw-disk-device\n",
	    arg0);
	exit(1);
}

#ifdef debugging
int
ssystem(char *foo)
{
	printf("system: %s\n", foo);
	return (system(foo));
}

#define	system ssystem
#endif

/*
 *	scavenge through the fdisk table
 *	It's fatal if there is no primary DOS partition,
 *	but it's OK to not have a x41 partition, assuming
 *	there is real Open Firmware on the machine.
 *
 *	Note: partition #s are the index to the table, plus 1.
 */

int
read_fdisk(char *device, int *primary_dos, int *x41, int *dossize)
{
	int retval = 1;		/* assume failure */
	int i, type, fd;
	struct ipart part[FD_NUMPART];
	struct mboot mb;

	if ((fd = open(device, O_RDONLY)) == -1) {
		printf("cannot open %s\n", device);
		return (retval);
	}

	if (read(fd, &mb, sizeof (mb)) != sizeof (mb))
		goto cloze;

	memcpy(part, mb.parts, sizeof (part));

	for (i = 0; i < FD_NUMPART; i++) {
		type = part[i].systid;
		if (type == DOSOS12 || type == DOSOS16 || type == DOSHUGE) {
			*primary_dos = i + 1;
			*dossize = part[i].numsect;
			retval = 0;	/* success */
		} else if (type == PPCBOOT) {
			*x41 = i + 1;
		}
	}
cloze:
	(void) close(fd);
	return (retval);
}

/*
 *	Validate device argument.
 *	Create all the variations of the device string.
 *	(the list is declared just prior to the function definition.
 */

char *mntdev;		/* PCFS mount device */
char *dosdev;		/* partition device, used for PCFS creation */
char *x41dev;		/* partition device, used for openfirmware */
int dossectors;		/* size of PCFS partition device, in sectors */

int
make_device_strings(char *device)
{
	char *rawp0, *prefix, *disk, *tmp, *p;
	int x41_part, primary_dos_part, size;

	tmp = strdup(device);
	size = strlen(device);	/* determine device string size */
	p = &tmp[size];
	size += 20;		/* guarantee no overflow */

	/*
	 * break up the copy of the argument "tmp" into components:
	 *
	 *	prefix		/dev/		(not used)
	 *	middle		dsk or rdsk
	 *	disk		/c?t?d? or /c?d?
	 *	postfix		p0 or whatever	(not used)
	 */

	while (*p != 'd' && p > tmp)
		p--;
	if (p <= tmp)
		return (1);
	*(p + 2) = 0;	/* terminate the "disk" component */
	while (*p != '/' && p > tmp)
		p--;
	if (p <= tmp)
		return (1);
	disk = p;

	p--;
	while (*p != '/' && p > tmp)
		p--;
	if (p <= tmp)
		return (1);
	*(p + 1) = 0;	/* terminate the "prefix" component */
	prefix = tmp;

	x41_part = -1;
	rawp0 = (char *) malloc(size);
	sprintf(rawp0, "%s%s%s%s", prefix, "rdsk", disk, "p0");
	if (read_fdisk(rawp0, &primary_dos_part, &x41_part, &dossectors))
		return (3);
	if (x41_part != -1) {
		x41dev = (char *) malloc(size);
		sprintf(x41dev, "%s%s%s%s%d", prefix, "dsk", disk, "p",
		    x41_part);
	}

	mntdev = (char *) malloc(size);
	sprintf(mntdev, "%s%s%s%s", prefix, "dsk", disk, "p0:c");

	dosdev = (char *) malloc(size);
	sprintf(dosdev, "%s%s%s%s%d", prefix, "dsk", disk, "p",
	    primary_dos_part);

	return (0);
}

/*
 * The logic of this was grabbed from cmd/fs.d/pcfs/ident/ident_pcfs.c.
 *
 * We assume it's a pcfs file system iff:
 *	The "media type" descriptor in the label == the media type
 *		descriptor that's supposed to be the first byte
 *		of the FAT.
 *	The second byte of the FAT is 0xff.
 *	The third byte of the FAT is 0xff.
 *
 *	Assumptions:
 *
 *	1.	I don't really know how safe this is, but it is
 *	mentioned as a way to tell whether you have a dos disk
 *	in my book (Advanced MSDOS Programming, Microsoft Press).
 *	Actually it calls it an "IBM-compatible disk" but that's
 *	good enough for me.
 *
 * 	2.	The FAT is right after the reserved sector(s), and both
 *	the sector size and number of reserved sectors must be gotten
 *	from the boot sector.
 */

int
obviously_not_pcfs(char *dosdev)
{
	int fd;
	u_char	pc_stuff[PC_SECSIZE * 4];
	uint_t	fat_off;
	int	retval = 1;

	if ((fd = open(dosdev, O_RDONLY)) == -1) {
		printf("Could not open Primary DOS partition, errno = %d\n",
		    errno);
		goto dun;
	}

	/* read the boot sector (plus some) */
	if (read(fd, pc_stuff, sizeof (pc_stuff)) < 0) {
		perror("pcfs read");	/* should be able to read 4 sectors */
		goto dun;
	}

	/* no need to go farther if magic# is wrong */
	if ((*pc_stuff != (uchar_t)DOS_ID1) &&
	    (*pc_stuff != (uchar_t)DOS_ID2a)) {
		goto dun;	/* magic# wrong */
	}

	/* calculate where FAT starts */
	fat_off = ltohs(pc_stuff[PCB_BPSEC]) * ltohs(pc_stuff[PCB_RESSEC]);

	/* if offset is too large we probably have garbage */
	if (fat_off >= sizeof (pc_stuff)) {
		goto dun;	/* FAT offset out of range */
	}

	if ((pc_stuff[PCB_MEDIA] == pc_stuff[fat_off]) &&
	    ((uchar_t)0xff == pc_stuff[fat_off + 1]) &&
	    ((uchar_t)0xff == pc_stuff[fat_off + 2])) {
		retval = 0;
	}

dun:
	(void) close(fd);
	return (retval);
}

void
unmountpcfs(char *mountpoint)
{
	char sh[512];		/* buffer */

	sprintf(sh, "/usr/sbin/umount %s > /dev/null 2>&1", mountpoint);
	(void) system(sh);
}

int
mountpcfs(char *mountdev, char *mountpoint)
{
	char sh[512];		/* buffer */

	sprintf(sh, "/usr/sbin/mount -F pcfs %s %s > /dev/null 2>&1",
	    mountdev, mountpoint);
	return (system(sh));
}

int
copyfile(char *bootblk, char *mountpoint, char *file)
{
	struct stat buf;
	char sh[512];		/* buffer */


	/*
	 * Make sure the new bootblk exists before unlinking
	 * the old one.
	 */
	sprintf(sh, "%s", bootblk);
	if (stat(sh, &buf) == -1) {
		printf("cannot access %s\n", sh);
		return (-1);
	}
	sprintf(sh, "%s/%s", mountpoint, file);
	(void) unlink(sh);
	sprintf(sh, "/usr/bin/cp %s %s/%s", bootblk, mountpoint, file);
	return (system(sh));
}

int
dd_file(char *src, char *dst)
{
	char sh[512];		/* buffer */

	sprintf(sh, "/usr/bin/dd if=%s of=%s conv=sync 2>&1 | "
	    "grep -v ' records '", src, dst);
	return (system(sh));
}

main(int argc, char **argv)
{
	char *mountpoint;
	char *device;
	int c;
	extern int optind;
	extern char *optarg;
	char *bootblk;		/* argument */
	char *openfirmware;	/* optional argument */


	openfirmware = (char *)0;
	while ((c = getopt(argc, argv, "f:")) != -1)
		switch (c) {
		case 'f':
			openfirmware = optarg;
			break;
		case '?':
			usage(argv[0]);
		}

	if (optind + 2 != argc)
		usage(argv[0]);

	bootblk = argv[optind];
	device = argv[optind + 1];

	if (make_device_strings(device) != 0) {
		printf("'device' argument (%s) is not /dev/rdsk/c?t?d?p0\n",
		    device);
		exit(3);
	}

	/* run the algorthm described at the start of this file */

	mountpoint = tempnam("/tmp", "mntpcfs");
	if (mkdir(mountpoint, 0777) == -1) {
		printf("could not create directory for PCFS mount point\n");
		exit(1);
	}

	if (obviously_not_pcfs(dosdev) ||
	    (mountpcfs(mntdev, mountpoint) != 0)) {

		/* less that size of empty PCFS file system image? */
		if (dossectors < 2880) {
			printf("Primary DOS partition is too small,"
			    " %d sectors is less than 1.44Mb\n");
			exit(2);
		}
		if (pcfs_mkfs(dosdev) != 0) {
			printf("could not create DOS file system\n");
			exit(2);
		}
		if (mountpcfs(mntdev, mountpoint) != 0) {
			printf("could not mount DOS file system\n");
			exit(1);
		}
	}

	if (copyfile(bootblk, mountpoint, "SOLARIS.ELF") != 0) {
		unmountpcfs(mountpoint);
		exit(1);
	}
	unmountpcfs(mountpoint);
	(void) rmdir(mountpoint);

	/* Even if the following fails, "installboot" still succeeds */
	if (x41dev != 0 && openfirmware != 0)
		(void) dd_file(openfirmware, x41dev);

	return (0);
}

/* XXX - need to call real mkfs command for pcfs someday */

int
pcfs_mkfs(char *dosdev)
{
	int fd;

	if ((fd = open(dosdev, O_RDWR)) == -1) {
		printf("Could not open Primary DOS partition, errno = %d\n",
		    errno);
		return (1);
	}
	if (write_DOS_label(fd) != 0) {
		(void) close(fd);
		return (1);
	}
	(void) close(fd);
	return (0);
}

/*
 * NOTE: this is stolen from fdformat.c, almost verbatum.
 *
 * The following is a copy of MS-DOS 3.3 boot block.
 * It consists of the BIOS parameter block, and a disk
 * bootstrap program.
 *
 * The BIOS parameter block contains the right values
 * for the 3.5" high-density 1.44MB floppy format.
 *
 */
char bootloadr[512] = {
	0xeb, 0x34, 0x90,	/* 8086 short jump + displacement + NOP */
	'M', 'S', 'D', 'O', 'S', '3', '.', '3',	/* OEM name & version */
	0, 2, 1, 1, 0,		/* Start of BIOS parameter block */
	2, 224, 0, 0x40, 0xb, 0xf0, 9, 0,
	18, 0, 2, 0, 0, 0,	/* End of BIOS parameter block */
	0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x12,
	0x0, 0x0, 0x0, 0x0,
	0x1, 0x0, 0xfa, 0x33,	/* 0x34, start of the bootstrap. */
	0xc0, 0x8e, 0xd0, 0xbc, 0x0, 0x7c, 0x16, 0x7,
	0xbb, 0x78, 0x0, 0x36, 0xc5, 0x37, 0x1e, 0x56,
	0x16, 0x53, 0xbf, 0x2b, 0x7c, 0xb9, 0xb, 0x0,
	0xfc, 0xac, 0x26, 0x80, 0x3d, 0x0, 0x74, 0x3,
	0x26, 0x8a, 0x5, 0xaa, 0x8a, 0xc4, 0xe2, 0xf1,
	0x6, 0x1f, 0x89, 0x47, 0x2, 0xc7, 0x7, 0x2b,
	0x7c, 0xfb, 0xcd, 0x13, 0x72, 0x67, 0xa0, 0x10,
	0x7c, 0x98, 0xf7, 0x26, 0x16, 0x7c, 0x3, 0x6,
	0x1c, 0x7c, 0x3, 0x6, 0xe, 0x7c, 0xa3, 0x3f,
	0x7c, 0xa3, 0x37, 0x7c, 0xb8, 0x20, 0x0, 0xf7,
	0x26, 0x11, 0x7c, 0x8b, 0x1e, 0xb, 0x7c, 0x3,
	0xc3, 0x48, 0xf7, 0xf3, 0x1, 0x6, 0x37, 0x7c,
	0xbb, 0x0, 0x5, 0xa1, 0x3f, 0x7c, 0xe8, 0x9f,
	0x0, 0xb8, 0x1, 0x2, 0xe8, 0xb3, 0x0, 0x72,
	0x19, 0x8b, 0xfb, 0xb9, 0xb, 0x0, 0xbe, 0xd6,
	0x7d, 0xf3, 0xa6, 0x75, 0xd, 0x8d, 0x7f, 0x20,
	0xbe, 0xe1, 0x7d, 0xb9, 0xb, 0x0, 0xf3, 0xa6,
	0x74, 0x18, 0xbe, 0x77, 0x7d, 0xe8, 0x6a, 0x0,
	0x32, 0xe4, 0xcd, 0x16, 0x5e, 0x1f, 0x8f, 0x4,
	0x8f, 0x44, 0x2, 0xcd, 0x19, 0xbe, 0xc0, 0x7d,
	0xeb, 0xeb, 0xa1, 0x1c, 0x5, 0x33, 0xd2, 0xf7,
	0x36, 0xb, 0x7c, 0xfe, 0xc0, 0xa2, 0x3c, 0x7c,
	0xa1, 0x37, 0x7c, 0xa3, 0x3d, 0x7c, 0xbb, 0x0,
	0x7, 0xa1, 0x37, 0x7c, 0xe8, 0x49, 0x0, 0xa1,
	0x18, 0x7c, 0x2a, 0x6, 0x3b, 0x7c, 0x40, 0x38,
	0x6, 0x3c, 0x7c, 0x73, 0x3, 0xa0, 0x3c, 0x7c,
	0x50, 0xe8, 0x4e, 0x0, 0x58, 0x72, 0xc6, 0x28,
	0x6, 0x3c, 0x7c, 0x74, 0xc, 0x1, 0x6, 0x37,
	0x7c, 0xf7, 0x26, 0xb, 0x7c, 0x3, 0xd8, 0xeb,
	0xd0, 0x8a, 0x2e, 0x15, 0x7c, 0x8a, 0x16, 0xfd,
	0x7d, 0x8b, 0x1e, 0x3d, 0x7c, 0xea, 0x0, 0x0,
	0x70, 0x0, 0xac, 0xa, 0xc0, 0x74, 0x22, 0xb4,
	0xe, 0xbb, 0x7, 0x0, 0xcd, 0x10, 0xeb, 0xf2,
	0x33, 0xd2, 0xf7, 0x36, 0x18, 0x7c, 0xfe, 0xc2,
	0x88, 0x16, 0x3b, 0x7c, 0x33, 0xd2, 0xf7, 0x36,
	0x1a, 0x7c, 0x88, 0x16, 0x2a, 0x7c, 0xa3, 0x39,
	0x7c, 0xc3, 0xb4, 0x2, 0x8b, 0x16, 0x39, 0x7c,
	0xb1, 0x6, 0xd2, 0xe6, 0xa, 0x36, 0x3b, 0x7c,
	0x8b, 0xca, 0x86, 0xe9, 0x8a, 0x16, 0xfd, 0x7d,
	0x8a, 0x36, 0x2a, 0x7c, 0xcd, 0x13, 0xc3, '\r',
	'\n', 'N', 'o', 'n', '-', 'S', 'y', 's',
	't', 'e', 'm', ' ', 'd', 'i', 's', 'k',
	' ', 'o', 'r', ' ', 'd', 'i', 's', 'k',
	' ', 'e', 'r', 'r', 'o', 'r', '\r', '\n',
	'R', 'e', 'p', 'l', 'a', 'c', 'e', ' ',
	'a', 'n', 'd', ' ', 's', 't', 'r', 'i',
	'k', 'e', ' ', 'a', 'n', 'y', ' ', 'k',
	'e', 'y', ' ', 'w', 'h', 'e', 'n', ' ',
	'r', 'e', 'a', 'd', 'y', '\r', '\n', '\0',
	'\r', '\n', 'D', 'i', 's', 'k', ' ', 'B',
	'o', 'o', 't', ' ', 'f', 'a', 'i', 'l',
	'u', 'r', 'e', '\r', '\n', '\0', 'I', 'O',
	' ', ' ', ' ', ' ', ' ', ' ', 'S', 'Y',
	'S', 'M', 'S', 'D', 'O', 'S', ' ', ' ',
	' ', 'S', 'Y', 'S', '\0', 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0x55, 0xaa
};

struct bios_param_blk {
	u_char	b_bps[2];		/* bytes per sector */
	u_char  b_spcl;			/* sectors per alloction unit */
	u_char  b_res_sec[2];		/* reserved sectors, starting at 0 */
	u_char  b_nfat;			/* number of FATs */
	u_char  b_rdirents[2];		/* number of root directory entries */
	u_char  b_totalsec[2];		/* total sectors in logical image */
	char    b_mediadescriptor;	/* media descriptor byte */
	u_char	b_fatsec[2];		/* number of sectors per FAT */
	u_char	b_spt[2];		/* sectors per track */
	u_char	b_nhead[2];		/* number of heads */
	u_char	b_hiddensec[2];		/* number of hidden sectors */
};

#define	getlobyte(A)	(A & 0xFF)
#define	gethibyte(A)	(A >> 8 & 0xFF)

int
write_DOS_label(int fd)
{
	int	i;
	size_t fat_bsize;
	u_short totalsec;
	u_char	*fat_rdir;
	int	rdirsec = 224;
	struct bios_param_blk bpblock, *bpb;
	int ncyl;
	int nhead;
	int secptrack;

	ncyl = 80;
	nhead = 2;
	secptrack = 18;

	bpb = &bpblock;
	bpb->b_spcl = 1;
	bpb->b_mediadescriptor = (char)0xF8;
	bpb->b_fatsec[0] = 9;
	bpb->b_fatsec[1] = 0;
	bpb->b_nfat = 2;

	bpb->b_bps[0] = getlobyte(512);
	bpb->b_bps[1] = gethibyte(512);
	/* MS-DOS 5.0 supports only 1 reserved sector :-( */
	bpb->b_res_sec[0] = 1;
	bpb->b_res_sec[1] = 0;

	totalsec = ncyl * nhead * secptrack;
	bpb->b_totalsec[0] = getlobyte(totalsec);
	bpb->b_totalsec[1] = gethibyte(totalsec);
	bpb->b_spt[0] = (u_char)secptrack;
	bpb->b_spt[1] = (u_char)0;
	bpb->b_nhead[0] = (u_char)nhead;
	bpb->b_nhead[1] = (u_char)0;
	bpb->b_hiddensec[0] = 0;
	bpb->b_hiddensec[1] = 0;

	bpb->b_rdirents[0] = rdirsec & 0xff;
	bpb->b_rdirents[1] = (rdirsec >> 8) & 0xff;

	(void) memcpy((char *)(bootloadr + 0x0B), (char *)bpb,
					sizeof (struct  bios_param_blk));

	if (write(fd, bootloadr, 512) != 512) {
		printf("write of DOS boot sector failed\n");
		return (3);
	}

	fat_bsize = 512 * bpb->b_fatsec[0];
	fat_rdir = (u_char *)malloc(fat_bsize);
	(void) memset(fat_rdir, (char)0, fat_bsize);

	*fat_rdir = bpb->b_mediadescriptor;
	*(fat_rdir + 1) = 0xFF;
	*(fat_rdir + 2) = 0xFF;
	for (i = 0; i < (int)bpb->b_nfat; ++i)
		if (write(fd, fat_rdir, fat_bsize) != fat_bsize) {
			printf("write of DOS File Allocation Table failed\n");
			return (1);
		}
	rdirsec = bpb->b_rdirents[0];
	rdirsec = 32 * (int) rdirsec / 512;

	(void) memset(fat_rdir, (char)0, 512);
	for (i = 0; i < (int)rdirsec; ++i) {
		if (write(fd, fat_rdir, 512) != 512) {
			printf("write of DOS root directory failed\n");
			return (1);
		}
	}
	return (0);
}
