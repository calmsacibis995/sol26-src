#ident	"@(#)rwlock.c	1.55	96/09/10 SMI"

/*
 * This file contains the readers/writer locking intrinsics.
 */

#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/thread.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/vtrace.h>
#include <sys/dki_lkinfo.h>
#include <sys/dki_lock.h>
#include <sys/tblock.h>
#include <sys/sleepq.h>
#include <sys/prioinherit.h>
#include <sys/mutex.h>
#include <sys/mutex_impl.h>
#include <sys/rwlock_impl.h>
#include <sys/rwlock.h>

extern char *panicstr;
extern void panic_hook();

/* rwlock default (no statistics) functions */
static void rw_sleep_init(krwlock_t *, char *, krw_type_t, void *);
static void rw_sleep_destroy(krwlock_t *);

/* rwlock default with statistics functions */
static void rw_stat_init(krwlock_t *, char *, krw_type_t, void *);
static void rw_stat_enter(krwlock_t *, krw_t);
static int rw_stat_tryenter(krwlock_t *, krw_t);
static void rw_stat_downgrade(krwlock_t *);
static int rw_stat_tryupgrade(krwlock_t *);
static lkstat_t *rw_stat_get(krwlock_t *);
static lkstat_t *rw_no_stats(krwlock_t *);
static void rw_stat_exit(krwlock_t *);
static void rw_stat_destroy(krwlock_t *);

/* Driver-type initialization */
static void rw_driver_init(krwlock_t *, char *, krw_type_t, void *);
static void rw_driver_stat_init(krwlock_t *, char *, krw_type_t, void *);

extern int lock_stats;

static struct rwlock_stats *rwlock_stats_alloc(int sleep);
static void rwlock_stats_free(struct rwlock_stats *sp);

/*
 * The internal data of readers/writer locks is protected by a mutex.
 * The mutex is chosen from an array based on the address of the
 * readers/writer lock.
 */
#define	RW_MUTEXES	32		/* size of hashed mutex array */
#define	RW_MUTEX_HASH(rwlp)	\
		((((u_int)(rwlp) >> 2) + ((u_int)(rwlp) >> 9)) % RW_MUTEXES)
#define	RW_MUTEX(rwlp)	(&rw_mutex[RW_MUTEX_HASH(rwlp)])

static kmutex_t	rw_mutex[RW_MUTEXES];	/* don't do stats on these */

static kmutex_t rw_addword_mutex;

kthread_t *rw_owner(krwlock_t *rwlp);
static void reader_unsleep(kthread_t *t);
static void writer_unsleep(kthread_t *t);
static void reader_changepri(kthread_t *t, pri_t pri);
static void writer_changepri(kthread_t *t, pri_t pri);

/*
 * The sobj_ops vector exports a set of functions needed
 * when a thread is asleep on a synchronization object of
 * this type.
 *
 * Every blocking s-object should define one of these
 * structures and set the t_sobj_ops field of blocking
 * threads to it's address.
 *
 * Current users are setrun() and pi_changepri().
 */
/*
 * Vector for threads blocked waiting for a readers' lock
 */
static sobj_ops_t	reader_sops = {
	"Readers' Lock",
	SOBJ_READER,
	QOBJ_READER,
	rw_owner,
	reader_unsleep,
	reader_changepri
};

/*
 * Vector for threads blocked waiting for a writer lock.
 */
static sobj_ops_t	writer_sops = {
	"Writer Lock",
	SOBJ_WRITER,
	QOBJ_WRITER,
	rw_owner,
	writer_unsleep,
	writer_changepri
};


void
rw_mutex_init()
{
	char	name[30];
	int	i;
	kmutex_t	*lp;

	i = 0;
	for (lp = rw_mutex; lp < &rw_mutex[RW_MUTEXES]; lp++) {
		sprintf(name, "rw hash mutex %2d", i++);
		mutex_init(lp, name, MUTEX_DEFAULT, NULL);
	}
}

/* rwlock operations for various types */
struct rwlock_ops {
	void	(*rw_init)(krwlock_t *, char *, krw_type_t, void *);
	void	(*rw_enter)(krwlock_t *, krw_t);
	int	(*rw_tryenter)(krwlock_t *, krw_t);
	void	(*rw_downgrade)(krwlock_t *);
	int	(*rw_tryupgrade)(krwlock_t *);
	lkstat_t *(*rw_stats)(krwlock_t *);
	void	(*rw_exit)(krwlock_t *);
	void	(*rw_destroy)(krwlock_t *);
};

static struct rwlock_ops rwlock_ops[] = {
	{
		rw_sleep_init,		/* RW_SLEEP: vanilla without stats */
		rw_enter,
		rw_tryenter,
		rw_downgrade,
		rw_tryupgrade,
		rw_no_stats,
		rw_exit,
		rw_sleep_destroy
	},
	{
		rw_stat_init,		/* sleep with statistics */
		rw_stat_enter,
		rw_stat_tryenter,
		rw_stat_downgrade,
		rw_stat_tryupgrade,
		rw_stat_get,
		rw_stat_exit,
		rw_stat_destroy
	},
	{
		rw_driver_init		/* driver (default) */
					/* type converted by init routine */
	},
	{
		rw_driver_stat_init	/* driver (default) with stats */
					/* type converted by init routine */
	},
	{
		rw_driver_init		/* RW_DEFAULT: stats if enabled */
					/* type converted by init routine */
	}
};

#define	RWLOCK_NTYPES (sizeof (rwlock_ops) / sizeof (struct rwlock_ops))

/*
 * Macro to find the ops vector based on the offset stored in the mutex.
 *
 * This lets rw_enter, for example say:
 * (*(RWLOCK_OP(type_offset)->rw_enter))(lp);
 * Which is faster than:
 * (*(rwlock_ops[type]->rw_enter))(lp);
 * Because type_offset precomputes the multiply needed by indexing.
 */
#define	RWLOCK_OP(type) ((struct rwlock_ops *)((caddr_t)rwlock_ops + (type)))

/*
 * get internal type number that is set in the lock itself.
 */
#define	RWLOCK_INT_TYPE(type)	\
	((caddr_t)&rwlock_ops[(type)] - (caddr_t)rwlock_ops)
/*
 * get the old type (i.e. RW_SLEEP or RW_SLEEP_STAT from the offset stored
 * in the lock's "type" field (only after it's been init'd).
 */
#define	RWLOCK_TYPE(offset)	((offset) / sizeof (struct rwlock_ops))

lkstat_t *
rwlock_stats(krwlock_t *rwlp)
{
	rwlock_impl_t	*lp = (rwlock_impl_t *)rwlp;

	return ((*(RWLOCK_OP(lp->type)->rw_stats))(rwlp));
}

/* ARGSUSED */
static lkstat_t *
rw_no_stats(krwlock_t *lp)
{
	return (NULL);
}

/* ARGSUSED */
void
rw_init(krwlock_t *rwlp, char *name, krw_type_t type, void *arg)
{
	register struct rwlock_ops *op;
	register u_int ops_offset;
	register rwlock_impl_t	*lp = (rwlock_impl_t *)rwlp;

	if ((unsigned)type >= RWLOCK_NTYPES) {
		cmn_err(CE_PANIC,
			"rw_init: bad type %d name %s", type, name);
	}
	/*
	 * Make the type field in the lock be the offset in rwlock_ops
	 * for the start of the ops for this type.  This will save a little
	 * time in rw_enter().
	 */
	op = &rwlock_ops[type];
	ops_offset = (caddr_t)op - (caddr_t)rwlock_ops;
	ASSERT(ops_offset < 256);	/* must fit in a byte */
	lp->type = (u_char) ops_offset;
	(*op->rw_init)(rwlp, name, type, arg);
}

void
rw_destroy(krwlock_t *rwlp)
{
	rwlock_impl_t *lp = (rwlock_impl_t *)rwlp;

	(*(RWLOCK_OP(lp->type)->rw_destroy))(rwlp);
}

extern atomic_add_hword(u_short *hword, int value, struct mutex *lock);
extern int cas(u_int *ptr, u_int old_value, u_int new_value);
static void
rw_enter_sleep(rwlock_impl_t *lp, krw_t rw)
{
	register turnstile_t	*ts;


	if (panicstr) {
		panic_hook();
		return;
	}

	disp_lock_enter(&lp->rw_wlock);
	while ((rw == RW_READER &&
	    ((lp->un.rw.holdcnt < 0) || lp->writewanted)) ||
	    ((rw == RW_WRITER) && lp->un.rw.holdcnt)) {
		if (lp->un.rw.waiters == 0) {
			ts = tstile_alloc();
			lp->un.rw.waiters = ts->ts_id;
			lock_mutex_flush();
		}
		ts = tstile_pointer(lp->un.rw.waiters);
		if (((rw == RW_READER) &&
		    (lp->un.rw.holdcnt < 0 || lp->writewanted)) ||
		    ((rw == RW_WRITER) && lp->un.rw.holdcnt)) {
			thread_lock_high(curthread);
			t_block(ts, (caddr_t)lp,
			    (rw == RW_READER) ? &reader_sops : &writer_sops,
			    &lp->rw_wlock);
			pi_willto(curthread);	/* drops thread lock */
			swtch();
			disp_lock_enter(&lp->rw_wlock);
		} else {
			/*
			 * If we dont release the tunstile here, rw_exit
			 * could do a disp_lock_exit() on a freed rw_lock
			 */
			t_release(ts, &lp->un.rw.waiters, QOBJ_WRITER);
		}
	}
	disp_lock_exit(&lp->rw_wlock);
}

static void
rw_exit_wakeup(rwlock_impl_t *lp)
{
	register turnstile_t	*ts;
	u_int	hldcnt;


	/*
	 * we have to decrement the holdcnt only after we have
	 * looked at the waiters field.
	 */
	disp_lock_enter(&lp->rw_wlock);
	ts = tstile_pointer(lp->un.rw.waiters);
	pi_waive(ts);
	if (lp->un.rw.holdcnt > 0) {
		/*
		 * we are giving up the mutex. Since we have a
		 * waiter ( not an empty turnstile)
		 * the call to disp_lock_exit_nopreempt() is safe.
		 */

		/*
		 * atomic_decw((u_short *)&lp->un.rw.holdcnt)
		 * lp->un.rw.holdcnt is not zero
		 */
		while (((hldcnt = *(u_int *)&lp->un.rw.holdcnt) != 0) &&
		    	(cas((u_int *)&lp->un.rw.holdcnt, hldcnt, hldcnt-1)
			    != hldcnt));
		if (lp->un.rw.holdcnt == 0)
			t_release(ts, &lp->un.rw.waiters, QOBJ_WRITER);
	} else {
		if (lp->writewanted > 0)
			t_release(ts, &lp->un.rw.waiters, QOBJ_WRITER);
		else
			t_release_all(ts, &lp->un.rw.waiters, QOBJ_READER);
		lp->un.rw.holdcnt = 0;
	}
	THREAD_KPRI_RELEASE();
	disp_lock_exit_nopreempt(&lp->rw_wlock);
}


void
rw_enter(krwlock_t *rwlp, krw_t rw)
{
	register rwlock_impl_t	*lp = (rwlock_impl_t *)rwlp;
	u_int	*hldcnt_ptr, hldcnt_and_waiters;
	u_int	new_hldcnt, hldcnt_mask;

	if (lp->type != RWLOCK_INT_TYPE(RW_SLEEP)) {
		(*(RWLOCK_OP(lp->type)->rw_enter))(rwlp, rw);
		return;
	}
	THREAD_KPRI_REQUEST();
	hldcnt_ptr = (u_int *)&lp->un.rw.holdcnt;

#ifdef STATISTICS
	CPU_STAT_ADDQ(CPU, cpu_sysinfo.rw_enters, 1);
#endif

	switch (rw) {
	case RW_READER:
		TRACE_2(TR_FAC_LOCK, TR_RW_ENTER_RD_START,
			"rw_enter_reader_start:rwlp %x holdcnt %d",
			lp, lp->un.rw.holdcnt);
		do {
		    while (lp->writewanted ||
			((hldcnt_and_waiters = *(u_short *)hldcnt_ptr)
			    == RWLCK_HLDCNT_MASK)) {
			CPU_STAT_ADDQ(CPU, cpu_sysinfo.rw_rdfails, 1);
			/*
			 * If writewanted is set or if its locked by
			 * writer
			 */
			    rw_enter_sleep(lp, rw);
		    }
		    new_hldcnt = hldcnt_and_waiters + 1;
		} while (cas(hldcnt_ptr, hldcnt_and_waiters, new_hldcnt) !=
				hldcnt_and_waiters);
		if (hldcnt_and_waiters == 0)
			/*
			 * set the owner if we incremented the hldcnt
			 * from 0 to 1
			 */
			lp->owner = curthread;

		TRACE_2(TR_FAC_LOCK, TR_RW_ENTER_RD_END,
			"rw_enter_reader_end:rwlp %x holdcnt %d",
			lp, lp->un.rw.holdcnt);
		break;

	case RW_WRITER:
		TRACE_2(TR_FAC_LOCK, TR_RW_ENTER_WR_START,
			"rw_enter_writer_start:rwlp %x holdcnt %d",
			lp, lp->un.rw.holdcnt);
		/*
		 * hldcnt_mask is set to 0xffffffff so that if
		 * there are threads wanting write lock this thread
		 * is made to wait to ensure first-in first-out.
		 */
		hldcnt_mask = (u_int)-1;
		do {
			while ((hldcnt_and_waiters = *hldcnt_ptr) &
			    hldcnt_mask){
				if (hldcnt_mask == (u_int)-1) {
					atomic_add_hword(
					    (u_short *)&lp->writewanted, 1,
					    &rw_addword_mutex);
					/*
					 * henceforth ignore the waiters field
					 *
					 */
					hldcnt_mask = RWLCK_HLDCNT_MASK;
				}
				CPU_STAT_ADDQ(CPU, cpu_sysinfo.rw_wrfails, 1);
				rw_enter_sleep(lp, rw);
			}
			/*
			 * set hldcnt to -1, lower 16 bits of
			 * hldcnt_and_waiters are zero
			 */
			new_hldcnt = hldcnt_and_waiters|RWLCK_HLDCNT_MASK;
		} while (cas(hldcnt_ptr, hldcnt_and_waiters, new_hldcnt) !=
				hldcnt_and_waiters);
		lp->owner = curthread;
		if (hldcnt_mask == RWLCK_HLDCNT_MASK)
			atomic_add_hword((u_short *)&lp->writewanted, -1,
			    &rw_addword_mutex);
		break;
	default:
		cmn_err(CE_PANIC,
			"rw_enter: bad rw %d, rwlp = 0x%x", rw, (int)lp);
		break;
	}
}


int
rw_tryenter(krwlock_t *rwlp, krw_t rw)
{
	register rwlock_impl_t	*lp = (rwlock_impl_t *)rwlp;
	u_int	*hldcnt_ptr, hldcnt_and_waiters;
	u_int	new_hldcnt;

	if (lp->type != RWLOCK_INT_TYPE(RW_SLEEP)) {
		return ((*(RWLOCK_OP(lp->type)->rw_tryenter))(rwlp, rw));
	}
	THREAD_KPRI_REQUEST();
	hldcnt_ptr = (u_int *)&lp->un.rw.holdcnt;

	switch (rw) {

	case RW_READER:
		do {
			if (lp->writewanted ||
			    ((hldcnt_and_waiters = *(u_short *)hldcnt_ptr) ==
				RWLCK_HLDCNT_MASK)){
				THREAD_KPRI_RELEASE();
				return (0);
			}
			new_hldcnt = hldcnt_and_waiters + 1;
		} while (cas(hldcnt_ptr, hldcnt_and_waiters, new_hldcnt) !=
				hldcnt_and_waiters);
		if (hldcnt_and_waiters  == 0)
			lp->owner = curthread;
		break;

	case RW_WRITER:
		hldcnt_and_waiters = 0;
		new_hldcnt = RWLCK_HLDCNT_MASK;
		if (cas(hldcnt_ptr, hldcnt_and_waiters, new_hldcnt) !=
			hldcnt_and_waiters) {
			THREAD_KPRI_RELEASE();
			return (0);
		}
		lp->owner = curthread;
		break;
	default:
		cmn_err(CE_PANIC,
			"rw_tryenter: bad rw %d, rwlp = 0x%x", rw, (int)lp);
		break;
	}

	return (1);
}
void
rw_downgrade(krwlock_t *rwlp)
{
	register rwlock_impl_t	*lp = (rwlock_impl_t *)rwlp;

	if (lp->type != RWLOCK_INT_TYPE(RW_SLEEP)) {
		(*(RWLOCK_OP(lp->type)->rw_downgrade))(rwlp);
		return;
	}


	ASSERT(((lp->un.rw.holdcnt < 0) && (lp->owner == curthread)) ||
	    panicstr);

	lp->un.rw.holdcnt = 1;
	lock_mutex_flush();
	/*
	 * if no waiting writers then wakeup readers
	 */
	if (!(lp->writewanted)) {
		register turnstile_t	*ts;

		if (lp->un.rw.waiters != 0) {
			disp_lock_enter(&lp->rw_wlock);
			ts = tstile_pointer(lp->un.rw.waiters);
			pi_waive(ts);
			t_release_all(ts, &lp->un.rw.waiters, QOBJ_READER);
			disp_lock_exit(&lp->rw_wlock);
		}
	}
}

int
rw_tryupgrade(krwlock_t *rwlp)
{
	register rwlock_impl_t	*lp = (rwlock_impl_t *)rwlp;
	u_int	*hldcnt_ptr, hldcnt_and_waiters, new_hldcnt;

	if (lp->type != RWLOCK_INT_TYPE(RW_SLEEP)) {
		return ((*(RWLOCK_OP(lp->type)->rw_tryupgrade))(rwlp));
	}
	hldcnt_ptr = (u_int *)&lp->un.rw.holdcnt;
	if (lp->writewanted || ((hldcnt_and_waiters = *hldcnt_ptr) != 1)) {
		return (0);
	}
	new_hldcnt = RWLCK_HLDCNT_MASK;
	if (cas(hldcnt_ptr, hldcnt_and_waiters, new_hldcnt) !=
		hldcnt_and_waiters)
		return (0);
	lp->owner = curthread;
	return (1);
}

void
rw_exit(krwlock_t *rwlp)
{
	register rwlock_impl_t	*lp = (rwlock_impl_t *)rwlp;
	u_int	*hldcnt_ptr, hldcnt_and_waiters, new_hldcnt;
	int	was_owner;

	TRACE_2(TR_FAC_LOCK, TR_RW_EXIT_START,
		"rw_exit_start:rwlp %x holdcnt %d",
		lp, lp->un.rw.holdcnt);

	if (lp->type != RWLOCK_INT_TYPE(RW_SLEEP)) {
		(*(RWLOCK_OP(lp->type)->rw_exit))(rwlp);
		return;
	}

	ASSERT(lp->un.rw.holdcnt != 0);
	hldcnt_ptr = (u_int *)&lp->un.rw.holdcnt;

	if (lp->un.rw.holdcnt < 0) {		/* if this is a write lock */
		ASSERT(lp->owner == curthread || panicstr);
		lp->owner = NULL;
		do {
			if (lp->un.rw.waiters != 0) {
				rw_exit_wakeup(lp);
				return;
			}
		} while (cas(hldcnt_ptr, RWLCK_HLDCNT_MASK, 0) !=
				RWLCK_HLDCNT_MASK);
	} else {
		if (lp->owner == curthread) {
			lp->owner = NULL;
			was_owner = 1;
		} else was_owner = 0;
		do {
			hldcnt_and_waiters = *hldcnt_ptr;
			new_hldcnt = hldcnt_and_waiters - 1;
			if (lp->un.rw.waiters &&
			    (((new_hldcnt & RWLCK_HLDCNT_MASK) == 0) ||
			    was_owner)) {
				/*
				 * if there is a waiter and if we are the
				 * owner we need to give up inherited
				 * priority. If we are the last reader
				 * lock we need to wakeup writers
				 */
				rw_exit_wakeup(lp);
				return;
			}
		} while (cas(hldcnt_ptr, hldcnt_and_waiters, new_hldcnt) !=
				hldcnt_and_waiters);
	}
	THREAD_KPRI_RELEASE();
	TRACE_2(TR_FAC_LOCK, TR_RW_EXIT_END,
		"rw_exit_end:rwlp %x holdcnt %d",
		lp, lp->un.rw.holdcnt);
}
/*
 * rwlock default (without statistics) functions
 */
/* ARGSUSED */
static void
rw_sleep_init(krwlock_t *rwlp, char *name, krw_type_t type, void *arg)
{
	register rwlock_impl_t	*lp = (rwlock_impl_t *)rwlp;

	lp->un.rw.holdcnt = 0;
	lp->owner = NULL;
	lp->un.rw.waiters = 0;
	lp->writewanted = 0;
	disp_lock_init(&lp->rw_wlock, "rw waiters");
}

static void
rw_sleep_destroy(krwlock_t *rwlp)
{
	register rwlock_impl_t *lp = (rwlock_impl_t *)rwlp;
	turnstile_id_t	waiters;
	turnstile_t	*ts;

	/*
	 * rw_enter could leave an empty turnstile associated with the lock.
	 * The lock could be held by the current thread, and if so, the
	 * empty turnstile could still be there.  If so, free it.
	 */
	if ((waiters = lp->un.rw.waiters) != NULL) {
		disp_lock_enter(&lp->rw_wlock);
		ts = tstile_pointer(waiters);
		ASSERT(TSTILE_EMPTY(ts, QOBJ_READER));
		ASSERT(TSTILE_EMPTY(ts, QOBJ_WRITER));
		tstile_free(ts, &lp->un.rw.waiters);
		disp_lock_exit(&lp->rw_wlock);
	}
	disp_lock_destroy(&lp->rw_wlock);
}

/* rwlock default functions */
static lkstat_t *
rw_stat_get(krwlock_t *rwlp)
{
	register rwlock_impl_t *lp = (rwlock_impl_t *)rwlp;

	if (lp->un.sp) {
		return (lp->un.sp->lkstat);
	} else {
		return (NULL);
	}
}

/* ARGSUSED */
static void
rw_stat_init(krwlock_t *rwlp, char *name, krw_type_t type, void *arg)
{
	register rwlock_impl_t *lp = (rwlock_impl_t *)rwlp;
	register struct rwlock_stats *sp;

	lp->owner = NULL;
	lp->un.rw.waiters = 0;
	lp->writewanted = 0;
	lp->rw_wlock = 0;
	sp = rwlock_stats_alloc(KM_SLEEP);
	lp->un.sp = sp;
	(void) sprintf_len(LOCK_NAME_LEN, sp->name, "r%2d ", RW_MUTEX_HASH(lp));
	(void) sprintf_len(LOCK_NAME_LEN - 4, sp->name + 4,
				name, (u_long)lp);	/* Handle %x */
	sp->lkinfo.lk_name = sp->name;
	sp->lkstat = lkstat_alloc(&sp->lkinfo, KM_SLEEP);
	sp->real.un.rw.waiters = 0;
	rw_init((krwlock_t *)&sp->real, name, RW_SLEEP, NULL);
}

static void
rw_stat_enter(krwlock_t *rwlp, krw_t rw)
{
	register rwlock_impl_t *lp = (rwlock_impl_t *)rwlp;
	hrtime_t	start_time, end_time, wait_time;
	int waited = 0;		/* had to wait for lock */
	register struct rwlock_stats *sp = lp->un.sp;
	register rwlock_impl_t *rlp = &sp->real;
	lkstat_t *statsp = sp->lkstat;
	register kmutex_t	*mp;
	register turnstile_t	*ts;

	start_time = gethrtime();
	mp = RW_MUTEX(lp);
	mutex_enter(mp);

	switch (rw) {

	case RW_READER:
		while (rlp->un.rw.holdcnt < 0 || lp->writewanted) {
			mutex_exit(mp);
			if (panicstr) {
				panic_hook();
				return;
			}
			waited = 1;
			CPU_STAT_ADDQ(CPU, cpu_sysinfo.rw_rdfails, 1);
			/*
			 * block on readers' sleep queue
			 */
			disp_lock_enter(&lp->rw_wlock);
			if (rlp->un.rw.waiters == 0) {
				ts = tstile_alloc();
				rlp->un.rw.waiters = ts->ts_id;
#ifdef	MP
				lock_mutex_flush(); /* flush waiters field */
#endif	/* MP */
			} else {
				ts = tstile_pointer(rlp->un.rw.waiters);
			}
			if (rlp->un.rw.holdcnt < 0 || lp->writewanted) {
				thread_lock_high(curthread);
				t_block(ts, (caddr_t)lp, &reader_sops,
					&lp->rw_wlock);
				pi_willto(curthread);	/* drops thread lock */
				swtch();
			} else {
				/*
				 * If we allocated a turnstile above, but
				 * are not going to wait, there will be an
				 * empty turnstile for someone to handle in
				 * rw_exit().
				 */
				disp_lock_exit(&lp->rw_wlock);
			}
			mutex_enter(mp);
		}
		THREAD_KPRI_REQUEST();
		rlp->un.rw.holdcnt++;
		if (lp->owner == NULL)
			lp->owner = curthread;
		if (rlp->un.rw.holdcnt == 1) {
			/* time set on first read acquisition only */
			statsp->ls_solordcnt++;
			end_time = gethrtime();
			statsp->ls_stime = *(dl_t *)&end_time;
		} else if (waited) {
			end_time = gethrtime();
		}
		statsp->ls_rdcnt++;
		break;

	case RW_WRITER:
		while (rlp->un.rw.holdcnt != 0) {
			lp->writewanted++;
			mutex_exit(mp);

			if (panicstr) {
				panic_hook();
				return;
			}
			waited = 1;
			CPU_STAT_ADDQ(CPU, cpu_sysinfo.rw_wrfails, 1);
			/*
			 * Block on writer's sleep queue
			 */
			disp_lock_enter(&lp->rw_wlock);
			if (rlp->un.rw.waiters == 0) {
				ts = tstile_alloc();
				rlp->un.rw.waiters = ts->ts_id;
#ifdef	MP
				lock_mutex_flush(); /* flush waiters field */
#endif	/* MP */
			} else {
				ts = tstile_pointer(rlp->un.rw.waiters);
			}
			/*
			 * Reverify that the lock is held now that we're ready
			 * to block.
			 */
			if (rlp->un.rw.holdcnt != 0) {
				thread_lock_high(curthread);
				t_block(ts, (caddr_t)lp, &writer_sops,
					&lp->rw_wlock);
				/*
				 * t_block dropped the old thread lock
				 * and associated the thread with the
				 * rw_wlock.  pi_willto drops that.
				 */
				pi_willto(curthread);	/* drops thread lock */
				swtch();
			} else {
				/*
				 * If we allocated a turnstile above, but
				 * are not going to wait, there will be an
				 * empty turnstile for someone to handle in
				 * rw_exit().
				 */
				disp_lock_exit(&lp->rw_wlock);
			}
			mutex_enter(mp);
			lp->writewanted--;
		}
		rlp->un.rw.holdcnt = -1;
		lp->owner = curthread;
		statsp->ls_wrcnt++;
		end_time = gethrtime();
		statsp->ls_stime = *(dl_t *)&end_time;
		break;

	default:
		cmn_err(CE_PANIC,
			"rw_enter: bad rw %d, rwlp = 0x%x", rw, (int)lp);
		break;
	}

	if (waited) {
		start_time = end_time - start_time;
		*(dl_t *)&wait_time = statsp->ls_wtime;
		wait_time += start_time;
		statsp->ls_wtime = *(dl_t *)&wait_time;
		statsp->ls_fail++;
	}
	mutex_exit(mp);
}

static int
rw_stat_tryenter(krwlock_t *rwlp, krw_t rw)
{
	register rwlock_impl_t *lp = (rwlock_impl_t *)rwlp;
	int rc = 0;
	lkstat_t *statsp = lp->un.sp->lkstat;
	register rwlock_impl_t *rlp = &lp->un.sp->real;
	register kmutex_t *mp;
	hrtime_t	t1;

	mp = RW_MUTEX(lp);
	mutex_enter(mp);

	switch (rw) {

	case RW_READER:
		if (rlp->un.rw.holdcnt >= 0 && !(lp->writewanted)) {
			THREAD_KPRI_REQUEST();
			rlp->un.rw.holdcnt++;
			if (lp->owner == NULL)
				lp->owner = curthread;
			if (rlp->un.rw.holdcnt == 1) {
				/* stime set at first acquisition only */
				statsp->ls_solordcnt++;
				t1 = gethrtime();
				statsp->ls_stime = *(dl_t *)&t1;
			}
			statsp->ls_rdcnt++;
			rc = 1;
		}
		break;

	case RW_WRITER:
		if (rlp->un.rw.holdcnt == 0) {
			rlp->un.rw.holdcnt = -1;
			statsp->ls_wrcnt++;
			lp->owner = curthread;
			statsp->ls_stime = *(dl_t *)&hrestime;
			rc = 1;
		}
		break;

	default:
		cmn_err(CE_PANIC,
			"rw_tryenter: bad rw %d, rwlp = 0x%x", rw, (int)lp);
		break;
	}

	mutex_exit(mp);
	return (rc);
}

static void
rw_stat_downgrade(krwlock_t *rwlp)
{
	register rwlock_impl_t *lp = (rwlock_impl_t *)rwlp;
	register rwlock_impl_t *rlp = &lp->un.sp->real;
	lkstat_t *statsp = lp->un.sp->lkstat;
	register kmutex_t *mp;

	mp = RW_MUTEX(lp);
	mutex_enter(mp);

	ASSERT(lp->un.sp->real.un.rw.holdcnt < 0);

	THREAD_KPRI_REQUEST();
	rlp->un.rw.holdcnt = 1;

	/*
	 * Statistics times do not need to be updated here, since we're
	 * not dropping the lock.  The hold period started when we first
	 * got the lock.
	 */
	statsp->ls_solordcnt++;

	/*
	 * if no waiting writers then wakeup readers
	 */
	if (!(lp->writewanted)) {
		register turnstile_t	*ts;

		mutex_exit(mp);
		if (rlp->un.rw.waiters) {
			disp_lock_enter(&lp->rw_wlock);
			ts = tstile_pointer(rlp->un.rw.waiters);
			pi_waive(ts);
			t_release_all(ts, &rlp->un.rw.waiters, QOBJ_READER);
			disp_lock_exit(&lp->rw_wlock);
		}
	} else {
		mutex_exit(mp);
	}
}

static int
rw_stat_tryupgrade(krwlock_t *rwlp)
{
	register rwlock_impl_t *lp = (rwlock_impl_t *)rwlp;
	register int rc = 0;
	register rwlock_impl_t *rlp = &lp->un.sp->real;
	lkstat_t *statsp = lp->un.sp->lkstat;
	register kmutex_t *mp;

	mp = RW_MUTEX(lp);
	mutex_enter(mp);

	ASSERT(rlp->un.rw.holdcnt >= 1);

	/*
	 * Statistics times do not need to be updated here, since we're
	 * not dropping the lock.  The hold period started when we first
	 * got the lock.
	 */
	if (lp->writewanted == 0 && rlp->un.rw.holdcnt == 1) {
		lp->owner = curthread;
		rlp->un.rw.holdcnt = -1;
		statsp->ls_wrcnt++;
		rc = 1;
		THREAD_KPRI_RELEASE();
	}
	mutex_exit(mp);
	return (rc);
}

static void
rw_stat_exit(krwlock_t *rwlp)
{
	register rwlock_impl_t *lp = (rwlock_impl_t *)rwlp;
	hrtime_t	hold_time, now, start_time;
	lkstat_t	*statsp = lp->un.sp->lkstat;
	rwlock_impl_t	*rlp = &lp->un.sp->real;
	register kmutex_t *mp;

	mp = RW_MUTEX(lp);
	mutex_enter(mp);

	ASSERT(rlp->un.rw.holdcnt != 0);
	if (rlp->un.rw.holdcnt < 0) {		/* if this is a write lock */
		lp->owner = NULL;
		rlp->un.rw.holdcnt = 0;

		/*
		 * Add holding period to total hold time.
		 */
		now = gethrtime();
		*(dl_t *)&start_time = statsp->ls_stime;
		hold_time = now - start_time;
		*(dl_t *)&start_time = statsp->ls_htime;
		hold_time += start_time;
		statsp->ls_htime = *(dl_t *)&hold_time;
		ASSERT(lp->writewanted >= 0);	/* not neg */
		mutex_exit(mp);

		if (rlp->un.rw.waiters) {
			register turnstile_t	*ts;

			disp_lock_enter(&lp->rw_wlock);
			ts = tstile_pointer(rlp->un.rw.waiters);
			pi_waive(ts);
			if (lp->writewanted > 0) {
				t_release(ts, &rlp->un.rw.waiters, QOBJ_WRITER);
			} else {				/* zero */
				t_release_all(ts, &rlp->un.rw.waiters,
					QOBJ_READER);
			}
			disp_lock_exit(&lp->rw_wlock);
		}
	} else {
		register turnstile_t	*ts;

		/*
		 * read exit
		 */
		/*
		 * If no other readers, add holding period to total hold time,
		 * and wake up any waiting writers.
		 */
		if (--(rlp->un.rw.holdcnt) == 0) {
			now = gethrtime();
			*(dl_t *)&start_time = statsp->ls_stime;
			hold_time = now - start_time;
			*(dl_t *)&start_time = statsp->ls_htime;
			hold_time += start_time;
			statsp->ls_htime = *(dl_t *)&hold_time;
			/*
			 * Check for waiting writers.
			 */
			if (lp->writewanted) {
				mutex_exit(mp);
				disp_lock_enter(&lp->rw_wlock);
				ts = tstile_pointer(rlp->un.rw.waiters);
				t_release(ts, &rlp->un.rw.waiters, QOBJ_WRITER);
				if (lp->owner == curthread) {
					lp->owner = NULL;
					pi_waive(ts);
				}
				disp_lock_exit(&lp->rw_wlock);
			} else if (lp->owner == curthread) {
				lp->owner = NULL;
				mutex_exit(mp);
				disp_lock_enter(&lp->rw_wlock);
				ts = tstile_pointer(rlp->un.rw.waiters);
				pi_waive(ts);
				disp_lock_exit(&lp->rw_wlock);
			} else {
				mutex_exit(mp);
			}
		} else if (lp->owner == curthread) {
			/*
			 * If this reader was the "owner of record,"
			 * clear the owner field.
			 * Release owner and inheritance.
			 */
			lp->owner = NULL;
			mutex_exit(mp);
			if (rlp->un.rw.waiters) {
				disp_lock_enter(&lp->rw_wlock);
				ts = tstile_pointer(rlp->un.rw.waiters);
				pi_waive(ts);
				disp_lock_exit(&lp->rw_wlock);
			}
		} else {
			mutex_exit(mp);
		}
		THREAD_KPRI_RELEASE();
	}
}

/*
 * Free resources associated with stats lock (stats).
 */
static void
rw_stat_destroy(krwlock_t *rwlp)
{
	register rwlock_impl_t *lp = (rwlock_impl_t *)rwlp;

	lkstat_sum_on_destroy(lp->un.sp->lkstat);
	lkstat_free(lp->un.sp->lkstat, TRUE);
	rw_sleep_destroy((krwlock_t *)&lp->un.sp->real);
	rwlock_stats_free(lp->un.sp);
	lp->un.sp = NULL;
}

/*
 * Return 1 if the lock is a readers lock.
 * Used in page_lock.c
 * Caller must hold lock for either read or write.
 */
int
rw_read_locked(krwlock_t *rwlp)
{
	register rwlock_impl_t *lp = (rwlock_impl_t *)rwlp;
	int ret = 0;
	register kmutex_t *mp;
	register int holdcnt;

	if (panicstr)
		return (1);		/* can't tell if panicing */

	mp = RW_MUTEX(lp);
	mutex_enter(mp);

	switch (RWLOCK_TYPE(lp->type)) {
	case RW_SLEEP:
		holdcnt = lp->un.rw.holdcnt;
		break;

	case RW_SLEEP_STAT:
		holdcnt = lp->un.sp->real.un.rw.holdcnt;
		break;

	default:
		holdcnt = 0;
		break;
	}
	ASSERT(holdcnt != 0);
	if (holdcnt > 0) {
		ret = 1;
	}
	mutex_exit(mp);
	return (ret);
}


/*
 * rw_read_held() - return nonzero if the lock is held (by someone) for read.
 *
 * For asserts.
 *
 * This is different from rw_read_locked, because that function ASSERTs that
 * the lock must be held for write if not for read.
 *
 * The associated mutex is not needed, since the lock will not go from read
 * to write or unheld without action by this thread (which should hold it).
 */
int
rw_read_held(krwlock_t *rwlp)
{
	register rwlock_impl_t *lp = (rwlock_impl_t *)rwlp;
	int ret = 0;

	if (panicstr)
		return (1);

	switch (RWLOCK_TYPE(lp->type)) {
	case RW_SLEEP:
		if (lp->un.rw.holdcnt > 0)
			ret = 1;
		break;

	case RW_SLEEP_STAT:
		if (lp->un.sp->real.un.rw.holdcnt > 0)
			ret = 1;
		break;

	default:
		break;
	}
	return (ret);
}


/*
 * rw_write_held() - return nonzero if the lock is held (by someone) for write.
 *
 * For asserts.
 *
 * The associated mutex is not needed, since the lock will not go from read
 * to write or unheld without action by this thread (which should hold it).
 */
int
rw_write_held(krwlock_t *rwlp)
{
	register rwlock_impl_t *lp = (rwlock_impl_t *)rwlp;
	int ret = 0;

	if (panicstr)
		return (1);

	switch (RWLOCK_TYPE(lp->type)) {
	case RW_SLEEP:
		if (lp->un.rw.holdcnt < 0)
			ret = 1;
		break;

	case RW_SLEEP_STAT:
		if (lp->un.sp->real.un.rw.holdcnt < 0)
			ret = 1;
		break;

	default:
		break;
	}
	return (ret);
}

int
rw_iswriter(krwlock_t *rwlp)
{
	register rwlock_impl_t *lp = (rwlock_impl_t *)rwlp;

	if (panicstr)
		return (1);
	if (RWLOCK_TYPE(lp->type) == RW_SLEEP_STAT) {
		lp = &lp->un.sp->real;
	}

	return (lp->un.rw.holdcnt < 0 || lp->writewanted);
}


int
rw_lock_held(krwlock_t *rwlp)
{
	register rwlock_impl_t *lp = (rwlock_impl_t *)rwlp;
	int ret = 0;

	if (panicstr)
		return (1);

	switch (RWLOCK_TYPE(lp->type)) {
	case RW_SLEEP:
		ret = (lp->un.rw.holdcnt != 0);
		break;

	case RW_SLEEP_STAT:
		ret = (lp->un.sp->real.un.rw.holdcnt != 0);
		break;

	default:
		break;
	}
	return (ret);
}

/*
 * Return a pointer to the thread that owns this lock, if any.
 */
kthread_t *
rw_owner(krwlock_t *rwlp)
{
	register rwlock_impl_t *lp = (rwlock_impl_t *)rwlp;

	return (lp->owner);
}

/*
 * Unsleep a thread waiting for a readers' lock.
 */
void
reader_unsleep(kthread_t *t)
{
	register turnstile_t	*ts;

	ASSERT(THREAD_LOCK_HELD(t));
	if ((ts = t->t_ts) != NULL) {
		register rwlock_impl_t	*rwlp = (rwlock_impl_t *)t->t_wchan;
		register disp_lock_t	*lp;

		lp = t->t_lockp;
		/*
		 * When we return from TSTILE_UNSLEEP(), the
		 * thread will be in transition, but we'll
		 * still hold the lock on the synch object.
		 */
		if (TSTILE_UNSLEEP(ts, QOBJ_READER, t) != NULL) {
			tstile_free(ts, &rwlp->un.rw.waiters);
			disp_lock_exit_high(lp);
			CL_SETRUN(t);
			return;
		}
	}
	cmn_err(CE_PANIC, "reader_unsleep: 0x%x not in turnstile", (int)t);
}

/*
 * Unsleep a thread waiting for a writer lock.
 */
void
writer_unsleep(kthread_t *t)
{
	register turnstile_t	*ts;

	ASSERT(THREAD_LOCK_HELD(t));
	if ((ts = t->t_ts) != NULL) {
		register rwlock_impl_t	*rwlp = (rwlock_impl_t *)t->t_wchan;
		register disp_lock_t	*lp;

		lp = t->t_lockp;
		/*
		 * When we return from TSTILE_UNSLEEP(), the
		 * thread will be in transition, but we'll
		 * still hold the lock on the synch object.
		 */
		if (TSTILE_UNSLEEP(ts, QOBJ_WRITER, t) != NULL) {
			tstile_free(ts, &rwlp->un.rw.waiters);
			disp_lock_exit_high(lp);
			CL_SETRUN(t);
			return;
		}
	}
	cmn_err(CE_PANIC, "writer_unsleep: 0x%x not in turnstile", (int)t);
}

/*
 * Change the priority of a thread sleeping on
 * an rwlock. This requires that we:
 *	o dequeue the thread from its turnstile.
 *	o change the thread's priority.
 *	o re-enqueue the thread.
 * Called by: pi_changepri() via the SOBJ_CHANGEPRI() macro.
 * Conditions: the argument thread must be locked.
 * Side Effects: None.
 */

/*
 * For threads queued waiting for a readers' lock.
 */
static void
reader_changepri(kthread_t *t, pri_t pri)
{
	turnstile_t	*ts;

	ASSERT(THREAD_LOCK_HELD(t));
	if ((ts = t->t_ts) != NULL) {
		(void) TSTILE_DEQ(ts, QOBJ_READER, t);
		t->t_epri = pri;
		TSTILE_INSERT(ts, QOBJ_READER, t);
	} else {
		cmn_err(CE_PANIC,
			"reader_changepri: 0x%x not in turnstile", (int)t);
	}
}

/*
 * For threads queued waiting for a writer lock.
 */
static void
writer_changepri(kthread_t *t, pri_t pri)
{
	turnstile_t	*ts;

	ASSERT(THREAD_LOCK_HELD(t));
	if ((ts = t->t_ts) != NULL) {
		(void) TSTILE_DEQ(ts, QOBJ_WRITER, t);
		t->t_epri = pri;
		TSTILE_INSERT(ts, QOBJ_WRITER, t);
	} else {
		cmn_err(CE_PANIC,
			"writer_changepri: 0x%x not in turnstile", (int)t);
	}
}


/* ARGSUSED */
static void
rw_driver_init(krwlock_t *rwlp, char *name, krw_type_t type, void *arg)
{
	if (lock_stats) {
		type = RW_SLEEP_STAT;
	} else {
		type = RW_SLEEP;
	}
	rw_init(rwlp, name, type, NULL);
}

/* ARGSUSED */
static void
rw_driver_stat_init(krwlock_t *rwlp, char *name, krw_type_t type, void *arg)
{
	rw_init(rwlp, name, RW_SLEEP_STAT, NULL);
}

static void * rwlock_stats_header;

static struct rwlock_stats *
rwlock_stats_alloc(int sleep)
{
	register struct rwlock_stats *sp;

	if (kmem_ready) {
		sp = (struct rwlock_stats *)
		    kmem_zalloc(sizeof (struct rwlock_stats), sleep);
	} else {
		sp = (struct rwlock_stats *)
		    startup_alloc(sizeof (struct rwlock_stats),
			&rwlock_stats_header);
		sp->flag = RWSTAT_STARTUP_ALLOC;
	}
	if (sp == NULL)
		panic("rwlock_stats_alloc: can't alloc mutex_stats");
	return (sp);
}

static void
rwlock_stats_free(struct rwlock_stats *sp)
{
	if (sp->flag & RWSTAT_STARTUP_ALLOC) {
		startup_free((void *)sp, sizeof (*sp), &rwlock_stats_header);
	} else {
		kmem_free((caddr_t)sp, sizeof (*sp));
	}
}
