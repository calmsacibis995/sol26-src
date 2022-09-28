/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */

#ifndef	_LIBC_PPC_INC_PIC_H
#define	_LIBC_PPC_INC_PIC_H

#ident "@(#)PIC.h 1.3	96/11/25 SMI"

#include <sys/asm_linkage.h>

#if defined(_ASM) /* The remainder of this file is only for assembly files */

/*
 * This macro leaves a pointer to the global offset table in the link register.
 *
 * WARNING: It is up to the caller to:
 *		save the returned pointer as needed,
 *		save/restore the original link register contents
 *
 * These additional instructions were purposely omitted from this macro,
 * so that the calling sequence(s) could be optimized.
 */
#if defined(PIC)

#define PIC_SETUP()	bl	(_GLOBAL_OFFSET_TABLE_@local - 4)

#else	/* !defined(PIC)  - otherwise, becomes a no-op */

#define PIC_SETUP()

#endif	/* defined(PIC) */

#endif /* _ASM */

#endif	/* _LIBC_PPC_INC_PIC_H */
