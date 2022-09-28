/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.  All Rights Reserved.
 */

#pragma ident	"@(#)opsetpr.c	1.3	94/05/05 SMI"

#include <sys/types.h>
#include "db_as.h"
#include "adb.h"

/*
 * Disassemble instructions.  "The" function exported to the rest of adb/kadb.
 */

void
printins(char modif, int disp, ins_type inst)
{
	db_printf(5, "printins: called");
	printf("%s",disassemble( &inst, dot));
	dotinc = 4;		/* always */
}

int	oflag;

is_fpa_avail()
{
	return 1;
}
