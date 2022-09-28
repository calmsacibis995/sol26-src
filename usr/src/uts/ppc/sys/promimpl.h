/*
 * Copyright (c) 1991-1993, by Sun Microsystems, Inc.
 */

#ifndef	_SYS_PROMIMPL_H
#define	_SYS_PROMIMPL_H

#pragma ident	"@(#)promimpl.h	1.20	96/02/23 SMI"

/*
 * Promif implemenation functions and variables.
 *
 * These interfaces are not 'exported' in the same sense as
 * those described in promif.h
 *
 * Used so that the kernel and other stand-alones (eg boot)
 * don't have to directly reference the prom (of which there
 * are now several completely different variants).
 */

#include <sys/types.h>
#include <sys/promif.h>
#include <sys/prom_isa.h>
#include <sys/prom_plat.h>
#include <sys/openprom.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern int obp_romvec_version;

/*
 * Debugging macros for the promif functions.
 */

#define	PROMIF_DMSG_VERBOSE		2
#define	PROMIF_DMSG_NORMAL		1

extern int promif_debug;		/* externally patchable */

#define	PROMIF_DEBUG			/* define this to enable debugging */
#define	PROMIF_DEBUG_1275		/* Debug 1275 client interface calls */

#ifdef PROMIF_DEBUG
#define	PROMIF_DPRINTF(args)				\
	if (promif_debug) { 				\
		if (promif_debug == PROMIF_DMSG_VERBOSE)	\
			prom_printf("file %s line %d: ", __FILE__, __LINE__); \
		prom_printf args;			\
	}
#else
#define	PROMIF_DPRINTF(args)
#endif /* PROMIF_DEBUG */

/*
 * minimum alignment required by prom
 */
#define	PROMIF_MIN_ALIGN	4

#ifdef	_LITTLE_ENDIAN
/*
 * decode int for little endian machines.
 */
extern u_int swap_int(u_int *addr);
#define	prom_decode_int(v)	swap_int((u_int *)&(v))
#else
#define	prom_decode_int(v)	(v)
#endif	/* _LITTLE_ENDIAN */

/*
 * Private utility routines (not exported as part of the interface)
 */

extern	char		*prom_strcpy(char *s1, char *s2);
extern	char		*prom_strncpy(char *s1, char *s2, size_t n);
extern	int		prom_strcmp(char *s1, char *s2);
extern	int		prom_strncmp(char *s1, char *s2, size_t n);
extern	int		prom_strlen(char *s);
extern	char		*prom_strrchr(char *s1, char c);
extern	char		*prom_strchr(const char *s1, int c);
extern	char		*prom_strcat(char *s1, char *s2);

/*
 * IEEE 1275 Routines defined by each platform using IEEE 1275:
 */

extern	void		*p1275_cif_init(void *);
extern	int		p1275_cif_call(void *);

/*
 * More private globals
 */
extern	void		*p1275cif;	/* P1275 client interface cookie */

/*
 * Every call into the prom is wrappered with these calls so that
 * the caller can ensure that e.g. pre-emption is disabled
 * while we're in the firmware.  See 1109602.
 */
extern	void		(*promif_preprom)(void);
extern	void		(*promif_postprom)(void);

/*
 * The prom interface uses this string internally for prefixing error
 * messages so that the "client" of the given instance of
 * promif can be identified e.g. "boot", "kadb" or "kernel".
 *
 * It is passed into the library via prom_init().
 */
extern	char		promif_clntname[];

/*
 * This routine is called when all else fails (and there may be no firmware
 * interface at all!)
 */
extern void		prom_fatal_error(char *);

#ifdef	__cplusplus
}
#endif

#endif /* !_SYS_PROMIMPL_H */
