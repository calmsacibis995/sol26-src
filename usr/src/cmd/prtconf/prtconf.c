/*
 * Copyright (c) 1992 Sun Microsystems, Inc.  All Rights Reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident "@(#)prtconf.c	1.16	96/05/09 SMI"

#include	<stdio.h>
#include	<ctype.h>
#include	<unistd.h>
#include	<sys/param.h>
#include	<sys/types.h>
#include	<sys/utsname.h>


extern void prtconf_devinfo();
struct utsname uts_buf;
long sysmem();

char	*progname;
char	*promdev = "/dev/openprom";
static int	prominfo;
static int	noheader;
static int	fbname;
static int	promversion;
int	pseudodevs;
int	verbose;
int	drv_name;

static char *usage = "%s [ -v ] [ -p ] [ -F ] [ -P ] [ -V ] [ -D ]\n";


static void
setprogname(name)
char *name;
{
	register char *p;
	extern char *strrchr();

	if (p = strrchr(name, '/'))
		progname = p + 1;
	else
		progname = name;
}

void
main(int argc, char *argv[])
{
	longlong_t	ii;
	long pagesize, npages;
	int	c;
	extern char *optarg;
	extern void exit();
	extern int do_fbname();
	extern int do_promversion();
	extern int do_prominfo();

	setprogname(argv[0]);
	while ((c = getopt(argc, argv, "DvVpPFf:")) != -1)  {
		switch (c)  {
		case 'D':
			++drv_name;
			break;
		case 'v':
			++verbose;
			break;
		case 'p':
			++prominfo;
			break;
		case 'f':
			promdev = optarg;
			break;
		case 'V':
			++promversion;
			break;
		case  'F':
			++fbname;
			++noheader;
			break;
		case 'P':
			++pseudodevs;
			break;
		default:
			(void) fprintf(stderr, usage, progname);
			exit(1);
			/*NOTREACHED*/
		}
	}


	(void) uname(&uts_buf);

	if (fbname)
		exit(do_fbname());

	if (promversion)
		exit(do_promversion());

	(void) printf("System Configuration:  Sun Microsystems  %s\n",
	    uts_buf.machine);

	pagesize = sysconf(_SC_PAGESIZE);
	npages = sysconf(_SC_PHYS_PAGES);
	(void) printf("Memory size: ");
	if (pagesize == -1 || npages == -1)
		(void) printf("unable to determine\n");
	else {
		int kbyte = 1024;
		int mbyte = 1024 * 1024;

		ii = (longlong_t) pagesize * npages;
		if (ii >= mbyte)
			(void) printf("%d Megabytes\n",
				(int) ((ii+mbyte-1) / mbyte));
		else
			(void) printf("%d Kilobytes\n",
				(int) ((ii+kbyte-1) / kbyte));
	}

	if (prominfo)  {
		(void) printf("System Peripherals (PROM Nodes):\n\n");
		if (do_prominfo() == 0)
			exit(0);
		(void) fprintf(stderr, "%s: Defaulting to non-PROM mode...\n",
		    progname);
	}

	(void) printf("System Peripherals (Software Nodes):\n\n");

	(void) prtconf_devinfo();

	exit(0);
} /* main */
