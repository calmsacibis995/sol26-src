/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */

#pragma ident "@(#)machdep.c	1.10	96/06/11 SMI"


#include <sys/types.h>
#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/promif.h>
#include <sys/salib.h>

int pagesize = PAGESIZE;
int vac = 1;

void
fiximp(void)
{
	extern int use_align;

	use_align = 1;
}

void
setup_aux(void)
{
	pstack_t *stk;
	dnode_t node;
	dnode_t sp[OBP_STACKDEPTH];
	char name[OBP_MAXDRVNAME];
	static char cpubuf[2 * OBP_MAXDRVNAME];
	extern u_int icache_flush;
	extern char *cpulist;

	icache_flush = 1;
	stk = prom_stack_init(sp, sizeof (sp));
	node = prom_findnode_bydevtype(prom_rootnode(), "cpu", stk);
	if (node != OBP_NONODE && node != OBP_BADNODE) {
		if (prom_getprop(node, OBP_NAME, name) <= 0)
			prom_panic("no name in cpu node");
		(void) strcpy(cpubuf, name);
		if (prom_getprop(node, OBP_COMPATIBLE, name) > 0) {
			(void) strcat(cpubuf, ":");
			(void) strcat(cpubuf, name);
		}
		cpulist = cpubuf;
	} else
		prom_panic("no cpu node");
	prom_stack_fini(stk);
}


#ifdef MPSAS

void sas_symtab(int start, int end);
extern int sas_command(char *cmdstr);

/*
 * SAS support - inform SAS of new symbols being dynamically added
 * during simulation via the first standalone.
 */

#ifndef	BUFSIZ
#define	BUFSIZ	1024		/* for cmd string buffer allocation */
#endif

int	sas_symdebug = 0;		/* SAS support */

void
sas_symtab(int start, int end)
{
	char *addstr = "symtab add $LD_KERNEL_PATH/%s%s 0x%x 0x%x\n";
	char *file, *prefix, cmdstr[BUFSIZ];
	extern char filename[];

	file = filename;
	prefix = *file == '/' ? "../../.." : "";

	(void) sprintf(cmdstr, addstr, prefix, file, start, end);

	/* add the symbol table */
	if (sas_symdebug) (void) printf("sas_symtab: %s", cmdstr);
	sas_command(cmdstr);
}

void
sas_bpts()
{
	sas_command("file $KERN_SCRIPT_FILE\n");
}
#endif	/* MPSAS */

/*
 * Stub for sun4c/sunmmu routines.
 */

/* ARGSUSED */
void
sunm_vac_flush(caddr_t v, u_int nbytes)
{
}
