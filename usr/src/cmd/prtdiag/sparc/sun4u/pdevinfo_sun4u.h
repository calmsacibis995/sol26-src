/*
 * Copyright (c) 1994 Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_PDEVINFO_SUN4U_H
#define	_PDEVINFO_SUN4U_H

#pragma ident	"@(#)pdevinfo_sun4u.h	1.15	96/01/30 SMI"

#include <sys/fhc.h>
#include <sys/sysctrl.h>
#include <sys/environ.h>
#include <sys/simmstat.h>
#include "reset_info.h"

#ifdef	__cplusplus
extern "C" {
#endif

extern int desktop;

#define	UNIX	"unix"

/* Define names of nodes to search for */
#define	CPU_NAME	"SUNW,UltraSPARC"
#define	SBUS_NAME	"sbus"
#define	PCI_NAME	"pci"
#define	FFB_NAME	"SUNW,ffb"
#define	AFB_NAME	"SUNW,afb"

struct bd_kstat_data {
	u_longlong_t 	ac_memctl;	/* Memctl register contents */
	u_longlong_t 	ac_memdecode[2]; /* memory decode registers . */
	int	ac_kstats_ok;	/* successful kstat read occurred */
	u_int	fhc_bsr;	/* FHC Board Status Register */
	u_int	fhc_csr;	/* FHC Control Status Register */
	int	fhc_kstats_ok;	/* successful kstat read occurred */
	u_char	simm_status[SIMM_COUNT];	/* SIMM status */
	int	simmstat_kstats_ok;	/* successful read occurred */
	struct temp_stats tempstat;
	int	temp_kstat_ok;
};

/*
 * Hot plug info structure. If a hotplug kstat is found, the bd_info
 * structure from the kstat is filled in the the hp_info structure
 * is marked OK.
 */
struct hp_info {
	struct bd_info bd_info;
	int kstat_ok;
};

struct system_kstat_data {
	u_char	sysctrl;	/* sysctrl register contents */
	u_char	sysstat1;	/* system status1 register contents. */
	u_char	sysstat2;	/* system status2 register contents. */
	u_char 	ps_shadow[SYS_PS_COUNT];	/* power supply shadow */
	int	psstat_kstat_ok;
	u_char	clk_freq2;	/* clock frequency register 2 contents */
	u_char	fan_status;	/* shadow fan status */
	u_char	keysw_status;	/* status of the key switch */
	enum power_state power_state;	/* redundant power state */
	int	sys_kstats_ok;	/* successful kstat read occurred */
	struct temp_stats tempstat;
	int	temp_kstat_ok;
	struct reset_info reset_info;
	int	reset_kstats_ok;	/* kstat read OK */
	struct bd_kstat_data bd_ksp_list[MAX_BOARDS];
	struct hp_info hp_info[MAX_BOARDS];
	struct ft_list *ft_array;	/* fault array */
	int	nfaults;		/* number of faults in fault array */
	int	ft_kstat_ok;		/* Fault kstats OK */
};

/* Description of a single memory group */
struct grp {
	int valid;			/* active memory group present */
	u_longlong_t  base;		/* Phyiscal base of group */
	u_int size;			/* size in bytes */
	int board;			/* board number */
	enum board_type type;		/* board type */
	int group;			/* group # on board (0 or 1) */
	int factor;			/* interleave factor (0,2,4,8,16) */
	char groupid;			/* Alpha tag for group ID */
};

#define	MAX_GROUPS	32

/* Array of all possible groups in the system. */
struct grp_info {
	struct grp grp[MAX_GROUPS];
};

/* A memory interleave structure */
struct inter_grp {
	u_longlong_t base;	/* Physical base of group */
	int valid;
	int count;
	char groupid;
};

/* Array of all possible memory interleave structures */
struct mem_inter {
	struct inter_grp i_grp[MAX_GROUPS];
};

/* FFB info structure */
struct ffbinfo {
	int board;
	int upa_id;
	char *dev;
	struct ffbinfo *next;
};

/* FFB strap reg union */
union strap_un {
	struct {
		u_int	unused:24;
		u_int	afb_flag:1;
		u_int	major_rev:2;
		u_int	board_rev:2;
		u_int	board_mem:1;
		u_int	cbuf:1;
		u_int	bbuf:1;
	} fld;
	u_int ffb_strap_bits;
};

/* known values for manufacturer's JED code */
#define	MANF_BROOKTREE	214
#define	MANF_MITSUBISHI	28

/* FFB mnufacturer union */
union manuf {
	struct {
		u_int version:4;	/* version of part number */
		u_int partno:16;	/* part number */
		u_int manf:11;		/* manufacturer's JED code */
		u_int one:1;		/* always set to '1' */
	} fld;
	u_int encoded_id;
};

#define	FFBIOC		('F' << 8)
#define	FFB_SYS_INFO	(FFBIOC| 80)

struct ffb_sys_info {
	unsigned int	ffb_strap_bits;	/* ffb_strapping register	*/
#define	FFB_B_BUFF	0x01		/* B buffer present		*/
#define	FFB_C_BUFF	0x02		/* C buffer present		*/
#define	FB_TYPE_AFB	0x80		/* AFB or FFB			*/
	unsigned int	fbc_version;	/* revision of FBC chip		*/
	unsigned int	dac_version;	/* revision of DAC chip		*/
	unsigned int	fbram_version;	/* revision of FBRAMs chip	*/
	unsigned int	flags;		/* miscellaneous flags		*/
#define	FFB_KSIM	0x00000001	/* kernel simulator		*/
#define	FFB_PAGE_FILL_BUG 0x00000002	/* FBRAM has page fill bug	*/
	unsigned int	afb_nfloats;	/* no. of Float asics in AFB	*/
	unsigned int	pad[58];	/* padding for AFB chips & misc. */
};

void read_sun4u_kstats(Sys_tree *, struct system_kstat_data *);
Prom_node *find_upa_device(Board_node *, int, char *);
int get_upa_id(Prom_node *);

#ifdef	__cplusplus
}
#endif

#endif	/* _PDEVINFO_SUN4U_H */
