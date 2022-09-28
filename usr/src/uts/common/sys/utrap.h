/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 */

#ifndef _UTRAP_H
#define	_UTRAP_H

#pragma ident	"@(#)utrap.h	1.1	95/11/05 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * this file contains definitions for user-level traps.
 */

#ifndef _ASM

typedef enum {UTRAP_FAST_SW_1 = 1,
#ifndef sparc
	UTRAP_FAST_SW_2 = 2
#else
	UTRAP_FAST_SW_2 = 2,
	UTRAP_V8P_FP_DISABLED = 7,
	UTRAP_V8P_MEM_ADDRESS_NOT_ALIGNED = 15
#endif
} utrap_entry_t;

typedef void *utrap_handler_t;	/* user trap handler entry point */

int
install_utrap(utrap_entry_t type, utrap_handler_t new_handler,
		utrap_handler_t *old_handlerp);

#endif /* ASM */

#define	UTRAP_UTH_NOCHANGE ((utrap_handler_t)(-1))
#ifdef sparc
#define	UTRAP_MAXTRAPS					4
#define	UTRAP_V8P_MAXTRAPS				2
#else
#define	UTRAP_MAXTRAPS					2
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _UTRAP_H */
