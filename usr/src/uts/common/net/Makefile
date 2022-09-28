#
#ident	"@(#)Makefile	1.13	96/09/26 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
# All rights reserved.
#
# uts/common/net/Makefile
#
# include global definitions
include ../../../Makefile.master

HDRS=	af.h	if.h	if_arp.h	if_dl.h	if_types.h	route.h

ROOTDIRS= $(ROOT)/usr/include/net

ROOTHDRS= $(HDRS:%=$(ROOT)/usr/include/net/%)

CHECKHDRS= $(HDRS:%.h=%.check)

$(ROOTDIRS)/%: %
	$(INS.file)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS)

install_h: $(ROOTDIRS) $(ROOTHDRS)

$(ROOTDIRS):
	$(INS.dir)

check:	$(CHECKHDRS)
