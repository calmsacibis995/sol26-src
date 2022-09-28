
#pragma ident   "@(#)ecc.c	1.69	96/09/09 SMI"

/*LINTLIBRARY*/

/*
 * Copyright (c) 1989 by Sun Microsystems, Inc.
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/machparam.h>
#include <sys/machsystm.h>
#include <sys/machthread.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
#include <vm/page.h>
#include <vm/mach_page.h>
#include <vm/hat.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <sys/map.h>
#include <sys/vmmac.h>
#include <sys/mman.h>
#include <sys/cmn_err.h>
#include <sys/async.h>
#include <sys/spl.h>
#include <sys/trap.h>
#include <sys/machtrap.h>
#include <sys/promif.h>
#include <sys/prom_plat.h>
#include <sys/debug.h>
#include <sys/x_call.h>
#include <sys/ivintr.h>
#include <sys/cred.h>
#include <sys/atomic_prim.h>
#include <sys/cpu_module.h>
#include <sys/spitregs.h>

static void ecc_error_init(void);

static u_int handle_ce_error(struct ecc_flt *pce);
static int ce_log_mem_err(struct ecc_flt *ecc);
static void ce_log_unum(int found_unum, int persistent,
		int len, char *unum, short syn_code);
static void ce_log_syn_code(short syn_code);
static void ce_scrub_mem_err();

static u_int handle_ue_error(struct ecc_flt *pce);
static int ue_log_mem_err(struct ecc_flt *ecc, char *unum);
static int ue_reset_ecc(u_longlong_t *flt_addr);
static int ue_check_upa_func(void);

static u_int handle_bto_error(struct bto_flt *pto);
static void kill_proc(struct proc *up, caddr_t addr);

caddr_t map_paddr_to_vaddr(u_longlong_t aligned_addr);
void unmap_vaddr(u_longlong_t aligned_addr, caddr_t vaddr);
int ecc_gen(int high_bytes, int low_bytes);

extern u_int get_error_enable_tl1(volatile u_longlong_t *neer, u_int dflag);
extern u_int set_error_enable_tl1(volatile u_longlong_t *neer);

/*
 * This table used to determine which bit(s) is(are) bad when an ECC
 * error occurrs.  The array is indexed by the 8-bit syndrome which
 * comes from the Datapath Error Register.  The entries
 * of this array have the following semantics:
 *
 *      00-63   The number of the bad bit, when only one bit is bad.
 *      64      ECC bit C0 is bad.
 *      65      ECC bit C1 is bad.
 *      66      ECC bit C2 is bad.
 *      67      ECC bit C3 is bad.
 *      68      ECC bit C4 is bad.
 *      69      ECC bit C5 is bad.
 *      70      ECC bit C6 is bad.
 *      71      ECC bit C7 is bad.
 *      72      Two bits are bad.
 *      73      Three bits are bad.
 *      74      Four bits are bad.
 *      75      More than Four bits are bad.
 *      76      NO bits are bad.
 * Based on "Galaxy Memory Subsystem SPECIFICATION" rev 0.6, pg. 28.
 */
char ecc_syndrome_tab[] =
{
76, 64, 65, 72, 66, 72, 72, 73, 67, 72, 72, 73, 72, 73, 73, 74,
68, 72, 72, 32, 72, 57, 75, 72, 72, 37, 49, 72, 40, 72, 72, 44,
69, 72, 72, 33, 72, 61,  4, 72, 72, 75, 53, 72, 45, 72, 72, 41,
72,  0,  1, 72, 10, 72, 72, 75, 15, 72, 72, 75, 72, 73, 73, 72,
70, 72, 72, 42, 72, 59, 39, 72, 72, 75, 51, 72, 34, 72, 72, 46,
72, 25, 29, 72, 27, 74, 72, 75, 31, 72, 74, 75, 72, 75, 75, 72,
72, 75, 36, 72,  7, 72, 72, 54, 75, 72, 72, 62, 72, 48, 56, 72,
73, 72, 72, 75, 72, 75, 22, 72, 72, 18, 75, 72, 73, 72, 72, 75,
71, 72, 72, 47, 72, 63, 75, 72, 72,  6, 55, 72, 35, 72, 72, 43,
72,  5, 75, 72, 75, 72, 72, 50, 38, 72, 72, 58, 72, 52, 60, 72,
72, 17, 21, 72, 19, 74, 72, 75, 23, 72, 74, 75, 72, 75, 75, 72,
73, 72, 72, 75, 72, 75, 30, 72, 72, 26, 75, 72, 73, 72, 72, 75,
72,  8, 13, 72,  2, 72, 72, 73,  3, 72, 72, 73, 72, 75, 75, 72,
73, 72, 72, 73, 72, 75, 16, 72, 72, 20, 75, 72, 75, 72, 72, 75,
73, 72, 72, 73, 72, 75, 24, 72, 72, 28, 75, 72, 75, 72, 72, 75,
74, 12,  9, 72, 14, 72, 72, 75, 11, 72, 72, 75, 72, 75, 75, 74,
};
#define	SYND_TBL_SIZE 256

#define	MAX_CE_ERROR	255
#define	UNUM_NAMLEN	60
#define	MAX_SIMM	256
#define	MAX_CE_FLTS	10
#define	MAX_UE_FLTS	5
#define	MAX_BTO_FLTS	2

struct  ce_info {
	char    name[UNUM_NAMLEN];
	short	intermittent_cnt;
	short	persistent_cnt;
};

struct ce_info  *mem_ce_simm = NULL;
int mem_ce_simm_size = 0;

short	max_ce_err = MAX_CE_ERROR;

int	report_ce_console = 0;	/* don't print messages on console */
int	report_ce_log = 0;
int	log_ce_error = 0;
int	ce_errors_disabled = 0;

struct	ecc_flt *ce_flt = NULL;	/* correctable errors in process */
int	ce_flt_size = 0;
int	nce = 0;
int	oce = 0;
u_int	ce_inum, ce_pil = PIL_1;

struct	ecc_flt *ue_flt = NULL;	/* uncorrectable errors in process */
int	ue_flt_size = 0;
int	nue = 0;
int	oue = 0;
u_int	ue_inum, ue_pil = PIL_2;

struct	bto_flt *to_flt = NULL;	/* bus/timeout errors in process */
int	to_flt_size = 0;
int	nto = 0;
int	oto = 0;
u_int	to_inum, to_pil = PIL_1;

int	ce_verbose = 0;
int	ce_show_data = 0;
int	ce_debug = 0;
int	ue_verbose = 1;
int	ue_show_data = 0;
int	ue_debug = 0;
int	reset_debug = 0;

#define	MAX_UPA_FUNCS	120 /* XXX - 30 max sysio/pci devices on sunfire */
struct upa_func  register_func[MAX_UPA_FUNCS];
int nfunc = 0;

/*
 * Allocate error arrays based on ncpus.
 */
void
error_init()
{
	char tmp_name[MAXSYSNAME];
	dnode_t node;
	register int size;
	extern int ncpus;

	if ((mem_ce_simm == NULL) && (ce_flt == NULL) && (ue_flt == NULL)) {
		ecc_error_init();
	}

	if (to_flt == NULL) {
		to_flt_size = MAX_BTO_FLTS * ncpus;
		size = ((sizeof (struct bto_flt)) * to_flt_size);
		to_flt = (struct bto_flt *)kmem_zalloc(size, KM_SLEEP);
		if (to_flt == NULL) {
			cmn_err(CE_PANIC,
				"No space for BTO error initialization");
		}
		to_inum = add_softintr(to_pil, handle_bto_error,
			(caddr_t)to_flt, 0);
	}

	node = prom_rootnode();
	if ((node == OBP_NONODE) || (node == OBP_BADNODE)) {
		cmn_err(CE_CONT, "error_init: node 0x%x\n", (u_int)node);
		return;
	}
	if (((size = prom_getproplen(node, "reset-reason")) != -1) &&
	    (size <= MAXSYSNAME) &&
	    (prom_getprop(node, "reset-reason", tmp_name) != -1)) {
		if (reset_debug) {
			cmn_err(CE_CONT,
			    "System booting after %s\n", tmp_name);
		} else if (strncmp(tmp_name, "FATAL", 5) == 0) {
			cmn_err(CE_CONT,
			    "System booting after fatal error %s\n", tmp_name);
		}
	}
}

/*
 * Allocate error arrays based on ncpus.
 */
static void
ecc_error_init(void)
{
	register int size;
	extern int ncpus;

	mem_ce_simm_size = MAX_SIMM * ncpus;
	size = ((sizeof (struct ce_info)) * mem_ce_simm_size);
	mem_ce_simm = (struct ce_info *)kmem_zalloc(size, KM_SLEEP);
	if (mem_ce_simm == NULL) {
		cmn_err(CE_PANIC, "No space for CE unum initialization");
	}

	ce_flt_size = MAX_CE_FLTS * ncpus;
	size = ((sizeof (struct ecc_flt)) * ce_flt_size);
	ce_flt = (struct ecc_flt *)kmem_zalloc(size, KM_SLEEP);
	if (ce_flt == NULL) {
		cmn_err(CE_PANIC, "No space for CE error initialization");
	}
	ce_inum = add_softintr(ce_pil, handle_ce_error, (caddr_t)ce_flt, 0);

	ue_flt_size = MAX_UE_FLTS * ncpus;
	size = ((sizeof (struct ecc_flt)) * ue_flt_size);
	ue_flt = (struct ecc_flt *)kmem_zalloc(size, KM_SLEEP);
	if (ue_flt == NULL) {
		cmn_err(CE_PANIC, "No space for UE error initialization");
	}
	ue_inum = add_softintr(ue_pil, handle_ue_error, (caddr_t)ue_flt, 0);
}

/*
 * can be called from setup_panic at pil > XCALL_PIL, so use xt_all
 */
void
error_disable()
{
	register int n, nf;
	caddr_t arg;
	volatile u_longlong_t neer = 0;
	afunc errdis_func;

	xt_all((u_int)&set_error_enable_tl1, (u_int)&neer,
		(u_int)0, (u_int)0, (u_int)0);
	nf = nfunc;
	for (n = 0; n < nf; n++) {
		if (register_func[n].ftype != DIS_ERR_FTYPE)
			continue;
		errdis_func = register_func[n].func;
		ASSERT(errdis_func != NULL);
		arg = register_func[n].farg;
		(void) (*errdis_func)(arg);
	}
}

int
bto_error(u_short inst, proc_t *up, int pc)
{
	register int tn;

	if (to_flt == NULL) {		/* ring buffer not initialized */
		cmn_err(CE_PANIC,
			"BTO Error init: CPU %d curthread 0x%x pc 0x%x",
			inst, up, pc);
		/* NOTREACHED */
	}

	tn = atinc_cidx_word(&nto, to_flt_size-1);
	if (to_flt[tn].flt_in_proc == 1) {	/* ring buffer wrapped */
		cmn_err(CE_PANIC,
			"BTO Error space: CPU %d curthread 0x%x pc 0x%x",
			inst, up, pc);
		/* NOTREACHED */
	}

	to_flt[tn].flt_in_proc = 1;
	to_flt[tn].flt_proc = up;
	to_flt[tn].flt_pc = (caddr_t)pc;
	setsoftint(to_inum);
	return (0);
}

static u_int
handle_bto_error(struct bto_flt *pto)
{
	struct bto_flt bto;
	register int to;

	to = atinc_cidx_word(&oto, to_flt_size-1);
	if (pto[to].flt_in_proc != 1) { /* Ring buffer lost in space */
		cmn_err(CE_PANIC, "Bus/timeout error queue out of sync");
		/* NOTREACHED */
	}
	bto.flt_proc = pto[to].flt_proc;
	bto.flt_pc = pto[to].flt_pc;
	pto[oto].flt_in_proc = 0;
	kill_proc(bto.flt_proc, bto.flt_pc);
	return (1);
}

static void
kill_proc(proc_t *up, caddr_t addr)
{
	kthread_t *t;
	k_siginfo_t siginfo;

	t = proctot(up);
	bzero((caddr_t)&siginfo, sizeof (siginfo));
	siginfo.si_signo = SIGBUS;
	siginfo.si_code = FC_HWERR;
	siginfo.si_addr = addr;
	mutex_enter(&up->p_lock);
	sigaddq(up, t, &siginfo, KM_NOSLEEP);
	mutex_exit(&up->p_lock);
}

int
ue_error(u_longlong_t *afsr, u_longlong_t *afar, u_char ecc_synd,
	u_char size, u_char offset, u_short id, u_short inst, afunc log_func)
{
	register int tnue;
	union ull {
		u_longlong_t	afsr;
		u_longlong_t	afar;
		u_int		i[2];
	} j, k;

	j.afsr = *afsr;
	k.afar = *afar;
	if (ue_flt == NULL) {		/* ring buffer not initialized */
		cmn_err(CE_PANIC, "UE Error init: AFSR 0x%08x %08x "
			"AFAR 0x%08x %08x Synd 0x%x Id %d Inst %d",
			j.i[0], j.i[1], k.i[0], k.i[1], ecc_synd, id, inst);
		/* NOTREACHED */
	}

	tnue = atinc_cidx_word(&nue, ue_flt_size-1);
	if (ue_flt[tnue].flt_in_proc == 1) {	/* ring buffer wrapped */
		cmn_err(CE_PANIC, "UE Error space: AFSR 0x%08x %08x "
			"AFAR 0x%08x %08x Synd 0x%x Id %d Inst %d",
			j.i[0], j.i[1], k.i[0], k.i[1], ecc_synd, id, inst);
		/* NOTREACHED */
	}

	ue_flt[tnue].flt_in_proc = 1;
	ue_flt[tnue].flt_stat = *afsr;
	ue_flt[tnue].flt_addr = *afar;
	ue_flt[tnue].flt_synd = ecc_synd;
	ue_flt[tnue].flt_size = size;
	ue_flt[tnue].flt_offset = offset;
	ue_flt[tnue].flt_upa_id = id;
	ue_flt[tnue].flt_inst = inst;
	ue_flt[tnue].flt_func = log_func;
	setsoftint(ue_inum);
	return (0);
}

static u_int
handle_ue_error(struct ecc_flt *pue)
{
	static char buf[UNUM_NAMLEN];
	char *unum = &buf[0];
	struct ecc_flt ecc;
	struct ecc_flt *pecc = &ecc;
	register u_int fatal = 0;
	register int toue;
	union ull {
		u_longlong_t	afsr;
		u_longlong_t	afar;
		u_int		i[2];
	} j, k;

	toue = atinc_cidx_word(&oue, ue_flt_size-1);
	if (pue[toue].flt_in_proc != 1) { /* Ring buffer lost in space */
		cmn_err(CE_PANIC, "Harderror queue out of sync");
		/* NOTREACHED */
	}
	ecc.flt_stat = pue[toue].flt_stat;
	ecc.flt_addr = pue[toue].flt_addr;
	ecc.flt_synd = pue[toue].flt_synd;
	ecc.flt_size = pue[toue].flt_size;
	ecc.flt_offset = pue[toue].flt_offset;
	ecc.flt_upa_id = pue[toue].flt_upa_id;
	ecc.flt_inst = pue[toue].flt_inst;
	ecc.flt_func = pue[toue].flt_func;
	pue[oue].flt_in_proc = 0;

	fatal = ue_log_mem_err(pecc, unum);
	j.afsr = ecc.flt_stat;
	k.afar = ecc.flt_addr;
	switch (fatal) {
	case UE_FATAL:
		(void) ue_reset_ecc(&k.afar);
		cmn_err(CE_PANIC, "Harderror: AFSR 0x%08x %08x "
			"AFAR 0x%08x %08x Id %d Inst %d SIMM %s",
			j.i[0], j.i[1], k.i[0], k.i[1],
			ecc.flt_upa_id, ecc.flt_inst, unum);
		break;
	/*
	 * Note: do not reset the ecc (ie, write zeros to the cache line)
	 * if this code gets changed to not
	 * panic for user uncorrectable ecc errors.
	 */
	case UE_USER_FATAL:
		(void) ue_reset_ecc(&k.afar);
		cmn_err(CE_PANIC, "User Harderror: AFSR 0x%08x %08x "
			"AFAR 0x%08x %08x Id %d Inst %d SIMM %s",
			j.i[0], j.i[1], k.i[0], k.i[1],
			ecc.flt_upa_id, ecc.flt_inst, unum);
		break;
	case UE_DEBUG:		/* XXX - hack alert for sysio */
	default:
		break;
	}
	return (1);
}

static int
ue_log_mem_err(struct ecc_flt *ecc, char *unum)
{
	struct ecc_flt cecc;
	struct ecc_flt *pcecc = &cecc;
	int pix, len = 0, fatal = 0;
	union ull {
		u_longlong_t	afsr;
		u_longlong_t	afar;
		u_int		i[2];
	} j, k;
	afunc log_func;

	k.afar = ecc->flt_addr;
	k.i[1] &= 0xFFFFFFF8;	/* byte alignment for get-unumber */
	(void) prom_get_unum(-1, k.afar, unum, UNUM_NAMLEN, &len);
	if (len <= 1)
		(void) sprintf(unum, "%s", "Decoding Failed");

	/*
	 * Check for SDB copyout on other cpu(s).
	 * Don't bother to change cpu_get_status to a xt/tl1 function for now,
	 * as we know that we are calling this function at ue_pil = PIL_2;
	 * Check for Sysio DVMA and PIO parity errors.
	 */
	for (pix = 0; pix < NCPU; pix++) {
		if ((pix != CPU->cpu_id) && CPU_XCALL_READY(pix)) {
			xc_one(pix, cpu_get_status, (u_int)pcecc, 0);
			j.afsr = pcecc->flt_stat;
			k.afar = pcecc->flt_addr;
			if (j.afsr & P_AFSR_CP) {
				(void) ue_reset_ecc(&k.afar);
				cmn_err(CE_PANIC,
				    "CPU%d UE Error: Ecache Copyout on CPU%d: "
				    "AFSR 0x%08x %08x AFAR 0x%08x %08x",
				    CPU->cpu_id, pcecc->flt_inst, j.i[0],
				    j.i[1], k.i[0], k.i[1]);
				/* NOTREACHED */
			}
		}
	}
	ue_check_upa_func();

	/*
	 * Call specific error logging routine.
	 */
	log_func = ecc->flt_func;
	if (log_func != NULL) {
		fatal += (*log_func)(ecc, unum);
	}
	return (fatal);
}

static int
ue_check_upa_func(void)
{
	afunc ue_func;
	caddr_t arg;

	register int n, nf;
	int fatal = 0;

	nf = nfunc;
	for (n = 0; n < nf; n++) {
		if (register_func[n].ftype != UE_ECC_FTYPE)
			continue;
		ue_func = register_func[n].func;
		ASSERT(ue_func != NULL);
		arg = register_func[n].farg;
		fatal = (*ue_func)(arg);
	}
	return (fatal);
}

static int
ue_reset_ecc(u_longlong_t *flt_addr)
{
	volatile u_longlong_t neer;
	caddr_t vaddr;
	union ul {
		u_longlong_t	afsr;
		u_longlong_t	afar;
		u_longlong_t	aligned_addr;
		u_int		i[2];
	} al, j, k;
	extern int fpu_exists;

	/*
	 * XXX - cannot do block commit instructions w/out fpu regs,
	 *	 we may not emulate this unless we have a reasonable
	 *	 way to flush the cache(s)
	 */
	if (fpu_exists == 0)
		return (-1);
	/*
	 * 64 byte alignment for block load/store operations
	 */
	al.aligned_addr = *flt_addr;
	al.i[1] &= 0xFFFFFFC0;
	vaddr = map_paddr_to_vaddr(al.aligned_addr);
	if (vaddr == NULL)
		return (1);

	/*
	 * disable ECC errors, flush the cache(s)
	 */
	xc_attention(cpu_ready_set);

	neer = EER_ISAPEN;
	xt_all((u_int)&set_error_enable_tl1, (u_int)&neer, (u_int)0,
			(u_int)0, (u_int)0);
	scrub_ecc();

	reset_ecc(vaddr);

	/*
	 * clear any ECC errors
	 */
	get_asyncflt(&j.afsr);
	get_asyncaddr(&k.afar);
	if ((j.afsr & P_AFSR_UE) || (j.afsr & P_AFSR_CE)) {
		clr_datapath();
		if (ue_debug)
			cmn_err(CE_CONT,
			"\tue_reset_ecc: AFSR 0x%08x %08x AFAR 0x%08x %08x\n",
				j.i[0], j.i[1], k.i[0], k.i[1]);
		set_asyncflt(&j.afsr);
	}
	xc_dismissed(cpu_ready_set);
	unmap_vaddr(al.aligned_addr, vaddr);
	return (0);
}

int
ce_error(u_longlong_t *afsr, u_longlong_t *afar, u_char ecc_synd,
	u_char size, u_char offset, u_short id, u_short inst, afunc log_func)
{
	register int tnce, nnce;
	u_char flt_in_proc = 1;

	ASSERT(ce_flt != NULL);
	tnce = atinc_cidx_word(&nce, ce_flt_size-1);
	if (ce_flt[tnce].flt_in_proc == 1) {	/* ring buffer wrapped */
		if (ce_errors_disabled)		/* normal */
			return (0);
		else				/* abnormal */
			flt_in_proc = 3;
	}

	/*
	 * Check 2 places away in the ring buffer, and turn off
	 * correctable errors if we are about to fill up our ring buffer.
	 */
	if ((nnce = tnce + 2) > ce_flt_size)
		nnce = 0;
	if (ce_flt[nnce].flt_in_proc == 1) {
		ce_errors_disabled = 1;
		flt_in_proc = 2;
	}

	ce_flt[tnce].flt_in_proc = flt_in_proc;
	ce_flt[tnce].flt_stat = *afsr;
	ce_flt[tnce].flt_addr = *afar;
	ce_flt[tnce].flt_synd = ecc_synd;
	ce_flt[tnce].flt_size = size;
	ce_flt[tnce].flt_offset = offset;
	ce_flt[tnce].flt_upa_id = id;
	ce_flt[tnce].flt_inst = inst;
	ce_flt[tnce].flt_func = log_func;
	setsoftint(ce_inum);
	return (0);
}

static u_int
handle_ce_error(struct ecc_flt *pce)
{
	struct ecc_flt ecc;
	struct ecc_flt *pecc = &ecc;
	register int toce;
	volatile u_longlong_t neer;

	toce = atinc_cidx_word(&oce, ce_flt_size-1);
	if (pce[toce].flt_in_proc == 2) {	/* Ring buffer almost full */
		neer = (EER_ISAPEN | EER_NCEEN);
		xt_all((u_int)&set_error_enable_tl1, (u_int)&neer, (u_int)0,
			(u_int)0, (u_int)0);
		if (ce_verbose || ce_debug)
			cmn_err(CE_CONT, "Disabled softerrors\n");
	} else if (pce[toce].flt_in_proc == 3) { /* Ring buffer wrapped */
		cmn_err(CE_CONT, "CE Error queue wrapped\n");
		return (1);
	} else if (pce[toce].flt_in_proc != 1) { /* Ring buffer lost in space */
		cmn_err(CE_CONT, "CE Error queue out of sync\n");
		return (1);
	}
	ecc.flt_stat = pce[toce].flt_stat;
	ecc.flt_addr = pce[toce].flt_addr;
	ecc.flt_synd = pce[toce].flt_synd;
	ecc.flt_size = pce[toce].flt_size;
	ecc.flt_offset = pce[toce].flt_offset;
	ecc.flt_upa_id = pce[toce].flt_upa_id;
	ecc.flt_inst = pce[toce].flt_inst;
	ecc.flt_func = pce[toce].flt_func;
	pce[toce].flt_in_proc = 0;

	if (report_ce_log || report_ce_console)
		log_ce_error = 1;
	if (ce_log_mem_err(pecc)) {	/* only try to scrub memory errors */
		ce_scrub_mem_err(pecc);
	}
	log_ce_error = 0;

	/* we just freed up a space, so turn errors back on */
	if (ce_errors_disabled) {
		neer = (EER_ISAPEN | EER_NCEEN | EER_CEEN);
		xt_all((u_int)&set_error_enable_tl1, (u_int)&neer, (u_int)0,
			(u_int)0, (u_int)0);
		ce_errors_disabled = 0;
	}
	return (1);
}

#ifdef DEBUG
int test_ce_scrub = 0;
#endif

static int
ce_log_mem_err(struct ecc_flt *ecc)
{
	short syn_code, found_unum = 0;
	static char buf[UNUM_NAMLEN];
	char *unum = &buf[0];
	int len = 0, offset = 0;
	int persistent = 0, memory_error = 1;
	short loop, ce_err = 1;
	union ull {
		u_longlong_t	afar;
		u_longlong_t	afsr;
		u_longlong_t	aligned_addr;
		u_int		i[2];
	} j, k, al;
	afunc log_func;

	j.afsr = ecc->flt_stat;
	k.afar = ecc->flt_addr;
	/*
	 * Use the 8-bit syndrome to index the ecc_syndrome_tab to get
	 * the code indicating which bit(s) is(are) bad.
	 */
	if ((ecc->flt_synd <= 0) || (ecc->flt_synd >= SYND_TBL_SIZE)) {
		cmn_err(CE_CONT, "CE Error: AFSR 0x%08x %08x AFAR 0x%08x %08x "
			"Bad Syndrome 0x%x Id %d Inst %d\n",
			j.i[0], j.i[1], k.i[0], k.i[1],
			ecc->flt_synd, ecc->flt_upa_id, ecc->flt_inst);
		syn_code = 0;
	} else {
		syn_code = ecc_syndrome_tab[ecc->flt_synd];
	}

	/*
	 * Size of CPU transfer is 3 and offset 0 (ie, 8 byte aligned), may be
	 * larger for SYSIO, etc. byte alignment required for get-unumber.
	 */
	if (ecc->flt_size > 3)
		offset = ecc->flt_offset * 8;

	if (syn_code < 72) {
		if (syn_code < 64)
			offset = offset + (7 - syn_code / 8);
		else
			offset = offset + (7 - syn_code % 8);
		al.aligned_addr = k.afar + offset;
		al.i[1] &= 0xFFFFFFF8;
		(void) prom_get_unum((int)syn_code, al.aligned_addr, unum,
					UNUM_NAMLEN, &len);
		if (len > 1) {
			found_unum = 1;
		} else {
			(void) sprintf(unum, "%s", "Decoding Failed");
		}
	} else if (syn_code < 76) {
		al.aligned_addr = k.afar;
		al.i[1] &= 0xFFFFFFF8;
		(void) prom_get_unum(-1, al.aligned_addr, unum,
					UNUM_NAMLEN, &len);
		if (len <= 1)
			(void) sprintf(unum, "%s", "Decoding Failed");
		cmn_err(CE_PANIC, "CE/UE Error: AFSR 0x%08x%08x "
			"AFAR 0x%08x%08x Synd 0x%x Id %d Inst %d SIMM %s",
			j.i[0], j.i[1], k.i[0], k.i[1],
			ecc->flt_synd, ecc->flt_upa_id, ecc->flt_inst, unum);
	}

	/*
	 * Call specific error logging routine.
	 * Note that if we want to save information about non-memory errors,
	 * we need to find another way, not related to the unum, of saving
	 * this info, because otherwise we lose all the pertinent related info.
	 * If the specific error logging routine says it's possibly a
	 * memory error, then check if the error is persistent.
	 * We will get a tiny number of not-really-intermittent memory
	 * errors from bus and/or uncorrectable error overwrites.
	 */
	log_func = ecc->flt_func;
	if (log_func != NULL) {
		memory_error += (*log_func)(ecc, unum);
		if (memory_error) {
			if (ecc->flt_addr != 0) {
				loop = 1;
				al.aligned_addr = k.afar;
				if (ecc->flt_size == 3) {
					al.i[1] &= 0xFFFFFFF8;
				} else {
					al.i[1] &= 0xFFFFFFF0;
					al.aligned_addr += ecc->flt_offset * 8;
				}
				persistent = read_ecc_data(al.aligned_addr,
							loop, ce_err, 0);
			}
		} else {
			cmn_err(CE_CONT,
				"Non-memory-related Correctable ECC Error.\n");
		}
	}

	/*
	 * Do not bother to log non-memory CE errors... the relevant
	 * CE memory error logging routine should be verbose about these
	 * errors.
	 */
	if (memory_error)
		ce_log_unum(found_unum, persistent, len, unum, syn_code);
	if ((!(memory_error)) || (log_ce_error) || (ce_verbose))
		ce_log_syn_code(syn_code);

	/* Display entire cache line */
	if ((ce_show_data) && (ecc->flt_addr != 0)) {
		loop = 8;
		al.aligned_addr = k.afar;
		al.i[1] &= 0xFFFFFFF0;
		(void) read_ecc_data(al.aligned_addr, loop, ce_err, 1);
	}

#ifdef DEBUG
	if (test_ce_scrub)
		persistent = 1;
#endif
	return (persistent);
}

static void
ce_log_unum(int found_unum, int persistent,
		int len, char *unum, short syn_code)
{
	register int i;
	struct  ce_info *psimm = mem_ce_simm;

	ASSERT(psimm != NULL);
	if (found_unum) {
	    for (i = 0; i < mem_ce_simm_size; i++) {
		if (psimm[i].name[0] == NULL) {
			(void) strncpy(psimm[i].name, unum, len);
			if (persistent) {
				psimm[i].persistent_cnt = 1;
				psimm[i].intermittent_cnt = 0;
			} else {
				psimm[i].persistent_cnt = 0;
				psimm[i].intermittent_cnt = 1;
			}
			break;
		} else if (strncmp(unum, psimm[i].name, len) == 0) {
			if (persistent)
				psimm[i].persistent_cnt += 1;
			else
				psimm[i].intermittent_cnt += 1;
			if ((psimm[i].persistent_cnt +
			    psimm[i].intermittent_cnt) > max_ce_err) {
				cmn_err(CE_CONT,
					"Multiple Softerrors: ");
				cmn_err(CE_CONT,
			"Seen %d Intermittent and %d Corrected Softerrors ",
					psimm[i].intermittent_cnt,
					psimm[i].persistent_cnt);
				cmn_err(CE_CONT, "from SIMM %s\n", unum);
				cmn_err(CE_CONT,
					"\tCONSIDER REPLACING THE SIMM.\n");
				psimm[i].persistent_cnt = 0;
				psimm[i].intermittent_cnt = 0;
				log_ce_error = 1;
			}
			break;
		}
	    }
	    if (i >= mem_ce_simm_size)
		cmn_err(CE_CONT, "Softerror: mem_ce_simm[] out of space.\n");
	}

	if (log_ce_error) {
		if (persistent) {
			cmn_err(CE_CONT,
				"Softerror: Persistent ECC Memory Error");
			if (unum != "?") {
				if (syn_code < 72)
					cmn_err(CE_CONT,
						" Corrected SIMM %s\n", unum);
				else
					cmn_err(CE_CONT,
					" Possible Corrected SIMM %s\n",
						unum);
			} else {
				cmn_err(CE_CONT, "\n");
			}
		} else {
			cmn_err(CE_CONT,
			"Softerror: Intermittent ECC Memory Error SIMM %s\n",
				unum);
		}
	}
}


static void
ce_log_syn_code(short syn_code)
{
	if (syn_code < 64) {
		cmn_err(CE_CONT, "\tECC Data Bit %2d was corrected", syn_code);
	} else if (syn_code < 72) {
		cmn_err(CE_CONT, "\tECC Check Bit %2d was corrected",
			syn_code - 64);
	} else {
		switch (syn_code) {
		case 72:
		    cmn_err(CE_CONT, "\tTwo ECC Bits were corrected");
		    break;
		case 73:
		    cmn_err(CE_CONT, "\tThree ECC Bits were corrected");
		    break;
		case 74:
		    cmn_err(CE_CONT, "\tFour ECC Bits were corrected");
		    break;
		case 75:
		    cmn_err(CE_CONT, "\tMore than Four ECC Bits ");
		    cmn_err(CE_CONT, "were corrected");
		    break;
		default:
		    break;
		}
	}
	cmn_err(CE_CONT, "\n");
}

static void
ce_scrub_mem_err(struct ecc_flt *ecc)
{
	volatile u_longlong_t neer;
	caddr_t vaddr;
	union ul {
		u_longlong_t	afsr;
		u_longlong_t	afar;
		u_longlong_t	aligned_addr;
		u_int		i[2];
	} al, j, k;
	extern int fpu_exists;

	/*
	 * XXX - cannot do block commit instructions w/out fpu regs,
	 *	 we may not emulate this unless we have a reasonable
	 *	 way to flush the cache(s)
	 */
	if (fpu_exists == 0)
		return;

	/*
	 * 64 byte alignment for block load/store operations
	 * try to map paddr before called xc_attention...
	 */
	al.aligned_addr = ecc->flt_addr;
	al.i[1] &= 0xFFFFFFC0;
	vaddr = map_paddr_to_vaddr(al.aligned_addr);
	if (vaddr == NULL)
		return;

	/*
	 * disable ECC errors, flush and reenable the cache(s), then
	 * scrub memory, then check afsr for errors, and reenable errors
	 * XXX - For Spitfire, just flush the whole ecache because it's
	 *	 only 2 pages, change for a different implementation.
	 *	 disable ECC errors, flush cache(s)
	 */

	xc_attention(cpu_ready_set);

	neer = EER_ISAPEN;
	xt_all((u_int)&set_error_enable_tl1, (u_int)&neer, (u_int)0,
			(u_int)0, (u_int)0);
	scrub_ecc();

	scrubphys(vaddr);

	/*
	 * clear any ECC errors
	 */
	get_asyncflt(&j.afsr);
	get_asyncaddr(&k.afar);
	if ((j.afsr & P_AFSR_UE) || (j.afsr & P_AFSR_CE)) {
		clr_datapath();
		if (ce_debug)
			cmn_err(CE_CONT,
		"\tce_scrub_mem_err: AFSR 0x%08x %08x AFAR 0x%08x %08x\n",
					j.i[0], j.i[1], k.i[0], k.i[1]);
		set_asyncflt(&j.afsr);
	}
	/*
	 * enable ECC errors, unmap vaddr
	 */
	neer |= (EER_NCEEN | EER_CEEN);
	xt_all((u_int)&set_error_enable_tl1, (u_int)&neer, (u_int)0,
			(u_int)0, (u_int)0);

	xc_dismissed(cpu_ready_set);
	unmap_vaddr(al.aligned_addr, vaddr);
}

caddr_t
map_paddr_to_vaddr(u_longlong_t aligned_addr)
{
	union ul {
		u_longlong_t	aligned_addr;
		u_int		i[2];
	} al;
	u_long a;
	struct page *pp;
	caddr_t cvaddr, vaddr;
	u_int pfn, pagenum, pgoffset, len = 1;
	extern int pf_is_memory(uint);

	al.aligned_addr = aligned_addr;
	pagenum = (u_int)(al.aligned_addr >> MMU_PAGESHIFT);
	pp = page_numtopp(pagenum, SE_SHARED);
	if (pp == NULL) {
		if ((ce_debug) || (ue_debug))
			cmn_err(CE_CONT,
		"\tmap_paddr_to_vaddr: aligned_addr 0x%08x%08x, pagenum 0x%x\n",
			al.i[0], al.i[1], pagenum);
		return (NULL);
	}
	pfn = ((machpage_t *)pp)->p_pagenum;
	if (pf_is_memory(pfn)) {
		a = rmalloc(kernelmap, len);
		if (a == NULL) {
			if ((ce_debug) || (ue_debug))
				cmn_err(CE_CONT,
		"\tmap_paddr_to_vaddr: aligned_addr 0x%08x%08x, len 0x%x\n",
				al.i[0], al.i[1], len);
			return (NULL);
		}
		cvaddr = (caddr_t)kmxtob(a);
		segkmem_mapin(&kvseg, cvaddr, (u_int)mmu_ptob(len),
			(PROT_READ | PROT_WRITE), pfn, HAT_LOAD_NOCONSIST);
		pgoffset = (u_int)(al.aligned_addr & MMU_PAGEOFFSET);
		vaddr = (caddr_t)(cvaddr + pgoffset);
		return (vaddr);
	} else {
		if ((ce_debug) || (ue_debug))
			cmn_err(CE_CONT,
	"\tmap_paddr_to_vaddr: aligned_addr 0x%08x%08x, pp 0x%x, pfn 0x%x\n",
			al.i[0], al.i[1], pp, pfn);
		page_unlock(pp);
		return (NULL);
	}
}

void
unmap_vaddr(u_longlong_t aligned_addr, caddr_t vaddr)
{
	caddr_t a, cvaddr;
	u_int pagenum, pgoffset, len = 1;
	struct page *pp;
	extern struct seg kvseg;

	pagenum = (u_int)(aligned_addr >> MMU_PAGESHIFT);
	pp = page_numtopp_nolock(pagenum);
	page_unlock(pp);

	pgoffset = (u_int)(aligned_addr & MMU_PAGEOFFSET);
	cvaddr = (caddr_t)(vaddr - pgoffset);
	segkmem_mapout(&kvseg, cvaddr, (u_int)mmu_ptob(len));
	a = (caddr_t)(btokmx(cvaddr));
	rmfree(kernelmap, len, (u_long)a);
}

int
read_ecc_data(u_longlong_t aligned_addr, short loop,
		short ce_err, short verbose)
{
	union {
		volatile u_longlong_t	afsr;
		volatile u_longlong_t	afar;
		volatile u_longlong_t	paddr;
		u_int			i[2];
	} j, k, pa;
	register short i;
	int persist = 0;
	int pix = CPU->cpu_id;
	u_int ecc_0;
	volatile u_longlong_t neer;
	union {
		u_longlong_t data;
		u_int i[2];
	} d;

	/*
	 * disable ECC errors, read the data
	 */
	neer = EER_ISAPEN;
	xt_one(pix, (u_int)&set_error_enable_tl1, (u_int)&neer,
		(u_int)0, (u_int)0, (u_int)0);

	for (i = 0; i < loop; i++) {
		pa.paddr = aligned_addr + (i * 8);
		d.data = lddphys(pa.paddr);
		if (verbose) {
			if (ce_err) {
				ecc_0 = ecc_gen(d.i[0], d.i[1]);
				cmn_err(CE_CONT,
			"\tPaddr 0x%08x %08x, Data 0x%08x %08x, ECC 0x%x\n",
				pa.i[0], pa.i[1], d.i[0], d.i[1], ecc_0);
			} else {
				cmn_err(CE_CONT,
				"\tPaddr 0x%08x %08x, Data 0x%08x %08x\n",
				pa.i[0], pa.i[1], d.i[0], d.i[1]);
			}
		}
	}
	get_asyncflt(&j.afsr);
	get_asyncaddr(&k.afar);
	if ((j.afsr & P_AFSR_UE) || (j.afsr & P_AFSR_CE)) {
		if ((ce_debug) || (ue_debug))
			cmn_err(CE_CONT,
			"\tread_ecc_data: AFSR 0x%08x %08x AFAR 0x%08x %08x\n",
			j.i[0], j.i[1], k.i[0], k.i[1]);
		clr_datapath();
		set_asyncflt(&j.afsr);
		persist = 1;
	}
	neer |= (EER_NCEEN | EER_CEEN);
	xt_one(pix, (u_int)&set_error_enable_tl1, (u_int)&neer,
		(u_int)0, (u_int)0, (u_int)0);

	return (persist);
}

struct {		/* sec-ded-s4ed ecc code */
	unsigned long hi, lo;
} ecc_code[8] = {
	0xee55de23U, 0x16161161,
	0x55eede93, 0x61612212,
	0xbb557b8cU, 0x49494494,
	0x55bb7b6c, 0x94948848U,
	0x16161161, 0xee55de23U,
	0x61612212, 0x55eede93,
	0x49494494, 0xbb557b8cU,
	0x94948848U, 0x55bb7b6c,
};

int
ecc_gen(int high_bytes, int low_bytes)
{
	int i, j;
	u_char checker, bit_mask;
	struct {
		unsigned long hi, lo;
	} hex_data, masked_data[8];

	hex_data.hi = high_bytes;
	hex_data.lo = low_bytes;

	/* mask out bits according to sec-ded-s4ed ecc code */
	for (i = 0; i < 8; i++) {
		masked_data[i].hi = hex_data.hi & ecc_code[i].hi;
		masked_data[i].lo = hex_data.lo & ecc_code[i].lo;
	}

	/*
	 * xor all bits in masked_data[i] to get bit_i of checker,
	 * where i = 0 to 7
	 */
	checker = 0;
	for (i = 0; i < 8; i++) {
		bit_mask = 1 << i;
		for (j = 0; j < 32; j++) {
			if (masked_data[i].lo & 1) checker ^= bit_mask;
			if (masked_data[i].hi & 1) checker ^= bit_mask;
			masked_data[i].hi >>= 1;
			masked_data[i].lo >>= 1;
		}
	}
	return (checker);
}

void
register_upa_func(short type, afunc func, caddr_t arg)
{
	register int n;

	n = atinc_cidx_word(&nfunc, MAX_UPA_FUNCS-1);
	register_func[n].ftype = type;
	register_func[n].func = func;
	register_func[n].farg = arg;
}
