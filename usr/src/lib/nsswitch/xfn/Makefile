#
#ident	"@(#)Makefile	1.3	96/04/23 SMI"
#
# Copyright (c) 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/nsswitch/xfn/Makefile


LIBRARY= nss_xfn.a
VERS= .1

HDRS= xfn_common.h
OBJECTS= xfn_common.o gethostent.o
STYLEFILES= $(HDRS) $(OBJECTS:%.o=%.c) Makefile

include	../../Makefile.lib

LIBS=	 $(DYNLIB)		# a static library would be silly, eh
MAPFILE= mapfile-vers

CPPFLAGS += -D_REENTRANT
CFLAGS   += -v
LDLIBS   += -lnsl -lxfn -lc
DYNFLAGS += -M $(MAPFILE)
#DYNFLAGS += -zdefs	# commented out as a workaround for linker bug 1242605
LINTFLAGS=

all: $(LIBS)

install: $(ROOTLIBS)

include ../../Makefile.targ
#include ../Makefile.style

.KEEP_STATE:
