/*
 * Copyright (c) 1993 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpr_driver.c	1.24	96/04/23 SMI"

/*
 * CPR driver support routines
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/epm.h>
#include <sys/cpr.h>

#define	CPR_BUFSIZE	128

extern int devi_detach(dev_info_t *, int);
extern int devi_attach(dev_info_t *, int);

static char 	*devi_string(dev_info_t *, char *);
static int	cpr_is_real_device(dev_info_t *);

/*
 * Traverse the dev info tree:
 *	Call each device driver in the system via a special case
 *	of the detach() entry point to quiesce itself.
 *	Suspend children first.
 *
 * This is tricky because we only want to suspend/resume real devices.
 * At the moment we consider these devices to be devices which:
 *	1) Are attached	[has a driver]
 *	2) Have a probe routine (typically not bus nexi)...which
 *	3) ..is not nulldev unless device is self-identifying
 */
static dev_info_t *failed_driver;

int
cpr_suspend_devices(dev_info_t *dip)
{
	int		error;
	char		buf[CPR_BUFSIZE];

	failed_driver = NULL;
	for (; dip != NULL; dip = ddi_get_next_sibling(dip)) {
		if (cpr_suspend_devices(ddi_get_child(dip)))
			return (ENXIO);
		if (!cpr_is_real_device(dip))
			continue;
		DEBUG2(errp("Suspending device %s\n", devi_string(dip, buf)));
		error = devi_detach(dip, DDI_SUSPEND);
		if (error != DDI_SUCCESS) {
			DEBUG2(errp("WARNING: Unable to suspend device %s\n",
				devi_string(dip, buf)));
			cmn_err(CE_WARN, "Unable to suspend device %s",
				devi_string(dip, buf));
			cmn_err(CE_WARN,
			"Device is busy or does not support suspend/resume");
			failed_driver = dip;
			return (ENXIO);
		}
	}
	return (0);
}

/*
 * Traverse the dev info tree:
 *	Call each device driver in the system via a special case
 *	of the attach() entry point to restore itself.
 *	This is a little tricky because it has to reverse the traversal
 *	order of cpr_suspend_devices().
 */
int
cpr_resume_devices(dev_info_t *start)
{
	dev_info_t	*dip, *next, *last = NULL;
	int error;
	char buf[CPR_BUFSIZE];

	while (last != start) {
		dip = start;
		next = ddi_get_next_sibling(dip);
		while (next != last && dip != failed_driver) {
			dip = next;
			next = ddi_get_next_sibling(dip);
		}
		if (dip == failed_driver)
			failed_driver = NULL;
		else if (cpr_is_real_device(dip) && failed_driver == NULL) {
			DEBUG2(errp("Resuming device %s\n",
				devi_string(dip, buf)));
			error = devi_attach(dip, DDI_RESUME);
			if (error != DDI_SUCCESS) {
				DEBUG2(errp(
					"WARNING: Unable to resume device %s\n",
					devi_string(dip, buf)));
				cmn_err(CE_WARN, "Unable to resume device %s",
					devi_string(dip, buf));
				return (ENXIO);
			}
		}
		if (cpr_resume_devices(ddi_get_child(dip)))
			return (ENXIO);
		last = dip;
	}
	return (0);
}

/*
 * Returns a string which contains device name and address.
 */
static char *
devi_string(dev_info_t *devi, char *buf)
{
	char *name;
	char *address;
	int size;

	name = ddi_node_name(devi);
	address = ddi_get_name_addr(devi);
	size = (name == NULL) ?
		strlen("<null name>") : strlen(name);
	size += (address == NULL) ?
		strlen("<null>") : strlen(address);

	/*
	 * Make sure that we don't over-run the buffer.
	 * There are 2 additional characters in the string.
	 */
	ASSERT((size + 2) <= CPR_BUFSIZE);

	if (name == NULL)
		(void) strcpy(buf, "<null name>");
	else
		(void) strcpy(buf, name);

	(void) strcat(buf, "@");
	if (address == NULL)
		(void) strcat(buf, "<null>");
	else
		(void) strcat(buf, address);

	return (buf);
}

/*
 * This function determines whether the given device is real (and should
 * be suspended) or not (pseudo like).  If the device has a "reg" property
 * then it can have register state to save/restore.
 */
static int
cpr_is_real_device(dev_info_t *dip)
{
	struct regspec *regbuf;
	int length;
	int rc;

	if (ddi_get_driver(dip) == NULL)
		return (0);

	if (DEVI(dip)->devi_comp_flags & (PMC_NEEDS_SR|PMC_PARENTAL_SR))
		return (1);
	if (DEVI(dip)->devi_comp_flags & PMC_NO_SR)
		return (0);

	/*
	 * now the general case
	 */
	rc = ddi_getlongprop(DDI_DEV_T_NONE, dip, DDI_PROP_DONTPASS, "reg",
	    (caddr_t)&regbuf, &length);
	ASSERT(rc != DDI_PROP_NO_MEMORY);
	if (rc != DDI_PROP_SUCCESS) {
		return (0);
	} else {
		kmem_free((caddr_t)regbuf, length);
		return (1);
	}
}

/*
 * cpr callback handler
 * called to suspend device driver execution threads during checkpoint.
 */
void
cpr_hold_driver()
{
	mutex_enter(&CPR->c_dlock);
	cv_wait(&CPR->c_holddrv_cv, &CPR->c_dlock);
	mutex_exit(&CPR->c_dlock);
}


/*
 * Power down the system.
 */
void
cpr_power_down()
{
	int is_defined = 0;
	char *wordexists = "p\" power-off\" find nip swap ! ";

	/*
	 * is_defined has value -1 when defined
	 */
	prom_interpret(wordexists, (int)(&is_defined), 0, 0, 0, 0);
	if (is_defined)
		prom_interpret("power-off", 0, 0, 0, 0, 0);
}
