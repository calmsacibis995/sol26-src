#
#ident	"@(#)Makefile	1.30	96/07/17 SMI"
#
# Copyright (c) 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# uts/i86pc/sys/Makefile
#
# include global definitions
include ../../../Makefile.master

#
# NOTE:	vdi.h is retained in the source base until the finale (?) vpix
#	decision is made.  It is intentionally not shipped.
#
HDRS=  \
	bpp_io.h	nvm.h		\
	clock.h		cram.h		ddi_subrdefs.h	eisarom.h	\
	machcpuvar.h	machparam.h	machthread.h	mcdma.h		\
	vm_machparam.h	xque.h		smp_impldefs.h	\
	asm_misc.h	rm_platter.h	x_call.h	xc_levels.h	\
	psm.h		psm_defs.h	psm_types.h	machsystm.h

ROOTDIR=	$(ROOT)/usr/include/sys
ROOTDIRS=	$(ROOTDIR)

ROOTHDRS= $(HDRS:%=$(ROOTDIR)/%)

CHECKHDRS= $(HDRS:%.h=%.check)

# install rules
$(ROOTDIR)/%: %
	$(INS.file)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS) $(ROOTHDRS)

install_h: $(ROOTDIRS) .WAIT $(ROOTHDRS)

$(ROOTDIRS):
	$(INS.dir)

check:	$(CHECKHDRS)
