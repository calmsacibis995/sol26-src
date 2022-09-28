/*
 * Copyright (c) 1993, by Sun Microsystems, Inc.
 */

#ifndef _SYS_ARCHSYSTM_H
#define	_SYS_ARCHSYSTM_H

#pragma ident	"@(#)archsystm.h	1.10	96/06/20 SMI"

/*
 * A selection of ISA-dependent interfaces
 */

#include <sys/regset.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _KERNEL

extern greg_t getfp(void);
extern greg_t getpil(void);

extern void loadldt(int);
extern int cr0(void);
extern void setcr0(int);
extern int cr2(void);
extern int dr6(void);
extern void setdr6(int);
extern int dr7(void);

extern void sti(void);

extern void tenmicrosec(void);
void spinwait(int millis);

extern void restore_int_flag(int);
extern int clear_int_flag(void);

extern void int20(void);

extern unsigned char inb(int port);
extern unsigned short inw(int port);
extern unsigned long inl(int port);
extern void repinsb(int port, unsigned char *addr, int count);
extern void repinsw(int port, unsigned short *addr, int count);
extern void repinsd(int port, unsigned long *addr, int count);
extern void outb(int port, unsigned char value);
extern void outw(int port, unsigned short value);
extern void outl(int port, unsigned long value);
extern void repoutsb(int port, unsigned char *addr, int count);
extern void repoutsw(int port, unsigned short *addr, int count);
extern void repoutsd(int port, unsigned long *addr, int count);

extern void pc_reset(void);
extern void reset(void);
extern int goany(void);

extern void setgregs(klwp_id_t, gregset_t);
extern void setfpregs(klwp_id_t, fpregset_t *);
extern void getfpregs(klwp_t *, fpregset_t *);

extern void realsigprof(int, int);

extern int enable_cbcp; /* patchable in /etc/system */

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_ARCHSYSTM_H */
