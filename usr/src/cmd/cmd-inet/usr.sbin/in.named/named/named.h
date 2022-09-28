/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/* named.h - include the local definitions in the right order
 * vix 28aug93 [original]
 *
 * $Id: named.h,v 8.1 1994/12/15 06:24:14 vixie Exp $
 */

#pragma ident	"@(#)named.h	1.1	96/05/09 SMI"

#include "../conf/portability.h"
#include "../conf/options.h"

/*
 * Since the include of <sys/bitypes.h> was removed from <resolv.h>,
 * we must include it here instead.
 */
#include <sys/bitypes.h>

#include "pathnames.h"

#include "ns_defs.h"
#include "db_defs.h"

#include "ns_glob.h"
#include "db_glob.h"

#include "ns_func.h"
#include "db_func.h"
