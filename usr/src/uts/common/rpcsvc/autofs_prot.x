%/*
% *	Copyright (c) 1993 - 1996 Sun Microsystems Inc
% *	All Rights Reserved.
% */
%
%#pragma ident	"@(#)autofs_prot.x	1.10	96/05/03 SMI"
%
%#include <sys/vfs.h>
%#include <sys/dirent.h>
%
%#define	xdr_dev_t xdr_u_long
%#define	xdr_bool_t xdr_bool
%
/*
 * Autofs/automountd communication protocol.
 */

const AUTOFS_MAXPATHLEN		= 1024;
const AUTOFS_MAXCOMPONENTLEN	= 255;
const AUTOFS_MAXOPTSLEN		= 255;
const AUTOFS_DAEMONCOOKIE	= 100000;

/*
 * Action Status
 * Automountd replies to autofs indicating whether
 * the operation is done, or further action needs to be taken
 * by autofs.
 */
enum autofs_stat {
	AUTOFS_ACTION=0,	/* list of actions included */
	AUTOFS_DONE=1		/* no further action required by kernel */
};

/*
 * Used by autofs to either create a link,
 * or mount a new filesystem.
 */
enum autofs_action {
	AUTOFS_MOUNT_RQ=0,	/* mount request */
	AUTOFS_LINK_RQ=1	/* link create */
};

enum autofs_res {
	AUTOFS_OK=0,
	AUTOFS_NOENT=2,
	AUTOFS_ECOMM=5,
	AUTOFS_NOMEM=12,
	AUTOFS_NOTDIR=20,
	AUTOFS_SHUTDOWN=1000
};

/*
 * Lookup/Mount request.
 * Argument structure passed to both autofs_lookup() and autofs_mount().
 * autofs_lookup():
 *	Query automountd if 'path/subdir/name' exists in 'map'
 * autofs_mount():
 *	Request automountd to mount the map entry associated with
 *	'path/subdir/name' in 'map' given 'opts' options.
 */
struct autofs_lookupargs {
	string	map<AUTOFS_MAXPATHLEN>;		/* context or map name */
	string	path<AUTOFS_MAXPATHLEN>;	/* mountpoint */
	string	name<AUTOFS_MAXCOMPONENTLEN>;	/* entry we're looking for */
	string	subdir<AUTOFS_MAXPATHLEN>;	/* subdir within map */
	string	opts<AUTOFS_MAXOPTSLEN>;
	bool_t	isdirect;			/* direct mountpoint? */
};

/*
 * Symbolic link information.
 */
struct linka {
	string	dir<AUTOFS_MAXPATHLEN>;		/* original name */
	string	link<AUTOFS_MAXPATHLEN>;	/* link (new) name */
};

/*
 * We don't define netbuf in RPCL, we include the header file that
 * includes it, and implement the xdr function ourselves.
 */

/*
 * Autofs Mount specific information - used to mount a new
 * autofs filesystem.
 */
struct autofs_args {
	struct netbuf	addr;		/* daemon address */
	string path<AUTOFS_MAXPATHLEN>;	/* autofs mountpoint */
	string opts<AUTOFS_MAXOPTSLEN>;	/* default mount options */
	string map<AUTOFS_MAXPATHLEN>;	/* name of map */
	string subdir<AUTOFS_MAXPATHLEN>; /* subdir within map */
	string key<AUTOFS_MAXCOMPONENTLEN>; /* used in direct mounts only */
	int		mount_to;	/* time in sec the fs is to remain */
					/* mounted after last reference */
	int		rpc_to;		/* timeout for rpc calls */
	int		direct;		/* 1 = direct mount */
};

/*
 * Contains the necessary information to notify autofs to
 * perfom either a new mount or create a symbolic link.
 */
union action_list_entry switch (autofs_action action) {
case AUTOFS_MOUNT_RQ:
	struct mounta mounta;
case AUTOFS_LINK_RQ:
	struct linka linka;
default:
	void;
};

/*
 * List of actions that need to be performed by autofs to
 * finish the requested operation.
 */
struct action_list {
	action_list_entry action;
	action_list *next;
};

union mount_result_type switch (autofs_stat status) {
case AUTOFS_ACTION:
	action_list *list;
case AUTOFS_DONE:
	int error;
default:
	void;
};

/*
 * Result from mount operation.
 */
struct autofs_mountres {
	mount_result_type mr_type;
	int mr_verbose;
};

union lookup_result_type switch (autofs_action action) {
case AUTOFS_LINK_RQ:
	struct linka lt_linka;
default:
	void;
};

/*
 * Result from lookup operation.
 */
struct autofs_lookupres {
	enum autofs_res lu_res;
	lookup_result_type lu_type;	
	int lu_verbose;
};

/*
 * Post mount operation request
 * Currently defined to add the requested information
 * to /etc/mnttab.
 */
struct postmountreq {
	string	special<AUTOFS_MAXPATHLEN>;
	string	mountp<AUTOFS_MAXPATHLEN>;
	string	fstype<AUTOFS_MAXCOMPONENTLEN>;
	string	mntopts<AUTOFS_MAXOPTSLEN>;
	dev_t	devid;
};

/*
 * Post mount operation result
 * status = 0 if post mount request was successful,
 * otherwise status = errno.
 */
struct postmountres {
	int status;
};

/*
 * Unmount operation request
 * Automountd will match the given devid, rdevid pair to the
 * appropriate path in /etc/mnttab and use it to issue the
 * unmount system call.
 */
struct umntrequest {
	bool_t isdirect;		/* direct mount? */
	dev_t devid;			/* device id */
	dev_t rdevid;			/* rdevice id */
	struct umntrequest *next;	/* next unmount */
};

/*
 * Unmount operation result
 * status = 0 if unmount was successful,
 * otherwise status = errno.
 */
struct umntres {
	int status;
};

/*
 * Post unmount operation request
 * Currently defined to remove the requested entry
 * from /etc/mnttab.
 */
struct postumntreq {
	dev_t devid;			/* device id */
	dev_t rdevid;			/* rdevice id */
	struct postumntreq *next;	/* next post unmount */
};

/*
 * Post unmount operation result
 * status = 0 if succesful,
 * otherwise status = errno.
 */
struct postumntres {
	int status;
};

/*
 * AUTOFS readdir request
 * Request list of entries in 'rda_map' map starting at the given
 * offset 'rda_offset', for 'rda_count' bytes.
 */
struct autofs_rddirargs {
	string	rda_map<AUTOFS_MAXPATHLEN>;
	u_long	rda_offset;		/* starting offset */
	u_long	rda_count;		/* total size requested */
};

struct autofsrddir {
	u_long	rddir_offset;		/* last offset in list */
	u_long	rddir_size;		/* size in bytes of entries */
	bool_t	rddir_eof;		/* TRUE if last entry in result */
	struct dirent64 *rddir_entries;	/* variable number of entries */
};

/*
 * AUTOFS readdir result.
 */
struct autofs_rddirres {
	enum autofs_res rd_status;
	u_long rd_bufsize;		/* autofs request size (not xdr'ed) */
	struct autofsrddir rd_rddir;
};

/*
 * AUTOFS routines.
 */
program AUTOFS_PROG {
	version AUTOFS_VERS {
		void
		AUTOFS_NULL(void) = 0;

		autofs_mountres
		AUTOFS_MOUNT(autofs_lookupargs) = 1;

		umntres
		AUTOFS_UNMOUNT(umntrequest) = 2;

		postumntres
		AUTOFS_POSTUNMOUNT(postumntreq) = 3;

		postmountres
		AUTOFS_POSTMOUNT(postmountreq) = 4;

		autofs_rddirres
		AUTOFS_READDIR(autofs_rddirargs) = 5;

		autofs_lookupres
		AUTOFS_LOOKUP(autofs_lookupargs) = 6;
	} = 2;
} = 100099;
