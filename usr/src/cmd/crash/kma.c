#ident	"@(#)kma.c	1.8	96/07/28 SMI"		/* SVr4.0 1.1.1.1 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *		Copyright (C) 1991  Sun Microsystems, Inc
 *			All rights reserved.
 *		Notice of copyright on this source code
 *		product does not indicate publication.
 *
 *		RESTRICTED RIGHTS LEGEND:
 *   Use, duplication, or disclosure by the Government is subject
 *   to restrictions as set forth in subparagraph (c)(1)(ii) of
 *   the Rights in Technical Data and Computer Software clause at
 *   DFARS 52.227-7013 and in similar clauses in the FAR and NASA
 *   FAR Supplement.
 */

/*
 * This file contains code for the crash function kmastat.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/kmem_impl.h>
#include <sys/kstat.h>
#include <sys/elf.h>
#include "crash.h"

static void	kmainit(), prkmastat();

static void kmause(void *kaddr, void *buf,
		u_int size, kmem_bufctl_audit_t *bcp);

Elf32_Sym *kmem_null_cache_sym, *kmem_misc_kstat_sym;

/* get arguments for kmastat function */
int
getkmastat()
{
	int c;

	kmainit();
	optind = 1;
	while ((c = getopt(argcnt, args, "w:")) != EOF) {
		switch (c) {
			case 'w' :	(void) redirect();
					break;
			default  :	longjmp(syn, 0);
		}
	}
	prkmastat();
	return (0);
}

static int
knval(kstat_named_t *knp, char *name)
{
	while (strcmp(knp->name, name) != 0)
		knp++;
	return (knp->value.l);
}

/* print kernel memory allocator statistics */
static void
prkmastat()
{
	kmem_cache_t c, *cp;
	kmem_cache_stat_t kcs;
	u_int total_alloc, total_alloc_fail;
	u_int kmem_null_cache_addr;
	kstat_named_t *knp;

	if ((knp = malloc(kmem_misc_kstat_sym->st_size)) == NULL)
		return;

	(void) fprintf(fp, "%-25s %5s %5s %5s %8s %15s\n",
		"", "buf", "buf", "buf", "memory", "#allocations");
	(void) fprintf(fp, "%-25s %5s %5s %5s %8s %10s %4s\n",
		"cache name ", "size", "avail", "total", "in use",
		"succeed", "fail");
	(void) fprintf(fp, "%-25s %5s %5s %5s %8s %10s %4s\n",
		"----------", "-----", "-----", "-----", "--------",
		"-------", "----");

	kvm_read(kd, kmem_misc_kstat_sym->st_value, (char *)knp,
		kmem_misc_kstat_sym->st_size);

	kmem_null_cache_addr = kmem_null_cache_sym->st_value;
	kvm_read(kd, kmem_null_cache_addr, (char *)&c, sizeof (c));

	total_alloc = knval(knp, "perm_alloc") + knval(knp, "huge_alloc");
	total_alloc_fail = knval(knp, "perm_alloc_fail") +
		knval(knp, "huge_alloc_fail");

	for (cp = c.cache_next; cp != (kmem_cache_t *)kmem_null_cache_addr;
	    cp = c.cache_next) {

		kvm_read(kd, (u_int)cp, (char *)&c, sizeof (c));
		if (kmem_cache_getstats(cp, &kcs) == -1) {
			printf("error reading stats for %s\n", c.cache_name);
			continue;
		}

		total_alloc += kcs.kcs_alloc;
		total_alloc_fail += kcs.kcs_alloc_fail;

		(void) fprintf(fp, "%-25s %5u %5u %5u %8u %10u %4u\n",
			c.cache_name,
			(u_int)kcs.kcs_buf_size,
			(u_int)kcs.kcs_buf_avail,
			(u_int)kcs.kcs_buf_total,
			(u_int)kcs.kcs_slab_size *
			    (kcs.kcs_slab_create - kcs.kcs_slab_destroy),
			(u_int)kcs.kcs_alloc,
			(u_int)kcs.kcs_alloc_fail);
	}

	(void) fprintf(fp, "%-25s %5s %5s %5s %8s %10s %4s\n",
		"----------", "-----", "-----", "-----", "--------",
		"-------", "----");

	(void) fprintf(fp, "%-25s %5s %5s %5s %8u %10u %4u\n",
		"permanent", "-", "-", "-",
		knval(knp, "perm_size"),
		knval(knp, "perm_alloc"),
		knval(knp, "perm_alloc_fail"));

	(void) fprintf(fp, "%-25s %5s %5s %5s %8u %10u %4u\n",
		"oversize", "-", "-", "-",
		knval(knp, "huge_size"),
		knval(knp, "huge_alloc"),
		knval(knp, "huge_alloc_fail"));

	(void) fprintf(fp, "%-25s %5s %5s %5s %8s %10s %4s\n",
		"----------", "-----", "-----", "-----", "--------",
		"-------", "----");

	(void) fprintf(fp, "%-25s %5s %5s %5s %8u %10u %4u\n",
		"Total", "-", "-", "-",
		knval(knp, "arena_size"),
		total_alloc,
		total_alloc_fail);

	free(knp);
}

/* initialization for namelist symbols */
static void
kmainit()
{
	static int kmainit_done = 0;

	if (kmainit_done)
		return;
	if ((kmem_null_cache_sym = symsrch("kmem_null_cache")) == 0)
		(void) error("kmem_null_cache not in symbol table\n");
	if ((kmem_misc_kstat_sym = symsrch("kmem_misc_kstat")) == 0)
		(void) error("kmem_misc_kstat not in symbol table\n");
	kmainit_done = 1;
}

static int kmafull;
static kmem_cache_t *current_cache, *kmem_pagectl_cache;

/*
 * Print "kmem_alloc*" usage with stack traces when KMF_AUDIT is enabled
 */
int
getkmausers()
{
	int c;
	kmem_cache_t *cp, *kma_cache[1000];
	int ncaches, i;
	int mem_threshold = 8192;	/* Minimum # bytes for printing */
	int cnt_threshold = 100;	/* Minimum # blocks for printing */
	int audited_caches = 0;
	int do_all_caches = 0;

	kmafull = 0;
	optind = 1;
	while ((c = getopt(argcnt, args, "efw:")) != EOF) {
		switch (c) {
			case 'e':
				mem_threshold = 0;
				cnt_threshold = 0;
				break;
			case 'f':
				kmafull = 1;
				break;
			case 'w':
				redirect();
				break;
			default:
				longjmp(syn, 0);
		}
	}

	init_owner();

	if (args[optind]) {
		ncaches = 0;
		do {
			cp = kmem_cache_find(args[optind]);
			if (cp == NULL)
				error("Unknown cache: %s\n",
					args[optind]);
			kma_cache[ncaches++] = cp;
			optind++;
		} while (args[optind]);
	} else {
		ncaches = kmem_cache_find_all("", kma_cache, 1000);
		do_all_caches = 1;
	}

	for (i = 0; i < ncaches; i++) {
		kmem_cache_t c;
		cp = kma_cache[i];

		if (kvm_read(kd, (u_long)cp, (void *)&c, sizeof (c)) == -1) {
			perror("kvm_read kmem_cache");
			return (-1);
		}

		if (!(c.cache_flags & KMF_AUDIT)) {
			if (!do_all_caches)
				error("KMF_AUDIT is not enabled for %s\n",
					c.cache_name);
			continue;
		}

		if (strcmp(c.cache_name, "kmem_pagectl_cache") == 0)
			kmem_pagectl_cache = cp;
		current_cache = &c;
		kmem_cache_audit_apply(cp, kmause);
		audited_caches++;
	}

	if (audited_caches == 0 && do_all_caches)
		error("KMF_AUDIT is not enabled for any caches\n");

	print_owner("allocations", mem_threshold, cnt_threshold);
	return (0);
}

/* ARGSUSED */
static void
kmause(void *kaddr, void *buf, u_int size, kmem_bufctl_audit_t *bcp)
{
	int i;

	if (bcp->bc_cache == kmem_pagectl_cache) {
		kaddr = ((kmem_pagectl_t *)buf)->pc_addr;
		size = ((kmem_pagectl_t *)buf)->pc_size;
	}
	if (kmafull) {
		fprintf(fp, "size %d, addr %x, thread %x, cache %s\n",
			size, kaddr, bcp->bc_thread, current_cache->cache_name);
		for (i = 0; i < bcp->bc_depth; i++) {
			fprintf(fp, "\t ");
			prsymbol(NULL, bcp->bc_stack[i]);
		}
	}
	add_owner(bcp, size, size);
}
