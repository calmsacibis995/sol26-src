/*
 *	Copyright (c) 1996, by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ident	"@(#)cold_start.cc	1.17	96/05/22 SMI"

/*
 * Ported from SCCS version :
 * "@(#)cold_start.cc  1.16  91/03/14  Copyr 1988 Sun Micro";
 *
 *
 *  This file contains all the procedures that operate on the NIS
 *  cold start file.
 *  The NIS cold start file contains one directory object of the home
 *  domain that we trust. This file is created out of band, by the
 *  program nisinit or some other such facility. This trusted directory
 *  object is used to bootstrap the whole chain of trust that the
 *  authentication in NIS is based upon.
 *  This file is read in by the cachemgr when it starts up, and if
 *  the cache manager is not running, then by the LocaClientCache
 *  in each process.
 *  The cachemgr also updates this file when it gets a new directory
 *  object for this entry.
 */

#include "../gen/nis_local.h"
#include 	<stdlib.h>
#include 	<string.h>
#include 	<rpc/types.h>
#include 	<rpc/xdr.h>
#include 	<syslog.h>
#include 	<sys/file.h>
#include 	<sys/stat.h>
#include 	<sys/param.h>
#include 	<unistd.h>
#include 	<fcntl.h>
#include "cache.h"





/*
 * Routine to read from the cold start file.
 * Reads in a directory object into *dobj.
 * The cold start file contains an XDR'ed directory object
 * that is the "home" directory object and is used to establish
 * trust.
 * The file is normally created by the program nisinit.
 * It is read by the cachemgr on startup and by the local cache if
 * it needs to create a local, per-process cache.
 */


bool_t
readColdStartFile(char *fileName, directory_obj *dobj)
{
	FILE 		*fp;
	XDR 		xdrs;
	struct timeval 	now;
	bool_t 		ret_val = TRUE;

	memset((void*)dobj, 0, sizeof (directory_obj));
	if (!(fp = fopen(fileName, "r"))) {
		return (FALSE);
	}

	xdrstdio_create(&xdrs, fp, XDR_DECODE);
	if (!xdr_directory_obj(&xdrs, dobj)) {
		ret_val = FALSE;
	}
	fclose(fp);

	// change the absolute time in the stored directory object
	// back into a ttl that is the field in the directory object.
	if (ret_val) {
		gettimeofday(&now, NULL);
		dobj->do_ttl = ((now.tv_sec > dobj->do_ttl) ?
			    0 : (dobj->do_ttl - now.tv_sec));
	}
	return (ret_val);
}


/*
 * Writes out the directory object in dobj into the cold start file
 * in XDR form.
 * Converts the ttl field into absolute time that is converted back
 * into ttl whenever this entry is read.
 */

bool_t
__nis_writeColdStartFile(char *fileName, directory_obj *dobj)
{
	FILE 		*fp;
	int 		fd;
	XDR 		xdrs;
	struct timeval 	now;
	char 		tempName[MAXPATHLEN+1];


	// We do the standard create-a-temp-file-and-rename-it thing so
	// that writers and readers coexist happily.  This means that
	// we have to have write permission on the directory; so be it.
	// Symbolic links may also cause grief.

	sprintf(tempName, "%s.tmpXXXXXX", fileName);
	mktemp(tempName);
	fd = open(tempName, O_WRONLY|O_SYNC|O_CREAT|O_TRUNC, 0644);
	if (fd < 0) {
		syslog(LOG_ERR,
	"NIS+: writeColdStartFile cannot open file '%s' for writing: %m",
		    tempName);
		return (FALSE);
	}
	// get a stream for xdr
	if (!(fp = fdopen(fd, "w"))) {
		syslog(LOG_ERR,
		    "NIS+: writeColdStartFile: fdopen() failed for '%s': %m",
		    tempName);
		close(fd);
		unlink(tempName);
		return (FALSE);
	}
	// make sure the file has the right permissions
	// writable by root, and readable by everybody else
	if (fchmod(fd, 0644) == -1) {
		syslog(LOG_ERR,
	"NIS+: writeColdStartFile: could not chmod cold_start file: %m");
		goto err;
	}
	xdrstdio_create(&xdrs, fp, XDR_ENCODE);

	// change time to live in the directory object into absolute time.
	// this has to be reconverted back into a ttl when the directory
	// object is read
	gettimeofday(&now, NULL);
	dobj->do_ttl += now.tv_sec;
	if (!xdr_directory_obj(&xdrs, (directory_obj*) dobj)) {
		syslog(LOG_ERR,
		    "NIS+: writeColdStartFile: xdr_directory_obj failed");
		goto err;
	}

	fclose(fp);
	close(fd);

	// rename the temporary file to the actual cold start file file
	if (rename(tempName, fileName) != 0) {
		syslog(LOG_ERR,
	"NIS+: writeColdStartFile: error while renaming '%s' to '%s': (%m)",
		    tempName, fileName);
		unlink(tempName);
		return (FALSE);
	}
	return (TRUE);

err:
	fclose(fp);
	close(fd);
	unlink(tempName);
	return (FALSE);
}



/*
 * C interface to write Coldstart file
 * This routine is called to write the cold start file
 * called by the program nisinit
 */

extern "C" bool_t
writeColdStartFile_unsafe(directory_obj *dobj)
{
	return (__nis_writeColdStartFile(COLD_START_FILE, dobj));
}

extern "C" bool_t
writeColdStartFile(directory_obj *dobj)
{
	bool_t rc;
	static mutex_t ColdStart_lock = DEFAULTMUTEX;

	mutex_lock(&ColdStart_lock);
	rc = __nis_writeColdStartFile(COLD_START_FILE, dobj);
	mutex_unlock(&ColdStart_lock);
	return (rc);
}
