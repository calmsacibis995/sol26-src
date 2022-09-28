/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved
 */

#pragma	ident	"@(#)create.c	1.9	96/09/23 SMI"

/*
 * COPYRIGHT NOTICE
 * 
 * This source code is designated as Restricted Confidential Information
 * and is subject to special restrictions in a confidential disclosure
 * agreement between HP, IBM, SUN, NOVELL and OSF.  Do not distribute
 * this source code outside your company without OSF's specific written
 * approval.  This source code, and all copies and derivative works
 * thereof, must be returned or destroyed at request. You must retain
 * this notice on any copies which you make.
 * 
 * (c) Copyright 1990, 1991, 1992, 1993 OPEN SOFTWARE FOUNDATION, INC.
 * ALL RIGHTS RESERVED 
 */
/*
 * OSF/1 1.2
 */
#if !defined(lint) && !defined(_NOIDENT)
static char rcsid[] = "@(#)$RCSfile: create.c,v $ $Revision: 1.2.2.2 $ (OSF) $Date: 1991/10/01 15:53:37 $";
#endif
/* 
 * create.c - Create a tape archive. 
 *
 * DESCRIPTION
 *
 *	These functions are used to create/write and archive from an set of
 *	named files.
 *
 * AUTHOR
 *
 *     	Mark H. Colburn, NAPS International (mark@jhereg.mn.org)
 *
 * Sponsored by The USENIX Association for public distribution. 
 *
 * Copyright (c) 1989 Mark H. Colburn.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice is duplicated in all such 
 * forms and that any documentation, advertising materials, and other 
 * materials related to such distribution and use acknowledge that the 
 * software was developed * by Mark H. Colburn and sponsored by The 
 * USENIX Association. 
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Revision 1.3  89/02/12  10:29:37  mark
 * Fixed misspelling of Replstr
 * 
 * Revision 1.2  89/02/12  10:04:17  mark
 * 1.2 release fixes
 * 
 * Revision 1.1  88/12/23  18:02:06  mark
 * Initial revision
 * 
 */

/* Headers */

#include <sys/sysmacros.h>
#include "pax.h"

/* Messages */

#define	CR_LONG		"The name is longer than 255 bytes."

/* Function Prototypes */

static int writetar(char *, Stat *);
static int writecpio(char *, Stat *);
static char tartype(mode_t);
static int check_update(char *, Stat *);



/* create_archive - create a tar archive.
 *
 * DESCRIPTION
 *
 *	Create_archive is used as an entry point to both create and append
 *	archives.  Create archive goes through the files specified by the
 *	user and writes each one to the archive if it can.  Create_archive
 *	knows how to write both cpio and tar headers and the padding which
 *	is needed for each type of archive.
 *
 * RETURNS
 *
 *	Always returns 0
 */


int
create_archive(void)
{
    char            name[PATH_MAX + 1];
    Stat            sb;
    int             fd;
    int err;

    while (name_next(name, &sb) != -1) {
	if ((fd = openin(name, &sb)) < 0) {
	    continue;
	}

	if (!f_unconditional && (check_update(name, &sb) == 0)) {
	    /* Skip file... one in archive is newer */
	    if (fd) 
		close(fd);
	    continue;
	}

	if (rplhead != (Replstr *)NULL) {
	    rpl_name(name);
	    if (strlen(name) == 0) {
		if (fd)
		    close(fd);
		continue;
	    }
	}
	if (get_disposition(ADD, name) || get_newname(name, sizeof(name))) {
	    /* skip file... */
	    if (fd) {
		close(fd);
	    }
	    continue;
	}

	if (sb.sb_size > (OFFSET)(TAR_OFFSET_MAX)) {
		warn(name, gettext("too large to archive"));
		continue;
	}

	if (!f_link && sb.sb_nlink > 1) {
		if (islink(name, &sb)) {
			sb.sb_size = 0;
		}
		linkto(name, &sb);
	}
	if (ar_format == TAR) {
		err = writetar(name, &sb);
	} else {
		err = writecpio(name, &sb);
	}
	if (err == 0) {
		if (fd) {
			outdata(fd, name, &sb);
		}
		if (f_verbose) {
			print_entry(name, &sb);
		}
	}
    }

    write_eot();
    close_archive();
    return (0);
}


/* writetar - write a header block for a tar file
 *
 * DESCRIPTION
 *
 * 	Make a header block for the file name whose stat info is in st.  
 *
 * 	The tar header block is structured as follows:
 *
 *		FIELD NAME	OFFSET		SIZE
 *      	-------------|---------------|------
 *		name		  0		100
 *		mode		100		  8
 *		uid		108		  8
 *		gid		116		  8
 *		size		124		 12
 *		mtime		136		 12
 *		chksum		148		  8
 *		typeflag	156		  1
 *		linkname	157		100
 *		magic		257		  6
 *		version		263		  2
 *		uname		265		 32
 *		gname		297		 32
 *		devmajor	329		  8
 *		devminor	337		  8
 *		prefix		345		155
 *
 * PARAMETERS
 *
 *	char	*name	- name of file to create a header block for
 *	Stat	*asb	- pointer to the stat structure for the named file
 *
 *
 * RETURN
 *
 *	0 - ok
 *	1 - error - skip file
 */


static int
writetar(char *name, Stat *asb)
{
    char	   *p;
    char           *prefix = (char *)NULL;
    int             i;
    int             sum;
    char            hdr[BLOCKSIZE];
    Link           *from;
    uid_t	uid;
    gid_t	gid;

    memset(hdr, 0, BLOCKSIZE);
    if (strlen(name) > (size_t) TL_NAME+TL_PREFIX) {
	warn(name, MSGSTR(CR_LONG, "Name too long"));
	return (1);
    }
    
    /* 
     * If the pathname is longer than TL_NAME, but less than
     * TL_NAME+TL_PREFIX, then we can split it up into the prefix and the
     * filename. 
     */
    if (strlen(name) > (size_t) TL_NAME) {
	prefix = name;
	name += (strlen(name) - 1);
	while (name > prefix && *name != '/') {
	    name--;
	}

	/* no slash found....hmmm.... */
	if (name == prefix) {
	    warn(MSGSTR(CR_LONG, "Name too long"), prefix);
	    return (1);
	}
	*name++ = '\0';
    }

#ifdef S_IFLNK
    if ((asb->sb_mode & S_IFMT) == S_IFLNK) {
	    if (strlen(asb->sb_link) > (size_t) TL_LINKNAME) {
		    warn(gettext("Link name too long"), asb->sb_link);
		    return (1);
	    }

	    strncpy(&hdr[TO_LINKNAME], asb->sb_link, TL_LINKNAME);
	    asb->sb_size = 0;
    }
#endif
    if ((u_long)(uid = asb->sb_uid) > (u_long)07777777)
	    uid = UID_NOBODY;
    if ((u_long)(gid = asb->sb_gid) > (u_long)07777777)
	    gid = GID_NOBODY;

    (void) strncpy(&hdr[TO_NAME], name, TL_NAME);
    (void) sprintf(&hdr[TO_MODE], "%07o", asb->sb_mode & ~S_IFMT);
    (void) sprintf(&hdr[TO_UID], "%07o", uid);
    (void) sprintf(&hdr[TO_GID], "%07o", gid);
    (void) sprintf(&hdr[TO_SIZE], "%011llo", (OFFSET) asb->sb_size);
    (void) sprintf(&hdr[TO_MTIME], "%011lo", (long) asb->sb_mtime);
    (void) memset(&hdr[TO_CHKSUM], ' ', 8);

    if ((hdr[TO_TYPEFLG] = tartype(asb->sb_mode)) == (char) 0) {
	    fprintf(stderr, gettext("%s: not a file (mode = %%x%04x): %s\n"),
		    myname, asb->sb_mode & S_IFMT, name);
	    fprintf(stderr, gettext("not dumped\n"));
	    exit_status = 1;
	    return (1);
    }

    if (asb->sb_nlink > 1 && (from = linkfrom(name, asb)) != (Link *)NULL) {
	strncpy(&hdr[TO_LINKNAME], from->l_name, TL_LINKNAME);
	hdr[TO_TYPEFLG] = LNKTYPE;
    }
    strcpy(&hdr[TO_MAGIC], TMAGIC);
    strncpy(&hdr[TO_VERSION], TVERSION, TL_VERSION);
    strcpy(&hdr[TO_UNAME], finduname((int) asb->sb_uid));
    strcpy(&hdr[TO_GNAME], findgname((int) asb->sb_gid));
    sprintf(&hdr[TO_DEVMAJOR], "%07o", major(asb->sb_rdev));
    sprintf(&hdr[TO_DEVMINOR], "%07o", minor(asb->sb_rdev));
    if (prefix != (char *)NULL) {
	strncpy(&hdr[TO_PREFIX], prefix, TL_PREFIX);
    }

    /* Calculate the checksum */

    sum = 0;
    p = hdr;
    for (i = 0; i < 500; i++) {
	sum += 0xFF & *p++;
    }

    /* Fill in the checksum field. */

    sprintf(&hdr[TO_CHKSUM], "%07o", sum);

    outwrite(hdr, BLOCKSIZE);
    return (0);
}


/* tartype - return tar file type from file mode
 *
 * DESCRIPTION
 *
 *	tartype returns the character which represents the type of file
 *	indicated by "mode". A binary 0 indicates that an unrecognizable
 *      file type was detected.
 *
 * PARAMETERS
 *
 *	mode_t	mode	- file mode from a stat block
 *
 * RETURNS
 *
 *	The character which represents the particular file type in the 
 *	ustar standard headers. Binary 0 if there was a problem.
 */


static char
tartype(mode_t mode)
{
    switch (mode & S_IFMT) {

#ifdef S_IFCTG
    case S_IFCTG:
	return(CONTTYPE);
#endif

    case S_IFDIR:
	return (DIRTYPE);

#ifdef S_IFLNK
    case S_IFLNK:
	return (SYMTYPE);
#endif

#ifdef S_IFIFO
    case S_IFIFO:
	return (FIFOTYPE);
#endif

#ifdef S_IFCHR
    case S_IFCHR:
	return (CHRTYPE);
#endif

#ifdef S_IFBLK
    case S_IFBLK:
	return (BLKTYPE);
#endif

#ifdef S_IFREG
    case S_IFREG:
	return (REGTYPE);
#endif

    default:
	return((char) 0);
    }
}


/* writecpio - write a cpio archive header
 *
 * DESCRIPTION
 *
 *	Writes a new CPIO style archive header for the file specified.
 *
 * PARAMETERS
 *
 *	char	*name	- name of file to create a header block for
 *	Stat	*asb	- pointer to the stat structure for the named file
 */


static int
writecpio(char *name, Stat *asb)
{
    uint            namelen;
    char            header[M_STRLEN + H_STRLEN + 1];
    mode_t	mode;
    uid_t	uid;
    gid_t	gid;

    namelen = (uint) strlen(name) + 1;
    strcpy(header, M_ASCII);
    
    mode = asb->sb_mode;
    if ((u_long)(uid = asb->sb_uid) > (u_long)0777777) {
	    warn(name, gettext("uid too large for cpio archive format"));
	    uid = UID_NOBODY;
	    if (S_ISREG(mode))
		    mode &= ~S_ISUID;
    }
    if ((u_long)(gid = asb->sb_gid) > (u_long)0777777) {
	    warn(name, gettext("gid too large for cpio archive format"));
	    gid = GID_NOBODY;
	    if (S_ISREG(mode))
		    mode &= ~S_ISGID;
    }
    (void) sprintf(header + M_STRLEN, "%06o%06o%06o%06o%06o",
	    USH(asb->sb_dev), USH(asb->sb_ino), USH(mode), uid, gid);
    (void) sprintf(header + M_STRLEN + 30, "%06o%06o%011lo%06o%011llo",
	    USH(asb->sb_nlink), USH(asb->sb_rdev),
	    f_mtime ? asb->sb_mtime : time((time_t *) 0),
	    namelen, asb->sb_size);
    outwrite(header, M_STRLEN + H_STRLEN);
    outwrite(name, namelen);
#ifdef	S_IFLNK
    if ((asb->sb_mode & S_IFMT) == S_IFLNK) {
	outwrite(asb->sb_link, (uint) asb->sb_size);
    }
#endif	/* S_IFLNK */
    return (0);
}


/* check_update - compare modification times of archive file and real file.
 *
 * DESCRIPTION
 *
 *	check_update looks up name in the hash table and compares the
 *	modification time in the table iwth that in the stat buffer.
 *
 * PARAMETERS
 *
 *	char	*name	- The name of the current file
 *	Stat	*sb	- stat buffer of the current file
 *
 * RETURNS
 *
 *	1 - if the names file is new than the one in the archive
 *	0 - if we don't want to add this file to the archive.
 */
static int
check_update(char *name, Stat *sb)
{
    return (sb->sb_mtime > hash_lookup(name));
}
