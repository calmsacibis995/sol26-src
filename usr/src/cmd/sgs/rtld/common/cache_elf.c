/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)cache_elf.c	1.9	96/08/20 SMI"

#include	"_synonyms.h"

#include	<sys/mman.h>
#include	<sys/stat.h>
#include	<fcntl.h>
#include	<limits.h>
#include	<stdio.h>
#include	<string.h>
#include	"libld.h"
#include	"debug.h"
#include	"_rtld.h"
#include	"msg.h"

int
elf_cache(const char ** file)
{
	char		path[PATH_MAX];
	int		fd;
	off_t		size;
	struct stat	status;
	Ehdr *		ehdr;

	DEF_TIME(interval1);
	DEF_TIME(interval2);

	GET_TIME(interval1);

	/*
	 * If an alternative cache directory has been specified build the
	 * required cache filename (this is only allowed for non-secure
	 * applications), otherwise try opening up the default.
	 */
	if (cd_dir && !(rtld_flags & RT_FL_SECURE)) {
		(void) sprintf(path, MSG_ORIG(MSG_FMT_PATH), cd_dir,
		    MSG_ORIG(MSG_FIL_CACHE));
		*file = (const char *)path;
	} else
		*file = MSG_ORIG(MSG_PTH_CACHE);

	/*
	 * If we can't open the cache return silently.
	 */
	if ((fd = open(*file, O_RDONLY, 0)) == -1) {
		return (DBG_SUP_PRCFAIL);
	}

	/*
	 * Determine the cache file size and map the file.
	 */
	(void) fstat(fd, &status);
	size = (sizeof (Ehdr) + sizeof (Rtc_head));
	if (status.st_size < size) {
		(void) close(fd);
		return (DBG_SUP_CORRUPT);
	}
	/* LINTED */
	if ((ehdr = (Ehdr *)mmap(0, status.st_size, PROT_READ, MAP_SHARED,
	    fd, 0)) == MAP_FAILED) {
		(void) close(fd);
		return (DBG_SUP_PRCFAIL);
	}

	/*
	 * Determine the location of the cache data within the mapped image, and
	 * perform some sanity checks to try and validate the cache files
	 * integrity
	 */
	cachehead = (Rtc_head *)((Word)ehdr + ehdr->e_entry);
	if (status.st_size < (size + cachehead->rtc_size)) {
		(void) close(fd);
		(void) munmap((caddr_t)ehdr, status.st_size);
		return (DBG_SUP_CORRUPT);
	}

	/*
	 * Make sure the cache has been mapped to the correct location.
	 */
	if (cachehead->rtc_base != (Word)cachehead) {
		(void) close(fd);
		(void) munmap((caddr_t)ehdr, status.st_size);
		return (DBG_SUP_MAPINAP);
	}

	/*
	 * Select those entries from the cache we can process.
	 */
	if (cachehead->rtc_objects != 0) {

		/*
		 * Reserve the address space to which these objects would be
		 * mapped.
		 */
		if (mmap((caddr_t)cachehead->rtc_begin,
		    (size_t)((Word)cachehead->rtc_end -
		    (Word)cachehead->rtc_begin), PROT_READ,
		    (MAP_SHARED | MAP_FIXED), fd, 0) == MAP_FAILED) {
			(void) close(fd);
			(void) munmap((caddr_t)ehdr, status.st_size);
			return (DBG_SUP_RESFAIL);
		}
		rtld_flags |= RT_FL_CACHEAVL;
	}

	GET_TIME(interval2);
	SAV_TIME(interval1, " cache initialize");
	SAV_TIME(interval2, " cache processing complete");

	if (cachehead->rtc_flags & RTC_FLG_DEBUG) {
		if (cd_dir) {
			(void) sprintf(path, MSG_ORIG(MSG_FMT_PATH), cd_dir,
			    MSG_ORIG(MSG_FIL_DEBUG));
			cd_file = (char *)malloc(strlen(path) + 1);
			(void) strcpy((char *)cd_file, path);
		} else
			cd_file = MSG_ORIG(MSG_PTH_DEBUG);
	}
	(void) close(fd);

	return (0);
}
