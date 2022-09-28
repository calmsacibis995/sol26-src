/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 */

#ident	"@(#)tazmo.c	1.1	96/10/15 SMI"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sunddi.h>
#include <sys/modctl.h>

#include <sys/platform_module.h>

void
set_platform_defaults(void)
{
}

void
load_platform_drivers(void)
{
	if ((modload("drv", "envctrl") < 0) ||
	    (ddi_install_driver("envctrl") != DDI_SUCCESS)) {
		cmn_err(CE_WARN, "Envctrl failed to load\n");
	}
}
