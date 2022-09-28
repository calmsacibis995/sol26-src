/*
 *	cache_api.cc
 *
 *	Copyright (c) 1996 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)cache_api.cc	1.2	96/10/15 SMI"

/* The C interface to directory cache class functions */


#include "../../rpc/rpc_mt.h"
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>

#include <rpc/rpc.h>
#include <rpc/types.h>
#include <rpcsvc/nis.h>
#include "../gen/nis_local.h"
#include "local_cache.h"
#include "client_cache.h"
#include "mgr_cache.h"

static mutex_t cur_cache_lock = DEFAULTMUTEX;
static NisCache *cur_cache = NULL;	/* protected by cur_cache_lock */
static NisMgrCache *mgr_cache = NULL;	/* protected by cur_cache_lock */
static int checked_env = 0;		/* protected by cur_cache_lock */

FILE *__nis_debug_file = stdout;
int __nis_debug_bind;
int __nis_debug_rpc;
int __nis_debug_calls;
char *__nis_prefsrv;
char *__nis_preftype;

#define	OPT_INT 1
#define	OPT_STRING 2
#define	OPT_FILE 3

struct option {
	char *name;
	int type;
	void *address;
};

option options[] = {
	{ "debug_file", OPT_FILE, &__nis_debug_file },
	{ "debug_bind", OPT_INT, &__nis_debug_bind },
	{ "debug_rpc", OPT_INT, &__nis_debug_rpc },
	{ "debug_calls", OPT_INT, &__nis_debug_calls },
	{ PREF_SRVR, OPT_STRING, &__nis_prefsrv },
	{ PREF_TYPE, OPT_STRING, &__nis_preftype },
	{ 0, 0, 0 },
};

static
void
set_option(char *name, char *val)
{
	option *opt;
	int n;
	char *p;
	FILE *fp;

	for (opt = options; opt->name; opt++) {
		if (strcmp(name, opt->name) == 0) {
			switch (opt->type) {
			    case OPT_STRING:
				p = strdup(val);
				*((char **)opt->address) = p;
				break;

			    case OPT_INT:
				if (strcmp(val, "") == 0)
					n = 1;
				else
					n = atoi(val);
				*((int *)opt->address) = n;
				break;

			    case OPT_FILE:
				fp = fopen(val, "w");
				*((FILE **)opt->address) = fp;
				break;
			}
			break;
		}
	}
}

static
void
get_environment()
{
	char *p;
	char *base;
	char optname[100];
	char optval[100];

	p = getenv("NIS_OPTIONS");
	if (p == NULL)
		return;

	while (*p) {
		while (isspace(*p))
			p++;
		if (*p == '\0')
			break;

		base = p;
		while (*p && *p != '=' && !isspace(*p))
			p++;
		strncpy(optname, base, p-base);
		optname[p-base] = '\0';

		if (*p == '=') {
			p++;
			base = p;
			while (*p && !isspace(*p))
				p++;
			strncpy(optval, base, p-base);
			optval[p-base] = '\0';
		} else {
			optval[0] = '\0';
		}

		set_option(optname, optval);
	}
}

extern "C" {

/*
 * Initializes the client cache. Allocates the global data strucuture
 * NisSharedCache which is used by the other cache routines.
 * We return a copy of the cache variable so that it will remain
 * constant for each thread.  We can change the cache variable
 * from the shared cache to the local cache, but we must not
 * delete the cache because another thread might still be using it
 * (except in this routine if we create a cache with an error).
 */

nis_error
__nis_CacheInit(NisCache **cache)
{
	nis_error status = NIS_SUCCESS;

	mutex_lock(&cur_cache_lock);

	if (!checked_env) {
		get_environment();
		checked_env = 1;
	}

	if (cur_cache && !cur_cache->okay()) {
		    cur_cache = NULL;
	}

	if (cur_cache == NULL) {
		cur_cache = new NisClientCache(status);
		if (cur_cache == NULL) {
			status = NIS_NOMEMORY;
		} else if (status != NIS_SUCCESS) {
			delete cur_cache;
			cur_cache = NULL;
		}

		if (cur_cache == NULL) {
			cur_cache = new NisLocalCache(status);
			if (cur_cache == NULL) {
				status = NIS_NOMEMORY;
			} else if (status != NIS_SUCCESS) {
				delete cur_cache;
				cur_cache = NULL;
			}
		}
	}
	*cache = cur_cache;
	mutex_unlock(&cur_cache_lock);
	return (status);
}


/*
 *  The Federated Naming code needs to be able to talk to NIS+ servers
 *  in foreign domains.  It does this by calling __nis_CacheAddEntry
 *  with a "fabricated" directory object.  The binding code needs to
 *  check to for these added directories.  The simplest way to handle
 *  it would be to switch over to using the local cache but then we
 *  wouldn't be able to take advantage of the shared directory cache
 *  for lookups in the local domain.  So, instead, we create an
 *  auxiliary local cache and check there for bindings first.
 *
 *  Note that if the application is already using a local cache,
 *  then we will be creating another local cache.  That doesn't
 *  break anything (it just uses more memory), but it makes the
 *  code simpler to not check for this special case, which shouldn't
 *  happen under normal circumstances.
 */
static NisCache *aux_cache = NULL;

static
int
__nis_CacheAuxBind(char *dname, nis_bound_directory **binding, u_long flags)
{
	nis_bound_directory *t;
	nis_error err;
	nis_server *srv;
	int nsrv;

	/* check to see if we have an auxiliary cache */
	mutex_lock(&cur_cache_lock);
	if (aux_cache == NULL) {
		mutex_unlock(&cur_cache_lock);
		return (NIS_NOTFOUND);
	}
	mutex_unlock(&cur_cache_lock);

	/* check to see if directory is in the cache */
	err = aux_cache->searchDir(dname, &t, 0);
	if (err != NIS_SUCCESS)
		return (NIS_NOTFOUND);

	srv = t->dobj.do_servers.do_servers_val;
	nsrv = t->dobj.do_servers.do_servers_len;

	if (flags & MASTER_ONLY) {
		err = aux_cache->bindServer(srv, 1, binding);
	} else {
		err = aux_cache->bindServer(srv, nsrv, binding);
	}
	nis_free_binding(t);

	return (err);
}

void
__nis_CacheStart()
{
	nis_error status;
	NisCache *cache;

	while (1) {
		if ((status = __nis_CacheInit(&cache)) != NIS_SUCCESS)
			return;
		if (cache->okay())
			break;
	}
}

/*
 * The C interface to NisCache::Bind().
 * Returns a directory structure for a given dir_name.
 */

nis_error
__nis_CacheBind(char *dname, directory_obj *dobj)
{
	nis_error status;
	nis_error err;
	nis_bound_directory *binding;
	NisCache *cache;

	while (1) {
		if ((status = __nis_CacheInit(&cache)) != NIS_SUCCESS)
			return (status);

		err = cache->bindReplica(dname, &binding);
		if (cache->okay())
			break;
	}
	if (err == NIS_SUCCESS) {
		*dobj = binding->dobj;
		memset((char *)&binding->dobj, 0, sizeof (directory_obj));
		nis_free_binding(binding);
	} else {
		memset((void *)dobj, 0, sizeof (directory_obj));
	}
	return (err);
}

/*
 * The C interface to NisSharedCache::removeEntry()
 * Removes an entry from the cache.
 */

bool_t
__nis_CacheRemoveEntry(directory_obj *dobj)
{
	nis_error status;
	nis_error err;
	nis_bound_directory *binding;
	NisCache *cache;

	while (1) {
		if ((status = __nis_CacheInit(&cache)) != NIS_SUCCESS)
			return (status);

		err = cache->searchDir(dobj->do_name, &binding, 0);
		if (cache->okay())
			break;
	}
	if (err == NIS_SUCCESS) {
		cache->refreshBinding(binding);
		nis_free_binding(binding);
	}
	return (NIS_SUCCESS);
}

/*
 * The C interface to NisSharedCache::search()
 * searches the cache for a given directory_name
 */

nis_error
__nis_CacheSearch(char *dname, directory_obj *dobj)
{
	nis_error err;
	nis_bound_directory *binding;
	NisCache *cache;

	if (__nis_CacheAuxBind(dname, &binding, 0) == NIS_SUCCESS) {
		*dobj = binding->dobj;
		memset((char *)&binding->dobj, 0, sizeof (directory_obj));
		nis_free_binding(binding);
		return (NIS_SUCCESS);
	}

	while (1) {
		if ((err = __nis_CacheInit(&cache)) != NIS_SUCCESS)
			return (err);

		err = cache->searchDir(dname, &binding, 1);
		if (cache->okay())
			break;
	}
	if (err == NIS_SUCCESS) {
		*dobj = binding->dobj;
		memset((char *)&binding->dobj, 0, sizeof (directory_obj));
		nis_free_binding(binding);
	} else {
		memset((void *)dobj, 0, sizeof (directory_obj));
	}
	return (err);
}

/*
 * The C interface to NisSharedCache::read_coldstart().
 * It tells the caching system to reinitialize from the coldstart file.
 * sends a message to cachemgr if the cachefile is valid to do this or
 * if local_cache is valid reads in the coldstart on its own.
 */

void
__nis_CacheRestart()
{
	NisCache *cache;

	while (1) {
		if (__nis_CacheInit(&cache) != NIS_SUCCESS)
			return;

		cache->readColdStart();
		if (cache->okay())
			break;
	}
}

/*
 * The C interface to NisSharedCache::print()
 * dumps the entrire cache on stdout.
 */

void
__nis_CachePrint()
{
	NisCache *cache;

	while (1) {
		if (__nis_CacheInit(&cache) != NIS_SUCCESS)
			return;

		cache->print();
		if (cache->okay())
			break;
	}
}


bool_t
__nis_CacheAddEntry(fd_result *, directory_obj *dobj)
{
	directory_obj *tmp;
	nis_error status = NIS_SUCCESS;

	mutex_lock(&cur_cache_lock);
	if (!aux_cache) {
		aux_cache = new NisLocalCache(status);
		if (aux_cache == NULL) {
			mutex_unlock(&cur_cache_lock);
			return (0);
		} else if (status != NIS_SUCCESS) {
			delete aux_cache;
			aux_cache = NULL;
			mutex_unlock(&cur_cache_lock);
			return (0);
		}
	}
	mutex_unlock(&cur_cache_lock);

	/* make a copy of the dir. obj. because createBinding() frees it */
	tmp = (directory_obj *)calloc(1, sizeof (*tmp));
	if (!tmp) {
		return (NIS_NOMEMORY);
	}
	if (!__nis_xdr_dup((xdrproc_t)xdr_directory_obj,
			(char *)dobj, (char *)tmp)) {
		free((void *)tmp);
		return (NIS_NOMEMORY);
	}
	if (aux_cache->createBinding(tmp) != NIS_SUCCESS)
		return (0);
	return (1);
}

void
__nis_CacheRefreshEntry(char *)
{
	/* this function is obsolete, but remains for compatibility */
}

nis_error
__nis_CacheBindDir(char *dname, nis_bound_directory **binding, int flags)
{
	nis_error status;
	NisCache *cache;

	if (__nis_CacheAuxBind(dname, binding, flags) == NIS_SUCCESS)
		return (NIS_SUCCESS);

	while (1) {
		if ((status = __nis_CacheInit(&cache)) != NIS_SUCCESS)
			return (status);

		if (flags & MASTER_ONLY)
			status = cache->bindMaster(dname, binding);
		else
			status = cache->bindReplica(dname, binding);

		if (cache->okay())
			break;
	}
	return (status);
}

nis_error
__nis_CacheBindMaster(char *dname, nis_bound_directory **binding)
{
	nis_error status;
	NisCache *cache;

	if (__nis_CacheAuxBind(dname, binding, MASTER_ONLY) == NIS_SUCCESS)
		return (NIS_SUCCESS);

	while (1) {
		if ((status = __nis_CacheInit(&cache)) != NIS_SUCCESS)
			return (status);

		status = cache->bindMaster(dname, binding);
		if (cache->okay())
			break;
	}
	return (status);
}

nis_error
__nis_CacheBindServer(nis_server *srv, int nsrv, nis_bound_directory **binding)
{
	nis_error status;
	NisCache *cache;

	while (1) {
		if ((status = __nis_CacheInit(&cache)) != NIS_SUCCESS)
			return (status);

		status = cache->bindServer(srv, nsrv, binding);
		if (cache->okay())
			break;
	}
	return (status);
}

int
__nis_CacheRefreshBinding(nis_bound_directory *binding)
{
	int status;
	NisCache *cache;

	if (binding->dobj.do_name == NULL)
		return (1);

	while (1) {
		if (__nis_CacheInit(&cache) != NIS_SUCCESS)
			return (0);

		status = cache->refreshBinding(binding);
		if (cache->okay())
			break;
	}
	return (status);
}

int
__nis_CacheRefreshAddress(nis_bound_endpoint *bep)
{
	int status;
	NisCache *cache;

	while (1) {
		if (__nis_CacheInit(&cache) != NIS_SUCCESS)
			return (0);

		status = cache->refreshAddress(bep);
		if (cache->okay())
			break;
	}
	return (status);
}

int
__nis_CacheRefreshCallback(nis_bound_endpoint *bep)
{
	int status;
	NisCache *cache;

	while (1) {
		if (__nis_CacheInit(&cache) != NIS_SUCCESS)
			return (0);

		status = cache->refreshCallback(bep);
		if (cache->okay())
			break;
	}
	return (status);
}

u_long
__nis_CacheLocalLoadPref()
{
	return (cur_cache->loadPreferredServers());
}


nis_error
__nis_CacheLocalInit(u_long *exp_time)
{
	nis_error status = NIS_SUCCESS;

	mutex_lock(&cur_cache_lock);

	if (!checked_env) {
		get_environment();
		checked_env = 1;
	}

	cur_cache = new NisLocalCache(status, exp_time);
	if (cur_cache == NULL) {
		status = NIS_NOMEMORY;
	} else if (status != NIS_SUCCESS) {
		delete cur_cache;
		cur_cache = NULL;
	}

	mutex_unlock(&cur_cache_lock);

	return (status);
}

nis_error
__nis_CacheMgrInit()
{
	nis_error status = NIS_SUCCESS;

	mutex_lock(&cur_cache_lock);

	if (!checked_env) {
		get_environment();
		checked_env = 1;
	}

	mgr_cache = new NisMgrCache(status);
	if (mgr_cache == NULL) {
		status = NIS_NOMEMORY;
	} else if (status != NIS_SUCCESS) {
		delete mgr_cache;
		mgr_cache = NULL;
	}
	cur_cache = mgr_cache;
	mutex_unlock(&cur_cache_lock);

	if (mgr_cache)
		mgr_cache->start();

	return (status);
}

void
__nis_CacheMgrCleanup()
{
	mutex_lock(&cur_cache_lock);
	if (mgr_cache)
		delete mgr_cache;
	mgr_cache = NULL;
	mutex_unlock(&cur_cache_lock);
}

void
__nis_CacheMgrReadColdstart()
{
	mgr_cache->readColdStart();
}

nis_error
__nis_CacheMgrBindReplica(char *dname)
{
	nis_error err;
	nis_bound_directory *binding;

	err = mgr_cache->bindReplica(dname, &binding);
	if (err == NIS_SUCCESS)
		nis_free_binding(binding);
	return (err);
}

nis_error
__nis_CacheMgrBindMaster(char *dname)
{
	nis_error err;
	nis_bound_directory *binding;

	err = mgr_cache->bindMaster(dname, &binding);
	if (err == NIS_SUCCESS)
		nis_free_binding(binding);
	return (err);
}

nis_error
__nis_CacheMgrBindServer(nis_server *srv, int nsrv)
{
	nis_error err;
	nis_bound_directory *binding;

	err = mgr_cache->bindServer(srv, nsrv, &binding);
	if (err == NIS_SUCCESS)
		nis_free_binding(binding);
	return (err);
}

int
__nis_CacheMgrRefreshBinding(nis_bound_directory *binding)
{
	if (binding->dobj.do_name == NULL)
		return (1);
	return (mgr_cache->refreshBinding(binding));
}

int
__nis_CacheMgrRefreshAddress(nis_bound_endpoint *bep)
{
	return (mgr_cache->refreshAddress(bep));
}

int
__nis_CacheMgrRefreshCallback(nis_bound_endpoint *bep)
{
	return (mgr_cache->refreshCallback(bep));
}

int
__nis_CacheMgrUpdateUaddr(char *uaddr)
{
	return (mgr_cache->updateUaddr(uaddr));
}

void
__nis_CacheMgrMarkUp()
{
	mgr_cache->markUp();
}

u_long
__nis_CacheMgrTimers()
{
	return (mgr_cache->timers());
}

u_long
__nis_CacheMgrRefreshCache()
{
	return (mgr_cache->refreshCache());
}

u_long
__nis_serverRefreshCache()
{
	return (cur_cache->refreshCache());
}

}  /* extern "C" */
