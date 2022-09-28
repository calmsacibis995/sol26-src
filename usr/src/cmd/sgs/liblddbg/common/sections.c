/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)sections.c	1.12	96/09/11 SMI"

/* LINTLIBRARY */

#include	"msg.h"
#include	"_debug.h"
#include	"libld.h"

/*
 * Error message string table.
 */
static const int order_errors[] = {
	MSG_ORDER_ERR_INFO_RANGE,
	MSG_ORDER_ERR_INFO_ORDER,
	MSG_ORDER_ERR_LINK_OUTRANGE,
	MSG_ORDER_ERR_FLAGS,
	MSG_ORDER_ERR_CYCLIC,
	MSG_ORDER_ERR_LINK_ERROR
};

void
Dbg_sec_in(Is_desc * isp)
{
	const char *	str;

	if (DBG_NOTCLASS(DBG_SECTIONS))
		return;

	if (isp->is_file != NULL)
		str = isp->is_file->ifl_name;
	else
		str = MSG_INTL(MSG_STR_NULL);

	dbg_print(MSG_INTL(MSG_SEC_INPUT), isp->is_name, str);
}

void
Dbg_sec_added(Os_desc * osp, Sg_desc * sgp)
{
	const char *	str;

	if (DBG_NOTCLASS(DBG_SECTIONS))
		return;

	if (sgp->sg_name && *sgp->sg_name)
		str = sgp->sg_name;
	else
		str = MSG_INTL(MSG_STR_NULL);

	dbg_print(MSG_INTL(MSG_SEC_ADDED), osp->os_name, str);
}

void
Dbg_sec_created(Os_desc * osp, Sg_desc * sgp)
{
	const char *	str;

	if (DBG_NOTCLASS(DBG_SECTIONS))
		return;

	if (sgp->sg_name && *sgp->sg_name)
		str = sgp->sg_name;
	else
		str = MSG_INTL(MSG_STR_NULL);

	dbg_print(MSG_INTL(MSG_SEC_CREATED), osp->os_name, str);
}

void
Dbg_sec_order_list(Ofl_desc *ofl, int flag)
{
	Os_desc *osp;
	Is_desc *isp;
	Is_desc *isp2;
	Listnode *lnp1;
	Listnode *lnp2;
	const char *str1;

	if (DBG_NOTCLASS(DBG_SECTIONS))
		return;
	if (DBG_NOTDETAIL())
		return;

	/*
	 * If the flag == 0, then the routine is called before sorting.
	 */
	if (flag == 0)
		str1 = MSG_INTL(MSG_ORDER_TO_BE_SORTED);
	else
		str1 = MSG_INTL(MSG_ORDER_SORTED);
	for (LIST_TRAVERSE(&ofl->ofl_ordered, lnp1, osp)) {
		dbg_print(str1, osp->os_name);
		dbg_print(MSG_INTL(MSG_ORDER_HDR_1),
			osp->os_sort->st_beforecnt,
			osp->os_sort->st_aftercnt,
			osp->os_sort->st_ordercnt);
		for (LIST_TRAVERSE(&osp->os_isdescs, lnp2, isp)) {
			if ((isp->is_flags & FLG_IS_ORDERED) == 0)
			    dbg_print(MSG_INTL(MSG_ORDER_TITLE_0),
			    isp->is_name, isp->is_file->ifl_name);
			else if (isp->is_shdr->sh_info == SHN_BEFORE)
			    dbg_print(MSG_INTL(MSG_ORDER_TITLE_1),
			    isp->is_name, isp->is_file->ifl_name);
			else if (isp->is_shdr->sh_info == SHN_AFTER)
			    dbg_print(MSG_INTL(MSG_ORDER_TITLE_2),
			    isp->is_name, isp->is_file->ifl_name);
			else {
			    isp2 =
			    isp->is_file->ifl_isdesc[isp->is_shdr->sh_info];
			    dbg_print(MSG_INTL(MSG_ORDER_TITLE_3),
			    isp->is_name,
			    isp->is_file->ifl_name,
			    isp2->is_name,
			    isp2->is_key);
			}

		}
	}
}

void
Dbg_sec_order_error(Ifl_desc *ifl, Word ndx, int error)
{
	if (DBG_NOTCLASS(DBG_SECTIONS))
		return;
	if (DBG_NOTDETAIL())
		return;

	if (error == 0)
		return;

	dbg_print(MSG_INTL(MSG_ORDER_ERR_TITLE),
		ifl->ifl_isdesc[ndx]->is_name,
		ifl->ifl_name);

	if (error)
		dbg_print(MSG_INTL(order_errors[error-1]));
}
