/*
 *	Copyright (c) 1996 Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ifndef	_SYS_MODEL_H
#define	_SYS_MODEL_H

#pragma ident	"@(#)model.h	1.1	96/09/24 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *	These bits are used in various places to specify the data model
 *	of the originator (and/or consumer) of data items.  See <sys/conf.h>
 *	<sys/file.h>, <sys/stream.h> and <sys/sunddi.h>.
 */
#define	DATAMODEL_MASK	0x0FF00000

#define	DATAMODEL_ILP32	0x00100000
#define	DATAMODEL_LP64	0x00200000

#define	DATAMODEL_NONE	0

#if	defined(_LP64)
#define	DATAMODEL_NATIVE	DATAMODEL_LP64
#elif	defined(_ILP32)
#define	DATAMODEL_NATIVE	DATAMODEL_ILP32
#else
#error	"No DATAMODEL_NATIVE specified"
#endif	/* _LP64 || _ILP32 */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MODEL_H */
