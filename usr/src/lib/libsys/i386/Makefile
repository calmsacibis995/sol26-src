#
#pragma ident	"@(#)Makefile	1.4	95/02/24 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libsys/i386/Makefile

include		../Makefile.com

.KEEP_STATE:

all:		$(DYNLIB)

install:	all $(ROOTDYNLIB) $(ROOTLINKS)

$(COMSRC):	../common/libsys.sh libsys.list
		sh ../common/libsys.sh

include		../Makefile.targ
