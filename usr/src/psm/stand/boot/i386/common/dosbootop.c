/*
 * Copyright (c) 1995-1996, Sun Microsystems, Inc.  All Rights Reserved.
 */

#ident "@(#)dosbootop.c	1.6	96/05/03 SMI"

#include <sys/types.h>
#include <sys/bsh.h>
#include <sys/bootconf.h>
#include <sys/ramfile.h>
#include <sys/dosemul.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/bootvfs.h>
#include <sys/salib.h>
#include <sys/promif.h>

extern	struct bootops *bop;
extern	void cleardebugs(), restoredebugs(), savedebugs();
extern  int int21debug;
extern void init_arg(struct arg *argp);
extern void cleanup_arg(struct arg *argp);
extern void get_command(struct arg *argp);
extern void run_command(struct arg *argp);
extern void putchar();
extern int bgetproplen(struct bootops *, char *, phandle_t);

/*
 * Globals for handling output capture from boot shell command execution.
 */
short	DOSsnarf_flag = 0;
short	DOSsnarf_silent = 0;
rffd_t	*DOSsnarf_fp;

/*
 * Snarfing is how real mode modules can receive results from boot
 * shell operations.  Basically, a real mode module writes the input
 * it wants to be interpreted by the boot shell into the \bootops file.
 * Output from this is collected in the bootops.res file for the module
 * to read at its discretion.
 */

void
interpline(char *line, int len)
{
	extern	struct src src[];
	extern	int srcx;
	extern	int verbose_flag;
	struct src *srcp;
	struct arg *args;

	if (!line)
		return;
	else if ((args = (struct arg *)bkmem_alloc(sizeof (struct arg))) ==
	    (struct arg *)NULL) {
		printf("Allocation of bootops args failed!\n");
		return;
	}

	/* initialize source struct */
	srcp = &src[++srcx];
	srcp->type = SRC_FILE;
	srcp->bufsiz = len;
	srcp->buf = srcp->nextchar = (unsigned char *) line;
	srcp->pushedchars = 0;

	/* interpret line */
	if (int21debug)
		verbose_flag++;
	init_arg(args);
	get_command(args);
	run_command(args);
	cleanup_arg(args);
	if (int21debug)
		verbose_flag--;

	/* return to previous source and free args */
	--srcx;
	bkmem_free((caddr_t)args, sizeof (struct arg));
}	

static int
isblankORcomment(char *buf, int len)
{
	int rv = 1;   /* Assume line is blank line or a comment */
	int bc;

	if (buf && buf[0] != '#') {
		for (bc = 0; bc < len; bc++)
			if (!iswhitespace(buf[bc])) {
				rv = 0;
				break;
			}
	}
	return (rv);
}

/*
 * Snarf_succeed and snarf_fail are intended for the use of bootops
 * that want to indicate success or failure by a blank or non-blank
 * character as the first character in the bootops.res output.
 */
void
snarf_succeed(void)
{
	if (DOSsnarf_fp) {
		RAMrewind(DOSsnarf_fp);
		(void) RAMfile_write(DOSsnarf_fp, " ", 1);
	}
}

void
snarf_fail(void)
{
	if (DOSsnarf_fp) {
		RAMrewind(DOSsnarf_fp);
		(void) RAMfile_write(DOSsnarf_fp, "X", 1);
	}
}

void
snarf_on(void)
{
	/* If we aren't already snarfing... */
	if (!DOSsnarf_flag) {
		/* and we have a place to put it, snarf the output */
		if (DOSsnarf_fp) {
			/*
			 *  Save all debugging state and then turn all
			 *  debugging output off.
			 */
			savedebugs();
			cleardebugs();
			RAMtrunc(DOSsnarf_fp);
			RAMrewind(DOSsnarf_fp);
			DOSsnarf_flag = 1;
			/*
			 * Unless told otherwise, silence output
			 * for snarf duration.
			 */
			if (bgetproplen(bop, "dosloudsnarf", 0) < 0)
				DOSsnarf_silent = 1;
			else
				DOSsnarf_silent = 0;
		}
	}
}

void
snarf_off(void)
{
	/* If we are currently snarfing... */
	if (DOSsnarf_flag) {
		if (DOSsnarf_fp) {
			restoredebugs();
			/* Rewind so DOS module can read results */
			RAMrewind(DOSsnarf_fp);
			DOSsnarf_flag = 0;
		}
	}
}

void
dosbootop(dffd_t *fd, char *srcbuf, int nbytes)
{
	char *ibuf;
	int haseol = 0;
	int ilen;
	int bc;

	if (int21debug)
		printf("bootop:");

	/*
	 * If we don't have a new line in the input, the input isn't
	 * ready yet to interpret.
	 */
	for (bc = 0; bc < nbytes; bc++) {
		if (srcbuf[bc] == '\n') {
			haseol = 1;
			break;
		}
	}

	if (haseol) {
		/*
		 * The line is now stored in the RAMfile buffer space
		 * assigned when the real mode module creat()'d the
		 * bootops file.  It needs to be in a contiguous buffer
		 * for interpretation, which isn't guaranteed if the
		 * command was long.  So, we make sure it is now.
		 */

		/*
		 * We break the rules a bit and use our knowledge of
		 * the structure that defines a RAMfile.
		 */
		ilen = fd->rfp->file->size;

		if ((ibuf = (char *)bkmem_alloc(ilen)) == (char *)NULL) {
			prom_panic("No memory for property storage");
		} else {
			RAMrewind(fd->rfp);
			if (RAMfile_read(fd->rfp, ibuf, ilen) != ilen) {
				if (int21debug)
					printf("Contig buf creat error.");
				goto rewind;
			}
		}

		/*
		 * Debug splat of interpret line
		 */
		if (int21debug) {
			int c;
			printf("Interpret this (%d):", ilen);
			for (c = 0; c < ilen; c++)
				putchar(ibuf[c]);
		}

		/*
		 * Ignore blank & comment lines, they can potentially
		 * return us to the previous boot interpretation source,
		 * and cause us to free buffers we shouldn't!
		 */
		if (!isblankORcomment(ibuf, ilen)) {
			snarf_on();
			interpline(ibuf, ilen);
			snarf_off();
			if (int21debug) {
				static int sn = 0;
				char *p;
				int l, c;

				printf("snarfed[(%d)", sn);
				l = DOSsnarf_fp->file->size;
				p = DOSsnarf_fp->file->contents->datap;

				for (c = 0;
				    c < l && c <= RAMfile_BLKSIZE;
				    c++) {
					putchar(*p++);
				}
				if (l > RAMfile_BLKSIZE)
					printf("...");
				printf("]\n"); sn++;
			}
		}

rewind:
		/*
		 * Free the contiguous buffer
		 */
		bkmem_free(ibuf, ilen);

		/*
		 * Rewind RAM file for next bootop call.
		 */
		RAMtrunc(fd->rfp);
		RAMrewind(fd->rfp);
	}
}
