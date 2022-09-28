/*
 * Copyright (c) 1992-1996, Sun Microsystems, Inc.  All Rights Reserved.
 */

#ident "@(#)disk.c	1.37	96/05/03 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/vtoc.h>
#include <sys/bootdef.h>
#include <sys/bootcmn.h>
#include <sys/booti386.h>
#include <sys/bootlink.h>
#include <sys/dev_info.h>
#include <sys/bootp2s.h>
#include <sys/sysmacros.h>
#include <sys/ihandle.h>
#include <sys/salib.h>
#include <sys/promif.h>

extern caddr_t rm_malloc(u_int, u_int, caddr_t);
extern void rm_free(caddr_t, u_int);
extern int doint(void);
extern void *memcpy(void *s1, void *s2, size_t n);
extern int bd_getalts(struct ihandle *ihp, int prt);
extern void bd_freealts(struct ihandle *ihp);
extern int bd_readalts(struct ihandle *ihp, daddr_t off, int cnt);
extern long strtol(char *, char **, int);

extern struct int_pb ic;
extern char *new_root_type;

int reset_disk(struct ihandle *ihp);

#define	cachesize(x) ((x)->dev.disk.bps * (x)->dev.disk.spt)

int SilentDiskFailures = 0;

int
find_disk(int *table, int n, char *path)
{
	/* BEGIN CSTYLED */
	/*
	 *  Open disk unit:
	 *
	 *    The "boot-interface" property for disks consists of an array of
	 *    triplets (plus a leading "type" word).  These triplets are in-
	 *    terpreted as entries in a table with the following format:
	 *
	 *       +---------------+---------------+--------------------+
	 *       |  target addr  |  unit number  |  bios drive number |
	 *       +---------------+---------------+--------------------+
	 *
	 *    This routine extracts the target and lun from the device "path"
	 *    and returns the bios drive number from the "n"-entry
	 *    boot-interface "table" that matches these extracted values.
	 *    Columns containing -1 will match extracted value (including no
	 *    extracted value).
	 *
	 *    This routine returns -1 if it can't match the target/lun specified
	 *    in the path name.
	 */
	/* END CSTYLED */
	if (!(n % sizeof (int)) && (((n /= sizeof (int)) % 3) == 1)) {
		/*
		 *  The boot-interface property length is reasonable, get
		 *  ready to search the target/lun table.
		 */
		char *cp = strrchr(path, '/');
		int tar = -1, lun = -1;

		if (cp = strrchr((cp ? cp : path), '@')) {
			/*
			 *  There's a unit address attached to the last
			 *  component of the path name.  Extract the target
			 *  and lun values from this addr.
			 *  We're not very forgiving here:  If either value
			 *  is missing the open will fail.
			 */
			tar = (int)strtol(cp+1, &cp, 0);
			if (!*cp || (*cp++ != ','))
				return (-1);
			lun = (int)strtol(cp, 0, 0);
		}

		for (n--; n > 0; (n -= 3, table += 3)) {
			/*
			 *  Now step thru the boot-interface table looking for
			 *  a target/lun pair that matches the values we
			 *  extracted from the unit address.
			 */
			if (((table[1] == -1) || (table[1] == tar)) &&
			    ((table[2] == -1) || (table[2] == lun))) {
				/*
				 *  This is it!  Return the bios drive number
				 *  field of the current boot-interface table
				 *  entry.
				 */
				return (table[3]);
			}
		}
	}

	return (-1);	/* bogus (or missing) unit address! */
}

static caddr_t
get_track_cache(int size, int unit)
{
	/*
	 * routine to take care of floppy alignment restricts.
	 * recursively rm_malloc a buffer until it doesn't cross
	 * a 64k boundary. works well because the floppy buffer
	 * is < 10k.
	 */

	caddr_t tp, rp;

	tp = rm_malloc(size, 0, 0);
	if (tp == 0)
		return (tp);	/* out of mem */
	if (unit > 1)
		return (tp);	/* hard disks are easy */

	/*
	 * it's a floppy - jump through hoops
	 * check we don't cross 64k boundary
	 */
	if (((unsigned)tp >> 16) != ((unsigned)(tp+size) >> 16)) {
		/*
		 * rats, try another buf
		 * don't free the first buf until after the
		 * alloc of another or we'll probably get
		 * the same one.
		 */
		rp = tp;
		tp = get_track_cache(size, unit);
		rm_free(rp, size);
	}
	return (tp);
}

int
open_disk(struct ihandle *ihp)
{
	/*
	 *  Open a disk device:
	 *
	 *  This routine issues a bios call to obtain disk geometry of the
	 *  specified "unit" and saves it in the indicated "ihandle" structure.
	 *  If necessary, we also search for the Solaris partition and set up
	 *  the slice offset indicated by the partition specifier (e.g, ":a")
	 *  in the "path" name.  Once we know the disk geometry, we can
	 *  initialize the corresponding sector buffers.
	 *
	 *  Returns 0 if it works, -1 if something goes wrong.
	 */
	int prt = strncmp(ihp->pathnm, BOOT_DEV_NAME, sizeof (BOOT_DEV_NAME)-1);
	char *path = ihp->pathnm;
	struct fdpt *fpp;
	int x;

	if (new_root_type && strcmp(new_root_type, "ufs")) {
		/* BEGIN CSTYLED */
		/*
		 *  Here's how "new_root_type" works:
		 *
		 *  1).  If it's null, it means that the caller doesn't know
		 *       what the file system type is and wants us to probe
		 *       for VTOCs and return the appropriate file type in
		 *       "new_root_type".
		 *
		 *  2).  If it points to a null string, it means that the
		 *       caller is doing a device-open and doesn't care what
		 *       the file system type is.
		 *
		 *  3).  If it points to anything else, it means the caller
		 *       expects to find a file system of the given type on
		 *       the device.
		 *
		 *  Now, if we happen to find a "ufs" file system on the disk,
		 *  we'll need to know what slice to open.  Right now, we're
		 *  defaulting to slice "a" (because "dev.disk.num" is zero),
		 *  but if the caller does not want a "ufs" file system, we'll
		 *  want to bypass the Solaris VTOC search.  We can do this by
		 *  setting "dev.disk.num" to -1.
		 */
		/* END CSTYLED */

		ihp->dev.disk.num = -1;
	}

	ic.intval = DEVT_DSK;	/* Get the disk geometry		*/
	ic.bx = ic.cx = 0;
	ic.dx = ihp->unit;
	ic.ax = 0x800;

	/*
	 * 9/8/95 - timh
	 *
	 *	I removed an additional check from the if statement that
	 * follows this comment.  It used to read:
	 *
	 *	if (!doint() && !(ic.ax & 0xFF00)) {}
	 *
	 * This unfortunately, is not backwards compatible with older BEFs,
	 * since they forgot to clear the AX register after int 13, fn 0x800.
	 * According to the Phoenix BIOS technical reference, AX should
	 * be zeroed upon return from this interrupt.
	 *
	 * That oversight has since been fixed in the framework, but we
	 * need to be able to support the older BEFs with this booter.
	 *
	 */
	if (!doint()) {
		/*
		 *  No error on "get disk geometry" call.  What we do now
		 *  depends on what type of disk we're using.
		 *
		 *  NOTE: This code assumes that the caller has cleared the
		 *  "ihandle" struct to zero before calling us.  This
		 *  happens automatically when the struct is
		 *  "rm_malloc"ed.
		 */

		if (ihp->unit > 1) {
			/*
			 *  For hard disks, basic geometry is returned in
			 *  registers, but we also have to initialize the
			 *  partition offset and size info.
			 */

			char *cp = strrchr(path, '/');

			ihp->dev.disk.bps = 512;
			ihp->dev.disk.spt = (u_short)((x = (ic.cx & 0x03F)) ?
			    x : 32);
			ihp->dev.disk.spc = (u_short)(ihp->dev.disk.spt *
			    (((ic.dx >> 8) & 0xFF)+1));

			if (!ihp->dev.disk.num &&
			    (cp = strchr((cp ? cp : path), ':'))) {
				/*
				 *  Solaris slice number is given by the
				 *  partition modifier of the last component
				 *  of the path name (if any).
				 */

				if ((ihp->dev.disk.num = cp[1] - 'a') >=
				    V_NUMPAR) {
					/*
					 *  If the partition number is bogus,
					 *  complain about it but default to
					 *  the 'a' partition.
					 */

					if (!SilentDiskFailures) {
						printf("%s: bogus partition,",
						    path);
						printf(" using \":a\"\n");
					}
					ihp->dev.disk.num = 0;
				}

			} else if (!new_root_type) {
				/*
				 *  If we're looking for a disk of "any" type,
				 *  but user hasn't specified a partition,
				 *  don't assume "ufs"!
				 */

				ihp->dev.disk.num = -1;
			}

		} else switch (ic.bx & 0xFF) {
			/*
			 *  For floppy disks, device type information is
			 *  returned in registers but geometry must be lifted
			 *  from the nvram address returned in %di.
			 */

		default:	/* This particular floppy is not installed!  */
			if (!SilentDiskFailures) {
				printf("%s: device not installed\n", path);
			}
			return (-1);

		case 1:	/* 360K,  40 track,  5.25 inch */
		case 2:	/* 1.2M,  80 track,  5.25 inch */
		case 3:	/* 720K,  80 track,  3.50 inch */
		case 4:	/* 1.4M,  80 track,  3.50 inch */
		case 5: /* 2.88M, 80 track,  3.50 inch (obscure IBM drive */
		case 6: /* 2.88M, 80 track,  3.50 inch */

			fpp = (struct fdpt *)segoftop(ic.es, ic.di);

			ihp->dev.disk.spt = fpp->spt;
			ihp->dev.disk.bps = 128 << fpp->secsiz;
			ihp->dev.disk.spc = ihp->dev.disk.spt * 2;

			if (!prt || !new_root_type) {
				/*
				 *  If caller wants to know the file system
				 *  type, we can assume it's pcfs!
				 */
				new_root_type = "pcfs";

			} else if (prt && *new_root_type &&
			    strcmp(new_root_type, "pcfs")) {
				/*
				 *  DOS file systems are all we support on
				 *  floppies, reject attempts to open anything
				 *  else!
				 */
				if (!SilentDiskFailures) {
					printf("%s: only supports pcfs "
					    "file system type\n",
					    path);
				}
				return (-1);
			}

			break;
		}

		/*
		 *  We used to send cachesize(ihp) as the size of the
		 *  track cache to allocate for the device.  That macro
		 *  calculates the track size = bps * spt. Unfortunately,
		 *  if we are using a floppy, the spt value may be modified
		 *  later (because the floppy might actually be written at
		 *  a lower density than the drive is capable of handling)
		 *  Then even later we'd free the cache using the cachesize
		 *  calculation and end up freeing too small of a buffer.
		 *
		 *  So now we calculate the max track size the drive is
		 *  capable of supporting and store the size in the device
		 *  ihandle structure.
		 */
		ihp->dev.disk.csize = cachesize(ihp);
		if (ihp->cachep =
		    get_track_cache(ihp->dev.disk.csize, ihp->unit)) {

			/*
			 *   We now have a cache large enough to hold a full
			 *   track's worth of data.  Mark the cache empty and
			 *   use "bd_getalts" to read in the vtoc and set the
			 *   alternate track map (if any).
			 *
			 *   NOTE: Cache must be contained within a 64kb
			 *   boundary to get around floppy DMA
			 *   limitiations!
			 */

			ihp->dev.disk.cyl = ((ic.cx>>8) & 0xFF) +
			    ((ic.cx<<2) & 0x300) + 1;
			ihp->dev.disk.siz = ihp->dev.disk.cyl *
				ihp->dev.disk.spc;
			ihp->cfirst = (unsigned)-1;

			if ((ihp->unit > 1) && bd_getalts(ihp, prt)) {
				/*
				 *  Mounting a hard disk is a bit tricky,
				 *  given that it may contain one of three
				 *  possible file system types: "pcfs",
				 *  "ufs", or "hsfs".  The "bd_getalts"
				 *  routine figures it all out and returns
				 *  zero if all is well.
				 */

				rm_free(ihp->cachep, ihp->dev.disk.csize);
				return (-1);
			}

			return (0);

		} else {
			/*
			 *  CDROMs have very big track buffers.  It's quite
			 *  possible to fail here because of this ...
			 */
			if (!SilentDiskFailures) {
				printf("%s: can't open - no memory\n", path);
			}
		}

	} else {
		/*
		 *  Bios doesn't recognize the device; assume we have a config-
		 *  uration error.
		 */

		if (!SilentDiskFailures) {
			printf("%s: can't open - bios configuration error\n",
			    path);
		}
	}

	return (-1);
}

void
close_disk(struct ihandle *ihp)
{
	/*
	 *  Close a disk device:
	 *
	 *  All we have to do here is free up the alternate track map and
	 *  the cache buffer we allocated at open.
	 */
	bd_freealts(ihp);
	rm_free(ihp->cachep, ihp->dev.disk.csize);
}

int
reset_disk(struct ihandle *ihp)
{
	ic.dx = ihp->unit;
	ic.intval = DEVT_DSK;
	ic.ax = ic.cx = 0;
	return (doint());
}

#ifdef	notdef
void
splatinfo(int fd)
{
	struct ihandle *ihp = devp(fd);

	if (ihp->type == DEVT_DSK) {
		printf("ihp is %x, fd is %d\n", ihp, fd);
		printf("type 0x%x, unit %d, ref %d\n",
		    ihp->type, ihp->unit, ihp->usecnt);
		printf("fstype %s. name %s. cachep %x. cfirst %x.\n",
		    ihp->fstype ? ihp->fstype : "NONE",
		    ihp->pathnm ? ihp->pathnm : "NONE",
		    ihp->cachep, ihp->cfirst);

		printf("%x, %x, %x, %x, %x, %x, %x, %x, %x\n",
		    ihp->dev.disk.alt,
		    ihp->dev.disk.siz,
		    ihp->dev.disk.par,
		    ihp->dev.disk.cyl,
		    ihp->dev.disk.bas,
		    ihp->dev.disk.bps,
		    ihp->dev.disk.spt,
		    ihp->dev.disk.spc,
		    ihp->dev.disk.num);
	}
}
#endif	/* notdef */

int
read_disk(struct ihandle *ihp, caddr_t buf, u_int len, u_int off)
{
	/*
	 *  Read from disk:
	 *
	 *  This routine reads "len" bytes into the specified "buf"fer, starting
	 *  at the given disk block "off"set.  Note that "len" is given in bytes
	 *  while "off" is given in sectors (the length of which must be taken
	 *  from the "ihandle" struct).  This is designed to keep everyone hope-
	 *  lessly confused!
	 *
	 *  Routine may perform a short read if the "off"set is near the end of
	 *  the partition/device.  The value returned is the number of bytes
	 *  read (or -1 if there's an error).
	 */
	int j, k, bc = len;
	off += ihp->dev.disk.par;

	if ((off + ((bc + ihp->dev.disk.bps - 1) / ihp->dev.disk.bps)) >
	    (ihp->dev.disk.par + ihp->dev.disk.siz)) {
		/*
		 *  Caller is trying to read past the end of the partition
		 *  (or disk, in the case of floppies).  Adjust the byte count
		 *  so that we only read up to the EOF mark.
		 */

		if ((bc = (int)((ihp->dev.disk.par + ihp->dev.disk.siz) -
		    off)) < 0) {
			/*
			 *  If adjusted length is negative, it means that the
			 *  caller has specified a bogus starting offset.
			 *  Deliver an error return!
			 */
			if (!SilentDiskFailures) {
				printf("%s: bogus sector number\n",
				    ihp->pathnm);
			}
			return (-1);
		}

		bc *= ihp->dev.disk.bps;	/* Convert sectors to bytes */
	}

	while (bc > 0) {
		/*
		 *  Main data transfer loop.  Copy data from the track cache
		 *  until we fall off the end or reach the caller's byte count.
		 *  Each time we empty the cache, re-load it from disk.
		 */
		if ((off < ihp->cfirst) ||
		    (off >= (ihp->cfirst+ihp->dev.disk.spt))) {
			/*
			 *  If requested sector is not already in the cache,
			 *  read it in now.  We try to read an entire track,
			 *  but if we're near the end of the partition/device,
			 *  we'll shorten this up a bit.  The fact that we've
			 *  already adjusted the "bc" register prevents us
			 *  from reading the unused portion of the cache when
			 *  this happens.
			 */
			daddr_t dad;

			dad = (daddr_t)(off/ihp->dev.disk.spt) *
			    ihp->dev.disk.spt;

			j = ihp->dev.disk.par + ihp->dev.disk.siz - dad;
			if (j > ihp->dev.disk.spt) j = ihp->dev.disk.spt;

			if ((j = bd_readalts(ihp, dad, j)) != -1) {
				/*
				 *  The "bd_readalts" routine calls
				 *  "read_sectors" (see below) after first
				 *  remapping any bad sectors in the request.
				 *  If we get an I/O error, it will return the
				 *  address of the failing sector so we can
				 *  print an error message and bail out with a
				 *  short read.
				 */
				if (!SilentDiskFailures) {
					printf("%s: disk read error, ",
					    ihp->pathnm);
					printf("sector %d\n", j);
				}
				return (-1);
			}

			ihp->cfirst = (unsigned)dad;
		}

		j = ihp->dev.disk.spt - (off - ihp->cfirst);
		k = MIN(j * ihp->dev.disk.bps, bc);

		(void) memcpy(buf,
		    ihp->cachep+((off-ihp->cfirst)*ihp->dev.disk.bps), k);
		off += j;
		buf += k;
		bc -= k;
	}

	return (len - bc);
}

/*
 * []------------------------------------------------------------[]
 *  | write_disk - This routine is called infrequently enough	|
 *  | that I'm taking the approach of simplistic code which	|
 *  | equals smaller code. This program is currently growing	|
 *  | without bounds and it must stop before we run out of memory|
 * []------------------------------------------------------------[]
 */
int
write_disk(struct ihandle *ihp, caddr_t buf, u_int len, u_int off)
{
	int	cyl,
		sec,
		head,
		retry = RD_RETRY;
	u_int	bc = len;
	paddr_t	lowmem = (paddr_t)rm_malloc(ihp->dev.disk.bps,
					    ihp->dev.disk.bps, 0);

	if (!lowmem) prom_panic("write_disk: failed to get low memory");

	len /= ihp->dev.disk.bps;
	while (len) {
		cyl = off / ihp->dev.disk.spc;
		sec = off % ihp->dev.disk.spc;
		head = sec / ihp->dev.disk.spt;
		sec -= head * ihp->dev.disk.spt - 1;

		/*
		 * need to use low mem because we don't know where buf
		 * is located.
		 */
		bcopy(buf, (caddr_t)lowmem, ihp->dev.disk.bps);

		ic.ax = 0x301;	/* ... write one sector at a time */
		ic.intval = DEVT_DSK;
		ic.dx = (head << 8) + ihp->unit;
		ic.es = segpart(lowmem); ic.bx = offpart(lowmem);
		ic.cx = ((cyl & 0xFF) << 8) | ((cyl >> 2) & 0xC0) |
		    (sec & 0x3f);

		if (doint()) {
			printf("(Write error: ax %x, mem 0x%x)",
			    ic.ax, lowmem);
			if (!retry--) {
				rm_free((caddr_t)lowmem,
					(size_t)ihp->dev.disk.bps);
				return (-1);
			}
		} else {
			len--;
			buf += ihp->dev.disk.bps;
			off++;
		}
	}

	rm_free((caddr_t)lowmem, (size_t)ihp->dev.disk.bps);
	/* ---- flush in entire track cache ---- */
	ihp->cfirst = (unsigned)-1;

	return (bc);
}

int
read_sectors(struct ihandle *ihp, daddr_t off, int cnt)
{
	/*
	 *  Read disk sectors:
	 *
	 *  This routine is called from "read_disk" whenever the track cache is
	 *  exhausted.  It reads "cnt" sectors into the track cache of the given
	 *  "ihandle" struct, starting at the specified sector "off"set.  It
	 *  returns -1 if it works, a failing sector number if there's an un-
	 *  recoverable read error.
	 */
	paddr_t buf = (paddr_t)ihp->cachep;
	int retry = RD_RETRY;
	int x, n = cnt;

	ihp->cfirst = (unsigned)-1;	/* Mark cache empty */

	while (n > 0) {
		/*
		 *  Main read loop.
		 *
		 *  Normally, we will only need a single iteration of this loop.
		 *  Multiple iterations occur when (a) there's an I/O error or
		 *  (b) the track cache straddles a 64KB boundary.
		 */

		int cyl = off / ihp->dev.disk.spc;
		int sec = off - (cyl * ihp->dev.disk.spc);
		int hed = sec / ihp->dev.disk.spt;

		sec -= ((hed * ihp->dev.disk.spt) - 1);

		if (ihp->unit <= 1) {
			/*
			 *  Floppy disks are unable to read a full track at a
			 *  time unless the request is aligned on a track
			 *  boundary.  If this isn't the case, truncate this
			 *  request to read up to the next track boundary and
			 *  read the rest on the next iteration of the loop.
			 */
			int q = ihp->dev.disk.spt - (off % ihp->dev.disk.spt);
			if (n > q) n = q;
		}

		ic.ax = 0x200 + n;
		ic.intval = DEVT_DSK;
		ic.dx = (hed << 8) + ihp->unit;
		ic.es = segpart(buf); ic.bx = offpart(buf);
		ic.cx = ((cyl & 0xFF) << 8) | ((cyl >> 2) & 0xC0) |
		    (sec & 0x3f);

		if (doint() && (((x = ((ic.ax >> 8) & 0xFF)) != ECC_COR_ERR))) {
			/*
			 *  I/O error of some sort.  We're prepared to
			 *  retry these up to "FD_RETRY" times ...
			 */
			if (!retry--) {
				/*
				 *  ... after which we deliver the failing
				 *  sector number so that caller can include it
				 *  in an error message.
				 */
				return (off);
			} else if (ihp->unit <= 1) {
				/*
				 *  If this is a floppy disk, retry the read
				 *  for a single sector.  This sometimes gives
				 *  better results than trying to re-read the
				 *  entire track (especially if there are two
				 *  or more bad sectors on the track).
				 */
				if (x & (I13_SEK_ERR+I13_TMO_ERR)) {
					/*
					 *  Our problem may be that the drive
					 *  hasn't come up to speed yet.  Issue
					 *  a "reset drive" command and see if
					 *  that helps any.
					 */
					(void) reset_disk(ihp);
				}

				if (n > 1) {
					/*
					 *  Our problem may be that we're
					 *  trying to read too many sectors
					 *  (the bios may not support full
					 *  track reads from floppy disk).
					 *  Let's cut the read size in half
					 *  and try again!
					 */
					n >>= 1;
					retry += 1;
				}
			}

		} else {
			/*
			 *  Read complete.  Update pointers & counters JIC we
			 *  take another trip thru the read loop.
			 */
			buf += (n * ihp->dev.disk.bps);
			retry = RD_RETRY;
			off += n;
			cnt -= n;
			n = cnt;
		}
	}

	return (-1);	/* NOTE: This is a successful return! */
}

int
is_floppy(fd)
{
	/*
	 *  Mini-fstat:  Returns a non-zero value if the device open on the
	 *  given file descriptor is a floppy disk.
	 */
	extern struct ihandle *open_devices[];
	struct ihandle *ihp = open_devices[fd];

	return ((fd >= 0) && (fd < MAXDEVOPENS) &&
	    (ihp != (struct ihandle *)0) && (ihp->type == DEVT_DSK) &&
	    (ihp->unit <= 1));
}

int
is_floppy0(fd)
{
	/*
	 *  Mini-fstat:  Returns a non-zero value if the device open on the
	 *  given file descriptor is floppy drive 0.
	 */
	extern struct ihandle *open_devices[];
	struct ihandle *ihp = open_devices[fd];

	return ((fd >= 0) && (fd < MAXDEVOPENS) &&
	    (ihp != (struct ihandle *)0) && (ihp->type == DEVT_DSK) &&
	    (ihp->unit == 0));
}

int
is_floppy1(fd)
{
	/*
	 *  Mini-fstat:  Returns a non-zero value if the device open on the
	 *  given file descriptor is floppy drive 1.
	 */
	extern struct ihandle *open_devices[];
	struct ihandle *ihp = open_devices[fd];

	return ((fd >= 0) && (fd < MAXDEVOPENS) &&
	    (ihp != (struct ihandle *)0) && (ihp->type == DEVT_DSK) &&
	    (ihp->unit == 1));
}
