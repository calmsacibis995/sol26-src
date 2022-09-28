/*
 * Copyright (c) 1993 by Sun Microsystems Inc.
 */

#ifndef	_SYS_PROM_DEBUG_H
#define	_SYS_PROM_DEBUG_H

#pragma ident	"@(#)prom_debug.h	1.8	95/10/22 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(DEBUG) && !defined(lint)

extern int	prom_debug;

void prom_printf(char *, ...);

#define	HERE	if (prom_debug)						\
	prom_printf("%s:%d: HERE\n", __FILE__, __LINE__)

#define	PRM_DEBUG(q)	if (prom_debug) {				\
	prom_printf("%s:%d: '%s' is ", __FILE__, __LINE__, #q);		\
	if (sizeof (q) == sizeof (unsigned long long))			\
		prom_printf("0x%x %8x\n",				\
		    (unsigned long)((unsigned long long)(q) >> 32),	\
			(unsigned long)(q));				\
	else								\
		prom_printf("0x%8x\n", (unsigned long)(q));		\
}

#define	PRM_INFO(l)	if (prom_debug)					\
	(prom_printf("%s:%d: ", __FILE__, __LINE__), 			\
	prom_printf(l), prom_printf("\n"))

#define	PRM_INFO1(str, a)	if (prom_debug)				\
	(prom_printf("%s:%d: ", __FILE__, __LINE__), 			\
	prom_printf((str), (a)))

#define	PRM_INFO2(str, a, b)	if (prom_debug)				\
	(prom_printf("%s:%d: ", __FILE__, __LINE__), 			\
	prom_printf((str), (a), (b)))

#define	STUB(n)		if (prom_debug)					\
	(prom_printf("%s:%d: ", __FILE__, __LINE__), 			\
	prom_printf("STUB: %s", #n))

#else

#define	HERE

#define	PRM_DEBUG(q)

#define	PRM_INFO(l)

#define	PRM_INFO1(str, a)

#define	PRM_INFO2(str, a, b)

#define	STUB(n)

#endif /* DEBUG && !lint */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PROM_DEBUG_H */
