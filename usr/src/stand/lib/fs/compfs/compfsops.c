/*
 * Copyright (c) 1993-1996, Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident "@(#)compfsops.c 1.19       96/09/19 SMI"

/*
 *	Composite filesystem for secondary boot
 *	that uses mini-MS-DOS filesystem
 */

#include <sys/param.h>
#include <sys/vnode.h>
#include <sys/sysmacros.h>
#include <sys/promif.h>
#include <sys/stat.h>

#include <sys/fs/pc_fs.h>
#include <sys/fs/pc_label.h>
#include <sys/fs/pc_dir.h>
#include <sys/fs/pc_node.h>

#include <sys/bootcmn.h>
#include <sys/bootvfs.h>
#include <sys/bootdebug.h>
#include <sys/salib.h>

#define	MAPFILE	"\\SOLARIS.MAP"
#define	COMPFS_AUGMENT	0x1000	/* set by c flag in SOLARIS.MAP */
#define	COMPFS_TEXT	0x2000	/* set by t flag in SOLARIS.MAP */
#define	COMPFS_PATH	0x4000	/* set by p flag in SOLARIS.MAP */
#define	COMPFS_DECOMP	0x8000	/* set by z flag in SOLARIS.MAP */

#define	whitespace(C)	((unsigned char)(C) <= ' ' ? 1 : 0)

struct map_entry {
	char	*target;
	char	*source;
	int	flags;
	struct map_entry *link;
};

static char mapfile[] = MAPFILE;
static struct map_entry *map_listh = 0;
static struct map_entry *map_listt = 0;

static int	cpfs_mapped(char *, char **, int *);
static void	cpfs_build_map(void);

/* #define	COMPFS_OPS_DEBUG */

#ifdef COMPFS_OPS_DEBUG
static int	cpfsdebuglevel = 0;
static int	compfsverbose = 0;
#endif

/*
 * exported functional prototypes
 */
static int	boot_compfs_mountroot(char *str);
static int	boot_compfs_unmountroot(void);
static int	boot_compfs_open(char *filename, int flags);
static int	boot_compfs_close(int fd);
static int	boot_compfs_read(int fd, caddr_t buf, int size);
static off_t	boot_compfs_lseek(int, off_t, int);
static int	boot_compfs_fstat(int fd, struct stat *stp);
static void	boot_compfs_closeall(int flag);
static int	boot_compfs_getdents(int, struct dirent *, unsigned);

off_t	boot_compfs_getpos(int fd);

struct boot_fs_ops boot_compfs_ops = {
	"compfs",
	boot_compfs_mountroot,
	boot_compfs_unmountroot,
	boot_compfs_open,
	boot_compfs_close,
	boot_compfs_read,
	boot_compfs_lseek,
	boot_compfs_fstat,
	boot_compfs_closeall,
	boot_compfs_getdents
};

extern struct boot_fs_ops *get_fs_ops_pointer();
extern struct boot_fs_ops *extendfs_ops;
extern struct boot_fs_ops *origfs_ops;
extern int validvolname();

static struct compfsfile *compfshead = NULL;
static int	compfs_filedes = 1;	/* 0 is special */

typedef struct compfsfile {
	struct compfsfile *forw;	/* singly linked */
	int	fd;		/* the fd given out to caller */
	int	ofd;		/* original filesystem fd */
	int	efd;		/* extended filesystem fd */
	off_t	offset;		/* for lseek maintenance */
	off_t	decomp_offset;	/* for decomp lseek maintenance */
	int	o_size;		/* in original filesystem */
	int	e_size;		/* in extended filesystem */
	int	flags;
	char	compressed;	/* is file compressed */
} compfsfile_t;

extern int	decompress_init();
extern void	decompress_fini();
extern void	decompress();
static int	decomp_init();
static void	decomp_fini();
static void	decomp_file(compfsfile_t *);

static int	decomp_open(char *, int, compfsfile_t *);
static int	decomp_lseek(compfsfile_t *, off_t, int);
static int	decomp_fstat(compfsfile_t *, struct stat *);
static int	decomp_read(compfsfile_t *, caddr_t, int);
static int	decomp_close(compfsfile_t *);
static void	decomp_closeall(int);
static void	decomp_free_data();

/*
 * Given an fd, do a search (in this case, linear linked list search)
 * and return the matching compfsfile pointer or NULL;
 * By design, when fd is -1, we return a free compfsfile structure (if any).
 */

static struct compfsfile *
get_fileptr(int fd)
{	struct compfsfile *fileptr = compfshead;

	while (fileptr != NULL) {
		if (fd == fileptr->fd)
			break;
		fileptr = fileptr->forw;
	}
#ifdef COMPFS_OPS_DEBUG
	if (cpfsdebuglevel > 6)
		printf("compfs: returning fileptr 0x%x\n", fileptr);
#endif
	return (fileptr);
}

static void
cpfs_build_map(void)
{
	int	mapfid;
	struct map_entry *mlp;
	char	*bp;
	char	*bufend;
	char	*cp;
	char	buffer[PC_SECSIZE];
	char	dospath[MAXNAMELEN];
	char	fspath[MAXNAMELEN];
	int	rcount;
	int	dosplen, fsplen;
	int	flags = 0;
#ifdef DECOMP_DEBUG
	int	decomp_files = 0;
#endif


	if ((mapfid = (*extendfs_ops->fsw_open)(mapfile, flags)) < 0) {
#ifdef COMPFS_OPS_DEBUG
		if (cpfsdebuglevel > 2)
			printf("compfs: open %s file failed\n", mapfile);
#endif
		return;
	}

	if (!(rcount = (*extendfs_ops->fsw_read)(mapfid, buffer, PC_SECSIZE))) {
		goto mapend;
	}
	bp = buffer;
	bufend = buffer + rcount;

	do {	/* for each line in map file */

		*fspath = '\0';
		fsplen = 0;
		*dospath = '\0';
		dosplen = 0;

		while (whitespace(*bp)) {
			if (++bp >= bufend) {
				rcount = (*extendfs_ops->fsw_read)(mapfid,
				    buffer, PC_SECSIZE);
				if (rcount == 0)
					goto mapend;
				bp = buffer;
				bufend = buffer + rcount;
			}
		}
		if (*bp == '#')
			/* skip over comment lines */
			goto mapskip;

		cp = fspath;
#ifdef	notdef
		if (*bp != '/') {
			/*
			 * fs pathname does not begin with '/'
			 * so prepend with current path. More Work??
			 */
		}
#endif
		while (!whitespace(*bp) && fsplen < MAXNAMELEN) {
			*cp++ = *bp++;
			if (bp >= bufend) {
				rcount = (*extendfs_ops->fsw_read)(mapfid,
				    buffer, PC_SECSIZE);
				if (rcount == 0)
					goto mapend;
				bp = buffer;
				bufend = buffer + rcount;
			}
			fsplen++;
		}
		*cp = '\0';

		while (whitespace(*bp)) {
			if (*bp == '\n')
				goto mapskip;
			if (++bp >= bufend) {
				rcount = (*extendfs_ops->fsw_read)(mapfid,
				    buffer, PC_SECSIZE);
				if (rcount == 0)
					goto mapend;
				bp = buffer;
				bufend = buffer + rcount;
			}
		}

		cp = dospath;
		if (*bp != '/' && *bp != '\\' && *bp != '[' && *bp != ':') {
			/*
			 * DOS pathname does not begin with '\'
			 * so prepend with root
			 */
			*cp++ = '\\';
			dosplen = 1;
		} else if (*bp == '[' || *bp == ':') {
			/*
			 * A volume is specified, prepend it to the name
			 * with a following colon.
			 */
			bp++;
			while (*bp != ']' && *bp != ':' &&
			    dosplen < MAXNAMELEN) {
				/* Copy the volume name into the path */
				*cp++ = *bp++;
				if (bp >= bufend) {
					if (!(rcount =
					    (*extendfs_ops->fsw_read)
					    (mapfid, buffer, PC_SECSIZE)))
						goto mapend;
					bp = buffer;
					bufend = buffer + rcount;
				}
				dosplen++;
			}
			bp++; *cp++ = ':'; dosplen++;
		}

		while (!whitespace(*bp) && dosplen < MAXNAMELEN) {
			*cp++ = *bp++;
			if (bp >= bufend) {
				rcount = (*extendfs_ops->fsw_read)(mapfid,
				    buffer, PC_SECSIZE);
				if (rcount == 0)
					goto mapend;
				bp = buffer;
				bufend = buffer + rcount;
			}
			dosplen++;
		}
		*cp = '\0';

		mlp = (struct map_entry *)bkmem_alloc(
		    sizeof (struct map_entry) + fsplen + dosplen + 2);

		cp = (char *)(mlp + 1);
		bcopy(fspath, cp, fsplen + 1);
		mlp->target = cp;

		cp = (char *)(cp + fsplen + 1);
		bcopy(dospath, cp, dosplen + 1);
		mlp->source = cp;

		mlp->flags = 0;
		while (*bp != '\n') {
			if (*bp == 'c')
				mlp->flags |= COMPFS_AUGMENT;
			else if (*bp == 't')
				mlp->flags |= COMPFS_TEXT;
			else if (*bp == 'p')
				mlp->flags |= COMPFS_PATH;
			else if (*bp == 'z')
				mlp->flags |= COMPFS_DECOMP;

			if (++bp >= bufend) {
				rcount = (*extendfs_ops->fsw_read)(mapfid,
				    buffer, PC_SECSIZE);
				if (rcount == 0)
					goto mapend;
				bp = buffer;
				bufend = buffer + rcount;
			}
		}
		/*
		 * insert new entry into linked list
		 */
		mlp->link = NULL;
		if (!map_listh)
			map_listh = mlp;
		if (map_listt)
			map_listt->link = mlp;
		map_listt = mlp;

#ifdef COMPFS_OPS_DEBUG
		if (compfsverbose || cpfsdebuglevel > 2) {
			if (mlp->flags & COMPFS_AUGMENT)
				printf("compfs: %s augmented with %s\n",
				    fspath, dospath);
			else {
				printf("compfs: %s mapped to %s\n",
				    fspath, dospath);
			}
#ifdef DECOMP_DEBUG
			if (mlp->flags & COMPFS_DECOMP) {
				decomp_files++;
				printf("compfs: %s is compressed\n", dospath);
			}
#endif
		}
#endif

mapskip:
		while (*bp != '\n')
			if (++bp >= bufend) {
				rcount = (*extendfs_ops->fsw_read)(mapfid,
				    buffer, PC_SECSIZE);
				if (rcount == 0)
					goto mapend;
				bp = buffer;
				bufend = buffer + rcount;
			}
	/*CONSTANTCONDITION*/
	} while (1);
mapend:
	(*extendfs_ops->fsw_close)(mapfid);

#ifdef DECOMP_DEBUG
	if (compfsverbose || cpfsdebuglevel > 2) {
		printf("%d of the DOS files are compressed\n", decomp_files);
	}
#endif

#ifdef COMPFS_OPS_DEBUG
	if (compfsverbose || cpfsdebuglevel > 2) {
		printf("\n");
		if (cpfsdebuglevel & 1)
			(void) goany();
	}
#endif
}


static int
cpfs_mapped(char *str, char **dos_str, int *flagp)
{
	static char *strremap[2];
	struct map_entry *mlp;
	char *restp;
	int mapidx;
	int remapped = 0;
	int pathflags;
	int spcleft;

	if (!strremap[0] && !(strremap[0] = (char *)bkmem_alloc(MAXNAMELEN))) {
		/* Something's really awry */
		prom_panic("No memory to build a remapped path");
	}

	if (!strremap[1] && !(strremap[1] = (char *)bkmem_alloc(MAXNAMELEN))) {
		/* Something's really awry */
		prom_panic("No memory to build a remapped path");
	}

	strremap[0][0] = '\0';
	strremap[1][0] = '\0';
	mapidx = 1;

	/*
	 *  First apply any directory re-mappings
	 *  Note that the effects of re-mappings can be cumulative.
	 */
	for (mlp = map_listh; mlp; mlp = mlp->link) {
		if ((mlp->flags & COMPFS_PATH) &&
		    (strncmp(mlp->target, str, strlen(mlp->target))) == 0) {
#ifdef COMPFS_OPS_DEBUG
			if (cpfsdebuglevel > 4) {
				printf("(%s) matches (%s)\n", str, mlp->target);
				printf("use (%s) instead\n", mlp->source);
			}
#endif
			mapidx = (mapidx + 1)%2;
			strncpy(strremap[mapidx], mlp->source, MAXNAMELEN - 1);
			strremap[mapidx][MAXNAMELEN-1] = '\0';

			restp = &(str[strlen(mlp->target)]);
			spcleft = MAXNAMELEN - strlen(strremap[mapidx]);
			strncat(strremap[mapidx], restp, spcleft);
			strremap[mapidx][MAXNAMELEN-1] = '\0';
			pathflags = mlp->flags;
			remapped = 1;
#ifdef COMPFS_OPS_DEBUG
			if (cpfsdebuglevel > 4) {
				printf("(%s)->(%s)\n", str, strremap[mapidx]);
			}
#endif
			str = strremap[mapidx];
		}
	}

	/*
	 * Be prepared to handle an augmentation or replacement of this
	 * pathname; but also be prepared to send back the new path as
	 * is if no further mappings apply.
	 */
	if (remapped) {
		*dos_str = str;
		*flagp = pathflags;
	}

	for (mlp = map_listh; mlp; mlp = mlp->link) {
		extern int strcasecmp(char *, char *);

		if (strcasecmp(mlp->target, str) == 0) {
#ifdef COMPFS_OPS_DEBUG
			if (cpfsdebuglevel > 4) {
				printf("(%s)->(%s)\n", str, mlp->source);
			}
#endif
			*dos_str = mlp->source;
			*flagp = mlp->flags;
			return (1);
		}
	}

	/*
	 *  Finally, check for an implicit remapping, when a volume specifier
	 *  has been attached to the path!
	 */
	if (!remapped) {
		char *eov = strchr(str, ':');

		if (eov && validvolname(str) && ((eov - str) <= VOLLABELSIZE)) {
			remapped = 1;
			*dos_str = str;
			*flagp = COMPFS_TEXT;
		}
	}

	return (remapped);
}

/*
 * []-----------------------------------------------------------[]
 * | return true (non-zero) value if the default file system	 |
 * | is compfs.							 |
 * []-----------------------------------------------------------[]
 */
boot_compfs_writecheck(char *str, char **dos_str)
{
	extern struct boot_fs_ops *get_default_fs();
	extern struct boot_fs_ops boot_pcfs_ops;
	int ismapped, iscompfs;
	int mflags;

	/*
	 * XXX - What we aren't accounting for here is the possibility
	 * that the current pcfs volume is read-only!!
	 * This may require a bit of redesigning.  I'm not sure
	 * if you can even tell if it is write protected until you attempt
	 * the write and then get a failure result.  If that is the
	 * case we're going to have to take care of it at the write
	 * level, with some sort of "fall back to RAM file" mechanism. Of
	 * course the fall back will have to be transparent to the DOS
	 * module performing the write (it should just appear to them that
	 * their write succeeded.
	 */
	ismapped = cpfs_mapped(str, dos_str, &mflags);
	iscompfs = (get_default_fs() == &boot_compfs_ops);

	if (ismapped) {
		return (iscompfs && extendfs_ops == &boot_pcfs_ops);
	} else {
		/* If there's no mapping just pass back a ptr to str */
		*dos_str = str;
		return (iscompfs && origfs_ops == &boot_pcfs_ops);
	}
}

static int
boot_compfs_open(char *str, int flags)
{
	struct compfsfile *fileptr;
	char	*dos_str;
	int	mflags;
	int	retcode;
	int	dont_bother = 0;
	struct stat	statbuf;
	int	new = 0;

#ifdef COMPFS_OPS_DEBUG
	if (cpfsdebuglevel > 4) {
		printf("compfs_open(): open %s flag=0x%x\n",
			str, flags);
		if (cpfsdebuglevel & 1)
			(void) goany();
	}
#endif

	if ((fileptr = get_fileptr(0)) == NULL) {
		fileptr = (compfsfile_t *)
			bkmem_alloc(sizeof (compfsfile_t));
		new++;
	}
	fileptr->flags = 0;
	fileptr->offset = 0;
	fileptr->o_size = 0;
	fileptr->e_size = 0;
	fileptr->compressed = 0;

	if (cpfs_mapped(str, &dos_str, &mflags)) {
		retcode = decomp_open(dos_str, flags, fileptr);
		if (retcode >= 0) {
			union {
				char s[3];
				u_char u[3];
			} h;

			/*
			 * Look for compressed file signature bytes.
			 */

			if (((*extendfs_ops->fsw_read)(retcode, h.s, 3) == 3) &&
			    (h.u[0] == 0x1f) && (h.u[1] == 0x9d) &&
			    (h.u[2] >= 0x89) && (h.u[2] <= 0x90)) {
					fileptr->compressed = 1;
			}
			(*extendfs_ops->fsw_lseek)(retcode, 0, SEEK_SET);

			if (!(mflags & COMPFS_AUGMENT)) {
				/* complete replacement, don't bother */
				dont_bother = 1;
				fileptr->ofd = -1;
			}
			fileptr->flags = mflags;
		}
		fileptr->efd = retcode;
	} else {
		dos_str = "";
		fileptr->efd = -1;
	}

	if (!dont_bother)
		fileptr->ofd = (*origfs_ops->fsw_open)(str, flags);

	if (fileptr->ofd < 0 && fileptr->efd < 0) {
		if (new)
			bkmem_free((caddr_t)fileptr, sizeof (compfsfile_t));
		return (-1);
	} else {
		fileptr->fd = compfs_filedes++;
		/*
		 * XXX - Major kludge alert!
		 *
		 * old versions of adb use bit 0x80 in file descriptors
		 * for internal bookkeeping. This works fine under unix
		 * where descriptor numbers are re-used. This breaks
		 * on standalones (kadb) as soon as this ever increasing
		 * descriptor number hits 0x80 (128). We skip all
		 * ranges of numbers with this bit set so kadb does
		 * not get confused. Hey, kadb started it.
		 */
		if (compfs_filedes & 0x80)
			compfs_filedes += 0x80;
	}

	/*
	 * establish size information for seek and read
	 */

	if (fileptr->ofd >= 0) {
		(*origfs_ops->fsw_fstat)(fileptr->ofd, &statbuf);
		fileptr->o_size = statbuf.st_size;
	}
	if (fileptr->efd >= 0) {
		/*
		 * Defer the fstat since it could trigger decompression
		 * and pcfs is writable, so we have to recheck size at
		 * the time of fstat or read.
		 */
		fileptr->e_size = -1;
	}
	if (new) {
		fileptr->forw = compfshead;
		compfshead = fileptr;
	}

#ifdef COMPFS_OPS_DEBUG
	if (cpfsdebuglevel > 2) {
		printf("compfs_open(): compfs fd = %d, origfs file \"%s\" fd"
			" %d, dos file \"%s\" fd %d\n",
			fileptr->fd, str, fileptr->ofd, dos_str, fileptr->efd);
		printf("origfs file size %d, dos file size %d\n",
			fileptr->o_size, fileptr->e_size);
		if (cpfsdebuglevel & 1)
			(void) goany();
	}
#endif

	return (fileptr->fd);
}

/*
 * compfs_lseek()
 *
 * We maintain an offset at this level for composite file system.
 * This requires us keeping track the file offsets here and
 * in read() operations in consistent with the normal semantics.
 */

static off_t
boot_compfs_lseek(int fd, off_t addr, int whence)
{
	struct compfsfile *fileptr;
	off_t	newoff;

#ifdef COMPFS_OPS_DEBUG
	if (cpfsdebuglevel > 4) {
		printf("compfs_lseek(): fd %d addr=%d, whence=%d\n",
			fd, addr, whence);
		if (cpfsdebuglevel & 1)
			(void) goany();
	}
#endif

	fileptr = get_fileptr(fd);
	if (fileptr == NULL)
		return (-1);

	switch (whence) {
	case SEEK_CUR:
		newoff = fileptr->offset + addr;
		break;
	case SEEK_SET:
		newoff = addr;
		break;
	default:
	case SEEK_END:
#ifdef COMPFS_OPS_DEBUG
		printf("compfs_lseek(): invalid whence value %d\n", whence);
#endif
		return (-1);
	}

	/*
	 * A seek beyond origfs EOF implies reading the auxiliary
	 * (DOS) file of the composite filesystem.
	 * This is okay since this is a read-only filesystem.
	 * Actual "file offset seek" is done when read() is involved.
	 */

	/*
	 * Let's do the lseek motion on the low level file system.
	 */

	if (fileptr->ofd >= 0) {
		if ((newoff >= fileptr->o_size) && (fileptr->efd >= 0)) {
			if ((*extendfs_ops->fsw_lseek)(fileptr->efd,
			    newoff-fileptr->o_size, SEEK_SET) < 0)
				return (-1);
		} else {
			if ((*origfs_ops->fsw_lseek)(fileptr->ofd,
			    newoff, SEEK_SET) < 0)
				return (-1);
		}
	} else if (fileptr->efd >= 0) {
		if (decomp_lseek(fileptr, newoff, SEEK_SET) < 0)
			return (-1);
	}

#ifdef COMPFS_OPS_DEBUG
	if (cpfsdebuglevel > 4) {
		printf("compfs_lseek(): new offset %d\n", fileptr->offset);
		if (cpfsdebuglevel & 1)
			(void) goany();
	}
#endif

	fileptr->offset = newoff;
	return (newoff);
}

/*
 * compfs_fstat() only supports size and mode at present time.
 */

static int
boot_compfs_fstat(int fd, struct stat *stp)
{
	struct compfsfile *fileptr;
	struct stat sbuf;

#ifdef COMPFS_OPS_DEBUG
	if (cpfsdebuglevel > 4)
		printf("compfs_fstat(): fd =%d\n", fd);
#endif

	fileptr = get_fileptr(fd);
	if (fileptr == NULL) {
#ifdef COMPFS_OPS_DEBUG
		printf("compfs_fstat(): no such fd %d\n", fd);
		(void) goany();
#endif
		return (-1);
	}

	if (fileptr->ofd >= 0) {
		if ((*origfs_ops->fsw_fstat)(fileptr->ofd, stp) < 0)
			return (-1);
	} else {
		stp->st_mode = 0;
		stp->st_size = 0;
	}

	if (fileptr->efd >= 0) {
		if (decomp_fstat(fileptr, &sbuf) < 0)
			return (-1);
		stp->st_size += sbuf.st_size;
		stp->st_mode |= sbuf.st_mode;
	}

	return (0);
}

static int
boot_compfs_getdents(int fd, struct dirent *dep, unsigned size)
{
	struct compfsfile *fileptr;

	fileptr = get_fileptr(fd);
	if (fileptr == NULL) {
#ifdef COMPFS_OPS_DEBUG
		printf("compfs_getdents(): no such fd %d\n", fd);
#endif
		return (-1);
	}

	if (fileptr->efd >= 0) {
		return ((*extendfs_ops->fsw_getdents)(fileptr->efd, dep, size));
	} else if (fileptr->ofd >= 0) {
		return ((*origfs_ops->fsw_getdents)(fileptr->ofd, dep, size));
	}

	return (-1);
}

off_t
boot_compfs_getpos(int fd)
{
	struct compfsfile *fileptr;

	fileptr = get_fileptr(fd);
	if (fileptr == NULL)
		return (-1);

	return (fileptr->offset);
}

/*
 * Special dos-text adjustment processing:
 *  converting "\r\n" to " \n" presumably for ASCII files.
 */
static void
dos_text(char *p, int count)
{
	int i, j;
	for (i = count - 1, j = (int)p; i > 0; i--, j++)
		if (*(char *)j == '\r' && *(char *)(j + 1) == '\n')
			*(char *)j = ' ';
}

/*
 * compfs_read()
 */
static int
boot_compfs_read(int fd, caddr_t buf, int count)
{
	struct compfsfile *fileptr;
	int	pcretcode;
	int	retcode = -1;

	fileptr = get_fileptr(fd);
	if (fileptr == NULL) {
#ifdef COMPFS_OPS_DEBUG
		printf("compfs_read(): no such fd %d\n", fd);
		(void) goany();
#endif
		return (-1);
	}

	/*
	 * we seek to the right place before reading
	 */
#ifdef COMPFS_OPS_DEBUG
	if (cpfsdebuglevel > 4) {
		printf("compfs_read(): offset at %d (osz=%d, esz=%d)\n",
			fileptr->offset, fileptr->o_size, fileptr->e_size);
		if (cpfsdebuglevel & 1)
			(void) goany();
	}
#endif
	if (fileptr->ofd >= 0) {
		if (fileptr->offset < fileptr->o_size) {
			retcode = (*origfs_ops->fsw_read)(fileptr->ofd,
				buf, count);
#ifdef COMPFS_OPS_DEBUG
			if (cpfsdebuglevel > 4) {
				printf("compfs_read(): origfs read"
					" returned %d\n", retcode);
			}
#endif
			if (retcode > 0)
				fileptr->offset += retcode;
			if (retcode == count || fileptr->efd < 0 || retcode < 0)
				return (retcode);

			pcretcode = decomp_read(fileptr,
				buf + retcode, count - retcode);
#ifdef COMPFS_OPS_DEBUG
			if (cpfsdebuglevel > 2) {
				printf("compfs_read(): followup dos read"
					" returned %d\n", pcretcode);
				if (cpfsdebuglevel & 1)
					(void) goany();
			}
#endif
			if (pcretcode < 0)
				return (pcretcode);
			if (fileptr->flags & COMPFS_TEXT)
				(void) dos_text(buf+retcode, pcretcode);
			fileptr->offset += pcretcode;
			return (retcode + pcretcode);
		} else {
			if (fileptr->efd < 0)
				return (0);	/* easy */
			pcretcode = decomp_read(fileptr, buf,
				count);
			if (pcretcode > 0) {
				if (fileptr->flags & COMPFS_TEXT)
					(void) dos_text(buf, pcretcode);
				fileptr->offset += pcretcode;
			}
			return (pcretcode);
		}
	} else if (fileptr->efd >= 0) {
		retcode = decomp_read(fileptr, buf, count);
		if (retcode > 0) {
			fileptr->offset += retcode;
			if (fileptr->flags & COMPFS_TEXT)
				(void) dos_text(buf, retcode);
		}

#ifdef COMPFS_OPS_DEBUG
		if (cpfsdebuglevel > 4) {
			printf("compfs_read(): solo dos read returned %d\n",
				retcode);
			if (cpfsdebuglevel & 1)
				(void) goany();
		}
#endif
	}

#ifdef COMPFS_OPS_DEBUG
	if (cpfsdebuglevel > 4) {
		printf("compfs_read(): return code %d\n", retcode);
		if (cpfsdebuglevel & 1)
			(void) goany();
	}
#endif
	return (retcode);
}

static int
boot_compfs_close(int fd)
{
	struct compfsfile *fileptr;
	int ret1 = 0;
	int ret2 = 0;

	fileptr = get_fileptr(fd);
	if (fileptr == NULL) {
#ifdef COMPFS_OPS_DEBUG
		printf("compfs_close(): no such fd %d.\n", fd);
		(void) goany();
#endif
		return (-1);
	}

#ifdef COMPFS_OPS_DEBUG
	if (cpfsdebuglevel > 4) {
		printf("compfs_close(): fd=%d ofd=%d efd=%d\n",
			fd, fileptr->ofd, fileptr->efd);
		if (cpfsdebuglevel & 1)
			(void) goany();
	}
#endif

	if (fileptr->efd >= 0)
		ret1 = decomp_close(fileptr);
	if (fileptr->ofd >= 0)
		ret2 = (*origfs_ops->fsw_close)(fileptr->ofd);
	fileptr->fd = 0; /* don't bother to free, make it re-usable */

	if (ret1 < 0 || ret2 < 0)
		return (-1);
	return (0);
}


/*
 * compfs_mountroot() returns 0 on success and -1 on failure.
 */
#ifdef i386
static int
boot_compfs_mountroot(char *str)
{
	struct boot_fs_ops *tmp_ops;
	char *dev = str;
	int rc = 0, pfd, sv;
	static int x = 0;

	extern int SilentDiskFailures;
	extern int Oldstyleboot;
	extern int OpenCount;
	extern char *new_root_type;

	/*
	 *  An Oldstyleboot is one where we booted from ufsbootblk or
	 *  MDB.  In these cases we look for the extended fs strictly
	 *  on the floppy.  The newer, 2.6 boots, where we were loaded
	 *  from strap.com, we want to check whatever was the boot device
	 *  for the extended filesystem.
	 */
	if (!x) {
		char *new_root_save = new_root_type;
		new_root_type = extendfs_ops->fsw_name;

		if (Oldstyleboot) {
			sv = SilentDiskFailures;
			SilentDiskFailures = 1;
			rc = (*extendfs_ops->fsw_mountroot)(FLOPPY0_NAME);
			SilentDiskFailures = sv;
			/*
			 *  On an old style boot, we've either booted from
			 *  the root filesystem or know exactly what the
			 *  device is where the root lives.  We want to make
			 *  sure then that the coming prom_open
			 *  digs out this info and sets root_fs_type
			 *  accordingly.
			 */
			new_root_save = 0;
		} else {
			rc = (*extendfs_ops->fsw_mountroot)(BOOT_DEV_NAME);
		}

		if (!rc) {
			cpfs_build_map();
		}

		new_root_type = new_root_save;
	}

	if ((pfd = prom_open(x ? str : (char *)0)) > 0) {
		/*  Find the "real" file system type */

		static char dmy[sizeof (BOOT_DEV_NAME)+4];
		extern char *systype;

		tmp_ops = get_fs_ops_pointer(new_root_type);
		if (!tmp_ops)
			return (-1);	/* can happen on IO error */
		systype = tmp_ops->fsw_name;
		(void) prom_close(pfd);
		OpenCount--;

		if (x == 0) {
			/* Use dummy device name for mount */
			(void) sprintf(dmy, "%s \b", BOOT_DEV_NAME);
			dev = dmy;
		}

		/*
		 * XXX -- Ugh.  We need to close any files open on the root.
		 * We have to do that before we call mountroot, though,
		 * because if you do it after mounting root you can
		 * end up undoing all you just did in the mount.
		 * Of course if the mountroot fails, we're really hosed
		 * because we just closed everything on the old root!!
		 */
		if (x && origfs_ops != extendfs_ops)
			(*origfs_ops->fsw_closeall)(1);

		if (!(rc = (*tmp_ops->fsw_mountroot)(dev))) {
			x++;
			origfs_ops = tmp_ops;
			new_root_type = "";
			return (0);
		}
	}

	x = 1;
	return (-1);
}
#else
static int
boot_compfs_mountroot(char *str)
{
	extern char *init_disketteio();

	if ((*extendfs_ops->fsw_mountroot)(init_disketteio()) == 0)
		cpfs_build_map();

	return ((*origfs_ops->fsw_mountroot)(str));
}
#endif /* !i386 */

/*
 * Unmount the root fs -- unsupported on this fstype.
 */
int
boot_compfs_unmountroot(void)
{
	return (-1);
}

static void
boot_compfs_closeall(int flag)
{	struct map_entry *mlp;
	struct compfsfile *fileptr = compfshead;

	decomp_closeall(flag);
	(*origfs_ops->fsw_closeall)(flag);

	while (fileptr != NULL) {
		bkmem_free((caddr_t)fileptr, sizeof (struct compfsfile));
		fileptr = fileptr->forw;
	}
	compfshead = NULL;

	for (mlp = map_listh; mlp; mlp = map_listh) {
		map_listh = mlp->link;
		bkmem_free((caddr_t) mlp, sizeof (struct map_entry) +
		    strlen(mlp->target) + strlen(mlp->source) + 2);
	}
	map_listt = NULL;

#ifdef COMPFS_OPS_DEBUG
	if (cpfsdebuglevel > 2) {
		printf("compfs_closeall()\n");
		if (cpfsdebuglevel & 1)
			(void) goany();
	}
#endif
}

/*
 *	The remainder of this file is the routines for implementing the
 *	decompression layer on top of the pcfs code.
 */

#define	DLCHUNK		(32 * 1024)
struct decomp_list {
	struct decomp_list	*dl_next;
	char			dl_data[DLCHUNK];
};

static struct decomp_list *decomp_data; /* storage list for decompressed */
					/*    data */
static int decomp_filedes;	/* fd of currently decompressed file */
static int decomp_size;		/* size of currently decompressed file */

static int
decomp_open(char *filename, int flags, compfsfile_t *fileptr)
{
#ifdef DECOMP_DEBUG
	if (compfsverbose || cpfsdebuglevel > 2) {
		if (fileptr->compressed)
			printf("decomp_open: opening compressed DOS file %s\n",
				filename);
	}
#endif
	fileptr->decomp_offset = 0;
	return ((*extendfs_ops->fsw_open)(filename, flags));
}

static int
decomp_lseek(compfsfile_t *fileptr, off_t addr, int whence)
{
	if (fileptr->compressed) {
		fileptr->decomp_offset = addr;
#ifdef DECOMP_DEBUG
		if (compfsverbose || cpfsdebuglevel > 2) {
			printf("decomp_lseek: seeking to 0x%x\n", addr);
		}
#endif
		return (0);
	}
	return ((*extendfs_ops->fsw_lseek)(fileptr->efd, addr, whence));
}

static int
decomp_fstat(compfsfile_t *fileptr, struct stat *stp)
{
	/* We supply only the file size */
	if (fileptr->compressed) {
		/*
		 * Allow the extendfs_ops to fill in the stat structure
		 * first, namely the st_mode field, then we can fill in
		 * the size field.
		 */

		(*extendfs_ops->fsw_fstat)(fileptr->efd, stp);

		decomp_file(fileptr);
		stp->st_size = decomp_size;
#ifdef DECOMP_DEBUG
		if (compfsverbose || cpfsdebuglevel > 2) {
			printf("decomp_fstat: reporting size 0x%x\n",
				decomp_size);
		}
#endif
		return (0);
	}
	return ((*extendfs_ops->fsw_fstat)(fileptr->efd, stp));

}

static void
decomp_copyout(compfsfile_t *fileptr, caddr_t buf, int count)
{
	struct decomp_list *dlp;
	int rd_indx, wrt_indx, len;

	/* "seek" to the data in the decompressed data list */
	dlp = decomp_data;
	rd_indx = 0;
	while ((rd_indx + DLCHUNK) <= fileptr->decomp_offset) {
		dlp = dlp->dl_next;
		rd_indx += DLCHUNK;
	}
	rd_indx = fileptr->decomp_offset % DLCHUNK;

	/* copy count bytes to caller */
	wrt_indx = 0;
	do {
		len = ((DLCHUNK - rd_indx) > count) ?
			count : (DLCHUNK - rd_indx);
		bcopy(dlp->dl_data + rd_indx, buf + wrt_indx, len);
		dlp = dlp->dl_next;
		rd_indx = 0;
		count -= len;
		wrt_indx += len;
	} while (count != 0);
}

static int
decomp_read(compfsfile_t *fileptr, caddr_t buf, int count)
{
	if (fileptr->compressed) {
		decomp_file(fileptr);
		if (count > decomp_size - fileptr->decomp_offset)
			count = decomp_size - fileptr->decomp_offset;
		if (count <= 0) {
#ifdef DECOMP_DEBUG
			if (compfsverbose || cpfsdebuglevel > 2) {
				printf("decomp_read: returning end-of-file\n");
			}
#endif
			return (0);
		}
		decomp_copyout(fileptr, buf, count);
		fileptr->decomp_offset += count;
#ifdef DECOMP_DEBUG
		if (compfsverbose || cpfsdebuglevel > 2) {
			printf("decomp_read: returning %d bytes\n", count);
		}
#endif
		return (count);
	}
	return ((*extendfs_ops->fsw_read)(fileptr->efd, buf, count));
}

static int
decomp_close(compfsfile_t *fileptr)
{
	if (fileptr->efd == decomp_filedes) {
#ifdef DECOMP_DEBUG
		if (compfsverbose || cpfsdebuglevel > 2) {
			printf("decomp_close: closing active file\n");
		}
#endif
		decomp_free_data();
		decomp_filedes = 0;
	}
	return ((*extendfs_ops->fsw_close)(fileptr->efd));
}

void
decomp_closeall(int flag)
{
	if (decomp_filedes)
		decomp_filedes = 0;
	decomp_fini();
#ifdef DECOMP_DEBUG
	if (compfsverbose || cpfsdebuglevel > 2) {
		printf("decomp_umount: finished with decompression\n");
	}
#endif
	(*extendfs_ops->fsw_closeall)(flag);
}

static void
decomp_file(compfsfile_t *fileptr)
{
	/* With luck we are already set up for this file */
	if (decomp_data && decomp_filedes == fileptr->efd &&
			decomp_size != 0) {
#ifdef DECOMP_DEBUG
		if (compfsverbose || cpfsdebuglevel > 2) {
			printf("decomp_file: already cached\n");
		}
#endif
		return;
	}

	/* Set up to look like an empty file if we fail */
	decomp_free_data();
	decomp_filedes = fileptr->efd;
	decomp_size = 0;

	/*
	 * Initialize internal decompression bookkeeping.
	 */
	if (decomp_init() < 0) {
#ifdef DECOMP_DEBUG
		if (compfsverbose || cpfsdebuglevel > 2) {
			printf("decomp_file: decomp_init failed\n");
		}
#endif
		decomp_filedes = 0;
		return;
	}
	decompress();
#ifdef DECOMP_DEBUG
	if (compfsverbose || cpfsdebuglevel > 2) {
		printf("decomp_file: decompressed file is 0x%x bytes\n",
			decomp_size);
	}
#endif
}

#define	DCBSIZE		(64 * 1024)
static char *dcr_p;	/* pointer to decompression read buffer */
static int dcr_n;	/* number of bytes in decompression read buffer */
static int dcr_i;	/* index into decompression read buffer */

static int
decomp_init()
{
	if ((dcr_p == 0) && ((dcr_p = bkmem_alloc(DCBSIZE)) == 0))
		return (-1);
	if (decompress_init() < 0) {
		bkmem_free(dcr_p, DCBSIZE);
		dcr_p = 0;
		return (-1);
	}
	dcr_n = 0;
	dcr_i = 0;
	return (0);
}

static void
decomp_fini()
{
	if (dcr_p)
		bkmem_free(dcr_p, DCBSIZE);
	decomp_free_data();
	decompress_fini();
	dcr_p = 0;
}



/*
 * The following routines are callbacks from the
 * decompress function to get data from the compressed
 * input stream and to write the decompressed data
 * to the output stream. The input is buffered in big
 * chunks to keep the floppy moving along.
 *
 * The output is dumped into a buffer that must be big
 * enough for the largest file. This could be changed
 * to dynamically grow a linked list in chunks so that
 * there is no set limit other than the heap.
 */

int
decomp_getbytes(char *buffer, int count)
{
	int len, res, xfer;

	xfer = 0;	/* bytes transferred */
	res = count;	/* residual of request */
	do {
		if (dcr_n == 0) {
			dcr_n = (*extendfs_ops->fsw_read)
				(decomp_filedes, dcr_p, DCBSIZE);
			if (dcr_n <= 0) {
				/* EOF */
				dcr_n = 0;
				break;
			}
			dcr_i = 0;
		}
		if (res <= dcr_n) {
			len = res;
		} else {
			len = dcr_n;
		}
		bcopy(dcr_p+dcr_i, buffer+xfer, len);
		dcr_i += len;
		dcr_n -= len;
		xfer += len;
		res -= len;
	} while (res != 0);

	return (xfer);
}

int
decomp_getchar(void)
{
	char c;

	if (decomp_getbytes(&c, 1) <= 0)
		return (-1);
	return (c & 0xff);
}

/*
 * Put the decompressed bytes into a list of data buffers. The buffers
 * are allocated on the fly.
 */
void
decomp_putchar(char c)
{
	static struct decomp_list *dlp;
	static int indx, max_indx;
	struct decomp_list *old_dlp;

	if (decomp_filedes == 0) {
		/* could not get memory on previous call, dump the byte */
		return;
	}

	if (decomp_data == 0) {
		/* first call for this file */
		max_indx = 0;
	}

	if (decomp_size == max_indx) {
		/* need to link another buffer */
		old_dlp = dlp;
		dlp = (struct decomp_list *)bkmem_alloc(sizeof (*dlp));
		if (dlp == 0) {
			decomp_filedes = 0;
			decomp_size = 0;
			return;
		}
		if (decomp_data == 0) {
			decomp_data = dlp;
		} else {
			old_dlp->dl_next = dlp;
		}
		indx = 0;
		max_indx += DLCHUNK;
	}
	dlp->dl_data[indx++] = c;
	decomp_size++;
}

static void
decomp_free_data()
{
	struct decomp_list *dlp, *n_dlp;

	if (decomp_data == 0)
		return;
	dlp = decomp_data;
	while (dlp) {
		n_dlp = dlp->dl_next;
		bkmem_free((char *)dlp, sizeof (*dlp));
		dlp = n_dlp;
	}
	decomp_data = 0;
	decomp_filedes = 0;
	decomp_size = 0;
}
