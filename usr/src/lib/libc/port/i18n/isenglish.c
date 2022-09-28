/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)isenglish.c 1.4	96/07/31  SMI"

/*
 * COMPONENT_NAME: (LIBCCHR) LIBC Character Classification Funcions
 *
 * FUNCTIONS: isenglish
 *
 */
/*
 *
 * FUNCTION: Determines if the process code, pc, is an english character
 *
 *
 * PARAMETERS: pc  -- character to be classified
 *
 *
 * RETURN VALUES: 0 -- if pc is not an english character
 *                >0 - If c is an english character
 *
 *
 */

#include <ctype.h>
#include <wchar.h>
#include <sys/localedef.h>
#include <wctype.h>

#ifdef isenglish
#undef isenglish
#endif

#pragma weak isenglish = _isenglish
int
_isenglish(wint_t pc)
{
	if ((unsigned long)pc > 0x9f)
		return (METHOD(__lc_ctype, iswctype)(__lc_ctype, pc, _E3));
	else
		return (0);
}
