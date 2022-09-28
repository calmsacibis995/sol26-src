/*
 *	Copyright (c) 1996, by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ifndef	__MGR_CACHE_H
#define	__MGR_CACHE_H

#pragma ident	"@(#)mgr_cache.h	1.2	96/05/22 SMI"

#include "mapped_cache.h"

#define	MIN_REFRESH_WAIT (5 * 60)	/* 5 minutes */
#define	PING_WAIT (15 * 60)		/* 15 minutes */
#define	CONFIG_WAIT (12 * 60 * 60)	/* 12 hours */

class NisMgrCache : public NisMappedCache {
    public:
	NisMgrCache(nis_error &error);
	~NisMgrCache();
	void start();
	u_long loadPreferredServers();

	u_long timers();
	u_long nextTime();
	u_long refreshCache();

	void *operator new(size_t bytes) { return calloc(1, bytes); }
	void operator delete(void *arg) { (void)free(arg); }

    private:
	u_long refresh_time;
	u_long ping_time;
	u_long config_time;
	u_long config_interval;

	void refresh();
	void ping();
	u_long config();
	void parse_info(char *info, char **srvr, char **option);
	char *get_line(FILE *fp);
	u_long writeDotFile();
	u_long loadLocalFile();
	u_long loadNisTable();
};

#endif	/* __MGR_CACHE_H */
