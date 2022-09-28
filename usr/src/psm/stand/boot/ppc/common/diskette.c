/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)diskette.c	1.8	96/10/15 SMI"

/*
 * Low level floppy I/O support (and other miscellany) for boot.
 */

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/promif.h>
#include <sys/salib.h>

extern caddr_t kmem_alloc(u_int);

static int fdblock;	/* first block in track buffer */

/* exported to the pcfs code, which seems questionable */
short		flop_bps;		/* bytes per sector */
short		flop_spt;		/* disk sectors per track */
short		flop_spc;		/* disk sectors per cylinder */
unsigned int    flop_endblk;

/* default to 2.88Mb diskette */
#define	BPS 512
#define	SPT 36
#define	CYLS 80

/*
 * invalidate diskette sectors in fdcache
 */
void
diskette_ivc(void)
{
	fdblock = -1;
}

int
goany(void)
{
	printf("Press any key to continue  ");
	(void) prom_getchar();
	printf("\n");
	return (0);
}

#ifdef FLOP_DEBUG
void
dump(caddr_t start, int count, char *hdr)
{
	int i = 512;
	int *ip = (int *)start;

	if (count < i)
		i = count;
	printf("buffer dump: %s (address=%x)\n", hdr, start);
	while (--i >= 0) {
		printf("%x %x %x %x %x %x %x %x\n",
		    ip[0], ip[1], ip[2], ip[3], ip[4], ip[5], ip[6], ip[7]);
		ip += 8;
		i -= 32;
	}
}
#endif

/*
 * This must be called before any I/O to diskette drive is attempted.
 */
char *
init_disketteio(void)
{
	flop_bps = BPS;
	flop_spt = SPT;
	flop_spc = 2 * flop_spt;
	flop_endblk = CYLS * flop_spc;

	diskette_ivc();

#ifdef TESTING_HD
	return ("/pci/pci1000,1@1/disk@6,0:2");
#else
	return ("floppy");
#endif
}

/*
 * rdiskette reads "len" bytes from sector "startblk" into "buf".
 * calls diskette to do actual absolute sector reads.
 * It handles translation from bytes to sectors
 */
int
rdiskette(int fd, caddr_t buf, u_int len, u_int startblk, char devtype)
{
	static char *fdcache;	/* track buffer */
	unsigned int bytes, count;
	int sectcnt;

#ifdef FLOP_DEBUG
	printf("rdiskette: fd 0x%x buf 0x%x len 0x%x startblk 0x%x type %d\n",
	    fd, buf, len, startblk, devtype);
#endif

	if (fdcache == (char *)0) {
		/*
		 * Set up track buffer for more efficient floppy I/O.
		 */
		fdcache = (char *)kmem_alloc(SPT * BPS);
#ifdef FLOP_DEBUG
		printf("Floppy cache at 0x%x, size 0x%x\n", fdcache, SPT * BPS);
#endif
	}

	/* make sure read does not extend beyond diskette */
	if ((startblk + (len / flop_bps)) >= flop_endblk) {
		printf("boot: WARNING: read beyond end of diskette\n");
#ifdef FLOP_DEBUG
		printf("rdiskette: startblk = 0x%x, numblks = 0x%x\n",
		    startblk, len / flop_bps);
		(void) goany();
#endif
		return (0);
	}

	bytes = len;

	while (bytes) {
		if (fdblock == -1 || startblk < fdblock ||
		    startblk >= (fdblock + flop_spt)) {
			/*
			 * freshen up cache by reading a track
			 */
			fdblock = (startblk / flop_spt) * flop_spt;
#ifdef FLOP_DEBUG
			printf("rdiskette: track cache refilled: fdblock=%d\n",
			    fdblock);
#endif
			if (prom_seek(fd, (unsigned long long)fdblock *
			    (unsigned long long)flop_bps) != 0) {
				printf("rdiskette: prom_seek error\n");
				(void) goany();
				return (len - bytes);
			}

			if (prom_read(fd, fdcache, flop_spt * flop_bps, fdblock,
			    devtype) != (flop_spt * flop_bps)) {
				printf("rdiskette: prom_read error\n");
				(void) goany();
				diskette_ivc();
				return (len - bytes); /* return bad value */
			}
		}
		sectcnt = flop_spt - (startblk - fdblock);
		count = sectcnt * flop_bps;
		if (bytes < count) {
			count = bytes;
			sectcnt = count / flop_bps;
		}
		bcopy(((startblk - fdblock) * flop_bps) + fdcache, buf, count);
#ifdef FLOP_DEBUG
		dump(buf, count, "dst");
		(void) goany();
#endif
		startblk += sectcnt;
		buf += count;
		bytes -= count;
	}

	/* return good value */
#ifdef FLOP_DEBUG
	printf("rdiskette end: startblk 0x%x buf 0x%x len 0x%x bytes 0x%x\n",
	    startblk, buf, len, bytes);
#endif
	return (len - bytes);
}

/*
 * check for diskette change
 */
int
chk_diskette(void)
{
	int	status = 0;

#ifdef FLOP_DEBUG
	printf("chk_diskette:  \n");
#endif
	/* XXXPPC - detect diskette change, and retry if error */

	return (status);
}

void
wait100ms(void)
{
	int forever = 10000000;		/* do not loop forever */
	int start = prom_gettime();	/* millisecond timer */

	while (prom_gettime() < start + 100)
		if (--forever < 0)
			break;
}
