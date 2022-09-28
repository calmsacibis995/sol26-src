
/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */


#pragma ident "@(#)init_cppc.c 1.2	94/06/27 SMI"

/*
 * This file defines the known controller types.  To add a new controller
 * type, simply add a new line to the array and define the necessary
 * ops vector in a 'driver' file.
 */
#include "global.h"
#include <sys/dkio.h>

extern	struct ctlr_ops scsiops;

/*
 * This array defines the supported controller types
 */
struct	ctlr_type ctlr_types[] = {

	{ DKC_SCSI_CCS,
		"SCSI",
		&scsiops,
		CF_SCSI | CF_EMBEDDED | CF_OLD_DRIVER },

};

/*
 * This variable is used to count the entries in the array so its
 * size is not hard-wired anywhere.
 */
int	nctypes = sizeof (ctlr_types) / sizeof (struct ctlr_type);
