/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)dlfcns.c	1.65	96/10/08 SMI"

/*
 * Programmatic interface to the run_time linker.
 */
#include	"_synonyms.h"

#include	<string.h>
#include	<dlfcn.h>
#include	<synch.h>
#include	"profile.h"
#include	"debug.h"
#include	"_rtld.h"
#include	"_elf.h"
#include	"msg.h"

/* LINTLIBRARY */


/*
 * Determine who called us - given a pc determine in which object it resides.
 *
 * For dlopen() the link map of the caller must be passed to load_so() so that
 * the appropriate search rules (4.x or 5.0) are used to locate any
 * dependencies.  Also, if we've been called from a 4.x module it may be
 * necessary to fix the specified pathname so that it conforms with the 5.0 elf
 * rules.
 *
 * For dlsym() the link map of the caller is used to determine RTLD_NEXT
 * requests, together with requests based off of a dlopen(0).
 *
 * If we can't figure out the calling link map assume it's the executable.  It
 * is assumed that at least a readers lock is held when this function is called.
 */
static Rt_map *
_caller(unsigned long cpc)
{
	Lm_list *	lml, ** list;

	for (list = lml_list, lml = *list; lml; list++, lml = *list) {
		Rt_map *	tlmp;

		for (tlmp = lml->lm_head; tlmp; tlmp = (Rt_map *)NEXT(tlmp)) {
			if ((cpc > ADDR(tlmp)) &&
			    (cpc < (ADDR(tlmp) + MSIZE(tlmp))))
				return (tlmp);
		}
	}
	return ((Rt_map *)lml_list[0]->lm_head);
}

/*
 * Return a pointer to the string describing the last occurring error.  The
 * last occurring error is cleared.
 */
#pragma weak dlerror = _dlerror

char *
_dlerror()
{
	char *	_lasterr = lasterr;

	PRF_MCOUNT(20, _dlerror);

	lasterr = (char *)0;
	return (_lasterr);
}

/*
 * When creating a Dl_obj dependency on a non-promiscuous object, propagate the
 * callers permit to insure symbols can be found within the dlopen family.
 */
static int
_dlp_permit(Dl_obj * dlp, Rt_map * clmp, Rt_map * nlmp, int mode)
{
	Listnode *	lnp;
	Rt_map *	tlmp;

	PRF_MCOUNT(58, _dlp_permit);
	/*
	 * Make sure this link-map hasn't already been recorded from another
	 * dependency.
	 */
	for (LIST_TRAVERSE(&dlp->dl_depends, lnp, tlmp)) {
		if (nlmp == tlmp)
			return (1);
	}

	/*
	 * Append the new link-map to the callers dependencies.  If the new
	 * object is deletable increment its reference count.
	 */
	if (list_append(&dlp->dl_depends, nlmp) == 0)
		return (0);

	if (!(MODE(nlmp) & RTLD_NODELETE))
		COUNT(nlmp)++;

	/*
	 * If the new object isn't a parent propagate any necessary flags.
	 * Although a parent can export symbols to the objects that have been
	 * dlopen()'ed it should not take on any of the properties of the
	 * dlopen() request, as this will alter the parents ability to continue
	 * relocating itself.
	 *
	 * Although modes can be established when an object is first analyzed,
	 * or get promoted as part of dependency analysis (needed processing),
	 * we get here when creating a dlp for an object that is already mapped.
	 */
	if (!(mode & RTLD_PARENT)) {
		int	_mode = MODE(nlmp);
		MODE(nlmp) |= mode;

		if (!(_mode & RTLD_NOW) && (mode & RTLD_NOW)) {
			if ((LM_RELOC(nlmp)(nlmp, 1)) == 0)
				return (0);
		}
	}

	/*
	 * If the referenced object isn't promiscuous propagate the required
	 * permissions.  Chain the referenced object to this Dl_obj structure
	 * so that should it be removed the permit can be dereferenced and
	 * eventually freed.
	 */
	if ((clmp != nlmp) && ((mode & RTLD_GROUP) ||
	    !(MODE(nlmp) & RTLD_GLOBAL))) {
		if (dlp->dl_permit == 0) {
			if ((dlp->dl_permit = perm_get()) == 0)
				return (0);
		}
		PERMIT(nlmp) = perm_set(PERMIT(nlmp), dlp->dl_permit);

		if (list_append(&REFROM(nlmp), dlp) == 0)
			return (0);

		dlp->dl_permcnt++;
	}
	DBG_CALL(Dbg_file_bind_entry(clmp, nlmp));

	return (1);
}

/*
 * Create a new dlp for this dlopen'ed object.  This object maintains a list of
 * dependent (needed) link map structures, with the dlopen'ed object's link map
 * at the head of the list.  This object provides the `handle' returned from
 * dlopen() and its link-map list provides the means of satisfying dlsym()
 * requests.
 */
Dl_obj *
dlp_create(Rt_map * lmp, Rt_map * clmp, int mode)
{
	Dl_obj	*	dlp = DLP(lmp);
	Listnode *	lnp1, * lnp2;
	Rt_map *	tlmp1, * tlmp2 = lmp;
	int		_mode, noperm = 1;

	PRF_MCOUNT(21, dlp_create);

	/*
	 * Establish a mode without RTLD_PARENT for _dlp_permit() processing.
	 * This allows the correct propagation of flags to the objects acquired
	 * from the dlopen() without modifying the status of the parent.
	 */
	_mode = mode & ~RTLD_PARENT;

	/*
	 * If this is the first dlopen() request for this objects allocate and
	 * initialize a new dlp.
	 */
	if (dlp == 0) {
		if ((dlp = (Dl_obj *)calloc(sizeof (Dl_obj), 1)) == 0)
			return (0);
		DLP(lmp) = dlp;

		dlp->dl_magic = DL_MAGIC;
		dlp->dl_usercnt = 1;
		dlp->dl_cigam = DL_CIGAM;

		/*
		 * A link-map of FLG_RT_ISMAIN indicates a dlopen(0) - a dlsym()
		 * through this object will simply traverse the whole link-map
		 * list looking for promiscuous objects, thus there is no need
		 * to add more information to the Dl_obj structure.
		 */
		if (FLAGS(lmp) & FLG_RT_ISMAIN) {
			dlp->dl_magic = DL_DLOPEN_0;
			return (dlp);
		}

		DBG_CALL(Dbg_file_bind_title(REF_DLOPEN));

		/*
		 * Create a head node for the dependent link map list.  The head
		 * node holds the link map of this dlopen'ed object.  Note that
		 * at this point no unique permission is assigned to the object,
		 * this will be assigned later if any non-promiscuous
		 * dependencies exist.
		 */
		if (_dlp_permit(dlp, lmp, tlmp2, _mode) == 0) {
			(void) dlclose_core(lmp, dlp);
			return (0);
		}

		/*
		 * Special case for ld.so.1 (which is effectively dlopen()'ed as
		 * a filtee of libdl.so.1).  We don't want any of it's
		 * dependencies available to the user.
		 */
		if (lmp == lml_rtld.lm_head)
			return (dlp);

		/*
		 * Traverse this objects dependency tree to establish the
		 * link-map list of all objects available through this handle
		 * (Dl_obj).
		 */
		for (LIST_TRAVERSE(&dlp->dl_depends, lnp1, tlmp1)) {
			if (LASTDEP(tlmp1) == 0)
				continue;
			for (LIST_TRAVERSE(&DEPENDS(tlmp1), lnp2, tlmp2)) {
				if (_dlp_permit(dlp, lmp, tlmp2, _mode) == 0) {
					(void) dlclose_core(lmp, dlp);
					return (0);
				}

				/*
				 * A dependency list may include additional
				 * objects that have been bound, but were not
				 * explicit dependencies, these are not valid
				 * dlsym() objects.
				 */
				if (LASTDEP(tlmp1) == tlmp2)
					break;
			}
		}

		/*
		 * Signify that the dependencies so far gathered are all that
		 * are available for dlsym().  If RTLD_PARENT is in effect then
		 * the parent link-map will also be added to the `depends' list,
		 * however it is not available for dlsym().
		 */
		dlp->dl_lastdep = tlmp2;

	} else {
		int	now;

		/*
		 * If a Dl_obj already exists bump its reference count and
		 * insure any modes are propagated appropriately.
		 */
		dlp->dl_usercnt++;
		if (dlp->dl_permit)
			noperm = 0;

		for (LIST_TRAVERSE(&dlp->dl_depends, lnp1, tlmp1)) {
			now = (MODE(tlmp1) & RTLD_NOW);
			MODE(tlmp1) |= _mode;

			/*
			 * If the new mode indicates RTLD_NOW promote any
			 * RTLD_LAZY objects.
			 */
			if ((now == 0) && (_mode & RTLD_NOW)) {
				if ((LM_RELOC(tlmp1)(tlmp1, 1)) == 0)
					return (0);
			}
			if (dlp->dl_lastdep == tlmp1)
				break;

			DBG_CALL(Dbg_file_bind_title(REF_DLOPEN));
		}
	}

	/*
	 * If the mode of the dlopen() indicates RTLD_PARENT propagate the
	 * permissions accordingly.  This check is carried out after an existing
	 * dlp has been updated as it's possible for multiple dlopen()'s of the
	 * same object with different parents.
	 */
	if (mode & RTLD_PARENT) {
		if ((bound_add(REF_NEEDED, lmp, clmp) == 0) ||
		    (_dlp_permit(dlp, lmp, clmp, mode) == 0)) {
			(void) dlclose_core(lmp, dlp);
			return (0);
		}
	}

	/*
	 * If any _dlp_permit() operation required the generation of a permit,
	 * make sure that this is also assigned to the initial dlopen'ed object
	 * (this optimization results in a single dlopen'ed object not being
	 * assigned a permit, as an object is always allowed to look within
	 * itself for symbols - see lookup_sym()).
	 */
	if (dlp->dl_permit && noperm) {
		PERMIT(lmp) = perm_set(PERMIT(lmp), dlp->dl_permit);
		if (list_append(&REFROM(lmp), dlp) == 0)
			return (0);
		dlp->dl_permcnt++;

		DBG_CALL(Dbg_file_bind_entry(lmp, lmp));
	}

	return (dlp);
}

/*
 * Remove a Dl_obj structure. Free up any permit, and mark the memory as no
 * longer in use.
 */
void
dlp_free(Dl_obj * dlp)
{
	PRF_MCOUNT(107, dlp_free);

	if (dlp->dl_permit != 0)
		perm_free(dlp->dl_permit);

	dlp->dl_magic = 0;
	dlp->dl_cigam = 0;

	free(dlp);
}


/*
 * Open a shared object.  Three levels of dlopen are provided:
 *
 * _dlopen	is commonly called from the user (via libdl.so) and provides
 *		for argument verification, determination of the caller, and
 *		any cleanup necessary before return to the user.
 *
 * dlopen_lock	insures that the appropriate locks are obtained and released,
 *		and that any new init sections are called.
 *
 * dlopen_core	provides the basic underlying functionality.
 *
 * On success, returns a pointer (handle) to the structure containing
 * information about the newly added object, ie. can be used by dlsym(). On
 * failure, returns a null pointer.
 */
void *
dlopen_core(const char * path, int mode, Rt_map ** ilmp, Rt_map * clmp)
{
	Rt_map *	lmp;
	Dl_obj *	dlp;
	const char *	name;

	PRF_MCOUNT(22, dlopen_core);

	DBG_CALL(Dbg_file_dlopen((path ? path : MSG_ORIG(MSG_STR_ZERO)),
	    NAME(clmp), mode));

	/*
	 * If the path specified is null then we're operating on global
	 * objects so there is no need to process a dlp list (dlsym will
	 * simply traverse all objects looking for global entries).
	 */
	if (!path)
		return (dlp_create(LIST(clmp)->lm_head, clmp,
		    (mode & ~RTLD_NOLOAD)));

	/*
	 * If this object is already loaded simply insure that a Dl_obj
	 * structure has been created to describe it.
	 */
	if ((lmp = is_so_loaded(LIST(clmp), path)) != 0)
		return (dlp_create(lmp, clmp, (mode & ~RTLD_NOLOAD)));

	/*
	 * If a noload request was made and the object doesn't exist we're out
	 * of here.
	 */
	if (mode & RTLD_NOLOAD) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_OPEN), path,
		    strerror(ENOENT));
		return (0);
	}

	/*
	 * Fix the pathname if necessary and generate a new link map.
	 */
	if (LM_FIX_NAME(clmp))
		name = LM_FIX_NAME(clmp)(path);
	else
		name = path;

	if ((dlp = dl_new_so(name, clmp, ilmp, mode)) == 0) {
		rtld_flags |= RT_FL_DELNEEDED;
		return (0);
	}

	return ((void *)dlp);
}

void *
dlopen_lock(const char * path, int mode, Rt_map * clmp)
{
	void *		error;
	int		bind;
	Rt_map *	ilmp = 0;

	PRF_MCOUNT(23, dlopen_lock);

	if ((bind = bind_guard(THR_FLG_BIND)) == 1)
		(void) rw_wrlock(&bindlock);

	if ((error = dlopen_core(path, mode, &ilmp, clmp)) == 0) {
		if (!(rtld_flags & RT_FL_DELINPROG) &&
		    (rtld_flags & RT_FL_DELNEEDED))
			remove_so(LIST(clmp));
	}

	if (bind) {
		(void) rw_unlock(&bindlock);
		(void) bind_clear(THR_FLG_BIND);
	}

	/*
	 * After releasing any locks call any .init sections if necessary.
	 * If the dlopen was successful error will be non-zero, and if any
	 * new objects have been added to the link map list the initial
	 * link map pointer will be non-zero.
	 */
	if (error && ilmp)
		call_init(ilmp);
	return (error);
}

#pragma weak dlopen = _dlopen

void *
_dlopen(const char * path, int mode)
{
	void *	error;

	PRF_MCOUNT(24, _dlopen);

	/*
	 * Verify that a valid pathname has been supplied.
	 */
	if (path && (*path == '\0')) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_ILLPATH));
		return (0);
	}

	/*
	 * Historically we've always verified the mode is either RTLD_NOW or
	 * RTLD_LAZY.  RTLD_NOLOAD is valid by itself.
	 */
	if (((mode & (RTLD_NOW | RTLD_LAZY | RTLD_NOLOAD)) == 0) ||
	    ((mode & (RTLD_NOW | RTLD_LAZY)) == (RTLD_NOW | RTLD_LAZY))) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_ILLMODE), mode);
		return (0);
	}
	if (((mode & (RTLD_GROUP | RTLD_WORLD)) == 0) && !(mode & RTLD_NOLOAD))
		mode |= (RTLD_GROUP | RTLD_WORLD);

	/*
	 * Open the new object (if necessary) and establish the dlp list.  Clean
	 * up any temporary memory mappings and file descriptors.
	 */
	error = dlopen_lock(path, mode, _caller(caller()));
	cleanup();
	return (error);
}


/*
 * Open a new shared object, assign a dlp and propagate any dependencies.
 * Notice that the new link-map is written to the caller immediately from
 * load_so().  This link-map indicates where to start calling .init sections
 * from, and may also be a RELFM() for things like filters.  Thus if we're
 * processing a filtee it may need relocation, which in turn might cause
 * symbol lookup processing to re-enter elf_intp_find_sym().  By returning the
 * new link-map immediately, recursion in elf_intp_find_sym() is avoided.
 */
Dl_obj *
dl_new_so(const char * name, Rt_map * clmp, Rt_map ** ilmp, int mode)
{
	Dl_obj *	dlp;
	int		bindingstate = 0;

	PRF_MCOUNT(59, dl_new_so);
	/*
	 * Alert the debuggers that we are about to mess with the link-map and
	 * re-initialize the state in preparation for the debugger consistency
	 * call.
	 */
	if ((rtld_flags & RT_FL_DBNOTIF) == 0) {
		rtld_flags |= RT_FL_DBNOTIF;
		bindingstate = 1;
		rd_event(RD_DLACTIVITY, RT_ADD, rtld_db_dlactivity());
	}

	/*
	 * Load the new object.
	 */
	if ((*ilmp = load_so(LIST(clmp), name, clmp)) == 0) {
		if (bindingstate) {
			rd_event(RD_DLACTIVITY, RT_CONSISTENT,
				rtld_db_dlactivity());
			rtld_flags &= ~RT_FL_DBNOTIF;
		}
		return (0);
	}

	/*
	 * Make sure this really is a newly loaded object.  We may have gotten
	 * here to load `libfoo.so' and found that it is linked to another
	 * (already loaded) object, say `libfoo.so.1'. Or, when using filters
	 * protect against the possibility that a filtee's relocation recurses
	 * back to itself.
	 */
	if (FLAGS(*ilmp) & (FLG_RT_ANALYZED | FLG_RT_RELOCING)) {
		if (bindingstate) {
			rd_event(RD_DLACTIVITY, RT_CONSISTENT,
				rtld_db_dlactivity());
			rtld_flags &= ~RT_FL_DBNOTIF;
		}
		return (dlp_create(*ilmp, clmp, mode));
	}

	/*
	 * Establish initial mode for the loaded object.
	 */
	MODE(*ilmp) |= mode;

	/*
	 * Load any dependencies of this new link map.
	 */
	if (analyze_so(LIST(clmp), *ilmp) == 0) {
		*ilmp = 0;
		if (bindingstate) {
			rd_event(RD_DLACTIVITY, RT_CONSISTENT,
				rtld_db_dlactivity());
			rtld_flags &= ~RT_FL_DBNOTIF;
		}
		return (0);
	}

	/*
	 * Propagate permissions.
	 */
	if ((dlp = dlp_create(*ilmp, clmp, mode)) == 0) {
		*ilmp = 0;
		if (bindingstate) {
			rd_event(RD_DLACTIVITY, RT_CONSISTENT,
				rtld_db_dlactivity());
			rtld_flags &= ~RT_FL_DBNOTIF;
		}
		return (0);
	}

	/*
	 * Relocate anything thats been dragged in.  Note that we drop the
	 * reference count of the first loaded object - the relocate might have
	 * failed for one of its dependencies, and thus its reference count
	 * and analyzed state would not cause its removal along with the
	 * dependency that caused the problem.
	 */
	if (relocate_so(*ilmp) == 0) {
		(void) dlclose_core(*ilmp, DLP(*ilmp));
		*ilmp = 0;
		if (bindingstate) {
			rd_event(RD_DLACTIVITY, RT_CONSISTENT,
				rtld_db_dlactivity());
			rtld_flags &= ~RT_FL_DBNOTIF;
		}
		return (0);
	}

	/*
	 * Tell the debuggers we're ok again.
	 */
	if (bindingstate) {
		rd_event(RD_DLACTIVITY, RT_CONSISTENT, rtld_db_dlactivity());
		rtld_flags &= ~RT_FL_DBNOTIF;
	}
	return (dlp);
}

/*
 * Sanity check a program-provided dlp handle.
 */
static int
valid_handle(Dl_obj * dlp)
{
	PRF_MCOUNT(26, valid_handle);

	/*
	 * Just incase we get some arbitrary, odd-byte aligned handle, round it
	 * (prevents bus errors and alike).
	 */
	dlp = (Dl_obj *)S_ROUND(dlp, sizeof (int));

	if (dlp) {
		if (((dlp->dl_magic != DL_MAGIC) &&
		    (dlp->dl_magic != DL_DLOPEN_0)) ||
		    (dlp->dl_cigam != DL_CIGAM) ||
		    (dlp->dl_usercnt == 0) ||
		    (dlp->dl_permit && ((dlp->dl_depends.head == NULL) ||
		    (dlp->dl_depends.tail == NULL))))
			return (0);
		else
			return (1);
	}
	return (0);
}

/*
 * Search for a specified symbol.  Three levels of dlsym are provided:
 *
 * _dlsym	is commonly called from the user (via libdl.so) and provides
 *		for argument verification, determination of the caller, and
 *		any cleanup necessary before return to the user.
 *
 * dlsym_lock	insures that the appropriate locks are obtained and released.
 *
 * dlsym_core	provides the basic underlying functionality.
 *
 * On success, returns a the address of the specified symbol. On error returns
 * a null.
 */
void *
dlsym_core(void * handle, const char * name, Rt_map * clmp)
{
	Rt_map *	_lmp;
	Sym *		sym;

	PRF_MCOUNT(27, dlsym_core);

	if (handle == RTLD_NEXT) {
		/*
		 * If the handle is RTLD_NEXT simply start looking in the next
		 * link map from the callers.
		 */
		DBG_CALL(Dbg_syms_dlsym(NAME((Rt_map *)NEXT(clmp)), name, 1));

		/*
		 * Determine the permissions from the present link map, and
		 * start looking for symbols in the next link map.
		 */
		sym = LM_LOOKUP_SYM(clmp)(name, PERMIT(clmp), clmp,
		    (Rt_map *)NEXT(clmp), &_lmp, LKUP_DEFT);
	} else {
		Dl_obj *	dlp = (Dl_obj *)handle;

		/*
		 * Look in the shared object specified by that handle and in all
		 * objects in the specified object's needed list.
		 */
		DBG_CALL(Dbg_syms_dlsym((dlp->dl_magic == DL_DLOPEN_0) ?
		    NAME((Rt_map *)(LIST(clmp)->lm_head)) :
		    NAME((Rt_map *)(dlp->dl_depends.head->data)), name, 0));

		sym = LM_DLSYM(clmp)(dlp, clmp, name, &_lmp);
	}

	if (sym) {
		unsigned int	addr = sym->st_value;

		if (!(FLAGS(_lmp) & FLG_RT_FIXED))
			addr += ADDR(_lmp);

		DBG_CALL(Dbg_bind_global(NAME(clmp), 0, 0, -1, NAME(_lmp),
		    (caddr_t)addr, (caddr_t)sym->st_value, name));

		return ((void *)addr);
	} else
		return (0);
}

void *
dlsym_lock(void * handle, const char * name, Rt_map * clmp)
{
	void *	error;
	int	bind;

	PRF_MCOUNT(28, dlsym_lock);

	if ((bind = bind_guard(THR_FLG_BIND)) == 1)
		(void) rw_rdlock(&bindlock);
	error = dlsym_core(handle, name, clmp);
	if (bind) {
		(void) rw_unlock(&bindlock);
		(void) bind_clear(THR_FLG_BIND);
	}
	return (error);
}

#pragma weak dlsym = _dlsym

void *
_dlsym(void * handle, const char * name)
{
	void *	error;

	PRF_MCOUNT(29, _dlsym);

	/*
	 * Verify the arguments.
	 */
	if (name == 0) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_ILLSYM));
		return (0);
	}
	if ((handle != RTLD_NEXT) && (!valid_handle((Dl_obj *)handle))) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_INVHNDL));
		return (0);
	}

	/*
	 * Determine the symbols address.  Clean up any temporary memory
	 * mappings and file descriptors.
	 */
	error = dlsym_lock(handle, name, _caller(caller()));
	cleanup();
	if (error == 0)
		eprintf(ERR_FATAL, MSG_INTL(MSG_GEN_NOSYM), name);
	return (error);
}

/*
 * Close a shared object.  Three levels of dlclose are provided:
 *
 * _dlclose	is commonly called from the user (via libdl.so) and provides
 *		for handle verification and any cleanup necessary before return
 *		to the user.
 *
 * dlclose_lock	insures that the appropriate locks are obtained and released.
 *
 * dlclose_core	provides the basic underlying functionality.
 *
 * On success returns 0, and on failure 1.
 */
int
dlclose_core(Rt_map * lmp, Dl_obj * dlp)
{
	Rt_map *	nlmp;
	Listnode *	lnp, * olnp = 0;

	PRF_MCOUNT(90, dlclose_core);

	DBG_CALL(Dbg_file_dlclose((dlp->dl_magic == DL_DLOPEN_0) ?
	    MSG_ORIG(MSG_STR_ZERO) : NAME(lmp)));

	/*
	 * Decrement reference count of this object.
	 */
	if (--(dlp->dl_usercnt))
		return (0);

	DLP(lmp) = 0;

	/*
	 * This dlopen handle is no longer being referenced.  Traverse its
	 * dependency list decrementing the reference count of each object.
	 * if any objects reference count is 0 it is a candidate for removal.
	 */
	DBG_CALL(Dbg_file_bind_title(REF_DLCLOSE));
	for (LIST_TRAVERSE(&dlp->dl_depends, lnp, nlmp)) {
		if (!(MODE(nlmp) & RTLD_NODELETE)) {
			if (--COUNT(nlmp) == 0)
				rtld_flags |= RT_FL_DELNEEDED;
			DBG_CALL(Dbg_file_bind_entry(lmp, nlmp));
		}
		if (olnp)
			free(olnp);
		olnp = lnp;
	}
	if (olnp)
		free(olnp);

	return (0);
}

int
dlclose_lock(Dl_obj * dlp, Rt_map * clmp)
{
	int	error, bind;

	PRF_MCOUNT(91, dlclose_lock);

	if ((bind = bind_guard(THR_FLG_BIND)) == 1)
		(void) rw_wrlock(&bindlock);

	error = dlclose_core(clmp, dlp);

	if (!(rtld_flags & RT_FL_DELINPROG) &&
	    (rtld_flags & RT_FL_DELNEEDED))
		remove_so(LIST(clmp));

	if (bind) {
		(void) rw_unlock(&bindlock);
		(void) bind_clear(THR_FLG_BIND);
	}
	return (error);
}

#pragma weak dlclose = _dlclose

int
_dlclose(void * handle)
{
	int 		error;
	Dl_obj *	dlp = (Dl_obj *)handle;
	Rt_map *	clmp;

	PRF_MCOUNT(92, _dlclose);

	if (!valid_handle(dlp)) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_INVHNDL));
		return (1);
	}

	/*
	 * Determine which link-map we're going to be closing.
	 */
	if (dlp->dl_magic == DL_DLOPEN_0)
		clmp = LIST(_caller(caller()))->lm_head;
	else
		clmp = (Rt_map *)dlp->dl_depends.head->data;

	error = dlclose_lock(dlp, clmp);
	cleanup();
	return (error);
}

/*
 * Return an information structure that reflects the symbol closest to the
 * address specified.
 */
int
__dladdr(void * addr, Dl_info * dlip)
{
	Rt_map *	lmp, * tlmp;

	PRF_MCOUNT(93, __dladdr);

	/*
	 * Scan the executables link map list to determine which image covers
	 * the required address.
	 */
	lmp = 0;
	for (tlmp = lml_main.lm_head; tlmp; tlmp = (Rt_map *)NEXT(tlmp)) {
		Pnode *	pnp;

		if (((unsigned long)addr >= ADDR(tlmp)) &&
		    ((unsigned long)addr <= (ADDR(tlmp) + MSIZE(tlmp)))) {
			lmp = tlmp;
			break;
		}

		/*
		 * If the object has a filter check there as well (this may
		 * be ld.so.1, which is not maintained on the executables link
		 * map list).
		 */
		for (pnp = FILTEES(tlmp); pnp; pnp = pnp->p_next) {
			Rt_map *	rlmp = (Rt_map *)pnp->p_info;

			if (pnp->p_len == 0)
				continue;

			if (((unsigned long)addr >= ADDR(rlmp)) &&
			    ((unsigned long)addr <=
			    (ADDR(rlmp) + MSIZE(rlmp)))) {
				lmp = rlmp;
				break;
			}
		}
	}
	if (lmp == 0) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_INVADDR), addr);
		return (0);
	}

	/*
	 * Set up generic information and any defaults.
	 */
	dlip->dli_fname = NAME(lmp);
	dlip->dli_fbase = (void *)ADDR(lmp);
	dlip->dli_sname = 0;
	dlip->dli_saddr = 0;

	/*
	 * Determine the nearest symbol to this address.
	 */
	LM_DLADDR(lmp)((unsigned long)addr, lmp, dlip);
	return (1);
}

#pragma weak dladdr = _dladdr

int
_dladdr(void * addr, Dl_info * dlip)
{
	int	error;
	int	bind;

	PRF_MCOUNT(96, _dladdr);

	if ((bind = bind_guard(THR_FLG_BIND)) == 1)
		(void) rw_rdlock(&bindlock);
	error = __dladdr(addr, dlip);
	if (bind) {
		(void) rw_unlock(&bindlock);
		(void) bind_clear(THR_FLG_BIND);
	}
	return (error);
}


#pragma weak dldump = _dldump

int
_dldump(const char * ipath, const char * opath, int flags)
{
	int		error, bind, pfd = 0;
	Addr		addr = 0;
	Rt_map *	lmp;

	PRF_MCOUNT(94, _dldump);

	/*
	 * Verify any arguments first.
	 */
	if ((!opath || (*opath == '\0')) || (ipath && (*ipath == '\0'))) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_ILLPATH));
		return (1);
	}

	/*
	 * If an input file is specified make sure its one of our dependencies.
	 */
	if (ipath) {
		if ((bind = bind_guard(THR_FLG_BIND)) == 1)
			(void) rw_rdlock(&bindlock);
		lmp = is_so_loaded(&lml_main, ipath);
		if (bind) {
			(void) rw_unlock(&bindlock);
			(void) bind_clear(THR_FLG_BIND);
		}
		if (lmp == 0) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_GEN_NOFILE), ipath);
			return (1);
		}
		if (FLAGS(lmp) & FLG_RT_CACHED) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_GEN_CACHED), ipath);
			return (1);
		}
	} else
		lmp = lml_main.lm_head;


	DBG_CALL(Dbg_file_dldump(NAME(lmp), opath, flags));

	/*
	 * If we've been asked to process the dynamic object that started this
	 * process, then obtain a /proc file descriptor.
	 */
	if (lmp == lml_main.lm_head) {
		if ((pfd = pr_open()) == FD_UNAVAIL)
			return (1);
	}
	if (!(FLAGS(lmp) & FLG_RT_FIXED))
		addr = ADDR(lmp);

	/*
	 * Obtain the shared object necessary to perform the real dldump(),
	 * and the shared objects entry point.
	 */
	if (rtld_lib == 0) {
		if ((rtld_lib = dlused_core(MSG_ORIG(MSG_FIL_LIBRTLD),
		    (bind_mode | RTLD_GLOBAL | RTLD_WORLD),
		    (Rt_map *)lml_rtld.lm_head)) == 0)
			return (1);
	}

	/*
	 * Dump the required image.  Note that we're getting a writer lock even
	 * though we're really only traversing the link-maps to obtain things
	 * like relocation information.  Because we are bringing libelf() which
	 * requires thread interfaces that aren't all being offered by ld.so.1
	 * yet this seems a safer option.
	 */
	if ((bind = bind_guard(THR_FLG_BIND)) == 1)
		(void) rw_wrlock(&bindlock);
	error = rt_dldump(lmp, opath, pfd, flags, addr);
	if (bind) {
		(void) rw_unlock(&bindlock);
		(void) bind_clear(THR_FLG_BIND);
	}
	cleanup();
	return (error);
}


/*
 * Determine and open a USED dependency.  USED dependencies are established
 * during the link-edit phase by reassigning a shared object dependency using
 * a mapfile file control definition.  Any references to symbols within a USED
 * dependency will have the appropriate .plt infrastructure created.  However,
 * because the dependency itself is not loaded by default, it must effectively
 * be dlopen'ed prior to any symbol references being called.
 *
 * As any symbol reference causes a .plt binding to the USED dependency, these
 * dependencies cannot be closed (unless we come up with a way of cleaning up
 * the appropriate .plts and bound_to structure).  Thus, this mechanism is
 * suitable for loading objects that should remain in the address space.
 *
 * USED dependencies are searched for using the objects compilation environment
 * name.  Using this name the runtime environment name is determined from the
 * callers dynamic entries.  Any versioning requirements established during the
 * link-edit are also verified.
 */
void *
dlused_core(const char * cname, int mode, Rt_map * clmp)
{
	const char *	rname = cname;
	Dyn *		used;
	int		len = strlen(cname);
	Dl_obj *	dlp;

	PRF_MCOUNT(95, dlused_core);

	/*
	 * Scan the our dynamic section looking for a DT_USED match.
	 */
	for (used = (Dyn *)DYN(clmp); used->d_tag != DT_NULL; used++) {
		const char *	str;

		if (used->d_tag != DT_USED)
			continue;
		str = (const char *)STRTAB(clmp) + used->d_un.d_val;
		if (strncmp(str, cname, len) == 0) {
			rname = str;
			break;
		}
	}

	/*
	 * Open the specified object (if we didn't find a DT_USED match simply
	 * use the filename passed to us).
	 */
	if ((dlp = (Dl_obj *)dlopen_lock(rname, mode, clmp)) == 0)
		return (0);

	/*
	 * Verify any versioning requirements.
	 */
	if (!(rtld_flags & RT_FL_NOVERSION) && VERNEED(clmp)) {
		if (elf_vers(rname, clmp,
		    (Rt_map *)dlp->dl_depends.head->data) == 0) {
			(void) dlclose_lock(dlp, clmp);
			if (!(rtld_flags & RT_FL_DELINPROG) &&
			    (rtld_flags & RT_FL_DELNEEDED))
				remove_so(LIST(clmp));
			return (0);
		}
	}
	return (dlp);
}
