/*
 * Copyright (c) 1992-1993, by Sun Microsystems, Inc.
 */

#ifndef _SYS_X86_ARCHEXT_H
#define	_SYS_X86_ARCHEXT_H

#pragma ident	"@(#)x86_archext.h	1.5	96/10/28 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


#define	P5_PSE_SUPPORTED	0x08
#define	P5_TSC_SUPPORTED	0x10
#define	P5_MSR_SUPPORTED	0x20
#define	P6_APIC_SUPPORTED	0x200
#define	P6_MTRR_SUPPORTED	0x1000
#define	P6_PGE_SUPPORTED	0x2000
#define	P6_MCA_SUPPORTED	0x4000
#define	P6_CMOV_SUPPORTED	0x8000
#define	P5_MMX_SUPPORTED	0x800000

#define	K5_PGE_SUPPORTED	0x20
#define	K5_SCE_SUPPORTED	0x40

#define	K5_PSE		0x10
#define	K5_GPE		0x20
#define	K5_PGE		0x200

#define	P5_PSE		0x10
#define	P6_GPE		0x80
#define	P6_PGE		0x2000


#define	P5_MCHADDR	0x0
#define	P5_CTR1		0x13

#define	K5_MCHADDR	0x0
#define	K5_MCHTYPE	0x01
#define	K5_TSC		0x10
#define	K5_TR12		0x12

#define	REG_MTRRCAP		0xfe
#define	REG_MTRRDEF		0x2ff
#define	REG_MTRR64K		0x250
#define	REG_MTRR16K1		0x258
#define	REG_MTRR16K2		0x259
#define	REG_MTRR4K1		0x268
#define	REG_MTRR4K2		0x269
#define	REG_MTRR4K3		0x26a
#define	REG_MTRR4K4		0x26b
#define	REG_MTRR4K5		0x26c
#define	REG_MTRR4K6		0x26d
#define	REG_MTRR4K7		0x26e
#define	REG_MTRR4K8		0x26f

#define	REG_MTRRPHYSBASE0	0x200
#define	REG_MTRRPHYSMASK7	0x20f
#define	REG_MC0_CTL		0x400
#define	REG_MC5_MISC		0x417
#define	REG_PERFCTR0		0xc1
#define	REG_PERFCTR1		0xc2

#define	REG_PERFEVNT0		0x186
#define	REG_PERFEVNT1		0x187

#define	REG_TSC			0x10
#define	REG_APIC_BASE_MSR	0x1b



#define	MTRRTYPE_MASK		0xff


#define	MTRRCAP_FIX		0x100
#define	MTRRCAP_VCNTMASK	0xff
#define	MTRRCAP_USWC		0x400

#define	MTRRDEF_E		0x800
#define	MTRRDEF_FE		0x400

#define	MTRRPHYSMASK_V		0x800

#define	MTRR_SETTYPE(a, t)	((a &= ~0xff), (a |= ((t) & 0xff)))
#define	MTRR_SETVINVALID(a)	(a &= ~MTRRPHYSMASK_V)


#define	MTRR_SETVBASE(a, b, t)	(a[1] = 0, \
			(a[0] = ((((u_int)(b)) & 0xfffff000) | ((t) & 0xff))))

#define	MTRR_SETVMASK(a, s, v)	(a[1] = 0x0f, \
			(a[0] = (~(s - 1) & 0xfffff000) | (v << 11)))


#define	MTRR_GETVBASE(a)	(a & 0xfffff000)
#define	MTRR_GETVTYPE(a)	(a & 0xff)
#define	MTRR_GETVSIZE(a)	(~a & 0xfffff000)


#define	MAX_MTRRVAR	8
typedef	struct	mtrrvar {
	u_int	mtrrphysbase[2];
	u_int	mtrrphysmask[2];
} mtrrvar_t;

#define	mtrrphys_base	mtrrphysbase[0]
#define	mtrrphys_mask	mtrrphysmask[0]
#define	X86_LARGEPAGE	0x01
#define	X86_TSC		0x02
#define	X86_MSR		0x04
#define	X86_MTRR	0x08
#define	X86_PGE		0x10
#define	X86_APIC	0x20
#define	X86_CMOV	0x40
#define	X86_MMX 	0x80

#define	X86_P5		0x10000
#define	X86_K5		0x20000
#define	X86_P6		0x40000

#define	X86_CPU_TYPE	0xff0000


#define	CR0_CD		0x40000000
#define	CR0_NW		0x20000000

#define	INST_RDMSR	0x320f
#define	INST_WRMSR	0x300f
#define	INST_RTSC	0x310f

#define	RDWRMSR_INST_LEN	2

extern int	x86_feature;
extern int	rdmsr(), wrmsr();
extern	void	mtrr_sync();



#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_X86_ARCHEXT_H */
