/*
 *	nis_cache.x
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

%#pragma ident	"@(#)nis_cache.x	1.9	96/05/22 SMI"


#ifdef RPC_HDR
%#include <rpc/types.h>
%#include <rpcsvc/nis.h>
%#include "nis_clnt.h"
#endif

struct bind_server_arg {
	nis_server *srv;
	int nsrv;
};

struct refresh_res {
	int changed;
	endpoint ep;
};

program CACHEPROG {
	version CACHE_VER_2 {
		void NIS_CACHE_ADD_ENTRY(fd_result) = 1;
		void NIS_CACHE_REMOVE_ENTRY(directory_obj) = 2;
		void NIS_CACHE_READ_COLDSTART(void) = 3;
		void NIS_CACHE_REFRESH_ENTRY(string<>) = 4;

		nis_error NIS_CACHE_BIND_REPLICA(string<>) = 5;
		nis_error NIS_CACHE_BIND_MASTER(string<>) = 6;
		nis_error NIS_CACHE_BIND_SERVER(bind_server_arg) = 7;
		refresh_res NIS_CACHE_REFRESH_BINDING(nis_bound_directory) = 8;
		refresh_res NIS_CACHE_REFRESH_ADDRESS(nis_bound_endpoint) = 9;
		refresh_res NIS_CACHE_REFRESH_CALLBACK(nis_bound_endpoint) = 10;
	} = 2;
} = 100301;
