
/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)init_csparc.c	1.1	93/09/07 SMI"

/*
 * This file defines the known controller types.  To add a new controller
 * type, simply add a new line to the array and define the necessary
 * ops vector in a 'driver' file.
 */
#include "global.h"

extern	struct ctlr_ops md21ops;
extern	struct ctlr_ops scsiops;
extern	struct ctlr_ops idops;

/*
 * This array defines the supported controller types
 */
struct	ctlr_type ctlr_types[] = {
	{ DKC_MD21,
		"MD21",
		&md21ops,
		CF_SCSI | CF_DEFECTS | CF_OLD_DRIVER },

	{ DKC_SCSI_CCS,
		"SCSI",
		&scsiops,
		CF_SCSI | CF_EMBEDDED | CF_OLD_DRIVER },
	{ DKC_PANTHER,
		"ISP-80",
		&idops,
		CF_IPI | CF_WLIST | CF_OLD_DRIVER },
};

/*
 * This variable is used to count the entries in the array so its
 * size is not hard-wired anywhere.
 */
int	nctypes = sizeof (ctlr_types) / sizeof (struct ctlr_type);
