/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */

#ifndef _SYS_FPU_FPUSYSTM_H
#define	_SYS_FPU_FPUSYSTM_H

#pragma ident	"@(#)fpusystm.h	1.7	94/11/18 SMI"

/*
 * ISA-dependent FPU interfaces
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _KERNEL

struct fpu;
struct regs;
struct core;

extern int fpu_exists;
extern int fpu_version;

void fpu_probe(void);
void fpu_hw_init(void);
void fp_save(struct fpu *);
void fp_restore(struct fpu *);
void fp_free(struct fpu *);
int fp_assist_fault(struct regs *);
int no_fpu_fault(struct regs *);
int fpu_en_fault(struct regs *);
void fpu_save(struct fpu *);
void fpu_restore(struct fpu *);
void fp_fork(kthread_id_t, kthread_id_t);
u_int fperr_reset(void);

#endif	/* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_FPU_FPUSYSTM_H */
