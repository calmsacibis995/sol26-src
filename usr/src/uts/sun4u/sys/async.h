/*
 * Copyright (c) 1991, by Sun Microsystems, Inc.
 */

#ifndef	_SYS_ASYNC_H
#define	_SYS_ASYNC_H

#pragma ident	"@(#)async.h	1.14	96/09/09 SMI"

#include <sys/privregs.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef	_ASM

struct proc;				/* forward, in lieu of proc.h */

typedef	u_int	(*afunc)();

struct ecc_flt {
	u_longlong_t	flt_stat;	/* async. fault stat. reg. */
	u_longlong_t	flt_addr;	/* async. fault addr. reg. */
	u_char		flt_in_proc;	/* fault being handled */
	u_char		flt_synd;	/* ECC syndrome (CE only) */
	u_char		flt_size;	/* size of failed transfer */
	u_char		flt_offset;	/* offset of fault failed transfer */
	u_short		flt_upa_id;	/* upa id# of cpu/sysio/pci */
	u_short		flt_inst;	/* instance of cpu/sysio/pci */
	afunc		flt_func;	/* logging func for fault */
};

struct bto_flt {
	struct proc 	*flt_proc;	/* curthread */
	caddr_t		flt_pc;		/* pc where curthread got bto error */
	u_char		flt_in_proc;	/* fault being handled */
};

struct  upa_func {
	u_short ftype;			/* function type */
	afunc 	func;			/* function to run */
	caddr_t	farg;			/* argument (pointer to soft state) */
};

extern void error_init(void);
extern void error_disable(void);
extern void register_upa_func(short type, afunc func, caddr_t arg);
extern int read_ecc_data(u_longlong_t aligned_addr, short loop,
		short ce_err, short verbose);

extern int ce_error(u_longlong_t *afsr, u_longlong_t *afar, u_char ecc_synd,
	u_char size, u_char offset, u_short id, u_short inst, afunc log_func);
extern int ue_error(u_longlong_t *afsr, u_longlong_t *afar, u_char ecc_synd,
	u_char size, u_char offset, u_short id, u_short inst, afunc log_func);
extern int bto_error(u_short inst, struct proc *up, int pc);

#endif	/* !_ASM */

/*
 * Uncorrectable error logging return values.
 */
#define	UE_USER_FATAL	0x0		/* NonPriv. UnCorrectable ECC Error */
#define	UE_FATAL	0x1		/* Priv. UnCorrectable ECC Error */
#define	UE_DEBUG	0x2		/* Debugging loophole */

/*
 * Types of error functions (for register_upa_func type field)
 */
#define	UE_ECC_FTYPE	0x0001		/* UnCorrectable ECC Error */
#define	DIS_ERR_FTYPE	0x0004		/* Disable errors */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ASYNC_H */
