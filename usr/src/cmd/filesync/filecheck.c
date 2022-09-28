/*
 * Copyright (c) 1995 Sun Microsystems, Inc.  All Rights Reserved
 *
 * module:
 *	filecheck.c
 *
 * purpose:
 *	This program is used by the functionality/regression suite.
 *	It displays information about files that is awkward to
 *	get from standard UNIX commands.
 *
 * synnopsis:
 *	filecheck filename ...
 *	filecheck -m filename1 filename2 ...
 *	filecheck -M filename1 filename2 ...
 *	filecheck -f filename
 *	filecheck -l filename
 *
 * notes:
 *	there is no pretense of generality here.  This is a special
 *	purpose program that does some things that are needed by the
 * 	filesync test suite, and are difficult to do with any
 *	of the standard file system commands.
 */
#ident	"@(#)filecheck.c	1.5	96/03/22 SMI"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>


/*
 * routine:
 *	show_info
 *
 * purpose:
 *	print out the modes, uid, gid and high res modtime for a file
 *
 * parms:
 *	file name
 *
 * note:
 *	technically, everything but the high res modtime can be
 *	obtained from an "ls -l", but this is so much easier and
 *	faster, and since I needed the mod time, I figured why not
 *	just get it all while I was at it.
 */
void
show_info(char *name)
{
	struct stat statb;

	if (lstat(name, &statb) < 0) {
		fprintf(stderr, "Unable to lstat %s\n", name);
		exit(1);
	}

	printf("MODE:%4lo  USER:%5ld  GROUP:%5ld  MOD: 0x%08lx.%08lx  %s\n",
		statb.st_mode&07777, statb.st_uid, statb.st_gid,
		statb.st_mtim.tv_sec, statb.st_mtim.tv_nsec, name);
}

/*
 * routine:
 *	print the relationship between the mod times of two files
 *
 * parms:
 *	names of the two files
 *	whether comparison should be hi-res or not
 *
 * notes:
 *	this should be an option to test(1)
 */
void
modtime_compare(char *name1, char *name2, int nano)
{
	struct stat statb1, statb2;
	char *sense;

	if (lstat(name1, &statb1) < 0) {
		fprintf(stderr, "Unable to lstat %s\n", name1);
		exit(1);
	}

	if (lstat(name2, &statb2) < 0) {
		fprintf(stderr, "Unable to lstat %s\n", name2);
		exit(1);
	}

	if (statb1.st_mtim.tv_sec > statb2.st_mtim.tv_sec)
		sense = ">>";
	else if (statb1.st_mtim.tv_sec < statb2.st_mtim.tv_sec)
		sense = "<<";
	else if (!nano)
		sense = "==";
	else if (statb1.st_mtim.tv_nsec > statb2.st_mtim.tv_nsec)
		sense = "> ";
	else if (statb1.st_mtim.tv_nsec < statb2.st_mtim.tv_nsec)
		sense = "< ";
	else
		sense = "= ";

	printf("MODTIME %s %s %s\n", name1, sense, name2);
}

/*
 * routine:
 *	show_type
 *
 * purpose:
 *	to print out the type of a file system
 *
 * parms:
 *	name of a file on that file system
 */
void
show_type(char *name)
{
	struct stat statb;

	if (lstat(name, &statb) < 0) {
		fprintf(stderr, "Unable to lstat %s\n", name);
		exit(1);
	}

	printf("FSTYPE %s %s\n", name, statb.st_fstype);
}

/*
 * routine:
 *	do_lock
 *
 * purpose:
 *	to set an advisory lock on a specified file for 30 seconds
 *
 * parms:
 *	name of file to be locked
 */
void
do_lock(char *name)
{	int fd;
	int ret;

	/* open the file read/write	*/
	fd = open(name, 2);

	/* lock the file */
	ret = lockf(fd, F_TLOCK, 0L);
	printf("locking file %s -> %d\n", name, ret);

	/* sleep for 30 seconds */
	sleep(30);

	/* unlock the file */
	ret = lockf(fd, F_ULOCK, 0L);
}

/*
 * routine:
 *	main
 *
 * purpose:
 *	to figure out what the user wants and do it
 */
void
main(int argc, char **argv)
{	int modcmp = 0;
	int fstype = 0;
	int lock = 0;
	int i;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			if (argv[i][1] == 'm')
				modcmp = 'm';
			else if (argv[i][1] == 'M')
				modcmp = 'M';
			else if (argv[i][1] == 'f')
				fstype = 1;
			else if (argv[i][1] == 'l')
				lock = 1;
			continue;
		}

		if (lock) {
			do_lock(argv[i]);
			continue;
		}

		if (modcmp) {
			modtime_compare(argv[i], argv[i+1], modcmp == 'M');
			i++;
		} else if (fstype)
			show_type(argv[i]);
		else
			show_info(argv[i]);
	}

	exit(0);
}
