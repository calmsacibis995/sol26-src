/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)files.c	1.31	96/09/30 SMI"

/* LINTLIBRARY */

#include	"_synonyms.h"

#include	<string.h>
#include	<unistd.h>
#include	<fcntl.h>
#include	"msg.h"
#include	"_debug.h"
#include	"libld.h"

static int	bind_title = 0;

void
Dbg_file_generic(Ifl_desc * ifl)
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;

	dbg_print(MSG_INTL(MSG_FIL_BASIC), ifl->ifl_name,
		conv_etype_str(ifl->ifl_ehdr->e_type));
}

void
Dbg_file_skip(const char * nname, const char * oname)
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;

	if (oname)
		dbg_print(MSG_INTL(MSG_FIL_SKIP_1), nname, oname);
	else
		dbg_print(MSG_INTL(MSG_FIL_SKIP_2), nname);
}

void
Dbg_file_reuse(const char * nname, const char * oname)
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;

	dbg_print(MSG_INTL(MSG_FIL_REUSE), nname, oname);
}

/*
 * This function doesn't test for any specific debugging category, thus it will
 * be generated for any debugging family.
 */
void
Dbg_file_unused(const char * name)
{
	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_FIL_UNUSED), name);
	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
}

void
Dbg_file_archive(const char * name, int found)
{
	const char *	str;

	if (DBG_NOTCLASS(DBG_FILES))
		return;

	if (found)
		str = MSG_INTL(MSG_STR_AGAIN);
	else
		str = MSG_ORIG(MSG_STR_EMPTY);

	dbg_print(MSG_INTL(MSG_FIL_ARCHIVE), name, str);
}

void
Dbg_file_analyze(const char * name, int mode)
{
	int	_mode = mode;

	if (DBG_NOTCLASS(DBG_FILES))
		return;

	if (DBG_NOTDETAIL())
		_mode &= RTLD_GLOBAL;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_FIL_ANALYZE), name, conv_dlmode_str(_mode));

	bind_title = 0;
}

void
Dbg_file_aout(const char * name, unsigned long dynamic, unsigned long base,
	unsigned long size)
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;

	dbg_print(MSG_INTL(MSG_FIL_AOUT), name);
	dbg_print(MSG_INTL(MSG_FIL_DATA_1), dynamic, base, size);
}

void
Dbg_file_elf(const char * name, unsigned long dynamic, unsigned long base,
	unsigned long size, unsigned long entry, unsigned long phdr,
	unsigned int phnum)
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;

	dbg_print(MSG_INTL(MSG_FIL_ELF), name);
	dbg_print(MSG_INTL(MSG_FIL_DATA_1), dynamic, base, size);
	dbg_print(MSG_INTL(MSG_FIL_DATA_2), entry, phdr, phnum);
}

void
Dbg_file_ldso(const char * name, unsigned long dynamic, unsigned long base,
	unsigned long envp, unsigned long auxv)
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_FIL_LDSO), name);
	dbg_print(MSG_INTL(MSG_FIL_DATA_3), dynamic, base);
	dbg_print(MSG_INTL(MSG_FIL_DATA_4), envp, auxv);
	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
}

void
Dbg_file_prot(const char * name, int prot)
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_FIL_PROT), name, (prot ? '+' : '-'));
}

void
Dbg_file_delete(const char * name)
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_FIL_DELETE), name);
}

static int	bind_str = MSG_STR_EMPTY;

void
Dbg_file_bind_title(int type)
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;
	if (DBG_NOTDETAIL())
		return;

	bind_title = 1;

	/*
	 * Establish a binding title for later use in Dbg_file_bind_entry.
	 */
	if (type == REF_NEEDED)
		bind_str = MSG_FIL_BND_NEED;
	else if (type == REF_SYMBOL)
		bind_str = MSG_FIL_BND_SYM;
	else if (type == REF_DELETE)
		bind_str = MSG_FIL_BND_DELETE;
	else if (type == REF_DLOPEN)
		bind_str = MSG_FIL_BND_DLOPEN;
	else if (type == REF_DLCLOSE)
		bind_str = MSG_FIL_BND_DLCLOSE;
	else if (type == REF_UNPERMIT)
		bind_str = MSG_FIL_BND_UNPERMIT;
	else
		bind_title = 0;
}

void
Dbg_file_bind_entry(Rt_map * clmp, Rt_map * nlmp)
{
	Permit *	permit = PERMIT(nlmp);
	const char *	str;

	if (DBG_NOTCLASS(DBG_FILES))
		return;
	if (DBG_NOTDETAIL())
		return;

	/*
	 * If this is the first time here print out a binding title.
	 */
	if (bind_title) {
		dbg_print(MSG_ORIG(MSG_STR_EMPTY));
		dbg_print(MSG_INTL(bind_str), NAME(clmp));
		bind_title = 0;
	}

	if (MODE(nlmp) & RTLD_NODELETE)
		dbg_print(MSG_INTL(MSG_FIL_REFCNT_1), NAME(nlmp));
	else
		dbg_print(MSG_INTL(MSG_FIL_REFCNT_2), COUNT(nlmp), NAME(nlmp));

	if (MODE(nlmp) & RTLD_GLOBAL)
		str = MSG_INTL(MSG_FIL_GLOBAL);
	else
		str = MSG_ORIG(MSG_STR_EMPTY);

	if (permit) {
		unsigned long	_cnt, cnt = permit->p_cnt;
		unsigned long *	value = &permit->p_value[0];

		dbg_print(MSG_INTL(MSG_FIL_PERMIT_1), *value, str);
		for (_cnt = 1, value++; _cnt < cnt; _cnt++, value++)
			dbg_print(MSG_INTL(MSG_FIL_PERMIT_2), *value);
	} else
		dbg_print(MSG_INTL(MSG_FIL_PERMIT_3), str);
}

void
Dbg_file_bind_needed(Rt_map * clmp)
{
	Listnode *	lnp;
	Rt_map *	nlmp;

	if (DBG_NOTCLASS(DBG_FILES))
		return;
	if (DBG_NOTDETAIL())
		return;

	/*
	 * Traverse the callers explicit (needed) dependency list.
	 */
	Dbg_file_bind_title(REF_NEEDED);
	for (LIST_TRAVERSE(&DEPENDS(clmp), lnp, nlmp))
		Dbg_file_bind_entry(clmp, nlmp);
}

void
Dbg_file_dlopen(const char * name, const char * from, int mode)
{
	int	_mode = mode;

	if (DBG_NOTCLASS(DBG_FILES))
		return;

	if (DBG_NOTDETAIL())
		_mode &= RTLD_GLOBAL;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_FIL_DLOPEN), name, from, conv_dlmode_str(_mode));
}

void
Dbg_file_dlclose(const char * name)
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_FIL_DLCLOSE), name);
}

void
Dbg_file_dldump(const char * ipath, const char * opath, int flags)
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_FIL_DLDUMP), ipath, opath,
		conv_dlflag_str(flags));
}

void
Dbg_file_nl()
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;
	if (DBG_NOTDETAIL())
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
}

void
Dbg_file_preload(const char * name)
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;

	dbg_print(MSG_INTL(MSG_FIL_PRELOAD), name);
}

void
Dbg_file_needed(const char * name, const char * parent)
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_FIL_NEEDED), name, parent);
}

void
Dbg_file_filter(const char * name, const char * filter)
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;

	if (filter) {
		dbg_print(MSG_ORIG(MSG_STR_EMPTY));
		dbg_print(MSG_INTL(MSG_FIL_FILTER_1), name, filter);
	} else {
		dbg_print(MSG_INTL(MSG_FIL_FILTER_2), name);
		dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	}
}

void
Dbg_file_fixname(const char * oname, const char * nname)
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_FIL_FIXNAME), oname, nname);
}

void
Dbg_file_output(Ofl_desc * ofl)
{
	const char *	prefix = MSG_ORIG(MSG_PTH_OBJECT);
	char	*	oname, * nname, * ofile;
	int		fd;

	if (DBG_NOTCLASS(DBG_FILES))
		return;
	if (DBG_NOTDETAIL())
		return;

	/*
	 * Obtain the present input object filename for concatenation to the
	 * prefix name.
	 */
	oname = (char *)ofl->ofl_name;
	if ((ofile = strrchr(oname, '/')) == NULL)
		ofile = oname;
	else
		ofile++;

	/*
	 * Concatenate the prefix with the object filename, open the file and
	 * write out the present Elf memory image.  As this is debugging we
	 * ignore all errors.
	 */
	if ((nname = (char *)malloc(strlen(prefix) + strlen(ofile) + 1)) != 0) {
		(void) strcpy(nname, prefix);
		(void) strcat(nname, ofile);
		if ((fd = open(nname, O_RDWR | O_CREAT | O_TRUNC, 0666)) != -1)
			(void) write(fd, ofl->ofl_ehdr, ofl->ofl_size);
	}
}

void
Dbg_file_cache_dis(const char * cache, int flags)
{
	const char *	str;

	if (DBG_NOTCLASS(DBG_FILES))
		return;

	if (flags == DBG_SUP_PRCFAIL)
		str = MSG_INTL(MSG_FIL_CACHE_ERR_1);
	else if (flags == DBG_SUP_CORRUPT)
		str = MSG_INTL(MSG_FIL_CACHE_ERR_2);
	else if (flags == DBG_SUP_MAPINAP)
		str = MSG_INTL(MSG_FIL_CACHE_ERR_3);
	else if (flags == DBG_SUP_RESFAIL)
		str = MSG_INTL(MSG_FIL_CACHE_ERR_4);
	else
		str = 0;

	if (str) {
		dbg_print(MSG_ORIG(MSG_STR_EMPTY));
		dbg_print(MSG_INTL(MSG_FIL_CACHE_ERR), cache, str);
	}
}

void
Dbg_file_cache_obj(const char * name, const char * cache)
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;

	dbg_print(MSG_INTL(MSG_FIL_CACHE), name, cache);
}
