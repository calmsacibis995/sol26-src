#
#ident	"@(#)Makefile.com	1.3	96/06/18 SMI"
#
# Copyright (c) 1994-1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/ptools/Makefile.com
#

CFLAGS += -v

lint	:= LINTFLAGS =	-mux

ROOTPROCBIN =		$(ROOT)/usr/proc/bin
ROOTPROCLIB =		$(ROOT)/usr/proc/lib

ROOTPROCBINPROG =	$(PROG:%=$(ROOTPROCBIN)/%)
ROOTPROCLIBLIB =	$(LIBS:%=$(ROOTPROCLIB)/%)

$(ROOTPROCBIN)/%: %
	$(INS.file)

$(ROOTPROCLIB)/%: %
	$(INS.file)
