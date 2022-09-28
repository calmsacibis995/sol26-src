/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)pset.c	1.3	96/07/16 SMI"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/cpuvar.h>
#include <sys/thread.h>
#include <sys/disp.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/sysmacros.h>
#include <sys/cpupart.h>
#include <sys/pset.h>
#include <sys/modctl.h>
#include <sys/syscall.h>

static int	pset(int, long, long, long, long);

static struct sysent pset_sysent = {
	6,
	SE_ARGC | SE_NOUNLOAD,
	(int (*)())pset,
};

static struct modlsys modlsys = {
	&mod_syscallops, "processor sets", &pset_sysent
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlsys, NULL
};

static int	pset_create(psetid_t *);
static int	pset_destroy(psetid_t);
static int	pset_assign(psetid_t, processorid_t, psetid_t *);
static int	pset_info(psetid_t, int *, u_int *, processorid_t *);
static int	pset_bind(psetid_t, idtype_t, id_t, psetid_t *);

static int	pset_dobind(kthread_t *, psetid_t, psetid_t *);

/*
 * pset_lock preserves the atomicity of multiple calls to cpupart
 * functions within the same system call.
 */
static kmutex_t	pset_lock;

_init()
{
	mutex_init(&pset_lock, "pset lock", MUTEX_DEFAULT, NULL);
	return (mod_install(&modlinkage));
}

_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
pset(int subcode, long arg1, long arg2, long arg3, long arg4)
{
	switch (subcode) {
	case PSET_CREATE:
		return (pset_create((psetid_t *)arg1));
	case PSET_DESTROY:
		return (pset_destroy((psetid_t)arg1));
	case PSET_ASSIGN:
		return (pset_assign((psetid_t)arg1,
		    (processorid_t)arg2, (psetid_t *)arg3));
	case PSET_INFO:
		return (pset_info((psetid_t)arg1, (int *)arg2,
		    (u_int *)arg3, (processorid_t *)arg4));
	case PSET_BIND:
		return (pset_bind((psetid_t)arg1, (idtype_t)arg2,
		    (id_t)arg3, (psetid_t *)arg4));
	default:
		return (set_errno(EINVAL));
	}
}

static int
pset_create(psetid_t *psetp)
{
	psetid_t newpset;
	int error;

	if (!suser(CRED()))
		return (set_errno(EPERM));

	mutex_enter(&pset_lock);
	error = cpupart_create((cpupartid_t *)(&newpset), CP_PRIVATE);
	if (error) {
		mutex_exit(&pset_lock);
		return (set_errno(error));
	}
	if (copyout((caddr_t)&newpset, (caddr_t)psetp, sizeof (psetid_t))
	    != 0) {
		(void) cpupart_destroy((cpupartid_t)newpset);
		mutex_exit(&pset_lock);
		return (set_errno(EFAULT));
	}
	mutex_exit(&pset_lock);
	return (0);
}

static int
pset_destroy(psetid_t pset)
{
	int error;
	u_int level;

	if (!suser(CRED()))
		return (set_errno(EPERM));

	mutex_enter(&pset_lock);
	if (cpupart_get_level((cpupartid_t)pset, &level) != 0) {
		mutex_exit(&pset_lock);
		return (set_errno(EINVAL));
	}
	if (level != CP_PRIVATE) {
		mutex_exit(&pset_lock);
		if (level == CP_SYSTEM) {
			/*
			 * Can't destroy system partitions from user level.
			 */
			return (set_errno(EPERM));
		} else {
			/*
			 * Other types of partitions aren't visible through
			 * the processor set interface.
			 */
			return (set_errno(EINVAL));
		}
	}
	error = cpupart_destroy((cpupartid_t)pset);
	mutex_exit(&pset_lock);
	if (error)
		return (set_errno(error));
	else
		return (0);
}

static int
pset_assign(psetid_t pset, processorid_t cpuid, psetid_t *opset)
{
	psetid_t oldpset;
	cpupartid_t oldpart;
	u_int	level;
	int	error = 0;
	cpu_t	*cp;

	if (!suser(CRED()) && pset != PS_QUERY)
		return (set_errno(EPERM));

	if ((cp = cpu_get(cpuid)) == NULL)
		return (set_errno(EINVAL));

	mutex_enter(&pset_lock);
	if (pset != PS_QUERY && pset != PS_NONE) {
		if (cpupart_get_level((cpupartid_t)pset, &level) != 0) {
			mutex_exit(&pset_lock);
			return (set_errno(EINVAL));
		}
		if (level != CP_PRIVATE) {
			mutex_exit(&pset_lock);
			if (level == CP_SYSTEM) {
				/*
				 * Can't modify system partitions from
				 * user level.
				 */
				return (set_errno(EPERM));
			} else {
				/*
				 * Other types of partitions aren't visible
				 * through the processor set interface.
				 */
				return (set_errno(EINVAL));
			}
		}
	}
	oldpart = cpupart_query_cpu(cp);
	error = cpupart_get_level(oldpart, &level);
	ASSERT(error == 0);
	if (level != CP_SYSTEM && level != CP_PRIVATE)
		oldpset = PS_NONE;
	else
		oldpset = (psetid_t)oldpart;
	if (pset != PS_QUERY)
		error = cpupart_attach_cpu((cpupartid_t)pset, cp);
	mutex_exit(&pset_lock);

	if (error)
		return (set_errno(error));

	if (opset != NULL)
		if (copyout((caddr_t)(&oldpset), (caddr_t)opset,
			    sizeof (psetid_t)) != 0)
			return (set_errno(EFAULT));

	return (0);
}

static int
pset_info(psetid_t pset, int *typep, u_int *numcpusp,
    processorid_t *cpulistp)
{
	u_int pset_level, cp_level;
	u_int user_ncpus = 0, real_ncpus, copy_ncpus;
	processorid_t *pset_cpus = NULL;
	int error = 0;

	if (numcpusp != NULL) {
		if (copyin(numcpusp, &user_ncpus, sizeof (u_int)) != 0)
			return (set_errno(EFAULT));
	}

	if (user_ncpus > ncpus)			/* sanity check */
		return (set_errno(EINVAL));

	if (user_ncpus != 0 && cpulistp != NULL)
		pset_cpus = kmem_alloc(sizeof (processorid_t) * user_ncpus,
		    KM_SLEEP);

	mutex_enter(&pset_lock);
	if (cpupart_get_level((cpupartid_t)pset, &cp_level) != 0) {
		error = EINVAL;
		goto out;
	}
	if (cp_level == CP_SYSTEM)
		pset_level = PS_SYSTEM;
	else if (cp_level == CP_PRIVATE)
		pset_level = PS_PRIVATE;
	else {
		/*
		 * "system" and "private" partitions are the only ones
		 * visible through the processor set interface.
		 */
		error = EINVAL;
		goto out;
	}

	real_ncpus = user_ncpus;
	if (numcpusp != NULL &&
	    (error = cpupart_get_cpus((cpupartid_t)pset, pset_cpus,
					&real_ncpus)) != 0) {
		goto out;
	}

	/*
	 * Now copyout the information about this processor set.
	 */
	mutex_exit(&pset_lock);
	/*
	 * Get number of cpus to copy back.  If the user didn't pass in
	 * a big enough buffer, only copy back as many cpus as fits in
	 * the buffer but copy back the real number of cpus.
	 */

	if (user_ncpus != 0 && cpulistp != NULL) {
		copy_ncpus = MIN(real_ncpus, user_ncpus);
		if (copyout(pset_cpus, cpulistp,
		    sizeof (processorid_t) * copy_ncpus) != 0) {
			kmem_free(pset_cpus,
			    sizeof (processorid_t) * user_ncpus);
			return (set_errno(EFAULT));
		}
	}
	if (pset_cpus != NULL)
		kmem_free(pset_cpus, sizeof (processorid_t) * user_ncpus);
	if (typep != NULL)
		if (copyout(&pset_level, typep, sizeof (int)) != 0)
			return (set_errno(EFAULT));
	if (numcpusp != NULL)
		if (copyout(&real_ncpus, numcpusp, sizeof (u_int)) != 0)
			return (set_errno(EFAULT));
	return (0);

out:
	mutex_exit(&pset_lock);
	if (pset_cpus != NULL)
		kmem_free(pset_cpus, sizeof (processorid_t) * user_ncpus);
	return (set_errno(error));
}

static int
pset_bind(psetid_t pset, idtype_t idtype, id_t id, psetid_t *opset)
{
	proc_t		*pp;
	kthread_t	*tp;
	psetid_t	oldpset;
	u_int		level;
	int		error = 0;

	mutex_enter(&pset_lock);
	if (pset != PS_NONE && pset != PS_QUERY) {
		if (cpupart_get_level((cpupartid_t)pset, &level) != 0) {
			error = EINVAL;
			goto out;
		}
		if (level != CP_SYSTEM && level != CP_PRIVATE) {
			error = EINVAL;
			goto out;
		}
		if (level == CP_PRIVATE && !suser(CRED())) {
			error = EPERM;
			goto out;
		}
	}

	mutex_enter(&cp_list_lock);
	switch (idtype) {
	case P_LWPID:
		pp = curproc;
		mutex_enter(&pp->p_lock);
		if (id == P_MYID) {
			tp = curthread;
		} else {
			tp = pp->p_tlist;
			do {
				if (tp->t_tid == id)
					break;
			} while ((tp = tp->t_forw) != pp->p_tlist);

			if (tp->t_tid != id) {
				mutex_exit(&pp->p_lock);
				mutex_exit(&cp_list_lock);
				error = ESRCH;
				goto out;
			}
		}
		error = pset_dobind(tp, pset, &oldpset);
		mutex_exit(&pp->p_lock);
		break;

	case P_PID:
		mutex_enter(&pidlock);
		if (id == P_MYID) {
			pp = curproc;
		} else {
			if ((pp = prfind(id)) == NULL) {
				mutex_exit(&pidlock);
				mutex_exit(&cp_list_lock);
				error = ESRCH;
				goto out;
			}
		}
		mutex_enter(&pp->p_lock);
		tp = pp->p_tlist;
		if (tp != NULL) {
			do {
				error = pset_dobind(tp, pset, &oldpset);
			} while ((tp = tp->t_forw) != pp->p_tlist);
		} else
			error = ESRCH;
		mutex_exit(&pp->p_lock);
		mutex_exit(&pidlock);
		break;

	default:
		error = EINVAL;
		break;
	}
	mutex_exit(&cp_list_lock);

out:
	mutex_exit(&pset_lock);
	if (error != 0)
		return (set_errno(error));
	if (opset != NULL) {
		if (copyout(&oldpset, opset, sizeof (psetid_t)) != 0)
			return (set_errno(EFAULT));
	}
	return (0);
}


static int
pset_dobind(kthread_t *tp, psetid_t pset, psetid_t *oldpset)
{
	int error = 0;

	ASSERT(MUTEX_HELD(&pset_lock));
	ASSERT(MUTEX_HELD(&cp_list_lock));
	ASSERT(MUTEX_HELD(&ttoproc(tp)->p_lock));

	if (!hasprocperm(tp->t_cred, CRED()) && pset != PS_QUERY)
		return (EPERM);
	*oldpset = tp->t_bind_pset;
	if (pset == PS_NONE)
		error = cpupart_bind_thread(tp, CP_NONE);
	else if (pset != PS_QUERY)
		error = cpupart_bind_thread(tp, (cpupartid_t)pset);
	if (error == 0 && pset != PS_QUERY)
		tp->t_bind_pset = pset;
	return (error);
}
