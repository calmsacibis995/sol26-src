/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident "@(#)ramfile.c	1.12	96/05/15 SMI"

#include <sys/bootconf.h>
#include <sys/ramfile.h>
#include <sys/doserr.h>
#include <sys/dosemul.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <sys/bootvfs.h>
#include <sys/salib.h>
#include "devtree.h"

/*
 *  RAMfile globals
 */
static rfil_t *RAMfileslist;			/* The entire file system */
static int num_delayed;

/*
 *  External functions and structures
 */
extern struct bootops *bop;
extern struct dnode *active_node;
extern char *dos_formpath(char *);
extern int bgetproplen(struct bootops *, char *, phandle_t);
extern int bgetprop(struct bootops *, char *, caddr_t, int, phandle_t);
extern int bsetprop(struct bootops *, char *, caddr_t, int, phandle_t);

/*
 * Debug printf toggle
 */
int	RAMfile_debug = 0;

/*
 * For situations where we must return an error indication,
 * the RAMfile_doserr contains an appropriate DOS error value.
 */
ushort	RAMfile_doserr;

/*
 * For situations where we must return both an error indication
 * and a number of bytes, the number of bytes are stored in the
 * RAMfile_bytecount global.
 */
int	RAMfile_bytecount;

rblk_t
*RAMblk_alloc(void)
{
	rblk_t *rb;

	if (RAMfile_debug)
		printf("[]alloc. ");

	if ((rb = (rblk_t *)bkmem_alloc(sizeof (rblk_t))) == (rblk_t *)NULL)
		goto error;
	if ((rb->datap = (char *)
	    bkmem_alloc(RAMfile_BLKSIZE)) == (char *)NULL) {
		bkmem_free((caddr_t)rb, sizeof (rblk_t));
		rb = (rblk_t *)NULL;
		goto error;
	}

	/* Zero out block contents */
	bzero(rb->datap, RAMfile_BLKSIZE);
	rb->next = (rblk_t *)NULL;
	rb->prev = (rblk_t *)NULL;
	goto done;

error:
	printf("Insufficient memory for RAMblk allocation!\n");
done:
	return (rb);
}

int
RAMblklst_free(rblk_t *rl)
{
	rblk_t	*cb;
	int	rv = RAMblk_OK;

	if (RAMfile_debug)
		printf("[]lst_free ");

	if (!rl)
		return (rv);	/* that was easy */

	/*
	 *  Separate from the rest of the list if this is only a
	 *  part of the list.
	 */
	if (rl->prev) {
		rl->prev->next = (rblk_t *)NULL;
		rl->prev = (rblk_t *)NULL;
	}

	for (cb = rl; cb; cb = cb->next) {
		if (cb->datap)
			bkmem_free(cb->datap, RAMfile_BLKSIZE);
	}

	/* Find second to last node in list */
	for (cb = rl; cb && cb->next; cb = cb->next);

	/* Free the last node if there is one */
	if (cb->next)
		bkmem_free((caddr_t)cb->next, sizeof (rblk_t));

	/* Work backwards to front of list */
	while (cb->prev) {
		cb = cb->prev;
		bkmem_free((caddr_t)cb->next, sizeof (rblk_t));
	}
	/* Free remaining node */
	bkmem_free((caddr_t)cb, sizeof (rblk_t));
	return (rv);
}

void
RAMfile_addtolist(rfil_t *nf)
{
	rfil_t *sl;

	if (RAMfile_debug)
		printf("@file_addtolist ");

	for (sl = RAMfileslist; sl && sl->next; sl = sl->next);

	if (sl)
		sl->next = nf;
	else
		RAMfileslist = nf;
}

void
RAMfile_rmfromlist(rfil_t *df)
{
	rfil_t *cf, *pf;

	if (RAMfile_debug)
		printf("@file_rmfromlist ");

	pf = (rfil_t *)NULL;
	for (cf = RAMfileslist; cf; pf = cf, cf = cf->next) {
		if (cf == df) {
			if (pf)
				pf->next = cf->next;
			else
				RAMfileslist = cf->next;
			break;
		}
	}
}

rfil_t *
RAMfile_alloc(char *fn, ulong attr)
{
	rfil_t *nf;

	if (RAMfile_debug)
		printf("@file_alloc ");

	if ((nf = (rfil_t *)bkmem_alloc(sizeof (rfil_t))) == (rfil_t *)NULL)
		goto done;
	if ((nf->contents = RAMblk_alloc()) == (rblk_t *)NULL) {
		bkmem_free((caddr_t)nf, sizeof (rfil_t));
		nf = (rfil_t *)NULL;
		goto done;
	}
	if ((nf->name = (char *)bkmem_alloc(strlen(fn)+1)) == (char *)NULL) {
		(void) RAMblklst_free(nf->contents);
		nf->contents = (rblk_t *)NULL;
		bkmem_free((caddr_t)nf, sizeof (rfil_t));
		nf = (rfil_t *)NULL;
		goto done;
	}
	(void) strcpy(nf->name, fn);
	nf->next = (rfil_t *)NULL;
	nf->attrib = attr;
	nf->size = 0;
	nf->flags = 0;

	RAMfile_addtolist(nf);

done:
	return (nf);
}

int
RAMfile_free(rfil_t *df)
{
	int rv;

	if (RAMfile_debug)
		printf("@file_free ");

	RAMfile_rmfromlist(df);

	if (RAMblklst_free(df->contents) == RAMblk_ERROR)
		goto error;
	bkmem_free(df->name, strlen(df->name)+1);
	bkmem_free((caddr_t)df, sizeof (rfil_t));

	rv = RAMfile_OK;
	goto done;
error:
	printf("RAMFILE resource free failure!?\n");
	rv = RAMfile_ERROR;

done:
	return (rv);
}

rffd_t *
RAMfile_allocfd(rfil_t *nf)
{
	rffd_t *rfd;

	if (RAMfile_debug)
		printf("@file_allocfd ");

	if ((rfd = (rffd_t *)bkmem_alloc(sizeof (rffd_t))) == (rffd_t *)NULL) {
		RAMfile_doserr = DOSERR_INSUFFICIENT_MEMORY;
	} else {
		rfd->file = nf;
		rfd->fptr = nf->contents->datap;
		rfd->cblkp = nf->contents;
		rfd->cblkn = 0;
		rfd->foff = 0;
	}
	if (RAMfile_debug)
		printf("=<%x>", rfd);
	return (rfd);
}

int
RAMfile_freefd(rffd_t *fd)
{
	int rv = RAMfile_OK;

	if (RAMfile_debug)
		printf("@file_freefd:%d:", fd);

	if (!fd) {
		if (RAMfile_debug)
			printf("NOTRAM\n");
		RAMfile_doserr = DOSERR_INVALIDHANDLE;
		rv = RAMfile_ERROR;
	}
	bkmem_free((caddr_t)fd, sizeof (rffd_t));
	return (rv);
}

/*ARGSUSED1*/
rffd_t *
RAMfile_open(char *fn, ulong mode)
{
	rfil_t *sl;

	if (RAMfile_debug)
		printf("@file_open:%s:", fn);

	/* Eliminate leading slashes from the filename */
	while (*fn == '/')
		fn++;

	/* Search for file in existing files list */
	for (sl = RAMfileslist; sl; sl = sl->next) {
		if (strcmp(sl->name, fn) == 0) {
			if (RAMfile_debug)
				printf("@match, %s:", sl->name);
			break;
		}
	}

	if (!sl) {
		if (RAMfile_debug)
			printf("FNF.\n");
		RAMfile_doserr = DOSERR_FILENOTFOUND;
		return ((rffd_t *)NULL);
	}
	return (RAMfile_allocfd(sl));
}

int
RAMfile_close(rffd_t *handle)
{
	if (RAMfile_debug)
		printf("@file_close:%x:", handle);

	/* Set up to perform a delayed write, but only if not a special file */
	if (!(strcmp(handle->file->name, DOSBOOTOPC_FN) == 0) &&
	    !(strcmp(handle->file->name, DOSBOOTOPR_FN) == 0) &&
	    (handle->file->flags & RAMfp_modified)) {
		RAMfiletoprop(handle);
	}

	return (RAMfile_freefd(handle));
}

int
RAMfile_trunc_atoff(rffd_t *handle)
{
	rfil_t *rf;
	if (RAMfile_debug)
		printf("@file_trunc_atoff <%x>", handle);

	if (!handle) {
		RAMfile_doserr = DOSERR_INVALIDHANDLE;
		return (RAMfile_ERROR);
	}

	rf = handle->file;

	if (handle->foff == 0)
		RAMfile_trunc(rf, rf->attrib);
	else {
		rf->size = handle->foff;
	}

	return (RAMfile_OK);
}

void
RAMfile_trunc(rfil_t *fp, ulong attr)
{
	if (RAMfile_debug)
		printf("@file_trunc [%s]", fp->name);

	fp->attrib = attr;
	fp->size = 0;
	/* Free all but the first contents block */
	if (RAMblklst_free(fp->contents->next) == RAMblk_ERROR)
		printf("WARNING: RAMblklst_free failed during truncate.\n");
	fp->contents->next = (rblk_t *)NULL;
	/* Zero out contents */
	bzero(fp->contents->datap, RAMfile_BLKSIZE);
}

rffd_t *
RAMfile_create(char *fn, ulong attr)
{
	rfil_t *sl;

	if (RAMfile_debug)
		printf("@file_create:%s,%x:", fn, attr);

	/* Eliminate leading slashes from the filename */
	while (*fn == '/')
		fn++;

	/* Search for file in existing files list */
	for (sl = RAMfileslist; sl; sl = sl->next) {
		if (strcmp(sl->name, fn) == 0) {
			if (RAMfile_debug)
				printf("@crematch, %s:", sl->name);
			break;
		}
	}

	if (sl) {
		RAMfile_trunc(sl, attr);
		sl->flags |= RAMfp_modified;
	} else {
		if ((sl = RAMfile_alloc(fn, attr)) == (rfil_t *)NULL) {
			printf("ERROR: No memory for RAM file\n");
			RAMfile_doserr = DOSERR_INSUFFICIENT_MEMORY;
			return ((rffd_t *)NULL);
		}
	}
	return (RAMfile_allocfd(sl));
}

int
RAMfile_destroy(char *fn)
{
	rfil_t *sl;

	if (RAMfile_debug)
		printf("@file_destroy ");

	/* Eliminate leading slashes from the filename */
	while (*fn == '/')
		fn++;

	/* Search for file in existing files list */
	for (sl = RAMfileslist; sl; sl = sl->next) {
		if (strcmp(sl->name, fn) == 0) {
			if (RAMfile_debug)
				printf("@desmatch, %s:", sl->name);
			break;
		}
	}

	if (sl) {
		return (RAMfile_free(sl));
	} else {
		RAMfile_doserr = DOSERR_FILENOTFOUND;
		return (RAMfile_ERROR);
	}
}

off_t
RAMfile_lseek(rffd_t *fd, off_t offset, int whence)
{
	ulong bn, reqblk, reqoff;
	rblk_t *fb, *pfb;
	rfil_t *fp;
	off_t newoff;
	int isrdonly = 0;

	if (RAMfile_debug)
		printf("@file_lseek <%x>", fd);

	if (!fd) {
		RAMfile_doserr = DOSERR_INVALIDHANDLE;
		return (RAMfile_ERROR);
	}

	fp = fd->file;

	isrdonly = fp->attrib & DOSATTR_RDONLY;

	switch (whence) {
	case SEEK_SET:
		newoff = offset;
		break;
	case SEEK_CUR:
		newoff = fd->foff + offset;
		break;
	case SEEK_END:
		newoff = fp->size + offset;
		break;
	default:
		goto seekerr;
	}

	/* Sanity checking the new offset */
	if ((isrdonly && newoff > fp->size) || newoff < 0)
		goto seekerr;

	/* Compute new block and the offset-within-block of the new offset */
	reqblk = newoff/RAMfile_BLKSIZE;
	reqoff = newoff%RAMfile_BLKSIZE;

	pfb = (rblk_t *)NULL;
	fb = fp->contents;
	bn = reqblk;
	while (reqblk--) {
		if (fb) {
			pfb = fb;
			fb = fb->next;
		} else {
			if (!(fb = RAMblk_alloc()))
				goto memerr;
			pfb->next = fb;
			fb->prev = pfb;
		}
	}

	fd->fptr = fb->datap + reqoff;
	fd->cblkp = fb;
	fd->cblkn = bn;
	fd->foff = newoff;
done:
	return (newoff);
seekerr:
	RAMfile_doserr = DOSERR_SEEKERROR;
	return (RAMfile_ERROR);
memerr:
	RAMfile_doserr = DOSERR_INSUFFICIENT_MEMORY;
	return (RAMfile_ERROR);
}

int
RAMfile_fstat(rffd_t *handle, struct stat *stbuf)
{
	/*
	 * Not at all a complete implementation, just enough to get us
	 * by.  THE ONLY FIELD ANYBODY CALLING THIS CARES ABOUT AT PRESS
	 * TIME IS THE SIZE, AND THAT IS ALL WE FILL IN.
	 */
	rfil_t *rf;

	if (RAMfile_debug)
		printf("@file_fstat <%x>", handle);

	if (!stbuf || !handle)
		return (RAMfile_ERROR);

	rf = handle->file;
	stbuf->st_size = rf->size;
	return (RAMfile_OK);
}

int
RAMfile_read(rffd_t *handle, char *buf, int buflen)
{
	rfil_t *rf;
	int oc = 0;

	if (RAMfile_debug)
		printf("@file_read <%x>", handle);

	if (!handle) {
		RAMfile_doserr = DOSERR_INVALIDHANDLE;
		RAMfile_bytecount = oc;
		return (RAMfile_ERROR);
	} else if (buflen == 0) {
		return (RAMfile_bytecount = oc);
	}

	rf = handle->file;

	while ((handle->foff < rf->size) && (oc < buflen)) {
		if ((handle->foff/RAMfile_BLKSIZE) != handle->cblkn) {
			handle->cblkp = handle->cblkp->next;
			handle->fptr = handle->cblkp->datap;
			handle->cblkn++;
		}
		buf[oc++] = *(handle->fptr);
		handle->fptr++; handle->foff++;
	}
	return (RAMfile_bytecount = oc);
}

int
RAMfile_write(rffd_t *handle, char *buf, int buflen)
{
	rfil_t *rf;
	ulong  begoff;
	int ic = 0;

	if (RAMfile_debug)
		printf("@file_write <%x>", handle);

	if (!handle) {
		RAMfile_doserr = DOSERR_INVALIDHANDLE;
		RAMfile_bytecount = ic;
		return (RAMfile_ERROR);
	} else if (buflen == 0) {
		return (RAMfile_bytecount = ic);
	}

	rf = handle->file;

	begoff = handle->foff;

	while (ic < buflen) {
		if ((handle->foff/RAMfile_BLKSIZE) != handle->cblkn) {
			if (!handle->cblkp->next) {
				handle->cblkp->next = RAMblk_alloc();
				if (!handle->cblkp->next) {
					RAMfile_doserr =
					    DOSERR_INSUFFICIENT_MEMORY;
					if ((RAMfile_bytecount = ic) > 0)
						rf->flags |= RAMfp_modified;
					return (RAMfile_ERROR);
				}
				handle->cblkp->next->prev = handle->cblkp;
			}
			handle->cblkp = handle->cblkp->next;
			handle->fptr = handle->cblkp->datap;
			handle->cblkn++;
		}
		*(handle->fptr) = buf[ic++];
		handle->fptr++; handle->foff++;
	}

	rf->flags |= RAMfp_modified;
	rf->size = MAX((begoff + ic), rf->size);
	return (RAMfile_bytecount = ic);
}

char *
RAMfile_gets(char *buf, int maxchars, rffd_t *rf)
{
	char *rp = buf;
	int oc = maxchars - 1;

	/* Slimy error cases */
	if (!rp || maxchars <= 0)
		return ((char *)NULL);

	while (oc) {
		if (RAMfile_read(rf, buf, 1) < 1) {
			break;
		} else {
			buf++; oc--;
			if (*(buf-1) == '\n')
				break;
		}
	}
	*buf = '\0';

	if (oc == maxchars - 1)
		return ((char *)NULL);
	else
		return (rp);
}

int
RAMfile_puts(char *buf, rffd_t *wf)
{
	int oc = 0;

	while (buf && *buf) {
		if (RAMfile_write(wf, buf, 1) < 1) {
			oc = RAMfile_ERROR;
			break;
		} else {
			buf++;
			oc++;
		}
	}
	return (oc);
}

int
RAMfile_rename(rffd_t *rf, char *nn)
{
	char *an;
	int nl;

	if (!nn || !rf)
		return (RAMfile_ERROR);

	nl = strlen(nn) + 1;
	if ((an = (char *)bkmem_alloc(nl)) == (char *)NULL)
		return (RAMfile_ERROR);
	(void) strcpy(an, nn);

	bkmem_free(rf->file->name, strlen(rf->file->name)+1);
	rf->file->name = an;
	return (RAMfile_OK);
}

/*
 * RAMcvtfile
 *	Convert existing file on disk to a RAMfile.  The disk file can then
 *	be re-written later using the eeprom delayed write mechanism.
 */
/*ARGSUSED1*/
rffd_t *
RAMcvtfile(char *fn, ulong mode)
{
	rffd_t *nf = (rffd_t *)NULL;
	char *fbuf, *afn;
	int rbytes, wbytes;
	int ffd;

	afn = dos_formpath(fn);
	if (!afn) {
		RAMfile_doserr = DOSERR_FILENOTFOUND;
		return (nf);
	}

	if ((ffd = open(afn, O_RDONLY)) >= 0) {

		if ((nf = RAMfile_create(fn, 0)) == (rffd_t *)NULL)
			goto bye;

		if ((fbuf = (char *)bkmem_alloc(RAMfile_BLKSIZE))) {
			while ((rbytes =
			    read(ffd, fbuf, RAMfile_BLKSIZE)) > 0) {
				wbytes = RAMfile_write(nf, fbuf, rbytes);
				if (wbytes != rbytes) {
					printf("WARNING: Write failure ");
					printf("during conversion to ");
					printf("RAM file!\n");
					break;
				}
			}
			nf->file->flags &= ~RAMfp_modified;
			RAMrewind(nf);
			bkmem_free(fbuf, RAMfile_BLKSIZE);
		} else {
			(void) RAMfile_freefd(nf);
			(void) RAMfile_destroy(fn);
			RAMfile_doserr = DOSERR_INSUFFICIENT_MEMORY;
			nf = (rffd_t *)NULL;
		}

bye:		(void) close(ffd);

	} else {	/* open disk file */
		RAMfile_doserr = DOSERR_FILENOTFOUND;
	}

	return (nf);
}

/*
 * RAMfile_striproot
 *	RAMfile names leave off the 'boottree' part of the name to save
 *	a little space.  When dirent operations are happening we frequently
 *	have full path names to look up.  This routine provides a convenient
 *	interface for stripping off the 'boottree' part of a filename.
 *	(Presumably, because a lookup of it is desired using RAMfile_open, etc.)
 */
char *
RAMfile_striproot(char *name)
{
	char *prop;
	int plen, clen;

	if ((plen = bgetproplen(bop, "boottree", 0)) > 0) {
		if ((prop = bkmem_alloc(plen)) == NULL) {
			return (name);
		}

		/*
		 * Grab the property for comparison with the name passed
		 * in.  Leave out any ending NULL character in the comparison.
		 */
		if (bgetprop(bop, "boottree", prop, plen, 0) == plen) {
			clen = prop[plen-1] ? plen : plen - 1;
			if (strncmp(name, prop, clen) == 0) {
				bkmem_free(prop, plen);
				return (name+clen);
			}
		}

		bkmem_free(prop, plen);
		return (name);
	} else {
		return (name);
	}
}

/*
 * RAMfile_patch_dirents
 *	Based on the list of current RAMfiles, add any dirents necessary to
 *	the dirent info stored in the ffinfo structure we've been handed.
 */
#define	NAMEBUFSIZE 33

void
RAMfile_patch_dirents(ffinfo *ffip)
{
	/*
	 *  The find file structure we've been passed has the current
	 *  path we're looking up dirents for.  We need to test to see
	 *  if anything in that path exists as a RAMfile.
	 *
	 *  The first step is determine if the required directory "exists"
	 *  in the RAMfiles we have.
	 */
	rfil_t *sl;
	char *rdn;

	/*
	 * All RAMfiles are chroot'ed and the root path isn't stored as
	 * part of the filename.
	 */
	rdn = RAMfile_striproot(ffip->curmatchpath);

	/*
	 * Eliminate leading slashes as well.
	 */
	while (*rdn == '/')
		rdn++;

	/*
	 * We must match the entire remainder of the path with a file in
	 * the RAMfile list.
	 */
	for (sl = RAMfileslist; sl; sl = sl->next) {
		if (!*rdn || strncmp(rdn, sl->name, strlen(rdn)) == 0) {
			static char pnebuf[NAMEBUFSIZE];
			struct dirent *dep;
			char *pne;
			int  pnelen = 0, dec;

			pne = sl->name + strlen(rdn);

			/*
			 * We match up to the end of the requested directory
			 * path.  Double check that our RAMfile path name is
			 * an EXACT match.  I.E., avoid the case where we
			 * think the RAMfile solaris/drivers/fish.bef is a
			 * match for a requested path solaris/driv.
			 */
			if (*pne != '/')
				continue;

			/*
			 * We have a match on the directory part.
			 * The next chunk of the RAMfile name, either up to
			 * the next / or the end of the name, is potentially
			 * a new dirent.
			 */
			while (*pne == '/')
				pne++;
			while (*pne != '/' && *pne && pnelen < NAMEBUFSIZE-1)
				pnebuf[pnelen++] = *pne++;
			pnebuf[pnelen] = '\0';

			dep = (struct dirent *)ffip->dentbuf;
			for (dec = 0; dec < ffip->maxdent; dec++) {
				/* See if it's already in the entries */
				if (strcmp(dep->d_name, pnebuf) == 0)
					break;
				dep = (struct dirent *)
				    ((char *)dep + dep->d_reclen);
			}

			if (dec == ffip->maxdent) {
				/*
				 * No matches found. We should add this to
				 * the dirents.
				 */
				dep->d_reclen =
					roundup(sizeof (struct dirent) + pnelen,
					    sizeof (long));
				if ((char *)dep + dep->d_reclen <
				    ffip->dentbuf + DOSDENTBUFSIZ) {
					/* Room for this entry */
					ffip->maxdent++;
					(void) strcpy(dep->d_name, pnebuf);
				}
			}
		}
	}
}

/*
 * RAMfiletoprop
 *	Convert a RAMfile to a delayed write property.
 */
void
RAMfiletoprop(rffd_t *handle)
{
	struct dnode *save;
	long	fullsize;
	void	*fillp;
	char	namebuf[16];
	char	*fbuf;
	int	xfrd;

	save = active_node;

	/* alloc fullsize buffer */
	fullsize = sizeof (long) + strlen(handle->file->name) + 1;
	fullsize += sizeof (long) + handle->file->size;
	if ((fbuf = (char *)bkmem_alloc((u_int)fullsize)) == (char *)NULL) {
		printf("ERROR:	No memory for delayed-write buffer!\n");
		printf("\tDelayed write of %s will not occur.\n",
		    handle->file->name);
		goto xit;
	}

	/* first bytes of buf: (long)(strlen(fname)+1) */
	fillp = fbuf;
	*((long *)fillp) = strlen(handle->file->name) + 1;
	fillp = ((long *)fillp) + 1;

	/* next bytes are strcpy(fname) */
	(void) strcpy((char *)fillp, handle->file->name);
	fillp = ((char *)fillp) + strlen(handle->file->name) + 1;

	/* next bytes are (long)(Ramfile_size) */
	*((long *)fillp) = handle->file->size;
	fillp = ((long *)fillp) + 1;

	/* next bytes are file contents */
	RAMrewind(handle);
	xfrd = RAMfile_read(handle, (char *)fillp, handle->file->size);
	if (xfrd < 0) {
		printf("ERROR: RAMfile conversion to property failed.  ");
		printf("RAMfile read failed.  ");
		printf("\tDelayed write of %s will not occur.\n",
		    handle->file->name);
		goto clean;
	} else if (xfrd != handle->file->size) {
		printf("WARNING: Error in RAMfile conversion to property.  ");
		printf("Possibly short read.\n");
	}

	/* form new property name 'write'num_delayedwrites */
	(void) sprintf(namebuf, "write%d", num_delayed);

	/* binary setprop new name, value in buf */
	if (bsetprop(bop, namebuf, fbuf, (int)fullsize, &delayed_node)) {
		printf("ERROR: RAMfile conversion to property failed.  ");
		printf("Setprop failed.  ");
		printf("\tDelayed write of %s will not occur.\n",
		    handle->file->name);
		goto clean;
	}

	num_delayed++;
	handle->file->flags &= ~RAMfp_modified;

clean: /* free buffer */
	bkmem_free(fbuf, (u_int)fullsize);

xit:	/* reset active node */
	active_node = save;
}
