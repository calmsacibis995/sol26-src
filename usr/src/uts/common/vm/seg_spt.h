/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

#ifndef	_VM_SPT_H
#define	_VM_SPT_H

#pragma ident	"@(#)seg_spt.h	1.5	96/03/28 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>

#ifdef	__cplusplus
extern "C" {
#endif



/*
 * Passed data when creating spt segment.
 */
struct  segspt_crargs {
	struct	seg	*seg_spt;
	struct anon_map *amp;
};

struct spt_data {
	struct vnode	*vp;
	struct anon_map	*amp;
	u_int realsize;
	struct	page	**ppa;
};

/*
 * Private data for spt_shm segment.
 */
struct sptshm_data {
	struct as	*sptas;
	struct anon_map *amp;
	int		softlockcnt;
	kmutex_t 	lock;
	struct	page	**ppa;
	struct seg 	*sptseg;	/* pointer to spt segment */
};

#ifdef _KERNEL

/*
 * Functions used in shm.c to call ISM.
 */
int	sptcreate(u_int size, struct seg **sptseg, struct anon_map *amp);
void	sptdestroy(struct as *, struct anon_map *);
int	segspt_shmattach(struct seg *, caddr_t *);

extern void atomic_add_word(u_int *word, int value, struct mutex *lock);

#define	isspt(sp)	((sp)->shm_sptas)
#define	spt_on(a)	(share_page_table || ((a) & SHM_SHARE_MMU))

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _VM_SPT_H */
