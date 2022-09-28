/*
 *	Copyright (c) 1996, by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ifndef	__MAPPED_CACHE_H
#define	__MAPPED_CACHE_H

#pragma ident	"@(#)mapped_cache.h	1.4	96/07/25 SMI"

#include "cache.h"

#define	SECTION_UADDR	0	/* cache manager's uaddr */
#define	SECTION_BINDING	1	/* list of bound directories */
#define	SECTION_ACTIVE	2	/* list of active servers */
#define	NUM_SECTIONS	3

struct CacheSection {
	int count;
	int offset;
	int length;
};

struct CacheHeader {
	int version;
	int valid;
	int map_size;
	int data_size;
	CacheSection sections[NUM_SECTIONS];
};

struct BindingEntry {
	char *base;
	int offset;
	int length;
	u_long exp_time;
	int min_rank;
	int optimal_rank;
	int levels;
	char **broken_name;
	int binding_len;
	void *binding;
};

struct ActiveEntry {
	char *base;
	int offset;
	int length;
	endpoint ep;
	int active_len;
	void *active;
};

class NisMappedCache : public NisCache {
    public:
	NisMappedCache(nis_error &error, int serverMode = 0);
	~NisMappedCache();

	nis_error searchDir(char *dname,
		nis_bound_directory **binding, int near);
	void addBinding(nis_bound_directory *binding);
	void removeBinding(nis_bound_directory *binding);
	void print();

	void activeAdd(nis_active_endpoint *act);
	void activeRemove(endpoint *ep, int all);
	int activeCheck(endpoint *ep);
	int activeGet(endpoint *ep, nis_active_endpoint **act);
	int activeCheckInternal(endpoint *ep);
	int activeGetInternal(endpoint *ep, nis_active_endpoint **act);

	int getStaleEntries(nis_bound_directory ***bindings);
	int getAllEntries(nis_bound_directory ***bindings);
	long nextStale();
	int getAllActive(nis_active_endpoint ***actlist);

	int updateUaddr(char *uaddr);
	char *getUaddr();
	void markUp();
	void markDown();
	int checkUp();

	void *operator new(size_t bytes) { return calloc(1, bytes); }
	void operator delete(void *arg) { (void)free(arg); }

    private:
	int up;
	int serverMode;
	char *mapBase;
	int mapSize;
	ino_t mapInode;
	dev_t mapDev;
	CacheHeader *header;
	int sem_reader;
	int sem_writer;

	int checkRoot();
	int createSemaphores();
	int getSemaphores();
	int mapCache();
	void unmapCache();
	int createCache();

	void freeSpace(int offset, int size, int section);
	int addSpace(int offset, int size, int sect);
	void writeCache(int offset, char *src, int len);

	int createBindingEntry(nis_bound_directory *binding,
			BindingEntry *entry);
	void readBinding(BindingEntry *entry, int offset);
	void firstBinding(BindingEntry *entry);
	void nextBinding(BindingEntry *entry);

	int createActiveEntry(ActiveEntry *entry, nis_active_endpoint *act);
	void readActiveEntry(ActiveEntry *entry, int offset);
	void firstActiveEntry(ActiveEntry *entry);
	void nextActiveEntry(ActiveEntry *entry);

	int align(int n);
	int lockExclusive(sigset_t *sset);
	void unlockExclusive(sigset_t *sset);
	int lockShared(sigset_t *sset);
	void unlockShared(sigset_t *sset);

	int fileChange();
};

#endif	/* __MAPPED_CACHE_H */
