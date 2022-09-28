/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */

#ifndef _SYS_PTE_H
#define	_SYS_PTE_H

#pragma ident	"@(#)pte.h	1.13	94/12/05 SMI"

#ifndef _ASM
#include <sys/types.h>
#endif /* _ASM */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Definitions for the PowerPC MMU.
 *
 * The definitions are valid only for 32bit implementation of PowerPC
 * architecture.
 */

#if !defined(_ASM) && defined(_KERNEL)

/*
 * Page Table Entry Structure definition.
 *
 * Note: In Little Endian mode the words within PTE are reversed, and the
 *	 byte ordering is such that the byte0 in Big Endian actually
 *	 maps to byte 7 of Little Endian, etc. So, care must be taken
 *	 if we are accessig a specific byte within the PTE. But accessing
 *	 thru the bit fields within C structure always works independent
 *	 of byte order.
 *
 *	 The pte structure has 4 different definitions depending on the
 *	 bit field ordering by the compiler and the endianness. For
 *	 PowerPC only two of them are really possible.
 */
#if defined(_BIT_FIELDS_LTOH) && defined(_LITTLE_ENDIAN)
typedef struct pte {
	/* pte word 1 */
	u_int pte_pp:2; 	/* Page Protection bits */
	u_int :1;		/* reserved */
	u_int pte_wimg:4;	/* Memory/Cache control bits */
	u_int pte_modified:1;	/* Changed bit */
	u_int pte_referenced:1; /* Referenced bit */
	u_int :3;		/* reserved */
	u_int pte_ppn:20;	/* Physical Page Number (RPN) */
	/* pte word 0 */
	u_int pte_api:6;	/* abbreviated page index */
	u_int pte_h:1;		/* hash function identifier (H bit) */
	u_int pte_vsid:24;	/* virtual segment id */
	u_int pte_valid:1;	/* entry valid (v=1) or invalid (v=0) */
} pte_t;
#endif

#if defined(_BIT_FIELDS_HTOL) && defined(_BIG_ENDIAN)
typedef struct pte {
	/* pte word 0 */
	u_int pte_valid:1;	/* entry valid (v=1) or invalid (v=0) */
	u_int pte_vsid:24;	/* virtual segment id */
	u_int pte_h:1;		/* hash function identifier (H bit) */
	u_int pte_api:6;	/* abbreviated page index */
	/* pte word 1 */
	u_int pte_ppn:20;	/* Physical Page Number (RPN) */
	u_int :3;		/* reserved */
	u_int pte_referenced:1; /* Referenced bit */
	u_int pte_modified:1;	/* Changed bit */
	u_int pte_wimg:4;	/* Memory/Cache control bits */
	u_int :1;		/* reserved */
	u_int pte_pp:2; 	/* Page Protection bits */
} pte_t;
#endif

#if defined(_BIT_FIELDS_HTOL) && defined(_LITTLE_ENDIAN)
typedef struct pte {
	/* pte word 1 */
	u_int pte_ppn:20;	/* Physical Page Number (RPN) */
	u_int :3;		/* reserved */
	u_int pte_referenced:1; /* Referenced bit */
	u_int pte_modified:1;	/* Changed bit */
	u_int pte_wimg:4;	/* Memory/Cache control bits */
	u_int :1;		/* reserved */
	u_int pte_pp:2; 	/* Page Protection bits */
	/* pte word 0 */
	u_int pte_valid:1;	/* entry valid (v=1) or invalid (v=0) */
	u_int pte_vsid:24;	/* virtual segment id */
	u_int pte_h:1;		/* hash function identifier (H bit) */
	u_int pte_api:6;	/* abbreviated page index */
} pte_t;
#endif

#if defined(_BIT_FIELDS_LTOH) && defined(_BIG_ENDIAN)
typedef struct pte {
	/* pte word 0 */
	u_int pte_api:6;	/* abbreviated page index */
	u_int pte_h:1;		/* hash function identifier (H bit) */
	u_int pte_vsid:24;	/* virtual segment id */
	u_int pte_valid:1;	/* entry valid (v=1) or invalid (v=0) */
	/* pte word 1 */
	u_int pte_pp:2; 	/* Page Protection bits */
	u_int :1;		/* reserved */
	u_int pte_wimg:4;	/* Memory/Cache control bits */
	u_int pte_modified:1;	/* Changed bit */
	u_int pte_referenced:1; /* Referenced bit */
	u_int :3;		/* reserved */
	u_int pte_ppn:20;	/* Physical Page Number (RPN) */
} pte_t;
#endif

typedef union ptes {
	struct pte pte;
	u_int	pte_words[2];
} ptes_t;

#define	hwpte_t	pte_t

/*
 * spte structure definition.
 *
 * This is essentially same as the second word in the pte structure except
 * the 'spte_valid' bit. This is used for constructing the Sysmap[]
 * array used with in the kernel.
 */
#ifdef _BIT_FIELDS_LTOH
typedef struct spte {
	u_int spte_pp:2; 	/* Page Protection bits */
	u_int spte_valid:1;	/* Valid bit */
	u_int spte_wimg:4;	/* Memory/Cache control bits */
	u_int spte_modified:1;	/* Changed bit */
	u_int spte_referenced:1; /* Referenced bit */
	u_int :3;		/* reserved */
	u_int spte_ppn:20;	/* Physical Page Number (RPN) */
} spte_t;
#endif

#ifdef _BIT_FIELDS_HTOL
typedef struct spte {
	u_int spte_ppn:20;	/* Physical Page Number (RPN) */
	u_int :3;		/* reserved */
	u_int spte_referenced:1; /* Referenced bit */
	u_int spte_modified:1;	/* Changed bit */
	u_int spte_wimg:4;	/* Memory/Cache control bits */
	u_int spte_valid:1;	/* Valid bit */
	u_int spte_pp:2; 	/* Page Protection bits */
} spte_t;
#endif

/*
 * Macros.
 */
/* WIMG bits; c=1 cached, c=0 uncached, g (guarded bit - 0 or 1) */
#define	WIMG(c, g) ((c) ? 2 : (6 | (g)))
#define	API(a)	(((a) >> 22) & 0x3F)  /* API bits from vaddr */
#define	VSID(range, a)	(((u_int)(range) << 4) | ((u_int)(a) >> 28))

/* bit masks in pte word 0 */
#define	PTEW0_VALID_MASK	0x80000000
#define	PTEW0_VSID_MASK		0x7FFFFF80
#define	PTEW0_HASH_MASK		0x00000040
#define	PTEW0_API_MASK		0x0000003F
#define	PTEW0_VSID_SHIFT	7
#define	PTEW0_HASH_SHIFT	6

/* bit masks in pte word 1 */
#define	PTEW1_REF_MASK		0x00000100
#define	PTEW1_MOD_MASK		0x00000080
#define	PTEW1_RM_MASK		0x00000180
#define	PTEW1_WIMG_MASK		0x00000078
#define	PTEW1_PP_MASK		0x00000003
#define	PTEW1_PPN_MASK		0xFFFFF000
#define	PTEW1_PPN_SHIFT		12
#define	PTEW1_RM_SHIFT		7

#define	PTEW1_PPN(p)		((u_int)p >> 12)
#define	PTEW1_WIMG(p)		(((u_int)p >> 3) & 0xf)
#define	PTEW1_RM(p)		(((u_int)p >> 7) & 0x3)

/*
 * PTEWORD0 - word number of the first word in the PTE that contains VSID.
 * PTEWORD1 - word number of the second word in the PTE that contains PPN.
 */
#ifdef _BIG_ENDIAN
#define	PTEWORD0	0
#define	PTEWORD1	1
#else
#define	PTEWORD0	1	/* on Little Endian the words are reversed */
#define	PTEWORD1	0
#endif

#define	SPTE_VALID_MASK		0x00000004

/* pte bit field values */
#define	PTE_VALID	1
#define	PTE_INVALID	0

/* pte_wimg value bit masks */
#define	WIMG_CACHE_DIS		0x4
#define	WIMG_GUARDED		0x1
#define	WIMG_WRITE_THRU		0x8
#define	WIMG_MEM_COHERENT	0x2

#define	MMU_INVALID_PTE		((u_longlong_t)0)
#define	MMU_INVALID_SPTE	((u_int)0)

#endif /* !_ASM  && _KERNEL */

/*
 * Definitions for Access Permissions (page or block). The values represent
 * the pp bits in PTE assuming that the key values of Ks and Ku are same
 * as MSR[PR] bit (i.e Ku=1 and Ks=0).
 *
 * Note: On PPC kernel pages can not be write protected without giving user
 *	 read access.
 *
 * (Names taken from SPARC Reference MMU code, but considering execute
 * and read permissions equivalent.)
 */

#define	MMU_STD_SRWX		0
#define	MMU_STD_SRWXURX		1
#define	MMU_STD_SRWXURWX	2
#define	MMU_STD_SRXURX		3

#define	MMU_STD_PAGEMASK	0xFFFFFF000
#define	MMU_STD_PAGESHIFT	12
#define	MMU_STD_PAGESIZE	(1 << MMU_STD_PAGESHIFT)

/*
 * Macros/functions for reading portions of hw pte.
 */
#ifndef _ASM

extern u_int byte_reverse_long(u_int);

#define	ppn_from_hwpte(pte)	(((pte_t *)(pte))->pte_ppn)
#define	api_from_hwpte(pte)	(((pte_t *)(pte))->pte_api)
#define	hbit_from_hwpte(pte)	(((pte_t *)(pte))->pte_h)
#define	vsid_from_hwpte(pte)	(((pte_t *)(pte))->pte_vsid)
#define	pp_from_hwpte(pte)	(((pte_t *)(pte))->pte_pp)
#define	rbit_from_hwpte(pte)	(((pte_t *)(pte))->pte_referenced)
#define	mbit_from_hwpte(pte)	(((pte_t *)(pte))->pte_modified)
#define	wimg_from_hwpte(pte)	(((pte_t *)(pte))->pte_wimg)
#define	hwpte_valid(pte)	(((pte_t *)(pte))->pte_valid)
#define	hwpte_to_pte(hwpte, pte) (*(pte_t *)(pte) = *(pte_t *)(hwpte))
#define	rmbits_from_hwpte(hwpte) \
	((((u_int *)(hwpte))[PTEWORD1] & PTEW1_RM_MASK) >> PTEW1_RM_SHIFT)

#define	MAKE_PFNUM(a)	\
	(((struct pte *)(a))->pte_ppn)

/* Convert pte (software) to spte format */
#define	pte_to_spte(pte, spte) \
	(*(u_int *)(spte) = *(u_int *)(pte) | ((pte)->pte_valid ? \
			    SPTE_VALID_MASK : 0))

#define	pte_valid(pte)		(((pte_t *)(pte))->pte_valid)
#define	spte_valid(spte)	((spte)->spte_valid != 0)
#define	pte_memory(pte)		(((pte)->pte_ppn & 0xf0000) == 0)

#endif /* _ASM */

#if !defined(_ASM) && defined(_KERNEL)
/* utilities defined in locore.s */
extern	struct spte Sysmap[];
extern	char Sysbase[];
extern	char Syslimit[];
#endif /* !defined(_ASM) && defined(_KERNEL) */

#ifdef	__cplusplus
}
#endif

#endif /* !_SYS_PTE_H */
