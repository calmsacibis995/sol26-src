/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)prom_prop.c	1.4	96/06/14 SMI"

/*
 * Stuff for mucking about with properties
 */

#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/prom_emul.h>

#if !defined(KADB) && !defined(I386BOOT)

extern void bcopy(caddr_t, caddr_t, size_t);

/*
 * Retrieve a PROM property
 */
struct prom_prop *
get_prom_prop(prom_node_t *pnp, char *name)
{
	struct prom_prop	*propp;

	for (propp = pnp->pn_propp; propp != NULL; propp = propp->pp_next) {
		if (prom_strcmp(propp->pp_name, name) == 0)
			break;
	}
	return (propp);
}

int
prom_getproplen(dnode_t nodeid, caddr_t name)
{
	struct prom_prop *propp;
	prom_node_t *pnp;
	int	len;

	if (!prom_is_p1275())
		return (-1); /* no tree to get info from */
	pnp = (prom_node_t *)nodeid;
	propp = get_prom_prop(pnp, name);
	if (propp == NULL) {
		return (-1);
	}
	len = propp->pp_len;
	return (len);
}


int
prom_getprop(dnode_t nodeid, caddr_t name, caddr_t value)
{
	struct prom_prop *propp;
	prom_node_t *pnp;
	int	len;

	pnp = (prom_node_t *)nodeid;
	propp = get_prom_prop(pnp, name);
	if (propp == NULL) {
		return (-1);
	}
	len = propp->pp_len;
	bcopy(propp->pp_val, value, len);
	return (len);
}

caddr_t
prom_nextprop(dnode_t nodeid, caddr_t previous, caddr_t next)
{
	struct prom_prop *propp;
	prom_node_t *pnp;

	next[0] = '\0';
	pnp = (prom_node_t *)nodeid;
	/*
	 * getting next of NULL or a null string returns the first prop name
	 */
	if (previous == NULL || *previous == '\0') {
		if (pnp->pn_propp)
			prom_strcpy(next, pnp->pn_propp->pp_name);
		return (next);
	}
	propp = get_prom_prop(pnp, previous);
	if (propp == NULL) {
		return (next);
	}
	if (propp->pp_next == NULL)
		return (next);
	prom_strcpy(next, propp->pp_next->pp_name);
	return (next);
}

/*
 * prom_decode_composite_string:
 *
 * Returns successive strings in a composite string property.
 * A composite string property is a buffer containing one or more
 * NULL terminated strings contained within the length of the buffer.
 *
 * Always call with the base address and length of the property buffer.
 * On the first call, call with prev == 0, call successively
 * with prev == to the last value returned from this function
 * until the routine returns zero which means no more string values.
 */
char *
prom_decode_composite_string(void *buf, size_t buflen, char *prev)
{
	if ((buf == 0) || (buflen == 0) || ((int)buflen == -1))
		return ((char *)0);

	if (prev == 0)
		return ((char *)buf);

	prev += prom_strlen(prev) + 1;
	if (prev >= ((char *)buf + buflen))
		return ((char *)0);
	return (prev);
}

int
prom_bounded_getprop(dnode_t nodeid, caddr_t name, caddr_t value, int len)
{
	struct prom_prop *propp;
	prom_node_t *pnp;
	int	plen;

	pnp = (prom_node_t *)nodeid;
	propp = get_prom_prop(pnp, name);
	if (propp == NULL) {
		return (-1);
	}
	plen = propp->pp_len;
	if (len < plen)
		plen = len;
	bcopy(propp->pp_val, value, plen);
	return (len);
}
#endif
