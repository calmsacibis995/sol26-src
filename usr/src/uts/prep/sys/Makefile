#
#pragma ident	"@(#)Makefile	1.43	96/06/26 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# uts/prep/sys/Makefile
#
# include global definitions
include ../../../Makefile.master

HDRS= \
	SDACreg.h	\
	asm_misc.h	\
	bt485reg.h	\
	clgd5434reg.h	\
	clock.h		\
	fairway.h	\
	glacier.h	\
	machcpuvar.h	\
	machparam.h	\
	machsystm.h	\
	machthread.h	\
	memvar.h	\
	p9000var.h	\
	p9100var.h	\
	pixrect.h	\
	pr_impl_util.h	\
	pr_planegroups.h	\
	prom_plat.h	\
	psm.h		\
	psm_defs.h	\
	psm_modctl.h	\
	psm_types.h	\
	rgb525reg.h	\
	s3reg.h		\
	smp_impldefs.h	\
	vgareg.h	\
	viperreg.h	\
	viperio.h	\
	vm_machparam.h	\
	wd90c24a2reg.h	\
	x_call.h	\
	xc_levels.h

SUBHDRS=

ROOTDIR=	$(ROOT)/usr/include/sys
ROOTDIRS=	$(ROOTDIR)

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
