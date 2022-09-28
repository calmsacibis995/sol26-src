/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)rtld_db.c	1.9	96/09/09 SMI"


#include	<stdlib.h>
#include	<stdio.h>
#include	<proc_service.h>
#include	<link.h>
#include	<rtld_db.h>
#include	"rtld.h"
#include	"_rtld_db.h"
#include	"msg.h"

/* LINTLIBRARY */


/*
 * Mutex to protect global data
 */
mutex_t	glob_mutex = DEFAULTMUTEX;
int	rtld_db_version = 0;
int	rtld_db_logging = 0;


void
rd_log(const int on_off)
{
	mutex_lock(&glob_mutex);
	rtld_db_logging = on_off;
	mutex_unlock(&glob_mutex);
	LOG(ps_plog(MSG_ORIG(MSG_DB_LOGENABLE)));
}

rd_err_e
rd_init(int version)
{
	if (version != RD_VERSION)
		return (RD_NOCAPAB);
	rtld_db_version = version;
	return (RD_OK);
}


rd_err_e
rd_reset(struct rd_agent *rap)
{
	psaddr_t			symaddr;
	const struct ps_prochandle *	php = rap->rd_psp;
	Rtld_db_priv			db_priv;

	RDAGLOCK(rap);

	rap->rd_flags = 0;

	/*
	 * Load in location of private symbols
	 */
	if (ps_pglobal_lookup(php, PS_OBJ_LDSO, MSG_ORIG(MSG_SYM_RTLDDBPV),
	    &symaddr) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_LOOKFAIL),
			MSG_ORIG(MSG_SYM_RTLDDBPV)));
		RDAGUNLOCK(rap);
		return (RD_DBERR);
	}

	rap->rd_rtlddbpriv = symaddr;

	if (ps_pglobal_lookup(php, PS_OBJ_LDSO, MSG_ORIG(MSG_SYM_DEBUG),
	    &symaddr) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_LOOKFAIL),
			MSG_ORIG(MSG_SYM_DEBUG)));
		RDAGUNLOCK(rap);
		return (RD_DBERR);
	}

	rap->rd_rdebug = symaddr;


	if (ps_pglobal_lookup(php, PS_OBJ_LDSO, MSG_ORIG(MSG_SYM_PREINIT),
	    &symaddr) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_LOOKFAIL),
		    MSG_ORIG(MSG_SYM_PREINIT)));
		RDAGUNLOCK(rap);
		return (RD_DBERR);
	}

	rap->rd_preinit = symaddr;

	if (ps_pglobal_lookup(php, PS_OBJ_LDSO, MSG_ORIG(MSG_SYM_POSTINIT),
	    &symaddr) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_NOFINDRTLD),
		    MSG_ORIG(MSG_SYM_POSTINIT)));
		RDAGUNLOCK(rap);
		return (RD_DBERR);
	}
	rap->rd_postinit = symaddr;

	if (ps_pglobal_lookup(php, PS_OBJ_LDSO, MSG_ORIG(MSG_SYM_DLACT),
	    &symaddr) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_NOFINDRTLD),
		    MSG_ORIG(MSG_SYM_DLACT)));
		RDAGUNLOCK(rap);
		return (RD_DBERR);
	}
	rap->rd_dlact = symaddr;
	rap->rd_tbinder = 0;

	/*
	 * Verify that librtld_db & rtld are at the proper revision
	 * levels.
	 */
	if (ps_pread(php, rap->rd_rtlddbpriv, (char *)&db_priv,
	    sizeof (Rtld_db_priv)) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_READPRIVFAIL_1),
			rap->rd_rtlddbpriv));
		return (RD_DBERR);
	}

	if (db_priv.rtd_version != R_RTLDDB_VERSION) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_BADPVERS),
			db_priv.rtd_version, R_RTLDDB_VERSION));
		return (RD_NOCAPAB);
	}

	/*
	 * Is the image being examined from a core file or not.
	 * If it is a core file then the following write will fail.
	 */
	if (ps_pwrite(php, rap->rd_rtlddbpriv, (char *)&db_priv,
	    sizeof (Rtld_db_priv)) != PS_OK)
		rap->rd_flags |= RDF_FL_COREFILE;

	RDAGUNLOCK(rap);
	return (RD_OK);
}

rd_agent_t *
rd_new(struct ps_prochandle * php)
{
	rd_agent_t *		rap;
	if ((rap = (rd_agent_t *)malloc(sizeof (rd_agent_t))) == NULL)
		return (0);
	rap->rd_psp = php;
	(void) mutex_init(&rap->rd_mutex, USYNC_THREAD, 0);
	if (rd_reset(rap) != RD_OK) {
		RDAGUNLOCK(rap);
		free(rap);
		LOG(ps_plog(MSG_ORIG(MSG_DB_RESETFAIL)));
		return ((rd_agent_t *)0);
	}
	return (rap);
}


void
rd_delete(rd_agent_t * rap)
{
	free(rap);
}


static rd_err_e
iter_map(rd_agent_t * rap, unsigned ident, psaddr_t lmaddr,
	rl_iter_f * cb, void * client_data)
{
	while (lmaddr) {
		Rt_map		rmap;
		rd_loadobj_t	lobj;
		Ehdr		ehdr;
		Phdr		phdr;
		int		i;
		ulong_t		off;

		if (ps_pread(rap->rd_psp, lmaddr, (char *)&rmap,
		    sizeof (Rt_map)) != PS_OK) {
			LOG(ps_plog(MSG_ORIG(MSG_DB_LKMAPFAIL)));
			return (RD_DBERR);
		}

		lobj.rl_nameaddr = (psaddr_t)NAME(&rmap);
		lobj.rl_refnameaddr = (psaddr_t)REFNAME(&rmap);
		lobj.rl_flags = 0;
		lobj.rl_base = (psaddr_t)ADDR(&rmap);
		lobj.rl_lmident = ident;
		lobj.rl_bend = ADDR(&rmap) + MSIZE(&rmap);
		lobj.rl_padstart = PADSTART(&rmap);
		lobj.rl_padend = PADSTART(&rmap) + PADIMLEN(&rmap);

		/*
		 * Look for beginning of data segment.
		 *
		 * NOTE: the data segment can only be found for full
		 *	processes and not from core images.
		 */
		lobj.rl_data_base = 0;
		if (rap->rd_flags & RDF_FL_COREFILE)
			lobj.rl_data_base = 0;
		else {
			off = ADDR(&rmap);
			if (ps_pread(rap->rd_psp, off, (char *)&ehdr,
			    sizeof (Ehdr)) != PS_OK) {
				LOG(ps_plog(MSG_ORIG(MSG_DB_LKMAPFAIL)));
				return (RD_DBERR);
			}
			off += sizeof (Ehdr);
			for (i = 0; i < ehdr.e_phnum; i++) {
				if (ps_pread(rap->rd_psp, off, (char *)&phdr,
				    sizeof (Phdr)) != PS_OK) {
					LOG(ps_plog(MSG_ORIG(
					    MSG_DB_LKMAPFAIL)));
					return (RD_DBERR);
				}
				if ((phdr.p_type == PT_LOAD) &&
				    (phdr.p_flags & PF_W)) {
					lobj.rl_data_base = phdr.p_vaddr;
					if (ehdr.e_type == ET_DYN)
						lobj.rl_data_base +=
							ADDR(&rmap);
					break;
				}
				off += ehdr.e_phentsize;
			}
		}


		/*
		 * When we transfer control to the client we free the
		 * lock and re-atain it after we've returned from the
		 * client.  This is to avoid any deadlock situations.
		 */
		RDAGUNLOCK(rap);
		if ((*cb)(&lobj, client_data) == 0) {
			RDAGLOCK(rap);
			break;
		}
		RDAGLOCK(rap);
		lmaddr = (psaddr_t)NEXT(&rmap);
	}
	return (RD_OK);
}


rd_err_e
rd_loadobj_iter(rd_agent_t * rap, rl_iter_f * cb, void * client_data)
{
	struct r_debug	rdebug;
	rd_err_e		rc;



	RDAGLOCK(rap);
	if (ps_pread(rap->rd_psp, rap->rd_rdebug, (char *)&rdebug,
	    sizeof (struct r_debug)) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_READDBGFAIL_1), rap->rd_rdebug));
		RDAGUNLOCK(rap);
		return (RD_DBERR);
	}

	if ((rdebug.r_map == 0) || (rdebug.r_ldsomap == 0)) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_LKMAPNOINIT), rdebug.r_map,
			rdebug.r_ldsomap));
		RDAGUNLOCK(rap);
		return (RD_NOMAPS);
	}

	if (cb == 0) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_NULLITER)));
		RDAGUNLOCK(rap);
		return (RD_ERR);
	}


	if ((rc = iter_map(rap, LM_ID_BASE, (psaddr_t)rdebug.r_map,
	    cb, client_data)) != RD_OK) {
		RDAGUNLOCK(rap);
		return (rc);
	}

	rc = iter_map(rap, LM_ID_LDSO, (psaddr_t)rdebug.r_ldsomap,
		cb, client_data);

	RDAGUNLOCK(rap);
	return (rc);
}



rd_err_e
rd_event_addr(rd_agent_t * rap, rd_event_e num, rd_notify_t * np)
{
	rd_err_e	rc = RD_OK;

	RDAGLOCK(rap);
	switch (num) {
	case RD_NONE:
		rc = RD_OK;
		break;
	case RD_PREINIT:
		np->type = RD_NOTIFY_BPT;
		np->u.bptaddr = rap->rd_preinit;
		break;
	case RD_POSTINIT:
		np->type = RD_NOTIFY_BPT;
		np->u.bptaddr = rap->rd_postinit;
		break;
	case RD_DLACTIVITY:
		np->type = RD_NOTIFY_BPT;
		np->u.bptaddr = rap->rd_dlact;
		break;
	default:
		LOG(ps_plog(MSG_ORIG(MSG_DB_UNEXPEVENT), num));
		rc = RD_ERR;
		break;
	}

	RDAGUNLOCK(rap);
	return (rc);
}


/* ARGSUSED 0 */
rd_err_e
rd_event_enable(rd_agent_t * rap, int onoff)
{
	const struct ps_prochandle *	php = rap->rd_psp;
	struct r_debug			rdb;

	RDAGLOCK(rap);
	/*
	 * Tell the debugged process that debugging is occuring
	 * This will enable the storing of event messages so that
	 * the can be gathered by the debugger.
	 */
	if (ps_pread(php, rap->rd_rdebug, (char *)&rdb,
	    sizeof (struct r_debug)) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_READFAIL_1), &rdb));
		RDAGUNLOCK(rap);
		return (RD_DBERR);
	}

	if (onoff)
		rdb.r_flags |= RD_FL_DBG;
	else
		rdb.r_flags &= ~RD_FL_DBG;

	if (ps_pwrite(php, rap->rd_rdebug, (char *)&rdb,
	    sizeof (struct r_debug)) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_WRITEFAIL_1), &rdb));
		RDAGUNLOCK(rap);
		return (RD_DBERR);
	}
	RDAGUNLOCK(rap);
	return (RD_OK);
}

rd_err_e
rd_event_getmsg(rd_agent_t * rap, rd_event_msg_t * emsg)
{
	struct r_debug	rdb;

	RDAGLOCK(rap);
	if (ps_pread(rap->rd_psp, rap->rd_rdebug, (char *)&rdb,
	    sizeof (struct r_debug)) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_READDBGFAIL_2), rap->rd_rdebug));
		RDAGUNLOCK(rap);
		return (RD_DBERR);
	}
	emsg->type = rdb.r_rdevent;
	if (emsg->type == RD_DLACTIVITY) {
		switch (rdb.r_state) {
			case RT_CONSISTENT:
				emsg->u.state = RD_CONSISTENT;
				break;
			case RT_ADD:
				emsg->u.state = RD_ADD;
				break;
			case RT_DELETE:
				emsg->u.state = RD_DELETE;
				break;
		}
	} else
		emsg->u.state = RD_NOSTATE;

	RDAGUNLOCK(rap);
	return (RD_OK);
}


rd_err_e
rd_binder_exit_addr(struct rd_agent * rap, psaddr_t * beaddr)
{
	Elf32_Sym	sym;

	if (rap->rd_tbinder) {
		*beaddr = rap->rd_tbinder;
		return (RD_OK);
	}
	if (ps_pglobal_sym(rap->rd_psp, PS_OBJ_LDSO, MSG_ORIG(MSG_SYM_RTBIND),
	    &sym) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_UNFNDSYM),
		    MSG_ORIG(MSG_SYM_RTBIND)));
		return (RD_ERR);
	}
	rap->rd_tbinder = *beaddr = sym.st_value + sym.st_size - M_BIND_ADJ;
	return (RD_OK);
}


rd_err_e
rd_objpad_enable(struct rd_agent * rap, size_t padsize)
{
	Rtld_db_priv			db_priv;
	const struct ps_prochandle *	php = rap->rd_psp;

	RDAGLOCK(rap);
	if (ps_pread(php, rap->rd_rtlddbpriv, (char *)&db_priv,
	    sizeof (Rtld_db_priv)) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_READFAIL_3), rap->rd_rtlddbpriv));
		RDAGUNLOCK(rap);
		return (RD_DBERR);
	}
	db_priv.rtd_objpad = padsize;
	if (ps_pwrite(php, rap->rd_rtlddbpriv, (char *)&db_priv,
	    sizeof (Rtld_db_priv)) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_WRITEFAIL_2), rap->rd_rtlddbpriv));
		RDAGUNLOCK(rap);
		return (RD_DBERR);
	}
	RDAGUNLOCK(rap);
	return (RD_OK);
}


char *
rd_errstr(rd_err_e rderr)
{
	/*
	 * Convert an 'rd_err_e' to a string
	 */
	switch (rderr) {
	case RD_OK:
		return ((char *)MSG_ORIG(MSG_ER_OK));
	case RD_ERR:
		return ((char *)MSG_ORIG(MSG_ER_ERR));
	case RD_DBERR:
		return ((char *)MSG_ORIG(MSG_ER_DBERR));
	case RD_NOCAPAB:
		return ((char *)MSG_ORIG(MSG_ER_NOCAPAB));
	case RD_NODYNAM:
		return ((char *)MSG_ORIG(MSG_ER_NODYNAM));
	case RD_NOBASE:
		return ((char *)MSG_ORIG(MSG_ER_NOBASE));
	case RD_NOMAPS:
		return ((char *)MSG_ORIG(MSG_ER_NOMAPS));
	default:
		return ((char *)MSG_ORIG(MSG_ER_DEFAULT));
	}
}
