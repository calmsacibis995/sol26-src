/*
 * Copyright (c) 1990-1993, Sun Microsystems, Inc.
 */

#pragma	ident	"@(#)sdevinfo.c	1.13	96/01/03 SMI"

/*
 * For machines that support the openprom, fetch and print the list
 * of devices that the kernel has fetched from the prom or conjured up.
 */

#include <stdio.h>
#include <string.h>
#include <kvm.h>
#include <nlist.h>
#include <fcntl.h>
#include <varargs.h>
#include <sys/utsname.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>

/*
 * Structure to map device names to major numbers.
 */
struct name_to_major {
	char    *ntm_name;
	major_t ntm_major;
	struct name_to_major *ntm_next;
};

/*
 * Structure to map driver names to aliases
 */
struct driver_alias {
	char    *driver;
	char    *alias;
	struct driver_alias *drv_alias_next;
};

/*
 * function declarations
 */

extern char *malloc();
static void build_devs(), walk_devs(), dump_devs();
static char *getkname();
static int _error();
static struct driver_alias *read_driver_aliases(void);
static char *strip_quotes(char *);
static char *resolve_drv_alias(char *);
static struct name_to_major *read_name_to_major(void);
static int valid_drv_name(char *);

/*
 * local data
 */
static kvm_t *kd;
static char *mfail = "malloc";
static char *progname = "sysdef";
static struct driver_alias *drv_alias_head;
static struct name_to_major *ntm_head;

extern int devflag;		/* SunOS4.x devinfo compatible output */
extern int drvname_flag;	/* print out the driver name too */

#define	DSIZE	(sizeof (struct dev_info))
#define	P_DSIZE	(sizeof (struct dev_info *))
#define	NO_PERROR	((char *) 0)

#define TRUE  1
#define FALSE 0


void
sysdef_devinfo(void)
{
	struct dev_info root_node;
	dev_info_t *rnodep;
	static struct nlist list[] = { { "top_devinfo" }, 0 };

	/* cache the driver_aliases and name_to_major files if needed */
	if (drvname_flag) {
		ntm_head = read_name_to_major();
		drv_alias_head = read_driver_aliases();
	}

	if ((kd = kvm_open((char *)0, (char *)0, (char *)0, O_RDONLY, progname))
	    == NULL) {
		exit(_error("kvm_open failed"));
	} else if ((kvm_nlist(kd, &list[0])) != 0) {
		struct utsname name_buf;
		(void)uname (&name_buf);
		(void)_error(NO_PERROR,
	    	    "%s not available on kernel architecture %s (yet).",
		    progname, name_buf.machine);
		exit(1);
	}

	/*
	 * first, build the root node...
	 */

	if (kvm_read(kd, list[0].n_value, (char *)&rnodep, P_DSIZE)
	    != P_DSIZE) {
		exit(_error("kvm_read of root node pointer fails"));
	}

	if (kvm_read(kd, (u_long)rnodep, (char *)&root_node, DSIZE) != DSIZE) {
		exit(_error("kvm_read of root node fails"));
	}

	/*
	 * call build_devs to fetch and construct a user space copy
	 * of the dev_info tree.....
	 */

	build_devs(&root_node);
	(void) kvm_close(kd);

	/*
	 * ...and call walk_devs to report it out...
	 */
	walk_devs (&root_node);
}

/*
 * build_devs copies all appropriate information out of the kernel
 * and gets it into a user addrssable place. the argument is a
 * pointer to a just-copied out to user space dev_info structure.
 * all pointer quantities that we might de-reference now have to
 * be replaced with pointers to user addressable data.
 */

static void
build_devs(dp)
register dev_info_t *dp;
{
	char *tptr;
	unsigned amt;

	if (DEVI(dp)->devi_node_name)
		DEVI(dp)->devi_node_name = getkname(DEVI(dp)->devi_node_name);

	if (DEVI(dp)->devi_binding_name)
		DEVI(dp)->devi_binding_name =
		    getkname(DEVI(dp)->devi_binding_name);

	if (DEVI(dp)->devi_child) {
		if (!(tptr = malloc(DSIZE))) {
			exit(_error(mfail));
		}
		if (kvm_read(kd, (u_long)DEVI(dp)->devi_child, tptr, DSIZE)
		    != DSIZE) {
			exit(_error("kvm_read of devi_child"));
		}
		DEVI(dp)->devi_child = (struct dev_info *) tptr;
		build_devs(DEVI(dp)->devi_child);
	}

	if (DEVI(dp)->devi_sibling) {
		if (!(tptr = malloc(DSIZE))) {
			exit(_error(mfail));
		}
		if (kvm_read(kd, (u_long)DEVI(dp)->devi_sibling, tptr, DSIZE) !=
		    DSIZE) {
			exit(_error("kvm_read of devi_sibling"));
		}
		DEVI(dp)->devi_sibling = (struct dev_info *) tptr;
		build_devs(DEVI(dp)->devi_sibling);
	}
}

/*
 * print out information about this node, descend to children, then
 * go to siblings
 */

static void
walk_devs(dp)
register dev_info_t *dp;
{
	static int root_yn      = TRUE;
	static int indent_level = -1;		/* we would start at 0, except
						   that we skip the root node*/
	char *driver_name;
	register i;

	if (devflag && indent_level < 0)
		indent_level = 0;

	for (i = 0; i < indent_level; i++)
		(void)putchar('\t');

	if (root_yn && !devflag)
		root_yn = FALSE;
	else {
		if (devflag) {
			/*
			 * 4.x devinfo(8) compatible..
			 */
			(void) printf("Node '%s', unit #%d",
				DEVI(dp)->devi_node_name,
				DEVI(dp)->devi_instance);
			if (drvname_flag) {
				driver_name = resolve_drv_alias(
				    DEVI(dp)->devi_binding_name);
				if (valid_drv_name(driver_name)) {
					(void) printf(" (driver name: %s)",
					    driver_name);
				}
			} else if (DEVI(dp)->devi_ops == NULL) {
				(void) printf(" (no driver)");
			}
		} else {
			/*
			 * prtconf(1M) compatible..
			 */
			(void) printf("%s", DEVI(dp)->devi_node_name);
			if (DEVI(dp)->devi_instance >= 0)
				(void) printf(", instance #%d",
				    DEVI(dp)->devi_instance);
			if (drvname_flag) {
				driver_name = resolve_drv_alias(
				    DEVI(dp)->devi_binding_name);
				if (valid_drv_name(driver_name)) {
					(void) printf(" (driver name: %s)",
					    driver_name);
				}
			} else if (DEVI(dp)->devi_ops == NULL) {
				(void) printf(" (driver not attached)");
			}
		}
		dump_devs(dp, indent_level+1);
		(void)printf("\n");	
	}
	if (DEVI(dp)->devi_child) {
		indent_level++;
		walk_devs(DEVI(dp)->devi_child);
		indent_level--;
	}
	if (DEVI(dp)->devi_sibling) {
		walk_devs(DEVI(dp)->devi_sibling);
	}
}

/*
 * utility routines
 */

static void
dump_devs(dp, ilev)
register dev_info_t *dp;
{
}

static char *
getkname(kaddr)
char *kaddr;
{
	auto char buf[32], *rv;
	register i = 0;
	char c;

	if (kaddr == (char *) 0) {
		(void)strcpy(buf, "<null>");
		i = 7;
	} else {
		while (i < 31) {
			if (kvm_read(kd, (u_long)kaddr++, (char *)&c, 1) != 1) {
				exit(_error("kvm_read of name string"));
			}
			if ((buf[i++] = c) == (char) 0)
				break;
		}
		buf[i] = 0;
	}
	if ((rv = malloc((unsigned)i)) == 0) {
		exit(_error(mfail));
	}
	strncpy(rv, buf, i);
	return (rv);
}

/* _error([no_perror, ] fmt [, arg ...]) */
/*VARARGS*/
static int
_error(va_alist)
va_dcl
{
	int saved_errno;
	va_list ap;
	int no_perror = 0;
	char *fmt;
	extern int errno, _doprnt();

	saved_errno = errno;

	if (progname)
		(void) fprintf(stderr, "%s: ", progname);

	va_start(ap);
	if ((fmt = va_arg(ap, char *)) == 0) {
		no_perror = 1;
		fmt = va_arg(ap, char *);
	}
	(void) _doprnt(fmt, ap, stderr);
	va_end(ap);

	if (no_perror)
		(void) fprintf(stderr, "\n");
	else {
		(void) fprintf(stderr, ": ");
		errno = saved_errno;
		perror("");
	}

	return (1);
}

static struct name_to_major *
read_name_to_major()
{
	FILE    *fp;
	char    name[MAXNAMELEN + 1];
	int     major;
	int     rval;
	struct name_to_major *head = NULL;
	struct name_to_major *tail = NULL;
	struct name_to_major *new;

	fp = fopen("/etc/name_to_major", "r");
	if (fp == NULL) {
		exit(_error("open of /etc/name_to_major failed"));
	}
	while ((rval = fscanf(fp, "%s%d", name, &major)) != EOF) {
		if (rval != 2) {
			exit(_error("parse error in /etc/name_to_major"));
		}

		if (major < 0) {
			exit(_error(
			    "invalid entry in /etc/name_to_major"));
		}

		new = (struct name_to_major *)
		    malloc(sizeof (struct name_to_major));
		if (new == NULL) {
			exit(_error(mfail));
		}

		new->ntm_next = NULL;
		new->ntm_major = major;
		new->ntm_name = strdup(name);
		if (new->ntm_name == NULL) {
			exit(_error(mfail));
		}

		if (head == NULL) {
			head = new;
			head->ntm_next = NULL;
			tail = head;
		} else {
			tail->ntm_next = new;
			tail = tail->ntm_next;
		}
	}
	fclose(fp);
	return (head);
}

static struct driver_alias *
read_driver_aliases()
{
	FILE    *fp;
	char	line[2 * MAXNAMELEN + 1];
	int	line_len = 2 * MAXNAMELEN + 1;
	char    drv_name[MAXNAMELEN + 1];
	char    drv_alias[MAXNAMELEN + 1];
	int     rval;
	struct driver_alias *head = NULL;
	struct driver_alias *tail = NULL;
	struct driver_alias *new;

	fp = fopen("/etc/driver_aliases", "r");
	if (fp == NULL) {
		return ((struct driver_alias *)NULL);
	}
	while (fgets(line, line_len, fp) != NULL) {
		rval = sscanf(line, "%s%s", drv_name, drv_alias);
		if (rval != 2) {
			exit(_error("parse error in /etc/driver_aliases"));
		}

		new = (struct driver_alias *)malloc(
		    sizeof (struct driver_alias));
		if (new == NULL) {
			exit(_error(mfail));
		}

		new->drv_alias_next = NULL;
		new->alias = strdup(strip_quotes(drv_alias));
		new->driver = strdup(strip_quotes(drv_name));
		if ((new->alias == NULL) || (new->driver == NULL)) {
			exit(_error(mfail));
		}

		if (head == NULL) {
			head = new;
			head->drv_alias_next = NULL;
			tail = head;
		} else {
			tail->drv_alias_next = new;
			tail = tail->drv_alias_next;
		}
	}
	fclose(fp);
	return (head);
}
static char *
resolve_drv_alias(char *drv_alias)
{
	struct driver_alias *alias_entry = drv_alias_head;

	if (drv_alias == NULL)
		return (drv_alias);
	/*
	 * search the list of driver aliases
	 */
	while (alias_entry != NULL) {
		if (strcmp(drv_alias, alias_entry->alias) == 0)
			return (alias_entry->driver);
		alias_entry = alias_entry->drv_alias_next;
	}
	/*
	 * no match in the driver_aliases file so this must
	 * be the driver name
	 */
	return (drv_alias);
}
/*
 * strip surrounding quotes from a driver alias name
 * if the quoting look wierd, then we just return the name as is
 */
static char *
strip_quotes(char *name)
{
	static char buf[MAXNAMELEN + 1];
	int end_quote;

	strcpy(buf, name);
	end_quote = strlen(buf) - 1;

	/*
	 * does it begin  and end with a quote?
	 */
	if ((buf[0] == '\"') && (buf[end_quote] == '\"')) {
		buf[end_quote] = '\0';
		return (buf + 1);
	} else {
		return (buf);
	}
}

static int
valid_drv_name(char *name)
{
	struct name_to_major *tmp;

	if (name == NULL)
		return (0);

	for (tmp = ntm_head; tmp != NULL; tmp = tmp->ntm_next) {
		if (strcmp(tmp->ntm_name, name) == 0) {
			return (1);
		}
	}
	return (0);
}
