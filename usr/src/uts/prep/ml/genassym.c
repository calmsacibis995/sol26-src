/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */

#pragma ident "@(#)genassym.c	1.72	96/10/17 SMI"

#ifndef _GENASSYM
#define	_GENASSYM
#endif

#define	SIZES	1

#include <sys/types.h>
#include <sys/param.h>
#include <sys/elf_notes.h>
#include <sys/sysinfo.h>
#include <sys/vmmeter.h>
#include <sys/vmparam.h>
#include <sys/signal.h>
#include <sys/user.h>
#include <sys/map.h>
#include <sys/thread.h>
#include <sys/t_lock.h>
#include <sys/mutex_impl.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/lwp.h>
#include <sys/rt.h>
#include <sys/ts.h>
#include <sys/msgbuf.h>
#include <sys/vmmac.h>
#include <sys/cpuvar.h>
#include <sys/dditypes.h>
#include <sys/vtrace.h>

#include <sys/trap.h>
#include <sys/stack.h>
#include <sys/psw.h>
#include <sys/mmu.h>
#include <sys/cpu.h>
#include <sys/clock.h>

#include <sys/devops.h>
#include <sys/ddi_impldefs.h>

#include <vm/hat_ppcmmu.h>
#include <vm/as.h>
#include <vm/seg.h>

#include <sys/avintr.h>
#include <sys/pic.h>
#include <sys/pit.h>

#include <sys/stream.h>
#include <sys/strsubr.h>

#define	OFFSET(type, field)	((int)(&((type *)0)->field))


main()
{
	register struct dev_info *devi = (struct dev_info *)0;
	register struct dev_ops *ops = (struct dev_ops *)0;
	register struct bus_ops *busops = (struct bus_ops *)0;
	register struct proc *p = (struct proc *)0;
	register kthread_id_t tid = (kthread_id_t)0;
	register klwp_id_t lwpid = (klwp_id_t)0;
	register struct user *up = (struct user *)0;
	register struct regs *rp = (struct regs *)0;
	register struct as *as = (struct as *)0;
	register struct hat *hat = (struct hat *)0;
	register struct fpu *fp = (struct fpu *)0;
	register struct autovec *av = (struct autovec *)0;
	register struct av_head *avh = (struct av_head *)0;
	register struct timestruc *tms = (struct timestruc *) 0;
	register struct cpu *cpu = (struct cpu *)0;
	register struct standard_pic *sc = (struct standard_pic *)0;
	register struct ctxop *ctxop = (struct ctxop *)0;

	printf("#define\tSYSBASE %d\n", SYSBASE);
	printf("#define\tSYSLIMIT %d\n", SYSLIMIT);
	printf("#define\tPCIISA_VBASE %d\n", PCIISA_VBASE);

	printf("#define\tPAGESIZE %d\n", PAGESIZE);

	printf("#define\tCPU_ARCH %d\n", CPU_ARCH);
	printf("#define\tCPU_601 %d\n", CPU_601);
	printf("#define\tCPU_603 %d\n", CPU_603);
	printf("#define\tCPU_604 %d\n", CPU_604);
	printf("#define\tCPU_620 %d\n",  CPU_620);

	printf("#define\tT_AST 0x%x\n", T_AST);

	printf("#define\tLOCK_LEVEL 0x%x\n", LOCK_LEVEL);
	printf("#define\tCLOCK_LEVEL 0x%x\n", CLOCK_LEVEL);

	printf("#define\tPIC_NSEOI 0x%x\n", PIC_NSEOI);

	printf("#define\tP_LINK 0x%x\n", &p->p_link);
	printf("#define\tP_NEXT 0x%x\n", &p->p_next);
	printf("#define\tP_CHILD 0x%x\n", &p->p_child);
	printf("#define\tP_SIBLING 0x%x\n", &p->p_sibling);
	printf("#define\tP_SIG 0x%x\n", &p->p_sig);
	printf("#define\tP_WCODE 0x%x\n", &p->p_wcode);
	printf("#define\tP_FLAG 0x%x\n", &p->p_flag);
	printf("#define\tP_TLIST 0x%x\n", &p->p_tlist);
	printf("#define\tP_AS 0x%x\n", &p->p_as);
	printf("#define\tP_FLAG 0x%x\n", &p->p_flag);
	printf("#define\tP_LOCKP 0x%x\n", &p->p_lockp);
	printf("#define\tSLOAD 0x%x\n", SLOAD);
	printf("#define\tSSLEEP 0x%x\n", SSLEEP);
	printf("#define\tSRUN 0x%x\n", SRUN);
	printf("#define\tSONPROC 0x%x\n", SONPROC);

	printf("#define\tT_PC 0x%x\n", &tid->t_pc);
	printf("#define\tT_SP 0x%x\n", &tid->t_sp);
	printf("#define\tT_LOCK 0x%x\n", &tid->t_lock);
	printf("#define\tT_LOCKP 0x%x\n", &tid->t_lockp);
	printf("#define\tT_OLDSPL 0x%x\n", &tid->t_oldspl);
	printf("#define\tT_PRI 0x%x\n", &tid->t_pri);
	printf("#define\tT_LWP 0x%x\n", &tid->t_lwp);
	printf("#define\tT_PROCP 0x%x\n", &tid->t_procp);
	printf("#define\tT_LINK 0x%x\n", &tid->t_link);
	printf("#define\tT_STATE 0x%x\n", &tid->t_state);
	printf("#define\tT_STACK 0x%x\n", &tid->t_stk);
	printf("#define\tT_SWAP 0x%x\n", &tid->t_swap);
	printf("#define\tT_WCHAN 0x%x\n", &tid->t_wchan);
	printf("#define\tT_FLAGS 0x%x\n", &tid->t_flag);
	printf("#define\tT_CTX 0x%x\n", &tid->t_ctx);
	printf("#define\tT_LOFAULT 0x%x\n", &tid->t_lofault);
	printf("#define\tT_ONFAULT 0x%x\n", &tid->t_onfault);
	printf("#define\tT_CPU 0x%x\n", &tid->t_cpu);
	printf("#define\tT_BOUND_CPU 0x%x\n", &tid->t_bound_cpu);
	printf("#define\tT_INTR 0x%x\n", &tid->t_intr);
	printf("#define\tT_FORW 0x%x\n", &tid->t_forw);
	printf("#define\tT_BACK 0x%x\n", &tid->t_back);
	printf("#define\tT_SIG 0x%x\n", &tid->t_sig);
	printf("#define\tT_TID 0x%x\n", &tid->t_tid);
	printf("#define\tT_POST_SYS_AST 0x%x\n", &tid->t_post_sys_ast);
	printf("#define\tT_PRE_SYS 0x%x\n", &tid->t_pre_sys);
	printf("#define\tT_PREEMPT 0x%x\n", &tid->t_preempt);
	printf("#define\tT_PROC_FLAG 0x%x\n", &tid->t_proc_flag);
	printf("#define\tT_STARTPC 0x%x\n", &tid->t_startpc);
	printf("#define\tT_SYSNUM 0x%x\n", &tid->t_sysnum);
	printf("#define\tT_ASTFLAG 0x%x\n", &tid->t_astflag);
	printf("#define\tT_INTR_THREAD %d\n", T_INTR_THREAD);
	printf("#define\tCTXOP_SAVE %d\n", &ctxop->save_op);
	printf("#define\tFREE_THREAD 0x%x\n", TS_FREE);
	printf("#define\tTS_FREE 0x%x\n", TS_FREE);
	printf("#define\tTS_ZOMB 0x%x\n", TS_ZOMB);
	printf("#define\tTP_MSACCT 0x%x\n", TP_MSACCT);
	printf("#define\tTP_WATCHPT 0x%x\n", TP_WATCHPT);
	printf("#define\tTP_TWAIT 0x%x\n", TP_TWAIT);
	printf("#define\tONPROC_THREAD 0x%x\n", TS_ONPROC);
	printf("#define\tT0STKSZ 0x%x\n", DEFAULTSTKSZ * 2);
	printf("#define\tTHREAD_SIZE %d\n", sizeof (kthread_t));
	printf("#define\tNSYSCALL %d\n", NSYSCALL);
	printf("#define\tSYSENT_SIZE %d\n", sizeof (struct sysent));
	printf("#define\tSY_CALLC 0x%x\n", OFFSET(struct sysent, sy_callc));

	printf("#define\tA_HAT 0x%x\n", &as->a_hat);
	printf("#define\tHAT_CTX 0x%x\n", &hat->hat_data[0]);

	printf("#define\tMSGBUFSIZE 0x%x\n", sizeof (struct msgbuf));
	printf("#define\tV_MSGBUF_ADDR 0x%x\n", V_MSGBUF_ADDR);

	printf("#define\tS_READ 0x%x\n", (int)S_READ);
	printf("#define\tS_WRITE 0x%x\n", (int)S_WRITE);
	printf("#define\tS_EXEC 0x%x\n", (int)S_EXEC);
	printf("#define\tS_OTHER 0x%x\n", (int)S_OTHER);

	printf("#define\tU_COMM 0x%x\n", up->u_comm);
	printf("#define\tU_SIGNAL 0x%x\n", up->u_signal);
	printf("#define\tUSIZEBYTES 0x%x\n", sizeof (struct user));

	printf("#define\tLWP_THREAD 0x%x\n", &lwpid->lwp_thread);
	printf("#define\tLWP_ERROR 0x%x\n", &lwpid->lwp_error);
	printf("#define\tLWP_EOSYS 0x%x\n", &lwpid->lwp_eosys);
	printf("#define\tLWP_OLDCONTEXT 0x%x\n", &lwpid->lwp_oldcontext);
	printf("#define\tLWP_RU_SYSC 0x%x\n", &lwpid->lwp_ru.sysc);
	printf("#define\tLWP_REGS 0x%x\n", &lwpid->lwp_regs);
	printf("#define\tLWP_QSAV 0x%x\n", &lwpid->lwp_qsav);
	printf("#define\tLWP_ARG 0x%x\n", lwpid->lwp_arg);
	printf("#define\tLWP_AP 0x%x\n", &lwpid->lwp_ap);
	printf("#define\tLWP_CURSIG 0x%x\n", &lwpid->lwp_cursig);
	printf("#define\tLWP_STATE 0x%x\n", &lwpid->lwp_state);
	printf("#define\tLWP_USER 0x%x\n", LWP_USER);
	printf("#define\tLWP_UTIME 0x%x\n", &lwpid->lwp_utime);
	printf("#define\tLWP_SYS 0x%x\n", LWP_SYS);
	printf("#define\tLWP_STATE_START 0x%x\n",
	    &lwpid->lwp_mstate.ms_state_start);
	printf("#define\tLWP_ACCT_USER 0x%x\n",
	    &lwpid->lwp_mstate.ms_acct[LMS_USER]);
	printf("#define\tLWP_ACCT_SYSTEM 0x%x\n",
	    &lwpid->lwp_mstate.ms_acct[LMS_SYSTEM]);
	printf("#define\tLWP_MS_PREV 0x%x\n",
	    &lwpid->lwp_mstate.ms_prev);
	printf("#define\tLWP_MS_START 0x%x\n", &lwpid->lwp_mstate.ms_start);
	printf("#define\tLWP_PCB 0x%x\n", &lwpid->lwp_pcb);
	printf("#define\tLWP_PCB_FLAGS 0x%x\n", &lwpid->lwp_pcb.pcb_flags);
	printf("#define\tLWP_PCB_FPU 0x%x\n", &lwpid->lwp_pcb.pcb_fpu);
	printf("#define\tLMS_USER 0x%x\n", LMS_USER);
	printf("#define\tLMS_SYSTEM 0x%x\n", LMS_SYSTEM);
	printf("#define\tPCBSIZE 0x%x\n", sizeof (struct pcb));
	printf("#define\tREGSIZE %d\n", sizeof (struct regs));

	printf("#define\tELF_NOTE_SOLARIS \"%s\"\n", ELF_NOTE_SOLARIS);
	printf("#define\tELF_NOTE_PAGESIZE_HINT %d\n", ELF_NOTE_PAGESIZE_HINT);

	printf("#define\tAV_VECTOR 0x%x\n", &av->av_vector);
	printf("#define\tAV_INTARG 0x%x\n", &av->av_intarg);
	printf("#define\tAV_MUTEX 0x%x\n", &av->av_mutex);
	printf("#define\tAV_INT_SPURIOUS %d\n", AV_INT_SPURIOUS);
	printf("#define\tAUTOVECSIZE 0x%x\n", sizeof (struct autovec));
	printf("#define\tAV_LINK 0x%x\n", &av->av_link);
	printf("#define\tAV_PRILEVEL 0x%x\n", &av->av_prilevel);

	printf("#define\tAVH_LINK 0x%x\n", &avh->avh_link);
	printf("#define\tAVH_HI_PRI 0x%x\n", &avh->avh_hi_pri);
	printf("#define\tAVH_LO_PRI 0x%x\n", &avh->avh_lo_pri);

	printf("#define\tCPU_CLOCKINTR 0x%x\n",
		OFFSET(struct cpu, cpu_clockintr));
	printf("#define\tCPU_CMNINT 0x%x\n", OFFSET(struct cpu, cpu_cmnint));
	printf("#define\tCPU_CMNTRAP 0x%x\n", OFFSET(struct cpu, cpu_cmntrap));
	printf("#define\tCPU_KADB 0x%x\n", OFFSET(struct cpu, cpu_trap_kadb));
	printf("#define\tCPU_MSR_DISABLED 0x%x\n",
		OFFSET(struct cpu, cpu_msr_disabled));
	printf("#define\tCPU_MSR_ENABLED 0x%x\n",
		OFFSET(struct cpu, cpu_msr_enabled));
	printf("#define\tCPU_MSR_KADB 0x%x\n",
		OFFSET(struct cpu, cpu_msr_kadb));
	printf("#define\tCPU_R3 0x%x\n", OFFSET(struct cpu, cpu_r3));
	printf("#define\tCPU_SRR0 0x%x\n", OFFSET(struct cpu, cpu_srr0));
	printf("#define\tCPU_SRR1 0x%x\n", OFFSET(struct cpu, cpu_srr1));
	printf("#define\tCPU_DSISR 0x%x\n", OFFSET(struct cpu, cpu_dsisr));
	printf("#define\tCPU_DAR 0x%x\n", OFFSET(struct cpu, cpu_dar));

	printf("#define\tCPU_SYSCALL 0x%x\n", OFFSET(struct cpu, cpu_syscall));

	printf("#define\tCPU_ID 0x%x\n", OFFSET(struct cpu, cpu_id));
	printf("#define\tCPU_FLAGS 0x%x\n", OFFSET(struct cpu, cpu_flags));
	printf("#define\tCPU_READY %d\n", CPU_READY);
	printf("#define\tCPU_QUIESCED %d\n", CPU_QUIESCED);
	printf("#define\tCPU_THREAD 0x%x\n", OFFSET(struct cpu, cpu_thread));
	printf("#define\tCPU_THREAD_LOCK 0x%x\n",
	    OFFSET(struct cpu, cpu_thread_lock));
	printf("#define\tCPU_KPRUNRUN 0x%x\n",
	    OFFSET(struct cpu, cpu_kprunrun));
	printf("#define\tCPU_LWP 0x%x\n", OFFSET(struct cpu, cpu_lwp));
	printf("#define\tCPU_FPOWNER 0x%x\n", OFFSET(struct cpu, cpu_fpowner));
	printf("#define\tCPU_IDLE_THREAD 0x%x\n",
	    OFFSET(struct cpu, cpu_idle_thread));
	printf("#define\tCPU_INTR_THREAD 0x%x\n",
	    OFFSET(struct cpu, cpu_intr_thread));
	printf("#define\tCPU_INTR_ACTV 0x%x\n",
	    OFFSET(struct cpu, cpu_intr_actv));
	printf("#define\tCPU_BASE_SPL 0x%x\n",
	    OFFSET(struct cpu, cpu_base_spl));
	printf("#define\tCPU_ON_INTR 0x%x\n", OFFSET(struct cpu, cpu_on_intr));
	printf("#define\tCPU_INTR_STACK 0x%x\n",
	    OFFSET(struct cpu, cpu_intr_stack));
	printf("#define\tCPU_STATS 0x%x\n", OFFSET(struct cpu, cpu_stat));
	printf("#define\tCPU_SYSINFO_INTR 0x%x\n",
	    OFFSET(struct cpu, cpu_stat.cpu_sysinfo.intr));
	printf("#define\tCPU_SYSINFO_INTRTHREAD 0x%x\n",
	    OFFSET(struct cpu, cpu_stat.cpu_sysinfo.intrthread));
	printf("#define\tCPU_SYSINFO_INTRBLK 0x%x\n",
	    OFFSET(struct cpu, cpu_stat.cpu_sysinfo.intrblk));
	printf("#define\tCPU_SYSINFO_CPUMIGRATE 0x%x\n",
	    OFFSET(struct cpu, cpu_stat.cpu_sysinfo.cpumigrate));
	printf("#define\tCPU_SYSINFO_SYSCALL 0x%x\n",
		OFFSET(struct cpu, cpu_stat.cpu_sysinfo.syscall));
	printf("#define\tCPU_SYSINFO_UO_CNT 0x%x\n",
	    OFFSET(struct cpu, cpu_stat.cpu_sysinfo.win_uo_cnt));
	printf("#define\tCPU_SYSINFO_UU_CNT 0x%x\n",
	    OFFSET(struct cpu, cpu_stat.cpu_sysinfo.win_uu_cnt));
	printf("#define\tCPU_SYSINFO_SO_CNT 0x%x\n",
	    OFFSET(struct cpu, cpu_stat.cpu_sysinfo.win_so_cnt));
	printf("#define\tCPU_SYSINFO_SU_CNT 0x%x\n",
	    OFFSET(struct cpu, cpu_stat.cpu_sysinfo.win_su_cnt));
	printf("#define\tCPU_SYSINFO_SUO_CNT 0x%x\n",
	    OFFSET(struct cpu, cpu_stat.cpu_sysinfo.win_suo_cnt));
	printf("#define\tCPU_PRI 0x%x\n", &cpu->cpu_m.mcpu_pri);
	printf("#define\tCPU_PRI_DATA 0x%x\n", &cpu->cpu_m.mcpu_pri_data);
	printf("#define\tCPU_INTR_STACK 0x%x\n", &cpu->cpu_intr_stack);
	printf("#define\tCPU_ON_INTR 0x%x\n", &cpu->cpu_on_intr);
	printf("#define\tCPU_INTR_THREAD 0x%x\n", &cpu->cpu_intr_thread);
	printf("#define\tCPU_INTR_ACTV 0x%x\n", &cpu->cpu_intr_actv);
	printf("#define\tCPU_BASE_SPL 0x%x\n", &cpu->cpu_base_spl);
	printf("#define\tC_CURMASK 0x%x\n", &sc->c_curmask);
	printf("#define\tC_IPLMASK 0x%x\n", &sc->c_iplmask);
	printf("#define\tMCMD_PORT %d\n", MCMD_PORT);
	printf("#define\tSCMD_PORT %d\n", SCMD_PORT);
	printf("#define\tMIMR_PORT %d\n", MIMR_PORT);
	printf("#define\tSIMR_PORT %d\n", SIMR_PORT);
	printf("#define\tCPU_PROFILING	0x%x\n",
	    OFFSET(struct cpu, cpu_profiling));
	printf("#define\tPROF_LOCK 0x%x\n",
	    OFFSET(struct kern_profiling, profiling_lock));
	printf("#define\tPROF_RP 0x%x\n",
		OFFSET(struct kern_profiling, rp));
	printf("#define\tPROFILING 0x%x\n",
	    OFFSET(struct kern_profiling, profiling));
	printf("#define\tPROF_FROMS 0x%x\n",
	    OFFSET(struct kern_profiling, froms));
	printf("#define\tPROF_TOS 0x%x\n",
	    OFFSET(struct kern_profiling, tos));
	printf("#define\tPROF_FROMSSIZE 0x%x\n",
	    OFFSET(struct kern_profiling, fromssize));
	printf("#define\tPROF_TOSSIZE 0x%x\n",
	    OFFSET(struct kern_profiling, tossize));
	printf("#define\tPROF_TOSNEXT 0x%x\n",
	    OFFSET(struct kern_profiling, tosnext));
	printf("#define\tKPC_LINK 0x%x\n",
	    OFFSET(struct kp_call, link));
	printf("#define\tKPC_FROM 0x%x\n",
	    OFFSET(struct kp_call, frompc));
	printf("#define\tKPC_TO 0x%x\n",
	    OFFSET(struct kp_call, topc));
	printf("#define\tKPC_COUNT 0x%x\n",
	    OFFSET(struct kp_call, count));
	printf("#define\tKPCSIZE 0x%x\n",
	    sizeof (struct kp_call));
	printf("#define\tKERNEL_LOWPC 0x%x\n",
	    OFFSET(struct kern_profiling, kernel_lowpc));
	printf("#define\tKERNEL_HIGHPC 0x%x\n",
	    OFFSET(struct kern_profiling, kernel_highpc));
	printf("#define\tKERNEL_TEXTSIZE 0x%x\n",
	    OFFSET(struct kern_profiling, kernel_textsize));
	printf("#define\tMODULE_LOWPC 0x%x\n",
	    OFFSET(struct kern_profiling, module_lowpc));
	printf("#define\tMODULE_HIGHPC 0x%x\n",
	    OFFSET(struct kern_profiling, module_highpc));
	printf("#define\tMODULE_TEXTSIZE 0x%x\n",
	    OFFSET(struct kern_profiling, module_textsize));
	printf("#define\tDEVI_DEV_OPS 0x%x\n", &devi->devi_ops);
	printf("#define\tDEVI_BUS_OPS 0x%x\n", &ops->devo_bus_ops);
	printf("#define\tOPS_MAP 0x%x\n", &busops->bus_dma_map);
	printf("#define\tOPS_MCTL 0x%x\n", &busops->bus_dma_ctl);
	printf("#define\tOPS_CTL 0x%x\n", &busops->bus_ctl);

	printf("#define\tMAXSYSARGS\t%d\n", MAXSYSARGS);

	printf("#define\tM_TYPE\t0x%x\n",
	    OFFSET(struct adaptive_mutex, m_type));
	printf("#define\tM_WAITERS\t0x%x\n",
	    OFFSET(struct adaptive_mutex, m_waiters));
	printf("#define\tM_SPINLOCK\t0x%x\n",
	    OFFSET(struct spin_mutex, m_spinlock));
	printf("#define\tM_OLDSPL\t0x%x\n",
	    OFFSET(struct spin_mutex, m_oldspl));
	printf("#define\tSD_LOCK 0x%x\n",
	    OFFSET(struct stdata, sd_lock));
	printf("#define\tQ_FLAG 0x%x\n",
	    OFFSET(queue_t, q_flag));
	printf("#define\tQ_NEXT 0x%x\n",
	    OFFSET(queue_t, q_next));
	printf("#define\tQ_STREAM 0x%x\n",
	    OFFSET(queue_t, q_stream));
	printf("#define\tQ_SYNCQ 0x%x\n",
	    OFFSET(queue_t, q_syncq));
	printf("#define\tQ_QINFO 0x%x\n",
	    OFFSET(queue_t, q_qinfo));
	printf("#define\tQI_PUTP 0x%x\n",
	    OFFSET(struct qinit, qi_putp));
	printf("#define\tSQ_FLAGS 0x%x\n",
	    OFFSET(syncq_t, sq_flags));
	printf("#define\tSQ_COUNT 0x%x\n",
	    OFFSET(syncq_t, sq_count));
	printf("#define\tSQ_LOCK 0x%x\n",
	    OFFSET(syncq_t, sq_lock));
	printf("#define\tSQ_WAIT 0x%x\n",
	    OFFSET(syncq_t, sq_wait));
	printf("#define\tSQ_SAVE 0x%x\n",
	    OFFSET(syncq_t, sq_save));

	printf("#define\tNSEC_PER_CLOCK_TICK 0x%x\n", NANOSEC / 100);
	printf("#define\tPITCTR0_PORT 0x%x\n", PITCTR0_PORT);

	printf("#define\tNBPW %d\n", NBPW);
	printf("#define\tKERNELBASE 0x%x\n", (u_int)KERNELBASE);
	printf("#define\tUSERLIMIT 0x%x\n", (u_int)USERLIMIT);

	/* offsets of the least/most significant words within a longlong */
#if defined(_LONG_LONG_LTOH)
	printf("#define\tLL_LSW 0\n");
	printf("#define\tLL_MSW 4\n");
#else
	printf("#define\tLL_LSW 4\n");
	printf("#define\tLL_MSW 0\n");
#endif

#ifdef _BIG_ENDIAN
	printf("#define\t_BIG_ENDIAN \n");
#endif

#ifdef _LITTLE_ENDIAN
	printf("#define\t_LITTLE_ENDIAN\n");
#endif

/*
 * Gross hack... Although genassym is a user program and hence exit has one
 * parameter, it is compiled with the kernel headers and the _KERNEL define
 * so ANSI-C thinks it should have two!
 */
	exit(0, 0);
}

bit(mask)
	register long mask;
{
	register int i;

	for (i = 0; i < sizeof (int) * NBBY; i++) {
		if (mask & 1)
			return (i);
		mask >>= 1;
	}

/*
 * Gross hack... Although genassym is a user program and hence exit has one
 * parameter, it is compiled with the kernel headers and the _KERNEL define
 * so ANSI-C thinks it should have two!
 */
	exit(1, 0);
}
