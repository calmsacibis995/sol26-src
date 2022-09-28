/*
 * Copyright (c) 1987-1995 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)cpu_module.c	1.8	96/09/09 SMI"

#include <sys/types.h>
#include <sys/cpu_module.h>

/*
 * This is a dummy file that provides the default cpu module
 * that is linked to unix.
 */

void
cpu_setup(void)
{}

/* ARGSUSED */
void
vtag_flushpage(caddr_t addr, u_int ctx)
{}

/* ARGSUSED */
void
vtag_flushctx(u_int ctx)
{}

/* ARGSUSED */
void
vtag_flushpage_tl1(caddr_t addr, u_int ctx)
{}

/* ARGSUSED */
void
vtag_flushctx_tl1(u_int ctx)
{}

/* ARGSUSED */
void
vac_flushpage(u_int pf, int color)
{}

/* ARGSUSED */
void
vac_flushpage_tl1(u_int pf, int color)
{}

/* ARGSUSED */
void
vac_flushcolor(int color)
{}

/* ARGSUSED */
void
vac_flushcolor_tl1(int color)
{}

/* ARGSUSED */
void
init_mondo(u_int func, u_int arg1, u_int arg2, u_int arg3, u_int arg4)
{}

/* ARGSUSED */
void
send_mondo(int upaid)
{}

void
fini_mondo(void)
{}

/* ARGSUSED */
void
flush_instr_mem(caddr_t addr, u_int len)
{}

void
syncfpu(void)
{}

void
scrub_ecc(void)
{}

void
ce_err(void)
{}

void
async_err(void)
{}

void
dis_err_panic1(void)
{}

void
clr_datapath(void)
{}

void
cpu_flush_ecache(void)
{}

/* ARGSUSED */
u_int
cpu_get_status(struct ecc_flt *ecc)
{ return 0; }

/* ARGSUSED */
void
itlb_rd_entry(u_int entry, tte_t *tte, caddr_t *addr, int *ctxnum)
{}

/* ARGSUSED */
void
dtlb_rd_entry(u_int entry, tte_t *tte, caddr_t *addr, int *ctxnum)
{}
