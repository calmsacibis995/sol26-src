/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */
#pragma ident "@(#)lconstants.c 1.4       94/09/09 SMI"
#ifdef __STDC__
	#pragma weak lzero = _lzero
	#pragma weak lone = _lone
	#pragma weak lten = _lten
#endif
#include	"synonyms.h"
#include	<sys/types.h>
#include	<sys/dl.h>

#ifdef	__LITTLE_ENDIAN
dl_t	lzero	= {0,  0};
dl_t	lone	= {1,  0};
dl_t	lten	= {10, 0};
#else	/* _BIG_ENDIAN */
dl_t	lzero	= {0,  0};
dl_t	lone	= {0,  1};
dl_t	lten	= {0, 10};
#endif	/* __LITTLE_ENDIAN */
