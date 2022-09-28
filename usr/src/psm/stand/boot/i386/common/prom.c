/*
 * Copyright (c) 1992-1996, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ident "@(#)prom.c	1.26	96/06/23 SMI"

#include <sys/types.h>
#include <sys/promimpl.h>
#include <sys/bootcmn.h>
#include <sys/booti386.h>
#include <sys/bootlink.h>
#include <sys/bootconf.h>
#include <sys/dev_info.h>
#include <sys/bootp2s.h>
#include <sys/bootdef.h>
#include <stdarg.h>
#include <sys/ramfile.h>
#include <sys/dosemul.h>
#include <sys/ihandle.h>
#include <sys/salib.h>

#include "devtree.h"

static struct ihandle stdio  = { DEVT_SER, 0, 1, "<stdio>" };
struct ihandle *open_devices[MAXDEVOPENS] = { &stdio };

extern struct pri_to_secboot *realp;
extern struct dnode *active_node;
extern struct bootops bootops;
extern char  *new_root_type;
extern struct int_pb ic;
extern int BootDev;
extern int BootDevType;

extern struct real_regs	*alloc_regs(void);
extern struct dnode *find_node(char *, struct dnode *);
extern ushort bcd_to_bin(ushort);
extern long strtol(char *, char **, int);
extern void free_regs(struct real_regs *);
extern void *memset(void *s, int c, size_t n);
extern int doint(void);
extern int doint_asm();
extern int bgetproplen(struct bootops *, char *, phandle_t);
extern int bgetprop(struct bootops *, char *, caddr_t, int, phandle_t);
extern int getchar();
extern void putchar();
extern int reset_disk();
extern int goany(void);
extern void bootabort(void);
extern paddr_t map_mem(u_int, u_int, int);
extern int macaddr_net(struct ihandle *ihp, unsigned char *eap);

int OpenCount = 0;

/*ARGSUSED*/
static int
read_stdin(struct ihandle *ihp, char *buf, int len)
{
	/*
	 *  Read from stdin:
	 *
	 *  Fills the specified "buf"fer from the input device until "len" bytes
	 *  are transferred or a newline is read.  Returns the number of char-
	 *  acters read.
	 */

	int cnt = 0;

	while (cnt < len) {
		/*
		 *  Read up to "len" bytes from standard input.  The "cnt"
		 *  register tells us how many bytes we've read already.
		 */
		cnt += 1;

		if ((*buf++ = getchar()) == '\n') {
			/*
			 *  If the next input character is a line terminator,
			 *  break loop and return a short read.
			 */
			break;
		}
	}

	return (cnt);
}

/*ARGSUSED*/
static int
write_stdout(struct ihandle *ihp, char *buf, int len)
{
	/*
	 *  Write to stdout:
	 *
	 *  Writes the contents of the specified "buf"fer to the output device.
	 *  Returns the number of bytes written ("len" argument).
	 */
	int cnt = len;

	while (cnt-- > 0) {
		/*
		 *  Use the low-level "putchar" routine to write the next byte.
		 */
		putchar(*buf++);
	}

	return (len);
}

int
prom_open(char *path)
{
	/*
	 *  Open a device:
	 *
	 *  Returns the device handle (file descriptor), or 0 if it fails.
	 *  If the device we're looking for is a disk, we also probe for
	 *  the geometry (and maybe the partition info) and save this stuff
	 *  in the ihandle struct gets bound to the file descriptor.
	 */
	int OpenError = OpenCount;
	struct ihandle *ihp;
	char *cp, *xp = 0;
	int j, k = 0;

	int dev_type = BootDevType;
	int dev_addr = BootDev;
	int dev_part = 0;

	if (realp != 0) {
		/*
		 *  If we were booted by the old (pre 2.5) "blueboot", the
		 *  default device type and address are stored in the
		 *  "bootfrom" field of primary/secondary boot communication
		 *  structure.
		 */
		BootDev = dev_addr = realp->bootfrom.ufs.boot_dev;
		dev_part = realp->bootfrom.ufs.root_slice;
		BootDevType = dev_type = (realp->F8.dev_type == MDB_NET_CARD) ?
		    DEVT_NET : DEVT_DSK;

		realp = 0;	/* We don't need this any more! */
		path = 0;	/* Don't care what the path was */
	}

	if (!path || !*path) {
		/*
		 *  An incomplete pathname implies the boot device.  Use a
		 *  dummy path name for this device which is slightly different
		 *  from the BOOT_DEV_NAME.  This lets us have two file
		 *  descriptors open on the device, one for the DOS file
		 *  system, one for the ufs system.
		 */
		static char dummy_path[sizeof (BOOT_DEV_NAME)+4];
		(void) sprintf(dummy_path, "%s:a", BOOT_DEV_NAME);
		path = dummy_path;
	}

	for (cp = strrchr(path, 0); cp; xp = cp) {
		/*
		 *  Search for the "boot-interface" property for this device.
		 *  This property tells us how to perform I/O to the
		 *  corresponding device and may be attached to any node in the
		 *  device path.  We have to search for it in a bottom-up
		 *  fashion (i.e, looking at the rightmost pathname component
		 *  first).
		 */
		struct dnode *dnp;
		int n;

		*cp = '\0';
		dnp = find_node(path, active_node);

		if (dnp &&
		    ((n = bgetproplen(&bootops, "boot-interface", dnp)) >=
		    (int)sizeof (int))) {
			/*
			 *  We have a "boot-interface" property, but we don't
			 *  know whether or not it's legal.  The first word of
			 *  the property value gives the device type ...
			 */
			int *ip = (int *)bkmem_alloc(n);

			if (xp) *xp = '/';
			if (ip == (int *)0) {
				printf("can't get bytes for boot-interface\n");
				return (0);
			}
			(void) bgetprop(&bootops, "boot-interface", (caddr_t)ip,
			    n, dnp);

			switch (dev_type = *ip) {
				/*
				 *  Parse remainder of the the "boot-interface"
				 *  property according to the device type code
				 *  appearing in the first word.
				 */
				case DEVT_DSK:
				    dev_addr = find_disk(ip, n, path);
				    break;
				case DEVT_NET:
				    dev_addr = find_net(ip, n, path);
				    break;
				default:
				    dev_addr = -1;
				    break;
			}

			/*
			 *  Free up the property buffer and check result of
			 *  device parse.  If "dev_addr" is -1, it means the
			 *  parse fails and we should deliver an error.
			 */
			bkmem_free((char *)ip, n);
			if (dev_addr == -1) goto erx;
			OpenError = 0;
			break;

		/*
		 *  Below are two new special kluge cases.  We want to be
		 *  able to open floppy devices that weren't the boot device.
		 */
		} else if (strcmp(path, FLOPPY0_NAME) == 0) {
			OpenError = 0;
			dev_type = DEVT_DSK;
			dev_addr = 0;
			dev_part = 0;
			break;
		} else if (strcmp(path, FLOPPY1_NAME) == 0) {
			OpenError = 0;
			dev_type = DEVT_DSK;
			dev_addr = 1;
			dev_part = 0;
			break;
		}

		cp = strrchr(path, '/'); /* Back up to next path component & */
		if (xp) *xp = '/';	 /* .. replace the slash we removed, */
	}

	if (OpenError) {
		/*
		 *  If this is the very first open (or we're explictly opening
		 *  the boot device), we can use the default device type and
		 *  device addr (as provided by the boot loader) if we can't
		 *  find a boot-interface property for the indicated device.
		 *  If this is NOT the first call to prom_open, however, we
		 *  fail if we can't find a boot-interface property.
		 */

erx:		printf("%s: can't open - invalid boot interface\n", path);
		return (0);
	}

	for (j = 1; j < MAXDEVOPENS; j++) {
		/*
		 *  Search the list of open devices for an unused entry to
		 *  associate with the open device.  Note that we skip the
		 *  first entry in the "open_devices" table (which is reserved
		 *  for stdin/stdout).
		 */
		if ((ihp = open_devices[j]) == 0) {
			/*
			 *  If this ihandle pointer is unused, we can use it
			 *  for a new ihandle struct (if we're forced to
			 *  allocate one).
			 */
			k = j;

		} else if (((strcmp(path, ihp->pathnm) == 0) &&
		    (!new_root_type ||
		    (strcmp(new_root_type, ihp->fstype) == 0))) ||
		    (new_root_type && (dev_type == ihp->type) &&
		    (dev_addr == ihp->unit) &&
		    (strcmp(new_root_type, ihp->fstype) == 0))) {
			/*
			 *  We already have an ihandle open on the requested
			 *  device.  All we have to do is increment the use
			 *  count and return its table index.
			 */
			if (!new_root_type) new_root_type = ihp->fstype;
			ihp->usecnt += 1;
			return (j);
		}
	}

	if (k && (ihp = (struct ihandle *)bkmem_alloc(sizeof (*ihp)))) {
		/*
		 *  We found an unused ihandle entry.  Allocate an "ihandle"
		 *  struct and use the device-dependent "open" routines to
		 *  initialize its contents.  Start by allocating a buffer
		 *  to hold the path name.
		 */

		if (ihp->pathnm = bkmem_alloc(strlen(path)+1)) {
			/*
			 *  We successfully allocated a pathname buffer, copy
			 *  the path name into it and set up the rest of the
			 *  ihandle struct.
			 */
			ihp->usecnt = 1;
			ihp->type = (unsigned char)dev_type;
			ihp->unit = (unsigned char)dev_addr;
			(void) strcpy(ihp->pathnm, path);

			switch (dev_type) {

			case DEVT_DSK:
				ihp->dev.disk.num = dev_part;
				if (open_disk(ihp)) k = 0;
				break;

			case DEVT_NET:
				if (open_net(ihp)) k = 0;
				break;

			default:
				k = 0;
				break;
			}

			if (k > 0) {
				/*
				 *  Device-specific initialization is
				 *  complete.  Plant a ptr to the new ihandle
				 *  structure and return its index.
				 */
				if ((strcmp(path, BOOT_DEV_NAME) != 0) &&
				    (strcmp(path, FLOPPY0_NAME) != 0))
					OpenCount++;
				(void) strcpy(ihp->fstype, new_root_type);
				open_devices[k] = ihp;
				return (k);
			}

			bkmem_free(ihp->pathnm, strlen(path)+1);
		}

		bkmem_free((char *)ihp, sizeof (struct ihandle));
	}

	printf("%s: can't open - %s\n", path,
	    k ? "no memory" : "too many open devices");
	return (0);
}

int
prom_close(int fd)
{
	/*
	 *  Close a device file:
	 *
	 *  Returns 0 if it works, -1 otherwise (e.g. if the indicated device
	 *  wasn't open).
	 */

	struct ihandle *ihp = devp(fd);

	if ((fd >= 0) && (fd < MAXDEVOPENS) && (ihp != 0)) {
		/*
		 *  If file descriptor is valid, perform close processing
		 *  according to device type.  If the device isn't open,
		 *  device type code in the corresponding ihandle struct
		 *  will be null!
		 */
		if (!fd || --(ihp->usecnt)) {
			/*
			 *  If device is still in use (or if device is
			 *  stdin/stdout), simply decrement the use count
			 *  and return.
			 */
			return (0);
		}

		switch (ihp->type) {
			/*
			 *  Perform device-dependent close processing (if any)
			 *  before freeing up the ihandle struct.
			 */
			case DEVT_DSK:	close_disk(ihp); break;
			case DEVT_NET:	close_net(ihp);  break;
		}

		bkmem_free(ihp->pathnm, strlen(ihp->pathnm)+1);
		bkmem_free((char *)ihp, sizeof (*ihp));
		open_devices[fd] = 0;
		return (0);
	}

	return (-1);	/* Bogus file descriptor!	*/
}

/*ARGSUSED*/
int
prom_seek(int fd, int high, int low)
{
	/*
	 *  Seek to given offset:
	 *
	 *  We don't need seek as a separate operation; Read/write calls are
	 *  always relative to the "startblk" argument.  All we do here is val-
	 *  idate the file descriptor and make sure the "high" order 32 bits of
	 *  the seek offset are null.
	 */

	return (-((fd < 0) || (fd >= MAXDEVOPENS) || !devp(fd) || high));
}

int
prom_devreset(int fd)
{
	/*
	 *  Reset device:
	 *
	 *  Currently only implemented for disk devices.
	 *
	 */
	struct ihandle *ihp = devp(fd);

	if (ihp->type == DEVT_DSK)
		return (reset_disk(ihp));
	else
		return (0);
}

/*ARGSUSED*/
int
prom_read(int fd, caddr_t buf, u_int len, u_int startblk, char devtype)
{
	/*
	 *  Read from a device:
	 *
	 *  The real work is done in one of the device-specific "read" modules,
	 *  all we have to do is figure out which one to call!
	 */
	struct ihandle *ihp = devp(fd);

	if ((fd >= 0) && (fd < MAXDEVOPENS) && (ihp != 0)) {
		/*
		 *  File descriptor is legit, use the "type" field of the
		 *  corresponding ihandle struct to select a read method.
		 *
		 *  NOTE: Stdin/stdout (fd zero) is the only "serial" device
		 *  that's currently supported.
		 */
		switch (ihp->type) {
		case DEVT_DSK:
			return (read_disk(ihp, buf, len, startblk));
		case DEVT_NET:
			return (read_net(ihp, buf, len, startblk));
		case DEVT_SER:
			if (!fd)
				return (read_stdin(ihp, buf, len));
			break;
		default:
			return (-1);
		}
	}

	return (-1);	/* Bogus file descriptor	*/
}

/*ARGSUSED*/
int
prom_write(int fd, caddr_t buf, u_int len, u_int startblk, char devtype)
{
	/*
	 *  Write to a device:
	 *
	 *  As with read, all we do here is call a device-dependent routine
	 *  based on the "type" field of the ihandle struct bound to the
	 *  caller's file descriptor.
	 */

	struct ihandle *ihp = devp(fd);

	if ((fd >= 0) && (fd < MAXDEVOPENS) && (ihp != 0)) {
		/*
		 *  File descriptor is legit, use the "type" field of the
		 *  corresponding ihandle struct to select a write method.
		 *
		 *  NOTE: Stdin/stdout (fd zero) is the only "serial" device
		 *  that's currently supported.
		 */
		switch (ihp->type) {

		case DEVT_DSK:
			return (write_disk(ihp, buf, len, startblk));
		case DEVT_NET:
			return (write_net(ihp, buf, len, startblk));
		case DEVT_SER:
			if (!fd)
				return (write_stdout(ihp, buf, len));
		default:
			return (-1);
		}
	}

	return (-1);	/* Bogus file descriptor	*/
}

unsigned
prom_gettime(void)
{
	/*
	 *  Read system timer:
	 *
	 *  Return milliseconds since last time counter was reset.
	 *  The timer ticks 18.2 times per second or approximately
	 *  55 milliseconds per tick.
	 *
	 *  The counter will be reset to zero by the bios after 24 hours
	 *  or 1,573,040 ticks. The first read after a counter
	 *  reset will flag this condition in the %al register.
	 *  Unfortunately, it is hard to take advantage of this
	 *  fact because some broken bioses will return bogus
	 *  counter values if the counter is in the process of
	 *  updating. We protect against this race by reading the
	 *  counter until we get consecutive identical readings.
	 *  By doing so, we lose the counter reset bit. To make this
	 *  highly unlikely, we reset the counter to zero on the
	 *  first call and assume 24 hours is enough time to get this
	 *  machine booted.
	 *
	 *  An attempt is made to provide a unique number on each
	 *  call by adding 1 millisecond if the 55 millisecond counter
	 *  hasn't changed. If this happens more than 54 times, we
	 *  return the same value until the next real tick.
	 *
	 */
	static unsigned lasttime = 0;
	static short fudge = 0;
	unsigned ticks, mills, first, tries;

	if (lasttime == 0) {
		/*
		 * initialize counter to zero so we don't have to
		 * worry about 24 hour wrap.
		 */
		(void) memset(&ic, 0, sizeof (ic));
		ic.ax = 0x0100;
		ic.intval = 0x1A;
		(void) doint();
	}
	tries = 0;
	do {
		/*
		 * Loop until we trust the counter value.
		 */
		(void) memset(&ic, 0, sizeof (ic));
		ic.intval = 0x1A;
		(void) doint();
		first = (ic.cx << 16) + (ic.dx & 0xFFFF);
		(void) memset(&ic, 0, sizeof (ic));
		ic.intval = 0x1A;
		(void) doint();
		ticks = (ic.cx << 16) + (ic.dx & 0xFFFF);
	} while (first != ticks && ++tries < 10);
	if (tries == 10)
		printf("prom_gettime: BAD BIOS TIMER\n");

	mills = ticks*55;
	if (mills > lasttime) {
		fudge = 0;
	} else {
		fudge += (fudge < 54) ? 1 : 0;
	}
	mills += fudge;
	lasttime = mills;
	return (mills);
}

int
prom_getmacaddr(int fd, unsigned char *eap)
{
	/*
	 *  Obtain machine's ethernet address:
	 *
	 *  This operation is only legal for network devices!
	 */
	struct ihandle *ihp = devp(fd);

	if ((fd > 0) && (fd < MAXDEVOPENS) && ihp && (ihp->type == DEVT_NET)) {
		/*
		 *  File descriptor is legit, and this is a real network device.
		 *  Call macaddr_net() to do the real work!
		 */
		return (macaddr_net(ihp, eap));
	}

	return (-1);
}

caddr_t
prom_alloc(caddr_t virthint, u_int size, int align)
{	/* Use virtual memory mapper to allocate memory */

	return ((caddr_t)map_mem((u_int)virthint, size, align));
}

void
prom_panic(char *str)
{	/* Print panic string, then blow up! */

	printf("prom_panic: %s\n", str);
	bootabort();
}

#ifdef XXX
void
prom_enter_mon()
{	/* There is no monitor; Wait for keystroke instead */

	(void) goany();
}
#endif

char *
prom_bootargs()
{	/* For now, we're just returning a constant */

	return ("kernel/unix");
}

/*
 * Defines for accessing BIOS real time clock.
 */
#define	BIOS_REALTIME_CLK_INT		0x1A
#define	BIOS_REALTIME_GETTIME_FN	0x2
#define	BIOS_REALTIME_GETDATE_FN	0x4

void
prom_rtc_time(ushort *hours, ushort *mins, ushort *secs)
{
	/*
	 * Read time from the Realtime clock.
	 */
	struct real_regs *rr;

	if (!(rr = alloc_regs()))
		prom_panic("No low memory for RTC timestamp");

	AH(rr) = BIOS_REALTIME_GETTIME_FN;
	(void) doint_asm(BIOS_REALTIME_CLK_INT, rr);

	if (rr->eflags & CARRY_FLAG) {
		printf("No RTC timer!?");
		*hours = *mins = *secs = 0;
	} else {
		*hours = bcd_to_bin((ushort)(CH(rr)));
		*mins  = bcd_to_bin((ushort)(CL(rr)));
		*secs  = bcd_to_bin((ushort)(DH(rr)));
	}

	free_regs(rr);
}

void
prom_rtc_date(ushort *year, ushort *month, ushort *day)
{
	/*
	 * Read date from the Realtime clock.
	 */
	struct real_regs *rr;

	if (!(rr = alloc_regs()))
		prom_panic("No low memory for RTC datestamp");

	AH(rr) = BIOS_REALTIME_GETDATE_FN;
	(void) doint_asm(BIOS_REALTIME_CLK_INT, rr);

	if (rr->eflags & CARRY_FLAG) {
		printf("No RTC date!?");
		*year = *month = *day = 0;
	} else {
		*year = bcd_to_bin(CX(rr));
		*month = bcd_to_bin((ushort)(DH(rr)));
		*day = bcd_to_bin((ushort)(DL(rr)));
	}

	free_regs(rr);
}

#ifdef	HAS_CACHEFS
int
prom_devicetype(int fd, char *devtype)
{
	/*
	 *  Determine if device is "network" or "block"
	 *
	 */
	struct ihandle *ihp = devp(fd);

	if ((fd >= 0) && (fd < MAXDEVOPENS) && (ihp != 0)) {
		/*
		 *  File descriptor is legit, use the "type" field of the
		 *  corresponding ihandle struct to check device type
		 *
		 */
		if ((ihp->type == DEVT_DSK) && (strcmp(devtype, "block") == 0))
			return (1);

		if ((ihp->type == DEVT_NET) &&
		    (strcmp(devtype, "network") == 0))
			return (1);

		return (0);	/* device type is not "devtype" */
	}

	return (0);		/* bad file descriptor	*/
}
#endif	/* HAS_CACHEFS */
