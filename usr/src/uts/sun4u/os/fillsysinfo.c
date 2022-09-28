/*
 * Copyright (c) 1992, by Sun Microsystems, Inc.
 */

#ident	"@(#)fillsysinfo.c	1.57	96/10/15 SMI"

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
#include <sys/clock.h>

#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/systm.h>
#include <sys/machsystm.h>
#include <sys/debug.h>
#include <sys/sysiosbus.h>
#include <sys/sunddi.h>
#include <sys/modctl.h>
#include <sys/spitregs.h>

/*
 * The OpenBoot Standalone Interface supplies the kernel with
 * implementation dependent parameters through the devinfo/property mechanism
 */
typedef enum { XDRBOOL, XDRINT, XDRSTRING } xdrs;

/*
 * structure describing properties that we are interested in querying the
 * OBP for.
 */
struct getprop_info {
	char   *name;
	xdrs	type;
	u_int  *var;
};

/*
 * structure used to convert between a string returned by the OBP & a type
 * used within the kernel. We prefer to paramaterize rather than type.
 */
struct convert_info {
	char *name;
	u_int var;
	char *realname;
};

/*
 * structure describing nodes that we are interested in querying the OBP for
 * properties.
 */
struct node_info {
	char			*name;
	int			size;
	struct getprop_info	*prop;
	struct getprop_info	*prop_end;
	unsigned int		*value;
};

/*
 * macro definitions for routines that form the OBP interface
 */
#define	NEXT			prom_nextnode
#define	CHILD			prom_childnode
#define	GETPROP			prom_getprop
#define	GETPROPLEN		prom_getproplen


/* 0=quiet; 1=verbose; 2=debug */
int	debug_fillsysinfo = 0;
#define	VPRINTF if (debug_fillsysinfo) prom_printf

#define	CLROUT(a, l)			\
{					\
	register int c = l;		\
	register char *p = (char *)a;	\
	while (c-- > 0)			\
		*p++ = 0;		\
}

#define	CLRBUF(a)	CLROUT(a, sizeof (a))

int ncpunode;
struct cpu_node cpunodes[NCPU];
char cpu_info_buf[NCPU][CPUINFO_SZ];

static void	check_cpus(void);
static void	fill_cpu(dnode_t);

extern dnode_t iommu_nodes[];

/*
 * list of well known devices that must be mapped, and the variables that
 * contain their addresses.
 */
caddr_t			v_auxio_addr = (caddr_t)0;
caddr_t			v_eeprom_addr = (caddr_t)0;
int			niobus = 0;

/*
 * Hardware watchdog support.
 */
#define	CHOSEN_EEPROM	"eeprom"
#define	WATCHDOG_ENABLE "watchdog-enable"
static dnode_t 		chosen_eeprom;

/*
 * Some nodes have functions that need to be called when they're seen.
 */
static void	have_sbus();
static void	have_pci();
static void	have_eeprom();
static void	have_auxio();
static void	have_flashprom(dnode_t);

static struct wkdevice {
	char *wk_namep;
	void (*wk_func)(dnode_t);
	caddr_t *wk_vaddrp;
	u_short wk_flags;
#define	V_OPTIONAL	0x0000
#define	V_MUSTHAVE	0x0001
#define	V_MAPPED	0x0002
#define	V_MULTI		0x0003	/* optional, may be more than one */
} wkdevice[] = {
	{ "sbus", have_sbus, NULL, V_MULTI },
	{ "pci", have_pci, NULL, V_MULTI },
	{ "eeprom", have_eeprom, NULL, V_MULTI },
	{ "auxio", have_auxio, NULL, V_OPTIONAL },
	{ "flashprom", have_flashprom, NULL, V_MULTI },
	{ 0, },
};

static void map_wellknown(dnode_t);

void
map_wellknown_devices()
{
	struct wkdevice *wkp;
	phandle_t	ieeprom;

	/*
	 * if there is a chosen eeprom, note it (for have_eeprom())
	 */
	if (GETPROPLEN(prom_chosennode(), CHOSEN_EEPROM) ==
	    sizeof (phandle_t) &&
	    GETPROP(prom_chosennode(), CHOSEN_EEPROM, (caddr_t)&ieeprom) != -1)
		chosen_eeprom = (dnode_t)prom_decode_int(ieeprom);

	map_wellknown(NEXT((dnode_t)0));

	/*
	 * See if it worked
	 */
	for (wkp = wkdevice; wkp->wk_namep; ++wkp) {
		if (wkp->wk_flags == V_MUSTHAVE) {
			cmn_err(CE_PANIC, "map_wellknown_devices: required "
				"device %s not mapped\n", wkp->wk_namep);
		}
	}

	/*
	 * all sun4u systems must have an IO bus, i.e. sbus or pcibus
	 */
	if (niobus == 0)
	    cmn_err(CE_PANIC, "map_wellknown_devices: no i/o bus node found");

#ifndef	MPSAS
	/*
	 * all sun4u systems must have an eeprom
	 */
	if (v_eeprom_addr == (caddr_t)0)
	    cmn_err(CE_PANIC, "map_wellknown_devices: no eeprom node found");
#endif

	check_cpus();
}

/*
 * map_wellknown - map known devices & registers
 */
static void
map_wellknown(dnode_t curnode)
{
	extern int status_okay(int, char *, int);
	char tmp_name[MAXSYSNAME];
	static void fill_address(dnode_t, char *);

#ifdef	VPRINTF
	VPRINTF("map_wellknown(%x)\n", curnode);
#endif	VPRINTF

	for (curnode = CHILD(curnode); curnode; curnode = NEXT(curnode)) {
		/*
		 * prune subtree if status property indicating not okay
		 */
		if (!status_okay((int)curnode, (char *)NULL, 0)) {
			char devtype_buf[OBP_MAXPROPNAME];
			int size;

#ifdef	VPRINTF
			VPRINTF("map_wellknown: !okay status property\n");
#endif	VPRINTF
			/*
			 * a status property indicating bad memory will be
			 * associated with a node which has a "device_type"
			 * property with a value of "memory-controller"
			 */
			if ((size = GETPROPLEN(curnode, OBP_DEVICETYPE))
			    == -1) {
				continue;
			}
			if (size > OBP_MAXPROPNAME) {
				cmn_err(CE_CONT, "node %x '%s' prop too "
					"big\n", curnode, OBP_DEVICETYPE);
				continue;
			}
			if (GETPROP(curnode, OBP_DEVICETYPE, devtype_buf)
			    == -1) {
				cmn_err(CE_CONT, "node %x '%s' get failed\n",
					curnode, OBP_DEVICETYPE);
				continue;
			}
			if (strcmp(devtype_buf, "memory-controller") != 0)
				continue;
			/*
			 * ...else fall thru and process the node...
			 */
		}
		CLRBUF(tmp_name);
		if (GETPROP(curnode, OBP_NAME, (caddr_t)tmp_name) != -1)
			fill_address(curnode, tmp_name);
		if (GETPROP(curnode, OBP_DEVICETYPE, tmp_name) != -1 &&
		    strcmp(tmp_name, "cpu") == 0)
			fill_cpu(curnode);
		map_wellknown(curnode);
	}
}

static void
fill_address(dnode_t curnode, char *namep)
{
	struct wkdevice *wkp;
	int size;

	for (wkp = wkdevice; wkp->wk_namep; ++wkp) {
		if (strcmp(wkp->wk_namep, namep) != 0)
			continue;
		if (wkp->wk_flags == V_MAPPED)
			return;
		if (wkp->wk_vaddrp != NULL) {
			if ((size = GETPROPLEN(curnode, OBP_ADDRESS)) == -1) {
				cmn_err(CE_CONT, "device %s size %d\n",
					namep, size);
				continue;
			}
			if (size > sizeof (caddr_t)) {
				cmn_err(CE_CONT, "device %s address prop too "
					"big\n", namep);
				continue;
			}
			if (GETPROP(curnode, OBP_ADDRESS,
				    (caddr_t)wkp->wk_vaddrp) == -1) {
				cmn_err(CE_CONT, "device %s not mapped\n",
					namep);
				continue;
			}
#ifdef	VPRINTF
			VPRINTF("fill_address: %s mapped to %x\n", namep,
				*wkp->wk_vaddrp);
#endif	/* VPRINTF */
		}
		if (wkp->wk_func != NULL)
			(*wkp->wk_func)(curnode);
		/*
		 * If this one is optional and there may be more than
		 * one, don't set V_MAPPED, which would cause us to skip it
		 * next time around
		 */
		if (wkp->wk_flags != V_MULTI)
			wkp->wk_flags = V_MAPPED;
	}
}

static void
fill_cpu(dnode_t node)
{
	void fiximp_obp();
	struct cpu_node *cpunode;
	u_int cpu_clock_Mhz;
	int upaid;
	char *cpu_info;
	extern int ecache_size;

	/*
	 * use upa port id as the index to cpunodes[]
	 */
	(void) GETPROP(node, "upa-portid", (caddr_t)&upaid);
	cpunode = &cpunodes[upaid];
	cpunode->upaid = upaid;
	(void) GETPROP(node, "name", cpunode->name);
	(void) GETPROP(node, "implementation#",
		(caddr_t)&cpunode->implementation);
	(void) GETPROP(node, "mask#", (caddr_t)&cpunode->version);
	(void) GETPROP(node, "clock-frequency", (caddr_t)&cpunode->clock_freq);

	/*
	 * If we didn't find it in the CPU node, look in the root node.
	 */
	if (cpunode->clock_freq == 0) {
		dnode_t root = prom_nextnode((dnode_t)0);
		(void) GETPROP(root, "clock-frequency",
		    (caddr_t)&cpunode->clock_freq);
	}

	cpu_clock_Mhz = (cpunode->clock_freq + 500000) / 1000000;

	cpunode->nodeid = node;

	cpu_info = &cpu_info_buf[upaid][0];
	sprintf(cpu_info,
	    "cpu%d: %s (upaid %d impl 0x%x ver 0x%x clock %d MHz)\n",
	    ncpunode, cpunode->name, cpunode->upaid, cpunode->implementation,
	    cpunode->version, cpu_clock_Mhz);
	cmn_err(CE_CONT,
	    "?cpu%d: %s (upaid %d impl 0x%x ver 0x%x clock %d MHz)\n",
	    ncpunode, cpunode->name, cpunode->upaid, cpunode->implementation,
	    cpunode->version, cpu_clock_Mhz);
	if (ncpunode == 0) {
		(void) fiximp_obp(node);
		cpunode->ecache_size = ecache_size;
	} else {
		int size = 0;
		/*
		 * set ecache size to the largest Ecache of all the cpus
		 * in the system. setting it to largest E$ is desirable
		 * for page coloring.
		 */
		(void) GETPROP(node, "ecache-size", (caddr_t)&size);
		cpunode->ecache_size = size;
		if (size > ecache_size)
			ecache_size = size;
	}
	ncpunode++;
}

#ifdef SF_ERRATA_30 /* call causes fp-disabled */
int spitfire_call_bug = 0;
#endif

static void
check_cpus(void)
{
	int i;
	int impl, cpuid = getprocessorid();
	char *msg = NULL;
	extern use_mp;
	int min_supported_rev;

	ASSERT(cpunodes[cpuid].nodeid != 0);

	/*
	 * We check here for illegal cpu combinations.
	 * Currently, we check that the implementations are the same.
	 */
	impl = cpunodes[cpuid].implementation;
	switch (impl) {
		default:
			min_supported_rev = 0;
			break;
		case SPITFIRE_IMPL:
			min_supported_rev = SPITFIRE_MINREV_SUPPORTED;
			break;
	}

	for (i = 0; i < NCPU; i++) {
		if (cpunodes[i].nodeid == 0)
			continue;

		if (IS_SPITFIRE(impl) &&
		    cpunodes[i].version < min_supported_rev) {
			cmn_err(CE_PANIC, "UltraSPARC versions older than "
			    "%d.%d are no longer supported (cpu #%d)",
			    SPITFIRE_MAJOR_VERSION(min_supported_rev),
			    SPITFIRE_MINOR_VERSION(min_supported_rev), i);
		}

		/*
		 * Min supported rev is 2.1 but we've seen problems
		 * with that so we still want to warn if we see one.
		 */
		if (IS_SPITFIRE(impl) && cpunodes[i].version < 0x22) {
			cmn_err(CE_WARN, "UltraSPARC versions older than "
			    "2.2 are not supported (cpu #%d)", i);
		}

#ifdef SF_ERRATA_30 /* call causes fp-disabled */
		if (IS_SPITFIRE(impl) && cpunodes[i].version < 0x22)
			spitfire_call_bug = 1;
#endif /* SF_ERRATA_30 */

		if (cpunodes[i].implementation != impl) {
			msg = " on mismatched modules";
			break;
		}
	}
	if (msg != NULL) {
		cmn_err(CE_NOTE, "MP not supported%s, booting UP only\n", msg);
		for (i = 0; i < NCPU; i++) {
			if (cpunodes[i].nodeid == 0)
				continue;
			cmn_err(CE_NOTE, "cpu%d: %s version 0x%x\n",
				    cpunodes[i].upaid,
				    cpunodes[i].name, cpunodes[i].version);
		}
		use_mp = 0;
	}
	/*
	 * Set max cpus we can have based on ncpunode and use_mp
	 * (revisited when dynamic attach of cpus becomes possible).
	 */
	if (use_mp)
		max_ncpus = ncpunode;
	else
		max_ncpus = 1;
}

/*
 * The first sysio must always programmed up for the system clock and error
 * handling purposes, referenced by v_sysio_addr in machdep.c.
 */
static void
have_sbus(dnode_t node)
{
	int size;
	u_int portid;

	size = GETPROPLEN(node, "upa-portid");
	if (size == -1 || size > sizeof (portid))
		cmn_err(CE_PANIC, "upa-portid size");
	if (GETPROP(node, "upa-portid", (caddr_t)&portid) == -1)
		cmn_err(CE_PANIC, "upa-portid");

	niobus++;

	/*
	 * mark each entry that needs a physical TSB
	 */
	iommu_nodes[portid] = node;
}


/*
 * The first psycho must always programmed up for the system clock and error
 * handling purposes.
 */
static void
have_pci(dnode_t node)
{
	int size;
	u_int portid;

	size = GETPROPLEN(node, "upa-portid");
	if (size == -1)
		return;
	if (size > sizeof (portid))
		cmn_err(CE_PANIC, "upa-portid size");
	if (GETPROP(node, "upa-portid", (caddr_t)&portid) == -1)
		cmn_err(CE_PANIC, "upa-portid");

	niobus++;

	/*
	 * mark each entry that needs a physical TSB
	 */
	iommu_nodes[portid] = node;
}

/*
 * The first eeprom is used as the TOD clock, referenced
 * by v_eeprom_addr in locore.s.
 */
static void
have_eeprom(dnode_t node)
{
	int size;

	/*
	 * We will quit checking eeprom devices when we find the
	 * interesting one -- either chosen or the 1st one we encounter.
	 */
	if (v_eeprom_addr)
		return;

	/*
	 * If we have a chosen eeprom and it is not this node, keep looking.
	 */
	if (chosen_eeprom != NULL && chosen_eeprom != node)
		return;

	/*
	 * multiple eeproms may exist but at least
	 * one must an "address" property
	 */
	if ((size = GETPROPLEN(node, OBP_ADDRESS)) == -1)
		return;
	if (size > sizeof (v_eeprom_addr))
		cmn_err(CE_PANIC, "eeprom addr size");
	if (GETPROP(node, OBP_ADDRESS, (caddr_t)&v_eeprom_addr) == -1)
		cmn_err(CE_PANIC, "eeprom address");

	/*
	 * Does this eeprom have watchdog support?
	 */
	if (GETPROPLEN(node, WATCHDOG_ENABLE) != -1)
		watchdog_available = 1;
}

static void
have_auxio(dnode_t node)
{
	int size, n;
	caddr_t addr[5];

	/*
	 * Get the size of the auzio's address property.
	 * On some platforms, the address property contains one
	 * entry and on others it contains five entries.
	 * In all cases, the first entries are compatible.
	 *
	 * This routine gets the address property for the auxio
	 * node and stores the first entry in v_auxio_addr which
	 * is used by the routine set_auxioreg in sun4u/ml/locore.s.
	 */
	size = GETPROPLEN(node, OBP_ADDRESS);

	if (size == -1) {
		cmn_err(CE_PANIC, "can't get address property "
			"length for auxio node\n");
		return;
	}

	switch (n = (size / sizeof (caddr_t))) {
	case 1:
		break;
	case 5:
		break;
	default:
		cmn_err(CE_PANIC, "auxio address property has %d "
			"entries\n", n);
	}

	if (GETPROP(node, OBP_ADDRESS, (caddr_t)addr) == -1) {
		cmn_err(CE_PANIC, "can't get address property "
			"for auxio node\n");
		return;
	}

	v_auxio_addr = addr[0];
}


/*
 * Table listing the minimum prom versions supported by this kernel.
 */
static struct obp_rev_table {
	char *model;
	char *version;
	int level;
} obp_min_revs[] = {
	{ /* neutron */
	"SUNW,525-1410", "OBP 3.0.4 1995/11/26 17:47", CE_WARN },
	{ /* neutron+ */
	"SUNW,525-1448", "OBP 3.0.2 1995/11/26 17:52", CE_WARN },
	{ /* electron */
	"SUNW,525-1411", "OBP 3.0.4 1995/11/26 17:57", CE_WARN },
	{ /* pulsar */
	"SUNW,525-1414", "OBP 3.1.0 1996/03/05 09:00", CE_WARN },
	{ /* sunfire */
	"SUNW,525-1431", "OBP 3.1.0 1996/02/12 18:57", CE_WARN },
	{ NULL, NULL, 0 }
};

#define	NMINS	60
#define	NHOURS	24
#define	NDAYS	31
#define	NMONTHS	12

#define	YEAR(y)	 ((y-1) * (NMONTHS * NDAYS * NHOURS * NMINS))
#define	MONTH(m) ((m-1) * (NDAYS * NHOURS * NMINS))
#define	DAY(d)   ((d-1) * (NHOURS * NMINS))
#define	HOUR(h)  ((h)   * (NMINS))
#define	MINUTE(m) (m)

/*
 * XXX - Having this here isn't cool.  There's another copy
 * in the rpc code.
 */
static int
strtoi(const char *str, const char **pos)
{
	int c;
	int val = 0;

	for (c = *str++; c >= '0' && c <= '9'; c = *str++) {
		val *= 10;
		val += c - '0';
	}
	if (pos)
		*pos = str;
	return (val);
}


/*
 * obp_timestamp: based on the OBP flashprom version string of the
 * format "OBP x.y.z YYYY/MM/DD HH:MM" calculate a timestamp based
 * on the year, month, day, hour and minute by turning that into
 * a number of minutes.
 */
static int
obp_timestamp(const char *v)
{
	const char *c;
	int maj, year, month, day, hour, min;

	if (v[0] != 'O' || v[1] != 'B' || v[2] != 'P')
		return (-1);

	c = v + 3;

	/* Find first non-space character after OBP */
	while (*c != '\0' && (*c == ' ' || *c == '\t'))
		c++;
	if (strlen(c) < 5)		/* need at least "x.y.z" */
		return (-1);

	maj = strtoi(c, &c);
	if (maj < 3)
		return (-1);

#if 0 /* XXX - not used */
	dot = dotdot = 0;
	if (*c == '.') {
		dot = strtoi(c + 1, &c);

		/* optional? dot-dot release */
		if (*c == '.')
			dotdot = strtoi(c + 1, &c);
	}
#endif

	/* Find space at the end of version number */
	while (*c != '\0' && *c != ' ')
		c++;
	if (strlen(c) < 11)		/* need at least " xxxx/xx/xx" */
		return (-1);

	/* Point to first character of date */
	c++;

	/* Validate date format */
	if (c[4] != '/' || c[7] != '/')
		return (-1);

	year = strtoi(c, NULL);
	month = strtoi(c + 5, NULL);
	day = strtoi(c + 8, NULL);

	if (year < 1995 || month == 0 || day == 0)
		return (-1);

	/*
	 * Find space at the end of date number
	 */
	c += 10;
	while (*c != '\0' && *c != ' ')
		c++;
	if (strlen(c) < 6)		/* need at least " xx:xx" */
		return (-1);

	/* Point to first character of time */
	c++;

	if (c[2] != ':')
		return (-1);

	hour = strtoi(c, NULL);
	min = strtoi(c + 3, NULL);

	return (YEAR(year) + MONTH(month) +
	    DAY(day) + HOUR(hour) + MINUTE(min));
}


/*
 * Check the prom against the obp_min_revs table and complain if
 * the system has an older prom installed.  The actual major/minor/
 * dotdot numbers are not checked, only the date/time stamp.
 */
static void
have_flashprom(dnode_t node)
{
	int tstamp, min_tstamp;
	char vers[512], model[64];
	int plen;
	struct obp_rev_table *ortp;

	plen = GETPROPLEN(node, "model");
	if (plen <= 0 || plen > sizeof (model)) {
		cmn_err(CE_WARN, "have_flashprom: invalid model "
		    "property, not checking prom version");
		return;
	}
	if (GETPROP(node, "model", model) == -1) {
		cmn_err(CE_WARN, "have_flashprom: error getting model "
		    "property, not checking prom version");
		return;
	}
	model[plen] = '\0';

	plen = GETPROPLEN(node, "version");
	if (plen == -1) {
		/* no version property, ignore */
		return;
	}
	if (plen == 0 || plen > sizeof (vers)) {
		cmn_err(CE_WARN, "have_flashprom: invalid version "
		    "property, not checking prom version");
		return;
	}
	if (GETPROP(node, "version", vers) == -1) {
		cmn_err(CE_WARN, "have_flashprom: error getting version "
		    "property, not checking prom version");
		return;
	}
	vers[plen] = '\0';

	/* Make sure it's an OBP flashprom */
	if (vers[0] != 'O' && vers[1] != 'B' && vers[2] != 'P')
		return;

#ifdef VPRINTF
	VPRINTF("fillsysinfo: Found OBP flashprom: %s\n", vers);
#endif

	tstamp = obp_timestamp(vers);
	if (tstamp == -1) {
		cmn_err(CE_WARN, "have_flashprom: node contains "
		    "improperly formatted version property,\n"
		    "\tnot checking prom version");
		return;
	}

	for (ortp = obp_min_revs; ortp->model != NULL; ortp++) {
		if (strcmp(model, ortp->model) == 0) {
			min_tstamp = obp_timestamp(ortp->version);
			if (min_tstamp == -1) {
				cmn_err(CE_WARN, "have_flashprom: "
				    "invalid OBP version string in table "
				    " (entry %d)", ortp - obp_min_revs);
				continue;
			}
			if (tstamp < min_tstamp) {
				cmn_err(ortp->level, "Down-rev OBP detected.  "
				    "Please update to at least:\n\t%s",
				    ortp->version);
				break;
			}
		}
	} /* for each obp_rev_table entry */
}
