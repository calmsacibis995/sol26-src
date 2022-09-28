#
#ident  "@(#)Makefile.com 1.77     96/10/04 SMI"
#
# Copyright (c) 1990-1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# uts/adb/common/Makefile.com
#

PROGS		= adbgen adbgen1 adbgen3 adbgen4
OBJS		= adbsub.o

# NOTE: The following have been at least temporarily removed:
#	dir.adb
#	dir.nxt.adb
#	fpu.adb
#	mount.adb
#	pme.adb
#	stat.adb

SRCS += \
as.adb			bootobj.adb		buf.adb \
bufctl.adb		bufctl_audit.adb	buflist.adb \
buflist.nxt.adb		buflistiter.nxt.adb	cachefsfsc.adb \
cachefsmeta.adb		callout.adb		calltrace.adb \
calltrace.nxt.adb	cg.adb			cglist.adb \
cglist.nxt.adb		cglistchk.nxt.adb	cglistiter.nxt.adb \
cnode.adb		cpu.adb \
cpun.adb		cpupart.adb		cpus.adb \
cpus.nxt.adb \
cred.adb		csum.adb \
dblk.adb		devinfo.adb \
dino.adb		disp.adb		direct.adb \
dispq.adb		dispq.nxt.adb		dispqtrace.adb \
dispqtrace.list.adb	dispqtrace.nxt.adb	door.adb \
door_arg.adb		door_data.adb		dqblk.adb \
dquot.adb		dumphdr.adb 		edge.adb \
exdata.adb		fifonode.adb		file.adb \
filsys.adb		fnnode.adb		fs.adb \
graph.adb \
hmelist.adb		ic_acl.adb \
inode.adb 		inodelist.adb		inodelist.nxt.adb \
inodelistiter.nxt.adb \
ifnet.adb		iocblk.adb		iovec.adb \
itimerval.adb		lock_descriptor.adb \
kmastat.adb		kmastat.nxt.adb \
kmem_cache.adb		kmem_cpu.adb		kmem_cpu.nxt.adb \
kmem_cpu_log.adb	kmem_cpu_log.nxt.adb	kmem_log.adb \
ksiginfo.adb		lockfs.adb \
rlimit64.adb		stat64.adb		lwp.adb \
mblk.adb		mblk.nxt.adb \
memlist.adb		memlist.nxt.adb		memlist.list.adb \
mntinfo.adb		modctl.adb \
modules.adb		modules.nxt.adb		modctl_list.adb \
modinfo.adb		modlinkage.adb		module.adb \
msgbuf.wrap.adb		netbuf.adb		page.adb \
pathname.adb		pcb.adb			pid.adb \
pid.print.adb		pid2proc.adb		pid2proc.chain.adb \
pollhead.adb		prcommon.adb		prgregset.adb \
prnode.adb		proc.adb \
proc_edge.adb	 	proc2u.adb		procthreads.adb	\
procthreads.list.adb	proc_vertex.adb \
putbuf.adb		putbuf.wrap.adb		qinit.adb \
qproc.info.adb		qthread.info.adb	queue.adb \
rlimit.adb		rnode.adb \
rpctimer.adb		rtproc.adb		schedctl.adb \
scsi_addr.adb		scsi_arq_status.adb	scsi_dev.adb \
scsi_hba_tran.adb	scsi_pkt.adb		scsi_tape.adb \
seg.adb			segdev.adb		segmap.adb \
segvn.adb		seglist.adb		seglist.nxt.adb \
session.adb		setproc.adb \
setproc.done.adb	setproc.nop.adb		setproc.nxt.adb \
si.adb			sigaltstack.adb		slab.adb \
sleepq.adb		sleepq.nxt.adb \
slpqtrace.adb		slpqtrace.list.adb	slpqtrace.nxt.adb \
smap.adb		snode.adb \
sobj.adb		stack.adb \
stackregs.adb		stacktrace.adb		stacktrace.nxt.adb \
stat.adb		stdata.adb		st_drivetype.adb \
strtab.adb		svcfh.adb \
sysinfo.adb		tcpip.adb		tcpcb.adb \
thread.adb		thread.trace.adb	threadlist.adb \
threadlist.nxt.adb	tmpnode.adb		tmount.adb \
traceall.nxt.adb	tsdpent.adb		tsproc.adb \
tune.adb		u.adb			u.sizeof.adb \
ucalltrace.adb		ucalltrace.nxt.adb	ufchunk.adb \
ufchunk.nxt.adb 	ufsq.adb		ufsvfs.adb \
ufs_acl.adb		ufs_acllist.adb		ufs_aclmask.adb \
uio.adb			ulockfs.adb \
ustack.adb		utsname.adb \
v.adb			v_call.adb		v_proc.adb \
vattr.adb		vfs.adb			vfslist.adb \
vfslist.nxt.adb		vnode.adb \
vpages.adb		vpages.nxt.adb

SCRIPTS		= $(SRCS:.adb=)

include $(ADB_BASE_DIR)/../Makefile.uts

# Following grossness is added because the x86 people can't follow the
# naming guidelines...
# Should be simply:
# INCLUDES	= -I${SYSDIR}/${MACH} -I${SYSDIR}/sun
INCLUDES-i386	= -I${SYSDIR}/i86
INCLUDES-sparc	= -I${SYSDIR}/${MACH} -I${SYSDIR}/sun
INCLUDES-ppc	= -I${SYSDIR}/${MACH}
INCLUDES	= ${INCLUDES-${MACH}}
INCDIR		= ${SYSDIR}/common
NATIVEDEFS	= -D${MACH} -D__${MACH} -D_KERNEL

ROOTUSRDIR	= $(ROOT)/usr
ROOTLIBDIR	= $(ROOTUSRDIR)/lib
ROOTADBDIR	= $(ROOTLIBDIR)/adb

ROOTPROGS	= $(PROGS:%=$(ROOTADBDIR)/%)
ROOTOBJS	= $(OBJS:%=$(ROOTADBDIR)/%)
ROOTSCRIPTS	= $(SCRIPTS:%=$(ROOTADBDIR)/%)

LDLIBS 		= $(ENVLDLIBS1)  $(ENVLDLIBS2)  $(ENVLDLIBS3)
LDFLAGS 	= $(STRIPFLAG) $(ENVLDFLAGS1) $(ENVLDFLAGS2) $(ENVLDFLAGS3)
CPPFLAGS	= $(CPPFLAGS.master)

$(ROOTOBJS)	:= FILEMODE = 644
$(ROOTSCRIPTS)	:= FILEMODE = 644

.KEEP_STATE:

MACH_FLAG=	__$(MACH)
NATIVEDEFS-ppc=	-D__LITTLE_ENDIAN -D$(MACH_FLAG) -Ui386
NATIVEDEFS-i386=
NATIVEDEFS-sparc=
NATIVEDEFS =	-D${MACH} -D_KERNEL
NATIVEDEFS +=	${NATIVEDEFS-${MACH}}
NATIVEDIR	= $(ADB_BASE_DIR)/native/${NATIVE_MACH}
NATIVEPROGS	= $(PROGS:%=$(NATIVEDIR)/%)
NATIVEOBJS	= $(OBJS:%=$(NATIVEDIR)/%)

.PARALLEL: $(PROGS) $(NATIVEPROGS) $(OBJS) $(NATIVEOBJS) $(SCRIPTS)

all lint: $(PROGS) $(OBJS) \
	$(NATIVEDIR) .WAIT \
	$(NATIVEPROGS) $(NATIVEOBJS) .WAIT \
	$(SCRIPTS)

install: all .WAIT $(ROOTADBDIR) .WAIT $(ROOTPROGS) $(ROOTOBJS) $(ROOTSCRIPTS)

clean:
	-$(RM) $(OBJS) $(NATIVEOBJS)
	-$(RM) $(SCRIPTS:=.adb.c) $(SCRIPTS:=.run) $(SCRIPTS:=.adb.o)

clobber: clean
	-$(RM) $(PROGS) $(NATIVEPROGS)
	-$(RM) $(SCRIPTS)

# installation things

$(ROOTADBDIR)/%: %
	$(INS.file)

$(ROOTUSRDIR) $(ROOTLIBDIR) $(ROOTADBDIR):
	$(INS.dir)

# specific build rules

adbgen:		$(COMMONDIR)/adbgen.sh
	$(RM) $@
	cat $(COMMONDIR)/adbgen.sh >$@
	$(CHMOD) +x $@

adbgen%:	$(COMMONDIR)/adbgen%.c
	$(LINK.c) -o $@ $< $(LDLIBS)
	$(POST_PROCESS)

adbsub.o:	$(COMMONDIR)/adbsub.c
	$(COMPILE.c) $(OUTPUT_OPTION) $(COMMONDIR)/adbsub.c
	$(POST_PROCESS_O)

$(NATIVEDIR):
	$(INS.dir)

$(NATIVEDIR)/adbgen:	$(COMMONDIR)/adbgen.sh
	$(RM) $@
	cat $(COMMONDIR)/adbgen.sh >$@
	$(CHMOD) +x $@

$(NATIVEDIR)/adbgen%:	$(COMMONDIR)/adbgen%.c
	$(NATIVECC) -o $@ $<
	$(POST_PROCESS)

$(NATIVEDIR)/adbsub.o:	$(COMMONDIR)/adbsub.c $(NATIVEDIR)
	$(NATIVECC) -c -o $@ $(COMMONDIR)/adbsub.c
	$(POST_PROCESS_O)

#
# the following section replaces the actions of adbgen.sh.
#
# there are two reuseable build macros, pattern-matching rules for kernel
# architectures, and required explicit dependencies.
#
BUILD.run= (unset LD_LIBRARY_PATH; \
	$(NATIVECC) ${ARCHOPTS} $(NATIVEDEFS) ${INCLUDES} \
	$(CCYFLAG)${INCDIR} -o $@.run $@.adb.c $(NATIVEDIR)/adbsub.o)

#
# note that we *deliberately* use the '-e' flag here to force the
# build to break if warnings result.  the right way to fix this
# is to repair the macro (or the header!), NOT to take the '-e' away.
#
BUILD.adb= ./$@.run -e > $@.runout && \
	$(NATIVEDIR)/adbgen3 < $@.runout | $(NATIVEDIR)/adbgen4 > $@

% : $(ISADIR)/%.adb
	$(NATIVEDIR)/adbgen1 < $< > $@.adb.c
	$(BUILD.run)
	$(BUILD.adb)
	-$(RM) $@.adb.c $@.run $@.adb.o $@.runout

% : $(COMMONDIR)/%.adb
	$(NATIVEDIR)/adbgen1 < $< > $@.adb.c
	$(BUILD.run)
	$(BUILD.adb)
	-$(RM) $@.adb.c $@.run $@.adb.o $@.runout

check:
	@echo $(SCRIPTS) | tr ' ' '\012' | sed 's/$$/&.adb/' |\
		sort > script.files
	@(cd $(ADB_BASE_DIR); ls *.adb) > actual.files
	diff script.files actual.files
	-$(RM) script.files actual.files

# the macro list is platform-architecture specific too.

maclist1:
	@(dir=`pwd`; \
	for i in $(SCRIPTS); do \
		echo "$$dir $$i"; \
	done)

