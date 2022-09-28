/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */

#ifndef _SYS_ARCHSYSTM_H
#define	_SYS_ARCHSYSTM_H

#pragma ident	"@(#)archsystm.h	1.11	96/06/18 SMI"

/*
 * A selection of ISA-dependent interfaces
 */

#include <sys/reg.h>
#include <sys/thread.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_KERNEL)

extern greg_t getfp(void);
extern greg_t getpil(void);
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

extern void *lokmem_zalloc(size_t, int);
extern void lokmem_free(void *, size_t);
extern void lokmem_init(caddr_t, int);
extern void lokmem_gc();

extern u_int va_to_pfn(caddr_t);
extern u_int va_to_pa(u_int);

extern void sync_instruction_memory(caddr_t addr, u_int len);
extern void eieio(void);

extern void realsigprof(int, int);

#endif /* defined(_KERNEL) */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_ARCHSYSTM_H */
