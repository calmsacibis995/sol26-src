/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)i86_vtrace.c 1.4	94/09/27 SMI"

/*
 * Code to drive the kernel tracing mechanism and vtrace system call
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/archsystm.h>
#include <sys/user.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/proc.h>
#include <sys/disp.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/cmn_err.h>
#include <sys/vtrace.h>
#include <vm/seg.h>
#include <vm/seg_map.h>

#ifdef	TRACE

/*
 * On the SPARC machines the trace code is implemented using fast traps
 * which is written in assembly language. The following routines are the
 * C versions of that code.
 * Rick McNeal 17-Nov-1992
 */

extern u_long bytes2data[];
caddr_t trace_dump_head(u_long);
caddr_t trace_store_string(int, u_long, u_long, caddr_t);
void trace_write_buffer(u_long *, u_long);
void trace_dump_tail(caddr_t);

/* in common/os/vtrace.c */
extern hrtime_t get_vtrace_time(void);

caddr_t
trace_dump_head(u_long event)
{
	kthread_id_t lastkthread, curkthread;
	vt_pointer_t v;
	struct cpu *lcp = curcpup();
	u_long hrtime, ltime;

	lastkthread = lcp->cpu_trace.last_thread;
	curkthread = lcp->cpu_thread;

	/*
	 * If the last traced thread is different than the current
	 * cpu thread switch. Also create a header indicating the
	 * new thread.
	 */
	if (curkthread != lastkthread) {
		if (lastkthread == (kthread_id_t)0)
			return (NULL);

		lcp->cpu_trace.last_thread = curkthread;
		v.char_p = lcp->cpu_trace.tbuf_head;
		v.raw_kthread_id_p->head =
		    FTT2HEAD(TR_FAC_TRACE, TR_RAW_KTHREAD_ID, 0);
		v.raw_kthread_id_p->tid = (u_long)curkthread;
		v.raw_kthread_id_p++;
		lcp->cpu_trace.tbuf_head = v.char_p;
	}

	/*
	 * Get a high resolution time value (Needs to be microsecond or better)
	 */
	ltime = (u_long)get_vtrace_time();
	v.char_p = lcp->cpu_trace.tbuf_head;
	hrtime = lcp->cpu_trace.last_hrtime_lo32;
	lcp->cpu_trace.last_hrtime_lo32 = ltime;
	ltime -= hrtime;
	hrtime = ltime >> 16;

	/*
	 * Since the last time we checked has the timer overflowed the
	 * sixteen bit value?
	 * If so, create a record show the new time base.
	 */
	if (hrtime) {
		v.elapsed_time_p->time = ltime;
		v.elapsed_time_p->head =
		    FTT2HEAD(TR_FAC_TRACE, TR_ELAPSED_TIME, 0);
		v.elapsed_time_p++;
		ltime = 0;
	}

	/*
	 * Update the event and store this new event in the trace buffer
	 */
	hrtime = (event & ~0xff) | ltime;
	*v.u_long_p = hrtime;

	return ((caddr_t)v.char_p);
}

caddr_t
trace_store_string(int mask, u_long event, u_long d1, caddr_t headp)
{
	int i;
	char c;
	char *strptr = (char *)d1;

	/*
	 * Is this event a string item?
	 */
	if (event & mask) {
		/*
		 * Copy string to trace buffer. Include null termination.
		 */
		for (i = 0; i < (VT_MAX_BYTES - 5); i++) {
			c = strptr[i];
			headp[i+4] = c;
			if (c == 0)
				break;
		}
		i++;

		/*
		 * Update the trace buffer. headp is the beginning of a
		 * record and needs a header. bytes2data[i] contains that
		 * value. bytes2data[i + 256] contains the len of the
		 * complete record.
		 */
		*(u_long *)headp = bytes2data[i];
		headp += bytes2data[256 + i];
	}
	return (headp);
}

void
trace_write_buffer(u_long *bufp, u_long d1)
{
	struct cpu *lcp = curcpup();
	caddr_t headp, newp;
	int s = clear_int_flag();

	/*
	 * Make sure that the count is a multiple of 8. Since this routine
	 * is written using 'char *' it would be possible to use a count
	 * not a mod of 8 though.
	 */
	if (((d1 & 3) == 0) && lcp->cpu_trace.last_thread) {
		headp = (caddr_t)lcp->cpu_trace.tbuf_head;
		newp = headp + d1;
		bcopy((char *)bufp, headp, d1);
		trace_dump_tail(newp);
	} else
		cmn_err(CE_CONT, "trace_write: bad buff 0x%x\n",
		    lcp->cpu_trace.tbuf_head - lcp->cpu_trace.tbuf_start);
	restore_int_flag(s);
}

void
trace_dump_tail(caddr_t headp)
{
	struct cpu *lcp = curcpup();
	caddr_t redzone = (caddr_t)lcp->cpu_trace.tbuf_redzone;
	caddr_t curpage = (caddr_t)((int)headp & ~0xfff);
	caddr_t wrap = lcp->cpu_trace.tbuf_wrap;
	caddr_t end;
	u_long padval;

	if (curpage == redzone)
		lcp->cpu_trace.tbuf_overflow = (char *)headp;

	lcp->cpu_trace.tbuf_head = (char *)headp;
	if (headp >= wrap) {
		padval = FTT2HEAD(TR_FAC_TRACE, TR_PAD, 0);
		end = lcp->cpu_trace.tbuf_end;
		for (; headp < end; headp += 4)
			*(u_long *)headp = padval;
		headp = lcp->cpu_trace.tbuf_start;
		lcp->cpu_trace.tbuf_head = headp;
	}
}

void
trace_0(u_long event)
{
	int s = clear_int_flag();
	caddr_t headp;

	if ((headp = trace_dump_head(event)) == NULL) {
		restore_int_flag(s);
		return;
	}
	headp += 4;
	trace_dump_tail(headp);
	restore_int_flag(s);
}

void
trace_1(u_long event, u_long d1)
{
	int s = clear_int_flag();
	caddr_t headp;

	if ((headp = trace_dump_head(event)) == NULL) {
		restore_int_flag(s);
		return;
	}
	*(((u_long *)headp) + 1) = d1;
	headp += 8;
	headp = trace_store_string(VT_STRING_1, event, d1, headp);
	trace_dump_tail(headp);
	restore_int_flag(s);
}

void
trace_2(u_long event, u_long d1, u_long d2)
{
	int s = clear_int_flag();
	caddr_t headp;

	if ((headp = trace_dump_head(event)) == NULL) {
		restore_int_flag(s);
		return;
	}
	*(((u_long *)headp) + 1) = d1;
	*(((u_long *)headp) + 2) = d2;
	headp += 12;
	if (event & VT_STRING_MASK) {
		headp = trace_store_string(VT_STRING_1, event, d1, headp);
		headp = trace_store_string(VT_STRING_2, event, d2, headp);
	}
	trace_dump_tail(headp);
	restore_int_flag(s);
}

void
trace_3(u_long event, u_long d1, u_long d2, u_long d3)
{
	int s = clear_int_flag();
	caddr_t headp;

	if ((headp = trace_dump_head(event)) == NULL) {
		restore_int_flag(s);
		return;
	}
	*(((u_long *)headp) + 1) = d1;
	*(((u_long *)headp) + 2) = d2;
	*(((u_long *)headp) + 3) = d3;
	headp += 16;
	if (event & VT_STRING_MASK) {
		headp = trace_store_string(VT_STRING_1, event, d1, headp);
		headp = trace_store_string(VT_STRING_2, event, d2, headp);
		headp = trace_store_string(VT_STRING_3, event, d3, headp);
	}
	trace_dump_tail(headp);
	restore_int_flag(s);
}

void
trace_4(u_long event, u_long d1, u_long d2, u_long d3, u_long d4)
{
	int s = clear_int_flag();
	caddr_t headp;

	if ((headp = trace_dump_head(event)) == NULL) {
		restore_int_flag(s);
		return;
	}
	*(((u_long *)headp) + 1) = d1;
	*(((u_long *)headp) + 2) = d2;
	*(((u_long *)headp) + 3) = d3;
	*(((u_long *)headp) + 4) = d4;
	headp += 20;
	if (event & VT_STRING_MASK) {
		headp = trace_store_string(VT_STRING_1, event, d1, headp);
		headp = trace_store_string(VT_STRING_2, event, d2, headp);
		headp = trace_store_string(VT_STRING_3, event, d3, headp);
		headp = trace_store_string(VT_STRING_4, event, d4, headp);
	}
	trace_dump_tail(headp);
	restore_int_flag(s);
}

void
trace_5(u_long event, u_long d1, u_long d2, u_long d3, u_long d4, u_long d5)
{
	int s = clear_int_flag();
	caddr_t headp;

	if ((headp = trace_dump_head(event)) == NULL) {
		restore_int_flag(s);
		return;
	}
	*(((u_long *)headp) + 1) = d1;
	*(((u_long *)headp) + 2) = d2;
	*(((u_long *)headp) + 3) = d3;
	*(((u_long *)headp) + 4) = d4;
	*(((u_long *)headp) + 5) = d5;
	headp += 24;
	if (event & VT_STRING_MASK) {
		headp = trace_store_string(VT_STRING_1, event, d1, headp);
		headp = trace_store_string(VT_STRING_2, event, d2, headp);
		headp = trace_store_string(VT_STRING_3, event, d3, headp);
		headp = trace_store_string(VT_STRING_4, event, d4, headp);
		headp = trace_store_string(VT_STRING_5, event, d5, headp);
	}
	trace_dump_tail(headp);
	restore_int_flag(s);
}

#endif	/* TRACE */
