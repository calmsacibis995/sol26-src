/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)psrset.c	1.1	96/05/20 SMI"

/*
 * psrset - create and manage processor sets
 */

#include <sys/types.h>
#include <sys/procset.h>
#include <sys/processor.h>
#include <sys/pset.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <locale.h>
#include <unistd.h>
#include <string.h>

#if !defined(TEXT_DOMAIN)		/* should be defined by cc -D */
#define	TEXT_DOMAIN 	"SYS_TEST"	/* Use this only if it wasn't */
#endif

#define	PROCDIR	"/proc"

static char *cmdname = "psrset";

static int	do_info(psetid_t pset);
static void	create_out(psetid_t pset);
static void	destroy_out(psetid_t pset);
static void	assign_out(processorid_t cpu, psetid_t old, psetid_t new);
static void	info_out(psetid_t pset, int type, u_int numcpus,
		    processorid_t *cpus);
static void	print_out(processorid_t cpu, psetid_t pset);
static void	query_out(id_t pid, psetid_t cpu);
static void	bind_out(id_t pid, psetid_t old, psetid_t new);
static int	info_all(void);
static int	print_all(void);
static int	query_all(void);
static void	errmsg(char *msg);
static void	usage(void);
static int	comparepset(const void *x, const void *y);

int
main(int argc, char *argv[])
{
	extern int optind;
	int	c;
	id_t	pid;
	processorid_t	cpu;
	psetid_t	pset, old_pset;
	char	*errptr;
	char	create = 0;
	char	destroy = 0;
	char	assign = 0;
	char	remove = 0;
	char	info = 0;
	char	bind = 0;
	char	unbind = 0;
	char	query = 0;
	char	print = 0;
	int	errors;
	char	buf[256];

	cmdname = argv[0];	/* put actual command name in messages */

	(void) setlocale(LC_ALL, "");	/* setup localization */
	(void) textdomain(TEXT_DOMAIN);

	while ((c = getopt(argc, argv, "cdarpibqu")) != EOF) {
		switch (c) {

		case 'c':
			create = 1;
			break;

		case 'd':
			destroy = 1;
			break;

		case 'a':
			assign = 1;
			break;

		case 'r':
			remove = 1;
			pset = PS_NONE;
			break;

		case 'p':
			print = 1;
			pset = PS_QUERY;
			break;

		case 'i':
			info = 1;
			break;

		case 'b':
			bind = 1;
			break;

		case 'u':
			unbind = 1;
			pset = PS_NONE;
			break;

		case 'q':
			query = 1;
			pset = PS_QUERY;
			break;

		default:
			usage();
			return (2);
		}
	}


	/*
	 * Make sure that at most one of the options was specified.
	 */
	c = create + destroy + assign + remove + print +
	    info + bind + unbind + query;
	if (c < 1) {				/* nothing specified */
		info = 1;			/* default is to get info */
	} else if (c > 1) {
		errmsg("options -c, -d, -a, -r, -p, -b, -u, -q, and -i are "
		    "mutually exclusive.");
		usage();
		return (2);
	}

	errors = 0;
	argc -= optind;
	argv += optind;

	/*
	 * Handle query of all processes.
	 */
	if (query && argc == 0)
		return (query_all());

	/*
	 * Handle print for all processors.
	 */
	if (print && argc == 0)
		return (print_all());

	/*
	 * Handle info for all processors.
	 */
	if (info && argc == 0)
		return (info_all());

	/*
	 * Get processor set id.
	 */
	if (destroy || assign || bind) {
		if (argc < 1) {
			/* must specify processor set */
			errmsg("must specify processor set");
			usage();
			return (2);
		}
		pset = strtol(*argv, &errptr, 10);
		if (errptr != NULL && *errptr != '\0' || pset < 0) {
			(void) fprintf(stderr, "%s: %s %s\n", cmdname,
			    gettext("invalid processor set ID"), *argv);
			return (1);
		}
		argv++;
		argc--;
	}

	if (create) {
		if (pset_create(&pset) != 0) {
			(void) sprintf(buf, "%s: %s", cmdname,
			    gettext("could not create processor set"));
			perror(buf);
			return (-1);
		} else {
			create_out(pset);
			if (argc == 0)
				return (0);
		}
	} else if (destroy) {
		if (pset_destroy(pset) != 0) {
			(void) sprintf(buf, "%s: %s %d", cmdname,
			    gettext("could not remove processor set"),
			    pset);
			perror(buf);
			return (-1);
		}
		destroy_out(pset);
	} else if (info) {
		if (argc == 0) {
			errmsg("must specify at least one processor set");
			usage();
			return (2);
		}
		/*
		 * Go through listed processor sets.
		 */
		for (; argc > 0; argv++, argc--) {
			pset = (psetid_t) strtol(*argv, &errptr, 10);
			if (errptr != NULL && *errptr != '\0') {
				(void) fprintf(stderr, "%s: %s %s\n", cmdname,
				    gettext("invalid processor set ID"),
				    *argv);
				errors = 2;
				continue;
			}
			errors = do_info(pset);
		}
	}

	if (create || assign || remove || print) {
		/*
		 * Perform function for each processor specified.
		 */
		if (argc == 0) {
			errmsg("must specify at least one processor");
			usage();
			return (2);
		}

		/*
		 * Go through listed processors.
		 */
		for (; argc > 0; argv++, argc--) {
			cpu = (processorid_t) strtol(*argv, &errptr, 10);
			if (errptr != NULL && *errptr != '\0') {
				(void) fprintf(stderr, "%s: %s %s\n", cmdname,
				    gettext("invalid processor ID"),
				    *argv);
				errors = 2;
				continue;
			}
			if (pset_assign(pset, cpu, &old_pset)
			    != 0) {
				char	*msg;

				(void) strcpy(buf, cmdname);
				(void) strcat(buf, ": ");

				switch (pset) {
				case PS_NONE:
					msg = "%s: cannot remove processor %d";
					break;
				case PS_QUERY:
					msg = "%s: cannot query processor %d";
					break;
				default:
					msg = "%s: cannot assign processor %d";
					break;
				}
				(void) sprintf(buf, gettext(msg),
				    cmdname, cpu);
				perror(buf);
				errors = 1;
				continue;
			}
			if (print)
				print_out(cpu, old_pset);
			else
				assign_out(cpu, old_pset, pset);
		}
	} else if (bind || unbind || query) {
		/*
		 * Perform function for each pid specified.
		 */
		if (argc == 0) {
			errmsg("must specify at least one pid");
			usage();
			return (2);
		}

		/*
		 * Go through listed processes.
		 */
		for (; argc > 0; argv++, argc--) {
			pid = (id_t) strtol(*argv, &errptr, 10);
			if (errptr != NULL && *errptr != '\0') {
				(void) fprintf(stderr, "%s: %s %s\n", cmdname,
				    gettext("invalid process ID (pid)"),
				    *argv);
				errors = 2;
				continue;
			}
			if (pset_bind(pset, P_PID, pid, &old_pset)
			    != 0) {
				char	buf[256];
				char	*msg;

				(void) strcpy(buf, cmdname);
				(void) strcat(buf, ": ");

				switch (pset) {
				case PS_NONE:
					msg = "%s: cannot unbind pid %d";
					break;
				case PS_QUERY:
					msg = "%s: cannot query pid %d";
					break;
				default:
					msg = "%s: cannot bind pid %d";
					break;
				}
				(void) sprintf(buf, gettext(msg),
				    cmdname, pid);
				perror(buf);
				errors = 1;
				continue;
			}
			if (query)
				query_out(pid, old_pset);
			else
				bind_out(pid, old_pset, pset);
		}
	}
	return (errors);
}

static int
do_info(pset)
{
	int	type;
	u_int	numcpus;
	processorid_t	*cpus;
	char	buf[256];

	numcpus = (u_int)sysconf(_SC_NPROCESSORS_CONF);
	cpus = (processorid_t *)
	    malloc(numcpus * sizeof (processorid_t));
	if (pset_info(pset, &type, &numcpus, cpus) != 0) {
		free(cpus);
		(void) sprintf(buf, "%s: %s %d", cmdname,
		    gettext("cannot get info for processor set"), pset);
		perror(buf);
		return (1);
	}
	info_out(pset, type, numcpus, cpus);
	free(cpus);
	return (0);
}

/*
 * Query the type and CPUs for all active processor sets in the system.
 * We do this by getting the processor sets for each CPU in the system,
 * sorting the list, and displaying the information for each unique
 * processor set.  We also try to find any processor sets that may not
 * have processors assigned to them.  This assumes that the kernel will
 * generally assign low-numbered processor set id's and do aggressive
 * recycling of the id's of destroyed sets.  Yeah, this is ugly.
 */
static int
info_all(void)
{
	psetid_t	*psetlist;
	psetid_t	lastpset, pset;
	int	numpsets = 0, maxpsets;
	u_int	numcpus;
	int	cpuid;
	int	i;
	int	errors = 0;

	numcpus = (u_int)sysconf(_SC_NPROCESSORS_CONF);
	maxpsets = numcpus * 3;		/* should be "enough" */
	psetlist = (psetid_t *)malloc(sizeof (psetid_t) * maxpsets);
	for (cpuid = 0; numcpus != 0; cpuid++) {
		if (pset_assign(PS_QUERY, cpuid, &psetlist[numpsets])
		    == 0) {
			numcpus--;
			numpsets++;
		}
	}
	for (pset = 0; pset < maxpsets; pset++) {
		if (pset_info(pset, NULL, NULL, NULL) == 0)
			psetlist[numpsets++] = pset;
	}
	qsort(psetlist, (size_t)numpsets, sizeof (psetid_t), comparepset);
	lastpset = PS_NONE;
	for (i = 0; i < numpsets; i++) {
		pset = psetlist[i];
		if (pset == lastpset)
			continue;
		lastpset = pset;
		if (do_info(pset))
			errors = 1;
	}
	free(psetlist);
	return (errors);
}

static int
comparepset(const void *x, const void *y)
{
	psetid_t *a = (psetid_t *)x;
	psetid_t *b = (psetid_t *)y;

	if (*a > *b)
		return (1);
	if (*a < *b)
		return (-1);
	return (0);
}

/*
 * Output for create.
 */
static void
create_out(psetid_t pset)
{
	(void) printf("%s %d\n", gettext("created processor set"), pset);
}

/*
 * Output for destroy.
 */
static void
destroy_out(psetid_t pset)
{
	(void) printf("%s %d\n", gettext("removed processor set"), pset);
}

/*
 * Output for assign.
 */
static void
assign_out(processorid_t cpu, psetid_t old, psetid_t new)
{
	if (old == PS_NONE) {
		if (new == PS_NONE)
			(void) printf(gettext("processor %d: was not assigned,"
			    " now not assigned\n"), cpu);
		else
			(void) printf(gettext("processor %d: was not assigned,"
			    " now %d\n"), cpu, new);
	} else {
		if (new == PS_NONE)
			(void) printf(gettext("processor %d: was %d, "
			    "now not assigned\n"), cpu, old);
		else
			(void) printf(gettext("processor %d: was %d, "
			    "now %d\n"), cpu, old, new);
	}
}

/*
 * Output for query.
 */
static void
query_out(id_t pid, psetid_t pset)
{
	if (pset == PS_NONE)
		(void) printf(gettext("process id %d: not bound\n"), pid);
	else
		(void) printf(gettext("process id %d: %d\n"), pid, pset);
}

/*
 * Output for info.
 */
static void
info_out(psetid_t pset, int type, u_int numcpus, processorid_t *cpus)
{
	int i;
	if (type == PS_SYSTEM)
		(void) printf(gettext("system processor set %d:"), pset);
	else
		(void) printf(gettext("user processor set %d:"), pset);
	if (numcpus == 0)
		(void) printf(gettext(" empty"));
	else if (numcpus > 1)
		(void) printf(gettext(" processors"));
	else
		(void) printf(gettext(" processor"));
	for (i = 0; i < numcpus; i++)
		(void) printf(" %d", cpus[i]);
	(void) printf("\n");
}

/*
 * Output for print.
 */
static void
print_out(processorid_t cpu, psetid_t pset)
{
	if (pset == PS_NONE)
		(void) printf(gettext("processor %d: not assigned\n"), cpu);
	else
		(void) printf(gettext("processor %d: %d\n"), cpu, pset);
}

/*
 * Output for bind.
 */
static void
bind_out(id_t pid, psetid_t old, psetid_t new)
{
	if (old == PS_NONE) {
		if (new == PS_NONE)
			(void) printf(gettext("process id %d: was not bound, "
				"now not bound\n"), pid);
		else
			(void) printf(gettext("process id %d: was not bound, "
				"now %d\n"), pid, new);
	} else {
		if (new == PS_NONE)
			(void) printf(gettext("process id %d: was %d, "
				"now not bound\n"), pid, old);
		else
			(void) printf(gettext("process id %d: was %d, "
				"now %d\n"), pid, old, new);
	}
}

/*
 * Query the processor set assignments for all CPUs in the system.
 */
static int
print_all(void)
{
	psetid_t	pset;
	u_int	numcpus;
	int	cpuid;
	char	buf[256];
	int	error;
	int	errors = 0;

	numcpus = (u_int)sysconf(_SC_NPROCESSORS_CONF);
	for (cpuid = 0; numcpus != 0; cpuid++) {
		error = pset_assign(PS_QUERY, cpuid, &pset);
		if (error == 0) {
			numcpus--;
			if (pset != PS_NONE)
				print_out(cpuid, pset);
		} else if (error != EINVAL) {
			(void) sprintf(buf,
			    gettext("%s: cannot query processor %d\n"),
			    cmdname, cpuid);
			perror(buf);
			errors = 1;
		}
	}
	return (errors);
}

/*
 * Query the binding for all processes in the system.
 */
static int
query_all(void)
{
	DIR	*procdir;
	struct dirent	*dp;
	char	buf[256];
	id_t	pid;
	psetid_t binding;
	char	*errptr;
	int	errors = 0;

	procdir = opendir(PROCDIR);

	if (procdir == NULL) {
		(void) sprintf(buf, "%s: %s " PROCDIR "\n", cmdname,
			gettext("cannot open"));
		perror(buf);
		return (2);
	}

	while ((dp = readdir(procdir)) != NULL) {

		/*
		 * skip . and .. (and anything else like that)
		 */
		if (dp->d_name[0] == '.')
			continue;
		pid = (id_t) strtol(dp->d_name, &errptr, 10);
		if (errptr != NULL && *errptr != '\0') {
			(void) fprintf(stderr, gettext("%s: invalid process ID"
				" (pid) %s in %s\n"),
				cmdname, dp->d_name, PROCDIR);
			errors = 2;
			continue;
		}
		if (pset_bind(PS_QUERY, P_PID, pid, &binding) < 0) {
			/*
			 * Ignore search errors.  The process may have exited
			 * since we read the directory.
			 */
			if (errno == ESRCH)
				continue;
			(void) sprintf(buf,
			    gettext("%s: cannot query process %d\n"),
			    cmdname, pid);
			perror(buf);
			errors = 1;
			continue;
		}
		if (binding != PS_NONE)
			query_out(pid, binding);
	}
	return (errors);
}

static void
errmsg(char *msg)
{
	(void) fprintf(stderr, "%s: %s\n", cmdname, gettext(msg));
}

void
usage(void)
{
	(void) fprintf(stderr, gettext(
		"usage: \n"
		"\t%s -c [processor_id ...]\n"
		"\t%s -d processor_set_id\n"
		"\t%s -a processor_set_id processor_id ...\n"
		"\t%s -r processor_id ...\n"
		"\t%s -p [processorid ...]\n"
		"\t%s -b processor_set_id pid ...\n"
		"\t%s -u pid ...\n"
		"\t%s -q [pid ...]\n"
		"\t%s [-i] [processor_set_id ...]\n"),
		cmdname, cmdname, cmdname, cmdname, cmdname,
		cmdname, cmdname, cmdname, cmdname);
}
