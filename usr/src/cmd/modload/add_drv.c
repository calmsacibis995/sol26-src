#pragma ident	"@(#)add_drv.c	1.23	96/09/26 SMI"


/*
 * Copyright (c) 1993 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <wait.h>
#include <unistd.h>
#include <libintl.h>
#include <sys/modctl.h>
#include <string.h>
#include <limits.h>
#include <signal.h>
#include <locale.h>
#include <device_info.h>
#include <ftw.h>
#include <sys/sunddi.h>
#include "addrem.h"
#include "errmsg.h"

/*
 * globals needed for libdevinfo - there is no way to pass
 * private data to the find routine.
 */
struct dev_list {
	int clone;
	char *dev_name;
	char *driver_name;
	struct dev_list *next;
};

static char *new_drv;
static struct dev_list *conflict_lst = NULL;

static int module_not_found(char *, char *);
static int unique_driver_name(char *, char *, int *);
static int check_perm_opts(char *);
static int update_name_to_major(char *, major_t *, int);
static int aliases_unique(char *);
static int unique_drv_alias(char *);
static int config_driver(char *, major_t, char *, char *, int, int);
static void usage();
static int update_minor_perm(char *, char *);
static int update_driver_classes(char *, char *);
static int update_driver_aliases(char *, char *);
static int do_the_update(char *, char *);
static void signal_rtn();
static int exec_command(char *, char **);

static int drv_name_conflict(void);
static void devfs_node(const char *devfsnm, const dev_info_t *dip);
static int drv_name_match(const dev_info_t *, char *, char *);
static void print_drv_conflict_info(int);
static void check_dev_dir(int);
static char *decode_composite_string(char *, size_t, char *);
static int dev_node(const char *, const struct stat *,
    int, struct FTW *);
static void free_conflict_list(struct dev_list *);
static int clone(const dev_info_t *dip);
static int check_space_within_quote(char *);

int
main(int argc, char *argv[])
{
	int opt;
	struct stat buf;
	FILE *fp;
	major_t major_num;
	char driver_name[FILENAME_MAX + 1];
	char *path_driver_name;
	char *perms = NULL;
	char *aliases = NULL;
	char *classes = NULL;
	int noload_flag = 0;
	int verbose_flag = 0;
	int force_flag = 0;
	int i_flag = 0;
	int c_flag = 0;
	int m_flag = 0;
	int cleanup_flag = 0;
	int server = 0;
	char *basedir = NULL;
	char *cmdline[MAX_CMD_LINE];
	int n;
	int is_unique;
	FILE *reconfig_fp;
	char basedir_rec[PATH_MAX + FILENAME_MAX + 1];
	char *slash;
	int pathlen;
	int x;

	(void) setlocale(LC_ALL, "");
#if	!defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"	/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	/*  must be run by root */

	if (getuid() != 0) {
		(void) fprintf(stderr, gettext(ERR_NOT_ROOT));
		exit(1);
	}

	while ((opt = getopt(argc, argv, "vfm:ni:b:c:")) != EOF) {
		switch (opt) {
		case 'm' :
			m_flag = 1;
			perms = calloc(strlen(optarg) + 1, 1);
			if (perms == NULL) {
				(void) fprintf(stderr, gettext(ERR_NO_MEM));
				exit(1);
			}
			(void) strcat(perms, optarg);
			break;
		case 'f':
			force_flag++;
			break;
		case 'v':
			verbose_flag++;
			break;
		case 'n':
			noload_flag++;
			break;
		case 'i' :
			i_flag = 1;
			aliases = calloc(strlen(optarg) + 1, 1);
			if (aliases == NULL) {
				(void) fprintf(stderr, gettext(ERR_NO_MEM));
				exit(1);
			}
			(void) strcat(aliases, optarg);
			if (check_space_within_quote(aliases) == ERROR) {
				(void) fprintf(stderr, gettext(ERR_NO_SPACE),
					aliases);
				exit(1);
			}
			break;
		case 'b' :
			server = 1;
			basedir = calloc(strlen(optarg) + 1, 1);
			if (basedir == NULL) {
				(void) fprintf(stderr, gettext(ERR_NO_MEM));
				exit(1);
			}
			(void) strcat(basedir, optarg);
			break;
		case 'c':
			c_flag = 1;
			classes = strdup(optarg);
			if (classes == NULL) {
				(void) fprintf(stderr, gettext(ERR_NO_MEM));
				exit(1);
			}
			break;
		case '?' :
		default:
			usage();
			exit(1);
		}
	}


	if (argv[optind] != NULL) {
		path_driver_name = calloc(strlen(argv[optind]) + 1, 1);
		if (path_driver_name == NULL) {
			(void) fprintf(stderr, gettext(ERR_NO_MEM));
			exit(1);

		}
		(void) strcat(path_driver_name, argv[optind]);
		/*
		 * check for extra args
		 */
		if ((optind + 1) != argc) {
			usage();
			exit(1);
		}

	} else {
		usage();
		exit(1);
	}

	/* get module name from path */

	/* if <path>/<driver> ends with slash; strip off slash/s */

	pathlen = strlen(path_driver_name);
	for (x = 1; ((path_driver_name[pathlen - x ] == '/') &&
	    (pathlen != 1)); x++) {
		path_driver_name[pathlen - x] = '\0';
	}

	slash = strrchr(path_driver_name, '/');

	if (slash == NULL) {
		(void) strcpy(driver_name, path_driver_name);

	} else {
		(void) strcpy(driver_name, ++slash);
		if (driver_name[0] == '\0') {
			(void) fprintf(stderr, gettext(ERR_NO_DRVNAME),
			    path_driver_name);
			usage();
			exit(1);
		}
	}
	new_drv = driver_name;

	/* set up add_drv filenames */
	if ((build_filenames(basedir)) == ERROR) {
		exit(1);
	}

	(void) sigset(SIGINT, signal_rtn);
	(void) sigset(SIGHUP, signal_rtn);
	(void) sigset(SIGTERM, signal_rtn);

	/* must be only running version of add_drv/rem_drv */

	if ((stat(add_rem_lock, &buf) == -1) && errno == ENOENT) {
		fp = fopen(add_rem_lock, "a");
		(void) fclose(fp);
	} else {
		(void) fprintf(stderr,
		gettext(ERR_PROG_IN_USE));
		exit(1);
	}

	if ((some_checking(m_flag, i_flag)) == ERROR)
		err_exit();

	/*
	 * check validity of options
	 */
	if (m_flag) {
		if ((check_perm_opts(perms)) == ERROR)
			err_exit();
	}

	if (i_flag) {
		if (aliases != NULL)
			if ((aliases_unique(aliases)) == ERROR)
				err_exit();
	}


	if ((unique_driver_name(driver_name, name_to_major,
	    &is_unique)) == ERROR)
		err_exit();

	if (is_unique == NOT_UNIQUE) {
		(void) fprintf(stderr, gettext(ERR_NOT_UNIQUE), driver_name);
		err_exit();
	}

	if (!server) {
		if ((module_not_found(driver_name, path_driver_name))
		    == ERROR) {
			perror(NULL);
			(void) fprintf(stderr, gettext(ERR_NOMOD), driver_name);
			err_exit();
		}
		/*
		 * Check for a more specific driver conflict - see
		 * PSARC/1995/239
		 * Note that drv_name_conflict() can return -1 for error
		 * or 1 for a conflict.  Since the default is to fail unless
		 * the -f flag is specified, we don't bother to differentiate.
		 */
		if (drv_name_conflict()) {
			/*
			 * if the force flag is not set, we fail here
			 */
			if (!force_flag) {
				(void) fprintf(stderr,
				    gettext(ERR_INSTALL_FAIL), driver_name);
				(void) fprintf(stderr, "Device managed by "
				    "another driver.\n");
				if (verbose_flag)
					print_drv_conflict_info(force_flag);
				err_exit();
			}
			/*
			 * The force flag was specified so we print warnings
			 * and install the driver anyways
			 */
			if (verbose_flag)
				print_drv_conflict_info(force_flag);
			free_conflict_list(conflict_lst);
		}
	}

	if ((update_name_to_major(driver_name, &major_num, server)) == ERROR) {
		err_exit();
	}

	cleanup_flag |= CLEAN_NAM_MAJ;


	if (m_flag) {
		if (update_minor_perm(driver_name, perms) == ERROR) {
			cleanup_flag |= CLEAN_MINOR_PERM;
			remove_entry(cleanup_flag, driver_name);
			err_exit();
		}
		cleanup_flag |= CLEAN_MINOR_PERM;
	}

	if (i_flag) {
		if (update_driver_aliases(driver_name, aliases) == ERROR) {
			cleanup_flag |= CLEAN_DRV_ALIAS;
			remove_entry(cleanup_flag, driver_name);
			err_exit();

		}
		cleanup_flag |= CLEAN_DRV_ALIAS;
	}

	if (c_flag) {
		if (update_driver_classes(driver_name, classes) == ERROR) {
			cleanup_flag |= CLEAN_DRV_CLASSES;
			remove_entry(cleanup_flag, driver_name);
			err_exit();

		}
		cleanup_flag |= CLEAN_DRV_CLASSES;
	}

	if (server) {
		(void) fprintf(stderr, gettext(BOOT_CLIENT));

		/*
		 * create /reconfigure file so system reconfigures
		 * on reboot
		 */
		(void) strcpy(basedir_rec, basedir);
		(void) strcat(basedir_rec, RECONFIGURE);
		reconfig_fp = fopen(basedir_rec, "a");
		(void) fclose(reconfig_fp);
	} else {
		/*
		 * paranoia - if we crash whilst configuring the driver
		 * this might avert possible file corruption.
		 */
		sync();

		if (config_driver(driver_name, major_num,
		    aliases, classes, cleanup_flag, noload_flag) == ERROR) {
			err_exit();
		}
	}

	if (!server && !noload_flag) {
		/*
		 * run devlinks -r /
		 * run disks -r /
		 * run ports -r /
		 * run tapes -r /
		 */

		n = 0;
		cmdline[n++] = "devlinks";
		cmdline[n] = (char *)0;

		if (exec_command(DEVLINKS_PATH, cmdline)) {
			(void) fprintf(stderr, gettext(ERR_DEVLINKS),
			    DEVLINKS_PATH);
		}

		cmdline[0] = "disks";
		if (exec_command(DISKS_PATH, cmdline)) {
			(void) fprintf(stderr, gettext(ERR_DISKS), DISKS_PATH);
		}

		cmdline[0] = "ports";
		if (exec_command(PORTS_PATH, cmdline)) {
			(void) fprintf(stderr, gettext(ERR_PORTS), PORTS_PATH);
		}

		cmdline[0] = "tapes";
		if (exec_command(TAPES_PATH, cmdline)) {
			(void) fprintf(stderr, gettext(ERR_TAPES), TAPES_PATH);
		}
	}

	exit_unlock();

	if (verbose_flag) {
		(void) fprintf(stderr, gettext(DRIVER_INSTALLED), driver_name);
	}

	return (NOERR);
}


int
module_not_found(char *module_name, char *path_driver_name)
{
	char path[MAXPATHLEN + FILENAME_MAX + 1];
	struct stat buf;
	struct stat ukdbuf;
	char data [MAXMODPATHS];
	char usr_kernel_drv[FILENAME_MAX + 17];
	char *next = data;

	/*
	 * if path
	 * 	if (path/module doesn't exist AND
	 *	 /usr/kernel/drv/module doesn't exist)
	 *	error msg
	 *	exit add_drv
	 */


	if (strcmp(module_name, path_driver_name)) {
		(void) strcpy(usr_kernel_drv, "/usr/kernel/drv/");
		(void) strcat(usr_kernel_drv, module_name);

		if (((stat(path_driver_name, &buf) == 0) &&
			((buf.st_mode & S_IFMT) == S_IFREG)) ||

		    ((stat(usr_kernel_drv, &ukdbuf) == 0) &&
			((ukdbuf.st_mode & S_IFMT) == S_IFREG))) {

			return (NOERR);
		}
	} else {
		/* no path */
		if (modctl(MODGETPATH, NULL, data) != 0) {
			(void) fprintf(stderr, gettext(ERR_MODPATH));
			return (ERROR);
		}

		next = strtok(data, MOD_SEP);
		while (next != NULL) {
			(void) sprintf(path, "%s/drv/%s", next, module_name);

			if ((stat(path, &buf) == 0) &&
			    ((buf.st_mode & S_IFMT) == S_IFREG)) {
				return (NOERR);
			}
			next = strtok((char *)NULL, MOD_SEP);
		}
	}

	return (ERROR);
}

/*
 * search for driver_name in first field of file file_name
 * searching name_to_major and driver_aliases: name separated from rest of
 * line by blank
 * if there return
 * else return
 */
int
unique_driver_name(char *driver_name, char *file_name,
	int *is_unique)
{
	int ret;

	if ((ret = get_major_no(driver_name, file_name)) == ERROR) {
		(void) fprintf(stderr, gettext(ERR_CANT_ACCESS_FILE),
		    file_name);
	} else {
		/* XXX */
		/* check alias file for name collision */
		if (unique_drv_alias(driver_name) == ERROR) {
			ret = ERROR;
		} else {
			if (ret != UNIQUE)
				*is_unique = NOT_UNIQUE;
			else
				*is_unique = ret;
			ret = NOERR;
		}
	}
	return (ret);
}

/*
 * check each entry in perm_list for:
 *	4 arguments
 *	permission arg is in valid range
 * permlist entries separated by comma
 * return ERROR/NOERR
 */
int
check_perm_opts(char *perm_list)
{
	char *current_head;
	char *previous_head;
	char *one_entry;
	int i, len, scan_stat;
	char minor[FILENAME_MAX + 1];
	char perm[OPT_LEN + 1];
	char own[OPT_LEN + 1];
	char grp[OPT_LEN + 1];
	char dumb[OPT_LEN + 1];
	int status = NOERR;
	int intperm;

	len = strlen(perm_list);

	if (len == 0) {
		usage();
		return (ERROR);
	}

	one_entry = calloc(len + 1, 1);
	if (one_entry == NULL) {
		(void) fprintf(stderr, gettext(ERR_NO_MEM));
		return (ERROR);
	}

	previous_head = perm_list;
	current_head = perm_list;

	while (*current_head != '\0') {

		for (i = 0; i <= len; i++)
			one_entry[i] = 0;

		current_head = get_entry(previous_head, one_entry, ',');

		previous_head = current_head;
		scan_stat = sscanf(one_entry, "%s%s%s%s%s", minor, perm, own,
		    grp, dumb);

		if (scan_stat < 4) {
			(void) fprintf(stderr, gettext(ERR_MIS_TOK),
			    "-m", one_entry);
			status = ERROR;
		}
		if (scan_stat > 4) {
			(void) fprintf(stderr, gettext(ERR_TOO_MANY_ARGS),
			    "-m", one_entry);
			status = ERROR;
		}

		intperm = atoi(perm);
		if (intperm < 0000 || intperm > 4777) {
			(void) fprintf(stderr, gettext(ERR_BAD_MODE), perm);
			status = ERROR;
		}

	}

	free(one_entry);
	return (status);
}


/*
 * get major number
 * write driver_name major_num to name_to_major file
 * major_num returned in major_num
 * return success/failure
 */
int
update_name_to_major(
	char *driver_name,
	major_t *major_num,
	int server)
{
	char drv[FILENAME_MAX + 1];
	char major[MAX_STR_MAJOR + 1];
	struct stat buf;
	char *num_list;
	FILE *fp;
	char drv_majnum[MAX_STR_MAJOR + 1];
	char line[MAX_N2M_ALIAS_LINE + 1];
	int new_maj = -1;
	int i;
	int is_unique;
	int max_dev = 0;
	int tmp = 0;

	/*
	 * if driver_name already in rem_name_to_major
	 * 	delete entry from rem_nam_to_major
	 *	put entry into name_to_major
	 */

	if (stat(rem_name_to_major, &buf) == 0) {

		if ((is_unique = get_major_no(driver_name, rem_name_to_major))
		    == ERROR)
			return (ERROR);

		if (is_unique != UNIQUE) {
			/*
			 * found matching entry in /etc/rem_name_to_major
			 */
			(void) sprintf(major, "%d", is_unique);

			if (append_to_file(driver_name, major,
			    name_to_major, ' ', " ") == ERROR) {
				(void) fprintf(stderr,
				    gettext(ERR_NO_UPDATE),
				    name_to_major);
				return (ERROR);
			} else if (delete_entry(rem_name_to_major,
			    driver_name, " ") == ERROR) {
				(void) fprintf(stderr,
				    gettext(ERR_DEL_ENTRY),
				    driver_name,
				    rem_name_to_major);
				return (ERROR);
			}

			/* found matching entry : no errors */

			*major_num = is_unique;
			return (NOERR);
		}
		/*
		 * no match found in rem_name_to_major
		 */
	}

	/*
	 * Bugid: 1264079
	 * In a server case (with -b option), we can't use modctl() to find
	 *    the maximum major number, we need to dig thru client's
	 *    /etc/name_to_major and /etc/rem_name_to_major for the max_dev.
	 *
	 * if (server)
	 *    get maximum major number thru (rem_)name_to_major file on client
	 * else
	 *    get maximum major number allowable on current system using modctl
	 */
	if (server) {
		max_dev = 0;
		tmp = 0;

		max_dev = get_max_major(name_to_major);

		/* If rem_name_to_major exists, we need to check it too */
		if (stat(rem_name_to_major, &buf) == 0) {
			tmp = get_max_major(rem_name_to_major);

			/*
			 * If name_to_major is missing, we can get max_dev from
			 * /etc/rem_name_to_major.  If both missing, bail out!
			 */
			if ((max_dev == ERROR) && (tmp == ERROR)) {
				(void) fprintf(stderr,
					gettext(ERR_CANT_ACCESS_FILE),
					name_to_major);
				return (ERROR);
			}

			/* guard against bigger maj_num in rem_name_to_major */
			if (tmp > max_dev)
				max_dev = tmp;
		} else {
			/*
			 * If we can't get major from name_to_major file
			 * and there is no /etc/rem_name_to_major file,
			 * then we don't have a max_dev, bail out quick!
			 */
			if (max_dev == ERROR)
				return (ERROR);
		}

	} else {
		if (modctl(MODRESERVED, NULL, &max_dev) < 0) {
			perror(NULL);
			(void) fprintf(stderr, gettext(ERR_MAX_MAJOR));
			return (ERROR);
		}
	}

	num_list = calloc(max_dev, 1);
	if (num_list == NULL) {
		(void) fprintf(stderr, gettext(ERR_NO_MEM));
		return (ERROR);
	}

	/*
	 * read thru name_to_major, marking each major number found
	 * order of name_to_major not relevant
	 */
	if ((fp = fopen(name_to_major, "r")) == NULL) {
		perror(NULL);
		(void) fprintf(stderr, gettext(ERR_CANT_ACCESS_FILE),
		    name_to_major);
		return (ERROR);
	}

	while (fgets(line, sizeof (line), fp) != 0) {

		if (sscanf(line, "%s %s", drv, drv_majnum) != 2) {
			(void) fprintf(stderr, gettext(ERR_BAD_LINE),
			    name_to_major, line);
			(void) fclose(fp);
			return (ERROR);
		}

		if (atoi(drv_majnum) > max_dev) {
			num_list = realloc(num_list, atoi(drv_majnum));
			if (num_list == NULL) {
				(void) fprintf(stderr, gettext(ERR_NO_MEM));
				return (ERROR);
			}
			/* If realloc succeed, update the max_dev */
			max_dev = atoi(drv_majnum);
		}
		num_list[atoi(drv_majnum)] = 1;
	}

	/*
	 * read thru rem_name_to_major, marking each major number found
	 * order of rem_name_to_major not relevant
	 */

	(void) fclose(fp);
	fp = NULL;

	if (stat(rem_name_to_major, &buf) == 0) {
		if ((fp = fopen(rem_name_to_major, "r")) == NULL) {
			perror(NULL);
			(void) fprintf(stderr, gettext(ERR_CANT_ACCESS_FILE),
				rem_name_to_major);
			return (ERROR);
		}

		while (fgets(line, sizeof (line), fp) != 0) {

			if (sscanf(line, "%s %s", drv, drv_majnum) != 2) {
				(void) fprintf(stderr, gettext(ERR_BAD_LINE),
				name_to_major, line);
				(void) fclose(fp);
				return (ERROR);
			}

			if (atoi(drv_majnum) >= max_dev) {
				num_list = realloc(num_list, atoi(drv_majnum));
				if (num_list == NULL) {
					(void) fprintf(stderr,
						gettext(ERR_NO_MEM));
					return (ERROR);
				}
				/* If realloc succeed, update the max_dev */
				max_dev = atoi(drv_majnum);
			}
			num_list[atoi(drv_majnum)] = 1;
		}
		(void) fclose(fp);
	}

	/* find first free major number */
	for (i = 0; i < max_dev; i++) {
		if (num_list[i] != 1) {
			new_maj = i;
			break;
		}
	}

	if (new_maj == -1) {
		(void) fprintf(stderr, gettext(ERR_NO_FREE_MAJOR));
		return (ERROR);
	}

	(void) sprintf(drv_majnum, "%d", new_maj);
	if (do_the_update(driver_name, drv_majnum) == ERROR) {
		return (ERROR);
	}

	*major_num = new_maj;
	return (NOERR);
}

static void
usage()
{
	(void) fprintf(stderr, gettext(USAGE));
}

/*
 * check each alias :
 *	alias list members separated by white space
 *	cannot exist as driver name in /etc/name_to_major
 *	cannot exist as driver or alias name in /etc/driver_aliases
 */
int
aliases_unique(char *aliases)
{
	char *current_head;
	char *previous_head;
	char *one_entry;
	int i, len;
	int is_unique;

	len = strlen(aliases);

	one_entry = calloc(len + 1, 1);
	if (one_entry == NULL) {
		(void) fprintf(stderr, gettext(ERR_NO_MEM));
		return (ERROR);
	}

	previous_head = aliases;

	do {
		for (i = 0; i <= len; i++)
			one_entry[i] = 0;

		current_head = get_entry(previous_head, one_entry, ' ');
		previous_head = current_head;

		if ((unique_driver_name(one_entry, name_to_major,
		    &is_unique)) == ERROR) {
			free(one_entry);
			return (ERROR);
		}

		if (is_unique != UNIQUE) {
			(void) fprintf(stderr, gettext(ERR_ALIAS_IN_NAM_MAJ),
			    one_entry);
			free(one_entry);
			return (ERROR);
		}

		if (unique_drv_alias(one_entry) != NOERR) {
			free(one_entry);
			return (ERROR);
		}

	} while (*current_head != '\0');

	free(one_entry);

	return (NOERR);

}

int
unique_drv_alias(char *drv_alias)
{
	FILE *fp;
	char drv[FILENAME_MAX + 1];
	char line[MAX_N2M_ALIAS_LINE + 1];
	char alias[FILENAME_MAX + 1];
	int status = NOERR;

	fp = fopen(driver_aliases, "r");

	if (fp != NULL) {
		while ((fgets(line, sizeof (line), fp) != 0) &&
		    status != ERROR) {
			if (sscanf(line, "%s %s", drv, alias) != 2)
				(void) fprintf(stderr, gettext(ERR_BAD_LINE),
				    driver_aliases, line);

			if ((strcmp(drv_alias, drv) == 0) ||
			    (strcmp(drv_alias, alias) == 0)) {
				(void) fprintf(stderr,
				    gettext(ERR_ALIAS_IN_USE),
				    drv_alias);
				status = ERROR;
			}
		}
		(void) fclose(fp);
		return (status);
	} else {
		perror(NULL);
		(void) fprintf(stderr, gettext(ERR_CANT_OPEN), driver_aliases);
		return (ERROR);
	}

}

/*
 * check that major_num doesn`t exceed maximum on this machine
 * do this here (again) to support add_drv on server for diskless clients
 */
int
config_driver(
	char *driver_name,
	major_t major_num,
	char *aliases,
	char *classes,
	int cleanup_flag,
	int noload_flag)
{
	int max_dev;
	int n = 0;
	char *cmdline[MAX_CMD_LINE];
	char maj_num[128];
	char *previous;
	char *current;
	int exec_status;
	int len;
	FILE *fp;

	if (modctl(MODRESERVED, NULL, &max_dev) < 0) {
		perror(NULL);
		(void) fprintf(stderr, gettext(ERR_MAX_MAJOR));
		return (ERROR);
	}

	if (major_num >= max_dev) {
		(void) fprintf(stderr, gettext(ERR_MAX_EXCEEDS),
		    major_num, max_dev);
		return (ERROR);
	}

	/* bind major number and driver name */

	/* build command line */
	cmdline[n++] = DRVCONFIG;
	if (noload_flag)
		cmdline[n++] = "-n";
	cmdline[n++] = "-b";
	if (classes) {
		cmdline[n++] = "-c";
		cmdline[n++] = classes;
	}
	cmdline[n++] = "-i";
	cmdline[n++] = driver_name;
	cmdline[n++] = "-m";
	(void) sprintf(maj_num, "%lu", major_num);
	cmdline[n++] = maj_num;

	if (aliases != NULL) {
		len = strlen(aliases);
		previous = aliases;
		do {
			cmdline[n++] = "-a";
			cmdline[n] = calloc(len + 1, 1);
			if (cmdline[n] == NULL) {
				(void) fprintf(stderr,
				    gettext(ERR_NO_MEM));
				return (ERROR);
			}
			current = get_entry(previous,
			    cmdline[n++], ' ');
			previous = current;

		} while (*current != '\0');

	}
	cmdline[n] = (char *)0;

	exec_status = exec_command(DRVCONFIG_PATH, cmdline);

	if (exec_status != NOERR) {
		perror(NULL);
		remove_entry(cleanup_flag, driver_name);
		return (ERROR);
	}


	/*
	 * now that we have the name to major number bound,
	 * config the driver
	 */

	/*
	 * create /reconfigure file so system reconfigures
	 * on reboot if we're actually loading the driver
	 * now
	 */
	if (!noload_flag) {
		fp = fopen(RECONFIGURE, "a");
		(void) fclose(fp);
	}

	/* build command line */

	n = 0;
	cmdline[n++] = DRVCONFIG;
	if (noload_flag)
		cmdline[n++] = "-n";
	cmdline[n++] = "-i";
	cmdline[n++] = driver_name;
	cmdline[n++] = "-r";
	cmdline[n++] = DEVFS_ROOT;
	cmdline[n] = (char *)0;

	exec_status = exec_command(DRVCONFIG_PATH, cmdline);

	if (exec_status != NOERR) {
		/* no clean : name and major number are bound */
		(void) fprintf(stderr, gettext(ERR_CONFIG), driver_name);
		return (ERROR);
	}

	return (NOERR);
}

static int
update_driver_classes(
	char *driver_name,
	char *classes)
{
	/* make call to update the classes file */
	return (append_to_file(driver_name, classes, driver_classes,
	    ' ', "\t"));
}

static int
update_driver_aliases(
	char *driver_name,
	char *aliases)
{
	/* make call to update the aliases file */
	return (append_to_file(driver_name, aliases, driver_aliases, ' ', " "));

}

static int
update_minor_perm(
	char *driver_name,
	char *perm_list)
{
	return (append_to_file(driver_name, perm_list, minor_perm, ',', ":"));
}

static int
do_the_update(
	char *driver_name,
	char *major_number)
{

	return (append_to_file(driver_name, major_number, name_to_major,
	    ' ', " "));
}

static void
signal_rtn()
{
	exit_unlock();
}

static int
exec_command(
	char *path,
	char *cmdline[MAX_CMD_LINE])
{
	pid_t pid;
	u_int stat_loc;
	int waitstat;
	int exit_status;

	/* child */
	if ((pid = fork()) == 0) {

		(void) execv(path, cmdline);
		perror(NULL);
		return (ERROR);
	} else if (pid == -1) {
		/* fork failed */
		perror(NULL);
		(void) fprintf(stderr, gettext(ERR_FORK_FAIL), cmdline);
		return (ERROR);
	} else {
		/* parent */
		do {
			waitstat = waitpid(pid, (int *)&stat_loc, 0);

		} while ((!WIFEXITED(stat_loc) &&
			!WIFSIGNALED(stat_loc)) || (waitstat == 0));

		exit_status = WEXITSTATUS(stat_loc);

		return (exit_status);
	}
}

/*
 * Check to see if the driver we are adding is a more specific
 * driver for a device already attached to a less specific driver.
 * In other words, see if this driver comes earlier on the compatible
 * list of a device already attached to another driver.
 * If so, the new node will not be created (since the device is
 * already attached) but when the system reboots, it will attach to
 * the new driver but not have a node - we need to warn the user
 * if this is the case.
 */
static int
drv_name_conflict()
{

	/* walk the device tree checking each node */
	if (devfs_find_all(devfs_node)) {	/* failed */
		free_conflict_list(conflict_lst);
		conflict_lst = (struct dev_list *)NULL;
		(void) fprintf(stderr, gettext(ERR_DEVTREE));
		return (-1);
	}

	if (conflict_lst == NULL)
		/* no conflicts found */
		return (0);
	else
		/* conflicts! */
		return (1);
}

/*
 * called via devfs_find().
 * called for each node in the device tree.  We skip nodes that:
 *	1. are not hw nodes (since they cannot have generic names)
 *	2. that do not have a compatible property
 *	3. whose node name = binding name.
 *	4. nexus nodes - the name of a generic nexus node would
 *	not be affected by a driver change.
 * Otherwise, we parse the compatible property, if we find a
 * match with the new driver before we find a match with the
 * current driver, then we have a conflict and we save the
 * node away.
 */
static void
devfs_node(const char *devfsnm, const dev_info_t *dip)
{
	char *binding_name, *node_name;
	struct dev_list *new_entry;
	char strbuf[MAXPATHLEN];

	/*
	 * if there is no compatible property, we don't
	 * have to worry about any conflicts.
	 */
	if (DEVI(dip)->devi_compat_length == 0)
		return;

	/*
	 * if the binding name and the node name match, then
	 * either no driver existed that could be bound to this node,
	 * or the driver name is the same as the node name.
	 */
	binding_name = (char *)local_addr(DEVI(dip)->devi_binding_name);
	node_name = (char *)local_addr(DEVI(dip)->devi_node_name);
	if ((binding_name == NULL) ||
	    (strcmp(node_name, binding_name) == 0))
		return;

	/*
	 * we can skip nexus drivers since they do not
	 * have major/minor number info encoded in their
	 * /devices name and therefore won't change.
	 */
	if (devfs_is_nexus_driver(dip))
		return;

	/*
	 * check for conflicts
	 * If we do find that the new driver is a more specific driver
	 * than the driver already attached to the device, we'll save
	 * away the node name for processing later.
	 */
	if (drv_name_match(dip, binding_name, new_drv)) {
		(void) sprintf(strbuf, "%s/%s", DEVFS_ROOT, devfsnm);
		new_entry = (struct dev_list *)calloc(1,
		    sizeof (struct dev_list));
		if (new_entry == (struct dev_list *)NULL) {
			(void) fprintf(stderr, gettext(ERR_NO_MEM));
			err_exit();
		}
		/* save the /devices name */
		if ((new_entry->dev_name = strdup(strbuf)) == NULL) {
			(void) fprintf(stderr, gettext(ERR_NO_MEM));
			free(new_entry);
			err_exit();
		}
		/* save the driver name */
		(void) strcpy(strbuf, binding_name);
		(void) devfs_resolve_aliases(strbuf);
		if ((new_entry->driver_name = strdup(strbuf)) == NULL) {
			(void) fprintf(stderr, gettext(ERR_NO_MEM));
			free(new_entry->dev_name);
			free(new_entry);
			err_exit();
		}
		/* check to see if this is a clone device */
		if (clone(dip))
			new_entry->clone = 1;

		/* add it to the list */
		new_entry->next = conflict_lst;
		conflict_lst = new_entry;
	}
}

static int
clone(const dev_info_t *dip)
{
	struct ddi_minor_data *dp;

	dp = (struct ddi_minor_data *)
	    local_addr((caddr_t)DEVI(dip)->devi_minor);

	while (dp != (struct ddi_minor_data *)NULL) {
		if (dp->type == DDM_ALIAS)
			return (1);
		dp = (struct ddi_minor_data *)local_addr((caddr_t)dp->next);
	}
	return (0);
}
/*
 * check to see if the new_name shows up on the compat list before
 * the cur_name (driver currently attached to the device).
 */
static int
drv_name_match(const dev_info_t *dip, char *cur_name, char *new_name)
{
	char *compat_list;
	char *p = 0;
	size_t compat_len;
	int ret = 0;

	if (strcmp(cur_name, new_name) == 0)
		return (0);

	compat_list = (char *)local_addr(DEVI(dip)->devi_compat_names);
	compat_len = DEVI(dip)->devi_compat_length;

	/* parse the coompatible list */
	while ((p = decode_composite_string(compat_list, compat_len, p))
	    != 0) {
		if (strcmp(p, new_name) == 0) {
			ret = 1;
			break;
		}
		if (strcmp(p, cur_name) == 0) {
			break;
		}
	}
	return (ret);
}

/*
 * A more specific driver is being added for a device already attached
 * to a less specific driver.  Print out a general warning and if
 * the force flag was passed in, give the user a hint as to what
 * nodes may be affected in /devices and /dev
 */
static void
print_drv_conflict_info(int force)
{
	struct dev_list *ptr;

	if (conflict_lst == NULL)
		return;
	if (force) {
		(void) fprintf(stderr,
		    "\nA reconfiguration boot must be performed to "
		    "complete the\n");
		(void) fprintf(stderr, "installation of this driver.\n");
	}

	if (force) {
		(void) fprintf(stderr,
		    "\nThe following entries in /devices will be "
		    "affected:\n\n");
	} else {
		(void) fprintf(stderr,
		    "\nDriver installation failed because the following\n");
		(void) fprintf(stderr,
		    "entries in /devices would be affected:\n\n");
	}

	ptr = conflict_lst;
	while (ptr != NULL) {
		(void) fprintf(stderr, "\t%s", ptr->dev_name);
		if (ptr->clone)
			(void) fprintf(stderr, " (clone device)\n");
		else
			(void) fprintf(stderr, "[:*]\n");
		(void) fprintf(stderr, "\t(Device currently managed by driver "
		    "\"%s\")\n\n", ptr->driver_name);
		ptr = ptr->next;
	}
	check_dev_dir(force);
}

/*
 * use nftw to walk through /dev looking for links that match
 * an entry in the conflict list.
 */
static void
check_dev_dir(int force)
{
	int  walk_flags = FTW_PHYS | FTW_MOUNT;
	int ft_depth = 15;

	if (force) {
		(void) fprintf(stderr, "\nThe following entries in /dev will "
		    "be affected:\n\n");
	} else {
		(void) fprintf(stderr, "\nThe following entries in /dev would "
		    "be affected:\n\n");
	}

	(void) nftw("/dev", dev_node, ft_depth, walk_flags);

	(void) fprintf(stderr, "\n");
}

/*
 * checks a /dev link to see if it matches any of the conlficting
 * /devices nodes in conflict_lst.
 */
/*ARGSUSED1*/
static int
dev_node(const char *node, const struct stat *node_stat, int flags,
	struct FTW *ftw_info)
{
	char linkbuf[MAXPATHLEN];
	struct dev_list *ptr;

	if (readlink(node, linkbuf, MAXPATHLEN) == -1)
		return (0);

	ptr = conflict_lst;

	while (ptr != NULL) {
		if (strstr(linkbuf, ptr->dev_name) != NULL)
			(void) fprintf(stderr, "\t%s\n", node);
		ptr = ptr->next;
	}
	return (0);
}


/*
 * Returns successive strings in a composite string property.
 * A composite string property is a buffer containing one or more
 * NULL terminated strings contained within the length of the buffer.
 *
 * Always call with the base address and of the property buffer.
 * On the first call, call with prev == 0, call successively
 * with prev == to the last value returned from this function
 * until the routine returns zero which means no more string values.
 */
static char *
decode_composite_string(char *buf, size_t buflen, char *prev)
{
	if ((buf == 0) || (buflen == 0) || ((int)buflen == -1))
		return ((char *)0);

	if (prev == 0)
		return (buf);

	prev += strlen(prev) + 1;
	if (prev >= (buf + buflen))
		return ((char *)0);
	return (prev);
}

static void
free_conflict_list(struct dev_list *list)
{
	struct dev_list *save;

	/* free up any dev_list structs we allocated. */
	while (list != NULL) {
		save = list;
		list = list->next;
		free(save->dev_name);
		free(save);
	}
}

static int
check_space_within_quote(char *str)
{
	register int i;
	register int len;
	int quoted = 0;

	len = strlen(str);
	for (i = 0; i < len; i++, str++) {
		if (*str == '"') {
			if (quoted == 0)
				quoted++;
			else
				quoted--;
		} else if (*str == ' ' && quoted)
			return (ERROR);
	}

	return (0);
}
