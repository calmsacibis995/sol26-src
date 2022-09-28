#
#ident	"@(#)Makefile	1.57	96/09/18 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libthread/ppc/Makefile
#
LIBTHREADDIR=..
LIBCDIR=../../libc
SYS=${LIBTHREADDIR}/sys
SYSARCH=${SYS}/${MACH}
SYSCOM=${SYS}/common
COMMON=${LIBTHREADDIR}/common
LIBTHREAD.ARCH=${LIBTHREADDIR}/${MACH}/ml
INLINE= ${LIBTHREAD.ARCH}/thread.il

LIBCOBJ=	libc_int.o
RTLDOBJ=	rtld_int.o
COMMONOBJS=	condvar.o callout.o disp.o lwp_subr.o ma.o mutex.o\
		preempt.o rwlock.o sema.o subr.o slp.o reaper.o\
		sig.o thread.o thr_subr.o tsd.o \
		attr_subr.o pthr_cond.o pthr_mutex.o pthr_attr.o \
		pthread.o cancel.o thread_interface.o schedctl.o tdb_agent.o

ARCHOBJS=	ppcdep.o ppcsubr.o cresume.o _mutex.o

SYSOBJS= 	fork1.o _fork1.o _fork.o sigaction.o sleep.o\
		ucontext.o sigprocmask.o sigsuspend.o sigwait.o hrestime.o \
		setitimer.o syscall_cancelpoint.o sigpending.o alarm.o \
		fcntl.o

#
# Since assym.s is a derived file, the dependency must be explicit for
# all files including this file. (This is only actually required in the
# instance when the .make.state file does not exist.)
#
ASSYM_DEPS=	ppcsubr.o syscall_cancelpoint.o _mutex.o

RESUMEOBJ=

CFILES=	${COMMON}/condvar.c ${COMMON}/disp.c ${COMMON}/lwp.c \
	${COMMON}/lwp_subr.c ${COMMON}/ma.c\
	${COMMON}/subr.c ${COMMON}/mutex.c ${COMMON}/preempt.c \
	${COMMON}/reaper.c  ${LIBTHREAD.ARCH}/cresume.c ${COMMON}/rwlock.c \
	${COMMON}/sema.c ${COMMON}/thread.c  ${COMMON}/thr_subr.c \
	${COMMON}/tsd.c ${LIBTHREAD.ARCH}/ppcdep.c ${SYSCOM}/sigaction.c\
	${COMMON}/slp.c\
	${SYSCOM}/sleep.c ${COMMON}/sig.c ${SYSCOM}/fork1.c\
	${COMMON}/callout.c ${SYSCOM}/sigsuspend.c ${SYSCOM}/sigwait.c\
	${SYSARCH}/sigsetjmp.c \
	${COMMON}/thread_interface.c ${SYSCOM}/setitimer.c \
	${COMMON}/pthread.c ${COMMON}/attr_subr.c\
	${COMMON}/cancel.c\
	${COMMON}/pthr_cond.c ${COMMON}/pthr_mutex.c ${COMMON}/pthr_attr.c\
	${SYSCOM}/sigpending.c \
	${COMMON}/rtld_int.c ${COMMON}/libc_int.c ${COMMON}/tdb_agent.c

HFILES = ${COMMON}/libthread.h ${COMMON}/libpthr.h

SFILES= ${SYSARCH}/alarm.s\
	${LIBTHREAD.ARCH}/ppcsubr.s \
	${LIBTHREAD.ARCH}/_mutex.s \
	${SYSARCH}/_fork.s ${SYSARCH}/syscall_cancelpoint.s \
	${SYSARCH}/hrestime.s ${SYSARCH}/sigprocmask.s \
	${SYSARCH}/ucontext.s ${SYSARCH}/_fork1.s

INC=-I. -I${LIBTHREADDIR}/inc -I${COMMON} -I${SYS}/inc -I${LIBTHREAD.ARCH} \
    -I${LIBCDIR}/inc -I${LIBCDIR}/ppc/inc

# Set DBG to null if you do not wish to turn DEBUG on
DBG=

#
# NOTE: ARCHOBJS are listed first because the .init of ppcsubr.o
#       must be called before the init inside of rtld.o.  This is
#       because the call to ld_concurrency must have libthread
#       properly initialized so that it can make calls back into
#       libthread.
#
#OBJECTS = ${ARCHOBJS} ${COMMONOBJS} ${SYSOBJS}
OBJECTS = ${LIBCOBJ} ${COMMONOBJS} ${SYSOBJS} ${ARCHOBJS} ${RTLDOBJ}

LWP.O_IFDEBUG = if test -n "`echo $(DBG)|grep DEBUG`"; \
		then echo "lwp.o";\
		fi

OBJECTS += $(LWP.O_IFDEBUG:sh)

LIBRARY= libthread.a
LIBRARY_CRES= libthread_cres.a
DYNLIB_CRES= libthread_cres.so.1
LIB_PIC= libthread_pic.a
LIBRARY_T= libthread_trace.a
LIBRARY_IT= libthread_itrace.a
DYNLIB_T= libthread_trace.so.1
DYNLIB_IT= libthread_itrace.so.1
VERS=	.1


#include global definitions for libraries
include ../../Makefile.lib

MAPFILES=	../common/mapfile-vers mapfile-vers
MAPOPTS=	$(MAPFILES:%=-M %)

#$(LIBRARY) 	:= OBJS += objs/resume.o
$(LIBRARY_CRES) := OBJS += objs/cresume.o

#$(LIB_PIC) 	:= PICS += pics/resume.o
$(LIBPIC_CRES) 	:= PICS += pics/cresume.o

#$(DYNLIB) 	:= PICS += pics/resume.o
$(DYNLIB_CRES) 	:= PICS += pics/cresume.o

#$(DYNLIB_T) 	:= PICS += pics/resume.o
$(DYNLIB_IT) 	:= PICS += pics/cresume.o

#$(LIBRARY_T) 	:= OBJS += objs/resume.o
$(LIBRARY_IT) 	:= OBJS += objs/cresume.o

# Use CPP_EOPTS to switch on TLS, e.g. "make libthread.a CPP_EOPTS=-DTLS"
CPP_EOPTS=
CPPFLAGS += ${INC} ${DBG} ${CPP_EOPTS} -D_REENTRANT

# Use C_EOPTS to add c flags, such as -g
C_EOPTS=
CFLAGS += ${C_EOPTS}

INLINE_IF_NOTLS= if test -n "`echo $(EOPTS)|grep TLS`"; \
		then echo "";\
		else echo ${INLINE};\
		fi
CFLAGS +=  ${INLINE_IF_NOTLS:sh}
DYNFLAGS += $(MAPOPTS)

ASFLAGS += -P -D__STDC__ -DLOCORE -D_ASM -D_SYS_SYS_S ${CPPFLAGS}
ARFLAGS = rcu


TRACE_OBJS= utrace.o gethrtime.o
DY_TRACE_OBJS= utrace_pic.o gethrtime.o

$(LIBRARY) 	:= AROBJS=$(OBJS)
$(LIBRARY_CRES) := AROBJS=$(OBJS)

$(LIBRARY_T) 	:= OBJS_T=$(OBJS:objs/%=objs_t/%)
$(LIBRARY_T) 	:= AROBJS=$(OBJS_T) $(TRACE_OBJS)
$(LIBRARY_T)	:= CPPFLAGS += -DUTRACE

$(LIBRARY_IT) 	:= OBJS_IT=$(OBJS:objs/%=objs_it/%)
$(LIBRARY_IT) 	:= AROBJS=$(OBJS_IT) $(TRACE_OBJS)
$(LIBRARY_IT)	:= CPPFLAGS += -DITRACE

$(LIB_PIC) 	:= AROBJS=$(PICS)
$(LIB_PIC) 	:= ASFLAGS += -DPIC

$(LIBPIC_CRES) 	:= AROBJS=$(PICS)
$(LIBPIC_CRES) 	:= ASFLAGS += -DPIC

$(DYNLIB) 	:= ASFLAGS += -DPIC
$(DYNLIB_CRES) 	:= ASFLAGS += -DPIC

$(DYNLIB_T) 	:= PICS_T=$(PICS:pics/%=pics_t/%) ${DY_TRACE_OBJS}
$(DYNLIB_T) 	:= CPPFLAGS += -K PIC -DUTRACE
$(DYNLIB_T) 	:= ASFLAGS += -DPIC
$(DYNLIB_T) 	:= DYNFLAGS = -ztext -h $(DYNLIB_T)

$(DYNLIB_IT) 	:= PICS_IT=$(PICS:pics/%=pics_it/%) ${DY_TRACE_OBJS}
$(DYNLIB_IT) 	:= CPPFLAGS += -K PIC -DITRACE
$(DYNLIB_IT) 	:= ASFLAGS += -DPIC
$(DYNLIB_IT) 	:= DYNFLAGS = -ztext -h $(DYNLIB_IT)

LDLIBS +=	-ldl -lc

# include common library targets
include ../../Makefile.targ

$(LIB_PIC) := DIR = pics
$(LIB_PIC): pics $$(PICS)
	$(BUILD.AR)
	$(POST_PROCESS_A)

$(LIBRARY_CRES): objs $$(OBJS)
	$(BUILD.AR)
	$(POST_PROCESS_A)

$(LIBRARY_T): objs_t $$(OBJS_T)
	$(BUILD.AR)
	$(POST_PROCESS_A)

$(LIBRARY_IT): objs_it $$(OBJS_IT)
	$(BUILD.AR)
	$(POST_PROCESS_A)

$(DYNLIB_CRES): pics $$(PICS)
	$(CC) -o $@ -dy -G $(DYNFLAGS) $(PICS) $(LDLIBS)
	$(POST_PROCESS_SO)

$(DYNLIB_T): pics_t $$(PICS_T)
	$(CC) -o $@ -dy -G $(DYNFLAGS) $(PICS_T) $(LDLIBS)
	$(POST_PROCESS_SO)

$(DYNLIB_IT): pics_it $$(PICS_IT)
	$(CC) -o $@ -dy -G $(DYNFLAGS) $(PICS_IT) $(LDLIBS)
	$(POST_PROCESS_SO)

SRCS=	$(CFILES)
CLEANFILES += assym.s genassym

$(DYNLIB):	$(MAPFILES)

.KEEP_STATE:

all:	${LIBRARY} 
install_static:	$(ROOTLIBDIR)/${LIBRARY}

lint:		$(LINTLIB)

$(ASSYM_DEPS:%=pics/%):	assym.s
$(ASSYM_DEPS:%=objs/%):	assym.s
assym.s:	genassym	${INLINE}
	./genassym > assym.s
genassym:	${LIBTHREAD.ARCH}/genassym.c ${LIBTHREADDIR}/inc/utrace.h
	${NATIVECC} $(INC) $(CPPFLAGS.master) ${LIBTHREAD.ARCH}/genassym.c -o genassym

objs/%.o pics/%.o: ${COMMON}/%.c $(HFILES) \
		   ${INLINE} $(LIBTHREADDIR)/inc/utrace.h
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o: ${LIBTHREAD.ARCH}/%.c $(HFILES) \
		   ${INLINE} $(LIBTHREADDIR)/inc/utrace.h
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o: ${LIBTHREAD.ARCH}/%.s ${INLINE}
	$(COMPILE.s) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o: ${SYSARCH}/%.s ${INLINE}
	$(COMPILE.s) -o $@ $<
	$(POST_PROCESS_O)

objc/%.o pics/%.o: ${SYSARCH}/%.c $(HFILES) \
		   ${INLINE} $(LIBTHREADDIR)/inc/utrace.h
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o: ${SYSCOM}/%.c $(HFILES) \
		   ${INLINE} $(LIBTHREADDIR)/inc/utrace.h
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# Trced objects

objs_t/%.o pics_t/%.o: ${COMMON}/%.c $(HFILES) \
		   ${INLINE} $(LIBTHREADDIR)/inc/utrace.h
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs_t/%.o pics_t/%.o: ${LIBTHREAD.ARCH}/%.c $(HFILES) \
		   ${INLINE} $(LIBTHREADDIR)/inc/utrace.h
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs_t/%.o pics_t/%.o: ${LIBTHREAD.ARCH}/%.s ${INLINE}
	$(COMPILE.s) -o $@ $<
	$(POST_PROCESS_O)

objs_t/%.o pics_t/%.o: ${SYSARCH}/%.s ${INLINE}
	$(COMPILE.s) -o $@ $<
	$(POST_PROCESS_O)

objs_t/%.o pics_t/%.o: ${SYSCOM}/%.c $(HFILES) \
		   ${INLINE} $(LIBTHREADDIR)/inc/utrace.h
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# Trced objects

pics_t:
	mkdir pics_t

objs_it/%.o pics_it/%.o: ${COMMON}/%.c $(HFILES) \
		   ${INLINE} $(LIBTHREADDIR)/inc/utrace.h
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs_it/%.o pics_it/%.o: ${LIBTHREAD.ARCH}/%.c $(HFILES) \
		   ${INLINE} $(LIBTHREADDIR)/inc/utrace.h
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs_it/%.o pics_it/%.o: ${LIBTHREAD.ARCH}/%.s ${INLINE}
	$(COMPILE.s) -o $@ $<
	$(POST_PROCESS_O)

objs_it/%.o pics_it/%.o: ${SYSARCH}/%.s ${INLINE}
	$(COMPILE.s) -o $@ $<
	$(POST_PROCESS_O)

objs_it/%.o pics_it/%.o: ${SYSCOM}/%.c $(HFILES) \
		   ${INLINE} $(LIBTHREADDIR)/inc/utrace.h
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
