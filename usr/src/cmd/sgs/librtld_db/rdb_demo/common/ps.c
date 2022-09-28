
/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided this notice is preserved and that due credit is given
 * to Sun Microsystems, Inc.  The name of Sun Microsystems, Inc. may
 * not be used to endorse or promote products derived from this
 * software without specific prior written permission.  This software
 * is provided ``as is'' without express or implied warranty.
 */
#pragma ident	"@(#)ps.c	1.7	96/09/10 SMI"


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <sys/procfs.h>
#include <sys/auxv.h>
#include <libelf.h>
#include <sys/param.h>
#include <stdarg.h>

#include <proc_service.h>

#include "rdb.h"
#include "disasm.h"


retc_t
ps_init(int pfd, struct ps_prochandle * procp)
{
	int		fd;
	rd_notify_t	rd_notify;
	long		pflags;

	procp->pp_fd = pfd;
	if ((procp->pp_ldsobase = get_ldbase(pfd)) != (ulong_t)-1) {
		if ((fd = ioctl(pfd, PIOCOPENM, &(procp->pp_ldsobase))) == -1)
			perr("ip: PIOCOPENM");
		load_map(fd, &(procp->pp_ldsomap));
		procp->pp_ldsomap.mi_addr += procp->pp_ldsobase;
		procp->pp_ldsomap.mi_end += procp->pp_ldsobase;
		procp->pp_ldsomap.mi_name = "<procfs: interp>";
		close(fd);
	}
	if ((fd = ioctl(pfd, PIOCOPENM, 0)) == -1)
		perr("ip1: PIOCOPENM");

	load_map(fd, &(procp->pp_execmap));
	procp->pp_execmap.mi_name = "<procfs: exec>";
	close(fd);
	procp->pp_breakpoints = 0;
	procp->pp_flags = FLG_PP_PACT | FLG_PP_PLTSKIP;
	procp->pp_lmaplist.ml_head = 0;
	procp->pp_lmaplist.ml_tail = 0;
	procp->pp_auxvp = 0;
	if ((procp->pp_rap = rd_new(procp)) == 0) {
		fprintf(stderr, "rdb: rtld_db: rd_new() call failed\n");
		exit(1);
	}
	rd_event_enable(procp->pp_rap, 1);

	/*
	 * For those architectures that increment the PC on
	 * a breakpoint fault we enable the PR_BPTADJ adjustments.
	 */
	pflags = PR_BPTADJ;
	if (ioctl(procp->pp_fd, PIOCSET, &pflags) != 0)
		perr("ps_init: PIOCSET(PR_BPTADJ)");


	/*
	 * Set breakpoints for special handshakes between librtld_db.so
	 * and the debugger.  These include:
	 *	PREINIT		- before .init processing.
	 *	POSTINIT	- after .init processing
	 *	DLACTIVITY	- link_maps status has changed
	 */
	if (rd_event_addr(procp->pp_rap, RD_PREINIT, &rd_notify) == RD_OK) {
		if (set_breakpoint(procp, rd_notify.u.bptaddr,
		    FLG_BP_RDPREINIT) != RET_OK)
			fprintf(stderr,
				"psi: failed to set BP for preinit at: 0x%x\n",
				rd_notify.u.bptaddr);
	} else
		fprintf(stderr, "psi: no event registered for preinit\n");

	if (rd_event_addr(procp->pp_rap, RD_POSTINIT, &rd_notify) == RD_OK) {
		if (set_breakpoint(procp, rd_notify.u.bptaddr,
		    FLG_BP_RDPOSTINIT) != RET_OK)
			fprintf(stderr,
				"psi: failed to set BP for postinit at: 0x%x\n",
				rd_notify.u.bptaddr);
	} else
		fprintf(stderr, "psi: no event registered for postinit\n");

	if (rd_event_addr(procp->pp_rap, RD_DLACTIVITY, &rd_notify) == RD_OK) {
		if (set_breakpoint(procp, rd_notify.u.bptaddr,
		    FLG_BP_RDDLACT) != RET_OK)
			fprintf(stderr,
				"psi: failed to set BP for dlact at: 0x%x\n",
				rd_notify.u.bptaddr);
	} else
		fprintf(stderr, "psi: no event registered for dlact\n");

	return (RET_OK);
}


retc_t
ps_close(struct ps_prochandle * ph)
{
	if (ph->pp_auxvp)
		free(ph->pp_auxvp);
	delete_all_breakpoints(ph);
	free_linkmaps(ph);
	return (RET_OK);
}


ps_err_e
ps_auxv(struct ps_prochandle * ph, auxv_t ** auxvp)
{
	int		auxnum;

	if (ph->pp_auxvp != 0) {
		*auxvp = ph->pp_auxvp;
		return (PS_OK);
	}
	if (ioctl(ph->pp_fd, PIOCNAUXV, &auxnum) != 0)
		return (PS_ERR);

	if (auxnum < 1)
		return (PS_ERR);

	ph->pp_auxvp = (auxv_t *)malloc(sizeof (auxv_t) * auxnum);

	if (ioctl(ph->pp_fd, PIOCAUXV, auxvp) != 0) {
		free(ph->pp_auxvp);
		return (PS_ERR);
	}
	*auxvp = ph->pp_auxvp;
	return (PS_OK);
}

ps_err_e
ps_pread(const struct ps_prochandle * ph, ulong_t addr, char * buf,
	int size)
{
	if ((lseek(ph->pp_fd, addr, SEEK_SET) == -1) ||
	    (read(ph->pp_fd, buf, size) != size))
		return (PS_ERR);
	return (PS_OK);
}


ps_err_e
ps_pwrite(const struct ps_prochandle * ph, ulong_t addr, char * buf,
	int size)
{
	if ((lseek(ph->pp_fd, addr, SEEK_SET) == -1) ||
	    (write(ph->pp_fd, buf, size) != size))
		return (PS_ERR);
	return (PS_OK);
}


ps_err_e
ps_pglobal_sym(const struct ps_prochandle * ph,
	const char * ld_object_name, const char * ld_symbol_name,
	Elf32_Sym * symp)
{
	map_info_t *	mip;

	if ((mip = str_to_map(ph, ld_object_name)) == 0)
		return (PS_ERR);

	if (str_map_sym(ld_symbol_name, mip, symp) == RET_FAILED)
		return (PS_ERR);

	return (PS_OK);
}


ps_err_e
ps_pglobal_lookup(const struct ps_prochandle * ph,
	const char * ld_object_name, const char * ld_symbol_name,
	ulong_t * ld_symbol_addr)
{
	Elf32_Sym	sym;
	map_info_t *	mip;

	if ((mip = str_to_map(ph, ld_object_name)) == 0)
		return (PS_ERR);

	if (str_map_sym(ld_symbol_name, mip, &sym) == RET_FAILED)
		return (PS_ERR);
	*ld_symbol_addr = sym.st_value;

	return (PS_OK);
}



ps_err_e
ps_pget_ehdr(const struct ps_prochandle * ph, const char * ld_object_name,
	Elf32_Ehdr * ehdr)
{
	map_info_t *	mip;
	if (ld_object_name == (const char *)0) {
		if (ph->pp_ldsobase == (ulong_t)-1)
			return (PS_ERR);
		ehdr =  ph->pp_ldsomap.mi_ehdr;
		return (PS_OK);
	}
	if (ld_object_name == (const char *)1) {
		ehdr = ph->pp_execmap.mi_ehdr;
		return (PS_OK);
	}

	for (mip = ph->pp_lmaplist.ml_head; mip; mip = mip->mi_next)
		if (strcmp(mip->mi_name, ld_object_name) == 0) {
			ehdr = mip->mi_ehdr;
			return (PS_OK);
		}
	return (PS_ERR);
}


ps_err_e
ps_lgetregs(const struct ps_prochandle * ph, lwpid_t lid,
	prgregset_t gregset)
{
	int	lwpfd;

	if ((lwpfd = ioctl(ph->pp_fd, PIOCOPENLWP, &lid)) == -1)
		return (PS_ERR);
	if (ioctl(lwpfd, PIOCGREG, gregset) != 0)
		return (PS_ERR);
	close(lwpfd);
	return (PS_OK);
}


void
ps_plog(const char * fmt, ...)
{
	va_list		args;
	static FILE *	log_fp = 0;

	if (log_fp == 0) {
		char		log_fname[256];
		(void) sprintf(log_fname, "/tmp/tdlog.%d", getpid());
		if ((log_fp = fopen(log_fname, "w")) == 0) {
			/*
			 * unable to open log file - default to
			 * stderr.
			 */
			fprintf(stderr, "unable to open %s, logging "
				"redirected to stderr");
			log_fp = stderr;
		}
	}

	va_start(args, fmt);
	vfprintf(log_fp, fmt, args);
	va_end(args);
	fflush(log_fp);
}
