/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 */

#ident	"@(#)sunfire.c	1.1	96/10/15 SMI"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sunddi.h>
#include <sys/modctl.h>

#include <sys/platform_module.h>

void
set_platform_defaults(void)
{
	extern int ce_verbose;
	extern int report_ce_console;
	extern int report_ce_log;

	ce_verbose = 1;		/* verbose ecc error correction */
	report_ce_console = 1;
	report_ce_log = 1;
}

char *sunfire_drivers[] = {
	"ac",
	"sysctrl",
	"environ",
	"simmstat",
	"sram",
};

int must_load_sunfire_modules = 1;

void
load_platform_drivers(void)
{
	int i;
	char *c = NULL;
	char buf[128];

	for (i = 0; i < (sizeof (sunfire_drivers) / sizeof (char *)); i++) {
		if ((modload("drv", sunfire_drivers[i]) < 0) ||
		    (ddi_install_driver(sunfire_drivers[i]) != DDI_SUCCESS)) {
			(void) strcat(buf, sunfire_drivers[i]);
			c = strcat(buf, ",");
		}
	}

	if (c) {
		c = strrchr(buf, ',');
		*c = '\0';
		cmn_err(must_load_sunfire_modules ? CE_PANIC : CE_WARN,
		    "Cannot load the [%s] system module(s) which "
		    "monitor hardware including temperature, "
		    "power supplies, and fans", buf);
	}
}
