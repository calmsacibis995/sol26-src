/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All Rights Reserved.
 */
#pragma ident	"@(#)paths.c	1.38	96/09/09 SMI"

/*
 * PATH setup and search directory functions.
 */
#include	"_synonyms.h"

#include	<stdio.h>
#include	<limits.h>
#include	<fcntl.h>
#include	<string.h>
#include	<sys/systeminfo.h>
#include	"_rtld.h"
#include	"msg.h"
#include	"profile.h"
#include	"debug.h"

/*
 * Given a search rule type, return a list of directories to search according
 * to the specified rule.
 */
static Pnode *
get_dir_list(int rules, Rt_map * lmp)
{
	Pnode *	dirlist = (Pnode *)0;

	PRF_MCOUNT(46, get_dir_list);
	switch (rules) {
	case ENVDIRS:
		/*
		 * Initialize the environment variable (LD_LIBRARY_PATH) search
		 * path list.  Note, we always call Dbg_libs_path() so that
		 * every library lookup diagnostic can be preceeded with the
		 * appropriate search path information.
		 */
		if (envdirs) {
			DBG_CALL(Dbg_libs_path(envdirs));

			/*
			 * For ldd(1) -s, indicate the search paths that'll
			 * be used.  If this is a secure program then some
			 * search paths may be ignored, therefore reset the
			 * envlist pointer each time so that the diagnostics
			 * related to these unsecure directories will be
			 * output for each image loaded.
			 */
			if (rtld_flags & RT_FL_SEARCH)
				(void) printf(MSG_INTL(MSG_LDD_PTH_ENVDIR),
				    envdirs);
			if (envlist && (rtld_flags & RT_FL_SECURE) &&
			    ((rtld_flags & RT_FL_SEARCH) || dbg_mask)) {
				free(envlist);
				envlist = 0;
			}
			if (!envlist) {
				/*
				 * If this is a secure application we need to
				 * to be selective over what LD_LIBRARY_PATH
				 * directories we use.  Pass the list of
				 * trusted directories so that the appropriate
				 * security check can be carried out.
				 */
				envlist = make_pnode_list(envdirs, 1,
				    LM_SECURE_DIRS(LIST(lmp)->lm_head), lmp);
			}
			dirlist = envlist;
		}
		break;
	case RUNDIRS:
		/*
		 * Initialize the runpath search path list.  To be consistant
		 * with the debugging display of ENVDIRS (above), always call
		 * Dbg_libs_rpath().
		 */
		if (RPATH(lmp)) {
			DBG_CALL(Dbg_libs_rpath(NAME(lmp), RPATH(lmp)));

			/*
			 * For ldd(1) -s, indicate the search paths that'll
			 * be used.  If this is a secure program then some
			 * search paths may be ignored, therefore reset the
			 * runlist pointer each time so that the diagnostics
			 * related to these unsecure directories will be
			 * output for each image loaded.
			 */
			if (rtld_flags & RT_FL_SEARCH)
				(void) printf(MSG_INTL(MSG_LDD_PTH_RPATH),
				    RPATH(lmp), NAME(lmp));
			if (RLIST(lmp) && (rtld_flags & RT_FL_SECURE) &&
			    ((rtld_flags & RT_FL_SEARCH) || dbg_mask)) {
				free(RLIST(lmp));
				RLIST(lmp) = 0;
			}
			if (!(RLIST(lmp)))
				RLIST(lmp) = make_pnode_list(RPATH(lmp), 1,
				    0, lmp);
			dirlist = RLIST(lmp);
		}
		break;
	case DEFAULT:
		dirlist = LM_DFLT_DIRS(lmp);
		/*
		 * For ldd(1) -s, indicate the default paths that'll be used.
		 */
		if (dirlist && ((rtld_flags & RT_FL_SEARCH) || dbg_mask)) {
			Pnode *	pnp = dirlist;

			if (rtld_flags & RT_FL_SEARCH)
				(void) printf(MSG_INTL(MSG_LDD_PTH_BGNDFL));
			for (; pnp && pnp->p_name; pnp = pnp->p_next) {
				if (rtld_flags & RT_FL_SEARCH)
					(void)
					    printf(MSG_ORIG(MSG_LDD_FMT_FILE),
					    pnp->p_name);
				else
					DBG_CALL(Dbg_libs_dpath(pnp->p_name));
			}
			if (rtld_flags & RT_FL_SEARCH)
				(void) printf(MSG_INTL(MSG_LDD_PTH_ENDDFL));
		}
		break;
	default:
		break;
	}
	return (dirlist);
}

/*
 * Get the next dir in the search rules path.
 */
Pnode *
get_next_dir(Pnode ** dirlist, Rt_map * lmp)
{
	static int *	rules = NULL;

	PRF_MCOUNT(45, get_next_dir);
	/*
	 * Search rules consist of one or more directories names. If this is a
	 * new search, then start at the beginning of the search rules.
	 * Otherwise traverse the list of directories that make up the rule.
	 */
	if (!*dirlist) {
		rules = LM_SEARCH_RULES(lmp);
	} else {
		if ((*dirlist = (*dirlist)->p_next) != 0)
			return (*dirlist);
		else
			rules++;
	}

	while (*rules) {
		if ((*dirlist = get_dir_list(*rules, lmp)) != 0)
			return (*dirlist);
		else
			rules++;
	}

	/*
	 * If we got here, no more directories to search, return NULL.
	 */
	return ((Pnode *) NULL);
}

/*
 * Process a directory (runpath) or filename (needed or filter) string looking
 * for tokens to expand.  The `new' flag indicates whether a new string should
 * be created.  This is typical for multiple runpath strings, as these are
 * colon separated and must be null separated to facilitate the creation of
 * a search path list.  Note that if token expansion occurs a new string must be
 * produced.
 */
int
expand(const char ** name, int * len, Rt_map * lmp, int new)
{
	char		_name[PATH_MAX];
	const char *	optr, * _optr;
	char *		nptr;
	int		olen = 0, nlen = 0, nrem = PATH_MAX, _len, _expand;

	PRF_MCOUNT(47, expand);

	optr = _optr = *name;
	nptr = _name;

	while ((olen < *len) && nrem) {
		if ((*optr != '$') || ((olen - *len) == 1)) {
			olen++, optr++, nrem--;
			continue;
		}

		/*
		 * Copy any string we've presently passed over to the new
		 * buffer.
		 */
		if ((_len = (optr - _optr)) != 0) {
			(void) strncpy(nptr, _optr, _len);
			nptr += _len;
			nlen += _len;
		}

		/*
		 * Skip the token delimiter and determine if a reserved token
		 * match is found.
		 */
		olen++, optr++;
		_expand = 0;

#ifdef	AT_SUN_EXECNAME
		if (strncmp(optr, MSG_ORIG(MSG_TKN_ORIGIN),
		    MSG_TKN_ORIGIN_SIZE) == 0) {
			const char *	str;

			/*
			 * $ORIGIN expansion is required.  Determine this
			 * objects basename.
			 */
			if ((str = DIRNAME(lmp)) != 0) {
				const char *	_str;

				if ((_len = DIRSZ(lmp)) == 0) {
					if ((_str = strrchr(str, '/')) != NULL)
						DIRSZ(lmp) = _len = _str - str;
				}
				if (nrem > _len) {
					(void) strncpy(nptr,
					    DIRNAME(lmp), _len);
					nptr += _len;
					nlen += _len;
					nrem -= _len;
					olen += MSG_TKN_ORIGIN_SIZE;
					optr += MSG_TKN_ORIGIN_SIZE;
					new = _expand = 1;
				}
			}
		} else if (strncmp(optr, MSG_ORIG(MSG_TKN_ISA),
		    MSG_TKN_ISA_SIZE) == 0) {
			/*
			 * $ISA expansion required. This would have been
			 * established from the AT_SUN_???? aux vector, but if
			 * not attempt to get it from sysconf().
			 */
			if ((isa == (char *)0) && (isa_sz != -1)) {
				char	_info[SYS_NMLN];

				isa_sz = (int)sysinfo((int)SI_ARCHITECTURE,
				    _info, SYS_NMLN);
				if ((isa_sz != -1) &&
				    (isa = (char *)malloc(isa_sz))) {
					(void) strcpy(isa, _info);
					isa_sz--;
				}
			}
			if (isa && (nrem > isa_sz)) {
				(void) strncpy(nptr, isa, isa_sz);
				nptr += isa_sz;
				nlen += isa_sz;
				nrem -= isa_sz;
				olen += MSG_TKN_ISA_SIZE;
				optr += MSG_TKN_ISA_SIZE;
				new = _expand = 1;
			}
		} else
#endif
		if (strncmp(optr, MSG_ORIG(MSG_TKN_PLATFORM),
		    MSG_TKN_PLATFORM_SIZE) == 0) {
			/*
			 * $PLATFORM expansion required.  This would have been
			 * established from the AT_SUN_PLATFORM aux vector, but
			 * if not attempt to get it from sysconf().
			 */
			if ((platform == (char *)0) && (platform_sz != -1)) {
				char	_info[SYS_NMLN];

				platform_sz = (int)sysinfo((int)SI_PLATFORM,
				    _info, SYS_NMLN);
				if ((platform_sz != -1) &&
				    (platform = (char *)malloc(platform_sz))) {
					(void) strcpy(platform, _info);
					platform_sz--;
				}
			}
			if (platform && (nrem > platform_sz)) {
				(void) strncpy(nptr, platform, platform_sz);
				nptr += platform_sz;
				nlen += platform_sz;
				nrem -= platform_sz;
				olen += MSG_TKN_PLATFORM_SIZE;
				optr += MSG_TKN_PLATFORM_SIZE;
				new = _expand = 1;
			}
		}

		/*
		 * No reserved token has been found, or its expansion isn't
		 * possible.  Replace the token delimiter.
		 */
		if (_expand == 0) {
			*nptr++ = '$';
			nlen++, nrem--;
		}
		_optr = optr;
	}

	/*
	 * If a copy of the string isn't required, and no token expansion has
	 * occurred, we can return now.
	 */
	if (new == 0)
		return (1);

	/*
	 * Copy any remaining string. Terminate the new string with a null as
	 * this string can be displayed via debugging diagnostics.
	 */
	if (((_len = (optr - _optr)) != 0) && (nrem > _len)) {
		(void) strncpy(nptr, _optr, _len);
		nptr += _len;
		nlen += _len;
	}
	*nptr = '\0';

	/*
	 * Allocate permanent storage for the new string and return to the user.
	 */
	if ((nptr = (char *)malloc(nlen + 1)) == 0)
		return (0);
	(void) strcpy(nptr, _name);
	*name = nptr;
	*len = nlen;

	return (1);
}

/*
 * Take a colon separated file or path specification and build a list of Pnode
 * structures.  Each string is passed to expand() for possible token expansion.
 * Regardless of any token expansion occuring, a newly malloc()'ed, null
 * terminated string is returned.
 */
Pnode *
make_pnode_list(const char * list, int secure, Pnode * sdir, Rt_map * lmp)
{
	int		len;
	const char *	str;
	Pnode *		pnp, * npnp, * opnp;

	PRF_MCOUNT(48, make_pnode_list);

	for (pnp = 0, opnp = 0, str = list; *list; str = list) {
		if (*list == ';')
			++list;
		if (*list == ':') {
			str = MSG_ORIG(MSG_FMT_CWD);
			len = MSG_FMT_CWD_SIZE;
		} else {
			len = 0;
			while (*list && (*list != ':') && (*list != ';')) {
				list++, len++;
			}
		}
		if (*list)
			list++;

		/*
		 * Expand the captured string.
		 */
		if (str != MSG_ORIG(MSG_FMT_CWD)) {
			if (expand(&str, &len, lmp, 1) == 0)
				return ((Pnode *)0);
		}

		/*
		 * If we're only allowed to recognize secure paths make sure
		 * that the path just processed is valid.
		 */
		if (secure && (rtld_flags & RT_FL_SECURE)) {
			Pnode *		_sdir;
			int		ok = 0;

			if (*str == '/') {
				if (sdir) {
					for (_sdir = sdir;
					    (_sdir && _sdir->p_name);
					    _sdir = _sdir->p_next) {
						if (strcmp(str,
						    _sdir->p_name) == 0) {
							ok = 1;
							break;
						}
					}
				} else
					ok = 1;
			}
			if (!ok) {
				DBG_CALL(Dbg_libs_ignore(str));
				if (rtld_flags & RT_FL_SEARCH)
					(void)
					    printf(MSG_INTL(MSG_LDD_PTH_IGNORE),
						str);
				if (str != MSG_ORIG(MSG_FMT_CWD))
					free((void *)str);
				continue;
			}
		}

		/*
		 * Allocate a new Pnode for this string.
		 */
		if ((npnp = (Pnode *)malloc(sizeof (Pnode))) == 0)
			return ((Pnode *)0);
		if (opnp == 0)
			pnp = npnp;
		else
			opnp->p_next = npnp;

		npnp->p_name = str;
		npnp->p_len = len;
		npnp->p_info = 0;
		npnp->p_next = 0;

		opnp = npnp;
	}
	return (pnp);
}
