/*
 * Copyright (c) 1992-1996, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident	"@(#)genassym.c	1.76	96/10/18 SMI"

#ifndef _GENASSYM
#define	_GENASSYM
#endif

#define	SIZES	1

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
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

#include <sys/tss.h>
#include <sys/trap.h>
#include <sys/stack.h>
#include <sys/psw.h>
#include <sys/segment.h>
#include <sys/mmu.h>
#include <sys/cpu.h>
#include <sys/clock.h>

#include <sys/devops.h>
#include <sys/ddi_impldefs.h>

#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/hat_i86.h>

#include <sys/avintr.h>
#include <sys/pic.h>
#include <sys/pit.h>
#include <sys/fp.h>

#include <sys/rm_platter.h>
#include <sys/x86_archext.h>

#ifdef _VPIX
#include <sys/v86.h>
#endif

#include <sys/stream.h>
#include <sys/strsubr.h>

#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ddi_subrdefs.h>

#include <vm/mach_page.h>

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
	register struct timestruc *tms = (struct timestruc *)0;
	register struct tss386 *tss = (struct tss386 *)0;
	register struct cpu *cpu = (struct cpu *)0;
	register struct machcpu *mc = (struct machcpu *)0;
	register struct standard_pic *sc = (struct standard_pic *)0;
	register struct rm_platter *rmp = (struct rm_platter *)0;
	register struct ctxop *ctxop = (struct ctxop *)0;
	struct machpage	mypp;
	register struct ddi_acc_impl *acchdlp = (struct ddi_acc_impl *)0;
#ifdef _VPIX
	register xtss_t	*xtss = (xtss_t *)XTSSADDR;
	register v86_t	*v86p = (v86_t *)0;
#endif
	int	bit();
	int	byteoffset();
	int	bytevalue();

	printf("#define\tSYSBASE %d\n", SYSBASE);
	printf("#define\tSYSLIMIT %d\n", SYSLIMIT);

	printf("#define\tCPU_ARCH %d\n", CPU_ARCH);

	printf("#define\tI86_386_ARCH %d\n", I86_386_ARCH);
	printf("#define\tI86_486_ARCH %d\n", I86_486_ARCH);
	printf("#define\tI86_P5_ARCH %d\n",  I86_P5_ARCH);

	printf("#define\tREGS_CS 0x%x\n", &rp->r_cs);
	printf("#define\tREGS_SS 0x%x\n", &rp->r_ss);
	printf("#define\tREGS_UESP 0x%x\n", &rp->r_uesp);
	printf("#define\tREGS_EAX 0x%x\n", &rp->r_eax);
	printf("#define\tREGS_EDX 0x%x\n", &rp->r_edx);
	printf("#define\tREGS_TRAPNO 0x%x\n", &rp->r_trapno);
	printf("#define\tREGS_EFL 0x%x\n", &rp->r_efl);
	printf("#define\tREGS_PC 0x%x\n", &rp->r_pc);
	printf("#define\tREGS_GS 0x%x\n", &rp->r_gs);

	printf("#define\tT_AST 0x%x\n", T_AST);

	printf("#define\tLOCK_LEVEL 0x%x\n", LOCK_LEVEL);
	printf("#define\tCLOCK_LEVEL 0x%x\n", CLOCK_LEVEL);

	printf("#define\tPIC_NSEOI 0x%x\n", PIC_NSEOI);
	printf("#define\tPIC_SEOI_LVL7 0x%x\n", PIC_SEOI_LVL7);

	printf("#define\tNANOSEC 0x%x\n", NANOSEC);
	printf("#define\tADJ_SHIFT 0x%x\n", ADJ_SHIFT);

	printf("#define\tTSS_ESP0 0x%x\n", &tss->t_esp0);
	printf("#define\tTSS_SS0 0x%x\n", &tss->t_ss0);
	printf("#define\tTSS_LDT 0x%x\n", &tss->t_ldt);
	printf("#define\tTSS_CR3 0x%x\n", &tss->t_cr3);
	printf("#define\tTSS_CS 0x%x\n", &tss->t_cs);
	printf("#define\tTSS_SS 0x%x\n", &tss->t_ss);
	printf("#define\tTSS_DS 0x%x\n", &tss->t_ds);
	printf("#define\tTSS_ES 0x%x\n", &tss->t_es);
	printf("#define\tTSS_FS 0x%x\n", &tss->t_fs);
	printf("#define\tTSS_GS 0x%x\n", &tss->t_gs);
	printf("#define\tTSS_EBP 0x%x\n", &tss->t_ebp);
	printf("#define\tTSS_EIP 0x%x\n", &tss->t_eip);
	printf("#define\tTSS_EFL 0x%x\n", &tss->t_eflags);
	printf("#define\tTSS_ESP 0x%x\n", &tss->t_esp);
	printf("#define\tTSS_EAX 0x%x\n", &tss->t_eax);
	printf("#define\tTSS_EBX 0x%x\n", &tss->t_ebx);
	printf("#define\tTSS_ECX 0x%x\n", &tss->t_ecx);
	printf("#define\tTSS_EDX 0x%x\n", &tss->t_edx);
	printf("#define\tTSS_ESI 0x%x\n", &tss->t_esi);
	printf("#define\tTSS_EDI 0x%x\n", &tss->t_edi);
	printf("#define\tTSS_LDT 0x%x\n", &tss->t_ldt);
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
	printf("#define\tP_USER 0x%x\n", &p->p_user);
	printf("#define\tP_LDT 0x%x\n", &p->p_ldt);
	printf("#define\tP_LDT_DESC 0x%x\n", &p->p_ldt_desc);
	printf("#define\tPROCSIZE 0x%x\n", sizeof (struct proc));
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
	printf("#define\tT_MSTATE 0x%x\n", &tid->t_mstate);
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
	printf("#define\tT_PRE_SYS 0x%x\n", &tid->t_pre_sys);
	printf("#define\tT_PREEMPT 0x%x\n", &tid->t_preempt);
	printf("#define\tT_PROC_FLAG 0x%x\n", &tid->t_proc_flag);
	printf("#define\tT_MMUCTX 0x%x\n", &tid->t_mmuctx);
	printf("#define\tT_STARTPC 0x%x\n", &tid->t_startpc);
	printf("#define\tT_ASTFLAG 0x%x\n", &tid->t_astflag);
	printf("#define\tT_POST_SYS_AST 0x%x\n", &tid->t_post_sys_ast);
	printf("#define\tT_SYSNUM 0x%x\n", &tid->t_sysnum);
	printf("#define\tT_INTR_THREAD %d\n", T_INTR_THREAD);
	printf("#define\tCTXOP_SAVE %d\n", &ctxop->save_op);
	printf("#define\tFREE_THREAD 0x%x\n", TS_FREE);
	printf("#define\tTS_FREE 0x%x\n", TS_FREE);
	printf("#define\tTS_ZOMB 0x%x\n", TS_ZOMB);
	printf("#define\tTP_MSACCT 0x%x\n", TP_MSACCT);
	printf("#define\tTP_WATCHPT 0x%x\n", TP_WATCHPT);
	printf("#define\tTP_TWAIT 0x%x\n", TP_TWAIT);
	printf("#define\tONPROC_THREAD 0x%x\n", TS_ONPROC);
	printf("#define\tT0STKSZ 0x%x\n", DEFAULTSTKSZ);
	printf("#define\tTHREAD_SIZE %d\n", sizeof (kthread_t));

	printf("#define\tA_HAT 0x%x\n", &as->a_hat);
	printf("#define\tHAT_MUTEX 0x%x\n", &hat->hat_mutex);

	printf("#define\tMSGBUFSIZE 0x%x\n", sizeof (struct msgbuf));

	printf("#define\tS_READ 0x%x\n", (int)S_READ);
	printf("#define\tS_WRITE 0x%x\n", (int)S_WRITE);
	printf("#define\tS_EXEC 0x%x\n", (int)S_EXEC);
	printf("#define\tS_OTHER 0x%x\n", (int)S_OTHER);

	printf("#define\tU_COMM 0x%x\n", up->u_comm);
	printf("#define\tU_SIGNAL 0x%x\n", up->u_signal);
	printf("#define\tUSIZEBYTES 0x%x\n", sizeof (struct user));

	printf("#define\tNORMALRETURN 0x%x\n", (int)NORMALRETURN);
	printf("#define\tLWP_THREAD 0x%x\n", &lwpid->lwp_thread);
	printf("#define\tLWP_PROCP 0x%x\n", &lwpid->lwp_procp);
	printf("#define\tLWP_EOSYS 0x%x\n", &lwpid->lwp_eosys);
	printf("#define\tLWP_REGS 0x%x\n", &lwpid->lwp_regs);
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
	printf("#define\tLWP_GPFAULT 0x%x\n", &lwpid->lwp_gpfault);
	printf("#define\tLWP_PCB_FLAGS 0x%x\n", &lwpid->lwp_pcb.pcb_flags);
	printf("#define\tLWP_PCB_DREGS 0x%x\n", &lwpid->lwp_pcb.pcb_dregs);
	printf("#define\tLWP_RU_SYSC 0x%x\n", &lwpid->lwp_ru.sysc);
	printf("#define\tLWP_FPU_REGS 0x%x\n",
	    &lwpid->lwp_pcb.pcb_fpu.fpu_regs);
	printf("#define\tLWP_FPU_FLAGS 0x%x\n",
	    &lwpid->lwp_pcb.pcb_fpu.fpu_flags);
	printf("#define\tLWP_FPU_CHIP_STATE 0x%x\n",
	    &lwpid->lwp_pcb.pcb_fpu.fpu_regs.fp_reg_set.fpchip_state);
	printf("#define\tLWP_PCB_FPU 0x%x\n", &lwpid->lwp_pcb.pcb_fpu);
	printf("#define\tPCB_FPU_REGS 0x%x\n",
	    (int)&lwpid->lwp_pcb.pcb_fpu.fpu_regs -
	    (int)&lwpid->lwp_pcb);
	printf("#define\tPCB_FPU_FLAGS 0x%x\n",
	    (int)&lwpid->lwp_pcb.pcb_fpu.fpu_flags -
	    (int)&lwpid->lwp_pcb);
	printf("#define\tLMS_USER 0x%x\n", LMS_USER);
	printf("#define\tLMS_SYSTEM 0x%x\n", LMS_SYSTEM);

	printf("#define\tFP_487 %d\n", FP_487);
	printf("#define\tFP_486 %d\n", FP_486);
	printf("#define\tFPU_CW_INIT 0x%x\n", FPU_CW_INIT);
	printf("#define\tFPU_EN 0x%x\n", FPU_EN);
	printf("#define\tFPU_VALID 0x%x\n", FPU_VALID);

	printf("#define\tPCB_FLAGS 0x%x\n", &lwpid->lwp_pcb.pcb_flags);
	printf("#define\tPCBSIZE 0x%x\n", sizeof (struct pcb));

	printf("#define\tREGSIZE %d\n", sizeof (struct regs));

	printf("#define\tELF_NOTE_SOLARIS \"%s\"\n", ELF_NOTE_SOLARIS);
	printf("#define\tELF_NOTE_PAGESIZE_HINT %d\n", ELF_NOTE_PAGESIZE_HINT);

	printf("#define\tIDTSZ %d\n", IDTSZ);
	printf("#define\tGDTSZ %d\n", GDTSZ);
	printf("#define\tMINLDTSZ %d\n", MINLDTSZ);

	printf("#define\tFP_NO %d\n", FP_NO);
	printf("#define\tFP_SW %d\n", FP_SW);
	printf("#define\tFP_HW %d\n", FP_HW);
	printf("#define\tFP_287 %d\n", FP_287);
	printf("#define\tFP_387 %d\n", FP_387);

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

	printf("#define\tCPU_ID 0x%x\n", OFFSET(struct cpu, cpu_id));
	printf("#define\tCPU_PAGEDIR 0x%x\n", OFFSET(struct cpu, cpu_pagedir));
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
	printf("#define\tCPU_SYSINFO_UO_CNT 0x%x\n",
	    OFFSET(struct cpu, cpu_stat.cpu_sysinfo.win_uo_cnt));
	printf("#define\tCPU_SYSINFO_UU_CNT 0x%x\n",
	    OFFSET(struct cpu, cpu_stat.cpu_sysinfo.win_uu_cnt));
	printf("#define\tCPU_SYSINFO_SO_CNT 0x%x\n",
	    OFFSET(struct cpu, cpu_stat.cpu_sysinfo.win_so_cnt));
	printf("#define\tCPU_SYSINFO_SU_CNT 0x%x\n",
	    OFFSET(struct cpu, cpu_stat.cpu_sysinfo.win_su_cnt));
	printf("#define\tCPU_SYSINFO_SYSCALL 0x%x\n",
	    OFFSET(struct cpu, cpu_stat.cpu_sysinfo.syscall));
	printf("#define\tCPU_SYSINFO_SUO_CNT 0x%x\n",
	    OFFSET(struct cpu, cpu_stat.cpu_sysinfo.win_suo_cnt));
	printf("#define\tCPU_CURRENT_HAT 0x%x\n",
	    OFFSET(struct cpu, cpu_current_hat));
	printf("#define\tCPU_CR3 0x%x\n", OFFSET(struct cpu, cpu_cr3));
	printf("#define\tCPU_GDT 0x%x\n", OFFSET(struct cpu, cpu_gdt));
	printf("#define\tCPU_IDT 0x%x\n", OFFSET(struct cpu, cpu_idt));
	printf("#define\tCPU_TSS 0x%x\n", OFFSET(struct cpu, cpu_tss));
	printf("#define\tCPU_PRI 0x%x\n", &mc->mcpu_pri);
	printf("#define\tCPU_PRI_DATA 0x%x\n", &mc->mcpu_pri_data);
	printf("#define\tCPU_INTR_STACK 0x%x\n", &cpu->cpu_intr_stack);
	printf("#define\tCPU_ON_INTR 0x%x\n", &cpu->cpu_on_intr);
	printf("#define\tCPU_INTR_THREAD 0x%x\n", &cpu->cpu_intr_thread);
	printf("#define\tCPU_INTR_ACTV 0x%x\n", &cpu->cpu_intr_actv);
	printf("#define\tCPU_BASE_SPL 0x%x\n", &cpu->cpu_base_spl);
	printf("#define\tCPU_M 0x%x\n", &cpu->cpu_m);
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
	printf("#define\tPROFILING 0x%x\n",
	    OFFSET(struct kern_profiling, profiling));
	printf("#define\tPROF_RP 0x%x\n",
	    OFFSET(struct kern_profiling, rp));
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
	printf("#define\tDEVI_BUS_CTL 0x%x\n", &devi->devi_bus_ctl);
	printf("#define\tDEVI_BUS_DMA_MAP 0x%x\n", &devi->devi_bus_dma_map);
	printf("#define\tDEVI_BUS_DMA_CTL 0x%x\n", &devi->devi_bus_dma_ctl);
	printf("#define\tDEVI_BUS_DMA_ALLOCHDL 0x%x\n",
	    &devi->devi_bus_dma_allochdl);
	printf("#define\tDEVI_BUS_DMA_FREEHDL 0x%x\n",
	    &devi->devi_bus_dma_freehdl);
	printf("#define\tDEVI_BUS_DMA_BINDHDL 0x%x\n",
	    &devi->devi_bus_dma_bindhdl);
	printf("#define\tDEVI_BUS_DMA_UNBINDHDL 0x%x\n",
	    &devi->devi_bus_dma_unbindhdl);
	printf("#define\tDEVI_BUS_DMA_FLUSH 0x%x\n", &devi->devi_bus_dma_flush);
	printf("#define\tDEVI_BUS_DMA_WIN 0x%x\n", &devi->devi_bus_dma_win);

	printf("#define\tDEVI_BUS_OPS 0x%x\n", &ops->devo_bus_ops);
	printf("#define\tOPS_CTL 0x%x\n", &busops->bus_ctl);
	printf("#define\tOPS_MAP 0x%x\n", &busops->bus_dma_map);
	printf("#define\tOPS_MCTL 0x%x\n", &busops->bus_dma_ctl);
	printf("#define\tOPS_ALLOCHDL 0x%x\n", &busops->bus_dma_allochdl);
	printf("#define\tOPS_FREEHDL 0x%x\n", &busops->bus_dma_freehdl);
	printf("#define\tOPS_BINDHDL 0x%x\n", &busops->bus_dma_bindhdl);
	printf("#define\tOPS_UNBINDHDL 0x%x\n", &busops->bus_dma_unbindhdl);
	printf("#define\tOPS_FLUSH 0x%x\n", &busops->bus_dma_flush);
	printf("#define\tOPS_WIN 0x%x\n", &busops->bus_dma_win);

	printf("#define\tM_TYPE\t0x%x\n",
	    OFFSET(struct adaptive_mutex, m_type));
	printf("#define\tM_WAITERS\t0x%x\n",
	    OFFSET(struct adaptive_mutex, m_waiters));
	printf("#define\tM_SPINLOCK\t0x%x\n",
	    OFFSET(struct spin_mutex, m_spinlock));
	printf("#define\tM_OLDSPL\t0x%x\n",
	    OFFSET(struct spin_mutex, m_oldspl));
	printf("#define\tM_OWNER\t0x%x\n",
	    OFFSET(struct adaptive_mutex2, m_owner_lock) & ~(sizeof (long)-1));
	printf("#define\tNSYSCALL %d\n", NSYSCALL);
	printf("#define\tSYSENT_SIZE %d\n", sizeof (struct sysent));
	printf("#define\tSY_CALLC 0x%x\n", OFFSET(struct sysent, sy_callc));
	printf("#define\tSY_NARG 0x%x\n", OFFSET(struct sysent, sy_narg));

	printf("#define\tMAXSYSARGS\t%d\n", MAXSYSARGS);

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

	/* Hack value just to allow clock to be kicked */
	printf("#define\tNSEC_PER_CLOCK_TICK 0x%x\n", NANOSEC / 100);

	printf("#define\tPITCTR0_PORT 0x%x\n", PITCTR0_PORT);

	printf("#define\tNBPW %d\n", NBPW);
	printf("#define\tKERNELBASE 0x%x\n", (u_int)KERNELBASE);
	printf("#define\tKERNELSHADOW_VADDR 0x%x\n", (u_int)KERNELSHADOW_VADDR);

#ifdef _VPIX
	printf("#define\tT_V86DATA %d\n", &tid->t_v86data);
	printf("#define\tV86_SWTCH %d\n", &v86p->vp_ops.v86_swtch);
	printf("#define\tV86_RTT %d\n", &v86p->vp_ops.v86_rtt);

	printf("#define\tXT_VFLPTR 0x%x\n", &xtss->xt_vflptr);
	printf("#define\tXT_OP_EMUL 0x%x\n", &xtss->xt_op_emul);
	printf("#define\tXT_MAGICTRAP 0x%x\n", &xtss->xt_magictrap);
	printf("#define\tXT_MAGICSTAT 0x%x\n", &xtss->xt_magicstat);
	printf("#define\tXT_INTR_PIN 0x%x\n", &xtss->xt_intr_pin);
	printf("#define\tXT_IMASKBITS 0x%x\n", &xtss->xt_imaskbits);

	printf("#define\tCPU_V86PROCFLAG 0x%x\n", &cpu->cpu_v86procflag);
	printf("#define\tCPU_GDT 0x%x\n", &cpu->cpu_gdt);

	printf("#define\tGDT_XTSSSEL 0x%x\n", seltoi(XTSSSEL) *
						(sizeof (struct seg_desc)));

#endif
	printf("#define\tIDTROFF 0x%x\n", &rmp->rm_idt_lim);
	printf("#define\tGDTROFF 0x%x\n", &rmp->rm_gdt_lim);
	printf("#define\tCR3OFF 0x%x\n", &rmp->rm_pdbr);
	printf("#define\tCPUNOFF 0x%x\n", &rmp->rm_cpu);
	printf("#define\tCR4OFF 0x%x\n", &rmp->rm_cr4);
	printf("#define\tX86FEATURE 0x%x\n", &rmp->rm_x86feature);

	printf("#define\tACC_GETB 0x%x\n", &acchdlp->ahi_get8);
	printf("#define\tACC_GETW 0x%x\n", &acchdlp->ahi_get16);
	printf("#define\tACC_GETL 0x%x\n", &acchdlp->ahi_get32);
	printf("#define\tACC_GETLL 0x%x\n", &acchdlp->ahi_get64);
	printf("#define\tACC_PUTB 0x%x\n", &acchdlp->ahi_put8);
	printf("#define\tACC_PUTW 0x%x\n", &acchdlp->ahi_put16);
	printf("#define\tACC_PUTL 0x%x\n", &acchdlp->ahi_put32);
	printf("#define\tACC_PUTLL 0x%x\n", &acchdlp->ahi_put64);
	printf("#define\tACC_REP_GETB 0x%x\n", &acchdlp->ahi_rep_get8);
	printf("#define\tACC_REP_GETW 0x%x\n", &acchdlp->ahi_rep_get16);
	printf("#define\tACC_REP_GETL 0x%x\n", &acchdlp->ahi_rep_get32);
	printf("#define\tACC_REP_GETLL 0x%x\n", &acchdlp->ahi_rep_get64);
	printf("#define\tACC_REP_PUTB 0x%x\n", &acchdlp->ahi_rep_put8);
	printf("#define\tACC_REP_PUTW 0x%x\n", &acchdlp->ahi_rep_put16);
	printf("#define\tACC_REP_PUTL 0x%x\n", &acchdlp->ahi_rep_put32);
	printf("#define\tACC_REP_PUTLL 0x%x\n", &acchdlp->ahi_rep_put64);
	printf("#define\tDDI_DEV_AUTOINCR 0x%x\n", DDI_DEV_AUTOINCR);

	printf("#define\tX86_P5 0x%x\n", X86_P5);
	printf("#define\tX86_K5 0x%x\n", X86_K5);
	printf("#define\tX86_P6 0x%x\n", X86_P6);

	printf("#define\tK5_PSE 0x%x\n", K5_PSE);
	printf("#define\tK5_GPE 0x%x\n", K5_GPE);
	printf("#define\tK5_PGE 0x%x\n", K5_PGE);
	printf("#define\tP5_PSE 0x%x\n", P5_PSE);
	printf("#define\tP6_GPE 0x%x\n", P6_GPE);
	printf("#define\tP6_PGE 0x%x\n", P6_PGE);
	printf("#define\tP5_PSE_SUPPORTED 0x%x\n", P5_PSE_SUPPORTED);
	printf("#define\tP5_TSC_SUPPORTED 0x%x\n", P5_TSC_SUPPORTED);
	printf("#define\tP5_MSR_SUPPORTED 0x%x\n", P5_MSR_SUPPORTED);
	printf("#define\tP6_APIC_SUPPORTED 0x%x\n", P6_APIC_SUPPORTED);
	printf("#define\tP6_MTRR_SUPPORTED 0x%x\n", P6_MTRR_SUPPORTED);
	printf("#define\tP6_PGE_SUPPORTED 0x%x\n", P6_PGE_SUPPORTED);
	printf("#define\tP6_CMOV_SUPPORTED 0x%x\n", P6_CMOV_SUPPORTED);
	printf("#define\tP5_MMX_SUPPORTED 0x%x\n", P5_MMX_SUPPORTED);
	printf("#define\tK5_PGE_SUPPORTED 0x%x\n", K5_PGE_SUPPORTED);
	printf("#define\tK5_SCE_SUPPORTED 0x%x\n", K5_SCE_SUPPORTED);


	printf("#define\tX86_LARGEPAGE 0x%x\n", X86_LARGEPAGE);
	printf("#define\tX86_TSC 0x%x\n", X86_TSC);
	printf("#define\tX86_MSR 0x%x\n", X86_MSR);
	printf("#define\tX86_MTRR 0x%x\n", X86_MTRR);
	printf("#define\tX86_PGE 0x%x\n", X86_PGE);
	printf("#define\tX86_APIC 0x%x\n", X86_APIC);
	printf("#define\tX86_CMOV 0x%x\n", X86_CMOV);
	printf("#define\tX86_MMX 0x%x\n", X86_MMX);

	/*
	 * We need to get address of the byte containing bit fields in
	 * 'struct page'. we also
	 * need to get the value of the byte when various bit fields are set
	 */
	bzero(&mypp, sizeof (struct machpage));
	mypp.p_inuse = 1;
	printf("#define\tPP_INUSE 0x%x\n", byteoffset(&mypp, sizeof (mypp)));
	mypp.p_inuse = 0;

	*(u_short *)&mypp.p_mlistcv = 1;
	printf("#define\tPP_CV 0x%x\n", byteoffset(&mypp, sizeof (mypp)));
	*(u_short *)&mypp.p_mlistcv = 0;

	mypp.p_inuse = 1;
	printf("#define\tP_INUSE_VALUE 0x%x\n",
		bytevalue(&mypp, sizeof (mypp)));
	printf("#define\tP_INUSE 0x%x\n",
		bit(bytevalue(&mypp, sizeof (mypp))));
	mypp.p_wanted = 1;
	printf("#define\tP_INUSE_WANTED_VALUE 0x%x\n",
		bytevalue(&mypp, sizeof (mypp)));
	mypp.p_inuse = 0;

	mypp.p_wanted = 1;
	printf("#define\tP_WANTED_VALUE 0x%x\n",
		bytevalue(&mypp, sizeof (mypp)));
	printf("#define\tP_WANTED 0x%x\n",
		bit(bytevalue(&mypp, sizeof (mypp))));
	mypp.p_wanted = 0;

	mypp.p_impl = 1;
	printf("#define\tP_HMEUNLOAD_VALUE 0x%x\n",
		bytevalue(&mypp, sizeof (mypp)));
	printf("#define\tP_HMEUNLOAD 0x%x\n",
		bit(bytevalue(&mypp, sizeof (mypp))));

	printf("#define\tKERNELBASE_INDEX 0x%x\n", ((u_int)KERNELBASE) >> 20);
	printf("#define\tMMU_STD_PAGESHIFT 0x%x\n", (u_int)MMU_STD_PAGESHIFT);
	printf("#define\tMMU_STD_PAGEMASK 0x%x\n", (u_int)MMU_STD_PAGEMASK);
	printf("#define\tMMU_L2_MASK 0x%x\n", (u_int)(NPTEPERPT - 1));
	printf("#define\tMMU_PAGEOFFSET 0x%x\n", (u_int)(MMU_PAGESIZE - 1));
	printf("#define\tNPTESHIFT 0x%x\n", (u_int)NPTESHIFT);
	printf("#define\tFOURMB_PAGEOFFSET 0x%x\n", (u_int)FOURMB_PAGEOFFSET);

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

/*
 * This function returns the byte offset of the first non zero byte in the
 * block of length 'len' starting at ptr
 */
int
byteoffset(ptr, len)
char	*ptr;
int	len;
{

	int	i;

	for (i = 0; i < len; i++, ptr++)
		if (*ptr)
			return (i);
	return (0);
}
/*
 * This function returns the value of the first non zero byte in the
 * block of length 'len' starting at 'ptr'
 */
int
bytevalue(ptr, len)
char	*ptr;
int	len;
{
	int	i;


	for (i = 0; i < len; i++, ptr++)
		if (*ptr)
			return (*ptr);
	return (0);
}
void
bzero(void *ptr_arg, size_t len)
{
	char	*ptr = ptr_arg;
	int	i;


	for (i = 0; i < len; i++, ptr++)
		*ptr = 0;
}
