#
#ident	"@(#)Makefile	1.5	95/04/07 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libsys/ppc/Makefile

include		../Makefile.com

.KEEP_STATE:

all:		$(DYNLIB)

install:	all $(ROOTDYNLIB) $(ROOTLINKS)

$(COMSRC):	../common/libsys.sh libsys.list
		sh ../common/libsys.sh

include		../Makefile.targ
