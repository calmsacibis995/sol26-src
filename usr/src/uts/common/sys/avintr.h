/*
 * Copyright (c) 1992-1996, by Sun Microsystems, Inc.
 */

#ifndef _SYS_AVINTR_H
#define	_SYS_AVINTR_H

#pragma ident	"@(#)avintr.h	1.10	96/06/28 SMI"

#include <sys/mutex.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Period of autovector structures (add this in to get the next level).
 */
#define	MAXIPL	16

#define	INT_IPL(x) (x)

#define	AV_INT_SPURIOUS	-1

#ifdef	__STDC__
typedef u_int (*avfunc)(caddr_t);
#else
typedef u_int (*avfunc)();
#endif	/* __STDC__ */

struct autovec {

	/*
	 * Interrupt handler and argument to pass to it.
	 */

	struct autovec *av_link;	/* pointer to next on in chain */
	u_int	(*av_vector)();
	caddr_t	av_intarg;
	u_int	av_prilevel;		/* priority level */

	/*
	 * XXX: temporary
	 * av_mutex, if non-zero, points to &unsafe_driver.
	 */
	kmutex_t	*av_mutex;

	/*
	 * Interrupt handle/id (like intrspec structure pointer) used to
	 * identify a specific instance of interrupt handler in case we
	 * have to remove the interrupt handler later.
	 *
	 */
	void	*av_intr_id;
};

struct av_head {
	struct 	autovec	*avh_link;
	u_short	avh_hi_pri;
	u_short	avh_lo_pri;
};

/* softing contains a bit field of software interrupts which are pending */
struct softint {
	int st_pending;
};

#ifdef _KERNEL

extern kmutex_t av_lock;
extern int add_avintr(void *intr_id, int lvl, avfunc xxintr, char *name,
	int vect, caddr_t arg, kmutex_t *mutex_p);
extern int add_nmintr(int lvl, avfunc nmintr, char *name, caddr_t arg);
extern int add_avsoftintr(void *intr_id, int lvl, avfunc xxintr,
	char *name, caddr_t arg, kmutex_t *mutex_p);
extern int rem_avsoftintr(void *intr_id, int lvl, avfunc xxintr);
extern void rem_avintr(void *intr_id, int lvl, avfunc xxintr, int vect);
extern void wait_till_seen(int ipl);
extern u_int softlevel1(caddr_t);
extern u_int nullintr(caddr_t intrg);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_AVINTR_H */
