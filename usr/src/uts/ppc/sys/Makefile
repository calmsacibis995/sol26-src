#
#pragma ident	"@(#)Makefile	1.52	96/08/15 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
# All Rights Reserved.
#
# uts/ppc/sys/Makefile
#
# include global definitions
include ../../../Makefile.master

HDRS=  \
	archsystm.h	asm_linkage.h	\
	bootconf.h	bootsvcs.h	bpp_io.h	\
	cpu.h		ddi_isa.h	\
	dumphdr.h	dkio.h		frame.h		\
	immu.h		inline.h	machlock.h	\
	machsig.h	machtypes.h	mmu.h		mutex_impl.h	\
	obpdefs.h	old_procfs.h	openprom.h	pcb.h		\
	procfs_isa.h	promif.h	psw.h		\
	pte.h		promimpl.h	\
	prom_config.h	prom_isa.h	reg.h		\
	regset.h	rwlock_impl.h\
	spl.h		stack.h		stat.h		\
	sysconfig_impl.h synch32.h	\
	trap.h		traptrace.h	ucontext.h	vmparam.h

SUBHDRS=  \
	debug/debug.h		debug/debugger.h	debug/debugreg.h \
	fpu/fpusystm.h


ROOTDIR=	$(ROOT)/usr/include/sys
SCSIDIR=	$(ROOTDIR)/scsi
SCSIDIRS=	$(SCSIDIR) $(SCSIDIR)/conf $(SCSIDIR)/generic $(SCSIDIR)/impl
ROOTDIRS=	$(ROOTDIR) $(ROOTDIR)/dktp $(SCSIDIRS) \
		$(ROOTDIR)/debug $(ROOTDIR)/fpu
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
