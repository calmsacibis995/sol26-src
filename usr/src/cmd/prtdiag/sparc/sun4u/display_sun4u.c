/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)display_sun4u.c 1.26	96/08/13 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <kvm.h>
#include <varargs.h>
#include <time.h>
#include <sys/fcntl.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/openpromio.h>
#include <libintl.h>
#include <syslog.h>
#include <sys/ac.h>
#include <sys/fhc.h>
#include "pdevinfo.h"
#include "display.h"
#include "anlyzerr.h"
#include "pdevinfo_sun4u.h"

/* uppper level system display functions */
static void display_cpu_devices(Sys_tree *);
static void display_cpus(Board_node *);
static void display_mem(struct grp_info *);
static void display_io_devices(Sys_tree *tree);
static void display_io_cards(Board_node *board);
static void display_hp_boards(struct system_kstat_data *);
static void display_sbus(Prom_node *);
static void display_pci(Prom_node *);
static void display_dt_pci(Prom_node *);
static void display_ffb(struct ffbinfo *, int);
static int disp_err_log(struct system_kstat_data *);
static int disp_fault_list(struct system_kstat_data *);
static int disp_env_status(struct system_kstat_data *);
static int disp_keysw_and_leds(struct system_kstat_data *);
static void disp_asic_revs(Sys_tree *);
static void dt_disp_asic_revs(Sys_tree *);
static void sf_disp_prom_versions(Sys_tree *);
static void dt_disp_prom_version(Sys_tree *);
static void disp_prom_version(Prom_node *);
static void get_mem_total(struct mem_total *, struct grp_info *);
static void build_mem_tables(Sys_tree *,
	struct system_kstat_data *,
	struct grp_info *);
static int get_cpu_freq(Prom_node *);
static int get_cache_size(Prom_node *);
static Prom_node *find_card(Prom_node *, int);
static Prom_node *next_card(Prom_node *, int);
static int get_sbus_slot(Prom_node *);
static int get_pci_device(Prom_node *);
static void erase_msgs(char **);
static void display_msgs(char **msgs, int board);
static int disp_fail_parts(Sys_tree *);
static Prom_node *find_pci_bus(Prom_node *, int, int);
static int get_pci_bus(Prom_node *);
static char *fmt_manf_id(unsigned int, char *);

extern struct ffbinfo *ffb_list;

/* Maximum number of Sbus slot numbers on fusion systems */
#define	SUN4U_SBUS_SLOTS	16

int
display(Sys_tree *tree,
	Prom_node *root,
	struct system_kstat_data *kstats,
	int syserrlog)
{
	int exit_code = 0;	/* init to all OK */
	void *value;		/* used for opaque PROM data */
	struct mem_total memory_total;	/* Total memory in system */
	struct grp_info grps;	/* Info on all groups in system */

	/*
	 * silently check for any types of machine errors
	 */
	print_flag = 0;
	if (disp_fail_parts(tree) || disp_fault_list(kstats) ||
	    disp_err_log(kstats) || disp_env_status(kstats)) {
		/* set exit_code to show failures */
		exit_code = 1;
	}
	print_flag = 1;

	/*
	 * Now display the machine's configuration. We do this if we
	 * are not logging or exit_code is set (machine is broke).
	 */
	if (!logging || exit_code) {
		struct utsname uts_buf;

		/*
		 * Display system banner
		 */
		(void) uname(&uts_buf);
		log_printf(
			gettext("System Configuration:  Sun Microsystems"
			"  %s %s\n"), uts_buf.machine,
			get_prop_val(find_prop(root, "banner-name")), 0);

		/* display system clock frequency */
		value = get_prop_val(find_prop(root, "clock-frequency"));
		if (value != NULL) {
			log_printf(gettext("System clock frequency: "
				"%d MHz\n"),
				((*((int *)value)) + 500000) / 1000000, 0);
		}

		if (desktop != 0) {
			long pagesize = sysconf(_SC_PAGESIZE);
			long npages = sysconf(_SC_PHYS_PAGES);
			log_printf("Memory size: ", 0);
			if (pagesize == -1 || npages == -1)
				log_printf("unable to determine\n", 0);
			else {
				long long ii;
				int kbyte = 1024;
				int mbyte = 1024 * 1024;

				ii = (long long) pagesize * npages;
				if (ii >= mbyte)
					log_printf("%d Megabytes\n",
						(int) ((ii+mbyte-1) / mbyte),
						0);
				else
					log_printf("%d Kilobytes\n",
						(int) ((ii+kbyte-1) / kbyte),
						0);
			}
		} else {
			/* Build the memory group tables and interleave data */
			build_mem_tables(tree, kstats, &grps);

			/* display total usable installed memory */
			get_mem_total(&memory_total, &grps);
			(void) log_printf(gettext("Memory size: %4dMb\n"),
				memory_total.dram, 0);

			/* We display the NVSIMM size totals separately. */
			if (memory_total.nvsimm != 0) {
				(void) log_printf(
					gettext("NVSIMM size: %4dMb\n"),
					memory_total.nvsimm);
			}
		}

		/* Display the CPU devices */
		display_cpu_devices(tree);

		/* Display the Memory configuration */
		if (desktop == 0) {
			display_mem(&grps);
		}

		/* Display the IO cards on all the boards. */
		(void) display_io_devices(tree);

		/* Display Hot plugged, disabled and failed boards */
		(void) display_hp_boards(kstats);

		/* Display failed units */
		(void) disp_fail_parts(tree);

		/* Display fault info */
		(void) disp_fault_list(kstats);
	}

	/*
	 * Now display the last powerfail time and the fatal hardware
	 * reset information. We do this under a couple of conditions.
	 * First if the user asks for it. The second is iof the user
	 * told us to do logging, and we found a system failure.
	 */
	if (syserrlog || (logging && exit_code)) {
		/*
		 * display time of latest powerfail. Not all systems
		 * have this capability. For those that do not, this
		 * is just a no-op.
		 */
		disp_powerfail(root);

		/* Display system environmental conditions. */
		if (desktop == 0) {
			(void) disp_env_status(kstats);

			/* Display ASIC Chip revs for all boards. */
			disp_asic_revs(tree);

			/* Print the PROM revisions here */
			sf_disp_prom_versions(tree);

			/*
			 * Display the latest system fatal hardware error data,
			 * if any. The system holds this data in SRAM, so it
			 * does not persist across power-on resets.
			 */
			(void) disp_err_log(kstats);
		} else {
			dt_disp_asic_revs(tree);

			dt_disp_prom_version(tree);
		}
	}

	return (exit_code);
}

/*
 * display_cpu_devices
 *
 * This routine is the generic link into displaying CPU and memory info.
 * It displays the table header, then calls the CPU and memory display
 * routine for all boards.
 */
static void
display_cpu_devices(Sys_tree *tree)
{
	Board_node *bnode;

	/*
	 * Display the table header for CPUs . Then display the CPU
	 * frequency, cache size, and processor revision  on all the boards.
	 */
	log_printf(
		gettext("       CPU Units: Frequency Cache-Size Version\n"),
		0);
	log_printf("            ", 0);
	log_printf("A: MHz  MB  Impl. Mask  B: MHz  MB  Impl. Mask\n", 0);
	log_printf("            ", 0);
	log_printf("----------  ----- ----  ----------  ----- ----\n", 0);

	/* Now display all of the boards */
	bnode = tree->bd_list;
	while (bnode != NULL) {
		display_cpus(bnode);
		bnode = bnode->next;
	}
}

/*
 * display_cpus
 *
 * Display the CPUs present on this board.
 */
static void
display_cpus(Board_node *board)
{
	int upa_id;	/* UPA Bus Port ID of CPU */
	char display_str[256];

	/* print the board number first */

	/*
	 * TODO - omit the word 'Board' if this is a single board
	 * system. i.e., a desktop system.
	 */
	/*
	 * TRANSLATION_NOTE
	 *	Following string is used as a table header.
	 *	Please maintain the current alignment in
	 *	translation.
	 */
	if (desktop == 0) {
		(void) sprintf(display_str, gettext("Board%2d:    "),
			board->board_num);
	} else {
		(void) sprintf(display_str, "            ");
	}

	/*
	 * display the CPUs' operating frequencies, cache size, impl.
	 * field and mask revision.
	 */
	for (upa_id = bd_to_upa(board->board_num);
	    upa_id < bd_to_upa(board->board_num + 1); upa_id += 1) {
		char temp_str[256];
		int freq;	/* CPU clock frequency */
		int cachesize;	/* MXCC cache size */
		int *impl;
		int *mask;
		Prom_node *cpu;

		cpu = find_upa_device(board, upa_id, CPU_NAME);
		freq = (get_cpu_freq(cpu) + 500000) / 1000000;
		cachesize = get_cache_size(cpu);
		impl = (int *) get_prop_val(find_prop(cpu, "implementation#"));
		mask = (int *) get_prop_val(find_prop(cpu, "mask#"));

		/* Do not display a failed CPU node */
		if ((freq != 0) && (node_failed(cpu) == 0)) {
			(void) sprintf(temp_str, "   %3d", freq);
			(void) strcat(display_str, temp_str);
			if (cachesize == 0) {
				(void) strcat(display_str, "    ");
			} else {
				(void) sprintf(temp_str, " %0.1f",
					(float)cachesize/ (float)(1024*1024));
				(void) strcat(display_str, temp_str);
			}

			if (impl != NULL) {
				(void) sprintf(temp_str, "   %2x", *impl);
				(void) strcat(display_str, temp_str);
			} else {
				(void) strcat(display_str, "     ");
			}

			if (mask != NULL) {
				(void) sprintf(temp_str, "    %d.%d  ",
					(*mask >> 4) & 0xF, *mask & 0xF);
				(void) strcat(display_str, temp_str);
			} else {
				(void) strcat(display_str, "         ");
			}

		} else {
			/* Print 24 spaces, the equivalent width of above */
			(void) strcat(display_str, "                        ");
		}

	}
	if ((board->board_type == CPU_BOARD) ||
	    (board->board_type == MEM_BOARD)) {
		log_printf("%s\n", display_str, 0);
	}
}

/*
 * display_mem
 *
 * This routine displays the memory configuration for all boards in the
 * system.
 */
static void
display_mem(struct grp_info *grps)
{
	int board;
	char indent_str[] = "          ";

	/* Print the header for the memory section. */
	log_printf(indent_str, 0);
	log_printf("Memory Units: Size, Interleave Factor, Interleave "
		"With\n", 0);
	log_printf(indent_str, 0);
	log_printf("0: MB  Factor:  With:  1: MB  Factor:  With:\n", 0);
	log_printf(indent_str, 0);
	log_printf("-----  -------  -----  -----  -------  -----\n", 0);

	/* Print the Memory groups information. */
	for (board = 0; board < MAX_BOARDS; board++) {
		struct grp *grp0, *grp1;

		grp0 = &grps->grp[board*2];
		grp1 = &grps->grp[board*2 + 1];

		/* If this board is not a CPU or MEM board, skip it. */
		if ((grp0->type != MEM_BOARD) && (grp0->type != CPU_BOARD)) {
			continue;
		}

		/* Print the board header */
		log_printf("Board%2d:  ", board, 0);

		if (grp0->valid) {
			log_printf("%4d   %2d-way", grp0->size,
				grp0->factor, 0);
			if (grp0->factor > 1) {
				log_printf("     %c     ", grp0->groupid, 0);
			} else {
				log_printf("           ", 0);
			}
		} else {
			log_printf("                        ", 0);
		}

		if (grp1->valid) {
			log_printf("%4d   %2d-way", grp1->size,
				grp1->factor, 0);
			if (grp1->factor > 1) {
				log_printf("    %c\n", grp1->groupid, 0);
			} else {
				log_printf("\n", 0);
			}
		} else {
			log_printf("\n", 0);
		}
	}
}

/*
 * display_io_devices
 *
 * This routine is the generic link into displaying system IO
 * configuration. It displays the table header, then displays
 * all the SBus cards, then displays all fo the PCI IO cards.
 */
static void
display_io_devices(Sys_tree *tree)
{
	Board_node *bnode;

	/*
	 * TRANSLATION_NOTE
	 *	Following string is used as a table header.
	 *	Please maintain the current alignment in
	 *	translation.
	 */
	log_printf(gettext("======================IO Cards"), 0);
	log_printf("=========================================\n", 0);
	bnode = tree->bd_list;
	while (bnode != NULL) {
		display_io_cards(bnode);
		bnode = bnode->next;
	}
}

/*
 * display_io_cards
 *
 * Display the SBus cards present on this board. This will also display
 * onboard parts present on the SBus.
 * TODO - Find out if we really want to display all of the onboard
 * components on the desktop systems.
 */
static void
display_io_cards(Board_node *board)
{
	Prom_node *pnode;
	int upa_id;
	struct ffbinfo *fptr;

	/*
	 * display the SBus cards present on this board. Sun5f systems
	 * can have up to two SBus's per board.
	 */
	if (!desktop) {
		for (upa_id = bd_to_upa(board->board_num); upa_id <
		    bd_to_upa(board->board_num + 1); upa_id++) {
			pnode = find_upa_device(board, upa_id, SBUS_NAME);

			if ((pnode != NULL) && (node_failed(pnode) == 0)) {
				log_printf("Board%2d, SBus%d:\n",
					board->board_num, upa_id & 0x1, 0);

				display_sbus(pnode);
			}
			pnode = find_upa_device(board, upa_id, FFB_NAME);
			if ((pnode != NULL) && (node_failed(pnode) == 0)) {
				log_printf("Board%2d, FFB", board->board_num,
					0);

				/* find the specific FFB in question */
				for (fptr = ffb_list; fptr != NULL;
				    fptr = fptr->next) {
					if (fptr->board == board->board_num) {
						display_ffb(fptr, 0);
					}
				}
			}

			pnode = find_upa_device(board, upa_id, PCI_NAME);

			if ((pnode != NULL) && (node_failed(pnode) == 0)) {
				log_printf("Board%2d, PCI Bus%d\n",
					board->board_num, upa_id & 0x1, 0);
				display_pci(pnode);
			}
		}
	} else {
		pnode = find_upa_device(board, 0x1F, SBUS_NAME);

		if (pnode == NULL) {
			pnode = dev_find_node(board->nodes, PCI_NAME);
			if (pnode == NULL) {
				log_printf("dev_find_node() Could not find "
					"any IO bus\n", 0);
				exit(2);
			}

			display_dt_pci(board->nodes);
		} else {
			display_sbus(pnode);
		}

		if ((find_upa_device(board, 0x1E, FFB_NAME) != NULL) &&
		    (ffb_list != NULL)) {
			log_printf("               FFB:    ", 0);
			display_ffb(ffb_list, 0);
		}
	}
}

static void
display_hp_boards(struct system_kstat_data *kstats)
{
	int i;
	int j;
	int hp_found = 0;
	struct hp_info *hp;
	char *state;

	for (i = 0, hp = &kstats->hp_info[0]; i < MAX_BOARDS; i++, hp++) {
		if (!hp->kstat_ok) {
			continue;
		}

		hp_found = 1;
	}

	/* return if there are no hotplug boards in the system. */
	if (!hp_found) {
		return;
	}

	if (hp_found != 0) {
		log_printf("\n", 0);
		log_printf("Detached Boards\n", 0);
		log_printf("===============\n", 0);
		log_printf("  Slot  State       Type      Info\n", 0);
		log_printf("  ----  ---------   ------    ----"
			"-------------------------------------\n", 0);
	}

	/* Display all detached boards */
	for (i = 0, hp = &kstats->hp_info[0]; i < MAX_BOARDS; i++, hp++) {
		struct cpu_info *cpu;

		if (hp->kstat_ok == 0) {
			continue;
		}


		switch (hp->bd_info.state) {
		case UNKNOWN_STATE:
			state = "unknown";
			break;

		case ACTIVE_STATE:
			state = "active";
			break;

		case LOWPOWER_STATE:
			state = "low-power";
			break;

		case HOTPLUG_STATE:
			state = "hot-plug";
			break;

		case DISABLED_STATE:
			state = "disabled";
			break;

		case FAILED_STATE:
			state = "failed";
			break;

		default:
			state = "unknown";
			break;
		}

		log_printf(gettext("   %2d   %9s   "), i, state, 0);

		switch (hp->bd_info.type) {
		case CPU_BOARD:
			log_printf("%-9s ", CPU_BD_NAME, 0);

			/* Cannot display CPU info for disabled boards */
			if ((hp->bd_info.state == DISABLED_STATE) ||
			    (hp->bd_info.state == FAILED_STATE)) {
				break;
			}

			/* Display both CPUs if present */
			cpu = &hp->bd_info.bd.cpu[0];
			for (j = 0; j < 2; j++, cpu++) {
				log_printf("CPU %d: ", j, 0);
				/* Print the rated speed of the CPU. */
				if (cpu->cpu_speed > 1) {
					log_printf("%3d MHz", cpu->cpu_speed,
						0);
				} else {
					log_printf("no CPU       ", 0);
					continue;
				}

				/* Display the size of the cache */
				if (cpu->cache_size != 0) {
					log_printf(" %0.1fM ",
						(float)cpu->cache_size /
						(float)(1024*1024), 0);
				} else {
					log_printf("    ", 0);
				}
			}
			break;

		case IO_2SBUS_BOARD:
			log_printf("%-9s ", IO_2SBUS_BD_NAME, 0);
			break;

		case IO_SBUS_FFB_BOARD:
			log_printf("%-9s ", IO_SBUS_FFB_BD_NAME, 0);
			switch (hp->bd_info.bd.io2.ffb_size) {
			case FFB_SINGLE:
				log_printf("Single buffered FFB", 0);
				break;

			case FFB_DOUBLE:
				log_printf("Double buffered FFB", 0);
				break;

			case FFB_NOT_FOUND:
				log_printf("No FFB installed", 0);
				break;

			default:
				log_printf("Illegal FFB size", 0);
				break;
			}
			break;

		case DISK_BOARD:
			log_printf("%-9s ", "disk", 0);
			for (j = 0; j < 2; j++) {
				log_printf("Disk %d:", j, 0);
				if (hp->bd_info.bd.dsk.disk_pres[j]) {
				    log_printf(" Target: %2d   ",
					hp->bd_info.bd.dsk.disk_id[j], 0);
				} else {
				    log_printf(" no disk      ", 0);
				}
			}
			break;

		case UNKNOWN_BOARD:
		case UNINIT_BOARD:
		default:
			log_printf("UNKNOWN ", 0);
			break;
		}
		log_printf("\n");
	}
}

/*
 * display_sbus
 * Display all the SBus IO cards on this bus.
 */
static void
display_sbus(Prom_node *pnode)
{
	int card;
	void *value;

	/* get sbus clock frequency */
	value = get_prop_val(find_prop(pnode, "clock-frequency"));

	/* display SBus clock frequency */
	log_printf(gettext("               SBus clock frequency: %d MHz\n"),
		(*((int *) value) + 500000) / 1000000, 0);

	for (card = 0; card < SUN4U_SBUS_SLOTS; card++) {
		Prom_node *card_node;
		int device = 0;		/* # of device on card */

		card_node = find_card(pnode, card);

		/* display nothing for no card or failed card */
		if ((card_node == NULL) ||
		    (find_prop(card_node, "status") != NULL)) {
			continue;
		}

		/*
		 * High slot numbers should not be displayed on desktops,
		 * but they should be displayed on servers.
		 */
		if ((card >= MX_SBUS_SLOTS) && (desktop != 0)) {
			continue;
		}

		/*
		 * On servers, the only high slot number that needs to
		 * be displayed is the # 13 slot.
		 */
		if ((card >= MX_SBUS_SLOTS) && (card != 13)) {
			continue;
		}

		/* now display all of the node names for that card */
		while (card_node != NULL) {
			char *model;
			char *name;
			char *child_name;
			char fmt_str[(OPROMMAXPARAM*3)+1];
			char tmp_str[OPROMMAXPARAM+1];

			model = (char *)get_prop_val(find_prop(card_node,
				"model"));
			name = get_node_name(card_node);

			if (name == NULL) {
				card_node = next_card(card_node, card);
				continue;
			}

			/*
			 * TRANSLATION_NOTE
			 * Following string is used as a table header.
			 * Please maintain the current alignment in
			 * translation.
			 */
			if (device == 0) {
				log_printf("               %d: ",
					card, 0);
				(void) sprintf(fmt_str, "%s", name);
			} else {
				log_printf("                  ", 0);
				(void) sprintf(fmt_str, "%s", name);
			}

			if ((card_node->child != NULL) &&
			    ((child_name = get_node_name(card_node->child)) !=
			    NULL)) {
				void *value;

				(void) sprintf(tmp_str, "/%s", child_name);
				(void) strcat(fmt_str, tmp_str);

				if ((value = get_prop_val(find_prop(
				    card_node->child,
				    "device_type"))) != NULL) {
					(void) sprintf(tmp_str, "(%s)",
						(char *)value);
					(void) strcat(fmt_str, tmp_str);
				}
			}

			log_printf("%-20s", fmt_str, 0);

			if (model != NULL) {
				log_printf("\t'%s'\n", model, 0);
			} else {
				log_printf("\n", 0);
			}
			card_node = next_card(card_node, card);
			device++;

		}
	}
}

/*
 * display_dt_pci
 *
 * Display all the PCI IO cards on this system. This function applies
 * specifically to the Positron systems.
 */
static void
display_dt_pci(Prom_node  *pnode)
{
	int bus;	/* PCI bus number from 'assigned-addresses' */
	int device;	/* PCI device number from 'assigned-addresses' */
	Prom_node *node;
	char *name;
	char *model;

	/* The plug in card slots on a Positron both exist on PCI bus B */

	/*
	 * XXX - Decide whether or not to display the on board devices
	 * on the Positron.
	 */
	for (bus = 0; bus < 2; bus++) {
		/* find PCI bus */
		pnode = find_pci_bus(pnode, 0x1F, bus);

		if (pnode != NULL) {
			node = pnode->child;
		} else {
			continue;
		}

		log_printf("PCI Bus %c\n", 'A'+bus);
		while (node != NULL) {
			model = (char *)get_prop_val(find_prop(node,
				"model"));
			name = get_node_name(node);

			device = get_pci_device(node);

			if (device == -1) {
				node = node->sibling;
				continue;
			}
			log_printf("    Slot %d:    ", device, 0);

			if (name != NULL) {
				log_printf("%s ", name, 0);
			}

			if (model != NULL) {
				log_printf("%s", model, 0);
			}

			log_printf("\n");

			node = node->sibling;
		}
	}
	log_printf("\n");
}

/*
 * display_pci
 *
 * Display all the PCI IO cards on this board.
 *
 * XXX - Until we have a definition for the board design for this Sunfire
 * board type, this routine cannot be completed.
 */

/* ARGSUSED */
static void
display_pci(Prom_node  *pnode)
{
}

static char	*prconf_board_msg = "    Board: %srev %d\n";
static char	*prconf_stdres_msg = "";
static char	*prconf_dblres_msg = "double memory ";
static char	*prconf_versionhex_msg = "    %s: version 0x%x\n";
static char	*prconf_versiondac_msg = "    DAC: %s\n";
static char	*prconf_versionfbram_msg = "    3DRAM: %s\n";
static char	*prconf_fmtmanf1d_msg = "%s %d, version %d";
static char	*prconf_fmtmanf1x_msg = "%s %x, version %d";
static char	*prconf_fmtmanf2_msg =
	"JED code %d, Part num 0x%x, version %d";

/* ARGSUSED */
static void
display_ffb(struct ffbinfo *fptr, int verbose)
{
	struct ffb_sys_info fsi;
	int fd = -1;
	char msgbuf[256];
	union strap_un strap;

	/* Make sure we do not get a NULL pointer */
	if (fptr == NULL) {
		return;
	}

	(void) sprintf(msgbuf, "/devices/%s", fptr->dev);

	/* open the FFB device */
	if ((fd = open(msgbuf, O_RDWR, 0666)) == -1) {
		/* error is probably driver not loaded, but cannot be sure */
		log_printf("\n", 0);
		return;
	}

	/* run the ioctl to get the FFB hardware revs */
	if (ioctl(fd, FFB_SYS_INFO, &fsi) < 0) {
		return;
	}

	if (verbose) {
		strap.ffb_strap_bits = fsi.ffb_strap_bits;

		/* Print board revision */
		log_printf(prconf_board_msg, strap.fld.board_mem ?
			prconf_dblres_msg : prconf_stdres_msg,
			(int) strap.fld.board_rev, 0);

		/* print FBC revision */
		log_printf(prconf_versionhex_msg, "FBC", fsi.fbc_version, 0);

		/* print DAC revision */
		log_printf(prconf_versiondac_msg,
			fmt_manf_id(fsi.dac_version, msgbuf), 0);

		/* print 3DRAM revision */
		log_printf(prconf_versionfbram_msg,
			fmt_manf_id(fsi.fbram_version, msgbuf), 0);
	} else {
		/* Print single or double buffered */
		if (fsi.ffb_strap_bits & FFB_B_BUFF) {
			log_printf("    Double Buffered\n", 0);
		} else {
			log_printf("    Single Buffered\n", 0);
		}
	}
}

static char *
fmt_manf_id(unsigned int encoded_id, char *outbuf)
{
	union manuf manuf;

	/*
	 * Format the manufacturer's info.  Note a small inconsistency we
	 * have to work around - Brooktree has it's part number in decimal,
	 * while Mitsubishi has it's part number in hex.
	 */
	manuf.encoded_id = encoded_id;
	switch (manuf.fld.manf) {
	case MANF_BROOKTREE:
		(void) sprintf(outbuf, prconf_fmtmanf1d_msg, "Brooktree",
			manuf.fld.partno, manuf.fld.version);
		break;

	case MANF_MITSUBISHI:
		(void) sprintf(outbuf, prconf_fmtmanf1x_msg, "Mitsubishi",
			manuf.fld.partno, manuf.fld.version);
		break;

	default:
		(void) sprintf(outbuf, prconf_fmtmanf2_msg, manuf.fld.manf,
			manuf.fld.partno, manuf.fld.version);
	}
	return (outbuf);
}

/*
 * disp_fail_parts
 *
 * Display the failed parts in the system. This function looks for
 * the status property in all PROM nodes. On systems where
 * the PROM does not supports passing diagnostic information
 * thruogh the device tree, this routine will be silent.
 */
static int
disp_fail_parts(Sys_tree *tree)
{
	int exit_code;
	int system_failed = 0;
	Board_node *bnode = tree->bd_list;
	Prom_node *pnode;

	exit_code = 0;

	/* go through all of the boards looking for failed units. */
	while (bnode != NULL) {
		/* find failed chips */
		pnode = find_failed_node(bnode->nodes);
		if ((pnode != NULL) && !system_failed) {
			system_failed = 1;
			exit_code = 1;
			if (print_flag == 0) {
				return (exit_code);
			}
			log_printf("\n", 0);
			log_printf(
	gettext("Failed Field Replaceable Units (FRU) in System:\n"), 0);
			log_printf("=========================="
				"====================\n", 0);
		}

		while (pnode != NULL) {
			void *value;
			char *name;		/* node name string */
			char *type;		/* node type string */
			char *board_type = NULL;

			value = get_prop_val(find_prop(pnode, "status"));
			name = get_node_name(pnode);

			/* sanity check of data retreived from PROM */
			if ((value == NULL) || (name == NULL)) {
				pnode = next_failed_node(pnode);
				continue;
			}

			/* Find the board type of this board */
			if (bnode->board_type == CPU_BOARD) {
				board_type = "CPU";
			} else {
				board_type = "IO";
			}

			log_printf(gettext("%s unavailable on %s Board #%d\n"),
				name, board_type,
				bnode->board_num, 0);

			log_printf(
				gettext("Failed Field Replaceable Unit is "),
				0);

			/*
			 * Determine whether FRU is CPU module, system
			 * board, or SBus card.
			 */
			if ((name != NULL) && (strstr(name, "sbus"))) {
				log_printf(gettext("SBus Card %d"),
					get_sbus_slot(pnode), 0);
			} else if (((name = get_node_name(pnode->parent)) !=
			    NULL) && (strstr(name, "pci"))) {
				log_printf(gettext("PCI Card %d"),
					get_pci_device(pnode), 0);
			} else if (((type = get_node_type(pnode)) != NULL) &&
			    (strstr(type, "cpu"))) {
				log_printf(
			gettext("UltraSPARC module Board %d Module %d\n"),
					get_upa_id(pnode) >> 1,
					get_upa_id(pnode) & 0x1);
			} else {
				log_printf(gettext("%s board %d\n"),
					board_type, bnode->board_num, 0);
			}
			pnode = next_failed_node(pnode);
		}
		bnode = bnode->next;
	}

	if (!system_failed) {
		log_printf("\n", 0);
		log_printf(gettext("No failures found in System\n"),
			0);
		log_printf("===========================\n", 0);
	}

	if (system_failed)
		return (1);
	else
		return (0);
}

static int
disp_fault_list(struct system_kstat_data *kstats)
{
	struct ft_list *ftp;
	int i;
	int result = 0;

	if (desktop)
		return (result);

	if (!kstats->ft_kstat_ok) {
		return (result);
	}

	for (i = 0, ftp = kstats->ft_array; i < kstats->nfaults;
		i++, ftp++) {
		if (!result) {
			log_printf("\n", 0);
			log_printf("Detected System Faults\n", 0);
			log_printf("======================\n", 0);
		}
		result = 1;
		if (ftp->class == FT_BOARD) {
			log_printf("Board %d %s\n", ftp->unit, ftp->msg, 0);
		} else if ((ftp->type == FT_CORE_PS) ||
		    (ftp->type == FT_PPS)) {
			log_printf("Unit %d %s failure\n", ftp->unit,
				ftp->msg, 0);
		} else if ((ftp->type == FT_OVERTEMP) &&
		    (ftp->class == FT_SYSTEM)) {
			log_printf("Clock board %s\n", ftp->msg, 0);
		} else {
			log_printf("%s failure\n", ftp->msg, 0);
		}

		log_printf("\tDetected %s",
			asctime(localtime(&ftp->create_time)), 0);
	}

	if (!result) {
		log_printf("\n", 0);
		log_printf("No System Faults found\n", 0);
		log_printf("======================\n", 0);
	}

	log_printf("\n", 0);

	return (result);
}

/*
 * disp_err_log
 *
 * Display the fatal hardware reset system error logs. These logs are
 * collected by POST and passed up through the kernel to userland.
 * They will not necessarily be present in all systems. Their form
 * might also be different in different systems.
 *
 * NOTE - We are comparing POST defined board types here. Do not confuse
 * them with kernel board types. The structure being analyzed in this
 * function is created by POST. All the defines for it are in reset_info.h,
 * which was ported from POST header files.
 */
static int
disp_err_log(struct system_kstat_data *kstats)
{
	int exit_code = 0;
	int i;
	struct reset_info *rst_info;
	struct board_info *bdp;
	char *err_msgs[MAX_MSGS]; /* holds all messages for a system board */
	int msg_idx;		/* current msg number */
	int count;		/* number added by last analyze call */
	char **msgs;

	if (desktop)
		return (exit_code);

	/* start by initializing the err_msgs array to all NULLs */
	for (i = 0; i < MAX_MSGS; i++) {
		err_msgs[i] = NULL;
	}

	/* First check to see that the reset-info kstats are present. */
	if (kstats->reset_kstats_ok == 0) {
		return (exit_code);
	}

	rst_info = &kstats->reset_info;

	/* Everything is OK, so print out time/date stamp first */
	log_printf("\n", 0);
	log_printf(
		gettext("Analysis of most recent Fatal Hardware Watchdog:\n"),
		0);
	log_printf("======================================================\n",
		0);
	log_printf("Log Date: %s\n",
		get_time(&kstats->reset_info.tod_timestamp[0]), 0);

	/* initialize the vector and the message index. */
	msgs = err_msgs;
	msg_idx = 0;

	/* Loop Through all of the boards. */
	bdp = &rst_info->bd_reset_info[0];
	for (i = 0; i < MAX_BOARDS; i++, bdp++) {

		/* Is there data for this board? */
		if ((bdp->board_desc & BD_STATE_MASK) == BD_NOT_PRESENT) {
			continue;
		}

		/* If it is a CPU Board, look for CPU data. */
		if (BOARD_TYPE(bdp->board_desc) == CPU_TYPE) {
			/* analyze CPU 0 if present */
			if (bdp->board_desc & CPU0_OK) {
				count = analyze_cpu(msgs, 0,
					bdp->cpu[0].afsr);
				msgs += count;
				msg_idx += count;
			}

			/* analyze CPU1 if present. */
			if (bdp->board_desc & CPU1_OK) {
				count = analyze_cpu(msgs, 1,
					bdp->cpu[1].afsr);
				msgs += count;
				msg_idx += count;
			}
		}

		/* Always Analyze the AC and the DCs on a board. */
		count = analyze_ac(msgs, bdp->ac_error_status);
		msgs += count;
		msg_idx += count;

		count = analyze_dc(i, msgs, bdp->dc_shadow_chain);
		msgs += count;
		msg_idx += count;

		display_msgs(err_msgs, i);

		erase_msgs(err_msgs);

		/* If any messages are logged, we have errors */
		if (msg_idx != 0) {
			exit_code = 1;
		}

		/* reset the vector and the message index */
		msg_idx = 0;
		msgs = &err_msgs[0];
	}

	return (exit_code);
}

/*
 * build_mem_tables
 *
 * This routine builds the memory table which tells how much memory
 * is present in each SIMM group of each board, what the interleave
 * factors are, and the group ID of the interleave group.
 *
 * The algorithms used are:
 *	First fill in the sizes of groups.
 *	Next build lists of all groups with same physical base.
 *	From #of members in each list, interleave factor is
 *	determined.
 *	All members of a certain list get the same interleave
 *	group ID.
 */
static void
build_mem_tables(Sys_tree *tree,
		struct system_kstat_data *kstats,
		struct grp_info *grps)
{
	struct mem_inter inter_grps;	/* temp structure for interleaves */
	struct inter_grp *intrp;
	int group;
	int i;

	/* initialize the interleave lists */
	for (i = 0, intrp = &inter_grps.i_grp[0]; i < MAX_GROUPS; i++,
	    intrp++) {
		intrp->valid = 0;
		intrp->count = 0;
		intrp->groupid = '\0';
		intrp->base = 0;
	}

	for (group = 0; group < MAX_GROUPS; group++) {
		int found;
		int board;
		struct grp *grp;
		struct bd_kstat_data *bksp;
		u_char simm_reg;
		Board_node *bnode;

		board = group/2;
		bksp = &kstats->bd_ksp_list[board];
		grp = &grps->grp[group];
		grp->group = group % 2;

		/*
		 * Copy the board type field into the group record.
		 */
		if ((bnode = find_board(tree, board)) != NULL) {
			grp->type = bnode->board_type;
		} else {
			grp->type = UNKNOWN_BOARD;
		}

		/* Make sure we have kstats for this board */
		if ((bksp->ac_kstats_ok == 0) ||
		    ((bksp->ac_memdecode[grp->group] & AC_MEM_VALID) == 0)) {
			/* Mark this group as invalid and move to next one */
			grp->valid = 0;
			continue;
		}

		/* base the group size off of the simmstat kstat. */
		if (bksp->simmstat_kstats_ok == 0) {
			grp->valid = 0;
			continue;
		}

		/* Is it bank 0 or bank 1 */
		if (grp->group == 0) {
			simm_reg = bksp->simm_status[0];
		} else {
			simm_reg = bksp->simm_status[1];
		}

		/* Now decode the size field. */
		if ((simm_reg & 0x1F) == 0x4) {
			grp->size = 64;
		} else if ((simm_reg & 0x1F) == 0xB) {
			grp->size = 256;
		} else if ((simm_reg & 0x1F) == 0xF) {
			grp->size = 1024;
		} else {
			grp->valid = 0;
			continue;
		}

		grp->valid = 1;
		grp->base = GRP_BASE(bksp->ac_memdecode[grp->group]);
		grp->board = board;
		if (grp->group == 0) {
			grp->factor = INTLV0(bksp->ac_memctl);
		} else {	/* assume it is group 1 */
			grp->factor = INTLV1(bksp->ac_memctl);
		}
		grp->groupid = '\0';	/* Not in a group yet */

		/*
		 * find the interleave list this group belongs on. If the
		 * interleave list corresponding to this base address is
		 * not found, then create a new one.
		 */

		i = 0;
		intrp = &inter_grps.i_grp[0];
		found = 0;
		while ((i < MAX_GROUPS) && !found && (intrp->valid != 0)) {
			if ((intrp->valid != 0) &&
			    (intrp->base == grp->base)) {
				grp->groupid = intrp->groupid;
				intrp->count++;
				found = 1;
			}
			i++;
			intrp++;
		}
		/*
		 * We did not find a matching base. So now i and intrp
		 * now point to the next interleave group in the list.
		 */
		if (!found) {
			intrp->count++;
			intrp->valid = 1;
			intrp->groupid = 'A' + (char) i;
			intrp->base = grp->base;
			grp->groupid = intrp->groupid;
		}
	}
}

static void
get_mem_total(struct mem_total *mem_total, struct grp_info *grps)
{
	struct grp *grp;
	int i;

	/* Start with total of zero */
	mem_total->dram = 0;
	mem_total->nvsimm = 0;

	/* For now we ignore NVSIMMs. We might want to fix this later. */
	for (i = 0, grp = &grps->grp[0]; i < MAX_GROUPS; i++, grp++) {
		if (grp->valid != 0) {
			mem_total->dram += grp->size;
		}
	}
}

/*
 * get_cpu_freq
 *
 * Return the operating frequency of a processor in Hertz. This function
 * requires as input a legal "SUNW,spitfire" node pointer. If a NULL
 * is passed in or the clock-frequency property does not exist, the
 * function returns 0.
 */
static int
get_cpu_freq(Prom_node *pnode)
{
	Prop *prop;
	int *value;

	/* find the property */
	if ((prop = find_prop(pnode, "clock-frequency")) == NULL) {
		return (0);
	}

	if ((value = (int *)get_prop_val(prop)) == NULL) {
		return (0);
	}

	return (*value);
}

/*
 * get_cache_size
 *
 * returns the size of the given processors external cache in
 * bytes. If the properties required to determine this are not
 * present, then the function returns 0.
 */
static int
get_cache_size(Prom_node *node)
{
	int *cache_size_p;		/* pointer to number of cache lines */

	/* find the properties */
	if ((cache_size_p = (int *)get_prop_val(find_prop(node,
		"ecache-size"))) == NULL) {
		return (0);
	}

	return (*cache_size_p);
}

static Prom_node *
find_pci_bus(Prom_node *node, int upa_id, int bus)
{
	Prom_node *pnode;

	/* find the first pci node */
	pnode = dev_find_node(node, "pci");

	while (pnode != NULL) {
		int id;
		int tmp_bus;

		id = get_upa_id(pnode);
		tmp_bus = get_pci_bus(pnode);

		if ((id == upa_id) &&
		    (tmp_bus == bus)) {
			break;
		}

		pnode = dev_next_node(pnode, "pci");
	}
	return (pnode);
}

/*
 * get_pci_bus
 *
 * Determines the PCI bus, either A (0) or B (1). If the function cannot
 * find the bus-ranges property, it returns -1.
 */
static int
get_pci_bus(Prom_node *pnode)
{
	int *value;

	/* look up the bus-range property */
	if ((value = (int *)get_prop_val(find_prop(pnode, "bus-range"))) ==
	    NULL) {
		return (-1);
	}

	if (*value == 0) {
		return (0);
	} else {
		return (1);
	}
}

/*
 * Find the next sibling node on the requested SBus card.
 */
static Prom_node *
next_card(Prom_node *node, int card)
{
	Prom_node *pnode;

	if (node == NULL)
		return (NULL);

	pnode = node->sibling;
	while (pnode != NULL) {
		void *value;

		/*
		 * TODO - Figure out the correct way to determine slot
		 * number of an Sbus Device. We also want to discriminate
		 * to only the Sbus Slots, and not the Macio or Happy
		 * Meal devices.
		 */
		if ((value = get_prop_val(find_prop(pnode, "reg"))) != NULL) {
			if (*(int *)value == card)
				return (pnode);
			pnode = pnode->sibling;
		}
	}
	return (NULL);
}

/*
 * find_card
 *
 * Find the first device node for the requested Sbus card.
 */
static Prom_node *
find_card(Prom_node *node, int card)
{
	Prom_node *pnode;

	if (node == NULL)
		return (NULL);

	pnode = node->child;

	while (pnode != NULL) {
		void *value;

		/*
		 * TODO - Figure out the correct way to determine slot
		 * number of an Sbus Device. We also want to discriminate
		 * to only the Sbus Slots, and not the Macio or Happy
		 * Meal devices.
		 */
		if ((value = get_prop_val(find_prop(pnode, "reg"))) != NULL)
			if (*(int *)value == card)
				return (pnode);
		pnode = pnode->sibling;
	}
	return (NULL);
}

/*
 * Find the sbus slot number of this Sbus device. If no slot number can
 * be determined, then return -1.
 */
static int
get_sbus_slot(Prom_node *pnode)
{
	void *value;

	if ((value = get_prop_val(find_prop(pnode, "reg"))) != NULL) {
		return (*(int *)value);
	} else {
		return (-1);
	}
}

#define	PCI_DEVICE(x)	((x  >> 11) & 0x1f)

/*
 * Find the PCI slot number of this PCI device. If no slot number can
 * be determined, then return -1.
 */
static int
get_pci_device(Prom_node *pnode)
{
	void *value;

	if ((value = get_prop_val(find_prop(pnode, "assigned-addresses"))) !=
	    NULL) {
		return (PCI_DEVICE(*(int *)value));
	} else {
		return (-1);
	}
}

/*
 * disp_keysw_and_leds
 *
 * This routine displays the position of the keyswitch and the front panel
 * system LEDs. The keyswitch can be in either normal, diagnostic, or
 * secure position. The three front panel LEDs are of importance because
 * the center LED indicates component failure on the system.
 */
static int
disp_keysw_and_leds(struct system_kstat_data *kstats)
{
	int board;
	int diag_mode = 0;
	int secure_mode = 0;
	int result = 0;

	/* Check the first valid board to determeine the diag bit */
	/* Find the first valid board */
	for (board = 0; board < MAX_BOARDS; board++) {
		if (kstats->bd_ksp_list[board].fhc_kstats_ok != 0) {
			/* If this was successful, break out of loop */
			if ((kstats->bd_ksp_list[board].fhc_bsr &
			    FHC_DIAG_MODE) == 0)
				diag_mode = 1;
			break;
		}
	}

	/*
	 * Check the register on the clock-board to determine the
	 * secure bit.
	 */
	if (kstats->sys_kstats_ok) {
		/* The secure bit is negative logic. */
		if (kstats->keysw_status == KEY_SECURE) {
			secure_mode = 1;
		}
	}

	/*
	 * The system cannot be in diag and secure mode. This is
	 * illegal.
	 */
	if (secure_mode && diag_mode) {
		result = 2;
		return (result);
	}

	/* Now print the keyswitch position. */
	log_printf("Keyswitch position is in ", 0);

	if (diag_mode) {
		log_printf("Diagnostic Mode\n");
	} else if (secure_mode) {
		log_printf("Secure Mode\n", 0);
	} else {
		log_printf("Normal Mode\n");
	}

	/* display the redundant power status */
	if (kstats->sys_kstats_ok) {
		log_printf("System Power Status: ", 0);

		switch (kstats->power_state) {
		case REDUNDANT:
			log_printf("Redundant\n", 0);
			break;

		case MINIMUM:
			log_printf("Minimum Available\n", 0);
			break;

		case BELOW_MINIMUM:
			log_printf("Insufficient Power Available\n", 0);
			break;

		default:
			log_printf("Unknown\n", 0);
			break;
		}
	}

	if (kstats->sys_kstats_ok) {
		/*
		 * If the center LED is on, then we return a non-zero
		 * result.
		 */
		log_printf("System LED Status:    GREEN     YELLOW     "
			"GREEN\n", 0);
		if ((kstats->sysctrl & SYS_LED_MID) != 0) {
			log_printf("WARNING                ", 0);
		} else {
			log_printf("Normal                 ", 0);
		}

		/*
		 * Left LED is negative logic, center and right LEDs
		 * are positive logic.
		 */
		if ((kstats->sysctrl & SYS_LED_LEFT) == 0) {
			log_printf("ON ", 0);
		} else {
			log_printf("OFF", 0);
		}

		log_printf("       ", 0);
		if ((kstats->sysctrl & SYS_LED_MID) != 0) {
			log_printf("ON ", 0);
		} else {
			log_printf("OFF", 0);
		}

		log_printf("       BLINKING", 0);
	}

	log_printf("\n", 0);
	return (result);
}

/*
 * disp_env_status
 *
 * This routine displays the environmental status passed up from
 * device drivers via kstats. The kstat names are defined in
 * kernel header files included by this module.
 */
static int
disp_env_status(struct system_kstat_data *kstats)
{
	struct bd_kstat_data *bksp;
	int exit_code = 0;
	int i;
	u_char curr_temp;
	int is4slot;

	/*
	 * Define some message arrays to make life simpler.  These
	 * messages correspond to definitions in <sys/fhc.c> for
	 * temperature trend (enum temp_trend) and temperature state
	 * (enum temp_state).
	 */
	static char *temp_trend_msg[] = {	"unknown",
						"rapidly falling",
						"falling",
						"stable",
						"rising",
						"rapidly rising",
						"unknown (noisy)" };
	static char *temp_state_msg[] = {	"   OK    ",
						"WARNING  ",
						" DANGER  " };

	if (desktop)
		return (exit_code);

	log_printf("\n", 0);
	log_printf(gettext("==================Environmental Status"), 0);
	log_printf("=================================\n", 0);

	if (disp_keysw_and_leds(kstats)) {
		exit_code = 1;
	}

	if (!kstats->sys_kstats_ok) {
		log_printf("*** Error: Unavailable ***\n\n");
		return (1);
	}

	is4slot = IS4SLOT(kstats->sysstat1);

	log_printf("Fans:\n", 0);
	log_printf("-----\n", 0);

	log_printf("Unit   Status\n", 0);
	log_printf("----   ------\n", 0);

	log_printf("%-4s    ", is4slot ? "Disk" : "Rack", 0);
	/* Check the status of the Rack Fans */
	if ((kstats->fan_status & SYS_RACK_FANFAIL) == 0) {
		log_printf("OK\n", 0);
	} else {
		log_printf("FAIL\n", 0);
		exit_code = 1;
	}

	if (!is4slot) {
		/*
		 * keyswitch and ac box are on 8 & 16 slot only
		 */
		/* Check the status of the Keyswitch Fan assembly. */
		log_printf("%-4s    ", "Key", 0);
		if ((kstats->fan_status & SYS_KEYSW_FAN_OK) != 0) {
			log_printf("OK\n", 0);
		} else {
			log_printf("FAIL\n", 0);
			exit_code = 1;
		}

		log_printf("%-4s    ", "AC", 0);
		if ((kstats->fan_status & SYS_AC_FAN_OK) != 0) {
			log_printf("OK\n", 0);
		} else {
			log_printf("FAIL\n", 0);
			exit_code = 1;
		}
	} else {
		/*
		 * peripheral fan is on 4 slot only
		 * XXX might want to indicate transient states too
		 */
		if (kstats->psstat_kstat_ok) {
			if (kstats->ps_shadow[SYS_P_FAN_INDEX] == PS_OK) {
				log_printf("PPS     OK\n", 0);
			} else if (kstats->ps_shadow[SYS_P_FAN_INDEX] ==
			    PS_FAIL) {
				log_printf("PPS     FAIL\n", 0);
				exit_code = 1;
			}
		}
	}

	log_printf("\n", 0);


	log_printf("System Temperatures (Celsius):\n", 0);
	log_printf("------------------------------\n", 0);
	log_printf("                ", 0);
	log_printf(" State   Current  Min  Max  Trend\n", 0);
	log_printf("                ", 0);
	log_printf("-------  -------  ---  ---  -----\n", 0);

	for (i = 0, bksp = &kstats->bd_ksp_list[0]; i < MAX_BOARDS;
	    i++, bksp++) {

		/* Make sure we have kstats for this board first */
		if (!bksp->temp_kstat_ok) {
			continue;
		}
		log_printf("Board%3d:       ", i, 0);

		/* Print the current state of the temperature */
		log_printf("%s", temp_state_msg[bksp->tempstat.state], 0);

		/* Print the current temperature */
		curr_temp = bksp->tempstat.l1[bksp->tempstat.index % L1_SZ];
		log_printf("   %2d    ", curr_temp, 0);

		/* Print the minimum recorded temperature */
		log_printf(" %2d  ", bksp->tempstat.min, 0);

		/* Print the maximum recorded temperature */
		log_printf(" %2d  ", bksp->tempstat.max, 0);

		/* Print the current trend in temperature (if available) */
		if (bksp->tempstat.version < 2)
		    log_printf("unknown\n", 0);
		else
		    log_printf("%s\n", temp_trend_msg[bksp->tempstat.trend], 0);
	}
	if (kstats->temp_kstat_ok) {
		log_printf("Control Board:  ", 0);

		/* Print the current state of the temperature */
		log_printf("%s", temp_state_msg[kstats->tempstat.state], 0);

		/* Print the current temperature */
		curr_temp = kstats->tempstat.l1[kstats->tempstat.index % L1_SZ];
		log_printf("   %2d    ", curr_temp, 0);

		/* Print the minimum recorded temperature */
		log_printf(" %2d  ", kstats->tempstat.min, 0);

		/* Print the maximum recorded temperature */
		log_printf(" %2d  ", kstats->tempstat.max, 0);

		/* Print the current trend in temperature (if available) */
		if (kstats->tempstat.version < 2)
		    log_printf("unknown\n\n", 0);
		else
		    log_printf("%s\n\n",
			    temp_trend_msg[kstats->tempstat.trend], 0);
	} else {
		log_printf("\n");
	}

	log_printf("\n", 0);
	log_printf("Power Supplies:\n", 0);
	log_printf("---------------\n", 0);
	log_printf("Supply                        Status\n", 0);
	log_printf("---------                     ------\n", 0);
	if (kstats->psstat_kstat_ok) {
		for (i = 0; i < SYS_PS_COUNT; i++) {
			char *ps, *state;

			/* skip core power supplies that are not present */
			if (i <= SYS_PPS0_INDEX && kstats->ps_shadow[i] ==
			    PS_OUT)
				continue;

			/* Display the unit Number */
			switch (i) {
			case 0: ps = "0"; break;
			case 1: ps = "1"; break;
			case 2: ps = "2"; break;
			case 3: ps = "3"; break;
			case 4: ps = "4"; break;
			case 5: ps = "5"; break;
			case 6: ps = "6"; break;
			case 7: ps = is4slot ? "2nd PPS" : "7"; break;

			case SYS_PPS0_INDEX: ps = "PPS"; break;
			case SYS_CLK_33_INDEX: ps = "System 3.3v"; break;
			case SYS_CLK_50_INDEX: ps = "System 5.0v"; break;
			case SYS_V5_P_INDEX: ps = "Peripheral 5.0v"; break;
			case SYS_V12_P_INDEX: ps = "Peripheral 12v"; break;
			case SYS_V5_AUX_INDEX: ps = "Auxilary 5.0v"; break;
			case SYS_V5_P_PCH_INDEX: ps =
				"Peripheral 5.0v precharge";
				break;
			case SYS_V12_P_PCH_INDEX: ps =
				"Peripheral 12v precharge";
				break;
			case SYS_V3_PCH_INDEX: ps =
				"System 3.3v precharge"; break;
			case SYS_V5_PCH_INDEX: ps =
				"System 5.0v precharge"; break;

			/* skip the peripheral fan here */
			case SYS_P_FAN_INDEX:
				continue;
			}

			/* what is the state? */
			switch (kstats->ps_shadow[i]) {
			case PS_OK:
				state = "OK";
				break;

			case PS_FAIL:
				state = "FAIL";
				exit_code = 1;
				break;

			/* XXX is this an exit_code condition? */
			case PS_OUT:
				state = "PPS Out";
				exit_code = 1;
				break;

			case PS_UNKNOWN:
				state = "Unknown";
				break;

			default:
				state = "Illegal State";
				break;
			}

			log_printf("%-30s %s\n", ps, state, 0);
		}
	}

	/* Check status of the system AC Power Source */
	log_printf("%-30s ", "AC Power", 0);
	if ((kstats->sysstat2 & SYS_AC_FAIL) == 0) {
		log_printf("OK\n", 0);
	} else {
		log_printf("failed\n", 0);
		exit_code = 1;
	}
	log_printf("\n", 0);

	return (exit_code);
}

/*
 * Many of the ASICs present in fusion machines have implementation and
 * version numbers stored in the OBP device tree. These codes are displayed
 * in this routine in an effort to aid Engineering and Field service
 * in detecting old ASICs which may have bugs in them.
 */
void
disp_asic_revs(Sys_tree *tree)
{
	Board_node *bnode;
	Prom_node *pnode;
	struct ffbinfo *ffb_ptr;

	/* Else this is a Sunfire or campfire */
	log_printf("ASIC Revisions:\n", 0);
	log_printf("---------------\n", 0);

	/* Display Firetruck ASIC Revisions first */
	log_printf("         FHC  AC  SBus0 SBus1 FEPS\n", 0);
	log_printf("         ---  --  ----- ----- ----\n", 0);

	/*
	 * Display all of the FHC, AC, and chip revisions for the entire
	 * machine. The AC anf FHC chip revs are available  from the device
	 * tree that was read out of the PROM, but the DC chip revs will be
	 * read via a kstat. The interfaces for this are not completely
	 * available at this time.
	 */
	bnode = tree->bd_list;
	while (bnode != NULL) {
		int *version;
		int upa = bd_to_upa(bnode->board_num);

		/* Display the header with the board number */
		log_printf("Board%2d: ", bnode->board_num, 0);

		/* display the FHC version */
		if ((pnode = dev_find_node(bnode->nodes, "fhc")) == NULL) {
			log_printf("     ", 0);
		} else {
			if ((version = (int *)get_prop_val(find_prop(pnode,
			    "version#"))) == NULL) {
				log_printf("     ", 0);
			} else {
				log_printf(" %d   ", *version, 0);
			}
		}

		/* display the AC version */
		if ((pnode = dev_find_node(bnode->nodes, "ac")) == NULL) {
			log_printf("    ", 0);
		} else {
			if ((version = (int *)get_prop_val(find_prop(pnode,
			    "version#"))) == NULL) {
				log_printf("    ", 0);
			} else {
				log_printf(" %d  ", *version, 0);
			}
		}

		/* Find sysio 0 on board and print rev */
		if ((pnode = find_upa_device(bnode, upa, "sbus")) == NULL) {
			log_printf("      ", 0);
		} else {
			if ((version = (int *)get_prop_val(find_prop(pnode,
			    "ver#"))) == NULL) {
				log_printf("      ", 0);
			} else {
				log_printf("  %d   ", *version, 0);
			}
		}

		/* Find sysio 1 on board and print rev */
		if ((pnode = find_upa_device(bnode, upa+1, "sbus")) == NULL) {
			log_printf("      ", 0);
		} else {
			if ((version = (int *)get_prop_val(find_prop(pnode,
			    "ver#"))) == NULL) {
				log_printf("      ", 0);
			} else {
				log_printf("  %d   ", *version, 0);
			}
		}

#if 0
		/* Find Psycho 0 on board and print rev */
		if ((pnode = find_upa_device(bnode, upa, "pci")) == NULL) {
			log_printf("      ", 0);
		} else {
			if ((version = (int *)get_prop_val(find_prop(pnode,
			    "ver#"))) == NULL) {
				log_printf("      ", 0);
			} else {
				log_printf(" %d    ", *version, 0);
			}
		}

		/* Find Psycho 1 on board and print rev */
		if ((pnode = find_upa_device(bnode, upa+1, "pci")) == NULL) {
			log_printf("     ", 0);
		} else {
			if ((version = (int *)get_prop_val(find_prop(pnode,
			    "ver#"))) == NULL) {
				log_printf("      ", 0);
			} else {
				log_printf(" %d    ", *version, 0);
			}
		}
#endif

		/* Find the FEPS on board and print rev */
		if ((pnode = dev_find_node(bnode->nodes, "SUNW,hme")) != NULL) {
			if ((version = (int *) get_prop_val(find_prop(pnode,
			    "hm-rev"))) != NULL) {
				if (*version == 0xa0) {
					log_printf(" 2.0", 0);
				} else if (*version == 0x20) {
					log_printf(" 2.1", 0);
				} else {
					log_printf(" %x", *version, 0);
				}
			}
		}

		log_printf("\n", 0);
		bnode = bnode->next;
	}
	log_printf("\n", 0);

	/* Now display the FFB board component revisions */
	for (ffb_ptr = ffb_list; ffb_ptr != NULL; ffb_ptr = ffb_ptr->next) {
		log_printf("Board %d FFB Hardware Configuration:\n",
			ffb_ptr->board, 0);
		log_printf("-----------------------------------\n", 0);

		display_ffb(ffb_ptr, 1);
		log_printf("\n", 0);
	}
}

void
dt_disp_asic_revs(Sys_tree *tree)
{
	Board_node *bnode;
	Prom_node *pnode;
	char *name;
	int *version;

	bnode = tree->bd_list;

	log_printf("ASIC Revisions:\n", 0);
	log_printf("---------------\n", 0);

	/* Find sysio and print rev */
	if ((pnode = dev_find_node(bnode->nodes, "sbus")) != NULL) {
		version = (int *) get_prop_val(find_prop(pnode, "ver#"));
		name = get_prop_val(find_prop(pnode, "name"));

		if ((version != NULL) && (name != NULL)) {
			log_printf("SBus: %s Rev %d\n", name, *version, 0);
		}
	}

#if 0
	/* Find Psycho and print rev */
	if ((pnode = dev_find_node(bnode->nodes, "pci")) != NULL) {
		version = (int *) get_prop_val(find_prop(pnode, "ver#"));
		name = get_prop_val(find_prop(pnode, "name"));

		if ((version != NULL) && (name != NULL)) {
			log_printf("PCI: %s Rev %d\n", name,  *version, 0);
		}
	}
#endif

	/* Find the FEPS and print rev */
	if ((pnode = dev_find_node(bnode->nodes, "SUNW,hme")) != NULL) {
		version = (int *) get_prop_val(find_prop(pnode,	"hm-rev"));
		name = get_prop_val(find_prop(pnode, "name"));

		if ((version != NULL) && (name != NULL)) {
			log_printf("FEPS: %s Rev ", name);
			if (*version == 0xa0) {
				log_printf("2.0\n", 0);
			} else if (*version == 0x20) {
				log_printf("2.1\n", 0);
			} else {
				log_printf("%x\n", *version, 0);
			}
		}
	}
	log_printf("\n", 0);

	if ((find_upa_device(tree->bd_list, 0x1E, FFB_NAME) != NULL) &&
	    (ffb_list != NULL)) {
		log_printf("FFB Hardware Configuration:\n", 0);
		log_printf("----------------------------\n", 0);

		display_ffb(ffb_list, 1);
		log_printf("\n", 0);
	}
}

void
sf_disp_prom_versions(Sys_tree *tree)
{
	Board_node *bnode;

	/* Display Prom revision header */
	log_printf("System Board PROM revisions:\n", 0);
	log_printf("----------------------------\n", 0);

	/* For each board, print the POST and OBP versions */
	for (bnode = tree->bd_list; bnode != NULL; bnode = bnode->next) {
		Prom_node *flashprom;	/* flashprom device node */

		/* find a flashprom node for this board */
		flashprom = dev_find_node(bnode->nodes, "flashprom");

		/* If no flashprom node found, continue */
		if (flashprom == NULL)
			continue;

		/* flashprom node found, display board# */
		log_printf("Board%2d: ", bnode->board_num, 0);

		disp_prom_version(flashprom);
	}
}

static void
dt_disp_prom_version(Sys_tree *tree)
{
	Board_node *bnode;
	Prom_node *pnode;

	bnode = tree->bd_list;

	/* Display Prom revision header */
	log_printf("System PROM revisions:\n", 0);
	log_printf("----------------------\n", 0);

	if ((pnode = find_upa_device(bnode, 0x1F, SBUS_NAME)) == NULL) {
		pnode = find_pci_bus(bnode->nodes, 0x1F, 0);
	}

	pnode = dev_find_node(pnode, "flashprom");

	if (pnode != NULL) {
		disp_prom_version(pnode);
	}
}

static void
disp_prom_version(Prom_node *flashprom)
{
	Prop *version;
	char *vers;		/* OBP version */
	char *temp;

	/* Look version */
	version = find_prop(flashprom, "version");

	vers = (char *) get_prop_val(version);

	if (vers != NULL) {
		log_printf("  %s   ", vers, 0);

		/*
		 * POST string follows the NULL terminated OBP
		 * version string. Do not attempt to print POST
		 * string unless node size is larger than the
		 * length of the OBP version string.
		 */
		if ((strlen(vers) + 1) < version->size) {
			temp = vers + strlen(vers) + 1;
			log_printf("%s", temp, 0);
		}
	}

	log_printf("\n", 0);
}

static void
display_msgs(char **msgs, int board)
{
	int i;

	/* display the header for this board */
	print_header(board);

	for (i = 0; (*msgs != NULL) && (i < MAX_MSGS); i++, msgs++) {
		log_printf(*msgs, 0);
	}
}

static void
erase_msgs(char **msgs)
{
	int i;

	for (i = 0; (*msgs != NULL) && (i < MAX_MSGS); i++, msgs++) {
		free(*msgs);
		*msgs = NULL;
	}
}
