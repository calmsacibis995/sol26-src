/*
 * Copyright (c) 1992-1993, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef _SYS_SPL_H
#define	_SYS_SPL_H

#pragma ident	"@(#)spl.h	1.2	94/11/30 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Convert system interrupt priorities (0-7) into a psr for splx.
 * In general, the processor priority (0-15) should be 2 times
 * the system pririty.
 */

/*
 * on PPC hardware spl and priority are the same
 */
#define	pritospl(n)	(n)


/*
 * on PPC hardware these are all identity functions
 */
#define	ipltospl(n)	(n)
#define	spltoipl(n)	(n)
#define	spltopri(n)	(n)

/*
 * Hardware spl levels
 * it should be replace by the appropriate interrupt class info.
 */
#define	SPL7    13

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SPL_H */
