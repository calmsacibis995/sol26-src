/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_WATCHPOINT_H
#define	_SYS_WATCHPOINT_H

#pragma ident	"@(#)watchpoint.h	1.1	96/06/18 SMI"

#include <sys/types.h>
#include <vm/seg_enum.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Definitions for the VM implementation of watchpoints.
 * See proc(4) and <sys/procfs.h> for definitions of the user interface.
 */

/*
 * Each process with watchpoints has a linked list of watched areas.
 * The list is kept sorted by user-level virtual address.
 */
struct watched_area {
	struct watched_area *wa_forw;	/* linked list */
	struct watched_area *wa_back;
	caddr_t	wa_vaddr;	/* virtual address of watched area */
	caddr_t	wa_eaddr;	/* virtual address plus size */
	u_int	wa_flags;	/* watch type flags (see <sys/procfs.h>) */
};

/*
 * The list of watched areas maps into a list of pages with modified
 * protections.  The list is kept sorted by user-level virtual address.
 */
struct watched_page {
	struct watched_page *wp_forw;	/* linked list */
	struct watched_page *wp_back;
	caddr_t	wp_vaddr;	/* virtual address of this page */
	u_char	wp_prot;	/* modified protection bits */
	u_char	wp_oprot;	/* original protection bits */
	u_char	wp_umap[3];	/* reference counts of user pr_mappage()s */
	u_char	wp_kmap[3];	/* reference counts of kernel pr_mappage()s */
	u_short	wp_flags;	/* see below */
	short	wp_read;	/* number of WA_READ areas in this page */
	short	wp_write;	/* number of WA_WRITE areas in this page */
	short	wp_exec;	/* number of WA_EXEC areas in this page */
};

/* wp_flags */
#define	WP_NOWATCH	0x01	/* protections temporarily restored */
#define	WP_SETPROT	0x02	/* SEGOP_SETPROT() needed on this page */

#ifdef	_KERNEL

struct k_siginfo;
extern	int	pr_mappage(caddr_t, u_int, enum seg_rw, int);
extern	void	pr_unmappage(caddr_t, u_int, enum seg_rw, int);
extern	void	setallwatch(void);
extern	int	pr_is_watchpage(caddr_t, enum seg_rw);
extern	int	pr_is_watchpoint(caddr_t *, int *, int, size_t *, enum seg_rw);
extern	void	do_watch_step(caddr_t, int, enum seg_rw, int, greg_t);
extern	int	undo_watch_step(struct k_siginfo *);

extern	int	watch_copyin(caddr_t, caddr_t, size_t);
extern	int	watch_xcopyin(caddr_t, caddr_t, size_t);
extern	int	watch_copyout(caddr_t, caddr_t, size_t);
extern	int	watch_xcopyout(caddr_t, caddr_t, size_t);
extern	int	watch_copyinstr(char *, char *, size_t, size_t *);
extern	int	watch_copyoutstr(char *, char *, size_t, size_t *);
extern	int	watch_fubyte(caddr_t);
extern	int	watch_fuibyte(caddr_t);
extern	int	watch_fuword(int *);
extern	int	watch_fuiword(int *);
extern	int	watch_fusword(caddr_t);
extern	int	watch_subyte(caddr_t, char);
extern	int	watch_suibyte(caddr_t, char);
extern	int	watch_suword(int *, int);
extern	int	watch_suiword(int *, int);
extern	int	watch_susword(caddr_t, int);

extern	int	_copyin(caddr_t, caddr_t, size_t);
extern	int	_xcopyin(caddr_t, caddr_t, size_t);
extern	int	_copyout(caddr_t, caddr_t, size_t);
extern	int	_xcopyout(caddr_t, caddr_t, size_t);
extern	int	_copyinstr(char *, char *, size_t, size_t *);
extern	int	_copyoutstr(char *, char *, size_t, size_t *);
extern	int	_fubyte(caddr_t);
extern	int	_fuibyte(caddr_t);
extern	int	_fuword(int *);
extern	int	_fuiword(int *);
extern	int	_fusword(caddr_t);
extern	int	_subyte(caddr_t, char);
extern	int	_suibyte(caddr_t, char);
extern	int	_suword(int *, int);
extern	int	_suiword(int *, int);
extern	int	_susword(caddr_t, int);

#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_WATCHPOINT_H */
