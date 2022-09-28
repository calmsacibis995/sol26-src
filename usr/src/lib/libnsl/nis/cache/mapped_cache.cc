/*
 *	Copyright (c) 1996, by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ident	"@(#)mapped_cache.cc	1.8	96/07/25 SMI"

#include <sys/types.h>
#include <unistd.h>
#include <values.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <malloc.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "mapped_cache.h"

/*
 *  For BPC programs (which only run on SPARC), we need to use _fstat
 *  so that we get the 5.x version of fstat instead of the 4.x version.
 *  On other architectures, the regular fstat is the right one to use.
 */

#if defined(sparc)
#define	_FSTAT _fstat
extern "C" int _fstat(int, struct stat *);
#else  /* !sparc */
#define	_FSTAT fstat
#endif /* sparc */

#define	CACHE_FILE "/var/nis/NIS_SHARED_DIRCACHE"
#define	CACHE_VERSION 3
#define	CACHE_MAGIC   0xbabeeeee


union semun {
	int val;
	struct semid_ds *buf;
	ushort *array;
} arg;

NisMappedCache::NisMappedCache(nis_error &err, int mode)
{
	sigset_t oset;
	err = NIS_SUCCESS;

	serverMode = mode;
	mapBase = (char *)-1;
	mapSize = -1;
	if (serverMode) {
		up = 0;
		if (!checkRoot()) {
			err = NIS_PERMISSION;
			return;
		}

		if (!createSemaphores()) {
			err = NIS_SYSTEMERROR;
			return;
		}

		lockExclusive(&oset);
		if (!mapCache()) {
			if (!createCache()) {
				unlockExclusive(&oset);
				err = NIS_SYSTEMERROR;
				return;
			}

			if (!mapCache()) {
				unlockExclusive(&oset);
				err = NIS_SYSTEMERROR;
				return;
			}
		}
		unlockExclusive(&oset);
	} else {
		if (!getSemaphores()) {
			err = NIS_SYSTEMERROR;
			return;
		}
		/* lockShared() will map in the cache */
		if (!lockShared(&oset))
			err = NIS_SYSTEMERROR;
		unlockShared(&oset);
	}
}

NisMappedCache::~NisMappedCache()
{
}

int
NisMappedCache::checkRoot()
{
	if (geteuid() != 0) {
		syslog(LOG_ERR, "must be root to manage cache");
		return (0);
	}

	return (1);
}

int
NisMappedCache::createSemaphores()
{
	int st;
	u_short w_array[NIS_W_NSEMS];
	union semun semarg;
	int semflg;

	semflg = IPC_CREAT |
		SEM_OWNER_READ | SEM_OWNER_ALTER |
		SEM_GROUP_READ | SEM_GROUP_ALTER |
		SEM_OTHER_READ | SEM_OTHER_ALTER;

	// get writer semaphore
	if ((sem_writer = semget(NIS_SEM_W_KEY, NIS_W_NSEMS, semflg)) == -1) {
		syslog(LOG_ERR, "can't create writer semaphore: %m");
		return (0);
	}

	// get reader semaphore
	if ((sem_reader = semget(NIS_SEM_R_KEY, NIS_R_NSEMS, semflg)) == -1) {
		syslog(LOG_ERR, "can't create reader semaphore: %m");
		return (0);
	}

	// get writer semaphore value
	semarg.array = w_array;
	if (semctl(sem_writer, 0, GETALL, semarg) == -1) {
		syslog(LOG_ERR, "can't get writer value: %m");
		return (0);
	}

	// check to see if a manager is already handling the cache
	if (w_array[NIS_SEM_MGR_UP] != 0) {
		syslog(LOG_ERR, "WARNING: cache already being managed: %m");
		semarg.val = 0;
		st = semctl(sem_writer, NIS_SEM_MGR_UP, SETVAL, semarg);
		if (st == -1) {
			syslog(LOG_ERR, "can't clear write semaphore: %m");
			return (0);
		}
	}

	semarg.val = 0;
	if (semctl(sem_writer, NIS_SEM_MGR_EXCL, SETVAL, semarg) == -1) {
		syslog(LOG_ERR, "can't set exclusive semaphore: %m");
		return (0);
	}

	return (1);
}

int
NisMappedCache::getSemaphores()
{
	int st;
	u_short w_array[NIS_W_NSEMS];
	union semun semarg;
	int semflg = 0;

	// get writer semaphore
	if ((sem_writer = semget(NIS_SEM_W_KEY, NIS_W_NSEMS, semflg)) == -1) {
		return (0);
	}

	// get reader semaphore
	if ((sem_reader = semget(NIS_SEM_R_KEY, NIS_R_NSEMS, semflg)) == -1) {
		return (0);
	}

	return (1);
}

int
NisMappedCache::mapCache()
{
	int open_mode;
	int map_mode;
	int status = 0;
	int fd = -1;
	struct stat stbuf;
	sigset_t oset;

	if (serverMode) {
		open_mode = O_RDWR | O_SYNC;
		map_mode = PROT_READ|PROT_WRITE;
	} else {
		open_mode = O_RDONLY;
		map_mode = PROT_READ;
	}

	fd = open(CACHE_FILE, open_mode);
	if (fd == -1)
		goto done;
	if (_FSTAT(fd, &stbuf) == -1) {
		syslog(LOG_ERR, "can't stat %s:  %m", CACHE_FILE);
		goto done;
	}
	mapSize = (int)stbuf.st_size;
	mapBase = mmap(0, mapSize, map_mode, MAP_SHARED, fd, 0);
	if (mapBase == (char *)-1) {
		mapSize = -1;
		syslog(LOG_ERR, "can't mmap %s:  %m", CACHE_FILE);
		goto done;
	}

	header = (CacheHeader *)mapBase;
	if (header->version != CACHE_VERSION) {
		goto done;
	}
	if (header->valid == 0) {
		syslog(LOG_ERR, "cache left in invalid state");
		goto done;

	}
	if (serverMode)
		header->map_size = mapSize;

	status = 1;

done:
	if (fd != -1)
		close(fd);

	if (status == 0) {
		if (mapBase != (char *)-1) {
			munmap(mapBase, mapSize);
			mapBase = (char *)-1;
			mapSize = -1;
		}
	}

	return (status);
}

void
NisMappedCache::unmapCache()
{
	if (mapBase != (char *)-1)
		munmap(mapBase, mapSize);
}

int
NisMappedCache::createCache()
{
	int i;
	int fd;
	int st;
	CacheHeader hdr;
	int status = 0;
	sigset_t oset;

	unlink(CACHE_FILE);	/* remove any left-over version */
	fd = open(CACHE_FILE, O_RDWR|O_CREAT, 0644);
	if (fd == -1) {
		syslog(LOG_ERR, "can't create cache file:  %m");
		goto done;
	}

	hdr.version = CACHE_VERSION;
	hdr.valid = 1;
	hdr.data_size = sizeof (CacheHeader);
	hdr.map_size = sizeof (CacheHeader);
	for (i = 0; i < NUM_SECTIONS; i++) {
		hdr.sections[i].count = 0;
		hdr.sections[i].offset = sizeof (CacheHeader);
		hdr.sections[i].length = 0;
	}

	st = write(fd, &hdr, hdr.data_size);
	if (st == -1) {
		syslog(LOG_ERR, "error writing cache file: %m");
		goto done;
	} else if (st != hdr.data_size) {
		syslog(LOG_ERR, "short write to cache file (%d, %d)",
				st, hdr.data_size);
		goto done;
	}

	close(fd);

	status = 1;

done:
	if (status == 0) {
		if (fd != -1)
			close(fd);
		unlink(CACHE_FILE);	/* don't leave broken cache */
	}

	return (status);
}

int
NisMappedCache::updateUaddr(char *uaddr)
{
	int size;
	int offset;
	int length;
	sigset_t oset;

	if (!lockExclusive(&oset))
		return (0);

	size = align(strlen(uaddr) + 1);	/* include null terminator */

	offset = header->sections[SECTION_UADDR].offset;
	length = header->sections[SECTION_UADDR].length;
	freeSpace(offset, length, SECTION_UADDR);

	offset = header->sections[SECTION_UADDR].offset;
	if (!addSpace(offset, size, SECTION_UADDR)) {
		unlockExclusive(&oset);
		return (0);
	}
	writeCache(offset, uaddr, strlen(uaddr) + 1);	/* include null */
	unlockExclusive(&oset);
	return (1);
}

char *
NisMappedCache::getUaddr()
{
	int offset;
	int length;
	char *uaddr;
	sigset_t oset;

	if (!lockShared(&oset))
		return (0);

	offset = header->sections[SECTION_UADDR].offset;
	length = header->sections[SECTION_UADDR].length;

	uaddr = (char *)malloc(length);
	if (uaddr) {
		strcpy(uaddr, mapBase + offset);
	}
	unlockShared(&oset);
	return (uaddr);
}

void
NisMappedCache::markUp()
{
	struct sembuf buf;

	buf.sem_num = NIS_SEM_MGR_UP;
	buf.sem_op = 1;
	buf.sem_flg = SEM_UNDO;

	if (semop(sem_writer, &buf, 1) == -1) {
		syslog(LOG_ERR, "NIS_SEM_MGR_UP semop failed: %m");
	}
	up = 1;
}

void
NisMappedCache::markDown()
{
	struct sembuf buf;

	/* if we never successfully started, just return */
	if (!up)
		return;

	/*
	 *  Sync the cache file in case we were in the middle
	 *  of an update.
	 */
	if (mapBase != (char *)-1) {
		if (msync(mapBase, mapSize, MS_SYNC) == -1) {
			syslog(LOG_ERR, "msync failed:  %s");
			/* what should we do here? */
		}
	}

	buf.sem_num = NIS_SEM_MGR_UP;
	buf.sem_op = -1;
	buf.sem_flg = SEM_UNDO | IPC_NOWAIT;

	if (semop(sem_writer, &buf, 1) == -1) {
		syslog(LOG_ERR, "NIS_SEM_MGR_UP semop failed: %m");
	}
}

int
NisMappedCache::checkUp()
{
	ushort w_array[NIS_W_NSEMS];
	union semun semarg;

	if (sem_reader == -1 || sem_writer == -1)
		return (FALSE);

	semarg.array = w_array;
	if (semctl(sem_writer, 0, GETALL, semarg) == -1)
		return (FALSE);

	if (w_array[NIS_SEM_MGR_UP] == 0) {
		// cache manager not running
		return (FALSE);
	}
	return (TRUE);
}

nis_error
NisMappedCache::searchDir(char *dname, nis_bound_directory **binding, int near)
{
	int i;
	nis_error err;
	int distance;
	int minDistance = MAXINT;
	int minLevels = MAXINT;
	struct timeval now;
	char **target;
	int target_levels;
	int found = 0;
	CacheSection *section;
	BindingEntry scan;
	BindingEntry found_entry;
	sigset_t oset;

	found_entry.broken_name = 0;

	target = __break_name(dname, &target_levels);
	if (target == 0)
		return (NIS_NOMEMORY);

	gettimeofday(&now, NULL);

	if (!lockShared(&oset)) {
		__free_break_name(target, target_levels);
		return (NIS_SYSTEMERROR);
	}

	section = &header->sections[SECTION_BINDING];
	for (i = 0; i < section->count; i++) {
		if (i == 0)
			firstBinding(&scan);
		else
			nextBinding(&scan);

		distance = __name_distance(target, scan.broken_name);
		if (distance <= minDistance) {
			// if two directories are at the same distance
			// then we want to select the directory closer to
			// the root.
			if (distance == minDistance &&
			    scan.levels >= minLevels) {
				free(scan.broken_name);
				continue;
			}
			/*
			 *  Free broken name of old saved entry.
			 */
			if (found)
				free(found_entry.broken_name);
			/*
			 *  Save this entry.
			 */
			found = 1;
			found_entry = scan;
			minDistance = distance;
			minLevels = scan.levels;
			/*
			 *  If we got an exact match, then we are done.
			 */
			if (distance == 0)
				break;
		} else {
			free(scan.broken_name);
		}
	}

	if (found == 0) {
		// cache is empty (no coldstart even)
		unlockShared(&oset);
		err = NIS_NAMEUNREACHABLE;
	} else if (near == 0 && distance != 0) {
		// we wanted an exact match, but it's not there
		unlockShared(&oset);
		err = NIS_NOTFOUND;
		free(found_entry.broken_name);
	} else {
		// we got an exact match or something near target
		err = NIS_SUCCESS;
		free(found_entry.broken_name);
		*binding = unpackBinding(found_entry.binding,
				found_entry.binding_len);
		unlockShared(&oset);
	}
	__free_break_name(target, target_levels);

	return (err);
}

void
NisMappedCache::addBinding(nis_bound_directory *binding)
{
	int i;
	char *dname;
	int is_coldstart = 0;
	BindingEntry entry;
	BindingEntry scan;
	CacheSection *section;
	sigset_t oset;

	if (!createBindingEntry(binding, &entry))
		return;

	dname = binding->dobj.do_name;
	if (nis_dir_cmp(dname, nis_local_directory()) == SAME_NAME)
		is_coldstart = 1;

	if (!lockExclusive(&oset))
		return;

	section = &header->sections[SECTION_BINDING];
	for (i = 0; i < section->count; i++) {
		if (i == 0)
			firstBinding(&scan);
		else
			nextBinding(&scan);
		if (__dir_same(scan.broken_name, entry.broken_name)) {
			free(scan.broken_name);
			freeSpace(scan.offset, scan.length,
					SECTION_BINDING);
			section->count -= 1;
			break;
		}
		free(scan.broken_name);
	}

	if (is_coldstart)
		entry.offset = section->offset;
	else
		entry.offset = section->offset + section->length;

	if (!addSpace(entry.offset, entry.length, SECTION_BINDING)) {
		free(entry.broken_name);
		free(entry.base);
		unlockExclusive(&oset);
		return;
	}
	writeCache(entry.offset, entry.base, entry.length);
	header->sections[SECTION_BINDING].count += 1;

	free(entry.broken_name);
	free(entry.base);

	unlockExclusive(&oset);
}

void
NisMappedCache::removeBinding(nis_bound_directory *binding)
{
	int i;
	int levels;
	char **broken_name;
	BindingEntry scan;
	CacheSection *section;
	sigset_t oset;

	if (!lockExclusive(&oset))
		return;

	broken_name = __break_name(binding->dobj.do_name, &levels);
	if (broken_name == NULL) {
		unlockExclusive(&oset);
		return;
	}

	section = &header->sections[SECTION_BINDING];
	for (i = 0; i < section->count; i++) {
		if (i == 0)
			firstBinding(&scan);
		else
			nextBinding(&scan);
		if (__dir_same(scan.broken_name, broken_name)) {
			free(scan.broken_name);
			freeSpace(scan.offset, scan.length,
					SECTION_BINDING);
			section->count -= 1;
			break;
		}
		free(scan.broken_name);
	}
	__free_break_name(broken_name, levels);

	unlockExclusive(&oset);
}

void
NisMappedCache::print()
{
	int i;
	CacheSection *section;
	BindingEntry scan;
	nis_bound_directory *binding;
	ActiveEntry act_scan;
	nis_active_endpoint *act;
	sigset_t oset;

	if (!lockShared(&oset))
		return;

	section = &header->sections[SECTION_BINDING];
	for (i = 0; i < section->count; i++) {
		if (i == 0)
			firstBinding(&scan);
		else
			nextBinding(&scan);

		// hack for special format in nisshowcache
		if (__nis_debuglevel != 6) {
			if (i == 0)
				printf("\nCold Start directory:\n");
			else
				printf("\nNisSharedCacheEntry[%d]:\n", i);
		}

		if (__nis_debuglevel == 1) {
			printf("\tdir_name:'");
			__broken_name_print(scan.broken_name, scan.levels);
			printf("'\n");
		}

		if (__nis_debuglevel > 2) {
			binding = unpackBinding(scan.binding, scan.binding_len);
			printBinding(binding);
			nis_free_binding(binding);
		}
		free(scan.broken_name);
	}

	printf("\nActive servers:\n");
	section = &header->sections[SECTION_ACTIVE];
	for (i = 0; i < section->count; i++) {
		if (i == 0)
			firstActiveEntry(&act_scan);
		else
			nextActiveEntry(&act_scan);

		act = unpackActive(act_scan.active, act_scan.active_len);
		printActive(act);
		activeFree(act);
	}

	unlockShared(&oset);

}

int
NisMappedCache::addSpace(int offset, int size, int sect)
{
	int i;
	int n;
	int extra;
	int fd = -1;
	char *buf = 0;
	int status = 0;
	char *src;
	char *dst;
	int amount;

	if (header->data_size + size > mapSize) {
		/* need to increase the size of the cache file */
		extra = header->data_size + size - mapSize;
		munmap(mapBase, mapSize);
		buf = (char *)calloc(1, extra);
		if (buf == 0) {
			syslog(LOG_ERR, "out of memory");
			goto done;
		}
		fd = open(CACHE_FILE, O_RDWR|O_APPEND);
		if (fd == -1) {
			syslog(LOG_ERR, "can't open %s:  %m", CACHE_FILE);
			goto done;
		}

		n = write(fd, buf, extra);
		if (n == -1) {
			syslog(LOG_ERR, "error writing to %s: %m", CACHE_FILE);
			goto done;
		} else if (n != extra) {
			syslog(LOG_ERR, "short write (%d, %d)", n, extra);
			goto done;
		}
		mapSize += extra;
		mapBase = mmap(0, mapSize,
				PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
		if (mapBase == (char *)-1) {
			syslog(LOG_ERR, "can't mmap %s:  %m", CACHE_FILE);
			mapSize = -1;
			goto done;
		}
		header = (CacheHeader *)mapBase;
		header->map_size = mapSize;
	}

	src = mapBase + offset;
	dst = mapBase + offset + size;
	amount = header->data_size - offset;
	memmove(dst, src, amount);

	header->sections[sect].length += size;
	for (i = sect+1; i < NUM_SECTIONS; i++) {
		header->sections[i].offset += size;
	}
	header->data_size += size;

	status = 1;

done:
	if (fd != -1)
		close(fd);
	free(buf);

	return (status);
}

void
NisMappedCache::freeSpace(int offset, int size, int sect)
{
	int i;
	char *src;
	char *dst;
	int amount;

	src = mapBase + offset + size;
	dst = mapBase + offset;
	amount = header->data_size - offset - size;
	memmove(dst, src, amount);

	header->sections[sect].length -= size;
	for (i = sect+1; i < NUM_SECTIONS; i++) {
		header->sections[i].offset -= size;
	}
	header->data_size -= size;
}

void
NisMappedCache::writeCache(int offset, char *src, int len)
{
	memcpy(mapBase + offset, src, len);
}

int
NisMappedCache::createBindingEntry(nis_bound_directory *binding,
		BindingEntry *entry)
{
	int i;
	int size;
	int offset;
	int levels;
	char **broken_name;
	void *packed;
	int packed_len;
	char *buf;
	char *name_start;
	u_long magic = CACHE_MAGIC;
	int status = 0;

	packed = packBinding(binding, &packed_len);
	if (packed == NULL)
		goto done;

	broken_name = __break_name(binding->dobj.do_name, &levels);
	if (broken_name == NULL)
		goto done;

	/* determine space needed to store entry */
	size = 0;
	size += sizeof (u_long);	/* magic number */
	size += sizeof (int);		/* entry length */
	size += sizeof (int);		/* expire time */
	size += sizeof (int);		/* levels in directory name */
	for (i = 0; i < levels; i++) {
		size += strlen(broken_name[i]) + 1;	/* include null */
	}
	size = align(size);
	size += sizeof (int);		/* data length */
	size += packed_len;		/* room needed for data */
	size = align(size);

	/* create buffer to hold data */
	buf = (char *)malloc(size);
	if (buf == NULL)
		goto done;

	/* write data to buffer */
	offset = 0;
	entry->base = buf;
	entry->offset = 0;
	entry->length = size;
	entry->exp_time = expireTime(binding->dobj.do_ttl);
	entry->levels = levels;

	memcpy(buf+offset, (char *)&magic, sizeof (u_long));
	offset += sizeof (u_long);

	memcpy(buf+offset, (char *)&size, sizeof (int));
	offset += sizeof (int);

	memcpy(buf+offset, (char *)&entry->exp_time, sizeof (u_long));
	offset += sizeof (u_long);

	memcpy(buf+offset, (char *)&levels, sizeof (int));
	offset += sizeof (int);

	name_start = buf+offset;
	for (i = 0; i < levels; i++) {
		strcpy(buf+offset, broken_name[i]);
		offset += strlen(broken_name[i]) + 1;
	}

	offset = align(offset);

	memcpy(buf+offset, (char *)&packed_len, sizeof (int));
	offset += sizeof (int);

	entry->binding_len = packed_len;
	entry->binding = entry->base + offset;
	memcpy(buf+offset, (char *)packed, packed_len);

	entry->broken_name = (char **)
			malloc((entry->levels + 1) * sizeof (char *));
	for (i = 0; i < entry->levels; i++) {
		entry->broken_name[i] = name_start;
		name_start += strlen(name_start) + 1;
	}
	entry->broken_name[i] = NULL;

	status = 1;

done:
	free(packed);
	if (broken_name)
		__free_break_name(broken_name, levels);

	return (status);
}

void
NisMappedCache::firstBinding(BindingEntry *entry)
{
	readBinding(entry, header->sections[SECTION_BINDING].offset);
}

void
NisMappedCache::nextBinding(BindingEntry *entry)
{
	readBinding(entry, entry->offset + entry->length);
}

void
NisMappedCache::readBinding(BindingEntry *entry, int offset)
{
	int i;
	char *p;
	u_long magic;

	entry->offset = offset;

	p = mapBase + offset;
	entry->base = (char *)p;

	magic = *(u_long *)p;
	if (magic != CACHE_MAGIC) {
		syslog(LOG_ERR, "corrupted cache (binding): 0x%x", magic);
		return;
	}
	p += sizeof (int);

	entry->length = *(int *)p;
	p += sizeof (int);

	entry->exp_time = *(u_long *)p;
	p += sizeof (u_long);

	entry->levels = *(int *)p;
	p += sizeof (int);

	entry->broken_name = (char **)
			malloc((entry->levels + 1) * sizeof (char *));
	for (i = 0; i < entry->levels; i++) {
		entry->broken_name[i] = p;
		p += strlen(p) + 1;
	}
	entry->broken_name[i] = NULL;

	p = (char *)align((int)p);
	entry->binding_len = *(int *)p;
	p += sizeof (int);

	entry->binding = p;
}

int
NisMappedCache::createActiveEntry(ActiveEntry *entry, nis_active_endpoint *act)
{
	int size;
	int offset;
	char *buf;
	void *packed;
	int packed_len;
	u_long magic = CACHE_MAGIC;
	int status = 0;

	packed = packActive(act, &packed_len);
	if (packed == NULL)
		goto done;

	/* determine space needed to store entry */
	size = 0;
	size += sizeof (u_long);	/* magic number */
	size += sizeof (int);		/* entry length */
	size += strlen(act->ep.family) + 1;	/* family */
	size += strlen(act->ep.proto) + 1;	/* proto */
	size += strlen(act->ep.uaddr) + 1;	/* uaddr */
	size = align(size);
	size += sizeof (int);		/* data length */
	size += packed_len;		/* room needed for data */
	size = align(size);

	/* create buffer to hold data */
	buf = (char *)malloc(size);
	if (buf == NULL)
		goto done;

	/* write data to buffer */
	offset = 0;
	entry->base = buf;
	entry->offset = 0;
	entry->length = size;

	memcpy(buf+offset, (char *)&magic, sizeof (u_long));
	offset += sizeof (u_long);

	memcpy(buf+offset, (char *)&size, sizeof (int));
	offset += sizeof (int);

	entry->ep.family = entry->base + offset;
	strcpy(buf+offset, act->ep.family);
	offset += strlen(act->ep.family) + 1;

	entry->ep.proto = entry->base + offset;
	strcpy(buf+offset, act->ep.proto);
	offset += strlen(act->ep.proto) + 1;

	entry->ep.uaddr = entry->base + offset;
	strcpy(buf+offset, act->ep.uaddr);
	offset += strlen(act->ep.uaddr) + 1;

	offset = align(offset);

	memcpy(buf+offset, (char *)&packed_len, sizeof (int));
	offset += sizeof (int);

	entry->active_len = packed_len;
	entry->active = entry->base + offset;
	memcpy(buf+offset, (char *)packed, packed_len);
	offset += packed_len;

	status = 1;

done:
	free(packed);
	return (status);
}

void
NisMappedCache::readActiveEntry(ActiveEntry *entry, int offset)
{
	char *p;
	u_long magic;

	entry->offset = offset;

	p = mapBase + offset;
	entry->base = (char *)p;

	magic = *(u_long *)p;
	if (magic != CACHE_MAGIC) {
		syslog(LOG_ERR, "corrupted cache (endpoint): 0x%x", magic);
		return;
	}
	p += sizeof (int);

	entry->length = *(int *)p;
	p += sizeof (int);

	entry->ep.family = p;
	p += strlen(p) + 1;

	entry->ep.proto = p;
	p += strlen(p) + 1;

	entry->ep.uaddr = p;
	p += strlen(p) + 1;

	p = (char *)align((int)p);
	entry->active_len = *(int *)p;
	p += sizeof (int);

	entry->active = p;
}

void
NisMappedCache::firstActiveEntry(ActiveEntry *entry)
{
	readActiveEntry(entry, header->sections[SECTION_ACTIVE].offset);
}

void
NisMappedCache::nextActiveEntry(ActiveEntry *entry)
{
	readActiveEntry(entry, entry->offset + entry->length);
}

void
NisMappedCache::activeAdd(nis_active_endpoint *act)
{
	ActiveEntry entry;
	sigset_t oset;

	if (!lockExclusive(&oset))
		return;

	if (!createActiveEntry(&entry, act)) {
		unlockExclusive(&oset);
		return;
	}
	entry.offset = header->sections[SECTION_ACTIVE].offset;
	if (!addSpace(entry.offset, entry.length, SECTION_ACTIVE)) {
		free(entry.base);
		unlockExclusive(&oset);
		return;
	}
	writeCache(entry.offset, entry.base, entry.length);
	header->sections[SECTION_ACTIVE].count += 1;

	free(entry.base);
	unlockExclusive(&oset);
}

void
NisMappedCache::activeRemove(endpoint *ep, int all)
{
	int i;
	ActiveEntry scan;
	CacheSection *section;
	sigset_t oset;

	if (!lockExclusive(&oset))
		return;

restart:
	section = &header->sections[SECTION_ACTIVE];
	for (i = 0; i < section->count; i++) {
		if (i == 0)
			firstActiveEntry(&scan);
		else
			nextActiveEntry(&scan);

		if (strcmp(scan.ep.family, ep->family) == 0 &&
		    (all || strcmp(scan.ep.proto, ep->proto) == 0) &&
		    strcmp(scan.ep.uaddr, ep->uaddr) == 0) {
			freeSpace(scan.offset, scan.length, SECTION_ACTIVE);
			header->sections[SECTION_ACTIVE].count -= 1;
			/*
			 *  If we are getting rid of all servers regardless
			 *  of protocol, then we need to restart the
			 *  search because removing an entry invalidates
			 *  our "iteration".  If we are just removing
			 *  a single server, then we are done.
			 */
			if (all)
				goto restart;
			break;
		}
	}
	unlockExclusive(&oset);
}

int
NisMappedCache::activeCheck(endpoint *ep)
{
	int ret = 0;
	sigset_t oset;

	if (!lockShared(&oset))
		return (ret);

	ret = activeCheckInternal(ep);

	unlockShared(&oset);

	return (ret);
}

int
NisMappedCache::activeCheckInternal(endpoint *ep)
{
	int i;
	ActiveEntry scan;
	CacheSection *section;

	section = &header->sections[SECTION_ACTIVE];
	for (i = 0; i < section->count; i++) {
		if (i == 0)
			firstActiveEntry(&scan);
		else
			nextActiveEntry(&scan);

		if (strcmp(scan.ep.family, ep->family) == 0 &&
		    strcmp(scan.ep.proto, ep->proto) == 0 &&
		    strcmp(scan.ep.uaddr, ep->uaddr) == 0) {
			return (1);
		}
	}
	return (0);
}

int
NisMappedCache::activeGet(endpoint *ep, nis_active_endpoint **act)
{
	int ret = 0;
	sigset_t oset;

	if (!lockShared(&oset))
		return (ret);

	ret = activeGetInternal(ep, act);

	unlockShared(&oset);
	return (ret);
}

int
NisMappedCache::activeGetInternal(endpoint *ep, nis_active_endpoint **act)
{
	int i;
	ActiveEntry scan;
	CacheSection *section;

	section = &header->sections[SECTION_ACTIVE];
	for (i = 0; i < section->count; i++) {
		if (i == 0)
			firstActiveEntry(&scan);
		else
			nextActiveEntry(&scan);

		if (strcmp(scan.ep.family, ep->family) == 0 &&
		    strcmp(scan.ep.proto, ep->proto) == 0 &&
		    strcmp(scan.ep.uaddr, ep->uaddr) == 0) {
			*act = unpackActive(scan.active,
					scan.active_len);
			return (1);
		}
	}
	return (0);
}

int
NisMappedCache::getStaleEntries(nis_bound_directory ***bindings)
{
	int i;
	struct timeval now;
	int stale_count = 0;
	CacheSection *section;
	BindingEntry scan;
	sigset_t oset;

	gettimeofday(&now, NULL);

	if (!lockShared(&oset)) {
		*bindings = 0;
		return (0);
	}

	/*
	 * We allocate more than we need so that we don't have to
	 * figure out how many stale entries there are ahead of time.
	 */
	section = &header->sections[SECTION_BINDING];
	*bindings = (nis_bound_directory **)
		    malloc(section->count * sizeof (nis_bound_directory *));
	if (*bindings == NULL) {
		unlockShared(&oset);
		return (0);
	}
	for (i = 0; i < section->count; i++) {
		if (i == 0)
			firstBinding(&scan);
		else
			nextBinding(&scan);

		if (now.tv_sec > scan.exp_time) {
			/*
			 *  Unpack the binding, but don't bother adding
			 *  the bound addresses, because purging doesn't
			 *  need them.
			 */
			(*bindings)[stale_count] =
				unpackBinding(scan.binding, scan.binding_len);
			stale_count++;
		}
		free(scan.broken_name);
	}
	unlockShared(&oset);

	return (stale_count);
}

int
NisMappedCache::getAllEntries(nis_bound_directory ***bindings)
{
	int i;
	CacheSection *section;
	BindingEntry scan;
	sigset_t oset;

	if (!lockShared(&oset)) {
		*bindings = 0;
		return (0);
	}

	section = &header->sections[SECTION_BINDING];
	*bindings = (nis_bound_directory **)
		    malloc(section->count * sizeof (nis_bound_directory *));
	if (*bindings == NULL) {
		unlockShared(&oset);
		return (0);
	}
	for (i = 0; i < section->count; i++) {
		if (i == 0)
			firstBinding(&scan);
		else
			nextBinding(&scan);

		/*
		 *  Unpack the binding, but don't bother adding
		 *  the bound addresses, because we don't
		 *  need them.
		 */
		(*bindings)[i] =
			unpackBinding(scan.binding, scan.binding_len);
		free(scan.broken_name);
	}
	unlockShared(&oset);

	return (i);
}

int
NisMappedCache::getAllActive(nis_active_endpoint ***actlist)
{
	int i;
	CacheSection *section;
	ActiveEntry scan;
	sigset_t oset;

	if (!lockShared(&oset)) {
		*actlist = 0;
		return (0);
	}

	section = &header->sections[SECTION_ACTIVE];
	*actlist = (nis_active_endpoint **)
		    malloc(section->count * sizeof (nis_active_endpoint *));
	if (*actlist == NULL) {
		unlockShared(&oset);
		return (0);
	}
	for (i = 0; i < section->count; i++) {
		if (i == 0)
			firstActiveEntry(&scan);
		else
			nextActiveEntry(&scan);

		/*
		 *  Unpack the entry, but don't bother adding
		 *  the bound addresses, because we don't
		 *  need them.
		 */
		(*actlist)[i] = unpackActive(scan.active, scan.active_len);
	}
	unlockShared(&oset);

	return (i);
}

long
NisMappedCache::nextStale()
{
	int i;
	long diff;
	long min = -1;
	int stale_count = 0;
	struct timeval now;
	CacheSection *section;
	BindingEntry scan;
	sigset_t oset;

	gettimeofday(&now, NULL);

	if (!lockShared(&oset)) {
		return (-1);
	}

	section = &header->sections[SECTION_BINDING];
	for (i = 0; i < section->count; i++) {
		if (i == 0)
			firstBinding(&scan);
		else
			nextBinding(&scan);

		diff = scan.exp_time - now.tv_sec;
		if (diff < 0)
			diff = 0;
		if (min == -1 || diff < min)
			min = diff;
		free(scan.broken_name);
	}
	unlockShared(&oset);

	return (min);
}

int
NisMappedCache::align(int n)
{
	return ((n + 3) & ~3);
}

int
NisMappedCache::lockExclusive(sigset_t *sset)
{
	struct sembuf buf;

	thr_sigblock(sset);
	/* set the exclusive flag to indicate that manager wants to write */
	buf.sem_num = NIS_SEM_MGR_EXCL;
	buf.sem_op = 1;
	buf.sem_flg = SEM_UNDO;

	if (semop(sem_writer, &buf, 1) == -1) {
		thr_sigsetmask(SIG_SETMASK, sset, NULL);
		syslog(LOG_ERR, "can't set exclusive lock:  %m");
		return (0);
	}

	/* wait for the reader count to go to 0 */
	buf.sem_num = NIS_SEM_READER;
	buf.sem_op = 0;
	buf.sem_flg = 0;


	if (semop(sem_reader, &buf, 1) == -1) {
		thr_sigsetmask(SIG_SETMASK, sset, NULL);
		syslog(LOG_ERR, "failed waiting on reader count:  %m");
		return (0);
	}

	if (serverMode && mapBase != (char *)-1) {
		header->valid = 0;
		if (msync(mapBase, mapSize, MS_SYNC) == -1) {
			syslog(LOG_ERR, "msync failed:  %s");
			/* what should we do here? */
		}
	}

	return (1);
}

void
NisMappedCache::unlockExclusive(sigset_t *sset)
{
	struct sembuf buf;

	if (serverMode && mapBase != (char *)-1) {
		header->valid = 1;
		if (msync(mapBase, mapSize, MS_SYNC) == -1) {
			syslog(LOG_ERR, "msync failed:  %s");
			/* what should we do here? */
		}
	}

	/* unset the exclusive flag */
	buf.sem_num = NIS_SEM_MGR_EXCL;
	buf.sem_op = -1;
	buf.sem_flg = SEM_UNDO;

	semop(sem_writer, &buf, 1);
	thr_sigsetmask(SIG_SETMASK, sset, NULL);
}

int
NisMappedCache::lockShared(sigset_t *sset)
{
	struct sembuf buf;
	int save_errno;

	thr_sigblock(sset);
	while (1) {
		/* wait for manager to finish exclusive access */
		buf.sem_num = NIS_SEM_MGR_EXCL;
		buf.sem_op = 0;
		buf.sem_flg = 0;

		if (semop(sem_writer, &buf, 1) == -1) {
			thr_sigsetmask(SIG_SETMASK, sset, NULL);
			return (0);
		}

		/* increment reader count */
		buf.sem_num = NIS_SEM_READER;
		buf.sem_op = 1;
		buf.sem_flg = SEM_UNDO;
		if (semop(sem_reader, &buf, 1) == -1) {
			thr_sigsetmask(SIG_SETMASK, sset, NULL);
			return (0);
		}

		/* make sure that the manager hasn't gotten there first */
		buf.sem_num = NIS_SEM_MGR_EXCL;
		buf.sem_op = 0;
		buf.sem_flg = IPC_NOWAIT;   /* don't block */

		if (semop(sem_writer, &buf, 1) == -1) {
			save_errno = errno;

			/* decrement reader count */
			buf.sem_num = NIS_SEM_READER;
			buf.sem_op = -1;
			buf.sem_flg = SEM_UNDO | IPC_NOWAIT;

			if (semop(sem_reader, &buf, 1) == -1) {
				thr_sigsetmask(SIG_SETMASK, sset, NULL);
				return (0);
			}

			errno = save_errno;
			if (errno != EAGAIN) {
				thr_sigsetmask(SIG_SETMASK, sset, NULL);
				return (0);
			}

			/*
			 *  The manager beat us to the cache.  He have
			 *  decremented the reader count, so we can go
			 *  back to the top of the loop and try again.
			 */
			continue;
		}

		/*
		 *  Make sure that we have mapped the whole cache.  It is
		 *  okay to map more than the size of the cache; we won't
		 *  read that far.  We release our shared lock and then
		 *  grab an exclusive lock.  This will prevent any
		 *  interaction between threads on the cache pointers.
		 */
		if (mapBase == (char *)-1 || mapSize < header->map_size) {
			unlockShared(sset);
			lockExclusive(sset);
			unmapCache();
			if (!mapCache()) {
				unlockExclusive(sset);
				return (0);
			}
			unlockExclusive(sset);
			continue;
		}

		break;
	}

	return (1);
}

void
NisMappedCache::unlockShared(sigset_t *sset)
{
	struct sembuf buf;

	buf.sem_num = NIS_SEM_READER;
	buf.sem_op = -1;
	buf.sem_flg = SEM_UNDO | IPC_NOWAIT;

	(void) semop(sem_reader, &buf, 1);
	thr_sigsetmask(SIG_SETMASK, sset, NULL);
}
