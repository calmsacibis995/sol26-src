/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_I386_SYS_RAMFILE_H
#define	_I386_SYS_RAMFILE_H

#pragma ident	"@(#)ramfile.h	1.5	96/05/13 SMI"

#include <sys/types.h>
#include <sys/stat.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	RAMblk_ERROR	-2
#define	RAMblk_OK	0

#define	RAMfile_EOF	-2
#define	RAMfile_ERROR	-1
#define	RAMfile_OK	0

#define	RAMfile_BLKSIZE	1024

#define	RAMrewind(rfd) \
	(void) RAMfile_lseek((rfd), 0, SEEK_SET)

#define	RAMtrunc(rfd) \
	RAMfile_trunc((rfd)->file, (rfd)->file->attrib)

typedef struct rblk {
	struct rblk *next;
	struct rblk *prev;
	char	*datap;
} rblk_t;

#define	RAMfp_modified	0x1	/* Indicate if RAMfile actually modified */

typedef struct rfil {
	struct rfil *next;
	rblk_t	*contents;
	char	*name;
	ulong	attrib;
	ulong	size;
	ulong	flags;
} rfil_t;

typedef struct orf {
	rfil_t	*file;
	ulong	foff;	/* Absolute offset of file pointer */
	char	*fptr;	/* Pointer to offset's storage in actual RAMblk */
	rblk_t	*cblkp;	/* Pointer to RAMblk struct in which offset resides */
	/*
	 * cblkn -- Indicates which RAMfile_BLKSIZE sized
	 * 	chunk the current offset resides in.
	 * 	Chunks numbered from zero.
	 */
	ulong	cblkn;
} rffd_t;

/*
 *  RAMblk prototypes
 */
rblk_t	*RAMblk_alloc(void);
int	RAMblklst_free(rblk_t *);

/*
 *  RAMfile prototypes
 */
extern	void	RAMfile_addtolist(rfil_t *);
extern	void	RAMfile_rmfromlist(rfil_t *);
extern	rfil_t	*RAMfile_alloc(char *, ulong);
extern	int	RAMfile_free(rfil_t *);
extern	rffd_t	*RAMfile_allocfd(rfil_t *);
extern	int	RAMfile_freefd(rffd_t *);
extern	rffd_t	*RAMfile_open(char *, ulong);
extern	int	RAMfile_close(rffd_t *);
extern	char	*RAMfile_striproot(char *);
extern	int	RAMfile_trunc_atoff(rffd_t *);
extern	void	RAMfile_trunc(rfil_t *, ulong);
extern	rffd_t	*RAMfile_create(char *, ulong);
extern	int	RAMfile_destroy(char *);
extern	off_t	RAMfile_lseek(rffd_t *, off_t, int);
extern	int	RAMfile_fstat(rffd_t *, struct stat *);
extern	int	RAMfile_write(rffd_t *, char *, int);
extern	int	RAMfile_read(rffd_t *, char *, int);
extern	char	*RAMfile_gets(char *, int, rffd_t *);
extern	int	RAMfile_puts(char *, rffd_t *);
extern  int	RAMfile_rename(rffd_t *, char *);
extern	rffd_t	*RAMcvtfile(char *, ulong);
extern	void	RAMfiletoprop(rffd_t *);

#ifdef	__cplusplus
}
#endif

#endif /* _I386_SYS_RAMFILE_H */
