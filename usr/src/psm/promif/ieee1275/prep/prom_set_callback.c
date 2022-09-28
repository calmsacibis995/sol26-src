/*
 * Copyright (c) 1993-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)prom_set_callback.c	1.4	96/07/03 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * This file contains support routines for OpenFirmware's Call back to
 * Client program. some more work is required with this file.
 */

extern int strcmp(char *, char *);

/*
 * These are the services supported for ppc.
 */
struct callbacks handlers[] = {
	"map", NULL,
	"unmap", NULL,
	"translate", NULL,
	NULL, NULL
};

int callback_handler(cell_t *);

/*
 * fill in a pointer to the correct callback function
 * for each callback entry in handlers[], then notify the prom
 */
int
init_callback_handler(struct callbacks *cbks)
{
	register int i, j;

	for (i = 0; cbks[i].name != NULL; i++) {
		for (j = 0; handlers[j].name != NULL; j++) {
			if (strcmp(cbks[i].name, handlers[j].name) == 0) {
				handlers[j].fn = cbks[i].fn;
				break;
			}
		}
	}
	/* Check that all the Call backs are initialized */
	for (i = 0; handlers[i].name != NULL; i++) {
		if (handlers[i].fn == NULL) {
			prom_printf("error in init_callbacks\n");
			return (0);
		}
	}
	prom_set_callback((void *)callback_handler);
	return (1);
}

/*
 * Handler for firmware callbacks, ci[] contains...
 * ci[0] - service name
 * ci[1] - number of arguments
 * ci[2] - number of results expected
 * ci[3] - ...args...
 * ci[num_args + 3] - ...return values...
 */
int
callback_handler(cell_t *ci)
{
	int i, rv;
	int args;	/* number of arguments for service */

#ifdef DEBUG_CALLBACKS
	for (i = 0; i <= (ci[1] + 3); i++) {
		prom_printf("ci[%d]: 0x%x\n", i, ci[i]);
	}
#endif
	args = ci[1];

	for (i = 0; handlers[i].name != NULL; i++) {
		if (strcmp((char *)ci[0], handlers[i].name) == 0)
			break;
	}
	if (handlers[i].name == NULL) {
		prom_printf("Error in callback_handler for OpenFirmware\n");
		rv = 0xffffffff;
		ci[args+3] = rv;
		return (rv);
	}
	/*
	 * Call the handler for the Service passing argument pointer,
	 * number of argument Cells, address where results have to be
	 * placed: as parameters.
	 *
	 */
	rv = (*handlers[i].fn)(&ci[3], args, &ci[args+3]);
	return (rv);
}
