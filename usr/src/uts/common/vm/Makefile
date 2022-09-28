#
#ident	"@(#)Makefile	1.18	96/05/01 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# uts/common/vm/Makefile
#
# include global definitions
include ../../../Makefile.master

HDRS=	anon.h as.h faultcode.h hat.h page.h pvn.h rm.h \
	seg.h seg_dev.h seg_enum.h seg_kp.h seg_map.h seg_vn.h vpage.h seg_spt.h

ROOTDIRS= $(ROOT)/usr/include/vm

ROOTHDRS= $(HDRS:%=$(ROOTDIRS)/%)

CHECKHDRS= $(HDRS:%.h=%.check)

# install rule
$(ROOTDIRS)/%: %
	$(INS.file)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS)

install_h: $(ROOTDIRS) $(ROOTHDRS)

$(ROOTDIRS):
	$(INS.dir)

check:	$(CHECKHDRS)
