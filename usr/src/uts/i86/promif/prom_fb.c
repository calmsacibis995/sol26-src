/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)prom_fb.c	1.2	96/05/14 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/prom_emul.h>
#include <sys/obpdefs.h>

#if !defined(KADB) && !defined(I386BOOT)

extern dnode_t prom_optionsnode();
extern struct prom_prop *get_prom_prop(prom_node_t *pnp, char *name);

/*
 * return true if stdout is the frame buffer
 */
int
prom_stdout_is_framebuffer(void)
{
	struct prom_node * pnp;
	struct prom_prop *pp;

	pnp = (struct prom_node *)prom_optionsnode();
	if (pnp == OBP_NONODE || pnp == OBP_BADNODE)
		return (0);
	pp = get_prom_prop(pnp, "output-device");
	if (pp == NULL)
		return (0);
	return (prom_strncmp(pp->pp_val, "screen", pp->pp_len) == 0);
}
#endif
