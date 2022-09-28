/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SCSI_SCSI_PARAMS_H
#define	_SYS_SCSI_SCSI_PARAMS_H

#pragma ident	"@(#)scsi_params.h	1.14	96/06/07 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	NUM_SENSE_KEYS		16	/* total number of Sense keys */

#define	NTAGS			256	/* number of tags per lun */

/*
 * General parallel SCSI parameters
 */
#define	NTARGETS		8	/* total # of targets per SCSI bus */
#define	NTARGETS_WIDE		16	/* #targets per wide SCSI bus */
#define	NLUNS_PER_TARGET	8	/* number of luns per target */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SCSI_SCSI_PARAMS_H */
