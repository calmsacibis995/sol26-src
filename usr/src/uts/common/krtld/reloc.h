/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)reloc.h	1.5	96/07/12 SMI"

#ifndef		RELOC_DOT_H
#define		RELOC_DOT_H

#if	defined(_KERNEL)
#include	"machelf.h"
#include	<sys/bootconf.h>
#include	<sys/kobj.h>
#include	<sys/kobj_impl.h>
#else
#include	"machdep.h"
#endif /* _KERNEL */

/*
 * Global include file for relocation common code.
 *
 * Flags for reloc_entry->re_flags
 */
#define	FLG_RE_NOTREL		0x0000
#define	FLG_RE_GOTREL		0x0001
#define	FLG_RE_PCREL		0x0002
#define	FLG_RE_RELPC		0x0004
#define	FLG_RE_PLTREL		0x0008
#define	FLG_RE_VERIFY		0x0010	/* verify value fits */
#define	FLG_RE_UNALIGN		0x0020	/* offset is not aligned */
#define	FLG_RE_WDISP16		0x0040	/* funky sparc DISP16 rel */
#define	FLG_RE_SIGN		0x0080	/* value is signed */
#define	FLG_RE_HA		0x0100	/* HIGH ADJUST (ppc) */
#define	FLG_RE_BRTAK		0x0200	/* BRTAKEN (ppc) */
#define	FLG_RE_BRNTAK		0x0400	/* BRNTAKEN (ppc) */
#define	FLG_RE_SDAREL		0x0800	/* SDABASE REL (ppc) */
#define	FLG_RE_SECTREL		0x1000	/* SECTOFF relative */
#define	FLG_RE_ADDRELATIVE	0x2000	/* RELATIVE relocatin required for */
					/* non-fixed objects */


/*
 * Macros for testing relocation table flags
 */
extern	const Rel_entry		reloc_table[];

#define	IS_PLT(X)		((reloc_table[(X)].re_flags & \
					FLG_RE_PLTREL) != 0)
#define	IS_GOT_RELATIVE(X)	((reloc_table[(X)].re_flags & \
					FLG_RE_GOTREL) != 0)
#define	IS_GOT_PC(X)		((reloc_table[(X)].re_flags & \
					FLG_RE_RELPC) != 0)
#define	IS_PC_RELATIVE(X)	((reloc_table[(X)].re_flags & \
					FLG_RE_PCREL) != 0)
#define	IS_SDA_RELATIVE(X)	((reloc_table[(X)].re_flags & \
					FLG_RE_SDAREL) != 0)
#define	IS_SECT_RELATIVE(X)	((reloc_table[(X)].re_flags & \
					FLG_RE_SECTREL) != 0)
#define	IS_ADD_RELATIVE(X)	((reloc_table[(X)].re_flags & \
					FLG_RE_ADDRELATIVE) != 0)

/*
 * Functions.
 */
extern	int	do_reloc(unsigned char, unsigned char *, Word *,
			const char *, const char *);

#if	defined(_KERNEL)
/*
 * These are macro's that are only needed for krtld.  Many of these
 * are already defined in the sgs/include files referenced by
 * ld and rtld
 */

#define	S_MASK(n)	((1 << (n)) -1)
#define	S_INRANGE(v, n)	(((-(1 << (n)) - 1) < (v)) && ((v) < (1 << (n))))

/*
 * This converts the sgs eprintf() routine into the _printf()
 * as used by krtld.
 */
#define	eprintf		_printf
#define	ERR_FATAL	ops

/*
 * Message strings used by doreloc()
 */
#define	MSG_INTL(x)		x
#define	MSG_ORIG(x)		x
#define	MSG_STR_UNKNOWN		"(unknown)"
#define	MSG_REL_UNIMPL		"relocation error: file %s: symbol %s:" \
				" unimplemented relocation type: %d"
#define	MSG_REL_NONALIGN	"relocation error: %s: file %s: symbol %s:" \
				" offset 0x%x is non-aligned"
#define	MSG_REL_UNNOBITS	"relocation error: %s: file %s: symbol %s:"\
				" unsupported number of bits: %d"
#define	MSG_REL_LOOSEBITS	"relocation error: %s: file %s: symbol %s:" \
				" value 0x%x looses %d bits at offset 0x%x"
#define	MSG_REL_NOFIT		"relocation error: %s: file %s: symbol %s:" \
				" value 0x%x does not fit"
#define	MSG_REL_UNSUPSZ		"relocation error: %s: file %s: symbol %s: " \
				"offset size (%d bytes) is not supported"

extern const char *	conv_reloc_SPARC_type_str(Word rtype);
extern const char *	conv_reloc_386_type_str(Word rtype);
extern const char *	conv_reloc_PPC_type_str(Word rtype);
#endif /* _KERNEL */


#endif /* RELOC_DOT_H */
