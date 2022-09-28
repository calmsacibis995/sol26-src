/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_RAMDATA_H
#define	_RAMDATA_H

#pragma	ident	"@(#)ramdata.h	1.3	96/06/18 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* ramdata.h -- read/write data declarations */

/* maximum sizes of things */
#define	PRMAXSIG	(32*sizeof (sigset_t) / sizeof (long))
#define	PRMAXFAULT	(32*sizeof (fltset_t) / sizeof (long))
#define	PRMAXSYS	(32*sizeof (sysset_t) / sizeof (long))

extern	char	*command;	/* name of command ("truss") */
extern	int	interrupt;	/* interrupt signal was received */

extern	int	Fflag;		/* option flags from getopt() */
extern	int	qflag;

extern	char	*str_buffer;	/* fetchstring() buffer */
extern	unsigned str_bsize;	/* sizeof(*str_buffer) */

extern	process_t	Proc;	/* the process structure */
extern	process_t	*PR;	/* pointer to same (for abend()) */

extern	char	*procdir;	/* default PROC directory */

extern	int	timeout;	/* set TRUE by SIGALRM catchers */

extern	int	debugflag;	/* enable debugging printfs */

#ifdef	__cplusplus
}
#endif

#endif	/* _RAMDATA_H */
