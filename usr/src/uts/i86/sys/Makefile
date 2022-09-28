#
#pragma ident	"@(#)Makefile	1.50	96/06/26 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
#
# uts/i86/sys/Makefile
#
# include global definitions
include ../../../Makefile.master

# NOTE: hrtcntl.h and hrtsys.h are present in this directory so that the
#	hrtsys system call can be built to facilitate transportability of
#	stock SVr4 programs.  Every effort is to be made to prevent objects
#	from being built, so these headers are not exported (installed).
#
# NOTE:	v86.h, v86intr.h, vc.h and vcpriocntl.h are for VPIX.  This
#	is currently under discussion, but ifdef'd out in the kernel.
#	Hence vc.h and vcpriocntl.h are (temporarily) retained but not
#	installed.  We still install v86.h and v86intr.h because
#	the windows server needs the V86SC_IOPL define in v86.h for
#	its sysi86() call, and v86.h includes v86intr.h.

HDRS=  \
	archsystm.h	asm_linkage.h	bootconf.h	\
	bootsvcs.h	cpu.h		\
	ddi_isa.h	debugreg.h	dkio.h		\
	dumphdr.h	fp.h		frame.h         \
	immu.h		inline.h        \
	machlock.h	machsig.h	machtypes.h	\
	mmu.h		mutex_impl.h	obpdefs.h	\
	old_procfs.h	pcb.h		procfs_isa.h	\
	prom_isa.h	prom_plat.h	promif.h	promimpl.h	\
	psw.h		pte.h		reg.h		regset.h	\
	rwlock_impl.h	spl.h		stat.h		segment.h	\
	stack.h		sysconfig_impl.h sysi86.h	\
	trap.h	 	tss.h		ucontext.h	v86.h		\
	v86intr.h	vmparam.h	x86_archext.h

SUBHDRS=  \
	debug/debug.h		debug/debugger.h


ROOTDIR=	$(ROOT)/usr/include/sys
SCSIDIR=	$(ROOTDIR)/scsi
SCSIDIRS=	$(SCSIDIR) $(SCSIDIR)/conf $(SCSIDIR)/generic $(SCSIDIR)/impl
ROOTDIRS=	$(ROOTDIR) $(SCSIDIRS) $(ROOTDIR)/debug
ROOTHDRS= $(HDRS:%=$(ROOTDIR)/%) $(SUBHDRS:%=$(ROOTDIR)/%) 

CHECKHDRS= $(HDRS:%.h=%.check) $(SUBHDRS:%.h=%.check)

# install rules
$(ROOTDIR)/%: %
	$(INS.file)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS) $(ROOTHDRS)

install_h: $(ROOTDIRS) .WAIT $(ROOTHDRS)

$(ROOTDIRS):
	$(INS.dir)

check:	$(CHECKHDRS)
