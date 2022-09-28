/*
 * Copyright (c) 1995-1996, Sun Microsystems, Inc.  All Rights Reserved.
 */

#ident "@(#)dosdirs.c	1.11	96/05/15 SMI"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ramfile.h>
#include <sys/doserr.h>
#include <sys/dosemul.h>
#include <sys/fcntl.h>
#include <sys/bootvfs.h>
#include <sys/salib.h>
#include <sys/promif.h>

extern int int21debug;

struct dos_fninfo *DOScurrent_dta;
ffinfo *DOS_ffinfo_list;

static char *
builddosname(char *sp)
{
	static char bn[DOSMAX_FILNAM+1];
	char *dp = bn;
	int nl = 0, el = 0;

	/*
	 * Because of the limitations of DOS filenames, we will only
	 * actually build a non-empty name if the file name fits within
	 * the rigid standards of the DOS naming convention.  There is
	 * no use in reporting mangled names because if they send back
	 * requests to open the files we wouldn't be able to!!
	 *
	 * Of course the two notable exceptions are that we need to pass
	 * back . and ..
	 */
	if (strcmp(sp, ".") == 0 || strcmp(sp, "..") == 0) {
		(void) strcpy(dp, sp);
		return (bn);
	}

	if (*sp != '.') {
		/* Name part is limited in its number of characters */
		while (*sp && *sp != '.' && nl < DOSMAX_NMPART) {
			*dp++ = *sp++; nl++;
		}

		/* Name part too long? */
		if (*sp && *sp != '.') {
			dp = bn;
		} else if (*sp) {
			*dp++ = *sp++;
			while (*sp && el < DOSMAX_EXTPART) {
				*dp++ = *sp++; el++;
			}
			if (*sp)
				dp = bn;
		}
	}

	/* Last character of name should be terminating null */
	*dp = '\0';
	return (bn);
}

static int
dosfilldta(struct dirent *dep, ffinfo *ffip)
{
	static struct stat statbuf;
	char *sn, *rn;
	rffd_t *rfd;
	int fd = -1, rv = -1;
	int sr, snl;

	if (int21debug)
		printf("dosfilldta ");

	snl = strlen(ffip->curmatchpath) + 2 + strlen(dep->d_name);

	if (sn = (char *)bkmem_alloc(snl)) {
		(void) strcpy(sn, ffip->curmatchpath);
		(void) strcat(sn, "/");
		(void) strcat(sn, dep->d_name);
	} else {
		if (int21debug)
			printf("No space for path to stat!\n");
		return (rv);
	}

	/* Strip off 'boottree' prefix to get a valid RAMfile name */
	rn = RAMfile_striproot(sn);

	/* Might be a RAMfile, check there first */
	if ((rfd = RAMfile_open(rn, O_RDONLY)) != (rffd_t *)NULL &&
	    !(sr = RAMfile_fstat(rfd, &statbuf))) {
		if (int21debug)
			printf("{Rsize %d} ", statbuf.st_size);
		DOScurrent_dta->attr = '\0';
		DOScurrent_dta->time[0] = '\0';
		DOScurrent_dta->time[1] = '\0';
		DOScurrent_dta->date[0] = '\0';
		DOScurrent_dta->date[1] = '\0';
		DOScurrent_dta->size = statbuf.st_size;
		(void) strcpy(DOScurrent_dta->name,
			builddosname(dep->d_name));
		rv = 0;
	} else if (((fd = open(sn, O_RDONLY)) >= 0) &&
	    !(sr = fstat(fd, &statbuf))) {
		int accept = 1;

		if (int21debug)
			printf("{mode %x, size %d} ", statbuf.st_mode,
			    statbuf.st_size);

		if (S_ISDIR(statbuf.st_mode)) {
			if (ffip->curmatchattr & DOSATTR_DIR) {
				DOScurrent_dta->attr = DOSATTR_DIR;
			} else {
				accept = 0;
			}
		} else {
			DOScurrent_dta->attr = '\0';
		}

		if (accept) {
			DOScurrent_dta->time[0] = '\0';
			DOScurrent_dta->time[1] = '\0';
			DOScurrent_dta->date[0] = '\0';
			DOScurrent_dta->date[1] = '\0';
			DOScurrent_dta->size = statbuf.st_size;
			(void) strcpy(DOScurrent_dta->name,
				builddosname(dep->d_name));
			if (int21debug)
				printf("-ATTR-0x%x-", DOScurrent_dta->attr);
			rv = 0;
		}
	}

	if (int21debug && fd < 0)
		printf("(open failed %s) ", sn);
	else if (int21debug && sr)
		printf("(stat failed %s) ", sn);

	if (rfd)
		(void) RAMfile_close(rfd);
	if (fd >= 0)
		(void) close(fd);
	bkmem_free(sn, snl);

	return (rv);
}

static int
regexmatch(char *fn, char *re)
{
	if (!*fn && !*re)
		return (1);

	if (!*fn && *re == '*')
		return (regexmatch(fn, ++re));

	if (!*re || !*fn)
		return (0);

	if (*fn == *re || *re == '?')
		return (regexmatch(++fn, ++re));

	if (*re == '*') {
		if (regexmatch(++fn, ++re))
			return (1);
		else if (regexmatch(fn, --re))
			return (1);
		else
			return (regexmatch(--fn, ++re));
	}
	return (0);
}

static int
isamatch(char *f, char *re)
{
	char *fn = builddosname(f);

	if (int21debug)
		printf("Isamatch <%s> <%s> ", fn, re);
	return (regexmatch(fn, re));
}

static int
reportnxtmatch(ffinfo *ffip)
{
	extern int kern_getdents(int, struct dirent *, u_int);
	int rv = -1;

	if (ffip->requestreset) {
		if (ffip->dirfd >= 0)
			(void) close(ffip->dirfd);
		ffip->dirfd = ffip->nextdentidx = ffip->curdent = -1;
		ffip->requestreset = 0;
	}

	if (!ffip->curmatchfile || !ffip->curmatchpath) {
		if (int21debug)
			printf("Empty path or filename.");
		return (rv);
	}

	if (!ffip->dentbuf &&
	    !(ffip->dentbuf = (char *)bkmem_alloc(DOSDENTBUFSIZ))) {
		/* Can't do much without a dirent buffer! */
		prom_panic("no memory for dir ents!!");
	}

	/* Open directory if necessary */
	if (ffip->dirfd < 0) {
		if ((ffip->dirfd = open(ffip->curmatchpath, O_RDONLY)) < 0) {
			if (int21debug)
				printf("Dir open failed: %s.",
				    ffip->curmatchpath);
			/*
			 * We used to return here.  But its actually possible
			 * that we might fail to open the directory because
			 * it only exists in the RAMfile file system.  So, now
			 * we accept the failure and maybe the RAMfile_patch
			 * routine will get us some entries for this
			 * particular directory.
			 */
		} else {
			/* Read first set of directory entries */
			if ((ffip->maxdent = kern_getdents(ffip->dirfd,
			    (struct dirent *)ffip->dentbuf,
			    DOSDENTBUFSIZ)) > 0) {
				if (int21debug)
					printf("[%d] dents available ",
					    ffip->maxdent);
				ffip->nextdentidx = 0;
				ffip->curdent = 0;
			} else {
				if (int21debug)
					printf("(getdents failed (%d)) ",
					    ffip->maxdent);
				return (rv);
			}
		}
	}

	/*
	 * RAMfiles can potentially add both new files and new
	 * directories to the file system.  Call the routine that
	 * patches the dirents with any new items in the RAMfiles list.
	 */
	RAMfile_patch_dirents(ffip);

	while (ffip->nextdentidx >= 0) {
		struct dirent *de;

		de = (struct dirent *)&(ffip->dentbuf[ffip->nextdentidx]);
		if (int21debug)
			printf("de is %x [%d]", de, ffip->curdent);
		if (ffip->curdent < ffip->maxdent) {
			ffip->nextdentidx += de->d_reclen;
			ffip->curdent++;
			if (isamatch(de->d_name, ffip->curmatchfile)) {
				if (!dosfilldta(de, ffip)) {
					if (int21debug)
						printf("(Matched %s) ",
						    de->d_name);
					rv = 1;
					break;
				}
			}
		} else {
			/*
			 * It appears getdents will just start over from
			 * the beginning and thus put me in an infinite
			 * loop.  Until getdents is fixed, don't try to
			 * get more entries.
			 *
				if ((ffip->maxdent = kern_getdents(
				    ffip->dirfd,
				    (struct dirent *)ffip->dentbuf,
				    DOSDENTBUFSIZ)) > 0) {
					ffip->nextdentidx = 0;
					ffip->curdent = 0;
				} else {
					(void) close(ffip->dirfd);
					ffip->dirfd = ffip->nextdentidx = -1;
					ffip->curdent = -1;
					break;
				}
			 */
			(void) close(ffip->dirfd);
			ffip->dirfd = ffip->nextdentidx = ffip->curdent = -1;
			break;
		}
	}
	return (rv);
}

static char *
dosparsepath(char *fullpath, char *buf)
{
	char *t;
	char *DOSpathsearch;

	if (buf == 0) {
		DOSpathsearch = (char *)bkmem_alloc(MAXPATHLEN);
		if (DOSpathsearch == 0) {
			prom_panic("No memory for directory lookups.");
		}
	} else {
		DOSpathsearch = buf;
	}

	*DOSpathsearch = '\0';

	t = fullpath + strlen(fullpath);
	while (t >= fullpath) {
		if (*t == '/') {
			*t = '\0';
			(void) strcpy(DOSpathsearch, fullpath);
			*t = '/';
			break;
		}
		t--;
	}

	if (!*DOSpathsearch)
		(void) strcpy(DOSpathsearch, ".");
	else if (--t > fullpath && *t == ':')
		(void) strcat(DOSpathsearch, "/");
	return (DOSpathsearch);
}

static char *
dosparsefile(char *fullpath, char *buf)
{
	char *t;
	char *DOSfilesearch;

	if (buf == 0) {
		DOSfilesearch = (char *)bkmem_alloc(MAXPATHLEN);
		if (DOSfilesearch == 0) {
			prom_panic("No memory for file lookups.");
		}
	} else {
		DOSfilesearch = buf;
	}
	*DOSfilesearch = '\0';

	t = fullpath + strlen(fullpath);
	while (t >= fullpath) {
		if (*t == '/') {
			(void) strcpy(DOSfilesearch, t+1);
			break;
		}
		t--;
	}

	if (!*DOSfilesearch)
		(void) strcpy(DOSfilesearch, fullpath);

	return (DOSfilesearch);
}

void
dosfindfirst(struct real_regs *rp)
{
	extern char *dos_formpath(char *);
	ffinfo *ffip;
	char *ufn = dosfn_to_unixfn(DS_DX(rp));
	char *matchme;

	if (int21debug)
		printf("dosfndfrst ");

	/*
	 * Find the state structure for the current dta.
	 */
	for (ffip = DOS_ffinfo_list; ffip; ffip = ffip->next)
		if (ffip->cookie == DOScurrent_dta)
			break;

	if (ffip == 0) {
		ffip = (ffinfo *)bkmem_alloc(sizeof (ffinfo));
		if (ffip == 0)
			prom_panic("dosfindfirst: no mem");
		ffip->cookie = DOScurrent_dta;
		ffip->next = DOS_ffinfo_list;
		DOS_ffinfo_list = ffip;
	}

	ffip->requestreset = 1;
	ffip->curdoserror = DOSERR_FILENOTFOUND;

	if (matchme = dos_formpath(ufn)) {
		ffip->curmatchpath = dosparsepath(matchme, ffip->curmatchpath);
		ffip->curmatchfile = dosparsefile(matchme, ffip->curmatchfile);
		ffip->curmatchattr = CX(rp);

		if (int21debug) {
			printf("cp <%s>, cf <%s>, mm <%s>, ma <%x> ",
				ffip->curmatchpath ?
				    ffip->curmatchpath : "NONE",
				ffip->curmatchfile ?
				    ffip->curmatchfile : "NONE",
				matchme ? matchme : "NONE",
				ffip->curmatchattr);
		}

		if (reportnxtmatch(ffip) > 0) {
			ffip->curdoserror = DOSERR_NOMOREFILES;
			CLEAR_CARRY(rp);
		} else {
			if (int21debug)
				printf("(not found) ");
			AX(rp) = ffip->curdoserror;
			SET_CARRY(rp);
		}
	} else {
		if (int21debug)
			printf("(path not found) ");
		AX(rp) = ffip->curdoserror;
		SET_CARRY(rp);
	}
}

void
dosfindnext(struct real_regs *rp)
{
	ffinfo *ffip;

	if (int21debug)
		printf("dosfindnxt ");

	/*
	 * Find the state structure for the current dta.
	 */
	for (ffip = DOS_ffinfo_list; ffip; ffip = ffip->next)
		if (ffip->cookie == DOScurrent_dta)
			break;
	if (ffip == 0)
		prom_panic("findnext: no dta");

	if (reportnxtmatch(ffip) > 0) {
		CLEAR_CARRY(rp);
	} else {
		if (int21debug)
			printf("(not found) ");
		AX(rp) = ffip->curdoserror;
		SET_CARRY(rp);
	}
}

void
dosgetcwd(struct real_regs *rp)
{
	char *store = DS_SI(rp);

	if (int21debug)
		printf("dosgetcwd ");

	/* Indicate user is in root directory */
	*store = '\0';
	CLEAR_CARRY(rp);
}
